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

#ifndef __HAVE_DPP_BS_CLIENT_H__
#define __HAVE_DPP_BS_CLIENT_H__

#include "ds.h"
#include "ds_dlist.h"
#include "dpp_types.h"


#define DPP_MAX_BS_EVENT_RECORDS    60
#define DPP_MAX_BS_BANDS            2

// proto: BSEventType
typedef enum
{
    PROBE                   = 0,
    CONNECT,
    DISCONNECT,
    BACKOFF,
    ACTIVITY,
    OVERRUN,
    BAND_STEERING_ATTEMPT,
    CLIENT_STEERING_ATTEMPT,
    CLIENT_STEERING_STARTED,
    CLIENT_STEERING_DISABLED,
    CLIENT_STEERING_EXPIRED,
    CLIENT_STEERING_FAILED,
    AUTH_BLOCK,
    CLIENT_KICKED,
    CLIENT_BS_BTM,
    CLIENT_STICKY_BTM,
    CLIENT_BTM,
    CLIENT_CAPABILITIES,
    CLIENT_BS_BTM_RETRY,
    CLIENT_STICKY_BTM_RETRY,
    CLIENT_BTM_RETRY,
    CLIENT_RRM_BCN_RPT,
    CLIENT_BS_KICK,
    CLIENT_STICKY_KICK,
    CLIENT_SPECULATIVE_KICK,
    CLIENT_DIRECTED_KICK,
    CLIENT_GHOST_DEVICE_KICK,
    MAX_EVENTS
} dpp_bs_client_event_type_t;

// proto: DisconnectSrc
typedef enum
{
    LOCAL                   = 0,
    REMOTE,
    MAX_DISCONNECT_SOURCES
} dpp_bs_client_disconnect_src_t;

// proto: DisconnectType
typedef enum
{
    DISASSOC                = 0,
    DEAUTH,
    MAX_DISCONNECT_TYPES
} dpp_bs_client_disconnect_type_t;


typedef struct
{
    dpp_bs_client_event_type_t      type;
    uint64_t                        timestamp_ms;
    uint32_t                        rssi;
    uint32_t                        probe_bcast;
    uint32_t                        probe_blocked;
    dpp_bs_client_disconnect_src_t  disconnect_src;
    dpp_bs_client_disconnect_type_t disconnect_type;
    uint32_t                        disconnect_reason;
    bool                            backoff_enabled;
    bool                            active;
    bool                            rejected;
    bool                            is_BTM_supported;
    bool                            is_RRM_supported;
    bool                            band_cap_2G;
    bool                            band_cap_5G;
    uint32_t                        max_chwidth;
    uint32_t                        max_streams;
    uint32_t                        phy_mode;
    uint32_t                        max_MCS;
    uint32_t                        max_txpower;
    bool                            is_static_smps;
    bool                            is_mu_mimo_supported;
    bool                            rrm_caps_link_meas;
    bool                            rrm_caps_neigh_rpt;
    bool                            rrm_caps_bcn_rpt_passive;
    bool                            rrm_caps_bcn_rpt_active;
    bool                            rrm_caps_bcn_rpt_table;
    bool                            rrm_caps_lci_meas;
    bool                            rrm_caps_ftm_range_rpt;
    uint32_t                        backoff_period;
} dpp_bs_client_event_record_t;

typedef struct
{
    radio_type_t                    type;
    bool                            connected;
    uint32_t                        rejects;
    uint32_t                        connects;
    uint32_t                        disconnects;
    uint32_t                        activity_changes;
    uint32_t                        steering_success_cnt;
    uint32_t                        steering_fail_cnt;
    uint32_t                        steering_kick_cnt;
    uint32_t                        sticky_kick_cnt;
    uint32_t                        probe_bcast_cnt;
    uint32_t                        probe_bcast_blocked;
    uint32_t                        probe_direct_cnt;
    uint32_t                        probe_direct_blocked;
    uint32_t                        num_event_records;
    dpp_bs_client_event_record_t    event_record[DPP_MAX_BS_EVENT_RECORDS];
} dpp_bs_client_band_record_t;

typedef struct
{
    mac_address_t                   mac;
    uint32_t                        num_band_records;
    dpp_bs_client_band_record_t     band_record[DPP_MAX_BS_BANDS];
} dpp_bs_client_record_t;

typedef struct
{
    dpp_bs_client_record_t          entry;
    ds_dlist_node_t                 node;
} dpp_bs_client_record_list_t;

typedef ds_dlist_t                  dpp_bs_client_list_t;

static inline dpp_bs_client_record_list_t *dpp_bs_client_record_alloc()
{
    dpp_bs_client_record_list_t *record = NULL;

    record = malloc( sizeof( dpp_bs_client_record_list_t ) );
    if( record )
    {
        memset( record, 0, sizeof( dpp_bs_client_record_t ) );
    }

    return record;
}

static inline void dpp_bs_client_record_free( dpp_bs_client_record_list_t *record )
{
    if( record != NULL )
    {
        free( record );
    }
}

typedef struct
{
    uint64_t                        timestamp_ms;
    dpp_bs_client_list_t            list;
} dpp_bs_client_report_data_t;

#endif  /* __HAVE_DPP_BS_CLIENT_H__ */
