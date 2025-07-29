// SPDX-License-Identifier: GPL-2.0+
/*
 * net/dsa/tag_mxl862xx_8021q.c - DSA driver 802.1q based Special Tag support for MaxLinear 862xx switch chips
 *
 * Copyright (C) 2024 MaxLinear Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#endif

#include <linux/dsa/8021q.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 1, 0))
#include "tag_8021q.h"
#endif
#include <net/dsa.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
#include "dsa_priv.h"
#else
#include "tag.h"
#endif


#define MXL862_NAME	"mxl862xx"

/* To define the outgoing port and to discover the incoming port
 * a special 4-byte outer VLAN tag is used by the MxL862xx.
 *
 *       Dest MAC       Src MAC    special   optional  EtherType
 *                                 outer     inner
 *                                 VLAN tag  tag(s)
 * ...| 1 2 3 4 5 6 | 1 2 3 4 5 6 | 1 2 3 4 | 1 2 3 4 | 1 2 |...
 *                                |<------->|
 */

/* special tag in TX path header */

/* The mxl862_8021q 4-byte tagging is not yet supported in
 * kernels >= 5.16 due to differences in DSA 8021q tagging handlers.
 * DSA tx/rx vid functions are not avaliable, so dummy
 * functions are here to make the code compilable. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION (5, 16, 0))
static u16 dsa_8021q_rx_vid(struct dsa_switch *ds, int port)
{
   return 0;
}

static u16 dsa_8021q_tx_vid(struct dsa_switch *ds, int port)
{
	return 0;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION (5, 14, 0))
static void dsa_8021q_rcv(struct sk_buff *skb, int *source_port, int *switch_id)
{
	u16 vid, tci;

	skb_push_rcsum(skb, ETH_HLEN);
	if (skb_vlan_tag_present(skb)) {
		tci = skb_vlan_tag_get(skb);
		__vlan_hwaccel_clear_tag(skb);
	} else {
		__skb_vlan_pop(skb, &tci);
	}
	skb_pull_rcsum(skb, ETH_HLEN);

	vid = tci & VLAN_VID_MASK;

	*source_port = dsa_8021q_rx_source_port(vid);
	*switch_id = dsa_8021q_rx_switch_id(vid);
	skb->priority = (tci & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT;
}

/* If the ingress port offloads the bridge, we mark the frame as autonomously
 * forwarded by hardware, so the software bridge doesn't forward in twice, back
 * to us, because we already did. However, if we're in fallback mode and we do
 * software bridging, we are not offloading it, therefore the dp->bridge_dev
 * pointer is not populated, and flooding needs to be done by software (we are
 * effectively operating in standalone ports mode).
 */
static inline void dsa_default_offload_fwd_mark(struct sk_buff *skb)
{
	struct dsa_port *dp = dsa_slave_to_port(skb->dev);

	skb->offload_fwd_mark = !!(dp->bridge_dev);
}
#endif

static struct sk_buff *mxl862_8021q_tag_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 7, 0))
	struct dsa_port *dp = dsa_slave_to_port(dev);
#else
	struct dsa_port *dp = dsa_user_to_port(dev);
#endif
	unsigned int port = dp->index ;

	u16 tx_vid = dsa_8021q_tx_vid(dp->ds, port);
	u16 queue_mapping = skb_get_queue_mapping(skb);
	u8 pcp = netdev_txq_to_tc(dev, queue_mapping);


	dsa_8021q_xmit(skb, dev, ETH_P_8021Q,
			      ((pcp << VLAN_PRIO_SHIFT) | tx_vid));

	return skb;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
static struct sk_buff *mxl862_8021q_tag_rcv(struct sk_buff *skb,
				      struct net_device *dev,
				      struct packet_type *pt)
#else
static struct sk_buff *mxl862_8021q_tag_rcv(struct sk_buff *skb,
				      struct net_device *dev)
#endif
{
	uint16_t vlan = ntohs(*(uint16_t*)(skb->data));
	int port = dsa_8021q_rx_source_port(vlan);
	int src_port = -1;
	int switch_id = -1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 7, 0))
	skb->dev = dsa_master_find_slave(dev, 0, port);
#else
	skb->dev = dsa_conduit_find_user(dev, 0, port);
#endif
	if (!skb->dev) {
		dev_warn_ratelimited(&dev->dev,"Dropping packet due to invalid source port:%d\n", port);
		return NULL;
	}
	/* removes Outer VLAN tag */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	dsa_8021q_rcv(skb, &src_port, &switch_id);
#else
	dsa_8021q_rcv(skb, &src_port, &switch_id, NULL);
#endif

	dsa_default_offload_fwd_mark(skb);

	return skb;
}

static const struct dsa_device_ops mxl862_8021q_netdev_ops = {
	.name = "mxl862_8021q",
	.proto = DSA_TAG_PROTO_MXL862_8021Q,
	.xmit = mxl862_8021q_tag_xmit,
	.rcv = mxl862_8021q_tag_rcv,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0))
	.overhead = VLAN_HLEN,
#else
	.needed_headroom	= VLAN_HLEN,
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 7, 0) && \
	 LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
	.promisc_on_master	= true,
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(6, 7, 0))
	.promisc_on_conduit = true,
#endif
};


MODULE_LICENSE("GPL");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_MXL862_8021Q);
#else
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_MXL862_8021Q, MXL862_NAME);
#endif

module_dsa_tag_driver(mxl862_8021q_netdev_ops);
