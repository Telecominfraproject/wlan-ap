/* SPDX-License-Identifier: BSD-3-Clause */

/* code is derived from hapd proxy_arp snooping */

#include "dns.h"

 /* sudo tcpdump -s 3000 -dd greater 96 and '(ip or ip6)' and '(udp or tcp)' and '(port 53)' */
static struct sock_filter dns_sock_filter_insns[] = {
	{ 0x80, 0, 0, 0x00000000 },
	{ 0x35, 0, 19, 0x00000060 },
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 0, 9, 0x00000800 },
	{ 0x30, 0, 0, 0x00000017 },
	{ 0x15, 0, 15, 0x00000011 },
	{ 0x28, 0, 0, 0x00000014 },
	{ 0x45, 13, 0, 0x00001fff },
	{ 0xb1, 0, 0, 0x0000000e },
	{ 0x48, 0, 0, 0x0000000e },
	{ 0x15, 9, 0, 0x00000035 },
	{ 0x48, 0, 0, 0x00000010 },
	{ 0x15, 7, 8, 0x00000035 },
	{ 0x15, 0, 7, 0x000086dd },
	{ 0x30, 0, 0, 0x00000014 },
	{ 0x15, 0, 5, 0x00000011 },
	{ 0x28, 0, 0, 0x00000036 },
	{ 0x15, 2, 0, 0x00000035 },
	{ 0x28, 0, 0, 0x00000038 },
	{ 0x15, 0, 1, 0x00000035 },
	{ 0x6, 0, 0, 0x00000bb8 },
	{ 0x6, 0, 0, 0x00000000 },
};

static const struct sock_fprog sock_filter = {
	.len = ARRAY_SIZE(dns_sock_filter_insns),
	.filter = dns_sock_filter_insns,
};

static struct ubus_auto_conn conn;
static struct uloop_fd fd;
static struct blob_buf b;
static char *ifname;
static int qosify;

static struct ubus_object_type ubus_object_type = {
	.name = "dnssnoop"
};

static void ubus_state_handler(struct ubus_context *ctx, struct ubus_object *obj)
{
}

struct ubus_object ubus_object = {
	.name = "dnssnoop",
	.type = &ubus_object_type,
	.subscribe_cb = ubus_state_handler,
};


static int
proto_is_vlan(uint16_t h_proto)
{
	return !!(h_proto == ETH_P_8021Q ||
		  h_proto == ETH_P_8021AD);
}

static int
consume_buffer(uint8_t **buf, int *len, int size)
{
	if (size > *len)
		return -1;

	*buf += size;
	*len -= size;

	return 0;
}

static void
packet_handle(uint8_t *buf, int len)
{
	struct ethhdr *eth = (struct ethhdr *)buf;
	uint16_t h_proto;

	if (consume_buffer(&buf, &len, sizeof(*eth)))
		return;

	h_proto = eth->h_proto;

	if (proto_is_vlan(ntohs(h_proto))) {
		struct vlan_hdr *vlanh = (struct vlan_hdr *)buf;

		if (consume_buffer(&buf, &len, sizeof(struct vlan_hdr)))
			return;

		h_proto = vlanh->h_vlan_encapsulated_proto;
	}

	switch (ntohs(eth->h_proto)) {
	case ETH_P_IP:
		if (consume_buffer(&buf, &len, sizeof(struct ip)))
			return;
		break;
	case ETH_P_IPV6:
		if (consume_buffer(&buf, &len, sizeof(struct ip6_hdr)))
			return;
		break;
	default:
		return;
	}

	if (consume_buffer(&buf, &len, sizeof(struct udphdr)))
		return;

	dns_handle_packet(buf, len);
}

static void
socket_fd_cb(struct uloop_fd *fd, unsigned int events)
{
	uint8_t buf[8192];

	do {
		int len = recvfrom(fd->fd, buf, sizeof(buf), MSG_DONTWAIT, NULL, NULL);

		if (len <= 0) {
			switch (errno) {
			case EINTR:
			case EAGAIN:
				return;
			default:
				exit(1);
			}
			break;
		}
		packet_handle(buf, len);
	} while (true);
}

static int
socket_open(void)
{
	int sock;

	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock == -1) {
		ULOG_ERR("failed to open socket on %s\n", ifname);
		return -1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname))) {
		ULOG_ERR("failed to bind socket to %s\n", ifname);
		close(sock);
		return -1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER,
		       &sock_filter, sizeof(struct sock_fprog))) {
		ULOG_ERR("failed to attach filter to %s\n", ifname);
		close(sock);
		return -1;
	}

	return sock;
}

static void
snoop_start(void)
{
	int sock = socket_open();
	if (sock == -1) {
		ULOG_ERR("failed to open socket on %s\n", ifname);
		return;
	}
	fd.cb = socket_fd_cb;
	fd.fd = sock;
	uloop_fd_add(&fd, ULOOP_READ);
}

void
ubus_notify_qosify(char *name, char *address, int type, int ttl)
{
	if (!qosify)
		return;

	blob_buf_init(&b, 0);
	switch (type) {
	case TYPE_AAAA:
		blobmsg_add_string(&b, "type", "AAAA");
		break;
	case TYPE_A:
		blobmsg_add_string(&b, "type", "A");
		break;
	default:
		return;
	}
	blobmsg_add_string(&b, "name", name);
	blobmsg_add_string(&b, "address", address);
        blobmsg_add_u32(&b, "ttl", ttl);

	ubus_invoke(&conn.ctx, qosify, "add_dns_host", b.head, NULL, NULL, 200);
}

static void
ubus_handle_status(struct ubus_context *ctx,  struct ubus_event_handler *ev,
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
	uint32_t id;
	char *path;

	blobmsg_parse(status_policy, __EVENT_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[EVENT_ID] || !tb[EVENT_PATH])
		return;

	path = blobmsg_get_string(tb[EVENT_PATH]);
	id = blobmsg_get_u32(tb[EVENT_ID]);

	if (strcmp(path, "qosify"))
		return;

	if (!strcmp("ubus.object.remove", type))
		qosify = 0;

	if (!strcmp("ubus.object.add", type))
		qosify = id;
}

static void
ubus_lookup_cb(struct ubus_context *ctx, struct ubus_object_data *obj,
                    void *priv)
{
	if (strcmp(obj->path, "qosify"))
		return;

	qosify = obj->id;
}

static struct ubus_event_handler ubus_status_handler = { .cb = ubus_handle_status };

static void
ubus_connect_handler(struct ubus_context *ctx)
{
        ULOG_NOTE("connected to ubus\n");
	ubus_add_object(ctx, &ubus_object);

	ubus_register_event_handler(ctx, &ubus_status_handler, "ubus.object.add");
	ubus_register_event_handler(ctx, &ubus_status_handler, "ubus.object.remove");

	ubus_lookup(ctx, NULL, ubus_lookup_cb, NULL);
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		return -1;

	ifname = argv[1];

	ulog_open(ULOG_STDIO | ULOG_SYSLOG, LOG_DAEMON, "udnssnoop");

	uloop_init();

	conn.cb = ubus_connect_handler;
        ubus_auto_connect(&conn);
	snoop_start();
	uloop_run();
	uloop_done();

	return 0;
}
