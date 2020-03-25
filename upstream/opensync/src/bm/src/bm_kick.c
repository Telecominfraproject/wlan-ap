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
 * Band Steering Manager - Kicking Logic
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

#include "bm.h"


/*****************************************************************************/
#define MODULE_ID LOG_MODULE_ID_KICK

#define INST_RSSI_SAMPLE_CNT        5

/*****************************************************************************/
typedef struct {
    os_macaddr_t        macaddr;
    bsal_band_t         band;
    bsal_t              bsal;

    bm_kick_type_t      type;
    uint8_t             rssi;
    bool                measuring;

    ds_list_t           dsl_node;
} bm_kick_t;

static ds_list_t        bm_kick_queue;

static c_item_t map_bsal_kick_type[] = {
    C_ITEM_VAL(BM_CLIENT_KICK_DISASSOC,         BSAL_DISC_TYPE_DISASSOC),
    C_ITEM_VAL(BM_CLIENT_KICK_DEAUTH,           BSAL_DISC_TYPE_DEAUTH),
    C_ITEM_VAL(BM_CLIENT_KICK_BTM_DISASSOC,     BSAL_DISC_TYPE_DISASSOC),
    C_ITEM_VAL(BM_CLIENT_KICK_BTM_DEAUTH,       BSAL_DISC_TYPE_DEAUTH),
    C_ITEM_VAL(BM_CLIENT_KICK_RRM_DISASSOC,     BSAL_DISC_TYPE_DISASSOC),
    C_ITEM_VAL(BM_CLIENT_KICK_RRM_DEAUTH,       BSAL_DISC_TYPE_DEAUTH)
};


/*****************************************************************************/
static bm_kick_t *      bm_kick_find_by_macaddr(os_macaddr_t macaddr);
static bm_kick_t *      bm_kick_find_by_macstr(char *mac_str);
static void             bm_kick_free(bm_kick_t *kick);
static void             bm_kick_task_queue_run(void *arg);
static void             bm_kick_queue_start(void);


/*****************************************************************************/
static bm_client_kick_t
bm_kick_get_kick_type( bm_client_t *client, bm_kick_type_t kick )
{
    bm_client_kick_t kick_type = BM_CLIENT_KICK_NONE;

    switch( kick )
    {
        case BM_STEERING_KICK:
            kick_type = client->kick_type;
            break;

        case BM_STICKY_KICK:
            kick_type = client->sticky_kick_type;
            break;

        case BM_FORCE_KICK:
            kick_type = client->sc_kick_type;
            break;

        default:
            break;
    }

    return kick_type;
}

static uint8_t
bm_kick_get_kick_reason( bm_client_t *client, bm_kick_type_t kick )
{
    uint8_t kick_reason = 0;

    switch( kick )
    {
        case BM_STEERING_KICK:
            kick_reason = client->kick_reason;
            break;

        case BM_STICKY_KICK:
            kick_reason = client->sticky_kick_reason;
            break;

        case BM_FORCE_KICK:
            kick_reason = client->sc_kick_reason;
            break;

        default:
            break;
    }

    return kick_reason;
}

static uint16_t
bm_kick_get_debounce_period( bm_client_t *client, bm_kick_type_t kick )
{
    uint16_t kick_debounce_period = 0;

    switch( kick )
    {
        case BM_STEERING_KICK:
            kick_debounce_period = client->kick_debounce_period;
            break;

        case BM_STICKY_KICK:
            kick_debounce_period = client->sticky_kick_debounce_period;
            break;

        case BM_FORCE_KICK:
            kick_debounce_period = client->sc_kick_debounce_period;
            break;

        default:
            break;
    }

    return kick_debounce_period;
}

static char *
bm_kick_get_kick_type_str(bm_kick_type_t type)
{
    char *str = NULL;

    switch( type )
    {
        case BM_STEERING_KICK:
            str = "STEERING";
            break;

        case BM_STICKY_KICK:
            str = "STICKY";
            break;

        case BM_FORCE_KICK:
            str = "FORCE";
            break;

        default:
            str = "NONE";
            break;
    }

    return str;
}

