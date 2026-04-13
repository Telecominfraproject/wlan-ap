// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include "core.h"
#include "pcic.h"
#include "debug.h"
#include "hif.h"
#include "hw.h"
#include "ahb.h"
#include "wifi7/hal.h"

unsigned int tx_comp_budget = 0x7F;
module_param_named(tx_comp_budget, tx_comp_budget, uint, 0644);
MODULE_PARM_DESC(tx_comp_budget, "tx_comp_budget");

unsigned int ath12k_napi_poll_budget = 0x7f;
module_param_named(napi_budget, ath12k_napi_poll_budget, uint, 0644);
MODULE_PARM_DESC(napi_budget, "Napi budget processing per rx intr");

static const char *irq_name[ATH12K_IRQ_NUM_MAX] = {
	"bhi",
	"mhi-er0",
	"mhi-er1",
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
	"ce12",
	"ce13",
	"ce14",
	"ce15",
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
};

char dp_irq_name[ATH12K_MAX_PCI_DOMAINS + 1][ATH12K_EXT_IRQ_DP_NUM_VECTORS][DP_IRQ_NAME_LEN] = {};
char dp_pcic_irq_name[ATH12K_MAX_PCI_DOMAINS + 1][ATH12K_EXT_IRQ_DP_NUM_VECTORS][DP_IRQ_NAME_LEN] = {};
char ce_irq_name[ATH12K_MAX_PCI_DOMAINS + 1][ATH12K_IRQ_NUM_MAX][DP_IRQ_NAME_LEN] = {};

void ath12k_pcic_config_static_window(struct ath12k_base *ab)
{
	u32 umac_window = u32_get_bits(HAL_SEQ_WCSS_UMAC_OFFSET, WINDOW_VALUE_MASK);
	u32 ce_window = u32_get_bits(HAL_CE_WFSS_CE_REG_BASE, WINDOW_VALUE_MASK);
	u32 window;

	window = (umac_window << 12) | (ce_window << 6);

	iowrite32(WINDOW_ENABLE_BIT | window, ab->mem + WINDOW_REG_ADDRESS);
}

static void ath12k_pcic_select_static_window(struct ath12k_base *ab, u32 addr)
{
	u32 curr_window, cur_val, prev_window = 0;
	volatile u32 read_val = 0;
	int retry = 0;
	u32 window = u32_get_bits(addr, WINDOW_VALUE_MASK);

	prev_window = readl_relaxed(ab->mem + WINDOW_REG_ADDRESS);

	/* Clear out last 6 bits of window register */
	prev_window = prev_window & ~(0x3f);

	/* Write the new last 6 bits of window register. Only window 1 values
	 * are changed. Window 2 and 3 are unaffected.
	 */
	curr_window = prev_window | window;

	/* Skip writing into window register if the read value
	 * is same as calculated value.
	 */
	if (curr_window == prev_window)
		return;

	cur_val = WINDOW_ENABLE_BIT | curr_window;
	writel_relaxed(cur_val, ab->mem + WINDOW_REG_ADDRESS);

	read_val = readl_relaxed(ab->mem + WINDOW_REG_ADDRESS);

	/* If value written is not yet reflected, wait till it is reflected */
	while ((read_val != cur_val) && (retry < 10)) {
		mdelay(1);
		read_val = readl_relaxed(ab->mem + WINDOW_REG_ADDRESS);
		retry++;
	}
	if (retry == 10)
		ath12k_warn(ab, "Failed to set static window for cmem init\n");
}

u32 ath12k_pcic_cmem_read32(struct ath12k_base *ab, u32 addr)
{
	u32 val;

	if (addr < WINDOW_START)
		return readl_relaxed(ab->mem + addr);

	ath12k_pcic_select_static_window(ab, addr);

	val = readl_relaxed(ab->mem + WINDOW_START + (addr & WINDOW_RANGE_MASK));

	return val;
}

void ath12k_pcic_cmem_write32(struct ath12k_base *ab, u32 addr, u32 value)
{
	if (addr < WINDOW_START) {
		writel_relaxed(value, ab->mem + addr);
		return;
	}

	ath12k_pcic_select_static_window(ab, addr);

	writel_relaxed(value, ab->mem + WINDOW_START + (addr & WINDOW_RANGE_MASK));
}

u32 ath12k_pcic_get_window_start(struct ath12k_base *ab, u32 offset)
{
	u32 window_start;

	/* If offset lies within DP register range, use 3rd window */
	if ((offset ^ HAL_SEQ_WCSS_UMAC_OFFSET) < WINDOW_RANGE_MASK)
		window_start = 3 * WINDOW_START;
	/* If offset lies within CE register range, use 2nd window */
	else if ((offset ^ HAL_CE_WFSS_CE_REG_BASE) < WINDOW_RANGE_MASK)
		window_start = 2 * WINDOW_START;
	else
		window_start = WINDOW_START;

	return window_start;
}

void ath12k_pcic_free_ext_irq(struct ath12k_base *ab)
{
	int i, j;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath12k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		for (j = 0; j < irq_grp->num_irq; j++)
			devm_free_irq(ab->dev, ab->irq_num[irq_grp->irqs[j]], irq_grp);

		netif_napi_del(&irq_grp->napi);
#if LINUX_VERSION_IS_GEQ(6,10,0)
		free_netdev(irq_grp->napi_ndev);
#endif
	}
}

