/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../dp_mon.h"

extern const struct ath12k_dp_arch_mon_ops ath12k_wifi7_dp_arch_mon_quad_ring_ops;
extern const struct ath12k_dp_arch_mon_ops ath12k_wifi7_dp_arch_mon_dual_ring_ops;

static inline
void ath12k_wifi7_dp_mon_ops_register(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;

	if (ath12k_dp_get_mon_type(dp) == ATH12K_DP_MON_TYPE_DUAL_RING)
		dp_mon->mon_ops = &ath12k_wifi7_dp_arch_mon_dual_ring_ops;
	else
		dp_mon->mon_ops = &ath12k_wifi7_dp_arch_mon_quad_ring_ops;
}

static inline
int ath12k_dp_rx_mon_process_ring(struct ath12k_dp *dp, int mac_id,
				  struct napi_struct *napi, int budget)
{
	struct ath12k_pdev_dp *dp_pdev;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	u8 pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, mac_id);
	int num_buffs_reaped = 0;

	mon_ops = ath12k_dp_mon_ops_get(dp);
	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev)) {
		rcu_read_unlock();
		return 0;
	}

	if (mon_ops && mon_ops->mon_rx_srng_process) {
		num_buffs_reaped =
			mon_ops->mon_rx_srng_process(dp_pdev, mac_id,
							napi, &budget);
	}

	rcu_read_unlock();

	return num_buffs_reaped;
}

static inline
int ath12k_dp_tx_mon_process_ring(struct ath12k_dp *dp, int mac_id,
				  struct napi_struct *napi, int budget)
{
	/* TODO: Implement Tx Processing */
	return 0;
}
