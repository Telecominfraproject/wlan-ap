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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <protobuf-c.h>

#include "statssim.h"
#include "ds_list.h"
#include "os_nif.h"
#include "os_time.h"
#include "dppline.h"
#include "util.h"

#define FIX_SIZE

#define SSID_TEST   "mqttsim_ssid_"

sim_ap_conf_t *sim_ap_conf;

/*
 * Generate
 */

void mkRandMac(mac_address_t mac)
{
    int r1 = rand();
    int r2 = rand();
    mac[0] = r1 & 0xFF;
    mac[1] = (r1 >> 8) & 0xFF;
    mac[2] = (r1 >> 16) & 0xFF;
    mac[3] = r2 & 0xFF;
    mac[4] = (r2 >> 8) & 0xFF;
    mac[5] = (r2 >> 16) & 0xFF;
}

void mkRandMacStr(char *macstr, int size)
{
    mac_address_t mac;
    mkRandMac(mac);
    dpp_mac_to_str(mac, macstr);
}

void mkNeighborRec(dpp_neighbor_record_t *rec, radio_type_t radio_type)
{
    rec->type = radio_type; // not used
    rec->tsf = rand() % 10000;
    rec->sig = -50 - rand() % 30;
    if (radio_type == RADIO_TYPE_2G) {
        rec->chan = 6 + rand() % 8;
        rec->chanwidth = RADIO_CHAN_WIDTH_40MHZ;
    } else {
        rec->chan = 40 + rand() % 20;
        rec->chanwidth = RADIO_CHAN_WIDTH_80MHZ;
    }
}

void getNeighborReport(dpp_neighbor_report_data_t *r, radio_type_t radio_type)
{
    int rindex = radio_type - 1;
    int i, qty = 0;
    dpp_neighbor_list_t              *res_list =
        &r->list;
    dpp_neighbor_record_list_t       *res = NULL;
    dpp_neighbor_record_t            *rec;

    r->report_type = REPORT_TYPE_RAW;
    r->scan_type = RADIO_SCAN_TYPE_FULL;
    r->radio_type = radio_type;

    if (sim_ap_conf)
        qty = sim_ap_conf->r[rindex].n_neigh;
    else
        qty = 6;

    for (i=0; i < qty; i++)
    {
        res=
            dpp_neighbor_record_alloc();
        if (NULL == res) {
            return;
        }
        rec = &res->entry;

        mkNeighborRec(rec, radio_type);
        if (sim_ap_conf)
            STRSCPY(rec->bssid, sim_ap_conf->r[rindex].neigh[i]);
        else
            mkRandMacStr(rec->bssid, sizeof(rec->bssid));
        snprintf(rec->ssid, sizeof(rec->ssid), "TestSsid_%s_%d",
                radio_type == RADIO_TYPE_2G ? "24" : "5", i);

        ds_dlist_insert_tail(res_list, res);
    }
}


