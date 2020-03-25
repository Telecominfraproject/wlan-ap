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

#ifndef INET_H_INCLUDED
#define INET_H_INCLUDED

#include <stdbool.h>
#include <stdlib.h>

#include "os_types.h"
#include "const.h"

#include "osn_inet6.h"
#include "osn_dhcpv6.h"
#include "osn_dhcp.h"
#include "osn_upnp.h"
#include "osn_inet.h"

/*
 * ===========================================================================
 *  inet_t main interface
 * ===========================================================================
 */

/**
 * Amount of time to wait before commiting current configuration
 */
enum inet_assign_scheme
{
    INET_ASSIGN_INVALID,
    INET_ASSIGN_NONE,
    INET_ASSIGN_STATIC,
    INET_ASSIGN_DHCP
};

enum inet_proto
{
    INET_PROTO_UDP,
    INET_PROTO_TCP
};

/*
 * ===========================================================================
 *  Inet class definition
 * ===========================================================================
 */
typedef struct __inet_state inet_state_t;

struct __inet_state
{
    bool                    in_interface_enabled;
    bool                    in_network_enabled;
    enum inet_assign_scheme in_assign_scheme;
    int                     in_mtu;
    bool                    in_nat_enabled;
    enum osn_upnp_mode      in_upnp_mode;
    bool                    in_dhcps_enabled;
    bool                    in_dhcpc_enabled;
    bool                    in_port_status;
    osn_ip_addr_t           in_ipaddr;
    osn_ip_addr_t           in_netmask;
    osn_ip_addr_t           in_bcaddr;
    osn_ip_addr_t           in_gateway;
    osn_mac_addr_t          in_macaddr;
};

#define INET_STATE_INIT (inet_state_t)  \
{                                       \
    .in_ipaddr = OSN_IP_ADDR_INIT,      \
    .in_netmask = OSN_IP_ADDR_INIT,     \
    .in_bcaddr = OSN_IP_ADDR_INIT,      \
    .in_gateway = OSN_IP_ADDR_INIT,     \
    .in_macaddr = OSN_MAC_ADDR_INIT,    \
}

typedef struct __inet inet_t;

/* Port forwarding argument structure */
struct inet_portforward
{
    uint16_t         pf_dst_port;
    uint16_t         pf_src_port;
    enum inet_proto  pf_proto;
    osn_ip_addr_t    pf_dst_ipaddr;
};

#define INET_PORTFORWARD_INIT (struct inet_portforward) \
{                                                       \
    .pf_dst_ipaddr = OSN_IP_ADDR_INIT,                  \
}

enum inet_ip6_addr_origin
{
    INET_IP6_ORIGIN_STATIC,
    INET_IP6_ORIGIN_DHCP,
    INET_IP6_ORIGIN_AUTO_CONFIGURED,
    INET_IP6_ORIGIN_ERROR,
};

/*
 * Interface IPv6 address status
 */
struct inet_ip6_addr_status
{
    osn_ip6_addr_t              is_addr;
    enum inet_ip6_addr_origin   is_origin;
};

/*
 * Neighbor report
 */
struct inet_ip6_neigh_status
{
    osn_ip6_addr_t              in_ipaddr;
    osn_mac_addr_t              in_hwaddr;
};

/* Lease notify callback */
typedef bool inet_dhcp_lease_fn_t(
        void *data,
        bool released,
        struct osn_dhcp_server_lease *dl);

/* IPv6 status change notification */
typedef void inet_ip6_addr_status_fn_t(
        inet_t *self,
        struct inet_ip6_addr_status *status,
        bool remove);

/* IPv6 neighbor notification */
typedef void inet_ip6_neigh_status_fn_t(
        inet_t *self,
        struct inet_ip6_neigh_status *status,
        bool remove);

/* DHCPv6 Client status change notification */
typedef void inet_dhcp6_client_notify_fn_t(
        void *data,
        struct osn_dhcpv6_client_status *status);

/* DHCPv6 Server status change notification */
typedef void inet_dhcp6_server_notify_fn_t(
        void *data,
        struct osn_dhcpv6_server_status *status);

/* Pretty printing for inet_portforward */
#define PRI_inet_portforward \
        "[%s]@0.0.0.0:%d->"PRI_osn_ip_addr":%d"

