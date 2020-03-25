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

#ifndef __ASSERT__H__
#define __ASSERT__H__

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

#include "log.h"
#include "os_backtrace.h"

#define ASSERT_EXIT_RESTART     64

#define ASSERT_RET(cond, ret, fmt...)                                              \
   if (!(cond))                                                                    \
{                                                                                  \
   LOG(ERR, "%s: ASSERT: %s, %s:%d ", __FUNCTION__, #cond, __FILE__, __LINE__);    \
   LOG(ERR, fmt);                                                                  \
   backtrace_dump();                                                               \
   exit(ret); /* Exit with errors .... */                                          \
}

/** Asserts restart the agent, while errors shut it down */
#define ASSERT(cond, fmt...)   ASSERT_RET(cond, ASSERT_EXIT_RESTART, fmt)
#define FAIL(cond, fmt...)     ASSERT_RET(cond, 1, fmt)

#define ASSERT_INVALID_ARG "invalid argument"

#define ASSERT_ARG(cond) ASSERT(cond, ASSERT_INVALID_ARG)

#endif /* __ASSERT__H__ */

