/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DP_MON_FILTER_H_
#define _DP_MON_FILTER_H_

#include "debug.h"
#include "pktlog.h"
/**
 * struct dp_mon_rx_filter - Monitor RX TLV filter
 * @tlv_filter: Rx ring TLV filter
 * @valid: enable/disable TLV filter
 */
struct dp_mon_rx_filter {
	struct htt_rx_ring_tlv_filter rx_tlv_filter;
	bool valid;
};

enum dp_mon_filter_mode {
	DP_MON_FILTER_STATS_MODE,
	DP_MON_FILTER_MONITOR_MODE,
	DP_MON_FILTER_NRP_MODE,
	DP_MON_FILTER_PKTLOG_FULL_MODE,
	DP_MON_FILTER_PKTLOG_LITE_MODE,
	DP_MON_FILTER_MAX_MODE
};

enum dp_mon_filter_srng_type {
	DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF,
	DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS,
	DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF,
	DP_MON_FILTER_SRNG_TYPE_RXMON_DEST,
	DP_MON_FILTER_SRNG_TYPE_MAX
};

enum dp_mpdu_filter_category {
	DP_MPDU_FILTER_CATEGORY_FP,
	DP_MPDU_FILTER_CATEGORY_MD,
	DP_MPDU_FILTER_CATEGORY_MO,
	DP_MPDU_FILTER_CATEGORY_FP_MO
};

int ath12k_dp_mon_rx_filter_alloc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_filter_free(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_stats_config_filter(struct ath12k_pdev_dp *dp_pdev,
					  enum dp_mon_stats_mode stats_mode,
					  bool enable);
int ath12k_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev,
					     bool enable);
void
ath12k_dp_mon_rx_setup_mon_mode_filter(struct ath12k_dp *dp,
				       struct htt_rx_ring_tlv_filter *tlv_filter,
				       u32 rx_mon_tlv_filter_flags);
int ath12k_dp_mon_rx_config_filters(struct ath12k_dp *dp,
				    struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_filter_srng_type srng_type,
				    struct htt_rx_ring_tlv_filter *rx_tlv_filter);
void ath12k_dp_mon_rx_prepare_filter(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_filter_srng_type srng_type,
				     struct dp_mon_rx_filter *rx_mon_filter);
void ath12k_dp_mon_rx_display_filters(struct ath12k_dp *dp,
				      enum dp_mon_filter_mode mode,
				      struct dp_mon_rx_filter *rx_filter);
void ath12k_dp_mon_rx_nrp_config_filter(struct ath12k_pdev_dp *dp_pdev,
					bool enable);
void ath12k_dp_mon_rx_wmask_subscribe(void *ptr,
				      struct htt_rx_ring_tlv_filter *tlv_filter);
void ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_mon_rx_enable_packet_filters(void *ptr,
					    struct htt_rx_ring_tlv_filter *tlv_filter);
void ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(u32 *ptr, u16 filter);
#endif
