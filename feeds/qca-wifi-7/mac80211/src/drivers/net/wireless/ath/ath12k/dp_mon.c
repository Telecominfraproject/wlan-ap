// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "dp_mon.h"
#include "debug.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "peer.h"
#include "debugfs.h"
#include "dp_mon_filter.h"

static inline u32
ath12k_dp_mon_rx_ul_ofdma_ru_size_to_width(enum ath12k_eht_ru_size ru_size)
{
	switch (ru_size) {
	case ATH12K_EHT_RU_26:
		return RU_26;
	case ATH12K_EHT_RU_52:
		return RU_52;
	case ATH12K_EHT_RU_52_26:
		return RU_52_26;
	case ATH12K_EHT_RU_106:
		return RU_106;
	case ATH12K_EHT_RU_106_26:
		return RU_106_26;
	case ATH12K_EHT_RU_242:
		return RU_242;
	case ATH12K_EHT_RU_484:
		return RU_484;
	case ATH12K_EHT_RU_484_242:
		return RU_484_242;
	case ATH12K_EHT_RU_996:
		return RU_996;
	case ATH12K_EHT_RU_996_484:
		return RU_996_484;
	case ATH12K_EHT_RU_996_484_242:
		return RU_996_484_242;
	case ATH12K_EHT_RU_996x2:
		return RU_2X996;
	case ATH12K_EHT_RU_996x2_484:
		return RU_2X996_484;
	case ATH12K_EHT_RU_996x3:
		return RU_3X996;
	case ATH12K_EHT_RU_996x3_484:
		return RU_3X996_484;
	case ATH12K_EHT_RU_996x4:
		return RU_4X996;
	default:
		return RU_INVALID;
	}
}

void
ath12k_dp_mon_fill_rx_stats_info(struct hal_rx_mon_ppdu_info *ppdu_info,
				 struct ieee80211_rx_status *rx_status)
{
	u32 center_freq = ppdu_info->freq;

	rx_status->freq = center_freq;
	rx_status->bw = ath12k_mac_bw_to_mac80211_bw(ppdu_info->bw);
	rx_status->nss = ppdu_info->nss;
	rx_status->rate_idx = 0;
	rx_status->encoding = RX_ENC_LEGACY;
	rx_status->flag |= RX_FLAG_NO_SIGNAL_VAL;

	if (center_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    center_freq <= ATH12K_MAX_6GHZ_FREQ) {
		rx_status->band = NL80211_BAND_6GHZ;
	} else if (center_freq >= ATH12K_MIN_2GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_2GHZ_FREQ) {
		rx_status->band = NL80211_BAND_2GHZ;
	} else if (center_freq >= ATH12K_MIN_5GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_5GHZ_FREQ) {
		rx_status->band = NL80211_BAND_5GHZ;
	} else {
		rx_status->band = NUM_NL80211_BANDS;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_fill_rx_stats_info);

static void
ath12k_dp_mon_rx_update_radiotap_he(struct hal_rx_mon_ppdu_info *rx_status,
				    u8 *rtap_buf)
{
	u32 rtap_len = 0;

	put_unaligned_le16(rx_status->he_data1, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data2, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data3, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data4, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data5, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data6, &rtap_buf[rtap_len]);
}

static void
ath12k_dp_mon_rx_update_radiotap_he_mu(struct hal_rx_mon_ppdu_info *rx_status,
				       u8 *rtap_buf)
{
	u32 rtap_len = 0;

	put_unaligned_le16(rx_status->he_flags1, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_flags2, &rtap_buf[rtap_len]);
	rtap_len += 2;

	rtap_buf[rtap_len] = rx_status->he_RU[0];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[1];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[2];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[3];
}

void ath12k_dp_mon_update_radiotap(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_rx_mon_ppdu_info *ppduinfo,
				   struct sk_buff *mon_skb,
				   struct ieee80211_rx_status *rxs)
{
	struct ieee80211_supported_band *sband;
	u8 *ptr = NULL;

	rxs->flag |= RX_FLAG_MACTIME_START;
	rxs->signal = (s8)(ppduinfo->rssi_comb + dp_pdev->ar->rssi_offsets.rssi_offset);
	rxs->nss = ppduinfo->nss + 1;

	if (ppduinfo->userstats[ppduinfo->userid].ampdu_present) {
		rxs->flag |= RX_FLAG_AMPDU_DETAILS;
		rxs->ampdu_reference = ppduinfo->userstats[ppduinfo->userid].ampdu_id;
	}

	if (ppduinfo->is_eht || ppduinfo->eht_usig) {
		struct ieee80211_radiotap_tlv *tlv;
		struct ieee80211_radiotap_eht *eht;
		struct ieee80211_radiotap_eht_usig *usig;
		u16 len = 0, i, eht_len, usig_len;
		u8 user;

		if (ppduinfo->is_eht) {
			eht_len = struct_size(eht,
					      user_info,
					      ppduinfo->eht_info.num_user_info);
			len += sizeof(*tlv) + eht_len;
		}

		if (ppduinfo->eht_usig) {
			usig_len = sizeof(*usig);
			len += sizeof(*tlv) + usig_len;
		}

		rxs->flag |= RX_FLAG_RADIOTAP_TLV_AT_END;
		rxs->encoding = RX_ENC_EHT;

		skb_reset_mac_header(mon_skb);

		tlv = skb_push(mon_skb, len);

		if (ppduinfo->is_eht) {
			tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT);
			tlv->len = cpu_to_le16(eht_len);

			eht = (struct ieee80211_radiotap_eht *)tlv->data;
			eht->known = ppduinfo->eht_info.eht.known;

			for (i = 0;
			     i < ARRAY_SIZE(eht->data) &&
			     i < ARRAY_SIZE(ppduinfo->eht_info.eht.data);
			     i++)
				eht->data[i] = ppduinfo->eht_info.eht.data[i];

			for (user = 0; user < ppduinfo->eht_info.num_user_info; user++)
				put_unaligned_le32(ppduinfo->eht_info.user_info[user],
						   &eht->user_info[user]);

			tlv = (struct ieee80211_radiotap_tlv *)&tlv->data[eht_len];
		}

		if (ppduinfo->eht_usig) {
			tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT_USIG);
			tlv->len = cpu_to_le16(usig_len);

			usig = (struct ieee80211_radiotap_eht_usig *)tlv->data;
			*usig = ppduinfo->u_sig_info.usig;
		}
	} else if (ppduinfo->he_mu_flags) {
		rxs->flag |= RX_FLAG_RADIOTAP_HE_MU;
		rxs->encoding = RX_ENC_HE;
		ptr = skb_push(mon_skb, sizeof(struct ieee80211_radiotap_he_mu));
		ath12k_dp_mon_rx_update_radiotap_he_mu(ppduinfo, ptr);
	} else if (ppduinfo->he_flags) {
		rxs->flag |= RX_FLAG_RADIOTAP_HE;
		rxs->encoding = RX_ENC_HE;
		ptr = skb_push(mon_skb, sizeof(struct ieee80211_radiotap_he));
		ath12k_dp_mon_rx_update_radiotap_he(ppduinfo, ptr);
		rxs->rate_idx = ppduinfo->rate;
	} else if (ppduinfo->vht_flags) {
		rxs->encoding = RX_ENC_VHT;
		rxs->rate_idx = ppduinfo->rate;
	} else if (ppduinfo->ht_flags) {
		rxs->encoding = RX_ENC_HT;
		rxs->rate_idx = ppduinfo->rate;
	} else {
		rxs->encoding = RX_ENC_LEGACY;
		sband = &dp_pdev->ar->mac.sbands[rxs->band];
		rxs->rate_idx = ath12k_mac_hw_rate_to_idx(sband, ppduinfo->rate,
							  ppduinfo->cck_flag);
	}

	rxs->mactime = ppduinfo->tsft;
}
EXPORT_SYMBOL(ath12k_dp_mon_update_radiotap);

void ath12k_dp_mon_rx_deliver_skb(struct ath12k_pdev_dp *dp_pdev,
				  struct napi_struct *napi,
				  struct sk_buff *msdu,
				  struct ieee80211_rx_status *status,
				  struct hal_rx_mon_ppdu_info *ppduinfo)
{
	static const struct ieee80211_radiotap_he known = {
		.data1 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
				     IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN),
		.data2 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN),
	};
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_radiotap_he *he = NULL;

	status->link_valid = 0;
	status->link_id = 0;

	if ((status->encoding == RX_ENC_HE) && !(status->flag & RX_FLAG_RADIOTAP_HE) &&
	    !(status->flag & RX_FLAG_SKIP_MONITOR)) {
		he = skb_push(msdu, sizeof(known));
		memcpy(he, &known, sizeof(known));
		status->flag |= RX_FLAG_RADIOTAP_HE;
	}

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (!napi)
		ieee80211_rx_ni(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
	else
		ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), NULL, msdu, napi);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_deliver_skb);

