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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "log.h"
#include "const.h"
#include "network.h"
#include "rtypes.h"
#include "strutils.h"

#include "fsm.h"
#include "fsm_policy.h"
#include "dns_parse.h"
#include "ds_tree.h"
#include "json_mqtt.h"
#include "ovsdb_utils.h"
#include "wc_telemetry.h"


void handler(uint8_t *, const struct pcap_pkthdr *, const uint8_t *);
void dns_rr_free(dns_rr *);
void dns_question_free(dns_question *);
uint32_t parse_rr(uint32_t, uint32_t, struct pcap_pkthdr *,
                  uint8_t *, dns_rr *);
void print_rr_section(dns_rr *, char *, struct dns_session *);
void print_packet(uint32_t, uint8_t *, uint32_t, uint32_t, u_int);

static struct dns_cache cache_mgr =
{
    .initialized = false,
};

struct dns_cache *dns_get_mgr(void)
{
    return &cache_mgr;
}

/**
 * @brief compare DNS messages
 * @param id1 dns message
 * @param id2 dns message
 *
 * Compare DNS messages based on their embedded IDs
 */
static int
dns_req_id_cmp(void *a, void *b)
{
    uint16_t *id_a = a;
    uint16_t *id_b = b;

    return ((int)(*id_a) - (int)(*id_b));
};

/**
 * @brief compare device sessions
 * @param dev_session_a: device session
 * @param dev_session_b: device session
 *
 * Compare device sessions based on their device IDs (MAC address)
 */
static int
dns_dev_id_cmp(void *a, void *b)
{
    os_macaddr_t *dev_id_a = a;
    os_macaddr_t *dev_id_b = b;

    return memcmp(dev_id_a->addr,
                  dev_id_b->addr,
                  sizeof(dev_id_a->addr));
}


static void
dns_process_provider(struct fsm_session *session)
{
    struct dns_session *dns_session;
    struct fsm_session *service;
    bool reset;

    dns_session = session->handler_ctxt;
    /* If no provider plugin yet, attempt update */
    if (dns_session->provider_plugin == NULL)
    {
        service = session->service;
        dns_session->provider_plugin = service;
        if (service != NULL)
        {
            dns_session->provider = strdup(service->name);
            dns_session->provider_ops = &service->p_ops->web_cat_ops;
        }
        return;
    }

    LOGT("%s: original provider %s, new provider %s", __func__,
         dns_session->provider,
         session->service ? session->service->name : "unknown");

    /* Provider was set, check if it changed */
    reset = (dns_session->provider_plugin != session->service);

    if (reset)
    {
        sleep(2);
        LOGEM("%s: provider change detected. Restarting", __func__);
        exit(EXIT_SUCCESS);
    }
}


static void
dns_parse_update(struct fsm_session *session)
{
    struct dns_session *dns_session = session->handler_ctxt;
    char *dbg_str = session->ops.get_config(session, "debug");
    char *cache_ip_str = session->ops.get_config(session, "cache_ip");
    char *mqtt_blocker_topic = session->ops.get_config(session, "blk_mqtt");
    char *policy_table = session->ops.get_config(session, "policy_table");
    char *hs_report_interval;
    char *hs_report_topic;
    long interval;
    int val;

    dns_session->policy_client.name = policy_table;
    dns_process_provider(session);

    dns_session->health_stats_report_interval = (long)INT_MAX;
    hs_report_interval = session->ops.get_config(session,
                                                 "wc_health_stats_interval_secs");
    if (hs_report_interval != NULL)
    {
        interval = strtoul(hs_report_interval,  NULL, 10);
        dns_session->health_stats_report_interval = (long)interval;
    }
    hs_report_topic = session->ops.get_config(session,
                                              "wc_health_stats_topic");
    dns_session->health_stats_report_topic = hs_report_topic;

    if (dbg_str != NULL)
    {
        LOGT("%s: session %p: debug key value: %s",
             __func__, session, dbg_str);
        val = strcmp(dbg_str, "on");
        if (val == 0)
        {
            dns_session->debug = true;
        }
        else
        {
            val = strcmp(dbg_str, "off");
            if (val == 0)
            {
                dns_session->debug = false;
            }
        }
    }
    else
    {
        LOGT("%s: session %p could not find debug key in other_config",
             __func__, session);
        dns_session->debug = false;
    }

    LOGI("%s: debug %s", __func__,
         dns_session->debug == true ? "on" : "off");

    if (cache_ip_str != NULL)
    {
        LOGT("%s: session %p: cache_ip key value: %s",
             __func__, session, cache_ip_str);
        val = strcmp(cache_ip_str, "on");
        if (val == 0)
        {
            dns_session->cache_ip = true;
        } else
        {
            val = strcmp(cache_ip_str, "off");
            if (val == 0)
            {
                dns_session->cache_ip = false;
            }
        }
    }
    else
    {
        LOGT("%s: session %p could not find cache_ip key in other_config",
             __func__, session);
        dns_session->cache_ip = false;
    }
    LOGI("%s: cache_ip %s", __func__,
         dns_session->cache_ip == true ? "on" : "off");

    if (mqtt_blocker_topic != NULL)
    {
        dns_session->blocker_topic = mqtt_blocker_topic;
        LOGT("%s: session %p: mqtt_blocker_topic key value: %s",
             __func__, session, mqtt_blocker_topic);
    }
    else
    {
        LOGT("%s: session %p could not find blocker topic in other_config",
             __func__, session);
        dns_session->blocker_topic = NULL;
    }
    LOGI("%s: mqtt_blocker_topic %s", __func__,
         mqtt_blocker_topic ? mqtt_blocker_topic : "None");
}


/**
 * @brief compare sessions
 *
 * @param a session pointer
 * @param b session pointer
 * @return 0 if sessions matches
 */
static int
dns_session_cmp(void *a, void *b)
{
    uintptr_t p_a = (uintptr_t)a;
    uintptr_t p_b = (uintptr_t)b;

    if (p_a ==  p_b) return 0;
    if (p_a < p_b) return -1;
    return 1;
}


/**
 * @brief look up a session
 *
 * Looks up a session, and allocates it if not found.
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct dns_session *
dns_lookup_session(struct fsm_session *session)
{
    struct dns_cache *mgr;
    struct dns_session *d_session;
    ds_tree_t *sessions;

    mgr = dns_get_mgr();
    sessions = &mgr->fsm_sessions;

    d_session = ds_tree_find(sessions, session);
    return d_session;
}

/**
 * @brief get a session
 *
 * Looks up a session, and allocates it if not found.
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct dns_session *
dns_get_session(struct fsm_session *session)
{
    struct dns_cache *mgr;
    struct dns_session *d_session;
    ds_tree_t *sessions;

    mgr = dns_get_mgr();
    sessions = &mgr->fsm_sessions;

    d_session = ds_tree_find(sessions, session);
    if (d_session != NULL) return d_session;

    LOGD("%s: Adding new session %s", __func__, session->name);
    d_session = calloc(1, sizeof(struct dns_session));
    if (d_session == NULL) return NULL;

    d_session->initialized = false;
    ds_tree_insert(sessions, d_session, session);

    return d_session;
}

void
dns_free_device(struct dns_device *ddev)
{
    free(ddev);
}


/**
 * @brief Frees a dns session
 *
 * @param d_session the dns session to delete
 */
