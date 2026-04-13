// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/ieee80211.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <crypto/hash.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include "core.h"
#include "debug.h"
#include "hw.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "peer.h"
#include "dp_mon.h"
#include "debugfs_htt_stats.h"
#include "erp.h"
#include "fse.h"
#include "vendor.h"

void ath12k_tid_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].rx_pkt_stats[reason]++;
	tstats->tid_stats[tid].rx_pkt_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_rx_stats);

void ath12k_tid_drop_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].rx_drop_stats[reason]++;
	tstats->tid_stats[tid].rx_drop_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_drop_rx_stats);

size_t ath12k_dp_list_cut_nodes(struct list_head *list,
				struct list_head *head,
				size_t count)
{
	struct list_head *cur;
	struct ath12k_rx_desc_info *rx_desc;
	size_t nodes = 0;

	if (!count) {
		INIT_LIST_HEAD(list);
		goto out;
	}

	list_for_each(cur, head) {
		if (!count)
			break;

		rx_desc = list_entry(cur, struct ath12k_rx_desc_info, list);
		rx_desc->in_use = true;

		count--;
		nodes++;
	}

	list_cut_before(list, head, cur);
out:
	return nodes;
}

int ath12k_dp_rx_crypto_mic_len(struct ath12k_pdev_dp *dp_pdev,
				enum hal_encrypt_type enctype)
{
	switch (enctype) {
	case HAL_ENCRYPT_TYPE_OPEN:
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		return 0;
	case HAL_ENCRYPT_TYPE_CCMP_128:
		return IEEE80211_CCMP_MIC_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_256:
		return IEEE80211_CCMP_256_MIC_LEN;
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		return IEEE80211_GCMP_MIC_LEN;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		break;
	}

	ath12k_warn(dp_pdev->dp->ab, "unsupported encryption type %d for mic len\n", enctype);
	return 0;
}

int ath12k_dp_rx_crypto_param_len(struct ath12k_pdev_dp *dp_pdev,
				  enum hal_encrypt_type enctype)
{
	switch (enctype) {
	case HAL_ENCRYPT_TYPE_OPEN:
		return 0;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		return IEEE80211_TKIP_IV_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_128:
		return IEEE80211_CCMP_HDR_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_256:
		return IEEE80211_CCMP_256_HDR_LEN;
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		return IEEE80211_GCMP_HDR_LEN;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		break;
	}

	ath12k_warn(dp_pdev->dp->ab, "unsupported encryption type %d\n", enctype);
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_rx_crypto_param_len);

int ath12k_dp_rx_crypto_icv_len(struct ath12k_pdev_dp *dp_pdev,
				enum hal_encrypt_type enctype)
{
	switch (enctype) {
	case HAL_ENCRYPT_TYPE_OPEN:
	case HAL_ENCRYPT_TYPE_CCMP_128:
	case HAL_ENCRYPT_TYPE_CCMP_256:
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		return 0;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		return IEEE80211_TKIP_ICV_LEN;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		break;
	}

	ath12k_warn(dp_pdev->dp->ab, "unsupported encryption type %d\n", enctype);
	return 0;
}

static int ath12k_dp_rx_extract_tuple(struct sk_buff *skb,
				      struct cfg80211_qm_tclas4_params *flow_params)
{
	struct ethhdr *eth;
	u16 eth_type;
	struct udphdr *uh;

	eth = (struct ethhdr *)skb->data;
	eth_type = ntohs(eth->h_proto);

	if (eth_type != ETH_P_IP &&
	    eth_type != ETH_P_IPV6)
		return -EINVAL;

	if (eth_type == ETH_P_IP) {
		struct iphdr *iph = (struct iphdr *)(skb->data + sizeof(struct ethhdr));

		if (iph->protocol != IPPROTO_TCP &&
		    iph->protocol != IPPROTO_UDP)
			return -EINVAL;

		flow_params->protocol = iph->protocol;
		flow_params->ip_ver = IP_VERSION_4;

		memcpy(flow_params->src_ip.ipv4, &iph->saddr, IPV4_LEN);
		memcpy(flow_params->dst_ip.ipv4, &iph->daddr, IPV4_LEN);

		uh = (struct udphdr *)((u8 *)iph + ip_hdrlen(iph));
		flow_params->src_port = ntohs(uh->source);
		flow_params->dst_port = ntohs(uh->dest);

	} else if (eth_type == ETH_P_IPV6) {
		struct ipv6hdr *ipv6h = (struct ipv6hdr *)(skb->data +
				sizeof(struct ethhdr));

		if (ipv6h->nexthdr != IPPROTO_TCP &&
		    ipv6h->nexthdr != IPPROTO_UDP)
			return -EINVAL;

		flow_params->ip_ver = IP_VERSION_6;
		flow_params->protocol = ipv6h->nexthdr;

		memcpy(flow_params->src_ip.ipv6, &ipv6h->saddr, IPV6_LEN);
		memcpy(flow_params->dst_ip.ipv6, &ipv6h->daddr, IPV6_LEN);

		uh = (struct udphdr *)(ipv6h + 1);
		flow_params->src_port = ntohs(uh->source);
		flow_params->dst_port = ntohs(uh->dest);
	}

	return 0;
}

static int
ath12k_dp_rx_mscs_add_fse_flow_entry(struct ath12k_base *ab,
				     struct cfg80211_qm_tclas4_params
				     *flow_params)
{
	struct rx_flow_info flow_info = {0};
	struct hal_flow_tuple_info *tuple_info = &flow_info.flow_tuple_info;
	u32 *src_ip, *dst_ip;
	u8 version = flow_params->ip_ver;

	tuple_info->src_port = flow_params->src_port;
	tuple_info->dest_port = flow_params->dst_port;
	tuple_info->l4_protocol = flow_params->protocol;

	if (version == IP_VERSION_4) {
		flow_info.is_addr_ipv4 = 1;
		src_ip = (u32 *)flow_params->src_ip.ipv4;
		dst_ip = (u32 *)flow_params->dst_ip.ipv4;
		tuple_info->src_ip_31_0 = *src_ip;
		tuple_info->dest_ip_31_0 = *dst_ip;
	} else if (version == IP_VERSION_6) {
		src_ip = (u32 *)flow_params->src_ip.ipv6;
		dst_ip = (u32 *)flow_params->dst_ip.ipv6;

		tuple_info->src_ip_127_96 = src_ip[0];
		tuple_info->src_ip_95_64  = src_ip[1];
		tuple_info->src_ip_63_32  = src_ip[2];
		tuple_info->src_ip_31_0   = src_ip[3];

		tuple_info->dest_ip_127_96 = dst_ip[0];
		tuple_info->dest_ip_95_64  = dst_ip[1];
		tuple_info->dest_ip_63_32  = dst_ip[2];
		tuple_info->dest_ip_31_0   = dst_ip[3];
	} else {
		return -EINVAL;
	}

	flow_info.fse_metadata = ATH12K_RX_FSE_FLOW_MSCS_RULE_PROGRAMMED;
	return ath12k_dp_rx_flow_add_entry(ab, &flow_info);
}

