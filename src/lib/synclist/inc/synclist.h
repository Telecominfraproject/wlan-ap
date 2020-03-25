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

#ifndef SYNCLIST_H_INCLUDED
#define SYNCLIST_H_INCLUDED

#include "ds_tree.h"

typedef struct synclist_node synclist_node_t;
typedef struct synclist synclist_t;

typedef void *synclist_fn_t(synclist_t *list, void *pold, void *pnew);

struct synclist
{
    ds_tree_t           sl_tree;    /* RBtree for sync list lookup */
    off_t               sl_cof;     /* Offset to synclist */
    synclist_fn_t      *sl_fn;      /* synclist update function */
};

struct synclist_node
{
    ds_tree_node_t  sl_tnode;
    bool            sl_active;
};

#define SYNCLIST_INIT(cmp, type, elem, fn)                  \
{                                                           \
    .sl_tree = DS_TREE_INIT(cmp, type, elem.sl_tnode),      \
    .sl_cof = offsetof(type, elem),                         \
    .sl_fn = fn,                                            \
}

#define synclist_foreach(list, p) ds_tree_foreach(&(list)->sl_tree, p)

#define synclist_init(list, cmp, type, elem, fn)         \
        __synclist_init(list, cmp, offsetof(type, elem), fn)

void __synclist_init(
        synclist_t *list,
        ds_key_cmp_t *cmp,
        size_t cof,
        synclist_fn_t *fn);

/* Add an element to the synclist */
void *synclist_add(synclist_t *list, void *data);
/* Remove element from synclist */
void synclist_del(synclist_t *list, void *data);

/* Begin a synchronization cycle */
void synclist_begin(synclist_t *list);
/* End a synchronization cycle */
void synclist_end(synclist_t *list);

#endif /* SYNCLIST_H_INCLUDED */

