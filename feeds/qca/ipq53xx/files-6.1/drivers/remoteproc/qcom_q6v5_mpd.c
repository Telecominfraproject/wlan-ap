// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Linaro Ltd.
 * Copyright (C) 2014 Sony Mobile Communications AB
 * Copyright (c) 2012-2018, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#ifdef CONFIG_QCOM_NON_SECURE_PIL
#include <linux/mfd/syscon.h>
#endif
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#ifdef CONFIG_QCOM_NON_SECURE_PIL
#include <linux/regmap.h>
#endif
#include <linux/reset.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/qcom_scm.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <soc/qcom/license_manager.h>
#include "qcom_common.h"
#include "qcom_q6v5.h"

#include "remoteproc_internal.h"

#define WCSS_CRASH_REASON		421
#define WCSS_SMEM_HOST			1

#define WCNSS_PAS_ID			6
#define MPD_WCNSS_PAS_ID        0xD

#define RPD_SWID		MPD_WCNSS_PAS_ID
#define UPD_SWID		0x12

#define BUF_SIZE			35

#define REMOTE_PID			1
#define Q6_BOOT_ARGS_SMEM_SIZE		4096
#define UPD_BOOTARGS_HEADER_TYPE	0x2
#define LIC_BOOTARGS_HEADER_TYPE        0x3

#define RESET_CMD_ID			0x18

#ifdef CONFIG_QCOM_NON_SECURE_PIL
#define MAX_TCSR_REG			3
#define Q6SS_DBG_CFG                    0x18
#define Q6SS_RST_EVB			0x10
#define Q6SS_XO_CBCR			GENMASK(5, 3)
#define Q6SS_SLEEP_CBCR			GENMASK(5, 2)
#define Q6SS_CORE_CBCR			BIT(5)
#define Q6SS_BOOT_CORE_START		0x400
#define Q6SS_BOOT_CMD                   0x404
#define Q6SS_BOOT_STATUS		0x408
#define TCSR_HALT_ACK			0x4

#define GCC_BASE			0x1825000
#define Q6_TSCTR_1TO2			0x20
#define Q6SS_TRIG			0x28
#define Q6_AHB_S			0x18
#define Q6_AHB				0x14
#define Q6SS_ATBM			0x1C
#define Q6_AXIM				0x0C
#define Q6SS_BOOT			0x2C
#define Q6SS_PCLKDBG			0x24
#define WCSS_ECAHB			0x58
#define CNOC_WCSS_AHB			0xC0AC

static long userpd_bootaddr;
static long userpd_size;
#endif
static int debug_wcss;
/**
 * enum state - state of a wcss (private)
 * @WCSS_NORMAL: subsystem is operating normally
 * @WCSS_CRASHED: subsystem has crashed and hasn't been shutdown
 * @WCSS_RESTARTING: subsystem has been shutdown and is now restarting
 * @WCSS_SHUTDOWN: subsystem has been shutdown
 *
 */
enum q6_wcss_state {
	WCSS_NORMAL,
	WCSS_CRASHED,
	WCSS_RESTARTING,
	WCSS_SHUTDOWN,
};

enum {
	Q6_IPQ,
	WCSS_AHB_IPQ,
	WCSS_PCIE_IPQ,
};

enum q6_bootargs_version {
	VERSION1 = 1,
	VERSION2,
};

struct q6_wcss {
	struct device *dev;
	struct qcom_rproc_glink glink_subdev;
	struct qcom_rproc_ssr ssr_subdev;
	struct qcom_q6v5 q6;
	phys_addr_t mem_phys;
	phys_addr_t mem_reloc;
	void *mem_region;
	size_t mem_size;
	int crash_reason_smem;
	u32 version;
	s8 pd_asid;
	enum q6_wcss_state state;
	bool is_fw_shared;
	bool need_mem_protection;
	void __iomem *reg_base;
	void __iomem *wcmn_base;
#ifdef CONFIG_QCOM_NON_SECURE_PIL
	struct reset_control *wcss_q6_reset;
	struct regmap *tcsr_map;
	u32 tcsr_boot;
	u32 tcsr_halt;
	struct clk_bulk_data *clks;
	int num_clks;
	bool is_emulation;
	bool backdoor;
	unsigned short *clk_offset;
#endif
};

