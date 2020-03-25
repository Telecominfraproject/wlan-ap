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

#include "pjs_common.h"
#include "pjs_types.h"

/*
 * =============================================================
 *  Generic OVS_SET handlers to_json/from_json
 * =============================================================
 */

/**
 * Generic handler _from_json OVS_SET; traverse a SET and call the callback for each entry
 */
bool pjs_ovs_set_from_json(
        pjs_type_from_json_t *t_from_json,
        void *t_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    json_t *jsarr;
    json_t *jsdata;
    size_t ii;

    bool is_set = false;

    /* Reset the map */
    json_t *jsset = json_object_get(js, name);
    if (jsset == NULL || json_is_null(jsset))
    {
        /*
         * If object doesn't exists, and we're NOT in update mode, return an error (false).
         * If we're in update mode, just return true, but do not touch any of the output values.
         */
        if (update)
        {
            return true;
        }

        /*
         * This is another exception in how OVS interprets certain types. Typically when a type is optional and is not set
         * and empty set is returned ([ "set", []]). However, some internal types are just plain missing, for example _uuid
         * in update notifications. Somebody probably thought it would be a nice optimization to leave it out as it is present
         * in the key.
         *
         * Not only do we need to add special code here to handle the case, but we have to manually set the _uuid in initial
         * update requests.
         */
        if (name[0] == '_')
        {
            *out_len = 0;
            return true;
        }

        PJS_ERR(err, "Object '%s' doesn't exist in OVS SET.", name);
        return false;
    }

    // mark presence of field
    *present = true;

    /*
     * Check if we're dealing with a JSON that resembles a SET format: [ "set", [ ... ]]
     */
    if (json_is_array(jsset) &&
            json_array_size(jsset) == 2 &&
            json_is_string(json_array_get(jsset, 0)) &&
            json_is_array(json_array_get(jsset, 1)))
    {
        const char *setstr = json_string_value(json_array_get(jsset, 0));
        is_set = (strcmp(setstr, "set") == 0);
    }

    /* Doesn't look like it's a SET, try to parse it as it was a basic value */
    if (!is_set)
    {
        if (!t_from_json(t_data, 0, jsset))
        {
            PJS_ERR(err, "'%s' cannot convert JSON to type.", name);
            return false;
        }

        *out_len = 1;
        return true;
    }

    /*
     * We have a SET, parse it as an array
     */
    jsarr = json_array_get(jsset, 1);

    if (out_max < (int)json_array_size(jsarr))
    {
        PJS_ERR(err, "OVS_SET '%s' set size too big. Max %d.", name, out_max);
        return false;
    }

    json_array_foreach(jsarr, ii, jsdata)
    {
        if (!t_from_json(t_data, ii, jsdata))
        {
            PJS_ERR(err, "'%s' error converting JSON to type.", name);
            return false;
        }
    }

    *out_len = ii;

    return true;
}

/**
 * Generic handler _to_json OVS_SET; traverse a SET and call the callback for each entry
 */
bool pjs_ovs_set_to_json(
        pjs_type_to_json_t *in_cb,
        void *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    int ii;
    json_t *jsarr;
    json_t *jsval;
    json_t *jsset;

    /*
     * I'm not sure what would happen if we try to override the _uuid or_version
     * fields of a table -- try to avoid generating all internal types during
     * serialization.
     */
    if (name[0] == '_') return true;

    /*
     * In case there's only one element, instead of creating a set
     * with a single element ( "object" : ["set", [ value ] ), just use the value
     * directly -- ( object : value )
     */
    if (in_len == 1)
    {
        jsset = in_cb(in_data, 0);
        if (jsset == NULL)
        {
            PJS_ERR(err, "'%s' error converting single value to JSON.", name);
            goto error;
        }

        if (json_object_set_new(js, name, jsset) != 0)
        {
            PJS_ERR(err, "'%s' unable to add OVS_SET to root object.", name);
            goto error;
        }

        return true;
    }

    jsset = json_array();
    if (jsset == NULL)
    {
        PJS_ERR(err, "'%s' OVS_SET error creating object.", name);
        goto error;
    }

    if (json_array_append_new(jsset, json_string("set")) != 0)
    {
        PJS_ERR(err, "'%s' OVS_SET error appending \"set\" string.", name);
        goto error;
    }

    jsarr = json_array();
    if (jsarr == NULL)
    {
        PJS_ERR(err, "'%s' OVS_SET error creating data array.", name);
        goto error;
    }

    if (json_array_append_new(jsset, jsarr) != 0)
    {
        PJS_ERR(err, "'%s' OVS_SET error appending data array.", name);
        goto error;
    }

    for (ii = 0; ii < in_len; ii++)
    {
        jsval = in_cb(in_data, ii);
        if (jsval == NULL)
        {
            PJS_ERR(err, "'%s' error converting type to JSON.", name);
            goto error;
        }

        if (json_array_append_new(jsarr, jsval) != 0)
        {
            PJS_ERR(err, "'%s' OVS_SET error appending value.", name);
        }
    }

    if (json_object_set_new(js, name, jsset) != 0)
    {
        PJS_ERR(err, "'%s' unable to add OVS_SET to root object.", name);
        goto error;
    }

    return true;

error:
    if (jsset != NULL) json_decref(jsset);
    return false;
}