void getClientReport(dpp_client_report_data_t *r, radio_type_t radio_type)
{
    size_t                          i, j, qty = 0;
    int rindex =                    radio_type - 1;
    ds_dlist_t                     *res_list =
        &r->list;
    dpp_client_record_t            *res = NULL;

    r->radio_type = radio_type;
    r->channel = (radio_type == RADIO_TYPE_2G) ? 1 : 36;
    if (sim_ap_conf) {
        qty = sim_ap_conf->r[rindex].n_client;
    }
    else {
        qty = 5;
    }

    for (i = 0; i < qty; i++)
    {
        res= dpp_client_record_alloc();
        if (NULL == res) {
            return;
        }

        res->is_connected = rand() % 2;
        res->connected = 1 + rand() % 4;
        res->disconnected = res->connected - res->is_connected;
        res->connect_ts = 12345;
        res->disconnect_ts = 12345;
        res->duration_ms = 10;

        res->info.type = radio_type;
        // mac
        if (sim_ap_conf) {
            os_nif_macaddr_from_str((void*)res->info.mac,
                    sim_ap_conf->r[rindex].client[i]);
        }
        else {
            mkRandMac(res->info.mac);
        }
        // ssid
        sprintf(res->info.essid, "ssid%04d", rand()%10000);

        // stats
        res->stats.rssi = 50 - rand() % 30;
        res->stats.bytes_tx = rand() % 1000;
        res->stats.bytes_rx = rand() % 1000;
        res->stats.frames_tx = rand() % 10;
        res->stats.frames_rx = rand() % 10;
        res->stats.retries_tx = rand() % 2;
        res->stats.retries_rx = rand() % 2;
        res->stats.errors_tx = rand() % 2;
        res->stats.errors_rx = rand() % 2;
        res->stats.rate_tx = (double)(rand() % 800);
        res->stats.rate_rx = (double)(rand() % 600);

        // advanced stats
        dpp_client_stats_rx_t *rx;
        ds_dlist_iter_t        rx_iter;
        int mcs = 0;
        int nss = 0;
        int bw = 0;
        bool found = false;
        for (j = 0; j < 4; j++)
        {
            mcs = rand() % 10;
            nss = rand() % 4;
            bw = rand() % 3;
            found = false;
            for (   rx = ds_dlist_ifirst(&rx_iter, &res->stats_rx);
                    rx != NULL;
                    rx = ds_dlist_inext(&rx_iter))
            {
                if (    (rx->mcs = mcs)
                     && (rx->nss = nss)
                     && (rx->bw = bw)
                   )
                {
                    found = true;
                    break;
                }
            }

            if (!found) {
                rx = dpp_client_stats_rx_record_alloc();
                if (NULL == rx) {
                    return;
                }
                rx->mcs  = mcs;
                rx->nss  = nss;
                rx->bw   = bw;
            }
            rx->bytes += rand() % 1000;
            rx->msdu += rand() % 10;
            rx->mpdu += rand() % 10;
            rx->ppdu += rand() % 10;
            rx->retries += rand() % 2;
            rx->errors += rand() % 2;
            rx->rssi = 50 - rand() % 30;

            if (!found) ds_dlist_insert_tail(&res->stats_rx, rx);
        }

        dpp_client_stats_tx_t *tx;
        ds_dlist_iter_t        tx_iter;
        for (j = 0; j < 4; j++)
        {
            mcs = rand() % 10;
            nss = rand() % 4;
            bw = rand() % 3;
            found = false;
            for (   tx = ds_dlist_ifirst(&tx_iter, &res->stats_tx);
                    tx != NULL;
                    tx = ds_dlist_inext(&tx_iter))
            {
                if (    (tx->mcs = mcs)
                     && (tx->nss = nss)
                     && (tx->bw = bw)
                   )
                {
                    found = true;
                    break;
                }
            }

            if (!found) {
                tx = dpp_client_stats_tx_record_alloc();
                if (NULL == tx) {
                    return;
                }
                tx->mcs  = mcs;
                tx->nss  = nss;
                tx->bw   = bw;
            }

            tx->bytes += rand() % 1000;
            tx->msdu += rand() % 10;
            tx->mpdu += rand() % 10;
            tx->ppdu += rand() % 10;
            tx->retries += rand() % 2;
            tx->errors += rand() % 2;

            if (!found) ds_dlist_insert_tail(&res->stats_tx, tx);
        }
        ds_dlist_insert_tail(res_list, res);
    }
}

/*
 * Generate survey report
 */
