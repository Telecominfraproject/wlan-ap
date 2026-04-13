/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_WIFI7_H
#define ATH12K_DP_WIFI7_H

#include "../dp_cmn.h"
#include "hw.h"


struct ath12k_base;
struct ath12k_dp;

struct ath12k_dp *ath12k_wifi7_dp_init(struct ath12k_base *ab);
void ath12k_wifi7_dp_deinit(struct ath12k_dp *dp);
int ath12k_wifi7_dp_pdev_alloc(struct ath12k_base *ab);
void ath12k_wifi7_dp_pdev_free(struct ath12k_base *ab);
#endif
