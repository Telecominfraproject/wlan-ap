/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Henry Yen <henry.yen@mediatek.com>
 */

#include <linux/regmap.h>
#include "mtk_eth_soc.h"
#include "mtk_eth_dbg.h"
#include "mtk_eth_reset.h"

char* mtk_reset_event_name[32] = {
	[MTK_EVENT_FORCE]	= "Force",
	[MTK_EVENT_WARM_CNT]	= "Warm",
	[MTK_EVENT_COLD_CNT]	= "Cold",
	[MTK_EVENT_TOTAL_CNT]	= "Total",
	[MTK_EVENT_FQ_EMPTY]	= "FQ Empty",
	[MTK_EVENT_TSO_FAIL]	= "TSO Fail",
	[MTK_EVENT_TSO_ILLEGAL]	= "TSO Illegal",
	[MTK_EVENT_TSO_ALIGN]	= "TSO Align",
	[MTK_EVENT_RFIFO_OV]	= "RFIFO OV",
	[MTK_EVENT_RFIFO_UF]	= "RFIFO UF",
};

struct mtk_qdma_cfg {
	u32 qtx_cfg[MTK_QDMA_TX_NUM];
	u32 qtx_sch[MTK_QDMA_TX_NUM];
	u32 tx_sch[2];
};

static struct mtk_qdma_cfg mtk_qdma_cfg_backup;
static int mtk_rest_cnt = 0;
int mtk_wifi_num;
bool mtk_stop_fail;

typedef u32 (*mtk_monitor_xdma_func) (struct mtk_eth *eth);

bool (*mtk_check_wifi_busy)(u32 wdma_idx) = NULL;
EXPORT_SYMBOL(mtk_check_wifi_busy);

void mtk_reset_event_update(struct mtk_eth *eth, u32 id)
{
	struct mtk_reset_event *reset_event = &eth->reset_event;
	reset_event->count[id]++;
}

static void mtk_dump_reg(void *_eth, char *name, u32 offset, u32 range)
{
	struct mtk_eth *eth = _eth;
	u32 cur = offset;

	pr_info("\n============ %s ============\n", name);
	while (cur < offset + range) {
		pr_info("0x%x: %08x %08x %08x %08x\n",
			cur, mtk_r32(eth, cur), mtk_r32(eth, cur + 0x4),
			mtk_r32(eth, cur + 0x8), mtk_r32(eth, cur + 0xc));
		cur += 0x10;
	}
}

static void mtk_dump_regmap(struct regmap *pmap, char *name,
			    u32 offset, u32 range)
{
	unsigned int cur = offset;
	unsigned int val1 = 0, val2 = 0, val3 = 0, val4 = 0;

	if (!pmap)
		return;

	pr_info("\n============ %s ============\n", name);
	while (cur < offset + range) {
		regmap_read(pmap, cur, &val1);
		regmap_read(pmap, cur + 0x4, &val2);
		regmap_read(pmap, cur + 0x8, &val3);
		regmap_read(pmap, cur + 0xc, &val4);
		pr_info("0x%x: %08x %08x %08x %08x\n",
			cur, val1, val2, val3, val4);
		cur += 0x10;
	}
}

int mtk_eth_cold_reset(struct mtk_eth *eth)
{
	u32 reset_bits = 0;

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V2) ||
	    MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3))
		regmap_write(eth->ethsys, ETHSYS_FE_RST_CHK_IDLE_EN, 0);

	reset_bits = RSTCTRL_FE | RSTCTRL_PPE0;
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_RSTCTRL_PPE1))
		reset_bits |= RSTCTRL_PPE1;
#if defined(CONFIG_MEDIATEK_NETSYS_V3)
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_RSTCTRL_PPE2))
		reset_bits |= RSTCTRL_PPE2;
	if (eth->reset.event == MTK_FE_START_RESET)
		reset_bits |= RSTCTRL_WDMA0 | RSTCTRL_WDMA1 | RSTCTRL_WDMA2;
#endif
	if (eth->reset.rstctrl_eth)
		reset_bits |= RSTCTRL_ETH;
	ethsys_reset(eth, reset_bits);

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3))
		regmap_write(eth->ethsys, ETHSYS_FE_RST_CHK_IDLE_EN, 0x6f8ff);
	else if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V2))
		regmap_write(eth->ethsys, ETHSYS_FE_RST_CHK_IDLE_EN, 0x3ffffff);

	return 0;
}

int mtk_eth_warm_reset(struct mtk_eth *eth)
{
	u32 reset_bits = 0, i = 0, done = 0;
	u32 val1 = 0, val2 = 0, val3 = 0;

	mdelay(100);

	if (dbg_show_level)
		mtk_dump_netsys_info_brief(eth);

	reset_bits |= RSTCTRL_FE;
	regmap_update_bits(eth->ethsys, ETHSYS_RSTCTRL,
			   reset_bits, reset_bits);

	while (i < 1000) {
		regmap_read(eth->ethsys, ETHSYS_RSTCTRL, &val1);
		if (val1 & RSTCTRL_FE)
			break;
		i++;
		udelay(1);
	}

	if (i < 1000) {
		done = 1;
		reset_bits = RSTCTRL_PPE0;
		if (MTK_HAS_CAPS(eth->soc->caps, MTK_RSTCTRL_PPE1))
			reset_bits |= RSTCTRL_PPE1;
#if defined(CONFIG_MEDIATEK_NETSYS_V3)
		if (MTK_HAS_CAPS(eth->soc->caps, MTK_RSTCTRL_PPE2))
			reset_bits |= RSTCTRL_PPE2;
		if (eth->reset.event == MTK_FE_START_RESET)
			reset_bits |= RSTCTRL_WDMA0 | RSTCTRL_WDMA1 | RSTCTRL_WDMA2;
#endif
		if (eth->reset.rstctrl_eth)
			reset_bits |= RSTCTRL_ETH;

		regmap_update_bits(eth->ethsys, ETHSYS_RSTCTRL,
				   reset_bits, reset_bits);

		udelay(1);
		regmap_read(eth->ethsys, ETHSYS_RSTCTRL, &val2);
		if (!(val2 & reset_bits)) {
			pr_info("[%s] error val2=0x%x reset_bits=0x%x !\n",
				__func__, val2, reset_bits);
			done = 0;
		}
		reset_bits |= RSTCTRL_FE;
		regmap_update_bits(eth->ethsys, ETHSYS_RSTCTRL,
				   reset_bits, ~reset_bits);
		udelay(1);
		regmap_read(eth->ethsys, ETHSYS_RSTCTRL, &val3);
		if (val3 & reset_bits) {
			pr_info("[%s] error val3=0x%x reset_bits=0x%x !\n",
				__func__, val3, reset_bits);
			done = 0;
		}
		mtk_reset_event_update(eth, MTK_EVENT_WARM_CNT);
	}

	pr_info("[%s] reset record val1=0x%x, val2=0x%x, val3=0x%x i:%d done:%d\n",
		__func__, val1, val2, val3, i, done);

	if (!done) {
		pr_info("[%s] execute fe cold reset\n", __func__);
		mtk_eth_cold_reset(eth);
		mtk_dump_netsys_info_brief(eth);
	}

	return 0;
}

