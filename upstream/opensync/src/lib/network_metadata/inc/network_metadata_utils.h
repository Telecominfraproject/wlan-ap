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

#ifndef __NETWORK_METADATA_UTILS_H__
#define __NETWORK_METADATA_UTILS_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "ds_tree.h"
#include "os_types.h"

#include "network_metadata.h"

enum acc_state
{
    ACC_STATE_INIT = 0,           /* Not accessed yet */
    ACC_STATE_WINDOW_ACTIVE = 1,  /* Updated in the current observation window */
    ACC_STATE_WINDOW_RESET = 2,   /* Resetted for the current window */
};


/**
 * @brief representation of a ethertype tagged flow and counters
 */
struct net_md_flow
{
    struct net_md_stats_accumulator *tuple_stats;
    ds_tree_node_t flow_node;
};


/**
 * @brief Representation of a pair of communicating devices
 */
struct net_md_eth_pair
{
    struct net_md_stats_accumulator *mac_stats;
    ds_tree_t ethertype_flows;
    ds_tree_t five_tuple_flows;
    ds_tree_node_t eth_pair_node;
};

struct net_md_aggregator;

/**
 * @brief helper function: string to os_macaddr_t
 *
 * @param strmac: ethernet mac in string representation
 * @return a os_macaddr_t pointer
 */
os_macaddr_t *str2os_mac(char *strmac);


struct net_md_flow_key * set_net_md_flow_key(struct net_md_flow_key *lkey);
void free_net_md_flow_key(struct net_md_flow_key *lkey);
void free_flow_report(struct flow_report *report);
void free_report_window(struct flow_window *window);
void free_window_stats(struct flow_stats *stats);
void free_flow_counters(struct flow_counters *counters);
void free_flow_counters(struct flow_counters *counters);
void free_flow_key(struct flow_key *key);
void free_node_info(struct node_info *node);
struct net_md_stats_accumulator * net_md_treelookup_acc(struct net_md_eth_pair *pair,
                                                        struct net_md_flow_key *key);

struct net_md_stats_accumulator * net_md_lookup_acc(struct net_md_aggregator *aggr,
                                                    struct net_md_flow_key *key);
void net_md_free_flow(struct net_md_flow *flow);
void net_md_free_eth_pair(struct net_md_eth_pair *pair);
int net_md_eth_cmp(void *a, void *b);
int net_md_5tuple_cmp(void *a, void *b);
char * net_md_set_str(char *in_str);
os_macaddr_t * net_md_set_os_macaddr(os_macaddr_t *in_mac);
bool net_md_set_ip(uint8_t ipv, uint8_t *ip, uint8_t **ip_tgt);
struct node_info * net_md_set_node_info(struct node_info *info);
void net_md_free_acc(struct net_md_stats_accumulator *acc);
void net_md_free_flow_tree(ds_tree_t *tree);
struct net_md_stats_accumulator * net_md_set_acc(struct net_md_flow_key *key);
struct net_md_eth_pair * net_md_set_eth_pair(struct net_md_flow_key *key);
struct net_md_eth_pair * net_md_lookup_eth_pair(struct net_md_aggregator *aggr,
                                                struct net_md_flow_key *key);
bool is_eth_only(struct net_md_flow_key *key);
bool has_eth_info(struct net_md_flow_key *key);
struct net_md_stats_accumulator *
net_md_lookup_acc_from_pair(struct net_md_eth_pair *pair,
                            struct net_md_flow_key *key);
struct net_md_stats_accumulator *
net_md_lookup_eth_acc(struct net_md_aggregator *aggr,
                      struct net_md_flow_key *key);
struct net_md_stats_accumulator *
net_md_lookup_5tuple_acc(struct net_md_aggregator *aggr,
                         struct net_md_flow_key *key);
struct flow_window * net_md_active_window(struct net_md_aggregator *aggr);
void net_md_set_counters(struct net_md_aggregator *aggr,
                         struct net_md_stats_accumulator *acc,
                         struct flow_counters *counters);
void net_md_report_accs(struct net_md_aggregator *aggr);
void net_md_free_flow_report(struct flow_report *report);
void net_md_reset_aggregator(struct net_md_aggregator *aggr);
#endif // __NETWORK_METADATA_UTILS_H__
