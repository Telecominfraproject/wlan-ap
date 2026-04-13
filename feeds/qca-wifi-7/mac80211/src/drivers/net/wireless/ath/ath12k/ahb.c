// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/dma-mapping.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/soc/qcom/mdt_loader.h>
#include "ahb.h"
#include "debug.h"
#include "hif.h"
#include "fw.h"
#include "pcic.h"
#include "wifi7/hal.h"
#include "ppe.h"
#include "erp.h"

#define ATH12K_IRQ_PPE_OFFSET 54
#define ATH12K_IRQ_CE0_OFFSET 4
#define ATH12K_MAX_UPDS 1
#define ATH12K_UPD_IRQ_WRD_LEN  18

static struct ath12k_ahb_driver *ath12k_ahb_family_drivers[ATH12K_DEVICE_FAMILY_MAX];
static struct platform_driver ath12k_ahb_drivers[ATH12K_DEVICE_FAMILY_MAX];
static const char ath12k_userpd_irq[][9] = {"spawn",
				     "ready",
				     "stop-ack",
				     "fatal"};

static const char *irq_name[ATH12K_IRQ_NUM_MAX] = {
	"misc-pulse1",
	"misc-latch",
	"sw-exception",
	"watchdog",
	"ce0",
	"ce1",
	"ce2",
	"ce3",
	"ce4",
	"ce5",
	"ce6",
	"ce7",
	"ce8",
	"ce9",
	"ce10",
	"ce11",
	"host2wbm-desc-feed",
	"host2reo-re-injection",
	"host2reo-command",
	"host2rxdma-monitor-ring3",
	"host2rxdma-monitor-ring2",
	"host2rxdma-monitor-ring1",
	"reo2ost-exception",
	"wbm2host-rx-release",
	"reo2host-status",
	"reo2host-destination-ring4",
	"reo2host-destination-ring3",
	"reo2host-destination-ring2",
	"reo2host-destination-ring1",
	"rxdma2host-monitor-destination-mac3",
	"rxdma2host-monitor-destination-mac2",
	"rxdma2host-monitor-destination-mac1",
	"ppdu-end-interrupts-mac3",
	"ppdu-end-interrupts-mac2",
	"ppdu-end-interrupts-mac1",
	"rxdma2host-monitor-status-ring-mac3",
	"rxdma2host-monitor-status-ring-mac2",
	"rxdma2host-monitor-status-ring-mac1",
	"host2rxdma-host-buf-ring-mac3",
	"host2rxdma-host-buf-ring-mac2",
	"host2rxdma-host-buf-ring-mac1",
	"rxdma2host-destination-ring-mac3",
	"rxdma2host-destination-ring-mac2",
	"rxdma2host-destination-ring-mac1",
	"host2tcl-input-ring4",
	"host2tcl-input-ring3",
	"host2tcl-input-ring2",
	"host2tcl-input-ring1",
	"wbm2host-tx-completions-ring4",
	"wbm2host-tx-completions-ring3",
	"wbm2host-tx-completions-ring2",
	"wbm2host-tx-completions-ring1",
	"tcl2host-status-ring",
	"umac_reset",
	"reo2ppe",
	"ppe_wbm_rel",
	"ppe2tcl"
};

enum ext_irq_num {
	host2wbm_desc_feed = 16,
	host2reo_re_injection,
	host2reo_command,
	host2rxdma_monitor_ring3,
	host2rxdma_monitor_ring2,
	host2rxdma_monitor_ring1,
	reo2host_exception,
	wbm2host_rx_release,
	reo2host_status,
	reo2host_destination_ring4,
	reo2host_destination_ring3,
	reo2host_destination_ring2,
	reo2host_destination_ring1,
	rxdma2host_monitor_destination_mac3,
	rxdma2host_monitor_destination_mac2,
	rxdma2host_monitor_destination_mac1,
	ppdu_end_interrupts_mac3,
	ppdu_end_interrupts_mac2,
	ppdu_end_interrupts_mac1,
	rxdma2host_monitor_status_ring_mac3,
	rxdma2host_monitor_status_ring_mac2,
	rxdma2host_monitor_status_ring_mac1,
	host2rxdma_host_buf_ring_mac3,
	host2rxdma_host_buf_ring_mac2,
	host2rxdma_host_buf_ring_mac1,
	rxdma2host_destination_ring_mac3,
	rxdma2host_destination_ring_mac2,
	rxdma2host_destination_ring_mac1,
	host2tcl_input_ring4,
	host2tcl_input_ring3,
	host2tcl_input_ring2,
	host2tcl_input_ring1,
	wbm2host_tx_completions_ring4,
	wbm2host_tx_completions_ring3,
	wbm2host_tx_completions_ring2,
	wbm2host_tx_completions_ring1,
	tcl2host_status_ring,
	umac_reset,
	reo2ppe,
	ppe_wbm_rel,
	ppe2tcl
};

static u32 ath12k_ahb_read32(struct ath12k_base *ab, u32 offset)
{
	if (ab->ce_remap && offset < ab->cmem_offset)
		return ioread32(ab->mem_ce + offset);
	return ioread32(ab->mem + offset);
}

static void ath12k_ahb_write32(struct ath12k_base *ab, u32 offset,
			       u32 value)
{
	if (ab->ce_remap && offset < ab->cmem_offset)
		iowrite32(value, ab->mem_ce + offset);
	else
		iowrite32(value, ab->mem + offset);
}

static inline u32 ath12k_ahb_pmm_read32(struct ath12k_base *ab, u32 offset)
{
	return ioread32(ab->mem_pmm + offset);
}

#if LINUX_VERSION_IS_GEQ(6,13,0)
static void ath12k_ahb_cancel_workqueue(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		struct ath12k_ce_pipe *ce_pipe = &ab->ce.ce_pipe[i];

		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		cancel_work_sync(&ce_pipe->intr_wq);
	}
}
#endif

static void ath12k_ahb_ext_grp_disable(struct ath12k_ext_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		disable_irq_nosync(irq_grp->ab->irq_num[irq_grp->irqs[i]]);
}

static void __ath12k_ahb_ext_irq_disable(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath12k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		ath12k_ahb_ext_grp_disable(irq_grp);

		if (test_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags))
			continue;

		if (irq_grp->napi_enabled) {
			napi_synchronize(&irq_grp->napi);
			napi_disable(&irq_grp->napi);
			irq_grp->napi_enabled = false;
		}
	}
}

static void ath12k_ahb_ext_grp_enable(struct ath12k_ext_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		enable_irq(irq_grp->ab->irq_num[irq_grp->irqs[i]]);
}

static void ath12k_ahb_setbit32(struct ath12k_base *ab, u8 bit, u32 offset)
{
	u32 val;

	val = ath12k_ahb_read32(ab, offset);
	ath12k_ahb_write32(ab, offset, val | BIT(bit));
}

static void ath12k_ahb_clearbit32(struct ath12k_base *ab, u8 bit, u32 offset)
{
	u32 val;

	val = ath12k_ahb_read32(ab, offset);
	ath12k_ahb_write32(ab, offset, val & ~BIT(bit));
}

static void ath12k_ahb_ce_irq_enable(struct ath12k_base *ab, u16 ce_id)
{
	const struct ce_attr *ce_attr;
	const struct ce_ie_addr *ce_ie_addr = ab->hw_params->ce_ie_addr;
	u32 ie1_reg_addr, ie2_reg_addr, ie3_reg_addr;

	ie1_reg_addr = ce_ie_addr->ie1_reg_addr;
	ie2_reg_addr = ce_ie_addr->ie2_reg_addr;
	ie3_reg_addr = ce_ie_addr->ie3_reg_addr;

	ce_attr = &ab->hw_params->host_ce_config[ce_id];
	if (ce_attr->src_nentries)
		ath12k_ahb_setbit32(ab, ce_id, ie1_reg_addr);

	if (ce_attr->dest_nentries) {
		ath12k_ahb_setbit32(ab, ce_id, ie2_reg_addr);
		ath12k_ahb_setbit32(ab, ce_id + CE_HOST_IE_3_SHIFT,
				    ie3_reg_addr);
	}
}

