/* Copyright (c) 2015-2016, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/notifier.h>
#include <linux/panic_notifier.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/remoteproc.h>
#include <soc/qcom/ipq-debug.h>

static struct restart_reason *g_reason;

int debug_log_reset_reason(unsigned int val)
{
	if (!g_reason)
		return -EOPNOTSUPP;

	if (val >= IPQ5424_RESET_MAX)
		return -EINVAL;

	memcpy_toio(g_reason->wr_addr, &val, sizeof(int));

	return 0;
}
EXPORT_SYMBOL_GPL(debug_log_reset_reason);

static int debug_panic_handler(struct notifier_block *nb, unsigned long action,
			       void *data)
{
	struct restart_reason *reason;
	int val = IPQ5424_HLOS_PANIC;
	int tmp;

	reason = container_of(nb, struct restart_reason, panic_blk);

	/* If the reason is IPQ5424_INTERNAL_Q6_CRASH, then the rproc recovery
	 * is not enabled, so treat it as INTERNAL_Q6_CRASH, not as HLOS_PANIC.
	 */
	memcpy_fromio(&tmp, reason->wr_addr, sizeof(int));
	if (tmp != IPQ5424_INTERNAL_Q6_CRASH)
		memcpy_toio(reason->wr_addr, &val, sizeof(int));

	if (!in_interrupt())
		iounmap(reason->wr_addr);

	return NOTIFY_DONE;
}

static int ipq_debug_atomic_ssr_handler(struct notifier_block *nb,
					unsigned long action, void *data)
{
	struct restart_reason *reason;
	int val = IPQ5424_INTERNAL_Q6_CRASH;

	reason = container_of(nb, struct restart_reason, atomic_ssr_blk);

	if (action == QCOM_SSR_NOTIFY_CRASH)
		memcpy_toio(reason->wr_addr, &val, sizeof(int));

	return NOTIFY_DONE;
}

static int ipq_debug_ssr_handler(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct restart_reason *reason;
	int val = 0;

	reason = container_of(nb, struct restart_reason, ssr_blk);

	if (action == QCOM_SSR_BEFORE_POWERUP)
		memcpy_toio(reason->wr_addr, &val, sizeof(int));

	return NOTIFY_DONE;
}

static int restart_reason_logging(unsigned int reason)
{
	char reset_reason_msg[RESET_REASON_MSG_MAX_LEN] = {};

	switch(reason) {
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

	pr_info("reset_reason : %s[0x%X]\n", reset_reason_msg, reason);
	return 0;
}

static int restart_reason_logging_ipq5424(unsigned int reason, unsigned int q6_reason)
{
	char reset_reason_msg[RESET_REASON_MSG_MAX_LEN] = {};

	switch(reason) {
		case IPQ5424_POWER_ON_RESET:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "Power on Reset");
			break;
		case IPQ5424_SYSTEM_RESET_OR_REBOOT:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "System reset or reboot");
			break;
		case IPQ5424_TME_L_SECURE_WATCHDOG:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "TME-L Secure Watchdog");
			break;
		case IPQ5424_SECURE_WATCHDOG:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "Secure Watchdog");
			break;
		case IPQ5424_NON_SECURE_WATCHDOG:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "Non-Secure Watchdog");
			break;
		case IPQ5424_HLOS_PANIC:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "HLOS Panic");
			break;
		case IPQ5424_EXTERNAL_WDT:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "External Watchdog");
			break;
		case IPQ5424_TME_L_FORCE_RESET:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "TME-L Force Reset");
			break;
		case IPQ5424_TSENS_HW_RESET:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "TSENS HW Reset");
			break;
		case IPQ5424_AHB_TIMEOUT:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "AHB Timeout");
			break;
		case IPQ5424_INTERNAL_Q6_CRASH:
			if (q6_reason != 0)
				scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
						"%s[0x%X]", "Internal Q6 Fatal error", q6_reason);
			else
				scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
						"%s", "Internal Q6 WDT error");
			break;
		case IPQ5424_TSENS_SW_RESET:
			scnprintf(reset_reason_msg, RESET_REASON_MSG_MAX_LEN,
					"%s", "TSENS SW Reset");
			break;
	}

	pr_info("reset_reason : %s[0x%X]\n", reset_reason_msg, reason);
	return 0;
}

static const struct of_device_id ipq_debug_match_table[] = {
	{ .compatible = "qcom,ipq-debug",
	},
	{ .compatible = "qcom,ipq-debug-ipq5424",
	},
	{}
};
MODULE_DEVICE_TABLE(of, ipq_debug_match_table);

void __iomem *ipq_debug_parse_address(struct device *dev,
				      const char *compatible)
{
	struct device_node *np;
	void __iomem *addr;

	np = of_find_compatible_node(NULL, NULL, compatible);
	if (!np) {
		dev_err(dev, "node %s doesn't exist\n", compatible);
		return ERR_PTR(-ENODEV);
	}

	addr = of_iomap(np, 0);
	of_node_put(np);
	if (!addr) {
		dev_err(dev, "iomap failed for compatible %s\n", compatible);
		return addr;
	}

	return addr;
}

static bool is_rproc_device_available(void)
{
	struct device_node *node;

	node = of_find_node_by_name(NULL, "remoteproc");
	if (!of_device_is_available(node))
		return false;

	of_node_put(node);
	return true;
}

