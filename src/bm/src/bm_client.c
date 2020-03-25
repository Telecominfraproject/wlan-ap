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
 * Band Steering Manager - Clients
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
#include <math.h>

#include "bm.h"


/*****************************************************************************/
#define MODULE_ID LOG_MODULE_ID_CLIENT

/*****************************************************************************/
static ovsdb_update_monitor_t   bm_client_ovsdb_mon;
static ds_tree_t                bm_clients = DS_TREE_INIT((ds_key_cmp_t *)strcmp,
                                                          bm_client_t,
                                                          dst_node);

static c_item_t map_bsal_bands[] = {
    C_ITEM_STR(BSAL_BAND_24G,                       "2.4G"),
    C_ITEM_STR(BSAL_BAND_5G,                        "5G")
};

static c_item_t map_state_names[] = {
    C_ITEM_STR(BM_CLIENT_STATE_DISCONNECTED,    "DISCONNECTED"),
    C_ITEM_STR(BM_CLIENT_STATE_CONNECTED,       "CONNECTED"),
    C_ITEM_STR(BM_CLIENT_STATE_STEERING_5G,     "STEERING_5G"),
    C_ITEM_STR(BM_CLIENT_STATE_STEERING_2G,     "STEERING_2G"),
    C_ITEM_STR(BM_CLIENT_STATE_BACKOFF,         "BACKOFF")
};

static c_item_t map_ovsdb_reject_detection[] = {
    C_ITEM_STR(BM_CLIENT_REJECT_NONE,           "none"),
    C_ITEM_STR(BM_CLIENT_REJECT_PROBE_ALL,      "probe_all"),
    C_ITEM_STR(BM_CLIENT_REJECT_PROBE_NULL,     "probe_null"),
    C_ITEM_STR(BM_CLIENT_REJECT_PROBE_DIRECT,   "probe_direct"),
    C_ITEM_STR(BM_CLIENT_REJECT_AUTH_BLOCKED,   "auth_block")
};

static c_item_t map_ovsdb_kick_type[] = {
    C_ITEM_STR(BM_CLIENT_KICK_NONE,             "none"),
    C_ITEM_STR(BM_CLIENT_KICK_DISASSOC,         "disassoc"),
    C_ITEM_STR(BM_CLIENT_KICK_DEAUTH,           "deauth"),
    C_ITEM_STR(BM_CLIENT_KICK_BSS_TM_REQ,       "bss_tm_req"),
    C_ITEM_STR(BM_CLIENT_KICK_RRM_BR_REQ,       "rrm_br_req"),
    C_ITEM_STR(BM_CLIENT_KICK_BTM_DISASSOC,     "btm_disassoc"),
    C_ITEM_STR(BM_CLIENT_KICK_BTM_DEAUTH,       "btm_deauth"),
    C_ITEM_STR(BM_CLIENT_KICK_RRM_DISASSOC,     "rrm_disassoc"),
    C_ITEM_STR(BM_CLIENT_KICK_RRM_DEAUTH,       "rrm_deauth")
};

static c_item_t map_cs_modes[] = {
    C_ITEM_STR(BM_CLIENT_CS_MODE_OFF,           "off"),
    C_ITEM_STR(BM_CLIENT_CS_MODE_HOME,          "home"),
    C_ITEM_STR(BM_CLIENT_CS_MODE_AWAY,          "away")
};

static c_item_t map_cs_states[] = {
    C_ITEM_STR(BM_CLIENT_CS_STATE_NONE,             "none"),
    C_ITEM_STR(BM_CLIENT_CS_STATE_STEERING,         "steering"),
    C_ITEM_STR(BM_CLIENT_CS_STATE_EXPIRED,          "expired"),
    C_ITEM_STR(BM_CLIENT_CS_STATE_FAILED,           "failed"),
    C_ITEM_STR(BM_CLIENT_CS_STATE_XING_LOW,         "xing_low"),
    C_ITEM_STR(BM_CLIENT_CS_STATE_XING_HIGH,        "xing_high"),
    C_ITEM_STR(BM_CLIENT_CS_STATE_XING_DISABLED,    "xing_disabled")
};

static c_item_t map_ovsdb_pref_5g[] = {
    C_ITEM_STR(BM_CLIENT_5G_NEVER,              "never" ),
    C_ITEM_STR(BM_CLIENT_5G_HWM,                "hwm"   ),
    C_ITEM_STR(BM_CLIENT_5G_ALWAYS,             "always")
};

static c_item_t map_ovsdb_force_kick[] = {
    C_ITEM_STR(BM_CLIENT_FORCE_KICK_NONE,       "none"),
    C_ITEM_STR(BM_CLIENT_SPECULATIVE_KICK,      "speculative"),
    C_ITEM_STR(BM_CLIENT_DIRECTED_KICK,         "directed"),
    C_ITEM_STR(BM_CLIENT_GHOST_DEVICE_KICK,     "ghost_device")
};

/*****************************************************************************/
static bool     bm_client_to_bsal_conf(bm_client_t *client,
                                        bsal_band_t band, bsal_client_config_t *dest);
static bool     bm_client_add_to_pair(bm_client_t *client, bm_pair_t *pair);
static bool     bm_client_update_pair(bm_client_t *client, bm_pair_t *pair);
static bool     bm_client_remove_from_pair(bm_client_t *client, bm_pair_t *pair);
static bool     bm_client_add_to_all_pairs(bm_client_t *client);
static bool     bm_client_update_all_pairs(bm_client_t *client);
static bool     bm_client_remove_from_all_pairs(bm_client_t *client);
static bool     bm_client_from_ovsdb(struct schema_Band_Steering_Clients *bscli,
                                                                 bm_client_t *client);
static void     bm_client_remove(bm_client_t *client);
static void     bm_client_ovsdb_update_cb(ovsdb_update_monitor_t *self);
static void     bm_client_backoff(bm_client_t *client, bool enable);
static void     bm_client_disable_steering(bm_client_t *client);
static void     bm_client_task_backoff(void *arg);
static void     bm_client_state_change(bm_client_t *client,
                                                 bm_client_state_t state, bool force);


/*****************************************************************************/

static bool
bm_client_to_cs_bsal_conf( bm_client_t *client, bsal_client_config_t *dest, bool block )
{
    dest->blacklist             = false;
    dest->rssi_inact_xing       = 0;
    dest->auth_reject_reason    = client->cs_auth_reject_reason;

    if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ) {
        if( client->cs_probe_block ) {
            dest->rssi_probe_hwm    = BM_CLIENT_MIN_HWM;
            dest->rssi_probe_lwm    = BM_CLIENT_MAX_LWM;
        } else {
            dest->rssi_probe_hwm    = 0;
            dest->rssi_probe_lwm    = 0;
        }

        if( client->cs_auth_block ) {
            dest->rssi_auth_hwm     = BM_CLIENT_MIN_HWM;
            dest->rssi_auth_lwm     = BM_CLIENT_MAX_LWM;
        } else {
            dest->rssi_auth_hwm     = 0;
            dest->rssi_auth_lwm     = 0;
        }

        dest->rssi_high_xing        = 0;
        dest->rssi_low_xing         = 0;
    } else {
        if( block ) {
            dest->rssi_probe_hwm    = BM_CLIENT_MIN_HWM;
            dest->rssi_probe_lwm    = BM_CLIENT_MAX_LWM;

            dest->rssi_auth_hwm     = BM_CLIENT_MIN_HWM;
            dest->rssi_auth_lwm     = BM_CLIENT_MAX_LWM;

            dest->rssi_high_xing    = 0;
            dest->rssi_low_xing     = 0;
        } else {
            dest->rssi_probe_hwm    = 0;
            dest->rssi_probe_lwm    = 0;

            dest->rssi_auth_hwm     = 0;
            dest->rssi_auth_lwm     = 0;

            dest->rssi_high_xing    = client->cs_hwm;
            dest->rssi_low_xing     = client->cs_lwm;
        }
    }

    LOGD("cs %s block %d (probe %d-%d, auth %d-%d, xing %d-%d", client->mac_addr, block,
         dest->rssi_probe_lwm, dest->rssi_probe_hwm,
	 dest->rssi_auth_lwm, dest->rssi_auth_hwm,
	 dest->rssi_low_xing, dest->rssi_high_xing);

    return true;
}