static void ath12k_ahb_ce_irq_disable(struct ath12k_base *ab, u16 ce_id)
{
	const struct ce_attr *ce_attr;
	const struct ce_ie_addr *ce_ie_addr = ab->hw_params->ce_ie_addr;
	u32 ie1_reg_addr, ie2_reg_addr, ie3_reg_addr;

	ie1_reg_addr = ce_ie_addr->ie1_reg_addr;
	ie2_reg_addr = ce_ie_addr->ie2_reg_addr;
	ie3_reg_addr = ce_ie_addr->ie3_reg_addr;

	ce_attr = &ab->hw_params->host_ce_config[ce_id];
	if (ce_attr->src_nentries)
		ath12k_ahb_clearbit32(ab, ce_id, ie1_reg_addr);

	if (ce_attr->dest_nentries) {
		ath12k_ahb_clearbit32(ab, ce_id, ie2_reg_addr);
		ath12k_ahb_clearbit32(ab, ce_id + CE_HOST_IE_3_SHIFT,
				      ie3_reg_addr);
	}
}

static void ath12k_ahb_sync_ce_irqs(struct ath12k_base *ab)
{
	int i;
	int irq_idx;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		irq_idx = ATH12K_IRQ_CE0_OFFSET + i;
		synchronize_irq(ab->irq_num[irq_idx]);
	}
}

static void ath12k_ahb_sync_ext_irqs(struct ath12k_base *ab)
{
	int i, j;
	int irq_idx;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath12k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		for (j = 0; j < irq_grp->num_irq; j++) {
			irq_idx = irq_grp->irqs[j];
			synchronize_irq(ab->irq_num[irq_idx]);
		}
	}
}

static void ath12k_ahb_ce_irqs_enable(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		ath12k_ahb_ce_irq_enable(ab, i);
	}
}

static void ath12k_ahb_ce_irqs_disable(struct ath12k_base *ab)
{
	int i;

	if (ab->pm_suspend)
		return;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		ath12k_ahb_ce_irq_disable(ab, i);
	}
}

static int ath12k_ahb_start(struct ath12k_base *ab)
{
	ath12k_ahb_ce_irqs_enable(ab);
	ath12k_ce_rx_post_buf(ab);

	return 0;
}

static void ath12k_ahb_ext_irq_enable(struct ath12k_base *ab)
{
	struct ath12k_ext_irq_grp *irq_grp;
	int i;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		irq_grp = &ab->ext_irq_grp[i];
		if (!irq_grp->napi_enabled) {
			napi_enable(&irq_grp->napi);
			irq_grp->napi_enabled = true;
		}
		ath12k_ahb_ext_grp_enable(irq_grp);
	}
}

static void ath12k_ahb_ext_irq_disable(struct ath12k_base *ab)
{
	if (ab->pm_suspend)
		return;

	__ath12k_ahb_ext_irq_disable(ab);

	if (!test_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags))
		ath12k_ahb_sync_ext_irqs(ab);
}

static void ath12k_ahb_kill_tasklets(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		struct ath12k_ce_pipe *ce_pipe = &ab->ce.ce_pipe[i];

		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		tasklet_kill(&ce_pipe->intr_tq);
	}
}

static void ath12k_ahb_stop(struct ath12k_base *ab)
{
	if (!test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		ath12k_ahb_ce_irqs_disable(ab);
	ath12k_ahb_sync_ce_irqs(ab);
#if LINUX_VERSION_IS_GEQ(6,13,0)
	ath12k_ahb_cancel_workqueue(ab);
#else
	ath12k_ahb_kill_tasklets(ab);
#endif
	del_timer_sync(&ab->rx_replenish_retry);
}

static int ath12k_ahb_power_up(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	char fw_name[ATH12K_USERPD_FW_NAME_LEN];
	char fw2_name[ATH12K_USERPD_FW_NAME_LEN];
	struct device *dev = ab->dev;
	const struct firmware *fw, *fw2;
	struct reserved_mem *rmem = NULL;
	unsigned long time_left;
	u32 pasid;
	int ret;

	if (ath12k_check_erp_power_down(ab->ag)) {
		if (ab_ahb->tgt_rproc->state != RPROC_RUNNING) {
			ret = ath12k_ahb_boot_root_pd(ab);
			if (ret < 0) {
				ath12k_err(ab, "failed to boot the remote processor Q6\n");
				return ret;
			}
		}
	}

	rmem = ath12k_core_get_reserved_mem_by_name(ab, "q6-region");
	if (!rmem)
		return -ENODEV;

	ab_ahb->mem_phys = rmem->base;
	ab_ahb->mem_size = rmem->size;
	ab_ahb->mem_region = (void *)devm_ioremap_wc(dev, ab_ahb->mem_phys, ab_ahb->mem_size);
	if (!ab_ahb->mem_region) {
		ath12k_err(ab, "unable to map memory region: %pa+%pa\n",
			   &rmem->base, &rmem->size);
		return -ENOMEM;
	}

	snprintf(fw_name, sizeof(fw_name), "%s/%s/%s%d%s", ATH12K_FW_DIR,
		 ab->hw_params->fw.dir, ATH12K_AHB_FW_PREFIX, ab_ahb->userpd_id,
		 ATH12K_AHB_FW_SUFFIX);

	ret = request_firmware(&fw, fw_name, dev);
	if (ret < 0) {
		ath12k_err(ab, "request_firmware failed\n");
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_AHB, "Booting fw image %s, size %zd\n", fw_name,
		   fw->size);

	if (!fw->size) {
		ath12k_err(ab, "Invalid firmware size\n");
		ret = -EINVAL;
		goto err_fw;
	}

	pasid = (u32_encode_bits(ab_ahb->userpd_id, ATH12K_USERPD_ID_MASK)) |
		ATH12K_AHB_UPD_SWID;

	/* Load FW image to a reserved memory location */
	ret = ab_ahb->ahb_ops->mdt_load(dev, fw, fw_name, pasid,
					ab_ahb->mem_region,
					ab_ahb->mem_phys,
					ab_ahb->mem_size,
					&ab_ahb->mem_phys);
	if (ret) {
		ath12k_err(ab, "Failed to load MDT segments: %d\n", ret);
		goto err_fw;
	}

	snprintf(fw2_name, sizeof(fw2_name), "%s/%s/%s", ATH12K_FW_DIR,
		 ab->hw_params->fw.dir, ATH12K_AHB_FW2);

	ret = request_firmware(&fw2, fw2_name, dev);
	if (ret < 0) {
		ath12k_err(ab, "request_firmware failed\n");
		goto err_fw;
	}

	ath12k_dbg(ab, ATH12K_DBG_AHB, "Booting fw image %s, size %zd\n", fw2_name,
		   fw2->size);

	if (!fw2->size) {
		ath12k_err(ab, "Invalid firmware size\n");
		ret = -EINVAL;
		goto err_fw2;
	}

	ret = qcom_mdt_load_no_init(dev, fw2, fw2_name, pasid,
				    ab_ahb->mem_region, ab_ahb->mem_phys,
				    ab_ahb->mem_size, &ab_ahb->mem_phys);
	if (ret) {
		ath12k_err(ab, "Failed to load MDT segments with no init : %d\n", ret);
		goto err_fw2;
	}

	reinit_completion(&ab_ahb->userpd_spawned);
	reinit_completion(&ab_ahb->userpd_ready);
	reinit_completion(&ab_ahb->userpd_stopped);

	if (ab_ahb->scm_auth_enabled) {
		/* Authenticate FW image using peripheral ID */
		ret = qcom_scm_pas_auth_and_reset(pasid);
		if (ret) {
			ath12k_err(ab, "failed to boot the remote processor %d\n", ret);
			goto err_fw2;
		}
	}

	/* Instruct Q6 to spawn userPD thread */
	ret = qcom_smem_state_update_bits(ab_ahb->spawn_state, BIT(ab_ahb->spawn_bit),
					  BIT(ab_ahb->spawn_bit));
	if (ret) {
		ath12k_err(ab, "Failed to update spawn state %d\n", ret);
		goto err_fw2;
	}

	time_left = wait_for_completion_timeout(&ab_ahb->userpd_spawned,
						ATH12K_USERPD_SPAWN_TIMEOUT);
	if (!time_left) {
		ath12k_err(ab, "UserPD spawn wait timed out\n");
		ret = -ETIMEDOUT;
		goto reset_spawn;
	}

	time_left = wait_for_completion_timeout(&ab_ahb->userpd_ready,
						ATH12K_USERPD_READY_TIMEOUT);
	if (!time_left) {
		ath12k_err(ab, "UserPD ready wait timed out\n");
		ret = -ETIMEDOUT;
		goto reset_spawn;
	}

	ath12k_info(ab, "UserPD%d is now UP\n", ab_ahb->userpd_id);
	ab->ag->num_userpd_started++;
	clear_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags);

reset_spawn:
	qcom_smem_state_update_bits(ab_ahb->spawn_state, BIT(ab_ahb->spawn_bit), 0);
err_fw2:
	release_firmware(fw2);
err_fw:
	release_firmware(fw);
	return ret;
}

