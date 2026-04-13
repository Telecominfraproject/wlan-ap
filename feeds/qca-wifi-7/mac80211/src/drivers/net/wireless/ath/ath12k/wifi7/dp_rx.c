// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/ieee80211.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <crypto/hash.h>
#include "../core.h"
#include "hal.h"
#include "../debug.h"
#include "../peer.h"
#include "../hw.h"
#include "../dp_rx.h"
#include "../debugfs_htt_stats.h"
#include "../dp_tx.h"
#include "../dp_mon.h"
#include "hal_rx.h"
#include "dp_rx.h"
#include "hal_qcn9274.h"
#include "hal_wcn7850.h"
#include "../debugfs.h"
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
#include <ppe_vp_public.h>
#include <ppe_vp_tx.h>
#endif
#include "../fse.h"

#define ATH12K_DP_RX_FRAGMENT_TIMEOUT_MS (2 * HZ)

extern bool ath12k_debug_critical;

static int ath12k_wifi7_peer_rx_tid_delete_handler(struct ath12k_base *ab,
						   struct ath12k_dp_rx_tid *rx_tid,
						   u8 tid);
void ath12k_wifi7_peer_rx_tid_qref_reset(struct ath12k_base *ab, u16 peer_id, u16 tid);

static inline u8 ath12k_wifi7_dp_rx_get_msdu_src_link(struct ath12k_dp *dp,
						      struct hal_rx_desc *desc)
{
	return dp->hw_params->hal_ops->rx_desc_get_msdu_src_link_id(desc);
}

static inline void ath12k_wifi7_dp_rx_desc_end_tlv_copy(struct ath12k_base *ab,
							struct hal_rx_desc *fdesc,
							struct hal_rx_desc *ldesc)
{
	ab->hw_params->hal_ops->rx_desc_copy_end_tlv(fdesc, ldesc);
}

static inline void ath12k_wifi7_dp_rxdesc_set_msdu_len(struct ath12k_base *ab,
						       struct hal_rx_desc *desc,
						       u16 len)
{
	ab->hw_params->hal_ops->rx_desc_set_msdu_len(desc, len);
}

static inline void ath12k_wifi7_dp_rx_desc_get_dot11_hdr(struct ath12k_base *ab,
							 struct hal_rx_desc *desc,
							 struct ieee80211_hdr *hdr)
{
	ab->hw_params->hal_ops->rx_desc_get_dot11_hdr(desc, hdr);
}

static inline
void ath12k_wifi7_dp_rx_desc_get_crypto_header(struct ath12k_base *ab,
					       struct hal_rx_desc *desc,
					       u8 *crypto_hdr,
					       enum hal_encrypt_type enctype)
{
	ab->hw_params->hal_ops->rx_desc_get_crypto_header(desc, crypto_hdr,
			enctype);
}

static inline u16 ath12k_wifi7_dp_rxdesc_get_mpdu_frame_ctrl(struct ath12k_base *ab,
							     struct hal_rx_desc *desc)
{
	return ab->hw_params->hal_ops->rx_desc_get_mpdu_frame_ctl(desc);
}

static bool ath12k_wifi7_dp_rx_h_more_frags(struct ath12k_base *ab,
					    struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;

	hdr = (struct ieee80211_hdr *)(skb->data + ab->hal.hal_desc_sz);
	return ieee80211_has_morefrags(hdr->frame_control);
}

static u16 ath12k_wifi7_dp_rx_h_frag_no(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;

	hdr = (struct ieee80211_hdr *)(skb->data + ab->hal.hal_desc_sz);
	return le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_FRAG;
}

static void ath12k_wifi7_dp_clean_up_skb_list(struct sk_buff_head *skb_list)
{
	struct sk_buff *skb;

	while ((skb = __skb_dequeue(skb_list)))
		dev_kfree_skb_any(skb);
}

int ath12k_wifi7_dp_reo_cmd_send(struct ath12k_base *ab,
				 struct ath12k_dp_rx_tid *rx_tid,
				 enum hal_reo_cmd_type type,
				 struct ath12k_hal_reo_cmd *cmd,
				 void (*cb)(struct ath12k_dp *dp, void *ctx,
					    enum hal_reo_cmd_status status))
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_rx_reo_cmd *dp_cmd;
	struct hal_srng *cmd_ring;
	int cmd_num;

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) ||
	    test_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags))
		return -ESHUTDOWN;

	cmd_ring = &ab->hal.srng_list[dp->reo_cmd_ring.ring_id];
	cmd_num = ath12k_wifi7_hal_reo_cmd_send(ab, cmd_ring, type, cmd);

	/* cmd_num should start from 1, during failure return the error code */
	if (cmd_num < 0)
		return cmd_num;

	/* reo cmd ring descriptors has cmd_num starting from 1 */
	if (cmd_num == 0)
		return -EINVAL;

	if (!cb)
		return 0;

	/* Can this be optimized so that we keep the pending command list only
	 * for tid delete command to free up the resource on the command status
	 * indication?
	 */
	dp_cmd = kzalloc(sizeof(*dp_cmd), GFP_ATOMIC);

	if (!dp_cmd)
		return -ENOMEM;

	memcpy(&dp_cmd->data, rx_tid, sizeof(*rx_tid));
	dp_cmd->cmd_num = cmd_num;
	dp_cmd->handler = cb;

	spin_lock_bh(&dp->reo_cmd_lock);
	list_add_tail(&dp_cmd->list, &dp->reo_cmd_list);
	spin_unlock_bh(&dp->reo_cmd_lock);

	return 0;
}

int ath12k_wifi7_dp_reo_cache_flush(struct ath12k_base *ab,
                                    struct ath12k_dp_rx_tid *rx_tid)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag |= HAL_REO_CMD_FLG_NEED_STATUS |
		HAL_REO_CMD_FLG_FLUSH_FWD_ALL_MPDUS;

	/* For all QoS TIDs (except NON_QOS), the driver allocates a maximum
	 * window size of 1024. In such cases, the driver can issue a single
	 * 1KB descriptor flush command instead of sending multiple 128-byte
	 * flush commands for each QoS TID, improving efficiency.
	 */

	if (rx_tid->tid != HAL_DESC_REO_NON_QOS_TID)
		cmd.flag |= HAL_REO_CMD_FLG_FLUSH_QUEUE_1K_DESC;

	ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid,
					   HAL_REO_CMD_FLUSH_CACHE,
					   &cmd, ath12k_dp_reo_cmd_free);

	return ret;
}

void ath12k_wifi7_peer_rx_tid_qref_setup(struct ath12k_base *ab, u16 peer_id, u16 tid,
					 dma_addr_t paddr)
{
	struct ath12k_reo_queue_ref *qref;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	bool ml_peer = false;

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (peer_id & ATH12K_PEER_ML_ID_VALID) {
		peer_id &= ~ATH12K_PEER_ML_ID_VALID;
		ml_peer = true;
	}

	if (ml_peer)
		qref = (struct ath12k_reo_queue_ref *)dp->ml_reoq_lut.vaddr +
				(peer_id * (IEEE80211_NUM_TIDS + 1) + tid);
	else
		qref = (struct ath12k_reo_queue_ref *)dp->reoq_lut.vaddr +
				(peer_id * (IEEE80211_NUM_TIDS + 1) + tid);

	qref->info0 = u32_encode_bits(lower_32_bits(paddr),
				      BUFFER_ADDR_INFO0_ADDR);
	qref->info1 = u32_encode_bits(upper_32_bits(paddr),
				      BUFFER_ADDR_INFO1_ADDR) |
		      u32_encode_bits(tid, DP_REO_QREF_NUM);
	ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(ab);
}

void ath12k_wifi7_dp_rx_tid_del_func(struct ath12k_dp *dp, void *ctx,
				     enum hal_reo_cmd_status status)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_rx_tid *rx_tid = ctx, *update_rx_tid;
	struct ath12k_dp_rx_reo_cache_flush_elem *elem, *tmp;
	struct dp_reo_update_rx_queue_elem *qelem, *qtmp;

	if (status == HAL_REO_CMD_DRAIN) {
		goto free_desc;
	} else if (status != HAL_REO_CMD_SUCCESS) {
		/* Shouldn't happen! Cleanup in case of other failure? */
		ath12k_warn(ab, "failed to delete rx tid %d hw descriptor %d\n",
			    rx_tid->tid, status);
		return;
	}

	/* Check if there is any pending rx_queue, if yes then update it */
	spin_lock_bh(&dp->reo_cmd_update_rx_queue_lock);
	list_for_each_entry_safe(qelem, qtmp, &dp->reo_cmd_update_rx_queue_list,
				 list) {
		if (qelem->reo_cmd_update_rx_queue_resend_flag &&
		    qelem->data.active) {
			update_rx_tid = &qelem->data;

			if (ath12k_wifi7_peer_rx_tid_delete_handler(ab, update_rx_tid,
							      qelem->tid)) {
				update_rx_tid->active = true;
				break;
			}
			update_rx_tid->active = false;
			update_rx_tid->vaddr = NULL;
			update_rx_tid->paddr = 0;
			update_rx_tid->size = 0;
			update_rx_tid->pending_desc_size = 0;

			list_del(&qelem->list);
			kfree(qelem);
		}
	}
	spin_unlock_bh(&dp->reo_cmd_update_rx_queue_lock);

	elem = kzalloc(sizeof(*elem), GFP_ATOMIC);
	if (!elem)
		goto free_desc;

	elem->ts = jiffies;
	memcpy(&elem->data, rx_tid, sizeof(*rx_tid));

	spin_lock_bh(&dp->reo_cmd_lock);
	list_add_tail(&elem->list, &dp->reo_cmd_cache_flush_list);
	dp->reo_cmd_cache_flush_count++;

	/* Flush and invalidate aged REO desc from HW cache */
	list_for_each_entry_safe(elem, tmp, &dp->reo_cmd_cache_flush_list,
				 list) {
		if (dp->reo_cmd_cache_flush_count > ATH12K_DP_RX_REO_DESC_FREE_THRES ||
		    time_after(jiffies, elem->ts +
			       msecs_to_jiffies(ATH12K_DP_RX_REO_DESC_FREE_TIMEOUT_MS))) {
			/* Unlock the reo_cmd_lock before using ath12k_dp_reo_cmd_send()
			 * within ath12k_wifi7_dp_reo_cache_flush. The reo_cmd_cache_flush_list
			 * is used in only two contexts, one is in this function called
			 * from napi and the other in ath12k_dp_free during core destroy.
			 * Before dp_free, the irqs would be disabled and would wait to
			 * synchronize. Hence there wouldn’t be any race against add or
			 * delete to this list. Hence unlock-lock is safe here.
			 */
			spin_unlock_bh(&dp->reo_cmd_lock);
			if (ath12k_wifi7_dp_reo_cache_flush(dp->ab, &elem->data)) {
				/* In failure case, just update the timestamp
				 * for flush cache elem and continue
				 */
				spin_lock_bh(&dp->reo_cmd_lock);
				elem->ts = jiffies;
				break;
			}
			spin_lock_bh(&dp->reo_cmd_lock);
			list_del(&elem->list);
			dp->reo_cmd_cache_flush_count--;
			kfree(elem);
		}
	}
	spin_unlock_bh(&dp->reo_cmd_lock);

	return;
free_desc:
	rx_tid->active = false;
	ath12k_core_dma_unmap_single(ab->dev, rx_tid->paddr, rx_tid->size,
				     DMA_BIDIRECTIONAL);
	kfree(rx_tid->vaddr);
	rx_tid->vaddr = NULL;
}

static int ath12k_wifi7_peer_rx_tid_delete_handler(struct ath12k_base *ab,
						   struct ath12k_dp_rx_tid *rx_tid,
						   u8 tid)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	lockdep_assert_held(&dp->reo_cmd_update_rx_queue_lock);

	rx_tid->active = false;
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.upd0 |= HAL_REO_CMD_UPD0_VLD;
	cmd.upd0 |= HAL_REO_CMD_UPD0_BA_WINDOW_SIZE;
	cmd.ba_window_size = (tid == HAL_DESC_REO_NON_QOS_TID) ?
			      rx_tid->ba_win_sz : DP_BA_WIN_SZ_MAX;
	cmd.upd1 |= HAL_REO_CMD_UPD1_VLD;

	return ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid,
					    HAL_REO_CMD_UPDATE_RX_QUEUE, &cmd,
					    ath12k_wifi7_dp_rx_tid_del_func);
}

void ath12k_wifi7_peer_rx_tid_qref_reset(struct ath12k_base *ab, u16 peer_id, u16 tid)
{
	struct ath12k_reo_queue_ref *qref;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	bool ml_peer = false;

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (peer_id & ATH12K_PEER_ML_ID_VALID) {
		peer_id &= ~ATH12K_PEER_ML_ID_VALID;
		ml_peer = true;
	}

	if (ml_peer)
		qref = (struct ath12k_reo_queue_ref *)dp->ml_reoq_lut.vaddr +
				(peer_id * (IEEE80211_NUM_TIDS + 1) + tid);
	else
		qref = (struct ath12k_reo_queue_ref *)dp->reoq_lut.vaddr +
				(peer_id * (IEEE80211_NUM_TIDS + 1) + tid);

	qref->info0 = u32_encode_bits(0, BUFFER_ADDR_INFO0_ADDR);
	qref->info1 = u32_encode_bits(0, BUFFER_ADDR_INFO1_ADDR);
}

void ath12k_wifi7_dp_rx_peer_tid_delete(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer, u8 tid)
{
	struct ath12k_dp_rx_tid *rx_tid = &peer->dp_peer->rx_tid[tid];
	struct ath12k_dp_rx_tid *temp_rx_tid = NULL;
	struct dp_reo_update_rx_queue_elem *elem, *tmp;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp   = ath12k_ab_to_dp(ab);

	if (!rx_tid->active)
		return;

	elem = kzalloc(sizeof(*elem), GFP_ATOMIC);
	if (!elem)
		return;

	elem->reo_cmd_update_rx_queue_resend_flag = false;
	elem->peer_id = peer->peer_id;
	elem->tid = tid;
	elem->is_ml_peer = peer->mlo ? true : false;
	elem->ml_peer_id = peer->ml_id;

	memcpy(&elem->data, rx_tid, sizeof(*rx_tid));

	spin_lock_bh(&dp->reo_cmd_update_rx_queue_lock);
	list_add_tail(&elem->list, &dp->reo_cmd_update_rx_queue_list);

	list_for_each_entry_safe(elem, tmp, &dp->reo_cmd_update_rx_queue_list,
			list) {
		temp_rx_tid = &elem->data;

		if (ath12k_wifi7_peer_rx_tid_delete_handler(ab, temp_rx_tid, elem->tid)) {
			temp_rx_tid->active = true;
			elem->reo_cmd_update_rx_queue_resend_flag = true;
			break;
		}
		temp_rx_tid->active = false;
		temp_rx_tid->vaddr = NULL;
		temp_rx_tid->paddr = 0;
		temp_rx_tid->size = 0;
		temp_rx_tid->pending_desc_size = 0;

		list_del(&elem->list);
		kfree(elem);
	}
	spin_unlock_bh(&dp->reo_cmd_update_rx_queue_lock);

	rx_tid->active = false;
	ath12k_wifi7_peer_rx_tid_qref_reset(ab,	peer->mlo ? peer->ml_id : peer->peer_id, tid);
	ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(ab);
	rx_tid->vaddr = NULL;
	rx_tid->paddr = 0;
	rx_tid->size = 0;
	rx_tid->pending_desc_size = 0;
}

void  ath12k_wifi7_dp_setup_pn_check_reo_cmd(struct ath12k_hal_reo_cmd *cmd,
					     struct ath12k_dp_rx_tid *rx_tid,
					     u32 cipher, enum set_key_cmd key_cmd)
{
	cmd->flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd->upd0 = HAL_REO_CMD_UPD0_PN |
			HAL_REO_CMD_UPD0_PN_SIZE |
			HAL_REO_CMD_UPD0_PN_VALID |
			HAL_REO_CMD_UPD0_PN_CHECK |
			HAL_REO_CMD_UPD0_SVLD;

	switch (cipher) {
	case WLAN_CIPHER_SUITE_TKIP:
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		if (key_cmd == SET_KEY) {
			cmd->upd1 |= HAL_REO_CMD_UPD1_PN_CHECK;
			cmd->pn_size = 48;
		}
		break;
	default:
		break;
	}

	cmd->addr_lo = lower_32_bits(rx_tid->paddr);
	cmd->addr_hi = upper_32_bits(rx_tid->paddr);
}

int ath12k_wifi7_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action)
{
	struct hal_wbm_release_ring *desc;
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng;
	int ret = 0;

	srng = &ab->hal.srng_list[dp->wbm_desc_rel_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!desc) {
		ret = -ENOBUFS;
		goto exit;
	}

	ath12k_wifi7_hal_rx_msdu_link_desc_set(ab, desc, buf_addr_info, action);

exit:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return ret;
}

int ath12k_wifi7_peer_rx_tid_reo_update(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer,
					struct ath12k_dp_rx_tid *rx_tid,
					u32 ba_win_sz, u16 ssn,
					bool update_ssn)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	int ret;

	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.upd0 = HAL_REO_CMD_UPD0_BA_WINDOW_SIZE;
	cmd.ba_window_size = ba_win_sz;

	if (update_ssn) {
		cmd.upd0 |= HAL_REO_CMD_UPD0_SSN;
		cmd.upd2 = u32_encode_bits(ssn, HAL_REO_CMD_UPD2_SSN);
	}

	ret = ath12k_wifi7_dp_reo_cmd_send(ar->ab, rx_tid,
					   HAL_REO_CMD_UPDATE_RX_QUEUE, &cmd,
					   NULL);
	if (ret) {
		ath12k_warn(ar->ab, "failed to update rx tid queue, tid %d (%d)\n",
			    rx_tid->tid, ret);
		return ret;
	}

	rx_tid->ba_win_sz = ba_win_sz;

	return 0;
}

static inline
void ath12k_wifi7_dp_rx_update_ppe_msdu_mark(struct ath12k_base *ab,
					     struct ath12k_dp_peer *peer,
					     struct sk_buff *msdu,
					     struct rx_mpdu_desc_info *rx_mpdu_info,
					     struct hal_rx_desc *rx_desc)
{
	if (peer->ppe_vp_num <= 0)
		return;

