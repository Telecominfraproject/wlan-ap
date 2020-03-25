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

#ifndef NM2_IFACE_H_INCLUDED
#define NM2_IFACE_H_INCLUDED

/*
 * ===========================================================================
 *  This file exposes some of the internal NM2 structures. This is not intended
 *  to be permanent, however it is currently necessary in order to complete
 *  the transition from the legacy target API to Osync.
 * ===========================================================================
 */

#include <stdbool.h>
#include <ev.h>

#include "ds_list.h"
#include "ds_tree.h"
#include "schema.h"

#include "inet.h"
#include "inet_base.h"
#include "inet_eth.h"
#include "inet_vif.h"
#include "inet_gre.h"
#include "inet_vlan.h"

/* Debounce timer for interface configuration commit */
#define NM2_IFACE_APPLY_TIMER 0.3

/*
 * Macros for copying values from schema structures.
 *
 * _CONFIG_SZ() is used to calculate the size of a field.
 *
 * _CONFIG_COPY() is used to copy a filed to a buffer. If the size of the
 * buffer and the size of the schema field are mismatched, this macro
 * should fail at compile time.
 */
#define NM2_IFACE_INET_CONFIG_SZ(field)     sizeof(((struct schema_Wifi_Inet_Config *)NULL)->field)
#define NM2_IFACE_INET_CONFIG_COPY(a, b)    \
        do \
        { \
            C_STATIC_ASSERT(sizeof(a) == sizeof(b), #a " not equal in size to " #b); \
            memcpy(&a, &b, sizeof(a)); \
        } \
        while (0)

/*
 * ===========================================================================
 * Interface type definition
 * ===========================================================================
 */

/*
 * The type is implement as an X-macro. The nm2_iftype enum and conversion to/from string
 * is implemented using this macro. The interface type string must match the schema strings.
 */
#define NM2_IFTYPE(M)                   \
    M(NM2_IFTYPE_NONE,      "none")     \
    M(NM2_IFTYPE_ETH,       "eth")      \
    M(NM2_IFTYPE_VIF,       "vif")      \
    M(NM2_IFTYPE_VLAN,      "vlan")     \
    M(NM2_IFTYPE_BRIDGE,    "bridge")   \
    M(NM2_IFTYPE_GRE,       "gre")      \
    M(NM2_IFTYPE_TAP,       "tap")      \
    M(NM2_IFTYPE_MAX,       NULL)


/* Generate the nm2_iftype enum */
enum nm2_iftype
{
    #define _ENUM(sym, str) sym,
    NM2_IFTYPE(_ENUM)
    #undef _ENUM
};

const char *nm2_iftype_tostr(enum nm2_iftype type);
bool nm2_iftype_fromstr(enum nm2_iftype *type, const char *str);

/*
 * ===========================================================================
 *  DHCP options -- cached per interface
 * ===========================================================================
 */
struct nm2_iface_dhcp_option
{
    char                *do_name;
    char                *do_value;
    bool                do_invalid;
    ds_tree_node_t      do_tnode;
};

/*
 * ===========================================================================
 *  NM2 per-interface structure
 * ===========================================================================
 */
struct nm2_iface
{
    char                            if_name[C_IFNAME_LEN];      /* Interface name */
    enum nm2_iftype                 if_type;                    /* Interface type */
    bool                            if_commit;                  /* Commit pending */
    inet_t*                         if_inet;                    /* Inet structure */
    ds_tree_node_t                  if_tnode;                   /* ds_tree node -- for device lookup by name */
    inet_state_t                    if_state;                   /* Remembered last interface status */
    bool                            if_state_notify;            /* New status must be reported -- typically set by a configuration change */
    ds_tree_t                       if_dhcpc_options;           /* Options received from the DHCP client */
    struct nm2_ip_interface        *if_ipi;                     /* Non-NULL if there's a nm2_ip_interface structure associated */

    /*
     * Fields that should be copied from Wifi_Inet_Config to Wifi_Inet_State
     * TODO This should be moved to nm2_inet_config eventually.
     */
    struct
    {
        uint8_t                     if_type[NM2_IFACE_INET_CONFIG_SZ(if_type)];
        uint8_t                     if_uuid[NM2_IFACE_INET_CONFIG_SZ(if_uuid)];
        uint8_t                     _uuid[NM2_IFACE_INET_CONFIG_SZ(_uuid)];
        uint8_t                     dns[NM2_IFACE_INET_CONFIG_SZ(dns)];
        uint8_t                     dns_keys[NM2_IFACE_INET_CONFIG_SZ(dns_keys)];
        int                         dns_len;
        uint8_t                     dhcpd[NM2_IFACE_INET_CONFIG_SZ(dhcpd)];
        uint8_t                     dhcpd_keys[NM2_IFACE_INET_CONFIG_SZ(dhcpd_keys)];
        int                         dhcpd_len;
        bool                        gre_ifname_exists;
        uint8_t                     gre_ifname[NM2_IFACE_INET_CONFIG_SZ(gre_ifname)];
        bool                        gre_remote_inet_addr_exists;
        uint8_t                     gre_remote_inet_addr[NM2_IFACE_INET_CONFIG_SZ(gre_remote_inet_addr)];
        bool                        gre_local_inet_addr_exists;
        uint8_t                     gre_local_inet_addr[NM2_IFACE_INET_CONFIG_SZ(gre_local_inet_addr)];
    }
    if_cache;
};

/*
 * ===========================================================================
 *  Public functions
 * ===========================================================================
 */

bool nm2_iface_init(void);
struct nm2_iface *nm2_iface_get_by_name(char *_ifname);
struct nm2_iface *nm2_iface_new(const char *_ifname, enum nm2_iftype if_type);
struct nm2_iface *nm2_iface_find_by_ipv4(osn_ip_addr_t addr);
bool nm2_iface_del(struct nm2_iface *piface);
void nm2_iface_status_poll(void);
bool nm2_iface_apply(struct nm2_iface *piface);
void nm2_iface_status_sync(struct nm2_iface *piface);

#endif /* NM2_IFACE_H_INCLUDED */
