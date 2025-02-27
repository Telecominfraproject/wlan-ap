/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */
#ifndef __ATHMEMDEBUG_H
#define __ATHMEMDEBUG_H

struct sk_buff;
struct sk_buff_head;

void ath_update_alloc(void *ptr, int len, int line, void *func, int is_txskb);
void ath_upate_oom_panic(int is_panic);
void ath_update_free(void *ptr);
void ath_update_free_skb_list(struct sk_buff_head *skb_list);


void *ath_kmalloc(size_t len, gfp_t flags, int line, void *func);
void *ath_kmemdup(const void *src, size_t len, gfp_t flags, int line, void *func);
void *ath_kzalloc(size_t len, gfp_t flags, int line, void *func);
void *ath_vmalloc(unsigned long len, int line, void *func);
void *ath_vzalloc(unsigned long len, int line, void *func);
void *ath_kcalloc(size_t n, size_t len, gfp_t flags, int line, void *func);

void ath_kfree(void *ptr);
void ath_vfree(void *ptr);
void ath_kfree_sensitive(const void *ptr);


void *ath_netdev_alloc_skb_no_skb_reset(struct net_device *dev, unsigned int length,
					gfp_t flags, int line, void *func);
void *ath_netdev_alloc_skb(struct net_device *dev, unsigned int len,
			   int line, void *func);
void *ath_netdev_alloc_skb_fast(struct net_device *dev, unsigned int len,
				int line, void *func);
void *ath_dev_alloc_skb(unsigned int len, int line, void *func);
void *ath_skb_copy(const struct sk_buff *skb, gfp_t flags, int line, void *func);
void *ath_skb_clone(struct sk_buff *skb, gfp_t flags, int line, const char *func);
void *ath_skb_clone_sk(struct sk_buff *skb, int line, void *func);
void *ath_skb_share_check(struct sk_buff *skb, gfp_t flags, int line, void *func);

void *ath_dma_alloc_coherent(struct device *dev, size_t len,
			     dma_addr_t *handle, gfp_t flags,
			     int line, void *func);
void ath_dma_free_coherent(struct device *dev, size_t size,
			   void *cpu_addr, dma_addr_t dma_handle);


void *ath_nlmsg_new(size_t len, gfp_t flags, int line, void *func);

#endif /* __ATHMEMDEBUG_H */
