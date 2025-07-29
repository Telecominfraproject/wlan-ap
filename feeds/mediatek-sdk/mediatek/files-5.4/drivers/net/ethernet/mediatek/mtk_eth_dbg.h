/*
 *   Copyright (C) 2018 MediaTek Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   Copyright (C) 2009-2016 John Crispin <blogic@openwrt.org>
 *   Copyright (C) 2009-2016 Felix Fietkau <nbd@openwrt.org>
 *   Copyright (C) 2013-2016 Michael Lee <igvtee@gmail.com>
 */

#ifndef MTK_ETH_DBG_H
#define MTK_ETH_DBG_H

/* Debug Purpose Register */
#define MTK_PSE_FQFC_CFG		0x100
#define MTK_FE_CDM1_FSM			0x220
#define MTK_FE_CDM2_FSM			0x224
#define MTK_FE_CDM3_FSM			0x238
#define MTK_FE_CDM4_FSM			0x298
#define MTK_FE_CDM5_FSM			0x318
#define MTK_FE_CDM6_FSM			0x328
#define MTK_FE_CDM7_FSM			0x338
#define MTK_FE_GDM1_FSM			0x228
#define MTK_FE_GDM2_FSM			0x22C
#define MTK_FE_GDM3_FSM			0x23C
#define MTK_FE_CDM1_DBG1		0x200
#define MTK_FE_CDM1_DBG2		0x204
#define MTK_FE_CDM2_DBG1		0x208
#define MTK_FE_CDM2_DBG2		0x20C
#define MTK_FE_CDM3_DBG1		0x230
#define MTK_FE_CDM3_DBG2		0x234
#define MTK_FE_CDM4_DBG1		0x290
#define MTK_FE_CDM4_DBG2		0x294
#define MTK_FE_CDM5_DBG1		0x310
#define MTK_FE_CDM5_DBG2		0x314
#define MTK_FE_CDM6_DBG1		0x320
#define MTK_FE_CDM6_DBG2		0x324
#define MTK_FE_CDM7_DBG1		0x330
#define MTK_FE_CDM7_DBG2		0x334
#define MTK_FE_GDM1_DBG1		0x210
#define MTK_FE_GDM1_DBG2		0x214
#define MTK_FE_GDM2_DBG1		0x218
#define MTK_FE_GDM2_DBG2		0x21C
#define MTK_FE_PSE_FREE			0x240
#define MTK_FE_DROP_FQ			0x244
#define MTK_FE_DROP_FC			0x248
#define MTK_FE_DROP_PPE			0x24C
#define MTK_MAC_FSM(x)			(0x1010C + ((x) * 0x100))
#define MTK_SGMII_FALSE_CARRIER_CNT(x)	(0x10060028 + ((x) * 0x10000))
#define MTK_SGMII_EFUSE			0x11D008C8
#define MTK_WED_RTQM_GLO_CFG		0x15010B00

#define MTK_FE_GDM_FSM(x)		(((x) == 2) ? MTK_FE_GDM3_FSM :	\
					 ((x) == 1) ? MTK_FE_GDM2_FSM : MTK_FE_GDM1_FSM)

#define MTK_FE_CDM_FSM(x)		(((x) == 0) ? MTK_FE_CDM3_FSM :	\
					 ((x) == 1) ? MTK_FE_CDM4_FSM : MTK_FE_CDM5_FSM)

#define MTK_FE_WDMA_OQ(x)		\
	(((x) == 0) ? (mtk_r32(eth, MTK_PSE_OQ_STA(4)) & 0x00000FFF) :	\
	(((x) == 1) ? (mtk_r32(eth, MTK_PSE_OQ_STA(4)) & 0x0FFF0000) :	\
	(mtk_r32(eth, MTK_PSE_OQ_STA(6)) & 0x0FFF0000)))

#define MTK_FE_GDM_IQ(x)		\
	(((x) == 2) ? mtk_r32(eth, MTK_PSE_IQ_STA(7)) & 0x0fff0000 :	\
	((x) == 1) ? mtk_r32(eth, MTK_PSE_IQ_STA(1)) & 0x00000fff :	\
	mtk_r32(eth, MTK_PSE_IQ_STA(0)) & 0x0fff0000)

#define MTK_FE_GDM_OQ(x)		\
	(((x) == 2) ? mtk_r32(eth, MTK_PSE_OQ_STA(7)) & 0x0fff0000 :	\
	((x) == 1) ? mtk_r32(eth, MTK_PSE_OQ_STA(1)) & 0x00000fff :	\
	mtk_r32(eth, MTK_PSE_OQ_STA(0)) & 0x0fff0000)

