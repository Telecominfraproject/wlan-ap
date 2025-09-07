/*
 * Copyright (C) 2017 John Crispin <john@phrozen.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "udevmand.h"

struct blob_buf b = { 0 };

static char *iftype_string[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_STATION] = "station",
	[NL80211_IFTYPE_AP] = "ap",
	[NL80211_IFTYPE_MONITOR] = "monitor",
	[NL80211_IFTYPE_ADHOC] = "adhoc",
};

void blobmsg_add_iface(struct blob_buf *bbuf, char *name, int index)
{
	static char _ifname[IF_NAMESIZE];
	char *ifname = if_indextoname(index, _ifname);

	if (!ifname)
		return;
	blobmsg_add_string(&b, name, ifname);
}

void blobmsg_add_iftype(struct blob_buf *bbuf, const char *name, const uint32_t iftype)
{
	if (iftype_string[iftype])
		blobmsg_add_string(&b, name, iftype_string[iftype]);
	else
		blobmsg_add_u32(&b, name, iftype);
}

void blobmsg_add_ipv4(struct blob_buf *bbuf, const char *name, const uint8_t* addr)
{
	char ip[16];

	snprintf(ip, sizeof(ip), "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	blobmsg_add_string(&b, name, ip);
}

void blobmsg_add_ipv6(struct blob_buf *bbuf, const char *name, const uint8_t* _addr)
{
	const uint16_t* addr = (const uint16_t*) _addr;
	char ip[40];

	snprintf(ip, sizeof(ip), "%x:%x:%x:%x:%x:%x:%x:%x",
		ntohs(addr[0]), ntohs(addr[1]), ntohs(addr[2]), ntohs(addr[3]),
		ntohs(addr[4]), ntohs(addr[5]), ntohs(addr[6]), ntohs(addr[7]));
	blobmsg_add_string(&b, name, ip);
}

void blobmsg_add_mac(struct blob_buf *bbuf, const char *name, const uint8_t* addr)
{
	char mac[18];

	snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	blobmsg_add_string(&b, name, mac);
}
