/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DBRING_H
#define ATH12K_DBRING_H

#include <linux/types.h>
#include <linux/idr.h>
#include <linux/spinlock.h>
#include "dp.h"

struct ath12k_dbring_element {
	dma_addr_t paddr;
	u8 payload[];
};

struct ath12k_dbring_data {
	void *data;
	u32 data_sz;
	struct ath12k_wmi_dma_buf_release_meta_data_params meta;
	struct ath12k_dbring_element *buff;
	u32 buf_id;
};

struct ath12k_dbring_buf_release_event {
	struct ath12k_wmi_dma_buf_release_fixed_params fixed;
	const struct ath12k_wmi_dma_buf_release_entry_params *buf_entry;
	const struct ath12k_wmi_dma_buf_release_meta_data_params *meta_data;
	u32 num_buf_entry;
	u32 num_meta;
};

struct wmi_pdev_sscan_fw_param_event {
	struct ath12k_wmi_pdev_sscan_fw_cmd_fixed_param fixed;
	struct ath12k_wmi_pdev_sscan_fft_bin_index *bin;
	struct ath12k_wmi_pdev_sscan_chan_info ch_info;
	struct ath12k_wmi_pdev_sscan_per_detector_info *det_info;
};

struct wmi_spectral_capabilities_event {
	struct ath12k_wmi_spectral_scan_bw_capabilities *sscan_bw_caps;
	struct ath12k_wmi_spectral_fft_size_capabilities *fft_size_caps;
	u32 num_bw_caps_entry;
	u32 num_fft_size_caps_entry;
};

struct ath12k_dbring_cap {
	u32 pdev_id;
	enum wmi_direct_buffer_module id;
	u32 min_elem;
	u32 min_buf_sz;
	u32 min_buf_align;
};

struct ath12k_dbring {
	struct dp_srng refill_srng;
	struct idr bufs_idr;
	/* Protects bufs_idr */
	spinlock_t idr_lock;
	dma_addr_t tp_addr;
	dma_addr_t hp_addr;
	int bufs_max;
	u32 pdev_id;
	u32 buf_sz;
	u32 buf_align;
	u32 num_resp_per_event;
	u32 event_timeout_ms;
	int (*handler)(struct ath12k *ar, struct ath12k_dbring_data *data);
};

int ath12k_dbring_set_cfg(struct ath12k *ar,
			  struct ath12k_dbring *ring,
			  u32 num_resp_per_event,
			  u32 event_timeout_ms,
			  int (*handler)(struct ath12k *,
					 struct ath12k_dbring_data *));
int ath12k_dbring_bufs_replenish(struct ath12k *ar,
				 struct ath12k_dbring *ring,
				 struct ath12k_dbring_element *buff,
				 enum wmi_direct_buffer_module id,
				 gfp_t gfp);
int ath12k_dbring_wmi_cfg_setup(struct ath12k *ar,
				struct ath12k_dbring *ring,
				enum wmi_direct_buffer_module id);
int ath12k_dbring_buf_setup(struct ath12k *ar,
			    struct ath12k_dbring *ring,
			    struct ath12k_dbring_cap *db_cap);
int ath12k_dbring_srng_setup(struct ath12k *ar, struct ath12k_dbring *ring,
			     int ring_num, int num_entries);
int ath12k_dbring_buffer_release_event(struct ath12k_base *ab,
				       struct ath12k_dbring_buf_release_event *ev);
int ath12k_dbring_get_cap(struct ath12k_base *ab,
			  u8 pdev_idx,
			  enum wmi_direct_buffer_module id,
			  struct ath12k_dbring_cap *db_cap);
void ath12k_dbring_srng_cleanup(struct ath12k *ar, struct ath12k_dbring *ring);
void ath12k_dbring_buf_cleanup(struct ath12k *ar, struct ath12k_dbring *ring);
int ath12k_dbring_validate_buffer(struct ath12k *ar, void *data, u32 size);
#endif /* ATH12K_DBRING_H */
