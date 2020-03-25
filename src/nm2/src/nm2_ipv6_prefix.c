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

#include "ovsdb_table.h"
#include "os_util.h"
#include "reflink.h"

#include "nm2.h"

/*
 * ===========================================================================
 *  IPv6_Prefix table
 * ===========================================================================
 */
static ovsdb_table_t table_IPv6_Prefix;

static void callback_IPv6_Prefix(
        ovsdb_update_monitor_t *mon,
        struct schema_IPv6_Prefix *old,
        struct schema_IPv6_Prefix *new);

static bool nm2_ipv6_prefix_update(
        struct nm2_ipv6_prefix *addr,
        struct schema_IPv6_Prefix *schema);

static ds_tree_t nm2_ipv6_prefix_list = DS_TREE_INIT(ds_str_cmp, struct nm2_ipv6_prefix, ip6p_tnode);

static struct nm2_ipv6_prefix *nm2_ipv6_prefix_get(const ovs_uuid_t *uuid);
static void nm2_ipv6_prefix_release(struct nm2_ipv6_prefix *rel);

/*
 * Initialize table monitors
 */
void nm2_ipv6_prefix_init(void)
{
    LOG(INFO, "Initializing NM IPv6_Prefix monitoring.");

    OVSDB_TABLE_INIT_NO_KEY(IPv6_Prefix);
    OVSDB_TABLE_MONITOR(IPv6_Prefix, false);
}

/*
 * OVSDB monitor update callback for IP_Interface
 */
void callback_IPv6_Prefix(
        ovsdb_update_monitor_t *mon,
        struct schema_IPv6_Prefix *old,
        struct schema_IPv6_Prefix *new)
{
    struct nm2_ipv6_prefix *ip6p;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            ip6p = nm2_ipv6_prefix_get(&new->_uuid);
            if (ip6p == NULL)
            {
                LOG(ERR, "ipv6_prefix: Error allocating an IPv6_Address object (uuid %s).",
                        new->_uuid.uuid);
            }
            /* Increase the main reflink */
            reflink_ref(&ip6p->ip6p_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            ip6p = ds_tree_find(&nm2_ipv6_prefix_list, new->_uuid.uuid);
            if (ip6p == NULL)
            {
                LOG(ERR, "ipv6_prefix: IPv6_Prefix with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            ip6p = ds_tree_find(&nm2_ipv6_prefix_list, old->_uuid.uuid);
            if (ip6p == NULL)
            {
                LOG(ERR, "ipv6_prefix: IPv6_Prefix with uuid %s not found in cache. Cannot delete.", old->_uuid.uuid);
                return;
            }

            /* Decrease the main reflink and release */
            reflink_ref(&ip6p->ip6p_reflink, -1);
            return;

        default:
            LOG(ERR, "ipv6_prefix: Monitor update error.");
            return;
    }

    if (!nm2_ipv6_prefix_update(ip6p, new))
    {
        LOG(ERR, "ipv6_prefix: Unable to parse IPv6_Prefix schema.");
    }
}

/*
 * Release IPv6_Prefix object */
void nm2_ipv6_prefix_release(struct nm2_ipv6_prefix *ip6p)
{
    LOG(TRACE, "IPv6_Prefix: Releasing.");

    /* Invalidate this prefix so it gets removed, notify listeners */
    ip6p->ip6p_valid = false;
    reflink_signal(&ip6p->ip6p_reflink);

    reflink_fini(&ip6p->ip6p_reflink);

    ds_tree_remove(&nm2_ipv6_prefix_list, ip6p);

    free(ip6p);
}

/*
 * IPv6_Prefix reflink callback
 */
void nm2_ipv6_prefix_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_ipv6_prefix *ip6p;

    ip6p = CONTAINER_OF(obj, struct nm2_ipv6_prefix, ip6p_reflink);

    if (sender == NULL)
    {
        LOG(INFO, "ipv6_prefx: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, ip6p->ip6p_reflink));
        nm2_ipv6_prefix_release(ip6p);
    }
}

/*
 * Get a reference to the nm2_ipv6_prefix structure associated
 * with uuid.
 */
struct nm2_ipv6_prefix *nm2_ipv6_prefix_get(const ovs_uuid_t *uuid)
{
    struct nm2_ipv6_prefix *ip6p;

    ip6p = ds_tree_find(&nm2_ipv6_prefix_list, (void *)uuid->uuid);
    if (ip6p != NULL) return ip6p;

    /* Allocate a new dummy structure and insert it into the cache */
    ip6p = calloc(1, sizeof(struct nm2_ipv6_prefix));
    ip6p->ip6p_uuid = *uuid;
    reflink_init(&ip6p->ip6p_reflink, "IPv6_Prefix");
    reflink_set_fn(&ip6p->ip6p_reflink, nm2_ipv6_prefix_ref_fn);
    ds_tree_insert(&nm2_ipv6_prefix_list, ip6p, ip6p->ip6p_uuid.uuid);

    return ip6p;
}

/*
 * Acquire a reflink to an IPv6_Prefix object
 */
reflink_t *nm2_ipv6_prefix_getref(const ovs_uuid_t *uuid)
{
    struct nm2_ipv6_prefix *ip6p;

    ip6p = nm2_ipv6_prefix_get(uuid);
    if (ip6p == NULL)
    {
        LOG(TRACE, "ipv6_prefix: Unable to acquire an IPv6_Prefix object for UUID %s.",
                uuid->uuid);
        return NULL;
    }

    return &ip6p->ip6p_reflink;
}

/*
 * Update a struct nm2_ipv6_prefix from the schema definition
 */
bool nm2_ipv6_prefix_update(
        struct nm2_ipv6_prefix *ip6p,
        struct schema_IPv6_Prefix *schema)
{
    long lft;

    bool retval = false;

    /*
     * The IPv6_Prefix is changing, flag it as invalid and notify listeners
     */
    if (ip6p->ip6p_valid)
    {
        ip6p->ip6p_valid = false;
        reflink_signal(&ip6p->ip6p_reflink);
    }

    ip6p->ip6p_on_link = schema->on_link;
    ip6p->ip6p_autonomous = schema->autonomous;

    if (!osn_ip6_addr_from_str(&ip6p->ip6p_addr, schema->address))
    {
        LOG(TRACE, "ipv6_prefix: Unable to parse IPv6 address: %s", schema->address);
        goto error;
    }

    if (!schema->preferred_lifetime_exists || !os_strtoul(schema->preferred_lifetime, &lft, 0))
    {
        lft = -1;
    }
    ip6p->ip6p_addr.ia6_pref_lft = lft;

    if (!schema->valid_lifetime_exists || !os_strtoul(schema->valid_lifetime, &lft, 0))
    {
        lft = -1;
    }
    ip6p->ip6p_addr.ia6_valid_lft = lft;

    /*
     * Parsing successful, notify listeners that we have a valid structure now
     */
    ip6p->ip6p_valid = true;
    reflink_signal(&ip6p->ip6p_reflink);

    retval = true;
error:
    return retval;
}
