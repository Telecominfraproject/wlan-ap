/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef ATH11K_DEBUG_NSS_H
#define ATH11K_DEBUG_NSS_H

#include <nss_api_if.h>
#include <nss_cmn.h>

#define ATH11K_NSS_MPATH_DUMP_TIMEOUT (2 * HZ)

struct ath11k_vif;
extern enum nss_wifi_mesh_mpp_learning_mode mpp_mode;
extern struct list_head mesh_vaps;
struct ath11k_nss_dbg_priv_data {
	struct ath11k_vif *arvif;
	char *buf;
	unsigned int len;
};

#ifdef CPTCFG_MAC80211_DEBUGFS
void ath11k_debugfs_nss_mesh_vap_create(struct ath11k_vif *arvif);
void ath11k_debugfs_nss_soc_create(struct ath11k_base *ab);
#else
static inline void ath11k_debugfs_nss_mesh_vap_create(struct ath11k_vif *arvif)
{
}
static inline void ath11k_debugfs_nss_soc_create(struct ath11k_base *ab)
{
}
#endif

#endif
