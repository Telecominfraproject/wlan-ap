// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/vmalloc.h>

#include "pci.h"
#include "core.h"
#include "hif.h"
#include "mhi.h"
#include "debug.h"
#include "debugfs.h"
#include "fw.h"
#include "pcic.h"

#define ATH12K_PCI_BAR_NUM		0
#define ATH12K_PCI_DMA_MASK		32

#define WINDOW_STATIC_MASK		GENMASK(31, 6)

/* BAR0 + 4k is always accessible, and no
 * need to force wakeup.
 * 4K - 32 = 0xFE0
 */
#define ACCESS_ALWAYS_OFF 0xFE0

#define PCIE_LOCAL_REG_QRTR_NODE_ID	0x1E03164
#define DOMAIN_NUMBER_MASK		GENMASK(7, 4)
#define BUS_NUMBER_MASK			GENMASK(3, 0)

static struct ath12k_pci_driver *ath12k_pci_family_drivers[ATH12K_DEVICE_FAMILY_MAX];
static struct pci_driver ath12k_pci_drivers[ATH12K_DEVICE_FAMILY_MAX];

static void ath12k_pci_select_window(struct ath12k_pci *ab_pci, u32 offset)
{
	struct ath12k_base *ab = ab_pci->ab;

	u32 window = u32_get_bits(offset, WINDOW_VALUE_MASK);
	u32 static_window;

	lockdep_assert_held(&ab_pci->window_lock);

	/* Preserve the static window configuration and reset only dynamic window */
	static_window = ab_pci->register_window & WINDOW_STATIC_MASK;
	window |= static_window;

	if (window != ab_pci->register_window) {
		iowrite32(WINDOW_ENABLE_BIT | window,
			  ab->mem + WINDOW_REG_ADDRESS);
		ioread32(ab->mem + WINDOW_REG_ADDRESS);
		ab_pci->register_window = window;
	}
}

static void ath12k_pci_select_static_window(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	u32 umac_window;
	u32 ce_window;
	u32 window;

	umac_window = u32_get_bits(ab_pci->reg_base->umac_base, WINDOW_VALUE_MASK);
	ce_window = u32_get_bits(ab_pci->reg_base->ce_reg_base, WINDOW_VALUE_MASK);
	window = (umac_window << 12) | (ce_window << 6);

	spin_lock_bh(&ab_pci->window_lock);
	ab_pci->register_window = window;
	spin_unlock_bh(&ab_pci->window_lock);

	iowrite32(WINDOW_ENABLE_BIT | window, ab->mem + WINDOW_REG_ADDRESS);
}

static inline bool ath12k_pci_is_offset_within_mhi_region(u32 offset)
{
	return (offset >= PCI_MHIREGLEN_REG && offset <= PCI_MHI_REGION_END);
}

static void ath12k_pci_soc_global_reset(struct ath12k_base *ab)
{
	u32 val, delay;

	val = ath12k_pci_read32(ab, PCIE_SOC_GLOBAL_RESET);

	val |= PCIE_SOC_GLOBAL_RESET_V;

	ath12k_pci_write32(ab, PCIE_SOC_GLOBAL_RESET, val);

	/* TODO: exact time to sleep is uncertain */
	delay = 10;
	mdelay(delay);

	/* Need to toggle V bit back otherwise stuck in reset status */
	val &= ~PCIE_SOC_GLOBAL_RESET_V;

	ath12k_pci_write32(ab, PCIE_SOC_GLOBAL_RESET, val);

	mdelay(delay);

	val = ath12k_pci_read32(ab, PCIE_SOC_GLOBAL_RESET);
	if (val == 0xffffffff)
		ath12k_warn(ab, "link down error during global reset\n");
}

static void ath12k_pci_clear_dbg_registers(struct ath12k_base *ab)
{
	u32 val;

	/* read cookie */
	val = ath12k_pci_read32(ab, PCIE_Q6_COOKIE_ADDR);
	ath12k_dbg(ab, ATH12K_DBG_PCI, "cookie:0x%x\n", val);

	val = ath12k_pci_read32(ab, WLAON_WARM_SW_ENTRY);
	ath12k_dbg(ab, ATH12K_DBG_PCI, "WLAON_WARM_SW_ENTRY 0x%x\n", val);

	/* TODO: exact time to sleep is uncertain */
	mdelay(10);

	/* write 0 to WLAON_WARM_SW_ENTRY to prevent Q6 from
	 * continuing warm path and entering dead loop.
	 */
	ath12k_pci_write32(ab, WLAON_WARM_SW_ENTRY, 0);
	mdelay(10);

	val = ath12k_pci_read32(ab, WLAON_WARM_SW_ENTRY);
	ath12k_dbg(ab, ATH12K_DBG_PCI, "WLAON_WARM_SW_ENTRY 0x%x\n", val);

	/* A read clear register. clear the register to prevent
	 * Q6 from entering wrong code path.
	 */
	val = ath12k_pci_read32(ab, WLAON_SOC_RESET_CAUSE_REG);
	ath12k_dbg(ab, ATH12K_DBG_PCI, "soc reset cause:%d\n", val);
}

static void ath12k_pci_enable_ltssm(struct ath12k_base *ab)
{
	u32 val;
	int i;

	val = ath12k_pci_read32(ab, PCIE_PCIE_PARF_LTSSM);

	/* PCIE link seems very unstable after the Hot Reset*/
	for (i = 0; val != PARM_LTSSM_VALUE && i < 5; i++) {
		if (val == 0xffffffff)
			mdelay(5);

		ath12k_pci_write32(ab, PCIE_PCIE_PARF_LTSSM, PARM_LTSSM_VALUE);
		val = ath12k_pci_read32(ab, PCIE_PCIE_PARF_LTSSM);
	}

	ath12k_dbg(ab, ATH12K_DBG_PCI, "pci ltssm 0x%x\n", val);

	val = ath12k_pci_read32(ab, GCC_GCC_PCIE_HOT_RST);
	val |= GCC_GCC_PCIE_HOT_RST_VAL;
	ath12k_pci_write32(ab, GCC_GCC_PCIE_HOT_RST, val);
	val = ath12k_pci_read32(ab, GCC_GCC_PCIE_HOT_RST);

	ath12k_dbg(ab, ATH12K_DBG_PCI, "pci pcie_hot_rst 0x%x\n", val);

	mdelay(5);
}

static void ath12k_pci_clear_all_intrs(struct ath12k_base *ab)
{
	/* This is a WAR for PCIE Hotreset.
	 * When target receive Hotreset, but will set the interrupt.
	 * So when download SBL again, SBL will open Interrupt and
	 * receive it, and crash immediately.
	 */
	ath12k_pci_write32(ab, PCIE_PCIE_INT_ALL_CLEAR, PCIE_INT_CLEAR_ALL);
}

