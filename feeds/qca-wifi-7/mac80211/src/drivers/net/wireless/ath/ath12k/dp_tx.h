/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_TX_H
#define ATH12K_DP_TX_H

#include "core.h"

#define DP_SDWF_DEFINED_Q_PTID_MAX 2
#define DP_SDWF_DEFAULT_Q_PTID_MAX 2
#define DP_SDWF_TID_MAX 8

#define DP_SDWF_Q_MAX (DP_SDWF_DEFINED_Q_PTID_MAX * DP_SDWF_TID_MAX)
#define DP_SDWF_DEFAULT_Q_MAX (DP_SDWF_DEFAULT_Q_PTID_MAX * DP_SDWF_TID_MAX)

#define DP_GET_HW_LINK_ID_FRM_PPDU_ID(PPDU_ID, LINK_ID_OFFSET, LINK_ID_BITS) \
	(((PPDU_ID) >> (LINK_ID_OFFSET)) & ((1 << (LINK_ID_BITS)) - 1))

struct ath12k_ppeds_desc_params {
	unsigned int num_ppeds_desc;
	unsigned int ppeds_hotlist_len;
};

struct ath12k_dp_htt_wbm_tx_status {
	bool acked;
	s8 ack_rssi;
};

static inline bool ath12k_dp_tx_completion_valid(struct hal_wbm_release_ring *desc)
{
	struct htt_tx_wbm_completion *status_desc;

	if (FIELD_GET(HAL_WBM_COMPL_TX_INFO0_REL_SRC_MODULE, desc->info0) ==
			HAL_WBM_REL_SRC_MODULE_FW) {
		status_desc = (struct htt_tx_wbm_completion *)desc;

		/* Dont consider HTT_TX_COMP_STATUS_MEC_NOTIFY */
		if (FIELD_GET(HTT_TX_WBM_COMP_INFO0_STATUS, status_desc->info0) ==
				HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY)
			return false;
	}
	return true;
}

void ath12k_dp_tx_put_bank_profile(struct ath12k_dp *dp, u8 bank_id);

void ath12k_dp_tx_encap_nwifi(struct sk_buff *skb);
void *ath12k_dp_metadata_align_skb(struct sk_buff *skb, u8 tail_len);
int ath12k_dp_tx_align_payload(struct ath12k_dp *dp, struct sk_buff **pskb);
void ath12k_dp_tx_release_txbuf(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc,
				u8 pool_id);
struct ath12k_tx_desc_info *ath12k_dp_tx_assign_buffer(struct ath12k_dp *dp,
						       u8 pool_id);
int ath12k_dp_tx_htt_h2t_vdev_stats_ol_req(struct ath12k *ar, u64 reset_bitmask);
u8 ath12k_dp_get_link_id(struct ath12k_pdev_dp *dp_pdev,
			 struct hal_tx_status *ts, struct ath12k_dp_peer *peer);
void ath12k_dp_tx_update_peer_basic_stats(struct ath12k_dp_peer *peer,
					  u32 msdu_len, u8 tx_status,
					  u8 link_id, int ring_id);
void ath12k_dp_tx_comp_update_peer_stats(struct ath12k_dp_peer *peer,
					 struct hal_tx_status *ts, int ring_id,
					 u16 tx_desc_flags);
int ath12k_sdwf_reinject_handler(struct ath12k_base *ab, struct sk_buff *skb,
				 struct htt_tx_wbm_completion *status_desc, u8 mac_id);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_ppeds_tx_completion_handler(struct ath12k_base *ab, int ring_id);
struct ath12k_ppeds_tx_desc_info *
ath12k_dp_ppeds_tx_assign_desc_nolock(struct ath12k_dp *dp);
#endif
#endif
