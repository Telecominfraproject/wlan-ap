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

#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ev.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

#include "sm.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

static ds_dlist_t                   g_scan_ctx_list;
static uint32_t                     g_scan_queue_qty = 0;

#define SM_SCAN_SCHEDULE_TIMEOUT    (60) /* s */
static  ev_timer                    g_scan_schedule_timer;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

static
bool sm_scan_schedule_process (
        sm_scan_ctx_t             *scan_ctx);

static
sm_scan_ctx_t * sm_scan_ctx_alloc()
{
    sm_scan_ctx_t *scan_ctx = NULL;

    scan_ctx = malloc(sizeof(sm_scan_ctx_t));
    if (scan_ctx)
    {
        memset(scan_ctx, 0, sizeof(sm_scan_ctx_t));
    }

    return scan_ctx;
}

static
void sm_scan_ctx_free(sm_scan_ctx_t *scan_ctx)
{
    if (NULL != scan_ctx)
    {
        free(scan_ctx);
    }
}

static
bool scan_schedule_timeout_timer_set(
        ev_timer                   *timer,
        bool                        enable)
{
    if (enable)
    {
        ev_timer_again(EV_DEFAULT, timer);
    }
    else
    {
        ev_timer_stop(EV_DEFAULT, timer);
    }

    return true;
}

static
bool sm_scan_schedule_queue_flush ()
{
    sm_scan_ctx_t                  *scan_ctx = NULL;
    ds_dlist_iter_t                 queue_iter;

    for (   scan_ctx = ds_dlist_ifirst(&queue_iter, &g_scan_ctx_list);
            scan_ctx != NULL;
            scan_ctx = ds_dlist_inext(&queue_iter))
    {
        LOG(ERR,
            "Flushed scheduled %s %s %d scan queues",
            radio_get_name_from_type(scan_ctx->scan_request.radio_cfg->type),
            radio_get_scan_name_from_type(scan_ctx->scan_request.scan_type),
            scan_ctx->scan_request.chan_list[0]);

        /* Remove processed context */
        ds_dlist_iremove(&queue_iter);
        sm_scan_ctx_free(scan_ctx);
        scan_ctx = NULL;
    }
    g_scan_queue_qty = 0;

    return true;
}

static
bool sm_scan_schedule_cb (
        void                       *ctx,
        int                         scan_status)
{
    bool                            rc;
    sm_scan_ctx_t                  *scan_ctx = NULL;

    scan_schedule_timeout_timer_set(&g_scan_schedule_timer, false);

    /* Get pending scan */
    if (!ctx) {
        LOG(ERR,
            "Processing scan schedule (Failed to get context)");
        return false;
    }
    scan_ctx = (sm_scan_ctx_t *) ctx;

clean:
    /* Notify upper layer about scan status */
    if (scan_ctx->scan_request.scan_cb)
    {
        scan_ctx->scan_request.scan_cb(scan_ctx->scan_request.scan_ctx, scan_status);
    }

    // Something went wrong, clean pending requests and exit
    if (!scan_status) {
        return sm_scan_schedule_queue_flush();
    }

    /* Remove processed context */
    ds_dlist_remove_head(&g_scan_ctx_list);
    sm_scan_ctx_free(scan_ctx);
    scan_ctx = NULL;

    /* Check for waiting scan requests an schedule next in line */
    scan_ctx = ds_dlist_head(&g_scan_ctx_list);
    if (scan_ctx)
    {
        scan_status = true;
        rc =
            sm_scan_schedule_process (
                    scan_ctx);
        if (true != rc)
        {
            /* In case of error notify upper layer and remove entry */
            scan_status = false;
            goto clean;
        }
    }

    return true;
}

static
void scan_schedule_timeout_cb (EV_P_ ev_timer *w, int revents)
{
    sm_scan_ctx_t                  *scan_ctx =
        (sm_scan_ctx_t *) w->data;
    sm_scan_request_t              *request_ctx =
        &scan_ctx->scan_request;

    LOG(CRIT,
        "Schedule %s %s %d scan timeout occured",
        radio_get_name_from_type(request_ctx->radio_cfg->type),
        radio_get_scan_name_from_type(request_ctx->scan_type),
        request_ctx->chan_list[0]);

    sm_scan_schedule_stop(
            request_ctx->radio_cfg,
            request_ctx->scan_type);
}

