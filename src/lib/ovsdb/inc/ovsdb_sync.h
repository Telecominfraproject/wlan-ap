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

#ifndef OVSDB_SYNC_H_INCLUDED
#define OVSDB_SYNC_H_INCLUDED

#include <stdarg.h>
#include <stdbool.h>
#include <jansson.h>

#include "ovsdb.h"

// ovsdb sync api

json_t* ovsdb_where_simple(const char *column, const char *value);
json_t* ovsdb_where_simple_typed(const char *column, const void *value, ovsdb_col_t col_type);
json_t* ovsdb_where_uuid(const char *column, const char *uuid);
json_t* ovsdb_where_multi(json_t *where, ...);
json_t* ovsdb_mutation(const char *column, json_t *mutation, json_t *value);
int     ovsdb_get_update_result_count(json_t *result, const char *table, const char *oper);
bool    ovsdb_get_insert_result_uuid(json_t *result, const char *table, const char *oper, ovs_uuid_t *uuid);
json_t* ovsdb_sync_select_where(const char *table, json_t *where);
json_t* ovsdb_sync_select(const char *table, const char *column, const char *value);
bool    ovsdb_sync_insert(const char *table, json_t *row, ovs_uuid_t *uuid);
int     ovsdb_sync_delete_where(const char *table, json_t *where);
int     ovsdb_sync_update_where(const char *table, json_t *where, json_t *row);
int     ovsdb_sync_update(const char *table, const char *column, const char *value, json_t *row);
int     ovsdb_sync_update_one_get_uuid(const char *table, json_t *where, json_t *row, ovs_uuid_t *uuid);
bool    ovsdb_sync_upsert_where(const char *table, json_t *where, json_t *row, ovs_uuid_t *uuid);
bool    ovsdb_sync_upsert(const char *table, const char *column, const char *value, json_t *row, ovs_uuid_t *uuid);
int     ovsdb_sync_mutate_uuid_set(const char *table, json_t *where, const char *column, ovsdb_tro_t op, const char *uuid);
bool    ovsdb_sync_insert_with_parent(const char *table, json_t *row, ovs_uuid_t *uuid,
        const char *parent_table, json_t *parent_where, const char *parent_column);
bool    ovsdb_sync_upsert_with_parent(const char *table, json_t *where, json_t *row, ovs_uuid_t *uuid,
        const char *parent_table, json_t *parent_where, const char *parent_column);
int     ovsdb_sync_delete_with_parent(const char *table, json_t *where,
        const char *parent_table, json_t *parent_where, const char *parent_column);

#endif

