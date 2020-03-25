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

#include <stdbool.h>
#include <string.h>
#include <jansson.h>

#include "ds.h"
#include "util.h"
#include "log.h"
#include "json_util.h"
#include "ovsdb.h"
#include "ovsdb_priv.h"
#include "ovsdb_update.h"
#include "ovsdb_table.h"
#include "ovsdb_sync.h"

#define MODULE_ID LOG_MODULE_ID_OVSDB

void ovsdb_table_update_cb(ovsdb_update_monitor_t *self);

int ovsdb_table_init(
    char                *table_name,
    ovsdb_table_t       *table,
    int                 schema_size,
    int                 upd_type_offset,
    int                 uuid_offset,
    int                 version_offset,
    schema_from_json_t  *from_json,
    schema_to_json_t    *to_json,
    schema_mark_changed_t *mark_changed,
    char                **columns)
{
    memset(table, 0, sizeof(*table));
    table->table_name = strdup(table_name);
    table->schema_size = schema_size;
    table->upd_type_offset = upd_type_offset;
    table->uuid_offset = uuid_offset;
    table->key_offset = -1;
    table->key2_offset = -1;
    table->version_offset = version_offset;
    table->from_json = from_json;
    table->to_json = to_json;
    table->mark_changed = mark_changed;
    table->columns = columns;
    table->monitor_callback = ovsdb_table_update_cb;
    // cache
    table->row_size = sizeof(ovsdb_cache_row_t) + schema_size;
    ds_tree_init(&table->rows, (ds_key_cmp_t*)strcmp, ovsdb_cache_row_t, node);
    ds_tree_init(&table->rows_k, (ds_key_cmp_t*)strcmp, ovsdb_cache_row_t, node_k);
    ds_tree_init(&table->rows_k2, (ds_key_cmp_t*)strcmp, ovsdb_cache_row_t, node_k2);
    return 0;
}


// SCHEMA CONVERT

// first column has to be "+" or "-" to select filter in/out
json_t* ovsdb_table_filter_row(json_t *row, char *columns[])
{
    int argc = 0;
    char *op;
    if (!row) return row;
    if (!columns) return row; // no filter
    while (columns[argc]) argc++;
    if (!argc) return row; // empty filter
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE)) {
        char str[1024];
        strfmt_nt_array(str, sizeof(str), columns);
        LOG(TRACE, "%s: %s", __FUNCTION__, str);
    }
    op = columns[0]; // + or -
    if (strcmp(op, "+") && strcmp(op, "-")) {
        LOG(ERR, "%s: invalid op: '%s' [%d]", __FUNCTION__, op, argc);
        return row;
    }
    if (argc < 1) {
        LOG(WARNING, "%s: filter op: '%s' no columns [%d]", __FUNCTION__, op, argc);
        return row;
    }
    argc--;
    columns++;
    if (*op == '+')
    {
        return ovsdb_row_filter_argv(row, argc, columns);
    }
    else if (*op == '-')
    {
        return ovsdb_row_filtout_argv(row, argc, columns);
    }
    return row;
}


bool ovsdb_table_from_json(ovsdb_table_t *table, json_t *jrow, void *record)
{
    pjs_errmsg_t perr;
    bool ret;
    ret = table->from_json(record, jrow, false, perr);
    if (!ret)
    {
        LOG(ERR, "Table %s parsing error: %s", table->table_name, perr);
        return false;
    }
    return true;
}


json_t* ovsdb_table_to_json(ovsdb_table_t *table, void *record)
{
    pjs_errmsg_t perr;
    json_t *jrow = table->to_json(record, perr);
    if (!jrow)
    {
        LOG(ERR, "Table %s convert error: %s", table->table_name, perr);
        return NULL;
    }
    return jrow;
}

json_t* ovsdb_table_to_json_f(ovsdb_table_t *table, void *record, char *filter[])
{
    json_t *jrow = ovsdb_table_to_json(table, record);
    return ovsdb_table_filter_row(jrow, filter);
}


// WHERE
// if table key defined then select on primary key value from record
// otherwise select all (if no key defined one record expected)
// if record == NULL also select all
json_t* ovsdb_table_where(ovsdb_table_t *table, void *record)
{
    if (record && table->key_offset >= 0 && *table->key_name)
    {
        return ovsdb_where_simple(table->key_name, record + table->key_offset);
    }
    return NULL;
}



// SELECT