void ath12k_dp_rx_classify_mscs(struct ath12k_base *ab,
				struct ath12k_dp_peer *peer,
				struct sk_buff *skb, u8 tid)
{
	struct cfg80211_qm_tclas4_params flow_params = {0};

	if (ath12k_dp_rx_extract_tuple(skb, &flow_params))
		return;

	rcu_read_lock();
	if (ieee80211_rx_send_mscs_tuple(peer->sta, flow_params, tid)) {
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	if (ath12k_dp_rx_mscs_add_fse_flow_entry(ab, &flow_params))
		return;

	return;
}
EXPORT_SYMBOL(ath12k_dp_rx_classify_mscs);

void ath12k_dp_rx_h_undecap_frag(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				 enum hal_encrypt_type enctype, u32 flags)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ieee80211_hdr *hdr;
	size_t hdr_len;
	size_t crypto_len;
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;

	if (!flags)
		return;

	hdr = (struct ieee80211_hdr *)(msdu->data + hal_rx_desc_sz);

	if (flags & RX_FLAG_MIC_STRIPPED)
		skb_trim(msdu, msdu->len -
			 ath12k_dp_rx_crypto_mic_len(dp_pdev, enctype));

	if (flags & RX_FLAG_ICV_STRIPPED)
		skb_trim(msdu, msdu->len -
			 ath12k_dp_rx_crypto_icv_len(dp_pdev, enctype));

	if (flags & RX_FLAG_IV_STRIPPED) {
		hdr_len = ieee80211_hdrlen(hdr->frame_control);
		crypto_len = ath12k_dp_rx_crypto_param_len(dp_pdev, enctype);

		memmove(msdu->data + hal_rx_desc_sz + crypto_len,
			msdu->data + hal_rx_desc_sz, hdr_len);
		skb_pull(msdu, crypto_len);
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_h_undecap_frag);

int ath12k_dp_rx_h_michael_mic(struct crypto_shash *tfm, u8 *key,
			       struct ieee80211_hdr *hdr, u8 *data,
			       size_t data_len, u8 *mic)
{
	SHASH_DESC_ON_STACK(desc, tfm);
	u8 mic_hdr[16] = {0};
	u8 tid = 0;
	int ret;

	if (!tfm)
		return -EINVAL;

	desc->tfm = tfm;

	ret = crypto_shash_setkey(tfm, key, 8);
	if (ret)
		goto out;

	ret = crypto_shash_init(desc);
	if (ret)
		goto out;

	/* TKIP MIC header */
	memcpy(mic_hdr, ieee80211_get_DA(hdr), ETH_ALEN);
	memcpy(mic_hdr + ETH_ALEN, ieee80211_get_SA(hdr), ETH_ALEN);
	if (ieee80211_is_data_qos(hdr->frame_control))
		tid = ieee80211_get_tid(hdr);
	mic_hdr[12] = tid;

	ret = crypto_shash_update(desc, mic_hdr, 16);
	if (ret)
		goto out;
	ret = crypto_shash_update(desc, data, data_len);
	if (ret)
		goto out;
	ret = crypto_shash_final(desc, mic);
out:
	shash_desc_zero(desc);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_h_michael_mic);

static void ath12k_dp_rx_mld_addr_conv(struct ath12k_pdev_dp *dp_pdev,
				       struct sk_buff *msdu,
				       struct hal_rx_desc *rx_desc, u16 peer_id)
{
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_hdr *hdr = (void *)msdu->data;

	spin_lock_bh(&dp_pdev->dp->dp_lock);
	peer = ath12k_dp_rx_h_find_peer(dp_pdev->dp, rx_desc, peer_id);
	if (!peer || !peer->mlo) {
		spin_unlock_bh(&dp_pdev->dp->dp_lock);
		return;
	}
	ether_addr_copy(hdr->addr2, peer->ml_addr);
	spin_unlock_bh(&dp_pdev->dp->dp_lock);
}

void ath12k_dp_rx_h_undecap_raw(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				struct hal_rx_desc *rx_desc,
				enum hal_encrypt_type enctype,
				struct ieee80211_rx_status *status, bool decrypted,
				u16 peer_id, bool is_first_msdu, bool is_last_msdu)
{
	struct ieee80211_hdr *hdr;
	size_t hdr_len;
	size_t crypto_len;

	if (!is_first_msdu || !(is_first_msdu && is_last_msdu)) {
		/* TODO: Change below stats increment back to WARN_ON_ONCE(1) */
		dp_pdev->dp->device_stats.first_and_last_msdu_bit_miss++;
		return;
	}

	skb_trim(msdu, msdu->len - FCS_LEN);

	if (!decrypted)
		return;

	ath12k_dp_rx_mld_addr_conv(dp_pdev, msdu, rx_desc, peer_id);

	hdr = (void *)msdu->data;

	/* Tail */
	if (status->flag & RX_FLAG_IV_STRIPPED) {
		skb_trim(msdu, msdu->len -
			 ath12k_dp_rx_crypto_mic_len(dp_pdev, enctype));

		skb_trim(msdu, msdu->len -
			 ath12k_dp_rx_crypto_icv_len(dp_pdev, enctype));
	} else {
		/* MIC */
		if (status->flag & RX_FLAG_MIC_STRIPPED)
			skb_trim(msdu, msdu->len -
				 ath12k_dp_rx_crypto_mic_len(dp_pdev, enctype));

		/* ICV */
		if (status->flag & RX_FLAG_ICV_STRIPPED)
			skb_trim(msdu, msdu->len -
				 ath12k_dp_rx_crypto_icv_len(dp_pdev, enctype));
	}

	/* MMIC */
	if ((status->flag & RX_FLAG_MMIC_STRIPPED) &&
	    !ieee80211_has_morefrags(hdr->frame_control) &&
	    enctype == HAL_ENCRYPT_TYPE_TKIP_MIC)
		skb_trim(msdu, msdu->len - IEEE80211_CCMP_MIC_LEN);

	/* Head */
	if (status->flag & RX_FLAG_IV_STRIPPED) {
		hdr_len = ieee80211_hdrlen(hdr->frame_control);
		crypto_len = ath12k_dp_rx_crypto_param_len(dp_pdev, enctype);

		memmove(msdu->data + crypto_len, msdu->data, hdr_len);
		skb_pull(msdu, crypto_len);
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_h_undecap_raw);

static void ath12k_dp_rx_enqueue_free(struct ath12k_dp *dp,
				      struct list_head *used_list)
{
	struct ath12k_rx_desc_info *rx_desc, *tmp_rx_desc;
	struct sk_buff *skb;
	const void *end;

	/* Reset the use flag */
	list_for_each_entry_safe(rx_desc, tmp_rx_desc, used_list, list) {
		rx_desc->in_use = false;
		rx_desc->is_frag = 0;

		if (rx_desc->skb) {
			skb = rx_desc->skb;

			end = rx_desc->vaddr + DP_RX_BUFFER_SIZE;
			ath12k_core_dmac_inv_range(rx_desc->vaddr, end);

			dev_kfree_skb_any(skb);
		}
		dp->device_stats.free_excess_alloc_skb++;
	}
	spin_lock_bh(&dp->rx_desc_lock);
	list_splice_tail(used_list, &dp->rx_desc_free_list);
	spin_unlock_bh(&dp->rx_desc_lock);
}

/* Returns number of Rx buffers replenished */
void ath12k_dp_rx_bufs_replenish(struct ath12k_dp *dp,
				struct dp_rxdma_ring *rx_ring,
				struct list_head *used_list)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_buffer_addr *desc;
	struct hal_srng *srng;
	struct sk_buff *skb;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc, *tmp_rx_desc;
	enum hal_rx_buf_return_buf_manager mgr = dp->hal->hal_params->rx_buf_rbm;
	int allocated_entries = 0;
	bool is_dma_inv_done = false;

	list_for_each_entry_safe(rx_desc, tmp_rx_desc, used_list, list) {
#ifdef CPTCFG_MAC80211_SFE_SUPPORT
		skb = netdev_alloc_skb_fast(NULL, DP_RX_BUFFER_SIZE);
#else
		skb = dev_alloc_skb(DP_RX_BUFFER_SIZE);
#endif
		if (unlikely(!skb))
			break;

#ifndef CONFIG_IO_COHERENCY
		if (unlikely(!skb->fast_recycled)) {
			dmac_inv_range_no_dsb(skb->data, skb->data + DP_RX_BUFFER_SIZE);
			is_dma_inv_done = true;
		}
#endif
		paddr = virt_to_phys(skb->data);
		if(unlikely(!paddr)) {
			ath12k_dp_rx_skb_free(skb, dp, 0,
					      DP_RX_ERR_DROP_REPLENISH);
			break;
		}

		allocated_entries++;
		rx_desc->skb = skb;
		rx_desc->paddr = paddr;
		rx_desc->vaddr = skb->data;
		rx_desc->is_frag = 0;
	}

	if (unlikely(is_dma_inv_done))
		dsb(st);

	srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	while (allocated_entries > 0) {
		rx_desc = list_first_entry_or_null(used_list, struct ath12k_rx_desc_info, list);
		if (unlikely(!rx_desc))
			goto out;

		desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!desc))
			goto out;