struct wcss_data {
#ifdef CONFIG_QCOM_NON_SECURE_PIL
	int (*init_clock)(struct q6_wcss *wcss);
	int (*init_reset)(struct q6_wcss *wcss);
#endif
	int (*init_irq)(struct qcom_q6v5 *q6, struct platform_device *pdev,
			struct rproc *rproc, int remote_id,
			int crash_reason, const char *load_state,
			void (*handover)(struct qcom_q6v5 *q6));
	const char *q6_firmware_name;
	int crash_reason_smem;
	int remote_id;
	u32 version;
	const char *ssr_name;
	const struct rproc_ops *ops;
	bool need_auto_boot;
	bool glink_subdev_required;
	s8 pd_asid;
	bool reset_seq;
	u32 pasid;
	int (*mdt_load_sec)(struct device *dev, const struct firmware *fw,
			    const char *fw_name, int pas_id, void *mem_region,
			    phys_addr_t mem_phys, size_t mem_size,
			    phys_addr_t *reloc_base);
	bool is_fw_shared;
	int (*powerup_scm)(u32 peripheral);
	int (*powerdown_scm)(u32 peripheral);
	u8 bootargs_version;
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

static int handle_upd_in_rpd_crash(void *data)
{
	struct rproc *rpd_rproc = data, *upd_rproc;
	struct q6_wcss *rpd_wcss = rpd_rproc->priv;
	struct device_node *upd_np, *temp;
	struct platform_device *upd_pdev;
	const struct firmware *firmware_p;
	int ret;

	while (1) {
		if (rpd_rproc->state == RPROC_RUNNING)
			break;
		udelay(1);
	}

	for_each_available_child_of_node(rpd_wcss->dev->of_node, upd_np) {
		if (!strstr(upd_np->name, "pd"))
			continue;
		upd_pdev = of_find_device_by_node(upd_np);
		upd_rproc = platform_get_drvdata(upd_pdev);

		if (upd_rproc->state != RPROC_SUSPENDED)
			continue;

		/* load firmware */
		ret = request_firmware(&firmware_p, upd_rproc->firmware,
				       &upd_pdev->dev);
		if (ret < 0) {
			dev_err(&upd_pdev->dev, "request_firmware failed: %d\n",
				ret);
			continue;
		}

		/* start the userpd rproc*/
		ret = rproc_start(upd_rproc, firmware_p);
		if (ret)
			dev_err(&upd_pdev->dev, "failed to start %s\n",
				upd_rproc->name);
		release_firmware(firmware_p);

		for_each_available_child_of_node(upd_np, temp) {
			upd_pdev = of_find_device_by_node(temp);
			upd_rproc = platform_get_drvdata(upd_pdev);

			if (upd_rproc->state != RPROC_SUSPENDED)
				continue;

			/* load firmware */
			ret = request_firmware(&firmware_p, upd_rproc->firmware,
					       &upd_pdev->dev);
			if (ret < 0) {
				dev_err(&upd_pdev->dev,
					"request_firmware failed: %d\n", ret);
				continue;
			}

			/* start the userpd rproc*/
			ret = rproc_start(upd_rproc, firmware_p);
			if (ret)
				dev_err(&upd_pdev->dev, "failed to start %s\n",
					upd_rproc->name);
			release_firmware(firmware_p);
		}
	}
	rpd_wcss->state = WCSS_NORMAL;
	return 0;
}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
static int configure_clocks(struct q6_wcss *wcss, bool value)
{
	int loop1 = 0, loop2;
	int ret = 0;
	void __iomem *gcc_base;

	gcc_base = ioremap(GCC_BASE, 0xC0B0);
	if (IS_ERR_OR_NULL(gcc_base)) {
		dev_err(wcss->dev, "gcc base remap is failed\n");
		return PTR_ERR(gcc_base);
	}

	while (loop1 < wcss->num_clks) {
		writel(value, gcc_base + wcss->clk_offset[loop1]);

		for (loop2 = 0; loop2 < 10; loop2++) {
			if ((readl(gcc_base + wcss->clk_offset[loop1]) & 0x1) == value)
				break;
			mdelay(1);
		}
		loop1++;
	}

	iounmap(gcc_base);
	return ret;
}

static int enable_clocks(struct q6_wcss *wcss)
{
	int ret = 0;

	if (!wcss->is_emulation) {
		ret = clk_bulk_prepare_enable(wcss->num_clks, wcss->clks);
		if (ret) {
			dev_err(wcss->dev, "failed to enable clocks, err=%d\n", ret);
			return ret;
		};
	} else {
		ret = configure_clocks(wcss, 0x1);
		if (ret)
			return ret;
	}
	return ret;
}

static int q6_powerup(struct rproc *rproc)
{
	struct q6_wcss *wcss = rproc->priv;
	int ret;
	u32 val;
	u8 temp = 0;

	/* clear boot trigger */
	regmap_write(wcss->tcsr_map, wcss->tcsr_boot, 0x0);

	/* assert q6 blk reset */
	reset_control_assert(wcss->wcss_q6_reset);
	/* deassert q6 blk reset */
	reset_control_deassert(wcss->wcss_q6_reset);

	/* enable clocks */
	ret = enable_clocks(wcss);
	if (ret)
		return ret;

	if (debug_wcss)
		writel(0x20000001, wcss->reg_base + Q6SS_DBG_CFG);

	/* Write bootaddr to EVB so that Q6WCSS will jump there after reset */
	writel(rproc->bootaddr >> 4, wcss->reg_base + Q6SS_RST_EVB);

	/* BHS require xo cbcr to be enabled */
	val = readl(wcss->reg_base + Q6SS_XO_CBCR);
	val |= 0x1;
	writel(val, wcss->reg_base + Q6SS_XO_CBCR);

	/* Enable core cbcr*/
	val = readl(wcss->reg_base + Q6SS_CORE_CBCR);
	val |= 0x1;
	writel(val, wcss->reg_base + Q6SS_CORE_CBCR);

	/* Enable sleep cbcr*/
	val = readl(wcss->reg_base + Q6SS_SLEEP_CBCR);
	val |= 0x1;
	writel(val, wcss->reg_base + Q6SS_SLEEP_CBCR);

	/* Boot core start */
	writel(0x1, wcss->reg_base + Q6SS_BOOT_CORE_START);

	writel(0x1, wcss->reg_base + Q6SS_BOOT_CMD);

	/* wait for reset to complete */
	while (temp < 20) {
		val = readl(wcss->reg_base + Q6SS_BOOT_STATUS);
		if (val & 0x01)
			break;
		mdelay(1);
		temp++;
	}

	return 0;
}
#endif

static int q6_wcss_start(struct rproc *rproc)
{
	struct q6_wcss *wcss = rproc->priv;
	int ret;
	struct device_node *upd_np, *temp;
	struct platform_device *upd_pdev;
	struct rproc *upd_rproc;
	struct q6_wcss *upd_wcss;
	const struct wcss_data *desc;

	desc = of_device_get_match_data(wcss->dev);
	if (!desc)
		return -EINVAL;

	qcom_q6v5_prepare(&wcss->q6);

	if (wcss->need_mem_protection) {
		if (debug_wcss) {
			ret = qcom_scm_break_q6_start(RESET_CMD_ID);
			if (ret) {
				dev_err(wcss->dev, "breaking q6 failed\n");
				return ret;
			}
		}
		ret = qcom_scm_pas_auth_and_reset(desc->pasid);
		if (ret) {
			dev_err(wcss->dev, "wcss_reset failed\n");
			return ret;
		}
		goto wait_for_start;
	}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	ret = q6_powerup(rproc);
	if (ret)
		return ret;
#endif

wait_for_start:
	ret = qcom_q6v5_wait_for_start(&wcss->q6, msecs_to_jiffies(10000));
	if (ret == -ETIMEDOUT) {
		if (debug_wcss)
			goto wait_for_start;
		else
			dev_err(wcss->dev, "start timed out\n");
	}

	if (wcss->reg_base)
		dev_info(wcss->dev, "QDSP6SS Version : 0x%x\n",
			 readl(wcss->reg_base));
	if (wcss->wcmn_base)
		dev_info(wcss->dev, "WCSS Version : 0x%x\n",
			 readl(wcss->wcmn_base));

	/* start userpd's, if root pd getting recovered*/
	if (wcss->state == WCSS_RESTARTING) {
		kthread_run(handle_upd_in_rpd_crash, rproc, "mpd_restart");
	} else {
		/* Bring userpd wcss state to default value */
		for_each_available_child_of_node(wcss->dev->of_node, upd_np) {
			if (!strstr(upd_np->name, "pd"))
				continue;
			upd_pdev = of_find_device_by_node(upd_np);
			upd_rproc = platform_get_drvdata(upd_pdev);
			upd_wcss = upd_rproc->priv;
			upd_wcss->state = WCSS_NORMAL;

			for_each_available_child_of_node(upd_np, temp) {
				upd_pdev = of_find_device_by_node(temp);
				upd_rproc = platform_get_drvdata(upd_pdev);
				upd_wcss = upd_rproc->priv;
				upd_wcss->state = WCSS_NORMAL;
			}
		}
	}

	if (lic_param.buf) {
		lm_free_license(lic_param.buf, lic_param.dma_buf, lic_param.size);
		lic_param.buf = NULL;
	}
	return ret;
}

static int q6_wcss_spawn_pd(struct rproc *rproc)
{
	int ret;
	struct q6_wcss *wcss = rproc->priv;

	ret = qcom_q6v5_request_spawn(&wcss->q6);
	if (ret == -ETIMEDOUT) {
		pr_err("%s spawn timedout\n", rproc->name);
		return ret;
	}

	ret = qcom_q6v5_wait_for_start(&wcss->q6, msecs_to_jiffies(10000));
	if (ret == -ETIMEDOUT) {
		pr_err("%s start timedout\n", rproc->name);
		wcss->q6.running = false;
		return ret;
	}
	wcss->q6.running = true;
	return ret;
}

static int wcss_ahb_pcie_pd_start(struct rproc *rproc)
{
	struct q6_wcss *wcss = rproc->priv;
	const struct wcss_data *desc = of_device_get_match_data(wcss->dev);
	u8 pd_asid = qcom_get_pd_asid(wcss->dev->of_node);
	u32 pasid;
	int ret;

	if (!desc)
		return -EINVAL;

	pasid = desc->pasid;

	if (desc->reset_seq && wcss->need_mem_protection) {
		if (!pasid)
			pasid = (pd_asid << 8) | UPD_SWID;
		ret = desc->powerup_scm(pasid);
		if (ret) {
			dev_err(wcss->dev, "failed to power up ahb pd\n");
			return ret;
		}
	}

	if (wcss->q6.spawn_bit) {
		ret = q6_wcss_spawn_pd(rproc);
		if (ret)
			return ret;
	}

	wcss->state = WCSS_NORMAL;
	return 0;
}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
static int disable_clocks(struct q6_wcss *wcss)
{
	int ret = 0;

	if (!wcss->is_emulation) {
		clk_bulk_disable_unprepare(wcss->num_clks, wcss->clks);
	} else {
		ret = configure_clocks(wcss, 0x0);
		if (ret)
			return ret;
	}
	return ret;
}

static int q6_powerdown(struct q6_wcss *wcss)
{
	int val, loop;
	int ret;

	regmap_write(wcss->tcsr_map, wcss->tcsr_halt, 0x1);
	do {
		regmap_read(wcss->tcsr_map, wcss->tcsr_halt + TCSR_HALT_ACK,
			    &val);
		mdelay(1);
	} while	(!val);

	reset_control_assert(wcss->wcss_q6_reset);

	ret = disable_clocks(wcss);
	if (ret)
		return ret;

	regmap_write(wcss->tcsr_map, wcss->tcsr_halt, 0x0);
	for (loop = 0; loop < 20; loop++)
		mdelay(1);
	reset_control_deassert(wcss->wcss_q6_reset);

	return ret;
}
#endif

static int q6_wcss_stop(struct rproc *rproc)
{
	struct q6_wcss *wcss = rproc->priv;
	int ret;
	const struct wcss_data *desc =
			of_device_get_match_data(wcss->dev);
	struct device_node *upd_np, *temp;
	struct platform_device *upd_pdev;
	struct rproc *upd_rproc;

	if (!desc)
		return -EINVAL;

	/* stop userpd's, if root pd getting crashed*/
	if (rproc->state == RPROC_CRASHED) {
		for_each_available_child_of_node(wcss->dev->of_node, upd_np) {
			if (!strstr(upd_np->name, "pd"))
				continue;

			upd_pdev = of_find_device_by_node(upd_np);
			upd_rproc = platform_get_drvdata(upd_pdev);

			if (upd_rproc->state == RPROC_OFFLINE)
				continue;

			upd_rproc->state = RPROC_CRASHED;

			/* stop the userpd parent rproc*/
			ret = rproc_stop(upd_rproc, true);
			if (ret)
				dev_err(&upd_pdev->dev, "failed to stop %s\n",
					upd_rproc->name);
			upd_rproc->state = RPROC_SUSPENDED;

			for_each_available_child_of_node(upd_np, temp) {
				upd_pdev = of_find_device_by_node(temp);
				upd_rproc = platform_get_drvdata(upd_pdev);

				if (upd_rproc->state == RPROC_OFFLINE)
					continue;

				upd_rproc->state = RPROC_CRASHED;

				/* stop the userpd child rproc*/
				ret = rproc_stop(upd_rproc, true);
				if (ret)
					dev_err(&upd_pdev->dev, "failed to stop %s\n",
						upd_rproc->name);

				upd_rproc->state = RPROC_SUSPENDED;
			}
		}
		wcss->state = WCSS_RESTARTING;
	}

	if (wcss->need_mem_protection) {
		ret = qcom_scm_pas_shutdown(desc->pasid);
		if (ret) {
			dev_err(wcss->dev, "not able to shutdown\n");
			return ret;
		}
		goto unprepare;
	}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	ret = q6_powerdown(wcss);
	if (ret)
		return ret;
#endif

unprepare:
	qcom_q6v5_unprepare(&wcss->q6);

	return 0;
}

static int wcss_ahb_pcie_pd_stop(struct rproc *rproc)
{
	struct q6_wcss *wcss = rproc->priv;
	struct rproc *rpd_rproc = dev_get_drvdata(wcss->dev->parent);
	const struct wcss_data *desc = of_device_get_match_data(wcss->dev);
	u8 pd_asid = qcom_get_pd_asid(wcss->dev->of_node);
	u32 pasid;
	int ret;

	if (!desc)
		return -EINVAL;

	pasid = desc->pasid;

	if (rproc->state != RPROC_CRASHED && wcss->q6.stop_bit) {
		ret = qcom_q6v5_request_stop(&wcss->q6, NULL);
		if (ret) {
			dev_err(&rproc->dev, "pd not stopped\n");
			return ret;
		}
	}

	if (desc->reset_seq && wcss->need_mem_protection) {
		if (!pasid)
			pasid = (pd_asid << 8) | UPD_SWID;
		ret = desc->powerdown_scm(pasid);
		if (ret) {
			dev_err(wcss->dev, "failed to power down pd\n");
			return ret;
		}
	}

	if (rproc->state != RPROC_CRASHED)
		rproc_shutdown(rpd_rproc);

	wcss->state = WCSS_SHUTDOWN;
	return 0;
}

static void *q6_wcss_da_to_va(struct rproc *rproc, u64 da, size_t len,
			      bool *is_iomem)
{
	struct q6_wcss *wcss = rproc->priv;
	int offset;

	offset = da - wcss->mem_reloc;
	if (offset < 0 || offset + len > wcss->mem_size)
		return NULL;

	return wcss->mem_region + offset;
}

static void load_license_params_to_bootargs(struct device *dev,
					struct bootargs_smem_info *boot_args)
{
	u16 cnt;
	u32 rd_val;
	struct license_bootargs lic_bootargs = {0x0};

