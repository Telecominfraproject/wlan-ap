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
static ovsdb_table_t table_IPv6_RouteAdv;

static void callback_IPv6_RouteAdv(
        ovsdb_update_monitor_t *mon,
        struct schema_IPv6_RouteAdv *old,
        struct schema_IPv6_RouteAdv *new);

static bool nm2_ipv6_routeadv_update(
        struct nm2_ipv6_routeadv *ra,
        struct schema_IPv6_RouteAdv *schema);

static ds_tree_t nm2_ipv6_routeadv_list = DS_TREE_INIT(ds_str_cmp, struct nm2_ipv6_routeadv, ra_tnode);

struct nm2_ipv6_routeadv *nm2_ipv6_routeadv_get(ovs_uuid_t *uuid);
static void nm2_ipv6_routeadv_release(struct nm2_ipv6_routeadv *rel);
static void nm2_ipv6_routeadv_prefixes_update(uuidset_t *us, enum uuidset_event type, reflink_t *obj);
static void nm2_ipv6_routeadv_rdnss_update(uuidset_t *us, enum uuidset_event type, reflink_t *obj);
static void nm2_ipv6_routeadv_set(struct nm2_ipv6_routeadv *ra, bool enable);

/*
 * IPv6 DNS domain search list
 */
struct ipv6_routeadv_dnssl
{
    char               *ds_dnssl;       /* IPv6 search domain */
    synclist_node_t     ds_snode;

};

int ipv6_routeadv_dnssl_cmp(void *_a, void *_b)
{
    struct ipv6_routeadv_dnssl *a = _a;
    struct ipv6_routeadv_dnssl *b = _b;

    return strcmp(a->ds_dnssl, b->ds_dnssl);
}

static synclist_fn_t ipv6_routeadv_dnssl_sync;

/*
 * Initialize table monitors
 */
void nm2_ipv6_routeadv_init(void)
{
    LOG(INFO, "Initializing NM IPv6_RouteAdv monitoring.");

    OVSDB_TABLE_INIT_NO_KEY(IPv6_RouteAdv);
    OVSDB_TABLE_MONITOR(IPv6_RouteAdv, false);
}

/*
 * OVSDB monitor update callback for IPv6_RouteAdv
 */
void callback_IPv6_RouteAdv(
        ovsdb_update_monitor_t *mon,
        struct schema_IPv6_RouteAdv *old,
        struct schema_IPv6_RouteAdv *new)
{
    struct nm2_ipv6_routeadv *ra;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            ra = nm2_ipv6_routeadv_get(&new->_uuid);
            reflink_ref(&ra->ra_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            ra = ds_tree_find(&nm2_ipv6_routeadv_list, new->_uuid.uuid);
            if (ra == NULL)
            {
                LOG(ERR, "ipv6_routeadv: IPv6_RouteAdv with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            ra = ds_tree_find(&nm2_ipv6_routeadv_list, old->_uuid.uuid);
            if (ra == NULL)
            {
                LOG(ERR, "ipv6_routeadv: IPv6_RouteAdv with uuid %s not found in cache. Cannot delete.", old->_uuid.uuid);
                return;
            }

            nm2_ipv6_routeadv_set(ra, false);

            /* Decrease the reference count */
            reflink_ref(&ra->ra_reflink, -1);
            return;

        default:
            LOG(ERR, "ipv6_routeadv: Monitor update error.");
            return;
    }

    if (!nm2_ipv6_routeadv_update(ra, new))
    {
        LOG(ERR, "ipv6_routeadv: Unable to parse IPv6_RouteAdv schema.");
    }

    nm2_ipv6_routeadv_set(ra, true);
}

/*
 * Release IPv6_RouteAdv object */
void nm2_ipv6_routeadv_release(struct nm2_ipv6_routeadv *ra)
{
    reflink_t *ref;

    LOG(TRACE, "IPv6_RouteAdv: Releasing.");

    if (ra->ra_valid)
    {
        ref = nm2_ip_interface_getref(&ra->ra_ip_interface_uuid);
        if (ref == NULL)
        {
            LOG(ERR, "ipv6_routeadv: Unable to acquire reference to IP_Interface:%s (releas).",
                    ra->ra_ip_interface_uuid.uuid);
        }
        else
        {
            reflink_disconnect(&ra->ra_ip_interface_reflink, ref);
        }
    }

    /* Invalidate this address so it gets removed, notify listeners */
    ra->ra_valid = false;
    reflink_signal(&ra->ra_reflink);

    uuidset_fini(&ra->ra_prefixes);
    uuidset_fini(&ra->ra_rdnss);
    reflink_fini(&ra->ra_ip_interface_reflink);
    reflink_fini(&ra->ra_reflink);
    ds_tree_remove(&nm2_ipv6_routeadv_list, ra);

    free(ra);
}

/*
 * IPv6_RouteAdv reflink callback
 */
void nm2_ipv6_routeadv_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_ipv6_routeadv *ra;

    ra = CONTAINER_OF(obj, struct nm2_ipv6_routeadv, ra_reflink);

    /* Object deletion */
    if (sender == NULL)
    {
        LOG(INFO, "dhcp6_client: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, ra->ra_reflink));
        nm2_ipv6_routeadv_release(ra);
        return;
    }

    /* The IP_Interface object either just became ready or new config was pushed, either way push the new config */
    nm2_ipv6_routeadv_set(ra, true);
}

/*
 * IPv6_RouteAdv -> IP_Interface reflink
 */
void nm2_ipv6_routeadv_ip_interface_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_ipv6_routeadv *ra;
    struct nm2_ip_interface *ipi;
    struct ipv6_routeadv_dnssl *ds;

    /* Not interested in deletion events */
    if (sender == NULL) return;

    ra = CONTAINER_OF(obj, struct nm2_ipv6_routeadv, ra_ip_interface_reflink);
    ipi = CONTAINER_OF(sender, struct nm2_ip_interface, ipi_reflink);

    /* IP_Interface object not yet valid */
    if (!ipi->ipi_valid) return;

    /* Issue a refresh of the UUID list */
    uuidset_refresh(&ra->ra_prefixes);
    uuidset_refresh(&ra->ra_rdnss);

    /* Update DNSSL list */
    synclist_foreach(&ra->ra_dnssl, ds)
    {
        LOG(DEBUG, "ipv6_routeadv: %s: Adding (update) DNSSL: %s",
                ipi->ipi_ifname,
                ds->ds_dnssl);

        inet_radv_dnssl(ipi->ipi_iface->if_inet, true, ds->ds_dnssl);
    }

    /* Re-apply config */
    nm2_ipv6_routeadv_set(ra, true);
}


/*
 * Get a reference to the nm2_ipv6_routeadv structure associated
 * with uuid.
 */
struct nm2_ipv6_routeadv *nm2_ipv6_routeadv_get(ovs_uuid_t *uuid)
{
    struct nm2_ipv6_routeadv *ra;

