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
 *  IP_Interface table
 * ===========================================================================
 */
static ovsdb_table_t table_DHCPv6_Server;

static void callback_DHCPv6_Server(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCPv6_Server *old,
        struct schema_DHCPv6_Server *new);

static bool nm2_dhcpv6_server_update(
        struct nm2_dhcpv6_server *ds6,
        struct schema_DHCPv6_Server *schema);

static ds_tree_t nm2_dhcpv6_server_list = DS_TREE_INIT(ds_str_cmp, struct nm2_dhcpv6_server, ds6_tnode);

struct nm2_dhcpv6_server *nm2_dhcpv6_server_get(ovs_uuid_t *uuid);
static void nm2_dhcpv6_server_release(struct nm2_dhcpv6_server *rel);
static void nm2_dhcpv6_server_prefixes_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote);
static void nm2_dhcpv6_server_options_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote);
static void nm2_dhcpv6_server_lease_prefix_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote);
static void nm2_dhcpv6_server_static_prefix_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote);
static void nm2_dhcpv6_server_set(struct nm2_dhcpv6_server *ds6, bool enable);
static inet_dhcp6_server_notify_fn_t nm2_dhcpv6_server_notify;
static synclist_fn_t dhcpv6_server_lease_list_fn;

struct dhcpv6_server_lease_node
{
    char            d6l_duid[260+1];
    char            d6l_hostname[C_HOSTNAME_LEN];
    osn_ip6_addr_t  d6l_addr;
    int             d6l_lease_time;
    ovs_uuid_t      d6l_uuid;
    synclist_node_t d6l_snode;
};

/*
 * Initialize table monitors
 */
void nm2_dhcpv6_server_init(void)
{
    LOG(INFO, "Initializing NM DHCPv6_Server monitoring.");

    OVSDB_TABLE_INIT_NO_KEY(DHCPv6_Server);
    OVSDB_TABLE_MONITOR(DHCPv6_Server, false);
}

/*
 * OVSDB monitor update callback for DHCPv6_Server
 */