void
dns_free_session(struct dns_session *d_session)
{
    struct dns_device *ddev, *remove;
    ds_tree_t *tree;

    tree = &d_session->session_devices;
    ddev = ds_tree_head(tree);
    while (ddev != NULL)
    {
        remove = ddev;
        ddev = ds_tree_next(tree, ddev);
        ds_tree_remove(tree, remove);
        dns_free_device(remove);
    }
    free(d_session->provider);
    free(d_session);
}


/**
 * @brief deletes a session
 *
 * @param session the fsm session keying the dns session to delete
 */
void
dns_delete_session(struct fsm_session *session)
{
    struct dns_cache *mgr;
    struct dns_session *d_session;
    ds_tree_t *sessions;

    mgr = dns_get_mgr();
    sessions = &mgr->fsm_sessions;

    d_session = ds_tree_find(sessions, session);
    if (d_session == NULL) return;

    LOGD("%s: removing session %s", __func__, session->name);
    fsm_policy_deregister_client(&d_session->policy_client);
    ds_tree_remove(sessions, d_session);
    dns_free_session(d_session);

    return;
}


void
dns_plugin_exit(struct fsm_session *session)
{
    struct dns_cache *mgr;

    mgr = dns_get_mgr();
    if (!mgr->initialized) return;

    dns_delete_session(session);
}


void
dns_update_client(struct fsm_session *session,
                       struct policy_table *table)
{
    struct dns_session * dns_session;
    struct fsm_policy_client *client;

    if (session == NULL) return;
    if (table == NULL) return;

    dns_session = dns_lookup_session(session);
    if (dns_session == NULL) return;

    client = &dns_session->policy_client;
    client->table = table;
}


void
dns_set_provider(struct fsm_session *session)
{
    ds_tree_t *other_config;
    struct str_pair *pair;
    char *provider;

    other_config = session->conf->other_config;
    if (other_config == NULL) return;

    provider = session->ops.get_config(session, "provider_plugin");
    if (provider != NULL) return;

    provider = session->ops.get_config(session, "provider");
    if (provider != NULL) return;

    pair = get_pair("provider", "brightcloud");
    if (pair == NULL) return;

    ds_tree_insert(other_config, pair, pair->key);
    session->p_ops->parser_ops.get_service(session);
}


int
dns_plugin_init(struct fsm_session *session)
{
    struct dns_cache *mgr;
    struct dns_session *dns_session;
    struct ifreq ifr_i;
    struct ifreq ifreq_c;
    struct fsm_policy_client *client;
    struct fsm_parser_ops *parser_ops;
    struct fsm_session *service;
    time_t now;

    mgr = dns_get_mgr();
    /* Initialize the manager on first call */
    if (!mgr->initialized)
    {
        ds_tree_init(&mgr->fsm_sessions, dns_session_cmp,
                     struct dns_session, session_node);
        mgr->initialized = true;
    }

    /* Look up the dns session */
    dns_session = dns_get_session(session);
    if (dns_session == NULL)
    {
        LOGE("%s: could not allocate dns parser", __func__);
        return -1;
    }

    /* Bail if the session is already initialized */
    if (dns_session->initialized) return 0;

    session->ops.update = dns_parse_update;
    session->ops.periodic = dns_periodic;
    session->handler_ctxt = dns_session;
    session->ops.exit = dns_plugin_exit;

    /* Set the plugin specific ops */
    parser_ops = &session->p_ops->parser_ops;
    parser_ops->handler = dns_handler;

    /* Setting configuration defaults. */
    dns_session->RECORD_SEP = "";
    dns_session->SEP = '\t';

    dns_session->ip_config.ip_fragment_head = NULL;
    dns_session->fsm_context = session;

    ds_tree_init(&dns_session->device_sessions, dns_dev_id_cmp,
                 struct dns_device, device_node);

    /* Open raw socket to re-inject DNS responses */
    dns_session->sock_fd = socket(AF_PACKET, SOCK_RAW, 0);
    if (dns_session->sock_fd < 0)
    {
        LOGE("%s: socket() failed (%s)",
             __func__, strerror(errno));
        goto error;
    }

    memset(&ifr_i, 0, sizeof(ifr_i));
    STRSCPY(ifr_i.ifr_name, session->tx_intf);

    if ((ioctl(dns_session->sock_fd, SIOCGIFINDEX, &ifr_i)) < 0)
    {
        LOGE("%s: error in index ioctl reading (%s)",
             __func__, strerror(errno));
        goto error;
    }

    dns_session->raw_dst.sll_family = PF_PACKET;
    dns_session->raw_dst.sll_ifindex = ifr_i.ifr_ifindex;
    dns_session->raw_dst.sll_halen = ETH_ALEN;

    memset(&ifreq_c, 0, sizeof(ifreq_c));
    STRSCPY(ifreq_c.ifr_name, session->tx_intf);

    if ((ioctl(dns_session->sock_fd, SIOCGIFHWADDR, &ifreq_c)) < 0)
    {
        LOGE("%s: error in SIOCGIFHWADDR ioctl reading (%s)",
             __func__, strerror(errno));
        goto error;
    }

    memcpy(dns_session->src_eth_addr.addr, ifreq_c.ifr_hwaddr.sa_data, 6);
    now = time(NULL);
    dns_session->stat_report_ts = now;
    dns_session->stat_log_ts = now;
    dns_session->debug = false;
    dns_set_provider(session);
    dns_parse_update(session);
    fsm_policy_init();

    client = &dns_session->policy_client;
    client->session = session;
    client->update_client = dns_update_client;
    fsm_policy_register_client(&dns_session->policy_client);
    ds_tree_init(&dns_session->session_devices,dns_dev_id_cmp,
                 struct dns_device, device_node);
    service = session->service;
    if (session->service)
    {
        dns_session->provider_ops = &service->p_ops->web_cat_ops;
    }
    dns_session->cat_offline.check_offline = 30;
    dns_session->cat_offline.provider_offline = false;
    dns_session->initialized = true;
    return 0;

error:
    free(dns_session);
    return -1;
}


static void
dns_prepare_forward(uint8_t *packet,
                    struct dns_session *dns_session)
{
    ip_info *ip = &dns_session->ip;
    ip_addr *src = &ip->src;
    ip_addr *dst = &ip->dst;
    char buf[128] = { 0 };

