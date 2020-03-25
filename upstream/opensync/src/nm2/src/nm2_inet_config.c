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

#include <stdarg.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "nm2.h"
#include "nm2_iface.h"

#include "inet.h"

#define MODULE_ID LOG_MODULE_ID_OVSDB

#define SCHEMA_IF_TYPE_VIF "vif"
/*
 * ===========================================================================
 *  Global variables
 * ===========================================================================
 */

static ovsdb_table_t table_Wifi_Inet_Config;

/*
 * ===========================================================================
 *  Forward declarations
 * ===========================================================================
 */
static void callback_Wifi_Inet_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Inet_Config *old_rec,
        struct schema_Wifi_Inet_Config *iconf);

static bool nm2_inet_interface_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_igmp_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_ip_assign_scheme_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_upnp_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_static_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dns_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dhcps_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dhcps_options_set(
        struct nm2_iface *piface,
        const char *opts);

static bool nm2_inet_ip4tunnel_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dhsnif_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_vlan_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static void nm2_inet_copy(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

#define SCHEMA_FIND_KEY(x, key)    __find_key( \
        (char *)x##_keys, sizeof(*(x ## _keys)), \
        (char *)x, sizeof(*(x)), x##_len, key)

static const char * __find_key(
        char *keyv,
        size_t keysz,
        char *datav,
        size_t datasz,
        int vlen,
        const char *key);
struct nm2_iface *nm2_add_inet_conf(struct schema_Wifi_Inet_Config *iconf);
void nm2_del_inet_conf(struct schema_Wifi_Inet_Config *old_rec);
struct nm2_iface *nm2_modify_inet_conf(struct schema_Wifi_Inet_Config *iconf);


/*
 * ===========================================================================
 * Initialize Inet Config
 * ===========================================================================
 */
void nm2_inet_config_init(void)
{
    LOGI("Initializing NM Inet Config");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT(Wifi_Inet_Config, if_name);

    // Initialize OVSDB monitor callbacks
    OVSDB_TABLE_MONITOR(Wifi_Inet_Config, false);

    return;
}


/*
 * ===========================================================================
 *  Interface configuration
 * ===========================================================================
 */

/*
 * Push the configuration from the Wifi_Inet_Config (iconf) to the interface (piface)
 */
bool nm2_inet_config_set(struct nm2_iface *piface, struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;

    retval &= nm2_inet_interface_set(piface, iconf);
    retval &= nm2_inet_igmp_set(piface, iconf);
    retval &= nm2_inet_ip_assign_scheme_set(piface, iconf);
    retval &= nm2_inet_upnp_set(piface, iconf);
    retval &= nm2_inet_static_set(piface, iconf);
    retval &= nm2_inet_dns_set(piface, iconf);
    retval &= nm2_inet_dhcps_set(piface, iconf);
    retval &= nm2_inet_ip4tunnel_set(piface, iconf);
    retval &= nm2_inet_dhsnif_set(piface, iconf);
    retval &= nm2_inet_vlan_set(piface, iconf);

    nm2_inet_copy(piface, iconf);

    /*
     * Send notification about a configuration change regardless of the status in retval -- it might
     * have been a partially applied so a status change is warranted
     */
    piface->if_state_notify = true;

    return retval;

}

/* Apply general interface settings from schema */
bool nm2_inet_interface_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;

    if (!inet_interface_enable(piface->if_inet, iconf->enabled))
    {
        LOG(WARN, "inet_config: %s (%s): Error enabling interface (%d).",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->enabled);

        retval = false;
    }

    if (!inet_network_enable(piface->if_inet, iconf->network))
    {
        LOG(WARN, "inet_config: %s (%s): Error enabling network (%d).",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->network);

        retval = false;
    }

    if (iconf->mtu_exists && !inet_mtu_set(piface->if_inet, iconf->mtu))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting MTU %d.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->mtu);

        retval = false;
    }

    if (!inet_nat_enable(piface->if_inet, iconf->NAT_exists && iconf->NAT))
    {
        LOG(WARN, "inet_config: %s (%s): Error enabling NAT (%d).",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->NAT_exists && iconf->NAT);

        retval = false;
    }

    return retval;
}


