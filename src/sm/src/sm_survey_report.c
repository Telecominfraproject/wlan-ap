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

#include "os.h"
#include "os_time.h"

#include "sm.h"

/******************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN

typedef struct
{
    bool                            initialized;

    /* Internal structure used to lower layer radio selection */
    radio_entry_t                  *radio_cfg;
    radio_scan_type_t               scan_type;

    /* Internal channels that shall be processed */
    sm_chan_list_t                  chan_list;

    /* Internal structure to store report timers */
    ev_timer                        report_timer;
    ev_timer                        update_timer;
    ev_timer                        init_timer;

    /* Structure containing cloud request report and sampling params */
    sm_stats_request_t              request;
    /* Structure pointing to upper layer survey storage */
    dpp_survey_report_data_t        report;

    /* Structure containing cached survey sampling records
       (dpp_survey_record_t) */
    ds_dlist_t                      record_list;
    uint32_t                        record_qty;

    /* target client temporary list for deriving records */
    ds_dlist_t                      survey_list;

    /* Internal structure used to for delta calculation
       (multiple channel storage ... ) */
    target_survey_record_t          records[RADIO_MAX_CHANNELS];

    /* Threshold counters */
    time_t                          threshold_ts; // timestamp
    int                             threshold_count; // count chan_num
    target_survey_record_t          threshold_record;  // replace with capacity!
    time_t                          threshold_time_delta;

    /* Reporting start timestamp used for reporting timestamp calculation */
    uint64_t                        report_ts;

    /* Synchronization delay */
    uint64_t                        sync_delay;

    ds_dlist_node_t                 node;
} sm_survey_ctx_t;

#define SURVEY_MIN_SCAN_INTERVAL    10 /* ms */
#define SURVEY_INIT_TIME            20 /* s */
#define SURVEY_WARN_PROCENT_LIMIT   5  /* % */

/* The stats entry has per band (type) phy_name and scan_type context */
static ds_dlist_t                   g_survey_ctx_list =
                                        DS_DLIST_INIT(sm_survey_ctx_t,
                                                      node);

/******************************************************************************/

static inline sm_survey_ctx_t * sm_survey_ctx_alloc()
{
    sm_survey_ctx_t *survey_ctx = NULL;

    survey_ctx = malloc(sizeof(sm_survey_ctx_t));
    if (survey_ctx) {
        memset(survey_ctx, 0, sizeof(sm_survey_ctx_t));
    }

    return survey_ctx;
}

static inline void sm_survey_ctx_free(sm_survey_ctx_t *survey_ctx)
{
    if (NULL != survey_ctx) {
        free(survey_ctx);
    }
}

