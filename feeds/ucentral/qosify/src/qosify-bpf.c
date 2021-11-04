// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#define KBUILD_MODNAME "foo"
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/ipv6.h>
#include <uapi/linux/in.h>
#include <uapi/linux/tcp.h>
#include <uapi/linux/udp.h>
#include <uapi/linux/filter.h>
#include <uapi/linux/pkt_cls.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "qosify-bpf.h"

#define INET_ECN_MASK 3

#define FLOW_CHECK_INTERVAL	((u32)((1000000000ULL) >> 24))
#define FLOW_TIMEOUT		((u32)((30ULL * 1000000000ULL) >> 24))
#define FLOW_BULK_TIMEOUT	5

#define EWMA_SHIFT		12

const volatile static uint32_t module_flags = 0;

struct flow_bucket {
	__u32 last_update;
	__u32 pkt_len_avg;
	__u16 pkt_count;
	__u8 dscp;
	__u8 bulk_timeout;
};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(pinning, 1);
	__type(key, __u32);
	__type(value, struct qosify_config);
	__uint(max_entries, 1);
} config SEC(".maps");

typedef struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(pinning, 1);
	__type(key, __u32);
	__type(value, __u8);
	__uint(max_entries, 1 << 16);
} port_array_t;

struct {
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(pinning, 1);
	__type(key, __u32);
	__uint(value_size, sizeof(struct flow_bucket));
	__uint(max_entries, QOSIFY_FLOW_BUCKETS);
} flow_map SEC(".maps");

port_array_t tcp_ports SEC(".maps");
port_array_t udp_ports SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(pinning, 1);
	__uint(key_size, sizeof(struct in_addr));
	__type(value, struct qosify_ip_map_val);
	__uint(max_entries, 100000);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} ipv4_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(pinning, 1);
	__uint(key_size, sizeof(struct in6_addr));
	__type(value, struct qosify_ip_map_val);
	__uint(max_entries, 100000);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} ipv6_map SEC(".maps");

static struct qosify_config *get_config(void)
{
	__u32 key = 0;

	return bpf_map_lookup_elem(&config, &key);
}

static __always_inline int proto_is_vlan(__u16 h_proto)
{
	return !!(h_proto == bpf_htons(ETH_P_8021Q) ||
		  h_proto == bpf_htons(ETH_P_8021AD));
}

static __always_inline int proto_is_ip(__u16 h_proto)
{
	return !!(h_proto == bpf_htons(ETH_P_IP) ||
		  h_proto == bpf_htons(ETH_P_IPV6));
}

static __always_inline void *skb_ptr(struct __sk_buff *skb, __u32 offset)
{
	void *start = (void *)(unsigned long long)skb->data;

	return start + offset;
}

static __always_inline void *skb_end_ptr(struct __sk_buff *skb)
{
	return (void *)(unsigned long long)skb->data_end;
}

static __always_inline int skb_check(struct __sk_buff *skb, void *ptr)
{
	if (ptr > skb_end_ptr(skb))
		return -1;

	return 0;
}

static __always_inline __u32 cur_time(void)
{
	__u32 val = bpf_ktime_get_ns() >> 24;

	if (!val)
		val = 1;

	return val;
}

static __always_inline __u32 ewma(__u32 *avg, __u32 val)
{
	if (*avg)
		*avg = (*avg * 3) / 4 + (val << EWMA_SHIFT) / 4;
	else
		*avg = val << EWMA_SHIFT;

	return *avg >> EWMA_SHIFT;
}

static __always_inline void
ipv4_change_dsfield(struct iphdr *iph, __u8 mask, __u8 value, bool force)
{
	__u32 check = bpf_ntohs(iph->check);
	__u8 dsfield;

	if ((iph->tos & mask) && !force)
		return;

	dsfield = (iph->tos & mask) | value;
	if (iph->tos == dsfield)
		return;

	check += iph->tos;
	if ((check + 1) >> 16)
		check = (check + 1) & 0xffff;
	check -= dsfield;
	check += check >> 16;
	iph->check = bpf_htons(check);
	iph->tos = dsfield;
}

