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

#ifndef PJS_TYPES_H_INCLUDED
#define PJS_TYPES_H_INCLUDED

#include <jansson.h>
#include <stdbool.h>

#include "pjs_common.h"

typedef bool        pjs_type_from_json_t(void *data, int idx, json_t *jsval);
typedef json_t     *pjs_type_to_json_t(void *data, int idx);

struct pjs_string_args
{
    char    *data;                      /* Output buffer */
    size_t   sz;                        /* Single element size */
};

struct pjs_sub_args
{
    pjs_sub_from_json_cb_t *cb_out;     /* Deserialize callback */
    pjs_sub_to_json_cb_t   *cb_in;      /* Serialize callback */
    void                   *data;       /* Output buffer */
    size_t                  sz;         /* Single element size in output */
};

/*
 * Type handlers
 */
extern pjs_type_from_json_t pjs_int_t_from_json;
extern pjs_type_from_json_t pjs_bool_t_from_json;
extern pjs_type_from_json_t pjs_real_t_from_json;
extern pjs_type_from_json_t pjs_string_t_from_json;
extern pjs_type_from_json_t pjs_sub_t_from_json;
extern pjs_type_from_json_t  pjs_ovs_uuid_t_from_json;

extern pjs_type_to_json_t pjs_int_t_to_json;
extern pjs_type_to_json_t pjs_bool_t_to_json;
extern pjs_type_to_json_t pjs_real_t_to_json;
extern pjs_type_to_json_t pjs_string_t_to_json;
extern pjs_type_to_json_t pjs_sub_t_to_json;
extern pjs_type_to_json_t pjs_ovs_uuid_t_to_json;

#endif /* PJS_TYPES_H_INCLUDED */
