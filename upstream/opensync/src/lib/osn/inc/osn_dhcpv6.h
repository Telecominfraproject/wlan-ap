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

#if !defined(OSN_DHCPV6_H_INCLUDED)
#define OSN_DHCPV6_H_INCLUDED

#include "osn_inet6.h"

/**
 * @file osn_dhcpv6.h
 * @brief OpenSync DHCPv6
 *
 * @addtogroup OSN
 * @{
 *
 * @addtogroup OSN_IPV6 IPv6
 * @{
 */

/*
 * ===========================================================================
 *  DHCPv6/RA Client
 * ===========================================================================
 */

/**
 * @defgroup OSN_DHCPV6 DHCPv6
 *
 * Common DHCPv6 API definition.
 *
 * @{
 */

/** Maximum hostname size including the terminating \0 */
#define OSN_DHCP_HOSTNAME_LEN   64
/** Maximum number of DHCP options */
#define OSN_DHCP_OPTIONS_MAX    256

/**
 * @defgroup OSN_DHCPV6_CLIENT DHCPv6 Client
 *
 * DHCPv6 Client API definition.
 *
 * @{
 */

/**
 * @struct osn_dhcpv6_client
 *
 * OSN DHCPv6 client object. The actual structure implementation is hidden
 * and is platform dependent. A new instance of the object can be obtained by
 * calling @ref osn_dhcpv6_client_new() and must be destroyed using @ref
 * osn_dhcpv6_client_del().
 */
struct osn_dhcpv6_client;
typedef struct osn_dhcpv6_client osn_dhcpv6_client_t;

/**
 * DHCPv6 client status report structure. A structure of this type is used
 * for reporting the status of the DHCPv6 client object. See @ref
 * osn_dhcpv6_client_notify_fn_t
 */
struct osn_dhcpv6_client_status
{
    /** Private data */
    void            *d6c_data;
    /** True whether client has connected */
    bool            d6c_connected;
    /** Received options, base64 encoded string or NULL for none */
    char            *d6c_recv_options[OSN_DHCP_OPTIONS_MAX];
};

/**
 * osn_dhcpv6_client_t status notification callback. This function will be
 * invoked whenever the osn_dhcpv6_client_t object wishes to report the DHCPv6
 * status.
 *
 * Typically this will happen whenever an DHCPv6 status change is detected (for
 * example, when DHCPv6 client options are received).
 *
 * Some implementation may choose to call this function periodically even if
 * there has been no status change detected.
 *
 * @param[in]   self    A valid pointer to an osn_dhcpv6_client_t object
 * @param[in]   status  A pointer to a @ref osn_dhcpv6_client_status
 */
typedef void osn_dhcpv6_client_status_fn_t(
        osn_dhcpv6_client_t *self,
        struct osn_dhcpv6_client_status *status);

/**
 * Create a new instance of a DHCPv6 client object.
 *
 * @param[in]   ifname  Interface name to which the DHCPv6 client instance will
 *                      be bound
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_dhcpv6_client_t object is returned.
 */
osn_dhcpv6_client_t *osn_dhcpv6_client_new(const char *ifname);

/**
 * Destroy a valid osn_dhcpv6_client_t object.
 *
 * @param[in]   self  A valid pointer to an osn_dhcpv6_client_t object
 *
 * @return
 * This function returns true on success. On error, false is returned.
 * The input parameter should be considered invalid after this function
 * returns, regardless of the error code.
 */
bool osn_dhcpv6_client_del(osn_dhcpv6_client_t *client);

/**
 * Set DHCPv6 client options.
 *
 * @param[in]   self  A valid pointer to an osn_dhcpv6_client_t object
 * @param[in]   request_address  Request a DHCPv6 address
 * @param[in]   request_prefixes  Request DHCPv6 prefixes
 * @param[in]   rapid_commit  use fast rapid commit
 * @param[in[   renew  renew the IPv6 address
 *
 * @return
 * This function returns true on success or false on error.
 *
 * @note
 * If an error is returned, the options may be partially applied.
 */