    LOGD("%s: source mac: " PRI(os_macaddr_t) ", dst mac: " PRI(os_macaddr_t),
        __func__,
        FMT(os_macaddr_t, dns_session->eth_hdr.srcmac),
        FMT(os_macaddr_t, dns_session->eth_hdr.dstmac));

    if (ip->src.vers == IPv4)
    {
        /* UDP checksum to 0 */
        memset(dns_session->udp.udp_csum_ptr, 0, 2);

        inet_ntop(AF_INET, &dst->addr.v4.s_addr, buf, sizeof(buf));
        LOGD("%s: dst address %s, dst port %d, checksum 0x%x",
             __func__,
             buf, dns_session->udp.dstport, dns_session->udp.udp_checksum);

        inet_ntop(AF_INET, &src->addr.v4.s_addr, buf, sizeof(buf));
        LOGD("%s: src address %s, src port %d",
             __func__, buf, dns_session->udp.srcport);

    }
    else
    {   /* IPv6 */
        uint16_t csum = compute_udp_checksum(packet, ip, &dns_session->udp);

        *(uint16_t *)dns_session->udp.udp_csum_ptr = csum;
    }

    memcpy(dns_session->raw_dst.sll_addr, dns_session->eth_hdr.dstmac.addr,
           sizeof(dns_session->eth_hdr.dstmac.addr));
    memcpy(packet + 6, dns_session->src_eth_addr.addr,
           sizeof(dns_session->src_eth_addr.addr));

}


static void
process_response_ip(struct fqdn_pending_req *req,
                    char *ip, int len)
{
    if (len == INET_ADDRSTRLEN)
    {
        if (req->ipv4_cnt == MAX_RESOLVED_ADDRS) return;

        req->ipv4_addrs[req->ipv4_cnt] = strdup(ip);
        req->ipv4_cnt++;
    }

    if (len == INET6_ADDRSTRLEN)
    {
        if (req->ipv6_cnt == MAX_RESOLVED_ADDRS) return;

        req->ipv6_addrs[req->ipv6_cnt] = strdup(ip);
        req->ipv6_cnt++;
    }
}


static void
process_response_ips(dns_info *dns, uint8_t *packet,
                     struct fqdn_pending_req *req)
{
    dns_rr *answer;
    const char *res;
    int qtype = -1;
    int i = 0;

    if (dns == NULL) return;

    if (dns->queries == NULL) LOGT("%s: no queries", __func__);

    answer = dns->answers;
    qtype = dns->queries->type;
    LOGT("%s: query type: %d",
         __func__, qtype);

    for (i = 0; i < dns->ancount && answer != NULL; i++)
    {
        LOGT("%s: answer %d type: %d",
             __func__, i, qtype);
        if (answer->type == qtype)
        {
            LOGT("%s: type %d answer, addr %s",
                 __func__, qtype, answer->data);
            if (qtype == 1) /* IPv4 redirect */
            {
                char ipv4_addr[INET_ADDRSTRLEN];

                res = inet_ntop(AF_INET, packet + answer->type_pos + 10,
                                ipv4_addr, INET_ADDRSTRLEN);
                if (res == NULL)
                {
                    LOGE("%s: inet_ntop failed: %s", __func__,
                         strerror(errno));
                }
                else
                {
                    process_response_ip(req, ipv4_addr, INET_ADDRSTRLEN);
                }
            }
            else if (qtype == 28) /* IPv6 */
            {
                char ipv6_addr[INET6_ADDRSTRLEN];

                res = inet_ntop(AF_INET6, packet + answer->type_pos + 10,
                                ipv6_addr, INET6_ADDRSTRLEN);
                if (res == NULL)
                {
                    LOGE("%s: inet_ntop failed: %s", __func__,
                         strerror(errno));
                }
                else
                {
                    process_response_ip(req, ipv6_addr, INET6_ADDRSTRLEN);
                }
            }
        }
        answer = answer->next;
    }
}


void
dns_forward(struct dns_session *dns_session, dns_info *dns,
            uint8_t *packet, int len)
{
    int rc = 0;

    dns_prepare_forward(packet, dns_session);
    rc = sendto(dns_session->sock_fd, packet, len, 0,
                (struct sockaddr *)&dns_session->raw_dst,
                sizeof(dns_session->raw_dst));
    if (rc < 0)
    {
        LOGE("Could not forward DNS response (%s)", strerror(errno));
    }
}


static char redirect_prefix[RD_SZ][4] =
{
    "A-", "4A-", "C-"
};

static char *
check_redirect(char *redirect, int id)
{
    char *cmp = redirect_prefix[id];

    LOGT("%s: redirect: %s", __func__, redirect);
    if (strncmp(redirect, cmp, strlen(cmp)) == 0)
    {
        return redirect + strlen(cmp);
    }
    return NULL;
}


static bool
update_a_rrs(dns_info *dns, uint8_t *packet,
             struct fqdn_pending_req *req)
{
    dns_rr *answer = dns->answers;
    bool updated = false;
    int qtype = -1;
    int i = 0;

    if (dns->queries == NULL)
    {
        LOGT("%s: no queries", __func__);
        return false;
    }

    if (req->redirect == false) return false;

    qtype = dns->queries->type;
    LOGT("%s: query type: %d",
         __func__, qtype);
    for (i = 0; i < dns->ancount && answer != NULL; i++)
    {
        LOGT("%s: answer %d type: %d",
             __func__, i, qtype);
        if (answer->type == qtype)
        {
            uint8_t *p_ttl = packet + answer->type_pos + 4;
            LOGT("%s: type %d answer, addr %s",
                 __func__, qtype, answer->data);
            if (qtype == 1)  /* IPv4 redirect */
            {
                char *ipv4_addr = check_redirect(req->redirects[0],
                                                 IPv4_REDIRECT);
                if (ipv4_addr == NULL)
                {
                    ipv4_addr = check_redirect(req->redirects[1],
                                               IPv4_REDIRECT);
                }
                if (ipv4_addr != NULL)
                {
                    inet_pton(AF_INET, ipv4_addr,
                              packet + answer->type_pos + 10);
                    if (req->rd_ttl != -1)
                    {
                        *(uint32_t *)(p_ttl) = htonl(req->rd_ttl);
                    }
                    updated |= true;
                }
            }
            else if (qtype == 28)  /* IPv6 */
            {
                LOGT("%s: IPv6 record, rdlength == %d",
                     __func__, answer->rdlength);
                char *ipv6_addr = check_redirect(req->redirects[0],
                                                 IPv6_REDIRECT);
                if (ipv6_addr == NULL)
                {
                    ipv6_addr = check_redirect(req->redirects[1],
                                               IPv6_REDIRECT);
                }
                if (ipv6_addr != NULL)
                {
                    inet_pton(AF_INET6, ipv6_addr,
                              packet + answer->type_pos + 10);
                    if (req->rd_ttl != -1)
                    {
                        *(uint32_t *)(p_ttl) = htonl(req->rd_ttl);
                    }
                    updated |= true;
                }
            }
        }
        answer = answer->next;
    }
    return updated;
}


