// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "hal_mon.h"
#include "hal_mon_qcn9274.h"

u32
ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get_qcn9274(void)
{
	return RX_MON_PPDU_END_USER_STATS_WMASK_QCN9274;
}

static __always_inline void
ath12k_wifi7_hal_mon_handle_ofdma_info_qcn9274(u32 *info,
					       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 = info[12];
	rx_user_status->ul_ofdma_user_v0_word1 = info[13];
}

static __always_inline void
ath12k_wifi7_hal_mon_populate_byte_count_qcn9274(u32 *info,
						 struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		u32_get_bits(info[8],
			     HAL_RX_PPDU_END_USER_STATS_INFO8_MPDU_OK_BYTE_CNT_QCN9274);
	rx_user_status->mpdu_err_byte_count =
		u32_get_bits(info[9],
			     HAL_RX_PPDU_END_USER_STATS_INFO9_MPDU_ERR_BYTE_CNT_QCN9274);
}

static __always_inline void
ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_qcn9274(const void *tlv_data,
						       u32 userid,
						       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_ppdu_end_user_stats_qcn9274 *ppdu_eu_stats =
		(struct hal_rx_mon_ppdu_end_user_stats_qcn9274 *)tlv_data;
	u32 tid_bitmap;
	u32 info[14];

	info[0] = __le32_to_cpu(ppdu_eu_stats->info0);
	info[1] = __le32_to_cpu(ppdu_eu_stats->info1);
	info[2] = __le32_to_cpu(ppdu_eu_stats->info2);
	info[3] = __le32_to_cpu(ppdu_eu_stats->info3);
	info[4] = __le32_to_cpu(ppdu_eu_stats->info4);
	info[5] = __le32_to_cpu(ppdu_eu_stats->info5);
	info[6] = __le32_to_cpu(ppdu_eu_stats->info6);
	info[7] = __le32_to_cpu(ppdu_eu_stats->info7);
	info[8] = __le32_to_cpu(ppdu_eu_stats->info8);
	info[9] = __le32_to_cpu(ppdu_eu_stats->info9);
	info[10] = __le32_to_cpu(ppdu_eu_stats->info10);
	info[11] = __le32_to_cpu(ppdu_eu_stats->info11);
	info[12] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref);
	info[13] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref_ext);

	ppdu_info->num_mpdu_fcs_err =
		u32_get_bits(info[1],
			      HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR_QCN9274);
	ppdu_info->peer_id =
		u32_get_bits(info[1],
			     HAL_RX_PPDU_END_USER_STATS_INFO1_PEER_ID_QCN9274);
	ppdu_info->fc_valid =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID_QCN9274);
	ppdu_info->preamble_type =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE_QCN9274);
	ppdu_info->num_mpdu_fcs_ok =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK_QCN9274);
	ppdu_info->ast_index =
		u32_get_bits(info[3],
			     HAL_RX_PPDU_END_USER_STATS_INFO3_AST_INDEX_QCN9274);
	ppdu_info->tcp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_TCP_MSDU_CNT_QCN9274);
	ppdu_info->udp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_UDP_MSDU_CNT_QCN9274);
	ppdu_info->other_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_OTHER_MSDU_CNT_QCN9274);
	ppdu_info->tcp_ack_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_TCP_ACK_MSDU_CNT_QCN9274);
	tid_bitmap = u32_get_bits(info[7],
				  HAL_RX_PPDU_END_USER_STATS_INFO7_TID_BITMAP_QCN9274);
	ppdu_info->tid = ffs(tid_bitmap) - 1;
	ppdu_info->mpdu_retry_cnt =
		u32_get_bits(info[11],
			     HAL_RX_PPDU_END_USER_STATS_INFO11_MPDU_RETRY_CNT_QCN9274);
	switch (ppdu_info->preamble_type) {
	case HAL_RX_PREAMBLE_11N:
		ppdu_info->ht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AC:
		ppdu_info->vht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AX:
		ppdu_info->he_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11BE:
		ppdu_info->is_eht = true;
		break;
	default:
		break;
	}

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];

		ath12k_wifi7_hal_mon_handle_ofdma_info_qcn9274(info,
							       rxuser_stats);
		ath12k_wifi7_hal_mon_populate_byte_count_qcn9274(info,
								 rxuser_stats);
		rxuser_stats->retried_msdu_count =
			u32_get_bits(info[10],
				     HAL_RX_PPDU_END_USER_STATS_INFO10_MSDU_RETRY_CNT_QCN9274);
	}
}

static __always_inline void
ath12k_wifi7_hal_mon_handle_ofdma_info_compact_qcn9274(u32 *info,
						       struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->ul_ofdma_user_v0_word0 = info[12];
	rx_user_status->ul_ofdma_user_v0_word1 = info[13];
}

static __always_inline void
ath12k_wifi7_hal_mon_populate_byte_count_compact_qcn9274(u32 *info,
							 struct hal_rx_user_status *rx_user_status)
{
	rx_user_status->mpdu_ok_byte_count =
		u32_get_bits(info[8],
			     HAL_RX_PPDU_END_USER_STATS_INFO8_MPDU_OK_BYTE_CNT_CMPCT_QCN9274);
	rx_user_status->mpdu_err_byte_count =
		u32_get_bits(info[9],
			     HAL_RX_PPDU_END_USER_STATS_INFO9_MPDU_ERR_BYTE_CNT_CMPCT_QCN9274);
}