bool osn_dhcpv6_client_set(
        osn_dhcpv6_client_t *self,
        bool request_address,
        bool request_prefixes,
        bool rapid_commit,
        bool renew);
/**
 * Set various options that will be requested from the server during the
 * DHCP_REQUEST phase.
 *
 * @param[in]   self  A valid pointer to an osn_dhcpv6_client_t object
 * @param[in]   tag   The DHCPv6 option id
 *
 * @return
 * This function returns true if the option was successfully set, false
 * otherwise.
 *
 * @note
 * There's currently no API to unset a requested option.
 */
bool osn_dhcpv6_client_option_request(osn_dhcpv6_client_t *client, int tag);

/*
 * Set various DHCPv6 options that will be sent to the DHCPv6 server during
 * the DHCP_REQUEST phase.
 *
 * A value of NULL indicates that the options should be removed from the
 * request list.
 *
 * @param[in]   self   A valid pointer to an osn_dhcpv6_client_t object
 * @param[in]   tag    The DHCPv6 option id
 * @param[in]   value  DHCPv6 option value or NULL
 *
 * @return
 * This function returns true if the option was successfully set, false
 * otherwise.
 *
 */
bool osn_dhcpv6_client_option_send(osn_dhcpv6_client_t *self, int tag, const char *value);

/**
 * Set the DHCPv6 client status callback.
 *
 * Depending on the implementation, the status callback may be invoked
 * periodically or whenever the DHCPv6 client status change has been detected
 * (for example, when new DHCPv6 options are received).
 * For maximum portability, the callback implementation should assume it can
 * be called using either modes of operation.
 *
 * @param[in]   self  A valid pointer to an osn_ip6_t object
 * @param[in]   fn    A pointer to the function implementation
 * @param[in]   data  Private data
 */
void osn_dhcpv6_client_status_notify(
        osn_dhcpv6_client_t *self,
        osn_dhcpv6_client_status_fn_t *fn,
        void *data);

/**
 * Ensure that all configuration pertaining the @p self object is applied to
 * the running system.
 *
 * How the configuration is applied to the system is highly implementation
 * dependent. Sometimes it makes sense to cluster together several
 * configuration parameters (for example, dnsmasq uses a single config file).
 *
 * The osn_Dhcpv6_client_apply() function typically restarts the DHCPv6 client.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 */
bool osn_dhcpv6_client_apply(osn_dhcpv6_client_t *self);


/** @} OSN_DHCPV6_CLIENT */

/*
 * ===========================================================================
 *  DHCPv6 Server
 * ===========================================================================
 */

/**
 * @defgroup OSN_DHCPV6_SERVER DHCPv6 Server
 *
 * DHCPv6 Server API definition.
 *
 * @{
 */

/**
 * @struct osn_dhcpv6_server
 *
 * OSN DHCPv6 server object. The actual structure implementation is hidden
 * and is platform dependent. A new instance of the object can be obtained by
 * calling @ref osn_dhcpv6_server_new() and must be destroyed using @ref
 * osn_dhcpv6_server_del().
 */
struct osn_dhcpv6_server;
typedef struct osn_dhcpv6_server osn_dhcpv6_server_t;

/**
 * DHCPv6 prefix. This structure is used to add a prefix range to a DHCPv6
 * server configuration. It can be configured using the @ref
 * osn_dhcpv6_server_prefix_add() function and can be removed by using the
 * @ref osn_dhcpv6_server_prefix_del() function.
 */
struct osn_dhcpv6_server_prefix
{
    osn_ip6_addr_t      d6s_prefix;       /**< The DHCPv6 server prefix */
    bool                ds6_onlink;       /**< Unused */
    bool                ds6_autonomous;   /**< Unused */
};

/**
 * This structure is used to add static DHCPv6 leases. It must be added using
 * the @ref osn_dhcpv6_server_lease_add() function and can be deleted using
 * using @ref osn_dhcpv6_server_lease_del() function.
 */
