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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <linux/types.h>
#include <inttypes.h>

#include "os_time.h"
#include "bm.h"
#include "qm_conn.h"
#include "osa_assert.h"

#include "bm_stats.h"

/*****************************************************************************/

#define MODULE_ID LOG_MODULE_ID_STATS

#define STATS_VERBOSE_DEBUG_LEVEL        2

/* Wifi_Stats_Config stats_type */
#define BM_STATS_TYPE_STEERING          "steering"
#define BM_STATS_TYPE_RSSI              "rssi"

#define BM_STATS_QM_SEND_INTERVAL        5.0
#define BM_STATS_DEFAULT_REPORTING_TIME  60

/*****************************************************************************/

static struct ev_loop              *g_bm_stats_evloop = NULL;
static dpp_bs_client_report_data_t  g_bm_stats_steering_report;
static ovsdb_update_monitor_t       g_bm_stats_ovsdb_update;
static struct ev_timer              g_bm_stats_qm_send_timer;

static bm_stats_request_t           g_bm_stats_default_request;

static radio_type_t                 g_bm_stats_2g_radio_type = RADIO_TYPE_2G;
static radio_type_t                 g_bm_stats_5g_radio_type = RADIO_TYPE_5G;

static ds_tree_t bm_stats_table =
DS_TREE_INIT(
        (ds_key_cmp_t *)strcmp,
        bm_stats_request_t,
        tnode);

/*****************************************************************************/

static void bm_stats_steering_clear_all_stats();
static void bm_stats_steering_clear_all_event_records();

/*****************************************************************************/

static char*
bm_stats_get_event_to_str( dpp_bs_client_event_type_t event )
{
    char *str = NULL;

    switch( event )
    {
        case PROBE:
            str = "PROBE";
            break;

        case CONNECT:
            str = "CONNECT";
            break;

        case DISCONNECT:
            str = "DISCONNECT";
            break;

        case BACKOFF:
            str = "BACKOFF";
            break;

        case ACTIVITY:
            str = "ACTIVITY";
            break;

        case OVERRUN:
            str = "OVERRUN";
            break;

        case BAND_STEERING_ATTEMPT:
            str = "BAND_STEERING_ATTEMPT";
            break;

        case CLIENT_STEERING_ATTEMPT:
            str = "CLIENT_STEERING_ATTEMPT";
            break;

        case CLIENT_STEERING_STARTED:
            str = "CLIENT_STEERING_STARTED";
            break;

        case CLIENT_STEERING_DISABLED:
            str = "CLIENT_STEERING_DISABLED";
            break;

        case CLIENT_STEERING_EXPIRED:
            str = "CLIENT_STEERING_EXPIRED";
            break;

        case CLIENT_STEERING_FAILED:
            str = "CLIENT_STEERING_FAILED";
            break;

        case CLIENT_KICKED:
            str = "CLIENT_KICKED";
            break;

        case AUTH_BLOCK:
            str = "AUTH_BLOCK";
            break;

        case CLIENT_BS_BTM:
            str = "CLIENT_BS_BTM";
            break;

        case CLIENT_STICKY_BTM:
            str = "CLIENT_STICKY_BTM";
            break;

        case CLIENT_BTM:
            str = "CLIENT_BTM";
            break;

        case CLIENT_CAPABILITIES:
            str = "CLIENT_CAPABILITIES";
            break;
        
        case CLIENT_BS_BTM_RETRY:
            str = "CLIENT_BS_BTM_RETRY";
            break;

        case CLIENT_STICKY_BTM_RETRY:
            str = "CLIENT_STICKY_BTM_RETRY";
            break;

        case CLIENT_BTM_RETRY:
            str = "CLIENT_BTM_RETRY";
            break;

        case CLIENT_RRM_BCN_RPT:
            str = "CLIENT_RRM_BCN_RPT";
            break;

        case CLIENT_BS_KICK:
            str = "CLIENT_BS_KICK";
            break;

        case CLIENT_STICKY_KICK:
            str = "CLIENT_STICKY_KICK";
            break;

        case CLIENT_SPECULATIVE_KICK:
            str = "CLIENT_SPECULATIVE_KICK";
            break;

        case CLIENT_DIRECTED_KICK:
            str = "CLIENT_DIRECTED_KICK";
            break;

        case CLIENT_GHOST_DEVICE_KICK:
            str = "CLIENT_GHOST_DEVICE_KICK";
            break;

        default:
            str = "NONE";
            break;
    }

    return str;
}

static char*
bm_stats_get_disconnect_src_to_str( dpp_bs_client_disconnect_src_t src )
{
    char *str = NULL;

    switch( src )
    {
        case LOCAL:
            str = "LOCAL";
            break;

        case REMOTE:
            str = "REMOTE";
            break;

        default:
            str = "NONE";
            break;
    }

    return str;
}

static char*
bm_stats_get_disconnect_type_to_str( dpp_bs_client_disconnect_type_t type )
{
    char *str = NULL;

    switch( type )
    {
        case DISASSOC:
            str = "DISASSOC";
            break;

        case DEAUTH:
            str = "DEAUTH";
            break;

        default:
            str = "NONE";
            break;
    }

    return str;
}

static radio_type_t
bm_stats_get_band_type( bsal_band_t band )
{
    radio_type_t type = RADIO_TYPE_NONE;

    switch( band )
    {
        case BSAL_BAND_24G:
            type = g_bm_stats_2g_radio_type;
            break;

        case BSAL_BAND_5G:
            type = g_bm_stats_5g_radio_type;
            break;

        default:
            break;
    }

    return type;
}

static dpp_bs_client_disconnect_src_t
bm_stats_get_disconnect_src( bsal_event_t *event )
{
    if( event->data.disconnect.source == BSAL_DISC_SOURCE_LOCAL )
    {
        return LOCAL;
    }

    return REMOTE;
}

