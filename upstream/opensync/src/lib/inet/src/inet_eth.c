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

#include "inet.h"
#include "inet_base.h"
#include "inet_eth.h"

#include "execsh.h"

/*
 * ===========================================================================
 *  Inet Ethernet implementation for Linux
 * ===========================================================================
 */
static int ifreq_socket(void);
static bool ifreq_exists(const char *ifname, bool *exists);
static bool ifreq_mtu_set(const char *ifname, int mtu);
static bool ifreq_mtu_get(const char *ifname, int *mtu);
static bool ifreq_status_set(const char *ifname, bool up);
static bool ifreq_status_get(const char *ifname, bool *up);
static bool ifreq_running_get(const char *ifname, bool *up);
//static bool ifreq_ipaddr_set(const char *ifname, osn_ip_addr_t ipaddr);
static bool ifreq_ipaddr_get(const char *ifname, osn_ip_addr_t *ipaddr);
//static bool ifreq_netmask_set(const char *ifname, osn_ip_addr_t netmask);
static bool ifreq_netmask_get(const char *ifname, osn_ip_addr_t *netmask);
//static bool ifreq_bcaddr_set(const char *ifname, osn_ip_addr_t bcaddr);
static bool ifreq_bcaddr_get(const char *ifname, osn_ip_addr_t *bcaddr);
static bool ifreq_hwaddr_get(const char *ifname, osn_mac_addr_t *macaddr);

//static char eth_ip_route_add_default[] = _S(ip route add default via "$2" dev "$1" metric 100);
//static char eth_ip_route_del_default[] = _S(ip route del default dev "$1" 2> /dev/null || true);

/*
 * ===========================================================================
 *  Constructors/Destructors
 * ===========================================================================
 */

/**
 * New-type constructor
 */
inet_t *inet_eth_new(const char *ifname)
{
    inet_eth_t *self;

    self = calloc(1, sizeof(*self));
    if (self == NULL)
    {
        goto error;
    }

    if (!inet_eth_init(self, ifname))
    {
        LOG(ERR, "inet_eth: %s: Failed to initialize interface instance.", ifname);
        goto error;
    }

    return (inet_t *)self;

 error:
    if (self != NULL) free(self);
    return NULL;
}

bool inet_eth_init(inet_eth_t *self, const char *ifname)
{
    if (!inet_base_init(&self->base, ifname))
    {
        LOG(ERR, "inet_eth: %s: Failed to instantiate class, inet_base_init() failed.", ifname);
        return false;
    }

    self->in_ip = NULL;

    /* Override methods */
    self->inet.in_state_get_fn = inet_eth_state_get;
    self->base.in_service_commit_fn = inet_eth_service_commit;

    /* Verify that we can create the IFREQ socket */
    if (ifreq_socket() < 0)
    {
        LOG(ERR, "inet_eth: %s: Failed to create IFREQ socket.", ifname);
        return false;
    }

    return true;
}

/*
 * ===========================================================================
 *  Commit and service start & stop functions
 * ===========================================================================
 */
bool inet_eth_interface_start(inet_eth_t *self, bool enable)
{
    (void)enable;

    bool exists;
    bool retval;

    /*
     * Just check if the interface exists -- report an error otherwise so no
     * other services will be started.
     */
    retval = ifreq_exists(self->inet.in_ifname, &exists);
    if (!retval || !exists)
    {
        LOG(NOTICE, "inet_eth: %s: Interface does not exists. Stopping.",
                self->inet.in_ifname);

        return false;
    }

    return true;
}

/**
 * In case of inet_eth a "network start" roughly translates to a ifconfig up
 * Likewise, a "network stop" is basically an ifconfig down.
 */
bool inet_eth_network_start(inet_eth_t *self, bool enable)
{
    bool exists;

    /* Silence compiler errors */
    (void)ifreq_status_get;

    ifreq_exists(self->inet.in_ifname, &exists);

    if (exists && !ifreq_status_set(self->inet.in_ifname, enable))
    {
        LOG(ERR, "inet_eth: %s: Error %s network.",
                self->inet.in_ifname,
                enable ? "enabling" : "disabling");
        return false;
    }

    LOG(INFO, "inet_eth: %s: Network %s.",
            self->inet.in_ifname,
            enable ? "enabled" : "disabled");

    return true;
}

