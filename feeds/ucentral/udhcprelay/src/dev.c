// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <netpacket/packet.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <libubox/vlist.h>
#include <libubox/avl-cmp.h>

#include "dhcprelay.h"

#define APPEND(_buf, _ofs, _format, ...) _ofs += snprintf(_buf + _ofs, sizeof(_buf) - _ofs, _format, ##__VA_ARGS__)

static void dev_update_cb(struct vlist_tree *tree, struct vlist_node *node_new,
			  struct vlist_node *node_old);

static struct uloop_fd ufd;
static VLIST_TREE(devices, avl_strcmp, dev_update_cb, true, false);
static AVL_TREE(bridges, avl_strcmp, false, NULL);
static struct blob_attr *devlist;

static void
dhcprelay_socket_cb(struct uloop_fd *fd, unsigned int events)
{
	static uint8_t buf[8192];
	struct packet pkt = {
		.head = buf,
		.data = buf + 32,
		.end = &buf[sizeof(buf)],
	};
	struct sockaddr_ll sll;
	socklen_t socklen = sizeof(sll);
	int len;

retry:
	len = recvfrom(fd->fd, pkt.data, pkt.end - pkt.data, MSG_DONTWAIT, (struct sockaddr *)&sll, &socklen);
	if (len < 0) {
		if (errno == EINTR)
			goto retry;
		return;
	}

	if (!len)
		return;

	pkt.l2.ifindex = ntohl(*(uint32_t *)(pkt.data + 2));
	pkt.len = len;
	dhcprelay_packet_cb(&pkt);
}

static int
dhcprelay_open_socket(void)
{
	struct sockaddr_ll sll = {
		.sll_family = AF_PACKET,
		.sll_protocol = htons(ETH_P_ALL),
	};
	int sock, yes = 1;

	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock == -1) {
		ULOG_ERR("failed to create raw socket: %s\n", strerror(errno));
		return -1;
	}

	setsockopt(sock, SOL_PACKET, PACKET_ORIGDEV, &yes, sizeof(yes));

	sll.sll_ifindex = if_nametoindex(DHCPRELAY_IFB_NAME);
	if (bind(sock, (struct sockaddr *)&sll, sizeof(sll))) {
		ULOG_ERR("failed to bind socket to "DHCPRELAY_IFB_NAME": %s\n",
			 strerror(errno));
		goto error;
	}

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

	ufd.fd = sock;
	ufd.cb = dhcprelay_socket_cb;
	uloop_fd_add(&ufd, ULOOP_READ);

	return 0;

error:
	close(sock);
	return -1;
}

static int
prepare_filter_cmd(char *buf, int len, const char *dev, int prio, bool add)
{
	return snprintf(buf, len, "tc filter %s dev '%s' ingress prio %d",
			add ? "add" : "del", dev, prio);
}

static void
dhcprelay_add_filter(struct device *dev, int prio, const char *match_string)
{
	const char *ifname = dev->ifname;
	int ifindex = dev->ifindex;
	char buf[350];
	int ofs;

	ofs = prepare_filter_cmd(buf, sizeof(buf), ifname, prio++, true);
	APPEND(buf, ofs,
	       " %s flowid 1:1", match_string);
	if (!dev->upstream)
		APPEND(buf, ofs,
		       " action pedit ex munge eth dst set ff:ff:%02x:%02x:%02x:%02x pipe",
		       (uint8_t)(ifindex >> 24),
		       (uint8_t)(ifindex >> 16),
		       (uint8_t)(ifindex >> 8),
		       (uint8_t)(ifindex));
	APPEND(buf, ofs,
	       " action mirred ingress %s dev " DHCPRELAY_IFB_NAME "%s",
		   dev->upstream ? "mirror" : "redirect",
		   dev->upstream ? " continue" : "");
	dhcprelay_run_cmd(buf, false);
}

static void
dhcprelay_dev_attach_filters(struct device *dev)
{
	int prio = DHCPRELAY_PRIO_BASE;

	dhcprelay_add_filter(dev, prio++,
			     "protocol ip u32 match ip dport 67 0xffff");
	dhcprelay_add_filter(dev, prio++,
			     "protocol 802.1Q u32 offset plus 4 match ip dport 67 0xffff");
}