void callback_DHCPv6_Server(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCPv6_Server *old,
        struct schema_DHCPv6_Server *new)
{
    struct nm2_dhcpv6_server *ds6;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            ds6 = nm2_dhcpv6_server_get(&new->_uuid);
            reflink_ref(&ds6->ds6_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            ds6 = ds_tree_find(&nm2_dhcpv6_server_list, new->_uuid.uuid);
            if (ds6 == NULL)
            {
                LOG(ERR, "dhcpv6_server: DHCPv6_Server with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            ds6 = ds_tree_find(&nm2_dhcpv6_server_list, old->_uuid.uuid);
            if (ds6 == NULL)
            {
                LOG(ERR, "dhcpv6_server: DHCPv6_Server with uuid %s not found in cache. Cannot delete.", old->_uuid.uuid);
                return;
            }

            nm2_dhcpv6_server_set(ds6, false);

            /* Decrease the reference count */
            reflink_ref(&ds6->ds6_reflink, -1);
            return;

        default:
            LOG(ERR, "dhcpv6_server: Monitor update error.");
            return;
    }

    if (!nm2_dhcpv6_server_update(ds6, new))
    {
        LOG(ERR, "dhcpv6_server: Unable to parse DHCPv6_Server schema.");
    }

    nm2_dhcpv6_server_set(ds6, true);
}

/*
 * Release DHCPv6_Server object */
void nm2_dhcpv6_server_release(struct nm2_dhcpv6_server *ds6)
{
    reflink_t *ref;

    LOG(TRACE, "DHCPv6_Server: Releasing.");

    if (ds6->ds6_valid)
    {
        ref = nm2_ip_interface_getref(&ds6->ds6_ip_interface_uuid);
        if (ref == NULL)
        {
            LOG(ERR, "dhcpv6_server: Unable to acquire reference to IP_Interface:%s (releas).",
                    ds6->ds6_ip_interface_uuid.uuid);
        }
        else
        {
            reflink_disconnect(&ds6->ds6_ip_interface_reflink, ref);
        }
    }

    /* Invalidate this address so it gets removed, notify listeners */
    ds6->ds6_valid = false;
    reflink_signal(&ds6->ds6_reflink);

    uuidset_fini(&ds6->ds6_prefixes);
    uuidset_fini(&ds6->ds6_options);
    uuidset_fini(&ds6->ds6_lease_prefix);
    uuidset_fini(&ds6->ds6_static_prefix);

    reflink_fini(&ds6->ds6_ip_interface_reflink);

    reflink_fini(&ds6->ds6_reflink);

    ds_tree_remove(&nm2_dhcpv6_server_list, ds6);

    free(ds6);
}

/*
 * DHCPv6_Server reflink callback
 */
void nm2_dhcpv6_server_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_dhcpv6_server *ds6;

    ds6 = CONTAINER_OF(obj, struct nm2_dhcpv6_server, ds6_reflink);

    if (sender == NULL)
    {
        LOG(INFO, "dhcp6_client: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, ds6->ds6_reflink));
        nm2_dhcpv6_server_release(ds6);
    }
}

/*
 * Process notifications from IP_Interface -- this might mean that IP_Interface was finally instantiated
 */
void nm2_dhcpv6_server_interface_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_dhcpv6_server *ds6;
    struct nm2_ip_interface *ipi;

    /* Message to self, refcount reached 0 */
    if (sender == NULL)
    {
        return;
    }

    ds6 = CONTAINER_OF(obj, struct nm2_dhcpv6_server, ds6_ip_interface_reflink);
    ipi = CONTAINER_OF(sender, struct nm2_ip_interface, ipi_reflink);

    if (ipi->ipi_iface == NULL) return;

    /* Refresh uuidsets */
    uuidset_refresh(&ds6->ds6_prefixes);
    uuidset_refresh(&ds6->ds6_options);
    uuidset_refresh(&ds6->ds6_lease_prefix);
    uuidset_refresh(&ds6->ds6_static_prefix);

    /* Update server configuration */
    nm2_dhcpv6_server_set(ds6, true);
}

/*
 * Get a reference to the nm2_dhcpv6_server structure associated
 * with uuid.
 */
struct nm2_dhcpv6_server *nm2_dhcpv6_server_get(ovs_uuid_t *uuid)
{
    struct nm2_dhcpv6_server *ds6;

    ds6 = ds_tree_find(&nm2_dhcpv6_server_list, uuid->uuid);
    if (ds6 == NULL)
    {
        /* Allocate a new dummy structure and insert it into the cache */
        ds6 = calloc(1, sizeof(struct nm2_dhcpv6_server));
        ds6->ds6_uuid = *uuid;

        reflink_init(&ds6->ds6_reflink, "DHCPv6_Server");
        reflink_set_fn(&ds6->ds6_reflink, nm2_dhcpv6_server_ref_fn);

        reflink_init(&ds6->ds6_ip_interface_reflink, "DHCPv6_Server.interface");
        reflink_set_fn(&ds6->ds6_ip_interface_reflink, nm2_dhcpv6_server_interface_ref_fn);

        /*
         * Initialize UUID references
         */
        uuidset_init(
                &ds6->ds6_prefixes,
                "DHCPv6_Server.prefixes",
                nm2_ipv6_prefix_getref,
                nm2_dhcpv6_server_prefixes_update);

        uuidset_init(
                &ds6->ds6_options,
                "DHCPv6_Server.options",
                nm2_dhcp_option_getref,
                nm2_dhcpv6_server_options_update);

        uuidset_init(
                &ds6->ds6_lease_prefix,
                "DHCPv6_Server.lease_prefix",
                nm2_dhcpv6_lease_getref,
                nm2_dhcpv6_server_lease_prefix_update);

        uuidset_init(
                &ds6->ds6_static_prefix,
                "DHCPv6_Server.static_prefix",
                nm2_dhcpv6_lease_getref,
                nm2_dhcpv6_server_static_prefix_update);

        synclist_init(
                &ds6->ds6_lease_list,
                ds_str_cmp,
                struct dhcpv6_server_lease_node,
                d6l_snode,
                dhcpv6_server_lease_list_fn);

        ds_tree_insert(&nm2_dhcpv6_server_list, ds6, ds6->ds6_uuid.uuid);
    }

