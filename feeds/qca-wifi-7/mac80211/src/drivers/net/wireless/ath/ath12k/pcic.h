// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ATH11K_PCI_CMN_H
#define _ATH11K_PCI_CMN_H

#include <linux/platform_device.h>
#include <linux/pci.h>
#include "core.h"
#include "hif.h"
#include <linux/msi.h>
#include "pci.h"

#define ATH12K_PCI_IRQ_CE0_OFFSET		3

#define WINDOW_ENABLE_BIT		0x40000000
#define WINDOW_REG_ADDRESS		0x310c
#define WINDOW_VALUE_MASK		GENMASK(24, 19)
#define WINDOW_START			0x80000
#define WINDOW_RANGE_MASK		GENMASK(18, 0)

#define ATH12K_MAX_PCI_DOMAINS          0x5
#define DP_IRQ_NAME_LEN 20

static const struct ath12k_msi_config ath12k_wifi7_msi_config[] = {
	{
		/* MSI spec expects number of interrupts to be a power of 2 */
		.total_vectors = 32,
		.total_users = 3,
		.users = (struct ath12k_msi_user[]) {
			{ .name = "MHI", .num_vectors = 3, .base_vector = 0 },
			{ .name = "CE", .num_vectors = 5, .base_vector = 3 },
			{ .name = "DP", .num_vectors = 16, .base_vector = 8 },
		},
	},
	{
		/* In DP, we use num_vectors as 9 (6 REGULAR DP INTERRUPTS + 3 PPEDS
		 * INTERRUPTS)
		 */
		.total_vectors = 32,
		.total_users = 3,
		.users = (struct ath12k_msi_user[]) {
			{ .name = "QDSS", .num_vectors = 1, .base_vector = 0 },
			{ .name = "CE", .num_vectors = 5, .base_vector = 1 },
			{ .name = "DP", .num_vectors = 15, .base_vector = 6 },
		},
	},
	{
		/* MSI spec expects number of interrupts to be a power of 2 */
		.total_vectors = 16,
		.total_users = 3,
		.users = (struct ath12k_msi_user[]) {
			{ .name = "MHI", .num_vectors = 3, .base_vector = 0 },
			{ .name = "CE", .num_vectors = 5, .base_vector = 3 },
			{ .name = "DP", .num_vectors = 8, .base_vector = 8 },
		},
	},
};

int ath12k_pcic_start(struct ath12k_base *ab);
void ath12k_pcic_stop(struct ath12k_base *ab);
void ath12k_pcic_ipci_write32(struct ath12k_base *ab, u32 offset, u32 value);
u32 ath12k_pcic_ipci_read32(struct ath12k_base *ab, u32 offset);
int ath12k_pcic_get_user_msi_assignment(struct ath12k_base *ab, char *user_name,
					int *num_vectors, u32 *user_base_data,
					u32 *base_vector);
void ath12k_pcic_get_msi_address(struct ath12k_base *ab, u32 *msi_addr_lo,
				 u32 *msi_addr_hi);
void ath12k_pcic_config_static_window(struct ath12k_base *ab);
int ath12k_pcic_map_service_to_pipe(struct ath12k_base *ab, u16 service_id,
				    u8 *ul_pipe, u8 *dl_pipe);
void ath12k_pcic_free_hybrid_irq(struct ath12k_base *ab);
void ath12k_pcic_cmem_write32(struct ath12k_base *ab, u32 addr,
			      u32 value);
u32 ath12k_pcic_cmem_read32(struct ath12k_base *ab, u32 addr);
void ath12k_pcic_ext_irq_enable(struct ath12k_base *ab);
void ath12k_pcic_ext_irq_disable(struct ath12k_base *ab);
u32 ath12k_pcic_get_window_start(struct ath12k_base *ab, u32 offset);
void ath12k_pcic_ce_irqs_enable(struct ath12k_base *ab);
void ath12k_pcic_ce_irq_disable_sync(struct ath12k_base *ab);
int ath12k_pcic_get_msi_irq(struct ath12k_base *ab, unsigned int vector);
int ath12k_pcic_config_hybrid_irq(struct ath12k_base *ab);
int ath12k_pcic_config_irq(struct ath12k_base *ab);
void ath12k_pcic_free_irq(struct ath12k_base *ab);
int ath12k_pcic_ext_irq_config(struct ath12k_base *ab,
			       int (*irq_handler)(struct ath12k_dp *dp,
						  struct ath12k_ext_irq_grp *irq_grp,
						  int budget),
			       struct ath12k_dp *dp);
int ath12k_pcic_cfg_hybrid_ext_irq(struct ath12k_base *ab,
				   int (*irq_handler)(struct ath12k_dp *dp,
					   struct ath12k_ext_irq_grp *irq_grp,
					   int budget),
				   struct ath12k_dp *dp);
void ath12k_pcic_free_ext_irq(struct ath12k_base *ab);
int ath12k_pcic_get_msi_data(struct ath12k_base *ab, struct msi_desc *msi_desc,
			     int i, int *hal_ring_type, int *ring_num);
int ath12k_pcic_ppeds_register_interrupts(struct ath12k_base *ab, int type, int vector,
					  int ring_num);
void ath12k_pcic_ppeds_irq_disable(struct ath12k_base *ab, enum ppeds_irq_type type);
void ath12k_pcic_ppeds_irq_enable(struct ath12k_base *ab, enum ppeds_irq_type type);
void ath12k_pcic_ppeds_free_interrupts(struct ath12k_base *ab);
#endif
