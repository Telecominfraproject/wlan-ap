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
 * evsched.c
 */

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <ev.h>

#include <log.h>
#include <ds_list.h>

#include "evsched.h"


// Defines
#define MODULE_ID LOG_MODULE_ID_SCHED

#define TIME_JUMP_THRESHOLD     86400       // One day in seconds


/*****************************************************************************/


typedef struct {
    evsched_task_t          task_id;

    bool                    resched;
    bool                    remove;
    uint32_t                ms;
    ev_tstamp               sched_time;
    ev_tstamp               trigger_time;

    evsched_task_func_t     func;
    void                    *func_arg;

    ds_list_t               node;
} evsched_taskinfo_t;


/*****************************************************************************/

static evsched_taskinfo_t           *evsched_current;
static evsched_task_t               evsched_task_id = 1;
static struct ev_loop               *evsched_loop;
static ds_list_t                    evsched_tasklist;
static ds_list_t                    evsched_pending;
static ev_timer                     evsched_timer;
static bool                         evsched_initialized = false;


/*****************************************************************************/


// Private functions
static evsched_taskinfo_t   *evsched_get_taskinfo(evsched_task_t task, bool remove);
static void                 evsched_reset_timer(ev_tstamp trigger);
static void                 evsched_timer_callback(struct ev_loop *loop,
                                                      ev_timer *timer, int revents);
static bool                 evsched_task_insert(evsched_taskinfo_t *ntp, bool restart);


/*****************************************************************************/

static evsched_taskinfo_t *
evsched_get_taskinfo(evsched_task_t task, bool remove)
{
    evsched_taskinfo_t  *tp;
    ds_list_iter_t      iter;

    tp = ds_list_ifirst(&iter, &evsched_tasklist);
    while(tp) {
        if (tp->task_id == task) {
            if (remove) {
                ds_list_iremove(&iter);
            }

            return tp;
        }

        tp = ds_list_inext(&iter);
    }

    return NULL;
}

static void
evsched_reset_timer(ev_tstamp trigger)
{
    ev_timer_stop(evsched_loop, &evsched_timer);
    ev_timer_set(&evsched_timer, trigger, 0);
    ev_timer_start(evsched_loop, &evsched_timer);
    return;
}

static void
evsched_timer_callback(struct ev_loop *loop, ev_timer *timer, int revents)
{
    evsched_taskinfo_t      *tp;
    ds_list_iter_t          iter;
    ev_tstamp               cur_tm = ev_now(evsched_loop);

    // Avoid compiler warnings
    (void)loop;
    (void)timer;
    (void)revents;

    tp = ds_list_ifirst(&iter, &evsched_tasklist);
    while(tp) {
        if (tp->trigger_time > cur_tm) {
            break;
        }

        // Remove it from our task list
        ds_list_iremove(&iter);

        if (tp->remove) {
            // Marked for removal, so free it
            LOGT("Task %u canceled", tp->task_id);
            free(tp);
        }
        else {
            // Call function
            evsched_current = tp;
            tp->func(tp->func_arg);
            evsched_current = NULL;

            if (tp->resched) {
                // Queue it to be rescheduled
                tp->sched_time = cur_tm;
                ds_list_insert_tail(&evsched_pending, tp);
            }
            else {
                // we're done with it, let's free it
                free(tp);
            }
        }

        tp = ds_list_inext(&iter);
    }

    // Walk and free tasks marked for removal
    tp = ds_list_ifirst(&iter, &evsched_tasklist);
    while(tp) {
        if (tp->remove) {
            // Remove it from our task list
            ds_list_iremove(&iter);

            // Marked for removal, so free it
            LOGT("Task %u canceled", tp->task_id);
            free(tp);
        }

        tp = ds_list_inext(&iter);
    }

    // Reinsert pending tasks queued for rescheduling
    tp = ds_list_ifirst(&iter, &evsched_pending);
    while(tp) {
        ds_list_iremove(&iter);

        // Reinsert it
        if (evsched_task_insert(tp, false) == false) {
            LOGE("evsched_timer_callback() failed to reschedule task %u", tp->task_id);
            free(tp);
        }

        tp = ds_list_inext(&iter);
    }

    // See if we need to restart our timer
    if ((tp = ds_list_ifirst(&iter, &evsched_tasklist))) {
        evsched_reset_timer(tp->trigger_time - ev_now(evsched_loop));
    }

    return;
}