    return ds6;
}

/*
 * DHCPv6_Server.prefixes update
 */
void nm2_dhcpv6_server_prefixes_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Dereference classes */
    struct nm2_dhcpv6_server *ds6 = CONTAINER_OF(us, struct nm2_dhcpv6_server, ds6_prefixes);
    struct nm2_ipv6_prefix *ip6p = CONTAINER_OF(remote, struct nm2_ipv6_prefix, ip6p_reflink);

    bool add;

    (void)ds6;

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

    struct nm2_iface *piface = nm2_ip_interface_iface_get(&ds6->ds6_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(DEBUG, "dhcpv6_server: prefix_update: Unable to resolve interface.");
        return;
    }

    struct osn_dhcpv6_server_prefix prefix;

    memset(&prefix, 0, sizeof(prefix));
    prefix.d6s_prefix = ip6p->ip6p_addr;

    if (!inet_dhcp6_server_prefix(piface->if_inet, add, &prefix))
    {
        LOG(ERR, "dhcpv6_server: %s: Error adding/removing(%d) prefix.",
                piface->if_name,
                add);
        return;
    }

    inet_commit(piface->if_inet);
}

/*
 * DHCPv6_Server.options update
 */
void nm2_dhcpv6_server_options_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Dereference classes */
    struct nm2_dhcpv6_server *ds6 = CONTAINER_OF(us, struct nm2_dhcpv6_server, ds6_options);
    struct nm2_dhcp_option *dco = CONTAINER_OF(remote, struct nm2_dhcp_option, dco_reflink);

    bool add;

    switch (type)
    {
        case UUIDSET_NEW:
            if (!dco->dco_valid) return;
            add = true;
            break;

        case UUIDSET_MOD:
            add = dco->dco_valid;
            break;

        case UUIDSET_DEL:
            if (!dco->dco_valid) return;
            add = false;
            break;

        default:
            return;
    }

    struct nm2_iface *piface = nm2_ip_interface_iface_get(&ds6->ds6_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(DEBUG, "dhcpv6_server: options_update: Unable to resolve interface.");
        return;
    }

    /* Process new entry */
    if (add)
    {
        LOG(DEBUG, "dhcpv6_server: Adding DHCPv6 option: %d:%s", dco->dco_tag, dco->dco_value);

        /* Add to lower layer */
        if (!inet_dhcp6_server_option_send(piface->if_inet, dco->dco_tag, dco->dco_value))
        {
            LOG(ERR, "dhcpv6_server: %s: Error adding DHCPv6 option:%d, data:%s.",
                    piface->if_name,
                    dco->dco_tag,
                    dco->dco_value);
        }
    }
    else
    {
        /* Remove from lower layer */
        LOG(DEBUG, "dhcpv6_server: Removing DHCPv6 option: %d", dco->dco_tag);

        if (!inet_dhcp6_server_option_send(piface->if_inet, dco->dco_tag, NULL))
        {
            LOG(ERR, "dhcpv6_server: %s: Error removing DHCPv6 option:%d.",
                    piface->if_name,
                    dco->dco_tag);
        }
    }

    nm2_iface_apply(piface);
}

/*
 * DHCPv6_Server.lease_prefix update
 */
