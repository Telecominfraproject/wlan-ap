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

#include <string.h>

#include "pjs_types.h"

/*
 * =============================================================
 *  Generic OVS_MAP handler
 * =============================================================
 */

/**
 * Generic OVS_MAP _from_json handler
 */
bool pjs_ovs_map_from_json(
        pjs_type_from_json_t *key_cb,
        void *key_data,
        pjs_type_from_json_t *out_cb,
        void *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    json_t *jsmap;
    json_t *jsa;
    json_t *jstuple;
    const char *pstr;
    size_t ii;

    /* No such object, return */
    jsmap = json_object_get(js, name);
    if (jsmap == NULL || json_is_null(jsmap))
    {
        /*
         * If object doesn't exists, and we're NOT in update mode, return an error (false).
         * If we're in update mode, just return true, but do not touch any of the output values.
         */
        if (update) {
            return true;
        }
        PJS_ERR(err, "OVS_MAP: Object '%s' does not exist.", name);
        return false;
    }

    // mark presence
    *present = true;

    if (!json_is_array(jsmap))
    {
        PJS_ERR(err, "OVS_MAP: Object '%s' not an array type.", name);
        return false;
    }

    if (json_array_size(jsmap) != 2)
    {
        PJS_ERR(err, "OVS_MAP: Object '%s' must contain exactly 2 elements.", name);
        return false;
    }

    jsa = json_array_get(jsmap, 1);
    if (jsa == NULL || !json_is_array(jsa))
    {
        PJS_ERR(err, "OVS_MAP: Object '%s' second element is not an array type.", name);
        return false;
    }

    if (!json_is_string(json_array_get(jsmap, 0)))
    {
        PJS_ERR(err, "OVS_MAP: Object '%s' first element is not a string type.", name);
        return false;
    }

    pstr = json_string_value(json_array_get(jsmap, 0));
    if (pstr == NULL)
    {
        PJS_ERR(err, "OVS_MAP: Object '%s' first element is not a string.", name);
        return false;
    }

    if (strcmp(pstr, "map") != 0)
    {
        PJS_ERR(err, "OVS_MAP: Object '%s' first element is not 'map'.", name);
        return false;
    }

    if (out_max < (int)json_array_size(jsa))
    {
        PJS_ERR(err, "OVS_MAP: Object '%s'  map size too big. Max %d.", name, out_max);
        return false;
    }

    json_array_foreach(jsa, ii, jstuple)
    {
        if (!json_is_array(jstuple))
        {
            PJS_ERR(err, "OVS_MAP: Object '%s' doesn't contain array tuples.", name);
            return false;
        }

        json_t *jskey = json_array_get(jstuple, 0);
        json_t *jsdata = json_array_get(jstuple, 1);

        /* Parse and store the key */
        if (!key_cb(key_data, ii, jskey))
        {
            PJS_ERR(err, "'%s' key cannot convert to JSON.", name);
            return false;
        }

        /* Parse and store the data */
        if (!out_cb(out_data, ii, jsdata))
        {
            PJS_ERR(err, "'%s' type cannot convert to JSON.", name);
            return false;
        }
    }

    *out_len = ii;

    return true;
}

/**
 * Generic OVS_MAP _to_json handler
 */
