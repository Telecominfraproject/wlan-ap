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

#define PJS(name, ...)                                                                  \
bool name ## _from_json(struct name *out, json_t *js, bool update, pjs_errmsg_t err)    \
{                                                                                       \
    __VA_ARGS__                                                                         \
                                                                                        \
    return true;                                                                        \
                                                                                        \
error:                                                                                  \
    return false;                                                                       \
}                                                                                       \
static inline                                                                           \
bool name ## _from_json_cb(void *out, json_t *js, bool update, pjs_errmsg_t err)        \
{                                                                                       \
    return name ## _from_json((struct name *)out, js, update, err);                     \
}
// _from_json_cb is a wrapper for the above _from_json but with 'void *out' to match
// pjs_sub_from_json_cb_t, the wrapper is used for callback pointer type safety
// this way just one parameter is cast instead of the entire function


/*
 * =============================================================
 *  Standard Types
 * =============================================================
 */
#define PJS_INT(name) \
    if (!pjs_int_from_json(&out->name, js, #name, update, err)) goto error;

#define PJS_BOOL(name) \
    if (!pjs_bool_from_json(&out->name, js, #name, update, err)) goto error;

#define PJS_REAL(name) \
    if (!pjs_real_from_json(&out->name, js, #name, update, err)) goto error;

#define PJS_STRING(name, sz)                                                            \
    if (!pjs_string_from_json(out->name, sizeof(out->name), js, #name,                  \
            update, err))                                                               \
        goto error;

#define PJS_SUB(name, sub)                                                              \
{                                                                                       \
    if (!pjs_sub_from_json(                                                             \
            sub ## _from_json_cb,                                                       \
            (void *)&out->name,                                                         \
            sizeof(out->name),                                                          \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;                                                                     \
}

/*
 * =============================================================
 *  Optional Types
 * =============================================================
 */
#define PJS_INT_Q(name)                                                                 \
    if (!pjs_int_q_from_json(&out->name, &out->name ## _exists,                         \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_BOOL_Q(name)                                                                \
    if (!pjs_bool_q_from_json(&out->name, &out->name ## _exists,                        \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_REAL_Q(name)                                                                \
    if (!pjs_real_q_from_json(&out->name, &out->name ## _exists,                        \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_STRING_Q(name, sz)                                                          \
    if (!pjs_string_q_from_json(out->name, sizeof(out->name),                           \
            &out->name ## _exists, js, #name, update, err))                             \
        goto error;

#define PJS_SUB_Q(name, sub)                                                            \
{                                                                                       \
    if (!pjs_sub_q_from_json(                                                           \
            sub ## _from_json_cb,                                                       \
            (void *)&out->name,                                                         \
            sizeof(out->name),                                                          \
            &out->name ## _exists,                                                      \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;                                                                     \
}

/*
 * =============================================================
 *  Non-optional Array Types
 * =============================================================
 */
#define PJS_INT_A(name, sz)                                                             \
    if (!pjs_int_array_from_json(out->name, sz, &out->name ## _len, js,                 \
                #name, update, err))                                                    \
        goto error;

#define PJS_BOOL_A(name, sz)                                                            \
    if (!pjs_bool_array_from_json(out->name, sz, &out->name ## _len,                    \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_REAL_A(name, sz) \
    if (!pjs_real_array_from_json(out->name, sz, &out->name ## _len,                    \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_STRING_A(name, len, sz) \
    if (!pjs_string_array_from_json((char *)out->name, len, sz, &out->name ## _len,     \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_SUB_A(name, sub, sz)  \
    if (!pjs_sub_array_from_json(sub ## _from_json_cb,                                  \
            (void *)out->name, sizeof(out->name[0]), sz, &out->name ## _len,            \
            js, #name, update, err))                                                    \
        goto error;

/*
 * =============================================================
 *  Optional Array Types
 * =============================================================
 */
#define PJS_INT_QA(name, sz)                                                            \
    if (!pjs_int_array_q_from_json(out->name, sz, &out->name ## _len,                   \
                js, #name, update, err))                                                \
        goto error;

#define PJS_BOOL_QA(name, sz)                                                           \
    if (!pjs_bool_array_q_from_json(out->name, sz, &out->name ## _len,                  \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_REAL_QA(name, sz)                                                           \
    if (!pjs_real_array_q_from_json(out->name, sz, &out->name ## _len,                  \
            js, #name, update, err))                                                    \
        goto error;

#define PJS_STRING_QA(name, len, sz)                                                    \
    if (!pjs_string_array_q_from_json((char *)out->name, len, sz,                       \
            &out->name ## _len, js, #name, update, err))                                \
        goto error;

#define PJS_SUB_QA(name, sub, sz)                                                       \
    if (!pjs_sub_array_q_from_json(sub ## _from_json_cb,                                \
            (void *)out->name, sizeof(out->name[0]), &out->name ## _len, js,            \
            #name, update, err))                                                        \
        goto error;

/*
 * ===========================================================================
 *  OVS Basic Types
 * ===========================================================================
 */
#define PJS_OVS_INT(name)                                                               \
    if (!pjs_ovs_int_from_json(                                                         \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_BOOL(name)                                                              \
    if (!pjs_ovs_bool_from_json(                                                        \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_REAL(name)                                                              \
    if (!pjs_ovs_real_from_json(                                                        \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_STRING(name, sz)                                                        \
    if (!pjs_ovs_string_from_json(                                                      \
            out->name,                                                                  \
            sizeof(out->name),                                                          \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_UUID(name)                                                              \
    if (!pjs_ovs_uuid_from_json(                                                        \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

/*
 * ===========================================================================
 *  OVS Basic Optional Types
 * ===========================================================================
 */
#define PJS_OVS_INT_Q(name)                                                             \
    if (!pjs_ovs_int_q_from_json(                                                       \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_BOOL_Q(name)                                                            \
    if (!pjs_ovs_bool_q_from_json(                                                      \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_REAL_Q(name)                                                            \
    if (!pjs_ovs_real_q_from_json(                                                      \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_STRING_Q(name, sz)                                                      \
    if (!pjs_ovs_string_q_from_json(                                                    \
            out->name,                                                                  \
            sizeof(out->name),                                                          \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_UUID_Q(name)                                                            \
    if (!pjs_ovs_uuid_q_from_json(                                                      \
            &out->name,                                                                 \
            &out->name ## _exists,                                                      \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

/*
 * =============================================================
 *  OVS_SET type
 * =============================================================
 */
#define PJS_OVS_SET_INT(name, sz)                                                       \
    if (!pjs_ovs_set_int_from_json(                                                     \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_BOOL(name, sz)                                                      \
    if (!pjs_ovs_set_bool_from_json(                                                    \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_REAL(name, sz)                                                      \
    if (!pjs_ovs_set_real_from_json(                                                    \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_STRING(name, namesz, sz)                                            \
    if (!pjs_ovs_set_string_from_json(                                                  \
            (char *)out->name,                                                          \
            namesz,                                                                     \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_UUID(name, sz)                                                      \
    if (!pjs_ovs_set_uuid_from_json(                                                    \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

/*
 * =============================================================
 *  OVS_SMAP type
 * =============================================================
 */
#define PJS_OVS_SMAP_INT(name, sz)                                                      \
    if (!pjs_ovs_smap_int_from_json(                                                    \
            (char *)out->name ## _keys,                                                 \
            PJS_OVS_MAP_KEYSZ,                                                          \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_BOOL(name, sz)                                                     \
    if (!pjs_ovs_smap_bool_from_json(                                                   \
            (char *)out->name ## _keys,                                                 \
            PJS_OVS_MAP_KEYSZ,                                                          \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_REAL(name, sz)                                                     \
    if (!pjs_ovs_smap_real_from_json(                                                   \
            (char *)out->name ## _keys,                                                 \
            PJS_OVS_MAP_KEYSZ,                                                          \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_STRING(name, len, sz)                                              \
    if (!pjs_ovs_smap_string_from_json(                                                 \
            (char *)out->name ## _keys,                                                 \
            PJS_OVS_MAP_KEYSZ,                                                          \
            (char *)out->name,                                                          \
            len,                                                                        \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_UUID(name, sz)                                                     \
    if (!pjs_ovs_smap_uuid_from_json(                                                   \
            (char *)out->name ## _keys,                                                 \
            PJS_OVS_MAP_KEYSZ,                                                          \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

/*
 * =============================================================
 *  OVS_DMAP type
 * =============================================================
 */
#define PJS_OVS_DMAP_INT(name, sz)                                                      \
    if (!pjs_ovs_dmap_int_from_json(                                                    \
            out->name ## _keys,                                                         \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_BOOL(name, sz)                                                     \
    if (!pjs_ovs_dmap_bool_from_json(                                                   \
            out->name ## _keys,                                                         \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_REAL(name, sz)                                                     \
    if (!pjs_ovs_dmap_real_from_json(                                                   \
            out->name ## _keys,                                                         \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_STRING(name, len, sz)                                              \
    if (!pjs_ovs_dmap_string_from_json(                                                 \
            out->name ## _keys,                                                         \
            (char *)out->name,                                                          \
            len,                                                                        \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_UUID(name, sz)                                                     \
    if (!pjs_ovs_dmap_uuid_from_json(                                                   \
            out->name ## _keys,                                                         \
            out->name,                                                                  \
            sz,                                                                         \
            &out->name ## _len,                                                         \
            &out->name ## _present,                                                     \
            js,                                                                         \
            #name,                                                                      \
            update,                                                                     \
            err))                                                                       \
        goto error;
