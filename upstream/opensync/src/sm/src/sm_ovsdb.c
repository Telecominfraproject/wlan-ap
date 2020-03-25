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
 * Stats interacts with ovsdb
 */

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
#include "log.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "schema.h"

#include "sm.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

/******************************************************************************
 *  PRIVATE API definitions
 *****************************************************************************/
char *sm_report_type_str[STS_REPORT_MAX] =
{
    "neighbor",
    "survey",
    "client",
    "capacity",
    "radio",
    "essid",
    "device",
    "rssi",
};

#ifndef USE_QM
static ovsdb_update_monitor_t sm_update_awlan_node;
static struct schema_AWLAN_Node awlan_node;
#endif

static ovsdb_update_monitor_t sm_update_wifi_radio_state;
static ds_tree_t sm_radio_list =
DS_TREE_INIT(
        (ds_key_cmp_t*)strcmp,
        sm_radio_state_t,
        node);

static ovsdb_update_monitor_t sm_update_wifi_vif_state;
static ds_tree_t sm_vif_list =
DS_TREE_INIT(
        (ds_key_cmp_t*)strcmp,
        sm_vif_state_t,
        node);

static ovsdb_update_monitor_t sm_update_wifi_stats_config;
static ds_tree_t stats_config_table =
DS_TREE_INIT(
        (ds_key_cmp_t*)strcmp,
        sm_stats_config_t,
        node);

/******************************************************************************
 *                          AWLAN CONFIG
 *****************************************************************************/
#ifndef USE_QM
// this is handled by QM if it's used
static
void sm_update_awlan_node_cbk(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t perr;

    LOG(DEBUG, "%s", __FUNCTION__);

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            if (!schema_AWLAN_Node_from_json(&awlan_node, self->mon_json_new, false, perr))
            {
                LOG(ERR, "Parsing AWLAN_Node NEW request: %s", perr);
                return;
            }
            break;

        case OVSDB_UPDATE_MODIFY:
            if (!schema_AWLAN_Node_from_json(&awlan_node, self->mon_json_new, true, perr))
            {
                LOG(ERR, "Parsing AWLAN_Node MODIFY request.");
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            /* Reset configuration */
            memset(&awlan_node, 0, sizeof(awlan_node));
            break;

        default:
            LOG(ERR, "Update Monitor for AWLAN_Node reported an error.");
            break;
    }

    /*
     * Apply MQTT settings
     */
    int          ii;
    const char  *mqtt_broker = NULL;
    const char  *mqtt_topic = NULL;
    const char  *mqtt_qos = NULL;
    const char  *mqtt_port = NULL;
    int         mqtt_compress = 0;

    for (ii = 0; ii < awlan_node.mqtt_settings_len; ii++)
    {
        const char *key = awlan_node.mqtt_settings_keys[ii];
        const char *val = awlan_node.mqtt_settings[ii];

        if (strcmp(key, "broker") == 0)
        {
            mqtt_broker = val;
        }
        else if (strcmp(key, "topics") == 0)
        {
            mqtt_topic = val;
        }
        else if (strcmp(key, "qos") == 0)
        {
            mqtt_qos = val;
        }
        else if (strcmp(key, "port") == 0)
        {
            mqtt_port = val;
        }
        else if (strcmp(key, "compress") == 0)
        {
            if (strcmp(val, "zlib") == 0) mqtt_compress = 1;
        }
        else
        {
            LOG(ERR, "Unkown MQTT option: %s", key);
        }
    }

    sm_mqtt_set(mqtt_broker, mqtt_port, mqtt_topic, mqtt_qos, mqtt_compress);

    return;
}
#endif // USE_QM

/******************************************************************************
 *                          STATS CONFIG
 *****************************************************************************/