static void
dns_handle_reply(struct dns_session *dns_session, dns_info *dns,
                 eth_info *eth, struct pcap_pkthdr *header,
                 uint8_t *packet)
{
    struct dns_device *ds = NULL;
    struct fqdn_pending_req *req = NULL;
    struct fsm_session *session;

    LOGD("dns reply: looking up device " PRI(os_macaddr_lower_t),
         FMT(os_macaddr_t, eth->dstmac));

    session = dns_session->fsm_context;
    ds = ds_tree_find(&dns_session->device_sessions, &eth->dstmac);
    if (ds == NULL)
    {
        LOGD("dns reply: could not find device " PRI(os_macaddr_lower_t),
             FMT(os_macaddr_t, eth->dstmac));
        goto free_out;
    }

    LOGD("dns reply: looking up request %u type %d",
         dns->id, dns->answers ? dns->answers->type : -1);
    req = ds_tree_find(&ds->fqdn_pending_reqs, &dns->id);
    if (req == NULL)
    {
        LOGD("dns reply: could not retrieve request %u type %d",
             dns->id, dns->answers ? dns->answers->type : -1);
        goto free_out;
    }

    process_response_ips(dns, packet, req);

    /* forward the DNS response if the session has no categorization provider */
    if (dns_session->provider_ops == NULL)
    {
        LOGT("%s: %s session : no provider available", __func__,
             session->name);
        dns_forward(dns_session, dns, packet, header->caplen);
        dns_remove_req(dns_session, &eth->dstmac, req->req_id);
        goto free_out;
    }

    /* forward DNS response to category filter */
    if (dns_session->provider_ops->dns_response)
    {
        LOGT("%s: dns reply: forwarding to category provider %s", __func__,
             dns_session->provider);
        dns_session->provider_ops->dns_response(session, req);
    }

    if (req->action == FSM_FORWARD)
    {
        dns_forward(dns_session, dns, packet, header->caplen);
        dns_remove_req(dns_session, &eth->dstmac, req->req_id);
    }
    else if (dns->ancount == 0)
    {
        /*
         * If the DNS server did not provide a meaningful answer,
         * forward the reply
         */
        dns_forward(dns_session, dns, packet, header->caplen);
        dns_remove_req(dns_session, &eth->dstmac, req->req_id);
    }
    else if (req->action == FSM_BLOCK)
    {
        char reason[128] = { 0 };
        char risk[128] = { 0 };

        if (req->categorized == FSM_FQDN_CAT_SUCCESS) {
            if (dns_session->provider_ops->cat2str && (req->cat_match != -1))
            {
                snprintf(reason, sizeof(reason), "categorized as %s",
                         dns_session->provider_ops->cat2str(session,
                                                            req->cat_match));
            }
            if (req->risk_level != -1)
            {
                snprintf(risk, sizeof(risk), "risk level %d",
                         req->risk_level);
            }
        }
        else if (req->categorized == FSM_FQDN_CAT_NOP)
        {
            snprintf(reason, sizeof(reason), "(blacklisted)");
        }
        else
        {
            snprintf(reason, sizeof(reason), "for unclear reasons");
        }
        LOGI("device " PRI(os_macaddr_lower_t)
             ": blocking access to %s %s %s",
             FMT(os_macaddr_t, eth->dstmac),
             req->req_info->url,
             reason, risk);
        if (update_a_rrs(dns, packet, req) == true)
        {
            dns_forward(dns_session, dns, packet, header->caplen);
        }
        dns_remove_req(dns_session, &eth->dstmac, req->req_id);
    }
    else if (req->redirect == true)
    {
        update_a_rrs(dns, packet, req);
        dns_forward(dns_session, dns, packet, header->caplen);
        dns_remove_req(dns_session, &eth->dstmac, req->req_id);
    }
    else if (req->fsm_checked == true)
    {
        dns_forward(dns_session, dns, packet, header->caplen);
        dns_remove_req(dns_session, &eth->dstmac, req->req_id);
    }
    else
    {
        LOGD("stashing dns reply %u", req->req_id);
        req->response = calloc(1, header->caplen);
        if (req->response == NULL)
        {
            LOGE("Could not allocate memory for dns response %d",
                 req->req_id);
            dns_remove_req(dns_session, &eth->dstmac, req->req_id);
        }
        else
        {
            memcpy(req->response, packet, header->caplen);
            req->response_len = header->caplen;
        }
    }

  free_out:
    free_rrs(&dns_session->ip, &dns_session->udp, dns, header);
    return;
}


/**
 * @brief set web categorization provider ops to the DNS request
 *
 * Sets the request's web categorization operations. If not set,
 * the policy engine will pass categorization calls.
 * operations are not set if:
 * - no web categorization backend was provided
 * - the categorization is offline
 * @param dns_session the session container
 * @param req the request to be processed by the policy engine
 */
void
set_provider_ops(struct dns_session *dns_session,
                 struct fqdn_pending_req *req)
{
    struct web_cat_offline *offline;

    /* No backend available. Bail */
    if (dns_session->provider_ops == NULL) return;

    /* Check if the backend provider is offline */
    offline = &dns_session->cat_offline;
    if (offline->provider_offline)
    {
        time_t now = time(NULL);
        bool backoff;

        backoff = ((now - offline->offline_ts) < offline->check_offline);

        /* Within the back off interval. Bail */
        if (backoff) return;

        /* Out of the back off interval. Reset offline marker */
        offline->provider_offline = false;
    }

    /* Set the backend provider ops */
    req->categories_check = dns_session->provider_ops->categories_check;
    req->risk_level_check = dns_session->provider_ops->risk_level_check;
}


