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

#ifndef __OS_BACKTRACE__H__
#define __OS_BACKTRACE__H__

#include <stdbool.h>

/**
 * Executables that want to take advantage of backtracing should be compiled with -fasnychronous-unwind-tables
 * and linked with -rdynamic to get the best results.
 *
 * Executables with debug symbols can be used post-mortem to get more accurate information about the stack trace;
 * for example the line number and source file. This is the syntax one would normally use is:
 *
 * # addr2line -e EXECUTABLE -ifp ADDRESS
 *
 * Where ADDRESS in this case is the address reported by the backtrack library.
 *
 */
typedef enum {
    BTRACE_FILE_LOG = 0,
    BTRACE_LOG_ONLY
} btrace_type;

typedef bool        backtrace_func_t(void *ctx, void *addr, const char *func, const char *object);

extern void         backtrace_init(void);
extern void         backtrace_dump(void);
extern bool         backtrace(backtrace_func_t *handler, void *ctx);
bool backtrace_copy(void **addr, int size, int *count, int *all);
bool backtrace_resolve(void *addr, const char **func, const char **fname);

// Path where crashdump is generated
#define BTRACE_DUMP_PATH "/var/log/lm/crash"

#endif /* #define __OS_BACKTRACE__H__ */