/**
 * IP Assignment of NONE means that the interface must be UP, but should not have an address.
 * Just clear the IP to 0.0.0.0/0, it doesn't matter if its a stop or start event.
 */
bool inet_eth_scheme_none_start(inet_eth_t *self, bool enable)
{
    /*
     * Remove previous IP configuration, if any
     */
    if (self->in_ip != NULL)
    {
        osn_ip_del(self->in_ip);
        self->in_ip = NULL;
    }

    if (!enable) return true;

    /*
     * Just apply an empty configuration
     */
    self->in_ip = osn_ip_new(self->inet.in_ifname);
    if (self->in_ip == NULL)
    {
        LOG(ERR, "inet_eth: %s: Error creating IP configuration object.", self->inet.in_ifname);
        return false;
    }

    if (!osn_ip_apply(self->in_ip))
    {
        LOG(ERR, "inet_eth: %s: Error applying IP configuration.", self->inet.in_ifname);
        return false;
    }

    return true;
}

bool inet_eth_scheme_static_start(inet_eth_t *self, bool enable)
{
    /*
     * Remove previous IP configuration, if any
     */
    if (self->in_ip != NULL)
    {
        osn_ip_del(self->in_ip);
        self->in_ip = NULL;
    }

    if (!enable) return true;

    self->in_ip = osn_ip_new(self->inet.in_ifname);
    if (self->in_ip == NULL)
    {
        LOG(ERR, "inet_eth: %s: Error creating IP configuration object.", self->inet.in_ifname);
        return false;
    }

    /*
     * Check if all the necessary configuration is present (ipaddr, netmask and bcast)
     */
    if (osn_ip_addr_cmp(&self->base.in_static_addr, &OSN_IP_ADDR_INIT) == 0)
    {
        LOG(ERR, "inet_eth: %s: ipaddr is missing for static IP assignment scheme.", self->inet.in_ifname);
        return false;
    }

    if (osn_ip_addr_cmp(&self->base.in_static_netmask, &OSN_IP_ADDR_INIT) == 0)
    {
        LOG(ERR, "inet_eth: %s: netmask is missing for static IP assignment scheme.", self->inet.in_ifname);
        return false;
    }

    /* Construct full osn_ip_addr_t object from the IP address and Netmask */
    osn_ip_addr_t ip;

    ip = self->base.in_static_addr;
    /* Calculate the prefix from the netmask */
    ip.ia_prefix = osn_ip_addr_to_prefix(&self->base.in_static_netmask);

    if (!osn_ip_addr_add(self->in_ip, &ip))
    {
        LOG(ERR, "inet_eth: %s: Cannot assign IP address: "PRI_osn_ip_addr,
                self->inet.in_ifname,
                FMT_osn_ip_addr(ip));
        return false;
    }

    /*
     * Add the primary DNS server
     */
    if (osn_ip_addr_cmp(&self->base.in_dns_primary, &OSN_IP_ADDR_INIT) != 0)
    {
        LOG(TRACE, "inet_eth: %s: Adding primary DNS server: "PRI_osn_ip_addr,
                self->inet.in_ifname,
                FMT_osn_ip_addr(self->base.in_dns_primary));

        if (!osn_ip_dns_add(self->in_ip, &self->base.in_dns_primary))
        {
            LOG(WARN, "inet_eth: %s: Cannot assign primary DNS server: "PRI_osn_ip_addr,
                    self->inet.in_ifname,
                    FMT_osn_ip_addr(ip));
        }
    }

    /*
     * Add the secondary DNS server
     */
    if (osn_ip_addr_cmp(&self->base.in_dns_secondary, &OSN_IP_ADDR_INIT) != 0)
    {
        LOG(TRACE, "inet_eth: %s: Adding secondary DNS server: "PRI_osn_ip_addr,
                self->inet.in_ifname,
                FMT_osn_ip_addr(self->base.in_dns_secondary));

        if (!osn_ip_dns_add(self->in_ip, &self->base.in_dns_secondary))
        {
            LOG(WARN, "inet_eth: %s: Cannot assign secondary DNS server: "PRI_osn_ip_addr,
                    self->inet.in_ifname,
                    FMT_osn_ip_addr(ip));
        }
    }

    /*
     * Use the "ip route" command to add the default route
     */
    if (osn_ip_addr_cmp(&self->base.in_static_gwaddr, &OSN_IP_ADDR_INIT) != 0)
    {
        if (!osn_ip_route_gw_add(self->in_ip, &OSN_IP_ADDR_INIT, &self->base.in_static_gwaddr))
        {
            LOG(ERR, "inet_eth: %s: Error adding default route: "PRI_osn_ip_addr" -> "PRI_osn_ip_addr,
                    self->inet.in_ifname,
                    FMT_osn_ip_addr(OSN_IP_ADDR_INIT),
                    FMT_osn_ip_addr(self->base.in_static_gwaddr));
            return false;
        }
    }

    if (!osn_ip_apply(self->in_ip))
    {
        LOG(ERR, "inet_eth: %s: Error applying IP configuration.", self->inet.in_ifname);
        return false;
    }

    return true;
}

