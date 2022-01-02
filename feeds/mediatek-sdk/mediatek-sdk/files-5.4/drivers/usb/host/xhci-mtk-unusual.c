// SPDX-License-Identifier: GPL-2.0
/*
 * xHCI host controller toolkit driver
 *
 * Copyright (C) 2021  MediaTek Inc.
 *
 *  Author: Zhanyong Wang <zhanyong.wang@mediatek.com>
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <dt-bindings/phy/phy.h>
#include "xhci-mtk.h"
#include "xhci-mtk-test.h"
#include "xhci-mtk-unusual.h"


u32 binary_write_width1(u32 __iomem *addr, u32 shift, const char *buf)
{
	u32 val = 0;

	if (!strncmp(buf, STRNG_0_WIDTH_1, BIT_WIDTH_1))
		val = 0;
	else if (!strncmp(buf, STRNG_1_WIDTH_1, BIT_WIDTH_1))
		val = 1;
	else
		val = 0xFFFFFFFF;

	if (val <= 1)
		val = usb20hqa_write(addr, shift, MSK_WIDTH_1, val);

	return val;
}

u32 binary_write_width3(u32 __iomem *addr, u32 shift, const char *buf)
{
	u32 val = 0;

	if (!strncmp(buf, STRNG_0_WIDTH_3, BIT_WIDTH_3))
		val = 0;
	else if (!strncmp(buf, STRNG_1_WIDTH_3, BIT_WIDTH_3))
		val = 1;
	else if (!strncmp(buf, STRNG_2_WIDTH_3, BIT_WIDTH_3))
		val = 2;
	else if (!strncmp(buf, STRNG_3_WIDTH_3, BIT_WIDTH_3))
		val = 3;
	else if (!strncmp(buf, STRNG_4_WIDTH_3, BIT_WIDTH_3))
		val = 4;
	else if (!strncmp(buf, STRNG_5_WIDTH_3, BIT_WIDTH_3))
		val = 5;
	else if (!strncmp(buf, STRNG_6_WIDTH_3, BIT_WIDTH_3))
		val = 6;
	else if (!strncmp(buf, STRNG_7_WIDTH_3, BIT_WIDTH_3))
		val = 7;
	else
		val = 0xFFFFFFFF;

	if (val <= 7)
		val = usb20hqa_write(addr, shift, MSK_WIDTH_3, val);

	return val;
}

u32 binary_write_width4(u32 __iomem *addr, u32 shift, const char *buf)
{
	u32 val = 0;

	if (!strncmp(buf, STRNG_0_WIDTH_4, BIT_WIDTH_4))
		val = 0;
	else if (!strncmp(buf, STRNG_1_WIDTH_4, BIT_WIDTH_4))
		val = 1;
	else if (!strncmp(buf, STRNG_2_WIDTH_4, BIT_WIDTH_4))
		val = 2;
	else if (!strncmp(buf, STRNG_3_WIDTH_4, BIT_WIDTH_4))
		val = 3;
	else if (!strncmp(buf, STRNG_4_WIDTH_4, BIT_WIDTH_4))
		val = 4;
	else if (!strncmp(buf, STRNG_5_WIDTH_4, BIT_WIDTH_4))
		val = 5;
	else if (!strncmp(buf, STRNG_6_WIDTH_4, BIT_WIDTH_4))
		val = 6;
	else if (!strncmp(buf, STRNG_7_WIDTH_4, BIT_WIDTH_4))
		val = 7;
	else if (!strncmp(buf, STRNG_8_WIDTH_4, BIT_WIDTH_4))
		val = 8;
	else if (!strncmp(buf, STRNG_9_WIDTH_4, BIT_WIDTH_4))
		val = 9;
	else if (!strncmp(buf, STRNG_A_WIDTH_4, BIT_WIDTH_4))
		val = 10;
	else if (!strncmp(buf, STRNG_B_WIDTH_4, BIT_WIDTH_4))
		val = 11;
	else if (!strncmp(buf, STRNG_C_WIDTH_4, BIT_WIDTH_4))
		val = 12;
	else if (!strncmp(buf, STRNG_D_WIDTH_4, BIT_WIDTH_4))
		val = 13;
	else if (!strncmp(buf, STRNG_E_WIDTH_4, BIT_WIDTH_4))
		val = 14;
	else if (!strncmp(buf, STRNG_F_WIDTH_4, BIT_WIDTH_4))
		val = 15;
	else
		val = 0xFFFFFFFF;

	if (val <= 15)
		val = usb20hqa_write(addr, shift, MSK_WIDTH_4, val);

	return val;
}

u32 bin2str(u32 value, u32 width, char *buffer)
{
	int i, temp;

	temp = value;
	buffer[width] = '\0';
	for (i = (width - 1); i >= 0; i--) {
		buffer[i] = '0';
		if (value % 2)
			buffer[i] = '1';

		value /= 2;
	}

	return value;
}

int query_phy_addr(struct device_node *np, int *start, u32 *addr, u32 *length)
{
	int ret = -EPERM;
	struct of_phandle_args args;
	struct resource res;
	int numphys = 0;
	int index;

	if (start == NULL || addr == NULL || length == NULL)
		return -EINVAL;

	numphys = of_count_phandle_with_args(np, "phys", "#phy-cells");
	for ( index = *start; (numphys > 0) && index < numphys; index++) {
		ret = of_parse_phandle_with_args(np, "phys", "#phy-cells",
				index, &args);
		if (ret < 0)
			break;

		if (args.args[0] == PHY_TYPE_USB2) {
			ret = of_address_to_resource(args.np, 0, &res);
			if (ret < 0) {
				of_node_put(args.np);
				break;
			}

			*addr   = res.start;
			*length = (u32)resource_size(&res);
			*start  = index;
			if (!of_device_is_available(args.np))
				ret = -EACCES;

			of_node_put(args.np);
			break;
		}
	}

	ret = index < numphys ? ret : -EPERM;
	return ret;
}
