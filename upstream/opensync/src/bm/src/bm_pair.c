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
 * Band Steering Manager - Interface Pair
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

#include "target.h"
#include "bm.h"

/*****************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN

/*****************************************************************************/

static ovsdb_update_monitor_t   bm_pair_ovsdb_update;
static ds_tree_t                bm_pairs = DS_TREE_INIT((ds_key_cmp_t *)strcmp,
                                                        bm_pair_t,
                                                        dst_node);

static c_item_t map_debug_levels[] = {
    C_ITEM_VAL(0,       BM_LOG_LEVEL_0),
    C_ITEM_VAL(1,       BM_LOG_LEVEL_1),
    C_ITEM_VAL(2,       BM_LOG_LEVEL_2)
};

/*****************************************************************************/

static bool
bm_pair_from_ovsdb(struct schema_Band_Steering_Config *bsconf, bm_pair_t *bp)
{
    uint32_t    log_severity;

    memset(&bp->ifcfg, 0, sizeof(bp->ifcfg));

    /* This could be handled on the Cloud */
    char *target_name_2g = target_map_ifname(bsconf->if_name_2g);
    if (!target_name_2g) {
        LOGE("Failed to map 2.4G ifname %s)", bsconf->if_name_2g);
        return false;
    }
    char *target_name_5g = target_map_ifname(bsconf->if_name_5g);
    if (!target_name_5g) {
        LOGE("Failed to map 5G ifname %s)", bsconf->if_name_5g);
        return false;
    }
    STRSCPY(bp->ifcfg[BSAL_BAND_24G].ifname, target_name_2g);
    STRSCPY(bp->ifcfg[BSAL_BAND_5G].ifname, target_name_5g);

#define SET_2G(x,y)     bp->ifcfg[BSAL_BAND_24G].x = bsconf->y
#define SET_5G(x,y)     bp->ifcfg[BSAL_BAND_5G].x  = bsconf->y
#define SET_BOTH(x)     SET_2G(x,x); SET_5G(x,x)

    // Chan util check interval in seconds
    SET_BOTH(chan_util_check_sec);

    // Number of chan util samples to average
    SET_BOTH(chan_util_avg_count);

    // Inactivity check interval in seconds
    SET_BOTH(inact_check_sec);

    // Inactivity timeout in seconds (normal)
    SET_BOTH(inact_tmout_sec_normal);

    // Inactivity timeout in seconds (overload)
    SET_BOTH(inact_tmout_sec_overload);

    // Default RSSI inactive threshold
    SET_BOTH(def_rssi_inact_xing);

    // Default RSSI low threshold
    SET_BOTH(def_rssi_low_xing);

    // Default RSSI threshold
    SET_BOTH(def_rssi_xing);

    // Debug flag: Raw channel utilization reports
    SET_2G(debug.raw_chan_util, dbg_2g_raw_chan_util);
    SET_5G(debug.raw_chan_util, dbg_5g_raw_chan_util);

    // Debug flag: Raw RSSI reports
    SET_2G(debug.raw_rssi, dbg_2g_raw_rssi);
    SET_5G(debug.raw_rssi, dbg_5g_raw_rssi);

#undef SET_2G
#undef SET_5G
#undef SET_BOTH

    // Channel Utilization water marks (thresholds)
    bp->chan_util_hwm         = bsconf->chan_util_hwm;
    bp->chan_util_lwm         = bsconf->chan_util_lwm;
    bp->stats_report_interval = bsconf->stats_report_interval;

    // Kick debouce threshold
    if (bsconf->kick_debounce_thresh > 0) {
        bp->kick_debounce_thresh = bsconf->kick_debounce_thresh;
    }
    else {
        bp->kick_debounce_thresh = BM_DEF_KICK_DEBOUNCE_THRESH;
    }

    // Kick debouce period
    if (bsconf->kick_debounce_period > 0) {
        bp->kick_debounce_period = bsconf->kick_debounce_period;
    }
    else {
        bp->kick_debounce_period = BM_DEF_KICK_DEBOUNCE_PERIOD;
    }

    // Success threshold
    if (bsconf->success_threshold_secs > 0) {
        bp->success_threshold = bsconf->success_threshold_secs;
    }
    else {
        bp->success_threshold = BM_DEF_SUCCESS_TMOUT_SECS;
    }

    // This could be added to LOG level OVSDB table for all managers!
    if (bsconf->debug_level) {
        if (bsconf->debug_level >= ARRAY_LEN(map_debug_levels)) {
            bp->debug_level = ARRAY_LEN(map_debug_levels) - 1;
        }
        else {
            bp->debug_level = bsconf->debug_level;
        }

        if (c_get_value_by_key(map_debug_levels, bp->debug_level, &log_severity)) {
            log_severity_set(log_severity);
        }
    }

    bp->gw_only = bsconf->gw_only;

    return true;
}