#define FMT_inet_portforward(x) \
        (x).pf_proto == INET_PROTO_UDP ? "udp" : "tcp", \
        (x).pf_src_port, \
        FMT_osn_ip_addr((x).pf_dst_ipaddr), \
        (x).pf_dst_port

struct __inet
{
    /* Interface name */
    char        in_ifname[C_IFNAME_LEN];

    /* Used to store private data */
    void       *in_data;

    /* Destructor function */
    bool        (*in_dtor_fn)(inet_t *self);

    /* Enable/disable interface */
    bool        (*in_interface_enable_fn)(inet_t *self, bool enable);

    /* Set interface network  */
    bool        (*in_network_enable_fn)(inet_t *self, bool enable);

    /* Set MTU */
    bool        (*in_mtu_set_fn)(inet_t *self, int mtu);

    /**
     * Set IP assignment scheme:
     *   - INET_ASSIGN_NONE     - Interface does not have any IPv4 configuration
     *   - INET_ASSIGN_STATIC   - Use a static IP/Netmask/Broadcast address
     *   - INET_ASSIGN_DHCP     - Use dynamic IP address assignment
     */
    bool        (*in_assign_scheme_set_fn)(inet_t *self, enum inet_assign_scheme scheme);

    /*
     * Set IP Address/Netmask/Broadcast/Gateway -- only when assign_scheme == INET_ASSIGN_STATIC
     *
     * bcast and gateway can be INET_ADDR_NONE
     */
    bool        (*in_ipaddr_static_set_fn)(
                        inet_t *self,
                        osn_ip_addr_t addr,
                        osn_ip_addr_t netmask,
                        osn_ip_addr_t bcast);

    /* Set the interface default gateway, typically set when assign_scheme == INET_ASSIGN_STATIC */
    bool        (*in_gateway_set_fn)(inet_t *self, osn_ip_addr_t  gwaddr);

    /* Enable NAT */
    bool        (*in_nat_enable_fn)(inet_t *self, bool enable);

    /* Enable IGMP */
    bool        (*in_igmp_enable_fn)(inet_t *self, bool enable, int iage, int itsize);

    bool        (*in_upnp_mode_set_fn)(inet_t *self, enum osn_upnp_mode mode);

    /* Port forwarding */
    bool        (*in_portforward_set_fn)(inet_t *self, const struct inet_portforward *pf);
    bool        (*in_portforward_del_fn)(inet_t *self, const struct inet_portforward *pf);

    /* Set primary/secondary DNS servers */
    bool        (*in_dns_set_fn)(inet_t *self, osn_ip_addr_t primary, osn_ip_addr_t secondary);

    /* DHCP client options */
    bool        (*in_dhcpc_option_request_fn)(inet_t *self, enum osn_dhcp_option opt, bool req);
    bool        (*in_dhcpc_option_set_fn)(inet_t *self, enum osn_dhcp_option opt, const char *value);
    bool        (*in_dhcpc_option_notify_fn)(inet_t *self, osn_dhcp_client_opt_notify_fn_t *fn, void *ctx);

    /* True if DHCP server should be enabled on this interface */
    bool        (*in_dhcps_enable_fn)(inet_t *self, bool enabled);

    /* DHCP server otpions */
    bool        (*in_dhcps_lease_set_fn)(inet_t *self, int lease_time_s);
    bool        (*in_dhcps_range_set_fn)(inet_t *self, osn_ip_addr_t start, osn_ip_addr_t stop);
    bool        (*in_dhcps_option_set_fn)(inet_t *self, enum osn_dhcp_option opt, const char *value);
    bool        (*in_dhcps_lease_notify_fn)(inet_t *self, inet_dhcp_lease_fn_t *fn);
    bool        (*in_dhcps_rip_set_fn)(inet_t *super,
                        osn_mac_addr_t macaddr,
                        osn_ip_addr_t ip4addr,
                        const char *hostname);
    bool        (*in_dhcps_rip_del_fn)(inet_t *super, osn_mac_addr_t macaddr);


    /* IPv4 tunnels (GRE, softwds) */
    bool        (*in_ip4tunnel_set_fn)(inet_t *self,
                        const char *parent,
                        osn_ip_addr_t local,
                        osn_ip_addr_t remote,
                        osn_mac_addr_t macaddr);
    /* VLANs */
    bool       (*in_vlan_set_fn)(inet_t *self, const char *ifparent, int vlanid);

