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

#include "hello_world.h"

/* Log entries from this file will contain "OVSDB" */
#define MODULE_ID LOG_MODULE_ID_OVSDB

ovsdb_table_t table_Node_Config;
ovsdb_table_t table_Node_State;


void callback_Node_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Node_Config *old_rec,
        struct schema_Node_Config *conf)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW)
    {
        LOGI("%s: new node config entry: module %s, key: %s, value: %s",
             __func__, conf->module, conf->key, conf->value);
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        LOGI("%s: node config entry deleted: module %s, key: %s, value: %s",
             __func__, old_rec->module, old_rec->key, old_rec->value);
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        LOGI("%s: node config entry updated: \n"
             "old module: %s, old key: %s, old value: %s \n"
             "new module: %s, new key: %s, new value: %s",
             __func__, old_rec->module, old_rec->key, old_rec->value,
             conf->module, conf->key, conf->value);
    }

    // Special handling for demo
    hello_world_demo_config(
            mon->mon_type == OVSDB_UPDATE_DEL,
            mon->mon_type == OVSDB_UPDATE_DEL ? old_rec->module : conf->module,
            mon->mon_type == OVSDB_UPDATE_DEL ? old_rec->key    : conf->key,
            mon->mon_type == OVSDB_UPDATE_DEL ? old_rec->value  : conf->value);
}

void hello_world_ovsdb_state(
        bool remove,
        const char *module,
        const char *key,
        const char *value)
{
    json_t *where;
    json_t *cond;
    struct schema_Node_State node_state;

    where = json_array();

    cond = ovsdb_tran_cond_single("module", OFUNC_EQ, (char *)module);
    json_array_append_new(where, cond);

    // Remove entry
    if (remove)
    {
        ovsdb_table_delete_where(&table_Node_State, where);
        return;
    }

    // Update or insert entry
    MEMZERO(node_state);
    SCHEMA_SET_STR(node_state.module, module);
    SCHEMA_SET_STR(node_state.key, key);
    SCHEMA_SET_STR(node_state.value, value);
    SCHEMA_SET_INT(node_state.persist, true);
    ovsdb_table_upsert_where(&table_Node_State, where, &node_state, false);

    return;
}

int hello_world_ovsdb_init(void)
{
    LOGI("Initializing Hello World tables");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT_NO_KEY(Node_Config);
    OVSDB_TABLE_INIT_NO_KEY(Node_State);

    // Initialize OVSDB monitor callbacks
    OVSDB_TABLE_MONITOR(Node_Config, false);

    return 0;
}
