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
#include "schema_consts.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "connector.h"
#include "xm.h"

/*****************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN

/*****************************************************************************/

ovsdb_table_t table_AWLAN_Node;
ovsdb_table_t table_Wifi_Radio_Config;
ovsdb_table_t table_Wifi_VIF_Config;
ovsdb_table_t table_Wifi_Inet_Config;

static bool g_xm_init = false;

#define INIT_DONE() if (!g_xm_init) { LOGT("%s: init not done yet", __func__); return; }

#define REQUIRE(ctx, cond) if (!(cond)) { LOGW("%s: %s: failed check: %s", ctx, __func__, #cond); return false; }


/******************************************************************************
 *  EXTERNAL UPDATES
 *****************************************************************************/

static bool xm_device_info_update(const connector_device_info_t *info)
{
    struct schema_AWLAN_Node node;
    MEMZERO(node);

    // Change device_mode or redirector_addr
    LOGD("External: Updating device info");
    node._partial_update = true;
    REQUIRE(info->serial_number, strlen(info->serial_number) > 0);
    REQUIRE(info->platform_version, strlen(info->platform_version) > 0);
    REQUIRE(info->model, strlen(info->model) > 0);
    REQUIRE(info->revision, strlen(info->revision) > 0);

    SCHEMA_SET_STR(node.serial_number, info->serial_number);
    SCHEMA_SET_STR(node.platform_version, info->platform_version);
    SCHEMA_SET_STR(node.model, info->model);
    SCHEMA_SET_STR(node.revision, info->revision);

    if (ovsdb_table_upsert_f(&table_AWLAN_Node, &node, false, NULL))
    {
        LOGI("External: Updated device info");
    }
    else
    {
        LOGE("External: Updated device info");
        return false;
    }
    return true;
}

static bool xm_mode_update(const connector_device_mode_e mode)
{
    struct schema_AWLAN_Node node;
    MEMZERO(node);

    // Change device_mode or redirector_addr
    LOGD("External: Updating device mode");

    node._partial_update = true;
    switch (mode) {
        case MONITOR_MODE:
            SCHEMA_SET_STR(node.device_mode, SCHEMA_CONSTS_DEVICE_MODE_MONITOR);
            break;
        case CLOUD_MODE:
            SCHEMA_SET_STR(node.device_mode, SCHEMA_CONSTS_DEVICE_MODE_CLOUD);
            break;
        case BATTERY_MODE:
            SCHEMA_SET_STR(node.device_mode, SCHEMA_CONSTS_DEVICE_MODE_BATTERY);
            break;
        default:
            LOGE("External: Updating device mode (unsupported %d)", mode);
            return false;
    }

    if (ovsdb_table_upsert_f(&table_AWLAN_Node, &node, false, NULL))
    {
        LOGI("External: Updated device mode");
    }
    else
    {
        LOGE("External: Updated device mode");
        return false;
    }

    return true;
}

static bool xm_address_update(const char *cloud_address)
{
    struct schema_AWLAN_Node node;
    MEMZERO(node);

    LOGD("External: Updating cloud address '%s'", cloud_address);

    REQUIRE(cloud_address, strlen(cloud_address) > 0);
    node._partial_update = true;
    SCHEMA_SET_STR(node.redirector_addr, cloud_address);
    if (ovsdb_table_upsert_f(&table_AWLAN_Node, &node, false, NULL))
    {
        LOGI("External: Updated cloud address");
    }
    else
    {
        LOGE("External: Updated cloud address");
        return false;
    }
    return true;
}

static bool xm_radio_update(const struct schema_Wifi_Radio_Config *rconf)
{
    struct schema_Wifi_Radio_Config tmp;

    memcpy(&tmp, rconf, sizeof(tmp));
    LOGI("External: Updating radio %s (partial %d)", tmp.if_name, rconf->_partial_update);
    REQUIRE(rconf->if_name, strlen(rconf->if_name) > 0);
    tmp.vif_configs_present = false;
    REQUIRE(rconf->if_name, 1 == ovsdb_table_upsert_f(&table_Wifi_Radio_Config, &tmp, false, NULL));
    LOGI("External: Update radio %s", tmp.if_name);

    return true;
}

static bool xm_vif_update(const struct schema_Wifi_VIF_Config *vconf,
              const char *radio_ifname)
{
    struct schema_Wifi_VIF_Config tmp;
    json_t *where;

    memcpy(&tmp, vconf, sizeof(tmp));
    LOGI("External: Updating vif %s @ %s (partial %d)", vconf->if_name, radio_ifname, vconf->_partial_update);
    REQUIRE(vconf->if_name, strlen(vconf->if_name) > 0);
    if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Radio_Config, if_name), radio_ifname))){
        LOGI("External: Updating vif %s @ %s (no radio found)", vconf->if_name, radio_ifname);
        return false;
    }
    REQUIRE(vconf->if_name,
            ovsdb_table_upsert_with_parent(
                    &table_Wifi_VIF_Config,
                    &tmp,
                    false,
                    NULL,
                    // parent:
                    SCHEMA_TABLE(Wifi_Radio_Config),
                    where,
                    SCHEMA_COLUMN(Wifi_Radio_Config, vif_configs))
    );
    LOGI("External: Update vif %s @ %s", vconf->if_name, radio_ifname);

    return true;
}

