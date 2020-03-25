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

#include <stddef.h>

#include "log.h"
#include "ds_tree.h"

#include "synclist.h"

/*
 * Static initializer, this works in conjunction with the synclist_init() macro
 */
void __synclist_init(
        synclist_t *list,
        ds_key_cmp_t *cmp,
        size_t cof,
        synclist_fn_t *fn)
{
    memset(list, 0, sizeof(*list));

    /*
     * The tree node is the offset of the synclist_node plus the
     * offset of the tree_node inside the synclist_node
     */
    __ds_tree_init(
            &list->sl_tree,
            cmp,
            cof + offsetof(synclist_node_t, sl_tnode));

    list->sl_cof = cof;
    list->sl_fn = fn;
}

/*
 * Add an item to the sync list, return the pointer to the newly added item or a pointer to the current item on the
 * list.
 *
 * NULL is returned in case there was an error inserting/updating the entry.
 */
void *synclist_add(synclist_t *list, void *pnew)
{
    void *pold;
    void *pcur;
    synclist_node_t *psn;

    pold = ds_tree_find(&list->sl_tree, pnew);
    pcur = list->sl_fn(list, pold, pnew);

    /*
     * Depending on the return value of the callback, there are 4 possible
     * cases:
     *
     * |--------|-------|------------------------|
     * |  pcur  | pold  |  Action                |
     * |--------|-------|------------------------|
     * | !NULL  | NULL  | New element, insert    |
     * | !NULL  | !NULL | Update                 |
     * | NULL   | !NULL | Remove case            |
     * | NULL   | NULL  | No-op, discard element |
     * |--------|--------------------------------|
     *
     */

    /* New element, insert it */
    if (pcur != NULL && pold == NULL)
    {
        ds_tree_insert(&list->sl_tree, pcur, pcur);
    }
    /* Update case */
    else if (pcur != NULL && pold != NULL)
    {
        if (pcur != pold)
        {
            /*
             * This is a replace existing element operation. This is internally
             * realized as an add + remove operation. So the synclist callback
             * will be called 2 more times.
             *
             * The mechanic behind this is a bit counter intuitive, so use
             * at your own risk.
             *
             * It's probably much easier to copy data to pold rather than
             * replacing it.
             */
            list->sl_fn(list, pold, NULL);
            ds_tree_remove(&list->sl_tree, pold);

            pcur = list->sl_fn(list, NULL, pcur);
            if (pcur != NULL)
            {
                ds_tree_insert(&list->sl_tree, pcur, pcur);
            }
            return pcur;
        }
    }
    else if (pcur == NULL && pold != NULL)
    {
        ds_tree_remove(&list->sl_tree, pold);
        list->sl_fn(list, pold, NULL);
        return NULL;
    }
    else /* pcur == NULL && pold == NULL -- No op, pnew element */
    {
        return NULL;
    }

    /* Dereference the synclist_node_t structure */
    psn = CONT_TO_NODE(pcur, list->sl_cof);
    psn->sl_active = true;

    return pcur;
}

/*
 * Remove element from the list
 */
void synclist_del(synclist_t *list, void *item)
{
    void *obj;

    obj = ds_tree_find(&list->sl_tree, item);
    if (obj == NULL)
    {
        LOG(TRACE, "synclist: Error deleting element, not found in list.");
        return;
    }

    ds_tree_remove(&list->sl_tree, obj);

    /* Execute the callback */
    if (list->sl_fn(list, obj, NULL) != NULL)
    {
        LOG(TRACE, "synclist: Remove did not return NULL.");
    }
}

/*
 * Full list synchronization
 */
void synclist_begin(synclist_t *list)
{
    void *pdata;
    synclist_node_t *psl;

    ds_tree_foreach(&list->sl_tree, pdata)
    {
        psl = CONT_TO_NODE(pdata, list->sl_cof);
        psl->sl_active = false;
    }
}

/*
 * End list synchronization, purge elements that were not updated since the last _begin() function.
 *
 * An element is flagged as updated when synclist_add() is used on the element.
 */
void synclist_end(synclist_t *list)
{
    void *pdata;
    synclist_node_t *psl;
    ds_tree_iter_t iter;

    ds_tree_foreach_iter(&list->sl_tree, pdata, &iter)
    {
        psl = CONT_TO_NODE(pdata, list->sl_cof);

        if (psl->sl_active) continue;

        ds_tree_iremove(&iter);
        list->sl_fn(list, pdata, false);
    }
}
