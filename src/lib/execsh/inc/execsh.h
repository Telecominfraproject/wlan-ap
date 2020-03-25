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

#ifndef EXECSH_H_INCLUDED
#define EXECSH_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include "const.h"

#define EXECSH_SHELL_PATH       "/bin/sh"

#define EXECSH_PIPE_STDOUT      (1 << 0)
#define EXECSH_PIPE_STDERR      (1 << 1)

#define EXECSH_PIPE_BUF         256

/**
 * This macro works wonderfully for embedding shell code into C.
 * It also syntactically checks that quotes and braces are balanced.
 *
 * Most importantly, you don't have to escape every single double quote.
 */
#define _S(...)  #__VA_ARGS__

typedef bool execsh_fn_t(void *ctx, int msg_type, const char *buf);

/*
 * Execute a shell script specified by @p script synchronously,
 * redirect outputs to the logger.
 *
 * Think of it as a fancier system().
 *
 * The script specified in @p script is written to a temporary files and is
 * executed by passing the variable length arguments to it as parameters.
 */

/*
 * TODO: Add timeout?
 */
int execsh_fn_v(execsh_fn_t *fn, void *ctx, const char *script, va_list va);
int execsh_fn_a(execsh_fn_t *fn, void *ctx, const char *script, char *argv[]);

#define execsh_fn(fn, ctx, script, ...) \
    execsh_fn_a((fn), (ctx), (script), C_VPACK(__VA_ARGS__))

/*
 * Same as execsh_fn(), but redirect stderr/stdout to the logger instead.
 *
 * Example:
 * execsh_log(LOG_SEVERITY_INFO, ...);
 *
 */
int execsh_log_v(int severity, const char *script, va_list va);
int execsh_log_a(int severity, const char *script, char *argv[]);

#define execsh_log(severity, script, ...) \
    execsh_log_a(severity, script, C_VPACK(__VA_ARGS__))

#endif /* EXECSH_H_INCLUDED */
