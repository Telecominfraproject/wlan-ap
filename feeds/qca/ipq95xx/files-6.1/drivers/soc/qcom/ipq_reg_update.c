/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/qcom_scm.h>

#define MEM_NOC_XM_APP0_QOSGEN_MAINCTL_LOW 0x6088
#define MEM_NOC_XM_APP0_QOSGEN_REGUL0CTL_LOW 0x60C0
#define MEM_NOC_XM_APP0_QOSGEN_REGUL0BW_LOW 0x60C8
#define MAINCTL_LOW_VAL 0x70
#define REGUL0CTL_LOW_VAL 0x7703
#define REGUL0BW_LOW_VAL 0x3FF0FFF
/* UBI Control Offsets */
#define UBI_C1_GDS_CTRL_REQ 0x4
#define UBI_C2_GDS_CTRL_REQ 0x8
#define UBI_C3_GDS_CTRL_REQ 0xC
#define UBI32_CORE_GDS_COLLAPSE_EN_SW 0x1 << 28

static int reg_update_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *base;
	void __iomem *ubi_c0_gds;
	struct clk *nss_csr_clk;
	struct clk *nssnoc_nss_csr_clk;
	int ret;
	struct device_node *np = (&pdev->dev)->of_node;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "memnoc");
	if(res) {
		base = devm_ioremap_resource(&pdev->dev, res);
		if(IS_ERR(base))
			return PTR_ERR(base);
		writel(MAINCTL_LOW_VAL, base + MEM_NOC_XM_APP0_QOSGEN_MAINCTL_LOW);
		writel(REGUL0CTL_LOW_VAL, base + MEM_NOC_XM_APP0_QOSGEN_REGUL0CTL_LOW);
		writel(REGUL0BW_LOW_VAL, base + MEM_NOC_XM_APP0_QOSGEN_REGUL0BW_LOW);
	}
	if (!of_property_read_bool(np, "ubi_core_enable")) {
		/* Enabling NSS CSR clocks to access the UBI Power collapse registers */
		nss_csr_clk = devm_clk_get(&pdev->dev, "nss-csr-clk");
		if (IS_ERR(nss_csr_clk)) {
			ret = PTR_ERR(nss_csr_clk);
			pr_debug("Failed to get nss-csr-clk\n");
			goto err_out;
		}
		nssnoc_nss_csr_clk = devm_clk_get(&pdev->dev, "nss-nssnoc-csr-clk");
		if (IS_ERR(nssnoc_nss_csr_clk)) {
			ret = PTR_ERR(nssnoc_nss_csr_clk);
			pr_debug("Failed to get nss-nssnoc-csr-clk\n");
			goto err_out;
		}
		ret = clk_prepare_enable(nss_csr_clk);
		if(ret)
			goto err_out;
		ret = clk_prepare_enable(nssnoc_nss_csr_clk);
		if (ret)
			goto err_out;
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ubicore");
		if(res) {
			ubi_c0_gds = devm_ioremap_resource(&pdev->dev, res);
			if(IS_ERR(ubi_c0_gds)) {
				pr_debug("ubicore ioremap failed\n");
				goto err_out;
			}
			/* Power collapsing the 4 UBI32 Cores as it is not used in IPQ9574 except for AL02-C10 RDP */
			writel(readl(ubi_c0_gds) | UBI32_CORE_GDS_COLLAPSE_EN_SW, ubi_c0_gds);
			writel(readl(ubi_c0_gds + UBI_C1_GDS_CTRL_REQ) | UBI32_CORE_GDS_COLLAPSE_EN_SW, ubi_c0_gds + UBI_C1_GDS_CTRL_REQ);
			writel(readl(ubi_c0_gds + UBI_C2_GDS_CTRL_REQ) | UBI32_CORE_GDS_COLLAPSE_EN_SW, ubi_c0_gds + UBI_C2_GDS_CTRL_REQ);
			writel(readl(ubi_c0_gds + UBI_C3_GDS_CTRL_REQ) | UBI32_CORE_GDS_COLLAPSE_EN_SW, ubi_c0_gds + UBI_C3_GDS_CTRL_REQ);
			pr_info("UBI cores power collapsed successfully\n");
		}
	}
	else
		pr_info("Skipping UBI power collapse\n");
	return 0;

err_out:
	pr_err("Failed to power collapse UBI\n");
	return (ret);

}

static const struct of_device_id reg_update_dt_match[] = {
	{ .compatible = "ipq,reg-update", },
	{ },
};

MODULE_DEVICE_TABLE(of, reg_update_dt_match);

static struct platform_driver reg_update_driver = {
	.driver = {
		.name	= "reg_update",
		.owner	= THIS_MODULE,
		.of_match_table	= reg_update_dt_match,
	},
	.probe = reg_update_probe,
};

module_platform_driver(reg_update_driver);