	ab->hw_params->hal_ops->rx_desc_get_fse_info(rx_desc, rx_mpdu_info);
	if (!rx_mpdu_info->flow_idx_timeout &&
	    !rx_mpdu_info->flow_idx_invalid &&
	    rx_mpdu_info->flow_info.flow_metadata &&
	    (rx_mpdu_info->flow_info.flow_metadata &
	    ATH12K_RX_FSE_FLOW_MATCH_USE_PPE))
		msdu->mark =
			u32_encode_bits(ATH12K_FSE_MAGIC_NUM,
					ATH12K_FSE_MAGIC_NUM_MASK) |
			u32_encode_bits(rx_mpdu_info->flow_info.flow_metadata,
					ATH12K_PPE_VP_NUM);
}

static bool ath12k_wifi7_dp_rx_check_fast_rx(struct ath12k_dp *dp,
					     struct sk_buff *msdu,
					     struct rx_msdu_desc_info *rx_msdu_info,
					     struct rx_tlv_info_1 *tlv_info,
					     struct ath12k_dp_peer *peer)
{
	if (unlikely(!dp->stats_disable ||
		     tlv_info->decap != DP_RX_DECAP_TYPE_ETHERNET2_DIX))
		return false;

	/* mcbc packets go through mac80211 for PN validation */
	if (unlikely(rx_msdu_info->da_is_mcbc))
		return false;

	if (unlikely(!peer->is_authorized))
		return false;

	/* check if the msdu needs to be bridged to our connected peer */
	if (unlikely(rx_msdu_info->intra_bss))
		return false;

	/* allow direct rx */
	return true;
}

static int ath12k_wifi7_dp_rx_msdu_coalesce(struct ath12k_dp *dp,
					    struct hal_rx_spd_data *rx_status_desc,
					    struct sk_buff *first, u8 l3pad_bytes,
					    int msdu_len, struct hal_rx_desc *desc,
					    int *idx, int num_msdus)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_hal *hal = dp->hal;
	struct sk_buff *skb;
	int buf_first_hdr_len, buf_first_len;
	struct hal_rx_desc *ldesc;
	int space_extra, rem_len, buf_len;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;
	bool is_continuation;
	struct hal_rx_spd_data *spd_desc_l;
	int msdu_idx = *idx;

	/* As the msdu is spread across multiple rx buffers,
	 * find the offset to the start of msdu for computing
	 * the length of the msdu in the first buffer.
	 */
	buf_first_hdr_len = hal_rx_desc_sz + l3pad_bytes;
	buf_first_len = DP_RX_BUFFER_SIZE - buf_first_hdr_len;

	if (WARN_ON_ONCE(msdu_len <= buf_first_len)) {
		skb_put(first, buf_first_hdr_len + msdu_len);
		skb_pull(first, buf_first_hdr_len);
		return 0;
	}


	/* MSDU spans over multiple buffers because the length of the MSDU
	 * exceeds DP_RX_BUFFER_SIZE - HAL_RX_DESC_SIZE. So assume the data
	 * in the first buf is of length DP_RX_BUFFER_SIZE - HAL_RX_DESC_SIZE.
	 */
	skb_put(first, DP_RX_BUFFER_SIZE);
	skb_pull(first, buf_first_hdr_len);

	space_extra = msdu_len - (buf_first_len + skb_tailroom(first));
	if (space_extra > 0 &&
	    (pskb_expand_head(first, 0, space_extra, GFP_ATOMIC) < 0)) {
		/* Free up all buffers of the MSDU */
		for (; msdu_idx < num_msdus; msdu_idx++) {
			spd_desc_l = &rx_status_desc[msdu_idx];
			if (!spd_desc_l->msdu)
				continue;

			if (!spd_desc_l->rx_msdu_info.msdu_continuation) {
				dev_kfree_skb_any(spd_desc_l->msdu);
				spd_desc_l->msdu = NULL;
				break;
			}
			dev_kfree_skb_any(spd_desc_l->msdu);
			spd_desc_l->msdu = NULL;
		}
		return -ENOMEM;
	}

	rem_len = msdu_len - buf_first_len;
	msdu_idx++;
	for (; msdu_idx < num_msdus && rem_len > 0; msdu_idx++) {
		spd_desc_l = &rx_status_desc[msdu_idx];
		skb = spd_desc_l->msdu;
		is_continuation = spd_desc_l->rx_msdu_info.msdu_continuation;
		if (is_continuation) {
			buf_len = DP_RX_BUFFER_SIZE - hal_rx_desc_sz;
		} else {
			ldesc = (struct hal_rx_desc *)skb->data;
			ath12k_wifi7_dp_rx_desc_end_tlv_copy(ab, desc, ldesc);
			ath12k_wifi7_dp_extract_rx_spd_data(hal, spd_desc_l, ldesc, 0);
			buf_len = rem_len;
		}

		if (buf_len > (DP_RX_BUFFER_SIZE - hal_rx_desc_sz)) {
			WARN_ON_ONCE(1);
			dev_kfree_skb_any(skb);
			spd_desc_l->msdu = NULL;
			return -EINVAL;
		}

		skb_put(skb, buf_len + hal_rx_desc_sz);
		skb_pull(skb, hal_rx_desc_sz);
		skb_copy_from_linear_data(skb, skb_put(first, buf_len),
					  buf_len);

		dev_kfree_skb_any(skb);
		spd_desc_l->msdu = NULL;

		rem_len -= buf_len;
		if (!is_continuation)
			break;
	}

	*idx = (msdu_idx == num_msdus) ? (msdu_idx - 1) : msdu_idx;

	return 0;
}

static void ath12k_wifi7_dp_rx_h_csum_offload(struct sk_buff *msdu,
					      struct rx_msdu_desc_info *rx_msdu_info)
{
	msdu->ip_summed = (rx_msdu_info->tcp_udp_chksum_fail ||
				rx_msdu_info->ip_chksum_fail) ?
					CHECKSUM_NONE : CHECKSUM_UNNECESSARY;
}

static void ath12k_wifi7_dp_rx_h_undecap_nwifi(struct ath12k_pdev_dp *dp_pdev,
					       struct sk_buff *msdu,
					       enum hal_encrypt_type enctype,
					       struct ieee80211_rx_status *status,
					       struct hal_rx_desc *desc,
					       bool mesh_ctrl_present, u16 qos_ctl)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	u8 decap_hdr[DP_MAX_NWIFI_HDR_LEN];
	struct ieee80211_hdr *hdr;
	size_t hdr_len;
	u8 *crypto_hdr;
	int len;

	/* pull decapped header */
	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	skb_pull(msdu, hdr_len);

	/*  Rebuild qos header */
	hdr->frame_control |= __cpu_to_le16(IEEE80211_STYPE_QOS_DATA);

	/* Reset the order bit as the HT_Control header is stripped */
	hdr->frame_control &= ~(__cpu_to_le16(IEEE80211_FCTL_ORDER));

	if (mesh_ctrl_present)
		qos_ctl |= IEEE80211_QOS_CTL_MESH_CONTROL_PRESENT;

	/* TODO: Add other QoS ctl fields when required */

	/* copy decap header before overwriting for reuse below */
	memcpy(decap_hdr, hdr, hdr_len);

	/* Rebuild crypto header for mac80211 use */
	if (!(status->flag & RX_FLAG_IV_STRIPPED)) {
		len = ath12k_dp_rx_crypto_param_len(dp_pdev, enctype);
		crypto_hdr = skb_push(msdu, len);

		ath12k_wifi7_dp_rx_desc_get_crypto_header(ab, desc,
							  crypto_hdr, enctype);
	}

	memcpy(skb_push(msdu,
			IEEE80211_QOS_CTL_LEN), &qos_ctl,
			IEEE80211_QOS_CTL_LEN);
	memcpy(skb_push(msdu, hdr_len), decap_hdr, hdr_len);
}

static void ath12k_get_dot11_hdr_from_rx_desc(struct ath12k_pdev_dp *dp_pdev,
					      struct sk_buff *msdu,
					      struct ieee80211_rx_status *status,
					      enum hal_encrypt_type enctype,
					      bool mesh_ctrl_present,
					      struct hal_rx_desc *rx_desc,
					      bool is_mcbc, u16 qos_ctl)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	size_t hdr_len, crypto_len;
	struct ieee80211_hdr hdr;
	u8 *crypto_hdr;

	ath12k_wifi7_dp_rx_desc_get_dot11_hdr(ab, rx_desc, &hdr);
	hdr_len = ieee80211_hdrlen(hdr.frame_control);

	if (!(status->flag & RX_FLAG_IV_STRIPPED)) {
		crypto_len = ath12k_dp_rx_crypto_param_len(dp_pdev, enctype);
		crypto_hdr = skb_push(msdu, crypto_len);
		ath12k_wifi7_dp_rx_desc_get_crypto_header(ab, rx_desc,
							  crypto_hdr, enctype);
	}

	skb_push(msdu, hdr_len);
	memcpy(msdu->data, &hdr, min(hdr_len, sizeof(hdr)));

	if (is_mcbc)
		status->flag &= ~RX_FLAG_PN_VALIDATED;

	/* Add QOS header */
	if (ieee80211_is_data_qos(hdr.frame_control)) {
		struct ieee80211_hdr *qhdr = (struct ieee80211_hdr *)msdu->data;

		if (mesh_ctrl_present)
			qos_ctl |= IEEE80211_QOS_CTL_MESH_CONTROL_PRESENT;

		/* TODO: Add other QoS ctl fields when required */
		memcpy(ieee80211_get_qos_ctl(qhdr),
		       &qos_ctl, IEEE80211_QOS_CTL_LEN);
	}
}

static void ath12k_wifi7_dp_rx_h_undecap_eth(struct ath12k_pdev_dp *dp_pdev,
					     struct sk_buff *msdu,
					     enum hal_encrypt_type enctype,
					     struct ieee80211_rx_status *status,
					     bool mesh_ctrl_present,
					     struct hal_rx_desc *desc,
					     bool is_mcbc, u16 tid)
{
	struct ieee80211_hdr *hdr;
	struct ethhdr *eth;
	u8 da[ETH_ALEN];
	u8 sa[ETH_ALEN];
	struct ath12k_dp_rx_rfc1042_hdr rfc = {0xaa, 0xaa, 0x03, {0x00, 0x00, 0x00}};

	eth = (struct ethhdr *)msdu->data;
	ether_addr_copy(da, eth->h_dest);
	ether_addr_copy(sa, eth->h_source);
	rfc.snap_type = eth->h_proto;
	skb_pull(msdu, sizeof(*eth));
	memcpy(skb_push(msdu, sizeof(rfc)), &rfc,
	       sizeof(rfc));
	ath12k_get_dot11_hdr_from_rx_desc(dp_pdev, msdu, status, enctype,
					  mesh_ctrl_present, desc, is_mcbc, tid);

	/* original 802.11 header has a different DA and in
	 * case of 4addr it may also have different SA
	 */
	hdr = (struct ieee80211_hdr *)msdu->data;
	ether_addr_copy(ieee80211_get_DA(hdr), da);
	ether_addr_copy(ieee80211_get_SA(hdr), sa);
	status->flag &= ~RX_FLAG_8023;
}

static int ath12k_wifi7_dp_rx_h_undecap(struct ath12k_pdev_dp *dp_pdev,
					struct sk_buff *msdu,
					struct hal_rx_desc *desc,
					enum hal_encrypt_type enctype,
					struct ieee80211_rx_status *status,
					bool decrypted, bool is_4addr_sta,
					struct rx_msdu_desc_info *rx_msdu_info,
					struct rx_tlv_info_1 *tlv_info,
					struct ath12k_dp_peer *peer, u16 peer_id, u16 tid)
{
	struct ethhdr *ehdr;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct hal_rx_desc_data rx_desc_data = {0};
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_vif *ahvif;
	u32 pkt_reason;
	u8 is_mcbc;

	ath12k_wifi7_dp_extract_rx_desc_data(dp, &rx_desc_data, desc, desc);
	switch (tlv_info->decap) {
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		pkt_reason = ATH_RX_NATIVE_WIFI_PKTS;
		ath12k_wifi7_dp_rx_h_undecap_nwifi(dp_pdev, msdu, enctype, status, desc,
						   tlv_info->mesh_ctrl_present, tid);
		break;
	case DP_RX_DECAP_TYPE_RAW:
		pkt_reason = ATH_RX_RAW_PKTS;
		ath12k_dp_rx_h_undecap_raw(dp_pdev, msdu, desc, enctype, status,
					   decrypted, peer_id, rx_msdu_info->first_msdu,
					   rx_msdu_info->last_msdu);
		break;
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		pkt_reason = ATH_RX_ETH_PKTS;
		ehdr = (struct ethhdr *)msdu->data;

		/* PN for multicast packets are not validate in HW,
		 * so skip 802.3 rx path
		 * Also, fast_rx expects the STA to be authorized, hence
		 * eapol packets are sent in slow path.
		 */
		status->flag |= RX_FLAG_8023;

		is_mcbc = rx_msdu_info->ra_is_mcbc;
		/* mac80211 allows fast path only for authorized STA */
		if (ehdr->h_proto == cpu_to_be16(ETH_P_PAE) ||
		    enctype == HAL_ENCRYPT_TYPE_TKIP_MIC) {
			ath12k_wifi7_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype, status,
							 tlv_info->mesh_ctrl_present,
							 desc, is_mcbc, tid);
			break;
		}


		/* Drop the 3addr da_mcbc packets for 4addr sta as it will
		 * double the packet for connected clients.
		 */
		if (is_4addr_sta && rx_msdu_info->da_is_mcbc &&
		    !rx_msdu_info->to_ds) {
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				rcu_read_lock();
				link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
										     peer_id);
				if (link_peer) {
					ahvif = ath12k_vif_to_ahvif(link_peer->vif);
					ath12k_tid_drop_rx_stats(ahvif, rx_desc_data.tid, msdu->len,
								 ATH_RX_3AADR_DUP);
				}
				rcu_read_unlock();
			}
			return -1;
		}

		if (rx_msdu_info->fr_ds && rx_msdu_info->to_ds && peer && !peer->use_4addr) {
			ath12k_wifi7_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype, status,
							 tlv_info->mesh_ctrl_present,
							 desc, is_mcbc, tid);
			break;
		}


		/* PN for mcast packets will be validated in mac80211;
		 * remove eth header and add 802.11 header.
		 */
		if (is_mcbc && decrypted)
			ath12k_wifi7_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype,
							 status,
							 tlv_info->mesh_ctrl_present,
							 desc, is_mcbc, tid);
		break;
	case DP_RX_DECAP_TYPE_8023:
		pkt_reason = ATH_RX_8023_PKTS;
		/* Note that decap_format = 2 indicates that the decapped
		 * packet is either Ethernet 2 (DIX)  or 802.3 (uses SNAP/LLC).
		 * So, decap_format = 2 or 3 is all the same.
		 */
		status->flag |= RX_FLAG_8023;
		break;
	}
	dp_pdev->wmm_stats.total_wmm_rx_pkts[dp_pdev->wmm_stats.rx_type]++;

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
							     peer_id);
	if (link_peer) {
		ahvif = ath12k_vif_to_ahvif(link_peer->vif);
		ahvif->wmm_stats.total_wmm_rx_pkts[ahvif->wmm_stats.rx_type]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			ath12k_tid_rx_stats(ahvif, rx_desc_data.tid, msdu->len,
					    pkt_reason);
		}

	}
	rcu_read_unlock();
	return 0;
}

