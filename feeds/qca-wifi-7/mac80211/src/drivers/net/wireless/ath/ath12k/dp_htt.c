// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "peer.h"
#include "htc.h"
#include "dp_htt.h"
#include "debugfs_htt_stats.h"
#include "debugfs_sta.h"
#include "debugfs.h"
#include "dp_mon.h"
#include "dp_mon_filter.h"

static void ath12k_dp_htt_htc_tx_complete(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
}

int ath12k_dp_htt_connect(struct ath12k_dp *dp)
{
	struct ath12k_htc_svc_conn_req conn_req = {0};
	struct ath12k_htc_svc_conn_resp conn_resp = {0};
	int status;
	struct ath12k_base *ab = dp->ab;

	conn_req.ep_ops.ep_tx_complete = ath12k_dp_htt_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = ath12k_dp_htt_htc_t2h_msg_handler;

	/* connect to control service */
	conn_req.service_id = ATH12K_HTC_SVC_ID_HTT_DATA_MSG;

	ab->htt_flag = true;
	status = ath12k_htc_connect_service(&dp->ab->htc, &conn_req,
					    &conn_resp);

	if (status)
		return status;

	dp->eid = conn_resp.eid;

	return 0;
}

static int ath12k_get_ppdu_user_index(struct htt_ppdu_stats *ppdu_stats,
				      u16 peer_id)
{
	int i;

	for (i = 0; i < HTT_PPDU_STATS_MAX_USERS - 1; i++) {
		if (ppdu_stats->user_stats[i].is_valid_peer_id) {
			if (peer_id == ppdu_stats->user_stats[i].peer_id)
				return i;
		} else {
			return i;
		}
	}

	return -EINVAL;
}

static void ath12k_dp_ppdu_stats_flush_tlv_parse(struct ath12k_base *ab,
		struct htt_ppdu_stats_cmpltn_flush *msg,
		struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ath12k_dp_link_peer *peer = NULL;
	struct rate_info rate;
	struct ieee80211_tx_status status;
	struct ieee80211_rate_status status_rate = { 0 };
	u8 pdev_id;

	pdev_id = ppdu_info->pdev_id;

	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id - 1);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, msg->sw_peer_id);
	if (unlikely(!peer || !peer->sta)) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
				"dp_tx: failed to find the peer with peer_id %d\n",
				msg->sw_peer_id);
		rcu_read_unlock();
		return;
	}

	if (peer->vif->type != NL80211_IFTYPE_MESH_POINT) {
		rcu_read_unlock();
		return;
	}

	if (ether_addr_equal(peer->addr, peer->vif->addr)) {
		rcu_read_unlock();
		return;
	}

	memset(&status, 0, sizeof(status));
	status.sta = peer->sta;
	rate = peer->last_txrate;

	status_rate.rate_idx = rate;
	status_rate.try_count = 1;

	status.rates = &status_rate;
	status.n_rates = 1;
	status.mpdu_fail = FIELD_GET(HTT_PPDU_STATS_CMPLTN_FLUSH_INFO_NUM_MPDU,
			msg->info);
	ieee80211s_update_metric_ppdu(ath12k_dp_pdev_to_hw(dp_pdev), &status);

	rcu_read_unlock();
}

static int ath12k_htt_tlv_ppdu_stats_parse(struct ath12k_base *ab,
					   u16 tag, u16 len, const void *ptr,
					   void *data)
{
	const struct htt_ppdu_stats_usr_cmpltn_ack_ba_status *ba_status;
	const struct htt_ppdu_stats_usr_cmpltn_cmn *cmplt_cmn;
	const struct htt_ppdu_stats_user_rate *user_rate;
	struct htt_ppdu_stats_info *ppdu_info;
	struct htt_ppdu_user_stats *user_stats;
	int cur_user;
	u16 peer_id;
	u32 ppdu_id;
	u32 frame_type;

	ppdu_info = data;

	switch (tag) {
	case HTT_PPDU_STATS_TAG_COMMON:
		if (len < sizeof(struct htt_ppdu_stats_common)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}
		memcpy(&ppdu_info->ppdu_stats.common, ptr,
		       sizeof(struct htt_ppdu_stats_common));

		frame_type =
			FIELD_GET(HTT_PPDU_STATS_CMN_FLAGS_FRAME_TYPE_M,
				  ppdu_info->ppdu_stats.common.flags);
		switch (frame_type) {
			case HTT_STATS_FTYPE_TIDQ_DATA_SU:
			case HTT_STATS_FTYPE_TIDQ_DATA_MU:
				if (u32_get_bits(ppdu_info->frame_ctrl, HTT_STATS_FRAMECTRL_TYPE_MASK) <= HTT_STATS_FRAME_CTRL_TYPE_CTRL)
					ppdu_info->frame_type = HTT_STATS_PPDU_FTYPE_CTRL;
				else
					ppdu_info->frame_type = HTT_STATS_PPDU_FTYPE_DATA;
				break;
			default:
				ppdu_info->frame_type = HTT_STATS_PPDU_FTYPE_CTRL;
			break;
		}
		ppdu_info->htt_frame_type = frame_type;
		break;
	case HTT_PPDU_STATS_TAG_USR_RATE:
		if (len < sizeof(struct htt_ppdu_stats_user_rate)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}
		user_rate = ptr;
		peer_id = le16_to_cpu(user_rate->sw_peer_id);
		cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats,
						      peer_id);
		if (cur_user < 0)
			return -EINVAL;
		user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
		user_stats->peer_id = peer_id;
		user_stats->is_valid_peer_id = true;
		memcpy(&user_stats->rate, ptr,
		       sizeof(struct htt_ppdu_stats_user_rate));
		ppdu_info->tlv_bitmap |= BIT(tag);
		break;
	case HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON:
		if (len < sizeof(struct htt_ppdu_stats_usr_cmpltn_cmn)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}

		cmplt_cmn = ptr;
		peer_id = le16_to_cpu(cmplt_cmn->sw_peer_id);
		cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats,
						      peer_id);
		if (cur_user < 0)
			return -EINVAL;
		user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
		user_stats->peer_id = peer_id;
		user_stats->is_valid_peer_id = true;
		memcpy(&user_stats->cmpltn_cmn, ptr,
		       sizeof(struct htt_ppdu_stats_usr_cmpltn_cmn));
		ppdu_info->tlv_bitmap |= BIT(tag);
		break;
	case HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS:
		if (len <
		    sizeof(struct htt_ppdu_stats_usr_cmpltn_ack_ba_status)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}

		ba_status = ptr;
		ppdu_id =
		((struct htt_ppdu_stats_usr_cmpltn_ack_ba_status *)ptr)->ppdu_id;
		peer_id = le16_to_cpu(ba_status->sw_peer_id);
		cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats,
						      peer_id);
		if (cur_user < 0)
			return -EINVAL;
		user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
		user_stats->peer_id = peer_id;
		user_stats->is_valid_peer_id = true;
		ppdu_info->ppdu_id = FIELD_GET(HTT_PPDU_STATS_PPDU_ID, ppdu_id);
		memcpy(&user_stats->ack_ba, ptr,
		       sizeof(struct htt_ppdu_stats_usr_cmpltn_ack_ba_status));
		ppdu_info->tlv_bitmap |= BIT(tag);
		break;
	case HTT_PPDU_STATS_TAG_SCH_CMD_STATUS:
		ppdu_info->tlv_bitmap |= BIT(tag);
		break;
	case HTT_PPDU_STATS_TAG_USR_COMMON:
		if (len < sizeof(struct htt_ppdu_stats_user_common)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}
		peer_id = ((struct htt_ppdu_stats_user_common *)ptr)->sw_peer_id;
		cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats,
						      peer_id);
		if (cur_user < 0)
			return -EINVAL;
		user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
		memcpy(&user_stats->common, ptr,
		       sizeof(struct htt_ppdu_stats_user_common));
		ppdu_info->frame_ctrl = FIELD_GET(HTT_PPDU_STATS_USR_CMN_CTL_FRM_CTRL,
						  user_stats->common.ctrl);
		user_stats->delay_ba = FIELD_GET(HTT_PPDU_STATS_USR_CMN_FLAG_DELAYBA,
						  user_stats->common.info);
		ppdu_info->delay_ba = user_stats->delay_ba;
		break;
	case HTT_PPDU_STATS_TAG_USR_COMPLTN_FLUSH:
		if (len < sizeof(struct htt_ppdu_stats_cmpltn_flush)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
					len, tag);
			return -EINVAL;
		}
		/* No need to use these stats when SW is already
		 * doing it on a per packet basis
		 */
		if (!ab->stats_disable)
			break;
		ath12k_dp_ppdu_stats_flush_tlv_parse(ab,
			(struct htt_ppdu_stats_cmpltn_flush *)ptr, ppdu_info);
		break;
	}
	return 0;
}

int ath12k_dp_htt_tlv_iter(struct ath12k_base *ab, const void *ptr, size_t len,
			   int (*iter)(struct ath12k_base *ar, u16 tag, u16 len,
				       const void *ptr, void *data),
			   void *data)
{
	struct htt_ppdu_stats_info *ppdu_info = NULL;
	const struct htt_tlv *tlv;
	const void *begin = ptr;
	u16 tlv_tag, tlv_len;
	int ret = -EINVAL;

	if (!data)
		return ret;

	ppdu_info = (struct htt_ppdu_stats_info *)data;

	while (len > 0) {
		if (len < sizeof(*tlv)) {
			ath12k_err(ab, "htt tlv parse failure at byte %zd (%zu bytes left, %zu expected)\n",
				   ptr - begin, len, sizeof(*tlv));
			return -EINVAL;
		}
		tlv = (struct htt_tlv *)ptr;
		tlv_tag = le32_get_bits(tlv->header, HTT_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, HTT_TLV_LEN);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);

		if (tlv_len > len) {
			ath12k_err(ab, "htt tlv parse failure of tag %u at byte %zd (%zu bytes left, %u expected)\n",
				   tlv_tag, ptr - begin, len, tlv_len);
			return -EINVAL;
		}
		ret = iter(ab, tlv_tag, tlv_len, ptr, ppdu_info);
		if (ret == -ENOMEM)
			return ret;

		ptr += tlv_len;
		len -= tlv_len;
	}
	return 0;
}

static u32 ath12k_dp_rx_ru_alloc_from_ru_size(u16 ru_size)
{
       u32 width = 0;

       switch (ru_size) {
       case HTT_PPDU_STATS_RU_26:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_26;
               break;
       case HTT_PPDU_STATS_RU_52:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_52;
               break;
       case HTT_PPDU_STATS_RU_106:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_106;
               break;
       case HTT_PPDU_STATS_RU_242:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_242;
               break;
       case HTT_PPDU_STATS_RU_484:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_484;
               break;
       case HTT_PPDU_STATS_RU_996:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_996;
               break;
       default:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_26;
               break;
       }

       return width;
}

/* Align bw value as per host data structures */
static u8 ath12k_htt_bw_to_mac_bw(u32 rate_flags)
{
       u8 bw = HTT_USR_RATE_BW(rate_flags);

       switch (bw) {
       case HTT_PPDU_STATS_BANDWIDTH_320MHZ:
               bw = ATH12K_BW_320;
               break;
       case HTT_PPDU_STATS_BANDWIDTH_160MHZ:
               bw = ATH12K_BW_160;
               break;
       case HTT_PPDU_STATS_BANDWIDTH_80MHZ:
               bw = ATH12K_BW_80;
               break;
       case HTT_PPDU_STATS_BANDWIDTH_40MHZ:
               bw = ATH12K_BW_40;
               break;
       default:
               bw = ATH12K_BW_20;
       break;
       }

       return bw;
}

