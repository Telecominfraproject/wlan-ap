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

#define MAX_STRLEN 256

/**
 * @brief compares 2 flow keys'ethernet content
 *
 * Compares source mac, dest mac and vlan id
 * Used to lookup a node in the manager's eth_pairs tree
 *
 * @param a void pointer cast to a net_md_flow_key struct
 * @param a void pointer cast to a net_md_flow_key struct
 */
int net_md_eth_cmp(void *a, void *b)
{
    struct net_md_flow_key *key_a = a;
    struct net_md_flow_key *key_b = b;
    int cmp;

    /* Compare source mac addresses */
    cmp = memcmp(key_a->smac->addr, key_b->smac->addr,
                 sizeof(key_a->smac->addr));
    if (cmp != 0) return cmp;

    /* Compare destination mac addresses */
    cmp = memcmp(key_a->dmac->addr, key_b->dmac->addr,
                 sizeof(key_a->dmac->addr));
    if (cmp != 0) return cmp;

    /* Compare vlan id */
   cmp = (int)(key_a->vlan_id) - (int)(key_b->vlan_id);

   return cmp;
}


/**
 * @brief compares 2 flow keys'ethertype
 *
 * Compares ethertype
 * Used to lookup a node in the manager's eth_pairs ethertype tree
 *
 * @param a void pointer cast to a net_md_flow_key struct
 * @param a void pointer cast to a net_md_flow_key struct
 */
int net_md_ethertype_cmp(void *a, void *b)
{
    struct net_md_flow_key *key_a = a;
    struct net_md_flow_key *key_b = b;
    int cmp;

    /* Compare vlan id */
   cmp = (int)(key_a->ethertype) - (int)(key_b->ethertype);

   return cmp;
}


/**
 * @brief compares 2 five tuples content
 *
 * Compare the 5 tuple content of the given keys
 * Used to lookup a node in an eth_pair's 5 tuple tree.
 * The lookup could be organized differently, using a hash approach.
 * The approach can be optimized later on without changing the API
 *
 * @param a void pointer cast to a net_md_flow_key struct
 * @param a void pointer cast to a net_md_flow_key struct
 */
int net_md_5tuple_cmp(void *a, void *b)
{
    struct net_md_flow_key *key_a  = a;
    struct net_md_flow_key *key_b  = b;
    size_t ipl;
    int cmp;

    /* Compare ip versions */
    cmp = (int)(key_a->ip_version) - (int)(key_b->ip_version);
    if (cmp != 0) return cmp;

    /* Get ip version compare length */
    ipl = (key_a->ip_version == 4 ? 4 : 32);

    /* Compare source IP addresses */
    cmp = memcmp(key_a->src_ip, key_b->src_ip, ipl);
    if (cmp != 0) return cmp;

    /* Compare destination IP addresses */
    cmp = memcmp(key_a->dst_ip, key_b->dst_ip, ipl);
    if (cmp != 0) return cmp;

    /* Compare ip protocols */
    cmp = (int)(key_a->ipprotocol) - (int)(key_b->ipprotocol);
    if (cmp != 0) return cmp;

    /* Compare source ports */
    cmp = (int)(key_a->sport) - (int)(key_b->sport);
    if (cmp != 0) return cmp;

    /* Compare destination ports */
    cmp = (int)(key_a->sport) - (int)(key_b->sport);
    return cmp;
}

/**
 * @brief helper function: string to os_macaddr_t
 *
 * @param strmac: ethernet mac in string representation
 * @return a os_macaddr_t pointer
 */
os_macaddr_t *str2os_mac(char *strmac)
{
    os_macaddr_t *mac;
    size_t len, i, j;

    if (strmac == NULL) return NULL;

    /* Validate the input string */
    len = strlen(strmac);
    if (len != 17) return NULL;

    mac = calloc(1, sizeof(*mac));
    if (mac == NULL) return NULL;

    i = 0;
    j = 0;
    do {
        char a = strmac[i++];
        char b = strmac[i++];
        uint8_t v;

        if (!isxdigit(a)) goto err_free_mac;
        if (!isxdigit(b)) goto err_free_mac;

        v = (isdigit(a) ? (a - '0') : (toupper(a) - 'A' + 10));
        v *= 16;
        v += (isdigit(b) ? (b - '0') : (toupper(b) - 'A' + 10));
        mac->addr[j] = v;

        if (i == len) break;
        if (strmac[i++] != ':') goto err_free_mac;
        j++;
    } while (i < len);

    return mac;

err_free_mac:
    free(mac);

    return NULL;
}


