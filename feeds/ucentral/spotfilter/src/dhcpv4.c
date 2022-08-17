// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include "spotfilter.h"

enum dhcpv4_msg {
	DHCPV4_MSG_DISCOVER = 1,
	DHCPV4_MSG_OFFER = 2,
	DHCPV4_MSG_REQUEST = 3,
	DHCPV4_MSG_DECLINE = 4,
	DHCPV4_MSG_ACK = 5,
	DHCPV4_MSG_NAK = 6,
	DHCPV4_MSG_RELEASE = 7,
	DHCPV4_MSG_INFORM = 8,
	DHCPV4_MSG_FORCERENEW = 9,
};

enum dhcpv4_opt {
	DHCPV4_OPT_PAD = 0,
	DHCPV4_OPT_NETMASK = 1,
	DHCPV4_OPT_ROUTER = 3,
	DHCPV4_OPT_DNSSERVER = 6,
	DHCPV4_OPT_DOMAIN = 15,
	DHCPV4_OPT_MTU = 26,
	DHCPV4_OPT_BROADCAST = 28,
	DHCPV4_OPT_NTPSERVER = 42,
	DHCPV4_OPT_LEASETIME = 51,
	DHCPV4_OPT_MESSAGE = 53,
	DHCPV4_OPT_SERVERID = 54,
	DHCPV4_OPT_REQOPTS = 55,
	DHCPV4_OPT_RENEW = 58,
	DHCPV4_OPT_REBIND = 59,
	DHCPV4_OPT_IPADDRESS = 50,
	DHCPV4_OPT_MSG_TYPE = 53,
	DHCPV4_OPT_HOSTNAME = 12,
	DHCPV4_OPT_REQUEST = 17,
	DHCPV4_OPT_USER_CLASS = 77,
	DHCPV4_OPT_AUTHENTICATION = 90,
	DHCPV4_OPT_SEARCH_DOMAIN = 119,
	DHCPV4_OPT_FORCERENEW_NONCE_CAPABLE = 145,
	DHCPV4_OPT_END = 255,
};

struct dhcpv4_message {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	struct in_addr ciaddr;
	struct in_addr yiaddr;
	struct in_addr siaddr;
	struct in_addr giaddr;
	uint8_t chaddr[16];
	char sname[64];
	char file[128];
	uint32_t magic;
	uint8_t options[];
} __attribute__((packed));

#define DHCPV4_MAGIC 0x63825363

struct dhcpv4_option {
	uint8_t type;
	uint8_t len;
	uint8_t data[];
};

#define dhcpv4_for_each_option(opt, start, end)				\
	for (opt = (const struct dhcpv4_option *)(start);		\
	     &opt[1] <= (const struct dhcpv4_option *)(end) &&		\
	     &opt->data[opt->len] <= (const uint8_t *)(end);		\
	     opt = (const struct dhcpv4_option *)&opt->data[opt->len])

void spotfilter_recv_dhcpv4(const void *msgdata, int len, const void *eth_addr)
{
	const struct dhcpv4_message *msg = msgdata;
	const struct dhcpv4_option *opt;
	uint8_t bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	int op = -1;

	if (ntohl(msg->magic) != DHCPV4_MAGIC)
		return;

	if (msg->op != 2 || msg->htype != 1 || msg->hlen != 6)
		return;

	if (memcmp(eth_addr, bcast_addr, ETH_ALEN) != 0 &&
	    memcmp(eth_addr, msg->chaddr, ETH_ALEN) != 0)
		return;

	dhcpv4_for_each_option(opt, msg->options, msgdata + len) {
		switch (opt->type) {
		case DHCPV4_OPT_MESSAGE:
			if (opt->len != 1)
				break;

			op = opt->data[0];
			break;
		}
	}

	if (op != DHCPV4_MSG_ACK)
		return;

	client_set_ipaddr(msg->chaddr, (uint32_t *)&msg->yiaddr, false);
}