		list_del(&rx_desc->list);

		allocated_entries--;

		ath12k_hal_rx_buf_addr_info_set(desc, rx_desc->paddr, rx_desc->cookie, mgr);
	}

out:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	if (unlikely(!list_empty(used_list)))
		ath12k_dp_rx_enqueue_free(dp, used_list);

}
EXPORT_SYMBOL(ath12k_dp_rx_bufs_replenish);

static int ath12k_dp_rxdma_ring_buf_setup(struct ath12k_base *ab,
					  struct dp_rxdma_ring *rx_ring)
{
	LIST_HEAD(list);
	size_t req_entries;

	rx_ring->bufs_max = rx_ring->refill_buf_ring.size /
			ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_BUF);

	req_entries = ath12k_dp_get_req_entries_from_buf_ring(ab, rx_ring, &list);
	if (req_entries)
		ath12k_dp_rx_bufs_replenish(ab->dp, rx_ring, &list);

	return 0;
}

int ath12k_dp_rxdma_buf_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	ret = ath12k_dp_rxdma_ring_buf_setup(ab, &dp->rx_refill_buf_ring);
	if (ret) {
		ath12k_warn(ab,
			    "failed to setup HAL_RXDMA_BUF\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_rxdma_buf_setup);

void ath12k_dp_rx_reo_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		ath12k_dp_srng_cleanup(ab, &dp->reo_dst_ring[i]);
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_cleanup);

int ath12k_dp_rx_reo_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;
	int i;

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		ret = ath12k_dp_srng_setup(ab, &dp->reo_dst_ring[i],
					   HAL_REO_DST, i, 0,
					   DP_REO_DST_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to setup reo_dst_ring\n");
			goto err_reo_cleanup;
		}
	}

	return 0;

err_reo_cleanup:
	ath12k_dp_rx_reo_cleanup(ab);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_setup);

void ath12k_dp_rx_reo_cmd_list_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_rx_reo_cmd *cmd, *tmp;
	struct ath12k_dp_rx_reo_cache_flush_elem *cmd_cache, *tmp_cache;
	struct ath12k_dp_rx_tid *rx_tid;
	struct dp_reo_update_rx_queue_elem *cmd_queue, *tmp_queue;

	spin_lock_bh(&dp->reo_cmd_update_rx_queue_lock);
	list_for_each_entry_safe(cmd_queue, tmp_queue, &dp->reo_cmd_update_rx_queue_list,
				 list) {
		list_del(&cmd_queue->list);
		rx_tid = &cmd_queue->data;
		if (rx_tid->vaddr) {
			rx_tid->active = false;
			ath12k_core_dma_unmap_single(ab->dev, rx_tid->paddr,
					 rx_tid->size, DMA_BIDIRECTIONAL);
			kfree(rx_tid->vaddr);
			rx_tid->vaddr = NULL;
		}
		kfree(cmd_queue);
	}
	spin_unlock_bh(&dp->reo_cmd_update_rx_queue_lock);

	spin_lock_bh(&dp->reo_cmd_lock);
	list_for_each_entry_safe(cmd, tmp, &dp->reo_cmd_list, list) {
		list_del(&cmd->list);
		cmd->handler(dp, &cmd->data, HAL_REO_CMD_DRAIN);
		kfree(cmd);
	}

	list_for_each_entry_safe(cmd_cache, tmp_cache,
				 &dp->reo_cmd_cache_flush_list, list) {
		list_del(&cmd_cache->list);
		dp->reo_cmd_cache_flush_count--;
		rx_tid = &cmd_cache->data;
		if (rx_tid->vaddr) {
			rx_tid->active = false;
			ath12k_core_dma_unmap_single(ab->dev, rx_tid->paddr,
						     rx_tid->size, DMA_BIDIRECTIONAL);
			kfree(rx_tid->vaddr);
			rx_tid->vaddr = NULL;
		}
		kfree(cmd_cache);
	}
	spin_unlock_bh(&dp->reo_cmd_lock);
}
EXPORT_SYMBOL(ath12k_dp_rx_reo_cmd_list_cleanup);

void ath12k_dp_reo_cmd_free(struct ath12k_dp *dp, void *ctx,
			    enum hal_reo_cmd_status status)
{
	struct ath12k_dp_rx_tid *rx_tid = ctx;

	if (status == HAL_REO_CMD_DRAIN)
		goto free_desc;
	else if (status != HAL_REO_CMD_SUCCESS)
		ath12k_warn(dp->ab, "failed to flush rx tid hw desc, tid %d status %d\n",
			    rx_tid->tid, status);

free_desc:
	ath12k_hal_reo_shared_qaddr_cache_clear(dp->ab);

	if (rx_tid->vaddr) {
		rx_tid->active = false;
		ath12k_core_dma_unmap_single(dp->ab->dev, rx_tid->paddr, rx_tid->size,
					     DMA_BIDIRECTIONAL);
		kfree(rx_tid->vaddr);
		rx_tid->vaddr = NULL;
	}
}
EXPORT_SYMBOL(ath12k_dp_reo_cmd_free);

