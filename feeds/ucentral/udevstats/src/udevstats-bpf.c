// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#define KBUILD_MODNAME "udevstats"
#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/filter.h>
#include <uapi/linux/pkt_cls.h>
#include <linux/if_vlan.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "bpf_skb_utils.h"
#include "udevstats-bpf.h"

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(struct udevstats_vlan_key));
	__type(value, struct udevstats_vlan_stats);
	__uint(max_entries, 1000);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} vlans SEC(".maps");

static inline int udevstats_handle_packet(struct __sk_buff *skb, int ifindex, bool tx)
{
	struct udevstats_vlan_stats *stats;
	struct udevstats_vlan_key key = {
		.vlan_tx = tx,
		.vlan_ifindex = ifindex,
	};
	struct skb_parser_info info;
	struct vlan_hdr *vlan;
	__be16 vlan_proto = 0;

	skb_parse_init(&info, skb);
	if (!skb_parse_ethernet(&info))
		return TC_ACT_UNSPEC;

	if (skb->vlan_present) {
		key.vlan_id = skb->vlan_tci;
		vlan_proto = skb->vlan_proto;
	} else if ((vlan = skb_parse_vlan(&info)) != NULL) {
		vlan_proto = info.proto;
		key.vlan_id = bpf_ntohs(vlan->h_vlan_TCI);
	}

	key.vlan_id &= VLAN_VID_MASK;
	key.vlan_is_ad = vlan_proto == bpf_htons(ETH_P_8021AD);

	stats = bpf_map_lookup_elem(&vlans, &key);
	if (!stats)
		return TC_ACT_UNSPEC;

	__sync_fetch_and_add(&stats->packets, 1);
	__sync_fetch_and_add(&stats->bytes, skb->len);

	return TC_ACT_UNSPEC;
}


SEC("tc/egress")
int udevstats_out(struct __sk_buff *skb)
{
	return udevstats_handle_packet(skb, skb->ifindex, true);
}

SEC("tc/ingress")
int udevstats_in(struct __sk_buff *skb)
{
	return udevstats_handle_packet(skb, skb->ingress_ifindex, false);
}

char _license[] SEC("license") = "GPL";
