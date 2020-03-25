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

#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <ev.h>
#include <libmnl/libmnl.h>
#include <linux/netfilter/nfnetlink_conntrack.h>

#include "os_types.h"
#include "log.h"
#include "nf_utils.h"

#define IPV4_ADDR_LEN     (4)
#define IPV6_ADDR_LEN     (16)
#define PROTO_NUM_ICMPV4  (1)
#define PROTO_NUM_ICMPV6  (58)
#define ICMP_ECHO_REQUEST (8)

// extern int cb_dump_data(const struct nlmsghdr *nlh, void *data);

static struct nf_ct
{
    struct ev_loop *loop;
    struct ev_io wmnl;
    struct mnl_socket *mnl;
    int fd;
} nf_ct;



static int
cb_err(const struct nlmsghdr *nlh, void *data)
{
    struct nlmsgerr *err = (void *)(nlh + 1);
    if (err->error != 0)
        LOGD("message with seq %u has failed: %s\n",
            nlh->nlmsg_seq, strerror(-err->error));
    return MNL_CB_OK;
}

static int
cb_overrun(const struct nlmsghdr *nlh, void *data)
{
    LOGD("message with seq %u has run out of memory\n",
          nlh->nlmsg_seq);
    return MNL_CB_OK;
}

static mnl_cb_t cb_ctl_array[NLMSG_MIN_TYPE] = {
    [NLMSG_ERROR] = cb_err,
    [NLMSG_OVERRUN] = cb_overrun,
};

static void read_mnl_socket_cbk(struct ev_loop *loop, struct ev_io *watcher,
                         int revents)
{
    char rcv_buf[MNL_SOCKET_BUFFER_SIZE];
    int ret = 0, portid = 0;
    if (EV_ERROR & revents)
    {
        LOGE("Invalid mnl socket event");
        return;
    }
    LOGD("MNL socket read callback");
    ret = mnl_socket_recvfrom(nf_ct.mnl, rcv_buf, sizeof(rcv_buf));
    if (ret == -1)
    {
        LOGE("%s: mnl_socket_recvfrom\n", __func__);
        return;
    }
    portid = mnl_socket_get_portid(nf_ct.mnl);
    LOGD("MNL ACK message received");
#if 0
    ret = mnl_cb_run2(rcv_buf, ret, 0, portid,
                      cb_dump_data, NULL, cb_ctl_array,
                      MNL_ARRAY_SIZE(cb_ctl_array));
#else
    ret = mnl_cb_run2(rcv_buf, ret, 0, portid,
                      NULL, NULL, cb_ctl_array,
                      MNL_ARRAY_SIZE(cb_ctl_array));

#endif
    if (ret == -1)
        LOGE("%s: mnl_cb_run\n", __func__);
}

static int build_ipv4_addr(
        struct nlmsghdr *nlh,
        uint32_t saddr,
        uint32_t daddr
)
{
    struct nlattr *nest;
    char str[INET_ADDRSTRLEN];

    nest = mnl_attr_nest_start(nlh, CTA_TUPLE_IP);
    mnl_attr_put_u32(nlh, CTA_IP_V4_SRC, saddr);
    inet_ntop(AF_INET, &saddr, str, INET_ADDRSTRLEN);
    LOGD("Added src_ip4: %s", str);

    mnl_attr_put_u32(nlh, CTA_IP_V4_DST, daddr);
    str[0] = '\0';
    inet_ntop(AF_INET, &daddr, str, INET_ADDRSTRLEN);
    LOGD("Added dst_ip4: %s", str);
    mnl_attr_nest_end(nlh, nest);
    return 0;
}

static int build_ipv6_addr(
        struct nlmsghdr *nlh,
        const void *saddr,
        const void *daddr
)
{
    struct nlattr *nest;
    nest = mnl_attr_nest_start(nlh, CTA_TUPLE_IP);
    if (saddr)
        mnl_attr_put(nlh, CTA_IP_V6_SRC, 16, saddr);
    if (daddr)
        mnl_attr_put(nlh, CTA_IP_V6_DST, 16, daddr);
    mnl_attr_nest_end(nlh, nest);
    return 0;
}

