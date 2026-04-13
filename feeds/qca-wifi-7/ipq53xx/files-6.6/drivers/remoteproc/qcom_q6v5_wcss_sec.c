// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Linaro Ltd.
 * Copyright (C) 2014 Sony Mobile Communications AB
 * Copyright (c) 2012-2018, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/soc/qcom/smem.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/tmelcom_ipc.h>
#include <soc/qcom/license_manager.h>

#include "qcom_common.h"
#include "qcom_q6v5.h"
#include "remoteproc_internal.h"

#define WCSS_CRASH_REASON		421
#define WCSS_SMEM_HOST			1
#define WCSS_PASID			0x6
#define RPD_SWID			0xD

#define Q6_BOOT_ARGS_SMEM_SIZE		4096
#define UPD_BOOTARGS_HEADER_TYPE	0x2
#define LIC_BOOTARGS_HEADER_TYPE        0x3

#define RESET_CMD_ID			0x18

#define TCSR_SPARE_REG0		0x1959000
#define TCSR_SPARE_REG0_SIZE	4

static int debug_wcss;

enum q6_bootargs_version {
	VERSION1 = 1,
	VERSION2,
};

struct q6v5_wcss_sec {
	struct device *dev;
	struct qcom_rproc_glink glink_subdev;
	const char *textpd_fw;
	u32 textpd_pasid;
	struct qcom_rproc_ssr ssr_subdev;
	struct qcom_q6v5 q6;
	phys_addr_t mem_phys;
	phys_addr_t mem_reloc;
	void *mem_region;
	size_t mem_size;
	int crash_reason_smem;
	void *metadata;
	size_t metadata_len;
	void *debug_wcss_reg;
};

struct wcss_data {
	const char *q6_firmware_name;
	int crash_reason_smem;
	int remote_id;
	const struct rproc_ops *ops;
	bool need_auto_boot;
	u32 pasid;
	u8 bootargs_version;
	bool tmelcom_support;
};

struct bootargs_smem_info {
	void *smem_base_ptr;
	void *smem_elem_cnt_ptr;
	void *smem_bootargs_ptr;
};

struct license_params {
	dma_addr_t dma_buf;
	void *buf;
	size_t size;
};

static struct license_params lic_param;

struct bootargs_header {
	u8 type;
	u8 length;
};

struct q6_userpd_bootargs {
	struct bootargs_header header;
	u8 pid;
	u32 bootaddr;
	u32 data_size;
} __packed;

struct license_bootargs {
	struct bootargs_header header;
	u8 license_type;
	u32 addr;
	u32 size;
} __packed;

static int q6v5_wcss_sec_start(struct rproc *rproc)
{
	struct q6v5_wcss_sec *wcss = rproc->priv;
	int ret;
	const struct wcss_data *desc;

	desc = of_device_get_match_data(wcss->dev);
	if (!desc)
		return -EINVAL;

	qcom_q6v5_prepare(&wcss->q6);

	if (debug_wcss) {
		if (desc->tmelcom_support) {
			writel(0x1, wcss->debug_wcss_reg);
			dev_info(wcss->dev, "Writing 1 to TCSR_SPARE_REG0\n");
		} else {
			dev_info(wcss->dev, "Invoking SCM for break_at_start\n");
			ret = qcom_scm_break_q6_start(RESET_CMD_ID);
			if (ret) {
				dev_err(wcss->dev, "breaking q6 failed\n");
				goto free_lic_buf;
			}
		}
	}

	if (desc->tmelcom_support)
		ret = tmelcom_secboot_sec_auth(desc->pasid, wcss->metadata,
					       wcss->metadata_len);
	else
		ret = qcom_scm_pas_auth_and_reset(desc->pasid);

	if (ret) {
		dev_err(wcss->dev, "wcss_reset failed: %d\n", ret);
		goto out;
	}

wait_for_start:
	ret = qcom_q6v5_wait_for_start(&wcss->q6, msecs_to_jiffies(10000));
	if (ret == -ETIMEDOUT) {
		if (debug_wcss) {
			goto wait_for_start;
		} else {
			dev_err(wcss->dev, "start timed out\n");
			goto out;
		}
	}

	if (!ret && wcss->textpd_fw) {
		ret = qcom_scm_pas_auth_and_reset(wcss->textpd_pasid);
		if (ret) {
			dev_err(wcss->dev, "Failed to start textpd fw : %d\n", ret);
			goto out;
		}
	}

	ret = q6v5_start_user_pd(rproc);
	if (ret)
		dev_err(wcss->dev, "Failed to start userpd %d\n", ret);

out:
	if (ret && desc->tmelcom_support)
		tmelcom_secboot_teardown(desc->pasid, 0);

free_lic_buf:
	if (lic_param.buf) {
		lm_free_license(lic_param.buf, lic_param.dma_buf, lic_param.size);
		lic_param.buf = NULL;
	}

	return ret;
}

