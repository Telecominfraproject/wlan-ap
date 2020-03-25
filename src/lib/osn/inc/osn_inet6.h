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

#if !defined(OSN_INET6_H_INCLUDED)
#define OSN_INET6_H_INCLUDED

#include <netinet/in.h>
#include <limits.h>

#include "osn_types.h"

/**
 * @file osn_inet6.h
 * @brief OpenSync IPv6
 *
 * @addtogroup OSN
 * @{
 */

/*
 * ===========================================================================
 *  IPv6 Interface Configuration
 * ===========================================================================
 */

/**
 * @defgroup OSN_IPV6 IPv6
 *
 * OpenSync IPv6 API
 *
 * @{
 */

/**
 * @struct osn_ip6
 *
 * OSN IPv6 object. The actual structure implementation is hidden and is
 * platform dependent. A new instance of the object can be obtained by calling
 * @ref osn_ip6_new() and must be destroyed using @ref osn_ip6_del().
 */
struct osn_ip6;

typedef struct osn_ip6 osn_ip6_t;

/**
 * IPv6 neighbor report structure. Used by osn_ip6_status to report IPv6
 * neighbors.
 */
struct osn_ip6_neigh
{
    osn_ip6_addr_t      i6n_ipaddr;         /**< Neighbor IPv6 address  */
    osn_mac_addr_t      i6n_hwaddr;         /**< Neighbor MAC address */
};

/**
 * IPv6 status structure. A structure of this type is used when reporting the
 * status of the IPv6 object. See @ref osn_ip_status_fn_t() and @ref
 * osn_ip_status_notify() for more details.
 */
struct osn_ip6_status
{
    const char             *is6_ifname;     /**< Interface name */
    void                   *is6_data;       /**< Custom data */
    osn_ip6_addr_t         *is6_addr;       /**< List of IPv6 addresses on interface */
    size_t                  is6_addr_len;   /**< Length of is6_addr array */
    osn_ip6_addr_t         *is6_dns;        /**< List of configure DNSv6 servers */
    size_t                  is6_dns_len;    /**< Length of is6_dns array */
    struct osn_ip6_neigh   *is6_neigh;      /**< List of IPv6 neighbors */
    size_t                  is6_neigh_len;  /**< Length of is6_neigh array */
};

/**
 * osn_ip6_t status notification callback. This function will be invoked
 * whenever the osn_ip6_t object wishes to report the IPv6 status.
 *
 * Typically this will happen whenever an IPv6 status change is detected (for
 * example, when the IP of the interface changes).
 *
 * Some implementation may choose to call this function periodically even if
 * there has been no status change detected.
 *
 * @param[in]   self    A valid pointer to an osn_ip6_t object
 * @param[in]   status  A pointer to a @ref osn_ip6_status
 */
typedef void osn_ip6_status_fn_t(osn_ip6_t *self, struct osn_ip6_status *status);

/**
 * Create a new instance of a IPv6 object.
 *
 * @param[in]   ifname  Interface name to which the IPv6 instance will be
 *                      bound to
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_ip6_t object is returned.
 */
osn_ip6_t *osn_ip6_new(const char *ifname);

/**
 * Destroy a valid osn_ip6_t object.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_t object
 *
 * @return
 * This function returns true on success. On error, false is returned.
 * The input parameter should be considered invalid after this function
 * returns, regardless of the error code.
 *
 * @note
 * All global IPv6 addresses that were allocated during the lifetime of the
 * object are removed.
 * The implementation may choose to remove all global IPv6 addresses regardless
 * if they were added using @ref osn_ip6_addr_add().
 */
bool osn_ip6_del(osn_ip6_t *ip6);

/**
 * Ensure that all configuration pertaining the @p self object is applied to
 * the running system.
 *
 * How the configuration is applied to the system is highly implementation
 * dependent. Sometimes it makes sense to cluster together several
 * configuration parameters (for example, dnsmasq uses a single config file).
 *
 * osn_ip6_apply() makes sure that a write operation is initiated for
 * all currently cached (dirty) configuration data.
 *
 * @note It is not guaranteed that the configuration will be applied as soon
 * as osn_ip6_apply() returns -- only that the configuration process
 * will be started for all pending operations.
 */
bool osn_ip6_apply(osn_ip6_t *ip6);

/**
 * Add an IPv6 address to the IPv6 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_t object
 * @param[in]   addr  A pointer to a valid IPv6 address (@ref osn_ip6_addr_t)
 *
 * @return
 * This function returns true if the address was successfully added to the
 * object, false otherwise.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_apply() is
 * called.
 *
 * @note
 * Adding a duplicate address may result in success.
 *
 * @note
 * If osn_ip6_addr_add() returns success when adding a duplicate address then
 * osn_ip6_addr_del() should return success when removing an invalid address.
 */
bool osn_ip6_addr_add(osn_ip6_t *ip6, const osn_ip6_addr_t *addr);

