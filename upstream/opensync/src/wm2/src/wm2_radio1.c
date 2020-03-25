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

/******************************************************************************
 *  WM2 radio v1 api
 *****************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "wm2.h"

#include "target.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

#define WM2_FLAG_RADIO_CONFIGURED           1
#define WM2_FLAG_VIF_POSTPONED              1
#define WM2_FLAG_VIF_CONFIGURED             2

#define BLACKLIST(SCHEMA, FILTER, TABLE, COLUMN) \
    do { \
        schema_filter_blacklist(filter, SCHEMA_COLUMN(TABLE, COLUMN)); \
        SCHEMA->COLUMN ## _present = false; \
    } while (0)

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

static void
wm2_radio_init_clear(
            ds_dlist_t              *init_cfg)
{
    ds_dlist_iter_t         radio_iter;
    ds_dlist_iter_t         vif_iter;
    target_radio_cfg_t     *radio;
    target_vif_cfg_t       *vif;

    for (   radio = ds_dlist_ifirst(&radio_iter, init_cfg);
            radio != NULL;
            radio = ds_dlist_inext(&radio_iter)) {

        for (   vif = ds_dlist_ifirst(&vif_iter, &radio->vifs_cfg);
                vif != NULL;
                vif = ds_dlist_inext(&vif_iter)) {
            ds_dlist_iremove(&vif_iter);
            free(vif);
            vif = NULL;
        }
        ds_dlist_iremove(&radio_iter);
        free(radio);
        radio = NULL;
    }
}


static bool
wm2_radio_update_VIF_state(
        struct schema_Wifi_Radio_State      *rstate_new,
        schema_filter_t                     *rfilter)
{
    struct schema_Wifi_Radio_State  rstate_old;
    struct schema_Wifi_VIF_State    vstate;
    schema_filter_t                 vfilter;
    int                             i;
    bool                            ret;
    char                            msg[64] = {0};

    memset(&rstate_old, 0, sizeof(rstate_old));
    memset(&vstate, 0, sizeof(vstate));
    schema_filter_init(&vfilter, "+");

    for(i=0; i<rfilter->num; i++)
    {
        if (!strcmp(rfilter->columns[i],SCHEMA_COLUMN(Wifi_Radio_State, channel)) ){
            SCHEMA_FF_SET_INT(&vfilter, &vstate, channel, rstate_new->channel);
            char channel[10];
            snprintf(channel, sizeof(channel), "%d", rstate_new->channel);
            strcat(msg, " channel: ");
            strcat(msg, channel);
        }
    }

    if (vfilter.num < 2){
        LOGD("Updating VIF for radio %s (skipped)",
                rstate_new->if_name);
        return true;
    }

    // Fetch VIF uuid's from radio state
    ret = ovsdb_table_select_one(&table_Wifi_Radio_State,
                           SCHEMA_COLUMN(Wifi_Radio_State, if_name),
                           rstate_new->if_name,
                           &rstate_old);
    if (!ret) {
        LOGE("Updating VIF for radio %s (Failed to get radio from OVSDB)",
                rstate_new->if_name);
        return false;
    }

    for (i=0; i < rstate_old.vif_states_len; i++) {
        ret =
            ovsdb_table_update_where_f(
                    &table_Wifi_VIF_State,
                    ovsdb_where_uuid("_uuid", rstate_old.vif_states[i].uuid),
                    &vstate,
                    vfilter.columns);

        if (!ret){
            LOGE("Updating VIF uuid %s for radio %s (Failed to update OVSDB)",
                    (char *)&rstate_old.vif_states[i],
                    rstate_old.if_name);
        }
    }

    LOGD("Updated VIF's with%s for radio %s",
            msg,
            rstate_new->if_name);

    return true;
}

// Warning: if_name must be populated
static void
wm2_radio_state_update_cb(
        struct schema_Wifi_Radio_State *rstate, schema_filter_t *filter)
{
    schema_filter_t rfilter;
    bool  ret;
    char  msg[64] = {0};

    tsnprintf(msg, sizeof(msg), "Updating radio state %s", rstate->if_name);
    LOGD("%s", msg);

    if (!*rstate->if_name) {
        LOGW("%s: if_name missing", msg);
        return;
    }

    if (rstate->_partial_update) {
        // if partial update ignore filter
        filter = NULL;
        goto do_update;
    }

    if (filter && filter->num <= 1) {
        LOGW("%s: no field selected", msg);
        return;
    }

    if (filter) {
        ret = wm2_radio_update_VIF_state(rstate, filter);
        if (!ret) {
            LOGE("Updating radio %s (Failed to update VIF state)", rstate->if_name);
        }
    }

    if (!filter) {
        filter = &rfilter;
        schema_filter_init(filter, "-");
    }

do_update:

    BLACKLIST(rstate, filter, Wifi_Radio_State, radio_config);
    BLACKLIST(rstate, filter, Wifi_Radio_State, vif_states);

    ret = ovsdb_table_update_f(&table_Wifi_Radio_State, rstate, filter ? filter->columns : NULL);

    if (ret) {
        LOGT("%s: Done", msg);
    } else {
        LOGE("%s: Error", msg);
    }
}

// Warning: if_name must be populated
void wm2_radio_config_update_cb(
        struct schema_Wifi_Radio_Config *rconf, schema_filter_t *filter)
{
    schema_filter_t s_filter;
    char        **fcolumns = NULL;
    char        msg[64];
    bool        ret;
    int         i;

    tsnprintf(msg, sizeof(msg), "Updating ovsdb radio conf %s", rconf->if_name);
    LOGI("%s", msg);

    if (!*rconf->if_name) {
        LOGW("%s: if_name missing", msg);
        return;
    }

    if (rconf->_partial_update) {
        // if partial update ignore filter
        filter = NULL;
        goto do_update;
    }

    if (filter && filter->num <= 1) {
        LOGW("%s: no field selected", msg);
        return;
    }

    if (!filter) {
        // Auto-generate filter
        filter = &s_filter;
        schema_filter_init(&s_filter, "+");
        i = s_filter.num;

#define UPDATE_FILTER(x)     if (rconf->x##_exists && (i < SCHEMA_FILTER_LEN)) \
                                s_filter.columns[i++] = SCHEMA_COLUMN(Wifi_Radio_Config, x)
#define UPDATE_OBJ_FILTER(x) if (rconf->x##_len > 0 && (i < SCHEMA_FILTER_LEN)) \
                                s_filter.columns[i++] = SCHEMA_COLUMN(Wifi_Radio_Config, x)

        UPDATE_FILTER(enabled);
        UPDATE_FILTER(hw_type);
        UPDATE_OBJ_FILTER(hw_config);
        UPDATE_FILTER(country);
        UPDATE_FILTER(channel);
        UPDATE_FILTER(channel_sync);
        UPDATE_FILTER(channel_mode);
        UPDATE_FILTER(hw_mode);
        UPDATE_FILTER(ht_mode);
        UPDATE_FILTER(thermal_shutdown);
        UPDATE_OBJ_FILTER(temperature_control);
        UPDATE_FILTER(tx_power);

#undef UPDATE_FILTER
#undef UPDATE_OBJ_FILTER

        if (i >= SCHEMA_FILTER_LEN) {
            LOGE("Radio config update callback %s (filter too large)",
                                                                  rconf->if_name);
            return;
        }
        else if (i <= 1) {
            LOGE("Radio config update callback %s (filter empty)",
                                                                  rconf->if_name);
            return;
        }
    }
    if (filter) {
        fcolumns = filter->columns;
    }

do_update:

    BLACKLIST(rconf, filter, Wifi_Radio_Config, vif_configs);

    ret = ovsdb_table_update_f(&table_Wifi_Radio_Config, rconf, fcolumns);
    if (ret) {
        LOGT("%s: Done", msg);
    }
    else {
        LOGE("%s: Error", msg);
    }
}

static void
wm2_radio_configure_postponed_vifs(
        ovsdb_update_monitor_t             *mon,
        struct schema_Wifi_Radio_Config    *radio_conf)
{
    ovsdb_cache_row_t                      *vconf_row;
    ovsdb_update_type_t                     orig_type;
    struct schema_Wifi_VIF_Config          *vconf;
    char                                   *uuid;
    int                                     i;

    for (i=0; i < radio_conf->vif_configs_len; i++) {
        uuid = radio_conf->vif_configs[i].uuid;
        vconf_row = ovsdb_cache_find_row_by_uuid(&table_Wifi_VIF_Config, uuid);
        if (!vconf_row || !vconf_row->user_flags) {
            continue;
        }
        vconf = (void*)vconf_row->record;
        if (vconf_row->user_flags == WM2_FLAG_VIF_POSTPONED) {
            /* FIXME: The `mon` here will belong to Radio
             *        update and it may contain NEW or
             *        MODIFY.  However VIF can be postponed
             *        only during NEW. It must be re-tried
             *        with NEW as well becuase it's the only
             *        way to insert the initial VIF_State
             *        entry for the VIF.
             *
             *        There are other mon-> fields that can
             *        be touched and still contain Radio
             *        table specific bits. Fortunately for
             *        NEW case they aren't touched. This
             *        should be fixed though.
             */
            orig_type = mon->mon_type;
            mon->mon_type = OVSDB_UPDATE_NEW;
            callback_Wifi_VIF_Config_v1(mon, NULL, vconf, vconf_row);
            mon->mon_type = orig_type;
        }
    }
}

