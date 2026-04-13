// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <crypto/hash.h>
#include "core.h"
#include "dp_tx.h"
#include "hif.h"
#include "hal.h"
#include "debug.h"
#include "dp_rx.h"
#include "peer.h"
#include "dp_mon.h"
#include "dp_cmn.h"
#include "debugfs.h"
#include "dp_stats.h"
#include "dp_peer.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif

/* DSCP TID mapping as per RFC 8325
 * ===============================
 * DSCP         User Priority (UP)
 * ===============================
 * 56           7
 * 48           7
 * 46           6
 * 44           6
 * 40           5
 * 34, 36, 38   4
 * 32           4
 * 26, 28, 30   4
 * 24           4
 * 18, 20, 22   3
 * 16           0
 * 10, 12, 14   0
 * 0            0
 * 8            1
 */

u8 ath12k_default_dscp_tid_map[DSCP_TID_MAP_TBL_ENTRY_SIZE] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 1, 0, 1, 0, 1,
	0, 2, 3, 2, 3, 2, 3, 2,
	4, 3, 4, 3, 4, 3, 4, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 6, 5, 6, 5,
	7, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7,
};
EXPORT_SYMBOL(ath12k_default_dscp_tid_map);

/*
 * TODO: fix this
 */
int ath12k_wifi7_dp_rx_peer_tid_setup(struct ath12k *ar, const u8 *peer_mac, int vdev_id,
				      u8 tid, u32 ba_win_sz, u16 ssn,
				      enum hal_pn_type pn_type);

enum ath12k_dp_desc_type {
	ATH12K_DP_TX_DESC,
	ATH12K_DP_RX_DESC,
	ATH12K_DP_PPEDS_TX_DESC,
};

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_dp_ppe_rxole_rxdma_cfg(struct ath12k_base *ab)
{
	struct ath12k_dp_htt_rxdma_ppe_cfg_param param = {0};
	int ret;

	param.override = 1;
	param.reo_dst_ind = HAL_REO2PPE_DST_IND;
	param.multi_buffer_msdu_override_en = 0;

	/* Override use_ppe to 0 in RxOLE for the following cases */
	param.intra_bss_override = 0;
	param.decap_raw_override = 1;
	param.decap_nwifi_override = 1;
	param.ip_frag_override = 1;

	ret = ath12k_dp_rx_htt_rxdma_rxole_ppe_cfg_set(ab, &param);
	if (ret)
		ath12k_err(ab, "RxOLE and RxDMA PPE config failed %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_ppe_rxole_rxdma_cfg);
#endif

void ath12k_dp_peer_cleanup(struct ath12k *ar, int vdev_id, const u8 *addr)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	/* TODO: Any other peer specific DP cleanup */

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer || !peer->dp_peer) {
		ath12k_warn(ab, "failed to lookup peer %pM on vdev %d\n",
			    addr, vdev_id);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	if (!peer->primary_link) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	ath12k_dp_rx_peer_tid_cleanup(ar, peer);
	crypto_free_shash(peer->dp_peer->tfm_mmic);
	if (peer->primary_link)
		peer->dp_peer->primary_link_frag_setup = false;
	spin_unlock_bh(&dp->dp_lock);
}

int ath12k_dp_peer_setup(struct ath12k *ar, struct ath12k_link_vif *arvif, const u8 *addr)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	u32 reo_dest, vdev_id = arvif->vdev_id;
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	int ret = 0, tid;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct crypto_shash *tfm;

	/* NOTE: reo_dest ring id starts from 1 unlike mac_id which starts from 0 */
	reo_dest = ar->dp.mac_id + 1;
	ret = ath12k_wmi_set_peer_param(ar, addr, vdev_id,
					WMI_PEER_SET_DEFAULT_ROUTING,
					DP_RX_HASH_ENABLE | (reo_dest << 1));

	if (ret) {
		ath12k_warn(ab, "failed to set default routing %d peer :%pM vdev_id :%d\n",
			    ret, addr, vdev_id);
		return ret;
	}

	tfm = crypto_alloc_shash("michael_mic", 0, 0);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer) {
		ath12k_warn(ab, "failed to find the peer to del rx tid\n");
		spin_unlock_bh(&dp->dp_lock);
		ret = -ENOENT;
		goto free_shash;
	}

	sta = peer->sta;
	ahsta = ath12k_sta_to_ahsta(sta);
	if (peer->mlo && peer->link_id != ahsta->primary_link_id) {
		peer->primary_link = false;
		arvif->primary_sta_link = false;
		if (ar->dp.dp_hw) {
			spin_lock_bh(&ar->dp.dp_hw->peer_lock);
			if (peer->dp_peer->qos_stats_lvl ==
			    ATH12K_QOS_MULTI_LINK_STATS)
				ath12k_dp_qos_stats_alloc(ar, vif, peer);
			spin_unlock_bh(&ar->dp.dp_hw->peer_lock);
		}
		spin_unlock_bh(&dp->dp_lock);
		goto free_shash;
	}

	peer->primary_link = true;
	arvif->primary_sta_link = true;

	/* Allocate qos stats for primary link alone */
	ath12k_dp_qos_stats_alloc(ar, vif, peer);

	spin_unlock_bh(&dp->dp_lock);

	if (vif->type == NL80211_IFTYPE_STATION)
		ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(ar->ab, arvif, true);

	for (tid = 0; tid <= IEEE80211_NUM_TIDS; tid++) {
		ret = ath12k_wifi7_dp_rx_peer_tid_setup(ar, addr, vdev_id, tid, 1, 0,
							HAL_PN_TYPE_NONE);
		if (ret) {
			ath12k_warn(ab, "failed to setup rxd tid queue for tid %d: %d\n",
				    tid, ret);
			goto peer_clean;
		}
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer) {
		ath12k_warn(ab, "failed to find the peer to set up fragment info\n");
		ret = -ENOENT;
		spin_unlock_bh(&dp->dp_lock);
		goto free_shash;
	}

	ret = ath12k_dp_rx_peer_frag_setup(ar, peer, tfm);
	if (ret) {
		ath12k_warn(ab, "failed to setup rx defrag context\n");
		goto tid_clean;
	}

	spin_unlock_bh(&dp->dp_lock);

	/* TODO: Setup other peer specific resource used in data path */

	return 0;

peer_clean:
	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer) {
		spin_unlock_bh(&dp->dp_lock);
		ath12k_warn(ab, "failed to find the peer in err case of del rx tid\n");
		goto free_shash;
	}

tid_clean:
	for (tid--; tid >= 0; tid--)
		ath12k_dp_arch_rx_peer_tid_delete(ab->dp, ar, peer, tid);

	spin_unlock_bh(&dp->dp_lock);

free_shash:
	crypto_free_shash(tfm);
	return ret;
}

void ath12k_dp_srng_cleanup(struct ath12k_base *ab, struct dp_srng *ring)
{
	if (!ring->vaddr_unaligned)
		return;

	if (ring->cached)
		kfree(ring->vaddr_unaligned);
	else
		ath12k_hal_dma_free_coherent(ab->dev, ring->size, ring->vaddr_unaligned,
					      ring->paddr_unaligned);

	ring->vaddr_unaligned = NULL;
}
EXPORT_SYMBOL(ath12k_dp_srng_cleanup);

static int ath12k_dp_srng_find_ring_in_mask(int ring_num, const u8 *grp_mask, int size)
{
	int ext_group_num;
	u8 mask = 1 << ring_num;

	for (ext_group_num = 0; ext_group_num < size; ext_group_num++) {
		if (mask & grp_mask[ext_group_num])
			return ext_group_num;
	}

	return -ENOENT;
}

static int ath12k_dp_srng_calculate_msi_group(struct ath12k_base *ab,
					      enum hal_ring_type type, int ring_num)
{
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	struct ath12k_hw_ring_mask *ring_mask;
	const u8 *grp_mask;
	int i, size = ATH12K_EXT_IRQ_DP_NUM_VECTORS;

	ring_mask = ab->hw_params->ring_mask;
	switch (type) {
	case HAL_WBM2SW_RELEASE:
		if (ring_num == HAL_WBM2SW_REL_ERR_RING_NUM) {
			grp_mask = &ring_mask->rx_wbm_rel[0];
			ring_num = 0;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		} else if (ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM) {
			grp_mask = &ring_mask->wbm2sw6_ppeds_tx_cmpln[0];
			ring_num = 0;
#endif
		} else {
			map = ab->hal.tcl_to_cmp_rbm_map;
			for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
				if (ring_num == map[i].cmp_ring_num) {
					ring_num = i;
					break;
				}
			}

			grp_mask = &ring_mask->tx[0];
		}
		break;
	case HAL_REO_EXCEPTION:
		grp_mask = &ring_mask->rx_err[0];
		break;
	case HAL_REO_DST:
		grp_mask = &ring_mask->rx[0];
		break;
	case HAL_REO_STATUS:
		grp_mask = &ring_mask->reo_status[0];
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		grp_mask = &ab->hw_params->ring_mask->rx_mon_status[0];
		size = ATH12K_EXT_IRQ_GRP_NUM_MAX;
		break;
	case HAL_RXDMA_MONITOR_DST:
		grp_mask = &ring_mask->rx_mon_dest[0];
		break;
	case HAL_TX_MONITOR_DST:
		grp_mask = &ring_mask->tx_mon_dest[0];
		break;
	case HAL_RXDMA_BUF:
		grp_mask = &ring_mask->host2rxdma[0];
		break;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	case HAL_PPE2TCL:
		grp_mask = &ring_mask->ppe2tcl[0];
		break;
	case HAL_REO2PPE:
		grp_mask = &ring_mask->reo2ppe[0];
		break;
#endif
	case HAL_RXDMA_MONITOR_BUF:
		grp_mask = &ab->hw_params->ring_mask->host2rxmon[0];
		size = ATH12K_EXT_IRQ_GRP_NUM_MAX;
		break;

	case HAL_TCL_DATA:
	case HAL_TCL_CMD:
	case HAL_REO_CMD:
	case HAL_SW2WBM_RELEASE:
	case HAL_WBM_IDLE_LINK:
	case HAL_TCL_STATUS:
	case HAL_REO_REINJECT:
	case HAL_CE_SRC:
	case HAL_CE_DST:
	case HAL_CE_DST_STATUS:
	default:
		return -ENOENT;
	}

	return ath12k_dp_srng_find_ring_in_mask(ring_num, grp_mask, size);
}

void ath12k_dp_srng_msi_setup(struct ath12k_base *ab,
			      struct hal_srng_params *ring_params,
			      enum hal_ring_type type, int ring_num)
{
	int msi_group_number, msi_data_count;
	u32 msi_data_start, msi_irq_start, addr_lo, addr_hi;
	int ret;
	int vector;

	ret = ath12k_hif_get_user_msi_vector(ab, "DP",
					     &msi_data_count, &msi_data_start,
					     &msi_irq_start);
	if (ret)
		return;

	msi_group_number = ath12k_dp_srng_calculate_msi_group(ab, type,
							      ring_num);
	if (msi_group_number < 0) {
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "ring not part of an ext_group; ring_type: %d,ring_num %d",
			   type, ring_num);
		ring_params->msi_addr = 0;
		ring_params->msi_data = 0;
		return;
	}

	if (msi_group_number > msi_data_count) {
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "multiple msi_groups share one msi, msi_group_num %d",
			   msi_group_number);
	}

	ath12k_hif_get_msi_address(ab, &addr_lo, &addr_hi);

	ring_params->msi_addr = addr_lo;
	ring_params->msi_addr |= (dma_addr_t)(((uint64_t)addr_hi) << 32);
	if (ab->hif.bus == ATH12K_BUS_HYBRID)
		ring_params->msi_data = ab->ipci.dp_msi_data[msi_group_number];
	else
		ring_params->msi_data = (msi_group_number % msi_data_count)
			+ msi_data_start;
	ring_params->flags |= HAL_SRNG_FLAGS_MSI_INTR;

	vector = msi_irq_start  + (msi_group_number % msi_data_count);

	if (ab->hw_params->ds_support && !ath12k_dp_umac_reset_in_progress(ab))
		ath12k_hif_ppeds_register_interrupts(ab, type, vector, ring_num);
}

bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab)
{
        struct ath12k_hw_group *ag = ab->ag;
        struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
        bool umac_in_progress = false;

        if (!ab->hw_params->support_umac_reset)
                return umac_in_progress;

        spin_lock_bh(&mlo_umac_reset->lock);
        if (mlo_umac_reset->umac_reset_info &
            ATH12K_IS_UMAC_RESET_IN_PROGRESS)
                umac_in_progress = true;
        spin_unlock_bh(&mlo_umac_reset->lock);

        return umac_in_progress;
}

int ath12k_dp_srng_setup(struct ath12k_base *ab, struct dp_srng *ring,
			 enum hal_ring_type type, int ring_num,
			 int mac_id, int num_entries)
{
	struct hal_srng_params params = { 0 };
	int entry_sz = ath12k_hal_srng_get_entrysize(ab, type);
	int max_entries = ath12k_hal_srng_get_max_entries(ab, type);
	int vector = 0;
	int ret;
	bool cached = false;

	if (max_entries < 0 || entry_sz < 0)
		return -EINVAL;

	if (num_entries > max_entries)
		num_entries = max_entries;

	ring->size = (num_entries * entry_sz) + HAL_RING_BASE_ALIGN - 1;
#ifndef CONFIG_IO_COHERENCY
	if (ab->hw_params->alloc_cacheable_memory) {
		/* Allocate the reo dst and tx completion rings from cacheable memory */
		switch (type) {
		case HAL_REO_DST:
		case HAL_WBM2SW_RELEASE:
			cached = true;
			break;
		default:
			cached = false;
		}
	}
#else
	cached = true;
#endif

	if (ath12k_dp_umac_reset_in_progress(ab))
		goto skip_dma_alloc;

	if (cached) {
		ring->vaddr_unaligned = kzalloc(ring->size, GFP_KERNEL);
		ring->paddr_unaligned = virt_to_phys(ring->vaddr_unaligned);
	} else {
		ring->vaddr_unaligned = dma_alloc_coherent(ab->dev, ring->size,
							   &ring->paddr_unaligned,
							   GFP_KERNEL);
	}
	if (!ring->vaddr_unaligned)
		return -ENOMEM;

	ring->vaddr = PTR_ALIGN(ring->vaddr_unaligned, HAL_RING_BASE_ALIGN);
	ring->paddr = ring->paddr_unaligned + ((unsigned long)ring->vaddr -
		      (unsigned long)ring->vaddr_unaligned);

skip_dma_alloc:
	params.ring_base_vaddr = ring->vaddr;
	params.ring_base_paddr = ring->paddr;
	params.num_entries = num_entries;
	ath12k_dp_srng_msi_setup(ab, &params, type, ring_num + mac_id);

	if (ab->hw_params->ds_support && ab->hif.bus == ATH12K_BUS_AHB &&
	    !ath12k_dp_umac_reset_in_progress(ab))
		ath12k_hif_ppeds_register_interrupts(ab, type, vector, ring_num);

	switch (type) {
	case HAL_REO_DST:
	case HAL_REO2PPE:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_RX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_RXDMA_BUF:
		params.low_threshold = num_entries >> 3;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 0;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_RXDMA_MONITOR_BUF:
		params.low_threshold = num_entries >> 1;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 0;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		params.low_threshold = num_entries >> 3;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 1;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_TX_MONITOR_DST:
		params.low_threshold = DP_TX_MONITOR_BUF_SIZE_MAX >> 3;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 0;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_WBM2SW_RELEASE:
		if (ab->hw_params->hw_ops->dp_srng_is_tx_comp_ring(ring_num)) {
			params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_TX;
			params.intr_timer_thres_us =
					HAL_SRNG_INT_TIMER_THRESHOLD_TX;
			break;
		} else if (ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM) {
			params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_PPE_WBM2SW_REL;
			params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_TX;

				break;
		}
		/* follow through when ring_num != HAL_WBM2SW_REL_ERR_RING_NUM */
		fallthrough;
	case HAL_REO_EXCEPTION:
	case HAL_REO_REINJECT:
	case HAL_REO_CMD:
	case HAL_REO_STATUS:
	case HAL_TCL_DATA:
	case HAL_TCL_CMD:
	case HAL_TCL_STATUS:
	case HAL_WBM_IDLE_LINK:
	case HAL_SW2WBM_RELEASE:
	case HAL_RXDMA_DST:
	case HAL_RXDMA_MONITOR_DST:
	case HAL_RXDMA_MONITOR_DESC:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_OTHER;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_OTHER;
		break;
	case HAL_RXDMA_DIR_BUF:
		break;
	case HAL_PPE2TCL:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL;
		break;
	default:
		ath12k_warn(ab, "Not a valid ring type in dp :%d\n", type);
		return -EINVAL;
	}

	if (cached) {
		params.flags |= HAL_SRNG_FLAGS_CACHED;
		ring->cached = 1;
	}

	ret = ath12k_hal_srng_setup_idx(ab, type, ring_num, mac_id, &params, 0);
	if (ret < 0) {
		ath12k_warn(ab, "failed to setup srng: %d ring_id %d\n",
			    ret, ring_num);
		return ret;
	}

	ring->ring_id = ret;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_srng_setup);

void ath12k_dp_increment_bank_num_users(struct ath12k_dp *dp,
					int bank_id)
{
	spin_lock_bh(&dp->tx_bank_lock);
	dp->bank_profiles[bank_id].num_users++;
	spin_unlock_bh(&dp->tx_bank_lock);
}

void ath12k_dp_tx_put_bank_profile(struct ath12k_dp *dp, u8 bank_id)
{
	spin_lock_bh(&dp->tx_bank_lock);
	if (dp->bank_profiles[bank_id].num_users)
		dp->bank_profiles[bank_id].num_users--;
	spin_unlock_bh(&dp->tx_bank_lock);
}

int ath12k_dp_tx_get_bank_profile(struct ath12k_base *ab,
				  struct ath12k_link_vif *arvif,
				  struct ath12k_dp *dp, bool vdev_id_check_en)
{
	int bank_id = DP_INVALID_BANK_ID;
	int i;
	u32 bank_config;
	bool configure_register = false;


	/* convert vdev params into hal_tx_bank_config */
	bank_config = ath12k_dp_arch_tx_get_vdev_bank_config(dp, arvif, vdev_id_check_en);

	spin_lock_bh(&dp->tx_bank_lock);
	/* TODO: implement using idr kernel framework*/
	for (i = 0; i < dp->num_bank_profiles; i++) {
		if (dp->bank_profiles[i].is_configured &&
		    (dp->bank_profiles[i].bank_config ^ bank_config) == 0) {
			bank_id = i;
			goto inc_ref_and_return;
		}
		if (!dp->bank_profiles[i].is_configured ||
		    !dp->bank_profiles[i].num_users) {
			bank_id = i;
			goto configure_and_return;
		}
	}

	if (bank_id == DP_INVALID_BANK_ID) {
		spin_unlock_bh(&dp->tx_bank_lock);
		ath12k_err(ab, "unable to find TX bank!");
		return bank_id;
	}

configure_and_return:
	dp->bank_profiles[bank_id].is_configured = true;
	dp->bank_profiles[bank_id].bank_config = bank_config;
	configure_register = true;
inc_ref_and_return:
	dp->bank_profiles[bank_id].num_users++;
	spin_unlock_bh(&dp->tx_bank_lock);

	if (configure_register)
		ath12k_hal_tx_configure_bank_register(ab,
						      bank_config, bank_id);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "dp_htt tcl bank_id %d input 0x%x match 0x%x num_users %u",
		   bank_id, bank_config, dp->bank_profiles[bank_id].bank_config,
		   dp->bank_profiles[bank_id].num_users);

	return bank_id;
}

void ath12k_dp_tx_update_bank_profile(struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = arvif->ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_vif *ahvif = arvif->ahvif;
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];

	if (arvif->splitphy_ds_bank_id != DP_INVALID_BANK_ID) {
		ath12k_dp_tx_put_bank_profile(dp, arvif->splitphy_ds_bank_id);
		arvif->splitphy_ds_bank_id = ath12k_dp_tx_get_bank_profile(ab, arvif, dp, false);
		ath12k_ppeds_update_splitphy_bank_id(ab, arvif);
	}

	ath12k_dp_tx_put_bank_profile(dp, dp_link_vif->bank_id);
	dp_link_vif->bank_id = ath12k_dp_tx_get_bank_profile(ab, arvif, dp, dp_link_vif->vdev_id_check_en);

	ath12k_dp_ppeds_update_vp_entry(arvif->ar, arvif);
}

void ath12k_dp_deinit_bank_profiles(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	kfree(dp->bank_profiles);
	dp->bank_profiles = NULL;
}
EXPORT_SYMBOL(ath12k_dp_deinit_bank_profiles);

int ath12k_dp_init_bank_profiles(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 num_tcl_banks = ab->hw_params->num_tcl_banks;
	int i;

	dp->num_bank_profiles = num_tcl_banks;
	dp->bank_profiles = kmalloc_array(num_tcl_banks,
					  sizeof(struct ath12k_dp_tx_bank_profile),
					  GFP_KERNEL);
	if (!dp->bank_profiles)
		return -ENOMEM;

	spin_lock_init(&dp->tx_bank_lock);

	for (i = 0; i < num_tcl_banks; i++) {
		dp->bank_profiles[i].is_configured = false;
		dp->bank_profiles[i].num_users = 0;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_init_bank_profiles);

void ath12k_dp_srng_common_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	ath12k_dp_srng_cleanup(ab, &dp->reo_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp->reo_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp->reo_except_ring);
	ath12k_dp_srng_cleanup(ab, &dp->rx_rel_ring);
	ath12k_dp_srng_cleanup(ab, &dp->reo_reinject_ring);
	ath12k_dp_srng_cleanup(ab, &dp->wbm_desc_rel_ring);

	ath12k_dp_srng_ppeds_cleanup(ab);
}
EXPORT_SYMBOL(ath12k_dp_srng_common_cleanup);

int ath12k_dp_srng_common_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct hal_srng *srng;
	int ret;

	ret = ath12k_dp_srng_setup(ab, &dp->wbm_desc_rel_ring,
				   HAL_SW2WBM_RELEASE, 0, 0,
				   DP_WBM_RELEASE_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up wbm2sw_release ring :%d\n",
			    ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp->reo_reinject_ring, HAL_REO_REINJECT,
				   0, 0, DP_REO_REINJECT_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_reinject ring :%d\n",
			    ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp->reo_except_ring, HAL_REO_EXCEPTION,
				   0, 0, DP_REO_EXCEPTION_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_exception ring :%d\n",
			    ret);
		goto err;
	}

	ret = ath12k_dp_srng_setup(ab, &dp->reo_cmd_ring, HAL_REO_CMD,
				   0, 0, DP_REO_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_cmd ring :%d\n", ret);
		goto err;
	}

	srng = &ab->hal.srng_list[dp->reo_cmd_ring.ring_id];
	ath12k_hal_reo_init_cmd_ring(ab, srng);

	ret = ath12k_dp_srng_setup(ab, &dp->reo_status_ring, HAL_REO_STATUS,
				   0, 0, DP_REO_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_status ring :%d\n", ret);
		goto err;
	}

	if (ath12k_dp_umac_reset_in_progress(ab))
		goto skip_reo_setup;

	ath12k_hal_reo_hw_setup(ab);

skip_reo_setup:
	ret = ath12k_dp_srng_ppeds_setup(ab);
	if (ret) {
		ath12k_warn(ab, "failed to set up ppe-ds srngs :%d\n", ret);
		goto err;
	}

	return 0;
err:
	ath12k_dp_srng_common_cleanup(ab);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_srng_common_setup);

static void ath12k_dp_scatter_idle_link_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct wbm_idle_scatter_list *slist = dp->scatter_list;
	int i;

	for (i = 0; i < DP_IDLE_SCATTER_BUFS_MAX; i++) {
		if (!slist[i].vaddr)
			continue;

		ath12k_hal_dma_free_coherent(ab->dev, HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX,
					      slist[i].vaddr, slist[i].paddr);
		slist[i].vaddr = NULL;
	}
}

