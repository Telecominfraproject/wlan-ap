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

#include <stdbool.h>
#include <string.h>

#include "ds_tree.h"

#define OTN_PROP_RED     1
#define OTN_PROP_BLACK   0

#define OTN_IS_RED(x)    ( (x) != NULL && ((x)->otn_prop == OTN_PROP_RED) )

/*
 * ============================================================
 *  Red-black tree implementation
 * ============================================================
 */

/**
 * Red-black tree run-time initializer
 */
void __ds_tree_init(ds_tree_t* root, ds_key_cmp_t* cmp_fn, size_t cof)
{
    root->ot_cof    = cof;
    root->ot_root   = NULL;
    root->ot_cmp_fn = cmp_fn;
    root->ot_str_fn = NULL;
    root->ot_ndel   = 0;
}

/*
 * ============================================================
 *  Static declarations
 * ============================================================
 */

static char *ds_tree_node_str(ds_tree_t *root, ds_tree_node_t *node);

/*
 * ============================================================
 *  Inline support functions for tree data structures
 * ============================================================
 */

/**
 * Standard binary search tree rotate
 *
 * @p dir is the direction of the rotation:
 *
 *  - 0: bring up the left child -> right rotation
 *  - 1: bring up the right child -> left rotation
 *
 *
 * Example of ration in the 0 (left side goes up = right rotation) direction:
 *
 *          node                                       save
 *
 *         /    \                                     /     \
 *                            left side up
 *      save   right          ==========>           A      node
 *
 *      / \      / \                                       /   \
 *
 *    A    B    C   D                                     B    right
 *
 *                                                             /    \
 *
 *                                                            C      D
 * @return
 * This function returns the new root of the subtree, either the left or the right node
 */
static inline ds_tree_node_t *ds_bst_rotate(ds_tree_t *root, ds_tree_node_t *node, int dir)
{
    ds_tree_node_t *save;

    save = node->otn_child[dir];
    save->otn_parent = node->otn_parent;

    node->otn_child[dir] = save->otn_child[!dir];
    if (node->otn_child[dir] != NULL) node->otn_child[dir]->otn_parent = node;

    save->otn_child[!dir] = node;
    node->otn_parent = save;

    /* save is the new subtree root, update the subtree's parent */
    if (save->otn_parent == NULL)
    {
        /* save is the new tree root */
        root->ot_root = save;
    }
    else
    {
        int dir = (node == save->otn_parent->otn_child[1]);
        save->otn_parent->otn_child[dir] = save;
    }

    return save;
}

/**
 * Red-black tree rotate, basically the same as the standard binary tree rotate with the
 * addition of repainting nodes.
 *
 * The new subtree root is painted black, the old root is painted red
 */
static inline ds_tree_node_t *ds_rbt_rotate(ds_tree_t *root, ds_tree_node_t *node, int dir)
{
    ds_tree_node_t *save = ds_bst_rotate(root, node, dir);

    node->otn_prop = OTN_PROP_RED;
    save->otn_prop = OTN_PROP_BLACK;

    return save;
}

/**
 * Double red-black rotation
 */
static inline ds_tree_node_t *ds_rbt_rotate_double(ds_tree_t *root, ds_tree_node_t *node, int dir)
{
    ds_rbt_rotate(root, node->otn_child[dir], !dir);

    return ds_rbt_rotate(root, node, dir);
}

/*
 * ============================================================
 *  Insertion
 * ============================================================
 */

/**
 * Test and fix a red violation around node @p base that might happen during insert
 */
static inline void ds_tree_insert_rebalance(ds_tree_t *root, ds_tree_node_t *base)
{
    /* A color flip or an insert may cause a red violation, fix it here */
    if (OTN_IS_RED(base) && OTN_IS_RED(base->otn_parent))
    {
        /* Get the grand parent */
        ds_tree_node_t *gbase = base->otn_parent->otn_parent;
        /* Figure out the direction of the parent compared to the grand parent */
        int gdir = (base->otn_parent == gbase->otn_child[1]);
        /* Figure out the direction of the base compared to the parent */
        int pdir = (base == base->otn_parent->otn_child[1]);

        /*
         * Do a rotation around the grand parent. Use a single rotation if the branch
         * grandparent -> parent -> base is a straight branch or a double rotation
         * for twisted branches
         */
        if (gdir == pdir)
        {
            //printf("Rotating at %s\n", (char *) gbase->otn_key);
            ds_rbt_rotate(root, gbase, gdir);
        }
        else
        {
            //printf("Double rotate\n");
            ds_rbt_rotate_double(root, gbase, gdir);
        }
    }

}
/**
 * Insert node @p node with key @p key into the tree @p root
 *
 * This function uses the single pass (top-down) red-black tree insertion algorithm
 */
