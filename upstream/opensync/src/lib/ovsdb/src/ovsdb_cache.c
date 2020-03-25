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
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "ovsdb_table.h"
#include "ovsdb_sync.h"

#define MODULE_ID LOG_MODULE_ID_OVSDB

void ovsdb_cache_update_cb(ovsdb_update_monitor_t *self);

// ignore_version can be used if we are not interested in receiving
// updates for when a referenced table has been modified
bool ovsdb_cache_monitor_columns(ovsdb_table_t *table,
        ovsdb_cache_callback_t *callback, char **columns)
{
    table->monitor_callback = ovsdb_cache_update_cb;
    table->cache_callback = callback;
    return ovsdb_table_monitor_columns(table, NULL, columns);
}

bool ovsdb_cache_monitor(ovsdb_table_t *table,
        ovsdb_cache_callback_t *callback, bool ignore_version)
{
    table->monitor_callback = ovsdb_cache_update_cb;
    table->cache_callback = callback;
    return ovsdb_table_monitor(table, NULL, ignore_version);
}

bool ovsdb_cache_monitor_filter(ovsdb_table_t *table,
        ovsdb_cache_callback_t *callback, char **filter)
{
    table->monitor_callback = ovsdb_cache_update_cb;
    table->cache_callback = callback;
    return ovsdb_table_monitor(table, NULL, filter);
}

// debug dump table
void ovsdb_cache_dump_table(ovsdb_table_t *table, char *str)
{
    ovsdb_cache_row_t *row, *prev = NULL;
    char *uuid;
    char *version;
    char *key = "";
    int i = 0;

    if (LOG_SEVERITY_DEBUG > log_module_severity_get(MODULE_ID)) return;

    printf("Table cache: %s %s\n", table->table_name, str?str:"");
    ds_tree_foreach(&table->rows, row)
    {
        if (row == prev)
        {
            printf(" %d loop error %p %p\n", i, prev, row);
            break;
        }
        prev = row;
        uuid = row->record + table->uuid_offset;
        version = row->record + table->version_offset;
        if (table->key_offset >= 0)
        {
            key = row->record + table->key_offset;
        }
        printf(" %d uuid: %s ver: %s key: %s\n", i, uuid, version, key);
        i++;
    }
}

void _ovsdb_cache_insert_row(ovsdb_table_t *table, ovsdb_cache_row_t *row)
{
    char *row_uuid = row->record + table->uuid_offset;
    char *key = "";
    char *key2 = "";
    char msg[128];

    ds_tree_insert(&table->rows, row, row_uuid);
    if (table->key_offset >= 0)
    {
        key = row->record + table->key_offset;
        ds_tree_insert(&table->rows_k, row, key);
    }
    if (table->key2_offset >= 0)
    {
        key2 = row->record + table->key2_offset;
        ds_tree_insert(&table->rows_k2, row, key2);
    }
    snprintf(msg, sizeof(msg), "insert %s key: %s", row_uuid, key);
    ovsdb_cache_dump_table(table, msg);
}