static dpp_bs_client_disconnect_type_t
bm_stats_get_disconnect_type( bsal_event_t *event )
{
    if( event->data.disconnect.type == BSAL_DISC_TYPE_DISASSOC )
    {
        return DISASSOC;
    }

    return DEAUTH;
}

/******************************************************************************
 *  Band/Client Steering Stats definitions
 *****************************************************************************/

static void
bm_stats_steering_remove_all_clients()
{
    dpp_bs_client_list_t        *bs_client_list = &g_bm_stats_steering_report.list;
    dpp_bs_client_record_list_t *bs_client      = NULL;
    ds_dlist_iter_t             bs_client_iter;

    for( bs_client = ds_dlist_ifirst( &bs_client_iter, bs_client_list );
         bs_client != NULL;
         bs_client = ds_dlist_inext( &bs_client_iter ) )
    {
        ds_dlist_iremove( &bs_client_iter );
        dpp_bs_client_record_free( bs_client );
        bs_client = NULL;
    }

    return;
}

/* Very verbose - use for debugging only!! */
static void
bm_stats_steering_print_all_records()
{
    dpp_bs_client_list_t            *bs_client_list   = &g_bm_stats_steering_report.list;
    dpp_bs_client_record_list_t     *bs_client        = NULL;
    dpp_bs_client_record_t          *bs_client_entry  = NULL;
    ds_dlist_iter_t                 bs_client_iter;

    dpp_bs_client_band_record_t     *band_rec         = NULL;
    dpp_bs_client_event_record_t    *event_rec        = NULL;

    int                             i,j;

    LOGT( "===== Printing all BS client records ======" );

    for( bs_client = ds_dlist_ifirst( &bs_client_iter, bs_client_list );
         bs_client != NULL;
         bs_client = ds_dlist_inext( &bs_client_iter ) )
    {
        bs_client_entry = &bs_client->entry;

        if( !bs_client_entry ) {
            LOGE( "BS Client entry is NULL" );
            continue;
        }

        LOGT( "------------------------------------------------------" );
        LOGT( "BS Client entry mac is " MAC_ADDRESS_FORMAT " and num_band_records = %d",
               MAC_ADDRESS_PRINT( bs_client_entry->mac ), bs_client_entry->num_band_records );

        for( i = 0; i < BSAL_BAND_COUNT ; i++ )
        {
            band_rec = &bs_client_entry->band_record[i];

            if( !band_rec ) {
                LOGE( "Band record is NULL" );
                continue;
            }

            LOGT( "\n");

            LOGT( " ----- Band %s stats ------ ", ( i == BSAL_BAND_24G ) ? "2.4G" : "5G" );
            LOGT( "Connected            = %s", band_rec->connected ? "Yes" : "No"  );
            LOGT( "Rejects              = %d", band_rec->rejects );
            LOGT( "Connects             = %d", band_rec->connects );
            LOGT( "disconnects          = %d", band_rec->disconnects );
            LOGT( "activity_changes     = %d", band_rec->activity_changes );
            LOGT( "steering_success_cnt = %d", band_rec->steering_success_cnt );
            LOGT( "steering_fail_cnt    = %d", band_rec->steering_fail_cnt );
            LOGT( "steering_kick_cnt    = %d", band_rec->steering_kick_cnt );
            LOGT( "sticky_kick_cnt      = %d", band_rec->sticky_kick_cnt );
            LOGT( "Probe bcast cnt      = %d", band_rec->probe_bcast_cnt );
            LOGT( "Probe bcast blocked  = %d", band_rec->probe_bcast_blocked );
            LOGT( "Probe direct cnt     = %d", band_rec->probe_direct_cnt );
            LOGT( "Probe direct blocked = %d\n", band_rec->probe_direct_blocked );

            LOGT( "Total event records  = %d", band_rec->num_event_records );

            for( j = 0; j < (int)band_rec->num_event_records ; j++ )
            {
                event_rec = &band_rec->event_record[j];

                if( !event_rec ) {
                    LOGE( "Event record is NULL" );
                    continue;
                }

                LOGT( "\n" );

                LOGT( "// ------------------------ // " );
                LOGT( "Event record number  = %d", j);
                LOGT( "Type                 = %s", bm_stats_get_event_to_str( event_rec->type ) );
                LOGT( "timestamp_ms         = %"PRIu64"", event_rec->timestamp_ms );
                LOGT( "rssi                 = %d", event_rec->rssi );
                LOGT( "probe bcast          = %d", event_rec->probe_bcast );
                LOGT( "probe blocked        = %d", event_rec->probe_blocked );
                LOGT( "disconnect src       = %s", bm_stats_get_disconnect_src_to_str( event_rec->disconnect_src ) );
                LOGT( "disconnect type      = %s", bm_stats_get_disconnect_type_to_str( event_rec->disconnect_type ) );
                LOGT( "backoff enabled      = %s", event_rec->backoff_enabled ? "Yes" : "No" );
                LOGT( "backoff period       = %d", event_rec->backoff_period);
                LOGT( "active               = %s", event_rec->active ? "Yes" : "No" );
                LOGT( "auth rejected        = %s", event_rec->rejected ? "Yes" : "No" );
                LOGT( "is_BTM_supported     = %s", event_rec->is_BTM_supported ? "Yes" : "No" );
                LOGT( "is_RRM_supported     = %s", event_rec->is_RRM_supported ? "Yes" : "No" );
                LOGT( "2G capable           = %s", event_rec->band_cap_2G ? "Yes" : "No" );
                LOGT( "5G capable           = %s", event_rec->band_cap_5G ? "Yes" : "No" );
                LOGT( "Max Channel Width    = %d", event_rec->max_chwidth );
                LOGT( "Max Streams          = %d", event_rec->max_streams );
                LOGT( "PHY Mode             = %d", event_rec->phy_mode );
                LOGT( "Max MCS              = %d", event_rec->max_MCS );
                LOGT( "Max TX Power         = %d", event_rec->max_txpower );
                LOGT( "Is Static SMPS       = %s", event_rec->is_static_smps ? "Yes" : "No" );
                LOGT( "Supports MU-MIMO     = %s", event_rec->is_mu_mimo_supported ? "Yes" : "No" );
                LOGT( "RRM Link Measurement = %s", event_rec->rrm_caps_link_meas ? "Yes" : "No" );
                LOGT( "RRM Neighbor Report  = %s", event_rec->rrm_caps_neigh_rpt ? "Yes" : "No" );
                LOGT( "RRM Bcn Rpt Passive  = %s", event_rec->rrm_caps_bcn_rpt_passive ? "Yes" : "No" );
                LOGT( "RRM Bcn Rpt Active   = %s", event_rec->rrm_caps_bcn_rpt_active ? "Yes" : "No" );
                LOGT( "RRM Bcn Rpt Table    = %s", event_rec->rrm_caps_bcn_rpt_table ? "Yes" : "No" );
                LOGT( "RRM LCI measurement  = %s", event_rec->rrm_caps_lci_meas ? "Yes" : "No" );
                LOGT( "RRM FTM Range Rpt    = %s", event_rec->rrm_caps_ftm_range_rpt ? "Yes" : "No" );
                LOGT( " // ------------------------ // " );
            }

            LOGT( "---------------------------------" );
        }

        LOGT( " ----- End Client specific stats ----- " );
    }
}