static void ath12k_pci_set_wlaon_pwr_ctrl(struct ath12k_base *ab)
{
	u32 val;

	val = ath12k_pci_read32(ab, WLAON_QFPROM_PWR_CTRL_REG);
	val &= ~QFPROM_PWR_CTRL_VDD4BLOW_MASK;
	ath12k_pci_write32(ab, WLAON_QFPROM_PWR_CTRL_REG, val);
}

static void ath12k_pci_force_wake(struct ath12k_base *ab)
{
	ath12k_pci_write32(ab, PCIE_SOC_WAKE_PCIE_LOCAL_REG, 1);
	mdelay(5);
}

static void ath12k_pci_sw_reset(struct ath12k_base *ab, bool power_on)
{
	if (power_on) {
		ath12k_pci_enable_ltssm(ab);
		ath12k_pci_clear_all_intrs(ab);
		ath12k_pci_set_wlaon_pwr_ctrl(ab);
	}

	ath12k_mhi_clear_vector(ab);
	ath12k_pci_clear_dbg_registers(ab);
	ath12k_pci_soc_global_reset(ab);
	ath12k_mhi_set_mhictrl_reset(ab);
}

static int ath12k_pci_set_irq_affinity_hint(struct ath12k_pci *ab_pci,
					    const struct cpumask *m)
{
	if (test_bit(ATH12K_PCI_FLAG_MULTI_MSI_VECTORS, &ab_pci->flags))
		return 0;

	return irq_set_affinity_and_hint(ab_pci->pdev->irq, m);
}

static void ath12k_pci_init_qmi_ce_config(struct ath12k_base *ab)
{
	struct ath12k_qmi_ce_cfg *cfg = &ab->qmi.ce_cfg;

	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	struct pci_bus *bus = ab_pci->pdev->bus;

	cfg->tgt_ce = ab->hw_params->target_ce_config;
	cfg->tgt_ce_len = ab->hw_params->target_ce_count;

	cfg->svc_to_ce_map = ab->hw_params->svc_to_ce_map;
	cfg->svc_to_ce_map_len = ab->hw_params->svc_to_ce_map_len;
	ab->qmi.service_ins_id = ab->hw_params->qmi_service_ins_id;

	if (ab->fw.api_version == ATH12K_FW_API_V1 ||test_bit(ATH12K_FW_FEATURE_MULTI_QRTR_ID, ab->fw.fw_features)) {
		ab_pci->qmi_instance =
			u32_encode_bits(pci_domain_nr(bus), DOMAIN_NUMBER_MASK) |
			u32_encode_bits(bus->number, BUS_NUMBER_MASK);
		ab->qmi.service_ins_id += ab_pci->qmi_instance;
	}
}

static void ath12k_pci_msi_config(struct ath12k_pci *ab_pci, bool enable)
{
	struct pci_dev *dev = ab_pci->pdev;
	u16 control;

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &control);

	if (enable)
		control |= PCI_MSI_FLAGS_ENABLE;
	else
		control &= ~PCI_MSI_FLAGS_ENABLE;

	pci_write_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, control);
}

static void ath12k_pci_msi_enable(struct ath12k_pci *ab_pci)
{
	ath12k_pci_msi_config(ab_pci, true);
}

static void ath12k_pci_msi_disable(struct ath12k_pci *ab_pci)
{
	ath12k_pci_msi_config(ab_pci, false);
}

static int ath12k_pci_msi_alloc(struct ath12k_pci *ab_pci)
{
	struct ath12k_base *ab = ab_pci->ab;
	const struct ath12k_msi_config *msi_config = ab->msi.config;
	struct msi_desc *msi_desc;
	int num_vectors;
	int ret;

	num_vectors = pci_alloc_irq_vectors(ab_pci->pdev,
					    msi_config->total_vectors,
					    msi_config->total_vectors,
					    PCI_IRQ_MSI);

	if (num_vectors == msi_config->total_vectors) {
		set_bit(ATH12K_PCI_FLAG_MULTI_MSI_VECTORS, &ab_pci->flags);
		ab_pci->irq_flags = IRQF_SHARED;
	} else {
		num_vectors = pci_alloc_irq_vectors(ab_pci->pdev,
						    1,
						    1,
						    PCI_IRQ_MSI);
		if (num_vectors < 0) {
			ret = -EINVAL;
			goto reset_msi_config;
		}
		clear_bit(ATH12K_PCI_FLAG_MULTI_MSI_VECTORS, &ab_pci->flags);
	//	ab_pci->msi_config = &msi_config_one_msi;
		ab_pci->irq_flags = IRQF_SHARED | IRQF_NOBALANCING;
		ath12k_dbg(ab, ATH12K_DBG_PCI, "request MSI one vector\n");
	}

	ath12k_info(ab, "MSI vectors: %d\n", num_vectors);

	ath12k_pci_msi_disable(ab_pci);

	msi_desc = irq_get_msi_desc(ab_pci->pdev->irq);
	if (!msi_desc) {
		ath12k_err(ab, "msi_desc is NULL!\n");
		ret = -EINVAL;
		goto free_msi_vector;
	}

	ab->msi.ep_base_data = msi_desc->msg.data;
	if (msi_desc->pci.msi_attrib.is_64)
		set_bit(ATH12K_PCI_FLAG_IS_MSI_64, &ab_pci->flags);

	ath12k_dbg(ab, ATH12K_DBG_PCI, "msi base data is %d\n", ab->msi.ep_base_data);

	return 0;

free_msi_vector:
	pci_free_irq_vectors(ab_pci->pdev);

reset_msi_config:
	return ret;
}

static void ath12k_pci_msi_free(struct ath12k_pci *ab_pci)
{
	struct ath12k_base *ab = ab_pci->ab;

	if (ab->msi.config->total_vectors == ATH12K_MSI_16)
		kfree(ab->hw_params);
	pci_free_irq_vectors(ab_pci->pdev);
}

static int ath12k_pci_config_msi_data(struct ath12k_pci *ab_pci)
{
	struct msi_desc *msi_desc;

	msi_desc = irq_get_msi_desc(ab_pci->pdev->irq);
	if (!msi_desc) {
		ath12k_err(ab_pci->ab, "msi_desc is NULL!\n");
		pci_free_irq_vectors(ab_pci->pdev);
		return -EINVAL;
	}

	ab_pci->msi_ep_base_data = msi_desc->msg.data;

	ath12k_dbg(ab_pci->ab, ATH12K_DBG_PCI, "pci after request_irq msi_ep_base_data %d\n",
		   ab_pci->msi_ep_base_data);

	return 0;
}

