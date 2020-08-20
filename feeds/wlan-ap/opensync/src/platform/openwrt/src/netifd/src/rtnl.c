/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unl.h>
#include <ev.h>
#include <net/if.h>
#include <linux/limits.h>

#include "ovsdb.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"

#include <syslog.h>

static struct unl unl;
static ev_io unl_io;

static ovsdb_table_t table_Wifi_Route_State;

static void route_handler(struct nlmsghdr *nh, int add)
{
	struct schema_Wifi_Route_State route;
	struct nlattr *nla[__RTA_MAX];
	struct rtmsg *rtm = nlmsg_data(nh);
	void *dst = NULL, *gateway = NULL;
	char ifname[IF_NAMESIZE];
	uint32_t oif = 0, tmp = 0xffffffff;
	char buf[64];
	//char mask[8];
	json_t *where;
	json_t *cond;

	nlmsg_parse(nh, sizeof(struct rtmsg), nla, __RTA_MAX - 1, NULL);
	if (nla[RTA_DST])
		dst = nla_data(nla[RTA_DST]);
	if (nla[RTA_GATEWAY])
		gateway = nla_data(nla[RTA_GATEWAY]);
	if (nla[RTA_OIF])
		oif = nla_get_u32(nla[RTA_OIF]);

	if (!oif)
		return;

	if (!if_indextoname(oif, ifname))
		return;

	if (!strcmp(ifname, "lo"))
		return;

	SCHEMA_SET_STR(route.if_name, ifname);
	route.gateway_hwaddr_exists = false;
	switch (rtm->rtm_family) {
	case AF_INET:
		if (dst) {
			SCHEMA_SET_STR(route.dest_addr, inet_ntop(AF_INET, dst, buf, sizeof(buf)));
			tmp >>= 32 - rtm->rtm_dst_len;
			SCHEMA_SET_STR(route.dest_mask, inet_ntop(AF_INET, &tmp, buf, sizeof(buf)));
		} else {
			SCHEMA_SET_STR(route.dest_addr, "0.0.0.0");
			SCHEMA_SET_STR(route.dest_mask, "0.0.0.0");
		}
		if (gateway) {
			SCHEMA_SET_STR(route.gateway, inet_ntop(AF_INET, gateway, buf, sizeof(buf)));
		} else {
			SCHEMA_SET_STR(route.gateway, "0.0.0.0");
		}
		break;

	case AF_INET6:
	/*	if (dst) {
			SCHEMA_SET_STR(route.dest_addr, inet_ntop(AF_INET6, dst, buf, sizeof(buf)));
			sprintf(mask, "/%d", rtm->rtm_dst_len);
			strncat(buf, mask, sizeof(buf) - 1);
		} else {
			SCHEMA_SET_STR(route.dest_addr, "0:0:0:0");
		}
		SCHEMA_SET_STR(route.dest_mask, "0:0:0:0");
		if (gateway)
			SCHEMA_SET_STR(route.gateway, inet_ntop(AF_INET6, gateway, buf, sizeof(buf)));
		else
			SCHEMA_SET_STR(route.gateway, "0:0:0:0");
		break;
	*/
		return;

	default:
		return;
	}

	where = json_array();
	cond = ovsdb_tran_cond_single("if_name", OFUNC_EQ, route.if_name);
	json_array_append_new(where, cond);
	cond = ovsdb_tran_cond_single("dest_addr", OFUNC_EQ, route.dest_addr);
	json_array_append_new(where, cond);
	if (rtm->rtm_family == AF_INET) {
		cond = ovsdb_tran_cond_single("dest_mask", OFUNC_EQ, route.dest_mask);
		json_array_append_new(where, cond);
	}
	cond = ovsdb_tran_cond_single("gateway", OFUNC_EQ, route.gateway);
	json_array_append_new(where, cond);

	if (add) {
		route._update_type = OVSDB_UPDATE_MODIFY;
		if (!ovsdb_table_upsert_where(&table_Wifi_Route_State, where, &route, false))
			LOG(ERR, "netifd: failed to add route");
	} else {
		route._update_type = OVSDB_UPDATE_DEL;
		if (!ovsdb_table_delete_where(&table_Wifi_Route_State, where))
			LOG(ERR, "netifd: failed to delete route");
	}
}

static int rtnl_ev_cb(struct nl_msg *msg, void *arg)
{

	struct nlmsghdr *nh = nlmsg_hdr(msg);

	switch (nh->nlmsg_type) {
	case RTM_NEWLINK:
		break;

	case RTM_DELLINK:
		break;

	case RTM_NEWNEIGH:
		break;

	case RTM_DELNEIGH:
		break;

	case RTM_NEWROUTE:
		route_handler(nh, 1);
		break;

	case RTM_DELROUTE:
		route_handler(nh, 0);
		break;

	default:
		break;
	}

	return 0;
}

static void rtnl_ev(struct ev_loop *ev, struct ev_io *io, int event)
{
	unl_loop(&unl, rtnl_ev_cb, NULL);
}

static void rtnl_dump(int cmd, unsigned char rtm_family)
{
	struct nl_msg *msg;
	struct rtmsg hdr = {
		.rtm_family = rtm_family,
	};
	msg = unl_rtnl_msg(&unl, cmd, true);
	nlmsg_append(msg, &hdr, sizeof(hdr), 0);

	unl_request(&unl, msg, rtnl_ev_cb, NULL);
}

int rtnl_init(struct ev_loop *loop)
{
	OVSDB_TABLE_INIT_NO_KEY(Wifi_Route_State);

	unl_rtnl_init(&unl);

	nl_socket_add_membership(unl.sock, RTNLGRP_LINK);
	nl_socket_add_membership(unl.sock, RTNLGRP_NEIGH);
	nl_socket_add_membership(unl.sock, RTNLGRP_IPV4_ROUTE);
	nl_socket_add_membership(unl.sock, RTNLGRP_IPV6_ROUTE);

	rtnl_dump(RTM_GETLINK, AF_INET);
	rtnl_dump(RTM_GETROUTE, AF_INET);
	rtnl_dump(RTM_GETNEIGH, AF_INET);

	rtnl_dump(RTM_GETLINK, AF_INET6);
	rtnl_dump(RTM_GETROUTE, AF_INET6);
	rtnl_dump(RTM_GETNEIGH, AF_INET6);

	ev_io_init(&unl_io, rtnl_ev, unl.sock->s_fd, EV_READ);
	ev_io_start(loop, &unl_io);

	return 0;
}