static dpp_bs_client_record_t *
bm_stats_steering_get_bs_client_record( bm_client_t *client )
{
    dpp_bs_client_list_t        *bs_client_list     = &g_bm_stats_steering_report.list;
    dpp_bs_client_record_list_t *bs_client          = NULL;
    dpp_bs_client_record_t      *bs_client_entry    = NULL;
    ds_dlist_iter_t              bs_client_iter;

    mac_address_t mac_addr;

    for( bs_client = ds_dlist_ifirst( &bs_client_iter, bs_client_list );
         bs_client != NULL;
         bs_client = ds_dlist_inext( &bs_client_iter ) )
    {
        bs_client_entry = &bs_client->entry;

        if( !os_nif_macaddr_from_str((os_macaddr_t *)&mac_addr, client->mac_addr ) ) {
            LOGE( "Failed to parse mac address '%s'", client->mac_addr );
        }

        if( MAC_ADDR_EQ( bs_client_entry->mac, mac_addr ) ) {
            return bs_client_entry;
        }
    }

    return NULL;
}

static void
bm_stats_steering_process_stats( void )
{
    bm_client_t                 *client;
    bm_client_stats_t           *stats;

    dpp_bs_client_record_t      *bs_client_entry    = NULL;
    dpp_bs_client_band_record_t *band_rec           = NULL;

    int                         i;

    ds_tree_t *bm_clients = bm_client_get_tree();

    // Copy the stats into the super report
    ds_tree_foreach( bm_clients, client )
    {
        bs_client_entry = bm_stats_steering_get_bs_client_record( client );
        if( !bs_client_entry ) {
            continue;
        }

        for( i = 0; i < BSAL_BAND_COUNT ; i++ )
        {
            band_rec                        = &bs_client_entry->band_record[i];
            stats                           = &client->stats[i];

            band_rec->type                  = bm_stats_get_band_type( i );

            // Check if the client is connected on this band
            if( (int)client->band == i ) {
                band_rec->connected         = client->connected;
            } else {
                band_rec->connected         = 0;
            }

            band_rec->rejects               = stats->rejects;
            band_rec->connects              = stats->connects;
            band_rec->disconnects           = stats->disconnects;
            band_rec->activity_changes      = stats->activity_changes;
            band_rec->steering_success_cnt  = stats->steering_success_cnt;
            band_rec->steering_fail_cnt     = stats->steering_fail_cnt;
            band_rec->steering_kick_cnt     = stats->steering_kick_cnt;
            band_rec->sticky_kick_cnt       = stats->sticky_kick_cnt;
            band_rec->probe_bcast_cnt       = stats->probe.null_cnt;
            band_rec->probe_bcast_blocked   = stats->probe.null_blocked;
            band_rec->probe_direct_cnt      = stats->probe.direct_cnt;
            band_rec->probe_direct_blocked  = stats->probe.direct_blocked;
        }
    }

    if( log_module_severity_get( LOG_MODULE_ID_STATS ) >=
            ( LOG_SEVERITY_TRACE ) ) {
        bm_stats_steering_print_all_records();
    }

    // Set the report timestamp_ms -- time when the report is being sent
    g_bm_stats_steering_report.timestamp_ms = clock_real_ms();
    LOGT( "Report timestamp = %"PRIu64"", g_bm_stats_steering_report.timestamp_ms );

    // Insert the report into DPP queue
    dpp_put_bs_client( &g_bm_stats_steering_report );

    // Remove all clients from the BS report.
    //  - If a client has events in the next stats_report_interval, it is
    //    automatically added into the report.
    //  - If a client has no events in the next stats_report_interval,
    //    empty record for that client will not be included in the report
    bm_stats_steering_remove_all_clients();

    // Reset the stats for the clients
    bm_stats_steering_clear_all_stats();

    return;
}

