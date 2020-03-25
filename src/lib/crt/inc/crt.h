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

#ifndef CRT_H_INCLUDED
#define CRT_H_INCLUDED

#include <string.h>

#define CRT_ERROR           1024
#define CRT_ERR_CALL        (CRT_ERROR + 1)
#define CRT_ERR_STACK       (CRT_ERROR + 2)

#define CRT_OK              0

#define CRT_STACK_DEPTH     32      /* Maximum CRT stack depth */

typedef struct crt crt_t;

struct crt
{
    int crt_line[CRT_STACK_DEPTH];  /* Co-routine stack */
};

#define CRT(FN, ...)                                    \
FN(crt_t *__crt, int __crt_depth, ##__VA_ARGS__)

#define CRT_INIT(crt)                                   \
do                                                      \
{                                                       \
    int __cii;                                          \
    for (__cii = 0; __cii < CRT_STACK_DEPTH; __cii++)   \
        (crt)->crt_line[__cii] = CRT_OK;                \
}                                                       \
while (0)

#define CRT_RUN(crt, fn, ...)   fn(crt, 0, ##__VA_ARGS__)

#define CRT_BEGIN()                                     \
if (__crt->crt_line[__crt_depth] < 0)                   \
    __crt->crt_line[__crt_depth] = 0;                   \
                                                        \
switch (__crt->crt_line[__crt_depth])                   \
{                                                       \
    case CRT_OK:                                        \

#define CRT_YIELD(...)                                  \
do                                                      \
{                                                       \
    __crt->crt_line[__crt_depth] = __LINE__;            \
    return __VA_ARGS__;                                 \
    case __LINE__:;                                     \
}                                                       \
while (0)

#define CRT_EXIT(code)                                  \
do                                                      \
{                                                       \
    __crt->crt_line[__crt_depth] = -(code);             \
    goto __crt_exit;                                    \
}                                                       \
while (0)

#define CRT_END(...)                                    \
    CRT_EXIT(CRT_OK);                                   \
                                                        \
    default:                                            \
        CRT_EXIT(CRT_ERR_STACK);                        \
}                                                       \
__crt_exit:

/* Call a co-routine repeatedly, and yield if the co-routine is not finished */
#define CRT_CALL_WAIT(statement, ...)                   \
do                                                      \
{                                                       \
    __crt->crt_line[__crt_depth] = __LINE__;            \
                                                        \
    case __LINE__:                                      \
        statement;                                      \
                                                        \
        if (!CRT_CALL_IFEXITED())                       \
            return __VA_ARGS__;                         \
}                                                       \
while (0)

#define CRT_CALL_VOID(...)          CRT_CALL_WAIT( { CRT_CALL(__VA_ARGS__);} )
#define CRT_CALL_RET(fn, r)         CRT_CALL_WAIT( { CRT_CALL fn;}, r )

#define CRT_MY_STATUS()             (-(__crt->crt_line[__crt_depth]))

#define CRT_STATUS(ctx)             (-((ctx)->crt_line[0]))
#define CRT_IFEXITED(ctx)           (CRT_STATUS(ctx) >= 0)

#define CRT_CALL(fn, ...)           fn(__crt, __crt_depth + 1, ##__VA_ARGS__)
#define CRT_CALL_STATUS()           (-(__crt->crt_line[__crt_depth + 1]))
#define CRT_CALL_IFEXITED()         (CRT_CALL_STATUS() >= 0)


#endif /* CRT_H_INCLUDED */
