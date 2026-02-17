/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_PCI_WIFI7_H
#define ATH12K_PCI_WIFI7_H

int ath12k_wifi7_pci_init(void);
void ath12k_wifi7_pci_exit(void);

/* QCN9224 WiFi7-specific register definitions */

/* SRAM layout for Q6 bootloader logs */
#define QCN9224_SRAM_START          0x01300000
#define QCN9224_SRAM_SIZE           0x005A8000
#define QCN9224_SRAM_END \
				(QCN9224_SRAM_START + QCN9224_SRAM_SIZE - 1)

/* BHI error/debug registers capturing Q6 SBL log start/size */
#define QCN9224_PCIE_BHI_ERRDBG2_REG        0x1E0E238
#define QCN9224_PCIE_BHI_ERRDBG3_REG        0x1E0E23C

/* Q6 Primary Boot Loader (PBL) logging region and related regs */
#define QCN9224_PBL_LOG_SRAM_START      0x01303da0
#define QCN9224_v2_PBL_LOG_SRAM_START   0x01303e98
#define QCN9224_PBL_LOG_SRAM_MAX_SIZE   40
#define QCN9224_TCSR_PBL_LOGGING_REG    0x1B00094
#define QCN9224_PBL_WLAN_BOOT_CFG       0x1E22B34
#define QCN9224_PBL_BOOTSTRAP_STATUS    0x1A006D4

/* Q6 Misc PCIE/WLAON/GCC registers for debug */
#define QCN9224_PCIE_PCIE_LOCAL_REG_REMAP_BAR_CTRL  0x310C
#define QCN9224_WLAON_SOC_RESET_CAUSE_SHADOW_REG    0x1F80718
#define QCN9224_PCIE_PCIE_PARF_LTSSM                0x1E081B0
#define QCN9224_GCC_RAMSS_CBCR                      0x1E38200

/* Q6 NOC error logger registers */
#define QCN9224_SNOC_ERL_ErrVld_Low         0x1E80010
#define QCN9224_SNOC_ERL_ErrLog0_Low        0x1E80020
#define QCN9224_SNOC_ERL_ErrLog0_High       0x1E80024
#define QCN9224_SNOC_ERL_ErrLog1_Low        0x1E80028
#define QCN9224_SNOC_ERL_ErrLog1_High       0x1E8002C
#define QCN9224_SNOC_ERL_ErrLog2_Low        0x1E80030
#define QCN9224_SNOC_ERL_ErrLog2_High       0x1E80034
#define QCN9224_SNOC_ERL_ErrLog3_Low        0x1E80038
#define QCN9224_SNOC_ERL_ErrLog3_High       0x1E8003C

#define QCN9224_PCNOC_ERL_ErrVld_Low        0x1F00010
#define QCN9224_PCNOC_ERL_ErrLog0_Low       0x1F00020
#define QCN9224_PCNOC_ERL_ErrLog0_High      0x1F00024
#define QCN9224_PCNOC_ERL_ErrLog1_Low       0x1F00028
#define QCN9224_PCNOC_ERL_ErrLog1_High      0x1F0002C
#define QCN9224_PCNOC_ERL_ErrLog2_Low       0x1F00030
#define QCN9224_PCNOC_ERL_ErrLog2_High      0x1F00034
#define QCN9224_PCNOC_ERL_ErrLog3_Low       0x1F00038
#define QCN9224_PCNOC_ERL_ErrLog3_High      0x1F0003C

#endif /* ATH12K_PCI_WIFI7_H */
