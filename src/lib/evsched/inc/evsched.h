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
 * evsched.h
 */

#ifndef __EVSCHED_H__
#define __EVSCHED_H__

#include <stdbool.h>
#include <stdint.h>
#include <ev.h>

// Defines
#define EVSCHED_ASAP            0
#define EVSCHED_MS(x)           (x)
#define EVSCHED_SEC(x)          EVSCHED_MS(x * 1000)
#define EVSCHED_MIN(x)          (EVSCHED_SEC(60) * x)

#define EVSCHED_FIND_BY_FUNC    (1 << 0)
#define EVSCHED_FIND_BY_ARG     (1 << 1)    


/*****************************************************************************/

typedef uint32_t    evsched_task_t;
typedef void        (*evsched_task_func_t)(void *arg);


/*****************************************************************************/


// External Functions
extern evsched_task_t   evsched_task(evsched_task_func_t func, void *arg, uint32_t ms);
extern evsched_task_t   evsched_task_find(evsched_task_func_t func, void *arg, uint8_t find_by);
extern uint32_t         evsched_task_remaining(evsched_task_t task);
extern bool             evsched_task_update(evsched_task_t task, uint32_t ms);
extern bool             evsched_task_cancel(evsched_task_t task);
extern bool             evsched_task_cancel_by_find(evsched_task_func_t func, void *arg, uint8_t find_by);
extern bool             evsched_task_reschedule_ms(uint32_t ms);
extern bool             evsched_task_reschedule(void);
extern bool             evsched_init(struct ev_loop *loop);
extern bool             evsched_cleanup(void);

#endif /* __EVSCHED_H__ */