static int ath12k_wifi7_dp_rx_h_mpdu(struct ath12k_pdev_dp *dp_pdev,
				     struct sk_buff *msdu,
				     struct hal_rx_desc *rx_desc,
				     struct ieee80211_rx_status *rx_status,
				     struct rx_msdu_desc_info *rx_msdu_info,
				     struct rx_mpdu_desc_info *rx_mpdu_info,
				     struct rx_tlv_info_1 *tlv_info,
				     u32 err_bitmap, bool *fast_rx,
				     struct ath12k_dp_peer *peer)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	enum hal_encrypt_type enctype;
	bool is_decrypted = false;
	bool is_4addr_sta = false;
	struct ieee80211_hdr *hdr;
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_dp_link_peer *link_peer;
	struct  ath12k_vif *ahvif;
	u8 tid;
	int ret = 0;
	u16 peer_id;

	peer_id = rx_mpdu_info->flow_info.peer_id;
	tid = rx_mpdu_info->tid;
	/* PN for multicast packets will be checked in mac80211 */

	ath12k_wifi7_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	if (likely(peer)) {
		msdu->dev = peer->dev;
		if (unlikely(peer->mscs_session_exists)) {
			if (tlv_info->decap == DP_RX_DECAP_TYPE_ETHERNET2_DIX) {
				/* Get the FSE metadata to check if flow entry
				 * has been programmed and if yes, check if
				 * metadata has the MSCS tag. If it has, do not
				 * classify the UL packet.
				 */
				dp->hal->hal_ops->rx_desc_get_fse_info(rx_desc,
						rx_mpdu_info);
				/* Update skb priority with the tid received
				 * in rx_mpdu_info.
				 * This has to be done per packet since
				 * a packet from the same flow can have
				 * different tid values
				 */
				msdu->priority = tid;
				/** Check if the flow has been timed out
				 * or if the flow is invalid
				 * If the flow is invalid, check for five-tuple info and
				 * then program the FST entry with MSCS tag
				 * If tid is 0, which is the default case,
				 * do not program the FSE entry and MSCS rule
				 */
				if (tid && !rx_mpdu_info->flow_idx_timeout &&
				    !(rx_mpdu_info->flow_info.flow_metadata &
				    ATH12K_RX_FSE_FLOW_MSCS_RULE_PROGRAMMED))
					ath12k_dp_rx_classify_mscs(dp->ab, peer, msdu, tid);
			}
		}

		if (likely(*fast_rx &&
		    ath12k_wifi7_dp_rx_check_fast_rx(dp, msdu, rx_msdu_info,
							 tlv_info, peer))) {
			if (peer->ppe_vp_num) {
				dp->hal->hal_ops->rx_desc_get_fse_info(rx_desc,
								       rx_mpdu_info);
				/**
				 * Check if flow has been timed out
				 * or if the flow is invalid
				 */
				if (!rx_mpdu_info->flow_idx_timeout &&
				    !rx_mpdu_info->flow_idx_invalid &&
				    rx_mpdu_info->flow_info.flow_metadata) {
					/**
					 * Based on the metadata tag, send the
					 * packet to the PPE driver if ppe_vp_num
					 * is valid.
					 */
					if ((rx_mpdu_info->flow_info.flow_metadata &
					    ATH12K_RX_FSE_FLOW_MATCH_USE_PPE)) {
						if (peer->dev->offload_ops->recv(peer->dev, msdu))
							return 0;
					}
				}
			}

#ifdef CONFIG_IO_COHERENCY
			prefetch(skb_shinfo(msdu));
#endif
			msdu->protocol = eth_type_trans(msdu, peer->dev);
			netif_receive_skb(msdu);
			return ret;
		}

		/* restting 4addr da mcbc packets as in 4addr mcbc packets are
		 * unicast only and sta send sends unicast pkts only.
		 */
		rx_msdu_info->ra_is_mcbc =
				rx_msdu_info->da_is_mcbc && !peer->is_reset_mcbc;

		is_4addr_sta = peer->vdev_type_4addr & BIT(NL80211_IFTYPE_STATION);
		if (rx_msdu_info->ra_is_mcbc)
			enctype = peer->sec_type_grp;
		else
			enctype = peer->sec_type;

		rx_tid = &peer->rx_tid[tid];
		dp_pdev->wmm_stats.rx_type =
			ath12k_tid_to_ac(rx_tid->tid >
					 ATH12K_DSCP_PRIORITY ? 0: rx_tid->tid);
		dp_pdev->wmm_stats.total_wmm_rx_pkts[dp_pdev->wmm_stats.rx_type]++;

		rcu_read_lock();
		link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								     peer_id);
		if (link_peer) {
			ahvif = ath12k_vif_to_ahvif(link_peer->vif);
			ahvif->wmm_stats.rx_type = dp_pdev->wmm_stats.rx_type;
			ahvif->wmm_stats.total_wmm_rx_pkts[ahvif->wmm_stats.rx_type]++;
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev))
				ath12k_tid_rx_stats(ahvif, tid, msdu->len,
						    ATH_RX_SFE_PKTS);
		}
		rcu_read_unlock();

		ath12k_wifi7_dp_rx_update_ppe_msdu_mark(dp->ab, peer, msdu,
							rx_mpdu_info, rx_desc);

	} else {
		enctype = HAL_ENCRYPT_TYPE_OPEN;
	}

	*fast_rx = false;

	tlv_info->is_decrypted = ath12k_hal_rx_h_is_decrypted(dp->hal, rx_desc);

	if (enctype != HAL_ENCRYPT_TYPE_OPEN && !err_bitmap)
		is_decrypted = tlv_info->is_decrypted;

	/* Clear per-MPDU flags while leaving per-PPDU flags intact */
	rx_status->flag &= ~(RX_FLAG_FAILED_FCS_CRC |
			     RX_FLAG_MMIC_ERROR |
			     RX_FLAG_DECRYPTED |
			     RX_FLAG_IV_STRIPPED |
			     RX_FLAG_MMIC_STRIPPED);

	if (err_bitmap & HAL_RX_MPDU_ERR_FCS)
		rx_status->flag |= RX_FLAG_FAILED_FCS_CRC;
	if (err_bitmap & HAL_RX_MPDU_ERR_TKIP_MIC)
		rx_status->flag |= RX_FLAG_MMIC_ERROR;

	if (is_decrypted) {
		rx_status->flag |= RX_FLAG_DECRYPTED | RX_FLAG_MMIC_STRIPPED;

		if (rx_msdu_info->ra_is_mcbc)
			rx_status->flag |= RX_FLAG_MIC_STRIPPED |
					RX_FLAG_ICV_STRIPPED;
		else
			rx_status->flag |= RX_FLAG_IV_STRIPPED |
					   RX_FLAG_PN_VALIDATED;
	}

	ret = ath12k_wifi7_dp_rx_h_undecap(dp_pdev, msdu, rx_desc, enctype, rx_status,
					   is_decrypted, is_4addr_sta, rx_msdu_info,
					   tlv_info, peer, peer_id, tid);

	if (!is_decrypted || rx_msdu_info->ra_is_mcbc)
		return ret;

	if (tlv_info->decap != DP_RX_DECAP_TYPE_ETHERNET2_DIX) {
		hdr = (void *)msdu->data;
		hdr->frame_control &= ~__cpu_to_le16(IEEE80211_FCTL_PROTECTED);
	}

	return ret;
}

static bool ath12k_wifi7_dp_rx_h_rate(struct ath12k_pdev_dp *dp_pdev,
				      struct ieee80211_rx_status *rx_status,
				      struct rx_tlv_info_1 *tlv_info)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ieee80211_supported_band *sband;
	enum rx_msdu_start_pkt_type pkt_type = tlv_info->pkt_type;
	u8 bw = tlv_info->bw, sgi = tlv_info->sgi;
	u8 rate_mcs = tlv_info->rate_mcs, nss = tlv_info->nss;
	bool is_cck;

	switch (pkt_type) {
	case RX_MSDU_START_PKT_TYPE_11A:
	case RX_MSDU_START_PKT_TYPE_11B:
		if (rx_status->band == NUM_NL80211_BANDS) {
			ath12k_warn(dp->ab, "Received with invalid band");
			WARN_ON_ONCE(1);
			return true;
		}
		is_cck = (pkt_type == RX_MSDU_START_PKT_TYPE_11B);
		sband = &dp_pdev->ar->mac.sbands[rx_status->band];
		rx_status->rate_idx = ath12k_mac_hw_rate_to_idx(sband, rate_mcs,
								is_cck);
		break;
	case RX_MSDU_START_PKT_TYPE_11N:
		rx_status->encoding = RX_ENC_HT;
		if (rate_mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in HT mode %d\n",
				     rate_mcs);
			return true;
		}
		rx_status->rate_idx = rate_mcs + (8 * (nss - 1));
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AC:
		rx_status->encoding = RX_ENC_VHT;
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in VHT mode %d\n",
				     rate_mcs);
			return true;
		}
		rx_status->nss = nss;
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AX:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in HE mode %d\n",
				    rate_mcs);
			return true;
		}
		rx_status->encoding = RX_ENC_HE;
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11BE:
		rx_status->rate_idx = rate_mcs;

		if (rate_mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in EHT mode %d\n",
				    rate_mcs);
			return true;
		}

		rx_status->encoding = RX_ENC_EHT;
		rx_status->nss = nss;
		rx_status->eht.gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(sgi);
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	default:
		break;
	}

	return false;
}

bool ath12k_wifi7_dp_rx_h_ppdu(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_rx_status *rx_status,
			       struct rx_tlv_info_1 *tlv_info,
			       u8 err_rel_src)
{
	u8 channel_num;
	u32 center_freq, meta_data;
	struct ieee80211_channel *channel;
	struct ath12k *ar = dp_pdev->ar;

	rx_status->freq = 0;
	rx_status->rate_idx = 0;
	rx_status->nss = 0;
	rx_status->encoding = RX_ENC_LEGACY;
	rx_status->bw = RATE_INFO_BW_20;
	rx_status->enc_flags = 0;

	rx_status->flag |= RX_FLAG_NO_SIGNAL_VAL;

	meta_data = tlv_info->freq;
	channel_num = meta_data;
	center_freq = meta_data >> 16;

	rx_status->band = NUM_NL80211_BANDS;

	if (center_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    center_freq <= ATH12K_MAX_6GHZ_FREQ) {
		rx_status->band = NL80211_BAND_6GHZ;
		rx_status->freq = center_freq;
	} else if (center_freq >= ATH12K_MIN_2GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_2GHZ_FREQ) {
		rx_status->band = NL80211_BAND_2GHZ;
	} else if (center_freq >= ATH12K_MIN_5GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_5GHZ_FREQ) {
		rx_status->band = NL80211_BAND_5GHZ;
	}

	if (unlikely(rx_status->band == NUM_NL80211_BANDS ||
		!dp_pdev->hw->wiphy->bands[rx_status->band])) {
		if (err_rel_src == HAL_WBM_REL_SRC_MODULE_REO ||
		    err_rel_src == HAL_WBM_REL_SRC_MODULE_RXDMA) {
			if(ath12k_debug_critical)
				WARN_ON_ONCE(1);
			return true;
		}
		else {
			ath12k_err(ar->ab,
				   "sband is NULL for status band %d channel_num %d center_freq %d pdev_id %d\n",
				   rx_status->band, channel_num, center_freq,
				   ar->pdev_idx);
		}

		spin_lock_bh(&ar->data_lock);
		channel = ar->rx_channel;
		if (channel) {
			rx_status->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		} else {
			ath12k_err(ar->ab, "unable to determine channel, band for rx packet");
		}
		spin_unlock_bh(&ar->data_lock);

		rx_status->freq = ieee80211_channel_to_frequency(channel_num,
								 rx_status->band);
		goto h_rate;
	}

	if (rx_status->band != NL80211_BAND_6GHZ)
		rx_status->freq = ieee80211_channel_to_frequency(channel_num,
								 rx_status->band);

h_rate:
	return ath12k_wifi7_dp_rx_h_rate(dp_pdev, rx_status, tlv_info);
}

static bool ath12k_dp_rx_check_nwifi_hdr_len_valid(struct ath12k_dp *dp,
						   u8 decap_type,
						   struct sk_buff *msdu)
{
	struct ieee80211_hdr *hdr;
	u32 hdr_len;

	if (decap_type != DP_RX_DECAP_TYPE_NATIVE_WIFI)
		return true;

	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);

	if ((likely(hdr_len <= DP_MAX_NWIFI_HDR_LEN)))
		return true;

	dp->device_stats.invalid_rbm++;

	if (ath12k_rx_nwifi_err_dump)
		WARN_ON_ONCE(1);

	return false;
}

static enum ath12k_dp_rx_error
ath12k_wifi7_dp_rx_process_msdu(struct ath12k_pdev_dp *dp_pdev,
				struct sk_buff *msdu,
				struct hal_rx_spd_data *rx_status_desc,
				int num_msdus, int *idx,
				struct ieee80211_rx_status *rx_status,
				bool *fast_rx, int ring_id)
{
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct hal_rx_desc *rx_desc;
	struct hal_rx_spd_data *spd_desc_l;
	u8 l3_pad_bytes;
	u16 msdu_len;
	int ret;
	struct ath12k_hal *hal = dp->hal;
	u32 hal_rx_desc_sz = hal->hal_desc_sz;
	struct ath12k_dp_peer *peer = NULL;
	u8 link_id = 0;
	u16 peer_id = 0;
	enum ath12k_dp_rx_error drop_reason = DP_RX_ERR_DROP_MISC;
	int msdu_idx = *idx;

	spd_desc_l = &rx_status_desc[msdu_idx];
	rx_msdu_info = &spd_desc_l->rx_msdu_info;
	rx_mpdu_info = &spd_desc_l->rx_mpdu_info;
	tlv_info = &spd_desc_l->tlv_info;
	rx_desc = (struct hal_rx_desc *)msdu->data;
	peer_id = rx_mpdu_info->flow_info.peer_id;

	ath12k_wifi7_dp_extract_rx_spd_data(hal, spd_desc_l, rx_desc, 0);

	msdu_len = rx_msdu_info->msdu_length;
	l3_pad_bytes = rx_msdu_info->l3_header_padding_msb ? 2 : 0;

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (peer) {
		link_id = peer->hw_links[spd_desc_l->src_link_id];
		DP_PEER_STATS_PKT_LEN(peer, rx, ring_id, recv_from_reo, link_id,
				      1, msdu_len);

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
				ath12k_dp_rx_update_peer_msdu_stats(peer,
								    rx_msdu_info,
								    rx_mpdu_info,
								    link_id,
								    ring_id);
			}
		}
	} else {
		DP_DEVICE_STATS_INC(dp, rx.rx_err[DP_RX_ERR_DROP_INV_PEER][ring_id], 1);
	}

	if (unlikely(rx_mpdu_info->fragment_flag)) {
		skb_pull(msdu, hal_rx_desc_sz);
	} else if (likely(!rx_msdu_info->msdu_continuation)) {
		if (unlikely((msdu_len + hal_rx_desc_sz) > DP_RX_BUFFER_SIZE)) {
			ath12k_dbg(dp->ab, ATH12K_DBG_DATA, "invalid msdu len %u\n",
				   msdu_len);
			ath12k_dbg_dump(dp->ab, ATH12K_DBG_DATA, NULL, "", rx_desc,
					sizeof(*rx_desc));
			drop_reason = DP_RX_ERR_DROP_INV_MSDU_LEN;
			goto free_out;
		}
		skb_put(msdu, hal_rx_desc_sz + l3_pad_bytes + msdu_len);
		skb_pull(msdu, hal_rx_desc_sz + l3_pad_bytes);
	} else {
		ret = ath12k_wifi7_dp_rx_msdu_coalesce(dp, rx_status_desc,
						       msdu, l3_pad_bytes, msdu_len,
						       rx_desc, &msdu_idx, num_msdus);

		if (ret) {
			ath12k_warn(dp,
				    "failed to coalesce msdu rx buffer%d\n", ret);
			drop_reason = DP_RX_ERR_DROP_MSDU_COALESCE_FAIL;
			goto free_out;
		}

		spd_desc_l = &rx_status_desc[msdu_idx];
		tlv_info = &spd_desc_l->tlv_info;
		*idx = msdu_idx;
	}

	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, tlv_info->decap,
							     msdu))) {
		drop_reason = DP_RX_ERR_DROP_NWIFI_HDR_LEN_INVALID;
		goto free_out;
	}

	ret = ath12k_wifi7_dp_rx_h_mpdu(dp_pdev, msdu, rx_desc, rx_status,
					rx_msdu_info, rx_mpdu_info, tlv_info, 0,
					fast_rx, peer);
	if (unlikely(ret)) {
		drop_reason = DP_RX_ERR_DROP_H_MPDU;
		goto free_out;
	}

#ifndef CONFIG_IO_COHERENCY
	if (likely(msdu_idx + 1 < num_msdus)) {
		struct hal_rx_spd_data *spd_desc_next =
			&rx_status_desc[msdu_idx + 1];

		prefetch(spd_desc_next->vaddr);
		prefetch(&spd_desc_next->vaddr[64]);
		prefetch(&spd_desc_next->vaddr[128]);
	}
#endif

	if (likely(*fast_rx)) {
		DP_PEER_STATS_PKT_LEN(peer, rx, ring_id, sent_to_stack_fast, link_id, 1, msdu_len);
		return DP_RX_SUCCESS;
	}

	ath12k_wifi7_dp_extract_rx_spd_data(hal, spd_desc_l, rx_desc, 1);

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, rx_status, tlv_info,
					HAL_WBM_REL_SRC_MODULE_REO);
	if (unlikely(ret)) {
		drop_reason = DP_RX_ERR_DROP_H_PPDU;
		goto free_out;
	}

	rx_status->flag |= RX_FLAG_SKIP_MONITOR | RX_FLAG_DUP_VALIDATED;
	DP_PEER_STATS_PKT_LEN(peer, rx, ring_id, sent_to_stack, link_id, 1, msdu_len);

	return DP_RX_SUCCESS;

free_out:
	return drop_reason;
}

static void ath12k_soc_dp_rx_stats(struct ath12k_dp *dp, bool is_mcbc, int ring_id)
{
	if (is_mcbc)
		dp->device_stats.non_fast_mcast_rx[ring_id][dp->device_id]++;
	else
		dp->device_stats.non_fast_unicast_rx[ring_id][dp->device_id]++;
}

static void
ath12k_wifi7_dp_rx_process_received_packets(struct ath12k_dp *dp,
					    struct napi_struct *napi,
					    struct hal_rx_spd_data *rx_status_desc,
					    int ring_id, int num_msdus)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ieee80211_rx_status rx_status = {0};
	struct sk_buff *msdu;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	struct ath12k_base *partner_ab;
	struct ath12k_dp *partner_dp;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id, pdev_id;
	int msdu_idx;
	bool fast_rx = true;
	enum ath12k_dp_rx_error ret;

	rcu_read_lock();

	for (msdu_idx = 0; msdu_idx < num_msdus; msdu_idx++) {
		struct hal_rx_spd_data *spd_desc_l = &rx_status_desc[msdu_idx];

		msdu = spd_desc_l->msdu;

#ifndef CONFIG_IO_COHERENCY
		prefetch(msdu);
		prefetch(&msdu->_skb_refdst);
		prefetch(&msdu->__pkt_type_offset);
#endif
		if (likely(msdu_idx + 1 < num_msdus)) {
			struct hal_rx_spd_data *spd_desc_next =
				&rx_status_desc[msdu_idx + 1];
			struct sk_buff *next_msdu = spd_desc_next->msdu;
#ifdef CONFIG_IO_COHERENCY
			{
				u8 *vaddr = spd_desc_next->vaddr;

				prefetch(vaddr);
				prefetch(&vaddr[64]);
				prefetch(&vaddr[128]);
				prefetch(next_msdu);
				prefetch(&next_msdu->_skb_refdst);
				prefetch(&next_msdu->__pkt_type_offset);
			}
#endif
			prefetch(&next_msdu->head);
		}

		hw_link_id = spd_desc_l->src_link_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp,
						    hw_links[hw_link_id].device_id);
		pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
						      hw_links[hw_link_id].pdev_idx);
		partner_ab = partner_dp->ab;
		if (unlikely(!rcu_dereference(partner_ab->pdevs_active[pdev_id]))) {
			ath12k_dp_rx_skb_free(msdu, dp, ring_id,
					      DP_RX_ERR_DROP_PDEV_NA);
			spd_desc_l->msdu = NULL;
			continue;
		}

		dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
		if (unlikely(!dp_pdev)) {
			ath12k_dp_rx_skb_free(msdu, dp, ring_id,
					      DP_RX_ERR_DROP_PDEV_NA);
			spd_desc_l->msdu = NULL;
			continue;
		}

		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			rcu_read_lock();
			link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
									     spd_desc_l->rx_mpdu_info.flow_info.peer_id);
			if (link_peer) {
				ahvif = ath12k_vif_to_ahvif(link_peer->vif);
				ath12k_tid_rx_stats(ahvif, spd_desc_l->rx_mpdu_info.tid,
						    msdu->len,
						    ATH_RX_SFE_PKTS);
			}
			rcu_read_unlock();
		}

		ret = ath12k_wifi7_dp_rx_process_msdu(dp_pdev, msdu, rx_status_desc,
						      num_msdus, &msdu_idx,
						      &rx_status, &fast_rx, ring_id);
		if (unlikely(ret)) {
			ath12k_dbg(partner_ab, ATH12K_DBG_DATA,
				   "Unable to process msdu %d", ret);
			ath12k_dp_rx_skb_free(msdu, dp, ring_id, ret);
			spd_desc_l->msdu = NULL;
			continue;
		}

		if (unlikely(!fast_rx)) {
			bool is_mcbc = spd_desc_l->rx_msdu_info.da_is_mcbc;

			if (!partner_dp->stats_disable)
				ath12k_soc_dp_rx_stats(partner_dp, is_mcbc, ring_id);

			ath12k_dp_rx_deliver_msdu(dp_pdev, napi, msdu, &rx_status,
						  spd_desc_l->src_link_id,
						  is_mcbc,
						  spd_desc_l->rx_mpdu_info.flow_info.peer_id,
						  spd_desc_l->rx_mpdu_info.tid);
		} else {
			partner_dp->device_stats.fast_rx[ring_id][partner_dp->device_id]++;
		}
	}

	rcu_read_unlock();
}