    /* DHCP sniffing - register callback for DHCP sniffing - if set to NULL sniffing is disabled */
    bool        (*in_dhsnif_lease_notify_fn)(inet_t *self, inet_dhcp_lease_fn_t *func);

    /* Routing table methods  -- if set to NULL route state reporting is disabled */
    bool        (*in_route_notify_fn)(inet_t *self, osn_route_status_fn_t *func);

    /* Commit all pending changes */
    bool        (*in_commit_fn)(inet_t *self);

    /* State get method */
    bool        (*in_state_get_fn)(inet_t *self, inet_state_t *out);

    /*
     * ===========================================================================
     *  IPv6 support
     * ===========================================================================
     */

    /* Static IPv6 configuration: Add/remove IP addresess */
    bool        (*in_ip6_addr_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *addr);

    /* Static IPv6 configuration: Add/remove DNS servers */
    bool        (*in_ip6_dns_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *addr);

    bool        (*in_ip6_addr_status_notify_fn)(
                        inet_t *self,
                        inet_ip6_addr_status_fn_t *fn);

    bool        (*in_ip6_neigh_status_notify_fn)(
                        inet_t *self,
                        inet_ip6_neigh_status_fn_t *fn);

    /* DHCPv6 client configuration options */
    bool        (*in_dhcp6_client_fn)(
                        inet_t *self,
                        bool enable,
                        bool request_addr,
                        bool request_prefix,
                        bool rapid_commit);

    /* DHCPv6 options that shall be requested from remote */
    bool        (*in_dhcp6_client_option_request_fn)(inet_t *self, int tag, bool request);

    /* DHCPv6 options that will be sent to the server */
    bool        (*in_dhcp6_client_option_send_fn)(inet_t *self, int tag, char *value);

    /* DHCPv6 client status notification */
    bool        (*in_dhcp6_client_notify_fn)(inet_t *self, inet_dhcp6_client_notify_fn_t *fn, void *ctx);

    /* DHCPv6 Server options */
    bool        (*in_dhcp6_server_fn)(inet_t *self, bool enable);

    /* DHCPv6 Server prefixes */
    bool        (*in_dhcp6_server_prefix_fn)(
                        inet_t *self,
                        bool add,
                        struct osn_dhcpv6_server_prefix *prefix);

    /* DHCPv6 options that will be sent to the client */
    bool        (*in_dhcp6_server_option_send_fn)(inet_t *self, int tag, char *value);

    /* DHCPv6 Server static leases */
    bool        (*in_dhcp6_server_lease_fn)(
                        inet_t *self,
                        bool add,
                        struct osn_dhcpv6_server_lease *lease);

    /* DHCPv6 server status notification */
    bool        (*in_dhcp6_server_notify_fn)(inet_t *self, inet_dhcp6_server_notify_fn_t *fn, void *ctx);

    /* Router Advertisement: Set options */
    bool        (*in_radv_fn)(
                        inet_t *self,
                        bool enable,
                        struct osn_ip6_radv_options *opts);

    /* Router Advertisement: Add or remove */
    bool        (*in_radv_prefix_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *prefix,
                        bool autonomous,
                        bool onlink);

    /* Router Advertisement: Add or remove RDNSS entries */
    bool        (*in_radv_rdnss_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *dns);

    /* Router Advertisement: Add or remove DNSSL entries */
    bool        (*in_radv_dnssl_fn)(
                        inet_t *self,
                        bool add,
                        char *sl);

};

/*
 * ===========================================================================
 *  Inet Class Interfaces
 * ===========================================================================
 */

/**
 * Static destructor, counterpart to _init()
 */
static inline bool inet_fini(inet_t *self)
{
    if (self->in_dtor_fn == NULL) return false;

    return self->in_dtor_fn(self);
}

/**
 * Dynamic destructor, counterpart to _new()
 */
static inline bool inet_del(inet_t *self)
{
    bool retval = inet_fini(self);

    free(self);

    return retval;
}

static inline bool inet_interface_enable(inet_t *self, bool enable)
{
    if (self->in_interface_enable_fn == NULL) return false;

    return self->in_interface_enable_fn(self, enable);
}

static inline bool inet_network_enable(inet_t *self, bool enable)
{
    if (self->in_network_enable_fn == NULL) return false;

    return self->in_network_enable_fn(self, enable);
}

