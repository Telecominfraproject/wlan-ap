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

#ifndef __NET_HEADER_PARSE_H__
#define __NET_HEADER_PARSE_H__

#include <pcap.h>
#include <stdint.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

#include "log.h"
#include "os_types.h"

/**
 * @brief indications from a plugin to FSM about how to proceed with a flow
 *
 * FLOW_NA is the default value FSM  sets when presenting a packet to the plugin.
 * The plugin is expected to use the other values.
 */
enum
{
    FLOW_NA = 0,
    FLOW_INSPECT,
    FLOW_PASSTHROUGH,
    FLOW_DROP,
};


/**
 * @brief container for traffic tags
 */
struct flow_tags
{
    char *vendor;
    char *app_name;
    size_t nelems;
    char **tags;
};


/**
 * @brief container for parsed ethernet header information
 */
struct eth_header
{
    os_macaddr_t *dstmac;
    os_macaddr_t *srcmac;
    uint16_t ethertype;
    uint16_t vlan_id;
};


/**
 * @brief container for parsed pcap data
 */
struct net_header_parser
{
    int pcap_datalink;
    size_t packet_len;
    size_t parsed;
    struct eth_header eth_header;
    int ip_version;
    int ip_protocol;
    union
    {
        uint8_t *payload;
        union
        {
            struct iphdr *iphdr;
            struct ip6_hdr *ipv6hdr;
        } ip;
    } eth_pld;
    union
    {
        void *payload;
        struct udphdr *udphdr;
        struct tcphdr *tcphdr;
    } ip_pld;
    uint8_t *data;
    uint8_t *start;
    size_t caplen;
    int flow_action;
    struct flow_tags tags;
};


/**
 * @brief parses the network header parts of a message
 *
 * @param parser the parsed data container
 * @return the size of the parsed message, or 0 if the advertized size is bigger
 *         than the captured size.
 */
size_t net_header_parse(struct net_header_parser *parser);


/**
 * @brief parse the ethernet header of a pcap capture
 *
 * @param parser the parsed data container
 * @return the buffer offset passed the ethernet header if successful,
 *         0 otherwise
 */
size_t net_header_parse_eth(struct net_header_parser *parser);


/**
 * @brief access the ethernet header of a pcap capture
 *
 * @param parser the parsed data container
 * @return a pointer to the ethernet header if successful,
 *         NULL otherwise
 */
struct eth_header * net_header_get_eth(struct net_header_parser *parser);


/**
 * @brief get parsed ethertype
 *
 * @param parser the parsed data container
 * @return the parsed ethertype
 */
uint16_t net_header_get_ethertype(struct net_header_parser *parser);


/**
 * @brief parse the ip header of a pcap capture
 *
 * @param parser the parsed data container
 * @return the buffer offset passed the ip header if successful,
 *         0 otherwise
 */
size_t net_header_parse_ip(struct net_header_parser *parser);


/**
 * @brief parse the ipv4 header of a pcap capture
 *
 * @param parser the parsed data container
 * @return the buffer offset passed the ipv4 header if successful,
 *         0 otherwise
 */
size_t net_header_parse_ipv4(struct net_header_parser *parser);


/**
 * @brief parse the ipv6 header of a pcap capture
 *
 * @param parser the parsed data container
 * @return the buffer offset passed the ipv6 header if successful,
 *         0 otherwise
 */
size_t net_header_parse_ipv6(struct net_header_parser *parser);


/**
 * @brief hop through IPv6 extensions headers
 *
 * @param parser the parsed data container
 * @return the buffer offset passed the ipv6 header extensions if successful,
 *         0 otherwise
 */
size_t net_header_get_ipv6_payload(struct net_header_parser *parser);


/**
 * @brief access the parser's ipv4 information
 *
 * @param parser the parsed data container
 * @return a pointer to the ipv6 info if present, NULL otherwise
 */
struct iphdr * net_header_get_ipv4_hdr(struct net_header_parser *parser);


/**
 * @brief access the parser's ipv6 information
 *
 * @param parser the parsed data container
 * @return a pointer to the ipv6 info if present, NULL otherwise
 */
struct ip6_hdr * net_header_get_ipv6_hdr(struct net_header_parser *parser);


/**
 * @brief write the requested ip address as a string in the provided buffer
 *
 * The caller must provide a buffer with enough capacity.
 * @param parser the parsed data container
 * @param bool src reqesting source IP address if true, dest IP address if false
 * @param ip_buf the buffer to be filled with the ip as a string
 * @param ip_buf_len the provided buffer length
 * @return true if the conversion succeeded, false otherwise
 */
bool net_header_ip_str(struct net_header_parser *parser, bool src,
                       char *ip_buf, size_t ip_buf_len);


