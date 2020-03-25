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
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "json_util.h"
#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "schema.h"
#include "log.h"

#define OVSDB_OPENFLOW_TAG_TABLE "Openflow_Tag"
#define OFTAG_LOCAL_ETH_DEVICES  "eth_devices"

bool lan_clients_check_oftag(char *oftag)
{
    json_t *row;

    LOGD("%s: checking oftag", oftag);

    row = ovsdb_sync_select(OVSDB_OPENFLOW_TAG_TABLE, "name", OFTAG_LOCAL_ETH_DEVICES);
    if (json_array_size(row) > 0) {
        LOGD("%s: tag is present", oftag);
        return true;
    }

    return false;
}

bool lan_clients_add_oftag(char *oftag)
{
    pjs_errmsg_t err;
    ovs_uuid_t uuid;
    struct schema_Openflow_Tag eth_tag;

    LOGD("%s: adding oftag", oftag);

    memset(&eth_tag, 0, sizeof(eth_tag));
    strncpy(eth_tag.name, OFTAG_LOCAL_ETH_DEVICES, (sizeof(eth_tag.name) - 1));

    if (false == ovsdb_sync_insert(OVSDB_OPENFLOW_TAG_TABLE,
                                   schema_Openflow_Tag_to_json(&eth_tag, err),
                                   &uuid))
    {
        LOGE("%s: adding oftag", oftag);
        return false;
    }

    return true;
}


int lan_clients_oftag_add_mac(char *mac)
{
    json_t *result = NULL;
    json_t *where = NULL;
    json_t *rows = NULL;
    json_t *row = NULL;
    int cnt = 0;
    char *oftag = OFTAG_LOCAL_ETH_DEVICES;

    LOGD("%s: setting oftag='%s'", mac, oftag);

    where = ovsdb_where_simple("name", oftag);
    if (!where)
    {
        LOGW("%s: failed to allocate ovsdb condition, oom?", mac);
        goto error_where;
    }

    row = ovsdb_mutation("device_value",
                         json_string("insert"),
                         json_string(mac));
    if (!row)
    {
        LOGW("%s: failed to allocate ovsdb mutation, oom?", mac);
        goto error_row;
    }

    rows = json_array();
    if (!rows) {
        LOGW("%s: failed to allocate ovsdb mutation list, oom?", mac);
        goto error_rows;
    }

    json_array_append_new(rows, row);

    result = ovsdb_tran_call_s(OVSDB_OPENFLOW_TAG_TABLE,
                               OTR_MUTATE,
                               where,
                               rows);
    if (!result) {
        LOGW("%s: failed to execute ovsdb transact", mac);
        goto error_result;
    }

    cnt = ovsdb_get_update_result_count(result,
                                        OVSDB_OPENFLOW_TAG_TABLE,
                                        "mutate");

    return cnt;

error_result:
    if (rows != NULL)
        json_decref(rows);
error_rows:
    if (row != NULL)
        json_decref(row);
error_row:
    if (where != NULL)
        json_decref(where);
error_where:
    return -1;

}

int lan_clients_oftag_remove_mac(char *mac)
{
    json_t *result;
    json_t *where;
    json_t *rows;
    json_t *row;
    char col[32];
    char val[32];
    int cnt;

    LOGD("%s: removing oftag", mac);

    snprintf(col, sizeof(col), "device_value");
    tsnprintf(val, sizeof(val), "%s", mac);

    where = ovsdb_tran_cond(OCLM_STR, col, OFUNC_INC, val);
    if (!where) {
        LOGW("%s: failed to allocate ovsdb condition, oom?", mac);
        goto error_where;
    }

    row = ovsdb_mutation("device_value",
                         json_string("delete"),
                         json_string(mac));
    if (!row) {
        LOGW("%s: failed to allocate ovsdb mutation, oom?", mac);
        goto error_row;
    }

    rows = json_array();
    if (!rows) {
        LOGW("%s: failed to allocate ovsdb mutation list, oom?", mac);
        goto error_rows;
    }

    json_array_append_new(rows, row);

    result = ovsdb_tran_call_s(OVSDB_OPENFLOW_TAG_TABLE,
                               OTR_MUTATE,
                               where,
                               rows);
    if (!result) {
        LOGW("%s: failed to execute ovsdb transact", mac);
        goto error_result;
    }

    cnt = ovsdb_get_update_result_count(result,
                                        OVSDB_OPENFLOW_TAG_TABLE,
                                        "mutate");

    return cnt;

error_result:
    if (rows != NULL)
        json_decref(rows);
error_rows:
    if (row != NULL)
        json_decref(row);
error_row:
    if (where != NULL)
        json_decref(where);
error_where:
    return -1;
}

int nm2_mac_tags_ovsdb_init(void)
{
    if (lan_clients_check_oftag(OFTAG_LOCAL_ETH_DEVICES) == false)
        lan_clients_add_oftag(OFTAG_LOCAL_ETH_DEVICES);

    return 0;
}