static inline bool inet_mtu_set(inet_t *self, int mtu)
{
    if (self->in_mtu_set_fn == NULL) return false;

    return self->in_mtu_set_fn(self, mtu);
}

static inline bool inet_assign_scheme_set(inet_t *self, enum inet_assign_scheme scheme)
{
    if (self->in_assign_scheme_set_fn == NULL) return false;

    return self->in_assign_scheme_set_fn(self, scheme);
}

static inline bool inet_ipaddr_static_set(
        inet_t *self,
        osn_ip_addr_t ipaddr,
        osn_ip_addr_t netmask,
        osn_ip_addr_t  bcaddr)
{
    if (self->in_ipaddr_static_set_fn == NULL) return false;

    return self->in_ipaddr_static_set_fn(
            self,
            ipaddr,
            netmask,
            bcaddr);
}

static inline bool inet_gateway_set(inet_t *self, osn_ip_addr_t gwaddr)
{
    if (self->in_gateway_set_fn == NULL) return false;

    return self->in_gateway_set_fn(self, gwaddr);
}

static inline bool inet_nat_enable(inet_t *self, bool enable)
{
    if (self->in_nat_enable_fn == NULL) return false;

    return self->in_nat_enable_fn(self, enable);
}

static inline bool inet_igmp_enable(inet_t *self, int iigmp, int iage, int itsize)
{
    if (self->in_igmp_enable_fn == NULL) return false;

    return self->in_igmp_enable_fn(self, iigmp, iage, itsize);
}

static inline bool inet_upnp_mode_set(inet_t *self, enum osn_upnp_mode mode)
{
    if (self->in_upnp_mode_set_fn == NULL) return false;

    return self->in_upnp_mode_set_fn(self, mode);
}


static inline bool inet_portforward_set(inet_t *self, const struct inet_portforward *pf)
{
    if (self->in_portforward_set_fn == NULL) return false;

    return self->in_portforward_set_fn(self, pf);
}

static inline bool inet_portforward_del(inet_t *self, const struct inet_portforward *pf)
{
    if (self->in_portforward_del_fn == NULL) return false;

    return self->in_portforward_del_fn(self, pf);
}

static inline bool inet_dns_set(inet_t *self, osn_ip_addr_t primary, osn_ip_addr_t secondary)
{
    if (self->in_dns_set_fn == NULL) return false;

    return self->in_dns_set_fn(self, primary, secondary);
}

static inline bool inet_dhcpc_option_request(inet_t *self, enum osn_dhcp_option opt, bool req)
{
    if (self->in_dhcpc_option_set_fn == NULL) return false;

    return self->in_dhcpc_option_request_fn(self, opt, req);
}

static inline bool inet_dhcpc_option_set(inet_t *self, enum osn_dhcp_option opt, const char *value)
{
    if (self->in_dhcpc_option_set_fn == NULL) return false;

    return self->in_dhcpc_option_set_fn(self, opt, value);
}

static inline bool inet_dhcpc_option_notify(inet_t *self, osn_dhcp_client_opt_notify_fn_t *fn, void *ctx)
{
    if (self->in_dhcpc_option_notify_fn == NULL) return false;

    return self->in_dhcpc_option_notify_fn(self, fn, ctx);
}

static inline bool inet_dhcps_enable(inet_t *self, bool enabled)
{
    if (self->in_dhcps_enable_fn == NULL) return false;

    return self->in_dhcps_enable_fn(self, enabled);
}

static inline bool inet_dhcps_lease_set(inet_t *self, int lease_time_s)
{
    if (self->in_dhcps_lease_set_fn == NULL) return false;

    return self->in_dhcps_lease_set_fn(self, lease_time_s);
}

static inline bool inet_dhcps_range_set(inet_t *self, osn_ip_addr_t start, osn_ip_addr_t stop)
{
    if (self->in_dhcps_range_set_fn == NULL) return false;

    return self->in_dhcps_range_set_fn(self, start, stop);
}

static inline bool inet_dhcps_option_set(inet_t *self, enum osn_dhcp_option opt, const char *value)
{
    if (self->in_dhcps_option_set_fn == NULL) return false;

    return self->in_dhcps_option_set_fn(self, opt, value);
}