/**
 * MTU Settings
 */
bool inet_eth_mtu_start(inet_eth_t *self, bool enable)
{
    if (!enable) return true;

    /* MTU not set */
    if (self->base.in_mtu <= 0) return true;

    /* IPv6 requires a MTU of 1280 */
    if (self->base.in_mtu < 1280)
    {
        LOG(ERR, "inet_eth: %s: A MTU setting of %d is invalid.",
                self->inet.in_ifname,
                self->base.in_mtu);

        return false;
    }

    LOG(INFO, "inet_eth: %s: Setting MTU to %d.",
            self->inet.in_ifname,
            self->base.in_mtu);

    if (!ifreq_mtu_set(self->inet.in_ifname, self->base.in_mtu))
    {
        LOG(ERR, "inet_eth: %s: Error setting MTU to %d.",
                self->inet.in_ifname,
                self->base.in_mtu);

        return false;
    }

    return true;
}

/**
 * Start/stop event dispatcher
 */
bool inet_eth_service_commit(inet_base_t *super, enum inet_base_services srv, bool enable)
{
    inet_eth_t *self = (inet_eth_t *)super;

    LOG(INFO, "inet_eth: %s: Service %s -> %s.",
            self->inet.in_ifname,
            inet_base_service_str(srv),
            enable ? "start" : "stop");

    switch (srv)
    {
        case INET_BASE_INTERFACE:
            return inet_eth_interface_start(self, enable);

        case INET_BASE_NETWORK:
            return inet_eth_network_start(self, enable);

        case INET_BASE_SCHEME_NONE:
            return inet_eth_scheme_none_start(self, enable);

        case INET_BASE_SCHEME_STATIC:
            return inet_eth_scheme_static_start(self, enable);

        case INET_BASE_MTU:
            return inet_eth_mtu_start(self, enable);

        default:
            LOG(DEBUG, "inet_eth: %s: Delegating service %s %s to inet_base.",
                    self->inet.in_ifname,
                    inet_base_service_str(srv),
                    enable ? "start" : "stop");

            /* Delegate everything else to inet_base() */
            return inet_base_service_commit(super, srv, enable);
    }

    return true;
}

/*
 * ===========================================================================
 *  Status reporting
 * ===========================================================================
 */
bool inet_eth_state_get(inet_t *super, inet_state_t *out)
{
    bool exists;

    inet_eth_t *self = (inet_eth_t *)super;

    if (!inet_base_state_get(&self->inet, out))
    {
        return false;
    }

    if (ifreq_exists(self->inet.in_ifname, &exists) && exists)
    {
        (void)ifreq_mtu_get(self->inet.in_ifname, &out->in_mtu);
        (void)ifreq_ipaddr_get(self->inet.in_ifname, &out->in_ipaddr);
        (void)ifreq_netmask_get(self->inet.in_ifname, &out->in_netmask);
        (void)ifreq_bcaddr_get(self->inet.in_ifname, &out->in_bcaddr);
        (void)ifreq_hwaddr_get(self->inet.in_ifname, &out->in_macaddr);
        (void)ifreq_running_get(self->inet.in_ifname, &out->in_port_status);
    }

    return true;
}