static int build_ip_tuple_v4(
        struct nlmsghdr *nlh,
        uint32_t saddr,
        uint32_t daddr,
        uint16_t sport,
        uint16_t dport,
        uint8_t proto
)
{
    struct nlattr *nest;
    build_ipv4_addr(nlh, saddr, daddr);

    nest = mnl_attr_nest_start(nlh, CTA_TUPLE_PROTO);
    mnl_attr_put_u8(nlh, CTA_PROTO_NUM, proto);
    LOGD("Added proto: %d", proto);
    switch (proto)
    {
        case IPPROTO_UDP:
        case IPPROTO_TCP:
            mnl_attr_put_u16(nlh, CTA_PROTO_SRC_PORT, sport);
            LOGD("Added src_port: %d", ntohs(sport));
            mnl_attr_put_u16(nlh, CTA_PROTO_DST_PORT, dport);
            LOGD("Added dst_port: %d", ntohs(dport));
            break;
        default:
            LOGD("Unknown protocol");
            mnl_attr_nest_cancel(nlh, nest);
            return -1;
    }
    mnl_attr_nest_end(nlh, nest);
    return 0;
}

static int build_ip_tuple_v6(
        struct nlmsghdr *nlh,
        const void *saddr,
        const void *daddr,
        uint16_t sport,
        uint16_t dport,
        uint8_t proto
)
{
    struct nlattr *nest;
    build_ipv6_addr(nlh, saddr, daddr);
    nest = mnl_attr_nest_start(nlh, CTA_TUPLE_PROTO);
    mnl_attr_put_u8(nlh, CTA_PROTO_NUM, proto);
    switch (proto)
    {
        case IPPROTO_UDP:
        case IPPROTO_TCP:
            if (sport > 0)
                mnl_attr_put_u16(nlh, CTA_PROTO_SRC_PORT, sport);
            if (dport > 0)
                mnl_attr_put_u16(nlh, CTA_PROTO_DST_PORT, dport);
            break;
        default:
            mnl_attr_nest_cancel(nlh, nest);
        return -1;
    }
    mnl_attr_nest_end(nlh, nest);
    return 0;
}

static int build_icmp_params_v4(
        struct nlmsghdr *nlh,
        uint32_t saddr,
        uint32_t daddr,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        uint8_t proto
)
{
    struct nlattr *nest;

    build_ipv4_addr(nlh, saddr, daddr);
    nest = mnl_attr_nest_start(nlh, CTA_TUPLE_PROTO);
    mnl_attr_put_u8(nlh, CTA_PROTO_NUM, proto);
    LOGD("Added proto: %d", proto);
    switch (proto)
    {
        case IPPROTO_ICMP:
            mnl_attr_put_u8(nlh, CTA_PROTO_ICMP_CODE, code);
            LOGD("Added icmp code: %d", code);
            mnl_attr_put_u8(nlh, CTA_PROTO_ICMP_TYPE, type);
            LOGD("Added icmp type: %d", type);
            mnl_attr_put_u16(nlh, CTA_PROTO_ICMP_ID, id);
            LOGD("Added icmp id: %d", ntohs(id));
            break;
        default:
            LOGD("Unknown protocol");
            mnl_attr_nest_cancel(nlh, nest);
            return -1;
    }
    mnl_attr_nest_end(nlh, nest);
    return 0;
}

static int build_icmp_params_v6(
        struct nlmsghdr *nlh,
        const void *saddr,
        const void *daddr,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        uint8_t proto
)
{
    struct nlattr *nest;

    build_ipv6_addr(nlh, saddr, daddr);
    nest = mnl_attr_nest_start(nlh, CTA_TUPLE_PROTO);
    mnl_attr_put_u8(nlh, CTA_PROTO_NUM, proto);
    LOGD("Added proto: %d", proto);
    switch (proto)
    {
        case IPPROTO_ICMPV6:
            mnl_attr_put_u8(nlh, CTA_PROTO_ICMPV6_CODE, code);
            LOGD("Added icmp code: %d", ntohs(code));
            mnl_attr_put_u8(nlh, CTA_PROTO_ICMPV6_TYPE, type);
            LOGD("Added icmp code: %d", ntohs(type));
            mnl_attr_put_u16(nlh, CTA_PROTO_ICMPV6_ID, id);
            LOGD("Added icmp id: %d", ntohs(id));
            break;
        default:
            LOGD("Unknown protocol");
            mnl_attr_nest_cancel(nlh, nest);
            return -1;
    }
    mnl_attr_nest_end(nlh, nest);
    return 0;
}