	lic_param.buf = lm_get_license(INTERNAL, &lic_param.dma_buf, &lic_param.size, 0);
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
	return;
}

static int copy_userpd_bootargs(struct bootargs_smem_info *boot_args,
				struct rproc *upd_rproc)
{
	struct q6_wcss *upd_wcss = upd_rproc->priv;
	int ret = 0;
	const struct firmware *fw;
	struct q6_userpd_bootargs upd_bootargs = {0};
	struct device *dev = upd_wcss->dev;

	/* TYPE */
	upd_bootargs.header.type = UPD_BOOTARGS_HEADER_TYPE;

	/* LENGTH */
	upd_bootargs.header.length =
		sizeof(struct q6_userpd_bootargs) - sizeof(upd_bootargs.header);

	/* PID */
	upd_bootargs.pid = qcom_get_pd_asid(dev->of_node) + 1;

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	if (upd_wcss->backdoor) {
		/* Load address */
		upd_bootargs.bootaddr = userpd_bootaddr;
		upd_rproc->bootaddr = userpd_bootaddr;

		/* PIL data size */
		upd_bootargs.data_size = userpd_size;
		goto memcpy_to_smem;
	};
#endif
	ret = request_firmware(&fw, upd_rproc->firmware, upd_wcss->dev);
	if (ret < 0)
		return ret;