static inline bool inet_dhcps_lease_notify(inet_t *self, inet_dhcp_lease_fn_t *fn)
{
    if (self->in_dhcps_lease_notify_fn == NULL) return false;

    return self->in_dhcps_lease_notify_fn(self, fn);
}

static inline bool inet_dhcps_rip_set(inet_t *self, osn_mac_addr_t macaddr,
                                      osn_ip_addr_t ip4addr, const char *hostname)
{
    if (self->in_dhcps_rip_set_fn == NULL) return false;

    return self->in_dhcps_rip_set_fn(self, macaddr, ip4addr, hostname);
}

static inline bool inet_dhcps_rip_del(inet_t *self, osn_mac_addr_t macaddr)
{
    if (self->in_dhcps_rip_del_fn == NULL) return false;

    return self->in_dhcps_rip_del_fn(self, macaddr);
}


/**
 * Set IPv4 tunnel options
 *
 * parent   - parent interface
 * laddr    - local IP address
 * raddr    - remote IP address
 * rmac     - remote MAC address, this field is ignored for some protocols such as GRETAP
 */
static inline bool inet_ip4tunnel_set(
        inet_t *self,
        const char *parent,
        osn_ip_addr_t laddr,
        osn_ip_addr_t raddr,
        osn_mac_addr_t rmac)
{
    if (self->in_ip4tunnel_set_fn == NULL) return false;

    return self->in_ip4tunnel_set_fn(self, parent, laddr, raddr, rmac);
}

/*
 * VLAN settings
 *
 * ifparent     - parent interface
 * vlanid       - the VLAN id
 */
static inline bool inet_vlan_set(
        inet_t *self,
        const char *ifparent,
        int vlanid)
{
    if (self->in_vlan_set_fn == NULL) return false;

    return self->in_vlan_set_fn(self, ifparent, vlanid);
}

/*
 * DHCP sniffing - @p func will be called each time a DHCP packet is sniffed
 * on the interface. This can happen multiple times for the same client
 * (depending on the DHCP negotiation phase, more data can be made available).
 *
 * If func is NULL, DHCP sniffing is disabled on the interface.
 */
static inline bool inet_dhsnif_lease_notify(inet_t *self, inet_dhcp_lease_fn_t *func)
{
    if (self->in_dhsnif_lease_notify_fn == NULL) return false;

    return self->in_dhsnif_lease_notify_fn(self, func);
}

/*
 * Subscribe to route table state changes.
 */
static inline bool inet_route_notify(inet_t *self, osn_route_status_fn_t *func)
{
    if (self->in_route_notify_fn == NULL) return false;

    return self->in_route_notify_fn(self, func);
}

/**
 * Commit all pending changes; the purpose of this function is to figure out the order
 * in which subsystems must be brought up or tore down and call inet_apply() for each
 * one of them
 */
static inline bool inet_commit(inet_t *self)
{
    if (self->in_commit_fn == NULL) return false;

    return self->in_commit_fn(self);
}

/**
 * State retrieval -- simple polling interface
 */
static inline bool inet_state_get(inet_t *self, inet_state_t *out)
{
    if (self->in_state_get_fn == NULL) return false;

    return self->in_state_get_fn(self, out);
}


/*
 * ===========================================================================
 *  IPv6 support
 * ===========================================================================
 */
/* Static IPv6 configuration: Add/remove IP addresess */
static inline bool inet_ip6_addr(inet_t *self, bool add, osn_ip6_addr_t *addr)
{
    if (self->in_ip6_addr_fn == NULL) return false;

    return self->in_ip6_addr_fn(self, add, addr);
}

/* Static IPv6 configuration: Add/remove DNS servers */
static inline bool inet_ip6_dns(inet_t *self, bool add, osn_ip6_addr_t *dns)
{
    if (self->in_ip6_dns_fn == NULL) return false;

    return self->in_ip6_dns_fn(self, add, dns);
}

/*
 * IPv6 status change notification callback
 */
static inline bool inet_ip6_addr_status_notify(
        inet_t *self,
        inet_ip6_addr_status_fn_t *fn)
{
    if (self->in_ip6_addr_status_notify_fn == NULL) return false;

    return self->in_ip6_addr_status_notify_fn(self, fn);
}

/*
 * IPv6 neighbor status change notification callback
 */
