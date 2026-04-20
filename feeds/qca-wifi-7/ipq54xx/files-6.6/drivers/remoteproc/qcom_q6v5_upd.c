// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/firmware.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>

/* To be removed */
#include "qcom_common.h"

#define WCSS_CRASH_REASON		421
#define WCSS_SMEM_HOST			1

#define UPD_SWID		0x12
#define BUF_SIZE			35

struct upd_ops {
	int (*mdt_load)(struct device *dev, const struct firmware *fw,
			const char *fw_name, int pas_id, void *mem_region,
			phys_addr_t mem_phys, size_t mem_size,
			phys_addr_t *reloc_base);
	int (*powerup_scm)(u32 peripheral);
	int (*powerdown_scm)(u32 peripheral);
};

struct user_pd {
	struct device *dev;
	struct rproc *rproc;
	struct notifier_block rproc_atomic_nb;
	void *ssr_notifier;
	void *ssr_atomic_notifier;
	phys_addr_t mem_phys;
	phys_addr_t mem_reloc;
	void *mem_region;
	size_t mem_size;
	struct qcom_smem_state *state;
	struct qcom_smem_state *spawn_state;
	struct qcom_smem_state *stop_state;
	unsigned int spawn_bit;
	unsigned int stop_bit;
	int fatal_irq;
	int spawn_irq;
	int ready_irq;
	int stop_irq;
	struct completion spawn_done;
	struct completion start_done;
	struct completion stop_done;
	bool start_ack;
	bool stop_ack;
	bool spawn_ack;
	int crash_reason_smem;
	s8 pd_asid;
	bool running;
	bool autostart;
	const struct upd_ops *ops;
};

static struct user_pd *g_upd;

static int q6v5_userpd_load_m3_firmware(struct user_pd *upd)
{
	int ret;
	const struct firmware *m3_fw;
	const char *m3_fw_name;

	ret = of_property_read_string(upd->dev->of_node, "m3_firmware",
				      &m3_fw_name);
	if (ret) {
		dev_err(upd->dev, "No m3_firmware entry, skipping m3 firmware load\n");
		return 0;
	}

	ret = request_firmware(&m3_fw, m3_fw_name, upd->dev);
	if (ret) {
		dev_err(upd->dev, "Failed to get m3_firmware %d\n", ret);
		return ret;
	}

	ret = qcom_mdt_load_no_init(upd->dev, m3_fw,
				    m3_fw_name, 0,
				    upd->mem_region, upd->mem_phys,
				    upd->mem_size, &upd->mem_reloc);
	release_firmware(m3_fw);

	if (ret) {
		dev_err(upd->dev, "can't load %s ret:%d\n", m3_fw_name, ret);
		return ret;
	}

	dev_info(upd->dev, "m3 firmware %s loaded to DDR\n", m3_fw_name);
	return ret;
}

static int q6v5_user_pd_load(struct user_pd *upd)
{
	const char *fw_name = NULL;
	const struct firmware *fw;
	u32 pasid;
	int ret;

	ret = of_property_read_string(upd->dev->of_node, "firmware", &fw_name);
	if (ret) {
		dev_err(upd->dev, "Failed to get firmware name\n");
		return -ENOENT;
	}

	ret = request_firmware(&fw, fw_name, upd->dev);
	if (ret) {
		dev_err(upd->dev, "Failed to get firmware %d", ret);
		return ret;
	}

	pasid = (upd->pd_asid << 8) | UPD_SWID;

	if (upd->ops->mdt_load)
		ret = upd->ops->mdt_load(upd->dev, fw, fw_name, pasid,
					  upd->mem_region, upd->mem_phys,
					  upd->mem_size, &upd->mem_reloc);
	else
		ret = -EINVAL;

	release_firmware(fw);
	return ret;
}