static
bool sm_enumerate_stats_config(sm_stats_config_t *stats)
{
    int                             i;
    bool                            ret = true;
    struct schema_Wifi_Stats_Config *schema = &stats->schema;

#define STS_SURVEY_MAX 3
    char *sm_survey_type_str[STS_SURVEY_MAX] =
    {
        "on-chan",
        "off-chan",
        "full",
    };

    int scan_type_map[STS_SURVEY_MAX] = {
        RADIO_SCAN_TYPE_ONCHAN,
        RADIO_SCAN_TYPE_OFFCHAN,
        RADIO_SCAN_TYPE_FULL,
    };

    if (strcmp(schema->radio_type, RADIO_TYPE_STR_2G) == 0) {
        stats->radio_type = RADIO_TYPE_2G;
    }
    else if (strcmp(schema->radio_type, RADIO_TYPE_STR_5G) == 0) {
        stats->radio_type = RADIO_TYPE_5G;
    }
    else if (strcmp(schema->radio_type, RADIO_TYPE_STR_5GL) == 0) {
        stats->radio_type = RADIO_TYPE_5GL;
    }
    else if (strcmp(schema->radio_type, RADIO_TYPE_STR_5GU) == 0) {
        stats->radio_type = RADIO_TYPE_5GU;
    }
    else {
        stats->radio_type = RADIO_TYPE_NONE;
    }

    /* All reprots are raw by default */
    stats->report_type = REPORT_TYPE_RAW;
    if (strcmp(schema->report_type, "average") == 0) {
        stats->report_type = REPORT_TYPE_AVERAGE;
    }
    else if (strcmp(schema->report_type, "histogram") == 0) {
        stats->report_type = REPORT_TYPE_HISTOGRAM;
    }
    else if (strcmp(schema->report_type, "percentile") == 0) {
        stats->report_type = REPORT_TYPE_PERCENTILE;
    }
    else if (strcmp(schema->report_type, "diff") == 0) {
        stats->report_type = REPORT_TYPE_DIFF;
    }

    for (i=0; i < STS_REPORT_MAX; i++) {
        if (strcmp(schema->stats_type, sm_report_type_str[i]) == 0) break;
    }
    stats->sm_report_type = i;
    if (i == STS_REPORT_MAX) ret = false;

    if (schema->survey_type_exists) {
        for (i=0; i<STS_SURVEY_MAX; i++) {
            if (strcmp(schema->survey_type, sm_survey_type_str[i]) == 0) break;
        }
        if (i == STS_SURVEY_MAX) {
            stats->scan_type = RADIO_SCAN_TYPE_NONE;
            ret = false;
        }
        else {
            stats->scan_type = scan_type_map[i];
        }
    }
    else {
        stats->scan_type = RADIO_SCAN_TYPE_NONE;
    }

    return ret;
}