u32 mtk_check_reset_event(struct mtk_eth *eth, u32 status)
{
	u32 ret = 0, val = 0;

	if ((status & MTK_FE_INT_FQ_EMPTY) ||
	    (status & MTK_FE_INT_RFIFO_UF) ||
	    (status & MTK_FE_INT_RFIFO_OV) ||
	    (status & MTK_FE_INT_TSO_FAIL) ||
	    (status & MTK_FE_INT_TSO_ALIGN) ||
	    (status & MTK_FE_INT_TSO_ILLEGAL)) {
		while (status) {
			val = ffs((unsigned int)status) - 1;
			mtk_reset_event_update(eth, val);
			status &= ~(1 << val);
		}
		ret = 1;
	}

	if (atomic_read(&force)) {
		mtk_reset_event_update(eth, MTK_EVENT_FORCE);
		ret = 1;
	}

	if (ret) {
		mtk_reset_event_update(eth, MTK_EVENT_TOTAL_CNT);
		if (dbg_show_level)
			mtk_dump_netsys_info(eth);
	}

	return ret;
}

u32 mtk_gdm_dbg_out(struct mtk_eth *eth, u32 reg, u32 mask, u32 index)
{
	mtk_m32(eth, mask, index, reg);

	return mtk_r32(eth, reg + 0x4);
}

