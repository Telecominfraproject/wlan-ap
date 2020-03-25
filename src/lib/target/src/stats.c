/*
Copyright (c) 2019, Plume Design Inc. All rights reserved.

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

#include "target.h"
#include <stdio.h>
#include <stdbool.h>


/*****************************************************************************
 *  INTERFACE definitions
 *****************************************************************************/

bool target_is_radio_interface_ready(char *phy_name)
{
    return true;
}

bool target_is_interface_ready(char *if_name)
{
    return true;
}


/******************************************************************************
 *  STATS definitions
 *****************************************************************************/

bool target_radio_tx_stats_enable(radio_entry_t *radio_cfg, bool enable)
{
    return true;
}

bool target_radio_fast_scan_enable(radio_entry_t *radio_cfg, ifname_t if_name)
{
    return true;
}


/******************************************************************************
 *  CLIENT definitions
 *****************************************************************************/

target_client_record_t* target_client_record_alloc()
{
    target_client_record_t *record = NULL;

    record = malloc(sizeof(target_client_record_t));
    if (record == NULL) return NULL;

    memset(record, 0, sizeof(target_client_record_t));

    return record;
}

void target_client_record_free(target_client_record_t *record)
{
    if (record != NULL)
    {
        free(record);
    }
}

bool target_stats_clients_get(
        radio_entry_t *radio_cfg,
        radio_essid_t *essid,
        target_stats_clients_cb_t *client_cb,
        ds_dlist_t *client_list,
        void *client_ctx)
{
    mac_address_t            mac             = {0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    ifname_t                 interface       = "br0";
    target_client_record_t  *client_entry    = NULL;

    client_entry = target_client_record_alloc();

    /* Insert dummy data for clients */
    client_entry->info.type = RADIO_TYPE_2G;
    memcpy(client_entry->info.mac, mac, sizeof(mac));
    memcpy(client_entry->info.ifname, interface, sizeof(interface));
    client_entry->stats.bytes_tx = 100;
    client_entry->stats.bytes_rx = 200;
    client_entry->stats.rssi = 80;
    client_entry->stats.rate_tx = 1;
    client_entry->stats.rate_rx = 2;

    ds_dlist_insert_tail(client_list, client_entry);

    (*client_cb)(client_list, client_ctx, true);

    return true;
}

bool target_stats_clients_convert(
        radio_entry_t *radio_cfg,
        target_client_record_t *data_new,
        target_client_record_t *data_old,
        dpp_client_record_t *client_record)
{
    memcpy(client_record->info.mac, data_new->info.mac, sizeof(data_new->info.mac));

    client_record->stats.bytes_tx   = data_new->stats.bytes_tx;
    client_record->stats.bytes_rx   = data_new->stats.bytes_rx;
    client_record->stats.rssi       = data_new->stats.rssi;
    client_record->stats.rate_tx    = data_new->stats.rate_tx;
    client_record->stats.rate_rx    = data_new->stats.rate_rx;

    return true;
}


/******************************************************************************
 *  SURVEY definitions
 *****************************************************************************/

target_survey_record_t* target_survey_record_alloc()
{
    target_survey_record_t *record = NULL;

    record = malloc(sizeof(target_survey_record_t));
    if (record == NULL) return NULL;

    memset(record, 0, sizeof(target_survey_record_t));

    return record;
}

void target_survey_record_free(target_survey_record_t *result)
{
    if (result != NULL)
    {
        free(result);
    }
}

bool target_stats_survey_get(
        radio_entry_t *radio_cfg,
        uint32_t *chan_list,
        uint32_t chan_num,
        radio_scan_type_t scan_type,
        target_stats_survey_cb_t *survey_cb,
        ds_dlist_t *survey_list,
        void *survey_ctx)
{
    target_survey_record_t  *survey_record;

    survey_record = target_survey_record_alloc();
    survey_record->info.chan = 1;
    ds_dlist_insert_tail(survey_list, survey_record);

    (*survey_cb)(survey_list, survey_ctx, true);

    return true;
}

bool target_stats_survey_convert(
        radio_entry_t *radio_cfg,
        radio_scan_type_t scan_type,
        target_survey_record_t *data_new,
        target_survey_record_t *data_old,
        dpp_survey_record_t *survey_record)
{
    survey_record->chan_tx       = 30;
    survey_record->chan_self     = 30;
    survey_record->chan_rx       = 40;
    survey_record->chan_busy_ext = 50;
    survey_record->duration_ms   = 60;
    survey_record->chan_busy     = 70;

    return true;
}


/******************************************************************************
 *  NEIGHBORS definitions
 *****************************************************************************/

bool target_stats_scan_start(
        radio_entry_t *radio_cfg,
        uint32_t *chan_list,
        uint32_t chan_num,
        radio_scan_type_t scan_type,
        int32_t dwell_time,
        target_scan_cb_t *scan_cb,
        void *scan_ctx)
{
    (*scan_cb)(scan_ctx, true);

    return true;
}

bool target_stats_scan_stop(
        radio_entry_t *radio_cfg,
        radio_scan_type_t scan_type)
{
    return true;
}

bool target_stats_scan_get(
        radio_entry_t *radio_cfg,
        uint32_t *chan_list,
        uint32_t chan_num,
        radio_scan_type_t scan_type,
        dpp_neighbor_report_data_t *scan_results)
{
    /* Insert dummy data for neighbors */
    dpp_neighbor_record_list_t *neighbor;

    neighbor = dpp_neighbor_record_alloc();
    if (neighbor == NULL) return false;

    neighbor->entry.type        = radio_cfg->type;
    neighbor->entry.lastseen    = 1000;
    neighbor->entry.sig         = 50;
    neighbor->entry.chan        = chan_list[0];
    neighbor->entry.chanwidth   = 40;
    strncpy(neighbor->entry.ssid, "NeighWifi", 10);
    strncpy(neighbor->entry.bssid, "ff:ee:dd:cc:bb:aa", 17);

    ds_dlist_insert_tail(&scan_results->list, neighbor);

    return true;
}


/******************************************************************************
 *  DEVICE definitions
 *****************************************************************************/

bool target_stats_device_temp_get(
        radio_entry_t *radio_cfg,
        dpp_device_temp_t *temp_entry)
{
    temp_entry->type  = radio_cfg->type;
    temp_entry->value = 42;

    return true;
}

bool target_stats_device_txchainmask_get(
        radio_entry_t              *radio_cfg,
        dpp_device_txchainmask_t   *txchainmask_entry)
{
    txchainmask_entry->type  = radio_cfg->type;
    txchainmask_entry->value = 2;

    return true;
}

bool target_stats_device_fanrpm_get(uint32_t *fan_rpm)
{
    return true;
}


/******************************************************************************
 *  CAPACITY definitions
 *****************************************************************************/

bool target_stats_capacity_enable(radio_entry_t *radio_cfg, bool enabled)
{
    return true;
}

bool target_stats_capacity_get(
        radio_entry_t *radio_cfg,
        target_capacity_data_t *capacity_new)
{
    return true;
}

bool target_stats_capacity_convert(
        target_capacity_data_t *capacity_new,
        target_capacity_data_t *capacity_old,
        dpp_capacity_record_t *capacity_entry)
{
    return true;
}
