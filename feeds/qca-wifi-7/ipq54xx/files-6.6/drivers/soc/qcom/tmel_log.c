// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/key.h>
#include <linux/kobject.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/tmelcom_ipc.h>
#include <linux/firmware/qcom/qcom_scm.h>

#define MAX_COMPONENT		30
#define MAX_ARG_SIZE		MAX_COMPONENT * 2
#define MAX_LOG_SIZE		4096
#define QTI_CLASS_KEY_ID			0x8
#define OEM_PRODUCT_SEED_KEY_ID			0xC
#define TMEL_ECDH_IP_KEY_MAX_SIZE		0x60
#define TMEL_ECC_MAX_KEY_LEN			((TMEL_ECDH_IP_KEY_MAX_SIZE * 2) + 1)

static int log_level[MAX_ARG_SIZE] = {-1};
static int argc = 0;

struct tmellog_data {
	unsigned long base_addr;
	u32 fuse_addr;
	u32 fuse_addr_size;
	u32 tme_auth_en_mask;
	bool tmelcom_support;
	u32 version;
};

static ssize_t tmel_log_read(struct file *fp, char __user *user_buffer,
				size_t count, loff_t *position)
{
	char *log;
	uint32_t size;
	int ret = 0;

	log = kzalloc(MAX_LOG_SIZE, GFP_KERNEL);
	if (!log)
		return -ENOMEM;

	ret = tmelcom_get_tmel_log(log, MAX_LOG_SIZE, &size);
        if (ret) {
		pr_err("%s : Get TMEL LOG is failed\n", __func__);
		return ret;
	}
	pr_info("TMEL Log buffer size : %x\n", size);

	return simple_read_from_buffer(user_buffer, count, position,
					log, size);
}

static const struct file_operations tmel_log_fops = {
	.read = tmel_log_read,
};

static ssize_t get_ecc_public_key_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret, index;
	u32 type, len;
	void *resp_buf;
	char key_buf[TMEL_ECC_MAX_KEY_LEN] = {0};
	const struct tmellog_data *data = dev_get_drvdata(dev);

	if (!data->tmelcom_support)
		return -EOPNOTSUPP;

	if (kstrtouint(buf, 0, &type))
		return -EINVAL;

	if (type != OEM_PRODUCT_SEED_KEY_ID && type != QTI_CLASS_KEY_ID) {
		pr_err("Invalid Key ID. Valid key IDs are 0x8(QTI), 0xC(OEM)\n");
		return -EINVAL;
	}

	resp_buf = kzalloc(TMEL_ECDH_IP_KEY_MAX_SIZE, GFP_KERNEL);
	if (!resp_buf)
		return -ENOMEM;

	ret = tmelcomm_get_ecc_public_key(type, resp_buf,
					  TMEL_ECDH_IP_KEY_MAX_SIZE, &len);
	if (ret)
		goto out;

	for (index = 0; index < len; index++) {
		snprintf(key_buf + strlen(key_buf),
			 (TMEL_ECC_MAX_KEY_LEN - strlen(key_buf)),
			 "%02X", *(u8 *)(resp_buf + index));
	}
	/* Printing ECC Public Key */
	pr_info("%s\n", key_buf);

out:
	kfree(resp_buf);
	return count;
}

static void dump_fuse_v1(unsigned int addr)
{
	struct fuse_payload_ipq9574 *fuse __free(kfree);
	int ret;

	fuse = kzalloc(sizeof(struct fuse_payload_ipq9574), GFP_KERNEL);
	if (!fuse)
		return;

	fuse->fuse_addr = addr;
	ret = qcom_scm_get_ipq_fuse_list(fuse,
					 sizeof(struct fuse_payload_ipq9574));
	if (ret) {
		pr_err("Fuse list SCM call failed, ret = %d\n", ret);
		return;
	}

	pr_info("TMEL_FUSE_ADDR: 0x%08X\tVALUE: 0x%08X\n", fuse->fuse_addr,
		fuse->val);
}

static void dump_fuse_v2(unsigned int addr)
{
	struct fuse_payload *fuse __free(kfree);
	int ret;

	fuse = kzalloc(sizeof(struct fuse_payload), GFP_KERNEL);
	if (!fuse)
		return;

	fuse->fuse_addr = addr;
	ret = qcom_scm_get_ipq_fuse_list(fuse,
					 sizeof(struct fuse_payload));
	if (ret) {
		pr_err("Fuse list SCM call failed, ret = %d\n", ret);
		return;
	}

	pr_info("TMEL_FUSE_ADDR: 0x%08X\tVALUE: 0x%08X%08X\n",
		fuse->fuse_addr, fuse->msb_val, fuse->lsb_val);
}

