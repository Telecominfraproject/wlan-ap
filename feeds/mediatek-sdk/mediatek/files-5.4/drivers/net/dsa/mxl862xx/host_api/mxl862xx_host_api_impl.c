// spdx-license-identifier: gpl-2.0
/*
 * drivers/net/dsa/host_api/mxl862xx_host_api_impl.c - dsa driver for maxlinear mxl862xx switch chips family
 *
 * copyright (c) 2024 maxlinear inc.
 *
 * this program is free software; you can redistribute it and/or
 * modify it under the terms of the gnu general public license
 * as published by the free software foundation; either version 2
 * of the license, or (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program; if not, write to the free software
 * foundation, inc., 51 franklin street, fifth floor, boston, ma  02110-1301, usa.
 *
 */

#include "mxl862xx_mmd_apis.h"

#define CTRL_BUSY_MASK BIT(15)
#define CTRL_CMD_MASK (BIT(15) - 1)

#define MAX_BUSY_LOOP 1000 /* roughly 10ms */

#define THR_RST_DATA 5

#define ENABLE_GETSET_OPT 1

//#define C22_MDIO

#if defined(ENABLE_GETSET_OPT) && ENABLE_GETSET_OPT
static struct {
	uint16_t ctrl;
	int16_t ret;
	mmd_api_data_t data;
} shadow = { .ctrl = ~0, .ret = -1, .data = { { 0 } } };
#endif

/* required for Clause 22 extended read/write access */
#define MXL862XX_MMDDATA			0xE
#define MXL862XX_MMDCTRL			0xD

#define MXL862XX_ACTYPE_ADDRESS		(0 << 14)
#define MXL862XX_ACTYPE_DATA			(1 << 14)

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
#include <linux/delay.h>
#endif

#ifdef C22_MDIO
/**
 *  write access to MMD register of PHYs via Clause 22 extended access
 */
static int __mxl862xx_c22_ext_mmd_write(const mxl862xx_device_t *dev, struct mii_bus *bus, int sw_addr, int mmd,
			    int reg, u16 val)
{
	int res;

	/* Set the DevID for Write Command */
	res = __mdiobus_write(bus, sw_addr, MXL862XX_MMDCTRL, mmd);
	if (res < 0)
		goto error;

	/* Issue the write command */
	res = __mdiobus_write(bus, sw_addr, MXL862XX_MMDDATA, reg);
	if (res < 0)
		goto error;

	/* Set the DevID for Write Command */
	res = __mdiobus_write(bus, sw_addr, MXL862XX_MMDCTRL, MXL862XX_ACTYPE_DATA | mmd);
	if (res < 0)
		goto error;

	/* Issue the write command */
	res = __mdiobus_write(bus, sw_addr, MXL862XX_MMDDATA, val);
	if (res < 0)
		goto error;

error:
	return res;
}

/**
 *  read access to MMD register of PHYs via Clause 22 extended access
 */
static int __mxl862xx_c22_ext_mmd_read(const mxl862xx_device_t *dev, struct mii_bus *bus, int sw_addr, int mmd, int reg)
{
	int res;

/* Set the DevID for Write Command */
	res = __mdiobus_write(bus, sw_addr, MXL862XX_MMDCTRL, mmd);
	if (res < 0)
		goto error;

	/* Issue the write command */
	res = __mdiobus_write(bus, sw_addr, MXL862XX_MMDDATA, reg);
	if (res < 0)
		goto error;

	/* Set the DevID for Write Command */
	res = __mdiobus_write(bus, sw_addr, MXL862XX_MMDCTRL, MXL862XX_ACTYPE_DATA | mmd);
	if (res < 0)
		goto error;

	/* Read the data */
	res = __mdiobus_read(bus, sw_addr, MXL862XX_MMDDATA);
	if (res < 0)
		goto error;

error:
	return res;
}
#endif

