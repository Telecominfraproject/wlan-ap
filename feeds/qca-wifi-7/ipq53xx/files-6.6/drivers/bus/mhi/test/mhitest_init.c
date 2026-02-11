/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include "commonmhitest.h"

static bool rddm_r;
module_param(rddm_r, bool, 0);
MODULE_PARM_DESC(rddm_r, "Do need to go for recovery after rddm?");

int domain = -1;
module_param(domain, int, 0);
MODULE_PARM_DESC(domain, "Domain number of specific device to be probed");

int timeout_ms = MHI_TIMEOUT_DEFAULT;
module_param(timeout_ms, int, 0);
MODULE_PARM_DESC(timeout_ms, "timeout mhi test");

static LIST_HEAD(mplat_g);
static struct platform_device *m_plat_dev;

struct platform_device *get_plat_device(void)
{
	return m_plat_dev;
}

void mhitest_store_mplat(struct mhitest_platform *temp)
{
	list_add(&temp->node, &mplat_g);
}

void mhitest_remove_mplat(struct mhitest_platform *temp)
{
	list_del(&temp->node);
}

void mhitest_free_mplat(struct mhitest_platform *temp)
{
	devm_kfree(&temp->plat_dev->dev, temp);
	mhitest_remove_mplat(temp);
}

struct mhitest_platform *get_mhitest_mplat_by_pcidev(struct pci_dev *pci_dev)
{
	struct mhitest_platform *temp;

	list_for_each_entry(temp, &mplat_g, node) {
		if (temp->pci_dev == pci_dev)
			return temp;
	}

	return NULL;
}

char *mhitest_recov_reason_to_str(enum mhitest_recovery_reason reason)
{
	switch (reason) {
	case MHI_DEFAULT:
		return "MHI_DEFAULT";
	case MHI_LINK_DOWN:
		return "MHI_LINK_DOWN";
	case MHI_RDDM:
		return "MHI_RDDM";
	case MHI_TIMEOUT:
		return "MHI_TIMEOUT";
	default:
		return "UNKNOWN";
	}
}

void mhitest_recovery_post_rddm(struct mhitest_platform *mplat)
{
	int ret;

	pr_debug("Enter\n");
	msleep(10000); /*Let's wait for some time !*/

	mhitest_pci_soc_reset(mplat);
	mhitest_pci_set_mhi_state(mplat, MHI_POWER_OFF);
	mhitest_pci_set_mhi_state(mplat, MHI_DEINIT);
	mplat->running = false;

	mhitest_global_soc_reset(mplat);
	msleep(2000);
	mhitest_reset_mhi_state(mplat);

	ret = mhitest_prepare_start_mhi(mplat);
	if (ret) {
		pr_err("Error preapare start mhi  ret:%d\n", ret);
		return;
	}

	pr_debug("Exit\n");
}

int mhitest_recovery_event_handler(struct mhitest_platform *mplat, void *data)
{
	struct mhitest_driver_event *event = data;
	struct mhitest_recovery_data *rdata = event->data;

	pr_emerg("Recovery triggred with reason:(%s)-(%d)\n",
		 mhitest_recov_reason_to_str(rdata->reason), rdata->reason);

	switch (rdata->reason) {
	case MHI_DEFAULT:
	case MHI_LINK_DOWN:
	case MHI_TIMEOUT:
		break;
	case MHI_RDDM:
		mhitest_dump_info(mplat, false);
		mhitest_dev_ramdump(mplat);

		if (rddm_r) { /*using mod param for now*/
			mhitest_recovery_post_rddm(mplat);
			return 0; /*for now*/
		} else
			return 0;
		break;
	default:
		pr_err("Incorrect reason\n");
		break;
	}
	kfree(data);
	return 0;
}

static void mhitest_event_work(struct work_struct *work)
{
	struct mhitest_platform *mplat =
		container_of(work, struct mhitest_platform, event_work);
	struct mhitest_driver_event *event;
	unsigned long flags;
	int ret = 0;

	if (!mplat) {
		pr_err("NULL mplat\n");
		return;
	}
	spin_lock_irqsave(&mplat->event_lock, flags);
	while (!list_empty(&mplat->event_list)) {
		event = list_first_entry(&mplat->event_list,
				struct mhitest_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&mplat->event_lock, flags);

		switch (event->type) {
		/*only support recovery event so far*/
		case MHITEST_RECOVERY_EVENT:
			pr_info("MHITEST_RECOVERY_EVENT event\n");
			ret = mhitest_recovery_event_handler(mplat, event);
			break;
		default:
			pr_err("Invalid event received ..\n");
			kfree(event);
			continue;
		}

		spin_lock_irqsave(&mplat->event_lock, flags);
		event->ret = ret;
		if (event->sync) {
			pr_err("Sending event completion event\n");
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&mplat->event_lock, flags);
		kfree(event);
		spin_lock_irqsave(&mplat->event_lock, flags);
	}
	spin_unlock_irqrestore(&mplat->event_lock, flags);

}
int mhitest_register_driver(void)
{
	int ret = 1;

	pr_info("Going for register pci and subsystem\n");
	ret = mhitest_pci_register();
	if (ret) {
		pr_err("Error pci register ret:%d\n", ret);
		goto error_pci_reg;
	}
	return 0;

error_pci_reg:
	mhitest_pci_unregister();
	return ret;
}