int ath12k_dp_mon_rx_set_pktlen(struct sk_buff *skb, u32 len)
{
	if (skb->len > len) {
		skb_trim(skb, len);
	} else {
		if (skb_tailroom(skb) < len - skb->len) {
			if ((pskb_expand_head(skb, 0,
					      len - skb->len - skb_tailroom(skb),
					      GFP_ATOMIC))) {
				return -ENOMEM;
			}
		}
		skb_put(skb, (len - skb->len));
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_set_pktlen);

static void
ath12k_dp_mon_handle_mon_desc(struct ath12k_dp *dp, struct ath12k_dp_mon_desc *mon_desc)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u8 *mon_buf = mon_desc->mon_buf;

	if (mon_buf) {
		ath12k_core_dma_unmap_page(dp->dev, mon_desc->paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE,
					   DMA_FROM_DEVICE);
		page_frag_free(mon_buf);
		dp_mon->num_frag_free++;
		mon_desc->mon_buf = NULL;
	}

	spin_lock_bh(&dp->dp_mon->mon_desc_lock);
	list_del(&mon_desc->list);
	list_add_tail(&mon_desc->list, &dp_mon->mon_desc_free_list);
	spin_unlock_bh(&dp->dp_mon->mon_desc_lock);
}

int ath12k_dp_mon_buf_replenish(struct ath12k_dp *dp,
				struct dp_rxdma_mon_ring *buf_ring,
				struct list_head *used_list,
				int req_entries)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_mon_buf_ring *mon_buf_desc;
	struct hal_srng *srng;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_mon_desc *mon_desc, *tmp_mon_desc;
	struct page *page;
	u64 offset;
	dma_addr_t paddr;
	int ret = 0;
	u8 *mon_buf;

	list_for_each_entry_safe(mon_desc, tmp_mon_desc, used_list, list) {
		if (unlikely(mon_desc->in_use != DP_MON_DESC_REPLENISH)) {
			ath12k_warn(dp,
				    "Invalid in_use %d, possibly desc from freelist\n",
				    mon_desc->in_use);
			ath12k_dp_mon_handle_mon_desc(dp, mon_desc);
			continue;
		}

		mon_buf = page_frag_alloc(&dp_mon->rx_mon_pf_cache,
					  ATH12K_DP_MON_RX_BUF_SIZE,
					  GFP_ATOMIC);
		if (unlikely(!mon_buf)) {
			ret = -ENOMEM;
			goto out;
		}

		page = virt_to_head_page(mon_buf);
		offset = ((void *)mon_buf) - page_address(page);
		paddr = ath12k_core_dma_map_page(ab->dev, page, offset,
						 ATH12K_DP_MON_RX_BUF_SIZE,
						 DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(ab->dev, paddr))) {
			page_frag_free(mon_buf);
			mon_desc->mon_buf = NULL;
			dp_mon->num_frag_free++;
			ret = -EIO;
			goto out;
		}

		mon_desc->mon_buf = mon_buf;
		mon_desc->paddr = paddr;
		mon_desc->magic = ATH12K_MON_MAGIC_VALUE;
		mon_desc->in_use = DP_MON_DESC_TO_HW;
		mon_desc->buf_len = 0;
		mon_desc->end_of_ppdu = 0;
		dp_mon->num_frag_replenish++;
	}

	srng = &ab->hal.srng_list[buf_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (req_entries > 0) {
		mon_desc = list_first_entry_or_null(used_list,
						    struct ath12k_dp_mon_desc, list);
		if (unlikely(!mon_desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		mon_buf_desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!mon_buf_desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		list_del(&mon_desc->list);
		mon_buf_desc->paddr_lo = cpu_to_le32(lower_32_bits(mon_desc->paddr));
		mon_buf_desc->paddr_hi = cpu_to_le32(upper_32_bits(mon_desc->paddr));
		mon_buf_desc->cookie = cpu_to_le64((uintptr_t)mon_desc);

		req_entries--;
	}

ring_unlock:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

out:
	if (unlikely(!list_empty(used_list))) {
		/* Reset the use flag */
		list_for_each_entry_safe(mon_desc, tmp_mon_desc, used_list, list) {
			mon_buf = mon_desc->mon_buf;
			if (mon_buf) {
				ath12k_core_dma_unmap_page(ab->dev, mon_desc->paddr,
							   ATH12K_DP_MON_RX_BUF_SIZE,
							   DMA_FROM_DEVICE);
				page_frag_free(mon_buf);
				mon_desc->mon_buf = NULL;
				dp_mon->num_frag_free++;
			}

			ath12k_dp_mon_desc_reset(mon_desc);
			mon_desc->in_use = DP_MON_DESC_H_REPLENISH_ERR;
		}

		spin_lock_bh(&dp_mon->mon_desc_lock);
		list_splice_tail(used_list, &dp_mon->mon_desc_free_list);
		spin_unlock_bh(&dp_mon->mon_desc_lock);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_buf_replenish);

size_t ath12k_dp_mon_list_cut_nodes(struct list_head *list, struct list_head *head,
				    size_t count)
{
	struct list_head *cur;
	struct ath12k_dp_mon_desc *mon_desc;
	size_t nodes = 0;

	if (!count) {
		INIT_LIST_HEAD(list);
		goto out;
	}

	list_for_each(cur, head) {
		if (!count)
			break;

		mon_desc = list_entry(cur, struct ath12k_dp_mon_desc, list);
		ath12k_dp_mon_desc_reset(mon_desc);
		mon_desc->in_use = DP_MON_DESC_REPLENISH;

		count--;
		nodes++;
	}

	list_cut_before(list, head, cur);

out:
	return nodes;
}

size_t ath12k_dp_mon_get_req_entries_from_buf_ring(struct ath12k_dp *dp,
						   struct dp_rxdma_mon_ring *rx_ring,
						   struct list_head *list)
{
	struct hal_srng *srng;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	size_t num_free, req_entries;

	srng = &dp->hal->srng_list[rx_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	if (!num_free) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return 0;
	}
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	spin_lock_bh(&dp_mon->mon_desc_lock);
	req_entries = ath12k_dp_mon_list_cut_nodes(list,
						   &dp_mon->mon_desc_free_list,
						   num_free);
	spin_unlock_bh(&dp_mon->mon_desc_lock);

	return req_entries;
}

static struct dp_mon_tx_ppdu_info *
ath12k_dp_mon_tx_get_ppdu_info(struct ath12k_mon_data *pmon,
			       unsigned int ppdu_id,
			       enum dp_mon_tx_ppdu_info_type type)
{
	struct dp_mon_tx_ppdu_info *tx_ppdu_info;

	if (type == DP_MON_TX_PROT_PPDU_INFO) {
		tx_ppdu_info = pmon->tx_prot_ppdu_info;

		if (tx_ppdu_info && !tx_ppdu_info->is_used)
			return tx_ppdu_info;
		kfree(tx_ppdu_info);
	} else {
		tx_ppdu_info = pmon->tx_data_ppdu_info;

		if (tx_ppdu_info && !tx_ppdu_info->is_used)
			return tx_ppdu_info;
		kfree(tx_ppdu_info);
	}

	/* allocate new tx_ppdu_info */
	tx_ppdu_info = kzalloc(sizeof(*tx_ppdu_info), GFP_ATOMIC);
	if (!tx_ppdu_info)
		return NULL;

	tx_ppdu_info->is_used = 0;
	tx_ppdu_info->tx_info.ppdu_id = ppdu_id;

	if (type == DP_MON_TX_PROT_PPDU_INFO)
		pmon->tx_prot_ppdu_info = tx_ppdu_info;
	else
		pmon->tx_data_ppdu_info = tx_ppdu_info;

	return tx_ppdu_info;
}

static struct dp_mon_tx_ppdu_info *
ath12k_dp_mon_hal_tx_ppdu_info(struct ath12k_mon_data *pmon,
			       u16 tlv_tag)
{
	switch (tlv_tag) {
	case HAL_TX_FES_SETUP:
	case HAL_TX_FLUSH:
	case HAL_PCU_PPDU_SETUP_INIT:
	case HAL_TX_PEER_ENTRY:
	case HAL_TX_QUEUE_EXTENSION:
	case HAL_TX_MPDU_START:
	case HAL_TX_MSDU_START:
	case HAL_TX_DATA:
	case HAL_MON_BUF_ADDR:
	case HAL_TX_MPDU_END:
	case HAL_TX_LAST_MPDU_FETCHED:
	case HAL_TX_LAST_MPDU_END:
	case HAL_COEX_TX_REQ:
	case HAL_TX_RAW_OR_NATIVE_FRAME_SETUP:
	case HAL_SCH_CRITICAL_TLV_REFERENCE:
	case HAL_TX_FES_SETUP_COMPLETE:
	case HAL_TQM_MPDU_GLOBAL_START:
	case HAL_SCHEDULER_END:
	case HAL_TX_FES_STATUS_USER_PPDU:
		break;
	case HAL_TX_FES_STATUS_PROT: {
		if (!pmon->tx_prot_ppdu_info->is_used)
			pmon->tx_prot_ppdu_info->is_used = true;

		return pmon->tx_prot_ppdu_info;
	}
	}

	if (!pmon->tx_data_ppdu_info->is_used)
		pmon->tx_data_ppdu_info->is_used = true;

	return pmon->tx_data_ppdu_info;
}

#define MAX_MONITOR_HEADER 512
#define MAX_DUMMY_FRM_BODY 128

struct sk_buff *ath12k_dp_mon_tx_alloc_skb(void)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(MAX_MONITOR_HEADER + MAX_DUMMY_FRM_BODY);
	if (!skb)
		return NULL;

	skb_reserve(skb, MAX_MONITOR_HEADER);

	if (!IS_ALIGNED((unsigned long)skb->data, 4))
		skb_pull(skb, PTR_ALIGN(skb->data, 4) - skb->data);

	return skb;
}

static int
ath12k_dp_mon_tx_gen_cts2self_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_cts *cts;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	cts = (struct ieee80211_cts *)skb->data;
	memset(cts, 0, MAX_DUMMY_FRM_BODY);
	cts->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTS);
	cts->duration = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(cts->ra, tx_ppdu_info->tx_info.rx_status.addr1, sizeof(cts->ra));

	skb_put(skb, sizeof(*cts));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_rts_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_rts *rts;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	rts = (struct ieee80211_rts *)skb->data;
	memset(rts, 0, MAX_DUMMY_FRM_BODY);
	rts->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_RTS);
	rts->duration = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(rts->ra, tx_ppdu_info->tx_info.rx_status.addr1, sizeof(rts->ra));
	memcpy(rts->ta, tx_ppdu_info->tx_info.rx_status.addr2, sizeof(rts->ta));

	skb_put(skb, sizeof(*rts));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_3addr_qos_null_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_qos_hdr *qhdr;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	qhdr = (struct ieee80211_qos_hdr *)skb->data;
	memset(qhdr, 0, MAX_DUMMY_FRM_BODY);
	qhdr->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC);
	qhdr->duration_id = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(qhdr->addr1, tx_ppdu_info->tx_info.rx_status.addr1, ETH_ALEN);
	memcpy(qhdr->addr2, tx_ppdu_info->tx_info.rx_status.addr2, ETH_ALEN);
	memcpy(qhdr->addr3, tx_ppdu_info->tx_info.rx_status.addr3, ETH_ALEN);

	skb_put(skb, sizeof(*qhdr));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_4addr_qos_null_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct dp_mon_qosframe_addr4 *qhdr;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	qhdr = (struct dp_mon_qosframe_addr4 *)skb->data;
	memset(qhdr, 0, MAX_DUMMY_FRM_BODY);
	qhdr->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC);
	qhdr->duration = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(qhdr->addr1, tx_ppdu_info->tx_info.rx_status.addr1, ETH_ALEN);
	memcpy(qhdr->addr2, tx_ppdu_info->tx_info.rx_status.addr2, ETH_ALEN);
	memcpy(qhdr->addr3, tx_ppdu_info->tx_info.rx_status.addr3, ETH_ALEN);
	memcpy(qhdr->addr4, tx_ppdu_info->tx_info.rx_status.addr4, ETH_ALEN);

	skb_put(skb, sizeof(*qhdr));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_ack_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct dp_mon_frame_min_one *fbmhdr;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	fbmhdr = (struct dp_mon_frame_min_one *)skb->data;
	memset(fbmhdr, 0, MAX_DUMMY_FRM_BODY);
	fbmhdr->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_CFACK);
	memcpy(fbmhdr->addr1, tx_ppdu_info->tx_info.rx_status.addr1, ETH_ALEN);

	/* set duration zero for ack frame */
	fbmhdr->duration = 0;

	skb_put(skb, sizeof(*fbmhdr));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_prot_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	int ret = 0;

	switch (tx_ppdu_info->tx_info.rx_status.medium_prot_type) {
	case DP_MON_TX_MEDIUM_RTS_LEGACY:
	case DP_MON_TX_MEDIUM_RTS_11AC_STATIC_BW:
	case DP_MON_TX_MEDIUM_RTS_11AC_DYNAMIC_BW:
		ret = ath12k_dp_mon_tx_gen_rts_frame(tx_ppdu_info);
		break;
	case DP_MON_TX_MEDIUM_CTS2SELF:
		ret = ath12k_dp_mon_tx_gen_cts2self_frame(tx_ppdu_info);
		break;
	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_3ADDR:
		ret = ath12k_dp_mon_tx_gen_3addr_qos_null_frame(tx_ppdu_info);
		break;
	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_4ADDR:
		ret = ath12k_dp_mon_tx_gen_4addr_qos_null_frame(tx_ppdu_info);
		break;
	}

	return ret;
}