static
bool sm_update_stats_config(sm_stats_config_t *stats_cfg)
{
    sm_stats_request_t              req;
    int                             i;

    if (NULL == stats_cfg) {
        return false;
    }

    struct timespec                 ts;
    memset (&ts, 0, sizeof (ts));
    if(clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return false;


    /* Search for existing radio entry and use fallback */
    sm_radio_state_t               *radio = NULL;
    ds_tree_foreach(&sm_radio_list, radio) {
        if(radio->config.type == stats_cfg->radio_type) {
            break;
        }
    }

    /* Radio does not exist */
    if (NULL == radio) {
        LOGW("Skip configuring stats (%s radio not present!)",
             radio_get_name_from_type(stats_cfg->radio_type));
        return false;
    }

    /* Stats request */
    memset(&req, 0, sizeof(req));
    req.radio_type = stats_cfg->radio_type;
    req.report_type = stats_cfg->report_type;
    req.scan_type  = stats_cfg->scan_type;

    req.reporting_interval = stats_cfg->schema.reporting_interval;
    req.reporting_count = stats_cfg->schema.reporting_count;
    req.sampling_interval = stats_cfg->schema.sampling_interval;
    req.scan_interval = stats_cfg->schema.survey_interval_ms;
    for (i = 0; i < stats_cfg->schema.channel_list_len; i++)
    {
        req.radio_chan_list.chan_list[i] = stats_cfg->schema.channel_list[i];
    }
    req.radio_chan_list.chan_num = stats_cfg->schema.channel_list_len;

    req.threshold_util = 0;
    req.threshold_max_delay = 0;
    req.threshold_pod_qty = 0;
    req.threshold_pod_num = 0;
    for (i = 0; i < stats_cfg->schema.threshold_len; i++)
    {
        if (strcmp(stats_cfg->schema.threshold_keys[i], "util" ) == 0)
        {
            req.threshold_util = stats_cfg->schema.threshold[i];
        }
        if (strcmp(stats_cfg->schema.threshold_keys[i], "max_delay" ) == 0)
        {
            req.threshold_max_delay = stats_cfg->schema.threshold[i];
        }
        if (strcmp(stats_cfg->schema.threshold_keys[i], "pod_qty" ) == 0)
        {
            req.threshold_pod_qty = stats_cfg->schema.threshold[i];
        }
        if (strcmp(stats_cfg->schema.threshold_keys[i], "pod_num" ) == 0)
        {
            req.threshold_pod_num = stats_cfg->schema.threshold[i];
        }
        if (strcmp(stats_cfg->schema.threshold_keys[i], "mac_filter" ) == 0)
        {
            req.mac_filter = stats_cfg->schema.threshold[i];
        }
    }

    req.reporting_timestamp = timespec_to_timestamp(&ts);

    switch(stats_cfg->sm_report_type)
    {
        case STS_REPORT_NEIGHBOR:
            sm_neighbor_report_request(&radio->config, &req);
            break;
        case STS_REPORT_CLIENT:
            sm_client_report_request(&radio->config, &req);
            break;
        case STS_REPORT_SURVEY:
            sm_survey_report_request(&radio->config, &req);
            break;
        case STS_REPORT_DEVICE:
            sm_device_report_request(&req);
            break;
        case STS_REPORT_CAPACITY:
#ifdef USE_CAPACITY_QUEUE_STATS
            sm_capacity_report_request(&radio->config, &req);
#else
            LOGW("Skip configuring capacity stats (stats not supported!)");
#endif
            break;
        case STS_REPORT_RSSI:
            sm_rssi_report_request(&radio->config, &req);
            break;
        default:
            return false;
    }

    return true;
}

static
void sm_update_wifi_stats_config_cb(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t                    perr;
    sm_stats_config_t              *stats;
    bool                            ret;

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            stats = calloc(1, sizeof(sm_stats_config_t));
            if (NULL == stats) {
                LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: Failed to allocate memory");
                return;
            }

            ret = schema_Wifi_Stats_Config_from_json(&stats->schema, self->mon_json_new, false, perr);
            if (ret) ret = sm_enumerate_stats_config(stats);
            if (!ret) {
                free(stats);
                LOG(ERR, "Parsing Wifi_Stats_Config NEW request: %s", perr);
                return;
            }
            ds_tree_insert(&stats_config_table, stats, stats->schema._uuid.uuid);
            break;

        case OVSDB_UPDATE_MODIFY:
            stats = ds_tree_find(&stats_config_table, (char*)self->mon_uuid);
            if (!stats)
            {
                LOG(ERR, "Unexpected MODIFY %s", self->mon_uuid);
                return;
            }
            ret = schema_Wifi_Stats_Config_from_json(&stats->schema, self->mon_json_new, true, perr);
            if (ret) ret = sm_enumerate_stats_config(stats);
            if (!ret)
            {
                LOG(ERR, "Parsing Wifi_Stats_Config MODIFY request.");
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            stats = ds_tree_find(&stats_config_table, (char*)self->mon_uuid);
            if (!stats)
            {
                LOG(ERR, "Unexpected DELETE %s", self->mon_uuid);
                return;
            }
            /* Reset configuration */
            stats->schema.reporting_interval = 0;
            stats->schema.reporting_count = 0;
            sm_update_stats_config(stats);
            ds_tree_remove(&stats_config_table, stats);
            free(stats);
            return;

        default:
            LOG(ERR, "Update Monitor for Wifi_Stats_Config reported an error. %s", self->mon_uuid);
            return;
    }

    sm_update_stats_config(stats);

    return;
}

/******************************************************************************
 *                          RADIO CONFIG
 *****************************************************************************/
static
bool sm_is_radio_config_changed (
        radio_entry_t              *old_cfg,
        radio_entry_t              *new_cfg)
{
    if (old_cfg->chan != new_cfg->chan)
    {
        LOG(DEBUG,
                "Radio Config: %s chan changed %d != %d",
                radio_get_name_from_cfg(new_cfg),
                old_cfg->chan,
                new_cfg->chan);
        return true;
    }

    if (strcmp(old_cfg->phy_name, new_cfg->phy_name))
    {
        LOG(DEBUG,
                "Radio Config: %s phy_name changed %s != %s",
                radio_get_name_from_cfg(new_cfg),
                old_cfg->phy_name,
                new_cfg->phy_name);
        return true;
    }

    if (strcmp(old_cfg->if_name, new_cfg->if_name))
    {
        LOG(DEBUG,
                "Radio Config: %s if_name changed %s != %s",
                radio_get_name_from_cfg(new_cfg),
                old_cfg->if_name,
                new_cfg->if_name);
        return true;
    }

    return false;
}

static
void sm_radio_cfg_update(void)
{
    sm_radio_state_t               *radio;
    radio_entry_t                   radio_cfg;

    /* SM requires both radio and VIF information to
       access driver sublayer therefore join the schema
       to radio config entry type and use it for stats
       fetching
     */
    ds_tree_foreach(&sm_radio_list, radio)
    {
        memset(&radio_cfg, 0, sizeof(radio_cfg));

        /* For easy handling (type and an index) the internal config
           is always stored as 2.4 and 5G */
        if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_2G) == 0) {
            radio_cfg.type = RADIO_TYPE_2G;
        }
        else if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_5G) == 0) {
            radio_cfg.type = RADIO_TYPE_5G;
        }
        else if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_5GL) == 0) {
            radio_cfg.type = RADIO_TYPE_5GL;
        }
        else if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_5GU) == 0) {
            radio_cfg.type = RADIO_TYPE_5GU;
        }
        else {
            LOG(ERR,
                "Radio Config: Unknown radio frequency band: %s",
                radio->schema.freq_band);
            radio_cfg.type = RADIO_TYPE_NONE;
            return;
        }

        /* Admin mode */
        radio_cfg.admin_status =
            radio->schema.enabled ? RADIO_STATUS_ENABLED : RADIO_STATUS_DISABLED;

        /* Assign operating channel */
        radio_cfg.chan = radio->schema.channel;

        /* Radio physical name */
        STRSCPY(radio_cfg.phy_name, radio->schema.if_name);

        /* Country code */
        STRSCPY(radio_cfg.cntry_code, radio->schema.country);

        /* Channel width and HT mode */
        if (strcmp(radio->schema.ht_mode, "HT20") == 0) {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_20MHZ;
        }
        else if (strcmp(radio->schema.ht_mode, "HT2040") == 0) {
            LOG(DEBUG,
                "Radio Config: No direct mapping for HT2040");
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_NONE;
        }
        else if (strcmp(radio->schema.ht_mode, "HT40") == 0) {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_40MHZ;
        }
        else if (strcmp(radio->schema.ht_mode, "HT40+") == 0) {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_40MHZ_ABOVE;
        }
        else if (strcmp(radio->schema.ht_mode, "HT40-") == 0) {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_40MHZ_BELOW;
        }
        else if (strcmp(radio->schema.ht_mode, "HT80") == 0) {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_80MHZ;
        }
        else if (strcmp(radio->schema.ht_mode, "HT160") == 0) {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_160MHZ;
        }

        else if (strcmp(radio->schema.ht_mode, "HT80+80") == 0) {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_80_PLUS_80MHZ;
        }
        else {
            radio_cfg.chanwidth = RADIO_CHAN_WIDTH_NONE;
            LOG(DEBUG,
                "Radio Config: Unknown radio HT mode: %s",
                radio->schema.ht_mode);
        }

        if (strcmp(radio->schema.hw_mode, "11a") == 0) {
            radio_cfg.protocol = RADIO_802_11_A;
        }
        else if (strcmp(radio->schema.hw_mode, "11b") == 0) {
            radio_cfg.protocol = RADIO_802_11_BG;
        }
        else if (strcmp(radio->schema.hw_mode, "11g") == 0) {
            radio_cfg.protocol = RADIO_802_11_BG;
        }
        else if (strcmp(radio->schema.hw_mode, "11n") == 0) {
            radio_cfg.protocol = RADIO_802_11_NG;
        }
        else if (strcmp(radio->schema.hw_mode, "11ab") == 0) {
            radio_cfg.protocol = RADIO_802_11_A;
        }
        else if (strcmp(radio->schema.hw_mode, "11ac") == 0) {
            radio_cfg.protocol = RADIO_802_11_AC;
        }
        else {
            LOG(DEBUG,
                "Radio Config: Unkown protocol: %s",
                radio->schema.hw_mode);
            radio_cfg.protocol = RADIO_802_11_AUTO;
        }

        /* Interface name - just fetch the first interface from the
           interface list. vif_configs is the array of VIF uuids
           linked to this radio interface.
         */
        if (radio->schema.vif_states_len > 0) {
            int ii;
            sm_vif_state_t *vif = NULL;

            /* Some platforms disable tx stats when
             * last vap is deleted.
             * Ideally this should be called per-radio
             * whenever first vap is created (on given
             * radio) but that's not easy because
             * there's no reliable way of getting
             * this out of OVSDB directly because
             * OVSDB represents states and not
             * state transitions. WM interprets
             * different states into transitions.
             *
             * This results in a bit excessive
             * driver config tweaking.
             */
            sm_radio_config_enable_tx_stats(&radio_cfg);

            /* Lookup the first interface */
            for (ii = 0; ii < radio->schema.vif_states_len; ii++)
            {
                vif =
                    ds_tree_find(
                            &sm_vif_list,
                            radio->schema.vif_states[ii].uuid);
                if (vif == NULL) {
                    continue;
                }

                /* skip STA interfaces because of 5G scan issue */
                if (strcmp(vif->schema.mode, "sta") == 0) {
                    continue;
                }

                /* skip disabled interfaces */
                if (vif->schema.enabled == 0) {
                    continue;
                }

                /* Radio VIF/VAP interface name */
                STRSCPY(radio_cfg.if_name, vif->schema.if_name);

                /* Enable fast scanning on all ap interfaces */
                sm_radio_config_enable_fast_scan(&radio_cfg);
                break;
            }
        }
        else {
            LOG(WARNING,
                "Radio Config: No interfaces associated with %s radio.",
                radio_get_name_from_cfg(&radio_cfg));
        }

        bool is_changed =
            sm_is_radio_config_changed(
                    &radio->config,
                    &radio_cfg);

        /* Update cache config */
        radio->config = radio_cfg;

        /* Restart stats with new radio parameters when
           type, chan, if_name and phy_name are configured or
           change on those parameters is detected
         */
        if(    is_changed
            && radio->config.type
            && (radio->config.chan != 0)
            && (radio->config.if_name[0] != '\0')
            && (radio->config.phy_name[0] != '\0')) {
            sm_neighbor_report_radio_change(&radio->config);
            sm_survey_report_radio_change(&radio->config);
            sm_client_report_radio_change(&radio->config);
            sm_rssi_report_radio_change(&radio->config);

#ifdef USE_CAPACITY_QUEUE_STATS
            sm_capacity_report_radio_change(&radio->config);
#endif
        }

    }
}

