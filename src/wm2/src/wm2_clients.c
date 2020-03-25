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

#include <stdlib.h>
#include <sys/wait.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <inttypes.h>
#include <jansson.h>
#include <ctype.h>

#include "json_util.h"
#include "ds_list.h"
#include "schema.h"
#include "log.h"
#include "wm2.h"
#include "ovsdb.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "target.h"

// Defines
#define MODULE_ID LOG_MODULE_ID_MAIN

// OVSDB constants
#define OVSDB_CLIENTS_TABLE                 "Wifi_Associated_Clients"
#define OVSDB_CLIENTS_PARENT                "Wifi_VIF_State"
#define OVSDB_OPENFLOW_TAG_TABLE            "Openflow_Tag"

#define OVSDB_CLIENTS_PARENT_COL            "associated_clients"

/******************************************************************************
 *  PRIVATE definitions
 *****************************************************************************/
static int
wm2_clients_oftag_from_key_id(const char *cloud_vif_ifname,
                              const char *key_id,
                              char *oftag,
                              int len)
{
    struct schema_Wifi_VIF_Config vconf;
    ovsdb_table_t table_Wifi_VIF_Config;
    char oftagkey[32];
    char *ptr;
    bool ok;

    OVSDB_TABLE_INIT(Wifi_VIF_Config, if_name);
    ok = ovsdb_table_select_one(&table_Wifi_VIF_Config,
                                SCHEMA_COLUMN(Wifi_VIF_Config, if_name),
                                cloud_vif_ifname,
                                &vconf);
    if (!ok) {
        LOGW("%s: failed to lookup in table", cloud_vif_ifname);
        return -1;
    }

    if (strlen(SCHEMA_KEY_VAL(vconf.security, "oftag")) == 0) {
        LOGD("%s: no main oftag found, assuming backhaul/non-home interface, ignoring",
             cloud_vif_ifname);
        return 0;
    }

    if (strstr(key_id, "key-") == key_id)
        snprintf(oftagkey, sizeof(oftagkey), "oftag-%s", key_id);
    else
        snprintf(oftagkey, sizeof(oftagkey), "oftag");

    ptr = SCHEMA_KEY_VAL(vconf.security, oftagkey);
    if (!ptr || strlen(ptr) == 0)
        return -1;

    snprintf(oftag, len, "%s", ptr);
    return 0;
}

static int
wm2_clients_oftag_set(const char *mac,
                      const char *oftag)
{
    json_t *result;
    json_t *where;
    json_t *rows;
    json_t *row;
    int cnt;

    if (strlen(oftag) == 0) {
        LOGD("%s: oftag is empty, expect openflow/firewall issues", mac);
        return 0;
    }

    LOGD("%s: setting oftag='%s'", mac, oftag);

    where = ovsdb_where_simple("name", oftag);
    if (!where) {
        LOGW("%s: failed to allocate ovsdb condition, oom?", mac);
        return -1;
    }

    row = ovsdb_mutation("device_value",
                         json_string("insert"),
                         json_string(mac));
    if (!row) {
        LOGW("%s: failed to allocate ovsdb mutation, oom?", mac);
        json_decref(where);
        return -1;
    }

    rows = json_array();
    if (!rows) {
        LOGW("%s: failed to allocate ovsdb mutation list, oom?", mac);
        json_decref(where);
        json_decref(row);
        return -1;
    }

    json_array_append_new(rows, row);

    result = ovsdb_tran_call_s(OVSDB_OPENFLOW_TAG_TABLE,
                               OTR_MUTATE,
                               where,
                               rows);
    if (!result) {
        LOGW("%s: failed to execute ovsdb transact", mac);
        return -1;
    }

    cnt = ovsdb_get_update_result_count(result,
                                        OVSDB_OPENFLOW_TAG_TABLE,
                                        "mutate");

    return cnt;
}

static int
wm2_clients_oftag_unset(const char *mac,
                        const char *oftag)
{
    json_t *result;
    json_t *where;
    json_t *rows;
    json_t *row;
    int cnt;

    LOGD("%s: removing oftag", mac);

    where = ovsdb_tran_cond(OCLM_STR,
                            SCHEMA_COLUMN(Openflow_Tag, name),
                            OFUNC_EQ,
                            oftag);
    if (!where) {
        LOGW("%s: failed to allocate ovsdb condition, oom?", mac);
        return -1;
    }

    row = ovsdb_mutation("device_value",
                         json_string("delete"),
                         json_string(mac));
    if (!row) {
        LOGW("%s: failed to allocate ovsdb mutation, oom?", mac);
        json_decref(where);
        return -1;
    }

    rows = json_array();
    if (!rows) {
        LOGW("%s: failed to allocate ovsdb mutation list, oom?", mac);
        json_decref(where);
        json_decref(row);
        return -1;
    }

    json_array_append_new(rows, row);

    result = ovsdb_tran_call_s(OVSDB_OPENFLOW_TAG_TABLE,
                               OTR_MUTATE,
                               where,
                               rows);
    if (!result) {
        LOGW("%s: failed to execute ovsdb transact", mac);
        return -1;
    }

    cnt = ovsdb_get_update_result_count(result,
                                        OVSDB_OPENFLOW_TAG_TABLE,
                                        "mutate");

    return cnt;
}

