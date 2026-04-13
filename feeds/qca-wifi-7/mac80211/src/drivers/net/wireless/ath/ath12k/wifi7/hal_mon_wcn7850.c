// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "hal_mon.h"

const struct hal_mon_ops hal_wcn7850_mon_ops = {
	.rx_mpdu_start_info_get =
		ath12k_wifi7_hal_mon_rx_mpdu_start_info_parse,
	.rx_msdu_end_info_get =
		ath12k_wifi7_hal_mon_rx_msdu_end_info_parse,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_parse,
};
