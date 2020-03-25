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
    radio_scan_type_t               scan_type;

    /* Internal structure used to keep channel scan track */
    sm_chan_list_t                  chan_list;

    /* Internal structure to store report timers */
    ev_timer                        report_timer;
    ev_timer                        update_timer;

    /* Structure containing cloud request and sampling params */
    sm_stats_request_t              request;
    /* Structure pointing to upper layer neighbor storage */
    dpp_neighbor_report_data_t      report;
    /* Report containing only changes */
    dpp_neighbor_list_t             diff_cache;

    /* Internal structure used to for neighbor result fetching */
    dpp_neighbor_report_data_t      results;
    uint32_t                        neighbor_qty;

    /* Reporting start timestamp used for reporting timestamp calculation */
    uint64_t                        report_ts;

    ds_dlist_node_t                 node;
} sm_neighbor_ctx_t;

/* The stats entry has per band (type) phy_name and scan_type context */
static ds_dlist_t                   g_neighbor_ctx_list=
                                            DS_DLIST_INIT(sm_neighbor_ctx_t,
                                                          node);

static inline sm_neighbor_ctx_t * sm_neighbor_ctx_alloc()
{
    sm_neighbor_ctx_t *neighbor_ctx = NULL;

    neighbor_ctx = malloc(sizeof(sm_neighbor_ctx_t));
    if (neighbor_ctx) {
        memset(neighbor_ctx, 0, sizeof(sm_neighbor_ctx_t));
    }

    return neighbor_ctx;
}

static inline void sm_neighbor_ctx_free(sm_neighbor_ctx_t *neighbor_ctx)
{
    if (NULL != neighbor_ctx) {
        free(neighbor_ctx);
    }
}

