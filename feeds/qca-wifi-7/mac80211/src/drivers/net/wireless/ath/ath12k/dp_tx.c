// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "dp.h"
#include "dp_tx.h"
#include "debug.h"
#include "debugfs.h"
#include "hw.h"
#include "peer.h"
#include "mac.h"
#include "ppe.h"
#include "hal.h"
#include "dp_peer.h"
#include "dp_stats.h"
#include "dp_htt.h"

void ath12k_tid_tx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_pkt_stats[reason]++;
	tstats->tid_stats[tid].tx_pkt_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_tx_stats);

void ath12k_tid_tx_drop_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_drop_stats[reason]++;
	tstats->tid_stats[tid].tx_drop_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_tx_drop_stats);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static void
ath12k_dp_ppeds_tx_release_desc_list_bulk(struct ath12k_dp *dp,
					  struct list_head *local_list,
					  int local_list_len,
					  struct list_head *local_list_no_skb,
					  int list_no_skb_count)
{
	struct ath12k_ppeds_tx_desc_info *desc = NULL, *first_desc = NULL, *last_desc = NULL, *tmp;
	int hotlist_remaining_len;
	struct sk_buff_head free_list_head;
	struct sk_buff *skb;
	int count = 0;
	struct list_head local_list_for_reuse;

	spin_lock_bh(&dp->ppe.ppeds_tx_desc_lock);

	if (unlikely(list_no_skb_count)) {
		list_for_each_entry_safe(desc, tmp, local_list_no_skb, list) {
			desc->paddr = (dma_addr_t)NULL;
			desc->in_use = false;
		}

		list_splice_tail(local_list_no_skb, &dp->ppe.ppeds_tx_desc_free_list);
	}

	hotlist_remaining_len = ath12k_ppeds_desc_params.ppeds_hotlist_len -
						dp->ppe.ppeds_tx_desc_reuse_list_len;

	if (likely(hotlist_remaining_len >= local_list_len)) {
		list_splice_tail(local_list, &dp->ppe.ppeds_tx_desc_reuse_list);
		dp->ppe.ppeds_tx_desc_reuse_list_len += local_list_len;
		spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);
		return;
	}

	if (hotlist_remaining_len == 0)
		goto skip_reuse_list;

	/* Identify the first and last descriptors in local_list with length of
	 * available room in hotlist
	 */
	list_for_each_entry_safe(desc, tmp, local_list, list) {
		if (count == 0)
			first_desc = desc;

		if (count == (hotlist_remaining_len - 1)) {
			last_desc = desc;
			break;
		}

		count++;
	}

	INIT_LIST_HEAD(&local_list_for_reuse);

	if (first_desc && last_desc) {
		/* cut the local_list into local_list_for_reuse and local_list */
		list_cut_position(&local_list_for_reuse, local_list, &last_desc->list);

		/* merge local_list_for_reuse into global dp->ppe.ppeds_tx_desc_reuse_list */
		list_splice_tail(&local_list_for_reuse, &dp->ppe.ppeds_tx_desc_reuse_list);
		dp->ppe.ppeds_tx_desc_reuse_list_len += count + 1;
	}

skip_reuse_list:
	skb_queue_head_init(&free_list_head);

	list_for_each_entry_safe(desc, tmp, local_list, list) {
		skb = desc->skb;
		desc->skb = NULL;
		desc->paddr = (dma_addr_t)NULL;
		desc->in_use = false;
		if (!skb) {
			pr_err("no skb in ds completion path");
			continue;
		}

		if (likely(skb->is_from_recycler))
			__skb_queue_head(&free_list_head, skb);
		else
			dev_kfree_skb(skb);
	}

	/* Add the remaining descriptors to the free list */
	list_splice_tail(local_list, &dp->ppe.ppeds_tx_desc_free_list);

	spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);

	dev_kfree_skb_list_fast(&free_list_head);
}
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static inline
void ath12k_dp_ppeds_tx_comp_get_desc(struct ath12k_base *ab,
				      struct hal_wbm_completion_ring_tx *tx_status,
				      struct ath12k_ppeds_tx_desc_info **tx_desc)
{
	u64 desc_va = 0;
	u32 desc_id;