static int ath12k_pci_claim(struct ath12k_pci *ab_pci, struct pci_dev *pdev)
{
	struct ath12k_base *ab = ab_pci->ab;
	u16 device_id;
	int ret = 0;

	pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);
	if (device_id != ab_pci->dev_id)  {
		ath12k_err(ab, "pci device id mismatch: 0x%x 0x%x\n",
			   device_id, ab_pci->dev_id);
		ret = -EIO;
		goto out;
	}

	ret = pci_assign_resource(pdev, ATH12K_PCI_BAR_NUM);
	if (ret) {
		ath12k_err(ab, "failed to assign pci resource: %d\n", ret);
		goto out;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		ath12k_err(ab, "failed to enable pci device: %d\n", ret);
		goto out;
	}

	ret = pci_request_region(pdev, ATH12K_PCI_BAR_NUM, "ath12k_pci");
	if (ret) {
		ath12k_err(ab, "failed to request pci region: %d\n", ret);
		goto disable_device;
	}

	ret = dma_set_mask_and_coherent(&pdev->dev,
					DMA_BIT_MASK(ATH12K_PCI_DMA_MASK));
	if (ret) {
		ath12k_err(ab, "failed to set pci dma mask to %d: %d\n",
			   ATH12K_PCI_DMA_MASK, ret);
		goto release_region;
	}

	pci_set_master(pdev);

	ab->mem_len = pci_resource_len(pdev, ATH12K_PCI_BAR_NUM);
	ab->mem = pci_iomap(pdev, ATH12K_PCI_BAR_NUM, 0);
	if (!ab->mem) {
		ath12k_err(ab, "failed to map pci bar %d\n", ATH12K_PCI_BAR_NUM);
		ret = -EIO;
		goto release_region;
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot pci_mem 0x%p\n", ab->mem);
	return 0;

release_region:
	pci_release_region(pdev, ATH12K_PCI_BAR_NUM);
disable_device:
	pci_disable_device(pdev);
out:
	return ret;
}

static void ath12k_pci_free_region(struct ath12k_pci *ab_pci)
{
	struct ath12k_base *ab = ab_pci->ab;
	struct pci_dev *pci_dev = ab_pci->pdev;

	pci_iounmap(pci_dev, ab->mem);
	ab->mem = NULL;
	pci_release_region(pci_dev, ATH12K_PCI_BAR_NUM);
	if (pci_is_enabled(pci_dev))
		pci_disable_device(pci_dev);
}

static void ath12k_pci_aspm_disable(struct ath12k_pci *ab_pci)
{
	struct ath12k_base *ab = ab_pci->ab;

	pcie_capability_read_word(ab_pci->pdev, PCI_EXP_LNKCTL,
				  &ab_pci->link_ctl);

	ath12k_dbg(ab, ATH12K_DBG_PCI, "pci link_ctl 0x%04x L0s %d L1 %d\n",
		   ab_pci->link_ctl,
		   u16_get_bits(ab_pci->link_ctl, PCI_EXP_LNKCTL_ASPM_L0S),
		   u16_get_bits(ab_pci->link_ctl, PCI_EXP_LNKCTL_ASPM_L1));

	/* disable L0s and L1 */
	pcie_capability_clear_word(ab_pci->pdev, PCI_EXP_LNKCTL,
				   PCI_EXP_LNKCTL_ASPMC);

	set_bit(ATH12K_PCI_ASPM_RESTORE, &ab_pci->flags);
}

static void ath12k_pci_update_qrtr_node_id(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	u32 reg;

	/* On platforms with two or more identical mhi devices, qmi service run
	 * with identical qrtr-node-id. Because of this identical ID qrtr-lookup
	 * cannot register more than one qmi service with identical node ID.
	 *
	 * This generates a unique instance ID from PCIe domain number and bus number,
	 * writes to the given register, it is available for firmware when the QMI service
	 * is spawned.
	 */
	reg = PCIE_LOCAL_REG_QRTR_NODE_ID & WINDOW_RANGE_MASK;
	ath12k_pci_write32(ab, reg, ab_pci->qmi_instance);

	ath12k_dbg(ab, ATH12K_DBG_PCI, "pci reg 0x%x instance 0x%x read val 0x%x\n",
		   reg, ab_pci->qmi_instance, ath12k_pci_read32(ab, reg));
}

static void ath12k_pci_aspm_restore(struct ath12k_pci *ab_pci)
{
	if (ab_pci->ab->hw_params->supports_aspm &&
	    test_and_clear_bit(ATH12K_PCI_ASPM_RESTORE, &ab_pci->flags))
		pcie_capability_clear_and_set_word(ab_pci->pdev, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_ASPMC,
						   ab_pci->link_ctl &
						   PCI_EXP_LNKCTL_ASPMC);
}

int ath12k_pci_map_service_to_pipe(struct ath12k_base *ab, u16 service_id,
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

int ath12k_pci_get_msi_irq(struct ath12k_base *ab, unsigned int vector)
{
	struct device *dev = ab->dev;
        struct pci_dev *pci_dev = to_pci_dev(dev);

        return pci_irq_vector(pci_dev, vector);
}

int ath12k_pci_get_user_msi_assignment(struct ath12k_base *ab, char *user_name,
				       int *num_vectors, u32 *user_base_data,
				       u32 *base_vector)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	const struct ath12k_msi_config *msi_config = ab_pci->msi_config;
	int idx;

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*base_vector =  msi_config->users[idx].base_vector;
			*user_base_data = *base_vector + ab_pci->msi_ep_base_data;

			ath12k_dbg(ab, ATH12K_DBG_PCI,
				   "Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				   user_name, *num_vectors, *user_base_data,
				   *base_vector);

			return 0;
		}
	}

	ath12k_err(ab, "Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}

void ath12k_pci_get_msi_address(struct ath12k_base *ab, u32 *msi_addr_lo,
				u32 *msi_addr_hi)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	struct pci_dev *pci_dev = to_pci_dev(ab->dev);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_LO,
			      msi_addr_lo);

	if (test_bit(ATH12K_PCI_FLAG_IS_MSI_64, &ab_pci->flags)) {
		pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_HI,
				      msi_addr_hi);
	} else {
		*msi_addr_hi = 0;
	}
}

