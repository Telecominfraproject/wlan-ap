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

/**
 */
#ifndef OSN_DHCP_H_INCLUDED
#define OSN_DHCP_H_INCLUDED

#include <stdbool.h>

#include "const.h"

#include "osn_types.h"

/**
 * @file osn_dhcp.h
 * @brief OpenSync DHCPv4
 *
 * @addtogroup OSN
 * @{
 *
 * @addtogroup OSN_IPV4
 * @{
 */

/*
 * ===========================================================================
 *  DHCP common definitions
 * ===========================================================================
 */

/**
 * @defgroup OSN_DHCPV4 DHCPv4
 *
 * Common DHCPv4 API definitions.
 *
 * @{
 */

/**
 * List update protocol
 *
 *      NOTIFY_UPDATE   - Report a new or update a current entry; during SYNC/FLUSH cycled un-flag entry for deletion
 *      NOTIFY_DELETE   - Delete entry
 *
 *      NOTIFY_SYNC     - Start synchronization cycle, flag all entries for deletion
 *      NOTIFY_FLUSH    - Flush all entries flagged for deletion
 *
 * @note
 * Obsolete.
 */
enum osn_notify
{
    NOTIFY_UPDATE,
    NOTIFY_DELETE,
    NOTIFY_SYNC,
    NOTIFY_FLUSH
};

/**
 * DHCP option list.
 *
 * @note
 * This list is incomplete. Please find a full list of DHCP options on:
 * https://www.iana.org/assignments/bootp-dhcp-parameters/bootp-dhcp-parameters.xhtml
 */
enum osn_dhcp_option
{
    DHCP_OPTION_SUBNET_MASK = 1,
    DHCP_OPTION_ROUTER = 3,
    DHCP_OPTION_DNS_SERVERS = 6,
    DHCP_OPTION_HOSTNAME = 12,
    DHCP_OPTION_DOMAIN_NAME = 15,
    DHCP_OPTION_BCAST_ADDR = 28,
    DHCP_OPTION_VENDOR_SPECIFIC = 43,
    DHCP_OPTION_ADDRESS_REQUEST = 50,
    DHCP_OPTION_LEASE_TIME = 51,
    DHCP_OPTION_MSG_TYPE = 53,
    DHCP_OPTION_PARAM_LIST = 55,
    DHCP_OPTION_VENDOR_CLASS = 60,
    DHCP_OPTION_DOMAIN_SEARCH= 119,
    DHCP_OPTION_PLUME_SWVER = 225,
    DHCP_OPTION_PLUME_PROFILE = 226,
    DHCP_OPTION_PLUME_SERIAL_OPT = 227,
    DHCP_OPTION_MAX = 256
};

/**
 * Maximum size of a DHCP fingerprint, including the ending \0
 */
#define OSN_DHCP_FINGERPRINT_MAX    256

/**
 * Maximum size of a DHCP vendor class string, including the ending \0
 */
#define OSN_DHCP_VENDORCLASS_MAX    256


/*
 * ===========================================================================
 *  DHCP client definitions
 * ===========================================================================
 */

/**
 * @defgroup OSN_DHCPV4_CLIENT DHCPv4 Client
 *
 * DHCPv4 Client API definitions and functions
 *
 * @{
 */

/**
 * DHCP client options reporting callback -- this callback will be triggered
 * when new DHCP options are received by the DHCP client.
 */
typedef bool osn_dhcp_client_opt_notify_fn_t(
        void *ctx,
        enum osn_notify hint,
        const char *key,
        const char *value);


typedef struct osn_dhcp_client osn_dhcp_client_t;
typedef void osn_dhcp_client_error_fn_t(osn_dhcp_client_t *self);

osn_dhcp_client_t *osn_dhcp_client_new(const char *ifname);
bool osn_dhcp_client_del(osn_dhcp_client_t *self);
bool osn_dhcp_client_start(osn_dhcp_client_t *self);
bool osn_dhcp_client_stop(osn_dhcp_client_t *self);