/*
 * =============================================================
 *  Integer OVS_SET handlers to_json/from_json
 * =============================================================
 */

/**
 * Integer OVS_SET handler
 */
bool pjs_ovs_set_int_from_json(
        int *out,
        int out_max,
        int *out_num,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_from_json(
            pjs_int_t_from_json,
            out,
            out_max,
            out_num,
            present,
            js,
            name,
            update,
            err);
}

/*
 * Required Integer ARRAY handler
 */
bool pjs_ovs_set_int_to_json(
        int *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_to_json(
            pjs_int_t_to_json,
            in_data,
            in_len,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  Boolean OVS_SET handlers to_json/from_json
 * =============================================================
 */

/**
 * Boolean OVS_SET handler
 */
bool pjs_ovs_set_bool_from_json(
        bool *out,
        int out_max,
        int *out_num,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_from_json(
            pjs_bool_t_from_json,
            out,
            out_max,
            out_num,
            present,
            js,
            name,
            update,
            err);
}

/*
 * Required Boolean ARRAY handler
 */
bool pjs_ovs_set_bool_to_json(
        bool *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_to_json(
            pjs_bool_t_to_json,
            in_data,
            in_len,
            js,
            name,
            err);
}


/*
 * =============================================================
 *  Real OVS_SET handlers to_json/from_json
 * =============================================================
 */

/**
 * Real OVS_SET handler
 */
bool pjs_ovs_set_real_from_json(
        double *out,
        int out_max,
        int *out_num,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_from_json(
            pjs_real_t_from_json,
            out,
            out_max,
            out_num,
            present,
            js,
            name,
            update,
            err);
}

/*
 * Real OVS_SET handler
 */
bool pjs_ovs_set_real_to_json(
        double *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_to_json(
            pjs_real_t_to_json,
            in_data,
            in_len,
            js,
            name,
            err);
}


/*
 * =============================================================
 *  String OVS_SET handlers to_json/from_json
 * =============================================================
 */

/**
 * string OVS_SET handler
 */
bool pjs_ovs_set_string_from_json(
        char *out,
        size_t out_sz,
        int out_max,
        int *out_num,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = out;
    args.sz = out_sz;

    return pjs_ovs_set_from_json(
            pjs_string_t_from_json,
            &args,
            out_max,
            out_num,
            present,
            js,
            name,
            update,
            err);
}


/*
 * String ARRAY handler
 */
bool pjs_ovs_set_string_to_json(
        char *in_data,
        size_t in_sz,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = in_data;
    args.sz = in_sz;

    return pjs_ovs_set_to_json(
            pjs_string_t_to_json,
            &args,
            in_len,
            js,
            name,
            err);
}

/*
 * =============================================================
 *  UUID OVS_SET handlers to_json/from_json
 * =============================================================
 */

/**
 * Real OVS_SET handler
 */
bool pjs_ovs_set_uuid_from_json(
        ovs_uuid_t *out,
        int out_max,
        int *out_num,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_from_json(
            pjs_ovs_uuid_t_from_json,
            out,
            out_max,
            out_num,
            present,
            js,
            name,
            update,
            err);
}

/*
 * Real OVS_SET handler
 */
bool pjs_ovs_set_uuid_to_json(
        ovs_uuid_t *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_to_json(
            pjs_ovs_uuid_t_to_json,
            in_data,
            in_len,
            js,
            name,
            err);
}