/**
 * Remove an IPv6 address from the IPv6 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_t object
 * @param[in]   addr  A pointer to a valid IPv6 address (@ref osn_ip6_addr_t)
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_apply() is
 * called.
 *
 * @note
 * Removing an non-existent address may result in success.
 *
 * @note
 * If osn_ip6_addr_add() returns success when adding a duplicate address then
 * osn_ip6_addr_del() should return success when removing an invalid address.
 */
bool osn_ip6_addr_del(osn_ip6_t *ip6, const osn_ip6_addr_t *addr);

/**
 * Add an DNSv6 server address to the IPv6 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_t object
 * @param[in]   dns   A pointer to a valid IPv6 address (@ref osn_ip6_addr_t)
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_apply() is
 * called.
 */
bool osn_ip6_dns_add(osn_ip6_t *ip6, const osn_ip6_addr_t *dns);

/**
 * Remove an DNSv4 server IPv6 address from the IPv6 object.
 *
 * @param[in]   self  A valid pointer to an osn_ip_t object
 * @param[in]   dns   A pointer to a valid DNSv4 address (@ref osn_ip_addr_t)
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip_apply() is
 * called.
 */
bool osn_ip6_dns_del(osn_ip6_t *ip6, const osn_ip6_addr_t *dns);

/**
 * Set the IPv6 status callback.
 *
 * Depending on the implementation, the status callback may be invoked
 * periodically or whenever a IPv6 status change has been detected.
 * For maximum portability, the callback implementation should assume it can
 * be called using either modes of operation.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_t object
 * @param[in]   fn    A pointer to the function implementation
 */
void osn_ip6_status_notify(osn_ip6_t *ip6, osn_ip6_status_fn_t *fm, void *data);

/*
 * ===========================================================================
 *  Router Advertisement
 * ===========================================================================
 */

/**
 * @defgroup OSN_IPV6_RA Router Advertisement
 *
 * OpenSync IPv6 Router Advertisement API
 *
 * @{
 */

/**
 * @struct osn_ipv6_radv
 *
 * IPv6 Router Advertisement object. The actual structure implementation is
 * hidden and is platform dependent. A new instance of the object can be
 * obtained by calling @ref osn_ip6_radv_new() and must be destroyed using @ref
 * osn_ip6_radv_del().
 */
struct osn_ipv6_radv;

typedef struct osn_ip6_radv osn_ip6_radv_t;

/**
 * Router advertisement options, typically INT_MIN means that the value is unset
 * and the default should be used.
 *
 * The OSN_IP6_RADV_OPTIONS_INIT macro can be used to initialize the structure
 * to sane defaults.
 *
 * This structure can be applied to an existing @ref osn_ip6_radv_t object by
 * using the @ref osn_ip6_radv_set() function.
 */
struct osn_ip6_radv_options
{
    bool    ra_managed;             /**< Managed flag */
    bool    ra_other_config;        /**< Other Config flag */
    bool    ra_home_agent;          /**< Home Agent flag */
    int     ra_max_adv_interval;    /**< Max advertisement interval */
    int     ra_min_adv_interval;    /**< Min advertisement interval */
    int     ra_default_lft;         /**< Default lifetime */
    int     ra_preferred_router;    /**< Preferred router: 0 - low, 1 - med, 2 - high */
    int     ra_mtu;                 /**< Advertised MTU */
    int     ra_reachable_time;      /**< Reachable time */
    int     ra_retrans_timer;       /**< Retransmit timer */
    int     ra_current_hop_limit;   /**< Current hop limit */
};

#define OSN_IP6_RADV_OPTIONS_INIT (struct osn_ip6_radv_options) \
{                                                               \
    .ra_max_adv_interval    = INT_MIN,                          \
    .ra_min_adv_interval    = INT_MIN,                          \
    .ra_default_lft         = INT_MIN,                          \
    .ra_preferred_router    = INT_MIN,                          \
    .ra_mtu                 = INT_MIN,                          \
    .ra_reachable_time      = INT_MIN,                          \
    .ra_retrans_timer       = INT_MIN,                          \
    .ra_current_hop_limit   = INT_MIN,                          \
}

/**
 * Create a new instance of the Router Advertisement object.
 *
 * @param[in]   ifname  Interface name to which the IPv4 instance will be
 *                      bound to
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_ip6_radv_t object is returned.
 */
osn_ip6_radv_t *osn_ip6_radv_new(const char *ifname);

/**
 * Destroys the a Router Advertisement object. This function should return
 * the system to its original state (stopping any RADV services that are running
 * as a consequence of this object, etc.)
 *
 * @param[in]   self  A valid pointer to an osn_ip6_radv_t object
 *
 * @return
 * This function returns true on success. On error, false is returned.
 * The input parameter should be considered invalid after this function
 * returns, regardless of the error code.
 */
bool osn_ip6_radv_del(osn_ip6_radv_t *self);