void nm2_dhcpv6_server_lease_prefix_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Dereference classes */
    struct nm2_dhcpv6_server *ds6 = CONTAINER_OF(us, struct nm2_dhcpv6_server, ds6_lease_prefix);
    struct nm2_dhcpv6_lease *d6l = CONTAINER_OF(remote, struct nm2_dhcpv6_lease, d6l_reflink);

    bool add;

    (void)ds6;

    switch (type)
    {
        case UUIDSET_NEW:
            if (!d6l->d6l_valid) return;
            add = true;
            break;

        case UUIDSET_MOD:
            add = d6l->d6l_valid;
            break;

        case UUIDSET_DEL:
            if (!d6l->d6l_valid) return;
            add = false;
            break;

        default:
            return;
    }

    struct nm2_iface *piface = nm2_ip_interface_iface_get(&ds6->ds6_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(DEBUG, "dhcpv6_server: prefix_update: Unable to resolve interface.");
        return;
    }

    struct osn_dhcpv6_server_lease lease;

    memset(&lease, 0, sizeof(lease));

    lease.d6s_addr = d6l->d6l_prefix;
    if (STRSCPY(lease.d6s_hostname, d6l->d6l_hostname) < 0)
    {
        LOG(ERR, "dhcpv6_server: %s: Error updating lease, hostname too long: %s",
                piface->if_name,
                d6l->d6l_hostname);
        return;
    }

    if (STRSCPY(lease.d6s_duid, d6l->d6l_duid) < 0)
    {
        LOG(ERR, "dhcpv6_server: %s: Error updating lease, duid too long: %s",
                piface->if_name,
                d6l->d6l_duid);
        return;
    }

    if (!inet_dhcp6_server_lease(piface->if_inet, add, &lease))
    {
        LOG(ERR, "dhcpv6_server: %s: Error adding/removing(%d) prefix.",
                piface->if_name,
                add);
        return;
    }

    inet_commit(piface->if_inet);
}

/*
 * DHCPv6_Server.static_prefix update
 */
void nm2_dhcpv6_server_static_prefix_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Dereference classes */
    struct nm2_dhcpv6_server *ds6 = CONTAINER_OF(us, struct nm2_dhcpv6_server, ds6_prefixes);
    struct nm2_dhcpv6_lease *d6l = CONTAINER_OF(remote, struct nm2_dhcpv6_lease, d6l_reflink);

    bool add;

    (void)ds6;

    switch (type)
    {
        case UUIDSET_NEW:
            if (!d6l->d6l_valid) return;
            add = true;
            break;

        case UUIDSET_MOD:
            add = d6l->d6l_valid;
            break;

        case UUIDSET_DEL:
            if (!d6l->d6l_valid) return;
            add = false;
            break;

        default:
            return;
    }

    /* Process new entry */
    if (add)
    {
        LOG(NOTICE, "ADD DHCPV6SERVER-STATIC_PREFIX");
        /* Add to lower layer */
    }
    else
    {
        LOG(NOTICE, "REMOVE DHCPV6SERVER-STATIC_PREFIX");
        /* Remove from lower layer */
    }
}

/*
 * Update a struct nm2_dhcpv6_server from the schema
 */
bool nm2_dhcpv6_server_update(
        struct nm2_dhcpv6_server *ds6,
        struct schema_DHCPv6_Server *schema)
{
    reflink_t *ref;

    bool retval = false;

    /*
     * The DHCPv6_Server is changing, flag it as invalid and notify listeners
     */
    if (ds6->ds6_valid)
    {
        ds6->ds6_valid = false;
        reflink_signal(&ds6->ds6_reflink);

        /*
         * Remove reflink to IP_Interface
         */
        ref = nm2_ip_interface_getref(&ds6->ds6_ip_interface_uuid);
        if (ref == NULL)
        {
            LOG(ERR, "dhcpv6_server: Unable to acquire reference to IP_Interface:%s (delete)",
                    ds6->ds6_ip_interface_uuid.uuid);
        }
        else
        {
            reflink_disconnect(&ds6->ds6_ip_interface_reflink, ref);
        }
    }

