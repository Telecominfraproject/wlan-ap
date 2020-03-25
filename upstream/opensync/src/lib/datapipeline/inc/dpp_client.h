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

#ifndef __HAVE_DPP_CLIENT_H__
#define __HAVE_DPP_CLIENT_H__

#include "ds.h"
#include "ds_dlist.h"

#include "dpp_types.h"

/* MCS/NSS/BW rate table and indexes that should be used for supported rates
   ----------------------------------------------
   | type | bw         | nss        |  mcs
   ----------------------------------------------
   | OFDM | 0 (20Mhz)  | 0 (legacy) |  0 - 6M
   |      |            |            |  1 - 9M
   |      |            |            |  2 - 12M
   |      |            |            |  3 - 18M
   |      |            |            |  4 - 24M
   |      |            |            |  5 - 36M
   |      |            |            |  6 - 48M
   |      |            |            |  7 - 54M
   ----------------------------------------------
   | CCK  | 0 (20Mhz)  | 0 (legacy) |  8 - L1M
   |      |            |            |  9 - L2M
   |      |            |            | 10 - L5.5M
   |      |            |            | 11 - L11M
   |      |            |            | 12 - S2M
   |      |            |            | 13 - S5.5M
   |      |            |            | 14 - S11M"
   ----------------------------------------------
   | VHT  | 0 (20Mhz)  | 1 (chain1) |  1 - HT/VHT
   |      | 1 (40Mhz)  | ...        |  2 - HT/VHT
   |      | 2 (80MHz)  | 8 (chain8) |  3 - HT/VHT
   |      | 3 (160MHz) |            |  4 - HT/VHT
   |      |            |            |  5 - HT/VHT
   |      |            |            |  6 - HT/VHT
   |      |            |            |  7 - HT/VHT
   |      |            |            |  8 - VHT
   |      |            |            |  9 - VHT
   ----------------------------------------------
NOTE: The size of this table on 4x4 can be big - we could send only non zero elements!
*/
struct __dpp_client_stats_rxtx /* per-RX or per-TX rate-stats base-struct */
{
    uint32_t                        mcs;
    uint32_t                        nss;
    client_mcs_width_t              bw;
    uint64_t                        bytes;
    uint64_t                        msdu;
    uint64_t                        mpdu;
    uint64_t                        ppdu;
    uint64_t                        retries;
    uint64_t                        errors;
    int32_t                         rssi;     /* Used for RX only.  */
    ds_dlist_node_t                 node;
};

typedef struct __dpp_client_stats_rxtx   dpp_client_stats_rx_t;
typedef struct __dpp_client_stats_rxtx   dpp_client_stats_tx_t;


static inline dpp_client_stats_rx_t * dpp_client_stats_rx_record_alloc()
{
    dpp_client_stats_rx_t *record = NULL;

    record = malloc(sizeof(dpp_client_stats_rx_t));
    if (record) {
        memset(record, 0, sizeof(dpp_client_stats_rx_t));
    }

    return record;
}

static inline void dpp_client_stats_rx_record_free(dpp_client_stats_rx_t *record)
{
    if (NULL != record) {
        free(record);
    }
}


static inline dpp_client_stats_tx_t * dpp_client_stats_tx_record_alloc()
{
    dpp_client_stats_tx_t *record = NULL;

    record = malloc(sizeof(dpp_client_stats_tx_t));
    if (record) {
        memset(record, 0, sizeof(dpp_client_stats_tx_t));
    }

    return record;
}

static inline void dpp_client_stats_tx_record_free(dpp_client_stats_tx_t *record)
{
    if (NULL != record) {
        free(record);
    }
}


#define CLIENT_MAX_TID_RECORDS      (16)
/* AC/TID rate table
    ----------------------
    |    TID   |    AC    |
    -----------------------
    |  0  | 8  |    BE    |
    |  1  | 9  |    BK    |
    |  2  | 10 |    BK    |
    |  3  | 11 |    BE    |
    |  4  | 12 |    VI    |
    |  5  | 13 |    VI    |
    |  6  | 14 |    VO    |
    |  7  | 15 |    VO    |
    ----------------------- */
typedef struct
{
    radio_queue_type_t              ac;
    uint32_t                        tid;
    uint64_t                        ewma_time_ms;
    uint64_t                        sum_time_ms;
    uint64_t                        num_msdus;
} dpp_client_stats_tid_t;

typedef struct
{
    dpp_client_stats_tid_t          entry[CLIENT_MAX_TID_RECORDS];
    uint64_t                        timestamp_ms;
    ds_dlist_node_t                 node;
} dpp_client_tid_record_list_t;

static inline dpp_client_tid_record_list_t * dpp_client_tid_record_alloc()
{
    dpp_client_tid_record_list_t *record = NULL;

    record = malloc(sizeof(dpp_client_tid_record_list_t));
    if (record) {
        memset(record, 0, sizeof(dpp_client_tid_record_list_t));
    }

    return record;
}

static inline void dpp_client_tid_record_free(dpp_client_tid_record_list_t *record)
{
    if (NULL != record) {
        free(record);
    }
}

typedef struct
{
    uint64_t                        bytes_tx;
    uint64_t                        bytes_rx;
    uint64_t                        frames_tx;
    uint64_t                        frames_rx;
    uint64_t                        retries_rx;
    uint64_t                        retries_tx;
    uint64_t                        errors_rx;
    uint64_t                        errors_tx;
    double                          rate_rx;
    double                          rate_tx;
    int32_t                         rssi;
} dpp_client_stats_t;

typedef void (*dpp_client_report_cb_t)(
		radio_type_t    radio_type);

typedef struct
{
    radio_type_t                    type;
    mac_address_t                   mac;
    ifname_t                        ifname;
    radio_essid_t                   essid;
} dpp_client_info_t;

#define DPP_TARGET_CLIENT_RECORD_COMMON_STRUCT \
    struct { \
        ds_dlist_node_t node; \
        dpp_client_info_t info; \
        uint64_t stats_cookie; \
    }

typedef struct
{
    /* General client data (All targets must provide same) */
    dpp_client_info_t               info;

    /* Statistics client data */
    dpp_client_stats_t              stats;
    ds_dlist_t                      stats_rx;       /* dpp_client_stats_rx_t */
    ds_dlist_t                      stats_tx;       /* dpp_client_stats_tx_t */
    ds_dlist_t                      tid_record_list;/* dpp_client_tid_record_list_t */

    /* Connectivity client data */
    uint32_t                        is_connected;
    uint32_t                        connected;
    uint32_t                        disconnected;
    uint64_t                        connect_ts;
    uint64_t                        disconnect_ts;
    uint64_t                        duration_ms;

    uint32_t                        uapsd;
    ds_dlist_node_t                 node;
} dpp_client_record_t;

dpp_client_record_t * dpp_client_record_alloc();

static inline void dpp_client_record_free(dpp_client_record_t *record)
{
    if (NULL != record) {
        free(record);
    }
}

typedef struct
{
    radio_type_t                    radio_type;
    uint32_t                        channel;
    uint64_t                        timestamp_ms;
    ds_dlist_t                      list;       /* dpp_client_record_t */
} dpp_client_report_data_t;

#endif /* __HAVE_DPP_CLIENT_H__ */