static struct nlmsghdr * nf_build_nl_msg_hdr(
        char *buf,
        uint32_t type,
        uint16_t flags
)
{

    struct nlmsghdr *nlh = NULL;
    struct nfgenmsg *nfh = NULL;
    static unsigned int seq = 0;

    nlh = mnl_nlmsg_put_header(buf);

    nlh->nlmsg_type = type;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_seq = ++seq;
    nlh->nlmsg_pid = mnl_socket_get_portid(nf_ct.mnl);

    nfh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct nfgenmsg));
    nfh->nfgen_family = AF_INET;
    nfh->version = NFNETLINK_V0;
    nfh->res_id = 0;

    return nlh;
}

static struct nlmsghdr * nf_build_icmp_nl_msg(
        char *buf,
        nf_addr_t *addr,
        nf_icmp_t *icmp,
        int proto,
        int family,
        uint32_t mark,
        uint16_t zone
)
{
    struct nlmsghdr *nlh = NULL;
    struct nlattr *nest = NULL;
    uint32_t saddr4 = 0;
    uint32_t daddr4 = 0;
    void *src_ip = NULL;
    void *dst_ip = NULL;
    uint16_t id = icmp->id;
    uint8_t  type = icmp->type;
    uint8_t  code = icmp->code;

    LOGD("Building nlmsg");

    nlh = nf_build_nl_msg_hdr(buf,
                              (NFNL_SUBSYS_CTNETLINK << 8) | IPCTNL_MSG_CT_NEW,
                              NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK);
    if (family == AF_INET)
    {
        src_ip = &addr->src_ip.ipv4.s_addr;
        dst_ip = &addr->dst_ip.ipv4.s_addr;
        if (src_ip)
            memcpy(&saddr4, src_ip, IPV4_ADDR_LEN);
        if (dst_ip)
            memcpy(&daddr4, dst_ip, IPV4_ADDR_LEN);

        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        LOGD("Building nlmsg origin icmpv4");
        build_icmp_params_v4(nlh, saddr4, daddr4, id, type, code, proto);
        mnl_attr_nest_end(nlh, nest);
        if (type == ICMP_ECHO_REQUEST)
        {
            LOGD("Building nlmsg reply icmpv4");
            nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
            build_icmp_params_v4(nlh, daddr4, saddr4, id, 0, 0, proto);
            mnl_attr_nest_end(nlh, nest);
        }
    }
    else if (family == AF_INET6)
    {
        src_ip = &addr->src_ip.ipv6.s6_addr;
        dst_ip = &addr->dst_ip.ipv6.s6_addr;
        LOGD("Building nlmsg origin icmpv6");
        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        build_icmp_params_v6(nlh, src_ip, dst_ip, id, type, code, proto);
        mnl_attr_nest_end(nlh, nest);
        if (type == ICMP_ECHO_REQUEST)
        {
            LOGD("Building nlmsg reply icmpv6");
            nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
            build_icmp_params_v6(nlh, dst_ip, src_ip, id, 0, 0, proto);
            mnl_attr_nest_end(nlh, nest);
        }
    }
    LOGD("Added mark: %d", mark);
    mnl_attr_put_u32(nlh, CTA_MARK, htonl(mark));
    LOGD("Added zone: %d", zone);
    mnl_attr_put_u16(nlh, CTA_ZONE, htons(zone));
    return nlh;
}