#if defined(CONFIG_MEDIATEK_NETSYS_V2) || defined(CONFIG_MEDIATEK_NETSYS_V3)
#define MTK_PSE_IQ_STA(x)		(0x180 + (x) * 0x4)
#define MTK_PSE_OQ_STA(x)		(0x1A0 + (x) * 0x4)
#else
#define MTK_PSE_IQ_STA(x)		(0x110 + (x) * 0x4)
#define MTK_PSE_OQ_STA(x)		(0x118 + (x) * 0x4)
#endif

#define MTKETH_MII_READ                  0x89F3
#define MTKETH_MII_WRITE                 0x89F4
#define MTKETH_ESW_REG_READ              0x89F1
#define MTKETH_ESW_REG_WRITE             0x89F2
#define MTKETH_MII_READ_CL45             0x89FC
#define MTKETH_MII_WRITE_CL45            0x89FD
#define REG_ESW_MAX                     0xFC

#define PROCREG_ESW_CNT			"esw_cnt"
#define PROCREG_MAC_CNT			"mac_cnt"
#define PROCREG_XFI_CNT			"xfi_cnt"
#define PROCREG_TXRING			"tx_ring"
#define PROCREG_HWTXRING		"hwtx_ring"
#define PROCREG_RXRING			"rx_ring"
#define PROCREG_DIR			"mtketh"
#define PROCREG_DBG_REGS		"dbg_regs"
#define PROCREG_RSS_CTRL		"rss_ctrl"
#define PROCREG_HW_LRO_STATS		"hw_lro_stats"
#define PROCREG_HW_LRO_AUTO_TLB		"hw_lro_auto_tlb"
#define PROCREG_RESET_EVENT		"reset_event"

#define MAX_SWITCH_PORT_NUM		(6)
#if defined(CONFIG_MEDIATEK_NETSYS_V3)
#define MAX_PPPQ_QUEUE_NUM		(2 * MAX_SWITCH_PORT_NUM + 2)
#else
#define MAX_PPPQ_QUEUE_NUM		(2 * MAX_SWITCH_PORT_NUM)
#endif

/* GMAC MIB Register */
#define MTK_MAC_MIB_BASE(x)		(0x10400 + (x * 0x60))
#define MTK_MAC_RX_UC_PKT_CNT_L		0x00
#define MTK_MAC_RX_UC_PKT_CNT_H		0x04
#define MTK_MAC_RX_UC_BYTE_CNT_L	0x08
#define MTK_MAC_RX_UC_BYTE_CNT_H	0x0C
#define MTK_MAC_RX_MC_PKT_CNT_L		0x10
#define MTK_MAC_RX_MC_PKT_CNT_H		0x14
#define MTK_MAC_RX_MC_BYTE_CNT_L	0x18
#define MTK_MAC_RX_MC_BYTE_CNT_H	0x1C
#define MTK_MAC_RX_BC_PKT_CNT_L		0x20
#define MTK_MAC_RX_BC_PKT_CNT_H		0x24
#define MTK_MAC_RX_BC_BYTE_CNT_L	0x28
#define MTK_MAC_RX_BC_BYTE_CNT_H	0x2C
#define MTK_MAC_TX_UC_PKT_CNT_L		0x30
#define MTK_MAC_TX_UC_PKT_CNT_H		0x34
#define MTK_MAC_TX_UC_BYTE_CNT_L	0x38
#define MTK_MAC_TX_UC_BYTE_CNT_H	0x3C
#define MTK_MAC_TX_MC_PKT_CNT_L		0x40
#define MTK_MAC_TX_MC_PKT_CNT_H		0x44
#define MTK_MAC_TX_MC_BYTE_CNT_L	0x48
#define MTK_MAC_TX_MC_BYTE_CNT_H	0x4C
#define MTK_MAC_TX_BC_PKT_CNT_L		0x50
#define MTK_MAC_TX_BC_PKT_CNT_H		0x54
#define MTK_MAC_TX_BC_BYTE_CNT_L	0x58
#define MTK_MAC_TX_BC_BYTE_CNT_H	0x5C