char * net_md_set_str(char *in_str)
{
    char *out;
    size_t len;

    if (in_str == NULL) return NULL;

    len = strnlen(in_str, MAX_STRLEN);
    if (len == 0) return NULL;

    out = strndup(in_str, MAX_STRLEN);

    return out;
}

os_macaddr_t * net_md_set_os_macaddr(os_macaddr_t *in_mac)
{
    os_macaddr_t *mac;

    if (in_mac == NULL) return NULL;

    mac = calloc(1, sizeof(*mac));
    if (mac == NULL) return NULL;

    memcpy(mac, in_mac, sizeof(*mac));
    return mac;
}


bool net_md_set_ip(uint8_t ipv, uint8_t *ip, uint8_t **ip_tgt)
{
    size_t ipl;

    if ((ipv != 4) && (ipv != 6))
    {
        *ip_tgt = NULL;
        return true;
    }

    ipl = (ipv == 4 ? 4 : 32);

    *ip_tgt = calloc(1, ipl);
    if (*ip_tgt == NULL) return false;

    memcpy(*ip_tgt, ip, ipl);
    return true;
}


struct node_info * net_md_set_node_info(struct node_info *info)
{
    struct node_info *node;

    if (info == NULL) return NULL;

    node = calloc(1, sizeof(*node));
    if (node == NULL) return NULL;

    node->node_id = net_md_set_str(info->node_id);
    if (node->node_id == NULL) goto err_free_node;;

    node->location_id = net_md_set_str(info->location_id);
    if (node->location_id == NULL) goto err_free_node_id;

    return node;

err_free_node_id:
    free(node->node_id);

err_free_node:
    free(node);

    return NULL;
}


void free_node_info(struct node_info *node)
{
    if (node == NULL) return;

    free(node->node_id);
    free(node->location_id);

    free(node);
}


void
free_flow_key_tag(struct flow_tags *tag)
{
    size_t i;

    free(tag->vendor);
    free(tag->app_name);

    for (i = 0; i < tag->nelems; i++) free(tag->tags[i]);
    free(tag->tags);

    free(tag);
}

void
free_flow_key_tags(struct flow_key *key)
{
    size_t i;
    for (i = 0; i < key->num_tags; i++)
    {
        free_flow_key_tag(key->tags[i]);
        key->tags[i] = NULL;
    }

    free(key->tags);
}

void
free_flow_key(struct flow_key *key)
{
    size_t i;

    if (key == NULL) return;

    free(key->smac);
    free(key->dmac);
    free(key->src_ip);
    free(key->dst_ip);

    for (i = 0; i < key->num_tags; i++)
    {
        free_flow_key_tag(key->tags[i]);
        key->tags[i] = NULL;
    }

    free(key->tags);
    free(key);
}


void free_flow_counters(struct flow_counters *counters)
{
    if (counters == NULL) return;

    free(counters);
}


void free_window_stats(struct flow_stats *stats)
{
    if (stats == NULL) return;

    if (stats->owns_key) free_flow_key(stats->key);
    free_flow_counters(stats->counters);

    free(stats);
}


void free_report_window(struct flow_window *window)
{
    size_t i;

    if (window == NULL) return;

    for (i = 0; i < window->num_stats; i++)
    {
        free_window_stats(window->flow_stats[i]);
        window->flow_stats[i] = NULL;
    }

    free(window->flow_stats);

    free(window);
}


void free_flow_report(struct flow_report *report)
{
    size_t i;

    if (report == NULL) return;

    for (i = 0; i < report->num_windows; i++)
    {
        free_report_window(report->flow_windows[i]);
        report->flow_windows[i] = NULL;
    }

    free(report->flow_windows);
    free_node_info(report->node_info);
    free(report);
}


void free_net_md_flow_key(struct net_md_flow_key *lkey)
{
    if (lkey == NULL) return;

    free(lkey->smac);
    free(lkey->dmac);
    free(lkey->src_ip);
    free(lkey->dst_ip);

    free(lkey);
}


struct net_md_flow_key * set_net_md_flow_key(struct net_md_flow_key *lkey)
{
    struct net_md_flow_key *key;
    bool ret, err;

    key = calloc(1, sizeof(*key));
    if (key == NULL) return NULL;