static int ath12k_dp_scatter_idle_link_desc_setup(struct ath12k_base *ab,
						  int size,
						  u32 n_link_desc_bank,
						  u32 n_link_desc,
						  u32 last_bank_sz)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_hal *hal = dp->hal;
	struct dp_link_desc_bank *link_desc_banks = dp->link_desc_banks;
	struct wbm_idle_scatter_list *slist = dp->scatter_list;
	u32 n_entries_per_buf;
	int num_scatter_buf, scatter_idx;
	struct wbm_link_desc *scatter_buf;
	int align_bytes, n_entries;
	dma_addr_t paddr;
	int rem_entries;
	int i;
	int ret = 0;
	u32 end_offset, cookie;
	enum hal_rx_buf_return_buf_manager rbm = dp->idle_link_rbm;
	u16 link_desc_size = ab->hal.hal_params->link_desc_size;

	n_entries_per_buf = HAL_WBM_IDLE_SCATTER_BUF_SIZE /
		ath12k_hal_srng_get_entrysize(ab, HAL_WBM_IDLE_LINK);
	num_scatter_buf = DIV_ROUND_UP(size, HAL_WBM_IDLE_SCATTER_BUF_SIZE);

	if (num_scatter_buf > DP_IDLE_SCATTER_BUFS_MAX)
		return -EINVAL;

	if (!ath12k_dp_umac_reset_in_progress(ab)) {
		for (i = 0; i < num_scatter_buf; i++) {
			slist[i].vaddr = ath12k_hal_dma_alloc_coherent(ab->dev,
				             HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX,
						&slist[i].paddr, GFP_KERNEL);
			if (!slist[i].vaddr) {
				ret = -ENOMEM;
				goto err;
			}
		}
	}

	scatter_idx = 0;
	scatter_buf = slist[scatter_idx].vaddr;
	rem_entries = n_entries_per_buf;

	for (i = 0; i < n_link_desc_bank; i++) {
		align_bytes = link_desc_banks[i].vaddr -
			      link_desc_banks[i].vaddr_unaligned;
		n_entries = (DP_LINK_DESC_ALLOC_SIZE_THRESH - align_bytes) /
			     link_desc_size;
		paddr = link_desc_banks[i].paddr;
		while (n_entries) {
			cookie = DP_LINK_DESC_COOKIE_SET(n_entries, i);
			ath12k_hal_set_link_desc_addr(hal, scatter_buf, cookie,
						      paddr, rbm);
			n_entries--;
			paddr += link_desc_size;
			if (rem_entries) {
				rem_entries--;
				scatter_buf++;
				continue;
			}

			rem_entries = n_entries_per_buf;
			scatter_idx++;
			scatter_buf = slist[scatter_idx].vaddr;
		}
	}

	end_offset = (scatter_buf - slist[scatter_idx].vaddr) *
		     sizeof(struct wbm_link_desc);
	ath12k_hal_setup_link_idle_list(ab, slist, num_scatter_buf,
					n_link_desc, end_offset);

	return 0;

err:
	ath12k_dp_scatter_idle_link_desc_cleanup(ab);

	return ret;
}

static void
ath12k_dp_link_desc_bank_free(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks)
{
	int i;

	for (i = 0; i < DP_LINK_DESC_BANKS_MAX; i++) {
		if (link_desc_banks[i].vaddr_unaligned) {
			ath12k_hal_dma_free_coherent(ab->dev,
						      link_desc_banks[i].size,
						      link_desc_banks[i].vaddr_unaligned,
						      link_desc_banks[i].paddr_unaligned);
			link_desc_banks[i].vaddr_unaligned = NULL;
		}
	}
}

static int ath12k_dp_link_desc_bank_alloc(struct ath12k_base *ab,
					  struct dp_link_desc_bank *desc_bank,
					  int n_link_desc_bank,
					  int last_bank_sz)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;
	int ret = 0;
	int desc_sz = DP_LINK_DESC_ALLOC_SIZE_THRESH;

	for (i = 0; i < n_link_desc_bank; i++) {
		if (i == (n_link_desc_bank - 1) && last_bank_sz)
			desc_sz = last_bank_sz;

		desc_bank[i].vaddr_unaligned =
				ath12k_hal_dma_alloc_coherent(ab->dev, desc_sz,
							       &desc_bank[i].paddr_unaligned,
							       GFP_KERNEL);
		if (!desc_bank[i].vaddr_unaligned) {
			ret = -ENOMEM;
			goto err;
		}

		desc_bank[i].vaddr = PTR_ALIGN(desc_bank[i].vaddr_unaligned,
					       HAL_LINK_DESC_ALIGN);
		desc_bank[i].paddr = desc_bank[i].paddr_unaligned +
				     ((unsigned long)desc_bank[i].vaddr -
				      (unsigned long)desc_bank[i].vaddr_unaligned);
		desc_bank[i].size = desc_sz;
	}

	return 0;

err:
	ath12k_dp_link_desc_bank_free(ab, dp->link_desc_banks);

	return ret;
}

void ath12k_dp_link_desc_cleanup(struct ath12k_base *ab,
				 struct dp_link_desc_bank *desc_bank,
				 u32 ring_type, struct dp_srng *ring)
{
	ath12k_dp_link_desc_bank_free(ab, desc_bank);

	if (ring_type != HAL_RXDMA_MONITOR_DESC) {
		ath12k_dp_srng_cleanup(ab, ring);
		ath12k_dp_scatter_idle_link_desc_cleanup(ab);
	}
}
EXPORT_SYMBOL(ath12k_dp_link_desc_cleanup);

int ath12k_wbm_idle_ring_setup(struct ath12k_base *ab, u32 *n_link_desc)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 n_mpdu_link_desc, n_mpdu_queue_desc;
	u32 n_tx_msdu_link_desc, n_rx_msdu_link_desc;
	int ret = 0;

	n_mpdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_MPDUS_PER_TID_MAX) /
			   ab->hal.hal_params->num_mpdus_per_link_desc;

	n_mpdu_queue_desc = n_mpdu_link_desc /
			    ab->hal.hal_params->num_mpdu_links_per_queue_desc;

	n_tx_msdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_FLOWS_PER_TID *
			       DP_AVG_MSDUS_PER_FLOW) /
			      ab->hal.hal_params->num_tx_msdus_per_link_desc;

	n_rx_msdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_MPDUS_PER_TID_MAX *
			       DP_AVG_MSDUS_PER_MPDU) /
			      ab->hal.hal_params->num_rx_msdus_per_link_desc;

	*n_link_desc = n_mpdu_link_desc + n_mpdu_queue_desc +
		      n_tx_msdu_link_desc + n_rx_msdu_link_desc;

	if (*n_link_desc & (*n_link_desc - 1))
		*n_link_desc = 1 << fls(*n_link_desc);

	ret = ath12k_dp_srng_setup(ab, &dp->wbm_idle_ring,
				   HAL_WBM_IDLE_LINK, 0, 0, *n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL(ath12k_wbm_idle_ring_setup);

int ath12k_dp_link_desc_setup(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks,
			      u32 ring_type, struct hal_srng *srng,
			      u32 n_link_desc)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 tot_mem_sz;
	u32 n_link_desc_bank, last_bank_sz;
	u32 entry_sz, align_bytes, n_entries;
	struct wbm_link_desc *desc;
	u32 paddr;
	int i, ret;
	u32 cookie;
	enum hal_rx_buf_return_buf_manager rbm = dp->idle_link_rbm;
	u16 link_desc_size = ab->hal.hal_params->link_desc_size;

	tot_mem_sz = n_link_desc * link_desc_size;
	tot_mem_sz += HAL_LINK_DESC_ALIGN;

	if (tot_mem_sz <= DP_LINK_DESC_ALLOC_SIZE_THRESH) {
		n_link_desc_bank = 1;
		last_bank_sz = tot_mem_sz;
	} else {
		n_link_desc_bank = tot_mem_sz /
				   (DP_LINK_DESC_ALLOC_SIZE_THRESH -
				    HAL_LINK_DESC_ALIGN);
		last_bank_sz = tot_mem_sz %
			       (DP_LINK_DESC_ALLOC_SIZE_THRESH -
				HAL_LINK_DESC_ALIGN);

		if (last_bank_sz)
			n_link_desc_bank += 1;
	}

	if (n_link_desc_bank > DP_LINK_DESC_BANKS_MAX)
		return -EINVAL;

	if (!ath12k_dp_umac_reset_in_progress(ab)) {
		ret = ath12k_dp_link_desc_bank_alloc(ab, link_desc_banks,
						     n_link_desc_bank,
						     last_bank_sz);
		if (ret)
			return ret;
	}

	/* Setup link desc idle list for HW internal usage */
	entry_sz = ath12k_hal_srng_get_entrysize(ab, ring_type);
	tot_mem_sz = entry_sz * n_link_desc;

	/* Setup scatter desc list when the total memory requirement is more */
	if (tot_mem_sz > DP_LINK_DESC_ALLOC_SIZE_THRESH &&
	    ring_type != HAL_RXDMA_MONITOR_DESC) {
		ret = ath12k_dp_scatter_idle_link_desc_setup(ab, tot_mem_sz,
							     n_link_desc_bank,
							     n_link_desc,
							     last_bank_sz);
		if (ret) {
			ath12k_warn(ab, "failed to setup scatting idle list descriptor :%d\n",
				    ret);
			goto fail_desc_bank_free;
		}

		return 0;
	}

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	for (i = 0; i < n_link_desc_bank; i++) {
		align_bytes = link_desc_banks[i].vaddr -
			      link_desc_banks[i].vaddr_unaligned;
		n_entries = (link_desc_banks[i].size - align_bytes) /
			    link_desc_size;
		paddr = link_desc_banks[i].paddr;
		while (n_entries &&
		       (desc = ath12k_hal_srng_src_get_next_entry(ab, srng))) {
			cookie = DP_LINK_DESC_COOKIE_SET(n_entries, i);
			ath12k_hal_set_link_desc_addr(dp->hal, desc, cookie, paddr,
						      rbm);
			n_entries--;
			paddr += link_desc_size;
		}
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return 0;

fail_desc_bank_free:
	ath12k_dp_link_desc_bank_free(ab, link_desc_banks);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_link_desc_setup);

int ath12k_dp_pdev_pre_alloc(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_pdev_dp *dp = &ar->dp;
	int ret;

	dp->hw = ar->ah->hw;
	dp->dp = ath12k_ab_to_dp(ar->ab);
	dp->mac_id = ar->pdev_idx;
	dp->ar = ar;
	dp->dp_hw = &ar->ah->dp_hw;
	dp->hw_link_id = ar->hw_link_id;

	atomic_set(&dp->num_tx_pending, 0);
	init_waitqueue_head(&dp->tx_empty_waitq);

	if (!dp->dp_mon_pdev_configured) {
		ret = ath12k_dp_mon_pdev_init(dp);
		if (ret) {
			ath12k_warn(ab, "failed to initialize mon pdev for pdev with mac_id: %d\n", dp->mac_id);
			return ret;
		}

		ret = ath12k_dp_mon_pdev_rx_alloc(dp, dp->mac_id);
		if (ret) {
			ath12k_warn(ab, "failed to alloc rx filter for pdev with mac_id: %d\n", dp->mac_id);
			goto mon_pdev_deinit;
		}

		ret = ath12k_dp_mon_pdev_rx_htt_setup(dp, dp->mac_id);
		if (ret) {
			ath12k_warn(ab, "failed to setup rx htt for pdev with mac_id: %d\n", dp->mac_id);
			goto mon_pdev_rx_free;
		}

		dp->dp_mon_pdev_configured = true;
	}

	/* TODO: Add any RXDMA setup required per pdev */

	return 0;

mon_pdev_rx_free:
	ath12k_dp_mon_pdev_rx_free(dp);

mon_pdev_deinit:
	ath12k_dp_mon_pdev_deinit(dp);

	return ret;
}

int ath12k_dp_get_pdev_telemetry_stats(struct ath12k_base *ab,
                                      int pdev_id,
                                      struct ath12k_pdev_telemetry_stats *stats)
{
       struct ath12k_pdev_dp *dp;
       struct ath12k *ar;
       u8 ac;

       if (!ab) {
               pr_warn("Failed to fetch pdev dp telemetry stats\n");
               return -EINVAL;
       }

       ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
       if (!ar)
               return -EINVAL;

       dp = &ar->dp;

       spin_lock_bh(&ar->data_lock);

       /* Convert *_link_airtime from telemetry stats to a percentage of
        * microseconds (us) */
       for (ac = 0; ac < WLAN_MAX_AC; ac++) {
               stats->tx_link_airtime[ac] =
                       ((dp->stats.telemetry_stats.tx_link_airtime[ac] * 100) / 1000000);
               stats->rx_link_airtime[ac] =
                       ((dp->stats.telemetry_stats.rx_link_airtime[ac] * 100) / 1000000);
               stats->link_airtime[ac] =
                       (((dp->stats.telemetry_stats.tx_link_airtime[ac] +
                          dp->stats.telemetry_stats.rx_link_airtime[ac]) * 100) / 1000000);
       }

	stats->rx_data_msdu_cnt = dp->stats.telemetry_stats.rx_data_msdu_cnt;
	stats->total_rx_data_bytes = dp->stats.telemetry_stats.total_rx_data_bytes;
	stats->tx_data_msdu_cnt = dp->stats.telemetry_stats.tx_data_msdu_cnt;
	stats->total_tx_data_bytes = dp->stats.telemetry_stats.total_tx_data_bytes;
	stats->sta_vap_exist = dp->stats.telemetry_stats.sta_vap_exist;
	stats->time_last_assoc = dp->stats.telemetry_stats.time_last_assoc;

       spin_unlock_bh(&ar->data_lock);

       return 0;
}
static void ath12k_dp_update_vdev_search(struct ath12k_link_vif *arvif)
{
	u8 link_id = arvif->link_id;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];

	switch (arvif->ahvif->vdev_type) {
	case WMI_VDEV_TYPE_STA:
		dp_link_vif->hal_addr_search_flags = HAL_TX_ADDRX_EN;
		dp_link_vif->search_type = HAL_TX_ADDR_SEARCH_INDEX;
		break;
	case WMI_VDEV_TYPE_AP:
	case WMI_VDEV_TYPE_IBSS:
		dp_link_vif->hal_addr_search_flags = HAL_TX_ADDRX_EN;
		dp_link_vif->search_type = HAL_TX_ADDR_SEARCH_DEFAULT;
		break;
	case WMI_VDEV_TYPE_MONITOR:
	default:
		return;
	}
}