static bool
wm2_radio_equal(
        ovsdb_update_monitor_t              *mon,
        struct schema_Wifi_Radio_Config     *rconf,
        struct schema_Wifi_Radio_Config     *rconf_set)
{
    struct schema_Wifi_Radio_State      rstate;
    int                                 index = 0;
    bool                                is_equal = true;

    memset(&rstate, 0, sizeof(rstate));
    bool ret = ovsdb_table_select_one(&table_Wifi_Radio_State,
                               SCHEMA_COLUMN(Wifi_Radio_State, if_name),
                               rconf->if_name,
                               &rstate);

    if (!ret){
        LOGW("Sync check (radio state missing)");
        memmove (rconf_set, rconf, sizeof(*rconf_set));
        return false;
    }

#define RADIO_EQUAL(equal) if (!equal) is_equal = false

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, channel))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, channel));
        if (!is_equal) {
            rconf_set->channel = rconf->channel;
            rconf_set->channel_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, channel_mode))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_STR(rconf, &rstate, channel_mode));
        if (!is_equal) {
            strcpy(rconf_set->channel_mode, rconf->channel_mode);
            rconf_set->channel_mode_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, channel_sync))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, channel_sync));
        if (!is_equal) {
            rconf_set->channel_sync = rconf->channel_sync;
            rconf_set->channel_sync_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, country))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_STR(rconf, &rstate, country));
        if (!is_equal) {
            strcpy(rconf_set->country, rconf->country);
            rconf_set->country_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, enabled))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, enabled));
        if (!is_equal) {
            rconf_set->enabled = rconf->enabled;
            rconf_set->enabled_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, freq_band))){
        if (strcmp(rconf->freq_band, rstate.freq_band)) {
            is_equal = false;
            strcpy(rconf_set->freq_band, rconf->freq_band);
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, ht_mode))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_STR(rconf, &rstate, ht_mode));
        if (!is_equal) {
            strcpy(rconf_set->ht_mode, rconf->ht_mode);
            rconf_set->ht_mode_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, hw_config))) {
        if (rconf->hw_config_len == rstate.hw_config_len) {
            for (index = 0; index < rconf->hw_config_len; index++) {
                RADIO_EQUAL(SCHEMA_FIELD_CMP_MAP_STR(rconf, &rstate, hw_config, index));
            }
        } else {
            is_equal = false;
        }
        if (!is_equal) {
            for (index = 0; index < rconf->hw_config_len; index++) {
                strcpy(rconf_set->hw_config[index], rconf->hw_config[index]);
                strcpy(rconf_set->hw_config_keys[index], rconf->hw_config_keys[index]);
            }
            rconf_set->hw_config_len = rconf->hw_config_len;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, hw_mode))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_STR(rconf, &rstate, hw_mode));
        if (!is_equal) {
            strcpy(rconf_set->hw_mode, rconf->hw_mode);
            rconf_set->hw_mode_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, thermal_shutdown))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, thermal_shutdown));
        if (!is_equal) {
            rconf_set->thermal_shutdown = rconf->thermal_shutdown;
            rconf_set->thermal_shutdown_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, thermal_integration))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, thermal_integration));
        if (!is_equal) {
            rconf_set->thermal_integration = rconf->thermal_integration;
            rconf_set->thermal_integration_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, thermal_downgrade_temp))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, thermal_downgrade_temp));
        if (!is_equal) {
            rconf_set->thermal_downgrade_temp = rconf->thermal_downgrade_temp;
            rconf_set->thermal_downgrade_temp_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, thermal_upgrade_temp))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, thermal_upgrade_temp));
        if (!is_equal) {
            rconf_set->thermal_upgrade_temp = rconf->thermal_upgrade_temp;
            rconf_set->thermal_upgrade_temp_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, tx_chainmask))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, tx_chainmask));
        if (!is_equal) {
            rconf_set->tx_chainmask = rconf->tx_chainmask;
            rconf_set->tx_chainmask_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, temperature_control))){
        if (rconf->temperature_control_len == rstate.temperature_control_len) {
            for (index = 0; index < rconf->temperature_control_len; index++) {
                RADIO_EQUAL(SCHEMA_FIELD_CMP_MAP_INT_STR(rconf, &rstate, temperature_control, index));
            }
        } else {
            is_equal = false;
        }
        if (!is_equal) {
            for (index = 0; index < rconf->hw_config_len; index++) {
                strcpy(rconf_set->temperature_control[index], rconf->temperature_control[index]);
                rconf_set->temperature_control_keys[index] = rconf->temperature_control_keys[index];
            }
            rconf_set->temperature_control_len = rconf->temperature_control_len;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Radio_Config, tx_power))) {
        RADIO_EQUAL(SCHEMA_FIELD_CMP_INT(rconf, &rstate, tx_power));
        if (!is_equal) {
            rconf_set->tx_power = rconf->tx_power;
            rconf_set->tx_power_exists = true;
        }
    }