static u16 ath12k_wifi7_dp_rx_get_peer_id(struct ath12k_base *ab,
					  enum ath12k_peer_metadata_version ver,
					  __le32 peer_metadata)
{
	switch (ver) {
	default:
		ath12k_warn(ab, "Unknown peer metadata version: %d", ver);
		fallthrough;
	case ATH12K_PEER_METADATA_V0:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V0_PEER_ID);
	case ATH12K_PEER_METADATA_V1:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1_PEER_ID);
	case ATH12K_PEER_METADATA_V1A:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1A_PEER_ID);
	case ATH12K_PEER_METADATA_V1B:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1B_PEER_ID);
	}
}

int ath12k_wifi7_dp_rx_process(struct ath12k_dp *dp, int ring_id,
			       struct napi_struct *napi, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct list_head rx_desc_used_list[ATH12K_MAX_SOCS];
	struct list_head rx_desc_sg_list;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	int num_buffs_reaped[ATH12K_MAX_SOCS] = {};
	struct ath12k_rx_desc_info *desc_info, *rx_desc, *temp_rx_desc;
	struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
	int cpu_id = smp_processor_id();
	struct hal_rx_spd_data *rx_status_desc =
		(struct hal_rx_spd_data *)dp_hw_grp->rx_status_buf[cpu_id];
	struct hal_reo_dest_ring *desc;
	struct hal_reo_dest_ring *next_desc;
	struct ath12k_dp *partner_dp;
	struct sk_buff_head local_msdu_list;
	int total_msdu_reaped = 0;
	u8 device_id, hw_link_id;
	int pdev_id;
	struct hal_srng *srng;
	struct sk_buff *msdu;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_pdev_dp *dp_pdev;
	bool done = true;
	u64 desc_va, next_desc_va;
	u32 last_tp, first_msdu_tp;
	void *next_desc_info;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
#endif
	int incomplete_msdu_chain = 0;

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++)
		INIT_LIST_HEAD(&rx_desc_used_list[device_id]);

	INIT_LIST_HEAD(&rx_desc_sg_list);
	__skb_queue_head_init(&local_msdu_list);

	srng = &dp->hal->srng_list[dp->reo_dst_ring[ring_id].ring_id];

	first_msdu_tp = __ath12k_hal_srng_access_begin(srng);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = __ath12k_hal_srng_dst_num_free(srng, false);
	if (unlikely(!valid_entries)) {
		ath12k_hal_srng_access_end(ab, srng);
		goto exit;
	}
	ath12k_hal_srng_dst_invalidate_entry(dp, srng, valid_entries);
#endif
	while ((desc = __ath12k_hal_srng_dst_get_next_cached_entry(srng, &last_tp))) {
		struct rx_mpdu_desc_info *mpdu_info;
		struct hal_rx_spd_data *spd_desc_l = &rx_status_desc[total_msdu_reaped];

		next_desc = __ath12k_hal_srng_dst_peek(srng);
		if (next_desc) {
			next_desc_va = ((u64)le32_to_cpu(next_desc->buf_va_hi) << 32 |
						le32_to_cpu(next_desc->buf_va_lo));
			next_desc_info = (void *)((unsigned long)next_desc_va);

			if (next_desc_info)
				prefetch(next_desc_info);
		}

		desc_va = ((u64)le32_to_cpu(desc->buf_va_hi) << 32 |
			   le32_to_cpu(desc->buf_va_lo));
		desc_info = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);
		if (likely(desc_info))
			prefetch(desc_info);

		spd_desc_l->info1 = le64_to_cpu(desc->info1);
		spd_desc_l->info2 = le32_to_cpu(desc->info2);
		spd_desc_l->info0 = le32_to_cpu(desc->info0);

		device_id = hw_links[spd_desc_l->src_link_id].device_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		if (unlikely(!partner_dp)) {
			if (desc_info && desc_info->skb) {
				ath12k_dp_rx_skb_free(desc_info->skb, dp,
						      ring_id,
						      DP_RX_ERR_DROP_PARTNER_DP_NA);
				desc_info->skb = NULL;
			}

			continue;
		}

		if (unlikely(!desc_info)) {
			DP_DEVICE_STATS_INC(dp, rx.rx_err[DP_RX_ERR_GET_SW_DESC_FROM_CK][ring_id], 1);
			/* retry manual desc retrieval */
			u32 cookie = le32_get_bits(desc->buf_addr_info.info1,
						   BUFFER_ADDR_INFO1_SW_COOKIE);

			desc_info = ath12k_dp_get_rx_desc(partner_dp, cookie);
			if (!desc_info) {
				DP_DEVICE_STATS_INC(dp, rx.rx_err[DP_RX_ERR_GET_SW_DESC][ring_id], 1);
				ath12k_warn(ab, "Unable to retrieve rx_desc for va 0x%lx",
						(unsigned long)desc_va);
				continue;
			}
		}

		num_buffs_reaped[device_id]++;

		dp->device_stats.reo_rx[ring_id][dp->device_id]++;

		mpdu_info = &spd_desc_l->rx_mpdu_info;

		mpdu_info->flow_info.peer_id =
			ath12k_wifi7_dp_rx_get_peer_id(ab, dp->peer_metadata_ver,
						       mpdu_info->peer_meta_data);

		if (unlikely(desc_info->magic != ATH12K_DP_RX_DESC_MAGIC))
			ath12k_warn(ab, "Check HW CC implementation");

		mpdu_info->fragment_flag = desc_info->is_frag;
		desc_info->is_frag = 0;

		ath12k_core_dmac_inv_range_no_dsb(desc_info->vaddr,
						  desc_info->vaddr + DP_RX_BUFFER_SIZE);

		spd_desc_l->vaddr = desc_info->vaddr;
		msdu = desc_info->skb;

		hw_link_id = le32_get_bits(desc->info0,
					   HAL_REO_DEST_RING_INFO0_SRC_LINK_ID);
		pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params,
						      hw_links[hw_link_id].pdev_idx);

		dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			rcu_read_lock();
			link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
									     mpdu_info->flow_info.peer_id);
			if (link_peer) {
				ahvif = ath12k_vif_to_ahvif(link_peer->vif);
				ath12k_tid_rx_stats(ahvif, mpdu_info->tid, msdu->len,
						    ATH_RX_REO_PKTS);
			}
			rcu_read_unlock();
		}

		/*
		 * Create local lists for rx_desc_info descriptors
		 * to temporarily store in the scatter gather case.
		 *
		 * Once msdu_continuation is seen to be 0, append the
		 * local list with the actual list that are taken further
		 * for processing.
		 *
		 * If the list is seen to be incomplete in the current budget,
		 * discard the local list and set the tp back to the start of
		 * the beginning of the unprocessed sg list, which will then
		 * be fetched and processed again in the next NAPI cycle.
		 *
		 * For the regular non-sg msdus, we will add the datastructures
		 * to the corresponding list directly, without using local list.
		 *
		 */

		if (likely(!spd_desc_l->rx_msdu_info.msdu_continuation)) {
			if (unlikely(!done)) {
				list_for_each_entry_safe(rx_desc, temp_rx_desc,
							 &rx_desc_sg_list, list) {
					rx_desc->skb = NULL;
				}

				list_splice_tail_init(&rx_desc_sg_list,
						      &rx_desc_used_list[device_id]);
				incomplete_msdu_chain = 0;
			}

			desc_info->skb = NULL;
			list_add_tail(&desc_info->list, &rx_desc_used_list[device_id]);
			spd_desc_l->msdu = msdu;
			first_msdu_tp = last_tp;
			done = true;
		} else {
			list_add_tail(&desc_info->list, &rx_desc_sg_list);
			spd_desc_l->msdu = msdu;
			incomplete_msdu_chain++;
			done = false;
		}

		if (++total_msdu_reaped >= budget)
			break;
	}

	ath12k_core_dsb();

	__ath12k_hal_srng_update_tp(srng, first_msdu_tp);

	__ath12k_hal_srng_access_end(ab, srng);

	total_msdu_reaped -= incomplete_msdu_chain;

	if (!total_msdu_reaped)
		goto exit;

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
		if (!num_buffs_reaped[device_id])
			continue;

		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		rx_ring = &partner_dp->rx_refill_buf_ring;

		ath12k_dp_rx_bufs_replenish(partner_dp, rx_ring,
					    &rx_desc_used_list[device_id]);
	}

	ath12k_wifi7_dp_rx_process_received_packets(dp, napi, rx_status_desc,
						    ring_id, total_msdu_reaped);

exit:
	return total_msdu_reaped;
}

static int ath12k_wifi7_dp_rx_h_verify_tkip_mic(struct ath12k_pdev_dp *dp_pdev,
						struct ath12k_dp_peer *peer,
						enum hal_encrypt_type enctype,
						struct sk_buff *msdu,
						struct hal_rx_desc_data *rx_desc_data)
{
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_tlv_info_1 tlv_info;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;
	struct ieee80211_rx_status *rxs = IEEE80211_SKB_RXCB(msdu);
	struct ieee80211_key_conf *key_conf;
	struct ieee80211_hdr *hdr;
	u8 mic[IEEE80211_CCMP_MIC_LEN];
	int head_len, tail_len, ret;
	size_t data_len;
	u32 hdr_len, hal_rx_desc_sz = ab->hal.hal_desc_sz;
	u8 *key, *data;
	u8 key_idx;

	if (enctype != HAL_ENCRYPT_TYPE_TKIP_MIC)
		return 0;

	hdr = (struct ieee80211_hdr *)(msdu->data + hal_rx_desc_sz);
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	head_len = hdr_len + hal_rx_desc_sz + IEEE80211_TKIP_IV_LEN;
	tail_len = IEEE80211_CCMP_MIC_LEN + IEEE80211_TKIP_ICV_LEN + FCS_LEN;

	if (!is_multicast_ether_addr(hdr->addr1))
		key_idx = peer->ucast_keyidx;
	else
		key_idx = peer->mcast_keyidx;

	key_conf = peer->keys[key_idx];

	data = msdu->data + head_len;
	data_len = msdu->len - head_len - tail_len;
	key = &key_conf->key[NL80211_TKIP_DATA_OFFSET_RX_MIC_KEY];

	ret = ath12k_dp_rx_h_michael_mic(peer->tfm_mmic, key, hdr, data,
					 data_len, mic);
	if (ret || memcmp(mic, data + data_len, IEEE80211_CCMP_MIC_LEN))
		goto mic_fail;

	return 0;

mic_fail:
	(ATH12K_SKB_RXCB(msdu))->is_first_msdu = true;
	(ATH12K_SKB_RXCB(msdu))->is_last_msdu = true;

	rxs->flag |= RX_FLAG_MMIC_ERROR | RX_FLAG_MMIC_STRIPPED |
		    RX_FLAG_IV_STRIPPED | RX_FLAG_DECRYPTED;
	skb_pull(msdu, hal_rx_desc_sz);

	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu)))
		return -EINVAL;

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;


	tlv_info.freq = rx_desc_data->freq;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.nss = rx_desc_data->nss;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, rxs, &tlv_info,
					(ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret))
		return -EINVAL;

	ret = ath12k_wifi7_dp_rx_h_undecap(dp_pdev, msdu, rx_desc,
					   HAL_ENCRYPT_TYPE_TKIP_MIC, rxs, true, false,
					   &rx_msdu_info, &tlv_info, NULL,
					   ATH12K_SKB_RXCB(msdu)->peer_id,
					   rx_desc_data->tid);
	if (unlikely(ret))
		return -EINVAL;

	ieee80211_rx(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
	return -EINVAL;
}

static int ath12k_wifi7_dp_rx_h_defrag(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_dp_peer *peer,
				       struct ath12k_dp_rx_tid *rx_tid,
				       struct sk_buff **defrag_skb,
				       enum hal_encrypt_type enctype,
				       bool decrypted, struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb, *first_frag, *last_frag;
	struct ieee80211_hdr *hdr;
	bool is_decrypted = false;
	int msdu_len = 0;
	int extra_space;
	u32 flags, hal_rx_desc_sz = ab->hal.hal_desc_sz;

	first_frag = skb_peek(&rx_tid->rx_frags);
	last_frag = skb_peek_tail(&rx_tid->rx_frags);
	if (!first_frag || !last_frag)
		return -EINVAL;

	skb_queue_walk(&rx_tid->rx_frags, skb) {
		flags = 0;
		hdr = (struct ieee80211_hdr *)(skb->data + hal_rx_desc_sz);

		if (enctype != HAL_ENCRYPT_TYPE_OPEN)
			is_decrypted = decrypted;

		if (is_decrypted) {
			if (skb != first_frag)
				flags |= RX_FLAG_IV_STRIPPED;
			if (skb != last_frag)
				flags |= RX_FLAG_ICV_STRIPPED |
					 RX_FLAG_MIC_STRIPPED;
		}

		/* RX fragments are always raw packets */
		if (skb != last_frag)
			skb_trim(skb, skb->len - FCS_LEN);
		ath12k_dp_rx_h_undecap_frag(dp_pdev, skb, enctype, flags);

		if (skb != first_frag)
			skb_pull(skb, hal_rx_desc_sz +
				      ieee80211_hdrlen(hdr->frame_control));
		msdu_len += skb->len;
	}

	extra_space = msdu_len - (DP_RX_BUFFER_SIZE + skb_tailroom(first_frag));
	if (extra_space > 0 &&
	    (pskb_expand_head(first_frag, 0, extra_space, GFP_ATOMIC) < 0))
		return -ENOMEM;

	__skb_unlink(first_frag, &rx_tid->rx_frags);
	while ((skb = __skb_dequeue(&rx_tid->rx_frags))) {
		skb_put_data(first_frag, skb->data, skb->len);
		dev_kfree_skb_any(skb);
	}

	hdr = (struct ieee80211_hdr *)(first_frag->data + hal_rx_desc_sz);
	hdr->frame_control &= ~__cpu_to_le16(IEEE80211_FCTL_MOREFRAGS);

	if (ath12k_wifi7_dp_rx_h_verify_tkip_mic(dp_pdev, peer, enctype, first_frag,
						 rx_desc_data))
		first_frag = NULL;

	*defrag_skb = first_frag;
	return 0;
}

static int
ath12k_wifi7_dp_rx_h_defrag_reo_reinject(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_rx_tid *rx_tid,
					 struct sk_buff *defrag_skb)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_hal *hal = dp->hal;
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)defrag_skb->data;
	struct hal_reo_entrance_ring *reo_ent_ring;
	struct hal_reo_dest_ring *reo_dest_ring;
	struct dp_link_desc_bank *link_desc_banks;
	struct hal_rx_msdu_link *msdu_link;
	struct hal_rx_msdu_details *msdu0;
	struct hal_srng *srng;
	dma_addr_t link_paddr, buf_paddr;
	u32 desc_bank, msdu_info, msdu_ext_info, mpdu_info;
	u32 cookie, hal_rx_desc_sz, dest_ring_info0, queue_addr_hi;
	int ret, len_diff;
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_buffer_addr *info;
	enum hal_rx_buf_return_buf_manager idle_link_rbm = dp->idle_link_rbm;
	const void *end;
	u8 dst_ind;

	hal_rx_desc_sz = hal->hal_desc_sz;
	link_desc_banks = dp->link_desc_banks;
	reo_dest_ring = rx_tid->dst_ring_desc;

	ath12k_wifi7_hal_rx_reo_ent_paddr_get(ab, &reo_dest_ring->buf_addr_info,
					      &link_paddr, &cookie);
	desc_bank = u32_get_bits(cookie, DP_LINK_DESC_BANK_MASK);

	msdu_link = (struct hal_rx_msdu_link *)(link_desc_banks[desc_bank].vaddr +
			(link_paddr - link_desc_banks[desc_bank].paddr));
	msdu0 = &msdu_link->msdu_link[0];
	msdu_ext_info = le32_to_cpu(msdu0->rx_msdu_ext_info.info0);
	dst_ind = u32_get_bits(msdu_ext_info, RX_MSDU_EXT_DESC_INFO0_REO_DEST_IND);

	memset(msdu0, 0, sizeof(*msdu0));

	msdu_info = u32_encode_bits(1, RX_MSDU_DESC_INFO0_FIRST_MSDU_IN_MPDU) |
		    u32_encode_bits(1, RX_MSDU_DESC_INFO0_LAST_MSDU_IN_MPDU) |
		    u32_encode_bits(0, RX_MSDU_DESC_INFO0_MSDU_CONTINUATION) |
		    u32_encode_bits(defrag_skb->len - hal_rx_desc_sz,
				    RX_MSDU_DESC_INFO0_MSDU_LENGTH) |
		    u32_encode_bits(1, RX_MSDU_DESC_INFO0_VALID_SA) |
		    u32_encode_bits(1, RX_MSDU_DESC_INFO0_VALID_DA);
	msdu0->rx_msdu_info.info0 = cpu_to_le32(msdu_info);
	msdu0->rx_msdu_ext_info.info0 = cpu_to_le32(msdu_ext_info);

	len_diff = defrag_skb->len - hal_rx_desc_sz;
	/* change msdu len in hal rx desc */
	ath12k_wifi7_dp_rxdesc_set_msdu_len(ab, rx_desc, len_diff);

	end = defrag_skb->data + DP_RX_BUFFER_SIZE;
	ath12k_core_dmac_clean_range(defrag_skb->data, end);

	buf_paddr = virt_to_phys(defrag_skb->data);
	if (!buf_paddr)
		return -ENOMEM;

	spin_lock_bh(&dp->rx_desc_lock);
	desc_info = list_first_entry_or_null(&dp->rx_desc_free_list,
					     struct ath12k_rx_desc_info,
					     list);
	if (!desc_info) {
		spin_unlock_bh(&dp->rx_desc_lock);
		ath12k_warn(ab, "failed to find rx desc for reinject\n");
		ret = -ENOMEM;
		goto err_unmap_dma;
	}

	desc_info->skb = defrag_skb;
	desc_info->in_use = true;
	desc_info->paddr = buf_paddr;
	desc_info->vaddr = defrag_skb->data;
	desc_info->is_frag = 1;

	list_del(&desc_info->list);
	spin_unlock_bh(&dp->rx_desc_lock);

	info = (struct ath12k_buffer_addr *)&msdu0->buf_addr_info;
	ath12k_hal_rx_buf_addr_info_set(info, buf_paddr,
				       desc_info->cookie,
				       HAL_RX_BUF_RBM_SW5_BM);

	/* Fill mpdu details into reo entrance ring */
	srng = &hal->srng_list[dp->reo_reinject_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	reo_ent_ring = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!reo_ent_ring) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		ret = -ENOSPC;
		goto err_free_desc;
	}
	memset(reo_ent_ring, 0, sizeof(*reo_ent_ring));

	info = (struct ath12k_buffer_addr *)&reo_ent_ring->buf_addr_info;
	ath12k_hal_rx_buf_addr_info_set(info, link_paddr, cookie, idle_link_rbm);

	mpdu_info = u32_encode_bits(1, RX_MPDU_DESC_INFO0_MSDU_COUNT) |
		    u32_encode_bits(0, RX_MPDU_DESC_INFO0_FRAG_FLAG) |
		    u32_encode_bits(1, RX_MPDU_DESC_INFO0_RAW_MPDU) |
		    u32_encode_bits(1, RX_MPDU_DESC_INFO0_VALID_PN) |
		    u32_encode_bits(rx_tid->tid, RX_MPDU_DESC_INFO0_TID);

	reo_ent_ring->rx_mpdu_info.info0 = cpu_to_le32(mpdu_info);
	reo_ent_ring->rx_mpdu_info.peer_meta_data =
		reo_dest_ring->rx_mpdu_info.peer_meta_data;

	if (ab->hw_params->reoq_lut_support) {
		reo_ent_ring->queue_addr_lo = reo_dest_ring->rx_mpdu_info.peer_meta_data;
		queue_addr_hi = 0;
	} else {
		reo_ent_ring->queue_addr_lo = cpu_to_le32(lower_32_bits(rx_tid->paddr));
		queue_addr_hi = upper_32_bits(rx_tid->paddr);
	}

	reo_ent_ring->info0 = le32_encode_bits(queue_addr_hi,
					       HAL_REO_ENTR_RING_INFO0_QUEUE_ADDR_HI) |
			      le32_encode_bits(dst_ind,
					       HAL_REO_ENTR_RING_INFO0_DEST_IND);

	reo_ent_ring->info1 = le32_encode_bits(rx_tid->cur_sn,
					       HAL_REO_ENTR_RING_INFO1_MPDU_SEQ_NUM);
	dest_ring_info0 = le32_get_bits(reo_dest_ring->info0,
					HAL_REO_DEST_RING_INFO0_SRC_LINK_ID);
	reo_ent_ring->info2 =
		cpu_to_le32(u32_get_bits(dest_ring_info0,
					 HAL_REO_ENTR_RING_INFO2_SRC_LINK_ID));

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return 0;

