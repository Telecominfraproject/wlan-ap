// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "../core.h"
#include "../debug.h"
#include "../dp_rx.h"
#include "../dp_tx.h"
#include "../hif.h"
#include "../dp_cmn.h"
#include "dp_rx.h"
#include "dp.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "hal.h"
#include "dp_mon.h"

static int ath12k_wifi7_dp_service_srng(struct ath12k_dp *dp,
					struct ath12k_ext_irq_grp *irq_grp,
					int budget)
{
	struct napi_struct *napi = &irq_grp->napi;
	struct ath12k_base *ab = dp->ab;
	int cpu_id = smp_processor_id();
	int grp_id = irq_grp->grp_id;
	int work_done = 0;
	int i = 0, j;
	int tot_work_done = 0;
	u8 ring_mask, rx_mask, tx_mask;

	set_bit(cpu_id, &dp->service_rings_running);
	rx_mask = dp->hw_params->ring_mask->rx[grp_id];
	tx_mask = dp->hw_params->ring_mask->tx[grp_id];

	while (tx_mask) {
		i = fls(tx_mask) - 1;
		tx_mask ^= 1 << i;
		work_done = ath12k_wifi7_dp_tx_completion_handler(dp, i, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_err[grp_id]) {
		work_done = ath12k_wifi7_dp_rx_process_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_wbm_rel[grp_id]) {
		work_done = ath12k_wifi7_dp_rx_process_wbm_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;

		if (budget <= 0)
			goto done;
	}

	while (rx_mask) {
		i = fls(rx_mask) - 1;
		rx_mask ^= 1 << i;
		work_done = ath12k_wifi7_dp_rx_process(dp, i, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_mon_status[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->rx_mon_status[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_rx_mon_process_ring(dp, id,
								      napi, budget);
					budget -= work_done;
					tot_work_done += work_done;
					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->rx_mon_dest[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->rx_mon_dest[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_rx_mon_process_ring(dp, id,
								      napi, budget);
					budget -= work_done;
					tot_work_done += work_done;

					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->tx_mon_dest[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->tx_mon_dest[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_tx_mon_process_ring(dp, id, napi,
								      budget);
					budget -= work_done;
					tot_work_done += work_done;

					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->reo_status[grp_id])
		ath12k_wifi7_dp_rx_process_reo_status(dp);

	if (dp->hw_params->ring_mask->host2rxdma[grp_id]) {
		struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
		LIST_HEAD(list);
		size_t req_entries;

		req_entries = ath12k_dp_get_req_entries_from_buf_ring(dp->ab, rx_ring, &list);
		if (req_entries)
			ath12k_dp_rx_bufs_replenish(dp, rx_ring, &list);
	}

	if (dp->hw_params->ring_mask->host2rxmon[grp_id])
		ath12k_dp_mon_rx_process_low_thres(dp);

	/* TODO: Implement handler for other interrupts */

done:
	clear_bit(cpu_id, &dp->service_rings_running);
	if (ab->dp_umac_reset.umac_pre_reset_in_prog)
		ath12k_umac_reset_notify_pre_reset_done(ab);

	return tot_work_done;
}

static int ath12k_wifi7_dp_op_device_init(struct ath12k_dp *dp)
{
	int ret;
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng = NULL;
	u32 n_link_desc = 0;
	int i;

	INIT_LIST_HEAD(&dp->reo_cmd_list);
	INIT_LIST_HEAD(&dp->reo_cmd_cache_flush_list);
	INIT_LIST_HEAD(&dp->reo_cmd_update_rx_queue_list);
	spin_lock_init(&dp->reo_cmd_update_rx_queue_lock);
	spin_lock_init(&dp->reo_cmd_lock);

	ret = ath12k_hif_ext_irq_setup(dp->ab, ath12k_wifi7_dp_service_srng, dp);
	if (ret)
		return ret;

	dp->reo_cmd_cache_flush_count = 0;
	dp->idle_link_rbm =
			ath12k_hal_get_idle_link_rbm(&ab->hal, ab->device_id);

	ret = ath12k_wbm_idle_ring_setup(ab, &n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);
		goto fail_irq_cleanup;
	}

	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];

	/* memset wbm link desc pool to 0 before desc_setup */
	ath12k_dp_clear_link_desc_pool(dp);

	ret = ath12k_dp_link_desc_setup(ab, dp->link_desc_banks,
					HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to setup link desc: %d\n", ret);
		goto fail_irq_cleanup;
	}

	ret = ath12k_dp_cc_init(ab);

	if (ret) {
		ath12k_warn(ab, "failed to setup cookie converter %d\n", ret);
		goto fail_link_desc_cleanup;
	}

	ret = ath12k_dp_init_bank_profiles(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup bank profiles %d\n", ret);
		goto fail_hw_cc_cleanup;
	}

	ret = ath12k_nss_plugin_register_ops(ab);
	if (ret) {
		ath12k_warn(ab, "failed to register nss plugin %d\n", ret);
		goto fail_dp_bank_profiles_cleanup;
	}

	ret = ath12k_ppeds_attach(ab);
	if (ret) {
		ath12k_warn(ab, "failed to attach PPE DS %d\n", ret);
		goto fail_nss_plugin_unregister;
	}

	ret = ath12k_dp_srng_common_setup(ab);
	if (ret)
		goto fail_ppeds_detach;

	ret = ath12k_wifi7_dp_tx_ring_setup(ab);
	if (ret)
		goto fail_cmn_srng_cleanup;

	ret = ath12k_dp_reoq_lut_setup(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup reoq table %d\n", ret);
		goto fail_tx_ring_cleanup;
	}

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		dp->tx_ring[i].tcl_data_ring_id = i;

	for (i = 0; i < HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX; i++)
		ath12k_hal_tx_set_dscp_tid_map(ab, ath12k_default_dscp_tid_map, i);

	ret = ath12k_wifi7_dp_rx_ring_setup(ab);
	if (ret) {
		ath12k_warn(ab, "rx allod failed ret = %d\n", ret);
		goto fail_dp_rx_free;
	}

	ret = ath12k_dp_mon_rx_alloc(dp);
	if (ret) {
		ath12k_warn(ab, "failed to setup rxdma rings ret = %d\n", ret);
		goto fail_dp_mon_rx_free;
	}

	return 0;

fail_dp_mon_rx_free:
	ath12k_dp_mon_rx_free(dp);

fail_dp_rx_free:
	ath12k_wifi7_dp_rx_ring_free(ab);
	ath12k_dp_reoq_lut_cleanup(ab);

fail_tx_ring_cleanup:
	ath12k_wifi7_dp_tx_ring_cleanup(ab);

fail_cmn_srng_cleanup:
	ath12k_dp_srng_common_cleanup(ab);

fail_ppeds_detach:
	ath12k_ppeds_detach(ab);

fail_nss_plugin_unregister:
	ath12k_nss_plugin_unregister_ops(ab);

fail_dp_bank_profiles_cleanup:
	ath12k_dp_deinit_bank_profiles(ab);

fail_hw_cc_cleanup:
	ath12k_dp_cc_cleanup(ab);

fail_link_desc_cleanup:
	ath12k_dp_link_desc_cleanup(ab, dp->link_desc_banks,
				    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);

fail_irq_cleanup:
	ath12k_hif_ext_irq_cleanup(dp->ab);

	return ret;

}

static void ath12k_wifi7_dp_op_device_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;

	if (!dp->ab)
		return;

	ath12k_dp_link_desc_cleanup(ab, dp->link_desc_banks,
				    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);

	ath12k_ppeds_detach(ab);
	ath12k_dp_cc_cleanup(ab);
	ath12k_dp_reoq_lut_cleanup(ab);
	ath12k_dp_deinit_bank_profiles(ab);
	ath12k_wifi7_dp_tx_ring_cleanup(ab);
	ath12k_dp_srng_common_cleanup(ab);

	ath12k_dp_rx_reo_cmd_list_cleanup(ab);

	ath12k_dp_mon_rx_free(dp);
	ath12k_nss_plugin_unregister_ops(ab);
	ath12k_wifi7_dp_rx_ring_free(ab);

	ath12k_hif_ext_irq_cleanup(dp->ab);
}

static struct ath12k_dp_hw_group *ath12k_wifi7_dp_hw_group_alloc(void)
{
	struct ath12k_dp_hw_group *dp_hw_grp;

	dp_hw_grp = kzalloc(sizeof(*dp_hw_grp), GFP_KERNEL);
	if (!dp_hw_grp) {
		pr_err("failed to allocate dp_hw_group\n");
		return NULL;
	}

	return dp_hw_grp;
}

static struct ath12k_dp_arch_ops ath12k_wifi7_dp_arch_ops = {
	.dp_op_device_init = ath12k_wifi7_dp_op_device_init,
	.dp_op_device_deinit = ath12k_wifi7_dp_op_device_deinit,
	.dp_tx_get_vdev_bank_config = ath12k_wifi7_dp_tx_get_vdev_bank_config,
	.dp_reo_cmd_send = ath12k_wifi7_dp_reo_cmd_send,
	.setup_pn_check_reo_cmd = ath12k_wifi7_dp_setup_pn_check_reo_cmd,
	.rx_peer_tid_delete = ath12k_wifi7_dp_rx_peer_tid_delete,
	.reo_cache_flush = ath12k_wifi7_dp_reo_cache_flush,
	.rx_link_desc_return = ath12k_wifi7_dp_rx_link_desc_return,
	.peer_rx_tid_reo_update = ath12k_wifi7_peer_rx_tid_reo_update,
	.alloc_reo_qdesc = ath12k_wifi7_dp_alloc_reo_qdesc,
	.peer_rx_tid_qref_setup = ath12k_wifi7_peer_rx_tid_qref_setup,
	.dp_pdev_alloc = ath12k_wifi7_dp_pdev_alloc,
	.dp_pdev_free = ath12k_wifi7_dp_pdev_free,
	.rx_fst_attach = ath12k_wifi7_dp_rx_fst_attach,
	.rx_fst_detach = ath12k_wifi7_dp_rx_fst_detach,
	.rx_flow_dump_entry = ath12k_wifi7_dp_rx_flow_dump_entry,
	.rx_flow_add_entry = ath12k_wifi7_dp_rx_flow_add_entry,
	.rx_flow_delete_entry = ath12k_wifi7_dp_rx_flow_delete_entry,
	.rx_flow_delete_all_entries = ath12k_wifi7_dp_rx_flow_delete_all_entries,
	.dump_fst_table = ath12k_wifi7_dp_dump_fst_table,
	.dp_hw_group_alloc = ath12k_wifi7_dp_hw_group_alloc,
	.peer_migrate_reo_cmd = ath12k_wifi7_dp_peer_migrate_reo_cmd,
	.sdwf_reinject_handler = ath12k_wifi7_sdwf_reinject_handler,
	.dp_tx_ring_setup = ath12k_wifi7_dp_tx_ring_setup,
	.dp_tx_status_parse = ath12k_wifi7_dp_tx_status_parse,
};

/* TODO: remove export once this file is built with wifi7 ko */
struct ath12k_dp *ath12k_wifi7_dp_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;
	int ret;

	dp = kzalloc(sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return NULL;

	dp->arch_ops = &ath12k_wifi7_dp_arch_ops;

	dp->ab = ab;
	dp->dev = ab->dev;
	dp->hw_params = ab->hw_params;
	dp->hal = &ab->hal;

	ret = ath12k_dp_mon_init(dp);
	if (ret) {
		ath12k_warn(dp, "dp_mon_init failed %d\n", ret);
		goto dp_err;
	}

	ath12k_wifi7_dp_mon_ops_register(dp);

	return dp;
dp_err:
	ath12k_wifi7_dp_deinit(dp);
	return NULL;
}

void ath12k_wifi7_dp_deinit(struct ath12k_dp *dp)
{
	ath12k_dp_mon_deinit(dp);
	kfree(dp);
}
