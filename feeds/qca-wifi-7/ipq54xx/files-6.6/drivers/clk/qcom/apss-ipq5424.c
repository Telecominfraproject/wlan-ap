// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,apss-ipq.h>
#include <dt-bindings/arm/qcom,ids.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"

#define CPU_SVS_CLK_RATE	816000000
#define CPU_NOM_CLK_RATE	1416000000
#define CPU_TURBO_CLK_RATE	1800000000
#define L3_SVS_CLK_RATE		816000000
#define L3_NOM_CLK_RATE		984000000
#define L3_TURBO_CLK_RATE	1272000000

enum {
	P_XO,
	P_GPLL0,
	P_APSS_PLL_EARLY,
	P_L3_PLL,
};

struct apss_clk {
	struct notifier_block cpu_clk_notifier;
	struct clk_hw *hw;
	struct device *dev;
	struct clk *l3_clk;
};

/*
 * IPQ5424 Huayra PLL offsets are different from the one mentioned in the
 * clk-alpha-pll.c, hence define the IPQ5424 offsets here
 */
static const u8 ipq5424_pll_offsets[][PLL_OFF_MAX_REGS] = {
	[CLK_ALPHA_PLL_TYPE_HUAYRA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_CONFIG_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL_U] = 0x14,
		[PLL_OFF_CONFIG_CTL_U1] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_TEST_CTL_U1] = 0x24,
		[PLL_OFF_STATUS] = 0x38,
	},
};

static struct clk_alpha_pll ipq5424_apss_pll = {
	.offset = 0x0,
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_HUAYRA],
	.flags = SUPPORTS_DYNAMIC_UPDATE,
	.clkr = {
		.enable_reg = 0x0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "apss_pll",
			.parent_data = &(const struct clk_parent_data) {
				.fw_name = "xo",
			},
			.parent_names = (const char *[]){ "xo"},
			.num_parents = 1,
			.ops = &clk_alpha_pll_huayra_ops,
		},
	},
};

static const struct clk_parent_data parents_apss_silver_clk_src[] = {
	{ .fw_name = "xo" },
	{ .fw_name = "gpll0" },
	{ .hw = &ipq5424_apss_pll.clkr.hw },
};

static const struct parent_map parents_apss_silver_clk_src_map[] = {
	{ P_XO, 0 },
	{ P_GPLL0, 4 },
	{ P_APSS_PLL_EARLY, 5 },
};

static const struct freq_tbl ftbl_apss_clk_src[] = {
	F(CPU_SVS_CLK_RATE, P_APSS_PLL_EARLY, 1, 0, 0),
	F(CPU_NOM_CLK_RATE, P_APSS_PLL_EARLY, 1, 0, 0),
	F(CPU_TURBO_CLK_RATE, P_APSS_PLL_EARLY, 1, 0, 0),
	{ }
};

