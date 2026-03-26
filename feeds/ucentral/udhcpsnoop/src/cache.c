// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */

#include <libubox/avl.h>

#include "dhcpsnoop.h"
#include "msg.h"

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_VAR(x) x[0], x[1], x[2], x[3], x[4], x[5]

#define IP_FMT  "%d.%d.%d.%d"
#define IP_VAR(x) x[0], x[1], x[2], x[3]

struct mac {
        struct avl_node avl;
	uint8_t mac[6];
	uint8_t ip[4];
	char hostname[64];
	struct uloop_timeout rebind;
};

/* Temporary store for hostnames seen in client DHCP Discover/Request packets.
 * The hostname lives in these client-sent messages (Option 12), not in the
 * server ACK.  We key by MAC and look it up when the ACK arrives. */
struct pending_hostname {
	struct avl_node avl;
	uint8_t mac[6];
	char hostname[64];
};

static int
avl_mac_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, 6);
}

static struct avl_tree mac_tree     = AVL_TREE_INIT(mac_tree,     avl_mac_cmp, false, NULL);
static struct avl_tree pending_tree = AVL_TREE_INIT(pending_tree, avl_mac_cmp, false, NULL);

static void
cache_expire(struct uloop_timeout *t)
{
	struct mac *mac = container_of(t, struct mac, rebind);

	avl_delete(&mac_tree, &mac->avl);
	free(mac);
}

/* Called from dev.c for Discover and Request packets to stash the hostname
 * sent by the client before we see the server's ACK. */
void
cache_pending_hostname(const uint8_t *chaddr, const char *hostname)
{
	struct pending_hostname *p;

	if (!hostname || !hostname[0])
		return;

	p = avl_find_element(&pending_tree, chaddr, p, avl);
	if (!p) {
		p = malloc(sizeof(*p));
		if (!p)
			return;
		memset(p, 0, sizeof(*p));
		memcpy(p->mac, chaddr, 6);
		p->avl.key = p->mac;
		avl_insert(&pending_tree, &p->avl);
	}
	snprintf(p->hostname, sizeof(p->hostname), "%s", hostname);
}

void
cache_entry(void *_msg, uint32_t rebind, const char *hostname)
{
	struct dhcpv4_message *msg = (struct dhcpv4_message *) _msg;
	struct pending_hostname *p;
	struct mac *mac;

	mac = avl_find_element(&mac_tree, msg->chaddr, mac, avl);

	if (!mac) {
		mac = malloc(sizeof(*mac));
		if (!mac)
			return;
		memset(mac, 0, sizeof(*mac));
		memcpy(mac->mac, msg->chaddr, 6);
		mac->avl.key = mac->mac;
		mac->rebind.cb = cache_expire;
		avl_insert(&mac_tree, &mac->avl);
	}
	memcpy(mac->ip, &msg->yiaddr.s_addr, 4);

	/* Prefer hostname from the ACK itself (rare), otherwise use the one
	 * captured from the client's earlier Discover/Request. */
	if (hostname && hostname[0]) {
		snprintf(mac->hostname, sizeof(mac->hostname), "%s", hostname);
	} else {
		p = avl_find_element(&pending_tree, msg->chaddr, p, avl);
		if (p && p->hostname[0])
			snprintf(mac->hostname, sizeof(mac->hostname), "%s", p->hostname);
	}

	uloop_timeout_set(&mac->rebind, rebind * 1000);
}

void
cache_dump(struct blob_buf *b)
{
	struct mac *mac;

	avl_for_each_element(&mac_tree, mac, avl) {
		char addr[18];
		char ip[16];

		snprintf(addr, sizeof(addr), MAC_FMT, MAC_VAR(mac->mac));
		snprintf(ip, sizeof(ip), IP_FMT, IP_VAR(mac->ip));

		blobmsg_add_string(b, addr, ip);
	}
}

void
cache_dump_full(struct blob_buf *b)
{
	struct mac *mac;

	avl_for_each_element(&mac_tree, mac, avl) {
		char addr[18];
		char ip[16];
		void *entry;

		snprintf(addr, sizeof(addr), MAC_FMT, MAC_VAR(mac->mac));
		snprintf(ip, sizeof(ip), IP_FMT, IP_VAR(mac->ip));

		entry = blobmsg_open_table(b, addr);
		blobmsg_add_string(b, "ip", ip);
		if (mac->hostname[0])
			blobmsg_add_string(b, "hostname", mac->hostname);
		blobmsg_close_table(b, entry);
	}
}