void mtk_gdm_pl_end_page_error(struct mtk_eth *eth, int id)
{
	enum mtk_gdm_dbg_index {
		FS_PL_END_MISMATCH_LGC_INFO_LSB = 20,
		FS_PL_END_MISMATCH_LGC_INFO_MSB,
		FS_PL_END_MISMATCH_EXT_INFO_31_0,
		FS_PL_END_MISMATCH_EXT_INFO_63_32,
		FS_PL_END_MISMATCH_EXT_INFO_95_64,
		FS_PL_END_MISMATCH_EXT_INFO_127_96,
		FS_PL_END_MISMATCH_EXT_INFO_159_128,
		FS_PL_END_MISMATCH_EXT_INFO_161_160,
		FS_PL_END_MISMATCH_LINK_LIST_INDEX,
	};

	if (id < 0 || id > 2)
		return;

	pr_info("====================================================");
	pr_info("GDM%d FS_PL_END_MISMATCH_LGC_INFO_LSB      = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LGC_INFO_LSB)));
	pr_info("GDM%d FS_PL_END_MISMATCH_LGC_INFO_MSB      = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LGC_INFO_MSB)));
	pr_info("GDM%d FS_PL_END_MISMATCH_EXT_INFO[31:0]    = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_31_0)));
	pr_info("GDM%d FS_PL_END_MISMATCH_EXT_INFO[63:32]   = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_63_32)));
	pr_info("GDM%d FS_PL_END_MISMATCH_EXT_INFO[95:64]   = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_95_64)));
	pr_info("GDM%d FS_PL_END_MISMATCH_EXT_INFO[127:96]  = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_127_96)));
	pr_info("GDM%d FS_PL_END_MISMATCH_EXT_INFO[159:128] = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_159_128)));
	pr_info("GDM%d FS_PL_END_MISMATCH_EXT_INFO[161:160] = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_161_160)));
	pr_info("GDM%d FS_PL_END_MISMATCH_LINK_LIST_INDEX = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_GDM_DBG_CTRL(id), GDM_DBG_IDX_MASK,
				FIELD_PREP(GDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LINK_LIST_INDEX)));
	pr_info("====================================================");
}

void mtk_cdm_v2_pl_end_page_error(struct mtk_eth *eth, int id)
{
	enum mtk_gdm_dbg_index {
		FS_PL_END_MISMATCH_LGC_INFO_LSB = 24,
		FS_PL_END_MISMATCH_LGC_INFO_MSB,
		FS_PL_END_MISMATCH_EXT_INFO_31_0,
		FS_PL_END_MISMATCH_EXT_INFO_63_32,
		FS_PL_END_MISMATCH_EXT_INFO_95_64,
		FS_PL_END_MISMATCH_EXT_INFO_127_96,
		FS_PL_END_MISMATCH_EXT_INFO_159_128,
		FS_PL_END_MISMATCH_EXT_INFO_161_160,
		FS_PL_END_MISMATCH_LINK_LIST_INDEX,
	};

	if (id < 0 || id > 2)
		return;

	pr_info("====================================================");
	pr_info("CDM%d FS_PL_END_MISMATCH_LGC_INFO_LSB      = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LGC_INFO_LSB)));
	pr_info("CDM%d FS_PL_END_MISMATCH_LGC_INFO_MSB      = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LGC_INFO_MSB)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[31:0]    = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_31_0)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[63:32]   = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_63_32)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[95:64]   = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_95_64)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[127:96]  = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_127_96)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[159:128] = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_159_128)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[161:160] = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_161_160)));
	pr_info("CDM%d FS_PL_END_MISMATCH_LINK_LIST_INDEX = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LINK_LIST_INDEX)));
	pr_info("====================================================");
}

void mtk_cdm_v3_pl_end_page_error(struct mtk_eth *eth, int id)
{
	enum mtk_gdm_dbg_index {
		FS_PL_END_MISMATCH_LGC_INFO_LSB = 27,
		FS_PL_END_MISMATCH_LGC_INFO_MSB,
		FS_PL_END_MISMATCH_EXT_INFO_31_0,
		FS_PL_END_MISMATCH_EXT_INFO_63_32,
		FS_PL_END_MISMATCH_EXT_INFO_95_64,
		FS_PL_END_MISMATCH_EXT_INFO_127_96,
		FS_PL_END_MISMATCH_EXT_INFO_159_128,
		FS_PL_END_MISMATCH_EXT_INFO_161_160,
		FS_PL_END_MISMATCH_LINK_LIST_INDEX,
	};

	if (id < 2 || id > 8)
		return;

	pr_info("====================================================");
	pr_info("CDM%d FS_PL_END_MISMATCH_LGC_INFO_LSB      = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LGC_INFO_LSB)));
	pr_info("CDM%d FS_PL_END_MISMATCH_LGC_INFO_MSB      = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LGC_INFO_MSB)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[31:0]    = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_31_0)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[63:32]   = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_63_32)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[95:64]   = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_95_64)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[127:96]  = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_127_96)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[159:128] = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_159_128)));
	pr_info("CDM%d FS_PL_END_MISMATCH_EXT_INFO[161:160] = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_EXT_INFO_161_160)));
	pr_info("CDM%d FS_PL_END_MISMATCH_LINK_LIST_INDEX = %08x\n", id + 1,
		mtk_gdm_dbg_out(eth, FE_CDM_DBG_CTRL(id), CDM_DBG_IDX_MASK,
				FIELD_PREP(CDM_DBG_IDX_MASK,
					   FS_PL_END_MISMATCH_LINK_LIST_INDEX)));
	pr_info("====================================================");
}

irqreturn_t mtk_handle_fe_irq(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;
	u32 status = 0, val = 0;
	int i;

	status = mtk_r32(eth, MTK_FE_INT_STATUS);
	pr_info("[%s] Trigger FE Misc ISR: 0x%x\n", __func__, status);

	while (status) {
		val = ffs((unsigned int)status) - 1;
		status &= ~(1 << val);

		if ((val == MTK_EVENT_TSO_FAIL) ||
		    (val == MTK_EVENT_TSO_ILLEGAL) ||
		    (val == MTK_EVENT_TSO_ALIGN) ||
		    (val == MTK_EVENT_RFIFO_OV) ||
		    (val == MTK_EVENT_RFIFO_UF))
			pr_info("[%s] Detect reset event: %s !\n", __func__,
				mtk_reset_event_name[val]);
	}
	mtk_w32(eth, 0xFFFFFFFF, MTK_FE_INT_STATUS);

	if (eth->soc->caps == MT7987_CAPS) {
		bool pl_end_error = false;

		/* handle GDM/CDM l3len_over_run and pl_end_error */
		status = mtk_r32(eth, MTK_FE_PINFO_INT_STATUS);
		for (i = 0; i < 3; i++) {
			if (status & MTK_GDM_L3LEN_OVER_RUN(i))
				pr_warn("[%s] Detect GDM%d L3 length over run !",
					__func__, i + 1);
			if (status & MTK_GDM_PL_END_ERR(i)) {
				pr_warn("[%s] Detect GDM%d packet end page error !",
					__func__, i + 1);
				mtk_gdm_pl_end_page_error(eth, i);
				pl_end_error = true;
			}
		}
		for (i = 0; i < 9; i++) {
			if (status & MTK_CDM_L3LEN_OVER_RUN(i))
				pr_warn("[%s] Detect CDM%d L3 length over run !",
					__func__, i + 1);
			if (status & MTK_CDM_PL_END_ERR(i)) {
				pr_warn("[%s] Detect CDM%d packet end page error !",
					__func__, i + 1);
				if (i >= 2)
					mtk_cdm_v3_pl_end_page_error(eth, i);
				else
					mtk_cdm_v2_pl_end_page_error(eth, i);
				pl_end_error = true;
			}
		}
		mtk_w32(eth, status, MTK_FE_PINFO_INT_STATUS);

		/* handle GDM/CDM page_num_mismatch and runt_pkt_error */
		status = mtk_r32(eth, FE_CGDM_INT3);
		for (i = 0; i < 3; i++) {
			if (status & FE_GDM_PAGE_MISMATCH(i))
				pr_warn("[%s] Detect GDM%d page number mismatch !",
					__func__, i + 1);
		}
		for (i = 0; i < 9; i++) {
			if (status & FE_CDM_PAGE_MISMATCH(i))
				pr_warn("[%s] Detect CDM%d page number mismatch !",
					__func__, i + 1);
			if (status & FE_CDM_RUNT_PACKET(i))
				pr_warn("[%s] Detect CDM%d runt packet error !",
					__func__, i + 1);
		}
		mtk_w32(eth, status, FE_CGDM_INT3);

		if (pl_end_error) {
			if (!test_bit(MTK_RESETTING, &eth->state)) {
				/* disable GDM and GDM packet end page error interrupt */
				mtk_w32(eth, 0x0000ffff, MTK_FE_PINFO_INT_ENABLE);
				/* trigger an SER to let PSE go back to work normally */
				atomic_inc(&force);
				schedule_work(&eth->pending_work);
			}
		}
	}

	return IRQ_HANDLED;
}

void mtk_dump_netsys_info(void *_eth)
{
	struct mtk_eth *eth = _eth;
	u32 id = 0;

	mtk_dump_reg(eth, "FE", 0x0, 0x600);
	mtk_dump_reg(eth, "FE", 0x1400, 0x300);
	mtk_dump_reg(eth, "ADMA", PDMA_BASE, 0x300);
	for (id = 0; id < MTK_QDMA_PAGE_NUM; id++){
		mtk_w32(eth, id, MTK_QDMA_PAGE);
		pr_info("\nQDMA PAGE:%x ",mtk_r32(eth, MTK_QDMA_PAGE));
		mtk_dump_reg(eth, "QDMA", QDMA_BASE, 0x100);
		mtk_w32(eth, 0, MTK_QDMA_PAGE);
	}
	mtk_dump_reg(eth, "QDMA", MTK_QRX_BASE_PTR0, 0x300);
	mtk_dump_reg(eth, "WDMA", WDMA_BASE(0), 0x600);
	mtk_dump_reg(eth, "PPE0", PPE_BASE(0), 0x200);
	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V1))
		mtk_dump_reg(eth, "PPE1", PPE_BASE(1), 0x200);
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3))
		mtk_dump_reg(eth, "PPE2", PPE_BASE(2), 0x200);
	mtk_dump_reg(eth, "GMAC", 0x10000, 0x300);
	mtk_dump_regmap(eth->sgmii->pcs[0].regmap,
			"SGMII0", 0, 0x1a0);
	mtk_dump_regmap(eth->sgmii->pcs[1].regmap,
			"SGMII1", 0, 0x1a0);
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		mtk_dump_reg(eth, "XGMAC0", 0x12000, 0x300);
		if (MTK_HAS_CAPS(eth->soc->caps, MTK_GMAC2_USXGMII)) {
			mtk_dump_regmap(eth->usxgmii->pcs[1].regmap,
					"USXGMII1", 0x800, 0x500);
		}
		if (MTK_HAS_CAPS(eth->soc->caps, MTK_GMAC3_USXGMII)) {
			mtk_dump_reg(eth, "XGMAC1", 0x13000, 0x300);
			mtk_dump_regmap(eth->usxgmii->pcs[0].regmap,
					"USXGMII0", 0x800, 0x500);
		}
	}
}

void mtk_dump_netsys_info_brief(void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		regmap_write(eth->ethsys, ETHSYS_LP_NONE_IDLE_LAT0, 0xffffffff);
		regmap_write(eth->ethsys, ETHSYS_LP_NONE_IDLE_LAT1, 0xffffffff);
		mtk_dump_regmap(eth->ethsys, "ETHSYS", 0x140, 0x10);
	}
	mtk_dump_reg(eth, "FE", 0x180, 0x1C0);
	mtk_dump_reg(eth, "ADMA", PDMA_BASE + 0x200, 0x40);
	mtk_dump_reg(eth, "QDMA", QDMA_BASE + 0x200, 0x10);
	mtk_dump_reg(eth, "WDMA0", WDMA_BASE(0), 0x10);
	mtk_dump_reg(eth, "WDMA0", WDMA_BASE(0) + 0x100, 0x10);
	mtk_dump_reg(eth, "WDMA0", WDMA_BASE(0) + 0x200, 0x10);
	mtk_dump_reg(eth, "WDMA1", WDMA_BASE(1), 0x10);
	mtk_dump_reg(eth, "WDMA1", WDMA_BASE(1) + 0x100, 0x10);
	mtk_dump_reg(eth, "WDMA1", WDMA_BASE(1) + 0x200, 0x10);
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		mtk_dump_reg(eth, "WDMA2", WDMA_BASE(2), 0x10);
		mtk_dump_reg(eth, "WDMA2", WDMA_BASE(2) + 0x100, 0x10);
		mtk_dump_reg(eth, "WDMA2", WDMA_BASE(2) + 0x200, 0x10);
	}
	mtk_dump_reg(eth, "PPE0", PPE_BASE(0), 0x10);
	mtk_dump_reg(eth, "PPE0", PPE_BASE(0) + 0x180, 0x20);
	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V1)) {
		mtk_dump_reg(eth, "PPE1", PPE_BASE(1), 0x10);
		mtk_dump_reg(eth, "PPE1", PPE_BASE(1) + 0x180, 0x20);
	}
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		mtk_dump_reg(eth, "PPE2", PPE_BASE(2), 0x10);
		mtk_dump_reg(eth, "PPE2", PPE_BASE(2) + 0x180, 0x20);
	}
	mtk_dump_reg(eth, "GMAC0", 0x10100, 0x10);
	mtk_dump_reg(eth, "GMAC1", 0x10200, 0x10);
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		mtk_dump_reg(eth, "GMAC2", 0x10300, 0x10);
		mtk_dump_reg(eth, "XGMAC0", 0x12000, 0x20);
		if (MTK_HAS_CAPS(eth->soc->caps, MTK_GMAC3_USXGMII))
			mtk_dump_reg(eth, "XGMAC1", 0x13000, 0x20);
	}
}

void mtk_check_pse_oq_sta(struct mtk_eth *eth, u8 port, u16 *pre_opq, u8 *err_opq)
{
	u32 mask = (port % 2) ? 0x0FFF0000 : 0x00000FFF;
	u8 id = port / 2;
	u16 cur_opq;

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V1))
		return;

	cur_opq = (mtk_r32(eth, MTK_PSE_OQ_STA(id)) & mask);
	if ((cur_opq != 0) && (cur_opq == *pre_opq))
		(*err_opq)++;
	else
		*err_opq = 0;

	*pre_opq = cur_opq;
}

