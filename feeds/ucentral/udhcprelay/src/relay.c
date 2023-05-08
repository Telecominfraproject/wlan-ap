#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <endian.h>
#include <libubox/uloop.h>
#include <libubox/usock.h>
#include <libubox/avl.h>
#include <libubox/avl-cmp.h>
#include <libubox/utils.h>
#include <libubus.h>
#include "dhcprelay.h"
#include "msg.h"

struct dhcprelay_req_key {
	uint32_t xid;
	uint8_t addr[6];
};

struct dhcprelay_req {
	struct avl_node node;
	struct uloop_timeout timeout;
	struct dhcprelay_req_key key;

	struct packet_l2 l2;
};

struct dhcprelay_local {
	struct avl_node node;
	struct in_addr addr;
	struct uloop_fd fd;

	struct list_head conn_list;
};

struct dhcprelay_conn {
	struct avl_node node;

	struct dhcprelay_local *local;
	struct list_head local_list;
	struct sockaddr_in local_addr;

	struct uloop_timeout timeout;
	struct uloop_fd fd;
};

static int relay_req_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, sizeof(struct dhcprelay_req_key));
}

static int in_addr_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, sizeof(struct in_addr));
}

static AVL_TREE(requests, relay_req_cmp, false, NULL);
static AVL_TREE(connections, avl_strcmp, false, NULL);
static AVL_TREE(local_addr, in_addr_cmp, false, NULL);

static void
dhcprelay_req_timeout_cb(struct uloop_timeout *t)
{
	struct dhcprelay_req *req = container_of(t, struct dhcprelay_req, timeout);

	avl_delete(&requests, &req->node);
	free(req);
}

static void
__dhcprelay_conn_free(struct dhcprelay_conn *conn)
{
	uloop_timeout_cancel(&conn->timeout);
	avl_delete(&connections, &conn->node);
	uloop_fd_delete(&conn->fd);
	close(conn->fd.fd);
	free(conn);
}

static void
dhcprelay_local_free(struct dhcprelay_local *local)
{
	struct dhcprelay_conn *conn;

	while (!list_empty(&local->conn_list)) {
		conn = list_first_entry(&local->conn_list, struct dhcprelay_conn, local_list);
		list_del_init(&conn->local_list);
		__dhcprelay_conn_free(conn);
	}

	avl_delete(&local_addr, &local->node);
	uloop_fd_delete(&local->fd);
	close(local->fd.fd);
	free(local);
}

static void
dhcprelay_conn_free(struct dhcprelay_conn *conn)
{
	struct dhcprelay_local *local = conn->local;

	if (local) {
		list_del_init(&conn->local_list);
		if (list_empty(&local->conn_list))
			dhcprelay_local_free(local);
	}

	__dhcprelay_conn_free(conn);
}

static void
dhcprelay_conn_timeout_cb(struct uloop_timeout *t)
{
	struct dhcprelay_conn *conn = container_of(t, struct dhcprelay_conn, timeout);

	dhcprelay_conn_free(conn);
}

static void
dhcprelay_req_key_init(struct dhcprelay_req_key *key, struct dhcpv4_message *msg)
{
	key->xid = msg->xid;
	memcpy(key->addr, msg->chaddr, sizeof(key->addr));
}

static struct dhcprelay_req *
dhcprelay_req_from_pkt(struct packet *pkt)
{
	struct dhcpv4_message *msg = pkt->data;
	struct dhcprelay_req_key key = {};
	struct dhcprelay_req *req;

	dhcprelay_req_key_init(&key, msg);
	req = avl_find_element(&requests, &key, req, node);
	if (req)
		goto out;

	req = calloc(1, sizeof(*req));
	memcpy(&req->key, &key, sizeof(key));
	req->node.key = &req->key;
	req->timeout.cb = dhcprelay_req_timeout_cb;
	avl_insert(&requests, &req->node);

out:
	req->l2 = pkt->l2;
	uloop_timeout_set(&req->timeout, 10 * 1000);

	return req;
}

static void
dhcprelay_forward_response(struct packet *pkt, struct dhcprelay_req *req,
			   struct dhcpv4_message *msg)
{
	uint16_t proto = ETH_P_IP;

	if (req->l2.vlan_proto) {
		struct vlan_hdr *vlan;

		vlan = pkt_push(pkt, sizeof(*vlan));
		if (!vlan)
			return;

		vlan->tci = cpu_to_be16(req->l2.vlan_tci);
		vlan->proto = cpu_to_be16(proto);
		proto = req->l2.vlan_proto;
	}