/**
 * Apply the Router Advertisement options to the osn_ip6_radv_t object.
 * The osn_ip6_radv_options can be used to modify the default parameters
 * of the RA service. The @ref osn_ip6_radv_options structure must be
 * initialized with the @ref OSN_IP6_RADV_OPTIONS_INIT initializer.
 *
 * @param[in]   self  A valid pointer to an @ref osn_ip6_radv_t object
 * @param[in]   opts  A valid pointer to an @ref osn_ip6_radv_options structure
 *
 * @return
 * This function returns true on success, false otherwise. If false is returned
 * options may have been partially applied.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_radv_apply() is
 * called.
 */
bool osn_ip6_radv_set(osn_ip6_radv_t *self, const struct osn_ip6_radv_options *opts);

/**
 * Add the IPv6 prefix to the list of advertised prefixes.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_radv_t object
 * @param[in]   prefix  IPv6 prefix to add
 * @param[in]   autonomous  Set the A flag for this prefix
 * @param[in]   on_link  Set the L flag for this prefix
 *
 * @return
 * Return true if the prefix was successfully added, false otherwise.
 *
 * @note
 * If osn_ip6_radv_add_prefix() allows the addition of duplicate prefixes, then
 * @ref osn_ip6_radv_del_prefix() should allow removal of non-existing prefixes.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_radv_apply() is
 * called.
 */
bool osn_ip6_radv_add_prefix(
        osn_ip6_radv_t *self,
        const osn_ip6_addr_t *prefix,
        bool autonomous,
        bool onlink);

/**
 * Remove the IPv6 prefix from the list of advertised prefixes.
 *
 * @param[in]   self    A valid pointer to an osn_ip6_radv_t object
 * @param[in]   prefix  IPv6 prefix to remove
 *
 * @return
 * Return true if the prefix was successfully added, false otherwise.
 *
 * @note
 * If @ref osn_ip6_radv_add_prefix() allows the addition of duplicate prefixes,
 * then osn_ip6_radv_del_prefix() should allow removal of non-existing prefixes.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_radv_apply() is
 * called.
 */
bool osn_ip6_radv_del_prefix(osn_ip6_radv_t *self, const osn_ip6_addr_t *prefix);

/**
 * Add the IPv6 address to the list of advertised RDNS servers.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_radv_t object
 * @param[in]   dns   IPv6 DNS server address to add
 *
 * @return
 * Return true if the DNSv6 server address was successfully added, false
 * otherwise.
 *
 * @note
 * If osn_ip6_radv_add_rdnss() allows the addition of duplicate addresses, then
 * @ref osn_ip6_radv_del_rdnss() should allow removal of non-existing addresses.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_radv_apply() is
 * called.
 */
bool osn_ip6_radv_add_rdnss(osn_ip6_radv_t *self, const osn_ip6_addr_t *dns);

/**
 * Remove the IPv6 address from the list of advertised RDNS servers.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_radv_t object
 * @param[in]   dns   IPv6 DNS server address to remove
 *
 * @return
 * Return true if the DNSv6 server address was successfully added, false
 * otherwise.
 *
 * @note
 * If @ref osn_ip6_radv_add_rdnss() allows the addition of duplicate addresses,
 * then osn_ip6_radv_del_rdnss() should allow removal of non-existing addresses.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_radv_apply() is
 * called.
 */
bool osn_ip6_radv_del_rdnss(osn_ip6_radv_t *self, const osn_ip6_addr_t *dns);

/**
 * Add the DNSv6 domain to the list of advertised DNSSL domains
 *
 * @param[in]   self  A valid pointer to an osn_ip6_radv_t object
 * @param[in]   sl    Search domain
 *
 * @return
 * Return true if the DNSv6search domain server address was successfully added,
 * false otherwise.
 *
 * @note
 * If osn_ip6_radv_add_dnssl() allows the addition of duplicate domains, then
 * @ref osn_ip6_radv_del_dnssl() should allow removal of non-existing domains.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_radv_apply() is
 * called.
 */
bool osn_ip6_radv_add_dnssl(osn_ip6_radv_t *self, char *sl);

/**
 * Remove the DNSv6 search domain from the list of advertised DNSSL servers.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_radv_t object
 * @param[in]   sl    IPv6 DNS server address to remove
 *
 * @return
 * Return true if the DNSv6 search domain server address was successfully
 * removed, false otherwise.
 *
 * @note
 * If @ref osn_ip6_radv_add_dnssl() allows the addition of duplicate domains,
 * then osn_ip6_radv_del_dnssl() should allow removal of non-existing domains.
 *
 * @note
 * The new configuration may not take effect until @ref osn_ip6_radv_apply() is
 * called.
 */
bool osn_ip6_radv_del_dnssl(osn_ip6_radv_t *self, char *sl);

bool osn_ip6_radv_apply(osn_ip6_radv_t *ip6);

/** @} OSN_IPV6_RA */

/** @} OSN_IPV6 */

/** @} OSN */

#endif /* OSN_INET6_H_INCLUDED */
