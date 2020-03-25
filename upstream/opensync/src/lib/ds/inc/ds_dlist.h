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

#ifndef DS_DLIST_H_INCLUDED
#define DS_DLIST_H_INCLUDED

#include <stddef.h>
#include "ds.h"

/*
 * ============================================================
 *  Macros
 * ============================================================
 */

/** Static initialization */
#define DS_DLIST_INIT(type, elem)       \
{                                       \
    .od_cof  = offsetof(type, elem),    \
    .od_head = NULL,                    \
    .od_tail = NULL,                    \
    .od_ndel = 0,                       \
}

/** Run-time initialization */
#define ds_dlist_init(list, type, elem) __ds_dlist_init(list, offsetof(type, elem))

#define ds_dlist_foreach(list, p)       \
    for (p = ds_dlist_head(list); p != NULL; p = ds_dlist_next(list, p))

#define ds_dlist_foreach_iter ds_dlist_iforeach

#define ds_dlist_iforeach(list, p, iter) \
    for (p = ds_dlist_ifirst(&iter, list); p != NULL; p = ds_dlist_inext(&iter))

/*
 * ============================================================
 *  Typedefs
 * ============================================================
 */
typedef struct ds_dlist             ds_dlist_t;
typedef struct ds_dlist_node        ds_dlist_node_t;
typedef struct ds_dlist_iter        ds_dlist_iter_t;

/*
 * ============================================================
 *  Structs
 * ============================================================
 */
struct ds_dlist
{
    size_t                          od_cof;
    ds_dlist_node_t*                od_head;
    ds_dlist_node_t*                od_tail;
    uint32_t                        od_ndel;
};

struct ds_dlist_node
{
    ds_dlist_node_t*                odn_prev;
    ds_dlist_node_t*                odn_next;
};

struct ds_dlist_iter
{
    ds_dlist_t                      *odi_list;
    ds_dlist_node_t                 *odi_curr;
    ds_dlist_node_t                 *odi_next;
    uint32_t                        odi_ndel;
};

/*
 * ===========================================================================
 *  Public API
 * ===========================================================================
 */

static inline bool   ds_dlist_is_empty(ds_dlist_t *list);
static inline void  *ds_dlist_next(ds_dlist_t *list, void *data);
static inline void  *ds_dlist_prev(ds_dlist_t *list, void *data);
static inline void  *ds_dlist_head(ds_dlist_t *list);
static inline void  *ds_dlist_tail(ds_dlist_t *list);
static inline void   ds_dlist_insert_head(ds_dlist_t* list, void *data);
static inline void   ds_dlist_insert_tail(ds_dlist_t* list, void *data);
static inline void   ds_dlist_insert_after(ds_dlist_t *list, void *after, void *data);
static inline void   ds_dlist_insert_before(ds_dlist_t *list, void *before, void *data);
static inline void   ds_dlist_remove(ds_dlist_t* list, void *data);
static inline void  *ds_dlist_remove_head(ds_dlist_t* list);
static inline void  *ds_dlist_remove_tail(ds_dlist_t* list);
static inline void  *ds_dlist_remove_after(ds_dlist_t *list, void *after);
static inline void  *ds_dlist_remove_before(ds_dlist_t *list, void *before);

/*
 * ===========================================================================
 *  Iterator API
 * ===========================================================================
 */
static inline void  *ds_dlist_ifirst(ds_dlist_iter_t* iter, ds_dlist_t* list);
static inline void  *ds_dlist_inext(ds_dlist_iter_t* iter);
static inline void  *ds_dlist_iinsert(ds_dlist_iter_t *iter, void *data);
static inline void  *ds_dlist_iremove(ds_dlist_iter_t* iter);

#include "../src/ds_dlist.c.h"

extern int ds_dlist_check(ds_dlist_t* list);

#endif /* DS_DLIST_H_INCLUDED */
