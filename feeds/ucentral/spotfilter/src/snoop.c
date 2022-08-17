// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <resolv.h>

#include <libubox/uloop.h>

#include "spotfilter.h"

#define FLAG_RESPONSE		0x8000
#define FLAG_OPCODE		0x7800
#define FLAG_AUTHORATIVE	0x0400
#define FLAG_RCODE		0x000f

#define TYPE_A			0x0001
#define TYPE_CNAME		0x0005
#define TYPE_PTR		0x000c
#define TYPE_TXT		0x0010
#define TYPE_AAAA		0x001c
#define TYPE_SRV		0x0021
#define TYPE_ANY		0x00ff

#define IS_COMPRESSED(x)	((x & 0xc0) == 0xc0)

#define CLASS_FLUSH		0x8000
#define CLASS_UNICAST		0x8000
#define CLASS_IN		0x0001

#define MAX_NAME_LEN            256
#define MAX_DATA_LEN            8096

int spotfilter_ifb_ifindex;
static struct uloop_fd ufd;
static struct uloop_timeout cname_gc_timer;

struct vlan_hdr {
	uint16_t tci;
	uint16_t proto;
};

struct packet {
	void *head;
	void *buffer;
	unsigned int len;
};

struct dns_header {
	uint16_t id;
	uint16_t flags;
	uint16_t questions;
	uint16_t answers;
	uint16_t authority;
	uint16_t additional;
} __packed;

struct dns_question {
	uint16_t type;
	uint16_t class;
} __packed;

struct dns_answer {
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdlength;
} __packed;

struct addr_entry_data {
	union {
		struct {
			uint32_t _pad;
			uint32_t ip4addr;
		};
		uint32_t ip6addr[4];
	};
	uint32_t timeout;
};

struct addr_entry {
	struct avl_node node;
	struct addr_entry_data data;
};

struct cname_entry {
	struct avl_node node;
	uint8_t class;
	uint8_t age;
};

static uint32_t spotfilter_gettime(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec;
}


static void *
pkt_peek(struct packet *pkt, unsigned int len)
{
	if (len > pkt->len)
		return NULL;

	return pkt->buffer;
}


static void *
pkt_pull(struct packet *pkt, unsigned int len)
{
	void *ret = pkt_peek(pkt, len);

	if (!ret)
		return NULL;

	pkt->buffer += len;
	pkt->len -= len;

	return ret;
}

static bool
proto_is_vlan(uint16_t proto)
{
	return proto == ETH_P_8021Q || proto == ETH_P_8021AD;
}

static int pkt_pull_name(struct packet *pkt, const void *hdr, char *dest)
{
	int len;

	if (dest)
		len = dn_expand(hdr, pkt->buffer + pkt->len, pkt->buffer,
				(void *)dest, MAX_NAME_LEN);
	else
		len = dn_skipname(pkt->buffer, pkt->buffer + pkt->len - 1);

	if (len < 0 || !pkt_pull(pkt, len))
		return -1;

	return 0;
}

static void
cname_cache_set(struct interface *iface, const char *name, int class)
{
	struct cname_entry *e;

	if (class < 0)
		return;

	e = avl_find_element(&iface->cname_cache, name, e, node);
	if (!e) {
		char *name_buf;

		e = calloc_a(sizeof(*e), &name_buf, strlen(name) + 1);
		e->node.key = strcpy(name_buf, name);
		avl_insert(&iface->cname_cache, &e->node);
	}

	e->age = 0;
	e->class = (uint8_t)class;
}

static int
cname_cache_get(struct interface *iface, const char *name, int *class)
{
	struct cname_entry *e;

	e = avl_find_element(&iface->cname_cache, name, e, node);
	if (!e)
		return -1;

	if (*class < 0)
		*class = e->class;

	return 0;
}

