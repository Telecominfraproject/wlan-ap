// SPDX-License-Identifier: GPL-2.0
/*
 * Mediatek bus hang protect driver
 *
 * Copyright (C) 2022 MediaTek Inc.
 * Author: Sam Shih <sam.shih@mediatek.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/of_address.h>
#include <linux/io.h>

#define	HANG_FREE_PROT_INFRA_AO		0x0

/**
 * struct mediatek_bus_prot - struct representing mediatek
 * @regs: base address of PWM chip
 */
struct mediatek_bus_prot {
	void __iomem *regs;
};

struct bus_hang_prot_of_data {
	unsigned int offset;
};

static int mtk_bus_hang_prot_probe(struct platform_device *pdev)
{
	const struct bus_hang_prot_of_data *data;
	struct resource *res;
	void __iomem *regs;
	int ret = 0;

	data = of_device_get_match_data(&pdev->dev);
	if (!data)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(regs))
		return PTR_ERR(regs);

	writel(0x0, regs + data->offset);

	return 0;
}

static const struct bus_hang_prot_of_data infracfg_ao_bus_hang_prot_data = {
	.offset = HANG_FREE_PROT_INFRA_AO,
};

static const struct of_device_id bus_hang_prot_match[] = {
	{
		.compatible = "mediatek,infracfg_ao_bus_hang_prot",
		.data = &infracfg_ao_bus_hang_prot_data
	},
	{ }
};

static struct platform_driver mtk_bus_hang_prot_driver = {
	.probe = mtk_bus_hang_prot_probe,
	.driver = {
		.name = "mediatek,bus_hang_prot",
		.of_match_table = bus_hang_prot_match,
	},
};

static int __init mtk_bus_hang_prot_init(void)
{
	return platform_driver_register(&mtk_bus_hang_prot_driver);
}

arch_initcall(mtk_bus_hang_prot_init);