static int
ath12k_dp_mon_tx_process_status_tlv(u32 tlv_status,
				    struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	int ret = 0;

	switch (tlv_status) {
	case HAL_TX_MON_MPDU_START:
		struct dp_mon_mpdu *mon_mpdu = tx_ppdu_info->tx_mon_mpdu;

		mon_mpdu = kzalloc(sizeof(*mon_mpdu), GFP_ATOMIC);
		break;
	case HAL_TX_MON_MPDU_END:
		list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
			      &tx_ppdu_info->dp_tx_mon_mpdu_list);
		break;
	case HAL_TX_MON_FES_STATUS_PROT:
		ret = ath12k_dp_mon_tx_gen_prot_frame(tx_ppdu_info);
		break;
	case HAL_TX_MON_FRAME_BITMAP_ACK:
		ret = ath12k_dp_mon_tx_gen_ack_frame(tx_ppdu_info);
		break;
	case HAL_RX_MON_RESPONSE_REQUIRED_INFO:
		if (tx_ppdu_info->tx_info.rx_status.reception_type == 0)
			ret = ath12k_dp_mon_tx_gen_cts2self_frame(tx_ppdu_info);
		break;
	}

	return ret;
}

static void
ath12k_dp_mon_tx_process_ppdu_info(struct ath12k_pdev_dp *dp_pdev,
				   struct napi_struct *napi,
				   struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct dp_mon_mpdu *tmp, *mon_mpdu;

	list_for_each_entry_safe(mon_mpdu, tmp,
				 &tx_ppdu_info->dp_tx_mon_mpdu_list, list) {
		list_del(&mon_mpdu->list);
		/* TODO: Call ath12k_dp_mon_rx_deliver while enabling TX monitor
		if (mon_mpdu->head)
			ath12k_dp_mon_rx_deliver(dp_pdev, mon_mpdu,
						 &tx_ppdu_info->tx_info.rx_status, napi);
		 */
		kfree(mon_mpdu);
	}
}

enum hal_tx_mon_status
ath12k_dp_mon_tx_parse_mon_status(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_mon_data *pmon,
				  struct sk_buff *skb,
				  struct napi_struct *napi,
				  u32 ppdu_id)
{
	struct dp_mon_tx_ppdu_info *tx_prot_ppdu_info, *tx_data_ppdu_info;
	struct dp_mon_tx_ppdu_info *tx_ppdu_info;
	struct hal_tlv_hdr *tlv;
	u8 *ptr = skb->data;
	u16 tlv_tag;
	u16 tlv_len;
	u32 tlv_userid = 0;
	u8 num_user;
	enum hal_tx_mon_status hal_status = HAL_TX_MON_STATUS_PPDU_NOT_DONE;

	tx_prot_ppdu_info = ath12k_dp_mon_tx_get_ppdu_info(pmon, ppdu_id,
							   DP_MON_TX_PROT_PPDU_INFO);
	if (!tx_prot_ppdu_info)
		return -ENOMEM;

	tlv = (struct hal_tlv_hdr *)ptr;
	tlv_tag = le32_get_bits(tlv->tl, HAL_TLV_HDR_TAG);

	hal_status = ath12k_hal_mon_tx_status_get_num_user(dp_pdev->dp->hal,
							   tlv_tag, tlv, &num_user);
	if (hal_status == HAL_TX_MON_STATUS_PPDU_NOT_DONE || !num_user)
		return -EINVAL;

	tx_data_ppdu_info = ath12k_dp_mon_tx_get_ppdu_info(pmon, ppdu_id,
							   DP_MON_TX_DATA_PPDU_INFO);
	if (!tx_data_ppdu_info)
		return -ENOMEM;

	do {
		tlv = (struct hal_tlv_hdr *)ptr;
		tlv_tag = le32_get_bits(tlv->tl, HAL_TLV_HDR_TAG);
		tlv_len = le32_get_bits(tlv->tl, HAL_TLV_HDR_LEN);
		tlv_userid = le32_get_bits(tlv->tl, HAL_TLV_USR_ID);

		tx_ppdu_info = ath12k_dp_mon_hal_tx_ppdu_info(pmon,
							      tlv_tag);

		hal_status = ath12k_hal_mon_tx_parse_status(dp_pdev->dp->hal,
							    &tx_ppdu_info->tx_info,
							    tlv_tag, ptr,
							    tlv_userid);
		ath12k_dp_mon_tx_process_status_tlv(hal_status,
						    tx_ppdu_info);

		ptr += tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_ALIGN);
		if ((ptr - skb->data) >= DP_TX_MONITOR_BUF_SIZE)
			break;
	} while (hal_status != HAL_TX_MON_FES_STATUS_END);

	ath12k_dp_mon_tx_process_ppdu_info(dp_pdev, napi, tx_data_ppdu_info);
	ath12k_dp_mon_tx_process_ppdu_info(dp_pdev, napi, tx_prot_ppdu_info);

	return hal_status;
}

