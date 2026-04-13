// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_tx.h"
#include "../peer.h"
#include "dp_tx.h"
#include "hal_rx.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../sdwf.h"
#include "../dp_stats.h"
#include "../dp_peer.h"
#include "../telemetry.h"
#include "../telemetry_agent_if.h"

struct ath12k_tx_sw_metadata {
	struct sk_buff *skb;
	struct sk_buff *skb_ext_desc;
	u64 paddr : 40,
	    len   : 16,
	    mac_id: 5,
	    flags : 3;
	u64 paddr_ext_desc : 40,
	    ext_desc_len   : 16,
	    rsvd2          : 8;
} __packed __aligned(32);

static_assert(sizeof(struct ath12k_tx_sw_metadata) == 32, "size of struct ath12k_tx_sw_metadata is not 64 bytes!");

struct ath12k_wifi7_tx_status_entry {
	struct hal_wbm_completion_ring_tx tx_status;
	union {
		struct ath12k_tx_sw_metadata sw_metadata;
		void *tx_desc;
	};
} __packed;

static_assert(sizeof(struct ath12k_wifi7_tx_status_entry) == 64, "size of struct ath12k_wifi7_tx_status_entry is not 64 bytes!");

static enum hal_tcl_encap_type
ath12k_dp_tx_get_encap_type(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);

	if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags))
		return HAL_TCL_ENCAP_TYPE_RAW;

	if (tx_info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
		return HAL_TCL_ENCAP_TYPE_ETHERNET;

	return HAL_TCL_ENCAP_TYPE_NATIVE_WIFI;
}

static void
ath12k_wifi7_hal_tx_cmd_ext_desc_setup(struct ath12k_base *ab,
				       struct hal_tx_msdu_ext_desc *tcl_ext_cmd,
				       struct hal_tx_info *ti)
{
	tcl_ext_cmd->info0 = le32_encode_bits(ti->paddr,
					      HAL_TX_MSDU_EXT_INFO0_BUF_PTR_LO);
	tcl_ext_cmd->info1 = le32_encode_bits(0x0,
					      HAL_TX_MSDU_EXT_INFO1_BUF_PTR_HI) |
			       le32_encode_bits(ti->data_len,
						HAL_TX_MSDU_EXT_INFO1_BUF_LEN);

	tcl_ext_cmd->info1 |= le32_encode_bits(1, HAL_TX_MSDU_EXT_INFO1_EXTN_OVERRIDE) |
				le32_encode_bits(ti->encap_type,
						 HAL_TX_MSDU_EXT_INFO1_ENCAP_TYPE) |
				le32_encode_bits(ti->encrypt_type,
						 HAL_TX_MSDU_EXT_INFO1_ENCRYPT_TYPE);
}

static inline u32 ath12k_qos_get_metadata(u16 qos_id)
{
	u32 tcl_metadata = 0;

	tcl_metadata = u32_encode_bits(HTT_TCL_META_DATA_TYPE_SVC_ID_BASED,
				       HTT_TCL_META_DATA_TYPE_MISSION) |
			u32_encode_bits(1, HTT_TCL_META_DATA_SAWF_TID_OVERRIDE) |
			u32_encode_bits(qos_id, HTT_TCL_META_DATA_SAWF_SVC_ID);
	return tcl_metadata;
}

static inline u32 ath12k_qos_get_tcl_cmd(u32 msduq)
{
	u32 tid, flow_override, who_classify_info_sel, update = 0;

	tid = u32_get_bits(msduq, MSDUQ_TID);
	flow_override = u32_get_bits(msduq, MSDUQ_FLOW_OVERRIDE);
	who_classify_info_sel = u32_get_bits(msduq, MSDUQ_WHO_CL_INFO);

	update = u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO3_TID) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_TID_OVERWRITE) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_FLOW_OVERRIDE_EN) |
		 u32_encode_bits(who_classify_info_sel,
				 HAL_TCL_DATA_CMD_INFO3_CLASSIFY_INFO_SEL) |
		 u32_encode_bits(flow_override,
				 HAL_TCL_DATA_CMD_INFO3_FLOW_OVERRIDE);
	return update;
}

static inline
void ath12k_wifi_qos_desc(struct hal_tcl_data_cmd *desc,
			  u32 msduq, u16 qos_id)
{
	u32 meta_data_flags;

	desc->info3 |= ath12k_qos_get_tcl_cmd(msduq);
	meta_data_flags = ath12k_qos_get_metadata(qos_id);
	desc->info1 = u32_encode_bits(meta_data_flags,
				      HAL_TCL_DATA_CMD_INFO1_CMD_NUM);
}

static inline
void ath12k_wifi_qos_hlos_tid(struct hal_tcl_data_cmd *desc,
			      u8 tid)
{
	desc->info3 |= u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO3_TID) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_TID_OVERWRITE);
}

static inline u8 ath12k_get_qos_tag(u32 mark)
{
	u8 qos_tag = u32_get_bits(mark, QOS_TAG_MASK);

	if (qos_tag == QOS_SCS_TAG || qos_tag == QOS_MSCS_TAG)
		return qos_tag;

	return 0;
}

static inline void
ath12k_dp_qos_update(struct ath12k_dp *dp, struct ath12k_pdev_dp *dp_pdev,
		     u32 mark, struct hal_tcl_data_cmd *desc, u8 qos_tag,
		     u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_peer *dp_peer;
	u8 scs_id;
	u16 msduq, peer_id;
	u16 qos_id = QOS_ID_MAX;
	int ret;

	if (mark & SDWF_VALID_MASK) {
		rcu_read_lock();
		peer_id = u32_get_bits(mark, SDWF_PEER_ID);
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      peer_id);
		if (!dp_peer) {
			rcu_read_unlock();
			return;
		}
		msduq = u32_get_bits(mark, SDWF_MSDUQ_ID);
		if (msduq >= MSDUQ_MAX_DEF)
			qos_id = dp_peer_msduq_qos_id(dp->ab, dp_peer->qos,
						      msduq);
		rcu_read_unlock();
	} else if (qos_tag == QOS_SCS_TAG) {
		scs_id = u32_get_bits(mark, QOS_QOS_ID_MASK);

		spin_lock_bh(&dp->dp_lock);
		peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
		if (!peer) {
			spin_unlock_bh(&dp->dp_lock);
			return;
		}
		ret = ath12k_dp_peer_scs_data(dp, peer->dp_peer->qos,
					      scs_id, &msduq, &qos_id);
		spin_unlock_bh(&dp->dp_lock);

		if (ret != 0) {
			ath12k_err(dp->ab, "SCS Peer Data is NULL");
			return;
		}
	} else {
		msduq = u32_get_bits(mark, QOS_QOS_ID_MASK);
	}
	/* Update Desc for HLOS TID Override */
	if (msduq < MSDUQ_MAX_DEF) {
		ath12k_wifi_qos_hlos_tid(desc, msduq);
	} else if (msduq < QOS_MSDUQ_MAX){
	/* Update Desc for User Defined QoS MSDUQ */
			ath12k_wifi_qos_desc(desc, msduq, qos_id);
	}
}

static void ath12k_qos_tx_enqueue_peer_stats(struct ath12k_dp_link_peer_stats *peer_stats,
					     u16 msduq_id,
					     unsigned int len)
{
	struct tx_stats *qos_tx;
	u8 tid, q_id;

	if (!peer_stats->qos_stats) {
		ath12k_err(NULL, "Qos stats not initialized\n");
		return;
	}

	if (unlikely(msduq_id >= QOS_MSDUQ_MAX &&
		     msduq_id < MSDUQ_MAX_DEF))
		return;

	msduq_id -= MSDUQ_MAX_DEF;

	q_id = u16_get_bits(msduq_id, MSDUQ_MASK);
	tid = u16_get_bits(msduq_id, MSDUQ_TID_MASK);

	qos_tx = &peer_stats->qos_stats->qos_tx[tid][q_id];

	qos_tx->queue_depth++;
	qos_tx->tx_ingress.num++;
	qos_tx->tx_ingress.bytes += len;
}

static void
ath12k_dp_sdwftx_ingress_stats_update(struct ath12k_link_vif *arvif,
				      u32 *skb_mark, u32 qos_nw_delay,
				      unsigned int skb_len)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_dp *dp;
	struct ath12k_dp_link_peer *pri_peer;
	u16 msduq, peer_id, qos_id;

	if (!ar)
		return;

	if (!(*skb_mark & SDWF_VALID_MASK))
		return;

	msduq = u32_get_bits(*skb_mark, SDWF_MSDUQ_ID);

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    (ath12k_debugfs_is_qos_stats_enabled(ar) &
	    ATH12K_QOS_STATS_BASIC)) {
		peer_id = u32_get_bits(*skb_mark, SDWF_PEER_ID);

		if (!ar->dp.dp)
			return;

		dp = ar->dp.dp;

		spin_lock_bh(&dp->dp_lock);

		pri_peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
		if (!pri_peer || !pri_peer->dp_peer || !pri_peer->dp_peer->qos) {
			spin_unlock_bh(&dp->dp_lock);
			return;
		}

		qos_id = dp_peer_msduq_qos_id(ar->ab, pri_peer->dp_peer->qos,
					      msduq);
		if (qos_id == QOS_ID_INVALID) {
			ath12k_err(ar->ab, "msduq_id: %u not yet reserved\n",
				   msduq);
			spin_unlock_bh(&dp->dp_lock);
			return;
		}

		ath12k_qos_tx_enqueue_peer_stats(&pri_peer->peer_stats,
						 msduq, skb_len);
		spin_unlock_bh(&dp->dp_lock);
	}

	/* Store the NWDELAY to skb->mark which can be fetched
	 * during tx completion
	 */
	if (qos_nw_delay > QOS_NW_DELAY_MAX)
		qos_nw_delay = QOS_NW_DELAY_MAX;

	*skb_mark = u32_encode_bits((u32_get_bits(*skb_mark, QOS_NW_TAG_SHIFT)), QOS_TAG_ID) | (qos_nw_delay << QOS_NW_DELAY_SHIFT) | msduq;
}

void ath12k_sdwf_update_peer_mcs_stats(struct tx_stats *qos_tx,
				       struct hal_tx_status *ts)
{
	u8 mcs = MAX_MCS, pkt_type;

	mcs = ts->mcs;
	pkt_type = ts->pkt_type;

	if (pkt_type > HAL_TX_RATE_STATS_PKT_TYPE_11BE ||
	    pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BA) {
		return;
	}

	if (pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BE)
		pkt_type = DOT11_BE;