static void ath12k_ahb_power_down(struct ath12k_base *ab, bool is_suspend)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	unsigned long time_left;
	u32 pasid;
	int ret;

	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	if (ab_ahb->crash_type == ATH12K_NO_CRASH) {
		qcom_smem_state_update_bits(ab_ahb->stop_state, BIT(ab_ahb->stop_bit),
					    BIT(ab_ahb->stop_bit));

		time_left = wait_for_completion_timeout(&ab_ahb->userpd_stopped,
						ATH12K_USERPD_STOP_TIMEOUT);
		if (!time_left) {
			ath12k_err(ab, "UserPD stop wait timed out\n");
			qcom_smem_state_update_bits(ab_ahb->stop_state,
						    BIT(ab_ahb->stop_bit), 0);
			return;
		}

		qcom_smem_state_update_bits(ab_ahb->stop_state, BIT(ab_ahb->stop_bit), 0);
	}

	if (ab_ahb->scm_auth_enabled) {
		pasid = (u32_encode_bits(ab_ahb->userpd_id, ATH12K_USERPD_ID_MASK)) |
			 ATH12K_AHB_UPD_SWID;
		/* Release the firmware */
		ret = qcom_scm_pas_shutdown(pasid);
		if (ret)
			ath12k_err(ab, "scm pas shutdown failed for userPD%d\n",
				   ab_ahb->userpd_id);
	}

	ab->ag->num_userpd_started--;

	/* Turn off rootPD during rmmod and shutdown only. RootPD is handled
	 * by RProc driver in case of recovery
	 */

	if (!ab->ag->num_userpd_started &&
	    (test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ab->ag->flags) ||
	     ath12k_erp_get_sm_state() == ATH12K_ERP_ENTER_COMPLETE)) {
		rproc_shutdown(ab_ahb->tgt_rproc);
	}

	set_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags);
}

static void ath12k_ahb_init_qmi_ce_config(struct ath12k_base *ab)
{
	struct ath12k_qmi_ce_cfg *cfg = &ab->qmi.ce_cfg;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	cfg->tgt_ce_len = ab->hw_params->target_ce_count;
	cfg->tgt_ce = ab->hw_params->target_ce_config;
	cfg->svc_to_ce_map_len = ab->hw_params->svc_to_ce_map_len;
	cfg->svc_to_ce_map = ab->hw_params->svc_to_ce_map;
	ab->qmi.service_ins_id = ab->hw_params->qmi_service_ins_id;
	ab->qmi.service_ins_id += ab_ahb->userpd_id - 1;
}

#if LINUX_VERSION_IS_GEQ(6,13,0)
static void ath12k_ahb_ce_workqueue(struct work_struct *work)
{
	struct ath12k_ce_pipe *ce_pipe = from_work(ce_pipe, work, intr_wq);

	ath12k_ce_per_engine_service(ce_pipe->ab, ce_pipe->pipe_num);

	ath12k_ahb_ce_irq_enable(ce_pipe->ab, ce_pipe->pipe_num);
}
#endif

static irqreturn_t ath12k_ahb_ce_interrupt_handler(int irq, void *arg)
{
	struct ath12k_ce_pipe *ce_pipe = arg;

	if (unlikely(!ce_pipe->ab->ce_pipe_init_done))
		return IRQ_HANDLED;

	/* last interrupt received for this CE */
	ce_pipe->timestamp = jiffies;

	ath12k_ahb_ce_irq_disable(ce_pipe->ab, ce_pipe->pipe_num);
#if LINUX_VERSION_IS_GEQ(6,13,0)
	queue_work(system_bh_wq, &ce_pipe->intr_wq);
#else
	tasklet_schedule(&ce_pipe->intr_tq);
#endif

	if (ce_pipe->ab->ce.enable_ce_stats)
		ath12k_record_sched_entry_ts(ce_pipe->ab, ce_pipe->pipe_num);

	return IRQ_HANDLED;
}

static int ath12k_ahb_ext_grp_napi_poll(struct napi_struct *napi, int budget)
{
	struct ath12k_ext_irq_grp *irq_grp = container_of(napi,
						struct ath12k_ext_irq_grp,
						napi);
	int work_done;

	work_done = irq_grp->irq_handler(irq_grp->dp, irq_grp, budget);
	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		ath12k_ahb_ext_grp_enable(irq_grp);
	}

	if (work_done > budget)
		work_done = budget;

	return work_done;
}

static irqreturn_t ath12k_ahb_ext_interrupt_handler(int irq, void *arg)
{
	struct ath12k_ext_irq_grp *irq_grp = arg;

	/* last interrupt received for this group */
	irq_grp->timestamp = jiffies;

	ath12k_ahb_ext_grp_disable(irq_grp);

	napi_schedule(&irq_grp->napi);

	return IRQ_HANDLED;
}

static int
ath12k_ahb_config_ext_irq(struct ath12k_base *ab,
			  int (*irq_handler)(struct ath12k_dp *dp,
					     struct ath12k_ext_irq_grp *irq_grp,
					     int budget),
			  struct ath12k_dp *dp)

{
	const struct ath12k_hw_ring_mask *ring_mask;
	struct ath12k_ext_irq_grp *irq_grp;
	const struct hal_ops *hal_ops;
	struct net_device *napi_ndev;
	int i, j, irq, irq_idx, ret;
	u32 num_irq;

	ring_mask = ab->hw_params->ring_mask;
	hal_ops = ab->hw_params->hal_ops;
	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		irq_grp = &ab->ext_irq_grp[i];
		num_irq = 0;

		irq_grp->ab = ab;
		irq_grp->grp_id = i;
		irq_grp->irq_handler = irq_handler;
		irq_grp->dp = dp;
#if LINUX_VERSION_IS_GEQ(6,10,0)
		irq_grp->napi_ndev = alloc_netdev_dummy(0);
		napi_ndev = irq_grp->napi_ndev;
#else
		init_dummy_netdev(&irq_grp->napi_ndev);
		napi_ndev = &irq_grp->napi_ndev;
#endif
		if (!napi_ndev)
			return -ENOMEM;

		netif_napi_add_weight(napi_ndev, &irq_grp->napi,
				      ath12k_ahb_ext_grp_napi_poll,
				      TX_NAPI_BUDGET);