void ath12k_pci_get_ce_msi_idx(struct ath12k_base *ab, u32 ce_id,
			       u32 *msi_idx)
{
	u32 i, msi_data_idx;

	for (i = 0, msi_data_idx = 0; i < ab->hw_params->ce_count; i++) {
		if (ath12k_ce_get_attr_flags(ab, i) & CE_ATTR_DIS_INTR)
			continue;

		if (ce_id == i)
			break;

		msi_data_idx++;
	}
	*msi_idx = msi_data_idx;
}


int ath12k_pci_hif_suspend(struct ath12k_base *ab)
{
	struct ath12k_pci *ar_pci = ath12k_pci_priv(ab);

	ath12k_mhi_suspend(ar_pci);

	return 0;
}

int ath12k_pci_hif_resume(struct ath12k_base *ab)
{
	struct ath12k_pci *ar_pci = ath12k_pci_priv(ab);

	ath12k_mhi_resume(ar_pci);

	return 0;
}

void ath12k_pci_stop(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);

	if (!test_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags))
		return;

	ath12k_pcic_ce_irq_disable_sync(ab);
}

int ath12k_pci_start(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);

	set_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags);

	if (test_bit(ATH12K_PCI_FLAG_MULTI_MSI_VECTORS, &ab_pci->flags))
		ath12k_pci_aspm_restore(ab_pci);
	else
		ath12k_info(ab, "leaving PCI ASPM disabled to avoid MHI M2 problems\n");

	ath12k_pcic_start(ab);
	return 0;
}

u32 ath12k_pci_read32(struct ath12k_base *ab, u32 offset)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	u32 val, window_start;
	int ret = 0;

	/* for offset beyond BAR + 4K - 32, may
	 * need to wakeup MHI to access.
	 */
	if (test_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags) &&
	    offset >= ACCESS_ALWAYS_OFF && ab_pci->pci_ops->wakeup)
		ret = ab_pci->pci_ops->wakeup(ab);

	if (offset < WINDOW_START) {
		val = ioread32(ab->mem + offset);
	} else {
		if (ab->static_window_map)
			window_start = ath12k_pcic_get_window_start(ab, offset);
		else
			window_start = WINDOW_START;

		if (window_start == WINDOW_START) {
			spin_lock_bh(&ab_pci->window_lock);
			ath12k_pci_select_window(ab_pci, offset);

			if (ath12k_pci_is_offset_within_mhi_region(offset)) {
				offset = offset - PCI_MHIREGLEN_REG;
				val = ioread32(ab->mem +
					       (offset & WINDOW_RANGE_MASK));
			} else {
				val = ioread32(ab->mem + window_start +
					       (offset & WINDOW_RANGE_MASK));
			}
			spin_unlock_bh(&ab_pci->window_lock);
		} else {
			val = ioread32(ab->mem + window_start +
				       (offset & WINDOW_RANGE_MASK));
		}
	}

	if (test_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags) &&
	    offset >= ACCESS_ALWAYS_OFF && ab_pci->pci_ops->release &&
	    !ret)
		ab_pci->pci_ops->release(ab);
	return val;
}
EXPORT_SYMBOL(ath12k_pci_read32);

void ath12k_pci_write32(struct ath12k_base *ab, u32 offset, u32 value)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	u32 window_start;
	int ret = 0;

	/* for offset beyond BAR + 4K - 32, may
	 * need to wakeup MHI to access.
	 */
	if (test_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags) &&
	    offset >= ACCESS_ALWAYS_OFF && ab_pci->pci_ops->wakeup)
		ret = ab_pci->pci_ops->wakeup(ab);

	if (offset < WINDOW_START) {
		iowrite32(value, ab->mem + offset);
	} else {
		if (ab->static_window_map)
			window_start = ath12k_pcic_get_window_start(ab, offset);
		else
			window_start = WINDOW_START;

		if (window_start == WINDOW_START) {
			spin_lock_bh(&ab_pci->window_lock);
			ath12k_pci_select_window(ab_pci, offset);

			if (ath12k_pci_is_offset_within_mhi_region(offset)) {
				offset = offset - PCI_MHIREGLEN_REG;
				iowrite32(value, ab->mem +
					  (offset & WINDOW_RANGE_MASK));
			} else {
				iowrite32(value, ab->mem + window_start +
					  (offset & WINDOW_RANGE_MASK));
			}
			spin_unlock_bh(&ab_pci->window_lock);
		} else {
			iowrite32(value, ab->mem + window_start +
				  (offset & WINDOW_RANGE_MASK));
		}
	}

	if (test_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags) &&
	    offset >= ACCESS_ALWAYS_OFF && ab_pci->pci_ops->release &&
	    !ret)
		ab_pci->pci_ops->release(ab);
}

#ifdef CPTCFG_ATH12K_COREDUMP
static int ath12k_pci_coredump_calculate_size(struct ath12k_base *ab, u32 *dump_seg_sz)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	struct mhi_controller *mhi_ctrl = ab_pci->mhi_ctrl;
	struct image_info *rddm_img, *fw_img;
	struct ath12k_tlv_dump_data *dump_tlv;
	enum ath12k_fw_crash_dump_type mem_type;
	u32 len = 0, rddm_tlv_sz = 0, paging_tlv_sz = 0;
	struct ath12k_dump_file_data *file_data;
	int i;

	rddm_img = mhi_ctrl->rddm_image;
	if (!rddm_img) {
		ath12k_err(ab, "No RDDM dump found\n");
		return 0;
	}

	fw_img = mhi_ctrl->fbc_image;

	for (i = 0; i < fw_img->entries ; i++) {
		if (!fw_img->mhi_buf[i].buf)
			continue;

		paging_tlv_sz += fw_img->mhi_buf[i].len;
	}
	dump_seg_sz[FW_CRASH_DUMP_PAGING_DATA] = paging_tlv_sz;

	for (i = 0; i < rddm_img->entries; i++) {
		if (!rddm_img->mhi_buf[i].buf)
			continue;

		rddm_tlv_sz += rddm_img->mhi_buf[i].len;
	}
	dump_seg_sz[FW_CRASH_DUMP_RDDM_DATA] = rddm_tlv_sz;

	for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		mem_type = ath12k_coredump_get_dump_type(ab->qmi.target_mem[i].type);

		if (mem_type == FW_CRASH_DUMP_NONE)
			continue;

		if (mem_type == FW_CRASH_DUMP_TYPE_MAX) {
			ath12k_dbg(ab, ATH12K_DBG_PCI,
				   "target mem region type %d not supported",
				   ab->qmi.target_mem[i].type);
			continue;
		}

		if (!ab->qmi.target_mem[i].paddr)
			continue;

		dump_seg_sz[mem_type] += ab->qmi.target_mem[i].size;
	}

	for (i = 0; i < FW_CRASH_DUMP_TYPE_MAX; i++) {
		if (!dump_seg_sz[i])
			continue;

		len += sizeof(*dump_tlv) + dump_seg_sz[i];
	}

	if (len)
		len += sizeof(*file_data);

	return len;
}