#undef RADIO_EQUAL

   return is_equal;
}

void callback_Wifi_Radio_Config_v1(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_Radio_Config *old_rec,
        struct schema_Wifi_Radio_Config *rconf,
        ovsdb_cache_row_t *row)
{
    bool                                ret;
    struct schema_Wifi_Radio_State      rstate;
    struct schema_Wifi_Radio_Config     rconf_set;

    // configure
    switch (mon->mon_type) {
        default:
        case OVSDB_UPDATE_ERROR:
            LOGW("Radio config: mon upd error: %d", mon->mon_type);
            return;

        case OVSDB_UPDATE_DEL:
            // disable on delete
            rconf->enabled = false;
            // falls through

        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:

            memset(&rconf_set, 0, sizeof(rconf_set));
            if (!wm2_radio_equal(mon, rconf, &rconf_set)) {
                LOGD("Configuring radio %s through %s", rconf->if_name,
                        ovsdb_update_type_to_str(mon->mon_type));

                /* Set radio with new data */
                ret = target_radio_config_set (rconf->if_name, &rconf_set);
                if (true != ret) {
                    return;
                }

                LOGN("Configured radio %s band %s chan %d mode %s status %s",
                        rconf->if_name,
                        rconf->freq_band,
                        rconf->channel,
                        rconf->channel_mode,
                        rconf->enabled ? "enabled":"disabled");
            }
            else {
                LOGN("Configuring Radio %s (skipped)", rconf->if_name);
            }

            if (mon->mon_type == OVSDB_UPDATE_NEW) {
                // register callback for external (non-ovsdb) radio config update
                ret = target_radio_config_register(rconf->if_name, wm2_radio_config_update_cb);
                if (true != ret) {
                    LOGE("Updating radio %s (Failed to register callback)", rconf->if_name);
                    return;
                }
            }
            break;
    }

    // update state and register state callback
    switch (mon->mon_type) {
        case OVSDB_UPDATE_DEL:
            {
                // Cannot select on Wifi_Radio_State.rconfig == rconf->_uuid
                // because uuid references get removed automatically by ovsdb
                // immediately when the config record is removed, event before we get a
                // monitor update. So at this time Wifi_Radio_State.rconfig is empty.
                // Therefore we use if_name to identify the matching radio state.
                ovsdb_table_delete_simple(&table_Wifi_Radio_State,
                        SCHEMA_COLUMN(Wifi_Radio_State, if_name),
                        rconf->if_name);

                LOGD("Removed radio %s from state",
                        rconf->if_name);
            }
            break;
        case OVSDB_UPDATE_NEW:
            {
                memset(&rstate, 0, sizeof(rstate));
                ret = target_radio_state_get(rconf->if_name, &rstate);
                if (true != ret) {
                    LOGE("Updating radio %s (Failed to fetch state data)",
                            rconf->if_name);
                    return;
                }

                strcpy(rstate.radio_config.uuid, rconf->_uuid.uuid);
                rstate.radio_config_exists = true;

                char *filter[] = { "-", SCHEMA_COLUMN(Wifi_Radio_State, vif_states), NULL };
                ret = ovsdb_table_upsert_f(&table_Wifi_Radio_State, &rstate, false, filter);
                if (true != ret) {
                    LOGE("Updating radio %s (Failed to insert state)",
                            rstate.if_name);
                    return;
                }
                LOGD("Added radio %s to state", rstate.if_name);

                // NEW Radio mark Configured
                row->user_flags = WM2_FLAG_RADIO_CONFIGURED;
                wm2_radio_configure_postponed_vifs(mon, rconf);

                ret =
                    target_radio_state_register(
                            rconf->if_name,
                            wm2_radio_state_update_cb);
                if (true != ret) {
                    LOGE("Updating radio %s (Failed to register state data)",
                            rconf->if_name);
                    return;
                }
            }
            break;
        case OVSDB_UPDATE_MODIFY:
            {
                // state is updated by registered callbacks
                // LOGD("Updated radio %s to state", rconf->if_name);
                wm2_radio_configure_postponed_vifs(mon, rconf);
            }
            break;
        default:
            return;
    }
}

