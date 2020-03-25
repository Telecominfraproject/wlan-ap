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

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include "log.h"
#include "os.h"
#include "os_time.h"
#include "osa_assert.h"
#include "monitor.h"

#ifdef __linux__
#define MON_USE_TID_TO_KILL
#endif

#define MON_CHECKIN(id)        mon_checkin((id), __FILE__, __LINE__)


#define MON_LOOP_DELAY         300         /* 300ms between process checks         */
#define MON_RESTART_DELAY      2000        /* Process restart delay in ms          */
#define MON_COUNTER_MAX        120         /* Maximum timeout of monitor counters  */

#define MON_SIGSTR_SZ          64          /* Maximum length of signal descr string*/

#if defined(__UCLIBC_MAJOR__)
/** For some weird reason uClibc doesn't export strsignal() */
extern char *strsignal(int sig);
#endif

/** Definition of a single counter */
struct mon_counter
{
    uint32_t        amc_counter;        /**< Counter                                 */
    char            amc_file[64];       /**< Filename:line of last check-in          */

    /* Private members */
    uint32_t        PRIV(amc_counter);  /**< Private counter                         */
    ticks_t         PRIV(amc_last);     /**< Timestamp of detected counter change    */

#ifdef MON_USE_TID_TO_KILL
    pid_t           amc_thread_id;      /**< Linux specific, thread ID               */
#endif
};

static bool         mon_process_wait(pid_t child, ticks_t timeout);
static void         mon_install_parent_signals();
static void         mon_reset_parent_signals();
static void         mon_install_child_signals();
static void         mon_counter_reset(void);

static void         mon_sig_crash(int signum);
static void         mon_sig_forward(int signum);
static void         mon_sig_str(int signum, char *strsig, size_t strsig_sz);

static bool         mon_check_process(pid_t child, int *exit_status);
static bool         mon_check_counter(pid_t child, int *exit_status);

/** The monitor counter list will be initialized in a shared memory segment between the child and the
    parent process */
static struct mon_counter *mon_counter_list       = NULL;
/** SIGTERM and SIGINT will be forwarded to the PID specified in mon_sig_forward_pid */
static pid_t                    mon_sig_forward_pid    = 0;

/**
 * Start the process monitor. The parent will be the child monitor; the child will return and proceed with the execution of the application
 */
void mon_start(int argc, char *argv[])
{
    (void)argc;

    pid_t   child;

    char    old_name[OS_TASK_NAME_SZ];
    char    new_name[OS_TASK_NAME_SZ];

    int     exit_status  = MON_EXIT_RESTART;
    bool    exit_restart = true;

    if (!task_name_get(old_name, sizeof(old_name)))
    {
        LOG(WARNING, "MONITOR: Error acquiring current task name.");
        *old_name = '\0';
    }

    /* If MONITOR_DISABLE is set, break out of the main loop immediately. */
    if (getenv("MONITOR_DISABLE") != NULL)
    {
        LOG(WARNING, "MONITOR_DISABLE is set, process restarting will be disabled.\n");
        exit_restart = false;
    }

    do
    {
        /* Initialize counters */
        mon_counter_reset();

        /* Fork the child and exit immediately */
        child = fork();

        if (child == 0)
        {
            /* Set child's task name */
            if (*old_name != '\0')
            {
                snprintf(new_name, sizeof(new_name), "%.8s.slave", old_name);
                task_name_set(new_name);
            }

            /* In order to cleanup after our own child, set it to be the process group leader. This way we acquire the ability
               to send signals to its children */
            setpgid(0, 0);

            mon_install_child_signals();

            return;
        }

        if (*old_name != '\0')
        {
            snprintf(new_name, sizeof(new_name), "%.8s.master", old_name);
            task_name_set(new_name);
        }

        LOG(NOTICE, "%s monitor started process %d.\n", argv[0], child);

        /* mon_sig_forward_pid is used to forward SIGTERM and SIGINT to the child process; this is done by the signal
           handler installed by mon_install_parent_signals() */
        mon_sig_forward_pid = child;
        mon_install_parent_signals();

        while (true)
        {
            if (!mon_check_process(child, &exit_status))
            {
                break;
            }

            if (!mon_check_counter(child, &exit_status))
            {
                break;
            }

            usleep(MON_LOOP_DELAY * 1000);
        }

        mon_reset_parent_signals();

        if (exit_status == MON_EXIT_RESTART)
        {
            /* MONITOR_DISABLE is set, do not restart the process ... */
            if (!exit_restart) break;

            /* The process should be restarted, sleep a bit before doing so */
            usleep(MON_RESTART_DELAY * 1000);
        }

    }
    while (exit_status == MON_EXIT_RESTART);

    LOG(NOTICE, "Main monitor loop exiting with status: %d\n", exit_status);

    /* Parent exit */
    exit(exit_status);
}

