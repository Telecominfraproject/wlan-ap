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

#ifndef INET_BASE_H_INCLUDED
#define INET_BASE_H_INCLUDED

#include "ds_tree.h"

#include "inet.h"
#include "inet_unit.h"
#include "osn_dhcp.h"
#include "inet_fw.h"
#include "inet_igmp.h"
#include "osn_upnp.h"
#include "inet_dns.h"
#include "inet_dhsnif.h"
#include "osn_inet.h"
#include "synclist.h"

#define INET_BASE_SERVICE_LIST(M)   \
    M(INET_BASE_INTERFACE)          \
    M(INET_BASE_FIREWALL)           \
    M(INET_BASE_UPNP)               \
    M(INET_BASE_IGMP)               \
    M(INET_BASE_NETWORK)            \
    M(INET_BASE_MTU)                \
    M(INET_BASE_SCHEME_NONE)        \
    M(INET_BASE_SCHEME_STATIC)      \
    M(INET_BASE_SCHEME_DHCP)        \
    M(INET_BASE_DHCP_SERVER)        \
    M(INET_BASE_DNS)                \
    M(INET_BASE_DHCPSNIFF)          \
    M(INET_BASE_INET6)              \
    M(INET_BASE_DHCP6_CLIENT)       \
    M(INET_BASE_DHCP6_SERVER)       \
    M(INET_BASE_RADV)               \
    M(INET_BASE_MAX)

/*
 * This enum is used mainly to apply partial configurations through the
 * inet_unit subsystem
 */
enum inet_base_services
{
    #define _E(x) x,
    INET_BASE_SERVICE_LIST(_E)
};

typedef struct __inet_base inet_base_t;

/*
 * Base class definition.
 */
struct __inet_base
{
    union
    {
        inet_t                  inet;
    };

    /* Start/stop services */
    bool                  (*in_service_commit_fn)(
                                inet_base_t *self,
                                enum inet_base_services srv,
                                bool start);

    /* Service units dependency tree */
    inet_unit_t            *in_units;

    /* DHCP server class */
    osn_dhcp_server_t      *in_dhcps;
    /* Configured DHCPv4 options */
    char                   *in_dhcps_opts[DHCP_OPTION_MAX];

    /* DHCP client class */
    osn_dhcp_client_t      *in_dhcpc;

    /* Firewall class */
    inet_fw_t              *in_fw;

    /* IGMP snooping class */
    inet_igmp_t            *in_igmp;

    /* UPnP class */
    osn_upnp_t             *in_upnp;

    /* DNS server settings */
    inet_dns_t             *in_dns;

    /* DHCP sniffing class */
    inet_dhsnif_t          *in_dhsnif;

    /* Routing table */
    osn_route_t            *in_route;

    /* Osync IPv6 API */
    osn_ip6_t              *in_ip6;
    osn_ip6_radv_t         *in_radv;
    osn_dhcpv6_client_t    *in_dhcp6_client;
    osn_dhcpv6_server_t    *in_dhcp6_server;

    int                     in_mtu;

    /* DHCP sniffing callback */

    /* The following variables below are mainly used as cache */
    enum inet_assign_scheme in_assign_scheme;

    osn_ip_addr_t           in_static_addr;
    osn_ip_addr_t           in_static_netmask;
    osn_ip_addr_t           in_static_bcast;
    osn_ip_addr_t           in_static_gwaddr;

    bool                    in_interface_enabled;
    bool                    in_network_enabled;

    bool                    in_nat_enabled;
    bool                    in_igmp_enabled;
    int                     in_igmp_age;
    int                     in_igmp_tsize;

    enum osn_upnp_mode      in_upnp_mode;

    bool                    in_dhcps_enabled;
    int                     in_dhcps_lease_time_s;
    osn_ip_addr_t           in_dhcps_lease_start;
    osn_ip_addr_t           in_dhcps_lease_stop;
    ds_tree_t               in_dhcps_reservation_list;
    inet_dhcp_lease_fn_t   *in_dhcps_lease_fn;
    synclist_t              in_dhcps_lease_list;

    osn_ip_addr_t           in_dns_primary;
    osn_ip_addr_t           in_dns_secondary;

    inet_dhcp_lease_fn_t   *in_dhsnif_lease_fn;

    /* Tree of configured IPv6 addresses */
    ds_tree_t               in_ip6addr_list;
    ds_tree_t               in_ip6dns_list;