/* Apply IGMP configuration from schema */
bool nm2_inet_igmp_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    if (piface->if_type != NM2_IFTYPE_BRIDGE)
    {
        return true;
    }

    int iigmp = iconf->igmp_exists ? iconf->igmp : false;
    int iage = iconf->igmp_age_exists ? iconf->igmp_age : 300;
    int itsize = iconf->igmp_tsize_present ? iconf->igmp_tsize: 1024;

    if (!inet_igmp_enable(piface->if_inet, iigmp, iage, itsize))
    {
        LOG(WARN, "inet_config: %s (%s): Error enabling IGMP (%d).",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->igmp_exists && iconf->igmp);

        return false;
    }

    return true;
}

/* Apply IP assignment scheme from OVSDB schema */
bool nm2_inet_ip_assign_scheme_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    enum inet_assign_scheme assign_scheme = INET_ASSIGN_NONE;

    if (iconf->ip_assign_scheme_exists)
    {
        if (strcmp(iconf->ip_assign_scheme, "static") == 0)
        {
            assign_scheme = INET_ASSIGN_STATIC;
        }
        else if (strcmp(iconf->ip_assign_scheme, "dhcp") == 0)
        {
            assign_scheme = INET_ASSIGN_DHCP;
        }
    }

    if (!inet_assign_scheme_set(piface->if_inet, assign_scheme))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting the IP assignment scheme: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->ip_assign_scheme);

        return false;
    }

    return true;
}

/* Apply UPnP settings from schema */
bool nm2_inet_upnp_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    enum osn_upnp_mode upnp = UPNP_MODE_NONE;

    if (iconf->upnp_mode_exists)
    {
        if (strcmp(iconf->upnp_mode, "disabled") == 0)
        {
            upnp = UPNP_MODE_NONE;
        }
        else if (strcmp(iconf->upnp_mode, "internal") == 0)
        {
            upnp = UPNP_MODE_INTERNAL;
        }
        else if (strcmp(iconf->upnp_mode, "external") == 0)
        {
            upnp = UPNP_MODE_EXTERNAL;
        }
        else
        {
            LOG(WARN, "inet_config: %s (%s): Unknown UPnP mode %s. Assuming \"disabled\".",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->upnp_mode);
        }
    }

    if (!inet_upnp_mode_set(piface->if_inet, upnp))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting UPnP mode: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->upnp_mode);

        return false;
    }

    return true;
}

/* Apply static IP configuration from schema */
bool nm2_inet_static_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;

    osn_ip_addr_t ipaddr = OSN_IP_ADDR_INIT;
    osn_ip_addr_t netmask = OSN_IP_ADDR_INIT;
    osn_ip_addr_t bcaddr = OSN_IP_ADDR_INIT;

    if (iconf->inet_addr_exists && !osn_ip_addr_from_str(&ipaddr, iconf->inet_addr))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid IP address: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->inet_addr);
    }

    if (iconf->netmask_exists && !osn_ip_addr_from_str(&netmask, iconf->netmask))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid netmask: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->netmask);
    }

    if (iconf->broadcast_exists && !osn_ip_addr_from_str(&bcaddr, iconf->broadcast))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid broadcast address: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->netmask);
    }

    if (!inet_ipaddr_static_set(piface->if_inet, ipaddr, netmask, bcaddr))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting the static IP configuration,"
                " ipaddr = "PRI_osn_ip_addr
                " netmask = "PRI_osn_ip_addr
                " bcaddr = "PRI_osn_ip_addr".",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                FMT_osn_ip_addr(ipaddr),
                FMT_osn_ip_addr(netmask),
                FMT_osn_ip_addr(bcaddr));

        retval = false;
    }

    osn_ip_addr_t gateway = OSN_IP_ADDR_INIT;
    if (iconf->gateway_exists && !osn_ip_addr_from_str(&gateway, iconf->gateway))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid broadcast address: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->netmask);
    }

    if (!inet_gateway_set(piface->if_inet, gateway))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting the default gateway "PRI_osn_ip_addr".",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                FMT_osn_ip_addr(gateway));

        retval = false;
    }

    return retval;
}