static struct nlmsghdr * nf_build_ip_nl_msg(
        char *buf,
        nf_addr_t *addr,
        nf_port_t *port,
        int proto,
        int family,
        uint32_t mark,
        uint16_t zone
)
{
    struct nlmsghdr *nlh = NULL;
    struct nlattr *nest = NULL;
    uint32_t saddr4 = 0;
    uint32_t daddr4 = 0;
    void *src_ip = NULL;
    void *dst_ip = NULL;
    uint16_t src_port = 0;
    uint16_t dst_port = 0;

    LOGD("Building nlmsg");

    nlh = nf_build_nl_msg_hdr(buf,
                              (NFNL_SUBSYS_CTNETLINK << 8) | IPCTNL_MSG_CT_NEW,
                              NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK);
    src_port = port->src_port;
    dst_port = port->dst_port;

    if (family == AF_INET)
    {
        src_ip = &addr->src_ip.ipv4.s_addr;
        dst_ip = &addr->dst_ip.ipv4.s_addr;
        if (src_ip)
            memcpy(&saddr4, src_ip, IPV4_ADDR_LEN);
        if (dst_ip)
            memcpy(&daddr4, dst_ip, IPV4_ADDR_LEN);

        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        LOGD("Building nlmsg origin ipv4");
        build_ip_tuple_v4(nlh, saddr4, daddr4, src_port, dst_port, proto);
        mnl_attr_nest_end(nlh, nest);
        LOGD("Building nlmsg reply ipv4");
        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
        build_ip_tuple_v4(nlh, daddr4, saddr4, dst_port, src_port, proto);
        mnl_attr_nest_end(nlh, nest);
    }
    else if (family == AF_INET6)
    {
        src_ip = &addr->src_ip.ipv6.s6_addr;
        dst_ip = &addr->dst_ip.ipv6.s6_addr;
        LOGD("Building nlmsg origin ipv6");
        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        build_ip_tuple_v6(nlh, src_ip, dst_ip, src_port, dst_port, proto);
        mnl_attr_nest_end(nlh, nest);
        LOGD("Building nlmsg reply ipv6");
        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
        build_ip_tuple_v6(nlh, dst_ip, src_ip, dst_port, src_port, proto);
        mnl_attr_nest_end(nlh, nest);
    }
    LOGD("Added mark: %d", mark);
    mnl_attr_put_u32(nlh, CTA_MARK, htonl(mark));
    LOGD("Added zone: %d", zone);
    mnl_attr_put_u16(nlh, CTA_ZONE, htons(zone));
    return nlh;
}

static void nf_ct_timeout_cbk(EV_P_ ev_timer *timer, int revents)
{
    nf_flow_t *ctx = container_of(timer, nf_flow_t, timeout);
    // clear the conn-track mark
    LOGD("conntrack timer expired");
    // clear the mark
    ctx->mark = 0;
    nf_ct_set_mark(ctx);
    ev_timer_stop(EV_A_ &ctx->timeout);
    free(ctx);
}


int nf_ct_set_mark(nf_flow_t *flow)
{
    uint8_t proto = 0;
    uint16_t family = 0;
    uint32_t mark = 0;
    uint16_t zone = 0;
    char buf[512];
    struct nlmsghdr *nlh = NULL;
    int res = 0;

    if (flow == NULL)
    {
        LOGE("Empty flow");
        return -1;
    }
    proto  = flow->proto;
    family = flow->family;
    mark   = flow->mark;
    zone   = flow->zone;
    if (family != AF_INET && family != AF_INET6)
    {
        LOGE("Unknown protocol family");
        return -1;
    }
    memset(buf, 0, sizeof(buf));
    if (proto == PROTO_NUM_ICMPV4 || proto == PROTO_NUM_ICMPV6)
    {

        nlh = nf_build_icmp_nl_msg(
                      buf,
                      &flow->addr,
                      &flow->fields.icmp,
                      proto,
                      family,
                      mark,
                      zone);
    }
    else
    {
        nlh = nf_build_ip_nl_msg(
                      buf,
                      &flow->addr,
                      &flow->fields.port,
                      proto,
                      family,
                      mark,
                      zone);
    }
    if (nlh == NULL)
        return -1;
    res = mnl_socket_sendto(nf_ct.mnl, nlh, nlh->nlmsg_len);
    LOGD("%s: nlh->nlmsg_len = %d res = %d\n", __func__, nlh->nlmsg_len, res);
    return res;
}

int nf_ct_set_mark_timeout(nf_flow_t *flow, uint32_t timeout)
{
    nf_flow_t *timer_ctx = NULL;

    timer_ctx = calloc(1, sizeof(nf_flow_t));
    if (timer_ctx == NULL)
    {
        LOGE("Memory allocation failure");
        return -1;
    }
    memcpy(timer_ctx, flow, sizeof(nf_flow_t));

    if (nf_ct_set_mark(flow) < 0)
    {
        LOGE("setting connection mark failed");
        return -1;
    }
    ev_timer_init(&timer_ctx->timeout, nf_ct_timeout_cbk, timeout, 0);
    ev_timer_start(nf_ct.loop, &timer_ctx->timeout);
    return 0;
}

