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
 *  Generic ARRAY handlers to_json/from_json
 * =============================================================
 */

/**
 * Generic ARRAY _from_json handler
 */
bool pjs_generic_array_from_json(
        pjs_type_from_json_t *out_cb,
        void *out_data,
        int out_max,
        int *out_num,
        bool *out_exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    json_t *jsarr;

    if (!update)
    {
        *out_num = 0;
        *out_exists = false;
    }

    jsarr = json_object_get(js, name);
    if (jsarr == NULL || json_is_null(jsarr))
    {
        return true;
    }

    /*
     * OVS returns arrays with a single entry as a basic
     * type (not as an array).
     */
    if (!json_is_array(jsarr))
    {
        PJS_ERR(err, "'%s' not an array.", name);
        return false;
    }

    if (out_max < (int)json_array_size(jsarr))
    {
        PJS_ERR(err, "'%s' array to big.", name);
        return false;
    }

    json_t *jsdata;
    size_t ii;

    json_array_foreach(jsarr, ii, jsdata)
    {
        if (!out_cb(out_data, ii, jsdata))
        {
            PJS_ERR(err, "'%s' error converting type to JSON.", name);
            return false;
        }
    }

    *out_num = ii;
    *out_exists = true;

    return true;
}

/**
 * Generic ARRAY _to_json handler
 */
bool pjs_generic_array_to_json(
        pjs_type_to_json_t *in_cb,
        void *in_data,
        int in_num,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    int     ii;
    json_t *jsval;
    json_t *jsarr;

    if (!in_exists) return true;

    jsarr = json_array();
    if (jsarr == NULL)
    {
        PJS_ERR(err, "'%s' unable to create array object.", name);
        goto error;
    }

    for (ii = 0; ii < in_num; ii++)
    {
        jsval = in_cb(in_data, ii);
        if (jsval == NULL)
        {
            PJS_ERR(err, "'%s' error converting type to JSON.", name);
            goto error;
        }

        if (json_array_append_new(jsarr, jsval) != 0)
        {
            PJS_ERR(err, "'%s' unable to append to array.", name);
            goto error;
        }

    }

    if (json_object_set_new(js, name, jsarr) != 0)
    {
        PJS_ERR(err, "'%s' unable to add array to root object.", name);
        goto error;
    }

    return true;

error:
    if (jsarr != NULL) json_decref(jsarr);

    return false;
}


/*
 * =============================================================
 *  Integer ARRAY handlers to_json/from_json
 * =============================================================
 */

/**
 * Optional Integer specialized ARRAY handler
 */
bool pjs_int_array_q_from_json(
        int *out_data,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;

    return pjs_generic_array_from_json(
                pjs_int_t_from_json,
                out_data,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err);
}

/**
 * Required Integer ARRAY handler
 */
bool pjs_int_array_from_json(
        int *out_data,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;

    if (!pjs_generic_array_from_json(
                pjs_int_t_from_json,
                out_data,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err))
    {
        return false;
    }

    if (!update && !exists)
    {
        PJS_ERR(err, "Required integer array '%s' does not exist.", name);
        return false;
    }

    return true;
}

/**
 * Optional Integer ARRAY handler
 */
bool pjs_int_array_q_to_json(
        int *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_generic_array_to_json(
                pjs_int_t_to_json,
                in_data,
                in_num,
                in_num > 0,
                js,
                name,
                err);
}

/**
 * Required Integer ARRAY handler
 */
bool pjs_int_array_to_json(
        int *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_generic_array_to_json(
                pjs_int_t_to_json,
                in_data,
                in_num,
                true,
                js,
                name,
                err);
}

/*
 * =============================================================
 *  Boolean ARRAY handlers to_json/from_json
 * =============================================================
 */

/**
 * Optional Bool specialized ARRAY handler
 */
bool pjs_bool_array_q_from_json(
        bool *out_data,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;

    return pjs_generic_array_from_json(
                pjs_bool_t_from_json,
                out_data,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err);
}

/**
 * Required Bool ARRAY handler
 */
bool pjs_bool_array_from_json(
        bool *out_data,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;

    if (!pjs_generic_array_from_json(
                pjs_bool_t_from_json,
                out_data,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err))
    {
        return false;
    }

    if (!update && !exists)
    {
        PJS_ERR(err, "Required bool array '%s' does not exists.", name);
        return false;
    }

    return true;
}

/**
 * Optional Boolean ARRAY handler
 */
bool pjs_bool_array_q_to_json(
        bool *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_generic_array_to_json(
                pjs_bool_t_to_json,
                in_data,
                in_num,
                in_num > 0,
                js,
                name,
                err);
}

/**
 * Required Boolean ARRAY handler
 */
