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

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ev.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

#include "os.h"
#include "log.h"
#include "monitor.h"
#include "statem.h"
#include "os_proc.h"
#include "target.h"

#include "dm.h"

/* not really max FD, but we won't go further than
 * this number in closefrom() surrogate implementation */
#define DM_MAX_FD       (1024)

#define IGNORE_SIGMASK ((1 << SIGTERM) | (1 << SIGKILL) | (1 << SIGUSR1) | \
                        (1 << SIGUSR2) | (1 << SIGINT))

#define TM_OUT_FAST     (5)
#define TM_OUT_SLOW     (60)

#define B_SIZE          (64)


STATE_MACHINE_USE;

static ev_child *  wchilds = NULL;

/* in case of crash don't re-start process immediately, instead
 * we rather make a break and then try to restart the process
 */
static ev_timer *  wtimers = NULL;

/* prototypes  */
static bool child_start(ev_child * w, target_managers_config_t * ci);
static bool child_restart(struct ev_loop *loop, ev_child *w, target_managers_config_t *ci, int after);
static bool store_manager_pid(int i, int pid);


/**
 * Timer callback used to start crashed process with some delay after the crash
 **/
void cb_timeout(struct ev_loop *loop, ev_timer * w, int events)
{
    (void)events;

    /* extract required data for process restart */
    ev_child * wc = NULL;
    target_managers_config_t  * ci = NULL;

    /* stop timer watcher */
    ev_timer_stop(loop, w);

    /* child watcher is pointed by ->data member*/
    wc = (ev_child *)w->data;

    /* and target_managers_config is pointed by ev_child -> data member */
    ci = (target_managers_config_t *)wc->data;

    /* ci->started is indicator that process was successfully
     * started but either crashed or exited
     * If process is not started at all, most probably executable is missing
     * In that case there is no reason to restart all processes
     */
    if (ci->needs_plan_b && ci->started)
    {
        target_managers_restart();
    }

    /* restart failed process */
    child_start(wc, ci);
}

/**
 * Helper used to check if a signal should be ignored
 */
static bool ignore_signal(struct ev_loop *loop, ev_child *w)
{
    target_managers_config_t *ci = (target_managers_config_t *)w->data;
    uint32_t termsig;

    /* Bail if the child process was not terminated by a signal */
    if (!WIFSIGNALED(w->rstatus))
    {
        return true;
    }

    /* Check if the child process needs to be restarted no matter what */
    if (ci->always_restart != 0)
    {
        return false;
    }

    /* Check if the signal reflects a user action */
    if (WTERMSIG(w->rstatus) <= 31)
    {
        termsig = 1 << WTERMSIG(w->rstatus);
        return ((termsig & IGNORE_SIGMASK) != 0);
    }

    return false;
}

/**
 * Helper used to compute the restart delay for a process
 */
static int get_start_delay(target_managers_config_t *ci)
{
    /* For backwards compatibility reasons,
       a restart_delay value of 0 maps to TM_OUT_FAST
       and restart_delay value of -1 maps to 0 */
    if (ci->restart_delay == 0)
    {
        return TM_OUT_FAST;
    }

    if (ci->restart_delay == -1)
    {
        return 0;
    }

    return ci->restart_delay;
}

/**
 * Child signals callback (one callback for all children
 **/
static void cb_child (struct ev_loop *loop, ev_child *w, int revents)
{
    (void)revents;

    target_managers_config_t * ci = NULL;
    int start_delay = TM_OUT_FAST;

    LOG(INFO, "CB child pid=%d invoked, rstatus=%d", w->rpid, w->rstatus);
    LOG(DEBUG, "Term signal %d", WTERMSIG(w->rstatus));

    /* needed as on new process restart pid is changed */
    /* also it will be restarted when needed */
    ev_child_stop(loop, w);

    /* extract target_managers_config from ev_child  */
    ci = (target_managers_config_t *)w->data;
    start_delay = get_start_delay(ci);

#ifdef WIFCONTINUED
    if (WIFCONTINUED(w->rstatus))
    {
        LOG(DEBUG, "process continued  pid=%d", w->rpid);
    }
#endif

    if (WIFEXITED(w->rstatus) && !WIFSIGNALED(w->rstatus))
    {
        LOG(INFO, "process exited name=%s|pid=%d", ci->name, w->rpid);

        /* restart only processes that exited, not those that failed to start*/
        if (!WEXITSTATUS(w->rstatus))
        {
            LOG(NOTICE, "process exited: restarting in %d seconds  names=%s|pid=%d",
                start_delay,
                ci->name,
                w->rpid);

            /* on exit try to restart it relatively fast */
            child_restart(loop, w, ci, start_delay);
            return;
        }
    }

    if (WEXITSTATUS(w->rstatus) && !WIFSIGNALED(w->rstatus))
    {
        /* exec failed, most probably executable is missing
         * restart it with bigger timeout */
        LOG(INFO,
            "process failed to start, restart in 60 seconds  name=%s|pid=%d",
            ci->name,
            w->rpid);

        ci->started = false;
        child_restart(loop, w, ci, TM_OUT_SLOW);
        return;
    }

    /* check if the on of the manager processes had crashed     */
    /* ignore managers stop due to user actions                 */
    if (ignore_signal(loop, w) == false)
    {
        LOG(NOTICE,
            "process terminated, signal: %d, restarting in %d seconds  name=%s|pid=%d",
            WTERMSIG(w->rstatus),
            start_delay,
            ci->name,
            w->rpid);

        child_restart(loop, w, ci, start_delay);
    }
    else
    {
        LOG(NOTICE,
            "process user termination by signal: %d, process=%s pid=%d",
            WTERMSIG(w->rstatus),
            ci->name,
            w->rpid);
    }

}