bool pjs_ovs_map_to_json(
        pjs_type_to_json_t *key_cb,
        void *key_data,
        pjs_type_to_json_t *in_cb,
        void *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    int ii;

    json_t *jsmap;
    json_t *jsarr;

    jsmap = json_array();
    if (jsmap == NULL)
    {
        PJS_ERR(err, "'%s' OVS_MAP unable to create map object.", name);
        goto error;
    }

    if (json_array_append_new(jsmap, json_string("map")) != 0)
    {
        PJS_ERR(err, "'%s' OVS_MAP unable to append 'map' constant.", name);
        goto error;
    }

    jsarr = json_array();
    if (jsarr == NULL)
    {
        PJS_ERR(err, "'%s' OVS_MAP cannot create data array object.", name);
        goto error;
    }

    if (json_array_append_new(jsmap, jsarr) != 0)
    {
        PJS_ERR(err, "'%s' cannot append tuple array to map.", name);
        json_decref(jsarr);
        goto error;
    }

    for (ii = 0; ii < in_num; ii++)
    {
        json_t *jstuple;

        jstuple = json_array();
        if (jstuple == NULL)
        {
            PJS_ERR(err, "'%s' OVS_MAP error creating tuple.", name);
            goto error;
        }

        /* Append the tuple to the map */
        if (json_array_append_new(jsarr, jstuple) != 0)
        {
            PJS_ERR(err, "'%s' OVS_MAP error appending tuple.", name);
            json_decref(jstuple);
            goto error;
        }

        /*
         * Populate the tuple
         */
        if (json_array_append_new(jstuple, key_cb(key_data, ii)) != 0)
        {
            PJS_ERR(err, "'%s' OVS_MAP error appending tuple key.", name);
            goto error;
        }

        if (json_array_append_new(jstuple, in_cb(in_data, ii)) != 0)
        {
            PJS_ERR(err, "'%s' OVS_MAP error appending tuple data.", name);
            goto error;
        }
    }

    /* Insert it as soon as possible so the error handler frees it */
    if (json_object_set_new(js, name, jsmap) != 0)
    {
        PJS_ERR(err, "'%s' OVS_MAP error inserting to root object.", name);
        goto error;
    }

    return true;

error:
    if (jsmap != NULL) json_decref(jsmap);

    return false;
}

/*
 * =============================================================
 *  Integer OVS_SMAP handler
 * =============================================================
 */

/**
 * INTEGER OVS_SMAP _from_json handler
 */
