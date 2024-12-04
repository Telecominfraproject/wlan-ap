/* Copyright (c) 2015-2016, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#define NON_SECURE_WATCHDOG             0x1
#define AHB_TIMEOUT                     0x3
#define NOC_ERROR                       0x6
#define SYSTEM_RESET_OR_REBOOT          0x10
#define POWER_ON_RESET                  0x20
#define SECURE_WATCHDOG                 0x23
#define HLOS_PANIC                      0x47
#define VFSM_RESET                      0x68
#define TME_L_FATAL_ERROR               0x49
#define TME_L_WDT_BITE_FATAL_ERROR      0x69

#define RESET_REASON_MSG_MAX_LEN        100

static int restart_reason_logging(struct platform_device *pdev)
{
	unsigned int reset_reason;
	struct device_node *imem_np;
	void __iomem *imem_base;
	char reset_reason_msg[RESET_REASON_MSG_MAX_LEN];

	memset(reset_reason_msg, 0, sizeof(reset_reason_msg));
	imem_np = of_find_compatible_node(NULL, NULL,
			"qcom,msm-imem-restart-reason-buf-addr");
	if (!imem_np) {
		dev_err(&pdev->dev,
				"restart_reason_buf_addr imem DT node does not exist\n");
		return -ENODEV;
	}

	imem_base = of_iomap(imem_np, 0);
	if (!imem_base) {
		dev_err(&pdev->dev,
				"restart_reason_buf_addr imem offset mapping failed\n");
		return -ENOMEM;
	}
	memcpy_fromio(&reset_reason, imem_base, 4);
	iounmap(imem_base);
	switch(reset_reason) {
		case NON_SECURE_WATCHDOG:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "Non-Secure Watchdog ");
			break;
		case AHB_TIMEOUT:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "AHB Timeout ");
			break;
		case NOC_ERROR:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "NOC Error ");
			break;
		case SYSTEM_RESET_OR_REBOOT:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "System reset or reboot ");
			break;
		case POWER_ON_RESET:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "Power on Reset ");
			break;
		case SECURE_WATCHDOG:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "Secure Watchdog ");
			break;
		case HLOS_PANIC:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "HLOS Panic ");
			break;
		case VFSM_RESET:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "VFSM Reset ");
			break;
		case TME_L_FATAL_ERROR:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "TME-L Fatal Error ");
			break;
		case TME_L_WDT_BITE_FATAL_ERROR:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "TME-L WDT Bite occurred ");
			break;
	}

	dev_info(&pdev->dev, "reset_reason : %s[0x%X]\n", reset_reason_msg,
			reset_reason);
	return 0;
}

static const struct of_device_id ipq_debug_match_table[] = {
	{ .compatible = "qcom,ipq-debug",
	},
	{}
};
MODULE_DEVICE_TABLE(of, ipq_debug_match_table);

static int ipq_debug_probe(struct platform_device *pdev)
{
	struct device_node *np;
	int ret;
	unsigned int no_reset_reason = 0;

	np = of_node_get(pdev->dev.of_node);
	if (!np)
		return 0;

	no_reset_reason = of_property_read_bool(np, "no-reset-reason");
	if (!no_reset_reason) {
		ret = restart_reason_logging(pdev);
		if (ret < 0) {
			dev_err(&pdev->dev, "reset reason logging failed!\n");
		}
	}

	return 0;
}

static struct platform_driver ipq_debug_driver = {
	.probe      = ipq_debug_probe,
	.driver     = {
		.name = "qcom,ipq-debug",
		.of_match_table = ipq_debug_match_table,
	},
};

module_platform_driver(ipq_debug_driver);

MODULE_DESCRIPTION("QCOM IPQ DEBUG Driver");

