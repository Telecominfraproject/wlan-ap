// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <netinet/if_ether.h>
#include <libubox/uloop.h>
#include <libubox/avl-cmp.h>
#include "spotfilter.h"

#define CACHE_TIMEOUT 10

struct cache_entry {
	struct avl_node node;
	uint8_t macaddr[ETH_ALEN];
	uint32_t ip4addr;
	uint32_t ip6addr[4];
	uint32_t time;
};

static int avl_mac_cmp(const void *k1, const void *k2, void *priv)
{
	return memcmp(k1, k2, ETH_ALEN);
}

static AVL_TREE(cache, avl_mac_cmp, false, NULL);

static uint32_t client_gettime(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec;
}

static bool client_is_active(const uint8_t *mac)
{
	struct interface *iface;

	avl_for_each_element(&interfaces, iface, node) {
		if (avl_find(&iface->clients, mac))
			return true;
	}

	return false;
}

static void client_gc(struct uloop_timeout *t)
{
	struct cache_entry *c, *tmp;
	uint32_t now = client_gettime();

	avl_for_each_element_safe(&cache, c, node, tmp) {
		uint32_t diff;

		if (client_is_active(c->macaddr)) {
			c->time = now;
			continue;
		}

		diff = now - c->time;
		if (diff < CACHE_TIMEOUT)
			continue;

		avl_delete(&cache, &c->node);
		free(c);
	}

	if (!avl_is_empty(&cache))
		uloop_timeout_set(t, 1000);
}

void client_init_interface(struct interface *iface)
{
	avl_init(&iface->clients, avl_mac_cmp, false, NULL);
	avl_init(&iface->client_ids, avl_strcmp, false, NULL);
}

static void __client_free(struct interface *iface, struct client *cl)
{
	if (cl->id_node.key)
		avl_delete(&iface->client_ids, &cl->id_node);
	avl_delete(&iface->clients, &cl->node);
	kvlist_free(&cl->kvdata);
	free(cl->device);
	spotfilter_bpf_set_client(iface, &cl->key, NULL);
	free(cl);
}

void client_free(struct interface *iface, struct client *cl)
{
	spotfilter_ubus_notify(iface, cl, "client_delete");
	__client_free(iface, cl);
}

static void client_set_id(struct interface *iface, struct client *cl, const char *id)
{
	if (id == cl->id_node.key)
		return;

	if (id && cl->id_node.key && !strcmp(id, cl->id_node.key))
		return;

	if (cl->id_node.key) {
		avl_delete(&iface->client_ids, &cl->id_node);
		free((void *) cl->id_node.key);
		cl->id_node.key = NULL;
	}

	if (!id)
		return;

	cl->id_node.key = strdup(id);
	avl_insert(&iface->client_ids, &cl->id_node);
}

int client_set(struct interface *iface, const void *addr, const char *id,
	       int state, int dns_state, int accounting, struct blob_attr *data,
	       const char *device, bool flush)
{
	struct cache_entry *c;
	struct blob_attr *cur;
	struct client *cl;
	bool new_client = false;
	int rem;

	cl = avl_find_element(&iface->clients, addr, cl, node);
	if (!cl) {
		cl = calloc(1, sizeof(*cl));
		cl->node.key = &cl->key.addr;
		memcpy(cl->key.addr, addr, ETH_ALEN);
		avl_insert(&iface->clients, &cl->node);
		cl->data.cur_class = iface->default_class;
		cl->data.dns_class = iface->default_dns_class;
		kvlist_init(&cl->kvdata, kvlist_blob_len);
		new_client = true;
	}

	client_set_id(iface, cl, id);
	if (!new_client)
		spotfilter_bpf_get_client(iface, &cl->key, &cl->data);

	c = avl_find_element(&cache, addr, c, node);
	if (c) {
		if (!cl->data.ip4addr)
			cl->data.ip4addr = c->ip4addr;
		if (!cl->data.ip6addr[0])
			memcpy(cl->data.ip6addr, c->ip6addr, sizeof(cl->data.ip6addr));
	}

	if (state >= SPOTFILTER_NUM_CLASS || dns_state >= SPOTFILTER_NUM_CLASS) {
		if (new_client)
			__client_free(iface, cl);

		return -1;
	}

	blobmsg_for_each_attr(cur, data, rem) {
		if (!blobmsg_check_attr(cur, true))
			continue;

		kvlist_set(&cl->kvdata, blobmsg_name(cur), cur);
	}
	if (device) {
		free(cl->device);
		cl->device = strdup(device);
	}
	if (state >= 0)
		cl->data.cur_class = state;
	if (dns_state >= 0)
		cl->data.dns_class = dns_state;
	if (accounting >= 0)
		cl->data.flags = accounting;
	if (flush) {
		kvlist_free(&cl->kvdata);
		cl->data.packets_ul = 0;
		cl->data.packets_dl = 0;
		cl->data.bytes_ul = 0;
		cl->data.bytes_dl = 0;
	}
	spotfilter_bpf_set_client(iface, &cl->key, &cl->data);

	if (new_client)
		spotfilter_ubus_notify(iface, cl, "client_add");

	return 0;
}

void client_set_ipaddr(const void *mac, const void *addr, bool ipv6)
{
	static struct uloop_timeout gc_timer = {
		.cb = client_gc
	};
	struct interface *iface;
	struct cache_entry *c;
	struct client *cl;

	c = avl_find_element(&cache, mac, c, node);
	if (!c) {
		c = calloc(1, sizeof(*c));
		memcpy(c->macaddr, mac, ETH_ALEN);
		c->node.key = c->macaddr;
		avl_insert(&cache, &c->node);
		if (!gc_timer.pending)
			uloop_timeout_set(&gc_timer, CACHE_TIMEOUT * 1000);
	}

	if (!ipv6 && !c->ip4addr)
		memcpy(&c->ip4addr, addr, sizeof(c->ip4addr));
	else if (ipv6 && !c->ip6addr[0])
		memcpy(&c->ip6addr, addr, sizeof(c->ip6addr));
	else
		return;

	c->time = client_gettime();

	avl_for_each_element(&interfaces, iface, node) {
		cl = avl_find_element(&iface->clients, mac, cl, node);
		if (!cl)
			continue;

		spotfilter_bpf_get_client(iface, &cl->key, &cl->data);

		if (!ipv6 && !cl->data.ip4addr)
			memcpy(&cl->data.ip4addr, addr, sizeof(cl->data.ip4addr));
		else if (ipv6 && !cl->data.ip6addr[0])
			memcpy(&cl->data.ip6addr, addr, sizeof(cl->data.ip6addr));
		else
			continue;

		spotfilter_bpf_set_client(iface, &cl->key, &cl->data);
	}
}
