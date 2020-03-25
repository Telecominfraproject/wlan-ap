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

#include <arpa/inet.h>
#include <pcap.h>
#include <stdbool.h>
#include <time.h>

#include "os_types.h"

#ifndef __BP_NETWORK
#define __BP_NETWORK

/*
 * Verbosity flags. Switch which function is defined to add or remove
 * various output printfs from the source. These are all for debugging
 * purposes.
 */
/* #define VERBOSE(A) A */
#define VERBOSE(A)
/* #define DBG(A) A fflush(stdout); */
#define DBG(A)
/* #define SHOW_RAW(A) A */
#define SHOW_RAW(A)

/*
 * There are a lot of DBG statements in the tcp and ip_fragment sections.
 * When debugging those areas, it's really nice to know what's going on
 * exactly at each point.
 */

/* Ethernet data struct. */
typedef struct
{
    os_macaddr_t dstmac;
    os_macaddr_t srcmac;
    uint16_t ethtype;
} eth_info;

#define IPv4 0x04
#define IPv6 0x06

/* Fix missing symbols on OSX */
#ifndef s6_addr16
#define s6_addr16 __u6_addr.__u6_addr16
#endif
#ifndef s6_addr32
#define s6_addr32 __u6_addr.__u6_addr32
#endif

/*
 * IP address container that is IP version agnostic.
 * The IPvX_MOVE macros handle filling these with packet data correctly.
 */
typedef struct ip_addr
{
    /* Should always be either 4 or 6. */
    uint8_t vers;
    union
    {
        struct in_addr v4;
        struct in6_addr v6;
    } addr;
} ip_addr;

/* Move IPv4 addr at pointer P into ip object D, and set it's type. */
#define IPv4_MOVE(D, P) D.addr.v4.s_addr = *(in_addr_t *)(P); \
                        D.vers = IPv4;
/* Move IPv6 addr at pointer P into ip object D, and set it's type. */
#define IPv6_MOVE(D, P) memcpy(D.addr.v6.s6_addr, P, 16); D.vers = IPv6;

/* Compare two IP addresses. */
#define IP_CMP(ipA, ipB) \
    ((ipA.vers == ipB.vers) &&                                          \
     (ipA.vers == IPv4 ?                                                \
      ipA.addr.v4.s_addr == ipB.addr.v4.s_addr :                        \
      ((ipA.addr.v6.s6_addr32[0] == ipB.addr.v6.s6_addr32[0]) &&        \
       (ipA.addr.v6.s6_addr32[1] == ipB.addr.v6.s6_addr32[1]) &&        \
       (ipA.addr.v6.s6_addr32[2] == ipB.addr.v6.s6_addr32[2]) &&        \
       (ipA.addr.v6.s6_addr32[3] == ipB.addr.v6.s6_addr32[3]))          \
         ))

/* Basic network layer information. */
typedef struct
{
    uint32_t ip_header_pos;
    ip_addr src;
    ip_addr dst;
    uint32_t length;
    uint8_t proto;
} ip_info;

/* IP fragment information. */
typedef struct ip_fragment
{
    uint32_t id;
    ip_addr src;
    ip_addr dst;
    uint32_t start;
    uint32_t end;
    uint8_t * data;
    uint8_t islast;
    struct ip_fragment * next;
    struct ip_fragment * child;
} ip_fragment;

#define UDP 0x11
#define TCP 0x06

/* Transport information. */
typedef struct
{
    uint32_t tpt_header_pos;
    uint16_t srcport;
    uint16_t dstport;
    /* Length of the payload. */
    uint16_t length;
    uint8_t transport;
    uint16_t udp_checksum;
    uint8_t *udp_csum_ptr;
} transport_info;

typedef struct
{
    int datalink;
} eth_config;

typedef struct
{
    ip_fragment *ip_fragment_head;
} ip_config;

/*
 * TCP header information. Also contains pointers used to connect to this
 * to other TCP streams, and to connect this packet to other packets in
 * it's stream.
 */
typedef struct tmp_tcp_info
{
    struct timeval ts;
    ip_addr src;
    ip_addr dst;
    uint16_t srcport;
    uint16_t dstport;
    uint32_t sequence;
    uint32_t ack_num;
    /* The length of the data portion. */
    uint32_t len;
    uint8_t syn;
    uint8_t ack;
    uint8_t fin;
    uint8_t rst;
    uint8_t * data;
    /* The next item in the list of tcp sessions. */
    struct tmp_tcp_info * next_sess;
    /*
     * These are for connecting all the packets in a session. The session
     * pointers above will always point to the most recent packet.
     * next_pkt and prev_pkt make chronological sense (next_pkt is always
     * more recent, and prev_pkt is less), we just hold the chain by the tail.
     */
    struct tmp_tcp_info * next_pkt;
    struct tmp_tcp_info * prev_pkt;
} temp_tcp_info;


typedef struct tcp_config
{
    temp_tcp_info *tcp_sessions_head;
    bool assemble;
    void (*process_sessions)(struct tcp_config *, temp_tcp_info *);
} tcp_config;

/*
 * Parsers all follow the same basic pattern. They take the position in
 * the packet data, the packet data pointer, the header, and an object
 * to fill out. They return the position of the first byte of their payload.
 * On error, they report the error and return 0.
 * Exceptions are noted.
 */

/* No pos is passed, since we always start at 0. */
uint32_t
eth_parse(struct pcap_pkthdr *, uint8_t *, eth_info *, eth_config *);

uint32_t
udp_parse(uint32_t, struct pcap_pkthdr *, uint8_t *,
          transport_info *);

/*
 * The ** to the packet data is passed, instead of the data directly.
 * They may set the packet pointer to a new data array.
 * On error, the packet pointer is set to NULL.
 */
uint32_t
ipv4_parse(uint32_t, struct pcap_pkthdr *,
           uint8_t **, ip_info *, ip_config *);
uint32_t
ipv6_parse(uint32_t, struct pcap_pkthdr *,
           uint8_t **, ip_info *, ip_config *);


/*
 * Add the ip_fragment object to our lists of fragments. If a fragment is
 * complete, returns the completed fragment object.
 */
ip_fragment *
ip_frag_add(ip_fragment *, ip_config *);

/* Frees all fragment objects. */
void ip_frag_free(ip_config *);

/*
 * Convert an ip struct to a string. The returned buffer is internal,
 * and need not be freed.
 */
char *
iptostr(ip_addr *);

uint16_t
compute_udp_checksum(uint8_t *packet, ip_info *ip, transport_info *udp);
#endif