static bool
__spotfilter_dns_whitelist_lookup(struct blob_attr *attr, const char *name, int *class)
{
	enum {
		WL_ATTR_CLASS,
		WL_ATTR_HOSTS,
		__WL_ATTR_MAX
	};
	static const struct blobmsg_policy policy[__WL_ATTR_MAX] = {
		[WL_ATTR_CLASS] = { "class", BLOBMSG_TYPE_INT32 },
		[WL_ATTR_HOSTS] = { "hosts", BLOBMSG_TYPE_ARRAY },
	};
	struct blob_attr *tb[__WL_ATTR_MAX];
	struct blob_attr *cur;
	int rem;

	blobmsg_parse(policy, __WL_ATTR_MAX, tb, blobmsg_data(attr), blobmsg_len(attr));

	if (!tb[WL_ATTR_CLASS] || !tb[WL_ATTR_HOSTS])
		return false;

	blobmsg_for_each_attr(cur, tb[WL_ATTR_HOSTS], rem) {
		if (fnmatch(blobmsg_get_string(cur), name, 0))
			continue;

		*class = blobmsg_get_u32(tb[WL_ATTR_CLASS]);
		return true;
	}

	return false;
}

static void
spotfilter_dns_whitelist_lookup(struct interface *iface, const char *name, int *class)
{
	struct blob_attr *cur;
	int rem;

	if (!iface->whitelist)
		return;

	blobmsg_for_each_attr(cur, iface->whitelist, rem) {
		if (__spotfilter_dns_whitelist_lookup(cur, name, class))
			return;
	}
}

static void
spotfilter_dns_whitelist_map_add(struct interface *iface, const struct addr_entry_data *data,
				 bool ipv6, int class)
{
	struct addr_entry *e;
	uint8_t val = (uint8_t)class;
	int32_t delta;

	if (class < 0)
		return;

	e = avl_find_element(&iface->addr_map, data, e, node);
	if (!e) {
		e = calloc(1, sizeof(*e));
		memcpy(&e->data, data, sizeof(e->data));
		e->node.key = &e->data;
		avl_insert(&iface->addr_map, &e->node);
	}

	spotfilter_bpf_set_whitelist(iface, ipv6 ? data->ip6addr : &data->ip4addr, ipv6, &val);
	e->data.timeout = spotfilter_gettime() + data->timeout;

	delta = e->data.timeout - iface->next_gc;
	if (iface->next_gc && delta < 0)
		uloop_timeout_set(&iface->addr_gc, data->timeout);
}

static int
dns_parse_question(struct interface *iface, struct packet *pkt, const void *hdr, int *class)
{
	char qname[MAX_NAME_LEN];

	if (pkt_pull_name(pkt, hdr, qname) ||
	    !pkt_pull(pkt, sizeof(struct dns_question)))
		return -1;

	cname_cache_get(iface, qname, class);
	spotfilter_dns_whitelist_lookup(iface, qname, class);

	return 0;
}

static int
dns_parse_answer(struct interface *iface, struct packet *pkt, void *hdr, int *class)
{
	char cname[MAX_NAME_LEN];
	struct dns_answer *a;
	struct addr_entry_data data = {};
	bool ipv6 = false;
	void *rdata;
	int len;

	if (pkt_pull_name(pkt, hdr, NULL))
		return -1;

	a = pkt_pull(pkt, sizeof(*a));
	if (!a)
		return -1;

	len = be16_to_cpu(a->rdlength);
	rdata = pkt_pull(pkt, len);
	if (!rdata)
		return -1;

	switch (be16_to_cpu(a->type)) {
	case TYPE_CNAME:
		if (dn_expand(hdr, pkt->buffer + pkt->len, rdata,
			      cname, sizeof(cname)) < 0)
			return -1;

		spotfilter_dns_whitelist_lookup(iface, cname, class);
		cname_cache_set(iface, cname, *class);
		return 0;
	case TYPE_A:
		memcpy(&data.ip4addr, rdata, 4);
		if (!data.ip4addr)
			return 0;
		break;
	case TYPE_AAAA:
		ipv6 = true;
		memcpy(&data.ip6addr, rdata, 16);
		if (!data.ip6addr[0])
			return 0;
		break;
	default:
		return 0;
	}

	if (class < 0)
	    return 0;

	data.timeout = be32_to_cpu(a->ttl);
	spotfilter_dns_whitelist_map_add(iface, &data, ipv6, *class);

	return 0;
}