void ath12k_coredump_q6crash_reason(struct ath12k_base *ab)
{
        int i = 0;
        uint64_t coredump_offset = 0;
        struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
        struct mhi_controller *mhi_ctrl = ar_pci->mhi_ctrl;
        struct mhi_buf *mhi_buf;
        struct image_info *rddm_image;
        struct ath12k_coredump_q6ramdump_header *ramdump_header;
        struct ath12k_coredump_q6ramdump_entry *ramdump_table;
        char *msg = NULL;
        struct pci_dev *pci_dev = ar_pci->pdev;

        rddm_image = mhi_ctrl->rddm_image;
        mhi_buf = rddm_image->mhi_buf;

        ath12k_info(ab, "CRASHED - [DID:DOMAIN:BUS:SLOT] - %x:%04u:%02u:%02u\n",
                    pci_dev->device, pci_dev->bus->domain_nr,
                    pci_dev->bus->number, PCI_SLOT(pci_dev->devfn));

        /* Get RDDM header size */
        ramdump_header = (struct ath12k_coredump_q6ramdump_header *)mhi_buf[0].buf;
        ramdump_table = ramdump_header->ramdump_table;
        coredump_offset = le32_to_cpu(ramdump_header->header_size);

        /* Traverse ramdump table to get coredump offset */
        while (i < MAX_RAMDUMP_TABLE_SIZE) {
                if (!strncmp(ramdump_table->description, COREDUMP_DESC,
                             sizeof(COREDUMP_DESC)) ||
                    !strncmp(ramdump_table->description, Q6_SFR_DESC,
                             sizeof(Q6_SFR_DESC))) {
                        break;
                }
                coredump_offset += le64_to_cpu(ramdump_table->size);
                ramdump_table++;
                i++;
        }

        if (i == MAX_RAMDUMP_TABLE_SIZE) {
                ath12k_warn(ab, "Cannot find '%s' entry in ramdump\n",
                            COREDUMP_DESC);
                return;
        }

        /* Locate coredump data from the ramdump segments */
        for (i = 0; i < rddm_image->entries; i++) {
                if (coredump_offset < mhi_buf[i].len) {
                        msg = mhi_buf[i].buf + coredump_offset;
                        break;
                }

                coredump_offset -= mhi_buf[i].len;
        }

        if (msg && msg[0])
                ath12k_err(ab, "Fatal error received from wcss!\n%s\n",
                            msg);
}

static void ath12k_pci_coredump_download(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	struct mhi_controller *mhi_ctrl = ab_pci->mhi_ctrl;
	struct image_info *rddm_img, *fw_img;
	struct timespec64 timestamp;
	int i, len, mem_idx;
	enum ath12k_fw_crash_dump_type mem_type;
	struct ath12k_dump_file_data *file_data;
	struct ath12k_tlv_dump_data *dump_tlv;
	size_t hdr_len = sizeof(*file_data);
	void *buf;
	u32 dump_seg_sz[FW_CRASH_DUMP_TYPE_MAX] = { 0 };

	ath12k_mhi_coredump(mhi_ctrl, false);
	ath12k_coredump_q6crash_reason(ab);

	len = ath12k_pci_coredump_calculate_size(ab, dump_seg_sz);
	if (!len) {
		ath12k_warn(ab, "No crash dump data found for devcoredump");
		return;
	}

	rddm_img = mhi_ctrl->rddm_image;
	fw_img = mhi_ctrl->fbc_image;

	/* dev_coredumpv() requires vmalloc data */
	buf = vzalloc(len);
	if (!buf)
		return;

	ab->dump_data = buf;
	ab->ath12k_coredump_len = len;
	file_data = ab->dump_data;
	strscpy(file_data->df_magic, "ATH12K-FW-DUMP", sizeof(file_data->df_magic));
	file_data->len = cpu_to_le32(len);
	file_data->version = cpu_to_le32(ATH12K_FW_CRASH_DUMP_V2);
	file_data->chip_id = cpu_to_le32(ab_pci->dev_id);
	file_data->qrtr_id = cpu_to_le32(ab_pci->ab->qmi.service_ins_id);
	file_data->bus_id = cpu_to_le32(pci_domain_nr(ab_pci->pdev->bus));
	guid_gen(&file_data->guid);
	ktime_get_real_ts64(&timestamp);
	file_data->tv_sec = cpu_to_le64(timestamp.tv_sec);
	file_data->tv_nsec = cpu_to_le64(timestamp.tv_nsec);
	buf += hdr_len;
	dump_tlv = buf;
	dump_tlv->type = cpu_to_le32(FW_CRASH_DUMP_PAGING_DATA);
	dump_tlv->tlv_len = cpu_to_le32(dump_seg_sz[FW_CRASH_DUMP_PAGING_DATA]);
	buf += COREDUMP_TLV_HDR_SIZE;

	/* append all segments together as they are all part of a single contiguous
	 * block of memory
	 */
	for (i = 0; i < fw_img->entries ; i++) {
		if (!fw_img->mhi_buf[i].buf)
			continue;

		memcpy_fromio(buf, (void const __iomem *)fw_img->mhi_buf[i].buf,
			      fw_img->mhi_buf[i].len);
		buf += fw_img->mhi_buf[i].len;
	}

	dump_tlv = buf;
	dump_tlv->type = cpu_to_le32(FW_CRASH_DUMP_RDDM_DATA);
	dump_tlv->tlv_len = cpu_to_le32(dump_seg_sz[FW_CRASH_DUMP_RDDM_DATA]);
	buf += COREDUMP_TLV_HDR_SIZE;

	/* append all segments together as they are all part of a single contiguous
	 * block of memory
	 */
	for (i = 0; i < rddm_img->entries; i++) {
		if (!rddm_img->mhi_buf[i].buf)
			continue;

		memcpy_fromio(buf, (void const __iomem *)rddm_img->mhi_buf[i].buf,
			      rddm_img->mhi_buf[i].len);
		buf += rddm_img->mhi_buf[i].len;
	}

	mem_idx = FW_CRASH_DUMP_REMOTE_MEM_DATA;
	for (; mem_idx < FW_CRASH_DUMP_TYPE_MAX; mem_idx++) {
		if (!dump_seg_sz[mem_idx] || mem_idx == FW_CRASH_DUMP_NONE)
			continue;

		dump_tlv = buf;
		dump_tlv->type = cpu_to_le32(mem_idx);
		dump_tlv->tlv_len = cpu_to_le32(dump_seg_sz[mem_idx]);
		buf += COREDUMP_TLV_HDR_SIZE;

		for (i = 0; i < ab->qmi.mem_seg_count; i++) {
			mem_type = ath12k_coredump_get_dump_type
							(ab->qmi.target_mem[i].type);

			if (mem_type != mem_idx)
				continue;

			if (!ab->qmi.target_mem[i].paddr) {
				ath12k_dbg(ab, ATH12K_DBG_PCI,
					   "Skipping mem region type %d",
					   ab->qmi.target_mem[i].type);
				continue;
			}

			memcpy_fromio(buf, ab->qmi.target_mem[i].v.ioaddr,
				      ab->qmi.target_mem[i].size);
			buf += ab->qmi.target_mem[i].size;
		}
	}

	queue_work(ab->workqueue, &ab->dump_work);
}
#endif