void ath12k_pcic_free_irq(struct ath12k_base *ab)
{
	int i, irq_idx;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		irq_idx = ATH12K_PCI_IRQ_CE0_OFFSET + i;
		free_irq(ab->irq_num[irq_idx], &ab->ce.ce_pipe[i]);
	}
}

void ath12k_pcic_free_hybrid_irq(struct ath12k_base *ab)
{
	struct platform_device *pdev = ab->pdev;

	ath12k_pcic_free_irq(ab);
	platform_msi_domain_free_irqs(&pdev->dev);
}

static void ath12k_pcic_ce_irq_enable(struct ath12k_base *ab, u16 ce_id)
{
	u32 irq_idx;

	irq_idx = ATH12K_PCI_IRQ_CE0_OFFSET + ce_id;
	enable_irq(ab->irq_num[irq_idx]);
}

static void ath12k_pcic_ce_irq_disable(struct ath12k_base *ab, u16 ce_id)
{
	u32 irq_idx;

	irq_idx = ATH12K_PCI_IRQ_CE0_OFFSET + ce_id;
	disable_irq_nosync(ab->irq_num[irq_idx]);
}

static void ath12k_pcic_ce_irqs_disable(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		ath12k_pcic_ce_irq_disable(ab, i);
	}
}


#if LINUX_VERSION_IS_GEQ(6,13,0)
static void ath12k_pcic_ce_workqueue(struct work_struct *work)
{
	struct ath12k_ce_pipe *ce_pipe = from_work(ce_pipe, work, intr_wq);

	ath12k_ce_per_engine_service(ce_pipe->ab, ce_pipe->pipe_num);

	ath12k_pcic_ce_irq_enable(ce_pipe->ab, ce_pipe->pipe_num);
}
#endif

static irqreturn_t ath12k_pcic_ce_interrupt_handler(int irq, void *arg)
{
	struct ath12k_ce_pipe *ce_pipe = arg;
	struct ath12k_base *ab = ce_pipe->ab;

	if (unlikely(!ab->ce_pipe_init_done))
		return IRQ_HANDLED;

	/* last interrupt received for this CE */
	ce_pipe->timestamp = jiffies;

	ath12k_pcic_ce_irq_disable(ce_pipe->ab, ce_pipe->pipe_num);
#if LINUX_VERSION_IS_GEQ(6,13,0)
	queue_work(system_bh_wq, &ce_pipe->intr_wq);
#else
	tasklet_schedule(&ce_pipe->intr_tq);
#endif
	if (ce_pipe->ab->ce.enable_ce_stats)
		ath12k_record_sched_entry_ts(ce_pipe->ab, ce_pipe->pipe_num);

	return IRQ_HANDLED;
}

static void ath12k_pcic_ext_grp_disable(struct ath12k_ext_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		disable_irq_nosync(irq_grp->ab->irq_num[irq_grp->irqs[i]]);
}

static void __ath12k_pcic_ext_irq_disable(struct ath12k_base *sc)
{
	int i;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath12k_ext_irq_grp *irq_grp = &sc->ext_irq_grp[i];

		ath12k_pcic_ext_grp_disable(irq_grp);

		/* As Umac reset will happen in atomic context doing napi_synchronize
		 * will lead to sleep in atmoic context that's why avoiding napi sync
		 * during umac reset.
		 */
		if (test_bit(ATH12K_FLAG_UMAC_PRERESET_START, &sc->dev_flags))
			continue;

		if (irq_grp->napi_enabled) {
			napi_synchronize(&irq_grp->napi);
			napi_disable(&irq_grp->napi);
			irq_grp->napi_enabled = false;
		}

	}
}

static void ath12k_pcic_ext_grp_enable(struct ath12k_ext_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		enable_irq(irq_grp->ab->irq_num[irq_grp->irqs[i]]);
}

static void ath12k_pcic_sync_ext_irqs(struct ath12k_base *ab)
{
	int i, j, irq_idx;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath12k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		for (j = 0; j < irq_grp->num_irq; j++) {
			irq_idx = irq_grp->irqs[j];
			synchronize_irq(ab->irq_num[irq_idx]);
		}
	}
}

static int ath12k_pcic_ext_grp_napi_poll(struct napi_struct *napi, int budget)
{
	struct ath12k_ext_irq_grp *irq_grp = container_of(napi,
						struct ath12k_ext_irq_grp,
						napi);
	int work_done;

	work_done = irq_grp->irq_handler(irq_grp->dp, irq_grp, budget);
	if (work_done < budget) {
		if(likely(napi_complete_done(napi, work_done)))
				ath12k_pcic_ext_grp_enable(irq_grp);
	}

	if (work_done > budget)
		work_done = budget;

	return work_done;
}

static irqreturn_t ath12k_pcic_ext_interrupt_handler(int irq, void *arg)
{
	struct ath12k_ext_irq_grp *irq_grp = arg;

	ath12k_dbg(irq_grp->ab, ATH12K_DBG_PCI, "ext irq:%d\n", irq);

	/* last interrupt received for this group */
	irq_grp->timestamp = jiffies;

	ath12k_pcic_ext_grp_disable(irq_grp);

	napi_schedule(&irq_grp->napi);

	return IRQ_HANDLED;
}

void ath12k_pcic_ce_irqs_enable(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;
		ath12k_pcic_ce_irq_enable(ab, i);
	}
}