err_free_desc:
	spin_lock_bh(&dp->rx_desc_lock);
	desc_info->in_use = false;
	desc_info->skb = NULL;
	list_add_tail(&desc_info->list, &dp->rx_desc_free_list);
	spin_unlock_bh(&dp->rx_desc_lock);
err_unmap_dma:
	ath12k_core_dma_unmap_single(ab->dev, buf_paddr, DP_RX_BUFFER_SIZE,
				     DMA_TO_DEVICE);
	return ret;
}

static int ath12k_wifi7_dp_rx_h_cmp_frags(struct ath12k_base *ab,
					  struct sk_buff *a, struct sk_buff *b)
{
	int frag1, frag2;

	frag1 = ath12k_wifi7_dp_rx_h_frag_no(ab, a);
	frag2 = ath12k_wifi7_dp_rx_h_frag_no(ab, b);

	return frag1 - frag2;
}

static void ath12k_wifi7_dp_rx_h_sort_frags(struct ath12k_base *ab,
					    struct sk_buff_head *frag_list,
					    struct sk_buff *cur_frag)
{
	struct sk_buff *skb;
	int cmp;

	skb_queue_walk(frag_list, skb) {
		cmp = ath12k_wifi7_dp_rx_h_cmp_frags(ab, skb, cur_frag);
		if (cmp < 0)
			continue;
		__skb_queue_before(frag_list, skb, cur_frag);
		return;
	}
	__skb_queue_tail(frag_list, cur_frag);
}

static u64 ath12k_wifi7_dp_rx_h_get_pn(struct ath12k_dp *dp, struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;
	u64 pn = 0;
	u8 *ehdr;
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;

	hdr = (struct ieee80211_hdr *)(skb->data + hal_rx_desc_sz);
	ehdr = skb->data + hal_rx_desc_sz + ieee80211_hdrlen(hdr->frame_control);

	pn = ehdr[0];
	pn |= (u64)ehdr[1] << 8;
	pn |= (u64)ehdr[4] << 16;
	pn |= (u64)ehdr[5] << 24;
	pn |= (u64)ehdr[6] << 32;
	pn |= (u64)ehdr[7] << 40;

	return pn;
}

static bool
ath12k_wifi7_dp_rx_h_defrag_validate_incr_pn(struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_dp_rx_tid *rx_tid,
					     enum hal_encrypt_type encrypt_type)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct sk_buff *first_frag, *skb;
	u64 last_pn;
	u64 cur_pn;

	first_frag = skb_peek(&rx_tid->rx_frags);
	if (!first_frag)
		return false;

	if (encrypt_type != HAL_ENCRYPT_TYPE_CCMP_128 &&
	    encrypt_type != HAL_ENCRYPT_TYPE_CCMP_256 &&
	    encrypt_type != HAL_ENCRYPT_TYPE_GCMP_128 &&
	    encrypt_type != HAL_ENCRYPT_TYPE_AES_GCMP_256)
		return true;

	last_pn = ath12k_wifi7_dp_rx_h_get_pn(dp, first_frag);
	skb_queue_walk(&rx_tid->rx_frags, skb) {
		if (skb == first_frag)
			continue;

		cur_pn = ath12k_wifi7_dp_rx_h_get_pn(dp, skb);
		if (cur_pn != last_pn + 1)
			return false;
		last_pn = cur_pn;
	}
	return true;
}

static int ath12k_wifi7_dp_rx_frag_h_mpdu(struct ath12k_pdev_dp *dp_pdev,
					  struct sk_buff *msdu,
					  struct hal_reo_dest_ring *ring_desc,
					  struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_peer *peer;
	struct ath12k_dp_rx_tid *rx_tid;
	struct sk_buff *defrag_skb = NULL;
	u32 peer_id = rx_desc_data->peer_id;
	u16 seqno, frag_no;
	u8 tid = rx_desc_data->tid;
	int ret = 0;
	bool more_frags;
	enum hal_encrypt_type enctype;

	frag_no = ath12k_wifi7_dp_rx_h_frag_no(ab, msdu);
	more_frags = ath12k_wifi7_dp_rx_h_more_frags(ab, msdu);
	seqno = rx_desc_data->seq_no;

	if (!rx_desc_data->seq_ctl_valid || !rx_desc_data->fc_valid || tid > IEEE80211_NUM_TIDS)
		return -EINVAL;

	/* received unfragmented packet in reo
	 * exception ring, this shouldn't happen
	 * as these packets typically come from
	 * reo2sw srngs.
	 */
	if (WARN_ON_ONCE(!frag_no && !more_frags))
		return -EINVAL;

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer) {
		ath12k_warn(ab, "failed to find the peer to de-fragment received fragment peer_id %d\n",
			    peer_id);
		ret = -ENOENT;
		goto out_unlock;
	}

	if (rx_desc_data->is_mcbc)
		enctype = peer->sec_type_grp;
	else
		enctype = peer->sec_type;

	if (!peer->primary_link_frag_setup) {
		ath12k_warn(ab, "The peer %pM [%d] has uninitialized datapath\n",
			    peer->addr, peer_id);
		ret = -ENOENT;
		goto out_unlock;
	}

	rx_tid = &peer->rx_tid[tid];

	if ((!skb_queue_empty(&rx_tid->rx_frags) && seqno != rx_tid->cur_sn) ||
	    skb_queue_empty(&rx_tid->rx_frags)) {
		/* Flush stored fragments and start a new sequence */
		ath12k_dp_rx_frags_cleanup(rx_tid, true);
		rx_tid->cur_sn = seqno;
	}

	if (rx_tid->rx_frag_bitmap & BIT(frag_no)) {
		/* Fragment already present */
		ret = -EINVAL;
		goto out_unlock;
	}

	if ((!rx_tid->rx_frag_bitmap || frag_no > __fls(rx_tid->rx_frag_bitmap)))
		__skb_queue_tail(&rx_tid->rx_frags, msdu);
	else
		ath12k_wifi7_dp_rx_h_sort_frags(ab, &rx_tid->rx_frags, msdu);

	rx_tid->rx_frag_bitmap |= BIT(frag_no);
	if (!more_frags)
		rx_tid->last_frag_no = frag_no;

	if (frag_no == 0) {
		rx_tid->dst_ring_desc = kmemdup(ring_desc,
						sizeof(*rx_tid->dst_ring_desc),
						GFP_ATOMIC);
		if (!rx_tid->dst_ring_desc) {
			ret = -ENOMEM;
			goto out_unlock;
		}
	} else {
		ath12k_wifi7_dp_rx_link_desc_return(dp, &ring_desc->buf_addr_info,
						    HAL_WBM_REL_BM_ACT_PUT_IN_IDLE);
	}

	if (!rx_tid->last_frag_no ||
	    rx_tid->rx_frag_bitmap != GENMASK(rx_tid->last_frag_no, 0)) {
		mod_timer(&rx_tid->frag_timer, jiffies +
					       ATH12K_DP_RX_FRAGMENT_TIMEOUT_MS);
		goto out_unlock;
	}

	spin_unlock_bh(&dp->dp_lock);
	del_timer_sync(&rx_tid->frag_timer);
	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer)
		goto err_frags_cleanup;

	if (!ath12k_wifi7_dp_rx_h_defrag_validate_incr_pn(dp_pdev, rx_tid, enctype))
		goto err_frags_cleanup;

	if (ath12k_wifi7_dp_rx_h_defrag(dp_pdev, peer, rx_tid, &defrag_skb,
					enctype, rx_desc_data->is_decrypted,
					rx_desc_data))
		goto err_frags_cleanup;

	if (!defrag_skb)
		goto err_frags_cleanup;

	if (ath12k_wifi7_dp_rx_h_defrag_reo_reinject(dp, dp_pdev, rx_tid, defrag_skb))
		goto err_frags_cleanup;

	ath12k_dp_rx_frags_cleanup(rx_tid, false);
	goto out_unlock;

err_frags_cleanup:
	dev_kfree_skb_any(defrag_skb);
	ath12k_dp_rx_frags_cleanup(rx_tid, true);
out_unlock:
	spin_unlock_bh(&dp->dp_lock);
	return ret;
}

static int
ath12k_wifi7_dp_process_rx_err_buf(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_reo_dest_ring *desc,
				   struct list_head *used_list,
				   bool drop, u32 cookie)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_desc_data rx_desc_data = {0};
	struct hal_rx_desc *rx_desc;
	struct ath12k_skb_rxcb *rxcb;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *peer;
	struct sk_buff *msdu;
	u16 msdu_len;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;
	struct ath12k_rx_desc_info *desc_info;
	u64 desc_va;

	desc_va = ((u64)le32_to_cpu(desc->buf_va_hi) << 32 |
		   le32_to_cpu(desc->buf_va_lo));
	desc_info = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);

	/* retry manual desc retrieval */
	if (!desc_info) {
		desc_info = ath12k_dp_get_rx_desc(dp, cookie);
		if (!desc_info) {
			ath12k_warn(ab, "Invalid cookie in DP rx error descriptor retrieval: 0x%x\n",
				    cookie);
			return -EINVAL;
		}
	}

	if (desc_info->magic != ATH12K_DP_RX_DESC_MAGIC)
		ath12k_warn(ab, " RX Exception, Check HW CC implementation");

	msdu = desc_info->skb;
	desc_info->skb = NULL;
	rxcb = ATH12K_SKB_RXCB(msdu);
	rxcb->peer_id = le32_get_bits(desc->rx_mpdu_info.peer_meta_data,
				      RX_MPDU_DESC_META_DATA_V1_PEER_ID);

	list_add_tail(&desc_info->list, used_list);

	ath12k_core_dmac_inv_range(desc_info->vaddr,
				   desc_info->vaddr + DP_RX_BUFFER_SIZE);

	if (drop) {
		rcu_read_lock();
		peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								rxcb->peer_id);
		if (peer) {
			ahvif = ath12k_vif_to_ahvif(peer->vif);
			ahvif->wmm_stats.rx_type = dp_pdev->wmm_stats.rx_type;
			ahvif->wmm_stats.total_wmm_rx_drop[ahvif->wmm_stats.rx_type]++;
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev))
				ath12k_tid_drop_rx_stats(ahvif, rxcb->tid, 0,
							 ATH_RX_RBM_ERR);
		}
		rcu_read_unlock();
		dev_kfree_skb_any(msdu);
		return 0;
	}

	rcu_read_lock();
	if (!rcu_dereference(ar->ab->pdevs_active[ar->pdev_idx])) {
		dev_kfree_skb_any(msdu);
		goto exit;
	}

	if (test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) {
		dev_kfree_skb_any(msdu);
		goto exit;
	}

	rx_desc = (struct hal_rx_desc *)msdu->data;
	ath12k_wifi7_dp_extract_rx_desc_data(dp, &rx_desc_data, rx_desc, rx_desc);

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								rxcb->peer_id);
		if (peer) {
			ahvif = ath12k_vif_to_ahvif(peer->vif);
			ath12k_tid_rx_stats(ahvif, rxcb->tid, rx_desc_data.msdu_len,
					    ATH_RX_FRAG_PKTS);
		}
	}

	msdu_len = rx_desc_data.msdu_len;
	if ((msdu_len + hal_rx_desc_sz) > DP_RX_BUFFER_SIZE) {
		ath12k_warn(ab, "invalid msdu leng %u", msdu_len);
		ath12k_dbg_dump(ab, ATH12K_DBG_DATA, NULL, "", rx_desc,
				sizeof(*rx_desc));
		dev_kfree_skb_any(msdu);
		goto exit;
	}

	skb_put(msdu, hal_rx_desc_sz + msdu_len);

	if (ath12k_wifi7_dp_rx_frag_h_mpdu(dp_pdev, msdu, desc, &rx_desc_data)) {
		dev_kfree_skb_any(msdu);
		ath12k_wifi7_dp_rx_link_desc_return(dp, &desc->buf_addr_info,
						    HAL_WBM_REL_BM_ACT_PUT_IN_IDLE);
	}
exit:
	rcu_read_unlock();
	return 0;
}

static int ath12k_wifi7_handle_msdu_buftype(struct ath12k_dp *dp,
					    struct hal_reo_dest_ring *reo_desc,
					    struct list_head *rx_desc_used_list)
{
	struct ath12k_rx_desc_info *desc_info;
	struct sk_buff *msdu;
	const void *end;
	u64 desc_va;

	desc_va = ((u64)le32_to_cpu(reo_desc->buf_va_hi) << 32 |
		   le32_to_cpu(reo_desc->buf_va_lo));
	desc_info = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);

	if (!desc_info) {
		ath12k_warn(dp, " rx exception, hw cookie conversion failed");
		u32 cookie = le32_get_bits(reo_desc->buf_addr_info.info1,
					   BUFFER_ADDR_INFO1_SW_COOKIE);
		desc_info = ath12k_dp_get_rx_desc(dp, cookie);
		if (!desc_info) {
			ath12k_warn(dp->ab, "Unable to retrieve rx_desc for va 0x%lx",
				    (unsigned long)desc_va);
			return -EINVAL;
		}
	}

	if (desc_info->magic != ATH12K_DP_RX_DESC_MAGIC) {
		ath12k_warn(dp, " rx exception, magic check failed");
		return -EINVAL;
	}

	msdu = desc_info->skb;
	desc_info->skb = NULL;

	list_add_tail(&desc_info->list, rx_desc_used_list);

	end = desc_info->vaddr + DP_RX_BUFFER_SIZE;
	ath12k_core_dmac_inv_range(desc_info->vaddr, end);
	dev_kfree_skb_any(msdu);

	return 0;
}

int ath12k_wifi7_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp *partner_dp;
	struct list_head rx_desc_used_list[ATH12K_MAX_SOCS];
	u32 msdu_cookies[HAL_NUM_RX_MSDUS_PER_LINK_DESC];
	int num_buffs_reaped[ATH12K_MAX_SOCS] = {};
	struct dp_link_desc_bank *link_desc_banks;
	enum hal_rx_buf_return_buf_manager rbm;
	struct hal_rx_msdu_link *link_desc_va;
	int tot_n_bufs_reaped, quota, ret, i;
	struct hal_reo_dest_ring *reo_desc;
	struct dp_rxdma_ring *rx_ring;
	struct dp_srng *reo_except;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	u8 hw_link_id, device_id;
	u32 desc_bank, num_msdus;
	struct hal_srng *srng;
	struct ath12k_pdev_dp *dp_pdev;
	dma_addr_t paddr;
	bool is_frag, drop = false;
	int pdev_id;
	struct list_head *used_list;
	enum hal_wbm_rel_bm_act act;

	tot_n_bufs_reaped = 0;
	quota = budget;

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++)
		INIT_LIST_HEAD(&rx_desc_used_list[device_id]);

	reo_except = &dp->reo_except_ring;

	srng = &ab->hal.srng_list[reo_except->ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while (budget &&
	       (reo_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		drop = false;
		dp->device_stats.err_ring_pkts++;

		hw_link_id = le32_get_bits(reo_desc->info0,
					   HAL_REO_DEST_RING_INFO0_SRC_LINK_ID);
		device_id = hw_links[hw_link_id].device_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);

		ret = ath12k_wifi7_hal_desc_reo_parse_err(partner_dp, reo_desc, &paddr,
							  &desc_bank);
		if (ret) {
			ath12k_warn(ab, "failed to parse error reo desc %d\n",
				    ret);
			if (ret == -EOPNOTSUPP) {
				used_list = &rx_desc_used_list[device_id];
				if (!ath12k_wifi7_handle_msdu_buftype(partner_dp,
								      reo_desc,
								      used_list))
					tot_n_bufs_reaped++;
			}
			continue;
		}

		pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
						      hw_links[hw_link_id].pdev_idx);

		link_desc_banks = partner_dp->link_desc_banks;
		link_desc_va = link_desc_banks[desc_bank].vaddr +
			       (paddr - link_desc_banks[desc_bank].paddr);
		ath12k_wifi7_hal_rx_msdu_link_info_get(link_desc_va, &num_msdus,
						       msdu_cookies, &rbm);
		if (rbm != partner_dp->idle_link_rbm &&
		    rbm != HAL_RX_BUF_RBM_SW5_BM &&
		    rbm != partner_dp->hal->hal_params->rx_buf_rbm) {
			act = HAL_WBM_REL_BM_ACT_REL_MSDU;
			partner_dp->device_stats.invalid_rbm++;
			ath12k_warn(ab, "invalid return buffer manager %d\n", rbm);
			ath12k_wifi7_dp_rx_link_desc_return(partner_dp,
							    &reo_desc->buf_addr_info,
							    act);
			continue;
		}

		is_frag = !!(le32_to_cpu(reo_desc->rx_mpdu_info.info0) &
			     RX_MPDU_DESC_INFO0_FRAG_FLAG);

		/* Process only rx fragments with one msdu per link desc below, and drop
		 * msdu's indicated due to error reasons.
		 * Dynamic fragmentation not supported in Multi-link client, so drop the
		 * partner device buffers.
		 */
		if (!is_frag || num_msdus > 1 ||
		    partner_dp->device_id != dp->device_id) {
			drop = true;
			act = HAL_WBM_REL_BM_ACT_PUT_IN_IDLE;

			/* Return the link desc back to wbm idle list */
			ath12k_wifi7_dp_rx_link_desc_return(partner_dp,
							    &reo_desc->buf_addr_info,
							    act);
		}

		rcu_read_lock();

		dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
		if (!dp_pdev) {
			rcu_read_unlock();
			continue;
		}

		if (drop)
			dp_pdev->wmm_stats.total_wmm_rx_drop[dp_pdev->wmm_stats.rx_type]++;

		for (i = 0; i < num_msdus; i++) {
			used_list = &rx_desc_used_list[device_id];

			if (!ath12k_wifi7_dp_process_rx_err_buf(dp_pdev, reo_desc,
								used_list,
								drop,
								msdu_cookies[i])) {
				num_buffs_reaped[device_id]++;
				tot_n_bufs_reaped++;
			}
		}

		rcu_read_unlock();

		if (tot_n_bufs_reaped >= quota) {
			tot_n_bufs_reaped = quota;
			goto exit;
		}

		budget = quota - tot_n_bufs_reaped;
	}