void ath12k_dp_rx_frags_cleanup(struct ath12k_dp_rx_tid *rx_tid,
				       bool rel_link_desc)
{
	struct ath12k_buffer_addr *buf_addr_info;
	struct ath12k_dp *dp = rx_tid->dp;
	enum hal_wbm_rel_bm_act bm_act;

	lockdep_assert_held(&dp->dp_lock);

	if (rx_tid->dst_ring_desc) {
		if (rel_link_desc) {
			bm_act = HAL_WBM_REL_BM_ACT_PUT_IN_IDLE;
			buf_addr_info = &rx_tid->dst_ring_desc->buf_addr_info;
			ath12k_dp_arch_rx_link_desc_return(dp, buf_addr_info,
							   bm_act);
		}

		kfree(rx_tid->dst_ring_desc);
		rx_tid->dst_ring_desc = NULL;
	}

	rx_tid->cur_sn = 0;
	rx_tid->last_frag_no = 0;
	rx_tid->rx_frag_bitmap = 0;
	__skb_queue_purge(&rx_tid->rx_frags);
}
EXPORT_SYMBOL(ath12k_dp_rx_frags_cleanup);

void ath12k_dp_rx_peer_tid_cleanup(struct ath12k *ar, struct ath12k_dp_link_peer *peer)
{
	struct ath12k_dp_rx_tid *rx_tid;
	int i;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	lockdep_assert_held(&dp->dp_lock);

	if (!peer->primary_link)
		return;

	for (i = 0; i <= IEEE80211_NUM_TIDS; i++) {
		rx_tid = &peer->dp_peer->rx_tid[i];

		ath12k_dp_arch_rx_peer_tid_delete(dp, ar, peer, i);
		ath12k_dp_rx_frags_cleanup(rx_tid, true);

		spin_unlock_bh(&dp->dp_lock);
		del_timer_sync(&rx_tid->frag_timer);
		spin_lock_bh(&dp->dp_lock);
	}
}

int ath12k_wifi7_dp_rx_peer_tid_setup(struct ath12k *ar, const u8 *peer_mac, int vdev_id,
				      u8 tid, u32 ba_win_sz, u16 ssn,
				      enum hal_pn_type pn_type)
{
	struct hal_rx_reo_queue *addr_aligned;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_rx_tid *rx_tid;
	dma_addr_t paddr;
	int ret;

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, peer_mac);
	if (!peer || !peer->dp_peer) {
		spin_unlock_bh(&dp->dp_lock);
		ath12k_warn(ab, "failed to find the peer to set up rx tid\n");
		return -ENOENT;
	}

	if (!peer->primary_link) {
		spin_unlock_bh(&dp->dp_lock);
		return 0;
	}

	if (ab->hw_params->reoq_lut_support &&
	    (!dp->reoq_lut.vaddr || !dp->ml_reoq_lut.vaddr)) {
		spin_unlock_bh(&dp->dp_lock);
		ath12k_warn(ab, "reo qref table is not setup\n");
		return -EINVAL;
	}

	if (peer->peer_id > DP_MAX_PEER_ID || tid > IEEE80211_NUM_TIDS) {
		ath12k_warn(ab, "peer id of peer %d or tid %d doesn't allow reoq setup\n",
			    peer->peer_id, tid);
		spin_unlock_bh(&dp->dp_lock);
		return -EINVAL;
	}

	rx_tid = &peer->dp_peer->rx_tid[tid];
	/* Update the tid queue if it is already setup */
	if (rx_tid->active) {
		paddr = rx_tid->paddr;
		ret = ath12k_dp_arch_peer_rx_tid_reo_update(dp, ar, peer, rx_tid,
							    ba_win_sz, ssn, true);
		spin_unlock_bh(&dp->dp_lock);
		if (ret) {
			ath12k_warn(ab, "failed to update reo for peer %pM rx tid %d\n",
									peer_mac, tid);
			return ret;
		}

		if (!ab->hw_params->reoq_lut_support) {
			ret = ath12k_wmi_peer_rx_reorder_queue_setup(ar, vdev_id,
								     peer_mac,
								     paddr, tid, 1,
								     ba_win_sz);
			if (ret) {
				ath12k_warn(ab, "failed to setup peer rx reorder queuefor tid %d: %d\n",
					    tid, ret);
				return ret;
			}
		}

		return 0;
	}

	rx_tid->tid = tid;

	rx_tid->ba_win_sz = ba_win_sz;

	ret = ath12k_dp_arch_alloc_reo_qdesc(dp, rx_tid, ssn, pn_type, &addr_aligned);
	if (ret < 0) {
		spin_unlock_bh(&dp->dp_lock);
		return ret;
	}

	rx_tid->active = true;

	if (ab->hw_params->reoq_lut_support) {
		/* Update the REO queue LUT at the corresponding peer id
		 * and tid with qaddr.
		 */
		if (peer->mlo)
			ath12k_dp_arch_peer_rx_tid_qref_setup(dp, peer->ml_id,
							      rx_tid->tid,
							      rx_tid->paddr);
		else
			ath12k_dp_arch_peer_rx_tid_qref_setup(dp, peer->peer_id,
							      rx_tid->tid,
							      rx_tid->paddr);

		spin_unlock_bh(&dp->dp_lock);
	} else {
		spin_unlock_bh(&dp->dp_lock);
		ret = ath12k_wmi_peer_rx_reorder_queue_setup(ar, vdev_id,
							     peer_mac,
							     rx_tid->paddr,
							     rx_tid->tid, 1,
							     rx_tid->ba_win_sz);
	}

	return ret;
}

int ath12k_dp_rx_ampdu_start(struct ath12k *ar,
			     struct ieee80211_ampdu_params *params,
			     u8 link_id)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(params->sta);
	struct ath12k_link_sta *arsta;
	int vdev_id;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				  ahsta->link[link_id]);
	if (!arsta)
		return -ENOLINK;

	vdev_id = arsta->arvif->vdev_id;

	ret = ath12k_wifi7_dp_rx_peer_tid_setup(ar, arsta->addr, vdev_id,
						params->tid, params->buf_size,
						params->ssn, arsta->ahsta->pn_type);
	if (ret)
		ath12k_warn(ab, "failed to setup rx tid %d\n", ret);

	return ret;
}

int ath12k_dp_rx_ampdu_stop(struct ath12k *ar,
			    struct ieee80211_ampdu_params *params,
			    u8 link_id)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(params->sta);
	struct ath12k_link_sta *arsta;
	int vdev_id;
	bool active;
	int ret;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				  ahsta->link[link_id]);
	if (!arsta)
		return -ENOLINK;

	vdev_id = arsta->arvif->vdev_id;

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, arsta->addr);
	if (!peer || !peer->dp_peer) {
		spin_unlock_bh(&dp->dp_lock);
		ath12k_warn(ab, "failed to find the peer to stop rx aggregation\n");
		return -ENOENT;
	}

	if (!peer->primary_link) {
		spin_unlock_bh(&dp->dp_lock);
		return 0;
	}

	active = peer->dp_peer->rx_tid[params->tid].active;

	if (!active) {
		spin_unlock_bh(&dp->dp_lock);
		return 0;
	}

	ret = ath12k_dp_arch_peer_rx_tid_reo_update(dp, ar, peer,
				&peer->dp_peer->rx_tid[params->tid], 1, 0, false);
	spin_unlock_bh(&dp->dp_lock);
	if (ret) {
		ath12k_warn(ab, "failed to update reo for rx tid %d: %d\n",
			    params->tid, ret);
		return ret;
	}

	return ret;
}