static void
ath12k_dp_mon_rx_update_peer_rate_table_stats(struct ath12k_rx_peer_stats *rx_stats,
					      struct hal_rx_mon_ppdu_info *ppdu_info,
					      struct hal_rx_user_status *user_stats,
					      u32 num_msdu)
{
	struct ath12k_rx_peer_rate_stats *stats;
	u32 mcs_idx = (user_stats) ? user_stats->mcs : ppdu_info->mcs;
	u32 nss_idx = (user_stats) ? user_stats->nss - 1 : ppdu_info->nss - 1;
	u32 bw_idx = ppdu_info->bw;
	u32 gi_idx = ppdu_info->gi;
	u32 len;

	if (mcs_idx > HAL_RX_MAX_MCS_BE || nss_idx >= HAL_RX_MAX_NSS ||
	    bw_idx >= HAL_RX_BW_MAX || gi_idx >= HAL_RX_GI_MAX) {
		return;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AX ||
	    ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BE)
		gi_idx = ath12k_he_gi_to_nl80211_he_gi(ppdu_info->gi);

	rx_stats->pkt_stats.rx_rate[bw_idx][gi_idx][nss_idx][mcs_idx] += num_msdu;
	stats = &rx_stats->byte_stats;

	if (user_stats)
		len = user_stats->mpdu_ok_byte_count;
	else
		len = ppdu_info->mpdu_len;

	stats->rx_rate[bw_idx][gi_idx][nss_idx][mcs_idx] += len;
}

void ath12k_dp_mon_rx_update_peer_su_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct ath12k_dp_link_peer *peer,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_rx_peer_stats *rx_stats = peer->peer_stats.rx_stats;
	u32 num_msdu;

	peer->rssi_comb = ppdu_info->rssi_comb;
	ewma_avg_rssi_add(&peer->avg_rssi, ppdu_info->rssi_comb);

	if (!ath12k_extd_rx_stats_enabled(pdev_dp->ar) || !rx_stats)
		return;

	peer->peer_stats.rx_retries += ppdu_info->mpdu_retry;
	num_msdu = ppdu_info->tcp_msdu_count + ppdu_info->tcp_ack_msdu_count +
		   ppdu_info->udp_msdu_count + ppdu_info->other_msdu_count;

	rx_stats->num_msdu += num_msdu;
	rx_stats->tcp_msdu_count += ppdu_info->tcp_msdu_count +
				    ppdu_info->tcp_ack_msdu_count;
	rx_stats->udp_msdu_count += ppdu_info->udp_msdu_count;
	rx_stats->other_msdu_count += ppdu_info->other_msdu_count;

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11A ||
	    ppdu_info->preamble_type == HAL_RX_PREAMBLE_11B) {
		ppdu_info->nss = 1;
		ppdu_info->mcs = HAL_RX_MAX_MCS;
		ppdu_info->tid = IEEE80211_NUM_TIDS;
	}

	if (ppdu_info->ldpc < HAL_RX_SU_MU_CODING_MAX)
		rx_stats->coding_count[ppdu_info->ldpc] += num_msdu;

	if (ppdu_info->tid <= IEEE80211_NUM_TIDS)
		rx_stats->tid_count[ppdu_info->tid] += num_msdu;

	if (ppdu_info->preamble_type < HAL_RX_PREAMBLE_MAX)
		rx_stats->pream_cnt[ppdu_info->preamble_type] += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	if (ppdu_info->is_stbc)
		rx_stats->stbc_count += num_msdu;

	if (ppdu_info->beamformed)
		rx_stats->beamformed_count += num_msdu;

	if (ppdu_info->num_mpdu_fcs_ok > 1)
		rx_stats->ampdu_msdu_count += num_msdu;
	else
		rx_stats->non_ampdu_msdu_count += num_msdu;

	rx_stats->num_mpdu_fcs_ok += ppdu_info->num_mpdu_fcs_ok;
	rx_stats->num_mpdu_fcs_err += ppdu_info->num_mpdu_fcs_err;
	rx_stats->dcm_count += ppdu_info->dcm;

	rx_stats->rx_duration += ppdu_info->rx_duration;
	peer->rx_duration = rx_stats->rx_duration;

	if (ppdu_info->nss > 0 && ppdu_info->nss <= HAL_RX_MAX_NSS) {
		rx_stats->pkt_stats.nss_count[ppdu_info->nss - 1] += num_msdu;
		rx_stats->byte_stats.nss_count[ppdu_info->nss - 1] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11N &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_HT) {
		rx_stats->pkt_stats.ht_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.ht_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
		/* To fit into rate table for HT packets */
		ppdu_info->mcs = ppdu_info->mcs % 8;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AC &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_VHT) {
		rx_stats->pkt_stats.vht_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.vht_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AX &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_HE) {
		rx_stats->pkt_stats.he_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.he_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BE &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_BE) {
		rx_stats->pkt_stats.be_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.be_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
	}

	if ((ppdu_info->preamble_type == HAL_RX_PREAMBLE_11A ||
	     ppdu_info->preamble_type == HAL_RX_PREAMBLE_11B) &&
	     ppdu_info->rate < HAL_RX_LEGACY_RATE_INVALID) {
		rx_stats->pkt_stats.legacy_count[ppdu_info->rate] += num_msdu;
		rx_stats->byte_stats.legacy_count[ppdu_info->rate] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->gi < HAL_RX_GI_MAX) {
		rx_stats->pkt_stats.gi_count[ppdu_info->gi] += num_msdu;
		rx_stats->byte_stats.gi_count[ppdu_info->gi] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->bw < HAL_RX_BW_MAX) {
		rx_stats->pkt_stats.bw_count[ppdu_info->bw] += num_msdu;
		rx_stats->byte_stats.bw_count[ppdu_info->bw] += ppdu_info->mpdu_len;
	}

	ath12k_dp_mon_rx_update_peer_rate_table_stats(rx_stats, ppdu_info,
						      NULL, num_msdu);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_peer_su_stats);

void ath12k_dp_mon_rx_process_ulofdma_stats(struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *rx_user_status;
	u32 num_users, i, mu_ul_user_v0_word0, mu_ul_user_v0_word1, ru_size;
	u32 ru_width;

	if (!(ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_MIMO ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO))
		return;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (i = 0; i < num_users; i++) {
		rx_user_status = &ppdu_info->userstats[i];
		mu_ul_user_v0_word0 =
			rx_user_status->ul_ofdma_user_v0_word0;
		mu_ul_user_v0_word1 =
			rx_user_status->ul_ofdma_user_v0_word1;

		if (u32_get_bits(mu_ul_user_v0_word0,
				 HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VALID) &&
		    !u32_get_bits(mu_ul_user_v0_word0,
				  HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VER)) {
			rx_user_status->mcs =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_MCS);
			rx_user_status->nss =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_NSS) + 1;

			ppdu_info->usr_nss_sum += rx_user_status->nss;
			rx_user_status->ofdma_info_valid = 1;
			rx_user_status->ul_ofdma_ru_start_index =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_START);

			ru_size = u32_get_bits(mu_ul_user_v0_word1,
					       HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_SIZE);
			rx_user_status->ul_ofdma_ru_width = ru_size;
			ru_width = ath12k_dp_mon_rx_ul_ofdma_ru_size_to_width(ru_size);
			rx_user_status->ul_ofdma_ru_width = ru_width;
			rx_user_status->ul_ofdma_ru_size = ru_size;
		}
		rx_user_status->ldpc = u32_get_bits(mu_ul_user_v0_word1,
						    HAL_RX_UL_OFDMA_USER_INFO_V0_W1_LDPC);
	}
	ppdu_info->ldpc = 1;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_process_ulofdma_stats);