/**
 * Fork and execute on of the managers (create child process)
 * Errors will be handled in calling function(s)
 **/
pid_t child_exec(char * child)
{
    pid_t cpid;
    int ifd;

    cpid = fork();

    /* fork seems to made it */
    if (cpid >= 0)
    {
        /* we are in child process */
        if (cpid == 0)
        {
            /* close all open fds in parent process */
            /* This is usually done by closefrom(), uclibc has not such */
            /* function - use this surrogate implementation */
            for (ifd = 3; ifd < DM_MAX_FD; ifd++)
            {
                close(ifd);
            }
            /* exec manager process */
            if (0 > execl(child, child, NULL))
            {
                /* exit this child, we don't need it */
                exit(EXIT_FAILURE);
            }
        }
    }

    return cpid;
}


bool child_start(ev_child * w, target_managers_config_t * ci)
{
    pid_t cpid;

    cpid = child_exec(ci->name);

    if (0 == cpid)
    {
        /* major error, how this happened ???  */
        LOG(ERR, "Not supposed to reach this line \n");
    }
    else if (cpid > 0)
    {
        /*NOTE: even if execXX fails, child has been started    */
        /* This means callback function handle tow cases correctly
         * 1. execXX failed
         * 2. execXX made it, but then manager crashed*/
        /* store child pid */
        ci->pid = cpid;
        ci->started = true;

        LOG(NOTICE, "Starting process name=%s pid=%d", ci->name, ci->pid);

        /* save pid for all nice and clean process termination */
        if (false == store_manager_pid(ci->ordinal, ci->pid))
        {
            LOG(ERR, "Error storing pid for manager  name=%s|pid=%d",
                     ci->name,
                     ci->pid);
        }

        /* start monitoring this process */
        ev_child_init(w, cb_child, cpid, 0);
        w->data = ci;
        ev_child_start(EV_DEFAULT, w);

    }
    else
    {
        LOG(ERR, "Fork failed for unknown reasons");
    }

    return true;
}

/*
 * Restart the child process after "timeout" seconds
 */
bool child_restart(struct ev_loop *loop, ev_child *w, target_managers_config_t *ci, int timeout)
{
    ev_timer_init(&wtimers[ci->ordinal], cb_timeout, timeout, 0);
    wtimers[ci->ordinal].data = w;
    ev_timer_start(loop, &wtimers[ci->ordinal]);

    return true;
}

/**
 * Return pid file name for given manager ordinal
 */
bool pid_file(int i, char * buff, size_t bs)
{
    bool success = false;
    char * path = NULL;

    /* prevent catastrophic failure */
    if (i > (int)target_managers_num - 1)
    {
        LOG(DEBUG, "pid_file error  i=%d|max=%d",
                i, (int)target_managers_num);
        return success;
    }

    memset(buff, 0, bs);

    /* copy path to buffer, but first verify that buffer size */
    /*
     *  separator       : 1
     *  suffix(".pid")  : 4
     *  string term     : 1
     *  ====================
     *  total           : 6
     *
     */
    if (strlen(TARGET_MANAGERS_PID_PATH) + 6 < bs)
    {
        /* copy directory path to buffer */
        strncpy(buff, TARGET_MANAGERS_PID_PATH, bs);

        /* add separator */
        strcat(buff, "/");

        /* extract filename */
        path = strdup(target_managers_config[i].name);

        if (path == NULL) return false;

        strcat(buff, basename(path));
        free(path);

        strcat(buff, ".pid");

        LOG(DEBUG, "pid_file generated  file=%s", buff);

        success = true;
    }

    return success;
}


