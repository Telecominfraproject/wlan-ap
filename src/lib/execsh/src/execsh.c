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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ev.h>

#include "log.h"
#include "const.h"
#include "read_until.h"
#include "execsh.h"

static void execsh_closefrom(int fd);
static bool execsh_set_nonblock(int fd, bool enable);
static pid_t execsh_pspawn(const char *path, char *argv[], int *pstdin, int *pstdout, int *pstderr);

static void __execsh_fn_std_write(struct ev_loop *loop, ev_io *w, int revent);
static void __execsh_fn_std_read(struct ev_loop *loop, ev_io *w, int revent);

/* pipe() indexes */
#define P_RD    0       /* Read end */
#define P_WR    1       /* Write end */

/**
 * execsh_fn() this is the main execsh function -- all other functions are based on this one.
 */
struct __execsh
{
    const char      *pscript;
    execsh_fn_t     *fn;
    void            *ctx;
    int             stdin_fd;
    int             stdout_fd;
    read_until_t    stdout_ru;
    int             stderr_fd;
    read_until_t    stderr_ru;
};

int execsh_fn_v(execsh_fn_t *fn, void *ctx, const char *script, va_list __argv)
{
    int retval = -1;

    /* Build the argument list */
    char **argv = NULL;

    /*
     * Build the argument list
     */
    int argc = 0;
    int argm = 16;
    char *parg;

    argv = malloc(argm * sizeof(char *));
    if (argv == NULL)
    {
        LOG(ERR, "execsh_fn_v: Error allocating argv buffer");
        goto exit;
    }

    /* Add variable arguments */
    while ((parg = va_arg(__argv, char *)) != NULL)
    {
        if (argc + 1 > argm)
        {
            argm <<= 1;
            argv = realloc(argv, argm * sizeof(char *));
            if (argv == NULL)
            {
                LOG(ERR, "execsh_fn_v: Error re-allocating argv buffer");
                goto exit;
            }
        }

        argv[argc++] = parg;
    }

    /* Terminate it with NULL */
    argv[argc] = NULL;

    retval = execsh_fn_a(fn, ctx, script, argv);

exit:
    if (argv != NULL) free(argv);

    return retval;
}

int execsh_fn_a(execsh_fn_t *fn, void *ctx, const char *script, char *__argv[])
{
    struct __execsh es;

    pid_t cpid = -1;
    int retval = -1;

    int pin[2] = { -1 };
    int pout[2] = { -1 };
    int perr[2] = { -1 };
    char **argv = NULL;
    struct ev_loop *loop = NULL;

    /**
     * Create STDIN/STDOUT/STDERR: common pipes
     */
    if (pipe(pin) != 0)
    {
        LOG(ERR, "execsh: Error creating STDIN pipes.");
        goto exit;
    }

    if (pipe(pout) != 0)
    {
        LOG(ERR, "execsh: Error creating STDOUT pipes.");
        goto exit;
    }

    if (pipe(perr) != 0)
    {
        LOG(ERR, "execsh: Error creating STDERR pipes.");
        goto exit;
    }

    /*
     * Build the argument list
     */
    int argc = 0;
    int argm = 16;
    char **parg;

    argv = malloc(argm * sizeof(char *));
    if (argv == NULL)
    {
        LOG(ERR, "execsh: Error allocating argv buffer");
        goto exit;
    }

    argv[argc++] = EXECSH_SHELL_PATH;
    argv[argc++] = "-e";                /* Abort on error */
    argv[argc++] = "-x";                /* Verbose */
    argv[argc++] = "-s";                /* Read script from ... */
    argv[argc++] = "-";                 /* ... STDIN */

    /* Add variable arguments */
    for (parg = __argv; *parg != NULL; parg++)
    {
        if (argc + 1 > argm)
        {
            argm <<= 1;
            argv = realloc(argv, argm * sizeof(char *));
            if (argv == NULL)
            {
                LOG(ERR, "execsh: Error re-allocating argv buffer");
                goto exit;
            }
        }

        argv[argc++] = *parg;
    }

    /* Terminate it with NULL */
    argv[argc] = NULL;

    /* Finally, after all this prep work, run the child */
    cpid = execsh_pspawn(EXECSH_SHELL_PATH, argv, pin, pout, perr);

    /* Callback context */
    char outbuf[EXECSH_PIPE_BUF];
    char errbuf[EXECSH_PIPE_BUF];

    es.fn = fn;
    es.ctx = ctx;
    es.pscript = script;
    es.stdin_fd = pin[P_WR];
    es.stdout_fd = pout[P_RD];
    es.stderr_fd = perr[P_RD];

    read_until_init(&es.stdout_ru, outbuf, sizeof(outbuf));
    read_until_init(&es.stderr_ru, errbuf, sizeof(errbuf));

    /*
     * Initialize and start watchers
     */
    ev_io win;
    ev_io wout;
    ev_io werr;

    loop = ev_loop_new(EVFLAG_AUTO);
    if (loop == NULL)
    {
        LOG(ERR, "execsh: Error creating loop, execsh() fialed.");
        goto exit;
    }

    ev_io_init(&win, __execsh_fn_std_write, pin[P_WR], EV_WRITE);
    win.data = &es;
    execsh_set_nonblock(pin[P_WR], true);
    ev_io_start(loop, &win);

    ev_io_init(&wout, __execsh_fn_std_read, pout[P_RD], EV_READ);
    wout.data = &es;
    execsh_set_nonblock(pout[P_RD], true);
    ev_io_start(loop, &wout);

    ev_io_init(&werr, __execsh_fn_std_read, perr[P_RD], EV_READ);
    werr.data = &es;
    execsh_set_nonblock(perr[P_RD], true);
    ev_io_start(loop, &werr);

    /* Loop until all watchers are active */
    while (ev_run(loop, EVRUN_ONCE))
    {
        bool loop = false;

        loop |= ev_is_active(&win);
        loop |= ev_is_active(&wout);
        loop |= ev_is_active(&werr);

        if (!loop) break;
    }

    /* Wait for the process to terminate */
    if (waitpid(cpid, &retval, 0) <= 0)
    {
        LOG(ERR, "execsh: Error waiting on child.");
        retval = -1;
    }

exit:
    if (loop != NULL) ev_loop_destroy(loop);

    if (argv != NULL) free(argv);

    int ii;
    for (ii = 0; ii < 2; ii++)
    {
        close(pin[ii]);
        close(pout[ii]);
        close(perr[ii]);
    }

    return retval;
}

