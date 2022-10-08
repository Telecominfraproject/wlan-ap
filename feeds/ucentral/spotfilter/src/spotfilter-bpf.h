// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __BPF_SPOTFILTER_H
#define __BPF_SPOTFILTER_H

struct spotfilter_client_key {
	uint8_t addr[6];
};

#define SPOTFILTER_CLIENT_F_ACCT_UL	(1 << 0)
#define SPOTFILTER_CLIENT_F_ACCT_DL	(1 << 1)

struct spotfilter_client_data {
	uint32_t ip4addr;
	uint32_t ip6addr[4];
	uint8_t cur_class;
	uint8_t dns_class;
	uint8_t flags;

	uint64_t packets_ul;
	uint64_t packets_dl;
	uint64_t bytes_ul;
	uint64_t bytes_dl;
};

struct spotfilter_bpf_config {
	uint32_t snoop_ifindex;
};

struct spotfilter_whitelist_entry {
	uint8_t val;
	uint8_t seen;
};

#define SPOTFILTER_NUM_CLASS 16

#define SPOTFILTER_ACTION_FWMARK	(1 << 0)
#define SPOTFILTER_ACTION_REDIRECT	(1 << 1)
#define SPOTFILTER_ACTION_REDIRECT_VLAN	(1 << 2)
#define SPOTFILTER_ACTION_SET_DEST_MAC	(1 << 3)

#define SPOTFILTER_ACTION_VALID		(1 << 15)


struct spotfilter_bpf_class {
	uint16_t actions;
	uint8_t dest_mac[6];

	uint32_t fwmark_val;
	uint32_t fwmark_mask;

	uint32_t redirect_ifindex;
	uint16_t redirect_vlan;
	uint16_t redirect_vlan_proto;
};

#endif