static bool xm_inet_update(const struct schema_Wifi_Inet_Config *iconf)
{
    struct schema_Wifi_Inet_Config tmp;

    memcpy(&tmp, iconf, sizeof(tmp));
    LOGI("External: Updating inet %s (partial %d)", tmp.if_name, iconf->_partial_update);
    REQUIRE(iconf->if_name, strlen(iconf->if_name) > 0);
    REQUIRE(iconf->if_name, 1 == ovsdb_table_upsert_f(&table_Wifi_Inet_Config, &tmp, false, NULL));
    LOGI("External: Update inet %s", tmp.if_name);

    return true;
}


/******************************************************************************
 *  OVSDB UPDATES
 *****************************************************************************/

static void callback_AWLAN_Node(
        ovsdb_update_monitor_t    *mon,
        struct schema_AWLAN_Node  *old_rec,
        struct schema_AWLAN_Node  *awlan)
{
    connector_device_mode_e mode;

    INIT_DONE()

    switch (mon->mon_type) {
        case OVSDB_UPDATE_ERROR:
        case OVSDB_UPDATE_DEL:
            LOGW("OVSDB: device node error: %d", mon->mon_type);
            return;
        default:
            break;
    }

    if (awlan->device_mode_changed)
    {
        LOGD("OVSDB: Exchange device node %s", awlan->device_mode);
        if (!strcmp(SCHEMA_CONSTS_DEVICE_MODE_MONITOR, awlan->device_mode))
        {
            mode = MONITOR_MODE;
        }
        else if (!strcmp(SCHEMA_CONSTS_DEVICE_MODE_CLOUD, awlan->device_mode))
        {
            mode = CLOUD_MODE;
        }
        else
        {
            LOGD("OVSDB: Exchange device node %s (no action)", awlan->device_mode);
            return;
        }
        connector_sync_mode(mode);
    }
}

static void callback_Wifi_Radio_Config(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_Radio_Config *old_rec,
        struct schema_Wifi_Radio_Config *rconf,
        ovsdb_cache_row_t               *row)
{
    INIT_DONE()
    LOGD("OVSDB: Exchange radio %s", rconf->if_name);

    switch (mon->mon_type) {
        case OVSDB_UPDATE_ERROR:
            LOGW("OVSDB: Exchange radio %s error: %d", rconf->if_name, mon->mon_type);
            return;
        case OVSDB_UPDATE_DEL:
            /* The record contains the data that was deleted,
               therefore we set this radio to disabled */
            SCHEMA_SET_INT(rconf->enabled, false);
            break;
        default:
            break;
    }
    connector_sync_radio(rconf);
}

static void callback_Wifi_VIF_Config(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_VIF_Config   *old_rec,
        struct schema_Wifi_VIF_Config   *vconf,
        ovsdb_cache_row_t               *row)
{
    INIT_DONE()
    LOGD("OVSDB: Exchange vif %s", vconf->if_name);

    switch (mon->mon_type) {
        case OVSDB_UPDATE_ERROR:
            LOGW("OVSDB: Exchange vif %s error: %d", vconf->if_name, mon->mon_type);
            return;
        case OVSDB_UPDATE_DEL:
            /* The record contains the data that was deleted,
               therefore we set this interface to disabled */
            SCHEMA_SET_INT(vconf->enabled, false);
            break;
        default:
            break;
    }
    connector_sync_vif(vconf);
}

static void callback_Wifi_Inet_Config(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_Inet_Config  *old_rec,
        struct schema_Wifi_Inet_Config  *iconf,
        ovsdb_cache_row_t *row)
{
    INIT_DONE()
    LOGD("OVSDB: Exchange inet %s", iconf->if_name);

    switch (mon->mon_type) {
        case OVSDB_UPDATE_ERROR:
            LOGW("OVSDB: Exchange inet %s error: %d", iconf->if_name, mon->mon_type);
            return;
        case OVSDB_UPDATE_DEL:
            /* The record contains the data that was deleted,
               therefore we set this entry to disabled */
            SCHEMA_SET_INT(iconf->enabled, false);
            break;
        default:
            break;
    }
    connector_sync_inet(iconf);
}


/******************************************************************************
 *  Callback Table
 *****************************************************************************/

static const struct connector_ovsdb_api g_xm_api =
{
    .connector_device_info_cb = xm_device_info_update,
    .connector_device_mode_cb = xm_mode_update,
    .connector_cloud_address_cb = xm_address_update,
    .connector_radio_update_cb = xm_radio_update,
    .connector_vif_update_cb = xm_vif_update,
    .connector_inet_update_cb = xm_inet_update
};


/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/

bool xm_ovsdb_init(struct ev_loop *loop)
{
    LOGD("Initializing external database");

    if (g_xm_init) {
        LOGW("Initializing external database (already done)");
        return true;
    }

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT_NO_KEY(AWLAN_Node);
    OVSDB_TABLE_INIT(Wifi_Radio_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_VIF_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_Inet_Config, if_name);

    if (!connector_init(loop, &g_xm_api))
    {
        LOGE("Initializing external database (failed)");
        return false;
    }

    // Initialize OVSDB monitor callbacks
    OVSDB_TABLE_MONITOR(AWLAN_Node, false);
    OVSDB_CACHE_MONITOR(Wifi_Radio_Config, true);
    OVSDB_CACHE_MONITOR(Wifi_VIF_Config, true);
    OVSDB_CACHE_MONITOR(Wifi_Inet_Config, true);

    g_xm_init = true;

    return true;
}

bool xm_ovsdb_close(struct ev_loop *loop)
{
    LOGD("Closing external database");

    connector_close(loop);
    g_xm_init = false;

    return true;
}