static bool
evsched_task_insert(evsched_taskinfo_t *ntp, bool restart)
{
    evsched_taskinfo_t      *tp;
    ds_list_iter_t          iter;

    // Calculate the trigger time for this task
    ntp->trigger_time = ntp->sched_time + ((float)ntp->ms / 1000);

    // Clear reschedule flag
    ntp->resched = false;

    // Check for time jump
    tp = ds_list_head(&evsched_tasklist);
    if (tp && ((ntp->sched_time - tp->sched_time) > TIME_JUMP_THRESHOLD)) {
        // Time has jumped.  Best we can do is fix-up existing events to run
        // immediately.  Not ideal, but best we can do for now.
        LOGW("Detected time jump! Events may happen sooner then requested");
        while(tp) {
            if ((ntp->sched_time - tp->sched_time) > TIME_JUMP_THRESHOLD) {
                tp->sched_time = ntp->sched_time;
                tp->trigger_time = tp->sched_time;
            }
            else {
                break;
            }

            tp = ds_list_next(&evsched_tasklist, tp);
        }

        // Reschedule timer for immediate run
        if (restart) {
            evsched_reset_timer(0);
        }
    }

    // Insert into task list
    tp = ds_list_ifirst(&iter, &evsched_tasklist);
    while(tp) {
        if (ntp->trigger_time < tp->trigger_time) {
            ds_list_iinsert(&iter, ntp);
            break;
        }

        tp = ds_list_inext(&iter);
    }
    if (!tp) {
        ds_list_insert_tail(&evsched_tasklist, ntp);
    }

    // See if we need to restart our timer
    if (restart && ds_list_head(&evsched_tasklist) == ntp) {
        evsched_reset_timer(ntp->trigger_time - ntp->sched_time);
    }

    return true;
}


/*****************************************************************************/

bool
evsched_init(struct ev_loop *loop)
{
    if (evsched_initialized)
        return true;

    LOGI("Initializing...");

    // Save our loop
    if (loop) {
        evsched_loop = loop;
    }
    else {
        evsched_loop = EV_DEFAULT;
    }

    // Initialize our link list of tasks
    ds_list_init(&evsched_tasklist, evsched_taskinfo_t, node);
    ds_list_init(&evsched_pending,  evsched_taskinfo_t, node);

    // Initialize our EV timer
    ev_init(&evsched_timer, evsched_timer_callback);

    evsched_current = NULL;
    evsched_initialized = true;
    return true;
}

bool
evsched_cleanup(void)
{
    evsched_taskinfo_t      *tp;
    ds_list_iter_t          iter;

    if (!evsched_initialized) {
        return true;
    }

    LOGI("Cleaning up...");

    // Stop our timer
    ev_timer_stop(evsched_loop, &evsched_timer);

    // Free our task list
    tp = ds_list_ifirst(&iter, &evsched_tasklist);
    while(tp) {
        ds_list_iremove(&iter);
        free(tp);

        tp = ds_list_inext(&iter);
    }

    evsched_initialized = false;
    return true;
}

evsched_task_t
evsched_task(evsched_task_func_t func, void *arg, uint32_t ms)
{
    evsched_taskinfo_t      *ntp;

    if (!evsched_initialized) {
        LOGE("evsched_task() called before initialization!");
        return 0;
    }

    ntp = calloc(1, sizeof(*ntp));
    if (!ntp) {
        LOGE("evsched_task() failed to allocate memory for new task!");
        return 0;
    }

    // Assign it a task id
    ntp->task_id = evsched_task_id++;

    // Store it's info
    ntp->ms         = ms;
    ntp->func       = func;
    ntp->func_arg   = arg;
    ntp->sched_time = ev_now(evsched_loop);

    // If we're running a task now, put it in the pending queue
    if (evsched_current) {
        ds_list_insert_tail(&evsched_pending, ntp);
        LOGT("Task %u queued to be scheduled (%u ms)", ntp->task_id, ntp->ms);
    }
    else {
        // Insert it into our task list
        if (evsched_task_insert(ntp, true) == false) {
            LOGE("evsched_task() failed to insert task into tasklist!");
            free(ntp);
            return 0;
        }

        LOGT("Task %u scheduled (%u ms)", ntp->task_id, ntp->ms);
    }

    // Return task id
    return ntp->task_id;
}

