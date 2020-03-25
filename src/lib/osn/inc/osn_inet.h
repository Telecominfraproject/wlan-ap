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

#if !defined(OSN_INET_H_INCLUDED)
#define OSN_INET_H_INCLUDED

#include <stdbool.h>

#include "osn_types.h"

/**
 * @file osn_inet.h
 * @brief OpenSync IPv4
 *
 * @addtogroup OSN
 * @{
 */

/*
 * ===========================================================================
 *  IPv4 Configuration API
 * ===========================================================================
 */

/**
 * @defgroup OSN_IPV4 IPv4
 *
 * OpenSync IPv4 API
 *
 * @{
 */

/**
 * @struct osn_ip
 *
 * OSN IPv4 object. The actual structure implementation is hidden and is
 * platform dependent. A new instance of the object can be obtained by calling
 * @ref osn_ip_new() and must be destroyed using @ref osn_ip_del().
 */
struct osn_ip;

typedef struct osn_ip osn_ip_t;

/**
 * IPv4 status structure. A structure of this type is used when reporting the
 * status of the IPv4 object. See @ref osn_ip_status_fn_t() and @ref
 * osn_ip_status_notify() for more details.
 */
struct osn_ip_status
{
    const char         *is_ifname;    /**<Interface name */
    void               *is_data;      /**< User data */
    size_t              is_addr_len;  /**< Length of is_addr array */
    osn_ip_addr_t       is_addr;      /**< List of IPv4 addresses on interface */
    size_t              is_dns_len;   /**< Length of is_dns array */
    osn_ip_addr_t       is_dns;       /**< List of DNS servers */
};

/**
 * osn_ip_t status notification callback. This function will be invoked
 * whenever the osn_ip_t object wishes to report the IPv4 status.
 *
 * Typically this will happen whenever an IPv4 status change is detected (for
 * example, when the IP of the interface changes).
 *
 * Some implementation may choose to call this function periodically even if
 * there has been no status change detected.
 *
 * @param[in]   self    A valid pointer to an osn_ip_t object
 * @param[in]   status  A pointer to a @ref osn_ip_status
 */
typedef void osn_ip_status_fn_t(osn_ip_t *ip, struct osn_ip_status *status);

/**
 * Create a new instance of a IPv4 object.
 *
 * @param[in]   ifname  Interface name to which the IPv4 instance will be
 *                      bound to
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_ip_t object is returned.
 */
osn_ip_t *osn_ip_new(const char *ifname);

/**
 * Destroy a valid osn_ip_t object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 *
 * @return
 * This function returns true on success. On error, false is returned.
 * The input parameter should be considered invalid after this function
 * returns, regardless of the error code.
 *
 * @note
 * All resources that were allocated during the lifetime of the object are
 * freed.
 * The implementation may choose to remove all IPv4 addresses regardless if
 * they were added using @ref osn_ip_addr_add().
 *
 * @note
 * If osn_ip_addr_add() returns success when adding a duplicate address then
 * osn_ip_addr_del() should return success when removing an invalid address.
 */
bool osn_ip_del(osn_ip_t *ip);

/**
 * Add an IPv4 address to the IPv4 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   addr  A pointer to a valid IPv4 address (@ref osn_ip_addr_t)
 *
 * @note
 * The new configuration may not take effect until osn_ip_apply() is called.
 *
 * @note
 * If osn_ip_addr_add() returns success when adding a duplicate address then
 * osn_ip_addr_del() should return success when removing an invalid address.
 */
bool osn_ip_addr_add(osn_ip_t *ip, const osn_ip_addr_t *addr);

/**
 * Remove an IPv4 address from the IPv4 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   addr  A pointer to a valid IPv4 address (@ref osn_ip_addr_t)
 *
 * @note
 * The new configuration may not take effect until osn_ip_apply() is called.
 */
bool osn_ip_addr_del(osn_ip_t *ip, const osn_ip_addr_t *addr);

/**
 * Add an DNSv4 server IP to the IPv4 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   addr  A pointer to a valid DNSv4 address (@ref osn_ip_addr_t)
 *
 * @note
 * The new configuration may not take effect until osn_ip_apply() is called.
 */
bool osn_ip_dns_add(osn_ip_t *ip, const osn_ip_addr_t *dns);

/**
 * Remove an DNSv4 server IP from the IPv4 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   addr  A pointer to a valid DNSv4 address (@ref osn_ip_addr_t)
 *
 * @note
 * The new configuration may not take effect until osn_ip_apply() is called.
 */
bool osn_ip_dns_del(osn_ip_t *ip, const osn_ip_addr_t *dns);

/**
 * Add gateway route to IPv4 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   src   Source IPv4 subnet
 * @param[in]   gw    Gateway IPv4 address
 *
 * @note
 * The new configuration may not take effect until osn_ip_apply() is called.
 * This might be moved to the OSN_ROUTEV4 API.
 */
bool osn_ip_route_gw_add(osn_ip_t *ip, const osn_ip_addr_t *src, const osn_ip_addr_t *gw);

