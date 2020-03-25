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

#include <stdlib.h>

#include "ds_list.h"

static inline void ds_list_node_insert_after(
        ds_list_t* list,
        ds_list_node_t* after,
        ds_list_node_t *node);
static inline ds_list_node_t* ds_list_node_remove_after(
        ds_list_t* list,
        ds_list_node_t* after);

static inline ds_list_node_t* ds_list_node_next(ds_list_node_t *cur);
static inline void ds_list_node_insert_head(ds_list_t* list, ds_list_node_t* node);
static inline void ds_list_node_insert_tail(ds_list_t* list, ds_list_node_t* node);

/**
 * Insert an element at the current node's position (before the current node)
 */
static inline void ds_list_node_iinsert(ds_list_iter_t* iter, ds_list_node_t *node)
{
    ds_list_node_insert_after(iter->oli_list, iter->oli_prev, node);

    /* The previous node is now "node" */
    iter->oli_prev = node;
}

/*
 * ============================================================
 *  Basic API
 * ============================================================
 */

/**
 * Initialize linked list
 */
static inline void __ds_list_init(ds_list_t *list, size_t cof)
{
    list->ol_cof            = cof;
    list->ol_root.oln_next  = NULL;
    list->ol_tail           = NULL;
    list->ol_ndel           = 0;
}

/*
 * Return true if list is empty
 */
static inline bool ds_list_is_empty(ds_list_t *list)
{
    return (list->ol_root.oln_next == NULL);
}

/*
 * Return the next element in list
 */
static inline void* ds_list_next(ds_list_t *list, void *curr)
{
    ds_list_node_t *next = ds_list_node_next(CONT_TO_NODE(curr, list->ol_cof));

    return NODE_TO_CONT(next, list->ol_cof);
}

/**
 * Return the head of the list
 */
static inline void* ds_list_head(ds_list_t *list)
{
    return NODE_TO_CONT(list->ol_root.oln_next, list->ol_cof);
}

/**
 * Return the tail of the list
 */
static inline void* ds_list_tail(ds_list_t *list)
{
    /* If the tail points to the dummy element, the list is empty, therefore there is no TAIL element */
    if (list->ol_tail == NULL || list->ol_tail == &list->ol_root) return NULL;

    return NODE_TO_CONT(list->ol_tail, list->ol_cof);
}

/*
 * Insert to head of the list
 */
static inline void ds_list_insert_head(ds_list_t *list, void *data)
{
    ds_list_node_t *n = CONT_TO_NODE(data, list->ol_cof);

    ds_list_node_insert_head(list, n);
}

/*
 * Insert to tail of the list
 */
static inline void ds_list_insert_tail(ds_list_t *list, void *data)
{
    ds_list_node_t *n = CONT_TO_NODE(data, list->ol_cof);

    ds_list_node_insert_tail(list, n);
}

/*
 * Insert "data" after node "after"
 */
static inline void ds_list_insert_after(ds_list_t *list, void *after, void *data)
{
    ds_list_node_insert_after(
            list,
            CONT_TO_NODE(after, list->ol_cof),
            CONT_TO_NODE(data, list->ol_cof));
}

/*
 * Insert "data" after element "after" in list "list"
 */
static inline void *ds_list_remove_after(ds_list_t *list, void *after)
{
    ds_list_node_t *rm;

    rm = ds_list_node_remove_after(list, CONT_TO_NODE(after, list->ol_cof));

    return NODE_TO_CONT(rm, list->ol_cof);
}


/**
 * Remove the head of the list
 */
static inline void* ds_list_remove_head(ds_list_t* list)
{
    ds_list_node_t *rm;

    rm = ds_list_node_remove_after(list, &list->ol_root);

    return NODE_TO_CONT(rm, list->ol_cof);
}

/*
 * ============================================================
 *  Iterator API
 * ============================================================
 */