void ovsdb_cache_update_cb(ovsdb_update_monitor_t *self)
{
    ovsdb_table_t *table;
    ovsdb_cache_row_t *row;
    pjs_errmsg_t perr;
    char *mon_uuid;
    char *row_uuid;
    char *typestr;
    char *key = NULL;
    bool ret;
    bool new_ver;
    int mon_type;

    mon_uuid = (char*)self->mon_uuid;

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:      typestr = "NEW"; break;
        case OVSDB_UPDATE_MODIFY:   typestr = "MOD"; break;
        case OVSDB_UPDATE_DEL:      typestr = "DEL"; break;
        default:
            LOG(ERR, "Table %s update %s type error %d",
                    self->mon_table, mon_uuid, self->mon_type);
            return;
    }

    table = self->mon_data;
    if (!table)
    {
        LOG(ERR, "Table %s %s %s: mon_data NULL",
            self->mon_table, typestr, mon_uuid);
        return;
    }

    // table integrity check
    if (strcmp(table->table_name, self->mon_table))
    {
        LOG(ERR, "Table %s %s mismatch %s",
            table->table_name, typestr, self->mon_table);
        return;
    }

    row = ds_tree_find(&table->rows, mon_uuid);

    if (row && table->key_offset >= 0)
    {
        key = row->record + table->key_offset;
    }

    LOG(INFO, "MON upd: %s table: %s row: %s cached: %s%s%s",
        typestr, table->table_name, mon_uuid, row?"y":"n",
        row?" key: ":"", row?key:"");

    if (LOG_SEVERITY_TRACE <= log_module_severity_get(MODULE_ID))
    {
        // json_old contains _uuid and columns that have changed with old values
        LOG(TRACE, "OLD: %s", json_dumps_static(self->mon_json_old, 0));
        // json_new always contains full record of monitored columns
        LOG(TRACE, "NEW: %s", json_dumps_static(self->mon_json_new, 0));
    }

    char record[table->schema_size];
    char old_record[table->schema_size];
    memset(old_record, 0, sizeof(old_record));
    self->mon_old_rec = old_record;

    mon_type = self->mon_type;
    if (row && (mon_type == OVSDB_UPDATE_NEW))
    {
        LOG(NOTICE, "Table %s %s found cached %s doing update",
                table->table_name, typestr, mon_uuid);
        mon_type = OVSDB_UPDATE_MODIFY;
    }

    switch (mon_type)
    {
        case OVSDB_UPDATE_NEW:
            if (row)
            {
                LOG(ERR, "Table %s %s unexpected %s already exists",
                    table->table_name, typestr, mon_uuid);
                return;
            }
            row = calloc(1, table->row_size);
            ret = table->from_json(row->record, self->mon_json_new,
                    table->partial_update, perr);
            if (!ret)
            {
                free(row);
                LOG(ERR, "Table %s %s parsing %s error: %s",
                    table->table_name, typestr, mon_uuid, perr);
                return;
            }
            // uuid integrity check
            row_uuid = row->record + table->uuid_offset;
            if (strcmp(row_uuid, mon_uuid))
            {
                LOG(ERR, "Table %s %s uuid mismatch %s %s",
                    table->table_name, typestr, mon_uuid, row_uuid);
            }

            _ovsdb_cache_insert_row(table, row);
            break;

        case OVSDB_UPDATE_MODIFY:
            if (!row)
            {
                LOG(ERR, "Table %s %s unexpected %s does not exist",
                    table->table_name, typestr, mon_uuid);
                return;
            }
            memset(record, 0, sizeof(record));
            ret = table->from_json(record, self->mon_json_new, true, perr);
            if (!ret)
            {
                LOG(ERR, "Table %s %s parsing %s error: %s",
                    table->table_name, typestr, mon_uuid, perr);
                return;
            }
            // set _update_type
            int *_update_type = (int*)(record + table->upd_type_offset);
            *_update_type = mon_type;
            // uuid integrity check
            row_uuid = record + table->uuid_offset;
            if (strcmp(row_uuid, mon_uuid))
            {
                LOG(ERR, "Table %s %s uuid mismatch %s %s",
                    table->table_name, typestr, mon_uuid, row_uuid);
            }
            if (self->mon_type != OVSDB_UPDATE_NEW)
            {
                // check if version changed
                new_ver = memcmp(row->record + table->version_offset,
                        record + table->version_offset,
                        sizeof(ovs_uuid_t));
                // ignore version change in record compare
                memcpy(row->record + table->version_offset,
                        record + table->version_offset,
                        sizeof(ovs_uuid_t));
                // ignore _update_type
                memcpy(row->record + table->upd_type_offset,
                        record + table->upd_type_offset,
                        sizeof(int));
                // clear previous _changed flags
                table->mark_changed(old_record, row->record);
                // compare
                if (memcmp(record, row->record, sizeof(record)) == 0)
                {
                    LOG(INFO, "=== upd: %s table: %s row: %s %s%s, EQUAL record - skip callback",
                        typestr, table->table_name, mon_uuid,
                        new_ver?"new-ver: ":"same-ver",
                        new_ver?record + table->version_offset:"");
                    return;
                }
                // convert old
                ret = table->from_json(old_record, self->mon_json_old, true, perr);
                if (!ret)
                {
                    LOG(ERR, "Table %s %s parsing OLD %s error: %s",
                            table->table_name, typestr, mon_uuid, perr);
                    return;
                }
                // mark _changed
                table->mark_changed(old_record, record);
            }
            memcpy(row->record, record, sizeof(record));
            break;

        case OVSDB_UPDATE_DEL:
            if (!row)
            {
                LOG(ERR, "Table %s %s unexpected %s does not exist",
                    table->table_name, typestr, mon_uuid);
                return;
            }
            if (table->cache_callback) table->cache_callback(self, old_record, row->record, row);
            ds_tree_remove(&table->rows, row);
            if (table->key_offset >= 0)
            {
                ds_tree_remove(&table->rows_k, row);
            }
            if (table->key2_offset >= 0)
            {
                ds_tree_remove(&table->rows_k2, row);
            }
            free(row);
            return;

        default:
            return;
    }

    if (table->cache_callback) table->cache_callback(self, old_record, row->record, row);

    LOG(INFO, "<<< DONE: MON upd: %s table: %s row: %s ver: %s",
        typestr, table->table_name, mon_uuid, row->record + table->version_offset);

    return;
}


