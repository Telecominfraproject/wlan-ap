/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_TX_WIFI7_H
#define ATH12K_DP_TX_WIFI7_H

#define DP_TX_SFE_BUFFER_SIZE           256

int ath12k_wifi7_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget);
enum ath12k_dp_tx_enq_error
ath12k_wifi7_dp_tx(struct ath12k_pdev_dp *dp_pdev,
		   struct ath12k_link_vif *arvif,
		   struct sk_buff *skb, bool gsn_valid, int mcbc_gsn,
		   bool is_mcast, struct ath12k_link_sta *arsta,
		   u8 ring_id, u32 qos_nw_delay);
enum ath12k_dp_tx_enq_error
ath12k_wifi7_dp_tx_fast(struct ath12k_pdev_dp *dp_pdev,
			struct ath12k_link_vif *arvif,
			struct sk_buff *skb,
			u32 qos_nw_delay);
u32 ath12k_wifi7_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_link_vif *arvif, bool vdev_id_check_en);
bool ath12k_mac_tx_check_max_limit(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *skb);
int ath12k_wifi7_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb, struct ath12k_link_sta *arsta);
void ath12k_wifi7_dp_tx_status_parse(struct ath12k_base *ab,
				     struct hal_wbm_completion_ring_tx *desc,
				     struct hal_tx_status *ts);
int ath12k_wifi7_dp_tx_ring_setup(struct ath12k_base *ab);
void ath12k_wifi7_dp_tx_ring_cleanup(struct ath12k_base *ab);
#endif