static void
ath12k_dp_mon_rx_update_user_stats(struct ath12k_pdev_dp *pdev_dp,
				   struct hal_rx_mon_ppdu_info *ppdu_info,
				   u32 uid)
{
	struct ath12k_rx_peer_stats *rx_stats = NULL;
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_pdev_dp_stats *pdev_stats = &pdev_dp->stats;
	struct ath12k_dp_link_peer *peer;
	u32 num_msdu;
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_base *ab = dp->ab;

	if (ppdu_info->peer_id == HAL_MON_INVALID_PEERID)
		return;

	peer = ath12k_dp_link_peer_find_by_ast(dp, user_stats->ast_index);
	if (!peer) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_RX, "peer with peer id %d can't be found\n",
			   ppdu_info->peer_id);
		return;
	}

	peer->peer_stats.rx_retries = user_stats->mpdu_retry;
	peer->rssi_comb = ppdu_info->rssi_comb;
	ewma_avg_rssi_add(&peer->avg_rssi, ppdu_info->rssi_comb);

	if (!ath12k_extd_rx_stats_enabled(pdev_dp->ar))
		return;

	rx_stats = peer->peer_stats.rx_stats;
	if (!rx_stats)
		return;

	ppdu_info->usr_nss_sum += user_stats->nss;
	ppdu_info->usr_ru_tones_sum += user_stats->ul_ofdma_ru_width;

	num_msdu = user_stats->tcp_msdu_count + user_stats->tcp_ack_msdu_count +
		   user_stats->udp_msdu_count + user_stats->other_msdu_count;

	rx_stats->num_msdu += num_msdu;
	rx_stats->tcp_msdu_count += user_stats->tcp_msdu_count +
				    user_stats->tcp_ack_msdu_count;
	rx_stats->udp_msdu_count += user_stats->udp_msdu_count;
	rx_stats->other_msdu_count += user_stats->other_msdu_count;

	if (ppdu_info->ldpc < HAL_RX_SU_MU_CODING_MAX)
		rx_stats->coding_count[ppdu_info->ldpc] += num_msdu;

	if (user_stats->tid <= IEEE80211_NUM_TIDS)
		rx_stats->tid_count[user_stats->tid] += num_msdu;

	if (user_stats->preamble_type < HAL_RX_PREAMBLE_MAX)
		rx_stats->pream_cnt[user_stats->preamble_type] += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	if (ppdu_info->is_stbc)
		rx_stats->stbc_count += num_msdu;

	if (ppdu_info->beamformed)
		rx_stats->beamformed_count += num_msdu;

	if (user_stats->mpdu_cnt_fcs_ok > 1)
		rx_stats->ampdu_msdu_count += num_msdu;
	else
		rx_stats->non_ampdu_msdu_count += num_msdu;

	rx_stats->num_mpdu_fcs_ok += user_stats->mpdu_cnt_fcs_ok;
	rx_stats->num_mpdu_fcs_err += user_stats->mpdu_cnt_fcs_err;
	rx_stats->dcm_count += ppdu_info->dcm;
	if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	    ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO)
		rx_stats->ru_alloc_cnt[user_stats->ul_ofdma_ru_size] += num_msdu;

	rx_stats->rx_duration += ppdu_info->rx_duration;
	peer->rx_duration = rx_stats->rx_duration;

	if (user_stats->nss > 0 && user_stats->nss <= HAL_RX_MAX_NSS) {
		rx_stats->pkt_stats.nss_count[user_stats->nss - 1] += num_msdu;
		rx_stats->byte_stats.nss_count[user_stats->nss - 1] +=
						user_stats->mpdu_ok_byte_count;
	}

	if (user_stats->preamble_type == HAL_RX_PREAMBLE_11AX &&
	    user_stats->mcs <= HAL_RX_MAX_MCS_HE) {
		rx_stats->pkt_stats.he_mcs_count[user_stats->mcs] += num_msdu;
		rx_stats->byte_stats.he_mcs_count[user_stats->mcs] +=
						user_stats->mpdu_ok_byte_count;
	}

	if (ppdu_info->gi < HAL_RX_GI_MAX) {
		rx_stats->pkt_stats.gi_count[ppdu_info->gi] += num_msdu;
		rx_stats->byte_stats.gi_count[ppdu_info->gi] +=
						user_stats->mpdu_ok_byte_count;
	}

	if (ppdu_info->bw < HAL_RX_BW_MAX) {
		rx_stats->pkt_stats.bw_count[ppdu_info->bw] += num_msdu;
		rx_stats->byte_stats.bw_count[ppdu_info->bw] +=
						user_stats->mpdu_ok_byte_count;
	}

	ath12k_dp_mon_rx_update_peer_rate_table_stats(rx_stats, ppdu_info,
						      user_stats, num_msdu);

	pdev_stats->telemetry_stats.rx_data_msdu_cnt = rx_stats->num_msdu;
	pdev_stats->telemetry_stats.total_rx_data_bytes = user_stats->mpdu_ok_byte_count;
}

void
ath12k_dp_mon_rx_update_peer_mu_stats(struct ath12k_pdev_dp *pdev_dp,
				      struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 num_users, i;

	if (!ath12k_extd_rx_stats_enabled(pdev_dp->ar))
		return;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (i = 0; i < num_users; i++)
		ath12k_dp_mon_rx_update_user_stats(pdev_dp, ppdu_info, i);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_peer_mu_stats);

static void
ath12k_dp_mon_ppdu_per_user_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
                                          struct hal_rx_mon_ppdu_info *ppdu_info,
                                          u32 uid)
{
       struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
       struct ath12k_dp_link_peer_stats *stats = NULL;
       struct ath12k_dp_link_peer *peer;
       u32 nss_ru_width_sum = 0;
       u64 temp_result = 0;
       u16 rx_time_us = 0;
       u8 ac = 0;

       if (!dp_pdev)
               return;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "PPDU per user rx time update called without rcu lock\n");
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

       peer = ath12k_dp_link_peer_find_by_id(dp_pdev->dp, user_stats->sw_peer_id);
       if (!peer || !peer->sta) {
               ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_PEER,
                          "peer stats not found on ppdu peer id %d\n",
                          user_stats->sw_peer_id);
               return;
       }

       nss_ru_width_sum = ppdu_info->usr_nss_sum * ppdu_info->usr_ru_tones_sum;
       if (!nss_ru_width_sum)
               nss_ru_width_sum = 1;

       if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU) {
               temp_result = ppdu_info->rx_duration * user_stats->nss *
                             user_stats->ul_ofdma_ru_width;
               rx_time_us = (u16)div_u64(temp_result, nss_ru_width_sum);
       } else
               rx_time_us = ppdu_info->rx_duration;

       ac = ath12k_tid_to_ac(ppdu_info->tid);
       stats = &peer->peer_stats;
       stats->dp_mon_stats.mon_stats.rx_airtime_consumption[ac].consumption += rx_time_us;
       ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_DP_HTT, "peer: %pM tid: %d ac: %d sum nss: %d tones: %d per user nss: %d tone: %d time: %d cons: %d\n",
                  peer->addr,
                  ppdu_info->tid, ac,
                  ppdu_info->usr_nss_sum, ppdu_info->usr_ru_tones_sum,
                  user_stats->nss, user_stats->ul_ofdma_ru_width,
                  rx_time_us,
                  stats->dp_mon_stats.mon_stats.rx_airtime_consumption[ac].consumption);
}

static void
ath12k_dp_mon_per_user_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					u32 uid)
{
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_dp_mon_peer_stats *stats = NULL;
	struct ath12k_dp_link_peer *peer;
	u8 rssi_comb;

	if (!dp_pdev)
		return;

	lockdep_assert_held(&dp_pdev->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_id(dp_pdev->dp, user_stats->sw_peer_id);
	if (!peer || !peer->sta) {
		ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_PEER,
			   "peer stats not found on ppdu peer id %d\n",
			   user_stats->sw_peer_id);
		return;
	}

	rssi_comb = ppdu_info->rssi_comb;
	stats = &peer->peer_stats.dp_mon_stats;
	stats->snr = rssi_comb;
	if (unlikely(stats->avg_snr == SNR_INVALID))
		stats->avg_snr = SNR_IN(stats->snr);
	else
		SNR_UPDATE_AVG(stats->avg_snr, stats->snr);
}

void ath12k_dp_mon_ppdu_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
				       struct hal_rx_mon_ppdu_info *ppdu_info,
				       bool is_stat)
{
       u32 num_users, uid;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "PPDU rx time update called without rcu lock\n");
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

       num_users = ppdu_info->num_users;
       if (num_users > HAL_MAX_UL_MU_USERS)
               num_users = HAL_MAX_UL_MU_USERS;

       for (uid = 0; uid < num_users; uid++)
               ath12k_dp_mon_ppdu_per_user_rx_time_update(dp_pdev, ppdu_info, uid);
}
EXPORT_SYMBOL(ath12k_dp_mon_ppdu_rx_time_update);