static bool
bm_kick_get_dpp_event_by_kick_type( bm_client_t *client, bm_kick_type_t type,
                                    dpp_bs_client_event_type_t *dpp_event )
{
    bm_client_force_kick_t  force_kick_type = client->force_kick_type;

    switch( type )
    {
        case BM_STEERING_KICK:
            *dpp_event = CLIENT_BS_KICK;
            break;

        case BM_STICKY_KICK:
            *dpp_event = CLIENT_STICKY_KICK;
            break;

        case BM_FORCE_KICK:
        {
            switch( force_kick_type )
            {
                case BM_CLIENT_SPECULATIVE_KICK:
                    *dpp_event = CLIENT_SPECULATIVE_KICK;
                    LOGN( "Client '%s' being speculatively kicked", client->mac_addr );
                    break;

                case BM_CLIENT_DIRECTED_KICK:
                    *dpp_event = CLIENT_DIRECTED_KICK;
                    LOGN( "Client '%s' being issued a directed kicked", client->mac_addr );
                    break;

                case BM_CLIENT_GHOST_DEVICE_KICK:
                    *dpp_event = CLIENT_GHOST_DEVICE_KICK;
                    LOGN( "Client '%s' being issued a ghost device kick", client->mac_addr );
                    break;

                default:
                    *dpp_event = CLIENT_KICKED;
                    LOGN( "Client '%s': LWM value toggled, kicking", client->mac_addr );
                    break;
            }

            break;
        }

        default:
            LOGE( "Unknown bm_kick_type '%d'", type );
            return false;
    }

    return true;
}

static bm_kick_t *
bm_kick_find_by_macaddr(os_macaddr_t macaddr)
{
    bm_kick_t       *kick;

    ds_list_foreach(&bm_kick_queue, kick) {
        if (memcmp(&macaddr, &kick->macaddr, sizeof(macaddr)) == 0) {
            return kick;
        }
    }

    return NULL;
}

static bm_kick_t *
bm_kick_find_by_macstr(char *mac_str)
{
    os_macaddr_t        macaddr;

    if (!os_nif_macaddr_from_str(&macaddr, mac_str)) {
        LOGE("Failed to parse mac address: '%s'", mac_str);
        return NULL;
    }

    return bm_kick_find_by_macaddr(macaddr);
}

static void
bm_kick_free(bm_kick_t *kick)
{
    evsched_task_cancel_by_find(NULL, kick, EVSCHED_FIND_BY_ARG);
    free(kick);
    return;
}

static void
bm_kick_task_kick(void *arg)
{
    bm_client_stats_t           *stats;
    bm_client_times_t           *times;
    bm_client_t                 *client;
    bm_kick_t                   *kick = arg;
    bm_client_kick_t            kick_type;
    uint32_t                    bsal_kick_type;
    uint8_t                     kick_reason;
    int                         ret;
    bsal_event_t                event;
    dpp_bs_client_event_type_t  dpp_event;

    if (kick != ds_list_head(&bm_kick_queue)) {
        LOGW("bm_kick_task_kick() called with entry not head of queue!");
        return;
    }
    ds_list_remove_head(&bm_kick_queue);


    do {
        client = bm_client_find_by_macaddr(kick->macaddr);
        if (!client || !client->connected || !client->pair ||
                           client->pair->bsal != kick->bsal || client->band != kick->band) {
            // No longer in same state, don't kick
            break;
        }
        stats = &client->stats[client->band];
        times = &client->times;

        kick_type   = bm_kick_get_kick_type( client, kick->type );
        kick_reason = bm_kick_get_kick_reason( client, kick->type );

        if (kick->type != BM_FORCE_KICK && kick->rssi == 0) {
            LOGW("Client '%s' RSSI measurement failed, not kicking...", client->mac_addr);
            break;
        }

        if (!c_get_value_by_key(map_bsal_kick_type, kick_type, &bsal_kick_type)) {
            LOGE("Client '%s' kick type %u failed to map to BSAL",
                                                      client->mac_addr, kick_type);
            break;
        }

        if (kick->type == BM_STICKY_KICK) {
            if (kick->rssi > client->lwm) {
                LOGD("Client '%s' measured RSSI %u is higer then LWM %u, not kicking...",
                                                client->mac_addr, kick->rssi, client->lwm);
                break;
            }
            LOGI("Client '%s' measured RSSI %u is lower then LWM %u, kicking...",
                                                client->mac_addr, kick->rssi, client->lwm);
        }
        else if (kick->type == BM_STEERING_KICK) {
            if (kick->rssi <= client->hwm) {
                LOGD("Client '%s' measured RSSI %u is lower then HWM %u, not kicking...",
                                                client->mac_addr, kick->rssi, client->hwm);
                break;
            }
            LOGI("Client '%s' measured RSSI %u is higher then HWM %u, kicking...",
                                                client->mac_addr, kick->rssi, client->hwm);
        } else if (kick->type == BM_FORCE_KICK) {
            LOGI("Client '%s' being forced kicked...", client->mac_addr );
        }
        else {
            break;
        }

        ret = target_bsal_client_disconnect(client->pair->ifcfg[kick->band].ifname,
                                           (uint8_t *)&kick->macaddr,
                                            bsal_kick_type,
                                            kick_reason);
        if (ret < 0) {
            LOGE("Client '%s' BSAL kick failed, ret = %d", client->mac_addr, ret);
        }
        else {
            times->last_kick = time(NULL);
            switch(kick->type) {
                case BM_STICKY_KICK:
                case BM_FORCE_KICK:
                    stats->sticky_kick_cnt++;
                    break;

                case BM_STEERING_KICK:
                    stats->steering_kick_cnt++;
                    break;
            }

            if( bm_kick_get_dpp_event_by_kick_type( client, kick->type, &dpp_event ) ) {
                memset( &event, 0, sizeof( event ) );
                event.band = kick->band;
                bm_stats_add_event_to_report( client, &event, dpp_event, false );
            } else {
                LOGE( "Client '%s': Unable to get Kick DPP event", client->mac_addr );
            }
        }
    } while(0);

    bm_kick_free(kick);

    if( ds_list_head( &bm_kick_queue ) ) {
        // Continue processing other kicks in the queue
        bm_kick_queue_start();
    }

    return;
}