static
sm_neighbor_ctx_t *sm_neighbor_ctx_get (
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type)
{
    sm_neighbor_ctx_t              *neighbor_ctx = NULL;
    ds_dlist_iter_t                 ctx_iter;

    radio_entry_t                  *radio_entry = NULL;

    /* Find per radio neighbor ctx */
    for (   neighbor_ctx = ds_dlist_ifirst(&ctx_iter,&g_neighbor_ctx_list);
            neighbor_ctx != NULL;
            neighbor_ctx = ds_dlist_inext(&ctx_iter))
    {
        radio_entry = neighbor_ctx->radio_cfg;

        /* The stats entry has per band (type) and phy_name context */
        if (    (radio_cfg->type == radio_entry->type)
             && (scan_type == neighbor_ctx->scan_type)
		   )
        {
            LOG(TRACE,
                "Fetched %s %s neighbor reporting context",
                radio_get_name_from_cfg(radio_entry),
                radio_get_scan_name_from_type(scan_type));
            return neighbor_ctx;
        }
    }

    /* No neighbor ctx found create new */
    neighbor_ctx = NULL;
    neighbor_ctx = sm_neighbor_ctx_alloc();
    if(neighbor_ctx) {
        neighbor_ctx->scan_type = scan_type;
        neighbor_ctx->radio_cfg = radio_cfg;
        ds_dlist_insert_tail(&g_neighbor_ctx_list, neighbor_ctx);
        LOG(TRACE,
            "Created %s %s neighbor reporting context",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));
    }

    return neighbor_ctx;
}

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static
bool sm_neighbor_update_timer_set(
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
bool sm_neighbor_report_timer_set(
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
bool sm_neighbor_report_timer_restart(
        ev_timer                   *timer)
{
    sm_neighbor_ctx_t              *neighbor_ctx =
        (sm_neighbor_ctx_t *) timer->data;
    sm_stats_request_t             *request_ctx =
            &neighbor_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        neighbor_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        neighbor_ctx->scan_type;

    if (request_ctx->reporting_count) {
        request_ctx->reporting_count--;

        LOG(DEBUG,
            "Updated %s %s neighbor reporting count=%d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            request_ctx->reporting_count);

        /* If reporting_count becomes zero, then stop reporting */
        if (0 == request_ctx->reporting_count) {
            sm_neighbor_report_timer_set(timer, false);
            if (request_ctx->sampling_interval) {
                sm_neighbor_update_timer_set(
                        &neighbor_ctx->update_timer, false);
            }

            LOG(DEBUG,
                "Stopped %s %s neighbor reporting (count expired)",
                radio_get_name_from_cfg(radio_cfg_ctx),
                radio_get_scan_name_from_type(scan_type));
            return true;
        }
    }

    return true;
}

static
bool sm_neighbor_results_clear(
        sm_neighbor_ctx_t          *neighbor_ctx,
        dpp_neighbor_list_t        *neighbor_list)
{
    dpp_neighbor_record_list_t     *neighbor = NULL;
    ds_dlist_iter_t                 neighbor_iter;

    for (   neighbor = ds_dlist_ifirst(&neighbor_iter, neighbor_list);
            neighbor != NULL;
            neighbor = ds_dlist_inext(&neighbor_iter)) {
        ds_dlist_iremove(&neighbor_iter);
        dpp_neighbor_record_free(neighbor);
        neighbor = NULL;
    }

    return true;
}

static
bool sm_neighbor_report_send_diff(
        sm_neighbor_ctx_t          *neighbor_ctx)
{
    bool                            status;
    dpp_neighbor_report_data_t     *report_ctx =
        &neighbor_ctx->report;
    dpp_neighbor_list_t            *neighbor_list =
        &report_ctx->list;
    sm_stats_request_t             *request_ctx =
        &neighbor_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        neighbor_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        neighbor_ctx->scan_type;

    uint32_t                        found = 0;

    /* Create new report for diff data (only add/remove 
       compared to previous report)
     */
    dpp_neighbor_report_data_t      report_diff;
    memset(&report_diff, 0, sizeof(report_diff));
    ds_dlist_init(
            &report_diff.list,
            dpp_neighbor_record_list_t,
            node);

    report_diff.radio_type  = radio_cfg_ctx->type;
    report_diff.scan_type   = scan_type;
    report_diff.report_type = request_ctx->report_type;

    /* Report_timestamp is base-timestamp + relative start time offset */
    report_diff.timestamp_ms =
        request_ctx->reporting_timestamp - neighbor_ctx->report_ts +
        get_timestamp();

    /* Compose new reprot from collected values and cache */
    dpp_neighbor_record_list_t     *neighbor = NULL;
    ds_dlist_iter_t                 neighbor_iter;
    dpp_neighbor_record_t          *neighbor_entry = NULL;

    dpp_neighbor_record_list_t     *cache = NULL;
    ds_dlist_iter_t                 cache_iter;
    dpp_neighbor_record_t          *cache_entry = NULL;

    dpp_neighbor_record_list_t     *diff = NULL;
    dpp_neighbor_record_t          *diff_entry = NULL;

    /* Check for removed entries */
    for (   cache = ds_dlist_ifirst(&cache_iter, &neighbor_ctx->diff_cache);
            cache != NULL;
            cache = ds_dlist_inext(&cache_iter))
    {
        cache_entry = &cache->entry;
        found = false;

        /* Search for existing entry in cache */
        for (   neighbor = ds_dlist_ifirst(&neighbor_iter, neighbor_list);
                neighbor != NULL;
                neighbor = ds_dlist_inext(&neighbor_iter))
        {
            neighbor_entry = &neighbor->entry;
            if (!strcmp (cache_entry->bssid, neighbor_entry->bssid)) {
                found = true;
                break;
            }
        }

        /* Mark entry removed */
        if (!found) {
            diff =
                dpp_neighbor_record_alloc();
            if (NULL == diff) {
                LOGE("Processing %s %s neighbor diff- report "
                        "(Failed to allocate memmory)",
                        radio_get_name_from_cfg(radio_cfg_ctx),
                        radio_get_scan_name_from_type(scan_type));
                goto clear;
            }
            diff_entry = &diff->entry;

            /* Mark entry expired */
            cache_entry->lastseen = 0;

            memcpy (diff_entry,
                    cache_entry,
                    sizeof (dpp_neighbor_record_t));

            LOGT("Sending %s %s neighbor diff- {bssid='%s' ssid='%s' rssi=%d chan=%d}\n",
                    radio_get_name_from_cfg(radio_cfg_ctx),
                    radio_get_scan_name_from_type(scan_type),
                    diff_entry->bssid,
                    diff_entry->ssid,
                    diff_entry->sig,
                    diff_entry->chan);

            ds_dlist_insert_tail(&report_diff.list, diff);
        }
    }

    /* Check for new entries */
    for (   neighbor = ds_dlist_ifirst(&neighbor_iter, neighbor_list);
            neighbor != NULL;
            neighbor = ds_dlist_inext(&neighbor_iter))
    {
        neighbor_entry = &neighbor->entry;
        found = false;

        /* Search for existing entry in cache */
        for (   cache = ds_dlist_ifirst(&cache_iter, &neighbor_ctx->diff_cache);
                cache != NULL;
                cache = ds_dlist_inext(&cache_iter))
        {
            cache_entry = &cache->entry;
            if (!strcmp (cache_entry->bssid, neighbor_entry->bssid)) {
                found = true;
                break;
            }
        }

        /* Mark entry added */
        if (!found) {
            diff =
                dpp_neighbor_record_alloc();
            if (NULL == diff) {
                LOG(ERR,
                        "Processing %s %s neighbor diff+ report "
                        "(Failed to allocate memmory)",
                        radio_get_name_from_cfg(radio_cfg_ctx),
                        radio_get_scan_name_from_type(scan_type));
                goto clear;
            }
            diff_entry = &diff->entry;

            memcpy (diff_entry,
                    neighbor_entry,
                    sizeof (dpp_neighbor_record_t));

            LOG(TRACE,
                "Sending %s %s neighbor diff+ {bssid='%s' ssid='%s' rssi=%d chan=%d}\n",
                radio_get_name_from_cfg(radio_cfg_ctx),
                radio_get_scan_name_from_type(scan_type),
                diff_entry->bssid,
                diff_entry->ssid,
                diff_entry->sig,
                diff_entry->chan);

            ds_dlist_insert_tail(&report_diff.list, diff);
        }
    }

    LOGI("Sending %s %s neighbor report at '%s'",
         radio_get_name_from_cfg(radio_cfg_ctx),
         radio_get_scan_name_from_type(scan_type),
         sm_timestamp_ms_to_date(report_diff.timestamp_ms));

    /* Send records to MQTT FIFO (Skip empty reports) */
    if(!ds_dlist_is_empty(&report_diff.list)) {
        dpp_put_neighbor(&report_diff);
    }

    /* Clear previous cache */
    status =
        sm_neighbor_results_clear(
                neighbor_ctx,
                &neighbor_ctx->diff_cache);
    if (true != status) {
        goto clear;
    }

    /* Update neighbor cache */
    for (   neighbor = ds_dlist_ifirst(&neighbor_iter, neighbor_list);
            neighbor != NULL;
            neighbor = ds_dlist_inext(&neighbor_iter))
    {
        neighbor_entry = &neighbor->entry;

        cache =
            dpp_neighbor_record_alloc();
        if (NULL == cache) {
            LOGE("Processing %s %s neighbor diff report "
                 "(Failed to allocate memmory)",
                 radio_get_name_from_cfg(radio_cfg_ctx),
                 radio_get_scan_name_from_type(scan_type));
            goto clear;
        }
        cache_entry = &cache->entry;

        memcpy (cache_entry,
                neighbor_entry,
                sizeof (dpp_neighbor_record_t));

        ds_dlist_insert_tail(&neighbor_ctx->diff_cache, cache);
    }

clear:
    status =
        sm_neighbor_results_clear(
                neighbor_ctx,
                neighbor_list);
    if (true != status) {
        return false;
    }

    status =
        sm_neighbor_results_clear(
                neighbor_ctx,
                &report_diff.list);
    if (true != status) {
        return false;
    }

    neighbor_ctx->neighbor_qty = 0;

    SM_SANITY_CHECK_TIME(report_ctx->timestamp_ms,
                         &request_ctx->reporting_timestamp,
                         &neighbor_ctx->report_ts);
    return true;
}

static
bool sm_neighbor_report_send_raw(
        sm_neighbor_ctx_t          *neighbor_ctx)
{
    bool                            status;
    dpp_neighbor_report_data_t     *report_ctx =
        &neighbor_ctx->report;
    dpp_neighbor_list_t            *neighbor_list =
        &report_ctx->list;
    sm_stats_request_t             *request_ctx =
        &neighbor_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        neighbor_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        neighbor_ctx->scan_type;

    report_ctx->radio_type = radio_cfg_ctx->type;
    report_ctx->scan_type = scan_type;
    report_ctx->report_type = request_ctx->report_type;

    /* Report_timestamp is base-timestamp + relative start time offset */
    report_ctx->timestamp_ms =
        request_ctx->reporting_timestamp - neighbor_ctx->report_ts +
        get_timestamp();

    dpp_neighbor_record_list_t     *neighbor = NULL;
    ds_dlist_iter_t                 neighbor_iter;
    dpp_neighbor_record_t          *neighbor_entry = NULL;

    for (   neighbor = ds_dlist_ifirst(&neighbor_iter, neighbor_list);
            neighbor != NULL;
            neighbor = ds_dlist_inext(&neighbor_iter))
    {
        neighbor_entry = &neighbor->entry;

        LOGD("Sending %s %s neighbors {bssid='%s' ssid='%s' rssi=%d chan=%d}\n",
             radio_get_name_from_cfg(radio_cfg_ctx),
             radio_get_scan_name_from_type(scan_type),
             neighbor_entry->bssid,
             neighbor_entry->ssid,
             neighbor_entry->sig,
             neighbor_entry->chan);
    }

    LOGI("Sending %s %s neighbor report at '%s'",
         radio_get_name_from_cfg(radio_cfg_ctx),
         radio_get_scan_name_from_type(scan_type),
         sm_timestamp_ms_to_date(report_ctx->timestamp_ms));

    /* Send records to MQTT FIFO (Skip empty reports) */
    if(!ds_dlist_is_empty(neighbor_list)) {
        dpp_put_neighbor(report_ctx);
    }

    status =
        sm_neighbor_results_clear(
                neighbor_ctx,
                neighbor_list);
    if (true != status) {
        return false;
    }

    neighbor_ctx->neighbor_qty = 0;


    SM_SANITY_CHECK_TIME(report_ctx->timestamp_ms,
                         &request_ctx->reporting_timestamp,
                         &neighbor_ctx->report_ts);
    return true;
}

static
bool sm_neighbor_report_send(
        sm_neighbor_ctx_t          *neighbor_ctx)
{
    bool                            status = false;
    sm_stats_request_t             *request_ctx =
        &neighbor_ctx->request;
    ev_timer                       *report_timer =
        &neighbor_ctx->report_timer;

    /* Restart timer */
    sm_neighbor_report_timer_restart(report_timer);

    /* Send only changes (This preserves neighbor cache) */
    if (REPORT_TYPE_DIFF == request_ctx->report_type) {
        status = sm_neighbor_report_send_diff(neighbor_ctx);
    } else {
        status = sm_neighbor_report_send_raw(neighbor_ctx);
    }

    return status;
}

static
void sm_neighbor_stats_results(
        void                       *scan_ctx,
        int                         scan_status)
{
    bool                            status;
    sm_neighbor_ctx_t              *neighbor_ctx = NULL;
    dpp_neighbor_report_data_t     *report_ctx = NULL;
    dpp_neighbor_report_data_t     *results_ctx = NULL;
    sm_chan_list_t                 *channel_ctx = NULL;
    radio_entry_t                  *radio_cfg_ctx = NULL;

    bool                            rc;
    dpp_neighbor_list_t            *scan_list = NULL;
    dpp_neighbor_record_list_t     *scan = NULL;
    dpp_neighbor_record_t          *scan_entry = NULL;
    ds_dlist_iter_t                 scan_iter;
    uint32_t                        scan_qty = 0;
    uint32_t                       *scan_chan;
    uint32_t                        scan_num;
    radio_scan_type_t               scan_type;

    dpp_neighbor_list_t            *neighbor_list = NULL;
    dpp_neighbor_record_list_t     *neighbor = NULL;
    dpp_neighbor_record_t          *neighbor_entry = NULL;
    ds_dlist_iter_t                 neighbor_iter;

    mac_address_t                   mac;
    uint32_t                        found = 0;

    if (NULL == scan_ctx) {
        return;
    }

    neighbor_ctx    = (sm_neighbor_ctx_t *) scan_ctx;
    report_ctx      = &neighbor_ctx->report;
    radio_cfg_ctx   = neighbor_ctx->radio_cfg;
    channel_ctx     = &neighbor_ctx->chan_list;
    results_ctx     = &neighbor_ctx->results;
    scan_list       = &results_ctx->list;
    neighbor_list   = &report_ctx->list;
    scan_type       = neighbor_ctx->scan_type;

    /* Prepare channels for which we need results */
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

    /* Scan error occurred skip results and retry */
    if (!scan_status) {
        LOG(ERR,
            "Processing %s %s %d neighbor report "
            "(Failed to scan)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            *scan_chan);
        goto clear;
    }

    rc =
        target_stats_scan_get (
                radio_cfg_ctx,
                scan_chan,
                scan_num,
                scan_type,
                results_ctx);
    if (true != rc) {
        LOG(ERR,
            "Processing %s %s %d neighbor reprot ",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            *scan_chan);
        goto clear;
    }

    /* Loop through scan results store them in report */
    for (   scan = ds_dlist_ifirst(&scan_iter, scan_list);
            scan != NULL;
            scan = ds_dlist_inext(&scan_iter))
    {
        scan_entry = &scan->entry;
        found = false;

        /* Filter entries from non scanned channel (some targets return
           cached entries also for non scanned channels). Only valid for
           on and off channel scans.
         */
        if (scan_type != RADIO_SCAN_TYPE_FULL) {
            if (scan_entry->chan != *scan_chan) {
                continue;
            }
        }

        /* Filter neighbor with 0 SNR */
        if (scan_entry->sig == 0) {
            LOG(TRACE,
                "Remove %s %s neighbor due to signal {bssid='%s' ssid='%s' rssi=%d chan=%d}\n",
                radio_get_name_from_cfg(radio_cfg_ctx),
                radio_get_scan_name_from_type(scan_type),
                scan_entry->bssid,
                scan_entry->ssid,
                scan_entry->sig,
                scan_entry->chan);
            continue;
        }

        os_nif_macaddr_from_str(
                (void*)&mac,
                scan_entry->bssid);

        /* Update RSSI reporting every sampling interval if enabled */
        if(sm_rssi_is_reporting_enabled(radio_cfg_ctx)) {
            status =
                sm_rssi_stats_results_update (
                        radio_cfg_ctx,
                        mac,
                        scan_entry->sig,
                        0, // rx_ppdus
                        0, // tx_ppdus
                        RSSI_SOURCE_NEIGHBOR);
            if (true != status) {
                LOG(ERR,
                        "Updating %s interface client stats "
                        "(Failed to update RSSI data)",
                        radio_get_name_from_cfg(radio_cfg_ctx));
            }
        }

        /* Search for existing entry it */
        for (   neighbor = ds_dlist_ifirst(&neighbor_iter, neighbor_list);
                neighbor != NULL;
                neighbor = ds_dlist_inext(&neighbor_iter))
        {
            neighbor_entry = &neighbor->entry;
            if (!strcmp (scan_entry->bssid, neighbor_entry->bssid)) {
                /* Update with latest value */
                memcpy (neighbor_entry,
                        scan_entry,
                        sizeof (dpp_neighbor_record_t));
                found = true;
                break;
            }
        }

        /* Add new entry to the end */
        if (!found) {
            neighbor =
                dpp_neighbor_record_alloc();
            if (NULL == neighbor) {
                LOG(ERR,
                    "Processing %s %s neighbor report "
                    "(Failed to allocate memmory)",
                    radio_get_name_from_cfg(radio_cfg_ctx),
                    radio_get_scan_name_from_type(scan_type));
                goto clear;
            }
            neighbor_entry = &neighbor->entry;

            memcpy (neighbor_entry,
                    scan_entry,
                    sizeof (dpp_neighbor_record_t));

            LOG(TRACE,
                "Adding %s %s neighbor {bssid='%s' ssid='%s' rssi=%d chan=%d}\n",
                radio_get_name_from_cfg(radio_cfg_ctx),
                radio_get_scan_name_from_type(scan_type),
                neighbor_entry->bssid,
                neighbor_entry->ssid,
                neighbor_entry->sig,
                neighbor_entry->chan);

            ds_dlist_insert_tail(neighbor_list, neighbor);

            scan_qty++;
            neighbor_ctx->neighbor_qty++;
        }
    }

    LOG(DEBUG,
        "Processed %s %s scan records new %d total %d",
        radio_get_name_from_cfg(radio_cfg_ctx),
        radio_get_scan_name_from_type(scan_type),
        scan_qty,
        neighbor_ctx->neighbor_qty);

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
            sm_neighbor_report_send(neighbor_ctx);
            break;
        default:
            goto clear;
            break;
    }

clear:
    sm_neighbor_results_clear(
            neighbor_ctx,
            scan_list);
}

static
void sm_neighbor_update (EV_P_ ev_timer *w, int revents)
{
    bool                            rc;

    sm_neighbor_ctx_t              *neighbor_ctx =
        (sm_neighbor_ctx_t *) w->data;
    sm_stats_request_t             *request_ctx =
        &neighbor_ctx->request;
    sm_chan_list_t                 *channel_ctx =
        &neighbor_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        neighbor_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        neighbor_ctx->scan_type;
    uint32_t                       *scan_chan;
    uint32_t                        scan_num;
    uint32_t                        scan_interval = request_ctx->scan_interval;

    /* Check if radio is exists */
    if (target_is_radio_interface_ready(radio_cfg_ctx->phy_name) != true) {
        LOG(TRACE,
            "Skip processintg %s %s neighbor report "
            "(Radio %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            radio_cfg_ctx->phy_name);
        return;
    }

    /* Check if vif interface exists */
    if (target_is_interface_ready(radio_cfg_ctx->if_name) != true) {
        LOG(TRACE,
            "Skip processing %s %s neighbor report "
            "(Interface %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            radio_cfg_ctx->if_name);
        return;
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
            return;
    }

    LOG(DEBUG,
        "Processing %s %s neighbor chan=%d",
        radio_get_name_from_cfg(radio_cfg_ctx),
        radio_get_scan_name_from_type(scan_type),
        *scan_chan);

    sm_scan_request_t           scan_request;
    memset(&scan_request, 0, sizeof(scan_request));
    scan_request.radio_cfg = radio_cfg_ctx;
    scan_request.chan_list = scan_chan;
    scan_request.chan_num = scan_num;
    scan_request.scan_type = scan_type;
    scan_request.dwell_time = scan_interval;
    scan_request.scan_cb = sm_neighbor_stats_results;

    rc = sm_scan_schedule(&scan_request);
    if (true != rc)
    {
        LOG(ERR,
            "Processing %s %s %d neighbors ",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            *scan_chan);
        return;
    }
}

static
void sm_neighbor_report (EV_P_ ev_timer *w, int revents)
{
    sm_neighbor_ctx_t               *neighbor_ctx =
        (sm_neighbor_ctx_t *) w->data;
    sm_stats_request_t              *request_ctx =
        &neighbor_ctx->request;
    radio_scan_type_t                scan_type =
       request_ctx->scan_type;

    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
            /* Send report */
            sm_neighbor_report_send(neighbor_ctx);
            break;
        case RADIO_SCAN_TYPE_FULL:
            sm_neighbor_update(EV_DEFAULT, w, revents);
        default:
            return;
    }
}

static
bool sm_neighbor_stats_chanlist_update (
        sm_neighbor_ctx_t          *neighbor_ctx)
{
    sm_chan_list_t                 *channel_ctx =
        &neighbor_ctx->chan_list;
    sm_chan_list_t                 *channel_cfg =
        &neighbor_ctx->request.radio_chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        neighbor_ctx->radio_cfg;
    radio_scan_type_t               scan_type =
        neighbor_ctx->scan_type;
    uint32_t                        chan_index = 0;

    memset(channel_ctx, 0, sizeof(sm_chan_list_t));

    if (scan_type == RADIO_SCAN_TYPE_ONCHAN)
    {
        channel_ctx->chan_list[channel_ctx->chan_num++] =
            radio_cfg_ctx->chan;
        LOG(DEBUG,
            "Updated %s %s neighbor chan %d",
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
                "Skip updating %s %s neighbor chan %d "
                "(on-chan specified)",
                radio_get_name_from_cfg(radio_cfg_ctx),
                radio_get_scan_name_from_type(scan_type),
                channel_cfg->chan_list[chan_index]);
            continue;
        }

        channel_ctx->chan_list[channel_ctx->chan_num++] =
            channel_cfg->chan_list[chan_index];

        LOG(DEBUG,
            "Updated %s %s neighbor chan %d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_get_scan_name_from_type(scan_type),
            channel_ctx->chan_list[channel_ctx->chan_num - 1]);
    }

    return true;
}

static
bool sm_neighbor_stats_init (
        sm_neighbor_ctx_t          *neighbor_ctx)
{
    bool                            status;

    dpp_neighbor_report_data_t     *report_ctx =
        &neighbor_ctx->report;
    dpp_neighbor_list_t            *neighbor_list =
        &report_ctx->list;

    dpp_neighbor_report_data_t     *results_ctx =
        &neighbor_ctx->results;
    dpp_neighbor_list_t            *results_list =
        &results_ctx->list;

    status =
        sm_neighbor_results_clear(
                neighbor_ctx,
                neighbor_list);
    if (true != status)
    {
        return false;
    }

    status =
        sm_neighbor_results_clear(
                neighbor_ctx,
                results_list);
    if (true != status)
    {
        return false;
    }

    status =
        sm_neighbor_results_clear(
                neighbor_ctx,
                &neighbor_ctx->diff_cache);
    if (true != status) {
        return false;
    }

    return true;
}

static
bool sm_neighbor_stats_process (
        radio_entry_t              *radio_cfg, // For logging
        radio_scan_type_t           scan_type,
        sm_neighbor_ctx_t          *neighbor_ctx)
{
    bool                            status;

    sm_stats_request_t             *request_ctx =
        &neighbor_ctx->request;
    sm_chan_list_t                 *channel_ctx =
        &neighbor_ctx->chan_list;
    radio_entry_t                  *radio_cfg_ctx =
        neighbor_ctx->radio_cfg;
    ev_timer                       *update_timer =
        &neighbor_ctx->update_timer;
    ev_timer                       *report_timer =
        &neighbor_ctx->report_timer;

    /* Skip on-chan neighbor report start if radio is not configured */
    if (!radio_cfg_ctx) {
        LOG(TRACE,
            "Skip starting %s %s neighbor reporting "
            "(Radio not configured)",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));
        return true;
    }

    /* Skip on-chan neighbor report start if stats are not configured */
    if (!neighbor_ctx->initialized) {
        LOG(TRACE,
            "Skip starting %s %s neighbor reporting "
            "(Stats not configured)",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));
        return true;
    }

    /* Stop when reconfiguration is detected */
    if (RADIO_SCAN_TYPE_FULL != scan_type) {
        sm_neighbor_update_timer_set(update_timer, false);
    }
    sm_neighbor_report_timer_set(report_timer, false);

    /* Skip on-chan neighbor report start if timestamp is not specified */
    if (!request_ctx->reporting_timestamp) {
        LOG(TRACE,
            "Skip starting %s %s neighbor reporting "
            "(Timestamp not configured)",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));
        return true;
    }

    /* Update list and consider on-channel */
    status =
        sm_neighbor_stats_chanlist_update (
                neighbor_ctx);
    if (true != status) {
        return false;
    }

    /* No channels no neighbor */
    if (!channel_ctx->chan_num) {
        LOG(ERR,
            "Starting %s %s neighbor reporting "
            "(No channels)",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));
        return false;
    }

    /* Stop any target scan processing */
    sm_scan_schedule_stop(radio_cfg, scan_type);

    if (request_ctx->reporting_interval) {
        status =
            sm_neighbor_stats_init(
                    neighbor_ctx);
        if (true != status) {
            return false;
        }

        if (request_ctx->sampling_interval) {
            update_timer->repeat = request_ctx->sampling_interval;
            sm_neighbor_update_timer_set(update_timer, true);
        }

        report_timer->repeat = request_ctx->reporting_interval;
        sm_neighbor_report_timer_set(report_timer, true);
        neighbor_ctx->report_ts = get_timestamp();

        LOG(INFO,
            "Started %s %s neighbor reporting",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));
    }
    else {
        LOG(INFO,
            "Stopped %s %s neighbor reporting",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));

        memset(request_ctx, 0, sizeof(sm_stats_request_t));
    }

    return true;
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
bool sm_neighbor_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request)
{
    bool                            status;

    sm_neighbor_ctx_t              *neighbor_ctx = NULL;
    sm_stats_request_t             *request_ctx = NULL;
    dpp_neighbor_report_data_t     *report_ctx = NULL;
    ev_timer                       *update_timer = NULL;
    ev_timer                       *report_timer = NULL;
    radio_scan_type_t               scan_type;

    if (NULL == request) {
        LOG(ERR,
            "Initializing neighbor reporting "
            "(Invalid request config)");
        return false;
    }
    scan_type = request->scan_type;

    neighbor_ctx    = sm_neighbor_ctx_get(radio_cfg, scan_type);
    request_ctx     = &neighbor_ctx->request;
    report_ctx      = &neighbor_ctx->report;
    update_timer    = &neighbor_ctx->update_timer;
    report_timer    = &neighbor_ctx->report_timer;

    /* Initialize global storage and timer config only once */
    if (!neighbor_ctx->initialized) {
        memset(request_ctx, 0, sizeof(*request_ctx));
        memset(report_ctx, 0, sizeof(*report_ctx));

        LOG(INFO,
            "Initializing %s %s neighbor reporting",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));

        /* Initialize neighbor list */
        ds_dlist_init(
                &report_ctx->list,
                dpp_neighbor_record_list_t,
                node);

        /* Initialize scan result storage list */
        ds_dlist_init(
                &neighbor_ctx->results.list,
                dpp_neighbor_record_list_t,
                node);

        /* Initialize delta neighbor list */
        ds_dlist_init(
                &neighbor_ctx->diff_cache,
                dpp_neighbor_record_list_t,
                node);

        if (RADIO_SCAN_TYPE_FULL != scan_type) {
            ev_init (update_timer, sm_neighbor_update);
            update_timer->data = neighbor_ctx;
        }

        ev_init (report_timer, sm_neighbor_report);
        report_timer->data = neighbor_ctx;

        neighbor_ctx->initialized = true;
    }

    /* Store and compare every request parameter ...
       memcpy would be easier but we want some debug info
     */
    char param_str[32];
    sprintf(param_str,
            "%s %s",
            radio_get_scan_name_from_type(scan_type),
            "neighbor");
    REQUEST_PARAM_UPDATE(param_str, scan_type, "%d");
    REQUEST_PARAM_UPDATE(param_str, radio_type, "%d");
    REQUEST_PARAM_UPDATE(param_str, report_type, "%d");
    REQUEST_PARAM_UPDATE(param_str, reporting_count, "%d");
    REQUEST_PARAM_UPDATE(param_str, scan_interval, "%d");
    REQUEST_PARAM_UPDATE(param_str, reporting_interval, "%d");
    REQUEST_PARAM_UPDATE(param_str, sampling_interval, "%d");
    REQUEST_PARAM_UPDATE(param_str, reporting_timestamp, "%"PRIu64"");

    memcpy(&request_ctx->radio_chan_list,
            &request->radio_chan_list,
            sizeof (request_ctx->radio_chan_list));

    status =
        sm_neighbor_stats_process (
                radio_cfg,
                scan_type,
                neighbor_ctx);
    if (true != status) {
        return false;
    }

    return true;
}

