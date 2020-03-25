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

#ifndef OVSDB_TABLE_H_INCLUDED
#define OVSDB_TABLE_H_INCLUDED

#include <stdarg.h>
#include <stdbool.h>
#include <jansson.h>

#include "ovsdb.h"
#include "ovsdb_update.h"
#include "schema.h"
#include "ds.h"
#include "json_util.h"

// ovsdb table api

typedef _Bool schema_from_json_t(void *out, json_t *js, _Bool update, pjs_errmsg_t err);

typedef json_t* schema_to_json_t(void *in, pjs_errmsg_t err);

typedef void schema_mark_changed_t(void *old, void *rec);

typedef struct ovsdb_cache_row
{
    ds_tree_node_t  node; // tree node uuid key
    ds_tree_node_t  node_k; // tree node primary key
    ds_tree_node_t  node_k2; // tree node alternate key2
    int             user_flags;
    char            record[]; // actual values placeholder
} ovsdb_cache_row_t;

typedef void ovsdb_table_callback_t(ovsdb_update_monitor_t *self,
        void *old_rec, void *record);

typedef void ovsdb_cache_callback_t(ovsdb_update_monitor_t *self,
        void *old_rec, void *record, ovsdb_cache_row_t *row);

#define OVSDB_TABLE_KEY_SIZE 64

typedef struct ovsdb_table
{
    char                    *table_name;
    int                     schema_size;
    int                     upd_type_offset;
    int                     uuid_offset;
    int                     version_offset;
    int                     key_offset; // primary key, example: if_name
    char                    key_name[OVSDB_TABLE_KEY_SIZE];
    int                     key2_offset; // optional alternate key2, example: radio_config
    char                    key2_name[OVSDB_TABLE_KEY_SIZE];
    schema_from_json_t      *from_json;
    schema_to_json_t        *to_json;
    schema_mark_changed_t   *mark_changed;
    ovsdb_update_monitor_t  monitor;
    char                    **columns; // all schema columns, null term
    bool                    partial_update;
    ovsdb_update_cbk_t      *monitor_callback;
    ovsdb_table_callback_t  *table_callback;
    // cache:
    ovsdb_cache_callback_t  *cache_callback;
    int                     row_size;
    ds_tree_t               rows; // uuid key
    ds_tree_t               rows_k; // primary key
    ds_tree_t               rows_k2; // alternate key2
} ovsdb_table_t;


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
    char                **columns);

