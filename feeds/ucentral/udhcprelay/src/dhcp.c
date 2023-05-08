// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */

#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include "dhcprelay.h"
#include "msg.h"

static const char *
dhcprelay_parse_ipv4(const void *buf, size_t len, uint16_t port, uint32_t *expire,
		     size_t *tail, bool *response)
{
	const struct dhcpv4_message *msg = buf;
	const uint8_t *pos, *end;
	uint32_t leasetime = 0, rebind = 0, renew = 0;
	char type = 0;

	if (port != 67 && port != 68)
		return NULL;

	if (len < sizeof(*msg))
		return NULL;

	if (ntohl(msg->magic) != DHCPV4_MAGIC)
		return NULL;

	pos = msg->options;
	end = buf + len;
	*tail = (const void *)pos - buf;

	while (pos < end) {
		const uint8_t *opt;

		opt = pos++;
		if (*opt == DHCPV4_OPT_PAD)
			continue;

		if (*opt == DHCPV4_OPT_END)
			break;

		if (pos >= end || 1 + *pos > end - pos)
			break;

		pos += *pos + 1;
		if (pos >= end)
			break;

		*tail = (const void *)pos - buf;
		switch (*opt) {
		case DHCPV4_OPT_MSG_TYPE:
			if (!opt[1])
				continue;
			type = opt[2];
			break;
		case DHCPV4_OPT_LEASETIME:
			if (opt[1] != 4)
				continue;
			leasetime = *((uint32_t *) &opt[2]);
			break;
		case DHCPV4_OPT_REBIND:
			if (opt[1] != 4)
				continue;
			rebind = *((uint32_t *) &opt[2]);
			break;
		case DHCPV4_OPT_RENEW:
			if (opt[1] != 4)
				continue;
			renew = *((uint32_t *) &opt[2]);
			break;
		}
	}

	if (renew)
		*expire = renew;
	else if (rebind)
		*expire = rebind;
	else if (leasetime)
		*expire = leasetime;
	else
		*expire = 24 * 60 * 60;
	*expire = ntohl(*expire);

	switch(type) {
	case DHCPV4_MSG_DISCOVER:
		return "discover";
	case DHCPV4_MSG_REQUEST:
		return "request";
	case DHCPV4_MSG_DECLINE:
		return "decline";
	case DHCPV4_MSG_RELEASE:
		return "release";
	case DHCPV4_MSG_INFORM:
		return "inform";

	case DHCPV4_MSG_OFFER:
	case DHCPV4_MSG_ACK:
	case DHCPV4_MSG_NAK:
		*response = true;
		return NULL;
	}

	return NULL;
}

static bool
proto_is_vlan(uint16_t proto)
{
	return proto == ETH_P_8021Q || proto == ETH_P_8021AD;
}

static int
dhcprelay_add_option(struct packet *pkt, uint32_t opt, const void *data, size_t len)
{
	uint8_t *tail = pkt->data + pkt->dhcp_tail;

	if (opt > 255 || len > 255)
		return -1;

	if ((void *)tail + len + 1 > pkt->end)
		return -1;

	*(tail++) = opt;
	*(tail++) = len;
	if (data && len)
		memcpy(tail, data, len);
	pkt->dhcp_tail += len + 2;

	return 0;
}

int dhcprelay_add_options(struct packet *pkt, struct blob_attr *data)
{
	static const struct blobmsg_policy policy[2] = {
		{ .type = BLOBMSG_TYPE_INT32 },
		{ .type = BLOBMSG_TYPE_STRING },
	};
	struct blob_attr *tb[2], *cur;
	int rem;

	if (!data)
		goto out;

	if (blobmsg_check_array(data, BLOBMSG_TYPE_ARRAY) < 0)
		return -1;

	blobmsg_for_each_attr(cur, data, rem) {
		blobmsg_parse_array(policy, 2, tb, blobmsg_data(cur), blobmsg_len(cur));

		if (!tb[0] || !tb[1])
			return -1;

		if (!blobmsg_len(tb[1]))
			return -1;

		if (dhcprelay_add_option(pkt, blobmsg_get_u32(tb[0]),
					 blobmsg_get_string(tb[1]), blobmsg_len(tb[1]) - 1))
			return -1;
	}

out:
	dhcprelay_add_option(pkt, DHCPV4_OPT_END, NULL, 0);

	pkt->len = pkt->dhcp_tail;
	if (pkt->len < 300) {
		pkt->len = 300;
		memset(pkt->data + pkt->dhcp_tail, 0, pkt->len - pkt->dhcp_tail);
	}

	return 0;
}

void dhcprelay_packet_cb(struct packet *pkt)
{
	struct ethhdr *eth;
	struct ip *ip;
	struct udphdr *udp;
	uint16_t proto, port;
	const char *type;
	uint32_t rebind = 0;
	size_t tail = 0;
	bool response = false;

	eth = pkt_pull(pkt, sizeof(*eth));
	if (!eth)
		return;

	memcpy(pkt->l2.addr, eth->h_source, ETH_ALEN);

	proto = be16_to_cpu(eth->h_proto);
	if (proto_is_vlan(proto)) {
		struct vlan_hdr *vlan;

		vlan = pkt_pull(pkt, sizeof(*vlan));
		if (!vlan)
			return;

		pkt->l2.vlan_proto = proto;
		pkt->l2.vlan_tci = ntohs(vlan->tci);
		proto = be16_to_cpu(vlan->proto);
	}

	if (proto != ETH_P_IP)
		return;

	ip = pkt_peek(pkt, sizeof(struct ip));
	if (!ip)
		return;

	if (!pkt_pull(pkt, ip->ip_hl * 4))
		return;

	if (ip->ip_p != IPPROTO_UDP)
		return;

	udp = pkt_pull(pkt, sizeof(struct udphdr));
	if (!udp)
		return;

	port = ntohs(udp->uh_sport);
	type = dhcprelay_parse_ipv4(pkt->data, pkt->len, port, &rebind, &tail, &response);

	if (response) {
		dhcprelay_handle_response(pkt);
		return;
	}

	if (!type)
		return;

	pkt->dhcp_tail = tail;
	dhcprelay_ubus_notify(type, pkt);
}