void ath12k_dp_vdev_tx_attach(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
	u8 link_id = arvif->link_id;
	int bank_id;
	bool mec_support;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];

	dp_link_vif->tcl_metadata = u32_encode_bits(1, HTT_TCL_META_DATA_TYPE) |
				     u32_encode_bits(arvif->vdev_id,
						     HTT_TCL_META_DATA_VDEV_ID) |
				     u32_encode_bits(ar->pdev->pdev_id,
						     HTT_TCL_META_DATA_PDEV_ID);

	/* set HTT extension valid bit to 0 by default */
	dp_link_vif->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT;

	ath12k_dp_update_vdev_search(arvif);
	dp_link_vif->vdev_id_check_en = true;
	bank_id = ath12k_dp_tx_get_bank_profile(ab, arvif, ath12k_ab_to_dp(ab), dp_link_vif->vdev_id_check_en);
	dp_link_vif->bank_id = bank_id;
	arvif->splitphy_ds_bank_id = DP_INVALID_BANK_ID;


	mec_support = test_bit(WMI_SERVICE_MEC_AGING_TIMER_SUPPORT, ab->wmi_ab.svc_map);
	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    ath12k_frame_mode == ATH12K_HW_TXRX_ETHERNET && mec_support) {
		ath12k_wmi_pdev_set_timer_for_mec(ar, arvif->vdev_id,
						  WMI_PDEV_MEC_AGING_TIMER_THRESHOLD_VALUE);
		ath12k_hal_vdev_mcast_ctrl_set(ab, arvif->vdev_id,
					       HAL_TX_PACKET_CONTROL_CONFIG_MEC_NOTIFY);
	}

	/* TODO: error path for bank id failure */
	if (bank_id == DP_INVALID_BANK_ID) {
		ath12k_err(ar->ab, "Failed to initialize DP TX Banks");
		return;
	}
}

void ath12k_dp_cc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_tx_desc_info *tx_desc_info;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_skb_cb *skb_cb;
	struct sk_buff *skb;
	struct ath12k *ar;
	int i, j, k;
	u32 pool_id, tx_spt_page;

	if (!dp->spt_info)
		return;

	/* RX Descriptor cleanup */
	spin_lock_bh(&dp->rx_desc_lock);

	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		const void *end;

		desc_info = dp->rxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			if (!desc_info[j].in_use) {
				list_del(&desc_info[j].list);
				continue;
			}

			skb = desc_info[j].skb;
			if (!skb)
				continue;

			end = desc_info[j].vaddr + DP_RX_BUFFER_SIZE;
			ath12k_core_dmac_inv_range(desc_info[j].vaddr, end);
			dev_kfree_skb_any(skb);
		}
	}

	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		if (!dp->rxbaddr[i])
			continue;

		kfree(dp->rxbaddr[i]);
		dp->rxbaddr[i] = NULL;
	}

	spin_unlock_bh(&dp->rx_desc_lock);

	/* TX Descriptor cleanup */
	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		spin_lock_bh(&dp->tx_desc_lock[i]);

		for (j = 0; j < ATH12K_TX_SPT_PAGES_PER_POOL; j++) {
			tx_spt_page = j + i * ATH12K_TX_SPT_PAGES_PER_POOL;
			tx_desc_info = dp->txbaddr[tx_spt_page];
			for (k = 0; k < ATH12K_MAX_SPT_ENTRIES; k++) {
				if (!tx_desc_info[k].in_use)
					continue;

				skb = tx_desc_info[k].skb;
				if (!skb)
					continue;

				tx_desc_info[k].skb = NULL;

				if (tx_desc_info[k].skb_ext_desc) {
					ath12k_core_dma_unmap_single(ab->dev,
								     tx_desc_info[k].paddr_ext_desc,
								     tx_desc_info[k].skb_ext_desc->len,
								     DMA_TO_DEVICE);
					dev_kfree_skb_any(tx_desc_info[k].skb_ext_desc);
					tx_desc_info[k].skb_ext_desc = NULL;
				}

				/* if we are unregistering, hw would've been destroyed and
				 * ar is no longer valid.
				 */
				if (!(test_bit(ATH12K_FLAG_UNREGISTERING, &ab->dev_flags))) {
					skb_cb = ATH12K_SKB_CB(skb);
					ar = skb_cb->u.ar;
					if (atomic_dec_and_test(&ar->dp.num_tx_pending))
						wake_up(&ar->dp.tx_empty_waitq);
				}

				ath12k_core_dma_unmap_single(ab->dev, tx_desc_info[k].paddr,
							     tx_desc_info[k].len, DMA_TO_DEVICE);
				dev_kfree_skb_any(skb);

				tx_desc_info[k].in_use = false;
			}
		}

		spin_unlock_bh(&dp->tx_desc_lock[i]);
	}

	for (pool_id = 0; pool_id < ATH12K_HW_MAX_QUEUES; pool_id++) {
		spin_lock_bh(&dp->tx_desc_lock[pool_id]);

		for (i = 0; i < ATH12K_TX_SPT_PAGES_PER_POOL; i++) {
			tx_spt_page = i + pool_id * ATH12K_TX_SPT_PAGES_PER_POOL;
			if (!dp->txbaddr[tx_spt_page])
				continue;

			kfree(dp->txbaddr[tx_spt_page]);
			dp->txbaddr[tx_spt_page] = NULL;
		}

		spin_unlock_bh(&dp->tx_desc_lock[pool_id]);
	}

	/* unmap SPT pages */
	for (i = 0; i < dp->num_spt_pages; i++) {
		if (!dp->spt_info[i].vaddr)
			continue;

		ath12k_hal_dma_free_coherent(ab->dev, ATH12K_PAGE_SIZE,
					      dp->spt_info[i].vaddr, dp->spt_info[i].paddr);
		dp->spt_info[i].vaddr = NULL;
	}

	kfree(dp->spt_info);
	dp->spt_info = NULL;
}
EXPORT_SYMBOL(ath12k_dp_cc_cleanup);

void ath12k_dp_reoq_lut_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (dp->reoq_lut.vaddr_unaligned) {
		ath12k_hal_dma_free_coherent(ab->dev, dp->reoq_lut.size,
					      dp->reoq_lut.vaddr_unaligned,
					      dp->reoq_lut.paddr_unaligned);
		dp->reoq_lut.vaddr_unaligned = NULL;
	}

	if (dp->ml_reoq_lut.vaddr_unaligned) {
		ath12k_hal_dma_free_coherent(ab->dev, dp->ml_reoq_lut.size,
					      dp->ml_reoq_lut.vaddr_unaligned,
					      dp->ml_reoq_lut.paddr_unaligned);
		dp->ml_reoq_lut.vaddr_unaligned = NULL;
	}
}
EXPORT_SYMBOL(ath12k_dp_reoq_lut_cleanup);

static u32 ath12k_dp_cc_cookie_gen(u16 ppt_idx, u16 spt_idx)
{
	return (u32)ppt_idx << ATH12K_CC_PPT_SHIFT | spt_idx;
}

static inline void *ath12k_dp_cc_get_desc_addr_ptr(struct ath12k_dp *dp,
						   u16 ppt_idx, u16 spt_idx)
{
	return dp->spt_info[ppt_idx].vaddr + spt_idx;
}

struct ath12k_rx_desc_info *ath12k_dp_get_rx_desc(struct ath12k_dp *dp,
						  u32 cookie)
{
	struct ath12k_rx_desc_info **desc_addr_ptr;
	u16 start_ppt_idx, end_ppt_idx, ppt_idx, spt_idx;

	ppt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_PPT);
	spt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_SPT);

	start_ppt_idx = dp->rx_ppt_base + ATH12K_RX_SPT_PAGE_OFFSET;
	end_ppt_idx = start_ppt_idx + ATH12K_NUM_RX_SPT_PAGES;

	if (ppt_idx < start_ppt_idx ||
	    ppt_idx >= end_ppt_idx ||
	    spt_idx > ATH12K_MAX_SPT_ENTRIES)
		return NULL;

	ppt_idx = ppt_idx - dp->rx_ppt_base;
	desc_addr_ptr = ath12k_dp_cc_get_desc_addr_ptr(dp, ppt_idx, spt_idx);

	return *desc_addr_ptr;
}
EXPORT_SYMBOL(ath12k_dp_get_rx_desc);

struct ath12k_tx_desc_info *ath12k_dp_get_tx_desc(struct ath12k_dp *dp,
						  u32 cookie)
{
	struct ath12k_tx_desc_info **desc_addr_ptr;
	u16 start_ppt_idx, end_ppt_idx, ppt_idx, spt_idx;

	ppt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_PPT);
	spt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_SPT);

	start_ppt_idx = ATH12K_TX_SPT_PAGE_OFFSET;
	end_ppt_idx = start_ppt_idx +
		      (ATH12K_TX_SPT_PAGES_PER_POOL * ATH12K_HW_MAX_QUEUES);

	if (ppt_idx < start_ppt_idx ||
	    ppt_idx >= end_ppt_idx ||
	    spt_idx > ATH12K_MAX_SPT_ENTRIES)
		return NULL;

	desc_addr_ptr = ath12k_dp_cc_get_desc_addr_ptr(dp, ppt_idx, spt_idx);

	return *desc_addr_ptr;
}
EXPORT_SYMBOL(ath12k_dp_get_tx_desc);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static u8 *ath12k_dp_cc_find_desc(struct ath12k_base *ab, u32 cookie, bool is_rx)
{
	struct ath12k_dp *dp = ab->dp;
	u16 spt_page_id, spt_idx;
	u8 *spt_va;

	spt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_SPT);
	spt_page_id = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_PPT);

	if (is_rx) {
		if (WARN_ON(spt_page_id < dp->rx_ppt_base))
			return NULL;
		spt_page_id = spt_page_id - dp->rx_ppt_base;
	}

	spt_va = (u8 *)dp->spt_info[spt_page_id].vaddr;
	return (spt_va + spt_idx * sizeof(u64));
}

struct ath12k_ppeds_tx_desc_info *ath12k_dp_get_ppeds_tx_desc(struct ath12k_base *ab,
							      u32 desc_id)
{
	u8 *desc_addr_ptr;

	desc_addr_ptr = ath12k_dp_cc_find_desc(ab, desc_id, false);
	return *(struct ath12k_ppeds_tx_desc_info **)desc_addr_ptr;
}
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_dp_ppeds_tx_cmem_init(struct ath12k_base *ab, struct ath12k_dp *dp)
{
	u32 cmem_base;
	int i;

	cmem_base = ab->qmi.dev_mem[ATH12K_QMI_DEVMEM_CMEM_INDEX].start;

	for (i = ATH12K_PPEDS_TX_SPT_PAGE_OFFSET;
	     i < (ATH12K_PPEDS_TX_SPT_PAGE_OFFSET + ATH12K_NUM_PPEDS_TX_SPT_PAGES); i++) {
		/* Write to PPT in CMEM */
		if (ab->hif.ops->cmem_write32)
			ath12k_hif_cmem_write32(ab, cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
						dp->spt_info[i].paddr >> ATH12K_SPT_4K_ALIGN_OFFSET);
		else
			ath12k_hif_write32(ab, cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
					   dp->spt_info[i].paddr >> ATH12K_SPT_4K_ALIGN_OFFSET);
	}
}

