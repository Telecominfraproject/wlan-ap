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

/*
 * ===========================================================================
 *  Daemon monitoring implementation using libev
 * ===========================================================================
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

#include "log.h"
#include "util.h"
#include "daemon.h"
#include "read_until.h"

#define DAEMON_DEFAULT_RESTART_DELAY      1.0   /* Default delay between daemon restarts in secods */
#define DAEMON_DEFAULT_RESTART_MAX        5     /* Number of retries before giving up */
#define DAEMON_DEFAULT_KILL_TIMEOUT       6.0   /* Seconds to wait for child termination */

/* Forward declaration of private functions */
static void __daemon_arg_reset(daemon_t *self);
static bool __daemon_pidfile_read(daemon_t *self, pid_t *pid);
static bool __daemon_pidfile_create(daemon_t *self);
static bool __daemon_start(daemon_t *self);
static void __daemon_teardown(daemon_t *self, int wstatus);
static void __daemon_log_output(struct ev_loop *loop, ev_io *w, int revent);
static void __daemon_child_ev(struct ev_loop *loop, ev_child *w, int revent);
static void __daemon_restart_ev(struct ev_loop *loop, ev_timer *w, int revent);
static bool __daemon_kill_unrelated(daemon_t *self, pid_t pid);
static bool __daemon_set_nonblock(int fd, bool enable);
static ssize_t __daemon_flush_pipe(daemon_t *self, read_until_t *ru, int fd);


/**
 * Initialize a daemon structure
 */
bool daemon_init(daemon_t *self, const char *exe_path, int flags)
{
    memset(self, 0, sizeof(*self));

    self->dn_flags = flags;

    if (access(exe_path, X_OK) != 0)
    {
        LOG(ERR, "daemon: Path %s is not executable by current user.", exe_path);
        return false;
    }

    self->dn_exec = strdup(exe_path);

    /* Add the executable to the argument list */
    if (!daemon_arg_reset(self))
    {
        LOG(ERR, "daemon: Error initializing arguments.");
        free(self->dn_exec);
        return false;
    }

    self->dn_sig_term = SIGTERM;
    self->dn_sig_kill = SIGKILL;

    self->dn_restart_delay = DAEMON_DEFAULT_RESTART_DELAY;

    self->dn_stdout_fd = -1;
    self->dn_stderr_fd = -1;

    self->dn_child.data = self;
    self->dn_stdout.data = self;
    self->dn_stderr.data = self;
    self->dn_restart_timer.data = self;

    return true;
}

/**
 * Deinitialize the daemon_t structure, free all resources (should it kill the daemon?)
 */
void daemon_fini(daemon_t *self)
{
    if (!daemon_stop(self))
    {
        LOG(ERR, "daemon: Unable to kill the daemon in destructor: %s (pid %jd).",
                self->dn_exec,
                (intmax_t)self->dn_pid);
    }

    __daemon_arg_reset(self);

    if (self->dn_pidfile_path != NULL) free(self->dn_pidfile_path);
    if (self->dn_exec != NULL) free(self->dn_exec);
}

/**
 * Reset the argument list
 */
bool daemon_arg_reset(daemon_t *self)
{
    __daemon_arg_reset(self);

    /* Automatically add the executable to the argument list */
    return daemon_arg_add(self, self->dn_exec);
}

void __daemon_arg_reset(daemon_t *self)
{
    int ii;

    /* Free the argument list */
    for (ii = 0; ii < self->dn_argc; ii++)
    {
        if (self->dn_argv[ii] != NULL) free(self->dn_argv[ii]);
    }

    self->dn_argc = 0;
    if (self->dn_argv != NULL) free(self->dn_argv);
    self->dn_argv = NULL;
}

/**
 * Add arguments
 */