static void
bm_stats_steering_clear_all_event_records()
{
    dpp_bs_client_list_t        *bs_client_list     = &g_bm_stats_steering_report.list;
    dpp_bs_client_record_list_t *bs_client          = NULL;
    dpp_bs_client_record_t      *bs_client_entry    = NULL;
    ds_dlist_iter_t              bs_client_iter;

    dpp_bs_client_band_record_t  *band_rec          = NULL;
    dpp_bs_client_event_record_t *event_rec         = NULL;

    int                          i,j;

    for( bs_client = ds_dlist_ifirst( &bs_client_iter, bs_client_list );
         bs_client != NULL;
         bs_client = ds_dlist_inext( &bs_client_iter ) )
    {
        bs_client_entry = &bs_client->entry;

        if( !bs_client_entry ) {
            LOGE( "BS Client entry is NULL" );
            continue;
        }

        for( i = 0; i < BSAL_BAND_COUNT ; i++ )
        {
            band_rec = &bs_client_entry->band_record[i];

            if( !band_rec ) {
                LOGE( "Band record is NULL" );
                continue;
            }

            for( j = 0; j < (int)band_rec->num_event_records ; j++ )
            {
                event_rec = &band_rec->event_record[j];

                if( !event_rec ) {
                    LOGE( "Event record is NULL" );
                    continue;
                }

                memset( event_rec, 0, sizeof( dpp_bs_client_event_record_t ) );
            }

            band_rec->num_event_records = 0;
        }
    }
}

static void
bm_stats_steering_clear_all_stats( void )
{
    bm_client_t         *client;
    bm_client_stats_t   *stats;
    int                 i;

    ds_tree_t *bm_clients = bm_client_get_tree();

    ds_tree_foreach( bm_clients, client )
    {
        for( i = 0; i < BSAL_BAND_COUNT ; i++ )
        {
            stats = &client->stats[i];

            stats->rejects = 0;
            stats->connects = 0;
            stats->disconnects = 0;
            stats->activity_changes = 0;

            stats->steering_success_cnt = 0;
            stats->steering_fail_cnt = 0;
            stats->steering_kick_cnt = 0;
            stats->sticky_kick_cnt = 0;

            stats->probe.null_cnt = 0;
            stats->probe.null_blocked = 0;
            stats->probe.direct_cnt = 0;
            stats->probe.direct_blocked = 0;

            stats->rssi.higher = 0;
            stats->rssi.lower = 0;
            stats->rssi.inact_higher = 0;
            stats->rssi.inact_lower = 0;
        }

        client->num_rejects_copy = 0;
    }

    return;
}

static dpp_bs_client_record_t *
bm_stats_steering_add_client_to_report( bm_client_t *client )
{
    dpp_bs_client_list_t        *bs_client_list     = &g_bm_stats_steering_report.list;
    dpp_bs_client_record_list_t *bs_client          = NULL;
    dpp_bs_client_record_t      *bs_client_entry    = NULL;

    int i;

    bs_client = dpp_bs_client_record_alloc();
    if( bs_client == NULL ) {
        LOGE( "BS Client record allocation failed" );
        return bs_client_entry;
    }

    bs_client_entry = &bs_client->entry;

    // Copy client mac and initialize number of bands and events to 0
    if( !os_nif_macaddr_from_str((os_macaddr_t *)&bs_client_entry->mac,
         client->mac_addr ) ) {
        LOGE( "Failed to parse mac address '%s'", client->mac_addr );
    }

    bs_client_entry->num_band_records = BSAL_BAND_COUNT;
    for( i = 0; i < BSAL_BAND_COUNT ; i++ )
    {
        bs_client_entry->band_record[i].num_event_records = 0;
    }

    LOGT( "Adding a new record for client: %s", client->mac_addr );
    ds_dlist_insert_tail( bs_client_list, bs_client );

    return bs_client_entry;
}

