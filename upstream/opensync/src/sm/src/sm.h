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
 * Stats manager common header file
 */

#ifndef __STATS_PRIV_H__
#define __STATS_PRIV_H__

#include <stdbool.h>
#include <jansson.h>
#include <ev.h>
#include <sys/time.h>
#include <syslog.h>

#include "log.h"

#include "ds.h"
#include "ds_tree.h"

#include "os_nif.h"

#include "target.h"
#include "dppline.h"

#define STATS_MQTT_INTERVAL     5.0 /* Report interval in seconds -- float */

struct sm_cxt {
    struct          ev_io nl_watcher;
    int             nl_fd;
    int             monitor_id;
    int             tx_sm_band;
    int             rx_sm_band;
};

static inline char *sm_timestamp_ms_to_date (uint64_t   timestamp_ms)
{
    struct tm      *dt;
    time_t          t = timestamp_ms / 1000;
    static char     b[32];

    dt = localtime((time_t *)&t);

    memset (b, 0, sizeof(b));
    strftime(b, sizeof(b), "%F %T%z", dt);

    return b;
}

#define REQUEST_PARAM_UPDATE(TYPE, VAR, FMT) \
    if (request_ctx->VAR != request->VAR) \
    { \
        LOG(DEBUG, \
            "Updated %s %s "#VAR" "FMT" -> "FMT"", \
            radio_get_name_from_cfg(radio_cfg), \
            TYPE, \
            request_ctx->VAR, \
            request->VAR); \
        request_ctx->VAR = request->VAR; \
    }

#define REQUEST_VAL_UPDATE(TYPE, VAR, FMT) \
    if (request_ctx->VAR != request->VAR) \
    { \
        LOG(DEBUG, \
            "Updated %s "#VAR" "FMT" -> "FMT"", \
            TYPE, \
            request_ctx->VAR, \
            request->VAR); \
        request_ctx->VAR = request->VAR; \
    }

#define SM_SANITY_CHECK_TIME(timestamp_ms, reporting_timestamp, report_ts) \
    sm_sanity_check_report_timestamp( \
            __FUNCTION__, \
            timestamp_ms, \
            reporting_timestamp, \
            report_ts)


typedef struct
{
    uint32_t                        chan_list[RADIO_MAX_CHANNELS];
    uint32_t                        chan_num;
    uint32_t                        chan_index;
} sm_chan_list_t;

typedef struct
{
    radio_type_t                    radio_type;
    report_type_t                   report_type;
    radio_scan_type_t               scan_type;
    int                             sampling_interval;
    int                             reporting_interval;
    int                             reporting_count;
    int                             scan_interval;
    int                             threshold_util;
    int                             threshold_max_delay;
    int                             threshold_pod_qty;
    int                             threshold_pod_num;
    bool                            mac_filter;
    uint64_t                        reporting_timestamp;
    sm_chan_list_t                  radio_chan_list;
} sm_stats_request_t;

/* functions */
int sm_setup_monitor(void);
int sm_cancel_monitor(void);

int sm_init_nl(struct sm_cxt *cxt);

bool sm_mqtt_init(void);
void sm_mqtt_set(const char *broker, const char *port, const char *topic, const char *qos, int compress);
void sm_mqtt_stop(void);

#ifdef USE_CAPACITY_QUEUE_STATS
/******************************************************************************
 *  CAPACITY REPORT definitions
 *****************************************************************************/
bool sm_capacity_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request);
bool sm_capacity_report_radio_change(
        radio_entry_t              *radio_cfg);
#endif

/******************************************************************************
 *  DEVICE REPORT definitions
 *****************************************************************************/
bool sm_device_report_request(
        sm_stats_request_t         *request);

/******************************************************************************
 *  SURVEY_REPORT definitions
 *****************************************************************************/
bool sm_survey_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request);
bool sm_survey_report_radio_change(
        radio_entry_t              *radio_cfg);

/******************************************************************************
 *  CLIENT REPORT definitions
 *****************************************************************************/
