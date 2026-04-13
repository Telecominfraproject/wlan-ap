/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

int ath12k_wifi7_dp_mon_rx_srng_setup(struct ath12k_dp *dp);
void ath12k_wifi7_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp);
int ath12k_wifi7_dp_mon_rx_buf_setup(struct ath12k_dp *dp);
void ath12k_wifi7_dp_mon_rx_buf_free(struct ath12k_dp *dp);
int ath12k_wifi7_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp);
int ath12k_wifi7_dp_mon_rx_quad_ring_process(struct ath12k_pdev_dp *pdev_dp,
					     int mac_id, struct napi_struct *napi,
					     int *budget);
void ath12k_wifi7_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi7_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi7_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *pdev_dp);
void ath12k_wifi7_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi7_dp_mon_rx_mon_mode_reset_filter(struct ath12k_pdev_dp *dp_pdev);