static
bool sm_scan_schedule_process(
        sm_scan_ctx_t              *scan_ctx)
{
    bool                            rc;
    sm_scan_request_t              *request_ctx =
        &scan_ctx->scan_request;

    LOG(TRACE,
        "Scheduled %s %s %d scan",
        radio_get_name_from_type(request_ctx->radio_cfg->type),
        radio_get_scan_name_from_type(request_ctx->scan_type),
        request_ctx->chan_list[0]);

    g_scan_queue_qty--;

    ev_init (&g_scan_schedule_timer, scan_schedule_timeout_cb);
    g_scan_schedule_timer.repeat =  SM_SCAN_SCHEDULE_TIMEOUT;
    g_scan_schedule_timer.data = scan_ctx;
    scan_schedule_timeout_timer_set(&g_scan_schedule_timer, true);

    /* The scan was scheduled therefore call the
       target scan start and wait for it to be finished
     */
    rc =
        target_stats_scan_start (
                request_ctx->radio_cfg,
                request_ctx->chan_list,
                request_ctx->chan_num,
                request_ctx->scan_type,
                request_ctx->dwell_time,
                sm_scan_schedule_cb,
                scan_ctx);
    if (true != rc) {
        LOG(ERR,
            "Scheduling %s %s %d scan",
            radio_get_name_from_type(request_ctx->radio_cfg->type),
            radio_get_scan_name_from_type(request_ctx->scan_type),
            request_ctx->chan_list[0]);
        scan_schedule_timeout_timer_set(&g_scan_schedule_timer, false);
        return false;
    }

    return true;
}

bool sm_scan_is_request_pending(
        sm_scan_request_t          *request)
{
    sm_scan_ctx_t                  *scan_ctx = NULL;
    ds_dlist_iter_t                 ctx_iter;

    for (   scan_ctx = ds_dlist_ifirst(&ctx_iter, &g_scan_ctx_list);
            scan_ctx != NULL;
            scan_ctx = ds_dlist_inext(&ctx_iter))
    {
        if (    (scan_ctx->scan_request.radio_cfg == request->radio_cfg)
             && (scan_ctx->scan_request.scan_type == request->scan_type)
             && (scan_ctx->scan_request.scan_cb   == request->scan_cb)
           ) {
            return true;
        }
    }

    return false;
}

/******************************************************************************
 *  PUBLIC definitions
 *****************************************************************************/
bool sm_scan_schedule_init()
{
    ds_dlist_init(
            &g_scan_ctx_list,
            sm_scan_ctx_t,
            node);

    return true;
}

bool sm_scan_schedule(
        sm_scan_request_t          *request)
{
    bool                            rc;

    sm_scan_ctx_t                  *scan_in_progress = NULL;
    sm_scan_ctx_t                  *scan_ctx = NULL;

    if (NULL == request) {
        return false;
    }

    /* Check if scan is in progress by looking at the processing list */
    scan_in_progress = ds_dlist_head(&g_scan_ctx_list);

    scan_ctx = sm_scan_ctx_alloc();
    if (NULL == scan_ctx) {
        return false;
    }
    scan_ctx->scan_request  = *request;

    g_scan_queue_qty++;
    ds_dlist_insert_tail(&g_scan_ctx_list, scan_ctx);


    if (NULL == scan_in_progress) {
        /* Trigger the scan and wait for results */
        rc =
            sm_scan_schedule_process(
                    scan_ctx);
        if (true != rc) {
            /* Remove processed context */
            ds_dlist_remove_head(&g_scan_ctx_list);
            sm_scan_ctx_free(scan_ctx);
            scan_ctx = NULL;
            return false;
        }
    }
    else {
        LOG(TRACE,
            "Scheduling %s %s %d scan to #%d (%s %s %d in progress)",
            radio_get_name_from_type(request->radio_cfg->type),
            radio_get_scan_name_from_type(request->scan_type),
            request->chan_list[0],
            g_scan_queue_qty,
            radio_get_name_from_type(scan_in_progress->scan_request.radio_cfg->type),
            radio_get_scan_name_from_type(scan_in_progress->scan_request.scan_type),
            scan_in_progress->scan_request.chan_list[0]);
    }

    return true;
}

bool sm_scan_schedule_stop (
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type)
{
    /* Stop scanning on target and flush pending scans to diff */
    target_stats_scan_stop(radio_cfg, scan_type);

    scan_schedule_timeout_timer_set(&g_scan_schedule_timer, false);

    return sm_scan_schedule_queue_flush();
}