bool daemon_arg_add_v(daemon_t *self, const char *arg, ...)
{
    va_list va;
    const char *pa;
    int nargc;

    /* Count the number of new arguments */
    nargc = 0;
    va_start(va, arg);
    for (pa = arg; pa != NULL; pa = va_arg(va, const char *))
    {
        nargc++;
    }
    va_end(va);

    /* Reallocate argument list */
    self->dn_argv = realloc(
            self->dn_argv,
            sizeof(self->dn_argv[0]) * (self->dn_argc + nargc + 1));

    if (self->dn_argv == NULL)
    {
        self->dn_argc = 0;
        return false;
    }

    va_start(va, arg);
    for (pa = arg; pa != NULL; pa = va_arg(va, const char *))
    {
        self->dn_argv[self->dn_argc++] = strdup(pa);
    }
    va_end(va);

    self->dn_argv[self->dn_argc] =  NULL;

    return true;
}

/**
 * Add arguments
 */
bool daemon_arg_add_a(daemon_t *self, char *argv[])
{
    char **parg;

    int nargc;

    /* Count the number of new arguments */
    nargc = 0;
    for (parg = argv; *parg != NULL; parg++)
    {
        nargc++;
    }

    /* Reallocate argument list */
    self->dn_argv = realloc(
            self->dn_argv,
            sizeof(self->dn_argv[0]) * (self->dn_argc + nargc + 1));

    if (self->dn_argv == NULL)
    {
        self->dn_argc = 0;
        return false;
    }

    for (parg = argv; *parg != NULL; parg++)
    {
        self->dn_argv[self->dn_argc++] = strdup(*parg);
    }

    self->dn_argv[self->dn_argc] =  NULL;

    return true;
}


/**
 * Start the daemon process
 */
bool daemon_start(daemon_t *self)
{
    pid_t stale_pid;

    /* Is the process already started? */
    if (self->dn_enabled) return true;

    /* Reset the restart counter */
    self->dn_restart_num = 0;

    /* Kill any stale processes */
    if (__daemon_pidfile_read(self, &stale_pid))
    {
        LOG(INFO, "daemon: Found stale instance of daemon %s at pid %jd. Killing it.",
                self->dn_exec,
                (intmax_t)stale_pid);

        /* Note there's no way to monitor unrelated processes besides polling */
        if (!__daemon_kill_unrelated(self, stale_pid))
        {
            LOG(ERR, "daemon: Unable to kill stale daemon instance %s at pid %jd. Is it a zombie?",
                    self->dn_exec,
                    (intmax_t)stale_pid);

            //return false;
        }
    }

    self->dn_enabled = __daemon_start(self);

    return self->dn_enabled;
}