/* Apply DNS settings from schema */
bool nm2_inet_dns_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    osn_ip_addr_t primary_dns = OSN_IP_ADDR_INIT;
    osn_ip_addr_t secondary_dns = OSN_IP_ADDR_INIT;

    const char *sprimary = SCHEMA_FIND_KEY(iconf->dns, "primary");
    const char *ssecondary = SCHEMA_FIND_KEY(iconf->dns, "secondary");

    if (sprimary != NULL &&
            !osn_ip_addr_from_str(&primary_dns, sprimary))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid primary DNS: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                sprimary);
    }

    if (ssecondary != NULL &&
            !osn_ip_addr_from_str(&secondary_dns, ssecondary))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid secondary DNS: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                ssecondary);
    }

    if (!inet_dns_set(piface->if_inet, primary_dns, secondary_dns))
    {
        LOG(WARN, "inet_config: %s (%s): Error applying DNS settings.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));

        return false;
    }

    return true;
}

/* Apply DHCP server configuration from schema */
bool nm2_inet_dhcps_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    osn_ip_addr_t lease_start = OSN_IP_ADDR_INIT;
    osn_ip_addr_t lease_stop = OSN_IP_ADDR_INIT;
    int lease_time = -1;
    bool retval = true;

    const char *slease_time = SCHEMA_FIND_KEY(iconf->dhcpd, "lease_time");
    const char *slease_start = SCHEMA_FIND_KEY(iconf->dhcpd, "start");
    const char *slease_stop = SCHEMA_FIND_KEY(iconf->dhcpd, "stop");
    const char *sdhcp_opts = SCHEMA_FIND_KEY(iconf->dhcpd, "dhcp_option");

    /*
     * Parse the lease range, start/stop fields are just IPs
     */
    if (slease_start != NULL)
    {
        if (!osn_ip_addr_from_str(&lease_start, slease_start))
        {
            LOG(WARN, "inet_config: %s (%s): Error parsing IP address lease_start: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    slease_start);
            retval = false;
        }
    }

    /* Lease stop options */
    if (slease_stop != NULL)
    {
        if (!osn_ip_addr_from_str(&lease_stop, slease_stop))
        {
            LOG(WARN, "inet_config: %s (%s): Error parsing IP address lease_stop: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    slease_stop);
            retval = false;
        }
    }

    /*
     * Parse the lease time, the lease time may have a suffix denoting the units of time -- h for hours, m for minutes, s for seconds
     */
    if (slease_time != NULL)
    {
        char *suffix;
        /* The lease time is usually expressed with a suffix, for example 12h for hours. */
        lease_time = strtoul(slease_time, &suffix, 10);

        /* If the amount of characters converted is 0, then it's an error */
        if (suffix == slease_time)
        {
            LOG(WARN, "inet_config: %s (%s): Error parsing lease time: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    slease_time);
        }
        else
        {
            switch (*suffix)
            {
                case 'h':
                case 'H':
                    lease_time *= 60;
                    /* fall through */

                case 'm':
                case 'M':
                    lease_time *= 60;
                    /* fall through */

                case 's':
                case 'S':
                case '\0':
                    break;

                default:
                    LOG(WARN, "inet_config: %s (%s): Error parsing lease time -- invalid time suffix: %s",
                            piface->if_name,
                            nm2_iftype_tostr(piface->if_type),
                            slease_time);
                    lease_time = -1;
            }
        }
    }

    /*
     * Parse options -- options are a ';' separated list of comma separated key,value pairs
     */
    if (sdhcp_opts != NULL && !nm2_inet_dhcps_options_set(piface, sdhcp_opts))
    {
        LOG(ERR, "inet_config: %s (%s): Error parsing DHCP server options: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                sdhcp_opts);
        retval = false;
    }

    if (!inet_dhcps_range_set(piface->if_inet, lease_start, lease_stop))
    {
        LOG(ERR, "inet_config: %s (%s): Error setting DHCP range.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
        retval = false;
    }

    if (!inet_dhcps_lease_set(piface->if_inet, lease_time))
    {
        LOG(ERR, "inet_config: %s (%s): Error setting DHCP lease time: %d",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                lease_time);
        retval = false;
    }

    bool enable = slease_start != NULL && slease_stop != NULL;

    /* Enable DHCP lease notifications */
    if (!inet_dhcps_lease_notify(piface->if_inet, nm2_dhcp_lease_notify))
    {
        LOG(ERR, "inet_config: %s (%s): Error registering DHCP lease event handler.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
        retval = false;
    }

    /* Start the DHCP server */
    if (!inet_dhcps_enable(piface->if_inet, enable))
    {
        LOG(ERR, "inet_config: %s (%s): Error %s DHCP server.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                enable ? "enabling" : "disabling");
        retval = false;
    }

    return retval;
}

/* Apply DHCP server options from schema MAP */
bool nm2_inet_dhcps_options_set(struct nm2_iface *piface, const char *opts)
{
    int ii;

    char *topts = strdup(opts);

    /* opt_track keeps track of options that were set -- we must unset all other options at the end */
    uint8_t opt_track[C_SET_LEN(DHCP_OPTION_MAX, uint8_t)];
    memset(opt_track, 0, sizeof(opt_track));

    /* First, split by semi colons */
    char *psemi = topts;
    char *csemi;

    while ((csemi = strsep(&psemi, ";")) != NULL)
    {
        char *pcomma = csemi;

        /* Split by comma */
        char *sopt = strsep(&pcomma, ",");
        char *sval = pcomma;

        int opt = strtoul(sopt, NULL, 0);
        if (opt <= 0 || opt >= DHCP_OPTION_MAX)
        {
            LOG(WARN, "inet_config: %s (%s): Invalid DHCP option specified: %s. Ignoring.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    sopt);
            continue;
        }

        C_SET_ADD(opt_track, opt);

        if (!inet_dhcps_option_set(piface->if_inet, opt, sval))
        {
            LOG(WARN, "inet_config: %s (%s): Error setting DHCP option %d.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    opt);
        }
    }

    /* Clear all other options */
    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (C_IS_SET(opt_track, ii)) continue;

        if (!inet_dhcps_option_set(piface->if_inet, ii, NULL))
        {
            LOG(WARN, "inet_config: %s (%s): Error clearing DHCP option %d.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    ii);
        }
    }

    if (topts != NULL) free(topts);

    return true;
}

/* Apply IPv4 tunnel settings */
bool nm2_inet_ip4tunnel_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;
    const char *ifparent = NULL;
    osn_ip_addr_t local_addr = OSN_IP_ADDR_INIT;
    osn_ip_addr_t remote_addr = OSN_IP_ADDR_INIT;
    osn_mac_addr_t remote_mac = OSN_MAC_ADDR_INIT;

    if (piface->if_type != NM2_IFTYPE_GRE) return retval;

    if (iconf->gre_ifname_exists)
    {
        if (iconf->gre_ifname[0] == '\0')
        {
            LOG(WARN, "inet_config: %s (%s): Parent interface exists, but is empty.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type));
            retval = false;
        }

        ifparent = iconf->gre_ifname;
    }

    if (iconf->gre_local_inet_addr_exists)
    {
        if (!osn_ip_addr_from_str(&local_addr, iconf->gre_local_inet_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Local IPv4 address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_local_inet_addr);

            retval = false;
        }
    }

    if (iconf->gre_remote_inet_addr_exists)
    {
        if (!osn_ip_addr_from_str(&remote_addr, iconf->gre_remote_inet_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Remote IPv4 address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_remote_inet_addr);

            retval = false;
        }
    }

    if (iconf->gre_remote_mac_addr_exists)
    {
        if (!osn_mac_addr_from_str(&remote_mac, iconf->gre_remote_mac_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Remote MAC address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_remote_mac_addr);

            retval = false;
        }
    }

    if (!inet_ip4tunnel_set(piface->if_inet, ifparent, local_addr, remote_addr, remote_mac))
    {
        LOG(ERR, "inet_config: %s (%s): Error setting IPv4 tunnel settings: parent=%s local="PRI_osn_ip_addr" remote="PRI_osn_ip_addr" remote_mac="PRI_osn_mac_addr,
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                ifparent,
                FMT_osn_ip_addr(local_addr),
                FMT_osn_ip_addr(remote_addr),
                FMT_osn_mac_addr(remote_mac));

        retval = false;
    }

    return retval;
}

/* Apply DHCP sniffing configuration from schema */
bool nm2_inet_dhsnif_set(struct nm2_iface *piface, const struct schema_Wifi_Inet_Config *iconf)
{
    /*
     * Enable or disable the DHCP sniffing callback according to schema values
     */
    if (iconf->dhcp_sniff_exists && iconf->dhcp_sniff)
    {
        return inet_dhsnif_lease_notify(piface->if_inet, nm2_dhcp_lease_notify);
    }

    return inet_dhsnif_lease_notify(piface->if_inet, NULL);
}

/* Apply VLAN configuration from schema */
bool nm2_inet_vlan_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    const char *ifparent = NULL;
    int vlanid = 0;

    /* Not supported for non-VLAN types */
    if (piface->if_type != NM2_IFTYPE_VLAN) return true;

    if (iconf->vlan_id_exists)
    {
        vlanid = iconf->vlan_id;
    }

    if (iconf->parent_ifname_exists)
    {
        ifparent = iconf->parent_ifname;
    }

    return inet_vlan_set(piface->if_inet, ifparent, vlanid);
}

/*
 * Some fields must be cached for later retrieval; these are typically used
 * for populating the Wifi_Inet_State and Wifi_Master_State tables.
 *
 * This function is responsible for copying these fields from the schema
 * structure to nm2_iface.
 */
void nm2_inet_copy(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    /* Copy fields that should be simply copied to Wifi_Inet_State */
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.if_type, iconf->if_type);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache._uuid, iconf->_uuid);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.if_uuid, iconf->if_uuid);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.dns, iconf->dns);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.dns_keys, iconf->dns_keys);
    piface->if_cache.dns_len = iconf->dns_len;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.dhcpd, iconf->dhcpd);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.dhcpd_keys, iconf->dhcpd_keys);
    piface->if_cache.dhcpd_len = iconf->dhcpd_len;
    piface->if_cache.gre_ifname_exists = iconf->gre_ifname_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_ifname, iconf->gre_ifname);
    piface->if_cache.gre_ifname_exists = iconf->gre_ifname_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_ifname, iconf->gre_ifname);
    piface->if_cache.gre_remote_inet_addr_exists = iconf->gre_remote_inet_addr_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_remote_inet_addr, iconf->gre_remote_inet_addr);
    piface->if_cache.gre_local_inet_addr_exists = iconf->gre_local_inet_addr_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_local_inet_addr, iconf->gre_local_inet_addr);
}