static void
dhcprelay_dev_cleanup_filters(struct device *dev)
{
	char buf[128];
	int i;

	for (i = DHCPRELAY_PRIO_BASE; i < DHCPRELAY_PRIO_BASE + 2; i++) {
		prepare_filter_cmd(buf, sizeof(buf), dev->ifname, i, false);
		dhcprelay_run_cmd(buf, true);
	}
	dev->active = false;
}

static void
dhcprelay_dev_attach(struct device *dev)
{
	char buf[64];

	dev->active = true;
	snprintf(buf, sizeof(buf), "tc qdisc add dev '%s' clsact", dev->ifname);
	dhcprelay_run_cmd(buf, true);

	dhcprelay_dev_attach_filters(dev);
}

static void
__dhcprelay_dev_check(struct device *dev)
{
	int ifindex = if_nametoindex(dev->ifname);

	if (ifindex == dev->ifindex)
		return;

	dev->ifindex = ifindex;
	dhcprelay_dev_cleanup_filters(dev);
	if (ifindex)
		dhcprelay_dev_attach(dev);
}

static void dev_update_cb(struct vlist_tree *tree, struct vlist_node *node_new,
			  struct vlist_node *node_old)
{
	struct device *dev = NULL, *dev_free = NULL;

	if (node_old && node_new) {
		dev = container_of(node_old, struct device, node);
		dev_free = container_of(node_new, struct device, node);
		if (dev->upstream != dev_free->upstream) {
			dev->upstream = dev_free->upstream;
			dev->ifindex = 0;
			dhcprelay_dev_cleanup_filters(dev_free);
		}
	} else if (node_old) {
		dev_free = container_of(node_old, struct device, node);
		if (dev_free->active)
			dhcprelay_dev_cleanup_filters(dev_free);
	} else if (node_new) {
		dev = container_of(node_new, struct device, node);
	}

	if (dev)
		__dhcprelay_dev_check(dev);
	if (dev_free)
		free(dev_free);
}

void dhcprelay_dev_add(const char *name, bool upstream)
{
	struct device *dev;
	int len;

	dev = calloc(1, sizeof(*dev));
	len = snprintf(dev->ifname, sizeof(dev->ifname), "%s", name);
	if (!len || len > IFNAMSIZ) {
		free(dev);
		return;
	}
	dev->upstream = upstream;

	vlist_add(&devices, &dev->node, dev->ifname);
}

static struct device *
dhcprelay_dev_get_by_index(int ifindex)
{
	struct device *dev;

	vlist_for_each_element(&devices, dev, node) {
		if (dev->ifindex == ifindex)
			return dev;
	}

	return NULL;
}