void ath12k_dp_mon_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 num_users, uid;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (uid = 0; uid < num_users; uid++)
		ath12k_dp_mon_per_user_ppdu_rssi_update(dp_pdev, ppdu_info, uid);
}
EXPORT_SYMBOL(ath12k_dp_mon_ppdu_rssi_update);

void ath12k_dp_rxdma_mon_buf_ring_free(struct ath12k_dp *dp,
				       struct dp_rxdma_mon_ring *rx_ring)
{
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb;
	int buf_id;

	spin_lock_bh(&rx_ring->idr_lock);
	idr_for_each_entry(&rx_ring->bufs_idr, skb, buf_id) {
		idr_remove(&rx_ring->bufs_idr, buf_id);
		/* TODO: Understand where internal driver does this dma_unmap
		 * of rxdma_buffer.
		 */
		dma_unmap_single(ab->dev, ATH12K_SKB_RXCB(skb)->paddr,
				 skb->len + skb_tailroom(skb), DMA_FROM_DEVICE);
		dev_kfree_skb_any(skb);
	}

	idr_destroy(&rx_ring->bufs_idr);
	spin_unlock_bh(&rx_ring->idr_lock);
}
EXPORT_SYMBOL(ath12k_dp_rxdma_mon_buf_ring_free);

int ath12k_dp_mon_rx_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int ret;

	ret = ath12k_dp_srng_setup(ab,
				   &dp_mon->rxdma_mon_buf_ring.refill_buf_ring,
				   HAL_RXDMA_MONITOR_BUF, 0, 0,
				   DP_RXDMA_MONITOR_BUF_RING_SIZE);
	if (ret) {
		ath12k_warn(dp, "failed to setup HAL_RXDMA_MONITOR_BUF %d\n",
			    ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_srng_setup);

void ath12k_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;

	srng = &dp_mon->rxdma_mon_buf_ring.refill_buf_ring;

	ath12k_dp_srng_cleanup(ab, srng);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_srng_cleanup);

int ath12k_dp_mon_rx_buf_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	LIST_HEAD(list);
	size_t req_entries;
	int num_entries, ret = -EINVAL, i;

	INIT_LIST_HEAD(&dp_mon->mon_desc_free_list);
	spin_lock_init(&dp_mon->mon_desc_lock);

	spin_lock_bh(&dp_mon->mon_desc_lock);
	dp_mon->mon_desc_pool = kcalloc(DP_RXDMA_MONITOR_BUF_RING_SIZE,
					sizeof(*dp_mon->mon_desc_pool),
					GFP_ATOMIC);
	if (!dp_mon->mon_desc_pool) {
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		ath12k_warn(dp, "failed to allocate memory for mon desc pool\n");
		ret = -ENOMEM;
		return ret;
	}

	for (i = 0; i < DP_RXDMA_MONITOR_BUF_RING_SIZE; i++) {
		dp_mon->mon_desc_pool[i].magic = ATH12K_MON_MAGIC_VALUE;
		INIT_LIST_HEAD(&dp_mon->mon_desc_pool[i].list);
		list_add_tail(&dp_mon->mon_desc_pool[i].list,
			      &dp_mon->mon_desc_free_list);
	}

	spin_unlock_bh(&dp_mon->mon_desc_lock);

	rx_ring = &dp_mon->rxdma_mon_buf_ring;

	num_entries =  rx_ring->refill_buf_ring.size /
		ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_BUF);
	rx_ring->bufs_max = num_entries;

	req_entries = ath12k_dp_mon_get_req_entries_from_buf_ring(dp, rx_ring, &list);
	if (req_entries)
		ret = ath12k_dp_mon_buf_replenish(dp, rx_ring, &list, req_entries);
	else
		ath12k_warn(dp, "No required entries available for mon buf ring\n");

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_buf_setup);

void ath12k_dp_mon_rx_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int i;
	u8 *mon_buf;

	spin_lock_bh(&dp_mon->mon_desc_lock);
	if (!dp_mon->mon_desc_pool) {
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		return;
	}

	for (i = 0; i < DP_RXDMA_MONITOR_BUF_RING_SIZE; i++) {
		if (dp_mon->mon_desc_pool[i].in_use != DP_MON_DESC_TO_HW)
			continue;

		mon_buf = dp_mon->mon_desc_pool[i].mon_buf;
		if (!mon_buf)
			goto reset_mon_desc;

		ath12k_core_dma_unmap_page(dp->dev, dp_mon->mon_desc_pool[i].paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE, DMA_FROM_DEVICE);
		page_frag_free(mon_buf);
		dp_mon->mon_desc_pool[i].mon_buf = NULL;
		dp_mon->num_frag_free++;

reset_mon_desc:
		ath12k_dp_mon_desc_reset(&dp_mon->mon_desc_pool[i]);
	}

	kfree(dp_mon->mon_desc_pool);
	dp_mon->mon_desc_pool = NULL;
	spin_unlock_bh(&dp_mon->mon_desc_lock);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_buf_free);

int ath12k_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u32 ring_id;
	int ret;

	ring_id = dp_mon->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
					  0, HAL_RXDMA_MONITOR_BUF);
	if (ret) {
		ath12k_warn(ab, "failed to configure rxdma_mon_buf_ring %d\n",
			    ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_htt_srng_setup);

int ath12k_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev,
				     u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int i;
	int ret;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		ret = ath12k_dp_srng_setup(dp->ab,
					   &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i],
					   HAL_RXDMA_MONITOR_DST,
					   0, mac_id + i,
					   DP_RXDMA_MONITOR_DST_RING_SIZE);
		if (ret) {
			ath12k_warn(dp->ab,
				    "failed to setup HAL_RXDMA_MONITOR_DST\n");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_srng_setup);

int ath12k_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev,
					 u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 ring_id;
	int i;
	int ret;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		ring_id = dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(dp->ab, ring_id,
						  mac_id + i,
						  HAL_RXDMA_MONITOR_DST);
		if (ret) {
			ath12k_warn(dp->ab,
				    "failed to configure rxdma_mon_dst_ring %d %d\n",
				    i, ret);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_htt_srng_setup);

void ath12k_dp_mon_pdev_rx_mpdu_list_init(struct ath12k_mon_data *pmon)
{
	INIT_LIST_HEAD(&pmon->dp_rx_mon_mpdu_list);
	pmon->mon_mpdu = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_mpdu_list_init);

void ath12k_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int i;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++)
		ath12k_dp_srng_cleanup(dp->ab,
				       &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i]);
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_srng_cleanup);

int ath12k_dp_mon_init(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon;

	dp_mon = kzalloc(sizeof(*dp_mon), GFP_KERNEL);
	if (!dp_mon)
		return -ENOMEM;

	dp_mon->dp = dp;
	dp->dp_mon = dp_mon;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_init);

void ath12k_dp_mon_deinit(struct ath12k_dp *dp)
{
	if (dp->dp_mon)
		kfree(dp->dp_mon);
	dp->dp_mon = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_deinit);

int ath12k_dp_mon_pdev_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;

	dp_mon_pdev = kzalloc(sizeof(*dp_mon_pdev), GFP_KERNEL);
	if (!dp_mon_pdev)
		return -ENOMEM;

	dp_mon_pdev->dp_pdev = dp_pdev;
	dp_mon_pdev->dp_mon = dp_pdev->dp->dp_mon;
	dp_pdev->dp_mon_pdev = dp_mon_pdev;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_alloc);

void ath12k_dp_mon_pdev_free(struct ath12k_pdev_dp *dp_pdev)
{
	kfree(dp_pdev->dp_mon_pdev);
	dp_pdev->dp_mon_pdev = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_free);

void ath12k_dp_mon_pdev_rx_attach(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = &dp_mon_pdev->mon_data;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	int i;

	skb_queue_head_init(&pmon->rx_status_q);

	pmon->mon_ppdu_status = DP_PPDU_STATUS_START;

	memset(&pmon->rx_mon_stats, 0,
	       sizeof(pmon->rx_mon_stats));

	pmon->mon_last_linkdesc_paddr = 0;
	pmon->mon_last_buf_cookie = DP_RX_DESC_COOKIE_MAX + 1;
	spin_lock_init(&pmon->mon_lock);

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);

	if (mon_ops && mon_ops->mon_pdev_rx_mpdu_list_init)
		mon_ops->mon_pdev_rx_mpdu_list_init(pmon);

	INIT_LIST_HEAD(&dp_mon_pdev->mon_desc_used_list);

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++)
		skb_queue_head_init(&ppdu_info->mpdu_q[i]);
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_attach);

void ath12k_dp_mon_rx_stats_enable(struct ath12k_pdev_dp *dp_pdev,
				   enum dp_mon_stats_mode mode)
{
	struct ath12k *ar = dp_pdev->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_extd_rx_stats_enabled(ar))
		mode = ATH12k_DP_MON_EXTD_STATS;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ar->ab->dev_flags))
		 mode = ATH12k_DP_MON_EXTD_STATS;