void ds_tree_node_insert(ds_tree_t *root, ds_tree_node_t* node, void *key)
{
    /*
     * Initialize node
     */
    memset(node, 0, sizeof(*node));
    node->otn_key = key;

    /* Start by inserting a RED node */
    node->otn_prop = OTN_PROP_RED;

    /* Special case, inserting root */
    if (root->ot_root == NULL)
    {
        root->ot_root = node;
        node->otn_prop = OTN_PROP_BLACK;
        return;
    }

    /*
     * Binary Search Tree insert; descend until we find a node with a NULL child
     * in the direction we are descending.
     */
    ds_tree_node_t *base = root->ot_root;

    int dir = root->ot_cmp_fn(base->otn_key, key) < 0;

    /*
     * Traverse down the tree until we find a free leaf node; do the red-black magic
     * on the way down
     */
    while (base->otn_child[dir] != NULL)
    {
        /* Flip colors when both children are red */
        if (OTN_IS_RED(base->otn_child[0]) && OTN_IS_RED(base->otn_child[1]))
        {
            base->otn_prop = OTN_PROP_RED;
            base->otn_child[0]->otn_prop = OTN_PROP_BLACK;
            base->otn_child[1]->otn_prop = OTN_PROP_BLACK;
        }

        /* Fix any red violations that might have happened due to the color flip */
        ds_tree_insert_rebalance(root, base);

        /* Descend the tree */
        base = base->otn_child[dir];
        dir  = root->ot_cmp_fn(base->otn_key, key) < 0;
    }

    /* Insert node */
    base->otn_child[dir] = node;
    node->otn_parent = base;

    /* An insertion may cause a red violation, fix it */
    ds_tree_insert_rebalance(root, base->otn_child[dir]);

    /* Always paint root black */
    root->ot_root->otn_prop = OTN_PROP_BLACK;
}
/*
 * ============================================================
 *  Removal
 * ============================================================
 */

/**
 * Remove a leaf node which is a node that has at least 1 child NULL
 */
static inline ds_tree_node_t *ds_tree_remove_leaf(ds_tree_t *root, ds_tree_node_t *node)
{
    ds_tree_node_t *retval = node->otn_parent;

    int dir = (node->otn_child[0] == NULL);

    /*
     * Handle red-black tree exceptions here
     */
    if (OTN_IS_RED(node))
    {
        retval = NULL;

    }
    else if (OTN_IS_RED(node->otn_child[dir]))
    {
        node->otn_child[dir]->otn_prop = OTN_PROP_BLACK;
        retval = NULL;
    }

    /*
     * Remove the node
     */
    if (node->otn_parent == NULL)
    {
        root->ot_root = node->otn_child[dir];
    }
    else
    {
        /* Direction from the parent to node */
        int pdir = (node == node->otn_parent->otn_child[1]);
        node->otn_parent->otn_child[pdir] = node->otn_child[dir];
    }

    if (node->otn_child[dir] != NULL)
    {
        node->otn_child[dir]->otn_parent = node->otn_parent;
    }

    return retval;
}

/**
 * Replace node @p pold with node @p pnew
 */
static inline void ds_tree_node_replace(ds_tree_t *root, ds_tree_node_t *pold, ds_tree_node_t *pnew)
{
    pnew->otn_prop     = pold->otn_prop;
    pnew->otn_parent   = pold->otn_parent;
    pnew->otn_child[0] = pold->otn_child[0];
    pnew->otn_child[1] = pold->otn_child[1];

    if (pold->otn_child[0] != NULL) pold->otn_child[0]->otn_parent = pnew;
    if (pold->otn_child[1] != NULL) pold->otn_child[1]->otn_parent = pnew;

    if (pold->otn_parent == NULL)
    {
        root->ot_root = pnew;
    }
    else
    {
        int pdir = (pold == pold->otn_parent->otn_child[1]);

        pold->otn_parent->otn_child[pdir] = pnew;
    }
}