struct nm2_iface *nm2_add_inet_conf(struct schema_Wifi_Inet_Config *iconf)
{
    enum nm2_iftype iftype;
    struct nm2_iface *piface = NULL;


    LOG(INFO, "nm2_add_inet_conf: INSERT interface %s (type %s).",
            iconf->if_name,
            iconf->if_type);


    if (!nm2_iftype_fromstr(&iftype, iconf->if_type))
    {
        LOG(ERR, "nm2_add_inet_conf: %s: Unknown interface type %s. Unable to create interface.",
                iconf->if_name,
                iconf->if_type);
        return NULL;
    }

    piface = nm2_iface_new(iconf->if_name, iftype);
    if (piface == NULL)
    {
        LOG(ERR, "nm2_add_inet_conf: %s (%s): Unable to create interface.",
                iconf->if_name,
                iconf->if_type);
        return NULL;
    }

    return piface;
}

void nm2_del_inet_conf(struct schema_Wifi_Inet_Config *old_rec)
{
    struct nm2_iface *piface = NULL;

    LOG(INFO, "nm2_del_inet_conf: interface %s (type %s).", old_rec->if_name, old_rec->if_type);

    piface = nm2_iface_get_by_name(old_rec->if_name);
    if (piface == NULL)
    {
        LOG(ERR, "nm2_del_inet_conf: Unable to delete non-existent interface %s.", old_rec->if_name);
    }

    if (piface != NULL && !nm2_iface_del(piface))
    {
        LOG(ERR, "nm2_del_inet_conf: Error during destruction of interface %s.", old_rec->if_name);
    }

    nm2_inet_state_del(old_rec->if_name);

    return;
}