static bool
bm_client_to_bsal_conf(bm_client_t *client, bsal_band_t band, bsal_client_config_t *dest)
{
    if( client->lwm == BM_KICK_MAGIC_NUMBER ) {
        dest->rssi_low_xing = 0;
    } else {
        dest->rssi_low_xing = client->lwm;
    }

    if (band == BSAL_BAND_24G && client->state != BM_CLIENT_STATE_BACKOFF) {
        /* Block client based on HWM */
        dest->blacklist             = false;

        if( client->pref_5g == BM_CLIENT_5G_ALWAYS ) {
            dest->rssi_probe_hwm    = BM_CLIENT_MIN_HWM;
        } else if( client->pref_5g == BM_CLIENT_5G_HWM ) {
            dest->rssi_probe_hwm    = client->hwm;
        } else {
            dest->rssi_probe_hwm    = 0;
        }
        LOGT( "Client '%s': Setting hwm to '%hhu'",
                                client->mac_addr, dest->rssi_probe_hwm );

        dest->rssi_probe_lwm        = client->lwm;
        dest->rssi_high_xing        = client->hwm;
        dest->rssi_inact_xing       = 0;

        if( client->pre_assoc_auth_block ) {
            LOGT( "Client '%s': Blocking auth requests for"
                  " pre-assocation band steering", client->mac_addr );
            // This value should always mirror dest->rssi_probe_hwm
            dest->rssi_auth_hwm     = dest->rssi_probe_hwm;
        } else {
            dest->rssi_auth_hwm     = 0;
        }

        dest->rssi_auth_lwm         = 0;
        dest->auth_reject_reason    = 0;
    }
    else {
        /* Don't block client */
        dest->blacklist             = false;
        dest->rssi_probe_hwm        = 0;
        dest->rssi_probe_lwm        = 0;
        dest->rssi_high_xing        = 0;
        dest->rssi_inact_xing       = 0;

        dest->rssi_auth_hwm         = 0;
        dest->rssi_auth_lwm         = 0;
        dest->auth_reject_reason    = 0;

        if (client->state == BM_CLIENT_STATE_BACKOFF &&  client->steer_during_backoff) {
           LOGD("bs %s steer during backoff", client->mac_addr);
           dest->rssi_high_xing = client->hwm;
        }
    }

    LOGD("bs %s band %d (probe %d-%d, auth %d-%d, xing %d-%d", client->mac_addr, band,
         dest->rssi_probe_lwm, dest->rssi_probe_hwm,
	 dest->rssi_auth_lwm, dest->rssi_auth_hwm,
	 dest->rssi_low_xing, dest->rssi_high_xing);

    return true;
}

static void bm_client_check_connected(bm_client_t *client, bm_pair_t *pair, bsal_band_t band, const os_macaddr_t macaddr)
{
    bsal_client_info_t          info;
    bsal_event_t                event;
    const char                  *ifname;

    ifname = pair->ifcfg[band].ifname;

    if (target_bsal_client_info(ifname, macaddr.addr, &info)) {
        LOGD("%s: Client %s no client info.", ifname, client->mac_addr);
        return;
    }

    if (!info.connected) {
        LOGD("%s: Client %s not connected.", ifname, client->mac_addr);
        return;
    }

    LOGI("%s: Client %s already connected.", ifname, client->mac_addr);

    /* create an event with correct CAPS */
    memset(&event, 0, sizeof(event));

    strncpy(event.ifname, ifname, BSAL_IFNAME_LEN);
    event.type = BSAL_EVENT_CLIENT_CONNECT;
    event.band = band;
    memcpy(&event.data.connect.client_addr,
           &macaddr,
           sizeof(event.data.connect.client_addr));

    event.data.connect.is_BTM_supported = info.is_BTM_supported;
    event.data.connect.is_RRM_supported = info.is_RRM_supported;
    event.data.connect.band_cap_2G = info.band_cap_2G | client->band_cap_2G;
    event.data.connect.band_cap_5G = info.band_cap_5G | client->band_cap_5G;
    memcpy(&event.data.connect.datarate_info, &info.datarate_info, sizeof(event.data.connect.datarate_info));
    memcpy(&event.data.connect.rrm_caps, &info.rrm_caps, sizeof(event.data.connect.rrm_caps));

    if (bm_events_client_cap_changed(client, &event)) {
        bm_stats_add_event_to_report(client, &event, CLIENT_CAPABILITIES, false);
    }

    bm_events_record_client_cap(client, &event);
    bm_client_connected(client, pair->bsal, band, &event);
}

static bool
bm_client_add_to_pair(bm_client_t *client, bm_pair_t *pair)
{
    bsal_client_config_t        cli_conf;
    os_macaddr_t                macaddr;

    if (!pair->enabled || !pair->bsal) {
        return true;
    }

    if (!os_nif_macaddr_from_str(&macaddr, client->mac_addr)) {
        LOGE("Failed to parse mac address '%s'", client->mac_addr);
        return false;
    }

    // 2.4G
    if (!bm_client_to_bsal_conf(client, BSAL_BAND_24G, &cli_conf)) {
        LOGE("Failed to convert client '%s' to BSAL 2.4G config", client->mac_addr);
        return false;
    }

    if (target_bsal_client_add(pair->ifcfg[BSAL_BAND_24G].ifname, (uint8_t *)&macaddr, &cli_conf) < 0) {
        LOGE("Failed to add client '%s' to BSAL:%s",
                                  client->mac_addr, pair->ifcfg[BSAL_BAND_24G].ifname);
        return false;
    }

    LOGD("Client '%s' added to BSAL:%s",
                                  client->mac_addr, pair->ifcfg[BSAL_BAND_24G].ifname);

    // 5G
    if (!bm_client_to_bsal_conf(client, BSAL_BAND_5G, &cli_conf)) {
        LOGE("Failed to convert client '%s' to BSAL 5G config", client->mac_addr);
        return false;
    }

    if (target_bsal_client_add(pair->ifcfg[BSAL_BAND_5G].ifname, (uint8_t *)&macaddr, &cli_conf) < 0) {
        LOGE("Failed to add client '%s' to BSAL:%s",
                                  client->mac_addr, pair->ifcfg[BSAL_BAND_5G].ifname);
        return false;
    }

    LOGD("Client '%s' added to BSAL:%s",
                                  client->mac_addr, pair->ifcfg[BSAL_BAND_5G].ifname);

    // Now check to see if client is already connected
    bm_client_check_connected(client, pair, BSAL_BAND_24G, macaddr);
    bm_client_check_connected(client, pair, BSAL_BAND_5G, macaddr);

    return true;
}

static bool
bm_client_update_pair(bm_client_t *client, bm_pair_t *pair)
{
    bsal_client_config_t        cli_conf;
    os_macaddr_t                macaddr;
    bsal_band_t                 blocked_band  = BSAL_BAND_24G;
    bsal_band_t                 steering_band = BSAL_BAND_5G;

    if (!pair->enabled || !pair->bsal) {
        return false;
    }

    if (!os_nif_macaddr_from_str(&macaddr, client->mac_addr)) {
        LOGE("Failed to parse mac address '%s'", client->mac_addr);
        return false;
    }

    if( client->steering_state != BM_CLIENT_CLIENT_STEERING ) {
        LOGT( "Client '%s': Applying Band Steering BSAL configuration", client->mac_addr );

        // 2.4G
        if (!bm_client_to_bsal_conf(client, BSAL_BAND_24G, &cli_conf)) {
            LOGE("Failed to convert client '%s' to BSAL 2.4G config", client->mac_addr);
            return false;
        }

        if (target_bsal_client_update(pair->ifcfg[BSAL_BAND_24G].ifname, (uint8_t *)&macaddr, &cli_conf) < 0) {
            LOGE("Failed to update client '%s' for BSAL:%s",
                    client->mac_addr, pair->ifcfg[BSAL_BAND_24G].ifname);
            return false;
        }

        LOGD("Client '%s' updated for BSAL:%s",
                client->mac_addr, pair->ifcfg[BSAL_BAND_24G].ifname);

        // 5G
        if (!bm_client_to_bsal_conf(client, BSAL_BAND_5G, &cli_conf)) {
            LOGE("Failed to convert client '%s' to BSAL 5G config", client->mac_addr);
            return false;
        }

        if (target_bsal_client_update(pair->ifcfg[BSAL_BAND_5G].ifname, (uint8_t *)&macaddr, &cli_conf) < 0) {
            LOGE("Failed to update client '%s' for BSAL:%s",
                    client->mac_addr, pair->ifcfg[BSAL_BAND_5G].ifname);
            return false;
        }

        LOGD("Client '%s' updated for BSAL:%s",
                client->mac_addr, pair->ifcfg[BSAL_BAND_5G].ifname);
    } else {
        LOGT( "Client '%s': Applying Client Steering BSAL configuration", client->mac_addr );

        if( client->cs_mode == BM_CLIENT_CS_MODE_HOME ) {
            if( client->cs_band == BSAL_BAND_5G ) {
                blocked_band  = BSAL_BAND_24G;
                steering_band = BSAL_BAND_5G;
            } else if( client->cs_band == BSAL_BAND_24G ) {
                blocked_band  = BSAL_BAND_5G;
                steering_band = BSAL_BAND_24G;
            }

            char *bandstr = c_get_str_by_key(map_bsal_bands, blocked_band);
            LOGD( "Client '%s': HOME mode, blocked band = %s", client->mac_addr, bandstr );

            if( !bm_client_to_cs_bsal_conf( client, &cli_conf, true ) ) {
                LOGE( "Failed to convert client '%s' to blocked BSAL config", client->mac_addr );
                return false;
            }

            // Blocked band
            if( target_bsal_client_update(pair->ifcfg[blocked_band].ifname, (uint8_t *)&macaddr, &cli_conf ) < 0 ) {
                LOGE( "Failed to update client '%s' for BSAL:%s",
                        client->mac_addr, pair->ifcfg[blocked_band].ifname );
                return false;
            }

            if( !bm_client_to_cs_bsal_conf( client, &cli_conf, false ) ) {
                LOGE("Failed to convert client '%s' to steering BSAL config", client->mac_addr);
                return false;
            }

            // Steering band
            if (target_bsal_client_update(pair->ifcfg[steering_band].ifname, (uint8_t *)&macaddr, &cli_conf) < 0) {
                LOGE("Failed to update client '%s' for BSAL:%s",
                        client->mac_addr, pair->ifcfg[steering_band].ifname);
                return false;
            }
        } else if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ) {
            LOGD( "Client '%s': AWAY mode, blocking both bands", client->mac_addr );

            // Block on both bands
            if( !bm_client_to_cs_bsal_conf( client, &cli_conf, true ) ) {
                LOGE( "Failed to convert client '%s' to blocked BSAL config", client->mac_addr );
                return false;
            }

            if( target_bsal_client_update(pair->ifcfg[BSAL_BAND_24G].ifname, (uint8_t *)&macaddr, &cli_conf ) < 0 ) {
                LOGE( "Failed to update client '%s' for BSAL:%s",
                        client->mac_addr, pair->ifcfg[BSAL_BAND_24G].ifname );
                return false;
            }

            if( target_bsal_client_update(pair->ifcfg[BSAL_BAND_5G].ifname, (uint8_t *)&macaddr, &cli_conf ) < 0 ) {
                LOGE( "Failed to update client '%s' for BSAL:%s",
                        client->mac_addr, pair->ifcfg[BSAL_BAND_5G].ifname );
                return false;
            }
        }
    }

    return true;
}