static int q6v5_wcss_sec_stop(struct rproc *rproc)
{
	struct q6v5_wcss_sec *wcss = rproc->priv;
	int ret;
	const struct wcss_data *desc =
			of_device_get_match_data(wcss->dev);

	if (!desc)
		return -EINVAL;

	ret = q6v5_stop_user_pd(rproc);
	if (ret)
		dev_err(wcss->dev, "Failed to stop userpd %d\n", ret);

	if (wcss->textpd_fw) {
		ret = qcom_scm_pas_shutdown(wcss->textpd_pasid);
		if (ret)
			dev_err(wcss->dev, "Failed to stop textpd fw: %d\n", ret);
	}

	if (desc->tmelcom_support)
		ret = tmelcom_secboot_teardown(desc->pasid, 0);
	else
		ret = qcom_scm_pas_shutdown(desc->pasid);

	if (ret) {
		dev_err(wcss->dev, "not able to shutdown\n");
		return ret;
	}

	qcom_q6v5_unprepare(&wcss->q6);

	return 0;
}

static void *q6v5_wcss_sec_da_to_va(struct rproc *rproc, u64 da, size_t len,
				    bool *is_iomem)
{
	struct q6v5_wcss_sec *wcss = rproc->priv;
	int offset;

	offset = da - wcss->mem_reloc;
	if (offset < 0 || offset + len > wcss->mem_size)
		return NULL;

	return wcss->mem_region + offset;
}

static int load_userpd_info_to_bootargs(struct rproc *rproc,
					struct bootargs_smem_info *boot_args)
{
	struct q6v5_wcss_sec *wcss = rproc->priv;
	struct q6_userpd_bootargs upd_bootargs = {0};
	int ret, num_userpds, i;
	const struct firmware *fw;
	const char **fw_names;
	u16 cnt;

	num_userpds = of_property_count_strings(wcss->dev->of_node,
						"upd-firmware-names");
	if (num_userpds < 0)
		return -ENODEV;

	fw_names = kcalloc(num_userpds + 1, sizeof(*fw_names), GFP_KERNEL);
	if (!fw_names)
		return -ENOMEM;

	ret = of_property_read_string_array(wcss->dev->of_node,
					    "upd-firmware-names",
					    fw_names, num_userpds);
	if (ret < 0) {
		dev_err(wcss->dev, "Failed to get userpd firmware names: %d\n", ret);
		goto out;
	}

	cnt = *((u16 *)boot_args->smem_elem_cnt_ptr);
	cnt += sizeof(struct q6_userpd_bootargs) * num_userpds;
	memcpy_toio(boot_args->smem_elem_cnt_ptr, &cnt, sizeof(u16));

	for (i = 0; i < num_userpds; i++) {
		dev_info(wcss->dev, "fw_names[%d/%d] = %s\n", i, num_userpds,
			 fw_names[i]);

		/* TYPE */
		upd_bootargs.header.type = UPD_BOOTARGS_HEADER_TYPE;

		/* LENGTH */
		upd_bootargs.header.length =
			sizeof(struct q6_userpd_bootargs) - sizeof(upd_bootargs.header);

		/* PID*/
		upd_bootargs.pid = i + 2;

		ret = request_firmware(&fw, fw_names[i], wcss->dev);
		if (ret) {
			dev_err(wcss->dev, "Failed to get firmware %d", ret);
			break;
		}

		/* Load address */
		upd_bootargs.bootaddr = rproc_get_boot_addr(rproc, fw);

		/* PIL data size */
		upd_bootargs.data_size = qcom_mdt_get_file_size(fw);

		release_firmware(fw);

		/* copy into smem bootargs array*/
		memcpy_toio(boot_args->smem_bootargs_ptr,
			    &upd_bootargs, sizeof(struct q6_userpd_bootargs));

		boot_args->smem_bootargs_ptr +=
					sizeof(struct q6_userpd_bootargs);
	}

out:
	kfree(fw_names);
	return ret;
}

