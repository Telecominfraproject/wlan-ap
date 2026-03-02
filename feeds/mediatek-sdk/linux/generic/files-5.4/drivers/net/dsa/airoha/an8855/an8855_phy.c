// SPDX-License-Identifier:     GPL-2.0+
/*
 * Common part for Airoha AN8855 gigabit switch
 *
 * Copyright (C) 2023 Airoha Inc. All Rights Reserved.
 *
 * Author: Min Yao <min.yao@airoha.com>
 */

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <net/dsa.h>
#include "an8855.h"
#include "an8855_phy.h"

#define AN8855_NUM_PHYS 5

static u32
an8855_phy_read_dev_reg(struct dsa_switch *ds, u32 port_num,
				   u32 dev_addr, u32 reg_addr)
{
	struct an8855_priv *priv = ds->priv;
	u32 phy_val;
	u32 addr;

	addr = MII_ADDR_C45 | (dev_addr << 16) | (reg_addr & 0xffff);
	phy_val = priv->info->phy_read(ds, port_num, addr);

	return phy_val;
}

static void
an8855_phy_write_dev_reg(struct dsa_switch *ds, u32 port_num,
				     u32 dev_addr, u32 reg_addr, u32 write_data)
{
	struct an8855_priv *priv = ds->priv;
	u32 addr;

	addr = MII_ADDR_C45 | (dev_addr << 16) | (reg_addr & 0xffff);

	priv->info->phy_write(ds, port_num, addr, write_data);
}

static void
an8855_switch_phy_write(struct dsa_switch *ds, u32 port_num,
				    u32 reg_addr, u32 write_data)
{
	struct an8855_priv *priv = ds->priv;

	priv->info->phy_write(ds, port_num, reg_addr, write_data);
}

static u32
an8855_switch_phy_read(struct dsa_switch *ds, u32 port_num,
				  u32 reg_addr)
{
	struct an8855_priv *priv = ds->priv;

	return priv->info->phy_read(ds, port_num, reg_addr);
}

static void
an8855_phy_setting(struct dsa_switch *ds)
{
	struct an8855_priv *priv = ds->priv;
	int i;
	u32 val;

	/* Release power down */
	an8855_write(priv, RG_GPHY_AFE_PWD, 0x0);
	for (i = 0; i < AN8855_NUM_PHYS; i++) {
		/* Enable HW auto downshift */
		an8855_switch_phy_write(ds, i, 0x1f, 0x1);
		val = an8855_switch_phy_read(ds, i, PHY_EXT_REG_14);
		val |= PHY_EN_DOWN_SHFIT;
		an8855_switch_phy_write(ds, i, PHY_EXT_REG_14, val);
		an8855_switch_phy_write(ds, i, 0x1f, 0x0);

		/* Enable Asymmetric Pause Capability */
		val = an8855_switch_phy_read(ds, i, MII_ADVERTISE);
		val |= ADVERTISE_PAUSE_ASYM;
		an8855_switch_phy_write(ds, i, MII_ADVERTISE, val);
	}
}

static void
an8855_low_power_setting(struct dsa_switch *ds)
{
	int port, addr;

	for (port = 0; port < AN8855_NUM_PHYS; port++) {
		an8855_phy_write_dev_reg(ds, port, 0x1e, 0x11, 0x0f00);
		an8855_phy_write_dev_reg(ds, port, 0x1e, 0x3c, 0x0000);
		an8855_phy_write_dev_reg(ds, port, 0x1e, 0x3d, 0x0000);
		an8855_phy_write_dev_reg(ds, port, 0x1e, 0x3e, 0x0000);
		an8855_phy_write_dev_reg(ds, port, 0x1e, 0xc6, 0x53aa);
	}

	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x268, 0x07f1);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x269, 0x2111);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x26a, 0x0000);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x26b, 0x0074);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x26e, 0x00f6);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x26f, 0x6666);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x271, 0x2c02);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x272, 0x0c22);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x700, 0x0001);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x701, 0x0803);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x702, 0x01b6);
	an8855_phy_write_dev_reg(ds, 0, 0x1f, 0x703, 0x2111);

	an8855_phy_write_dev_reg(ds, 1, 0x1f, 0x700, 0x0001);

	for (addr = 0x200; addr <= 0x230; addr += 2)
		an8855_phy_write_dev_reg(ds, 0, 0x1f, addr, 0x2020);

	for (addr = 0x201; addr <= 0x231; addr += 2)
		an8855_phy_write_dev_reg(ds, 0, 0x1f, addr, 0x0020);
}

static void
an8855_eee_setting(struct dsa_switch *ds, u32 port)
{
	/* Disable EEE */
	an8855_phy_write_dev_reg(ds, port, PHY_DEV07, PHY_DEV07_REG_03C, 0);
}

int
an8855_phy_setup(struct dsa_switch *ds)
{
	int ret = 0;
	int i;

	an8855_phy_setting(ds);

	for (i = 0; i < AN8855_NUM_PHYS; i++)
		an8855_eee_setting(ds, i);

	return ret;
}