u32 mtk_monitor_wdma_tx(struct mtk_eth *eth)
{
	struct wdma_tx_monitor *wdma_tx = &eth->reset.wdma_monitor.tx;
	u32 cur_dtx, tx_busy, fsm_ts;
	u32 i, err_opq = 0, err_flag = 0;

	mtk_check_pse_oq_sta(eth, PSE_GDM1_PORT, &wdma_tx->pre_opq_gdm[0],
			     &wdma_tx->err_opq_gdm[0]);
	mtk_check_pse_oq_sta(eth, PSE_GDM2_PORT, &wdma_tx->pre_opq_gdm[1],
			     &wdma_tx->err_opq_gdm[1]);
	mtk_check_pse_oq_sta(eth, PSE_GDM3_PORT, &wdma_tx->pre_opq_gdm[2],
			     &wdma_tx->err_opq_gdm[2]);
	mtk_check_pse_oq_sta(eth, PSE_WDMA0_PORT, &wdma_tx->pre_opq_wdma[0],
			     &wdma_tx->err_opq_wdma[0]);
	mtk_check_pse_oq_sta(eth, PSE_WDMA1_PORT, &wdma_tx->pre_opq_wdma[1],
			     &wdma_tx->err_opq_wdma[1]);
	mtk_check_pse_oq_sta(eth, PSE_WDMA2_PORT, &wdma_tx->pre_opq_wdma[2],
			     &wdma_tx->err_opq_wdma[2]);

	if ((wdma_tx->err_opq_gdm[0] >= 3) || (wdma_tx->err_opq_gdm[1] >= 3) ||
	    (wdma_tx->err_opq_gdm[2] >= 3) || (wdma_tx->err_opq_wdma[0] >= 3) ||
	    (wdma_tx->err_opq_wdma[1] >= 3) || (wdma_tx->err_opq_wdma[2] >= 3))
		err_opq = 1;

	for (i = 0; i < MTK_WDMA_CNT; i++) {
		cur_dtx = mtk_r32(eth, MTK_WDMA_DTX_PTR(i));
		tx_busy = mtk_r32(eth, MTK_WDMA_GLO_CFG(i)) & MTK_TX_DMA_BUSY;
		fsm_ts = mtk_r32(eth, MTK_FE_CDM_FSM(i)) &
			(MTK_CDM_TS_FSM_MASK | MTK_CDM_TS_PARSER_FSM_MASK);
		/* dtx unchange && tx busy && cdm-ts-fsm && ouput */
		if (cur_dtx == wdma_tx->pre_dtx[i] && tx_busy && fsm_ts && err_opq) {
			wdma_tx->hang_count[i]++;
			if (wdma_tx->hang_count[i] >= 5) {
				pr_info("WDMA %d Tx Info\n", i);
				pr_info("hang count = %d\n", wdma_tx->hang_count[i]);
				pr_info("prev_dtx = 0x%x	| cur_dtx = 0x%x\n",
					wdma_tx->pre_dtx[i], cur_dtx);
				pr_info("WDMA_CTX_PTR = 0x%x\n",
					mtk_r32(eth, MTK_WDMA_CTX_PTR(i)));
				pr_info("WDMA_DTX_PTR = 0x%x\n",
					mtk_r32(eth, MTK_WDMA_DTX_PTR(i)));
				pr_info("WDMA_GLO_CFG = 0x%x\n",
					mtk_r32(eth, MTK_WDMA_GLO_CFG(i)));
				pr_info("WDMA_TX_DBG_MON0 = 0x%x\n",
					mtk_r32(eth, MTK_WDMA_TX_DBG_MON0(i)));
				pr_info("WDMA_CDM_FSM = 0x%x\n",
					mtk_r32(eth, MTK_FE_CDM_FSM(i)));
				pr_info("PSE_OQ_STA0 = 0x%x\n",
					mtk_r32(eth, MTK_PSE_OQ_STA(0)));
				pr_info("PSE_OQ_STA1 = 0x%x\n",
					mtk_r32(eth, MTK_PSE_OQ_STA(1)));
				pr_info("PSE_OQ_STA4 = 0x%x\n",
					mtk_r32(eth, MTK_PSE_OQ_STA(4)));
				pr_info("PSE_OQ_STA7 = 0x%x\n",
					mtk_r32(eth, MTK_PSE_OQ_STA(7)));
				pr_info("==============================\n");
				err_flag = 1;
			}
		} else
			wdma_tx->hang_count[i] = 0;

		wdma_tx->pre_dtx[i] = cur_dtx;
	}

	if (err_flag)
		return MTK_FE_START_RESET;
	else
		return 0;
}