    key->smac = net_md_set_os_macaddr(lkey->smac);
    err = ((key->smac == NULL) && (lkey->smac != NULL));
    if (err) goto err_free_key;

    key->dmac = net_md_set_os_macaddr(lkey->dmac);
    err = ((key->dmac == NULL) && (lkey->dmac != NULL));
    if (err) goto err_free_smac;

    ret = net_md_set_ip(lkey->ip_version, lkey->src_ip, &key->src_ip);
    if (!ret) goto err_free_dmac;

    ret = net_md_set_ip(lkey->ip_version, lkey->dst_ip, &key->dst_ip);
    if (!ret) goto err_free_src_ip;

    key->ip_version = lkey->ip_version;
    key->vlan_id = lkey->vlan_id;
    key->ethertype = lkey->ethertype;
    key->ipprotocol = lkey->ipprotocol;
    key->sport = lkey->sport;
    key->dport = lkey->dport;

    return key;

err_free_src_ip:
    free(key->src_ip);

err_free_dmac:
    free(key->dmac);

err_free_smac:
    free(key->smac);

err_free_key:
    free(key);

    return NULL;
}


struct flow_key * net_md_set_flow_key(struct net_md_flow_key *key)
{
    char buf[INET6_ADDRSTRLEN];
    struct flow_key *fkey;
    const char *res;
    int family;
    size_t ip_size;

    fkey = calloc(1, sizeof(*fkey));
    if (fkey == NULL) return NULL;

    if (key->smac != NULL)
    {
        snprintf(buf, sizeof(buf), PRI_os_macaddr_lower_t,
                 FMT_os_macaddr_pt(key->smac));
        fkey->smac = strndup(buf, sizeof(buf));
        if (fkey->smac == NULL) goto err_free_fkey;
    }

    if (key->dmac != NULL)
    {
        snprintf(buf, sizeof(buf), PRI_os_macaddr_lower_t,
                 FMT_os_macaddr_pt(key->dmac));
        fkey->dmac = strndup(buf, sizeof(buf));
        if (fkey->dmac == NULL) goto err_free_smac;
    }

    fkey->vlan_id = key->vlan_id;
    fkey->ethertype = key->ethertype;

    if (key->ip_version == 0) return fkey;

    family = ((key->ip_version == 4) ? AF_INET : AF_INET6);
    ip_size = ((family == AF_INET) ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN);

    fkey->src_ip = calloc(1, ip_size);
    if (fkey->src_ip == NULL) goto err_free_dmac;

    res = inet_ntop(family, key->src_ip, fkey->src_ip, ip_size);
    if (res == NULL) goto err_free_src_ip;

    fkey->dst_ip = calloc(1, ip_size);
    if (fkey->dst_ip == NULL) goto err_free_src_ip;

    res = inet_ntop(family, key->dst_ip, fkey->dst_ip, ip_size);
    if (res == NULL) goto err_free_dst_ip;

    fkey->protocol = key->ipprotocol;
    fkey->sport = ntohs(key->sport);
    fkey->dport = ntohs(key->dport);

    return fkey;

err_free_dst_ip:
    free(fkey->dst_ip);

err_free_src_ip:
    free(fkey->src_ip);

err_free_dmac:
    free(fkey->dmac);

err_free_smac:
    free(fkey->smac);

err_free_fkey:
    free(fkey);

    return NULL;
}


void net_md_free_acc(struct net_md_stats_accumulator *acc)
{
    if (acc == NULL) return;

    free_net_md_flow_key(acc->key);
    free_flow_key(acc->fkey);
    free(acc);
}


struct net_md_stats_accumulator * net_md_set_acc(struct net_md_flow_key *key)
{
    struct net_md_stats_accumulator *acc;

    if (key == NULL) return NULL;

    acc = calloc(1, sizeof(*acc));
    if (acc == NULL) return NULL;

    acc->key = set_net_md_flow_key(key);
    if (acc->key == NULL) goto err_free_acc;

    acc->fkey = net_md_set_flow_key(key);
    if (acc->fkey == NULL) goto err_free_md_flow_key;

    return acc;

err_free_md_flow_key:
    free_net_md_flow_key(acc->key);

err_free_acc:
    free(acc);

    return NULL;
}


void net_md_free_flow(struct net_md_flow *flow)
{
    if (flow == NULL) return;

    net_md_free_acc(flow->tuple_stats);
    free(flow);
}