/* Start the process, 2nd half of the procedure */
bool __daemon_start(daemon_t *self)
{
    int fd;
    int pipe_fd[2];

    /* Replace STDIN, STDOUT, STDERR */
    int fd_devnull;
    int fd_stdout;
    int fd_stderr;

    fd_devnull = open("/dev/null", O_RDWR);
    if (fd_devnull < 0)
    {
        LOG(ERR, "Error opening /dev/null");
        fd_devnull = -1;
    }

    fd_stdout = fd_devnull;
    fd_stderr = fd_devnull;

    if (self->dn_flags & DAEMON_LOG_STDOUT)
    {
        if (pipe(pipe_fd) == 0)
        {
            fd_stdout = pipe_fd[1];
            self->dn_stdout_fd = pipe_fd[0];

            if (!__daemon_set_nonblock(self->dn_stdout_fd, true))
            {
                LOG(WARN, "daemon: Error setting non-blocking mode on STDOUT pipe.");
            }

            read_until_init(
                    &self->dn_stdout_ru,
                    self->dn_stdout_buf,
                    sizeof(self->dn_stdout_buf));

            ev_io_init(&self->dn_stdout, __daemon_log_output, self->dn_stdout_fd, EV_READ);
            ev_io_start(EV_DEFAULT, &self->dn_stdout);
        }
        else
        {
            LOG(ERR, "daemon: pipe() failed, unable to redirect stdout");
        }
    }

    if (self->dn_flags & DAEMON_LOG_STDERR)
    {
        if (pipe(pipe_fd) == 0)
        {
            self->dn_stderr_fd = pipe_fd[0];
            fd_stderr = pipe_fd[1];

            if (!__daemon_set_nonblock(self->dn_stderr_fd, true))
            {
                LOG(WARN, "daemon: Error setting non-blocking mode on STDERR pipe.");
            }

            read_until_init(
                    &self->dn_stderr_ru,
                    self->dn_stderr_buf,
                    sizeof(self->dn_stderr_buf));

            ev_io_init(&self->dn_stderr, __daemon_log_output, self->dn_stderr_fd, EV_READ);
            ev_io_start(EV_DEFAULT, &self->dn_stderr);
        }
        else
        {
            LOG(ERR, "daemon: pipe() failed, unable to redirect stderr");
        }
    }

    self->dn_pid = fork();
    /* Error case */
    if (self->dn_pid == -1)
    {
        /* Do some cleanups */
        close(fd_devnull);
        close(fd_stdout);
        close(fd_stderr);
        close(self->dn_stdout_fd);
        close(self->dn_stderr_fd);

        self->dn_stdout_fd = self->dn_stderr_fd = -1;

        return false;
    }
    /* Parent case */
    else if (self->dn_pid != 0)
    {
        LOG(INFO, "Started daemon %s (%jd)...", self->dn_exec, (intmax_t)self->dn_pid);

        close(fd_devnull);
        close(fd_stdout);
        close(fd_stderr);

        /* Write out the PID file */
        if (self->dn_pidfile_create && !__daemon_pidfile_create(self))
        {
            LOG(WARN, "Error creating PID file for daemon: %s.", self->dn_exec);
        }

        /* Register the child handler */
        ev_child_init(&self->dn_child, __daemon_child_ev, self->dn_pid, 0);
        ev_child_start(EV_DEFAULT, &self->dn_child);

        return true;
    }

    /* Child case */
    int maxfd = sysconf(_SC_OPEN_MAX);

    /* Duplicate the file descriptors */
    dup2(fd_devnull, 0);
    dup2(fd_stdout, 1);
    dup2(fd_stderr, 2);

    /* Close all file descriptors */
    for (fd = 3; fd < maxfd; fd++) close(fd);

    execv(self->dn_exec, self->dn_argv);

    LOG(ERR, "Error executing daemon %s ... errno = %d (%s)",
            self->dn_exec,
            errno,
            strerror(errno));

    exit(255);
}

void __daemon_child_ev(struct ev_loop *loop, ev_child *w, int revent)
{
    daemon_t *self = (daemon_t *)w->data;

    /* Stop any active io watchers */
    if (ev_is_active(&self->dn_child)) ev_child_stop(EV_DEFAULT, &self->dn_child);
    if (ev_is_active(&self->dn_stdout)) ev_io_stop(EV_DEFAULT, &self->dn_stdout);
    if (ev_is_active(&self->dn_stderr)) ev_io_stop(EV_DEFAULT, &self->dn_stderr);

    __daemon_teardown(self, w->rstatus);
}