void getSurveyReport(dpp_survey_report_data_t *r, radio_type_t radio_type, radio_scan_type_t s_type)
{
    size_t                          i, qty = 0;
    int rindex =                    radio_type - 1;
    static int                      timestamp = 0;
    ds_dlist_t                     *res_list =
        &r->list;

    if (!r) return;

    if (sim_ap_conf) {
        qty = sim_ap_conf->r[rindex].n_surv;
    }
    else {
        qty = 5;
    }

    timestamp += qty * 1000 + rand() % 100;
    r->timestamp_ms = timestamp;
    r->report_type = REPORT_TYPE_RAW + rand() % 1;
    r->radio_type = radio_type;
    r->scan_type = s_type;

    for (i = 0; i < qty; i++)
    {
        if (REPORT_TYPE_RAW == r->report_type) {
            dpp_survey_record_t            *sr;
            sr = dpp_survey_record_alloc();
            if (NULL == sr) {
                return;
            }

            sr->info.chan = ((RADIO_TYPE_2G == radio_type) ? 1 : 36);
            if (s_type == RADIO_SCAN_TYPE_OFFCHAN) sr->info.chan += i;
            sr->info.timestamp_ms = timestamp - (qty - i) * 1000;

            sr->chan_busy = rand() % 90;
            sr->chan_busy_ext = rand() % 40;
            sr->chan_tx = rand() % 20;
            sr->chan_rx = rand() % 50;
            sr->chan_self = 0;
            sr->duration_ms = rand() % 10 + 100;

            ds_dlist_insert_tail(res_list, sr);
        } else {
            dpp_survey_record_avg_t        *sr;

            sr= calloc(1, sizeof(*sr));
            if (NULL == sr) {
                return;
            }

            sr->info.chan = ((RADIO_TYPE_2G == radio_type) ? 1 : 36);
            if (s_type == RADIO_SCAN_TYPE_OFFCHAN) sr->info.chan += i;

#define AVG( _name, _val) do { \
        sr->_name.avg = (_val) - rand() % 10; \
        sr->_name.min  = (_val) - rand() % 30; \
        sr->_name.max  = (_val) - rand() % 20; \
        sr->_name.num  = rand() % 10; \
    } while (0)

            AVG(chan_busy,      (rand() % 90));
            AVG(chan_busy_ext,  (rand() % 40));
            AVG(chan_tx,        (rand() % 20));
            AVG(chan_rx,        (rand() % 50));
            AVG(chan_self,      (0));

            ds_dlist_insert_tail(res_list, sr);
        }
    }
}

/*
 * Generate survey report
 */
void getDeviceReport(dpp_device_report_data_t *r)
{
    size_t                          i;
    dpp_device_temp_t              *temp;

    if (!r) return;

    r->timestamp_ms = clock_real_ms();
    r->record.load[DPP_DEVICE_LOAD_AVG_ONE] = (double)(rand() % 10000) / 1000;
    r->record.load[DPP_DEVICE_LOAD_AVG_FIVE] = (double)(rand() % 7000) / 1000;
    r->record.load[DPP_DEVICE_LOAD_AVG_FIFTEEN] = (double)(rand() % 5000) / 1000;

    for (i = 0; i < RADIO_MAX_DEVICE_QTY; i++)
    {
        temp =
            dpp_device_temp_record_alloc();
        if (NULL == temp) {
            return;
        }

        temp->type = i+1;
        temp->value = rand() % 60;

        ds_dlist_insert_tail(&r->temp, temp);
    }
}

/*
 * Generate survey report
 */
void getCapacityReport(dpp_capacity_report_data_t *r, radio_type_t radio_type)
{
    size_t                          i, j, qty = 0;
    int rindex =                    radio_type - 1;
    static int                      timestamp = 0;
    dpp_capacity_list_t            *res_list = &r->list;
    dpp_capacity_record_list_t     *res = NULL;
    dpp_capacity_record_t          *sr;

    if (!r) return;

    if (sim_ap_conf)
    {
        qty = sim_ap_conf->r[rindex].n_cap;
    }
    else
    {
        qty = 5;
    }

    timestamp += qty * 1000 + rand() % 100;
    r->timestamp_ms = timestamp;
    r->radio_type = radio_type;

    for (i = 0; i < qty; i++)
    {
        res = dpp_capacity_record_alloc();
        if (NULL == res)
        {
            return;
        }
        sr = &res->entry;

        sr->busy_tx = rand() % 20;
        sr->bytes_tx = rand() % 10000;
        sr->samples = 1000 + rand() % 20;

        for (j = 0; j < RADIO_QUEUE_MAX_QTY; j++)
        {
            sr->queue[j] = rand() % 80;
        }

        sr->timestamp_ms = timestamp - (qty - i) * 1000;

        ds_dlist_insert_tail(res_list, res);
    }
}

// generate band steering report