static int qcom_q6v5_userpd_spawn(struct user_pd *upd)
{
	int ret;

	upd->spawn_ack = false;
	ret = qcom_smem_state_update_bits(upd->spawn_state,
					  BIT(upd->spawn_bit), BIT(upd->spawn_bit));

	ret = wait_for_completion_timeout(&upd->spawn_done, 5 * HZ);

	qcom_smem_state_update_bits(upd->spawn_state,
				    BIT(upd->spawn_bit), 0);

	if (!ret)
		return -ETIMEDOUT;
	else
		return upd->spawn_ack ? 0 : -ERESTARTSYS;
}

static int qcom_q6v5_userpd_start(struct user_pd *upd, int timeout)
{
	int ret;

	ret = wait_for_completion_timeout(&upd->start_done, timeout);

	if (!ret)
		return -ETIMEDOUT;
	else
		return upd->start_ack ? 0 : -ERESTARTSYS;
}

static int __q6v5_start_user_pd(struct user_pd *upd)
{
	int ret;
	u32 pasid = (upd->pd_asid << 8) | UPD_SWID;

	if (upd->running) {
		dev_info(upd->dev, "userpd already started, skipping start\n");
		return 0;
	}

	dev_info(upd->dev, "starting userpd\n");

	ret = q6v5_user_pd_load(upd);
	if (ret) {
		dev_err(upd->dev, "Failed to load userpd firmware %d\n", ret);
		return ret;
	}

	ret = q6v5_userpd_load_m3_firmware(upd);
	if (ret)
		dev_info(upd->dev, "Skipping m3_firmware load\n");

	if (upd->ops->powerup_scm) {
		ret = upd->ops->powerup_scm(pasid);
		if (ret) {
			dev_err(upd->dev, "Power up SCM failed for %d, ret %d",
				pasid, ret);
			return ret;
		}
	}

	reinit_completion(&upd->start_done);
	reinit_completion(&upd->stop_done);
	reinit_completion(&upd->spawn_done);

	ret = qcom_q6v5_userpd_spawn(upd);
	if (ret) {
		dev_err(upd->dev, "Spawn failed, ret = %d\n", ret);
		return ret;
	}

	ret = qcom_q6v5_userpd_start(upd, msecs_to_jiffies(10000));
	if (ret) {
		dev_err(upd->dev, "Start failed, ret = %d\n", ret);
		upd->running = false;
		return ret;
	}

	upd->running = true;
	dev_info(upd->dev, "userpd is now up!\n");
	return ret;
}

int q6v5_start_user_pd(struct rproc *rproc)
{
	struct user_pd *upd = g_upd;
	int ret;

	/* check if upd driver is probed, else return success */
	if (!upd)
		return 0;

	if (!upd->autostart) {
		dev_info(upd->dev, "Ignoring start event for rproc: %s",
			 upd->rproc->name);
		return 0;
	}

	ret = __q6v5_start_user_pd(upd);
	if (ret)
		dev_err(upd->dev, "Failed to start userpd %d\n", ret);

	return ret;
}

static int qcom_q6v5_userpd_stop(struct user_pd *upd)
{
	int ret;

	if (!upd->running) {
		dev_info(upd->dev, "userpd not started, skipping stop\n");
		return 0;
	}

	upd->running = false;
	upd->stop_ack = false;

	/* Don't perform SMP2P update if remote isn't running */
	if (upd->rproc->state != RPROC_RUNNING)
		return 0;

	qcom_smem_state_update_bits(upd->state,
				    BIT(upd->stop_bit), BIT(upd->stop_bit));

	ret = wait_for_completion_timeout(&upd->stop_done, 5 * HZ);

	qcom_smem_state_update_bits(upd->state, BIT(upd->stop_bit), 0);

	if (!ret)
		return -ETIMEDOUT;
	else
		return upd->stop_ack ? 0 : -ERESTARTSYS;
}

static int __q6v5_stop_user_pd(struct user_pd *upd)
{
	int ret;
	u32 pasid = (upd->pd_asid << 8) | UPD_SWID;

	dev_info(upd->dev, "Stopping userpd\n");
	ret = qcom_q6v5_userpd_stop(upd);
	if (ret) {
		dev_err(upd->dev, "Stop failed, ret = %d\n", ret);
		return ret;
	}

	if (upd->ops->powerdown_scm) {
		ret = upd->ops->powerdown_scm(pasid);
		if (ret) {
			dev_err(upd->dev, "Failed to shutdown userpd, ret = %d\n",
				ret);
			return ret;
		}
	}

	dev_info(upd->dev, "stopped userpd!\n");

	return ret;
}