bool sm_client_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request);
bool sm_client_report_radio_change(
        radio_entry_t              *radio_cfg);

/******************************************************************************
 *  NEIGHBOR REPORT definitions
 *****************************************************************************/
bool sm_neighbor_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request);
bool sm_neighbor_report_radio_change (
        radio_entry_t              *radio_cfg);

/* Special case update neighbor through survey to minimize scans */
bool sm_neighbor_stats_results_update(
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type,
        int                         success);

/******************************************************************************
 *  RSSI definitions
 *****************************************************************************/
bool sm_rssi_report_request(
        radio_entry_t              *radio_cfg,
        sm_stats_request_t         *request);
bool sm_rssi_report_radio_change (
        radio_entry_t              *radio_cfg);

/* Special update for neighbor and client RSSI updates */
bool sm_rssi_stats_results_update(
        radio_entry_t              *radio_cfg,
        mac_address_t               mac,
        uint32_t                    rssi,
        uint64_t                    rx_ppdus,
        uint64_t                    tx_ppdus,
        rssi_source_t               source);
bool sm_rssi_is_reporting_enabled (
        radio_entry_t              *radio_cfg);

/******************************************************************************
 *  RADIO definitions
 *****************************************************************************/
bool sm_radio_config_enable_tx_stats(
        radio_entry_t              *radio_cfg);
bool sm_radio_config_enable_fast_scan(
        radio_entry_t              *radio_cfg);

/******************************************************************************
 *  SCAN SCHED definitions
 *****************************************************************************/
typedef void (*sm_scan_cb_t)(
        void                       *scan_ctx,
        int                         status);

typedef struct {
    radio_entry_t                  *radio_cfg;
    uint32_t                       *chan_list;
    uint32_t                        chan_num;
    radio_scan_type_t               scan_type;
    int32_t                         dwell_time;
    sm_scan_cb_t                    scan_cb;
    void                           *scan_ctx;
    dpp_neighbor_report_data_t     *scan_results;
} sm_scan_request_t;

typedef struct {
    sm_scan_request_t               scan_request;
    ds_dlist_node_t                 node;
} sm_scan_ctx_t;

bool sm_scan_schedule(sm_scan_request_t *scan_request);

bool sm_scan_schedule_init();

bool sm_scan_schedule_stop (
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type);

/******************************************************************************
 *  OVSDB WRAPPER definitions
 *****************************************************************************/
/*
 * Wrappers around Schema structures -- we need this in order to properly
 * insert them into the red-black tree.
 */

typedef enum
{
    STS_REPORT_NEIGHBOR,
    STS_REPORT_SURVEY,
    STS_REPORT_CLIENT,
    STS_REPORT_CAPACITY,
    STS_REPORT_RADIO,
    STS_REPORT_ESSID,
    STS_REPORT_DEVICE,
    STS_REPORT_RSSI,
    STS_REPORT_MAX,
    STS_REPORT_ERROR = STS_REPORT_MAX
} sm_report_type_t;

typedef struct
{
    struct schema_Wifi_Radio_State  schema;
    bool                            init;
    radio_entry_t                   config;

    ds_tree_node_t                  node;
} sm_radio_state_t;

typedef struct
{
    struct schema_Wifi_VIF_State    schema;

    ds_tree_node_t                  node;
} sm_vif_state_t;

typedef struct
{
    struct schema_Wifi_Stats_Config schema;
    sm_report_type_t                sm_report_type;
    report_type_t                   report_type;
    radio_type_t                    radio_type;
    radio_scan_type_t               scan_type;

    ds_tree_node_t                  node;
} sm_stats_config_t;

ds_tree_t *sm_radios_get();


void sm_vif_whitelist_get(char **mac_list, uint16_t *mac_size, uint16_t *mac_qty);

void sm_sanity_check_report_timestamp(
        const char *log_prefix,
        uint64_t    timestamp_ms,
        uint64_t   *reporting_timestamp,
        uint64_t   *report_ts);

#endif /* __STATS_PRIV_H__ */
