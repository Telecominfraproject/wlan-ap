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

#ifndef DAEMON_H_INCLUDED
#define DAEMON_H_INCLUDED

#include <stdlib.h>
#include <stdbool.h>
#include <ev.h>

#include "const.h"
#include "read_until.h"

typedef struct __daemon daemon_t;
typedef bool daemon_atexit_fn_t(daemon_t *self);

#define DAEMON_LOG_STDOUT   (1 << 0)    /* Log stdout of the daemoness */
#define DAEMON_LOG_STDERR   (1 << 1)    /* Log stderr of the daemoness */

#define DAEMON_LOG_ALL      (DAEMON_LOG_STDOUT | DAEMON_LOG_STDERR)

#define DAEMON_PIPE_BUF     256

/**
 * Daemon structure
 */
struct __daemon
{
    char                *dn_exec;           /* Path to executable */
    int                 dn_flags;           /* Flags */
    pid_t               dn_pid;             /* PID of the currently running daemoness */
    int                 dn_argc;            /* Number of arguments in list, excluding the terminating NULL */
    char                **dn_argv;          /* Argument list */
    daemon_atexit_fn_t  *dn_atexit_fn;
    ev_timer            dn_restart_timer;   /* Restart timer */
    double              dn_restart_delay;   /* Time between restarts */
    int                 dn_restart_max;     /* Maximum tries before giving up */
    int                 dn_restart_num;     /* Current number of restarts */
    ev_child            dn_child;           /* Child daemoness watcher */
    ev_io               dn_stdout;          /* Daemon stdout watcher */
    int                 dn_stdout_fd;       /* Stdout file descriptor or -1 if not applicable */
    char                dn_stdout_buf[DAEMON_PIPE_BUF];
    read_until_t        dn_stdout_ru;       /* _read_until() structure */
    ev_io               dn_stderr;          /* Daemon stderr watcher */
    int                 dn_stderr_fd;       /* Stderr file descriptor or -1 if not applicable */
    char                dn_stderr_buf[DAEMON_PIPE_BUF];
    read_until_t        dn_stderr_ru;       /* _read_until() structure */
    int                 dn_sig_term;        /* Signal to send instead of SIGTERM (ask to terminate nicely) */
    int                 dn_sig_kill;        /* Signal to send instead of SIGKILL (force kill) */
    bool                dn_enabled;         /* The daemoness is enabled */
    bool                dn_auto_restart;    /* Enable daemoness auto restart on error */
    char                *dn_pidfile_path;   /* PID file */
    bool                dn_pidfile_create;  /* true whether we should create the PID file */
};

extern bool daemon_init(daemon_t *self, const char *exe_path, int flags);
extern void daemon_fini(daemon_t *self);
extern bool daemon_start(daemon_t *self);
extern bool daemon_stop(daemon_t *self);
extern bool daemon_wait(daemon_t *self, double timeout);
extern bool daemon_arg_reset(daemon_t *self);
extern bool daemon_arg_add_a(daemon_t *self, char *argv[]);
extern bool daemon_signal_set(daemon_t *self, int sig_term, int sig_kill);
extern bool daemon_restart_set(daemon_t *self, bool auto_restart, double delay, int retries);
extern bool daemon_atexit(daemon_t *self, daemon_atexit_fn_t *fn);
extern bool daemon_pidfile_set(daemon_t *self, const char *path, bool create);
extern bool daemon_is_started(daemon_t *self, bool *started);

#define daemon_arg_add(self, ...)   \
    daemon_arg_add_a(self, C_VPACK(__VA_ARGS__))

#endif /* DAEMON_H_INCLUDED */