void
dns_handler(struct fsm_session *session, struct net_header_parser *net_header)
{
    struct dns_session *dns_session;
    eth_info *eth;
    dns_question *qnext;
    os_macaddr_t *mac = NULL;
    struct fsm_url_request *req_info = NULL;
    int cnt = 0;
    int pos, i;
    dns_info dns = { 0 };
    struct dns_device *ds = NULL;
    struct fqdn_pending_req *req = NULL;
    struct pcap_pkthdr header;
    uint8_t * packet;

    dns_session = (struct dns_session *)session->handler_ctxt;
    eth = &dns_session->eth_hdr;

    /*
     * The way we handle IP fragments means we may have to replace
     * the original data and correct the header info, so a const won't work.
     */
    packet = (uint8_t *)net_header->start;
    header.caplen = net_header->caplen;
    header.len = net_header->caplen;

    if (dns_session->debug == true)
    {
        size_t len = (sizeof(dns_session->debug_pkt_copy) > header.len ?
                      header.len : sizeof(dns_session->debug_pkt_copy));
        memcpy(dns_session->debug_pkt_copy, packet, len);
        dns_session->debug_pkt_len = len;
    }

    pos = eth_parse(&header, packet, eth, &dns_session->eth_config);
    if (pos == 0) return;

    dns_session->post_eth = pos;

    if (eth->ethtype == 0x0800)
    {
        pos = ipv4_parse(pos, &header, &packet, &dns_session->ip,
                         &dns_session->ip_config);
    }
    else if (eth->ethtype == 0x86DD)
    {
        pos = ipv6_parse(pos, &header, &packet, &dns_session->ip,
                         &dns_session->ip_config);
    }
    else
    {
        LOGD("%s: Unsupported EtherType: %04x\n", __func__, eth->ethtype);
        return;
    }

    if (packet == NULL) return;

    if (dns_session->ip.proto != 17) return;

    pos = udp_parse(pos, &header, packet, &dns_session->udp);
    if (pos == 0) return;

    dns_session->data_offset = pos;
    pos = dns_parse(pos, &header, packet, &dns, dns_session, !FORCE);
    if (dns.qdcount == 0)
    {
        LOGD("%s: dropping packet with no question", __func__);
        free_rrs(&dns_session->ip, &dns_session->udp, &dns, &header);
        return;
    }

    dns_session->last_byte_pos = pos;
    if (dns.qr == 1)
    {
        dns_handle_reply(dns_session, &dns, eth, &header, packet);
        return;
    }

    qnext = dns.queries;
    mac = (dns.qr == 0 ? &eth->srcmac : &eth->dstmac);

    LOGD("%s: looking up device " PRI_os_macaddr_lower_t,
         __func__, FMT_os_macaddr_pt(mac));
    ds = ds_tree_find(&dns_session->device_sessions, mac);

    /* Look for the device in the device tree */
    if (ds == NULL)
    {
        ds = calloc(sizeof(*ds), 1);
        memcpy(ds->device_mac.addr, eth->srcmac.addr,
               sizeof(ds->device_mac.addr));
        ds_tree_init(&ds->fqdn_pending_reqs, dns_req_id_cmp,
                     struct fqdn_pending_req, req_node);
        ds_tree_insert(&dns_session->device_sessions, ds,
                       &ds->device_mac);
    }

    /* Check if the request is a duplicate */
    req = ds_tree_find(&ds->fqdn_pending_reqs, &dns.id);
    if (req != NULL)
    {
        LOGT("%s: request id %d already pending",
             __func__, dns.id);
        req->dedup++;
        req->timestamp = time(NULL);
        free_rrs(&dns_session->ip, &dns_session->udp, &dns, &header);
        return;
    }

    /* Insert the pending request */
    req = calloc(sizeof(*req), 1);
    memcpy(req->dev_id.addr, eth->srcmac.addr,
           sizeof(req->dev_id.addr));
    req->req_id = dns.id;
    req->dedup = 1;
    req->timestamp = time(NULL);
    req->req_info = calloc(sizeof(struct fsm_url_request),
                           dns.qdcount);
    req->fsm_context = dns_session->fsm_context;
    req->send_report = dns_session->fsm_context->ops.send_report;
    req->dev_session = ds;
    req->redirect = false;
    req->to_report = false;
    req->fsm_checked = false;
    req->risk_level = -1;
    req->cat_match = -1;
    req->policy_table = dns_session->policy_client.table;

    set_provider_ops(dns_session, req);
    req->provider = dns_session->provider;
    req_info = req->req_info;
    qnext = dns.queries;
    for (i = 0; i < dns.qdcount; i++)
    {
        if ((qnext->type == 0x1) || (qnext->type == 0x1c))
        {
            STRSCPY(req_info->url, qnext->name);
            memcpy(&req_info->dev_id, &eth->srcmac,
                   sizeof(req_info->dev_id));
            req_info->req_id = req->req_id;
            req_info++;
            cnt++;
        }
        qnext = qnext->next;
    }
    req->numq = cnt;
    dns_policy_check(ds, req);
    free_rrs(&dns_session->ip, &dns_session->udp, &dns, &header);
}


/* Free DNS data. */
void
free_rrs(ip_info * ip, transport_info * trns, dns_info * dns,
              struct pcap_pkthdr * header)
{
    dns_question_free(dns->queries);
    dns_rr_free(dns->answers);
    dns_rr_free(dns->name_servers);
    dns_rr_free(dns->additional);
}


/* Free a dns_rr struct. */
void
dns_rr_free(dns_rr * rr)
{
    if (rr == NULL) return;
    if (rr->name != NULL) free(rr->name);
    if (rr->data != NULL) free(rr->data);
    dns_rr_free(rr->next);
    free(rr);
}


/* Free a dns_question struct. */
void
dns_question_free(dns_question * question)
{
    if (question == NULL) return;

    if (question->name != NULL) free(question->name);
    dns_question_free(question->next);
    free(question);
}


/*
 * Parse the questions section of the dns protocol.
 * pos - offset to the start of the questions section.
 * id_pos - offset set to the id field. Needed to decompress dns data.
 * packet, header - the packet location and header data.
 * count - Number of question records to expect.
 * root - Pointer to where to store the question records.
 */
uint32_t
parse_questions(uint32_t pos, uint32_t id_pos,
                struct pcap_pkthdr *header,
                uint8_t *packet, uint16_t count,
                dns_question ** root)
{
    uint32_t start_pos = pos;
    dns_question * last = NULL;
    dns_question * current;
    uint16_t i;

    *root = NULL;

    for (i = 0; i < count; i++)
    {
        current = malloc(sizeof(dns_question));
        current->next = NULL; current->name = NULL;

        current->name = read_rr_name(packet, &pos, id_pos, header->len);
        if (current->name == NULL || (pos + 2) >= header->len)
        {
            /* Handle a bad DNS name. */
            LOGD("DNS question error");
            char * buffer = escape_data(packet, start_pos, header->len);
            const char * msg = "Bad DNS question: ";
            current->name = malloc(sizeof(char) * (strlen(buffer) +
                                                   strlen(msg) + 1));
            sprintf(current->name, "%s%s", msg, buffer);
            free(buffer);
            current->type = 0;
            current->cls = 0;
            if (last == NULL) *root = current;
            else last->next = current;
            return 0;
        }
        current->type = (packet[pos] << 8) + packet[pos+1];
        current->cls = (packet[pos+2] << 8) + packet[pos+3];

        /* Add this question object to the list. */
        if (last == NULL) *root = current;
        else last->next = current;
        last = current;
        pos = pos + 4;
   }

    return pos;
}

#define plume_NULL_DOC "Plume"