		for (j = 0; j < ATH12K_EXT_IRQ_NUM_MAX; j++) {
			/* For TX ring, ensure that the ring mask and the
			 * tcl_to_cmp_rbm_map point to the same ring number.
			 */
			if ((j < DP_TCL_NUM_RING_MAX) && (ring_mask->tx[i] &
			    BIT(ab->hal.tcl_to_cmp_rbm_map[j].cmp_ring_num))) {
				irq_grp->irqs[num_irq++] =
					wbm2host_tx_completions_ring1 - j;
			}

			if (ring_mask->rx[i] & BIT(j)) {
				irq_grp->irqs[num_irq++] =
					reo2host_destination_ring1 - j;
			}

			if (ring_mask->rx_err[i] & BIT(j))
				irq_grp->irqs[num_irq++] = reo2host_exception;

			if (ring_mask->rx_wbm_rel[i] & BIT(j))
				irq_grp->irqs[num_irq++] = wbm2host_rx_release;

			if (ring_mask->reo_status[i] & BIT(j))
				irq_grp->irqs[num_irq++] = reo2host_status;

			if (ring_mask->rx_mon_dest[i] & BIT(j))
				irq_grp->irqs[num_irq++] =
					rxdma2host_monitor_destination_mac1;
		}

		irq_grp->num_irq = num_irq;

		for (j = 0; j < irq_grp->num_irq; j++) {
			irq_idx = irq_grp->irqs[j];

			irq = platform_get_irq_byname(ab->pdev,
						      irq_name[irq_idx]);
			ab->irq_num[irq_idx] = irq;
			irq_set_status_flags(irq, IRQ_NOAUTOEN | IRQ_DISABLE_UNLAZY);
			ret = devm_request_irq(ab->dev, irq,
					       ath12k_ahb_ext_interrupt_handler,
					       IRQF_TRIGGER_RISING,
					       irq_name[irq_idx], irq_grp);
			if (ret)
				ath12k_warn(ab, "failed request_irq for %d\n", irq);
		}
	}

	return 0;
}

static void ath12k_ahb_ce_tasklet(struct tasklet_struct *t)
{
	struct ath12k_ce_pipe *ce_pipe = from_tasklet(ce_pipe, t, intr_tq);

	if (ce_pipe->ab->ce.enable_ce_stats)
		ath12k_record_exec_entry_ts(ce_pipe->ab, ce_pipe->pipe_num);

	ath12k_ce_per_engine_service(ce_pipe->ab, ce_pipe->pipe_num);

	if (ce_pipe->ab->ce.enable_ce_stats)
		ath12k_update_ce_stats_bucket(ce_pipe->ab, ce_pipe->pipe_num);

	ath12k_ahb_ce_irq_enable(ce_pipe->ab, ce_pipe->pipe_num);
}

int ath12k_ahb_config_irq(struct ath12k_base *ab)
{
	int irq, irq_idx, i;
	int ret = -1;

	if (ab->hif.bus == ATH12K_BUS_HYBRID) {
		init_waitqueue_head(&ab->ipci.gic_msi_waitq);
		return ath12k_pcic_config_hybrid_irq(ab);
	}

	/* Configure CE irqs */
	for (i = 0; i < ab->hw_params->ce_count; i++) {
		struct ath12k_ce_pipe *ce_pipe = &ab->ce.ce_pipe[i];

		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		irq_idx = ATH12K_IRQ_CE0_OFFSET + i;

#if LINUX_VERSION_IS_GEQ(6,13,0)
		INIT_WORK(&ce_pipe->intr_wq, ath12k_ahb_ce_workqueue);
#else
		tasklet_setup(&ce_pipe->intr_tq, ath12k_ahb_ce_tasklet);
#endif
		irq = platform_get_irq_byname(ab->pdev, irq_name[irq_idx]);
		ret = devm_request_irq(ab->dev, irq, ath12k_ahb_ce_interrupt_handler,
				       IRQF_TRIGGER_RISING, irq_name[irq_idx],
				       ce_pipe);
		if (ret)
			return ret;

		ab->irq_num[irq_idx] = irq;
	}

	return ret;
}

static int ath12k_ahb_map_service_to_pipe(struct ath12k_base *ab, u16 service_id,
					  u8 *ul_pipe, u8 *dl_pipe)
{
	const struct service_to_pipe *entry;
	bool ul_set = false, dl_set = false;
	u32 pipedir;
	int i;

	for (i = 0; i < ab->hw_params->svc_to_ce_map_len; i++) {
		entry = &ab->hw_params->svc_to_ce_map[i];

		if (__le32_to_cpu(entry->service_id) != service_id)
			continue;

		pipedir = __le32_to_cpu(entry->pipedir);
		if (pipedir == PIPEDIR_IN || pipedir == PIPEDIR_INOUT) {
			WARN_ON(dl_set);
			*dl_pipe = __le32_to_cpu(entry->pipenum);
			dl_set = true;
		}

		if (pipedir == PIPEDIR_OUT || pipedir == PIPEDIR_INOUT) {
			WARN_ON(ul_set);
			*ul_pipe = __le32_to_cpu(entry->pipenum);
			ul_set = true;
		}
	}

	if (WARN_ON(!ul_set || !dl_set))
		return -ENOENT;

	return 0;
}

static void ath12k_ahb_free_ext_irq(struct ath12k_base *ab)
{
	int i, j;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath12k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		for (j = 0; j < irq_grp->num_irq; j++)
			devm_free_irq(ab->dev, ab->irq_num[irq_grp->irqs[j]], irq_grp);

		netif_napi_del(&irq_grp->napi);
	}
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_ahb_ppeds_register_interrupts(struct ath12k_base *ab, int type, int vector,
					 int ring_num)
{
	int ret = -EINVAL, irq;
	struct platform_device *pdev = ab->pdev;
	int irq_idx;

	if (ab->dp->ppe.ppeds_soc_idx == ATH12K_PPEDS_INVALID_SOC_IDX) {
		ath12k_err(ab, "invalid soc idx in ppeds IRQ registration\n");
		goto irq_fail;
	}
	if (type == HAL_WBM2SW_RELEASE && ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM) {
		irq_idx = ATH12K_IRQ_PPE_OFFSET + 1;
		irq = platform_get_irq_byname(ab->pdev,
					      irq_name[irq_idx]);
		if (irq < 0) {
			ath12k_err(ab, "ppeds RegIRQ: invalid irq:%d for type:%d\n",
				   irq, type);
			return irq;
		}
		ab->irq_num[irq_idx] = irq;
		irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);

		snprintf(&ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL][0],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL]),
			 "ppe_wbm_rel_%d", ab->dp->ppe.ds_node_id);

		ret = devm_request_irq(&pdev->dev, irq,  ath12k_dp_ppeds_handle_tx_comp,
				       IRQF_NO_AUTOEN | IRQF_NO_SUSPEND,
				       ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL],
				       (void *)ab);

		if (ret) {
			ath12k_err(ab, "ppeds RegIRQ: req_irq fail:%d\n", ret);
			goto irq_fail;
		}
		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL] = irq;

	} else if (type == HAL_PPE2TCL) {
		irq_idx = ATH12K_IRQ_PPE_OFFSET + 2;
		irq = platform_get_irq_byname(ab->pdev,
					      irq_name[irq_idx]);
		if (irq < 0) {
			ath12k_err(ab, "ppeds RegIRQ: invalid irq:%d for type:%d\n",
				   irq, type);
			return irq;
		}
		ab->irq_num[irq_idx] = irq;
		irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);

		snprintf(&ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL][0],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL]),
			 "ppe2tcl_%d",  ab->dp->ppe.ds_node_id);

		ret = devm_request_irq(&pdev->dev, irq,  ath12k_ds_ppe2tcl_irq_handler,
				       IRQF_NO_AUTOEN | IRQF_NO_SUSPEND,
				       ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL],
				       (void *)ab);
		if (ret) {
			ath12k_err(ab, "ppeds RegIRQ: req_irq fail:%d\n", ret);
			goto irq_fail;
		}
		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL] = irq;

	} else if (type == HAL_REO2PPE) {
		irq_idx = ATH12K_IRQ_PPE_OFFSET;
		irq = platform_get_irq_byname(ab->pdev,
					      irq_name[irq_idx]);
		if (irq < 0) {
			ath12k_err(ab, "ppeds IRQreg: invalid irq:%d for type:%d\n",
				   irq, type);
			return irq;
		}
		ab->irq_num[irq_idx] = irq;
		irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);

		snprintf(&ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE][0],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE]),
			 "reo2ppe_%d", ab->dp->ppe.ds_node_id);

		ret = devm_request_irq(&pdev->dev, irq,  ath12k_ds_reo2ppe_irq_handler,
				       IRQF_SHARED | IRQF_NO_SUSPEND,
				       ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE],
				       (void *)ab);
		if (ret) {
			ath12k_err(ab, "ppeds RegIRQ: req_irq fail:%d\n", ret);
			goto irq_fail;
		}
		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE] = irq;
		disable_irq_nosync(irq);
	}

	return 0;