struct nm2_iface *nm2_modify_inet_conf(struct schema_Wifi_Inet_Config *iconf)
{
    enum nm2_iftype iftype;
    struct nm2_iface *piface = NULL;

    LOG(INFO, "nm2_modify_inet_conf: UPDATE interface %s (type %s).",
            iconf->if_name,
            iconf->if_type);


    if (!nm2_iftype_fromstr(&iftype, iconf->if_type))
    {
        LOG(ERR, "nm2_modify_inet_conf: %s: Unknown interface type %s. Unable to create interface.",
                iconf->if_name,
                iconf->if_type);
        return NULL;
    }

    piface = nm2_iface_get_by_name(iconf->if_name);
    if (piface == NULL)
    {
        LOG(ERR, "nm2_modify_inet_conf: %s (%s): Unable to modify interface, didn't find it.",
        iconf->if_name,
        iconf->if_type);
    }

    return piface;
}


/*
 * OVSDB Wifi_Inet_Config table update handler.
 */
void callback_Wifi_Inet_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Inet_Config *old_rec,
        struct schema_Wifi_Inet_Config *iconf)
{
    struct nm2_iface *piface = NULL;

    TRACE();

    switch(mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            piface = nm2_add_inet_conf(iconf);
            break;
        case OVSDB_UPDATE_DEL:
            nm2_del_inet_conf(old_rec);
            break;
        case OVSDB_UPDATE_MODIFY:
            piface = nm2_modify_inet_conf(iconf);
            break;
        default:
            LOG(ERR, "Invalid Wifi_Inet_Config mon_type(%d)", mon->mon_type);
    }

