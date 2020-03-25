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

#include "evx.h"

/**
 * Debounce callback wrapper. It just resets the initial timestamp
 * and invokes the real callback.
 */
static void ev_debounce_fn(struct ev_loop *loop, ev_debounce *w, int revent)
{
    /* Reset the start timer */
    w->ts_start = 0.0;
    w->fn(loop, w, revent);
}

void ev_debounce_init2(
        ev_debounce *ev,
        ev_debounce_fn_t *fn,
        double timeout,
        double timeout_max)
{
    ev->timeout = timeout;
    ev->timeout_max = timeout_max;
    ev->fn = fn;
    ev_timer_init(&ev->timer, (void *)ev_debounce_fn, timeout, 0.0);
}

void ev_debounce_init(ev_debounce *ev, ev_debounce_fn_t *fn, double timeout)
{
    ev_debounce_init2(ev, fn, timeout, -1.0);
}

void ev_debounce_start(struct ev_loop *loop, ev_debounce *w)
{
    ev_tstamp timeout;

    if (ev_is_active(&w->timer))
    {
        ev_timer_stop(loop, &w->timer);
    }

    timeout = w->timeout;

    /* Check if maximum timeout is set */
    if (w->timeout_max > 0.0)
    {
        if (w->ts_start > 0.0)
        {
            /*
             * If the next timeout shoots past max_timeout, use max_timeout
             * instead
             */
            ev_tstamp max_timeout = w->timeout_max + w->ts_start -  ev_now(loop);
            if (max_timeout < timeout) timeout = max_timeout;
        }
        else
        {
            w->ts_start = ev_now(loop);
        }
    }

    if (timeout < 0.0) timeout = 0.0;

    /* Re-arm the timer */
    ev_timer_set(&w->timer, timeout, 0.0);
    ev_timer_start(loop, &w->timer);
}

void ev_debounce_stop(struct ev_loop *loop, ev_debounce *w)
{
    ev_timer_stop(loop, &w->timer);
}