static
sm_survey_ctx_t *sm_survey_ctx_get (
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type)
{
    sm_survey_ctx_t                *survey_ctx = NULL;
    ds_dlist_iter_t                 ctx_iter;

    radio_entry_t                  *radio_entry = NULL;

    /* Find per radio survey ctx  */
    for (   survey_ctx = ds_dlist_ifirst(&ctx_iter,&g_survey_ctx_list);
            survey_ctx != NULL;
            survey_ctx = ds_dlist_inext(&ctx_iter))
    {
        radio_entry = survey_ctx->radio_cfg;

        /* The stats entry has per band (type) and phy_name context */
        if (    (radio_cfg->type == radio_entry->type)
             && (scan_type == survey_ctx->scan_type)
           ) {
            LOGT("Fetched %s %s survey reporting context",
                 radio_get_name_from_cfg(radio_entry),
                 radio_get_scan_name_from_type(scan_type));
            return survey_ctx;
        }
    }

    /* No survey ctx found, create new one */
    survey_ctx = NULL;
    survey_ctx = sm_survey_ctx_alloc();
    if(survey_ctx) {
        survey_ctx->scan_type = scan_type;
        survey_ctx->radio_cfg = radio_cfg;
        ds_dlist_insert_tail(&g_survey_ctx_list, survey_ctx);
        LOGT("Created %s %s survey reporting context",
             radio_get_name_from_cfg(radio_cfg),
             radio_get_scan_name_from_type(scan_type));
    }

    return survey_ctx;
}

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static
bool sm_survey_timer_set(
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
bool sm_survey_report_timer_restart(
        ev_timer                   *timer)
{
    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) timer->data;
    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;

    if (request_ctx->reporting_count) {
        request_ctx->reporting_count--;

        LOGD("Updated %s %s survey reporting count=%d",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             request_ctx->reporting_count);

        /* If reporting_count becomes zero, then stop reporting */
        if (0 == request_ctx->reporting_count) {
            sm_survey_timer_set(timer, false);
            if (request_ctx->sampling_interval) {
                sm_survey_timer_set(
                        &survey_ctx->update_timer, false);
            }

            LOGD("Stopped %s %s survey reporting (count expired)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type));
            return true;
        }
    }

    return true;
}

static
bool sm_survey_records_clear(
        sm_survey_ctx_t            *survey_ctx,
        ds_dlist_t                 *record_list)
{
    ds_dlist_iter_t                 survey_iter;

    if (!survey_ctx || !record_list) {
        return false;
    }

    if (ds_dlist_is_empty(record_list)) {
        return true;
    }

    dpp_survey_record_t            *survey = NULL;
    for (   survey = ds_dlist_ifirst(&survey_iter, record_list);
            survey != NULL;
            survey = ds_dlist_inext(&survey_iter))
    {
        ds_dlist_iremove(&survey_iter);
        dpp_survey_record_free(survey);
        survey = NULL;
    }

    return true;
}

static
bool sm_survey_target_clear(
        sm_survey_ctx_t            *survey_ctx,
        ds_dlist_t                 *survey_list)
{
    target_survey_record_t         *survey = NULL;
    ds_dlist_iter_t                 survey_iter;

    if (!survey_ctx || !survey_list) {
        return false;
    }

    if (ds_dlist_is_empty(survey_list)) {
        return true;
    }

    for (   survey = ds_dlist_ifirst(&survey_iter, survey_list);
            survey != NULL;
            survey = ds_dlist_inext(&survey_iter))
    {
        ds_dlist_iremove(&survey_iter);
        target_survey_record_free(survey);
        survey = NULL;
    }

    return true;
}

static
bool sm_survey_report_clear(
        sm_survey_ctx_t            *survey_ctx,
        ds_dlist_t                 *report_list)
{
    ds_dlist_iter_t                 survey_iter;
    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;

    if (!survey_ctx || !report_list) {
        return false;
    }

    if (ds_dlist_is_empty(report_list)) {
        return true;
    }

    if (REPORT_TYPE_RAW == request_ctx->report_type) {
        dpp_survey_record_t            *survey = NULL;
        for (   survey = ds_dlist_ifirst(&survey_iter, report_list);
                survey != NULL;
                survey = ds_dlist_inext(&survey_iter))
        {
            ds_dlist_iremove(&survey_iter);
            dpp_survey_record_free(survey);
            survey = NULL;
        }
    }
    else if (REPORT_TYPE_AVERAGE == request_ctx->report_type) {
        dpp_survey_record_avg_t        *survey = NULL;
        for (   survey = ds_dlist_ifirst(&survey_iter, report_list);
                survey != NULL;
                survey = ds_dlist_inext(&survey_iter))
        {
            ds_dlist_iremove(&survey_iter);
            free(survey);
            survey = NULL;
        }
    }
    else {
        return false;
    }

    return true;
}

static
void sm_survery_target_validate (
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type,
        dpp_survey_record_t        *survey_record)
{
    uint32_t chan_sum = survey_record->chan_rx + survey_record->chan_tx;
    if (chan_sum > survey_record->chan_busy){
        if ((chan_sum - survey_record->chan_busy) > SURVEY_WARN_PROCENT_LIMIT) {
            LOGW("Processed %s %s %u survey percent "
                 "{busy=%u is less then rx=%u + tx=%u)",
                 radio_get_name_from_cfg(radio_cfg),
                 radio_get_scan_name_from_type(scan_type),
                 survey_record->info.chan,
                 survey_record->chan_busy,
                 survey_record->chan_rx,
                 survey_record->chan_tx);
        }

        if (survey_record->chan_tx > survey_record->chan_busy)
            survey_record->chan_tx = survey_record->chan_busy;

        survey_record->chan_rx = survey_record->chan_busy - survey_record->chan_tx;
    }

    if (survey_record->chan_self > survey_record->chan_rx){
        if ((survey_record->chan_self - survey_record->chan_rx) > SURVEY_WARN_PROCENT_LIMIT) {
            LOGW("Processed %s %s %u survey percent "
                 "{self=%u is bigger then rx=%u)",
                 radio_get_name_from_cfg(radio_cfg),
                 radio_get_scan_name_from_type(scan_type),
                 survey_record->info.chan,
                 survey_record->chan_self,
                 survey_record->chan_rx);
        }
        survey_record->chan_self = survey_record->chan_rx;
    }
}

static
bool sm_survey_report_calculate_average (
        sm_survey_ctx_t            *survey_ctx)
{
    dpp_survey_report_data_t       *report_ctx =
        &survey_ctx->report;
    ds_dlist_t                     *report_list =
        &report_ctx->list;
    ds_dlist_t                     *record_list =
        &survey_ctx->record_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CALC(_name) do { \
        avg_record[chan_index]._name.avg += record_entry->_name;  \
        if(avg_record[chan_index]._name.num) { \
            avg_record[chan_index]._name.min = \
                MIN(avg_record[chan_index]._name.min, record_entry->_name); \
            avg_record[chan_index]._name.max = \
                MAX(avg_record[chan_index]._name.max, record_entry->_name); \
        } else { \
            avg_record[chan_index]._name.min = record_entry->_name; \
            avg_record[chan_index]._name.max = record_entry->_name; \
        } \
        avg_record[chan_index]._name.num++; \
    } while (0)

    ds_dlist_init(
            report_list,
            dpp_survey_record_avg_t,
            node);

    dpp_survey_record_t            *record_entry = NULL;
    ds_dlist_iter_t                 record_iter;
    dpp_survey_record_avg_t        *report_entry = NULL;
    dpp_survey_record_avg_t         avg_record[RADIO_MAX_CHANNELS];
    uint32_t                        chan_index;

    /* Traverse through cached raw results and derive aggregate */
    memset(&avg_record, 0, sizeof(avg_record));
    for (   record_entry = ds_dlist_ifirst(&record_iter, record_list);
            record_entry != NULL;
            record_entry = ds_dlist_inext(&record_iter))
    {
        /* Get index from channel to fetch the cached entry */
        chan_index =
            radio_get_chan_index(
                    radio_cfg_ctx->type,
                    record_entry->info.chan);

        avg_record[chan_index].info.chan = record_entry->info.chan;

        /* Sum all and derive average later */
        CALC(chan_busy);
        CALC(chan_tx);
        CALC(chan_self);
        CALC(chan_rx);
        CALC(chan_busy_ext);
    }

#define AVG(_name) do { \
        report_entry->_name.avg = report_entry->_name.avg / report_entry->_name.num; \
        LOGD("Sending %s %s %u  survey report " \
             "average %s %d (min %d, max %d, num %d)", \
             radio_get_name_from_cfg(radio_cfg_ctx), \
             radio_get_scan_name_from_type(scan_type), \
             report_entry->info.chan, \
             #_name, \
             report_entry->_name.avg, \
             report_entry->_name.min, \
             report_entry->_name.max, \
             report_entry->_name.num); \
    } while (0)

    for (chan_index = 0; chan_index < RADIO_MAX_CHANNELS; chan_index++)
    {
        /* Skip non averaged channels */
        if (avg_record[chan_index].info.chan) {
            report_entry = calloc(1, sizeof(*report_entry));
            if (NULL == report_entry) {
                LOGE("Sending %s %s survey report"
                     "(Failed to allocate memory)",
                     radio_get_name_from_cfg(radio_cfg_ctx),
                     radio_get_scan_name_from_type(scan_type));
                return false;
            }

            memcpy(report_entry, &avg_record[chan_index], sizeof(*report_entry));

            /* Calculate average value from above sum */
            AVG(chan_busy);
            AVG(chan_tx);
            AVG(chan_self);
            AVG(chan_rx);
            AVG(chan_busy_ext);

            ds_dlist_insert_tail(report_list, report_entry);
        }
    }

#undef MIN
#undef MAX
#undef CALC
#undef AVG

    return true;
}

