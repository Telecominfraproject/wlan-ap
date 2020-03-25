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

/*
 *
 * ===========================================================================
 *  Inet Base class implementation -- implements some boilerplate code
 *  that should be more or less common to the majority of inet class
 *  implementations. For example, this class takes care of applying
 *  the config only once, DHCP client/server settings, firewall and UPnP
 *  settings ...
 * ===========================================================================
 */
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"
#include "util.h"
#include "target.h"
#include "version.h"
#include "ds.h"

#include "inet.h"
#include "inet_base.h"
#include "osn_types.h"

#define WAR_ESW_3313

static bool inet_base_init_ip6(inet_base_t *self);
static bool inet_base_firewall_commit(inet_base_t *self, bool start);
static bool inet_base_igmp_commit(inet_base_t *self, bool start);
static bool inet_base_upnp_commit(inet_base_t *self, bool start);
static void inet_base_dhcp_server_status(osn_dhcp_server_t *ds, struct osn_dhcp_server_status *st);
static void *inet_base_dhcp_server_lease_sync(synclist_t *list, void *_old, void *_new);
static bool inet_base_dhcp_client_commit(inet_base_t *self, bool start);
static bool inet_base_dhcp_server_commit(inet_base_t *self, bool start);
static bool inet_base_dns_commit(inet_base_t *self, bool start);
static bool inet_base_dhsnif_commit(inet_base_t *self, bool start);
static bool inet_base_inet6_commit(inet_base_t *self, bool start);
static bool inet_base_dhcp6_client_commit(inet_base_t *self, bool start);
static bool inet_base_radv_commit(inet_base_t *self, bool start);
static bool inet_base_dhcp6_server_commit(inet_base_t *self, bool start);
static void inet_base_upnp_start_stop(inet_base_t *self);
static osn_dhcpv6_client_status_fn_t inet_base_dhcp6_client_notify_fn;
static osn_dhcpv6_server_status_fn_t inet_base_dhcp6_server_notify_fn;
static ds_key_cmp_t osn_dhcpv6_server_lease_cmp;

/* DHCPv4 reservation list cache */
struct dhcp_reservation_node
{
    osn_ip_addr_t       rn_ipaddr;
    osn_mac_addr_t      rn_macaddr;
    char               *rn_hostname;
    ds_tree_node_t      rn_tnode;
};

/* DHCPv4 leased entries cache */
struct dhcp_lease_node
{
    struct osn_dhcp_server_lease    dl_lease;
    synclist_node_t                 dl_snode;
};

int dhcp_lease_node_cmp(void *_a, void *_b)
{
    int rc;

    struct dhcp_lease_node *a = _a;
    struct dhcp_lease_node *b = _b;

    rc = osn_mac_addr_cmp(&a->dl_lease.dl_hwaddr, &b->dl_lease.dl_hwaddr);
    if (rc != 0) return rc;

    rc = osn_ip_addr_cmp(&a->dl_lease.dl_ipaddr, &b->dl_lease.dl_ipaddr);
    if (rc != 0) return rc;

    return 0;
}

/* Structure for building lists of osn_ip6_addr structures */
struct osn_ip6_addr_node
{
    osn_ip6_addr_t      in_addr;
    ds_tree_node_t      in_tnode;
};

/*
 * Router Advertisement prefix
 */
struct ip6_prefix_node
{
    osn_ip6_addr_t      pr_addr;            /* IPv6 prefix address */
    bool                pr_autonomous;      /* Autonomous flag */
    bool                pr_onlink;          /* On-Link flag */
    ds_tree_node_t      pr_tnode;
};

struct dnssl_node
{
    char                dn_dnssl[128];
    ds_tree_node_t      dn_tnode;
};

struct ip6_addr_status_node
{
    struct inet_ip6_addr_status         as_addr;
    synclist_node_t                     as_snode;
};

ds_key_cmp_t ip6_addr_status_node_cmp;
synclist_fn_t ip6_addr_status_node_sync;

struct ip6_neigh_status_node
{
    struct inet_ip6_neigh_status        ns_neigh;
    synclist_node_t                     ns_snode;
};

ds_key_cmp_t ip6_neigh_status_node_cmp;
synclist_fn_t ip6_neigh_status_node_sync;

/* DHCPv6 server prefix list */
struct osn_dhcpv6_server_prefix_node
{
    struct osn_dhcpv6_server_prefix     sp_prefix;
    ds_tree_node_t                      sp_tnode;
};

/* DHCPv6 server send options list */
struct osn_dhcpv6_server_optsend_node
{
    int                                 so_tag;
    char                               *so_data;
    ds_tree_node_t                      so_tnode;
};

/* DHCPv6 server lease list */
struct osn_dhcpv6_server_lease_node
{
    struct osn_dhcpv6_server_lease      sl_lease;
    ds_tree_node_t                      sl_tnode;
};

#define VAL_CMP(x, a, b)        \
do                              \
{                               \
    (x) |= (a) != (b);          \
    (a) = (b);                  \
} while (0)

/*
 * ===========================================================================
 *  Constructor and destructor methods
 * ===========================================================================
 */

/**
 * Statically initialize a new instance of inet_base_t
 */
bool inet_base_init(inet_base_t *self, const char *ifname)
{
    TRACE("%p %s", self, ifname);

    memset(self, 0, sizeof(*self));

    self->in_static_addr = OSN_IP_ADDR_INIT;
    self->in_static_netmask = OSN_IP_ADDR_INIT;
    self->in_static_bcast = OSN_IP_ADDR_INIT;
    self->in_static_gwaddr = OSN_IP_ADDR_INIT;
    self->in_dhcps_lease_start = OSN_IP_ADDR_INIT;
    self->in_dhcps_lease_stop = OSN_IP_ADDR_INIT;
    self->in_dns_primary = OSN_IP_ADDR_INIT;
    self->in_dns_secondary = OSN_IP_ADDR_INIT;

    if (STRSCPY(self->inet.in_ifname, ifname) < 0)
    {
        LOG(ERR, "inet_base: %s: Interface name is too long, cannot instantiate class.", ifname);
        return false;
    }

    /*
     * Override default methods
     */
    self->inet.in_dtor_fn                       = inet_base_dtor;
    self->inet.in_interface_enable_fn           = inet_base_interface_enable;
    self->inet.in_network_enable_fn             = inet_base_network_enable;
    self->inet.in_mtu_set_fn                    = inet_base_mtu_set;
    self->inet.in_nat_enable_fn                 = inet_base_nat_enable;
    self->inet.in_igmp_enable_fn                = inet_base_igmp_enable;
    self->inet.in_upnp_mode_set_fn              = inet_base_upnp_mode_set;
    self->inet.in_assign_scheme_set_fn          = inet_base_assign_scheme_set;
    self->inet.in_ipaddr_static_set_fn          = inet_base_ipaddr_static_set;
    self->inet.in_gateway_set_fn                = inet_base_gateway_set;
    self->inet.in_portforward_set_fn            = inet_base_portforward_set;
    self->inet.in_portforward_del_fn            = inet_base_portforward_del;
    self->inet.in_dhcpc_option_request_fn       = inet_base_dhcpc_option_request;
    self->inet.in_dhcpc_option_set_fn           = inet_base_dhcpc_option_set;
    self->inet.in_dhcpc_option_notify_fn        = inet_base_dhcpc_option_notify;
    self->inet.in_dhcps_enable_fn               = inet_base_dhcps_enable;
    self->inet.in_dhcps_lease_set_fn            = inet_base_dhcps_lease_set;
    self->inet.in_dhcps_range_set_fn            = inet_base_dhcps_range_set;
    self->inet.in_dhcps_option_set_fn           = inet_base_dhcps_option_set;
    self->inet.in_dhcps_lease_notify_fn         = inet_base_dhcps_lease_notify;
    self->inet.in_dhcps_rip_set_fn              = inet_base_dhcps_rip_set;
    self->inet.in_dhcps_rip_del_fn              = inet_base_dhcps_rip_del;
    self->inet.in_dns_set_fn                    = inet_base_dns_set;
    self->inet.in_dhsnif_lease_notify_fn        = inet_base_dhsnif_lease_notify;
    self->inet.in_route_notify_fn               = inet_base_route_notify;
    self->inet.in_commit_fn                     = inet_base_commit;
    self->inet.in_state_get_fn                  = inet_base_state_get;

    /*
     * IPv6 support
     */
    self->inet.in_ip6_addr_fn                   = inet_base_ip6_addr;
    self->inet.in_ip6_dns_fn                    = inet_base_ip6_dns;
    self->inet.in_ip6_addr_status_notify_fn     = inet_base_ip6_addr_status_notify;
    self->inet.in_ip6_neigh_status_notify_fn    = inet_base_ip6_neigh_status_notify;
    self->inet.in_dhcp6_client_fn               = inet_base_dhcp6_client;
    self->inet.in_dhcp6_client_option_request_fn= inet_base_dhcp6_client_option_request;
    self->inet.in_dhcp6_client_option_send_fn   = inet_base_dhcp6_client_option_send;
    self->inet.in_dhcp6_client_notify_fn        = inet_base_dhcp6_client_notify;
    self->inet.in_dhcp6_server_fn               = inet_base_dhcp6_server;
    self->inet.in_dhcp6_server_prefix_fn        = inet_base_dhcp6_server_prefix;
    self->inet.in_dhcp6_server_option_send_fn   = inet_base_dhcp6_server_option_send;
    self->inet.in_dhcp6_server_lease_fn         = inet_base_dhcp6_server_lease;
    self->inet.in_dhcp6_server_notify_fn        = inet_base_dhcp6_server_notify;
    self->inet.in_radv_fn                       = inet_base_radv;
    self->inet.in_radv_prefix_fn                = inet_base_radv_prefix;
    self->inet.in_radv_rdnss_fn                 = inet_base_radv_rdnss;
    self->inet.in_radv_dnssl_fn                 = inet_base_radv_dnssl;

    /*
     * inet_base methods
     */
    self->in_service_commit_fn                  = inet_base_service_commit;

    /*
     * Initialize data
     */
    ds_tree_init(&self->in_dhcps_reservation_list, osn_mac_addr_cmp, struct dhcp_reservation_node, rn_tnode);

    synclist_init(
            &self->in_dhcps_lease_list,
            dhcp_lease_node_cmp,
            struct dhcp_lease_node,
            dl_snode,
            inet_base_dhcp_server_lease_sync);

    ds_tree_init(
            &self->in_ip6addr_list,
            osn_ip6_addr_nolft_cmp,
            struct osn_ip6_addr_node,
            in_tnode);

    ds_tree_init(
            &self->in_ip6dns_list,
            osn_ip6_addr_cmp,
            struct osn_ip6_addr_node, in_tnode);

    synclist_init(
            &self->in_ip6_addr_status_list,
            ip6_addr_status_node_cmp,
            struct ip6_addr_status_node,
            as_snode,
            ip6_addr_status_node_sync);

    synclist_init(
            &self->in_ip6_neigh_status_list,
            ip6_neigh_status_node_cmp,
            struct ip6_neigh_status_node,
            ns_snode,
            ip6_neigh_status_node_sync);

    ds_tree_init(&self->in_radv_prefix_list, osn_ip6_addr_cmp, struct ip6_prefix_node, pr_tnode);
    ds_tree_init(&self->in_radv_rdnss_list, osn_ip6_addr_cmp, struct osn_ip6_addr_node, in_tnode);
    ds_tree_init(&self->in_radv_dnssl_list, ds_str_cmp, struct dnssl_node, dn_tnode);

    /* DHCPv6 server prefix list */
    ds_tree_init(
            &self->in_dhcp6_server_prefix_list,
            osn_ip6_addr_cmp,
            struct osn_dhcpv6_server_prefix_node,
            sp_tnode);

    /* DHCPv6 server send options list */
    ds_tree_init(
            &self->in_dhcp6_server_optsend_list,
            ds_int_cmp,
            struct osn_dhcpv6_server_optsend_node,
            so_tnode);

    /* DHCPv6 server lease list */
    ds_tree_init(
            &self->in_dhcp6_server_lease_list,
            osn_dhcpv6_server_lease_cmp,
            struct osn_dhcpv6_server_lease_node,
            sl_tnode);

    self->in_fw = inet_fw_new(self->inet.in_ifname);
    if (self->in_fw == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating FW instance.", self->inet.in_ifname);
        goto error;
    }

    self->in_igmp = inet_igmp_new(self->inet.in_ifname);
    if (self->in_igmp == NULL)
    {
        LOG(ERR, "inet_base: %s: Ignoring creating IGMP snooping instance. ", self->inet.in_ifname);
    }

    self->in_upnp = osn_upnp_new(self->inet.in_ifname);
    if (self->in_upnp == NULL)
    {
        LOG(WARN, "inet_base: %s: Error initializing UPnP instance.", self->inet.in_ifname);
        goto error;
    }

    if (!inet_base_init_ip6(self))
    {
        LOG(ERR, "inet_base: %s: Error creating IPv6 instance.", self->inet.in_ifname);
        goto error;
    }

    self->in_dhcpc = osn_dhcp_client_new(self->inet.in_ifname);
    if (self->in_dhcpc == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating DHCP client instance.", self->inet.in_ifname);
        goto error;
    }

    self->in_dhsnif = inet_dhsnif_new(self->inet.in_ifname);
    if (self->in_dhsnif == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating DHCP sniffing instance.", self->inet.in_ifname);
        goto error;
    }

    self->in_dns = inet_dns_new(self->inet.in_ifname);
    if (self->in_dns == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating DNS instance.", self->inet.in_ifname);
        goto error;
    }

    self->in_route = osn_route_new(self->inet.in_ifname);
    if (self->in_route == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating ROUTE instance.", self->inet.in_ifname);
        goto error;
    }

    /*
     * Define the unit dependency tree structure
     */
    self->in_units =
            inet_unit_s(INET_BASE_INTERFACE,
                    inet_unit(INET_BASE_FIREWALL,
                        inet_unit(INET_BASE_UPNP, NULL),
                        NULL),
                    inet_unit(INET_BASE_MTU, NULL),
                    inet_unit_s(INET_BASE_NETWORK,
                            inet_unit_s(INET_BASE_SCHEME_NONE, NULL),
                            inet_unit(INET_BASE_SCHEME_STATIC,
                                    inet_unit(INET_BASE_DHCP_SERVER, NULL),
                                    inet_unit(INET_BASE_DNS, NULL),
                                    NULL),
                            inet_unit(INET_BASE_SCHEME_DHCP, NULL),
                            inet_unit(INET_BASE_DHCPSNIFF, NULL),
                            inet_unit(INET_BASE_IGMP, NULL),
                            NULL),
                    inet_unit(INET_BASE_INET6,
                        inet_unit(INET_BASE_RADV, NULL),
                        inet_unit(INET_BASE_DHCP6_SERVER, NULL),
                        NULL),
                    inet_unit(INET_BASE_DHCP6_CLIENT, NULL),
                    NULL);
    if (self->in_units == NULL)
    {
        LOG(ERR, "inet_base: %s: Error initializing units structure.", self->inet.in_ifname);
        goto error;
    }

    static bool once = true;

    if (once)
    {
        osn_dhcp_server_t *ds;

        /*
         * Instantiate the DHCPv4 object at least once. This helps with cases
         * where CONFIG_OSN_DNSMASQ_KEEP_RUNNING is set, but a DHCPv4 server
         * instance is never created.
         *
         * On certain platforms (BCM), dnsmasq is essential for onboarding as
         * the default /etc/resolv.conf file points to 127.0.0.1 (due to
         * glibc issues).
         */
        ds = osn_dhcp_server_new("lo");
        if (ds != NULL)
        {
            osn_dhcp_server_del(ds);
        }

        once = false;
    }

    return true;

error:
    inet_fini(&self->inet);
    return false;
}