static
void load_license_params_to_bootargs(struct device *dev,
				     struct bootargs_smem_info *boot_args)
{
	u16 cnt;
	u32 rd_val;
	struct license_bootargs lic_bootargs = {0x0};

	lic_param.buf = lm_get_license(INTERNAL, &lic_param.dma_buf,
				       &lic_param.size, 0, NULL, 0);
	if (!lic_param.buf) {
		dev_info(dev, "No license file passed in bootargs\n");
		return;
	}

	/* No of elements */
	cnt = *((u16 *)boot_args->smem_elem_cnt_ptr);
	cnt += sizeof(struct license_bootargs);
	memcpy_toio(boot_args->smem_elem_cnt_ptr, &cnt, sizeof(u16));

	/* TYPE */
	lic_bootargs.header.type = LIC_BOOTARGS_HEADER_TYPE;

	/* LENGTH */
	lic_bootargs.header.length =
			sizeof(lic_bootargs) - sizeof(lic_bootargs.header);

	/* license type */
	if (!of_property_read_u32(dev->of_node, "license-type", &rd_val))
		lic_bootargs.license_type = (u8)rd_val;

	/* ADDRESS */
	lic_bootargs.addr = (u32)lic_param.dma_buf;

	/* License file size */
	lic_bootargs.size = lic_param.size;
	memcpy_toio(boot_args->smem_bootargs_ptr,
		    &lic_bootargs, sizeof(lic_bootargs));
	boot_args->smem_bootargs_ptr += sizeof(lic_bootargs);

	dev_info(dev, "License file copied in bootargs\n");
}

static int share_bootargs_to_q6(struct rproc *rproc, struct device *dev)
{
	int ret;
	u32 smem_id, rd_val;
	const char *key = "qcom,bootargs_smem";
	size_t size;
	u16 cnt, tmp, version;
	void *ptr;
	u8 *bootargs_arr;
	struct device_node *np = dev->of_node;
	struct bootargs_smem_info boot_args;
	const struct wcss_data *desc =
				of_device_get_match_data(dev);

	if (!desc)
		return -EINVAL;

	ret = of_property_read_u32(np, key, &smem_id);
	if (ret) {
		dev_err(dev, "failed to get smem id\n");
		return ret;
	}

	ret = qcom_smem_alloc(WCSS_SMEM_HOST, smem_id, Q6_BOOT_ARGS_SMEM_SIZE);
	if (ret && ret != -EEXIST) {
		dev_err(dev, "failed to allocate q6 bootargs smem segment\n");
		return ret;
	}

	boot_args.smem_base_ptr = qcom_smem_get(WCSS_SMEM_HOST, smem_id, &size);
	if (IS_ERR(boot_args.smem_base_ptr)) {
		dev_err(dev, "Unable to acquire smp2p item(%d) ret:%ld\n",
			smem_id, PTR_ERR(boot_args.smem_base_ptr));
		return PTR_ERR(boot_args.smem_base_ptr);
	}
	ptr = boot_args.smem_base_ptr;

	/*get physical address*/
	dev_info(dev, "smem physical address:0x%lX\n",
		 (uintptr_t)qcom_smem_virt_to_phys(ptr));

	/*Version*/
	version = desc->bootargs_version;
	memcpy_toio(ptr, &version, sizeof(version));
	ptr += sizeof(version);
	boot_args.smem_elem_cnt_ptr = ptr;

	ret = of_property_count_u32_elems(np, "boot-args");
	cnt = ret;
	if (ret < 0) {
		if (ret == -ENODATA) {
			dev_err(dev, "failed to read boot args ret:%d\n", ret);
			return ret;
		}
		cnt = 0;
	}

	/* No of elements */
	memcpy_toio(ptr, &cnt, sizeof(u16));
	ptr += sizeof(u16);

	bootargs_arr = kzalloc(cnt, GFP_KERNEL);
	if (!bootargs_arr)
		return -ENOMEM;

	for (tmp = 0; tmp < cnt; tmp++) {
		ret = of_property_read_u32_index(np, "boot-args", tmp, &rd_val);
		if (ret) {
			dev_err(dev, "failed to read boot args\n");
			kfree(bootargs_arr);
			return ret;
		}
		bootargs_arr[tmp] = (u8)rd_val;
	}

	/* Copy bootargs */
	memcpy_toio(ptr, bootargs_arr, cnt);
	ptr += (cnt);
	boot_args.smem_bootargs_ptr = ptr;

	of_node_put(np);
	kfree(bootargs_arr);

	ret = load_userpd_info_to_bootargs(rproc, &boot_args);
	if (ret < 0) {
		dev_err(dev, "failed to read userpd boot args ret:%d\n", ret);
		return ret;
	}

	load_license_params_to_bootargs(dev, &boot_args);

	return 0;
}