int mxl862xx_read(const mxl862xx_device_t *dev, uint32_t regaddr)
{
	int mmd = MXL862XX_MMD_DEV;
#ifdef C22_MDIO
	int ret = __mxl862xx_c22_ext_mmd_read(dev, dev->bus, dev->sw_addr, mmd, regaddr);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
	u32 addr = MII_ADDR_C45 | (mmd << 16) | (regaddr & 0xffff);
	int ret = __mdiobus_read(dev->bus, dev->sw_addr, addr);
#else
	int ret = __mdiobus_c45_read(dev->bus, dev->sw_addr, mmd, regaddr);
#endif
#endif
	return ret;
}

int mxl862xx_write(const mxl862xx_device_t *dev, uint32_t regaddr, uint16_t data)
{
	int mmd = MXL862XX_MMD_DEV;
#ifdef C22_MDIO
	int ret = __mxl862xx_c22_ext_mmd_write(dev, dev->bus, dev->sw_addr, mmd, regaddr, data);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
	u32 addr = MII_ADDR_C45 | (mmd << 16) | (regaddr & 0xffff);
	int ret = __mdiobus_write(dev->bus, dev->sw_addr, addr, data);
#else
	int ret = __mdiobus_c45_write(dev->bus, dev->sw_addr, mmd, regaddr, data);
#endif
#endif
	return ret;
}

static int __wait_ctrl_busy(const mxl862xx_device_t *dev)
{
	int ret, i;

	for (i = 0; i < MAX_BUSY_LOOP; i++) {
		ret = mxl862xx_read(dev, MXL862XX_MMD_REG_CTRL);
		if (ret < 0) {
			goto busy_check_exit;
		}

		if (!(ret & CTRL_BUSY_MASK)) {
			ret = 0;
			goto busy_check_exit;
		}

		usleep_range(10, 15);
	}
	ret = -ETIMEDOUT;
busy_check_exit:
	return ret;
}

static int __mxl862xx_rst_data(const mxl862xx_device_t *dev)
{
	int ret;

	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_LEN_RET, 0);
	if (ret < 0)
		return ret;

	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_CTRL,
			MMD_API_RST_DATA | CTRL_BUSY_MASK);
	if (ret < 0)
		return ret;

	return __wait_ctrl_busy(dev);
}

static int __mxl862xx_set_data(const mxl862xx_device_t *dev, uint16_t words)
{
	int ret;
	uint16_t cmd;

	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_LEN_RET,
			MXL862XX_MMD_REG_DATA_MAX_SIZE * sizeof(uint16_t));
	if (ret < 0)
		return ret;

	cmd = words / MXL862XX_MMD_REG_DATA_MAX_SIZE - 1;
	if (!(cmd < 2))
		return -EINVAL;

	cmd += MMD_API_SET_DATA_0;
	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_CTRL, cmd | CTRL_BUSY_MASK);
	if (ret < 0)
		return ret;

	return __wait_ctrl_busy(dev);
}

static int __mxl862xx_get_data(const mxl862xx_device_t *dev, uint16_t words)
{
	int ret;
	uint16_t cmd;

	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_LEN_RET,
			MXL862XX_MMD_REG_DATA_MAX_SIZE * sizeof(uint16_t));
	if (ret < 0)
		return ret;

	cmd = words / MXL862XX_MMD_REG_DATA_MAX_SIZE;
	if (!(cmd > 0 && cmd < 3))
		return -EINVAL;
	cmd += MMD_API_GET_DATA_0;
	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_CTRL, cmd | CTRL_BUSY_MASK);
	if (ret < 0)
		return ret;

	return __wait_ctrl_busy(dev);
}

static int __mxl862xx_send_cmd(const mxl862xx_device_t *dev, uint16_t cmd, uint16_t size,
			  int16_t *presult)
{
	int ret;

	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_LEN_RET, size);
	if (ret < 0) {
		return ret;
	}

	ret = mxl862xx_write(dev, MXL862XX_MMD_REG_CTRL, cmd | CTRL_BUSY_MASK);
	if (ret < 0) {
		return ret;
	}

	ret = __wait_ctrl_busy(dev);
	if (ret < 0) {
		return ret;
	}

	ret = mxl862xx_read(dev, MXL862XX_MMD_REG_LEN_RET);
	if (ret < 0) {
		return ret;
	}

	*presult = ret;
	return 0;
}