static ovsdb_cache_row_t*
wm2_radio_find_Radio_Config_by_vif_config(
        char                        *vconf_uuid)
{
    struct schema_Wifi_Radio_Config *conf;
    ovsdb_table_t                   *table = &table_Wifi_Radio_Config;
    ovsdb_cache_row_t               *row;
    int                             i;

    ds_tree_foreach(&table->rows, row) {
        conf = (void*)row->record;
        for (i=0; i < conf->vif_configs_len; i++) {
            if (strcmp(conf->vif_configs[i].uuid, vconf_uuid) == 0) { // match
                return row;
            }
        }
    }
    return NULL;
}

void wm2_vif_config_update_cb(
        struct schema_Wifi_VIF_Config *vconf, schema_filter_t *filter)
{
    schema_filter_t s_filter;
    char        **fcolumns = NULL;
    char        msg[64];
    bool        ret;
    int         i;

    tsnprintf(msg, sizeof(msg), "Updating VIF conf %s", vconf->if_name);

    if (!*vconf->if_name) {
        LOGW("%s: if_name missing", msg);
        return;
    }

    if (vconf->_partial_update) {
        // if partial update ignore filter
        filter = NULL;
        goto do_update;
    }

    if (filter && filter->num <= 1) {
        LOGW("%s: no field selected", msg);
        return;
    }