static inline void* ds_list_ifirst(ds_list_iter_t* iter, ds_list_t* list)
{
    iter->oli_list = list;
    iter->oli_prev = &list->ol_root;
    iter->oli_curr = list->ol_root.oln_next;
    iter->oli_ndel = list->ol_ndel;

    return NODE_TO_CONT(iter->oli_curr, list->ol_cof);
}

/**
 * Return the next node
 */
static inline void* ds_list_inext(ds_list_iter_t* iter)
{
    if (iter->oli_list->ol_ndel != iter->oli_ndel)
    {
        return DS_ITER_ERROR;
    }

    /* curr is NULL if it was removed. In such case, the prev stays the same */
    if (iter->oli_curr != NULL)
    {
        iter->oli_prev = iter->oli_curr;
        iter->oli_curr = ds_list_node_next(iter->oli_curr);
    }
    else
    {
        iter->oli_curr = ds_list_node_next(iter->oli_prev);
    }

    return NODE_TO_CONT(iter->oli_curr, iter->oli_list->ol_cof);
}

/*
 * Insert an element at the current position in the iterator
 */
static inline void* ds_list_iinsert(ds_list_iter_t *iter, void *data)
{
    if (iter->oli_list->ol_ndel != iter->oli_ndel)
    {
        return DS_ITER_ERROR;
    }

    if (iter->oli_curr == NULL) return NULL;

    ds_list_node_iinsert(iter, CONT_TO_NODE(data, iter->oli_list->ol_cof));

    return data;
}

/**
 * Remove the node at the current iterator position
 */
static inline void* ds_list_iremove(ds_list_iter_t* iter)
{
    if (iter->oli_list->ol_ndel != iter->oli_ndel)
    {
        return DS_ITER_ERROR;
    }

    ds_list_node_t* rm = iter->oli_curr;

    /* Current node was already removed once */
    if (iter->oli_curr == NULL) return NULL;

    ds_list_node_remove_after(iter->oli_list, iter->oli_prev);

    iter->oli_ndel++;

    iter->oli_curr->oln_next = NULL;
    iter->oli_curr = NULL;


    return NODE_TO_CONT(rm, iter->oli_list->ol_cof);
}

/*
 * ============================================================
 *  Low level functions
 * ============================================================
 */

/*
 * Insert @p node after element @p after
 */
static inline void ds_list_node_insert_after(ds_list_t* list, ds_list_node_t* after, ds_list_node_t *node)
{
    if (list->ol_tail == NULL)
    {
        after = list->ol_tail = &list->ol_root;
    }

    node->oln_next = after->oln_next;
    after->oln_next = node;

    /* Update list tail */
    if (node->oln_next == NULL)
    {
        list->ol_tail = node;
    }
}

/*
 * Remove the node after @p node
 */
static inline ds_list_node_t* ds_list_node_remove_after(ds_list_t* list, ds_list_node_t* after)
{
    ds_list_node_t *rm;

    if (after->oln_next == NULL) return NULL;

    rm = after->oln_next;

    after->oln_next = after->oln_next->oln_next;

    /* If the next element is NULL, this MUST be the last element in the list -> tail */
    if (after->oln_next == NULL)
    {
        list->ol_tail = after;
    }

    rm->oln_next = NULL;

    list->ol_ndel++;

    return rm;
}

/*
 * Get the next element in list
 */
static inline ds_list_node_t* ds_list_node_next(ds_list_node_t *cur)
{
    return cur == NULL ? NULL : cur->oln_next;
}

/**
 * Insert @p node before the first element in the list; @p node becomes the new head
 */
static inline void ds_list_node_insert_head(ds_list_t* list, ds_list_node_t* node)
{
    ds_list_node_insert_after(list, &list->ol_root, node);
}

/**
 * Insert @p node before the first element in the list; @p node becomes the new head
 */
static inline void ds_list_node_insert_tail(ds_list_t* list, ds_list_node_t* node)
{
    ds_list_node_insert_after(list, list->ol_tail, node);
}