static struct nlmsghdr * nf_build_ip_nl_msg_alt(
        char *buf,
        void *src_ip,
        void *dst_ip,
        uint16_t src_port,
        uint16_t dst_port,
        int proto,
        int family,
        uint32_t mark,
        uint16_t zone,
        bool build_reply
)
{
    struct nlmsghdr *nlh = NULL;
    struct nlattr *nest = NULL;
    uint32_t *saddr4 = NULL;
    uint32_t *daddr4 = NULL;

    LOGD("Building nlmsg");

    nlh = nf_build_nl_msg_hdr(buf,
                              (NFNL_SUBSYS_CTNETLINK << 8) | IPCTNL_MSG_CT_NEW,
                              NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK);

    if (family == AF_INET)
    {
        if (src_ip) saddr4 = src_ip;
        if (dst_ip) daddr4 = dst_ip;

        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        LOGD("Building nlmsg origin ipv4");
        build_ip_tuple_v4(nlh, *saddr4, *daddr4, src_port, dst_port, proto);
        mnl_attr_nest_end(nlh, nest);
        if (build_reply)
        {
            LOGD("Building nlmsg reply ipv4");
            nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
            build_ip_tuple_v4(nlh, *daddr4, *saddr4,
                              dst_port, src_port, proto);
            mnl_attr_nest_end(nlh, nest);
        }
    }
    else if (family == AF_INET6)
    {
        LOGD("Building nlmsg origin ipv6");
        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        build_ip_tuple_v6(nlh, src_ip, dst_ip, src_port, dst_port, proto);
        mnl_attr_nest_end(nlh, nest);
        if (build_reply)
        {
            LOGD("Building nlmsg reply ipv6");
            nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
            build_ip_tuple_v6(nlh, dst_ip, src_ip, dst_port, src_port, proto);
            mnl_attr_nest_end(nlh, nest);
        }
    }
    LOGD("Added mark: %d", mark);
    mnl_attr_put_u32(nlh, CTA_MARK, htonl(mark));
    LOGD("Added zone: %d", zone);
    mnl_attr_put_u16(nlh, CTA_ZONE, htons(zone));
    return nlh;
}

static struct nlmsghdr * nf_build_icmp_nl_msg_alt(
        char *buf,
        void *src_ip,
        void *dst_ip,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        int proto,
        int family,
        uint32_t mark,
        uint16_t zone,
        bool build_reply
)
{
    struct nlmsghdr *nlh = NULL;
    struct nlattr *nest = NULL;
    uint32_t *saddr4 = NULL;
    uint32_t *daddr4 = NULL;

    LOGD("Building nlmsg");

    nlh = nf_build_nl_msg_hdr(buf,
                              (NFNL_SUBSYS_CTNETLINK << 8) | IPCTNL_MSG_CT_NEW,
                              NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK);
    if (family == AF_INET)
    {
        if (src_ip) saddr4 = src_ip;
        if (dst_ip) daddr4 = dst_ip;

        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        LOGD("Building nlmsg origin icmpv4");
        build_icmp_params_v4(nlh, *saddr4, *daddr4, id, type, code, proto);
        mnl_attr_nest_end(nlh, nest);
        if ((type == ICMP_ECHO_REQUEST) && build_reply)
        {
            LOGD("Building nlmsg reply icmpv4");
            nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
            build_icmp_params_v4(nlh, *daddr4, *saddr4, id, 0, 0, proto);
            mnl_attr_nest_end(nlh, nest);
        }
    }
    else if (family == AF_INET6)
    {
        LOGD("Building nlmsg origin icmpv6");
        nest = mnl_attr_nest_start(nlh, CTA_TUPLE_ORIG);
        build_icmp_params_v6(nlh, src_ip, dst_ip, id, type, code, proto);
        mnl_attr_nest_end(nlh, nest);
        if ((type == ICMP_ECHO_REQUEST) && build_reply)
        {
            LOGD("Building nlmsg reply icmpv6");
            nest = mnl_attr_nest_start(nlh, CTA_TUPLE_REPLY);
            build_icmp_params_v6(nlh, dst_ip, src_ip, id, 0, 0, proto);
            mnl_attr_nest_end(nlh, nest);
        }
    }
    LOGD("Added mark: %d", mark);
    mnl_attr_put_u32(nlh, CTA_MARK, htonl(mark));
    LOGD("Added zone: %d", zone);
    mnl_attr_put_u16(nlh, CTA_ZONE, htons(zone));
    return nlh;
}