void __daemon_teardown(daemon_t *self, int wstatus)
{
    /* Figure out the reason the process died */
    if (WIFSIGNALED(wstatus))
    {
#if defined(WCOREDUMP)
        if (!WCOREDUMP(wstatus))
#else
        if (true)
#endif
        {
            LOG(INFO, "Daemon %s (%jd) was terminated by signal %d.",
                    self->dn_exec,
                    (intmax_t)self->dn_pid,
                    WTERMSIG(wstatus));
        }
        else
        {
            LOG(INFO, "Daemon %s (%jd) crashed (signal %d) and produced a core dump.",
                    self->dn_exec,
                    (intmax_t)self->dn_pid,
                    WTERMSIG(wstatus));
        }
    }
    else if (WIFEXITED(wstatus))
    {
        LOG(INFO, "Daemon %s (%jd) exited with exit status %d.",
                self->dn_exec,
                (intmax_t)self->dn_pid,
                WEXITSTATUS(wstatus));
    }
    else
    {
        LOG(INFO, "Daemon %s (%jd) exited abnormally with exit status %d.",
                self->dn_exec,
                (intmax_t)self->dn_pid,
                WEXITSTATUS(wstatus));
    }

    /* Close pipes */
    if (self->dn_stdout_fd >= 0)
    {
        (void)__daemon_flush_pipe(self, &self->dn_stdout_ru, self->dn_stdout_fd);
        close(self->dn_stdout_fd);
        self->dn_stdout_fd = -1;
    }

    if (self->dn_stderr_fd >= 0)
    {
        (void)__daemon_flush_pipe(self, &self->dn_stderr_ru, self->dn_stderr_fd);
        close(self->dn_stderr_fd);
        self->dn_stderr_fd = -1;
    }

    if (self->dn_enabled && self->dn_auto_restart)
    {
        if (self->dn_restart_num++ < self->dn_restart_max)
        {
            /* Schedule a restart */
            ev_timer_init(
                    &self->dn_restart_timer,
                    __daemon_restart_ev,
                    self->dn_restart_delay,
                    self->dn_restart_delay);

            ev_timer_start(EV_DEFAULT, &self->dn_restart_timer);

            LOG(INFO, "Restarting daemon %s (%d/%d).",
                    self->dn_exec,
                    self->dn_restart_num,
                    self->dn_restart_max);
            return;
        }
    }

    /* ataxit() callback */
    if (self->dn_enabled && self->dn_atexit_fn != NULL)
    {
        (void)self->dn_atexit_fn(self);
    }

    /* Remove the PID file */
    if (self->dn_pidfile_path != NULL)
    {
        /* Some processes do not clean-up their PID file */
        unlink(self->dn_pidfile_path);
    }

    self->dn_pid = 0;
}

ssize_t __daemon_flush_pipe(daemon_t *self, read_until_t *ru, int fd)
{
    char bname[C_MAXPATH_LEN];
    char *name;
    char *prefix = "E";
    char *line;
    ssize_t nrd;

    STRSCPY(bname, self->dn_exec);

    name = basename(bname);

    if (fd == self->dn_stdout_fd)
    {
        prefix = ">";
    }
    else if (fd == self->dn_stderr_fd)
    {
        prefix = "|";
    }

    while ((nrd = read_until(ru, &line, fd, "\n")) > 0)
    {
        LOG(INFO, "%s [%jd] %s %s", name, (intmax_t)self->dn_pid, prefix, line);
    }

    return nrd;
}

void __daemon_log_output(struct ev_loop *loop, ev_io *w, int revent)
{
    daemon_t *self = (daemon_t *)w->data;

    read_until_t *ru = NULL;
    ssize_t nrd;

    if (!(revent & EV_READ)) return;

    if (w->fd == self->dn_stdout_fd)
    {
        ru = &self->dn_stdout_ru;
    }
    else if (w->fd == self->dn_stderr_fd)
    {
        ru = &self->dn_stderr_ru;
    }

    if (ru == NULL) return;

    nrd = __daemon_flush_pipe(self, ru, w->fd);

    /* Non-blocking IO -- return if this is a temporary error */
    if (nrd == -1 && errno == EAGAIN) return;

    /* nrd <= 0 and errno is not EAGAIN */
    ev_io_stop(loop, w);

    if (w->fd == self->dn_stdout_fd)
    {
        close(self->dn_stdout_fd);
        self->dn_stdout_fd = -1;
    }

    if (w->fd == self->dn_stderr_fd)
    {
        close(self->dn_stderr_fd);
        self->dn_stderr_fd = -1;
    }
}