static void
bm_kick_task_queue_run(void *arg)
{
    bm_client_t         *client;
    bm_kick_t           *kick;
    bool                remove = false;
    int                 ret;

    (void)arg;

    if ((kick = ds_list_head(&bm_kick_queue)) == NULL) {
        // Finished
        return;
    }

    do {
        // Make sure client is still in same state
        client = bm_client_find_by_macaddr(kick->macaddr);
        if (!client || !client->connected || !client->pair ||
                        client->pair->bsal != kick->bsal || client->band != kick->band) {
            // No longer in same state, don't kick
            remove = true;
            break;
        }

        if( kick->type == BM_FORCE_KICK ) {
            // Don't do an instant measurement for force kick
            evsched_task(bm_kick_task_kick, kick, EVSCHED_ASAP);
            break;
        }

        LOGD("Starting measurement for '%s'", client->mac_addr);

        // Kick off an instant measurement for this entry
        ret = target_bsal_client_measure(client->pair->ifcfg[kick->band].ifname,
                                    (uint8_t *)&kick->macaddr, INST_RSSI_SAMPLE_CNT);
        if (ret == -ENOSYS) {
            // BSAL library doesn't support instant measurement, just use last RSSI
            evsched_task(bm_kick_task_kick, kick, EVSCHED_ASAP);
            break;
        }
        else if (ret < 0) {
            LOGE("Failed to perform instant measrement for '%s', ret = %d",
                                                               client->mac_addr, ret);
            remove = true;
            break;
        }

        kick->measuring = true;
    } while(0);

    if (remove) {
        bm_kick_free(ds_list_remove_head(&bm_kick_queue));
        evsched_task_reschedule();
    }

    return;
}

static void
bm_kick_queue_start(void)
{
    evsched_task(bm_kick_task_queue_run, NULL, EVSCHED_ASAP);
    return;
}

/*****************************************************************************/

static bsal_btm_params_t *
bm_kick_get_btm_params_by_kick_type( bm_client_t *client, bm_kick_type_t type )
{
    switch( type )
    {
        case BM_STEERING_KICK:
            return &client->steering_btm_params;

        case BM_STICKY_KICK:
            return &client->sticky_btm_params;

        case BM_FORCE_KICK:
            return &client->sc_btm_params;

        default:
            LOGE( "Unknown bm_kick_type '%d'", type );
            break;
    }

    return NULL;
}

static bool
bm_kick_get_dpp_event_by_btm_type( bm_kick_type_t type,
                                   dpp_bs_client_event_type_t *dpp_event, bool retry )
{
    if( !retry ) {
        switch( type )
        {
            case BM_STEERING_KICK:
                *dpp_event = CLIENT_BS_BTM;
                break;

            case BM_STICKY_KICK:
                *dpp_event = CLIENT_STICKY_BTM;
                break;

            case BM_FORCE_KICK:
                *dpp_event = CLIENT_BTM;
                break;

            default:
                LOGE( "Unknown bm_kick_type '%d'", type );
                return false;

        }
    } else {
        switch( type )
        {
            case BM_STEERING_KICK:
                *dpp_event = CLIENT_BS_BTM_RETRY;
                break;

            case BM_STICKY_KICK:
                *dpp_event = CLIENT_STICKY_BTM_RETRY;
                break;

            case BM_FORCE_KICK:
                *dpp_event = CLIENT_BTM_RETRY;
                break;

            default:
                LOGE( "Retry: Unknown bm_kick_type '%d'", type );
                return false;

        }
    }

    return true;
}

