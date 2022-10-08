// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __SPOTFILTER_INTERFACE_H
#define __SPOTFILTER_INTERFACE_H

#include <libubox/vlist.h>
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>

struct bpf_object;

struct interface {
	struct avl_node node;

	struct blob_attr *config;
	struct blob_attr *whitelist;

	struct avl_tree cname_cache;
	struct avl_tree addr_map;

	struct uloop_timeout addr_gc;
	uint32_t next_gc;

	uint32_t active_timeout;

	uint8_t default_class;
	uint8_t default_dns_class;

	bool client_autocreate;
	bool client_autoremove;
	int client_timeout;

	struct {
		struct bpf_object *obj;

		int prog_ingress;
		int prog_egress;
		int map_class;
		int map_client;
		int map_whitelist_v4;
		int map_whitelist_v6;
	} bpf;

	struct spotfilter_bpf_class cdata[SPOTFILTER_NUM_CLASS];

	struct vlist_tree devices;

	struct avl_tree clients;
	struct avl_tree client_ids;
};

struct device {
	struct vlist_node node;

	int ifindex;
};

extern struct avl_tree interfaces;

static inline const char *interface_name(struct interface *iface)
{
	return iface->node.key;
}

static inline const char *device_name(struct device *dev)
{
	return dev->node.avl.key;
}

void interface_add(const char *name, struct blob_attr *config,
		   struct blob_attr *devices);
void interface_free(struct interface *iface);
void interface_check_devices(void);
void interface_done(void);

#endif
