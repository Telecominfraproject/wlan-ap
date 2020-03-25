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

#include "ds_tree.h"
#include "reflink.h"
#include "ovsdb_table.h"
#include "inet.h"
#include "osn_types.h"

#include "nm2.h"
#include "nm2_iface.h"

/*
 * ===========================================================================
 *  IP_Interface table
 * ===========================================================================
 */
static ovsdb_table_t table_IP_Interface;

static void callback_IP_Interface(
        ovsdb_update_monitor_t *mon,
        struct schema_IP_Interface *old,
        struct schema_IP_Interface *new);

static ds_tree_t nm2_ip_interface_list = DS_TREE_INIT(ds_str_cmp, struct nm2_ip_interface, ipi_tnode);

static struct nm2_ip_interface *nm2_ip_interface_get(const ovs_uuid_t *uuid);
static void nm2_ip_interface_release(struct nm2_ip_interface *ipi);
static bool nm2_ip_interface_update(struct nm2_ip_interface *ipi, struct schema_IP_Interface *schema);
static void nm2_ip_interface_ref_fn(reflink_t *obj, reflink_t *remote);
static uuidset_update_fn_t nm2_ip_interface_ipv6_addr_update;
static uuidset_update_fn_t nm2_ip_interface_ipv6_prefix_update;

/*
 * Initialize table monitors
 */
void nm2_ip_interface_init(void)
{
    LOG(INFO, "ip_interface: Initializing NM IP_Interface monitoring.");

    OVSDB_TABLE_INIT(IP_Interface, name);
    OVSDB_TABLE_MONITOR(IP_Interface, false);
}

/*
 * OVSDB monitor update callback for IP_Interface
 */
void callback_IP_Interface(
        ovsdb_update_monitor_t *mon,
        struct schema_IP_Interface *old,
        struct schema_IP_Interface *new)
{
    struct nm2_ip_interface *ipi;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            ipi = nm2_ip_interface_get(&new->_uuid);
            if (ipi == NULL)
            {
                LOG(ERR, "ipv6_addr: Error allocating an IP_Interace object (uuid %s).",
                        new->_uuid.uuid);
                return;
            }