static int load_m3_firmware(struct q6v5_wcss_sec *wcss)
{
	int ret;
	const struct firmware *m3_fw;
	const char *m3_fw_name;
	struct device_node *np = wcss->dev->of_node;

	ret = of_property_read_string(np, "m3_firmware", &m3_fw_name);
	if (ret == -EINVAL) {
		return 0;
	} else if (ret) {
		dev_err(wcss->dev, "m3_firmware load failed ret:%d\n", ret);
		return ret;
	}

	ret = request_firmware(&m3_fw, m3_fw_name, wcss->dev);
	if (ret) {
		dev_err(wcss->dev, "request_firmware failed %s ret:%d\n", m3_fw_name, ret);
		return 0;
	}

	ret = qcom_mdt_load_no_init(wcss->dev, m3_fw,
				    m3_fw_name, 0,
				    wcss->mem_region, wcss->mem_phys,
				    wcss->mem_size, &wcss->mem_reloc);
	release_firmware(m3_fw);

	if (ret) {
		dev_err(wcss->dev, "can't load %s ret:%d\n", m3_fw_name, ret);
		return ret;
	}

	dev_info(wcss->dev, "m3 firmware %s loaded to DDR\n", m3_fw_name);
	return ret;
}

static int q6v5_wcss_sec_load(struct rproc *rproc, const struct firmware *fw)
{
	struct q6v5_wcss_sec *wcss = rproc->priv;
	const struct firmware *textpd_fw;
	int ret;
	const struct wcss_data *desc =
				of_device_get_match_data(wcss->dev);

	if (!desc)
		return -EINVAL;

	/* Share boot args to Q6 remote processor */
	ret = share_bootargs_to_q6(rproc, wcss->dev);
	if (ret && ret != -EINVAL) {
		dev_err(wcss->dev,
			"boot args sharing with q6 failed %d\n",
			ret);
		return ret;
	}

	/* Read metadata */
	wcss->metadata = qcom_mdt_read_metadata(fw, &wcss->metadata_len,
						rproc->firmware, wcss->dev);
	if (IS_ERR(wcss->metadata)) {
		ret = PTR_ERR(wcss->metadata);
		dev_err(wcss->dev, "error %d reading firmware %s metadata\n",
			ret, rproc->firmware);
		return ret;
	}

	/* Load firmware into DDR */
	if (desc->tmelcom_support)
		ret = qcom_mdt_load_no_init(wcss->dev, fw, rproc->firmware,
					    desc->pasid, wcss->mem_region,
					    wcss->mem_phys, wcss->mem_size,
					    &wcss->mem_reloc);
	else
		ret = qcom_mdt_load(wcss->dev, fw, rproc->firmware,
				    desc->pasid, wcss->mem_region,
				    wcss->mem_phys, wcss->mem_size,
				    &wcss->mem_reloc);

	if (!ret && wcss->textpd_fw) {
		ret = request_firmware(&textpd_fw, wcss->textpd_fw,
				       wcss->dev);
		if (ret < 0) {
			dev_err(wcss->dev, "Failed to get textpd firmware: %d\n",
				ret);
			return ret;
		}

		ret = qcom_mdt_load(wcss->dev, textpd_fw,
				    wcss->textpd_fw,
				    wcss->textpd_pasid,
				    wcss->mem_region, wcss->mem_phys,
				    wcss->mem_size, &wcss->mem_reloc);
		if (ret)
			dev_err(wcss->dev, "Failed to load %s\n",
				wcss->textpd_fw);

		release_firmware(textpd_fw);
	}

	ret = load_m3_firmware(wcss);
	if (ret)
		return ret;

	return ret;
}