exit:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
		if (!num_buffs_reaped[device_id])
			continue;

		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		rx_ring = &partner_dp->rx_refill_buf_ring;

		ath12k_dp_rx_bufs_replenish(partner_dp, rx_ring,
					    &rx_desc_used_list[device_id]);
	}

	return tot_n_bufs_reaped;
}

static inline void ath12k_wifi7_dp_rx_h_err_update_peer_stats(struct ath12k_pdev_dp *dp_pdev,
							      struct ath12k_skb_rxcb *rxcb)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_peer *peer = ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev, rxcb->peer_id);

	if (peer) {
		u8 link_id = peer->hw_links[rxcb->hw_link_id];

		switch(rxcb->err_rel_src) {
		case HAL_WBM_REL_SRC_MODULE_REO:
			DP_PEER_LINK_STATS_CNT(peer, wbm_err.reo_error[rxcb->err_code], 1, link_id);
			break;
		case HAL_WBM_REL_SRC_MODULE_RXDMA:
			DP_PEER_LINK_STATS_CNT(peer, wbm_err.rxdma_error[rxcb->err_code], 1, link_id);
			break;
		default:
			break;
		}
	}
	else {
		DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_INVALID_PEER_ID_ERROR], 1);
	}
}

static void
ath12k_wifi7_dp_rx_null_q_desc_sg_drop(struct ath12k_dp *dp,
				       struct ath12k_pdev_dp *dp_pdev, int msdu_len,
				       struct sk_buff_head *msdu_list)
{
	struct sk_buff *skb, *tmp;
	struct ath12k_skb_rxcb *rxcb;
	int n_buffs;

	n_buffs = DIV_ROUND_UP(msdu_len,
			       (DP_RX_BUFFER_SIZE - dp->ab->hal.hal_desc_sz));

	skb_queue_walk_safe(msdu_list, skb, tmp) {
		rxcb = ATH12K_SKB_RXCB(skb);
		if (rxcb->err_rel_src == HAL_WBM_REL_SRC_MODULE_REO &&
		    rxcb->err_code == HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO) {
			if (!n_buffs)
				break;
			__skb_unlink(skb, msdu_list);
			dev_kfree_skb_any(skb);
			n_buffs--;
		}
	}
}

static int ath12k_wifi7_dp_rx_h_null_q_desc(struct ath12k_pdev_dp *dp_pdev,
					    struct sk_buff *msdu,
					    struct ieee80211_rx_status *status,
					    struct sk_buff_head *msdu_list,
					    struct hal_rx_desc_data *rx_desc_data)
{
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_mpdu_desc_info rx_mpdu_info;
	struct rx_tlv_info_1 tlv_info = {0};
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	u16 msdu_len = rx_desc_data->msdu_len;
	struct hal_rx_desc *desc = (struct hal_rx_desc *)msdu->data;
	u8 l3pad_bytes = rx_desc_data->l3_pad_bytes;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;
	bool fast_rx = false;
	int ret = 0;
	struct ath12k_dp_peer *peer = NULL;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *link_peer;

	if (!rxcb->is_frag && ((msdu_len + hal_rx_desc_sz) > DP_RX_BUFFER_SIZE)) {
		/* First buffer will be freed by the caller, so deduct it's length */
		msdu_len = msdu_len - (DP_RX_BUFFER_SIZE - hal_rx_desc_sz);
		ath12k_wifi7_dp_rx_null_q_desc_sg_drop(dp, dp_pdev, msdu_len, msdu_list);
		DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_DROP_SCATTER_GATHER], 1);
		return -EINVAL;
	}

	/* Even after cleaning up the sg buffers in the msdu list with above check
	 * any msdu received with continuation flag needs to be dropped as invalid.
	 * This protects against some random err frame with continuation flag.
	 */
	if (rxcb->is_continuation)
		return -EINVAL;

	if (!rx_desc_data->msdu_done) {
		ath12k_warn(ab,
			    "msdu_done bit not set in null_q_des processing\n");
		__skb_queue_purge(msdu_list);
		return -EIO;
	}

	/* Handle NULL queue descriptor violations arising out a missing
	 * REO queue for a given peer or a given TID. This typically
	 * may happen if a packet is received on a QOS enabled TID before the
	 * ADDBA negotiation for that TID, when the TID queue is setup. Or
	 * it may also happen for MC/BC frames if they are not routed to the
	 * non-QOS TID queue, in the absence of any other default TID queue.
	 * This error can show up both in a REO destination or WBM release ring.
	 */

	if (rxcb->is_frag) {
		skb_pull(msdu, hal_rx_desc_sz);
	} else {
		if ((hal_rx_desc_sz + l3pad_bytes + msdu_len) > DP_RX_BUFFER_SIZE)
			return -EINVAL;

		skb_put(msdu, hal_rx_desc_sz + l3pad_bytes + msdu_len);
		skb_pull(msdu, hal_rx_desc_sz + l3pad_bytes);
	}
	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu))) {
		DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_DROP_INVALID_NWIFI_HDR_LEN], 1);
		return -EINVAL;
	}

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	rx_msdu_info.tcp_udp_chksum_fail = rx_desc_data->l4_csum_fail;
	rx_msdu_info.ip_chksum_fail = rx_desc_data->ip_csum_fail;
	rx_mpdu_info.flow_info.peer_id = rxcb->peer_id;
	rx_mpdu_info.tid = rx_desc_data->tid;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.freq = rx_desc_data->freq;
	tlv_info.nss = rx_desc_data->nss;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, &tlv_info,
					(ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret))
		return -EINVAL;

	rcu_read_lock();
	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
						   rx_mpdu_info.flow_info.peer_id);
	ret = ath12k_wifi7_dp_rx_h_mpdu(dp_pdev, msdu, desc, status, &rx_msdu_info,
					&rx_mpdu_info, &tlv_info,
					rx_desc_data->err_bitmap, &fast_rx,
					peer);
	if (unlikely(ret)) {
		rcu_read_unlock();
		return -EINVAL;
	}
	rcu_read_unlock();

	rxcb->tid = rx_desc_data->tid;

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		rcu_read_lock();
		link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								     rxcb->peer_id);
		if (link_peer) {
			ahvif = ath12k_vif_to_ahvif(link_peer->vif);
			ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu_len, ATH_RX_HW_PKTS);
		}
		rcu_read_unlock();
	}
	/* Please note that caller will having the access to msdu and completing
	 * rx with mac80211. Need not worry about cleaning up amsdu_list.
	 */

	return 0;
}

static void ath12k_fill_reo_drop_reason(struct ath12k_skb_rxcb *rxcb,
					u32 drop_reason)
{
	switch (rxcb->err_code) {
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_INVALID:
			drop_reason = ATH_RX_DESC_INVALID;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_AMPDU_IN_NON_BA:
			drop_reason = ATH_RX_NON_BA;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_NON_BA_DUPLICATE:
			drop_reason = ATH_RX_NON_BA_DUP;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_BA_DUPLICATE:
			drop_reason = ATH_RX_BA_DUP;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_2K_JUMP:
	case HAL_REO_DEST_RING_ERROR_CODE_BAR_2K_JUMP:
			drop_reason = ATH_RX_2K_JUMP;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_OOR:
	case HAL_REO_DEST_RING_ERROR_CODE_BAR_OOR:
			drop_reason = ATH_RX_ERR_OOR;
			break;
	case  HAL_REO_DEST_RING_ERROR_CODE_NO_BA_SESSION:
			drop_reason = ATH_RX_NO_BA;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_SN_EQUALS_SSN:
			drop_reason = ATH_RX_EQUALS_SSN;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_2K_ERR_FLAG_SET:
	case HAL_REO_DEST_RING_ERROR_CODE_PN_ERR_FLAG_SET:
			drop_reason = ATH_RX_ERR_FLAG_SET;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_BLOCKED:
			drop_reason = ATH_RX_DESC_BLOCKED;
			break;
	}
}

static bool ath12k_wifi7_dp_rx_h_reo_err(struct ath12k_pdev_dp *dp_pdev,
					 struct sk_buff *msdu,
					 struct ieee80211_rx_status *status,
					 struct sk_buff_head *msdu_list,
					 struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *link_peer;
	bool drop = false;
	u32 drop_reason;
	u16 msdu_len;

	DP_DEVICE_STATS_INC(dp, wbm_err.reo_error[rxcb->err_code], 1);
	ath12k_wifi7_dp_rx_h_err_update_peer_stats(dp_pdev, rxcb);

	switch (rxcb->err_code) {
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO:
		struct ath12k *ar = dp_pdev->ar;

		if (ath12k_wifi7_dp_rx_h_null_q_desc(dp_pdev, msdu, status, msdu_list,
						     rx_desc_data)) {
			drop = true;
			drop_reason = ATH_RX_NULL_Q_DESC;
		}

		/* CCE rule configured for the ERP feature on REO RELEASE RING gets
		 * handled under this specific error code
		 * */
		if (!drop && ar->erp_trigger_set)
			queue_work(ar->ab->workqueue, &ar->erp_handle_trigger_work);

		break;
	case HAL_REO_DEST_RING_ERROR_CODE_PN_CHECK_FAILED:
		/* TODO: Do not drop PN failed packets in the driver;
		 * instead, it is good to drop such packets in mac80211
		 * after incrementing the replay counters.
		 */
		fallthrough;
	default:
		/* TODO: Review other errors and process them to mac80211
		 * as appropriate.
		 */
		DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_DROP_REO_GENERIC], 1);
		drop_reason = ATH_RX_REO_ERR;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev))
			ath12k_fill_reo_drop_reason(rxcb, drop_reason);
		drop = true;
		break;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		msdu_len = le32_get_bits(rx_desc->u.qcn9274.msdu_end.info10,
					 RX_MSDU_END_INFO10_MSDU_LENGTH);

		rcu_read_lock();
		link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								     rxcb->peer_id);
		if (link_peer) {
			ahvif = ath12k_vif_to_ahvif(link_peer->vif);
			if (drop)
				ath12k_tid_drop_rx_stats(ahvif, rxcb->tid, msdu_len,
							 drop_reason);
			else
				ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu_len,
						    ATH_RX_REO_ERR_PKTS);
		}
		rcu_read_unlock();
	}

	return drop;
}

static bool ath12k_wifi7_dp_rx_h_tkip_mic_err(struct ath12k_pdev_dp *dp_pdev,
					      struct sk_buff *msdu,
					      struct ieee80211_rx_status *status,
					      struct hal_rx_desc_data *rx_desc_data)
{
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_tlv_info_1 tlv_info;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	u16 msdu_len = rx_desc_data->msdu_len;
	struct hal_rx_desc *desc = (struct hal_rx_desc *)msdu->data;
	u8 l3pad_bytes = rx_desc_data->l3_pad_bytes;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;
	int ret;

	rxcb->is_first_msdu = rx_desc_data->is_first_msdu;
	rxcb->is_last_msdu = rx_desc_data->is_last_msdu;

	if ((hal_rx_desc_sz + l3pad_bytes + msdu_len) > DP_RX_BUFFER_SIZE) {
		ath12k_warn(ab, "invalid msdu len in tkip mirc err %u\n", msdu_len);
		ath12k_dbg_dump(ab, ATH12K_DBG_DATA, NULL, "", desc,
				sizeof(struct hal_rx_desc));
		return true;
	}

	skb_put(msdu, hal_rx_desc_sz + l3pad_bytes + msdu_len);
	skb_pull(msdu, hal_rx_desc_sz + l3pad_bytes);

	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu)))
		return true;

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;
	tlv_info.freq = rx_desc_data->freq;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.nss = rx_desc_data->nss;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, &tlv_info,
				  (ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret))
		return true;

	status->flag |= (RX_FLAG_MMIC_STRIPPED | RX_FLAG_MMIC_ERROR |
				     RX_FLAG_DECRYPTED);

	ret = ath12k_wifi7_dp_rx_h_undecap(dp_pdev, msdu, desc,
					   HAL_ENCRYPT_TYPE_TKIP_MIC, status, false,
					   false, &rx_msdu_info, &tlv_info, NULL,
					   rxcb->peer_id, rx_desc_data->tid);
	if (ret)
		return true;

	return false;
}

static bool ath12k_dp_rx_h_mec_drop(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_link_sta *arsta = NULL;
	struct ath12k_dp_link_peer *peer;

	spin_lock_bh(&dp_pdev->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp_pdev->dp, rx_desc_data->peer_id);
	if (!peer)
		goto drop;

	if (peer->vif && peer->vif->type != NL80211_IFTYPE_STATION) {
		ath12k_warn(ab, "vif type is not station for peer with peer_id %u\n",
			    rx_desc_data->peer_id);
		goto drop;
	}

	if (peer && peer->sta) {
		rcu_read_lock();

		arsta = ath12k_peer_get_link_sta(ab, peer);
		if (arsta) {
			spin_lock_bh(&arsta->arvif->link_stats_lock);
			arsta->arvif->link_stats.rx_dropped++;
			spin_unlock_bh(&arsta->arvif->link_stats_lock);
		}

		rcu_read_unlock();
	}
drop:
	spin_unlock_bh(&dp_pdev->dp->dp_lock);
	return true;
}

static inline void
ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(struct ath12k_dp* dp,
					struct sk_buff *msdu,
					enum ath12k_wbm_err_drop_reason drop_reason)
{
	DP_DEVICE_STATS_INC(dp, wbm_err.drop[drop_reason], 1);
	dev_kfree_skb_any(msdu);
}

static int ath12k_wifi7_dp_rx_h_unauth_wds_err(struct ath12k_pdev_dp *dp_pdev,
					       struct sk_buff *msdu,
					       struct ieee80211_rx_status *status,
					       struct hal_rx_desc_data *rx_desc_data)
{
	struct hal_rx_desc *desc = (struct hal_rx_desc *)msdu->data;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 hdr_len, hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;
	u8 l3pad_bytes = rx_desc_data->l3_pad_bytes;
	u16 msdu_len = rx_desc_data->msdu_len;
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_mpdu_desc_info rx_mpdu_info;
	struct ath12k_dp_rx_rfc1042_hdr *llc;
	struct ath12k_dp_peer *peer = NULL;
	struct rx_tlv_info_1 tlv_info;
	struct ieee80211_hdr *hdr;
	bool fast_rx = false;
	bool drop = false;
	int ret;

	rcu_read_lock();
	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
						   rxcb->peer_id);
	if (!peer) {
		ath12k_dbg(dp->ab, ATH12K_DBG_DATA,
			   "failed to find the peer to process unauth wds err handling peer_id %d\n",
			   rxcb->peer_id);
		drop = true;
		goto exit;
	}

	if ((hal_rx_desc_sz + l3pad_bytes + msdu_len) > DP_RX_BUFFER_SIZE) {
		drop = true;
		goto exit;
	}

	skb_put(msdu, hal_rx_desc_sz + l3pad_bytes + msdu_len);
	skb_pull(msdu, hal_rx_desc_sz + l3pad_bytes);

	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu))) {
		drop = true;
		goto exit;
	}

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	rx_msdu_info.tcp_udp_chksum_fail = rx_desc_data->l4_csum_fail;
	rx_msdu_info.ip_chksum_fail = rx_desc_data->ip_csum_fail;
	rx_mpdu_info.flow_info.peer_id = rxcb->peer_id;
	rx_mpdu_info.tid = rx_desc_data->tid;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;
	tlv_info.freq = rx_desc_data->freq;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.nss = rx_desc_data->nss;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, &tlv_info,
				  (ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret)) {
		drop = true;
		goto exit;
	}

	ret = ath12k_wifi7_dp_rx_h_mpdu(dp_pdev, msdu, desc, status, &rx_msdu_info,
					&rx_mpdu_info, &tlv_info,
					rx_desc_data->err_bitmap, &fast_rx,
					peer);
	if (unlikely(ret)) {
		drop = true;
		goto exit;
	}

	rxcb->tid = rx_desc_data->tid;

	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	llc = (struct ath12k_dp_rx_rfc1042_hdr *)(msdu->data + hdr_len);

	if (!(llc->snap_type == cpu_to_be16(ETH_P_PAE) ||
	    ieee80211_is_qos_nullfunc(hdr->frame_control)))
		drop = true;

exit:
	rcu_read_unlock();
	return drop;
}