static void
ath12k_update_per_peer_tx_stats(struct ath12k_pdev_dp *dp_pdev,
				struct htt_ppdu_stats_info *ppdu_info, u8 user)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct ieee80211_sta *sta;
	struct htt_ppdu_stats_user_rate *user_rate;
	struct htt_ppdu_stats *ppdu_stats = &ppdu_info->ppdu_stats;
	struct ath12k_per_peer_tx_stats *peer_stats = &dp_pdev->peer_tx_stats;
	struct htt_ppdu_user_stats *usr_stats = &ppdu_stats->user_stats[user];
	struct htt_ppdu_stats_common *common = &ppdu_stats->common;
	int ret;
	u8 flags, mcs, nss, bw, sgi, dcm, rate_idx = 0;
	u32 v, succ_bytes = 0, ppdu_type;
	u16 tones;
	u16 rate = 0, succ_pkts = 0, ru_start, ru_end;
	u32 tx_duration = 0, ru_tones, ru_format, tlv_bitmap, rate_flags;
	bool is_ampdu = false, resp_type_valid, is_ofdma;
	u8 tid = HTT_PPDU_STATS_NON_QOS_TID;
	u16 tx_retry_failed = 0, tx_retry_count = 0;

	if (!usr_stats)
		return;

	tlv_bitmap = ppdu_info->tlv_bitmap;

	if (!(tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_RATE)))
		return;

	if (tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON)) {
		is_ampdu =
			HTT_USR_CMPLTN_IS_AMPDU(usr_stats->cmpltn_cmn.flags);
		if (tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_SCH_CMD_STATUS)) {
			succ_pkts = usr_stats->cmpltn_cmn.mpdu_success;
		        tid = usr_stats->cmpltn_cmn.tid_num;
		}
	}

	if (tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS)) {
		succ_bytes = le32_to_cpu(usr_stats->ack_ba.success_bytes);
		succ_pkts = le32_get_bits(usr_stats->ack_ba.info,
					  HTT_PPDU_STATS_ACK_BA_INFO_NUM_MSDU_M);
		tid = le32_get_bits(usr_stats->ack_ba.info,
				    HTT_PPDU_STATS_ACK_BA_INFO_TID_NUM);
		dp_pdev->wmm_stats.tx_type = ath12k_tid_to_ac(tid > ATH12K_DSCP_PRIORITY ? 0: tid);
		dp_pdev->wmm_stats.total_wmm_tx_pkts[dp_pdev->wmm_stats.tx_type]++;
		if (tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON)) {
			tx_retry_failed =
				__le16_to_cpu(usr_stats->cmpltn_cmn.mpdu_tried) -
				__le16_to_cpu(usr_stats->cmpltn_cmn.mpdu_success);
			tx_retry_count =
				HTT_USR_CMPLTN_LONG_RETRY(usr_stats->cmpltn_cmn.flags) +
				HTT_USR_CMPLTN_SHORT_RETRY(usr_stats->cmpltn_cmn.flags);
		}
		if (common->fes_duration_us)
			tx_duration = le32_to_cpu(common->fes_duration_us);
	}

	user_rate = &usr_stats->rate;
	ppdu_type = HTT_USR_RATE_PPDU_TYPE(user_rate->info1);

	resp_type_valid = u32_get_bits(user_rate->info1,
                               HTT_PPDU_STATS_USER_RATE_INFO1_RESP_TYPE_VALID);
	if (resp_type_valid) {
		rate_flags = user_rate->resp_rate_flags;
	        ru_start = user_rate->resp_ru_start;
		ru_end = user_rate->ru_end;
	        ppdu_type = HTT_USR_RESP_RATE_PPDU_TYPE(user_rate->resp_rate_flags);
		if (ppdu_type == HTT_PPDU_STATS_RESP_PPDU_TYPE_MU_OFDMA_UL)
	                ppdu_type = HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA;
		else
	                ppdu_type = HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO;
	} else {
		rate_flags = user_rate->rate_flags;
	        ru_start = user_rate->ru_start;
		ru_end = user_rate->ru_end;
	}

	flags = HTT_USR_RATE_PREAMBLE(rate_flags);
	bw = ath12k_htt_bw_to_mac_bw(rate_flags);
	nss = HTT_USR_RATE_NSS(rate_flags) + 1;
	mcs = HTT_USR_RATE_MCS(rate_flags);
	sgi = HTT_USR_RATE_GI(rate_flags);
	dcm = HTT_USR_RATE_DCM(rate_flags);
	ru_format = FIELD_GET(HTT_PPDU_STATS_USER_RATE_INFO0_RU_SIZE,
		              user_rate->info0);
	if (ru_format == 1)
		ru_tones = ath12k_dp_rx_ru_alloc_from_ru_size(ru_start);
	else if (!ru_format)
		ru_tones = ru_end - ru_start + 1;
	else
		ru_tones = ath12k_dp_rx_ru_alloc_from_ru_size(HTT_PPDU_STATS_RU_26);

	is_ofdma = (ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA) |
		(ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA);

	ppdu_info->usr_ru_tones_sum += ru_tones;
	/* Note: If host configured fixed rates and in some other special
	 * cases, the broadcast/management frames are sent in different rates.
	 * Firmware rate's control to be skipped for this?
	 */

	if (flags == WMI_RATE_PREAMBLE_HE && mcs > ATH12K_HE_MCS_MAX) {
		ath12k_warn(ab, "Invalid HE mcs %d peer stats",  mcs);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_EHT && mcs > ATH12K_EHT_MCS_MAX) {
		ath12k_warn(ab, "Invalid EHT mcs %d peer stats",  mcs);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_VHT && mcs > ATH12K_VHT_MCS_MAX) {
		ath12k_warn(ab, "Invalid VHT mcs %d peer stats",  mcs);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_HT && (mcs > ATH12K_HT_MCS_MAX || nss < 1)) {
		ath12k_warn(ab, "Invalid HT mcs %d nss %d peer stats",
			    mcs, nss);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_CCK || flags == WMI_RATE_PREAMBLE_OFDM) {
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(mcs,
							    flags,
							    &rate_idx,
							    &rate);
		if (ret < 0)
			return;
	}

	rcu_read_lock();
	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, usr_stats->peer_id);

	if (!peer || !peer->sta) {
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
		return;
	}

	ahvif = ath12k_vif_to_ahvif(peer->vif);
	if (tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS)) {
		ahvif->wmm_stats.tx_type = dp_pdev->wmm_stats.tx_type;
		ahvif->wmm_stats.total_wmm_tx_pkts[ahvif->wmm_stats.tx_type]++;
	}
	sta = peer->sta;

	memset(&peer->txrate, 0, sizeof(peer->txrate));

	switch (flags) {
	case WMI_RATE_PREAMBLE_OFDM:
		peer->txrate.legacy = rate;
		break;
	case WMI_RATE_PREAMBLE_CCK:
		peer->txrate.legacy = rate;
		break;
	case WMI_RATE_PREAMBLE_HT:
		peer->txrate.mcs = mcs + 8 * (nss - 1);
		peer->txrate.flags = RATE_INFO_FLAGS_MCS;
		if (sgi)
			peer->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case WMI_RATE_PREAMBLE_VHT:
		peer->txrate.mcs = mcs;
		peer->txrate.flags = RATE_INFO_FLAGS_VHT_MCS;
		if (sgi)
			peer->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case WMI_RATE_PREAMBLE_HE:
		peer->txrate.mcs = mcs;
		peer->txrate.flags = RATE_INFO_FLAGS_HE_MCS;
		peer->txrate.he_dcm = dcm;
		peer->txrate.he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		peer->txrate.he_ru_alloc = ru_tones;
		peer_stats->ru_tones = peer->txrate.he_ru_alloc;
		break;
	case WMI_RATE_PREAMBLE_EHT:
		peer->txrate.mcs = mcs;
		peer->txrate.flags = RATE_INFO_FLAGS_EHT_MCS;
		peer->txrate.he_dcm = dcm;
		peer->txrate.eht_gi = ath12k_eht_gi_to_nl80211_eht_gi(sgi);
		tones = le16_to_cpu(user_rate->ru_end) -
			le16_to_cpu(user_rate->ru_start) + 1;
		v = ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(tones);
		peer->txrate.eht_ru_alloc = v;
		break;
	}

	peer->tx_retry_failed += tx_retry_failed;
	peer->tx_retry_count += tx_retry_count;
	peer->txrate.nss = nss;
	usr_stats->nss = nss;
	ppdu_info->usr_nss_sum += nss;
	peer->txrate.bw = ath12k_mac_bw_to_mac80211_bw(bw);
	peer->tx_duration += tx_duration;
	memcpy(&peer->last_txrate, &peer->txrate, sizeof(struct rate_info));

	if (is_ofdma) {
		if (flags == WMI_RATE_PREAMBLE_HE)
			peer->txrate.bw = RATE_INFO_BW_HE_RU;
		else if (flags == WMI_RATE_PREAMBLE_EHT)
			peer->txrate.bw = RATE_INFO_BW_EHT_RU;
	}

	/* PPDU stats reported for mgmt packet doesn't have valid tx bytes.
	 * So skip peer stats update for mgmt packets.
	 */
	if (tid < HTT_PPDU_STATS_NON_QOS_TID) {
		memset(peer_stats, 0, sizeof(*peer_stats));
		peer_stats->succ_pkts = succ_pkts;
		peer_stats->succ_bytes = succ_bytes;
		peer_stats->is_ampdu = is_ampdu;
		peer_stats->duration = tx_duration;
		peer_stats->ru_tones = ru_tones;
		peer_stats->ba_fails =
			HTT_USR_CMPLTN_LONG_RETRY(usr_stats->cmpltn_cmn.flags) +
			HTT_USR_CMPLTN_SHORT_RETRY(usr_stats->cmpltn_cmn.flags);
	}

	peer_stats->ppdu_type = ppdu_type;
	usr_stats->ru_tones = ru_tones;

	if (ath12k_extd_tx_stats_enabled(dp_pdev->ar))
		ath12k_debugfs_sta_add_tx_stats(peer, peer_stats, rate_idx);

	spin_unlock_bh(&dp->dp_lock);
	rcu_read_unlock();
}

static void
ath12k_ppdu_per_user_stats_phy_tx_time_update(struct ath12k_base *ab,
                                             struct ath12k_dp_link_peer *peer,
                                             const struct htt_ppdu_stats_info *ppdu_info,
                                             const struct htt_ppdu_user_stats *user)
{
       const struct htt_ppdu_stats_common *common = &ppdu_info->ppdu_stats.common;
       struct ath12k_dp_link_peer_stats *stats = NULL;
       u32 ru_nss_width_sum = 0;
       u16 phy_tx_time_us = 0;
       u8 tid;
       u8 ac;

	lockdep_assert_held(&ab->dp->dp_lock);

       if (!peer || !user || !common) {
               ath12k_warn(ab, "Invalid ppdu user info received\n");
               return;
       }

       ru_nss_width_sum = ppdu_info->usr_nss_sum * ppdu_info->usr_ru_tones_sum;
       if (!ru_nss_width_sum)
               ru_nss_width_sum = 1;

       if (ppdu_info->htt_frame_type == HTT_STATS_FTYPE_TIDQ_DATA_SU)
               phy_tx_time_us = common->phy_ppdu_tx_time_us;
       else
               phy_tx_time_us = (common->phy_ppdu_tx_time_us *
                                 user->nss * user->ru_tones) / ru_nss_width_sum;

       tid = user->rate.tid_num;
       ac = ath12k_tid_to_ac(tid);
       stats = &peer->peer_stats;
       stats->dp_mon_stats.mon_stats.tx_airtime_consumption[ac].consumption += phy_tx_time_us;
       ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "ppdu info id: %d tid: %d htt frame type: %d  ppdu frame type: %d time: %d nss: %d tones: %d sum [nss: %d tone: %d consum: %d]\n",
                  ppdu_info->ppdu_id,
                  user->rate.tid_num,
                  ppdu_info->htt_frame_type,
                  ppdu_info->frame_type,
                  common->phy_ppdu_tx_time_us,
                  user->nss, user->ru_tones,
                  ppdu_info->usr_nss_sum, ppdu_info->usr_ru_tones_sum,
                  stats->dp_mon_stats.mon_stats.tx_airtime_consumption[ac].consumption);
}