static void ath12k_pcic_ce_tasklet(struct tasklet_struct *t)
{
        struct ath12k_ce_pipe *ce_pipe = from_tasklet(ce_pipe, t, intr_tq);

	if (ce_pipe->ab->ce.enable_ce_stats)
		ath12k_record_exec_entry_ts(ce_pipe->ab, ce_pipe->pipe_num);

	ath12k_ce_per_engine_service(ce_pipe->ab, ce_pipe->pipe_num);

	if (ce_pipe->ab->ce.enable_ce_stats)
		ath12k_update_ce_stats_bucket(ce_pipe->ab, ce_pipe->pipe_num);

	ath12k_pcic_ce_irq_enable(ce_pipe->ab, ce_pipe->pipe_num);
}

static
int ath12k_pcic_ext_cfg_gic_msi_irq(struct ath12k_base *ab,
				    int (*irq_handler)(struct ath12k_dp *dp,
						       struct ath12k_ext_irq_grp *irq_grp,
						       int budget),
				    struct ath12k_dp *dp,
				    struct msi_desc *msi_desc, int i)
{
	u32 user_base_data = 0, base_vector = 0, base_idx;
	struct ath12k_ext_irq_grp *irq_grp;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	struct platform_device *pdev = ab->pdev;
	int j, budget, ret = 0, num_vectors = 0;
	struct net_device *napi_ndev;
	int ring_num = 0, ring_type = 0;
	u8 userpd_idx;
	u32 num_irq = 0;

	userpd_idx = ab_ahb->userpd_id - 1;
	base_idx = ATH12K_PCI_IRQ_CE0_OFFSET + CE_COUNT_MAX;
	ret = ath12k_pcic_get_user_msi_assignment(ab, "DP", &num_vectors,
						  &user_base_data, &base_vector);
	if (ret < 0)
		return ret;

	irq_grp = &ab->ext_irq_grp[i];
	irq_grp->ab = ab;
	irq_grp->dp = dp;
	irq_grp->grp_id = i;
	irq_grp->irq_handler = irq_handler;

#if LINUX_VERSION_IS_GEQ(6,10,0)
		irq_grp->napi_ndev = alloc_netdev_dummy(0);
		napi_ndev = irq_grp->napi_ndev;
#else
		init_dummy_netdev(&irq_grp->napi_ndev);
		napi_ndev = &irq_grp->napi_ndev;
#endif

	if (!napi_ndev)
		return -ENOMEM;

	if (ab->hw_params->ring_mask->rx_mon_dest[i])
		budget = NAPI_POLL_WEIGHT;
	else
		budget = ath12k_napi_poll_budget;

	/* Apply a reduced budget for tx completion to prioritize tx enqueue operation */
	if (ab->hw_params->ring_mask->tx[i])
		budget = tx_comp_budget;

	if (ab->hw_params->ring_mask->tx[i] ||
	    ab->hw_params->ring_mask->rx[i] ||
	    ab->hw_params->ring_mask->rx_err[i] ||
	    ab->hw_params->ring_mask->rx_wbm_rel[i] ||
	    ab->hw_params->ring_mask->reo_status[i] ||
	    ab->hw_params->ring_mask->host2rxdma[i] ||
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	    ab->hw_params->ring_mask->ppe2tcl[i] ||
	    ab->hw_params->ring_mask->wbm2sw6_ppeds_tx_cmpln[i] ||
	    ab->hw_params->ring_mask->reo2ppe[i] ||
#endif
	    ab->hw_params->ring_mask->rx_mon_dest[i]) {
		num_irq = 1;
	}

	irq_grp->num_irq = num_irq;
	irq_grp->irqs[0] = base_idx + i;

	for (j = 0; j < irq_grp->num_irq; j++) {
		int irq_idx = irq_grp->irqs[j];
		int vector = (i % num_vectors);

		if (ab->hw_params->ring_mask->ppe2tcl[i] ||
		    ab->hw_params->ring_mask->wbm2sw6_ppeds_tx_cmpln[i] ||
		    ab->hw_params->ring_mask->reo2ppe[i]) {
			ret = ath12k_pcic_get_msi_data(ab, msi_desc, i, &ring_type, &ring_num);
			if (ret) {
				ath12k_err(ab, "failed to get msi data for irq %d: %d",
					   msi_desc->irq, ret);
				return ret;
			}
			ath12k_hif_ppeds_register_interrupts(ab, ring_type, 0, ring_num);
		} else {
			netif_napi_add_weight(napi_ndev, &irq_grp->napi,
					      ath12k_pcic_ext_grp_napi_poll, budget);
			scnprintf(dp_pcic_irq_name[userpd_idx][i], DP_IRQ_NAME_LEN,
				  "pcic%u_wlan_dp_%u", userpd_idx, i);
			irq_set_status_flags(msi_desc->irq, IRQ_DISABLE_UNLAZY);
			ret = devm_request_irq(&pdev->dev, msi_desc->irq,
					       ath12k_pcic_ext_interrupt_handler, IRQF_SHARED,
					       dp_pcic_irq_name[userpd_idx][i], irq_grp);
			if (ret) {
				ath12k_warn(ab, "failed to request irq %d: %d\n", irq_idx, ret);
				return ret;
			}

			ab->irq_num[irq_idx] = msi_desc->irq;
			ab->ipci.dp_irq_num[vector] = msi_desc->irq;
			ab->ipci.dp_msi_data[i] = msi_desc->msg.data;
			disable_irq_nosync(ab->irq_num[irq_idx]);
		}
	}
	return ret;
}