static unsigned long q6v5_wcss_sec_panic(struct rproc *rproc)
{
	struct q6v5_wcss_sec *wcss = rproc->priv;

	return qcom_q6v5_panic(&wcss->q6);
}

static
void q6v5_wcss_sec_copy_segment(struct rproc *rproc,
				struct rproc_dump_segment *segment,
				void *dest, size_t offset, size_t size)
{
	struct q6v5_wcss_sec *wcss = rproc->priv;
	struct device *dev = wcss->dev;

	if (!segment->io_ptr) {
		segment->io_ptr = devm_ioremap_wc(dev, segment->da, segment->size);
		dev_dbg(dev, "ioremap region io_ptr:0x%px da:0x%pad size:%zx\n",
			segment->io_ptr, &segment->da, segment->size);
	}

	if (!segment->io_ptr) {
		dev_err(dev, "Failed to ioremap segment %pad size %zx\n",
			&segment->da, segment->size);
		return;
	}

	memcpy(dest, segment->io_ptr + offset, size);
	if (offset + size >= segment->size) {
		dev_dbg(dev, "iounmap region io_ptr:0x%px da:0x%pad size:%zx\n",
			segment->io_ptr, &segment->da, segment->size);
		devm_iounmap(dev, segment->io_ptr);
		segment->io_ptr = NULL;
	}
}

static int q6v5_wcss_sec_dump_segments(struct rproc *rproc,
				       const struct firmware *fw)
{
	struct device *dev = rproc->dev.parent;
	struct reserved_mem *rmem = NULL;
	struct device_node *node;
	int num_segs, index = 0;
	int ret;

	/* Parse through additional reserved memory regions for the rproc
	 * and add them to the coredump segments
	 */
	num_segs = of_count_phandle_with_args(dev->of_node,
					      "memory-region", NULL);
	while (index < num_segs) {
		node = of_parse_phandle(dev->of_node,
					"memory-region", index);
		if (!node)
			return -EINVAL;

		rmem = of_reserved_mem_lookup(node);
		if (!rmem) {
			dev_err(dev, "unable to acquire memory-region index %d num_segs %d\n",
				index, num_segs);
			return -EINVAL;
		}

		of_node_put(node);

		dev_dbg(dev, "Adding segment 0x%pa size 0x%pa",
			&rmem->base, &rmem->size);
		ret = rproc_coredump_add_custom_segment(rproc,
							rmem->base,
							rmem->size,
							q6v5_wcss_sec_copy_segment,
							NULL);
		if (ret)
			return ret;

		index++;
	}

	return 0;
}

static const struct rproc_ops q6v5_wcss_sec_ipq5424_ops = {
	.start = q6v5_wcss_sec_start,
	.stop = q6v5_wcss_sec_stop,
	.da_to_va = q6v5_wcss_sec_da_to_va,
	.load = q6v5_wcss_sec_load,
	.get_boot_addr = rproc_elf_get_boot_addr,
	.panic = q6v5_wcss_sec_panic,
	.parse_fw = q6v5_wcss_sec_dump_segments,
};

static int q6_alloc_memory_region(struct q6v5_wcss_sec *wcss)
{
	struct reserved_mem *rmem = NULL;
	struct device_node *node;
	struct device *dev = wcss->dev;

	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (node)
		rmem = of_reserved_mem_lookup(node);

	of_node_put(node);

	if (!rmem) {
		dev_err(dev, "unable to acquire memory-region\n");
		return -EINVAL;
	}

	wcss->mem_phys = rmem->base;
	wcss->mem_reloc = rmem->base;
	wcss->mem_size = rmem->size;
	wcss->mem_region = devm_ioremap_wc(dev, wcss->mem_phys, wcss->mem_size);
	if (!wcss->mem_region) {
		dev_err(dev, "unable to map memory region: %pa+%pa\n",
			&rmem->base, &rmem->size);
		return -EBUSY;
	}
	return 0;
}