static void ath12k_htt_update_peer_telemetry_stats(struct ath12k_pdev_dp *dp_pdev,
						   struct htt_ppdu_stats_info *ppdu_info)
{
       struct ath12k_base *ab = dp_pdev->ar->ab;
       struct ath12k_dp_link_peer *peer;
       struct ieee80211_sta *sta;
       struct ath12k_link_sta *arsta;
       struct htt_ppdu_stats *ppdu_stats = &ppdu_info->ppdu_stats;
       struct htt_ppdu_user_stats *user_stats = NULL;
       u32 tlv_bitmap;
       u8 uid;

       if (!ppdu_info)
               return;

       if (ppdu_info->frame_type != HTT_STATS_PPDU_FTYPE_DATA)
               return;

       tlv_bitmap = ppdu_info->tlv_bitmap;
       if (!(tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_RATE)))
               return;

       for (uid = 0; uid < HTT_PPDU_STATS_MAX_USERS; uid++) {
               user_stats = &ppdu_stats->user_stats[uid];

		spin_lock_bh(&dp_pdev->dp->dp_lock);

               peer = ath12k_dp_link_peer_find_by_id(dp_pdev->dp,
						     user_stats->peer_id);
               if (!peer || !peer->sta) {
			spin_unlock_bh(&dp_pdev->dp->dp_lock);
                       return;
               }

               sta = peer->sta;

		rcu_read_lock();
               arsta = ath12k_peer_get_link_sta(ab, peer);
               if (!arsta) {
			rcu_read_unlock();
			spin_unlock_bh(&dp_pdev->dp->dp_lock);
                       return;
               }
		rcu_read_unlock();

               ath12k_ppdu_per_user_stats_phy_tx_time_update(ab, peer,
                                                             ppdu_info,
                                                             user_stats);
		spin_unlock_bh(&dp_pdev->dp->dp_lock);
       }
}

static void ath12k_htt_update_ppdu_stats(struct ath12k_pdev_dp *dp_pdev,
					 struct htt_ppdu_stats_info *ppdu_info)
{
	u8 user;

	for (user = 0; user < HTT_PPDU_STATS_MAX_USERS - 1; user++)
		ath12k_update_per_peer_tx_stats(dp_pdev, ppdu_info, user);

	ath12k_htt_update_peer_telemetry_stats(dp_pdev, ppdu_info);
}

static
struct htt_ppdu_stats_info *ath12k_dp_htt_get_ppdu_desc(struct ath12k_pdev_dp *dp_pdev,
							u32 ppdu_id)
{
	struct htt_ppdu_stats_info *ppdu_info;

	lockdep_assert_held(&dp_pdev->ppdu_list_lock);
	if (!list_empty(&dp_pdev->ppdu_stats_info)) {
		list_for_each_entry(ppdu_info, &dp_pdev->ppdu_stats_info, list) {
			if (ppdu_info->ppdu_id == ppdu_id)
				return ppdu_info;
		}

		if (dp_pdev->ppdu_stat_list_depth > HTT_PPDU_DESC_MAX_DEPTH) {
			ppdu_info = list_first_entry(&dp_pdev->ppdu_stats_info,
						     typeof(*ppdu_info), list);
			list_del(&ppdu_info->list);
			dp_pdev->ppdu_stat_list_depth--;
			/* Update the stats once per ppdu info as this function can be
			 * called multiple times per ppdu info with data frame,
			 * avoid updating the same user stats again for data frame
			 */
			if (ppdu_info->frame_type != HTT_STATS_PPDU_FTYPE_DATA)
			        ath12k_htt_update_ppdu_stats(dp_pdev, ppdu_info);
			kfree(ppdu_info);
		}
	}

	ppdu_info = kzalloc(sizeof(*ppdu_info), GFP_ATOMIC);
	if (!ppdu_info)
		return NULL;

	list_add_tail(&ppdu_info->list, &dp_pdev->ppdu_stats_info);
	dp_pdev->ppdu_stat_list_depth++;

	return ppdu_info;
}

static void ath12k_copy_to_delay_stats(struct ath12k_dp_link_peer *peer,
				       struct htt_ppdu_user_stats *usr_stats)
{
	peer->ppdu_stats_delayba.sw_peer_id = le16_to_cpu(usr_stats->rate.sw_peer_id);
	peer->ppdu_stats_delayba.info0 = le32_to_cpu(usr_stats->rate.info0);
	peer->ppdu_stats_delayba.ru_end = le16_to_cpu(usr_stats->rate.ru_end);
	peer->ppdu_stats_delayba.ru_start = le16_to_cpu(usr_stats->rate.ru_start);
	peer->ppdu_stats_delayba.info1 = le32_to_cpu(usr_stats->rate.info1);
	peer->ppdu_stats_delayba.rate_flags = le32_to_cpu(usr_stats->rate.rate_flags);
	peer->ppdu_stats_delayba.resp_rate_flags =
		le32_to_cpu(usr_stats->rate.resp_rate_flags);

	peer->delayba_flag = true;
}

static void ath12k_copy_to_bar(struct ath12k_dp_link_peer *peer,
			       struct htt_ppdu_user_stats *usr_stats)
{
	usr_stats->rate.sw_peer_id = cpu_to_le16(peer->ppdu_stats_delayba.sw_peer_id);
	usr_stats->rate.info0 = cpu_to_le32(peer->ppdu_stats_delayba.info0);
	usr_stats->rate.ru_end = cpu_to_le16(peer->ppdu_stats_delayba.ru_end);
	usr_stats->rate.ru_start = cpu_to_le16(peer->ppdu_stats_delayba.ru_start);
	usr_stats->rate.info1 = cpu_to_le32(peer->ppdu_stats_delayba.info1);
	usr_stats->rate.rate_flags = cpu_to_le32(peer->ppdu_stats_delayba.rate_flags);
	usr_stats->rate.resp_rate_flags =
		cpu_to_le32(peer->ppdu_stats_delayba.resp_rate_flags);

	peer->delayba_flag = false;
}

static void
ath12k_dp_htt_ppdu_stats_update_tx_comp_stats(struct ath12k_pdev_dp *dp_pdev,
		                struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_sta *arsta;
	struct ath12k_dp_link_peer *peer = NULL;
	struct htt_ppdu_user_stats* usr_stats = NULL;
	struct rate_info rate;
	struct ieee80211_tx_status status;
	struct ieee80211_rate_status status_rate = { 0 };
	u32 peer_id = 0;
	int i;

	lockdep_assert_held(&ar->data_lock);

	ath12k_htt_update_ppdu_stats(dp_pdev, ppdu_info);
	/* Update below stats when msdu update path is disabled, It is likely
	 * the case when KPI is enabled
	 */
	if (!ab->stats_disable)
		return;

	for (i = 0; i < ppdu_info->ppdu_stats.common.num_users; i++) {
		usr_stats = &ppdu_info->ppdu_stats.user_stats[i];
		peer_id = usr_stats->peer_id;
		rcu_read_lock();
		peer = ath12k_dp_link_peer_find_by_peerid_index(ab->dp,
								dp_pdev,
								peer_id);
		if (unlikely(!peer || !peer->sta)) {
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "dp_tx: failed to find the peer with peer_id %d\n",
				peer_id);
			rcu_read_unlock();
			continue;
		}

		if (peer->vif->type != NL80211_IFTYPE_MESH_POINT) {
			rcu_read_unlock();
			return;
		}

		if (ether_addr_equal(peer->addr, peer->vif->addr)) {
			rcu_read_unlock();
			continue;
		}

		arsta = ath12k_peer_get_link_sta(ab, peer);
		if (!arsta) {
			ath12k_warn(ab, "link sta not found on peer %pM id %d\n",
				    peer->addr, peer->peer_id);
			rcu_read_unlock();
			continue;
		}

		memset(&status, 0, sizeof(status));

		status.sta = peer->sta;
		rate = peer->last_txrate;

		status_rate.rate_idx = rate;
		status_rate.try_count = 1;

		status.rates = &status_rate;
		status.n_rates = 1;
		status.mpdu_succ = usr_stats->cmpltn_cmn.mpdu_success;

		ieee80211s_update_metric_ppdu(ar->ah->hw, &status);
		rcu_read_unlock();
	}
}

static int ath12k_htt_pull_ppdu_stats(struct ath12k_base *ab,
				      struct sk_buff *skb)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_htt_ppdu_stats_msg *msg;
	struct htt_ppdu_stats_info *ppdu_info;
	struct ath12k_dp_link_peer *peer = NULL;
	struct htt_ppdu_user_stats *usr_stats = NULL;
	u32 peer_id = 0;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k *ar;
	int ret, i;
	u8 pdev_id;
	u32 ppdu_id, len;

	msg = (struct ath12k_htt_ppdu_stats_msg *)skb->data;
	len = le32_get_bits(msg->info, HTT_T2H_PPDU_STATS_INFO_PAYLOAD_SIZE);
	if (len > (skb->len - struct_size(msg, data, 0))) {
		ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
			   "HTT PPDU STATS event has unexpected payload size %u, should be smaller than %u\n",
			   len, skb->len);
		return -EINVAL;
	}

	pdev_id = le32_get_bits(msg->info, HTT_T2H_PPDU_STATS_INFO_PDEV_ID);
	ppdu_id = le32_to_cpu(msg->ppdu_id);

	if (pdev_id < 1) {
		ath12k_warn(ab, "HTT PPDU STATS invalid pdev id");
		return -EINVAL;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		/* It is possible that the ar is not yet active (started).
		 * The above function will only look for the active pdev
		 * and hence %NULL return is possible. Just silently
		 * discard this message
		 */
		rcu_read_unlock();
		goto exit;
	}

	if (ar->debug.is_pkt_logging &&
	    (dp->rx_pktlog_mode == ATH12K_PKTLOG_MODE_LITE)) {
		trace_ath12k_htt_ppdu_stats(ar, skb->data, len);
		ath12k_htt_ppdu_pktlog_process(ar, (u8 *)skb->data, skb->len);
	}

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id - 1);
	if (!dp_pdev) {
		rcu_read_unlock();
		ret = -EINVAL;
		goto exit;
	}
	rcu_read_unlock();

	spin_lock_bh(&dp_pdev->ppdu_list_lock);
	ppdu_info = ath12k_dp_htt_get_ppdu_desc(dp_pdev, ppdu_id);
	if (!ppdu_info) {
		spin_unlock_bh(&dp_pdev->ppdu_list_lock);
		ret = -EINVAL;
		goto exit;
	}

	ppdu_info->pdev_id = pdev_id;
	ppdu_info->ppdu_id = ppdu_id;
	ret = ath12k_dp_htt_tlv_iter(ab, msg->data, len,
				     ath12k_htt_tlv_ppdu_stats_parse,
				     (void *)ppdu_info);
	if (ret) {
		spin_unlock_bh(&dp_pdev->ppdu_list_lock);
		ath12k_warn(ab, "Failed to parse tlv %d\n", ret);
		goto exit;
	}

	if (ppdu_info->ppdu_stats.common.num_users >= HTT_PPDU_STATS_MAX_USERS) {
		spin_unlock_bh(&dp_pdev->ppdu_list_lock);
		ath12k_warn(ab,
			    "HTT PPDU STATS event has unexpected num_users %u, should be smaller than %u\n",
			    ppdu_info->ppdu_stats.common.num_users,
			    HTT_PPDU_STATS_MAX_USERS);
		ret = -EINVAL;
		goto exit;
	}

	/* back up data rate tlv for all peers */
	if (ppdu_info->frame_type == HTT_STATS_PPDU_FTYPE_DATA &&
	    (ppdu_info->tlv_bitmap & (1 << HTT_PPDU_STATS_TAG_USR_COMMON)) &&
	    ppdu_info->delay_ba) {
		for (i = 0; i < ppdu_info->ppdu_stats.common.num_users; i++) {
			peer_id = ppdu_info->ppdu_stats.user_stats[i].peer_id;
			spin_lock_bh(&dp->dp_lock);
			peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
			if (!peer) {
				spin_unlock_bh(&dp->dp_lock);
				continue;
			}

			usr_stats = &ppdu_info->ppdu_stats.user_stats[i];
			if (usr_stats->delay_ba)
				ath12k_copy_to_delay_stats(peer, usr_stats);
			spin_unlock_bh(&dp->dp_lock);
		}
	}

	/* restore all peers' data rate tlv to mu-bar tlv */
	if (ppdu_info->frame_type == HTT_STATS_PPDU_FTYPE_BAR &&
	    (ppdu_info->tlv_bitmap & (1 << HTT_PPDU_STATS_TAG_USR_COMMON))) {
		for (i = 0; i < ppdu_info->bar_num_users; i++) {
			peer_id = ppdu_info->ppdu_stats.user_stats[i].peer_id;
			spin_lock_bh(&dp->dp_lock);
			peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
			if (!peer) {
				spin_unlock_bh(&dp->dp_lock);
				continue;
			}

			usr_stats = &ppdu_info->ppdu_stats.user_stats[i];
			if (peer->delayba_flag)
				ath12k_copy_to_bar(peer, usr_stats);
			spin_unlock_bh(&dp->dp_lock);
		}
	}

	/* Update tx completion stats */
	if ((ppdu_info->frame_type == HTT_STATS_PPDU_FTYPE_DATA &&
		(ppdu_info->tlv_bitmap & (1 << HTT_PPDU_STATS_TAG_USR_RATE)) &&
		 ppdu_info->tlv_bitmap & (1 << HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON)))
		ath12k_dp_htt_ppdu_stats_update_tx_comp_stats(dp_pdev, ppdu_info);

	spin_unlock_bh(&dp_pdev->ppdu_list_lock);

