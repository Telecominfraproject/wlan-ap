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

#define sm_client_report_stat_percent_get(v1, v2) \
    ((v2 > 0 && v1 < v2) ? (v1*100/v2) : 0)

#define sm_client_report_stat_delta(n, o) ((n) - (o))

typedef struct
{
    dpp_client_record_t             entry;
    ds_dlist_t                      result_list;
    target_client_record_t          cache;
    ds_dlist_node_t                 node;
} sm_client_record_t;

static inline sm_client_record_t * sm_client_record_alloc()
{
    sm_client_record_t *record = NULL;

    record = malloc(sizeof(sm_client_record_t));
    if (record) {
        memset(record, 0, sizeof(sm_client_record_t));
    }

    return record;
}

static inline void sm_client_record_free(sm_client_record_t *record)
{
    if (NULL != record) {
        free(record);
    }
}

#define CLIENT_INIT_TIME            20 /* s */

/* new part */
typedef struct
{
    bool                            initialized;

    /* Internal structure used to lower layer radio selection */
    radio_entry_t                  *radio_cfg;

    /* Internal structure to store report timers */
    ev_timer                        report_timer;
    ev_timer                        update_timer;
    ev_timer                        init_timer;

    /* Structure containing cloud request timer params */
    sm_stats_request_t              request;
    /* Structure pointing to upper layer client storage */
    dpp_client_report_data_t        report;

    /* Structure containing cached client sampling records
       (sm_client_record_t) */
    ds_dlist_t                      record_list;
    uint32_t                        record_qty;

    /* target client temporary list for deriving records */
    ds_dlist_t                      client_list;

    /* Reporting start timestamp used for client duration calculation */
    uint64_t                        duration_ts;
    /* Reporting start timestamp used for reporting timestamp calculation */
    uint64_t                        report_ts;

    ds_dlist_node_t                 node;
} sm_client_ctx_t;

/* The stats entry has per band (type) and phy_name context */
static ds_dlist_t                   g_client_ctx_list =
                                        DS_DLIST_INIT(sm_client_ctx_t,
                                                      node);

static inline sm_client_ctx_t * sm_client_ctx_alloc()
{
    sm_client_ctx_t *client_ctx = NULL;

    client_ctx = malloc(sizeof(sm_client_ctx_t));
    if (client_ctx) {
        memset(client_ctx, 0, sizeof(sm_client_ctx_t));
    }

    return client_ctx;
}

static inline void sm_client_ctx_free(sm_client_ctx_t *client_ctx)
{
    if (NULL != client_ctx) {
        free(client_ctx);
    }
}