static int ath12k_pcic_config_gic_msi_irq(struct ath12k_base *ab,
					  struct platform_device *pdev,
					  struct msi_desc *msi_desc, int i)
{
	struct ath12k_ce_pipe *ce_pipe = &ab->ce.ce_pipe[i];
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	int irq_idx, ret;
	u8 userpd_idx = ab_ahb->userpd_id - 1;

#if LINUX_VERSION_IS_GEQ(6,13,0)
		INIT_WORK(&ce_pipe->intr_wq, ath12k_pcic_ce_workqueue);
#else
		tasklet_setup(&ce_pipe->intr_tq, ath12k_pcic_ce_tasklet);
#endif

	irq_idx = ATH12K_PCI_IRQ_CE0_OFFSET + i;

	scnprintf(ce_irq_name[userpd_idx][irq_idx], DP_IRQ_NAME_LEN,
		  "pci%u_wlan_ce_%u", userpd_idx, i);

	ret = devm_request_irq(&pdev->dev, msi_desc->irq,
			       ath12k_pcic_ce_interrupt_handler, IRQF_SHARED,
			       ce_irq_name[userpd_idx][irq_idx], ce_pipe);
	if (ret) {
		ath12k_warn(ab, "failed to request irq %d: %d\n", irq_idx, ret);
		return ret;
	}

	ab->irq_num[irq_idx] = msi_desc->irq;
	ab->ipci.ce_msi_data[i] = msi_desc->msg.data;
	ath12k_pcic_ce_irq_disable(ab, i);

	return ret;
}

#if LINUX_VERSION_IS_GEQ(6,13,0)
static void ath12k_pcic_cancel_workqueue(struct ath12k_base *ab)
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

static void ath12k_pcic_kill_tasklets(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		struct ath12k_ce_pipe *ce_pipe = &ab->ce.ce_pipe[i];

		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		 tasklet_kill(&ce_pipe->intr_tq);
	}
}

static void ath12k_pcic_sync_ce_irqs(struct ath12k_base *ab)
{
	int i;
	int irq_idx;

	for (i = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		irq_idx = ATH12K_PCI_IRQ_CE0_OFFSET + i;
		synchronize_irq(ab->irq_num[irq_idx]);
	}
}

void ath12k_pcic_ce_irq_disable_sync(struct ath12k_base *ab)
{
	if (ab->pm_suspend)
		return;

	ath12k_pcic_ce_irqs_disable(ab);
	ath12k_pcic_sync_ce_irqs(ab);
#if LINUX_VERSION_IS_GEQ(6,13,0)
	ath12k_pcic_cancel_workqueue(ab);
#else
	ath12k_pcic_kill_tasklets(ab);
#endif

}

int ath12k_pcic_map_service_to_pipe(struct ath12k_base *ab, u16 service_id,
				    u8 *ul_pipe, u8 *dl_pipe)
{
	const struct service_to_pipe *entry;
	bool ul_set = false, dl_set = false;
	int i;

	for (i = 0; i < ab->hw_params->svc_to_ce_map_len; i++) {
		entry = &ab->hw_params->svc_to_ce_map[i];

		if (__le32_to_cpu(entry->service_id) != service_id)
			continue;

		switch (__le32_to_cpu(entry->pipedir)) {
		case PIPEDIR_NONE:
			break;
		case PIPEDIR_IN:
			WARN_ON(dl_set);
			*dl_pipe = __le32_to_cpu(entry->pipenum);
			dl_set = true;
			break;
		case PIPEDIR_OUT:
			WARN_ON(ul_set);
			*ul_pipe = __le32_to_cpu(entry->pipenum);
			ul_set = true;
			break;
		case PIPEDIR_INOUT:
			WARN_ON(dl_set);
			WARN_ON(ul_set);
			*dl_pipe = __le32_to_cpu(entry->pipenum);
			*ul_pipe = __le32_to_cpu(entry->pipenum);
			dl_set = true;
			ul_set = true;
			break;
		}
	}

	if (WARN_ON(!ul_set || !dl_set))
		return -ENOENT;

	return 0;
}

int ath12k_pcic_get_msi_irq(struct ath12k_base *ab, unsigned int vector)
{
	return ab->msi.irqs[vector];
}