    if (strscpy(ds6->ds6_status, schema->status, sizeof(ds6->ds6_status)) < 0)
    {
        LOG(ERR, "dhcpv6_server: DHCPv6_Server.status is too long: %s.", schema->status);
        goto error;
    }

    ds6->ds6_prefix_delegation = schema->prefix_delegation_exists ? schema->prefix_delegation : false;

    uuidset_set(&ds6->ds6_prefixes, schema->prefixes, schema->prefixes_len);
    uuidset_set(&ds6->ds6_options, schema->options, schema->options_len);
    uuidset_set(&ds6->ds6_lease_prefix, schema->lease_prefix, schema->lease_prefix_len);
    uuidset_set(&ds6->ds6_static_prefix, schema->static_prefix, schema->static_prefix_len);

    /*
     * Re-establish reflink to IP_Interface
     */
    ds6->ds6_ip_interface_uuid = schema->interface;
    ref = nm2_ip_interface_getref(&ds6->ds6_ip_interface_uuid);
    if (ref == NULL)
    {
        LOG(ERR, "dhcpv6_server: Unable to acquire reference to IP_Interface:%s (add)",
                ds6->ds6_ip_interface_uuid.uuid);
        goto error;
    }

    /* Establish connection to IP_Interface */
    reflink_connect(&ds6->ds6_ip_interface_reflink, ref);

    /*
     * Parsing successful, notify listeners that we have a valid structure now
     */
    ds6->ds6_valid = true;
    reflink_signal(&ds6->ds6_reflink);

    retval = true;

error:
    return retval;
}

void nm2_dhcpv6_server_set(struct nm2_dhcpv6_server *ds6, bool enable)
{
    struct nm2_iface *piface;

    piface = nm2_ip_interface_iface_get(&ds6->ds6_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(DEBUG, "dhcpv6_server: Unable to resolve interface with UUID %s.",
                ds6->ds6_ip_interface_uuid.uuid);
        return;
    }

    if (!inet_dhcp6_server(piface->if_inet, enable))
    {
        LOG(ERR, "dhcpv6_server: %s: Unable to set configuration.",
                piface->if_name);
        return;
    }

    if (!inet_dhcp6_server_notify(piface->if_inet, nm2_dhcpv6_server_notify, ds6))
    {
        LOG(ERR, "dhcpv6_server: %s: Unable to set status notification callback.",
                piface->if_name);
        return;
    }

    /* Commit configuration */
    inet_commit(piface->if_inet);
}

/* DHCPv6 server status notification change callback */
void nm2_dhcpv6_server_notify(void *data, struct osn_dhcpv6_server_status *status)
{
    (void)data;
    int ii;

    struct nm2_dhcpv6_server *ds6;
    struct nm2_iface *piface;

    ds6 = data;

    piface = nm2_ip_interface_iface_get(&ds6->ds6_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(WARN , "dhcpv6_server: %s: Got status notification, but IP_Interface is not ready yet.",
                status->d6st_iface);
        return;
    }

    LOG(INFO, "dhcpv6_server: Update: Number of leases: %d.", status->d6st_leases_len);

    synclist_begin(&ds6->ds6_lease_list);

    for (ii = 0; ii < status->d6st_leases_len; ii++)
    {
        struct dhcpv6_server_lease_node dl;

        memset(&dl, 0, sizeof(dl));

        if (STRSCPY(dl.d6l_duid, status->d6st_leases[ii].d6s_duid) < 0)
        {
            LOG(ERR, "dhcpv6_server: %s: Unable to populate lease entry, duid too long: %s",
                    status->d6st_iface,
                    status->d6st_leases[ii].d6s_duid);
            continue;
        }

        if (STRSCPY(dl.d6l_hostname, status->d6st_leases[ii].d6s_hostname) < 0)
        {
            LOG(ERR, "dhcpv6_server: %s: Unable to populate lease entry, hostname too long: %s",
                    status->d6st_iface,
                    status->d6st_leases[ii].d6s_hostname);
            continue;
        }

        dl.d6l_addr = status->d6st_leases[ii].d6s_addr;
        dl.d6l_lease_time = status->d6st_leases[ii].d6s_leased_time;

        LOG(TRACE, "dhcpv6_server: Lease for: hostname=%s, duid=%s", dl.d6l_hostname, dl.d6l_duid);

        synclist_add(&ds6->ds6_lease_list, &dl);
    }

    synclist_end(&ds6->ds6_lease_list);
}