static bool
bm_client_remove_from_pair(bm_client_t *client, bm_pair_t *pair)
{
    os_macaddr_t                macaddr;

    if (!pair->enabled || !pair->bsal) {
        return false;
    }

    if (!os_nif_macaddr_from_str(&macaddr, client->mac_addr)) {
        LOGE("Failed to parse mac address '%s'", client->mac_addr);
        return false;
    }

    // 2.4G
    if (target_bsal_client_remove(pair->ifcfg[BSAL_BAND_24G].ifname, (uint8_t *)&macaddr) < 0) {
        LOGE("Failed to remove client '%s' from BSAL:%s",
                                   client->mac_addr, pair->ifcfg[BSAL_BAND_24G].ifname);
    }

    LOGD("Client '%s' removed from BSAL:%s",
                                   client->mac_addr, pair->ifcfg[BSAL_BAND_24G].ifname);

    // 5G
    if (target_bsal_client_remove(pair->ifcfg[BSAL_BAND_5G].ifname, (uint8_t *)&macaddr) < 0) {
        LOGE("Failed to remove client '%s' from BSAL:%s",
                                   client->mac_addr, pair->ifcfg[BSAL_BAND_5G].ifname);
    }

    LOGD("Client '%s' removed from BSAL:%s",
                                   client->mac_addr, pair->ifcfg[BSAL_BAND_5G].ifname);

    client->pair = NULL;

    return true;
}

static bool
bm_client_add_to_all_pairs(bm_client_t *client)
{
    ds_tree_t       *pairs;
    bm_pair_t       *pair;
    bool            success = true;

    if (!(pairs = bm_pair_get_tree())) {
        LOGE("bm_client_add_to_all_pairs() failed to get pair tree");
        return false;
    }

    ds_tree_foreach(pairs, pair) {
        if (bm_client_add_to_pair(client, pair) == false) {
            success = false;
        }
    }

    return success;
}

static bool
bm_client_update_all_pairs(bm_client_t *client)
{
    ds_tree_t       *pairs;
    bm_pair_t       *pair;
    bool            success = true;

    if (!(pairs = bm_pair_get_tree())) {
        LOGE("bm_client_update_all_pairs() failed to get pair tree");
        return false;
    }

    ds_tree_foreach(pairs, pair) {
        if (bm_client_update_pair(client, pair) == false) {
            success = false;
        }
    }

    return success;
}

static bool
bm_client_remove_from_all_pairs(bm_client_t *client)
{
    ds_tree_t       *pairs;
    bm_pair_t       *pair;
    bool            success = true;

    if (!(pairs = bm_pair_get_tree())) {
        LOGE("bm_client_remove_from_all_pairs() failed to get pair tree");
        return false;
    }

    ds_tree_foreach(pairs, pair) {
        if (bm_client_remove_from_pair(client, pair) == false) {
            success = false;
        }
    }

    return success;
}

static void
bm_client_remove(bm_client_t *client)
{
    if (!bm_client_remove_from_all_pairs(client)) {
        LOGW("Client '%s' failed to remove from one or more pairs", client->mac_addr);
    }

    while (evsched_task_cancel_by_find(NULL, client, EVSCHED_FIND_BY_ARG))
        ;

    bm_kick_cleanup_by_client(client);
    free(client);

    return;
}

static void
bm_client_cs_task( void *arg )
{
    bm_client_t     *client = arg;
    bsal_event_t    event;

    LOGN( "Client steering enforce period completed for client '%s'", client->mac_addr );

    client->cs_state = BM_CLIENT_CS_STATE_EXPIRED;

    bm_kick_cancel_btm_retry_task(client);
    bm_client_disable_client_steering( client );

    memset( &event, 0, sizeof( event ) );
    if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ) {
        event.band = BSAL_BAND_24G;
        bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_EXPIRED, false );

        event.band = BSAL_BAND_5G;
        bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_EXPIRED, false );
    } else if( client->cs_mode == BM_CLIENT_CS_MODE_HOME ) {
        event.band = client->cs_band;
        bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_EXPIRED, false );
    }

    return;
}

static void
bm_client_trigger_client_steering( bm_client_t *client )
{
    char            *modestr = c_get_str_by_key( map_cs_modes, client->cs_mode );
    bsal_event_t    event;

    if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ||
        ( client->cs_mode == BM_CLIENT_CS_MODE_HOME &&
          client->cs_band != BSAL_BAND_COUNT ) ) {
        // Set client steering state
        client->steering_state = BM_CLIENT_CLIENT_STEERING;
        LOGN( "Setting state to CLIENT STEERING for '%s'", client->mac_addr );

        if( client->cs_state != BM_CLIENT_CS_STATE_STEERING ) {
            memset( &event, 0, sizeof( event ) );
            if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ) {
                event.band = BSAL_BAND_24G;
                bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_STARTED, false );

                event.band = BSAL_BAND_5G;
                bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_STARTED, false );
            } else if( client->cs_mode == BM_CLIENT_CS_MODE_HOME ) {
                event.band = client->cs_band;
                bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_STARTED, false );
            }
        }

        // Change cs state to STEERING
        client->cs_state = BM_CLIENT_CS_STATE_STEERING;
        bm_client_update_cs_state( client );

        LOGN( "Triggering client steering for client '%s' and mode '%s'"\
              " and steering state: %d", client->mac_addr, modestr, client->steering_state );
    } else {
        // Band is unspecified. Apply band steering configuration for
        // this client
        LOGN( " Band unspecified client '%s', applying band steering" \
              " configuration", client->mac_addr );

        client->steering_state = BM_CLIENT_STEERING_NONE;
    }

    // Cancel any instances of the timer running
    evsched_task_cancel_by_find( bm_client_cs_task, client,
                               ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG ) );
    client->cs_task = evsched_task( bm_client_cs_task,
                                    client,
                                    EVSCHED_SEC( client->cs_enforce_period ) );

    return;
}

/*****************************************************************************/

static const char *
bm_client_get_rrm_bcn_rpt_param( struct schema_Band_Steering_Clients *bscli, char *key )
{
    int i;

    for( i = 0; i < bscli->rrm_bcn_rpt_params_len; i++ ) {
        const char *params_key = bscli->rrm_bcn_rpt_params_keys[i];
        const char *params_val = bscli->rrm_bcn_rpt_params[i];

        if( !strcmp( key, params_key ) ) {
            return params_val;
        }
    }

    return NULL;
}

static bool
bm_client_get_rrm_bcn_rpt_params( struct schema_Band_Steering_Clients *bscli,
                                  bm_client_t *client )
{
    const char      *val;

    if( !( val = bm_client_get_rrm_bcn_rpt_param( bscli, "enable_scan" ))) {
        client->enable_ch_scan = false;
    } else {
        if( !strcmp( val, "true" ) ) {
            client->enable_ch_scan = true;
        } else {
            client->enable_ch_scan = false;
        }
    }

    if( !( val = bm_client_get_rrm_bcn_rpt_param( bscli, "scan_interval" ))) {
        client->ch_scan_interval = RRM_BCN_RPT_DEFAULT_SCAN_INTERVAL;
    } else {
        client->ch_scan_interval = atoi( val );
    }

    return true;
}

static const char *
bm_client_get_cs_param( struct schema_Band_Steering_Clients *bscli, char *key )
{
    int i;

    for( i = 0; i < bscli->cs_params_len; i++ )
    {
        const char *cs_params_key = bscli->cs_params_keys[i];
        const char *cs_params_val = bscli->cs_params[i];

        if( !strcmp( key, cs_params_key ) )
        {
            return cs_params_val;
        }
    }

    return NULL;
}

