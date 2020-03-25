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

#ifndef NM2_H_INCLUDED
#define NM2_H_INCLUDED

#include "ovsdb.h"
#include "reflink.h"
#include "synclist.h"
#include "const.h"

#include "osn_types.h"
#include "osn_inet6.h"
#include "osn_dhcpv6.h"
#include "nm2_iface.h"
#include "nm2_util.h"

/*
 * ===========================================================================
 *  New schema
 * ===========================================================================
 */

/*
 * IP_Interface cached structure.
 */
struct nm2_ip_interface
{
    ovs_uuid_t          ipi_uuid;                   /* uuid of this entry */
    reflink_t           ipi_reflink;                /* Reflink of this object */
    bool                ipi_valid;                  /* True if entry is valid */
    char                ipi_ifname[C_IFNAME_LEN];   /* Interface name */
    bool                ipi_enable;                 /* Enabled status */
    uuidset_t           ipi_ipv4_addr;              /* TODO: uuidset of IPv4_Address objects */
    uuidset_t           ipi_ipv6_addr;              /* uuidset of IPv6_Address objects */
    uuidset_t           ipi_ipv6_prefix;            /* uuidset of IPv6_Prefix objects */
    struct nm2_iface    *ipi_iface;                 /* Pointer to resolved nm2_iface structure */
    ds_tree_node_t      ipi_tnode;                  /* Tree element structure */
};

void nm2_ip_interface_init(void);
reflink_t *nm2_ip_interface_getref(const ovs_uuid_t *);

/*
 * IPv6 address cached structure.
 */
struct nm2_ipv6_address
{
    ovs_uuid_t                  ip6_uuid;                   /* UUID */
    reflink_t                   ip6_reflink;                /* Main reflink of this object */
    bool                        ip6_valid;                  /* True if entry is valid */
    struct osn_ip6_addr         ip6_addr;                   /* IPv6 Address */
    enum inet_ip6_addr_origin   ip6_origin;                 /* IPv6 Address origin */
    ds_tree_node_t              ip6_tnode;                  /* Tree element structure */
};

void nm2_ipv6_address_init(void);
reflink_t *nm2_ipv6_address_getref(const ovs_uuid_t *uuid);

/*
 * IPv6 prefix cached structure.
 */
struct nm2_ipv6_prefix
{
    ovs_uuid_t          ip6p_uuid;                  /* UUID */
    reflink_t           ip6p_reflink;               /* Main reflink of this object */
    bool                ip6p_valid;                 /* True if entry is valid */
    struct osn_ip6_addr ip6p_addr;                  /* IPv6 Address */
    bool                ip6p_on_link;               /* On link status */
    bool                ip6p_autonomous;            /* Autonomous status */
    ds_tree_node_t      ip6p_tnode;                 /* Tree element structure */
};

void nm2_ipv6_prefix_init(void);
reflink_t *nm2_ipv6_prefix_getref(const ovs_uuid_t *uuid);

/*
 * IPv6 DHCP client cached structure.
 */
struct nm2_dhcpv6_client
{
    ovs_uuid_t          dc6_uuid;                   /* UUID of this structure */
    reflink_t           dc6_reflink;                /* Main reflink of this object */
    bool                dc6_valid;                  /* True if entry is valid */
    ovs_uuid_t          dc6_ip_interface_uuid;      /* UUID of the referenced IP_Interface */
    reflink_t           dc6_ip_interface_reflink;   /* Reflink to IP_Interface */
    bool                dc6_request_address;        /* Request address */
    bool                dc6_request_prefixes;       /* Request prefixes */
    bool                dc6_rapid_commit;           /* Rapid Commit */
    bool                dc6_renew;                  /* Renew */
    int                 dc6_request_opts[32];       /* Request option IDs */
    int                 dc6_request_opts_len;       /* Length of option array */
    uuidset_t           dc6_send_options;           /* uuidset of DHCP_Option objects */
    uuidset_t           dc6_received_options;       /* uuidset of DHCP_Option objects */
    synclist_t          dc6_received_list;          /* synclist of received options */
    ds_tree_node_t      dc6_tnode;                  /* Tree element structure */
};

void nm2_dhcpv6_client_init(void);
reflink_t *nm2_dhcpv6_client_getref(const ovs_uuid_t *uuid);
struct nm2_iface *nm2_ip_interface_iface_get(ovs_uuid_t *uuid);

/*
 * IPv6 DHCP server cached structure.
 */