void net_md_free_flow_tree(ds_tree_t *tree)
{
    struct net_md_flow *flow, *next;

    if (tree == NULL) return;

    flow = ds_tree_head(tree);
    while (flow != NULL)
    {
        next = ds_tree_next(tree, flow);
        ds_tree_remove(tree, flow);
        net_md_free_flow(flow);
        flow = next;
    }

}


void net_md_free_eth_pair(struct net_md_eth_pair *pair)
{
    if (pair == NULL) return;

    net_md_free_acc(pair->mac_stats);
    net_md_free_flow_tree(&pair->ethertype_flows);
    net_md_free_flow_tree(&pair->five_tuple_flows);
    free(pair);
}


struct net_md_eth_pair * net_md_set_eth_pair(struct net_md_flow_key *key)
{
    struct net_md_eth_pair *eth_pair;

    if (key == NULL) return NULL;

    eth_pair = calloc(1, sizeof(*eth_pair));
    if (eth_pair == NULL) return NULL;

    eth_pair->mac_stats = net_md_set_acc(key);
    if (eth_pair->mac_stats == NULL) goto err_free_eth_pair;

    ds_tree_init(&eth_pair->ethertype_flows, net_md_ethertype_cmp,
                 struct net_md_flow, flow_node);

    ds_tree_init(&eth_pair->five_tuple_flows, net_md_5tuple_cmp,
                 struct net_md_flow, flow_node);

    return eth_pair;

err_free_eth_pair:
    free(eth_pair);

    return NULL;
}


bool is_eth_only(struct net_md_flow_key *key)
{
    return (key->ip_version == 0);
}


struct net_md_stats_accumulator *
net_md_tree_lookup_acc(ds_tree_t *tree, struct net_md_flow_key *key)
{
    struct net_md_flow *flow;
    struct net_md_stats_accumulator *acc;

    flow = ds_tree_find(tree, key);
    if (flow != NULL) return flow->tuple_stats;

    /* Allocate flow */
    flow = calloc(1, sizeof(*flow));
    if (flow == NULL) return NULL;

    /* Allocate the flow accumulator */
    acc = net_md_set_acc(key);
    if (acc == NULL) goto err_free_flow;

    flow->tuple_stats = acc;
    ds_tree_insert(tree, flow, acc->key);

    return acc;

err_free_flow:
    free(flow);

    return NULL;
}


struct net_md_stats_accumulator *
net_md_lookup_acc_from_pair(struct net_md_eth_pair *pair,
                            struct net_md_flow_key *key)
{
    ds_tree_t *tree;

    /* Check if the key refers to a L2 flow */
    tree = is_eth_only(key) ? &pair->ethertype_flows : &pair->five_tuple_flows;

    return net_md_tree_lookup_acc(tree, key);
}


bool has_eth_info(struct net_md_flow_key *key)
{
    return (key->smac != NULL);
}


struct net_md_eth_pair * net_md_lookup_eth_pair(struct net_md_aggregator *aggr,
                                                struct net_md_flow_key *key)
{
    struct net_md_eth_pair *eth_pair;
    bool has_eth;

    if (aggr == NULL) return NULL;
    has_eth = has_eth_info(key);
    if (!has_eth) return NULL;

    eth_pair = ds_tree_find(&aggr->eth_pairs, key);
    if (eth_pair != NULL) return eth_pair;

    /* Allocate and insert a new ethernet pair */
    eth_pair = net_md_set_eth_pair(key);
    if (eth_pair == NULL) return NULL;

    ds_tree_insert(&aggr->eth_pairs, eth_pair, eth_pair->mac_stats->key);

    return eth_pair;
}


struct net_md_stats_accumulator *
net_md_lookup_eth_acc(struct net_md_aggregator *aggr,
                      struct net_md_flow_key *key)
{
    struct net_md_eth_pair *eth_pair;
    struct net_md_stats_accumulator *acc;

    if (aggr == NULL) return NULL;

    eth_pair = net_md_lookup_eth_pair(aggr, key);
    if (eth_pair == NULL) return NULL;

    acc = net_md_lookup_acc_from_pair(eth_pair, key);

    return acc;
}


struct net_md_stats_accumulator *
net_md_lookup_acc(struct net_md_aggregator *aggr,
                  struct net_md_flow_key *key)
{
    struct net_md_stats_accumulator *acc;

    if (aggr == NULL) return NULL;

