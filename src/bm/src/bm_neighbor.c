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
 * Band Steering Manager - Neighbors
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

#define MODULE_ID LOG_MODULE_ID_NEIGHBORS

/*****************************************************************************/
static ovsdb_update_monitor_t   bm_neighbor_ovsdb_update;
static ds_tree_t                bm_neighbors = DS_TREE_INIT( (ds_key_cmp_t *)strcmp,
                                                             bm_neighbor_t,
                                                             dst_node );

static c_item_t map_ovsdb_chanwidth[] = {
    C_ITEM_STR( RADIO_CHAN_WIDTH_20MHZ,         "HT20" ),
    C_ITEM_STR( RADIO_CHAN_WIDTH_40MHZ,         "HT40" ),
    C_ITEM_STR( RADIO_CHAN_WIDTH_40MHZ_ABOVE,   "HT40+" ),
    C_ITEM_STR( RADIO_CHAN_WIDTH_40MHZ_BELOW,   "HT40-" ),
    C_ITEM_STR( RADIO_CHAN_WIDTH_80MHZ,         "HT80" ),
    C_ITEM_STR( RADIO_CHAN_WIDTH_160MHZ,        "HT160" ),
    C_ITEM_STR( RADIO_CHAN_WIDTH_80_PLUS_80MHZ, "HT80+80" ),
    C_ITEM_STR( RADIO_CHAN_WIDTH_NONE,          "HT2040" )
};

uint8_t
bm_neighbor_get_op_class( uint8_t channel )
{
    if( channel >= 1 && channel <= 13 ) {
        return BTM_24_OP_CLASS;
    }

    if( channel >= 36 && channel <= 48 ) {
        return BTM_5GL_OP_CLASS;
    }

    if( channel >= 52 && channel <= 64 ) {
        return BTM_L_DFS_OP_CLASS;
    }

    if( channel >= 100 && channel <= 140 ) {
        return BTM_U_DFS_OP_CLASS;
    }

    if( channel >= 149 && channel <= 169 ) {
        return BTM_5GU_OP_CLASS;
    }

    return 0;
}

uint8_t
bm_neighbor_get_phy_type( uint8_t channel )
{
    if( channel >= 1 && channel <= 13 ) {
        return BTM_24_PHY_TYPE;
    }

    if( channel >= 36 && channel <= 169 ) {
        return BTM_5_PHY_TYPE;
    }

    return 0;
}

bool
bm_neighbor_get_self_neighbor(const bm_pair_t *pair, bsal_band_t band, bsal_neigh_info_t *neigh)
{
    struct  schema_Wifi_VIF_State   vif;
    json_t                          *jrow;
    pjs_errmsg_t                    perr;
    char                            *ifname;
    os_macaddr_t                    macaddr;

    memset(neigh, 0, sizeof(*neigh));

    // On platforms other than pods, the home-ap-* interfaces are mapped to
    // other platform-dependent names such as ath0, etc. On the pod, the
    // mapping is the same.
    ifname = target_unmap_ifname( (char *) pair->ifcfg[band].ifname );
    if( strlen( ifname ) == 0 ) {
        LOGE( "Unable to target_unmap_ifname '%s'", pair->ifcfg[band].ifname );
        return false;
    }

    json_t  *where = ovsdb_where_simple( SCHEMA_COLUMN( Wifi_VIF_State, if_name),
                                         ifname );

    /* TODO use ovsdb_sync_select() here */
    jrow = ovsdb_sync_select_where( SCHEMA_TABLE( Wifi_VIF_State ), where );

    if( !schema_Wifi_VIF_State_from_json(
                &vif,
                json_array_get( jrow, 0 ),
                false,
                perr ) )
    {
        LOGE( "Unable to parse Wifi_VIF_State column: %s", perr );
        json_decref(jrow);
        return false;
    }

    neigh->channel = ( uint8_t )vif.channel;

    if (!vif.mac_exists) {
        LOGE("%s: mac doesn't exists", ifname);
        json_decref(jrow);
        return false;
    }

    if( !os_nif_macaddr_from_str( &macaddr, vif.mac ) ) {
        LOGE( "Unable to parse mac address '%s'", vif.mac );
        json_decref(jrow);
        return false;
    }

    LOGT( "Got self channel: %d and self bssid: %s", vif.channel, vif.mac );
    memcpy( neigh->bssid, (uint8_t *)&macaddr, sizeof( neigh->bssid ) );

    // Assume the default BSSID_INFO
    neigh->bssid_info = BTM_DEFAULT_NEIGH_BSS_INFO;
    neigh->op_class = bm_neighbor_get_op_class(neigh->channel);
    neigh->phy_type = bm_neighbor_get_phy_type(neigh->channel);

    json_decref(jrow);
    return true;
}