exit:
	return ret;
}

static void ath12k_htt_backpressure_event_handler(struct ath12k_base *ab,
						  struct sk_buff *skb)
{
	u32 *data = (u32 *)skb->data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u8 pdev_id, ring_type, ring_id, pdev_idx;
	u16 hp, tp;
	u32 backpressure_time;
	struct ath12k_bp_stats *bp_stats;

	pdev_id = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_PDEV_ID_M);
	ring_type = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_RING_TYPE_M);
	ring_id = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_RING_ID_M);
	++data;

	hp = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_HP_M);
	tp = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_TP_M);
	++data;

	backpressure_time = *data;

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "htt backpressure event, pdev %d, ring type %d,ring id %d, hp %d tp %d, backpressure time %d\n",
		   pdev_id, ring_type, ring_id, hp, tp, backpressure_time);

	if (ring_type == HTT_BACKPRESSURE_UMAC_RING_TYPE) {
		if (ring_id >= HTT_SW_UMAC_RING_IDX_MAX)
			return;

		bp_stats = &dp->device_stats.bp_stats.umac_ring_bp_stats[ring_id];
	} else if (ring_type == HTT_BACKPRESSURE_LMAC_RING_TYPE) {
		pdev_idx = DP_HW2SW_MACID(pdev_id);

		if (ring_id >= HTT_SW_LMAC_RING_IDX_MAX || pdev_idx >= MAX_RADIOS)
			return;

		bp_stats = &dp->device_stats.bp_stats.lmac_ring_bp_stats[ring_id][pdev_idx];
	} else {
		ath12k_warn(ab, "unknown ring type received in htt bp event %d\n",
			    ring_type);
		return;
	}

	spin_lock_bh(&ab->base_lock);
	bp_stats->hp = hp;
	bp_stats->tp = tp;
	bp_stats->count++;
	bp_stats->jiffies = jiffies;
	spin_unlock_bh(&ab->base_lock);
}

static void ath12k_htt_mlo_offset_event_handler(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	struct ath12k_htt_mlo_offset_msg *msg;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int i, j;
	u8 pdev_id;

	msg = (struct ath12k_htt_mlo_offset_msg *)skb->data;
	pdev_id = u32_get_bits(__le32_to_cpu(msg->info),
			       HTT_T2H_MLO_OFFSET_INFO_PDEV_ID);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		/* It is possible that the ar is not yet active (started).
		 * The above function will only look for the active pdev
		 * and hence %NULL return is possible. Just silently
		 * discard this message
		 */
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	pdev = ar->pdev;

	pdev->timestamp.info = __le32_to_cpu(msg->info);
	pdev->timestamp.sync_timestamp_lo_us = __le32_to_cpu(msg->sync_timestamp_lo_us);
	pdev->timestamp.sync_timestamp_hi_us = __le32_to_cpu(msg->sync_timestamp_hi_us);
	pdev->timestamp.mlo_offset_lo = __le32_to_cpu(msg->mlo_offset_lo);
	pdev->timestamp.mlo_offset_hi = __le32_to_cpu(msg->mlo_offset_hi);
	pdev->timestamp.mlo_offset_clks = __le32_to_cpu(msg->mlo_offset_clks);
	pdev->timestamp.mlo_comp_clks = __le32_to_cpu(msg->mlo_comp_clks);
	pdev->timestamp.mlo_comp_timer = __le32_to_cpu(msg->mlo_comp_timer);

	ag->mlo_tstamp_offset = ((u64)pdev->timestamp.mlo_offset_hi << 32 |
				 pdev->timestamp.mlo_offset_lo);

	/* MLO TSAMP OFFSET is common for all chips and
	 * fetch delta_tsf2 for all the radios for this event
	 */
	for (i = 0; i < ag->num_devices; i++) {
		struct ath12k_base *tmp_ab = ag->ab[i];

		/* Skip MLO offset processing during recovery to avoid NOC errors.
		 * PMM registers are not accessible during chip recovery, and accessing
		 * them causes NOC bus errors. MLO timestamp synchronization will resume
		 * after recovery completes.
		 */
		if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &tmp_ab->dev_flags) ||
		    test_bit(ATH12K_FLAG_RECOVERY, &tmp_ab->dev_flags))
			continue;

		for (j = 0; j < tmp_ab->num_radios; j++) {
			struct ath12k *tmp_ar;

			pdev = &tmp_ab->pdevs[j];
			tmp_ar = pdev->ar;
			if (!tmp_ar || !tmp_ab->ce_pipe_init_done)
				continue;

			if (tmp_ab->hw_params->hal_ops->hal_get_tsf2_scratch_reg)
				tmp_ab->hw_params->hal_ops->hal_get_tsf2_scratch_reg(tmp_ab, tmp_ar->lmac_id,
										     &tmp_ar->delta_tsf2);
		}
	}

	spin_unlock_bh(&ar->data_lock);
exit:
	rcu_read_unlock();
}

static void
ath12k_htt_pktlog_tx_handler(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct ath12k_htt_pktlog_msg *msg;
	u16 payload_size;
	u8 pdev_id;

	if (!skb->data)
		return;

	msg = (struct ath12k_htt_pktlog_msg *)skb->data;

	pdev_id = le32_get_bits(msg->header, HTT_T2H_PKTLOG_PDEV_ID);
	if (pdev_id < 1) {
		ath12k_warn(ab, "HTT PKTLOG MSG has invalid pdev id");
		return;
	}

	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "pktlog tx: failed to get ar\n");
		return;
	}

	payload_size = le32_get_bits(msg->header, HTT_T2H_PKTLOG_PAYLOAD_SIZE);
	trace_ath12k_htt_pktlog_tx_handler(ar, msg->payload, payload_size,
					   ar->ab->pktlog_defs_checksum);

	if (ar->debug.is_pkt_logging)
		ath12k_htt_pktlog_process(ar, (u8 *)msg->payload);
}

static void ath12k_htt_t2h_ppdu_id_fmt_handler(struct ath12k_dp *dp,
					       struct sk_buff *skb)
{
	u8 valid, bits, offset;
	struct ath12k_htt_ppdu_id_fmt_info *msg;

	msg = (struct ath12k_htt_ppdu_id_fmt_info *)skb->data;
	valid = le32_get_bits(msg->link_id, HTT_PPDU_ID_FMT_VALID_BITS);
	bits = le32_get_bits(msg->link_id, HTT_PPDU_ID_FMT_GET_BITS);
	offset = le32_get_bits(msg->link_id, HTT_PPDU_ID_FMT_GET_OFFSET);

	if (valid) {
		dp->link_id_bits = bits;
		dp->link_id_offset = offset;
	}
}

static void
ath12k_htt_pri_link_peer_migrate_indication(struct ath12k_base *ab,
					    struct sk_buff *skb)
{
	struct ath12k_htt_pri_link_migr_ind_msg *msg;
	struct ath12k_hw_group *ag = ab->ag;
	u16 vdev_id, peer_id, ml_peer_id;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *pri_ab;
	struct ath12k_dp *dp;
	struct ath12k_sta *ahsta;
	u8 pdev_id, chip_id;
	int ret;

	msg = (struct ath12k_htt_pri_link_migr_ind_msg *)skb->data;

	chip_id = le32_get_bits(msg->info0, ATH12K_HTT_PRI_LINK_MIGR_CHIP_ID);
	pdev_id = le32_get_bits(msg->info0, ATH12K_HTT_PRI_LINK_MIGR_PDEV_ID);
	vdev_id = le32_get_bits(msg->info0, ATH12K_HTT_PRI_LINK_MIGR_VDEV_ID);
	peer_id = le32_get_bits(msg->info1, ATH12K_HTT_PRI_LINK_MIGR_PEER_ID);
	ml_peer_id = le32_get_bits(msg->info1, ATH12K_HTT_PRI_LINK_MIGR_ML_PEER_ID);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
		   "htt MLO pri link migr ind for peer_id 0x%x ml_peer_id 0x%x vdev_id 0x%x pdev_id 0x%x chip_id 0x%x\n",
		   peer_id, ml_peer_id, vdev_id, pdev_id, chip_id);

	ml_peer_id |= ATH12K_PEER_ML_ID_VALID;

	if (chip_id >= ATH12K_MAX_SOCS) {
		ath12k_warn(ab,
			    "htt incorrect chip id %d in MLO pri link migration event\n",
			    chip_id);
		goto err_pri_link_migr_ind;
	}

	pri_ab = ag->ab[chip_id];
	if (!pri_ab) {
		ath12k_warn(ab,
			    "htt can not find ab for chip id %d in MLO pri link migration event\n",
			    chip_id);
		goto err_pri_link_migr_ind;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(pri_ab, vdev_id);
	if (!arvif || !arvif->ar) {
		ath12k_err(pri_ab, "htt error in getting arvif from vdev id:%d\n",
			   vdev_id);
		rcu_read_unlock();
		goto err_pri_link_migr_ind;
	}
	rcu_read_unlock();

	dp = ath12k_ab_to_dp(pri_ab);

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (!peer) {
		ath12k_warn(pri_ab, "htt can not find peer fo peer id %d\n",
			    peer_id);
		goto exit_pri_link_migr_ind;
	}

	if (peer->ml_id != ml_peer_id) {
		ath12k_warn(pri_ab, "htt ML peer id mis-match. Expected %d got %d\n",
			    peer->ml_id, ml_peer_id);
		goto exit_pri_link_migr_ind;
	}

	if (peer->primary_link) {
		ath12k_warn(pri_ab, "htt ML peer is already primary\n");
		goto exit_pri_link_migr_ind;
	}

	ahsta = ath12k_sta_to_ahsta(peer->sta);

	ahsta->migration_data.ab = ab;
	ahsta->migration_data.vdev_id = vdev_id;
	ahsta->migration_data.peer_id = peer_id;
	ahsta->migration_data.ml_peer_id = ml_peer_id;
	ahsta->migration_data.pdev_id = pdev_id;
	ahsta->migration_data.chip_id = chip_id;
	ahsta->migration_data.ppe_vp_num = peer->dp_peer->ppe_vp_num;

	reinit_completion(&ahsta->dp_migration_event);

	ret = ath12k_dp_peer_migrate(ahsta, peer_id, chip_id);
	if (ret)
		ath12k_warn(pri_ab, "htt ML peer failed to migrate (%d)\n", ret);

exit_pri_link_migr_ind:
	spin_unlock_bh(&dp->dp_lock);
err_pri_link_migr_ind:
	ieee80211_queue_work(arvif->ar->ah->hw, &ahsta->migration_wk);
}

