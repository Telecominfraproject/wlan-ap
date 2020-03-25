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

#ifndef __DNS_PARSE_H__
#define __DNS_PARSE_H__

#include <linux/if_packet.h>
#include <pcap.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include "network.h"
#include "os_types.h"
#include "ds_tree.h"
#include "fsm.h"
#include "fsm_policy.h"


struct web_cat_offline
{
    time_t offline_ts;
    time_t check_offline;
    bool provider_offline;
    uint32_t connection_failures;
};


#define MAX_EXCLUDES 100
struct dns_session
{
    uint16_t EXCLUDED[MAX_EXCLUDES];
    uint16_t EXCLUDES;
    char SEP;
    char * RECORD_SEP;
    int AD_ENABLED;
    int NS_ENABLED;
    int COUNTS;
    int PRETTY_DATE;
    int PRINT_RR_NAME;
    int MISSING_TYPE_WARNINGS;
    uint32_t DEDUPS;
    eth_info eth_hdr;
    eth_config eth_config;
    ip_info ip;
    ip_config ip_config;
    transport_info udp;
    tcp_config tcp_config;
    uint32_t dedup_pos;
    ds_tree_t device_sessions;
    struct sockaddr_ll raw_dst;
    os_macaddr_t src_eth_addr;
    int sock_fd;
    int post_eth;
    int data_offset;
    int last_byte_pos;
    struct fsm_session *fsm_context;
    time_t stat_report_ts;
    time_t stat_log_ts;
    bool debug;
    bool cache_ip;
    char *blocker_topic;
    uint8_t debug_pkt_copy[512];
    uint8_t debug_pkt_len;
    int32_t reported_lookup_failures;
    int32_t remote_lookup_retries;
    ds_tree_node_t session_node;
    ds_tree_t session_devices;
    struct fsm_policy_client policy_client;
    char *provider;
    struct fsm_session *provider_plugin;
    struct fsm_web_cat_ops *provider_ops;
    long health_stats_report_interval;
    char *health_stats_report_topic;
    struct fsm_url_stats health_stats;
    struct web_cat_offline cat_offline;
    bool initialized;
};

struct dns_cache
{
    bool initialized;
    ds_tree_t fsm_sessions;
};

/* Holds the information for a dns question. */
typedef struct dns_question
{
    char * name;
    uint16_t type;
    uint16_t cls;
    struct dns_question * next;
} dns_question;

/* Holds the information for a dns resource record. */
typedef struct dns_rr
{
    char * name;
    uint16_t type;
    uint32_t type_pos;
    uint16_t cls;
    const char * rr_name;
    uint16_t ttl;
    uint16_t rdlength;
    uint16_t data_len;
    char * data;
    struct dns_rr * next;
} dns_rr;

/* Holds general DNS information. */
typedef struct
{
    uint16_t id;
    char qr;
    char AA;
    char TC;
    uint8_t Z;
    uint8_t rcode;
    uint8_t opcode;
    uint16_t qdcount;
    dns_question * queries;
    uint16_t ancount;
    dns_rr * answers;
    uint32_t answer_pos;
    uint16_t nscount;
    dns_rr * name_servers;
    uint16_t arcount;
    dns_rr * additional;
} dns_info;

#define FORCE 1

#define REQ_CACHE_TTL 120

/*
 * Parse DNS from from the given 'packet' byte array starting at offset 'pos',
 * with libpcap header information in 'header'.
 * The parsed information is put in the 'dns' struct, and the
 * new pos in the packet is returned. (0 on error).
 * The config struct gives needed configuration options.
 * force - Force fully parsing the dns data, even if
 *    configuration parameters mean it isn't necessary. If this is false,
 *  the returned position may not correspond with the end of the DNS data.
 */
uint32_t
dns_parse(uint32_t pos, struct pcap_pkthdr *header,
          uint8_t *packet, dns_info * dns,
          struct dns_session *dns_session, uint8_t force);

void
free_rrs(ip_info * ip, transport_info * trns, dns_info * dns,
         struct pcap_pkthdr * header);

void
dns_handler(struct fsm_session *session,
            struct net_header_parser *net_header);

int
dns_plugin_init(struct fsm_session *session);

void
dns_remove_req(struct dns_session *dns_session, os_macaddr_t *mac,
               uint16_t req_id);

void
dns_forward(struct dns_session *dns_session, dns_info *dns,
            uint8_t *packet, int len);

void
dns_periodic(struct fsm_session  *session);

char *
dns_report_cat(int category);

void
fqdn_policy_check(struct dns_device *ds,
                  struct fqdn_pending_req *req);

void
dns_policy_check(struct dns_device *ds,
                 struct fqdn_pending_req *req);

struct dns_session *
dns_lookup_session(struct fsm_session *session);

struct dns_session *
dns_get_session(struct fsm_session *session);

#endif /* __DNS_PARSE_H__ */