/**
 * Wait for a process or process group to terminate, this function works on any process not just direct descendants.
 *
 * If @p child is a direct descendant, this function will collect the waitable status therefore reaping any defunct processes.
 */
bool mon_process_wait(pid_t child, ticks_t timeout)
{
   timeout += ticks();

    while (timeout >= ticks())
    {
        int status;

        /* Reap defunct processes -- kill(child, 0) succeeds on defunct process, so make sure they disappear before we check for their existence */
        while (waitpid(child, &status, WNOHANG) > 0);

        if (kill(child, 0) != 0) return true;

        usleep(100 * 1000); // 100 ms
    }

    return false;
}

/**
 * Kill a whole process group
 */
void mon_process_terminate(pid_t child)
{
    int ii;

    /* First try sending SIGTERM 3 times */
    for (ii = 0; ii < 3; ii++)
    {
        LOG(DEBUG, "Sending SIGTERM to process %d ...\n", child);
        if (kill(child, SIGTERM) != 0)
        {
            return;
        }

        if (mon_process_wait(child, 1 * TICKS_HZ))
        {
            return;
        }
    }

    /* Children don't want to die, bring out the heavy guns */
    for (ii = 0; ii < 3; ii++)
    {
        LOG(DEBUG, "Sending SIGKILL to process %d ... \n", child);

        if (kill(child, SIGKILL) != 0)
        {
            return;
        }

        if (mon_process_wait(child, 1 * TICKS_HZ))
        {
            return;
        }

    }

    LOG(ERR, "Giving up on process termination (%d)...\n", child);
}

/**
 * Check if @p child is still alive; if not check the cause of death and extract the exit_status appropriately
 *
 * In case the process died normally, it just forward the exit status to the @p exit_status variable
 *
 * If the cause of death is a signal, then it tries to restart the child by setting exit_status to MON_EXIT_RESTART
 * The exception to this rule are SIGTERM and SIGINT, which are interpreted as exit requests
 */
bool mon_check_process(pid_t child, int *exit_status)
{
    int ret;
    int status = 0;

    *exit_status = -1;

    /*
     *  Check if the child exited; if it did check why:
     *      - If it died due to a signal, it probably crashed or got killed by the counter watcher, in which case we should restart it
     *      - If it exited regularly, use the child return status to exit
     */
    ret = waitpid(child, &status, WNOHANG);
    if (ret == 0)
    {
        /* Child is still alive, return true */
        return true;
    }
    else if (ret == -1)
    {
        /* This is an abnormal condition; restart the child */
        LOG(ERR, "Child %d suddenly disappeared: %s\n", child, strerror(errno));
        *exit_status = MON_EXIT_RESTART;
        return false;
    }

    /*
     * ret is above 0, this means that the child died. Determine the cause of death and extract the exit status
     */
    if (WIFEXITED(status))
    {
        /* Child exited normally, just return it's status */
        *exit_status  = WEXITSTATUS(status);

        LOG(NOTICE, "Child %d exited with status %d ...\n", child, *exit_status);
    }
    else if (WIFSIGNALED(status))
    {
        char sigstr[MON_SIGSTR_SZ];

        mon_sig_str(WTERMSIG(status), sigstr, sizeof(sigstr));
        /* The child was terminated by a signal, check what to do */
        switch (WTERMSIG(status))
        {
            case SIGTERM:
            case SIGINT:
                LOG(NOTICE, "Child terminated by non-fatal signal %s, exiting ...\n", sigstr);
                *exit_status = WEXITSTATUS(status);
                break;

            default:
                LOG(NOTICE, "Child terminated abruptly by signal %s, restarting ...\n", sigstr);
                *exit_status = MON_EXIT_RESTART;
                break;
        }
    }

    return false;
}