struct osn_dhcpv6_server_lease
{
    osn_ip6_addr_t      d6s_addr;         /**< IPV6 address */
    char                d6s_duid[261];    /**< Client DUID */
    osn_mac_addr_t      d6s_hwaddr;       /**< Hardware address */
    char                d6s_hostname[OSN_DHCP_HOSTNAME_LEN];  /**< Hostname */
    int                 d6s_leased_time;  /**< Leased time */
};

/**
 * DHCPv6 server status report structure. A structure of this type is used
 * for reporting the status of the DHCPv6 server object. See @ref
 * osn_dhcpv6_server_notify_fn_t
 */
struct osn_dhcpv6_server_status
{
    char               *d6st_iface;       /**< Interface of DHCPv6 server instance */
    int                 d6st_leases_len;  /**< Number of active leases */
    struct osn_dhcpv6_server_lease
                       *d6st_leases;      /**< Currently active leases */
};

/**
 * DHCPv6 server status reporting callback. A function of this type can be
 * registered with @ref osn_dhcpv6_server_notify() to start receiving status
 * events from the DHCPv6 server object. The callback may be invoked before
 * the osn_dhcpv6_server_apply() function is called. This can be used to
 * report the DHCPv6 status without applying any system configuration.
 *
 * @param[in]   d6s     DHPCv6 server object
 * @param[in]   status  A structure of type osn_dhcpv6_server_status
 */
typedef void osn_dhcpv6_server_status_fn_t(
        osn_dhcpv6_server_t *d6s,
        struct osn_dhcpv6_server_status *status);

/**
 * Create a new instance of a DHCPv6 server object.
 *
 * @param[in]   ifname  Interface name to which the DHCPv6 server instance will
 *                      be bound
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_dhcpv6_server_t object is returned.
 */
osn_dhcpv6_server_t *osn_dhcpv6_server_new(const char *iface);

/**
 * Destroy a valid osn_dhcpv6_server_t object.
 *
 * @param[in]   self   A valid pointer to an osn_dhcpv6_server_t object
 *
 * @return
 * This function returns true on success. On error, false is returned.
 * The input parameter should be considered invalid after this function
 * returns, regardless of the error code.
 */
bool osn_dhcpv6_server_del(osn_dhcpv6_server_t *self);

/**
 * Ensure that all configuration pertaining the @p self object is applied to
 * the running system.
 *
 * How the configuration is applied to the system is highly implementation
 * dependent. Sometimes it makes sense to cluster together several
 * configuration parameters (for example, dnsmasq uses a single config file).
 *
 * The osn_dhcpv6_server_apply() function typically restarts the DHCPv6 server.
 *
 * @param[in]   self  A valid pointer to an osn_dhcpv6_server_t object
 *
 * @return
 * This function returns true when the configuration was successfully applied
 * to the system, false otherwise.
 */
bool osn_dhcpv6_server_apply(osn_dhcpv6_server_t *self);

/**
 * Set the DHCPv6 server object private data. The data can be used to set
 * custom data that is associated with the object.
 *
 * The private data can be retrieved with @ref osn_dhcpv6_server_data_get()
 *
 * @param[in]   self  A valid pointer to an osn_dhcpv6_server_t object
 * @param[in]   data  an opaque pointer to custom data
 */
void osn_dhcpv6_server_data_set(osn_dhcpv6_server_t *self, void *data);

/**
 * Retrieve the custom data associated with the object.
 *
 * @param[in]   self  A valid pointer to an osn_dhcpv6_server_t object
 *
 * @return
 * This function returns the pointer which was previously set with @ref
 * osn_dhcpv6_server_data_set() or NULL if no data was set.
 */
void *osn_dhcpv6_server_data_get(osn_dhcpv6_server_t *self);