u32 mtk_monitor_wdma_rx(struct mtk_eth *eth)
{
	struct wdma_rx_monitor *wdma_rx = &eth->reset.wdma_monitor.rx;
	bool connsys_busy, netsys_busy;
	u32 cur_crx, cur_drx, cur_opq, cur_fsm, max_cnt;
	u32 i, j, err_flag = 0;
	bool rx_en, rx_busy, crx_unchanged, drx_unchanged;
	int rx_cnt;

	for (i = 0; i < MTK_WDMA_CNT; i++) {
		rx_en = !!(mtk_r32(eth, MTK_WDMA_GLO_CFG(i)) & MTK_RX_DMA_EN);
		rx_busy = !!(mtk_r32(eth, MTK_WDMA_GLO_CFG(i)) & MTK_RX_DMA_BUSY);
		max_cnt = mtk_r32(eth, MTK_WDMA_RX_MAX_CNT(i));
		if (!rx_en || max_cnt == 0)
			continue;

		connsys_busy = netsys_busy = false;
		crx_unchanged = drx_unchanged = true;
		for (j = 0; j < 3; j++) {
			cur_crx = mtk_r32(eth, MTK_WDMA_CRX_PTR(i));
			if (cur_crx != wdma_rx->pre_crx[i])
				crx_unchanged = false;

			cur_drx = mtk_r32(eth, MTK_WDMA_DRX_PTR(i));
			if (cur_drx != wdma_rx->pre_drx[i])
				drx_unchanged = false;

			msleep(50);
		}
		rx_cnt = (cur_drx > cur_crx) ? (cur_drx - 1 - cur_crx) :
					       (cur_drx - 1 - cur_crx + max_cnt);
		cur_opq = MTK_FE_WDMA_OQ(i);
		cur_fsm = mtk_r32(eth, MTK_FE_CDM_FSM(i)) &
			(MTK_CDM_FS_FSM_MASK | MTK_CDM_FS_PARSER_FSM_MASK);
		/* drx and crx remain unchanged && rx_cnt is not zero && wifi is not busy */
		if (drx_unchanged && crx_unchanged && (rx_cnt > 0) &&
		    mtk_check_wifi_busy && !mtk_check_wifi_busy(i))
			connsys_busy = true;
		/* ring is empty && wdma busy && pse_opq is not empty && fs_fsm busy*/
		else if (rx_cnt == 0 && rx_busy &&
			(cur_opq != 0 && cur_opq == wdma_rx->pre_opq[i]) &&
			(cur_fsm != 0 && cur_fsm == wdma_rx->pre_fsm[i]))
			netsys_busy = true;

		if (connsys_busy || netsys_busy) {
			if (connsys_busy)
				wdma_rx->hang_count_connsys[i]++;
			else
				wdma_rx->hang_count_netsys[i]++;

			if (wdma_rx->hang_count_connsys[i] >= 5 ||
			    wdma_rx->hang_count_netsys[i] >= 5) {
				pr_info("WDMA %d Rx Info (%s)\n", i,
					connsys_busy ? "CONNSYS busy" : "NETSYS busy");
				pr_info("hang count = %d",
					connsys_busy ? wdma_rx->hang_count_connsys[i] :
						       wdma_rx->hang_count_netsys[i]);
				pr_info("prev_drx = 0x%x	| cur_drx = 0x%x\n",
					wdma_rx->pre_drx[i], cur_drx);
				pr_info("prev_crx = 0x%x	| cur_crx = 0x%x\n",
					wdma_rx->pre_crx[i], cur_crx);
				pr_info("rx cnt = %d\n", rx_cnt);
				pr_info("WDMA_CRX_PTR = 0x%x\n",
					mtk_r32(eth, MTK_WDMA_CRX_PTR(i)));
				pr_info("WDMA_DRX_PTR = 0x%x\n",
					mtk_r32(eth, MTK_WDMA_DRX_PTR(i)));
				pr_info("WDMA_GLO_CFG = 0x%x\n",
					mtk_r32(eth, MTK_WDMA_GLO_CFG(i)));
				pr_info("PSE_OQ_STA = 0x%x\n", MTK_FE_WDMA_OQ(i));
				pr_info("PSE_CDM_FSM = 0x%x\n", mtk_r32(eth, MTK_FE_CDM_FSM(i)));
				pr_info("==============================\n");
				err_flag = 1;
			}
		} else {
			wdma_rx->hang_count_connsys[i] = 0;
			wdma_rx->hang_count_netsys[i] = 0;
		}

		wdma_rx->pre_crx[i] = cur_crx;
		wdma_rx->pre_drx[i] = cur_drx;
		wdma_rx->pre_opq[i] = cur_opq;
		wdma_rx->pre_fsm[i] = cur_fsm;
	}

	if (err_flag)
		return MTK_FE_START_RESET;
	else
		return 0;
}

u32 mtk_monitor_qdma_tx(struct mtk_eth *eth)
{
	struct qdma_tx_monitor *qdma_tx = &eth->reset.qdma_monitor.tx;
	bool pse_fc, qfsm_hang, qfwd_hang;
	u32 err_flag = 0;

	pse_fc = !!(mtk_r32(eth, MTK_FE_INT_STATUS) & BIT(PSE_QDMA_TX_PORT));
	if (pse_fc)
		mtk_w32(eth, BIT(PSE_QDMA_TX_PORT), MTK_FE_INT_STATUS);

	qfsm_hang = !!(mtk_r32(eth, MTK_QDMA_FSM) & 0xF00);
	qfwd_hang = !mtk_r32(eth, MTK_QDMA_FWD_CNT);

	if (!pse_fc && qfsm_hang && qfwd_hang) {
		qdma_tx->hang_count++;
		if (qdma_tx->hang_count >= 5) {
			pr_info("QDMA Tx Info\n");
			pr_info("hang count = %d\n", qdma_tx->hang_count);
			pr_info("qfsm_hang = %d\n", qfsm_hang);
			pr_info("qfwd_hang = %d\n", qfwd_hang);
			pr_info("-- -- -- -- -- -- --\n");
			pr_info("MTK_QDMA_FSM = 0x%x\n",
				mtk_r32(eth, MTK_QDMA_FSM));
			pr_info("MTK_QDMA_FWD_CNT = 0x%x\n",
				mtk_r32(eth, MTK_QDMA_FWD_CNT));
			pr_info("MTK_QDMA_FQ_CNT = 0x%x\n",
				mtk_r32(eth, MTK_QDMA_FQ_CNT));
			pr_info("==============================\n");
			err_flag = 1;
		}
	} else
		qdma_tx->hang_count = 0;

	qdma_tx->pse_fc = pse_fc;

	if (err_flag)
		return MTK_FE_STOP_TRAFFIC;
	else
		return 0;
}

u32 mtk_monitor_qdma_rx(struct mtk_eth *eth)
{
	struct qdma_rx_monitor *qdma_rx = &eth->reset.qdma_monitor.rx;
	u32 cur_fq_head, cur_fq_tail;
	u32 err_flag = 0;
	bool qrx_fsm;

	qrx_fsm = (mtk_r32(eth, MTK_QDMA_FSM) & 0x1F) == 0x9;
	cur_fq_head = mtk_r32(eth, MTK_QDMA_FQ_HEAD);
	cur_fq_tail = mtk_r32(eth, MTK_QDMA_FQ_TAIL);

	if (qrx_fsm && (cur_fq_head == qdma_rx->pre_fq_head) &&
		       (cur_fq_tail == qdma_rx->pre_fq_tail)) {
		qdma_rx->hang_count++;
		if (qdma_rx->hang_count >= 5) {
			pr_info("QDMA Rx Info\n");
			pr_info("hang count = %d\n", qdma_rx->hang_count);
			pr_info("MTK_QDMA_FSM = 0x%x\n",
				mtk_r32(eth, MTK_QDMA_FSM));
			pr_info("FQ_HEAD = 0x%x\n",
				mtk_r32(eth, MTK_QDMA_FQ_HEAD));
			pr_info("FQ_TAIL = 0x%x\n",
				mtk_r32(eth, MTK_QDMA_FQ_TAIL));
			err_flag = 1;
		}
	} else
		qdma_rx->hang_count = 0;

	qdma_rx->pre_fq_head = cur_fq_head;
	qdma_rx->pre_fq_tail = cur_fq_tail;

	if (err_flag)
		return MTK_FE_STOP_TRAFFIC;
	else
		return 0;
}