static int ath12k_svc_burst_stats_update(struct ath12k_base *ab,
					 struct ath12k_dp_peer *mld_peer,
					 u8 tid, u8 q_type, u16 svc_int_success,
					 u16 svc_int_fail, u16 burst_sz_success,
					 u16 burst_sz_fail)
{
	struct fw_mpdu_stats *svc_intval_stats;
	struct fw_mpdu_stats *burst_size_stats;
	u8 q_id;

	if (tid >= QOS_TID_MAX)
		return -EINVAL;

	if (q_type >= QOS_TID_DEF_MSDUQ_MAX + QOS_TID_MDSUQ_MAX)
		return -EINVAL;

	q_id = q_type - QOS_TID_MDSUQ_MAX;

	svc_intval_stats = &mld_peer->mld_qos_stats[tid][q_id].svc_intval_stats;
	if (!svc_intval_stats)
		return -ENODATA;

	svc_intval_stats->success_cnt += svc_int_success;
	svc_intval_stats->failure_cnt += svc_int_fail;

	burst_size_stats = &mld_peer->mld_qos_stats[tid][q_id].burst_size_stats;
	if (!burst_size_stats)
		return -ENODATA;

	burst_size_stats->success_cnt += burst_sz_success;
	burst_size_stats->failure_cnt += burst_sz_fail;

	return 0;
}

static int ath12k_fw_mpdu_stats_update(struct ath12k_base *ab,
				       u32 tlv_tag,
				       u8 *data)
{
	struct htt_stats_strm_gen_mpdus_tlv *mpdus_tlv;
	struct htt_stats_strm_gen_mpdus_details_tlv *mpdus_detail_tlv;
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k *ar;
	struct ath12k_dp_hw *dp_hw;
	struct ath12k_pdev_dp *dp_pdev;
	int ret = 0, vdev_id;
	u16 svc_int_success, svc_int_failure, burst_sz_success;
	u16 info, burst_sz_failure, peer_id;
	u8 tid, q_type;

	if (tlv_tag == HTT_STATS_STRM_GEN_MPDUS_TAG) {
		mpdus_tlv = (struct htt_stats_strm_gen_mpdus_tlv *)data;
		peer_id = __le16_to_cpu(mpdus_tlv->peer_id);
		info = __le16_to_cpu(mpdus_tlv->info);
		svc_int_success = __le16_to_cpu(mpdus_tlv->svc_interval_success);
		svc_int_failure = __le16_to_cpu(mpdus_tlv->svc_interval_failure);
		burst_sz_success = __le16_to_cpu(mpdus_tlv->burst_size_success);
		burst_sz_failure = __le16_to_cpu(mpdus_tlv->burst_size_failure);
	} else if (tlv_tag == HTT_STATS_STRM_GEN_MPDUS_DETAILS_TAG) {
		mpdus_detail_tlv = (struct htt_stats_strm_gen_mpdus_details_tlv *)data;
		info = __le16_to_cpu(mpdus_detail_tlv->info);
		peer_id = __le16_to_cpu(mpdus_detail_tlv->peer_id);
	} else {
		return -ENOENT;
	}

	tid = u16_get_bits(info, SAWF_TTH_TID_MASK);
	q_type = u16_get_bits(info, SAWF_TTH_QTYPE_MASK);

	spin_lock_bh(&dp->dp_lock);
	link_peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (!link_peer) {
		spin_unlock_bh(&dp->dp_lock);
		return -ENOENT;
	}
	vdev_id = link_peer->vdev_id;
	spin_unlock_bh(&dp->dp_lock);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		rcu_read_unlock();
		return -ENOENT;
	}

	if (!ar->ah) {
		rcu_read_unlock();
		return -ENOENT;
	}

	dp_hw = &ar->ah->dp_hw;
	dp_pdev = &ar->dp;
	if (!(ath12k_debugfs_is_qos_stats_enabled(ar) &
	      ATH12K_QOS_STATS_ADVANCED)) {
		rcu_read_unlock();
		return -EOPNOTSUPP;
	}

	if (tlv_tag == HTT_STATS_STRM_GEN_MPDUS_TAG) {
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
		if (!dp_peer) {
			rcu_read_unlock();
			return -ENOENT;
		}
		spin_lock_bh(&dp_hw->peer_lock);
		ath12k_svc_burst_stats_update(ab, dp_peer, tid,
					      q_type, svc_int_success,
					      svc_int_failure,
					      burst_sz_success,
					      burst_sz_failure);
		spin_unlock_bh(&dp_hw->peer_lock);
		rcu_read_unlock();
	} else {
		rcu_read_unlock();
		ath12k_dbg(ab, ATH12K_DBG_QOS, "SDWF: peer_id %u tid %u qtype %u "
			   "svc_intvl: ts_prior %ums ts_now %ums "
			   "intvl_spec %ums margin %ums|"
			   "burst_size: consumed_bytes_orig %u "
			   "consumed_bytes_final %u remaining_bytes %u "
			   "burst_size_spec %u margin_bytes %u\n",
			   __le16_to_cpu(mpdus_detail_tlv->peer_id),
			   tid, q_type,
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_timestamp_prior_ms),
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_timestamp_now_ms),
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_interval_spec_ms),
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_interval_margin_ms),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_consumed_bytes_orig),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_consumed_bytes_final),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_remaining_bytes),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_burst_size_spec),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_margin_bytes));
	}
	return ret;
}

void ath12k_htt_sawf_streaming_stats_ind_handler(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	const struct htt_tlv *tlv;
	u8 *data = NULL;
	u8 *tlv_data;
	u32 len, tlv_tag, tlv_len;

	data = skb->data + HTT_T2H_STREAMING_STATS_IND_HDR_SIZE;
	len = skb->len;

	if (len > HTT_T2H_STREAMING_STATS_IND_HDR_SIZE)
		len -= HTT_T2H_STREAMING_STATS_IND_HDR_SIZE;
	else
		return;

	while (len > 0) {
		tlv_data = data;
		tlv = (struct htt_tlv *)data;
		tlv_tag = le32_get_bits(tlv->header, HTT_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, HTT_TLV_LEN);

		if (!tlv_len)
			break;

		if (len < tlv_len) {
			ath12k_err(ab, "SDWF: len %d tlv_len %d\n", len, tlv_len);
			break;
		}

		data += sizeof(*tlv);

		ath12k_fw_mpdu_stats_update(ab, tlv_tag, data);

		data = (tlv_data + tlv_len);
		len -= tlv_len;
	}
}

void ath12k_dp_htt_htc_t2h_msg_handler(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_resp_msg *resp = (struct htt_resp_msg *)skb->data;
	enum htt_t2h_msg_type type;
	u16 peer_id;
	u8 vdev_id;
	u8 mac_addr[ETH_ALEN];
	u16 peer_mac_h16;
	u16 ast_hash = 0;
	u16 hw_peer_id;

	type = le32_get_bits(resp->version_msg.version, HTT_T2H_MSG_TYPE);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "dp_htt rx msg type :0x%0x\n", type);

	switch (type) {
	case HTT_T2H_MSG_TYPE_VERSION_CONF:
		dp->htt_tgt_ver_major = le32_get_bits(resp->version_msg.version,
						      HTT_T2H_VERSION_CONF_MAJOR);
		dp->htt_tgt_ver_minor = le32_get_bits(resp->version_msg.version,
						      HTT_T2H_VERSION_CONF_MINOR);
		complete(&dp->htt_tgt_version_received);
		break;
	/* TODO: remove unused peer map versions after testing */
	case HTT_T2H_MSG_TYPE_PEER_MAP:
		vdev_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_VDEV_ID);
		peer_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_PEER_ID);
		peer_mac_h16 = le32_get_bits(resp->peer_map_ev.info1,
					     HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16);
		ath12k_dp_get_mac_addr(le32_to_cpu(resp->peer_map_ev.mac_addr_l32),
				       peer_mac_h16, mac_addr);
		ath12k_peer_map_event(ab, vdev_id, peer_id, mac_addr, 0, 0);
		break;
	case HTT_T2H_MSG_TYPE_PEER_MAP2:
		vdev_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_VDEV_ID);
		peer_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_PEER_ID);
		peer_mac_h16 = le32_get_bits(resp->peer_map_ev.info1,
					     HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16);
		ath12k_dp_get_mac_addr(le32_to_cpu(resp->peer_map_ev.mac_addr_l32),
				       peer_mac_h16, mac_addr);
		ast_hash = le32_get_bits(resp->peer_map_ev.info2,
					 HTT_T2H_PEER_MAP_INFO2_AST_HASH_VAL);
		hw_peer_id = le32_get_bits(resp->peer_map_ev.info1,
					   HTT_T2H_PEER_MAP_INFO1_HW_PEER_ID);
		ath12k_peer_map_event(ab, vdev_id, peer_id, mac_addr, ast_hash,
				      hw_peer_id);
		break;
	case HTT_T2H_MSG_TYPE_PEER_MAP3:
		vdev_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_VDEV_ID);
		peer_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_PEER_ID);
		peer_mac_h16 = le32_get_bits(resp->peer_map_ev.info1,
					     HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16);
		ath12k_dp_get_mac_addr(le32_to_cpu(resp->peer_map_ev.mac_addr_l32),
				       peer_mac_h16, mac_addr);
		ast_hash = le32_get_bits(resp->peer_map_ev.info2,
					 HTT_T2H_PEER_MAP3_INFO2_AST_HASH_VAL);
		hw_peer_id = le32_get_bits(resp->peer_map_ev.info2,
					   HTT_T2H_PEER_MAP3_INFO2_HW_PEER_ID);
		ath12k_peer_map_event(ab, vdev_id, peer_id, mac_addr, ast_hash,
				      hw_peer_id);
		break;
	case HTT_T2H_MSG_TYPE_PEER_UNMAP:
	case HTT_T2H_MSG_TYPE_PEER_UNMAP2:
		peer_id = le32_get_bits(resp->peer_unmap_ev.info,
					HTT_T2H_PEER_UNMAP_INFO_PEER_ID);
		ath12k_peer_unmap_event(ab, peer_id);
		break;
	case HTT_T2H_MSG_TYPE_PPDU_STATS_IND:
		ath12k_htt_pull_ppdu_stats(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_EXT_STATS_CONF:
		ath12k_debugfs_htt_ext_stats_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_BKPRESSURE_EVENT_IND:
		ath12k_htt_backpressure_event_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_MLO_TIMESTAMP_OFFSET_IND:
		ath12k_htt_mlo_offset_event_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_PKTLOG:
		ath12k_htt_pktlog_tx_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_MLO_RX_PEER_MAP:
		ath12k_peer_mlo_map_event(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_QOS_MSDUQ_INFO_IND:
		ath12k_peer_qos_queue_ind_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_MLO_RX_PEER_UNMAP:
		ath12k_peer_mlo_unmap_event(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_PPDU_ID_FMT_IND:
		ath12k_htt_t2h_ppdu_id_fmt_handler(dp, skb);
		break;
	case HTT_T2H_MSG_TYPE_STREAMING_STATS_IND:
		ath12k_htt_sawf_streaming_stats_ind_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_IND:
		ath12k_htt_pri_link_peer_migrate_indication(ab, skb);
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "dp_htt event %d not handled\n",
			   type);
		break;
	}

	dev_kfree_skb_any(skb);
}
EXPORT_SYMBOL(ath12k_dp_htt_htc_t2h_msg_handler);