/*
 * Parse an individual resource record, placing the acquired data in 'rr'.
 * 'packet', 'pos', and 'id_pos' serve the same uses as in parse_rr_set.
 * Return 0 on error, the new 'pos' in the packet otherwise.
 */
uint32_t
parse_rr(uint32_t pos, uint32_t id_pos, struct pcap_pkthdr *header,
         uint8_t *packet, dns_rr * rr)
{
    int i;
    uint32_t rr_start = pos;
    rr_parser_container * parser;
    rr_parser_container opts_cont = {0, 0, opts, "Plume", plume_NULL_DOC, 0};

    rr->name = NULL;
    rr->data = NULL;

    rr->name = read_rr_name(packet, &pos, id_pos, header->len);
    /*
     * Handle a bad rr name.
     * We still want to print the rest of the escaped rr data.
     */
    if (rr->name == NULL)
    {
        const char * msg = "Bad rr name: ";

        rr->name = malloc(sizeof(char) * (strlen(msg) + 1));
        sprintf(rr->name, "%s", "Bad rr name");
        rr->type = 0;
        rr->rr_name = NULL;
        rr->cls = 0;
        rr->ttl = 0;
        rr->data = escape_data(packet, pos, header->len);
        return 0;
    }

    if ((header->len - pos) < 10 ) return 0;

    rr->type = (packet[pos] << 8) + packet[pos+1];
    rr->type_pos = pos;
    rr->rdlength = (packet[pos+8] << 8) + packet[pos + 9];
    /* Handle edns opt RR's differently. */
    if (rr->type == 41)
    {
        rr->cls = 0;
        rr->ttl = 0;
        rr->rr_name = "OPTS";
        parser = &opts_cont;
        /*
         * We'll leave the parsing of the special EDNS opt fields to
         * our opt rdata parser.
         */
        pos = pos + 2;
    }
    else
    {
        /* The normal case. */
        rr->cls = (packet[pos+2] << 8) + packet[pos+3];
        rr->ttl = 0;
        for (i = 0; i < 4; i++)
        {
            rr->ttl = (rr->ttl << 8) + packet[pos+4+i];
        }
        /* Retrieve the correct parser function. */
        parser = find_parser(rr->cls, rr->type);
        rr->rr_name = parser->name;
        pos = pos + 10;
    }

    /*
     * Make sure the data for the record is actually there.
     * If not, escape and print the raw data.
     */
    if (header->len < (rr_start + 10 + rr->rdlength))
    {
        char * buffer;
        const char * msg = "Truncated rr: ";
        rr->data = escape_data(packet, rr_start, header->len);
        buffer = malloc(sizeof(char) * (strlen(rr->data) + strlen(msg) + 1));
        sprintf(buffer, "%s%s", msg, rr->data);
        free(rr->data);
        rr->data = buffer;
        return 0;
    }
    /* Parse the resource record data. */
    rr->data = parser->parser(packet, pos, id_pos, rr->rdlength,
                              header->len);
    return pos + rr->rdlength;
}


/*
 * Parse a set of resource records in the dns protocol in 'packet', starting
 * at 'pos'. The 'id_pos' offset is necessary for putting together
 * compressed names. 'count' is the expected number of records of this type.
 * 'root' is where to assign the parsed list of objects.
 * Return 0 on error, the new 'pos' in the packet otherwise.
 */
uint32_t
parse_rr_set(uint32_t pos, uint32_t id_pos,
             struct pcap_pkthdr *header,
             uint8_t *packet, uint16_t count,
             dns_rr ** root)
{
    dns_rr * last = NULL;
    dns_rr * current;
    uint16_t i;

    *root = NULL;
    for (i = 0; i < count; i++)
    {
        /* Create and clear the data in a new dns_rr object. */
        current = malloc(sizeof(dns_rr));
        current->next = NULL; current->name = NULL; current->data = NULL;

        pos = parse_rr(pos, id_pos, header, packet, current);
        /*
         * If a non-recoverable error occurs when parsing an rr,
         *  we can only return what we've got and give up.
         */
        if (pos == 0)
        {
            if (last == NULL) *root = current;
            else last->next = current;
            return 0;
        }
        if (last == NULL) *root = current;
        else last->next = current;
        last = current;
    }
    return pos;
}


/*
 * Parse the dns protocol in 'packet'.
 * See RFC1035
 * See dns_parse.h for more info.
 */
uint32_t
dns_parse(uint32_t pos, struct pcap_pkthdr *header,
          uint8_t *packet, dns_info *dns,
          struct dns_session *dns_session, uint8_t force)
{
    uint32_t id_pos = pos;

    if (header->len - pos < 12)
    {
        return 0;
    }

    dns->id = (packet[pos] << 8) + packet[pos+1];
    dns->qr = packet[pos+2] >> 7;
    dns->opcode = (packet[pos+2] & (0x7f)) >> 1;
    dns->AA = (packet[pos+2] & 0x04) >> 2;
    dns->TC = (packet[pos+2] & 0x02) >> 1;
    dns->Z = (packet[pos+3] >> 6) & 1;
    dns->rcode = packet[pos + 3] & 0x0f;
    /*
     * rcodes > 5 indicate various protocol errors and redefine most of the
     * remaining fields. Parsing this would hurt more than help.
     */
    if ((dns->Z != 0) || ((int16_t)(dns->opcode) > 5) ||
        ((int16_t)(dns->rcode) > 5))
    {
        LOGD("%s: ignoring request with opcode %u Z bit %u rcode %u",
             __func__, dns->opcode, dns->Z, dns->rcode);
        dns->qdcount = dns->ancount = dns->nscount = dns->arcount = 0;
        dns->queries = NULL;
        dns->answers = NULL;
        dns->name_servers = NULL;
        dns->additional = NULL;
        return pos + 12;
    }

    LOGD("%s: transaction id %d, type %s", __func__,
         dns->id, dns->qr == 0 ? "query" : "response");

    /* Counts for each of the record types. */
    dns->qdcount = (packet[pos+4] << 8) + packet[pos+5];
    dns->ancount = (packet[pos+6] << 8) + packet[pos+7];
    dns->nscount = (packet[pos+8] << 8) + packet[pos+9];
    dns->arcount = (packet[pos+10] << 8) + packet[pos+11];

    if (((int16_t)(dns->qdcount) > 1) || ((int16_t)(dns->ancount) > 40))
    {
        LOGD("%s: ignoring request with qdcount %u ancount %u",
             __func__, dns->qdcount, dns->ancount);
        dns->qdcount = dns->ancount = dns->nscount = dns->arcount = 0;
        dns->queries = NULL;
        dns->answers = NULL;
        dns->name_servers = NULL;
        dns->additional = NULL;
        return pos + 12;
    }