    if (!filter) {
        // Auto-generate filter
        filter = &s_filter;
        schema_filter_init(&s_filter, "+");
        i = s_filter.num;
#define UPDATE_FILTER(x)     if (vconf->x##_exists && (i < SCHEMA_FILTER_LEN)) \
                                s_filter.columns[i++] = SCHEMA_COLUMN(Wifi_VIF_Config, x)
#define UPDATE_OBJ_FILTER(x) if (vconf->x##_len > 0 && (i < SCHEMA_FILTER_LEN)) \
                                s_filter.columns[i++] = SCHEMA_COLUMN(Wifi_VIF_Config, x)

        UPDATE_FILTER(bridge);
        UPDATE_FILTER(enabled);
        UPDATE_FILTER(mode);
        UPDATE_FILTER(parent);
        UPDATE_FILTER(vif_dbg_lvl);
        UPDATE_FILTER(vif_radio_idx);
        UPDATE_FILTER(wds);
        UPDATE_FILTER(ssid);
        UPDATE_FILTER(ssid_broadcast);
        UPDATE_OBJ_FILTER(security);
        UPDATE_FILTER(mac_list_type);
        UPDATE_OBJ_FILTER(mac_list);

#undef UPDATE_FILTER
#undef UPDATE_OBJ_FILTER

        if (i >= SCHEMA_FILTER_LEN) {
            LOGE("Radio VIF config update callback %s (filter too large)",
                                                                  vconf->if_name);
            return;
        }
        else if (i <= 1) {
            LOGE("Radio VIF config update callback %s (filter empty)",
                                                                  vconf->if_name);
            return;
        }
    }
    if (filter) {
        fcolumns = filter->columns;
    }

do_update:

    ret = ovsdb_table_update_f(&table_Wifi_VIF_Config, vconf, fcolumns);
    if (ret) {
        LOGT("%s: Done", msg);
    }
    else {
        LOGE("%s: Error", msg);
    }
}

// Warning: if_name must be populated
static void
wm2_radio_vif_state_update_cb(
         struct schema_Wifi_VIF_State  *vstate, schema_filter_t *filter)
{
    schema_filter_t vfilter;
    bool ret;
    char msg[64] = {0};


    tsnprintf(msg, sizeof(msg), "Updating VIF state %s", vstate->if_name);
    LOGD("%s", msg);

    if (!*vstate->if_name) {
        LOGW("%s: if_name missing", msg);
        return;
    }

    if (vstate->_partial_update) {
        // if partial update ignore filter
        filter = NULL;
        goto do_update;
    }

    if (filter && filter->num <= 1) {
        LOGW("%s: no field selected", msg);
        return;
    }

    if (!filter) {
        filter = &vfilter;
        schema_filter_init(filter, "-");
    }

do_update:

    BLACKLIST(vstate, filter, Wifi_VIF_State, associated_clients);
    BLACKLIST(vstate, filter, Wifi_VIF_State, vif_config);

    ret = ovsdb_table_update_f(&table_Wifi_VIF_State, vstate, filter ? filter->columns : NULL);

    wm2_radio_update_port_state(vstate->if_name);

    if (ret){
        LOGT("%s: Done", msg);
    }
    else {
        LOGE("%s: Error", msg);
    }
}

static bool
wm2_vif_equal(
        ovsdb_update_monitor_t         *mon,
        struct schema_Wifi_VIF_Config  *vconf,
        struct schema_Wifi_VIF_Config  *vconf_set)
{
    struct schema_Wifi_VIF_State        vstate;
    int                                 index = 0;
    bool                                is_equal = true;