void dhcprelay_dev_send(struct packet *pkt, int ifindex, const uint8_t *addr, uint16_t proto)
{
	struct sockaddr_ll sll = {
		.sll_family = AF_PACKET,
		.sll_protocol = cpu_to_be16(ETH_P_ALL),
		.sll_ifindex = ifindex,
	};
	struct ifreq ifr = {};
	struct ethhdr *eth;
	struct device *dev;
	int fd = ufd.fd;

	if (!ifindex)
		return;

	dev = dhcprelay_dev_get_by_index(ifindex);
	if (!dev)
		return;

	strncpy(ifr.ifr_name, dev->node.avl.key, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0 ||
	    ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
		return;

	eth = pkt_push(pkt, sizeof(*eth));
	if (!eth)
		return;

	memcpy(eth->h_source, ifr.ifr_hwaddr.sa_data, sizeof(eth->h_source));
	memcpy(eth->h_dest, addr, sizeof(eth->h_dest));
	eth->h_proto = cpu_to_be16(proto);
	sendto(fd, pkt->data, pkt->len, 0, (struct sockaddr *)&sll, sizeof(sll));
}

void dhcprelay_update_devices(void)
{
	struct bridge_entry *br;
	struct blob_attr *cur;
	int rem;

	vlist_update(&devices);

	avl_for_each_element(&bridges, br, node) {
		blobmsg_for_each_attr(cur, br->upstream, rem) {
			if (!blobmsg_check_attr(cur, false) ||
				blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
				continue;

			dhcprelay_dev_add(blobmsg_get_string(cur), true);
		}

		dhcprelay_ubus_query_bridge(br);
	}

	blobmsg_for_each_attr(cur, devlist, rem) {
		if (!blobmsg_check_attr(cur, false) ||
			blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
			continue;

		dhcprelay_dev_add(blobmsg_get_string(cur), false);
	}

	vlist_flush(&devices);
}

static void dhcprelay_bridge_add(struct blob_attr *data)
{
	enum {
		BRCONF_ATTR_VLANS,
		BRCONF_ATTR_IGNORE,
		BRCONF_ATTR_UPSTREAM,
		__BRCONF_ATTR_MAX,
	};
	static const struct blobmsg_policy policy[__BRCONF_ATTR_MAX] = {
		[BRCONF_ATTR_VLANS] = { "vlans", BLOBMSG_TYPE_ARRAY },
		[BRCONF_ATTR_IGNORE] = { "ignore", BLOBMSG_TYPE_ARRAY },
		[BRCONF_ATTR_UPSTREAM] = { "upstream", BLOBMSG_TYPE_ARRAY },
	};
	struct blob_attr *tb[__BRCONF_ATTR_MAX];
	struct blob_atttr *vlan_buf, *ignore_buf, *upstream_buf;
	size_t vlan_size = 0, ignore_size = 0, upstream_size = 0;
	struct bridge_entry *br;
	char *name_buf;

	blobmsg_parse(policy, __BRCONF_ATTR_MAX, tb, blobmsg_data(data), blobmsg_len(data));

	if (tb[BRCONF_ATTR_VLANS] &&
	    blobmsg_check_array(tb[BRCONF_ATTR_VLANS], BLOBMSG_TYPE_UNSPEC) > 0)
		vlan_size = blob_pad_len(tb[BRCONF_ATTR_VLANS]);

	if (tb[BRCONF_ATTR_IGNORE] &&
	    blobmsg_check_array(tb[BRCONF_ATTR_IGNORE], BLOBMSG_TYPE_STRING) > 0)
		ignore_size = blob_pad_len(tb[BRCONF_ATTR_IGNORE]);

	if (tb[BRCONF_ATTR_UPSTREAM] &&
	    blobmsg_check_array(tb[BRCONF_ATTR_UPSTREAM], BLOBMSG_TYPE_STRING) > 0)
		upstream_size = blob_pad_len(tb[BRCONF_ATTR_UPSTREAM]);

	br = calloc_a(sizeof(*br),
		      &vlan_buf, vlan_size,
		      &ignore_buf, ignore_size,
		      &upstream_buf, upstream_size,
		      &name_buf, strlen(blobmsg_name(data)) + 1);
	if (vlan_size)
		br->vlans = memcpy(vlan_buf, tb[BRCONF_ATTR_VLANS], vlan_size);
	if (ignore_size)
		br->ignore = memcpy(ignore_buf, tb[BRCONF_ATTR_IGNORE], ignore_size);
	if (upstream_size)
		br->upstream = memcpy(upstream_buf, tb[BRCONF_ATTR_UPSTREAM], upstream_size);

	br->node.key = strcpy(name_buf, blobmsg_name(data));
	avl_insert(&bridges, &br->node);
}

void dhcprelay_dev_config_update(struct blob_attr *br_attr, struct blob_attr *dev)
{
	struct bridge_entry *br, *tmp;
	struct blob_attr *cur;
	int rem;

	avl_remove_all_elements(&bridges, br, node, tmp)
		free(br);

	blobmsg_for_each_attr(cur, br_attr, rem) {
		if (!blobmsg_check_attr(cur, true) ||
		    blobmsg_type(cur) != BLOBMSG_TYPE_TABLE)
			continue;

		dhcprelay_bridge_add(cur);
	}

	free(devlist);
	devlist = dev ? blob_memdup(dev) : NULL;

	dhcprelay_update_devices();
}

int dhcprelay_dev_init(void)
{
	dhcprelay_dev_done();

	if (dhcprelay_run_cmd("ip link add "DHCPRELAY_IFB_NAME" type ifb", false) ||
	    dhcprelay_run_cmd("ip link set dev "DHCPRELAY_IFB_NAME" up", false) ||
	    dhcprelay_open_socket())
		return -1;

	return 0;
}

void dhcprelay_dev_done(void)
{
	if (ufd.registered) {
		uloop_fd_delete(&ufd);
		close(ufd.fd);
	}

	dhcprelay_run_cmd("ip link del "DHCPRELAY_IFB_NAME, true);
	vlist_flush_all(&devices);
}