static void
bm_neighbor_set_neighbor(const bsal_neigh_info_t *neigh_report)
{
    ds_tree_t       *pairs;
    bm_pair_t       *pair;

    if (!(pairs = bm_pair_get_tree())) {
        LOGE("%s failed to get pair tree", __func__);
        return;
    }

    ds_tree_foreach(pairs, pair) {
        bm_neighbor_remove_all_from_pair(pair);
        bm_neighbor_set_all_to_pair(pair);
    }
}

static void
bm_neighbor_remove_neighbor(const bsal_neigh_info_t *neigh_report)
{
    ds_tree_t       *pairs;
    bm_pair_t       *pair;
    unsigned int    i;

    if (!(pairs = bm_pair_get_tree())) {
        LOGE("%s failed to get pair tree", __func__);
        return;
    }

    ds_tree_foreach(pairs, pair) {
        for (i = 0; i < ARRAY_SIZE(pair->ifcfg); i++) {
            if (target_bsal_rrm_remove_neighbor(pair->ifcfg[i].ifname, neigh_report))
                LOGW("%s: remove_neigh: "PRI(os_macaddr_t)" failed", pair->ifcfg[i].ifname,
                     FMT(os_macaddr_pt, (os_macaddr_t *) neigh_report->bssid));
        }
    }
}

/*****************************************************************************/
static bool
bm_neighbor_from_ovsdb( struct schema_Wifi_VIF_Neighbors *nconf, bm_neighbor_t *neigh )
{
    os_macaddr_t bssid;
    c_item_t    *item;

    STRSCPY(neigh->ifname, nconf->if_name);
    STRSCPY(neigh->bssid,  nconf->bssid);

    neigh->channel  = nconf->channel;
    neigh->priority = nconf->priority;

    if (!nconf->ht_mode_exists) {
        neigh->ht_mode = RADIO_CHAN_WIDTH_20MHZ;
    } else {
        item = c_get_item_by_str( map_ovsdb_chanwidth, nconf->ht_mode );
        if( !item ) {
            LOGE( "Neighbor %s - unknown ht_mode '%s'", neigh->bssid, nconf->ht_mode );
            return false;
        }
        neigh->ht_mode  = (radio_chanwidth_t)item->key;
    }

    if(!os_nif_macaddr_from_str(&bssid, neigh->bssid)) {
        return false;
    }

    memcpy(&neigh->neigh_report.bssid, &bssid, sizeof(bssid));
    neigh->neigh_report.bssid_info = BTM_DEFAULT_NEIGH_BSS_INFO;
    neigh->neigh_report.channel = nconf->channel;
    neigh->neigh_report.op_class = bm_neighbor_get_op_class(nconf->channel);
    neigh->neigh_report.phy_type = bm_neighbor_get_phy_type(nconf->channel);

    return true;
}