    memset(&vstate, 0, sizeof(vstate));
    bool ret = ovsdb_table_select_one(&table_Wifi_VIF_State,
                               SCHEMA_COLUMN(Wifi_VIF_State, if_name),
                               vconf->if_name,
                               &vstate);
    if (!ret){
        LOGW("Sync check (vif state missing)");
        memmove (vconf_set, vconf, sizeof(*vconf_set));
        return false;
    }

#define VIF_EQUAL(equal) if (!equal) is_equal = false

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, bridge))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_STR(vconf, &vstate, bridge));
        if (!is_equal) {
            strcpy(vconf_set->bridge, vconf->bridge);
            vconf_set->bridge_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, enabled))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, enabled));
        if (!is_equal) {
            vconf_set->enabled = vconf->enabled;
            vconf_set->enabled_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, ap_bridge))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, ap_bridge));
        if (!is_equal) {
            vconf_set->ap_bridge = vconf->ap_bridge;
            vconf_set->ap_bridge_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, mac_list))){
        if (vconf->mac_list_len == vstate.mac_list_len) {
            for (index = 0; index < vconf->mac_list_len; index++) {
                VIF_EQUAL(SCHEMA_FIELD_CMP_LIST_STR(vconf, &vstate, mac_list, index));
            }
        } else {
            is_equal = false;
        }
        if (!is_equal) {
            for (index = 0; index < vconf->mac_list_len; index++) {
                strcpy(vconf_set->mac_list[index], vconf->mac_list[index]);
            }
            vconf_set->mac_list_len = vconf->mac_list_len;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, mac_list_type))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_STR(vconf, &vstate, mac_list_type));
        if (!is_equal) {
            strcpy(vconf_set->mac_list_type, vconf->mac_list_type);
            vconf_set->mac_list_type_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, mode))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_STR(vconf, &vstate, mode));
        if (!is_equal) {
            strcpy(vconf_set->mode, vconf->mode);
            vconf_set->mode_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, parent))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_STR(vconf, &vstate, parent));
        if (!is_equal) {
            strcpy(vconf_set->parent, vconf->parent);
            vconf_set->parent_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, security))){
        if (vconf->security_len == vstate.security_len) {
            for (index = 0; index < vconf->security_len; index++) {
                VIF_EQUAL(SCHEMA_FIELD_CMP_MAP_STR(vconf, &vstate, security, index));
            }
        } else {
            is_equal = false;
        }
        if (!is_equal) {
            for (index = 0; index < vconf->security_len; index++) {
                strcpy(vconf_set->security[index], vconf->security[index]);
                strcpy(vconf_set->security_keys[index], vconf->security_keys[index]);
            }
            vconf_set->security_len = vconf->security_len;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, ssid))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_STR(vconf, &vstate, ssid));
        if (!is_equal) {
            strcpy(vconf_set->ssid, vconf->ssid);
            vconf_set->ssid_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, ssid_broadcast))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_STR(vconf, &vstate, ssid_broadcast));
        if (!is_equal) {
            strcpy(vconf_set->ssid_broadcast, vconf->ssid_broadcast);
            vconf_set->ssid_broadcast_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, vif_radio_idx))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, vif_radio_idx));
        if (!is_equal) {
            vconf_set->vif_radio_idx = vconf->vif_radio_idx;
            vconf_set->vif_radio_idx_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, uapsd_enable))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, uapsd_enable));
        if (!is_equal) {
            vconf_set->uapsd_enable = vconf->uapsd_enable;
            vconf_set->uapsd_enable_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, group_rekey))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, group_rekey));
        if (!is_equal) {
            vconf_set->group_rekey = vconf->group_rekey;
            vconf_set->group_rekey_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, ft_psk))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, ft_psk));
        if (!is_equal) {
            vconf_set->ft_psk = vconf->ft_psk;
            vconf_set->ft_psk_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, ft_mobility_domain))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, ft_mobility_domain));
        if (!is_equal) {
            vconf_set->ft_mobility_domain = vconf->ft_mobility_domain;
            vconf_set->ft_mobility_domain_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, vlan_id))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, vlan_id));
        if (!is_equal) {
            vconf_set->vlan_id = vconf->vlan_id;
            vconf_set->vlan_id_exists = true;
        }
    }
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_VIF_Config, wds))) {
        VIF_EQUAL(SCHEMA_FIELD_CMP_INT(vconf, &vstate, wds));
        if (!is_equal) {
            vconf_set->wds = vconf->wds;
            vconf_set->wds_exists = true;
        }
    }

#undef VIF_EQUAL

   return is_equal;
}