static bool ath12k_wifi7_dp_rx_h_rxdma_err(struct ath12k_pdev_dp *dp_pdev,
					   struct sk_buff *msdu,
					   struct ieee80211_rx_status *status,
					   struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *link_peer;
	u32 drop_reason;
	u16 msdu_len;
	bool drop = false;

	DP_DEVICE_STATS_INC(dp, wbm_err.rxdma_error[rxcb->err_code], 1);
	ath12k_wifi7_dp_rx_h_err_update_peer_stats(dp_pdev, rxcb);

	switch (rxcb->err_code) {
	case HAL_REO_ENTR_RING_RXDMA_ECODE_UNAUTH_WDS_ERR:
		drop = ath12k_wifi7_dp_rx_h_unauth_wds_err(dp_pdev, msdu, status, rx_desc_data);
		if (drop)
			drop_reason = ATH_RX_UNAUTH_WDS_ERR;
		break;
	case HAL_REO_ENTR_RING_RXDMA_ECODE_MULTICAST_ECHO_ERR:
		drop = ath12k_dp_rx_h_mec_drop(dp_pdev, rx_desc_data);
		if (drop)
			drop_reason = ATH_RX_ECHO_ERR;
		break;
	case HAL_REO_ENTR_RING_RXDMA_ECODE_DECRYPT_ERR:
	case HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR:
		if (rx_desc_data->err_bitmap & HAL_RX_MPDU_ERR_TKIP_MIC) {
			drop = ath12k_wifi7_dp_rx_h_tkip_mic_err(dp_pdev, msdu, status,
								 rx_desc_data);
			break;
		}
		fallthrough;
	default:
		/* TODO: Review other rxdma error code to check if anything is
		 * worth reporting to mac80211
		 */
		drop = true;
		drop_reason = ATH_RX_RXDMA_ERR;
		break;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		msdu_len = le32_get_bits(rx_desc->u.qcn9274.msdu_end.info10,
					 RX_MSDU_END_INFO10_MSDU_LENGTH);

		rcu_read_lock();
		link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
								     rxcb->peer_id);
		if (link_peer) {
			ahvif = ath12k_vif_to_ahvif(link_peer->vif);
			if (drop)
				ath12k_tid_drop_rx_stats(ahvif, rxcb->tid, msdu_len,
							 drop_reason);
			else
				ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu_len,
						    ATH_RX_RXDMA_PKTS);
		}
		rcu_read_unlock();
	}

	return drop;
}

static void ath12k_wifi7_dp_rx_wbm_err(struct ath12k_pdev_dp *dp_pdev,
				       struct napi_struct *napi,
				       struct sk_buff *msdu,
				       struct sk_buff_head *msdu_list)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct hal_rx_desc_data rx_desc_data = {0};
	struct ieee80211_rx_status rxs = {0};
	bool drop = true;
	struct ieee80211_hdr *hdr;
	struct ath12k_dp_rx_rfc1042_hdr *llc;
	enum ath12k_dp_eapol_key_type subtype;
	size_t hdr_len;
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;

	ath12k_wifi7_dp_extract_rx_desc_data(dp, &rx_desc_data, rx_desc, rx_desc);

	switch (rxcb->err_rel_src) {
	case HAL_WBM_REL_SRC_MODULE_REO:
		drop = ath12k_wifi7_dp_rx_h_reo_err(dp_pdev, msdu, &rxs, msdu_list,
						    &rx_desc_data);
		break;
	case HAL_WBM_REL_SRC_MODULE_RXDMA:
		drop = ath12k_wifi7_dp_rx_h_rxdma_err(dp_pdev, msdu, &rxs, &rx_desc_data);
		if (drop)
			DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_DROP_RXDMA_GENERIC], 1);
		break;
	default:
		/* msdu will get freed */
		break;
	}

	if (drop) {
		dev_kfree_skb_any(msdu);
		return;
	}
	rxs.flag |= RX_FLAG_SKIP_MONITOR;

	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	llc = (struct ath12k_dp_rx_rfc1042_hdr *)(msdu->data + hdr_len);
	if (llc->snap_type == cpu_to_be16(ETH_P_PAE)) {
		dp->device_stats.rx_eapol[ab->device_id]++;
		subtype = ath12k_dp_get_eapol_subtype(msdu->data + hdr_len + LLC_SNAP_HDR_LEN);
		if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
			dp->device_stats.rx_eapol_type[subtype-1][ab->device_id]++;
			ath12k_dbg(ab, ATH12K_DBG_EAPOL, "Received %s%d EAPOL frame from "
				   "STA %pM\n", subtype <= 4 ? "M" : "G",
				   subtype <= 4 ? subtype : (subtype - 4), hdr->addr2);
		}
	}

	ath12k_dp_rx_deliver_msdu(dp_pdev, napi, msdu, &rxs, rxcb->hw_link_id,
				  rx_desc_data.is_mcbc, rx_desc_data.peer_id,
				  rx_desc_data.tid);
}

int ath12k_wifi7_dp_rx_process_wbm_err(struct ath12k_dp *dp,
				       struct napi_struct *napi, int budget)
{
	struct list_head rx_desc_used_list[ATH12K_MAX_SOCS];
	struct ath12k *ar;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_pdev_dp *dp_pdev; //TODO: Check this
	struct ath12k_dp *partner_dp;
	struct dp_rxdma_ring *rx_ring;
	struct hal_rx_wbm_rel_info err_info;
	struct hal_srng *srng;
	struct sk_buff *msdu;
	struct sk_buff_head msdu_list, scatter_msdu_list;
	struct ath12k_skb_rxcb *rxcb;
	void *rx_desc;
	int num_buffs_reaped[ATH12K_MAX_SOCS] = {};
	int total_num_buffs_reaped = 0;
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_device_dp_stats *device_stats = &dp->device_stats;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	u8 hw_link_id, device_id;
	int ret, pdev_id;
	struct hal_rx_desc *msdu_data;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *link_peer;

	__skb_queue_head_init(&msdu_list);
	__skb_queue_head_init(&scatter_msdu_list);

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++)
		INIT_LIST_HEAD(&rx_desc_used_list[device_id]);

	srng = &ab->hal.srng_list[dp->rx_rel_ring.ring_id];
	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while (budget) {
		rx_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng);
		if (!rx_desc)
			break;

		ret = ath12k_wifi7_hal_wbm_desc_parse_err(dp, rx_desc,
							  &err_info);
		if (ret) {
			DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_DESC_PARSE_ERROR], 1);
			ath12k_warn(ab,
				    "failed to parse rx error in wbm_rel ring desc %d\n",
				    ret);
			continue;
		}

		desc_info = err_info.rx_desc;

		/* retry manual desc retrieval if hw cc is not done */
		if (!desc_info) {
			DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_GET_SW_DESC_FROM_CK_ERROR], 1);
			desc_info = ath12k_dp_get_rx_desc(dp, err_info.cookie);
			if (!desc_info) {
				DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_GET_SW_DESC_ERROR], 1);
				ath12k_warn(ab, "Invalid cookie in DP WBM rx error descriptor retrieval: 0x%x\n",
					    err_info.cookie);
				continue;
			}
		}

		if (desc_info->magic != ATH12K_DP_RX_DESC_MAGIC)
			ath12k_warn(ab, "WBM RX err, Check HW CC implementation");

		msdu = desc_info->skb;
		desc_info->skb = NULL;

		device_id = desc_info->device_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		if (unlikely(!partner_dp)) {
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, WBM_ERR_DROP_NULL_PARTNER_DP);

			/* In any case continuation bit is set
			 * in the previous record, cleanup scatter_msdu_list
			 */
			ath12k_wifi7_dp_clean_up_skb_list(&scatter_msdu_list);
			continue;
		}

		list_add_tail(&desc_info->list, &rx_desc_used_list[device_id]);

		rxcb = ATH12K_SKB_RXCB(msdu);
		ath12k_core_dma_unmap_single(partner_dp->dev, desc_info->paddr,
					     DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE);

		num_buffs_reaped[device_id]++;
		total_num_buffs_reaped++;

		if (!err_info.continuation)
			budget--;

		msdu_data = (struct hal_rx_desc *)msdu->data;
		rxcb->err_rel_src = err_info.err_rel_src;
		rxcb->err_code = err_info.err_code;
		rxcb->is_first_msdu = err_info.first_msdu;
		rxcb->is_last_msdu = err_info.last_msdu;
		rxcb->is_continuation = err_info.continuation;
		rxcb->is_frag = desc_info->is_frag;
		rxcb->peer_id =
		ath12k_wifi7_dp_rx_get_peer_id(ab, dp->peer_metadata_ver,
					       err_info.peer_metadata);
		rxcb->rx_desc = msdu_data;

		desc_info->is_frag = 0;

		if (err_info.continuation) {
			__skb_queue_tail(&scatter_msdu_list, msdu);
			continue;
		}

		hw_link_id = ath12k_wifi7_dp_rx_get_msdu_src_link(partner_dp, rxcb->rx_desc);

		if (hw_link_id >= ATH12K_GROUP_MAX_RADIO) {
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, WBM_ERR_DROP_INVALID_HW_ID);

			/* In any case continuation bit is set
			 * in the previous record, cleanup scatter_msdu_list
			 */
			ath12k_wifi7_dp_clean_up_skb_list(&scatter_msdu_list);
			continue;
		}

		if (!skb_queue_empty(&scatter_msdu_list)) {
			struct sk_buff *msdu;

			skb_queue_walk(&scatter_msdu_list, msdu) {
				rxcb = ATH12K_SKB_RXCB(msdu);
				rxcb->hw_link_id = hw_link_id;
			}

			skb_queue_splice_tail_init(&scatter_msdu_list,
						   &msdu_list);
		}

		rxcb = ATH12K_SKB_RXCB(msdu);
		rxcb->hw_link_id = hw_link_id;
		__skb_queue_tail(&msdu_list, msdu);
	}

	/* In any case continuation bit is set in the
	 * last record, cleanup scatter_msdu_list
	 */
	ath12k_wifi7_dp_clean_up_skb_list(&scatter_msdu_list);

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	if (!total_num_buffs_reaped)
		goto done;

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
		if (!num_buffs_reaped[device_id])
			continue;

		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		rx_ring = &partner_dp->rx_refill_buf_ring;

		ath12k_dp_rx_bufs_replenish(partner_dp, rx_ring,
					    &rx_desc_used_list[device_id]);
	}

	rcu_read_lock();
	while ((msdu = __skb_dequeue(&msdu_list))) {
		rxcb = ATH12K_SKB_RXCB(msdu);
		hw_link_id = rxcb->hw_link_id;

		device_id = hw_links[hw_link_id].device_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		if (unlikely(!partner_dp)) {
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "Unable to process WBM error msdu due to invalid hw link id %d device id %d\n",
				   hw_link_id, device_id);
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, WBM_ERR_DROP_PROCESS_NULL_PARTNER_DP);
			continue;
		}

		pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
						      hw_links[hw_link_id].pdev_idx);
		dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
		if (!dp_pdev) {
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, WBM_ERR_DROP_NULL_PDEV);
			continue;
		}
		ar = dp_pdev->ar;

		if (!ar || !rcu_dereference(ar->ab->pdevs_active[pdev_id])) {
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, WBM_ERR_DROP_NULL_AR);
			continue;
		}

		if (test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) {
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, WBM_ERR_DROP_CAC_RUNNING);
			continue;
		}

		if (rxcb->err_rel_src < HAL_WBM_REL_SRC_MODULE_MAX) {
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				rcu_read_lock();
				link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp,
										     dp_pdev,
										     rxcb->peer_id);
				if (link_peer) {
					ahvif = ath12k_vif_to_ahvif(link_peer->vif);
					ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu->len,
							    ATH_RX_TOTAL_PKTS);
					ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu->len,
							    ATH_RX_WBM_REL_TOTAL);
				}
				rcu_read_unlock();
			}
			device_stats->rx_wbm_rel_source[rxcb->err_rel_src][ar->ab->device_id]++;
		}

		ath12k_wifi7_dp_rx_wbm_err(dp_pdev, napi, msdu, &msdu_list);
	}

	rcu_read_unlock();
done:
	return total_num_buffs_reaped;
}

int ath12k_wifi7_dp_alloc_reo_qdesc(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
				    enum hal_pn_type pn_type,
				    struct hal_rx_reo_queue **addr_aligned)
{
	u8 tid = rx_tid->tid;
	u32 ba_win_sz = rx_tid->ba_win_sz;
	void *vaddr;
	u32 hw_desc_sz;
	dma_addr_t paddr;
	int ret;

	/* TODO: Optimize the memory allocation for qos tid based on
	 * the actual BA window size in REO tid update path.
	 */
	if (tid == HAL_DESC_REO_NON_QOS_TID)
		hw_desc_sz = ath12k_wifi7_hal_reo_qdesc_size(ba_win_sz, tid);
	else
		hw_desc_sz = ath12k_wifi7_hal_reo_qdesc_size(DP_BA_WIN_SZ_MAX, tid);

	vaddr = kzalloc(hw_desc_sz + HAL_LINK_DESC_ALIGN - 1, GFP_ATOMIC);
	if (!vaddr)
		return -ENOMEM;

	*addr_aligned = PTR_ALIGN(vaddr, HAL_LINK_DESC_ALIGN);
	ath12k_wifi7_hal_reo_qdesc_setup(*addr_aligned, tid, ba_win_sz, ssn, pn_type);
#ifndef CONFIG_IO_COHERENCY
	paddr = dma_map_single(ab->dev, *addr_aligned, hw_desc_sz,
			       DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(ab->dev, paddr);
	if (ret) {
		kfree(vaddr);
		return ret;
	}
#else
	paddr = virt_to_phys(*addr_aligned);
	if (!paddr) {
		kfree(vaddr);
		return ret;
	}
#endif
	rx_tid->vaddr = vaddr;
	rx_tid->paddr = paddr;
	rx_tid->size = hw_desc_sz;

	return 0;
}

int ath12k_wifi7_dp_rxdma_ring_sel_config_qcn9274(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;

	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;

	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;
	tlv_filter.fp_ctrl_filter = FILTER_CTRL_BA_REQ;
	tlv_filter.fp_data_filter = FILTER_DATA_UCAST | FILTER_DATA_MCAST |
				    FILTER_DATA_NULL;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_mpdu_start_offset =
		ath12k_wifi7_hal_rx_desc_get_mpdu_start_offset_qcn9274();
	tlv_filter.rx_msdu_end_offset =
		ath12k_wifi7_hal_rx_desc_get_msdu_end_offset_qcn9274();

	tlv_filter.rx_mpdu_start_wmask =
			ath12k_wifi7_hal_rx_mpdu_start_wmask_get_qcn9274();
	tlv_filter.rx_msdu_end_wmask =
			ath12k_wifi7_hal_rx_msdu_end_wmask_get_qcn9274();

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "Configuring compact tlv masks rx_mpdu_start_wmask 0x%x rx_msdu_end_wmask 0x%x\n",
		   tlv_filter.rx_mpdu_start_wmask, tlv_filter.rx_msdu_end_wmask);

	ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, 0,
					       HAL_RXDMA_BUF,
					       DP_RX_BUFFER_SIZE,
					       &tlv_filter);

	return ret;
}

int ath12k_wifi7_dp_rxdma_ring_sel_config_wcn7850(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret = 0;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;
	int i;

	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;

	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;
	tlv_filter.fp_ctrl_filter = FILTER_CTRL_BA_REQ;
	tlv_filter.fp_data_filter = FILTER_DATA_UCAST | FILTER_DATA_MCAST |
				    FILTER_DATA_NULL;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_header_offset = offsetof(struct hal_rx_desc_wcn7850, pkt_hdr_tlv);

	tlv_filter.rx_mpdu_start_offset =
		ath12k_wifi7_hal_rx_desc_get_mpdu_start_offset_wcn7850();
	tlv_filter.rx_msdu_end_offset =
		ath12k_wifi7_hal_rx_desc_get_msdu_end_offset_wcn7850();

	/* TODO: Selectively subscribe to required qwords within msdu_end
	 * and mpdu_start and setup the mask in below msg
	 * and modify the rx_desc struct
	 */

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ring_id = dp->rx_mac_buf_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, i,
						       HAL_RXDMA_BUF,
						       DP_RX_BUFFER_SIZE,
						       &tlv_filter);
	}

	return ret;
}

void ath12k_wifi7_dp_rx_process_reo_status(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_tlv_64_hdr *hdr;
	struct hal_srng *srng;
	struct ath12k_dp_rx_reo_cmd *cmd, *tmp;
	bool found = false;
	u16 tag;
	struct hal_reo_status reo_status;

	srng = &ab->hal.srng_list[dp->reo_status_ring.ring_id];

	memset(&reo_status, 0, sizeof(reo_status));

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while ((hdr = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		tag = le64_get_bits(hdr->tl, HAL_SRNG_TLV_HDR_TAG);

		switch (tag) {
		case HAL_REO_GET_QUEUE_STATS_STATUS:
			ath12k_wifi7_hal_reo_status_queue_stats(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_QUEUE_STATUS:
			ath12k_wifi7_hal_reo_flush_queue_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_CACHE_STATUS:
			ath12k_wifi7_hal_reo_flush_cache_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_UNBLOCK_CACHE_STATUS:
			ath12k_wifi7_hal_reo_unblk_cache_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_TIMEOUT_LIST_STATUS:
			ath12k_wifi7_hal_reo_flush_timeout_list_status(ab, hdr,
								       &reo_status);
			break;
		case HAL_REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS:
			ath12k_wifi7_hal_reo_desc_thresh_reached_status(ab, hdr,
									&reo_status);
			break;
		case HAL_REO_UPDATE_RX_REO_QUEUE_STATUS:
			ath12k_wifi7_hal_reo_update_rx_reo_queue_status(ab, hdr,
									&reo_status);
			break;
		default:
			ath12k_warn(ab, "Unknown reo status type %d\n", tag);
			continue;
		}

		spin_lock_bh(&dp->reo_cmd_lock);
		list_for_each_entry_safe(cmd, tmp, &dp->reo_cmd_list, list) {
			if (reo_status.uniform_hdr.cmd_num == cmd->cmd_num) {
				found = true;
				list_del(&cmd->list);
				break;
			}
		}
		spin_unlock_bh(&dp->reo_cmd_lock);

		if (found) {
			cmd->handler(dp, (void *)&cmd->data,
				     reo_status.uniform_hdr.cmd_status);
			kfree(cmd);
		}

		found = false;
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);
}

int ath12k_wifi7_dp_rx_fst_attach(struct ath12k_dp *dp, struct dp_rx_fst *fst)
{
	struct ath12k_base *ab = dp->ab;

	fst->num_entries = 0;

	fst->base = kcalloc(HAL_RX_FLOW_SEARCH_TABLE_SIZE,
			    sizeof(struct dp_rx_fse), GFP_KERNEL);
	if (!fst->base)
		return -ENOMEM;

	fst->hal_rx_fst = ath12k_wifi7_hal_rx_fst_attach(ab);
	if (!fst->hal_rx_fst) {
		ath12k_err(ab, "Rx Hal fst allocation failed\n");
		kfree(fst->base);
		return -ENOMEM;
	}

	return 0;
}

void ath12k_wifi7_dp_rx_fst_detach(struct ath12k_dp *dp, struct dp_rx_fst *fst)
{
	struct ath12k_base *ab = dp->ab;

	ath12k_wifi7_hal_rx_fst_detach(ab, fst->hal_rx_fst);
	kfree(fst->base);
}

void ath12k_wifi7_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					struct rx_flow_info *flow_info)
{
	struct hal_flow_tuple_info *tuple_info = &flow_info->flow_tuple_info;
	struct ath12k_base *ab = dp->ab;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Dest IP address %x:%x:%x:%x",
		   tuple_info->dest_ip_127_96,
		   tuple_info->dest_ip_95_64,
		   tuple_info->dest_ip_63_32,
		   tuple_info->dest_ip_31_0);
	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Source IP address %x:%x:%x:%x",
		   tuple_info->src_ip_127_96,
		   tuple_info->src_ip_95_64,
		   tuple_info->src_ip_63_32,
		   tuple_info->src_ip_31_0);
	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Dest port %u, Src Port %u, Protocol %u",
		   tuple_info->dest_port,
		   tuple_info->src_port,
		   tuple_info->l4_protocol);
}

