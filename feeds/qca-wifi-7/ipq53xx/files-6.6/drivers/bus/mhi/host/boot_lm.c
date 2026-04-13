// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mhi.h>
#include "internal.h"
#include <soc/qcom/license_manager.h>

#define PCIE_PCIE_LOCAL_REG_PCIE_LOCAL_RSV1     0x3168
#define PCIE_SOC_PCIE_REG_PCIE_SCRATCH_0	0x4040
#define PCIE_REMAP_BAR_CTRL_OFFSET              0x310C
#define PCIE_SCRATCH_0_WINDOW_VAL		0x4000003C
#define MAX_UNWINDOWED_ADDRESS                  0x80000
#define WINDOW_ENABLE_BIT                       0x40000000
#define WINDOW_SHIFT                            19
#define WINDOW_VALUE_MASK                       0x3F
#define WINDOW_START                            MAX_UNWINDOWED_ADDRESS
#define WINDOW_RANGE_MASK                       0x7FFFF
#define PCIE_REG_FOR_BOOT_ARGS			PCIE_SOC_PCIE_REG_PCIE_SCRATCH_0

#define NONCE_SIZE                              34
#define CBOR_REQ_MAGIC				"SFID"
#define CBOR_REQ_SIZE				2048

static int mhi_select_window(struct mhi_controller *mhi_cntrl, u32 addr)
{
	u32 window = (addr >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;
	u32 prev_window = 0, curr_window = 0;
	u32 read_val = 0;
	int retry = 0;
	int ret;

	 ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, PCIE_REMAP_BAR_CTRL_OFFSET, &prev_window);
	 if (ret)
		 return ret;

	/* Using the last 6 bits for Window 1. Window 2 and 3 are unaffected */
	curr_window = (prev_window & ~(WINDOW_VALUE_MASK)) | window;

	if (curr_window == prev_window)
		return 0;

	curr_window |= WINDOW_ENABLE_BIT;

	mhi_write_reg(mhi_cntrl, mhi_cntrl->regs, PCIE_REMAP_BAR_CTRL_OFFSET, curr_window);

	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, PCIE_REMAP_BAR_CTRL_OFFSET, &read_val);
	if (ret)
		return ret;

	/* Wait till written value reflects */
	while((read_val != curr_window) && (retry < 10)) {
		mdelay(1);
		ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, PCIE_REMAP_BAR_CTRL_OFFSET, &read_val);
		if (ret)
			return ret;
		retry++;
	}

	if(read_val != curr_window)
		return -EINVAL;

	return 0;
}

void mhi_free_nonce_buffer(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	if (mhi_cntrl->nonce_buf != NULL) {
		dma_free_coherent(dev, NONCE_SIZE, mhi_cntrl->nonce_buf,
				mhi_cntrl->nonce_dma_addr);
		mhi_cntrl->nonce_buf = NULL;
	}

	if (mhi_cntrl->cbor_req != NULL) {
		kfree(mhi_cntrl->cbor_req);
		mhi_cntrl->cbor_req = NULL;
	}
}

static int mhi_get_nonce(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	unsigned int sram_addr, rd_addr;
	unsigned int rd_val;
	int ret, i;

	dev_info(dev, "Reading NONCE from Endpoint\n");

	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, PCIE_PCIE_LOCAL_REG_PCIE_LOCAL_RSV1,
			&sram_addr);
	if (ret)
		return ret;
	if (sram_addr != 0) {
		mhi_cntrl->nonce_buf = dma_alloc_coherent(mhi_cntrl->cntrl_dev, NONCE_SIZE,
							  &mhi_cntrl->nonce_dma_addr, GFP_KERNEL);
		if (!mhi_cntrl->nonce_buf) {
			dev_err(dev, "Error Allocating memory buffer for NONCE\n");
			return -ENOMEM;
		}

		/* Select window to read the NONCE from Q6 SRAM address */
		ret = mhi_select_window(mhi_cntrl, sram_addr);
		if (ret)
			return ret;

		for (i=0; i < NONCE_SIZE; i+=4) {
			/* Calculate read address based on the Window range and read it */
			rd_addr = ((sram_addr + i) & WINDOW_RANGE_MASK) + WINDOW_START;
			ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, rd_addr, &rd_val);
			if (ret)
				return ret;

			/* Copy the read value to nonce_buf */
			memcpy(mhi_cntrl->nonce_buf + i, &rd_val, 4);
		}

		/* Check for CBOR request which is in TLV format */
		/* Check for Type "SFID" */
		sram_addr += NONCE_SIZE + 2; //2 Bytes reserved
		rd_addr = (sram_addr & WINDOW_RANGE_MASK) + WINDOW_START;
		ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, rd_addr, &rd_val);
		if (ret)
			return ret;

		if (strncmp((const char *)&rd_val, CBOR_REQ_MAGIC, 4))
			return 0;

		/* Get the CBOR Length */
		sram_addr += 4;
		rd_addr = (sram_addr & WINDOW_RANGE_MASK) + WINDOW_START;
		ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, rd_addr, &mhi_cntrl->cbor_req_len);
		if (ret)
			return ret;

		if (!mhi_cntrl->cbor_req_len || mhi_cntrl->cbor_req_len > CBOR_REQ_SIZE) {
			dev_err(dev, "CBOR_NONCE length is invalid %u\n", mhi_cntrl->cbor_req_len);
			return -EIO;
		}

		/* Allocate memory and read the CBOR_REQ */
		mhi_cntrl->cbor_req = kzalloc(CBOR_REQ_SIZE, GFP_KERNEL);
		if (!mhi_cntrl->cbor_req)
			return -ENOMEM;

		/* Get the CBOR Value */
		sram_addr += 4;
		for (i=0; i < mhi_cntrl->cbor_req_len; i+=4) {
			/* Calculate read address based on the Window range and read it */
			rd_addr = ((sram_addr + i) & WINDOW_RANGE_MASK) + WINDOW_START;
			ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, rd_addr, &rd_val);
			if (ret)
				return ret;

			/* Copy the read value to cbor_req buffer */
			memcpy(mhi_cntrl->cbor_req + i, &rd_val, 4);
		}
	}
	else {
		dev_err(dev, "No NONCE from device\n");
		mhi_cntrl->nonce_buf = NULL;
		mhi_cntrl->nonce_dma_addr = 0;
	}

	return 0;
}