    if (has_eth_info(key)) return net_md_lookup_eth_acc(aggr, key);

    acc = net_md_tree_lookup_acc(&aggr->five_tuple_flows, key);

    return acc;
}


void net_md_set_counters(struct net_md_aggregator *aggr,
                         struct net_md_stats_accumulator *acc,
                         struct flow_counters *counters)
{
    if (acc->state != ACC_STATE_WINDOW_ACTIVE) aggr->active_accs++;

    acc->counters = *counters;
    acc->state = ACC_STATE_WINDOW_ACTIVE;
    acc->last_updated = time(NULL);
}


/* Get aggregator's active windows */
struct flow_window * net_md_active_window(struct net_md_aggregator *aggr)
{
    struct flow_report *report;
    struct flow_window *window;
    size_t idx;

    report = aggr->report;
    idx = aggr->windows_cur_idx;
    if (idx == aggr->max_windows) return NULL;

    window = report->flow_windows[idx];
    if (window != NULL) return window;

    window = calloc(1, sizeof(*window));
    if (window == NULL) return NULL;

    report->flow_windows[idx] = window;
    aggr->report->num_windows++;
    return window;
}


void net_md_close_counters(struct net_md_aggregator *aggr,
                           struct net_md_stats_accumulator *acc)
{
    acc->report_counters = acc->counters;

    /* Relative report */
    if (aggr->report_type == NET_MD_REPORT_RELATIVE)
    {
        acc->report_counters.bytes_count -= acc->first_counters.bytes_count;
        acc->report_counters.packets_count -= acc->first_counters.packets_count;
    }

    acc->first_counters = acc->counters;
}


bool net_md_add_sample_to_window(struct net_md_aggregator *aggr,
                                 struct net_md_stats_accumulator *acc)
{
    struct flow_window *window;
    struct flow_stats *stats;
    size_t stats_idx;
    bool filter_add;

    window = net_md_active_window(aggr);
    if (window == NULL) return false;

    stats_idx = aggr->stats_cur_idx;
    if (stats_idx == window->provisioned_stats) return false;

    if (aggr->report_filter != NULL)
    {
        filter_add = aggr->report_filter(acc);
        if (filter_add == false) return false;
    }

    stats = window->flow_stats[stats_idx];
    stats->counters = calloc(1, sizeof(*(stats->counters)));
    if (stats->counters == NULL) return false;

    stats->owns_key = false;
    stats->key = acc->fkey;
    *stats->counters = acc->report_counters;

    aggr->stats_cur_idx++;
    aggr->total_report_flows++;
    return true;
}


void net_md_report_5tuples_accs(struct net_md_aggregator *aggr,
                                ds_tree_t *tree)
{
    struct net_md_flow *flow;

    flow = ds_tree_head(tree);
    while (flow != NULL)
    {
        struct net_md_stats_accumulator *acc;
        struct net_md_flow *next;
        struct net_md_flow *remove;
        time_t now;
        double cmp;
        bool active_flow;
        bool retire_flow;
        bool keep_flow;

        acc = flow->tuple_stats;
        active_flow = (acc->state == ACC_STATE_WINDOW_ACTIVE);
        if (active_flow)
        {
            net_md_close_counters(aggr, acc);
            net_md_add_sample_to_window(aggr, acc);
            acc->state = ACC_STATE_WINDOW_RESET;
        }

        next = ds_tree_next(tree, flow);
        /* Check if the accumulator is old enough to be removed */
        now = time(NULL);
        cmp = difftime(now, acc->last_updated);
        retire_flow = (cmp >= aggr->acc_ttl);

        /* keep the flow if it's active and not yet retired */
        keep_flow = (active_flow || !retire_flow);
        if (!keep_flow)
        {
            remove = flow;
            ds_tree_remove(tree, remove);
            net_md_free_flow(remove);
        }

        flow = next;
    }
}


void net_md_update_eth_acc(struct net_md_stats_accumulator *eth_acc,
                           struct net_md_stats_accumulator *acc)
{
    struct flow_counters *from, *to;

    from = &acc->counters;
    to = &eth_acc->counters;
    to->bytes_count += from->bytes_count;
    to->packets_count += from->packets_count;

    /* Don't count twice the previously reported counters */
    from = &acc->first_counters;
    to->bytes_count -= from->bytes_count;
    to->packets_count -= from->packets_count;
}