static bool
bm_kick_get_self_btm_values( bsal_btm_params_t *btm_params,
                             bm_pair_t *pair, bsal_band_t band )
{
    if (!bm_neighbor_get_self_neighbor(pair, band, &btm_params->neigh[0])) {
        LOGW("%s failed for %s", __func__, pair->ifcfg[band].ifname);
        return false;
    }

    // Set number of neighbors to 1 since the BTM request is to move the
    // client from one bssid to another on this AP
    btm_params->num_neigh = 1;
    return true;
}

static bool
bm_kick_build_neighbor_list( bm_client_t *client, bsal_btm_params_t *btm_params )
{
    ds_tree_t                   *bm_neighbors   = NULL;
    bm_neighbor_t               *bm_neigh       = NULL;

    bsal_neigh_info_t           *bsal_neigh     = NULL;
    os_macaddr_t                macaddr;

    bm_neighbors = bm_neighbor_get_tree();
    if( !bm_neighbors ) {
        LOGE( "Unable to get bm_neighbors tree" );
        return false;
    }

    btm_params->num_neigh = 0;

    ds_tree_foreach( bm_neighbors, bm_neigh ) {

        if( btm_params->num_neigh >= BSAL_MAX_TM_NEIGHBORS ) {
            LOGT( "Built maximum allowed neighbors" );
            break;
        }

        // Include only 5GHz neighbors
        if( bm_neigh->channel <= 13 ) {
            LOGT( "Skipping neighbor = %s, channel = %hhu",
                                bm_neigh->bssid, bm_neigh->channel );
        } else {
            bsal_neigh = &btm_params->neigh[btm_params->num_neigh];

            if( !os_nif_macaddr_from_str( &macaddr, bm_neigh->bssid ) ) {
                LOGE( "Unable to parse mac address '%s'", bm_neigh->bssid );
                return false;
            }

            memcpy( bsal_neigh->bssid, (uint8_t *)&macaddr, sizeof( bsal_neigh->bssid ) );
            bsal_neigh->channel = bm_neigh->channel;
            bsal_neigh->bssid_info = BTM_DEFAULT_NEIGH_BSS_INFO;

            btm_params->num_neigh++;

            LOGT( "Built neighbor: %s, channel: %hhu", bm_neigh->bssid, bm_neigh->channel );
        }
    }

    LOGT( "Client '%s': Total neighbors = %hhu", client->mac_addr, btm_params->num_neigh );

    return true;
}

static bool
bm_kick_get_rrm_op_class( bsal_rrm_params_t *rrm_params, uint8_t channel )
{
    uint8_t op_class;

    rrm_params->channel = channel;

    if( channel == RRM_DEFAULT_CHANNEL ) {
        rrm_params->op_class = RRM_DEFAULT_OP_CLASS;
    } else {
        op_class = bm_neighbor_get_op_class( channel );
        if( op_class == 0 ) {
            LOGE( "get_rrm_params: Unable to get op_class for channel '%hhu'", channel );
            return false;
        }

        rrm_params->op_class = op_class;
    }

    return true;
}

static bool
bm_kick_get_rrm_params( bsal_rrm_params_t *rrm_params )
{
    rrm_params->rand_ivl        = RRM_DEFAULT_RAND_IVL;
    rrm_params->meas_dur        = RRM_DEFAULT_MEAS_DUR;
    rrm_params->meas_mode       = RRM_DEFAULT_MEAS_MODE;
    rrm_params->req_ssid        = RRM_DEFAULT_REQ_SSID;
    rrm_params->rep_cond        = RRM_DEFAULT_REP_COND;
    rrm_params->rpt_detail      = RRM_DEFAULT_RPT_DETAIL;
    rrm_params->req_ie          = RRM_DEFAULT_REQ_IE;
    rrm_params->chanrpt_mode    = RRM_DEFAULT_CHANRPT_MODE;

    return true;
}