bool sm_neighbor_report_radio_change(
        radio_entry_t              *radio_cfg)
{
    bool                            status;
    sm_neighbor_ctx_t              *neighbor_ctx = NULL;
    radio_scan_type_t               scan_type;
    int                             scan_index;

    if (NULL == radio_cfg) {
        LOG(ERR,
            "Changing neighbor reporting "
            "(Invalid radio config)");
        return false;
    }

    /* Update radio on all scan contexts and if initialized, the
       reports will start. This is not ideal but needed to
       simplify the design (since all report data is allocated
       preallocation does not consume much memory)
     */
    for (   scan_index = 0;
            scan_index < RADIO_SCAN_MAX_TYPE_QTY;
            scan_index++
        ) {
        scan_type       = radio_get_scan_type_from_index(scan_index);
        neighbor_ctx    = sm_neighbor_ctx_get(radio_cfg, scan_type);
        if (NULL == neighbor_ctx) {
            LOGE("Changing neighbor reporting "
                 "(Invalid neighbor context)");
            return false;
        }

        status =
            sm_neighbor_stats_process (
                    radio_cfg,
                    scan_type,
                    neighbor_ctx);
        if (true != status) {
            return false;
        }
    }

    return true;
}

bool sm_neighbor_stats_results_update(
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type,
        int                         scan_status)
{
    sm_neighbor_ctx_t              *neighbor_ctx = NULL;
    sm_stats_request_t             *request_ctx = NULL;

    if (NULL == radio_cfg) {
        LOG(ERR,
            "Initializing neighbor reporting "
            "(Invalid radio config)");
        return false;
    }

    neighbor_ctx = sm_neighbor_ctx_get(radio_cfg, scan_type);
    request_ctx  = &neighbor_ctx->request;

    if (!request_ctx->reporting_interval) {
        LOG(TRACE,
            "Skip processing %s %s neighbor report through survey "
            "(neighbor reporting not configured)",
            radio_get_name_from_cfg(radio_cfg),
            radio_get_scan_name_from_type(scan_type));
        return true;
    }
    else {
        if (request_ctx->sampling_interval && request_ctx->scan_interval) {
            LOG(TRACE,
                "Skip processing %s %s neighbor report through survey "
                "(neighbor report sampling started)",
                radio_get_name_from_cfg(radio_cfg),
                radio_get_scan_name_from_type(scan_type));
            return true;
        }
    }
    LOG(DEBUG,
        "Processing %s %s neighbor report through survey",
        radio_get_name_from_cfg(radio_cfg),
        radio_get_scan_name_from_type(scan_type));

    sm_neighbor_stats_results(neighbor_ctx, scan_status);

    return true;
}
