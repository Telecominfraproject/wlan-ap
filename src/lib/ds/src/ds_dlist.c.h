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

#include "ds_dlist.h"

static inline void ds_dlist_insert_into(ds_dlist_t* list, ds_dlist_node_t** into, ds_dlist_node_t* node);
static inline void ds_dlist_node_remove(ds_dlist_t* list, ds_dlist_node_t* node);

/*
 * ============================================================
 *  Basic API
 * ============================================================
 */

/**
 * Initialize a double linked list
 */
static inline void __ds_dlist_init(ds_dlist_t* list, size_t cof)
{
    list->od_cof  = cof;
    list->od_head = list->od_tail = NULL;
    list->od_ndel = 0;
}

/*
 * Return true if dlist is empty
 */
static inline bool ds_dlist_is_empty(ds_dlist_t *list)
{
    return list->od_head == NULL;
}

/*
 * Return next element in list
 */
static inline void *ds_dlist_next(ds_dlist_t *list, void *data)
{
    ds_dlist_node_t *n = CONT_TO_NODE(data, list->od_cof);

    return NODE_TO_CONT(n->odn_next, list->od_cof);
}

/*
 * Return previous element
 */
static inline void *ds_dlist_prev(ds_dlist_t *list, void *data)
{
    ds_dlist_node_t *n = CONT_TO_NODE(data, list->od_cof);

    return NODE_TO_CONT(n->odn_prev, list->od_cof);
}

/**
 * Return the head of the list
 */
static inline void* ds_dlist_head(ds_dlist_t *list)
{
    return NODE_TO_CONT(list->od_head, list->od_cof);
}

/**
 * Return the tail of the list
 */
static inline void* ds_dlist_tail(ds_dlist_t *list)
{
    return NODE_TO_CONT(list->od_tail, list->od_cof);
}

/**
 * Insert element @p cont before the first element in the list; @p cont becomes the new head
 */
static inline void ds_dlist_insert_head(ds_dlist_t* list, void *data)
{
    ds_dlist_insert_into(list, &list->od_head, CONT_TO_NODE(data, list->od_cof));
}

/*
 * Insert after the current element
 */
static inline void ds_dlist_insert_after(ds_dlist_t *list, void *after, void *data)
{
    ds_dlist_node_t *nafter = CONT_TO_NODE(after, list->od_cof);
    ds_dlist_node_t *ndata = CONT_TO_NODE(data, list->od_cof);

    ds_dlist_insert_into(
            list,
            &nafter->odn_next,
            ndata);
}

/*
 * Insert before the current element
 */
static inline void ds_dlist_insert_before(ds_dlist_t *list, void *before, void *data)
{
    ds_dlist_node_t *nbefore = CONT_TO_NODE(before, list->od_cof);

    if (nbefore->odn_prev == NULL)
    {
        ds_dlist_insert_head(list, data);
    }
    else
    {
        ds_dlist_node_t *ndata = CONT_TO_NODE(data, list->od_cof);
        ds_dlist_insert_into(
                list,
                &nbefore->odn_prev->odn_next,
                ndata);
    }
}

/**
 * Insert elemt @p cont at the tail of the list; @p cont becomes the new tail
 */
static inline void ds_dlist_insert_tail(ds_dlist_t* list, void *data)
{
    /*
     * If the tail is null, it means the list is empty. Reduce to the insert-head case.
     */
    if (list->od_tail == NULL)
    {
        ds_dlist_insert_head(list, data);
    }
    else
    {
        ds_dlist_node_t *node = CONT_TO_NODE(data, list->od_cof);
        ds_dlist_insert_into(list, &list->od_tail->odn_next, node);
    }
}

/*
 * Remove an element from the list
 */
static inline void ds_dlist_remove(ds_dlist_t* list, void *data)
{
    ds_dlist_node_remove(list, CONT_TO_NODE(data, list->od_cof));
}

/**
 * Remove the head of the list
 */
static inline void* ds_dlist_remove_head(ds_dlist_t* list)
{
    ds_dlist_node_t* rm = list->od_head;

    if (rm == NULL) return NULL;

    ds_dlist_node_remove(list, list->od_head);

    return NODE_TO_CONT(rm, list->od_cof);
}

/**
 * Remove the tail of the list
 */
