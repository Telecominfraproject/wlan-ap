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

#ifndef OVSDB_PRIV_H_INCLUDED
#define OVSDB_PRIV_H_INCLUDED

#include "ovsdb.h"
#include "jansson.h"

/**
 * Common Definitions
 */
#define MODULE_ID LOG_MODULE_ID_OVSDB

#define OVSDB_DEF_DB "Open_vSwitch"

/**
 * Common methods shared between OVSDB modules only
 */

/* Return new JSON-RPC ID */
extern int ovsdb_jsonrpc_id_new(void);

/* Insert a comment into an OVSDB transaction */
extern bool ovsdb_tran_comment(json_t *js_array, ovsdb_tro_t oper, json_t *where);

/* Return a transaction operation as JSON string */
extern json_t *ovsdb_tran_operation(ovsdb_tro_t tran);

#endif /* OVSDB_PRIV_H_INCLUDED */
