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

#ifndef DS_TREE_H_INCLUDED
#define DS_TREE_H_INCLUDED

#include <stdio.h>
#include <stddef.h>

#include "ds.h"

/*
 * ============================================================
 * ACLA Data Structures: Red Black Trees
 * ============================================================
 */

#define DS_TREE_INIT_EX(C, S, type, elem)   \
{                                           \
    .ot_cof    = offsetof(type, elem),      \
    .ot_root   = NULL,                      \
    .ot_cmp_fn = (C),                       \
    .ot_str_fn = (S),                       \
    .ot_ndel   = 0,                         \
}

#define DS_TREE_INIT(C, type, elem)             DS_TREE_INIT_EX((C), NULL, type, elem)

#define ds_tree_init(root, cmp, type, elem)     __ds_tree_init(root, cmp, offsetof(type, elem))

#define DS_TREE_NODE_STR_SZ                     64  /**< Maximum size of node string */

#define ds_tree_foreach(tree, p)       \
    for (p = ds_tree_head(tree); p != NULL; p = ds_tree_next(tree, p))

#define ds_tree_foreach_iter(tree, p, iter) \
    for (p = ds_tree_ifirst(iter, tree); p != NULL; p = ds_tree_inext(iter))

typedef struct ds_tree_node ds_tree_node_t;
typedef struct ds_tree ds_tree_t;
typedef struct ds_tree_iter ds_tree_iter_t;

/**
 * Return a string representation of node @p node. Mainly used for debugging and drawing
 * dot graphs.
 *
 * @note Callers should be aware that it is assumed that this function can safely
 * return a pointer to a static buffer.
 */
typedef char *ds_tree_str_t(ds_tree_node_t *node);

/**
 * Tree node
 */
struct ds_tree_node
{
    void*               otn_key;            /**< Node key                   */
    int                 otn_prop;           /**< Node property, color       */
    ds_tree_node_t*     otn_parent;         /**< Node's parent pointer      */
    ds_tree_node_t*     otn_child[2];       /**< Node children pointers     */
};

/**
 * This structure defines a tree root
 */
struct ds_tree
{
    size_t              ot_cof;             /**< Container offset           */
    ds_tree_node_t*     ot_root;            /**< Root pointer               */
    ds_key_cmp_t*       ot_cmp_fn;          /**< Compare function           */
    ds_tree_str_t*      ot_str_fn;          /**< Node to string function    */
    uint32_t            ot_ndel;            /**< Number of delete operations
                                                 This is used by iterators. */
};

/**
 * Iterator structure
 */
struct ds_tree_iter
{
    ds_tree_t           *oti_root;
    ds_tree_node_t      *oti_curr;
    ds_tree_node_t      *oti_next;
    uint32_t            oti_ndel;           /**< oti_ndel and ds_tree->ot_ndel must match exactly,
                                                 otherwise assume somebody removed an element while
                                                 we were looping through it */
};

/*
 * ===========================================================================
 *  Public API
 * ===========================================================================
 */
static inline bool   ds_tree_is_empty(ds_tree_t *root);
static inline void  *ds_tree_next(ds_tree_t *root, void *data);
static inline void  *ds_tree_prev(ds_tree_t *root, void *data);
static inline void   ds_tree_insert(ds_tree_t *root, void *data, void *key);
static inline void  *ds_tree_find(ds_tree_t *root, void *key);
static inline void  *ds_tree_remove(ds_tree_t *root, void *data);

/*
 * ===========================================================================
 *  Iterator API
 * ===========================================================================
 */
static inline void  *ds_tree_ifirst(ds_tree_iter_t *iter, ds_tree_t *root);
static inline void  *ds_tree_inext(ds_tree_iter_t *iter);
static inline void  *ds_tree_iremove(ds_tree_iter_t *iter);

extern void         __ds_tree_init(ds_tree_t* root, ds_key_cmp_t *cmp_fn, size_t cof);
extern int          ds_tree_check(ds_tree_t* root);
extern void         ds_tree_graphviz(ds_tree_t* root, FILE* fdot);

#include "../src/ds_tree.c.h"

#endif /* DS_TREE_H_INCLUDED */