static bool
bm_kick_issue_bss_tm_req(bm_client_t *client, bm_kick_type_t type)
{
    dpp_bs_client_event_type_t  dpp_event;
    bsal_btm_params_t           *btm_params = NULL;
    bsal_neigh_info_t           *neigh      = NULL; 
    os_macaddr_t                macaddr;
    bsal_event_t                event;

    int                         i;

    if (!client || !client->pair) {
        LOGE("bm_kick_issue_bss_tm_req: client or client->pair is NULL");
        return false;
    }

    if (!client->is_BTM_supported) {
        LOGW("Client '%s' CAPS indicate no 11v (BTM) support", client->mac_addr);
    }

    if( type == BM_STICKY_KICK && client->band == BSAL_BAND_24G &&
        client->pair->gw_only ) {
        LOGN( "Client '%s' connected on 2.4Ghz, not issuing sticky"
              " kick", client->mac_addr );
        return false;
    }

    if (!os_nif_macaddr_from_str(&macaddr, client->mac_addr)) {
        LOGE("bm_kick_issue_bss_tm_req: Failed to parse mac"
             " address '%s'", client->mac_addr);
        return false;
    }

    btm_params = bm_kick_get_btm_params_by_kick_type(client, type);
    if (!btm_params) {
        LOGE("Client %s: BTM params are NULL for kick_type %d", client->mac_addr, type);
        return false;
    }

    if( btm_params->tries == 0 ) {
        if( !bm_kick_get_dpp_event_by_btm_type( type, &dpp_event, false ) ) {
            LOGE( "Client '%s': Unable to get BTM DPP event", client->mac_addr );
            return false;
        }
    } else {
        if( !bm_kick_get_dpp_event_by_btm_type( type, &dpp_event, true ) ) {
            LOGE( "Client '%s': Unable to get BTM Retry DPP event", client->mac_addr );
            return false;
        }
    }

    memset(&event, 0, sizeof(event));
    event.band = client->band;
    bm_stats_add_event_to_report( client, &event, dpp_event, false );

    LOGD("Client %s: Initiating 11v BSS Transition for kick type %s", 
                                    client->mac_addr, bm_kick_get_kick_type_str(type));

    if (type == BM_STEERING_KICK) {
        if( !bm_kick_get_self_btm_values( btm_params, client->pair, BSAL_BAND_5G ) ) {
            LOGE( "Unable to get channel/bssid for steering BTM requests" );
            return false;
        }
    } else if( type == BM_STICKY_KICK ) {
        if( client->pair->gw_only ) {
            if( !bm_kick_get_self_btm_values( btm_params, client->pair, BSAL_BAND_24G ) ) {
                LOGE( "Unable to get channel/bssid for sticky BTM gw-only mode request" );
                return false;
            }

            // Override pref=0 for STICKY BSS TM Request since we are providing
            // our own 2.4 BSSID
            btm_params->pref = 1;
        } else {
            if( btm_params->inc_neigh ) {

                LOGT(" Client '%s': Building sticky neighbor list", client->mac_addr );
                // Since we are providing the list of neighbors,
                // ask clients to prefer this list
                btm_params->pref = 1;

                if( !bm_kick_build_neighbor_list( client, btm_params ) ) {
                    LOGE( "Client '%s': Unable to build stikcy 11v nieghbor list",
                            client->mac_addr );
                    return false;
                }

                if (btm_params->inc_self) {
                    if (!bm_neighbor_get_self_neighbor(client->pair, BSAL_BAND_5G, &btm_params->neigh[btm_params->num_neigh])) {
                        LOGW("get self for sticky 11v neighbor list failed for %s", client->pair->ifcfg[BSAL_BAND_5G].ifname);
                        return false;
                    }

                    btm_params->num_neigh++;
                }

                // We get XING_LOW here and no neighbor candidate - only local ifaces
                if (btm_params->num_neigh == 0) {
                    LOGI("Client '%s': empty sticky 11v neighbor list", client->mac_addr);
                    btm_params->pref = 0;
                }
            } else {
                LOGT(" Client '%s': NOT building sticky neighbor list", client->mac_addr );
                // Set back default values for 5 --> 2.4 GHz Sticky BSS TM
                btm_params->pref      = 0;
                btm_params->num_neigh = 0;
            }
        }
    }

    if (btm_params->num_neigh > 0) {
        for( i = 0 ; i < btm_params->num_neigh ; i++ ) {
            neigh = &btm_params->neigh[i];

            neigh->op_class = bm_neighbor_get_op_class( neigh->channel );
            if( neigh->op_class == 0 ) {
                LOGE( "Client '%s': Unable to get op_class for channel '%hhu'",
                                                            client->mac_addr, neigh->channel );
                return false;
            }

            neigh->phy_type = bm_neighbor_get_phy_type( neigh->channel );
            if( neigh->phy_type == 0 ) {
                LOGE( "Client '%s': Unable to get phy_type for channel '%hhu'",
                                                            client->mac_addr, neigh->channel );
                return false;
            }
        }
    }

    if (target_bsal_bss_tm_request(client->pair->ifcfg[client->band].ifname,
                           (uint8_t *)&macaddr, btm_params ) < 0) {
        LOGE("BSS Transition Request failed for client %s", client->mac_addr);
        return false;
    }

    return true;
}

