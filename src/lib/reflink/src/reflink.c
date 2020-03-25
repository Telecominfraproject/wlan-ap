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
#include <stdio.h>

#include "reflink.h"
#include "const.h"

/* reflink connection structure */
struct reflink_connection
{
    reflink_t      *rc_dst;             /* Destination reflink */
    ds_tree_node_t  rc_tnode;           /* Tree node */
};

static int reflink_cmp(void *a, void *b);

int reflink_ref(reflink_t *a, int ref);
bool reflink_disconnect(reflink_t *src, reflink_t *dst);

/*
 * ===========================================================================
 *  Public API
 * ===========================================================================
 */
/*
 * Initialize reflink structure
 */
void reflink_init(reflink_t *rl, const char *name)
{
    memset(rl, 0, sizeof(*rl));

    rl->rl_name = (name != NULL) ? name : "(null)";

    ds_tree_init(&rl->rl_connections, reflink_cmp, struct reflink_connection, rc_tnode);
}

/*
 * Destroy reflink structure
 */
void reflink_fini(reflink_t *rl)
{
    if (rl->rl_refcount > 0)
    {
        LOG(ALERT, "reflink: Unable to destroy reflink. Refcount of object %s:%p is not zero. Memory will leak.",
                rl->rl_name,
                rl);
        return;
    }
}

void reflink_set_fn(reflink_t *rl, reflink_fn_t *fn)
{
    rl->rl_fn = fn;
}

/*
 * Establish connection between two reflinks src -> dst;
 * src will be subscribed to dst events.
 *
 * Both reference counts will be increased.
 */
bool reflink_connect(reflink_t *src, reflink_t *dst)
{
    struct reflink_connection *prc;

    prc = calloc(1, sizeof(struct reflink_connection));

    prc->rc_dst = src;

    /* The destination reflink is the key */
    ds_tree_insert(&dst->rl_connections, prc, src);

    reflink_ref(dst, 1);
    reflink_ref(src, 1);

    LOG(DEBUG, "reflink: CONNECT "PRI(reflink_t)" -> "PRI(reflink_t),
            FMT(reflink_t, *src),
            FMT(reflink_t, *dst));

    return true;
}

/*
 * Sever the connection between a and b, decrese the reference count
 *
 * src will no longer be subscribed to events from dst.
 */
bool reflink_disconnect(reflink_t *src, reflink_t *dst)
{
    struct reflink_connection *prc;

    prc = ds_tree_find(&dst->rl_connections, src);
    if (prc == NULL)
    {
        LOG(ERR, "reflink: Unable to server connection %s:%p -> %s:%p. It does not exists.",
            src->rl_name, src,
            dst->rl_name, dst);

        return false;
    }

    LOG(DEBUG, "reflink: DISCONNECT "PRI(reflink_t)" -> "PRI(reflink_t),
            FMT(reflink_t, *src),
            FMT(reflink_t, *dst));

    /* Sever the connection */
    ds_tree_remove(&dst->rl_connections, prc);
    free(prc);

    /* Decrease reference counters */
    reflink_ref(dst, -1);
    reflink_ref(src, -1);

    return true;
}

/*
 * Signal connected reflinks.
 *
 * Each reflink that's connected to this one will get their callback executed.
 */
void reflink_signal(reflink_t *rl)
{
    ds_tree_iter_t iter;
    struct reflink_connection *prc;

    /* Iterate over the connections and call their respective callbacks */
    ds_tree_foreach_iter(&rl->rl_connections, prc, &iter)
    {
        if (prc->rc_dst->rl_fn == NULL) continue;
        prc->rc_dst->rl_fn(prc->rc_dst, rl);
    }
}

/*
 * Increase/decrease references manually
 */
int reflink_ref(reflink_t *rl, int refcnt)
{
    int retval = (rl->rl_refcount += refcnt);

    if (rl->rl_refcount <= 0 && rl->rl_fn != NULL)
    {
        rl->rl_fn(rl, NULL);
    }

    return retval;
}

/*
 * ===========================================================================
 *  Private functions
 * ===========================================================================
 */

int reflink_cmp(void *a, void *b)
{
    return (uint8_t *)a - (uint8_t *)b;
}
