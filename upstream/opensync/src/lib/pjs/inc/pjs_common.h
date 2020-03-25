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

#ifndef PJS_COMMON_H_INCLUDED
#define PJS_COMMON_H_INCLUDED

#include <stdlib.h>
#include <stdbool.h>
#include <jansson.h>

#define PJS_OVS_MAP_KEYSZ           64      /* Default KEY size for OVS MAPs */
#define PJS_OVS_UUID_SZ             37      /* Max OVS UUID size */

#define PJS_STR(x)                  #x

#define PJS_ERR(err, fmt, ...)                                      \
do                                                                  \
{                                                                   \
    if (err == NULL) break;                                         \
    snprintf(err, sizeof(pjs_errmsg_t), fmt, ##__VA_ARGS__);        \
} while (0)

typedef struct
{
    char uuid[PJS_OVS_UUID_SZ];
} ovs_uuid_t;

typedef char pjs_errmsg_t[128];

typedef bool pjs_sub_from_json_cb_t(void *data, json_t *jsval, bool update, pjs_errmsg_t err);
typedef json_t *pjs_sub_to_json_cb_t(void *data, pjs_errmsg_t err);

/*
 * =============================================================
 *  BASIC types
 * =============================================================
 */
extern bool pjs_int_q_from_json(
        int *out,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_int_from_json(
        int *out, json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_int_q_to_json(
        int in,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_int_to_json(
        int in,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_bool_q_from_json(
        bool *out,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_bool_from_json(
        bool *out,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_bool_q_to_json(
        bool in,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_bool_to_json(
        bool in,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_real_q_from_json(
        double *out,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_real_from_json(
        double *out,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_real_q_to_json(
        double in,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_real_to_json(
        double in,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_string_q_from_json(
        char *out,
        size_t outsz,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_string_from_json(
        char *out,
        size_t outsz,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_string_q_to_json(
        char *in,
        size_t insz,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_string_to_json(
        char *in,
        size_t insz,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_sub_q_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_sub_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_sub_q_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_sub_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);


/*
 * =============================================================
 *  INTEGER arrays
 * =============================================================
 */
extern bool pjs_int_array_q_from_json(
        int *out_data,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_int_array_from_json(
        int *out_data,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_int_array_q_to_json(
        int *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_int_array_to_json(
        int *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  BOOLEAN arrays
 * =============================================================
 */
extern bool pjs_bool_array_q_from_json(
        bool *out_data,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_bool_array_from_json(
        bool *out_data,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_bool_array_q_to_json(
        bool *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_bool_array_to_json(
        bool *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);
/*
 * =============================================================
 *  REAL arrays
 * =============================================================
 */
extern bool pjs_real_array_q_from_json(
        double *out_data,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_real_array_from_json(
        double *out_data,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_real_array_q_to_json(
        double *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_real_array_to_json(
        double *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);
/*
 * =============================================================
 *  STRING arrays
 * =============================================================
 */
extern bool pjs_string_array_q_from_json(
        char *out_data,
        size_t out_sz,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_string_array_from_json(
        char *out_data,
        size_t out_sz,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_string_array_to_json(
        char *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_string_array_q_to_json(
        char *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  SUB arrays
 * =============================================================
 */
extern bool pjs_sub_array_q_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_sub_array_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        int out_max,
        int *out_len,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_sub_array_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_sub_array_q_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * ===========================================================================
 *  Basic OVS types
 * ===========================================================================
 */
extern bool pjs_ovs_int_q_from_json(
        int *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_int_from_json(
        int *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_int_q_to_json(
        int in,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_int_to_json(
        int in,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_bool_q_from_json(
        bool *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_bool_from_json(
        bool *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_bool_q_to_json(
        bool in,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_bool_to_json(
        bool in,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_real_q_from_json(
        double *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_real_from_json(
        double *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_real_q_to_json(
        double in,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_real_to_json(
        double in,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_string_q_from_json(
        char *out,
        size_t outsz,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_string_from_json(
        char *out,
        size_t outsz,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_string_q_to_json(
        char *in,
        size_t insz,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_string_to_json(
        char *in,
        size_t insz,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_uuid_q_from_json(
        ovs_uuid_t *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_uuid_from_json(
        ovs_uuid_t *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_uuid_q_to_json(
        ovs_uuid_t *in,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

extern bool pjs_ovs_uuid_to_json(
        ovs_uuid_t *in,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);


/*
 * =============================================================
 *  Integer OVS_SET
 * =============================================================
 */
extern bool pjs_ovs_set_int_from_json(
        int *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_set_int_to_json(
        int *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Boolean OVS_SET
 * =============================================================
 */
extern bool pjs_ovs_set_bool_from_json(
        bool *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_set_bool_to_json(
        bool *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Real OVS_SET
 * =============================================================
 */
extern bool pjs_ovs_set_real_from_json(
        double *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_set_real_to_json(
        double *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  String OVS_SET
 * =============================================================
 */
extern bool pjs_ovs_set_string_from_json(
        char *out_data,
        size_t out_sz,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_set_string_to_json(
        char *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Uuid OVS_SET
 * =============================================================
 */
extern bool pjs_ovs_set_uuid_from_json(
        ovs_uuid_t *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_set_uuid_to_json(
        ovs_uuid_t *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Integer OVS_SMAP
 * =============================================================
 */
extern bool pjs_ovs_smap_int_from_json(
        char *keys,
        size_t key_sz,
        int *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_smap_int_to_json(
        char *keys,
        size_t key_sz,
        int *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Boolean OVS_SMAP
 * =============================================================
 */
extern bool pjs_ovs_smap_bool_from_json(
        char *keys,
        int key_sz,
        bool *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_smap_bool_to_json(
        char *keys,
        size_t key_sz,
        bool *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Real OVS_SMAP
 * =============================================================
 */
extern bool pjs_ovs_smap_real_from_json(
        char *keys,
        size_t key_sz,
        double *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_smap_real_to_json(
        char *keys,
        size_t key_sz,
        double *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  String OVS_SMAP
 * =============================================================
 */
extern bool pjs_ovs_smap_string_from_json(
        char *keys,
        int key_sz,
        char *out_data,
        int out_sz,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_smap_string_to_json(
        char *keys,
        size_t key_sz,
        char *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Uuid OVS_SMAP
 * =============================================================
 */
extern bool pjs_ovs_smap_uuid_from_json(
        char *keys,
        size_t key_sz,
        ovs_uuid_t *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_smap_uuid_to_json(
        char *keys,
        size_t key_sz,
        ovs_uuid_t *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Integer OVS_DMAP
 * =============================================================
 */
extern bool pjs_ovs_dmap_int_from_json(
        int *keys,
        int *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_dmap_int_to_json(
        int *keys,
        int *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);
/*
 * =============================================================
 *  Boolean OVS_DMAP
 * =============================================================
 */
extern bool pjs_ovs_dmap_bool_from_json(
        int *keys,
        bool *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_dmap_bool_to_json(
        int *keys,
        bool *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Real OVS_DMAP
 * =============================================================
 */
extern bool pjs_ovs_dmap_real_from_json(
        int *keys,
        double *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_dmap_real_to_json(
        int *keys,
        double *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  String OVS_DMAP
 * =============================================================
 */
extern bool pjs_ovs_dmap_string_from_json(
        int *keys,
        char *out_data,
        int out_sz,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_dmap_string_to_json(
        int *keys,
        char *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

/*
 * =============================================================
 *  Real OVS_DMAP
 * =============================================================
 */
extern bool pjs_ovs_dmap_uuid_from_json(
        int *keys,
        ovs_uuid_t *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_dmap_uuid_to_json(
        int *keys,
        ovs_uuid_t *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);

#endif /* PJS_COMMON_H_INCLUDED */
