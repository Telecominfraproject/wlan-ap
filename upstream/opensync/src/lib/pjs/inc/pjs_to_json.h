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

#include "pjs_undef.h"
#include "pjs_common.h"

#define PJS(name, ...) json_t *name ## _to_json(struct name *in, pjs_errmsg_t err)      \
{                                                                                       \
    json_t *js = NULL;                                                                  \
                                                                                        \
    js = json_object();                                                                 \
    if (js == NULL)                                                                     \
    {                                                                                   \
        PJS_ERR(err, "'%s': Unable to allocate root object.", #name);                   \
        goto error;                                                                     \
    }                                                                                   \
    if (in->_partial_update) {                                                          \
        LOG(TRACE, "%s to json: partial update", #name);                                \
    }                                                                                   \
                                                                                        \
    __VA_ARGS__                                                                         \
                                                                                        \
    return js;                                                                          \
                                                                                        \
error:                                                                                  \
    if (js != NULL) json_decref(js);                                                    \
    return NULL;                                                                        \
}

/*
 * =============================================================
 *  Standard Types
 * =============================================================
 */
#define PJS_INT(name)                                                                   \
    if (!pjs_int_to_json(in->name, js, #name, err)) goto error;

#define PJS_BOOL(name)                                                                  \
    if (!pjs_bool_to_json(in->name, js, #name, err)) goto error;

#define PJS_REAL(name)                                                                  \
    if (!pjs_real_to_json(in->name, js, #name, err)) goto error;

#define PJS_STRING(name, sz)                                                            \
    if (!pjs_string_to_json(in->name, sizeof(in->name), js, #name, err)) goto error;

#define PJS_SUB(name, sub)                                                              \
{                                                                                       \
    if (!pjs_sub_to_json(                                                               \
            (pjs_sub_to_json_cb_t *)sub ## _to_json,                                    \
            (void *)&in->name,                                                          \
            sizeof(in->name),                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;                                                                     \
}

/*
 * =============================================================
 *  Optional Types
 * =============================================================
 */
#define PJS_INT_Q(name)                                                                 \
    if (!pjs_int_q_to_json(in->name, in->name ## _exists, js, #name, err)) goto error;

#define PJS_BOOL_Q(name)                                                                \
    if (!pjs_bool_q_to_json(in->name, in->name ## _exists, js, #name, err)) goto error;

#define PJS_REAL_Q(name)                                                                \
    if (!pjs_real_q_to_json(in->name, in->name ## _exists, js, #name, err)) goto error;

#define PJS_STRING_Q(name, sz)                                                          \
    if (!pjs_string_q_to_json(                                                          \
            in->name,                                                                   \
            sizeof(in->name),                                                           \
            in->name ## _exists,                                                        \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
    goto error;

#define PJS_SUB_Q(name, sub)                                                            \
{                                                                                       \
    if (!pjs_sub_q_to_json(                                                             \
            (pjs_sub_to_json_cb_t *)sub ## _to_json,                                    \
            (void *)&in->name,                                                          \
            sizeof(in->name),                                                           \
            in->name ## _exists,                                                        \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;                                                                     \
}

/*
 * =============================================================
 *  Array Types
 * =============================================================
 */
#define PJS_INT_A(name, sz)                                                             \
    if (!pjs_int_array_to_json(in->name, in->name ## _len, js, #name, err))             \
        goto error;

#define PJS_BOOL_A(name, sz)                                                            \
    if (!pjs_bool_array_to_json(in->name, in->name ## _len, js, #name, err))            \
        goto error;

#define PJS_REAL_A(name, sz)                                                            \
    if (!pjs_real_array_to_json(in->name, in->name ## _len, js, #name, err))            \
        goto error;

#define PJS_STRING_A(name, len, sz)                                                     \
    if (!pjs_string_array_to_json(                                                      \
            (char *)in->name,                                                           \
            len,                                                                        \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_SUB_A(name, sub, sz)                                                        \
    if (!pjs_sub_array_to_json(                                                         \
            (pjs_sub_to_json_cb_t *)sub ## _to_json,                                    \
            (void *)in->name,                                                           \
            sizeof(in->name[0]),                                                        \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

/*
 * =============================================================
 *  Optional Array Types
 * =============================================================
 */
#define PJS_INT_QA(name, sz)                                                            \
    if (!pjs_int_array_q_to_json(in->name, in->name ## _len, js, #name, err))           \
        goto error;

#define PJS_BOOL_QA(name, sz)                                                           \
    if (!pjs_bool_array_q_to_json(in->name, in->name ## _len, js, #name, err))          \
        goto error;

#define PJS_REAL_QA(name, sz)                                                           \
    if (!pjs_real_array_q_to_json(in->name, in->name ## _len, js, #name, err))          \
        goto error;

#define PJS_STRING_QA(name, len, sz)                                                    \
    if (!pjs_string_array_q_to_json(                                                    \
            (char *)in->name,                                                           \
            len,                                                                        \
            in->name ## _len,                                                           \
            js, #name,                                                                  \
            err))                                                                       \
        goto error;

#define PJS_SUB_QA(name, sub, sz)                                                       \
    if (!pjs_sub_array_q_to_json(                                                       \
            (pjs_sub_to_json_cb_t *)sub ## _to_json,                                    \
            (void *)in->name,                                                           \
            sizeof(in->name[0]),                                                        \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

/*
 * ===========================================================================
 *  OVS Basic Types
 * ===========================================================================
 */

#define PJS_OVS_IF_PARTIAL_UPDATE(name) \
    if (!in->_partial_update || in->name ## _present)

#define PJS_OVS_INT(name)                                                               \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_int_to_json(in->name, js, #name, err)) goto error;

#define PJS_OVS_BOOL(name)                                                              \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_bool_to_json(in->name, js, #name, err)) goto error;

#define PJS_OVS_REAL(name)                                                              \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_real_to_json(in->name, js, #name, err)) goto error;

#define PJS_OVS_STRING(name, sz)                                                        \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_string_to_json(in->name, sizeof(in->name), js, #name, err)) goto error;

#define PJS_OVS_UUID(name)                                                              \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_uuid_to_json(&in->name, js, #name, err)) goto error;

/*
 * ===========================================================================
 *  OVS Basic Optional Types
 * ===========================================================================
 */
#define PJS_OVS_INT_Q(name)                                                             \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_int_q_to_json(                                                         \
            in->name,                                                                   \
            in->name ## _exists,                                                        \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_BOOL_Q(name)                                                            \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_bool_q_to_json(                                                        \
            in->name,                                                                   \
            in->name ## _exists,                                                        \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_REAL_Q(name)                                                            \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_real_q_to_json(                                                        \
            in->name,                                                                   \
            in->name ## _exists,                                                        \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_STRING_Q(name, sz)                                                      \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_string_q_to_json(                                                      \
            in->name,                                                                   \
            sizeof(in->name),                                                           \
            in->name ## _exists,                                                        \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_UUID_Q(name)                                                            \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_uuid_q_to_json(                                                        \
            &in->name,                                                                  \
            in->name ## _exists,                                                        \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;                                                                     \


/*
 * =============================================================
 *  OVS_SET
 * =============================================================
 */
#define PJS_OVS_SET_INT(name, sz)                                                       \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_set_int_to_json(                                                       \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_BOOL(name, sz)                                                      \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_set_bool_to_json(                                                      \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_REAL(name, sz)                                                      \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_set_real_to_json(                                                      \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_STRING(name, len, sz)                                               \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_set_string_to_json(                                                    \
            (char *)in->name,                                                           \
            len,                                                                        \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SET_UUID(name, sz)                                                      \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_set_uuid_to_json(                                                      \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

/*
 * =============================================================
 *  OVS_SMAP is a type where the key is a STRING
 * =============================================================
 */
#define PJS_OVS_SMAP_INT(name, sz)                                                      \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_smap_int_to_json(                                                      \
            (char *)in->name ## _keys,                                                  \
            sizeof(in->name ## _keys[0]),                                               \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_BOOL(name, sz)                                                     \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_smap_bool_to_json(                                                     \
            (char *)in->name ## _keys,                                                  \
            sizeof(in->name ## _keys[0]),                                               \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_REAL(name, sz)                                                     \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_smap_real_to_json(                                                     \
            (char *)in->name ## _keys,                                                  \
            sizeof(in->name ## _keys[0]),                                               \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_STRING(name, len, sz)                                              \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_smap_string_to_json(                                                   \
            (char *)in->name ## _keys,                                                  \
            sizeof(in->name ## _keys[0]),                                               \
            (char *)in->name,                                                           \
            sizeof(in->name[0]),                                                        \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_SMAP_UUID(name, sz)                                                     \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_smap_uuid_to_json(                                                     \
            (char *)in->name ## _keys,                                                  \
            sizeof(in->name ## _keys[0]),                                               \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

/*
 * =================================================================
 *  OVS_DMAP is a type where the key is an INTEGER
 * =================================================================
 */
#define PJS_OVS_DMAP_INT(name, sz)                                                      \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_dmap_int_to_json(                                                      \
            in->name ## _keys,                                                          \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_BOOL(name, sz)                                                     \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_dmap_bool_to_json(                                                     \
            in->name ## _keys,                                                          \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_REAL(name, sz)                                                     \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_dmap_real_to_json(                                                     \
            in->name ## _keys,                                                          \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_STRING(name, len, sz)                                              \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_dmap_string_to_json(in->name ## _keys,                                 \
            (char *)in->name,                                                           \
            sizeof(in->name[0]),                                                        \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;

#define PJS_OVS_DMAP_UUID(name, sz)                                                     \
    PJS_OVS_IF_PARTIAL_UPDATE(name)                                                     \
    if (!pjs_ovs_dmap_uuid_to_json(                                                     \
            in->name ## _keys,                                                          \
            in->name,                                                                   \
            in->name ## _len,                                                           \
            js,                                                                         \
            #name,                                                                      \
            err))                                                                       \
        goto error;