// return array of records[*count]
void* ovsdb_table_select_where(ovsdb_table_t *table, json_t *where, int *count)
{
    int i;
    int cnt = 0;
    json_t *jrows = NULL;
    json_t *jrow = NULL;
    void *records_array = NULL;
    void *record = NULL;
    void *retval = NULL;

    *count = 0;
    jrows = ovsdb_sync_select_where(table->table_name, where);
    if (!jrows) return NULL;
    cnt = json_array_size(jrows);
    if (!cnt) goto out;
    records_array = calloc(1, cnt * table->schema_size);
    if (!records_array) goto out;
    for (i = 0; i < cnt; i++)
    {
        jrow = json_array_get(jrows, i);
        if (!jrow) goto out;
        record = records_array + table->schema_size * i;
        if (!ovsdb_table_from_json(table, jrow, record)) goto out;
    }
    retval = records_array;
    *count = cnt;

out:
    if (!retval && records_array)
        free(records_array);
    json_decref(jrows);
    return retval;
}

void* ovsdb_table_select(ovsdb_table_t *table, char *column, char *value, int *count)
{
    return ovsdb_table_select_where(table, ovsdb_where_simple(column, value), count);
}

void* ovsdb_table_select_typed(ovsdb_table_t *table, char *column, ovsdb_col_t col_type, void *value, int *count)
{
    return ovsdb_table_select_where(table, ovsdb_where_simple_typed(column, value, col_type), count);
}

// where has to match a single record otherwise error is returned
bool ovsdb_table_select_one_where(ovsdb_table_t *table, json_t *where, void *record)
{
    int cnt = 0;
    json_t *jrows = NULL;
    json_t *jrow = NULL;
    bool retval = false;

    jrows = ovsdb_sync_select_where(table->table_name, where);
    if (!jrows) goto out;
    cnt = json_array_size(jrows);
    if (cnt != 1) goto out;
    jrow = json_array_get(jrows, 0);
    if (!jrow) goto out;
    if (!ovsdb_table_from_json(table, jrow, record)) goto out;
    retval = true;

out:
    json_decref(jrows);
    return retval;
}

bool ovsdb_table_select_one(ovsdb_table_t *table, const char *column, const char *value, void *record)
{
    return ovsdb_table_select_one_where(table, ovsdb_where_simple(column, value), record);
}



// INSERT

bool ovsdb_table_insert(ovsdb_table_t *table, void *record)
{
    json_t *jrow = NULL;
    bool ret;

    jrow = ovsdb_table_to_json(table, record);
    if (!jrow) return false;
    ret = ovsdb_sync_insert(table->table_name, jrow, record + table->uuid_offset);
    return ret;
}


// DELETE

// if where is NULL, delete all rows in table
int ovsdb_table_delete_where(ovsdb_table_t *table, json_t *where)
{
    return ovsdb_sync_delete_where(table->table_name, where);
}

int ovsdb_table_delete_simple(ovsdb_table_t *table, const char *column, const char *value)
{
    json_t *where = ovsdb_where_simple(column, value);
    return ovsdb_sync_delete_where(table->table_name, where);
}

// if the table has no key, or record is NULL then delete all rows
int ovsdb_table_delete(ovsdb_table_t *table, void *record)
{
    json_t *where = ovsdb_table_where(table, record);
    return ovsdb_sync_delete_where(table->table_name, where);
}


// UPDATE

// if where is NULL, update all rows in table
int ovsdb_table_update_where_f(ovsdb_table_t *table, json_t *where, void *record, char *filter[])
{
    json_t *jrow = NULL;
    int ret;

    jrow = ovsdb_table_to_json_f(table, record, filter);
    if (!jrow) {
        json_decref(where);
        return 0;
    }
    ret = ovsdb_sync_update_where(table->table_name, where, jrow);
    return ret;
}


int ovsdb_table_update_where(ovsdb_table_t *table, json_t *where, void *record)
{
    return ovsdb_table_update_where_f(table, where, record, NULL);
}

int ovsdb_table_update_simple_f(ovsdb_table_t *table, char *column, char *value, void *record, char *filter[])
{
    json_t *where = ovsdb_where_simple(column, value);
    return ovsdb_table_update_where_f(table, where, record, filter);
}

int ovsdb_table_update_simple(ovsdb_table_t *table, char *column, char *value, void *record)
{
    return ovsdb_table_update_simple_f(table, column, value, record, NULL);
}

// update using table primary key
// if the table has no key, then update all rows (1 expected)
int ovsdb_table_update_f(ovsdb_table_t *table, void *record, char *filter[])
{
    json_t *where = ovsdb_table_where(table, record);
    int ret = ovsdb_table_update_where_f(table, where, record, filter);
    if (ret > 1)
    {
        LOG(ERR, "%s: count > 1: %d", __FUNCTION__, ret);
    }
    return ret;
}

int ovsdb_table_update(ovsdb_table_t *table, void *record)
{
    return ovsdb_table_update_f(table, record, NULL);
}

// UPSERT