static void ath12k_dp_ppeds_tx_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_ppeds_tx_desc_info *ppeds_tx_descs;
	struct ath12k_dp *dp = ab->dp;
	struct sk_buff *skb;
	int i, j;

	/* PPEDS TX Descriptor cleanup */
	spin_lock_bh(&dp->ppe.ppeds_tx_desc_lock);

	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		ppeds_tx_descs = dp->ppedstxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			if (!ppeds_tx_descs[j].in_use)
				continue;

			skb = ppeds_tx_descs[j].skb;
			if (!skb) {
				WARN_ON(1);
				continue;
			}

			ppeds_tx_descs[j].skb = NULL;
			ppeds_tx_descs[j].in_use = false;
			ath12k_core_dma_unmap_single_attrs(ab->dev,
							   ppeds_tx_descs[j].paddr,
							   skb->len, DMA_TO_DEVICE,
							   DMA_ATTR_SKIP_CPU_SYNC);

			dev_kfree_skb_any(skb);

			list_add_tail(&ppeds_tx_descs[j].list, &dp->ppe.ppeds_tx_desc_free_list);
		}
	}

	dp->ppe.ppeds_tx_desc_reuse_list_len = 0;

	spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);
}

int ath12k_dp_cc_ppeds_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_ppeds_tx_desc_info *ppeds_tx_descs;
	struct ath12k_dp *dp = ab->dp;
	struct sk_buff *skb;
	int i, j;

	if (!dp->ppedstxbaddr) {
		ath12k_err(ab, "%s failed", __func__);
		return -EINVAL;
	}

	spin_lock_bh(&dp->ppe.ppeds_tx_desc_lock);

	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		ppeds_tx_descs = dp->ppedstxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			skb = ppeds_tx_descs[j].skb;

			if (!skb)
				continue;

			ppeds_tx_descs[j].skb = NULL;
			ppeds_tx_descs[j].in_use = false;
			ppeds_tx_descs[j].paddr = (dma_addr_t)NULL;
			dev_kfree_skb_any(skb);
		}
	}

	dp->ppe.ppeds_tx_desc_reuse_list_len = 0;

	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		if (!dp->ppedstxbaddr[i])
			continue;

		kfree(dp->ppedstxbaddr[i]);
	}

	kfree(dp->ppedstxbaddr);
	dp->ppedstxbaddr = NULL;

	spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);

	ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);

	return 0;
}

int ath12k_dp_cc_ppeds_desc_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_ppeds_tx_desc_info *ppeds_tx_descs;
	struct ath12k_spt_info *ppeds_tx_spt_pages;
	u32 i, j;
	u32 ppt_idx;

	INIT_LIST_HEAD(&dp->ppe.ppeds_tx_desc_free_list);
	INIT_LIST_HEAD(&dp->ppe.ppeds_tx_desc_reuse_list);
	spin_lock_init(&dp->ppe.ppeds_tx_desc_lock);
	dp->ppe.ppeds_tx_desc_reuse_list_len = 0;

	dp->ppedstxbaddr = kmalloc_array(ATH12K_NUM_PPEDS_TX_SPT_PAGES,
					 sizeof(struct ath12k_ppeds_tx_desc_info *),
					 GFP_KERNEL);
	if (!dp->ppedstxbaddr)
		return -ENOMEM;

	/* pointer to start of TX pages */
	ppeds_tx_spt_pages = &dp->spt_info[ATH12K_PPEDS_TX_SPT_PAGE_OFFSET];

	spin_lock_bh(&dp->ppe.ppeds_tx_desc_lock);
	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		ppeds_tx_descs = kcalloc(ATH12K_MAX_SPT_ENTRIES, sizeof(*ppeds_tx_descs),
					 GFP_ATOMIC);
		if (!ppeds_tx_descs) {
			spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);
			ath12k_dp_cc_ppeds_desc_cleanup(ab);
			return -ENOMEM;
		}
		dp->ppedstxbaddr[i] = &ppeds_tx_descs[0];
		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			ppt_idx = ATH12K_PPEDS_TX_SPT_PAGE_OFFSET + i;
			ppeds_tx_descs[j].desc_id = ath12k_dp_cc_cookie_gen(ppt_idx, j);
			ppeds_tx_descs[j].in_use = false;
			list_add_tail(&ppeds_tx_descs[j].list,
				      &dp->ppe.ppeds_tx_desc_free_list);
			/* Update descriptor VA in SPT */
			*(struct ath12k_ppeds_tx_desc_info **)
				((u8 *)ppeds_tx_spt_pages[i].vaddr +
				 (j * sizeof(u64))) = &ppeds_tx_descs[j];
		}
	}
	spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);

	ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);

	return 0;
}
#endif

static int ath12k_dp_cc_desc_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_rx_desc_info *rx_descs, **rx_desc_addr;
	struct ath12k_tx_desc_info *tx_descs, **tx_desc_addr;
	u32 i, j, pool_id, tx_spt_page;
	u32 ppt_idx, cookie_ppt_idx;

	spin_lock_bh(&dp->rx_desc_lock);

	/* First ATH12K_NUM_RX_SPT_PAGES of allocated SPT pages are used for RX */
	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		rx_descs = kcalloc(ATH12K_MAX_SPT_ENTRIES, sizeof(*rx_descs),
				   GFP_ATOMIC);

		if (!rx_descs) {
			spin_unlock_bh(&dp->rx_desc_lock);
			return -ENOMEM;
		}

		ppt_idx = ATH12K_RX_SPT_PAGE_OFFSET + i;
		cookie_ppt_idx = dp->rx_ppt_base + ppt_idx;
		dp->rxbaddr[i] = &rx_descs[0];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			rx_descs[j].cookie = ath12k_dp_cc_cookie_gen(cookie_ppt_idx, j);
			rx_descs[j].magic = ATH12K_DP_RX_DESC_MAGIC;
			rx_descs[j].device_id = ab->device_id;
			list_add_tail(&rx_descs[j].list, &dp->rx_desc_free_list);

			/* Update descriptor VA in SPT */
			rx_desc_addr = ath12k_dp_cc_get_desc_addr_ptr(dp, ppt_idx, j);
			*rx_desc_addr = &rx_descs[j];
		}
	}

	spin_unlock_bh(&dp->rx_desc_lock);

	for (pool_id = 0; pool_id < ATH12K_HW_MAX_QUEUES; pool_id++) {
		spin_lock_bh(&dp->tx_desc_lock[pool_id]);
		for (i = 0; i < ATH12K_TX_SPT_PAGES_PER_POOL; i++) {
			tx_descs = kcalloc(ATH12K_MAX_SPT_ENTRIES, sizeof(*tx_descs),
					   GFP_ATOMIC);

			if (!tx_descs) {
				spin_unlock_bh(&dp->tx_desc_lock[pool_id]);
				/* Caller takes care of TX pending and RX desc cleanup */
				return -ENOMEM;
			}

			tx_spt_page = i + pool_id * ATH12K_TX_SPT_PAGES_PER_POOL;
			ppt_idx = ATH12K_TX_SPT_PAGE_OFFSET + tx_spt_page;

			dp->txbaddr[tx_spt_page] = &tx_descs[0];

			for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
				tx_descs[j].desc_id = ath12k_dp_cc_cookie_gen(ppt_idx, j);
				tx_descs[j].pool_id = pool_id;
				list_add_tail(&tx_descs[j].list,
					      &dp->tx_desc_free_list[pool_id]);

				/* Update descriptor VA in SPT */
				tx_desc_addr =
					ath12k_dp_cc_get_desc_addr_ptr(dp, ppt_idx, j);
				*tx_desc_addr = &tx_descs[j];
			}
		}
		spin_unlock_bh(&dp->tx_desc_lock[pool_id]);
	}
	return 0;
}

static int ath12k_dp_cmem_init(struct ath12k_base *ab,
			       struct ath12k_dp *dp,
			       enum ath12k_dp_desc_type type)
{
	u32 cmem_base;
	int i, start, end;

	cmem_base = ab->qmi.dev_mem[ATH12K_QMI_DEVMEM_CMEM_INDEX].start;

	switch (type) {
	case ATH12K_DP_TX_DESC:
		start = ATH12K_TX_SPT_PAGE_OFFSET;
		end = start + ATH12K_NUM_TX_SPT_PAGES;
		break;
	case ATH12K_DP_RX_DESC:
		cmem_base += ATH12K_PPT_ADDR_OFFSET(dp->rx_ppt_base);
		start = ATH12K_RX_SPT_PAGE_OFFSET;
		end = start + ATH12K_NUM_RX_SPT_PAGES;
		break;
	default:
		ath12k_err(ab, "invalid descriptor type %d in cmem init\n", type);
		return -EINVAL;
	}

	/* Write to PPT in CMEM */
	for (i = start; i < end; i++) {
		if (ab->hif.ops->cmem_write32 && (ab->hif.bus == ATH12K_BUS_HYBRID))
			ath12k_hif_cmem_write32(ab, cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
					dp->spt_info[i].paddr >> ATH12K_SPT_4K_ALIGN_OFFSET);
		else
			ath12k_hif_write32(ab, cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
					dp->spt_info[i].paddr >> ATH12K_SPT_4K_ALIGN_OFFSET);
	}

	return 0;
}

void ath12k_dp_partner_cc_init(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		if (ag->ab[i]->is_bypassed || ag->ab[i] == ab)
			continue;

		ath12k_dp_cmem_init(ab, ag->ab[i]->dp, ATH12K_DP_RX_DESC);
	}
}

int ath12k_dp_cc_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret = 0;

	INIT_LIST_HEAD(&dp->rx_desc_free_list);
	spin_lock_init(&dp->rx_desc_lock);

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		INIT_LIST_HEAD(&dp->tx_desc_free_list[i]);
		spin_lock_init(&dp->tx_desc_lock[i]);
	}

	dp->num_spt_pages = ATH12K_NUM_SPT_PAGES;
	if (dp->num_spt_pages > ATH12K_MAX_PPT_ENTRIES)
		dp->num_spt_pages = ATH12K_MAX_PPT_ENTRIES;

	dp->spt_info = kcalloc(dp->num_spt_pages, sizeof(struct ath12k_spt_info),
			       GFP_KERNEL);

	if (!dp->spt_info) {
		ath12k_warn(ab, "SPT page allocation failure");
		return -ENOMEM;
	}

	dp->rx_ppt_base = ab->device_id * ATH12K_NUM_RX_SPT_PAGES;

	for (i = 0; i < dp->num_spt_pages; i++) {
		dp->spt_info[i].vaddr = ath12k_hal_dma_alloc_coherent(ab->dev,
								       ATH12K_PAGE_SIZE,
								       &dp->spt_info[i].paddr,
								       GFP_KERNEL);

		if (!dp->spt_info[i].vaddr) {
			ret = -ENOMEM;
			goto free;
		}

		if (dp->spt_info[i].paddr & ATH12K_SPT_4K_ALIGN_CHECK) {
			ath12k_warn(ab, "SPT allocated memory is not 4K aligned");
			ret = -EINVAL;
			goto free;
		}
	}

	ret = ath12k_dp_cmem_init(ab, dp, ATH12K_DP_TX_DESC);
	if (ret) {
		ath12k_warn(ab, "HW CC Tx cmem init failed %d", ret);
		goto free;
	}

	ret = ath12k_dp_cmem_init(ab, dp, ATH12K_DP_RX_DESC);
	if (ret) {
		ath12k_warn(ab, "HW CC Rx cmem init failed %d", ret);
		goto free;
	}

	ret = ath12k_dp_cc_desc_init(ab);
	if (ret) {
		ath12k_warn(ab, "HW CC desc init failed %d", ret);
		goto free;
	}

	return 0;
free:
	ath12k_dp_cc_cleanup(ab);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_cc_init);

