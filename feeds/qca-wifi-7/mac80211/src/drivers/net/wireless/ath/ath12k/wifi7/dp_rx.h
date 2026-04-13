/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_RX_WIFI7_H
#define ATH12K_DP_RX_WIFI7_H

#include "../core.h"
#include "../dp_rx.h"
#include "hal_desc.h"
#include "hal.h"

struct dp_rx_fse {
	struct hal_rx_fse *hal_fse;
	u32 flow_hash;
	u32 flow_id;
	u8 reo_indication;
	bool is_valid;
};

int ath12k_wifi7_dp_reo_cmd_send(struct ath12k_base *ab, struct ath12k_dp_rx_tid *rx_tid,
				 enum hal_reo_cmd_type type,
				 struct ath12k_hal_reo_cmd *cmd,
				 void (*cb)(struct ath12k_dp *dp, void *ctx,
					    enum hal_reo_cmd_status status));
int ath12k_wifi7_dp_rx_process_wbm_err(struct ath12k_dp *dp,
				       struct napi_struct *napi, int budget);
int ath12k_wifi7_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget);
int ath12k_wifi7_dp_rx_process(struct ath12k_dp *dp, int mac_id,
			       struct napi_struct *napi,
			       int budget);
void ath12k_wifi7_dp_rx_peer_tid_delete(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer, u8 tid);
bool ath12k_wifi7_dp_rx_h_ppdu(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_rx_status *rx_status,
			       struct rx_tlv_info_1 *tlv_info,
			       u8 err_rel_src);
int ath12k_wifi7_dp_reo_cache_flush(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid);
int ath12k_wifi7_peer_rx_tid_reo_update(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer,
					struct ath12k_dp_rx_tid *rx_tid,
					u32 ba_win_sz, u16 ssn,
					bool update_ssn);
void ath12k_wifi7_peer_rx_tid_qref_setup(struct ath12k_base *ab, u16 peer_id,
					 u16 tid, dma_addr_t paddr);
int ath12k_wifi7_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action);
void ath12k_wifi7_dp_rx_process_reo_status(struct ath12k_dp *dp);

int ath12k_wifi7_dp_rx_peer_tid_setup(struct ath12k *ar, const u8 *peer_mac, int vdev_id,
				      u8 tid, u32 ba_win_sz, u16 ssn,
				      enum hal_pn_type pn_type);
void ath12k_wifi7_dp_setup_pn_check_reo_cmd(struct ath12k_hal_reo_cmd *cmd,
					    struct ath12k_dp_rx_tid *rx_tid,
					    u32 cipher, enum set_key_cmd key_cmd);
int ath12k_wifi7_dp_alloc_reo_qdesc(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
				    enum hal_pn_type pn_type,
				    struct hal_rx_reo_queue **addr_aligned);
int ath12k_wifi7_dp_rxdma_ring_sel_config_qcn9274(struct ath12k_base *ab);
int ath12k_wifi7_dp_rxdma_ring_sel_config_wcn7850(struct ath12k_base *ab);
int ath12k_wifi7_dp_rx_fst_attach(struct ath12k_dp *dp, struct dp_rx_fst *fst);
void ath12k_wifi7_dp_rx_fst_detach(struct ath12k_dp *dp, struct dp_rx_fst *fst);

static inline
void ath12k_wifi7_dp_extract_rx_spd_data(struct ath12k_hal *hal,
					 struct hal_rx_spd_data *rx_info,
					 struct hal_rx_desc *rx_desc, int set)
{
	hal->hal_ops->extract_rx_spd_data(rx_info, rx_desc, set);
}

static inline
void ath12k_wifi7_dp_extract_rx_desc_data(struct ath12k_dp *dp,
					  struct hal_rx_desc_data *rx_desc_data,
					  struct hal_rx_desc *rx_desc,
					  struct hal_rx_desc *ldesc)
{
	dp->hw_params->hal_ops->extract_rx_desc_data(rx_desc_data, rx_desc, ldesc);
}

void ath12k_wifi7_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					struct rx_flow_info *flow_info);
int ath12k_wifi7_dp_rx_flow_add_entry(struct ath12k_dp *dp,
				      struct rx_flow_info *flow_info);
int ath12k_wifi7_dp_rx_flow_delete_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info);
int ath12k_wifi7_dp_rx_flow_delete_all_entries(struct ath12k_dp *dp);
ssize_t ath12k_wifi7_dp_dump_fst_table(struct ath12k_dp *dp, char *buf, int size);
int ath12k_wifi7_dp_peer_migrate_reo_cmd(struct ath12k_dp *dp,
					 struct ath12k_dp_link_peer *peer,
					 u16 peer_id, u8 chip_id);
void ath12k_dp_rx_tid_del_func(struct ath12k_dp *dp, void *ctx,
			       enum hal_reo_cmd_status status);
void ath12k_wifi7_dp_rx_ring_free(struct ath12k_base *ab);
int ath12k_wifi7_dp_rx_ring_setup(struct ath12k_base *ab);
#endif