	switch (pkt_type) {
	case DOT11_A:
		mcs = (mcs >= MAX_MCS_11A) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_B:
		mcs = (mcs >= MAX_MCS_11B) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_N:
		mcs = (mcs >= MAX_MCS_11N) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_AC:
		mcs = (mcs >= MAX_MCS_11AC) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_AX:
		mcs = (mcs >= MAX_MCS_11AX) ? (MAX_MCS - 1) : mcs;
		break;
	case DOT11_BE:
		mcs = (mcs >= MAX_MCS_11BE) ? (MAX_MCS - 1) : mcs;
		break;
	default:
		break;
	}

	if (mcs != MAX_MCS)
		qos_tx->pkt_type[pkt_type].mcs_count[mcs]++;
}

bool ath12k_get_qos_params_delay_bound(struct ath12k_base *ab, u8 qos_id,
				       u32 *delay_bound)
{
	struct ath12k_qos_ctx *qos_ctx;

	if (qos_id >= QOS_PROFILES_MAX) {
		ath12k_err(NULL, "Invalid qos id :%u\n", qos_id);
		return false;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(NULL, "QoS Context is NULL\n");
		return false;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	if (!qos_ctx->profiles[qos_id].ref_count) {
		ath12k_err(NULL, "Qos ctx : %u profiles not present\n",
			   qos_id);
		spin_unlock_bh(&qos_ctx->profile_lock);
		return false;
	}

	*delay_bound = qos_ctx->profiles[qos_id].params.msdu_delivery_info;
	spin_unlock_bh(&qos_ctx->profile_lock);

	return true;
}

#define HW_TX_DELAY_MAX				0x1000000

#define TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US	10
#define HW_TX_DELAY_MASK			0x1FFFFFFF
#define TX_COMPL_BUFFER_TSTAMP_US(TSTAMP) \
	(((TSTAMP) << TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US) & \
	 HW_TX_DELAY_MASK)

#define ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER 1000
#define ATH12K_MOV_AVG_PKT_WIN	10

#define ATH12K_HIST_AVG_DIV	2

const u32 ath12k_hist_hw_tx_comp_bucket[] = {
	250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000, U32_MAX
};

void ath12k_hist_fill_buckets(struct hist_bucket *hist, u32 value)
{
	int idx;

	hist->hist_type = HIST_TYPE_HW_TX_COMP_DELAY;

	for (idx = HIST_BUCKET_0; idx < ARRAY_SIZE(ath12k_hist_hw_tx_comp_bucket); idx++) {
		if (value <= ath12k_hist_hw_tx_comp_bucket[idx]) {
			hist->freq[idx]++;
			return;
		}
	}
}

void ath12k_update_hist_stats(struct hist_stats *hist_stats, u32 value)
{
	if (!hist_stats)
		return;

	ath12k_hist_fill_buckets(&hist_stats->hist, value);

	if (!hist_stats->min || value < hist_stats->min)
		hist_stats->min = value;

	if (value > hist_stats->max)
		hist_stats->max = value;

	if (unlikely(!hist_stats->avg))
		hist_stats->avg = value;
	else
		hist_stats->avg = (hist_stats->avg + value) / ATH12K_HIST_AVG_DIV;
}

void ath12k_sdwf_compute_hw_delay(struct ath12k *ar, struct hal_tx_status *ts,
				  u32 *hw_delay)
{
	/* low 32 alone will be filled for TSF2 from FW and the value can be
	 * negative for both TSF2 and TQM delta
	 */
	int tmp_delta_tsf2 = ar->delta_tsf2, tmp_delta_tqm = ar->delta_tqm;
	u32 msdu_tqm_enqueue_tstamp_us, final_msdu_tqm_enqueue_tstamp_us;
	u32 msdu_compl_tsf_tstamp_us, final_msdu_compl_tsf_tstamp_us;
	struct ath12k_hw_group *ag = ar->ab->ag;
	/* MLO TSTAMP OFFSET can be negative
	 */
	int mlo_offset = ag->mlo_tstamp_offset;
	int delta_tsf2, delta_tqm;

	msdu_tqm_enqueue_tstamp_us =
		TX_COMPL_BUFFER_TSTAMP_US(ts->buffer_timestamp);
	msdu_compl_tsf_tstamp_us = ts->tsf;
	delta_tsf2 = mlo_offset - tmp_delta_tsf2;
	delta_tqm = mlo_offset - tmp_delta_tqm;

	final_msdu_tqm_enqueue_tstamp_us =
		(msdu_tqm_enqueue_tstamp_us + delta_tqm) & HW_TX_DELAY_MASK;
	final_msdu_compl_tsf_tstamp_us =
		(msdu_compl_tsf_tstamp_us + delta_tsf2) & HW_TX_DELAY_MASK;

	*hw_delay = (final_msdu_compl_tsf_tstamp_us -
			final_msdu_tqm_enqueue_tstamp_us) & HW_TX_DELAY_MASK;
}

/* reinject_pkt stats - needs to be implemented */
void ath12k_qos_stats_update(struct ath12k *ar, struct sk_buff *skb,
			     struct hal_tx_status *ts,
			     struct ath12k_pdev_dp *dp_pdev,
			     ktime_t timestamp)
{
	struct ath12k_dp_link_peer *link_peer = NULL, *pri_peer = NULL;
	struct ath12k_dp_peer *mld_peer;
	struct ath12k_dp *dp = NULL;
	struct ath12k_dp_hw *dp_hw = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_mld_qos_stats *mld_qos;
	struct tx_stats *qos_tx;
	struct delay_stats *qos_delay;
	void *telemetry_peer_ctx = NULL;
	u64 enqueue_timestamp, total_delay_pkts, tmp_div;
	u32 len, q_id, tid, hw_delay, nw_delay, sw_delay, delay_bound;
	u32 pkt_win, num_pkts, dropped_age_out = 0;
	u16 msduq_id;
	u8 link_id, pri_link_id, qos_id;
	bool update_pri_peer = false;

	if (!ts || !dp_pdev)
		return;

	if (!(skb->mark & QOS_VALID_TAG))
		return;

	msduq_id = u32_get_bits(skb->mark, SDWF_MSDUQ_ID);
	if (msduq_id >= QOS_MSDUQ_MAX &&
	    msduq_id < MSDUQ_MAX_DEF)
		return;

	msduq_id -= MSDUQ_MAX_DEF;

	q_id = u16_get_bits(msduq_id, MSDUQ_MASK);
	tid = u16_get_bits(msduq_id, MSDUQ_TID_MASK);
	len = skb->len;

	if (!(ath12k_debugfs_is_qos_stats_enabled(ar) & ATH12K_QOS_STATS_BASIC))
		return;

	mld_peer = ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp,
						       dp_pdev,
						       ts->peer_id);
	if (!mld_peer) {
		ath12k_err(ar->ab, "MLD peer NA with peer_id: %u\n",
			   ts->peer_id);
		return;
	}

	if (mld_peer->qos_stats_lvl == ATH12K_QOS_SINGLE_LINK_STATS) {
		/* primary link only */
		link_id = mld_peer->hw_links[dp_pdev->hw_link_id];
	} else {
		link_id = ath12k_dp_get_link_id(dp_pdev, ts, mld_peer);
	}

	if (link_id < ATH12K_NUM_MAX_LINKS) {
		link_peer = rcu_dereference(mld_peer->link_peers[link_id]);
		if (!link_peer) {
			ath12k_err(ar->ab, "link peer not present with link_id: %u\n",
				   link_id);
			return;
		}

		if (link_peer->vif) {
			ahvif = ath12k_vif_to_ahvif(link_peer->vif);
		} else {
			ath12k_err(ar->ab, "vif not present with link_id: %u\n",
				   link_id);
			return;
		}

		if (ahvif) {
			arvif = rcu_dereference(ahvif->link[link_id]);
		} else {
			ath12k_err(ar->ab, "ath12k vif not present with link_id: %u\n",
				   link_id);
			return;
		}

		if (arvif && arvif->ar) {
			dp = arvif->ar->ab->dp;
			dp_hw = arvif->ar->dp.dp_hw;
		} else {
			ath12k_err(ar->ab, "link vif or link vif radio not present with link_id: %u\n",
				   link_id);
			return;
		}

		if (!dp || !dp_hw) {
			ath12k_err(ar->ab, "dp or dp_hw not present %u\n",
				   link_id);
			return;
		}
	} else {
		ath12k_err(ar->ab, "link peer NA with link_id: %u\n",
			   link_id);
		return;
	}

	spin_lock_bh(&dp->dp_lock);
	spin_lock_bh(&dp_hw->peer_lock);

	mld_qos = &mld_peer->mld_qos_stats[tid][q_id];

	if (!link_peer->peer_stats.qos_stats) {
		spin_unlock_bh(&dp_hw->peer_lock);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	qos_tx = &link_peer->peer_stats.qos_stats->qos_tx[tid][q_id];

	switch (ts->status) {
	case HAL_WBM_TQM_REL_REASON_FRAME_ACKED:
		mld_qos->tx_success_pkts++;
		qos_tx->tx_success.num++;
		qos_tx->tx_success.bytes += len;
		if (ts->transmit_cnt > 1) {
			qos_tx->total_retries_count += (ts->transmit_cnt - 1);
			qos_tx->retry_count++;
			if (ts->transmit_cnt > 2)
				qos_tx->multiple_retry_count++;
		}
		ath12k_sdwf_update_peer_mcs_stats(qos_tx, ts);
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
		qos_tx->dropped.fw_rem.num++;
		qos_tx->dropped.fw_rem.bytes += len;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
		qos_tx->dropped.fw_rem_tx++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX:
		qos_tx->dropped.fw_rem_notx++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
		qos_tx->dropped.age_out++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1:
		qos_tx->dropped.fw_reason1++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2:
		qos_tx->dropped.fw_reason2++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3:
		qos_tx->dropped.fw_reason3++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_DISABLE_QUEUE:
		qos_tx->dropped.fw_rem_queue_disable++;
		break;
	case HAL_WBM_TQM_REL_REASON_CMD_TILL_NONMATCHING:
		qos_tx->dropped.fw_rem_no_match++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
		qos_tx->dropped.drop_threshold++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_LINK_DESC_UNAVAIL:
		qos_tx->dropped.drop_link_desc_na++;
		break;
	case HAL_WBM_TQM_REL_REASON_DROP_OR_INVALID_MSDU:
		qos_tx->dropped.invalid_drop++;
		break;
	case HAL_WBM_TQM_REL_REASON_MULTICAST_DROP:
		qos_tx->dropped.mcast_vdev_drop++;
		break;
	default:
		qos_tx->dropped.invalid_rr++;
		break;
	}

	if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		qos_tx->tx_failed.num++;
		qos_tx->tx_failed.bytes += len;
		mld_qos->tx_failed_pkts++;
		if (ts->transmit_cnt > DP_RETRY_COUNT)
			qos_tx->failed_retry_count++;
	}

	pri_link_id = mld_peer->hw_links[dp_pdev->hw_link_id];

	if (link_id == pri_link_id) {
		qos_tx->queue_depth--;
	} else {
		update_pri_peer = true;
	}

	ath12k_telemetry_get_sla_num_pkts(&num_pkts);
	if (mld_peer->qos)
		telemetry_peer_ctx = mld_peer->qos->telemetry_peer_ctx;

	tmp_div = mld_qos->tx_success_pkts + mld_qos->tx_failed_pkts;
	if ((!(do_div(tmp_div, num_pkts))) &&
	    telemetry_peer_ctx) {
		if (mld_peer->qos_stats_lvl ==
		    ATH12K_QOS_SINGLE_LINK_STATS) {
			dropped_age_out = qos_tx->dropped.age_out;
		} else {
			struct ath12k_dp_link_peer *tmp_peer = NULL;
			struct tx_stats *tmp_qos_tx = NULL;
			unsigned long peer_links_map, scan_links_map;
			u8 tmp_link_id;

			peer_links_map = mld_peer->peer_links_map;
			scan_links_map = ATH12K_SCAN_LINKS_MASK;

			for_each_andnot_bit(tmp_link_id, &peer_links_map,
					    &scan_links_map,
					    ATH12K_NUM_MAX_LINKS) {
				tmp_peer = rcu_dereference(mld_peer->link_peers[tmp_link_id]);
				if (!tmp_peer ||
				    !tmp_peer->peer_stats.qos_stats) {
					continue;
				}

				tmp_qos_tx = &tmp_peer->peer_stats.qos_stats->qos_tx[tid][q_id];
				dropped_age_out += tmp_qos_tx->dropped.age_out;
				tmp_peer = NULL;
			}
		}
		ath12k_telemetry_update_msdu_drop(telemetry_peer_ctx, tid, msduq_id,
						  mld_qos->tx_success_pkts,
						  mld_qos->tx_failed_pkts,
						  dropped_age_out);
	}

	qos_delay = &link_peer->peer_stats.qos_stats->qos_delay[tid][q_id];

	ath12k_sdwf_compute_hw_delay(ar, ts, &hw_delay);
	if (hw_delay > HW_TX_DELAY_MAX) {
		mld_qos->tx_invalid_delay_pkts++;
		qos_delay->invalid_delay_pkts++;
		goto out;
	}

	mld_qos->hwdelay_win_total += hw_delay;
	ath12k_update_hist_stats(&qos_delay->delay_hist, hw_delay);

	nw_delay = u32_get_bits(skb->mark, QOS_NW_DELAY);
	mld_qos->nwdelay_win_total += nw_delay;

	enqueue_timestamp = ktime_to_us(timestamp);

	if (!enqueue_timestamp)
		sw_delay = 0;
	else
		sw_delay = (u32) (enqueue_timestamp);

	mld_qos->swdelay_win_total += sw_delay;

	ath12k_telemetry_get_sla_mov_avg_num_pkt(&pkt_win);
	if (!pkt_win)
		pkt_win = ATH12K_MOV_AVG_PKT_WIN;

	total_delay_pkts = mld_qos->tx_success_pkts +
			   mld_qos->tx_failed_pkts -
			   mld_qos->tx_invalid_delay_pkts;
	tmp_div = total_delay_pkts;

	if (telemetry_peer_ctx && !(do_div(tmp_div, pkt_win))) {
		u32 nwdelay_avg, hwdelay_avg, swdelay_avg;
		nwdelay_avg = div_u64(mld_qos->nwdelay_win_total,
				      pkt_win);
		swdelay_avg = div_u64(mld_qos->swdelay_win_total,
				      pkt_win);
		hwdelay_avg = div_u64(mld_qos->hwdelay_win_total,
				      pkt_win);
		mld_qos->nwdelay_win_total = 0;
		mld_qos->swdelay_win_total = 0;
		mld_qos->hwdelay_win_total = 0;

		ath12k_telemetry_update_delay_mvng(telemetry_peer_ctx,
						   tid, msduq_id,
						   nwdelay_avg,
						   swdelay_avg,
						   hwdelay_avg);
	}

	if (!mld_peer->qos) {
		ath12k_err(ar->ab, "link peer's qos not present\n");
		goto out;
	}

	qos_id = mld_peer->qos->msduq_map[tid][q_id].qos_id;

	if (ath12k_get_qos_params_delay_bound(arvif->ar->ab, qos_id,
					      &delay_bound)) {
		if (hw_delay > (delay_bound *
				ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER))
			qos_delay->delay_failure++;
		else
			qos_delay->delay_success++;

		tmp_div = total_delay_pkts;
		if (!(do_div(tmp_div, num_pkts)) && telemetry_peer_ctx) {
			u64 delay_success = 0, delay_failure = 0;

			if (mld_peer->qos_stats_lvl ==
			    ATH12K_QOS_SINGLE_LINK_STATS) {
				delay_success = qos_delay->delay_success;
				delay_failure = qos_delay->delay_failure;
			} else {
				struct ath12k_dp_link_peer *tmp_peer = NULL;
				struct delay_stats *tmp_qos_delay = NULL;
				unsigned long peer_links_map, scan_links_map;
				u8 tmp_link_id;

				peer_links_map = mld_peer->peer_links_map;
				scan_links_map = ATH12K_SCAN_LINKS_MASK;

				for_each_andnot_bit(tmp_link_id, &peer_links_map,
						    &scan_links_map,
						    ATH12K_NUM_MAX_LINKS) {
					tmp_peer = rcu_dereference(mld_peer->link_peers[tmp_link_id]);
					if (!tmp_peer ||
					    !tmp_peer->peer_stats.qos_stats) {
						continue;
					}

					tmp_qos_delay = &tmp_peer->peer_stats.qos_stats->qos_delay[tid][q_id];
					delay_success += tmp_qos_delay->delay_success;
					delay_failure += tmp_qos_delay->delay_failure;
					tmp_peer = NULL;
				}
			}
			ath12k_telemetry_update_delay(telemetry_peer_ctx,
						      tid, msduq_id,
						      delay_success,
						      delay_failure);
		}
	}

out:
	spin_unlock_bh(&dp_hw->peer_lock);
	spin_unlock_bh(&dp->dp_lock);

	if (update_pri_peer) {
		 pri_peer = rcu_dereference(mld_peer->link_peers[pri_link_id]);
		 if (pri_peer) {
			spin_lock_bh(&dp_pdev->dp->dp_lock);
			if (pri_peer->peer_stats.qos_stats)
				pri_peer->peer_stats.qos_stats->qos_tx[tid][q_id].queue_depth--;
			spin_unlock_bh(&dp_pdev->dp->dp_lock);
		}
	}
}

