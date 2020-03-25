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

#include <pthread.h>

#include "os.h"
#include "os_util.h"
#include "log.h"

#define MODULE_ID LOG_MODULE_ID_EXEC

#define OS_CMD_LINE_SIZE 256
#define OS_CMD_BUF_CHUNK 1024

/* os_cmd_exec: a simple exec util
 *
 * input:
 *   flags     : control behaviour
 *   fmt, ...  : command line - printf format
 * output:
 *   buffer    : output of command, allocated and zero terminated
 *   len       : exact len of output - without zero termination
 *   exit_code : command exit code
 * return:
 *   true on success
 * default behvaiour (flags=0):
 *   log all output
 *   require exit_code = 0 or return failure
 *   on failure use error log severity
 */

bool os_cmd_exec_xv(char **buffer, int *len, int *exit_code, int flags, char *fmt, va_list args)
{
    va_list args2;
    char    cmd1[OS_CMD_LINE_SIZE];
    char    *cmd2 = NULL;
    char    *command = cmd1;
    int     cmdlen;
    FILE    *fp;
    int     ret = 0;
    int     xcode = 0;
    char    *buf = NULL;
    char    *tmp = NULL;
    int     bufsize = 0;
    int     buflen = 0;
    int     loglen = 0;
    int     errlevel = LOG_SEVERITY_ERROR;

    if (!buffer) {
        return false;
    }
    *buffer = NULL;
    if (len) *len = 0;
    if (exit_code) *exit_code = 0;

    errno = 0;
    va_copy(args2, args);
    cmdlen = vsnprintf(cmd1, sizeof(cmd1), fmt, args);

    if (cmdlen >= (int)sizeof(cmd1)) {
        // fixed cmd buffer too small, allocate and retry
        cmdlen++;
        cmd2 = alloca(cmdlen);
        cmdlen = vsnprintf(cmd2, cmdlen, fmt, args);
        command = cmd2;
    }
    va_end(args2);
    if (cmdlen <= 0) {
        command = fmt;
        goto error;
    }

    LOG(TRACE, "Executing '%s'", command);
    errno = 0;
    fp = popen(command, "r");
    if (fp == NULL) {
        goto error;
    }

    while (!feof(fp))
    {
        // always have chunk size and zero term ready to read
        bufsize = buflen + OS_CMD_BUF_CHUNK + 1;
        tmp = realloc(buf, bufsize);
        if (!tmp) {
            goto error;
        }
        buf = tmp;
        // read, reserve 1 for zero term
        errno = 0;
        ret = fread(buf + buflen, 1, bufsize - buflen - 1, fp);
        if (ret < 0) {
            LOG(DEBUG, "read: %d %d(%s)", ret, errno, strerror(errno));
            break;
        }
        buflen += ret;
        // zero term
        buf[buflen] = 0;
        // log lines
        if (!(flags & OS_CMD_FLAG_NO_LOG_OUTPUT)) {
            while (1) {
                char *p = buf + loglen;
                char *nl = strchr(p, '\n');
                if (!nl) break;
                if (nl - p > 0) {
                    LOG(TRACE, "> %.*s", (int) (nl - p), p);
                }
                loglen = nl - buf + 1;
            }
        }
    }
    // log remaining
    if (loglen < buflen) {
        LOG(TRACE, ": %*s", buflen - loglen, buf + loglen);
    }
    // reduce alloc size
    buf = realloc(buf, buflen + 1);
    // close
    errno = 0;
    xcode = pclose(fp);
    if (xcode < 0) goto error;
    if (exit_code) *exit_code = xcode;
    if (!(flags & OS_CMD_FLAG_ANY_EXIT_CODE)) {
        if (xcode != 0) goto error;
    }
    if (len) *len = buflen;
    *buffer = buf;
    LOG(TRACE, "exit=%d len=%d '%s'", xcode, buflen, command);

    return true;

error:
    if (flags & OS_CMD_FLAG_LOG_ERROR_DEBUG) {
        // useful when failure is expected
        errlevel = LOG_SEVERITY_DEBUG;
    }
    LOG_SEVERITY(errlevel, "exit=%d err=%d(%s) '%s'", xcode, errno, strerror(errno), command);
    if (buf) free(buf);
    return false;
}

bool os_cmd_exec_x(char **buffer, int *len, int *exit_code, int flags, char *fmt, ...)
{
    bool result;
    va_list args;
    va_start(args, fmt);
    result = os_cmd_exec_xv(buffer, len, exit_code, flags, fmt, args);
    va_end(args);
    return result;
}

bool os_cmd_exec(char **buffer, char *fmt, ...)
{
    bool result;
    va_list args;
    va_start(args, fmt);
    result = os_cmd_exec_xv(buffer, NULL, NULL, 0, fmt, args);
    va_end(args);
    return result;
}