    /* Parse each type of records in turn. */
    pos = parse_questions(pos+12, id_pos, header, packet,
                          dns->qdcount, &(dns->queries));
    if (pos != 0)
    {
        dns->answer_pos = pos;
        pos = parse_rr_set(pos, id_pos, header, packet,
                           dns->ancount, &(dns->answers));
    }
    else
    {
        dns->answers = NULL;
    }
    if (pos != 0 &&
        (dns_session->NS_ENABLED || dns_session->AD_ENABLED || force))
    {
        pos = parse_rr_set(pos, id_pos, header, packet,
                           dns->nscount, &(dns->name_servers));
    }
    else
    {
        dns->name_servers = NULL;
    }
    if (pos != 0 && (dns_session->AD_ENABLED || force))
    {
        pos = parse_rr_set(pos, id_pos, header, packet,
                           dns->arcount, &(dns->additional));
    }
    else
    {
        dns->additional = NULL;
    }

    return pos;
}


static void
dns_send_report(struct fqdn_pending_req *req)
{
    struct fsm_session *session = req->fsm_context;
    char *report;
    char *topic = session->topic;
    struct dns_session *context = session->handler_ctxt;

    if (req->to_report != true) return;

    if (req->action == FSM_BLOCK &&
        context->blocker_topic != NULL)
    {
        LOGT("%s: Switching topic to %s", __func__, context->blocker_topic);
        session->topic = context->blocker_topic;
    }
    report = jencode_url_report(session, req);
    session->ops.send_report(session, report);
    session->topic = topic;
}


static void
dns_free_req(struct fqdn_pending_req *req)
{
    struct fsm_url_request *req_info;
    int i;

    if (req->response != NULL) free(req->response);
    if (req->rule_name != NULL) free(req->rule_name);
    if (req->policy != NULL) free(req->policy);

    for (i = 0; i < req->ipv4_cnt; i++) free(req->ipv4_addrs[i]);

    for (i = 0; i < req->ipv6_cnt; i++) free(req->ipv6_addrs[i]);

    req_info = req->req_info;
    for (i = 0; i < req->numq; i++)
    {
        free(req_info->reply);
        free(req_info);
    }
    free(req);
}


void
dns_remove_req(struct dns_session *dns_session, os_macaddr_t *mac,
               uint16_t req_id)
{
    struct dns_device *ds = NULL;
    struct fqdn_pending_req *req = NULL;

    ds = ds_tree_find(&dns_session->device_sessions, mac);
    if (ds == NULL)
    {
        LOGE("%s: could not find device " PRI_os_macaddr_lower_t,
             __func__, FMT_os_macaddr_pt(mac));
        return;
    }

    req = ds_tree_find(&ds->fqdn_pending_reqs, &req_id);
    if (req == NULL)
    {
        LOGD("%s: could not retrieve request %d",
             __func__, req_id);
        return;
    }

    if (--req->dedup != 0)
    {
        LOGT("%s: req id %u has dedup value %d",
             __func__, req_id, req->dedup);
        return;
    }

    if ((req->categorized == FSM_FQDN_CAT_FAILED) &&
        (req->req_info->reply->lookup_status != -1))
    {
        dns_session->reported_lookup_failures++;
    }

    LOGD("Removing req id %u for device " PRI_os_macaddr_lower_t,
         req_id, FMT_os_macaddr_pt(mac));
    dns_send_report(req);

    ds_tree_remove(&ds->fqdn_pending_reqs, req);
    dns_free_req(req);
}


/* Local log interval */
#define DNS_LOG_PERIODIC 120

static void dns_log_stats(struct dns_session *dns_session,
                          struct fsm_url_stats *stats, time_t now)
{
    struct fsm_session *session;

    session = dns_session->fsm_context;
    LOGI("DNS session %s (%s) activity report",
         session->name,
         session->service->name);

    LOGI("dns: reported lookup failures: %d",
         dns_session->reported_lookup_failures);
    if (dns_session->remote_lookup_retries != 0)
    {
        LOGI("dns: reported lookup retries: %d",
             dns_session->remote_lookup_retries);
    }
    LOGI("dns: cloud lookups: %" PRId64, stats->cloud_lookups);
    LOGI("dns: cloud lookup errors: %" PRId64,
         stats->cloud_lookup_failures);
    LOGI("dns: cloud hits: %" PRId64, stats->cloud_hits);
    LOGI("dns: cloud categorization errors: %" PRId64,
         stats->categorization_failures);
    LOGI("dns: cloud uncategorized responses: %" PRId64,
         stats->uncategorized);
    LOGI("dns: cache lookups: %" PRId64, stats->cache_lookups);
    LOGI("dns: cache hits: %" PRId64, stats->cache_hits);
    LOGI("dns: cache entries: [%" PRId64 "/%" PRId64 "]",
         stats->cache_entries, stats->cache_size);
    LOGI("dns: min lookup latency in ms: %" PRId64,
         stats->min_lookup_latency);
    LOGI("dns: max lookup latency in ms: %" PRId64,
         stats->max_lookup_latency);
    LOGI("dns: avg lookup latency in ms: %" PRId64,
         stats->avg_lookup_latency);

    dns_session->stat_log_ts = now;
}


static void
dns_report_health_stats(struct dns_session *dns_session,
                        struct fsm_url_stats *stats,
                        time_t now)
{
    struct wc_packed_buffer *serialized;
    struct fsm_url_stats *prev_stats;
    struct wc_observation_window ow;
    struct wc_observation_point op;
    struct wc_stats_report report;
    struct fsm_session *session;
    struct wc_health_stats hs;
    uint32_t count;

    memset(&report, 0, sizeof(report));
    memset(&ow, 0, sizeof(ow));
    memset(&op, 0, sizeof(op));
    memset(&hs, 0, sizeof(hs));
    session = dns_session->fsm_context;

    /* Set opbservation point */
    op.location_id = session->location_id;
    op.node_id = session->node_id;

    /* set observation window */
    ow.started_at = dns_session->stat_report_ts;
    ow.ended_at = now;
    dns_session->stat_report_ts = now;

    /* set health metrics */
    prev_stats = &dns_session->health_stats;

    /* Compute total lookups */
    count = (uint32_t)(stats->cloud_lookups + stats->cache_lookups);
    count -= (uint32_t)(prev_stats->cloud_lookups + prev_stats->cache_lookups);
    hs.total_lookups = count;
    prev_stats->cache_lookups = stats->cache_lookups;

    /* Compute cache hits */
    count = (uint32_t)(stats->cache_hits - prev_stats->cache_hits);
    hs.cache_hits = count;
    prev_stats->cache_hits = stats->cache_hits;

    /* Compute remote_lookups */
    count = (uint32_t)(stats->cloud_lookups - prev_stats->cloud_lookups);
    hs.remote_lookups = count;
    prev_stats->cloud_lookups = stats->cloud_lookups;

    /* Compute connectivity_failures */
    hs.connectivity_failures = dns_session->cat_offline.connection_failures;
    dns_session->cat_offline.connection_failures = 0;

    /* Compute service_failures */
    count = (uint32_t)(stats->categorization_failures -
                       prev_stats->categorization_failures);
    prev_stats->categorization_failures = stats->categorization_failures;

    /* Compute uncategorized requests */
    count = (uint32_t)(stats->uncategorized - prev_stats->uncategorized);
    hs.uncategorized = count;
    prev_stats->uncategorized = stats->uncategorized;

    /* Compute min latency */
    count = (uint32_t)(stats->min_lookup_latency);
    hs.min_latency = count;

    /* Compute max latency */
    count = (uint32_t)(stats->max_lookup_latency);
    hs.max_latency = count;

    /* Compute average latency */
    count = (uint32_t)(stats->avg_lookup_latency);
    hs.avg_latency = count;

    /* Compute cached entries */
    count = (uint32_t)(stats->cache_entries);
    hs.cached_entries = count;

    /* Compute cache size */
    count = (uint32_t)(stats->cache_size);
    hs.cache_size = count;

    /* Prepare report */
    report.provider = dns_session->provider;
    report.op = &op;
    report.ow = &ow;
    report.health_stats = &hs;

    /* Serialize report */
    serialized = wc_serialize_wc_stats_report(&report);

    /* Emit report */
    session->ops.send_pb_report(session, dns_session->health_stats_report_topic,
                                serialized->buf, serialized->len);

    /* Free the serialized protobuf */
    wc_free_packed_buffer(serialized);

}