static void
spotfilter_dns_iface_recv(struct interface *iface, struct packet *pkt)
{
	struct dns_header *h;
	int class = -1;
	int i;

	h = pkt_pull(pkt, sizeof(*h));
	if (!h)
		return;

	if ((h->flags & cpu_to_be16(FLAG_RESPONSE | FLAG_OPCODE | FLAG_RCODE)) !=
	    cpu_to_be16(FLAG_RESPONSE))
		return;

	if (h->questions != cpu_to_be16(1))
		return;

	if (dns_parse_question(iface, pkt, h, &class))
		return;

	for (i = 0; i < be16_to_cpu(h->answers); i++)
		if (dns_parse_answer(iface, pkt, h, &class))
			return;
}

static void
spotfilter_dns_recv(struct packet *pkt)
{
	struct interface *iface;

	avl_for_each_element(&interfaces, iface, node) {
		struct packet tmp_pkt = *pkt;

		spotfilter_dns_iface_recv(iface, &tmp_pkt);
	}
}

static void
spotfilter_parse_udp_v4(struct packet *pkt, uint16_t src_port, uint16_t dst_port)
{
	struct ethhdr *eth = pkt->head;

	if (src_port != 67 || dst_port != 68)
		return;

	spotfilter_recv_dhcpv4(pkt->buffer, pkt->len, eth->h_dest);
}

static void
spotfilter_packet_cb(struct packet *pkt)
{
	uint16_t proto, src_port, dst_port;
	struct ethhdr *eth;
	struct ip6_hdr *ip6;
	struct ip *ip;
	struct udphdr *udp;
	bool ipv4;

	eth = pkt_pull(pkt, sizeof(*eth));
	if (!eth)
		return;

	proto = be16_to_cpu(eth->h_proto);
	if (proto_is_vlan(proto)) {
		struct vlan_hdr *vlan;

		vlan = pkt_pull(pkt, sizeof(*vlan));
		if (!vlan)
			return;

		proto = be16_to_cpu(vlan->proto);
	}

	switch (proto) {
	case ETH_P_IP:
		ip = pkt_peek(pkt, sizeof(struct ip));
		if (!ip)
			return;

		if (!pkt_pull(pkt, ip->ip_hl * 4))
			return;

		proto = ip->ip_p;
		ipv4 = true;
		break;
	case ETH_P_IPV6:
		ip6 = pkt_pull(pkt, sizeof(*ip6));
		if (!ip6)
			return;

		proto = ip6->ip6_nxt;
		if (proto == IPPROTO_ICMPV6) {
			if (ip6->ip6_hlim != 255)
				return;

			spotfilter_recv_icmpv6(pkt->buffer, pkt->len, eth->h_source, eth->h_dest);
			return;
		}
		break;
	default:
		return;
	}

	if (proto != IPPROTO_UDP)
		return;

	udp = pkt_pull(pkt, sizeof(struct udphdr));
	if (!udp)
		return;

	src_port = ntohs(udp->uh_sport);
	dst_port = ntohs(udp->uh_dport);

	if (ipv4)
		spotfilter_parse_udp_v4(pkt, src_port, dst_port);

	if (src_port == 53)
		spotfilter_dns_recv(pkt);
}

static void
spotfilter_socket_cb(struct uloop_fd *fd, unsigned int events)
{
	static uint8_t buf[8192];
	struct packet pkt = {
		.head = buf,
		.buffer = buf,
	};
	int len;

retry:
	len = recvfrom(fd->fd, buf, sizeof(buf), MSG_DONTWAIT, NULL, NULL);
	if (len < 0) {
		if (errno == EINTR)
			goto retry;
		return;
	}

	if (!len)
		return;

	pkt.len = len;
	spotfilter_packet_cb(&pkt);
}