	if (likely(HAL_WBM_COMPL_TX_INFO0_CC_DONE & tx_status->info0)) {
		/* HW done cookie conversion */
		desc_va = ((u64)tx_status->buf_va_hi << 32 |
			   tx_status->buf_va_lo);
		*tx_desc = (struct ath12k_ppeds_tx_desc_info *)((unsigned long)desc_va);
	} else {
		/* SW does cookie conversion to VA */
		desc_id = u32_get_bits(tx_status->buf_va_hi,
				       BUFFER_ADDR_INFO1_SW_COOKIE);

		*tx_desc = ath12k_dp_get_ppeds_tx_desc(ab, desc_id);
	}
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_hal_srng_ppeds_dst_inv_entry(struct ath12k_base *ab,
					 struct hal_srng *srng, int entries)
{
	u32 *desc, *last_desc;
	u32 tp, hp;
	u32 remaining_entries;

	if (!(srng->flags & HAL_SRNG_FLAGS_CACHED) || !entries)
		return;

	tp = srng->u.dst_ring.tp;
	hp = srng->u.dst_ring.cached_hp;

	desc = srng->ring_base_vaddr + tp;
	if (hp > tp) {
		last_desc = ((void *)desc + entries * srng->entry_size * sizeof(u32));
		ath12k_core_dmac_inv_range_no_dsb((void *)desc, (void *)last_desc);
	} else {
		remaining_entries = srng->ring_size - tp;
		last_desc = ((void *)desc + remaining_entries * sizeof(u32));
		ath12k_core_dmac_inv_range_no_dsb((void *)desc, (void *)last_desc);

		last_desc = ((void *)srng->ring_base_vaddr + hp * sizeof(u32));
		ath12k_core_dmac_inv_range_no_dsb((void *)srng->ring_base_vaddr,
						  (void *)last_desc);
	}

	dsb(st);
}
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static void ath12k_ppeds_tx_update_stats(struct ath12k *ar, int skb_len,
					 struct hal_wbm_completion_ring_tx *tx_status)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct ath12k_link_sta *arsta;
	struct hal_tx_status ts = { 0 };
	bool tx_drop = false;
	bool tx_status_default = false;
	struct ieee80211_tx_info info;
	u8 reason;

	memset(&info, 0, sizeof(info));
	info.status.rates[0].idx = -1;

	dp = ath12k_ab_to_dp(ab);
	dp->arch_ops->dp_tx_status_parse(ab, tx_status, &ts);
	info.status.ack_signal = ATH12K_DEFAULT_NOISE_FLOOR + ts.ack_rssi;
	info.status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	dp->ppe.ppeds_stats.tqm_rel_reason[ts.status]++;

	if (ts.status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
		info.flags |= IEEE80211_TX_STAT_ACK;
	else if (ts.status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX)
		info.flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts.status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
			reason = ATH_TX_DS_TQM_REMOVE_MPDU;
			break;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_DS_TQM_DROP_THRESHOLD;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_DS_TQM_REMOVE_TX;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_DS_TQM_REMOVE_AGED;
			break;
		default:
			reason = ATH_TX_DS_TQM_REMOVE_DEF;
			//TODO: Remove this print and add as a stats
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "tx frame is not acked status %d\n",
				   ts.status);
			tx_status_default = true;
		}
		tx_drop = true;
	}

	rcu_read_lock();

	peer = ath12k_dp_link_peer_find_by_id(dp, ts.peer_id);
	if (unlikely(!peer || !peer->sta || !peer->vif)) {
		rcu_read_unlock();
		return;
	}

	arsta = ath12k_peer_get_link_sta(ab, peer);
	if (!arsta) {
		rcu_read_unlock();
		return;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		ahvif = ath12k_vif_to_ahvif(peer->vif);
		if (tx_drop) {
			ath12k_tid_tx_drop_stats(ahvif, ts.tid, skb_len,
						 reason);
		} else {
			ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
					    ATH_TX_PPEDS_PKTS);
			ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
					    ATH_TX_COMPLETED_PKTS);
		}
	}

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !tx_status_default) {
		rcu_read_unlock();
		return;
	}

