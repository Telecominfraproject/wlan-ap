/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_WMI_WIFI7_H
#define ATH12K_WMI_WIFI7_H

void ath12k_wifi7_wmi_init_qcn9274(struct ath12k_base *ab,
				   struct ath12k_wmi_resource_config_arg *config);
void ath12k_wifi7_wmi_init_wcn7850(struct ath12k_base *ab,
				   struct ath12k_wmi_resource_config_arg *config);

#endif