int q6v5_stop_user_pd(struct rproc *rproc)
{
	struct user_pd *upd = g_upd;

	/* check if upd driver is probed, else return success */
	if (!upd)
		return 0;

	return __q6v5_stop_user_pd(upd);
}

irqreturn_t q6v5_upd_fatal_handler(int irq, void *data)
{
	struct user_pd *upd = data;
	size_t len;
	char *msg;

	if (!upd->running)
		return IRQ_HANDLED;

	msg = qcom_smem_get(WCSS_SMEM_HOST, WCSS_CRASH_REASON, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		dev_err(upd->dev, "fatal error received: %s\n", msg);
	else
		dev_err(upd->dev, "fatal error without message\n");

	upd->running = false;

	/* Complete any pending waits for this rproc */
	complete(&upd->spawn_done);
	complete(&upd->start_done);
	complete(&upd->stop_done);

	rproc_report_crash(upd->rproc, RPROC_FATAL_ERROR);
	return IRQ_HANDLED;
}

irqreturn_t q6v5_upd_spawn_handler(int irq, void *data)
{
	struct user_pd *upd = data;

	upd->spawn_ack = true;
	complete(&upd->spawn_done);

	return IRQ_HANDLED;
}

irqreturn_t q6v5_upd_stop_handler(int irq, void *data)
{
	struct user_pd *upd = data;

	upd->stop_ack = true;
	complete(&upd->stop_done);

	return IRQ_HANDLED;
}

irqreturn_t q6v5_upd_ready_handler(int irq, void *data)
{
	struct user_pd *upd = data;

	upd->start_ack = true;
	complete(&upd->start_done);

	return IRQ_HANDLED;
}

static int q6_get_inbound_irq(struct user_pd *upd,
			      struct platform_device *pdev,
			      const char *int_name,
			      irqreturn_t (*handler)(int irq, void *data))
{
	int ret, irq;
	char *interrupt, *tmp = (char *)int_name;

	irq = platform_get_irq_byname(pdev, int_name);
	if (irq < 0) {
		if (irq != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to retrieve %s IRQ: %d\n",
				int_name, irq);
		return irq;
	}

	if (!strcmp(int_name, "fatal")) {
		upd->fatal_irq = irq;
	} else if (!strcmp(int_name, "stop-ack")) {
		upd->stop_irq = irq;
		tmp = "stop_ack";
	} else if (!strcmp(int_name, "ready")) {
		upd->ready_irq = irq;
	} else if (!strcmp(int_name, "spawn-ack")) {
		upd->spawn_irq = irq;
		tmp = "spawn_ack";
	} else {
		dev_err(&pdev->dev, "unknown interrupt\n");
		return -EINVAL;
	}

	interrupt = devm_kzalloc(&pdev->dev, BUF_SIZE, GFP_KERNEL);
	if (!interrupt)
		return -ENOMEM;

	snprintf(interrupt, BUF_SIZE, "q6v5_userpd%d", upd->pd_asid);
	strlcat(interrupt, "_", BUF_SIZE);
	strlcat(interrupt, tmp, BUF_SIZE);

	ret = devm_request_threaded_irq(&pdev->dev, irq,
					NULL, handler,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					interrupt, upd);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire %s irq\n", interrupt);
		return ret;
	}
	return 0;
}

static int q6_get_outbound_irq(struct user_pd *upd,
			       struct platform_device *pdev,
			       const char *int_name)
{
	struct qcom_smem_state *tmp_state;
	unsigned int bit;

	tmp_state = qcom_smem_state_get(&pdev->dev, int_name, &bit);
	if (IS_ERR(tmp_state)) {
		dev_err(&pdev->dev, "failed to acquire %s state\n", int_name);
		return PTR_ERR(tmp_state);
	}

	if (!strcmp(int_name, "stop")) {
		upd->state = tmp_state;
		upd->stop_bit = bit;
	} else if (!strcmp(int_name, "spawn")) {
		upd->spawn_state = tmp_state;
		upd->spawn_bit = bit;
	}

	return 0;
}