    ra = ds_tree_find(&nm2_ipv6_routeadv_list, uuid->uuid);
    if (ra == NULL)
    {
        /* Allocate a new dummy structure and insert it into the cache */
        ra = calloc(1, sizeof(struct nm2_ipv6_routeadv));
        ra->ra_uuid = *uuid;

        reflink_init(&ra->ra_reflink, "IPv6_RouteAdv");
        reflink_set_fn(&ra->ra_reflink, nm2_ipv6_routeadv_ref_fn);

        reflink_init(&ra->ra_ip_interface_reflink, "IPv6_RouteAdv.interface");
        reflink_set_fn(&ra->ra_ip_interface_reflink, nm2_ipv6_routeadv_ip_interface_ref_fn);

        /*
         * Initialize UUID references
         */
        uuidset_init(
                &ra->ra_prefixes,
                "IPv6_RouteAdv.prefixes",
                nm2_ipv6_prefix_getref,
                nm2_ipv6_routeadv_prefixes_update);

        uuidset_init(
                &ra->ra_rdnss,
                "IPv6_RouteAdv.rdnss",
                nm2_ipv6_address_getref,
                nm2_ipv6_routeadv_rdnss_update);

        /* DNSSL synclist */
        synclist_init(
                &ra->ra_dnssl,
                ipv6_routeadv_dnssl_cmp,
                struct ipv6_routeadv_dnssl,
                ds_snode,
                ipv6_routeadv_dnssl_sync);

       ds_tree_insert(&nm2_ipv6_routeadv_list, ra, ra->ra_uuid.uuid);
    }

    return ra;
}

/*
 * IPv6_RouteAdv.prefixes update; this function is called whenever a prefix is added, removed or modified
 */
void nm2_ipv6_routeadv_prefixes_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Just re-apply the configuration */
    struct nm2_ipv6_routeadv *ra = CONTAINER_OF(us, struct nm2_ipv6_routeadv, ra_prefixes);
    struct nm2_ipv6_prefix *ip6p = CONTAINER_OF(remote, struct nm2_ipv6_prefix, ip6p_reflink);

    bool add;

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

    /* Lookup interface name */
    reflink_t *ref;
    struct nm2_ip_interface *ipi;

    ref = nm2_ip_interface_getref(&ra->ra_ip_interface_uuid);
    if (ref == NULL)
    {
        LOG(ERR, "ipv6_routeadv: Unable to get parent IP_Interface with UUID %s.",
                ra->ra_ip_interface_uuid.uuid);
        return;
    }

