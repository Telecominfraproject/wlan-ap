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

#include <string.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include "log.h"
#include "nf_utils.h"
#include "net_header_parse.h"
#include "fsm_dpi_utils.h"

#define DEFAULT_ZONE (0)
#define FSM_DPI_ZONE (1)

#define NET_HDR_BUFF_SIZE 128 + (2 * INET6_ADDRSTRLEN + 128) + 256

static char log_buf[NET_HDR_BUFF_SIZE] = { 0 };

static void copy_nf_ip_flow(
        nf_flow_t *flow,
        void *src_ip,
        void *dst_ip,
        uint16_t src_port,
        uint16_t dst_port,
        uint8_t proto,
        uint16_t family,
        uint16_t zone,
        enum fsm_dpi_state state
)
{
    if (family == AF_INET)
    {
        memcpy(&flow->addr.src_ip.ipv4.s_addr, src_ip, 4);
        memcpy(&flow->addr.dst_ip.ipv4.s_addr, dst_ip, 4);
    }
    else if (family == AF_INET6)
    {
        memcpy(flow->addr.src_ip.ipv6.s6_addr, src_ip, 16);
        memcpy(flow->addr.dst_ip.ipv6.s6_addr, dst_ip, 16);
    }
    flow->proto = proto;
    flow->family = family;
    flow->fields.port.src_port = src_port;
    flow->fields.port.dst_port = dst_port;
    flow->mark = state;
    flow->zone = zone;
}

static void copy_nf_icmp_flow(
        nf_flow_t *flow,
        void *src_ip,
        void *dst_ip,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        uint16_t family,
        uint16_t zone,
        enum fsm_dpi_state state
)
{
    if (family == AF_INET)
    {
        memcpy(&flow->addr.src_ip.ipv4.s_addr, src_ip, 4);
        memcpy(&flow->addr.dst_ip.ipv4.s_addr, dst_ip, 4);
        flow->proto = 1;
    }
    else if (family == AF_INET6)
    {
        memcpy(flow->addr.src_ip.ipv6.s6_addr, src_ip, 16);
        memcpy(flow->addr.dst_ip.ipv6.s6_addr, dst_ip, 16);
        flow->proto = 58;
    }
    flow->family = family;
    flow->fields.icmp.id = id;
    flow->fields.icmp.type = type;
    flow->fields.icmp.code = code;
    flow->mark = state;
    flow->zone = zone;
}

static void copy_nf_flow_from_net_hdr(
        nf_flow_t *flow,
        struct net_header_parser *net_hdr,
        uint16_t zone,
        enum fsm_dpi_state state
)
{
    struct icmphdr icmpv4hdr;
    struct icmp6_hdr icmpv6hdr;
    struct iphdr *ipv4hdr = NULL;
    struct ip6_hdr *ipv6hdr = NULL;

    LOGT("%s: %s", __func__,
         net_header_fill_trace_buf(log_buf, NET_HDR_BUFF_SIZE, net_hdr));

    if (net_hdr->ip_version == 4)
    {
        ipv4hdr = net_header_get_ipv4_hdr(net_hdr);
        memcpy(&flow->addr.src_ip.ipv4.s_addr, &ipv4hdr->saddr, 4);
        memcpy(&flow->addr.dst_ip.ipv4.s_addr, &ipv4hdr->daddr, 4);
        flow->family = AF_INET;
    }
    else if (net_hdr->ip_version == 6)
    {
        ipv6hdr = net_header_get_ipv6_hdr(net_hdr);
        memcpy(&flow->addr.src_ip.ipv6.s6_addr, ipv6hdr->ip6_src.s6_addr, 16);
        memcpy(&flow->addr.dst_ip.ipv6.s6_addr, ipv6hdr->ip6_dst.s6_addr, 16);
        flow->family = AF_INET6;
    }

    flow->proto = net_hdr->ip_protocol;

    switch (net_hdr->ip_protocol)
    {
        case IPPROTO_TCP:
            flow->fields.port.src_port = ntohs(net_hdr->ip_pld.tcphdr->source);
            flow->fields.port.dst_port = ntohs(net_hdr->ip_pld.tcphdr->dest);
            break;

        case IPPROTO_UDP:
            flow->fields.port.src_port = ntohs(net_hdr->ip_pld.udphdr->source);
            flow->fields.port.dst_port = ntohs(net_hdr->ip_pld.udphdr->dest);
            break;

        case IPPROTO_ICMP:
            // icmpv4 hdr present in payload of ip
            memcpy(&icmpv4hdr, net_hdr->eth_pld.payload,
                    sizeof(struct icmphdr));
            flow->fields.icmp.id = icmpv4hdr.un.echo.id;
            flow->fields.icmp.type = icmpv4hdr.type;
            flow->fields.icmp.code = icmpv4hdr.code;
            LOGD("icmp: id:%d type:%d code:%d",
                 icmpv4hdr.un.echo.id, icmpv4hdr.type, icmpv4hdr.code);
            break;

        case IPPROTO_ICMPV6:
            // icmpv6 hdr present in payload of ipv6
            memcpy(&icmpv6hdr, net_hdr->eth_pld.payload,
                    sizeof(struct icmp6_hdr));
            flow->fields.icmp.id = ICMP6_ECHO_REQUEST;
            flow->fields.icmp.type = icmpv6hdr.icmp6_type;
            flow->fields.icmp.code = icmpv6hdr.icmp6_code;
            break;

        default:
            LOGD("protocol not supported for connection marking");
            break;
    }
    flow->mark = state;
    flow->zone = zone;
}