static int
spotfilter_open_socket(void)
{
	struct sockaddr_ll sll = {
		.sll_family = AF_PACKET,
		.sll_protocol = htons(ETH_P_ALL),
	};
	int sock;

	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock == -1) {
		ULOG_ERR("failed to create raw socket: %s\n", strerror(errno));
		return -1;
	}

	sll.sll_ifindex = if_nametoindex(SPOTFILTER_IFB_NAME);
	if (bind(sock, (struct sockaddr *)&sll, sizeof(sll))) {
		ULOG_ERR("failed to bind socket to "SPOTFILTER_IFB_NAME": %s\n",
			 strerror(errno));
		goto error;
	}

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

	ufd.fd = sock;
	ufd.cb = spotfilter_socket_cb;
	uloop_fd_add(&ufd, ULOOP_READ);

	return 0;

error:
	close(sock);
	return -1;
}


static void
spotfilter_addr_gc(struct uloop_timeout *t)
{
	struct interface *iface = container_of(t, struct interface, addr_gc);
	struct addr_entry *e, *tmp;
	uint32_t now = spotfilter_gettime();
	int32_t timeout = 0;

	iface->next_gc = 0;
	avl_for_each_element_safe(&iface->addr_map, e, node, tmp) {
		const void *addr = e->data.ip6addr[0] ? &e->data.ip6addr[0] : &e->data.ip4addr;
		bool ipv6 = !!e->data.ip6addr[0];
		int32_t cur_timeout;

		cur_timeout = e->data.timeout - now;
		if (cur_timeout <= 0) {
			if (!spotfilter_bpf_whitelist_seen(iface, addr, ipv6)) {
				spotfilter_bpf_set_whitelist(iface, addr, ipv6, NULL);
				avl_delete(&iface->addr_map, &e->node);
				free(e);
				continue;
			}

			e->data.timeout = now + iface->active_timeout;
		}

		if (!timeout || cur_timeout < timeout) {
			timeout = cur_timeout;
			iface->next_gc = e->data.timeout;
		}
	}

	if (!timeout)
		return;

	uloop_timeout_set(&iface->addr_gc, timeout * 1000);
}

static void
spotfilter_cname_cache_gc(struct uloop_timeout *timeout)
{
	struct interface *iface;
	struct cname_entry *e, *tmp;

	avl_for_each_element(&interfaces, iface, node) {
		avl_for_each_element_safe(&iface->cname_cache, e, node, tmp) {
			if (e->age++ < 5)
				continue;

			avl_delete(&iface->cname_cache, &e->node);
			free(e);
		}
	}

	uloop_timeout_set(timeout, 1000);
}

static int avl_addr_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, 16);
}


void spotfilter_dns_init(struct interface *iface)
{
	avl_init(&iface->cname_cache, avl_strcmp, false, NULL);
	avl_init(&iface->addr_map, avl_addr_cmp, false, NULL);
	iface->addr_gc.cb = spotfilter_addr_gc;
}

void spotfilter_dns_free(struct interface *iface)
{
	struct cname_entry *e, *tmp;

	avl_remove_all_elements(&iface->cname_cache, e, node, tmp)
		free(e);
}

int spotfilter_dev_init(void)
{
	cname_gc_timer.cb = spotfilter_cname_cache_gc;
	spotfilter_cname_cache_gc(&cname_gc_timer);

	spotfilter_dev_done();

	if (spotfilter_run_cmd("ip link add "SPOTFILTER_IFB_NAME" type ifb", false) ||
	    spotfilter_run_cmd("ip link set dev "SPOTFILTER_IFB_NAME" up", false) ||
	    spotfilter_open_socket())
		return -1;

	spotfilter_ifb_ifindex = if_nametoindex(SPOTFILTER_IFB_NAME);

	return 0;
}

void spotfilter_dev_done(void)
{
	if (ufd.registered) {
		uloop_fd_delete(&ufd);
		close(ufd.fd);
	}

	spotfilter_run_cmd("ip link del "SPOTFILTER_IFB_NAME, true);
}