irq_fail:
	return ret;
}

void ath12k_ahb_ppeds_irq_disable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[type]);
}

void ath12k_ahb_ppeds_irq_enable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	enable_irq(ab->dp->ppe.ppeds_irq[type]);
}

void ath12k_ahb_ppeds_free_interrupts(struct ath12k_base *ab)
{
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL], ab);
}
#endif

static int ath12k_ahb_dp_umac_config_irq(struct ath12k_base *ab)
{
        struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
        struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
        struct device *dev = ab->dev;
        int irq, ret;
        u32 interrupt_reset_addr;

        irq = platform_get_irq_byname(ab->pdev, "umac_reset");

        if (irq < 0) {
                ath12k_err(ab, "umac reset interrupt not found in dts\n");
                return -ENXIO;
        }

        umac_reset->irq_num = irq;

        if (ab->hw_params->umac_irq_line_reset) {
                if (of_property_read_u32(dev->of_node, "qcom,umac-irq-reset-addr", &interrupt_reset_addr)) {
                        ath12k_err(ab, "qcom,umac-irq-reset-addr is not found in dts\n");
                        return -ENXIO;
                }

                ab_ahb->interrupt_reset_base_addr = ioremap(interrupt_reset_addr, PCIE_MEM_SIZE);
                if (!ab_ahb->interrupt_reset_base_addr) {
                        ath12k_err(ab, "Not requesting irq %d for umac dp reset Due to I/O remap failure\n", irq);
                        return -ENOMEM;
                }
        }

        tasklet_setup(&umac_reset->intr_tq, ath12k_umac_reset_tasklet_handler);

        ret = request_irq(irq, ath12k_umac_reset_interrupt_handler,
                          IRQF_NO_SUSPEND, "umac_dp_reset_ahb", ab);

        if (ret) {
                ath12k_err(ab, "failed to request irq %d for umac dp reset\n", irq);
                return ret;
        }

        disable_irq_nosync(umac_reset->irq_num);

        return 0;
}

static void ath12k_ahb_dp_umac_reset_enable_irq(struct ath12k_base *ab)
{
        struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

        enable_irq(umac_reset->irq_num);
}

static void ath12k_ahb_dp_umac_reset_free_irq(struct ath12k_base *ab)
{
        struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
        struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

        if (ab->hw_params->umac_irq_line_reset) {
                iounmap(ab_ahb->interrupt_reset_base_addr);
                ab_ahb->interrupt_reset_base_addr = NULL;
        }

        disable_irq_nosync(umac_reset->irq_num);
        free_irq(umac_reset->irq_num, ab);
}

void ath12k_ahb_umac_intr_line_reset(struct ath12k_base *ab)
{
        struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

        if (!ab_ahb->interrupt_reset_base_addr) {
                ath12k_err(ab, "Base address is NULL\n");
                return;
        }
        writel_relaxed(ATH12K_UMAC_INTR_LINE_RESET_VAL, ab_ahb->interrupt_reset_base_addr);
}

static struct ath12k_hif_ops ath12k_ahb_hif_ops = {
	.start = ath12k_ahb_start,
	.stop = ath12k_ahb_stop,
	.read32 = ath12k_ahb_read32,
	.write32 = ath12k_ahb_write32,
	.pmm_read32 = ath12k_ahb_pmm_read32,
	.irq_enable = ath12k_ahb_ext_irq_enable,
	.irq_disable = ath12k_ahb_ext_irq_disable,
	.map_service_to_pipe = ath12k_ahb_map_service_to_pipe,
	.power_up = ath12k_ahb_power_up,
	.power_down = ath12k_ahb_power_down,
	.ce_irq_enable = ath12k_ahb_ce_irqs_enable,
	.ce_irq_disable = ath12k_ahb_ce_irqs_disable,
	.ext_irq_setup = ath12k_ahb_config_ext_irq,
	.ext_irq_cleanup = ath12k_ahb_free_ext_irq,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppeds_register_interrupts = ath12k_ahb_ppeds_register_interrupts,
	.ppeds_free_interrupts = ath12k_ahb_ppeds_free_interrupts,
	.ppeds_irq_enable = ath12k_ahb_ppeds_irq_enable,
	.ppeds_irq_disable = ath12k_ahb_ppeds_irq_disable,
#endif
	.dp_umac_reset_irq_config = ath12k_ahb_dp_umac_config_irq,
	.dp_umac_reset_enable_irq = ath12k_ahb_dp_umac_reset_enable_irq,
	.dp_umac_reset_free_irq = ath12k_ahb_dp_umac_reset_free_irq,
};

static const struct ath12k_hif_ops ath12k_ahb_hif_ops_qcn6432 = {
	.start = ath12k_pcic_start,
	.stop = ath12k_pcic_stop,
	.cmem_read32 = ath12k_pcic_cmem_read32,
	.cmem_write32 = ath12k_pcic_cmem_write32,
	.power_down = ath12k_ahb_power_down,
	.power_up = ath12k_ahb_power_up,
	.read32 = ath12k_pcic_ipci_read32,
	.write32 = ath12k_pcic_ipci_write32,
	.irq_enable = ath12k_pcic_ext_irq_enable,
	.irq_disable = ath12k_pcic_ext_irq_disable,
	.get_msi_address =  ath12k_pcic_get_msi_address,
	.get_user_msi_vector = ath12k_pcic_get_user_msi_assignment,
	.config_static_window = ath12k_pcic_config_static_window,
	.get_msi_irq = ath12k_pcic_get_msi_irq,
	.map_service_to_pipe = ath12k_pcic_map_service_to_pipe,
	.ce_irq_enable = ath12k_pcic_ce_irqs_enable,
	.ce_irq_disable = ath12k_pcic_ce_irq_disable_sync,
	.ext_irq_setup = ath12k_pcic_cfg_hybrid_ext_irq,
	.ext_irq_cleanup = ath12k_pcic_free_ext_irq,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppeds_register_interrupts = ath12k_pcic_ppeds_register_interrupts,
	.ppeds_free_interrupts = ath12k_pcic_ppeds_free_interrupts,
	.ppeds_irq_enable = ath12k_pcic_ppeds_irq_enable,
	.ppeds_irq_disable = ath12k_pcic_ppeds_irq_disable,
#endif
	.dp_umac_intr_line_reset = ath12k_ahb_umac_intr_line_reset,
	.dp_umac_reset_irq_config = ath12k_ahb_dp_umac_config_irq,
	.dp_umac_reset_enable_irq = ath12k_ahb_dp_umac_reset_enable_irq,
	.dp_umac_reset_free_irq = ath12k_ahb_dp_umac_reset_free_irq,
};