/* XFI MAC MIB Register */
#define MTK_XFI_MIB_BASE(x)		(MTK_XMAC_MCR(x))
#define MTK_XFI_CNT_CTRL		0x100
#define MTK_XFI_TX_PKT_CNT		0x108
#define MTK_XFI_TX_ETH_CNT		0x114
#define MTK_XFI_TX_PAUSE_CNT		0x120
#define MTK_XFI_TX_BYTE_CNT		0x134
#define MTK_XFI_TX_UC_PKT_CNT_L		0x150
#define MTK_XFI_TX_UC_PKT_CNT_H		0x154
#define MTK_XFI_TX_MC_PKT_CNT_L		0x160
#define MTK_XFI_TX_MC_PKT_CNT_H		0x164
#define MTK_XFI_TX_BC_PKT_CNT_L		0x170
#define MTK_XFI_TX_BC_PKT_CNT_H		0x174

#define MTK_XFI_RX_PKT_CNT		0x188
#define MTK_XFI_RX_ETH_CNT		0x18C
#define MTK_XFI_RX_PAUSE_CNT		0x190
#define MTK_XFI_RX_LEN_ERR_CNT		0x194
#define MTK_XFI_RX_CRC_ERR_CNT		0x198
#define MTK_XFI_RX_UC_PKT_CNT_L		0x1C0
#define MTK_XFI_RX_UC_PKT_CNT_H		0x1C4
#define MTK_XFI_RX_MC_PKT_CNT_L		0x1D0
#define MTK_XFI_RX_MC_PKT_CNT_H		0x1D4
#define MTK_XFI_RX_BC_PKT_CNT_L		0x1E0
#define MTK_XFI_RX_BC_PKT_CNT_H		0x1E4
#define MTK_XFI_RX_UC_DROP_CNT		0x200
#define MTK_XFI_RX_BC_DROP_CNT		0x204
#define MTK_XFI_RX_MC_DROP_CNT		0x208
#define MTK_XFI_RX_ALL_DROP_CNT		0x20C

#define MTK_MT753X_PMCR_P(x)	(0x3000 + (x) * 100)