#define HTT_META_DATA_ALIGNMENT 0x8

/* Preparing HTT Metadata when utilized with ext MSDU */
static int ath12k_wifi7_dp_prepare_htt_metadata(struct sk_buff *skb)
{
	struct hal_tx_msdu_metadata *desc_ext;
	u8 htt_desc_size;
	/* Size rounded of multiple of 8 bytes */
	u8 htt_desc_size_aligned;

	htt_desc_size = sizeof(struct hal_tx_msdu_metadata);
	htt_desc_size_aligned = ALIGN(htt_desc_size, HTT_META_DATA_ALIGNMENT);

	desc_ext = ath12k_dp_metadata_align_skb(skb, htt_desc_size_aligned);
	if (!desc_ext)
		return -ENOMEM;

	desc_ext->info0 = le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_FLAG) |
			  le32_encode_bits(0, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_TYPE) |
			  le32_encode_bits(1,
					   HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL);

	return 0;
}

bool ath12k_mac_tx_check_max_limit(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *skb)
{
	if (atomic_read(&dp_pdev->num_tx_pending) > ATH12K_DP_PDEV_TX_LIMIT) {
		/* Allow EAPOL */
		if (!(skb->protocol == cpu_to_be16(ETH_P_PAE))) {
			dp_pdev->dp->device_stats.tx_err.threshold_limit++;
			return true;
		}
	}

	return false;
}

static inline void
ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end) {
#ifndef CONFIG_IO_COHERENCY
        dmac_clean_range_no_dsb(start, end);
#endif
}

#ifdef CONFIG_IO_COHERENCY
static inline void
ath12k_wifi7_dp_tx_populate_tcl_desc(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_dp_link_vif *dp_link_vif,
				     struct sk_buff *skb,
				     struct hal_tcl_data_cmd *hal_tcl_desc,
				     struct ath12k_tx_desc_info *tx_desc,
				     u32 qos_nw_delay)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;

	hal_tcl_desc->buf_addr_info.info0 = (u32)virt_to_phys(skb->data);
	hal_tcl_desc->buf_addr_info.info1 =
				(((u64)virt_to_phys(skb->data) >> 32) |
				(tx_desc->desc_id << 12));
	hal_tcl_desc->info0 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BANK_ID,
					 dp_link_vif->bank_id);
	hal_tcl_desc->info1 =  FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_CMD_NUM,
					  dp_link_vif->tcl_metadata);
	hal_tcl_desc->info2 =  skb->len;
	hal_tcl_desc->info3 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_PMAC_ID,
					 dp_link_vif->lmac_id) |
			      FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_VDEV_ID,
					 dp_link_vif->vdev_id);
	hal_tcl_desc->info4 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO4_SEARCH_INDEX,
					 dp_link_vif->ast_idx) |
			      FIELD_PREP(HAL_TCL_DATA_CMD_INFO4_CACHE_SET_NUM,
					 dp_link_vif->ast_hash);
	hal_tcl_desc->info5 = 0;

	/**
	 * Check if the vif supports mscs hlos tid override, which
	 * will be true if there is an active MSCS session
	 * In this case, check for skb->priority which would have
	 * the correct tid value and program it to the TCL metadata
	 * For accelerated packets with MSCS, skb->mark will not be
	 * set
	 */
	if (unlikely(skb->priority &&
		     dp_vif->mscs_hlos_tid_override))
		ath12k_wifi_qos_hlos_tid(hal_tcl_desc, skb->priority);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
				     0, NULL);
		ath12k_dp_sdwftx_ingress_stats_update(arvif, &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}
}
#else
static inline void
ath12k_wifi7_dp_tx_populate_tcl_desc(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_dp_link_vif *dp_link_vif,
				     struct sk_buff *skb,
				     struct hal_tcl_data_cmd *hal_tcl_desc,
				     struct ath12k_tx_desc_info *tx_desc,
				     u32 qos_nw_delay)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct hal_tcl_data_cmd tcl_desc = {0};

	tcl_desc.buf_addr_info.info0 = (u32)virt_to_phys(skb->data);
	tcl_desc.buf_addr_info.info1 = (((u64)virt_to_phys(skb->data) >> 32) |
				       (tx_desc->desc_id << 12));
	tcl_desc.info0 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BANK_ID,
				    dp_link_vif->bank_id);
	tcl_desc.info1 =  FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_CMD_NUM,
				     dp_link_vif->tcl_metadata);
	tcl_desc.info2 =  skb->len;
	tcl_desc.info3 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_PMAC_ID,
				    dp_link_vif->lmac_id) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_VDEV_ID,
				    dp_link_vif->vdev_id);
	tcl_desc.info4 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO4_SEARCH_INDEX,
				    dp_link_vif->ast_idx) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO4_CACHE_SET_NUM,
				    dp_link_vif->ast_hash);
	tcl_desc.info5 = 0;

	/**
	 * Check if the vif supports mscs hlos tid override, which
	 * will be true if there is an active MSCS session
	 * In this case, check for skb->priority which would have
	 * the correct tid value and program it to the TCL metadata
	 * For accelerated packets with MSCS, skb->mark will not be
	 * set
	 */
	if (unlikely(skb->priority &&
		     dp_vif->mscs_hlos_tid_override))
		ath12k_wifi_qos_hlos_tid(&tcl_desc, skb->priority);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, &tcl_desc,
				     0, NULL);
		ath12k_dp_sdwftx_ingress_stats_update(arvif, &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}
	memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));
}
#endif