u32 mtk_monitor_adma_rx(struct mtk_eth *eth)
{
	struct adma_rx_monitor *adma_rx = &eth->reset.adma_monitor.rx;
	u32 i, cur_opq, cur_fsm, cur_drx[4], err_flag = 0;
	bool drx_hang = true;

	cur_opq = (mtk_r32(eth, MTK_PSE_OQ_STA(0)) & 0xFFF);
	cur_fsm = (mtk_r32(eth, MTK_FE_CDM1_FSM) & 0xFFFF0000);
	cur_drx[0] = mtk_r32(eth, MTK_ADMA_DRX_PTR);
	if (cur_drx[0] != adma_rx->pre_drx[0])
		drx_hang = false;

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_RSS)) {
		for (i = 1; i < eth->soc->rss_num; i++) {
			cur_drx[i] = mtk_r32(eth, MTK_PRX_DRX_IDX_CFG(i));
			if (cur_drx[i] != adma_rx->pre_drx[i])
				drx_hang = false;
		}
	}

	/* drx remain unchanged && output queue is not zero && fs_fsm busy */
	if (drx_hang && (cur_opq != 0 && cur_opq == adma_rx->pre_opq) &&
	    (cur_fsm != 0 && cur_fsm == adma_rx->pre_fsm)) {
		adma_rx->hang_count++;
		if (adma_rx->hang_count >= 5) {
			pr_info("ADMA Rx Info\n");
			pr_info("hang count = %d\n", adma_rx->hang_count);
			pr_info("CDM1_FSM = 0x%x\n",
				mtk_r32(eth, MTK_FE_CDM1_FSM));
			pr_info("MTK_PSE_OQ_STA1 = 0x%x\n",
				mtk_r32(eth, MTK_PSE_OQ_STA(0)));
			pr_info("MTK_ADMA_RX_DBG0 = 0x%x\n",
				mtk_r32(eth, MTK_ADMA_RX_DBG0));
			pr_info("MTK_ADMA_RX_DBG1 = 0x%x\n",
				mtk_r32(eth, MTK_ADMA_RX_DBG1));
			pr_info("MTK_ADMA_CRX_PTR = 0x%x\n",
				mtk_r32(eth, MTK_ADMA_CRX_PTR));
			pr_info("MTK_ADMA_DRX_PTR = 0x%x\n",
				mtk_r32(eth, MTK_ADMA_DRX_PTR));
			if (MTK_HAS_CAPS(eth->soc->caps, MTK_RSS)) {
				for (i = 1; i < eth->soc->rss_num; i++) {
					pr_info("MTK_ADMA_CRX_PTR%d = 0x%x\n", i,
						mtk_r32(eth, MTK_PRX_CRX_IDX_CFG(i)));
					pr_info("MTK_ADMA_DRX_PTR%d = 0x%x\n", i,
						mtk_r32(eth, MTK_PRX_DRX_IDX_CFG(i)));
				}
			}
			pr_info("==============================\n");
			err_flag = 1;
		}
	} else
		adma_rx->hang_count = 0;

	adma_rx->pre_opq = cur_opq;
	adma_rx->pre_fsm = cur_fsm;
	adma_rx->pre_drx[0] = cur_drx[0];
	if (MTK_HAS_CAPS(eth->soc->caps, MTK_RSS)) {
		for (i = 1; i < eth->soc->rss_num; i++)
			adma_rx->pre_drx[i] = cur_drx[i];
	}

	if (err_flag)
		return MTK_FE_STOP_TRAFFIC;
	else
		return 0;
}

u32 mtk_monitor_tdma_tx(struct mtk_eth *eth)
{
	struct tdma_tx_monitor *tdma_tx = &eth->reset.tdma_monitor.tx;
	u32 cur_ipq10, cur_fsm, tx_busy, err_flag = 0;
	u8 port = 10;

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		cur_ipq10 = mtk_r32(eth, MTK_PSE_IQ_STA(port / 2)) & 0xFFF;
		cur_fsm = mtk_r32(eth, MTK_FE_CDM6_FSM) & 0x1FFF;
		tx_busy = (mtk_r32(eth, MTK_TDMA_GLO_CFG) & 0x2) != 0;

		if (cur_ipq10 && cur_fsm && tx_busy &&
		    (cur_fsm == tdma_tx->pre_fsm) &&
		    (cur_ipq10 == tdma_tx->pre_ipq10)) {
			tdma_tx->hang_count++;
			if (tdma_tx->hang_count >= 5) {
				pr_info("TDMA Tx Info\n");
				pr_info("hang count = %d\n", tdma_tx->hang_count);
				pr_info("CDM6_FSM = 0x%x, PRE_CDM6_FSM = 0x%x\n",
					mtk_r32(eth, MTK_FE_CDM6_FSM), tdma_tx->pre_fsm);
				pr_info("PSE_IQ_P10 = 0x%x, PRE_PSE_IQ_P10 = 0x%x\n",
					mtk_r32(eth, MTK_PSE_IQ_STA(port / 2)), tdma_tx->pre_ipq10);
				pr_info("DMA_CFG = 0x%x\n",
					mtk_r32(eth, MTK_TDMA_GLO_CFG));
				pr_info("==============================\n");
				err_flag = 1;
			}
		} else
			tdma_tx->hang_count = 0;

		tdma_tx->pre_fsm = cur_fsm;
		tdma_tx->pre_ipq10 = cur_ipq10;
	}

	if (err_flag)
		return MTK_FE_STOP_TRAFFIC;
	else
		return 0;
}

u32 mtk_monitor_tdma_rx(struct mtk_eth *eth)
{
	struct tdma_rx_monitor *tdma_rx = &eth->reset.tdma_monitor.rx;
	u32 i, cur_rx_didx = 0, cur_opq10, cur_fsm, rx_busy, err_flag = 0;
	u8 port = 10;

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		for (i = 0, cur_rx_didx = 0; i < 4; i++)
			cur_rx_didx |= (mtk_r32(eth, MTK_TDMA_RX_DRX_IDX(i)) & 0xFF) << (8 * i);
		cur_opq10 = mtk_r32(eth, MTK_PSE_OQ_STA(port / 2)) & 0xFFF;
		cur_fsm = mtk_r32(eth, MTK_FE_CDM6_FSM) & 0xFFF0000;
		rx_busy = (mtk_r32(eth, MTK_TDMA_GLO_CFG) & 0x8) != 0;

		if (cur_opq10 && cur_fsm && rx_busy &&
		    (cur_fsm == tdma_rx->pre_fsm) &&
		    (cur_opq10 == tdma_rx->pre_opq10) &&
		    (cur_rx_didx == tdma_rx->pre_rx_didx)) {
			tdma_rx->hang_count++;
			if (tdma_rx->hang_count >= 5) {
				pr_info("TDMA Rx Info\n");
				pr_info("hang count = %d\n", tdma_rx->hang_count);
				pr_info("CDM6_FSM = 0x%x, PRE_CDM6_FSM = 0x%x\n",
					mtk_r32(eth, MTK_FE_CDM6_FSM), tdma_rx->pre_fsm);
				pr_info("PSE_OQ_P10 = 0x%x, PRE_PSE_OQ_P10 = 0x%x\n",
					mtk_r32(eth, MTK_PSE_OQ_STA(port / 2)), tdma_rx->pre_opq10);
				pr_info("DMA_CFG = 0x%x\n",
					mtk_r32(eth, MTK_TDMA_GLO_CFG));
				for (i = 0; i < 4; i++)
					pr_info("DMA_RX_DRX_IDX_%d = 0x%x, PRE_DMA_RX_DRX_IDX_%d = 0x%x\n",
						i, mtk_r32(eth, MTK_TDMA_RX_DRX_IDX(i)),
						i, (tdma_rx->pre_rx_didx >> (8 * i)) & 0xFF);
				pr_info("==============================\n");
				err_flag = 1;
			}
		} else
			tdma_rx->hang_count = 0;

		tdma_rx->pre_fsm = cur_fsm;
		tdma_rx->pre_opq10 = cur_opq10;
		tdma_rx->pre_rx_didx = cur_rx_didx;
	}

	if (err_flag)
		return MTK_FE_STOP_TRAFFIC;
	else
		return 0;
}