void mhitest_unregister_driver(void)
{
	pr_info("Unregistering\n");

	/* add driver related unregister stuffs here */
	mhitest_pci_unregister();

}

int mhitest_event_work_init(struct mhitest_platform *mplat)
{
	spin_lock_init(&mplat->event_lock);
	mplat->event_wq = alloc_workqueue("mhitest_mod_event",
						      WQ_UNBOUND, 1);
	if (!mplat->event_wq) {
		pr_err("Failed to create event workqueue!\n");
		return -EFAULT;
	}
	INIT_WORK(&mplat->event_work, mhitest_event_work);
	INIT_LIST_HEAD(&mplat->event_list);

	return 0;
}

void mhitest_event_work_deinit(struct mhitest_platform *mplat)
{
	if (mplat->event_wq)
		destroy_workqueue(mplat->event_wq);
}

static int mhitest_probe(struct platform_device *plat_dev)
{
	int ret;

	pr_debug("Enter\n");

	m_plat_dev = plat_dev;

	ret = mhitest_register_driver();
	if (ret) {
		pr_err("Error ret:%d\n", ret);
		goto fail_probe;
	}
	pr_debug("Exit\n");
	return 0;

fail_probe:
	return ret;
}

static int mhitest_remove(struct platform_device *plat_dev)
{
	pr_debug("Enter\n");
	mhitest_unregister_driver();
	m_plat_dev = NULL;
	pr_debug("Exit\n");
	return 0;
}

void mhitest_pci_disable_msi(struct mhitest_platform *mplat)
{
	pci_free_irq_vectors(mplat->pci_dev);
}

void mhitest_pci_unregister_mhi(struct mhitest_platform *mplat)
{
	struct mhi_controller *mhi_ctrl = mplat->mhi_ctrl;

	mhi_unregister_controller(mhi_ctrl);
	kfree(mhi_ctrl->irq);
}

static void mhitest_pci_free_mhi_controller(struct mhitest_platform *mplat)
{
	mhi_free_controller(mplat->mhi_ctrl);
	mplat->mhi_ctrl = NULL;
}

int mhitest_pci_remove_all(struct mhitest_platform *mplat)
{
	pr_debug("Enter\n");

	mhitest_pci_unregister_mhi(mplat);
	mhitest_pci_disable_msi(mplat);
	mhitest_pci_disable_bus(mplat);
	mhitest_pci_free_mhi_controller(mplat);
	mhitest_unregister_ramdump(mplat);

	pr_debug("Exit\n");

	return 0;
}

static const struct platform_device_id test_platform_id_table[] = {
	{ .name = "qcn90xx", .driver_data = QCN90XX_DEVICE_ID, },
};

static const struct of_device_id test_of_match_table[] = {
	{
	.compatible = "qcom,testmhi",
	.data = (void *)&test_platform_id_table[0]},
	{ },
};

MODULE_DEVICE_TABLE(of, test_of_match_table);

struct platform_driver mhitest_platform_driver = {
	.probe = mhitest_probe,
	.remove = mhitest_remove,
	.driver = {
		.name = "mhitest",
		.owner = THIS_MODULE,
		.of_match_table = test_of_match_table,
	},
};

int __init mhitest_init(void)
{
	int ret;
	pr_debug("--->\n");
	ret = platform_driver_register(&mhitest_platform_driver);
	if (ret)
		pr_err("Error ret:%d\n", ret);
	pr_debug("<---done\n");
	return ret;
}

void __exit mhitest_exit(void)
{
	pr_debug("Enter\n");
	platform_driver_unregister(&mhitest_platform_driver);
	pr_debug("Exit\n");
}

module_init(mhitest_init);
module_exit(mhitest_exit);

MODULE_DESCRIPTION("MHITEST");
MODULE_LICENSE("GPL v2");