static bool 
bm_client_get_cs_params( struct schema_Band_Steering_Clients *bscli, bm_client_t *client )
{
    c_item_t                *item;
    const char              *val;

    if( !(val = bm_client_get_cs_param( bscli, "hwm" ))) {
        client->cs_hwm = 0;
    } else {
        client->cs_hwm = atoi( val );
    }

    if( !(val = bm_client_get_cs_param( bscli, "lwm" ))) {
        client->cs_lwm = 0;
    } else {
        client->cs_lwm = atoi( val );
    }

    if( !(val = bm_client_get_cs_param( bscli, "cs_max_rejects" ))) {
        client->cs_max_rejects = 0;
    } else {
        client->cs_max_rejects = atoi( val );
    }

    if( !(val = bm_client_get_cs_param( bscli, "cs_max_rejects_period" ))) {
        client->cs_max_rejects_period = 0;
    } else {
        client->cs_max_rejects_period = atoi( val );
    }

    if( !(val = bm_client_get_cs_param( bscli, "cs_enforce_period" ))) {
        client->cs_enforce_period = 0;
    } else {
        client->cs_enforce_period = atoi( val );
    }

    if( !(val = bm_client_get_cs_param( bscli, "cs_reject_detection" ))) {
        client->cs_reject_detection = BM_CLIENT_REJECT_NONE;
    } else {
        item = c_get_item_by_str(map_ovsdb_reject_detection, val);
        if (!item) {
            LOGE("Client %s - unknown reject detection '%s'",
                    client->mac_addr, val);
            return false;
        }
        client->cs_reject_detection = (bm_client_reject_t)item->key;
    }

    if( !(val = bm_client_get_cs_param( bscli, "band" ))) {
        // ABS: Check if this is correct
        LOGD("%s - unknown band", client->mac_addr);
        client->cs_band = BSAL_BAND_COUNT;
    } else {
        item = c_get_item_by_str(map_bsal_bands, val);
        if( !item ) {
            LOGE(" Client %s - unknown band '%s'", client->mac_addr, val );
            return false;
        }
        client->cs_band = ( bsal_band_t )item->key;
    }

    if( !bscli->cs_mode_exists ) {
        client->cs_mode = BM_CLIENT_CS_MODE_OFF;
    } else {
        item = c_get_item_by_str(map_cs_modes, bscli->cs_mode);
        if( !item ) {
            LOGE(" Client %s - unknown Client Steering mode '%s'", client->mac_addr, val );
            return false;
        }
        client->cs_mode = (bm_client_cs_mode_t)item->key;
    }

    if( !bscli->cs_state_exists ) {
        client->cs_state = BM_CLIENT_CS_STATE_NONE;
    } else {
        item = c_get_item_by_str(map_cs_states, bscli->cs_state);
        if( !item ) {
            LOGE(" Client %s - unknown Client Steering state '%s'", client->mac_addr, val );
            return false;
        }
        client->cs_state = (bm_client_cs_state_t)item->key;
    }

    if( !(val = bm_client_get_cs_param( bscli, "cs_probe_block" ))) {
        client->cs_probe_block = false;
    } else {
        if( !strcmp( val, "true" ) ) {
            client->cs_probe_block = true;
        } else {
            client->cs_probe_block = false;
        }
    }

    if( !(val = bm_client_get_cs_param( bscli, "cs_auth_block" ))) {
        client->cs_auth_block = false;
    } else {
        if( !strcmp( val, "true" ) ) {
            client->cs_auth_block = true;
        } else {
            client->cs_auth_block = false;
        }
    }

    if( !(val = bm_client_get_cs_param( bscli, "cs_auth_reject_reason" ))) {
        // Value 0 is used for blocking authentication requests.
        // Hence, use -1 as default
        client->cs_auth_reject_reason = -1;
    } else {
        client->cs_auth_reject_reason = atoi( val );
    }

    // This value is true by default
    if( !(val = bm_client_get_cs_param( bscli, "cs_auto_disable" ))) {
        client->cs_auto_disable = true;
    } else {
        if( !strcmp( val, "false" ) ) {
            client->cs_auto_disable = false;
        } else {
            client->cs_auto_disable = true;
        }
    }


    return true;
}

static bsal_btm_params_t *
bm_client_get_btm_params_by_type( bm_client_t *client, bm_client_btm_params_type_t type )
{
    switch( type )
    {
        case BM_CLIENT_BTM_PARAMS_STEERING:
            return &client->steering_btm_params;

        case BM_CLIENT_BTM_PARAMS_STICKY:
            return &client->sticky_btm_params;

        case BM_CLIENT_BTM_PARAMS_SC:
            return &client->sc_btm_params;

        default:
            return NULL;
    }

    return NULL;
}

