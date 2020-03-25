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
#include "ovsdb_sync.h"

#include "nm2.h"

/*
 * ===========================================================================
 *  IPv6_Address table
 * ===========================================================================
 */
static ovsdb_table_t table_IPv6_Address;

static void callback_IPv6_Address(
        ovsdb_update_monitor_t *mon,
        struct schema_IPv6_Address *old,
        struct schema_IPv6_Address *new);

static bool nm2_ipv6_address_update(
        struct nm2_ipv6_address *addr,
        struct schema_IPv6_Address *schema);

static ds_tree_t nm2_ipv6_address_list = DS_TREE_INIT(ds_str_cmp, struct nm2_ipv6_address, ip6_tnode);

static struct nm2_ipv6_address *nm2_ipv6_address_get(const ovs_uuid_t *uuid);
static void nm2_ipv6_address_release(struct nm2_ipv6_address *rel);
/*
 * Initialize table monitors
 */
void nm2_ipv6_address_init(void)
{
    LOG(INFO, "Initializing NM IPv6_Address monitoring.");

    OVSDB_TABLE_INIT_NO_KEY(IPv6_Address);
    OVSDB_TABLE_MONITOR(IPv6_Address, false);
}

/*
 * OVSDB monitor update callback for IP_Interface
 */
void callback_IPv6_Address(
        ovsdb_update_monitor_t *mon,
        struct schema_IPv6_Address *old,
        struct schema_IPv6_Address *new)
{
    struct nm2_ipv6_address *ip6;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            ip6 = nm2_ipv6_address_get(&new->_uuid);
            if (ip6 == NULL)
            {
                LOG(ERR, "ipv6_addr: Error allocating an IPv6_Address object (uuid %s).",
                        new->_uuid.uuid);
            }
            /* Increase the main reflink */
            reflink_ref(&ip6->ip6_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            ip6 = ds_tree_find(&nm2_ipv6_address_list, new->_uuid.uuid);
            if (ip6 == NULL)
            {
                LOG(ERR, "ipv6_addr: IPv6_Address with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            ip6 = ds_tree_find(&nm2_ipv6_address_list, old->_uuid.uuid);
            if (ip6 == NULL)
            {
                LOG(ERR, "ipv6_addr: IPv6_Address with uuid %s not found in cache. Cannot delete.", old->_uuid.uuid);
                return;
            }

            /* Decrease the main reflink and release */
            reflink_ref(&ip6->ip6_reflink, -1);
            return;

        default:
            LOG(ERR, "ipv6_addr: Monitor update error.");
            return;
    }

    if (!nm2_ipv6_address_update(ip6, new))
    {
        LOG(ERR, "ipv6_addr: Unable to parse IPv6_Address schema.");
    }
}

/*
 * Release IPv6_Address object */
void nm2_ipv6_address_release(struct nm2_ipv6_address *ip6)
{
    LOG(TRACE, "IPv6_Address: Releasing.");

    /* Invalidate this address so it gets removed, notify listeners */
    ip6->ip6_valid = false;
    reflink_signal(&ip6->ip6_reflink);

    reflink_fini(&ip6->ip6_reflink);

    ds_tree_remove(&nm2_ipv6_address_list, ip6);

    free(ip6);
}

/*
 * IPv6_Address reflink callback
 */
void nm2_ipv6_address_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_ipv6_address *ip6;

    ip6 = CONTAINER_OF(obj, struct nm2_ipv6_address, ip6_reflink);

    if (sender == NULL)
    {
        LOG(INFO, "ipv6_address: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, ip6->ip6_reflink));
        nm2_ipv6_address_release(ip6);
    }
}

/*
 * Get a reference to the nm2_ipv6_address structure associated
 * with uuid.
 */
struct nm2_ipv6_address *nm2_ipv6_address_get(const ovs_uuid_t *uuid)
{
    struct nm2_ipv6_address *ip6;

    ip6 = ds_tree_find(&nm2_ipv6_address_list, (void *)uuid->uuid);
    if (ip6 != NULL) return ip6;

    /* Allocate a new dummy structure and insert it into the cache */
    ip6 = calloc(1, sizeof(struct nm2_ipv6_address));
    ip6->ip6_uuid = *uuid;
    reflink_init(&ip6->ip6_reflink, "IPv6_Address");
    reflink_set_fn(&ip6->ip6_reflink, nm2_ipv6_address_ref_fn);
    ds_tree_insert(&nm2_ipv6_address_list, ip6, ip6->ip6_uuid.uuid);

    return ip6;
}

/*
 * Acquire a reflink to an IPv6_Address object
 */
reflink_t *nm2_ipv6_address_getref(const ovs_uuid_t *uuid)
{
    struct nm2_ipv6_address *ip6;

    ip6 = nm2_ipv6_address_get(uuid);
    if (ip6 == NULL)
    {
        LOG(TRACE, "ipv6_address: Unable to acquire an IPv6_Address object for UUID %s.",
                uuid->uuid);
        return NULL;
    }

    return &ip6->ip6_reflink;
}

/*
 * Update a struct nm2_ipv6_address from the schema definition
 */
bool nm2_ipv6_address_update(
        struct nm2_ipv6_address *ip6,
        struct schema_IPv6_Address *schema)
{
    long lft;

    bool retval = false;

    /*
     * The IPv6_Address is changing, flag it as invalid and notify listeners
     */
    if (ip6->ip6_valid)
    {
        ip6->ip6_valid = false;
        reflink_signal(&ip6->ip6_reflink);
    }

    if (!osn_ip6_addr_from_str(&ip6->ip6_addr, schema->address))
    {
        LOG(ERR, "ipv6_addr: Unable to parse IPv6 address: %s", schema->address);
        goto error;
    }

    /*
     * Lifetimes can be specified as part of the address, but these override lifetimes from the address string
     */
    if (schema->preferred_lifetime_exists && os_strtoul(schema->preferred_lifetime, &lft, 0))
    {
        ip6->ip6_addr.ia6_pref_lft = lft;
    }
    ip6->ip6_addr.ia6_pref_lft = lft;

    if (schema->valid_lifetime_exists && os_strtoul(schema->valid_lifetime, &lft, 0))
    {
        ip6->ip6_addr.ia6_valid_lft = lft;
    }

    if (strcmp(schema->origin, "static") == 0)
    {
        ip6->ip6_origin = INET_IP6_ORIGIN_STATIC;
    }
    else
    {
        ip6->ip6_origin = INET_IP6_ORIGIN_AUTO_CONFIGURED;
    }

    /*
     * Parsing successful, notify listeners that we have a valid structure now
     */
    ip6->ip6_valid = true;
    reflink_signal(&ip6->ip6_reflink);

    retval = true;
error:
    return retval;
}

/**
 * IPv6 address update callback from libinet; take the address report from
 * libinet and fill in the schema structure
 */
void nm2_ip6_addr_status_fn(
        inet_t *inet,
        struct inet_ip6_addr_status *as,
        bool remove)
{
    struct nm2_ip_interface *parent;
    struct nm2_iface *piface;
    bool rc;

    piface = nm2_iface_get_by_name(inet->in_ifname);

    if (piface->if_ipi == NULL)
    {
        LOG(DEBUG, "ipv6_address: %s: Cannot process IPv6_Address "PRI_osn_ip6_addr". No associated IP_Interface table.",
            inet->in_ifname,
            FMT_osn_ip6_addr(as->is_addr));
        return;
    }

    parent = piface->if_ipi;

    LOG(INFO, "ipv6_address: %s: Updating IPv6_Address table: %s address="PRI_osn_ip6_addr" origin=%d parent=%s",
            parent->ipi_ifname,
            remove ? "DEL" : "ADD",
            FMT_osn_ip6_addr(as->is_addr),
            as->is_origin,
            parent->ipi_uuid.uuid);

    if (remove)
    {
        struct uuidset_node *us;
        struct nm2_ipv6_address *addr;

        /* Traverse the uuidset of the parent interface and try to find the IPv6 address */
        synclist_foreach(&parent->ipi_ipv6_addr.us_list, us)
        {
            addr = nm2_ipv6_address_get(&us->un_uuid);
            if (addr == NULL)
            {
                LOG(DEBUG, "ipv6_address: %s: Cannot find IPv6_Address with uuid: %s",
                        parent->ipi_ifname,
                        us->un_uuid.uuid);
                continue;
            }

            LOG(DEBUG, "ipv6_address: %s: IPv6 compare "PRI_osn_ip6_addr" == "PRI_osn_ip6_addr,
                    parent->ipi_ifname,
                    FMT_osn_ip6_addr(as->is_addr),
                    FMT_osn_ip6_addr(addr->ip6_addr));
            /* Check if we have a match */
            if (osn_ip6_addr_nolft_cmp(&as->is_addr, &addr->ip6_addr) == 0) break;

            addr = NULL;
        }

        if (addr == NULL)
        {
            LOG(ERR, "ipv6_address: %s: Cannot remove IPv6 Address "PRI_osn_ip6_addr". Not found in parent uuid set.",
                    parent->ipi_ifname,
                    FMT_osn_ip6_addr(as->is_addr));
            return;
        }

        /* Remove the IPv6_Address by mutating the uuidset */
        ovsdb_sync_mutate_uuid_set(
                SCHEMA_TABLE(IP_Interface),
                ovsdb_where_uuid("_uuid", parent->ipi_uuid.uuid),
                SCHEMA_COLUMN(IP_Interface, ipv6_addr),
                OTR_DELETE,
                addr->ip6_uuid.uuid);
    }
    else
    {
        /*
         * Copy data to schema structure
         */
        osn_ip6_addr_t paddr;
        struct schema_IPv6_Address addr;
        json_t *jaddr;
        pjs_errmsg_t perr;

        /* Remove lifetimes from the IPv6 Address */
        paddr = as->is_addr;
        paddr.ia6_pref_lft = INT_MIN;
        paddr.ia6_valid_lft = INT_MIN;

        memset(&addr, 0, sizeof(addr));
        snprintf(addr.address, sizeof(addr.address), PRI_osn_ip6_addr, FMT_osn_ip6_addr(paddr));
        snprintf(addr.prefix, sizeof(addr.prefix), PRI_osn_ip6_addr, FMT_osn_ip6_addr(paddr));
        STRSCPY(addr.origin, "auto_configured");
        snprintf(addr.origin, sizeof(addr.origin), "auto_configured");
        addr.origin_exists = true;
        STRSCPY(addr.address_status, "unknown");
        addr.enable = true;

        jaddr = schema_IPv6_Address_to_json(&addr, perr);

        ovs_uuid_t _uuid;
        /*
         * Insert with parent
         */
        rc = ovsdb_sync_insert_with_parent(
                SCHEMA_TABLE(IPv6_Address),
                jaddr,
                &_uuid,
                SCHEMA_TABLE(IP_Interface),
                ovsdb_where_uuid("_uuid", parent->ipi_uuid.uuid),
                SCHEMA_COLUMN(IP_Interface, ipv6_addr));
        if (!rc)
        {
            LOG(ERR, "ipv6_address: %s: Cannot insert IPv6 Address "PRI_osn_ip6_addr".",
                    parent->ipi_ifname,
                    FMT_osn_ip6_addr(as->is_addr));
        }
    }
}