static void
bm_neighbor_ovsdb_update_cb( ovsdb_update_monitor_t *self )
{
    struct schema_Wifi_VIF_Neighbors    nconf;
    pjs_errmsg_t                        perr;
    bm_neighbor_t                       *neigh;

    switch( self->mon_type )
    {
        case OVSDB_UPDATE_NEW:
        {
            if( !schema_Wifi_VIF_Neighbors_from_json( &nconf,
                                                      self->mon_json_new, false, perr )) {
                LOGE( "Failed to parse new Wifi_VIF_Neighbors row: %s", perr );
                return;
            }

            if ((neigh = ds_tree_find(&bm_neighbors, nconf.bssid))) {
                LOGE("Ignoring duplicate neighbor '%s' (orig uuid=%s, new uuid=%s)",
                      neigh->bssid, neigh->uuid, nconf._uuid.uuid);
                return;
            }

            neigh = calloc( 1, sizeof( *neigh ) );
            STRSCPY(neigh->uuid, nconf._uuid.uuid);

            if( !bm_neighbor_from_ovsdb( &nconf, neigh ) ) {
                LOGE( "Failed to convert row to neighbor info (uuid=%s)", neigh->uuid );
                free( neigh );
                return;
            }

            ds_tree_insert( &bm_neighbors, neigh, neigh->bssid );
            LOGN( "Initialized Neighbor VIF bssid:%s if-name:%s Priority: %hhu"
                  " Channel: %hhu HT-Mode: %hhu", neigh->bssid, neigh->ifname,
                                                  neigh->priority, neigh->channel,
                                                  neigh->ht_mode );
            bm_neighbor_set_neighbor(&neigh->neigh_report);
            break;
        }

        case OVSDB_UPDATE_MODIFY:
        {
            if( !( neigh = bm_neighbor_find_by_uuid( self->mon_uuid ))) {
                LOGE( "Unable to find Neighbor for modify with uuid=%s", self->mon_uuid );
                return;
            }

            if( !schema_Wifi_VIF_Neighbors_from_json( &nconf,
                                                      self->mon_json_new, true, perr )) {
                LOGE( "Failed to parse modeified Wifi_VIF_Neighbors row uuid=%s: %s", 
                                                                        self->mon_uuid, perr );
                return;
            }

            if( !bm_neighbor_from_ovsdb( &nconf, neigh ) ) {
                LOGE( "Failed to convert row to neighbor for modify (uuid=%s)", neigh->uuid );
                return;
            }

            LOGN( "Updated Neighbor %s", neigh->bssid );
            bm_neighbor_set_neighbor(&neigh->neigh_report);

            break;
        }

        case OVSDB_UPDATE_DEL:
        {
            if( !( neigh = bm_neighbor_find_by_uuid( self->mon_uuid ))) {
                LOGE( "Unable to find neighbor for delete with uuid=%s", self->mon_uuid );
                return;
            }

            LOGN( "Removing neighbor %s", neigh->bssid );
            bm_neighbor_remove_neighbor(&neigh->neigh_report);
            ds_tree_remove( &bm_neighbors, neigh );
            free( neigh );

            break;
        }

        default:
            break;
    }

    return;
}


/*****************************************************************************/

ds_tree_t *
bm_neighbor_get_tree( void )
{
    return &bm_neighbors;
}

bm_neighbor_t *
bm_neighbor_find_by_uuid( const char *uuid )
{
    bm_neighbor_t   *neigh;

    ds_tree_foreach( &bm_neighbors, neigh ) {
        if( !strcmp( neigh->uuid, uuid )) {
            return neigh;
        }
    }

    return NULL;
}

bm_neighbor_t *
bm_neighbor_find_by_macstr( char *mac_str )
{
    return ( bm_neighbor_t * )ds_tree_find( &bm_neighbors, (char *)mac_str );
}


/*****************************************************************************/
bool
bm_neighbor_init( void )
{
    LOGI( "BM Neighbors Initializing" );

    // Start OVSDB monitoring
    if( !ovsdb_update_monitor( &bm_neighbor_ovsdb_update,
                               bm_neighbor_ovsdb_update_cb,
                               SCHEMA_TABLE( Wifi_VIF_Neighbors ),
                               OMT_ALL ) ) {
        LOGE( "Failed to monitor OVSDB table '%s'", SCHEMA_TABLE( Wifi_VIF_Neighbors ) );
        return false;
    }

    return true;
}

bool
bm_neighbor_cleanup( void )
{
    ds_tree_iter_t  iter;
    bm_neighbor_t   *neigh;

    LOGI( "BM Neighbors cleaning up" );

    neigh = ds_tree_ifirst( &iter, &bm_neighbors );
    while( neigh ) {
        ds_tree_iremove( &iter );

        free( neigh );

        neigh = ds_tree_inext( &iter );
    }

    return true;
}

/*
 * In the future client should have list of available/supported
 * channels. Next we could chose compatible neighbors
 */