static int q6v5_wcss_sec_probe(struct platform_device *pdev)
{
	const struct wcss_data *desc;
	struct q6v5_wcss_sec *wcss;
	const char *fw_name = NULL;
	struct rproc *rproc;
	int ret;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	of_property_read_string(pdev->dev.of_node, "firmware", &fw_name);
	if (!fw_name)
		fw_name = desc->q6_firmware_name;

	rproc = rproc_alloc(&pdev->dev, pdev->name, desc->ops,
			    fw_name, sizeof(*wcss));
	if (!rproc) {
		dev_err(&pdev->dev, "failed to allocate rproc\n");
		return -ENOMEM;
	}
	wcss = rproc->priv;
	wcss->dev = &pdev->dev;

	ret = of_property_read_string(pdev->dev.of_node, "textpd_fw",
				      &wcss->textpd_fw);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(&pdev->dev, "Failed to get textpd fw: %d\n", ret);
		goto free_rproc;
	}

	if (wcss->textpd_fw) {
		if (of_property_read_u32(pdev->dev.of_node, "textpd_pasid",
					  &wcss->textpd_pasid)) {
			dev_err(&pdev->dev, "Failed to get texpd pasid: %d\n", ret);
			goto free_rproc;
		}
	}

	ret = q6_alloc_memory_region(wcss);
	if (ret)
		goto free_rproc;

	wcss->debug_wcss_reg = devm_ioremap(&pdev->dev, TCSR_SPARE_REG0,
					    TCSR_SPARE_REG0_SIZE);
	if (!wcss->debug_wcss_reg) {
		dev_err(&pdev->dev, "Failed to ioremap debug_wcss register\n");
		ret = PTR_ERR(wcss->debug_wcss_reg);
		goto free_rproc;
	}

	ret = qcom_q6v5_init(&wcss->q6, pdev, rproc, desc->remote_id,
			     desc->crash_reason_smem, NULL, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize: %d", ret);
		goto free_rproc;
	}

	qcom_add_glink_subdev(rproc, &wcss->glink_subdev, "q6wcss");

	qcom_add_ssr_subdev(rproc, &wcss->ssr_subdev, pdev->name);

	rproc->auto_boot = desc->need_auto_boot;
	rproc->dump_conf = RPROC_COREDUMP_INLINE;
	rproc_coredump_set_elf_info(rproc, ELFCLASS32, EM_NONE);
	ret = rproc_add(rproc);
	if (ret)
		goto free_rproc;

	platform_set_drvdata(pdev, rproc);

	return 0;

free_rproc:
	rproc_free(rproc);

	return ret;
}

static int q6v5_wcss_sec_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct q6v5_wcss_sec *wcss = rproc->priv;

	qcom_q6v5_deinit(&wcss->q6);

	rproc_del(rproc);
	rproc_free(rproc);

	return 0;
}

static const struct wcss_data q6_ipq5424_res_init = {
	.q6_firmware_name = "IPQ5424/q6_fw0.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ops = &q6v5_wcss_sec_ipq5424_ops,
	.pasid = RPD_SWID,
	.bootargs_version = VERSION2,
	.tmelcom_support = true,
};

static const struct wcss_data q6_ipq5332_res_init = {
	.q6_firmware_name = "IPQ5332/q6_fw0.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ops = &q6v5_wcss_sec_ipq5424_ops,
	.pasid = RPD_SWID,
	.bootargs_version = VERSION2,
	.tmelcom_support = false,
};

static const struct wcss_data q6_ipq9574_res_init = {
	.q6_firmware_name = "IPQ9574/q6_fw.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ops = &q6v5_wcss_sec_ipq5424_ops,
	.pasid = WCSS_PASID,
	.tmelcom_support = false,
};

static const struct of_device_id q6v5_wcss_sec_of_match[] = {
	{ .compatible = "qcom,ipq5424-q6v5-wcss-sec", .data = &q6_ipq5424_res_init },
	{ .compatible = "qcom,ipq5332-q6v5-wcss-sec", .data = &q6_ipq5332_res_init },
	{ .compatible = "qcom,ipq9574-q6v5-wcss-sec", .data = &q6_ipq9574_res_init },
	{ },
};
MODULE_DEVICE_TABLE(of, q6v5_wcss_sec_of_match);

static struct platform_driver q6v5_wcss_sec_driver = {
	.probe = q6v5_wcss_sec_probe,
	.remove = q6v5_wcss_sec_remove,
	.driver = {
		.name = "qcom-q6v5-wcss-sec",
		.of_match_table = q6v5_wcss_sec_of_match,
	},
};
module_platform_driver(q6v5_wcss_sec_driver);
module_param(debug_wcss, int, 0644);
MODULE_DESCRIPTION("Hexagon WCSS Secure Peripheral Image Loader");
MODULE_LICENSE("GPL v2");