	dhcprelay_dev_send(pkt, req->l2.ifindex, msg->chaddr, proto);
}

static inline uint32_t
csum_tcpudp_nofold(uint32_t saddr, uint32_t daddr, uint8_t proto, uint32_t len)
{
	uint64_t sum = 0;

	sum += saddr;
	sum += daddr;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	sum += (proto + len) << 8;
#else
	sum += proto + len;
#endif

	sum = (sum & 0xffffffff) + (sum >> 32);
	sum = (sum & 0xffffffff) + (sum >> 32);

	return (uint32_t)sum;
}

static inline uint32_t csum_add(uint32_t sum, uint32_t addend)
{
	sum += addend;
	return sum + (sum < addend);
}

static inline uint16_t csum_fold(uint32_t sum)
{
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return (uint16_t)~sum;
}

static uint32_t csum_partial(const void *buf, int len)
{
	const uint16_t *data = buf;
	uint32_t sum = 0;

	while (len > 1) {
		sum += *data++;
		len -= 2;
	}

	if (len == 1)
#if __BYTE_ORDER == __LITTLE_ENDIAN
		sum += *(uint8_t *)data;
#else
		sum += *(uint8_t *)data << 8;
#endif

	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return sum;
}

static int
dhcprelay_add_ip_udp(struct packet *pkt)
{
	struct dhcpv4_message *msg = pkt->data;
	struct udphdr *udp;
	struct ip *ip;
	uint16_t msg_len = pkt->len;
	uint16_t udp_len = sizeof(*udp) + msg_len;
	uint16_t ip_len = sizeof(*ip) + udp_len;
	uint32_t sum;

	udp = pkt_push(pkt, sizeof(*udp));
	ip = pkt_push(pkt, sizeof(*ip));
	if (!udp || !ip)
		return -1;

	ip->ip_v = 4;
	ip->ip_hl = sizeof(*ip) / 4;
	ip->ip_len = cpu_to_be16(ip_len);
	ip->ip_p = IPPROTO_UDP;
	ip->ip_src = msg->siaddr;
	if (msg->flags & cpu_to_be16((1 << 15)))
		ip->ip_dst = (struct in_addr){ ~0 };
	else
		ip->ip_dst = msg->yiaddr;

	udp->uh_sport = cpu_to_be16(67);
	udp->uh_dport = cpu_to_be16(68);
	udp->uh_ulen = cpu_to_be16(udp_len);

	udp->uh_sum = 0;
	udp->uh_ulen = cpu_to_be16(udp_len);
	sum = csum_tcpudp_nofold(*(uint32_t *)&ip->ip_src, *(uint32_t *)&ip->ip_dst,
				 ip->ip_p, udp_len);
	sum = csum_add(sum, csum_partial(udp, sizeof(*udp)));
	sum = csum_add(sum, csum_partial(msg, msg_len));
	udp->uh_sum = csum_fold(sum);

	ip->ip_sum = 0;
	ip->ip_sum = csum_fold(csum_partial(ip, sizeof(*ip)));

	return 0;
}

void dhcprelay_handle_response(struct packet *pkt)
{
	struct dhcpv4_message *msg = pkt->data;
	struct dhcprelay_req_key key = {};
	struct dhcprelay_req *req;

	if (pkt->len < sizeof(*msg))
		return;

	dhcprelay_req_key_init(&key, msg);
	req = avl_find_element(&requests, &key, req, node);
	if (!req)
		return;

	if (dhcprelay_add_ip_udp(pkt))
		return;

	dhcprelay_forward_response(pkt, req, msg);
}

static int
dhcprelay_read_relay_fd(int fd)
{
	static struct {
		uint8_t l2[sizeof(struct ethhdr) + 4];
		struct ip ip;
		struct udphdr udp;
		uint8_t data[1500];
	} __packed buf = {};
	struct packet pkt = {
		.head = &buf,
		.data = buf.data,
		.end = &buf.data[sizeof(buf.data)],
	};
	ssize_t len;

retry:
	len = recv(fd, pkt.data, sizeof(pkt.data), 0);
	if (len < 0) {
		if (errno == EINTR)
			goto retry;

		if (errno == EAGAIN)
			return 0;

		return -1;
	}

	pkt.len = len;
	dhcprelay_handle_response(&pkt);

	return 0;
}

static void
dhcprelay_local_cb(struct uloop_fd *fd, unsigned int events)
{
	struct dhcprelay_local *local = container_of(fd, struct dhcprelay_local, fd);

	if (dhcprelay_read_relay_fd(fd->fd) < 0)
		dhcprelay_local_free(local);
}

