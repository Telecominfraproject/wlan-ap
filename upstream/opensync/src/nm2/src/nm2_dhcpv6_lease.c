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
 *  IP_Interface table
 * ===========================================================================
 */
static ovsdb_table_t table_DHCPv6_Lease;

static void callback_DHCPv6_Lease(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCPv6_Lease *old,
        struct schema_DHCPv6_Lease *new);

static bool nm2_dhcpv6_lease_update(
        struct nm2_dhcpv6_lease *d6l,
        struct schema_DHCPv6_Lease *schema);

static ds_tree_t nm2_dhcpv6_lease_list = DS_TREE_INIT(ds_str_cmp, struct nm2_dhcpv6_lease, d6l_tnode);

struct nm2_dhcpv6_lease *nm2_dhcpv6_lease_get(const ovs_uuid_t *uuid);
static void nm2_dhcpv6_lease_release(struct nm2_dhcpv6_lease *rel);

/*
 * Initialize table monitors
 */
void nm2_dhcpv6_lease_init(void)
{
    LOG(INFO, "Initializing NM DHCPv6_Lease monitoring.");

    OVSDB_TABLE_INIT_NO_KEY(DHCPv6_Lease);
    OVSDB_TABLE_MONITOR(DHCPv6_Lease, false);
}

/*
 * OVSDB monitor update callback for DHCPv6_Lease
 */
void callback_DHCPv6_Lease(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCPv6_Lease *old,
        struct schema_DHCPv6_Lease *new)
{
    struct nm2_dhcpv6_lease *d6l;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            d6l = nm2_dhcpv6_lease_get(&new->_uuid);
            reflink_ref(&d6l->d6l_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            d6l = ds_tree_find(&nm2_dhcpv6_lease_list, new->_uuid.uuid);
            if (d6l == NULL)
            {
                LOG(ERR, "dhcpv6_lease: DHCPv6_Lease with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            d6l = ds_tree_find(&nm2_dhcpv6_lease_list, old->_uuid.uuid);
            if (d6l == NULL)
            {
                LOG(ERR, "dhcpv6_lease: DHCPv6_Lease with uuid %s not found in cache. Cannot delete.", old->_uuid.uuid);
                return;
            }

            /* Decrease the reference count */
            reflink_ref(&d6l->d6l_reflink, -1);
            return;

        default:
            LOG(ERR, "dhcpv6_lease: Monitor update error.");
            return;
    }

    if (!nm2_dhcpv6_lease_update(d6l, new))
    {
        LOG(ERR, "dhcpv6_lease: Unable to parse DHCPv6_Lease schema.");
    }
}

/*
 * Release DHCPv6_Lease object
 */
void nm2_dhcpv6_lease_release(struct nm2_dhcpv6_lease *d6l)
{
    LOG(TRACE, "DHCPv6_Lease: Releasing.");

    d6l->d6l_valid = false;
    reflink_signal(&d6l->d6l_reflink);

    reflink_fini(&d6l->d6l_reflink);

    ds_tree_remove(&nm2_dhcpv6_lease_list, d6l);

    free(d6l);
}

/*
 * DHCPv6_Lease reflink callback
 */
void nm2_dhcpv6_lease_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_dhcpv6_lease *d6l;

    d6l = CONTAINER_OF(obj, struct nm2_dhcpv6_lease, d6l_reflink);

    if (sender == NULL)
    {
        LOG(INFO, "dhcpv6_lease: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, d6l->d6l_reflink));
        nm2_dhcpv6_lease_release(d6l);
    }
}

/*
 * Get a reference to the nm2_dhcpv6_lease structure associated
 * with uuid.
 */
struct nm2_dhcpv6_lease *nm2_dhcpv6_lease_get(const ovs_uuid_t *uuid)
{
    struct nm2_dhcpv6_lease *d6l;

    d6l = ds_tree_find(&nm2_dhcpv6_lease_list, (void *)uuid->uuid);
    if (d6l == NULL)
    {
        /* Allocate a new dummy structure and insert it into the cache */
        d6l = calloc(1, sizeof(struct nm2_dhcpv6_lease));
        d6l->d6l_hwaddr = OSN_MAC_ADDR_INIT;
        d6l->d6l_uuid = *uuid;

        reflink_init(&d6l->d6l_reflink, "DHCPv6_Lease");
        reflink_set_fn(&d6l->d6l_reflink, nm2_dhcpv6_lease_ref_fn);

        ds_tree_insert(&nm2_dhcpv6_lease_list, d6l, d6l->d6l_uuid.uuid);
    }

    return d6l;
}

/*
 * Get reflink to DHCPv6_Lease with UUID
 */
reflink_t *nm2_dhcpv6_lease_getref(const ovs_uuid_t *uuid)
{
    struct nm2_dhcpv6_lease *d6l;

    d6l = nm2_dhcpv6_lease_get(uuid);
    if (d6l == NULL)
    {
        return NULL;
    }

    return &d6l->d6l_reflink;
}

/*
 * Update a struct nm2_dhcpv6_lease from the schema
 */
bool nm2_dhcpv6_lease_update(
        struct nm2_dhcpv6_lease *d6l,
        struct schema_DHCPv6_Lease *schema)
{
    bool retval = false;

    /*
     * The DHCPv6_Lease is changing, flag it as invalid and notify listeners
     */
    if (d6l->d6l_valid)
    {
        d6l->d6l_valid = false;
        reflink_signal(&d6l->d6l_reflink);
    }

    if (!osn_ip6_addr_from_str(&d6l->d6l_prefix, schema->prefix))
    {
        LOG(ERR, "dhcpv6_lease: Unable to parse IPv6_Lease.prefix: %s", schema->prefix);
        goto error;
    }

    if (schema->hwaddr_exists && !osn_mac_addr_from_str(&d6l->d6l_hwaddr, schema->hwaddr))
    {
        LOG(ERR, "dhcpv6_lease: Error, IPv6_Lease.hwaddr is invalid: %s", schema->hwaddr);
        goto error;
    }
    else
    {
        d6l->d6l_hwaddr = OSN_MAC_ADDR_INIT;
    }

    if (strscpy(d6l->d6l_status, schema->status, sizeof(d6l->d6l_status)) < 0)
    {
        LOG(ERR, "dhcpv6_lease: Error, IPv6_Lease.status is too long: %s", schema->status);
        goto error;
    }

    if (schema->duid_exists && strscpy(d6l->d6l_duid, schema->duid, sizeof(d6l->d6l_duid)) < 0)
    {
        LOG(ERR, "dhcpv6_lease: Error, IPv6_Lease.duid is too long: %s", schema->duid);
        goto error;
    }
    else
    {
        d6l->d6l_duid[0] = '\0';
    }

    if (schema->hostname_exists && strscpy(d6l->d6l_hostname, schema->hostname, sizeof(d6l->d6l_hostname)) < 0)
    {
        LOG(ERR, "dhcpv6_lease: Error, IPv6_Lease.hostname is too long: %s", schema->hostname);
        goto error;
    }
    else
    {
        schema->hostname[0] = '\0';
    }

    d6l->d6l_leased_time = schema->leased_time_exists ? schema->leased_time : -1;

    /*
     * Parsing successful, notify listeners that we have a valid structure now
     */
    d6l->d6l_valid = true;
    reflink_signal(&d6l->d6l_reflink);

    retval = true;

error:
    return retval;
}
