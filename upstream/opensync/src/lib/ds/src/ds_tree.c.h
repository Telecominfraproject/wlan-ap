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

/*
 * ============================================================
 *  Inline functions
 * ============================================================
 */
#include <stdbool.h>
#include <string.h>

#include "ds_tree.h"

static inline void              *ds_tree_node_key(ds_tree_node_t *node);
static inline ds_tree_node_t    *ds_tree_node_next(ds_tree_node_t *node);
static inline ds_tree_node_t    *ds_tree_node_prev(ds_tree_node_t *node);
static inline ds_tree_node_t    *ds_tree_node_min(ds_tree_node_t *node);
static inline ds_tree_node_t    *ds_tree_node_max(ds_tree_node_t *node);
extern void                      ds_tree_node_insert(ds_tree_t* root, ds_tree_node_t* node, void* key);
extern void                      ds_tree_node_remove(ds_tree_t* root, ds_tree_node_t* node);

/*
 * ===========================================================================
 *  Public API
 * ===========================================================================
 */

/**
 * Find the node corresponding to the node @p key in the tree @p root
 *
 * @return
 * This function returns they node that corresponds to key @p key or NULL if not found
 */
static inline void* ds_tree_find(ds_tree_t *root, void *key)
{
    ds_tree_node_t *node = root->ot_root;

    while (node != NULL)
    {
        int c = root->ot_cmp_fn(node->otn_key, key);

        if (c == 0) break;

        /* Move down the tree, depending on c */
        node = node->otn_child[c < 0];
    }

    return NODE_TO_CONT(node, root->ot_cof);
}

/**
 * Return true if tree is empty
 */
static inline bool ds_tree_is_empty(ds_tree_t *root)
{
    return (root->ot_root == NULL);
}

/*
 * Return the head of the tree -- the lowest member in the tree
 */
static inline void *ds_tree_head(ds_tree_t *root)
{
    /* The head of the tree is the lowest element in the tree, counted from below the root element */
    ds_tree_node_t *node = ds_tree_node_min(root->ot_root);

    return NODE_TO_CONT(node, root->ot_cof);
}

/*
 * Return the tail of the tree -- the highest member in the tree
 */
static inline void *ds_tree_tail(ds_tree_t *root)
{
    /* The head of the tree is the highest element in the tree, counted from below the root element */
    ds_tree_node_t *node = ds_tree_node_max(root->ot_root);

    return NODE_TO_CONT(node, root->ot_cof);
}


/*
 * Return the next element in the list
 */
static inline void *ds_tree_next(ds_tree_t *root, void *data)
{
    ds_tree_node_t *node = CONT_TO_NODE(data, root->ot_cof);

    ds_tree_node_t *next = ds_tree_node_next(node);

    return NODE_TO_CONT(next, root->ot_cof);
}

/*
 * Return the next element in the list
 */
static inline void *ds_tree_prev(ds_tree_t *root, void *data)
{
    ds_tree_node_t *node = CONT_TO_NODE(data, root->ot_cof);

    ds_tree_node_t *next = ds_tree_node_prev(node);

    return NODE_TO_CONT(next, root->ot_cof);
}

/*
 * Insert an element into the tree
 */
static inline void ds_tree_insert(ds_tree_t *root, void *data, void *key)
{
    ds_tree_node_t *node = CONT_TO_NODE(data, root->ot_cof);

    ds_tree_node_insert(root, node, key);
}

/*
 * Remove an element from the tree
 */
static inline void *ds_tree_remove(ds_tree_t *root, void *data)
{
    ds_tree_node_t *node = CONT_TO_NODE(data, root->ot_cof);

    ds_tree_node_remove(root, node);

    return data;
}

/*
 * Remove the head of the tree (lowest member)
 */
static inline void *ds_tree_remove_head(ds_tree_t *root)
{
    ds_tree_node_t *head = ds_tree_node_min(root->ot_root);

    if (head == NULL) return NULL;

    ds_tree_node_remove(root, head);

    return NODE_TO_CONT(head, root->ot_cof);
}

/*
 * Remove the tail of the tree (highest member)
 */