void callback_Wifi_VIF_Config_v1(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_VIF_Config   *old_rec,
        struct schema_Wifi_VIF_Config   *vconf,
        ovsdb_cache_row_t               *row)
{
    bool                                ret;
    ovsdb_cache_row_t                  *radio_conf_row = NULL;
    struct schema_Wifi_VIF_State        vstate;
    struct schema_Wifi_VIF_Config       vconf_set;

    if (mon->mon_type == OVSDB_UPDATE_NEW) {
        /* Only configure VIF if associated Radio has
         * been configured first because Radio_State has
         * to exist so that vif_states reference can be
         * updated.
         *
         * Moreover currently libtarget may not be able
         * to tell which radio newly created vif
         * interface should descend from.
         *
         * Therefore config must be deferred until after
         * there's a Radio_Config entry that contains
         * this VIF_Config entry's uuid in vif_config.
         */
        radio_conf_row = wm2_radio_find_Radio_Config_by_vif_config(vconf->_uuid.uuid);
        if (!radio_conf_row || radio_conf_row->user_flags != WM2_FLAG_RADIO_CONFIGURED) {
            LOGW("Skip configuring VIF %s (Radio not %s)",
                 vconf->if_name,
                 radio_conf_row ? "configured" : "found");
            row->user_flags = WM2_FLAG_VIF_POSTPONED;
            return;
        }
    }

    // configure
    switch (mon->mon_type) {
        default:
        case OVSDB_UPDATE_ERROR:
            LOGW("VIF config: mon upd error: %d", mon->mon_type);
            return;

        case OVSDB_UPDATE_DEL:
            /* The schema contains value before delete, therefore
               disable the interfaces status */
            vconf->enabled = false;
            vconf->enabled_exists = true;
            vconf->mac_list_len = 0;
            // falls through

        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:

            memset(&vconf_set, 0, sizeof(vconf_set));
            strcpy(vconf_set.if_name, vconf->if_name);
            if (!wm2_vif_equal(mon, vconf, &vconf_set)){
                LOGD("Configuring VIF %s through %s", vconf->if_name,
                        ovsdb_update_type_to_str(mon->mon_type));
                ret = target_vif_config_set(vconf->if_name, &vconf_set);
                if (true != ret) {
                    return;
                }
                LOGN("Configured VIF %s ssid %s status %s",
                        vconf->if_name,
                        vconf->ssid,
                        vconf->enabled ? "enabled":"disabled");
            }
            else {
                LOGN("Configuring VIF %s (skipped)", vconf->if_name);
            }

            if (mon->mon_type == OVSDB_UPDATE_NEW) {
                // register callback for external (non-ovsdb) VIF config update
                ret = target_vif_config_register(vconf->if_name, wm2_vif_config_update_cb);
                if (true != ret) {
                    LOGE("Updating VIF %s (Failed to register callback)", vconf->if_name);
                    return;
                }
            }
            break;
    }

    // update status
    switch (mon->mon_type) {
        case OVSDB_UPDATE_DEL:
            {
                // can't select on vconf._uuid because reference to
                // it was already removed by ovsdb automatically
                ovsdb_table_delete_simple(&table_Wifi_VIF_State,
                        SCHEMA_COLUMN(Wifi_VIF_State, if_name),
                        vconf->if_name);

                wm2_radio_update_port_state(vconf->if_name);

                // delete_with_parent for Radio_State.vif_states
                // is not necessary because ovsdb removes
                // references automatically upon their deletion.
                LOGD("Removed VIF %s to state", vconf->if_name);
            }
            break;
        case OVSDB_UPDATE_NEW:
            {
                if (!radio_conf_row) {
                    LOGE("Skipping VIF %s state update (radio conf row is NULL)", vconf->if_name);
                    return;
                }

                memset(&vstate, 0, sizeof(vstate));
                ret = target_vif_state_get(vconf->if_name, &vstate);
                if (true != ret) {
                    LOGE("Updating VIF %s (Failed to fetch state data)", vconf->if_name);
                    return;
                }

                struct schema_Wifi_Radio_Config *radio_conf = (void*)radio_conf_row->record;
                // upsert to ovsdb and insert reference into Radio_State.vif_states
                strcpy(vstate.vif_config.uuid, vconf->_uuid.uuid);
                vstate.vif_config_exists = true;
                char *filter[] = { "-", SCHEMA_COLUMN(Wifi_VIF_State, associated_clients), NULL };
                json_t *parent_where = ovsdb_where_uuid(
                        SCHEMA_COLUMN(Wifi_Radio_State, radio_config),
                        radio_conf->_uuid.uuid);
                ret = ovsdb_table_upsert_with_parent(
                        &table_Wifi_VIF_State,
                        &vstate,
                        false, // uuid not needed
                        filter,
                        // parent:
                        SCHEMA_TABLE(Wifi_Radio_State),
                        parent_where,
                        SCHEMA_COLUMN(Wifi_Radio_State, vif_states));
                if (true != ret) {
                    LOGE("Updating VIF %s (Failed to insert state)", vstate.if_name);
                    return;
                }

                LOGD("Added VIF %s to state", vstate.if_name);

                // Set associated clients once VIF state is set up
                wm2_clients_init(vconf->if_name);

                ret = target_vif_state_register(vconf->if_name, wm2_radio_vif_state_update_cb);
                if (true != ret) {
                    LOGE("Updating VIF %s (Failed to register state data)", vconf->if_name);
                    return;
                }

                wm2_radio_update_port_state(vconf->if_name);

                // mark VIF configured
                row->user_flags = WM2_FLAG_VIF_CONFIGURED;
            }
            break;
        case OVSDB_UPDATE_MODIFY:
            {
                // state is updated by registered callbacks
                // LOGD("Updated VIF %s to state", vconf->if_name);
                wm2_radio_update_port_state(vconf->if_name);
            }
            break;
        default:
            return;
    }
}