static void
bm_pair_ovsdb_update_cb(ovsdb_update_monitor_t *self)
{
    struct schema_Band_Steering_Config      bsconf;
    pjs_errmsg_t                            perr;
    bm_pair_t                               *bp;

    switch(self->mon_type) {

    case OVSDB_UPDATE_NEW:
        if (!schema_Band_Steering_Config_from_json(&bsconf,
                                                    self->mon_json_new, false, perr)) {
            LOGE("Failed to parse new Band_Steering_Config row: %s", perr);
            return;
        }

        bp = calloc(1, sizeof(*bp));
        STRSCPY(bp->uuid, bsconf._uuid.uuid);

        if (!bm_pair_from_ovsdb(&bsconf, bp)) {
            LOGE("Failed to convert row to if-config (uuid=%s)", bp->uuid);
            free(bp);
            return;
        }

        /*
         * XXX: maps radio type (2G, 5G, 5GL, 5GU) through target API using interface
         * names from last pair added.  This is to be replaced in the future when BM
         * has proper support for tri-radio platforms.
         */
        bm_stats_map_radio_type(BSAL_BAND_24G, bp->ifcfg[BSAL_BAND_24G].ifname);
        bm_stats_map_radio_type(BSAL_BAND_5G,  bp->ifcfg[BSAL_BAND_5G].ifname);

        if (target_bsal_iface_add(&bp->ifcfg[BSAL_BAND_24G])) {
            LOGE("Failed to add iface 24G to BSAL (uuid=%s)", bp->uuid);
            free(bp);
            return;
	}

        if (target_bsal_iface_add(&bp->ifcfg[BSAL_BAND_5G])) {
            target_bsal_iface_remove(&bp->ifcfg[BSAL_BAND_24G]);
            LOGE("Failed to add iface 5G t BSAL (uuid=%s)", bp->uuid);
            free(bp);
            return;
	}

        /* Temporary set this to make BM happy */
        bp->bsal = bp;
        bp->enabled = true;

        ds_tree_insert(&bm_pairs, bp, bp->uuid);
        LOGN("Initialized if-pair %s/%s (uuid=%s)",
                                                bp->ifcfg[BSAL_BAND_24G].ifname,
                                                bp->ifcfg[BSAL_BAND_5G].ifname,
                                                bp->uuid);

        if (!bm_client_add_all_to_pair(bp)) {
            LOGW("Failed to add one or more clients to if-pair %s/%s",
                                                bp->ifcfg[BSAL_BAND_24G].ifname,
                                                bp->ifcfg[BSAL_BAND_5G].ifname);
        }

        if (!bm_neighbor_get_self_neighbor(bp, BSAL_BAND_24G, &bp->self_neigh[BSAL_BAND_24G]))
            LOGW("Failed to get self neighbor %s", bp->ifcfg[BSAL_BAND_24G].ifname);

        if (!bm_neighbor_get_self_neighbor(bp, BSAL_BAND_5G, &bp->self_neigh[BSAL_BAND_5G]))
            LOGW("Failed to get self neighbor %s", bp->ifcfg[BSAL_BAND_5G].ifname);

        bm_neighbor_set_all_to_pair(bp);
        break;

    case OVSDB_UPDATE_MODIFY:
        if (!(bp = bm_pair_find_by_uuid((char *)self->mon_uuid))) {
            LOGE("Unable to find if-pair for modify with UUID %s", self->mon_uuid);
            return;
        }

        if (!schema_Band_Steering_Config_from_json(&bsconf,
                                                    self->mon_json_new, true, perr)) {
            LOGE("Failed to parse modified Band_Steering_Config row: %s", perr);
            return;
        }

        if (!bm_pair_from_ovsdb(&bsconf, bp)) {
            LOGE("Failed to convert row to if-config for modify (uuid=%s)", bp->uuid);
            return;
        }

        if (target_bsal_iface_update(&bp->ifcfg[BSAL_BAND_24G]) != 0) {
            LOGE("Failed to update BSAL 2.4G config (uuid=%s)", bp->uuid);
            return;
        }
        if (target_bsal_iface_update(&bp->ifcfg[BSAL_BAND_5G]) != 0) {
            LOGE("Failed to update BSAL 5G config (uuid=%s)", bp->uuid);
            return;
        }

        if (!bm_neighbor_get_self_neighbor(bp, BSAL_BAND_24G, &bp->self_neigh[BSAL_BAND_24G]))
            LOGW("Failed to get self neighbor %s", bp->ifcfg[BSAL_BAND_24G].ifname);

        if (!bm_neighbor_get_self_neighbor(bp, BSAL_BAND_5G, &bp->self_neigh[BSAL_BAND_5G]))
            LOGW("Failed to get self neighbor %s", bp->ifcfg[BSAL_BAND_5G].ifname);

	if (bsconf.if_name_2g_changed || bsconf.if_name_5g_changed)
            bm_neighbor_set_all_to_pair(bp);

        LOGN("Updated if-pair %s/%s (uuid=%s)", bp->ifcfg[BSAL_BAND_24G].ifname,
                                                bp->ifcfg[BSAL_BAND_5G].ifname,
                                                bp->uuid);
        break;

    case OVSDB_UPDATE_DEL:
        if (!(bp = bm_pair_find_by_uuid((char *)self->mon_uuid))) {
            LOGE("Unable to find if-pair for delete with UUID %s", self->mon_uuid);
            return;
        }

        if (!bm_client_remove_all_from_pair(bp)) {
            LOGW("Failed to remove one or more clients from if-pair %s/%s",
                                                bp->ifcfg[BSAL_BAND_24G].ifname,
                                                bp->ifcfg[BSAL_BAND_5G].ifname);
        }

        bm_neighbor_remove_all_from_pair(bp);
        bm_kick_cleanup_by_bsal(bp->bsal);
        if (target_bsal_iface_remove(&bp->ifcfg[BSAL_BAND_24G]) != 0) {
            LOGE("Failed to remove 2G (uuid=%s)", bp->uuid);
        }

        if (target_bsal_iface_remove(&bp->ifcfg[BSAL_BAND_5G]) != 0) {
            LOGE("Failed to remove 5G (uuid=%s)", bp->uuid);
        }

        LOGN("Removed if-pair %s/%s (uuid=%s)", bp->ifcfg[BSAL_BAND_24G].ifname,
                                                bp->ifcfg[BSAL_BAND_5G].ifname,
                                                bp->uuid);

        ds_tree_remove(&bm_pairs, bp);
        free(bp);
        break;

    default:
        break;

    }
}