#ifdef CPTCFG_MAC80211_DS_SUPPORT
	ieee80211_ppeds_tx_update_stats(ar->ah->hw, peer->sta, &info,
					peer->txrate, peer->link_id, skb_len);
#endif
	rcu_read_unlock();
}
#endif

u16 dp_sawf_msduq_peer_id_set(u16 peer_id, u8 msduq)
{
	u16 peer_msduq = 0;

	peer_msduq |= (peer_id & SDWF_PEER_ID_MASK) << SDWF_PEER_ID_SHIFT;
	peer_msduq |= (msduq & SDWF_MSDUQ_MASK);
	return peer_msduq;
}

int ath12k_sdwf_reinject_handler(struct ath12k_base *ab, struct sk_buff *skb,
				 struct htt_tx_wbm_completion *status_desc, u8 mac_id)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	struct ath12k_skb_cb *skb_cb;
	struct ath12k_dp *dp;
	u8 host_tid_queue;
	u16 data_length;
	u16 peer_msduq;
	u8 htt_q_idx;
	u8 msduq_idx;
	u16 peer_id;
	u8 pdev_id;
	u8 msduq;
	int ret;
	u8 tid;
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);

	peer_id = le32_get_bits(status_desc->info2, HTT_TX_WBM_REINJECT_SW_PEER_ID_M);
	data_length = le32_get_bits(status_desc->info2, HTT_TX_WBM_REINJECT_DATA_LEN_M);
	tid = le32_get_bits(status_desc->info3, HTT_TX_WBM_REINJECT_TID_M);
	htt_q_idx = le32_get_bits(status_desc->info3, HTT_TX_WBM_REINJECT_MSDUQ_ID_M);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
		   "peer_id %u data_length %u tid %u htt_q_idx %u",
		   peer_id, data_length, tid, htt_q_idx);

	host_tid_queue = htt_q_idx - DP_SDWF_DEFAULT_Q_PTID_MAX;
	msduq_idx = tid + host_tid_queue * DP_SDWF_TID_MAX;

	if (msduq_idx > DP_SDWF_Q_MAX - 1) {
		ath12k_err(ab, "Invalid msduq idx: %u, tid %u htt_q_idx %u",
			   msduq_idx, tid, htt_q_idx);
		return -EINVAL;
	}

	dp = ath12k_ab_to_dp(ab);
	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, mac_id);

	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (!peer) {
		ath12k_err(ab, "Invalid peer id %u", peer_id);
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	if (!peer->dp_peer) {
		ath12k_err(ab, "dp_peer is NULL for peer_id %u", peer_id);
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	qos = peer->dp_peer->qos;
	if (!qos) {
		ath12k_err(ab, "QOS ctx for peer id %u", peer_id);
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	msduq = msduq_idx + DP_SDWF_DEFAULT_Q_MAX;
	peer_msduq = dp_sawf_msduq_peer_id_set(peer_id, msduq);

	skb->mark = ath_encode_sdwf_metadata(peer_msduq);
	skb->len = data_length;

	skb_cb = ATH12K_SKB_CB(skb);
	skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;
	tx_info->flags |= IEEE80211_TX_CTL_HW_80211_ENCAP;

	arsta = ath12k_peer_get_link_sta(ab, peer);
	spin_unlock_bh(&dp->dp_lock);

	if (!arsta) {
		rcu_read_unlock();
		return -EINVAL;
	}
	arvif = arsta->arvif;

	/* This arch ops is temporary, must be removed once ppeds handler is moved to wifi7 */
	ret = dp->arch_ops->sdwf_reinject_handler(dp_pdev, arvif, skb, arsta);

	rcu_read_unlock();

	return ret;
}

static inline
void ath12k_ppeds_reinject_handler(struct ath12k_base *ab,
				   struct ath12k_ppeds_tx_desc_info *tx_desc,
				   struct htt_tx_wbm_completion *status_desc)
{
	u8 reinject_reason;
	int status;

	reinject_reason = le32_get_bits(status_desc->info1,
					HTT_TX_WBM_COMPLETION_V3_REINJECT_REASON_M);

	if (reinject_reason == HTT_TX_FW2WBM_REINJECT_REASON_SDWF_SVC_CLASS_ID_ABSENT) {
		struct sk_buff *skb = tx_desc->skb;
		/* sdwf reinject handler consume the skb,
		 * so set tx_desc->skb = NULL here.
		 */
		status = ath12k_sdwf_reinject_handler(ab, skb, status_desc,
						      tx_desc->mac_id);
		if (!status)
			tx_desc->skb = NULL;

		return;
	}
}

int ath12k_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k *ar;
	struct ath12k_pdev_dp *dp_pdev;
	struct dp_ppeds_tx_comp_ring *tx_ring = &dp->ppe.ppeds_comp_ring;
	int hal_ring_id = tx_ring->ppe_wbm2sw_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_ppeds_tx_desc_info *tx_desc = NULL;
	int valid_entries, count = 0;
	int list_no_skb_count = 0;
	struct hal_wbm_release_ring *desc;
	struct hal_wbm_completion_ring_tx *tx_status, *status;
	struct htt_tx_wbm_completion *status_desc;
	enum hal_wbm_rel_src_module buf_rel_source;
	int htt_status;
	struct list_head local_list;
	struct list_head local_list_no_skb;
	size_t stat_size;

	BUG_ON(budget > DP_PPEDS_SERVICE_BUDGET);

	if (likely(ab->stats_disable))
		/* only need buf_addr_info and info0 */
		stat_size = 3 * sizeof(u32);
	else
		stat_size = sizeof(struct hal_wbm_release_ring);
	INIT_LIST_HEAD(&local_list);
	INIT_LIST_HEAD(&local_list_no_skb);

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return count;
	}

	if (valid_entries >= budget)
		valid_entries = budget;

	ath12k_hal_srng_ppeds_dst_inv_entry(ab, status_ring, valid_entries);

	while (likely(valid_entries--)) {
		desc = (struct hal_wbm_release_ring *)
			__ath12k_hal_srng_dst_get_next_cached_entry(status_ring, NULL);
		if (!desc || !ath12k_dp_tx_completion_valid(desc))
			continue;

		tx_status = (struct hal_wbm_completion_ring_tx *)desc;
		if (likely(!ab->stats_disable))
			memcpy(&tx_ring->tx_status[count], desc, stat_size);

		buf_rel_source = FIELD_GET(HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE,
					   tx_status->info0);

		ath12k_dp_ppeds_tx_comp_get_desc(ab, tx_status, &tx_desc);
		if (unlikely(!tx_desc)) {
			ath12k_warn(ab, "unable to retrieve ppe ds tx_desc!");
			continue;
		}
		tx_ring->macid[count] = tx_desc->mac_id;

		if (unlikely(buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW)) {
			status_desc = (void *)tx_status;
			htt_status = le32_get_bits(status_desc->info0,
						   HAL_TX_COMP_TQM_RELEASE_REASON_MASK);

			if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ) {
				ath12k_ppeds_reinject_handler(ab, tx_desc, status_desc);
			}

			if (htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_OK &&
			    htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ) {
				ab->dp->ppe.ppeds_stats.fw2wbm_pkt_drops++;
				ath12k_dbg(ab, ATH12K_DBG_PPE,
					   "ath12k: Frame received from unexpected source %d status %d!\n",
					   buf_rel_source, htt_status);
			}
			tx_ring->macid[count] = 0xF;
		}
		/* add descriptor to local list to process in bulk */
		tx_desc->in_use = false;
		if (likely(tx_desc->skb)) {
			list_add_tail(&tx_desc->list, &local_list);
			if (tx_ring->macid[count] != 0xF) {
				ar = ab->pdevs[tx_ring->macid[count]].ar;
				dp_pdev = &ar->dp;
				if (ath12k_dp_stats_enabled(dp_pdev)) {
					status = &tx_ring->tx_status[count];
					ath12k_ppeds_tx_update_stats(ar,
								     tx_desc->skb->len,
								     status);
				}
			}
			count++;
		} else {
			list_add_tail(&tx_desc->list, &local_list_no_skb);
			list_no_skb_count++;
		}
	}
	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	ath12k_dp_ppeds_tx_release_desc_list_bulk(dp, &local_list, count,
						  &local_list_no_skb, list_no_skb_count);
	return (count + list_no_skb_count);
}
#endif