enum ath12k_dp_tx_enq_error
ath12k_wifi7_dp_tx_fast(struct ath12k_pdev_dp *dp_pdev,
			struct ath12k_link_vif *arvif,
			struct sk_buff *skb,
			u32 qos_nw_delay)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_hal *hal = dp->hal;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_srng *tcl_ring;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[arvif->link_id];
	struct dp_tx_ring *tx_ring;
	u8 pool_id;
	u8 hal_ring_id;
	u8 tid;
	bool is_from_recycler;
	bool stats_disable = ab->stats_disable;
	u8 ring_id = smp_processor_id();

	DP_STATS_INC_PKT(dp_vif, tx_i.recv_from_stack, 1, skb->len, ring_id);

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		return DP_TX_ENQ_DROP_CRASH_FLUSH;

	if (test_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags)) {
		kfree_skb(skb);
		return DP_TX_ENQ_SUCCESS;
	}

	pool_id = skb_get_queue_mapping(skb) & (ATH12K_HW_MAX_QUEUES - 1);

	tx_desc = ath12k_dp_tx_assign_buffer(dp, ring_id);
	if (unlikely(!tx_desc)) {
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, skb->len,
						 ATH_TX_BUF_ERR);
		}
		dp->device_stats.tx_err.txbuf_na[ring_id]++;
		return DP_TX_ENQ_DROP_SW_DESC_NA;
	}

	ath12k_core_dma_clean_range_no_dsb(skb->data, skb->data + DP_TX_SFE_BUFFER_SIZE);

	/* the edma driver uses this flags to optimize the cache invalidation */
	is_from_recycler = (skb->fast_recycled = !!skb->is_from_recycler);
	if (likely(is_from_recycler))
		tx_desc->flags = (DP_TX_DESC_FLAG_FAST & stats_disable);
	else
		tx_desc->flags = 0;

	tx_desc->skb = skb;
	tx_desc->mac_id = dp_link_vif->pdev_idx;

	tx_ring = &dp->tx_ring[ring_id];
	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &hal->srng_list[hal_ring_id];

	hal_tcl_desc =
	(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);
	if (unlikely(!hal_tcl_desc)) {
		/* NOTE: It is highly unlikely we'll be running out of tcl_ring
		 * desc because the desc is directly enqueued onto hw queue.
		 */
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		dp->device_stats.tx_err.desc_na[ring_id]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, skb->len,
						 ATH_TX_DESC_ERR);
		}
		ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);
		return DP_TX_ENQ_DROP_TCL_DESC_NA;
	}

	ath12k_wifi7_dp_tx_populate_tcl_desc(dp_pdev, arvif,
					     dp_link_vif,
					     skb, hal_tcl_desc,
					     tx_desc, qos_nw_delay);
	dmb(oshst);
	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	if (unlikely(ath12k_dp_stats_enabled(dp_pdev) &&
		     ath12k_tid_stats_enabled(dp_pdev))) {
		tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
		ath12k_tid_tx_stats(ahvif, tid, skb->len,
				    ATH_TX_FAST_UNICAST);
	}
	dp->device_stats.tx_fast_unicast[ring_id]++;

	DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw_fast, 1, skb->len, ring_id);
	atomic_inc(&dp_pdev->num_tx_pending);

	return DP_TX_ENQ_SUCCESS;
}

/* TODO: Remove the export once this file is built with wifi7 ko */
enum ath12k_dp_tx_enq_error
ath12k_wifi7_dp_tx(struct ath12k_pdev_dp *dp_pdev,
		   struct ath12k_link_vif *arvif,
		   struct sk_buff *skb, bool gsn_valid, int mcbc_gsn,
		   bool is_mcast, struct ath12k_link_sta *arsta, u8 ring_id,
		   u32 qos_nw_delay)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_hal *hal = dp->hal;
	struct ath12k_base *ab = dp->ab;
	struct hal_tx_info ti = {0};
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_tx_msdu_ext_desc *msg;
	struct sk_buff *skb_ext_desc = NULL;
	struct ethhdr *eth = NULL;
	struct hal_srng *tcl_ring;
	struct ieee80211_hdr *hdr = NULL;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[arvif->link_id];
	struct dp_tx_ring *tx_ring;
	u8 pool_id;
	u8 hal_ring_id;
	int ret;
	u8 reason, tid;
	u8 ring_selector, subtype;
	bool msdu_ext_desc = false;
	size_t hdrlen;
	bool add_htt_metadata = false;
	u32 iova_mask = dp->hw_params->iova_mask;
	bool is_diff_encap = false, is_null = false;
	u8 qos_tag;
	enum ath12k_dp_tx_enq_error err = DP_TX_ENQ_SUCCESS;

	DP_STATS_INC_PKT(dp_vif, tx_i.recv_from_stack, 1, skb->len, ring_id);

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		return DP_TX_ENQ_DROP_CRASH_FLUSH;

	if (test_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags)) {
		kfree_skb(skb);
		return err;
	}

	if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP)
		eth = (struct ethhdr *)skb->data;
	else
		hdr = (void *)skb->data;

	if (!(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	    !ieee80211_is_data(hdr->frame_control))
		return DP_TX_ENQ_DROP_NON_DATA_FRAME;

	if (eth && is_multicast_ether_addr(eth->h_dest) && arsta) {
		ti.meta_data_flags = arsta->tcl_metadata;
		ti.bss_ast_hash = arsta->ast_hash;
		ti.bss_ast_idx = arsta->ast_idx;
		ti.lookup_override = true;
	} else if (hdr && ieee80211_has_a4(hdr->frame_control) &&
	    is_multicast_ether_addr(hdr->addr3) && arsta) {
		ti.meta_data_flags = arsta->tcl_metadata;
		ti.flags0 |= FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_TO_FW, 1);
	} else {
		ti.meta_data_flags = dp_link_vif->tcl_metadata;
	}
	pool_id = skb_get_queue_mapping(skb) & (ATH12K_HW_MAX_QUEUES - 1);

	/* Let the default ring selection be based on current processor
	 * number, where one of the 3 tcl rings are selected based on
	 * the smp_processor_id(). In case that ring
	 * is full/busy, we resort to other available rings.
	 * If all rings are full, we drop the packet.
	 * TODO: Add throttling logic when all rings are full
	 */
	ring_selector = dp->hw_params->hw_ops->get_ring_selector(skb);

	ti.ring_id = ring_selector % dp->hw_params->max_tx_ring;

	ti.rbm_id = hal->tcl_to_cmp_rbm_map[ti.ring_id].rbm_id;

	tx_ring = &dp->tx_ring[ti.ring_id];

	tx_desc = ath12k_dp_tx_assign_buffer(dp, ti.ring_id);
	if (!tx_desc) {
		dp->device_stats.tx_err.txbuf_na[ti.ring_id]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, skb->len,
						 ATH_TX_BUF_ERR);
		}
		return DP_TX_ENQ_DROP_SW_DESC_NA;
	}

	ti.bank_id = dp_link_vif->bank_id;

	if (gsn_valid && !(ti.lookup_override)) {
		/* Reset and Initialize meta_data_flags with Global Sequence
		 * Number (GSN) info.
		 */
		ti.meta_data_flags =
			u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
					HTT_TCL_META_DATA_TYPE) |
			u32_encode_bits(mcbc_gsn, HTT_TCL_META_DATA_GLOBAL_SEQ_NUM);

		if (arvif->nawds_support)
			ti.meta_data_flags |= u32_encode_bits(1, HTT_TCL_META_DATA_GLOBAL_SEQ_HOST_INSPECTED);
	}

	ti.encap_type = ath12k_dp_tx_get_encap_type(ab, skb);
	ti.addr_search_flags = dp_link_vif->hal_addr_search_flags;
	ti.search_type = dp_link_vif->search_type;
	ti.type = HAL_TCL_DESC_TYPE_BUFFER;
	ti.pkt_offset = 0;
	ti.lmac_id = dp_link_vif->lmac_id;

	ti.vdev_id = dp_link_vif->vdev_id;
	if (gsn_valid)
		ti.vdev_id += HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID;
	else if (arvif->nawds_support && is_mcast && !ti.lookup_override)
		ti.meta_data_flags |= u32_encode_bits(1, HTT_TCL_META_DATA_HOST_INSPECTED_MISSION);

	if (!(ti.lookup_override)) {
		ti.bss_ast_hash = dp_link_vif->ast_hash;
		ti.bss_ast_idx = dp_link_vif->ast_idx;
	}
	ti.dscp_tid_tbl_idx = 0;

	switch (ti.encap_type) {
	case HAL_TCL_ENCAP_TYPE_NATIVE_WIFI:
		is_null = ieee80211_is_nullfunc(hdr->frame_control);
		if ((ahvif->vif->offload_flags & IEEE80211_OFFLOAD_ENCAP_ENABLED) &&
		    (skb->protocol == cpu_to_be16(ETH_P_PAE) || is_null))
			is_diff_encap = true;
		else
			ath12k_dp_tx_encap_nwifi(skb);
		break;
	case HAL_TCL_ENCAP_TYPE_RAW:
		if (!test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags)) {
			err = DP_TX_ENQ_DROP_ENCAP_RAW;
			goto fail_remove_tx_buf;
		}
		break;
	case HAL_TCL_ENCAP_TYPE_ETHERNET:
		/* no need to encap */
		break;
	case HAL_TCL_ENCAP_TYPE_802_3:
	default:
		/* TODO: Take care of other encap modes as well */
		err = DP_TX_ENQ_DROP_ENCAP_802_3;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, 0, ATH_TX_MISC_FAIL);
		}
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		goto fail_remove_tx_buf;
	}

	if (unlikely(dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_ETHERNET &&
		     !(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP))) {
		msdu_ext_desc = true;
		if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
			ti.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
			ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		}
	}

	if (unlikely(dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_RAW)) {
		if (skb->protocol == cpu_to_be16(ETH_P_ARP)) {
			ti.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
			ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
			msdu_ext_desc = true;
		}

		if (skb_cb->flags & ATH12K_SKB_CIPHER_SET) {
			ti.encrypt_type =
				ath12k_dp_tx_get_encrypt_type(skb_cb->cipher);

			if (ieee80211_has_protected(hdr->frame_control))
				skb_put(skb, IEEE80211_CCMP_MIC_LEN);
		} else {
			ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		}
	}

	if (iova_mask &&
	    (unsigned long)skb->data & iova_mask) {
		ret = ath12k_dp_tx_align_payload(dp, &skb);
		if (ret) {
			ath12k_warn(ab, "failed to align TX buffer %d\n", ret);
			/* don't bail out, give original buffer
			 * a chance even unaligned.
			 */
			goto map;
		}

		/* hdr is pointing to a wrong place after alignment,
		 * so refresh it for later use.
		 */
		hdr = (void *)skb->data;
	}