/*****************************************************************************/
bool
bm_pair_init(void)
{
    LOGI("Interface Pair Initializing");

    // Start OVSDB monitoring
    if (!ovsdb_update_monitor(&bm_pair_ovsdb_update,
                              bm_pair_ovsdb_update_cb,
                              SCHEMA_TABLE(Band_Steering_Config),
                              OMT_ALL)) {
        LOGE("Failed to monitor OVSDB table '%s'", SCHEMA_TABLE(Band_Steering_Config));
        return false;
    }

    return true;
}

bool
bm_pair_cleanup(void)
{
    ds_tree_iter_t  iter;
    bm_pair_t       *bp;

    LOGI("Interface Pair cleaning up");

    bp = ds_tree_ifirst(&iter, &bm_pairs);
    while(bp) {
        ds_tree_iremove(&iter);

        if (bp->enabled && bp->bsal) {
            if (target_bsal_iface_remove(&bp->ifcfg[BSAL_BAND_24G]) != 0) {
                LOGW("Failed to remove ifpair from BSAL");
            }
            if (target_bsal_iface_remove(&bp->ifcfg[BSAL_BAND_5G]) != 0) {
                LOGW("Failed to remove ifpair from BSAL");
            }
        }
        free(bp);

        bp = ds_tree_inext(&iter);
    }

    return true;
}

ds_tree_t *
bm_pair_get_tree(void)
{
    return &bm_pairs;
}

bm_pair_t *
bm_pair_find_by_uuid(char *uuid)
{
    return (bm_pair_t *)ds_tree_find(&bm_pairs, uuid);
}

bm_pair_t *
bm_pair_find_by_bsal(bsal_t bsal)
{
    bm_pair_t       *bp;

    ds_tree_foreach(&bm_pairs, bp) {
        if (bp->bsal == bsal) {
            return bp;
        }
    }

    return NULL;
}

bm_pair_t *
bm_pair_find_by_ifname(const char *ifname)
{
    bm_pair_t       *bp;
    int             i;

    ds_tree_foreach(&bm_pairs, bp) {
        for(i = 0;i < BSAL_BAND_COUNT;i++) {
            if (!strcmp(bp->ifcfg[i].ifname, ifname)) {
                return bp;
            }
        }
    }

    return NULL;
}

bsal_band_t
bsal_band_find_by_ifname(const char *ifname)
{
    bm_pair_t       *bp;
    int             i;

    ds_tree_foreach(&bm_pairs, bp) {
        for(i = 0;i < BSAL_BAND_COUNT;i++) {
            if (!strcmp(bp->ifcfg[i].ifname, ifname)) {
                return i;
            }
        }
    }

    return BSAL_BAND_COUNT;
}