	/* Load address */
	upd_bootargs.bootaddr = rproc_get_boot_addr(upd_rproc, fw);

	/* PIL data size */
	upd_bootargs.data_size = qcom_mdt_get_file_size(fw);

	release_firmware(fw);

#ifdef CONFIG_QCOM_NON_SECURE_PIL
memcpy_to_smem:
#endif
	/* copy into smem bootargs array*/
	memcpy_toio(boot_args->smem_bootargs_ptr,
		    &upd_bootargs, sizeof(struct q6_userpd_bootargs));
	boot_args->smem_bootargs_ptr += sizeof(struct q6_userpd_bootargs);
	return ret;
}

static int load_userpd_params_to_bootargs(struct device *dev,
					  struct bootargs_smem_info *boot_args)
{
	int ret = 0;
	struct device_node *upd_np, *temp;
	struct platform_device *upd_pdev;
	struct rproc *upd_rproc;
	u16 cnt;
	u8 upd_cnt = 0;

	if (!of_property_read_bool(dev->of_node, "qcom,userpd-bootargs"))
		return -EINVAL;

	for_each_available_child_of_node(dev->of_node, upd_np) {
		if (!strstr(upd_np->name, "pd"))
			continue;
		upd_cnt++;
		for_each_available_child_of_node(upd_np, temp)
			upd_cnt++;
	}

	/* No of elements */
	cnt = *((u16 *)boot_args->smem_elem_cnt_ptr);
	cnt += (sizeof(struct q6_userpd_bootargs) * upd_cnt);
	memcpy_toio(boot_args->smem_elem_cnt_ptr, &cnt, sizeof(u16));