int ath12k_dp_rx_peer_pn_replay_config(struct ath12k_link_vif *arvif,
				       const u8 *peer_addr,
				       enum set_key_cmd key_cmd,
				       struct ieee80211_key_conf *key)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_rx_tid *rx_tid;
	u8 tid;
	int ret = 0;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	/* NOTE: Enable PN/TSC replay check offload only for unicast frames.
	 * We use mac80211 PN/TSC replay check functionality for bcast/mcast
	 * for now.
	 */
	if (!(key->flags & IEEE80211_KEY_FLAG_PAIRWISE))
		return 0;

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp,
							    arvif->vdev_id, peer_addr);
	if (!peer || !peer->dp_peer) {
		spin_unlock_bh(&dp->dp_lock);
		ath12k_warn(ab, "failed to find the peer %pM to configure pn replay detection\n",
			    peer_addr);
		return -ENOENT;
	}

	if (!peer->primary_link) {
		spin_unlock_bh(&dp->dp_lock);
		return 0;
	}

	for (tid = 0; tid <= IEEE80211_NUM_TIDS; tid++) {
		rx_tid = &peer->dp_peer->rx_tid[tid];
		if (!rx_tid->active)
			continue;

		ath12k_dp_arch_setup_pn_check_reo_cmd(dp, &cmd, rx_tid, key->cipher,
						      key_cmd);
		ret = ath12k_dp_arch_reo_cmd_send(dp, rx_tid,
						  HAL_REO_CMD_UPDATE_RX_QUEUE,
						  &cmd, NULL);
		if (ret) {
			ath12k_warn(ab, "failed to configure rx tid %d queue of peer %pM for pn replay detection %d\n",
				    tid, peer_addr, ret);
			break;
		}
	}

	spin_unlock_bh(&dp->dp_lock);

	return ret;
}

struct sk_buff *ath12k_dp_rx_get_msdu_last_buf(struct sk_buff_head *msdu_list,
					       struct sk_buff *first)
{
	struct sk_buff *skb;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(first);

	if (!rxcb->is_continuation)
		return first;

	skb_queue_walk(msdu_list, skb) {
		rxcb = ATH12K_SKB_RXCB(skb);
		if (!rxcb->is_continuation)
			return skb;
	}

	return NULL;
}

struct ath12k_dp_link_peer *
ath12k_dp_rx_h_find_peer(struct ath12k_dp *dp, struct hal_rx_desc *rx_desc, u16 peer_id)
{
	struct ath12k_dp_link_peer *peer = NULL;
	void *peer_mac;


	peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (peer)
		return peer;

	peer_mac = ath12k_hal_rxdesc_get_mpdu_start_addr2(dp->hal, rx_desc);
	if (peer_mac)
		peer = ath12k_dp_link_peer_find_by_addr(dp, peer_mac);

	return peer;
}

void ath12k_dp_rx_deliver_msdu(struct ath12k_pdev_dp *dp_pdev,
			       struct napi_struct *napi,
			       struct sk_buff *msdu,
			       struct ieee80211_rx_status *status,
			       u8 hw_link_id, bool is_mcbc, u16 peer_id, u16 tid)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_dp_peer *peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_vif *ahvif;
	u8 addr[ETH_ALEN] = {0};

	rcu_read_lock();

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);

	if (peer) {
		pubsta = peer->sta;
		memcpy(addr, peer->addr, ETH_ALEN);
	}

	if (pubsta && pubsta->valid_links) {
		status->link_valid = 1;
		status->link_id = peer->hw_links[hw_link_id];

		link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								     peer_id);
		if (link_peer && link_peer->is_bridge_peer) {
			dev_kfree_skb_any(msdu);
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "Packet received on bridge peer link_id %d, drop it\n",
				   link_peer->link_id);
			rcu_read_unlock();
			return;
		}
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								     peer_id);
		if (link_peer) {
			ahvif = ath12k_vif_to_ahvif(link_peer->vif);
			ath12k_tid_rx_stats(ahvif, tid, msdu->len,
					    ATH_RX_TOTAL_OUT_PKTS);
		}
		msdu->priority = tid;
	}

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "rx skb %p len %u peer %pM %d %s %s%s%s%s%s%s%s%s%s%s rate_idx %u vht_nss %u freq %u band %u flag 0x%x fcs-err %i mic-err %i amsdu-more %i\n",
		   msdu,
		   msdu->len,
		   addr,
		   tid,
		   is_mcbc ? "mcast" : "ucast",
		   (status->encoding == RX_ENC_LEGACY) ? "legacy" : "",
		   (status->encoding == RX_ENC_HT) ? "ht" : "",
		   (status->encoding == RX_ENC_VHT) ? "vht" : "",
		   (status->encoding == RX_ENC_HE) ? "he" : "",
		   (status->encoding == RX_ENC_EHT) ? "eht" : "",
		   (status->bw == RATE_INFO_BW_40) ? "40" : "",
		   (status->bw == RATE_INFO_BW_80) ? "80" : "",
		   (status->bw == RATE_INFO_BW_160) ? "160" : "",
		   (status->bw == RATE_INFO_BW_320) ? "320" : "",
		   status->enc_flags & RX_ENC_FLAG_SHORT_GI ? "sgi " : "",
		   status->rate_idx,
		   status->nss,
		   status->freq,
		   status->band, status->flag,
		   !!(status->flag & RX_FLAG_FAILED_FCS_CRC),
		   !!(status->flag & RX_FLAG_MMIC_ERROR),
		   !!(status->flag & RX_FLAG_AMSDU_MORE));

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_RX, NULL, "dp rx msdu: ",
			msdu->data, msdu->len);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	/* TODO: trace rx packet */

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
}
EXPORT_SYMBOL(ath12k_dp_rx_deliver_msdu);

static void ath12k_dp_rx_frag_timer(struct timer_list *timer)
{
	struct ath12k_dp_rx_tid *rx_tid = from_timer(rx_tid, timer, frag_timer);

	spin_lock_bh(&rx_tid->dp->dp_lock);
	if (rx_tid->last_frag_no &&
	    rx_tid->rx_frag_bitmap == GENMASK(rx_tid->last_frag_no, 0)) {
		spin_unlock_bh(&rx_tid->dp->dp_lock);
		return;
	}
	ath12k_dp_rx_frags_cleanup(rx_tid, true);
	spin_unlock_bh(&rx_tid->dp->dp_lock);
}

int ath12k_dp_rx_peer_frag_setup(struct ath12k *ar,
				 struct ath12k_dp_link_peer *peer,
				 struct crypto_shash *tfm)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_rx_tid *rx_tid;
	int i;

	lockdep_assert(&dp->dp_lock);

	for (i = 0; i <= IEEE80211_NUM_TIDS; i++) {
		rx_tid = &peer->dp_peer->rx_tid[i];
		rx_tid->dp = dp;
		timer_setup(&rx_tid->frag_timer, ath12k_dp_rx_frag_timer, 0);
		skb_queue_head_init(&rx_tid->rx_frags);
	}

	peer->dp_peer->tfm_mmic = tfm;
	peer->dp_peer->primary_link_frag_setup = true;

	return 0;
}