#define PRINT_FORMATTED_MAC_MIB64(seq, reg)			\
{								\
	seq_printf(seq, "| MAC%d_%s	: %010llu |\n",		\
		   gdm_id, #reg,				\
		   mtk_r32(eth, MTK_MAC_MIB_BASE(gdm_id) +	\
			   MTK_MAC_##reg##_L) +			\
		   ((u64)mtk_r32(eth, MTK_MAC_MIB_BASE(gdm_id) +\
				 MTK_MAC_##reg##_H) << 32));	\
}

#define PRINT_FORMATTED_XFI_MIB(seq, reg, mask)			\
{								\
	seq_printf(seq, "| XFI%d_%s	: %010lu |\n",		\
		   gdm_id, #reg,				\
		   FIELD_GET(mask, mtk_r32(eth,			\
			     MTK_XFI_MIB_BASE(gdm_id) +		\
			     MTK_XFI_##reg)));			\
}

#define PRINT_FORMATTED_XFI_MIB64(seq, reg)			\
{								\
	seq_printf(seq, "| XFI%d_%s	: %010llu |\n",		\
		   gdm_id, #reg,				\
		   mtk_r32(eth, MTK_XFI_MIB_BASE(gdm_id) +	\
			   MTK_XFI_##reg##_L) +			\
		   ((u64)mtk_r32(eth, MTK_XFI_MIB_BASE(gdm_id) +\
				 MTK_XFI_##reg##_H) << 32));	\
}

/* HW LRO flush reason */
#define MTK_HW_LRO_AGG_FLUSH		(1)
#define MTK_HW_LRO_AGE_FLUSH		(2)
#define MTK_HW_LRO_NOT_IN_SEQ_FLUSH	(3)
#define MTK_HW_LRO_TIMESTAMP_FLUSH	(4)
#define MTK_HW_LRO_NON_RULE_FLUSH	(5)

#define SET_PDMA_RXRING_MAX_AGG_CNT(eth, x, y)						\
{											\
	const struct mtk_reg_map *reg_map = eth->soc->reg_map;				\
	u32 reg_val1, reg_val2;								\
	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_GLO_MEM_ACCESS)) {			\
		reg_val1 = mtk_r32(eth, reg_map->pdma.lro_rx_ctrl_dw0 +			\
					0x8 + (x * 0x40));				\
		reg_val2 = mtk_r32(eth, reg_map->pdma.lro_rx_ctrl_dw0 +			\
					0xc + (x * 0x40));				\
		reg_val1 &= ~MTK_LRO_RING_AGG_CNT_L_MASK;				\
		reg_val2 &= ~MTK_LRO_RING_AGG_CNT_H_MASK;				\
		reg_val1 |= ((y) & 0x3f) << MTK_LRO_RING_AGG_CNT_L_OFFSET;		\
		reg_val2 |= (((y) >> 6) & 0x03) <<					\
			     MTK_LRO_RING_AGG_CNT_H_OFFSET;				\
		mtk_w32(eth, reg_val1, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				       0x8 + (x * 0x40));				\
		mtk_w32(eth, reg_val2, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				       0xc + (x * 0x40));				\
	} else {									\
		reg_val1 = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);		\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_READ);		\
		mtk_w32(eth, reg_val1, MTK_GLO_MEM_CTRL);				\
		reg_val1 = mtk_r32(eth, MTK_GLO_MEM_DATA(1));				\
		reg_val1 &= ~MTK_RING_MAX_AGG_CNT;					\
		reg_val1 |= FIELD_PREP(MTK_RING_MAX_AGG_CNT, y);			\
		mtk_w32(eth, reg_val1, MTK_GLO_MEM_DATA(1));				\
		reg_val1 = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);		\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_WRITE);		\
		mtk_w32(eth, reg_val1, MTK_GLO_MEM_CTRL);				\
	}										\
}

#define SET_PDMA_RXRING_AGG_TIME(eth, x, y)						\
{											\
	const struct mtk_reg_map *reg_map = eth->soc->reg_map;				\
	u32 reg_val;									\
	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_GLO_MEM_ACCESS)) {			\
		reg_val = mtk_r32(eth, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				       0x8 + (x * 0x40));				\
		reg_val &= ~MTK_LRO_RING_AGG_TIME_MASK;					\
		reg_val |= ((y) & 0xffff) << MTK_LRO_RING_AGG_TIME_OFFSET;		\
		mtk_w32(eth, reg_val, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				      0x8 + (x * 0x40));				\
	} else {									\
		reg_val = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);			\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_READ);		\
		mtk_w32(eth, reg_val, MTK_GLO_MEM_CTRL);				\
		reg_val = mtk_r32(eth, MTK_GLO_MEM_DATA(0));				\
		reg_val &= ~MTK_RING_MAX_AGG_TIME_V2;					\
		reg_val |= FIELD_PREP(MTK_RING_MAX_AGG_TIME_V2, y);			\
		mtk_w32(eth, reg_val, MTK_GLO_MEM_DATA(0));				\
		reg_val = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);			\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_WRITE);		\
		mtk_w32(eth, reg_val, MTK_GLO_MEM_CTRL);				\
	}										\
}

#define SET_PDMA_RXRING_AGE_TIME(eth, x, y)						\
{											\
	const struct mtk_reg_map *reg_map = eth->soc->reg_map;				\
	u32 reg_val1, reg_val2;								\
	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_GLO_MEM_ACCESS)) {			\
		reg_val1 = mtk_r32(eth, reg_map->pdma.lro_rx_ctrl_dw0 +			\
					0x4 + (x * 0x40));				\
		reg_val2 = mtk_r32(eth, reg_map->pdma.lro_rx_ctrl_dw0 +			\
					0x8 + (x * 0x40));				\
		reg_val1 &= ~MTK_LRO_RING_AGE_TIME_L_MASK;				\
		reg_val2 &= ~MTK_LRO_RING_AGE_TIME_H_MASK;				\
		reg_val1 |= ((y) & 0x3ff) << MTK_LRO_RING_AGE_TIME_L_OFFSET;		\
		reg_val2 |= (((y) >> 10) & 0x03f) <<					\
			    MTK_LRO_RING_AGE_TIME_H_OFFSET;				\
		mtk_w32(eth, reg_val1, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				       0x4 + (x * 0x40));				\
		mtk_w32(eth, reg_val2, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				       0x8 + (x * 0x40));				\
	} else {									\
		reg_val1 = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);		\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_READ);		\
		mtk_w32(eth, reg_val1, MTK_GLO_MEM_CTRL);				\
		reg_val1 = mtk_r32(eth, MTK_GLO_MEM_DATA(0));				\
		reg_val1 &= ~MTK_RING_AGE_TIME;						\
		reg_val1 |= FIELD_PREP(MTK_RING_AGE_TIME, y);				\
		mtk_w32(eth, reg_val1, MTK_GLO_MEM_DATA(0));				\
		reg_val1 = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);		\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val1 |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_WRITE);		\
		mtk_w32(eth, reg_val1, MTK_GLO_MEM_CTRL);				\
	}										\
}

