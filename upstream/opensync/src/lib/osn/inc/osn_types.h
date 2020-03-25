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

#if !defined(OSN_TYPES_H_INCLUDED)
#define OSN_TYPES_H_INCLUDED

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/**
 * @file osn_types.h
 * @brief OpenSync Networking Common Types
 *
 * @defgroup OSN OpenSync Networking
 *
 * OpenSync Networking API
 *
 * @{
 */

/*
 * ===========================================================================
 *  Support functions for various types in OpenSync networking
 *  - string conversion functions
 *  - comparators
 *  - misc
 * ===========================================================================
 */

/**
 * @defgroup OSN_COMMON Common API and Types
 *
 * Common OpenSync Networking API and types
 *
 * @{
 */

/*
 * ===========================================================================
 *  IPv4 types
 * ===========================================================================
 */

/**
 * @defgroup OSN_COMMON_osn_ip_addr_t osn_ip_addr_t
 *
 * IPv4 Address types and associated functions.
 *
 * @{
 */

/**
 * IPv4 address definition; this includes the netmask (prefix).
 *
 * A negative prefix indicates that the prefix is not present. Note that a
 * prefix of 0 is valid (for example, the default route).
 *
 * This structure should not be accessed directly.
 *
 * Use @ref OSN_IP_ADDR_INIT to initialize this structure.
 */
struct osn_ip_addr
{
    struct in_addr      ia_addr;        /**< IPv4 Address */
    int                 ia_prefix;      /**< Netmask in /XX notation */
};

typedef struct osn_ip_addr osn_ip_addr_t;

/**
 * Initializer for a IPv4 address structure (@ref osn_ip_addr_t)
 */
#define OSN_IP_ADDR_INIT (osn_ip_addr_t)    \
{                                           \
    .ia_prefix = -1,                        \
}

/**
 * Maximum length of a IPv4 Address structure when expressed as a string,
 * including the terminating \0
 */
#define OSN_IP_ADDR_LEN     sizeof("255.255.255.255/32")

/**
 * Macro helpers for printf() formatting. The PRI_ macro can be used in
 * conjunction with the FMT_ macro to print IPv4 addresses.
 *
 * Examples:
 *
 * @code
 * osn_ip_addr_t my_ipaddr;
 *
 * printf("Hello. The IP address is: "PRI_osn_ip_addr"\n", FMT_osn_ip_addr(my_ipaddr));
 * @endcode
 */
#define PRI_osn_ip_addr         "%s"

/**
 * Macro helper for printf() formatting. See @ref PRI_osn_ip_addr for more
 * info.
 */
#define FMT_osn_ip_addr(x)      (__FMT_osn_ip_addr((char[OSN_IP_ADDR_LEN]){0}, OSN_IP_ADDR_LEN, &x))

char *__FMT_osn_ip_addr(char *buf, size_t sz, const osn_ip_addr_t *addr);

/**
 * Initialize a osn_ip_addr_t from a string. The valid string formats are:
 *
 * "NN.NN.NN.NN"
 *
 * or
 *
 * "NN.NN.NN.NN/NN"
 *
 * @param[in]   out  Output osn_ip_addr_t structure
 * @param[in]   str  Input string
 *
 * @return
 * This function returns true if @p str is valid and was successfully parsed,
 * false otherwise. If false is returned, @p out should be considered invalid.
 */
bool osn_ip_addr_from_str(osn_ip_addr_t *out, const char *str);

/**
 * Comparator for @ref osn_ip_addr_t structures.
 *
 * @param[in]   a  First osn_ip_addr_t to compare
 * @param[in]   b  Second osn_ip_addr_t to compare
 *
 * @return
 * This function returns an integer less than, equal to, or greater than zero
 * if @p a is found, respectively, to be less than, to match, or be
 * greater than @p b.
 */
int osn_ip_addr_cmp(void *a, void *b);

/**
 * Strip the non-subnet part of an IP address. For example:
 *
 * @code
 * 192.168.40.1/24 -> 192.168.40.0/24
 * @endcode
 *
 * @param[in]   addr  Address to convert
 *
 * @return
 * Returns a osn_ip_addr_t structure that has its non-subnet part set to all
 * zeroes
 */
