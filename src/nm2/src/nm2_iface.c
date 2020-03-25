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

#include "osa_assert.h"
#include "log.h"
#include "version.h"
#include "inet.h"
#include "evx.h"
#include "schema.h"
#include "target.h"

#include "nm2.h"
#include "nm2_iface.h"

/*
 * ===========================================================================
 *  KConfig backward compatibility
 * ===========================================================================
 */
/* For platforms not using KConfig yet, default to polling */
#if !defined(CONFIG_USE_KCONFIG)
#define CONFIG_INET_STATUS_NETLINK_POLL         1
#define CONFIG_INET_STATUS_NETLINK_DEBOUNCE_MS  200
#endif

/*
 * ===========================================================================
 *  Globals and forward declarations
 * ===========================================================================
 */
static ds_key_cmp_t nm2_iface_cmp;

ds_tree_t nm2_iface_list = DS_TREE_INIT(nm2_iface_cmp, struct nm2_iface, if_tnode);

static bool nm2_iface_dhcpc_notify(void *ctx, enum osn_notify hint, const char *name, const char *value);
static inet_t *nm2_iface_new_inet(const char *ifname, enum nm2_iftype type, void *ctx);
static void nm2_iface_status_init(void);

/*
 * ===========================================================================
 *  Type handling
 * ===========================================================================
 */

/*
 * Convert an NM2_IFTYPE_* enum to a human readable string
 */
const char *nm2_iftype_tostr(enum nm2_iftype type)
{
    const char *iftype_str[NM2_IFTYPE_MAX + 1] =
    {
        #define _STR(sym, str) [sym] = str,
        NM2_IFTYPE(_STR)
        #undef _STR
    };

    ASSERT(type <= NM2_IFTYPE_MAX, "Unknown nm2_iftype value");

    return iftype_str[type];
}

/*
 * Convert iftype as string to an enum
 */
bool nm2_iftype_fromstr(enum nm2_iftype *type, const char *str)
{
    const char *iftype_str_map[NM2_IFTYPE_MAX + 1] =
    {
        #define _STR(sym, str) [sym] = str,
        NM2_IFTYPE(_STR)
        #undef _STR
    };

    int ii;

    for (ii = 0; ii < NM2_IFTYPE_MAX; ii++)
    {
        if (iftype_str_map[ii] == NULL) break;

        if (strcmp(iftype_str_map[ii], str) == 0)
        {
            *type = ii;
            return true;
        }
    }

    *type = NM2_IFTYPE_NONE;

    return false;
}

/*
 * ===========================================================================
 *  Interface handling
 * ===========================================================================
 */

/*
 * General initializer
 */
bool nm2_iface_init(void)
{
    /* Initialize interface status polling */
    nm2_iface_status_init();

    return true;
}

/*
 * Find and return the interface @p ifname.
 * If the interface is not found  NULL will
 * be returned.
 */
struct nm2_iface *nm2_iface_get_by_name(char *_ifname)
{
    char *ifname = _ifname;
    struct nm2_iface *piface;

    piface = ds_tree_find(&nm2_iface_list, ifname);
    if (piface != NULL)
        return piface;

    LOG(ERR, "nm2_iface_get_by_name: Couldn't find the interface(%s)", ifname);

    return NULL;
}

/*
* Creates a new interface of the specified type.
* Also initializes dhcp_options and inet interface.
*/
struct nm2_iface *nm2_iface_new(const char *_ifname, enum nm2_iftype if_type)
{
    TRACE();

    const char *ifname = _ifname;

    struct nm2_iface *piface;

    piface = calloc(1, sizeof(struct nm2_iface));
    piface->if_type = if_type;

    if (strscpy(piface->if_name, ifname, sizeof(piface->if_name)) < 0)
    {
        LOG(ERR, "nm2_iface_new: %s (%s): Error creating interface, name too long.",
                ifname,
                nm2_iftype_tostr(if_type));
        free(piface);
        return NULL;
    }

    /* Dynamically initialize the DHCP client options tree */
    ds_tree_init(
            &piface->if_dhcpc_options,
            ds_str_cmp,
            struct nm2_iface_dhcp_option,
            do_tnode);