static
bool sm_survey_report_calculate_raw (
        sm_survey_ctx_t            *survey_ctx)
{
    dpp_survey_report_data_t       *report_ctx =
        &survey_ctx->report;
    ds_dlist_t                     *report_list =
        &report_ctx->list;
    ds_dlist_t                     *record_list =
        &survey_ctx->record_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;

    ds_dlist_init(
            report_list,
            dpp_survey_record_t,
            node);

    dpp_survey_record_t            *record_entry = NULL;
    ds_dlist_iter_t                 record_iter;
    dpp_survey_record_t            *report_entry = NULL;

    for (   record_entry = ds_dlist_ifirst(&record_iter, record_list);
            record_entry != NULL;
            record_entry = ds_dlist_inext(&record_iter))
    {
        report_entry = dpp_survey_record_alloc();
        if (NULL == report_entry) {
            LOGE("Sending %s %s survey report"
                 "(Failed to allocate memory)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type));
            return false;
        }

        /* Copy info and stats */
        memcpy(report_entry, record_entry, sizeof(*report_entry));

        LOGD("Sending %s %s %u survey report "
             "{busy=%u tx=%u self=%u rx=%u ext=%u duration=%u}",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             report_entry->info.chan,
             report_entry->chan_busy,
             report_entry->chan_tx,
             report_entry->chan_self,
             report_entry->chan_rx,
             report_entry->chan_busy_ext,
             report_entry->duration_ms);

        ds_dlist_insert_tail(report_list, report_entry);
    }

    return true;
}

static
bool sm_survey_report_send(
        sm_survey_ctx_t            *survey_ctx)
{
    bool                            status;
    dpp_survey_report_data_t       *report_ctx =
        &survey_ctx->report;
    ds_dlist_t                     *report_list =
        &report_ctx->list;
    ds_dlist_t                     *record_list =
        &survey_ctx->record_list;
    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    ev_timer                       *report_timer =
        &survey_ctx->report_timer;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;

    /* Restart timer */
    sm_survey_report_timer_restart(report_timer);

    report_ctx->radio_type  = radio_cfg_ctx->type;
    report_ctx->report_type = request_ctx->report_type;
    report_ctx->scan_type   = scan_type;

    /* Report_timestamp is base-timestamp + relative start time offset */
    report_ctx->timestamp_ms =
        request_ctx->reporting_timestamp - survey_ctx->report_ts +
        get_timestamp();

    /* Derive values based on request from cloud */
    switch(request_ctx->report_type)
    {
        case REPORT_TYPE_RAW:
            sm_survey_report_calculate_raw(
                    survey_ctx);
            break;
        case REPORT_TYPE_AVERAGE:
            sm_survey_report_calculate_average(
                    survey_ctx);
            break;
        case REPORT_TYPE_HISTOGRAM:
        case REPORT_TYPE_PERCENTILE:
        default:
            LOGE("Sending %s %s survey report"
                 " (Invalid report type)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type));
            goto exit;
    }

    LOGI("Sending %s %s survey report at '%s'",
         radio_get_name_from_cfg(radio_cfg_ctx),
         radio_get_scan_name_from_type(scan_type),
         sm_timestamp_ms_to_date(report_ctx->timestamp_ms));

    /* Send records to MQTT FIFO (Skip empty reports) */
    if (!ds_dlist_is_empty(&report_ctx->list)) {
        dpp_put_survey(report_ctx);
    }

    status =
            sm_survey_report_clear(
                survey_ctx,
                report_list);
    if (true != status) {
        goto exit;
    }

exit:
    status =
        sm_survey_records_clear(
                survey_ctx,
                record_list);
    if (true != status) {
        return false;
    }

    survey_ctx->record_qty = 0;

    SM_SANITY_CHECK_TIME(report_ctx->timestamp_ms,
                         &request_ctx->reporting_timestamp,
                         &survey_ctx->report_ts);

    return true;
}

static
bool sm_survey_update_list_cb (
        ds_dlist_t                 *survey_list,
        void                       *ctx,
        int                         survey_status)
{
    bool                            rc;

    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) ctx;
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    ds_dlist_t                     *record_list =
        &survey_ctx->record_list;
    radio_scan_type_t               scan_type =
       survey_ctx->scan_type;

    dpp_survey_record_t            *result_entry = NULL;

    target_survey_record_t         *record_entry;
    target_survey_record_t         *survey_entry = NULL;
    ds_dlist_iter_t                 survey_iter;

    uint32_t                        chan_index;

    if (NULL == survey_ctx) {
        LOGE("Processing survey record "
             "(empty context)");
        return false;
    }

    if(true != survey_status) {
        LOGE("Processing %s %s survey record %u "
             "(failed to get stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             survey_ctx->record_qty);
        goto clear;
    }

    /* Search through received survey records and calculate delta */
    for (   survey_entry = ds_dlist_ifirst(&survey_iter, survey_list);
            survey_entry != NULL;
            survey_entry = ds_dlist_inext(&survey_iter))
    {
        /* Get index from channel to fetch the cached entry */
        chan_index =
            radio_get_chan_index(
                    radio_cfg_ctx->type,
                    survey_entry->info.chan);

        record_entry = &survey_ctx->records[chan_index];

        result_entry =
            dpp_survey_record_alloc();
        if (NULL == result_entry) {
            LOGE("Processing %s %s survey report "
                 "(Failed to allocate memory)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type));

            goto clear;
        }

        /* Copy general data (chan) */
        memcpy (&result_entry->info,
                &survey_entry->info,
                sizeof(result_entry->info));

        /* Calculate delta inside target and convert it to your needs */
        rc =
            target_stats_survey_convert (
                    radio_cfg_ctx,
                    scan_type,
                    survey_entry,
                    record_entry,
                    result_entry);
        if (true != rc) {
            LOGE("Processing %s %s %u survey record %u "
                 "(Failed to convert stats)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type),
                 result_entry->info.chan,
                 survey_ctx->record_qty);
            dpp_survey_record_free(result_entry);
            continue;
        }

        sm_survery_target_validate (
                radio_cfg_ctx,
                scan_type,
                result_entry);

        LOGD("Processed %s %s %u survey percent "
             "{busy=%u tx=%u self=%u rx=%u ext=%u duration=%u}",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             result_entry->info.chan,
             result_entry->chan_busy,
             result_entry->chan_tx,
             result_entry->chan_self,
             result_entry->chan_rx,
             result_entry->chan_busy_ext,
             result_entry->duration_ms);

        result_entry->info.timestamp_ms =
            request_ctx->reporting_timestamp - survey_ctx->report_ts +
            survey_entry->info.timestamp_ms;

        survey_ctx->record_qty++;

        ds_dlist_insert_tail(record_list, result_entry);

        /* Update cache */
        *record_entry = *survey_entry;
    }

    LOGD("Processed %s %s survey records %u",
         radio_get_name_from_cfg(radio_cfg_ctx),
         radio_get_scan_name_from_type(scan_type),
         survey_ctx->record_qty);

    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN: /* Move to the next channel */
            channel_ctx->chan_index++;

            /* Start from beginning when max is reached */
            if (channel_ctx->chan_index >= channel_ctx->chan_num) {
                channel_ctx->chan_index = 0;
            }
            break;
        case RADIO_SCAN_TYPE_FULL: /* Send report */
            sm_survey_report_send(survey_ctx);
            break;
        default:
            LOGE("Processed %s %s survey records (unsupported type)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type));
            sm_survey_records_clear(
                    survey_ctx,
                    record_list);
            goto clear;
    }

clear:
    /* Clear temporary received survey target list */
    rc =
        sm_survey_target_clear(
                survey_ctx,
                survey_list);
    if (true != rc) {
        LOGE("Processing %s %s survey report "
             "(failed to clear survey list)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    return true;
}

static
void sm_survey_scan_cb(
        void                       *scan_ctx,
        int                         scan_status)
{
    bool                            rc;

    sm_survey_ctx_t                *survey_ctx = NULL;
    sm_chan_list_t                 *channel_ctx = NULL;
    radio_entry_t                  *radio_cfg_ctx = NULL;

    uint32_t                       *survey_chan;
    uint32_t                        survey_num;

    ds_dlist_t                     *survey_list = NULL;
    radio_scan_type_t               scan_type;

    if (NULL == scan_ctx) {
        return;
    }

    survey_ctx      = (sm_survey_ctx_t *) scan_ctx;
    radio_cfg_ctx   = survey_ctx->radio_cfg;
    channel_ctx     = &survey_ctx->chan_list;
    survey_list     = &survey_ctx->survey_list;
    scan_type       = survey_ctx->scan_type;

    /* Prepare channels for which we need records */
    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
            survey_chan =
                &channel_ctx->chan_list[channel_ctx->chan_index];
            survey_num = 1;
            break;
        case RADIO_SCAN_TYPE_FULL:
            survey_chan = channel_ctx->chan_list;
            survey_num = channel_ctx->chan_num;
            break;
        default:
            return;
    }

    /* Scan error occurred, skip records and retry */
    if (!scan_status) {
        LOGE("Processing %s %s %d survey report "
             "(Failed to scan)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             *survey_chan);
        return;
    }

    /* Fetch latest neighbor records. This is dependent on the
       neighbor configuration if reproting_interval in set and the
       sampling is not we shall update neighbor records through
       survey scanning!
     */
    sm_neighbor_stats_results_update(
            radio_cfg_ctx,
            scan_type,
            scan_status);

    /* Fetch latest survey records */
    rc =
        target_stats_survey_get (
                radio_cfg_ctx,
                survey_chan,
                survey_num,
                scan_type,
                sm_survey_update_list_cb,
                survey_list,
                survey_ctx);
    if (true != rc) {
        LOGE("Processing %s %s %u survey record %u "
             "(failed to get stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             *survey_chan,
             survey_ctx->record_qty);
    }

    return;
}

static
bool sm_survey_threshold_util_cb (
        ds_dlist_t                 *survey_list,
        void                       *ctx,
        int                         survey_status)
{
    bool                            rc;
    bool                            status;

    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) ctx;
    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
       survey_ctx->scan_type;

    dpp_survey_record_t             result_entry;
    target_survey_record_t         *survey_entry = NULL;

    uint32_t                       *scan_chan;
    uint32_t                        scan_num;
    uint32_t                        scan_interval = request_ctx->scan_interval;

    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
            scan_chan =
                &channel_ctx->chan_list[channel_ctx->chan_index];
            scan_num = 1;
            break;
        case RADIO_SCAN_TYPE_FULL:
            scan_chan = channel_ctx->chan_list;
            scan_num = channel_ctx->chan_num;
            break;
        default:
            goto clear;
    }

    if(true != survey_status) {
        LOGE("Processing %s %s %u survey record %u "
             "(failed to parse threshold stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             *scan_chan,
             survey_ctx->record_qty);
        goto clear;
    }

    /* Search through received survey records (only one for on_chan!)
       and calculate delta */
    survey_entry = ds_dlist_head(survey_list);
    if (NULL == survey_entry) {
        LOGE("Processing %s %s %u survey record %u "
             "(Empty threshold stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             *scan_chan,
             survey_ctx->record_qty);
        goto clear;
    }

    memset(&result_entry, 0, sizeof (result_entry));
    rc =
        target_stats_survey_convert (
                radio_cfg_ctx,
                RADIO_SCAN_TYPE_ONCHAN,
                survey_entry,
                &survey_ctx->threshold_record,
                &result_entry);
    if (true != rc) {
        LOGE("Processing %s %s %u survey "
             "(failed to convert threshold stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             radio_cfg_ctx->chan);
        goto clear;
    }

    sm_survery_target_validate (
            radio_cfg_ctx,
            scan_type,
            &result_entry);

    /* update cache with current values */
    survey_ctx->threshold_record = *survey_entry;

    int threshold_util = result_entry.chan_tx + result_entry.chan_self;
    LOGD("Checking %s %s survey threshold util: %d/%d delay: %ld/%d count: %d/%d",
         radio_get_name_from_cfg(radio_cfg_ctx),
         radio_get_scan_name_from_type(scan_type),
         threshold_util, request_ctx->threshold_util,
         survey_ctx->threshold_time_delta, request_ctx->threshold_max_delay,
         survey_ctx->threshold_count, request_ctx->radio_chan_list.chan_num);

    if (threshold_util >= request_ctx->threshold_util) {
        LOGI("Skip processing %s %s survey (threshold util exceeded %d >= %d)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             threshold_util,
             request_ctx->threshold_util);
        goto clear;
    }
    else {
        survey_ctx->threshold_ts = 0;
        survey_ctx->threshold_count = 0;
    }

    /* Schedule offchannel scan if ON_CHANNEL threshold is not crossed */
    sm_scan_request_t           scan_request;
    memset(&scan_request, 0, sizeof(scan_request));
    scan_request.radio_cfg  = radio_cfg_ctx;
    scan_request.chan_list  = scan_chan;
    scan_request.chan_num   = scan_num;
    scan_request.scan_type  = scan_type;
    scan_request.dwell_time = scan_interval;
    scan_request.scan_cb    = sm_survey_scan_cb;
    scan_request.scan_ctx   = survey_ctx;

    rc = sm_scan_schedule(&scan_request);
    if (true != rc) {
        LOGE("Processing %s %s %d survey (Failed to schedule scan)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             *scan_chan);
        goto clear;
    }

clear:
    status =
        sm_survey_target_clear(
                survey_ctx,
                survey_list);
    if (true != status) {
        LOGE("Processing %s %s survey report "
             "(failed to clear threshold survey list)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    return true;
}

static
bool sm_survey_stats_update (
        sm_survey_ctx_t            *survey_ctx)
{
    bool                            rc;

    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    ds_dlist_t                     *survey_list =
        &survey_ctx->survey_list;
    radio_scan_type_t               scan_type =
       survey_ctx->scan_type;

    uint32_t                       *scan_chan;
    uint32_t                        scan_num;
    uint32_t                        scan_interval = request_ctx->scan_interval;

    /* check if radio is exists */
    if (target_is_radio_interface_ready(radio_cfg_ctx->phy_name) != true) {
        LOGT("Skip processing %s %s survey report "
             "(radio not configured)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    /* check if vif interface exists */
    if (target_is_interface_ready(radio_cfg_ctx->if_name) != true) {
        LOGT("Skip processing %s %s survey report "
             "(interface %s not configured)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             radio_cfg_ctx->if_name);
        return false;
    }

    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
            scan_chan =
                &channel_ctx->chan_list[channel_ctx->chan_index];
            scan_num = 1;
            break;
        case RADIO_SCAN_TYPE_FULL:
            scan_chan = channel_ctx->chan_list;
            scan_num = channel_ctx->chan_num;
            break;
        default:
            return false;
    }

     /* Threshold
      * when the threshold limit is exceeded, start a time window
      * of threshold_max_delay, within this time each channel has to
      * be scanned at least once, otherwise ignore threshold when
      * this time limit runs out and pass through all remaining requests.
      */
    if (request_ctx->threshold_util) {
        // threshold configured
        time_t t_now = time_monotonic();
        survey_ctx->threshold_time_delta = request_ctx->sampling_interval;
        if (survey_ctx->threshold_ts) {
            survey_ctx->threshold_time_delta = t_now - survey_ctx->threshold_ts;
        } else { /* initialize */
            survey_ctx->threshold_ts = t_now - request_ctx->sampling_interval;
            survey_ctx->threshold_count = 0;
        }

        if (survey_ctx->threshold_time_delta >= request_ctx->threshold_max_delay) {
            scan_interval = SURVEY_MIN_SCAN_INTERVAL;

            // always scan if max_delay exceeded
            LOGI("Force processing %s %s survey delay: %ld/%d count: %d/%d",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type),
                 survey_ctx->threshold_time_delta, request_ctx->threshold_max_delay,
                 survey_ctx->threshold_count, request_ctx->radio_chan_list.chan_num);

            survey_ctx->threshold_ts = t_now;
            if (++survey_ctx->threshold_count >= (int)request_ctx->radio_chan_list.chan_num) {
                // all channels were processed, reset threshold timestamp and count
                survey_ctx->threshold_count = 0;
            }
        } else { // max_delay not exceeded, check limits
            /* set buffer and calback function to be called after records are ready */
            rc =
                target_stats_survey_get (
                        radio_cfg_ctx,
                        &radio_cfg_ctx->chan,
                        1,
                        RADIO_SCAN_TYPE_ONCHAN,
                        sm_survey_threshold_util_cb,
                        survey_list,
                        survey_ctx);
            if (true != rc) {
                LOGE("initializing %s %s %u survey reporting "
                     "(failed to get threshold stats)",
                     radio_get_name_from_cfg(radio_cfg_ctx),
                     radio_get_scan_name_from_type(scan_type),
                     radio_cfg_ctx->chan);
                return false;
            }

            /* Scan will be scheduled by sm_survey_threshold_util_cb
               if conditions are OK!
             */
            return true;
        }
    }

    sm_scan_request_t           scan_request;
    memset(&scan_request, 0, sizeof(scan_request));
    scan_request.radio_cfg  = radio_cfg_ctx;
    scan_request.chan_list  = scan_chan;
    scan_request.chan_num   = scan_num;
    scan_request.scan_type  = scan_type;
    scan_request.dwell_time = scan_interval;
    scan_request.scan_cb    = sm_survey_scan_cb;
    scan_request.scan_ctx   = survey_ctx;

    rc = sm_scan_schedule(&scan_request);
    if (true != rc) {
        LOG(ERR,
            "Processing %s %s %d survey ",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            *scan_chan);
        return false;
    }

    return true;
}

static
void sm_survey_update_timer_cb (EV_P_ ev_timer *w, int revents)
{
    bool                           status;

    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) w->data;
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;

    LOG(DEBUG,
        "Processing %s %s %u (%u) survey sample %u",
        radio_get_name_from_cfg(radio_cfg_ctx),
        radio_get_scan_name_from_type(scan_type),
        channel_ctx->chan_list[channel_ctx->chan_index],
        radio_cfg_ctx->chan,
        ++survey_ctx->record_qty);

    status =
        sm_survey_stats_update(
                survey_ctx);
    if (true != status) {
        /* If error is reported due to any kind of reason
           we should not exit the Processing therefore we
           skip this sample and send the report
         */
    }

    /* NTP is not available on all devices, therefore sync to Cloud time
       (NOTE this drifts a little bit because of connfiguration sending path)

       Synchronization delay is added to all nodes only once to allow
       devices in multihop cluster to approximately snyc to same
       sampling interval - this is needed only for off-channel survey!
     */
     if (survey_ctx->sync_delay) {
         sm_stats_request_t                 *request_ctx =
             &survey_ctx->request;
         ev_timer                           *update_timer =
             &survey_ctx->update_timer;
         update_timer->repeat = request_ctx->sampling_interval;
         sm_survey_timer_set(update_timer, true);
         survey_ctx->sync_delay = 0;
     }
}

static
void sm_survey_report_timer_cb (EV_P_ ev_timer *w, int revents)
{
    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) w->data;
    radio_scan_type_t               scan_type =
       survey_ctx->scan_type;

    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
            sm_survey_report_send(survey_ctx);
            break;
        case RADIO_SCAN_TYPE_FULL:
            sm_survey_update_timer_cb(EV_DEFAULT, w, revents);
        default:
            return;
    }
}

static
bool sm_survey_stats_chanlist_update (
        sm_survey_ctx_t            *survey_ctx)
{
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    sm_chan_list_t                 *channel_cfg =
        &survey_ctx->request.radio_chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;
    uint32_t                        chan_index = 0;

    memset(channel_ctx, 0, sizeof(sm_chan_list_t));

    if (scan_type == RADIO_SCAN_TYPE_ONCHAN) {
        channel_ctx->chan_list[channel_ctx->chan_num++] =
            radio_cfg_ctx->chan;
        if (radio_cfg_ctx->chan == 0){
            LOG(ERROR,
                "Updated %s %s survey chan %u (Invalid channel)",
                radio_get_name_from_cfg(radio_cfg_ctx),
                radio_get_scan_name_from_type(scan_type),
                channel_ctx->chan_list[channel_ctx->chan_num - 1]);
            return false;
        }
        LOG(DEBUG,
            "Updated %s %s survey chan %u",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            channel_ctx->chan_list[channel_ctx->chan_num - 1]);
        return true;
    }

    for (   chan_index = 0;
            chan_index < channel_cfg->chan_num;
            chan_index++) {
        /* Skip on-channel */
        if (radio_cfg_ctx->chan == channel_cfg->chan_list[chan_index]) {
            LOG(TRACE,
                "Skip updating %s %s survey chan %u "
                "(on-chan specified)",
                radio_get_name_from_cfg(radio_cfg_ctx),
                radio_get_scan_name_from_type(scan_type),
                channel_cfg->chan_list[chan_index]);
            continue;
        }

        channel_ctx->chan_list[channel_ctx->chan_num++] =
            channel_cfg->chan_list[chan_index];

        LOG(DEBUG,
            "Updated %s %s survey chan %u",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            channel_ctx->chan_list[channel_ctx->chan_num - 1]);
    }

    return true;
}

static
bool sm_survey_stats_init (
        sm_survey_ctx_t           *survey_ctx);

static
void sm_survey_init_timer_cb (EV_P_ ev_timer *w, int revents)
{
    bool                           status;

    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) w->data;

    status =
        sm_survey_stats_init(
                survey_ctx);
    if (true != status) {
        return;
    }
}

static
bool sm_survey_init_cb (
        ds_dlist_t                 *survey_list,
        void                       *ctx,
        int                         survey_status)
{
    bool                            rc;

    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) ctx;
    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;

    ev_timer                       *report_timer =
        &survey_ctx->report_timer;
    ev_timer                       *update_timer =
        &survey_ctx->update_timer;
    ev_timer                       *init_timer =
        &survey_ctx->init_timer;

    target_survey_record_t         *record_entry;
    target_survey_record_t         *survey_entry = NULL;
    ds_dlist_iter_t                 survey_iter;
    uint32_t                        chan_index;

    if(true != survey_status) {
        LOGE("Initializing %s %s survey record %u "
             "(failed to get stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             survey_ctx->record_qty);
        goto clear;
    }

    /* Search through received survey records and calculate delta */
    for (   survey_entry = ds_dlist_ifirst(&survey_iter, survey_list);
            survey_entry != NULL;
            survey_entry = ds_dlist_inext(&survey_iter))
    {
        /* Get index from channel to fetch the cached entry */
        chan_index =
            radio_get_chan_index(
                    radio_cfg_ctx->type,
                    survey_entry->info.chan);

        record_entry = &survey_ctx->records[chan_index];

        /* Update cache */
        *record_entry = *survey_entry;
    }

    LOGI("Initialized %s %s survey stats",
         radio_get_name_from_cfg(radio_cfg_ctx),
         radio_get_scan_name_from_type(scan_type));

    /* Sampling vs one big averaged report */
    if (request_ctx->sampling_interval) {
        update_timer->repeat = request_ctx->sampling_interval + survey_ctx->sync_delay;

        /* The 2.4 and 5G need to be apart for min 10s so we
           do not degrade performance on middle nodes!

           Since min interval is 20 we can use half of sampling.
           The same logic need to be considered with max delay

           NOTE: Only the first sample is delayed!
         */
        if ( (    (RADIO_TYPE_5G == radio_cfg_ctx->type)
               || (RADIO_TYPE_5GL == radio_cfg_ctx->type)
               || (RADIO_TYPE_5GU == radio_cfg_ctx->type)
             )
             && survey_ctx->sync_delay) {
            update_timer->repeat += request_ctx->sampling_interval / 2 ;
        }
        sm_survey_timer_set(update_timer, true);
    }

    report_timer->repeat = request_ctx->reporting_interval;
    sm_survey_timer_set(report_timer, true);

    survey_ctx->report_ts = get_timestamp();

    /* Stop init timer and start with processing */
    sm_survey_timer_set(init_timer, false);

    LOGI("Started %s %s survey reporting",
         radio_get_name_from_cfg(radio_cfg_ctx),
         radio_get_scan_name_from_type(scan_type));

clear:
    rc =
        sm_survey_target_clear(
                survey_ctx,
                survey_list);

    if (true != rc) {
        LOGE("Initializing %s %s survey report "
             "(failed to clear survey list)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    return true;
}

static
bool sm_survey_threshold_init_cb (
        ds_dlist_t                 *survey_list,
        void                       *ctx,
        int                         survey_status)
{
    bool                            rc;

    sm_survey_ctx_t                *survey_ctx =
        (sm_survey_ctx_t *) ctx;
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;

    target_survey_record_t         *survey_entry = NULL;

    uint32_t                       *scan_chan;
    uint32_t                        scan_num;

    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
            scan_chan =
                &channel_ctx->chan_list[channel_ctx->chan_index];
            scan_num = 1;
            break;
        case RADIO_SCAN_TYPE_FULL:
            scan_chan = channel_ctx->chan_list;
            scan_num = channel_ctx->chan_num;
            break;
        default:
            goto clear;
    }

    if(true != survey_status) {
        LOGE("Initializing %s %s %u survey record %u "
             "(failed to get threshold stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             *scan_chan,
             survey_ctx->record_qty);
        goto clear;
    }

    /* Search through received survey records (only one for on_chan!)
       and calculate delta */
    survey_entry = ds_dlist_head(survey_list);
    if (NULL == survey_entry) {
        LOGE("Processing %s %s %u survey record %u "
             "(Empty threshold stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             *scan_chan,
             survey_ctx->record_qty);
        goto clear;
    }

    /* update cache with current values */
    survey_ctx->threshold_record = *survey_entry;

    rc =
        sm_survey_target_clear(
                survey_ctx,
                survey_list);
    if (true != rc) {
        LOGE("Initializing %s %s survey report "
             "(failed to clear survey list)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    LOG(DEBUG,
        "Initialized %s %s survey threshold stats",
        radio_get_name_from_cfg(radio_cfg_ctx),
        radio_get_scan_name_from_type(scan_type));

    rc =
        target_stats_survey_get (
                radio_cfg_ctx,
                scan_chan,
                scan_num,
                scan_type,
                sm_survey_init_cb,
                survey_list,
                survey_ctx);
    if (true != rc) {
        LOG(ERR,
            "Initializing %s %s survey reporting"
            "(Failed to get stats)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type));
        return false;
    }

    return true;
clear:
    rc =
        sm_survey_target_clear(
                survey_ctx,
                survey_list);
    if (true != rc) {
        LOGE("Initializing %s %s survey report "
             "(failed to clear survey list)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    return false;
}

static
bool sm_survey_stats_init (
        sm_survey_ctx_t            *survey_ctx)
{
    bool                            rc;
    bool                            status;

    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    ds_dlist_t                     *record_list =
        &survey_ctx->record_list;
    ds_dlist_t                     *survey_list =
        &survey_ctx->survey_list;
    radio_scan_type_t               scan_type =
        survey_ctx->scan_type;
    ev_timer                       *init_timer =
        &survey_ctx->init_timer;

    uint32_t                       *scan_chan;
    uint32_t                        scan_num;

    init_timer->repeat = SURVEY_INIT_TIME;
    sm_survey_timer_set(init_timer, true);

    /* Check if radio is exists */
    if (!strlen(radio_cfg_ctx->phy_name)) {
        LOGE("Initializing %s %s survey report "
             "(Radio not configured)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    /* Check if vif interface exists */
    if (!strlen(radio_cfg_ctx->if_name)) {
        LOGE("Initializing %s %s survey report "
             "(Interface not configured)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    status =
        sm_survey_records_clear(
                survey_ctx,
                record_list);
    if (true != status) {
        return false;
    }

    status =
        sm_survey_target_clear(
                survey_ctx,
                survey_list);
    if (true != status) {
        return false;
    }

    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
            scan_chan =
                &channel_ctx->chan_list[channel_ctx->chan_index];
            scan_num = 1;
            break;
        case RADIO_SCAN_TYPE_FULL:
            scan_chan = channel_ctx->chan_list;
            scan_num = channel_ctx->chan_num;
            break;
        default:
            return false;
    }

    /* Threshold logic prevent scanning during traffic */
    if (request_ctx->threshold_util) {
        /* Set buffer and callback function to be called after records are ready */
        rc =
            target_stats_survey_get (
                    radio_cfg_ctx,
                    &radio_cfg_ctx->chan,
                    1,
                    RADIO_SCAN_TYPE_ONCHAN,
                    sm_survey_threshold_init_cb,
                    survey_list,
                    survey_ctx);
        if (true != rc) {
            LOGE("Initializing %s %s %u survey reporting "
                 "(Failed to get threshold stats)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type),
                 radio_cfg_ctx->chan);
            return false;
        }

        /* Survey is started through sm_survey_threshold_init_cb */
        return true;
    }

    rc =
        target_stats_survey_get (
                radio_cfg_ctx,
                scan_chan,
                scan_num,
                scan_type,
                sm_survey_init_cb,
                survey_list,
                survey_ctx);
    if (true != rc) {
        LOGE("Initializing %s %s survey reporting"
             "(Failed to get stats)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    return true;
}

static
bool sm_survey_stats_process (
        radio_entry_t              *radio_cfg, // For logging
        radio_scan_type_t           scan_type,
        sm_survey_ctx_t            *survey_ctx)
{
    bool                            status;

    sm_stats_request_t             *request_ctx =
        &survey_ctx->request;
    sm_chan_list_t                 *channel_ctx =
        &survey_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        survey_ctx->radio_cfg;
    ev_timer                       *report_timer =
        &survey_ctx->report_timer;
    ev_timer                       *update_timer =
        &survey_ctx->update_timer;

    /* Skip %s survey report start if radio is not configured */
    if (!radio_cfg_ctx || (radio_cfg_ctx->chan == 0)) {
        LOGT("Skip starting %s %s survey reporting "
             "(Radio not configured)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return true;
    }

    /* Skip %s survey report start if stats are not configured */
    if (!survey_ctx->initialized) {
        LOGT("Skip starting %s %s survey reporting "
             "(Stats not configured)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return true;
    }

    /* Stop when reconfiguration is detected */
    if (RADIO_SCAN_TYPE_FULL != scan_type) {
        sm_survey_timer_set(update_timer, false);
    }
    sm_survey_timer_set(report_timer, false);

    /* Skip %s survey report start if timestamp is not specified */
    if (!request_ctx->reporting_timestamp) {
        LOGT("Skip starting %s %s survey reporting "
             "(Timestamp not configured)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return true;
    }

    /* Update list and consider on-channel */
    status =
        sm_survey_stats_chanlist_update (
                survey_ctx);
    if (true != status) {
        return false;
    }

    if (!channel_ctx->chan_num) {
        LOGE("Starting %s %s survey reporting "
             "(No channels)",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));
        return false;
    }

    /* Stop any target scan Processing */
    sm_scan_schedule_stop(radio_cfg, scan_type);

    if (request_ctx->reporting_interval) {
        /* When POD cluster (qty and num) is specified we shall try
           to synchronize scans across the multihop environment
           (Note this is applicable only when there is traffic)

           max_delay consists of:
           - pod_delay
           - cluster_delay

           max_delay =
           (pod_num * sampling_interval) +
           (pod_qty * sampling_interval)

         */
        if (request_ctx->threshold_util) {
            if (!request_ctx->sampling_interval) {
                LOGT("Skip starting %s %s survey reporting "
                     "(Sampling not configured)",
                     radio_get_name_from_cfg(radio_cfg_ctx),
                     radio_get_scan_name_from_type(scan_type));
                return true;
            }

            if (request_ctx->threshold_pod_qty) {
                request_ctx->threshold_max_delay =
                    (request_ctx->threshold_pod_num + request_ctx->threshold_pod_qty) *
                    request_ctx->sampling_interval;
            }
            else {
                /* Introduce random delay for max pod Qty = 6 (Selling package) */
                request_ctx->threshold_pod_num = rand() % 5 + 1;
                request_ctx->threshold_pod_qty = 6;

                request_ctx->threshold_max_delay +=
                    request_ctx->threshold_pod_num * request_ctx->sampling_interval;
            }

            LOGI("Updated %s %s survey threshold_max_delay (%d + %d) * %d) -> %ds) ",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type),
                 request_ctx->threshold_pod_num,
                 request_ctx->threshold_pod_qty,
                 request_ctx->sampling_interval,
                 request_ctx->threshold_max_delay);

            /* NTP is not available on all devices therefore sync to Cloud time
               (NOTE this drifts a little bit because of configuration sending path)

               Synchronization delay is added to all nodes only once to allow
               devices in multihop cluster to approximately sync to same
               sampling interval - this is needed only for off-channel survey!
             */
            survey_ctx->sync_delay =
                request_ctx->sampling_interval -
                (uint32_t)((request_ctx->reporting_timestamp / 1000) % request_ctx->sampling_interval);

            LOGI("Updated %s %s survey synchronization_delay %d - %d -> %"PRIu64"s) ",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type),
                 request_ctx->sampling_interval,
                 (uint32_t)((request_ctx->reporting_timestamp / 1000) % request_ctx->sampling_interval),
                 survey_ctx->sync_delay);
        }

        status =
            sm_survey_stats_init(
                    survey_ctx);
        if (true != status) {
            return false;
        }
    }
    else {
        LOGI("Stopped %s %s survey reporting",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type));

        memset(request_ctx, 0, sizeof(*request_ctx));

        sm_survey_timer_set(update_timer, false);
        sm_survey_timer_set(report_timer, false);
        sm_survey_timer_set(&survey_ctx->init_timer, false);
    }

    return true;
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
bool sm_survey_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request)
{
    bool                            status;

    sm_survey_ctx_t                *survey_ctx = NULL;
    sm_stats_request_t             *request_ctx = NULL;
    ev_timer                       *report_timer = NULL;
    ev_timer                       *update_timer = NULL;
    ev_timer                       *init_timer = NULL;
    ds_dlist_t                     *record_list = NULL;
    ds_dlist_t                     *survey_list = NULL;
    radio_scan_type_t               scan_type;

    if (NULL == request) {
        LOGE("Initializing survey reporting "
             "(Invalid request config)");
        return false;
    }
    scan_type = request->scan_type;

    survey_ctx = sm_survey_ctx_get(radio_cfg, scan_type);
    if (NULL == survey_ctx) {
        LOGE("Initializing survey reporting "
             "(Invalid survey context)");
        return false;
    }

    request_ctx     = &survey_ctx->request;
    update_timer    = &survey_ctx->update_timer;
    report_timer    = &survey_ctx->report_timer;
    record_list     = &survey_ctx->record_list;
    survey_list     = &survey_ctx->survey_list;
    init_timer      = &survey_ctx->init_timer;

    /* Initialize global storage and timer config only once */
    if (!survey_ctx->initialized) {
        memset(request_ctx, 0, sizeof(*request_ctx));

        LOGI("Initializing %s %s survey reporting",
             radio_get_name_from_cfg(radio_cfg),
             radio_get_scan_name_from_type(scan_type));

        /* Initialize survey list */
        ds_dlist_init(
                record_list,
                dpp_survey_record_t,
                node);

        ds_dlist_init(
                survey_list,
                target_survey_record_t,
                node);

        /* Reschedule initialization in case of error */
        ev_init (init_timer, sm_survey_init_timer_cb);
        init_timer->data = survey_ctx;

        /* Used only on/off channel survey */
        ev_init (update_timer, sm_survey_update_timer_cb);
        update_timer->data = survey_ctx;

        /* Initialize event lib timers and pass the global internal cache */
        ev_init (report_timer, sm_survey_report_timer_cb);
        report_timer->data = survey_ctx;

        survey_ctx->initialized = true;
    }

    /* Store and compare every request parameter */
    char param_str[32];
    sprintf(param_str,
            "%s %s",
            radio_get_scan_name_from_type(scan_type),
            "survey");
    REQUEST_PARAM_UPDATE(param_str, scan_type, "%d");
    REQUEST_PARAM_UPDATE(param_str, radio_type, "%d");
    REQUEST_PARAM_UPDATE(param_str, report_type, "%d");
    REQUEST_PARAM_UPDATE(param_str, reporting_count, "%d");
    REQUEST_PARAM_UPDATE(param_str, scan_interval, "%d");
    REQUEST_PARAM_UPDATE(param_str, reporting_interval, "%d");
    REQUEST_PARAM_UPDATE(param_str, sampling_interval, "%d");
    REQUEST_PARAM_UPDATE(param_str, threshold_util, "%d");
    REQUEST_PARAM_UPDATE(param_str, threshold_max_delay, "%d");
    REQUEST_PARAM_UPDATE(param_str, threshold_pod_qty, "%d");
    REQUEST_PARAM_UPDATE(param_str, threshold_pod_num, "%d");
    REQUEST_PARAM_UPDATE(param_str, reporting_timestamp, "%"PRIu64"");

    /* Update channel list */
    memcpy(&request_ctx->radio_chan_list,
            &request->radio_chan_list,
            sizeof (request_ctx->radio_chan_list));

    status =
        sm_survey_stats_process (
                radio_cfg,
                scan_type,
                survey_ctx);

    if (true != status) {
        return false;
    }

    return true;
}

bool sm_survey_report_radio_change(
        radio_entry_t              *radio_cfg)
{
    bool                            status;
    sm_survey_ctx_t                *survey_ctx = NULL;
    radio_scan_type_t               scan_type;
    int                             scan_index;

    if (NULL == radio_cfg) {
        LOG(ERR,
            "Changing survey reporting "
            "(Invalid radio config)");
        return false;
    }

    /* Update radio on all scan contexts. If initialized, reports will start */
    for (   scan_index = 0;
            scan_index < RADIO_SCAN_MAX_TYPE_QTY;
            scan_index++
        ) {
        scan_type   = radio_get_scan_type_from_index(scan_index);
        survey_ctx  = sm_survey_ctx_get(radio_cfg, scan_type);
        if (NULL == survey_ctx) {
            LOGE("Changing survey reporting "
                 "(Invalid survey context)");
            return false;
        }

        status =
            sm_survey_stats_process (
                    radio_cfg,
                    scan_type,
                    survey_ctx);

        if (true != status) {
            return false;
        }
    }

    return true;
}
