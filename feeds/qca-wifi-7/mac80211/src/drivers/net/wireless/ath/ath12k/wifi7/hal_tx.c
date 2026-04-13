// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "hal_desc.h"
#include "../hal.h"
#include "hal_tx.h"
#include "../hif.h"
#include "hal_rx.h"

#define DSCP_TID_MAP_TBL_ENTRY_SIZE 64
#define HAL_TX_BITS_PER_TID 3
#define HAL_TX_NUM_DSCP_REG_SIZE 32

void ath12k_wifi7_hal_tx_cmd_desc_setup(struct ath12k_base *ab,
					struct hal_tcl_data_cmd *tcl_cmd,
					struct hal_tx_info *ti)
{
	tcl_cmd->buf_addr_info.info0 =
		le32_encode_bits(ti->paddr, BUFFER_ADDR_INFO0_ADDR);
	tcl_cmd->buf_addr_info.info1 =
		le32_encode_bits(((uint64_t)ti->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				 BUFFER_ADDR_INFO1_ADDR);
	tcl_cmd->buf_addr_info.info1 |=
		le32_encode_bits((ti->rbm_id), BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(ti->desc_id, BUFFER_ADDR_INFO1_SW_COOKIE);

	tcl_cmd->info0 =
		le32_encode_bits(ti->type, HAL_TCL_DATA_CMD_INFO0_DESC_TYPE) |
		le32_encode_bits(ti->bank_id, HAL_TCL_DATA_CMD_INFO0_BANK_ID);

	tcl_cmd->info1 =
		le32_encode_bits(ti->meta_data_flags,
				 HAL_TCL_DATA_CMD_INFO1_CMD_NUM);

	tcl_cmd->info2 = cpu_to_le32(ti->flags0) |
		le32_encode_bits(ti->data_len, HAL_TCL_DATA_CMD_INFO2_DATA_LEN) |
		le32_encode_bits(ti->pkt_offset, HAL_TCL_DATA_CMD_INFO2_PKT_OFFSET);

	tcl_cmd->info3 = cpu_to_le32(ti->flags1) |
		le32_encode_bits(ti->tid, HAL_TCL_DATA_CMD_INFO3_TID) |
		le32_encode_bits(ti->lmac_id, HAL_TCL_DATA_CMD_INFO3_PMAC_ID) |
		le32_encode_bits(ti->vdev_id, HAL_TCL_DATA_CMD_INFO3_VDEV_ID);

	tcl_cmd->info4 = le32_encode_bits(ti->lookup_override,
					  HAL_TCL_DATA_CMD_INFO4_IDX_LOOKUP_OVERRIDE) |
			 le32_encode_bits(ti->bss_ast_idx,
					  HAL_TCL_DATA_CMD_INFO4_SEARCH_INDEX) |
			 le32_encode_bits(ti->bss_ast_hash,
					  HAL_TCL_DATA_CMD_INFO4_CACHE_SET_NUM);
	tcl_cmd->info5 = 0;
}

void ath12k_update_dscp_register(struct ath12k_base *ab, u32 addr, u32 mask, u32 value)
{
	u32 reg_val;

	reg_val = ath12k_hif_read32(ab, addr);
	reg_val &= ~mask;
	reg_val |= value;
	ath12k_hif_write32(ab, addr, reg_val);
}

void ath12k_wifi7_hal_tx_update_dscp_tid_map(struct ath12k_base *ab, int id, u8 dscp, u8 tid)
{
	u32 ctrl_reg_val;
	u32 addr;
	u32 start_index, end_index;
	u32 mask;
	u32 value;

	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	/* Enable read/write access */
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	/* Calculate start and end indices within the register */
	start_index = dscp * HAL_TX_BITS_PER_TID;
	end_index = (start_index + (HAL_TX_BITS_PER_TID - 1)) %
		    HAL_TX_NUM_DSCP_REG_SIZE;
	addr += (4 * (start_index / HAL_TX_NUM_DSCP_REG_SIZE));
	start_index %= HAL_TX_NUM_DSCP_REG_SIZE;

	if (end_index < start_index) {
		/* Handle the case where the TID value spans two registers */
		mask = GENMASK((HAL_TX_NUM_DSCP_REG_SIZE - 1), start_index);
		value = (tid << start_index) & mask;
		/* Update the first register */
		ath12k_update_dscp_register(ab, addr, mask, value);

		/* Update the second register */
		addr = addr+4;
		mask = GENMASK(end_index, 0);
		value = (tid >> (HAL_TX_NUM_DSCP_REG_SIZE - start_index)) & mask;
		ath12k_update_dscp_register(ab, addr, mask, value);
	} else {
		/* Handle the case where the TID value fits within one register */
		mask = GENMASK(end_index, start_index);
		value = (tid << start_index) & mask;
		ath12k_update_dscp_register(ab, addr, mask, value);
	}

	/* Disable read/write access */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG,ctrl_reg_val);
}

void ath12k_wifi7_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, u8 *map, int id)
{
	u32 ctrl_reg_val;
	u32 addr;
	u8 hw_map_val[HAL_DSCP_TID_TBL_SIZE], count = 0;
	int i;
	u32 value;

	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	/* Enable read/write access */
	ctrl_reg_val |= HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG, ctrl_reg_val);

	addr = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_DSCP_TID_MAP +
	       (4 * id * (HAL_DSCP_TID_TBL_SIZE / 4));

	/* Configure each DSCP-TID mapping in three bits there by configure
	 * three bytes in an iteration.
	 */
	for (i = 0; i < DSCP_TID_MAP_TBL_ENTRY_SIZE; i += 8) {
		value = 0;

		value |= u32_encode_bits(map[i], HAL_TCL1_RING_FIELD_DSCP_TID_MAP0);
		value |= u32_encode_bits(map[i + 1], HAL_TCL1_RING_FIELD_DSCP_TID_MAP1);
		value |= u32_encode_bits(map[i + 2], HAL_TCL1_RING_FIELD_DSCP_TID_MAP2);
		value |= u32_encode_bits(map[i + 3], HAL_TCL1_RING_FIELD_DSCP_TID_MAP3);
		value |= u32_encode_bits(map[i + 4], HAL_TCL1_RING_FIELD_DSCP_TID_MAP4);
		value |= u32_encode_bits(map[i + 5], HAL_TCL1_RING_FIELD_DSCP_TID_MAP5);
		value |= u32_encode_bits(map[i + 6], HAL_TCL1_RING_FIELD_DSCP_TID_MAP6);
		value |= u32_encode_bits(map[i + 7], HAL_TCL1_RING_FIELD_DSCP_TID_MAP7);

		memcpy(&hw_map_val[count], &value, 3);
		count += 3;
	}

	for (i = 0; i < HAL_DSCP_TID_TBL_SIZE; i += 4) {
		ath12k_hif_write32(ab, addr, *(u32 *)&hw_map_val[i]);
		addr += 4;
	}

	/* Disable read/write access */
	ctrl_reg_val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG);
	ctrl_reg_val &= ~HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
			   HAL_TCL1_RING_CMN_CTRL_REG,
			   ctrl_reg_val);
}