struct nm2_dhcpv6_server
{
    ovs_uuid_t          ds6_uuid;                   /* UUID of this structure */
    reflink_t           ds6_reflink;                /* Main reflink of this object */
    bool                ds6_valid;                  /* True if entry is valid */
    ovs_uuid_t          ds6_ip_interface_uuid;      /* UUID of the referenced IP_Interface */
    reflink_t           ds6_ip_interface_reflink;   /* Reflink to IP_Interface */
    char                ds6_status[16];             /* Server status */
    bool                ds6_prefix_delegation;      /* True if prefix delegation is enabled */
    uuidset_t           ds6_prefixes;               /* uuidset of IPv6_Prefix objects */
    uuidset_t           ds6_options;                /* uuidset of DHCP_Option objects */
    uuidset_t           ds6_lease_prefix;           /* uuidset of DHCPv6_Lease objects */
    uuidset_t           ds6_static_prefix;          /* uuidset of DHCPv6_Lease objects */
    synclist_t          ds6_lease_list;             /* synclist of leases associated with server */
    ds_tree_node_t      ds6_tnode;
};

void nm2_dhcpv6_server_init(void);
reflink_t *nm2_dhcpv6_server_getref(const ovs_uuid_t *uuid);

/*
 * DHCP options
 */
struct nm2_dhcp_option
{
    ovs_uuid_t          dco_uuid;                   /* UUID of the structure */
    reflink_t           dco_reflink;                /* Main reflink of the structure */
    bool                dco_valid;                  /* True if structure was initialized and is valid */
    bool                dco_enable;                 /* Enable status */
    char                dco_version[3];             /* Either "v4" or "v6" */
    char                dco_type[3];                /* Either 'rx' or 'tx' */
    int                 dco_tag;                    /* Tag number */
    char                dco_value[341];             /* Base64 encoded value */
    ds_tree_node_t      dco_tnode;
};

void nm2_dhcp_option_init(void);
reflink_t *nm2_dhcp_option_getref(const ovs_uuid_t *uuid);

/*
 * DHCP lease
 */
struct nm2_dhcpv6_lease
{
    ovs_uuid_t          d6l_uuid;                   /* UUID of the structure */
    reflink_t           d6l_reflink;                /* Main reflink of the structure */
    bool                d6l_valid;                  /* True if structure was initialized and is valid */
    char                d6l_status[32];             /* Statis */
    struct osn_ip6_addr d6l_prefix;                 /* IPv6 prefix */
    char                d6l_duid[261];              /* Client unique DUID */
    osn_mac_addr_t      d6l_hwaddr;                 /* Hwardware address */
    char                d6l_hostname[65];           /* Hostname */
    int                 d6l_leased_time;
    ds_tree_node_t      d6l_tnode;
};

void nm2_dhcpv6_lease_init(void);
reflink_t *nm2_dhcpv6_lease_getref(const ovs_uuid_t *uuid);

/*
 * IPv6_RouterAdv cached structure.
 */
struct nm2_ipv6_routeadv
{
    ovs_uuid_t          ra_uuid;                    /* UUID of this structure */
    reflink_t           ra_reflink;                 /* Main reflink of this object */
    bool                ra_valid;                   /* True if entry is valid */
    char                ra_status[16];              /* RouteAdv status */
    ovs_uuid_t          ra_ip_interface_uuid;       /* UUID of the referenced IP_Interface */
    reflink_t           ra_ip_interface_reflink;    /* Reflink to IP_Interface */
    uuidset_t           ra_prefixes;                /* Prefixes: uuidset of IPv6_Prefix objects */
    uuidset_t           ra_rdnss;                   /* RDNSS: uuidset of IPv6_Address objects */
    synclist_t          ra_dnssl;                   /* List of DNSSL entries */
    struct osn_ip6_radv_options                     /* Router advertisement options */
                        ra_opts;
    ds_tree_node_t      ra_tnode;                   /* Tree element structure */
};

void nm2_ipv6_routeadv_init(void);
reflink_t *nm2_ipv6_routeadv_getref(const ovs_uuid_t *uuid);

/*
 * ===========================================================================
 *  Legacy schema
 * ===========================================================================
 */
/* Port forwarding */
bool nm2_portfw_init(void);

/*Interface config and state */
void nm2_inet_config_init(void);
void nm2_inet_state_init(void);
bool nm2_inet_state_update(struct nm2_iface *piface);
bool nm2_inet_state_del(const char *ifname);

/* DHCP */
bool nm2_dhcp_table_init(void);
bool nm2_dhcp_rip_init(void);
bool nm2_dhcp_lease_notify(void *self, bool released, struct osn_dhcp_server_lease *dl);

/* Route handling */
bool nm2_route_init(void);
bool nm2_route_notify(void *data, struct osn_route_status *rts, bool remove);

/*
 * ===========================================================================
 *  Misc
 * ===========================================================================
 */
bool nm2_mac_learning_init(void);
bool nm2_client_nickname_init(void);
int nm2_mac_tags_ovsdb_init(void);
int lan_clients_oftag_add_mac(char *mac);
int lan_clients_oftag_remove_mac(char *mac);

/*
 * ===========================================================================
 *  IPv6
 * ===========================================================================
 */

void nm2_ip6_addr_status_fn(inet_t *inet, struct inet_ip6_addr_status *as, bool remove);
void nm2_ip6_neigh_status_fn(inet_t *inet, struct inet_ip6_neigh_status *ns, bool remove);

#endif
