// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/socinfo.h>
#include <linux/nvmem-consumer.h>
#include <linux/nvmem-provider.h>

/**
 * struct gpio_regulator_data - gpio regulator data structure
 * @regulator_name:	Regulator name which needs to be controlled
 * @reference_volt:	Reference voltage used to calculate fused voltage
 * @step_volt:		Step size of fused voltage ticks
 * @min_volt:		Voltage used for Fast parts
 * @max_volt:		Voltage used for Slow parts
 * @threshold_volt:	Voltage threshold steps needs to be compared with
 * 			fuse voltage from nvmem
 */

struct gpio_regulator_data {
	const char *regulator_name;
	int bit_len;
	int reference_volt;
	int step_volt;
	int min_volt;
	int max_volt;
	int threshold_volt;
};

static const struct gpio_regulator_data ipq9574_gpio_regulator_data[] = {
	{"apc", 6, 862500, 10000, 850000, 925000, 800000},
	{"cx", 5, 800000, 10000, 800000, 863000, 800000},
	{"mx", 5, 850000, 10000, 850000, 925000, 850000},
	{ },
};

static const struct gpio_regulator_data ipq9574_4state_gpio_regulator_data[] = {
	{"apc", 6, 1062500, 10000, 1004000, 1068000, 1002500},
	{"cx", 5, 850000, 10000, 850000, 910000, 850000},
	{ }
};

static struct of_device_id gpio_regulator_match_table[] = {
	{
		.compatible = "qcom,ipq9574-gpio-regulator",
		.data = &ipq9574_gpio_regulator_data
	},
	{
		.compatible = "qcom,ipq9574-4state-gpio-regulator",
		.data = &ipq9574_4state_gpio_regulator_data
	},
	{}
};

int gpio_convert_open_loop_voltage_fuse(int ref_volt, int step_volt, u8 fuse,
                                        int fuse_len)
{
	int sign, steps;

	sign = (fuse & (1 << (fuse_len - 1))) ? -1 : 1;
	steps = fuse & ((1 << (fuse_len - 1)) - 1);

	return ref_volt + sign * steps * step_volt;
}

static int gpio_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct gpio_regulator_data *reg_data;
	struct regulator *gpio_regulator;
	u16 volt_ticks;
	u8 cpr_fuse;
	int fused_volt;
	int volt_select;
	bool fix_volt_max = false;
	int ret = -EINVAL;;

	match = of_match_device(gpio_regulator_match_table, dev);
	if (!match)
		return -ENODEV;

	if (device_property_read_bool(dev, "skip-voltage-scaling-turboL1-sku-quirk")) {
		if(cpu_is_ipq9574() || cpu_is_ipq9570())
			fix_volt_max = true;
	}

	ret = nvmem_cell_read_u8(dev, "cpr", &cpr_fuse);
	if (ret < 0) {
		if(ret != -EPROBE_DEFER)
			dev_err(dev, "%s CPR fuse revision read failed, ret %d\n", "cpr", ret);
		return ret;
	}

	for(reg_data = (struct gpio_regulator_data *)match->data;
					reg_data->regulator_name; reg_data++)
	{
		ret = nvmem_cell_read_u16(dev, reg_data->regulator_name, &volt_ticks);
		if (ret < 0) {
			dev_err(dev, "%s fuse read failed, ret %d\n", reg_data->regulator_name, ret);
			continue;
		}

		fused_volt = gpio_convert_open_loop_voltage_fuse(reg_data->reference_volt,
					reg_data->step_volt,
					volt_ticks,
					reg_data->bit_len);

		gpio_regulator = devm_regulator_get(dev, reg_data->regulator_name);
		if (IS_ERR(gpio_regulator)) {
			ret = PTR_ERR(gpio_regulator);
			dev_err(dev, "%s regulator get failed, ret %d\n",reg_data->regulator_name, ret);
			continue;
		}

		if (!cpr_fuse || fix_volt_max)
			volt_select = reg_data->max_volt;
		else
			volt_select = (fused_volt > reg_data->threshold_volt) ?
						reg_data->max_volt : reg_data->min_volt;

		ret = regulator_set_voltage(gpio_regulator, volt_select, volt_select);
		if (ret < 0) {
			dev_err(dev, "%s regulator voltage %u set failed, ret %d",
					reg_data->regulator_name, volt_select, ret);
			continue;
		}
	}

	return ret;
}

static struct platform_driver gpio_regulator_driver = {
	.driver		= {
		.name		= "qcom,gpio-regulator",
		.of_match_table	= gpio_regulator_match_table,
	},
	.probe		= gpio_regulator_probe,
};

module_platform_driver(gpio_regulator_driver);

MODULE_DESCRIPTION("QTI GPIO regulator driver");
MODULE_LICENSE("GPL v2");
