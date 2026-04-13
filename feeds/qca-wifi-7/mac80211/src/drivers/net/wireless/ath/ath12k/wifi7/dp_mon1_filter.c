// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../dp_mon.h"
#include "dp_mon1.h"
#include "../dp_mon_filter.h"

void ath12k_wifi7_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type;
	u32 rx_mon_tlv_filter_flags = 0;

	/* Configure buffer ring */
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_DEST_RING;
	rx_filter.valid = true;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_setup_mon_mode_filter(dp,
					       rx_tlv_filter,
					       rx_mon_tlv_filter_flags);
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;

	/* Configure status ring */
	memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
	rx_mon_tlv_filter_flags = HTT_RX_MON_FILTER_TLV_FLAGS_MON_STATUS_RING;
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	rx_filter.valid = true;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_setup_mon_mode_filter(dp,
					       rx_tlv_filter,
					       rx_mon_tlv_filter_flags);
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

void ath12k_wifi7_dp_mon_rx_mon_mode_reset_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct dp_mon_rx_filter rx_filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type;

	 /* reset buffer ring */
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;

	/* reset status ring */
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	ath12k_dp_mon_rx_display_filters(dp, mode, &rx_filter);
	dp_mon_pdev->rx_filter[mode][srng_type] = rx_filter;
}

int ath12k_wifi7_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_rx_filter rx_filter = {0};
	struct htt_rx_ring_tlv_filter *rx_tlv_filter;
	int ret = 0;
	enum dp_mon_filter_srng_type srng_type;

	/* update destination ring */
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_prepare_filter(dp, dp_pdev, srng_type, &rx_filter);
	if (rx_filter.valid)
		rx_tlv_filter->rxmon_disable = false;
	else
		rx_tlv_filter->rxmon_disable = true;
	ret = ath12k_dp_mon_rx_config_filters(dp, dp_pdev, srng_type,
						  rx_tlv_filter);
	if (ret)
		return ret;

	/* update status ring */
	memset(&rx_filter, 0, sizeof(struct dp_mon_rx_filter));
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	rx_tlv_filter = &rx_filter.rx_tlv_filter;
	ath12k_dp_mon_rx_prepare_filter(dp, dp_pdev, srng_type, &rx_filter);
	if (rx_filter.valid)
		rx_tlv_filter->rxmon_disable = false;
	else
		rx_tlv_filter->rxmon_disable = true;

	ret = ath12k_dp_mon_rx_config_filters(dp, dp_pdev, srng_type,
						  rx_tlv_filter);
	if (ret)
		return ret;

	return 0;
}
