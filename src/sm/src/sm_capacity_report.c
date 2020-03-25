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

typedef struct
{
    bool                            initialized;

    /* Internal structure used to lower layer radio selection */
    radio_entry_t                  *radio_cfg;

    /* Internal structure to store report timers */
    ev_timer                        update_timer;
    ev_timer                        report_timer;

    /* Structure containing cloud request and sampling params */
    sm_stats_request_t              request;
    /* Structure pointing to upper layer capacity storage */
    dpp_capacity_report_data_t      report;

    /* Internal structure used to for delta calculation */
    target_capacity_data_t          results;
    uint32_t                        capacity_qty;

    /* Reporting start timestamp used for reporting timestamp calculation */
    uint64_t                        report_ts;

    ds_dlist_node_t                 node;
} sm_capacity_ctx_t;

/* The stats entry has per band (type) and phy_name context */
static ds_dlist_t                   g_capacity_ctx_list;

static inline sm_capacity_ctx_t * sm_capacity_ctx_alloc()
{
    sm_capacity_ctx_t *capacity_ctx = NULL;

    capacity_ctx = malloc(sizeof(sm_capacity_ctx_t));
    if (capacity_ctx)
    {
        memset(capacity_ctx, 0, sizeof(sm_capacity_ctx_t));
    }

    return capacity_ctx;
}

static inline void sm_capacity_ctx_free(sm_capacity_ctx_t *capacity_ctx)
{
    if (NULL != capacity_ctx) {
        free(capacity_ctx);
    }
}

static
sm_capacity_ctx_t *sm_capacity_ctx_get (
        radio_entry_t              *radio_cfg)
{
    sm_capacity_ctx_t                *capacity_ctx = NULL;
    ds_dlist_iter_t                 ctx_iter;

    radio_entry_t                  *radio_entry = NULL;
    static bool                     init = false;

    if (!init) {
        /* Initialize report capacity list */
        ds_dlist_init(
                &g_capacity_ctx_list,
                sm_capacity_ctx_t,
                node);
        init = true;
    }

    /* Find per radio capacity ctx  */
    for (   capacity_ctx = ds_dlist_ifirst(&ctx_iter,&g_capacity_ctx_list);
            capacity_ctx != NULL;
            capacity_ctx = ds_dlist_inext(&ctx_iter))
    {
        radio_entry = capacity_ctx->radio_cfg;

        /* The stats entry has per band (type) and phy_name context */
        if (radio_cfg->type == radio_entry->type)
        {
            LOG(TRACE,
                "Fetched %s capacity reporting context",
                radio_get_name_from_cfg(radio_entry));
            return capacity_ctx;
        }
    }

    /* No capacity ctx found create new ... */
    capacity_ctx = NULL;
    capacity_ctx = sm_capacity_ctx_alloc();
    if(capacity_ctx) {
        capacity_ctx->radio_cfg = radio_cfg;
        ds_dlist_insert_tail(&g_capacity_ctx_list, capacity_ctx);
        LOG(TRACE,
            "Created %s capacity reporting context",
            radio_get_name_from_cfg(radio_cfg));
    }

    return capacity_ctx;
}

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static
bool sm_capacity_update_timer_set(
        ev_timer                   *timer,
        bool                        enable)
{
    if (enable) {
        ev_timer_again(EV_DEFAULT, timer);
    }
    else {
        ev_timer_stop(EV_DEFAULT, timer);
    }

    return true;
}

static
bool sm_capacity_report_timer_set(
        ev_timer                   *timer,
        bool                        enable)
{
    if (enable) {
        ev_timer_again(EV_DEFAULT, timer);
    }
    else {
        ev_timer_stop(EV_DEFAULT, timer);
    }

    return true;
}

