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

#ifndef DS_LIST_H_INCLUDED
#define DS_LIST_H_INCLUDED

#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>

#include "ds.h"

/*
 * ============================================================
 *  Macros
 * ============================================================
 */

/*
 * Static initializer -- can be used to initialize global structures.
 *
 * ds_list_t a = DS_LIST_INIT;
 */
#define DS_LIST_INIT(type, elem)        \
{                                       \
    .ol_cof     = offsetof(type, elem), \
    .ol_root    = { .oln_next = NULL }, \
    .ol_tail    = NULL,                 \
    .ol_ndel    = 0,                    \
}

/*
 * Dynamic initializer, used to initialize a list run-time.
 */
#define ds_list_init(list, type, elem)  __ds_list_init(list, offsetof(type, elem))

/*
 * The ever popular foreach statement
 */
#define ds_list_foreach(list, elem)     \
    for ((elem) = ds_list_head(list); (elem) != NULL; (elem) = ds_list_next(list, elem))

#define ds_list_foreach_iter(list, elem, iter)     \
    for ((elem) = ds_list_ifrst(&iter, list); (elem) != NULL; (elem) = ds_list_inext(&iter))

/*
 * ============================================================
 *  Typedefs
 * ============================================================
 */
typedef struct ds_list              ds_list_t;
typedef struct ds_list_node         ds_list_node_t;
typedef struct ds_list_iter         ds_list_iter_t;

/*
 * ============================================================
 *  Structs
 * ============================================================
 */
struct ds_list_node
{
    ds_list_node_t*                 oln_next;       /* Next node pointer                    */
};

struct ds_list
{
    size_t                          ol_cof;         /* Offset of the container structure    */
    ds_list_node_t                  ol_root;        /* Dummy root element                   */
    ds_list_node_t*                 ol_tail;        /* Tail of the list                     */
    uint32_t                        ol_ndel;        /* Number of deletions                  */
};

struct ds_list_iter
{
    ds_list_t*                      oli_list;       /* Root of the list                     */
    ds_list_node_t*                 oli_prev;       /* Prev element -- needed for deletion  */
    ds_list_node_t*                 oli_curr;       /* Current element -- null on remove    */
    ds_list_node_t*                 oli_next;       /* Next element                         */
    uint32_t                        oli_ndel;       /* Number of deletions                  */
};

static inline void __ds_list_init(ds_list_t *list, size_t cof);

/*
 * ===========================================================================
 *  Public API
 * ===========================================================================
 */

static inline bool   ds_list_is_empty(ds_list_t *list);
static inline void  *ds_list_next(ds_list_t *list, void *curr);
static inline void  *ds_list_head(ds_list_t *list);
static inline void  *ds_list_tail(ds_list_t *list);
static inline void   ds_list_insert_head(ds_list_t *list, void *data);
static inline void   ds_list_insert_tail(ds_list_t *list, void *data);
static inline void   ds_list_insert_after(ds_list_t *list, void *after, void *data);
static inline void  *ds_list_remove_after(ds_list_t *list, void *after);
static inline void  *ds_list_remove_head(ds_list_t* list);

/*
 * ===========================================================================
 *  Iterator API
 * ===========================================================================
 */
static inline void  *ds_list_ifirst(ds_list_iter_t* iter, ds_list_t* list);
static inline void  *ds_list_inext(ds_list_iter_t* iter);
static inline void  *ds_list_iinsert(ds_list_iter_t *iter, void *data);
static inline void  *ds_list_iremove(ds_list_iter_t* iter);

#include "../src/ds_list.c.h"

#endif /* DS_LIST_H_INCLUDED */