int ath12k_pcic_get_user_msi_assignment(struct ath12k_base *ab, char *user_name,
					int *num_vectors, u32 *user_base_data,
					u32 *base_vector)
{
	const struct ath12k_msi_config *msi_config = ab->msi.config;
	int idx;

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector
				+ ab->msi.ep_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			ath12k_dbg(ab, ATH12K_DBG_PCI, "Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				   user_name, *num_vectors, *user_base_data,
				   *base_vector);

			return 0;
		}
	}

	ath12k_err(ab, "Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}

void ath12k_pcic_get_msi_address(struct ath12k_base *ab, u32 *msi_addr_lo,
				 u32 *msi_addr_hi)
{
	*msi_addr_lo = ab->msi.addr_lo;
	*msi_addr_hi = ab->msi.addr_hi;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_pcic_ppeds_register_interrupts(struct ath12k_base *ab, int type, int vector,
					  int ring_num)
{
	struct platform_device *pdev;
	struct ath12k_ahb *ab_ahb;
	int ret = -EINVAL, irq;
	u8 bus_id;

	ab_ahb = ath12k_ab_to_ahb(ab);
	bus_id = ab_ahb->userpd_id - 1;
	pdev = ab->pdev;

	if (ab->dp->ppe.ppeds_soc_idx == -1) {
		ath12k_err(ab, "invalid ppeds_soc_idx in ppeds_register_interrupts\n");
		return -EINVAL;
	}

	if (type == HAL_PPE2TCL) {
		irq = ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL];
		if (!irq)
			goto irq_fail;
		irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);
		snprintf(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL]),
			 "pcic%d_ppe2tcl_%d", bus_id, ab->dp->ppe.ppeds_soc_idx);
		ret = devm_request_irq(&pdev->dev, irq,
				       ath12k_ds_ppe2tcl_irq_handler,
				       IRQF_NO_SUSPEND,
				       ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL],
				       (void *)ab);
		if (ret)
			goto irq_fail;
		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL] = irq;
	} else if (type == HAL_REO2PPE) {
		irq = ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE];
		if (!irq)
			goto irq_fail;
		irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);
		snprintf(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE]),
			 "pcic%d_reo2ppe_%d", bus_id, ab->dp->ppe.ppeds_soc_idx);
		ret = devm_request_irq(&pdev->dev, irq,
				       ath12k_ds_reo2ppe_irq_handler,
				       IRQF_SHARED,
				       ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE],
				       (void *)ab);
		if (ret)
			goto irq_fail;
		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE] = irq;
	} else if (type == HAL_WBM2SW_RELEASE && ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM) {
		irq = ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL];
		if (!irq)
			goto irq_fail;
		irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);
		snprintf(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL]),
			 "pcic%d_ppe_wbm_rel_%d", bus_id, ab->dp->ppe.ppeds_soc_idx);
		ret = devm_request_irq(&pdev->dev, irq,
				       ath12k_dp_ppeds_handle_tx_comp,
				       IRQF_SHARED,
				       ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL],
				       (void *)ab);
		if (ret)
			goto irq_fail;
		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL] = irq;
	} else {
		return 0;
	}

	disable_irq_nosync(irq);

	return 0;

irq_fail:
	return ret;
}

void ath12k_pcic_ppeds_irq_disable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[type]);
}

void ath12k_pcic_ppeds_irq_enable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	enable_irq(ab->dp->ppe.ppeds_irq[type]);
}

void ath12k_pcic_ppeds_free_interrupts(struct ath12k_base *ab)
{
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL], ab);
}

irqreturn_t ath12k_pcic_dummy_irq_handler(int irq, void *context)
{
	return IRQ_HANDLED;
}

int ath12k_pcic_get_msi_data(struct ath12k_base *ab, struct msi_desc *msi_desc,
			     int i, int *hal_ring_type, int *ring_num)
{
	int ret, type, hal_type, ring = 0;
	struct platform_device *pdev = ab->pdev;

	if (ab->hw_params->ring_mask->ppe2tcl[i]) {
		type = PPEDS_IRQ_PPE2TCL;
		hal_type = HAL_PPE2TCL;
	} else if (ab->hw_params->ring_mask->reo2ppe[i]) {
		type = PPEDS_IRQ_REO2PPE;
		hal_type = HAL_REO2PPE;
	} else if (ab->hw_params->ring_mask->wbm2sw6_ppeds_tx_cmpln[i]) {
		type = PPEDS_IRQ_PPE_WBM2SW_REL;
		hal_type = HAL_WBM2SW_RELEASE;
		ring = HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM;
	} else {
		return -EINVAL;
	}

	/* For multi-platform device, to retrieve msi base address and irq data,
	 * request a dummy irq  store the base address and data to
	 * provide the required base address/data info in
	 * hal_srng_init and srng_msi_setup API calls.
	 */
	ab->dp->ppe.ppeds_irq[type] = msi_desc->irq;
	ret = devm_request_irq(&pdev->dev, msi_desc->irq,
			       ath12k_pcic_dummy_irq_handler, IRQF_SHARED,
			       "dummy", (void *)ab);
	if (ret)
		return -EINVAL;

	ab->ipci.dp_msi_data[i] = msi_desc->msg.data;
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[type]);
	devm_free_irq(&pdev->dev, ab->dp->ppe.ppeds_irq[type], (void *)ab);
	*hal_ring_type = hal_type;
	*ring_num = ring;
	return 0;
}
#endif

void ath12k_pcic_ext_irq_enable(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		struct ath12k_ext_irq_grp *irq_grp = &ab->ext_irq_grp[i];

		if (!irq_grp->napi_enabled) {
			napi_enable(&irq_grp->napi);
			irq_grp->napi_enabled = true;
		}

		ath12k_pcic_ext_grp_enable(irq_grp);
	}

	set_bit(ATH12K_FLAG_EXT_IRQ_ENABLED, &ab->dev_flags);
}

void ath12k_pcic_ext_irq_disable(struct ath12k_base *ab)
{

	if (!test_bit(ATH12K_FLAG_EXT_IRQ_ENABLED, &ab->dev_flags) ||
	    ab->pm_suspend)
		return;

	__ath12k_pcic_ext_irq_disable(ab);

	if (!test_bit(ATH12K_FLAG_UMAC_PRERESET_START, &ab->dev_flags))
		ath12k_pcic_sync_ext_irqs(ab);
}

void ath12k_pcic_stop(struct ath12k_base *ab)
{
	ath12k_pcic_ce_irq_disable_sync(ab);
	timer_delete_sync(&ab->rx_replenish_retry);
}