static bool __mxl862xx_cmd_r_valid(uint16_t cmd_r)
{
#if defined(ENABLE_GETSET_OPT) && ENABLE_GETSET_OPT
	return (shadow.ctrl == cmd_r && shadow.ret >= 0) ? true : false;
#else
	return false;
#endif
}

/* This is usually used to implement CFG_SET command.
 * With previous CFG_GET command executed properly, the retrieved data
 * are shadowed in local structure. WSP FW has a set of shadow too,
 * so that only the difference to be sent over SMDIO.
 */
static int __mxl862xx_api_wrap_cmd_r(const mxl862xx_device_t *dev, uint16_t cmd,
				void *pdata, uint16_t size, uint16_t r_size)
{
#if defined(ENABLE_GETSET_OPT) && ENABLE_GETSET_OPT
	int ret;

	uint16_t max, i;
	uint16_t *data;
	int16_t result = 0;

	max = (size + 1) / 2;
	data = pdata;

	ret = __wait_ctrl_busy(dev);
	if (ret < 0) {
		return ret;
	}

	for (i = 0; i < max; i++) {
		uint16_t off = i % MXL862XX_MMD_REG_DATA_MAX_SIZE;

		if (i && off == 0) {
			/* Send command to set data when every
			 * MXL862XX_MMD_REG_DATA_MAX_SIZE of WORDs are written
			 * and reload next batch of data from last CFG_GET.
			 */
			ret = __mxl862xx_set_data(dev, i);
			if (ret < 0) {
				return ret;
			}
		}

		if (data[i] == shadow.data.data[i])
			continue;

		mxl862xx_write(dev, MXL862XX_MMD_REG_DATA_FIRST + off,
			  le16_to_cpu(data[i]));
		//sys_le16_to_cpu(data[i]));
	}

	ret = __mxl862xx_send_cmd(dev, cmd, size, &result);
	if (ret < 0) {
		return ret;
	}

	if (result < 0) {
		return result;
	}

	max = (r_size + 1) / 2;
	for (i = 0; i < max; i++) {
		uint16_t off = i % MXL862XX_MMD_REG_DATA_MAX_SIZE;

		if (i && off == 0) {
			/* Send command to fetch next batch of data
			 * when every MXL862XX_MMD_REG_DATA_MAX_SIZE of WORDs
			 * are read.
			 */
			ret = __mxl862xx_get_data(dev, i);
			if (ret < 0) {
				return ret;
			}
		}

		ret = mxl862xx_read(dev, MXL862XX_MMD_REG_DATA_FIRST + off);
		if (ret < 0) {
			return ret;
		}

		if ((i * 2 + 1) == r_size) {
			/* Special handling for last BYTE
			 * if it's not WORD aligned.
			 */
			*(uint8_t *)&data[i] = ret & 0xFF;
		} else {
			data[i] = cpu_to_le16((uint16_t)ret);
		}
	}

	shadow.data.data[max] = 0;
	memcpy(shadow.data.data, data, r_size);

	return result;
#else /* defined(ENABLE_GETSET_OPT) && ENABLE_GETSET_OPT */
	ARG_UNUSED(dev);
	ARG_UNUSED(cmd);
	ARG_UNUSED(pdata);
	ARG_UNUSED(size);
	return -ENOTSUP;
#endif /* defined(ENABLE_GETSET_OPT) && ENABLE_GETSET_OPT */
}