/**
 * Check the process counters
 *
 * The child is responsible of incrementing these counters; if it fails to do so, it is assumed that the child is stuck.
 * In this case SIGABRT is sent to the child and the counter timer is reset. mon_check_process() should pick-up the
 * dead zombie once it's done handling SIGABRT.
 */
bool mon_check_counter(pid_t child, int *exit_status)
{
    int cnt;

    (void)exit_status;

    bool aborted = false;

    for (cnt = 0; cnt < MON_LAST; cnt++)
    {
        struct mon_counter    *mc      = &mon_counter_list[cnt];
        ticks_t                curtime = ticks();

        /* Ignore counters that never started or are stopped */
        if (mc->PRIV(amc_counter) == 0) continue;

        if (mc->PRIV(amc_last) == 0) mc->PRIV(amc_last) = curtime;

        /* If the counters differ, refresh the timestamp and the private counter */
        if (mc->amc_counter != mc->PRIV(amc_counter))
        {
            mc->PRIV(amc_counter) = mc->amc_counter;
            mc->PRIV(amc_last)    = curtime;
        }
        else
        {
            /* If the counter was not update in the last 60 seconds, return false */
            if ((curtime - mc->PRIV(amc_last)) > TICKS_S(MON_COUNTER_MAX))
            {
                LOG(ERR, "FROZEN COUNTER: Counter id %d, last checked-in at \"%s\".\n", cnt, mc->amc_file);

                /* Reset the counter timer */
                mc->PRIV(amc_last) = curtime;

#ifdef MON_USE_TID_TO_KILL
                /*
                 * On Linux, first try to kill the thread ID instead of the process ID;
                 * By doing this, we have some chance to get the stack trace of the stuck thread
                 *
                 * However, if the thread for some reason exited, kill the main process
                 */
                if (!aborted && mc->amc_thread_id != 0 )
                {
                    LOG(ERR, "Aborting thread %d ...\n", mc->amc_thread_id);
                    if (kill(mc->amc_thread_id, SIGABRT) == 0)
                    {
                        aborted = true;
                    }
                }
#endif
                /* Try to send only one ABORT per iteration */
                if (!aborted)
                {
                    LOG(ERR, "Aborting process %d ...\n", child);
                    if (kill(child, SIGABRT) == 0)
                    {
                        aborted = true;
                    }
                }
            }
        }
    }

    return true;
}

/**
 * Allocate the counters shared-memory region and reset counters
 */
void mon_counter_reset(void)
{
    if (mon_counter_list == NULL)
    {
        /* Allocate shared memory segment with mmap */
        mon_counter_list = mmap(NULL,
                                     sizeof(struct mon_counter) * MON_LAST,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED | MAP_ANON,
                                     -1,
                                     0);

        if (mon_counter_list == NULL) assert(!"Error initializing mon_counter_list");
    }

    memset(mon_counter_list, 0, sizeof(struct mon_counter) * MON_LAST);
}


/**
 * Install handlers into the parent process
 */
void mon_install_parent_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = mon_sig_forward;
    sa.sa_flags     = 0;

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* The default action for USR1 & USR2 is to abort the process; we don't want that */
    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = SIG_IGN;
    sa.sa_flags     = 0;

    sigaction(SIGUSR1,  &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
}

/**
 * Reset signals that were previously installed by mon_install_parent_signals()
 */
void mon_reset_parent_signals(void)
{
    struct sigaction sa;

    /* Reset default handlers for SIGINT and SIGTERM which were overwritten in the parent process by
       mon_install_parent_signals() */
    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = SIG_DFL;
    sa.sa_flags     = SA_RESETHAND;

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

}

/**
 * Install child crash handlers
 */
void mon_install_child_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = mon_sig_crash;
    sa.sa_flags     = SA_RESETHAND;

    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL , &sa, NULL);
    sigaction(SIGFPE , &sa, NULL);
    sigaction(SIGBUS , &sa, NULL);

    /* SIGUSR2 is used just for stack reporting, while SIGINT and SITERM are forwarded to the child */
    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = mon_sig_crash;
    sa.sa_flags     = 0;

    sigaction(SIGUSR2, &sa, NULL);
}

