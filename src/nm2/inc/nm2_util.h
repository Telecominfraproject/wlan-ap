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

#ifndef NM2_UTIL_H_INCLUDED
#define NM2_UTIL_H_INCLUDED

#include "schema.h"
#include "reflink.h"
#include "synclist.h"

enum uuidset_event
{
    UUIDSET_NEW,        /* New entry */
    UUIDSET_DEL,        /* Delete entry */
    UUIDSET_MOD         /* Modify entry */
};

typedef struct uuidset uuidset_t;

typedef void uuidset_update_fn_t(uuidset_t *us, enum uuidset_event ev, reflink_t *remote);
typedef reflink_t *uuidset_getref_fn_t(const ovs_uuid_t *uuid);

struct uuidset_node
{
    ovs_uuid_t          un_uuid;                /* UUID of referenced object */
    synclist_node_t     un_snode;
};

struct uuidset
{
    synclist_t              us_list;            /* List of uuid/reflink structures */
    reflink_t               us_reflink;         /* reflink to use for establishing connection
                                                   to referenced objects */
    uuidset_getref_fn_t    *us_getref_fn;       /* Acquire reflink to object using UUID */
    uuidset_update_fn_t    *us_update_fn;       /* UUIDSET update notification callback */
};

void uuidset_init(
        uuidset_t *us,
        const char *name,
        uuidset_getref_fn_t *getref,
        uuidset_update_fn_t *udate);

void uuidset_fini(uuidset_t *us);
bool uuidset_set(uuidset_t *us, ovs_uuid_t *elem, int nelem);
void uuidset_refresh(uuidset_t *us);

#endif /* NM2_UTIL_H_INCLUDED */