u32 mtk_monitor_gdm_rx(struct mtk_eth *eth)
{
	struct gdm_rx_monitor *gdm_rx = &eth->reset.gdm_monitor.rx;
	struct mtk_hw_stats *hw_stats;
	u32 i, cur_rx_cnt, cur_fsm_gdm, cur_ipq_gdm, err_flag = 0;
	bool gmac_rx;

	for (i = 0; i < MTK_MAX_DEVS; i++) {
		if (!eth->mac[i] || !netif_running(eth->netdev[i]))
			continue;

		/* update GDM counter to the hw_stats */
		if (spin_trylock(&eth->mac[i]->hw_stats->stats_lock)) {
			mtk_stats_update_mac(eth->mac[i]);
			spin_unlock(&eth->mac[i]->hw_stats->stats_lock);
		}

		hw_stats = eth->mac[i]->hw_stats;
		cur_rx_cnt = hw_stats->rx_packets;

		if (eth->mac[i]->type == MTK_GDM_TYPE) {
			gmac_rx = (mtk_r32(eth, MTK_MAC_FSM(i)) & 0xFF0000) != 0x10000;
			if (gmac_rx && (cur_rx_cnt == gdm_rx->pre_rx_cnt[i])) {
				gdm_rx->hang_count_gmac[i]++;
				if (gdm_rx->hang_count_gmac[i] >= 5) {
					pr_info("GMAC%d Rx Info\n", i + 1);
					pr_info("hang count = %d\n",
						gdm_rx->hang_count_gmac[i]);
					pr_info("GMAC_FSM = 0x%x\n",
						mtk_r32(eth, MTK_MAC_FSM(i)));
					err_flag = 1;
				}
			} else
				gdm_rx->hang_count_gmac[i] = 0;
		} else if (eth->mac[i]->type == MTK_XGDM_TYPE && eth->mac[i]->id != MTK_GMAC1_ID) {
			cur_ipq_gdm = MTK_FE_GDM_IQ(i);
			cur_fsm_gdm = mtk_r32(eth, MTK_FE_GDM_FSM(i)) & 0xFF;
			if (((cur_fsm_gdm == gdm_rx->pre_fsm_gdm[i] && cur_fsm_gdm == 0x23) ||
			     (cur_fsm_gdm == gdm_rx->pre_fsm_gdm[i] && cur_fsm_gdm == 0x24)) &&
			    (cur_ipq_gdm == gdm_rx->pre_ipq_gdm[i] && cur_ipq_gdm != 0x00) &&
			    (cur_rx_cnt == gdm_rx->pre_rx_cnt[i])) {
				gdm_rx->hang_count_gdm[i]++;
				if (gdm_rx->hang_count_gdm[i] >= 5) {
					pr_info("GDM%d Rx Info\n", i + 1);
					pr_info("hang count = %d\n",
						gdm_rx->hang_count_gdm[i]);
					pr_info("GDM_FSM = 0x%x\n",
						mtk_r32(eth, MTK_FE_GDM_FSM(i)));
					pr_info("==============================\n");
					err_flag = 1;
				}
			} else
				gdm_rx->hang_count_gdm[i] = 0;

			gdm_rx->pre_fsm_gdm[i] = cur_fsm_gdm;
			gdm_rx->pre_ipq_gdm[i] = cur_ipq_gdm;
		}

		gdm_rx->pre_rx_cnt[i] = cur_rx_cnt;
	}

	if (err_flag) {
		if (atomic_read(&reset_lock) == 0)
			eth->reset.rstctrl_eth = true;

		return MTK_FE_STOP_TRAFFIC;
	} else
		return 0;
}

u32 mtk_monitor_gdm_tx(struct mtk_eth *eth)
{
	struct gdm_tx_monitor *gdm_tx = &eth->reset.gdm_monitor.tx;
	struct mtk_hw_stats *hw_stats;
	u32 i, cur_tx_cnt, cur_rxfc_cnt, cur_fsm_gdm, cur_opq_gdm, err_flag = 0;
	bool gmac_tx;

	for (i = 0; i < MTK_MAX_DEVS; i++) {
		if (!eth->mac[i] || !netif_running(eth->netdev[i]))
			continue;

		/* update GDM counter to the hw_stats */
		if (spin_trylock(&eth->mac[i]->hw_stats->stats_lock)) {
			mtk_stats_update_mac(eth->mac[i]);
			spin_unlock(&eth->mac[i]->hw_stats->stats_lock);
		}

		hw_stats = eth->mac[i]->hw_stats;
		cur_tx_cnt = hw_stats->tx_packets;
		cur_rxfc_cnt = hw_stats->rx_flow_control_packets;
		cur_opq_gdm = MTK_FE_GDM_OQ(i);

		if (eth->mac[i]->type == MTK_XGDM_TYPE)
			goto skip_gmac;

		gmac_tx = (mtk_r32(eth, MTK_MAC_FSM(i)) & 0xFF000000) != 0x1000000;
		if (gmac_tx && (cur_tx_cnt == gdm_tx->pre_tx_cnt[i]) &&
		    (cur_opq_gdm > 0)) {
			gdm_tx->hang_count_gmac[i]++;
			if (gdm_tx->hang_count_gmac[i] >= 5) {
				pr_info("GMAC%d Tx Info\n", i + 1);
				pr_info("hang count = %d\n",
					gdm_tx->hang_count_gmac[i]);
				pr_info("GMAC_FSM = 0x%x\n",
					mtk_r32(eth, MTK_MAC_FSM(i)));
				err_flag = 1;
			}
		} else
			gdm_tx->hang_count_gmac[i] = 0;

skip_gmac:
		cur_fsm_gdm = mtk_r32(eth, MTK_FE_GDM_FSM(i)) & 0x1FFF0000;
		if ((cur_fsm_gdm == gdm_tx->pre_fsm_gdm[i]) && (cur_fsm_gdm == 0x10330000) &&
		    (cur_opq_gdm == gdm_tx->pre_opq_gdm[i]) && (cur_opq_gdm > 0) &&
		    (cur_rxfc_cnt == gdm_tx->pre_rxfc_cnt[i]) &&
		    (cur_tx_cnt == gdm_tx->pre_tx_cnt[i])) {
			gdm_tx->hang_count_gdm[i]++;
			if (gdm_tx->hang_count_gdm[i] >= 5) {
				pr_info("GDM%d Tx Info\n", i + 1);
				pr_info("hang count = %d\n",
					gdm_tx->hang_count_gdm[i]);
				pr_info("GDM_FSM = 0x%x\n",
					mtk_r32(eth, MTK_FE_GDM_FSM(i)));
				err_flag = 1;
			}
		} else
			gdm_tx->hang_count_gdm[i] = 0;

		gdm_tx->pre_tx_cnt[i] = cur_tx_cnt;
		gdm_tx->pre_rxfc_cnt[i] = cur_rxfc_cnt;
		gdm_tx->pre_fsm_gdm[i] = cur_fsm_gdm;
		gdm_tx->pre_opq_gdm[i] = cur_opq_gdm;
	}

	if (err_flag) {
		if (atomic_read(&reset_lock) == 0)
			eth->reset.rstctrl_eth = true;

		return MTK_FE_STOP_TRAFFIC;
	} else
		return 0;
}

static const mtk_monitor_xdma_func mtk_reset_monitor_func[] = {
	[0] = mtk_monitor_wdma_tx,
	[1] = mtk_monitor_wdma_rx,
	[2] = mtk_monitor_qdma_tx,
	[3] = mtk_monitor_qdma_rx,
	[4] = mtk_monitor_adma_rx,
	[5] = mtk_monitor_tdma_tx,
	[6] = mtk_monitor_tdma_rx,
	[7] = mtk_monitor_gdm_tx,
	[8] = mtk_monitor_gdm_rx,
};

