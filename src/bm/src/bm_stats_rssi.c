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

#include "bm.h"
#include "bm_stats.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

typedef struct
{
    bool                            initialized;

    /* Internal structure used to lower layer radio selection */
    radio_entry_t                  *radio_cfg;

    /* Internal structure to store report timers */
    ev_timer                        report_timer;

    /* Structure containing cloud request and sampling params */
    bm_stats_request_t              request;
    /* Structure pointing to upper layer rssi storage */
    dpp_rssi_report_data_t          report;

    /* Internal structure used to for rssi record fetching */
    ds_dlist_t                      record_list;

    /* Reporting start timestamp used for reporting timestamp calculation */
    uint64_t                        report_ts;

    ds_dlist_node_t                 node;
} bm_stats_rssi_ctx_t;

/* The stats entry has per band (type) phy_name and scan_type context */
static ds_dlist_t                   g_rssi_ctx_list;

static inline bm_stats_rssi_ctx_t * bm_stats_rssi_ctx_alloc()
{
    bm_stats_rssi_ctx_t *rssi_ctx = NULL;

    rssi_ctx = malloc(sizeof(bm_stats_rssi_ctx_t));
    if (rssi_ctx) {
        memset(rssi_ctx, 0, sizeof(bm_stats_rssi_ctx_t));
    }

    return rssi_ctx;
}

static inline void bm_stats_rssi_ctx_free(bm_stats_rssi_ctx_t *rssi_ctx)
{
    if (NULL != rssi_ctx) {
        free(rssi_ctx);
    }
}

static
bm_stats_rssi_ctx_t *bm_stats_rssi_ctx_get (
        radio_entry_t              *radio_cfg)
{
    bm_stats_rssi_ctx_t            *rssi_ctx = NULL;
    ds_dlist_iter_t                 ctx_iter;

    radio_entry_t                  *radio_entry = NULL;
    static bool                     init = false;

    if (!init) {
        /* Initialize report rssi list */
        ds_dlist_init(
                &g_rssi_ctx_list,
                bm_stats_rssi_ctx_t,
                node);
        init = true;
    }

    /* Find per radio rssi ctx  */
    for (   rssi_ctx = ds_dlist_ifirst(&ctx_iter,&g_rssi_ctx_list);
            rssi_ctx != NULL;
            rssi_ctx = ds_dlist_inext(&ctx_iter))
    {
        radio_entry = rssi_ctx->radio_cfg;

        /* The stats entry has per band (type) and phy_name context */
        if (radio_cfg->type == radio_entry->type)
        {
            LOGT("Fetched %s rssi reporting context",
                 radio_get_name_from_cfg(radio_entry));
            return rssi_ctx;
        }
    }

    /* No rssi ctx found create new ... */
    rssi_ctx = NULL;
    rssi_ctx = bm_stats_rssi_ctx_alloc();
    if(rssi_ctx) {
        rssi_ctx->radio_cfg = radio_cfg;
        ds_dlist_insert_tail(&g_rssi_ctx_list, rssi_ctx);
        LOGT("Created %s rssi reporting context",
             radio_get_name_from_cfg(radio_cfg));
    }

    return rssi_ctx;
}

/******************************************************************************
 * PROTECTED definitions
 *****************************************************************************/