    ipi = CONTAINER_OF(ref, struct nm2_ip_interface, ipi_reflink);
    if (ipi->ipi_iface == NULL)
    {
        LOG(DEBUG, "ipv6_routeadv: Parent interface %s with UUID %s is not yet valid.",
                ipi->ipi_ifname,
                ra->ra_ip_interface_uuid.uuid);
        return;
    }

    if (!inet_radv_prefix(
                ipi->ipi_iface->if_inet,
                add,
                &ip6p->ip6p_addr,
                ip6p->ip6p_autonomous,
                ip6p->ip6p_on_link))
    {
        LOG(ERR, "ipv6_routeadv: %s: Error deleting/adding[%d] prefix "PRI_osn_ip6_addr".",
                ipi->ipi_ifname,
                add,
                FMT_osn_ip6_addr(ip6p->ip6p_addr));
        return;
    }

    inet_commit(ipi->ipi_iface->if_inet);
}

/*
 * IPv6_RouteAdv.rdnss update; this function is called whenever a RDNSS is added, removed or modified
 */
void nm2_ipv6_routeadv_rdnss_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Just re-apply the configuration */
    struct nm2_ipv6_routeadv *ra = CONTAINER_OF(us, struct nm2_ipv6_routeadv, ra_rdnss);
    struct nm2_ipv6_address *ip6 = CONTAINER_OF(remote, struct nm2_ipv6_address, ip6_reflink);

    bool add;

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

    /* Lookup interface name */
    reflink_t *ref;
    struct nm2_ip_interface *ipi;

    ref = nm2_ip_interface_getref(&ra->ra_ip_interface_uuid);
    if (ref == NULL)
    {
        LOG(ERR, "ipv6_routeadv: RDNSS: Unable to get parent IP_Interface with UUID %s.",
                ra->ra_ip_interface_uuid.uuid);
        return;
    }

    ipi = CONTAINER_OF(ref, struct nm2_ip_interface, ipi_reflink);
    if (ipi->ipi_iface == NULL)
    {
        LOG(DEBUG, "ipv6_routeadv: RDNSS: Parent interface %s with UUID %s is not yet valid.",
                ipi->ipi_ifname,
                ra->ra_ip_interface_uuid.uuid);
        return;
    }

    LOG(DEBUG, "ipv6_routeadv: RDNSS del/add[%d] iface=%s, add=%d, addr="PRI_osn_ip6_addr,
            add,
            ipi->ipi_ifname,
            add,
            FMT_osn_ip6_addr(ip6->ip6_addr));

    if (!inet_radv_rdnss(
                ipi->ipi_iface->if_inet,
                add,
                &ip6->ip6_addr))
    {
        LOG(ERR, "ipv6_routeadv: %s: Error deleting/adding[%d] RDNSS address "PRI_osn_ip6_addr".",
                ipi->ipi_ifname,
                add,
                FMT_osn_ip6_addr(ip6->ip6_addr));
        return;
    }

    inet_commit(ipi->ipi_iface->if_inet);
}


/*
 * Update a struct nm2_ipv6_routeadv from the schema
 */
bool nm2_ipv6_routeadv_update(
        struct nm2_ipv6_routeadv *ra,
        struct schema_IPv6_RouteAdv *schema)
{
    reflink_t *ref;
    int ii;

    bool retval = false;

    /*
     * The IPv6_RouteAdv is changing, flag it as invalid and notify listeners
     */
    if (ra->ra_valid)
    {
        ra->ra_valid = false;
        reflink_signal(&ra->ra_reflink);

        /*
         * Remove reflink to IP_Interface
         */
        ref = nm2_ip_interface_getref(&ra->ra_ip_interface_uuid);
        if (ref == NULL)
        {
            LOG(ERR, "ipv6_routeadv: Unable to acquire reference to IP_Interface:%s (delete)",
                    ra->ra_ip_interface_uuid.uuid);
        }
        else
        {
            reflink_disconnect(&ra->ra_ip_interface_reflink, ref);
        }
    }

    ra->ra_opts = (struct osn_ip6_radv_options)OSN_IP6_RADV_OPTIONS_INIT;

    if (schema->managed_exists) ra->ra_opts.ra_managed = schema->managed;
    if (schema->other_config_exists) ra->ra_opts.ra_other_config = schema->other_config;
    if (schema->home_agent_exists) ra->ra_opts.ra_home_agent = schema->home_agent;
    if (schema->max_adv_interval_exists) ra->ra_opts.ra_max_adv_interval = schema->max_adv_interval;
    if (schema->min_adv_interval_exists) ra->ra_opts.ra_min_adv_interval = schema->min_adv_interval;
    if (schema->default_lifetime_exists) ra->ra_opts.ra_default_lft = schema->default_lifetime;
    if (schema->mtu_exists) ra->ra_opts.ra_mtu = schema->mtu;
    if (schema->reachable_time_exists) ra->ra_opts.ra_reachable_time = schema->reachable_time;
    if (schema->retrans_timer_exists) ra->ra_opts.ra_retrans_timer = schema->retrans_timer;
    if (schema->current_hop_limit_exists) ra->ra_opts.ra_current_hop_limit = schema->current_hop_limit;