void wm2_radio_config_init_v1()
{
    bool                            ret;
    ds_dlist_t                      init_cfg;
    target_radio_cfg_t             *radio;
    target_vif_cfg_t               *vif;

    /* Update state at init */
    struct schema_Wifi_Radio_State  rstate;
    struct schema_Wifi_VIF_State    vstate;

    if (!target_radio_config_init(&init_cfg)) {
        LOGW("Initializing radios/vifs config (not found)");
        return;
    }

    ds_dlist_foreach(&init_cfg, radio) {
        /* Add device config to Wifi_Radio_Config */
        char *rconf_filter[] = { "-", SCHEMA_COLUMN(Wifi_Radio_Config, vif_configs), NULL };
        ret = ovsdb_table_upsert_f(&table_Wifi_Radio_Config, &radio->rconf, false, rconf_filter);
        if (true != ret) {
            continue;
        }

        LOGD("Added %s radio %s to config",
              radio->rconf.freq_band,
              radio->rconf.if_name);

        /* Add device config to Wifi_Radio_State */
        memset(&rstate, 0, sizeof(rstate));
        ret = target_radio_state_get(radio->rconf.if_name, &rstate);
        if (true != ret) {
            LOGE("Initializing %s radio %s (Failed to fetch state data)",
                 radio->rconf.freq_band,
                 radio->rconf.if_name);
            continue;
        }

        char *rstate_filter[] = { "-", SCHEMA_COLUMN(Wifi_Radio_State, vif_states), NULL };
        ret = ovsdb_table_upsert_f(&table_Wifi_Radio_State, &rstate, false, rstate_filter);
        if (true != ret) {
            LOGE("Initializing %s radio %s (Failed to insert state)",
                 rstate.freq_band,
                 rstate.if_name);
            return;
        }
        LOGD("Added %s radio %s to state",
              rstate.freq_band,
              rstate.if_name);

        ds_dlist_foreach(&radio->vifs_cfg, vif) {
            /* Add device vif config to Wifi_VIF_Config */
            ret = ovsdb_table_upsert_with_parent(&table_Wifi_VIF_Config,
                    &vif->vconf, false, NULL,
                    SCHEMA_TABLE(Wifi_Radio_Config),
                    ovsdb_where_simple(
                        SCHEMA_COLUMN(Wifi_Radio_Config, if_name), radio->rconf.if_name),
                    SCHEMA_COLUMN(Wifi_Radio_Config, vif_configs));
            if (true != ret) {
                continue;
            }

            LOGD("Added VIF %s to config",
                 vif->vconf.if_name);

            memset(&vstate, 0, sizeof(vstate));
            ret = target_vif_state_get(vif->vconf.if_name, &vstate);
            if (true != ret) {
                LOGE("Initializing VIF %s (Failed to fetch state data)", vif->vconf.if_name);
                return;
            }
            ret = ovsdb_table_upsert_with_parent(&table_Wifi_VIF_State,
                    &vstate, false, NULL,
                    SCHEMA_TABLE(Wifi_Radio_State),
                    ovsdb_where_simple(
                        SCHEMA_COLUMN(Wifi_Radio_State, if_name), rstate.if_name),
                    SCHEMA_COLUMN(Wifi_Radio_State, vif_states));
            if (true != ret) {
                continue;
            }

            LOGD("Added VIF %s to state", vstate.if_name);
        }
    }

    wm2_radio_init_clear(&init_cfg);

    return;
}