bool ovsdb_table_upsert_where_f(ovsdb_table_t *table,
        json_t *where, void *record, bool update_uuid, char *filter[])
{
    json_t *jrow = NULL;
    ovs_uuid_t *uuid = update_uuid ?  uuid = record + table->uuid_offset : NULL;
    bool ret;

    jrow = ovsdb_table_to_json_f(table, record, filter);
    if (!jrow) return false;
    ret = ovsdb_sync_upsert_where(table->table_name, where, jrow, uuid);
    LOG(DEBUG, "%s: %s %s", __FUNCTION__, table->table_name, ret?"success":"error");
    return ret;
}

bool ovsdb_table_upsert_where(ovsdb_table_t *table, json_t *where, void *record, bool update_uuid)
{
    return ovsdb_table_upsert_where_f(table, where, record, update_uuid, NULL);
}


bool ovsdb_table_upsert_simple_f(ovsdb_table_t *table,
        char *column, char *value, void *record, bool update_uuid, char *filter[])
{
    return ovsdb_table_upsert_where_f(table,
            ovsdb_where_simple(column, value),
            record, update_uuid, filter);
}

bool ovsdb_table_upsert_simple(ovsdb_table_t *table,
        char *column, char *value, void *record, bool update_uuid)
{
    return ovsdb_table_upsert_simple_f(table, column, value, record, update_uuid, NULL);
}

// upsert using table primary key
// if the table has no key, then update all rows (1 expected)
bool ovsdb_table_upsert_f(ovsdb_table_t *table, void *record, bool update_uuid, char *filter[])
{
    json_t *where = ovsdb_table_where(table, record);
    return ovsdb_table_upsert_where_f(table, where, record, update_uuid, filter);
}

bool ovsdb_table_upsert(ovsdb_table_t *table, void *record, bool update_uuid)
{
    return ovsdb_table_upsert_f(table, record, update_uuid, NULL);
}


// MUTATE


int ovsdb_table_mutate_uuid_set(ovsdb_table_t *table,
        json_t *where, char *column, ovsdb_tro_t op, char *uuid)
{
    return ovsdb_sync_mutate_uuid_set(table->table_name,
            where, column, op, uuid);
}

bool ovsdb_table_upsert_with_parent_where(ovsdb_table_t *table,
        json_t *where, void *record, bool update_uuid, char *filter[],
        char *parent_table, json_t *parent_where, char *parent_column)
{
    json_t *jrow = NULL;
    ovs_uuid_t *uuid = update_uuid ?  uuid = record + table->uuid_offset : NULL;
    bool ret;

    jrow = ovsdb_table_to_json_f(table, record, filter);
    if (!jrow) return false;
    ret = ovsdb_sync_upsert_with_parent(table->table_name, where, jrow, uuid,
        parent_table, parent_where, parent_column);
    LOG(DEBUG, "%s: %s %s", __FUNCTION__, table->table_name, ret?"success":"error");
    return ret;
}

// upsert using table primary key
// if the table has no key, then update all rows (1 expected)
bool ovsdb_table_upsert_with_parent(ovsdb_table_t *table,
        void *record, bool update_uuid, char *filter[],
        char *parent_table, json_t *parent_where, char *parent_column)
{
    json_t *where = ovsdb_table_where(table, record);
    return ovsdb_table_upsert_with_parent_where(table,
        where, record, update_uuid, filter,
        parent_table, parent_where, parent_column);
}

int ovsdb_table_delete_where_with_parent(ovsdb_table_t *table, json_t *where,
        char *parent_table, json_t *parent_where, char *parent_column)
{
    return ovsdb_sync_delete_with_parent(table->table_name, where,
        parent_table, parent_where, parent_column);
}


// MONITOR


bool ovsdb_table_monitor_columns(ovsdb_table_t *table,
        ovsdb_table_callback_t *callback, char **columns)
{
    bool ret;
    int count = count_nt_array(columns);

    if (!columns || !count)
    {
        LOG(NOTICE, "Monitor: %s: ALL", table->table_name);
        ret = ovsdb_update_monitor(
                &table->monitor,
                table->monitor_callback,
                table->table_name,
                OMT_ALL);
    }
    else
    {
        // are all schema columns present?
        if (!is_array_in_array(table->columns, columns))
        {
            // no, enable partial updates in monitor callback
            table->partial_update = true;
        }
        bool have_version = is_inarray("_version", count, columns);
        char tmp[1024];
        LOG(NOTICE, "Monitor: %s _version: %s partial: %s columns: %d %s", table->table_name,
                have_version ? "true" : "false",
                table->partial_update ? "true" : "false", count,
                strfmt_nt_array(tmp, sizeof(tmp), columns));
        ret = ovsdb_update_monitor_ex(
                &table->monitor,
                table->monitor_callback,
                table->table_name,
                OMT_ALL,
                count,
                columns);
    }
    if (!ret)
    {
        LOGE("Error registering watcher for %s.", table->table_name);
        return false;
    }
    table->monitor.mon_data = table;
    table->table_callback = callback;
    return true;
}

