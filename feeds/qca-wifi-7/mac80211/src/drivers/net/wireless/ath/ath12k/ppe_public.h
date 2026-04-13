// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_PPE_PUBLIC_H
#define ATH12K_PPE_PUBLIC_H

void ath12k_ppeds_set_tcl_prod_idx_v2(int ds_node_id, u16 tcl_prod_idx);
u16 ath12k_ppeds_get_tcl_cons_idx_v2(int ds_node_id);
void ath12k_ppeds_set_reo_cons_idx_v2(int ds_node_id, u16 reo_cons_idx);
u16 ath12k_ppeds_get_reo_prod_idx_v2(int ds_node_id);
void ath12k_ppeds_enable_srng_intr_v2(int ds_node_id, bool enable);

bool ath12k_ppeds_free_rx_desc_v2(struct ppe_ds_wlan_rxdesc_elem *arr,
				  struct ath12k_base *ab, int index,
				  u16 *idx_of_ab);

void ath12k_ppeds_release_rx_desc_v2(int ds_node_id,
				     struct ppe_ds_wlan_rxdesc_elem *arr,
				     u16 count);
void ath12k_ppeds_release_tx_desc_single_v2(int ds_node_id,
					    u32 cookie);
u32 ath12k_ppeds_get_batched_tx_desc_v2(int ds_node_id,
					struct ppe_ds_wlan_txdesc_elem *arr,
					u32 num_buff_req,
					u32 buff_size,
					u32 headroom);

bool
ath12k_dp_rx_ppeds_fse_add_flow_entry(struct ppe_drv_fse_rule_info *ppe_flow_info);

bool
ath12k_dp_rx_ppeds_fse_del_flow_entry(struct ppe_drv_fse_rule_info *ppe_flow_info);

#endif /* ATH12K_PPE_PUBLIC_H */