void ath12k_dp_tx_encap_nwifi(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (void *)skb->data;
	u8 *qos_ctl;

	if (!ieee80211_is_data_qos(hdr->frame_control))
		return;

	qos_ctl = ieee80211_get_qos_ctl(hdr);
	memmove(skb->data + IEEE80211_QOS_CTL_LEN,
		skb->data, (void *)qos_ctl - (void *)skb->data);
	skb_pull(skb, IEEE80211_QOS_CTL_LEN);

	hdr = (void *)skb->data;
	hdr->frame_control &= ~__cpu_to_le16(IEEE80211_STYPE_QOS_DATA);
}
EXPORT_SYMBOL(ath12k_dp_tx_encap_nwifi);

void ath12k_dp_tx_release_txbuf(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc,
				u8 pool_id)
{
	spin_lock_bh(&dp->tx_desc_lock[pool_id]);
	tx_desc->skb_ext_desc = NULL;
	tx_desc->in_use = false;
	tx_desc->flags = 0;
	list_add_tail(&tx_desc->list, &dp->tx_desc_free_list[pool_id]);
	spin_unlock_bh(&dp->tx_desc_lock[pool_id]);
}
EXPORT_SYMBOL(ath12k_dp_tx_release_txbuf);

struct ath12k_tx_desc_info *ath12k_dp_tx_assign_buffer(struct ath12k_dp *dp,
						       u8 pool_id)
{
	struct ath12k_tx_desc_info *desc, *next_desc;

	spin_lock_bh(&dp->tx_desc_lock[pool_id]);
	desc = list_first_entry_or_null(&dp->tx_desc_free_list[pool_id],
					struct ath12k_tx_desc_info,
					list);
	if (!desc) {
		spin_unlock_bh(&dp->tx_desc_lock[pool_id]);
		return NULL;
	}

	list_del(&desc->list);
	desc->in_use = true;

	next_desc = list_first_entry_or_null(&dp->tx_desc_free_list[pool_id],
					     struct ath12k_tx_desc_info,
			list);
	if (next_desc)
		prefetch(next_desc);

	spin_unlock_bh(&dp->tx_desc_lock[pool_id]);

	return desc;
}
EXPORT_SYMBOL(ath12k_dp_tx_assign_buffer);