void *dhcpv6_server_lease_list_fn(synclist_t *list, void *_old, void *_new)
{
    struct dhcpv6_server_lease_node *elem;
    pjs_errmsg_t msg;
    bool insert;
    json_t *jdl;
    int rc;

    struct nm2_dhcpv6_server *ds6 = CONTAINER_OF(list, struct nm2_dhcpv6_server, ds6_lease_list);
    (void)list;

    /* Insert case */
    if (_old == NULL)
    {
        insert = true;
        elem = _new;
    }
    /* Remove case */
    else if (_new == NULL)
    {
        insert = false;
        elem = _old;
    }
    /* Update case or invalid */
    else
    {
        return _old;
    }

    if (!insert)
    {
        /* Remove case */
        ovsdb_sync_mutate_uuid_set(
                SCHEMA_TABLE(DHCPv6_Server),
                ovsdb_where_uuid("_uuid", ds6->ds6_uuid.uuid),
                SCHEMA_COLUMN(DHCPv6_Server, lease_prefix),
                OTR_DELETE,
                elem->d6l_uuid.uuid);

        free(elem);
        return NULL;
    }

    /* Insert case */
    struct dhcpv6_server_lease_node *dl;

    dl = calloc(1, sizeof(struct dhcpv6_server_lease_node));
    *dl = *elem;

    struct schema_DHCPv6_Lease schema_dl;
    memset(&schema_dl, 0, sizeof(schema_dl));

    schema_dl.duid_exists = true;
    if (STRSCPY(schema_dl.duid, dl->d6l_duid) < 0)
    {
        LOG(ERR, "dhcpv6_server: Unable to insert DHCPv6_Lease, duid too long: %s",
                dl->d6l_duid);
        goto exit;
    }

    schema_dl.hostname_exists = true;
    if (STRSCPY(schema_dl.hostname, dl->d6l_hostname) < 0)
    {
        LOG(ERR, "dhcpv6_server: Unable to insert DHCPv6_Lease, hostname too long: %s",
                dl->d6l_hostname);
        goto exit;
    }

    snprintf(schema_dl.prefix, sizeof(schema_dl.prefix), PRI_osn_ip6_addr, FMT_osn_ip6_addr(dl->d6l_addr));

    schema_dl.leased_time_exists =true;
    schema_dl.leased_time = dl->d6l_lease_time;

    STRSCPY(schema_dl.status, "leased");

    jdl = schema_DHCPv6_Lease_to_json(&schema_dl, msg);
    if (jdl == NULL)
    {
        LOG(ERR, "dhcpv6_server: Unable to create JSON for DHCPv6_Lease insert: %s", msg);
        goto exit;
    }

    /* Insert a new DHCPv6_Lease entry with parent DHCPv6_Server */
    rc = ovsdb_sync_insert_with_parent(
            SCHEMA_TABLE(DHCPv6_Lease),
            jdl,
            &dl->d6l_uuid,
            SCHEMA_TABLE(DHCPv6_Server),
            ovsdb_where_uuid("_uuid", ds6->ds6_uuid.uuid),
            SCHEMA_COLUMN(DHCPv6_Server, lease_prefix));
    if (!rc)
    {
        LOG(ERR, "dhcpv6_server: Unable to insert DHCPv6_Lease (%s) with parent DHCPv6_Server (%s).",
                dl->d6l_duid,
                ds6->ds6_uuid.uuid);
        goto exit;
    }

exit:
    return dl;
}
