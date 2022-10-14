// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __SPOTFILTER_CLIENT_H
#define __SPOTFILTER_CLIENT_H

#include <netinet/if_ether.h>
#include <libubox/kvlist.h>

struct client {
	struct avl_node node;
	struct avl_node id_node;

	struct kvlist kvdata;
	int idle;

	struct spotfilter_client_key key;
	struct spotfilter_client_data data;
	char *device;
};

int client_set(struct interface *iface, const void *addr, const char *id,
	       int state, int dns_state, int accounting, struct blob_attr *data,
	       const char *device, bool flush);
void client_free(struct interface *iface, struct client *cl);
void client_set_ipaddr(const void *mac, const void *addr, bool ipv6);
void client_init_interface(struct interface *iface);

#endif