#define _bm_client_get_btm_param(bscli, type, key, val) \
    do { \
        int     i; \
        val = NULL; \
        for(i = 0;i < bscli->type##_btm_params_len;i++) { \
            if (!strcmp(key, bscli->type##_btm_params_keys[i])) { \
                val = bscli->type##_btm_params[i]; \
            } \
        } \
    } while(0)

static const char *
bm_client_get_btm_param( struct schema_Band_Steering_Clients *bscli, 
                        bm_client_btm_params_type_t type, char *key )
{
    char    *val = NULL;

    switch( type )
    {
        case BM_CLIENT_BTM_PARAMS_STEERING:
            _bm_client_get_btm_param( bscli, steering, key, val );
            break;

        case BM_CLIENT_BTM_PARAMS_STICKY:
            _bm_client_get_btm_param( bscli, sticky, key, val );
            break;

        case BM_CLIENT_BTM_PARAMS_SC:
            _bm_client_get_btm_param( bscli, sc, key, val );
            break;

        default:
            LOGW( "Unknown btm_params_type '%d'", type );
            break;
    }

    return val;
}

static bool
bm_client_get_btm_params( struct schema_Band_Steering_Clients *bscli,
                         bm_client_t *client, bm_client_btm_params_type_t type )
{
    bsal_btm_params_t           *btm_params  = NULL;
    bsal_neigh_info_t           *neigh       = NULL;
    os_macaddr_t                bssid;
    const char                  *val;
    char                        mac_str[18]  = { 0 };

    btm_params = bm_client_get_btm_params_by_type( client, type );
    if( !btm_params ) {
        LOGE( "Client %s - error getting BTM params type '%d'", client->mac_addr, type );
        return false;
    }
    memset(btm_params, 0, sizeof(*btm_params));

    // Process base BTM parameters
    if ((val = bm_client_get_btm_param( bscli, type, "valid_int" ))) {
        btm_params->valid_int = atoi( val );
    }
    else {
        btm_params->valid_int = BTM_DEFAULT_VALID_INT;
    }

    if ((val = bm_client_get_btm_param( bscli, type, "abridged" ))) {
        btm_params->abridged = atoi(val);
    }
    else {
        btm_params->abridged = BTM_DEFAULT_ABRIDGED;
    }

    if ((val = bm_client_get_btm_param( bscli, type, "pref" ))) {
        btm_params->pref = atoi( val );
    }
    else {
        btm_params->pref = BTM_DEFAULT_PREF;
    }

    if ((val = bm_client_get_btm_param( bscli, type, "disassoc_imminent" ))) {
        btm_params->disassoc_imminent = atoi( val );
    }
    else {
        btm_params->disassoc_imminent = BTM_DEFAULT_DISASSOC_IMMINENT;
    }

    if ((val = bm_client_get_btm_param( bscli, type, "bss_term" ))) {
        btm_params->bss_term = atoi( val );
    }
    else {
        btm_params->bss_term = BTM_DEFAULT_BSS_TERM;
    }

    if ((val = bm_client_get_btm_param( bscli, type, "btm_max_retries" ))) {
        btm_params->max_tries = atoi( val ) + 1;
    } else {
        btm_params->max_tries = BTM_DEFAULT_MAX_RETRIES + 1;
    }

    if ((val = bm_client_get_btm_param( bscli, type, "btm_retry_interval" ))) {
        btm_params->retry_interval = atoi( val );
    } else {
        btm_params->retry_interval = BTM_DEFAULT_RETRY_INTERVAL;
    }

    if( !( val = bm_client_get_btm_param( bscli, type, "inc_neigh" ))) {
        btm_params->inc_neigh = false;
    } else {
        if( !strcmp( val, "true" ) ) {
            btm_params->inc_neigh = true;
        } else {
            btm_params->inc_neigh = false;
        }
    }

    if( !( val = bm_client_get_btm_param( bscli, type, "inc_self" ))) {
        /* Base on disassoc imminent here */
        if (btm_params->disassoc_imminent)
            btm_params->inc_self = false;
        else
            btm_params->inc_self = true;
    } else {
        if( !strcmp( val, "true" ) ) {
            btm_params->inc_self = true;
        } else {
            btm_params->inc_self = false;
        }
    }

    // Check for static neighbor parameters
    if ((val = bm_client_get_btm_param( bscli, type, "bssid" ))) {
        neigh = &btm_params->neigh[0];
        STRSCPY(mac_str, val);
        if(strlen(mac_str) > 0) {
            if(!os_nif_macaddr_from_str( &bssid, mac_str)) {
                LOGE("bm_client_get_btm_params: Failed to parse bssid '%s'", mac_str);
                return false;
            }
            memcpy(&neigh->bssid, &bssid, sizeof(bssid));
            btm_params->num_neigh = 1;
        }

        if ((val = bm_client_get_btm_param( bscli, type, "bssid_info" ))) {
            neigh->bssid_info = atoi( val );
        }
        else {
            neigh->bssid_info = BTM_DEFAULT_NEIGH_BSS_INFO;
        }

        if ((val = bm_client_get_btm_param( bscli, type, "phy_type" ))) {
            neigh->phy_type = atoi( val );
        }

        if ((val = bm_client_get_btm_param( bscli, type, "channel" ))) {
            neigh->channel = atoi( val );

            if ((val = bm_client_get_btm_param( bscli, type, "op_class" ))) {
                neigh->op_class = atoi( val );
            }
        }
    }

    return true;
}

static bool
bm_client_from_ovsdb(struct schema_Band_Steering_Clients *bscli, bm_client_t *client)
{
    c_item_t                *item;

    STRSCPY(client->mac_addr, bscli->mac);

    if (!bscli->reject_detection_exists) {
        client->reject_detection = BM_CLIENT_REJECT_NONE;
    } else {
        item = c_get_item_by_str(map_ovsdb_reject_detection, bscli->reject_detection);
        if (!item) {
            LOGE("Client %s - unknown reject detection '%s'",
                                                client->mac_addr, bscli->reject_detection);
            return false;
        }
        client->reject_detection = (bm_client_reject_t)item->key;
    }

    if (!bscli->kick_type_exists) {
        client->kick_type = BM_CLIENT_KICK_NONE;
    } else {
        item = c_get_item_by_str(map_ovsdb_kick_type, bscli->kick_type);
        if (!item) {
            LOGE("Client %s - unknown kick type '%s'", client->mac_addr, bscli->kick_type);
            return false;
        }
        client->kick_type                   = (bm_client_kick_t)item->key;
    }

    if (!bscli->sc_kick_type_exists) {
        client->sc_kick_type = BM_CLIENT_KICK_NONE;
    } else {
        item = c_get_item_by_str(map_ovsdb_kick_type, bscli->sc_kick_type);
        if (!item) {
            LOGE("Client %s - unknown sc client kick type '%s'", client->mac_addr, bscli->sc_kick_type);
            return false;
        }
        client->sc_kick_type                = (bm_client_kick_t)item->key;
    }

    if (!bscli->sticky_kick_type_exists) {
        client->sticky_kick_type = BM_CLIENT_KICK_NONE;
    } else {
        item = c_get_item_by_str(map_ovsdb_kick_type, bscli->sticky_kick_type);
        if (!item) {
            LOGE("Client %s - unknown sticky client kick type '%s'", client->mac_addr, bscli->sc_kick_type);
            return false;
        }
        client->sticky_kick_type            = (bm_client_kick_t)item->key;
    }

    if (!bscli->pref_5g_exists) {
        client->pref_5g = BM_CLIENT_5G_NEVER;
    } else {
        item = c_get_item_by_str(map_ovsdb_pref_5g, bscli->pref_5g);
        if (!item) {
            LOGE("Client %s - unknown pref_5g value '%s'", client->mac_addr, bscli->pref_5g);
            return false;
        }
        client->pref_5g                     = (bm_client_pref_5g)item->key;
    }

    if (!bscli->force_kick_exists) {
        client->force_kick_type = BM_CLIENT_FORCE_KICK_NONE;
    } else {
        item = c_get_item_by_str(map_ovsdb_force_kick, bscli->force_kick);
        if (!item) {
            LOGE("Client %s - unknown force_kick value '%s'", client->mac_addr, bscli->force_kick);
            return false;
        }
        client->force_kick_type             = (bm_client_force_kick_t)item->key;
    }

    client->kick_reason                 = bscli->kick_reason;
    client->sc_kick_reason              = bscli->sc_kick_reason;
    client->sticky_kick_reason          = bscli->sticky_kick_reason;

    client->hwm                         = bscli->hwm;
    client->lwm                         = bscli->lwm;

    client->max_rejects                 = bscli->max_rejects;
    client->max_rejects_period          = bscli->rejects_tmout_secs;
    client->backoff_period              = bscli->backoff_secs;

    if (!bscli->backoff_exp_base_exists) {
        client->backoff_exp_base = BM_CLIENT_DEFAULT_BACKOFF_EXP_BASE;
    } else {
        client->backoff_exp_base = bscli->backoff_exp_base;
    }

    if (!bscli->steer_during_backoff_exists) {
        client->steer_during_backoff = BM_CLIENT_DEFAULT_STEER_DURING_BACKOFF;
    } else {
        client->steer_during_backoff = bscli->steer_during_backoff;
    }

    // If the kick_debounce_period or sc_kick_debounce_period was
    // changed, reset the last_kick time
    if( ( client->kick_debounce_period != bscli->kick_debounce_period ) ||
        ( client->sc_kick_debounce_period != bscli->kick_debounce_period ) )
    {
        client->times.last_kick = 0;
    }

    client->kick_debounce_period        = bscli->kick_debounce_period;
    client->sc_kick_debounce_period     = bscli->sc_kick_debounce_period;
    client->sticky_kick_debounce_period = bscli->sticky_kick_debounce_period;

    client->kick_upon_idle              = bscli->kick_upon_idle;
    client->pre_assoc_auth_block        = bscli->pre_assoc_auth_block;

    // Fetch all Client Steering parameters
    if( !bm_client_get_cs_params( bscli, client ) ) {
        LOGE( "Client %s - error getting client steering parameters",
                                                        client->mac_addr );
        return false;
    }

    // Fetch post-association transition management parameters
    if( !bm_client_get_btm_params( bscli, client, BM_CLIENT_BTM_PARAMS_STEERING )) {
        LOGE( "Client %s - error getting steering tm parameters", client->mac_addr );
        return false;
    }

    // Fetch sticky client transition management parameters
    if( !bm_client_get_btm_params( bscli, client, BM_CLIENT_BTM_PARAMS_STICKY ) ) {
        LOGE( "Client %s - error getting sticky tm parameters", client->mac_addr );
        return false;
    }

    // Fetch cloud-assisted(force kick) transition management parameters
    if( !bm_client_get_btm_params( bscli, client, BM_CLIENT_BTM_PARAMS_SC ) ) {
        LOGE( "Client %s - error getting sc tm parameters", client->mac_addr );
        return false;
    }

    // Fetch RRM Beacon Rpt Request parameters
    if( !bm_client_get_rrm_bcn_rpt_params( bscli, client ) ) {
        LOGE( "Client %s - error getting rrm_bcn_rpt params", client->mac_addr );
        return false;
    }

    return true;
}

#define ADD_STAT_BY_BAND(bandstr, bscli, key, val) \
        do { \
            int idx = bscli->stats_##bandstr##_len; \
            STRSCPY(bscli->stats_##bandstr##_keys[idx], key); \
            bscli->stats_##bandstr[idx] = val; \
            bscli->stats_##bandstr##_len++; \
        } while (0)

#define ADD_STAT(band, bscli, key, val) \
        do { \
            if (band == BSAL_BAND_24G) { \
                ADD_STAT_BY_BAND(2g, bscli, key, val); \
            } \
            else { \
                ADD_STAT_BY_BAND(5g, bscli, key, val); \
            } \
        } while (0)

/*
 * Check to see if OVSDB update transaction is from locally
 * initiated update or not.  Currently device only updates
 * the cs_state column.
 *
 * The cloud will only ever clear or set "none" to cs_state.
 */
static bool
bm_client_ovsdb_update_from_me(ovsdb_update_monitor_t *self,
                               struct schema_Band_Steering_Clients *bscli)
{
    c_item_t        *item;

    // Check if cs_state column was updated
    if (!json_object_get(self->mon_json_old,
                                SCHEMA_COLUMN(Band_Steering_Clients, cs_state))) {
        // It was NOT update: Cannot be my update
        return false;
    }

    // Check if new value is empty
    if (strlen(bscli->cs_state) == 0) {
        // It is empty: Cannot be my update
        return false;
    }

    // Decode new state value
    if (!bscli->cs_state_exists) {
        return false;
    } else if (!(item = c_get_item_by_str(map_cs_states, bscli->cs_state))) {
        // Could not decode state value, assume it's from cloud
        return false;
    }

    // Check if new state value is NONE
    if (item->key == BM_CLIENT_CS_STATE_NONE) {
        // It is, must be from cloud
        return false;
    }

    // Looks like it's our own update
    return true;
}

static bool
bm_client_lwm_toggled( uint8_t prev_lwm, bm_client_t *client, bool enable )
{
    if( enable ) {
        if( prev_lwm != BM_KICK_MAGIC_NUMBER &&
            client->lwm == BM_KICK_MAGIC_NUMBER ) {
            return true;
        }
    } else {
        if( prev_lwm == BM_KICK_MAGIC_NUMBER &&
            client->lwm != BM_KICK_MAGIC_NUMBER ) {
            return true;
        }
    }

    return false;
}

static bool
bm_client_force_kick_type_toggled( bm_client_force_kick_t prev_kick,
                                   bm_client_t *client, bool enable )
{
    if( enable ) {
        if( prev_kick == BM_CLIENT_FORCE_KICK_NONE &&
            client->force_kick_type != BM_CLIENT_FORCE_KICK_NONE ) {
            return true;
        }
    } else {
        if( prev_kick != BM_CLIENT_FORCE_KICK_NONE &&
            client->force_kick_type == BM_CLIENT_FORCE_KICK_NONE ) {
            return true;
        }
    }

    return false;
}

static void
bm_client_ovsdb_update_cb(ovsdb_update_monitor_t *self)
{
    struct schema_Band_Steering_Clients     bscli;
    pjs_errmsg_t                            perr;
    bm_client_t                             *client;
    bsal_event_t                            event;

    uint8_t                                 prev_lwm;
    bm_client_force_kick_t                  prev_force_kick;

    switch(self->mon_type) {

    case OVSDB_UPDATE_NEW:
        if (!schema_Band_Steering_Clients_from_json(&bscli,
                                                    self->mon_json_new, false, perr)) {
            LOGE("Failed to parse new Band_Steering_Clients row: %s", perr);
            return;
        }

        if ((client = bm_client_find_by_macstr(bscli.mac))) {
            LOGE("Ignoring duplicate client '%s' (orig uuid=%s, new uuid=%s)",
                                           client->mac_addr, client->uuid, bscli._uuid.uuid);
            return;
        }

        client = calloc(1, sizeof(*client));
        STRSCPY(client->uuid, bscli._uuid.uuid);

        if (!bm_client_from_ovsdb(&bscli, client)) {
            LOGE("Failed to convert row to client (uuid=%s)", client->uuid);
            free(client);
            return;
        }

        if( client->cs_mode != BM_CLIENT_CS_MODE_OFF ) {
            bm_client_trigger_client_steering( client );
        } else {
            if( client->steering_state == BM_CLIENT_CLIENT_STEERING ) {
                client->steering_state = BM_CLIENT_STEERING_NONE;
            }

            evsched_task_cancel_by_find( bm_client_cs_task, client,
                                       ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG ) );
        }

        if (!bm_client_add_to_all_pairs(client)) {
            LOGW("Client '%s' failed to add to one or more pairs", client->mac_addr);
        }

        ds_tree_insert(&bm_clients, client, client->mac_addr);
        LOGN("Added client %s (hwm=%u, lwm=%u, reject=%s, max_rejects=%d/%d sec)",
                                    client->mac_addr,
                                    client->hwm, client->lwm,
                                    c_get_str_by_key(map_ovsdb_reject_detection,
                                                     client->reject_detection),
                                    client->max_rejects, client->max_rejects_period);

        break;

    case OVSDB_UPDATE_MODIFY:
        if (!(client = bm_client_find_by_uuid(self->mon_uuid))) {
            LOGE("Unable to find client for modify with uuid=%s", self->mon_uuid);
            return;
        }

        if (!schema_Band_Steering_Clients_from_json(&bscli,
                                                    self->mon_json_new, true, perr)) {
            LOGE("Failed to parse modified Band_Steering_Clients row uuid=%s: %s",
                                                                    self->mon_uuid, perr);
            return;
        }

        /* Check to see if this is our own update of cs_state */
        /* NOTE: So this the sequence of events that the controller does when it sets client steering:
           ( each step is a separate ovsdb transaction )
            1. clear out cs_mode and other cs_params
            2. enable client steering, by setting cs_params, and cs_mode
            3. set lwm:=1 or set force_kick value
            4. set lwm:=0 or unset force_kick value

            The need for the below code comes because, after step 2, BM modifies
            the cs_state variable to tell the controller that it has set the cs_state to steering for
            this client. This write results in another ovsdb transaction. Since, this is a single variable
            change, it generally occurs at the same time as the kick value. OVSDB has been observed
            to combine the two into a single transaction, thereby resulting in the force being ignored.
        */
        if ( bm_client_ovsdb_update_from_me(self, &bscli) &&
           ( ( strlen( bscli.force_kick ) == 0 && bscli.lwm != BM_KICK_MAGIC_NUMBER ) ||
             ( !strcmp( bscli.force_kick, "none" )))) {
            LOGT("Ignoring my own client update");
            return;
        }

        // Get the existing values of force_kick and lwm for this client
        prev_lwm        = client->lwm;
        prev_force_kick = client->force_kick_type;

        if (!bm_client_from_ovsdb(&bscli, client)) {
            LOGE("Failed to convert row to client for modify (uuid=%s)", client->uuid);
            return;
        }

        if( bm_client_lwm_toggled( prev_lwm, client, true ) ||
            bm_client_force_kick_type_toggled( prev_force_kick, client, true ) )
        {
            // Force kick the client
            LOGN( "Client '%s': Force kicking due to cloud request", client->mac_addr );
            bm_kick(client, BM_FORCE_KICK, 0);
            return;
        } else if( bm_client_lwm_toggled( prev_lwm, client, false ) ||
                   bm_client_force_kick_type_toggled( prev_force_kick, client, false ) ) {
            LOGN( "Client '%s': Kicking mechanism toggled back, ignoring all" \
                  " pairs update", client->mac_addr );
            return;
        }

        if( client->cs_mode != BM_CLIENT_CS_MODE_OFF ) {
            bm_client_trigger_client_steering( client );
        } else {
            if( client->steering_state == BM_CLIENT_CLIENT_STEERING ) {
                client->steering_state = BM_CLIENT_STEERING_NONE;

                client->cs_state = BM_CLIENT_CS_STATE_EXPIRED;
                bm_client_update_cs_state( client );

                // NB: If the controller turns of client steering before cs_enforcement_period,
                //     the device does not if the cs_mode was set to home or away. Hence, send
                //     CLIENT_STEERING_DISABLED event on both bands
                memset( &event, 0, sizeof( event ) );

                event.band = BSAL_BAND_24G;
                bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_DISABLED, false );

                event.band = BSAL_BAND_5G;
                bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_DISABLED, false );
            }

            evsched_task_cancel_by_find( bm_client_cs_task, client,
                                       ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG ) );
        }

        if (!bm_client_update_all_pairs(client)) {
            LOGW("Client '%s' failed to update one or more pairs", client->mac_addr);
        }

        LOGN("Updated client %s (hwm=%u, lwm=%u, max_rejects=%d/%d sec)",
                                            client->mac_addr, client->hwm, client->lwm,
                                            client->max_rejects, client->max_rejects_period);

        break;

    case OVSDB_UPDATE_DEL:
        if (!(client = bm_client_find_by_uuid(self->mon_uuid))) {
            LOGE("Unable to find client for delete with uuid=%s", self->mon_uuid);
            return;
        }

        LOGN("Removing client %s", client->mac_addr);

        // Remove the client from the Band Steering report list
        bm_stats_remove_client_from_report( client );

        ds_tree_remove(&bm_clients, client);
        bm_client_remove(client);

        break;

    default:
        break;

    }
}