static int ipq_debug_register_rproc_notifiers(struct platform_device *pdev,
					      struct restart_reason *reason)
{
	struct rproc *rproc;
	u32 rproc_node;
	int ret;

	if (!is_rproc_device_available())
		return 0;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,rproc", &rproc_node);
	if (ret) {
		atomic_notifier_chain_unregister(&panic_notifier_list,
						 &reason->panic_blk);
		return ret;
	}

	rproc = rproc_get_by_phandle(rproc_node);
	if (!rproc) {
		atomic_notifier_chain_unregister(&panic_notifier_list,
						 &reason->panic_blk);
		return -EPROBE_DEFER;
	}

	reason->atomic_ssr_blk.notifier_call = ipq_debug_atomic_ssr_handler;
	reason->atomic_cookie = qcom_register_ssr_atomic_notifier(rproc->name,
								  &reason->atomic_ssr_blk);
	if (IS_ERR_OR_NULL(reason->atomic_cookie)) {
		dev_err(&pdev->dev, "failed to register the atomic ssr notifier, ret is %ld\n",
			PTR_ERR(reason->atomic_cookie));
		return PTR_ERR(reason->atomic_cookie);
	}

	reason->ssr_blk.notifier_call = ipq_debug_ssr_handler;
	reason->cookie = qcom_register_ssr_notifier(rproc->name,
						    &reason->ssr_blk);
	if (IS_ERR_OR_NULL(reason->cookie)) {
		dev_err(&pdev->dev, "failed to register the ssr notifier, ret is %ld\n",
			PTR_ERR(reason->cookie));
		return PTR_ERR(reason->cookie);
	}

	return 0;
}

static int ipq_debug_probe(struct platform_device *pdev)
{
	struct restart_reason *reason;
	unsigned int q6_reason;
	void __iomem *imem_base, *q6_base;
	struct device_node *np;
	int ret;

	np = of_node_get(pdev->dev.of_node);
	if (!np)
		return 0;

	reason = devm_kzalloc(&pdev->dev, sizeof(*reason), GFP_KERNEL);
	if (!reason)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, reason);

	imem_base = ipq_debug_parse_address(&pdev->dev,
				"qcom,msm-imem-restart-reason-buf-addr");
	if (IS_ERR_OR_NULL(imem_base))
		return PTR_ERR(imem_base);

	memcpy_fromio(&reason->reset_reason, imem_base, 4);
	iounmap(imem_base);

	if (of_device_is_compatible(np, "qcom,ipq-debug")) {
		restart_reason_logging(reason->reset_reason);
		return 0;
	}

	/*
	 * For ipq5424, kernel needs to write the restart reason in IMEM
	 * during the kernel panic and Q6 crash.
	 */

	reason->wr_addr = ipq_debug_parse_address(&pdev->dev,
				"qcom,imem-restart-reason-buf-wr-addr");
	if (IS_ERR_OR_NULL(reason->wr_addr))
		return PTR_ERR(reason->wr_addr);

	q6_base = ipq_debug_parse_address(&pdev->dev,
					  "qcom,imem-restart-reason-buf-q6-addr");
	if (IS_ERR_OR_NULL(q6_base))
		return PTR_ERR(q6_base);

	memcpy_fromio(&q6_reason, q6_base, 4);
	iounmap(q6_base);

	reason->panic_blk.notifier_call = debug_panic_handler;
	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &reason->panic_blk);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the panic notifier, ret is %d\n",
			ret);
		return ret;
	}

	ret = ipq_debug_register_rproc_notifiers(pdev, reason);
	if (ret)
		return ret;

	restart_reason_logging_ipq5424(reason->reset_reason, q6_reason);

	g_reason = reason;

	return 0;
}

static int ipq_debug_remove(struct platform_device *pdev)
{
	struct restart_reason *reason = dev_get_drvdata(&pdev->dev);

	if (reason)
		atomic_notifier_chain_unregister(&panic_notifier_list,
						 &reason->panic_blk);

	if (reason->atomic_cookie)
		qcom_unregister_ssr_atomic_notifier(reason->atomic_cookie,
						    &reason->atomic_ssr_blk);

	if (reason->cookie)
		qcom_unregister_ssr_notifier(reason->cookie,
					     &reason->ssr_blk);

	return 0;
}

static ssize_t reset_reason_store(struct device *device,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	ret = debug_log_reset_reason(val);

	return ret < 0 ? ret : count;
}

static ssize_t reset_reason_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct restart_reason *reason = dev_get_drvdata(dev);

	if (!reason)
		return -EINVAL;

	return sysfs_emit(buf, "%u\n", reason->reset_reason);
}

static DEVICE_ATTR_RW(reset_reason);

static struct attribute *ipq_debug_attrs[] = {
	&dev_attr_reset_reason.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ipq_debug);

static struct platform_driver ipq_debug_driver = {
	.probe	= ipq_debug_probe,
	.remove	= ipq_debug_remove,
	.driver	= {
		.name = "qcom,ipq-debug",
		.of_match_table = ipq_debug_match_table,
		.dev_groups = ipq_debug_groups,
	},
};

module_platform_driver(ipq_debug_driver);

MODULE_DESCRIPTION("QCOM IPQ DEBUG Driver");
