// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __DHCPRELAY_H
#define __DHCPRELAY_H

#include <libubox/blobmsg.h>
#include <libubox/ulog.h>
#include <libubox/uloop.h>
#include <libubox/vlist.h>
#include <libubox/utils.h>
#include <net/if.h>
#include <stdint.h>

#define DHCPRELAY_IFB_NAME "ifb-dhcprelay"
#define DHCPRELAY_PRIO_BASE	0x130

struct packet_l2 {
	int ifindex;
	uint16_t vlan_tci;
	uint16_t vlan_proto;
	uint8_t addr[6];
};

struct packet {
	struct packet_l2 l2;

	uint16_t len;
	uint16_t dhcp_tail;

	void *head;
	void *data;
	void *end;
};

struct device {
	struct vlist_node node;
	char ifname[IFNAMSIZ + 1];

	int ifindex;
	bool upstream;
	bool active;
};

struct bridge_entry {
	struct avl_node node;
	struct blob_attr *vlans, *ignore, *upstream;
};

static inline void *pkt_push(struct packet *pkt, unsigned int len)
{
	if (pkt->data - pkt->head < len)
		return NULL;

	pkt->data -= len;
	pkt->len += len;

	return pkt->data;
}

static inline void *pkt_peek(struct packet *pkt, unsigned int len)
{
	if (len > pkt->len)
		return NULL;

	return pkt->data;
}

static inline void *pkt_pull(struct packet *pkt, unsigned int len)
{
	void *ret = pkt_peek(pkt, len);

	if (!ret)
		return NULL;

	pkt->data += len;
	pkt->len -= len;

	return ret;
}

static inline const char *bridge_entry_name(struct bridge_entry *br)
{
	return br->node.key;
}

int dhcprelay_run_cmd(char *cmd, bool ignore_error);

int dhcprelay_dev_init(void);
void dhcprelay_dev_done(void);
void dhcprelay_dev_add(const char *name, bool upstream);
void dhcprelay_dev_config_update(struct blob_attr *br, struct blob_attr *dev);
void dhcprelay_dev_send(struct packet *pkt, int ifindex, const uint8_t *addr, uint16_t proto);
void dhcprelay_update_devices(void);

void dhcprelay_ubus_init(void);
void dhcprelay_ubus_done(void);
void dhcprelay_ubus_notify(const char *type, struct packet *pkt);
void dhcprelay_ubus_query_bridge(struct bridge_entry *br);

void dhcprelay_packet_cb(struct packet *pkt);
void dhcprelay_handle_response(struct packet *pkt);
int dhcprelay_forward_request(struct packet *pkt, struct blob_attr *data);
int dhcprelay_add_options(struct packet *pkt, struct blob_attr *data);

#endif