int ath12k_pci_power_up(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);
	int ret;

	ab_pci->register_window = 0;
	clear_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags);
	ath12k_pci_sw_reset(ab_pci->ab, true);

	/* Disable ASPM during firmware download due to problems switching
	 * to AMSS state.
	 */
	ath12k_pci_aspm_disable(ab_pci);

	ath12k_pci_msi_enable(ab_pci);

	if (ab->fw.api_version == ATH12K_FW_API_V1 || test_bit(ATH12K_FW_FEATURE_MULTI_QRTR_ID, ab->fw.fw_features))
		ath12k_pci_update_qrtr_node_id(ab);

	ret = ath12k_mhi_start(ab_pci);
	if (ret) {
		ath12k_err(ab, "failed to start mhi: %d\n", ret);
		return ret;
	}

	if (ab->static_window_map)
		ath12k_pci_select_static_window(ab);

	clear_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags);
	return 0;
}

void ath12k_pci_power_down(struct ath12k_base *ab, bool is_suspend)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);

#ifdef CONFIG_IO_COHERENCY
       int ret;
#endif

	ath12k_mhi_set_state(ab_pci, ATH12K_MHI_SOC_RESET);
	if (!wait_for_completion_timeout(&ab->rddm_reset_done,
					 msecs_to_jiffies(3000))) {
		ath12k_warn(ab, "failed to set RDDM mode\n");
		if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags)) {
			ath12k_warn(ab, "failed to clear MHI SOC RESET as mhi already in rddm state due to recovery in progress, clearing it here\n");
			clear_bit(ATH12K_MHI_SOC_RESET, &ab_pci->mhi_state);
			reinit_completion(&ab->rddm_reset_done);
		}
	}

#ifdef CONFIG_IO_COHERENCY
       ret = ath12k_core_config_iocoherency(ab, false);
       if (ret)
               ath12k_err(ab, "failed to configure IOCoherency: %d\n", ret);
#endif

	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	/* restore aspm in case firmware bootup fails */
	ath12k_pci_aspm_restore(ab_pci);

	ath12k_pci_force_wake(ab_pci->ab);
	ath12k_pci_msi_disable(ab_pci);
	ath12k_mhi_stop(ab_pci, is_suspend);
	clear_bit(ATH12K_PCI_FLAG_INIT_DONE, &ab_pci->flags);
	ath12k_pci_sw_reset(ab_pci->ab, false);
	set_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags);
}

static int ath12k_pci_panic_handler(struct ath12k_base *ab)
{
	ath12k_pci_sw_reset(ab, false);

	return NOTIFY_OK;
}

static int ath12k_dp_umac_pci_config_irq(struct ath12k_base *ab)
{
        u32 msi_data_start, msi_data_count, msi_irq_start;
        unsigned int msi_data;
        int irq, ret;
        struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

        ret = ath12k_pcic_get_user_msi_assignment(ab, "DP", &msi_data_count,
                                                  &msi_data_start, &msi_irq_start);
        if (ret)
                return ret;

        msi_data = (umac_reset->intr_offset % msi_data_count) + msi_irq_start;
        irq = ath12k_hif_get_msi_irq(ab, msi_data);

        umac_reset->irq_num = irq;
        tasklet_setup(&umac_reset->intr_tq, ath12k_umac_reset_tasklet_handler);

        ret = request_irq(irq, ath12k_umac_reset_interrupt_handler,
                          IRQF_NO_SUSPEND, "umac_dp_reset", ab);
        if (ret) {
                ath12k_err(ab, "failed to request irq for umac dp reset %d\n", ret);
                return ret;
        }

        disable_irq_nosync(umac_reset->irq_num);

        return 0;
}

static void ath12k_pci_dp_umac_reset_enable_irq(struct ath12k_base *ab)
{
        struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

        enable_irq(umac_reset->irq_num);
}

static void ath12k_pci_dp_umac_reset_free_irq(struct ath12k_base *ab)
{
        struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

        disable_irq_nosync(umac_reset->irq_num);
        free_irq(umac_reset->irq_num, ab);
}

static const struct ath12k_hif_ops ath12k_pci_hif_ops = {
	.start = ath12k_pci_start,
	.stop = ath12k_pcic_stop,
	.read32 = ath12k_pci_read32,
	.write32 = ath12k_pci_write32,
	.power_down = ath12k_pci_power_down,
	.power_up = ath12k_pci_power_up,
	.suspend = ath12k_pci_hif_suspend,
	.resume = ath12k_pci_hif_resume,
	.irq_enable = ath12k_pcic_ext_irq_enable,
	.irq_disable = ath12k_pcic_ext_irq_disable,
	.get_msi_address = ath12k_pci_get_msi_address,
	.get_user_msi_vector = ath12k_pcic_get_user_msi_assignment,
	.map_service_to_pipe = ath12k_pcic_map_service_to_pipe,
	.get_msi_irq = ath12k_pci_get_msi_irq,
	.ce_irq_enable = ath12k_pcic_ce_irqs_enable,
	.ce_irq_disable = ath12k_pcic_ce_irq_disable_sync,
	.get_ce_msi_idx = ath12k_pci_get_ce_msi_idx,
	.panic_handler = ath12k_pci_panic_handler,
#ifdef CPTCFG_ATH12K_COREDUMP
	.coredump_download = ath12k_pci_coredump_download,
#endif
	.ext_irq_setup = ath12k_pcic_ext_irq_config,
	.ext_irq_cleanup = ath12k_pcic_free_ext_irq,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.ppeds_register_interrupts = ath12k_pci_ppeds_register_interrupts,
	.ppeds_free_interrupts = ath12k_pci_ppeds_free_interrupts,
	.ppeds_irq_enable = ath12k_pci_ppeds_irq_enable,
	.ppeds_irq_disable = ath12k_pci_ppeds_irq_disable,
#endif
	.dp_umac_reset_irq_config = ath12k_dp_umac_pci_config_irq,
	.dp_umac_reset_enable_irq = ath12k_pci_dp_umac_reset_enable_irq,
	.dp_umac_reset_free_irq = ath12k_pci_dp_umac_reset_free_irq,
};