#define HTT_TARGET_VERSION_TIMEOUT_HZ (3 * HZ)

int ath12k_dp_tx_htt_h2t_ver_req_msg(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ver_req_cmd *cmd;
	int len = sizeof(*cmd);
	int ret;

	init_completion(&dp->htt_tgt_version_received);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_ver_req_cmd *)skb->data;
	cmd->ver_reg_info = le32_encode_bits(HTT_H2T_MSG_TYPE_VERSION_REQ,
					     HTT_OPTION_TAG);

	if (!ath12k_ftm_mode) {
		cmd->tcl_metadata_version = le32_encode_bits(HTT_TAG_TCL_METADATA_VERSION,
							     HTT_OPTION_TAG) |
					    le32_encode_bits(HTT_TCL_METADATA_VER_SZ,
							     HTT_OPTION_LEN) |
					    le32_encode_bits(HTT_OPTION_TCL_METADATA_VER_V2,
							     HTT_OPTION_VALUE);
	}

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		dev_kfree_skb_any(skb);
		return ret;
	}

	ret = wait_for_completion_timeout(&dp->htt_tgt_version_received,
					  HTT_TARGET_VERSION_TIMEOUT_HZ);
	if (ret == 0) {
		ath12k_warn(ab, "htt target version request timed out\n");
		return -ETIMEDOUT;
	}

	if (dp->htt_tgt_ver_major != HTT_TARGET_VERSION_MAJOR) {
		ath12k_err(ab, "unsupported htt major version %d supported version is %d\n",
			   dp->htt_tgt_ver_major, HTT_TARGET_VERSION_MAJOR);
		return -EOPNOTSUPP;
	}

	return 0;
}

int ath12k_dp_tx_htt_h2t_ppdu_stats_req(struct ath12k *ar, u32 mask)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ppdu_stats_cfg_cmd *cmd;
	int len = sizeof(*cmd);
	u8 pdev_mask;
	int ret;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		skb = ath12k_htc_alloc_skb(ab, len);
		if (!skb)
			return -ENOMEM;

		skb_put(skb, len);
		cmd = (struct htt_ppdu_stats_cfg_cmd *)skb->data;
		cmd->msg = le32_encode_bits(HTT_H2T_MSG_TYPE_PPDU_STATS_CFG,
					    HTT_PPDU_STATS_CFG_MSG_TYPE);

		pdev_mask = DP_SW2HW_MACID(ar->pdev_idx) + i;
		cmd->msg |= le32_encode_bits(pdev_mask, HTT_PPDU_STATS_CFG_PDEV_ID);
		cmd->msg |= le32_encode_bits(mask, HTT_PPDU_STATS_CFG_TLV_TYPE_BITMASK);

		ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
		if (ret) {
			dev_kfree_skb_any(skb);
			return ret;
		}
	}

	return 0;
}

static int
ath12k_dp_tx_get_ring_id_type(struct ath12k_base *ab,
			      int mac_id, u32 ring_id,
			      enum hal_ring_type ring_type,
			      enum htt_srng_ring_type *htt_ring_type,
			      enum htt_srng_ring_id *htt_ring_id)
{
	int ret = 0;

	switch (ring_type) {
	case HAL_RXDMA_BUF:
		/* for some targets, host fills rx buffer to fw and fw fills to
		 * rxbuf ring for each rxdma
		 */
		if (!ab->hw_params->rx_mac_buf_ring) {
			if (!(ring_id == HAL_SRNG_SW2RXDMA_BUF0 ||
			      ring_id == HAL_SRNG_SW2RXDMA_BUF1)) {
				ret = -EINVAL;
			}
			*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
			*htt_ring_type = HTT_SW_TO_HW_RING;
		} else {
			if (ring_id == HAL_SRNG_SW2RXDMA_BUF0) {
				*htt_ring_id = HTT_HOST1_TO_FW_RXBUF_RING;
				*htt_ring_type = HTT_SW_TO_SW_RING;
			} else {
				*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
				*htt_ring_type = HTT_SW_TO_HW_RING;
			}
		}
		break;
	case HAL_RXDMA_DST:
		*htt_ring_id = HTT_RXDMA_NON_MONITOR_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_BUF:
		*htt_ring_id = HTT_RX_MON_HOST2MON_BUF_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		*htt_ring_id = HTT_RXDMA_MONITOR_STATUS_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_DST:
		*htt_ring_id = HTT_RX_MON_MON2HOST_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_DESC:
		*htt_ring_id = HTT_RXDMA_MONITOR_DESC_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	default:
		ath12k_warn(ab, "Unsupported ring type in DP :%d\n", ring_type);
		ret = -EINVAL;
	}
	return ret;
}

int ath12k_dp_tx_htt_srng_setup(struct ath12k_base *ab, u32 ring_id,
				int mac_id, enum hal_ring_type ring_type)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_srng_setup_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	u32 ring_entry_sz;
	int len = sizeof(*cmd);
	dma_addr_t hp_addr, tp_addr;
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);

	hp_addr = ath12k_hal_srng_get_hp_addr(ab, srng);
	tp_addr = ath12k_hal_srng_get_tp_addr(ab, srng);

	ret = ath12k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);
	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_srng_setup_cmd *)skb->data;
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_SRING_SETUP,
				      HTT_SRNG_SETUP_CMD_INFO0_MSG_TYPE);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |= le32_encode_bits(DP_SW2HW_MACID(mac_id),
					       HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID);
	else
		cmd->info0 |= le32_encode_bits(mac_id,
					       HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID);
	cmd->info0 |= le32_encode_bits(htt_ring_type,
				       HTT_SRNG_SETUP_CMD_INFO0_RING_TYPE);
	cmd->info0 |= le32_encode_bits(htt_ring_id,
				       HTT_SRNG_SETUP_CMD_INFO0_RING_ID);

	cmd->ring_base_addr_lo = cpu_to_le32(params.ring_base_paddr &
					     HAL_ADDR_LSB_REG_MASK);

	cmd->ring_base_addr_hi = cpu_to_le32((u64)params.ring_base_paddr >>
					     HAL_ADDR_MSB_REG_SHIFT);

	ret = ath12k_hal_srng_get_entrysize(ab, ring_type);
	if (ret < 0)
		goto err_free;

	ring_entry_sz = ret;

	ring_entry_sz >>= 2;
	cmd->info1 = le32_encode_bits(ring_entry_sz,
				      HTT_SRNG_SETUP_CMD_INFO1_RING_ENTRY_SIZE);
	cmd->info1 |= le32_encode_bits(params.num_entries * ring_entry_sz,
				       HTT_SRNG_SETUP_CMD_INFO1_RING_SIZE);
	cmd->info1 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP),
				       HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_MSI_SWAP);
	cmd->info1 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP),
				       HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_TLV_SWAP);
	cmd->info1 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_RING_PTR_SWAP),
				       HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_HOST_FW_SWAP);
	if (htt_ring_type == HTT_SW_TO_HW_RING)
		cmd->info1 |= cpu_to_le32(HTT_SRNG_SETUP_CMD_INFO1_RING_LOOP_CNT_DIS);

	cmd->ring_head_off32_remote_addr_lo = cpu_to_le32(lower_32_bits(hp_addr));
	cmd->ring_head_off32_remote_addr_hi = cpu_to_le32(upper_32_bits(hp_addr));

	cmd->ring_tail_off32_remote_addr_lo = cpu_to_le32(lower_32_bits(tp_addr));
	cmd->ring_tail_off32_remote_addr_hi = cpu_to_le32(upper_32_bits(tp_addr));

	cmd->ring_msi_addr_lo = cpu_to_le32(lower_32_bits(params.msi_addr));
	cmd->ring_msi_addr_hi = cpu_to_le32(upper_32_bits(params.msi_addr));
	cmd->msi_data = cpu_to_le32(params.msi_data);

	cmd->intr_info =
		le32_encode_bits(params.intr_batch_cntr_thres_entries * ring_entry_sz,
				 HTT_SRNG_SETUP_CMD_INTR_INFO_BATCH_COUNTER_THRESH);
	cmd->intr_info |=
		le32_encode_bits(params.intr_timer_thres_us >> 3,
				 HTT_SRNG_SETUP_CMD_INTR_INFO_INTR_TIMER_THRESH);

	cmd->info2 = 0;
	if (params.flags & HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN) {
		cmd->info2 = le32_encode_bits(params.low_threshold,
					      HTT_SRNG_SETUP_CMD_INFO2_INTR_LOW_THRESH);
	}

	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "%s msi_addr_lo:0x%x, msi_addr_hi:0x%x, msi_data:0x%x\n",
		   __func__, cmd->ring_msi_addr_lo, cmd->ring_msi_addr_hi,
		   cmd->msi_data);

	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "ring_id:%d, ring_type:%d, intr_info:0x%x, flags:0x%x\n",
		   ring_id, ring_type, cmd->intr_info, cmd->info2);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_tx_htt_srng_setup);

void ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, ASSOC_REQ,
				      (filter & FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, ASSOC_RESP,
				      (filter & FILTER_MGMT_ASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, REASSOC_REQ,
				      (filter & FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, REASSOC_RESP,
				      (filter & FILTER_MGMT_REASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, PROBE_REQ,
				      (filter & FILTER_MGMT_PROBE_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, PROBE_RESP,
				      (filter & FILTER_MGMT_PROBE_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, PROBE_TIMING_ADV,
				      (filter & FILTER_MGMT_TIM_ADVT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, RESERVED_7,
				      (filter & FILTER_MGMT_RESERVED_7) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, BEACON,
				      (filter & FILTER_MGMT_BEACON) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, ATIM,
				      (filter & FILTER_MGMT_ATIM) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, ASSOC_REQ,
				      (filter & FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, ASSOC_RESP,
				      (filter & FILTER_MGMT_ASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, REASSOC_REQ,
				      (filter & FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, REASSOC_RESP,
				      (filter & FILTER_MGMT_REASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, PROBE_REQ,
				      (filter & FILTER_MGMT_PROBE_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, PROBE_RESP,
				      (filter & FILTER_MGMT_PROBE_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, PROBE_TIMING_ADV,
				      (filter & FILTER_MGMT_TIM_ADVT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, RESERVED_7,
				      (filter & FILTER_MGMT_RESERVED_7) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, BEACON,
				      (filter & FILTER_MGMT_BEACON) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, ATIM,
				      (filter & FILTER_MGMT_ATIM) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, ASSOC_REQ,
				      (filter & FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, ASSOC_RESP,
				      (filter & FILTER_MGMT_ASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, REASSOC_REQ,
				      (filter & FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, REASSOC_RESP,
				      (filter & FILTER_MGMT_REASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, PROBE_REQ,
				      (filter & FILTER_MGMT_PROBE_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, PROBE_RESP,
				      (filter & FILTER_MGMT_PROBE_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, PROBE_TIMING_ADV,
				      (filter & FILTER_MGMT_TIM_ADVT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, RESERVED_7,
				      (filter & FILTER_MGMT_RESERVED_7) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, BEACON,
				      (filter & FILTER_MGMT_BEACON) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, ATIM,
				      (filter & FILTER_MGMT_ATIM) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_mgmt_flag0_filter_set(u32 *ptr,
					  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp)
		ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(ptr,
							     tlv_filter->fp_mgmt_filter);
	if (tlv_filter->enable_mo)
		ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(ptr,
							     tlv_filter->mo_mgmt_filter);
	if (tlv_filter->enable_md)
		ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(ptr,
							     tlv_filter->md_mgmt_filter);
}

void ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, DISASSOC,
				      (filter & FILTER_MGMT_DISASSOC) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, AUTH,
				      (filter & FILTER_MGMT_AUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, DEAUTH,
				      (filter & FILTER_MGMT_DEAUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, ACTION,
				      (filter & FILTER_MGMT_ACTION) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, ACTION_NOACK,
				      (filter & FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, RESERVED_15,
				      (filter & FILTER_MGMT_RESERVED_15) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, DISASSOC,
				      (filter & FILTER_MGMT_DISASSOC) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, AUTH,
				      (filter & FILTER_MGMT_AUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, DEAUTH,
				      (filter & FILTER_MGMT_DEAUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, ACTION,
				      (filter & FILTER_MGMT_ACTION) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, ACTION_NOACK,
				      (filter & FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, RESERVED_15,
				      (filter & FILTER_MGMT_RESERVED_15) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, DISASSOC,
				      (filter & FILTER_MGMT_DISASSOC) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, AUTH,
				      (filter & FILTER_MGMT_AUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, DEAUTH,
				      (filter & FILTER_MGMT_DEAUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, ACTION,
				      (filter & FILTER_MGMT_ACTION) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, ACTION_NOACK,
				      (filter & FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, RESERVED_15,
				      (filter & FILTER_MGMT_RESERVED_15) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_mgmt_flag1_filter_set(u32 *ptr,
					  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp)
		ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(ptr,
							     tlv_filter->fp_mgmt_filter);
	if (tlv_filter->enable_mo)
		ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(ptr,
							     tlv_filter->mo_mgmt_filter);
	if (tlv_filter->enable_md)
		ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(ptr,
							     tlv_filter->md_mgmt_filter);
}

void ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_RESERVED_1,
				      (filter & FILTER_CTRL_RESERVED_1) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_RESERVED_2,
				      (filter & FILTER_CTRL_RESERVED_2) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_TRIGGER,
				      (filter & FILTER_CTRL_TRIGGER) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_RESERVED_4,
				      (filter & FILTER_CTRL_RESERVED_4) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_BF_REP_POLL,
				      (filter & FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_VHT_NDP,
				      (filter & FILTER_CTRL_VHT_NDP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_FRAME_EXT,
				      (filter & FILTER_CTRL_FRAME_EXT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_WRAPPER,
				      (filter & FILTER_CTRL_CTRLWRAP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, BAR,
				      (filter & FILTER_CTRL_BA_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, BA,
				      (filter & FILTER_CTRL_BA) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_RESERVED_1,
				      (filter & FILTER_CTRL_RESERVED_1) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_RESERVED_2,
				      (filter & FILTER_CTRL_RESERVED_2) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_TRIGGER,
				      (filter & FILTER_CTRL_TRIGGER) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_RESERVED_4,
				      (filter & FILTER_CTRL_RESERVED_4) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_BF_REP_POLL,
				      (filter & FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_VHT_NDP,
				      (filter & FILTER_CTRL_VHT_NDP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_FRAME_EXT,
				      (filter & FILTER_CTRL_FRAME_EXT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_WRAPPER,
				      (filter & FILTER_CTRL_CTRLWRAP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, BAR,
				      (filter & FILTER_CTRL_BA_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, BA,
				      (filter & FILTER_CTRL_BA) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_RESERVED_1,
				      (filter & FILTER_CTRL_RESERVED_1) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_RESERVED_2,
				      (filter & FILTER_CTRL_RESERVED_2) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_TRIGGER,
				      (filter & FILTER_CTRL_TRIGGER) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_RESERVED_4,
				      (filter & FILTER_CTRL_RESERVED_4) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_BF_REP_POLL,
				      (filter & FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_VHT_NDP,
				      (filter & FILTER_CTRL_VHT_NDP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_FRAME_EXT,
				      (filter & FILTER_CTRL_FRAME_EXT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_WRAPPER,
				      (filter & FILTER_CTRL_CTRLWRAP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, BAR,
				      (filter & FILTER_CTRL_BA_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, BA,
				      (filter & FILTER_CTRL_BA) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_ctrl_flag2_filter_set(u32 *ptr,
					  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp)
		ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(ptr,
							     tlv_filter->fp_ctrl_filter);
	if (tlv_filter->enable_mo)
		ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(ptr,
							     tlv_filter->mo_ctrl_filter);
	if (tlv_filter->enable_md)
		ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(ptr,
							     tlv_filter->md_ctrl_filter);
}

void ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, PSPOLL,
				      (filter & FILTER_CTRL_PSPOLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, RTS,
				      (filter & FILTER_CTRL_RTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, CTS,
				      (filter & FILTER_CTRL_CTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, ACK,
				      (filter & FILTER_CTRL_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, CFEND,
				      (filter & FILTER_CTRL_CFEND) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, CFEND_ACK,
				      (filter & FILTER_CTRL_CFEND_CFACK) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, DATA, FLASG3, MCAST,
				      (filter & FILTER_DATA_MCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, DATA, FLASG3, UCAST,
				      (filter & FILTER_DATA_UCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, DATA, FLASG3, NULL_DATA,
				      (filter & FILTER_DATA_NULL) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, PSPOLL,
				      (filter & FILTER_CTRL_PSPOLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, RTS,
				      (filter & FILTER_CTRL_RTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, CTS,
				      (filter & FILTER_CTRL_CTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, ACK,
				      (filter & FILTER_CTRL_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, CFEND,
				      (filter & FILTER_CTRL_CFEND) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, CFEND_ACK,
				      (filter & FILTER_CTRL_CFEND_CFACK) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, DATA, FLASG3, MCAST,
				      (filter & FILTER_DATA_MCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, DATA, FLASG3, UCAST,
				      (filter & FILTER_DATA_UCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, DATA, FLASG3, NULL_DATA,
				      (filter & FILTER_DATA_NULL) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, PSPOLL,
				      (filter & FILTER_CTRL_PSPOLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, RTS,
				      (filter & FILTER_CTRL_RTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, CTS,
				      (filter & FILTER_CTRL_CTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, ACK,
				      (filter & FILTER_CTRL_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, CFEND,
				      (filter & FILTER_CTRL_CFEND) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, CFEND_ACK,
				      (filter & FILTER_CTRL_CFEND_CFACK) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, DATA, FLASG3, MCAST,
				      (filter & FILTER_DATA_MCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, DATA, FLASG3, UCAST,
				      (filter & FILTER_DATA_UCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, DATA, FLASG3, NULL_DATA,
				      (filter & FILTER_DATA_NULL) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_ctrl_data_flag3_filter_set(u32 *ptr,
					       struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(ptr,
							     tlv_filter->fp_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(ptr,
							     tlv_filter->fp_data_filter);
	}
	if (tlv_filter->enable_mo) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(ptr,
							     tlv_filter->mo_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(ptr,
							     tlv_filter->mo_data_filter);
	}
	if (tlv_filter->enable_md) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(ptr,
							     tlv_filter->md_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(ptr,
							     tlv_filter->md_data_filter);
	}
}

int ath12k_dp_tx_htt_rx_filter_setup(struct ath12k_base *ab, u32 ring_id,
				     int mac_id, enum hal_ring_type ring_type,
				     int rx_buf_size,
				     struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_selection_cfg_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	int len = sizeof(*cmd);
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;
	u32 word = 0;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);

	ret = ath12k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);
	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_rx_ring_selection_cfg_cmd *)skb->data;
	memset(cmd, 0, len);
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG,
				      HTT_RX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |=
			le32_encode_bits(DP_SW2HW_MACID(mac_id),
					 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID);
	else
		cmd->info0 |=
			le32_encode_bits(mac_id,
					 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID);
	cmd->info0 |= le32_encode_bits(htt_ring_id,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_RING_ID);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP),
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_SS);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP),
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PS);
	cmd->info0 |= le32_encode_bits(tlv_filter->offset_valid,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_OFFSET_VALID);
	cmd->info0 |=
		le32_encode_bits(tlv_filter->drop_threshold_valid,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_DROP_THRES_VAL);
	cmd->info0 |= le32_encode_bits(!tlv_filter->rxmon_disable,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_EN_RXMON);
	cmd->info0 |= le32_encode_bits(!tlv_filter->rxmon_disable,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PKT_TYPE_EN_DATA);

	cmd->info1 = le32_encode_bits(rx_buf_size,
				      HTT_RX_RING_SELECTION_CFG_CMD_INFO1_BUF_SIZE);
	cmd->info1 |= le32_encode_bits(tlv_filter->conf_len_mgmt,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_MGMT);
	cmd->info1 |= le32_encode_bits(tlv_filter->conf_len_ctrl,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_CTRL);
	cmd->info1 |= le32_encode_bits(tlv_filter->conf_len_data,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_DATA);
	if (!tlv_filter->rxmon_disable)
		cmd->info1 |=
			le32_encode_bits(tlv_filter->rx_hdr_len,
					 HTT_RX_RING_SEL_CFG_CMD_INFO1_CONF_HDR_LEN);

	word = 0;
	ath12k_dp_tx_htt_rx_mgmt_flag0_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags0 = cpu_to_le32(word);

	word = 0;
	ath12k_dp_tx_htt_rx_mgmt_flag1_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags1 = cpu_to_le32(word);

	word = 0;
	ath12k_dp_tx_htt_rx_ctrl_flag2_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags2 = cpu_to_le32(word);

	word = 0;
	ath12k_dp_tx_htt_rx_ctrl_data_flag3_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags3 = cpu_to_le32(word);

	cmd->rx_filter_tlv = cpu_to_le32(tlv_filter->rx_filter);

	cmd->info2 = le32_encode_bits(tlv_filter->rx_drop_threshold,
				      HTT_RX_RING_SELECTION_CFG_CMD_INFO2_DROP_THRESHOLD);
	cmd->info2 |=
		le32_encode_bits(tlv_filter->enable_log_mgmt_type,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_LOG_MGMT_TYPE);
	cmd->info2 |=
		le32_encode_bits(tlv_filter->enable_log_ctrl_type,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_CTRL_TYPE);
	cmd->info2 |=
		le32_encode_bits(tlv_filter->enable_log_data_type,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_LOG_DATA_TYPE);

	cmd->info3 =
		le32_encode_bits(tlv_filter->enable_rx_tlv_offset,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO3_EN_TLV_PKT_OFFSET);
	cmd->info3 |=
		le32_encode_bits(tlv_filter->rx_tlv_offset,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO3_PKT_TLV_OFFSET);

	if (tlv_filter->offset_valid) {
		cmd->rx_packet_offset =
			le32_encode_bits(tlv_filter->rx_packet_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_PACKET_OFFSET);

		cmd->rx_packet_offset |=
			le32_encode_bits(tlv_filter->rx_header_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_HEADER_OFFSET);

		cmd->rx_mpdu_offset =
			le32_encode_bits(tlv_filter->rx_mpdu_end_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MPDU_END_OFFSET);

		cmd->rx_mpdu_offset |=
			le32_encode_bits(tlv_filter->rx_mpdu_start_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MPDU_START_OFFSET);

		cmd->rx_msdu_offset =
			le32_encode_bits(tlv_filter->rx_msdu_end_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MSDU_END_OFFSET);

		cmd->rx_msdu_offset |=
			le32_encode_bits(tlv_filter->rx_msdu_start_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MSDU_START_OFFSET);

		cmd->rx_attn_offset =
			le32_encode_bits(tlv_filter->rx_attn_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_ATTENTION_OFFSET);
	}

	if (dp->rx_pktlog_mode == ATH12K_PKTLOG_DISABLED) {
		if (tlv_filter->rx_mpdu_start_wmask > 0 &&
		    tlv_filter->rx_msdu_end_wmask > 0) {
			cmd->info2 |=
				le32_encode_bits(true,
						 HTT_RX_RING_SELECTION_CFG_WORD_MASK_COMPACT_SET);
			cmd->rx_mpdu_start_end_mask =
				le32_encode_bits(tlv_filter->rx_mpdu_start_wmask,
						 HTT_RX_RING_SELECTION_CFG_RX_MPDU_START_MASK);
			/* mpdu_end is not used for any hardwares so far
			 * please assign it in future if any chip is
			 * using through hal ops
			 */
			cmd->rx_mpdu_start_end_mask |=
				le32_encode_bits(tlv_filter->rx_mpdu_end_wmask,
						 HTT_RX_RING_SELECTION_CFG_RX_MPDU_END_MASK);
			cmd->rx_msdu_end_word_mask =
				le32_encode_bits(tlv_filter->rx_msdu_end_wmask,
						 HTT_RX_RING_SELECTION_CFG_RX_MSDU_END_MASK);
		}

		ath12k_dp_mon_rx_config_wmask(dp, cmd, tlv_filter);
	}

	ath12k_dp_mon_rx_config_packet_type_subtype(dp, cmd, tlv_filter);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_tx_htt_rx_filter_setup);

int
ath12k_dp_tx_htt_h2t_ext_stats_req(struct ath12k *ar, u8 type,
				   struct htt_ext_stats_cfg_params *cfg_params,
				   u64 cookie)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ext_stats_cfg_cmd *cmd;
	int len = sizeof(*cmd);
	int ret;
	u32 pdev_id;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct htt_ext_stats_cfg_cmd *)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.msg_type = HTT_H2T_MSG_TYPE_EXT_STATS_CFG;

	pdev_id = ath12k_mac_get_target_pdev_id(ar);
	cmd->hdr.pdev_mask = 1 << pdev_id;

	cmd->hdr.stats_type = type;
	cmd->cfg_param0 = cpu_to_le32(cfg_params->cfg0);
	cmd->cfg_param1 = cpu_to_le32(cfg_params->cfg1);
	cmd->cfg_param2 = cpu_to_le32(cfg_params->cfg2);
	cmd->cfg_param3 = cpu_to_le32(cfg_params->cfg3);
	cmd->cookie_lsb = cpu_to_le32(lower_32_bits(cookie));
	cmd->cookie_msb = cpu_to_le32(upper_32_bits(cookie));

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_warn(ab, "failed to send htt type stats request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_dp_tx_htt_tx_filter_setup(struct ath12k_base *ab, u32 ring_id,
				     int mac_id, enum hal_ring_type ring_type,
				     int tx_buf_size,
				     struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_tx_ring_selection_cfg_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	int len = sizeof(*cmd);
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);

	ret = ath12k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);

	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_tx_ring_selection_cfg_cmd *)skb->data;
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_TX_MONITOR_CFG,
				      HTT_TX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |=
			le32_encode_bits(DP_SW2HW_MACID(mac_id),
					 HTT_TX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID);
	else
		cmd->info0 |=
			le32_encode_bits(mac_id,
					 HTT_TX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID);
	cmd->info0 |= le32_encode_bits(htt_ring_id,
				       HTT_TX_RING_SELECTION_CFG_CMD_INFO0_RING_ID);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP),
				       HTT_TX_RING_SELECTION_CFG_CMD_INFO0_SS);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP),
				       HTT_TX_RING_SELECTION_CFG_CMD_INFO0_PS);

	cmd->info1 |=
		le32_encode_bits(tx_buf_size,
				 HTT_TX_RING_SELECTION_CFG_CMD_INFO1_RING_BUFF_SIZE);

	if (htt_tlv_filter->tx_mon_mgmt_filter) {
		cmd->info1 |=
			le32_encode_bits(HTT_STATS_FRAME_CTRL_TYPE_MGMT,
					 HTT_TX_RING_SELECTION_CFG_CMD_INFO1_PKT_TYPE);
		cmd->info1 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_pkt_dma_len,
				 HTT_TX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_MGMT);
		cmd->info2 |=
		le32_encode_bits(HTT_STATS_FRAME_CTRL_TYPE_MGMT,
				 HTT_TX_RING_SELECTION_CFG_CMD_INFO2_PKT_TYPE_EN_FLAG);
	}

	if (htt_tlv_filter->tx_mon_data_filter) {
		cmd->info1 |=
			le32_encode_bits(HTT_STATS_FRAME_CTRL_TYPE_CTRL,
					 HTT_TX_RING_SELECTION_CFG_CMD_INFO1_PKT_TYPE);
		cmd->info1 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_pkt_dma_len,
				 HTT_TX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_CTRL);
		cmd->info2 |=
		le32_encode_bits(HTT_STATS_FRAME_CTRL_TYPE_CTRL,
				 HTT_TX_RING_SELECTION_CFG_CMD_INFO2_PKT_TYPE_EN_FLAG);
	}

	if (htt_tlv_filter->tx_mon_ctrl_filter) {
		cmd->info1 |=
			le32_encode_bits(HTT_STATS_FRAME_CTRL_TYPE_DATA,
					 HTT_TX_RING_SELECTION_CFG_CMD_INFO1_PKT_TYPE);
		cmd->info1 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_pkt_dma_len,
				 HTT_TX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_DATA);
		cmd->info2 |=
		le32_encode_bits(HTT_STATS_FRAME_CTRL_TYPE_DATA,
				 HTT_TX_RING_SELECTION_CFG_CMD_INFO2_PKT_TYPE_EN_FLAG);
	}

	cmd->tlv_filter_mask_in0 =
		cpu_to_le32(htt_tlv_filter->tx_mon_downstream_tlv_flags);
	cmd->tlv_filter_mask_in1 =
		cpu_to_le32(htt_tlv_filter->tx_mon_upstream_tlv_flags0);
	cmd->tlv_filter_mask_in2 =
		cpu_to_le32(htt_tlv_filter->tx_mon_upstream_tlv_flags1);
	cmd->tlv_filter_mask_in3 =
		cpu_to_le32(htt_tlv_filter->tx_mon_upstream_tlv_flags2);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int
ath12k_dp_htt_rx_flow_fst_setup(struct ath12k_base *ab,
				struct htt_rx_flow_fst_setup *setup_info)
{
	struct sk_buff *skb;
	struct htt_rx_flow_fst_setup_cmd *cmd;
	int ret;
	u32 *key;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_rx_flow_fst_setup_cmd *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_FSE_SETUP_CFG,
				      HTT_DP_RX_FLOW_FST_SETUP_MSG_TYPE);
	cmd->info0 |= le32_encode_bits(0, HTT_DP_RX_FLOW_FST_SETUP_PDEV_ID);

	cmd->info1 = le32_encode_bits(setup_info->max_entries,
				      HTT_DP_RX_FLOW_FST_SETUP_NUM_RECORDS);
	cmd->info1 |= le32_encode_bits(setup_info->max_search,
				       HTT_DP_RX_FLOW_FST_SETUP_MAX_SEARCH);
	cmd->info1 |= le32_encode_bits(setup_info->ip_da_sa_prefix,
				       HTT_DP_RX_FLOW_FST_SETUP_IP_DA_SA);

	cmd->base_addr_lo = setup_info->base_addr_lo;
	cmd->base_addr_hi = setup_info->base_addr_hi;

	key = (u32 *)setup_info->hash_key;
	cmd->toeplitz31_0 = *key++;
	cmd->toeplitz63_32 = *key++;
	cmd->toeplitz95_64 = *key++;
	cmd->toeplitz127_96 = *key++;
	cmd->toeplitz159_128 = *key++;
	cmd->toeplitz191_160 = *key++;
	cmd->toeplitz223_192 = *key++;
	cmd->toeplitz255_224 = *key++;
	cmd->toeplitz287_256 = *key++;
	cmd->info2 = le32_encode_bits(*key, HTT_DP_RX_FLOW_FST_SETUP_TOEPLITZ);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FST setup HTT message:",
			(void *)cmd, len);

	ret = ath12k_htc_send(&ab->htc, ath12k_ab_to_dp(ab)->eid, skb);
	if (ret) {
		ath12k_err(ab, "DP FSE setup msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "DP FSE setup msg sent from host\n");

	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int ath12k_dp_htt_rx_flow_fse_operation(struct ath12k_base *ab,
					enum dp_htt_flow_fst_operation op_code,
					struct hal_flow_tuple_info *tuple_info)
{
	struct sk_buff *skb;
	struct htt_rx_msg_fse_operation *cmd;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_rx_msg_fse_operation *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_FSE_OPERATION_CFG,
				      HTT_H2T_MSG_RX_FSE_MSG_TYPE);
	cmd->info0 |= le32_encode_bits(0, HTT_H2T_MSG_RX_FSE_PDEV_ID);
	cmd->info1 = le32_encode_bits(false, HTT_H2T_MSG_RX_FSE_IPSEC_VALID);

	if (op_code == DP_HTT_FST_CACHE_INVALIDATE_ENTRY) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_CACHE_INVALIDATE_ENTRY,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
		cmd->ip_src_addr_31_0 = htonl(tuple_info->src_ip_31_0);
		cmd->ip_src_addr_63_32 = htonl(tuple_info->src_ip_63_32);
		cmd->ip_src_addr_95_64 = htonl(tuple_info->src_ip_95_64);
		cmd->ip_src_addr_127_96 = htonl(tuple_info->src_ip_127_96);
		cmd->ip_dest_addr_31_0 = htonl(tuple_info->dest_ip_31_0);
		cmd->ip_dest_addr_63_32 = htonl(tuple_info->dest_ip_63_32);
		cmd->ip_dest_addr_95_64 = htonl(tuple_info->dest_ip_95_64);
		cmd->ip_dest_addr_127_96 = htonl(tuple_info->dest_ip_127_96);
		cmd->info2 = le32_encode_bits(tuple_info->src_port,
					      HTT_H2T_MSG_RX_FSE_SRC_PORT);
		cmd->info2 |= le32_encode_bits(tuple_info->dest_port,
					       HTT_H2T_MSG_RX_FSE_DEST_PORT);
		cmd->info3 = le32_encode_bits(tuple_info->l4_protocol,
					      HTT_H2T_MSG_RX_FSE_L4_PROTO);
	} else if (op_code == DP_HTT_FST_CACHE_INVALIDATE_FULL) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_CACHE_INVALIDATE_FULL,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
	} else if (op_code == DP_HTT_FST_DISABLE) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_DISABLE,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
	} else if (op_code == DP_HTT_FST_ENABLE) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_ENABLE,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
	}

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FSE HTT message:",
			(void *)cmd, len);

	ret = ath12k_htc_send(&ab->htc, ath12k_ab_to_dp(ab)->eid, skb);
	if (ret) {
		ath12k_warn(ab, "DP FSE operation msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "DP FSE operation msg sent from host\n");
	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int ath12k_dp_htt_rx_fse_3_tuple_config_send(struct ath12k_base *ab,
					     u32 tuple_mask, u8 pdev_id)
{
	struct sk_buff *skb;
	struct htt_h2t_msg_rx_3_tuple_hash_cfg *cmd;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_h2t_msg_rx_3_tuple_hash_cfg *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_FSE_3_TUPLE_HASH_CFG,
				      HTT_H2T_MSG_RX_FSE_3_TUPLE_MSG_TYPE);
	cmd->info0 |= le32_encode_bits(pdev_id, HTT_H2T_MSG_RX_FSE_PDEV_ID);
	cmd->info1 |= le32_encode_bits(tuple_mask,
				       HTT_H2T_FLOW_CLASSIFY_3_TUPLE_FIELD_CONFIG);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FSE 3 TUPLE ENABLE HTT message:",
			(void *)cmd, len);

	ret = ath12k_htc_send(&ab->htc, ath12k_ab_to_dp(ab)->eid, skb);
	if (ret) {
		ath12k_err(ab, "DP FSE 3 TUPLE enable msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "DP FSE 3 TUPLE enable msg sent from host\n");

	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

