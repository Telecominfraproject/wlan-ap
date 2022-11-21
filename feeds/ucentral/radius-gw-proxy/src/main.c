/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libubox/uloop.h>
#include <libubox/usock.h>
#include <libubox/ulog.h>

#include <libubus.h>

#include "ubus.h"

#define RAD_PROX_BUFLEN		(4 * 1024)

#define TLV_NAS_IP		4
#define TLV_PROXY_STATE		33

struct radius_socket {
	struct uloop_fd fd;
	enum socket_type type;
};

struct radius_header {
	uint8_t code;
	uint8_t id;
	uint16_t len;
	char auth[16];
	char avp[];
};

struct radius_tlv {
	uint8_t id;
	uint8_t len;
	char data[];
};

struct radius_proxy_state_key {
	char id[256];
	enum socket_type type;
};

struct radius_proxy_state {
	struct avl_node avl;
	struct radius_proxy_state_key key;
	int port;
};

static struct radius_socket *sock_auth;
static struct radius_socket *sock_acct;
static struct radius_socket *sock_dae;

static int
avl_memcmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, sizeof(struct radius_proxy_state_key));
}

static AVL_TREE(radius_proxy_states, avl_memcmp, false, NULL);
static struct blob_buf b;

static void
radius_proxy_state_add(char *id, int port, enum socket_type type)
{
	struct radius_proxy_state *station;
	struct radius_proxy_state_key key = { .type = type };

	strcpy(key.id, id);
	station = avl_find_element(&radius_proxy_states, &key, station, avl);

	if (!station) {
		ULOG_INFO("new station/port, adding to avl tree\n");
		station = malloc(sizeof(*station));
		memset(station, 0, sizeof(*station));
		strcpy(station->key.id, id);
		station->key.type = type;
		station->avl.key = &station->key;
		avl_insert(&radius_proxy_states, &station->avl);
	}
	station->port = port;
}

static char *
b64enc(char *src, int len)
{
	char *dst;
	int ret;

	if (!src)
		return NULL;
	dst = malloc(len * 4);
	ret = b64_encode(src, len, dst, len * 4);
	if (ret < 1) {
		free(dst);
		return NULL;
	}
	return dst;
}

static char *
b64dec(char *src, int *ret)
{
	int len = strlen(src);
	char *dst = malloc(len);
	*ret = b64_decode(src, dst, len);
	if (*ret < 0)
		return NULL;
	return dst;
}

static void
radius_forward_gw(char *buf, enum socket_type type)
{
	struct radius_header *hdr = (struct radius_header *) buf;
	struct ubus_request async = { };
	char *data = b64enc(buf, ntohs(hdr->len));

	if (!data || !ucentral)
		return;

	blob_buf_init(&b, 0);
	switch (type) {
	case RADIUS_AUTH:
		blobmsg_add_string(&b, "radius", "auth");
		break;
	case RADIUS_ACCT:
		blobmsg_add_string(&b, "radius", "acct");
		break;
	case RADIUS_DAS:
		blobmsg_add_string(&b, "radius", "coa");
		break;
	default:
		return;
	}

	blobmsg_add_string(&b, "data", data);

	ubus_invoke_async(&conn.ctx, ucentral, "radius", b.head, &async);
	ubus_abort_request(&conn.ctx, &async);

	free(data);
}