static void
bm_client_backoff(bm_client_t *client, bool enable)
{
    int connect_counter;
    int exp;

    client->backoff = enable;

    if (client->state == BM_CLIENT_STATE_BACKOFF) {
        // State cannot be backoff for enable or disable
        return;
    }

    if (enable) {
        bm_client_set_state(client, BM_CLIENT_STATE_BACKOFF);
        bm_client_update_all_pairs(client);


        connect_counter = client->backoff_connect_counter;
        if (connect_counter > 10)
            connect_counter = 10;

        exp = pow(client->backoff_exp_base, connect_counter);
        client->backoff_period_used = client->backoff_period * exp;

        LOGI("%s using backoff period %dsec * %d (%d)", client->mac_addr,
             client->backoff_period, exp, client->backoff_connect_counter);

        client->backoff_task = evsched_task(bm_client_task_backoff,
                                            client,
                                            EVSCHED_SEC(client->backoff_period_used));
    }
    else if (client->num_rejects != 0) {
        LOGI("%s disable backoff", client->mac_addr);
        client->num_rejects = 0;
        client->backoff_connect_calculated = false;
        evsched_task_cancel(client->backoff_task);
        bm_client_update_all_pairs(client);
    } else {
        LOGI("%s backoff(%d) num_rejects %d", client->mac_addr, enable, client->num_rejects);
    }

    return;
}

void
bm_client_disable_client_steering( bm_client_t *client )
{
    // Stop enforcing the client steering parameters
    evsched_task_cancel_by_find( bm_client_cs_task, client,
                               ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG ) );

    // Update the cs_state to OVSDB
    bm_client_update_cs_state( client );

    client->steering_state = BM_CLIENT_STEERING_NONE;

    // Disable client steering for this client, and reenable band steering
    bm_client_update_all_pairs( client );

    return;
}

static void
bm_client_disable_steering(bm_client_t *client) 
{
    // Disable band steering for this client
    client->hwm = 0;
    bm_client_update_all_pairs(client);
    return;
}

static void
bm_client_task_backoff(void *arg)
{
    bm_client_t         *client = arg;

    LOGN("'%s' backoff period has expired, re-enabling steering", client->mac_addr);

    // If the client has connected during backoff period:
    // - 0N 5G  : bm_client_state_change() disables backoff immediately and
    //            re-enables steering. As a result, the code never arrives here.
    // - ON 2.4G: state is changed to steering, but backoff timer is finish
    //            gracefully. State change to DISCONNECTED should not be done,
    //            and steering should be re-enabled
    if( client->state != BM_CLIENT_STATE_CONNECTED ) {
        bm_client_state_change(client, BM_CLIENT_STATE_DISCONNECTED, true);
    }

    bm_client_backoff(client, false);
    bm_stats_add_event_to_report( client, NULL, BACKOFF, false );
    return;
}

static void
bm_client_state_task( void *arg )
{
    bm_client_t         *client = arg;

    evsched_task_cancel_by_find( bm_client_state_task, client,
                               ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_FUNC ) );

    if( client->state == BM_CLIENT_STATE_CONNECTED ) {
        LOGT( "Client '%s' connected, client state machine in proper state",
                                                        client->mac_addr );
        return;
    }

    // Reset the client's state machine to DISCONNECTED state
    client->connected = false;
    bm_client_set_state( client, BM_CLIENT_STATE_DISCONNECTED );

    return;
}


