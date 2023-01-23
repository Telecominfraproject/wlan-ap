// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __BPF_UDEVSTATS_H
#define __BPF_UDEVSTATS_H

struct udevstats_vlan_key {
	uint32_t vlan_ifindex;
	uint16_t vlan_id;
	uint8_t vlan_tx;
	uint8_t vlan_is_ad;
};

struct udevstats_vlan_stats {
	uint64_t packets;
	uint64_t bytes;
};

#endif