enum ath12k_dp_eapol_key_type ath12k_dp_get_eapol_subtype(u8 *data)
{
	u8 pkt_type = *(data + EAPOL_PACKET_TYPE_OFFSET);
	u16 key_info, key_data_length;
	enum ath12k_dp_eapol_key_type subtype = DP_EAPOL_KEY_TYPE_MAX;
	u64 *key_nonce;
	bool pairwise;

	if (pkt_type != EAPOL_PACKET_TYPE_KEY)
		return DP_EAPOL_KEY_TYPE_MAX;

	key_info = be16_to_cpu(*(u16 *)(data + EAPOL_KEY_INFO_OFFSET));

	key_data_length = be16_to_cpu(*(u16 *)(data + EAPOL_KEY_DATA_LENGTH_OFFSET));
	key_nonce = (u64 *)(data + EAPOL_WPA_KEY_NONCE_OFFSET);
	pairwise = key_info & EAPOL_WPA_KEY_INFO_KEY_TYPE;

	if (key_info & EAPOL_WPA_KEY_INFO_ACK) {
		if (key_info &
		   (EAPOL_WPA_KEY_INFO_MIC | EAPOL_WPA_KEY_INFO_ENCR_KEY_DATA))
			subtype = pairwise ?
				DP_EAPOL_KEY_TYPE_M3 :  DP_EAPOL_KEY_TYPE_G1;
		else
			subtype =  DP_EAPOL_KEY_TYPE_M1;
	} else {
		if (key_data_length == 0 ||
		    !((*key_nonce) || (*(key_nonce + 1)) ||
		      (*(key_nonce + 2)) || (*(key_nonce + 3))))
			subtype = pairwise ?
				DP_EAPOL_KEY_TYPE_M4 :  DP_EAPOL_KEY_TYPE_G2;
		else
			subtype =  DP_EAPOL_KEY_TYPE_M2;
	}
	return subtype;
}
EXPORT_SYMBOL(ath12k_dp_get_eapol_subtype);

static int ath12k_dp_alloc_reoq_lut(struct ath12k_base *ab,
				    struct ath12k_reo_q_addr_lut *lut)
{
	lut->size =  DP_REOQ_LUT_SIZE + HAL_REO_QLUT_ADDR_ALIGN - 1;
	lut->vaddr_unaligned = ath12k_hal_dma_alloc_coherent(ab->dev, lut->size,
							      &lut->paddr_unaligned,
							      GFP_KERNEL | __GFP_ZERO);
	if (!lut->vaddr_unaligned)
		return -ENOMEM;

	lut->vaddr = PTR_ALIGN(lut->vaddr_unaligned, HAL_REO_QLUT_ADDR_ALIGN);
	lut->paddr = lut->paddr_unaligned +
		     ((unsigned long)lut->vaddr - (unsigned long)lut->vaddr_unaligned);
	return 0;
}

int ath12k_dp_reoq_lut_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	if (!ab->hw_params->reoq_lut_support)
		return 0;

	ret = ath12k_dp_alloc_reoq_lut(ab, &dp->reoq_lut);
	if (ret) {
		ath12k_warn(ab, "failed to allocate memory for reoq table");
		return ret;
	}

	ret = ath12k_dp_alloc_reoq_lut(ab, &dp->ml_reoq_lut);
	if (ret) {
		ath12k_warn(ab, "failed to allocate memory for ML reoq table");
		ath12k_hal_dma_free_coherent(ab->dev, dp->reoq_lut.size,
					      dp->reoq_lut.vaddr_unaligned,
					      dp->reoq_lut.paddr_unaligned);
		dp->reoq_lut.vaddr_unaligned = NULL;
		return ret;
	}

	/* Bits in the register have address [39:8] LUT base address to be
	 * allocated such that LSBs are assumed to be zero. Also, current
	 * design supports paddr upto 4 GB max hence it fits in 32 bit register only
	 */

	ath12k_hal_write_reoq_lut_addr(ab, dp->reoq_lut.paddr >> 8);
	ath12k_hal_write_ml_reoq_lut_addr(ab, dp->ml_reoq_lut.paddr >> 8);

	ath12k_hal_reoq_lut_addr_read_enable(ab);
	ath12k_hal_reoq_lut_set_max_peerid(ab);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_reoq_lut_setup);

static int ath12k_dp_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	dp->ab = ab;

	spin_lock_init(&dp->dp_lock);
	INIT_LIST_HEAD(&dp->peers);
	INIT_LIST_HEAD(&dp->neighbor_peers);
	mutex_init(&dp->tbl_mtx_lock);
	ath12k_dp_link_peer_rhash_tbl_init(dp);

	return 0;
}

void ath12k_dp_cmn_device_deinit(struct ath12k_dp *dp)
{
	ath12k_dp_arch_op_device_deinit(dp);

	ath12k_dp_link_peer_rhash_tbl_destroy(dp);
}

int ath12k_dp_cmn_device_init(struct ath12k_dp *dp)
{
	int ret;

	ret = ath12k_dp_arch_op_device_init(dp);
	if (ret)
		return ret;

	ret = ath12k_dp_setup(dp->ab);
	if (ret)
		return ret;

	return 0;
}

void ath12k_dp_cmn_hw_group_unassign(struct ath12k_dp *dp,
				     struct ath12k_hw_group *ag)
{
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;
	int i;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		if (!dp_hw_grp->rx_status_buf[i])
			continue;
		kfree(dp_hw_grp->rx_status_buf[i]);
		dp_hw_grp->rx_status_buf[i] = NULL;
	}

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		if (!dp_hw_grp->tx_status_buf[i])
			continue;
		kfree(dp_hw_grp->tx_status_buf[i]);
		dp_hw_grp->tx_status_buf[i] = NULL;
	}

	if (dp_hw_grp->fst) {
		ath12k_dp_rx_fst_detach(dp->ab, dp_hw_grp->fst);
		dp_hw_grp->fst = NULL;
	}

	dp_hw_grp->dp[dp->device_id] = NULL;

	dp->dp_hw_grp = NULL;
	dp->device_id = ATH12K_INVALID_DEVICE_ID;
}

void ath12k_dp_cmn_hw_group_assign(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;
	int i;

	dp->dp_hw_grp = dp_hw_grp;
	dp->device_id = ab->device_id;
	dp_hw_grp->dp[dp->device_id] = dp;

	if (!dp_hw_grp->fst)
		dp_hw_grp->fst = ath12k_dp_rx_fst_attach(ab);

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		if (dp_hw_grp->tx_status_buf[i])
			continue;

		/* Each arch tx completion handler can use this buffer by typecasting its own
		 * entry struct (aligned to 32 or 64 bytes).
		 */
		dp_hw_grp->tx_status_buf[i] = kzalloc(TX_STATUS_BUFFER_SIZE, GFP_KERNEL);
	}

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		if (dp_hw_grp->rx_status_buf[i])
			continue;

		/* Each arch rx process handler can use this buffer by typecasting its own
		 * entry struct (aligned to 32 bytes).
		 */
		dp_hw_grp->rx_status_buf[i] = kzalloc(RX_STATUS_BUFFER_SIZE, GFP_KERNEL);
		if (!dp_hw_grp->rx_status_buf[i]) {
			ath12k_err(ab, "Failed to allocate rx_status_buf[%d]", i);
			BUG_ON(1);
		}
	}
}

static void ath12k_dp_srng_hw_disable(struct ath12k_base *ab, struct dp_srng *ring)
{
        struct hal_srng *srng = &ab->hal.srng_list[ring->ring_id];

	ath12k_hal_srng_hw_disable(ab, srng);
}

void ath12k_dp_srng_hw_ring_disable(struct ath12k_base *ab)
{
        struct ath12k_dp *dp;
        int i;

        dp = ath12k_ab_to_dp(ab);
        for (i = 0; i < DP_REO_DST_RING_MAX; i++)
                ath12k_dp_srng_hw_disable(ab, &dp->reo_dst_ring[i]);
        ath12k_dp_srng_hw_disable(ab, &dp->wbm_desc_rel_ring);

        for(i = 0; i < ab->hw_params->max_tx_ring; i++) {
                ath12k_dp_srng_hw_disable(ab, &dp->tx_ring[i].tcl_data_ring);
                ath12k_dp_srng_hw_disable(ab, &dp->tx_ring[i].tcl_comp_ring);
        }
        ath12k_dp_srng_hw_disable(ab, &dp->reo_reinject_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->rx_rel_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->reo_except_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->reo_cmd_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->reo_status_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->wbm_idle_ring);
}

void ath12k_dp_umac_txrx_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_tx_desc_info *tx_desc_info;
	struct ath12k_dp *dp;
	struct sk_buff *skb;
	int i, j, k;
	u32 tx_spt_page;

	dp = ath12k_ab_to_dp(ab);
	/* RX Descriptor cleanup */
	spin_lock_bh(&dp->rx_desc_lock);

	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		const void *end;

		desc_info = dp->rxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			if (!desc_info[j].in_use)
				continue;

			skb = desc_info[j].skb;
			if (!skb)
				continue;

			end = desc_info[j].vaddr + DP_RX_BUFFER_SIZE;
			ath12k_core_dmac_inv_range(desc_info[j].vaddr, end);

			dev_kfree_skb_any(skb);

			desc_info[j].skb = NULL;
			desc_info[j].vaddr = NULL;
			desc_info[j].paddr = 0;
			desc_info[j].in_use = false;
			list_add_tail(&desc_info[j].list, &dp->rx_desc_free_list);
		}
	}

	spin_unlock_bh(&dp->rx_desc_lock);

	/* TX Descriptor cleanup */
	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		spin_lock_bh(&dp->tx_desc_lock[i]);
		for (j = 0; j < ATH12K_TX_SPT_PAGES_PER_POOL; j++) {
			tx_spt_page = j + i * ATH12K_TX_SPT_PAGES_PER_POOL;
			tx_desc_info = dp->txbaddr[tx_spt_page];

			for (k = 0; k < ATH12K_MAX_SPT_ENTRIES; k++) {
				if (!tx_desc_info[k].in_use)
					continue;

				skb = tx_desc_info[k].skb;
				if (!skb)
					continue;

				tx_desc_info[k].skb = NULL;

				if (tx_desc_info[k].skb_ext_desc) {
					ath12k_core_dma_unmap_single(ab->dev,
								     tx_desc_info[k].paddr_ext_desc,
								     tx_desc_info[k].skb_ext_desc->len,
								     DMA_TO_DEVICE);
					dev_kfree_skb_any(tx_desc_info[k].skb_ext_desc);
					tx_desc_info[k].skb_ext_desc = NULL;
				}

				ath12k_core_dma_unmap_single(ab->dev, tx_desc_info[k].paddr,
							     tx_desc_info[k].len, DMA_TO_DEVICE);
				dev_kfree_skb_any(skb);

				tx_desc_info[k].in_use = false;
			}
		}
		spin_unlock_bh(&dp->tx_desc_lock[i]);
	}
}

size_t ath12k_dp_get_req_entries_from_buf_ring(struct ath12k_base *ab,
                                               struct dp_rxdma_ring *rx_ring,
                                               struct list_head *list)
{
        struct hal_srng *srng;
        struct ath12k_dp *dp;
        size_t num_free, req_entries;

        dp = ath12k_ab_to_dp(ab);
        srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
        spin_lock_bh(&srng->lock);
        ath12k_hal_srng_access_begin(ab, srng);
        num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
        if (!num_free) {
                ath12k_hal_srng_access_end(ab, srng);
                spin_unlock_bh(&srng->lock);
                return 0;
        }

        spin_lock_bh(&dp->rx_desc_lock);
        req_entries = ath12k_dp_list_cut_nodes(list,
                                               &dp->rx_desc_free_list,
                                               num_free);
        spin_unlock_bh(&dp->rx_desc_lock);

        ath12k_hal_srng_access_end(ab, srng);
        spin_unlock_bh(&srng->lock);

        return req_entries;
}
EXPORT_SYMBOL(ath12k_dp_get_req_entries_from_buf_ring);

int ath12k_dp_rxdma_ring_setup(struct ath12k_base *ab)
{
        struct ath12k_dp *dp;
        struct dp_rxdma_ring *rx_ring;
        LIST_HEAD(list);
        size_t req_entries;
        int ret;

        dp = ath12k_ab_to_dp(ab);
        rx_ring = &dp->rx_refill_buf_ring;
        ret = ath12k_dp_srng_setup(ab,
                                   &dp->rx_refill_buf_ring.refill_buf_ring,
                                   HAL_RXDMA_BUF, 0, 0,
                                   DP_RXDMA_BUF_RING_SIZE);

        if (ret) {
                ath12k_warn(ab, "failed to setup rx_refill_buf_ring\n");
                return ret;
        }

        req_entries = ath12k_dp_get_req_entries_from_buf_ring(ab, rx_ring, &list);
        if (req_entries)
                ath12k_dp_rx_bufs_replenish(dp, rx_ring, &list);

        return 0;
}