	for_each_available_child_of_node(dev->of_node, upd_np) {
		if (!strstr(upd_np->name, "pd"))
			continue;
		upd_pdev = of_find_device_by_node(upd_np);
		upd_rproc = platform_get_drvdata(upd_pdev);
		ret = copy_userpd_bootargs(boot_args, upd_rproc);
		if (ret)
			return ret;

		for_each_available_child_of_node(upd_np, temp) {
			upd_pdev = of_find_device_by_node(temp);
			upd_rproc = platform_get_drvdata(upd_pdev);
			ret = copy_userpd_bootargs(boot_args, upd_rproc);
			if (ret)
				return ret;
		}
	}
	return ret;
}

static int share_bootargs_to_q6(struct device *dev)
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
		pr_err("failed to get smem id\n");
		return ret;
	}

	ret = qcom_smem_alloc(REMOTE_PID, smem_id, Q6_BOOT_ARGS_SMEM_SIZE);
	if (ret && ret != -EEXIST) {
		pr_err("failed to allocate q6 bootargs smem segment\n");
		return ret;
	}

	boot_args.smem_base_ptr = qcom_smem_get(REMOTE_PID, smem_id, &size);
	if (IS_ERR(boot_args.smem_base_ptr)) {
		pr_err("Unable to acquire smp2p item(%d) ret:%ld\n",
		       smem_id, PTR_ERR(boot_args.smem_base_ptr));
		return PTR_ERR(boot_args.smem_base_ptr);
	}
	ptr = boot_args.smem_base_ptr;

	/*get physical address*/
	pr_info("smem phyiscal address:0x%lX\n",
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
			pr_err("failed to read boot args ret:%d\n", ret);
			return ret;
		}
		cnt = 0;
	}

	/* No of elements */
	memcpy_toio(ptr, &cnt, sizeof(u16));
	ptr += sizeof(u16);

	bootargs_arr = kzalloc(cnt, GFP_KERNEL);
	if (!bootargs_arr) {
		pr_err("failed to allocate memory\n");
		return PTR_ERR(bootargs_arr);
	}

	for (tmp = 0; tmp < cnt; tmp++) {
		ret = of_property_read_u32_index(np, "boot-args", tmp, &rd_val);
		if (ret) {
			pr_err("failed to read boot args\n");
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

	ret = load_userpd_params_to_bootargs(dev, &boot_args);
	if (ret < 0) {
		pr_err("failed to read userpd boot args ret:%d\n", ret);
		return ret;
	}

	load_license_params_to_bootargs(dev, &boot_args);

	return 0;
}

static int load_m3_firmware(struct device_node *np, struct q6_wcss *wcss)
{
	int ret;
	const struct firmware *m3_fw;
	const char *m3_fw_name;

	ret = of_property_read_string(np, "m3_firmware", &m3_fw_name);
	if (ret)
		return 0;

	ret = request_firmware(&m3_fw, m3_fw_name, wcss->dev);
	if (ret)
		return 0;

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

static int q6_wcss_load(struct rproc *rproc, const struct firmware *fw)
{
	struct q6_wcss *wcss = rproc->priv;
	int ret;
	struct platform_device *upd_pdev;
	struct device_node *upd_np, *temp;
	const struct wcss_data *desc =
				of_device_get_match_data(wcss->dev);

	if (!desc)
		return -EINVAL;

	/* Share boot args to Q6 remote processor */
	ret = share_bootargs_to_q6(wcss->dev);
	if (ret && ret != -EINVAL) {
		dev_err(wcss->dev,
			"boot args sharing with q6 failed %d\n",
			ret);
		return ret;
	}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	if (wcss->backdoor) {
		dev_info(wcss->dev, "skipping fw load\n");
		return 0;
	}

	ret = request_firmware(&fw, rproc->firmware, wcss->dev);
	if (ret < 0) {
		dev_err(wcss->dev, "request_firmware failed: %d\n", ret);
		return ret;
	}
	rproc->bootaddr = rproc_get_boot_addr(rproc, fw);
#endif

	/* load m3 firmware */
	for_each_available_child_of_node(wcss->dev->of_node, upd_np) {
		if (!strstr(upd_np->name, "pd"))
			continue;
		upd_pdev = of_find_device_by_node(upd_np);
		ret = load_m3_firmware(upd_np, wcss);
		if (ret)
			return ret;

		for_each_available_child_of_node(upd_np, temp) {
			upd_pdev = of_find_device_by_node(temp);
			ret = load_m3_firmware(temp, wcss);
			if (ret)
				return ret;
		}
	}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	ret = qcom_mdt_load_no_init(wcss->dev, fw, rproc->firmware,
				    desc->pasid, wcss->mem_region,
				    wcss->mem_phys, wcss->mem_size,
				    &wcss->mem_reloc);

	release_firmware(fw);
	return ret;
#endif

	return qcom_mdt_load(wcss->dev, fw, rproc->firmware,
				desc->pasid, wcss->mem_region,
				wcss->mem_phys, wcss->mem_size,
				&wcss->mem_reloc);
}

static int wcss_ahb_pcie_pd_load(struct rproc *rproc, const struct firmware *fw)
{
	struct q6_wcss *wcss_rpd, *wcss = rproc->priv;
	struct rproc *rpd_rproc = dev_get_drvdata(wcss->dev->parent);
	int ret;
	u8 pd_asid;
	u32 pasid;
	const struct wcss_data *desc =
				of_device_get_match_data(wcss->dev);

	wcss_rpd = rpd_rproc->priv;
	if (!desc)
		return -EINVAL;

	/* Don't boot rootpd rproc in case user/root pd recovering after crash */
	if (wcss->state != WCSS_RESTARTING &&
	    wcss_rpd->state != WCSS_RESTARTING) {
		/* Boot rootpd rproc*/
		ret = rproc_boot(rpd_rproc);
		if (ret || (wcss->state == WCSS_NORMAL && wcss->is_fw_shared))
			return ret;
	}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	if (wcss->backdoor) {
		dev_info(wcss->dev, "skipping fw load\n");
		return 0;
	}

	ret = request_firmware(&fw, rproc->firmware, wcss->dev);
	if (ret < 0) {
		dev_err(wcss->dev, "request_firmware failed: %d\n", ret);
		return ret;
	}
#endif

	pasid = desc->pasid;
	if (!pasid) {
		pd_asid = qcom_get_pd_asid(wcss->dev->of_node);
		pasid = (pd_asid << 8) | UPD_SWID;
	}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	ret = qcom_mdt_load_no_init(wcss->dev, fw, rproc->firmware,
				    pasid, wcss->mem_region,
				    wcss->mem_phys, wcss->mem_size,
				    &wcss->mem_reloc);

	release_firmware(fw);
	return ret;
#endif

	return desc->mdt_load_sec(wcss->dev, fw, rproc->firmware,
				pasid, wcss->mem_region,
				wcss->mem_phys, wcss->mem_size,
				&wcss->mem_reloc);
}

static unsigned long q6_wcss_panic(struct rproc *rproc)
{
	struct q6_wcss *wcss = rproc->priv;

	return qcom_q6v5_panic(&wcss->q6);
}

static void q6_wcss_copy_segment(struct rproc *rproc,
				 struct rproc_dump_segment *segment,
				 void *dest, size_t offset, size_t size)
{
	struct q6_wcss *wcss = rproc->priv;
	struct device *dev = wcss->dev;
	int base;
	void *ptr;

	base = segment->da + offset - wcss->mem_reloc;

	if (base < 0 || base + size > wcss->mem_size) {
		ptr = devm_ioremap_wc(dev, segment->da, segment->size);
		if (!ptr) {
			dev_err(dev, "Failed to ioremap segment %pad size %zx\n",
				&segment->da, segment->size);
			return;
		}

		memcpy(dest, ptr + offset, size);
		devm_iounmap(dev, ptr);
	} else {
		memcpy(dest, wcss->mem_region + offset, size);
	}
}

static int q6_wcss_dump_segments(struct rproc *rproc,
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
							q6_wcss_copy_segment,
							NULL);
		if (ret)
			return ret;

		index++;
	}

	return 0;
}

static const struct rproc_ops wcss_ahb_pcie_ipq5018_ops = {
	.start = wcss_ahb_pcie_pd_start,
	.stop = wcss_ahb_pcie_pd_stop,
	.load = wcss_ahb_pcie_pd_load,
	.get_boot_addr = rproc_elf_get_boot_addr,
	.parse_fw = q6_wcss_dump_segments,
};

static const struct rproc_ops q6_wcss_ipq5018_ops = {
	.start = q6_wcss_start,
	.stop = q6_wcss_stop,
	.da_to_va = q6_wcss_da_to_va,
	.load = q6_wcss_load,
	.get_boot_addr = rproc_elf_get_boot_addr,
	.panic = q6_wcss_panic,
	.parse_fw = q6_wcss_dump_segments,
};

