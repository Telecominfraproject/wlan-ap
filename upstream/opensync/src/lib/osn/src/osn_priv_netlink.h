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

#ifndef OSN_PRIV_NETLINK_H_INCLUDED
#define OSN_PRIV_NETLINK_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "const.h"
#include "ds_tree.h"
#include "ds_dlist.h"

#define OSN_NL_LINK         (1 << 0)    /* Interface L2 events */
#define OSN_NL_IP4ADDR      (1 << 1)    /* IPv4 interface events */
#define OSN_NL_IP6ADDR      (1 << 2)    /* IPv6 interface events */
#define OSN_NL_IP4ROUTE     (1 << 3)    /* IPv4 route events */
#define OSN_NL_IP6ROUTE     (1 << 4)    /* IPv6 route events */
#define OSN_NL_IP4NEIGH     (1 << 5)    /* IPv4 neighbor report */
#define OSN_NL_IP6NEIGH     (1 << 6)    /* IPv6 neighbor report */
#define OSN_NL_ALL          UINT64_MAX

typedef struct osn_nl osn_nl_t;

typedef void osn_nl_fn_t(osn_nl_t *nl, uint64_t event, const char *ifname);

struct osn_nl
{
    bool                nl_active;                  /* True if this object has been started */
    uint64_t            nl_pending;                 /* List of pending events */
    uint64_t            nl_events;                  /* Subscribed events */
    char                nl_ifname[C_IFNAME_LEN];    /* Filter events for this interface */
    osn_nl_fn_t        *nl_fn;                      /* Callback */
    ds_tree_node_t      nl_tnode;
};

/**
 * Initialize osn_nl_t structure. Each time a NETLINK event is received,
 * the fn callback is invoked.
 */
bool osn_nl_init(osn_nl_t *self, osn_nl_fn_t *fn);

/**
 * Destroy netlink object
 */
bool osn_nl_fini(osn_nl_t *self);

/* Subscribe to events */
void osn_nl_set_events(osn_nl_t *self, uint64_t events);

/**
 * Subscribe to interface
 */
void osn_nl_set_ifname(osn_nl_t *self, const char *ifname);

/**
 * Start receiving events
 */
bool osn_nl_start(osn_nl_t *self);

/**
 * Stop receiving events
 */
bool osn_nl_stop(osn_nl_t *self);

#endif /* OSN_PRIV_NETLINK_H_INCLUDED */