/* Add this option to the server request options, if none is specified a default set will be sent */
bool osn_dhcp_client_opt_request(osn_dhcp_client_t *self, enum osn_dhcp_option opt, bool request);
/* Set a DHCP client option -- these will be sent to the server */
bool osn_dhcp_client_opt_set(osn_dhcp_client_t *self, enum osn_dhcp_option opt, const char *value);
/* Retrieve DHCP option request status and set value (if any) */
bool osn_dhcp_client_opt_get(osn_dhcp_client_t *self, enum osn_dhcp_option opt, bool *request, const char **value);
/* Set the option reporting callback */
bool osn_dhcp_client_opt_notify_set(osn_dhcp_client_t *self, osn_dhcp_client_opt_notify_fn_t *fn, void *ctx);
/* Error callback, called whenever an error occurs on the dhcp client (sudden termination or otherwise) */
bool osn_dhcp_client_error_fn_set(osn_dhcp_client_t *self, osn_dhcp_client_error_fn_t *fn);
/* Set the vendor class */
bool osn_dhcp_client_vendorclass_set(osn_dhcp_client_t *self, const char *vendorspec);
/* Get the current active state of the DHCP client */
bool osn_dhcp_client_state_get(osn_dhcp_client_t *self, bool *enabled);

/** @} */

/*
 * ===========================================================================
 *  DHCP server definitions
 * ===========================================================================
 */

/**
 * @defgroup OSN_DHCPV4_SERVER DHCPv4 Server
 *
 * DHCPv4 Server API definitions and functions
 *
 * @{
 */

/**
 * @struct osn_dhcp_server
 *
 * OSN DHCPv4 object. The actual structure implementation is hidden and is
 * platform dependent. An new instance of the object can be obtained by calling
 * @ref osn_dhcp_server_new() and must be destroyed using @ref osn_dhcp_server_del().
 */
struct osn_dhcp_server;

typedef struct osn_dhcp_server osn_dhcp_server_t;

/**
 * DHPv4 server configuration parameters. This structure can be used to modify
 * default parameters used by the DHCPv4 server. The structure must be
 * initialized using the @ref OSN_DHCP_SERVER_CFG_INIT initializer. The
 * new configuration can be applied to the DHCPv4 object by calling
 * @ref osn_dhcp_server_cfg_set().
 */
struct osn_dhcp_server_cfg
{
    int ds_lease_time;      /**< Default lease time in seconds */
};

/**
 * Initializer for the @p osn_dhcp_server_cfg structure.
 */
#define OSN_DHCP_SERVER_CFG_INIT (struct osn_dhcp_server_cfg)   \
{                                                               \
    .ds_lease_time = -1,                                        \
}

/**
 * This structure is used for reporting DHCP lease information. Typically
 * reported as an array inside osn_dhcp_server_status.
 */
struct osn_dhcp_server_lease
{
    osn_mac_addr_t      dl_hwaddr;                                  /**< Client hardware address */
    osn_ip_addr_t       dl_ipaddr;                                  /**< Client IPv4 address */
    char                dl_hostname[C_HOSTNAME_LEN];                /**< Client hostname */
    char                dl_fingerprint[OSN_DHCP_FINGERPRINT_MAX];   /**< DHCP fingerprint information */
    char                dl_vendorclass[OSN_DHCP_VENDORCLASS_MAX];   /**< Vendor class information */
    double              dl_leasetime;                               /**< Lease time in seconds */
};

/**
 * Initializer for osn_dhcp_server_lease. This macro must be used to initialize
 * new instances of struct @ref osn_dhcp_server_lease
 */
#define OSN_DHCP_SERVER_LEASE_INIT (struct osn_dhcp_lease_info) \
{                                                               \
    .dl_hwaddr = OSN_MAC_ADDR_INIT,                             \
    .dl_ipaddr = OSN_IP_ADDR_INIT,                              \
    .dl_leasetime = -1.0,                                       \
}