ovsdb_cache_row_t* _ovsdb_cache_find_row_by_offset(ovsdb_table_t *table, int offset, const char *kname, const char *key)
{
    ovsdb_cache_row_t *row;
    char *row_key;
    if (offset < 0) return NULL;

    ds_tree_foreach(&table->rows, row)
    {
        row_key = row->record + offset;
        if (strcmp(row_key, key) == 0)
        {
            LOG(TRACE, "found table: %s %s: %s", table->table_name, kname, key);
            return row;
        }
    }
    LOG(TRACE, "NOT found table: %s %s: %s", table->table_name, kname, key);
    return NULL;
}

ovsdb_cache_row_t* ovsdb_cache_find_row_by_uuid(ovsdb_table_t *table, const char *uuid)
{
    return _ovsdb_cache_find_row_by_offset(table, table->uuid_offset, "uuid", uuid);
}

ovsdb_cache_row_t* ovsdb_cache_find_row_by_key(ovsdb_table_t *table, const char *key)
{
    return _ovsdb_cache_find_row_by_offset(table, table->key_offset, "key", key);
}

ovsdb_cache_row_t* ovsdb_cache_find_row_by_key2(ovsdb_table_t *table, const char *key2)
{
    return _ovsdb_cache_find_row_by_offset(table, table->key2_offset, "key2", key2);
}

void* ovsdb_cache_find_by_uuid(ovsdb_table_t *table, const char *uuid)
{
    ovsdb_cache_row_t *row = ovsdb_cache_find_row_by_uuid(table, uuid);
    if (row) return row->record;
    return NULL;
}

void* ovsdb_cache_find_by_key(ovsdb_table_t *table, const char *key)
{
    ovsdb_cache_row_t *row = ovsdb_cache_find_row_by_key(table, key);
    if (row) return row->record;
    return NULL;
}

void* ovsdb_cache_find_by_key2(ovsdb_table_t *table, const char *key2)
{
    ovsdb_cache_row_t *row = ovsdb_cache_find_row_by_key2(table, key2);
    if (row) return row->record;
    return NULL;
}


void* ovsdb_cache_get_by_uuid(ovsdb_table_t *table, const char *uuid, void *record)
{
    ovsdb_cache_row_t *row = ovsdb_cache_find_row_by_uuid(table, uuid);
    if (!row) return NULL;
    memcpy(record, row->record, table->schema_size);
    return record;
}

void* ovsdb_cache_get_by_key(ovsdb_table_t *table, const char *key, void *record)
{
    ovsdb_cache_row_t *row = ovsdb_cache_find_row_by_key(table, key);
    if (!row) return NULL;
    memcpy(record, row->record, table->schema_size);
    return record;
}

void* ovsdb_cache_get_by_key2(ovsdb_table_t *table, const char *key2, void *record)
{
    ovsdb_cache_row_t *row = ovsdb_cache_find_row_by_key2(table, key2);
    if (!row)
    {
        memset(record, 0, table->schema_size);
        return NULL;
    }
    else
    {
        memcpy(record, row->record, table->schema_size);
        return record;
    }
}