/**
 * @brief write the parsed ip source address as a string in the provided buffer
 *
 * The caller must provide a buffer with enough capacity.
 * @param parser the parsed data container
 * @param ip_buf the buffer to be filled with the source ip as a string
 * @param ip_buf_len the provided buffer length
 * @return true if the conversion succeeded, false otherwise
 */
bool net_header_srcip_str(struct net_header_parser *parser, char *ip_buf,
                          size_t ip_buf_len);


/**
 * @brief write the ip destination address as a string in the provided buffer
 *
 * The caller must provide a buffer with enough capacity.
 * @param parser the parsed data container
 * @param ip_buf the buffer to be filled with the destination ip as a string
 * @param ip_buf_len the provided buffer length
 * @return true if the conversion succeeded, false otherwise
 */
bool net_header_dstip_str(struct net_header_parser *parser, char *ip_buf,
                          size_t ip_buf_len);


/**
 * @brief parse the tcp header of a pcap capture
 *
 * @param parser the parsed data container
 * @return the buffer offset passed the tcp header if successful,
 *         0 otherwise
 */
size_t net_header_parse_tcp(struct net_header_parser *parser);


/**
 * @brief parse the udp header of a pcap capture
 *
 * @param parser the parsed data container
 * @return the buffer offset passed the udp header if successful,
 *         0 otherwise
 */
size_t net_header_parse_udp(struct net_header_parser *parser);


/**
 * @brief logs the network header info
 *
 * @param log_level the requested log level
 * @param parser the parsed data container
 * @return none
 *
 * Logs the ethernet, ip and transport info when available
 * at the requested log level
 */
void
net_header_log(int log_level, struct net_header_parser *parser);


/**
 * @brief logs the network header contetn at LOG_SEVERITY_INFO level
 *
 * @param parser the parsed data container
 * @return none
 *
 * Logs the ethernet, ip and transport info when available
 */
static inline void
net_header_logi(struct net_header_parser *parser)
{
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_INFO))
    {
        net_header_log(LOG_SEVERITY_INFO, parser);
    }
}


/**
 * @brief logs the network header content at LOG_SEVERITY_DEBUG level
 *
 * @param parser the parsed data container
 * @return none
 *
 * Logs the ethernet, ip and transport info when available
 */
static inline void
net_header_logd(struct net_header_parser *parser)
{
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_DEBUG))
    {
        net_header_log(LOG_SEVERITY_DEBUG, parser);
    }
}


/**
 * @brief logs the network header content at LOG_SEVERITY_TRACE level
 *
 * @param parser the parsed data container
 * @return none
 *
 * Logs the ethernet, ip and transport info when available
 */
static inline void
net_header_logt(struct net_header_parser *parser)
{
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
    {
        net_header_log(LOG_SEVERITY_TRACE, parser);
    }
}

/**
 * @brief fills the given string with the network header content
 *
 * @param buf the buffer to fill
 * @param len the buffer length
 * @param parser the parsed data container
 * @return a pointer to the buffer passed in argument
 *
 * Fills the buffer with the ethernet, ip and transport info when available
 */
char *
net_header_fill_buf(char *buf, size_t len, struct net_header_parser *parser);


/**
 * @brief fills the given string with the network header content
 *
 * @param buf the buffer to fill
 * @param len the buffer length
 * @param parser the parsed data container
 * @return a pointer to the buffer passed in argument
 *
 * Fills the buffer with the ethernet, ip and transport info when available
 * if the info log level is enabled
 */
static inline char *
net_header_fill_info_buf(char *buf, size_t len,
                         struct net_header_parser *parser)
{
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_INFO))
    {
        return net_header_fill_buf(buf, len, parser);
    }
    else
    {
        return "INFO log level not enabled";
    }
}


/**
 * @brief fills the given string with the network header content
 *
 * @param buf the buffer to fill
 * @param len the buffer length
 * @param parser the parsed data container
 * @return a pointer to the buffer passed in argument
 *
 * Fills the buffer with the ethernet, ip and transport info when available
 * if the debug log level is enabled
 */
static inline char *
net_header_fill_debug_buf(char *buf, size_t len,
                         struct net_header_parser *parser)
{
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_DEBUG))
    {
        return net_header_fill_buf(buf, len, parser);
    }
    else
    {
        return "DEBUG log level not enabled";
    }
}


/**
 * @brief fills the given string with the network header content
 *
 * @param buf the buffer to fill
 * @param len the buffer length
 * @param parser the parsed data container
 * @return a pointer to the buffer passed in argument
 *
 * Fills the buffer with the ethernet, ip and transport info when available
 * if the trace log level is enabled
 */
static inline char *
net_header_fill_trace_buf(char *buf, size_t len,
                         struct net_header_parser *parser)
{
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
    {
        return net_header_fill_buf(buf, len, parser);
    }
    else
    {
        return "TRACE log level not enabled";
    }
}

#endif /* __NET_HEADER_PARSE_H__ */