static __always_inline void
ipv6_change_dsfield(struct ipv6hdr *ipv6h, __u8 mask, __u8 value, bool force)
{
	__u16 *p = (__u16 *)ipv6h;
	__u16 val;

	if (((*p >> 4) & mask) && !force)
		return;

	val = (*p & bpf_htons((((__u16)mask << 4) | 0xf00f))) | bpf_htons((__u16)value << 4);
	if (val == *p)
		return;

	*p = val;
}

static __always_inline int
parse_ethernet(struct __sk_buff *skb, __u32 *offset)
{
	struct ethhdr *eth;
	__u16 h_proto;
	int i;

	eth = skb_ptr(skb, *offset);
	if (skb_check(skb, eth + 1))
		return -1;

	h_proto = eth->h_proto;
	*offset += sizeof(*eth);

#pragma unroll
	for (i = 0; i < 2; i++) {
		struct vlan_hdr *vlh = skb_ptr(skb, *offset);

		if (!proto_is_vlan(h_proto))
			break;

		if (skb_check(skb, vlh + 1))
			return -1;

		h_proto = vlh->h_vlan_encapsulated_proto;
		*offset += sizeof(*vlh);
	}

	return h_proto;
}

static void
parse_l4proto(struct qosify_config *config, struct __sk_buff *skb,
	      __u32 offset, __u8 proto, __u8 *dscp_out)
{
	struct udphdr *udp;
	__u32 src, dest, key;
	__u8 *value;

	udp = skb_ptr(skb, offset);
	if (skb_check(skb, &udp->len))
		return;

	if (config && (proto == IPPROTO_ICMP || proto == IPPROTO_ICMPV6)) {
		*dscp_out = config->dscp_icmp;
		return;
	}

	src = udp->source;
	dest = udp->dest;

	if (module_flags & QOSIFY_INGRESS)
		key = src;
	else
		key = dest;

	if (proto == IPPROTO_TCP) {
		value = bpf_map_lookup_elem(&tcp_ports, &key);
	} else {
		if (proto != IPPROTO_UDP)
			key = 0;

		value = bpf_map_lookup_elem(&udp_ports, &key);
	}

	if (!value)
		return;

	*dscp_out = *value;
}

static void
check_flow(struct qosify_config *config, struct __sk_buff *skb,
	   uint8_t *dscp)
{
	struct flow_bucket flow_data;
	struct flow_bucket *flow;
	__s32 delta;
	__u32 hash;
	__u32 time;

	if (!(*dscp & QOSIFY_DSCP_DEFAULT_FLAG))
		return;

	if (!config)
		return;

	if (!config->bulk_trigger_pps &&
	    !config->prio_max_avg_pkt_len)
		return;

	time = cur_time();
	hash = bpf_get_hash_recalc(skb);
	flow = bpf_map_lookup_elem(&flow_map, &hash);
	if (!flow) {
		memset(&flow_data, 0, sizeof(flow_data));
		bpf_map_update_elem(&flow_map, &hash, &flow_data, BPF_ANY);
		flow = bpf_map_lookup_elem(&flow_map, &hash);
		if (!flow)
			return;
	}

	if (!flow->last_update)
		goto reset;

	delta = time - flow->last_update;
	if ((u32)delta > FLOW_TIMEOUT)
		goto reset;

	if (delta >= FLOW_CHECK_INTERVAL) {
		if (flow->bulk_timeout) {
			flow->bulk_timeout--;
			if (!flow->bulk_timeout)
				flow->dscp = 0xff;
		}

		goto clear;
	}

	if (flow->pkt_count < 0xffff)
		flow->pkt_count++;

	if (config->bulk_trigger_pps &&
	    flow->pkt_count > config->bulk_trigger_pps) {
		flow->dscp = config->dscp_bulk;
		flow->bulk_timeout = config->bulk_trigger_timeout;
	}

out:
	if (config->prio_max_avg_pkt_len &&
	    flow->dscp != config->dscp_bulk) {
		if (ewma(&flow->pkt_len_avg, skb->len) <
		    config->prio_max_avg_pkt_len)
			flow->dscp = config->dscp_prio;
		else
			flow->dscp = 0xff;
	}

	if (flow->dscp != 0xff)
		*dscp = flow->dscp;

	return;

reset:
	flow->dscp = 0xff;
	flow->pkt_len_avg = 0;
clear:
	flow->pkt_count = 1;
	flow->last_update = time;

	goto out;
}

