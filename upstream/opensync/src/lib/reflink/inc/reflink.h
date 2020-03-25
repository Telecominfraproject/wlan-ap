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

#ifndef REFLINK_H_INCLUDED
#define REFLINK_H_INCLUDED

#include <stdbool.h>
#include "ds_tree.h"
#include "log.h"

/*
 * ===========================================================================
 *  Library for maintaining references between objects
 * ===========================================================================
 */
typedef struct reflink reflink_t;
typedef struct reflink_iter reflink_iter_t;

typedef void reflink_fn_t(reflink_t *obj, reflink_t *src);

struct reflink
{
    const char     *rl_name;            /* Object name */
    reflink_fn_t   *rl_fn;              /* reflink callback, used when refcount reaches 0 or
                                           when an event is received from a subscribed object. */
    ds_tree_t       rl_connections;     /* reflink connection list */
    int             rl_refcount;        /* Reference count */
};


/* Pretty printing */
#define PRI_reflink_t       "%s:%p"
#define FMT_reflink_t(r)    (r).rl_name, &(r)

static inline int reflink_refcount(reflink_t *rl)
{
    return rl->rl_refcount;
}

void reflink_init(reflink_t *rl, const char *name);
void reflink_fini(reflink_t *rl);
void reflink_set_fn(reflink_t *rl, reflink_fn_t *fn);
bool reflink_connect(reflink_t *src, reflink_t *dst);
bool reflink_disconnect(reflink_t *src, reflink_t *dst);
void reflink_signal(reflink_t *src);
int reflink_ref(reflink_t *rl, int refcnt);

#endif /* REFLINK_H_INCLUDED */
