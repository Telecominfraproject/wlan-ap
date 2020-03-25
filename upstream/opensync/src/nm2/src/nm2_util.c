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

#include "const.h"
#include "nm2_util.h"

/*
 * ===========================================================================
 *  UUIDSET module
 * ===========================================================================
 */

static ds_key_cmp_t uuidset_cmp;
static synclist_fn_t uuid_synclist_fn;
static int uuidset_cmp(void *_a, void *_b);
static void *uuid_synclist_fn(synclist_t *list, void *_old, void *_new);
static void uuidset_reflink_fn(reflink_t *obj, reflink_t *remote);

/*
 * Initialize an uuidset object
 */
void uuidset_init(
        uuidset_t *us,
        const char *name,
        uuidset_getref_fn_t *getref,
        uuidset_update_fn_t *update)
{
    memset(us, 0, sizeof(*us));

    us->us_getref_fn = getref;
    us->us_update_fn = update;

    reflink_init(&us->us_reflink, name);
    reflink_set_fn(&us->us_reflink, uuidset_reflink_fn);

    synclist_init(
            &us->us_list,
            uuidset_cmp,
            struct uuidset_node,
            un_snode,
            uuid_synclist_fn);
}

/*
 * Destroy a uuidset object. Any active reflink connection will be automatically unreferenced.
 */
void uuidset_fini(uuidset_t *us)
{
    /* Clear the list, disconnect all reflinks */
    uuidset_set(us, NULL, 0);

    reflink_fini(&us->us_reflink);
}

/*
 * Update the uuidset with an UUID array
 */
bool uuidset_set(uuidset_t *us, ovs_uuid_t *elem, int nelem)
{
    int ii;
    struct uuidset_node un;

    bool retval = true;

    synclist_begin(&us->us_list);

    for (ii = 0; ii < nelem; ii++)
    {
        un.un_uuid = elem[ii];
        synclist_add(&us->us_list, &un);
    }

    synclist_end(&us->us_list);

    return retval;
}

/*
 * Synchronized list callback;
 *
 * This function is responsible for allocating uuidset_node structures and establishing
 * reflinks with the parent object.
 *
 * us_getref_fn() is require here to acquire a mapping between UUID and the reflink of
 * the object represented by UUID
 */
void *uuid_synclist_fn(synclist_t *list, void *_old, void *_new)
{
    struct uuidset_node *node;
    reflink_t *ref;
    bool insert;

    /* Insert case */
    if (_old == NULL)
    {
        insert = true;
        node = _new;
    }
    /* Remove case */
    else if (_new == NULL)
    {
        insert = false;
        node = _old;
    }
    /* Update case or invalid */
    else
    {
        /* Nothing to do */
        return _old;
    }

    uuidset_t *us = CONTAINER_OF(list, uuidset_t, us_list);

    /* Acquire object reflink */
    ref = us->us_getref_fn(&node->un_uuid);
    if (ref == NULL)
    {
        LOG(ERR, "uuidset: Unable to acquire reflink for object with UUID %s. Parent is "PRI(reflink_t)".",
                node->un_uuid.uuid,
                FMT(reflink_t, us->us_reflink));
        return NULL;
    }

    if (insert) /* Add element */
    {
        struct uuidset_node *new_node = calloc(1, sizeof(*new_node));

        new_node->un_uuid = node->un_uuid;
        node = new_node;

        /* Connect the root reflink to the element reflink */
        reflink_connect(&us->us_reflink, ref);

        /* Notify the update handler */
        us->us_update_fn(us, UUIDSET_NEW, ref);
    }
    else /* Remove element */
    {
        /* Notify the update handler */
        us->us_update_fn(us, UUIDSET_DEL, ref);

        /* Disconnect root and element reflinks */
        reflink_disconnect(&us->us_reflink, ref);

        free(node);
        node = NULL;
    }

    return node;
}

/*
 * Send an UUIDSET_MOD notification for all currently active elements in the uuidset
 */
void uuidset_refresh(uuidset_t *us)
{
    struct uuidset_node *node;

    synclist_foreach(&us->us_list, node)
    {
        /* Dereference the object by UUID */
        reflink_t *ref = us->us_getref_fn(&node->un_uuid);
        if (ref == NULL)
        {
            LOG(ERR, "uuidset_update: Unable to acquire reference to object: %s", node->un_uuid.uuid);
            continue;
        }

        us->us_update_fn(us, UUIDSET_MOD, ref);
    }
}

/* Reflink callback, just translate it to an update notification */
void uuidset_reflink_fn(reflink_t *obj, reflink_t *remote)
{
    if (remote == NULL) return;

    uuidset_t *us = CONTAINER_OF(obj, uuidset_t, us_reflink);
    us->us_update_fn(us, UUIDSET_MOD, remote);
}

/* Compare two uuidset_node structures */
int uuidset_cmp(void *_a, void *_b)
{
    struct uuidset_node *a = _a;
    struct uuidset_node *b = _b;

    return strcmp(a->un_uuid.uuid, b->un_uuid.uuid);
}