static void
bm_client_state_change(bm_client_t *client, bm_client_state_t state, bool force)
{
    // The stats report interval is used to reset the client's state machine
    // from STEERING_5G to DISCONNECTED so that the client is not stuck in
    // in STEERING_5G for long periods of time.
    uint16_t    interval = bm_stats_get_stats_report_interval();

    if (!force && client->state == BM_CLIENT_STATE_BACKOFF) {
        if (state != BM_CLIENT_STATE_CONNECTED) {
            // Ignore state changes not forced while in backoff
            return;
        }
    }

    // Add backoff event to stats report only if the client was in
    // BM_CLIENT_STATE_BACKOFF
    if( client->state == BM_CLIENT_STATE_BACKOFF &&
        state == BM_CLIENT_STATE_CONNECTED && client->band == BSAL_BAND_5G ) {
        bm_stats_add_event_to_report( client, NULL, BACKOFF, false );
    }

    if (client->state != state) {
        LOGI("'%s' changed state %s -> %s",
                        client->mac_addr,
                        c_get_str_by_key(map_state_names, client->state),
                        c_get_str_by_key(map_state_names, state));
        client->state = state;
        client->times.last_state_change = time(NULL);

        switch(client->state)
        {
            case BM_CLIENT_STATE_CONNECTED:
            {
                // If the client has connected during backoff, only disable band
                // steering immediately if the client connects on 5GHz.
                if( client->band == BSAL_BAND_5G ) {
                    bm_client_backoff(client, false);
                }
                client->active = true;
                break;
            }

            case BM_CLIENT_STATE_STEERING_5G:
            {
                bm_stats_add_event_to_report( client, NULL, BAND_STEERING_ATTEMPT, false );
                evsched_task_cancel_by_find( bm_client_state_task, client,
                                           ( EVSCHED_FIND_BY_ARG | EVSCHED_FIND_BY_FUNC ) );

                client->state_task = evsched_task( bm_client_state_task,
                                                   client,
                                                   EVSCHED_SEC( interval ) );
                break;
            }

            default:
            {
                client->active = false;
                break;
            }

        }
    }

    return;
}

static void
bm_client_cs_task_rssi_xing( void *arg )
{
    bm_client_t     *client = arg;
    bsal_event_t    event;

    if( !client ) {
        LOGT( "Client arg is NULL" );
        return;
    }

    client->cs_state = BM_CLIENT_CS_STATE_XING_DISABLED;

    if( client->cs_auto_disable ) {
        LOGT( "Client '%s': Disabling client steering"
              " because of rssi xing", client->mac_addr );

        bm_client_disable_client_steering( client );

        memset( &event, 0, sizeof( event ) );
        if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ) {
            event.band = BSAL_BAND_24G;
            bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_DISABLED, false );

            event.band = BSAL_BAND_5G;
            bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_DISABLED, false );
        } else if( client->cs_mode == BM_CLIENT_CS_MODE_HOME ) {
            event.band = client->cs_band;
            bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_DISABLED, false );
        }
    } else {
        LOGT( "Client '%s': Updating the cs_state to XING_DISABLED",
                                                client->mac_addr );
        bm_client_update_cs_state( client );
    }

    return;
}

static void
bm_client_print_client_caps( bm_client_t *client )
{
    LOGD( " ~~~ Client '%s' ~~~", client->mac_addr );
    LOGD( " isBTMSupported        : %s", client->is_BTM_supported ? "Yes":"No" );
    LOGD( " isRRMSupported        : %s", client->is_RRM_supported ? "Yes":"No" );
    LOGD( " Supports 2G           : %s", client->band_cap_2G ? "Yes":"No" );
    LOGD( " Supports 5G           : %s", client->band_cap_5G ? "Yes":"No" );
 
    LOGD( "   ~~~Datarate Information~~~    " );
    LOGD( " Max Channel Width     : %hhu", client->datarate_info.max_chwidth );
    LOGD( " Max Streams           : %hhu", client->datarate_info.max_streams );
    LOGD( " PHY Mode              : %hhu", client->datarate_info.phy_mode );
    LOGD( " Max MCS               : %hhu", client->datarate_info.max_MCS );
    LOGD( " Max TX power          : %hhu", client->datarate_info.max_txpower );
    LOGD( " Is Static SMPS?       : %s", client->datarate_info.is_static_smps ? "Yes":"No" );
    LOGD( " Suports MU-MIMO       : %s", client->datarate_info.is_mu_mimo_supported? "Yes":"No" );

    LOGD( "   ~~~RRM Capabilites~~~     " );
    LOGD( " Link measurement      : %s", client->rrm_caps.link_meas ? "Yes":"No" );
    LOGD( " Neighbor report       : %s", client->rrm_caps.neigh_rpt ? "Yes":"No" );
    LOGD( " Beacon Report Passive : %s", client->rrm_caps.bcn_rpt_passive ? "Yes":"No" );
    LOGD( " Beacon Report Active  : %s", client->rrm_caps.bcn_rpt_active ? "Yes":"No" );
    LOGD( " Beacon Report Table   : %s", client->rrm_caps.bcn_rpt_table ? "Yes":"No" );
    LOGD( " LCI measurement       : %s", client->rrm_caps.lci_meas ? "Yes":"No");
    LOGD( " FTM Range report      : %s", client->rrm_caps.ftm_range_rpt ? "Yes":"No" );

    LOGD( " ~~~~~~~~~~~~~~~~~~~~ " );

    return;
}

/*****************************************************************************/
bool
bm_client_init(void)
{
    LOGI("Client Initializing");

    // Start OVSDB monitoring
    if (!ovsdb_update_monitor(&bm_client_ovsdb_mon,
                              bm_client_ovsdb_update_cb,
                              SCHEMA_TABLE(Band_Steering_Clients),
                              OMT_ALL)) {
        LOGE("Failed to monitor OVSDB table '%s'", SCHEMA_TABLE(Band_Steering_Clients));
        return false;
    }

    return true;
}

bool
bm_client_cleanup(void)
{
    ds_tree_iter_t  iter;
    bm_client_t     *client;

    LOGI("Client cleaning up");

    client = ds_tree_ifirst(&iter, &bm_clients);
    while(client) {
        ds_tree_iremove(&iter);

        bm_client_remove(client);

        client = ds_tree_inext(&iter);
    }

    return true;
}

void
bm_client_set_state(bm_client_t *client, bm_client_state_t state)
{
    bm_client_state_change(client, state, false);
    return;
}

void
bm_client_connected(bm_client_t *client, bsal_t bsal, bsal_band_t band, bsal_event_t *event)
{
    bm_client_stats_t           *stats;
    bm_client_times_t           *times;
    time_t                      now = time(NULL);

    if (!(client->pair = bm_pair_find_by_bsal(bsal))) {
        LOGE("Unable to find BM pair for connected client '%s'", client->mac_addr);
        return;
    }

    if (client->state == BM_CLIENT_STATE_CONNECTED) {
        if (event && client->band != band) {
            bm_stats_add_event_to_report( client, event, CONNECT, false );
        }
    } else {
	client->band = band;
        bm_client_set_state(client, BM_CLIENT_STATE_CONNECTED);
        if (event) {
            bm_stats_add_event_to_report( client, event, CONNECT, false );
        }
    }

    client->band = band;
    client->connected = true;
    client->xing_snr = 0;

    stats = &client->stats[band];
    stats->connects++;

    times = &client->times;
    times->last_connect = now;

    bm_client_print_client_caps( client );

    return;
}

void
bm_client_disconnected(bm_client_t *client)
{
    client->connected = false;
    client->xing_snr = 0;

    if (client->state == BM_CLIENT_STATE_CONNECTED) {
        bm_client_set_state(client, BM_CLIENT_STATE_DISCONNECTED);
    }
    return;
}

void
bm_client_rejected(bm_client_t *client, bsal_event_t *event)
{
    bm_client_stats_t       *stats              = &client->stats[event->band];
    bm_client_times_t       *times              = &client->times;
    int                     max_rejects         = client->max_rejects;
    int                     max_rejects_period  = client->max_rejects_period;
    time_t                  now                 = time(NULL);

    bsal_event_t            stats_event;

    if( client->cs_state == BM_CLIENT_CS_STATE_STEERING ) {
        max_rejects         = client->cs_max_rejects;
        max_rejects_period  = client->cs_max_rejects_period;
    }

    if (client->num_rejects > 0) {
        if ((now - times->reject.first) > max_rejects_period) {
            client->num_rejects = 0;
        }
    }

    stats->rejects++;
    client->num_rejects++;
    client->num_rejects_copy++;

    if( event->type == BSAL_EVENT_AUTH_FAIL ) {
        LOGD("'%s' auth reject %d/%d detected within %u seconds",
                                client->mac_addr, client->num_rejects,
                                max_rejects, max_rejects_period);
    } else {
        LOGD("'%s' reject %d/%d detected within %u seconds",
                                client->mac_addr, client->num_rejects,
                                max_rejects, max_rejects_period);
    }

    times->reject.last = now;
    if (client->num_rejects == 1) {
        times->reject.first = now;

        // If client is under CS_STATE_STEERING, this is the first probe request
        // that is blocked. Inform the cloud of the CS ATTEMPT
        if( client->cs_state == BM_CLIENT_CS_STATE_STEERING ) {
            bm_stats_add_event_to_report( client, event, CLIENT_STEERING_ATTEMPT, false );
        }
    }

    if (client->num_rejects == max_rejects) {
        stats->steering_fail_cnt++;

        if( client->steering_state == BM_CLIENT_CLIENT_STEERING ) {
            LOGW( "'%s' failed to client steer, disabling client steering ",
                            client->mac_addr );

            client->num_rejects = 0;

            // Update the cs_state to OVSDB
            client->cs_state = BM_CLIENT_CS_STATE_FAILED;
            bm_client_disable_client_steering( client );

            memset( &stats_event, 0, sizeof( stats_event ) );
            if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ) {
                stats_event.band = BSAL_BAND_24G;
                bm_stats_add_event_to_report( client, &stats_event, CLIENT_STEERING_FAILED, false );

                stats_event.band = BSAL_BAND_5G;
                bm_stats_add_event_to_report( client, &stats_event, CLIENT_STEERING_FAILED, false );
            } else if( client->cs_mode == BM_CLIENT_CS_MODE_HOME ) {
                stats_event.band = client->cs_band;
                bm_stats_add_event_to_report( client, &stats_event, CLIENT_STEERING_FAILED, false );
            }
        } else {
            LOGN("'%s' (total: %u times), %s...",
                    client->mac_addr,
                    stats->steering_fail_cnt,
                    client->backoff_period ?
                    "backing off" : "disabling");

            if (client->backoff_period) {
                bm_client_backoff(client, true);
                bm_stats_add_event_to_report( client, NULL , BACKOFF, true );
            }
            else {
                bm_client_disable_steering(client);
            }

        }
    }

    return;
}