static __always_inline void
ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_compact_qcn9274(const void *tlv_data,
							       u32 userid,
							       struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_ppdu_end_user_stats_compact_qcn9274 *ppdu_eu_stats =
		(struct hal_rx_mon_ppdu_end_user_stats_compact_qcn9274 *)tlv_data;
	u32 tid_bitmap;
	u32 info[14];

	info[0] = __le32_to_cpu(ppdu_eu_stats->info0);
	info[1] = __le32_to_cpu(ppdu_eu_stats->info1);
	info[2] = __le32_to_cpu(ppdu_eu_stats->info2);
	info[3] = __le32_to_cpu(ppdu_eu_stats->info3);
	info[4] = __le32_to_cpu(ppdu_eu_stats->info4);
	info[5] = __le32_to_cpu(ppdu_eu_stats->info5);
	info[6] = __le32_to_cpu(ppdu_eu_stats->info6);
	info[7] = __le32_to_cpu(ppdu_eu_stats->info7);
	info[8] = __le32_to_cpu(ppdu_eu_stats->info8);
	info[9] = __le32_to_cpu(ppdu_eu_stats->info9);
	info[10] = __le32_to_cpu(ppdu_eu_stats->info10);
	info[11] = __le32_to_cpu(ppdu_eu_stats->info11);
	info[12] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref);
	info[13] = __le32_to_cpu(ppdu_eu_stats->usr_resp_ref_ext);

	ppdu_info->num_mpdu_fcs_err =
		u32_get_bits(info[1],
			      HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR_CMPCT_QCN9274);
	ppdu_info->peer_id =
		u32_get_bits(info[1],
			     HAL_RX_PPDU_END_USER_STATS_INFO1_PEER_ID_CMPCT_QCN9274);
	ppdu_info->fc_valid =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID_CMPCT_QCN9274);
	ppdu_info->preamble_type =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE_CMPCT_QCN9274);
	ppdu_info->num_mpdu_fcs_ok =
		u32_get_bits(info[2],
			     HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK_CMPCT_QCN9274);
	ppdu_info->ast_index =
		u32_get_bits(info[3],
			     HAL_RX_PPDU_END_USER_STATS_INFO3_AST_INDEX_CMPCT_QCN9274);
	ppdu_info->tcp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_TCP_MSDU_CNT_CMPCT_QCN9274);
	ppdu_info->udp_msdu_count =
		u32_get_bits(info[5],
			     HAL_RX_PPDU_END_USER_STATS_INFO5_UDP_MSDU_CNT_CMPCT_QCN9274);
	ppdu_info->other_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_OTHER_MSDU_CNT_CMPCT_QCN9274);
	ppdu_info->tcp_ack_msdu_count =
		u32_get_bits(info[6],
			     HAL_RX_PPDU_END_USER_STATS_INFO6_TCP_ACK_MSDU_CNT_CMPCT_QCN9274);
	tid_bitmap = u32_get_bits(info[7],
				  HAL_RX_PPDU_END_USER_STATS_INFO7_TID_BITMAP_CMPCT_QCN9274);
	ppdu_info->tid = ffs(tid_bitmap) - 1;
	ppdu_info->mpdu_retry_cnt =
		u32_get_bits(info[11],
			     HAL_RX_PPDU_END_USER_STATS_INFO11_MPDU_RETRY_CNT_CMPCT_QCN9274);
	switch (ppdu_info->preamble_type) {
	case HAL_RX_PREAMBLE_11N:
		ppdu_info->ht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AC:
		ppdu_info->vht_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11AX:
		ppdu_info->he_flags = 1;
		break;
	case HAL_RX_PREAMBLE_11BE:
		ppdu_info->is_eht = true;
		break;
	default:
		break;
	}

	if (userid < HAL_MAX_UL_MU_USERS) {
		struct hal_rx_user_status *rxuser_stats =
				&ppdu_info->userstats[userid];

		ath12k_wifi7_hal_mon_handle_ofdma_info_compact_qcn9274(info,
								       rxuser_stats);
		ath12k_wifi7_hal_mon_populate_byte_count_compact_qcn9274(info,
									 rxuser_stats);
		rxuser_stats->retried_msdu_count =
			u32_get_bits(info[10],
				     HAL_RX_PPDU_END_USER_STATS_INFO10_MSDU_RETRY_CNT_CMPCT_QCN9274);
	}
}

void
ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_parse_qcn9274(const void *tlv_data, u32 userid,
							 struct hal_rx_mon_ppdu_info *ppdu_info,
							 u32 tlv_len)
{
	if (likely(tlv_len < HAL_MON_RX_PPDU_EU_STATS_TLV_SIZE_QCN9274))
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_compact_qcn9274(tlv_data,
									       userid,
									       ppdu_info);
	else
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_get_qcn9274(tlv_data,
								       userid,
								       ppdu_info);
}
const struct hal_mon_ops hal_qcn9274_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get_qcn9274,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_parse,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_parse,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_parse_qcn9274,
	.rx_desc_get_msdu_payload =
		ath12k_wifi7_hal_mon_rx_desc_get_msdu_payload,
};

const struct hal_mon_ops hal_ipq5332_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_parse,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_parse,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_parse,
	.rx_desc_get_msdu_payload =
		ath12k_wifi7_hal_mon_rx_desc_get_msdu_payload,
};

const struct hal_mon_ops hal_ipq5424_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_parse,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_parse,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_parse,
	.rx_desc_get_msdu_payload =
		ath12k_wifi7_hal_mon_rx_desc_get_msdu_payload,
};

const struct hal_mon_ops hal_qcn6432_mon_ops = {
	.tx_parse_status_tlv = ath12k_wifi7_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user= ath12k_wifi7_hal_mon_tx_status_get_num_user,
	.get_mon_mpdu_start_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get,
	.get_mon_mpdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_parse,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_parse,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_parse,
	.rx_desc_get_msdu_payload =
		ath12k_wifi7_hal_mon_rx_desc_get_msdu_payload,
};