            /* Acquire a reference to the main reflink of the IP_Interface object */
            reflink_ref(&ipi->ipi_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            ipi = ds_tree_find(&nm2_ip_interface_list, new->_uuid.uuid);
            if (ipi == NULL)
            {
                LOG(ERR, "ip_interface: IP_Interface with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            /* Remove case */
            ipi = ds_tree_find(&nm2_ip_interface_list, old->_uuid.uuid);
            if (ipi == NULL)
            {
                LOG(ERR, "ip_interface: IP_interface with uuid %s not found in cache. Cannot remove.",
                        old->_uuid.uuid);
                return;
            }

            /* Acquire a reference to the main reflink of the IP_Interface object */
            reflink_ref(&ipi->ipi_reflink, -1);
            return;

        default:
            LOG(ERR, "ip_interface: Monitor update error.");
            return;
    }

    if (!nm2_ip_interface_update(ipi, new))
    {
        LOG(ERR, "ip_interface: Unable to parse IP_Interface schema.");
    }

    ipi->ipi_valid = true;

    /* Notify connected structures */
    reflink_signal(&ipi->ipi_reflink);
}

/*
 * IP_Interface reflink
 */
void nm2_ip_interface_ref_fn(reflink_t *obj, reflink_t *remote)
{
    struct nm2_ip_interface *ipi;

    ipi = CONTAINER_OF(obj, struct nm2_ip_interface, ipi_reflink);

    if (remote == NULL)
    {
        LOG(INFO, "ip_interface: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, ipi->ipi_reflink));

        nm2_ip_interface_release(ipi);
    }
}

/*
 * Get a reference to the nm2_ip_interface structure associated with uuid.
 *
 * This function increases the reference count of the structure by 1 and
 * must be freed either using nm2_ip_interface_put().
 */
struct nm2_ip_interface *nm2_ip_interface_get(const ovs_uuid_t *uuid)
{
    struct nm2_ip_interface *ipi;

    ipi = ds_tree_find(&nm2_ip_interface_list, (void *)uuid->uuid);
    if (ipi != NULL) return ipi;

    /* Allocate a new empty structure */
    ipi = calloc(1, sizeof(struct nm2_ip_interface));
    if (ipi == NULL) return NULL;

    ipi->ipi_uuid = *uuid;
    reflink_init(&ipi->ipi_reflink, "IP_Interface");
    reflink_set_fn(&ipi->ipi_reflink, nm2_ip_interface_ref_fn);
    ds_tree_insert(&nm2_ip_interface_list, ipi, ipi->ipi_uuid.uuid);

    /* IPv6_Address list initialization */
    uuidset_init(
            &ipi->ipi_ipv6_addr,
            "IP_Interface.ipv6_addr",
            nm2_ipv6_address_getref,
            nm2_ip_interface_ipv6_addr_update);

    /* IPv6_Prefix list initialization */
    uuidset_init(
            &ipi->ipi_ipv6_prefix,
            "IP_Interface.ipv6_prefix",
            nm2_ipv6_prefix_getref,
            nm2_ip_interface_ipv6_prefix_update);

    return ipi;
}

/*
 * Return a reflink to IP_Interface with UUID
 */
reflink_t *nm2_ip_interface_getref(const ovs_uuid_t *uuid)
{
    struct nm2_ip_interface *ipi;

    ipi = nm2_ip_interface_get(uuid);
    if (ipi == NULL) return NULL;

    return &ipi->ipi_reflink;
}

void nm2_ip_interface_release(struct nm2_ip_interface *ipi)
{
    /* Dereference callback */
    LOG(TRACE, "IP_Interface: %s, releasing.", ipi->ipi_ifname);

    /* Signal to any connected reflinks that the structure is going away */
    ipi->ipi_valid = false;
    reflink_signal(&ipi->ipi_reflink);

    uuidset_fini(&ipi->ipi_ipv6_addr);
    uuidset_fini(&ipi->ipi_ipv6_prefix);

    reflink_fini(&ipi->ipi_reflink);

    /* Disassociate structures */
    if (ipi->ipi_iface != NULL &&
            ipi->ipi_iface->if_ipi == ipi)
    {
        ipi->ipi_iface->if_ipi = NULL;
    }

    ds_tree_remove(&nm2_ip_interface_list, ipi);

    free(ipi);
}

/*
 * IP_Interface -> IPv6_Address uuidset
 */
void nm2_ip_interface_ipv6_addr_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    //struct nm2_iface *piface;

    /* Dereference classes */
    struct nm2_ip_interface *ipi = CONTAINER_OF(us, struct nm2_ip_interface, ipi_ipv6_addr);
    struct nm2_ipv6_address *ip6 = CONTAINER_OF(remote, struct nm2_ipv6_address, ip6_reflink);

    bool add;

    (void)ipi;

    switch (type)
    {
        case UUIDSET_NEW:
            if (!ip6->ip6_valid) return;
            add = true;
            break;

        case UUIDSET_MOD:
            add = ip6->ip6_valid;
            break;

        case UUIDSET_DEL:
            if (!ip6->ip6_valid) return;
            add = false;
            break;

        default:
            return;
    }

    if (ip6->ip6_origin != INET_IP6_ORIGIN_STATIC)
    {
        LOG(DEBUG, "IP_Interface: %s: Skipping IPv6 address: "PRI_osn_ip6_addr". Origin not static.",
                ipi->ipi_ifname,
                FMT_osn_ip6_addr(ip6->ip6_addr));
        return;
    }

    /* FIXME: Print out the actual IPv6 address */
    LOG(INFO, "IP_Interface: %s: Adding IPv6 address (%p).",
            ipi->ipi_ifname,
            ipi->ipi_iface);

    if (!inet_ip6_addr(ipi->ipi_iface->if_inet, add, &ip6->ip6_addr))
    {
        LOG(ERR, "IP_Interface: %s: Unable to add/del(%d) IPv6_Address.",
                ipi->ipi_ifname,
                add);
        return;
    }

    inet_commit(ipi->ipi_iface->if_inet);
}

/*
 * IP_Interface -> IPv6_Prefix uuidset
 */
void nm2_ip_interface_ipv6_prefix_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Dereference classes */
    struct nm2_ip_interface *ipi = CONTAINER_OF(us, struct nm2_ip_interface, ipi_ipv6_addr);
    struct nm2_ipv6_prefix *ip6p = CONTAINER_OF(remote, struct nm2_ipv6_prefix, ip6p_reflink);

    bool add;

    (void)ipi;

    switch (type)
    {
        case UUIDSET_NEW:
            if (!ip6p->ip6p_valid) return;
            add = true;
            break;

        case UUIDSET_MOD:
            add = ip6p->ip6p_valid;
            break;

        case UUIDSET_DEL:
            if (!ip6p->ip6p_valid) return;
            add = false;
            break;

        default:
            return;
    }

    LOG(NOTICE, "ip_inetrface: %s: Receive IPv6_Prefix update (add/remove(%d).",
            ipi->ipi_ifname,
            add);
#if 0
    /* Process new entry */
    if (add)
    {
        LOG(NOTICE, "ADD PREFIX");
        /* Add to lower layer */
    }
    else
    {
        LOG(NOTICE, "REMOVE PREFIX");
        /* Remove from lower layer */
    }
#endif
    /* What should we do with IPv6 Prefixes? */
}

bool nm2_ip_interface_update(struct nm2_ip_interface *ipi, struct schema_IP_Interface *schema)
{
    if (strscpy(ipi->ipi_ifname, schema->name, sizeof(ipi->ipi_ifname)) < 0)
    {
        LOG(ERR, "ip_interface: IP_Interface:name is too long: %s", schema->name);
        return false;
    }

    /* Update nm2_iface pointer */
    ipi->ipi_iface = nm2_iface_get_by_name(ipi->ipi_ifname);
    if (ipi->ipi_iface == NULL)
    {
        LOG(ERR, "ip_interface: Error looking up for interface %s. Wifi_Inet_Config does not exists yet.",
                ipi->ipi_ifname);
    }
    else
    {
        /* Connect nm2_iface -> nm2_ip_interface for faster lookups */
        if (ipi->ipi_iface->if_ipi == NULL)
        {
            ipi->ipi_iface->if_ipi = ipi;
            /* Resync data */
            nm2_iface_status_sync(ipi->ipi_iface);
        }
    }

    ipi->ipi_enable = schema->enable;

    if (!uuidset_set(
                &ipi->ipi_ipv6_addr,
                schema->ipv6_addr,
                schema->ipv6_addr_len))
    {
        LOG(WARN, "ip_interface: Error updating ipv6_address list.");
    }

    if (!uuidset_set(
                &ipi->ipi_ipv6_prefix,
                schema->ipv6_prefix,
                schema->ipv6_prefix_len))
    {
        LOG(WARN, "ip_interface: Error updating ipv6_prefix list.");
    }

    return true;
}

/*
 * Return an struct nm2_iface structure associated with an IP_Interface:_uuid
 */
struct nm2_iface *nm2_ip_interface_iface_get(ovs_uuid_t *uuid)
{
    reflink_t *ref;

    /*
     * Lookup parent interface
     */
    ref = nm2_ip_interface_getref(uuid);
    if (ref == NULL)
    {
        LOG(ERR, "ip_interface: Unable to acquire reference to IP_Interface with UUID %s.",
                uuid->uuid);
        return NULL;
    }

    /* Dereference the reflink and check if the ipi_iface structure already exists */
    struct nm2_ip_interface *ipi = CONTAINER_OF(ref, struct nm2_ip_interface, ipi_reflink);

    if (ipi->ipi_iface == NULL)
    {
        LOG(DEBUG, "ip_interface: Parent interface %s with UUID %s is not valid yet.",
                ipi->ipi_ifname,
                uuid->uuid);
        return NULL;
    }

    return ipi->ipi_iface;
}
