/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_SPECTRAL_H
#define ATH12K_SPECTRAL_H

#include "../spectral_common.h"
#include "dbring.h"

/* enum ath12k_spectral_mode:
 *
 * @SPECTRAL_DISABLED: spectral mode is disabled
 * @SPECTRAL_BACKGROUND: hardware sends samples when it is not busy with
 *	something else.
 * @SPECTRAL_MANUAL: spectral scan is enabled, triggering for samples
 *	is performed manually.
 */
enum ath12k_spectral_mode {
	ATH12K_SPECTRAL_DISABLED = 0,
	ATH12K_SPECTRAL_BACKGROUND,
	ATH12K_SPECTRAL_MANUAL,
};

struct ath12k_spectral {
	struct ath12k_dbring rx_ring;
	/* Protects enabled */
	spinlock_t lock;
	struct rchan *rfs_scan;	/* relay(fs) channel for spectral scan */
	struct dentry *scan_ctl;
	struct dentry *scan_count;
	struct dentry *scan_bins;
	enum ath12k_spectral_mode mode;
	u16 count;
	u8 fft_size;
	bool enabled;
	bool is_primary;
	u32 ch_width;
	struct wmi_spectral_capabilities_event spectral_cap;
};

#ifdef CPTCFG_ATH12K_SPECTRAL

int ath12k_spectral_init(struct ath12k_base *ab);
void ath12k_spectral_deinit(struct ath12k_base *ab);
int ath12k_spectral_vif_stop(struct ath12k_link_vif *arvif);
void ath12k_spectral_reset_buffer(struct ath12k *ar);
enum ath12k_spectral_mode ath12k_spectral_get_mode(struct ath12k *ar);
struct ath12k_dbring *ath12k_spectral_get_dbring(struct ath12k *ar);

#else

static inline int ath12k_spectral_init(struct ath12k_base *ab)
{
	return 0;
}

static inline void ath12k_spectral_deinit(struct ath12k_base *ab)
{
}

static inline int ath12k_spectral_vif_stop(struct ath12k_link_vif *arvif)
{
	return 0;
}

static inline void ath12k_spectral_reset_buffer(struct ath12k *ar)
{
}

static inline
enum ath12k_spectral_mode ath12k_spectral_get_mode(struct ath12k *ar)
{
	return ATH12K_SPECTRAL_DISABLED;
}

static inline
struct ath12k_dbring *ath12k_spectral_get_dbring(struct ath12k *ar)
{
	return NULL;
}

#endif /* CPTCFG_ATH12K_SPECTRAL */
#endif /* ATH12K_SPECTRAL_H */