#define SET_PDMA_LRO_BW_THRESHOLD(eth, x)				\
{									\
	const struct mtk_reg_map *reg_map = eth->soc->reg_map;		\
	u32 reg_val = mtk_r32(eth, reg_map->pdma.lro_ctrl_dw0 + 0x8);	\
	reg_val = (x);							\
	mtk_w32(eth, reg_val, reg_map->pdma.lro_ctrl_dw0 + 0x8);	\
}

#define SET_PDMA_RXRING_VALID(eth, x, y)						\
{											\
	const struct mtk_reg_map *reg_map = eth->soc->reg_map;				\
	u32 reg_val;									\
	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_GLO_MEM_ACCESS)) {			\
		reg_val = mtk_r32(eth, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				       0x8 + (x * 0x40));				\
		reg_val &= ~(0x1 << MTK_RX_PORT_VALID_OFFSET);				\
		reg_val |= ((y) & 0x1) << MTK_RX_PORT_VALID_OFFSET;			\
		mtk_w32(eth, reg_val, reg_map->pdma.lro_rx_ctrl_dw0 +			\
				      0x8 + (x * 0x40));				\
	} else {									\
		reg_val = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);			\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_READ);		\
		mtk_w32(eth, reg_val, MTK_GLO_MEM_CTRL);				\
		reg_val = mtk_r32(eth, MTK_GLO_MEM_DATA(1));				\
		reg_val &= ~MTK_RING_OPMODE;						\
		reg_val |= FIELD_PREP(MTK_RING_OPMODE, y);				\
		mtk_w32(eth, reg_val, MTK_GLO_MEM_DATA(1));				\
		reg_val = FIELD_PREP(MTK_GLO_MEM_IDX, MTK_LRO_MEM_IDX);			\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_ADDR, MTK_LRO_MEM_CFG_BASE + x);	\
		reg_val |= FIELD_PREP(MTK_GLO_MEM_CMD, MTK_GLO_MEM_WRITE);		\
		mtk_w32(eth, reg_val, MTK_GLO_MEM_CTRL);				\
	}										\
}

struct mtk_pse_fs_lgc_info_v2 {
	u32 rev3 : 14;
	u32 ppe_crsn: 5;
	u32 sport : 4;
	u32 is_l4f: 1;
	u32 is_l4vld: 1;
	u32 is_tack : 1;
	u32 is_ip4f : 1;
	u32 is_ip4 : 1;
	u32 is_ip6 : 1;
	u32 dr_idx : 2;
	u32 rev2 : 4;
	u32 l3_pidx : 2;
	u32 rev : 2;
	u32 fport : 4;
	u32 l2_len : 7;
	u32 l3_len : 14;
} __packed;

struct mtk_pse_fs_lgc_info_v3 {
	u32 is_snap : 1;
	u32 vofst : 3;
	u32 l3_pidx : 2;
	u32 pse_sport : 4;
	u32 fport : 4;
	u32 ppe_crsn: 5;
	u32 sport : 4;
	u32 is_l4f: 1;
	u32 is_l4vld: 1;
	u32 is_tack : 1;
	u32 is_ip4f : 1;
	u32 is_ip4 : 1;
	u32 is_ip6 : 1;
	u32 is_err_pkt : 1;
	u32 err_pkt_action : 2;
	u32 pl_end : 11;
	u32 l2_len : 7;
	u32 l3_len : 14;
} __packed;

struct mtk_lro_alt_v1_info0 {
	u32 dtp : 16;
	u32 stp : 16;
};

struct mtk_lro_alt_v1_info1 {
	u32 sip0 : 32;
};

struct mtk_lro_alt_v1_info2 {
	u32 sip1 : 32;
};

struct mtk_lro_alt_v1_info3 {
	u32 sip2 : 32;
};

struct mtk_lro_alt_v1_info4 {
	u32 sip3 : 32;
};

struct mtk_lro_alt_v1_info5 {
	u32 vlan_vid0 : 32;
};

struct mtk_lro_alt_v1_info6 {
	u32 vlan_vid1 : 16;
	u32 vlan_vid_vld : 4;
	u32 cnt : 12;
};