enum hal_encrypt_type ath12k_dp_tx_get_encrypt_type(u32 cipher)
{
	switch (cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		return HAL_ENCRYPT_TYPE_WEP_40;
	case WLAN_CIPHER_SUITE_WEP104:
		return HAL_ENCRYPT_TYPE_WEP_104;
	case WLAN_CIPHER_SUITE_TKIP:
		return HAL_ENCRYPT_TYPE_TKIP_MIC;
	case WLAN_CIPHER_SUITE_CCMP:
		return HAL_ENCRYPT_TYPE_CCMP_128;
	case WLAN_CIPHER_SUITE_CCMP_256:
		return HAL_ENCRYPT_TYPE_CCMP_256;
	case WLAN_CIPHER_SUITE_GCMP:
		return HAL_ENCRYPT_TYPE_GCMP_128;
	case WLAN_CIPHER_SUITE_GCMP_256:
		return HAL_ENCRYPT_TYPE_AES_GCMP_256;
	default:
		return HAL_ENCRYPT_TYPE_OPEN;
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_get_encrypt_type);

void *ath12k_dp_metadata_align_skb(struct sk_buff *skb, u8 tail_len)
{
	struct sk_buff *tail;
	void *metadata;

	if (unlikely(skb_cow_data(skb, tail_len, &tail) < 0))
		return NULL;

	metadata = pskb_put(skb, tail, tail_len);
	memset(metadata, 0, tail_len);
	return metadata;
}
EXPORT_SYMBOL(ath12k_dp_metadata_align_skb);

static void ath12k_dp_tx_move_payload(struct sk_buff *skb,
				      unsigned long delta,
				      bool head)
{
	unsigned long len = skb->len;

	if (head) {
		skb_push(skb, delta);
		memmove(skb->data, skb->data + delta, len);
		skb_trim(skb, len);
	} else {
		skb_put(skb, delta);
		memmove(skb->data + delta, skb->data, len);
		skb_pull(skb, delta);
	}
}

int ath12k_dp_tx_align_payload(struct ath12k_dp *dp, struct sk_buff **pskb)
{
	u32 iova_mask = dp->hw_params->iova_mask;
	unsigned long offset, delta1, delta2;
	struct sk_buff *skb2, *skb = *pskb;
	unsigned int headroom = skb_headroom(skb);
	int tailroom = skb_tailroom(skb);
	int ret = 0;

	offset = (unsigned long)skb->data & iova_mask;
	delta1 = offset;
	delta2 = iova_mask - offset + 1;

	if (headroom >= delta1) {
		ath12k_dp_tx_move_payload(skb, delta1, true);
	} else if (tailroom >= delta2) {
		ath12k_dp_tx_move_payload(skb, delta2, false);
	} else {
		skb2 = skb_realloc_headroom(skb, iova_mask);
		if (!skb2) {
			ret = -ENOMEM;
			goto out;
		}

		dev_kfree_skb_any(skb);

		offset = (unsigned long)skb2->data & iova_mask;
		if (offset)
			ath12k_dp_tx_move_payload(skb2, offset, true);
		*pskb = skb2;
	}

out:
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_tx_align_payload);

int
ath12k_dp_tx_htt_h2t_vdev_stats_ol_req(struct ath12k *ar, u64 reset_bitmask)
{
	struct ath12k_base *ab = ar->ab;
	struct htt_h2t_msg_type_vdev_txrx_stats_req *cmd;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	int len = sizeof(*cmd), ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_h2t_msg_type_vdev_txrx_stats_req *)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr = FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_MSG_TYPE,
			      HTT_H2T_MSG_TYPE_VDEV_TXRX_STATS_CFG);
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_PDEV_ID,
			       ar->pdev->pdev_id);
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_ENABLE, true);

	/* Periodic interval is calculated as 1 units = 8 ms.
	* Ex: 125 -> 1000 ms
	*/
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_INTERVAL,
			       (ATH12K_STATS_TIMER_DUR_1SEC >> 3));
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_RESET_STATS, true);
	cmd->vdev_id_lo_bitmask = (reset_bitmask & HTT_H2T_VDEV_TXRX_LO_BITMASK);
	cmd->vdev_id_hi_bitmask = ((reset_bitmask &
				    HTT_H2T_VDEV_TXRX_HI_BITMASK) >> 32);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_warn(ab, "failed to send htt type vdev stats offload request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

u8 ath12k_dp_get_link_id(struct ath12k_pdev_dp *dp_pdev,
			 struct hal_tx_status *ts,
			 struct ath12k_dp_peer *peer)
{
	u8 hw_link_id = ts->hw_link_id;

	if (hw_link_id >= ATH12K_DP_MAX_MLO_LINKS) {
		/* For invalid Link_id update stats on primary link */
		hw_link_id = dp_pdev->hw_link_id;
	}

	if (peer->hw_links[hw_link_id] > ATH12K_DP_MAX_MLO_LINKS)
		return peer->hw_links[dp_pdev->hw_link_id];

	return peer->hw_links[hw_link_id];
}
EXPORT_SYMBOL(ath12k_dp_get_link_id);

void ath12k_dp_tx_update_peer_basic_stats(struct ath12k_dp_peer *peer,
					  u32 msdu_len, u8 tx_status,
					  u8 link_id, int ring_id)
{
	DP_PEER_STATS_PKT_LEN(peer, tx, ring_id, comp_pkt, link_id, 1, msdu_len);

	if (tx_status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		DP_PEER_STATS_PKT_LEN(peer, tx, ring_id, tx_success, link_id,
				      1, msdu_len);
	} else {
		DP_PEER_STATS_INC(peer, tx, ring_id, tx_failed, link_id, 1);
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_update_peer_basic_stats);

void ath12k_dp_tx_comp_update_peer_stats(struct ath12k_dp_peer *peer,
					 struct hal_tx_status *ts,
					 int ring_id, u16 tx_desc_flags)
{
	u8 link_id = peer->stats_link_id;

	if (peer->is_vdev_peer) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU) {
			if (tx_desc_flags & DP_TX_DESC_FLAG_BCAST)
				DP_PEER_STATS_INC(peer, tx, ring_id, bcast, link_id, 1);
			if (tx_desc_flags & DP_TX_DESC_FLAG_MCAST)
				DP_PEER_STATS_INC(peer, tx, ring_id, mcast, link_id, 1);
		}
	} else {
			DP_PEER_STATS_INC(peer, tx, ring_id, ucast, link_id, 1);
	}

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM) {
		DP_PEER_STATS_INC(peer, tx, ring_id, release_src_not_tqm,
				  link_id, 1);
		DP_PEER_STATS_INC(peer, tx, ring_id, wbm_rel_reason[ts->status],
				  link_id, 1);
		return;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		DP_PEER_STATS_COND_INC(peer, tx, ring_id, retry_count, link_id,
				       ts->transmit_cnt > 1, 1);

		DP_PEER_STATS_COND_INC(peer, tx, ring_id, total_msdu_retries,
				       link_id,
				       ts->transmit_cnt > 1,
				       ts->transmit_cnt - 1);

		DP_PEER_STATS_COND_INC(peer, tx, ring_id, multiple_retry_count,
				       link_id, ts->transmit_cnt > 2, 1);

		DP_PEER_STATS_COND_INC(peer, tx, ring_id, ofdma, link_id,
				       ts->ofdma, 1);

		DP_PEER_STATS_COND_INC(peer, tx, ring_id, amsdu_cnt, link_id,
				       ts->msdu_part_of_amsdu, 1);

		DP_PEER_STATS_COND_INC(peer, tx, ring_id, non_amsdu_cnt, link_id,
				       !ts->msdu_part_of_amsdu, 1);
	}

	if (ts->status < HAL_WBM_TQM_REL_REASON_MAX) {
		DP_PEER_STATS_INC(peer, tx, ring_id, tqm_rel_reason[ts->status],
				  link_id, 1);
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_comp_update_peer_stats);