void
dns_periodic(struct fsm_session *session)
{
    struct dns_session *dns_session;
    struct dns_device *ds = NULL;
    struct fsm_url_stats stats;
    time_t now = time(NULL);
    double cmp_report;
    double cmp_log;
    double cmp;
    bool get_stats;
    bool report;

    dns_session = dns_lookup_session(session);
    if (dns_session == NULL) return;

    /* Retire unresolved old requests */
    ds = ds_tree_head(&dns_session->device_sessions);
    while (ds != NULL)
    {
        struct fqdn_pending_req *req = ds_tree_head(&ds->fqdn_pending_reqs);

        while (req != NULL)
        {
            struct fqdn_pending_req *next = ds_tree_next(&ds->fqdn_pending_reqs,
                                                         req);
            struct fqdn_pending_req *remove = req;
            cmp = difftime(now, req->timestamp);

            LOGT("%s: " PRI_os_macaddr_lower_t
                 ": dns req id %d  on for %f seconds",
                 __func__, FMT_os_macaddr_t(req->dev_id), req->req_id, cmp);

            if (cmp < REQ_CACHE_TTL)
            {
                req = next;
                continue;
            }

            LOGT("%s: " PRI_os_macaddr_lower_t ": removing dns req id %d",
                 __func__, FMT_os_macaddr_t(req->dev_id), req->req_id);
            remove = req;
            req = next;
            ds_tree_remove(&ds->fqdn_pending_reqs, remove);
            dns_send_report(remove);

            dns_free_req(remove);
        }
        ds = ds_tree_next(&dns_session->device_sessions, ds);
    }

    /* Check if web categorization stats are available */
    if (session->service == NULL) return;
    if (dns_session->provider_ops == NULL) return;
    if (dns_session->provider_ops->get_stats == NULL) return;

    /* Check if the time has come to log the health stats locally */
    cmp_log = now - dns_session->stat_log_ts;
    get_stats = (cmp_log >= DNS_LOG_PERIODIC);

    /* Check if the time has come to report the stats through mqtt */
    cmp_report = now - dns_session->stat_report_ts;
    report = (cmp_report >= dns_session->health_stats_report_interval);
    get_stats |= report;

    /* No need to gather stats, bail */
    if (!get_stats) return;

    /* Get the stats */
    memset(&stats, 0, sizeof(stats));
    dns_session->provider_ops->get_stats(session, &stats);

    /* Log locally if the time has come */
    if (cmp_log >= DNS_LOG_PERIODIC) dns_log_stats(dns_session, &stats, now);

    /* Report to mqtt if the time has come */
    if (report) dns_report_health_stats(dns_session, &stats, now);
}


void
fqdn_policy_check(struct dns_device *ds,
                  struct fqdn_pending_req *req)
{
    struct fsm_url_request *req_info = req->req_info;
    struct fsm_policy_req preq;
    struct fsm_session *session;
    struct dns_session *dns_session;
    struct fsm_url_reply *reply;
    struct web_cat_offline *offline;

    if (req->numq == 0)
    {
        req->action = FSM_FORWARD;
        return;
    }

    session = req->fsm_context;
    dns_session = session->handler_ctxt;
    LOGD("Looking up %s id %d", req->req_info[0].url,
         req->req_id);
    memset(&preq, 0, sizeof(preq));
    preq.device_id = &req->dev_id;
    preq.url = req_info->url;
    preq.reply.rd_ttl = -1;
    preq.fqdn_req = req;
    req->fsm_checked = false;
    fsm_apply_policies(session, &preq);
    req->action = preq.reply.action;
    req->policy = preq.reply.policy;
    req->policy_idx = preq.reply.policy_idx;
    req->rule_name = preq.reply.rule_name;
    req->redirect = preq.reply.redirect;
    req->rd_ttl = preq.reply.rd_ttl;
    req->to_report = true;
    req->fsm_checked = true;

    /* Process reporting */
    if (preq.reply.log == FSM_REPORT_NONE)
    {
        req->to_report = false;
    }

    if ((preq.reply.log == FSM_REPORT_BLOCKED) &&
        (preq.reply.action != FSM_BLOCK))
    {
        req->to_report = false;
    }

    /* Overwrite logging and policy if categorization failed */
    if (req->categorized == FSM_FQDN_CAT_FAILED)
    {
        req->action = FSM_ALLOW;
        req->to_report = true;
    }

    /* Process web categorization provider connection failures */
    reply = req_info->reply;
    if (reply != NULL && reply->connection_error)
    {
        offline = &dns_session->cat_offline;
        offline->provider_offline = true;
        offline->offline_ts = time(NULL);
        offline->connection_failures++;
    }
}

void
dns_policy_check(struct dns_device *ds,
                 struct fqdn_pending_req *req)
{
    fqdn_policy_check(ds, req);

    /* Process the DNS reply if it was pending policy checking */
    if (req->response != NULL)
    {
        struct dns_session *dns_session = req->fsm_context->handler_ctxt;
        if (req->action != FSM_BLOCK)
        {
            dns_forward(dns_session, NULL, req->response, req->response_len);
        }
        free(req->response);
    }

    LOGT("%s: redirect = %s", __func__, req->redirect ? "true" : "false");
    ds_tree_insert(&ds->fqdn_pending_reqs, req, &req->req_id);
    return;
}