static void
bm_kick_btm_retry_task( void *arg )
{
    bm_client_kick_info_t       *kick_info  = NULL;
    bsal_btm_params_t           *btm_params = NULL;
    bm_client_t                 *client     = arg;

    kick_info = &client->kick_info;

    btm_params = bm_kick_get_btm_params_by_kick_type( client, kick_info->kick_type );
    if( !btm_params ) {
        LOGE( "Client %s: BTM params are NULL for kick_type %d", 
                                        client->mac_addr, kick_info->kick_type );
        return;
    }

    if( !client->connected ) {
        LOGD( "Client '%s' disconnected, stopping BSS TM Requests", client->mac_addr );
        return;
    }

    if (client->cancel_btm) {
        LOGD("client %s cancel BTM task", client->mac_addr);
        bm_kick_cancel_btm_retry_task( client );
        return;
    }

    btm_params->tries++;
    LOGD( "Re-issuing BSS TM Request to client '%s', total tries:'%d'",
                                            client->mac_addr, btm_params->tries );

    if( !bm_kick_issue_bss_tm_req( client, kick_info->kick_type ) ) {
        LOGE( "BSS Transition Request for client '%s' failed", client->mac_addr );
        return;
    }

    if( btm_params->tries >= btm_params->max_tries) {
        LOGD( "Maximum number of BSS TM Requests issued to client '%s'", client->mac_addr );
        bm_kick_cancel_btm_retry_task( client );

        return;
    }

    evsched_task_reschedule();

    return;
}

void
bm_kick_cancel_btm_retry_task( bm_client_t *client )
{
    bm_client_kick_info_t       *kick_info  = NULL;
    bsal_btm_params_t           *btm_params = NULL;

    kick_info = &client->kick_info;

    /* Cancel also legacy kick */
    kick_info->kick_pending = false;
    kick_info->rssi = 0;

    client->cancel_btm = false;
    btm_params = bm_kick_get_btm_params_by_kick_type( client, kick_info->kick_type );
    if( !btm_params ) {
        LOGE( "Client %s: BTM params are NULL for kick_type %d", 
                                        client->mac_addr, kick_info->kick_type );
        return;
    }

    if( btm_params->tries > 1 ) {
        if( client->connected ) {
            LOGD( "Client '%s' connected back, stopping BTM retries", client->mac_addr );
        } else {
            LOGD( "Client '%s' disconnected, stopping BTM retries", client->mac_addr );
        }
    }

    // Cancel any pending BSS TM Request retry task
    evsched_task_cancel_by_find( bm_kick_btm_retry_task, client,
            ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG ) );

    btm_params->tries = 0;

    return;
}

static bool
bm_kick_handle_bss_tm_req( bm_client_t *client, bm_kick_type_t kick )
{
    bsal_btm_params_t    *btm_params = NULL;

    btm_params = bm_kick_get_btm_params_by_kick_type( client, kick );
    if( !btm_params ) {
        LOGE( "Client %s: BTM params are NULL for kick_type %d", client->mac_addr, kick );
        return false;
    }

    // If a RSSI_XING event is received while BSS TM Request's are being retried
    // for this client, ignore the request
    if( btm_params->tries > 0) {
        if (client->kick_info.kick_type == kick) {
            LOGN( "BSS TM Request being retried for Client '%s', ignoring", client->mac_addr );
            return true;
        } else {
            LOGN("BSS TM Request '%s' switch kick type %d -> %d", client->mac_addr,
                 client->kick_info.kick_type, kick);
            bm_kick_cancel_btm_retry_task(client);
        }
    }

    if( !bm_kick_issue_bss_tm_req( client, kick ) ) {
        LOGI( "BSS Transition Request for client '%s' failed", client->mac_addr );
        return false;
    }

    /* Set to 1 while we already send first one */
    btm_params->tries     = 1;
    client->kick_info.kick_type = kick;

    client->btm_retry_task      = evsched_task( bm_kick_btm_retry_task,
                                                client,
                                                EVSCHED_SEC( btm_params->retry_interval ) );

    return true;
}

static bool
bm_kick_11k_channel_scan_scheduler( bm_client_t *client, os_macaddr_t macaddr,
                                    bsal_rrm_params_t *rrm_params )
{
    ds_tree_t       *bm_neighbors = NULL;
    bm_neighbor_t   *neigh        = NULL;
    uint8_t         channel;

    bm_neighbors = bm_neighbor_get_tree();
    if( !bm_neighbors ) {
        LOGE( "Unable to get bm_neighbors tree" );
        return false;
    }

    ds_tree_foreach( bm_neighbors, neigh ) {

        channel = neigh->channel;

        if( !bm_kick_get_rrm_op_class( rrm_params, channel ) ) {
            LOGE( "Client %s: Failed to get op_class for channel '%hhu'",
                                                    client->mac_addr, channel );
            return false;
        }

        LOGD( "Client %s: Initiating 11k RRM Beacon Report for channel '%hhu'",
                                                        client->mac_addr, channel );

        if( target_bsal_rrm_beacon_report_request( client->pair->ifcfg[client->band].ifname,
                                           (uint8_t *)&macaddr, rrm_params ) < 0) {
            LOGE( "RRM Beacon Report request failed for client %s", client->mac_addr );
            return false;
        }
    }

    return true;
}