osn_ip_addr_t osn_ip_addr_subnet(osn_ip_addr_t *addr);

/**
 * Converts a subnet IP representation to a prefix integer.
 * For example:
 *
 * @code
 * 255.255.255.0 -> 24
 * @endcode
 *
 * @param[in]   addr  Input address
 *
 * @return
 *
 * Returns the number of consecutive bits set in @p addr
 */
int osn_ip_addr_to_prefix(osn_ip_addr_t *addr);

/** @} OSN_COMMON_osn_ip_addr_t */

/*
 * ===========================================================================
 *  IPv6 types
 * ===========================================================================
 */

/**
 * @defgroup OSN_COMMON_osn_ip6_addr_t osn_ip6_addr_t
 *
 * IPv6 Address types and associated functions.
 *
 * @{
 */

/**
 * IPv6 Address definition; this includes the prefix and lifetimes.
 *
 * If the prefix is -1, it should be considered not present.
 *
 * If a lifetime is set to INT_MIN, it should be considered absent, while
 * a value of -1 means infinite.
 *
 * Use OSN_IP6_ADDR_INIT to initialize this structure to default values.
 */
struct osn_ip6_addr
{
    struct in6_addr     ia6_addr;           /* Global IP address */
    int                 ia6_prefix;         /* IP prefix -- usually 64 */
    int                 ia6_pref_lft;       /* Preferred lifetime in second - negative value mean infinite */
    int                 ia6_valid_lft;      /* valid lifetime in second - negative value mean infinite */
};

typedef struct osn_ip6_addr osn_ip6_addr_t;

/**
 * Initializer for a osn_ip6_addr_t structure.
 */
#define OSN_IP6_ADDR_INIT (osn_ip6_addr_t)  \
{                                           \
    .ia6_prefix = -1,                       \
    .ia6_pref_lft = INT_MIN,                \
    .ia6_valid_lft = INT_MIN,               \
}

/**
 * Maximum length of IPv6 Address structure when expressed as a string,
 * including the terminating \0
 */
#define OSN_IP6_ADDR_LEN sizeof("1111:2222:3333:4444:5555:6666:7777:8888/128,2147483648,2147483648")

/**
 * Macro helpers for printf() formatting. The PRI_ macro can be used in
 * conjunction with the FMT_ macro to print IPv6 addresses.
 *
 * Examples:
 *
 * @code
 * osn_ip6_addr_t my_ip6addr;
 *
 * printf("Hello. The IPv6 address is: "PRI_osn_ip6_addr"\n", FMT_osn_ip6_addr(my_ipaddr));
 * @endcode
 */
#define PRI_osn_ip6_addr        "%s"

/**
 * Macro helper for printf() formatting. See @ref PRI_osn_ip6_addr for more
 * info.
 */
#define FMT_osn_ip6_addr(x)     (__FMT_osn_ip6_addr((char[OSN_IP6_ADDR_LEN]){0}, OSN_IP6_ADDR_LEN, &x))

/*
 * Functions
 */
char *__FMT_osn_ip6_addr(char *buf, size_t sz, const osn_ip6_addr_t *addr);

/**
 * Initialize a osn_ip6_addr_t from a string. The valid string formats are:
 *
 * IPV6_ADDR/PREFIX,MIN_LFT,MAX_LFT
 *
 * - IPV6_ADDR - Anything that inet_pton(AF_INET6, ...) can understand
 * - PREFIX    - An integer between 1 and 64 bits, a value of -1 means that
 *               the prefix is not present
 * - MIN_LFT   - An integer representing the minimum lifetime in seconds
 * - MAX_LFT   - An integer representing the maximum lifetime in seconds
 *
 * A value of -1 for MIN_LFT and MAX_LFT means infinite lifetime.
 * A value of INT_MIN for MIN_LFT and MAX_LFT means that the lifetime is not
 * present.
 *
 * @param[in]   out  Output osn_ip6_addr_t structure
 * @param[in]   str  Input string
 *
 * @return
 * This function returns true if @p str is valid and was successfully parsed,
 * false otherwise. If false is returned, @p out should be considered invalid.
 */
