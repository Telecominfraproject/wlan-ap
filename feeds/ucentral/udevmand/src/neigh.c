/*
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
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

static struct nl_socket rtnl_sock;

static int
avl_ipv4_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, 4);
}

static int
avl_ipv6_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, 16);
}

static struct avl_tree ip4_tree = AVL_TREE_INIT(ip4_tree, avl_ipv4_cmp, false, NULL);
static struct avl_tree ip6_tree = AVL_TREE_INIT(ip6_tree, avl_ipv6_cmp, false, NULL);

static void
neigh_del(struct neigh *neigh)
{
	struct avl_tree *ip_tree = &ip4_tree;

	if (neigh->ip_ver == 6)
		ip_tree = &ip6_tree;

	uloop_timeout_cancel(&neigh->ageing);
	list_del(&neigh->list);
	avl_delete(ip_tree, &neigh->avl);
	free(neigh);
}

static void
neigh_ageing_cb(struct uloop_timeout *t)
{
	struct neigh *neigh = container_of(t, struct neigh, ageing);

	neigh_del(neigh);
}

static void
neigh_handler(struct nlmsghdr *nh, int type)
{
	uint8_t *lladdr, *dummy[6] = { 0 };
	uint8_t set[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	uint8_t clear[6] = { };
	uint8_t mcast[2] = { 0x33, 0x33 };
	struct ndmsg *ndm = nlmsg_data(nh);
	struct nlattr *nda[__NDA_MAX];
	uint8_t *mac_buf, *ip_buf;
	struct avl_tree *ip_tree;
	struct neigh *neigh;
	struct mac *mac;
	int ip_len, ip_ver, refresh = 0;
	void *dst;

	nlmsg_parse(nh, sizeof(struct ndmsg), nda, NDA_MAX, NULL);
	if (!nda[NDA_DST])
		return;

	dst = nla_data(nda[NDA_DST]);
	lladdr = nda[NDA_LLADDR] ? nla_data(nda[NDA_LLADDR]) : dummy;
	if (!memcmp(lladdr, set, 6) || !memcmp(lladdr, clear, 6) || !memcmp(lladdr, mcast, 2) || *lladdr == 0x1)
		return;

	mac = mac_find(lladdr);

	switch (ndm->ndm_family) {
	case AF_INET:
		ip_len = ip_ver = 4;
		ip_tree = &ip4_tree;
		break;

	case AF_INET6:
		ip_len = 16;
		ip_ver = 6;
		ip_tree = &ip6_tree;
		break;

	default:
		return;
	}

	neigh = avl_find_element(ip_tree, dst, neigh, avl);

	if (neigh) {
		if (!type)
			ULOG_INFO("del ipv%d for "MAC_FMT" on %s\n", neigh->ip_ver,  MAC_VAR(lladdr), neigh->ifname);
		refresh = 1;
		neigh_del(neigh);
	}

	if (!type)
		return;

	neigh = calloc_a(sizeof(struct neigh),
		&ip_buf, ip_len, &mac_buf, 6);
	if (!neigh)
		return;
	neigh->ip = memcpy(ip_buf, dst, ip_len);
	neigh->iface = ndm->ndm_ifindex;
	neigh->avl.key = neigh->ip;
	neigh->ip_ver = ip_ver;
	neigh->ageing.cb = neigh_ageing_cb;
	uloop_timeout_set(&neigh->ageing, 24 * 60 * 60 * 1000);
	if_indextoname(neigh->iface, neigh->ifname);
	if (neigh->ip_ver == 4)
		list_add(&neigh->list, &mac->neigh4);
	else
		list_add(&neigh->list, &mac->neigh6);
	avl_insert(ip_tree, &neigh->avl);
	mac_update(mac, neigh->ifname);
	if (!refresh)
		ULOG_INFO("new ipv%d for "MAC_FMT" on %s\n", neigh->ip_ver,  MAC_VAR(lladdr), neigh->ifname);
}

static int
neigh_netlink_cb(struct nl_msg *msg, void *arg)
{

	struct nlmsghdr *nh = nlmsg_hdr(msg);


	switch (nh->nlmsg_type) {
	case RTM_NEWNEIGH:
		neigh_handler(nh, 1);
		break;

	case RTM_DELNEIGH:
		neigh_handler(nh, 0);
		break;

	default:
		break;
	}
	return NL_OK;
}

void
neigh_enum(void)
{
	struct rtgenmsg msg = { .rtgen_family = AF_UNSPEC };

	nl_send_simple(rtnl_sock.sock, RTM_GETNEIGH, NLM_F_DUMP, &msg, sizeof(msg));
	nl_wait_for_ack(rtnl_sock.sock);
}

void
neigh_flush(void)
{
	struct neigh *neigh, *tmp;

	avl_for_each_element_safe(&ip4_tree, neigh, avl, tmp)
		neigh_del(neigh);
	avl_for_each_element_safe(&ip6_tree, neigh, avl, tmp)
		neigh_del(neigh);
}

int
neigh_init(void)
{
	ULOG_INFO("open neigh netlink socket\n");
	if (!nl_status_socket(&rtnl_sock, NETLINK_ROUTE, neigh_netlink_cb, NULL)) {
		ULOG_ERR("failed to open rtnl socket\n");
		return -1;
	}

	return 0;
}

void
neigh_done(void)
{
	nl_socket_free(rtnl_sock.sock);
}