/**
 * Tree rebalancing code after node removal
 */
static inline bool ds_tree_remove_rebalance(ds_tree_t *root, ds_tree_node_t *node, int dir)
{
    ds_tree_node_t *s = node->otn_child[!dir];

    /* Red sibling case */
    if (OTN_IS_RED(s))
    {
        ds_rbt_rotate(root, node, !dir);

        /* Recalculate the new sibling */
        s = node->otn_child[!dir];
    }

    if (s != NULL)
    {
        if (!OTN_IS_RED(s->otn_child[0]) && !OTN_IS_RED(s->otn_child[1]))
        {
            s->otn_prop = OTN_PROP_RED;

            if (OTN_IS_RED(node))
            {
                node->otn_prop = OTN_PROP_BLACK;
                return false;
            }
        }
        else
        {
            int prop = node->otn_prop;

            if (OTN_IS_RED(s->otn_child[!dir]))
            {
                node = ds_rbt_rotate(root, node, !dir);
            }
            else
            {
                node = ds_rbt_rotate_double(root, node, !dir);
            }

            node->otn_prop = prop;
            node->otn_child[0]->otn_prop = OTN_PROP_BLACK;
            node->otn_child[1]->otn_prop = OTN_PROP_BLACK;

            return false;
        }
    }

    return true;
}

/**
 * Remove node @p node from tree @p root; use the bottom-up algorithm
 *
 * This function does use the top-down removal algorithm as it does not make much
 * sense with an implementation with parent pointers
 */
void ds_tree_node_remove(ds_tree_t *root, ds_tree_node_t *node)
{
    ds_tree_node_t*     base;
    int                 dir = -1;

    if (node->otn_child[0] != NULL && node->otn_child[1] != NULL)
    {
        /*
         * We're removing an inner node, in such case, find the previous or next
         * node, which is guaranteed to be a leaf, and replace it with the current node.
         */
        ds_tree_node_t *rnode;

        rnode = node->otn_child[0];
        /* Find the previous node */
        while (rnode->otn_child[1] != NULL) rnode = rnode->otn_child[1];

        /* rnode can be node->otn_child[0], in which case dir should be 0 */
        dir = (rnode->otn_parent->otn_child[1] == rnode);

        /* Remove rnode from the tree */
        base = ds_tree_remove_leaf(root, rnode);

        /* Replace node with rnode */
        ds_tree_node_replace(root, node, rnode);

        /* This happens if rnode is a child of node */
        if (base == node) base = rnode;
    }
    else
    {
        if (node->otn_parent != NULL) dir = (node == node->otn_parent->otn_child[1]);
        base = ds_tree_remove_leaf(root, node);
    }

    while (base != NULL)
    {
        if (!ds_tree_remove_rebalance(root, base, dir))
        {
            break;
        }

        /* Climb up the tree, recalculate the direction */
        if (base->otn_parent != NULL) dir = (base == base->otn_parent->otn_child[1]);
        base = base->otn_parent;
    }

    /* Always repaint root to be black */
    if (root->ot_root != NULL) root->ot_root->otn_prop = OTN_PROP_BLACK;

    node->otn_child[0] = node->otn_child[1] = NULL;

    root->ot_ndel++;
}

/*
 * ============================================================
 *  Debug function for checking tree health and visualization
 * ============================================================
 */

/**
 * Recursive part of @ref ds_tree_check()
 */