static bool
bm_kick_handle_rrm_br_req(bm_client_t *client, bm_kick_type_t type)
{
    bsal_rrm_params_t       rrm_params;
    os_macaddr_t            macaddr;
    bsal_event_t            event;

    if (!client || !client->pair) {
        LOGE("bm_kick_handle_rrm_br_req: client or client->pair is NULL");
        return false;
    }

    if (!client->rrm_caps.bcn_rpt_active) {
        LOGD("Client '%s' CAPS report no 11k (beacon report active) support", client->mac_addr);
    }

    memset( &rrm_params, 0, sizeof( rrm_params ) );

    if (!os_nif_macaddr_from_str(&macaddr, client->mac_addr)) {
        LOGE("bm_kick_handle_rrm_br_req: Failed to parse mac address '%s'", client->mac_addr);
        return false;
    }

    if( !bm_kick_get_rrm_params( &rrm_params ) ) {
        LOGE( "Client %s: Failed to get RRM params", client->mac_addr );
        return false;
    }

    if( client->enable_ch_scan ) {
        LOGD( "Client %s: Initiating 11k channel scan scheduler", client->mac_addr );

        if( !bm_kick_11k_channel_scan_scheduler( client, macaddr, &rrm_params ) ) {
            LOGE( "Client %s: 11k channel scan scheduling failed", client->mac_addr );
            return false;
        }
    } else {
        LOGD("Client %s: Initiating 11k RRM Beacon Report"
             " for kick type %s", client->mac_addr, bm_kick_get_kick_type_str(type));

        if( !bm_kick_get_rrm_op_class( &rrm_params, 0 ) ) {
            LOGE( "Client %s: Failed to get op_class for channel 0", client->mac_addr );
            return false;
        }

        if (target_bsal_rrm_beacon_report_request( client->pair->ifcfg[client->band].ifname,
                    (uint8_t *)&macaddr, &rrm_params ) < 0) {
            LOGE("RRM Beacon Report request failed for client %s", client->mac_addr);
            return false;
        }
    }

    memset(&event, 0, sizeof(event));
    event.band = client->band;
    bm_stats_add_event_to_report(client, &event, CLIENT_RRM_BCN_RPT, false);

    return true;
}

/*****************************************************************************/
bool
bm_kick_init(void)
{
    LOGI("Kick Initializing");

    ds_list_init(&bm_kick_queue, bm_kick_t, dsl_node);
    return true;
}

bool
bm_kick_cleanup(void)
{
    ds_list_iter_t      iter;
    bm_kick_t           *kick;

    LOGI("Kick cleaning up");

    kick = ds_list_ifirst(&iter, &bm_kick_queue);
    while(kick) {
        ds_list_iremove(&iter);
        bm_kick_free(kick);

        kick = ds_list_inext(&iter);
    }

    return true;
}

bool
bm_kick_cleanup_by_bsal(bsal_t bsal)
{
    ds_list_iter_t      iter;
    bm_kick_t           *kick;
    bool                check_restart = false;

    kick = ds_list_ifirst(&iter, &bm_kick_queue);
    while(kick) {
        if (kick->bsal == bsal) {
            if (kick->measuring) {
                check_restart = true;
            }
            ds_list_iremove(&iter);
            bm_kick_free(kick);
        }

        kick = ds_list_inext(&iter);
    }

    if (check_restart) {
        if (ds_list_head(&bm_kick_queue)) {
            LOGW("Measuring entry removed -- restarting queue");
            bm_kick_queue_start();
        }
    }

    return true;
}

bool
bm_kick_cleanup_by_client(bm_client_t *client)
{
    ds_list_iter_t      iter;
    bm_kick_t           *kick;
    os_macaddr_t        macaddr;

    if (!os_nif_macaddr_from_str(&macaddr, client->mac_addr)) {
        LOGE("Failed to parse mac address: '%s'", client->mac_addr);
        return false;
    }

    kick = ds_list_ifirst(&iter, &bm_kick_queue);
    while(kick) {
        if (memcmp(&macaddr, &kick->macaddr, sizeof(macaddr)) == 0 &&
                                                    kick->measuring == false) {
            ds_list_iremove(&iter);
            bm_kick_free(kick);
        }

        kick = ds_list_inext(&iter);
    }

    return true;
}