bool inet_base_dtor(inet_t *super)
{
    ds_tree_iter_t iter;
    int ii;

    bool retval = true;

    inet_base_t *self = (inet_base_t *)super;

    if (self->in_units != NULL)
    {
        /* Stop the top service (INET_BASE_INTERFACE) -- this will effectively shutdown ALL services */
        inet_unit_stop(self->in_units, INET_BASE_INTERFACE);
        if (!inet_commit(&self->inet))
        {
            LOG(WARN, "inet_base: %s: Error shutting down services.", self->inet.in_ifname);
            retval = false;
        }
    }

    /*
     * Cleanup DHCPv4 server structures
     */

    /* Delete osn_dhcp_server_t object */
    if (self->in_dhcps != NULL && !osn_dhcp_server_del(self->in_dhcps))
    {
        LOG(WARN, "inet_base: %s: Error freeing DHCP server object.", self->inet.in_ifname);
        retval = false;
    }

    /* Cleanup DHCPv4 server options */
    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (self->in_dhcps_opts[ii] != NULL)
        {
            free(self->in_dhcps_opts[ii]);
            self->in_dhcps_opts[ii] = NULL;
        }
    }

    /* Remove IP reservation entries */
    struct dhcp_reservation_node *rn;
    ds_tree_foreach_iter(&self->in_dhcps_reservation_list, rn, &iter)
    {
        ds_tree_iremove(&iter);
        if (rn->rn_hostname != NULL) free(rn->rn_hostname);
        free(rn);
    }

    /* Remove cached DHCP lease entries */
    synclist_begin(&self->in_dhcps_lease_list);
    synclist_end(&self->in_dhcps_lease_list);

    if (self->in_dhcpc != NULL && !osn_dhcp_client_del(self->in_dhcpc))
    {
        LOG(WARN, "inet_base: %s: Error freeing DHCP client objects.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_fw != NULL && !inet_fw_del(self->in_fw))
    {
        LOG(WARN, "inet_base: %s: Error freeing Firewall objects.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_igmp != NULL && !inet_igmp_del(self->in_igmp))
    {
        LOG(WARN, "inet_base: %s: Error freeing IGMP snooping objects.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_upnp != NULL && !osn_upnp_del(self->in_upnp))
    {
        LOG(WARN, "inet_base: %s: Error freeing UPnP objects.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_dhsnif != NULL && !inet_dhsnif_del(self->in_dhsnif))
    {
        LOG(WARN, "inet_base: %s: Error freeing DHCP sniffing object.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_dns != NULL && !inet_dns_del(self->in_dns))
    {
        LOG(WARN, "inet_base: %s: Error freeing DNS sniffing object.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_route != NULL && !osn_route_del(self->in_route))
    {
        LOG(WARN, "inet_base: %s: Error freeing ROUTE object.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_ip6 != NULL && !osn_ip6_del(self->in_ip6))
    {
        LOG(WARN, "inet_base: %s: Error freeing IPv6 object.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_dhcp6_client != NULL && !osn_dhcpv6_client_del(self->in_dhcp6_client))
    {
        LOG(WARN, "inet_base: %s: Error freeing DHCPv6 client object.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_dhcp6_server != NULL && !osn_dhcpv6_server_del(self->in_dhcp6_server))
    {
        LOG(WARN, "inet_base: %s: Error freeing DHCPv6 server object.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_radv != NULL && !osn_ip6_radv_del(self->in_radv))
    {
        LOG(WARN, "inet_base: %s: Error freeing RouterAdvertisement server object.", self->inet.in_ifname);
        retval = false;
    }

    if (self->in_units != NULL) inet_unit_free(self->in_units);

    struct osn_ip6_addr_node *ip6addr;
    ds_tree_foreach_iter(&self->in_ip6addr_list, ip6addr, &iter)
    {
        ds_tree_iremove(&iter);
        free(ip6addr);
    }

    ds_tree_foreach_iter(&self->in_ip6dns_list, ip6addr, &iter)
    {
        ds_tree_iremove(&iter);
        free(ip6addr);
    }

    /* Remove IPv6 neighbors */
    synclist_begin(&self->in_ip6_neigh_status_list);
    synclist_end(&self->in_ip6_neigh_status_list);

    /* Cleanup options */
    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (self->in_dhcp6_client_send[ii] != NULL)
        {
            free(self->in_dhcp6_client_send[ii]);
            self->in_dhcp6_client_send[ii] = NULL;
        }
    }

    /*
     * DHCPv6 server cleanup list
     */

    /* Cleanup the lease list */
    struct osn_dhcpv6_server_prefix_node *sp;
    ds_tree_foreach_iter(&self->in_dhcp6_server_prefix_list, sp, &iter)
    {
        ds_tree_iremove(&iter);
        free(sp);
    }

    /* Cleanup the send options list */
    struct osn_dhcpv6_server_optsend_node *so;
    ds_tree_foreach_iter(&self->in_dhcp6_server_optsend_list, so, &iter)
    {
        ds_tree_iremove(&iter);
        free(so->so_data);
        free(so);
    }

    /* Cleanup the lease list */
    struct osn_dhcpv6_server_lease_node *sl;
    ds_tree_foreach_iter(&self->in_dhcp6_server_optsend_list, sl, &iter)
    {
        ds_tree_iremove(&iter);
        free(sl);
    }

    /* Cleanup IPv6 RouteAdv prefixes */
    struct ip6_prefix_node *prefix;
    ds_tree_foreach_iter(&self->in_radv_prefix_list, prefix, &iter)
    {
        ds_tree_iremove(&iter);
        free(prefix);
    }

    /* Cleanup IPv6 RouteAdv RDNSS configuration */
    struct ip6_rdnss_node *rdnss;
    ds_tree_foreach_iter(&self->in_radv_rdnss_list, rdnss, &iter)
    {
        ds_tree_iremove(&iter);
        free(rdnss);
    }

    /* Cleanup IPv6 RouteAdv  DNSSL configuration */
    struct ip6_rdnss_node *dnssl;
    ds_tree_foreach_iter(&self->in_radv_dnssl_list, dnssl, &iter)
    {
        ds_tree_iremove(&iter);
        free(dnssl);
    }

    memset(self, 0, sizeof(*self));

    return retval;
}

/**
 * Create a new instance of inet_base_t and return it cast to inet_t*
 */
inet_base_t *inet_base_new(const char *ifname)
{
    inet_base_t *self = malloc(sizeof(inet_base_t));

    if (!inet_base_init(self, ifname))
    {
        free(self);
        return NULL;
    }

    return self;
}

/*
 * ===========================================================================
 *  Interface enable
 * ===========================================================================
 */

/**
 * Interface enable switch
 */
bool inet_base_interface_enable(inet_t *super, bool enabled)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_interface_enabled == enabled) return true;

    self->in_interface_enabled = enabled;

    LOG(INFO, "inet_base: %s: %s interface.", self->inet.in_ifname, enabled ? "Starting" : "Stopping");

    return inet_unit_enable(self->in_units, INET_BASE_INTERFACE, enabled);
}

/*
 * ===========================================================================
 *  Firewall methods
 * ===========================================================================
 */

/**
 * Enable NAT on interface
 */
bool inet_base_nat_enable(inet_t *super, bool enabled)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_nat_enabled == enabled && inet_unit_is_enabled(self->in_units, INET_BASE_FIREWALL))
        return true;

    if (!inet_fw_nat_set(self->in_fw, enabled))
    {
        LOG(ERR, "inet_base: %s: Error setting NAT on interface (%d -> %d)",
                self->inet.in_ifname,
                self->in_nat_enabled,
                enabled);
        return false;
    }

    /* Restart the firewall service */
    self->in_nat_enabled = enabled;

    if (!inet_unit_restart(self->in_units, INET_BASE_FIREWALL, true))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_FIREWALL (NAT)",
                self->inet.in_ifname);
        return false;
    }

    /* NAT settings also affect UPnP, so handle it here */
    inet_base_upnp_start_stop(self);

    return true;
}

/**
 * Enable IGMP snooping on interface
 */
bool inet_base_igmp_enable(inet_t *super, bool enable, int iage, int itsize)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_igmp_enabled == enable &&
            iage == self->in_igmp_age &&
            itsize == self->in_igmp_tsize &&
            inet_unit_is_enabled(self->in_units, INET_BASE_IGMP))
    {
        /* Nothing to do -- configuration unchanged */
        return true;
    }

    /* Push new configuration to the driver */
    if (!inet_igmp_set(self->in_igmp, enable, iage, itsize))
    {
        LOG(ERR, "inet_base: %s: Error setting IGMP snooping on interface (%d -> %d)",
                self->inet.in_ifname,
                self->in_igmp_enabled,
                enable);
        return false;
    }

    /* Cache current values -- we don't want to restart IGMP if the configuration stays the same */
    self->in_igmp_enabled = enable;
    self->in_igmp_age = iage;
    self->in_igmp_tsize = itsize;

    if(enable)
    {
        /* The IGMP unit needs to be started or restarted (due to configuration change) */
        if (!inet_unit_restart(self->in_units, INET_BASE_IGMP, true))
        {
            LOG(ERR, "inet_base: %s: Error restarting INET_BASE_IGMP (NAT)",
                    self->inet.in_ifname);
            return false;
        }
    }
    else
    {
        /* The unit needs to be disabled */
        inet_unit_stop(self->in_units, INET_BASE_IGMP);
    }

    return true;
}

/**
 * UPnP mode selection; if mode is UPNP_MODE_NONE then the INET_BASE_UPNP service is stopped
 */
bool inet_base_upnp_mode_set(inet_t *super, enum osn_upnp_mode mode)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_upnp_mode != mode)
    {
        if (!osn_upnp_set(self->in_upnp, mode))
        {
            LOG(ERR, "inet_base: %s: Error setting UPnP mode on interface.",
                    self->inet.in_ifname);
            return false;
        }

        self->in_upnp_mode = mode;

        /* Stop the service for now, __inet_base_upnp_start_stop(self) will trigger a restart if needed */
        inet_unit_stop(self->in_units, INET_BASE_UPNP);
    }

    inet_base_upnp_start_stop(self);

    return true;
}

void inet_base_upnp_start_stop(inet_base_t *self)
{
    switch (self->in_upnp_mode)
    {
        case UPNP_MODE_NONE:
            /* Stop the UPnP service */
            inet_unit_stop(self->in_units, INET_BASE_UPNP);
            break;

        case UPNP_MODE_INTERNAL:
            /* Start the UPnP service only if we're not in NAT mode */
            inet_unit_enable(self->in_units, INET_BASE_UPNP, !self->in_nat_enabled);
            break;

        case UPNP_MODE_EXTERNAL:
            /* Star the UPnP service only if we're in NAT mode */
            inet_unit_enable(self->in_units, INET_BASE_UPNP, self->in_nat_enabled);
            break;
    }
}

/*
 * ===========================================================================
 *  Network and IP addressing
 * ===========================================================================
 */

/**
 * Interface network enable switch
 */
bool inet_base_network_enable(inet_t *super, bool enable)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_network_enabled == enable) return true;

    self->in_network_enabled = enable;

    return inet_unit_enable(self->in_units, INET_BASE_NETWORK, self->in_network_enabled);
}

/**
 * MTU of the interface
 */
bool inet_base_mtu_set(inet_t *super, int mtu)
{
    inet_base_t *self = (inet_base_t *)super;

    if (mtu == self->in_mtu) return true;

    self->in_mtu = mtu;

    if (self->in_mtu <= 0)
    {
        /* Stop the MTU service */
        if (!inet_unit_stop(self->in_units, INET_BASE_MTU))
        {
            LOG(ERR, "inet_base: %s: Error stopping INET_BASE_MTU.",
                    self->inet.in_ifname);
            return false;
        }
    }
    else
    {
        /* Start the MTU service */
        if (!inet_unit_restart(self->in_units, INET_BASE_MTU, true))
        {
            LOG(ERR, "inet_base: %s: Error restarting INET_BASE_MTU",
                    self->inet.in_ifname);
            return false;
        }
    }

    return true;
}


/**
 * IP assignment scheme -- this is basically a tristate. It toggles between NONE, STATIC and DHCP
 */
bool inet_base_assign_scheme_set(inet_t *super, enum inet_assign_scheme scheme)
{
    inet_base_t *self = (inet_base_t *)super;

    bool retval = true;
    bool none_enable = false;
    bool static_enable = false;
    bool dhcp_enable = false;

    if (self->in_assign_scheme == scheme) return true;

    self->in_assign_scheme = scheme;

    switch (scheme)
    {
        case INET_ASSIGN_NONE:
            none_enable = true;
            break;

        case INET_ASSIGN_STATIC:
            static_enable = true;
            break;

        case INET_ASSIGN_DHCP:
            dhcp_enable = true;
            break;

        default:
            return false;
    }

    if (!inet_unit_enable(self->in_units, INET_BASE_SCHEME_NONE, none_enable))
    {
        LOG(ERR, "inet_base: %s: Error setting enable status for SCHEME_NONE to %d.",
                self->inet.in_ifname,
                none_enable);
        retval = false;
    }

    if (!inet_unit_enable(self->in_units, INET_BASE_SCHEME_STATIC, static_enable))
    {
        LOG(ERR, "inet_base: %s: Error setting enable status for SCHEME_STATIC to %d.",
                self->inet.in_ifname,
                static_enable);
        retval = false;
    }

    if (!inet_unit_enable(self->in_units, INET_BASE_SCHEME_DHCP, dhcp_enable))
    {
        LOG(ERR, "inet_base: %s: Error setting enable status for SCHEME_DHCP to %d.",
                self->inet.in_ifname,
                dhcp_enable);
        retval = false;
    }

    return retval;
}

/**
 * Set the IPv4 address used by the STATIC assignment scheme
 */
bool inet_base_ipaddr_static_set(
        inet_t *super,
        osn_ip_addr_t addr,
        osn_ip_addr_t netmask,
        osn_ip_addr_t bcast)
{
    inet_base_t *self = (inet_base_t *)super;

    bool changed = false;

    changed |= osn_ip_addr_cmp(&self->in_static_addr, &addr) != 0;
    changed |= osn_ip_addr_cmp(&self->in_static_netmask, &netmask) != 0;
    changed |= osn_ip_addr_cmp(&self->in_static_bcast, &bcast) != 0;

    if (!changed) return true;

    self->in_static_addr = addr;
    self->in_static_netmask = netmask;
    self->in_static_bcast = bcast;

    return inet_unit_restart(self->in_units, INET_BASE_SCHEME_STATIC, false);
}

/**
 * Set the default gateway -- used by the STATIC assignment scheme
 */
bool inet_base_gateway_set(inet_t *super, osn_ip_addr_t gwaddr)
{
    inet_base_t *self = (inet_base_t *)super;

    if (osn_ip_addr_cmp(&self->in_static_gwaddr, &gwaddr) == 0) return true;

    self->in_static_gwaddr = gwaddr;

    return inet_unit_restart(self->in_units, INET_BASE_SCHEME_STATIC, false);
}

bool inet_base_portforward_set(inet_t *super, const struct inet_portforward *pf)
{
    inet_base_t *self = (inet_base_t *)super;

    if (inet_fw_portforward_get(self->in_fw, pf))
    {
        /* If the portforwarding entry already exists, return success */
        return true;
    }

    /* Add the portforwarding entry and restart services */
    if (!inet_fw_portforward_set(self->in_fw, pf))
    {
        LOG(ERR, "inet_base: %s: Error setting port forwarding.", self->inet.in_ifname);
        return false;
    }

    return inet_unit_restart(self->in_units, INET_BASE_FIREWALL, false);
}

bool inet_base_portforward_del(inet_t *super, const struct inet_portforward *pf)
{
    inet_base_t *self = (inet_base_t *)super;

    if (!inet_fw_portforward_del(self->in_fw, pf))
    {
        LOG(ERR, "inet_base: %s: Error deleting port forwarding.", self->inet.in_ifname);
        return false;
    }

    return inet_unit_restart(self->in_units, INET_BASE_FIREWALL, false);
}


/*
 * ===========================================================================
 *  DHCP Client methods
 * ===========================================================================
 */
bool inet_base_dhcpc_option_request(inet_t *super, enum osn_dhcp_option opt, bool req)
{
    inet_base_t *self = (void *)super;
    bool _req;
    const char *_value;

    if (!osn_dhcp_client_opt_get(self->in_dhcpc, opt, &_req, &_value))
    {
        LOG(ERR, "inet_base: %s: Error retrieving DHCP client options.", self->inet.in_ifname);
        return false;
    }

    /* No change required */
    if (req == _req) return true;

    if (!osn_dhcp_client_opt_request(self->in_dhcpc, opt, req))
    {
        LOG(ERR, "inet_base: %s: Error requesting option: %d", self->inet.in_ifname, opt);
        return false;
    }

    /* Restart the DHCP client if necessary */
    return inet_unit_restart(self->in_units, INET_BASE_SCHEME_DHCP, false);
}

bool inet_base_dhcpc_option_set(inet_t *super, enum osn_dhcp_option opt, const char *value)
{
    inet_base_t *self = (void *)super;
    bool _req;
    const char *_value;

    if (!osn_dhcp_client_opt_get(self->in_dhcpc, opt, &_req, &_value))
    {
        LOG(ERR, "inet_base: %s: Error retrieving DHCP client options.", self->inet.in_ifname);
        return false;
    }

    /* Both values are NULL, no change needed */
    if (value == NULL && _value == NULL)
    {
        return true;
    }
    /* One of the values is NULL while the other is not -- skip the strcmp() test */
    else if (value == NULL || _value == NULL)
    {
        /* pass */
    }
    /* Neither value is NULL, do a string compare */
    else if (strcmp(value, _value) == 0)
    {
        return true;
    }

    if (!osn_dhcp_client_opt_set(self->in_dhcpc, opt, value))
    {
        LOG(ERR, "inet_base: %s: Error setting option: %d:%s", self->inet.in_ifname, opt, value);
        return false;
    }

    /* Restart the DHCP client if necessary */
    return inet_unit_restart(self->in_units, INET_BASE_SCHEME_DHCP, false);
}

bool inet_base_dhcpc_option_notify(inet_t *super, osn_dhcp_client_opt_notify_fn_t *fn, void *ctx)
{
    inet_base_t *self = (void *)super;

    return osn_dhcp_client_opt_notify_set(self->in_dhcpc, fn, ctx);
}

/*
 * ===========================================================================
 *  DHCP Server methods
 * ===========================================================================
 */
bool inet_base_dhcps_enable(inet_t *super, bool enabled)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_dhcps_enabled == enabled) return true;

    self->in_dhcps_enabled = enabled;

    /* Start or stop the DHCP server service */
    if (!inet_unit_enable(self->in_units, INET_BASE_DHCP_SERVER, enabled))
    {
        LOG(ERR, "inet_base: %s: Error enabling/disabling INET_BASE_DHCP_SERVER", self->inet.in_ifname);
        return false;
    }

    return true;
}

/**
 * Set the lease time in seconds
 */
bool inet_base_dhcps_lease_set(inet_t *super, int lease_time_s)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_dhcps_lease_time_s == lease_time_s) return true;

    self->in_dhcps_lease_time_s = lease_time_s;

    /* Flag the DHCP server for restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_DHCP_SERVER, false))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_DHCP_SERVER", self->inet.in_ifname);
        return false;
    }

    return true;
}

/**
 * Set the DHCP lease range interval (from IP to IP)
 */
bool inet_base_dhcps_range_set(inet_t *super, osn_ip_addr_t start, osn_ip_addr_t stop)
{
    inet_base_t *self = (inet_base_t *)super;

    bool changed = false;

    changed |= osn_ip_addr_cmp(&self->in_dhcps_lease_start, &start) != 0;
    changed |= osn_ip_addr_cmp(&self->in_dhcps_lease_stop, &stop) != 0;

    if (!changed) return true;

    self->in_dhcps_lease_start = start;
    self->in_dhcps_lease_stop = stop;

    /* Flag the DHCP server for restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_DHCP_SERVER, false))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_DHCP_SERVER", self->inet.in_ifname);
        return false;
    }

    return true;
}

/**
 * Set various options -- these will be sent back to DHCP clients
 */
bool inet_base_dhcps_option_set(inet_t *super, enum osn_dhcp_option opt, const char *value)
{
    inet_base_t *self = (inet_base_t *)super;

    bool changed = false;

    if (self->in_dhcps_opts[opt] != NULL && value != NULL)
    {
        changed = (strcmp(self->in_dhcps_opts[opt], value) != 0);
    }
    else if (self->in_dhcps_opts[opt] != NULL || value != NULL)
    {
        changed = true;
    }

    if (!changed) return true;

    if (self->in_dhcps_opts[opt] != NULL)
    {
        free(self->in_dhcps_opts[opt]);
        self->in_dhcps_opts[opt] = NULL;
    }

    if (value != NULL) self->in_dhcps_opts[opt] = strdup(value);

    /* Flag the DHCP server for restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_DHCP_SERVER, false))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_DHCP_SERVER", self->inet.in_ifname);
        return false;
    }

    return true;
}

bool inet_base_dhcps_lease_notify(inet_t *super, inet_dhcp_lease_fn_t *fn)
{
    inet_base_t *self = (inet_base_t *)super;

    self->in_dhcps_lease_fn = fn;

    return true;
}

/*
 * Register a IP reservation entry
 */
bool inet_base_dhcps_rip_set(
        inet_t *super,
        osn_mac_addr_t macaddr,
        osn_ip_addr_t ip4addr,
        const char *hostname)
{
    struct dhcp_reservation_node *rn;

    inet_base_t *self = (inet_base_t *)super;

    bool changed = false;

    rn = ds_tree_find(&self->in_dhcps_reservation_list, &macaddr);
    /* Allocate a new entry */
    if (rn == NULL)
    {
        rn = calloc(1, sizeof(struct dhcp_reservation_node));
        rn->rn_macaddr = macaddr;
        rn->rn_ipaddr = OSN_IP_ADDR_INIT;

        /* Insert into the reservation list */
        ds_tree_insert(&self->in_dhcps_reservation_list, rn, &rn->rn_macaddr);

        changed = true;
    }

    if (osn_ip_addr_cmp(&rn->rn_ipaddr, &ip4addr) != 0)
    {
        rn->rn_ipaddr = ip4addr;
        changed = true;
    }

    /* First case, both new and old hostnames are non-null, compare hostnames */
    if (rn->rn_hostname != NULL && hostname != NULL)
    {
        changed = (strcmp(rn->rn_hostname, hostname) != 0);
    }
    /* One or the other is NULL (but but not both)*/
    else if (rn->rn_hostname != NULL || hostname != NULL)
    {
        changed = true;
    }

    /* Nothing to do -- just return */
    if (!changed) return true;

    /*
     * Assign new hostname -- free the old entry if there's any
     */
    if (rn->rn_hostname != NULL)
    {
        free(rn->rn_hostname);
        rn->rn_hostname = NULL;
    }

    if (hostname != NULL) rn->rn_hostname = strdup(hostname);

    /* Flag the DHCP server for restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_DHCP_SERVER, false))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_DHCP_SERVER", self->inet.in_ifname);
        return false;
    }

   return true;
}

bool inet_base_dhcps_rip_del(inet_t *super, osn_mac_addr_t macaddr)
{
    struct dhcp_reservation_node *rn;

    inet_base_t *self = (inet_base_t *)super;

    rn = ds_tree_find(&self->in_dhcps_reservation_list, &macaddr);
    if (rn == NULL)
    {
        LOG(ERR, "inet_base: %s: Unable to delete DHCPv4 reservation entry: "PRI_osn_mac_addr". It does not exist.",
                self->inet.in_ifname,
                FMT_osn_mac_addr(macaddr));

        return false;
    }

    /* Remove DHCP reservation entry */
    ds_tree_remove(&self->in_dhcps_reservation_list, rn);
    if (rn->rn_hostname != NULL) free(rn->rn_hostname);
    free(rn);

    /* Schedule a DHCPv4 server restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_DHCP_SERVER, false))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_DHCP_SERVER", self->inet.in_ifname);
        return false;
    }

    return true;
}

/*
 * ===========================================================================
 *  DNS related settings
 * ===========================================================================
 */
bool inet_base_dns_set(inet_t *super, osn_ip_addr_t primary, osn_ip_addr_t secondary)
{
    inet_base_t *self = (inet_base_t *)super;

    /* No change -- return success */
    if (osn_ip_addr_cmp(&self->in_dns_primary, &primary) == 0 &&
            osn_ip_addr_cmp(&self->in_dns_secondary, &secondary) == 0)
    {
        return true;
    }

    if (!inet_dns_server_set(self->in_dns, primary, secondary))
    {
        LOG(ERR, "inet_base: %s: Error setting DNS server settings.", self->inet.in_ifname);
        return false;
    }

    /* Flag the DNS service for restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_DNS, true))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_DNS", self->inet.in_ifname);
        return false;
    }

    return true;
}

/*
 * ===========================================================================
 *  DHCP sniffing
 * ===========================================================================
 */
bool inet_base_dhsnif_lease_notify(inet_t *super, inet_dhcp_lease_fn_t *func)
{
    inet_base_t *self = (inet_base_t *)super;

    if (self->in_dhsnif_lease_fn == func) return true;

    self->in_dhsnif_lease_fn = func;

    if (!inet_dhsnif_notify(self->in_dhsnif, func, super))
    {
        LOG(ERR, "inet_base: %s: Error setting the DHCP sniffing handler.", self->inet.in_ifname);
        return false;
    }

    /*
     * Restart or Stop the DHCPSNIFF service according to the value of func
     */
    if (func != NULL && !inet_unit_restart(self->in_units, INET_BASE_DHCPSNIFF, true))
    {
        LOG(ERR, "inet_base: %s: Error restarting INET_BASE_DHCPSNIFF", self->inet.in_ifname);
        return false;
    }

    if (func == NULL && !inet_unit_stop(self->in_units, INET_BASE_DHCPSNIFF))
    {
        LOG(ERR, "inet_base: %s: Error stopping INET_BASE_DHCPSNIFF", self->inet.in_ifname);
        return false;
    }

    return true;
}

/*
 * ===========================================================================
 *  Route functions
 * ===========================================================================
 */
bool inet_base_route_notify(inet_t *super, osn_route_status_fn_t *func)
{
    inet_base_t *self = (inet_base_t *)super;

    return osn_route_status_notify(self->in_route, func, self);
}

/*
 * ===========================================================================
 *  Status reporting
 * ===========================================================================
 */
bool inet_base_state_get(inet_t *super, inet_state_t *out)
{
    inet_base_t *self = (inet_base_t *)super;

    *out = INET_STATE_INIT;

    out->in_mtu = self->in_mtu;
    out->in_interface_enabled = inet_unit_status(self->in_units, INET_BASE_INTERFACE);
    out->in_network_enabled = inet_unit_status(self->in_units, INET_BASE_NETWORK);

    out->in_assign_scheme = self->in_assign_scheme;

    out->in_dhcps_enabled = self->in_dhcps_enabled;

    out->in_nat_enabled = false;
    out->in_upnp_mode = UPNP_MODE_NONE;

    if (!inet_fw_state_get(self->in_fw, &out->in_nat_enabled))
    {
        LOG(DEBUG, "inet_base: %s: Error retrieving firewall module state.",
                self->inet.in_ifname);
    }

    if (!osn_dhcp_client_state_get(self->in_dhcpc, &out->in_dhcpc_enabled))
    {
        LOG(DEBUG, "inet_base: %s: Error retrieving DHCP client state.",
                self->inet.in_ifname);
    }

    if (!osn_upnp_get(self->in_upnp, &out->in_upnp_mode))
    {
        LOG(DEBUG, "inet_base: %s: Erro retrieving UPnP mode.", self->inet.in_ifname);
    }

    /*
     * inet_base() has the currently inactive information below, the subclass
     * can fill with live data
     */
    out->in_ipaddr = self->in_static_addr;
    out->in_netmask = self->in_static_netmask;
    out->in_bcaddr = self->in_static_bcast;
    out->in_gateway = self->in_static_gwaddr;

    return true;
}

/*
 * ===========================================================================
 *  Commit / Start & Stop methods
 * ===========================================================================
 */
/**
 * Dispatcher of START/STOP events
 */
bool __inet_base_commit(void *ctx, intptr_t unitid, bool enable)
{
    inet_base_t *self = ctx;

    return inet_service_commit(self, unitid, enable);
}

bool inet_base_commit(inet_t *super)
{
    inet_base_t *self = (inet_base_t *)super;

    LOG(INFO, "inet_base: %s: Commiting new configuration.", self->inet.in_ifname);

    /* Commit all pending units */
    return inet_unit_commit(self->in_units, __inet_base_commit, self);
}

/**
 * Inet_base implementation of start/stop service. The service should be
 * started if @p start is true, otherwise it should be stopped.
 *
 * inet_base takes care of stopping/starting the service only once even if multiple starts or stops
 * are requested.
 *
 * New class implementations should implement the following services
 * - INET_BASE_INETFACE
 * - INET_BASE_NETWORK
 * - INET_BASE_SCHEME_DHCP
 */
bool inet_base_service_commit(
        inet_base_t *self,
        enum inet_base_services srv,
        bool start)
{
    LOG(INFO, "inet_base: %s: Service %s -> %s",
            self->inet.in_ifname,
            inet_base_service_str(srv),
            start ? "start" : "stop");

    switch (srv)
    {
        case INET_BASE_FIREWALL:
            return inet_base_firewall_commit(self, start);

        case INET_BASE_IGMP:
            return inet_base_igmp_commit(self, start);

        case INET_BASE_UPNP:
            return inet_base_upnp_commit(self, start);

        case INET_BASE_SCHEME_DHCP:
            return inet_base_dhcp_client_commit(self, start);

        case INET_BASE_DHCP_SERVER:
            return inet_base_dhcp_server_commit(self, start);

        case INET_BASE_DNS:
            return inet_base_dns_commit(self, start);

        case INET_BASE_DHCPSNIFF:
            return inet_base_dhsnif_commit(self, start);

        case INET_BASE_INET6:
            return inet_base_inet6_commit(self, start);

        case INET_BASE_DHCP6_CLIENT:
            return inet_base_dhcp6_client_commit(self, start);

        case INET_BASE_RADV:
            return inet_base_radv_commit(self, start);

        case INET_BASE_DHCP6_SERVER:
            return inet_base_dhcp6_server_commit(self, start);

        default:
            LOG(INFO, "inet_base: %s: Ignoring service start/stop request: %s -> %d",
                    self->inet.in_ifname,
                    inet_base_service_str(srv),
                    start);
            break;
    }

    return false;
}

/**
 * Start or stop the firewall service
 */
bool inet_base_firewall_commit(inet_base_t *self, bool start)
{
    /* Start service */
    if (start && !inet_fw_start(self->in_fw))
    {
        LOG(ERR, "inet_base: %s: Error starting the Firewall service.", self->inet.in_ifname);
        return false;
    }

    /* Stop service */
    if (!start && !inet_fw_stop(self->in_fw))
    {
        LOG(ERR, "inet_base: %s: Error stopping the Firewall service.", self->inet.in_ifname);
        return false;
    }

    return true;
}

bool inet_base_igmp_commit(inet_base_t *self, bool start)
{
    /* Start service */
    if (start && !inet_igmp_start(self->in_igmp))
    {
        LOG(ERR, "inet_base: %s: Error starting the IGMP snooping service.", self->inet.in_ifname);
        return false;
    }
    /* Stop service */
    if (!start && !inet_igmp_stop(self->in_igmp))
    {
        LOG(ERR, "inet_base: %s: Error stopping the IGMP snooping service.", self->inet.in_ifname);
        return false;
    }
    return true;
}

bool inet_base_upnp_commit(inet_base_t *self, bool start)
{
    /* Start service */
    if (start && !osn_upnp_start(self->in_upnp))
    {
        LOG(ERR, "inet_base: %s: Error starting the UPnP service.", self->inet.in_ifname);
        return false;
    }

    /* Stop service */
    if (!start && !osn_upnp_stop(self->in_upnp))
    {
        LOG(ERR, "inet_base: %s: Error stopping the UPnP service.", self->inet.in_ifname);
         return false;
    }

    return true;
}

/**
 * Start or stop the DHCP client service
 */
bool inet_base_dhcp_client_commit(inet_base_t *self, bool start)
{
    /* Start service */
    if (start && !osn_dhcp_client_start(self->in_dhcpc))
    {
        LOG(ERR, "inet_base: %s: Error starting the DHCP client service.", self->inet.in_ifname);
        return false;
    }

    /* Stop service */
    if (!start && !osn_dhcp_client_stop(self->in_dhcpc))
    {
        LOG(ERR, "inet_base: %s: Error stopping the DHCP client service.", self->inet.in_ifname);
        return false;
    }

    return true;
}

/*
 * DHCPv4 server status change callback -- this function is called whenever a status change is detected
 * (currently if a lease was removed or added)
 */
void inet_base_dhcp_server_status(osn_dhcp_server_t *ds, struct osn_dhcp_server_status *st)
{
    int ii;

    inet_base_t *self = osn_dhcp_server_data_get(ds);

    LOG(INFO, "inet_base: dhcpv4_server: %s: Number of leases %d.",
            self->inet.in_ifname,
            st->ds_leases_len);

    synclist_begin(&self->in_dhcps_lease_list);

    for (ii = 0; ii < st->ds_leases_len; ii++)
    {
        struct dhcp_lease_node dl;

        dl.dl_lease = st->ds_leases[ii];

        synclist_add(&self->in_dhcps_lease_list, &dl);
    }
    synclist_end(&self->in_dhcps_lease_list);
}

/*
 * Callback for handling a synchronized list of DHCPv4 leases
 */
void *inet_base_dhcp_server_lease_sync(synclist_t *list, void *_old, void *_new)
{
    inet_base_t *super = CONTAINER_OF(list, inet_base_t, in_dhcps_lease_list);

    struct dhcp_lease_node *dl_old = _old;
    struct dhcp_lease_node *dl_new = _new;

    /* Insert */
    if (dl_old == NULL)
    {
        /* Allocate a new lease node and return it */
        dl_old = calloc(1, sizeof(struct dhcp_lease_node));
        dl_old->dl_lease = dl_new->dl_lease;
        super->in_dhcps_lease_fn(NULL, false, &dl_old->dl_lease);
    }
    /* Update */
    else if (dl_old != NULL && _new != NULL)
    {
        bool changed = false;

        changed |= strcmp(dl_old->dl_lease.dl_hostname, dl_new->dl_lease.dl_hostname) != 0;
        changed |= strcmp(dl_old->dl_lease.dl_fingerprint, dl_new->dl_lease.dl_fingerprint) != 0;
        changed |= strcmp(dl_old->dl_lease.dl_vendorclass, dl_new->dl_lease.dl_vendorclass) != 0;

        /* Lease was updated, issue an update notification */
        if (changed)
        {
            /* Copy data to old element */
            dl_old->dl_lease = dl_new->dl_lease;
            super->in_dhcps_lease_fn(NULL, false, &dl_old->dl_lease);
        }
    }
    /* Remove */
    else if (dl_new == NULL)
    {
        super->in_dhcps_lease_fn(NULL, true, &dl_old->dl_lease);
        free(dl_old);
        return NULL;
    }

    return dl_old;
}

/**
 * Start or stop the DHCP server service
 */
bool inet_base_dhcp_server_commit(inet_base_t *self, bool start)
{
    struct dhcp_reservation_node *rn;
    int ii;

    /*
     * Destroy any currently active objects
     */
    if (self->in_dhcps != NULL) osn_dhcp_server_del(self->in_dhcps);
    self->in_dhcps = NULL;

    if (!start) return true;

    /*
     * Create new DHCPv4 object
     */
    self->in_dhcps = osn_dhcp_server_new(self->inet.in_ifname);
    if (self->in_dhcps == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating DHCPv4 object.", self->inet.in_ifname);
        return false;
    }

    /* Set private data -- a pointer to self */
    osn_dhcp_server_data_set(self->in_dhcps, self);

    osn_dhcp_server_status_notify(self->in_dhcps, inet_base_dhcp_server_status);

    /*
     * Push configuration
     */

    struct osn_dhcp_server_cfg cfg;
    cfg.ds_lease_time = self->in_dhcps_lease_time_s;
    if (!osn_dhcp_server_cfg_set(self->in_dhcps, &cfg))
    {
        LOG(ERR, "inet_base: %s: DHCPv4 object was unable to set configuration.", self->inet.in_ifname);
        return false;
    }

    /*
     * Although osn_dhcp_server_t in theory supports multiple ranges,
     * libinet and NM2 supports only single range
     */
    if (!osn_dhcp_server_range_add(self->in_dhcps, self->in_dhcps_lease_start, self->in_dhcps_lease_stop))
    {
        LOG(ERR, "inet_base: %s: DHCPv4 object was unable to set IP pool range.", self->inet.in_ifname);
        return false;
    }

    /* Set options */
    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (!osn_dhcp_server_option_set(self->in_dhcps, ii, self->in_dhcps_opts[ii]))
        {
            LOG(WARN, "inet_base: %s: Unable to set DHCPv4 server option %d: %s",
                    self->inet.in_ifname,
                    ii,
                    self->in_dhcps_opts[ii] == NULL ? "(null)" : self->in_dhcps_opts[ii]);
        }
    }

    /* Set reservation entries */
    ds_tree_foreach(&self->in_dhcps_reservation_list, rn)
    {
        if (!osn_dhcp_server_reservation_add(self->in_dhcps, rn->rn_macaddr, rn->rn_ipaddr, rn->rn_hostname))
        {
            LOG(WARN, "inet_base: %s: Unable to set DHCPv4 reservation "PRI_osn_mac_addr":"PRI_osn_ip_addr":%s",
                    self->inet.in_ifname,
                    FMT_osn_mac_addr(rn->rn_macaddr),
                    FMT_osn_ip_addr(rn->rn_ipaddr),
                    rn->rn_hostname == NULL ? "(null)" : rn->rn_hostname);
        }
    }

    /* Apply configuration */
    if (!osn_dhcp_server_apply(self->in_dhcps))
    {
        LOG(ERR, "inet_base: %s: Error applying DHCPv4 configuration.", self->inet.in_ifname);
        return false;
    }

    return true;
}

bool inet_base_dns_commit(inet_base_t *self, bool start)
{
    /* Start service */
    if (start && !inet_dns_start(self->in_dns))
    {
        LOG(ERR, "inet_base: %s: Error starting the DNS service.", self->inet.in_ifname);
        return false;
    }

    /* Stop service */
    if (!start && !inet_dns_stop(self->in_dns))
    {
        LOG(ERR, "inet_base: %s: Error stopping the DNS service.", self->inet.in_ifname);
        return false;
    }

    return true;
}

bool inet_base_dhsnif_commit(inet_base_t *self, bool start)
{
    /* Start service */
    if (start && !inet_dhsnif_start(self->in_dhsnif))
    {
        LOG(ERR, "inet_base: %s: Error starting the DHCP sniffing service.", self->inet.in_ifname);
        return false;
    }

    /* Stop service */
    if (!start && !inet_dhsnif_stop(self->in_dhsnif))
    {
        LOG(ERR, "inet_base: %s: Error stopping the DHCP sniffing service.", self->inet.in_ifname);
        return false;
    }

    return true;
}

/*
 * ===========================================================================
 *  IPv6 Support
 * ===========================================================================
 */

bool inet_base_ip6_addr(inet_t *super, bool add, osn_ip6_addr_t *addr)
{
    struct osn_ip6_addr_node *node;

    inet_base_t *self = (void *)super;

    node = ds_tree_find(&self->in_ip6addr_list, addr);
    if (add && node == NULL)
    {
        /* New entry */
        node = calloc(1, sizeof(*node));
        node->in_addr = *addr;
        ds_tree_insert(&self->in_ip6addr_list, node, &node->in_addr);
    }
    else if (!add && node != NULL)
    {
        /* Remove entry */
        ds_tree_remove(&self->in_ip6addr_list, node);
        free(node);
        node = NULL;
    }

    if (node != NULL)
    {
        /* Lifetimes may have been updated, update the address */
        node->in_addr = *addr;
    }

    /* Flag the INET_BASE_INET6 service for restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_INET6, true))
    {
        LOG(ERR, "inet_base: %s: Error restarting service INET_BASE_INET6 (IPv6)",
                super->in_ifname);
        return false;
    }

    return true;
}

bool inet_base_ip6_dns(inet_t *super, bool add, osn_ip6_addr_t *addr)
{
    struct osn_ip6_addr_node *node;

    inet_base_t *self = (void *)super;

    node = ds_tree_find(&self->in_ip6dns_list, addr);

    if (add)
    {
        if (node != NULL)
        {
            LOG(ERR, "inet_base: %s: Trying to add duplicate DNSv6 address.",
                    super->in_ifname);
            return false;
        }

        node = calloc(1, sizeof(*node));

        node->in_addr = *addr;
        ds_tree_insert(&self->in_ip6dns_list, node, &node->in_addr);
    }
    else
    {
        if (node == NULL)
        {
            LOG(ERR, "inet_base: %s: Trying to remove non-existent DNSv6 address.",
                    super->in_ifname);
        }

        ds_tree_remove(&self->in_ip6dns_list, node);
        free(node);
    }

    /* Flag the INET_BASE_INET6 service for restart */
    if (!inet_unit_restart(self->in_units, INET_BASE_INET6, true))
    {
        LOG(ERR, "inet_base: %s: Error restarting service INET_BASE_INET6 (DNSv6)",
                super->in_ifname);
        return false;
    }

    return true;
}

/*
 * ===========================================================================
 *  IPv6 status reporting
 * ===========================================================================
 */

/**
 * Callback that will be receiving IPv6 status change updates by registering it
 * with the osn_ip6_status_notify() function.
 */
void inet_base_osn_ip6_status_fn(
        osn_ip6_t *ip6,
        struct osn_ip6_status *status)
{
    size_t ii;

    (void)ip6;

    inet_base_t *self = status->is6_data;

    LOG(DEBUG, "inet_base: %s: IPv6 status notification: ipv6_addresses=%zu, ipv6_neighbors:%zu",
            self->inet.in_ifname,
            status->is6_addr_len,
            status->is6_neigh_len);

    /*
     * Update IPv6 addresses status list
     */
    synclist_begin(&self->in_ip6_addr_status_list);
    for (ii = 0; ii < status->is6_addr_len; ii ++)
    {
        struct ip6_addr_status_node as;
        struct osn_ip6_addr_node *on;

        /* If an address matches a configured address, skip it -- we do not want to overwrite the list */
        on = ds_tree_find(&self->in_ip6addr_list, &status->is6_addr[ii].ia6_addr);
        if (on != NULL)
        {
            continue;
        }

        as.as_addr.is_addr = status->is6_addr[ii];
        as.as_addr.is_origin = INET_IP6_ORIGIN_AUTO_CONFIGURED;
        /*
         * TODO: If we need to add DHCP addresses, the DHCP module must be
         * changed to actually report back the acquired DHCP address.
         *
         * For the time being, just use AUTO_CONFIGURED which should cover
         * most cases.
         */

        synclist_add(&self->in_ip6_addr_status_list, &as);
    }
    synclist_end(&self->in_ip6_addr_status_list);

    /*
     * Update the IPv6 neighbors status list
     */
    synclist_begin(&self->in_ip6_neigh_status_list);
    for (ii = 0; ii < status->is6_neigh_len; ii++)
    {
        struct ip6_neigh_status_node ns;

        ns.ns_neigh.in_hwaddr = status->is6_neigh[ii].i6n_hwaddr;
        ns.ns_neigh.in_ipaddr = status->is6_neigh[ii].i6n_ipaddr;

        synclist_add(&self->in_ip6_neigh_status_list, &ns);
    }
    synclist_end(&self->in_ip6_neigh_status_list);

}

/*
 * ===========================================================================
 *  IPv6 Address reporting
 * ===========================================================================
 */

/*
 * IPv6 address status notification
 */
bool inet_base_ip6_addr_status_notify(
        inet_t *super,
        inet_ip6_addr_status_fn_t *fn)
{
    inet_base_t *self = (inet_base_t *)super;
    struct ip6_addr_status_node *as;

    self->in_ip6_addr_status_fn = fn;

    /* Dispatch all cached entries so far */
    synclist_foreach(&self->in_ip6_addr_status_list, as)
    {
        fn(super, &as->as_addr, false);
    }

    return true;
}

/**
 * Index IPv6 address status structures by the IPv6 address and
 * by the origin
 */
int ip6_addr_status_node_cmp(void *_a, void *_b)
{
    int rc;

    struct ip6_addr_status_node *a = _a;
    struct ip6_addr_status_node *b = _b;

    /* Lifetimes are constantly changing, do not use them as the key */
    rc = osn_ip6_addr_nolft_cmp(&a->as_addr.is_addr, &b->as_addr.is_addr);
    if (rc != 0) return rc;

    return (int)a->as_addr.is_origin - (int)b->as_addr.is_origin;
}

/**
 * IPv6 neighbor report list synchronization function.
 */
void *ip6_addr_status_node_sync(synclist_t *sync, void *_sold, void *_snew)
{
    struct ip6_addr_status_node *aold = _sold;
    struct ip6_addr_status_node *anew = _snew;

    inet_base_t *self = CONTAINER_OF(sync, inet_base_t, in_ip6_addr_status_list);

    if (aold == NULL) /* Insert */
    {
        aold = calloc(1, sizeof(struct ip6_neigh_status_node));
        aold->as_addr = anew->as_addr;

        if (self->in_ip6_addr_status_fn != NULL)
        {
            self->in_ip6_addr_status_fn(&self->inet, &aold->as_addr, false);
        }

        return aold;
    }
    else if (anew == NULL) /* Delete */
    {
        if (self->in_ip6_addr_status_fn != NULL)
        {
            self->in_ip6_addr_status_fn(&self->inet, &aold->as_addr, true);
        }

        free(aold);
        return NULL;
    }

    return aold;
}

/*
 * ===========================================================================
 *  IPv6 Neighbor Report
 * ===========================================================================
 */

/*
 * IPv6 neighbors status notification
 */
bool inet_base_ip6_neigh_status_notify(
        inet_t *super,
        inet_ip6_neigh_status_fn_t *fn)
{
    inet_base_t *self = (inet_base_t *)super;
    struct ip6_neigh_status_node *ns;

    self->in_ip6_neigh_status_fn = fn;

    /* Dispatch all cached entries so far */
    synclist_foreach(&self->in_ip6_neigh_status_list, ns)
    {
        fn(super, &ns->ns_neigh, false);
    }

    return true;
}

int ip6_neigh_status_node_cmp(void *_a, void *_b)
{
    int rc;

    struct ip6_neigh_status_node *a = _a;
    struct ip6_neigh_status_node *b = _b;

    rc = osn_ip6_addr_cmp(&a->ns_neigh.in_ipaddr, &b->ns_neigh.in_ipaddr);
    if (rc != 0) return rc;

    rc = osn_mac_addr_cmp(&a->ns_neigh.in_hwaddr, &b->ns_neigh.in_hwaddr);
    if (rc != 0) return rc;

    return 0;
}

/**
 * IPv6 neighbor report list synchronization function.
 */
void *ip6_neigh_status_node_sync(synclist_t *sync, void *_sold, void *_snew)
{
    struct ip6_neigh_status_node *sold = _sold;
    struct ip6_neigh_status_node *snew = _snew;

    inet_base_t *self = CONTAINER_OF(sync, inet_base_t, in_ip6_neigh_status_list);

    if (sold == NULL) /* Insert */
    {
        sold = calloc(1, sizeof(struct ip6_neigh_status_node));
        sold->ns_neigh = snew->ns_neigh;

        if (self->in_ip6_neigh_status_fn != NULL)
        {
            self->in_ip6_neigh_status_fn(&self->inet, &sold->ns_neigh, false);
        }

        return sold;
    }
    else if (snew == NULL) /* Delete */
    {
        if (self->in_ip6_neigh_status_fn != NULL)
        {
            self->in_ip6_neigh_status_fn(&self->inet, &sold->ns_neigh, true);
        }

        free(sold);
        return NULL;
    }

    return sold;
}

/**
 * Initialize the IPv6 stack
 */
bool inet_base_init_ip6(inet_base_t *self)
{
    if (self->in_ip6 != NULL && !osn_ip6_del(self->in_ip6))
    {
        LOG(ERR, "inet_base: %s: Error destroying ip6 object.",
                self->inet.in_ifname);
    }

    /* Create new object */
    self->in_ip6 = osn_ip6_new(self->inet.in_ifname);
    if (self->in_ip6 == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating ip6 object.",
                self->inet.in_ifname);
        return false;
    }

    /* Register to status updates */
    osn_ip6_status_notify(self->in_ip6, inet_base_osn_ip6_status_fn, self);

    return true;
}

bool inet_base_inet6_commit(inet_base_t *self, bool start)
{
    struct osn_ip6_addr_node *node;

    /* Reinitialize the IPv6 stack */
    if (!inet_base_init_ip6(self))
    {
        LOG(ERR, "inet_base: %s: Error reinitializing IPv6 object.", self->inet.in_ifname);
        return false;
    }

    /* If we're stopping the service, return right now */
    if (!start) return true;

    /*
     * Apply cached configuration only if the service is started
     */
    ds_tree_foreach(&self->in_ip6addr_list, node)
    {
        (void)node;
        osn_ip6_addr_add(self->in_ip6, &node->in_addr);
    }

    ds_tree_foreach(&self->in_ip6dns_list, node)
    {
        osn_ip6_dns_add(self->in_ip6, &node->in_addr);
    }

    if (!osn_ip6_apply(self->in_ip6))
    {
        LOG(ERR, "inet_base: %s: Error applying IPv6 static configuration.",
                self->inet.in_ifname);
        return false;
    }

    return true;
}

/* DHCPv6 configuration options */
bool inet_base_dhcp6_client(inet_t *super, bool enable, bool request_addr, bool request_prefix, bool rapid_commit)
{
    inet_base_t *self = (void *)super;

    bool changed = false;

    VAL_CMP(changed, self->in_dhcp6_client_enable, enable);
    VAL_CMP(changed, self->in_dhcp6_client_request_addr, request_addr);
    VAL_CMP(changed, self->in_dhcp6_client_request_prefix, request_prefix);
    VAL_CMP(changed, self->in_dhcp6_client_rapid_commit, rapid_commit);

    if (!changed) return true;

    inet_unit_stop(self->in_units, INET_BASE_DHCP6_CLIENT);
    inet_unit_enable(self->in_units, INET_BASE_DHCP6_CLIENT, enable);

    return true;
}

/* DHCPv6 options that shall be requested from remote */
bool inet_base_dhcp6_client_option_request(inet_t *super, int tag, bool request)
{
    inet_base_t *self = (void *)super;

    if (self->in_dhcp6_client_request[tag] == request)
    {
        return true;
    }

    self->in_dhcp6_client_request[tag] = request;

    inet_unit_restart(self->in_units, INET_BASE_DHCP6_CLIENT, false);

    return true;
}

/* DHCPv6 options that will be sent to the server */
bool inet_base_dhcp6_client_option_send(inet_t *super, int tag, char *value)
{
    inet_base_t *self = (void *)super;

    if (self->in_dhcp6_client_send[tag] == NULL && value == NULL)
    {
        return true;
    }
    else if (self->in_dhcp6_client_send[tag] != NULL && value != NULL)
    {
        if (strcmp(self->in_dhcp6_client_send[tag], value) == 0) return true;
    }

    free(self->in_dhcp6_client_send[tag]);
    self->in_dhcp6_client_send[tag] = (value != NULL) ? strdup(value) : NULL;

    inet_unit_restart(self->in_units, INET_BASE_DHCP6_CLIENT, false);

    return true;
}

bool inet_base_dhcp6_client_notify(inet_t *super, inet_dhcp6_client_notify_fn_t *fn, void *ctx)
{
    inet_base_t *self = (void *)super;

    self->in_dhcp6_client_notify_fn = fn;
    self->in_dhcp6_client_notifyt_data = ctx;

    return true;
}

void inet_base_dhcp6_client_notify_fn(osn_dhcpv6_client_t *d6c, struct osn_dhcpv6_client_status *status)
{
    (void)d6c;
    inet_base_t *self = status->d6c_data;

    if (self->in_dhcp6_client_notify_fn == NULL) return;

    self->in_dhcp6_client_notify_fn(self->in_dhcp6_client_notifyt_data, status);
}

bool inet_base_dhcp6_client_commit(inet_base_t *self, bool start)
{
    bool rc;
    int ii;

    if (self->in_dhcp6_client != NULL)
    {
        rc = osn_dhcpv6_client_del(self->in_dhcp6_client);
        self->in_dhcp6_client = NULL;
        if (!rc)
        {
            LOG(ERR, "inet_base: %s: Error destroying dhcp6_client object.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* If we're stopping the service, return right now */
    if (!start) return true;

    /* Create new object */
    self->in_dhcp6_client = osn_dhcpv6_client_new(self->inet.in_ifname);
    if (self->in_dhcp6_client == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating dhcpv6_client object.",
                self->inet.in_ifname);
        return false;
    }

    /* Register status update handler */
    osn_dhcpv6_client_status_notify(
            self->in_dhcp6_client,
            inet_base_dhcp6_client_notify_fn,
            self);

    /*
     * Apply cached configuration
     */
    osn_dhcpv6_client_set(
            self->in_dhcp6_client,
            self->in_dhcp6_client_request_addr,
            self->in_dhcp6_client_request_prefix,
            self->in_dhcp6_client_rapid_commit,
            false);

    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (!self->in_dhcp6_client_request[ii]) continue;
        osn_dhcpv6_client_option_request(self->in_dhcp6_client, ii);
    }

    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (self->in_dhcp6_client_send[ii] == NULL) continue;
        osn_dhcpv6_client_option_send(self->in_dhcp6_client, ii, self->in_dhcp6_client_send[ii]);
    }

    /* Apply configuration */
    if (!osn_dhcpv6_client_apply(self->in_dhcp6_client))
    {
        LOG(ERR, "inet_base: %s: Error applying DHCPv6 Client configuration.",
                self->inet.in_ifname);
        return false;
    }

    return true;
}

/* Router Advertisement: Push new options for this interface */
bool inet_base_radv(inet_t *super, bool enable, struct osn_ip6_radv_options *opts)
{
    inet_base_t *self = (void *)super;

    bool changed = false;

    VAL_CMP(changed, self->in_radv_enable, enable);
    VAL_CMP(changed, self->in_radv_options.ra_managed, opts->ra_managed);
    VAL_CMP(changed, self->in_radv_options.ra_other_config, opts->ra_other_config);
    VAL_CMP(changed, self->in_radv_options.ra_home_agent, opts->ra_home_agent);
    VAL_CMP(changed, self->in_radv_options.ra_max_adv_interval, opts->ra_max_adv_interval);
    VAL_CMP(changed, self->in_radv_options.ra_min_adv_interval, opts->ra_min_adv_interval);
    VAL_CMP(changed, self->in_radv_options.ra_default_lft, opts->ra_default_lft);
    VAL_CMP(changed, self->in_radv_options.ra_preferred_router, opts->ra_preferred_router);
    VAL_CMP(changed, self->in_radv_options.ra_mtu, opts->ra_mtu);
    VAL_CMP(changed, self->in_radv_options.ra_reachable_time, opts->ra_reachable_time);
    VAL_CMP(changed, self->in_radv_options.ra_retrans_timer, opts->ra_retrans_timer);
    VAL_CMP(changed, self->in_radv_options.ra_current_hop_limit, opts->ra_current_hop_limit);

    if (!changed) return true;

    inet_unit_stop(self->in_units, INET_BASE_RADV);
    inet_unit_enable(self->in_units, INET_BASE_RADV, enable);

    return true;
}

/* Router Advertisement: Add or remove an advertised prefix */
bool inet_base_radv_prefix(
        inet_t *super,
        bool add,
        osn_ip6_addr_t *prefix,
        bool autonomous,
        bool onlink)
{
    inet_base_t *self = (void *)super;

    struct ip6_prefix_node *node;
    bool changed;

    node = ds_tree_find(&self->in_radv_prefix_list, prefix);

    /* First handle the simplest case -- remove */
    if (!add)
    {
        if (node == NULL) return true;

        ds_tree_remove(&self->in_radv_prefix_list, node);
        free(node);

        inet_unit_restart(self->in_units, INET_BASE_RADV, false);
        return true;
    }

    /*
     * Add case
     */
    if (node == NULL)
    {
        node = calloc(1, sizeof(*node));
        node->pr_addr = *prefix;
        node->pr_autonomous = autonomous;
        node->pr_onlink = onlink;

        ds_tree_insert(&self->in_radv_prefix_list, node, &node->pr_addr);
    }
    else
    {
        changed = false;
        VAL_CMP(changed, node->pr_autonomous, autonomous);
        VAL_CMP(changed, node->pr_onlink, onlink);
        if (!changed) return true;
    }

    inet_unit_restart(self->in_units, INET_BASE_RADV, false);

    return true;
}

bool inet_base_radv_rdnss(inet_t *super, bool add, osn_ip6_addr_t *addr)
{
    inet_base_t *self = (void *)super;

    struct osn_ip6_addr_node *node;

    node = ds_tree_find(&self->in_radv_rdnss_list, addr);

    if (add)
    {
        if (node != NULL) return true;

        node = calloc(1, sizeof(*node));
        node->in_addr = *addr;
        ds_tree_insert(&self->in_radv_rdnss_list, node, &node->in_addr);
    }
    else
    {
        if (node == NULL) return true;

        ds_tree_remove(&self->in_radv_rdnss_list, node);
        free(node);
    }

    inet_unit_restart(self->in_units, INET_BASE_RADV, false);

    return true;
}

bool inet_base_radv_dnssl(inet_t *super, bool add, char *sl)
{
    inet_base_t *self = (void *)super;

    struct dnssl_node *node;

    node = ds_tree_find(&self->in_radv_dnssl_list, sl);

    if (add)
    {
        if (node != NULL) return true;

        node = calloc(1, sizeof(*node));
        if (strscpy(node->dn_dnssl, sl, sizeof(node->dn_dnssl)) < 0)
        {
            LOG(ERR, "inet_base: %s: Error adding DNSSL entry %s. String too long.",
                    super->in_ifname,
                    sl);
            free(node);
            return false;
        }

        ds_tree_insert(&self->in_radv_dnssl_list, node, &node->dn_dnssl);
    }
    else
    {
        if (node == NULL) return true;

        ds_tree_remove(&self->in_radv_dnssl_list, node);
        free(node);
    }

    inet_unit_restart(self->in_units, INET_BASE_RADV, false);

    return true;
}

/* Apply RADV configuration to running system */
bool inet_base_radv_commit(inet_base_t *self, bool start)
{
    struct ip6_prefix_node *prefix;
    struct osn_ip6_addr_node *rdnss;
    struct dnssl_node *dnssl;
    bool rc;

    if (self->in_radv != NULL)
    {
        rc = osn_ip6_radv_del(self->in_radv);
        self->in_radv = NULL;
        if (!rc)
        {
            LOG(ERR, "inet_base: %s: Error destroying ip6_radv object.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* If we're stopping the service, return right now */
    if (!start) return true;

    /*
     * Create new object, apply cached configuration
     */
    self->in_radv = osn_ip6_radv_new(self->inet.in_ifname);
    if (self->in_radv == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating ip6_radv object.",
                self->inet.in_ifname);
        return false;
    }

    if (!osn_ip6_radv_set(self->in_radv, &self->in_radv_options))
    {
        LOG(ERR, "inet_base: %s: Unable to set RADV configuration.",
                self->inet.in_ifname);
        return false;
    }

    /* Set PREFIX configuration */
    ds_tree_foreach(&self->in_radv_prefix_list, prefix)
    {
        if (!osn_ip6_radv_add_prefix(
                    self->in_radv,
                    &prefix->pr_addr,
                    prefix->pr_autonomous,
                    prefix->pr_onlink))
        {
            LOG(ERR, "inet_base: %s: Unable to add RADV prefix.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* Set RDNSS configuration */
    ds_tree_foreach(&self->in_radv_rdnss_list, rdnss)
    {
        if (!osn_ip6_radv_add_rdnss(self->in_radv, &rdnss->in_addr))
        {
            LOG(ERR, "inet_base: %s: Unable to add RADV RDNSS.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* Set DNSSL configuration */
    ds_tree_foreach(&self->in_radv_dnssl_list, dnssl)
    {
        if (!osn_ip6_radv_add_dnssl(self->in_radv, dnssl->dn_dnssl))
        {
            LOG(ERR, "inet_base: %s: Unable to add RADV RDNSS.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* Apply configuration */
    if (!osn_ip6_radv_apply(self->in_radv))
    {
        LOG(ERR, "inet_base: %s: Error applying Router Advertisement configuration.",
                self->inet.in_ifname);
        return false;
    }

    return true;
}

/* DHCPv6 Server: Options that will be sent to the server */
bool inet_base_dhcp6_server(inet_t *super, bool enable)
{
    inet_base_t *self = (void *)super;

    bool changed = false;

    VAL_CMP(changed, self->in_dhcp6_server_enable, enable);

    if (!changed) return true;

    inet_unit_stop(self->in_units, INET_BASE_DHCP6_SERVER);
    inet_unit_enable(self->in_units, INET_BASE_DHCP6_SERVER, enable);

    return true;
}

/* DHCPv6 Server: Add an IPv6 range to the server */
bool inet_base_dhcp6_server_prefix(inet_t *super, bool add, struct osn_dhcpv6_server_prefix *prefix)
{
    inet_base_t *self = (void *)super;

    struct osn_dhcpv6_server_prefix_node *node;

    node = ds_tree_find(&self->in_dhcp6_server_prefix_list, &prefix->d6s_prefix);

    if (add)
    {
        if (node != NULL) return true;

        node = calloc(1, sizeof(*node));
        memcpy(&node->sp_prefix, prefix, sizeof(node->sp_prefix));
        ds_tree_insert(&self->in_dhcp6_server_prefix_list, node, &node->sp_prefix);
    }
    else
    {
        if (node == NULL) return true;

        ds_tree_remove(&self->in_dhcp6_server_prefix_list, node);
        free(node);
    }

    inet_unit_restart(self->in_units, INET_BASE_DHCP6_SERVER, false);

    return true;
}

/* DHCPv6 Server: Options that will be sent to the server */
bool inet_base_dhcp6_server_option_send(inet_t *super, int tag, char *data)
{
    inet_base_t *self = (void *)super;

    struct osn_dhcpv6_server_optsend_node *node;

    node = ds_tree_find(&self->in_dhcp6_server_optsend_list, &tag);

    if (data == NULL)
    {
        /* Removing a non-existing item is a noop */
        if (node == NULL) return true;

        ds_tree_remove(&self->in_dhcp6_server_optsend_list, node);
        free(node->so_data);
        free(node);
    }
    else
    {
        /* Add tag to the list of options */
        if (node == NULL)
        {
            node = calloc(1, sizeof(struct osn_dhcpv6_server_optsend_node));
            node->so_tag = tag;
            ds_tree_insert(&self->in_dhcp6_server_optsend_list, node, &node->so_tag);
        }

        /* Compare new and old data -- if the data is the same just return */
        if (node->so_data != NULL && strcmp(node->so_data, data) == 0)
        {
            return true;
        }

        /* Replace old with new data */
        free(node->so_data);
        node->so_data = strdup(data);
    }

    /* Restart the DHCPv6 server */
    inet_unit_restart(self->in_units, INET_BASE_DHCP6_SERVER, false);

    return true;
}

/* DHCPv6 Server: Add a static lease  */
bool inet_base_dhcp6_server_lease(inet_t *super, bool add, struct osn_dhcpv6_server_lease *lease)
{
    inet_base_t *self = (void *)super;

    struct osn_dhcpv6_server_lease_node *node;

    node = ds_tree_find(&self->in_dhcp6_server_lease_list, lease);

    if (add)
    {
        if (node != NULL) return true;

        node = calloc(1, sizeof(*node));
        memcpy(&node->sl_lease, lease, sizeof(node->sl_lease));
        ds_tree_insert(&self->in_dhcp6_server_lease_list, node, &node->sl_lease);
    }
    else
    {
        if (node == NULL) return true;

        ds_tree_remove(&self->in_dhcp6_server_lease_list, node);
        free(node);
    }

    inet_unit_restart(self->in_units, INET_BASE_DHCP6_SERVER, false);

    return true;
}

bool inet_base_dhcp6_server_notify(inet_t *super, inet_dhcp6_server_notify_fn_t *fn, void *ctx)
{
    inet_base_t *self = (void *)super;

    self->in_dhcp6_server_notify_fn = fn;
    self->in_dhcp6_server_notify_data = ctx;

    return true;
}

void inet_base_dhcp6_server_notify_fn(
        osn_dhcpv6_server_t *d6s,
        struct osn_dhcpv6_server_status *status)
{
    inet_base_t *self = osn_dhcpv6_server_data_get(d6s);

    if (self->in_dhcp6_server_notify_fn == NULL) return;

    self->in_dhcp6_server_notify_fn(self->in_dhcp6_server_notify_data, status);
}


bool inet_base_dhcp6_server_commit(inet_base_t *self, bool start)
{
    struct osn_dhcpv6_server_prefix_node *prefix;
    struct osn_dhcpv6_server_lease_node *lease;
    struct osn_dhcpv6_server_optsend_node *so;
    bool rc;

    if (self->in_dhcp6_server != NULL)
    {
        rc = osn_dhcpv6_server_del(self->in_dhcp6_server);
        self->in_dhcp6_server = NULL;
        if (!rc)
        {
            LOG(ERR, "inet_base: %s: Error destroying dhcp6_server object.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* If we're stopping the service, return right now */
    if (!start) return true;

    /*
     * Create new object, apply cached configuration
     */
    self->in_dhcp6_server = osn_dhcpv6_server_new(self->inet.in_ifname);
    if (self->in_dhcp6_server == NULL)
    {
        LOG(ERR, "inet_base: %s: Error creating dhcp6_server object.",
                self->inet.in_ifname);
        return false;
    }

    /* Set private data -- a pointer to self */
    osn_dhcpv6_server_data_set(self->in_dhcp6_server, self);

    /* Install status change notification handlers */
    osn_dhcpv6_server_status_notify(self->in_dhcp6_server, inet_base_dhcp6_server_notify_fn);

    /* Set PREFIX configuration */
    ds_tree_foreach(&self->in_dhcp6_server_prefix_list, prefix)
    {
        if (!osn_dhcpv6_server_prefix_add(self->in_dhcp6_server, &prefix->sp_prefix))
        {
            LOG(ERR, "inet_base: %s: Unable to add DHCPv6 Server prefix.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* Set DHCPv6 options */
    ds_tree_foreach(&self->in_dhcp6_server_optsend_list, so)
    {
        if (!osn_dhcpv6_server_option_send(self->in_dhcp6_server, so->so_tag, so->so_data))
        {
            LOG(ERR, "inet_base: %s: Unable to add DHCPv6 option, tag %d, data %s.",
                    self->inet.in_ifname,
                    so->so_tag,
                    so->so_data);
            return false;
        }
    }

    /* Set static lease configuration */
    ds_tree_foreach(&self->in_dhcp6_server_lease_list, lease)
    {
        if (!osn_dhcpv6_server_lease_add(self->in_dhcp6_server, &lease->sl_lease))
        {
            LOG(ERR, "inet_base: %s: Unable to add DHCPv6 Server static lease.",
                    self->inet.in_ifname);
            return false;
        }
    }

    /* Apply configuration */
    if (!osn_dhcpv6_server_apply(self->in_dhcp6_server))
    {
        LOG(ERR, "inet_base: %s: Error applying DHCPv6 Server configuration.",
                self->inet.in_ifname);
        return false;
    }

    return true;
}

/*
 * ===========================================================================
 *  Miscellaneous
 * ===========================================================================
 */
const char *inet_base_service_str(enum inet_base_services srv)
{
    #define _V(x) case x: return #x;

    switch (srv)
    {
        INET_BASE_SERVICE_LIST(_V)

        default:
            return "Unknown";
    }
}

int osn_dhcpv6_server_lease_cmp(void *_a, void *_b)
{
    struct osn_dhcpv6_server_lease *a = _a;
    struct osn_dhcpv6_server_lease *b = _b;
    int rc;

    rc = osn_ip6_addr_cmp(&a->d6s_addr, &b->d6s_addr);
    if (rc != 0) return rc;

    rc = strcmp(a->d6s_duid, b->d6s_duid);
    if (rc != 0) return rc;

    rc = strcmp(a->d6s_hostname, b->d6s_hostname);
    if (rc != 0) return rc;

    rc = memcmp(&a->d6s_hwaddr, &b->d6s_hwaddr, sizeof(a->d6s_hwaddr));
    if (rc != 0) return rc;

    return 0;
}

