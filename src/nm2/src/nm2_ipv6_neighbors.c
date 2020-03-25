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
#include <sys/wait.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <inttypes.h>
#include <jansson.h>

#include "json_util.h"
#include "evsched.h"
#include "schema.h"
#include "log.h"
#include "nm2.h"
#include "ovsdb.h"
#include "ovsdb_sync.h"

static bool nm2_ipv6_neighbors_table_update(
        struct schema_IPv6_Neighbors *neigh,
        bool remove);

/**
 * IPv6 neighbors callback from libinet; take the neighbor report from
 * libinet and fill in the schema structure
 */
void nm2_ip6_neigh_status_fn(
        inet_t *inet,
        struct inet_ip6_neigh_status *ns,
        bool remove)
{
    struct schema_IPv6_Neighbors neigh;

    memset(&neigh, 0, sizeof(neigh));

    /*
     * Copy data to schema
     */
    STRSCPY(neigh.if_name, inet->in_ifname);

    snprintf(neigh.hwaddr,
            sizeof(neigh.hwaddr),
            PRI_osn_mac_addr,
            FMT_osn_mac_addr(ns->in_hwaddr));

    snprintf(neigh.address,
            sizeof(neigh.address),
            PRI_osn_ip6_addr,
            FMT_osn_ip6_addr(ns->in_ipaddr));

    (void)nm2_ipv6_neighbors_table_update(&neigh, remove);
}

/**
 * Update a IPv6_Neighbors table with the entry below.
 *
 * If remove is set to true, the entry will be removed.
 *
 * If remove is false, the entry will be added using an upsert operation where
 * the keys are the interface name and the IPv6 and MAC addresses.
 */
bool nm2_ipv6_neighbors_table_update(struct schema_IPv6_Neighbors *neigh, bool remove)
{
    pjs_errmsg_t perr;
    json_t *cond;
    json_t *where;
    json_t *row;
    bool ret;

    LOG(INFO, "ipv6_neighbors: Updating IPv6_Neighbors table: %s address=%s mac=%s if_name=%s",
        remove ? "DEL" : "ADD",
        neigh->address,
        neigh->hwaddr,
        neigh->if_name);

    /*
     * Forge the where clause
     */
    where = json_array();

    cond = ovsdb_tran_cond_single("address", OFUNC_EQ, neigh->address);
    json_array_append_new(where, cond);

    cond = ovsdb_tran_cond_single("hwaddr", OFUNC_EQ, neigh->hwaddr);
    json_array_append_new(where, cond);

    cond = ovsdb_tran_cond_single("if_name", OFUNC_EQ, neigh->if_name);
    json_array_append_new(where, cond);


    if (remove)
    {
        /*
         * Remove stale IPv6 neighbor entries
         */
        ret = ovsdb_sync_delete_where(SCHEMA_TABLE(IPv6_Neighbors), where);
        if (!ret)
        {
            LOG(ERROR, "ipv6_neighbors: Failed to remove entry from IPv6_Neighbors.");
            return false;
        }
    }
    else
    {
        /*
         * Upsert IPv6 neighbor entry
         */
        row = schema_IPv6_Neighbors_to_json(neigh, perr);
        if (row == NULL)
        {
            LOG(ERROR, "ipv6_neighbors: Error convert schema structure to JSON.");
            return false;
        }

        if (!ovsdb_sync_upsert_where(SCHEMA_TABLE(IPv6_Neighbors), where, row, NULL))
        {
            LOG(ERROR, "ipv6_neighbors: Failed to upsert entry into IPv6_Neighbors.");
            return false;
        }
    }

    return true;
}