static void
wm2_clients_isolate(const char *ifname, const char *sta, bool connected)
{
    struct schema_Wifi_VIF_Config vconf;
    const char *p;
    char sta_ifname[16];
    char path[256];
    char cmd[1024];
    int err;
    bool ok;

    snprintf(path, sizeof(path), "/.devmode.softwds.%s", ifname);
    if (access(path, R_OK))
        return;

    snprintf(path, sizeof(path), "/sys/module/softwds");
    if (access(path, R_OK)) {
        LOGW("%s: %s: isolate: softwds is missing", ifname, sta);
        return;
    }

    memset(sta_ifname, 0, sizeof(sta_ifname));
    strcpy(sta_ifname, "sta");
    for (p = sta; *p && strlen(sta_ifname) < sizeof(sta_ifname); p++)
        if (*p != ':')
            sta_ifname[strlen(sta_ifname)] = tolower(*p);

    if (strlen(sta_ifname) != (3 + 12)) {
        LOGW("%s: %s: isolate: failed to derive interface name: '%s'",
             ifname, sta, sta_ifname);
        return;
    }

    if (connected) {
        ok = ovsdb_table_select_one(&table_Wifi_VIF_Config,
                                    SCHEMA_COLUMN(Wifi_VIF_Config, if_name),
                                    ifname,
                                    &vconf);
        if (!ok) {
            LOGW("%s: %s: isolate: failed to get vconf", ifname, sta);
            return;
        }

        if (!vconf.bridge_exists || !strlen(vconf.bridge)) {
            LOGW("%s: %s: isolate: no bridge", ifname, sta);
            return;
        }

        snprintf(cmd, sizeof(cmd), "ovs-vsctl del-port %s ;"
                                   "ip link add link %s name %s type softwds &&"
                                   "echo %s > /sys/class/net/%s/softwds/addr &&"
                                   "echo N > /sys/class/net/%s/softwds/wrap &&"
                                   "ovs-vsctl add-port %s %s &&"
                                   "ip link set %s up",
                                   ifname,
                                   ifname, sta_ifname,
                                   sta, sta_ifname,
                                   sta_ifname,
                                   vconf.bridge, sta_ifname,
                                   sta_ifname);
        err = system(cmd);
        LOGI("%s: %s: isolating into '%s': %d (errno: %d)", ifname, sta, sta_ifname, err, errno);
    } else {
        snprintf(cmd, sizeof(cmd), "ovs-vsctl del-port %s ;"
                                   "ip link del %s",
                                   sta_ifname,
                                   sta_ifname);
        err = system(cmd);
        LOGI("%s: %s: cleaning up isolation of '%s': %d (errno: %d)", ifname, sta, sta_ifname, err, errno);
    }
}

static int
wm2_clients_get_refcount(const char *uuid)
{
    const char *column;
    json_t *where;
    int count;

    column = SCHEMA_COLUMN(Wifi_VIF_State, associated_clients),
    where = ovsdb_tran_cond(OCLM_UUID, column, OFUNC_INC, uuid);
    free(ovsdb_table_select_where(&table_Wifi_VIF_State, where, &count));

    return count;
}

static void
wm2_clients_war_esw_2684_noc_163_plat_878(const char *addr, const char *key_id)
{
    const char *table = SCHEMA_TABLE(Wifi_Associated_Clients);
    const char *state = SCHEMA_COLUMN(Wifi_Associated_Clients, state);
    const char *mac = SCHEMA_COLUMN(Wifi_Associated_Clients, mac);
    json_t *row;

    LOGI("%s: applying workaround %s", addr, __func__);

    row = json_object();
    json_object_set_new(row, state, json_string("idle"));
    WARN_ON(ovsdb_sync_update(table, mac, addr, row) != 1);

    row = json_object();
    json_object_set_new(row, state, json_string("active"));
    WARN_ON(ovsdb_sync_update(table, mac, addr, row) != 1);
}