static
u32 ath12k_dp_rx_flow_compute_flow_hash(struct ath12k_base *ab,
					struct dp_rx_fst *fst,
					struct rx_flow_info *rx_flow_info,
					struct hal_rx_flow *flow)
{
	memcpy(&flow->tuple_info, &rx_flow_info->flow_tuple_info,
	       sizeof(struct hal_flow_tuple_info));

	return ath12k_wifi7_hal_flow_toeplitz_hash(ab, fst->hal_rx_fst,
						   &flow->tuple_info);
}

static inline struct dp_rx_fse *
ath12k_dp_rx_flow_get_fse(struct dp_rx_fst *fst, u32 flow_hash)
{
	struct dp_rx_fse *fse;
	u32 idx = ath12k_wifi7_hal_rx_get_trunc_hash(fst->hal_rx_fst, flow_hash);

	fse = (struct dp_rx_fse *)fst->base;
	return &fse[idx];
}

struct dp_rx_fse *
ath12k_dp_rx_flow_find_entry_by_tuple(struct ath12k_base *ab,
				      struct dp_rx_fst *fst,
				      struct rx_flow_info *flow_info,
				      struct hal_rx_flow *flow)
{
	u32 flow_hash;
	u32 flow_idx;
	int status;

	flow_hash = ath12k_dp_rx_flow_compute_flow_hash(ab, fst, flow_info, flow);

	status = ath12k_wifi7_hal_rx_find_flow_from_tuple(ab, fst->hal_rx_fst,
							  flow_hash,
							  &flow_info->flow_tuple_info,
							  &flow_idx);
	if (status != 0) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Could not find tuple with hash %u", flow_hash);
		ath12k_wifi7_dp_rx_flow_dump_entry(ab->dp, flow_info);
		return NULL;
	}

	return ath12k_dp_rx_flow_get_fse(fst, flow_idx);
}

ssize_t ath12k_wifi7_dp_dump_fst_table(struct ath12k_dp *dp, char *buf, int size)
{
	struct ath12k_base *ab = dp->ab;
	struct dp_rx_fst *fst = ab->ag->dp_hw_grp->fst;
	int len = 0;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	len += scnprintf(buf + len, size - len,
			 "Number of entries in FST table: %d\n", fst->num_entries);
	len += ath12k_wifi7_hal_rx_dump_fst_table(ab, fst->hal_rx_fst, buf + len, size - len);

	return len;
}

bool
ath12k_wifi7_dp_rx_check_if_flow_to_be_updated(struct dp_rx_fse *fse,
					       struct rx_flow_info *flow_info)
{
	struct hal_rx_fse *hal_fse = fse->hal_fse;
	u32 use_ppe;

	use_ppe = u32_get_bits(hal_fse->info2, HAL_RX_FSE_SERVICE_CODE);

	/* If everything matches, it is a duplicate flow request,
	 * no need to modify anything,
	 * else modify the flow entry.
	 */
	if (use_ppe == flow_info->use_ppe &&
	    (hal_fse->metadata & flow_info->fse_metadata))
		return false;

	return true;
}

struct dp_rx_fse *
ath12k_wifi7_dp_rx_flow_alloc_entry(struct ath12k_base *ab,
				    struct dp_rx_fst *fst,
				    struct rx_flow_info *flow_info,
				    struct hal_rx_flow *flow)
{
	struct dp_rx_fse *fse;
	u32 flow_hash;
	u32 flow_idx;
	int status;

	flow_hash = ath12k_dp_rx_flow_compute_flow_hash(ab, fst, flow_info, flow);

	status = ath12k_wifi7_hal_rx_flow_insert_entry(ab, fst->hal_rx_fst, flow_hash,
						       &flow_info->flow_tuple_info,
						       &flow_idx);
	if (status != 0) {
		if (status == -EEXIST) {
			/* Even if the flow tuple info exists, there is a possibility
			 * that the flow entry has to be updated - so check
			 * for rx_flow_info if it matches exactly with the programmed
			 * fse entry
			 */
			fse = ath12k_dp_rx_flow_get_fse(fst, flow_idx);

			if (ath12k_wifi7_dp_rx_check_if_flow_to_be_updated(fse,
									   flow_info)) {
				ath12k_dbg(ab, ATH12K_DBG_DP_FST,
					   "Flow entry to be updated - hash %u",
					   flow_hash);
				return fse;
			}
		}
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Add entry failed with status %d for tuple with hash %u",
			   status, flow_hash);
		return NULL;
	}

	fse = ath12k_dp_rx_flow_get_fse(fst, flow_idx);
	fse->flow_hash = flow_hash;
	fse->flow_id = flow_idx;
	fse->is_valid = true;

	return fse;
}

static int ath12k_dp_fst_get_reo_indication(struct ath12k_dp *dp)
{
	u8 reo_indication;

	reo_indication = dp->fst_config.fst_core_map[dp->fst_config.core_idx] + 1;
	dp->fst_config.core_idx = (dp->fst_config.core_idx + 1) %
					dp->fst_config.fst_num_cores;

	return reo_indication;
}

int ath12k_wifi7_dp_rx_flow_add_entry(struct ath12k_dp *dp,
				      struct rx_flow_info *flow_info)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	struct dp_rx_fse *fse;

	/* Allocate entry in DP FST */
	fse = ath12k_wifi7_dp_rx_flow_alloc_entry(ab, fst, flow_info, &flow);
	if (!fse) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "RX FSE alloc failed");
		return -ENOMEM;
	}

	flow.drop = flow_info->drop;

	/* Reo indication is required only when drop bit is not set */
	if (!flow.drop) {
		if (flow_info->ring_id && flow_info->ring_id <= DP_REO_DST_RING_MAX)
			flow.reo_indication = flow_info->ring_id;
		else
			flow.reo_indication = ath12k_dp_fst_get_reo_indication(dp);
	}

	fse->reo_indication = flow.reo_indication;
	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;
	flow.fse_metadata |= flow_info->fse_metadata;
	if (flow_info->use_ppe) {
		flow.use_ppe = flow_info->use_ppe;
		flow.service_code = PPE_DRV_SC_SPF_BYPASS;
	}

	fse->hal_fse = ath12k_wifi7_hal_rx_flow_setup_fse(ab, fst->hal_rx_fst,
							  fse->flow_id, &flow);
	if (!fse->hal_fse) {
		ath12k_err(ab, "Unable to alloc FSE entry");
		fse->is_valid = false;
		return -EEXIST;
	}

	fst->num_entries++;
	fst->flows_per_reo[fse->reo_indication - 1]++;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d, reo_dest_ind = %d, reo_dest_hand = %u",
		   fst->num_entries, flow.reo_indication,
		   flow.reo_destination_handler);

	return 0;
}

int ath12k_wifi7_dp_rx_flow_delete_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fse *fse;
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;

	fse = ath12k_dp_rx_flow_find_entry_by_tuple(ab, fst, flow_info, &flow);
	if (!fse || !fse->is_valid) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "RX flow delete entry failed");
		return -EINVAL;
	}

	/* Delete the FSE in HW FST */
	ath12k_wifi7_hal_rx_flow_delete_entry(ab, fse->hal_fse);

	/* mark the FSE entry as invalid */
	fse->is_valid = false;

	/* Decrement number of valid entries in table */
	fst->num_entries--;
	fst->flows_per_reo[fse->reo_indication - 1]--;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);

	return 0;
}

int ath12k_wifi7_dp_rx_flow_delete_all_entries(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	struct dp_rx_fse *fse;
	int i;

	fse = (struct dp_rx_fse *)fst->base;
	if (!fse)
		return -ENODEV;

	for (i = 0; i < fst->hal_rx_fst->max_entries; i++, fse++) {
		if (!fse->is_valid)
			continue;

		ath12k_wifi7_hal_rx_flow_delete_entry(ab, fse->hal_fse);

		fse->is_valid = false;

		fst->num_entries--;
		fst->flows_per_reo[fse->reo_indication - 1]--;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);

	return 0;
}

int ath12k_wifi7_dp_peer_migrate_reo_cmd(struct ath12k_dp *dp,
					 struct ath12k_dp_link_peer *peer,
					 u16 peer_id, u8 chip_id)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_base *ab = dp->ab;
	int ret, tid;

	for (tid = 0; tid <= IEEE80211_NUM_TIDS; tid++) {
		rx_tid = &peer->dp_peer->rx_tid[tid];

		ath12k_wifi7_peer_rx_tid_qref_reset(ab,
						    peer->mlo ? peer->ml_id :
					    peer->peer_id, tid);
		ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(ab);

		cmd.addr_lo = lower_32_bits(rx_tid->paddr);
		cmd.addr_hi = upper_32_bits(rx_tid->paddr);
		cmd.flag |= HAL_REO_CMD_FLG_NEED_STATUS;
		ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid,
						   HAL_REO_CMD_FLUSH_QUEUE,
						   &cmd, NULL);
		if (ret) {
			ath12k_warn(ab, "failed to flush rx tid queue, tid %d (%d)\n",
				    rx_tid->tid, ret);
			return ret;
		}
	}

	cmd.flag = 0;
	rx_tid = &peer->dp_peer->rx_tid[0];
	rx_tid->chip_id = chip_id;
	rx_tid->peer_id = peer_id;

	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag |= HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.flag |= HAL_REO_CMD_FLG_FLUSH_ALL;

	ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid,
					   HAL_REO_CMD_FLUSH_CACHE,
					   &cmd,
					   ath12k_dp_primary_peer_migrate_setup);
	if (ret) {
		ath12k_warn(ab, "failed to flush cache for peer_id %x\n", peer->peer_id);
		return ret;
	}

	cmd.flag = 0;
	cmd.flag = HAL_REO_CMD_FLG_UNBLK_CACHE;

	ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid,
					  HAL_REO_CMD_UNBLOCK_CACHE,
					  &cmd, NULL);
	if (ret)
		ath12k_warn(ab, "failed to unblock cache for peer_id %x\n", peer->peer_id);

	return ret;
}

int ath12k_wifi7_dp_rx_htt_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 ring_id;
	int i, ret;

	/* TODO: Need to verify the HTT setup for QCN9224 */
	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, 0, HAL_RXDMA_BUF);
	if (ret) {
		ath12k_warn(ab, "failed to configure rx_refill_buf_ring %d\n",
			    ret);
		return ret;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ring_id = dp->rx_mac_buf_ring[i].ring_id;
			ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
							  i, HAL_RXDMA_BUF);
			if (ret) {
				ath12k_warn(ab, "failed to configure rx_mac_buf_ring%d %d\n",
					    i, ret);
				return ret;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ring_id = dp->rxdma_err_dst_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
						  i, HAL_RXDMA_DST);
		if (ret) {
			ath12k_warn(ab, "failed to configure rxdma_err_dest_ring%d %d\n",
				    i, ret);
			return ret;
		}
	}

	ret = ath12k_dp_mon_rx_htt_setup(dp);
	if (ret) {
		ath12k_warn(ab, "Failed to setup rxdma monitor rings\n");
		return ret;
	}

	ret = ab->hw_params->hw_ops->rxdma_ring_sel_config(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup rxdma ring selection config\n");
		return ret;
	}

	return 0;
}

void ath12k_wifi7_dp_pdev_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k *ar;
	int i;

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], NULL);
	}
	spin_unlock_bh(&dp->dp_lock);

	synchronize_rcu();

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		ath12k_fw_stats_free(&ar->fw_stats);

		if (ar->dp.dp_mon_pdev_configured) {
			ath12k_dp_mon_pdev_rx_free(&ar->dp);
			ath12k_dp_mon_pdev_deinit(&ar->dp);

			ar->dp.dp_mon_pdev_configured = false;
		}
	}

	ath12k_dp_ppeds_stop(ab);
}

int ath12k_wifi7_dp_pdev_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k *ar;
	int ret, i, j;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_dp_ppe_rxole_rxdma_cfg(ab);
	if (ret) {
		ath12k_err(ab, "Failed to send htt RxOLE and RxDMA messages to target :%d\n",
			   ret);
		goto out;
	}
#endif

	ret = ath12k_wifi7_dp_rx_htt_setup(ab);
	if (ret)
		goto out;

	/* TODO: Per-pdev rx ring unlike tx ring which is mapped to different AC's */
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;

		memset(&ar->stats, 0, sizeof(struct ath12k_pdev_ctrl_path_stats));
		dp_pdev = &ar->dp;

		dp_pdev->hw = ar->ah->hw;
		dp_pdev->dp = dp;
		/* Below linking is a temporary linking to handle few cases like cac
		 * timeout, active pdev etc in dp rx. Some flags/fileds can be added
		 * in dp_pdev to remove ar dependencies in the performance critical
		 * path.
		 *
		 * TODO: remove this once those dependencies are resolved.
		 */
		dp_pdev->ar = ar;
		dp_pdev->dp_hw = &ar->ah->dp_hw;
		dp_pdev->hw_link_id = ar->hw_link_id;

		/* Enable enable_dp_stats by default */
		ar->dp.dp_stats_mask |= DP_ENABLE_STATS;

		if (!dp_pdev->dp_mon_pdev_configured) {
			ret = ath12k_dp_mon_pdev_init(dp_pdev);
			if (ret) {
				ath12k_warn(ab, "failed to initialize mon pdev %d\n", i);
				goto err_cleanup_pdevs;
			}

			ret = ath12k_dp_mon_pdev_rx_alloc(dp_pdev, i);
			if (ret) {
				ath12k_warn(ab,
					    "failed to alloc rx filter for pdev %d\n",
					    i);
				goto err_mon_pdev_deinit;
			}

			ret = ath12k_dp_mon_pdev_rx_htt_setup(dp_pdev, i);
			if (ret) {
				ath12k_warn(ab,
					    "failed to setup rx htt for pdev %d\n", i);
				goto err_mon_pdev_rx_free;
			}

			dp_pdev->dp_mon_pdev_configured = true;
		}
	}

	ret = ath12k_dp_ppeds_start(ab);
	if (ret) {
		ath12k_err(ab, "failed to start DP PPEDS\n");
		goto err_cleanup_pdevs;
	}

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], &ar->dp);
	}
	spin_unlock_bh(&dp->dp_lock);

	dp->num_radios = ab->num_radios;

	return ret;
err_mon_pdev_rx_free:
	/* Clean up the current pdev's RX allocation */
	ath12k_dp_mon_pdev_rx_free(dp_pdev);
err_mon_pdev_deinit:
	/* Clean up the current pdev's monitor initialization */
	ath12k_dp_mon_pdev_deinit(dp_pdev);
err_cleanup_pdevs:
	/* Clean up all previously configured pdevs */
	for (j = 0; j < i; j++) {
		ar = ab->pdevs[j].ar;
		if (ar->dp.dp_mon_pdev_configured) {
			ath12k_dp_mon_pdev_rx_free(&ar->dp);
			ath12k_dp_mon_pdev_deinit(&ar->dp);
			ar->dp.dp_mon_pdev_configured = false;
		}
	}

	ath12k_dp_ppeds_stop(ab);
out:
	return ret;
}

void ath12k_wifi7_dp_rx_ring_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;

	ath12k_dp_srng_cleanup(ab, &dp->rx_refill_buf_ring.refill_buf_ring);

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		if (ab->hw_params->rx_mac_buf_ring)
			ath12k_dp_srng_cleanup(ab, &dp->rx_mac_buf_ring[i]);
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++)
		ath12k_dp_srng_cleanup(ab, &dp->rxdma_err_dst_ring[i]);

	ath12k_dp_srng_cleanup(ab, &dp->rx_rel_ring);
	ath12k_dp_rx_reo_cleanup(ab);
}

int ath12k_wifi7_dp_rx_ring_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret;

	ret = ath12k_dp_rx_reo_setup(ab);
	if (ret) {
		ath12k_err(ab, "failed to initialize reo destination rings: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_setup(ab, &dp->rx_rel_ring, HAL_WBM2SW_RELEASE,
				   HAL_WBM2SW_REL_ERR_RING_NUM, 0,
				   DP_RX_RELEASE_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up rx_rel ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_setup(ab,
				   &dp->rx_refill_buf_ring.refill_buf_ring,
				   HAL_RXDMA_BUF, 0, 0,
				   DP_RXDMA_BUF_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to setup rx_refill_buf_ring\n");
		return ret;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ret = ath12k_dp_srng_setup(ab,
						   &dp->rx_mac_buf_ring[i],
						   HAL_RXDMA_BUF, 1,
						   i, DP_RX_MAC_BUF_RING_SIZE);
			if (ret) {
				ath12k_warn(ab, "failed to setup rx_mac_buf_ring %d\n",
					    i);
				return ret;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ret = ath12k_dp_srng_setup(ab, &dp->rxdma_err_dst_ring[i],
					   HAL_RXDMA_DST, 0, i,
					   DP_RXDMA_ERR_DST_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to setup rxdma_err_dst_ring %d\n", i);
			return ret;
		}
	}

	ret = ath12k_dp_rxdma_buf_setup(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup rxdma ring\n");
		return ret;
	}

	return 0;
}