static void
bm_stats_steering_parse_event(
        bm_client_t                *client,
        bsal_event_t               *event,
        dpp_bs_client_event_type_t  bs_event,
        bool                        backoff_enabled,
        bsal_band_t                 band)
{
    dpp_bs_client_record_t         *bs_client_entry = NULL;

    dpp_bs_client_band_record_t    *band_rec   = NULL;
    dpp_bs_client_event_record_t   *event_rec = NULL;

    bm_stats_request_t             *request;
    ds_tree_iter_t                  iter;
    bool                            enabled = false;

    // By default stats are enabled!
    if (g_bm_stats_default_request.reporting_interval) {
        enabled = true;
    } else {
        // Here we should define which iface steering is enabled
        for (   request = ds_tree_ifirst(&iter, &bm_stats_table);
                request != NULL;
                request = ds_tree_inext(&iter)) {
            if (request->stats_type == BM_STATS_STEERING) {
                enabled = true;
                break;
            }
        }
    }

    if (!enabled) {
        LOGW("BM event (stats not enabled)");
        return;
    }

    bs_client_entry = bm_stats_steering_get_bs_client_record( client );
    if( !bs_client_entry ) {
        // Add the client to the report
        bs_client_entry = bm_stats_steering_add_client_to_report( client );
        if( !bs_client_entry ) {
            LOGE( "Unable to add new client %s to report", client->mac_addr );
            return;
        }
    }

    band_rec  = &bs_client_entry->band_record[band];
    event_rec = &band_rec->event_record[band_rec->num_event_records];

    if( band_rec->num_event_records >= DPP_MAX_BS_EVENT_RECORDS ) {
        LOGT( "Max events limit reached for client "MAC_ADDRESS_FORMAT""
              ", num_event_records = %d",
               MAC_ADDRESS_PRINT( bs_client_entry->mac ), band_rec->num_event_records );
        return;
    };

    // use timestamp specified in event if provided, otherwise use current time
    if (event && event->timestamp_ms) {
        event_rec->timestamp_ms = event->timestamp_ms;
    } else {
        event_rec->timestamp_ms = clock_real_ms();
    }

    switch( bs_event )
    {
        case PROBE:
            LOGT( "Adding PROBE event" );
            event_rec->type = PROBE;

            // We do this satisfy common api
            radio_entry_t radio_cfg;
            memset(&radio_cfg, 0, sizeof(radio_cfg));
            radio_cfg.type = bm_stats_get_band_type(band);
            if(bm_stats_rssi_is_reporting_enabled(&radio_cfg)) {
                mac_address_t mac;
                if( os_nif_macaddr_from_str((os_macaddr_t *)&mac,
                            client->mac_addr ) ) {
                    // Add it into RSSI report
                    bm_stats_rssi_stats_results_update(
                            &radio_cfg,
                            mac,
                            (uint32_t)event->data.probe_req.rssi,
                            RSSI_SOURCE_PROBE);
                } else {
                    LOGE( "Failed to parse mac address '%s'", client->mac_addr );
                }
            } else {
                event_rec->rssi = event->data.probe_req.rssi;
            }
            event_rec->probe_bcast = event->data.probe_req.ssid_null ? 1 : 0;
            event_rec->probe_blocked = event->data.probe_req.blocked ? 1 : 0;
            break;

        case CONNECT:
            LOGT( "Adding CONNECT event" );
            event_rec->type = CONNECT;
            break;

        case DISCONNECT:
            LOGT( "Adding DISCONNECT event" );
            event_rec->type = DISCONNECT;
            event_rec->disconnect_src = bm_stats_get_disconnect_src( event );
            event_rec->disconnect_type = bm_stats_get_disconnect_type( event );
            break;

        case ACTIVITY:
            LOGT( "Adding ACTIVITY event" );
            event_rec->type = ACTIVITY;
            event_rec->active = event->data.activity.active;
            break;

        case BACKOFF:
            LOGT("Adding BACKOFF event" );
            event_rec->type = BACKOFF;
            event_rec->backoff_enabled = backoff_enabled;
            event_rec->backoff_period = client->backoff_period_used;
            break;

        case BAND_STEERING_ATTEMPT:
            LOGT( "Adding Band_Steering_Attempt event" );
            event_rec->type = BAND_STEERING_ATTEMPT;
            break;

        case CLIENT_STEERING_ATTEMPT:
            LOGT( "Adding Client_Steering_Attempt event" );
            event_rec->type = CLIENT_STEERING_ATTEMPT;
            break;

        case CLIENT_STEERING_STARTED:
            LOGT( "Adding Client_Steering_Started event" );
            event_rec->type = CLIENT_STEERING_STARTED;
            break;

        case CLIENT_STEERING_DISABLED:
            LOGT( "Adding Client Steering Disabled event" );
            event_rec->type = CLIENT_STEERING_DISABLED;
            break;

        case CLIENT_STEERING_EXPIRED:
            LOGT( "Adding Client_Steering_Expired event" );
            event_rec->type = CLIENT_STEERING_EXPIRED;
            break;

        case CLIENT_STEERING_FAILED:
            LOGT( "Adding Client_Steering_Failed event" );
            event_rec->type = CLIENT_STEERING_FAILED;
            break;

        case CLIENT_KICKED:
            LOGT( "Adding Client_Kicked event" );
            event_rec->type = CLIENT_KICKED;
            break;

        case AUTH_BLOCK:
            LOGT( "Adding Auth_Block event" );
            event_rec->type     = AUTH_BLOCK;
            event_rec->rejected = event->data.auth_fail.bs_rejected ? 1 : 0;
            break;

        case CLIENT_BS_BTM:
            LOGT( "Adding Client_BS_BTM event" );
            event_rec->type = CLIENT_BS_BTM;
            break;

        case CLIENT_STICKY_BTM:
            LOGT( "Adding Client_STICKY_BTM event" );
            event_rec->type = CLIENT_STICKY_BTM;
            break;

        case CLIENT_BTM:
            LOGT( "Adding Client_BTM event" );
            event_rec->type = CLIENT_BTM;
            break;

        case CLIENT_CAPABILITIES:
            LOGT( "Adding Client_Capabilities event" );
            event_rec->type = CLIENT_CAPABILITIES;

            event_rec->is_BTM_supported = event->data.connect.is_BTM_supported ? 1 : 0;
            event_rec->is_RRM_supported = event->data.connect.is_RRM_supported ? 1 : 0;

            event_rec->band_cap_2G = event->data.connect.band_cap_2G ? 1 : 0;
            event_rec->band_cap_5G = event->data.connect.band_cap_5G ? 1 : 0;

            event_rec->max_chwidth = event->data.connect.datarate_info.max_chwidth;
            event_rec->max_streams = event->data.connect.datarate_info.max_streams;
            event_rec->phy_mode = event->data.connect.datarate_info.phy_mode;
            event_rec->max_MCS = event->data.connect.datarate_info.max_MCS;
            event_rec->max_txpower = event->data.connect.datarate_info.max_txpower;

            event_rec->is_static_smps = 
                            event->data.connect.datarate_info.is_static_smps? 1 : 0;
            event_rec->is_mu_mimo_supported = 
                            event->data.connect.datarate_info.is_mu_mimo_supported? 1 : 0;

            event_rec->rrm_caps_link_meas = event->data.connect.rrm_caps.link_meas? 1 : 0;
            event_rec->rrm_caps_neigh_rpt = event->data.connect.rrm_caps.neigh_rpt? 1 : 0;
            event_rec->rrm_caps_bcn_rpt_passive =
                                    event->data.connect.rrm_caps.bcn_rpt_passive ? 1 : 0;
            event_rec->rrm_caps_bcn_rpt_active = 
                                    event->data.connect.rrm_caps.bcn_rpt_active ? 1 : 0;
            event_rec->rrm_caps_bcn_rpt_table = 
                                    event->data.connect.rrm_caps.bcn_rpt_table ? 1 : 0;
            event_rec->rrm_caps_lci_meas = event->data.connect.rrm_caps.lci_meas ? 1 : 0;
            event_rec->rrm_caps_ftm_range_rpt =
                                    event->data.connect.rrm_caps.ftm_range_rpt ? 1 : 0;
            break;

        case CLIENT_BS_BTM_RETRY:
            LOGT( "Adding Client_BS_BTM_RETRY event" );
            event_rec->type = CLIENT_BS_BTM_RETRY;
            break;

        case CLIENT_STICKY_BTM_RETRY:
            LOGT( "Adding Client_STICKY_BTM_RETRY event" );
            event_rec->type = CLIENT_STICKY_BTM_RETRY;
            break;

        case CLIENT_BTM_RETRY:
            LOGT( "Adding Client_BTM_RETRY event" );
            event_rec->type = CLIENT_BTM_RETRY;
            break;

        case CLIENT_RRM_BCN_RPT:
            LOGT( "Adding Client_RRM_BCN_RPT event" );
            event_rec->type = CLIENT_RRM_BCN_RPT;
            break;

        case CLIENT_BS_KICK:
            LOGT( "Adding Client_BS_KICK event" );
            event_rec->type = CLIENT_BS_KICK;
            break;

        case CLIENT_STICKY_KICK:
            LOGT( "Adding Client_STICKY_KICK event" );
            event_rec->type = CLIENT_STICKY_KICK;
            break;

        case CLIENT_SPECULATIVE_KICK:
            LOGT( "Adding Client_SPECULATIVE_KICK event" );
            event_rec->type = CLIENT_SPECULATIVE_KICK;
            break;

        case CLIENT_DIRECTED_KICK:
            LOGT( "Adding Client_DIRECTED_KICK event" );
            event_rec->type = CLIENT_DIRECTED_KICK;
            break;

        case CLIENT_GHOST_DEVICE_KICK:
            LOGT( "Adding Client_GHOST_DEVICE_KICK event" );
            event_rec->type = CLIENT_GHOST_DEVICE_KICK;
            break;

        default:
            break;
    }

    band_rec->num_event_records++;

    if( band_rec->num_event_records == DPP_MAX_BS_EVENT_RECORDS ) {
        event_rec->type = OVERRUN;
        band_rec->num_event_records++;

        LOGW( "Max events limit reached for client "MAC_ADDRESS_FORMAT""
              ", adding OVERRUN event", MAC_ADDRESS_PRINT( bs_client_entry->mac ) );
    }
}