static int q6_alloc_memory_region(struct q6_wcss *wcss)
{
	struct reserved_mem *rmem = NULL;
	struct device_node *node;
	struct device *dev = wcss->dev;

	if (wcss->version == Q6_IPQ) {
		node = of_parse_phandle(dev->of_node, "memory-region", 0);
		if (node)
			rmem = of_reserved_mem_lookup(node);

		of_node_put(node);

		if (!rmem) {
			dev_err(dev, "unable to acquire memory-region\n");
			return -EINVAL;
		}
	} else {
		struct rproc *rpd_rproc = dev_get_drvdata(dev->parent);
		struct q6_wcss *rpd_wcss = rpd_rproc->priv;

		wcss->mem_phys = rpd_wcss->mem_phys;
		wcss->mem_reloc = rpd_wcss->mem_reloc;
		wcss->mem_size = rpd_wcss->mem_size;
		wcss->mem_region = rpd_wcss->mem_region;
		return 0;
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

static int q6_get_inbound_irq(struct qcom_q6v5 *q6,
			      struct platform_device *pdev,
			      const char *int_name,
			      irqreturn_t (*handler)(int irq, void *data))
{
	int ret, irq;
	char *interrupt, *tmp = (char *)int_name;
	struct q6_wcss *wcss = q6->rproc->priv;

	irq = platform_get_irq_byname(pdev, int_name);
	if (irq < 0) {
		if (irq != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"failed to retrieve %s IRQ: %d\n",
					int_name, irq);
		return irq;
	}

	if (!strcmp(int_name, "fatal")) {
		q6->fatal_irq = irq;
	} else if (!strcmp(int_name, "stop-ack")) {
		q6->stop_irq = irq;
		tmp = "stop_ack";
	} else if (!strcmp(int_name, "ready")) {
		q6->ready_irq = irq;
	} else if (!strcmp(int_name, "handover")) {
		q6->handover_irq  = irq;
	} else if (!strcmp(int_name, "spawn-ack")) {
		q6->spawn_irq = irq;
		tmp = "spawn_ack";
	} else {
		dev_err(&pdev->dev, "unknown interrupt\n");
		return -EINVAL;
	}

	interrupt = devm_kzalloc(&pdev->dev, BUF_SIZE, GFP_KERNEL);
	if (!interrupt)
		return -ENOMEM;

	snprintf(interrupt, BUF_SIZE, "q6v5_wcss_userpd%d", wcss->pd_asid);
	strlcat(interrupt, "_", BUF_SIZE);
	strlcat(interrupt, tmp, BUF_SIZE);

	ret = devm_request_threaded_irq(&pdev->dev, irq,
					NULL, handler,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					interrupt, q6);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire %s irq\n", interrupt);
		return ret;
	}
	return 0;
}

static int q6_get_outbound_irq(struct qcom_q6v5 *q6,
			       struct platform_device *pdev,
			       const char *int_name)
{
	struct qcom_smem_state *tmp_state;
	unsigned  bit;

	tmp_state = qcom_smem_state_get(&pdev->dev, int_name, &bit);
	if (IS_ERR(tmp_state)) {
		dev_err(&pdev->dev, "failed to acquire %s state\n", int_name);
		return PTR_ERR(tmp_state);
	}

	if (!strcmp(int_name, "stop")) {
		q6->state = tmp_state;
		q6->stop_bit = bit;
	} else if (!strcmp(int_name, "spawn")) {
		q6->spawn_state = tmp_state;
		q6->spawn_bit = bit;
	}

	return 0;
}

static int init_irq(struct qcom_q6v5 *q6,
		    struct platform_device *pdev, struct rproc *rproc,
		    int remote_id, int crash_reason, const char *load_state,
		    void (*handover)(struct qcom_q6v5 *q6))
{
	int ret;

	q6->rproc = rproc;
	q6->dev = &pdev->dev;
	q6->crash_reason = crash_reason;
	q6->remote_id = remote_id;
	q6->handover = handover;

	init_completion(&q6->start_done);
	init_completion(&q6->stop_done);
	init_completion(&q6->spawn_done);

	ret = q6_get_inbound_irq(q6, pdev, "fatal",
				 q6v5_fatal_interrupt);
	if (ret)
		return ret;

	ret = q6_get_inbound_irq(q6, pdev, "ready",
				 q6v5_ready_interrupt);
	if (ret)
		return ret;

	ret = q6_get_inbound_irq(q6, pdev, "stop-ack",
				 q6v5_stop_interrupt);
	if (ret)
		return ret;

	ret = q6_get_inbound_irq(q6, pdev, "spawn-ack",
				 q6v5_spawn_interrupt);
	if (ret)
		return ret;

	ret = q6_get_outbound_irq(q6, pdev, "stop");
	if (ret)
		return ret;

	ret = q6_get_outbound_irq(q6, pdev, "spawn");
	if (ret)
		return ret;

	return 0;
}

#ifdef CONFIG_QCOM_NON_SECURE_PIL
static int devsoc_init_q6_clock(struct q6_wcss *wcss)
{
	int ret = 0;
	int i;

	wcss->num_clks =
		of_property_count_strings(wcss->dev->of_node, "clock-names");

	if (!wcss->is_emulation) {
		wcss->clks = devm_kcalloc(wcss->dev, wcss->num_clks,
					  sizeof(*wcss->clks), GFP_KERNEL);
		if (!wcss->clks)
			return -ENOMEM;

		for (i = 0; i < wcss->num_clks; i++) {
			ret = of_property_read_string_index(wcss->dev->of_node,
							    "clock-names", i,
							    &wcss->clks[i].id);
			if (ret)
				return ret;
		}
		ret = devm_clk_bulk_get(wcss->dev, wcss->num_clks, wcss->clks);
	} else {
		unsigned short clk[] = {Q6_TSCTR_1TO2, Q6SS_TRIG, Q6_AHB_S,
					Q6_AHB, Q6SS_ATBM, Q6_AXIM, Q6SS_BOOT,
					Q6SS_PCLKDBG, WCSS_ECAHB, CNOC_WCSS_AHB };

		wcss->clk_offset = devm_kcalloc(wcss->dev, wcss->num_clks,
						sizeof(unsigned short),
						GFP_KERNEL);
		if (!wcss->clk_offset)
			return -ENOMEM;

		for (i = 0; i < wcss->num_clks; i++)
			wcss->clk_offset[i] = clk[i];
	}
	return ret;
}

static int devsoc_init_reset(struct q6_wcss *wcss)
{
	struct device *dev = wcss->dev;

	wcss->wcss_q6_reset =
		devm_reset_control_get_exclusive(dev, "wcss_q6_reset");
	if (IS_ERR(wcss->wcss_q6_reset)) {
		dev_err(wcss->dev, "unable to acquire wcss_q6_reset\n");
		return PTR_ERR(wcss->wcss_q6_reset);
	}

	return 0;
}

static int q6_wcss_map_tcsr(struct q6_wcss *wcss,
			    struct platform_device *pdev)
{
	struct device_node *syscon;
	unsigned int tcsr_reg[MAX_TCSR_REG] = {0};
	int ret;

	syscon = of_parse_phandle(pdev->dev.of_node,
				  "qcom,q6-tcsr-regs", 0);
	if (syscon) {
		wcss->tcsr_map = syscon_node_to_regmap(syscon);
		of_node_put(syscon);
		if (IS_ERR(wcss->tcsr_map))
			return PTR_ERR(wcss->tcsr_map);

		ret = of_property_read_variable_u32_array(pdev->dev.of_node,
							  "qcom,q6-tcsr-regs",
							  tcsr_reg, 0,
							  MAX_TCSR_REG);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to parse qcom,q6-tcsr-regs\n");
			return -EINVAL;
		}

		wcss->tcsr_boot = tcsr_reg[1];
		wcss->tcsr_halt = tcsr_reg[2];
	}

	return 0;
}
#endif