/**
 * Remove gateway route from IPv4 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   src   Source IPv4 subnet
 * @param[in]   gw    Gateway IPv4 address
 *
 * @note
 * The new configuration may not take effect until osn_ip_apply() is called.
 * This might be moved to the OSN_ROUTEV4 API.
 */
bool osn_ip_route_gw_del(osn_ip_t *ip, const osn_ip_addr_t *src, const osn_ip_addr_t *gw);

/**
 * Set the IPv4 status callback.
 *
 * Depending on the implementation, the status callback may be invoked
 * periodically or whenever a IPv4 status change has been detected.
 * For maximum portability, the callback implementation should assume it can
 * be called using either modes of operation.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   fn    A pointer to the function implementation
 */
void osn_ip_status_notify(osn_ip_t *ip, osn_ip_status_fn_t *fn, void *data);

/**
 * Ensure that all configuration pertaining the @p self object is applied to
 * the running system.
 *
 * How the configuration is applied to the system is highly implementation
 * dependent. Sometimes it makes sense to cluster together several
 * configuration parameters (for example, dnsmasq uses a single config file).
 *
 * osn_ip_apply() makes sure that a write operation is initiated for
 * all currently cached (dirty) configuration data.
 *
 * @note It is not guaranteed that the configuration will be applied as soon
 * as osn_ip_apply() returns -- only that the configuration process
 * will be started for all pending operations.
 */
bool osn_ip_apply(osn_ip_t *ip);

/*
 * ===========================================================================
 *  IPv4 Routing API
 * ===========================================================================
 */

/**
 * @defgroup OSN_ROUTEV4 IPv4 Routing
 *
 * OpenSync IPv4 Routing API
 *
 * @note The IPv4 routing API is subject to change and may be merged with the
 * osn_ip_t class in the future.
 *
 * @{
 */

/**
 * @struct osn_route
 *
 * IPv4 Routing object. The actual structure implementation is hidden
 * and is platform dependent. A new instance of the object can be obtained by
 * calling @ref osn_route_new() and must be destroyed using @ref
 * osn_route_del().
 */
struct osn_route;
typedef struct osn_route osn_route_t;

/**
 * Structure passed to the route state notify callback, see @ref
 * osn_route_status_fn_t()
 */
struct osn_route_status
{
    osn_ip_addr_t   rts_dst_ipaddr;  /* Destination */
    osn_ip_addr_t   rts_dst_mask;    /* Netmask */
    osn_ip_addr_t   rts_gw_ipaddr;   /* Gateway, of OSN_IP_ADDR_INIT if none */
    osn_mac_addr_t  rts_gw_hwaddr;   /* Gateway MAC address */
};

/**
 * Initializer for the @ref osn_route_status structure.
 *
 * Use this macro to initialize a @ref osn_route_status structure to its
 * default values
 */
#define OSN_ROUTE_STATUS_INIT (struct osn_route_status) \
{                                                       \
    .rts_dst_ipaddr = OSN_IP_ADDR_INIT,                 \
    .rts_dst_mask = OSN_IP_ADDR_INIT,                   \
    .rts_gw_ipaddr = OSN_IP_ADDR_INIT,                  \
    .rts_gw_hwaddr = OSN_MAC_ADDR_INIT,                 \
}

/**
 * osn_route_t status notification callback. This function will be invoked
 * whenever the osn_route_t object detects a status change and wishes to report
 * it.
 *
 * Typically this will happen whenever an routing change is detected (for
 * example, when a new route is added to the system).
 *
 * Some implementation may choose to call this function periodically even if
 * there has been no status change detected.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   data  Private data
 * @param[in]   rts   A pointer to a @ref osn_route_status
 * @param[in]   remove  true if the route in @p rts was removed
 */
typedef bool osn_route_status_fn_t(
        void *data,
        struct osn_route_status *rts,
        bool remove);

/**
 * Create a new IPv4 routing object. This object can be used to add/remove
 * IPv4 routing rules. The object is bound to the interface @p ifname.
 *
 * @param[in]   ifname  Interface name to which the routing object instance
 *                      will be bound to
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_route_t object is returned.
 */
osn_route_t *osn_route_new(const char *ifname);

/**
 * Destroy a valid osn_route_t object.
 *
 * @param[in]   self  A valid pointer to an osn_route_t object
 *
 * @return
 * This function returns true on success. On error, false is returned.
 * The input parameter should be considered invalid after this function
 * returns, regardless of the error code.
 *
 * @note
 * All resources that were allocated during the lifetime of the object are
 * freed.
 */
bool osn_route_del(osn_route_t *self);

/**
 * Set the IPv4 status callback.
 *
 * Depending on the implementation, the status callback may be invoked
 * periodically or whenever a IPv4 status change has been detected.
 * For maximum portability, the callback implementation should assume it can
 * be called using either modes of operation.
 *
 * @param[in]   self  A valid pointer to an osn_route_t object
 * @param[in]   fn    A pointer to the function implementation
 * @param[in]   data  Private data, will be passed to the callback
 */
bool osn_route_status_notify(osn_route_t *self, osn_route_status_fn_t *fn, void *data);

/** @} OSN_ROUTEV4 */
/** @} OSN_IPV4 */
/** @} OSN */

#endif /* OSN_INET_H_INCLUDED */