static void ath12k_core_dump_crash_reason(struct ath12k_base *ab)
{
	size_t len;
	char *msg;

	msg = qcom_smem_get(ATH12K_SMEM_HOST, ATH12K_Q6_CRASH_REASON, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		ath12k_err(ab, "fatal error received: %s\n", msg);
	else
		ath12k_err(ab, "fatal error without message\n");
}

static void ath12k_ahb_handle_userpd_crash(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	struct ath12k_hw_group *ag = ab->ag;

	if (!test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags))
		return;

	ath12k_info(ab, "UserPD - %d CRASHED\n", ab_ahb->userpd_id);

	complete(&ab_ahb->userpd_spawned);
	complete(&ab_ahb->userpd_ready);
	complete(&ab_ahb->userpd_stopped);
	ath12k_core_dump_crash_reason(ab);
	set_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);

	if (!test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags)) {
		set_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags);
		set_bit(ATH12K_GROUP_FLAG_RECOVERY, &ag->flags);
		ab_ahb->crash_type = ATH12K_RPROC_USERPD_CRASH;
		queue_work(ab->workqueue_aux, &ab->reset_work);
	} else {
		ath12k_core_trigger_bug_on(ab);
	}
}

static irqreturn_t ath12k_userpd_irq_handler(int irq, void *data)
{
	struct ath12k_base *ab = data;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	if (irq == ab_ahb->userpd_irq_num[ATH12K_USERPD_SPAWN_IRQ]) {
		complete(&ab_ahb->userpd_spawned);
	} else if (irq == ab_ahb->userpd_irq_num[ATH12K_USERPD_READY_IRQ]) {
		complete(&ab_ahb->userpd_ready);
	} else if (irq == ab_ahb->userpd_irq_num[ATH12K_USERPD_STOP_ACK_IRQ]) {
		complete(&ab_ahb->userpd_stopped);
	} else if (irq == ab_ahb->userpd_irq_num[ATH12K_USERPD_FATAL_IRQ]) {
		ath12k_ahb_handle_userpd_crash(ab);
	} else {
		ath12k_err(ab, "Invalid userpd interrupt\n");
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int ath12k_ahb_config_rproc_irq(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	int i, ret;
	char *upd_irq_name;

	for (i = 0; i < ATH12K_USERPD_MAX_IRQ; i++) {
		ab_ahb->userpd_irq_num[i] = platform_get_irq_byname(ab->pdev,
								    ath12k_userpd_irq[i]);
		if (ab_ahb->userpd_irq_num[i] < 0)
			return ab_ahb->userpd_irq_num[i];

		upd_irq_name = devm_kzalloc(&ab->pdev->dev, ATH12K_UPD_IRQ_WRD_LEN,
					    GFP_KERNEL);
		if (!upd_irq_name)
			return -ENOMEM;

		scnprintf(upd_irq_name, ATH12K_UPD_IRQ_WRD_LEN, "UserPD%u-%s",
			  ab_ahb->userpd_id, ath12k_userpd_irq[i]);
		ret = devm_request_threaded_irq(&ab->pdev->dev, ab_ahb->userpd_irq_num[i],
						NULL, ath12k_userpd_irq_handler,
						IRQF_TRIGGER_RISING | IRQF_ONESHOT,
						upd_irq_name, ab);
		if (ret)
			return dev_err_probe(&ab->pdev->dev, ret,
					     "Request %s irq failed: %d\n",
					     ath12k_userpd_irq[i], ret);
	}

	ab_ahb->spawn_state = devm_qcom_smem_state_get(&ab->pdev->dev, "spawn",
						       &ab_ahb->spawn_bit);
	if (IS_ERR(ab_ahb->spawn_state))
		return dev_err_probe(&ab->pdev->dev, PTR_ERR(ab_ahb->spawn_state),
				     "Failed to acquire spawn state\n");

	ab_ahb->stop_state = devm_qcom_smem_state_get(&ab->pdev->dev, "stop",
						      &ab_ahb->stop_bit);
	if (IS_ERR(ab_ahb->stop_state))
		return dev_err_probe(&ab->pdev->dev, PTR_ERR(ab_ahb->stop_state),
				     "Failed to acquire stop state\n");

	init_completion(&ab_ahb->userpd_spawned);
	init_completion(&ab_ahb->userpd_ready);
	init_completion(&ab_ahb->userpd_stopped);
	return 0;
}

int ath12k_ahb_root_pd_fatal_notifier(struct notifier_block *nb,
                                     const unsigned long event, void *data)
{
	struct ath12k_ahb *ab_ahb = container_of(nb, struct ath12k_ahb, root_pd_fatal_nb);
	struct ath12k_base *ab = ab_ahb->ab;
	struct ath12k_hw_group *ag = ab->ag;

	if (event != ATH12K_RPROC_NOTIFY_CRASH)
		return NOTIFY_DONE;

	ath12k_info(ab, "RootPD CRASHED\n");
	if (!test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags))
		return NOTIFY_DONE;

	/* Disable rootPD recovery as FW recovery is disabled
	 * to collect dump in crashed state
	 */

	if (ab->fw_recovery_support == ATH12K_FW_RECOVERY_DISABLE)
		ab_ahb->tgt_rproc->recovery_disabled = true;

	set_bit(ATH12K_GROUP_FLAG_RECOVERY, &ag->flags);
	set_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);

	ab_ahb->crash_type = ATH12K_RPROC_ROOTPD_CRASH;
	ath12k_core_trigger_partner_device_crash(ab);
	queue_work(ab->workqueue_aux, &ag->reset_group_work);

	return NOTIFY_OK;
}

static void ath12k_ahb_queue_all_userpd_reset(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *partner_ab;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    partner_ab->hif.bus == ATH12K_BUS_PCI)
			continue;

		if (!test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags))
			queue_work(partner_ab->workqueue_aux, &partner_ab->reset_work);
		else
			ath12k_core_trigger_bug_on(ab);
	}
}

static int ath12k_ahb_release_all_userpd(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_ahb *ab_ahb;
	struct ath12k_base *partner_ab;
	int i, ret;
	u32 pasid;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    !(partner_ab->hif.bus == ATH12K_BUS_AHB ||
		      partner_ab->hif.bus == ATH12K_BUS_HYBRID))
			continue;

		ab_ahb = ath12k_ab_to_ahb(partner_ab);
		if (ab_ahb->scm_auth_enabled) {
			pasid = (u32_encode_bits(ab_ahb->userpd_id, ATH12K_USERPD_ID_MASK)) |
				 ATH12K_AHB_UPD_SWID;
			ret = qcom_scm_pas_shutdown(pasid);
			if (ret) {
				ath12k_err(ab, "userpd ID:- %u release failed\n",
					   ab_ahb->userpd_id);
				return ret;
			}
		}
	}
	return 0;
}