static int q6_wcss_init_mmio(struct q6_wcss *wcss,
			     struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qdsp6");
	if (res) {
		wcss->reg_base = devm_ioremap(&pdev->dev, res->start,
					      resource_size(res));
		if (IS_ERR(wcss->reg_base))
			return PTR_ERR(wcss->reg_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wcmn");
	if (res) {
		wcss->wcmn_base = devm_ioremap(&pdev->dev, res->start,
					       resource_size(res));
		if (IS_ERR(wcss->wcmn_base))
			return PTR_ERR(wcss->wcmn_base);
	}

	return 0;
}

static int q6_wcss_probe(struct platform_device *pdev)
{
	const struct wcss_data *desc;
	struct q6_wcss *wcss;
	struct rproc *rproc;
	int ret;
	char *subdev_name;
	const char *fw_name = NULL;
#ifdef CONFIG_QCOM_NON_SECURE_PIL
	u32 bootaddr;
#endif

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
	wcss->version = desc->version;
	wcss->is_fw_shared = desc->is_fw_shared;
	wcss->need_mem_protection = true;

	ret = q6_alloc_memory_region(wcss);
	if (ret)
		goto free_rproc;

	wcss->pd_asid = qcom_get_pd_asid(wcss->dev->of_node);
	if (wcss->pd_asid < 0)
		goto free_rproc;

	ret = q6_wcss_init_mmio(wcss, pdev);
	if (ret)
		goto free_rproc;

#ifdef CONFIG_QCOM_NON_SECURE_PIL
	/* Non-secure specific initializations */
	ret = q6_wcss_map_tcsr(wcss, pdev);
	if (ret)
		goto free_rproc;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,nosecure"))
		wcss->need_mem_protection = false;

	wcss->is_emulation = of_property_read_bool(pdev->dev.of_node,
						   "qcom,emulation");
	if (wcss->is_emulation) {
		ret = of_property_read_u32(pdev->dev.of_node, "bootaddr",
					   &bootaddr);
		if (ret) {
			dev_info(&pdev->dev,
				 "boot addr required for emulation,"
				 "since it's not there will proceed"
				 "with PIL images\n");
		} else {
			wcss->backdoor = true;
			rproc->bootaddr = bootaddr;
		}
	}

	if (desc->init_clock) {
		ret = desc->init_clock(wcss);
		if (ret)
			goto free_rproc;
	}

	if (desc->init_reset) {
		ret = desc->init_reset(wcss);
		if (ret)
			goto free_rproc;
	}
#endif

	if (desc->init_irq) {
		ret = desc->init_irq(&wcss->q6, pdev, rproc, desc->remote_id,
				     desc->crash_reason_smem, NULL, NULL);
		if (ret) {
			if (wcss->version == Q6_IPQ)
				goto free_rproc;
			else
				dev_info(wcss->dev,
					 "userpd irq registration failed\n");
		}
	}
	if (desc->glink_subdev_required)
		qcom_add_glink_subdev(rproc, &wcss->glink_subdev, desc->ssr_name);

	subdev_name = (char *)pdev->name;
	qcom_add_ssr_subdev(rproc, &wcss->ssr_subdev, subdev_name);

	rproc->auto_boot = desc->need_auto_boot;
	rproc->dump_conf = RPROC_COREDUMP_INLINE;
	rproc_coredump_set_elf_info(rproc, ELFCLASS32, EM_NONE);
	ret = rproc_add(rproc);
	if (ret)
		goto free_rproc;

	platform_set_drvdata(pdev, rproc);

	ret = of_platform_populate(wcss->dev->of_node, NULL,
				   NULL, wcss->dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to populate wcss pd nodes\n");
		goto free_rproc;
	}
	return 0;

free_rproc:
	rproc_free(rproc);

	return ret;
}

static int q6_wcss_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct q6_wcss *wcss = rproc->priv;

	if (wcss->version == Q6_IPQ)
		qcom_q6v5_deinit(&wcss->q6);

	rproc_del(rproc);
	rproc_free(rproc);

	return 0;
}

static const struct wcss_data q6_ipq5018_res_init = {
	.init_irq = qcom_q6v5_init,
	.q6_firmware_name = "IPQ5018/q6_fw.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ssr_name = "q6wcss",
	.ops = &q6_wcss_ipq5018_ops,
	.version = Q6_IPQ,
	.glink_subdev_required = true,
	.pasid = MPD_WCNSS_PAS_ID,
	.bootargs_version = VERSION1,
};

static const struct wcss_data q6_ipq5332_res_init = {
	.init_irq = qcom_q6v5_init,
	.q6_firmware_name = "IPQ5332/q6_fw0.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ssr_name = "q6wcss",
	.ops = &q6_wcss_ipq5018_ops,
	.version = Q6_IPQ,
	.glink_subdev_required = true,
	.pasid = RPD_SWID,
	.bootargs_version = VERSION2,
};

static const struct wcss_data q6_devsoc_res_init = {
#ifdef CONFIG_QCOM_NON_SECURE_PIL
	.init_clock = devsoc_init_q6_clock,
	.init_reset = devsoc_init_reset,
#endif
	.init_irq = qcom_q6v5_init,
	.q6_firmware_name = "IPQ5332/q6_fw0.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ssr_name = "q6wcss",
	.ops = &q6_wcss_ipq5018_ops,
	.version = Q6_IPQ,
	.glink_subdev_required = true,
	.pasid = RPD_SWID,
	.bootargs_version = VERSION2,
};

static const struct wcss_data q6_ipq9574_res_init = {
	.init_irq = qcom_q6v5_init,
	.q6_firmware_name = "IPQ9574/q6_fw.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ssr_name = "q6wcss",
	.ops = &q6_wcss_ipq5018_ops,
	.version = Q6_IPQ,
	.glink_subdev_required = true,
	.pasid = WCNSS_PAS_ID,
};

static const struct wcss_data wcss_ahb_ipq5018_res_init = {
	.init_irq = init_irq,
	.q6_firmware_name = "IPQ5018/q6_fw.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ops = &wcss_ahb_pcie_ipq5018_ops,
	.version = WCSS_AHB_IPQ,
	.pasid = MPD_WCNSS_PAS_ID,
	.reset_seq = true,
	.mdt_load_sec = qcom_mdt_load_pd_seg,
	.is_fw_shared = true,
	.powerup_scm = qti_scm_int_radio_powerup,
	.powerdown_scm = qti_scm_int_radio_powerdown,
};

static const struct wcss_data wcss_ahb_ipq5332_res_init = {
	.init_irq = init_irq,
	.q6_firmware_name = "IPQ5332/q6_fw1.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ops = &wcss_ahb_pcie_ipq5018_ops,
	.version = WCSS_AHB_IPQ,
	.reset_seq = true,
	.mdt_load_sec = qcom_mdt_load,
	.powerup_scm = qcom_scm_pas_auth_and_reset,
	.powerdown_scm = qcom_scm_pas_shutdown,
};

static const struct wcss_data wcss_ahb_ipq9574_res_init = {
	.q6_firmware_name = "IPQ9574/q6_fw.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ops = &wcss_ahb_pcie_ipq5018_ops,
	.version = WCSS_AHB_IPQ,
	.pasid = WCNSS_PAS_ID,
	.mdt_load_sec = qcom_mdt_load,
	.is_fw_shared = true,
};

static const struct wcss_data wcss_pcie_ipq5018_res_init = {
	.init_irq = init_irq,
	.q6_firmware_name = "IPQ5018/q6_fw.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.ops = &wcss_ahb_pcie_ipq5018_ops,
	.version = WCSS_PCIE_IPQ,
	.mdt_load_sec = qcom_mdt_load_pd_seg,
	.pasid = MPD_WCNSS_PAS_ID,
	.is_fw_shared = true,
};

static const struct wcss_data wcss_pcie_ipq5332_res_init = {
	.init_irq = init_irq,
	.q6_firmware_name = "IPQ5332/q6_fw2.mdt",
	.crash_reason_smem = WCSS_CRASH_REASON,
	.remote_id = WCSS_SMEM_HOST,
	.ops = &wcss_ahb_pcie_ipq5018_ops,
	.version = WCSS_PCIE_IPQ,
	.reset_seq = true,
	.mdt_load_sec = qcom_mdt_load,
	.powerup_scm = qcom_scm_pas_auth_and_reset,
	.powerdown_scm = qcom_scm_pas_shutdown,
};

static const struct wcss_data wcss_text_ipq5332_res_init = {
	.q6_firmware_name = "IPQ5332/q6_fw4.mdt",
	.ops = &wcss_ahb_pcie_ipq5018_ops,
	.version = WCSS_AHB_IPQ,
	.reset_seq = true,
	.mdt_load_sec = qcom_mdt_load,
	.powerup_scm = qcom_scm_pas_auth_and_reset,
	.powerdown_scm = qcom_scm_pas_shutdown,
};

static const struct of_device_id q6_wcss_of_match[] = {
	{ .compatible = "qcom,ipq5018-q6-mpd", .data = &q6_ipq5018_res_init },
	{ .compatible = "qcom,ipq5332-q6-mpd", .data = &q6_ipq5332_res_init },
	{ .compatible = "qcom,devsoc-q6-mpd", .data = &q6_devsoc_res_init },
	{ .compatible = "qcom,ipq9574-q6-mpd", .data = &q6_ipq9574_res_init },
	{ .compatible = "qcom,ipq5018-wcss-ahb-mpd",
		.data = &wcss_ahb_ipq5018_res_init },
	{ .compatible = "qcom,ipq5332-wcss-ahb-mpd",
		.data = &wcss_ahb_ipq5332_res_init },
	{ .compatible = "qcom,devsoc-wcss-ahb-mpd",
		.data = &wcss_ahb_ipq5332_res_init },
	{ .compatible = "qcom,ipq9574-wcss-ahb-mpd",
		.data = &wcss_ahb_ipq9574_res_init },
	{ .compatible = "qcom,ipq5018-wcss-pcie-mpd",
		.data = &wcss_pcie_ipq5018_res_init },
	{ .compatible = "qcom,ipq5332-wcss-pcie-mpd",
		.data = &wcss_pcie_ipq5332_res_init },
	{ .compatible = "qcom,ipq5332-mpd-upd-text",
		.data = &wcss_text_ipq5332_res_init },
	{ },
};
MODULE_DEVICE_TABLE(of, q6_wcss_of_match);

static struct platform_driver q6_wcss_driver = {
	.probe = q6_wcss_probe,
	.remove = q6_wcss_remove,
	.driver = {
		.name = "qcom-q6-mpd",
		.of_match_table = q6_wcss_of_match,
	},
};
module_platform_driver(q6_wcss_driver);
module_param(debug_wcss, int, 0644);
#ifdef CONFIG_QCOM_NON_SECURE_PIL
module_param(userpd_bootaddr, long, 0644);
module_param(userpd_size, long, 0644);
#endif
MODULE_DESCRIPTION("Hexagon WCSS Multipd Peripheral Image Loader");
MODULE_LICENSE("GPL v2");