bool pjs_ovs_smap_int_from_json(
        char *keys,
        size_t key_sz,
        int *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_from_json(
            pjs_string_t_from_json,
            &kargs,
            pjs_int_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * INTEGER OVS_SMAP _to_json handler
 */
bool pjs_ovs_smap_int_to_json(
        char *keys,
        size_t key_sz,
        int *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_to_json(
            pjs_string_t_to_json,
            &kargs,
            pjs_int_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  BOOLEAN OVS_SMAP handler
 * =============================================================
 */

/**
 * BOOLEAN OVS_SMAP handler
 */
bool pjs_ovs_smap_bool_from_json(
        char *keys,
        int key_sz,
        bool *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_from_json(
            pjs_string_t_from_json,
            &kargs,
            pjs_bool_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * BOOLEAN OVS_SMAP _to_json handler
 */
bool pjs_ovs_smap_bool_to_json(
        char *keys,
        size_t key_sz,
        bool *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_to_json(
            pjs_string_t_to_json,
            &kargs,
            pjs_bool_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  REAL OVS_SMAP handler
 * =============================================================
 */

/**
 * REAL OVS_SMAP handler
 */
bool pjs_ovs_smap_real_from_json(
        char *keys,
        size_t key_sz,
        double *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_from_json(
            pjs_string_t_from_json,
            &kargs,
            pjs_real_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}


/**
 * REAL OVS_SMAP _to_json handler
 */
bool pjs_ovs_smap_real_to_json(
        char *keys,
        size_t key_sz,
        double *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_to_json(
            pjs_string_t_to_json,
            &kargs,
            pjs_real_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}
/*
 * =============================================================
 *  String OVS_SMAP handler
 * =============================================================
 */

/**
 * String OVS_MAP handler
 */
bool pjs_ovs_smap_string_from_json(
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
        pjs_errmsg_t err)
{
    struct pjs_string_args sargs;
    struct pjs_string_args kargs;

    sargs.data = out_data;
    sargs.sz = out_sz;
    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_from_json(
            pjs_string_t_from_json,
            &kargs,
            pjs_string_t_from_json,
            &sargs,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}


/**
 * String OVS_SMAP _to_json handler
 */
bool pjs_ovs_smap_string_to_json(
        char *keys,
        size_t key_sz,
        char *in_data,
        size_t in_sz,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;
    struct pjs_string_args sargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    sargs.data = in_data;
    sargs.sz = in_sz;

    return pjs_ovs_map_to_json(
            pjs_string_t_to_json,
            &kargs,
            pjs_string_t_to_json,
            &sargs,
            in_num,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  UUID OVS_SMAP handler
 * =============================================================
 */

/**
 * UUID OVS_SMAP handler
 */
bool pjs_ovs_smap_uuid_from_json(
        char *keys,
        size_t key_sz,
        ovs_uuid_t *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_from_json(
            pjs_string_t_from_json,
            &kargs,
            pjs_ovs_uuid_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * UUID OVS_SMAP _to_json handler
 */
bool pjs_ovs_smap_uuid_to_json(
        char *keys,
        size_t key_sz,
        ovs_uuid_t *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args kargs;

    kargs.data = keys;
    kargs.sz = key_sz;

    return pjs_ovs_map_to_json(
            pjs_string_t_to_json,
            &kargs,
            pjs_ovs_uuid_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  Integer OVS_DMAP handler
 * =============================================================
 */

/**
 * INTEGER OVS_DMAP _from_json
 */
bool pjs_ovs_dmap_int_from_json(
        int *keys,
        int *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_from_json(
            pjs_int_t_from_json,
            keys,
            pjs_int_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * INTEGER OVS_DMAP _to_json handler
 */
bool pjs_ovs_dmap_int_to_json(
        int *keys,
        int *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_to_json(
            pjs_int_t_to_json,
            keys,
            pjs_int_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}


/*
 * =============================================================
 *  Boolean OVS_DMAP handler
 * =============================================================
 */

/**
 * BOOL OVS_DMAP _from_json handler
 */
bool pjs_ovs_dmap_bool_from_json(
        int *keys,
        bool *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_from_json(
            pjs_int_t_from_json,
            keys,
            pjs_bool_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * BOOL OVS_DMAP _to_json handler
 */
bool pjs_ovs_dmap_bool_to_json(
        int *keys,
        bool *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_to_json(
            pjs_int_t_to_json,
            keys,
            pjs_bool_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  Real OVS_DMAP handler
 * =============================================================
 */

/**
 * REAL OVS_DMAP _from_json handler
 */
bool pjs_ovs_dmap_real_from_json(
        int *keys,
        double *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_from_json(
            pjs_int_t_from_json,
            keys,
            pjs_real_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * REAL OVS_DMAP _to_json handler
 */
bool pjs_ovs_dmap_real_to_json(
        int *keys,
        double *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_to_json(
            pjs_int_t_to_json,
            keys,
            pjs_real_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  String OVS_DMAP handler
 * =============================================================
 */

/**
 * STRING OVS_DMAP _from_json handler
 */
bool pjs_ovs_dmap_string_from_json(
        int *keys,
        char *out_data,
        int out_sz,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args sargs;

    sargs.data = out_data;
    sargs.sz = out_sz;

    return pjs_ovs_map_from_json(
            pjs_int_t_from_json,
            keys,
            pjs_string_t_from_json,
            &sargs,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * STRING OVS_DMAP _to_json handler
 */
bool pjs_ovs_dmap_string_to_json(
        int *keys,
        char *in_data,
        size_t in_sz,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args sargs;

    sargs.data = in_data;
    sargs.sz = in_sz;

    return pjs_ovs_map_to_json(
            pjs_int_t_to_json,
            keys,
            pjs_string_t_to_json,
            &sargs,
            in_num,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  UUID OVS_DMAP handler
 * =============================================================
 */

/**
 * UUID OVS_DMAP _from_json handler
 */
bool pjs_ovs_dmap_uuid_from_json(
        int *keys,
        ovs_uuid_t *out_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_from_json(
            pjs_int_t_from_json,
            keys,
            pjs_ovs_uuid_t_from_json,
            out_data,
            out_max,
            out_len,
            present,
            js,
            name,
            update,
            err);
}

/**
 * UUID OVS_DMAP _to_json handler
 */
bool pjs_ovs_dmap_uuid_to_json(
        int *keys,
        ovs_uuid_t *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_map_to_json(
            pjs_int_t_to_json,
            keys,
            pjs_ovs_uuid_t_to_json,
            in_data,
            in_num,
            js,
            name,
            err);
}
