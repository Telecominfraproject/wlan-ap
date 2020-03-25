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

/**
 * This file is used to generate pjs structures from PJS_* macros
 */
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <jansson.h>

#include "pjs_common.h"
#include "pjs_undef.h"

// _update_type is the ovsdb monitor update type: ovsdb_update_type_t (NEW,MODIFY,DELETE)
// using int to minimize dependanciess

#define PJS(name, ...)                                                              \
struct name                                                                         \
{                                                                                   \
    int  _update_type;                                                              \
    bool _partial_update;                                                           \
    __VA_ARGS__                                                                     \
};                                                                                  \
                                                                                    \
extern bool name ## _from_json(                                                     \
        struct name *out,                                                           \
        json_t *js,                                                                 \
        bool update,                                                                \
        pjs_errmsg_t err);                                                          \
                                                                                    \
extern json_t *name ## _to_json(                                                    \
        struct name *in,                                                            \
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Standard types
 * =============================================================
 */
#define PJS_INT(name)                       int name;
#define PJS_BOOL(name)                      bool name;
#define PJS_REAL(name)                      double name;
#define PJS_STRING(name, len)               char name[len];
#define PJS_SUB(name, sub)                  struct sub name;

/*
 * =============================================================
 *  Optional types
 * =============================================================
 */
#define PJS_INT_Q(name)                     int name;               bool name ## _exists;
#define PJS_BOOL_Q(name)                    bool name;              bool name ## _exists;
#define PJS_STRING_Q(name, sz)              char name[sz];          bool name ## _exists;
#define PJS_REAL_Q(name)                    double name;            bool name ## _exists;
#define PJS_SUB_Q(name, sub)                struct sub name;        bool name ## _exists;

/*
 * =============================================================
 *  Array types
 * =============================================================
 */
#define PJS_INT_A(name, sz)                 int name[sz];           int name ## _len;
#define PJS_BOOL_A(name, sz)                bool name[sz];          int name ## _len;
#define PJS_STRING_A(name, len, sz)         char name[sz][len];     int name ## _len;
#define PJS_REAL_A(name, sz)                double name[sz];        int name ## _len;
#define PJS_SUB_A(name, sub, sz)            struct sub name[sz];    int name ## _len;

/*
 * =============================================================
 *  Optional Array types
 * =============================================================
 */
#define PJS_INT_QA(name, sz)                int name[sz];           int name ## _len;
#define PJS_BOOL_QA(name, sz)               bool name[sz];          int name ## _len;
#define PJS_STRING_QA(name, len, sz)        char name[sz][len];     int name ## _len;
#define PJS_REAL_QA(name, sz)               double name[sz];        int name ## _len;
#define PJS_SUB_QA(name, sub, sz)           struct sub name[sz];    int name ## _len;

/*
 * =============================================================
 *  OVS Common flags
 * =============================================================
 */
#define PJS_OVS_COMMON(name)                bool name ## _present; bool name ## _changed;
#define PJS_OVS_COMMON_EXISTS(name)         PJS_OVS_COMMON(name) bool name ## _exists;

/*
 * =============================================================
 *  OVS Basic Types
 * =============================================================
 */
#define PJS_OVS_INT(name)                   int name;               PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_BOOL(name)                  bool name;              PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_REAL(name)                  double name;            PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_STRING(name, len)           char name[len];         PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_UUID(name)                  ovs_uuid_t name;        PJS_OVS_COMMON_EXISTS(name)

/*
 * =============================================================
 *  OVS Basic Optional Types
 * =============================================================
 */
#define PJS_OVS_INT_Q(name)                 int name;               PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_BOOL_Q(name)                bool name;              PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_STRING_Q(name, sz)          char name[sz];          PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_REAL_Q(name)                double name;            PJS_OVS_COMMON_EXISTS(name)
#define PJS_OVS_UUID_Q(name)                ovs_uuid_t name;        PJS_OVS_COMMON_EXISTS(name)

/*
 * =============================================================
 * OVSDB Set(array)
 * =============================================================
 */
#define PJS_STRUCT_OVS_SET(name, sz, type)      type;                                          \
                                                int name ## _len;                              \
                                                PJS_OVS_COMMON(name)

#define PJS_OVS_SET_INT(name, sz)               PJS_STRUCT_OVS_SET(name, sz, int name[sz])
#define PJS_OVS_SET_BOOL(name, sz)              PJS_STRUCT_OVS_SET(name, sz, bool name[sz])
#define PJS_OVS_SET_REAL(name, sz)              PJS_STRUCT_OVS_SET(name, sz, double name[sz])
#define PJS_OVS_SET_STRING(name, len, sz)       PJS_STRUCT_OVS_SET(name, sz, char name[sz][len])
#define PJS_OVS_SET_UUID(name, sz)              PJS_STRUCT_OVS_SET(name, sz, ovs_uuid_t name[sz])


/*
 * =============================================================
 * OVSDB Map(dictionary), where the key is a string
 * =============================================================
 */
#define PJS_STRUCT_OVS_SMAP(name, sz, type)     type;                                           \
                                                char name ## _keys[sz][PJS_OVS_MAP_KEYSZ];      \
                                                int name ## _len;                               \
                                                PJS_OVS_COMMON(name)

#define PJS_OVS_SMAP_INT(name, sz)              PJS_STRUCT_OVS_SMAP(name, sz, int name[sz])
#define PJS_OVS_SMAP_BOOL(name, sz)             PJS_STRUCT_OVS_SMAP(name, sz, bool name[sz])
#define PJS_OVS_SMAP_REAL(name, sz)             PJS_STRUCT_OVS_SMAP(name, sz, double name[sz])
#define PJS_OVS_SMAP_STRING(name, len, sz)      PJS_STRUCT_OVS_SMAP(name, sz, char name[sz][len])
#define PJS_OVS_SMAP_UUID(name, sz)             PJS_STRUCT_OVS_SMAP(name, sz, ovs_uuid_t name[sz])

/*
 * =============================================================
 * OVSDB Map(dictionary), where the key is an int
 * =============================================================
 */
#define PJS_STRUCT_OVS_DMAP(name, sz, type)     type;                                           \
                                                int name ## _keys[sz];                          \
                                                int name ## _len;                               \
                                                PJS_OVS_COMMON(name)

#define PJS_OVS_DMAP_INT(name, sz)              PJS_STRUCT_OVS_DMAP(name, sz, int name[sz])
#define PJS_OVS_DMAP_BOOL(name, sz)             PJS_STRUCT_OVS_DMAP(name, sz, bool name[sz])
#define PJS_OVS_DMAP_REAL(name, sz)             PJS_STRUCT_OVS_DMAP(name, sz, double name[sz])
#define PJS_OVS_DMAP_STRING(name, len, sz)      PJS_STRUCT_OVS_DMAP(name, sz, char name[sz][len])
#define PJS_OVS_DMAP_UUID(name, sz)             PJS_STRUCT_OVS_DMAP(name, sz, ovs_uuid_t name[sz])