static int ath12k_ahb_root_pd_state_notifier(struct notifier_block *nb,
					     const unsigned long event, void *data)
{
	struct ath12k_ahb *ab_ahb = container_of(nb, struct ath12k_ahb, root_pd_nb);
	struct ath12k_base *ab = ab_ahb->ab;

	switch (event) {
	case ATH12K_RPROC_AFTER_POWERUP:
		ath12k_dbg(ab, ATH12K_DBG_AHB, "Root PD is UP\n");
		complete(&ab_ahb->rootpd_ready);

		if (ab_ahb->crash_type == ATH12K_RPROC_ROOTPD_CRASH &&
		    ab->fw_recovery_support)
		     ath12k_ahb_queue_all_userpd_reset(ab);

		return NOTIFY_OK;
	case ATH12K_RPROC_BEFORE_SHUTDOWN:
		ath12k_ahb_release_all_userpd(ab);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static int ath12k_ahb_register_rproc_notifier(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	ab_ahb->root_pd_nb.notifier_call = ath12k_ahb_root_pd_state_notifier;
	ab_ahb->root_pd_fatal_nb.notifier_call = ath12k_ahb_root_pd_fatal_notifier;

	init_completion(&ab_ahb->rootpd_ready);

	/* RootPD notification can be registered only once */
	if (ab_ahb->userpd_id != ATH12K_AHB_USERPD1)
		return 0;

	ab_ahb->root_pd_notifier = qcom_register_ssr_notifier(ab_ahb->tgt_rproc->name,
							      &ab_ahb->root_pd_nb);
	if (IS_ERR(ab_ahb->root_pd_notifier))
		return PTR_ERR(ab_ahb->root_pd_notifier);

	ab_ahb->root_pd_fatal_notifier =
			qcom_register_ssr_atomic_notifier(ab_ahb->tgt_rproc->name,
							  &ab_ahb->root_pd_fatal_nb);

	if (IS_ERR(ab_ahb->root_pd_fatal_notifier)) {
		qcom_unregister_ssr_notifier(ab_ahb->root_pd_notifier, &ab_ahb->root_pd_nb);
		return PTR_ERR(ab_ahb->root_pd_fatal_notifier);
	}

        return 0;
}

static void ath12k_ahb_unregister_rproc_notifier(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	if (ab_ahb->userpd_id != ATH12K_AHB_USERPD1)
		return;

	if (ab_ahb->root_pd_fatal_notifier) {

		qcom_unregister_ssr_atomic_notifier(ab_ahb->root_pd_fatal_notifier,
						    &ab_ahb->root_pd_fatal_nb);
		ab_ahb->root_pd_fatal_notifier = NULL;
	} else {
		ath12k_err(ab, "Rproc fatal notifier not registered\n");
	}

	if (ab_ahb->root_pd_notifier) {

		qcom_unregister_ssr_notifier(ab_ahb->root_pd_notifier,
					     &ab_ahb->root_pd_nb);
		ab_ahb->root_pd_notifier = NULL;
	} else {
		ath12k_err(ab, "Rproc notifier not registered\n");
	}
}

static int ath12k_ahb_get_rproc(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	struct device *dev = ab->dev;
	struct device_node *np;
	struct rproc *prproc;

	np = of_parse_phandle(dev->of_node, "qcom,rproc", 0);
	if (!np) {
		ath12k_err(ab, "failed to get q6_rproc handle\n");
		return -ENOENT;
	}

	prproc = rproc_get_by_phandle(np->phandle);
	of_node_put(np);
	if (!prproc)
		return dev_err_probe(&ab->pdev->dev, -EPROBE_DEFER,
				     "failed to get rproc\n");

	ab_ahb->tgt_rproc = prproc;

	return 0;
}

int ath12k_ahb_boot_root_pd(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	unsigned long time_left;
	int ret;

	ret = rproc_boot(ab_ahb->tgt_rproc);
	if (ret < 0) {
		ath12k_err(ab, "RootPD boot failed\n");
		return ret;
	}

	time_left = wait_for_completion_timeout(&ab_ahb->rootpd_ready,
						ATH12K_ROOTPD_READY_TIMEOUT);
	if (!time_left) {
		ath12k_err(ab, "RootPD ready wait timed out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int ath12k_ahb_configure_rproc(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	int ret;

	ret = ath12k_ahb_get_rproc(ab);
	if (ret < 0)
		return ret;

	ret = ath12k_ahb_register_rproc_notifier(ab);
	if (ret < 0) {
		ret = dev_err_probe(&ab->pdev->dev, ret,
				    "failed to register rproc notifier\n");
		goto err_put_rproc;
	}

	if (ab_ahb->tgt_rproc->state != RPROC_RUNNING) {
		ret = ath12k_ahb_boot_root_pd(ab);
		if (ret < 0) {
			ath12k_err(ab, "failed to boot the remote processor Q6\n");
			goto err_unreg_notifier;
		}
	}

	ret = ath12k_ahb_config_rproc_irq(ab);
	if (ret < 0)
		goto err_unreg_notifier;

	return ret;

err_unreg_notifier:
	ath12k_ahb_unregister_rproc_notifier(ab);

err_put_rproc:
	rproc_put(ab_ahb->tgt_rproc);
	return ret;
}

static void ath12k_ahb_deconfigure_rproc(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	ath12k_ahb_unregister_rproc_notifier(ab);
	rproc_put(ab_ahb->tgt_rproc);
}

static int ath12k_ahb_resource_init(struct ath12k_base *ab)
{
	struct platform_device *pdev = ab->pdev;
	struct resource *mem_res;
	int ret;

	if (ab->hif.bus == ATH12K_BUS_HYBRID)
		return 0;

	ab->mem = devm_platform_get_and_ioremap_resource(pdev, 0, &mem_res);
	if (IS_ERR(ab->mem)) {
		ret = dev_err_probe(&pdev->dev, PTR_ERR(ab->mem), "ioremap error\n");
		goto out;
	}

	ab->mem_len = resource_size(mem_res);

	if (ab->hw_params->ce_remap) {
		const struct ce_remap *ce_remap = ab->hw_params->ce_remap;
		/* CE register space is moved out of WCSS and the space is not
		 * contiguous, hence remapping the CE registers to a new space
		 * for accessing them.
		 */
		ab->mem_ce = ioremap(ce_remap->base, ce_remap->size);
		if (IS_ERR(ab->mem_ce)) {
			dev_err(&pdev->dev, "ce ioremap error\n");
			ret = -ENOMEM;
			goto err_mem_unmap;
		}
		ab->ce_remap = true;
		ab->cmem_offset = ce_remap->cmem_offset;
		ab->ce_remap_base_addr = ce_remap->base;
	}

	if (ab->hw_params->pmm_remap) {
		const struct pmm_remap *pmm = ab->hw_params->pmm_remap;

		ab->mem_pmm = ioremap(pmm->base, pmm->size);
		if (IS_ERR(ab->mem_pmm)) {
			dev_err(&pdev->dev, "pmm ioremap error\n");
			ret = -ENOMEM;
			goto err_ce_mem_unmap;
		}
		ab->pmm_remap = true;
		ab->pmm_remap_base_addr = pmm->base;
	}

	return 0;

err_ce_mem_unmap:
	ab->mem_pmm = NULL;
err_mem_unmap:
	ab->mem_ce = NULL;
	devm_iounmap(ab->dev, ab->mem);

out:
	ab->mem = NULL;
	return ret;
}

static void ath12k_ahb_resource_deinit(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	if (ab->mem)
		devm_iounmap(ab->dev, ab->mem);

	if (ab->mem_ce)
		iounmap(ab->mem_ce);

	ab->mem = NULL;
	ab->mem_ce = NULL;
	ab_ahb->xo_clk = NULL;
}

static enum ath12k_device_family
ath12k_ahb_get_device_family(const struct platform_device *pdev)
{
	enum ath12k_device_family device_id;
	struct ath12k_ahb_driver *driver;
	const struct of_device_id *of_id;

	for (device_id = ATH12K_DEVICE_FAMILY_WIFI7;
	     device_id < ATH12K_DEVICE_FAMILY_MAX; device_id++) {
		driver = ath12k_ahb_family_drivers[device_id];
		if (driver) {
			of_id = of_match_device(driver->id_table, &pdev->dev);
			if (of_id) {
				/* Found the driver */
				return 0;
			}
		}
	}

	return ATH12K_DEVICE_FAMILY_MAX;
}

static int ath12k_get_userpd_id(struct device *dev)
{
	int ret;
	int userpd_id = 0;
	const char *subsys_name;

	ret = of_property_read_string(dev->of_node,
				      "qcom,userpd-subsys-name",
				      &subsys_name);
	if (ret) {
		dev_err(dev, "Not multipd architecture");
		return 0;
	}

	if (strcmp(subsys_name, "q6v5_wcss_userpd1") == 0) {
		userpd_id = ATH12K_IPQ5332_USERPD_ID;
	} else if (strcmp(subsys_name, "q6v5_wcss_userpd2") == 0) {
		userpd_id = ATH12K_QCN6432_USERPD_ID_1;
	} else if (strcmp(subsys_name, "q6v5_wcss_userpd3") == 0) {
		userpd_id = ATH12K_QCN6432_USERPD_ID_2;
	}

	return userpd_id;
}

static int ath12k_ahb_probe(struct platform_device *pdev)
{
	enum ath12k_device_family device_id;
	const struct ath12k_hif_ops *hif_ops;
	struct device *dev = &pdev->dev;
	int ret, bus_type, userpd_id;
	struct ath12k_ahb *ab_ahb;
	enum ath12k_hw_rev hw_rev;
	struct ath12k_base *ab;

	userpd_id = ath12k_get_userpd_id(dev);

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "Failed to set 32-bit coherent dma\n");
		return ret;
	}

	hw_rev = (enum ath12k_hw_rev)of_device_get_match_data(dev);

	switch (hw_rev) {
		case ATH12K_HW_IPQ5424_HW10:
		case ATH12K_HW_IPQ5332_HW10:
			hif_ops = &ath12k_ahb_hif_ops;
			bus_type = ATH12K_BUS_AHB;
			break;
		case ATH12K_HW_QCN6432_HW10:
			bus_type = ATH12K_BUS_HYBRID;
			hif_ops = &ath12k_ahb_hif_ops_qcn6432;
			break;
		default:
			return -EOPNOTSUPP;
	}

	ab = ath12k_core_alloc(dev, sizeof(struct ath12k_ahb),
			       bus_type);
	if (!ab)
		return -ENOMEM;

	ab_ahb = ath12k_ab_to_ahb(ab);
	ab_ahb->ab = ab;
	ab_ahb->userpd_id = userpd_id;
	ab->pdev = pdev;
	ab->hw_rev = hw_rev;
	ab->hif.ops = hif_ops;
	platform_set_drvdata(pdev, ab);

	device_id = ath12k_ahb_get_device_family(pdev);
	if (device_id >= ATH12K_DEVICE_FAMILY_MAX) {
		ath12k_err(ab, "failed to get device family: %d\n", device_id);
		goto err_core_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_AHB, "AHB device family id: %d\n", device_id);

	ab_ahb->device_ops = &ath12k_ahb_family_drivers[device_id]->ops;

	/* Call device specific probe. This is the callback that can
	 * be used to override any ops in future
	 */
	if (ab_ahb->device_ops->probe) {
		ret = ab_ahb->device_ops->probe(pdev);
		if (ret) {
			ath12k_err(ab, "failed to probe device: %d\n", ret);
			goto err_core_free;
		}
	}
 
	ath12k_fw_map(ab);

	ret = ath12k_ahb_resource_init(ab);
	if (ret)
		goto err_core_free;

	ret = ath12k_hal_srng_init(ab);
	if (ret)
		goto err_resource_deinit;

	ret = ath12k_ce_alloc_pipes(ab);
	if (ret) {
		ath12k_err(ab, "failed to allocate ce pipes: %d\n", ret);
		goto err_hal_srng_deinit;
	}

	ath12k_ahb_init_qmi_ce_config(ab);

	ret = ath12k_ahb_configure_rproc(ab);
	if (ret)
		goto err_ce_free;

	ret = ath12k_ahb_config_irq(ab);
	if (ret) {
		ath12k_err(ab, "failed to configure irq: %d\n", ret);
		goto err_rproc_deconfigure;
	}

	/* as dp need hal srngs, dp_init op must be called after
	 * hal srng initialization
	 */
	if (ab_ahb->device_ops->dp_init) {
		ab->dp = ab_ahb->device_ops->dp_init(ab);
		if (!ab->dp) {
			ath12k_err(ab, "dp_init failed");
			goto err_rproc_deconfigure;
		}
	}

	ret = ath12k_core_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to init core: %d\n", ret);
		goto err_free_dp;
	}

	return 0;

err_free_dp:
	if (test_bit(ATH12K_FLAG_SOC_CREATE_FAIL, &ab->dev_flags))
		return ret;

	if (ab_ahb->device_ops->dp_deinit)
		ab_ahb->device_ops->dp_deinit(ab->dp);

err_rproc_deconfigure:
	ath12k_ahb_deconfigure_rproc(ab);

err_ce_free:
	ath12k_ce_free_pipes(ab);

err_hal_srng_deinit:
	ath12k_hal_srng_deinit(ab);

err_resource_deinit:
	ath12k_ahb_resource_deinit(ab);

err_core_free:
	ath12k_core_free(ab);
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static void ath12k_ahb_remove_prepare(struct ath12k_base *ab)
{
	unsigned long left;

	if (test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags)) {
		left = wait_for_completion_timeout(&ab->driver_recovery,
						   ATH12K_AHB_RECOVERY_TIMEOUT);
		if (!left)
			ath12k_warn(ab, "failed to receive recovery response completion\n");
	}

	set_bit(ATH12K_FLAG_UNREGISTERING, &ab->dev_flags);
	cancel_work_sync(&ab->reset_work);
	cancel_work_sync(&ab->restart_work);
	cancel_work_sync(&ab->qmi.event_work);
}

static void ath12k_ahb_free_resources(struct ath12k_base *ab)
{
	struct platform_device *pdev = ab->pdev;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	if (ab->hif.bus == ATH12K_BUS_HYBRID) {
		ath12k_pcic_free_hybrid_irq(ab);
		ath12k_ahb_deconfigure_rproc(ab);
		return;
	}

	ath12k_hal_srng_deinit(ab);
	ath12k_ce_free_pipes(ab);
	ath12k_ahb_resource_deinit(ab);
	ath12k_ahb_deconfigure_rproc(ab);
	if (ab_ahb->device_ops->dp_deinit)
		ab_ahb->device_ops->dp_deinit(ab->dp);
	ath12k_core_free(ab);
	platform_set_drvdata(pdev, NULL);
}

static int ath12k_ahb_remove(struct platform_device *pdev)
{
	struct ath12k_base *ab = platform_get_drvdata(pdev);

	if (test_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags)) {
		ath12k_ahb_power_down(ab, false);
		ath12k_qmi_deinit_service(ab);
		goto qmi_fail;
	}

	ath12k_ahb_remove_prepare(ab);
	ath12k_core_deinit(ab);

qmi_fail:
	ath12k_ahb_free_resources(ab);
	return 0;
}

