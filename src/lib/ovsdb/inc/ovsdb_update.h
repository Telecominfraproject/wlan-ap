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

#ifndef OVSDB_UPDATE_H_INCLUDED
#define OVSDB_UPDATE_H_INCLUDED

#include <stdbool.h>
#include <jansson.h>

#include "ovsdb.h"
#include "crt.h"
#include "os.h"

/*
 * ===========================================================================
 *  OVSDB Update Parsing facility
 * ===========================================================================
 */
typedef struct ovsdb_update_parse ovsdb_update_parse_t;

struct ovsdb_update_parse
{
    crt_t                up_crt;            /* Coroutine context */
    json_t              *up_jtable;         /* Table list */
    void                *up_itable;         /* Table list iterator */
    json_t              *up_jrow;           /* Current row list */
    void                *up_irow;           /* Row jansson iterator */
    void                *up_jnew;           /* Current (new) row data */
    void                *up_jold;           /* Old row data, if any */
    const char          *up_uuid;           /* The UUID */
};

extern bool ovsdb_update_parse_start(ovsdb_update_parse_t *self, json_t *js);
extern bool ovsdb_update_parse_next(ovsdb_update_parse_t *self);

static inline const char *ovsdb_update_parse_get_table(ovsdb_update_parse_t *self)
{
    return json_object_iter_key(self->up_itable);
}

static inline const char *ovsdb_update_parse_get_uuid(ovsdb_update_parse_t *self)
{
    return self->up_uuid;
}

static inline json_t *ovsdb_update_parse_get_new(ovsdb_update_parse_t *self)
{
    return self->up_jnew;
}

static inline json_t *ovsdb_update_parse_get_old(ovsdb_update_parse_t *self)
{
    return self->up_jold;
}

/*
 * ===========================================================================
 *  OVSDB Update Monitor
 * ===========================================================================
 */
/*
 * Update event type:
 *
 *  -- OVSDB_UPDATE_NEW     : Row was inserted or initial
 *  -- OVSDB_UPDATE_MODIFY  : Row was modified
 *  -- OVSDB_UPDATE_DEL     : Row was deleted
 *  -- OVSDB_UPDATE_ERROR   : Error occurred
 */

typedef enum
{
    OVSDB_UPDATE_NEW,
    OVSDB_UPDATE_MODIFY,
    OVSDB_UPDATE_DEL,
    OVSDB_UPDATE_ERROR,
}
ovsdb_update_type_t;

typedef struct ovsdb_update_monitor_s ovsdb_update_monitor_t;

typedef void ovsdb_update_cbk_t(ovsdb_update_monitor_t *self);

struct ovsdb_update_monitor_s
{
    ovsdb_update_cbk_t     *mon_cb;            /* Update callback */
    void                   *mon_data;          /* User-supplied data, not touched by ovsdb_update_* */

    /*
     * Except the fields below to be valid only during a callback,
     * the values should be considered undefined after the callback
     * returns
     */
    ovsdb_update_type_t     mon_type;           /* Update type */
    const char             *mon_table;          /* Table that was modified */
    const char             *mon_uuid;           /* UUID of the modified ROW */
    json_t                 *mon_json_new;       /* JSON message containing the update */
    json_t                 *mon_json_old;       /* JSON message containing old data */
    void                   *mon_old_rec;
};

/*
 * Start monitoring a table for updates
 */
extern bool ovsdb_update_monitor_ex(
        ovsdb_update_monitor_t *self,
        ovsdb_update_cbk_t *callback,
        char *mon_table,
        int mon_flags,
        int colc,
        char *colv[]);

/*
 * Start monitoring a table for updates -- monitors all columns
 */
extern bool ovsdb_update_monitor(
        ovsdb_update_monitor_t *self,
        ovsdb_update_cbk_t *callback,
        char *mon_table,
        int mon_flags);

bool ovsdb_update_changed(ovsdb_update_monitor_t *self, char *field);

char* ovsdb_update_type_to_str(ovsdb_update_type_t update_type);

#endif /* OVSDB_UPDATE_H_INCLUDED */