int
ath12k_dp_rx_htt_rxdma_rxole_ppe_cfg_set(struct ath12k_base *ab,
					 struct ath12k_dp_htt_rxdma_ppe_cfg_param *param)
{
	struct htt_h2t_msg_type_rxdma_rxole_ppe_cfg *cmd;
	struct ath12k_dp *dp = ab->dp;
	struct sk_buff *skb;
	int len = sizeof(*cmd), ret, val;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct htt_h2t_msg_type_rxdma_rxole_ppe_cfg *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 =
		u32_encode_bits(HTT_H2T_MSG_TYPE_RXDMA_RXOLE_PPE_CFG,
				HTT_H2T_RXOLE_PPE_CFG_MSG_TYPE) |
		u32_encode_bits(param->override, HTT_H2T_RXOLE_PPE_CFG_OVERRIDE) |
		u32_encode_bits(param->reo_dst_ind,
				HTT_H2T_RXOLE_PPE_CFG_REO_DST_IND) |
		u32_encode_bits(param->multi_buffer_msdu_override_en,
				HTT_H2T_RXOLE_PPE_CFG_MULTI_BUF_MSDU_OVRD_EN) |
		u32_encode_bits(param->intra_bss_override,
				HTT_H2T_RXOLE_PPE_CFG_INTRA_BUS_OVRD) |
		u32_encode_bits(param->decap_raw_override,
				HTT_H2T_RXOLE_PPE_CFG_DECAP_RAW_OVRD) |
		u32_encode_bits(param->decap_nwifi_override,
				HTT_H2T_RXOLE_PPE_CFG_NWIFI_OVRD) |
		u32_encode_bits(param->ip_frag_override,
				HTT_H2T_RXOLE_PPE_CFG_IP_FRAG_OVRD);

	val = cmd->info0;
	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_warn(ab, "failed to send htt type H2T rx ole ppe config request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_PPE, "RXOLE ppe config request sent val 0x%x\n", val);

	return 0;
}

static int ath12k_dp_rx_flow_send_fst_setup(struct ath12k_base *ab,
					    struct dp_rx_fst *fst)
{
	struct htt_rx_flow_fst_setup fst_setup = {0};
	int ret;

	fst_setup.max_entries = fst->hal_rx_fst->max_entries;
	fst_setup.max_search = fst->hal_rx_fst->max_skid_length;
	fst_setup.base_addr_lo = lower_32_bits(fst->hal_rx_fst->base_paddr);
	fst_setup.base_addr_hi = upper_32_bits(fst->hal_rx_fst->base_paddr);
	fst_setup.ip_da_sa_prefix =
		HAL_FST_IP_DA_SA_PFX_TYPE_IPV4_COMPATIBLE_IPV6;
	fst_setup.hash_key = fst->hal_rx_fst->key;
	fst_setup.hash_key_len = HAL_FST_HASH_KEY_SIZE_BYTES;

	ret = ath12k_dp_htt_rx_flow_fst_setup(ab, &fst_setup);
	if (ret) {
		ath12k_err(ab, "Failed to send Rx FSE Setup:status %d\n", ret);
		return ret;
	}

	return 0;
}

struct dp_rx_fst *ath12k_dp_rx_fst_attach(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst;
	int ret;

	if (!ab->hw_params->support_fse)
		return NULL;

	fst = kzalloc(sizeof(*fst), GFP_KERNEL);
	if (!fst)
		return NULL;

	ret = ath12k_dp_arch_rx_fst_attach(dp, fst);
	if (ret) {
		kfree(fst);
		return NULL;
	}

	spin_lock_init(&fst->fst_lock);

	ath12k_info(ab, "Rx FST attach successful\n");

	return fst;
}

void ath12k_dp_rx_fst_detach(struct ath12k_base *ab, struct dp_rx_fst *fst)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!fst)
		return;

	ath12k_dp_arch_rx_fst_detach(dp, fst);
	kfree(fst);
}

int ath12k_hw_grp_dp_rx_invalidate_entry(struct ath12k_hw_group *ag,
					 enum dp_htt_flow_fst_operation operation,
					 struct hal_flow_tuple_info *tuple_info)
{
	int i;
	int ret = 0;

	for (i = 0; i < ag->num_devices; i++) {
		struct ath12k_base *partner_ab = ag->ab[i];

		if (!partner_ab || partner_ab->is_bypassed)
			continue;

		/* Skip sending HTT command when recovery in progress */
		if (test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags))
			continue;

		/* Flush entries in the HW cache */
		ret = ath12k_dp_htt_rx_flow_fse_operation(partner_ab, operation,
							  tuple_info);
		if (ret) {
			ath12k_err(partner_ab, "Unable to invalidate cache entry ret %d",
				   ret);
			return ret;
		}
	}
	return ret;
}

static void ath12k_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info)
{
	ath12k_dp_arch_rx_flow_dump_entry(dp, flow_info);
}

int ath12k_dp_rx_flow_add_entry(struct ath12k_base *ab,
				struct rx_flow_info *flow_info)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	int ret;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	/* lock the FST table to prevent concurrent access */
	spin_lock_bh(&fst->fst_lock);

	ret = ath12k_dp_arch_rx_flow_add_entry(dp, flow_info);
	if (ret) {
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		fst->flow_add_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	ret = ath12k_hw_grp_dp_rx_invalidate_entry(ab->ag, DP_HTT_FST_CACHE_INVALIDATE_ENTRY,
						   &flow_info->flow_tuple_info);
	if (ret) {
		ath12k_err(ab, "Unable to invalidate cache entry ret %d", ret);
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		ath12k_dp_arch_rx_flow_delete_entry(dp, flow_info);
		fst->flow_add_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	spin_unlock_bh(&fst->fst_lock);

	if (flow_info->is_addr_ipv4)
		fst->ipv4_fse_rule_cnt++;
	else
		fst->ipv6_fse_rule_cnt++;

out:
	return ret;
}

int ath12k_dp_rx_flow_delete_entry(struct ath12k_base *ab,
				   struct rx_flow_info *flow_info)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	int ret;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	/* lock the FST table to prevent concurrent access */
	spin_lock_bh(&fst->fst_lock);

	ret = ath12k_dp_arch_rx_flow_delete_entry(dp, flow_info);
	if (ret) {
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		fst->flow_del_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	ret = ath12k_hw_grp_dp_rx_invalidate_entry(ab->ag, DP_HTT_FST_CACHE_INVALIDATE_ENTRY,
						   &flow_info->flow_tuple_info);
	if (ret) {
		ath12k_err(ab, "Rx flow delete fail due to invalidate ret %d", ret);
		ath12k_dp_rx_flow_dump_entry(dp, flow_info);
		fst->flow_del_fail++;
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	spin_unlock_bh(&fst->fst_lock);

	if (flow_info->is_addr_ipv4)
		fst->ipv4_fse_rule_cnt--;
	else
		fst->ipv6_fse_rule_cnt--;

out:
	return ret;
}

int ath12k_dp_rx_flow_delete_all_entries(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	int ret;

	spin_lock_bh(&fst->fst_lock);

	ret = ath12k_dp_arch_rx_flow_delete_all_entries(dp);
	if (ret) {
		ath12k_err(ab, "Rx flow delete all entries failed ret %d", ret);
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	ret = ath12k_hw_grp_dp_rx_invalidate_entry(ab->ag,
						   DP_HTT_FST_CACHE_INVALIDATE_FULL,
						   NULL);
	if (ret) {
		ath12k_err(ab, "Rx flow delete all fail due to invalidate ret %d", ret);
		spin_unlock_bh(&fst->fst_lock);
		goto out;
	}

	spin_unlock_bh(&fst->fst_lock);

	fst->ipv4_fse_rule_cnt = 0;
	fst->ipv6_fse_rule_cnt = 0;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);
out:
	return ret;
}

void ath12k_dp_fst_core_map_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_fst_config *fst_config = &dp->fst_config;
	u32 fst_core_mask = fst_config->fst_core_mask;
	int i;
	int core_map_index = 0;

	for (i = 0; i < 4; i++) {
		fst_config->fst_core_map[i] = 0xf;
		if ((fst_core_mask >> i) & 0x1) {
			fst_config->fst_core_map[core_map_index] = i;
			core_map_index++;
		}
	}

	fst_config->fst_num_cores = core_map_index;
	fst_config->core_idx = 0;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "FST core_mask %x num_cores %d\n",
		   fst_config->fst_core_mask, fst_config->fst_num_cores);

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "FST core map %x %x %x %x\n",
		   fst_config->fst_core_map[0],
		   fst_config->fst_core_map[1],
		   fst_config->fst_core_map[2],
		   fst_config->fst_core_map[3]);
}