int ath12k_pcic_start(struct ath12k_base *ab)
{
	ath12k_pcic_ce_irqs_enable(ab);
	ath12k_ce_rx_post_buf(ab);

	return 0;
}

u32 ath12k_pcic_ipci_read32(struct ath12k_base *ab, u32 offset)
{
	u32 val, window_start;

	window_start = ath12k_pcic_get_window_start(ab, offset);

	val = ioread32(ab->mem + window_start +
		       (offset & WINDOW_RANGE_MASK));

	return val;
}

void ath12k_pcic_ipci_write32(struct ath12k_base *ab, u32 offset, u32 value)
{
	u32 window_start;

	window_start = ath12k_pcic_get_window_start(ab, offset);

	iowrite32(value, ab->mem + window_start +
		  (offset & WINDOW_RANGE_MASK));
}

int ath12k_pcic_ext_irq_config(struct ath12k_base *ab,
			       int (*irq_handler)(struct ath12k_dp *dp,
						  struct ath12k_ext_irq_grp *irq_grp,
						  int budget),
			       struct ath12k_dp *dp)
{

	struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
	u32 user_base_data = 0, base_vector = 0, base_idx, budget;
	struct ath12k_ext_irq_grp *irq_grp;
	int i, j, n, ret, num_vectors = 0;
	struct net_device *napi_ndev;

	base_idx = ATH12K_PCI_IRQ_CE0_OFFSET + CE_COUNT_MAX;
	ret = ath12k_pcic_get_user_msi_assignment(ab, "DP", &num_vectors,
						  &user_base_data, &base_vector);
	if (ret < 0)
		return ret;

	if (ath12k_napi_poll_budget < NAPI_POLL_WEIGHT)
		ath12k_napi_poll_budget = NAPI_POLL_WEIGHT;

	for (i = 0; i < ATH12K_EXT_IRQ_GRP_NUM_MAX; i++) {
		irq_grp = &ab->ext_irq_grp[i];
		u32 num_irq = 0;

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
		if (!napi_ndev) {
			ret = -ENOMEM;
			goto fail_allocate;
		}

		if (ab->hw_params->ring_mask->rx_mon_dest[i])
			budget = NAPI_POLL_WEIGHT;
		else
			budget = ath12k_napi_poll_budget;

		/* Apply a reduced budget for tx completion to prioritize tx
		 * enqueue operation
		 */
		if (ab->hw_params->ring_mask->tx[i])
			budget = tx_comp_budget;

		netif_napi_add_weight(napi_ndev, &irq_grp->napi,
				      ath12k_pcic_ext_grp_napi_poll, budget);

		if (ab->hw_params->ring_mask->tx[i] ||
		    ab->hw_params->ring_mask->rx[i] ||
		    ab->hw_params->ring_mask->rx_err[i] ||
		    ab->hw_params->ring_mask->rx_wbm_rel[i] ||
		    ab->hw_params->ring_mask->reo_status[i] ||
		    ab->hw_params->ring_mask->host2rxdma[i] ||
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		    ab->hw_params->ring_mask->ppe2tcl[i] ||
		    ab->hw_params->ring_mask->wbm2sw6_ppeds_tx_cmpln[i] ||
		    ab->hw_params->ring_mask->reo2ppe[i] ||
#endif
		    ab->hw_params->ring_mask->rx_mon_dest[i] ||
		    ab->hw_params->ring_mask->rx_mon_status[i]) {
			num_irq = 1;
		}

		irq_grp->num_irq = num_irq;
		irq_grp->irqs[0] = base_idx + i;

		for (j = 0; j < irq_grp->num_irq; j++) {
			int irq_idx = irq_grp->irqs[j];
			int vector = (i % num_vectors) + base_vector;
			int irq = ath12k_hif_get_msi_irq(ab, vector);
			u8 bus_id = pci_domain_nr(ar_pci->pdev->bus);

			if (bus_id > ATH12K_MAX_PCI_DOMAINS) {
				ath12k_dbg(ab, ATH12K_DBG_PCI, "bus_id:%d\n",
					    bus_id);
				bus_id = ATH12K_MAX_PCI_DOMAINS;
			}

			ab->irq_num[irq_idx] = irq;

			ath12k_dbg(ab, ATH12K_DBG_PCI, "irq:%d group:%d\n", irq, i);

			scnprintf(dp_irq_name[bus_id][i], DP_IRQ_NAME_LEN,
				  "pci%u_wlan_dp_%u", bus_id, i);
			irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);
			ret = devm_request_irq(ab->dev, irq,
					       ath12k_pcic_ext_interrupt_handler,
					       IRQF_SHARED, dp_irq_name[bus_id][i],
						   irq_grp);
			if (ret) {
				ath12k_err(ab, "failed request irq %d: %d\n",
					   vector, ret);
				return ret;
			}

			disable_irq_nosync(ab->irq_num[irq_idx]);
		}
	}

	return 0;
fail_allocate:
	for (n = 0; n < i; n++) {
		irq_grp = &ab->ext_irq_grp[n];
#if LINUX_VERSION_IS_GEQ(6,10,0)
		free_netdev(&irq_grp->napi_ndev);
#endif
	}
	return ret;
}