map:
#ifndef CONFIG_IO_COHERENCY
	ti.paddr = dma_map_single(dp->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(dp->dev, ti.paddr)) {
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		ath12k_warn(ab, "failed to DMA map data Tx buffer\n");
		err = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail_remove_tx_buf;
	}
#else
	ti.paddr = virt_to_phys(skb->data);
	if (!ti.paddr) {
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		ath12k_warn(ab, "failed to DMA map data Tx buffer\n");
		err = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail_remove_tx_buf;
	}
#endif

	if ((!test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	     !(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) &&
	     !(skb_cb->flags & ATH12K_SKB_CIPHER_SET) &&
	     ieee80211_has_protected(hdr->frame_control)) ||
	     is_diff_encap) {
		if (is_null && msdu_ext_desc)
			goto skip_htt_metadata;
		/* Add metadata for sw encrypted vlan group traffic */
		add_htt_metadata = true;
		msdu_ext_desc = true;
		ti.meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT;
skip_htt_metadata:
		ti.flags0 |= u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_TO_FW);
		ti.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
		ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	}

	tx_desc->skb = skb;
	tx_desc->mac_id = dp_link_vif->pdev_idx;
	ti.desc_id = tx_desc->desc_id;
	ti.data_len = skb->len;

	tx_desc->paddr = ti.paddr;
	tx_desc->len = ti.data_len;

	tx_desc->paddr_ext_desc = 0;

	if (msdu_ext_desc) {
		skb_ext_desc = dev_alloc_skb(sizeof(struct hal_tx_msdu_ext_desc));
		if (!skb_ext_desc) {
			err = DP_TX_ENQ_DROP_EXT_DESC_NA;
			goto fail_unmap_dma;
		}

		skb_put(skb_ext_desc, sizeof(struct hal_tx_msdu_ext_desc));
		memset(skb_ext_desc->data, 0, skb_ext_desc->len);

		msg = (struct hal_tx_msdu_ext_desc *)skb_ext_desc->data;
		ath12k_wifi7_hal_tx_cmd_ext_desc_setup(ab, msg, &ti);

		if (add_htt_metadata) {
			ret = ath12k_wifi7_dp_prepare_htt_metadata(skb_ext_desc);
			if (ret < 0) {
				ath12k_dbg(ab, ATH12K_DBG_DP_TX,
					   "Failed to add HTT meta data, dropping packet\n");
				err = DP_TX_ENQ_DROP_HTT_MDATA_ERR;
				goto fail_free_ext_skb;
			}
		}
#ifndef CONFIG_IO_COHERENCY
		ti.paddr = dma_map_single(dp->dev, skb_ext_desc->data,
					  skb_ext_desc->len, DMA_TO_DEVICE);
		ret = dma_mapping_error(dp->dev, ti.paddr);
		if (ret) {
			err = DP_TX_ENQ_DROP_DMA_ERR;
			goto fail_free_ext_skb;
		}
#else
		ti.paddr = virt_to_phys(skb_ext_desc->data);
		if (!ti.paddr) {
			err = DP_TX_ENQ_DROP_DMA_ERR;
			goto fail_free_ext_skb;
		}
#endif
		ti.data_len = skb_ext_desc->len;
		ti.type = HAL_TCL_DESC_TYPE_EXT_DESC;

		tx_desc->paddr_ext_desc = ti.paddr;
		tx_desc->ext_desc_len = ti.data_len;
		tx_desc->skb_ext_desc = skb_ext_desc;
	}

	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &hal->srng_list[hal_ring_id];

	ath12k_hal_srng_access_begin_no_lock(tcl_ring);
	hal_tcl_desc = ath12k_hal_srng_src_get_next_entry(ab, tcl_ring);
	if (!hal_tcl_desc) {
		/* NOTE: It is highly unlikely we'll be running out of tcl_ring
		 * desc because the desc is directly enqueued onto hw queue.
		 */
		ath12k_hal_srng_access_end_no_lock(ab, tcl_ring);
		dp->device_stats.tx_err.desc_na[ti.ring_id]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_drop_stats(ahvif, tid, 0,
						 ATH_TX_DESC_NA_ERR);
		}
		err = DP_TX_ENQ_DROP_TCL_DESC_NA;
		goto fail_unmap_dma_ext;
	}

	spin_lock_bh(&arvif->link_stats_lock);
	if (is_mcast) {
		reason = ATH_TX_MCAST_PKTS;
		ab->dp->device_stats.tx_mcast[ti.ring_id]++;
	} else if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
		ab->dp->device_stats.tx_eapol[ti.ring_id]++;
		reason = ATH_TX_EAPOL_PKTS;
		if (ti.encap_type == HAL_TCL_ENCAP_TYPE_NATIVE_WIFI) {
			hdr = (struct ieee80211_hdr *)skb->data;
			hdrlen = ieee80211_get_hdrlen_from_skb(skb);
			subtype = ath12k_dp_get_eapol_subtype
						  (skb->data + hdrlen + LLC_SNAP_HDR_LEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
				ab->dp->device_stats.tx_eapol_type[subtype-1][ti.ring_id]++;
				ath12k_dbg(ab, ATH12K_DBG_EAPOL, "Transmit %s%d EAPOL "
					   "frame to STA %pM\n", subtype <= 4 ? "M" : "G",
					   subtype <= 4 ? subtype : (subtype - 4),
					   hdr->addr1);
			}
		} else {
			eth = (struct ethhdr *)skb->data;
			subtype = ath12k_dp_get_eapol_subtype(skb->data + ETH_HLEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
				ab->dp->device_stats.tx_eapol_type[subtype-1][ti.ring_id]++;
				ath12k_dbg(ab, ATH12K_DBG_EAPOL, "Transmit %s%d EAPOL "
					   "frame to STA %pM\n", subtype <= 4 ? "M" : "G",
					   subtype <= 4 ? subtype : (subtype - 4),
					   eth->h_dest);
			}
		}

	} else if (is_null) {
		ab->dp->device_stats.tx_null_frame[ti.ring_id]++;
		reason = ATH_TX_NULL_PKTS;
	} else {
		ab->dp->device_stats.tx_unicast[ti.ring_id]++;
		reason = ATH_TX_UNICAST_PKTS;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
		ath12k_tid_tx_stats(ahvif, tid, skb->len, reason);
	}

	arvif->link_stats.tx_encap_type[ti.encap_type]++;
	arvif->link_stats.tx_encrypt_type[ti.encrypt_type]++;
	arvif->link_stats.tx_desc_type[ti.type]++;

	if (is_mcast)
		arvif->link_stats.tx_bcast_mcast++;
	else
		arvif->link_stats.tx_enqueued++;
	spin_unlock_bh(&arvif->link_stats_lock);

	if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
		if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
			if (is_mcast) {
				eth = (struct ethhdr *)skb->data;
				if (eth && is_broadcast_ether_addr(eth->h_dest))
					tx_desc->flags |= DP_TX_DESC_FLAG_BCAST;
				else
					tx_desc->flags |= DP_TX_DESC_FLAG_MCAST;
				DP_STATS_INC(dp_vif, tx_i.mcast, 1, ti.ring_id);
			}
			DP_STATS_INC(dp_vif, tx_i.encap_type[ti.encap_type], 1,
				     ti.ring_id);
			DP_STATS_INC(dp_vif, tx_i.encrypt_type[ti.encrypt_type], 1,
				     ti.ring_id);
			DP_STATS_INC(dp_vif, tx_i.desc_type[ti.type], 1, ti.ring_id);
		}
	}

	ath12k_wifi7_hal_tx_cmd_desc_setup(ab, hal_tcl_desc, &ti);

	/* For SDWF DS support, either the slow packet or the
	 * reinject packets would not have skb->fast_xmit set
	 * and the msduq information should be updated if the
	 * SDWF is valid in skb->mark
	 */
	if (unlikely(skb->mark & SDWF_VALID_MASK))
		ath12k_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
				     0, NULL);

	if (unlikely(arsta)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_dp_qos_update(dp, dp_pdev, skb->mark,
					     hal_tcl_desc,
					     qos_tag, arsta->addr);
	}

	ath12k_hal_srng_access_end_no_lock(ab, tcl_ring);

	DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw, 1, ti.data_len, ti.ring_id);

	atomic_inc(&dp_pdev->num_tx_pending);

	return DP_TX_ENQ_SUCCESS;