static ssize_t dump_fuse_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	const struct tmellog_data *data = dev_get_drvdata(dev);
	unsigned int addr;

	if (kstrtouint(buf, 0, &addr))
		return -EINVAL;

	if (!data)
		return -EINVAL;

	if (data->version == 0x1)
		dump_fuse_v1(addr);
	else if (data->version == 0x2)
		dump_fuse_v2(addr);

	return count;
}

static int list_fuse_v1(const struct tmellog_data *data, char *buf)
{
	struct fuse_payload_ipq9574 *fuse __free(kfree);
	unsigned long base_addr = data->base_addr;
	int index = 0, next = 0;
	int ret, n = 0;

	fuse = kzalloc((sizeof(struct fuse_payload_ipq9574) *
			data->fuse_addr_size), GFP_KERNEL);
	if (!fuse)
		return -ENOMEM;

	fuse[index++].fuse_addr = data->fuse_addr;
	fuse[index].fuse_addr = data->fuse_addr + 0x4;

	for (index = 2; index < data->fuse_addr_size; index++) {
		fuse[index].fuse_addr = base_addr + next;
		next += 0x4;
	}

	ret = qcom_scm_get_ipq_fuse_list(fuse,
					 sizeof(struct fuse_payload_ipq9574) *
					 data->fuse_addr_size);
	if (ret) {
		pr_err("Fuse list SCM call failed, ret = %d\n", ret);
		return ret;
	}

	n += scnprintf(buf + n, PAGE_SIZE - n, "Fuse Name\tAddress\t\tValue\n");
	n += scnprintf(buf + n, PAGE_SIZE - n, "------------------------------------------------\n");

	n += scnprintf(buf + n, PAGE_SIZE - n, "TME_AUTH_EN\t0x%08X\t0x%08X\n", fuse[0].fuse_addr,
		       fuse[0].val & data->tme_auth_en_mask);
	n += scnprintf(buf + n, PAGE_SIZE - n, "TME_OEM_ID\t0x%08X\t0x%08X\n", fuse[0].fuse_addr,
		       fuse[0].val & 0xFFFF0000);
	n += scnprintf(buf + n, PAGE_SIZE - n, "TME_PRODUCT_ID\t0x%08X\t0x%08X\n",
		       fuse[1].fuse_addr, fuse[1].val & 0xFFFF);

	for (index = 2; index < data->fuse_addr_size; index++) {
		n += scnprintf(buf + n, PAGE_SIZE - n, "TME_MRC_HASH\t0x%08X\t0x%08X\n",
			       fuse[index].fuse_addr, fuse[index].val);
	}

	return n;
}

static int list_fuse_v2(const struct tmellog_data *data, char *buf)
{
	struct fuse_payload *fuse __free(kfree);
	unsigned long base_addr = data->base_addr;
	int index = 0, next = 0;
	int ret, n = 0;

	fuse = kzalloc((sizeof(struct fuse_payload) * data->fuse_addr_size),
			GFP_KERNEL);

	if (!fuse)
		return -ENOMEM;

	fuse[0].fuse_addr = data->fuse_addr;
	for (index = 1; index < data->fuse_addr_size; index++) {
		fuse[index].fuse_addr = base_addr + next;
		next += 0x8;
	}

	ret = qcom_scm_get_ipq_fuse_list(fuse, sizeof(struct fuse_payload ) *
					 data->fuse_addr_size);
	if (ret) {
		pr_err("Fuse list SCM call failed, ret = %d\n", ret);
		return ret;
	}

	n += scnprintf(buf + n, PAGE_SIZE - n, "Fuse Name\tAddress\t\tValue\n");
	n += scnprintf(buf + n, PAGE_SIZE - n, "------------------------------------------------\n");

	n += scnprintf(buf + n, PAGE_SIZE - n, "TME_AUTH_EN\t0x%08X\t0x%08X\n", fuse[0].fuse_addr,
		       fuse[0].lsb_val & data->tme_auth_en_mask);
	n += scnprintf(buf + n, PAGE_SIZE - n, "TME_OEM_ID\t0x%08X\t0x%08X\n", fuse[0].fuse_addr,
		       fuse[0].lsb_val & 0xFFFF0000);
	n += scnprintf(buf + n, PAGE_SIZE - n, "TME_PRODUCT_ID\t0x%08X\t0x%08X\n",
		       fuse[0].fuse_addr + 0x4, fuse[0].msb_val & 0xFFFF);

	for (index = 1; index < data->fuse_addr_size; index++) {
		n += scnprintf(buf + n, PAGE_SIZE - n, "TME_MRC_HASH\t0x%08X\t0x%08X\n",
			       fuse[index].fuse_addr, fuse[index].lsb_val);
		n += scnprintf(buf + n, PAGE_SIZE - n, "TME_MRC_HASH\t0x%08X\t0x%08X\n",
			       fuse[index].fuse_addr + 0x4, fuse[index].msb_val);
	}

	return n;
}

