// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#define _GNU_SOURCE
#include <linux/netlink.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include <linux/nl80211.h>

#include <errno.h>

#include <libubox/uloop.h>

#include "spotfilter.h"

static struct nl_sock *genl;
static struct nl_cb *genl_cb;
static struct uloop_fd genl_fd;
static struct uloop_timeout update_timer;
static int nl80211_id;

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}

struct handler_args {
	const char *group;
	int id;
};

static int family_handler(struct nl_msg *msg, void *arg)
{
	struct handler_args *grp = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int rem_mcgrp;

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
		struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
			  nla_data(mcgrp), nla_len(mcgrp), NULL);

		if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
		    !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
			continue;
		if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]),
			    grp->group, nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])))
			continue;
		grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	}

	return NL_SKIP;
}

static int nl_get_multicast_id(struct nl_sock *sock, const char *family, const char *group)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	struct handler_args grp = {
		.group = group,
		.id = -ENOENT,
	};
	int ret, ctrlid;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		ret = -ENOMEM;
		goto out_fail_cb;
	}

	ctrlid = genl_ctrl_resolve(sock, "nlctrl");

	genlmsg_put(msg, 0, 0, ctrlid, 0,
		    0, CTRL_CMD_GETFAMILY, 0);

	ret = -ENOBUFS;
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = nl_send_auto_complete(sock, msg);
	if (ret < 0)
		goto out;

	ret = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

	while (ret > 0)
		nl_recvmsgs(sock, cb);

	if (ret == 0)
		ret = grp.id;
 nla_put_failure:
 out:
	nl_cb_put(cb);
 out_fail_cb:
	nlmsg_free(msg);
	return ret;
}

static void
nl80211_sock_cb(struct uloop_fd *fd, unsigned int events)
{
	nl_recvmsgs(genl, genl_cb);
}

static void
nl80211_device_update(struct interface *iface, struct device *dev)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, nl80211_id, 0, NLM_F_DUMP,
		    NL80211_CMD_GET_STATION, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, dev->ifindex);

	nl_send_auto_complete(genl, msg);
	nlmsg_free(msg);
	nl_wait_for_ack(genl);
}

static void
nl80211_interface_update(struct interface *iface)
{
	struct client *cl, *tmp;
	struct device *dev;

	avl_for_each_element_safe(&iface->clients, cl, node, tmp) {
		if (cl->idle++ < iface->client_timeout)
			continue;

		if (iface->client_autoremove)
			client_free(iface, cl);
	}

	vlist_for_each_element(&iface->devices, dev, node)
		nl80211_device_update(iface, dev);
}

static void spotfilter_nl80211_update(struct uloop_timeout *t)
{
	struct interface *iface;

	avl_for_each_element(&interfaces, iface, node)
		nl80211_interface_update(iface);

	uloop_timeout_set(t, 1000);
}

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static int valid_msg(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct interface *iface;
	struct device *dev;
	struct client *cl;
	const void *addr;
	int ifindex;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (gnlh->cmd != NL80211_CMD_NEW_STATION)
		return NL_SKIP;

	if (!tb[NL80211_ATTR_IFINDEX] || !tb[NL80211_ATTR_MAC])
		return NL_SKIP;

	ifindex = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);
	addr = nla_data(tb[NL80211_ATTR_MAC]);

	avl_for_each_element(&interfaces, iface, node)
		vlist_for_each_element(&iface->devices, dev, node)
			if (dev->ifindex == ifindex)
				goto found;

	return NL_SKIP;

found:
	cl = avl_find_element(&iface->clients, addr, cl, node);
	if (cl)
		cl->idle = 0;
	else if (iface->client_autocreate)
		client_set(iface, addr, NULL, -1, -1, -1, NULL, device_name(dev), false);

	return NL_SKIP;
}

int spotfilter_nl80211_init(void)
{
	int id;

	genl = nl_socket_alloc();
	if (!genl)
		return -1;

	nl_socket_set_buffer_size(genl, 16384, 16384);
	if (genl_connect(genl))
		goto error;

	nl80211_id = genl_ctrl_resolve(genl, "nl80211");
	if (nl80211_id < 0)
		goto error;

	id = nl_get_multicast_id(genl, "nl80211", "mlme");
	if (id < 0)
		goto error;

	if (nl_socket_add_membership(genl, id) < 0)
		goto error;

	genl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	nl_cb_set(genl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_set(genl_cb, NL_CB_VALID, NL_CB_CUSTOM, valid_msg, NULL);
	nl_socket_set_cb(genl, genl_cb);

	genl_fd.fd = nl_socket_get_fd(genl);
	genl_fd.cb = nl80211_sock_cb;
	uloop_fd_add(&genl_fd, ULOOP_READ);

	update_timer.cb = spotfilter_nl80211_update;
	uloop_timeout_set(&update_timer, 1);

	return 0;

error:
	spotfilter_nl80211_done();
	return -1;
}

void spotfilter_nl80211_done(void)
{
	if (!genl)
		return;

	uloop_timeout_cancel(&update_timer);
	uloop_fd_delete(&genl_fd);
	nl_socket_free(genl);
	genl = NULL;
}