fail_unmap_dma_ext:
	if (tx_desc->paddr_ext_desc)
		ath12k_core_dma_unmap_single(dp->dev, tx_desc->paddr_ext_desc,
					     tx_desc->ext_desc_len,
					     DMA_TO_DEVICE);
fail_free_ext_skb:
	if (skb_ext_desc)
		kfree_skb(skb_ext_desc);

fail_unmap_dma:
	ath12k_core_dma_unmap_single(dp->dev, ti.paddr, ti.data_len, DMA_TO_DEVICE);

fail_remove_tx_buf:
	if (tx_desc)
		ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);

	spin_lock_bh(&arvif->link_stats_lock);
	arvif->link_stats.tx_dropped++;
	spin_unlock_bh(&arvif->link_stats_lock);
	return err;
}

static inline void
ath12k_wifi7_dp_tx_get_hw_link_id_from_ppdu_id(struct hal_tx_status *ts,
					       struct ath12k_dp *dp)
{
	ts->hw_link_id = (DP_GET_HW_LINK_ID_FRM_PPDU_ID(ts->ppdu_id,
							dp->link_id_offset,
							dp->link_id_bits));
}

static void ath12k_wifi7_dp_tx_free_txbuf(struct ath12k_dp *dp,
					  struct sk_buff *msdu,
					  struct dp_tx_ring *tx_ring,
					  struct ath12k_tx_sw_metadata *sw_metadata)
{
	struct ath12k_pdev_dp *dp_pdev;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;
	u8 pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);

	ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr, sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		dev_kfree_skb_any(msdu);
	else
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);

	rcu_read_unlock();
}

static void
ath12k_wifi7_dp_tx_htt_tx_complete_buf(struct ath12k_dp *dp,
				       struct sk_buff *msdu,
				       struct dp_tx_ring *tx_ring,
				       struct hal_tx_status *ts,
				       struct ath12k_tx_sw_metadata *sw_metadata,
				       u16 peer_id)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_tx_info *info;
	struct ath12k_link_vif *arvif;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;
	struct ath12k_pdev_dp *dp_pdev;
	struct ethhdr *eth;
	struct ieee80211_hdr *hdr;
	size_t hdrlen;
	enum ath12k_dp_eapol_key_type subtype;
	u8 pdev_id;

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);

	ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr, sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	skb_cb = ATH12K_SKB_CB(msdu);
	info = IEEE80211_SKB_CB(msdu);

	vif = skb_cb->vif;
	if (vif) {
		ahvif = ath12k_vif_to_ahvif(vif);
		if (dp_pdev->wmm_stats.tx_type) {
			ahvif->wmm_stats.tx_type = dp_pdev->wmm_stats.tx_type;
			if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
				ahvif->wmm_stats.total_wmm_tx_drop[ahvif->wmm_stats.tx_type]++;
		}

		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		if (arvif) {
			spin_lock_bh(&arvif->link_stats_lock);
			arvif->link_stats.tx_completed++;
			spin_unlock_bh(&arvif->link_stats_lock);
		}
	}

	if (msdu->protocol == cpu_to_be16(ETH_P_PAE)) {
		if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) {
			eth = (struct ethhdr *)msdu->data;
			subtype = ath12k_dp_get_eapol_subtype(msdu->data + ETH_HLEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX)
				ath12k_dbg(ab, ATH12K_DBG_EAPOL, "Tx completion success for"
					   " %s%d EAPOL frame to STA %pM\n",
					   subtype <= 4 ? "M" : "G",
					   subtype <= 4 ? subtype : (subtype - 4),
					   eth->h_dest);
		} else {
			hdr = (struct ieee80211_hdr *)msdu->data;
			hdrlen = ieee80211_get_hdrlen_from_skb(msdu);
			subtype = ath12k_dp_get_eapol_subtype(msdu->data
					+ hdrlen + LLC_SNAP_HDR_LEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX)
				ath12k_dbg(ab, ATH12K_DBG_EAPOL, "Tx completion success for"
					   " %s%d EAPOL frame to STA %pM\n",
					   subtype <= 4 ? "M" : "G",
					   subtype <= 4 ? subtype : (subtype - 4),
					   hdr->addr1);
		}
	}

	memset(&info->status, 0, sizeof(info->status));

	if (ts->acked) {
		if (!(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			info->flags |= IEEE80211_TX_STAT_ACK;
			info->status.ack_signal = ts->ack_rssi;

			if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
				      ab->wmi_ab.svc_map))
				info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

			info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
		} else {
			info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;
		}
	}

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer || !peer->sta)
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n", peer_id);
	else
		status.sta = peer->sta;

	if ((unlikely(ath12k_dp_stats_enabled(dp_pdev))) &&
	    (unlikely(ath12k_debugfs_is_qos_stats_enabled(dp_pdev->ar)))) {
		ath12k_qos_stats_update(dp_pdev->ar, msdu, ts, dp_pdev,
					msdu->tstamp);
	}

	status.info = info;
	status.skb = msdu;
	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	rcu_read_unlock();
}

static void
ath12k_wifi7_dp_tx_process_htt_tx_complete(struct ath12k_dp *dp,
					   void *desc, struct sk_buff *msdu,
					   struct dp_tx_ring *tx_ring,
					   struct ath12k_tx_sw_metadata *sw_metadata,
					   struct hal_tx_status *ts,
					   int ring_id, u32 htt_status)
{
	struct htt_tx_wbm_completion *status_desc;
	struct ath12k_pdev_dp *dp_pdev;
	u8 pdev_id;
	struct ath12k_dp_peer *peer = NULL;
	u8 link_id = 0;
	u32 msdu_len = msdu->len;
	u8 tx_desc_flags = sw_metadata->flags;

	status_desc = desc;

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);

	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	ts->ppdu_id = le32_get_bits(status_desc->info2,
				    HTT_TX_WBM_COMP_INFO2_PPDU_ID);
	ath12k_wifi7_dp_tx_get_hw_link_id_from_ppdu_id(ts, dp);

	if (le32_get_bits(status_desc->info3, HTT_TX_WBM_COMP_INFO3_VALID)) {
		ts->peer_id = le32_get_bits(status_desc->info3,
					    HTT_TX_WBM_COMP_INFO3_SW_PEER_ID);
	} else {
		ts->peer_id = HAL_INVALID_PEERID;
	}

	switch (htt_status) {
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_OK:
		ts->acked = (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK);
		ts->ack_rssi = le32_get_bits(status_desc->info2,
					    HTT_TX_WBM_COMP_INFO2_ACK_RSSI);

		ts->status = HAL_WBM_TQM_REL_REASON_FRAME_ACKED;
		ath12k_wifi7_dp_tx_htt_tx_complete_buf(dp, msdu, tx_ring, ts,
						       sw_metadata, ts->peer_id);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT:
		switch (htt_status) {
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU;
			break;
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX;
		default:
			break;
		}
		fallthrough;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH:
		ath12k_wifi7_dp_tx_free_txbuf(dp, msdu, tx_ring, sw_metadata);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY:
		/* This event is to be handled only when the driver decides to
		 * use WDS offload functionality.
		 */
		break;
	default:
		ath12k_warn(dp->ab, "Unknown htt tx status %d\n", htt_status);
		break;
	}

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (peer) {
		link_id = ath12k_dp_get_link_id(dp_pdev, ts, peer);
		peer->stats_link_id = link_id;
		ath12k_dp_tx_update_peer_basic_stats(peer, msdu_len,
						     htt_status,
						     link_id, ring_id);
		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_dp_tx_comp_update_peer_stats(peer, ts,
								    ring_id,
								    tx_desc_flags);
		}
	} else {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PEER][ring_id], 1);
	}
	rcu_read_unlock();
}

static void
ath12k_wifi7_dp_tx_cache_peer_stats(struct ath12k *ar,
					  struct sk_buff *msdu,
					  struct hal_tx_status *ts)
{
	struct ath12k_per_peer_tx_stats *peer_stats = &ar->cached_stats;

	if (ts->try_cnt > 1) {
		peer_stats->retry_pkts += ts->try_cnt - 1;
		peer_stats->retry_bytes += (ts->try_cnt - 1) * msdu->len;

		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
			peer_stats->failed_pkts += 1;
			peer_stats->failed_bytes += msdu->len;
		}
	}
}