/**
 * The actual crash handler
 */
void mon_sig_crash(int signum)
{
    char sigstr[MON_SIGSTR_SZ];

    mon_sig_str(signum, sigstr, sizeof(sigstr));
    LOG(ALERT, "Signal %s received, generating stack dump...\n", sigstr);

    backtrace_dump();

    if (signum != SIGUSR2)
    {
        /* At this point the handler for this signal was reset (except for SIGUSR2) due to the SA_RESETHAND flag;
           so re-send the signal to ourselves in order to properly crash */
        raise(signum);
    }
}

/**
 * Signal forwarder; this is typically used to forward SIGINT and SIGTERM to the child process
 */
void mon_sig_forward(int signum)
{
    if (mon_sig_forward_pid > 0)
    {
        char sigstr[MON_SIGSTR_SZ];

        mon_sig_str(signum, sigstr, sizeof(sigstr));

        LOG(DEBUG, "Forwarding signal %s to pid %d\n", sigstr, mon_sig_forward_pid);
        if (kill(mon_sig_forward_pid, signum) != 0)
        {
            LOG(DEBUG, "SIGNAL %s was not delivered as process %d is dead.\n", sigstr, mon_sig_forward_pid);
            mon_sig_forward_pid = 0;
        }
    }
}

/**
 * Return a description of signal @p signum, for example SIGSEGV -> SIGSEGV (Segmentation fault)
 */
void mon_sig_str(int signum, char *sigstr, size_t sigstr_sz)
{
    char sig_default[16];
    char *sig;

    switch (signum)
    {
        case SIGHUP:       sig = "SIGHUP";      break;
        case SIGINT:       sig = "SIGINT";      break;
        case SIGQUIT:      sig = "SIGQUIT";     break;
        case SIGILL:       sig = "SIGILL";      break;
        case SIGTRAP:      sig = "SIGTRAP";     break;
        case SIGABRT:      sig = "SIGABRT";     break;
        case SIGBUS:       sig = "SIGBUS";      break;
        case SIGFPE:       sig = "SIGFPE";      break;
        case SIGKILL:      sig = "SIGKILL";     break;
        case SIGUSR1:      sig = "SIGUSR1";     break;
        case SIGSEGV:      sig = "SIGSEGV";     break;
        case SIGUSR2:      sig = "SIGUSR2";     break;
        case SIGPIPE:      sig = "SIGPIPE";     break;
        case SIGALRM:      sig = "SIGALRM";     break;
        case SIGTERM:      sig = "SIGTERM";     break;
        case SIGCHLD:      sig = "SIGCHLD";     break;
        case SIGCONT:      sig = "SIGCONT";     break;
        case SIGSTOP:      sig = "SIGSTOP";     break;
        case SIGTSTP:      sig = "SIGTSTP";     break;
        case SIGTTIN:      sig = "SIGTTIN";     break;
        case SIGTTOU:      sig = "SIGTTOU";     break;
        case SIGURG:       sig = "SIGURG";      break;
        case SIGXCPU:      sig = "SIGXCPU";     break;
        case SIGXFSZ:      sig = "SIGXFSZ";     break;
        case SIGVTALRM:    sig = "SIGVTALRM";   break;
        case SIGPROF:      sig = "SIGPROF";     break;
        case SIGWINCH:     sig = "SIGWINCH";    break;
        case SIGIO:        sig = "SIGIO";       break;
        case SIGPWR:       sig = "SIGPWR";      break;
        case SIGSYS:       sig = "SIGSYS";      break;

        default:
            snprintf(sig_default, sizeof(sig_default), "%d", signum);
            sig = sig_default;
            break;
    }

    snprintf(sigstr, sigstr_sz, "%s (%s)", sig, strsignal(signum));
}

/**
 * Increment a single counter
 */
void mon_checkin(enum mon_cnt_id id, char *file, int line)
{
    pid_t thread_id;

    struct mon_counter *mc;

    if (id >= MON_LAST) assert(!"Invalid counter");

    mc = &mon_counter_list[id];

    thread_id = syscall(SYS_gettid);

    snprintf(mc->amc_file, sizeof(mc->amc_file), "%s:%d (%d)", file, line, thread_id);
    mc->amc_thread_id = thread_id;
    mc->amc_counter++;
}