// ignore_version can be used if we are not interested in receiving
// updates for when a referenced table has been modified
bool ovsdb_table_monitor(ovsdb_table_t *table,
        ovsdb_table_callback_t *callback, bool ignore_version)
{
    char **columns;
    if (ignore_version)
    {
        columns = table->columns;
    }
    else
    {
        columns = NULL;
    }
    return ovsdb_table_monitor_columns(table, callback, columns);
}

bool ovsdb_table_monitor_filter(ovsdb_table_t *table,
        ovsdb_table_callback_t *callback, char **filter)
{
    int schema_count = count_nt_array(table->columns);
    char *cols[schema_count + 2]; // +2: _version, NULL
    char **columns;
    if (!filter)
    {
        columns = NULL;
    }
    else if (**filter == '-')
    {
        // filter out
        // prepare all cols first - schema + _version, NULL
        memcpy(cols, table->columns, schema_count * sizeof(*cols));
        cols[schema_count+0] = "_version";
        cols[schema_count+1] = NULL;
        filter_out_nt_array(cols, filter + 1);
        columns = cols;
    }
    else if (**filter == '+')
    {
        columns = filter + 1;
    }
    else
    {
        columns = filter;
    }
    return ovsdb_table_monitor_columns(table, callback, columns);
}

void ovsdb_table_update_cb(ovsdb_update_monitor_t *self)
{
    ovsdb_table_t *table;
    pjs_errmsg_t perr;
    char *mon_uuid;
    char *row_uuid;
    char *typestr;
    bool ret;
    int mon_type = self->mon_type;

    mon_uuid = (char*)self->mon_uuid;

    switch (mon_type)
    {
        case OVSDB_UPDATE_NEW:      typestr = "NEW"; break;
        case OVSDB_UPDATE_MODIFY:   typestr = "MOD"; break;
        case OVSDB_UPDATE_DEL:      typestr = "DEL"; break;
        default:
            LOG(ERR, "Table %s update %s type error %d",
                    self->mon_table, mon_uuid, mon_type);
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

    LOG(INFO, "MON upd: %s table: %s row: %s", typestr, table->table_name, mon_uuid );

    if (LOG_SEVERITY_TRACE <= log_module_severity_get(MODULE_ID))
    {
        // json_old contains _uuid and columns that have changed with old values
        LOG(TRACE, "OLD: %s", json_dumps_static(self->mon_json_old, 0));
        // json_new always contains full record of monitored columns
        LOG(TRACE, "NEW: %s", json_dumps_static(self->mon_json_new, 0));
    }

    char record[table->schema_size];
    char old_record[table->schema_size];
    bool partial = table->partial_update;
    json_t *j_rec = NULL;

    memset(old_record, 0, sizeof(old_record));
    memset(record, 0, sizeof(record));
    self->mon_old_rec = old_record;

    // ovsdb monitor update json_old/json_new record behaviour:
    //   on NEW:    old = empty,                        new = full record
    //   on MODIFY: old = old values of changed fields, new = full record
    //   on DELETE: old = full old record,              new = empty
    // this maps to converted old_record/record for callback:
    //   on NEW:    old = empty,                        rec = full record
    //   on MODIFY: old = old values of changed fields, rec = full record
    //   on DELETE: old = full old record,              rec = full old record

    // convert old
    if (mon_type != OVSDB_UPDATE_NEW) {
        ret = table->from_json(old_record, self->mon_json_old, true, perr);
        if (!ret)
        {
            LOG(ERR, "Table %s %s parsing OLD %s error: %s",
                    table->table_name, typestr, mon_uuid, perr);
            return;
        }
    }
    // convert current
    if (mon_type == OVSDB_UPDATE_DEL) {
        j_rec = self->mon_json_old;
    } else {
        j_rec = self->mon_json_new;
    }
    ret = table->from_json(record, j_rec, partial, perr);
    if (!ret)
    {
        LOG(ERR, "Table %s %s parsing %s error: %s",
                table->table_name, typestr, mon_uuid, perr);
        return;
    }
    // set _update_type
    int *_update_type = (int*)(record + table->upd_type_offset);
    *_update_type = mon_type;
    // mark _changed
    if (mon_type == OVSDB_UPDATE_MODIFY) {
        table->mark_changed(old_record, record);
    }
    // uuid integrity check
    row_uuid = record + table->uuid_offset;
    if (strcmp(row_uuid, mon_uuid))
    {
        LOG(ERR, "Table %s %s uuid mismatch '%s' '%s'",
                table->table_name, typestr, mon_uuid, row_uuid);
    }

    // callback
    if (table->table_callback) table->table_callback(self, old_record, record);

    LOG(DEBUG, "<<< DONE: MON upd: %s table: %s row: %s ver: %s",
        typestr, table->table_name, mon_uuid, record + table->version_offset);

    return;
}