void __daemon_restart_ev(struct ev_loop *loop, ev_timer *w, int revent)
{
    daemon_t *self = (daemon_t *)w->data;

    if (!__daemon_start(self))
    {
        LOG(ERR, "Failed to restart daemon %s ...", self->dn_exec);
        return;
    }

    ev_timer_stop(loop, w);
}

void __daemon_waitpid_fn(struct ev_loop *loop, ev_timer *w, int revent)
{
}

/*
 * kill a pid (ask politely first) with timeout, if the process is a child,
 * reap the status after exit or timeout and return it
 *
 * wstatus will be set to -1 if we didn't successfully reap the status, make sure
 * to check for that value before using any W*() macros
 *
 * WARNING:
 * Do not use the EV_DEFAULT loop unless you are prepared to handle reentrancy
 * issues.
 *
 * Please note that the ev_child watcher works only with the EV_DEFAULT loop,
 * however we're not allowed to use that here!
 */
bool daemon_waitpid(
        struct ev_loop *loop,
        pid_t pid,
        double timeout,
        int *wstatus)
{
    bool retval = false;

    if (loop == EV_DEFAULT)
    {
        LOG(ERR, "daemon: daemon_waitpid() doesn't accept EV_DEFAULT as loop.");
        return false;
    }

    /*
     * Configure a repeating timer -- this is mainly used to break from
     * ev_run() periodically
     */
    ev_timer tmr;
    ev_timer_init(&tmr, __daemon_waitpid_fn, 0.1, 0.1);
    ev_timer_start(loop, &tmr);

    /* Start the timeout timer */
    ev_timer to;
    ev_timer_init(&to, __daemon_waitpid_fn, timeout, 0.0);
    ev_timer_start(loop, &to);

    *wstatus = -1;

    while (ev_run(loop, EVRUN_ONCE) && ev_is_active(&to))
    {
        /* Check if we can reap the status */
        if (waitpid(pid, wstatus, WNOHANG) == pid)
        {
            /* Sucess, return the status */
            retval = true;
            goto exit;
        }
        /* Alternative method for checking if the process died -- in case the PID is not a child */
        else if (kill(pid, 0) != 0)
        {
            retval = true;
            goto exit;
        }
    }

exit:
    if (ev_is_active(&to)) ev_timer_stop(loop, &to);
    if (ev_is_active(&tmr)) ev_timer_stop(loop, &tmr);

    return retval;
}

/**
 * Kill an unrelated process
 *
 * This function fails to detect if the process-to-be-terminatd is a zombie. In such case it will
 * return false (termination failed).
 *
 * TODO Add some /proc magic to determine if the process is a zombie or not.
 */
bool __daemon_kill_unrelated(daemon_t *self, pid_t pid)
{
    int wstatus;

    bool retval= false;
    struct ev_loop *stop_loop = NULL;

    stop_loop = ev_loop_new(EVFLAG_AUTO);
    if (stop_loop == NULL)
    {
        LOG(EMERG, "daemon: Error creating stop loop, unable to stop daemon %s with pid %jd.", self->dn_exec, (intmax_t)pid);
        goto exit;
    }

    /* Start with SIGTERM */
    kill(pid, self->dn_sig_term);
    if (daemon_waitpid(stop_loop, pid, DAEMON_DEFAULT_KILL_TIMEOUT, &wstatus))
    {
        retval = true;
        goto exit;
    }

    LOG(WARN, "daemon: Stale instance of daemon %s at pid %jd is ignoring termination signals. Forcibly killing it.",
            self->dn_exec,
            (intmax_t)pid);

    /* SIGTERM didn't work, try SIGKILL */
    kill(pid, self->dn_sig_kill);
    if (daemon_waitpid(stop_loop, pid, DAEMON_DEFAULT_KILL_TIMEOUT, &wstatus))
    {
        retval = true;
        goto exit;
    }

    LOG(ERR, "daemon: Stale instance of daemon %s at pid %jd cannot be killed. Is it a zombie?", self->dn_exec, (intmax_t)pid);

exit:
    if (stop_loop != NULL) ev_loop_destroy(stop_loop);

    /* The process is unstoppable ... stuck in kernel space or is it a zombie? */
    return retval;
}