#endif
	ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_stats_enable);

void ath12k_dp_mon_rx_stats_disable(struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_stats_mode mode)
{
	struct ath12k *ar = dp_pdev->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (mode == ATH12k_DP_MON_EXTD_STATS) {
		/* while disabling extd rx stats, Basic stats filters
		 * to be configured.
		 */
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     false);
		mode = ATH12k_DP_MON_BASIC_STATS;
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     true);
	} else {
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     false);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_stats_disable);

void ath12k_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_monitor_mode_set);

void ath12k_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_monitor_mode_reset);

void ath12k_dp_mon_rx_nrp_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_nrp_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_nrp_set);

void ath12k_dp_mon_rx_nrp_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_nrp_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_nrp_reset);

static void ath12k_dp_mon_clear_pdev_airtime_stats(struct ath12k *ar)
{
       struct ath12k_pdev_dp *pdev_dp = &ar->dp;
       u8 ac;

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               pdev_dp->stats.telemetry_stats.tx_link_airtime[ac] = 0;
               pdev_dp->stats.telemetry_stats.rx_link_airtime[ac] = 0;
       }
}

u64 ath12k_get_timestamp_in_us(void)
{
       struct timespec64 ts;

       ktime_get_ts64(&ts);

       return ((u64)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
}

void ath12k_dp_mon_peer_update_telemetry_stats(struct ath12k_base *ab,
                                              struct ath12k_dp_link_peer *peer,
                                              struct ath12k_pdev *pdev)
{
       struct ath12k_mon_peer_airtime_stats *airtime_stats;
       struct peer_airtime_consumption *peer_consump;
       u64 current_time = ath12k_get_timestamp_in_us();
       u32 remainder, time_diff;
       u32 usage;
       u16 consump_per_sec;
       struct ath12k *ar = pdev->ar;
       struct ath12k_pdev_dp *dp = &ar->dp;
       struct ath12k_pdev_dp_stats *pdev_stats = &dp->stats;
       u8 ac;
	struct ath12k_atf_peer_airtime *atf_airtime;

       airtime_stats = &peer->peer_stats.dp_mon_stats.mon_stats;
       time_diff = (u32)(current_time - airtime_stats->last_update_time);
	atf_airtime = &peer->atf_peer_airtime;

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               /* *_link_airtime refers to the amount of time a peer spends
                * transmitting or receiving data over the medium
                * To calculate total pdev's airtime cosumption, then store
                * each peer airtime consumption in pdev telemetry link airtime
                */

               /* Tx Airtime Consumption */
               peer_consump = &airtime_stats->tx_airtime_consumption[ac];
		atf_airtime->tx_airtime_consumption[ac].consumption +=
			airtime_stats->tx_airtime_consumption[ac].consumption;
		atf_airtime->tx_airtime_consumption[ac].avg_consumption_per_sec +=
			airtime_stats->tx_airtime_consumption[ac].avg_consumption_per_sec;
               usage = peer_consump->consumption;
               consump_per_sec = (u8)div_u64((u64)(usage * 100), time_diff);
               div_u64_rem((u64)(usage * 100), time_diff, &remainder);
               if (remainder < time_diff / 2) {
                       if (remainder && consump_per_sec == 0)
                               consump_per_sec++;
               } else {
                       if (consump_per_sec < 100)
                               consump_per_sec++;
               }
               peer_consump->avg_consumption_per_sec = consump_per_sec;
               pdev_stats->telemetry_stats.tx_link_airtime[ac] += peer_consump->consumption;
	       pdev_stats->atf_airtime.tx_airtime_consumption[ac] += peer_consump->consumption;
               peer_consump->consumption = 0;

               /* Rx Airtime Consumption */
               peer_consump = &airtime_stats->rx_airtime_consumption[ac];
		atf_airtime->rx_airtime_consumption[ac].consumption +=
			airtime_stats->rx_airtime_consumption[ac].consumption;
		atf_airtime->rx_airtime_consumption[ac].avg_consumption_per_sec +=
			airtime_stats->rx_airtime_consumption[ac].avg_consumption_per_sec;
               usage = peer_consump->consumption;
               consump_per_sec = (u8)div_u64((u64)(usage * 100), time_diff);
               div_u64_rem((u64)(usage * 100), time_diff, &remainder);
               if (remainder < time_diff / 2) {
                       if (remainder && consump_per_sec == 0)
                               consump_per_sec++;
               } else {
                       if (consump_per_sec < 100)
                               consump_per_sec++;
               }
               peer_consump->avg_consumption_per_sec = consump_per_sec;
               pdev_stats->telemetry_stats.rx_link_airtime[ac] += peer_consump->consumption;
	       pdev_stats->atf_airtime.rx_airtime_consumption[ac] += peer_consump->consumption;
               peer_consump->consumption = 0;

               ath12k_dbg(ab,
                          ATH12K_DBG_DP_HTT,
                          "peer: %pM time diff: %d link air tx: %d rx: %d cons tx: %d rx: %d\n",
                          peer->addr, time_diff,
                          pdev_stats->telemetry_stats.tx_link_airtime[ac],
                          pdev_stats->telemetry_stats.rx_link_airtime[ac],
                          airtime_stats->tx_airtime_consumption[ac].avg_consumption_per_sec,
                          airtime_stats->rx_airtime_consumption[ac].avg_consumption_per_sec);
       }

       airtime_stats->last_update_time = current_time;
}

static inline void ath12k_pdev_dp_iterate_peer(struct ath12k_base *ab,
                                              struct ath12k_pdev *pdev,
                                              void (*iter)(struct ath12k_base *ab, struct ath12k_dp_link_peer *peer, struct ath12k_pdev *pdev))
{
       struct ath12k_dp_link_peer *peer, *tmp;
       struct ieee80211_sta *sta;
       struct ath12k *ar;

       if (!pdev || !pdev->ar)
               return;
       ar = pdev->ar;
       spin_lock_bh(&ab->dp->dp_lock);
       list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
               if (!peer->vif)
                       continue;

               sta = peer->sta;
               if (!sta)
                       continue;
               /* In a split PHY scenario, if a pdev-level event occurs,
                * halt the operation if the peer belongs to a different pdev
                * than the one that triggered the event.
                */
               if (peer->pdev_idx != ar->pdev_idx)
                       continue;

               iter(ab, peer, pdev);
       }
       spin_unlock_bh(&ab->dp->dp_lock);
}

int ath12k_dp_mon_pdev_update_telemetry_stats(struct ath12k_base *ab,
                                             const int pdev_id)
{
       struct ath12k_pdev *pdev;

       rcu_read_lock();
       pdev = rcu_dereference(ab->pdevs_active[pdev_id]);
       if (!pdev) {
               rcu_read_unlock();
               return -EINVAL;
       }

       if (pdev->ar)
               ath12k_dp_mon_clear_pdev_airtime_stats(pdev->ar);

       ath12k_pdev_dp_iterate_peer(ab, pdev,
                                   ath12k_dp_mon_peer_update_telemetry_stats);
       rcu_read_unlock();

       return 0;
}

static void ath12k_dp_mon_peer_telemetry_stats(const struct ath12k_dp_link_peer *peer,
                                              struct ath12k_peer_telemetry_stats *stats)
{
       const struct ath12k_dp_mon_peer_stats *dp_stats;
       u8 ac;

       dp_stats = &peer->peer_stats.dp_mon_stats;
       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               stats->tx_airtime_consumption[ac] =
                       dp_stats->mon_stats.tx_airtime_consumption[ac].avg_consumption_per_sec;
               stats->rx_airtime_consumption[ac] =
                       dp_stats->mon_stats.rx_airtime_consumption[ac].avg_consumption_per_sec;
       }
	stats->snr = dp_stats->avg_snr;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_update_telemetry_stats);

int ath12k_dp_get_peer_telemetry_stats(struct ath12k_base *ab,
                                      const u8 *peer_addr,
                                      struct ath12k_peer_telemetry_stats *stats)
{
       struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	lockdep_assert_held(&dp->dp_lock);
       peer = ath12k_dp_link_peer_find_by_addr(dp, peer_addr);
       if (!peer) {
               ath12k_dbg(ab,
                          ATH12K_DBG_DP_HTT,
                          "Failed to find peer at addr: %pM\n", peer_addr);
               return -EINVAL;
       }

       ath12k_dp_mon_peer_telemetry_stats(peer, stats);

       return 0;
}

static inline struct sk_buff *ath12k_mon_get_last_skb_from_fraglist(struct sk_buff *skb)
{
	struct sk_buff *last_skb;

	for (last_skb = skb_shinfo(skb)->frag_list;
	     last_skb && last_skb->next;
	     last_skb = last_skb->next)
		;

	return last_skb;
}