#define OVSDB_TABLE_INIT_NO_KEY(TABLE) \
    ovsdb_table_init( \
        SCHEMA_TABLE(TABLE), \
        &table_ ## TABLE, \
        sizeof(struct schema_ ## TABLE), \
        offsetof(struct schema_ ## TABLE, _update_type), \
        offsetof(struct schema_ ## TABLE, _uuid), \
        offsetof(struct schema_ ## TABLE, _version), \
        (schema_from_json_t*)schema_ ## TABLE ## _from_json, \
        (schema_to_json_t*)schema_ ## TABLE ## _to_json, \
        (schema_mark_changed_t*)schema_ ## TABLE ## _mark_changed, \
        SCHEMA_COLUMNS_ARRAY(TABLE))

// set primary key
#define OVSDB_TABLE_KEY(TABLE, FIELD) \
    do { \
        table_ ## TABLE . key_offset = offsetof(struct schema_ ## TABLE, FIELD); \
        strscpy(table_ ## TABLE . key_name, #FIELD, OVSDB_TABLE_KEY_SIZE); \
    } while (0)

// set secondary key
#define OVSDB_TABLE_KEY2(TABLE, FIELD) \
    do { \
        table_ ## TABLE . key2_offset = offsetof(struct schema_ ## TABLE, FIELD); \
        strscpy(table_ ## TABLE . key2_name, #FIELD, OVSDB_TABLE_KEY_SIZE); \
    } while (0)

// init with key
#define OVSDB_TABLE_INIT(TABLE, FIELD) \
    do { \
        OVSDB_TABLE_INIT_NO_KEY(TABLE); \
        OVSDB_TABLE_KEY(TABLE, FIELD); \
    } while (0)

#define DECL_TABLE_CALLBACK_CAST(TABLE)                         \
    static inline ovsdb_table_callback_t* table_cb_cast_##TABLE(\
            void (*cb)(ovsdb_update_monitor_t *self,            \
                struct schema_##TABLE *old,                     \
                struct schema_##TABLE *record)) {               \
        return (ovsdb_table_callback_t*)(void*)cb;              \
    }

SCHEMA_LISTX(DECL_TABLE_CALLBACK_CAST)

#define OVSDB_TABLE_MONITOR(TABLE, IGN_VER) \
    ovsdb_table_monitor(&table_ ## TABLE, table_cb_cast_##TABLE(callback_ ## TABLE), IGN_VER)

#define OVSDB_TABLE_MONITOR_F(TABLE, FILTER) \
    ovsdb_table_monitor_filter(&table_ ## TABLE, table_cb_cast_##TABLE(callback_ ## TABLE), FILTER)

json_t* ovsdb_table_filter_row(json_t *row, char *columns[]);
bool    ovsdb_table_from_json(ovsdb_table_t *table, json_t *jrow, void *record);
json_t* ovsdb_table_to_json(ovsdb_table_t *table, void *record);
json_t* ovsdb_table_to_json_f(ovsdb_table_t *table, void *record, char *filter[]);
void*   ovsdb_table_select_where(ovsdb_table_t *table, json_t *where, int *count);
void*   ovsdb_table_select(ovsdb_table_t *table, char *column, char *value, int *count);
void*   ovsdb_table_select_typed(ovsdb_table_t *table, char *column, ovsdb_col_t col_type, void *value, int *count);
bool    ovsdb_table_select_one_where(ovsdb_table_t *table, json_t *where, void *record);
bool    ovsdb_table_select_one(ovsdb_table_t *table, const char *column, const char *value, void *record);
bool    ovsdb_table_insert(ovsdb_table_t *table, void *record);
int     ovsdb_table_delete_where(ovsdb_table_t *table, json_t *where);
int     ovsdb_table_delete_simple(ovsdb_table_t *table, const char *column, const char *value);
int     ovsdb_table_delete(ovsdb_table_t *table, void *record);
int     ovsdb_table_update_where_f(ovsdb_table_t *table, json_t *where, void *record, char *filter[]);
int     ovsdb_table_update_where(ovsdb_table_t *table, json_t *where, void *record);
int     ovsdb_table_update_simple_f(ovsdb_table_t *table, char *column, char *value, void *record, char *filter[]);
int     ovsdb_table_update_simple(ovsdb_table_t *table, char *column, char *value, void *record);
int     ovsdb_table_update_f(ovsdb_table_t *table, void *record, char *filter[]);
int     ovsdb_table_update(ovsdb_table_t *table, void *record);
bool    ovsdb_table_upsert_where_f(ovsdb_table_t *table, json_t *where, void *record, bool update_uuid, char *filter[]);
bool    ovsdb_table_upsert_where(ovsdb_table_t *table, json_t *where, void *record, bool update_uuid);
bool    ovsdb_table_upsert_simple_f(ovsdb_table_t *table, char *column, char *value, void *record, bool update_uuid, char *filter[]);
bool    ovsdb_table_upsert_simple(ovsdb_table_t *table, char *column, char *value, void *record, bool update_uuid);
bool    ovsdb_table_upsert_f(ovsdb_table_t *table, void *record, bool update_uuid, char *filter[]);
bool    ovsdb_table_upsert(ovsdb_table_t *table, void *record, bool update_uuid);
int     ovsdb_table_mutate_uuid_set(ovsdb_table_t *table, json_t *where, char *column, ovsdb_tro_t op, char *uuid);
bool    ovsdb_table_upsert_with_parent_where(ovsdb_table_t *table,
                json_t *where, void *record, bool update_uuid, char *filter[],
                char *parent_table, json_t *parent_where, char *parent_column);
bool    ovsdb_table_upsert_with_parent(ovsdb_table_t *table,
                void *record, bool update_uuid, char *filter[],
                char *parent_table, json_t *parent_where, char *parent_column);
int     ovsdb_table_delete_where_with_parent(ovsdb_table_t *table, json_t *where,
        char *parent_table, json_t *parent_where, char *parent_column);

bool ovsdb_table_monitor(ovsdb_table_t *table, ovsdb_table_callback_t *callback, bool ignore_version);
bool ovsdb_table_monitor_columns(ovsdb_table_t *table, ovsdb_table_callback_t *callback, char **columns);
bool ovsdb_table_monitor_filter(ovsdb_table_t *table, ovsdb_table_callback_t *callback, char **filter);

#endif