int mxl862xx_api_wrap(const mxl862xx_device_t *dev, uint16_t cmd, void *pdata,
		 uint16_t size, uint16_t cmd_r, uint16_t r_size)
{
	int ret;
	uint16_t max, i, cnt;
	uint16_t *data;
	int16_t result = 0;

	if (!dev || (!pdata && size))
		return -EINVAL;

	if (!(size <= sizeof(mmd_api_data_t)) || !(r_size <= size))
		return -EINVAL;

	mutex_lock_nested(&dev->bus->mdio_lock, MDIO_MUTEX_NESTED);

	if (__mxl862xx_cmd_r_valid(cmd_r)) {
		/* Special handling for GET and SET command pair. */
		ret = __mxl862xx_api_wrap_cmd_r(dev, cmd, pdata, size, r_size);
		goto EXIT;
	}

	max = (size + 1) / 2;
	data = pdata;

	/* Check whether it's worth to issue RST_DATA command. */
	for (i = cnt = 0; i < max && cnt < THR_RST_DATA; i++) {
		if (!data[i])
			cnt++;
	}

	ret = __wait_ctrl_busy(dev);
	if (ret < 0)
		goto EXIT;

	if (cnt >= THR_RST_DATA) {
		/* Issue RST_DATA commdand. */
		ret = __mxl862xx_rst_data(dev);
		if (ret < 0)
			goto EXIT;

		for (i = 0, cnt = 0; i < max; i++) {
			uint16_t off = i % MXL862XX_MMD_REG_DATA_MAX_SIZE;

			if (i && off == 0) {
				uint16_t cnt_old = cnt;

				cnt = 0;

				/* No actual data was written. */
				if (!cnt_old)
					continue;

				/* Send command to set data when every
				 * MXL862XX_MMD_REG_DATA_MAX_SIZE of WORDs are written
				 * and clear the MMD register space.
				 */
				ret = __mxl862xx_set_data(dev, i);
				if (ret < 0)
					goto EXIT;
			}

			/* Skip '0' data. */
			if (!data[i])
				continue;

			mxl862xx_write(dev, MXL862XX_MMD_REG_DATA_FIRST + off,
				  le16_to_cpu(data[i]));
			cnt++;
		}
	} else {
		for (i = 0; i < max; i++) {
			uint16_t off = i % MXL862XX_MMD_REG_DATA_MAX_SIZE;

			if (i && off == 0) {
				/* Send command to set data when every
				 * MXL862XX_MMD_REG_DATA_MAX_SIZE of WORDs are written.
				 */
				ret = __mxl862xx_set_data(dev, i);
				if (ret < 0)
					goto EXIT;
			}

			mxl862xx_write(dev, MXL862XX_MMD_REG_DATA_FIRST + off,
				  le16_to_cpu(data[i]));
		}
	}

	ret = __mxl862xx_send_cmd(dev, cmd, size, &result);
	if (ret < 0)
		goto EXIT;

	if (result < 0) {
		ret = result;
		goto EXIT;
	}

	max = (r_size + 1) / 2;
	for (i = 0; i < max; i++) {
		uint16_t off = i % MXL862XX_MMD_REG_DATA_MAX_SIZE;

		if (i && off == 0) {
			/* Send command to fetch next batch of data
			 * when every MXL862XX_MMD_REG_DATA_MAX_SIZE of WORDs
			 * are read.
			 */
			ret = __mxl862xx_get_data(dev, i);
			if (ret < 0)
				goto EXIT;
		}

		ret = mxl862xx_read(dev, MXL862XX_MMD_REG_DATA_FIRST + off);
		if (ret < 0)
			goto EXIT;

		if ((i * 2 + 1) == r_size) {
			/* Special handling for last BYTE
			 * if it's not WORD aligned.
			 */
			*(uint8_t *)&data[i] = ret & 0xFF;
		} else {
			data[i] = cpu_to_le16((uint16_t)ret);
		}
	}

#if defined(ENABLE_GETSET_OPT) && ENABLE_GETSET_OPT
	if ((cmd != 0x1801) && (cmd != 0x1802))
		shadow.data.data[max] = 0;
	memcpy(shadow.data.data, data, r_size);
#endif

	ret = result;

EXIT:
#if defined(ENABLE_GETSET_OPT) && ENABLE_GETSET_OPT
	shadow.ctrl = cmd;
	shadow.ret = ret;
#endif
	mutex_unlock(&dev->bus->mdio_lock);
	return ret;
}