// TODO ctx used to hold flow and its state when multiple plugins used
int fsm_set_ip_dpi_state(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t src_port,
        uint16_t dst_port,
        uint8_t proto,
        uint16_t family,
        enum fsm_dpi_state state
)
{
    nf_flow_t flow;
    int ret0;
    int ret1;

    memset(&flow, 0, sizeof(flow));
    copy_nf_ip_flow(
            &flow,
            src_ip,
            dst_ip,
            src_port,
            dst_port,
            proto,
            family,
            DEFAULT_ZONE,
            state);
    ret0 = nf_ct_set_mark(&flow);
    /* Set the conn mark for FSM_DPI_ZONE also */
    flow.zone = FSM_DPI_ZONE;
    ret1 = nf_ct_set_mark(&flow);

    /* -ve or 0 - failed in both zones or +ve atleast one zone passed */
    return (ret0 + ret1);
}

int fsm_set_ip_dpi_state_timeout(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t src_port,
        uint16_t dst_port,
        uint8_t proto,
        uint16_t family,
        enum fsm_dpi_state state,
        uint32_t timeout
)
{
    nf_flow_t flow;
    int ret0;
    int ret1;

    memset(&flow, 0, sizeof(flow));
    copy_nf_ip_flow(
            &flow,
            src_ip,
            dst_ip,
            src_port,
            dst_port,
            proto,
            family,
            DEFAULT_ZONE,
            state);
    ret0 = nf_ct_set_mark_timeout(&flow, timeout);
    /* Set the conn mark for FSM_DPI_ZONE also */
    flow.zone = FSM_DPI_ZONE;
    ret1 = nf_ct_set_mark(&flow);
    /* -ve or 0 - failed in both zones or +ve atleast one zone passed */
    return (ret0 + ret1);
}

int fsm_set_icmp_dpi_state(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        uint16_t family,
        enum fsm_dpi_state state
)
{
    nf_flow_t flow;
    int ret0;
    int ret1;

    memset(&flow, 0, sizeof(flow));
    copy_nf_icmp_flow(
            &flow,
            src_ip,
            dst_ip,
            id,
            type,
            code,
            family,
            DEFAULT_ZONE,
            state);
    ret0 = nf_ct_set_mark(&flow);
    /* Set the conn mark for FSM_DPI_ZONE also */
    flow.zone = FSM_DPI_ZONE;
    ret1 = nf_ct_set_mark(&flow);
    /* -ve or 0 - failed in both zones or +ve atleast one zone passed */
    return (ret0 + ret1);
}

int fsm_set_icmp_dpi_state_timeout(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        uint16_t family,
        enum fsm_dpi_state state,
        uint32_t timeout
)
{
    nf_flow_t flow;
    int ret0;
    int ret1;

    memset(&flow, 0, sizeof(flow));
    copy_nf_icmp_flow(
            &flow,
            src_ip,
            dst_ip,
            id,
            type,
            code,
            family,
            DEFAULT_ZONE,
            state);
    ret0 = nf_ct_set_mark_timeout(&flow, timeout);
    /* Set the conn mark for FSM_DPI_ZONE also */
    flow.zone = FSM_DPI_ZONE;
    ret1 = nf_ct_set_mark_timeout(&flow, timeout);
    /* -ve or 0 - failed in both zones or +ve atleast one zone passed */
    return (ret0 + ret1);
}

// APIs using net_header_parser
int fsm_set_dpi_state(
        void *ctx,
        struct net_header_parser *net_hdr,
        enum fsm_dpi_state state)
{
    int ret0;
    int ret1;

    ret0 = nf_ct_set_flow_mark(net_hdr, state, 0);
    /*
     * Set the mark for the default zone 0 also.
     * The reason behind it in router mode
     * two flows are present one in zone=1 and
     * another in zone=0 due to NAT functionality.
     * Mark has to be applied to flows in both the zones.
     * In Bridge mode zone=0 will not be present
     * so netlink call  will throw error which
     * now. Cloud will configure appropriate mode.
     * TODO Either check Router/Bridge mode and make this additional call
     * or dump_all_flows and apply mark for all mathching 5 tuple flows.
     */
    ret1 = nf_ct_set_flow_mark(net_hdr, state, FSM_DPI_ZONE);
    /* -ve or 0 - failed in both zones or +ve atleast one zone passed */
    return (ret0 + ret1);
}

int fsm_set_dpi_state_timeout(
        void *ctx,
        struct net_header_parser *net_hdr,
        enum fsm_dpi_state state,
        uint32_t timeout
)
{
    nf_flow_t flow;
    int ret0;
    int ret1;

    memset(&flow, 0, sizeof(flow));
    copy_nf_flow_from_net_hdr(&flow, net_hdr, DEFAULT_ZONE, state);
    ret0 = nf_ct_set_mark_timeout(&flow, timeout);
    /* Set the conn mark for FSM_DPI_ZONE also */
    flow.zone = FSM_DPI_ZONE;
    ret1 = nf_ct_set_mark_timeout(&flow, timeout);
    /* -ve or 0 - failed in both zones or +ve atleast one zone passed */
    return (ret0 + ret1);
}