static struct clk_rcg2 apss_silver_clk_src = {
	.cmd_rcgr = 0x0080,
	.freq_tbl = ftbl_apss_clk_src,
	.hid_width = 5,
	.parent_map = parents_apss_silver_clk_src_map,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "apss_silver_clk_src",
		.parent_data = parents_apss_silver_clk_src,
		.num_parents = ARRAY_SIZE(parents_apss_silver_clk_src),
		.ops = &clk_rcg2_ops,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_branch apss_silver_core_clk = {
	.halt_reg = 0x008c,
	.clkr = {
		.enable_reg = 0x008c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "apss_silver_core_clk",
			.parent_hws = (const struct clk_hw *[]){
				&apss_silver_clk_src.clkr.hw },
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_alpha_pll ipq5424_l3_pll = {
	.offset = 0x10000,
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_HUAYRA],
	.flags = SUPPORTS_DYNAMIC_UPDATE,
	.clkr = {
		.enable_reg = 0x0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "l3_pll",
			.parent_data = &(const struct clk_parent_data) {
				.fw_name = "xo",
			},
			.parent_names = (const char *[]){ "xo"},
			.num_parents = 1,
			.ops = &clk_alpha_pll_huayra_ops,
		},
	},
};

static const struct clk_parent_data parents_l3_clk_src[] = {
	{ .fw_name = "xo" },
	{ .fw_name = "gpll0" },
	{ .hw = &ipq5424_l3_pll.clkr.hw },
};

static const struct parent_map parents_l3_clk_src_map[] = {
	{ P_XO, 0 },
	{ P_GPLL0, 4 },
	{ P_L3_PLL, 5 },
};

static const struct freq_tbl ftbl_l3_clk_src[] = {
	F(L3_SVS_CLK_RATE, P_L3_PLL, 1, 0, 0),
	F(L3_NOM_CLK_RATE, P_L3_PLL, 1, 0, 0),
	F(L3_TURBO_CLK_RATE, P_L3_PLL, 1, 0, 0),
	{ }
};

static struct clk_rcg2 l3_clk_src = {
	.cmd_rcgr = 0x10080,
	.freq_tbl = ftbl_l3_clk_src,
	.hid_width = 5,
	.parent_map = parents_l3_clk_src_map,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "l3_clk_src",
		.parent_data = parents_l3_clk_src,
		.num_parents = ARRAY_SIZE(parents_l3_clk_src),
		.ops = &clk_rcg2_ops,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_branch l3_core_clk = {
	.halt_reg = 0x1008c,
	.clkr = {
		.enable_reg = 0x1008c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "l3_clk",
			.parent_hws = (const struct clk_hw *[]){
				&l3_clk_src.clkr.hw },
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static const struct regmap_config apss_ipq5424_regmap_config = {
	.reg_bits       = 32,
	.reg_stride     = 4,
	.val_bits       = 32,
	.max_register   = 0x20000,
	.fast_io        = true,
};

static struct clk_regmap *apss_ipq5424_clks[] = {
	[APSS_PLL_EARLY] = &ipq5424_apss_pll.clkr,
	[APSS_SILVER_CLK_SRC] = &apss_silver_clk_src.clkr,
	[APSS_SILVER_CORE_CLK] = &apss_silver_core_clk.clkr,
	[L3_PLL] = &ipq5424_l3_pll.clkr,
	[L3_CLK_SRC] = &l3_clk_src.clkr,
	[L3_CORE_CLK] = &l3_core_clk.clkr,

};

static const struct qcom_cc_desc apss_ipq5424_desc = {
	.config = &apss_ipq5424_regmap_config,
	.clks = apss_ipq5424_clks,
	.num_clks = ARRAY_SIZE(apss_ipq5424_clks),
};

static const struct alpha_pll_config apss_pll_config = {
	.l = 0x3b,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x1,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config l3_pll_config = {
	.l = 0x29,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x1,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static unsigned long get_l3_clk_from_tbl(unsigned long rate)
{
	struct clk_rcg2 *l3_rcg2 = container_of(&l3_clk_src.clkr, struct clk_rcg2, clkr);
	u8 max_clk = sizeof(ftbl_apss_clk_src) / sizeof(struct freq_tbl);
	u8 loop;

	for (loop = 0; loop < max_clk; loop++)
		if (ftbl_apss_clk_src[loop].freq == rate)
			return l3_rcg2->freq_tbl[loop].freq;
	return 0;
}

static int cpu_clk_notifier_fn(struct notifier_block *nb, unsigned long action,
			       void *data)
{
	struct apss_clk *apss_ipq5424_cfg = container_of(nb, struct apss_clk, cpu_clk_notifier);
	struct clk_notifier_data *cnd = (struct clk_notifier_data *)data;
	struct device *dev = apss_ipq5424_cfg->dev;
	unsigned long rate = 0, l3_rate;
	int err = 0;

	dev_dbg(dev, "action:%ld old_rate:%ld new_rate:%ld\n", action,
		cnd->old_rate, cnd->new_rate);

	switch (action) {
	case PRE_RATE_CHANGE:
		if (cnd->old_rate < cnd->new_rate)
			rate = cnd->new_rate;
	break;
	case POST_RATE_CHANGE:
		if (cnd->old_rate > cnd->new_rate)
			rate = cnd->new_rate;
	break;
	};

	if (!rate)
		goto notif_ret;

	l3_rate = get_l3_clk_from_tbl(rate);
	if (!l3_rate) {
		dev_err(dev, "Failed to get l3 clock rate from l3_tbl\n");
		return NOTIFY_BAD;
	}

	err = clk_set_rate(apss_ipq5424_cfg->l3_clk, l3_rate);
	if (err) {
		dev_err(dev, "Failed to set l3 clock rate(%ld) err(%d)\n", l3_rate, err);
		return NOTIFY_BAD;
	}

notif_ret:
	return NOTIFY_OK;
}

static int apss_ipq5424_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct apss_clk *apss_ipq5424_cfg;
	struct regmap *regmap;
	void __iomem *base;
	int ret;

	apss_ipq5424_cfg = devm_kzalloc(&pdev->dev, sizeof(struct apss_clk), GFP_KERNEL);
	if (IS_ERR_OR_NULL(apss_ipq5424_cfg))
		return PTR_ERR(apss_ipq5424_cfg);

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(dev, base, &apss_ipq5424_regmap_config);
	if (!regmap)
		return PTR_ERR(regmap);

	clk_alpha_pll_configure(&ipq5424_l3_pll, regmap, &l3_pll_config);

	clk_alpha_pll_configure(&ipq5424_apss_pll, regmap, &apss_pll_config);

	ret = qcom_cc_really_probe(pdev, &apss_ipq5424_desc, regmap);
	if (ret)
		return ret;

	dev_dbg(&pdev->dev, "Registered APSS & L3 clock provider\n");

	apss_ipq5424_cfg->dev = dev;
	apss_ipq5424_cfg->hw = &apss_silver_clk_src.clkr.hw;
	apss_ipq5424_cfg->cpu_clk_notifier.notifier_call = cpu_clk_notifier_fn;

	apss_ipq5424_cfg->l3_clk = clk_hw_get_clk(&l3_core_clk.clkr.hw, "l3_clk");
	if (IS_ERR(apss_ipq5424_cfg->l3_clk)) {
		dev_err(&pdev->dev, "Failed to get L3 clk, %ld\n",
			PTR_ERR(apss_ipq5424_cfg->l3_clk));
		return PTR_ERR(apss_ipq5424_cfg->l3_clk);
	}

	ret = devm_clk_notifier_register(&pdev->dev, apss_ipq5424_cfg->hw->clk,
					 &apss_ipq5424_cfg->cpu_clk_notifier);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id apss_ipq5424_match_table[] = {
	{ .compatible = "qcom,apss-ipq5424-clk" },
	{ }
};
MODULE_DEVICE_TABLE(of, apss_ipq5424_match_table);

static struct platform_driver apss_ipq5424_driver = {
	.probe = apss_ipq5424_probe,
	.driver = {
		.name   = "apss-ipq5424-clk",
		.of_match_table = apss_ipq5424_match_table,
	},
};

module_platform_driver(apss_ipq5424_driver);

MODULE_DESCRIPTION("QCOM APSS IPQ5424 CLK Driver");
MODULE_LICENSE("GPL v2");
