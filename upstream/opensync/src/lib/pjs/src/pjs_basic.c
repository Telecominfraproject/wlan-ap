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
 * ===========================================================================
 *  Generic handler of basic types
 * ===========================================================================
 */

/*
 * Convert optional JSON value to a C type
 */
bool pjs_basic_q_from_json(
        pjs_type_from_json_t *t_from_json,
        void *t_data,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    /*
     * Check if element doesn't exists in object -- if it does not, set the *exists, unless we're only
     * updating the current fields
     */
    json_t *jsval = json_object_get(js, name);
    if (jsval == NULL || json_is_null(jsval))
    {
        if (!update)
        {
            *exists = false;
            t_from_json(t_data, 0, NULL);
        }

        return true;
    }

    if (!t_from_json(t_data, 0, jsval))
    {
        PJS_ERR(err, "Unable to convert JSON to element '%s'.", name);
        return false;
    }

    *exists = true;

    return true;
}

/*
 * Convert a required JSON value to a C type
 */
bool pjs_basic_from_json(
        pjs_type_from_json_t *t_from_json,
        void *t_data,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    bool exists = false;

    if (!pjs_basic_q_from_json(
            t_from_json,
            t_data,
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
        PJS_ERR(err, "Required element '%s' does not exist.", name);
        return false;
    }

    return true;
}

/*
 * Convert a basic type to JSON
 */
bool pjs_basic_q_to_json(
        pjs_type_to_json_t *t_to_json,
        void *t_data,
        bool exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    json_t *jsval = NULL;

    if (!exists)
    {
        /* Element doesn't exists, do nothing */
        return true;
    }

    jsval = t_to_json(t_data, 0);
    if (jsval == NULL)
    {
        PJS_ERR(err, "'%s' unable to convert to JSON.", name);
        goto error;
    }

    if (json_object_set_new(js, name, jsval) != 0)
    {
        PJS_ERR(err, "'%s' unable to set object.", name);
        goto error;
    }

    return true;

error:
    if (jsval != NULL) json_decref(jsval);

    return false;
}

bool pjs_basic_to_json(
        pjs_type_to_json_t *t_to_json,
        void *t_data,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_basic_q_to_json(
            t_to_json,
            t_data,
            true,
            js,
            name,
            err);
}

/*
 * ===========================================================================
 *  BASIC INT
 * ===========================================================================
 */

/**
 * Parse a JSON integer object into a C int
 */
bool pjs_int_q_from_json(
        int *out,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_basic_q_from_json(
            pjs_int_t_from_json,
            out,
            exists,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON integer object into an int
 */
bool pjs_int_from_json(int *out, json_t *js, const char *name, bool update, pjs_errmsg_t err)
{
    return pjs_basic_from_json(
            pjs_int_t_from_json,
            out,
            js,
            name,
            update,
            err);
}

/**
 * Convert a integer to a json object
 */
bool pjs_int_q_to_json(int in, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_basic_q_to_json(
            pjs_int_t_to_json,
            &in,
            in_exists,
            js,
            name,
            err);
}

/**
 * Convert a integer to a json object
 */
bool pjs_int_to_json(int in, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_basic_to_json(
            pjs_int_t_to_json,
            &in,
            js,
            name,
            err);
}

/*
 * ===========================================================================
 *  BASIC BOOL
 * ===========================================================================
 */
/**
 * Parse a JSON boolean object into a C bool
 */
bool pjs_bool_q_from_json(bool *out, bool *exists, json_t *js, const char *name, bool update, pjs_errmsg_t err)
{
    return pjs_basic_q_from_json(
            pjs_bool_t_from_json,
            out,
            exists,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON boolean object into bool
 */
bool pjs_bool_from_json(bool *out, json_t *js, const char *name, bool update, pjs_errmsg_t err)
{
    return pjs_basic_from_json(
            pjs_bool_t_from_json,
            out,
            js,
            name,
            update,
            err);
}

/**
 * Convert a boolean to a json object
 */
bool pjs_bool_q_to_json(bool in, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_basic_q_to_json(
            pjs_bool_t_to_json,
            &in,
            in_exists,
            js,
            name,
            err);
}


/**
 * Convert a boolean to a json object
 */
bool pjs_bool_to_json(bool in , json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_basic_to_json(
            pjs_bool_t_to_json,
            &in,
            js,
            name,
            err);
}

/*
 * ===========================================================================
 *  BASIC REAL
 * ===========================================================================
 */

/**
 * Parse a JSON real object into a C double
 */
bool pjs_real_q_from_json(double *out, bool *exists, json_t *js, const char *name, bool update, pjs_errmsg_t err)
{
    return pjs_basic_q_from_json(
            pjs_real_t_from_json,
            out,
            exists,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON real object into a double
 */
bool pjs_real_from_json(double *out, json_t *js, const char *name, bool update, pjs_errmsg_t err)
{
    return pjs_basic_from_json(
            pjs_real_t_from_json,
            out,
            js,
            name,
            update,
            err);
}

/**
 * Convert a double to a json object
 */
bool pjs_real_q_to_json(double in, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_basic_q_to_json(
            pjs_real_t_to_json,
            &in,
            in_exists,
            js,
            name,
            err);
}

/**
 * Convert a double to a json object
 */
bool pjs_real_to_json(double in , json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_basic_to_json(
            pjs_real_t_to_json,
            &in,
            js,
            name,
            err);
}


/*
 * ===========================================================================
 *  BASIC STRING
 * ===========================================================================
 */

/**
 * Parse a JSON string object into a string
 */
bool pjs_string_q_from_json(
        char *out,
        size_t outsz,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = out;
    args.sz = outsz;

    return pjs_basic_q_from_json(
            pjs_string_t_from_json,
            &args,
            exists,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON string object into a string
 */
bool pjs_string_from_json(
        char *out,
        size_t outsz,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = out;
    args.sz = outsz;

    return pjs_basic_from_json(
            pjs_string_t_from_json,
            &args,
            js,
            name,
            update,
            err);
}

/**
 * Convert a string to a json object
 */
bool pjs_string_q_to_json(char *in, size_t insz, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = in;
    args.sz = insz;

    return pjs_basic_q_to_json(
            pjs_string_t_to_json,
            &args,
            in_exists,
            js,
            name,
            err);
}

/**
 * Convert a string to a json object
 */
bool pjs_string_to_json(char *in, size_t insz, json_t *js, const char *name, pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = in;
    args.sz = insz;

    return pjs_basic_to_json(
            pjs_string_t_to_json,
            &args,
            js,
            name,
            err);
}

/*
 * ===========================================================================
 *  BASIC SUB
 * ===========================================================================
 */

/**
 * Parse a JSON object into a substructre
 */
bool pjs_sub_q_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        bool *exists,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;

    args.cb_out = out_cb;
    args.sz = out_sz;
    args.data = out_data;

    return pjs_basic_q_from_json(
            pjs_sub_t_from_json,
            &args,
            exists,
            js,
            name,
            update,
            err);
}

/**
 * Parse a JSON object into a substructre
 */
bool pjs_sub_from_json(
        pjs_sub_from_json_cb_t *out_cb,
        void *out_data,
        size_t out_sz,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;

    args.cb_out = out_cb;
    args.sz     = out_sz;
    args.data   = out_data;

    return pjs_basic_from_json(
            pjs_sub_t_from_json,
            &args,
            js,
            name,
            update,
            err);
}

/**
 * Convert a sub-structure into a JSON object
 */
bool pjs_sub_q_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        bool in_exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;

    args.cb_in  = in_cb;
    args.sz     = in_sz;
    args.data   = in_data;

    return pjs_basic_q_to_json(
            pjs_sub_t_to_json,
            &args,
            in_exists,
            js,
            name,
            err);
}

/**
 * Convert a sub-structure into a JSON object
 */
bool pjs_sub_to_json(
        pjs_sub_to_json_cb_t *in_cb,
        void *in_data,
        size_t in_sz,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    struct pjs_sub_args args;

    args.cb_in  = in_cb;
    args.sz     = in_sz;
    args.data   = in_data;

    return pjs_basic_to_json(
            pjs_sub_t_to_json,
            &args,
            js,
            name,
            err);
}