/**
 * DHCPv4 server status structure.
 */
struct osn_dhcp_server_status
{
    char                            ds_iface;           /**< Interface name */
    struct osn_dhcp_server_lease   *ds_leases;          /**< Leases array */
    int                             ds_leases_len;      /**< Leases length */
};

/**
 * osn_dhcp_server_t status notification callback. This function will be invoked
 * whenever the osn_dhcp_server_t object wishes to report the DHCPv4 server
 * status.
 *
 * Typically this will happen whenever a DHCPv4 status change is detected (for
 * example, when a DHCP IP lease has been given out).
 *
 * Some implementation may choose to call this function periodically even if
 * there has been no status change detected.
 *
 * @param[in]   self    A valid pointer to an osn_dhcp_server_t object
 * @param[in]   status  A pointer to a @ref osn_dhcp_server status
 */
typedef void osn_dhcp_server_status_fn_t(
        osn_dhcp_server_t *self,
        struct osn_dhcp_server_status *status);


/**
 * osn_dhcp_server_t error callback definition.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 */
typedef void osn_dhcp_server_error_fn_t(osn_dhcp_server_t *self);

/**
 * Create a new instance of a DHCPv4 server object.
 *
 * @param[in]   ifname  Interface name to which the server instance will be
 *                      bound to
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_dhcp_server_t object is returned.
 */
osn_dhcp_server_t *osn_dhcp_server_new(const char *ifname);

/**
 * Destroy a valid osn_dhcp_server_t object.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
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
bool osn_dhcp_server_del(osn_dhcp_server_t *self);

/**
 * Set the object @p self private data.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 * @param[in]   data  Pointer to private data
 */
void osn_dhcp_server_data_set(osn_dhcp_server_t *self, void *data);

/**
 * Get the object @p self private data. If no private data was set, NULL will
 * be returned.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 *
 * @return
 * Returns a pointer to private data previously set using @ref
 * osn_dhcp_server_data_set()
 */
void *osn_dhcp_server_data_get(osn_dhcp_server_t *self);

/**
 * Set DHCPv4 server configuration options.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 * @param[in]   cfg   A pointer to DHCPv4 configuration structure
 *
 * @return
 * This function returns true on success. Error is returned in case one or more
 * parameters were found to be invalid or out of range. In such cases the
 * configuration may have been partially applied.
 *
 * @note
 * osn_dhcp_server_apply() must be called before this change can take effect.
 */
bool osn_dhcp_server_cfg_set(osn_dhcp_server_t *self, struct osn_dhcp_server_cfg *cfg);

/**
 * Add a DHCPv4 IP range to the server's lease pool. Many IP ranges can be
 * specified.
 * The caller must make sure the IP ranges do not overlap.
 *
 * @param[in]   self   A valid pointer to an osn_dhcp_server_t object
 * @param[in]   start  First IP in range
 * @param[in]   stop   Last IP in range
 *
 * @return
 * This function returns true on success. False otherwise.
 *
 * @note
 * osn_dhcp_server_apply() must be called before this change can take effect.
 */
bool osn_dhcp_server_range_add(osn_dhcp_server_t *self, osn_ip_addr_t start, osn_ip_addr_t stop);

/**
 * Remove a DHCPv4 IP range from the server lease pool. An IP range must have
 * been previously registered with @ref osn_dhcp_server_range_del().
 *
 * It is not possible to slice IP ranges. The @p start and @p stop parameters
 * must match the ones specified to @ref osn_dhcp_server_range_add().
 *
 * @param[in]   self   A valid pointer to an osn_dhcp_server_t object
 * @param[in]   start  First IP in range
 * @param[in]   stop   Last IP in range
 *
 * @return
 * This function returns true on success. False otherwise.
 *
 * @note
 * osn_dhcp_server_apply() must be called before this change can take effect.
 */