void __execsh_fn_std_write(struct ev_loop *loop, ev_io *w, int revent)
{
    struct __execsh *pes = w->data;

    if (!(revent & EV_WRITE)) return;

    ssize_t nwr;

    while ((nwr = write(w->fd, pes->pscript, strlen(pes->pscript))) > 0)
    {
        pes->pscript += nwr;

        /* DId we reach the end of the string? */
        if (pes->pscript[0] == '\0')
        {
            break;
        }
    }

    if (nwr == -1 && errno == EAGAIN) return;

    ev_io_stop(loop, w);

    close(w->fd);
}

void __execsh_fn_std_read(struct ev_loop *loop, ev_io *w, int revent)
{
    struct __execsh *pes = w->data;

    int type = 0;
    read_until_t *ru = NULL;

    if (!(revent & EV_READ)) return;

    if (w->fd == pes->stdout_fd)
    {
        type = EXECSH_PIPE_STDOUT;
        ru = &pes->stdout_ru;
    }
    else if (w->fd == pes->stderr_fd)
    {
        type = EXECSH_PIPE_STDERR;
        ru = &pes->stderr_ru;
    }

    if (type == 0 || ru == NULL) return;

    char *line;
    ssize_t nrd;

    while ((nrd = read_until(ru, &line, w->fd, "\n")) > 0)
    {
        /* Close the pipe if the callback returns false */
        if (!pes->fn(pes->ctx, type, line)) break;
    }

    if (nrd == -1 && errno == EAGAIN) return;

    ev_io_stop(loop, w);
    close(w->fd);
}

void execsh_closefrom(int fd)
{
    int ii;

    int maxfd = sysconf(_SC_OPEN_MAX);

    for (ii = 3; ii < maxfd; ii++) close(ii);
}

/**
 * Spawn a process:
 *
 *  - path: Full path to executable
 *  - pstdin, pstdout, pstderr: piar of file descriptors
 *  - argv: null-terminated variable list of char *
 */
pid_t execsh_pspawn(
        const char *path,
        char *argv[],
        int *pstdin,
        int *pstdout,
        int *pstderr)
{
    int fdevnull = -1;

    pid_t child = fork();

    if (child == -1) return -1;

    if (child > 0)
    {
        /* Parent keeps write end of STDIN */
        if (pstdin != NULL)
        {
            close(pstdin[P_RD]);
            pstdin[P_RD] = -1;
        }

        /* Parent keeps read-end of STDOUT */
        if (pstdout != NULL)
        {
            close(pstdout[P_WR]);
            pstdout[P_WR] = -1;
        }

        /* Parent keeps read-end of STDERR */
        if (pstderr != NULL)
        {
            close(pstderr[P_WR]);
            pstderr[P_WR] = -1;
        }

        return child;
    }

    /* Point of no return */

    close(0);
    close(1);
    close(2);

    /* Child keeps read end of STDIN and write-ends of STDOUT and STDERR */
    if (pstdin != NULL) close(pstdin[P_WR]);
    if (pstdout != NULL) close(pstdout[P_RD]);
    if (pstderr != NULL) close(pstderr[P_RD]);

    fdevnull = open("/dev/null", O_RDWR);

    if (pstdin != NULL && pstdin[P_RD] >= 0)
    {
        dup2(pstdin[P_RD], 0);
    }
    else
    {
        dup2(fdevnull, 0);
    }

    if (pstdout != NULL && pstdout[P_WR] >= 0)
    {
        dup2(pstdout[P_WR], 1);
    }
    else
    {
        dup2(fdevnull, 1);
    }

    if (pstderr != NULL && pstderr[P_WR] >= 0)
    {
        dup2(pstderr[P_WR], 2);
    }
    else
    {
        dup2(fdevnull, 2);
    }

    /* Close all other file descriptors */
    execsh_closefrom(3);

    execv(path, argv);

    _exit(255);
    return -1;
}

/**
 * Set the non-blocking flag
 */
static bool execsh_set_nonblock(int fd, bool enable)
{
    int opt = enable ? 1 : 0;

    if (ioctl(fd, FIONBIO, &opt) != 0) return false;

    return true;
}

bool __execsh_log(void *ctx, int type, const char *msg)
{
    int *severity = ctx;

    mlog(*severity, MODULE_ID,
            "%s %s",
            type == EXECSH_PIPE_STDOUT ? ">" : "|",
            msg);

    return true;
}

int execsh_log_v(int severity, const char *script, va_list va)
{
    return execsh_fn_v(__execsh_log, &severity, script, va);
}

int execsh_log_a(int severity, const char *script, char *argv[])
{
    return execsh_fn_a(__execsh_log, &severity, script, argv);
}