static inline void* ds_dlist_remove_tail(ds_dlist_t* list)
{
    ds_dlist_node_t* rm = list->od_tail;

    if (rm == NULL) return NULL;

    ds_dlist_node_remove(list, list->od_tail);

    return NODE_TO_CONT(rm, list->od_cof);
}

/*
 * Remove the next element
 */
static inline void *ds_dlist_remove_after(ds_dlist_t *list, void *after)
{
    ds_dlist_node_t *anode = CONT_TO_NODE(after, list->od_cof);
    ds_dlist_node_t *node = anode->odn_next;

    if (node == NULL) return NULL;

    ds_dlist_node_remove(list, node);

    return NODE_TO_CONT(node, list->od_cof);
}

/*
 * Remove the next element
 */
static inline void *ds_dlist_remove_before(ds_dlist_t *list, void *before)
{
    ds_dlist_node_t *bnode = CONT_TO_NODE(before, list->od_cof);
    ds_dlist_node_t *node = bnode->odn_prev;

    if (node == NULL) return NULL;

    ds_dlist_node_remove(list, node);

    return NODE_TO_CONT(node, list->od_cof);
}

/*
 * ============================================================
 *  Iterator API
 * ============================================================
 */
static inline void* ds_dlist_ifirst(ds_dlist_iter_t* iter, ds_dlist_t* list)
{
    iter->odi_list = list;
    iter->odi_curr = list->od_head;
    iter->odi_next = iter->odi_curr == NULL ? NULL : iter->odi_curr->odn_next;
    iter->odi_ndel = list->od_ndel;

    return NODE_TO_CONT(iter->odi_curr, list->od_cof);
}

/**
 * Return the next node
 */
static inline void* ds_dlist_inext(ds_dlist_iter_t* iter)
{
    if (iter->odi_list->od_ndel != iter->odi_ndel)
    {
        return DS_ITER_ERROR;
    }

    iter->odi_curr = iter->odi_next;

    if (iter->odi_curr == NULL) return NULL;

    iter->odi_next = iter->odi_curr->odn_next;

    return NODE_TO_CONT(iter->odi_curr, iter->odi_list->od_cof);
}

static inline void* ds_dlist_iinsert(ds_dlist_iter_t *iter, void *data)
{
    if (iter->odi_list->od_ndel != iter->odi_ndel)
    {
        return DS_ITER_ERROR;
    }

    if (iter->odi_curr == NULL) return NULL;

    ds_dlist_insert_before(
            iter->odi_list,
            NODE_TO_CONT(iter->odi_curr, iter->odi_list->od_cof),
            data);

    return data;
}

/**
 * Remove the current iteration element and return the next element;
 */
static inline void* ds_dlist_iremove(ds_dlist_iter_t* iter)
{
    if (iter->odi_list->od_ndel != iter->odi_ndel)
    {
        return DS_ITER_ERROR;
    }

    /* Element was already removed once */
    if (iter->odi_curr == NULL) return NULL;

    ds_dlist_node_t *node = iter->odi_curr;
    iter->odi_curr = NULL;

    ds_dlist_node_remove(iter->odi_list, node);
    iter->odi_ndel++;

    return NODE_TO_CONT(node, iter->odi_list->od_cof);
}

/*
 * ============================================================
 *  Support functions
 * ============================================================
 */

/**
 * Insert node @p node before node @p into
 */
static inline void ds_dlist_insert_into(ds_dlist_t* list,
                                        ds_dlist_node_t** into,
                                        ds_dlist_node_t* node)
{
    node->odn_next = *into;
    *into = node;

    /*
     * Update the next and previous nodes
     */

    /* If the next node is NULL, it means that @p node is the new tail */
    if (node->odn_next == NULL)
    {
        node->odn_prev = list->od_tail;
        list->od_tail = node;
    }
    else
    {
        /* Update previous pointers */
        node->odn_prev = node->odn_next->odn_prev;
        node->odn_next->odn_prev = node;
    }
}

/**
 * Remove node @p node from list @p list
 */
static inline void ds_dlist_node_remove(ds_dlist_t* list, ds_dlist_node_t* node)
{
    if (node->odn_prev == NULL)
    {
        list->od_head = node->odn_next;
    }
    else
    {
        node->odn_prev->odn_next = node->odn_next;
    }

    if (node->odn_next == NULL)
    {
        list->od_tail = node->odn_prev;
    }
    else
    {
        node->odn_next->odn_prev = node->odn_prev;
    }

    node->odn_next = node->odn_prev = NULL;

    list->od_ndel++;
}

