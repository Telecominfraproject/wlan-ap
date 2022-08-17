// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/socket.h>
#include <linux/rtnetlink.h>
#include "spotfilter.h"

static struct nl_sock *rtnl;
bool rtnl_ignore_errors;

static int
spotfilter_nl_error_cb(struct sockaddr_nl *nla, struct nlmsgerr *err,
		   void *arg)
{
	struct nlmsghdr *nlh = (struct nlmsghdr *) err - 1;
	struct nlattr *tb[NLMSGERR_ATTR_MAX + 1];
	struct nlattr *attrs;
	int ack_len = sizeof(*nlh) + sizeof(int) + sizeof(*nlh);
	int len = nlh->nlmsg_len;
	const char *errstr = "(unknown)";

	if (rtnl_ignore_errors)
		return NL_STOP;

	if (!(nlh->nlmsg_flags & NLM_F_ACK_TLVS))
		return NL_STOP;

	if (!(nlh->nlmsg_flags & NLM_F_CAPPED))
		ack_len += err->msg.nlmsg_len - sizeof(*nlh);

	attrs = (void *) ((unsigned char *) nlh + ack_len);
	len -= ack_len;

	nla_parse(tb, NLMSGERR_ATTR_MAX, attrs, len, NULL);
	if (tb[NLMSGERR_ATTR_MSG])
		errstr = nla_data(tb[NLMSGERR_ATTR_MSG]);

	fprintf(stderr, "Netlink error(%d): %s\n", err->error, errstr);

	return NL_STOP;
}

int rtnl_call(struct nl_msg *msg)
{
	int ret;

	ret = nl_send_auto_complete(rtnl, msg);
	nlmsg_free(msg);

	if (ret < 0)
		return ret;

	return nl_wait_for_ack(rtnl);
}

int rtnl_fd(void)
{
	return nl_socket_get_fd(rtnl);
}

int rtnl_init(void)
{
	int fd, opt;

	if (rtnl)
		return 0;

	rtnl = nl_socket_alloc();
	if (!rtnl)
		return -1;

	if (nl_connect(rtnl, NETLINK_ROUTE))
		goto free;

	nl_socket_disable_seq_check(rtnl);
	nl_socket_set_buffer_size(rtnl, 65536, 0);
	nl_cb_err(nl_socket_get_cb(rtnl), NL_CB_CUSTOM, spotfilter_nl_error_cb, NULL);

	fd = nl_socket_get_fd(rtnl);

	opt = 1;
	setsockopt(fd, SOL_NETLINK, NETLINK_EXT_ACK, &opt, sizeof(opt));

	opt = 1;
	setsockopt(fd, SOL_NETLINK, NETLINK_CAP_ACK, &opt, sizeof(opt));

	return 0;

free:
	nl_socket_free(rtnl);
	rtnl = NULL;
	return -1;
}