void getBS_BandRecord(dpp_bs_client_report_data_t *r, dpp_bs_client_band_record_t *rec, radio_type_t type)
{
    int i;
    dpp_bs_client_event_record_t *ev;
    rec->type = type;
    rec->connected              = rand() % 2;
    rec->rejects                = rand() % 5;
    rec->connects               = rand() % 5;
    rec->disconnects            = rand() % 5;
    rec->activity_changes       = rand() % 5;
    rec->steering_success_cnt   = rand() % 5;
    rec->steering_fail_cnt      = rand() % 5;
    rec->steering_kick_cnt      = rand() % 5;
    rec->sticky_kick_cnt        = rand() % 5;
    rec->probe_bcast_cnt        = rand() % 5;
    rec->probe_bcast_blocked    = rand() % 5;
    rec->probe_direct_cnt       = rand() % 5;
    rec->probe_direct_blocked   = rand() % 5;
    rec->num_event_records      = 2 + rand() % 5;
    uint32_t offset_ms = 0;
    for (i=0; i < (int)rec->num_event_records; i++) {
        offset_ms += rand() % 1000;
        ev = &rec->event_record[i];
        ev->type                = rand() % MAX_EVENTS;
        ev->timestamp_ms        = r->timestamp_ms - offset_ms;
        ev->rssi                = 50 + rand() % 50;
        ev->probe_bcast         = rand() % 10;
        ev->probe_blocked       = rand() % 10;
        ev->disconnect_src      = rand() % MAX_DISCONNECT_SOURCES;
        ev->disconnect_type     = rand() % MAX_DISCONNECT_TYPES;
        ev->disconnect_reason   = rand() % 1000;
        ev->backoff_enabled     = rand() % 2;
        ev->active              = rand() % 2;
    }
}

void getBSClientReport(dpp_bs_client_report_data_t *r)
{
    int i;
    int num = 3;
    dpp_bs_client_record_list_t *res = NULL;
    dpp_bs_client_record_t      *sr;

    r->timestamp_ms = clock_real_ms();
    for (i=0; i<num; i++) {
        res = dpp_bs_client_record_alloc();
        if (NULL == res) return;
        sr = &res->entry;
        mkRandMac(sr->mac);
        //printf("--- insert: %s\n", dpp_mac_str_tmp(sr->mac));
        switch (i % 3) {
            case 0:
                sr->num_band_records = 1;
                getBS_BandRecord(r, &sr->band_record[0], RADIO_TYPE_2G);
                break;
            case 1:
                sr->num_band_records = 1;
                getBS_BandRecord(r, &sr->band_record[0], RADIO_TYPE_5G);
                break;
            case 2:
                sr->num_band_records = 2;
                getBS_BandRecord(r, &sr->band_record[0], RADIO_TYPE_2G);
                getBS_BandRecord(r, &sr->band_record[1], RADIO_TYPE_5G);
                break;
        }
        ds_dlist_insert_tail(&r->list, res);
    }
}

void getRssiReport(dpp_rssi_report_data_t *r, radio_type_t radio_type)
{
    size_t                          i, j, qty = 0;
    int rindex =                    radio_type - 1;
    ds_dlist_t                     *res_list =
        &r->list;
    dpp_rssi_record_t              *res = NULL;

    r->radio_type = radio_type;
    r->report_type = REPORT_TYPE_RAW + rand() % 1;
    r->timestamp_ms = clock_real_ms();
    if (sim_ap_conf) {
        qty = sim_ap_conf->r[rindex].n_rssi;
    }
    else {
        qty = 5;
    }

    for (i = 0; i < qty; i++)
    {
        res=
            dpp_rssi_record_alloc();
        if (NULL == res) {
            return;
        }

        // mac
        if (sim_ap_conf) {
            os_nif_macaddr_from_str((void*)res->mac,
                    sim_ap_conf->r[rindex].rssi[i]);
        }
        else {
            mkRandMac(res->mac);
        }

        res->source = RSSI_SOURCE_CLIENT + rand() % 2;

        if (REPORT_TYPE_RAW == r->report_type) {
            ds_dlist_init(
                    &res->rssi.raw,
                    dpp_rssi_raw_t,
                    node);

            dpp_rssi_raw_t *raw;
            for (j = 0; j < 4; j++)
            {
                raw = calloc(1, sizeof(*raw));

                raw->rssi = 50 - rand() % 30;
                raw->timestamp_ms = r->timestamp_ms + (10 * (3 - i));

                ds_dlist_insert_tail(&res->rssi.raw, raw);
            }
        } else {
            dpp_avg_t *avg = &res->rssi.avg;

            avg->avg = 50 - rand() % 10;
            avg->min  = 50 - rand() % 30;
            avg->max  = 50 - rand() % 20;
            avg->num  = rand() % 10;
        }

        ds_dlist_insert_tail(res_list, res);
    }
}