int ath12k_dp_tx_htt_pri_link_migr_msg(struct ath12k_base *ab, u16 vdev_id,
				       u16 peer_id, u16 ml_peer_id, u8 pdev_id,
				       u8 chip_id, u16 src_info, bool status)
{
	struct ath12k_htt_pri_link_migr_h2t_msg *cmd;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	bool src_info_valid = false;
	int len = sizeof(*cmd);
	struct sk_buff *skb;
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct ath12k_htt_pri_link_migr_h2t_msg *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_RESP,
				      ATH12K_HTT_PRI_LINK_MIGR_MSG_TYPE) |
		     le32_encode_bits(chip_id, ATH12K_HTT_PRI_LINK_MIGR_CHIP_ID) |
		     le32_encode_bits(pdev_id, ATH12K_HTT_PRI_LINK_MIGR_PDEV_ID) |
		     le32_encode_bits(vdev_id, ATH12K_HTT_PRI_LINK_MIGR_VDEV_ID);

	ml_peer_id &= ~ATH12K_PEER_ML_ID_VALID;

	cmd->info1 = le32_encode_bits(peer_id, ATH12K_HTT_PRI_LINK_MIGR_PEER_ID) |
		     le32_encode_bits(ml_peer_id, ATH12K_HTT_PRI_LINK_MIGR_ML_PEER_ID);

	if (src_info != 0)
		src_info_valid = true;

	cmd->info2 = le32_encode_bits(status, ATH12K_HTT_PRI_LINK_MIGR_STATUS) |
		     le32_encode_bits(src_info, ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO) |
		     le32_encode_bits(src_info_valid,
		     		      ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO_VALID);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
		   "htt MLO send pri link migr resp for peer_id 0x%x ml_peer_id 0x%x vdev_id 0x%x pdev_id 0x%x chip_id 0x%x src_info 0x%x status %u\n",
		   peer_id, ml_peer_id, vdev_id, pdev_id, chip_id, src_info, status);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret)
		dev_kfree_skb_any(skb);

	return ret;
}