    inet_ip6_addr_status_fn_t
                           *in_ip6_addr_status_fn;
    inet_ip6_neigh_status_fn_t
                           *in_ip6_neigh_status_fn;

    synclist_t              in_ip6_addr_status_list;
    synclist_t              in_ip6_neigh_status_list;

    bool                    in_dhcp6_client_enable;
    bool                    in_dhcp6_client_request_addr;
    bool                    in_dhcp6_client_request_prefix;
    bool                    in_dhcp6_client_rapid_commit;

    bool                    in_dhcp6_client_request[DHCP_OPTION_MAX];
    char                   *in_dhcp6_client_send[DHCP_OPTION_MAX];

    inet_dhcp6_client_notify_fn_t
                           *in_dhcp6_client_notify_fn;
    void                   *in_dhcp6_client_notifyt_data;

    bool                    in_radv_enable;
    struct osn_ip6_radv_options
                            in_radv_options;
    ds_tree_t               in_radv_prefix_list;
    ds_tree_t               in_radv_rdnss_list;
    ds_tree_t               in_radv_dnssl_list;

    bool                    in_dhcp6_server_enable;
    ds_tree_t               in_dhcp6_server_prefix_list;    /* DHCPv6 server prefix list */
    ds_tree_t               in_dhcp6_server_optsend_list;   /* DHCPv6 server option list */
    ds_tree_t               in_dhcp6_server_lease_list;     /* DHCPv6 server lease list */
    inet_dhcp6_server_notify_fn_t
                           *in_dhcp6_server_notify_fn;
    void                   *in_dhcp6_server_notify_data;
};


/**
 * Extend the superclass with the inet_service_commit method
 */
static inline bool inet_service_commit(
        inet_base_t *self,
        enum inet_base_services srv,
        bool start)
{
    if (self->in_service_commit_fn == NULL) return true;

    return self->in_service_commit_fn(self, srv, start);
}

/*
 * ===========================================================================
 *  Constructors and destructor implementations
 * ===========================================================================
 */
extern bool inet_base_init(inet_base_t *self, const char *ifname);
extern bool inet_base_fini(inet_base_t *self);
extern bool inet_base_dtor(inet_t *super);
extern inet_base_t *inet_base_new(const char *ifname);

/* Interface enable method implementation */
extern bool inet_base_interface_enable(inet_t *super, bool enable);

/*
 * ===========================================================================
 *  Network/Assignment scheme method implementation
 * ===========================================================================
 */
extern bool inet_base_network_enable(inet_t *super, bool enable);
extern bool inet_base_mtu_set(inet_t *super, int mtu);

extern bool inet_base_assign_scheme_set(inet_t *super, enum inet_assign_scheme scheme);
extern bool inet_base_ipaddr_static_set(
                        inet_t *self,
                        osn_ip_addr_t addr,
                        osn_ip_addr_t netmask,
                        osn_ip_addr_t bcast);

extern bool inet_base_gateway_set(inet_t *super, osn_ip_addr_t gwaddr);

/*
 * ===========================================================================
 *  Firewall method implementations
 * ===========================================================================
 */
extern bool inet_base_nat_enable(inet_t *super, bool enable);
extern bool inet_base_igmp_enable(inet_t *super, bool enable, int iage, int itsize);
extern bool inet_base_upnp_mode_set(inet_t *super, enum osn_upnp_mode mode);


/*
 * ===========================================================================
 *  Port forwarding
 * ===========================================================================
 */
extern bool inet_base_portforward_set(inet_t *super, const struct inet_portforward *port_forward);
extern bool inet_base_portforward_del(inet_t *super, const struct inet_portforward *port_forward);

/*
 * ===========================================================================
 *  DHCP Client method implementation
 * ===========================================================================
 */
extern bool inet_base_dhcpc_option_request(inet_t *super, enum osn_dhcp_option opt, bool req);
extern bool inet_base_dhcpc_option_set(inet_t *super, enum osn_dhcp_option opt, const char *value);
extern bool inet_base_dhcpc_option_notify(inet_t *super, osn_dhcp_client_opt_notify_fn_t *fn, void *ctx);

/*
 * ===========================================================================
 *  DHCP Server method implementation
 * ===========================================================================
 */