void ath12k_dp_rx_fst_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	u32 tuple_mask = 0;
	int ret, i;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return;
	}

	fst->ipv4_fse_rule_cnt = 0;
	fst->ipv6_fse_rule_cnt = 0;
	dp->fst_config.fst_core_mask = 0x7;
	dp->fst_config.fst_num_cores = 0;

	ath12k_dp_fst_core_map_init(ab);
	ath12k_dp_rx_flow_send_fst_setup(ab, fst);
	/* After fst setup, make sure that the DDR table and HW cache is in sync
	 * by sending INVALIDATE FULL command. This is needed to avoid DDR
	 * and HW cache going out of sync when one soc goes for a recovery.
	 */
	ath12k_dp_htt_rx_flow_fse_operation(ab,
					    DP_HTT_FST_CACHE_INVALIDATE_FULL,
					    NULL);

	if (!ath12k_fse_3_tuple_enabled)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		tuple_mask = HTT_H2T_FLOW_CLASSIFY_3_TUPLE_FIELD_ENABLE;
		ret = ath12k_dp_htt_rx_fse_3_tuple_config_send(ab, tuple_mask, i);
		if (ret) {
			ath12k_warn(ab, "FSE 3 tuple config failed for pdev:%d\n",
					i);
			return;
		}
	}
}

ssize_t ath12k_dp_dump_fst_table(struct ath12k_base *ab, char *buf, int size)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	return ath12k_dp_arch_dump_fst_table(dp, buf, size);
}

void ath12k_dp_tid_cleanup(struct ath12k_base *ab)
{
        struct ath12k_dp_link_peer *peer;
        struct ath12k_dp_rx_tid *rx_tid;
        int tid;
        void *vaddr;
        u32 *addr_aligned;

        spin_lock_bh(&ab->dp->dp_lock);
        list_for_each_entry(peer, &ab->dp->peers, list) {
		if (peer->dp_peer) {
			for (tid = 0; tid <= IEEE80211_NUM_TIDS; tid++) {
				rx_tid = &peer->dp_peer->rx_tid[tid];
				if (rx_tid->active) {
					vaddr = rx_tid->vaddr;
					addr_aligned = PTR_ALIGN(vaddr,
								 HAL_LINK_DESC_ALIGN);
					ath12k_hal_reset_rx_reo_tid_q(&ab->hal,
								      addr_aligned,
								      rx_tid->ba_win_sz,
								      tid);
				}
			}
		}
        }
        spin_unlock_bh(&ab->dp->dp_lock);
}

void ath12k_dp_peer_reo_tid_setup(struct ath12k *ar, int vdev_id,
                                 const u8 *peer_mac)
{
       struct ath12k_dp_rx_tid *rx_tid;
       struct ath12k_dp_link_peer *peer;
       struct ath12k_dp *dp;
       int ret = 0, tid;

       dp = ath12k_ab_to_dp(ar->ab);
       spin_lock_bh(&dp->dp_lock);

       peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, peer_mac);
	if (!peer || !peer->dp_peer) {
		spin_unlock_bh(&dp->dp_lock);
		ath12k_warn(ar->ab, "failed to find the peer to set up rx tid\n");
		return;
	}

       for (tid = 0; tid <= IEEE80211_NUM_TIDS; tid++) {
               rx_tid = &peer->dp_peer->rx_tid[tid];
               if (!rx_tid->active)
                       continue;

               ret = ath12k_dp_arch_peer_rx_tid_reo_update(dp, ar,
                                                           peer, rx_tid,
                                                           rx_tid->ba_win_sz,
                                                           0, false);
               if (ret) {
                       ath12k_warn(ar->ab, "failed to update reo for peer %pM rx tid %d\n",
                                   peer_mac, tid);
               }
       }
       spin_unlock_bh(&dp->dp_lock);
}

void ath12k_dp_tid_setup(void *data, struct ieee80211_sta *sta)
{
        struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
        struct ath12k *ar;
        struct ath12k_base *ab = data;
        struct ath12k_link_sta *arsta;
        struct ath12k_link_vif *arvif;
        u8 link_id;
	unsigned long links_map;

	if (sta->mlo && ab->ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE2)
                return;

	links_map = ahsta->links_map;
	for_each_set_bit(link_id, &links_map,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		arsta = ahsta->link[link_id];
		if (!arsta)
			continue;
		arvif = arsta->arvif;
		if (arvif && arvif->ar && arvif->ar->ab == ab) {
			ar = arvif->ar;
			ath12k_dp_peer_reo_tid_setup(ar, arvif->vdev_id,
						     arsta->addr);
		}
	}
}

void ath12k_dp_peer_tid_setup(struct ath12k_base *ab)
{
        struct ath12k *ar;
        int i;

        for (i = 0; i <  ab->num_radios; i++) {
                ar = ab->pdevs[i].ar;
                ieee80211_iterate_stations_atomic(ar->ah->hw,
                                                  ath12k_dp_tid_setup,
                                                  ab);
        }
}

void
ath12k_dp_primary_peer_migrate_setup(struct ath12k_dp *dp, void *ctx,
				     enum hal_reo_cmd_status status)
{
	struct ath12k_dp_rx_tid *rx_tid = ctx;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *mig_ab;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_dp *mig_dp;
	u16 peer_id = rx_tid->peer_id;
	u8 chip_id = rx_tid->chip_id;
	int ret, tid;