/*
 * ===========================================================================
 *  IFREQ -- interface ioctl() and ifreq requests
 * ===========================================================================
 */

int ifreq_socket(void)
{
    static int ifreq_socket = -1;

    if (ifreq_socket >= 0) return ifreq_socket;

    ifreq_socket = socket(AF_INET, SOCK_DGRAM, 0);

    return ifreq_socket;
}

/**
 * ioctl() wrapper around ifreq
 */
bool ifreq_ioctl(const char *ifname, int cmd, struct ifreq *req)
{
    int s;

    if (strscpy(req->ifr_name, ifname, sizeof(req->ifr_name)) < 0)
    {
        LOG(ERR, "inet_eth: %s: ioctl() failed, interface name too long.", ifname);
        return false;
    }

    s = ifreq_socket();
    if (s < 0)
    {
        LOG(ERR, "inet_eth: %s: Unable to acquire the IFREQ socket: %s",
                ifname,
                strerror(errno));
        return false;
    }

    if (ioctl(s, cmd, (void *)req) < 0)
    {
        return false;
    }

    return true;
}

/**
 * Check if the interface exists
 */
bool ifreq_exists(const char *ifname, bool *exists)
{
    struct ifreq ifr;

    /* First get the current flags */
    if (!ifreq_ioctl(ifname, SIOCGIFINDEX, &ifr))
    {
        *exists = false;
    }
    else
    {
        *exists = true;
    }


    return true;
}

/**
 * Equivalent of ifconfig up/down
 */
bool ifreq_status_set(const char *ifname, bool up)
{
    struct ifreq ifr;

    /* First get the current flags */
    if (!ifreq_ioctl(ifname, SIOCGIFFLAGS, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCGIFFLAGS failed. Error retrieving the interface status: %s",
                ifname,
                strerror(errno));

        return false;
    }

    /* Set or clear IFF_UP depending on the action defined by @p up */
    if (up)
    {
        ifr.ifr_flags |= IFF_UP;
    }
    else
    {
        ifr.ifr_flags &= ~IFF_UP;
    }

    if (!ifreq_ioctl(ifname, SIOCSIFFLAGS, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCSIFFLAGS failed. Error setting the interface status: %s",
                ifname,
                strerror(errno));

        return false;
    }

    return true;
}

/**
 * Get interface status
 */
bool ifreq_status_get(const char *ifname, bool *up)
{
    struct ifreq ifr;

    /* First get the current flags */
    if (!ifreq_ioctl(ifname, SIOCGIFFLAGS, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCGIFFLAGS failed. Error retrieving the interface status: %s",
                ifname,
                strerror(errno));

        return false;
    }

    *up = ifr.ifr_flags & IFF_UP;

    return true;
}

/**
 * Get interface status
 */
bool ifreq_running_get(const char *ifname, bool *up)
{
    struct ifreq ifr;

    /* First get the current flags */
    if (!ifreq_ioctl(ifname, SIOCGIFFLAGS, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCGIFFLAGS failed. Error retrieving the interface running state: %s",
                ifname,
                strerror(errno));
        return false;
    }

    *up = ifr.ifr_flags & IFF_RUNNING;

    return true;
}

/**
 * Set the IP of an interface
 */
bool ifreq_ipaddr_set(const char *ifname, osn_ip_addr_t ipaddr)
{
    struct ifreq ifr;

    ifr.ifr_addr.sa_family = AF_INET;
    memcpy(&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr, &ipaddr, sizeof(ipaddr));
    if (!ifreq_ioctl(ifname, SIOCSIFADDR, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCSIFADDR failed. Error setting the IP address: %s",
                ifname,
                strerror(errno));

        return false;
    }

    return true;
}

/**
 * Get the IP of an interface
 */