bool osn_ip6_addr_from_str(osn_ip6_addr_t *out, const char *str);

/**
 * Comparator for @ref osn_ip6_addr_t structures.
 *
 * @param[in]   a  First osn_ip6_addr_t to compare
 * @param[in]   b  Second osn_ip6_addr_t to compare
 *
 * @return
 * This function returns an integer less than, equal to, or greater than zero
 * if @p a is found, respectively, to be less than, to match, or be
 * greater than @p b.
 */
int osn_ip6_addr_cmp(void *a, void *b);

/**
 * Comparator for @ref osn_ip6_addr_t structures. This version ignores the
 * IPv6 address lifetimes.
 *
 * @param[in]   a  First osn_ip6_addr_t to compare
 * @param[in]   b  Second osn_ip6_addr_t to compare
 *
 * @return
 * This function returns an integer less than, equal to, or greater than zero
 * if @p a is found, respectively, to be less than, to match, or be
 * greater than @p b.
 */
int osn_ip6_addr_nolft_cmp(void *_a, void *_b);

/** @} OSN_COMMON_osn_ip6_addr_t */

/*
 * ===========================================================================
 *  Other types
 * ===========================================================================
 */

/**
 * @defgroup OSN_COMMON_osn_mac_addr_t osn_mac_addr_t
 *
 * Hardware Address (MAC) types and associated functions.
 *
 * @{
 */

/**
 * MAC address definition. It is advisable that this structure
 * is never used directly but through osn_mac_addr_* functions.
 */
struct osn_mac_addr
{
    uint8_t             ma_addr[6];
};

typedef struct osn_mac_addr osn_mac_addr_t;

/**
 * Maximum length of MAC Address structure when expressed as a string,
 * including the terminating \0
 *
 * This structure should be initialized with @ref OSN_MAC_ADDR_INIT before use.
 */
#define OSN_MAC_ADDR_LEN sizeof("11:22:33:44:55:66")

/**
 * Initializer for a MAC address structure (@ref osn_mac_addr_t)
 */
#define OSN_MAC_ADDR_INIT (osn_mac_addr_t){ .ma_addr = { 0 }, }

/**
 * Macro helpers for printf() formatting. The PRI_ macro can be used in
 * conjunction with the FMT_ macro to print MAC addresses.
 *
 * Examples:
 *
 * @code
 * osn_mac_addr_t my_hwaddr;
 *
 * printf("Hello. The MAC address is: "PRI_osn_mac_addr"\n", FMT_osn_mac_addr(my_macaddr));
 * @endcode
 */
#define PRI_osn_mac_addr        "%02x:%02x:%02x:%02x:%02x:%02x"

/**
 * Macro helper for printf() formatting. See @ref PRI_osn_mac_addr for more
 * info.
 */
#define FMT_osn_mac_addr(x)     (x).ma_addr[0], \
                                (x).ma_addr[1], \
                                (x).ma_addr[2], \
                                (x).ma_addr[3], \
                                (x).ma_addr[4], \
                                (x).ma_addr[5]

/**
 * Initialize a osn_mac_addr_t from a string. The valid string formats are:
 *
 * "XX:XX:XX:XX:XX:XX"
 *
 * @param[in]   out  Output osn_mac_addr_t structure
 * @param[in]   str  Input string
 *
 * @return
 * This function returns true if @p str is valid and was successfully parsed,
 * false otherwise. If false is returned, @p out should be considered invalid.
 */
bool osn_mac_addr_from_str(osn_mac_addr_t *out, const char *str);

/**
 * Comparator for @ref osn_mac_addr_t structures.
 *
 * @param[in]   a  First osn_mac_addr_t to compare
 * @param[in]   b  Second osn_mac_addr_t to compare
 *
 * @return
 * This function returns an integer less than, equal to, or greater than zero
 * if @p a is found, respectively, to be less than, to match, or be
 * greater than @p b.
 */
int osn_mac_addr_cmp(void *_a, void *_b);

/** @} OSN_COMMON_osn_mac_addr_t */

/** @} OSN_COMMON */

/** @} OSN */

#endif /* OSN_TYPES_H_INCLUDED */