/**
 * Gracefully stop the daemon
 */
bool daemon_stop(daemon_t *self)
{
    if (!self->dn_enabled) return true;
    self->dn_enabled = false;

    /* Stop any impeding restart timers */
    if (ev_is_active(&self->dn_restart_timer))
    {
        ev_timer_stop(EV_DEFAULT, &self->dn_restart_timer);
    }

    /* Try to ask nicely first */
    if (self->dn_pid != 0)
    {
        LOG(INFO, "Terminating daemon %s (%jd).",
                self->dn_exec,
                (intmax_t)self->dn_pid);

        kill(self->dn_pid, self->dn_sig_term);
        daemon_wait(self, DAEMON_DEFAULT_KILL_TIMEOUT);
    }

    if (self->dn_pid != 0)
    {
        LOG(WARN, "Daemon %s (%jd) refused to die. Forcing a kill...",
                self->dn_exec,
                (intmax_t)self->dn_pid);

        kill(self->dn_pid, self->dn_sig_kill);
        daemon_wait(self, DAEMON_DEFAULT_KILL_TIMEOUT);
    }

    if (self->dn_pid != 0)
    {
        LOG(ERR, "Unable to stop daemon %s (%jd).",
                self->dn_exec,
                (intmax_t)self->dn_pid);

        return false;
    }

    return true;
}

/**
 * Wait for a maximum of @p timeout seconds for the process to finish
 *
 * This function is BLOCKING
 */
bool daemon_wait(daemon_t *self, double timeout)
{
    int wstatus;

    bool retval = false;
    struct ev_loop *stop_loop = NULL;
    bool restart_ev_child = false;

    /* Create a new internal loop */
    stop_loop = ev_loop_new(EVFLAG_AUTO);
    if (stop_loop == NULL)
    {
        LOG(ERR, "daemon: Unable to create STOP loop.");
        return false;
    }

    /* Stop the child watcher temporarily */
    if (ev_is_active(&self->dn_child))
    {
        ev_child_stop(EV_DEFAULT, &self->dn_child);
        restart_ev_child = true;
    }

    /*
     * Move the rest of the watchers to the stop loop, we don't want to be randomly
     * preempted from the default loop but we still want to see any output the daemon
     * may provide during shutdown
     */
    if (ev_is_active(&self->dn_stdout))
    {
        ev_io_stop(EV_DEFAULT, &self->dn_stdout);
        ev_io_start(stop_loop, &self->dn_stdout);
    }

    if (ev_is_active(&self->dn_stderr))
    {
        ev_io_stop(EV_DEFAULT, &self->dn_stderr);
        ev_io_start(stop_loop, &self->dn_stderr);
    }

    if (daemon_waitpid(stop_loop, self->dn_pid, timeout, &wstatus))
    {
        /* Child exited, begin teardown sequence */
        __daemon_teardown(self, wstatus);
        retval = true;
    }

exit:
    /* retval == false, the child is still active, restart the watchers on the default loop */
    if (ev_is_active(&self->dn_stdout))
    {
        ev_io_stop(stop_loop, &self->dn_stdout);
        if (!retval) ev_io_start(EV_DEFAULT, &self->dn_stdout);
    }

    if (ev_is_active(&self->dn_stderr))
    {
        ev_io_stop(stop_loop, &self->dn_stderr);
        if (!retval) ev_io_start(EV_DEFAULT, &self->dn_stderr);
    }

    if (restart_ev_child)
    {
        if (!retval) ev_child_start(EV_DEFAULT, &self->dn_child);
    }

    if (stop_loop != NULL) ev_loop_destroy(stop_loop);

    return retval;
}