    if (strcmp(schema->preferred_router, "low") == 0)
    {
        ra->ra_opts.ra_preferred_router = 0;
    }
    else if (strcmp(schema->preferred_router, "medium") == 0)
    {
       ra->ra_opts.ra_preferred_router = 1;
    }
    else if (strcmp(schema->preferred_router, "high") == 0)
    {
       ra->ra_opts.ra_preferred_router = 2;
    }

    if (strscpy(ra->ra_status, schema->status, sizeof(ra->ra_status)) < 0)
    {
        LOG(ERR, "ipv6_routeadv: IPv6_RouteAdv.status is too long: %s.", schema->status);
        goto error;
    }

    /*
     * Re-establish reflink to IP_Interface
     */
    ra->ra_ip_interface_uuid = schema->interface;

    ref = nm2_ip_interface_getref(&ra->ra_ip_interface_uuid);
    if (ref == NULL)
    {
        LOG(ERR, "ipv6_routeadv: Unable to acquire reference to IP_Interface:%s (add)",
                ra->ra_ip_interface_uuid.uuid);
        goto error;
    }

    /* Establish connection to IP_Interface */
    reflink_connect(&ra->ra_ip_interface_reflink, ref);

    uuidset_set(&ra->ra_prefixes, schema->prefixes, schema->prefixes_len);
    uuidset_set(&ra->ra_rdnss, schema->rdnss, schema->rdnss_len);

    /* Update the DNSSL synclist */
    synclist_begin(&ra->ra_dnssl);
    for (ii = 0; ii < schema->dnssl_len; ii++)
    {
        struct ipv6_routeadv_dnssl ds;
        ds.ds_dnssl = schema->dnssl[ii];
        synclist_add(&ra->ra_dnssl, &ds);
    }
    synclist_end(&ra->ra_dnssl);

    /*
     * Parsing successful, notify listeners that we have a valid structure now
     */
    ra->ra_valid = true;
    reflink_signal(&ra->ra_reflink);

    retval = true;

error:
    return retval;
}

void *ipv6_routeadv_dnssl_sync(synclist_t *list, void *_old, void *_new)
{
    struct ipv6_routeadv_dnssl *pnew;
    struct ipv6_routeadv_dnssl *pold;
    struct nm2_ipv6_routeadv *ra;
    struct nm2_ip_interface *ipi;
    reflink_t *ref;

    ra = CONTAINER_OF(list, struct nm2_ipv6_routeadv, ra_dnssl);

    /*
     * Lookup the parent interface
     */
    ipi = NULL;
    if ((ref = nm2_ip_interface_getref(&ra->ra_ip_interface_uuid)) != NULL)
    {
        ipi = CONTAINER_OF(ref, struct nm2_ip_interface, ipi_reflink);
        if (!ipi->ipi_valid) ipi = NULL;
    }

    pnew = _new;
    pold = _old;

    if (pold == NULL)
    {
        /* Add entry */
        pold = calloc(1, sizeof(struct ipv6_routeadv_dnssl));
        pold->ds_dnssl = strdup(pnew->ds_dnssl);

        if (ipi != NULL)
        {
            LOG(DEBUG, "ipv6_routeadv: %s: adding DNSSL: %s",
                    ipi->ipi_ifname,
                    pold->ds_dnssl);
            inet_radv_dnssl(ipi->ipi_iface->if_inet, true, pold->ds_dnssl);
        }
    }
    else if (pnew == NULL)
    {
        if (ipi != NULL)
        {
            LOG(DEBUG, "ipv6_routeadv: %s: Removing DNSSL: %s",
                    ipi->ipi_ifname,
                    pold->ds_dnssl);
            inet_radv_dnssl(ipi->ipi_iface->if_inet, false, pold->ds_dnssl);
        }

        /* Free entry */
        free(pold->ds_dnssl);
        free(pold);
        pold = NULL;
    }

    return pold;
}

void nm2_ipv6_routeadv_set(struct nm2_ipv6_routeadv *ra, bool enable)
{
    struct nm2_iface *piface = nm2_ip_interface_iface_get(&ra->ra_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(DEBUG, "ipv6_routeadv: Interface with UUID %s does nt have an inet object yet.",
                ra->ra_ip_interface_uuid.uuid);
        return;
    }

    if (!ra->ra_valid) return;

    inet_radv(piface->if_inet, enable, &ra->ra_opts);
    inet_commit(piface->if_inet);
}