void net_md_report_eth_acc(struct net_md_aggregator *aggr,
                           struct net_md_eth_pair *eth_pair)
{
    struct net_md_stats_accumulator *eth_acc;
    ds_tree_t *tree;
    struct net_md_flow *flow;

    eth_acc = eth_pair->mac_stats;
    tree = &eth_pair->ethertype_flows;
    flow = ds_tree_head(tree);

    while (flow != NULL)
    {
        struct net_md_stats_accumulator *acc;
        struct net_md_flow *next;
        struct net_md_flow *remove;
        time_t now;
        double cmp;
        bool active_flow;
        bool retire_flow;
        bool keep_flow;

        acc = flow->tuple_stats;
        active_flow = (acc->state == ACC_STATE_WINDOW_ACTIVE);
        if (active_flow)
        {
            eth_acc->state = ACC_STATE_WINDOW_ACTIVE;
            net_md_update_eth_acc(eth_acc, acc);
            net_md_close_counters(aggr, acc);
            if (aggr->report_all_samples) net_md_add_sample_to_window(aggr, acc);
            acc->state = ACC_STATE_WINDOW_RESET;
        }

        next = ds_tree_next(tree, flow);

        /* Check if the accumulator is old enough to be removed */
        now = time(NULL);
        cmp = difftime(now, acc->last_updated);
        retire_flow = (cmp >= aggr->acc_ttl);

        /* keep the flow if it's active and not yet retired */
        keep_flow = (active_flow || !retire_flow);
        if (!keep_flow)
        {
            remove = flow;
            ds_tree_remove(tree, remove);
            net_md_free_flow(remove);
        }

        flow = next;
    }

    if (eth_acc->state == ACC_STATE_WINDOW_ACTIVE)
    {
        net_md_close_counters(aggr, eth_acc);
        net_md_add_sample_to_window(aggr, eth_acc);
        eth_acc->state = ACC_STATE_WINDOW_RESET;
    }
}


void net_md_report_accs(struct net_md_aggregator *aggr)
{
    struct net_md_eth_pair *eth_pair;

    eth_pair = ds_tree_head(&aggr->eth_pairs);
    while (eth_pair != NULL)
    {
        net_md_report_eth_acc(aggr, eth_pair);
        net_md_report_5tuples_accs(aggr, &eth_pair->five_tuple_flows);
        eth_pair = ds_tree_next(&aggr->eth_pairs, eth_pair);
    }

    net_md_report_5tuples_accs(aggr, &aggr->five_tuple_flows);
}


static void net_md_free_stats(struct flow_stats *stats)
{
    /* Don't free the key, it is a reference */
    free(stats->counters);
}


static void net_md_free_free_window(struct flow_window *window)
{
    struct flow_stats **stats_array;
    struct flow_stats *stats;
    size_t i, n;

    if (window == NULL) return;

    n = window->provisioned_stats;
    if (n == 0)
    {
        free(window);
        return;
    }

    stats_array = window->flow_stats;
    stats = *stats_array;

    for (i = 0; i < n; i++) net_md_free_stats(stats_array[i]);
    free(stats);
    free(stats_array);
    window->provisioned_stats = 0;
    window->num_stats = 0;
    free(window);
}


void net_md_free_flow_report(struct flow_report *report)
{
    struct flow_window **windows_array;
    size_t i, n;

    free_node_info(report->node_info);

    n = report->num_windows;

    windows_array = report->flow_windows;
    for (i = 0; i < n; i++) net_md_free_free_window(windows_array[i]);
    free(windows_array);
    free(report);
}


void net_md_reset_aggregator(struct net_md_aggregator *aggr)
{
    struct flow_report *report;
    struct flow_window **windows_array;
    struct flow_window *window;
    size_t i, n;

    if (aggr == NULL) return;

    report = aggr->report;
    n = report->num_windows;
    if (n == 0) return;

    windows_array = report->flow_windows;
    for (i = 0; i < n; i++)
    {
        window = windows_array[i];
        net_md_free_free_window(window);
        windows_array[i] = NULL;
    }

    report->num_windows = 0;
    aggr->windows_cur_idx = 0;
    aggr->stats_cur_idx = 0;
    aggr->active_accs = 0;
    aggr->total_report_flows = 0;
}

size_t net_md_get_total_flows(struct net_md_aggregator *aggr)
{
    if (aggr == NULL) return 0;

    return aggr->total_report_flows;
}
