/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _DNS_H__
#define _DNS_H__

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <linux/udp.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <libubox/avl-cmp.h>
#include <libubox/utils.h>
#include <libubox/uloop.h>
#include <libubox/ulog.h>
#include <libubus.h>
#include <uci.h>
#include <uci_blob.h>

#define FLAG_RESPONSE		0x8000
#define FLAG_AUTHORATIVE	0x0400

#define TYPE_A			0x0001
#define TYPE_PTR		0x000C
#define TYPE_TXT		0x0010
#define TYPE_AAAA		0x001c
#define TYPE_SRV		0x0021
#define TYPE_ANY		0x00ff

#define IS_COMPRESSED(x)	((x & 0xc0) == 0xc0)

#define CLASS_FLUSH		0x8000
#define CLASS_UNICAST		0x8000
#define CLASS_IN		0x0001

#define MAX_NAME_LEN            8096
#define MAX_DATA_LEN            8096

struct vlan_hdr {
	uint16_t h_vlan_TCI;
	uint16_t h_vlan_encapsulated_proto;
};

struct dns_header {
	uint16_t id;
	uint16_t flags;
	uint16_t questions;
	uint16_t answers;
	uint16_t authority;
	uint16_t additional;
} __attribute__((packed));

struct dns_srv_data {
	uint16_t priority;
	uint16_t weight;
	uint16_t port;
} __attribute__((packed));

struct dns_answer {
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdlength;
} __attribute__((packed));

struct dns_question {
	uint16_t type;
	uint16_t class;
} __attribute__((packed));

void dns_handle_packet(uint8_t *buffer, int len);
void ubus_notify_qosify(char *name, char *address, int type, int ttl);

#endif
