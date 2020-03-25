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
 *  Generic handler of optional basic types with OVS support;
 * ===========================================================================
 *
 * The required basic types are basically the same as their pure-JSON
 * counterpart. However, optional types in OVS are a bit different -- when
 * they do not exists, an empty SET is used. This means that everything in OVS
 * is actually a SET. Basic types are therefore SETs where the maximum number
 * of elements is 1. This is problematic with optional members, as we must
 * return an empty SET when they do not exist, and actually parsing an empty
 * set should DELETE them. Although the required types could be handled by
 * the standard JSON types, they are re-implemented here using OVS SETs just
 * for completeness and to prepare future work where the JSON parser and
 * OVS parser might be split.
 *
 * ===========================================================================
 */


extern bool pjs_ovs_set_from_json(
        pjs_type_from_json_t *t_from_json,
        void *t_data,
        int out_max,
        int *out_len,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err);

extern bool pjs_ovs_set_to_json(
        pjs_type_to_json_t *in_cb,
        void *in_data,
        int in_len,
        json_t *js,
        const char *name,
        pjs_errmsg_t err);
/*
 * Convert optional JSON value to an OVS type
 */
bool pjs_ovs_basic_q_from_json(
        pjs_type_from_json_t *t_from_json,
        void *t_data,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    int len;

    /* Ignore non-existent objects in update mode */
    if (update && json_object_get(js, name) == NULL)
    {
        return true;
    }

    /*
     * A basic OVS type is just a SET that has a maximum length of 1 -- parse it as such.
     */
    if (!pjs_ovs_set_from_json(
            t_from_json,
            t_data,
            1,
            &len,
            present,
            js,
            name,
            update,
            err))
    {
        return false;
    }

    if (len > 0)
    {
        *exists = true;
    }
    else
    {
        t_from_json(t_data, 0, NULL);
        *exists = false;
    }

    return true;
}

/*
 * Convert a required JSON value to a C type
 */
bool pjs_ovs_basic_from_json(
        pjs_type_from_json_t *t_from_json,
        void *t_data,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    if (!pjs_ovs_basic_q_from_json(
            t_from_json,
            t_data,
            exists,
            present,
            js,
            name,
            update,
            err))
    {
        return false;
    }

    /* Ignore non-existent objects in update mode */
    if (!*exists && !update)
    {
        PJS_ERR(err, "Required OVS element '%s' does not exist.", name);
        return false;
    }

    return true;
}

/*
 * Convert an optional basic OVS type to JSON
 */
bool pjs_ovs_basic_q_to_json(
        pjs_type_to_json_t *t_to_json,
        void *t_data,
        bool exists,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_to_json(
            t_to_json,
            t_data,
            exists ? 1 : 0,
            js,
            name,
            err);
}

/*
 * Convert an required basic OVS type to JSON
 */
bool pjs_ovs_basic_to_json(
        pjs_type_to_json_t *t_to_json,
        void *t_data,
        json_t *js,
        const char *name,
        pjs_errmsg_t err)
{
    return pjs_ovs_set_to_json(
            t_to_json,
            t_data,
            1,
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
bool pjs_ovs_int_q_from_json(
        int *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_from_json(
            pjs_int_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON integer object into an int
 */
bool pjs_ovs_int_from_json(
        int *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_from_json(
            pjs_int_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Convert a integer to a json object
 */
bool pjs_ovs_int_q_to_json(int in, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_to_json(
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
bool pjs_ovs_int_to_json(int in, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_to_json(
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
bool pjs_ovs_bool_q_from_json(
        bool *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_from_json(
            pjs_bool_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON boolean object into bool
 */
bool pjs_ovs_bool_from_json(
        bool *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_from_json(
            pjs_bool_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Convert a boolean to a json object
 */
bool pjs_ovs_bool_q_to_json(bool in, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_to_json(
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
bool pjs_ovs_bool_to_json(bool in, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_to_json(
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
bool pjs_ovs_real_q_from_json(
        double *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_from_json(
            pjs_real_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON real object into a double
 */
bool pjs_ovs_real_from_json(
        double *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_from_json(
            pjs_real_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Convert a boolean to a json object
 */
bool pjs_ovs_real_q_to_json(double in, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_to_json(
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
bool pjs_ovs_real_to_json(double in, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_to_json(
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
bool pjs_ovs_string_q_from_json(
        char *out,
        size_t outsz,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = out;
    args.sz = outsz;

    return pjs_ovs_basic_q_from_json(
            pjs_string_t_from_json,
            &args,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Parse a non-optional JSON string object into a string
 */
bool pjs_ovs_string_from_json(
        char *out,
        size_t outsz,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = out;
    args.sz = outsz;

    return pjs_ovs_basic_from_json(
            pjs_string_t_from_json,
            &args,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * Convert a string to a json object
 */
bool pjs_ovs_string_q_to_json(char *in, size_t insz, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = in;
    args.sz = insz;

    return pjs_ovs_basic_q_to_json(
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
bool pjs_ovs_string_to_json(char *in, size_t insz, json_t *js, const char *name, pjs_errmsg_t err)
{
    struct pjs_string_args args;

    args.data = in;
    args.sz = insz;

    return pjs_ovs_basic_to_json(
            pjs_string_t_to_json,
            &args,
            js,
            name,
            err);
}

/*
 * ===========================================================================
 *  BASIC OVS UUID
 * ===========================================================================
 */

/**
 * JSON to ovs_uuid_t praser
 */
bool pjs_ovs_uuid_q_from_json(
        ovs_uuid_t *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_from_json(
            pjs_ovs_uuid_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * JSON to ovs_uuid_t parser
 */
bool pjs_ovs_uuid_from_json(
        ovs_uuid_t *out,
        bool *exists,
        bool *present,
        json_t *js,
        const char *name,
        bool update,
        pjs_errmsg_t err)
{
    return pjs_ovs_basic_from_json(
            pjs_ovs_uuid_t_from_json,
            out,
            exists,
            present,
            js,
            name,
            update,
            err);
}

/**
 * ovs_uuid_t to JSON
 */
bool pjs_ovs_uuid_q_to_json(ovs_uuid_t *in, bool in_exists, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_q_to_json(
            pjs_ovs_uuid_t_to_json,
            in,
            in_exists,
            js,
            name,
            err);
}


/**
 * ovs_uuid_t to JSON parser
 */
bool pjs_ovs_uuid_to_json(ovs_uuid_t *in, json_t *js, const char *name, pjs_errmsg_t err)
{
    return pjs_ovs_basic_to_json(
            pjs_ovs_uuid_t_to_json,
            in,
            js,
            name,
            err);
}