int nf_ct_set_flow_mark(struct net_header_parser *net_pkt, uint32_t mark, uint16_t zone)
{
    uint8_t proto = 0;
    uint16_t family = 0;
    char buf[512];
    struct nlmsghdr *nlh = NULL;
    int res = 0;
    struct iphdr *ipv4hdr = NULL;
    struct ip6_hdr *ipv6hdr = NULL;
    void *src_ip = NULL;
    void *dst_ip = NULL;
    uint32_t src_port = 0;
    uint32_t dst_port = 0;
    uint16_t id = 0;
    uint8_t type = 0;
    uint8_t code = 0;
    struct icmphdr *icmpv4hdr;
    struct icmp6_hdr *icmpv6hdr;

    if (net_pkt == NULL)
    {
        LOGE("Empty flow");
        return -1;
    }

    proto  = net_pkt->ip_protocol;

    if (net_pkt->ip_version == 4)
    {
        family = AF_INET;
        ipv4hdr = net_header_get_ipv4_hdr(net_pkt);
        src_ip = &ipv4hdr->saddr;
        dst_ip = &ipv4hdr->daddr;
    }
    else if (net_pkt->ip_version == 6)
    {
        family = AF_INET6;
        ipv6hdr = net_header_get_ipv6_hdr(net_pkt);
        src_ip = ipv6hdr->ip6_src.s6_addr;
        dst_ip = ipv6hdr->ip6_dst.s6_addr;
    }

    if (family != AF_INET && family != AF_INET6)
    {
        LOGE("Unknown protocol family");
        return -1;
    }
    memset(buf, 0, sizeof(buf));

    switch (net_pkt->ip_protocol)
    {
        case IPPROTO_TCP:
            src_port = net_pkt->ip_pld.tcphdr->source;
            dst_port = net_pkt->ip_pld.tcphdr->dest;
        break;

        case IPPROTO_UDP:
            src_port = net_pkt->ip_pld.udphdr->source;
            dst_port = net_pkt->ip_pld.udphdr->dest;
        break;

        case IPPROTO_ICMP:
            // icmpv4 hdr present in payload of ip
            icmpv4hdr = (struct icmphdr *)(net_pkt->data);
            id = icmpv4hdr->un.echo.id;
            type = icmpv4hdr->type;
            code = icmpv4hdr->code;
            LOGD("icmp: id:%d type:%d code:%d",
                 icmpv4hdr->un.echo.id, icmpv4hdr->type, icmpv4hdr->code);
        break;
        case IPPROTO_ICMPV6:
            // icmpv6 hdr present in payload of ipv6
            icmpv6hdr = (struct icmp6_hdr *)(net_pkt->data);
            id = ICMP6_ECHO_REQUEST;
            type = icmpv6hdr->icmp6_type;
            code = icmpv6hdr->icmp6_code;
        break;

        default:
        break;
    }

    if (proto == PROTO_NUM_ICMPV4 || proto == PROTO_NUM_ICMPV6)
    {

        nlh = nf_build_icmp_nl_msg_alt(
                      buf,
                      src_ip,
                      dst_ip,
                      id,
                      type,
                      code,
                      proto,
                      family,
                      mark,
                      zone,
                      true);
    }
    else
    {
        nlh = nf_build_ip_nl_msg_alt(
                      buf,
                      src_ip,
                      dst_ip,
                      src_port,
                      dst_port,
                      proto,
                      family,
                      mark,
                      zone,
                      true);
    }
    if (nlh == NULL)
        return -1;
    res = mnl_socket_sendto(nf_ct.mnl, nlh, nlh->nlmsg_len);
    LOGD("%s: nlh->nlmsg_len = %d res = %d\n", __func__, nlh->nlmsg_len, res);
    return res;
}


int nf_ct_init(struct ev_loop *loop)
{
    struct mnl_socket *nl = NULL;
    nl = mnl_socket_open(NETLINK_NETFILTER);
    if (nl == NULL)
    {
        LOGE("%s: mnl_socket_open", __func__);
        return -1;
    }
    if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0)
    {
        LOGE("%s: mnl_socket_bind", __func__);
        return -1;
    }
    nf_ct.mnl = nl;
    nf_ct.loop = loop;
    nf_ct.fd = mnl_socket_get_fd(nl);
    ev_io_init(&nf_ct.wmnl, read_mnl_socket_cbk, nf_ct.fd, EV_READ);
    ev_io_start(loop, &nf_ct.wmnl);
    LOGD("nf_ct initialized");
    return 0;
}

int nf_ct_exit(void)
{
    mnl_socket_close(nf_ct.mnl);
    return 0;
}