extern bool inet_base_dhcps_enable(inet_t *super, bool enable);
extern bool inet_base_dhcps_lease_set(inet_t *super, int lease_time_s);
extern bool inet_base_dhcps_range_set(inet_t *super, osn_ip_addr_t start, osn_ip_addr_t stop);
extern bool inet_base_dhcps_option_set(inet_t *super, enum osn_dhcp_option opt, const char *value);
extern bool inet_base_dhcps_lease_notify(inet_t *super, inet_dhcp_lease_fn_t *fn);
extern bool inet_base_dhcps_rip_set(inet_t *super, osn_mac_addr_t macaddr,
                                    osn_ip_addr_t ip4addr, const char *hostname);
extern bool inet_base_dhcps_rip_del(inet_t *super, osn_mac_addr_t macaddr);

/*
 * ===========================================================================
 *  DNS settings
 * ===========================================================================
 */
extern bool inet_base_dns_set(inet_t *super, osn_ip_addr_t primary, osn_ip_addr_t secondary);

/*
 * ===========================================================================
 *  DHCP Sniffing functions
 * ===========================================================================
 */
extern bool inet_base_dhsnif_lease_notify(inet_t *super, inet_dhcp_lease_fn_t *func);

/*
 * ===========================================================================
 *  Route functions
 * ===========================================================================
 */
extern bool inet_base_route_notify(inet_t *super, osn_route_status_fn_t *func);

/*
 * ===========================================================================
 *  Commit & Service start/stop method implementation
 * ===========================================================================
 */
extern bool inet_base_service_commit(
        inet_base_t *self,
        enum inet_base_services srv,
        bool start);

extern bool inet_base_commit(inet_t *super);

/*
 * ===========================================================================
 *  Interface status reporting
 * ===========================================================================
 */
bool inet_base_state_get(inet_t *super, inet_state_t *out);

/*
 * ===========================================================================
 *  IPv6 support
 * ===========================================================================
 */

/* Static IPv6 configuration: Add/remove IP addresses */
bool inet_base_ip6_addr(inet_t *self, bool add, osn_ip6_addr_t *addr);

/* Static IPv6 configuration: Add/remove DNS servers */
bool inet_base_ip6_dns(inet_t *self, bool add, osn_ip6_addr_t *addr);

/* IPv6 address status change notification */
bool inet_base_ip6_addr_status_notify(inet_t *self, inet_ip6_addr_status_fn_t *fn);
/* IPv6 neighbor status change notification */
bool inet_base_ip6_neigh_status_notify(inet_t *self, inet_ip6_neigh_status_fn_t *fn);

/* DHCPv6 configuration options */
bool inet_base_dhcp6_client(inet_t *self, bool enable, bool request_addr, bool request_prefix, bool rapid_commit);
/* DHCPv6 options that shall be requested from remote */
bool inet_base_dhcp6_client_option_request(inet_t *self, int tag, bool request);
/* DHCPv6 options that will be sent to the server */
bool inet_base_dhcp6_client_option_send(inet_t *self, int tag, char *value);
/* DHCPv6 client status notification */
bool inet_base_dhcp6_client_notify(inet_t *super, inet_dhcp6_client_notify_fn_t *fn, void *ctx);
/* DHCPv6 Server: Options that will be sent to the server */
bool inet_base_dhcp6_server(inet_t *self, bool enable);
/* DHCPv6 Server: Add an IPv6 range to the server */
bool inet_base_dhcp6_server_prefix(inet_t *self, bool add, struct osn_dhcpv6_server_prefix *prefix);
/* DHCPv6 Server: Options that will be sent to the client */
bool inet_base_dhcp6_server_option_send(inet_t *self, int tag, char *data);
/* DHCPv6 Server: Lease list */
bool inet_base_dhcp6_server_lease(inet_t *self, bool add, struct osn_dhcpv6_server_lease *lease);
/* DHCPv6 Server: status notification */
bool inet_base_dhcp6_server_notify(inet_t *self, inet_dhcp6_server_notify_fn_t *fn, void *ctx);
/* Router Advertisement: Push new options for this interface */
bool inet_base_radv(inet_t *self, bool enable, struct osn_ip6_radv_options *opts);
/* Router Advertisement: Add or remove an advertised prefix */
bool inet_base_radv_prefix(inet_t *self, bool add, osn_ip6_addr_t *prefix, bool autonomous, bool onlink);
bool inet_base_radv_rdnss(inet_t *self, bool add, osn_ip6_addr_t *addr);
bool inet_base_radv_dnssl(inet_t *self, bool add, char *sl);

/*
 * ===========================================================================
 *  Misc
 * ===========================================================================
 */

extern const char *inet_base_service_str(enum inet_base_services srv);

#endif /* INET_BASE_H_INCLUDED */