bool ifreq_ipaddr_get(const char *ifname, osn_ip_addr_t *ipaddr)
{
    struct ifreq ifr;

    *ipaddr = OSN_IP_ADDR_INIT;

    ifr.ifr_addr.sa_family = AF_INET;

    *ipaddr = OSN_IP_ADDR_INIT;
    if (!ifreq_ioctl(ifname, SIOCGIFADDR, &ifr))
    {
        return false;
    }

    ipaddr->ia_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

    return true;
}


/**
 * Set the Netmask of an interface
 */
bool ifreq_netmask_set(const char *ifname, osn_ip_addr_t netmask)
{
    struct ifreq ifr;

    ifr.ifr_addr.sa_family = AF_INET;
    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr = netmask.ia_addr;
    if (!ifreq_ioctl(ifname, SIOCSIFNETMASK, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCSIFNETMASK failed. Error setting the netmask address: %s",
                ifname,
                strerror(errno));

        return false;
    }

    return true;
}

/**
 * Get the Netmask of an interface
 */
bool ifreq_netmask_get(const char *ifname, osn_ip_addr_t *netmask)
{
    struct ifreq ifr;

    ifr.ifr_addr.sa_family = AF_INET;

    *netmask = OSN_IP_ADDR_INIT;
    if (!ifreq_ioctl(ifname, SIOCGIFNETMASK, &ifr))
    {
        return false;
    }

    memcpy(
            &netmask->ia_addr,
            &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr,
            sizeof(netmask->ia_addr));

    return true;
}


/**
 * Set the broadcast address of an interface
 */
bool ifreq_bcaddr_set(const char *ifname, osn_ip_addr_t bcaddr)
{
    struct ifreq ifr;

    ifr.ifr_addr.sa_family = AF_INET;
    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr = bcaddr.ia_addr;
    if (!ifreq_ioctl(ifname, SIOCSIFBRDADDR, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCSIFBRDADDR failed. Error setting the broadcast address: %s",
                ifname,
                strerror(errno));

        return false;
    }

    return true;
}

/**
 * Get the broadcast address of an interface
 */
bool ifreq_bcaddr_get(const char *ifname, osn_ip_addr_t *bcaddr)
{
    struct ifreq ifr;

    *bcaddr = OSN_IP_ADDR_INIT;

    ifr.ifr_addr.sa_family = AF_INET;

    if (!ifreq_ioctl(ifname, SIOCGIFBRDADDR, &ifr))
    {
        return false;
    }

    memcpy(
            &bcaddr->ia_addr,
            &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr,
            sizeof(bcaddr->ia_addr));

    return true;
}

/**
 * Set the MTU
 */
bool ifreq_mtu_set(const char *ifname, int mtu)
{
    struct ifreq ifr;

    ifr.ifr_mtu = mtu;

    if (!ifreq_ioctl(ifname, SIOCSIFMTU, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCSIFMTU failed. Error setting the MTU: %s",
                ifname,
                strerror(errno));

        return false;
    }

    return true;
}

/**
 * Get the MTU
 */
bool ifreq_mtu_get(const char *ifname, int *mtu)
{
    struct ifreq ifr;

    if (!ifreq_ioctl(ifname, SIOCGIFMTU, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCGIFMTU failed. Error retrieving the MTU: %s",
                ifname,
                strerror(errno));

        return false;
    }

    *mtu = ifr.ifr_mtu;

    return true;
}


/**
 * Get the MAC address, as string
 */
bool ifreq_hwaddr_get(const char *ifname, osn_mac_addr_t *macaddr)
{
    struct ifreq ifr;

    /* Get the MAC(hardware) address */
    if (!ifreq_ioctl(ifname, SIOCGIFHWADDR, &ifr))
    {
        LOG(ERR, "inet_eth: %s: SIOCGIFHWADDR failed. Error retrieving the MAC address: %s",
                ifname,
                strerror(errno));

        return false;
    }

    *macaddr = OSN_MAC_ADDR_INIT;
    /* Copy the address */
    memcpy(macaddr->ma_addr, ifr.ifr_addr.sa_data, sizeof(macaddr->ma_addr));

    return true;

}