void ath12k_umac_reset_handle_post_reset_start(struct ath12k_base *ab)
{
        struct ath12k_dp *dp;
        struct ath12k_hw_group *ag = ab->ag;
        struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
        int i, n_link_desc, ret;
        struct hal_srng *srng = NULL;
        unsigned long end;

        ath12k_dp_srng_hw_ring_disable(ab);

        /* Busy wait for 2 ms to make sure the rings are
         * in idle state before enabling it
         */
        end = jiffies + msecs_to_jiffies(2);
        while (time_before(jiffies, end))
                ;

        ret = ath12k_wbm_idle_ring_setup(ab, &n_link_desc);

        if (ret)
                ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);

	dp = ath12k_ab_to_dp(ab);
        srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];

        ret = ath12k_dp_link_desc_setup(ab, dp->link_desc_banks,
                                        HAL_WBM_IDLE_LINK, srng, n_link_desc);
        if (ret)
                ath12k_warn(ab, "failed to setup link desc: %d\n", ret);

	ath12k_dp_srng_common_setup(ab);
	dp->arch_ops->dp_tx_ring_setup(ab);
        ath12k_dp_umac_txrx_desc_cleanup(ab);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		ath12k_dp_ppeds_tx_desc_cleanup(ab);
#endif

	ret = ath12k_dp_srng_setup(ab, &dp->rx_rel_ring, HAL_WBM2SW_RELEASE,
				   HAL_WBM2SW_REL_ERR_RING_NUM, 0,
				   DP_RX_RELEASE_RING_SIZE);
	if (ret)
		ath12k_warn(ab, "failed to set up rx_rel ring :%d\n", ret);

        ath12k_dp_rxdma_ring_setup(ab);

        for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
                ret = ath12k_dp_srng_setup(ab, &dp->reo_dst_ring[i],
                                           HAL_REO_DST, i, 0,
                                           DP_REO_DST_RING_SIZE);
                if (ret)
                        ath12k_warn(ab, "failed to setup reo_dst_ring\n");
        }

        ath12k_dp_rx_reo_cmd_list_cleanup(ab);

        ath12k_dp_tid_cleanup(ab);

        atomic_inc(&mlo_umac_reset->response_chip);
        ath12k_umac_reset_notify_target_sync_and_send(ab, ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE);

        return;
}

void ath12k_dp_cmn_update_hw_links(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag,
				   struct ath12k *ar)
{
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;

	lockdep_assert_held(&ag->mutex);

	dp_hw_grp->hw_links[ar->hw_link_id].device_id = dp->device_id;
	dp_hw_grp->hw_links[ar->hw_link_id].pdev_idx = ar->pdev_idx;
}

void ath12k_dp_reoq_lut_addr_reset(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;

	if (test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags)){
		if (dp->reoq_lut.vaddr_unaligned)
			ath12k_hal_write_reoq_lut_addr(ab, 0);

		if (dp->ml_reoq_lut.vaddr_unaligned)
			ath12k_hal_write_ml_reoq_lut_addr(ab, 0);
	}
}

static void ath12k_dp_aggr_per_pkt_peer_stats(struct ath12k_pdev_dp *dp_pdev,
					      struct ath12k_dp_peer_stats *dst_peer_stats,
					      struct ath12k_dp_peer_stats *src_peer_stats,
					      bool is_vdev_peer)
{
	int i, j;

	/*tx peer stats*/
	for(i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		dst_peer_stats->tx[i].comp_pkt.packets +=
			src_peer_stats->tx[i].comp_pkt.packets;
		dst_peer_stats->tx[i].comp_pkt.bytes +=
			src_peer_stats->tx[i].comp_pkt.bytes;
		dst_peer_stats->tx[i].tx_success.packets +=
			src_peer_stats->tx[i].tx_success.packets;
		dst_peer_stats->tx[i].tx_success.bytes +=
			src_peer_stats->tx[i].tx_success.bytes;
		dst_peer_stats->tx[i].tx_failed +=
			src_peer_stats->tx[i].tx_failed;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
				for (j = 0; j < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; j++)
					dst_peer_stats->tx[i].wbm_rel_reason[j] +=
						src_peer_stats->tx[i].wbm_rel_reason[j];
				for (j = 0; j < HAL_WBM_TQM_REL_REASON_MAX; j++)
					dst_peer_stats->tx[i].tqm_rel_reason[j] +=
						src_peer_stats->tx[i].tqm_rel_reason[j];

				dst_peer_stats->tx[i].release_src_not_tqm +=
					src_peer_stats->tx[i].release_src_not_tqm;
				dst_peer_stats->tx[i].retry_count +=
					src_peer_stats->tx[i].retry_count;
				dst_peer_stats->tx[i].total_msdu_retries +=
					src_peer_stats->tx[i].total_msdu_retries;
				dst_peer_stats->tx[i].multiple_retry_count +=
					src_peer_stats->tx[i].multiple_retry_count;
				dst_peer_stats->tx[i].ofdma +=
					src_peer_stats->tx[i].ofdma;
				dst_peer_stats->tx[i].amsdu_cnt +=
					src_peer_stats->tx[i].amsdu_cnt;
				dst_peer_stats->tx[i].non_amsdu_cnt +=
					src_peer_stats->tx[i].non_amsdu_cnt;
				dst_peer_stats->tx[i].inval_link_id_pkt_cnt +=
					src_peer_stats->tx[i].inval_link_id_pkt_cnt;
				dst_peer_stats->tx[i].ucast +=
					src_peer_stats->tx[i].ucast;

				if (is_vdev_peer) {
					dst_peer_stats->tx[i].mcast +=
						src_peer_stats->tx[i].mcast;
					dst_peer_stats->tx[i].bcast +=
						src_peer_stats->tx[i].bcast;
				}
			}
		}
	}

	/*rx peer stats*/
	for(i = 0; i < DP_REO_DST_RING_MAX; i++) {
		dst_peer_stats->rx[i].recv_from_reo.packets +=
			src_peer_stats->rx[i].recv_from_reo.packets;
		dst_peer_stats->rx[i].recv_from_reo.bytes +=
			src_peer_stats->rx[i].recv_from_reo.bytes;

		dst_peer_stats->rx[i].sent_to_stack.packets +=
			src_peer_stats->rx[i].sent_to_stack.packets;
		dst_peer_stats->rx[i].sent_to_stack.bytes +=
			src_peer_stats->rx[i].sent_to_stack.bytes;

		dst_peer_stats->rx[i].sent_to_stack_fast.packets +=
			src_peer_stats->rx[i].sent_to_stack_fast.packets;
		dst_peer_stats->rx[i].sent_to_stack_fast.bytes +=
			src_peer_stats->rx[i].sent_to_stack_fast.bytes;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
				dst_peer_stats->rx[i].mcast +=
					src_peer_stats->rx[i].mcast;
				dst_peer_stats->rx[i].ucast +=
					src_peer_stats->rx[i].ucast;
				dst_peer_stats->rx[i].non_amsdu +=
					src_peer_stats->rx[i].non_amsdu;
				dst_peer_stats->rx[i].msdu_part_of_amsdu +=
					src_peer_stats->rx[i].msdu_part_of_amsdu;
				dst_peer_stats->rx[i].mpdu_retry +=
					src_peer_stats->rx[i].mpdu_retry;
			}
		}
	}

	/*rx error stats*/
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		dst_peer_stats->wbm_err.rxdma_error[i] +=
			src_peer_stats->wbm_err.rxdma_error[i];
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		dst_peer_stats->wbm_err.reo_error[i] +=
			src_peer_stats->wbm_err.reo_error[i];
}

static void ath12k_dp_update_per_pkt_peer_stats(struct ath12k_pdev_dp *dp_pdev,
						struct ath12k_dp_peer_stats *dst_peer_stats,
						struct ath12k_dp_peer_stats *src_peer_stats,
						bool is_vdev_peer)
{
	int i, j;

	/*tx peer stats*/
	for(i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		dst_peer_stats->tx[i].comp_pkt.packets =
			src_peer_stats->tx[i].comp_pkt.packets;
		dst_peer_stats->tx[i].comp_pkt.bytes =
			src_peer_stats->tx[i].comp_pkt.bytes;
		dst_peer_stats->tx[i].tx_success.packets =
			src_peer_stats->tx[i].tx_success.packets;
		dst_peer_stats->tx[i].tx_success.bytes =
			src_peer_stats->tx[i].tx_success.bytes;
		dst_peer_stats->tx[i].tx_failed =
			src_peer_stats->tx[i].tx_failed;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
				for (j = 0; j < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; j++)
					dst_peer_stats->tx[i].wbm_rel_reason[j] =
						src_peer_stats->tx[i].wbm_rel_reason[j];

				for (j = 0; j < HAL_WBM_TQM_REL_REASON_MAX; j++)
					dst_peer_stats->tx[i].tqm_rel_reason[j] =
						src_peer_stats->tx[i].tqm_rel_reason[j];

				dst_peer_stats->tx[i].release_src_not_tqm =
					src_peer_stats->tx[i].release_src_not_tqm;
				dst_peer_stats->tx[i].retry_count =
					src_peer_stats->tx[i].retry_count;
				dst_peer_stats->tx[i].total_msdu_retries =
					src_peer_stats->tx[i].total_msdu_retries;
				dst_peer_stats->tx[i].multiple_retry_count =
					src_peer_stats->tx[i].multiple_retry_count;
				dst_peer_stats->tx[i].ofdma =
					src_peer_stats->tx[i].ofdma;
				dst_peer_stats->tx[i].amsdu_cnt =
					src_peer_stats->tx[i].amsdu_cnt;
				dst_peer_stats->tx[i].non_amsdu_cnt =
					src_peer_stats->tx[i].non_amsdu_cnt;
				dst_peer_stats->tx[i].inval_link_id_pkt_cnt =
					src_peer_stats->tx[i].inval_link_id_pkt_cnt;
				dst_peer_stats->tx[i].ucast =
					src_peer_stats->tx[i].ucast;

				if (is_vdev_peer) {
					dst_peer_stats->tx[i].mcast =
						src_peer_stats->tx[i].mcast;
					dst_peer_stats->tx[i].bcast =
						src_peer_stats->tx[i].bcast;
				}
			}
		}
	}

	/*rx peer stats*/
	for(i = 0; i < DP_REO_DST_RING_MAX; i++) {
		dst_peer_stats->rx[i].recv_from_reo.packets =
			src_peer_stats->rx[i].recv_from_reo.packets;
		dst_peer_stats->rx[i].recv_from_reo.bytes =
			src_peer_stats->rx[i].recv_from_reo.bytes;

		dst_peer_stats->rx[i].sent_to_stack.packets =
			src_peer_stats->rx[i].sent_to_stack.packets;
		dst_peer_stats->rx[i].sent_to_stack.bytes =
			src_peer_stats->rx[i].sent_to_stack.bytes;
		dst_peer_stats->rx[i].sent_to_stack_fast.packets =
			src_peer_stats->rx[i].sent_to_stack_fast.packets;
		dst_peer_stats->rx[i].sent_to_stack_fast.bytes =
			src_peer_stats->rx[i].sent_to_stack_fast.bytes;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
				dst_peer_stats->rx[i].mcast =
					src_peer_stats->rx[i].mcast;
				dst_peer_stats->rx[i].ucast =
					src_peer_stats->rx[i].ucast;
				dst_peer_stats->rx[i].non_amsdu =
					src_peer_stats->rx[i].non_amsdu;
				dst_peer_stats->rx[i].msdu_part_of_amsdu =
					src_peer_stats->rx[i].msdu_part_of_amsdu;
				dst_peer_stats->rx[i].mpdu_retry =
					src_peer_stats->rx[i].mpdu_retry;
			}
		}
	}

	/*rx error stats*/
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		dst_peer_stats->wbm_err.rxdma_error[i] =
			src_peer_stats->wbm_err.rxdma_error[i];
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		dst_peer_stats->wbm_err.reo_error[i] =
			src_peer_stats->wbm_err.reo_error[i];
}