static inline bool inet_ip6_neigh_status_notify(
        inet_t *self,
        inet_ip6_neigh_status_fn_t *fn)
{
    if (self->in_ip6_neigh_status_notify_fn == NULL) return false;

    return self->in_ip6_neigh_status_notify_fn(self, fn);
}

/* DHCPv6 configuration options */
static inline bool inet_dhcp6_client(
        inet_t *self,
        bool enable,
        bool request_addr,
        bool request_prefix,
        bool rapid_commit)
{
    if (self->in_dhcp6_client_fn == NULL) return false;

    return self->in_dhcp6_client_fn(
            self,
            enable,
            request_addr,
            request_prefix,
            rapid_commit);
}

/* DHCPv6 options that shall be requested from remote */
static inline bool inet_dhcp6_client_option_request(inet_t *self, int tag, bool request)
{
    if (self->in_dhcp6_client_option_request_fn == NULL) return false;

    return self->in_dhcp6_client_option_request_fn(self, tag, request);
}

/* DHCPv6 options that will be sent to the server */
static inline bool inet_dhcp6_client_option_send(inet_t *self, int tag, char *value)
{
    if (self->in_dhcp6_client_option_send_fn == NULL) return false;

    return self->in_dhcp6_client_option_send_fn(self, tag, value);
}

static inline bool inet_dhcp6_client_notify(inet_t *self, inet_dhcp6_client_notify_fn_t *fn, void *ctx)
{
    if (self->in_dhcp6_client_notify_fn == NULL) return false;

    return self->in_dhcp6_client_notify_fn(self, fn, ctx);
}

/* DHCPv6 Server: Options that will be sent to the server */
static inline bool inet_dhcp6_server(inet_t *self, bool enable)
{
    if (self->in_dhcp6_server_fn == NULL) return false;

    return self->in_dhcp6_server_fn(self, enable);
}

/* DHCPv6 Server: Add an IPv6 range to the server */
static inline bool inet_dhcp6_server_prefix(inet_t *self, bool add, struct osn_dhcpv6_server_prefix *prefix)
{
    if (self->in_dhcp6_server_prefix_fn == NULL) return false;

    return self->in_dhcp6_server_prefix_fn(self, add, prefix);
}

/* DHCPv6 options that will be sent to the client */
static inline bool inet_dhcp6_server_option_send(inet_t *self, int tag, char *value)
{
    if (self->in_dhcp6_server_option_send_fn == NULL) return false;

    return self->in_dhcp6_server_option_send_fn(self, tag, value);
}


/* DHCPv6 Server: Options that will be sent to the server */
static inline bool inet_dhcp6_server_lease(inet_t *self, bool add, struct osn_dhcpv6_server_lease *lease)
{
    if (self->in_dhcp6_server_lease_fn == NULL) return false;

    return self->in_dhcp6_server_lease_fn(self, add, lease);
}

static inline bool inet_dhcp6_server_notify(inet_t *self, inet_dhcp6_server_notify_fn_t *fn, void *ctx)
{
    if (self->in_dhcp6_server_notify_fn == NULL) return false;

    return self->in_dhcp6_server_notify_fn(self, fn, ctx);
}

/* Router Advertisement: Push new options for this interface */
static inline bool inet_radv(inet_t *self, bool enable, struct osn_ip6_radv_options *opts)
{
    if (self->in_radv_fn == NULL) return false;

    return self->in_radv_fn(self, enable, opts);
}

/* Router Advertisement: Add or remove an advertised prefix */
static inline bool inet_radv_prefix(
        inet_t *self,
        bool add,
        osn_ip6_addr_t *prefix,
        bool autonomous,
        bool onlink)
{
    if (self->in_radv_prefix_fn == NULL) return false;

    return self->in_radv_prefix_fn(self, add, prefix, autonomous, onlink);
}

static inline bool inet_radv_rdnss(inet_t *self, bool add, osn_ip6_addr_t  *addr)
{
    if (self->in_radv_rdnss_fn == NULL) return false;

    return self->in_radv_rdnss_fn(self, add, addr);
}

static inline bool inet_radv_dnssl(inet_t *self, bool add, char *sl)
{
    if (self->in_radv_dnssl_fn == NULL) return false;

    return self->in_radv_dnssl_fn(self, add, sl);
}

#endif /* INET_H_INCLUDED */