int ath12k_pcic_config_irq(struct ath12k_base *ab)
{
	struct ath12k_ce_pipe *ce_pipe;
	u32 msi_data_start;
	u32 msi_data_count, msi_data_idx;
	u32 msi_irq_start;
	unsigned int msi_data;
	int irq, i, ret, irq_idx;

	ret = ath12k_pcic_get_user_msi_assignment(ab, "CE", &msi_data_count,
						  &msi_data_start, &msi_irq_start);
	if (ret)
		return ret;

	/* Configure CE irqs */
	for (i = 0, msi_data_idx = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		msi_data = (msi_data_idx % msi_data_count) + msi_irq_start;
		irq = ath12k_hif_get_msi_irq(ab, msi_data);
		ce_pipe = &ab->ce.ce_pipe[i];

		irq_idx = ATH12K_PCI_IRQ_CE0_OFFSET + i;

#if LINUX_VERSION_IS_GEQ(6,13,0)
		INIT_WORK(&ce_pipe->intr_wq, ath12k_pcic_ce_workqueue);
#else
		tasklet_setup(&ce_pipe->intr_tq, ath12k_pcic_ce_tasklet);
#endif
		ret = request_irq(irq, ath12k_pcic_ce_interrupt_handler,
				  IRQF_SHARED, irq_name[irq_idx],
				  ce_pipe);
		if (ret) {
			ath12k_err(ab, "failed to request irq %d: %d\n",
				   irq_idx, ret);
			return ret;
		}

		ab->irq_num[irq_idx] = irq;
		msi_data_idx++;

		ath12k_pcic_ce_irq_disable(ab, i);
	}

	return 0;
}

static void ath12k_msi_msg_handler(struct msi_desc *desc, struct msi_msg *msg)
{
	desc->msg.address_lo = msg->address_lo;
	desc->msg.address_hi = msg->address_hi;
	desc->msg.data = msg->data;
}

static
int ath12k_pcic_msi_desc_assign_irq(struct ath12k_base *ab,
				    int (*irq_handler)(struct ath12k_dp *dp,
						       struct ath12k_ext_irq_grp *irq_grp,
						       int budget),
				    int base_vector, int num_vectors,
				    struct platform_device *pdev, struct ath12k_dp *dp,
				    int *k)
{
	struct msi_desc *msi_desc;
	int ret = 0;
	int i = 0;

	msi_for_each_desc(msi_desc, &pdev->dev, MSI_DESC_ASSOCIATED) {
		if (i < base_vector) {
			i++;
			continue;
		}

		if (i >= (base_vector + num_vectors) || *k >= ATH12K_EXT_IRQ_DP_NUM_VECTORS)
			break;

		ret = ath12k_pcic_ext_cfg_gic_msi_irq(ab, irq_handler, dp,
						      msi_desc, *k);
		if (ret) {
			ath12k_warn(ab, "failed to config ext msi irq %d\n", ret);
			break;
		}

		(*k)++;
		i++;
	}

	return ret;
}

int ath12k_pcic_cfg_hybrid_ext_irq(struct ath12k_base *ab,
				   int (*irq_handler)(struct ath12k_dp *dp,
					   struct ath12k_ext_irq_grp *irq_grp,
					   int budget),
				   struct ath12k_dp *dp)
{
	int ret, k = 0;
	struct platform_device *pdev = ab->pdev;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	int user_base_data, base_vector, num_vectors = 0;

	if (ab_ahb->userpd_id != ATH12K_QCN6432_USERPD_ID_1 &&
	    ab_ahb->userpd_id != ATH12K_QCN6432_USERPD_ID_2) {
		ath12k_warn(ab, "ath12k userpd invalid %d\n", ab_ahb->userpd_id);
		return -ENODEV;
	}

	msi_lock_descs(&pdev->dev);

	ret = ath12k_pcic_get_user_msi_assignment(ab, "DP", &num_vectors,
						  &user_base_data, &base_vector);
	if (ret < 0) {
		ath12k_warn(ab, "failed to fetch msi data for DP");
		msi_unlock_descs(&pdev->dev);
		return -EINVAL;
	}

	ret = ath12k_pcic_msi_desc_assign_irq(ab, irq_handler, base_vector, num_vectors,
					      pdev, dp, &k);

	/* This is needed to handle the case where the irqs need to be shared
	 * when there are no enough msi lines available */
	ret = ath12k_pcic_msi_desc_assign_irq(ab, irq_handler, base_vector, num_vectors,
					      pdev, dp, &k);

	msi_unlock_descs(&pdev->dev);

	return ret;
}