static int init_irq(struct user_pd *upd, struct platform_device *pdev)
{
	int ret;

	ret = q6_get_inbound_irq(upd, pdev, "fatal",
				 q6v5_upd_fatal_handler);
	if (ret)
		return ret;

	ret = q6_get_inbound_irq(upd, pdev, "ready",
				 q6v5_upd_ready_handler);
	if (ret)
		return ret;

	ret = q6_get_inbound_irq(upd, pdev, "stop-ack",
				 q6v5_upd_stop_handler);
	if (ret)
		return ret;

	ret = q6_get_inbound_irq(upd, pdev, "spawn-ack",
				 q6v5_upd_spawn_handler);
	if (ret)
		return ret;

	ret = q6_get_outbound_irq(upd, pdev, "stop");
	if (ret)
		return ret;

	ret = q6_get_outbound_irq(upd, pdev, "spawn");
	if (ret)
		return ret;

	return 0;
}

static int upd_alloc_memory_region(struct user_pd *upd)
{
	struct reserved_mem *rmem = NULL;
	struct device_node *node;
	struct device *dev = upd->dev;

	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (node)
		rmem = of_reserved_mem_lookup(node);

	of_node_put(node);

	if (!rmem) {
		dev_err(dev, "unable to acquire memory-region\n");
		return -EINVAL;
	}

	upd->mem_phys = rmem->base;
	upd->mem_reloc = rmem->base;
	upd->mem_size = rmem->size;
	upd->mem_region = devm_ioremap_wc(dev, upd->mem_phys, upd->mem_size);
	if (!upd->mem_region) {
		dev_err(dev, "unable to map memory region: %pa+%pa\n",
			&rmem->base, &rmem->size);
		return -EBUSY;
	}
	return 0;
}

static int q6v5_upd_rproc_atomic_notifier_cb(struct notifier_block *nb,
					     unsigned long action, void *data)
{
	struct user_pd *upd = container_of(nb, struct user_pd, rproc_atomic_nb);

	dev_info(upd->dev, "Received atomic event %lu for rproc: %s\n",
		 action, upd->rproc->name);

	switch (action) {
	case QCOM_SSR_NOTIFY_CRASH:
		/* Complete any pending waits for the userpd if rproc crashed */
		complete(&upd->spawn_done);
		complete(&upd->start_done);
		complete(&upd->stop_done);
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

static ssize_t state_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct user_pd *upd = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", upd->running ? "started" : "stopped");
}

static ssize_t state_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct user_pd *upd = dev_get_drvdata(dev);
	int ret = 0;

	if (sysfs_streq(buf, "start")) {
		ret = __q6v5_start_user_pd(upd);
		if (ret)
			dev_err(upd->dev, "Failed to start userpd %d\n", ret);
	} else if (sysfs_streq(buf, "stop")) {
		ret = __q6v5_stop_user_pd(upd);
		if (ret)
			dev_err(upd->dev, "Failed to stop userpd %d\n", ret);
	} else {
		dev_err(upd->dev, "Unrecognised option: %s\n", buf);
		ret = -EINVAL;
	}

	return ret ? ret : count;
}

static ssize_t autostart_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct user_pd *upd = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", upd->autostart ? "enabled" : "disabled");
}

static ssize_t autostart_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct user_pd *upd = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "disabled")) {
		upd->autostart = false;
	} else if (sysfs_streq(buf, "enabled")) {
		upd->autostart = true;
	} else {
		dev_err(upd->dev, "Unrecognised option: %s\n", buf);
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR_RW(state);
static DEVICE_ATTR_RW(autostart);

static struct attribute *userpd_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_autostart.attr,
	NULL,
};

static struct attribute_group userpd_group = {
	.attrs = userpd_attrs,
};