static void
ath12k_wifi7_dp_tx_update_txcompl(struct ath12k_pdev_dp *dp_pdev,
				  struct hal_tx_status *ts)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_link_sta *arsta;
	struct rate_info txrate = {0};
	u16 rate, ru_tones;
	u8 rate_idx = 0;
	int ret;

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (!dp_peer) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "MLD peer NA with peer_id: %u\n", ts->peer_id);
		return;
	}

	spin_lock_bh(&dp->dp_lock);
	peer = rcu_dereference(dp_peer->link_peers[dp_peer->stats_link_id]);
	if (!peer || !peer->sta) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}
	sta = peer->sta;
	ahsta = ath12k_sta_to_ahsta(sta);
	arsta = &ahsta->deflink;

	/* This is to prefer choose the real NSS value arsta->last_txrate.nss,
	 * if it is invalid, then choose the NSS value while assoc.
	 */
	if (peer->last_txrate.nss)
		txrate.nss = peer->last_txrate.nss;
	else
		txrate.nss = arsta->peer_nss;
	spin_unlock_bh(&dp->dp_lock);

	switch (ts->pkt_type) {
	case HAL_TX_RATE_STATS_PKT_TYPE_11A:
	case HAL_TX_RATE_STATS_PKT_TYPE_11B:
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(ts->mcs,
							    ts->pkt_type,
							    &rate_idx,
							    &rate);
		if (ret < 0) {
			ath12k_warn(ab, "Invalid tx legacy rate %d\n", ret);
			return;
		}

		txrate.legacy = rate;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11N:
		if (ts->mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(ab, "Invalid HT mcs index %d\n", ts->mcs);
			return;
		}

		if (txrate.nss < 1 ||
		    (txrate.nss > hweight32(dp_pdev->ar->pdev->cap.tx_chain_mask)))
			ath12k_warn(ab, "Invalid nss value: %d", txrate.nss);
		else
			txrate.mcs = ts->mcs + 8 * (txrate.nss - 1);

		txrate.flags = RATE_INFO_FLAGS_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AC:
		if (ts->mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid VHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_VHT_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AX:
		if (ts->mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(ab, "Invalid HE mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_HE_MCS;
		txrate.he_gi = ath12k_he_gi_to_nl80211_he_gi(ts->sgi);
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11BE:
		if (ts->mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid EHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_EHT_MCS;
		txrate.eht_gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(ts->sgi);
		break;
	default:
		ath12k_warn(ab, "Invalid tx pkt type: %d\n", ts->pkt_type);
		return;
	}

	txrate.bw = ath12k_mac_bw_to_mac80211_bw(ts->bw);

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11AX) {
		txrate.bw = RATE_INFO_BW_HE_RU;
		ru_tones = ath12k_mac_he_convert_tones_to_ru_tones(ts->tones);
		txrate.he_ru_alloc =
			ath12k_he_ru_tones_to_nl80211_he_ru_alloc(ru_tones);
	}

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BE) {
		txrate.bw = RATE_INFO_BW_EHT_RU;
		txrate.eht_ru_alloc =
			ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(ts->tones);
	}

	spin_lock_bh(&dp->dp_lock);
	peer = rcu_dereference(dp_peer->link_peers[dp_peer->stats_link_id]);
	if (peer)
		peer->txrate = txrate;
	else
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
	spin_unlock_bh(&dp->dp_lock);
}

static void ath12k_wifi7_dp_tx_complete_msdu(struct ath12k_pdev_dp *dp_pdev,
					     struct sk_buff *msdu,
					     struct hal_tx_status *ts,
					     struct ath12k_tx_sw_metadata *sw_metadata,
					     u8 mac_id, int ring)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_rate_status status_rate = { 0 };
	struct rate_info rate;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ieee80211_tx_info *info;
	struct ath12k_link_vif *arvif;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *link_peer;
	struct sk_buff *skb_ext_desc = sw_metadata->skb_ext_desc;
	struct ath12k *ar;
	struct ath12k_dp_peer *peer = NULL;
	u8 link_id = 0;
	u8 reason, tid;
	enum ath12k_dp_tx_comp_error drop_reason = DP_TX_COMP_ERR_MISC;
	u32 msdu_len = msdu->len;
	u8 tx_desc_flags = sw_metadata->flags;

	if (WARN_ON_ONCE(ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)) {
		/* Must not happen */
		return;
	}

	if (sw_metadata->skb)
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr, sw_metadata->len, DMA_TO_DEVICE);
	if (sw_metadata->paddr_ext_desc) {
		ath12k_core_dma_unmap_single(dp->dev, sw_metadata->paddr_ext_desc,
					     sw_metadata->ext_desc_len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb_ext_desc);
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		return;

	dp_pdev->wmm_stats.tx_type = ath12k_tid_to_ac(ts->tid > ATH12K_DSCP_PRIORITY ? 0:ts->tid);
	if (dp_pdev->wmm_stats.tx_type) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
			dp_pdev->wmm_stats.total_wmm_tx_drop[dp_pdev->wmm_stats.tx_type]++;
	}

	skb_cb = ATH12K_SKB_CB(msdu);

	rcu_read_lock();

	if (!rcu_dereference(ab->pdevs_active[dp_pdev->mac_id])) {
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		drop_reason = DP_TX_COMP_ERR_INVALID_PDEV;
		goto exit;
	}

	if (!skb_cb->vif) {
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		drop_reason = DP_TX_COMP_ERR_INVALID_VIF;
		goto exit;
	}

	vif = skb_cb->vif;
	if (vif) {
		ahvif = ath12k_vif_to_ahvif(vif);
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_stats(ahvif, tid, msdu->len,
					    ATH_TX_COMPLETED_PKTS);
		}
		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		if (arvif) {
			spin_lock_bh(&arvif->link_stats_lock);
			arvif->link_stats.tx_completed++;
			spin_unlock_bh(&arvif->link_stats_lock);
		}
	}

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	/* skip tx rate update from ieee80211_status*/
	info->status.rates[0].idx = -1;

	ar = dp_pdev->ar;

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (peer) {
		link_id = ath12k_dp_get_link_id(dp_pdev, ts, peer);
		peer->stats_link_id = link_id;
		ath12k_dp_tx_update_peer_basic_stats(peer, msdu_len, ts->status,
						     link_id, ring);

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_dp_tx_comp_update_peer_stats(peer, ts,
								    ring,
								    tx_desc_flags);
			if (unlikely(ath12k_debugfs_is_qos_stats_enabled(ar)))
				ath12k_qos_stats_update(ar, msdu, ts,
							dp_pdev,
							msdu->tstamp);
		}
	} else {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PEER][ring], 1);
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
			!(info->flags & IEEE80211_TX_CTL_NO_ACK))	{
		info->flags |= IEEE80211_TX_STAT_ACK;
		info->status.ack_signal = ts->ack_rssi;

		if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
			      ab->wmi_ab.svc_map))
			info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

		info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX &&
			(info->flags & IEEE80211_TX_CTL_NO_ACK))
		info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts->status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
			reason = ATH_TX_TQM_REMOVE_MPDU;
			dev_kfree_skb_any(msdu);
			goto exit;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_TQM_THRESHOLD;
			dev_kfree_skb_any(msdu);
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_TQM_REMOVE_AGED;
			dev_kfree_skb_any(msdu);
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_TQM_REMOVE_TX;
			dev_kfree_skb_any(msdu);
			goto exit;
		default:
			//TODO: Remove this print and add as a stats
			ath12k_dbg(ab, ATH12K_DBG_DP_TX, "tx frame is not acked status %d\n", ts->status);
 		}
	}

	/* NOTE: Tx rate status reporting. Tx completion status does not have
	 * necessary information (for example nss) to build the tx rate.
	 * Might end up reporting it out-of-band from HTT stats.
	 */

	if (ath12k_extd_tx_stats_enabled(ar)) {
		if (ts->flags & HAL_TX_STATUS_FLAGS_FIRST_MSDU) {
			if (ar->last_ppdu_id == 0) {
				ar->last_ppdu_id = ts->ppdu_id;
			} else if (ar->last_ppdu_id == ts->ppdu_id ||
				ar->cached_ppdu_id == ar->last_ppdu_id) {
				ar->cached_ppdu_id = ar->last_ppdu_id;
				ar->cached_stats.is_ampdu = true;
				ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
						sizeof(struct ath12k_per_peer_tx_stats));
			} else {
				ar->cached_stats.is_ampdu = false;
				ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
						sizeof(struct ath12k_per_peer_tx_stats));
			}
			ar->last_ppdu_id = ts->ppdu_id;
		}

		ath12k_wifi7_dp_tx_cache_peer_stats(ar, msdu, ts);
	}

       	ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);

	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
							     ts->peer_id);
	if (!link_peer || !link_peer->sta) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			   ts->peer_id);
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
		drop_reason = DP_TX_COMP_ERR_INVALID_LINK_PEER;
		goto exit;
	}

	status.sta = link_peer->sta;
	status.info = info;
	status.skb = msdu;
	rate = link_peer->last_txrate;

	status_rate.rate_idx = rate;
	status_rate.try_count = 1;

	status.rates = &status_rate;
	status.n_rates = 1;

	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	rcu_read_unlock();
	return;

exit:
	DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[drop_reason][ring], 1);
	if (ahvif && ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev))
		ath12k_tid_tx_drop_stats(ahvif, tid, msdu_len, reason);
	rcu_read_unlock();
}

void
ath12k_wifi7_dp_tx_status_parse(struct ath12k_base *ab,
				struct hal_wbm_completion_ring_tx *desc,
				struct hal_tx_status *ts)
{
	u32 info0 = le32_to_cpu(desc->rate_stats.info0);

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_FW &&
	    ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)
		return;

	ts->ppdu_id = le32_get_bits(desc->info1,
				    HAL_WBM_COMPL_TX_INFO1_TQM_STATUS_NUMBER);

	ts->ack_rssi = FIELD_GET(HAL_WBM_COMPL_TX_INFO2_ACK_FRAME_RSSI,
				 desc->info2);

	ts->peer_id = le32_get_bits(desc->info3, HAL_WBM_COMPL_TX_INFO3_PEER_ID);

	if (info0 & HAL_TX_RATE_STATS_INFO0_VALID) {
		ts->pkt_type = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_PKT_TYPE);
		ts->mcs = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_MCS);
		ts->sgi = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_SGI);
		ts->bw = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_BW);
		ts->tones = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_TONES_IN_RU);
		ts->ofdma = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_OFDMA_TX);
	}

	ts->tid = FIELD_GET(HAL_WBM_RELEASE_TX_INFO3_TID, desc->info3);
	ts->transmit_cnt = le32_get_bits(desc->info1,
					 HAL_WBM_COMPL_TX_INFO1_TRANSMIT_COUNT);
	ts->buffer_timestamp = FIELD_GET(HAL_WBM_RELEASE_TX_INFO2_BUFFER_TIMESTAMP,
					 desc->info2);
	ts->tsf = desc->rate_stats.tsf;
	ts->first_msdu = le32_get_bits(desc->info2,
				       HAL_WBM_COMPL_TX_INFO2_FIRST_MSDU);
	ts->last_msdu = le32_get_bits(desc->info2,
				      HAL_WBM_COMPL_TX_INFO2_LAST_MSDU);
	ts->msdu_part_of_amsdu =
			(ts->first_msdu && ts->last_msdu) ? false : true;

	ath12k_wifi7_dp_tx_get_hw_link_id_from_ppdu_id(ts, ab->dp);
}

int ath12k_wifi7_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp *dp_pdev;
	int hal_ring_id = dp->tx_ring[ring_id].tcl_comp_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_tx_status ts = { 0 };
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	struct hal_wbm_release_ring *desc;
	u8 pdev_id;
	u64 desc_va;
	int i;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
#endif
	int orig_budget = budget;
	struct ath12k_skb_cb *skb_cb;
	struct sk_buff *msdu = NULL;
	struct ath12k_vif *ahvif = NULL;
	bool fast_flag;
	int pdev_tx_comp_cnt[MAX_RADIOS] = {0};
	u8 mac_id = 0;

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return 0;
	}

	if (valid_entries > budget)
		valid_entries = budget;

	ath12k_hal_srng_dst_invalidate_entry(dp, status_ring, valid_entries);
