/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Airoha Inc.
 * Author: Min Yao <min.yao@airoha.com>
 */

#ifndef _AN8855_H_
#define _AN8855_H_

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/of_mdio.h>
#include <linux/workqueue.h>
#include <linux/gpio/consumer.h>
#include <linux/phy.h>

#ifdef CONFIG_SWCONFIG
#include <linux/switch.h>
#endif

#include "an8855_vlan.h"

#define AN8855_DFL_CPU_PORT		5
#define AN8855_NUM_PHYS			5
#define AN8855_DFL_SMI_ADDR		0x1
#define AN8855_SMI_ADDR_MASK	0x1f

struct gsw_an8855;

enum an8855_model {
	AN8855 = 0x8855,
};

enum sgmii_mode {
	SGMII_MODE_AN,
	SGMII_MODE_FORCE,
};

struct an8855_port_cfg {
	struct device_node *np;
	phy_interface_t phy_mode;
	u32 enabled: 1;
	u32 force_link: 1;
	u32 speed: 2;
	u32 duplex: 1;
	bool stag_on;
};

struct gsw_an8855 {
	u32 id;

	struct device *dev;
	struct mii_bus *host_bus;
	u32 smi_addr;
	u32 new_smi_addr;
	u32 phy_base;

	enum an8855_model model;
	const char *name;

	struct an8855_port_cfg port5_cfg;

	int phy_link_sts;

	int irq;
	int reset_pin;
	struct work_struct irq_worker;

#ifdef CONFIG_SWCONFIG
	struct switch_dev swdev;
	u32 cpu_port;
#endif

	int global_vlan_enable;
	struct an8855_vlan_entry vlan_entries[AN8855_NUM_VLANS];
	struct an8855_port_entry port_entries[AN8855_NUM_PORTS];

	int (*mii_read)(struct gsw_an8855 *gsw, int phy, int reg);
	void (*mii_write)(struct gsw_an8855 *gsw, int phy, int reg, u16 val);

	int (*mmd_read)(struct gsw_an8855 *gsw, int addr, int devad, u16 reg);
	void (*mmd_write)(struct gsw_an8855 *gsw, int addr, int devad, u16 reg,
			  u16 val);

	struct list_head list;
};

struct chip_rev {
	const char *name;
	u32 rev;
};

struct an8855_sw_id {
	enum an8855_model model;
	int (*detect)(struct gsw_an8855 *gsw, struct chip_rev *crev);
	int (*init)(struct gsw_an8855 *gsw);
	int (*post_init)(struct gsw_an8855 *gsw);
};

extern struct list_head an8855_devs;
extern struct an8855_sw_id an8855_id;

struct gsw_an8855 *an8855_get_gsw(u32 id);
struct gsw_an8855 *an8855_get_first_gsw(void);
void an8855_put_gsw(void);
void an8855_lock_gsw(void);

u32 an8855_reg_read(struct gsw_an8855 *gsw, u32 reg);
void an8855_reg_write(struct gsw_an8855 *gsw, u32 reg, u32 val);

int an8855_mii_read(struct gsw_an8855 *gsw, int phy, int reg);
void an8855_mii_write(struct gsw_an8855 *gsw, int phy, int reg, u16 val);

int an8855_mmd_read(struct gsw_an8855 *gsw, int addr, int devad, u16 reg);
void an8855_mmd_write(struct gsw_an8855 *gsw, int addr, int devad, u16 reg,
			  u16 val);

void an8855_irq_worker(struct work_struct *work);
void an8855_irq_enable(struct gsw_an8855 *gsw);

#endif /* _AN8855_H_ */
