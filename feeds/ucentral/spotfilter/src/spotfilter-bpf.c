// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
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
#include <uapi/linux/icmpv6.h>
#include <uapi/linux/filter.h>
#include <uapi/linux/pkt_cls.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "bpf_skb_utils.h"
#include "spotfilter-bpf.h"

static const volatile struct spotfilter_bpf_config config = {};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(key_size, sizeof(uint32_t));
	__type(value, struct spotfilter_bpf_class);
	__uint(max_entries, SPOTFILTER_NUM_CLASS);
} class SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(struct spotfilter_client_key));
	__type(value, struct spotfilter_client_data);
	__uint(max_entries, 1000);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} client SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(struct in_addr));
	__type(value, struct spotfilter_whitelist_entry);
	__uint(max_entries, 10000);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} whitelist_ipv4 SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(struct in6_addr));
	__type(value, struct spotfilter_whitelist_entry);
	__uint(max_entries, 10000);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} whitelist_ipv6 SEC(".maps");

static bool
is_dhcpv4_port(uint16_t port)
{
	return port == bpf_htons(67) || port == bpf_htons(68);
}

static __always_inline bool
check_ipv4_control(struct skb_parser_info *info)
{
	struct udphdr *udph;

	if (info->proto != IPPROTO_UDP)
		return false;

	udph = skb_info_ptr(info, sizeof(*udph));
	if (!udph)
		return false;

	return is_dhcpv4_port(udph->source) && is_dhcpv4_port(udph->dest);
}

static bool
is_dhcpv6_port(uint16_t port)
{
	return port == bpf_htons(546) || port == bpf_htons(547);
}

static bool
is_icmpv6_control(uint8_t type)
{
	switch (type) {
	case ICMPV6_PKT_TOOBIG:
	case NDISC_ROUTER_SOLICITATION:
	case NDISC_ROUTER_ADVERTISEMENT:
	case NDISC_NEIGHBOUR_SOLICITATION:
	case NDISC_NEIGHBOUR_ADVERTISEMENT:
	case NDISC_REDIRECT:
	case ICMPV6_MGM_QUERY:
	case ICMPV6_MGM_REPORT:
		return true;
	default:
		return false;
	}
}

static __always_inline bool
check_ipv6_control(struct skb_parser_info *info)
{
	if (info->proto == IPPROTO_UDP) {
		struct udphdr *udph;

		udph = skb_info_ptr(info, sizeof(*udph));
		if (!udph)
			return false;

		return is_dhcpv6_port(udph->source) && is_dhcpv6_port(udph->dest);
	}

	if (info->proto == IPPROTO_ICMPV6) {
		struct icmp6hdr *icmp6h;

		icmp6h = skb_info_ptr(info, sizeof(*icmp6h));
		if (!icmp6h)
			return false;

		return is_icmpv6_control(icmp6h->icmp6_type);
	}

	return false;
}

static __always_inline bool
check_dns(struct skb_parser_info *info, bool ingress)
{
	struct udphdr *udph;

	if (info->proto != IPPROTO_UDP)
		return false;

	udph = skb_info_ptr(info, sizeof(*udph));
	if (!udph)
		return false;

	if (ingress)
		return udph->dest == bpf_htons(53);

	return udph->source == bpf_htons(53);
}

SEC("tc/egress")
int spotfilter_out(struct __sk_buff *skb)
{
	struct spotfilter_client_data *cl;
	struct skb_parser_info info;
	struct ethhdr *eth;
	bool is_control = false;
	bool is_dns = false;

	skb_parse_init(&info, skb);
	eth = skb_parse_ethernet(&info);
	if (!eth)
		return TC_ACT_UNSPEC;

	cl = bpf_map_lookup_elem(&client, eth->h_dest);
	if (cl && (cl->flags & SPOTFILTER_CLIENT_F_ACCT_DL)) {
		cl->packets_dl++;
		cl->bytes_dl += skb->len;
	}

	skb_parse_vlan(&info);
	if (skb_parse_ipv4(&info, sizeof(struct udphdr))) {
		is_control = check_ipv4_control(&info);
		is_dns = check_dns(&info, false);
	} else if (skb_parse_ipv6(&info, sizeof(struct icmp6hdr))) {
		is_control = check_ipv6_control(&info);
		is_dns = check_dns(&info, false);
	} else {
		return TC_ACT_UNSPEC;
	}

	if (is_control || is_dns)
		bpf_clone_redirect(skb, config.snoop_ifindex, BPF_F_INGRESS);

	return TC_ACT_UNSPEC;
}

