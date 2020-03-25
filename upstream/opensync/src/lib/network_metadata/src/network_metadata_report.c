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
#include <ctype.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "log.h"
#include "network_metadata_report.h"
#include "network_metadata_utils.h"
#include "qm_conn.h"


/**
 * @brief frees a stats aggregator
 *
 * @param aggr the aggregator to free
 */
void net_md_free_aggregator(struct net_md_aggregator *aggr)
{
    struct net_md_eth_pair *pair;
    if (aggr == NULL) return;

    net_md_free_flow_report(aggr->report);

    pair = ds_tree_head(&aggr->eth_pairs);
    while (pair != NULL)
    {
        struct net_md_eth_pair *next;

        next = ds_tree_next(&aggr->eth_pairs, pair);
        ds_tree_remove(&aggr->eth_pairs, pair);
        net_md_free_eth_pair(pair);
        pair = next;
    }

    net_md_free_flow_tree(&aggr->five_tuple_flows);

    free(aggr);
}

/**
 * @brief allocates a stats aggregator
 *
 * @param info pointer to the node info
 * @param num_windows the maxium number of windows the report will contain
 * @param acc_ttl how long an incative accumulator should be kept around
 * @param report_type absolute or relative
 * @return a pointer to an aggregator if the allocation succeeded,
 *         NULL otherwise.
 *         The caller is responsible to free the returned pointer.
 */
struct net_md_aggregator *
net_md_allocate_aggregator(struct node_info *info,
                           size_t num_windows,
                           int acc_ttl, int report_type,
                           bool (*report_filter)(struct net_md_stats_accumulator *))
{
    struct net_md_aggregator *aggr;
    struct flow_report *report;
    struct node_info *node;
    struct flow_window **windows_array;

    /* Allocate aggregator memory */
    aggr = calloc(1, sizeof(*aggr));
    if (aggr == NULL) return NULL;

    /* Allocate aggregator's report memory */
    report = calloc(1, sizeof(*report));
    if (report == NULL) goto err_free_aggr;

    aggr->report = report;

    /* Set aggregator's node info */
    node = net_md_set_node_info(info);
    if (node == NULL) goto err_free_report;

    report->node_info = node;

    /* Allocate aggregator's report's windows */
    windows_array = calloc(num_windows, sizeof(*windows_array));
    if (windows_array == NULL) goto err_free_node;

    report->flow_windows = windows_array;
    report->num_windows = 0;

    aggr->max_windows = num_windows;
    aggr->report_all_samples = false;
    aggr->acc_ttl = acc_ttl;
    aggr->report_type = report_type;
    ds_tree_init(&aggr->eth_pairs, net_md_eth_cmp,
                 struct net_md_eth_pair, eth_pair_node);
    ds_tree_init(&aggr->five_tuple_flows, net_md_5tuple_cmp,
                 struct net_md_flow, flow_node);
    aggr->report_filter = report_filter;

    return aggr;

err_free_node:
    free_node_info(node);

err_free_report:
    free(report);

err_free_aggr:
    free(aggr);

    return NULL;
}


/**
 * @brief Add sampled stats to the aggregator
 *
 * Called to add system level sampled data from a flow
 * within the current observation window
 *
 * @param aggr the aggregator
 * @param key the lookup flow key
 * @param counters the stat counters to aggregate
 * @return true if successful, false otherwise
 */
bool net_md_add_sample(struct net_md_aggregator *aggr,
                       struct net_md_flow_key *key,
                       struct flow_counters *counters)
{
    struct net_md_stats_accumulator *acc;

    if (aggr == NULL) return false;
    if (key == NULL) return false;
    if (counters == NULL) return false;

    acc = net_md_lookup_acc(aggr, key);
    if (acc == NULL) return false;

    net_md_set_counters(aggr, acc, counters);

    return true;
}


/**
 * @brief Activates the aggregator's current observation window
 *
 * @param aggr the aggregator
 * @return true if the current window was activated, false otherwise
 */
bool net_md_activate_window(struct net_md_aggregator *aggr)
{
    struct flow_window *window;

    window = net_md_active_window(aggr);
    if (window == NULL) return false;

    window->started_at = time(NULL);
    return true;
}


/**
 * @brief generates report content for the current window
 *
 * Walks through aggregated stats and generates the current observation window
 * report content. Prepares next window.
 * TBD: execute filtering
 *
 * @param aggr the aggregator
 * @return true if successful, false otherwise
 */
bool net_md_close_active_window(struct net_md_aggregator *aggr)
{
    struct flow_window *window;
    struct flow_stats **stats_array;
    struct flow_stats *stats;
    size_t provisioned_stats, i;

    window = net_md_active_window(aggr);
    window->ended_at = time(NULL);

    provisioned_stats = aggr->active_accs;
    stats_array = calloc(provisioned_stats, sizeof(*stats_array));
    if (stats_array == NULL) return false;

    window->flow_stats = stats_array;

    stats = calloc(provisioned_stats, sizeof(*stats));
    if (stats == NULL) goto err_free_stats_array;

    window->provisioned_stats = provisioned_stats;

    for (i = 0; i < provisioned_stats; i++) *stats_array++ = stats++;

    net_md_report_accs(aggr);

    /* Set the number of stats counters */
    window->num_stats = aggr->stats_cur_idx;

    /* Bump up the active window index */
    aggr->windows_cur_idx++;

    /* Reset the stats counter */
    aggr->stats_cur_idx = 0;

    return true;

err_free_stats_array:
    free(window->flow_stats);

    return false;
}


/**
 * @brief send report from aggregator
 *
 * @param aggr the aggregator
 * @return true if the report was successfully sent, false otherwise
 */
bool net_md_send_report(struct net_md_aggregator *aggr, char *mqtt_topic)
{
    struct flow_report *report;
    struct packed_buffer *pb;
    qm_response_t res;
    bool ret;

    if (aggr == NULL) return false;
    if (mqtt_topic == NULL) return false;

    report = aggr->report;
    report->reported_at = time(NULL);
    pb = serialize_flow_report(aggr->report);
    ret = qm_conn_send_direct(QM_REQ_COMPRESS_IF_CFG, mqtt_topic,
                              pb->buf, pb->len, &res);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Reset the aggregator */
    net_md_reset_aggregator(aggr);

    return ret;
}