bool pid_dir(void)
{
    bool isdir = false;
    int mdr = 0;    /* mkdir result */

    /* try to create folder for managers PIDs. In case dir exists, this is ok */
    mdr = mkdir(TARGET_MANAGERS_PID_PATH, 0x0777);

    if (mdr == 0 || (mdr == -1 && errno == EEXIST))
    {
        isdir = true;
    }

    LOG(DEBUG, "pid_dir creation  isdir=%s", isdir ? "true" : "false");
    return isdir;
}


/**
 * Store given managers PID into pid file
 */
bool store_manager_pid(int i, int pid)
{
    bool success = false;
    char buff[B_SIZE];
    int  n;
    int  pfd;

    if (true == pid_file(i, buff, B_SIZE))
    {
        /* open pid files */
        pfd = open(buff,
                   O_CREAT | O_WRONLY | O_TRUNC,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (pfd < 0)
        {
            LOG(ERR, "can't create pid file  file=%s", buff);
        }
        else
        {
            /* use the same buffer, to print to buffer */
            memset(buff, 0, B_SIZE);

            n = snprintf(buff, B_SIZE, "%d", pid);

            if ((n > 0) && (n < B_SIZE))
            {
                /* write pid id  and check if everything is written*/
                if (n == write(pfd, buff, strlen(buff) + 1) - 1)
                {
                    success = true;
                }
            }

            /* close file in any case */
            close(pfd);
        }
    }

    return success;
}

/*
 * Terminate old managers if they exist
 * The information about manager from previous dm session is stored
 * in pid file
 * Those are in current implementation available in
 * /tm;/dmpid/<mngr_base_name>.pid
 */
bool terminate_manager(int i)
{
    pid_t pid2kill = -1;
    bool retval = false;
    char * mgrpath = NULL;

    mgrpath = strdup(target_managers_config[i].name);
    if (NULL == mgrpath)
    {
        LOG(ERR, "Can't search for manager as strdup failed!");
        return retval;
    }

    if (0 < (pid2kill = os_name_to_pid(basename(mgrpath))))
    {
        if (target_managers_config[i].needs_plan_b == true)
        {
            LOG(NOTICE, "Plan b exec required by %s pid:%d",
                         target_managers_config[i].name,
                         pid2kill);
            target_managers_restart();
            /* should never return from this function */
            LOG(ERR, "Restart manager wasn't applied!");
        }
        else
        {
            /* try to terminate managers if it is running */
            LOG(NOTICE, "Killing process  pid=%d", pid2kill);
            mon_process_terminate(pid2kill);
            retval = true;
        }
    }
    else
    {
        LOG(NOTICE, "Manager: %s - no instance",
                target_managers_config[i].name);
        retval = true;
    }

    free(mgrpath);

    return retval;
}


bool init_managers()
{
    int i;

    /* terminate all running managers */
    if (true == pid_dir())
    {
        LOG(NOTICE, "Shutting down old managers");
        for (i = 0; i < (int)target_managers_num; i++)
        {
            if (false == terminate_manager(i))
            {
                LOG(DEBUG, "Manager not terminated  manager=%s",
                        target_managers_config[i].name);
            }
        }
    }
    else
    {
        LOG(ERR, "Can't create PID folder  path=%s", TARGET_MANAGERS_PID_PATH);
    }

    /* start in a loop all required managers and init all handlers */
    for (i = 0; i < (int)target_managers_num; i++)
    {
        target_managers_config[i].started = false;
        target_managers_config[i].pid = -1; /* i.e no valid pid */
        target_managers_config[i].ordinal = i;

        child_start(wchilds + i , target_managers_config + i);
    }

    return true;
}

bool act_init_managers (void)
{

    bool retval = false;

    /* allocate space for wtimers & wchilds */
    /* happens only once - not a memory leak */
    wtimers = malloc(sizeof(ev_timer) * target_managers_num);
    wchilds = malloc(sizeof(ev_child) * target_managers_num);

    if ((NULL == wtimers) || (NULL == wchilds))
    {
        STATE_TRANSIT(STATE_ERROR);
        return false;
    }

    retval = init_managers();

    if (true == retval)
    {
#if defined(USE_SPEED_TEST) || defined(CONFIG_PLUME_SPEEDTEST)
        /* start monitoring speedtest config table */
        dm_st_monitor();
#endif
    }

    STATE_TRANSIT(STATE_IDLE);

    return retval;
}