#endif

	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_wifi7_tx_status_entry *tx_status_entry;
	struct ath12k_tx_sw_metadata *sw_metadata;
	u8 n_entry = 0, idx = 0;
	struct list_head desc_free_list;
	struct hal_wbm_completion_ring_tx *tx_status;
	struct sk_buff_head free_list_head;
	int tx_status_idx = smp_processor_id();
	u32 tx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX] = {0};
	u32 tqm_rel_reason[MAX_TQM_RELEASE_REASON] = {0};
	u32 fw_tx_status[MAX_FW_TX_STATUS] = {0};
	u32 htt_status = 0, tx_completed = 0;
	u8 tid = 0;

	INIT_LIST_HEAD(&desc_free_list);
	skb_queue_head_init(&free_list_head);

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[tx_status_idx];
	while (budget-- && (desc = __ath12k_hal_srng_dst_get_next_cached_entry(status_ring, NULL))) {
		if (!ath12k_dp_tx_completion_valid(desc))
			continue;

		tx_status = (struct hal_wbm_completion_ring_tx *)desc;

		/* HW done cookie conversion */
		desc_va = ((u64)le32_to_cpu(tx_status->buf_va_hi) << 32 |
			   le32_to_cpu(tx_status->buf_va_lo));
		tx_desc = (struct ath12k_tx_desc_info *)((unsigned long)desc_va);
		if (unlikely(!tx_desc)) {
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_DESC][ring_id], 1);
			ath12k_warn(ab, "unable to retrieve tx_desc!");
			continue;
		}

		tx_status_entry->tx_desc = tx_desc;

		n_entry++;
		memcpy(&tx_status_entry->tx_status, tx_status, sizeof(*tx_status));
		tx_status_entry++;
	}

	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	if (!n_entry)
		return orig_budget - budget;

	spin_lock_bh(&dp->tx_desc_lock[ring_id]);

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[tx_status_idx];
	for (i = 0; i < n_entry; i++) {
		struct ath12k_wifi7_tx_status_entry *tx_status_entry_next;
		sw_metadata = &tx_status_entry->sw_metadata;
		tx_desc = tx_status_entry->tx_desc;
		tx_status_entry++;

		if ((i + 10) < n_entry) {
			tx_status_entry_next = tx_status_entry + 8;

			prefetch(tx_status_entry_next->tx_desc);
			prefetch((tx_status_entry_next + 1));
		}

		if (unlikely(!tx_desc->in_use)) {
			sw_metadata->skb = NULL;
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_DESC_INUSE][ring_id], 1);
			continue;
		}

		list_add_tail(&tx_desc->list, &desc_free_list);

		sw_metadata->skb = tx_desc->skb;
		sw_metadata->paddr = tx_desc->paddr;
		sw_metadata->len = tx_desc->len;
		sw_metadata->flags = tx_desc->flags;

		if (unlikely(!(sw_metadata->flags & DP_TX_DESC_FLAG_FAST))) {
			sw_metadata->skb_ext_desc = tx_desc->skb_ext_desc;
			sw_metadata->paddr_ext_desc = tx_desc->paddr_ext_desc;
			tx_desc->skb_ext_desc = NULL;
			tx_desc->paddr_ext_desc = 0;
			sw_metadata->ext_desc_len = tx_desc->ext_desc_len;
		}

		sw_metadata->mac_id = tx_desc->mac_id;
		pdev_tx_comp_cnt[sw_metadata->mac_id]++;

		tx_desc->skb = NULL;
		tx_desc->in_use = false;
		tx_desc->flags = 0;
	}

	list_splice(&desc_free_list, &dp->tx_desc_free_list[ring_id]);

	spin_unlock_bh(&dp->tx_desc_lock[ring_id]);

	for (mac_id = 0; mac_id < MAX_RADIOS; mac_id++) {
		if (likely(pdev_tx_comp_cnt[mac_id])) {
			pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, mac_id);
			rcu_read_lock();
			dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
			rcu_read_unlock();

			if (unlikely(!dp_pdev))
				continue;

			if (atomic_sub_and_test(pdev_tx_comp_cnt[mac_id], &dp_pdev->num_tx_pending))
				wake_up(&dp_pdev->tx_empty_waitq);
		}
	}

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[tx_status_idx];
	while (n_entry--) {
		fast_flag = false;
		tx_status = &tx_status_entry->tx_status;
		sw_metadata = &tx_status_entry->sw_metadata;

		tx_status_entry++;

		if (!sw_metadata->skb)
			continue;

		tx_completed++;
		ts.buf_rel_source =
			le32_get_bits(tx_status->info0, HAL_WBM_COMPL_TX_INFO0_REL_SRC_MODULE);

		tx_wbm_rel_source[ts.buf_rel_source]++;

		if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			msdu = sw_metadata->skb;
			skb_cb = ATH12K_SKB_CB(msdu);
			ahvif = ath12k_vif_to_ahvif(skb_cb->vif);
			tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_stats(ahvif, tid, msdu->len,
					    ATH_TX_WBM_REL_SRC);
		}

                if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_TQM) {
                        ts.status = le32_get_bits(tx_status->info0,
                                                  HAL_WBM_COMPL_TX_INFO0_TQM_RELEASE_REASON);
                        tqm_rel_reason[ts.status]++;
                } else if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW) {
                        htt_status = le32_get_bits(tx_status->info0,
                                                   HTT_TX_WBM_COMP_INFO0_STATUS);
                        fw_tx_status[htt_status]++;
			if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_FW_STATUS);
			}

			ath12k_wifi7_dp_tx_process_htt_tx_complete(dp, (void *)tx_status,
								   sw_metadata->skb,
								   tx_ring, sw_metadata,
								   &ts, ring_id, htt_status);
			sw_metadata->skb = NULL;
			continue;
                }

		if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
			__skb_queue_head(&free_list_head, sw_metadata->skb);
			sw_metadata->skb = NULL;
			fast_flag = true;
		}

		if (n_entry == 1)
			prefetch(&dp->device_stats);

		if (unlikely(!fast_flag && sw_metadata->skb)) {
			rcu_read_lock();

			pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, sw_metadata->mac_id);
			dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
			if (!dp_pdev) {
				DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
				rcu_read_unlock();
				continue;
			}

			ath12k_wifi7_dp_tx_status_parse(ab, tx_status, &ts);
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_COMPLETED_PKTS);
			}
			ath12k_wifi7_dp_tx_complete_msdu(dp_pdev, sw_metadata->skb, &ts,
					sw_metadata, sw_metadata->mac_id,
					ring_id);
			sw_metadata->skb = NULL;

			rcu_read_unlock();
		}

	}

        dp->device_stats.tx_comp_stats[ring_id].tx_completed += tx_completed;

        for (idx = 0; idx < HAL_WBM_REL_SRC_MODULE_MAX; idx++)
                dp->device_stats.tx_comp_stats[ring_id].tx_wbm_rel_source[idx] += tx_wbm_rel_source[idx];

        for (idx = 0; idx < MAX_TQM_RELEASE_REASON; idx++)
                dp->device_stats.tx_comp_stats[ring_id].tqm_rel_reason[idx] += tqm_rel_reason[idx];

        for (idx = 0; idx < MAX_FW_TX_STATUS; idx++)
                dp->device_stats.tx_comp_stats[ring_id].fw_tx_status[idx] += fw_tx_status[idx];

	dev_kfree_skb_list_fast(&free_list_head);

	return orig_budget - budget;
}

u32 ath12k_wifi7_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_link_vif *arvif,
					    bool vdev_id_check_en)
{
	u32 bank_config = 0;
	u8 link_id = arvif->link_id;
	enum hal_encrypt_type encrypt_type = 0;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[link_id];

	/* Only valid for raw frames with HW crypto enabled.
	 * With SW crypto, mac80211 sets key per packet
	 */
	if (dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_RAW &&
	    test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	    arvif->key_cipher != INVALID_CIPHER)
		bank_config |=
			u32_encode_bits(ath12k_dp_tx_get_encrypt_type(arvif->key_cipher),
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);
	else
		encrypt_type = HAL_ENCRYPT_TYPE_OPEN;

	bank_config |= u32_encode_bits(dp_vif->tx_encap_type,
					HAL_TX_BANK_CONFIG_ENCAP_TYPE) |
					u32_encode_bits(encrypt_type,
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);

	bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_SRC_BUFFER_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_LINK_META_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_EPD);

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		bank_config |= u32_encode_bits(1, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);
	else
		bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);

	bank_config |= u32_encode_bits(dp_link_vif->hal_addr_search_flags & HAL_TX_ADDRX_EN,
					HAL_TX_BANK_CONFIG_ADDRX_EN) |
			u32_encode_bits(!!(dp_link_vif->hal_addr_search_flags &
					HAL_TX_ADDRY_EN),
					HAL_TX_BANK_CONFIG_ADDRY_EN);

	bank_config |= u32_encode_bits(ieee80211_vif_is_mesh(ahvif->vif) ? 3 : 0,
					HAL_TX_BANK_CONFIG_MESH_EN) |
			u32_encode_bits(vdev_id_check_en,
					HAL_TX_BANK_CONFIG_VDEV_ID_CHECK_EN);

	bank_config |= u32_encode_bits(dp_link_vif->map_id, HAL_TX_BANK_CONFIG_DSCP_TIP_MAP_ID);

	return bank_config;
}

int ath12k_wifi7_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb, struct ath12k_link_sta *arsta)
{
	u8 ring_selector = 0, ring_id = 0;

	ring_selector = smp_processor_id();
	ring_id = ring_selector % dp_pdev->dp->hw_params->max_tx_ring;

	return ath12k_wifi7_dp_tx(dp_pdev, arvif, skb, false, 0, false, arsta, ring_id, 0);
}

void ath12k_wifi7_dp_tx_ring_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	int i;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_comp_ring);
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_data_ring);
	}
}

int ath12k_wifi7_dp_tx_ring_setup(struct ath12k_base *ab)
{
	int i, tx_comp_ring_num;
	struct ath12k_dp *dp = ab->dp;
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	int ret;
	u8 rbm_id;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		map = ab->hal.tcl_to_cmp_rbm_map;
		tx_comp_ring_num = map[i].cmp_ring_num;
		rbm_id = map[i].rbm_id;

		ret = ath12k_dp_srng_setup(ab, &dp->tx_ring[i].tcl_data_ring,
					   HAL_TCL_DATA, i, 0,
					   DP_TCL_DATA_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_data ring (%d) :%d\n",
				    i, ret);
			goto err;
		}

		ath12k_hal_tx_config_rbm_mapping(ab, i, rbm_id, HAL_TCL_DATA);

		ret = ath12k_dp_srng_setup(ab, &dp->tx_ring[i].tcl_comp_ring,
					   HAL_WBM2SW_RELEASE, tx_comp_ring_num, 0,
					   DP_TX_COMP_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_comp ring (%d) :%d\n",
				    tx_comp_ring_num, ret);
			goto err;
		}
	}

	return 0;

err:
	ath12k_wifi7_dp_tx_ring_cleanup(ab);
	return ret;
}
