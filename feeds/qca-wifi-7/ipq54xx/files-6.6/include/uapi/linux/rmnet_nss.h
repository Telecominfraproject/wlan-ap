/* SPDX-License-Identifier: ISC */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _RMNET_NSS_H_
#define _RMENT_NSS_H_

#include <linux/netdevice.h>
#include <linux/skbuff.h>

struct rmnet_nss_cb {
	int (*nss_create)(struct net_device *dev);
	int (*nss_free)(struct net_device *dev);
	int (*nss_tx)(struct sk_buff *skb);
};

extern struct rmnet_nss_cb *rmnet_nss_callbacks;

#endif
