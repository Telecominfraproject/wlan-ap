/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include <pthread.h>

#include "os.h"
#include "os_time.h"
#include "os_util.h"
#include "log.h"
#include "osa_assert.h"
#include "util.h"

#define MODULE_ID LOG_MODULE_ID_OSA

struct task_args
{
    char                   name[64];
    task_entry_point_t*    ep;
    void*                  arg;
};

/**
 * Get current task name
 */
bool task_name_get(char *name, size_t sz)
{
    if (sz < OS_TASK_NAME_SZ)
    {
        return false;
    }

    prctl(PR_GET_NAME, name);

    return true;
}

/**
 * Set the current task name
 */
bool task_name_set(char *name)
{
    if (strlen(name) >= OS_TASK_NAME_SZ)
    {
        return false;
    }

    prctl(PR_SET_NAME, name);

    return true;
}

void* task_create_fn(void *ctx)
{
    struct task_args *targs = ctx;

    task_name_set(targs->name);

    if (targs->ep(targs->arg) != true)
    {
        LOG(EMERG, "Task failed.::task=%s", targs->name);
    }

    /* This was allocated by task_create(), free it here */
    free(targs);

    return NULL;
}

bool
task_create (task_id_t          *id,
             char               *name,
             task_entry_point_t *ep,
             void               *arg )
{
    int   rc;

    //ASSERT_ARG(id);

    struct task_args *targs = malloc(sizeof(struct task_args));
    if (targs == NULL)
    {
        return false;
    }

    STRSCPY(targs->name, name);
    targs->ep   = ep;
    targs->arg  = arg;

    rc = pthread_create(id, NULL, task_create_fn, targs);
    if (0 != rc)
    {
        return(false);
    }

    /* Detach thread as there's our equivalent of pthread_join() */
    pthread_detach(*id);

    return true;
}

/**
 * Function that returns true only once in thread safe manner. This might be used by library initialization
 * code to execute the init function exactly once.
 *
 * Just like pthread_once(), but it does not take a function as argument.
 */
bool task_once(task_once_t *once)
{
    bool retval;

    pthread_mutex_lock(&once->mtx);

    retval = once->status;
    once->status = false;

    pthread_mutex_unlock(&once->mtx);

    return retval;
}

static int32_t hex2num(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/**
 * hwaddr_aton - Convert ASCII string to MAC address (colon-delimited format)
 * @txt: MAC address as a string (e.g., "00:11:22:33:44:55")
 * @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 * Returns: 0 on success, -1 on failure (e.g., string not a MAC address)
 */
int32_t hwaddr_aton(const char *txt, uint8_t *addr)
{
    int32_t i;

    for (i = 0; i < ETH_ALEN; i++)
    {
        int32_t a, b;

        a = hex2num(*txt++);
        if (a < 0)
            return -1;
        b = hex2num(*txt++);
        if (b < 0)
            return -1;
        *addr++ = (a << 4) | b;
        if (i < 5 && *txt++ != ':')
            return -1;
    }

    return 0;
}

pid_t os_popen(const char *shell_cmd, int *pipe_desc)
{
    int32_t pipefd[2];
    pid_t pid;

    if (pipe(pipefd) != 0)
    {
        LOG(ERR, "pipe creation failed:");
        return OS_ERROR_PID;
    }

    pid = fork();
    if (pid < 0)
    {
        LOG(ERR, "fork creation failed:");
        close(pipefd[0]);
        close(pipefd[1]);
        return OS_ERROR_PID;
    }
    else if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        execl("/bin/sh","sh", "-c", shell_cmd, NULL);
        LOG(INFO, "execl cmd failed:");
        exit(1);
    }

    close(pipefd[1]);
    *pipe_desc=pipefd[0];
    return pid;
}

/**
 * Read a PID from a file
 */
pid_t os_pid_from_file(char *pid_file)
{
    char buf[64];
    long pid;

    FILE *f = NULL;
    pid_t retval = 0;

    /* Open PID file */
    f = fopen(pid_file, "r");
    if (f == NULL)
    {
        goto error;
    }

    /* Read first line */
    if (fgets(buf, sizeof(buf), f) == NULL)
    {
        goto error;
    }

    /* Convert it to string */
    pid = atol(buf);
    if (pid == 0)
    {
        goto error;
    }

    /* Check if PID exists */
    if (kill(pid, 0) != 0)
    {
        goto error;
    }

    retval = (pid_t)pid;

error:
    if (f != NULL) fclose(f);

    return retval;
}

/*
 * Wait for process with @p pid to terminate; returns true if pid was terminated or false if the timer expired
 */
bool os_pid_wait(pid_t pid, int timeout_ms)
{
    ticks_t to;

    to = ticks() + TICKS_MS(timeout_ms);

    while (ticks() < to)
    {
        struct timespec ts;

        /* Check if PID is alive */
        if (kill(pid, 0) != 0)
        {
            return true;
        }

        ticks_to_timespec(TICKS_MS(100), &ts);
        nanosleep(&ts, NULL);
    }

    return false;
}

/*
 * Terminate a process; first try a graceful termination. If the process refuses to die, go in for a kill.
 */
bool os_pid_terminate(pid_t pid, int timeout_ms)
{
    if (pid == 0) return false;

    /* First try a TERM signal */
    kill(pid, SIGTERM);
    if (os_pid_wait(pid, timeout_ms))
    {
        return true;
    }

    /* Try a KILL */
    kill(pid, SIGKILL);
    if (os_pid_wait(pid, timeout_ms))
    {
        return true;
    }

    return false;
}

/*
 * Check if a process exists
 */
bool os_pid_exists(pid_t pid)
{
    if (kill(pid, 0) == 0)
        return true;

    return false;
}

/*
 * Execute shell_cmd and return shell exit code in a wait() style format
 */
int cmd_log(const char *shell_cmd)
{
    char buf[1024];

    FILE *fcmd = NULL;
    int rc =  -1;

    fcmd = popen(shell_cmd, "r");
    if (fcmd ==  NULL)
    {
        LOG(DEBUG, "Error executing command.::shell_cmd=%s", shell_cmd);
        goto exit;
    }

    LOG(DEBUG, "Executing command.::shell_cmd=%s ", shell_cmd);

    while (fgets(buf, sizeof(buf), fcmd) != NULL)
    {
        LOG(INFO, ">>::output=%s", buf);
    }

    if (ferror(fcmd))
    {
        LOG(ERR, "fgets() failed.");
        goto exit;
    }

    LOG(DEBUG, "EOF");
    rc = pclose(fcmd);

    fcmd = NULL;

exit:
    if (fcmd != NULL)
    {
        pclose(fcmd);
    }

    return rc;
}

/*
 * Common command-line parsing
 */
int os_get_opt(int argc, char ** argv, log_severity_t* log_severity)
{
    int opt;
    int verbose = 0;

    while((opt = getopt(argc, argv, "vh")) != -1) {
        switch(opt) {

        case 'v':
            verbose++;
            break;

        case 'h':
        default:
            fprintf(stderr, "Usage: %s [-v]... [-h]\n\n", argv[0]);
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "          [-v]...  set verbose level\n");
            fprintf(stderr, "           -h      display this help message\n");
            return -1;
        }
    }

    if (log_severity){
        if (verbose >= 2) {
            *log_severity = LOG_SEVERITY_TRACE;
        }
        else if (verbose == 1) {
            *log_severity = LOG_SEVERITY_DEBUG;
        }
        else {
            *log_severity = LOG_SEVERITY_INFO;
        }
    }

    return 0;
}