struct sk_buff *
ath12k_dp_mon_get_skb_valid_frag(struct ath12k_dp *dp, struct sk_buff *skb)
{
	struct sk_buff *last_skb;
	u32 num_frags;

	if (unlikely(!skb)) {
		ath12k_warn(dp, "invalid skb, cannot retrieve valid skb\n");
		return NULL;
	}

	num_frags = skb_shinfo(skb)->nr_frags;
	if (likely(num_frags < MAX_SKB_FRAGS))
		return skb;

	if (unlikely(!skb_has_frag_list(skb)))
		return NULL;

	last_skb = ath12k_mon_get_last_skb_from_fraglist(skb);
	if (unlikely(!last_skb)) {
		ath12k_warn(dp, "frag_list present but no valid last skb found\n");
		return NULL;
	}

	num_frags = skb_shinfo(last_skb)->nr_frags;
	if (likely(num_frags < MAX_SKB_FRAGS))
		return last_skb;

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON_RX,
		   "no skb with available frag slots found in skb or frag_list\n");
	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_skb_valid_frag);

void ath12k_dp_mon_update_skb_len(struct sk_buff *skb_head, u32 frag_len)
{
	skb_head->data_len += frag_len;
	skb_head->len += frag_len;
}
EXPORT_SYMBOL(ath12k_dp_mon_update_skb_len);

void ath12k_dp_mon_append_skb(struct sk_buff *skb, struct sk_buff *tmp_skb)
{
	struct sk_buff *last_skb;

	if (unlikely(!skb_has_frag_list(skb))) {
		skb_shinfo(skb)->frag_list = tmp_skb;
	} else {
		last_skb = ath12k_mon_get_last_skb_from_fraglist(skb);
		if (last_skb)
			last_skb->next = tmp_skb;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_append_skb);

void ath12k_dp_mon_skb_remove_frag(struct ath12k_dp *dp, struct sk_buff *skb,
				   u16 idx, u16 truesize)
{
	struct page *page;
	u16 frag_len;

	page = skb_frag_page(&skb_shinfo(skb)->frags[idx]);
	if (unlikely(!page))
		return;

	frag_len = ath12k_wifi7_dp_mon_get_frag_size_by_idx(dp, skb, idx);
	put_page(page);
	skb->len -= frag_len;
	skb->data_len -= frag_len;
	skb->truesize -= truesize;
	skb_shinfo(skb)->nr_frags--;
}
EXPORT_SYMBOL(ath12k_dp_mon_skb_remove_frag);

void ath12k_dp_mon_add_rx_frag(struct sk_buff *skb, const void *mon_buf,
			       int offset, int frag_len, bool take_frag_ref)
{
	struct page *page = virt_to_head_page(mon_buf);
	int frag_offset = mon_buf - page_address(page);
	int nr_frags = skb_shinfo(skb)->nr_frags;

	skb_add_rx_frag(skb, nr_frags, page,
			(frag_offset + offset), frag_len,
			ATH12K_DP_MON_RX_BUF_SIZE);

	if (unlikely(take_frag_ref))
		skb_frag_ref(skb, nr_frags);
}
EXPORT_SYMBOL(ath12k_dp_mon_add_rx_frag);

void ath12k_dp_mon_rx_process_low_thres(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	struct hal_srng *srng;
	int num_free, req_entries;
	LIST_HEAD(list);

	rx_ring = &dp_mon->rxdma_mon_buf_ring;
	srng = &dp->hal->srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	/* if ring is less than half filled need to replenish */
	if (num_free < (rx_ring->bufs_max / 2)) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	spin_lock_bh(&dp_mon->mon_desc_lock);
	req_entries = ath12k_dp_mon_list_cut_nodes(&list,
						   &dp->dp_mon->mon_desc_free_list,
						   num_free);
	spin_unlock_bh(&dp_mon->mon_desc_lock);

	if (req_entries)
		ath12k_dp_mon_buf_replenish(dp, rx_ring, &list, req_entries);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_process_low_thres);

void
ath12k_dp_mon_cnt_skb_and_frags(struct sk_buff *skb, u32 *skb_count, u32 *frag_count)
{
	struct sk_buff *iter;
	u32 total_skb = 0, total_frags = 0;

	if (unlikely(!skb))
		return;

	total_skb++;
	total_frags += skb_shinfo(skb)->nr_frags;

	if (skb_has_frag_list(skb)) {
		for (iter = skb_shinfo(skb)->frag_list; iter; iter = iter->next) {
			total_skb++;
			total_frags += skb_shinfo(iter)->nr_frags;
		}
	}

	*skb_count += total_skb;
	*frag_count += total_frags;
}
EXPORT_SYMBOL(ath12k_dp_mon_cnt_skb_and_frags);

u32 ath12k_wifi7_dp_mon_get_frag_size_by_idx(struct ath12k_dp *dp,
					     struct sk_buff *skb, u8 idx)
{
	u32 size = 0;

	if (likely(idx < MAX_SKB_FRAGS))
		size = skb_frag_size(&skb_shinfo(skb)->frags[idx]);

	return size;
}
EXPORT_SYMBOL(ath12k_wifi7_dp_mon_get_frag_size_by_idx);

void *ath12k_dp_mon_skb_get_frag_addr(struct sk_buff *skb, u8 idx)
{
	void *frag = NULL;

	if (likely(idx < MAX_SKB_FRAGS))
		frag = skb_frag_address(&skb_shinfo(skb)->frags[idx]);

	return frag;
}
EXPORT_SYMBOL(ath12k_dp_mon_skb_get_frag_addr);

int ath12k_dp_mon_adj_frag_offset(struct sk_buff *skb, u8 idx, int offset)
{
	u32 frag_offset;
	skb_frag_t *frag;

	if (unlikely(idx >= skb_shinfo(skb)->nr_frags))
		return -EINVAL;

	frag = &skb_shinfo(skb)->frags[idx];
	frag_offset = skb_frag_off(frag);
	frag_offset += offset;
	skb_frag_off_set(frag, frag_offset);
	skb_coalesce_rx_frag(skb, idx, -(offset), 0);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_adj_frag_offset);

u32 ath12k_dp_mon_get_num_frags_in_fraglist(struct sk_buff *skb)
{
	struct sk_buff *list = NULL;
	u32 num_frags = skb_shinfo(skb)->nr_frags;

	skb_walk_frags(skb, list)
		num_frags += skb_shinfo(list)->nr_frags;

	return num_frags;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_num_frags_in_fraglist);

void
ath12k_dp_mon_fill_rx_rate(struct ath12k_pdev_dp *dp_pdev,
			   struct hal_rx_mon_ppdu_info *ppdu_info,
			   struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_supported_band *sband;
	struct ath12k *ar = dp_pdev->ar;
	enum rx_msdu_start_pkt_type pkt_type;
	u8 rate_mcs, nss, sgi, bw;
	bool is_cck;

	pkt_type = ppdu_info->preamble_type;
	rate_mcs = ppdu_info->mcs;
	nss = ppdu_info->nss;
	sgi = ppdu_info->gi;
	bw = ppdu_info->bw;

	switch (pkt_type) {
	case RX_MSDU_START_PKT_TYPE_11A:
	case RX_MSDU_START_PKT_TYPE_11B:
		is_cck = (pkt_type == RX_MSDU_START_PKT_TYPE_11B);
		if (rx_status->band < NUM_NL80211_BANDS) {
			sband = &ar->mac.sbands[rx_status->band];
			rx_status->rate_idx = ath12k_mac_hw_rate_to_idx(sband, rate_mcs,
									is_cck);
		}
		break;
	case RX_MSDU_START_PKT_TYPE_11N:
		rx_status->encoding = RX_ENC_HT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		if (rate_mcs > ATH12K_HT_MCS_MAX) {
			ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON_RX,
				   "Received with invalid mcs in HT mode %d\n",
				   rate_mcs);
			break;
		}
		rx_status->rate_idx = rate_mcs + (8 * (nss - 1));
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		break;
	case RX_MSDU_START_PKT_TYPE_11AC:
		rx_status->encoding = RX_ENC_VHT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON_RX,
				   "Received with invalid mcs in VHT mode %d\n",
				   rate_mcs);
			break;
		}
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		break;
	case RX_MSDU_START_PKT_TYPE_11AX:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_HE_MCS_MAX) {
			ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON_RX,
				   "Received with invalid mcs in HE mode %d\n",
				   rate_mcs);
			break;
		}
		rx_status->encoding = RX_ENC_HE;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		break;
	case RX_MSDU_START_PKT_TYPE_11BE:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON_RX,
				   "Received with invalid mcs in EHT mode %d\n",
				   rate_mcs);
			break;
		}
		rx_status->encoding = RX_ENC_EHT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON_RX,
			   "monitor receives invalid preamble type %d",
			    pkt_type);
		break;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_fill_rx_rate);