static void
dhcprelay_conn_cb(struct uloop_fd *fd, unsigned int events)
{
	struct dhcprelay_conn *conn = container_of(fd, struct dhcprelay_conn, fd);

	uloop_timeout_set(&conn->timeout, 30 * 1000);

	if (dhcprelay_read_relay_fd(fd->fd) < 0)
		dhcprelay_conn_free(conn);
}

static struct dhcprelay_local *
dhcprelay_local_get(struct in_addr *addr)
{
	struct dhcprelay_local *local;
	struct sockaddr_in sin = {
		.sin_addr = *addr,
		.sin_port = cpu_to_be16(67),
	};
	const int yes = 1;
	int fd;

	local = avl_find_element(&local_addr, addr, local, node);
	if (local)
		return local;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return NULL;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		close(fd);
		return NULL;
	}

	local = calloc(1, sizeof(*local));
	local->fd.fd = fd;
	local->fd.cb = dhcprelay_local_cb;
	uloop_fd_add(&local->fd, ULOOP_READ);
	INIT_LIST_HEAD(&local->conn_list);
	local->node.key = &local->addr;
	memcpy(&local->addr, addr, sizeof(local->addr));
	avl_insert(&local_addr, &local->node);

	return local;
}

static struct dhcprelay_conn *
dhcprelay_conn_get(const char *addr)
{
	struct dhcprelay_conn *conn;
	const char *host = addr;
	const char *port = "67";
	const char *sep;
	char *name_buf;
	socklen_t sl = sizeof(local_addr);
	int fd;

	conn = avl_find_element(&connections, addr, conn, node);
	if (conn)
		goto out;

	sep = strchr(addr, ':');
	if (sep) {
		char *buf = alloca(strlen(addr) + 1);
		int ofs = sep - addr;

		buf[ofs++] = 0;
		host = buf;
		port = buf + ofs;
	}

	fd = usock(USOCK_UDP | USOCK_IPV4ONLY | USOCK_NONBLOCK, host, port);
	if (fd < 0)
		return NULL;

	conn = calloc_a(sizeof(*conn), &name_buf, strlen(addr) + 1);
	conn->fd.fd = fd;
	conn->fd.cb = dhcprelay_conn_cb;
	conn->timeout.cb = dhcprelay_conn_timeout_cb;
	uloop_fd_add(&conn->fd, ULOOP_READ);
	INIT_LIST_HEAD(&conn->local_list);
	conn->node.key = strcpy(name_buf, addr);
	getsockname(fd, (struct sockaddr *)&conn->local_addr, &sl);
	conn->local = dhcprelay_local_get(&conn->local_addr.sin_addr);
	if (conn->local)
		list_add_tail(&conn->local_list, &conn->local->conn_list);

	avl_insert(&connections, &conn->node);

out:
	uloop_timeout_set(&conn->timeout, 30 * 1000);

	return conn;
}

int dhcprelay_forward_request(struct packet *pkt, struct blob_attr *data)
{
	enum {
		FWD_ATTR_ADDRESS,
		FWD_ATTR_OPTIONS,
		__FWD_ATTR_MAX,
	};
	static const struct blobmsg_policy policy[] = {
		[FWD_ATTR_ADDRESS] = { "address", BLOBMSG_TYPE_STRING },
		[FWD_ATTR_OPTIONS] = { "options", BLOBMSG_TYPE_ARRAY },
	};
	struct dhcpv4_message *msg = pkt->data;
	struct blob_attr *tb[__FWD_ATTR_MAX];
	struct dhcprelay_conn *conn;
	struct packet cur_pkt = *pkt;
	int ret;

	blobmsg_parse(policy, __FWD_ATTR_MAX, tb, blobmsg_data(data), blobmsg_len(data));

	if (!tb[FWD_ATTR_ADDRESS])
		return UBUS_STATUS_INVALID_ARGUMENT;

	pkt = &cur_pkt;
	if (dhcprelay_add_options(pkt, tb[FWD_ATTR_OPTIONS]))
		return UBUS_STATUS_INVALID_ARGUMENT;

	conn = dhcprelay_conn_get(blobmsg_get_string(tb[FWD_ATTR_ADDRESS]));
	if (!conn)
		return UBUS_STATUS_CONNECTION_FAILED;

	msg->giaddr = conn->local_addr.sin_addr;
	if (msg->hops++ >= 20)
		return 0;

	dhcprelay_req_from_pkt(pkt);
	do {
		ret = send(conn->fd.fd, pkt->data, pkt->len, 0);
	} while (ret < 0 && errno == EINTR);

	return 0;
}
