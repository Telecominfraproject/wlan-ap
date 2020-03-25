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

#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "schema.h"
#include "log.h"
#include "qm_conn.h"
#include "dppline.h"
#include "network_metadata.h"
#include "fcm.h"
#include "fcm_priv.h"
#include "fcm_mgr.h"

/* Log entries from this file will contain "OVSDB" */
#define MODULE_ID LOG_MODULE_ID_OVSDB

ovsdb_table_t table_AWLAN_Node;
ovsdb_table_t table_FCM_Collector_Config;
ovsdb_table_t table_FCM_Report_Config;


/**
 * fcm_get_awlan_header_id: maps key to header_id
 * @key: key
 */
static fcm_header_ids fcm_get_awlan_header_id(const char *key)
{
    int val;

    val = strcmp(key, "locationId");
    if (val == 0) return FCM_HEADER_LOCATION_ID;

    val = strcmp(key, "nodeId");
    if (val == 0) return FCM_HEADER_NODE_ID;

    return FCM_NO_HEADER;
}

/**
 * fcm_get_awlan_headers: gather mqtt records from AWLAN_Node's
 * mqtt headers table
 * @awlan: AWLAN_Node record
 *
 * Parses the given record, looks up a matching FCM session, updates it if found
 * or allocates and inserts in sessions's tree if not.
 */
static void fcm_get_awlan_headers(struct schema_AWLAN_Node *awlan)
{
    fcm_mgr_t *mgr = fcm_get_mgr();
    int i = 0;

    LOGT("%s %d", __FUNCTION__,
         awlan ? awlan->mqtt_headers_len : 0);

    for (i = 0; i < awlan->mqtt_headers_len; i++)
    {
        char *key = awlan->mqtt_headers_keys[i];
        char *val = awlan->mqtt_headers[i];
        fcm_header_ids id = FCM_NO_HEADER;

        LOGT("mqtt_headers[%s]='%s'", key, val);

        id = fcm_get_awlan_header_id(key);
        if (id == FCM_NO_HEADER)
        {
            LOG(ERR, "%s: invalid mqtt_headers key", key);
            continue;
        }

        mgr->mqtt_headers[id] = calloc(1, sizeof(awlan->mqtt_headers[0]));
        if (mgr->mqtt_headers[id] == NULL)
        {
            LOGE("Could not allocate memory for mqtt header %s:%s",
                 key, val);
        }
        memcpy(mgr->mqtt_headers[id], val, sizeof(awlan->mqtt_headers[0]));
    }
}

void callback_AWLAN_Node(
        ovsdb_update_monitor_t *mon,
        struct schema_AWLAN_Node *old_rec,
        struct schema_AWLAN_Node *awlan)
{
    if (mon->mon_type != OVSDB_UPDATE_DEL)
    {
        fcm_get_awlan_headers(awlan);
    }
}



void callback_FCM_Collector_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_FCM_Collector_Config *old_rec,
        struct schema_FCM_Collector_Config *conf)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW)
    {
        if (init_collect_config(conf) == false)
        {
            LOGE("%s: FCM collector plugin init failed: name %s\n",
                __func__, conf->name);
            return;
        }
        LOGD("%s: FCM collector config entry added: name  %s\n",
             __func__, conf->name);
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        delete_collect_config(conf);
        LOGD("%s: FCM collector config entry deleted: name:  %s\n",
             __func__, conf->name);
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        update_collect_config(conf);
        LOGD("%s: FCM collector config entry updated: name: %s\n",
             __func__, conf->name);
    }
}

void callback_FCM_Report_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_FCM_Report_Config *old_rec,
        struct schema_FCM_Report_Config *conf)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW)
    {
        init_report_config(conf);
        LOGD("%s: FCM reporter config entry added: name: %s",
             __func__, conf->name);
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        delete_report_config(conf);
        LOGD("%s: FCM reporter config entry deleted: name: %s",
             __func__, conf->name);
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        update_report_config(conf);
        LOGD("%s: FCM reporter config entry updated: name: %s",
             __func__, conf->name);
    }
}

int fcm_ovsdb_init(void)
{
    LOGD("Initializing FCM tables");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT_NO_KEY(AWLAN_Node);
    OVSDB_TABLE_INIT_NO_KEY(FCM_Collector_Config);
    OVSDB_TABLE_INIT_NO_KEY(FCM_Report_Config);

    // Initialize OVSDB monitor callbacks
    OVSDB_TABLE_MONITOR(AWLAN_Node, false);
    OVSDB_TABLE_MONITOR(FCM_Collector_Config, false);
    OVSDB_TABLE_MONITOR(FCM_Report_Config, false);

    return 0;
}
