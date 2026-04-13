/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _ATH12K_MHI_H
#define _ATH12K_MHI_H

#include "pci.h"

#define PCIE_TXVECDB				0x360
#define PCIE_TXVECSTATUS			0x368
#define PCIE_RXVECDB				0x394
#define PCIE_RXVECSTATUS			0x39C

#define MHISTATUS				0x48
#define MHICTRL					0x38
#define MHICTRL_RESET_MASK			0x2
#define ATH12K_PCI_FW_RDDM_SZ			0x600000

#define MHI_Q6_MAX_PBL_DATA_SNAPSHOT 2
#define MAX_NOC_ERR_REGS 18
#define MHI_DEVICE_RDDM_COOKIE   0xCAFECACE

struct mhi_q6_pbl_err_data {
	u32 *pbl_vals;
	u32 *pbl_reg_tbl;
	u32 pbl_tbl_len;
};

struct mhi_q6_dump_pbl_sbl_data {
	struct mhi_q6_pbl_err_data pbl_data[MHI_Q6_MAX_PBL_DATA_SNAPSHOT];
	u32 *sbl_vals;
	u32 sbl_len;
	u32 *noc_vals;
	const struct mhi_q6_noc_err_reg *noc_tbl;
	u32 pcie_cfg_pcie_status;
	u32 parf_pm_stts;
	u16 type0_status_cmd_reg;
	u16 pci_msi_cap_id_next_ctrl_reg;
	u16 pci_msi_cap_off_04h_reg;
	u16 pci_msi_cap_off_08h_reg;
	u16 pci_msi_cap_off_0ch_reg;
	u32 remap_bar_ctrl;
	u32 soc_rc_shadow_reg;
	u32 parf_ltssm;
	u32 gcc_ramss_cbcr;
	u32 sbl_log_start;
	u32 pbl_stage;
	u32 pbl_wlan_boot_cfg;
	u32 pbl_bootstrap_status;
	u32 noc_len;
};

struct mhi_q6_sbl_reg_addr {
	u32 sbl_sram_start;
	u32 sbl_sram_end;
	u32 sbl_log_size_reg;
	u32 sbl_log_start_reg;
	u32 sbl_log_size_shift;
};

struct mhi_q6_pbl_reg_addr {
	u32 pbl_log_sram_start;
	u32 pbl_log_sram_max_size;
	u32 pbl_log_sram_start_v1;
	u32 pbl_log_sram_max_size_v1;
	u32 tcsr_pbl_logging_reg;
	u32 pbl_wlan_boot_cfg;
	u32 pbl_bootstrap_status;
};

struct mhi_q6_noc_err_reg {
	const char *reg_name;
	u32 reg_offset;
};

enum ath12k_mhi_state {
	ATH12K_MHI_INIT,
	ATH12K_MHI_DEINIT,
	ATH12K_MHI_POWER_ON,
	ATH12K_MHI_POWER_OFF,
	ATH12K_MHI_POWER_OFF_KEEP_DEV,
	ATH12K_MHI_FORCE_POWER_OFF,
	ATH12K_MHI_SUSPEND,
	ATH12K_MHI_RESUME,
	ATH12K_MHI_TRIGGER_RDDM,
	ATH12K_MHI_RDDM,
	ATH12K_MHI_RDDM_DONE,
	ATH12K_MHI_SOC_RESET,
	ATH12K_MHI_MISSION_MODE,
};

int ath12k_mhi_start(struct ath12k_pci *ar_pci);
void ath12k_mhi_stop(struct ath12k_pci *ar_pci, bool is_suspend);
int ath12k_mhi_register(struct ath12k_pci *ar_pci);
void ath12k_mhi_unregister(struct ath12k_pci *ar_pci);
void ath12k_mhi_set_mhictrl_reset(struct ath12k_base *ab);
void ath12k_mhi_clear_vector(struct ath12k_base *ab);
void ath12k_mhi_suspend(struct ath12k_pci *ar_pci);
void ath12k_mhi_resume(struct ath12k_pci *ar_pci);
void ath12k_mhi_coredump(struct mhi_controller *mhi_ctrl, bool in_panic);
int ath12k_mhi_set_state(struct ath12k_pci *ab_pci,
			 enum ath12k_mhi_state mhi_state);
void *ath12k_pci_get_priv(struct ath12k_base *ab);
void ath12k_mhi_q6_boot_debug_timeout_hdlr(struct timer_list *t);
#endif