static __always_inline void
parse_ipv4(struct __sk_buff *skb, __u32 *offset)
{
	struct qosify_config *config;
	struct qosify_ip_map_val *ip_val;
	const __u32 zero_port = 0;
	struct iphdr *iph;
	__u8 dscp = 0xff;
	__u8 *value;
	__u8 ipproto;
	int hdr_len;
	void *key;
	bool force;

	config = get_config();

	iph = skb_ptr(skb, *offset);
	if (skb_check(skb, iph + 1))
		return;

	hdr_len = iph->ihl * 4;
	if (bpf_skb_pull_data(skb, *offset + hdr_len + sizeof(struct udphdr)))
		return;

	iph = skb_ptr(skb, *offset);
	*offset += hdr_len;

	if (skb_check(skb, (void *)(iph + 1)))
		return;

	ipproto = iph->protocol;
	parse_l4proto(config, skb, *offset, ipproto, &dscp);

	if (module_flags & QOSIFY_INGRESS)
		key = &iph->saddr;
	else
		key = &iph->daddr;

	ip_val = bpf_map_lookup_elem(&ipv4_map, key);
	if (ip_val) {
		if (!ip_val->seen)
			ip_val->seen = 1;
		dscp = ip_val->dscp;
	} else if (dscp == 0xff) {
		/* use udp port 0 entry as fallback for non-tcp/udp */
		value = bpf_map_lookup_elem(&udp_ports, &zero_port);
		if (value)
			dscp = *value;
	}

	check_flow(config, skb, &dscp);

	force = !(dscp & QOSIFY_DSCP_FALLBACK_FLAG);
	dscp &= GENMASK(5, 0);

	ipv4_change_dsfield(iph, INET_ECN_MASK, dscp << 2, force);
}

static __always_inline void
parse_ipv6(struct __sk_buff *skb, __u32 *offset)
{
	struct qosify_config *config;
	struct qosify_ip_map_val *ip_val;
	const __u32 zero_port = 0;
	struct ipv6hdr *iph;
	__u8 dscp = 0;
	__u8 *value;
	__u8 ipproto;
	void *key;
	bool force;

	config = get_config();

	if (bpf_skb_pull_data(skb, *offset + sizeof(*iph) + sizeof(struct udphdr)))
		return;

	iph = skb_ptr(skb, *offset);
	*offset += sizeof(*iph);

	if (skb_check(skb, (void *)(iph + 1)))
		return;

	ipproto = iph->nexthdr;
	if (module_flags & QOSIFY_INGRESS)
		key = &iph->saddr;
	else
		key = &iph->daddr;

	parse_l4proto(config, skb, *offset, ipproto, &dscp);

	ip_val = bpf_map_lookup_elem(&ipv6_map, key);
	if (ip_val) {
		if (!ip_val->seen)
			ip_val->seen = 1;
		dscp = ip_val->dscp;
	} else if (dscp == 0xff) {
		/* use udp port 0 entry as fallback for non-tcp/udp */
		value = bpf_map_lookup_elem(&udp_ports, &zero_port);
		if (value)
			dscp = *value;
	}

	check_flow(config, skb, &dscp);

	force = !(dscp & QOSIFY_DSCP_FALLBACK_FLAG);
	dscp &= GENMASK(5, 0);

	ipv6_change_dsfield(iph, INET_ECN_MASK, dscp << 2, force);
}

SEC("classifier")
int classify(struct __sk_buff *skb)
{
	__u32 offset = 0;
	int type;

	if (module_flags & QOSIFY_IP_ONLY)
		type = skb->protocol;
	else
		type = parse_ethernet(skb, &offset);

	if (type == bpf_htons(ETH_P_IP))
		parse_ipv4(skb, &offset);
	else if (type == bpf_htons(ETH_P_IPV6))
		parse_ipv6(skb, &offset);

	return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";