/**
 * Add a DHCPv6 server prefix to the DHCPv6 server configuration. The new
 * configuration  must not take effect until osn_dhcpv6_server_apply() is called.
 *
 * @param[in]   self    A valid pointer to an osn_dhcpv6_server_t object
 * @param[in]   prefix  A pointer to a structure of type @ref osn_dhcpv6_server_prefix
 *
 * @return
 *
 * This function returns true on success, false otherwise. If this function
 * returns success when adding an existing entry, then @ref
 * osn_dhcpv6_server_prefix_del() must also return success when removing
 * a non-existing entry.
 */
bool osn_dhcpv6_server_prefix_add(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_prefix *prefix);

/**
 * Remove a DHCPv6 server prefix from the DHCPv6 server configuration. The new
 * configuration must not take effect until osn_dhcpv6_server_apply() is called.
 *
 * @param[in]   self    A valid pointer to an osn_dhcpv6_server_t object
 * @param[in]   prefix  A pointer to a structure of type @ref osn_dhcpv6_server_prefix
 *
 * @note
 *
 * Only the prefix->d6s_prefix field is used when looking up entries to be
 * removed.
 *
 * @return
 *
 * This function returns true on success, false otherwise. If this function
 * returns success when removing a non-existing entry, then @ref
 * osn_dhcpv6_server_prefix_add() must also return success adding a duplicate
 * entry.
 */
bool osn_dhcpv6_server_prefix_del(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_prefix *prefix);

/**
 * Add option with ID @p tag to the list of options that the DHCPv6 server
 * will be sending to DHCPv6 clients. The configuration may take effect only
 * after osn_dhcpv6_server_apply() is called.
 *
 * @param[in]   self   A valid pointer to an osn_dhcpv6_server_t object
 * @param[in]   tag    DHCPv6 option id
 * @param[in]   value  DHCPv6 option value or NULL to remove the option
 *
 * @return
 *
 * This function returns true if the option was successfully removed
 * or added to the DHCPv6 configuration.
 */
bool osn_dhcpv6_server_option_send(
        osn_dhcpv6_server_t *self,
        int tag,
        const char *value);

/**
 * Add a DHCPv6 client lease to the DHCPv6 server configuration. The new
 * configuration must not take effect until osn_dhcpv6_server_apply() is called.
 *
 * @param[in]   self   A valid pointer to an osn_dhcpv6_server_t object
 * @param[in]   lease  A pointer to a structure of type @ref osn_dhcpv6_server_lease
 *
 * @return
 *
 * This function returns true on success, false otherwise. If this function
 * returns success when removing a non-existing entry, then @ref
 * osn_dhcpv6_server_lease_add() must also return success adding a duplicate
 * entry.
 */
bool osn_dhcpv6_server_lease_add(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_lease *lease);

/**
 * Remove an DHCPv6 client lease from the DHCPv6 server configuration. The new
 * configuration must not take effect until osn_dhcpv6_server_apply() is called.
 *
 * @param[in]   self   A valid pointer to an osn_dhcpv6_server_t object
 * @param[in]   lease  A pointer to a structure of type
 *                     @ref osn_dhcpv6_server_lease. This structure defines
 *                     the entry that will be removed.
 *
 * @return
 *
 * This function returns true on success, false otherwise. If this function
 * returns success when removing a non-existing entry, then @ref
 * osn_dhcpv6_server_lease_add() must also return success adding a duplicate
 * entry.
 */
bool osn_dhcpv6_server_lease_del(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_lease *lease);

/**
 * Register a DHCPv6 server status notification callback. If @p fn
 * is NULL, the previous callback is unregistered.
 *
 * @param[in]   self  A valid pointer to an osn_dhcpv6_server_t object
 * @param[in]   fn    A function of type @ref osn_dhcpv6_server_status_fn_t
 *
 * @return
 *
 * This function returns true if the callback was successfully registered,
 * false otherwise.
 */
bool osn_dhcpv6_server_status_notify(
        osn_dhcpv6_server_t *self,
        osn_dhcpv6_server_status_fn_t *fn);

/** @} OSN_DHCPV6_SERVER */
/** @} OSN_DHCPV6 */
/** @} OSN_IPV6 */
/** @} OSN */

#endif /* OSN_DHCPV6_H_INCLUDED */
