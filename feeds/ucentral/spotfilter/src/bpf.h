// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __SPOTFILTER_BPF_H
#define __SPOTFILTER_BPF_H

struct interface;

int spotfilter_bpf_load(struct interface *iface);
void spotfilter_bpf_free(struct interface *iface);
void spotfilter_bpf_set_device(struct interface *iface, int ifindex, bool enabled);
void spotfilter_bpf_update_class(struct interface *iface, uint32_t index);
int spotfilter_bpf_get_client(struct interface *iface,
			      const struct spotfilter_client_key *key,
			      struct spotfilter_client_data *data);
int spotfilter_bpf_set_client(struct interface *iface,
			      const struct spotfilter_client_key *key,
			      const struct spotfilter_client_data *data);
void spotfilter_bpf_set_whitelist(struct interface *iface, const void *addr,
				  bool ipv6, const uint8_t *state);
bool spotfilter_bpf_whitelist_seen(struct interface *iface, const void *addr, bool ipv6);

#endif