bool
bm_kick(bm_client_t *client, bm_kick_type_t type, uint8_t rssi)
{
    bm_client_times_t   *times;
    bm_kick_t           *kick;
    bm_client_kick_t     kick_type;
    uint16_t             kick_debounce_period;

    if (!client->connected) {
        LOGW("Ignoring kick of '%s' because client is not connected",
                client->mac_addr);
        return false;
    }

    if (!client->pair) {
        LOGW("bm_kick: '%s' client->pair is NULL", client->mac_addr);
        return false;
    }

    if (client->kick_info.kick_type != type) {
       LOGI("bm_kick: '%s' switch kick type %d -> %d", client->mac_addr,
            client->kick_info.kick_type, type);
       bm_kick_cancel_btm_retry_task(client);
    }

    if (type == BM_STICKY_KICK && client->pref_5g == BM_CLIENT_5G_ALWAYS) {
        if (!bm_neighbor_number(client, BSAL_BAND_5G)) {
            /*
             * Empty neighbor table and we always prefer 5G, this mean one
             * POD left and only one interface. Skip kick.
             */
            LOGN("bm_kick: '%s' skip sticky kick empty neighbor list", client->mac_addr);
            return false;
        }
    }

    kick_type = bm_kick_get_kick_type( client, type );
    switch(kick_type) {

    case BM_CLIENT_KICK_NONE:
        // No kick type configured, so don't kick
        LOGD( "Ignoring kick of '%s' because kick type is NONE",
                client->mac_addr );
        return false;

    case BM_CLIENT_KICK_BSS_TM_REQ:
        // 802.11v BSS Transition Management Request
        return bm_kick_handle_bss_tm_req( client, type );

    case BM_CLIENT_KICK_RRM_BR_REQ:
        // 802.11k RRM Beacon Report Request
        return bm_kick_handle_rrm_br_req( client, type );

    case BM_CLIENT_KICK_BTM_DISASSOC:
    case BM_CLIENT_KICK_BTM_DEAUTH:
        if( client->is_BTM_supported ) {
            return bm_kick_handle_bss_tm_req( client, type );
        } else {
            // Client is not 11v capable, issue legacy kick
            break;
        }

    case BM_CLIENT_KICK_RRM_DISASSOC:
    case BM_CLIENT_KICK_RRM_DEAUTH:
        if( client->rrm_caps.bcn_rpt_active ) {
            return bm_kick_handle_rrm_br_req( client, type );
        } else {
            // Client is not 11k capable, issue legacy kick
            break;
        }

    default:
        // Disconnect type of kick, continue below
        break;

    }

    times = &client->times;
    kick_debounce_period = bm_kick_get_debounce_period( client, type );
    if (!(type == BM_FORCE_KICK && client->force_kick_type == BM_CLIENT_GHOST_DEVICE_KICK)) {
        if (((time(NULL) - times->last_connect ) <= kick_debounce_period) ||
            (kick_debounce_period == BM_KICK_MAGIC_DEBOUNCE_PERIOD)) {
            LOGW("Ignoring kick of '%s' -- reconnected within debounce period",
                 client->mac_addr);
            return false;
        }
    }

    if (type == BM_STEERING_KICK) {
        if (client->state != BM_CLIENT_STATE_CONNECTED ||
                                                client->band != BSAL_BAND_24G) {
            return false;
        }
    }

    if ((kick = bm_kick_find_by_macstr(client->mac_addr))) {
        // Already queued up -- so just update it
        kick->type = type;
        kick->rssi = rssi;
        kick->bsal = client->pair->bsal;
        kick->band = client->band;
        return true;
    }

    if (!(kick = (bm_kick_t *)calloc(1, sizeof(*kick)))) {
        LOGE("Failed to allocate memory for kick queue entry");
        return false;
    }

    if (!(os_nif_macaddr_from_str(&kick->macaddr, client->mac_addr))) {
        LOGE("Failed to parse mac address: '%s' for kick entry", client->mac_addr);
        free(kick);
        return false;
    }

    kick->type = type;
    kick->rssi = rssi;
    kick->bsal = client->pair->bsal;
    kick->band = client->band;
    ds_list_insert_tail(&bm_kick_queue, kick);

    if (ds_list_head(&bm_kick_queue) == kick) {
        // Kick off queue now
        bm_kick_queue_start();
    }

    LOGD("Queued %s kick for '%s'", bm_kick_get_kick_type_str( type ),
                                    client->mac_addr);
    return true;
}

void
bm_kick_measurement(os_macaddr_t macaddr, uint8_t rssi)
{
    bm_kick_t           *kick;

    if (!(kick = ds_list_head(&bm_kick_queue))) {
        return;
    }

    if (!kick->measuring ||
                memcmp(&macaddr, &kick->macaddr, sizeof(macaddr)) != 0) {
        return;
    }

    kick->measuring = false;
    kick->rssi = rssi;
    evsched_task(bm_kick_task_kick, kick, EVSCHED_ASAP);
    return;
}