bool pjs_bool_array_to_json(
        bool *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_generic_array_to_json(
                pjs_bool_t_to_json,
                in_data,
                in_num,
                true,
                js,
                name,
                err);
}

/*
 * =============================================================
 *  Real ARRAY handlers to_json/from_json
 * =============================================================
 */

/**
 * Optional Real specialized ARRAY handler
 */
bool pjs_real_array_q_from_json(
        double *out_data,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;

    return pjs_generic_array_from_json(
                pjs_real_t_from_json,
                out_data,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err);
}

/**
 * Required Real ARRAY handler
 */
bool pjs_real_array_from_json(
        double *out_data,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;

    if (!pjs_generic_array_from_json(
                pjs_real_t_from_json,
                out_data,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err))
    {
        return false;
    }

    if (!update && !exists)
    {
        PJS_ERR(err, "Required real array '%s' does not exists.", name);
        return false;
    }

    return true;
}

/**
 * Optional Real ARRAY handler
 */
bool pjs_real_array_q_to_json(
        double *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_generic_array_to_json(
                pjs_real_t_to_json,
                in_data,
                in_num,
                in_num > 0,
                js,
                name,
                err);
}

/**
 * Required Real ARRAY handler
 */
bool pjs_real_array_to_json(
        double *in_data,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_generic_array_to_json(
                pjs_real_t_to_json,
                in_data,
                in_num,
                true,
                js,
                name,
                err);

    return true;
}

/*
 * =============================================================
 *  String ARRAY handlers to_json/from_json
 * =============================================================
 */

/**
 * Optional String specialized ARRAY handler
 */
bool pjs_string_array_q_from_json(
        char *out_data,
        size_t out_sz,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;
    struct pjs_string_args  args;

    args.data = out_data;
    args.sz = out_sz;

    return pjs_generic_array_from_json(
                pjs_string_t_from_json,
                &args,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err);
}

/**
 * Required String ARRAY handler
 */
bool pjs_string_array_from_json(
        char *out_data,
        size_t out_sz,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;
    bool exists = false;

    args.data = out_data;
    args.sz = out_sz;

    if (!pjs_generic_array_from_json(
                pjs_string_t_from_json,
                &args,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err))
    {
        return false;
    }

    if (!update && !exists)
    {
        PJS_ERR(err, "Required string array '%s' does not exists.", name);
        return false;
    }

    return true;
}

/**
 * Optional String ARRAY handler
 */
bool pjs_string_array_q_to_json(
        char *in_data,
        size_t in_sz,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = in_data;
    args.sz = in_sz;

    return pjs_generic_array_to_json(
                pjs_string_t_to_json,
                &args,
                in_num,
                in_num > 0,
                js,
                name,
                err);
}

/**
 * Required String ARRAY handler
 */
bool pjs_string_array_to_json(
        char *in_data,
        size_t in_sz,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = in_data;
    args.sz = in_sz;

    return pjs_generic_array_to_json(
                pjs_string_t_to_json,
                &args,
                in_num,
                true,
                js,
                name,
                err);
}

/*
 * =============================================================
 *  Sub ARRAY handlers to_json/from_json
 * =============================================================
 */

/**
 * Optinal Sub ARRAY handler
 */
bool pjs_sub_array_q_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;
    bool exists = false;

    args.cb_out = out_cb;
    args.data = out_data;
    args.sz = out_sz;

    return pjs_generic_array_from_json(
                pjs_sub_t_from_json,
                &args,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err);
}

/**
 * Required Sub ARRAY handler
 */
bool pjs_sub_array_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        int out_max,
        int *out_num,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;
    bool exists = false;

    args.cb_out = out_cb;
    args.data = out_data;
    args.sz = out_sz;

    if (!pjs_generic_array_from_json(
                pjs_sub_t_from_json,
                &args,
                out_max,
                out_num,
                &exists,
                js,
                name,
                update,
                err))
    {
        return false;
    }

    if (!update && !exists)
    {
        PJS_ERR(err, "Required sub array '%s' does not exists.", name);
    }

    return true;
}

/**
 * Optional SUB array handler
 */
bool pjs_sub_array_q_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;

    args.cb_in = in_cb;
    args.data = in_data;
    args.sz = in_sz;

    return pjs_generic_array_to_json(
                pjs_sub_t_to_json,
                &args,
                in_num,
                in_num > 0,
                js,
                name,
                err);
}

/**
 * Required SUB array handler
 */
bool pjs_sub_array_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        int in_num,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;

    args.cb_in = in_cb;
    args.data = in_data;
    args.sz = in_sz;

    return pjs_generic_array_to_json(
                pjs_sub_t_to_json,
                &args,
                in_num,
                true,
                js,
                name,
                err);
}
