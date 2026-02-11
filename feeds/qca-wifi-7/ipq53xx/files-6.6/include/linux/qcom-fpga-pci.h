/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#ifndef _QCOM_FPGA_PCI_H
#define _QCOM_FPGA_PCI_H

#include <linux/regmap.h>

#define MODULENAME			"qcom-fpga-pci"
#define PCIE_MEM_ACCESS_BASE_ADDR_REG	0x4

#define QTI_FPGA_PON_VENDOR_ID		0x10EE
#define QTI_FPGA_PON_DEVICE_ID_1	0x9012
#define QTI_FPGA_PON_DEVICE_ID_2	0x9022
#define QTI_FPGA_PON_DEVICE_ID_3	0x9011
#define QTI_FPGA_PON_DEVICE_ID_4	0x9021

#define QCOM_GET_NEW_ADDR(reg)		(((reg) & 0xfffff) | 0x100000)
#define QCOM_GET_BASE_ADDR(reg)		(((((reg) & 0xfff00000) | (((reg) & 0xf00000000) >> 0x10))) | 1)

void qcom_fpga_mem_write(u64 reg, u64 val);
u64 qcom_fpga_mem_read(u64 reg);
u64 qcom_fpga_bulk_reg_write(u64 reg, const u64 *val,
			     size_t val_count);
u64 qcom_fpga_bulk_reg_read(u64 reg, u64 *val, size_t val_count);

u64 qcom_fpga_multi_reg_write(const struct reg_sequence *regs,
			      u64 num_regs);
u64 qcom_fpga_multi_reg_read(struct reg_sequence *regs, u64 num_regs);
#endif