void
bm_client_success(bm_client_t *client, bsal_band_t band)
{
    char                    *bandstr = c_get_str_by_key(map_bsal_bands, band);
    bm_client_stats_t       *stats = &client->stats[band];

    stats->steering_success_cnt++;
    LOGN("'%s' successfully steered to %s (total: %u times)",
                        client->mac_addr, bandstr, stats->steering_success_cnt);

    return;
}

void
bm_client_cs_connect( bm_client_t *client, bsal_band_t band )
{
    char            *bandstr = c_get_str_by_key(map_bsal_bands, band);
    bsal_event_t    event;

    memset( &event, 0, sizeof( event ) );

    if( client->cs_mode == BM_CLIENT_CS_MODE_HOME ) {
        LOGN( "Client steering successful for '%s' and %s band", client->mac_addr, bandstr );

        // Should this be success state?
        client->cs_state = BM_CLIENT_CS_STATE_EXPIRED;

        // NB: No need to send a CLIENT_STEERING_EXPIRED event here. When a client connects
        //     to the pod in the HOME mode, client steering is not stopped immediately and
        //     is stopped at the end of the enforecement period(when the CLIENT_STEERING_
        //     _EXPIRED event is sent automatically)
            
    } else if( client->cs_mode == BM_CLIENT_CS_MODE_AWAY ) {
        LOGN( "Client '%s' connected on %s band in AWAY mode", client->mac_addr, bandstr );

        client->cs_state = BM_CLIENT_CS_STATE_FAILED;

        event.band = client->cs_band;
        bm_stats_add_event_to_report( client, &event, CLIENT_STEERING_FAILED, false );
    }

    return;
}

bool
bm_client_update_cs_state( bm_client_t *client )
{
    struct      schema_Band_Steering_Clients bscli;
    c_item_t    *item;
    json_t      *js;
    char        *filter[] = { "+", SCHEMA_COLUMN( Band_Steering_Clients, cs_state ), NULL };

    // Reset the structure
    memset( &bscli, 0, sizeof( bscli ) );

    item = c_get_item_by_key( map_cs_states, client->cs_state );
    if( !item ) {
        LOGE( "Client '%s' - unknown CS state %d",
                            client->mac_addr, client->cs_state );
        return false;
    }

    STRSCPY(bscli.cs_state, (char *)item->data);
    bscli.cs_state_exists = true;

    js = schema_Band_Steering_Clients_to_json( &bscli, NULL );
    if( !js ) {
        LOGE( "Client '%s' failed to convert to schema", client->mac_addr );
        return false;
    }

    js = ovsdb_table_filter_row( js, filter );
    ovsdb_sync_update( SCHEMA_TABLE( Band_Steering_Clients ),
                       SCHEMA_COLUMN( Band_Steering_Clients, mac ),
                       client->mac_addr,
                       js );

    LOGT( "Client '%s' CS state updated to '%s'", client->mac_addr, (char *)item->data );

    return true;
}

void
bm_client_cs_check_rssi_xing( bm_client_t *client, bsal_event_t *event )
{
    bool                    rssi_xing = false;
    char                    *bandstr;
    evsched_task_t          task;

    bandstr  = c_get_str_by_key(map_bsal_bands, event->band);

    // This function should be called only when BSAL_EVENT_PROBE_REQ
    // is received.
    if( event->type != BSAL_EVENT_PROBE_REQ ) {
        return;
    }

    if( client->steering_state != BM_CLIENT_CLIENT_STEERING ) {
        LOGT( "Client '%s' not in 'client steering' mode", client->mac_addr );
        return;
    }

    if( client->cs_mode != BM_CLIENT_CS_MODE_AWAY ) {
        LOGT( "Client '%s' not in client steering 'AWAY' mode", client->mac_addr );
        return;
    }

    LOGT( "[%s] %s: RSSI: %2u hwm:%2u lwm:%2u", bandstr, client->mac_addr,
                                                event->data.probe_req.rssi,
                                                client->cs_hwm, client->cs_lwm );

    if( event->data.probe_req.rssi < BM_CLIENT_ROGUE_SNR_LEVEL ) {
        LOGD( "Client '%s' sent probe_req below acceptable signal"
              " strength, ignoring...", client->mac_addr );
        return;
    }

    if( client->cs_hwm && event->data.probe_req.rssi > client->cs_hwm ) {

        // client performed a high_xing higher;
        LOGT( "Client '%s' crossed HWM while in away mode", client->mac_addr );

        // Don't update the client->cs_state if its already in the required
        // state. Update it only if going from steering --> xing_high or
        // xing_low. Hopefully, the client does not go from xing_high to
        // xing_low or vice versa suddenly.
        if( client->cs_state != BM_CLIENT_CS_STATE_XING_HIGH ) {
            client->cs_state = BM_CLIENT_CS_STATE_XING_HIGH;
            rssi_xing = true;
        }

    } else if( client->cs_lwm && event->data.probe_req.rssi < client->cs_lwm ) {

        // client performed a low_xing lower;
        LOGT( "Client '%s' crossed LWM while in away mode", client->mac_addr );

        // Same logic as xing_high
        if( client->cs_state != BM_CLIENT_CS_STATE_XING_LOW ) {
            client->cs_state = BM_CLIENT_CS_STATE_XING_LOW;
            rssi_xing = true;
        }

    } else {

        // The client did not do any crossing.  
        LOGT( "Client '%s' within RSSI range, cancelling timer if running",
                                                        client->mac_addr );
        evsched_task_cancel_by_find( bm_client_cs_task_rssi_xing, client,
                                   ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG ) );
        return;
    }

    if( rssi_xing ) {
        task = evsched_task_find( bm_client_cs_task_rssi_xing, client,
                                ( EVSCHED_FIND_BY_FUNC | EVSCHED_FIND_BY_ARG ) );

        if( !task ) {
            LOGT( "Client '%s' has no rssi_xing task, firing one", client->mac_addr );
            client->rssi_xing_task = evsched_task( bm_client_cs_task_rssi_xing, client,
                                                   EVSCHED_SEC( BM_CLIENT_RSSI_HYSTERESIS ) );
        }
    }

    return;
}

bm_client_reject_t
bm_client_get_reject_detection( bm_client_t *client )
{
    bm_client_reject_t  reject_detection = BM_CLIENT_REJECT_NONE;

    if( !client ) {
        return reject_detection;
    }

    if( client->steering_state == BM_CLIENT_CLIENT_STEERING ) {
        reject_detection = client->cs_reject_detection;
    } else {
        reject_detection = client->reject_detection;
    }

    return reject_detection;
}

ds_tree_t *
bm_client_get_tree(void)
{
    return &bm_clients;
}

bm_client_t *
bm_client_find_by_uuid(const char *uuid)
{
    bm_client_t       *client;

    ds_tree_foreach(&bm_clients, client) {
        if (!strcmp(client->uuid, uuid)) {
            return client;
        }
    }

    return NULL;
}

bm_client_t *
bm_client_find_by_macstr(char *mac_str)
{
    return (bm_client_t *)ds_tree_find(&bm_clients, (char *)mac_str);
}

bm_client_t *
bm_client_find_by_macaddr(os_macaddr_t mac_addr)
{
    char              mac_str[MAC_STR_LEN];

    sprintf(mac_str, PRI(os_macaddr_lower_t), FMT(os_macaddr_t, mac_addr));
    return bm_client_find_by_macstr(mac_str);
}

bm_client_t *
bm_client_find_or_add_by_macaddr(os_macaddr_t *mac_addr)
{
    bm_client_t *client = bm_client_find_by_macaddr(*mac_addr);
    if (client) return client;
    // add new
    char mac_str[MAC_STR_LEN];
    sprintf(mac_str, PRI(os_macaddr_lower_t), FMT(os_macaddr_t, *mac_addr));
    client = calloc(1, sizeof(*client));
    STRSCPY(client->mac_addr, mac_str);
    ds_tree_insert(&bm_clients, client, client->mac_addr);
    LOGN("Added client %s", client->mac_addr);
    return client;
}

bool
bm_client_add_all_to_pair(bm_pair_t *pair)
{
    bm_client_t     *client;
    bool            success = true;

    ds_tree_foreach(&bm_clients, client) {
        if (bm_client_add_to_pair(client, pair) == false) {
            success = false;
        }
    }

    return success;
}

bool
bm_client_remove_all_from_pair(bm_pair_t *pair)
{
    bm_client_t     *client;
    bool            success = true;

    ds_tree_foreach(&bm_clients, client) {
        if (bm_client_remove_from_pair(client, pair) == false) {
            success = false;
        }
    }

    return success;
}