static void sm_update_wifi_radio_state_cb(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t                    perr;
    sm_radio_state_t               *radio = NULL;

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /*
             * New row update notification -- create new row, parse it and insert it into the table
             */
            radio = calloc(1, sizeof(sm_radio_state_t));
            if (NULL == radio) {
                LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: Failed to allocate memory");
                return;
            }

            if (!schema_Wifi_Radio_State_from_json(&radio->schema, self->mon_json_new, false, perr))
            {
                LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: %s", perr);
                free(radio);
                return;
            }

            /* The Radio state table is indexed by UUID */
            ds_tree_insert(&sm_radio_list, radio, radio->schema._uuid.uuid);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Find the row by UUID */
            radio = ds_tree_find(&sm_radio_list, (void *)self->mon_uuid);
            if (!radio) {
                LOG(ERR, "MODIFY: Radio State: Update request for non-existent radio UUID: %s", self->mon_uuid);
                return;
            }

            /* Update the row */
            if (!schema_Wifi_Radio_State_from_json(&radio->schema, self->mon_json_new, true, perr))
            {
                LOG(ERR, "MODIFY: Radio State: Parsing Wifi_Radio_State: %s", perr);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            radio = ds_tree_find(&sm_radio_list, (void *)self->mon_uuid);
            if (!radio)
            {
                LOG(ERR, "DELETE: Radio State: Delete request for non-existent radio UUID: %s", self->mon_uuid);
                return;
            }

            ds_tree_remove(&sm_radio_list, radio);
            free(radio);
            return;

        default:
            LOG(ERR, "Radio State: Unknown update notification type %d for UUID %s.", self->mon_type, self->mon_uuid);
            return;
    }

    /* Update the global radio configuration */
    sm_radio_cfg_update();
}