bool
wm2_clients_update(struct schema_Wifi_Associated_Clients *schema, char *ifname, bool status)
{
    struct schema_Wifi_Associated_Clients client;
    const char *column;
    const char *table;
    ovs_uuid_t uuid;
    json_t *where;
    json_t *row;
    char oftag[32];
    bool ok;
    int err;
    int n;

    oftag[0] = 0;

    if (schema->key_id_exists) {
        err = wm2_clients_oftag_from_key_id(ifname,
                                            schema->key_id,
                                            oftag,
                                            sizeof(oftag));
        if (err)
            LOGW("%s: failed to convert key '%s' to oftag (%s), expect openflow/firewall issues",
                 ifname, schema->key_id, oftag);

        LOGD("%s: key_id '%s' => oftag '%s'",
             schema->mac, schema->key_id, oftag);
    }

    LOGD("%s: update called with keyid='%s' oftag='%s' status=%d",
         schema->mac, schema->key_id, oftag, status);

    memset(&client, 0, sizeof(client));
    ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                           SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                           schema->mac,
                           &client);

    if (status) {
        table = SCHEMA_TABLE(Wifi_Associated_Clients);
        column = SCHEMA_COLUMN(Wifi_Associated_Clients, mac);
        where = ovsdb_where_simple(column, schema->mac);
        row = json_object();
        json_object_set_new(row, "mac", json_string(schema->mac));
        json_object_set_new(row, "key_id", json_string(schema->key_id));
        json_object_set_new(row, "state", json_string(schema->state));
        if (strlen(oftag) > 0)
            json_object_set_new(row, "oftag", json_string(oftag));

        json_incref(row);
        n = ovsdb_sync_update_one_get_uuid(table, where, row, &uuid);
        if (WARN_ON(n < 0)) {
            json_decref(row);
            return false;
        }
        else if (n == 0) {
            LOGN("Client '%s' connected on '%s' with key '%s'",
                 schema->mac, ifname, schema->key_id);

            ok = ovsdb_sync_insert(table, row, &uuid);
            if (WARN_ON(!ok))
                return false;
        }
        else {
            json_decref(row);
        }

        table = SCHEMA_TABLE(Wifi_VIF_State);
        column = SCHEMA_COLUMN(Wifi_VIF_State, if_name);
        where = ovsdb_where_simple(column, ifname);
        column = SCHEMA_COLUMN(Wifi_VIF_State, associated_clients);
        json_incref(where);

        ovsdb_sync_mutate_uuid_set(table, where, column, OTR_DELETE, uuid.uuid);
        ovsdb_sync_mutate_uuid_set(table, where, column, OTR_INSERT, uuid.uuid);
        n = wm2_clients_get_refcount(uuid.uuid);

        WARN_ON(n > 1 && !client.mac_exists);

        if (n == 1 &&
            client.mac_exists &&
            strcmp(client.key_id, schema->key_id)) {
            LOGN("Client '%s' re-connected on '%s' with key '%s'",
                 schema->mac, ifname, schema->key_id);
        }

        if (n > 1) {
            LOGN("Client '%s' roamed to '%s' with key '%s'",
                 schema->mac, ifname, schema->key_id);
        }

        if (strlen(client.oftag) > 0)
            wm2_clients_oftag_unset(schema->mac, client.oftag);
        if (strlen(oftag) > 0)
            wm2_clients_oftag_set(schema->mac, oftag);
        wm2_clients_isolate(ifname, schema->mac, true);
        wm2_clients_war_esw_2684_noc_163_plat_878(schema->mac, schema->key_id);
    } else {
        if (!client.mac_exists) {
            LOGD("Client '%s' cannot be removed from '%s' because it does not exist",
                 schema->mac, ifname);
            return true;
        }

        table = SCHEMA_TABLE(Wifi_VIF_State);
        column = SCHEMA_COLUMN(Wifi_VIF_State, if_name);
        where = ovsdb_where_simple(column, ifname);
        column = SCHEMA_COLUMN(Wifi_VIF_State, associated_clients);
        ovsdb_sync_mutate_uuid_set(table, where, column, OTR_DELETE, client._uuid.uuid);

        n = wm2_clients_get_refcount(client._uuid.uuid);
        if (n == 0) {
            LOGN("Client '%s' disconnected from '%s' with key '%s'",
                 schema->mac, ifname, schema->key_id);

            table = SCHEMA_TABLE(Wifi_Associated_Clients);
            column = SCHEMA_COLUMN(Wifi_Associated_Clients, mac);
            where = ovsdb_where_simple(column, schema->mac);
            n = ovsdb_sync_delete_where(table, where);
            WARN_ON(n != 1);

            if (strlen(client.oftag) > 0)
                wm2_clients_oftag_unset(schema->mac, client.oftag);
        } else {
            LOGN("Client '%s' removed from '%s' with key '%s'",
                 schema->mac, ifname, schema->key_id);
        }

        wm2_clients_isolate(ifname, schema->mac, false);
    }

    return true;
}

/******************************************************************************
 *  PUBLIC definitions
 *****************************************************************************/
bool
wm2_clients_init(char *if_name)
{
    if (!if_name) {
        LOGE("Initializing clients (input validation failed)" );
        return false;
    }

    if (false == target_clients_register(if_name, wm2_clients_update)) {
        return false;
    }

    return true;
}