evsched_task_t
evsched_task_find(evsched_task_func_t func, void *arg, uint8_t find_by)
{
    evsched_taskinfo_t      *tp;

    if (!evsched_initialized) {
        LOGE("evsched_task_find() called before initialization!");
        return 0;
    }

    if ((find_by & (EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG)) == 0) {
        LOGT("evsched_task_find() called with invalid find_by flags (0x%02X)", find_by);
        return 0;
    }

    ds_list_foreach(&evsched_tasklist, tp) {
        if ((find_by & EVSCHED_FIND_BY_FUNC) && tp->func != func) {
            continue;
        }

        if ((find_by & EVSCHED_FIND_BY_ARG) && tp->func_arg != arg) {
            continue;
        }

        // Found a match
        return tp->task_id;
    }

    // Not found
    return 0;
}

uint32_t
evsched_task_remaining(evsched_task_t task)
{
    evsched_taskinfo_t      *tp;
    ev_tstamp               cur_tm = ev_now(evsched_loop);

    if (!evsched_initialized) {
        LOGE("evsched_task_remaining() called before initialization!");
        return 0;
    }

    tp = evsched_get_taskinfo(task, false);
    if (tp) {
        return (uint32_t)((tp->trigger_time - cur_tm) * 1000);
    }

    return 0;
}

bool
evsched_task_update(evsched_task_t task, uint32_t ms)
{
    evsched_taskinfo_t      *tp;

    if (!evsched_initialized) {
        LOGE("evsched_task_update() called before initialization!");
        return false;
    }

    if (evsched_current && evsched_current->task_id == task) {
        // Can't update current running process.
        // should call evsched_task_reschedule_ms() instead
        return false;
    }

    tp = evsched_get_taskinfo(task, true);
    if (!tp) {
        return false;
    }

    tp->sched_time = ev_now(evsched_loop);
    if (evsched_current) {
        // Queue it up
        ds_list_insert_tail(&evsched_pending, tp);
        LOGT("Task %u queued to be updated (%u ms)", tp->task_id, tp->ms);
    }
    else {
        // Update ms and reinsert it
        tp->ms = ms;
        if (evsched_task_insert(tp, true) == false) {
            LOGE("evsched_task_update() failed to re-insert task into tasklist!");
            free(tp);
            return false;
        }

        LOGT("Task %u updated (%u ms)", task, ms);
    }
    return true;
}

bool
evsched_task_cancel(evsched_task_t task)
{
    evsched_taskinfo_t      *tp;

    if (!evsched_initialized) {
        LOGE("evsched_task_cancel() called before initialization!");
        return false;
    }

    if (evsched_current) {
        // Running a task, cannot remove it now
        if (task == evsched_current->task_id) {
            // Just clear out resched flag if set
            evsched_current->resched = false;
        }
        else {
            // Mark it for removal
            tp = evsched_get_taskinfo(task, false);
            if (!tp) {
                return false;
            }
            tp->remove = true;
            LOGT("Task %u marked for cancellation", task);
        }
        return true;
    }

    // Remove and free it now
    tp = evsched_get_taskinfo(task, true);
    if (!tp) {
        return false;
    }
    free(tp);

    LOGT("Task %u canceled", task);
    return true;
}

bool
evsched_task_cancel_by_find(evsched_task_func_t func, void *arg, uint8_t find_by)
{
    evsched_task_t      task;

    task = evsched_task_find(func, arg, find_by);
    if (task == 0) {
        return false;
    }

    return evsched_task_cancel(task);
}

bool
evsched_task_reschedule(void)
{
    if (!evsched_current) {
        return false;
    }

    LOGT("Task %u reschedule requested (same %u ms)", evsched_current->task_id, evsched_current->ms);
    evsched_current->resched = true;

    return true;
}

bool
evsched_task_reschedule_ms(uint32_t ms)
{
    if (!evsched_current) {
        return false;
    }

    LOGT("Task %u reschedule requested (new %u ms)", evsched_current->task_id, ms);
    evsched_current->resched = true;
    evsched_current->ms = ms;

    return true;
}