static void
bm_stats_ev_timer_cb(struct ev_loop *loop, ev_timer *watcher, int revents)
{
    (void)loop;
    (void)watcher;
    (void)revents;

    bm_stats_steering_process_stats();
}

static void
bm_stats_steering_report_request (bm_stats_request_t *request)
{
    // Stop default timer if new stats config is enabled
    if (g_bm_stats_default_request.reporting_interval) {
        ev_timer_stop(g_bm_stats_evloop, &g_bm_stats_default_request.timer);
        g_bm_stats_default_request.reporting_interval = 0;
    }

    if (request->reporting_interval > 0) {
        if (!request->initialized) {
            ev_init (&request->timer,
                    bm_stats_ev_timer_cb);
            request->initialized = true;
        }
        LOGI("BS stats start %s, repeat %d s",
                radio_get_name_from_type(request->radio_type),
                request->reporting_interval);

        ev_timer_stop(g_bm_stats_evloop, &request->timer);
        ev_timer_set (&request->timer,
                request->reporting_interval,
                request->reporting_interval);
        ev_timer_start(g_bm_stats_evloop, &request->timer);
    }
    else{

        LOGI("BS stats stop %s", radio_get_name_from_type(request->radio_type));

        ev_timer_stop(g_bm_stats_evloop, &request->timer);

        bm_stats_steering_remove_all_clients();
    }

}


/******************************************************************************
 *  MQTT definitions
 *****************************************************************************/

static void
bm_stats_steering_send_mqtt( void )
{
    static bool qm_err = false;
    uint32_t buf_len;
    uint8_t mqtt_buf[STATS_MQTT_BUF_SZ];
    qm_response_t res;

    // skip if empty queue
    if (dpp_get_queue_elements() <= 0) {
        LOGT( "Queue is empty, returning" );
        return;
    }

    // Publish statistics report to MQTT
    LOGT("Total %d elements queued for transmission.\n",
         dpp_get_queue_elements());

    // Do not report any stats if QM is not running
    if (!qm_conn_get_status(NULL)) {
        if (!qm_err) {
            // don't repeat same error
            LOGI("Cannot connect to QM (QM not running?)");
        }
        qm_err = true;
        return;
    }
    qm_err = false;

    while (dpp_get_queue_elements() > 0)
    {
        if (!dpp_get_report(mqtt_buf, sizeof(mqtt_buf), &buf_len))
        {
            LOGE("DPP: Get report failed.\n");
            break;
        }

        if (buf_len <= 0) continue;

        if (!qm_conn_send_stats(mqtt_buf, buf_len, &res))
        {
            LOGE("Publish report failed.\n");
            break;
        }

    }
}

static void
bm_stats_qm_timer_cb(struct ev_loop *loop, ev_timer *watcher, int revents)
{
    (void)loop;
    (void)watcher;
    (void)revents;

    bm_stats_steering_send_mqtt();

}

/******************************************************************************
 * OVSDB definitions
 *****************************************************************************/