bool osn_dhcp_server_range_del(osn_dhcp_server_t *self, osn_ip_addr_t start, osn_ip_addr_t stop);

/**
 * Set a DHCPv4 option value. The option will be sent to the client during the
 * DHCP-OFFER phase.
 *
 * If an option has already a value assigned, it will be overwritten with the
 * new value.
 *
 * Using a value of NULL will remove the option from the option list.
 *
 * @param[in]   self   A valid pointer to an osn_dhcp_server_t object
 * @param[in]   opt    A DHCPv4 option tag
 * @param[in]   value  Option value as string -- binary options may require
 *                     the string to be base64 encoded
 *
 * @returns
 * This function returns true on success. False otherwise.
 *
 * @note
 * osn_dhcp_server_apply() must be called before this change can take effect.
 */
bool osn_dhcp_server_option_set(osn_dhcp_server_t *self, enum osn_dhcp_option opt, const char *value);

/**
 * Set the DHCPv4 server error callback.
 *
 * The error callback is invoked whenever an error condition is detected during
 * run-time (for * example, when the server unexpectedly dies).
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 * @param[in]   fn    A pointer to the function implementation
 *
 * @note
 * The callback may be invoked only after a successful call to a @ref
 * osn_dhcp_server_apply() function.
 */
void osn_dhcp_server_error_notify(osn_dhcp_server_t *self, osn_dhcp_server_error_fn_t *fn);

/**
 * Set the DHCPv4 server status callback.
 *
 * Depending on the implementation, the status callback may be invoked
 * periodically or whenever a DHCP server status change has been detected.
 * For maximum portability, the callback implementation should assume it can
 * be called using either modes of operation.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 * @param[in]   fn    A pointer to the function implementation
 */
void osn_dhcp_server_status_notify(osn_dhcp_server_t *self, osn_dhcp_server_status_fn_t *fn);

/**
 * Add a DHCPv4 client IP reservation entry.
 *
 * @param[in]   self      A valid pointer to an osn_dhcp_server_t object
 * @param[in]   macaddr   The client MAC address
 * @param[in]   ip4addr   The client reserved IP address
 * @param[in]   hostname  Desired client hostname - can be NULL if unknown
 *
 * @return
 * This function returns true on success, false otherwise.
 *
 * @note
 * osn_dhcp_server_apply() must be called before this change can take effect.
 */
bool osn_dhcp_server_reservation_add(
        osn_dhcp_server_t *self,
        osn_mac_addr_t macaddr,
        osn_ip_addr_t ip4addr,
        const char *hostname);

/**
 * Remove a DHCPv4 IP reservation entry.
 *
 * @param[in]   self     A valid pointer to an osn_dhcp_server_t object
 * @param[in]   macaddr  The client MAC address
 *
 * @return
 * This function returns true on success, false otherwise.
 *
 * @note
 * osn_dhcp_server_apply() must be called before this change can take effect.
 */
bool osn_dhcp_server_reservation_del(
        osn_dhcp_server_t *self,
        osn_mac_addr_t macaddr);

/**
 * Ensure that all configuration pertaining the @p self object is applied to
 * the running system.
 *
 * How the configuration is applied to the system is highly implementation
 * dependent. Sometimes it makes sense to cluster together several
 * configuration parameters (for example, dnsmasq uses a single config file).
 *
 * osn_dhcp_server_apply() makes sure that a write operation is initiated for
 * all currently cached (dirty) configuration data.
 *
 * @param[in]   self  A valid pointer to an osn_dhcp_server_t object
 *
 * @note It is not guaranteed that the configuration will be applied as soon
 * as osn_dhcp_server_apply() returns -- only that the configuration process
 * will be started for all pending operations.
 */
bool osn_dhcp_server_apply(osn_dhcp_server_t *self);

/** @} OSN_DHCPV4_SERVER  */
/** @} OSN_DHCPV4 */
/** @} OSN_IPV4 */
/** @} OSN */

#endif /* OSN_DHCP_H_INCLUDED */
