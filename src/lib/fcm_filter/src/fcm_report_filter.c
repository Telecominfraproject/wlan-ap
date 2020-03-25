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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "os.h"
#include "util.h"
#include "log.h"
#include "fcm_filter.h"
#include "network_metadata_report.h"
#include "fcm_report_filter.h"

static fcm_plugin_filter_t filtername;


static
void net_md_print_net_md_flow_key_and_flow_key(struct net_md_flow_key *key,
                                          struct flow_key *fkey);
/**
 * @brief initialization function filter name.
 *
 * @param valid pointer to net_md_stats_accumulator.
 * @return void.
 */
void fcm_filter_context_init(fcm_collect_plugin_t *collector)
{
    filtername.collect = collector->filters.collect;
    filtername.hist = collector->filters.hist;
    filtername.report = collector->filters.report;
}

/**
 * @brief print function net_md_stats_accumulator paramter.
 *
 * @param valid pointer to net_md_stats_accumulator.
 * @return void.
 */
static void print_md_acc_key(struct net_md_stats_accumulator *md_acc)
{

    LOGT("net_md_stats_accumulator=%p key=%p fkey=%p",
            md_acc, md_acc->key, md_acc->fkey);

    net_md_print_net_md_flow_key_and_flow_key(md_acc->key, md_acc->fkey);

    LOGT("report_counter packets_count = %" PRIx64 "bytes_count = %"PRIx64,
         md_acc->report_counters.packets_count,
         md_acc->report_counters.bytes_count);
}

/**
 * @brief print function net_md_stats_accumulator's key and fkey.
 *
 * receive net_md_stats_accumulator and fill the structure for report filter.
 *
 * @param valid pointer to net_md_flow_key and flow_key.
 * @return void.
 */
static
void net_md_print_net_md_flow_key_and_flow_key(struct net_md_flow_key *key,
                                          struct flow_key *fkey)
{
    char src_ip[INET6_ADDRSTRLEN] = {0};
    char dst_ip[INET6_ADDRSTRLEN] = {0};

    if (key != NULL)
    {
        inet_ntop(AF_INET, key->src_ip, src_ip, INET6_ADDRSTRLEN);
        inet_ntop(AF_INET, key->dst_ip, dst_ip, INET6_ADDRSTRLEN);

        LOGD("%s: Printing key => net_md_flow_key :: fkey => flow_key",
             __func__);
        LOGD("------------");
        LOGD(" smac:" PRI_os_macaddr_lower_t \
             " dmac:" PRI_os_macaddr_lower_t \
             " vlanid: %d" \
             " ethertype: %d" \
             " ip_version: %d" \
             " src_ip: %s" \
             " dst_ip: %s" \
             " ipprotocol: %d" \
             " sport: %d" \
             " dport: %d",
            FMT_os_macaddr_t(*(key->smac)),
            FMT_os_macaddr_t(*(key->dmac)),
            key->vlan_id,
            key->ethertype,
            key->ip_version,
            src_ip,
            dst_ip,
            key->ipprotocol,
            ntohs(key->sport),
            ntohs(key->dport));
        LOGD("------------");
    }
    if (fkey != NULL)
    {
        /* LOGD("%s: Printing fkey => flow_key", __func__); */
        LOGD(" smac: %s" \
             " dmac: %s" \
             " vlanid: %d" \
             " ethertype: %d" \
             " src_ip: %s" \
             " dst_ip: %s" \
             " protocol: %d" \
             " sport: %d" \
             " dport: %d",
             fkey->smac,
             fkey->dmac,
             fkey->vlan_id,
             fkey->ethertype,
             fkey->src_ip,
             fkey->dst_ip,
             fkey->protocol,
             fkey->sport,
             fkey->dport);
        LOGD("------------");
    }
}


/**
 * @brief call 7 tuple filter for report filter.
 *
 * receive net_md_stats_accumulator and fill the structure for report filter.
 *
 * @param valid pointer to net_md_stats_accumulator.
 * @return true for include, false for exclude.
 */
static bool apply_report_filter(struct net_md_stats_accumulator *md_acc)
{
    bool action;
    fcm_filter_l2_info_t mac_filter;
    fcm_filter_l3_info_t filter;
    fcm_filter_stats_t pkt;

    if (filtername.report == NULL)
    {
        /* no filter name default included */
        return true;
    }
    snprintf(mac_filter.src_mac, sizeof(mac_filter.src_mac),
                 PRI_os_macaddr_lower_t,
                 FMT_os_macaddr_t(*(md_acc->key->smac)));
    snprintf(mac_filter.dst_mac, sizeof(mac_filter.dst_mac),
                 PRI_os_macaddr_lower_t,
                 FMT_os_macaddr_t(*(md_acc->key->dmac)));

    mac_filter.vlan_id = md_acc->key->vlan_id;
    mac_filter.eth_type = md_acc->key->ethertype;

    if (inet_ntop(AF_INET, md_acc->key->src_ip, filter.src_ip,
        INET6_ADDRSTRLEN) == NULL)
    {
        LOGE("inet_ntop src_ip error");
        return false;
    }
    if (inet_ntop(AF_INET, md_acc->key->dst_ip, filter.dst_ip,
        INET6_ADDRSTRLEN) == NULL)
    {
        LOGE("inet_ntop dst_ip error ");
        return false;
    }

    filter.sport = ntohs(md_acc->key->sport);
    filter.dport = ntohs(md_acc->key->dport);
    filter.l4_proto = md_acc->key->ipprotocol;

    /* key->ip_version No ip (0), ipv4 (4), ipv6 (6) */
    filter.ip_type = (md_acc->key->ip_version <= 4)? AF_INET: AF_INET6;

    pkt.pkt_cnt = md_acc->report_counters.packets_count;
    pkt.bytes = md_acc->report_counters.bytes_count;
    (void)pkt;
    fcm_filter_7tuple_apply(filtername.report,
                             &mac_filter,
                             &filter,
                             &pkt,
                             &action);
    return action;
}


/**
 * @brief callback function for network metadata.
 *
 * receive net_md_stats_accumulator and call report filter.
 *
 * @param valid pointer to net_md_stats_accumulator.
 * @return true for include, false for exclude.
 */
bool fcm_filter_nmd_callback(struct net_md_stats_accumulator *md_acc)
{
    if ((md_acc == NULL) || (md_acc->key == NULL))
    {
        return true;
    }
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
    {
        print_md_acc_key(md_acc);
    }
    return apply_report_filter(md_acc);
}