void mtk_hw_reset_monitor(struct mtk_eth *eth)
{
	u32 i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mtk_reset_monitor_func); i++) {
		ret = (*mtk_reset_monitor_func[i]) (eth);
		if ((ret == MTK_FE_START_RESET) ||
		    (ret == MTK_FE_STOP_TRAFFIC)) {
			if ((atomic_read(&reset_lock) == 0) &&
			    (atomic_read(&force) == 1)) {
				eth->reset.event = ret;
				schedule_work(&eth->pending_work);
			}
			break;
		}
	}
}

void mtk_save_qdma_cfg(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_QDMA_TX_NUM; i++) {
		mtk_m32(eth, MTK_QTX_CFG_PAGE, (i / MTK_QTX_PER_PAGE),
			MTK_QDMA_PAGE);

		mtk_qdma_cfg_backup.qtx_cfg[i] =
			mtk_r32(eth, MTK_QTX_CFG(i % MTK_QTX_PER_PAGE));
		mtk_qdma_cfg_backup.qtx_sch[i] =
			mtk_r32(eth, MTK_QTX_SCH(i % MTK_QTX_PER_PAGE));
	}
	mtk_m32(eth, MTK_QTX_CFG_PAGE, 0, MTK_QDMA_PAGE);

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V2) ||
	    MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		mtk_qdma_cfg_backup.tx_sch[0] =
			mtk_r32(eth, MTK_QDMA_TX_4SCH_BASE(0));
		mtk_qdma_cfg_backup.tx_sch[1] =
			mtk_r32(eth, MTK_QDMA_TX_4SCH_BASE(2));
	} else
		mtk_qdma_cfg_backup.tx_sch[0] =
			mtk_r32(eth, MTK_QDMA_TX_2SCH_BASE);
}

void mtk_restore_qdma_cfg(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_QDMA_TX_NUM; i++) {
		mtk_m32(eth, MTK_QTX_CFG_PAGE, (i / MTK_QTX_PER_PAGE),
			MTK_QDMA_PAGE);

		mtk_w32(eth, mtk_qdma_cfg_backup.qtx_cfg[i],
			MTK_QTX_CFG(i % MTK_QTX_PER_PAGE));
		mtk_w32(eth, mtk_qdma_cfg_backup.qtx_sch[i],
			MTK_QTX_SCH(i % MTK_QTX_PER_PAGE));
	}
	mtk_m32(eth, MTK_QTX_CFG_PAGE, 0, MTK_QDMA_PAGE);

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V2) ||
	    MTK_HAS_CAPS(eth->soc->caps, MTK_NETSYS_V3)) {
		mtk_w32(eth, mtk_qdma_cfg_backup.tx_sch[0],
			MTK_QDMA_TX_4SCH_BASE(0));
		mtk_w32(eth, mtk_qdma_cfg_backup.tx_sch[1],
			MTK_QDMA_TX_4SCH_BASE(2));
	} else
		mtk_w32(eth, mtk_qdma_cfg_backup.tx_sch[0],
			MTK_QDMA_TX_2SCH_BASE);
}

void mtk_pse_set_port_link(struct mtk_eth *eth, u32 port, bool enable)
{
	u32 val;

	val = mtk_r32(eth, MTK_FE_GLO_CFG(port));
	if (enable)
		val &= ~MTK_FE_LINK_DOWN_P(port);
	else
		val |= MTK_FE_LINK_DOWN_P(port);
	mtk_w32(eth, val, MTK_FE_GLO_CFG(port));
}

void mtk_prepare_reset_ppe(struct mtk_eth *eth, u32 ppe_id)
{
	u32 i = 0, poll_time = 5000, val;

	/* Disable KA */
	mtk_m32(eth, MTK_PPE_KA_CFG_MASK, 0, MTK_PPE_TB_CFG(ppe_id));
	mtk_m32(eth, MTK_PPE_NTU_KA_MASK, 0, MTK_PPE_BIND_LMT_1(ppe_id));
	mtk_w32(eth, 0, MTK_PPE_KA(ppe_id));
	mdelay(10);

	/* Set KA timer to maximum */
	mtk_m32(eth, MTK_PPE_NTU_KA_MASK, (0xFF << 16), MTK_PPE_BIND_LMT_1(ppe_id));
	mtk_w32(eth, 0xFFFFFFFF, MTK_PPE_KA(ppe_id));

	/* Set KA tick select */
	mtk_m32(eth, MTK_PPE_TICK_SEL_MASK, (0x1 << 24), MTK_PPE_TB_CFG(ppe_id));
	mtk_m32(eth, MTK_PPE_KA_CFG_MASK, (0x3 << 12), MTK_PPE_TB_CFG(ppe_id));
	mdelay(10);

	/* Disable scan mode */
	mtk_m32(eth, MTK_PPE_SCAN_MODE_MASK, 0, MTK_PPE_TB_CFG(ppe_id));
	mdelay(10);

	/* Check PPE idle */
	while (i++ < poll_time) {
		val = mtk_r32(eth, MTK_PPE_GLO_CFG(ppe_id));
		if (!(val & MTK_PPE_BUSY))
			break;
		mdelay(1);
	}

	if (i >= poll_time) {
		pr_warn("[%s] PPE%d keeps busy !\n", __func__, ppe_id);
		mtk_dump_reg(eth, "FE", 0x0, 0x500);
		mtk_dump_reg(eth, "PPE", PPE_BASE(ppe_id), 0x200);
	}
}

int mtk_eth_netdevice_event(struct notifier_block *n, unsigned long event, void *ptr)
{
	struct mtk_eth *eth = container_of(n, struct mtk_eth, netdevice_notifier);

	switch (event) {
	case MTK_TOPS_DUMP_DONE:
		complete(&wait_tops_done);
		break;
	case MTK_WIFI_RESET_DONE:
	case MTK_FE_STOP_TRAFFIC_DONE:
		pr_info("%s rcv done event:%lx\n", __func__, event);
		mtk_rest_cnt--;
		if (!mtk_rest_cnt) {
			complete(&wait_ser_done);
			mtk_rest_cnt = mtk_wifi_num;
		}
		break;
	case MTK_WIFI_CHIP_ONLINE:
		mtk_wifi_num++;
		mtk_rest_cnt = mtk_wifi_num;
		break;
	case MTK_WIFI_CHIP_OFFLINE:
		mtk_wifi_num--;
		mtk_rest_cnt = mtk_wifi_num;
		break;
	case MTK_FE_STOP_TRAFFIC_DONE_FAIL:
		mtk_stop_fail = true;
		eth->reset.event = MTK_FE_START_RESET;
		pr_info("%s rcv done event:%lx\n", __func__, event);
		complete(&wait_ser_done);
		mtk_rest_cnt = mtk_wifi_num;
		break;
	case MTK_FE_START_RESET_INIT:
		pr_info("%s rcv fe start reset init event:%lx\n", __func__, event);
		if (atomic_read(&reset_lock) == 0) {
			if (atomic_read(&force) == 0)
				atomic_set(&force, 2);
			eth->reset.event = MTK_FE_START_RESET;
			schedule_work(&eth->pending_work);
		}
		break;
	case MTK_WIFI_L1SER_DONE:
		pr_info("%s rcv wifi done ack :%lx\n", __func__, event);
		complete(&wait_ack_done);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}
