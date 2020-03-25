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
 * ===========================================================================
 *  DHCP IP reservation halding.
 * ===========================================================================
 */
#include "ds_tree.h"
#include "log.h"
#include "schema.h"
#include "json_util.h"
#include "ovsdb_update.h"

#include "nm2.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

static ovsdb_update_monitor_t dhcp_rip_monitor;
static ovsdb_update_cbk_t nm2_dhcp_rip_monitor_fn;
static bool nm2_dhcp_rip_set(struct schema_DHCP_reserved_IP *schema_rip);
static bool nm2_dhcp_rip_del(struct schema_DHCP_reserved_IP *schema_rip);

bool nm2_dhcp_rip_init(void)
{
    bool rc;

    /* Register for OVS monitor updates. */
    rc = ovsdb_update_monitor(
            &dhcp_rip_monitor,
            nm2_dhcp_rip_monitor_fn,
            SCHEMA_TABLE(DHCP_reserved_IP),
            OMT_ALL);
    if (!rc)
    {
        LOG(ERR, "dhcp_rip: Error initializing DHCP_reserved_IP monitor");
        return false;
    }
    return true;
}

/* Update Monitor callback for the DHCP_reserved_IP table. */
void nm2_dhcp_rip_monitor_fn(ovsdb_update_monitor_t *self)
{
    struct schema_DHCP_reserved_IP schema_rip;
    pjs_errmsg_t perr;

    memset(&schema_rip, 0, sizeof(schema_rip));

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:
            LOG(INFO, "dhcp_rip: DHCP_reserved_IP new/modify(%d): %s",
                    self->mon_type == OVSDB_UPDATE_NEW,
                    json_dumps_static(self->mon_json_new, 0));

            if (!schema_DHCP_reserved_IP_from_json(&schema_rip, self->mon_json_new, false, perr))
            {
                LOG(ERR, "dhcp_rip: DHCP_reserved_IP row: Error parsing: %s", perr);
                return;
            }

            nm2_dhcp_rip_set(&schema_rip);
            break;

        case OVSDB_UPDATE_DEL:
            LOG(INFO, "dhcp_rip: DHCP_reserved_IP delete: %s", json_dumps_static(self->mon_json_new, 0));

            if (!schema_DHCP_reserved_IP_from_json(&schema_rip, self->mon_json_old, false, perr))
            {
                LOG(ERR, "dhcp_rip: DHCP_reserved_IP: Error parsing: %s", perr);
                return;
            }

            nm2_dhcp_rip_del(&schema_rip);
            break;

        default:
            LOG(ERR, "dhcp_rip: DHCP_reserved_IP: Unhandled update notification type %d for UUID %s",
                     self->mon_type, self->mon_uuid);
            return;
    }
}

bool nm2_dhcp_rip_set(struct schema_DHCP_reserved_IP *schema_rip)
{
    osn_ip_addr_t ip4addr;
    osn_mac_addr_t macaddr;
    struct nm2_iface *piface;

    if (!osn_ip_addr_from_str(&ip4addr, schema_rip->ip_addr))
    {
        LOG(ERR, "dhcp_rip: Invalid IP address: %s (set). ", schema_rip->ip_addr);
        return false;
    }

    if (!osn_mac_addr_from_str(&macaddr, schema_rip->hw_addr))
    {
        LOG(ERR, "dhcp_rip: Invalid MAC address: %s (set).", schema_rip->hw_addr);
        return false;
    }

    /*
     * The DHCP_reserved_IP table doesn't contain the interface name. We must lookup the
     * interface instance using the DHCP reserveation IP address. We assume that the
     * client IP address matches the interface subnet.
     */
    piface = nm2_iface_find_by_ipv4(ip4addr);
    if (piface == NULL)
    {
        LOG(ERR, "dhcp_rip: Unable to find interface configuration with subnet: ip=%s mac=%s (set)",
                schema_rip->ip_addr,
                schema_rip->hw_addr);
        return false;
    }

    /* Push new configuration */
    if (!inet_dhcps_rip_set(
                piface->if_inet,
                macaddr,
                ip4addr,
                schema_rip->hostname_exists ? schema_rip->hostname : NULL))
    {
        LOG(ERR, "dhcp_rip: Error processing DHCP reservation: ip=%s mac=%s",
                schema_rip->ip_addr,
                schema_rip->hw_addr);
        return false;
    }

    /* Apply configuration */
    if (!nm2_iface_apply(piface))
    {
        LOG(ERR, "dhcp_rip: Unable to apply configuration (set).");
    }

    return true;
}

bool nm2_dhcp_rip_del(struct schema_DHCP_reserved_IP *schema_rip)
{
    osn_ip_addr_t ip4addr;
    osn_mac_addr_t macaddr;
    struct nm2_iface *piface;

    if (!osn_ip_addr_from_str(&ip4addr, schema_rip->ip_addr))
    {
        LOG(ERR, "dhcp_rip: Invalid IP address: %s (delete). ", schema_rip->ip_addr);
        return false;
    }

    if (!osn_mac_addr_from_str(&macaddr, schema_rip->hw_addr))
    {
        LOG(ERR, "dhcp_rip: Invalid MAC address: %s (delete).", schema_rip->hw_addr);
        return false;
    }

    /* See nm2_dhcp_rip_set() for and explanation on why we're doing an IP lookup */
    piface = nm2_iface_find_by_ipv4(ip4addr);
    if (piface == NULL)
    {
        LOG(ERR, "dhcp_rip: Unable to find interface configuration with subnet: ip=%s mac=%s (delete).",
                schema_rip->ip_addr,
                schema_rip->hw_addr);
        return false;
    }

    /* Remove IP reservation:  */
    if (!inet_dhcps_rip_del(piface->if_inet, macaddr))
    {
        LOG(ERR, "dhcp_rip: %s (%s): Error removing DHCP reserved IP.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
        return false;
    }

    /* Apply configuration */
    if (!nm2_iface_apply(piface))
    {
        LOG(ERR, "dhcp_rip: Unable to apply configuration (delete).");
    }

    return true;
}