SEC("tc/ingress")
int spotfilter_in(struct __sk_buff *skb)
{
	struct spotfilter_client_data *cl, cldata = {};
	struct spotfilter_bpf_class *c, cdata;
	struct skb_parser_info info;
	struct ipv6hdr *ip6h;
	struct ethhdr *eth;
	struct iphdr *iph;
	bool addr_match = false;
	bool is_control = false;
	bool has_vlan = false;
	bool is_dns = false;
	struct spotfilter_whitelist_entry *wl_val = NULL;
	uint32_t cur_class;

	skb_parse_init(&info, skb);
	eth = skb_parse_ethernet(&info);
	if (!eth)
		return TC_ACT_UNSPEC;

	cl = bpf_map_lookup_elem(&client, eth->h_source);
	if (cl) {
		cldata = *cl;
		if (cl->flags & SPOTFILTER_CLIENT_F_ACCT_UL) {
			cl->packets_ul++;
			cl->bytes_ul += skb->len;
		}
	}

	has_vlan = !!skb_parse_vlan(&info);
	if ((iph = skb_parse_ipv4(&info, sizeof(struct udphdr))) != NULL) {
		addr_match = iph->saddr == cldata.ip4addr;
		is_control = check_ipv4_control(&info);
		is_dns = check_dns(&info, true);

		if (!is_control)
			wl_val = bpf_map_lookup_elem(&whitelist_ipv4, &iph->daddr);
	} else if ((ip6h = skb_parse_ipv6(&info, sizeof(struct icmp6hdr))) != NULL) {
		addr_match = ipv6_addr_equal(&ip6h->saddr, (struct in6_addr *)&cldata.ip6addr);
		if ((ip6h->saddr.s6_addr[0] & 0xe0) != 0x20)
			addr_match = true;
		is_control = check_ipv6_control(&info);
		is_dns = check_dns(&info, true);

		if (!is_control)
			wl_val = bpf_map_lookup_elem(&whitelist_ipv6, &ip6h->daddr);
	} else {
			return TC_ACT_UNSPEC;
	}

	if (wl_val) {
		cldata.cur_class = wl_val->val;
		cldata.dns_class = wl_val->val;
		wl_val->seen = 1;
	}

	if (is_control) {
		bpf_clone_redirect(skb, config.snoop_ifindex, BPF_F_INGRESS);
		return TC_ACT_UNSPEC;
	}

	if (!addr_match) {
		if (!is_control)
			return TC_ACT_SHOT;

		memset(&cldata, 0, sizeof(cldata));
	}

	cur_class = is_dns ? cldata.dns_class : cldata.cur_class;
	c = bpf_map_lookup_elem(&class, &cur_class);
	if (c)
		cdata = *c;
	else
		return TC_ACT_UNSPEC;

	if (!(cdata.actions & SPOTFILTER_ACTION_VALID))
		return TC_ACT_SHOT;

	if (cdata.actions & SPOTFILTER_ACTION_SET_DEST_MAC) {
		eth = skb_ptr(skb, 0, sizeof(*eth));
		if (!eth)
			return TC_ACT_UNSPEC;

		memcpy(eth->h_dest, cdata.dest_mac, ETH_ALEN);
	}

	if (cdata.actions & SPOTFILTER_ACTION_FWMARK)
		skb->mark = (skb->mark & ~cdata.fwmark_mask) | cdata.fwmark_val;

	if (cdata.actions & SPOTFILTER_ACTION_REDIRECT) {
		if (cdata.actions & SPOTFILTER_ACTION_REDIRECT_VLAN) {
			if (has_vlan && bpf_skb_vlan_pop(skb))
				return -1;

			if (cdata.redirect_vlan_proto &&
				bpf_skb_vlan_push(skb, cdata.redirect_vlan_proto, cdata.redirect_vlan))
				return -1;
		}

		return bpf_redirect(cdata.redirect_ifindex, 0);
	}

	return TC_ACT_UNSPEC;
}

char _license[] SEC("license") = "GPL";
