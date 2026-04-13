/*
 *Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef __ATHDEBUG_SKBUFF_H
#define __ATHDEBUG_SKBUFF_H

#include <linux/ath_memdebug.h>

#define __netdev_alloc_skb_no_skb_reset(dev, len, flags) \
	ath_netdev_alloc_skb_no_skb_reset(dev, len, flags, __LINE__, __func__)
#define netdev_alloc_skb(dev, len) \
	ath_netdev_alloc_skb(dev, len, __LINE__, __func__)
#define netdev_alloc_skb_fast(dev, len) \
	ath_netdev_alloc_skb_fast(dev, len, __LINE__, __func__)
#define dev_alloc_skb(len)		ath_dev_alloc_skb(len, __LINE__, __func__)
#define skb_copy(skb, flags)		ath_skb_copy(skb, flags, __LINE__, __func__)
#define skb_clone(skb, flags)		ath_skb_clone(skb, flags, __LINE__, __func__)
#define skb_clone_sk(skb)		ath_skb_clone_sk(skb, __LINE__, __func__)
#define skb_share_check(skb, flags) \
	ath_skb_share_check(skb, flags, __LINE__, __func__)

#endif /* __ATHDEBUG_SKBUFF_H */
