// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Lu Tang <Lu.Tang@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include <dt-bindings/clock/mediatek,mt7987-clk.h>

static const struct mtk_gate_regs ethdma_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x30,
	.sta_ofs = 0x30,
};

#define GATE_ETHDMA(_id, _name, _parent, _shift)                  \
	{                                                         \
		.id = _id, .name = _name, .parent_name = _parent, \
		.regs = &ethdma_cg_regs, .shift = _shift,         \
		.ops = &mtk_clk_gate_ops_no_setclr_inv,           \
	}

static const struct mtk_gate ethdma_clks[] = {
	GATE_ETHDMA(CK_ETHDMA_FE_EN, "ethdma_fe_en", "netsys_2x_sel", 6),
	GATE_ETHDMA(CK_ETHDMA_GP2_EN, "ethdma_gp2_en", "netsys_500m_sel", 7),
	GATE_ETHDMA(CK_ETHDMA_GP1_EN, "ethdma_gp1_en", "netsys_500m_sel", 8),
	GATE_ETHDMA(CK_ETHDMA_GP3_EN, "ethdma_gp3_en", "netsys_500m_sel", 10),
};

static const struct mtk_gate_regs sgmii_cg_regs = {
	.set_ofs = 0xe4,
	.clr_ofs = 0xe4,
	.sta_ofs = 0xe4,
};

#define GATE_SGMII(_id, _name, _parent, _shift)                   \
	{                                                         \
		.id = _id, .name = _name, .parent_name = _parent, \
		.regs = &sgmii_cg_regs, .shift = _shift,          \
		.ops = &mtk_clk_gate_ops_no_setclr_inv,           \
	}

static const struct mtk_gate sgmii0_clks[] = {
	GATE_SGMII(CK_SGM0_TX_EN, "sgm0_tx_en", "clkxtal", 2),
	GATE_SGMII(CK_SGM0_RX_EN, "sgm0_rx_en", "clkxtal", 3),
};

static const struct mtk_gate sgmii1_clks[] = {
	GATE_SGMII(CK_SGM1_TX_EN, "sgm1_tx_en", "clkxtal", 2),
	GATE_SGMII(CK_SGM1_RX_EN, "sgm1_rx_en", "clkxtal", 3),
};

static void __init mtk_sgmiisys_0_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_SGMII0_NR_CLK);

	mtk_clk_register_gates(node, sgmii0_clks, ARRAY_SIZE(sgmii0_clks),
			       clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
		       __func__, r);
}
CLK_OF_DECLARE(mtk_sgmiisys_0, "mediatek,mt7987-sgmiisys_0",
	       mtk_sgmiisys_0_init);

static void __init mtk_sgmiisys_1_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_SGMII1_NR_CLK);

	mtk_clk_register_gates(node, sgmii1_clks, ARRAY_SIZE(sgmii1_clks),
			       clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
		       __func__, r);
}
CLK_OF_DECLARE(mtk_sgmiisys_1, "mediatek,mt7987-sgmiisys_1",
	       mtk_sgmiisys_1_init);

static void __init mtk_ethdma_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_ETHDMA_NR_CLK);

	mtk_clk_register_gates(node, ethdma_clks, ARRAY_SIZE(ethdma_clks),
			       clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
		       __func__, r);
}
CLK_OF_DECLARE(mtk_ethdma, "mediatek,mt7987-ethsys", mtk_ethdma_init);