void mhi_download_fw_license(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	int  ret;

	ret = mhi_get_nonce(mhi_cntrl);
	if (ret) {
		mhi_cntrl->nonce_dma_addr = 0;
		mhi_free_nonce_buffer(mhi_cntrl);
	}

	mhi_cntrl->license_buf = lm_get_license(EXTERNAL, &mhi_cntrl->license_dma_addr,
						&mhi_cntrl->license_buf_size,
						mhi_cntrl->nonce_dma_addr,
						mhi_cntrl->cbor_req,
						mhi_cntrl->cbor_req_len);

	if (!mhi_cntrl->license_buf) {
		mhi_free_nonce_buffer(mhi_cntrl);
		mhi_write_reg(mhi_cntrl, mhi_cntrl->regs,
				PCIE_PCIE_LOCAL_REG_PCIE_LOCAL_RSV1, (u32)0x0);
		dev_info(dev, "No license file passed in RSV1\n");
		return;
	}

	/* Let device know the about license data. Assuming 32 bit only*/
	mhi_write_reg(mhi_cntrl, mhi_cntrl->regs,
				PCIE_PCIE_LOCAL_REG_PCIE_LOCAL_RSV1,
					      lower_32_bits(mhi_cntrl->license_dma_addr));
	dev_dbg(dev, "DMA address 0x%x is copied to EP's RSV1\n",lower_32_bits(mhi_cntrl->license_dma_addr));

	dev_info(dev, "License file address copied to PCIE_PCIE_LOCAL_REG_PCIE_LOCAL_RSV1\n");

	return;
}

static int mhi_update_scratch_reg(struct mhi_controller *mhi_cntrl, u32 val)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	u32 rd_val;
	int ret = 0;

	/* Program Window register to update boot args pointer */
	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs, PCIE_REMAP_BAR_CTRL_OFFSET,
			&rd_val);
	if (ret)
		return ret;

	rd_val = rd_val & ~(0x3f);

	mhi_write_reg(mhi_cntrl, mhi_cntrl->regs, PCIE_REMAP_BAR_CTRL_OFFSET,
				PCIE_SCRATCH_0_WINDOW_VAL | rd_val);

	mhi_write_reg(mhi_cntrl, mhi_cntrl->regs + MAX_UNWINDOWED_ADDRESS,
			PCIE_REG_FOR_BOOT_ARGS, val);

	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->regs + MAX_UNWINDOWED_ADDRESS,
			PCIE_REG_FOR_BOOT_ARGS,	&rd_val);
	if (ret)
		return ret;

	if (rd_val != val) {
		dev_err(dev, "Write to PCIE_REG_FOR_BOOT_ARGS register failed\n");
		return -EFAULT;
	}

	return 0;
}

int mhi_handle_boot_args(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	dma_addr_t bootargs_dma;
	int i, cnt, ret;
	u32 val;

	if (!mhi_cntrl->cntrl_dev->of_node)
		return -EINVAL;

	cnt = of_property_count_u32_elems(mhi_cntrl->cntrl_dev->of_node,
					  "boot-args");
	if (cnt < 0) {
		dev_err(dev, "boot-args not defined in DTS. ret:%d\n", cnt);
		mhi_update_scratch_reg(mhi_cntrl, 0);
		return 0;
	}

	/* Allocate page from DMA Zone - 32bit DMA Address */
	mhi_cntrl->bootargs_buf = (u8 *)__get_dma_pages(GFP_KERNEL, 0);
	if (!mhi_cntrl->bootargs_buf) {
		mhi_update_scratch_reg(mhi_cntrl, 0);
		return -ENOMEM;
	}

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_u32_index(mhi_cntrl->cntrl_dev->of_node,
							"boot-args", i, &val);
		if (ret) {
			dev_err(dev, "failed to read boot args\n");
			free_pages((unsigned long)mhi_cntrl->bootargs_buf, 0);
			mhi_cntrl->bootargs_buf = NULL;
			mhi_update_scratch_reg(mhi_cntrl, 0);
			return ret;
		}
		mhi_cntrl->bootargs_buf[i] = (u8)val;
	}

	bootargs_dma = virt_to_phys(mhi_cntrl->bootargs_buf);

	dma_sync_single_for_device(mhi_cntrl->cntrl_dev, bootargs_dma, PAGE_SIZE, DMA_TO_DEVICE);

	ret = mhi_update_scratch_reg(mhi_cntrl, bootargs_dma);

	dev_dbg(dev, "boot-args address %pad copied to PCIE_REG_FOR_BOOT_ARGS\n", &bootargs_dma);

	return ret;
}

void mhi_free_boot_args(struct mhi_controller *mhi_cntrl)
{
	if (mhi_cntrl->bootargs_buf != NULL) {
		free_pages((unsigned long)mhi_cntrl->bootargs_buf, 0);
		mhi_cntrl->bootargs_buf = NULL;
	}
}
