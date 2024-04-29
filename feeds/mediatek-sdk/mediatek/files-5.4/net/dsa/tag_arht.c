// SPDX-License-Identifier: GPL-2.0
/*
 * Airoha DSA Tag support
 * Copyright (C) 2023 Min Yao <min.yao@airoha.com>
 */

#include <linux/etherdevice.h>
#include <linux/if_vlan.h>

#include "dsa_priv.h"

#define AIR_HDR_LEN		4
#define AIR_HDR_XMIT_UNTAGGED		0
#define AIR_HDR_XMIT_TAGGED_TPID_8100	1
#define AIR_HDR_XMIT_TAGGED_TPID_88A8	2
#define AIR_HDR_RECV_SOURCE_PORT_MASK	GENMASK(2, 0)
#define AIR_HDR_XMIT_DP_BIT_MASK	GENMASK(5, 0)

static struct sk_buff *air_tag_xmit(struct sk_buff *skb,
				    struct net_device *dev)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	u8 xmit_tpid;
	u8 *air_tag;
	unsigned char *dest = eth_hdr(skb)->h_dest;

	/* Build the special tag after the MAC Source Address. If VLAN header
	 * is present, it's required that VLAN header and special tag is
	 * being combined. Only in this way we can allow the switch can parse
	 * the both special and VLAN tag at the same time and then look up VLAN
	 * table with VID.
	 */
	switch (skb->protocol) {
	case htons(ETH_P_8021Q):
		xmit_tpid = AIR_HDR_XMIT_TAGGED_TPID_8100;
		break;
	case htons(ETH_P_8021AD):
		xmit_tpid = AIR_HDR_XMIT_TAGGED_TPID_88A8;
		break;
	default:
		if (skb_cow_head(skb, AIR_HDR_LEN) < 0)
			return NULL;

		xmit_tpid = AIR_HDR_XMIT_UNTAGGED;
		skb_push(skb, AIR_HDR_LEN);
		memmove(skb->data, skb->data + AIR_HDR_LEN, 2 * ETH_ALEN);
	}

	air_tag = skb->data + 2 * ETH_ALEN;

	/* Mark tag attribute on special tag insertion to notify hardware
	 * whether that's a combined special tag with 802.1Q header.
	 */
	air_tag[0] = xmit_tpid;
	air_tag[1] = (1 << dp->index) & AIR_HDR_XMIT_DP_BIT_MASK;

	/* Tag control information is kept for 802.1Q */
	if (xmit_tpid == AIR_HDR_XMIT_UNTAGGED) {
		air_tag[2] = 0;
		air_tag[3] = 0;
	}

	return skb;
}

static struct sk_buff *air_tag_rcv(struct sk_buff *skb, struct net_device *dev,
				   struct packet_type *pt)
{
	int port;
	__be16 *phdr, hdr;
	unsigned char *dest = eth_hdr(skb)->h_dest;
	bool is_multicast_skb = is_multicast_ether_addr(dest) &&
				!is_broadcast_ether_addr(dest);

	if (dev->features & NETIF_F_HW_VLAN_CTAG_RX) {
		hdr = ntohs(skb->vlan_proto);
		skb->vlan_proto = 0;
		skb->vlan_tci = 0;
	} else {
		if (unlikely(!pskb_may_pull(skb, AIR_HDR_LEN)))
			return NULL;

		/* The AIR header is added by the switch between src addr
		 * and ethertype at this point, skb->data points to 2 bytes
		 * after src addr so header should be 2 bytes right before.
		 */
		phdr = (__be16 *)(skb->data - 2);
		hdr = ntohs(*phdr);

		/* Remove AIR tag and recalculate checksum. */
		skb_pull_rcsum(skb, AIR_HDR_LEN);

		memmove(skb->data - ETH_HLEN,
			skb->data - ETH_HLEN - AIR_HDR_LEN,
			2 * ETH_ALEN);
	}

	/* Get source port information */
	port = (hdr & AIR_HDR_RECV_SOURCE_PORT_MASK);

	skb->dev = dsa_master_find_slave(dev, 0, port);
	if (!skb->dev)
		return NULL;

	/* Only unicast or broadcast frames are offloaded */
	if (likely(!is_multicast_skb))
		skb->offload_fwd_mark = 1;

	return skb;
}

static int air_tag_flow_dissect(const struct sk_buff *skb, __be16 *proto,
				int *offset)
{
	*offset = 4;
	*proto = ((__be16 *)skb->data)[1];

	return 0;
}

static const struct dsa_device_ops air_netdev_ops = {
	.name		= "air",
	.proto		= DSA_TAG_PROTO_ARHT,
	.xmit		= air_tag_xmit,
	.rcv		= air_tag_rcv,
	.flow_dissect	= air_tag_flow_dissect,
	.overhead	= AIR_HDR_LEN,
};

MODULE_LICENSE("GPL");
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_AIR);

module_dsa_tag_driver(air_netdev_ops);
