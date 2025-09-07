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

static void
nl_handler_nl_status(struct uloop_fd *u, unsigned int statuss)
{
	struct nl_socket *ev = container_of(u, struct nl_socket, uloop);
	int err;
	socklen_t errlen = sizeof(err);

	if (!u->error) {
		nl_recvmsgs_default(ev->sock);
		return;
	}

	if (getsockopt(u->fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen))
		goto abort;

	switch(err) {
	case ENOBUFS:
		ev->bufsize *= 2;
		if (nl_socket_set_buffer_size(ev->sock, ev->bufsize, 0))
			goto abort;
		break;

	default:
		goto abort;
	}
	u->error = false;
	return;

abort:
	uloop_fd_delete(&ev->uloop);
	return;
}

static struct nl_sock *
nl_create_socket(int protocol, int groups)
{
	struct nl_sock *sock;

	sock = nl_socket_alloc();
	if (!sock)
		return NULL;

	if (groups)
		nl_join_groups(sock, groups);

	if (nl_connect(sock, protocol))
		return NULL;

	return sock;
}

static bool
nl_raw_status_socket(struct nl_socket *ev, int protocol, int groups,
		    uloop_fd_handler cb, int flags)
{
	ev->sock = nl_create_socket(protocol, groups);
	if (!ev->sock)
		return false;

	ev->uloop.fd = nl_socket_get_fd(ev->sock);
	ev->uloop.cb = cb;
	if (uloop_fd_add(&ev->uloop, ULOOP_READ|flags))
		return false;

	return true;
}

bool
nl_status_socket(struct nl_socket *ev, int protocol,
		int (*cb)(struct nl_msg *msg, void *arg), void *priv)
{
	if (!nl_raw_status_socket(ev, protocol, 0, nl_handler_nl_status, ULOOP_ERROR_CB))
		return false;

	nl_socket_modify_cb(ev->sock, NL_CB_VALID, NL_CB_CUSTOM, cb, priv);
	nl_socket_disable_seq_check(ev->sock);
	ev->bufsize = 65535;
	if (nl_socket_set_buffer_size(ev->sock, ev->bufsize, 0))
		return false;

	return true;
}

int genl_send_and_recv(struct nl_socket *ev, struct nl_msg * msg)
{
	int ret = nl_send_auto_complete(ev->sock, msg);

	nlmsg_free(msg);
	if (ret < 0)
		return ret;

	nl_recvmsgs_default(ev->sock);
	nl_wait_for_ack(ev->sock);

	return 0;
}