static
bool sm_capacity_report_timer_restart(
        ev_timer                   *timer)
{
    sm_capacity_ctx_t              *capacity_ctx =
        (sm_capacity_ctx_t *) timer->data;
    sm_stats_request_t             *request_ctx =
        &capacity_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        capacity_ctx->radio_cfg;

    if (request_ctx->reporting_count)
    {
        request_ctx->reporting_count--;

        LOG(DEBUG,
            "Updated %s capacity reporting count=%d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            request_ctx->reporting_count);

        /* If reporting_count becomes zero, then stop reporting */
        if (0 == request_ctx->reporting_count)
        {
            sm_capacity_report_timer_set(timer, false);
            sm_capacity_update_timer_set(
                    &capacity_ctx->update_timer, false);

            LOG(DEBUG,
                "Stopped %s capacity reporting (count expired)",
                radio_get_name_from_cfg(radio_cfg_ctx));
            return true;
        }
    }

    return true;
}

static
bool sm_capacity_records_clear(
        sm_capacity_ctx_t          *capacity_ctx,
        dpp_capacity_list_t        *capacity_list)
{
    dpp_capacity_record_list_t     *capacity = NULL;
    ds_dlist_iter_t                 capacity_iter;

    for (   capacity = ds_dlist_ifirst(&capacity_iter, capacity_list);
            capacity != NULL;
            capacity = ds_dlist_inext(&capacity_iter))
    {
        ds_dlist_iremove(&capacity_iter);
        dpp_capacity_record_free(capacity);
        capacity = NULL;
    }

    capacity_ctx->capacity_qty = 0;

    return true;
}

static
bool sm_capacity_report_send(
        sm_capacity_ctx_t          *capacity_ctx)
{
    bool                            status;
    dpp_capacity_report_data_t     *report_ctx =
        &capacity_ctx->report;
    dpp_capacity_list_t            *capacity_list =
        &report_ctx->list;
    sm_stats_request_t             *request_ctx =
        &capacity_ctx->request;
    ev_timer                       *report_timer =
        &capacity_ctx->report_timer;
    radio_entry_t                  *radio_cfg_ctx =
        capacity_ctx->radio_cfg;

    /* Restart timer */
    sm_capacity_report_timer_restart(report_timer);

    report_ctx->radio_type = radio_cfg_ctx->type;

    /* Report_timestamp is base-timestamp + relative start time offset */
    report_ctx->timestamp_ms =
        request_ctx->reporting_timestamp - capacity_ctx->report_ts +
        get_timestamp();

    dpp_capacity_record_list_t      *capacity = NULL;
    ds_dlist_iter_t                  capacity_iter;
    dpp_capacity_record_t           *result_entry = NULL;

    capacity_ctx->capacity_qty++;
    for (   capacity = ds_dlist_ifirst(&capacity_iter, capacity_list);
            capacity != NULL;
            capacity = ds_dlist_inext(&capacity_iter))
    {
        result_entry = &capacity->entry;

        if (result_entry) {
            LOG(DEBUG,
                "Sending %s capacity stats "
                "{busy=%u bytes=%llu samples=%u (bk=%d be=%d vi=%d vo=%d cab=%d bcn=%d)}",
                radio_get_name_from_cfg(radio_cfg_ctx),
                result_entry->busy_tx,
                result_entry->bytes_tx,
                result_entry->samples,
                result_entry->queue[RADIO_QUEUE_TYPE_BK],
                result_entry->queue[RADIO_QUEUE_TYPE_BE],
                result_entry->queue[RADIO_QUEUE_TYPE_VI],
                result_entry->queue[RADIO_QUEUE_TYPE_VO],
                result_entry->queue[RADIO_QUEUE_TYPE_CAB],
                result_entry->queue[RADIO_QUEUE_TYPE_BCN]);
        }
    }

    LOG(INFO,
        "Sending %s capacity report at '%s'",
        radio_get_name_from_cfg(radio_cfg_ctx),
        sm_timestamp_ms_to_date(report_ctx->timestamp_ms));

    /* Send results to MQTT FIFO (Skip empty reports) */
    if(!ds_dlist_is_empty(capacity_list)) {
        dpp_put_capacity(report_ctx);
    }

    status =
        sm_capacity_records_clear(
                capacity_ctx,
                capacity_list);
    if (true != status)
    {
        return false;
    }


    SM_SANITY_CHECK_TIME(report_ctx->timestamp_ms,
                         &request_ctx->reporting_timestamp,
                         &capacity_ctx->report_ts);

    return true;
}

static
bool sm_capacity_stats_update (
        sm_capacity_ctx_t          *capacity_ctx)
{
    bool                            rc;

    sm_stats_request_t             *request_ctx =
        &capacity_ctx->request;
    target_capacity_data_t         *results_ctx =
        &capacity_ctx->results;
    radio_entry_t                  *radio_cfg_ctx =
        capacity_ctx->radio_cfg;

    target_capacity_data_t          results;

    /* Check if radio is exists */
    if (target_is_radio_interface_ready(radio_cfg_ctx->phy_name) != true)
    {
        LOG(DEBUG,
            "Skip processing %s capacity report "
            "(Radio %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_cfg_ctx->phy_name);
        return true;
    }

    memset(&results, 0, sizeof (results));
    rc =
        target_stats_capacity_get (
                radio_cfg_ctx,
                &results);
    if (true != rc)
    {
        LOG(ERR,
            "Processing %s capacity sample %d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            capacity_ctx->capacity_qty);
        return false;
    }

    dpp_capacity_report_data_t     *report_ctx =
        &capacity_ctx->report;
    dpp_capacity_list_t            *capacity_list =
        &report_ctx->list;
    dpp_capacity_record_list_t     *capacity = NULL;
    dpp_capacity_record_t          *capacity_entry = NULL;

    capacity =
        dpp_capacity_record_alloc();
    if (NULL == capacity)
    {
        LOG(ERR,
            "Processing %s capacity report "
            "(Failed to allocate memmory)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }
    capacity_entry = &capacity->entry;

    rc =
        target_stats_capacity_convert(
                &results,
                results_ctx,
                capacity_entry);
    if (true != rc)
    {
        LOG(ERR,
            "Processing %s capacity sample %d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            capacity_ctx->capacity_qty);
        return false;
    }

    LOG(DEBUG,
        "Processed %s capacity sample %d "
        "{busy=%u bytes=%llu samples=%u (bk=%u be=%u vi=%u vo=%u cab=%u bcn=%u)}",
        radio_get_name_from_cfg(radio_cfg_ctx),
        capacity_ctx->capacity_qty,
        capacity_entry->busy_tx,
        capacity_entry->bytes_tx,
        capacity_entry->samples,
        capacity_entry->queue[RADIO_QUEUE_TYPE_BK],
        capacity_entry->queue[RADIO_QUEUE_TYPE_BE],
        capacity_entry->queue[RADIO_QUEUE_TYPE_VI],
        capacity_entry->queue[RADIO_QUEUE_TYPE_VO],
        capacity_entry->queue[RADIO_QUEUE_TYPE_CAB],
        capacity_entry->queue[RADIO_QUEUE_TYPE_BCN]);

    capacity_entry->timestamp_ms =
        request_ctx->reporting_timestamp - capacity_ctx->report_ts +
        get_timestamp();

    ds_dlist_insert_tail(capacity_list, capacity);

    /* Store result to cache ... */
    *results_ctx = results;
    capacity_ctx->capacity_qty++;

    return true;
}

static
void sm_capacity_update (EV_P_ ev_timer *w, int revents)
{
    bool                            status;

    sm_capacity_ctx_t              *capacity_ctx =
        (sm_capacity_ctx_t *) w->data;

    status =
        sm_capacity_stats_update(
                capacity_ctx);
    if (true != status)
    {
        /* If error is reported due to any kind of reason
           we should not exit the processing therefore we
           skip this sample and send the report
         */
    }
}

static
void sm_capacity_report (EV_P_ ev_timer *w, int revents)
{
    sm_capacity_ctx_t                 *capacity_ctx =
        (sm_capacity_ctx_t *) w->data;

    sm_capacity_report_send(capacity_ctx);
}

static
bool sm_capacity_stats_init (
        sm_capacity_ctx_t          *capacity_ctx)
{
    bool                           rc;
    bool                           status;

    dpp_capacity_report_data_t    *report_ctx =
        &capacity_ctx->report;
    dpp_capacity_list_t           *capacity_list =
        &report_ctx->list;
    radio_entry_t                 *radio_cfg_ctx =
        capacity_ctx->radio_cfg;
    target_capacity_data_t        *results_ctx =
        &capacity_ctx->results;

    /* Reset the stats result cache and proceed */
    status =
        sm_capacity_records_clear(
                capacity_ctx,
                capacity_list);
    if (true != status) {
        return false;
    }

    /* Check if radio is exists */
    if (target_is_radio_interface_ready(radio_cfg_ctx->phy_name) != true) {
        LOG(ERR,
            "Initializing %s capacity report "
            "(Radio %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_cfg_ctx->phy_name);
        return false;
    }

    /* Clear cache */
    memset(results_ctx, 0, sizeof (*results_ctx));
    rc =
        target_stats_capacity_get (
                radio_cfg_ctx,
                results_ctx);
    if (true != rc) {
        LOG(ERR,
            "Processing %s capacity sample %d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            capacity_ctx->capacity_qty);
        return false;
    }

    return true;
}

static
bool sm_capacity_stats_process (
        radio_entry_t              *radio_cfg, // For logging
        sm_capacity_ctx_t          *capacity_ctx)
{
    bool                            status;

    sm_stats_request_t             *request_ctx =
        &capacity_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        capacity_ctx->radio_cfg;
    ev_timer                       *update_timer =
        &capacity_ctx->update_timer;
    ev_timer                       *report_timer =
        &capacity_ctx->report_timer;

    /* Skip capacity report start if radio is not configured */
    if (!radio_cfg_ctx) {
        LOG(TRACE,
            "Skip starting %s capacity reporting "
            "(Radio not configured)",
            radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    /* Skip capacity report start if stats are not configured */
    if (!capacity_ctx->initialized) {
        LOG(TRACE,
            "Skip starting %s capacity reporting "
            "(Stats not configured)",
            radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    /* Restart timers with new parameters */
    sm_capacity_update_timer_set(update_timer, false);
    sm_capacity_report_timer_set(report_timer, false);

    /* Skip capacity report start if timestamp is not specified */
    if (!request_ctx->reporting_timestamp) {
        LOG(TRACE,
            "Skip starting %s capacity reporting "
            "(Timestamp not configured)",
            radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    if (request_ctx->reporting_interval) {
        /* Enable stats processing n driver */
        target_stats_capacity_enable(radio_cfg_ctx, true);

        status =
            sm_capacity_stats_init(
                    capacity_ctx);
        if (true != status) {
            return false;
        }

        /* Sampling vs one big averaged report ... */
        if (request_ctx->sampling_interval) {
            update_timer->repeat = request_ctx->sampling_interval;
            sm_capacity_update_timer_set(update_timer, true);
        }

        report_timer->repeat = request_ctx->reporting_interval;
        sm_capacity_report_timer_set(report_timer, true);
        capacity_ctx->report_ts = get_timestamp();

        LOG(INFO,
            "Started %s capacity reporting",
            radio_get_name_from_cfg(radio_cfg));
    }
    else {
        /* Disable stats processing in driver */
        target_stats_capacity_enable(radio_cfg_ctx, false);

        LOG(INFO,
            "Stopped %s capacity reporting",
            radio_get_name_from_cfg(radio_cfg));

        memset(request_ctx, 0, sizeof(sm_stats_request_t    ));
    }

    return true;
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
bool sm_capacity_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request)
{
    bool                            status;
    sm_capacity_ctx_t              *capacity_ctx = NULL;
    sm_stats_request_t             *request_ctx = NULL;
    dpp_capacity_report_data_t     *report_ctx = NULL;
    ev_timer                       *update_timer = NULL;
    ev_timer                       *report_timer = NULL;

    if (NULL == request) {
        LOG(ERR,
            "Initializing survey reporting "
            "(Invalid request config)");
        return false;
    }

    capacity_ctx    = sm_capacity_ctx_get(radio_cfg);
    request_ctx     = &capacity_ctx->request;
    report_ctx      = &capacity_ctx->report;
    update_timer    = &capacity_ctx->update_timer;
    report_timer    = &capacity_ctx->report_timer;

    /* Initialize global stats only once */
    if (!capacity_ctx->initialized) {
        memset(request_ctx, 0, sizeof(*request_ctx));
        memset(report_ctx, 0, sizeof(*report_ctx));

        LOG(INFO,
            "Initializing %s capacity reporting",
            radio_get_name_from_cfg(radio_cfg));

        /* Initialize  capacity list */
        ds_dlist_init(
                &report_ctx->list,
                dpp_capacity_record_list_t,
                node);

        /* Initialize event lib timers and pass the global
           internal cache
         */
        ev_init (update_timer, sm_capacity_update);
        update_timer->data = capacity_ctx;
        ev_init (report_timer, sm_capacity_report);
        report_timer->data = capacity_ctx;

        capacity_ctx->initialized = true;
    }

    /* Store and compare every request parameter ...
       memcpy would be easier but we want some debug info
     */
    REQUEST_PARAM_UPDATE("capacity", radio_type, "%d");
    REQUEST_PARAM_UPDATE("capacity", reporting_count, "%d");
    REQUEST_PARAM_UPDATE("capacity", reporting_interval, "%d");
    REQUEST_PARAM_UPDATE("capacity", sampling_interval, "%d");
    REQUEST_PARAM_UPDATE("capacity", reporting_timestamp, "%lld");

    status =
        sm_capacity_stats_process (
                radio_cfg,
                capacity_ctx);
    if (true != status) {
        return false;
    }

    return true;
}

bool sm_capacity_report_radio_change(
        radio_entry_t              *radio_cfg)
{
    bool                            status;
    sm_capacity_ctx_t              *capacity_ctx = NULL;

    if (NULL == radio_cfg) {
        LOG(ERR,
            "Initializing client reporting "
            "(Invalid radio config)");
        return false;
    }

    capacity_ctx = sm_capacity_ctx_get(radio_cfg);

    status =
        sm_capacity_stats_process (
                radio_cfg,
                capacity_ctx);
    if (true != status) {
        return false;
    }

    return true;
}
