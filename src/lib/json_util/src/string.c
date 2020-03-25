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
#include <ctype.h>

#include "log.h"
#include "json_util.h"

enum json_split_mode
{
    JSON_SPLIT_BRACES,
    JSON_SPLIT_QUOTES
};

/*
 * Parameter structure for json_dump_callback() as used by json_gets()
 */
struct json_gets_data
{
    char   *buf;
    size_t  buf_sz;
    char   *pbuf;
};

char *JSON_SPLIT_ERROR = "JSON SPLIT ERROR";

/*
 * Buffer for the static version of json_dumps()
 */
#if (__GNUC__ == 4 && (__GNUC_MINOR__ > 3))
__thread char dumps_static_buf[8192];
#else
static char dumps_static_buf[8192];
#endif

/*
 * json_gets() callback
 */
static int json_gets_fn(const char *input, size_t input_len, void *ctx);

/**
 * This function tries to extract a full JSON message from the string str. It returns a pointer to the first character after
 * the JSON message or NULL if the message was incomplete.
 *
 * If there's an error in the string, a constant address of JSON_SPLIT_ERROR is returned.
 */
char *json_split(char *str)
{
    int level = 0;

    enum json_split_mode mode = JSON_SPLIT_BRACES;

    str += strspn(str, " \t");

    if (*str != '{' && *str != '\0')
    {
        return JSON_SPLIT_ERROR;
    }

    while (*str != '\0')
    {
        switch (mode)
        {
            /* Standard {} match */
            case JSON_SPLIT_BRACES:
                switch (*str)
                {
                    case '{':
                        level++;
                        break;

                    case '}':
                        level--;

                        if (level == 0)
                        {
                            return str + 1;
                        }

                        break;

                    case '"':
                        mode = JSON_SPLIT_QUOTES;
                        break;
                }
                break;

            /* "" match */
            case JSON_SPLIT_QUOTES:
                switch (*str)
                {
                    case '"':
                        /* End of "" */
                        mode = JSON_SPLIT_BRACES;
                        break;

                    case '\\':
                        /* Un-terminated quote character */
                        if (str[1] == '\0') return JSON_SPLIT_ERROR;

                        /*
                         * Valid characters after \ are:
                         *      - \"
                         *      - \\
                         *      - \/
                         *      - \b
                         *      - \f
                         *      - \n
                         *      - \r
                         *      - \t
                         *      - \uXXXX
                         */
                        if (!strchr("\\\"/bfnrtu", str[1]))
                        {
                            LOG(ERR, "\\ must be followed by \\\"/bfnrtu");
                            return JSON_SPLIT_ERROR;
                        }

                        /* \u requires 4 digits afterwards */
                        if (str[1] == 'u')
                        {
                            if (!isxdigit(str[2]) || !isxdigit(str[3]) ||
                                !isxdigit(str[4]) || !isxdigit(str[5]))
                            {
                                LOG(ERR, "json_split: \\u must be followed by 4 hex digits.");
                                return JSON_SPLIT_ERROR;
                            }

                            str += 4;
                        }

                        /* Skip escape char */
                        str++;
                        break;
                }
                break;
        }

        str++;
    }

    /* End reached before a complete message was found */
    return NULL;
}

/*
 * Dump the JSON object to a static string. If there's not enough room, this function shall return false and an empty string.
 */
bool json_gets(
        const json_t *json,
        char *output,
        size_t output_sz,
        int flags)
{
    struct json_gets_data data;

    data.buf = output;
    data.buf_sz = output_sz;
    data.pbuf = output;

    if (json_dump_callback(json, json_gets_fn, &data, flags) != 0)
    {
        snprintf(output, output_sz, "json_gets() error");
        return false;
    }

    return true;
}

int json_gets_fn(
        const char *input,
        size_t input_len,
        void *ctx)
{
    struct json_gets_data *data = ctx;
    int retval = -1;

    /* Boundary check */
    if (data->pbuf + input_len >= data->buf + data->buf_sz)
    {
        /* Pad string */
        goto error;
    }

    memcpy(data->pbuf, input, input_len);

    data->pbuf += input_len;

    retval = 0;

error:
    /* Pad string */
    *data->pbuf = '\0';

    return retval;
}


const char *json_dumps_static(const json_t *json, int flags)
{
    // this is mostly used by debug logs so enable ENCODE_ANY by default
    // to get a meaningful output instead of getting empty strings
    // when json is a simple value instead of object
    flags |= JSON_ENCODE_ANY;
    if (!json_gets(json, dumps_static_buf, sizeof(dumps_static_buf), flags))
    {
        dumps_static_buf[0] = '\0';
    }

    return dumps_static_buf;
}


// a simpler json_gets
bool json_get_str(const json_t *json, char *output, size_t output_sz)
{
    *output = 0;
    if (!json) return true;
    if (!json_gets(json, output, output_sz, JSON_ENCODE_ANY))
    {
        *output = 0;
        return false;
    }
    return true;
}

