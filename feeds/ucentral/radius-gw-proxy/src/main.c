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

#include <libubox/avl.h>
#include <libubox/avl-cmp.h>

#include <libubus.h>

#define RAD_PROX_BUFLEN		(4 * 1024)

#define TLV_STATION_ID		30
#define TLV_VENDOR		26
#define TLV_TIP_SERVER_V4	2

#define TIP_VENDOR		58888

enum socket_type {
	RADIUS_AUTH = 0,
	RADIUS_ACCT,
	RADIUS_DAS
};

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

struct radius_station_id {
	char id[256];
	enum socket_type type;
};

struct radius_station {
	struct avl_node avl;
	struct radius_station_id key;
	int port;
};

static struct ubus_auto_conn conn;
static uint32_t ucentral;
static struct blob_buf b;

static int
avl_memcmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, sizeof(struct radius_station_id));
}

static AVL_TREE(radius_stations, avl_memcmp, false, NULL);

static void
radius_station_add(char *id, int port, enum socket_type type)
{
	struct radius_station *station;
	struct radius_station_id key = { .type = type };

	strcpy(key.id, id);
	station = avl_find_element(&radius_stations, &key, station, avl);

	if (!station) {
		printf("new station/port, adding to avl tree\n");
		station = malloc(sizeof(*station));
		memset(station, 0, sizeof(*station));
		strcpy(station->key.id, id);
		station->key.type = type;
		station->avl.key = &station->key;
		avl_insert(&radius_stations, &station->avl);
	}
	station->port = port;
}


static char *
b64(char *src, int len)
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

static void
radius_forward(char *buf, char *server, enum socket_type type)
{
	struct radius_header *hdr = (struct radius_header *) buf;
	struct ubus_request async = { };
	char *data = b64(buf, hdr->len);

	if (!data)
		return;

	blob_buf_init(&b, 0);
	switch (type) {
	case RADIUS_AUTH:
		blobmsg_add_string(&b, "radius", "auth");
		break;
	case RADIUS_ACCT:
		blobmsg_add_string(&b, "radius", "acct");
		break;
	default:
		return;
	}

	blobmsg_add_string(&b, "data", data);
	blobmsg_add_string(&b, "dst", server);

	ubus_invoke_async(&conn.ctx, ucentral, "radius", b.head, &async);
	ubus_abort_request(&conn.ctx, &async);

	free(data);
}

static int
radius_parse(char *buf, int len, int port, enum socket_type type)
{
	struct radius_header *hdr = (struct radius_header *) buf;
	struct radius_tlv *station_id = NULL;
	char station_id_str[256] = {};
	char server_ip_str[256] = {};
	void *avp = hdr->avp;

	hdr->len = ntohs(hdr->len);

	if (hdr->len != len) {
		printf("invalid header length\n");
		return -1;
	}

	printf("\tcode:%d, id:%d, len:%d\n", hdr->code, hdr->id, hdr->len);

	len -= sizeof(*hdr);

	while (len > 0) {
		struct radius_tlv *tlv = (struct radius_tlv *)avp;

		if (len < tlv->len) {
			printf("invalid TLV length\n");
			return -1;
		}

		if (tlv->id == TLV_STATION_ID)
			station_id = tlv;
		if (tlv->id == TLV_VENDOR && ntohl(*(uint32_t *) tlv->data) == TIP_VENDOR) {
			struct radius_tlv *vendor = (struct radius_tlv *) &tlv->data[6];

			if (vendor->id == TLV_TIP_SERVER_V4)
				strncpy(server_ip_str, vendor->data, vendor->len - 2);
		}

		printf("\tID:%d, len:%d\n", tlv->id, tlv->len);
		avp += tlv->len;
		len -= tlv->len;
	}

	if (!station_id) {
		printf("no station ID found\n");
		return -1;
	}
	if (!*server_ip_str) {
		printf("no server ip found\n");
		return -1;
	}
	memcpy(station_id_str, station_id->data, station_id->len - 2);
	printf("\tcalling station id:%s, server ip:%s\n", station_id_str, server_ip_str);
	radius_station_add(station_id_str, port, type);

	radius_forward(buf, server_ip_str, type);

	return 0;
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
	struct cmsghdr *cmsg;
	struct radius_socket *sock = container_of(u, struct radius_socket, fd);
	int len;

	do {
		struct in_pktinfo *pkti = NULL;

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

		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			if (cmsg->cmsg_type != IP_PKTINFO)
				continue;

			pkti = (struct in_pktinfo *) CMSG_DATA(cmsg);
		}

		if (!pkti) {
			printf("Received packet without ifindex\n");
			continue;
		}

		inet_ntop(AF_INET, &sin.sin_addr, addr_str, sizeof(addr_str));
		printf("RX: src:%s:%d, len=%d\n", addr_str, sin.sin_port, len);
		radius_parse(buf, len, sin.sin_port, sock->type);
	} while (1);
}

static struct radius_socket *
sock_open(char *port, enum socket_type type)
{
	struct radius_socket *sock = malloc(sizeof(*sock));
	int yes = 1;

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
        if (setsockopt(sock->fd.fd, IPPROTO_IP, IP_PKTINFO, &yes, sizeof(yes)) < 0)
		perror("setsockopt(IP_PKTINFO)");

	sock->type = type;
	sock->fd.cb = sock_recv;

	uloop_fd_add(&sock->fd, ULOOP_READ);

	return sock;
}

static void
ubus_event_handler_cb(struct ubus_context *ctx,  struct ubus_event_handler *ev,
		      const char *type, struct blob_attr *msg)
{
	enum {
		EVENT_ID,
		EVENT_PATH,
		__EVENT_MAX
	};

	static const struct blobmsg_policy status_policy[__EVENT_MAX] = {
		[EVENT_ID] = { .name = "id", .type = BLOBMSG_TYPE_INT32 },
		[EVENT_PATH] = { .name = "path", .type = BLOBMSG_TYPE_STRING },
	};

	struct blob_attr *tb[__EVENT_MAX];
	char *path;
	uint32_t id;

	blobmsg_parse(status_policy, __EVENT_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[EVENT_ID] || !tb[EVENT_PATH])
		return;

	path = blobmsg_get_string(tb[EVENT_PATH]);
	id = blobmsg_get_u32(tb[EVENT_ID]);

	if (strcmp(path, "ucentral"))
		return;
	if (!strcmp("ubus.object.remove", type))
		ucentral = 0;
	else
		ucentral = id;
}

static struct ubus_event_handler ubus_event_handler = { .cb = ubus_event_handler_cb };

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	ubus_register_event_handler(ctx, &ubus_event_handler, "ubus.object.add");
	ubus_register_event_handler(ctx, &ubus_event_handler, "ubus.object.remove");

	ubus_lookup_id(ctx, "ucentral", &ucentral);
}

int main(int argc, char **argv)
{

	uloop_init();

	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);

	sock_open("1812", RADIUS_AUTH);
	sock_open("1813", RADIUS_ACCT);

	uloop_run();
	uloop_end();

	return 0;
}