    if (mon->mon_type == OVSDB_UPDATE_NEW ||
        mon->mon_type == OVSDB_UPDATE_MODIFY)
    {

        if(!piface)
        {
            LOG(ERR, "callback_Wifi_Inet_Config: Couldn't get the interface(%s)",
            iconf->if_name);
            return;
        }

        /* Push the configuration to the interface */
        if (!nm2_inet_config_set(piface, iconf))
        {
            LOG(ERR, "callback_Wifi_Inet_Config: %s (%s): Unable to set configuration",
                    iconf->if_name,
                    iconf->if_type);
        }

        /* Apply the configuration */
        if (!nm2_iface_apply(piface))
        {
            LOG(ERR, "callback_Wifi_Inet_Config: %s (%s): Unable to apply configuration.",
                    iconf->if_name,
                    iconf->if_type);
        }
    }

    return;
}

/*
 * ===========================================================================
 *  MISC
 * ===========================================================================
 */

/**
 * Lookup a value by key in a schema map structure.
 *
 * A schema map structures is defined as follows:
 *
 * char name[NAME_SZ][LEN]      - value array
 * char name_keys[KEY_SZ][LEN]  - key array
 * char name_len;               - length of name and name_keys arrays
 */
const char * __find_key(char *keyv, size_t keysz, char *datav, size_t datasz, int vlen, const char *key)
{
    int ii;

    for (ii = 0; ii < vlen; ii++)
    {
        if (strcmp(keyv, key) == 0) return datav;

        keyv += keysz;
        datav += datasz;
    }

    return NULL;
}

