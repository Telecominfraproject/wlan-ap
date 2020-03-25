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

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>

#include <string.h>

#include "log.h"
#include "util.h"

#include "inet_unit.h"

#include "inet.h"
#include "inet_base.h"
#include "inet_gretap.h"

#include "execsh.h"

#if !defined(CONFIG_USE_KCONFIG) && !defined(CONFIG_INET_GRE_USE_SOFTWDS)
/* Default to gretap */
#define CONFIG_INET_GRE_USE_GRETAP
#endif

#if defined(CONFIG_INET_GRE_USE_GRETAP)
/*
 * inet_gretap_t was selected as the default tunnelling
 * implementation -- return an instance with inet_gretap_new()
 */
inet_t *inet_gre_new(const char *ifname)
{
    static bool once = true;

    if (once)
    {
        LOG(NOTICE, "inet_gretap: Using Linux GRETAP implementation.");
        once = false;
    }

    return inet_gretap_new(ifname);
}
#endif

/*
 * ===========================================================================
 *  Initialization
 * ===========================================================================
 */
inet_t *inet_gretap_new(const char *ifname)
{
    inet_gretap_t *self = NULL;

    self = malloc(sizeof(*self));
    if (self == NULL)
    {
        goto error;
    }

    if (!inet_gretap_init(self, ifname))
    {
        LOG(ERR, "inet_vif: %s: Failed to initialize interface instance.", ifname);
        goto error;
    }

    return (inet_t *)self;

 error:
    if (self != NULL) free(self);
    return NULL;
}

bool inet_gretap_init(inet_gretap_t *self, const char *ifname)
{
    if (!inet_eth_init(&self->eth, ifname))
    {
        LOG(ERR, "inet_gretap: %s: Failed to instantiate class, inet_eth_init() failed.", ifname);
        return false;
    }

    self->in_local_addr = OSN_IP_ADDR_INIT;
    self->in_remote_addr = OSN_IP_ADDR_INIT;

    self->inet.in_ip4tunnel_set_fn = inet_gretap_ip4tunnel_set;
    self->base.in_service_commit_fn = inet_gretap_service_commit;

    return true;
}

/*
 * ===========================================================================
 *  IPv4 Tunnel functions
 * ===========================================================================
 */
bool inet_gretap_ip4tunnel_set(
        inet_t *super,
        const char *parent,
        osn_ip_addr_t laddr,
        osn_ip_addr_t raddr,
        osn_mac_addr_t rmac)
{
    (void)rmac; /* Unused */

    inet_gretap_t *self = (inet_gretap_t *)super;

    if (parent == NULL) parent = "";

    if (strcmp(parent, self->in_ifparent) == 0 &&
            osn_ip_addr_cmp(&self->in_local_addr, &laddr) == 0 &&
            osn_ip_addr_cmp(&self->in_remote_addr, &raddr) == 0)
    {
        return true;
    }

    if (strscpy(self->in_ifparent, parent, sizeof(self->in_ifparent)) < 0)
    {
        LOG(ERR, "inet_gretap: %s: Parent interface name too long: %s.",
                self->inet.in_ifname,
                parent);
        return false;
    }

    self->in_local_addr = laddr;
    self->in_remote_addr = raddr;

    /* Interface must be recreated, therefore restart the top service */
    return inet_unit_restart(self->base.in_units, INET_BASE_INTERFACE, false);
}

/*
 * ===========================================================================
 *  Commit and start/stop services
 * ===========================================================================
 */

/*
 * $1 - interface name
 * $2 - parent interface name
 * $3 - local address
 * $4 - remote address
 */
static char gre_create_gretap[] =
_S(
    if [ -e "/sys/class/net/$1" ];
    then
        ip link del "$1";
    fi;
    ip link add "$1" type gretap local "$3" remote "$4" dev "$2" tos 1;
#ifdef WAR_GRE_MAC
    /* Set the same MAC address for GRE as WiFI STA and enable locally administered bit */
    [ -z "$(echo $1 | grep g-)" ] || ( addr="$(cat /sys/class/net/$2/address)" && a="$(echo $addr | cut -d : -f1)" && b="$(echo $addr | cut -d : -f2-)" && c=$(( 0x$a & 2 )) && [ $c -eq 0 ] && c=$(( 0x$a | 2 )) && d=$(printf "%x" $c) && ifconfig "$1" hw ether "$d:$b";)
#endif
);

/*
 * $1 - interface name, always return success
 */
static char gre_delete_gretap[] =
_S(
    [ ! -e "/sys/class/net/$1" ] && exit 0;
    ip link del "$1"
);


/**
 * Create/destroy the GRETAP interface
 */
bool inet_gretap_interface_start(inet_gretap_t *self, bool enable)
{
    int status;
    char slocal_addr[C_IP4ADDR_LEN];
    char sremote_addr[C_IP4ADDR_LEN];

    if (enable)
    {
        if (self->in_ifparent[0] == '\0')
        {
            LOG(INFO, "inet_gretap: %s: No parent interface was specified.", self->inet.in_ifname);
            return false;
        }

        if (osn_ip_addr_cmp(&self->in_local_addr, &OSN_IP_ADDR_INIT) == 0)
        {
            LOG(INFO, "inet_gretap: %s: No local address was specified.", self->inet.in_ifname);
            return false;
        }

        if (osn_ip_addr_cmp(&self->in_remote_addr, &OSN_IP_ADDR_INIT) == 0)
        {
            LOG(INFO, "inet_gretap: %s: No remote address was specified.", self->inet.in_ifname);
            return false;
        }

        snprintf(slocal_addr, sizeof(slocal_addr), PRI_osn_ip_addr, FMT_osn_ip_addr(self->in_local_addr));
        snprintf(sremote_addr, sizeof(sremote_addr), PRI_osn_ip_addr, FMT_osn_ip_addr(self->in_remote_addr));

        status = execsh_log(LOG_SEVERITY_INFO, gre_create_gretap,
                self->inet.in_ifname,
                self->in_ifparent,
                slocal_addr,
                sremote_addr);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(ERR, "inet_gretap: %s: Error creating GRETAP interface.", self->inet.in_ifname);
            return false;
        }

        LOG(INFO, "inet_gretap: %s: GRETAP interface was successfully created.", self->inet.in_ifname);
    }
    else
    {
        status = execsh_log(LOG_SEVERITY_INFO, gre_delete_gretap, self->inet.in_ifname);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(ERR, "inet_gretap: %s: Error deleting GRETAP interface.", self->inet.in_ifname);
        }

        LOG(INFO, "inet_gretap: %s: GRETAP interface was successfully deleted.", self->inet.in_ifname);
    }

    return true;
}

bool inet_gretap_service_commit(inet_base_t *super, enum inet_base_services srv, bool enable)
{
    inet_gretap_t *self = (inet_gretap_t *)super;

    LOG(DEBUG, "inet_gretap: %s: Service %s -> %s.",
            self->inet.in_ifname,
            inet_base_service_str(srv),
            enable ? "start" : "stop");

    switch (srv)
    {
        case INET_BASE_INTERFACE:
            return inet_gretap_interface_start(self, enable);

        default:
            LOG(DEBUG, "inet_eth: %s: Delegating service %s %s to inet_eth.",
                    self->inet.in_ifname,
                    inet_base_service_str(srv),
                    enable ? "start" : "stop");

            /* Delegate everything else to inet_base() */
            return inet_eth_service_commit(super, srv, enable);
    }

    return true;
}