struct mtk_lro_alt_v1_info7 {
	u32 dw_len : 32;
};

struct mtk_lro_alt_v1_info8 {
	u32 dip_id : 2;
	u32 ipv6 : 1;
	u32 ipv4 : 1;
	u32 resv : 27;
	u32 valid : 1;
};

struct mtk_lro_alt_v1 {
	struct mtk_lro_alt_v1_info0 alt_info0;
	struct mtk_lro_alt_v1_info1 alt_info1;
	struct mtk_lro_alt_v1_info2 alt_info2;
	struct mtk_lro_alt_v1_info3 alt_info3;
	struct mtk_lro_alt_v1_info4 alt_info4;
	struct mtk_lro_alt_v1_info5 alt_info5;
	struct mtk_lro_alt_v1_info6 alt_info6;
	struct mtk_lro_alt_v1_info7 alt_info7;
	struct mtk_lro_alt_v1_info8 alt_info8;
};

struct mtk_lro_alt_v2_info0 {
	u32 v2_id_h:3;
	u32 v1_id:12;
	u32 v0_id:12;
	u32 v3_valid:1;
	u32 v2_valid:1;
	u32 v1_valid:1;
	u32 v0_valid:1;
	u32 valid:1;
};

struct mtk_lro_alt_v2_info1 {
	u32 sip3_h:9;
	u32 v6_valid:1;
	u32 v4_valid:1;
	u32 v3_id:12;
	u32 v2_id_l:9;
};

struct mtk_lro_alt_v2_info2 {
	u32 sip2_h:9;
	u32 sip3_l:23;
};
struct mtk_lro_alt_v2_info3 {
	u32 sip1_h:9;
	u32 sip2_l:23;
};
struct mtk_lro_alt_v2_info4 {
	u32 sip0_h:9;
	u32 sip1_l:23;
};
struct mtk_lro_alt_v2_info5 {
	u32 dip3_h:9;
	u32 sip0_l:23;
};
struct mtk_lro_alt_v2_info6 {
	u32 dip2_h:9;
	u32 dip3_l:23;
};
struct mtk_lro_alt_v2_info7 {
	u32 dip1_h:9;
	u32 dip2_l:23;
};
struct mtk_lro_alt_v2_info8 {
	u32 dip0_h:9;
	u32 dip1_l:23;
};
struct mtk_lro_alt_v2_info9 {
	u32 sp_h:9;
	u32 dip0_l:23;
};
struct mtk_lro_alt_v2_info10 {
	u32 resv:9;
	u32 dp:16;
	u32 sp_l:7;
};

struct mtk_lro_alt_v2 {
	struct mtk_lro_alt_v2_info0 alt_info0;
	struct mtk_lro_alt_v2_info1 alt_info1;
	struct mtk_lro_alt_v2_info2 alt_info2;
	struct mtk_lro_alt_v2_info3 alt_info3;
	struct mtk_lro_alt_v2_info4 alt_info4;
	struct mtk_lro_alt_v2_info5 alt_info5;
	struct mtk_lro_alt_v2_info6 alt_info6;
	struct mtk_lro_alt_v2_info7 alt_info7;
	struct mtk_lro_alt_v2_info8 alt_info8;
	struct mtk_lro_alt_v2_info9 alt_info9;
	struct mtk_lro_alt_v2_info10 alt_info10;
};

struct mtk_esw_reg {
	unsigned int off;
	unsigned int val;
};

struct mtk_mii_ioctl_data {
	u16 phy_id;
	u16 reg_num;
	unsigned int val_in;
	unsigned int val_out;
};

extern u32 _mtk_mdio_read(struct mtk_eth *eth, int phy_addr, int phy_reg);
extern u32 _mtk_mdio_write(struct mtk_eth *eth, int phy_addr,
		    int phy_reg, u16 write_data);

extern atomic_t force;
extern atomic_t reset_lock;
extern int eth_debug_level;

int debug_proc_init(struct mtk_eth *eth);
void debug_proc_exit(void);

int mtketh_debugfs_init(struct mtk_eth *eth);
void mtketh_debugfs_exit(struct mtk_eth *eth);
int mtk_do_priv_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
void hw_lro_stats_update(u32 ring_no, struct mtk_rx_dma_v2 *rxd);
void hw_lro_flush_stats_update(u32 ring_no, struct mtk_rx_dma_v2 *rxd);
void mt753x_set_port_link_state(bool up);

#endif /* MTK_ETH_DBG_H */