static ssize_t list_fuse_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	const struct tmellog_data *data = dev_get_drvdata(dev);

	if (!data)
		return -EINVAL;

	if (data->version == 0x1)
		return list_fuse_v1(data, buf);
	else if (data->version == 0x2)
		return list_fuse_v2(data, buf);

	return -EINVAL;
}

static DEVICE_ATTR_WO(dump_fuse);
static DEVICE_ATTR_RO(list_fuse);
static DEVICE_ATTR_WO(get_ecc_public_key);

static struct attribute *tmel_log_attrs[] = {
	&dev_attr_dump_fuse.attr,
	&dev_attr_list_fuse.attr,
	&dev_attr_get_ecc_public_key.attr,
	NULL,
};
ATTRIBUTE_GROUPS(tmel_log);

static int tmel_log_probe(struct platform_device *pdev)
{
	struct tmel_log_config *log_config;
	const struct tmellog_data *data;
	struct dentry *file;
	uint32_t count = 0;
	int i, ret = 0;

	data = of_device_get_match_data(&pdev->dev);
	if (!data)
		return -EINVAL;

	platform_set_drvdata(pdev, (void*)data);

	if (!data->tmelcom_support)
		return 0;

	file  = debugfs_create_file("tmel_log", 0444, NULL,
					NULL, &tmel_log_fops);
	if (IS_ERR_OR_NULL(file)) {
		dev_err(&pdev->dev, "unable to create tmel_log debugfs\n");
		return -EIO;
	}
	if (!argc)
		return ret;

	/* argc will have component id and loglevel, 2 for each entry, so
	   checks argc % 2 != 0
	 */
	if (argc % 2 != 0 || argc > MAX_ARG_SIZE) {
		dev_err(&pdev->dev,
			"Invalid arguments to parse component and log level\n");
		return ret;
	}

	log_config = kzalloc((argc / 2) * sizeof(*log_config), GFP_KERNEL);
	if (!log_config)
		return -ENOMEM;

	for (i = 0; i < argc; i = i + 2) {
		dev_info(&pdev->dev,
			"component ID : Log Level = %d : %d\n",
			log_level[i], log_level[i+1]);
		log_config[count].component_id = log_level[i];
		log_config[count].log_level = log_level[i+1];
		count++;
	}

	ret = tmelcom_set_tmel_log_config(log_config,
					  (argc / 2) * sizeof(log_config));
	if (ret)
		dev_err(&pdev->dev,
			"failed to set the config, ret = %d\n", ret);

	kfree(log_config);
	return ret;
}

static const struct tmellog_data tmellog_ipq5332_data = {
	.base_addr = 0xA00E8,
	.fuse_addr = 0xA00D0,
	.fuse_addr_size = 0x8,
	.tme_auth_en_mask = 0x41,
	.tmelcom_support = false,
	.version = 0x2,
};

static const struct tmellog_data tmellog_ipq5424_data = {
	.base_addr = 0xA00F8,
	.fuse_addr = 0xA00E0,
	.fuse_addr_size = 0x8,
	.tme_auth_en_mask = 0x82,
	.tmelcom_support = true,
	.version = 0x2,
};

static const struct tmellog_data tmellog_ipq9574_data = {
	.base_addr = 0xA00D8,
	.fuse_addr = 0xA00C0,
	.fuse_addr_size = 0x10,
	.tme_auth_en_mask = 0x80,
	.tmelcom_support = false,
	.version = 0x1,
};

static const struct of_device_id tmel_log_match_tbl[] = {
	{.compatible = "qcom,tmel-log-ipq5332",
	 .data = &tmellog_ipq5332_data,
	},
	{.compatible = "qcom,tmel-log-ipq5424",
	 .data = &tmellog_ipq5424_data,
	},
	{.compatible = "qcom,tmel-log-ipq9574",
	 .data = &tmellog_ipq9574_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, tmel_log_match_tbl);

static struct platform_driver tmel_log_driver = {
	.probe	= tmel_log_probe,
	.driver	= {
		.name = "tmel-log",
		.of_match_table = tmel_log_match_tbl,
		.dev_groups = tmel_log_groups,
	},
};

static int __init tmel_log_driver_init(void)
{
	return platform_driver_register(&tmel_log_driver);
}
subsys_initcall_sync(tmel_log_driver_init);

module_param_array(log_level, int, &argc, 0000);
MODULE_PARM_DESC(log_level, "An array of components and log level");

MODULE_DESCRIPTION("Provides multiple debugging features for TME-L");
MODULE_LICENSE("GPL");