static int q6v5_upd_probe(struct platform_device *pdev)
{
	phandle rproc_phandle;
	struct user_pd *upd;
	int ret;

	upd = devm_kzalloc(&pdev->dev, sizeof(struct user_pd), GFP_KERNEL);
	if (!upd)
		return -ENOMEM;

	upd->dev = &pdev->dev;
	upd->autostart = true;

	if (of_property_read_u32(upd->dev->of_node, "qcom,rproc",
				 &rproc_phandle)) {
		dev_err(upd->dev, "Failed to get rproc phandle\n");
		return -ENODEV;
	}

	upd->rproc = rproc_get_by_phandle(rproc_phandle);
	if (!upd->rproc) {
		dev_err(upd->dev, "Failed to get rproc\n");
		return -EPROBE_DEFER;
	}

	upd->ops = of_device_get_match_data(&pdev->dev);
	if (!upd->ops)
		return -EINVAL;

	upd->rproc_atomic_nb.notifier_call = q6v5_upd_rproc_atomic_notifier_cb;
	upd->rproc_atomic_nb.priority = 1;

	upd->ssr_atomic_notifier =
			qcom_register_ssr_atomic_notifier(upd->rproc->name,
							  &upd->rproc_atomic_nb);

	if (IS_ERR(upd->ssr_atomic_notifier)) {
		dev_err(upd->dev, "Failed to register SSR atomic notifier\n");
		return PTR_ERR(upd->ssr_atomic_notifier);
	}

	ret = upd_alloc_memory_region(upd);
	if (ret) {
		dev_err(upd->dev, "Failed to init reserved memory\n");
		return -ENOMEM;
	}

	upd->pd_asid = qcom_get_pd_asid(upd->dev->of_node);
	if (upd->pd_asid < 0) {
		dev_err(upd->dev, "Failed to get pd_asidn\n");
		return -EINVAL;
	}

	init_completion(&upd->start_done);
	init_completion(&upd->stop_done);
	init_completion(&upd->spawn_done);

	ret = init_irq(upd, pdev);
	if (ret) {
		dev_err(upd->dev, "userpd irq registration failed\n");
		return -EBUSY;
	}

	platform_set_drvdata(pdev, upd);

	ret = sysfs_create_group(&pdev->dev.kobj, &userpd_group);
	if (ret) {
		dev_err(upd->dev, "Unable to create sysfs group\n");
		return ret;
	}

	g_upd = upd;
	dev_info(upd->dev, "userpd probed successfully\n");

	return 0;
}

static int q6v5_upd_remove(struct platform_device *pdev)
{
	struct user_pd *upd = platform_get_drvdata(pdev);

	dev_info(upd->dev, "%s\n", __func__);

	qcom_unregister_ssr_atomic_notifier(upd->ssr_atomic_notifier,
					    &upd->rproc_atomic_nb);

	return 0;
}

static const struct upd_ops ipq5424_upd_ops = {
	.mdt_load = qcom_mdt_load_no_init,
};

static const struct upd_ops ipq5332_upd_ops = {
	.mdt_load = qcom_mdt_load_no_init,
	.powerup_scm = qcom_scm_pas_auth_and_reset,
	.powerup_scm = qcom_scm_pas_shutdown,
};

static const struct of_device_id q6v5_upd_of_match[] = {
	{ .compatible = "qcom,ipq5424-q6v5-upd", .data = &ipq5424_upd_ops },
	{ .compatible = "qcom,ipq5332-q6v5-upd", .data = &ipq5332_upd_ops },
	{ },
};
MODULE_DEVICE_TABLE(of, q6_wcss_of_match);

static struct platform_driver q6v5_upd_driver = {
	.probe = q6v5_upd_probe,
	.remove = q6v5_upd_remove,
	.driver = {
		.name = "qcom-q6v5-upd",
		.of_match_table = q6v5_upd_of_match,
	},
};
module_platform_driver(q6v5_upd_driver);
MODULE_DESCRIPTION("WCSS Multi-PD UserPD Driver Module");
MODULE_LICENSE("Dual BSD/GPL");
