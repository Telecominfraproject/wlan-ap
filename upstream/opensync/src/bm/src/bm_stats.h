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
 * Band Steering Manager - Stats
 */

#ifndef __BM_STATS_H__
#define __BM_STATS_H__

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

/*****************************************************************************/

typedef enum
{
    BM_STATS_STEERING = 0,
    BM_STATS_RSSI,
    BM_STATS_ERROR
} bm_stats_type_t;

typedef struct
{
    bm_stats_type_t                 stats_type;
    ev_timer                        timer;
    bool                            initialized;
    char                            uuid[OVSDB_UUID_LEN];
    ds_tree_node_t                  tnode;

    /* Common SM structure */
    radio_entry_t                   radio_cfg;
    radio_type_t                    radio_type;
    report_type_t                   report_type;
    int                             reporting_interval;
    int                             reporting_count;
    uint64_t                        reporting_timestamp;
    int                             sampling_interval;
    int                             threshold_util;
    int                             threshold_max_delay;
    int                             threshold_pod_qty;
    int                             threshold_pod_num;
    ds_dlist_node_t                 node;

} bm_stats_request_t;

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

static inline char *bm_timestamp_ms_to_date (uint64_t   timestamp_ms)
{
    struct tm      *dt;
    time_t          t = timestamp_ms / 1000;
    static char     b[32];

    dt = localtime((time_t *)&t);

    memset (b, 0, sizeof(b));
    strftime(b, sizeof(b), "%F %T%z", dt);

    return b;
}

/*****************************************************************************
 * GLOBAL BM STATS
 *****************************************************************************/

bool    bm_stats_init( struct ev_loop *loop );
bool    bm_stats_cleanup( void );

void    bm_stats_add_event_to_report(
            bm_client_t *client,
            bsal_event_t *event,
            dpp_bs_client_event_type_t bs_event,
            bool backoff_enabled );

void    bm_stats_remove_client_from_report( bm_client_t *client );
int     bm_stats_get_stats_report_interval( void );

void    bm_stats_map_radio_type(bsal_band_t band, const char *ifname);

/*****************************************************************************
 * RSSI BM STATS
 *****************************************************************************/

bool bm_stats_rssi_report_request(
        radio_entry_t              *radio_cfg,
        bm_stats_request_t    *request);

bool bm_stats_rssi_report_radio_change(
        radio_entry_t              *radio_cfg);

bool bm_stats_rssi_is_reporting_enabled (
        radio_entry_t              *radio_cfg);

bool bm_stats_rssi_stats_results_update(
        radio_entry_t              *radio_cfg,
        mac_address_t               mac,
        uint32_t                    rssi,
        rssi_source_t               source);

#endif /* __BM_STATS_H__ */