static bool
bm_neighbor_allowed(bm_neighbor_t *neighbor, bsal_band_t band)
{
    bool allowed = false;

    if (!neighbor)
        return allowed;

    switch (band) {
    case BSAL_BAND_24G:
        if (neighbor->channel <= 13)
            allowed = true;
        break;
    case BSAL_BAND_5G:
        if (neighbor->channel > 13)
            allowed = true;
        break;
    case BSAL_BAND_COUNT:
    default:
        allowed = true;
    }

    return allowed;
}

unsigned int
bm_neighbor_number(bm_client_t *client, bsal_band_t band)
{
    int neighbors = 0;
    bm_neighbor_t *bm_neigh;

    ds_tree_foreach(&bm_neighbors, bm_neigh) {
        if (!bm_neighbor_allowed(bm_neigh, band))
            continue;
        neighbors++;
    }

    return neighbors;
}

/* pair/group update */
static void
bm_neighbor_add_to_pair_by_band(const bm_pair_t *pair, bsal_band_t band, bool skip_2g_neighbors)
{
    bm_neighbor_t *neigh;
    unsigned int i;
    const char *ifname;

    ifname = pair->ifcfg[band].ifname;

    /* First add 5G neighbors */
    ds_tree_foreach(&bm_neighbors, neigh) {
        if (neigh->channel <= 13)
            continue;
        if (target_bsal_rrm_set_neighbor(ifname, &neigh->neigh_report))
            LOGW("%s: set_neigh: %s failed", ifname, neigh->bssid);
    }

    for (i = 0; i < ARRAY_SIZE(pair->self_neigh); i++) {
        if (pair->self_neigh[i].channel <= 13)
            continue;
        if (target_bsal_rrm_set_neighbor(ifname, &pair->self_neigh[i]))
            LOGW("%s: set_neigh: "PRI(os_macaddr_t)" failed", ifname,
                 FMT(os_macaddr_pt, (os_macaddr_t *) pair->self_neigh[i].bssid));
    }

    if (skip_2g_neighbors)
        return;

    /* Now add 2G neighbors */
    ds_tree_foreach(&bm_neighbors, neigh) {
        if (neigh->channel > 13)
            continue;
        if (target_bsal_rrm_set_neighbor(ifname, &neigh->neigh_report))
            LOGW("%s: set_neigh: %s failed", ifname, neigh->bssid);
    }

    for (i = 0; i < ARRAY_SIZE(pair->self_neigh); i++) {
        if (pair->self_neigh[i].channel > 13)
            continue;
        if (target_bsal_rrm_set_neighbor(ifname, &pair->self_neigh[i]))
            LOGW("%s: set_neigh: "PRI(os_macaddr_t)" failed", ifname,
                 FMT(os_macaddr_pt, (os_macaddr_t *) pair->self_neigh[i].bssid));
    }
}

void
bm_neighbor_set_all_to_pair(const bm_pair_t *pair)
{
    unsigned int band;
    bool skip_2g_neighbors;

    for (band = 0; band < ARRAY_SIZE(pair->ifcfg); band++) {
	if (band == BSAL_BAND_5G)
            skip_2g_neighbors = true;
	else
            skip_2g_neighbors = false;

        bm_neighbor_add_to_pair_by_band(pair, band, skip_2g_neighbors);
    }
}

void
bm_neighbor_remove_all_from_pair(const bm_pair_t *pair)
{
    bm_neighbor_t *neigh;
    unsigned int i, j;

    ds_tree_foreach(&bm_neighbors, neigh) {
        for (i = 0; i < ARRAY_SIZE(pair->ifcfg); i++) {
            if (target_bsal_rrm_remove_neighbor(pair->ifcfg[i].ifname, &neigh->neigh_report))
                LOGW("%s: remove_neigh: %s failed", pair->ifcfg[i].ifname, neigh->bssid);
	}
    }

    for (i = 0; i < ARRAY_SIZE(pair->ifcfg); i++) {
        for (j = 0; j < ARRAY_SIZE(pair->self_neigh); j++) {
            if (target_bsal_rrm_remove_neighbor(pair->ifcfg[i].ifname, &pair->self_neigh[j]))
                LOGW("%s: remove_neigh: "PRI(os_macaddr_t)" failed", pair->ifcfg[i].ifname,
                     FMT(os_macaddr_pt, (os_macaddr_t *) pair->self_neigh[j].bssid));
        }
    }
}