static enum ath12k_device_family
ath12k_get_device_family(const struct pci_device_id *pci_dev)
{
	enum ath12k_device_family device_id;
	const struct pci_device_id *id;

	for (device_id = ATH12K_DEVICE_FAMILY_WIFI7;
	     device_id < ATH12K_DEVICE_FAMILY_MAX; device_id++) {
		if (!ath12k_pci_family_drivers[device_id])
			continue;

		id = ath12k_pci_family_drivers[device_id]->id_table;
		while (id) {
			if (id->device == pci_dev->device)
				return device_id;
			id += 1;
		}
	}

	return ATH12K_DEVICE_FAMILY_MAX;
}

int ath12k_pci_get_link_status(struct pci_dev *pdev, u16 *speed, u16 *width)
{
	u16 link_status;
	int ret;

	if (!pdev || !speed || !width)
		return -EINVAL;

	ret = pcie_capability_read_word(pdev, PCI_EXP_LNKSTA, &link_status);
	if (ret)
		return ret;

	*speed = u16_get_bits(link_status, PCI_EXP_LNKSTA_CLS);
	*width = u16_get_bits(link_status, PCI_EXP_LNKSTA_NLW);
	return 0;
}

static int ath12k_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *pci_dev)
{
	enum ath12k_device_family device_id;
	struct ath12k_pci *ab_pci;
	struct ath12k_base *ab;
	int ret;

	ab = ath12k_core_alloc(&pdev->dev, sizeof(*ab_pci), ATH12K_BUS_PCI);
	if (!ab) {
		dev_err(&pdev->dev, "failed to allocate ath12k base\n");
		return -ENOMEM;
	}

	ab->dev = &pdev->dev;
	pci_set_drvdata(pdev, ab);
	ab_pci = ath12k_pci_priv(ab);
	ab_pci->dev_id = pci_dev->device;
	ab_pci->ab = ab;
	ab_pci->pdev = pdev;
	ab->hif.ops = &ath12k_pci_hif_ops;
	ab->hif.bus = ATH12K_BUS_PCI;
	ab->fw_mode = ATH12K_FIRMWARE_MODE_NORMAL;
	ab->msi.config = &ath12k_wifi7_msi_config[ATH12K_MSI_CONFIG_PCI];
	pci_set_drvdata(pdev, ab);
	spin_lock_init(&ab_pci->window_lock);

	ret = ath12k_pci_claim(ab_pci, pdev);
	if (ret) {
		ath12k_err(ab, "failed to claim device: %d\n", ret);
		goto err_free_core;
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "pci probe %04x:%04x %04x:%04x\n",
		   pdev->vendor, pdev->device,
		   pdev->subsystem_vendor, pdev->subsystem_device);

	ab->id.vendor = pdev->vendor;
	ab->id.device = pdev->device;
	ab->id.subsystem_vendor = pdev->subsystem_vendor;
	ab->id.subsystem_device = pdev->subsystem_device;

	device_id = ath12k_get_device_family(pci_dev);
	if (device_id >= ATH12K_DEVICE_FAMILY_MAX) {
		ath12k_err(ab, "failed to get device family id\n");
		goto err_pci_free_region;
	}

	ath12k_dbg(ab, ATH12K_DBG_PCI, "PCI device family id: %d\n", device_id);

	ab_pci->device_ops = &ath12k_pci_family_drivers[device_id]->ops;
	ab_pci->reg_base = ath12k_pci_family_drivers[device_id]->reg_base;

	/* Call device specific probe. This is the callback that can
	 * be used to override any ops in future
	 */
	if (ab_pci->device_ops->probe) {
		ret = ab_pci->device_ops->probe(pdev, pci_dev);
		if (ret) {
			ath12k_err(ab, "failed to probe device: %d\n", ret);
			goto err_pci_free_region;
		}
	}

	if (of_property_match_string(ab->dev->of_node, "qcom,early_cal_enabled", "okay") >= 0) {
		ab->early_cal_support = true;
		ath12k_info(ab,"coldboot calibration supported in uboot\n");
	}

	ret = ath12k_pci_msi_alloc(ab_pci);
	if (ret) {
		ath12k_err(ab, "failed to alloc msi: %d\n", ret);
		goto err_pci_free_region;
	}

	ath12k_fw_map(ab);

	ret = ath12k_pci_set_irq_affinity_hint(ab_pci, cpumask_of(0));
	if (ret) {
		ath12k_err(ab, "failed to set irq affinity %d\n", ret);
		goto err_pci_msi_free;
	}

	ret = ath12k_mhi_register(ab_pci);
	if (ret) {
		ath12k_err(ab, "failed to register mhi: %d\n", ret);
		goto err_irq_affinity_cleanup;
	}

	ret = ath12k_hal_srng_init(ab);
	if (ret)
		goto err_mhi_unregister;

	ret = ath12k_ce_alloc_pipes(ab);
	if (ret) {
		ath12k_err(ab, "failed to allocate ce pipes: %d\n", ret);
		goto err_hal_srng_deinit;
	}

	ath12k_pci_init_qmi_ce_config(ab);

	ret = ath12k_pcic_config_irq(ab);
	if (ret) {
		ath12k_err(ab, "failed to config irq: %d\n", ret);
		goto err_ce_free;
	}

	/* kernel may allocate a dummy vector before request_irq and
	 * then allocate a real vector when request_irq is called.
	 * So get msi_data here again to avoid spurious interrupt
	 * as msi_data will configured to srngs.
	 */
	ret = ath12k_pci_config_msi_data(ab_pci);
	if (ret) {
		ath12k_err(ab, "failed to config msi_data: %d\n", ret);
		goto err_free_irq;
	}

	/* as dp need hal srngs, dp_init op must be called after
	 * hal srng initialization
	 */
	if (ab_pci->device_ops->dp_init) {
		ab->dp = ab_pci->device_ops->dp_init(ab);
		if (!ab->dp) {
			ath12k_err(ab, "dp_init failed");
			goto err_pci_msi_free;
		}
	}

	ret = ath12k_core_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to init core: %d\n", ret);
		goto err_free_dp;
	}

	ret = ath12k_pci_get_link_status(ab_pci->pdev, &ab_pci->def_link_speed,
					 &ab_pci->def_link_width);
	if (ret)
		ath12k_err(ab, "failed to get PCIe link status %d\n", ret);

	return 0;