static
bool bm_stats_rssi_report_timer_set(
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
bool bm_stats_rssi_report_timer_restart(
        ev_timer                   *timer)
{
    bm_stats_rssi_ctx_t            *rssi_ctx =
        (bm_stats_rssi_ctx_t *) timer->data;
    bm_stats_request_t             *request_ctx =
            &rssi_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        rssi_ctx->radio_cfg;

    if (request_ctx->reporting_count) {
        request_ctx->reporting_count--;

        LOG(DEBUG,
            "Updated %s rssi reporting count=%d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            request_ctx->reporting_count);

        /* If reporting_count becomes zero, then stop reporting */
        if (0 == request_ctx->reporting_count) {
            bm_stats_rssi_report_timer_set(timer, false);

            LOG(DEBUG,
                "Stopped %s rssi reporting (count expired)",
                radio_get_name_from_cfg(radio_cfg_ctx));
            return true;
        }
    }

    return true;
}

static
bool bm_stats_rssi_raw_clear(
        bm_stats_rssi_ctx_t        *rssi_ctx,
        ds_dlist_t                 *rssi_list)
{
    dpp_rssi_raw_t                 *rssi = NULL;
    ds_dlist_iter_t                 rssi_iter;

    for (   rssi = ds_dlist_ifirst(&rssi_iter, rssi_list);
            rssi != NULL;
            rssi = ds_dlist_inext(&rssi_iter))
    {
        ds_dlist_iremove(&rssi_iter);
        free(rssi);
        rssi = NULL;
    }

    return true;
}

static
bool bm_stats_rssi_records_clear(
        bm_stats_rssi_ctx_t        *rssi_ctx,
        ds_dlist_t                 *record_list)
{
    dpp_rssi_record_t              *record_entry = NULL;
    ds_dlist_iter_t                 record_iter;

    for (   record_entry = ds_dlist_ifirst(&record_iter, record_list);
            record_entry != NULL;
            record_entry = ds_dlist_inext(&record_iter))
    {
        bm_stats_rssi_raw_clear(rssi_ctx, &record_entry->rssi.raw);

        ds_dlist_iremove(&record_iter);
        dpp_rssi_record_free(record_entry);
        record_entry = NULL;
    }

    return true;
}

static
bool bm_stats_rssi_result_clear(
        bm_stats_rssi_ctx_t        *rssi_ctx,
        ds_dlist_t                 *record_list)
{
    bm_stats_request_t             *request_ctx =
        &rssi_ctx->request;

    dpp_rssi_record_t              *record_entry = NULL;
    ds_dlist_iter_t                 record_iter;

    for (   record_entry = ds_dlist_ifirst(&record_iter, record_list);
            record_entry != NULL;
            record_entry = ds_dlist_inext(&record_iter))
    {
        if (REPORT_TYPE_RAW == request_ctx->report_type) {
            bm_stats_rssi_raw_clear(rssi_ctx, &record_entry->rssi.raw);
        }

        ds_dlist_iremove(&record_iter);
        dpp_rssi_record_free(record_entry);
        record_entry = NULL;
    }

    return true;
}

static
dpp_rssi_record_t *bm_stats_rssi_records_mac_get(
        bm_stats_rssi_ctx_t        *rssi_ctx,
        mac_address_t               mac,
        rssi_source_t               source)
{
    ds_dlist_t                     *record_list =
        &rssi_ctx->record_list;
    dpp_rssi_record_t              *record_entry = NULL;
    ds_dlist_iter_t                 record_iter;

    /* Find current rssi in existing list */
    for (   record_entry = ds_dlist_ifirst(&record_iter, record_list);
            record_entry != NULL;
            record_entry = ds_dlist_inext(&record_iter))
    {
        if (!memcmp(
                    record_entry->mac,
                    mac,
                    sizeof(record_entry->mac))
           ) {
            return record_entry;
        }
    }

    record_entry = NULL;
    record_entry = dpp_rssi_record_alloc();
    if (NULL != record_entry) {
        memcpy(record_entry->mac, mac, sizeof(mac_address_t));
        record_entry->source = source;

        /* Cache is always RAW */
        ds_dlist_init(
                &record_entry->rssi.raw,
                dpp_rssi_raw_t,
                node);

        ds_dlist_insert_tail(record_list, record_entry);
    }

    return record_entry;
}

static
bool bm_stats_rssi_report_calculate_raw(
        bm_stats_rssi_ctx_t        *rssi_ctx,
        dpp_rssi_record_t          *record_entry,
        dpp_rssi_record_t          *report_entry)
{
    radio_entry_t                  *radio_cfg_ctx =
        rssi_ctx->radio_cfg;
    bm_stats_request_t             *request_ctx =
        &rssi_ctx->request;
    dpp_rssi_raw_t                 *rssi_entry;
    ds_dlist_iter_t                 rssi_iter;
    dpp_rssi_raw_t                 *rssi;

    ds_dlist_init(
            &report_entry->rssi.raw,
            dpp_rssi_raw_t,
            node);

    /* Loop through each cached record and prepare RAW report */
    for (   rssi_entry = ds_dlist_ifirst(&rssi_iter, &record_entry->rssi.raw);
            rssi_entry != NULL;
            rssi_entry = ds_dlist_inext(&rssi_iter))
    {
        if (!(rssi = calloc(1, sizeof(*rssi)))) {
            LOGEM("Failed to allocate memory for new rssi data");
            return false;
        }

        rssi->rssi = rssi_entry->rssi;

        rssi->timestamp_ms =
            request_ctx->reporting_timestamp - rssi_ctx->report_ts +
            rssi_entry->timestamp_ms;

        LOGT("Sending %s raw rssi %d for "MAC_ADDRESS_FORMAT,
             radio_get_name_from_cfg(radio_cfg_ctx),
             rssi->rssi,
             MAC_ADDRESS_PRINT(record_entry->mac));

        ds_dlist_insert_tail(&report_entry->rssi.raw, rssi);
    }

    return true;
}

static
bool bm_stats_rssi_report_calculate_average (
        bm_stats_rssi_ctx_t        *rssi_ctx,
        dpp_rssi_record_t          *record_entry,
        dpp_rssi_record_t          *report_entry)
{
    radio_entry_t                  *radio_cfg_ctx =
        rssi_ctx->radio_cfg;
    dpp_rssi_raw_t                 *rssi_entry;
    ds_dlist_iter_t                 rssi_iter;
    dpp_avg_t                      *rssi = &report_entry->rssi.avg;

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

    /* Loop through each cached record and prepare RAW report */
    rssi_entry = ds_dlist_ifirst(&rssi_iter, &record_entry->rssi.raw);
    for (   rssi_entry = ds_dlist_ifirst(&rssi_iter, &record_entry->rssi.raw);
            rssi_entry != NULL;
            rssi_entry = ds_dlist_inext(&rssi_iter))
    {
        /* Sum all and derive average later */
        rssi->avg += rssi_entry->rssi;

        if(rssi->num) {
            rssi->min = MIN(rssi->min, rssi_entry->rssi);
            rssi->max = MAX(rssi->max, rssi_entry->rssi);
        } else {
            rssi->min = rssi_entry->rssi;
            rssi->max = rssi_entry->rssi;
        }
        rssi->num++;
    }

    /* Calculate average from sum */
    rssi->avg = rssi->avg / rssi->num;

    LOGT("Sending %s average rssi %d (min %d, max %d, num %d) for "MAC_ADDRESS_FORMAT,
         radio_get_name_from_cfg(radio_cfg_ctx),
         rssi->avg,
         rssi->min,
         rssi->max,
         rssi->num,
         MAC_ADDRESS_PRINT(record_entry->mac));

#undef MIN
#undef MAX

    return true;
}

static
bool bm_stats_rssi_report_send(
        bm_stats_rssi_ctx_t        *rssi_ctx)
{
    bool                            status;
    dpp_rssi_report_data_t         *report_ctx =
        &rssi_ctx->report;
    bm_stats_request_t             *request_ctx =
        &rssi_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        rssi_ctx->radio_cfg;
    ev_timer                       *report_timer =
        &rssi_ctx->report_timer;
    ds_dlist_t                     *record_list =
        &rssi_ctx->record_list;

    /* Restart timer */
    bm_stats_rssi_report_timer_restart(report_timer);

    report_ctx->radio_type  = radio_cfg_ctx->type;
    report_ctx->report_type = request_ctx->report_type;

    /* Report_timestamp is cloud_timestamp + relative start time offset */
    report_ctx->timestamp_ms =
        request_ctx->reporting_timestamp - rssi_ctx->report_ts +
        get_timestamp();

    ds_dlist_iter_t                 record_iter;
    dpp_rssi_record_t              *record_entry = NULL;
    dpp_rssi_record_t              *report_entry = NULL;

    /* Loop through each record and prepare report */
    for (   record_entry = ds_dlist_ifirst(&record_iter, record_list);
            record_entry != NULL;
            record_entry = ds_dlist_inext(&record_iter))
    {
        if (record_entry) {
            /* Allocate report rssi entry */
            report_entry = dpp_rssi_record_alloc();
            if (NULL == report_entry) {
                LOGE("Sending %s rssi report"
                     "(Failed to allocate memory)",
                     radio_get_name_from_cfg(radio_cfg_ctx));
                return false;
            }

            memcpy(report_entry->mac, record_entry->mac, sizeof(mac_address_t));
            report_entry->source = record_entry->source;

            /* Derive values based on request from cloud */
            switch(request_ctx->report_type)
            {
                case REPORT_TYPE_NONE:
                    report_ctx->report_type = REPORT_TYPE_RAW;
                    /* fall through */
                case REPORT_TYPE_RAW:
                    bm_stats_rssi_report_calculate_raw(
                            rssi_ctx,
                            record_entry,
                            report_entry);
                    break;
                case REPORT_TYPE_AVERAGE:
                    bm_stats_rssi_report_calculate_average(
                            rssi_ctx,
                            record_entry,
                            report_entry);
                    break;
                /* Not yet implemented */
                case REPORT_TYPE_HISTOGRAM:
                case REPORT_TYPE_PERCENTILE:
                default:
                    LOGE("Sending %s rssi for "MAC_ADDRESS_FORMAT
                         " (Invalid report type)",
                         radio_get_name_from_cfg(radio_cfg_ctx),
                         MAC_ADDRESS_PRINT(record_entry->mac));
                    goto exit;
            }

            ds_dlist_insert_tail(&report_ctx->list, report_entry);
        }
    }

    LOGI("Sending %s rssi report at '%s'",
         radio_get_name_from_cfg(radio_cfg_ctx),
         bm_timestamp_ms_to_date(report_ctx->timestamp_ms));

    /* Send results to MQTT FIFO (Skip empty reports) */
    if (!ds_dlist_is_empty(record_list)) {
        dpp_put_rssi(report_ctx);
    }

exit:
    status =
        bm_stats_rssi_result_clear(
                rssi_ctx,
                &report_ctx->list);
    if (true != status) {
        return false;
    }

    return true;
}

static
void bm_stats_rssi_report (EV_P_ ev_timer *w, int revents)
{
    bool                            status;

    bm_stats_rssi_ctx_t            *rssi_ctx =
        (bm_stats_rssi_ctx_t *) w->data;
    radio_entry_t                  *radio_cfg_ctx =
        rssi_ctx->radio_cfg;
    ds_dlist_t                     *record_list =
        &rssi_ctx->record_list;

    /* Send report */
    bm_stats_rssi_report_send(rssi_ctx);

    /* Reset the internal cache stats */
     status =
        bm_stats_rssi_records_clear(
                rssi_ctx,
                record_list);
    if (true != status) {
        LOGE("Processing %s rssi report "
             "(failed to reset rssi list)",
             radio_get_name_from_cfg(radio_cfg_ctx));
        return;
    }
}

static
bool bm_stats_rssi_stats_init (
        bm_stats_rssi_ctx_t        *rssi_ctx)
{
    bool                            status;

    dpp_rssi_report_data_t         *report_ctx =
        &rssi_ctx->report;
    ds_dlist_t                     *record_list =
        &rssi_ctx->record_list;

    status =
        bm_stats_rssi_result_clear(
                rssi_ctx,
                &report_ctx->list);
    if (true != status) {
        return false;
    }

     status =
        bm_stats_rssi_records_clear(
                rssi_ctx,
                record_list);
    if (true != status) {
        return false;
    }

    return true;
}

static
bool bm_stats_rssi_stats_process (
        radio_entry_t              *radio_cfg, // For logging
        bm_stats_rssi_ctx_t        *rssi_ctx)
{
    bool                            status;

    bm_stats_request_t             *request_ctx =
        &rssi_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        rssi_ctx->radio_cfg;
    ev_timer                       *report_timer =
        &rssi_ctx->report_timer;

    /* Skip on-chan rssi report start if radio is not configured */
    if (!radio_cfg_ctx) {
        LOGT("Skip starting %s rssi reporting "
             "(Radio not configured)",
             radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    /* Skip on-chan rssi report start if stats are not configured */
    if (!rssi_ctx->initialized) {
        LOGT("Skip starting %s rssi reporting "
             "(Stats not configured)",
             radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    bm_stats_rssi_report_timer_set(report_timer, false);

    /* Skip on-chan rssi report start if timestamp is not specified */
    if (!request_ctx->reporting_timestamp) {
        LOGT("Skip starting %s rssi reporting "
             "(Timestamp not configured)",
             radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    if (request_ctx->reporting_interval) {
        status =
            bm_stats_rssi_stats_init(
                    rssi_ctx);
        if (true != status) {
            return false;
        }

        report_timer->repeat = request_ctx->reporting_interval;
        bm_stats_rssi_report_timer_set(report_timer, true);
        rssi_ctx->report_ts = get_timestamp();

        LOGI("Started %s rssi reporting",
             radio_get_name_from_cfg(radio_cfg));
    }
    else {
        LOGI("Stopped %s rssi reporting",
             radio_get_name_from_cfg(radio_cfg));

        memset(request_ctx, 0, sizeof(bm_stats_request_t));
    }

    return true;
}

/******************************************************************************
 * PUBLIC API definitions
 *****************************************************************************/
bool bm_stats_rssi_report_request(
        radio_entry_t              *radio_cfg,
        bm_stats_request_t         *request)
{
    bool                            status;

    bm_stats_rssi_ctx_t            *rssi_ctx = NULL;
    bm_stats_request_t             *request_ctx = NULL;
    dpp_rssi_report_data_t         *report_ctx = NULL;
    ev_timer                       *report_timer = NULL;

    if (NULL == request) {
        LOGE("Initializing rssi reporting "
             "(Invalid request config)");
        return false;
    }

    rssi_ctx        = bm_stats_rssi_ctx_get(radio_cfg);
    request_ctx     = &rssi_ctx->request;
    report_ctx      = &rssi_ctx->report;
    report_timer    = &rssi_ctx->report_timer;

    /* Initialize global storage and timer config only once */
    if (!rssi_ctx->initialized) {
        memset(request_ctx, 0, sizeof(*request_ctx));
        memset(report_ctx, 0, sizeof(*report_ctx));

        LOGI("Initializing %s rssi reporting",
             radio_get_name_from_cfg(radio_cfg));

        /* Initialize report rssi list */
        ds_dlist_init(
                &report_ctx->list,
                dpp_rssi_record_t,
                node);

        /* Initialize rssi list */
        ds_dlist_init(
                &rssi_ctx->record_list,
                dpp_rssi_record_t,
                node);

        /* Initialize event lib timers and pass the global
           internal cache
         */
        ev_init (report_timer, bm_stats_rssi_report);
        report_timer->data = rssi_ctx;

        rssi_ctx->initialized = true;
    }

    REQUEST_PARAM_UPDATE("rssi", radio_type, "%d");
    REQUEST_PARAM_UPDATE("rssi", report_type, "%d");
    REQUEST_PARAM_UPDATE("rssi", reporting_count, "%d");
    REQUEST_PARAM_UPDATE("rssi", reporting_interval, "%d");
    REQUEST_PARAM_UPDATE("rssi", sampling_interval, "%d");
    REQUEST_PARAM_UPDATE("rssi", reporting_timestamp, "%"PRIu64"");
    strcpy(request_ctx->radio_cfg.phy_name, request->radio_cfg.phy_name);

    status =
        bm_stats_rssi_stats_process (
                radio_cfg,
                rssi_ctx);
    if (true != status) {
        return false;
    }

    return true;
}

bool bm_stats_rssi_report_radio_change(
        radio_entry_t              *radio_cfg)
{
    bool                            status;
    bm_stats_rssi_ctx_t            *rssi_ctx = NULL;

    if (NULL == radio_cfg) {
        LOGE("Initializing rssi reporting "
             "(Invalid radio config)");
        return false;
    }

    rssi_ctx = bm_stats_rssi_ctx_get(radio_cfg);

    status =
        bm_stats_rssi_stats_process (
                radio_cfg,
                rssi_ctx);
    if (true != status) {
        return false;
    }

    return true;
}

bool bm_stats_rssi_is_reporting_enabled (
        radio_entry_t              *radio_cfg)
{
    bm_stats_rssi_ctx_t            *rssi_ctx = NULL;
    ds_dlist_iter_t                 ctx_iter;

    /* Find per radio rssi ctx  */
    for (   rssi_ctx = ds_dlist_ifirst(&ctx_iter,&g_rssi_ctx_list);
            rssi_ctx != NULL;
            rssi_ctx = ds_dlist_inext(&ctx_iter))
    {
        if (radio_cfg->type == rssi_ctx->radio_cfg->type)
        {
            return !!rssi_ctx->request.reporting_interval;
        }
    }

    /* False if not found */
    return false;
}

bool bm_stats_rssi_stats_results_update(
        radio_entry_t              *radio_cfg,
        mac_address_t               mac,
        uint32_t                    rssi,
        rssi_source_t               source)
{
    bm_stats_rssi_ctx_t            *rssi_ctx = NULL;
    dpp_rssi_record_t              *record_entry = NULL;
    dpp_rssi_raw_t                 *rssi_entry;

    if (NULL == radio_cfg) {
        LOGE("Updating rssi reporting "
             "(Invalid radio config)");
        return false;
    }

    if(!bm_stats_rssi_is_reporting_enabled(radio_cfg)) {
        LOGT("Skip updating %s rssi %d "MAC_ADDRESS_FORMAT
             "(reporting not configured)",
             radio_get_name_from_type(radio_cfg->type),
             rssi,
             MAC_ADDRESS_PRINT(mac));
        return true;
    }

    rssi_ctx = bm_stats_rssi_ctx_get(radio_cfg),

    record_entry =
        bm_stats_rssi_records_mac_get(
                rssi_ctx,
                mac,
                source);
    if (NULL == record_entry) {
        LOGE("Updating %s rssi %d for "MAC_ADDRESS_FORMAT,
             radio_get_name_from_cfg(rssi_ctx->radio_cfg),
             rssi,
             MAC_ADDRESS_PRINT(mac));
        return false;
    }

    /* Add RSSI to the cache for later processing */
    if (!(rssi_entry = calloc(1, sizeof(*rssi_entry)))) {
        LOGEM("Updating %s rssi %d for "MAC_ADDRESS_FORMAT
              " (Failed to allocate memory for new rssi data)",
             radio_get_name_from_cfg(rssi_ctx->radio_cfg),
             rssi,
             MAC_ADDRESS_PRINT(mac));
        return false;
    }

    rssi_entry->rssi = rssi;
    rssi_entry->timestamp_ms = get_timestamp();

    LOGT("Updating %s rssi %d for "MAC_ADDRESS_FORMAT,
         radio_get_name_from_cfg(rssi_ctx->radio_cfg),
         rssi_entry->rssi,
         MAC_ADDRESS_PRINT(record_entry->mac));

    /* Cache is always RAW */
    ds_dlist_insert_tail(&record_entry->rssi.raw, rssi_entry);

    return true;
}