int ovsdb_cache_upsert(ovsdb_table_t *table, void *record)
{
    char *rec = record;
    ovsdb_cache_row_t *row;
    char *key;
    ovs_uuid_t uuid;
    bool ret;
    // check if primary key defined
    if (table->key_offset < 0)
    {
        LOG(ERR, "table %s upsert: no key", table->table_name);
        return -1;
    }
    // find existing
    key = rec + table->key_offset;
    row = ovsdb_cache_find_row_by_key(table, key);
    if (row)
    {
        // copy uuid and version for compare
        strcpy(rec + table->uuid_offset,    row->record + table->uuid_offset);
        strcpy(rec + table->version_offset, row->record + table->version_offset);
        if (memcmp(rec, row->record, table->schema_size) == 0)
        {
            LOG(INFO, "upsert %s %s: no change", table->table_name, key);
            return 0;
        }
    }
    // changed or new, upsert to db
    // convert to json
    pjs_errmsg_t perr;
    json_t *jrow = table->to_json(record, perr);
    if (!jrow)
    {
        LOG(ERR, "convert %s %s: %s", table->table_name, key, perr);
        return -1;
    }
    // if already have row, skip retrieve uuid
    bool have_uuid = row && (rec[table->uuid_offset] != 0);

    ret = ovsdb_sync_upsert(table->table_name, table->key_name, key, jrow, have_uuid ? NULL : &uuid);
    if (!ret)
    {
        LOG(ERR, "upsert %s %s: error", table->table_name, key);
        return -1;
    }
    // store to cache
    if (row)
    {
        // update existing
        memcpy(row->record, record, table->schema_size);
    }
    else
    {
        // add new
        ovsdb_cache_row_t *new_row;
        new_row = calloc(1, table->row_size);
        strcpy(record + table->uuid_offset, uuid.uuid);
        memcpy(new_row->record, record, table->schema_size);
        LOG(INFO, "upsert %s %s: uuid: %s", table->table_name, key, uuid.uuid);
        _ovsdb_cache_insert_row(table, new_row);
    }
    return 0;
}

int ovsdb_cache_upsert_get_uuid(ovsdb_table_t *table, void *record, ovs_uuid_t *uuid)
{
    int ret;
    char *rec = record;
    ret = ovsdb_cache_upsert(table, record);
    if (ret) return ret;
    if (uuid)
    {
        strcpy(uuid->uuid, rec + table->uuid_offset);
    }
    return ret;
}

int ovsdb_cache_pre_fetch(ovsdb_table_t *table, char *key)
{
    ovsdb_cache_row_t *row;
    pjs_errmsg_t perr;
    json_t *result;
    json_t *jrows;
    json_t *jrow;
    json_t *juuid;
    json_t *where;
    const char *uuid;
    int ret;

    if (ovsdb_cache_find_row_by_key(table, key))
    {
        // already exists
        return 0;
    }

    where = ovsdb_tran_cond(OCLM_STR, table->key_name, OFUNC_EQ, key);
    result = ovsdb_tran_call_s(table->table_name, OTR_SELECT, where, NULL);
    if (!result)
    {
        LOG(WARNING, "%s query: %s %s no result", __FUNCTION__, table->table_name, key);
        return -1;
    }
    LOG(DEBUG, "%s query %s %s result: %s", __FUNCTION__,
            table->table_name, key, json_dumps_static(result, 0));
    jrows = json_object_get(json_array_get(result, 0), "rows");
    if (json_array_size(jrows) != 1)
    {
        LOG(WARNING, "%s query: %s %s num rows: %zd", __FUNCTION__,
                table->table_name, key, json_array_size(jrows));
        json_decref(result);
        return -1;
    }
    jrow = json_array_get(jrows, 0);
    LOG(DEBUG, "row: %s", json_dumps_static(jrow, 0));
    juuid = json_object_get(jrow, "_uuid");
    uuid = json_string_value(json_array_get(juuid, 1));
    LOG(DEBUG, "uuid: %s", uuid);
    if (!uuid)
    {
        LOG(WARNING, "%s query: %s %s no uuid", __FUNCTION__,
                table->table_name, key);
        json_decref(result);
        return -1;
    }

    row = calloc(1, table->row_size);
    ret = table->from_json(row->record, jrow, false, perr);
    json_decref(result);
    if (!ret)
    {
        free(row);
        LOG(ERR, "Table %s parsing %s error: %s",
                table->table_name, uuid, perr);
        return -1;
    }

    // add new
    LOG(NOTICE, "pre-fetch %s %s uuid: %s", table->table_name, key, uuid);
    _ovsdb_cache_insert_row(table, row);

    return 0;
}