err_free_dp:
	if (test_bit(ATH12K_FLAG_SOC_CREATE_FAIL, &ab->dev_flags))
		return ret;

	if (ab_pci->device_ops->dp_deinit)
		ab_pci->device_ops->dp_deinit(ab->dp);

err_free_irq:
	/* __free_irq() expects the caller to have cleared the affinity hint */
	ath12k_pci_set_irq_affinity_hint(ab_pci, NULL);
	ath12k_pcic_free_irq(ab);

err_ce_free:
	ath12k_ce_free_pipes(ab);

err_hal_srng_deinit:
	ath12k_hal_srng_deinit(ab);

err_mhi_unregister:
	ath12k_mhi_unregister(ab_pci);

err_irq_affinity_cleanup:
	ath12k_pci_set_irq_affinity_hint(ab_pci, NULL);

err_pci_msi_free:
	ath12k_pci_msi_free(ab_pci);

err_pci_free_region:
	ath12k_pci_free_region(ab_pci);

err_free_core:
	ath12k_core_free(ab);

	return ret;
}

static void ath12k_pci_remove(struct pci_dev *pdev)
{
	struct ath12k_base *ab = pci_get_drvdata(pdev);
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);

	ath12k_pci_set_irq_affinity_hint(ab_pci, NULL);

	if (test_bit(ATH12K_FLAG_QMI_FAIL, &ab->dev_flags)) {
		ath12k_pci_power_down(ab, false);
		ath12k_qmi_deinit_service(ab);
		ath12k_core_hw_group_unassign(ab);
		ath12k_core_panic_notifier_unregister(ab);
		ath12k_debugfs_soc_destroy(ab);
		goto qmi_fail;
	}

	set_bit(ATH12K_FLAG_UNREGISTERING, &ab->dev_flags);

	cancel_work_sync(&ab->reset_work);
	cancel_work_sync(&ab->dump_work);
	ath12k_core_deinit(ab);

qmi_fail:
	ath12k_fw_unmap(ab);
	ath12k_mhi_unregister(ab_pci);
	ath12k_pcic_free_irq(ab);
	ath12k_pci_free_region(ab_pci);

	ath12k_hal_srng_deinit(ab);
	ath12k_ce_free_pipes(ab);

	if (ab_pci->device_ops->dp_deinit)
		ab_pci->device_ops->dp_deinit(ab->dp);

	ath12k_pci_msi_free(ab_pci);
	ath12k_core_free(ab);
}

static void ath12k_pci_shutdown(struct pci_dev *pdev)
{
	struct ath12k_base *ab = pci_get_drvdata(pdev);
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);

	ath12k_pci_set_irq_affinity_hint(ab_pci, NULL);

	if (ath12k_check_erp_power_down(ab->ag) &&
	    ab->pm_suspend)
		return;

	ath12k_qmi_firmware_stop(ab);
	ath12k_pci_power_down(ab, false);
}

static __maybe_unused int ath12k_pci_pm_suspend(struct device *dev)
{
	struct ath12k_base *ab = dev_get_drvdata(dev);
	int ret;

	ret = ath12k_core_suspend(ab);
	if (ret)
		ath12k_warn(ab, "failed to suspend core: %d\n", ret);

	return ret;
}

static __maybe_unused int ath12k_pci_pm_resume(struct device *dev)
{
	struct ath12k_base *ab = dev_get_drvdata(dev);
	int ret;

	ret = ath12k_core_resume(ab);
	if (ret)
		ath12k_warn(ab, "failed to resume core: %d\n", ret);

	return ret;
}

static __maybe_unused int ath12k_pci_pm_suspend_late(struct device *dev)
{
	struct ath12k_base *ab = dev_get_drvdata(dev);
	int ret;

	ret = ath12k_core_suspend_late(ab);
	if (ret)
		ath12k_warn(ab, "failed to late suspend core: %d\n", ret);

	return ret;
}

static __maybe_unused int ath12k_pci_pm_resume_early(struct device *dev)
{
	struct ath12k_base *ab = dev_get_drvdata(dev);
	int ret;

	ret = ath12k_core_resume_early(ab);
	if (ret)
		ath12k_warn(ab, "failed to early resume core: %d\n", ret);

	return ret;
}

static const struct dev_pm_ops __maybe_unused ath12k_pci_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ath12k_pci_pm_suspend,
				ath12k_pci_pm_resume)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(ath12k_pci_pm_suspend_late,
				     ath12k_pci_pm_resume_early)
};

int ath12k_pci_register_driver(const enum ath12k_device_family device_id,
			       struct ath12k_pci_driver *driver)
{
	struct pci_driver *pci_driver;

	if (device_id >= ATH12K_DEVICE_FAMILY_MAX)
		return -EINVAL;

	if (ath12k_pci_family_drivers[device_id]) {
		pr_err("Driver already for id: %d\n", device_id);
		return -EALREADY;
	}

	ath12k_pci_family_drivers[device_id] = driver;

	pci_driver = &ath12k_pci_drivers[device_id];
	pci_driver->name = driver->name;
	pci_driver->id_table = driver->id_table;
	pci_driver->probe = ath12k_pci_probe;
	pci_driver->remove = ath12k_pci_remove;
	pci_driver->shutdown = ath12k_pci_shutdown;
	pci_driver->driver.pm = &ath12k_pci_pm_ops;

	return pci_register_driver(pci_driver);
}
EXPORT_SYMBOL(ath12k_pci_register_driver);

void ath12k_pci_unregister_driver(const enum ath12k_device_family device_id)
{
	struct pci_driver *pci_driver;

	if (device_id >= ATH12K_DEVICE_FAMILY_MAX)
		return;

	if (!ath12k_pci_family_drivers[device_id])
		return;

	pci_driver = &ath12k_pci_drivers[device_id];
	pci_unregister_driver(pci_driver);
	ath12k_pci_family_drivers[device_id] = NULL;
}
EXPORT_SYMBOL(ath12k_pci_unregister_driver);

struct pci_dev *ath12k_pci_get_dev_by_ab(struct ath12k_base *ab)
{
	struct ath12k_pci *ab_pci = ath12k_pci_priv(ab);

	if (!ab_pci)
		return NULL;

	return ab_pci->pdev;
}
