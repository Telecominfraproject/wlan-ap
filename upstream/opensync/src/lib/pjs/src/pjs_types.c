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

#include "pjs_types.h"

/*
 * ===========================================================================
 *  INTEGER conversion handlers
 * ===========================================================================
 */

/**
 * Convert a JSON value to an integer
 */
bool pjs_int_t_from_json(void *data, int idx, json_t *jsdata)
{
    int *out = data;

    /* Initialize default value */
    if (jsdata == NULL)
    {
        out[idx] = 0;
        return true;
    }

    if (!json_is_integer(jsdata))
    {
        return false;
    }

    out[idx] = json_integer_value(jsdata);

    return true;
}

/**
 * Convert a integer to a JSON value
 */
json_t *pjs_int_t_to_json(void *data, int idx)
{
    int *in = data;

    return json_integer(in[idx]);
}

/*
 * ===========================================================================
 *  BOOLEAN conversion handlers
 * ===========================================================================
 */

/**
 * Convert a JSON value to a boolean
 */
bool pjs_bool_t_from_json(void *data, int idx, json_t *jsdata)
{
    bool *out = data;

    if (jsdata == NULL)
    {
        out[idx] = false;
        return true;
    }

    if (!json_is_boolean(jsdata))
    {
        return false;
    }

    out[idx] = json_boolean_value(jsdata);

    return true;
}

/**
 * Convert a boolean to a JSON value
 */
json_t *pjs_bool_t_to_json(void *data, int idx)
{
    bool *in = data;

    return json_boolean(in[idx]);
}

/*
 * ===========================================================================
 *  REAL conversion handlers
 * ===========================================================================
 */

/**
 * Convert a JSON value to a double
 */
bool pjs_real_t_from_json(void *data, int idx, json_t *jsdata)
{
    double *out = data;

    if (jsdata == NULL)
    {
        *out = 0.0;
        return true;
    }

    if (!json_is_number(jsdata))
    {
        return false;
    }

    out[idx] = json_real_value(jsdata);

    return true;
}

/**
 * Convert a double to a JSON value
 */
json_t *pjs_real_t_to_json(void *data, int idx)
{
    double *in = data;

    return json_real(in[idx]);
}

/*
 * ===========================================================================
 *  STRING conversion handlers
 * ===========================================================================
 */

/**
 * Convert a JSON value to a string
 */
bool pjs_string_t_from_json(void *data, int idx, json_t *jsdata)
{
    char *str;

    struct pjs_string_args *args = data;

    if (jsdata == NULL)
    {
        args->data[0] = '\0';
        return true;
    }

    if (!json_is_string(jsdata))
    {
        return false;
    }

    str = (char *)json_string_value(jsdata);
    if (str == NULL)
    {
        return false;
    }

    if (strlen(str) >= args->sz)
    {
        return false;
    }

    strcpy(&args->data[args->sz * idx], str);

    return true;
}

/**
 * Convert a string to a JSON value
 */
json_t *pjs_string_t_to_json(void *data, int idx)
{
    struct pjs_string_args *args = data;

    if (strlen(args->data + args->sz * idx) >= args->sz)
    {
        return NULL;
    }

    return json_string(args->data + args->sz * idx);
}

/*
 * ===========================================================================
 *  SUB conversion handlers
 * ===========================================================================
 */

/**
 * Convert a JSON value to SUB
 */
bool pjs_sub_t_from_json(void *data, int idx, json_t *jsdata)
{
    struct pjs_sub_args *args = data;

    if (jsdata == NULL)
    {
        memset(args->data + (idx * args->sz), 0, args->sz);
        return true;
    }

    return args->cb_out((char *)args->data + (idx * args->sz), jsdata, false, NULL);
}

/**
 * Convert a SUB to a JSON value
 */
json_t *pjs_sub_t_to_json(void *data, int idx)
{
    json_t *jsdata;

    struct pjs_sub_args *args = data;

    jsdata = args->cb_in((char *)args->data + (idx * args->sz), NULL);
    if (jsdata == NULL)
    {
        return NULL;
    }

    return jsdata;
}

/*
 * ===========================================================================
 *  OVS UUIDs are better handled with a special type
 * ===========================================================================
 */

/**
 * Convert a JSON value to ovs_uuid_t
 */
bool pjs_ovs_uuid_t_from_json(void *__data, int idx, json_t *jsdata)
{
    const char  *str;

    ovs_uuid_t *uuid = __data;

    if (jsdata == NULL)
    {
        strcpy(uuid->uuid, "00000000-0000-0000-0000-000000000000");
        return true;
    }

    /*
     * An UUID is in the format of [ "uuid", "<actual UUID data>" ] -- so first we check if the
     * array is of the correct size and that the first element in the array is the string "uuid"
     */

    if (!json_is_array(jsdata))
    {
        // PJS_ERR(err, "UUID '%s' not an array.", name);
        return false;
    }

    if (json_array_size(jsdata) != 2)
    {
        // PJS_ERR(err, "UUID '%s' needs exactly 2 elements in the array.", name);
        return false;
    }

    str = json_string_value(json_array_get(jsdata, 0));
    if (str == NULL || strcmp(str, "uuid") != 0)
    {
        // PJS_ERR(err, "UUID '%s' index 0 expected string value.", name);
        return false;
    }

    /* Extract the actual value */
    str = json_string_value(json_array_get(jsdata, 1));
    if (str == NULL)
    {
        // PJS_ERR(err, "UUID '%s' index 1 invalid string value.", name);
        return false;
    }

    /* String too small? */
    if (strlen(str) >= sizeof(ovs_uuid_t))
    {
        // PJS_ERR(err, "UUID '%s' string too long.", name);
        return false;
    }

    strcpy(uuid[idx].uuid, str);

    return true;
}

/**
 * Convert a UUID to a JSON object
 */
json_t *pjs_ovs_uuid_t_to_json(void *__data, int idx)
{
    json_t *jsval = NULL;

    ovs_uuid_t *uuid = __data;

    if (strlen(uuid->uuid) >= PJS_OVS_UUID_SZ)
    {
        goto error;
    }

    jsval = json_array();
    if (jsval == NULL)
    {
        // PJS_ERR(err, "UUID '%s' error allocating array.", name);
        goto error;
    }

    if (json_array_append_new(jsval, json_string("uuid")) != 0)
    {
        // PJS_ERR(err, "UUID '%s' error appending 'uuid' at index 0.", name);
        goto error;
    }

    if (json_array_append_new(jsval, json_string(uuid[idx].uuid)) != 0)
    {
        // PJS_ERR(err, "UUID '%s' error appending '%s' at index 1.", name, in);
        goto error;
    }

    return jsval;

error:
    if (jsval != NULL) json_decref(jsval);

    return false;
}