    piface->if_inet = nm2_iface_new_inet(ifname, if_type, piface);
    if (piface->if_inet == NULL)
    {
        LOG(ERR, "nm2_iface_new: %s (%s): Error creating interface, constructor failed.",
                ifname,
                nm2_iftype_tostr(if_type));

        free(piface);
        return NULL;
    }

    nm2_iface_status_sync(piface);

    ds_tree_insert(&nm2_iface_list, piface, piface->if_name);

    LOG(INFO, "nm2_iface_new: %s: Created new interface (type %s).", ifname, nm2_iftype_tostr(if_type));

    return piface;
}

/*
 * Delete interface and associated structures
 */
bool nm2_iface_del(struct nm2_iface *piface)
{
    ds_tree_iter_t iter;
    struct nm2_iface_dhcp_option *popt;

    bool retval = true;


    /* Destroy the inet object */
    if (!inet_del(piface->if_inet))
    {
        LOG(WARN, "nm2_iface: %s (%s): Error deleting interface.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
        retval = false;
    }

    /* Destroy DHCP options list */
    ds_tree_foreach_iter(&piface->if_dhcpc_options, popt, &iter)
    {
        ds_tree_iremove(&iter);
        free(popt);
    }


    /* Remove interface from global interface list */
    ds_tree_remove(&nm2_iface_list, piface);

    free(piface);
    return retval;
}

/*
 * Retrieve a nm2_iface structure by looking it up using the ip address
 */
struct nm2_iface *nm2_iface_find_by_ipv4(osn_ip_addr_t addr)
{
    TRACE();

    osn_ip_addr_t if_addr;
    osn_ip_addr_t if_subnet;
    osn_ip_addr_t addr_subnet;

    struct nm2_iface *piface;

    ds_tree_foreach(&nm2_iface_list, piface)
    {
        int prefix;

        if_addr = piface->if_state.in_ipaddr;

        /*
         * Derive the prefix from the "netmask" address in the if_state structure  and assign it
         * to both if_addr and addr
         */
        prefix = osn_ip_addr_to_prefix(&piface->if_state.in_netmask);
        if (prefix == 0) continue;

        if_addr.ia_prefix = prefix;
        addr.ia_prefix = prefix;

        /* Calculate the subnets of both IPs */
        if_subnet = osn_ip_addr_subnet(&if_addr);
        addr_subnet = osn_ip_addr_subnet(&addr);

        /* If both subnets match, it means that addr is part of the IP range in piface */
        if (osn_ip_addr_cmp(&if_subnet, &addr_subnet) == 0)
        {
            return piface;
        }
    }

    return NULL;
}

/*
 * Apply the pending configuration to the runtime system.
 *
 * This function is split into two parts, nm2_iface_apply() which just schedules
 * a callback to __nm2_iface_apply().
 *
 * Note: The actual configuration will be applied after the return point of
 * this function.
 */

void __nm2_iface_apply(EV_P_ ev_debounce *w, int revent)
{
    (void)loop;
    (void)w;
    (void)revent;

    TRACE();

    struct nm2_iface *piface;

    ds_tree_foreach(&nm2_iface_list, piface)
    {
        /* If not commit bit is set, continue */
        if (!piface->if_commit) continue;
        piface->if_commit = false;

        /* Apply the configuration */
        if (!inet_commit(piface->if_inet))
        {
            LOG(NOTICE, "nm2_iface: %s (%s): Error committing new configuration.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type));
        }
    }

    /* Poll new status */
    nm2_iface_status_poll();
}

/* Schedule __nm2_iface_apply() via debouncing */
bool nm2_iface_apply(struct nm2_iface *piface)
{
    TRACE();

    static ev_debounce apply_timer;

    static bool apply_init = false;

    /* Flag the interface as pending for commit */
    piface->if_commit = true;

    /* If it's not already initialized, initialize the commit timer */
    if (!apply_init)
    {
        ev_debounce_init(&apply_timer, __nm2_iface_apply, NM2_IFACE_APPLY_TIMER);
        apply_init = true;
    }

    /* Schedule the configuration apply */
    ev_debounce_start(EV_DEFAULT, &apply_timer);

    return true;
}

/*
 * Create a new interface of type @p type. This is primarily used as a dispatcher
 * for various inet_new_*() constructors.
 *
 * Additionally, it initailizes some of the DHCP client options -- this is
 * true also for interfaces that do not actually use DHCP.
 *
 * TODO: Get rid of the target dependency (target_sku_get(), target_model_get() etc).
 */
inet_t *nm2_iface_new_inet(const char *ifname, enum nm2_iftype type, void *ctx)
{
    TRACE();

    char serial_num[100] = { 0 };
    char sku_num[100] = { 0 };
    char hostname[C_HOSTNAME_LEN] = { 0 };
    char vendor_class[OSN_DHCP_VENDORCLASS_MAX] = { 0 };

    inet_t *nif = NULL;

    switch (type)
    {
        case NM2_IFTYPE_ETH:
        case NM2_IFTYPE_BRIDGE:
        case NM2_IFTYPE_TAP:
            nif = inet_eth_new(ifname);
            break;

        case NM2_IFTYPE_VIF:
            nif = inet_vif_new(ifname);
            break;

        case NM2_IFTYPE_GRE:
            nif = inet_gre_new(ifname);
            break;

        case NM2_IFTYPE_VLAN:
            nif = inet_vlan_new(ifname);
            break;

        default:
            /* Unsupported types */
            LOG(ERR, "nm2_iface: %s: Unsupported interface type: %d", ifname, type);
            return NULL;
    }

    if (nif == NULL)
    {
        LOG(ERR, "nm2_iface: %s: Error initializing interface type: %d", ifname, type);
        return NULL;
    }

    /*
     * Common initialization for all interfaces
     */

    /* Retrieve vendor class, sku, hostname ... we need these values to populate DHCP options */
    if (vendor_class[0] == '\0' && target_model_get(vendor_class, sizeof(vendor_class)) == false)
    {
        STRSCPY(vendor_class, TARGET_NAME);
    }

    if (serial_num[0] == '\0')
    {
        target_serial_get(serial_num, sizeof(serial_num));
    }

    /* read SKU number, if empty, reset buffer */
    if (hostname[0] == '\0')
    {
        if (target_sku_get(sku_num, sizeof(sku_num)) == false)
        {
            tsnprintf(hostname, sizeof(hostname), "%s_Pod", serial_num);
        }
        else
        {
            tsnprintf(hostname, sizeof(hostname), "%s_Pod_%s", serial_num, sku_num);
        }
    }

    /* Request DHCP options */
    inet_dhcpc_option_request(nif, DHCP_OPTION_SUBNET_MASK, true);
    inet_dhcpc_option_request(nif, DHCP_OPTION_ROUTER, true);
    inet_dhcpc_option_request(nif, DHCP_OPTION_DNS_SERVERS, true);
    inet_dhcpc_option_request(nif, DHCP_OPTION_HOSTNAME, true);
    inet_dhcpc_option_request(nif, DHCP_OPTION_DOMAIN_NAME, true);
    inet_dhcpc_option_request(nif, DHCP_OPTION_BCAST_ADDR, true);
    inet_dhcpc_option_request(nif, DHCP_OPTION_VENDOR_SPECIFIC, true);

    /* Set DHCP options */
    inet_dhcpc_option_set(nif, DHCP_OPTION_VENDOR_CLASS, vendor_class);
    inet_dhcpc_option_set(nif, DHCP_OPTION_HOSTNAME, hostname);
    inet_dhcpc_option_set(nif, DHCP_OPTION_PLUME_SWVER, app_build_number_get());
    inet_dhcpc_option_set(nif, DHCP_OPTION_PLUME_PROFILE, app_build_profile_get());
    inet_dhcpc_option_set(nif, DHCP_OPTION_PLUME_SERIAL_OPT, serial_num);

    /*
     * Register callbacks
     */
    nif->in_data = ctx;

    /* Register DHCP client option registrations */
    inet_dhcpc_option_notify(nif, nm2_iface_dhcpc_notify, ctx);

    /* Register the route state function */
    inet_route_notify(nif, nm2_route_notify);

    return nif;
}

/**
 * Re-registering to callbacks will cause the status to be synchronized.
 */
void nm2_iface_status_sync(struct nm2_iface *piface)
{
    /* Register to IPv6 address updates registrations */
    inet_ip6_addr_status_notify(piface->if_inet, nm2_ip6_addr_status_fn);

    /* Register to IPv6 status updates registrations */
    inet_ip6_neigh_status_notify(piface->if_inet, nm2_ip6_neigh_status_fn);
}

/*
 * ===========================================================================
 * DHCP client options notifications
 * ===========================================================================
 */
bool nm2_iface_dhcpc_notify(void *ctx, enum osn_notify hint, const char *name, const char *value)
{
    struct nm2_iface_dhcp_option *pdo;
    ds_tree_iter_t ido;

    struct nm2_iface *piface = ctx;
    bool update_status = false;

    switch (hint)
    {
        case NOTIFY_SYNC:
            /* Flag all entries as invalid */
            ds_tree_foreach(&piface->if_dhcpc_options, pdo)
            {
                pdo->do_invalid = true;
            }
            break;

        case NOTIFY_FLUSH:
            update_status = true;
            /*
             * Delete all invalid entries -- use an iterator version of foreach
             * so we can safely remove elements while traversing the list
             */
            ds_tree_foreach_iter(&piface->if_dhcpc_options, pdo, &ido)
            {
                if (!pdo->do_invalid) continue;

                ds_tree_iremove(&ido);
                free(pdo->do_name);
                free(pdo->do_value);
                free(pdo);
            }
            break;

        case NOTIFY_UPDATE:
            pdo = ds_tree_find(&piface->if_dhcpc_options, (void *)name);
            if (pdo == NULL)
            {
                /* New entry, create and add it to the tree */
                pdo = calloc(1, sizeof(struct nm2_iface_dhcp_option));
                pdo->do_name = strdup(name);

                ds_tree_insert(&piface->if_dhcpc_options, pdo, pdo->do_name);
            }

            if (pdo->do_value != NULL) free(pdo->do_value);

            /*
             * If we're updating an invalid entry it means we're in the middle of
             * a SYNC/FLUSH cycle. Do not force an update at this point.
             */
            update_status = !pdo->do_invalid;

            pdo->do_value = strdup(value);
            pdo->do_invalid = false;

            break;

        case NOTIFY_DELETE:
            update_status = true;
            pdo = ds_tree_find(&piface->if_dhcpc_options, (void *)name);
            if (pdo == NULL)
            {
                LOG(ERR, "nm2_iface: %s: Unable to delete option -- Interface has no DHCP client option named: %s",
                        piface->if_name,
                        name);
                return true;
            }

            /* Free the entry */
            ds_tree_remove(&piface->if_dhcpc_options, pdo);
            free(pdo->do_name);
            free(pdo->do_value);
            free(pdo);
            break;
    }

    /* Update the interface status */
    if (update_status)
    {
        /* Force a status update */
        piface->if_state_notify = true;
        nm2_iface_status_poll();
    }

    return true;
}

/*
 * Interface comparator
 */
int nm2_iface_cmp(void *_a, void  *_b)
{
    struct nm2_iface *a = _a;
    struct nm2_iface *b = _b;

    return strcmp(a->if_name, b->if_name);
}

/*
 * ===========================================================================
 *  Interface status polling
 *
 *  TODO This contains Linux specific code and should be moved to the new
 *  Osync API.
 * ===========================================================================
 */
/*
 * ===========================================================================
 *  Status reporting
 * ===========================================================================
 */

#if defined(CONFIG_INET_STATUS_POLL)
/*
 * Timer-based interface status polling
 */
void __if_status_poll_fn(EV_P_ ev_timer *w, int revent)
{
    nm2_iface_status_poll();
}

void nm2_iface_status_init(void)
{
    /*
     * Interface polling using a timer
     */
    static ev_timer if_poll_timer;

    MEMZERO(if_poll_timer);

    /* Create a repeating timer */
    ev_timer_init(
            &if_poll_timer,
            __if_status_poll_fn,
            0.0,
            CONFIG_INET_STATUS_POLL_INTERVAL_MS / 1000.0);

    ev_timer_start(EV_DEFAULT, &if_poll_timer);
}

#elif defined(CONFIG_INET_STATUS_NETLINK_POLL)
/*
 * Netlink based interface status polling
 *
 * Note: Netlink sockets are rather poorly documented. Instead of trying to implement the
 * whole protocol, it is much simpler to just use a NETLINK event as a trigger for the actual
 * interface polling code. This is not optimal, but still much faster than timer-based polling.
 */

/* Include netlink specific headers */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define RTNLGRP(x)  ((RTNLGRP_ ## x) > 0 ? 1 << ((RTNLGRP_ ## x) - 1) : 0)

static int __nlsock = -1;
static ev_debounce __nlsock_debounce_ev;

/*
 * nlsock I/O event callback
 */
void __if_status_nlsock_fn(EV_P_ ev_io *w, int revent)
{
    uint8_t buf[getpagesize()];

    if (revent & EV_ERROR)
    {
        LOG(EMERG, "iface: Error on netlink socket.");
        ev_io_stop(loop, w);
        return;
    }

    if (!(revent & EV_READ)) return;

    /* Read and discard the data from the socket */
    if (recv(__nlsock, buf, sizeof(buf), 0) < 0)
    {
        LOG(EMERG, "iface: Received EOF from netlink socket?");
        ev_io_stop(loop, w);
        return;
    }

    /*
     * Interface status changes may occur in bursts, since we're polling all status variables
     * at once, we can debounce the netlink events for a slight performance boost at the cost
     * of a small delay in status updates.
     *
     * This will trigger __if_status_debounce_fn() below when the debounce timer expires.
     */
    ev_debounce_start(EV_DEFAULT, &__nlsock_debounce_ev);
}

/*
 * nlsock debounce timer callback
 */
void __if_status_debounce_fn(EV_P_ ev_debounce *w, int revent)
{
    (void)loop;
    (void)w;
    (void)revent;

    /* Poll interfaces */
    nm2_iface_status_poll();
}

void nm2_iface_status_init(void)
{
     struct sockaddr_nl nladdr;

     /* Create the netlink socket in the NETLINK_ROUTE domain */
     __nlsock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
     if (__nlsock < 0)
     {
         LOGE("target_init: Error creating NETLINK socket.");
         goto error;
     }

     /*
      * Bind the netlink socket to events related to interface status change
      */
     memset(&nladdr, 0, sizeof(nladdr));
     nladdr.nl_family = AF_NETLINK;
     nladdr.nl_pid = 0;
     nladdr.nl_groups =
             RTNLGRP(LINK) |            /* Link/interface status */
             RTNLGRP(IPV4_IFADDR) |     /* IPv4 address status */
             RTNLGRP(IPV6_IFADDR) |     /* IPv6 address status */
             RTNLGRP(IPV4_NETCONF) |    /* No idea */
             RTNLGRP(IPV6_NETCONF);     /* -=- */
     if (bind(__nlsock, (struct sockaddr *)&nladdr, sizeof(nladdr)) != 0)
     {
         LOGE("iface: Error binding NETLINK socket");
         goto error;
     }

     /* Initialize the debouncer */
     ev_debounce_init(
             &__nlsock_debounce_ev,
             __if_status_debounce_fn,
             CONFIG_INET_STATUS_NETLINK_DEBOUNCE_MS / 1000.0);

     /*
      * Initialize an start an I/O watcher
      */
     static ev_io nl_ev;

     ev_io_init(&nl_ev, __if_status_nlsock_fn, __nlsock, EV_READ);
     ev_io_start(EV_DEFAULT, &nl_ev);

     return;

 error:
     if (__nlsock >= 0) close(__nlsock);
     __nlsock = -1;
}

#else
#error Interface status reporting backend not supported.
#endif

void nm2_iface_status_poll(void)
{
    ds_tree_iter_t iter;
    struct nm2_iface *piface;
    inet_state_t tstate;

    /**
     * Traverse the list of interface registered for status polling
     */
    for (piface = ds_tree_ifirst(&iter, &nm2_iface_list);
            piface != NULL;
            piface = ds_tree_inext(&iter))
    {
        if (!inet_state_get(piface->if_inet, &tstate))
        {
            LOG(ERR, "iface: %s (%s): Unable to retrieve interface state.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type));
            continue;
        }

        /* Binary compare old and new states */
        if (memcmp(&piface->if_state, &tstate, sizeof(piface->if_state)) != 0)
        {
            memcpy(&piface->if_state, &tstate, sizeof(piface->if_state));
            piface->if_state_notify = true;
        }

        if (!piface->if_state_notify) continue;
        piface->if_state_notify = false;

        if (!nm2_inet_state_update(piface))
        {
            LOG(ERR, "iface: Error updating interface state: %s", piface->if_name);
        }
    }
}