static bool
bm_stats_enumerate(
        struct schema_Wifi_Stats_Config *schema,
        bm_stats_request_t         *request)
{

    if (!strcmp(schema->stats_type, BM_STATS_TYPE_STEERING)) {
        request->stats_type = BM_STATS_STEERING;
    }
    else if (!strcmp(schema->stats_type, BM_STATS_TYPE_RSSI)) {
        request->stats_type = BM_STATS_RSSI;
    }
    else {
        request->stats_type = BM_STATS_ERROR;
        // There are a lot other stats which are used in SM
        LOGT("Steering stats update (ignore stats_type %s)",
              schema->stats_type );
        return false;
    }

    if (strcmp(schema->radio_type, RADIO_TYPE_STR_2G) == 0) {
        request->radio_type = RADIO_TYPE_2G;
        strcpy(request->radio_cfg.phy_name, SCHEMA_CONSTS_RADIO_PHY_NAME_2G);
    }
    else if (strcmp(schema->radio_type, RADIO_TYPE_STR_5G) == 0) {
        request->radio_type = RADIO_TYPE_5G;
        strcpy(request->radio_cfg.phy_name, SCHEMA_CONSTS_RADIO_PHY_NAME_5G);
    }
    else if (strcmp(schema->radio_type, RADIO_TYPE_STR_5GL) == 0) {
        request->radio_type = RADIO_TYPE_5GL;
        strcpy(request->radio_cfg.phy_name, SCHEMA_CONSTS_RADIO_PHY_NAME_5GL);
    }
    else if (strcmp(schema->radio_type, RADIO_TYPE_STR_5GU) == 0) {
        request->radio_type = RADIO_TYPE_5GU;
        strcpy(request->radio_cfg.phy_name, SCHEMA_CONSTS_RADIO_PHY_NAME_5GU);
    }
    else {
        LOGE("Steering stats update (unknown radio type %s)",
              schema->radio_type);
        return false;
    }
    request->radio_cfg.type = request->radio_type;

    request->report_type = REPORT_TYPE_NONE;
    if (schema->report_type_exists) {
        if (strcmp(schema->report_type, "raw") == 0) {
            request->report_type = REPORT_TYPE_RAW;
        }
        else if (strcmp(schema->report_type, "average") == 0) {
            request->report_type = REPORT_TYPE_AVERAGE;
        }
        else if (strcmp(schema->report_type, "histogram") == 0) {
            request->report_type = REPORT_TYPE_HISTOGRAM;
        }
        else if (strcmp(schema->report_type, "percentile") == 0) {
            request->report_type = REPORT_TYPE_PERCENTILE;
        }
    }

    request->reporting_count     = schema->reporting_count;
    request->reporting_interval  = schema->reporting_interval;

    struct timespec ts;
    memset (&ts, 0, sizeof (ts));
    if(clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return false;

    request->reporting_timestamp = timespec_to_timestamp(&ts);

    return true;
}

static void
bm_stats_set_report(bm_stats_request_t *request)
{
    if (request->stats_type == BM_STATS_STEERING)
        bm_stats_steering_report_request(
                request);
    else{
        bm_stats_rssi_report_request(
                &request->radio_cfg,
                request);
    }
}

static void
bm_stats_ovsdb_update_cb(ovsdb_update_monitor_t *self)
{
    bool                                    ret = false;
    struct schema_Wifi_Stats_Config         schema;
    pjs_errmsg_t                            perr;
    bm_stats_request_t                     *request = NULL;

    switch(self->mon_type) {

    case OVSDB_UPDATE_NEW:
        if (!schema_Wifi_Stats_Config_from_json(
                    &schema,
                    self->mon_json_new,
                    false,
                    perr)) {
            LOGE("Failed to parse new Wifi_Stats_Config row: %s", perr);
            return;
        }
        request = calloc(1, sizeof(bm_stats_request_t));
        if (NULL == request) {
            LOGE("OVSDB Steering stats config new (Failed to allocate memory)");
            return;
        }

        ret = bm_stats_enumerate(&schema, request);
        if (!ret) {
            free(request);
            return;
        }

        /* Sometimes on the startup, UUID gets lost, therefore we copy it */
        STRSCPY(request->uuid, (char *)self->mon_uuid);
        ds_tree_insert(&bm_stats_table, request, request->uuid);

        bm_stats_set_report(request);

        break;

    case OVSDB_UPDATE_MODIFY:
        request = ds_tree_find(&bm_stats_table, (char *)self->mon_uuid);
        if (!request)
        {
            // Too many logs from other SM stats_type
            LOGT("OVSDB Steering stats config modify (Failed to find %s)",
                 self->mon_uuid);
            return;
        }

        if (!schema_Wifi_Stats_Config_from_json(
                    &schema,
                    self->mon_json_new,
                    true,
                    perr)) {
            LOGE("Failed to parse modified Band_Steering_Config row: %s",
                 perr);
            return;
        }

        ret = bm_stats_enumerate(&schema, request);
        if (!ret)
        {
            LOGE("OVSDB Steering stats config modify (Failed to parse)");
            return;
        }

        bm_stats_set_report(request);
        break;

    case OVSDB_UPDATE_DEL:
        request = ds_tree_find(&bm_stats_table, (char *)self->mon_uuid);
        if (!request)
        {
            // Too many logs from other SM stats_type
            LOGT("OVSDB Steering stats config delete (Failed to find %s)",
                 self->mon_uuid);
            return;
        }

        /* Reset configuration */
        request->reporting_interval = 0;
        request->reporting_count = 0;

        bm_stats_set_report(request);

        ds_tree_remove(&bm_stats_table, request);
        free(request);
        break;

    default:
        break;

    }
}

/******************************************************************************
 * PUBLIC API definitions
 *****************************************************************************/

bool
bm_stats_init(struct ev_loop *loop)
{
    LOGI("BM stats Initializing");

    g_bm_stats_evloop = loop;

    ASSERT(DPP_MAX_BS_BANDS >= BSAL_BAND_COUNT, "INVALID BAND COUNT");

    // Initialize the report list
    ds_dlist_init(
            &g_bm_stats_steering_report.list,
            dpp_bs_client_record_list_t,
            node);

    // Initialize MQTT queue
    dpp_init();

    bm_stats_steering_clear_all_stats();
    bm_stats_steering_clear_all_event_records();

    // Start QM stats sending timer
    ev_timer_init (&g_bm_stats_qm_send_timer,
                    bm_stats_qm_timer_cb,
                    BM_STATS_QM_SEND_INTERVAL,
                    BM_STATS_QM_SEND_INTERVAL);
    ev_timer_start(g_bm_stats_evloop, &g_bm_stats_qm_send_timer);

    // Start BM event sending timer (legacy)
    ev_init (&g_bm_stats_default_request.timer,
            bm_stats_ev_timer_cb);
    g_bm_stats_default_request.reporting_interval =
        BM_STATS_DEFAULT_REPORTING_TIME;
    ev_timer_set (&g_bm_stats_default_request.timer,
            g_bm_stats_default_request.reporting_interval,
            g_bm_stats_default_request.reporting_interval);
    ev_timer_start(g_bm_stats_evloop,
            &g_bm_stats_default_request.timer);

    // Hook to cloud stats interval configuration
    if (!ovsdb_update_monitor(
                &g_bm_stats_ovsdb_update,
                bm_stats_ovsdb_update_cb,
                SCHEMA_TABLE(Wifi_Stats_Config),
                OMT_ALL)) {
        LOGE("Failed to monitor OVSDB table '%s'",
             SCHEMA_TABLE(Wifi_Stats_Config));
        return false;
    };

    return true;
}

bool
bm_stats_cleanup( void )
{
    bm_stats_request_t      *request;
    ds_tree_iter_t           iter;

    LOGI("BM stats cleanup");

    bm_stats_steering_remove_all_clients();

    ev_timer_stop(g_bm_stats_evloop, &g_bm_stats_qm_send_timer);

    ev_timer_stop(g_bm_stats_evloop, &g_bm_stats_default_request.timer);
    g_bm_stats_default_request.reporting_interval = 0;

    /* Cleanup requests */
    for (   request = ds_tree_ifirst(&iter, &bm_stats_table);
            request != NULL;
            request = ds_tree_inext(&iter)) {
        ev_timer_stop(g_bm_stats_evloop, &request->timer);
        free(request);
        request = NULL;
    }

    return true;
}

void
bm_stats_add_event_to_report(
                        bm_client_t *client,
                        bsal_event_t *event,
                        dpp_bs_client_event_type_t bs_event,
                        bool backoff_enabled )
{
    bsal_band_t band;

    // Band steering is currently only done from 2.4 --> 5 band.
    // Hence, add the backoff and BAND_STEERING_ATTEMPT events to 2.4G band
    if ( bs_event == BACKOFF || bs_event == BAND_STEERING_ATTEMPT ) {
        band = BSAL_BAND_24G;
    } else {
        band = event->band;
    }

    if (band == BSAL_BAND_COUNT) {
        LOGW("%s: client %s event %d wrong band", __func__, client ? client->mac_addr : NULL, bs_event);
    }

    /* Steering report */
    bm_stats_steering_parse_event(
            client,
            event,
            bs_event,
            backoff_enabled,
            band);
}

void
bm_stats_remove_client_from_report( bm_client_t *client )
{
    dpp_bs_client_list_t        *bs_client_list     = &g_bm_stats_steering_report.list;
    dpp_bs_client_record_list_t *bs_client          = NULL;
    dpp_bs_client_record_t      *bs_client_entry    = NULL;
    ds_dlist_iter_t             bs_client_iter;

    mac_address_t mac_addr;
    LOGN("Removing '%s' client record from Band Steering report",
         client->mac_addr );

    for( bs_client = ds_dlist_ifirst( &bs_client_iter, bs_client_list );
         bs_client != NULL;
         bs_client = ds_dlist_inext( &bs_client_iter ) )
    {
        bs_client_entry = &bs_client->entry;

        if( !os_nif_macaddr_from_str((os_macaddr_t *)&mac_addr, client->mac_addr ) ) {
            LOGE("Failed to parse mac address '%s'",
                 client->mac_addr );
        }

        if( MAC_ADDR_EQ( bs_client_entry->mac, mac_addr ) )
        {
            ds_dlist_iremove( &bs_client_iter );
            dpp_bs_client_record_free( bs_client );
            bs_client = NULL;

            break;
        }
    }

    return;
}

int
bm_stats_get_stats_report_interval( void )
{
    return g_bm_stats_default_request.reporting_interval;
}

void
bm_stats_map_radio_type(bsal_band_t band, const char *ifname)
{
    const char      *mapped_bandstr = target_map_ifname_to_bandstr(ifname);
    radio_type_t    mapped_radio_type;

    if (!mapped_bandstr) {
        // No mapping found
        return;
    }

    if (band == BSAL_BAND_24G && !strcmp(mapped_bandstr, "2G")) {
        mapped_radio_type = RADIO_TYPE_2G;
    }
    else if (band == BSAL_BAND_5G && !strcmp(mapped_bandstr, "5G")) {
        mapped_radio_type = RADIO_TYPE_5G;
    }
    else if (band == BSAL_BAND_5G && !strcmp(mapped_bandstr, "5GL")) {
        mapped_radio_type = RADIO_TYPE_5GL;
    }
    else if (band == BSAL_BAND_5G && !strcmp(mapped_bandstr, "5GU")) {
        mapped_radio_type = RADIO_TYPE_5GU;
    }
    else {
        LOGW("%s: Ignoring unknown mapped band string \"%s\" for band %d",
                                                    ifname, mapped_bandstr, band);
        return;
    }

    switch(band) {

    case BSAL_BAND_24G:
        g_bm_stats_2g_radio_type = mapped_radio_type;
        break;

    case BSAL_BAND_5G:
        g_bm_stats_5g_radio_type = mapped_radio_type;
        break;

    default:
        break;

    }

    return;
}