static int
radius_parse(char *buf, unsigned int len, int port, enum socket_type type, int tx)
{
	struct radius_header *hdr = (struct radius_header *) buf;
	struct radius_tlv *proxy_state = NULL;
	char proxy_state_str[256] = {};
	void *avp = hdr->avp;
	unsigned int len_orig = ntohs(hdr->len);
	uint8_t localhost[] = { 0x7f, 0, 0, 1 };

	if (len_orig != len) {
		ULOG_ERR("invalid header length, %d %d\n", len_orig, len);
		return -1;
	}

	printf("\tcode:%d, id:%d, len:%d\n", hdr->code, hdr->id, len_orig);

	len -= sizeof(*hdr);

	while (len >= sizeof(struct radius_tlv)) {
		struct radius_tlv *tlv = (struct radius_tlv *)avp;

		if (len < tlv->len || tlv->len < sizeof(*tlv)) {
			ULOG_ERR("invalid TLV length\n");
			return -1;
		}

		if (tlv->id == TLV_PROXY_STATE)
			proxy_state = tlv;

		if (type == RADIUS_DAS && tlv->id == TLV_NAS_IP && tlv->len == 6)
			memcpy(tlv->data, &localhost, 4);

		printf("\tID:%d, len:%d\n", tlv->id, tlv->len);
		avp += tlv->len;
		len -= tlv->len;
	}

	if (type == RADIUS_DAS) {
		if (tx) {
			radius_forward_gw(buf, type);
		} else {
			struct sockaddr_in dest;

			memset(&dest, 0, sizeof(dest));
			dest.sin_family = AF_INET;
			dest.sin_port = htons(3799);
			inet_pton(AF_INET, "127.0.0.1", &(dest.sin_addr.s_addr));

			if (sendto(sock_dae->fd.fd, buf, len_orig,
				   MSG_DONTWAIT, (struct sockaddr*)&dest, sizeof(dest)) < 0)
				ULOG_ERR("failed to deliver DAS frame to localhost\n");
		}
		return 0;
	}

	if (!proxy_state) {
		ULOG_ERR("no proxy_state found\n");
		return -1;
	}
	memcpy(proxy_state_str, proxy_state->data, proxy_state->len - 2);
	printf("\tfowarding to %s, prox_state:%s\n", tx ? "gateway" : "hostapd", proxy_state_str);
	if (tx) {
		radius_proxy_state_add(proxy_state_str, port, type);
		radius_forward_gw(buf, type);
	} else {
		struct radius_proxy_state *proxy = avl_find_element(&radius_proxy_states, proxy_state, proxy, avl);
		struct radius_proxy_state_key key = {};
		struct sockaddr_in dest;
		struct radius_socket *sock;

		switch(type) {
		case RADIUS_AUTH:
			sock = sock_auth;
			break;

		case RADIUS_ACCT:
			sock = sock_acct;
			break;
		default:
			ULOG_ERR("bad socket type\n");
			return -1;
		}

		strcpy(key.id, proxy_state_str);
		key.type = type;
		proxy = avl_find_element(&radius_proxy_states, &key, proxy, avl);

		if (!proxy) {
			ULOG_ERR("unknown proxy_state, dropping frame\n");
			return -1;
		}
		memset(&dest, 0, sizeof(dest));
		dest.sin_family = AF_INET;
		dest.sin_port = proxy->port;
		inet_pton(AF_INET, "127.0.0.1", &(dest.sin_addr.s_addr));

		if (sendto(sock->fd.fd, buf, len_orig,
			   MSG_DONTWAIT, (struct sockaddr*)&dest, sizeof(dest)) < 0)
			ULOG_ERR("failed to deliver frame to localhost\n");
	}

	return 0;
}

void
gateway_recv(char *data, enum socket_type type)
{
	int len = 0;
	char *frame;

	frame = b64dec(data, &len);

	if (!frame) {
		ULOG_ERR("failed to b64_decode frame\n");
		return;
	}

	radius_parse(frame, len, 0, type, 0);
	free(frame);
}

static void
sock_recv(struct uloop_fd *u, unsigned int events)
{
	static char buf[RAD_PROX_BUFLEN];
	static char cmsg_buf[( CMSG_SPACE(sizeof(struct in_pktinfo)) + sizeof(int)) + 1];
	static struct sockaddr_in sin;
	char addr_str[INET_ADDRSTRLEN];
	static struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf)
	};
	static struct msghdr msg = {
		.msg_name = &sin,
		.msg_namelen = sizeof(sin),
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cmsg_buf,
		.msg_controllen = sizeof(cmsg_buf),
	};
	struct radius_socket *sock = container_of(u, struct radius_socket, fd);
	int len;

	do {
		len = recvmsg(u->fd, &msg, 0);
		if (len < 0) {
			switch (errno) {
			case EAGAIN:
				return;
			case EINTR:
				continue;
			default:
				perror("recvmsg");
				uloop_fd_delete(u);
				return;
			}
		}

		inet_ntop(AF_INET, &sin.sin_addr, addr_str, sizeof(addr_str));
		printf("RX: src:%s:%d, len=%d\n", addr_str, sin.sin_port, len);
		radius_parse(buf, (unsigned int)len, sin.sin_port, sock->type, 1);
	} while (1);
}

static struct radius_socket *
sock_open(char *port, enum socket_type type)
{
	struct radius_socket *sock = malloc(sizeof(*sock));

	if (!sock)
		return NULL;

	memset(sock, 0, sizeof(*sock));

	sock->fd.fd = usock(USOCK_UDP | USOCK_SERVER | USOCK_NONBLOCK |
			    USOCK_NUMERIC | USOCK_IPV4ONLY,
			    "127.0.0.1", port);
	if (sock->fd.fd < 0) {
                perror("usock");
		free(sock);
                return NULL;
        }

	sock->type = type;
	sock->fd.cb = sock_recv;

	uloop_fd_add(&sock->fd, ULOOP_READ);

	return sock;
}

int main(int argc, char **argv)
{
	ulog_open(ULOG_STDIO | ULOG_SYSLOG, LOG_DAEMON, "radius-gw-proxy");

	uloop_init();

	ubus_init();

	sock_auth = sock_open("1812", RADIUS_AUTH);
	sock_acct = sock_open("1813", RADIUS_ACCT);
	sock_dae = sock_open("3379", RADIUS_DAS);

	uloop_run();
	uloop_end();
	ubus_deinit();

	return 0;
}