static void ath12k_dp_aggr_peer_stats(struct ath12k_link_vif *arvif,
				      struct ath12k_dp_link_peer *link_peer,
				      struct ath12k_dp_aggr_vif_stats *aggr_vif_stats)
{
	struct ath12k_dp_peer *peer;
	struct ath12k_pdev_dp *dp_pdev = &arvif->ar->dp;
	struct ath12k *ar = arvif->ar;
	int stats_link_id;

	if (!link_peer->dp_peer)
		return;

	peer = link_peer->dp_peer;
	stats_link_id = peer->hw_links[ar->hw_link_id];
	if (stats_link_id <= ATH12K_DP_MAX_MLO_LINKS)
		ath12k_dp_aggr_per_pkt_peer_stats(dp_pdev, &aggr_vif_stats->peer_stats,
						  &peer->stats[stats_link_id],
						  peer->is_vdev_peer);
}

static void ath12k_vif_iterate_peer(struct ath12k_link_vif *arvif,
				    struct ath12k_dp_aggr_vif_stats *aggr_vif_stats)
{

	struct ath12k_dp *dp = arvif ->ar->ab->dp;
	u32 vdev_id = arvif-> vdev_id;
	struct ath12k_dp_link_peer *link_peer;

	/* Iterate through all peers of particular vif*/
	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry(link_peer, &dp->peers, list)  {
		if (link_peer->vdev_id != vdev_id)
			continue;
		ath12k_dp_aggr_peer_stats(arvif, link_peer, aggr_vif_stats);
	}
	spin_unlock_bh(&dp->dp_lock);
}

static void ath12k_dp_aggr_vif_ingress_stats(struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_dp_aggr_vif_stats *aggr_vif_stats,
                                             struct ath12k_dp_vif *vif)
{
	int i, j;

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		aggr_vif_stats->stats[i].tx_i.recv_from_stack.packets +=
			vif->stats[i].tx_i.recv_from_stack.packets;
		aggr_vif_stats->stats[i].tx_i.recv_from_stack.bytes +=
			vif->stats[i].tx_i.recv_from_stack.bytes;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw.packets +=
			vif->stats[i].tx_i.enque_to_hw.packets;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw.bytes +=
			vif->stats[i].tx_i.enque_to_hw.bytes;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw_fast.packets +=
			vif->stats[i].tx_i.enque_to_hw_fast.packets;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw_fast.bytes +=
			vif->stats[i].tx_i.enque_to_hw_fast.bytes;


		for (j = 0; j < DP_TX_ENQ_ERR_MAX; j++)
			aggr_vif_stats->stats[i].tx_i.drop[j] +=
				vif->stats[i].tx_i.drop[j];

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
				aggr_vif_stats->stats[i].tx_i.mcast +=
					vif->stats[i].tx_i.mcast;

				for (j = 0; j < HAL_TCL_ENCAP_TYPE_MAX; j++)
					aggr_vif_stats->stats[i].tx_i.encap_type[j] +=
						vif->stats[i].tx_i.encap_type[j];

				for (j = 0; j < HAL_ENCRYPT_TYPE_MAX; j++)
					aggr_vif_stats->stats[i].tx_i.encrypt_type[j] +=
						vif->stats[i].tx_i.encrypt_type[j];
				for (j = 0; j < DP_TCL_DESC_TYPE_MAX; j++)
					aggr_vif_stats->stats[i].tx_i.desc_type[j] +=
						vif->stats[i].tx_i.desc_type[j];
			}
		}
	}
}

void ath12k_dp_get_pdev_stats(struct ath12k_pdev_dp *pdev,
			      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	struct ath12k *ar = pdev->ar;
	struct ath12k_dp_aggr_vif_stats *aggr_vif_stats;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_aggr_pdev_stats *aggr_pdev_stats =
					&telemetry_radio->aggr_pdev_stats;

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    ath12k_dp_debug_stats_enabled(&ar->dp))
		telemetry_radio->is_extended = true;

	aggr_vif_stats = vmalloc(sizeof(*aggr_vif_stats));
	if (aggr_vif_stats) {
		memset(aggr_vif_stats, 0, sizeof(*aggr_vif_stats));
		list_for_each_entry(arvif, &ar->arvifs, list) {
			ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
			ath12k_dp_aggr_per_pkt_peer_stats(pdev, &aggr_pdev_stats->peer_stats,
							  &aggr_vif_stats->peer_stats, 1);
		}
		vfree(aggr_vif_stats);
	}
}

void ath12k_dp_get_vif_stats(struct ath12k_vif *ahvif,
			     struct ath12k_telemetry_dp_vif *telemetry_vif,
			     u8 link_id)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k *ar = &ahvif->ah->radio[0];
	unsigned long links_map = ahvif->links_map;
	struct ath12k_dp_aggr_vif_stats *aggr_vif_stats =
						&telemetry_vif->aggr_vif_stats;

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    ath12k_dp_debug_stats_enabled(&ar->dp))
		telemetry_vif->is_extended = true;

	/*Vif stats for requested link*/
	if (links_map & BIT(link_id)) {
		rcu_read_lock();
		arvif = rcu_dereference(ahvif->link[link_id]);
		if (arvif)
			ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
		rcu_read_unlock();
	} else {
		/*MLD vif stats*/
		ath12k_dp_aggr_vif_ingress_stats(&ar->dp, aggr_vif_stats, dp_vif);

		/*legacy vif stats handling*/
		if (hweight16(links_map) == 0) {
			arvif =  &ahvif->deflink;
			if (arvif)
				ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
		} else {
			/*Aggregate vif stats of all link in MLD vif*/
			for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
				rcu_read_lock();
				arvif = rcu_dereference(ahvif->link[link_id]);
				if (!arvif || !arvif->is_created) {
					rcu_read_unlock();
					continue;
				}
				ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
				rcu_read_unlock();
			}
		}
	}
}

static int ath12k_dp_get_link_peer_stats(struct ath12k_link_vif *arvif,
					 struct ath12k_dp_peer_stats *peer_stats,
					 u8 *addr, u8 link_id, bool valid_link)
{
	struct ath12k_dp *dp = arvif->ar->ab->dp;
	struct ath12k_pdev_dp *dp_pdev = &arvif->ar->dp;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *peer;
	int hw_link_id = arvif->ar->hw_link_id;
	int stats_link_id;

	spin_lock_bh(&dp->dp_lock);
	link_peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
	if(link_peer) {
		/* Error case handling for legacy peer*/
		if (!link_peer->mlo && valid_link) {
			spin_unlock_bh(&dp->dp_lock);
			ath12k_err(NULL, "Error legacy peer with valid link id");
			return -EINVAL;
		}

		if (link_peer->hw_link_id == hw_link_id) {
			peer = link_peer->dp_peer;
			stats_link_id = peer->hw_links[hw_link_id];
			if (stats_link_id <= ATH12K_DP_MAX_MLO_LINKS)
				ath12k_dp_update_per_pkt_peer_stats(dp_pdev, peer_stats,
								    &peer->stats[stats_link_id],
								    peer->is_vdev_peer);
			spin_unlock_bh(&dp->dp_lock);
			return 0;
		}
	}

	spin_unlock_bh(&dp->dp_lock);
	ath12k_err(NULL, "Error link peer not found");
	return -EINVAL;
}

int ath12k_dp_get_peer_stats(struct ath12k_vif *ahvif,
			     struct ath12k_telemetry_dp_peer *telemetry_peer,
			     u8 *addr, u8 link_id)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_hw *dp_hw = &ahvif->ah->dp_hw;
	struct ath12k *ar = &ahvif->ah->radio[0];
	struct ath12k_dp_peer_stats *peer_stats;
	struct ath12k_dp_peer *peer;
	int stats_link_id, i, ret = 0;
	unsigned long links_map = ahvif->links_map;
	bool valid_link = ahvif->links_map & BIT(link_id);

	spin_lock_bh(&dp_hw->peer_lock);
	peer = ath12k_dp_peer_find(dp_hw, addr);
	peer_stats = &telemetry_peer->peer_stats;

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    ath12k_dp_debug_stats_enabled(&ar->dp))
		telemetry_peer->is_extended = true;

	if (peer) {
		/*Error case handling for legacy peer*/
		if (!peer->is_mlo && valid_link) {
			spin_unlock_bh(&dp_hw->peer_lock);
			ath12k_err(NULL, "Error legacy peer with valid link id");
			return -EINVAL;
		}

		/* Error case handling for non-associated links */
		if (valid_link && !(peer->peer_links_map & BIT(link_id))) {
			spin_unlock_bh(&dp_hw->peer_lock);
			ath12k_err(NULL, "Error MLO peer with invalid link id");
			return -EINVAL;
		}

		/*Peer stats of MLD peer for requested link id*/
		if (valid_link) {
			rcu_read_lock();
			arvif = rcu_dereference(ahvif->link[link_id]);
			if (arvif) {
				stats_link_id = peer->hw_links[arvif->ar->hw_link_id];
				if (stats_link_id <= ATH12K_DP_MAX_MLO_LINKS)
					ath12k_dp_update_per_pkt_peer_stats(&ar->dp,
									    peer_stats,
									    &peer->stats[stats_link_id],
									    peer->is_vdev_peer);
			}
			goto unlock;
		} else {
			/*legacy peer stats handling*/
			if (hweight16(links_map) == 0) {
				arvif =  &ahvif->deflink;
				if (arvif) {
					stats_link_id = peer->hw_links[arvif->ar->hw_link_id];
					if (stats_link_id <= ATH12K_DP_MAX_MLO_LINKS)
						ath12k_dp_update_per_pkt_peer_stats(&ar->dp,
										    peer_stats,
										    &peer->stats[stats_link_id],
										    peer->is_vdev_peer);
				}
			} else {
				/* Aggregated peer stats of all link in MLD peer*/
				for (i = 0; i < ATH12K_DP_MAX_MLO_LINKS; i++)
					ath12k_dp_aggr_per_pkt_peer_stats(&ar->dp,
									  peer_stats,
									  &peer->stats[i],
									  peer->is_vdev_peer);
			}
			spin_unlock_bh(&dp_hw->peer_lock);
			return ret;
		}
	} else {
		/*Peer stats of link peer for requested link id*/
		if (valid_link) {
			rcu_read_lock();
			arvif = rcu_dereference(ahvif->link[link_id]);
			if (arvif)
				ret = ath12k_dp_get_link_peer_stats(arvif, peer_stats, addr,
								    link_id, valid_link);
			goto unlock;
		} else {
			/*Peer stats of link peer without link id*/
			rcu_read_lock();
			for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
				arvif = rcu_dereference(ahvif->link[link_id]);
				if (!arvif || !arvif->is_created) {
					continue;
				}
				ret = ath12k_dp_get_link_peer_stats(arvif, peer_stats, addr,
								    link_id, valid_link);
				if (!ret)
					goto unlock;
			}
		}
	}

unlock:
	rcu_read_unlock();
	spin_unlock_bh(&dp_hw->peer_lock);
	return ret;
}

void ath12k_dp_get_device_stats(struct ath12k_dp *dp,
				struct ath12k_telemetry_dp_device *telemetry_device)
{
	memcpy(&telemetry_device->rxdma_error,
	       &dp->device_stats.wbm_err.rxdma_error,
	       sizeof(telemetry_device->rxdma_error));

	memcpy(&telemetry_device->reo_error,
	       &dp->device_stats.wbm_err.reo_error,
	       sizeof(telemetry_device->reo_error));

	memcpy(&telemetry_device->tx_comp_err,
	       &dp->device_stats.tx_err.tx_comp_err,
	       sizeof(telemetry_device->tx_comp_err));

	memcpy(&telemetry_device->rx_wbm_sw_drop_reason,
	       &dp->device_stats.wbm_err.drop,
	       sizeof(telemetry_device->rx_wbm_sw_drop_reason));

	memcpy(&telemetry_device->reo_sw_drop_reason,
	       &dp->device_stats.rx.rx_err,
	       sizeof(telemetry_device->reo_sw_drop_reason));
}

void ath12k_dp_clear_link_desc_pool(struct ath12k_dp *dp)
{
	struct dp_link_desc_bank *link_desc_banks = dp->link_desc_banks;
	int i;

	for (i = 0; i < DP_LINK_DESC_BANKS_MAX; i++) {
		if (link_desc_banks[i].vaddr_unaligned) {
			memset(link_desc_banks[i].vaddr_unaligned, 0x0,
			       link_desc_banks[i].size);
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_clear_link_desc_pool);
