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

#ifndef __TEST_NETWORK_METADATA_H__
#define __TEST_NETWORK_METADATA_H__

#include "unity.h"

#include "network_metadata.h"
#include "network_metadata_report.h"

/**
 * @brief defines structure passed to network data tests
 *
 * @see setUp() instanciates global object based on this structure
 */
struct test_network_data
{
    bool initialized;
    char *f_name;
    struct flow_stats *stats1;       /* flow_window 1 */
    struct flow_stats *stats2;       /* flow_window 2 */
    struct flow_stats *stats3;       /* flow_window 2 */
    struct flow_stats *stats4;       /* flow_window 2 */
    struct flow_window *window1;
    struct flow_window *window2;
    struct flow_report *report;
};

struct in_key
{
    char *smac;
    char *dmac;
    int16_t vlan_id;
    uint16_t ethertype;
    uint8_t ip_version;
    char *src_ip;
    char *dst_ip;
    uint8_t ipprotocol;
    uint16_t sport;
    uint16_t dport;
};

struct test_network_data_report
{
    bool initialized;
    char mqtt_topic[256];
    char node_id[32];
    char location_id[32];
    struct node_info node_info;
    struct flow_counters counters;
    size_t nelems;
    struct net_md_flow_key **net_md_keys;
};

void test_net_md_report_setup(void);
void test_net_md_report_teardown(void);

void test_str2mac(void);
void test_net_md_allocate_aggregator(void);
void test_activate_add_samples_close_send_report(void);
void test_add_2_samples_all_keys(void);
void test_ethernet_aggregate_one_key(void);
void test_ethernet_aggregate_two_keys(void);
void test_large_loop(void);
void test_add_remove_flows(void);
void test_multiple_windows(void);
void test_report_filter(void);
void test_activate_and_free_aggr(void);
void test_bogus_ttl(void);
void test_flow_tags_one_key(void);
#endif // __TEST_NETWORK_METADATA_H__