/**
 * Specify signals to use when terminating the child; -1 to use the current value
 */
bool daemon_signal_set(daemon_t *self, int sig_term, int sig_kill)
{
    if (sig_term >= 0)
        self->dn_sig_term = sig_term;
    else
        self->dn_sig_term = SIGTERM;

    if (sig_kill >= 0)
        self->dn_sig_kill = sig_kill;
    else
        self->dn_sig_kill = SIGKILL;

    return true;
}

/**
 * Set auto-restart options
 */
bool daemon_restart_set(daemon_t *self, bool enabled, double delay, int max)
{
    self->dn_auto_restart = enabled;

    if (delay > 0.0)
        self->dn_restart_delay = delay;
    else
        self->dn_restart_delay = DAEMON_DEFAULT_RESTART_DELAY;

    if (max > 0)
        self->dn_restart_max = max;
    else
        self->dn_restart_max = DAEMON_DEFAULT_RESTART_MAX;

    return true;
}

/**
 * Set the atexit callback
 */
bool daemon_atexit(daemon_t *self, daemon_atexit_fn_t *fn)
{
    self->dn_atexit_fn = fn;
    return true;
}

/**
 * Return true if the daemon has been started (this does not mean that it is
 * necessarily runing (during autorestart))
 */
bool daemon_is_started(daemon_t *self, bool *started)
{
    *started = self->dn_enabled;
    return true;
}


/**
 * Set the PID file path
 *
 * Set the path to the PID file, if @p create is true then it is assumed that the daemon
 * does not create its own PID file, so we must do it instead
 *
 * PID files are used to terminate any stale instances of the daemon (for example, after a restart)
 */
bool daemon_pidfile_set(daemon_t *self, const char *path, bool create)
{
    self->dn_pidfile_create = create;
    self->dn_pidfile_path = strdup(path);

    return true;
}

bool __daemon_pidfile_read(daemon_t *self, pid_t *pid)
{
    FILE *f = NULL;
    bool retval = false;
    char buf[C_PID_LEN];

    *pid = -1; // set to -1 if error

    if (access(self->dn_pidfile_path, R_OK) != 0) goto error;

    f = fopen(self->dn_pidfile_path, "r");
    if (f == NULL)
    {
        LOG(WARN, "daemon: pid file exists but it cannot be opened.");
        goto error;
    }

    if (fgets(buf, sizeof(buf), f) == NULL)
    {
        /* The PID file is empty */
        goto error;
    }

    errno = 0;
    *pid = strtoull(buf, NULL, 10);
    if (errno != 0)
    {
        LOG(WARN, "daemon: pid file is corrupted.");
        goto error;
    }

    retval = true;

error:
    if (f != NULL) fclose(f);

    return retval;
}

bool __daemon_pidfile_create(daemon_t *self)
{
    FILE *pf;

    if (self->dn_pid <= 0)
    {
        LOG(ERR, "daemon: pid invalid, cannot create PID file.");
        return false;
    }

    if (!self->dn_pidfile_create) return true;

    pf = fopen(self->dn_pidfile_path, "w");
    if (pf == NULL)
    {
        LOG(ERR, "daemon: Error writing pid file (%d) for daemon: %s",
                self->dn_pid,
                self->dn_exec);
        return false;
    }

    if (fprintf(pf, "%d\n", self->dn_pid) <= 0)
    {
        LOG(ERR, "daemon: Error writing pid file I/O error, daemon: %s", self->dn_exec);
    }

    fclose(pf);

    return true;
}

static bool __daemon_set_nonblock(int fd, bool enable)
{
    int opt = enable ? 1 : 0;

    if (ioctl(fd, FIONBIO, &opt) != 0) return false;

    return true;
}