int ath12k_pcic_config_hybrid_irq(struct ath12k_base *ab)
{
	int ret;
	struct platform_device *pdev = ab->pdev;
	struct msi_desc *msi_desc;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
	bool ce_done = false;
	int user_base_data, base_vector, num_vectors = 0;
	int i = 0, j = 0;

	if (ab_ahb->userpd_id != ATH12K_QCN6432_USERPD_ID_1 &&
	    ab_ahb->userpd_id != ATH12K_QCN6432_USERPD_ID_2) {
		ath12k_warn(ab, "ath12k userpd invalid %d\n", ab_ahb->userpd_id);
		return -ENODEV;
	}

	ab->msi.config = &ath12k_wifi7_msi_config[ATH12K_MSI_CONFIG_IPCI];

	ret = platform_msi_domain_alloc_irqs(&pdev->dev, ab->msi.config->total_vectors,
					     ath12k_msi_msg_handler);

	if (ret) {
		ath12k_warn(ab, "failed to alloc irqs %d ab %pM\n", ret, ab);
		return ret;
	}

	msi_lock_descs(&pdev->dev);

	ret = ath12k_pcic_get_user_msi_assignment(ab, "CE", &num_vectors,
						  &user_base_data, &base_vector);
	if (ret < 0)
		return ret;
	//TODO: Need to optimize the below code to have one loop
	msi_for_each_desc(msi_desc, &pdev->dev, MSI_DESC_ASSOCIATED) {
		if (i < base_vector) {
			i++;
			continue;
		}

		if (j < ab->hw_params->ce_count && i < (num_vectors + base_vector)) {
			while(j < ab->hw_params->ce_count &&
			      ath12k_ce_get_attr_flags(ab, j) & CE_ATTR_DIS_INTR) {
				++j;
			}

			ret = ath12k_pcic_config_gic_msi_irq(ab, pdev, msi_desc, j);
			if (ret) {
				ath12k_warn(ab, "failed to request irq %d\n", ret);
				return ret;
			}

			if (j == 0) {
				ab->msi.addr_lo = msi_desc->msg.address_lo;
				ab->msi.addr_hi = msi_desc->msg.address_hi;
				ab->msi.ep_base_data = msi_desc->msg.data;
				ath12k_info(ab, "msi ep base data %d\n", ab->msi.ep_base_data);
			}

			j++;
			if (j >= ab->hw_params->ce_count)
				ce_done = true;
		}
		i++;
	}

	msi_unlock_descs(&pdev->dev);
	i = 0;

	msi_lock_descs(&pdev->dev);
	msi_for_each_desc(msi_desc, &pdev->dev, MSI_DESC_ASSOCIATED) {
		if (ce_done)
			break;

                if (i < base_vector) {
                        i++;
                        continue;
		}

		if (i < (num_vectors + base_vector)) {
			if (j < ab->hw_params->ce_count) {
				while(j < ab->hw_params->ce_count &&
				      ath12k_ce_get_attr_flags(ab, j) & CE_ATTR_DIS_INTR) {
					j++;
				}

				if (j == ab->hw_params->ce_count) {
					ce_done = true;
					break;
				}

				ret = ath12k_pcic_config_gic_msi_irq(ab, pdev, msi_desc, j);
				if (ret) {
					ath12k_warn(ab, "failed to request irq %d\n", ret);
					return ret;
				}

				j++;
			}
		}

		i++;
	}

	msi_unlock_descs(&pdev->dev);
	ab->ipci.gic_enabled = 1;
	wake_up(&ab->ipci.gic_msi_waitq);

	return ret;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_pci_ppeds_register_interrupts(struct ath12k_base *ab, int type, int vector,
					 int ring_num)
{
	struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
	int irq;
	u8 bus_id = pci_domain_nr(ar_pci->pdev->bus);
	int ret;

	if (type != HAL_REO2PPE && type != HAL_PPE2TCL &&
	    !(type == HAL_WBM2SW_RELEASE &&
	    ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM)) {
		return 0;
	}

	if (ab->dp->ppe.ppeds_soc_idx == -1) {
		ath12k_err(ab, "invalid ppeds_node_idx in ppeds_register_interrupts\n");
		return -EINVAL;
	}

	irq = ath12k_pci_get_msi_irq(ab, vector);

	irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);
	if (type == HAL_PPE2TCL) {
		snprintf(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL]),
			 "pci%d_ppe2tcl_%d", bus_id, ab->dp->ppe.ppeds_soc_idx);
		ret = devm_request_irq(ab->dev, irq,  ath12k_ds_ppe2tcl_irq_handler,
				       IRQF_SHARED,
					   ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE2TCL],
					   (void *)ab);
		if (ret)
			goto irq_fail;

		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL] = irq;
	} else if (type == HAL_REO2PPE) {
		snprintf(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE]),
			 "pci%d_reo2ppe%d_irq%d", bus_id, ab->dp->ppe.ppeds_soc_idx,
			 irq);
		ret = devm_request_irq(ab->dev, irq, ath12k_ds_reo2ppe_irq_handler,
				       IRQF_SHARED,
					   ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_REO2PPE],
					   (void *)ab);
		if (ret)
			goto irq_fail;

		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE] = irq;
	} else if (type == HAL_WBM2SW_RELEASE &&
		   ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM) {
		snprintf(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL],
			 sizeof(ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL]),
			 "pci%d_ppe_wbm_rel_%d", bus_id, ab->dp->ppe.ppeds_soc_idx);
		ret = devm_request_irq(ab->dev, irq,  ath12k_dp_ppeds_handle_tx_comp,
				       IRQF_SHARED,
				       ab->dp->ppe.ppeds_irq_name[PPEDS_IRQ_PPE_WBM2SW_REL],
				       (void *)ab);
		if (ret)
			goto irq_fail;

		ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL] = irq;
	} else {
		return 0;
	}

	disable_irq_nosync(irq);

	return 0;
irq_fail:
	return ret;
}

void ath12k_pci_ppeds_irq_disable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[type]);
}

void ath12k_pci_ppeds_irq_enable(struct ath12k_base *ab, enum ppeds_irq_type type)
{
	enable_irq(ab->dp->ppe.ppeds_irq[type]);
}

void ath12k_pci_ppeds_free_interrupts(struct ath12k_base *ab)
{
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL]);
	devm_free_irq(ab->dev, ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE_WBM2SW_REL], ab);
}
#endif
