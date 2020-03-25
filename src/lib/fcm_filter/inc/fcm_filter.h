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

#ifndef FCM_FILTER_H_INCLUDED
#define FCM_FILTER_H_INCLUDED

#define FCM_FILTER_MAC_SIZE 18
#define FCM_FILTER_IP_SIZE 128



typedef struct fcm_filter_l3_info
{
    char        src_ip[FCM_FILTER_IP_SIZE];
    char        dst_ip[FCM_FILTER_IP_SIZE];
    uint16_t    sport;
    uint16_t    dport;
    uint8_t     l4_proto;
    uint8_t     ip_type;    // l3 proto ipv4 or ipv6
} fcm_filter_l3_info_t;


typedef struct fcm_filter_l2_info
{
    char            src_mac[FCM_FILTER_MAC_SIZE];
    char            dst_mac[FCM_FILTER_MAC_SIZE];
    unsigned int    vlan_id;
    unsigned int    eth_type;
} fcm_filter_l2_info_t;


typedef struct fcm_filter_stats
{
    unsigned long pkt_cnt;
    unsigned long bytes;
} fcm_filter_stats_t;


void fcm_filter_layer2_apply(char *filter_name,
                           struct fcm_filter_l2_info *data,
                           struct fcm_filter_stats *pkts,
                           bool *allow);


void fcm_filter_7tuple_apply(char *filter_name,
                              struct fcm_filter_l2_info *data,
                              struct fcm_filter_l3_info *data1,
                              struct fcm_filter_stats *pkts,
                              bool *action);
int fcm_filter_init(void);
void fcm_filter_cleanup(void);

#endif /* FCM_FILTER_H_INCLUDED */