	if (status != HAL_REO_CMD_SUCCESS)
		goto migration_fail;

	mig_ab = dp->ab->ag->ab[chip_id];
	mig_dp = ath12k_ab_to_dp(mig_ab);

	spin_lock_bh(&mig_dp->dp_lock);

	/* Get peer from the pdev to which the peer is going to migrate */
	peer = ath12k_dp_link_peer_find_by_id(mig_dp, peer_id);
	if (!peer || !peer->dp_peer) {
		ath12k_warn(mig_ab, "failed to find peer for peer_id %d\n", peer_id);
		spin_unlock_bh(&mig_dp->dp_lock);
		goto migration_fail;
	}

	ath12k_info(mig_ab, "htt new primary peer to %pM peer_id 0x%x ml_peer_id 0x%x link_id 0x%x chip_id 0x%x\n",
		    peer->addr, peer->peer_id, peer->ml_id, peer->link_id, chip_id);

	ahsta = ath12k_sta_to_ahsta(peer->sta);
	arsta = ahsta->link[peer->link_id];
	if (!arsta || !arsta->arvif) {
		spin_unlock_bh(&mig_dp->dp_lock);
		goto migration_fail;
	}

	if (!peer->dp_peer->primary_link_frag_setup) {
		ath12k_warn(mig_ab, "peer tid setup is not done for the peer_id %x in migration event\n",
			    peer->peer_id);
		WARN_ON(1);
		goto migration_fail;
	}

	if (mig_ab->hw_params->reoq_lut_support) {
		/* Update the REO queue LUT at the corresponding peer id
		 * and tid with qaddr.
		 */
		for (tid = 0; tid <= IEEE80211_NUM_TIDS; tid++) {
			ath12k_dp_arch_peer_rx_tid_qref_setup(mig_dp,
							      peer->mlo ? peer->ml_id : peer->peer_id,
							      rx_tid->tid,
							      rx_tid->paddr);
		}
	}

	arsta->arvif->primary_sta_link = true;
	peer->primary_link = true;

	spin_unlock_bh(&mig_dp->dp_lock);
	ret = ath12k_vendor_put_umac_migration_notif(peer->vif, peer->sta->addr, peer->link_id);
	if (ret)
		ath12k_warn(mig_ab, "failed to send notify UMAC migration event\n");
	complete(&ahsta->dp_migration_event);
	return;

migration_fail:
	if (ahsta)
		complete(&ahsta->dp_migration_event);

	return;
}
EXPORT_SYMBOL(ath12k_dp_primary_peer_migrate_setup);

int
ath12k_dp_peer_migrate(struct ath12k_sta *ahsta, u16 peer_id,
		       u8 chip_id)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	struct ath12k *ar;
	int ret;

	arsta = ahsta->link[ahsta->primary_link_id];
	if (!arsta->arvif || !arsta->arvif->ar || !arsta->arvif->ar->ab)
		goto out;

	ar = arsta->arvif->ar;
	ab = ar->ab;
	dp = ath12k_ab_to_dp(ab);

	lockdep_assert(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp,
							    arsta->arvif->vdev_id,
							    arsta->addr);
	if (!peer || !peer->primary_link) {
		ath12k_warn(ab,
			    "failed to fetch primary peer for peer addr %pM in MLO pri link migration event\n",
			    arsta->addr);
		goto out;
	}

	if (!peer->dp_peer) {
		ath12k_warn(ab,
			    "dp_peer is NULL for peer addr %pM in MLO pri link migration event\n",
			    arsta->addr);
		goto out;
	}

	ath12k_info(ab, "htt current primary peer  %pM peer_id 0x%x ml_peer_id 0x%x link_id 0x%x\n",
		    peer->addr, peer->peer_id, peer->ml_id, peer->link_id);

	peer->primary_link = false;
	arsta->arvif->primary_sta_link = false;

	ret = ath12k_dp_arch_peer_migrate_reo_cmd(dp, peer, peer_id,
						  chip_id);
	if (ret) {
		ath12k_warn(ab, "failed to send reo cmd, ret:%d\n", ret);
		goto out;
	}

	return 0;
out:
	complete(&ahsta->dp_migration_event);
	return -EINVAL;
}

/* Sends WMI config to filter packets to route packets to WBM release ring */
int ath12k_dp_rx_pkt_type_filter(struct ath12k *ar,
				 enum ath12k_routing_pkt_type pkt_type,
				 u32 meta_data)
{
	struct ath12k_wmi_pkt_route_param param;
	int ret;

	/* Routing Eapol/ARP packets to CCE is only allowed now */
	if (pkt_type != ATH12K_PKT_TYPE_EAP &&
	    pkt_type != ATH12K_PKT_TYPE_ARP_IPV4)
		return -EINVAL;

	param.opcode = ATH12K_WMI_PKTROUTE_ADD;
	param.meta_data = meta_data;
	param.dst_ring = ATH12K_REO_RELEASE_RING;
	param.dst_ring_handler = ATH12K_WMI_PKTROUTE_USE_CCE;
	param.route_type_bmap = 1 << pkt_type;

	ret = ath12k_wmi_send_pdev_pkt_route(ar, &param);
	if (ret)
		ath12k_warn(ar->ab, "failed to configure pkt route %d", ret);

	return ret;
}

void ath12k_dp_rx_skb_free(struct sk_buff *skb, struct ath12k_dp *dp, int ring,
			   enum ath12k_dp_rx_error drop_reason)
{
	if (unlikely(drop_reason > DP_RX_ERR_MAX))
		DP_DEVICE_STATS_INC(dp, rx.rx_err[DP_RX_ERR_DROP_MISC][ring], 1);
	else
		DP_DEVICE_STATS_INC(dp, rx.rx_err[drop_reason][ring], 1);

	dev_kfree_skb_any(skb);
}
EXPORT_SYMBOL(ath12k_dp_rx_skb_free);

void ath12k_dp_rx_update_peer_msdu_stats(struct ath12k_dp_peer *peer,
					 struct rx_msdu_desc_info *rx_msdu_info,
					 struct rx_mpdu_desc_info *rx_mpdu_info,
					 u8 link_id, int ring_id)
{
	bool is_not_msdu;

	if (!peer)
		return;

	is_not_msdu = rx_msdu_info->first_msdu & rx_msdu_info->last_msdu;

	if (is_not_msdu)
		DP_PEER_STATS_INC(peer, rx, ring_id, non_amsdu, link_id, 1);
	else
		DP_PEER_STATS_INC(peer, rx, ring_id, msdu_part_of_amsdu, link_id, 1);

	if (rx_msdu_info->da_is_mcbc)
		DP_PEER_STATS_INC(peer, rx, ring_id, mcast, link_id, 1);
	else
		DP_PEER_STATS_INC(peer, rx, ring_id, ucast, link_id, 1);

	DP_PEER_STATS_COND_INC(peer, rx, ring_id, mpdu_retry, link_id,
			       rx_mpdu_info->mpdu_retry_bit, 1);
}
EXPORT_SYMBOL(ath12k_dp_rx_update_peer_msdu_stats);