int ath12k_ahb_register_driver(const enum ath12k_device_family device_id,
			       struct ath12k_ahb_driver *driver)
{
	struct platform_driver *ahb_driver;

	if (device_id >= ATH12K_DEVICE_FAMILY_MAX)
		return -EINVAL;

	if (ath12k_ahb_family_drivers[device_id]) {
		pr_err("Driver already for id: %d\n", device_id);
		return -EALREADY;
	}

	ath12k_ahb_family_drivers[device_id] = driver;

	ahb_driver = &ath12k_ahb_drivers[device_id];
	ahb_driver->driver.name = driver->name;
	ahb_driver->driver.of_match_table = driver->id_table;
	ahb_driver->probe  = ath12k_ahb_probe;
	ahb_driver->remove = ath12k_ahb_remove;

	return platform_driver_register(ahb_driver);
}
EXPORT_SYMBOL(ath12k_ahb_register_driver);

void ath12k_ahb_unregister_driver(const enum ath12k_device_family device_id)
{
	struct platform_driver *ahb_driver;

	if (device_id >= ATH12K_DEVICE_FAMILY_MAX)
		return;

	if (!ath12k_ahb_family_drivers[device_id])
		return;

	ahb_driver = &ath12k_ahb_drivers[device_id];
	platform_driver_unregister(ahb_driver);
	ath12k_ahb_family_drivers[device_id] = NULL;
}
EXPORT_SYMBOL(ath12k_ahb_unregister_driver);