static
sm_client_ctx_t *sm_client_ctx_get (
        radio_entry_t              *radio_cfg)
{
    sm_client_ctx_t                *client_ctx = NULL;
    ds_dlist_iter_t                 ctx_iter;

    radio_entry_t                  *radio_entry = NULL;

    /* Find per radio client ctx  */
    for (   client_ctx = ds_dlist_ifirst(&ctx_iter,&g_client_ctx_list);
            client_ctx != NULL;
            client_ctx = ds_dlist_inext(&ctx_iter))
    {
        radio_entry = client_ctx->radio_cfg;

        /* The stats entry has per band (type) and phy_name context */
        if (radio_cfg->type == radio_entry->type)
        {
            LOG(TRACE,
                "Fetched %s client reporting context",
                radio_get_name_from_cfg(radio_entry));
            return client_ctx;
        }
    }

    /* No client ctx found create new ... */
    client_ctx = NULL;
    client_ctx = sm_client_ctx_alloc();
    if(client_ctx) {
        client_ctx->radio_cfg = radio_cfg;
        ds_dlist_insert_tail(&g_client_ctx_list, client_ctx);
        LOG(TRACE,
            "Created %s client reporting context",
            radio_get_name_from_cfg(radio_cfg));
    }

    return client_ctx;
}

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static
bool sm_client_timer_set(
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
bool sm_client_report_timer_restart(
        ev_timer                   *timer)
{
    sm_client_ctx_t                *client_ctx =
        (sm_client_ctx_t *) timer->data;
    sm_stats_request_t             *request_ctx =
            &client_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    if (request_ctx->reporting_count) {
        request_ctx->reporting_count--;

        LOG(DEBUG,
            "Updated %s client reporting count=%d",
            radio_get_name_from_cfg(radio_cfg_ctx),
            request_ctx->reporting_count);

        /* If reporting_count becomes zero, then stop reporting */
        if (0 == request_ctx->reporting_count) {
            sm_client_timer_set(timer, false);
            sm_client_timer_set(&client_ctx->update_timer, false);

            LOG(DEBUG,
                "Stopped %s client reporting (count expired)",
                radio_get_name_from_cfg(radio_cfg_ctx));
            return true;
        }
    }

    return true;
}

static
bool dpp_client_stats_rx_records_clear(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *stats_rx_list)
{
    dpp_client_stats_rx_t          *record = NULL;
    ds_dlist_iter_t                 record_iter;

    for (   record = ds_dlist_ifirst(&record_iter, stats_rx_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        ds_dlist_iremove(&record_iter);
        dpp_client_stats_rx_record_free(record);
        record = NULL;
    }

    return true;
}

static
bool dpp_client_stats_tx_records_clear(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *stats_tx_list)
{
    dpp_client_stats_tx_t          *record = NULL;
    ds_dlist_iter_t                 record_iter;

    for (   record = ds_dlist_ifirst(&record_iter, stats_tx_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
   {
        ds_dlist_iremove(&record_iter);
        dpp_client_stats_tx_record_free(record);
        record = NULL;
    }

    return true;
}

static
bool dpp_client_tid_records_clear(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *tid_record_list)
{
    dpp_client_tid_record_list_t   *record = NULL;
    ds_dlist_iter_t                 record_iter;

    for (   record = ds_dlist_ifirst(&record_iter, tid_record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        ds_dlist_iremove(&record_iter);
        dpp_client_tid_record_free(record);
        record = NULL;
    }

    return true;
}

static
bool sm_client_record_clear(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *record_list)
{
    dpp_client_record_t            *record = NULL;
    ds_dlist_iter_t                 record_iter;

    for (   record = ds_dlist_ifirst(&record_iter, record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        dpp_client_stats_rx_records_clear(
                client_ctx,
                &record->stats_rx);
        dpp_client_stats_tx_records_clear(
                client_ctx,
                &record->stats_tx);
        dpp_client_tid_records_clear(
                client_ctx,
                &record->tid_record_list);

        ds_dlist_iremove(&record_iter);
        dpp_client_record_free(record);
        record = NULL;
    }

    return true;
}

static
bool sm_client_sm_records_clear(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *record_list)
{
    sm_client_record_t             *record = NULL;
    ds_dlist_iter_t                 record_iter;

    for (   record = ds_dlist_ifirst(&record_iter, record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        sm_client_record_clear(
                client_ctx,
                &record->result_list);

        ds_dlist_iremove(&record_iter);
        sm_client_record_free(record);
        record = NULL;
    }

    return true;
}

static
bool sm_client_target_clear(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *client_list)
{
    target_client_record_t         *client = NULL;
    ds_dlist_iter_t                 client_iter;

    for (   client = ds_dlist_ifirst(&client_iter, client_list);
            client != NULL;
            client = ds_dlist_inext(&client_iter))
    {
        ds_dlist_iremove(&client_iter);
        target_client_record_free(client);
        client = NULL;
    }

    return true;
}

static
sm_client_record_t *sm_client_records_mac_find(
        sm_client_ctx_t            *client_ctx,
        target_client_record_t     *client_entry)
{
    ds_dlist_t                     *record_list =
        &client_ctx->record_list;
    sm_client_record_t             *record = NULL;
    dpp_client_record_t            *record_entry = NULL;
    ds_dlist_iter_t                 record_iter;

    /* Find current client in existing list */
    for (   record = ds_dlist_ifirst(&record_iter, record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        record_entry = &record->entry;

        if (!memcmp(
                    record_entry->info.mac,
                    client_entry->info.mac,
                    sizeof(record_entry->info.mac))
             && (record_entry->info.type == client_entry->info.type)
           ) {
            return record;
        }
    }

    return NULL;
}

static
void sm_client_report_calculate_duration (
        sm_client_ctx_t            *client_ctx,
        uint64_t                    timestamp_ms,
        dpp_client_record_t        *record_entry,
        dpp_client_record_t        *report_entry)
{
    /* If duration equals 0 it means that the client was either
       connected through all report all was connected in between but
       not disconnected
     */
    if (!record_entry->duration_ms) {
        /* check if connection exist through whole report duration */
        if (!record_entry->connect_ts) {
            report_entry->duration_ms =
                sm_client_report_stat_delta(
                        timestamp_ms,
                        client_ctx->duration_ts);
        }
        else /* client was connected in between and connection exist
                at the end of the report */
        {
            report_entry->duration_ms =
                sm_client_report_stat_delta(
                        timestamp_ms,
                        record_entry->connect_ts);
        }
    }
    else /* Client was disconnected between report duration */
    {
        /* client was reconnected and connection exists at the
           end of report interval */
        if (record_entry->is_connected)
        {
            report_entry->duration_ms +=
                sm_client_report_stat_delta(
                        timestamp_ms,
                        record_entry->connect_ts);
        }
    }

    /* Store report time for next calculation ... need to be bellow
       calculate_duration!
     */
    client_ctx->duration_ts = timestamp_ms;
}

static
bool sm_client_report_rx_stats_calculate_average(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *stats_record_list,
        ds_dlist_t                 *stats_report_list)
{
    dpp_client_stats_rx_t          *record = NULL;
    ds_dlist_iter_t                 record_iter;
    dpp_client_stats_rx_t          *report = NULL;
    ds_dlist_iter_t                 report_iter;
    bool                            found = false;

    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    /* Find the record and if exists update it otherwise create new! */
    for (   record = ds_dlist_ifirst(&record_iter, stats_record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        for (   report = ds_dlist_ifirst(&report_iter, stats_report_list);
                report != NULL;
                report = ds_dlist_inext(&report_iter))
        {
            found = false;
            if (    (report->mcs == record->mcs)
                 && (report->nss == record->nss)
                 && (report->bw == record->bw)
               )
            {
                found = true;
                break;
            }
        }

        /* Add new measurement */
        if (!found) {
            report =
                dpp_client_stats_rx_record_alloc();
            if (NULL == report) {
                LOG(ERR,
                    "Sending %s client report "
                    "(Failed to allocate rx stats memory)",
                    radio_get_name_from_cfg(radio_cfg_ctx));
                return false;
            }

            report->mcs = record->mcs;
            report->nss = record->nss;
            report->bw  = record->bw;
        }

        report->bytes   += record->bytes;
        report->msdu    += record->msdu;
        report->mpdu    += record->mpdu;
        report->ppdu    += record->ppdu;
        report->retries += record->retries;
        report->errors  += record->errors;
        report->rssi     = record->rssi;

        if (!found) {
            ds_dlist_insert_tail(stats_report_list, report);
        }
    }

    return true;
}

static
bool sm_client_report_tx_stats_calculate_average(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *stats_record_list,
        ds_dlist_t                 *stats_report_list)
{
    dpp_client_stats_tx_t          *record = NULL;
    ds_dlist_iter_t                 record_iter;
    dpp_client_stats_tx_t          *report = NULL;
    ds_dlist_iter_t                 report_iter;
    bool                            found = false;

    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    for (   record = ds_dlist_ifirst(&record_iter, stats_record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        for (   report = ds_dlist_ifirst(&report_iter, stats_report_list);
                report != NULL;
                report = ds_dlist_inext(&report_iter))
        {
            found = false;
            if (    (report->mcs == record->mcs)
                 && (report->nss == record->nss)
                 && (report->bw == record->bw)
               )
            {
                found = true;
                break;
            }
        }

        /* Add new measurement */
        if (!found) {
            report =
                dpp_client_stats_tx_record_alloc();
            if (NULL == report) {
                LOG(ERR,
                    "Sending %s client report "
                    "(Failed to allocate tx stats memory)",
                    radio_get_name_from_cfg(radio_cfg_ctx));
                return false;
            }

            report->mcs = record->mcs;
            report->nss = record->nss;
            report->bw  = record->bw;
        }

        report->bytes   += record->bytes;
        report->msdu    += record->msdu;
        report->mpdu    += record->mpdu;
        report->ppdu    += record->ppdu;
        report->retries += record->retries;
        report->errors  += record->errors;

        if (!found) {
            ds_dlist_insert_tail(stats_report_list, report);
        }
    }

    return true;
}

static
bool sm_client_report_tid_stats_calculate_average(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *tid_record_list,
        ds_dlist_t                 *tid_report_list)
{
    dpp_client_tid_record_list_t   *record = NULL;
    ds_dlist_iter_t                 record_iter;

    sm_stats_request_t             *request_ctx =
        &client_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    dpp_client_tid_record_list_t   *report = NULL;

    for (   record = ds_dlist_ifirst(&record_iter, tid_record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        /* Add new measurement */
        report =
            dpp_client_tid_record_alloc();
        if (NULL == report) {
            LOG(ERR,
                "Sending %s client report "
                "(Failed to allocate tid stats memory)",
                radio_get_name_from_cfg(radio_cfg_ctx));
            return false;
        }

        /* Copy tid records */
        memcpy(&report->entry, &record->entry, sizeof(record->entry));

        /* Adjust timestamp */
        report->timestamp_ms =
            request_ctx->reporting_timestamp - client_ctx->report_ts +
            record->timestamp_ms;

        ds_dlist_insert_tail(tid_report_list, report);
    }

    return true;
}

static
void sm_client_report_stats_calculate_average (
        sm_client_ctx_t            *client_ctx,
        dpp_client_stats_t         *record,
        dpp_client_stats_t         *report)
{
    report->bytes_tx    += record->bytes_tx;
    report->bytes_rx    += record->bytes_rx;
    report->frames_tx   += record->frames_tx;
    report->frames_rx   += record->frames_rx;
    report->retries_rx  += record->retries_rx;
    report->retries_tx  += record->retries_tx;
    report->errors_rx   += record->errors_rx;
    report->errors_tx   += record->errors_tx;

    /* Average an average? */
    report->rate_rx     = record->rate_rx;
    report->rate_tx     = record->rate_tx;
    report->rssi        = record->rssi;
}

static
void sm_client_report_calculate_average (
        sm_client_ctx_t            *client_ctx,
        sm_client_record_t         *record,
        dpp_client_record_t        *report_entry)
{
    dpp_client_record_t            *record_entry = NULL;
    ds_dlist_iter_t                 record_iter;

    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    for (   record_entry = ds_dlist_ifirst(&record_iter, &record->result_list);
            record_entry != NULL;
            record_entry = ds_dlist_inext(&record_iter))
    {
        /* Copy stats rx list stats */
        sm_client_report_stats_calculate_average(
                client_ctx,
                &record_entry->stats,
                &report_entry->stats);

        /* Copy uAPSD info (Debug purpose only) */
        report_entry->uapsd |= record_entry->uapsd;

        /* Copy stats rx list stats */
        sm_client_report_rx_stats_calculate_average(
                client_ctx,
                &record_entry->stats_rx,
                &report_entry->stats_rx);

        /* Copy stats tx list stats */
        sm_client_report_tx_stats_calculate_average(
                client_ctx,
                &record_entry->stats_tx,
                &report_entry->stats_tx);

        /* Copy TID list stats */
        sm_client_report_tid_stats_calculate_average(
                client_ctx,
                &record_entry->tid_record_list,
                &report_entry->tid_record_list);
    }

    LOG(DEBUG,
        "Sending %s client "MAC_ADDRESS_FORMAT
        " stats {rssi=%d bytes=(rx=%"PRIu64" tx=%"PRIu64")"
        " rate (rx=%0.3f tx=%0.3f)}",
        radio_get_name_from_cfg(radio_cfg_ctx),
        MAC_ADDRESS_PRINT(report_entry->info.mac),
        report_entry->stats.rssi,
        report_entry->stats.bytes_rx,
        report_entry->stats.bytes_tx,
        report_entry->stats.rate_rx,
        report_entry->stats.rate_tx);
}

static
bool sm_client_report_send(
        sm_client_ctx_t            *client_ctx)
{
    bool                            status;
    ds_dlist_t                     *record_list =
        &client_ctx->record_list;
    sm_client_record_t             *record = NULL;
    ds_dlist_iter_t                 record_iter;
    dpp_client_record_t            *record_entry = NULL;

    dpp_client_report_data_t       *report_ctx =
        &client_ctx->report;
    ds_dlist_t                     *report_list =
        &report_ctx->list;
    dpp_client_record_t            *report_entry = NULL;

    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;
    sm_stats_request_t             *request_ctx =
        &client_ctx->request;
    ev_timer                       *report_timer =
        &client_ctx->report_timer;

    /* Restart timer */
    sm_client_report_timer_restart(report_timer);

    /* Update type */
    report_ctx->radio_type = radio_cfg_ctx->type;
    report_ctx->channel = radio_cfg_ctx->chan;

    /* Report_timestamp is base-timestamp + relative start time offset */
    report_ctx->timestamp_ms =
        request_ctx->reporting_timestamp - client_ctx->report_ts +
        get_timestamp();

    client_ctx->record_qty++;
    for (   record = ds_dlist_ifirst(&record_iter, record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        record_entry = &record->entry;

        report_entry = dpp_client_record_alloc();
        if (NULL == report_entry) {
            LOG(ERR,
                "Sending %s client report "
                "(Failed to allocate memory)",
                radio_get_name_from_cfg(radio_cfg_ctx));
            return false;
        }

        /* Copy client info */
        memcpy(&report_entry->info, &record_entry->info, sizeof(record_entry->info));

        /* Perform data manipulation from the samples collected
           - for now only aggregation is supported
         */
        sm_client_report_calculate_average(
                client_ctx,
                record,
                report_entry);

        /* Copy connectivity stats */
        report_entry->is_connected  = record_entry->is_connected;
        report_entry->connected     = record_entry->connected;
        report_entry->disconnected  = record_entry->disconnected;
        report_entry->connect_ts    = record_entry->connect_ts;
        report_entry->disconnect_ts = record_entry->disconnect_ts;

        /* Calculate client connected duration before sending report */
        sm_client_report_calculate_duration(
                client_ctx,
                report_ctx->timestamp_ms,
                record_entry,
                report_entry);

        ds_dlist_insert_tail(report_list, report_entry);
    }

    LOG(INFO,
        "Sending %s client report at '%s'",
        radio_get_name_from_cfg(radio_cfg_ctx),
        sm_timestamp_ms_to_date(report_ctx->timestamp_ms));

    /* Send records to MQTT FIFO (Skip empty reports) */
    if(!ds_dlist_is_empty(report_list)) {
        dpp_put_client(report_ctx);
    }

    /* Clear pending report client records */
    status =
        sm_client_record_clear(
                client_ctx,
                report_list);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to clear record list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    SM_SANITY_CHECK_TIME(report_ctx->timestamp_ms,
                         &request_ctx->reporting_timestamp,
                         &client_ctx->report_ts);
    return true;
}

static
void sm_client_records_mark_disconnected (
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *client_list)
{
    sm_stats_request_t             *request_ctx =
        &client_ctx->request;
    ds_dlist_t                     *record_list =
        &client_ctx->record_list;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    sm_client_record_t             *record = NULL;
    ds_dlist_iter_t                 record_iter;
    dpp_client_record_t            *record_entry = NULL;

    target_client_record_t         *client_entry = NULL;
    ds_dlist_iter_t                 client_iter;

    uint32_t                        found;
    uint32_t                        count;

    for (   record = ds_dlist_ifirst(&record_iter, record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        record_entry = &record->entry;

        found = false;
        count = 0;

        for (   client_entry = ds_dlist_ifirst(&client_iter, client_list);
                client_entry != NULL;
                client_entry = ds_dlist_inext(&client_iter))
        {
            /* Check if client is already connected and if it is
               on the same interface
             */
            if (MAC_ADDR_EQ(
                        client_entry->info.mac,
                        record_entry->info.mac)) {
                /* Notify disconnection through stats cookie */
                if (client_entry->stats_cookie !=
                        record->cache.stats_cookie ) {
                    break;
                }

                /* Client changed interface */
                if(0 == strcmp(
                            client_entry->info.ifname,
                            record_entry->info.ifname )) {
                    found = true;
                }

                /* Driver did not yet kickout client so we have
                   it on both radios
                 */
                count++;
            }
        }

        /* Client was either disconnected or changed interface */
        if (!found && (1 >= count)) {
            /* Check if already notified */
            if (record_entry->is_connected) {
                /* Mark client entry disconnected */
                record_entry->is_connected = false;
                record_entry->disconnected++;
                record_entry->disconnect_ts =
                    request_ctx->reporting_timestamp - client_ctx->report_ts +
                    get_timestamp();

                /* Calculate connection time */
                if (record_entry->connect_ts) {
                    record_entry->duration_ms +=
                        sm_client_report_stat_delta(
                                record_entry->disconnect_ts,
                                record_entry->connect_ts);
                }
                else {
                    record_entry->duration_ms +=
                        sm_client_report_stat_delta(
                                record_entry->disconnect_ts,
                                client_ctx->duration_ts);
                }

                LOG(DEBUG,
                    "Marked %s client "MAC_ADDRESS_FORMAT
                    " disconnected (cc=%d dc=%d dur=%"PRIu64"ms)",
                    radio_get_name_from_cfg(radio_cfg_ctx),
                    MAC_ADDRESS_PRINT(record_entry->info.mac),
                    record_entry->connected,
                    record_entry->disconnected,
                    record_entry->duration_ms);
            }
        }
    }
}

static
bool sm_client_records_reset (
        sm_client_ctx_t            *client_ctx)
{
    ds_dlist_t                     *record_list =
        &client_ctx->record_list;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    sm_client_record_t             *record = NULL;
    ds_dlist_iter_t                 record_iter;
    dpp_client_record_t            *record_entry = NULL;

    /* Loop through cached structure */
    for (   record = ds_dlist_ifirst(&record_iter, record_list);
            record != NULL;
            record = ds_dlist_inext(&record_iter))
    {
        record_entry = &record->entry;

        /* Reset means end of report therefore clear all
           dynamic entries regardless of connection status
         */
        sm_client_record_clear(
                client_ctx,
                &record->result_list);

        if(record_entry->is_connected) {
            /* reset counters for new report */
            record_entry->connected = 0;
            record_entry->disconnected = 0;

            /* Duration 0 means that there was no interruption - client
               was connected all the time */
            record_entry->duration_ms = 0;
            record_entry->connect_ts = 0;
            record_entry->disconnect_ts = 0;

            /* Start averaging from beginning */
            memset (&record_entry->stats,
                    0,
                    sizeof (record_entry->stats));
        }
        else {
            /* Remove clients that are not connected */
            LOG(TRACE,
                "Removed %s client "MAC_ADDRESS_FORMAT " from cache",
                radio_get_name_from_cfg(radio_cfg_ctx),
                MAC_ADDRESS_PRINT(record_entry->info.mac));

            ds_dlist_iremove(&record_iter);
            sm_client_record_free(record);
            record = NULL;
        }
    }

    /* Start counting from beginning */
    client_ctx->record_qty = 0;

    return true;
}

static
bool sm_client_records_update_stats(
        sm_client_ctx_t            *client_ctx,
        sm_client_record_t         *record,
        target_client_record_t     *client_entry)
{
    bool                            status;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    dpp_client_record_t            *record_entry = NULL;
    dpp_client_record_t            *result_entry = NULL;

    if (NULL == record) {
        return false;
    }

    record_entry = &record->entry;

    /* Allocate new sample and convert new and old stats into result delta */
    result_entry =
        dpp_client_record_alloc ();
    if (NULL == result_entry) {
        LOG(ERR,
            "Updating %s interface client stats "
            "(Failed to allocate result memory)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    /* Start collecting rx stats for new client entry */
    status =
        target_stats_clients_convert (
                client_ctx->radio_cfg,
                client_entry,
                &record->cache,
                result_entry);
    if (true != status) {
        LOG(ERR,
            "Updating %s interface client stats "
            "(Failed to convert target data)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    /* Update RSSI reporting every sampling interval if enabled */
    if(sm_rssi_is_reporting_enabled(radio_cfg_ctx)) {
        status =
            sm_rssi_stats_results_update (
                    radio_cfg_ctx,
                    record_entry->info.mac,
                    result_entry->stats.rssi,
                    result_entry->stats.frames_rx,
                    result_entry->stats.frames_tx,
                    RSSI_SOURCE_CLIENT);
        if (true != status) {
            LOG(ERR,
                "Updating %s interface client stats "
                "(Failed to update RSSI data)",
                radio_get_name_from_cfg(radio_cfg_ctx));
            return false;
        }
    }

    ds_dlist_insert_tail(&record->result_list, result_entry);

    /* Update cache entry for average calculation */
    memcpy (&record->cache,
            client_entry,
            sizeof(record->cache));

    return true;
}

static
bool sm_client_records_update(
        sm_client_ctx_t            *client_ctx,
        ds_dlist_t                 *client_list,
        bool                        init)
{
    bool                            status;

    sm_stats_request_t             *request_ctx =
        &client_ctx->request;
    ds_dlist_t                     *record_list =
        &client_ctx->record_list;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    sm_client_record_t             *record = NULL;
    dpp_client_record_t            *record_entry = NULL;

    target_client_record_t         *client_entry = NULL;
    ds_dlist_iter_t                 client_iter;

    /* Checked if clients were connected/disconnected
       and notify cloud before cache update
     */
    sm_client_records_mark_disconnected(
            client_ctx,
            client_list);

    /* Send events for stations which have disconnected */
    for (   client_entry = ds_dlist_ifirst(&client_iter, client_list);
            client_entry != NULL;
            client_entry = ds_dlist_inext(&client_iter))
    {
        /* Lookup in global table and retrieve stats if exists */
        record=
            sm_client_records_mac_find(
                    client_ctx,
                    client_entry);
        if (NULL != record) {
            record_entry = &record->entry;

            /* Check reconnection */
            if(!record_entry->is_connected) {
                record_entry->is_connected = true;
                record_entry->connected++;
                record_entry->connect_ts =
                    request_ctx->reporting_timestamp - client_ctx->report_ts +
                    get_timestamp();

                LOG(DEBUG,
                    "Marked %s client "MAC_ADDRESS_FORMAT
                    " reconnected (cc=%d dc=%d dur=%"PRIu64"ms)",
                    radio_get_name_from_cfg(radio_cfg_ctx),
                    MAC_ADDRESS_PRINT(record_entry->info.mac),
                    record_entry->connected,
                    record_entry->disconnected,
                    record_entry->duration_ms);
                goto update_cache;
            }

            LOG(TRACE,
                "Updating %s client "MAC_ADDRESS_FORMAT " entry",
                radio_get_name_from_cfg(radio_cfg_ctx),
                MAC_ADDRESS_PRINT(record_entry->info.mac));
        }
        else /* Entry not found therefore append new one the end */
        {
            record =
                sm_client_record_alloc();
            if (NULL == record) {
                LOG(ERR,
                    "Updating %s interface client stats "
                    "(Failed to allocate record memory)",
                    radio_get_name_from_cfg(radio_cfg_ctx));
                return false;
            }
            record_entry = &record->entry;

            /* Initialize sampling/result list and add the first entry */
            ds_dlist_init(
                    &record->result_list,
                    dpp_client_record_t,
                    node);

            /* Copy general client info. */
            memcpy (&record_entry->info,
                     &client_entry->info,
                     sizeof(record_entry->info));

            /* Init connectivity stats */
            record_entry->is_connected = true;
            record_entry->connected++;
            record_entry->connect_ts =
                    request_ctx->reporting_timestamp - client_ctx->report_ts +
                    get_timestamp();
            record_entry->duration_ms = 0;

            LOG(DEBUG,
                "Marked %s client "MAC_ADDRESS_FORMAT
                " connected (cc=%d dc=%d dur=%"PRIu64"ms)",
                radio_get_name_from_cfg(radio_cfg_ctx),
                MAC_ADDRESS_PRINT(record_entry->info.mac),
                record_entry->connected,
                record_entry->disconnected,
                record_entry->duration_ms);

            /* Insert new entry */
            ds_dlist_insert_tail(record_list, record);
        }

update_cache:
        /* Update old data with current because timer restarted (report/sampling) => delta = 0 */
        if (init) {
            memcpy (&record->cache,
                    client_entry,
                    sizeof(record->cache));
        }

        status =
            sm_client_records_update_stats(
                    client_ctx,
                    record,
                    client_entry);
        if (true != status) {
            LOG(ERR,
                "Updating %s interface client stats "
                "(Failed to allocate record memory)",
                radio_get_name_from_cfg(radio_cfg_ctx));
            return false;
        }
    }

    client_ctx->record_qty++;

    return true;
}

static
bool sm_client_update_list_cb(
        ds_dlist_t                 *client_list,
        void                       *ctx,
        int                         client_status)
{
    bool                            status;

    sm_client_ctx_t                *client_ctx = (sm_client_ctx_t *)ctx;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    if(true != client_status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to get stats)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        goto clear;
    }

    /* Update cached client list with new entries
       old one stay in cache until report is send
     */
    status =
        sm_client_records_update(
                client_ctx,
                client_list,
                false);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to update client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        goto clear;
    }

clear:
    /* Clear temporary received client target list */
    status =
        sm_client_target_clear(
                client_ctx,
                client_list);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to clear client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    return true;
}

static
void sm_client_update (EV_P_ ev_timer *w, int revents)
{
    bool                            status;
    sm_client_ctx_t                *client_ctx =
        (sm_client_ctx_t *) w->data;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;
    ds_dlist_t                     *client_list =
        &client_ctx->client_list;

    /* Check if radio is exists */
    if (target_is_radio_interface_ready(radio_cfg_ctx->phy_name) != true) {
        LOG(TRACE,
            "Skip processing %s client report "
            "(Radio %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_cfg_ctx->phy_name);
        return;
    }

    /* Check if vif interface is exists */
    if (target_is_interface_ready(radio_cfg_ctx->if_name) != true) {
        LOG(TRACE,
            "Skip processing %s client report "
            "(Interface %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_cfg_ctx->if_name);
        return;
    }

    LOG(DEBUG,
        "Processing %s client sample %d",
        radio_get_name_from_cfg(radio_cfg_ctx),
        client_ctx->record_qty);

    /* Get current client target list */
    status =
        target_stats_clients_get(
                radio_cfg_ctx,
                NULL,  /* All */
                sm_client_update_list_cb,
                client_list,
                client_ctx);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to retreive client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return;
    }
}

static
void sm_client_report (EV_P_ ev_timer *w, int revents)
{
    bool                            status;

    sm_client_ctx_t                *client_ctx =
        (sm_client_ctx_t *) w->data;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;

    /* Check if radio is exists */
    if (target_is_radio_interface_ready(radio_cfg_ctx->phy_name) != true) {
        LOG(TRACE,
            "Skip processing %s client report "
            "(Radio %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_cfg_ctx->phy_name);
        return;
    }

    /* Check if vif interface is exists */
    if (target_is_interface_ready(radio_cfg_ctx->if_name) != true) {
        LOG(TRACE,
            "Skip processing %s client report "
            "(Interface %s not configured)",
            radio_get_name_from_cfg(radio_cfg_ctx),
            radio_cfg_ctx->if_name);
        return;
    }

    /* Send report */
    sm_client_report_send(client_ctx);

    /* Reset the internal cache stats */
    status = sm_client_records_reset(client_ctx);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to reset client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return;
    }
}

static
bool sm_client_report_init (
        sm_client_ctx_t           *client_ctx);

static
void sm_client_init_timer_cb (EV_P_ ev_timer *w, int revents)
{
    bool                           status;

    sm_client_ctx_t                *client_ctx =
        (sm_client_ctx_t *) w->data;

    status =
        sm_client_report_init(
                client_ctx);
    if (true != status) {
        return;
    }
}

static
bool sm_client_update_init_cb (
        ds_dlist_t                 *client_list,
        void                       *ctx,
        int                         client_status)
{
    bool                            status;

    sm_client_ctx_t                *client_ctx = (sm_client_ctx_t *)ctx;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;
    sm_stats_request_t             *request_ctx =
        &client_ctx->request;
    ev_timer                       *update_timer =
        &client_ctx->update_timer;
    ev_timer                       *report_timer =
        &client_ctx->report_timer;
    ev_timer                       *init_timer =
        &client_ctx->init_timer;

    if(true != client_status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to get client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    /* Update cached client list with new entries
       old one stay in cache until report is send
     */
    status =
        sm_client_records_update(
                client_ctx,
                client_list,
                true);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to update client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    /* Clear temporary received client target list */
    status =
        sm_client_target_clear(
                client_ctx,
                client_list);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to clear client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    /* No averaging since sampling is 0 */
    if (request_ctx->sampling_interval) {
        update_timer->repeat = request_ctx->sampling_interval;
        sm_client_timer_set(update_timer, true);
    }

    report_timer->repeat = request_ctx->reporting_interval;
    sm_client_timer_set(report_timer, true);

    /* Stop init timer and start with processing */
    sm_client_timer_set(init_timer, false);

    LOG(INFO,
        "Started %s client reporting",
        radio_get_name_from_cfg(radio_cfg_ctx));

    return true;
}

static
bool sm_client_report_init(
        sm_client_ctx_t            *client_ctx)
{
    bool                            status;

    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;
    ds_dlist_t                     *record_list =
        &client_ctx->record_list;
    ds_dlist_t                     *client_list =
        &client_ctx->client_list;
    ds_dlist_init(
            client_list,
            target_client_record_t,
            node);

    ev_timer                       *init_timer =
        &client_ctx->init_timer;
    init_timer->repeat = CLIENT_INIT_TIME;
    sm_client_timer_set(init_timer, true);

    /* Clear cached client records */
    status =
        sm_client_sm_records_clear(
                client_ctx,
                record_list);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to clear client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    /* Get target layer and update cache */
    status =
        target_stats_clients_get(
                radio_cfg_ctx,
                NULL,  /* All */
                sm_client_update_init_cb,
                client_list,
                client_ctx);
    if (true != status) {
        LOG(ERR,
            "Processing %s client report "
            "(failed to retreive client list)",
            radio_get_name_from_cfg(radio_cfg_ctx));
        return false;
    }

    return true;
}

static
bool sm_client_stats_process (
        radio_entry_t              *radio_cfg, // For logging
        sm_client_ctx_t            *client_ctx)
{
    bool                            status;

    sm_stats_request_t             *request_ctx =
        &client_ctx->request;
    radio_entry_t                  *radio_cfg_ctx =
        client_ctx->radio_cfg;
    ev_timer                       *update_timer =
        &client_ctx->update_timer;
    ev_timer                       *report_timer =
        &client_ctx->report_timer;

    /* Skip client client report start if radio is not configured */
    if (!radio_cfg_ctx) {
        LOG(TRACE,
            "Skip starting %s client client reporting "
            "(Radio not configured)",
            radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    /* Skip client client report start if stats are not configured */
    if (!client_ctx->initialized) {
        LOG(TRACE,
            "Skip starting %s client client reporting "
            "(Stats not configured)",
            radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    /* Skip client client report start if timestamp is not specified */
    if (!request_ctx->reporting_timestamp) {
        LOG(TRACE,
            "Skip starting %s client client reporting "
            "(Timestamp not configured)",
            radio_get_name_from_cfg(radio_cfg));
        return true;
    }

    /* Start counting from beginning */
    client_ctx->record_qty = 0;

    /* Restart timers with new parameters */
    sm_client_timer_set(update_timer, false);
    sm_client_timer_set(report_timer, false);

    if (request_ctx->reporting_interval) {

        /* Mark beginning of the report */
        client_ctx->report_ts = get_timestamp();

        status =
           sm_client_report_init(
                   client_ctx);
        if (true != status) {
            return false;
        }
   }
    else {
        LOG(INFO,
            "Stopped %s client reporting",
            radio_get_name_from_cfg(radio_cfg));

        memset(request_ctx, 0, sizeof(*request_ctx));
    }

    return true;
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
bool sm_client_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request)
{
    bool                            status;

    sm_client_ctx_t                *client_ctx = NULL;
    sm_stats_request_t             *request_ctx = NULL;
    dpp_client_report_data_t       *report_ctx = NULL;
    ev_timer                       *update_timer = NULL;
    ev_timer                       *report_timer = NULL;
    ev_timer                       *init_timer = NULL;

    if (NULL == request) {
        LOG(ERR,
            "Initializing client reporting "
            "(Invalid request config)");
        return false;
    }

    client_ctx      = sm_client_ctx_get(radio_cfg);
    request_ctx     = &client_ctx->request;
    report_ctx      = &client_ctx->report;
    update_timer    = &client_ctx->update_timer;
    report_timer    = &client_ctx->report_timer;
    init_timer      = &client_ctx->init_timer;

    /* Initialize global storage and timer config only once */
    if (!client_ctx->initialized) {
        memset(request_ctx, 0, sizeof(*request_ctx));
        memset(report_ctx, 0, sizeof(*report_ctx));

        LOG(INFO,
            "Initializing %s client reporting",
            radio_get_name_from_cfg(radio_cfg));

        /* Initialize report client list */
        ds_dlist_init(
                &report_ctx->list,
                dpp_client_record_t,
                node);

        /* Initialize client list */
        ds_dlist_init(
                &client_ctx->record_list,
                sm_client_record_t,
                node);

        /* Reschedule initialization in case of error */
        ev_init (init_timer, sm_client_init_timer_cb);
        init_timer->data = client_ctx;

        /* Initialize event lib timers and pass the global
           internal cache
         */
        ev_init (update_timer, sm_client_update);
        update_timer->data = client_ctx;
        ev_init (report_timer, sm_client_report);
        report_timer->data = client_ctx;

        client_ctx->initialized = true;
    }

    REQUEST_PARAM_UPDATE("client", radio_type, "%d");
    REQUEST_PARAM_UPDATE("client", reporting_count, "%d");
    REQUEST_PARAM_UPDATE("client", reporting_interval, "%d");
    REQUEST_PARAM_UPDATE("client", sampling_interval, "%d");
    REQUEST_PARAM_UPDATE("client", reporting_timestamp, "%"PRIu64"");

    status =
        sm_client_stats_process (
                radio_cfg,
                client_ctx);
    if (true != status) {
        return false;
    }

    return true;
}

bool sm_client_report_radio_change(
        radio_entry_t              *radio_cfg)
{
    bool                            status;
    sm_client_ctx_t                *client_ctx = NULL;

    if (NULL == radio_cfg) {
        LOG(ERR,
            "Initializing client reporting "
            "(Invalid radio config)");
        return false;
    }

    client_ctx = sm_client_ctx_get(radio_cfg);

    status =
        sm_client_stats_process (
                radio_cfg,
                client_ctx);
    if (true != status) {
        return false;
    }

    return true;
}