static inline void *ds_tree_remove_tail(ds_tree_t *root)
{
    ds_tree_node_t *tail = ds_tree_node_max(root->ot_root);

    if (tail == NULL) return NULL;

    ds_tree_node_remove(root, tail);

    return NODE_TO_CONT(tail, root->ot_cof);
}


/*
 * ============================================================
 *  Iterators
 * ============================================================
 */

/**
 * Initialize the @p iter strucure, @p iter will point to the first element in the tree
 */
static inline void* ds_tree_ifirst(ds_tree_iter_t *iter, ds_tree_t *root)
{
    memset(iter, 0, sizeof(*iter));

    iter->oti_root = root;
    iter->oti_ndel = root->ot_ndel;

    if (root->ot_root == NULL) return NULL;

    iter->oti_curr = ds_tree_node_min(root->ot_root);
    iter->oti_next = ds_tree_node_next(iter->oti_curr);

    return NODE_TO_CONT(iter->oti_curr, root->ot_cof);
}

/**
 * Retrieve the next node
 */
static inline void* ds_tree_inext(ds_tree_iter_t *iter)
{
    if (iter->oti_ndel != iter->oti_root->ot_ndel)
    {
        return DS_ITER_ERROR;
    }

    iter->oti_curr = iter->oti_next;
    if (iter->oti_curr == NULL) return NULL;

    iter->oti_next = ds_tree_node_next(iter->oti_curr);

    return NODE_TO_CONT(iter->oti_curr, iter->oti_root->ot_cof);
}

/**
 * Retrieve the next node while deleting the current node
 */
static inline void* ds_tree_iremove(ds_tree_iter_t *iter)
{
    if (iter->oti_ndel != iter->oti_root->ot_ndel)
    {
        return DS_ITER_ERROR;
    }

    /* Element was already removed once -- or we're at the end of the list */
    if (iter->oti_curr == NULL)
    {
        return NULL;
    }

    ds_tree_node_t *curr = iter->oti_curr;
    iter->oti_curr = NULL;

    ds_tree_node_remove(iter->oti_root, curr);

    iter->oti_ndel++;

    return NODE_TO_CONT(curr, iter->oti_root->ot_cof);
}

/*
 * ===========================================================================
 *  Support functions
 * ===========================================================================
 */

/**
 * Return a node key
 */
static inline void *ds_tree_node_key(ds_tree_node_t *node)
{
    return node->otn_key;
}

/**
 * Find the lexicographically smallest element in the subtree of which @p node is the
 * root
 */
static inline ds_tree_node_t *ds_tree_node_min(ds_tree_node_t *node)
{
    if (node == NULL) return NULL;

    ds_tree_node_t *min = node;
    while (min->otn_child[0] != NULL) min = min->otn_child[0];

    return min;
}

/**
 * Find the lexicographically highest element in the subtree of which @p node is the
 * root
 */
static inline ds_tree_node_t *ds_tree_node_max(ds_tree_node_t *node)
{
    if (node == NULL) return NULL;

    ds_tree_node_t *max = node;
    while (max->otn_child[1] != NULL) max = max->otn_child[1];

    return max;
}

/**
 * Retrieve the next node
 */
static inline ds_tree_node_t *ds_tree_node_next(ds_tree_node_t *node)
{
    if (node == NULL) return NULL;

    if (node->otn_child[1] != NULL)
    {
        node = ds_tree_node_min(node->otn_child[1]);
    }
    else
    {
        int dir;

        /* Climb up the tree; next is the left node, break out */
        while (node != NULL)
        {
            dir = (node->otn_parent != NULL) && (node == node->otn_parent->otn_child[1]);

            node = node->otn_parent;

            if (dir == 0) break;
        }
    }

    return node;
}

/**
 * Retrieve the previous node
 */
static inline ds_tree_node_t *ds_tree_node_prev(ds_tree_node_t *node)
{
    if (node == NULL) return NULL;

    if (node->otn_child[0] != NULL)
    {
        node = ds_tree_node_max(node->otn_child[0]);
    }
    else
    {
        int dir;

        /* Climb up the tree; if we're parent's left node, break */
        while (node != NULL)
        {
            dir = (node->otn_parent != NULL) && (node == node->otn_parent->otn_child[0]);

            node = node->otn_parent;

            if (dir == 0) break;
        }
    }

    return node;
}