int ds_tree_check_r(ds_tree_t *root, ds_tree_node_t *node)
{
    int rc[2] = { 0, 0 };

    int c;
    /* Check if both children's parents point to node */
    for (c = 0; c < 2; c++)
    {
        if (node->otn_child[c] != NULL)
        {
            if (node->otn_child[c]->otn_parent != node) return -1;

            rc[c] = ds_tree_check_r(root, node->otn_child[c]);

            if (rc[c] < 0) return -1;
        }
    }

    /*
     * Red-black specifics
     */

    /* Black violation: Each path in the subtrees must have the same number of black nodes */
    if (rc[0] != rc[1])
    {
        printf("Black violation at node %s\n", ds_tree_node_str(root, node));
        return -1;
    }

    /* Red-violation */
    if (!OTN_IS_RED(node))
    {
        return rc[0] + 1;
    }

    /* Left node red violation */
    if (OTN_IS_RED(node->otn_child[0]))
    {
        printf("Red violation at node: %s\n", ds_tree_node_str(root, node));
        return -1;
    }

    /* Right node red violation */
    if (OTN_IS_RED(node->otn_child[1]))
    {
        printf("Red violation at node: %s\n", ds_tree_node_str(root, node));
        return -1;
    }

    return rc[0];
}

/**
 * Check tree health
 */
int ds_tree_check(ds_tree_t *root)
{
    if (root->ot_root != NULL)
    {
        if (OTN_IS_RED(root->ot_root))
        {
            printf("Black-root violation\n");
            return -1;
        }

        return ds_tree_check_r(root, root->ot_root);
    }

    return 0;
}

/**
 * Return a string representation of node's @p node key; primarily used for debugging
 */
char *ds_tree_node_str(ds_tree_t *root, ds_tree_node_t *node)
{
    if (root->ot_str_fn != NULL)
    {
        return root->ot_str_fn(node);
    }

    static char node_ptr[22];

    snprintf(node_ptr, sizeof(node_ptr), "%p", node->otn_key);

    return node_ptr;
}

/**
 * Recursive part of @ref ds_tree_graphviz()
 */
void ds_tree_graphviz_r(ds_tree_t *root, FILE *fdot, ds_tree_node_t *node)
{
    char snode[DS_TREE_NODE_STR_SZ];
    char schild[DS_TREE_NODE_STR_SZ];
    int c;

    snprintf(snode, sizeof(snode), "%s", ds_tree_node_str(root, node));

    fprintf(fdot, "%s [fillcolor=%s];\n", snode, OTN_IS_RED(node) ? "red" : "black");


    for (c = 0; c < 2; c++)
    {
        if (node->otn_child[c] != NULL)
        {
            snprintf(schild, sizeof(schild), "%s", ds_tree_node_str(root, node->otn_child[c]));
            /* Recurs down */
            ds_tree_graphviz_r(root, fdot, node->otn_child[c]);
        }
        else
        {
            static int nullcnt = 0;
            nullcnt++;

            snprintf(schild, sizeof(schild), "null%d", nullcnt);

            fprintf(fdot, "null%d [shape=point];\n", nullcnt);
        }

        fprintf(fdot, "%s -> %s [color=%s, arrowhead=%s];\n", snode, schild, c == 0 ? "purple" : "green", c == 0 ? "rnormal" : "lnormal");
    }

}

/**
 * Dump the tree in a graphviz DOT file format, so it can be visualized with the "dot"
 * utility
 */
void ds_tree_graphviz(ds_tree_t *root, FILE *fdot)
{
    fprintf(fdot, "digraph red_black_tree {\n");
    /* Prettier graphs */
    fprintf(fdot, "nodesep = 0.3; ranksep = 0.2; margin = 0.1; node [shape=box, style=\"rounded,filled\", fontcolor=white];\n\n");

    fprintf(fdot, "%s;\n", ds_tree_node_str(root, root->ot_root));

    ds_tree_graphviz_r(root, fdot, root->ot_root);

    fprintf(fdot, "}\n");
}


/**
 * Integer comparator
 */
int ds_int_cmp(void *_a, void *_b)
{
    int *a = _a;
    int *b = _b;
    return *a - *b;
}

/**
 * Pointer comparison (the key value is stored directly)
 */
int ds_void_cmp(void *a, void *b)
{
    return (int)((intptr_t)a - (intptr_t)b);
}


/**
 * String comparator
 */
int ds_str_cmp(void *a, void *b)
{
    return strcmp((const char *)a, (const char *)b);
}