/******************************************************************************
 *                          VIF CONFIG
 *****************************************************************************/
static
void sm_update_wifi_vif_state_cb(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t                    perr;
    sm_vif_state_t                 *vif = NULL;

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /*
             * New row update notification -- create new row, parse it and insert it into the table
             */
            vif = calloc(1, sizeof(sm_vif_state_t));
            if (NULL == vif) {
                LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: Failed to allocate memory");
                return;
            }

            if (!schema_Wifi_VIF_State_from_json(&vif->schema, self->mon_json_new, false, perr))
            {
                LOG(ERR, "NEW: VIF Config: Parsing Wifi_Radio_Config: %s", perr);
                return;
            }

            /* The Radio config table is indexed by UUID */
            ds_tree_insert(&sm_vif_list, vif, vif->schema._uuid.uuid);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Find the row by UUID */
            vif = ds_tree_find(&sm_vif_list, (void *)self->mon_uuid);
            if (!vif)
            {
                LOG(ERR, "MODIFY: VIF Config: Update request for non-existent vif UUID: %s", self->mon_uuid);
                return;
            }

            /* Update the row */
            if (!schema_Wifi_VIF_State_from_json(&vif->schema, self->mon_json_new, true, perr))
            {
                LOG(ERR, "MODIFY: VIF Config: Parsing Wifi_Radio_Config: %s", perr);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            vif = ds_tree_find(&sm_vif_list, (void *)self->mon_uuid);
            if (!vif)
            {
                LOG(ERR, "DELETE: VIF Config: Delete request for non-existent stats- UUID: %s", self->mon_uuid);
                return;
            }

            ds_tree_remove(&sm_vif_list, vif);

            free(vif);

            return;

        default:
            LOG(ERR, "VIF Config: Unknown update notification type %d for UUID %s.", self->mon_type, self->mon_uuid);
            return;
    }

    /* Update the global radio configuration */
    sm_radio_cfg_update();
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
int sm_setup_monitor(void)
{
#ifndef USE_QM
    /* Monitor AWLAN_Node */
    if (!ovsdb_update_monitor(
            &sm_update_awlan_node,
            sm_update_awlan_node_cbk,
            SCHEMA_TABLE(AWLAN_Node),
            OMT_ALL))
    {
        LOGE("Error registering watcher for %s.",
                SCHEMA_TABLE(AWLAN_Node));
        return -1;
    }
#endif // USE_QM

    /* Monitor Wifi_Radio_State */
    if (!ovsdb_update_monitor(
            &sm_update_wifi_radio_state,
            sm_update_wifi_radio_state_cb,
            SCHEMA_TABLE(Wifi_Radio_State),
            OMT_ALL))
    {
        LOGE("Error registering watcher for %s.",
                SCHEMA_TABLE(Wifi_Radio_State));
        return -1;
    }

    /* Monitor Wifi_VIF_State  */
    if (!ovsdb_update_monitor(
            &sm_update_wifi_vif_state,
            sm_update_wifi_vif_state_cb,
            SCHEMA_TABLE(Wifi_VIF_State),
            OMT_ALL))
    {
        LOGE("Error registering watcher for %s.",
                SCHEMA_TABLE(Wifi_VIF_State));
        return -1;
    }

    /* Monitor Wifi_Stats_Config */
    if (!ovsdb_update_monitor(
            &sm_update_wifi_stats_config,
            sm_update_wifi_stats_config_cb,
            SCHEMA_TABLE(Wifi_Stats_Config),
            OMT_ALL))
    {
        LOGE("Error registering watcher for %s.",
                SCHEMA_TABLE(Wifi_Stats_Config));
        return -1;
    }

    return 0;
}

int sm_cancel_monitor(void)
{
    return 0;
}

ds_tree_t *sm_radios_get() {
    return &sm_radio_list;
}

ds_tree_t *sm_vifs_get() {
    return &sm_vif_list;
}

void sm_vif_whitelist_get(char **mac_list, uint16_t *mac_size, uint16_t *mac_qty)
{
    sm_vif_state_t *vif = NULL;
    ds_tree_iter_t  iter;

    for (   vif = ds_tree_ifirst(&iter, &sm_vif_list);
            vif != NULL;
            vif = ds_tree_inext(&iter)) {
        if (strstr(vif->schema.if_name, SCHEMA_CONSTS_IF_NAME_PREFIX_BHAUL)) {
            *mac_list = (char *)&vif->schema.mac_list[0];
            *mac_size = sizeof(vif->schema.mac_list[0]);
            *mac_qty = vif->schema.mac_list_len;
            break;
        }
    }
}

