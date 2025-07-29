// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/net/dsa/mxl862xx.c - DSA Driver for MaxLinear Mxl862xx switch chips family
 *
 * Copyright (C) 2024 MaxLinear Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/phy.h>
#include <net/dsa.h>
#include <linux/dsa/8021q.h>
#include "host_api/mxl862xx_api.h"
#include "host_api/mxl862xx_mmd_apis.h"
#include "host_api/mxl862xx_mdio_relay.h"

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#endif


#define MAX_BRIDGES 16
#define MAX_PORTS 13
#define MAX_VLAN_ENTRIES (1024-160)
#define IDX_INVAL (-1)

#define VID_RULES 2
#define INGRESS_FINAL_RULES 5
#define INGRESS_VID_RULES VID_RULES
#define EGRESS_FINAL_RULES 3
#define EGRESS_VID_RULES VID_RULES
/* It's only the size of the array for storing VLAN info.
 * The real number of simultaneous VLANS is lower
 * and depends on the number of filtering rules and ports.
 * It is calculated dynamically at runtime. */
#define MAX_VLANS  100
#define MAX_RULES_RECYCLED MAX_VLANS


/* Index of the array is bridgeID */
u16 mxl862xx_bridge_portmap[MAX_BRIDGES] = { 0 };

struct mxl862xx_hw_info {
	uint8_t max_ports;
	uint8_t phy_ports;
	uint8_t cpu_port;
	const struct dsa_switch_ops *ops;
};

struct mxl862xx_filter_ids {
	int16_t filters_idx[VID_RULES];
	bool valid;
};

struct mxl862xx_vlan {
	bool used;
	uint16_t vid;
	/* indexes of filtering rules(entries) used for this VLAN */
	int16_t filters_idx[VID_RULES];
	/* Indicates if tags are added for eggress direction. Makes sense only in egress block */
	bool untagged;
};

struct mxl862xx_extended_vlan_block_info {
	bool allocated;
	/* current index of the VID related filtering rules  */
	uint16_t vid_filters_idx;
	/* current index of the final filtering rules;
	 * counted backwards starting from the block end */
	uint16_t final_filters_idx;
	/* number of allocated  entries for filtering rules */
	uint16_t filters_max;
	/* number of entries per vlan */
	uint16_t entries_per_vlan;
	uint16_t block_id;
	/* use this for storing indexes of vlan entries
	 * for VLAN delete */
	struct mxl862xx_vlan vlans[MAX_VLANS];
	/* collect released filter entries (pairs) that can be reused */
	struct mxl862xx_filter_ids filter_entries_recycled[MAX_VLANS];
};

struct mxl862xx_port_vlan_info {
	uint16_t pvid;
	bool filtering;
	/* Allow one-time initial vlan_filtering port attribute configuration. */
	bool filtering_mode_locked;
	/* Only one block can be assigned per port and direction. Take care about releasing the previous one when
	 * overwriting with the new one.*/
	struct mxl862xx_extended_vlan_block_info ingress_vlan_block_info;
	struct mxl862xx_extended_vlan_block_info egress_vlan_block_info;
};

struct mxl862xx_port_info {
	bool port_enabled;
	bool isolated;
	uint16_t bridgeID;
	uint16_t bridge_port_cpu;
	struct net_device *bridge;
	enum dsa_tag_protocol tag_protocol;
	bool ingress_mirror_enabled;
	bool egress_mirror_enabled;
	struct mxl862xx_port_vlan_info vlan;
};

struct mxl862xx_priv {
	struct mii_bus *bus;
	struct device *dev;
	int sw_addr;
	const struct mxl862xx_hw_info *hw_info;
	struct mxl862xx_port_info port_info[MAX_PORTS];
	/* Number of simultaneously supported vlans (calculated in the runtime) */
	uint16_t max_vlans;
#if (KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE)
	/* pce_table_lock required for kernel 5.16 or later,
	 * since rtnl_lock has been dropped from DSA.port_fdb_{add,del}
	 * might cause dead-locks / hang in previous versions */
	struct mutex pce_table_lock;
#endif
};

/* The mxl862_8021q 4-byte tagging is not yet supported in
 * kernels >= 5.16 due to differences in DSA 8021q tagging handlers.
 * DSA tx/rx vid functions are not avaliable, so dummy
 * functions are here to make the code compilable. */
#if (KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE)
static u16 dsa_8021q_rx_vid(struct dsa_switch *ds, int port)
{
	return 0;
}

static u16 dsa_8021q_tx_vid(struct dsa_switch *ds, int port)
{
	return 0;
}
#endif

#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
static int mxl862xx_port_bridge_join(struct dsa_switch *ds, int port,
				       struct net_device *bridge);
static void mxl862xx_port_bridge_leave(struct dsa_switch *ds, int port,
				       struct net_device *bridge);
#else
static int mxl862xx_port_bridge_join(struct dsa_switch *ds, int port,
				   struct dsa_bridge br,
				   bool *tx_fwd_offload,
				   struct netlink_ext_ack *extack);
static void mxl862xx_port_bridge_leave(struct dsa_switch *ds, int port,
				       struct dsa_bridge br);
#endif

static int __update_bridge_conf_port(struct dsa_switch *ds, uint8_t port,
				     struct net_device *bridge, int action);

static int __deactivate_vlan_filter_entry(u16 block_id, u16 entry_idx);

static int __prepare_vlan_ingress_filters_sp_tag_cpu(struct dsa_switch *ds,
						uint8_t port, uint8_t cpu_port);
static int __prepare_vlan_egress_filters_off_sp_tag_cpu(struct dsa_switch *ds,
						uint8_t cpu_port);
static int __prepare_vlan_ingress_filters_off_sp_tag_no_vid(struct dsa_switch *ds,
						uint8_t port);
static int __prepare_vlan_egress_filters_off_sp_tag_no_vid(struct dsa_switch *ds,
						uint8_t port);

#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
static void mxl862xx_port_vlan_add(struct dsa_switch *ds, int port,
				  const struct switchdev_obj_port_vlan *vlan);
#else
static int mxl862xx_port_vlan_add(struct dsa_switch *ds, int port,
				  const struct switchdev_obj_port_vlan *vlan,
				  struct netlink_ext_ack *extack);
#endif
static int mxl862xx_port_vlan_del(struct dsa_switch *ds, int port,
				  const struct switchdev_obj_port_vlan *vlan);

static mxl862xx_device_t mxl_dev;

struct mxl862xx_rmon_cnt_desc {
	unsigned int size;
	unsigned int offset;
	const char *name;
};

#define MIB_DESC(_name)       \
	{                     \
		.name = _name \
	}

static const struct mxl862xx_rmon_cnt_desc mxl862xx_rmon_cnt[] = {
	MIB_DESC("RxGoodPkts"), //0
	MIB_DESC("RxUnicastPkts"),
	MIB_DESC("RxBroadcastPkts"),
	MIB_DESC("RxMulticastPkts"),
	MIB_DESC("RxFCSErrorPkts"),
	MIB_DESC("RxUnderSizeGoodPkts"),
	MIB_DESC("RxOversizeGoodPkts"),
	MIB_DESC("RxUnderSizeErrorPkts"),
	MIB_DESC("RxOversizeErrorPkts"),
	MIB_DESC("RxFilteredPkts"),
	MIB_DESC("Rx64BytePkts"), //10
	MIB_DESC("Rx127BytePkts"),
	MIB_DESC("Rx255BytePkts"),
	MIB_DESC("Rx511BytePkts"),
	MIB_DESC("Rx1023BytePkts"),
	MIB_DESC("RxMaxBytePkts"),
	MIB_DESC("RxDroppedPkts"),
	MIB_DESC("RxExtendedVlanDiscardPkts"),
	MIB_DESC("MtuExceedDiscardPkts"),
	MIB_DESC("RxGoodBytes"),
	MIB_DESC("RxBadBytes"), //20
	MIB_DESC("RxUnicastPktsYellowRed"),
	MIB_DESC("RxBroadcastPktsYellowRed"),
	MIB_DESC("RxMulticastPktsYellowRed"),
	MIB_DESC("RxGoodPktsYellowRed"),
	MIB_DESC("RxGoodBytesYellowRed"),
	MIB_DESC("RxGoodPausePkts"),
	MIB_DESC("RxAlignErrorPkts"),
	//-----------------------------
	MIB_DESC("TxGoodPkts"),
	MIB_DESC("TxUnicastPkts"),
	MIB_DESC("TxBroadcastPkts"), //30
	MIB_DESC("TxMulticastPkts"),
	MIB_DESC("Tx64BytePkts"),
	MIB_DESC("Tx127BytePkts"),
	MIB_DESC("Tx255BytePkts"),
	MIB_DESC("Tx511BytePkts"),
	MIB_DESC("Tx1023BytePkts"),
	MIB_DESC("TxMaxBytePkts"),
	MIB_DESC("TxDroppedPkts"),
	MIB_DESC("TxAcmDroppedPkts"),
	MIB_DESC("TxGoodBytes"), //40
	MIB_DESC("TxUnicastPktsYellowRed"),
	MIB_DESC("TxBroadcastPktsYellowRed"),
	MIB_DESC("TxMulticastPktsYellowRed"),
	MIB_DESC("TxGoodPktsYellowRed"),
	MIB_DESC("TxGoodBytesYellowRed"),
	MIB_DESC("TxSingleCollCount"),
	MIB_DESC("TxMultCollCount"),
	MIB_DESC("TxLateCollCount"),
	MIB_DESC("TxExcessCollCount"),
	MIB_DESC("TxCollCount"), //50
	MIB_DESC("TxPauseCount"),
};

static int mxl862xx_phy_read_mmd(struct dsa_switch *ds, int devnum, int port, int reg)
{
	int ret = MXL862XX_STATUS_ERR;
	struct mdio_relay_data param = { 0 };

	param.phy = port;
	param.mmd = devnum;
	param.reg = reg & 0xffff;

	ret = int_gphy_read(&mxl_dev, &param);
	if (ret == MXL862XX_STATUS_OK)
		goto EXIT;
	else {
		pr_err("%s: reading port: %d failed with err: %d\n",
			__func__, port, ret);
	}

	return ret;

EXIT:
	return param.data;
}

static int mxl862xx_phy_write_mmd(struct dsa_switch *ds, int devnum, int port, int reg,
			      u16 data)
{
	int ret = MXL862XX_STATUS_ERR;
	struct mdio_relay_data param = { 0 };

	param.phy = port;
	param.mmd = devnum;
	param.reg = reg;
	param.data = data;

	ret = int_gphy_write(&mxl_dev, &param);
	if (ret == MXL862XX_STATUS_OK)
		goto EXIT;
	else {
		pr_err("%s: writing port: %d failed with err: %d\n",
			__func__, port, ret);
	}

EXIT:
	return ret;
}

static int mxl862xx_phy_read(struct dsa_switch *ds, int port, int reg)
{
	int ret = mxl862xx_phy_read_mmd(ds, 0, port, reg);
	return ret;
}

static int mxl862xx_phy_write(struct dsa_switch *ds, int port, int reg,
			      u16 data)
{
	int ret = mxl862xx_phy_write_mmd(ds, 0, port, reg, data);
	return ret;
}

static int mxl862xx_mmd_write(const mxl862xx_device_t *dev, int reg, u16 data)
{
	int ret;
	/* lock mdio bus */
	mutex_lock_nested(&dev->bus->mdio_lock, MDIO_MUTEX_NESTED);
	ret = mxl862xx_write(dev, reg, data);
	/* unlock mdio bus */
	mutex_unlock(&dev->bus->mdio_lock);

	return ret;
}

static int __config_mxl862_tag_proto(struct dsa_switch *ds, uint8_t port, bool enable)
{

	int ret = MXL862XX_STATUS_ERR;
	uint8_t pid = port + 1;
	mxl862xx_ss_sp_tag_t param = { 0 };
	mxl862xx_ctp_port_assignment_t ctp_port_assign = { 0 };

	param.pid = pid;
	param.mask = BIT(0) | BIT(1);
	if (enable) {
		param.rx = 2;
		param.tx = 2;
	} else {
		param.rx = 1;
		param.tx = 3;
	}

	ret = mxl862xx_ss_sp_tag_set(&mxl_dev, &param);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
		"%s: Error %d while configuring DSA frame tagging for port:%d\n",
		__func__, ret, port);
		goto EXIT;
	}

	if (enable)
		ctp_port_assign.number_of_ctp_port = 32 - pid;
	else
		ctp_port_assign.number_of_ctp_port = 1;

	ctp_port_assign.logical_port_id = pid;
	ctp_port_assign.first_ctp_port_id = pid;
	ctp_port_assign.mode = MXL862XX_LOGICAL_PORT_GPON;

	ret = mxl862xx_ctp_port_assignment_set(&mxl_dev, &ctp_port_assign);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Error %d while configuring assignment of port %d for DSA frame tagging.\n",
			__func__, ret, port);
		goto EXIT;
	}

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 3, 0) > LINUX_VERSION_CODE)
static void mxl862xx_port_disable(struct dsa_switch *ds, int port, struct phy_device *phy)
#else
static void mxl862xx_port_disable(struct dsa_switch *ds, int port)
#endif
{
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_register_mod_t register_mod = { 0 };
	int ret = 0;

	if (!dsa_is_user_port(ds, port))
		return;

	/* Disable datapath */
	register_mod.reg_addr = MxL862XX_SDMA_PCTRLp(port + 1);
	register_mod.data = 0;
	register_mod.mask = MXL862XX_SDMA_PCTRL_EN;
	ret = mxl862xx_register_mod(&mxl_dev, &register_mod);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: Disable Datapath SDMA failed:%d\n",
			__func__, port);
		return;
	}
	register_mod.reg_addr = MxL862XX_FDMA_PCTRLp(port + 1);
	register_mod.data = 0;
	register_mod.mask = MXL862XX_FDMA_PCTRL_EN;
	ret = mxl862xx_register_mod(&mxl_dev, &register_mod);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: Disable Datapath FDMA failed:%d\n",
			__func__, port);
		return;
	}

	priv->port_info[port].port_enabled = false;
}

static int mxl862xx_port_enable(struct dsa_switch *ds, int port,
				struct phy_device *phydev)
{
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_register_mod_t register_mod = { 0 };
	int ret;

	if (!dsa_is_user_port(ds, port))
		return 0;

	if (!dsa_is_cpu_port(ds, port)) {
		/* Enable datapath */
		register_mod.reg_addr = MxL862XX_SDMA_PCTRLp(port + 1);
		register_mod.data = MXL862XX_SDMA_PCTRL_EN;
		register_mod.mask = MXL862XX_SDMA_PCTRL_EN;
		ret = mxl862xx_register_mod(&mxl_dev, &register_mod);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: Enable Datapath SDMA failed:%d\n",
				__func__, port);
			return ret;
		}
		register_mod.reg_addr = MxL862XX_FDMA_PCTRLp(port + 1);
		register_mod.data = MXL862XX_FDMA_PCTRL_EN;
		register_mod.mask = MXL862XX_FDMA_PCTRL_EN;
		ret = mxl862xx_register_mod(&mxl_dev, &register_mod);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: Enable Datapath FDMA failed:%d\n",
				__func__, port);
			return ret;
		}
	}

	priv->port_info[port].port_enabled = true;

	return 0;
}

static int __isolate_port(struct dsa_switch *ds, uint8_t port)
{
	mxl862xx_bridge_alloc_t br_alloc = { 0 };
	struct mxl862xx_priv *priv = ds->priv;
	int ret = -EINVAL;
	uint8_t cpu_port = priv->hw_info->cpu_port;
	bool vlan_sp_tag = (priv->port_info[cpu_port].tag_protocol == DSA_TAG_PROTO_MXL862_8021Q);

	/* Exit if port is already isolated */
	if (priv->port_info[port].isolated)
		return ret;

	ret = mxl862xx_bridge_alloc(&mxl_dev, &br_alloc);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: bridge alloc failed for port %d\n, ret:%d",
			__func__, port, ret);
		return ret;
	}

	priv->port_info[port].bridgeID = br_alloc.bridge_id;
	priv->port_info[port].bridge = NULL;

	ret = __update_bridge_conf_port(ds, port, NULL, 1);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: bridge port adding failed for port %d, ret %d\n",
			__func__, port, ret);
	}

	/* for VLAN special tagging mode add port to vlan 1 to apply also
	 * the special tag handling filters */
	if (vlan_sp_tag) {
		/* set port vlan 1 untagged */
		struct switchdev_obj_port_vlan vlan;
		uint16_t vid = 1;
		bool filtering_prev = priv->port_info[port].vlan.filtering;

		priv->port_info[port].vlan.filtering = true;
		vlan.flags = BRIDGE_VLAN_INFO_UNTAGGED|BRIDGE_VLAN_INFO_PVID;
#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
		vlan.vid_begin = vid;
		mxl862xx_port_vlan_add(ds, port, &vlan);
#else
		vlan.vid = vid;
		ret = mxl862xx_port_vlan_add(ds, port, &vlan, NULL);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: adding port %d, to vlan:%d failed with ret %d\n",
				__func__, port, vlan.vid, ret);
		}

#endif
		priv->port_info[port].vlan.filtering = filtering_prev;
	}

	priv->port_info[port].isolated = true;
	return ret;
}

static void __deisolate_port(struct dsa_switch *ds, uint8_t port)
{
	mxl862xx_bridge_alloc_t br_alloc = { 0 };
	struct mxl862xx_priv *priv = ds->priv;
	uint8_t cpu_port = priv->hw_info->cpu_port;
	bool vlan_sp_tag = (priv->port_info[cpu_port].tag_protocol == DSA_TAG_PROTO_MXL862_8021Q);
	int ret = -EINVAL;

	/* Check if port is isolated. The isolation with standalone bridges was
	 * done skipping the linux bridge layer so bridge should be NULL */
	if (!priv->port_info[port].isolated)
		return;

	ret = __update_bridge_conf_port(ds, port, NULL, 0);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: bridge port removal failed for port %d, ret %d\n",
			__func__, port, ret);
		return;
	}

	br_alloc.bridge_id = priv->port_info[port].bridgeID;
	ret = mxl862xx_bridge_free(&mxl_dev, &br_alloc);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: bridge free failed for port:%d, BridgeID: %d, ret: %d\n",
			__func__, port, br_alloc.bridge_id, ret);
		return;
	}

	/* For VLAN special tagging mode isolated port is assigned to VLAN 1
	 * to apply also the special tag handling filters. Now for deisolation
	 * VLAN 1 must be unassigned */
	if (vlan_sp_tag) {
		struct switchdev_obj_port_vlan vlan;
		uint16_t vid = 1;
		uint16_t i;

		vlan.flags = BRIDGE_VLAN_INFO_UNTAGGED|BRIDGE_VLAN_INFO_PVID;
#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
		vlan.vid_begin = vid;
#else
		vlan.vid = vid;
#endif
		/* Removes vid dependant 'dynamic' rules */
		ret = mxl862xx_port_vlan_del(ds, port, &vlan);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: deleting port %d, from vlan:%d failed with ret %d\n",
				__func__, port, vid, ret);
		}

		/* Clear/deactivate 'static' set of filtering rules, placed at the end of the block */
		for (i = 0 ; i < 2 ; i++) {
			uint16_t j, start_idx, stop_idx, block_id;
			struct mxl862xx_extended_vlan_block_info *block_info = (i == 0)
				? &priv->port_info[port].vlan.ingress_vlan_block_info
				: &priv->port_info[port].vlan.egress_vlan_block_info;

			block_id = block_info->block_id;
			stop_idx = block_info->filters_max;
			start_idx = block_info->final_filters_idx;

			for (j = start_idx ; j < stop_idx ; j++) {
				ret = __deactivate_vlan_filter_entry(block_id, j);
				if (ret != MXL862XX_STATUS_OK)
					return;
			}
			/* Entries cleared, so point out to the end */
			block_info->final_filters_idx = j;
		}
	}

	priv->port_info[port].isolated = false;

	return;
}

static int __update_bridge_conf_port(struct dsa_switch *ds, uint8_t port,
				     struct net_device *bridge, int action)
{
	int ret = -EINVAL;
	uint8_t i;
	struct mxl862xx_priv *priv = ds->priv;
	uint8_t phy_ports = priv->hw_info->phy_ports;
	uint8_t cpu_port = priv->hw_info->cpu_port;
	bool vlan_sp_tag = (priv->port_info[cpu_port].tag_protocol == DSA_TAG_PROTO_MXL862_8021Q);

	/* Update local bridge port map */
	for (i = 0; i < phy_ports; i++) {
		int bridgeID = priv->port_info[i].bridgeID;

		if (!((struct dsa_port *)dsa_to_port(ds, i)))
			continue;

		/* CPU port is assigned to all bridges and cannot be modified  */
		if ((dsa_is_cpu_port(ds, i)))
			continue;

		/* Skip if bridge does not match, except the self port assignment  */
#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
		if ((dsa_to_port(ds, i)->bridge_dev != bridge) && (i != port))
#else
		if ((dsa_port_bridge_dev_get(dsa_to_port(ds, i)) != bridge) && (i != port))
#endif
			continue;

		/* Case for standalone bridges assigned only to single user and CPU ports.
		 * Used only for initial ports isolation */
		if ((bridge == NULL) && (i != port))
			continue;

		if (action) {
			/* Add port to bridge portmap */
			mxl862xx_bridge_portmap[bridgeID] |= BIT(port + 1);
		} else {
			/* Remove port from the bitmap */
			mxl862xx_bridge_portmap[bridgeID] &= ~BIT(port + 1);
		}
	}

	/* Update switch according to local bridge port map */
	/* Add this port to the port maps of other ports skiping it's own map */
	for (i = 0; i < phy_ports; i++) {
		mxl862xx_bridge_port_config_t br_port_cfg = { 0 };
		int bridgeID = priv->port_info[i].bridgeID;

		if (!(dsa_is_user_port(ds, i)))
			continue;

		/* Case for standalone bridges assigned only to single user and CPU ports.
		 * Used only for initial ports isolation */
		if ((bridge == NULL) && (i != port))
			continue;

		/* Do not reconfigure any standalone bridge if this is bridge join scenario */
		if ((bridge != NULL) && (priv->port_info[i].bridge == NULL))
			continue;

		br_port_cfg.bridge_port_id = i + 1;
		br_port_cfg.mask |=
			MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP |
			MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID;

		ret = mxl862xx_bridge_port_config_get(&mxl_dev, &br_port_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: Bridge port configuration for port %d failed with %d\n",
				__func__, port, ret);
			goto EXIT;
		}

		/* Skip port map update if for the existing bridge the port
		 * is not assigned there. */
		if (i != port && (br_port_cfg.bridge_id != bridgeID ||
				  br_port_cfg.bridge_id == 0))
			continue;

		br_port_cfg.mask |=
			MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP |
			MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID |
			MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING;

		/* Skip the port itself in it's own portmap */
		br_port_cfg.bridge_port_map[0] =
			mxl862xx_bridge_portmap[bridgeID] & ~(BIT(i + 1));

		if (action) {
			/* If bridge is null then this is port isolation scenario. Disable MAC learning. */
			br_port_cfg.src_mac_learning_disable =
				(bridge == NULL) ? true : false;
			br_port_cfg.bridge_id = bridgeID;
		}
		/* When port is removed from the bridge, assign it back to the default
		 * bridge 0 */
		else {
			br_port_cfg.src_mac_learning_disable = true;
			/* Cleanup the port own map leaving only the CPU port mapping. */
			if (i == port) {
				br_port_cfg.bridge_port_map[0] =
					BIT(cpu_port + 1);
				br_port_cfg.bridge_id = 0;
			}
		}

		ret = mxl862xx_bridge_port_config_set(&mxl_dev, &br_port_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: Bridge port configuration for port %d failed with ret=%d\n",
				__func__, port, ret);
			goto EXIT;
		}
	}

	/* Configure additional bridge port for VLAN based tagging */
	if (vlan_sp_tag) {
		int bridgeID = priv->port_info[port].bridgeID;
		uint16_t bridge_port_cpu = port + 1 + 16;
		mxl862xx_bridge_port_alloc_t bpa_param = { 0 };
		mxl862xx_bridge_port_config_t br_port_cfg = { 0 };

		bpa_param.bridge_port_id = bridge_port_cpu;

		if (action) {
			ret = mxl862xx_bridge_port_alloc(&mxl_dev, &bpa_param);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare associated bridge port\n",
					__func__, port);
				goto EXIT;
			}

			br_port_cfg.mask |=
				MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP |
				MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID |
				MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_CTP_MAPPING |
				MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING;
			br_port_cfg.bridge_id = bridgeID;
			br_port_cfg.bridge_port_id = bridge_port_cpu;
			br_port_cfg.bridge_port_map[0] =	BIT(port + 1);
			br_port_cfg.dest_logical_port_id = cpu_port + 1;
			br_port_cfg.src_mac_learning_disable = true;

			ret = mxl862xx_bridge_port_config_set(&mxl_dev, &br_port_cfg);

			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
				"%s: Configuration of cpu bridge port:%d  for port:%d on bridge:%d failed with ret=%d\n",
				__func__, bridge_port_cpu, port, bridgeID, ret);
				goto EXIT;
			}

			/* Add bridge cpu port to portmap */
			mxl862xx_bridge_portmap[bridgeID] |= BIT(bridge_port_cpu);
			priv->port_info[port].bridge_port_cpu = bridge_port_cpu;
		}
		/* remove */
		else {
			ret = mxl862xx_bridge_port_free(&mxl_dev, &bpa_param);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to free associated bridge port\n",
					__func__, port);
				goto EXIT;
			}
			/* Remove bridge cpu port from portmap */
			mxl862xx_bridge_portmap[bridgeID] &= ~BIT(bridge_port_cpu);
			priv->port_info[port].bridge_port_cpu = 0;
		}

	}

EXIT:
	return ret;
}

/* Returns bridgeID, or 0 if bridge not found */
static int __find_bridgeID(struct dsa_switch *ds, struct net_device *bridge)
{
	uint8_t i;
	struct mxl862xx_priv *priv = ds->priv;
	uint8_t phy_ports = priv->hw_info->phy_ports;
	int bridgeID = 0;

	/* The only use case where bridge is NULL is the isolation
	 * with individual port bridges configured in the setup function. */
	if (bridge == NULL)
		return bridgeID;

	for (i = 0; i < phy_ports; i++) {
		if (priv->port_info[i].bridge == bridge) {
			bridgeID = priv->port_info[i].bridgeID;
			break;
		}
	}
	return bridgeID;
}

static void __set_vlan_filters_limits(struct dsa_switch *ds)
{
	uint8_t i;
	uint16_t cpu_ingress_entries;
	uint16_t cpu_egress_entries;
	uint16_t user_ingress_entries;
	uint16_t user_egress_entries;
	struct mxl862xx_priv *priv = ds->priv;
	uint8_t cpu_port = priv->hw_info->cpu_port;

	/* Set limits and indexes required for processing VLAN rules for CPU port */

	/* The calculation of the max number of simultaneously supported VLANS (priv->max_vlans)
	 * comes from the equation:
	 *
	 * MAX_VLAN_ENTRIES = phy_ports * (EGRESS_FINAL_RULES + EGRESS_VID_RULES * priv->max_vlans)
	 *  + phy_ports * (INGRESS_FINAL_RULES + INGRESS_VID_RULES * priv-> max_vlans)
	 *  + cpu_ingress_entries + cpu_egress_entries  */

	if (priv->port_info[cpu_port].tag_protocol == DSA_TAG_PROTO_MXL862_8021Q) {

		priv->max_vlans = (MAX_VLAN_ENTRIES - priv->hw_info->phy_ports *
				(EGRESS_FINAL_RULES + INGRESS_FINAL_RULES + 2) - 3) /
			(priv->hw_info->phy_ports * (EGRESS_VID_RULES + INGRESS_VID_RULES) + 2);
		/* 2 entries per port and 1 entry for fixed rule */
		cpu_ingress_entries = priv->hw_info->phy_ports * 2 + 1;
		/* 2 entries per each vlan and 2 entries for fixed rules */
		cpu_egress_entries = priv->max_vlans * 2 + 2;

		//priv->port_info[cpu_port].vlan.ingress_vlan_block_info.entries_per_vlan = 2;
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.entries_per_vlan = 0;
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.filters_max = cpu_ingress_entries;
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.entries_per_vlan = 2;
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.filters_max = cpu_egress_entries;

		user_ingress_entries = INGRESS_FINAL_RULES + INGRESS_VID_RULES * priv->max_vlans;
		user_egress_entries = EGRESS_FINAL_RULES + EGRESS_VID_RULES * priv->max_vlans;
	} else {
		priv->max_vlans = (MAX_VLAN_ENTRIES - priv->hw_info->phy_ports *
				(EGRESS_FINAL_RULES + INGRESS_FINAL_RULES) - 1) /
			(priv->hw_info->phy_ports * (EGRESS_VID_RULES + INGRESS_VID_RULES) + 2);
		/* 1 entry for fixed rule */
		cpu_ingress_entries =  1;
		/* 2 entries per each vlan  */
		cpu_egress_entries = priv->max_vlans * 2;
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.entries_per_vlan = 0;
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.filters_max = cpu_ingress_entries;
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.entries_per_vlan = 2;
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.filters_max = cpu_egress_entries;

		user_ingress_entries = INGRESS_FINAL_RULES + INGRESS_VID_RULES * priv->max_vlans;
		user_egress_entries = EGRESS_FINAL_RULES + EGRESS_VID_RULES * priv->max_vlans;
	}

	/* This index is counted backwards */
	priv->port_info[cpu_port].vlan.ingress_vlan_block_info.final_filters_idx =
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.filters_max - 1;
	priv->port_info[cpu_port].vlan.egress_vlan_block_info.final_filters_idx =
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.filters_max - 1;

	/* Set limits and indexes required for processing VLAN rules for user ports */
	for (i = 0; i < priv->hw_info->phy_ports; i++) {
		priv->port_info[i].vlan.ingress_vlan_block_info.entries_per_vlan = INGRESS_VID_RULES;
		priv->port_info[i].vlan.ingress_vlan_block_info.filters_max = user_ingress_entries;
		priv->port_info[i].vlan.egress_vlan_block_info.entries_per_vlan = EGRESS_VID_RULES;
		priv->port_info[i].vlan.egress_vlan_block_info.filters_max = user_egress_entries;
		/* This index is counted backwards */
		priv->port_info[i].vlan.ingress_vlan_block_info.final_filters_idx =
			priv->port_info[i].vlan.ingress_vlan_block_info.filters_max - 1;
		priv->port_info[i].vlan.egress_vlan_block_info.final_filters_idx =
			priv->port_info[i].vlan.egress_vlan_block_info.filters_max - 1;
	}
	dev_info(ds->dev, "%s: phy_ports:%d, priv->max_vlans: %d, cpu_egress_entries: %d, user_ingress_entries: %d, INGRESS_VID_RULES: %d\n",
			__func__, priv->hw_info->phy_ports, priv->max_vlans,
			cpu_egress_entries, user_ingress_entries, INGRESS_VID_RULES);
}

static enum dsa_tag_protocol __dt_parse_tag_proto(struct dsa_switch *ds, uint8_t port)
{
	/* Default value if no dt entry found */
	enum dsa_tag_protocol tag_proto = DSA_TAG_PROTO_MXL862;
	struct dsa_port *dp = (struct dsa_port *)dsa_to_port(ds, port);
	const char *user_protocol = NULL;

	if (dp != NULL)
		user_protocol = of_get_property(dp->dn, "dsa-tag-protocol", NULL);
	if (user_protocol != NULL) {
		if (strcmp("mxl862", user_protocol) == 0)
			tag_proto = DSA_TAG_PROTO_MXL862;
		else if (strcmp("mxl862_8021q", user_protocol) == 0)
			tag_proto = DSA_TAG_PROTO_MXL862_8021Q;
	}

	return tag_proto;
}

static int mxl862xx_set_ageing_time(struct dsa_switch *ds, unsigned int msecs)
{
	int ret = -EINVAL;
	mxl862xx_cfg_t cfg = { 0 };

	ret =  mxl862xx_cfg_get(&mxl_dev, &cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: failed to read switch config\n", __func__);
		goto EXIT;
	}

	cfg.mac_table_age_timer = MXL862XX_AGETIMER_CUSTOM;
	cfg.age_timer = msecs / 1000;

	ret =  mxl862xx_cfg_set(&mxl_dev, &cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: failed to configure global MAC ageing value: %ds\n",
			__func__, cfg.age_timer);
	}

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
static int mxl862xx_port_bridge_join(struct dsa_switch *ds, int port,
					struct net_device *bridge)
#else
static int mxl862xx_port_bridge_join(struct dsa_switch *ds, int port,
					struct dsa_bridge br, bool *tx_fwd_offload,
					struct netlink_ext_ack *extack)
#endif

{
#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	struct net_device *bridge = br.dev;
#endif
	struct mxl862xx_priv *priv = ds->priv;
	int bridgeID = 0;
	int ret = -EINVAL;
	uint8_t cpu_port = priv->hw_info->cpu_port;
	bool vlan_sp_tag = (priv->port_info[cpu_port].tag_protocol == DSA_TAG_PROTO_MXL862_8021Q);

	if (port < 0 || port >= MAX_PORTS) {
		dev_err(priv->dev, "invalid port: %d\n", port);
		return ret;
	}

	__deisolate_port(ds, port);

	bridgeID = __find_bridgeID(ds, bridge);

	/* no bridge found -> create new bridge */
	if (bridgeID == 0) {
		mxl862xx_bridge_alloc_t br_alloc = { 0 };

		ret = mxl862xx_bridge_alloc(&mxl_dev, &br_alloc);
		if (ret != MXL862XX_STATUS_OK) {
#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
			NL_SET_ERR_MSG_MOD(extack,
				   "MxL862xx: bridge alloc failed");
#endif
			dev_err(ds->dev,
				"%s: bridge alloc failed for port %d\n, ret:%d",
				__func__, port, ret);
			goto EXIT;
		}
		priv->port_info[port].bridgeID = br_alloc.bridge_id;
		priv->port_info[port].bridge = bridge;
		/* bridge found  */
	} else {
		priv->port_info[port].bridgeID = bridgeID;
		priv->port_info[port].bridge = bridge;
	}

	ret = __update_bridge_conf_port(ds, port, bridge, 1);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: bridge port adding failed for port %d, ret %d\n",
			__func__, port, ret);
		goto EXIT;
	}
	/* For some kernel versions for VLAN unaware bridges Linux calls .port_vlan_add,
	 * for others not. To support VLAN unaware bridge in mxl862_8021q tagging mode,
	 * the required vlan filtering rules (adding sp tag, forwarding to cpu port)
	 * are added here.*/
	if (vlan_sp_tag) {
		mxl862xx_ctp_port_config_t ctp_param = { 0 };
		mxl862xx_bridge_port_config_t br_port_cfg = { 0 };

		ret = __prepare_vlan_ingress_filters_sp_tag_cpu(ds, port, cpu_port);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;
		ret = __prepare_vlan_egress_filters_off_sp_tag_cpu(ds, cpu_port);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;
		ret = __prepare_vlan_ingress_filters_off_sp_tag_no_vid(ds, port);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;
		ret = __prepare_vlan_egress_filters_off_sp_tag_no_vid(ds, port);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		/* update cpu port */
		ctp_param.logical_port_id = cpu_port + 1;
		ctp_param.mask = MXL862XX_CTP_PORT_CONFIG_MASK_EGRESS_VLAN |
				     MXL862XX_CTP_PORT_CONFIG_MASK_INGRESS_VLAN;
		ctp_param.egress_extended_vlan_enable = true;
		ctp_param.egress_extended_vlan_block_id =
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.block_id;
		ctp_param.ingress_extended_vlan_enable = true;
		ctp_param.ingress_extended_vlan_block_id =
			priv->port_info[cpu_port].vlan.ingress_vlan_block_info.block_id;

		ret = mxl862xx_ctp_port_config_set(&mxl_dev, &ctp_param);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: CTP port %d config failed on port config set with %d\n",
				__func__, cpu_port, ret);
#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
			NL_SET_ERR_MSG_MOD(extack, "Failed to configure VLAN for cpu port");
#endif
			goto EXIT;
		}

		/* Update bridge port */
		br_port_cfg.bridge_port_id = port + 1;
		br_port_cfg.mask |= MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN |
			     MXL862XX_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN;
		br_port_cfg.egress_extended_vlan_enable = true;
		br_port_cfg.egress_extended_vlan_block_id =
			priv->port_info[port].vlan.egress_vlan_block_info.block_id;
		br_port_cfg.ingress_extended_vlan_enable = true;
		br_port_cfg.ingress_extended_vlan_block_id =
			priv->port_info[port].vlan.ingress_vlan_block_info.block_id;

		ret = mxl862xx_bridge_port_config_set(&mxl_dev, &br_port_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: Bridge port configuration for port %d failed with %d\n",
				__func__, port, ret);
#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
			NL_SET_ERR_MSG_MOD(extack, "Bridge port configuration for VLAN failed");
#endif
		}
	}

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
static void mxl862xx_port_bridge_leave(struct dsa_switch *ds, int port,
				       struct net_device *bridge)
#else
static void mxl862xx_port_bridge_leave(struct dsa_switch *ds, int port,
				       struct dsa_bridge br)
#endif
{
#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	struct net_device *bridge = br.dev;
#endif
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_bridge_alloc_t br_alloc = { 0 };
	unsigned int cpu_port = priv->hw_info->cpu_port;
	int bridgeID = 0;
	int ret;

	bridgeID = __find_bridgeID(ds, bridge);
	ret = __update_bridge_conf_port(ds, port, bridge, 0);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: bridge port removal failed for port %d, ret %d\n",
			__func__, port, ret);
		goto EXIT;
	}

	/* If only CPU port mapping found, the bridge should be deleted */
	if (mxl862xx_bridge_portmap[bridgeID] == BIT(cpu_port + 1)) {
		br_alloc.bridge_id = priv->port_info[port].bridgeID;
		ret = mxl862xx_bridge_free(&mxl_dev, &br_alloc);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: bridge free failed for port:%d, BridgeID: %d, ret: %d\n",
				__func__, port, br_alloc.bridge_id, ret);
			goto EXIT;
		}
	}

	ret = __isolate_port(ds, port);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: Port%d isolation failed with ret:%d\n",
			__func__, port, ret);
	}

	priv->port_info[port].vlan.filtering_mode_locked = false;

EXIT:
	return;
}


static int mxl862xx_phy_read_mii_bus(struct mii_bus *bus, int addr, int regnum)
{
	int ret = mxl862xx_phy_read_mmd(NULL, 0, addr, regnum);
	return ret;
}


static int mxl862xx_phy_write_mii_bus(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	int ret = mxl862xx_phy_write_mmd(NULL, 0, addr, regnum, val);
	return ret;
}

#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
static int mxl862xx_phy_read_c45_mii_bus(struct mii_bus *bus, int devnum, int addr, int regnum)
{
	int ret = mxl862xx_phy_read_mmd(NULL, devnum, addr, regnum);
	return ret;
}


static int mxl862xx_phy_write_c45_mii_bus(struct mii_bus *bus, int devnum, int addr, int regnum, u16 val)
{
	int ret = mxl862xx_phy_write_mmd(NULL, devnum, addr, regnum, val);
	return ret;
}
#endif

static int
mxl862xx_setup_mdio(struct dsa_switch *ds)
{
	struct device *dev = ds->dev;
	struct mii_bus *bus;
	static int idx;
	int ret;

	bus = devm_mdiobus_alloc(dev);
	if (!bus)
		return -ENOMEM;

#if (KERNEL_VERSION(6, 7, 0) > LINUX_VERSION_CODE)
	ds->slave_mii_bus = bus;
#else
	ds->user_mii_bus = bus;
#endif
	bus->name = KBUILD_MODNAME "-mii";
	snprintf(bus->id, MII_BUS_ID_SIZE, KBUILD_MODNAME "-%d", idx++);
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
	bus->read_c45 = mxl862xx_phy_read_c45_mii_bus;
	bus->write_c45 = mxl862xx_phy_write_c45_mii_bus;
#endif
	bus->read = mxl862xx_phy_read_mii_bus;
	bus->write = mxl862xx_phy_write_mii_bus;
	bus->parent = dev;
	bus->phy_mask = ~ds->phys_mii_mask;

#if (KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE)
	ret = devm_mdiobus_register(dev, bus);
#else
	ret = mdiobus_register(bus);
#endif
	if (ret)
		dev_err(dev, "failed to register MDIO bus: %d\n", ret);

	return ret;
}

static int mxl862xx_setup(struct dsa_switch *ds)
{
	struct mxl862xx_priv *priv = ds->priv;
	unsigned int cpu_port = priv->hw_info->cpu_port;
	int ret = 0;
	uint8_t i;

	ret = mxl862xx_setup_mdio(ds);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: mdio setup failed with %d\n", __func__,
			ret);
		goto EXIT;
	}

	/* Trigger ETHSW_SWRES to re-initiate all previous settings */
	ret = mxl862xx_mmd_write(&mxl_dev, 1, 0);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: mmd write failed with %d\n", __func__,
			ret);
		goto EXIT;
	}

	ret = mxl862xx_mmd_write(&mxl_dev, 0, 0x9907);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: mmd write failed with %d\n", __func__,
			ret);
		goto EXIT;
	}

	/* The reset duration could be improved by implementing
	 * register polling at offset 0, 1, 2 for specific values
	 * 0x20, 0x21, 0x22, indicating that booting is completed. */
	usleep_range(4000000, 6000000);

	priv->port_info[priv->hw_info->cpu_port].tag_protocol =
		__dt_parse_tag_proto(ds, priv->hw_info->cpu_port);

	if (priv->port_info[priv->hw_info->cpu_port].tag_protocol == DSA_TAG_PROTO_MXL862) {
		ret = __config_mxl862_tag_proto(ds, cpu_port, true);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev, "%s: DSA tagging protocol setting failed with  %d\n", __func__,
				ret);
			goto EXIT;
		}
	}

	/* For CPU port MAC learning setting depends on the kernel version. */
	{
		bool lrn_dis;
		mxl862xx_bridge_port_config_t br_port_cfg = { 0 };

		br_port_cfg.bridge_port_id = cpu_port + 1;
		br_port_cfg.mask = MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING;
#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
		lrn_dis = false;
#else
		lrn_dis = true;
#endif
		br_port_cfg.src_mac_learning_disable = lrn_dis;

		ret = mxl862xx_bridge_port_config_set(&mxl_dev, &br_port_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: %s MAC learning for port %d failed with ret=%d\n",
				__func__, lrn_dis ? "Disabled" : "Enabled", cpu_port, ret);
			goto EXIT;
		}
		dev_info(ds->dev, "%s: %s MAC learning for port:%d\n",
			 __func__, lrn_dis ? "Disabled" : "Enabled", cpu_port);
	}

	/* Store bridge portmap in the driver cache.
	 * Add CPU port for each bridge. */
	for (i = 0; i < MAX_BRIDGES; i++)
		mxl862xx_bridge_portmap[i] = BIT(cpu_port + 1);

	__set_vlan_filters_limits(ds);
	/* by default set all vlans on cpu port in untagged mode */
	for (i = 0; i < MAX_VLANS; i++)
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.vlans[i].untagged = true;

	for (i = 0; i < priv->hw_info->phy_ports; i++) {
		mxl862xx_register_mod_t register_mod = { 0 };

		/* unblock vlan_filtering change */
		priv->port_info[i].vlan.filtering_mode_locked = false;
		priv->port_info[i].isolated = false;

		if (dsa_is_cpu_port(ds, i)) {
			dev_info(ds->dev, "%s: cpu port with index :%d\n",
				 __func__, i);
			continue;
		}

		/* disable datapath */
		register_mod.reg_addr = MxL862XX_SDMA_PCTRLp(i + 1);
		register_mod.data = 0;
		register_mod.mask = MXL862XX_SDMA_PCTRL_EN;
		ret = mxl862xx_register_mod(&mxl_dev, &register_mod);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev, "%s: Disable Datapath failed:%d\n",
				__func__, i);
		}

		ret = __isolate_port(ds, i);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: Port%d isolation failed with ret:%d\n",
				__func__, i, ret);
			goto EXIT;
		}
	}

	/* Clear MAC address table */
	ret = mxl862xx_mac_table_clear(&mxl_dev);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: MAC table clear failed\n", __func__);
		goto EXIT;
	}

EXIT:
	return ret;
}

static void mxl862xx_port_stp_state_set(struct dsa_switch *ds, int port,
					u8 state)
{
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_stp_port_cfg_t stp_portCfg;
	int ret;

	if (port < 0 || port >= MAX_PORTS) {
		dev_err(priv->dev, "invalid port: %d\n", port);
		return;
	}

	stp_portCfg.port_id = port + 1;

	switch (state) {
	case BR_STATE_DISABLED:
		stp_portCfg.port_state = MXL862XX_STP_PORT_STATE_DISABLE;
		return;
	case BR_STATE_BLOCKING:
	case BR_STATE_LISTENING:
		stp_portCfg.port_state = MXL862XX_STP_PORT_STATE_BLOCKING;
		break;
	case BR_STATE_LEARNING:
		stp_portCfg.port_state = MXL862XX_STP_PORT_STATE_LEARNING;
		break;
	case BR_STATE_FORWARDING:
		stp_portCfg.port_state = MXL862XX_STP_PORT_STATE_FORWARD;
		break;
	default:
		dev_err(priv->dev, "invalid STP state: %d\n", state);
		return;
	}

	ret = mxl862xx_stp_port_cfg_set(&mxl_dev, &stp_portCfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: stp configuration failed, port: %d, state: %d\n",
			__func__, port, state);
	}

	/* Since MxL862xx firmware enables MAC learning in some STP states,
	 * we have to disable it for isolated user ports. For CPU port,
	 * this setting depends on the kernel version. */
	if ((priv->port_info[port].bridge == NULL) || (dsa_is_cpu_port(ds, port))) {
		mxl862xx_bridge_port_config_t br_port_cfg = { 0 };
		bool lrn_dis;

		if ((dsa_is_cpu_port(ds, port)))
#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
			lrn_dis = false;
#else
			lrn_dis = true;
#endif
		else
			lrn_dis = true;

		br_port_cfg.mask =
			MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING;
		br_port_cfg.bridge_port_id = port + 1;
		br_port_cfg.src_mac_learning_disable = lrn_dis;
		ret = mxl862xx_bridge_port_config_set(&mxl_dev, &br_port_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: %s MAC learning for port %d failed with ret=%d\n",
				__func__, lrn_dis ? "Disabled" : "Enabled", port, ret);
			return;
		}
	}
}

#if (KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE)
#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
static void mxl862xx_phylink_set_capab(unsigned long *supported,
				       struct phylink_link_state *state)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mask) = {
		0,
	};

	/* Allow all the expected bits */
	phylink_set(mask, Autoneg);
	phylink_set_port_modes(mask);
	phylink_set(mask, Pause);
	phylink_set(mask, Asym_Pause);

	phylink_set(mask, 2500baseT_Full);
	phylink_set(mask, 1000baseT_Full);
	phylink_set(mask, 1000baseT_Half);
	phylink_set(mask, 100baseT_Half);
	phylink_set(mask, 100baseT_Full);
	phylink_set(mask, 10baseT_Half);
	phylink_set(mask, 10baseT_Full);

	bitmap_and(supported, supported, mask, __ETHTOOL_LINK_MODE_MASK_NBITS);
	bitmap_and(state->advertising, state->advertising, mask,
		   __ETHTOOL_LINK_MODE_MASK_NBITS);
}
#endif

#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
static void mxl862xx_phylink_validate(struct dsa_switch *ds, int port,
				      unsigned long *supported,
				      struct phylink_link_state *state)
{
	struct mxl862xx_priv *priv = ds->priv;

	if (port >= 0 && port < priv->hw_info->phy_ports) {
		if (state->interface != PHY_INTERFACE_MODE_INTERNAL)
			goto unsupported;
	} else if (port == 8 || port == 9) {
#if (KERNEL_VERSION(5, 3, 0) > LINUX_VERSION_CODE)
		if (state->interface != PHY_INTERFACE_MODE_10GKR)
			goto unsupported;
#else
		if (state->interface != PHY_INTERFACE_MODE_USXGMII)
			goto unsupported;
#endif
	} else {
		bitmap_zero(supported, __ETHTOOL_LINK_MODE_MASK_NBITS);
		dev_err(ds->dev, "Unsupported port: %i\n", port);
		return;
	}

	mxl862xx_phylink_set_capab(supported, state);
	return;

unsupported:
	bitmap_zero(supported, __ETHTOOL_LINK_MODE_MASK_NBITS);
	dev_err(ds->dev, "Unsupported interface '%s' for port %d\n",
		phy_modes(state->interface), port);
}

#else
static void mxl862xx_phylink_get_caps(struct dsa_switch *ds, int port,
				      struct phylink_config *config)
{
	struct mxl862xx_priv *priv = ds->priv;

	if (port >= 0 && port < priv->hw_info->phy_ports) {
		__set_bit(PHY_INTERFACE_MODE_INTERNAL,
			  config->supported_interfaces);
	} else if (port == 8 || port == 9) {
		__set_bit(PHY_INTERFACE_MODE_USXGMII,
			  config->supported_interfaces);
	} else if (port > 9) {
		__set_bit(PHY_INTERFACE_MODE_NA, config->supported_interfaces);
	}

	config->mac_capabilities = MAC_ASYM_PAUSE | MAC_SYM_PAUSE | MAC_10 |
				   MAC_100 | MAC_1000 |
#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
					MAC2500FD;
#else
					MAC_2500FD;
#endif

	return;
}
#endif

static void mxl862xx_phylink_mac_config(struct dsa_switch *ds, int port,
					unsigned int mode,
					const struct phylink_link_state *state)
{
	switch (state->interface) {
	case PHY_INTERFACE_MODE_INTERNAL:
		return;
	case PHY_INTERFACE_MODE_SGMII:
		return;
#if (KERNEL_VERSION(5, 3, 0) > LINUX_VERSION_CODE)
	case PHY_INTERFACE_MODE_10GKR:
		/* Configure the USXGMII */
		break;
#else
	case PHY_INTERFACE_MODE_USXGMII:
		/* Configure the USXGMII */
		break;
#endif
	default:
		dev_err(ds->dev, "Unsupported interface: %d\n",
			state->interface);
		return;
	}
}

static void mxl862xx_phylink_mac_link_down(struct dsa_switch *ds, int port,
					   unsigned int mode,
					   phy_interface_t interface)
{
	mxl862xx_port_link_cfg_t port_link_cfg = { 0 };
	int ret;

	if (dsa_is_cpu_port(ds, port))
		return;

	port_link_cfg.port_id = port + 1;

	port_link_cfg.link_force = true;
	port_link_cfg.link = MXL862XX_PORT_LINK_DOWN;

	ret = mxl862xx_port_link_cfg_set(&mxl_dev, &port_link_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: MAC link port configuration for port %d failed with %d\n",
			__func__, port, ret);
	}
}

#if (KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE)
static void mxl862xx_phylink_mac_link_up(struct dsa_switch *ds, int port,
					 unsigned int mode,
					 phy_interface_t interface,
					 struct phy_device *phydev, int speed,
					 int duplex, bool tx_pause,
					 bool rx_pause)
{
	mxl862xx_port_link_cfg_t port_link_cfg = { 0 };
	mxl862xx_port_cfg_t port_cfg = { 0 };
	int ret;

	if (dsa_is_cpu_port(ds, port))
		return;

	port_link_cfg.port_id = port + 1;

	port_link_cfg.link_force = true;
	port_link_cfg.link = MXL862XX_PORT_LINK_UP;

	port_link_cfg.speed_force = true;
	switch (speed) {
	case SPEED_10:
		port_link_cfg.speed = MXL862XX_PORT_SPEED_10;
		break;
	case SPEED_100:
		port_link_cfg.speed = MXL862XX_PORT_SPEED_100;
		break;
	case SPEED_1000:
		port_link_cfg.speed = MXL862XX_PORT_SPEED_1000;
		break;
	case SPEED_2500:
		port_link_cfg.speed = MXL862XX_PORT_SPEED_2500;
		break;
	case SPEED_5000:
		port_link_cfg.speed = MXL862XX_PORT_SPEED_5000;
		break;
	case SPEED_10000:
		port_link_cfg.speed = MXL862XX_PORT_SPEED_10000;
		break;
	default:
		dev_err(ds->dev,
			"%s: Unsupported  MAC link speed %d Mbps on port:%d\n",
			__func__, speed, port);
		return;
	}

	port_link_cfg.duplex_force = true;
	switch (duplex) {
	case DUPLEX_HALF:
		port_link_cfg.duplex = MXL862XX_DUPLEX_HALF;
		break;
	case DUPLEX_FULL:
		port_link_cfg.duplex = MXL862XX_DUPLEX_FULL;
		break;
	default:
		port_link_cfg.duplex = MXL862XX_DUPLEX_AUTO;
	}

	ret = mxl862xx_port_link_cfg_set(&mxl_dev, &port_link_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Port link configuration for port %d failed with %d\n",
			__func__, port, ret);
		return;
	}

	port_cfg.port_id = port + 1;
	ret = mxl862xx_port_cfg_get(&mxl_dev, &port_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Port configuration read for port %d failed with %d\n",
			__func__, port, ret);
		return;
	}

	if (tx_pause && rx_pause)
		port_cfg.flow_ctrl = MXL862XX_FLOW_RXTX;
	else if (tx_pause)
		port_cfg.flow_ctrl = MXL862XX_FLOW_TX;
	else if (rx_pause)
		port_cfg.flow_ctrl = MXL862XX_FLOW_RX;
	else
		port_cfg.flow_ctrl = MXL862XX_FLOW_OFF;

	ret = mxl862xx_port_cfg_set(&mxl_dev, &port_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Port configuration for port %d failed with %d\n",
			__func__, port, ret);
	}

	return;
}

#else
static void mxl862xx_phylink_mac_link_up(struct dsa_switch *ds, int port,
					 unsigned int mode,
					 phy_interface_t interface,
					 struct phy_device *phydev)
{
	mxl862xx_port_link_cfg_t port_link_cfg = { 0 };
	int ret;

	if (dsa_is_cpu_port(ds, port))
		return;

	port_link_cfg.port_id = port + 1;

	port_link_cfg.link_force = true;
	port_link_cfg.link = MXL862XX_PORT_LINK_UP;

	ret = mxl862xx_port_link_cfg_set(&mxl_dev, &port_link_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Port link configuration for port %d failed with %d\n",
			__func__, port, ret);
		return;
	}
}
#endif
#endif

static void mxl862xx_get_ethtool_stats(struct dsa_switch *ds, int port,
				       uint64_t *data)
{
	int ret = -EINVAL;
	uint8_t i = 0;
	mxl862xx_debug_rmon_port_cnt_t dbg_rmon_port_cnt = { 0 };

	/* RX */
	dbg_rmon_port_cnt.port_id = port + 1;
	dbg_rmon_port_cnt.port_type = MXL862XX_RMON_CTP_PORT_RX;
	ret = mxl862xx_debug_rmon_port_get(&mxl_dev, &dbg_rmon_port_cnt);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Reading RMON RX statistics for port %d failed with %d\n",
			__func__, port, ret);
		return;
	}
	data[i++] = dbg_rmon_port_cnt.rx_good_pkts; //0
	data[i++] = dbg_rmon_port_cnt.rx_unicast_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_broadcast_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_multicast_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_fcserror_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_under_size_good_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_oversize_good_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_under_size_error_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_oversize_error_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_filtered_pkts;
	data[i++] = dbg_rmon_port_cnt.rx64byte_pkts; //10
	data[i++] = dbg_rmon_port_cnt.rx127byte_pkts;
	data[i++] = dbg_rmon_port_cnt.rx255byte_pkts;
	data[i++] = dbg_rmon_port_cnt.rx511byte_pkts;
	data[i++] = dbg_rmon_port_cnt.rx1023byte_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_max_byte_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_dropped_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_extended_vlan_discard_pkts;
	data[i++] = dbg_rmon_port_cnt.mtu_exceed_discard_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_good_bytes;
	data[i++] = dbg_rmon_port_cnt.rx_bad_bytes; //20
	data[i++] = dbg_rmon_port_cnt.rx_unicast_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.rx_broadcast_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.rx_multicast_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.rx_good_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.rx_good_bytes_yellow_red;
	data[i++] = dbg_rmon_port_cnt.rx_good_pause_pkts;
	data[i++] = dbg_rmon_port_cnt.rx_align_error_pkts;

	/* TX */
	memset(&dbg_rmon_port_cnt, 0, sizeof(dbg_rmon_port_cnt));
	dbg_rmon_port_cnt.port_id = port + 1;
	dbg_rmon_port_cnt.port_type = MXL862XX_RMON_CTP_PORT_TX;
	ret = mxl862xx_debug_rmon_port_get(&mxl_dev, &dbg_rmon_port_cnt);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Reading RMON TX statistics for port %d failed with %d\n",
			__func__, port, ret);
		return;
	}
	data[i++] = dbg_rmon_port_cnt.tx_good_pkts;
	data[i++] = dbg_rmon_port_cnt.tx_unicast_pkts;
	data[i++] = dbg_rmon_port_cnt.tx_broadcast_pkts; //30
	data[i++] = dbg_rmon_port_cnt.tx_multicast_pkts;
	data[i++] = dbg_rmon_port_cnt.tx64byte_pkts;
	data[i++] = dbg_rmon_port_cnt.tx127byte_pkts;
	data[i++] = dbg_rmon_port_cnt.tx255byte_pkts;
	data[i++] = dbg_rmon_port_cnt.tx511byte_pkts;
	data[i++] = dbg_rmon_port_cnt.tx1023byte_pkts;
	data[i++] = dbg_rmon_port_cnt.tx_max_byte_pkts;
	data[i++] = dbg_rmon_port_cnt.tx_dropped_pkts;
	data[i++] = dbg_rmon_port_cnt.tx_acm_dropped_pkts;
	data[i++] = dbg_rmon_port_cnt.tx_good_bytes; //40
	data[i++] = dbg_rmon_port_cnt.tx_unicast_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.tx_broadcast_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.tx_multicast_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.tx_good_pkts_yellow_red;
	data[i++] = dbg_rmon_port_cnt.tx_good_bytes_yellow_red;
	data[i++] = dbg_rmon_port_cnt.tx_single_coll_count;
	data[i++] = dbg_rmon_port_cnt.tx_mult_coll_count;
	data[i++] = dbg_rmon_port_cnt.tx_late_coll_count;
	data[i++] = dbg_rmon_port_cnt.tx_excess_coll_count;
	data[i++] = dbg_rmon_port_cnt.tx_coll_count; //50
	data[i++] = dbg_rmon_port_cnt.tx_pause_count;

	return;
}

static void mxl862xx_get_strings(struct dsa_switch *ds, int port,
				 uint32_t stringset, uint8_t *data)
{
	uint8_t i;

	if (stringset != ETH_SS_STATS)
		return;
	for (i = 0; i < ARRAY_SIZE(mxl862xx_rmon_cnt); i++)
		strscpy(data + i * ETH_GSTRING_LEN, mxl862xx_rmon_cnt[i].name,
			ETH_GSTRING_LEN);
}

static int mxl862xx_get_sset_count(struct dsa_switch *ds, int port, int sset)
{
	if (sset != ETH_SS_STATS)
		return 0;

	return ARRAY_SIZE(mxl862xx_rmon_cnt);
}

static void mxl862xx_port_fast_age(struct dsa_switch *ds, int port)
{
	int ret = -EINVAL;
	mxl862xx_mac_table_clear_cond_t param = { 0 };

	param.type = MXL862XX_MAC_CLEAR_PHY_PORT;
	param.port_id = port + 1;

	ret = mxl862xx_mac_table_clear_cond(&mxl_dev, &param);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Clearing MAC table for port %d failed with %d\n",
			__func__, port, ret);
	}
}

#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
static int mxl862xx_port_mirror_add(struct dsa_switch *ds, int port,
				    struct dsa_mall_mirror_tc_entry *mirror,
				    bool ingress)
#else
static int mxl862xx_port_mirror_add(struct dsa_switch *ds, int port,
				    struct dsa_mall_mirror_tc_entry *mirror,
				    bool ingress, struct netlink_ext_ack *extack)
#endif
{
	struct mxl862xx_priv *priv = ds->priv;
	int ret = -EINVAL;
	mxl862xx_ctp_port_config_t ctp_param = { 0 };
	mxl862xx_monitor_port_cfg_t mon_param = { 0 };

	if (port < 0 || port >= MAX_PORTS) {
		dev_err(priv->dev, "invalid port: %d\n", port);
		goto EXIT;
	}

	/* first read, then change */
	ctp_param.logical_port_id = port + 1;
	ctp_param.mask = MXL862XX_CTP_PORT_CONFIG_MASK_ALL;
	ret = mxl862xx_ctp_port_config_get(&mxl_dev, &ctp_param);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Enabling monitoring of port %d failed on port config get with %d\n",
			__func__, port, ret);
		goto EXIT;
	}

	ctp_param.mask = MXL862XX_CTP_PORT_CONFIG_LOOPBACK_AND_MIRROR;
	if (ingress) {
		priv->port_info[port].ingress_mirror_enabled = true;
		ctp_param.ingress_mirror_enable = true;
	} else {
		priv->port_info[port].egress_mirror_enabled = true;
		ctp_param.egress_mirror_enable = true;
	}

	ret = mxl862xx_ctp_port_config_set(&mxl_dev, &ctp_param);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Enabling monitoring of port %d failed on port config set with %d\n",
			__func__, port, ret);
		goto EXIT;
	}

	mon_param.port_id = mirror->to_local_port + 1;
	ret = mxl862xx_monitor_port_cfg_set(&mxl_dev, &mon_param);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev, "%s: Setting monitor port %d failed with %d\n",
			__func__, mon_param.port_id, ret);
		goto EXIT;
	}

EXIT:
	return ret;
}

static void mxl862xx_port_mirror_del(struct dsa_switch *ds, int port,
				     struct dsa_mall_mirror_tc_entry *mirror)
{
	int ret = -EINVAL;
	uint8_t i;
	struct mxl862xx_priv *priv = ds->priv;
	uint8_t phy_ports = priv->hw_info->phy_ports;
	mxl862xx_ctp_port_config_t ctp_param = { 0 };

	if (port < 0 || port >= MAX_PORTS) {
		dev_err(priv->dev, "invalid port: %d\n", port);
		return;
	}

	ctp_param.logical_port_id = port + 1;
	ctp_param.mask = MXL862XX_CTP_PORT_CONFIG_LOOPBACK_AND_MIRROR;
	if (mirror->ingress) {
		priv->port_info[port].ingress_mirror_enabled = false;
		ctp_param.ingress_mirror_enable = false;
	} else {
		priv->port_info[port].egress_mirror_enabled = false;
		ctp_param.egress_mirror_enable = false;
	}

	ret = mxl862xx_ctp_port_config_set(&mxl_dev, &ctp_param);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Disabling monitoring of port %d failed with %d\n",
			__func__, port, ret);
		goto EXIT;
	}

	for (i = 0; i < phy_ports; i++) {
		/* some ports are still mirrored, keep the monitor port configured */
		if (priv->port_info[i].egress_mirror_enabled ||
		    priv->port_info[i].egress_mirror_enabled)
			break;
		/* checked all and no port is being mirrored - release the monitor port */
		if (i == phy_ports - 1) {
			mxl862xx_monitor_port_cfg_t mon_param = { 0 };

			ret = mxl862xx_monitor_port_cfg_set(&mxl_dev,
							    &mon_param);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Releasing monitor port %d failed with %d\n",
					__func__, mon_param.port_id, ret);
				goto EXIT;
			}
		}
	}

EXIT:
	return;
}

static int
__get_vlan_vid_filters_idx(struct mxl862xx_priv *priv, uint8_t port, bool ingress,
		uint16_t vid, int *f_0, int *f_1, uint16_t *vlan_idx)
{
	int ret = -EINVAL;
	int x, i = 0;
	/* negative values if not found */
	int filter_0 = -1;
	int filter_1 = -1;
	struct mxl862xx_extended_vlan_block_info *block_info;

	if (ingress)
		block_info = &(priv->port_info[port].vlan.ingress_vlan_block_info);
	else
		block_info = &(priv->port_info[port].vlan.egress_vlan_block_info);

	/* Check if there's active entry for the requested VLAN. If found, overwrite it. */
	if (filter_0 < 0 && filter_1 < 0) {
		for (i = 0; i < MAX_VLANS; i++) {
			if (block_info->vlans[i].vid == vid) {
				filter_0 = block_info->vlans[i].filters_idx[0];
				filter_1 = block_info->vlans[i].filters_idx[1];
				ret = 0;
				break;
			}
		}
	}

	/* If there are no matching active VLAN entries, check in recycled */
	if (filter_0 < 0 && filter_1 < 0) {
	/* check if there are recycled filter entries for use */
		for (x = 0; x < MAX_VLANS; x++) {
			if (block_info->filter_entries_recycled[x].valid) {
				filter_0 = block_info->filter_entries_recycled[x].filters_idx[0];
				filter_1 = block_info->filter_entries_recycled[x].filters_idx[1];
				/* remove filter entries from recycled inventory */
				block_info->filter_entries_recycled[x].valid = false;
				ret = 0;
				break;
			}
		}

		/* find empty slot for storing ID's of vlan filtering rules */
		for (i = 0; i < MAX_VLANS; i++) {
			if (!(block_info->vlans[i].used)) {
				ret = 0;
				break;
			}
			if (i == priv->max_vlans - 1) {
				ret = -ENOSPC;
				dev_err(priv->dev,
					"%s: Port:%d reached max number of defined VLAN's: %d\n",
					__func__, port, priv->max_vlans);
				goto EXIT;
			}
		}
	}

	if (f_0 != NULL)
		*f_0 = filter_0;
	if (f_1 != NULL)
		*f_1 = filter_1;
	if (vlan_idx != NULL)
		*vlan_idx = i;

EXIT:
	return ret;
}


static int
__prepare_vlan_egress_filters_off_sp_tag_no_vid(struct dsa_switch *ds, uint8_t port)
{
	int ret = -EINVAL;
	struct mxl862xx_priv *priv = ds->priv;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	/* Allocate new block if needed */
	if (!(priv->port_info[port].vlan.egress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[port]
				.vlan.egress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		priv->port_info[port].vlan.egress_vlan_block_info.allocated =
			true;
		priv->port_info[port].vlan.egress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	// Static entry :  Outer and iner tag.
	// Remove outer tag  one as it must be sp_tag. Transparent for inner tag.
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 2;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	/* remove  sp tag */
	vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

	// Last entry :  Only outer tag. Remove it as it must be sp_tag
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 1;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	/* remove  sp tag */
	vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

EXIT:
	return ret;
}


static int
__prepare_vlan_egress_filters_off_sp_tag(struct dsa_switch *ds, uint8_t port, uint16_t vid, bool untagged)
{
	int ret = -EINVAL;
	uint16_t idx = 0;
	/* negative values if not found */
	int filter_0 = -1;
	int filter_1 = -1;
	struct mxl862xx_priv *priv = ds->priv;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	/* Allocate new block if needed */
	if (!(priv->port_info[port].vlan.egress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[port]
				.vlan.egress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		priv->port_info[port].vlan.egress_vlan_block_info.allocated =
			true;
		priv->port_info[port].vlan.egress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	/* VID specific entries must be processed before the final entries,
	 * so putting them at the beginnig of the block */

	ret = __get_vlan_vid_filters_idx(priv, port, false, vid, &filter_0, &filter_1, &idx);
	dev_dbg(priv->dev, "%s: Port:%d  vid:%d f_0:%d f_1:%d idx:%d\n", __func__, port, vid, filter_0, filter_1, idx);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	// Entry 0 :  Outer and Inner tags are present. Inner tag matching vid.
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ?
			filter_0 :
			priv->port_info[port]
				.vlan.egress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.vid_enable = true;
	vlan_cfg.filter.inner_vlan.vid_val = vid;

	if (untagged) {
		/* remove both sp_tag(outer) and vid (inner) */
		vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_2_TAG;
	} else {
		/* remove only sp tag */
		vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;
	}

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[0] = vlan_cfg.entry_index;

	priv->port_info[port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[1] = IDX_INVAL;

	// Static entry :  Outer and iner tag, not matching vid. Remove outer tag  one as it must be sp_tag
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 2;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	/* remove  sp tag */
	vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

	// Last entry :  Only outer tag. Remove it as it must be sp_tag
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 2;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	/* remove  sp tag */
	vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

	// Last entry :  Only outer tag. Remove it as it must be sp_tag
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 1;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	/* remove  sp tag */
	vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index ;


	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].used = true;
	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].untagged = untagged;

EXIT:
	return ret;
}

static int
__prepare_vlan_egress_filters_off(struct mxl862xx_priv *priv, uint8_t port, uint16_t vid, bool untagged)
{
	int ret = -EINVAL;
	uint16_t idx = 0;
	/* negative values if not found */
	int filter_0 = -1;
	int filter_1 = -1;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	/* Allocate new block if needed */
	if (!(priv->port_info[port].vlan.egress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[port]
				.vlan.egress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		priv->port_info[port].vlan.egress_vlan_block_info.allocated =
			true;
		priv->port_info[port].vlan.egress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	/* VID specific entries must be processed before the final entries,
	 * so putting them at the beginnig of the block */

	ret = __get_vlan_vid_filters_idx(priv, port, false, vid, &filter_0, &filter_1, &idx);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	// Entry 0 : ACCEPT VLAN tags that are matching  VID. Outer and Inner tags are present
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ?
			filter_0 :
			priv->port_info[port]
				.vlan.egress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.vid_enable = true;
	vlan_cfg.filter.inner_vlan.vid_val = vid;
	if (untagged)
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;
	else
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[0] = vlan_cfg.entry_index;

	//	 Entry 1 : ACCEPT VLAN tags that are matching PVID or port VID. Only the outer tags are present
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_1 >= 0 ?
			filter_1 :
			priv->port_info[port]
				.vlan.egress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.filter.inner_vlan.vid_enable = true;
	vlan_cfg.filter.inner_vlan.vid_val = vid;
	if (untagged)
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;
	else
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[1] = vlan_cfg.entry_index;

	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].used = true;
	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].untagged = untagged;


EXIT:
	return ret;
}


static int
__prepare_vlan_ingress_filters_off_sp_tag_no_vid(struct dsa_switch *ds, uint8_t port)
{
	struct mxl862xx_priv *priv = ds->priv;
	int ret = -EINVAL;
	struct mxl862xx_extended_vlan_block_info *block_info =
		&priv->port_info[port].vlan.ingress_vlan_block_info;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	/* Allocate new block if needed */
	if (!(block_info->allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries = block_info->filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		block_info->allocated =	true;
		block_info->block_id = vlan_alloc.extended_vlan_block_id;
	}

	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;

	//Static rules. No tags, add SP tag
	vlan_cfg.entry_index = block_info->filters_max - 3;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.vid_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
	vlan_cfg.treatment.outer_vlan.tpid =
		MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
	vlan_cfg.treatment.outer_vlan.priority_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.outer_vlan.dei =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
	vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	block_info->final_filters_idx = vlan_cfg.entry_index;

	// Static rules
	// Single tag. Use transparent mode. Add sp tag
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	vlan_cfg.entry_index = block_info->filters_max - 2;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.vid_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
	vlan_cfg.treatment.outer_vlan.tpid =
		MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
	vlan_cfg.treatment.outer_vlan.priority_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.outer_vlan.dei =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
	vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	block_info->final_filters_idx = vlan_cfg.entry_index;

	// Two tags. Use transparent mode. Do not apply vid as this is tagged pkt
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	vlan_cfg.entry_index = block_info->filters_max - 1;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.vid_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
	vlan_cfg.treatment.outer_vlan.tpid =
		MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
	vlan_cfg.treatment.outer_vlan.priority_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.outer_vlan.dei =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
	vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	block_info->final_filters_idx = vlan_cfg.entry_index;

EXIT:
	return ret;
}

static int
__prepare_vlan_ingress_filters_off_sp_tag(struct dsa_switch *ds, uint8_t port, uint16_t vid)
{
	struct mxl862xx_priv *priv = ds->priv;
	int ret = -EINVAL;
	//there's possible only one rule for single pvid, so it always uses idx 0
	uint16_t idx = 0;
	struct mxl862xx_extended_vlan_block_info *block_info =
		&priv->port_info[port].vlan.ingress_vlan_block_info;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	/* Allocate new block if needed */
	if (!(block_info->allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries = block_info->filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		block_info->allocated =	true;
		block_info->block_id = vlan_alloc.extended_vlan_block_id;
	}

	/*  If port has pvid then add vid dependand dynamic rule.
	 *  It's done that way because it's required for proper handling of
	 *  vlan delete scenario. If no pvid configured, create 'static' rule */
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;

	// vid dynamic rule
	if (priv->port_info[port].vlan.pvid) {
		/* As there's only one  pvid  per port possible, always overwrite the rule at position 0 */
		vlan_cfg.entry_index = 0;
		vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
		vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
		vlan_cfg.treatment.add_outer_vlan = true;
		vlan_cfg.treatment.outer_vlan.vid_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
		vlan_cfg.treatment.outer_vlan.tpid =
			MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.outer_vlan.priority_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
		vlan_cfg.treatment.outer_vlan.priority_val = 0;
		vlan_cfg.treatment.outer_vlan.dei =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
		vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);
		vlan_cfg.treatment.add_inner_vlan = true;
		vlan_cfg.treatment.inner_vlan.vid_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
		vlan_cfg.treatment.inner_vlan.vid_val =
			priv->port_info[port].vlan.pvid;
		vlan_cfg.treatment.inner_vlan.tpid =
			MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.inner_vlan.priority_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
		vlan_cfg.treatment.inner_vlan.priority_val = 0;
		vlan_cfg.treatment.inner_vlan.dei =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;

		ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
				__func__, port, vlan_cfg.entry_index,
				vlan_cfg.extended_vlan_block_id);
			goto EXIT;
		}

		block_info->vlans[idx].filters_idx[0] = vlan_cfg.entry_index;
		block_info->vlans[idx].filters_idx[1] = IDX_INVAL;

		block_info->vlans[idx].vid = vid;
		block_info->vlans[idx].used = true;

	}
	// no pvid, static rule
	else {
		// deactivate possible dynamic rule if there's no pvid
		if (block_info->vlans[idx].vid) {
			ret = __deactivate_vlan_filter_entry(block_info->block_id, block_info->vlans[idx].filters_idx[0]);
			if (ret != MXL862XX_STATUS_OK)
				goto EXIT;
			block_info->vlans[idx].vid = 0;
			block_info->vlans[idx].used = false;
		}

		vlan_cfg.entry_index = block_info->filters_max - 3;
		vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
		vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
		vlan_cfg.treatment.add_outer_vlan = true;
		vlan_cfg.treatment.outer_vlan.vid_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
		vlan_cfg.treatment.outer_vlan.tpid =
			MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.outer_vlan.priority_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
		vlan_cfg.treatment.outer_vlan.priority_val = 0;
		vlan_cfg.treatment.outer_vlan.dei =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
		vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);

		ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
				__func__, port, vlan_cfg.entry_index, vlan_cfg.extended_vlan_block_id);
			goto EXIT;
		}

		block_info->final_filters_idx = vlan_cfg.entry_index;
	}

	// Static rules
	// Single tag. Use transparent mode. Do not apply PVID as this is the tagged traffic
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	vlan_cfg.entry_index = block_info->filters_max - 2;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.vid_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
	vlan_cfg.treatment.outer_vlan.tpid =
		MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
	vlan_cfg.treatment.outer_vlan.priority_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.outer_vlan.dei =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
	vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	block_info->final_filters_idx = vlan_cfg.entry_index;

	// Two tags. Use transparent mode. Do not apply vid as this is tagged pkt
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	vlan_cfg.entry_index = block_info->filters_max - 1;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.vid_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
	vlan_cfg.treatment.outer_vlan.tpid =
		MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
	vlan_cfg.treatment.outer_vlan.priority_mode =
		MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.outer_vlan.dei =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
	vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	block_info->final_filters_idx = vlan_cfg.entry_index;

EXIT:
	return ret;
}

static int
__prepare_vlan_ingress_filters_off(struct mxl862xx_priv *priv, uint8_t port, uint16_t vid)
{
	int ret = -EINVAL;
	uint16_t idx = 0;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	/* Allocate new block if needed */
	if (!(priv->port_info[port].vlan.ingress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		priv->port_info[port].vlan.ingress_vlan_block_info.allocated =
			true;
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	// Entry 4  untagged pkts. If there's PVID accept and add PVID tag
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	/* for cpu port this entry is fixed and always put at the end of the block */
	if (port == priv->hw_info->cpu_port)
		vlan_cfg.entry_index = priv->port_info[port].vlan.ingress_vlan_block_info.filters_max - 1;
	else {
		vlan_cfg.entry_index =
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.final_filters_idx--;
	}
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;

	if (priv->port_info[port].vlan.pvid) {
		vlan_cfg.treatment.add_outer_vlan = true;
		vlan_cfg.treatment.outer_vlan.vid_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
		vlan_cfg.treatment.outer_vlan.vid_val =
			priv->port_info[port].vlan.pvid;
		vlan_cfg.treatment.outer_vlan.tpid =
			MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.outer_vlan.priority_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
		vlan_cfg.treatment.outer_vlan.priority_val = 0;
		vlan_cfg.treatment.outer_vlan.dei =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
	}

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	ret = __get_vlan_vid_filters_idx(priv, port, true, vid, NULL, NULL, &idx);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d couldn't get idx for VID:%d and  block ID:%d\n",
			__func__, port, vid, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.ingress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[port].vlan.ingress_vlan_block_info.vlans[idx].used = true;

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
static int mxl862xx_port_vlan_prepare(struct dsa_switch *ds, int port,
				    const struct switchdev_obj_port_vlan *vlan)
{
	/* function needed to make VLAN working for legacy Linux kernels */
	return 0;
}
#endif

#if (KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE)
static int mxl862xx_port_vlan_filtering(struct dsa_switch *ds, int port,
					bool vlan_filtering)
#else
static int mxl862xx_port_vlan_filtering(struct dsa_switch *ds, int port,
					bool vlan_filtering,
					struct netlink_ext_ack *extack)
#endif
{
	int ret = 0;
	struct mxl862xx_priv *priv = ds->priv;
	struct dsa_port *dsa_port = dsa_to_port(ds, port);
#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	struct net_device *bridge = dsa_port_bridge_dev_get(dsa_port);
#else
	struct net_device *bridge;

	if (dsa_port)
		bridge = dsa_port->bridge_dev;
	else
		return -EINVAL;
#endif

	/* Prevent dynamic setting of the vlan_filtering. */
	if (bridge && priv->port_info[port].vlan.filtering_mode_locked) {
		ret = -ENOTSUPP;
		dev_err(ds->dev, "%s: Change of vlan_filtering mode is not allowed while port:%d is joined to a bridge\n",
				__func__, port);
#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Change of vlan_filtering mode is not allowedwhile port is joind to a bridge.");
#endif

	} else {
		priv->port_info[port].vlan.filtering = vlan_filtering;
		/* Do not lock if port is isolated. */
		if (!priv->port_info[port].isolated)
			priv->port_info[port].vlan.filtering_mode_locked = true;
	}

	return ret;
}

static int
__prepare_vlan_egress_filters(struct dsa_switch *ds, uint8_t port, uint16_t vid, bool untagged)
{
	int ret = -EINVAL;
	uint16_t idx = 0;
	/* negative values if not found */
	int filter_0 = -1;
	int filter_1 = -1;
	struct mxl862xx_priv *priv = ds->priv;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	struct mxl862xx_extended_vlan_block_info *block_info =
		&priv->port_info[port].vlan.egress_vlan_block_info;

	/* Allocate new block if needed */
	if (!(block_info->allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries = block_info->filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		block_info->allocated = true;
		block_info->block_id = vlan_alloc.extended_vlan_block_id;
	}

	/* First populate the block with set of rules which should be executed finally after
	 * VID specific filtering. The final rules (not related to VID) are placed on the end of the block. The number of
	 * rules is fixed  per port. Order of execution  is important. To avoid static reservations they are
	 * stored in reversed order starting from the end of the block */

	//Entry 4: no outer/inner tag, no PVID  DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	vlan_cfg.entry_index = block_info->final_filters_idx--;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	//Entry 3: Only Outer tag present. Discard if VID is not matching the previous rules
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	vlan_cfg.entry_index = block_info->final_filters_idx--;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	//Entry 2: Outer and Inner tags are present. Discard if VID is not matching the previous rules
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	vlan_cfg.entry_index = block_info->final_filters_idx--;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* VID specific entries must be processed before the final entries,
	 * so putting them at the beginnig of the block */
	ret = __get_vlan_vid_filters_idx(priv, port, false, vid, &filter_0, &filter_1, &idx);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	// Entry 0 : ACCEPT VLAN tags that are matching  VID. Outer and Inner tags are present
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ? filter_0 : block_info->vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;

	if (untagged)
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;
	else
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	block_info->vlans[idx].filters_idx[0] = vlan_cfg.entry_index;

	//	 Entry 1 : ACCEPT VLAN tags that are matching PVID or port VID. Only the outer tags are present
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_1 >= 0 ? filter_1 : block_info->vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	if (untagged)
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;
	else
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	block_info->vlans[idx].filters_idx[1] = vlan_cfg.entry_index;

	block_info->vlans[idx].vid = vid;
	block_info->vlans[idx].used = true;
	block_info->vlans[idx].untagged = untagged;

EXIT:
	return ret;
}

static int
__prepare_vlan_egress_filters_sp_tag(struct dsa_switch *ds, uint8_t port, uint16_t vid, bool untagged)
{
	int ret = -EINVAL;
	uint16_t idx = 0;
	/* negative values if not found */
	int filter_0 = IDX_INVAL;
	int filter_1 = IDX_INVAL;
	struct mxl862xx_priv *priv = ds->priv;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };

	/* Allocate new block if needed */
	if (!(priv->port_info[port].vlan.egress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[port]
				.vlan.egress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Extended VLAN allocation for port%d egress filtering failed with %d\n",
				__func__, port, ret);
			goto EXIT;
		}

		priv->port_info[port].vlan.egress_vlan_block_info.allocated =
			true;
		priv->port_info[port].vlan.egress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	/* First populate the fixed block of rules which should be executed finally after
	 * VID specific filtering. The final rules (not related to VID)
	 * are placed at the end of the block. */

	//Entry 4: only outer tag (SP tag), no PVID  DISCARD
	if (untagged) {
		memset(&vlan_cfg, 0, sizeof(vlan_cfg));
		vlan_cfg.extended_vlan_block_id =
			priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	   vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 1;
		vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
		vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

		ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
				__func__, port, vlan_cfg.entry_index,
				vlan_cfg.extended_vlan_block_id);
			goto EXIT;
		}

		priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx =
			vlan_cfg.entry_index;
	}

	//Entry 3: there is any other inner tag -> discard upstream traffic
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 2;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.remove_tag =	MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx =
		vlan_cfg.entry_index;

	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.egress_vlan_block_info.filters_max - 3;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

	/* VID specific entries must be processed before the final entries,
	 * so putting them at the beginnig of the block */
	ret = __get_vlan_vid_filters_idx(priv, port, false, vid, &filter_0, &filter_1, &idx);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d couldn't get idx for VID specific filters for VID:%d and  block ID:%d\n",
			__func__, port, vid, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* Set of dynamic rules that depend on VID.
	 * The number of rules depends on the number of handled vlans */

	// Entry 0 : ACCEPT VLAN tags that are matching  VID. Outer and Inner tags are present
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ?
			filter_0 :
			priv->port_info[port]
				.vlan.egress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.vid_enable = true;
	vlan_cfg.filter.inner_vlan.vid_val = vid;

	if (untagged) {
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_2_TAG;
	} else {
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG;
	}

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[0] = vlan_cfg.entry_index;

   /* mark as unused */
	priv->port_info[port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[1] = IDX_INVAL;

	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].used = true;
	priv->port_info[port].vlan.egress_vlan_block_info.vlans[idx].untagged = untagged;

EXIT:
	return ret;
}


static int
__prepare_vlan_egress_filters_off_sp_tag_cpu(struct dsa_switch *ds, uint8_t cpu_port)
{
	int ret = -EINVAL;
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	struct mxl862xx_extended_vlan_block_info *block_info =
		&priv->port_info[cpu_port].vlan.egress_vlan_block_info;

	/* Allocate new block if needed */
	if (!(block_info->allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries = block_info->filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);

		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Extended VLAN allocation for port%d egress filtering failed with %d\n",
				__func__, cpu_port, ret);
			goto EXIT;
		}

		block_info->allocated =	true;
		block_info->block_id = vlan_alloc.extended_vlan_block_id;
	}

	// Entry last - 1  : Outer and Inner tags are present.
	// Transparent mode, no tag modifications
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;

	vlan_cfg.entry_index = block_info->filters_max - 2;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, cpu_port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	block_info->final_filters_idx = vlan_cfg.entry_index;

	// Entry last : Outer tag is present.
	// Transparent mode, no tag modifications
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id = block_info->block_id;

	vlan_cfg.entry_index = block_info->filters_max - 1;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, cpu_port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	block_info->final_filters_idx = vlan_cfg.entry_index;

EXIT:
	return ret;

}


static int
__prepare_vlan_egress_filters_sp_tag_cpu(struct dsa_switch *ds, uint8_t cpu_port, uint16_t vid, bool untagged)
{
	int ret = -EINVAL;
	struct mxl862xx_priv *priv = ds->priv;
	int filter_0 = -1;
	int filter_1 = -1;
	uint16_t idx = 0;
	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };

	/* Allocate new block if needed */
	if (!(priv->port_info[cpu_port].vlan.egress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[cpu_port]
				.vlan.egress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);

		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Extended VLAN allocation for port%d egress filtering failed with %d\n",
				__func__, cpu_port, ret);
			goto EXIT;
		}

		priv->port_info[cpu_port].vlan.egress_vlan_block_info.allocated =
			true;
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	/* Populate the  block of rules related to VID */

	/* VID specific entries must be processed before the final entries,
	 * so putting them at the beginnig of the block */
	ret = __get_vlan_vid_filters_idx(priv, cpu_port, false, vid, &filter_0, &filter_1, &idx);

	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	// Entry 0 : Outer and Inner tags are present. If user port is untagged
	// remove inner tag if the outer tag is matching the user port
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[cpu_port].vlan.egress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ?
			filter_0 :
			priv->port_info[cpu_port]
				.vlan.egress_vlan_block_info.vid_filters_idx++;

	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.vid_enable = true;
	vlan_cfg.filter.inner_vlan.vid_val = vid;

	if (untagged) {
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_2_TAG;
		vlan_cfg.treatment.add_outer_vlan = true;
		vlan_cfg.treatment.outer_vlan.vid_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_OUTER_VID;
		vlan_cfg.treatment.outer_vlan.tpid = MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.outer_vlan.priority_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_OUTER_PRORITY;
		vlan_cfg.treatment.outer_vlan.dei = MXL862XX_EXTENDEDVLAN_TREATMENT_INNER_DEI;
	}	else {
		vlan_cfg.treatment.remove_tag = MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;
	}

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for egress extended VLAN block ID:%d\n",
			__func__, cpu_port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[cpu_port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[0] = vlan_cfg.entry_index;

   /* mark as unused */
	priv->port_info[cpu_port]
		.vlan.egress_vlan_block_info.vlans[idx]
		.filters_idx[1] = IDX_INVAL;

	priv->port_info[cpu_port].vlan.egress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[cpu_port].vlan.egress_vlan_block_info.vlans[idx].untagged = untagged;
	priv->port_info[cpu_port].vlan.egress_vlan_block_info.vlans[idx].used = true;

EXIT:
	return ret;

}


static int
__prepare_vlan_ingress_filters_sp_tag(struct dsa_switch *ds, uint8_t port, uint16_t vid)
{
	int ret = -EINVAL;
	uint16_t idx = 0;
	/* negative values if not found */
	int filter_0 = -1;
	int filter_1 = -1;
	struct mxl862xx_priv *priv = ds->priv;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };

	/* Allocate new block if needed */
	if (!(priv->port_info[port].vlan.ingress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);

		if (ret != MXL862XX_STATUS_OK)
			goto EXIT;

		priv->port_info[port].vlan.ingress_vlan_block_info.allocated =
			true;
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	/* First populate the fixed block of rules which should be executed finally after
	 * VID specific filtering. The final rules (not related to VID)
	 * are placed at the end of the block. */

	//Entry 6 no other rule applies Outer tag default Inner tag  not present DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.ingress_vlan_block_info.filters_max - 1;
	vlan_cfg.filter.outer_vlan.type =
		MXL862XX_EXTENDEDVLAN_FILTER_TYPE_DEFAULT;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	priv->port_info[port].vlan.ingress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

	//Entry 5 no other rule applies Outer tag default Inner tag  present DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.ingress_vlan_block_info.filters_max - 2;
	vlan_cfg.filter.outer_vlan.type =
		MXL862XX_EXTENDEDVLAN_FILTER_TYPE_DEFAULT;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	priv->port_info[port].vlan.ingress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

	// Entry 4  untagged pkts. If there's PVID accept and add PVID tag, otherwise reject
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.ingress_vlan_block_info.filters_max - 3;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	if (!priv->port_info[port].vlan.pvid) {
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;
	} else {
		vlan_cfg.treatment.add_outer_vlan = true;
		vlan_cfg.treatment.outer_vlan.vid_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
		vlan_cfg.treatment.outer_vlan.tpid =
			MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.outer_vlan.priority_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
		vlan_cfg.treatment.outer_vlan.priority_val = 0;
		vlan_cfg.treatment.outer_vlan.dei =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
		vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);
		vlan_cfg.treatment.add_inner_vlan = true;
		vlan_cfg.treatment.inner_vlan.vid_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
		vlan_cfg.treatment.inner_vlan.vid_val =
			priv->port_info[port].vlan.pvid;
		vlan_cfg.treatment.inner_vlan.tpid =
			MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.inner_vlan.priority_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
		vlan_cfg.treatment.inner_vlan.priority_val = 0;
		vlan_cfg.treatment.inner_vlan.dei =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
	}

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add PVID entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.ingress_vlan_block_info.final_filters_idx =
		vlan_cfg.entry_index;

	// Entry 3 : Only Outer tag present : not matching  DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.ingress_vlan_block_info.filters_max - 4;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add DISCARD entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.ingress_vlan_block_info.final_filters_idx =
		vlan_cfg.entry_index;

	// Entry 2 : Outer and Inner VLAN tag present : not matching  DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index = priv->port_info[port].vlan.ingress_vlan_block_info.filters_max - 5;

	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add DISCARD entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	priv->port_info[port].vlan.ingress_vlan_block_info.final_filters_idx = vlan_cfg.entry_index;

	/* VID specific filtering rules which should be executed first before final ones.
	 * Storing starts at the beginning of the block. */

	ret = __get_vlan_vid_filters_idx(priv, port, true, vid, &filter_0, &filter_1, &idx);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d couldn't get idx for VID specific filters for VID:%d and  block ID:%d\n",
			__func__, port, vid, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	// Entry 0 : Outer and Inner VLAN tag present :  matching  ACCEPT
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ?
			filter_0 :
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.vid_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
	vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);
	vlan_cfg.treatment.outer_vlan.tpid = MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
	vlan_cfg.treatment.outer_vlan.priority_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.outer_vlan.dei = MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for block_id:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.ingress_vlan_block_info.vlans[idx]
		.filters_idx[0] = vlan_cfg.entry_index;

	// Entry 1 : Only Outer tags is present : matching  ACCEPT
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_1 >= 0 ?
			filter_1 :
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.vid_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
	vlan_cfg.treatment.outer_vlan.vid_val = dsa_8021q_rx_vid(ds, port);
	vlan_cfg.treatment.outer_vlan.tpid = MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
	vlan_cfg.treatment.outer_vlan.priority_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.outer_vlan.dei = MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add entry:%d for block_id:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.ingress_vlan_block_info.vlans[idx]
		.filters_idx[1] = vlan_cfg.entry_index;

	priv->port_info[port].vlan.ingress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[port].vlan.ingress_vlan_block_info.vlans[idx].used = true;

EXIT:
	return ret;
}


static int
__prepare_vlan_ingress_filters_sp_tag_cpu(struct dsa_switch *ds, uint8_t port, uint8_t cpu_port)
{
	int ret = -EINVAL;
	uint16_t idx = 0;
	/* negative values if not found */
	int filter_0 = -1;
	int filter_1 = -1;
	struct mxl862xx_priv *priv = ds->priv;
	uint16_t bridge_port_cpu = priv->port_info[port].bridge_port_cpu;
	uint16_t vid = dsa_8021q_tx_vid(ds, port);

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };

	/* Allocate new block if needed */
	if (!(priv->port_info[cpu_port].vlan.ingress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[cpu_port]
				.vlan.ingress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Extended VLAN allocation for cpu_port%d ingress filtering failed with %d\n",
				__func__, cpu_port, ret);
			goto EXIT;
		}

		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.allocated =
			true;
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	/* VID specific filtering rules which should be executed first before final ones.
	 * Storing starts at the beginning of the block. */
	ret = __get_vlan_vid_filters_idx(priv, cpu_port, true, vid, &filter_0, &filter_1, &idx);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d couldn't get idx for VID specific filters for VID:%d and  block ID:%d\n",
			__func__, cpu_port, vid, vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	// Entry 0 : Outer and Inner VLAN tag present
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ?
			filter_0 :
			priv->port_info[cpu_port]
				.vlan.ingress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.reassign_bridge_port = true;
	vlan_cfg.treatment.new_bridge_port_id = bridge_port_cpu;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: cpu_port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, cpu_port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[cpu_port]
		.vlan.ingress_vlan_block_info.vlans[idx]
		.filters_idx[0] = vlan_cfg.entry_index;

	// Entry 1 : Only Outer tags is present
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[cpu_port].vlan.ingress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_1 >= 0 ?
			filter_1 :
			priv->port_info[cpu_port]
				.vlan.ingress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.reassign_bridge_port = true;
	vlan_cfg.treatment.new_bridge_port_id = bridge_port_cpu;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: cpu_port:%d failed to add entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, cpu_port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[cpu_port]
		.vlan.ingress_vlan_block_info.vlans[idx]
		.filters_idx[1] = vlan_cfg.entry_index;

	priv->port_info[cpu_port].vlan.ingress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[cpu_port].vlan.ingress_vlan_block_info.vlans[idx].used = true;

EXIT:
	return ret;
}

static int
__prepare_vlan_ingress_filters(struct dsa_switch *ds, uint8_t port, uint16_t vid)
{
	int ret = -EINVAL;
	uint16_t idx = 0;
	/* negative values if not found */
	int filter_0 = -1;
	int filter_1 = -1;
	struct mxl862xx_priv *priv = ds->priv;

	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };
	/* Allocate new block if needed */
	if (!(priv->port_info[port].vlan.ingress_vlan_block_info.allocated)) {
		mxl862xx_extendedvlan_alloc_t vlan_alloc = { 0 };
		/* Reserve fixed number of entries per port and direction */
		vlan_alloc.number_of_entries =
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.filters_max;
		ret = mxl862xx_extended_vlan_alloc(&mxl_dev, &vlan_alloc);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(priv->dev,
				"%s: Extended VLAN allocation for port%d ingress filtering failed with %d\n",
				__func__, port, ret);
			goto EXIT;
		}

		priv->port_info[port].vlan.ingress_vlan_block_info.allocated =
			true;
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id =
			vlan_alloc.extended_vlan_block_id;
	}

	/* First populate the block with set of rules which should be executed finally after
	 * VID specific filtering. The final rules (not related to VID) are placed on the end of the block. The number of
	 * rules is fixed  per port. Order of execution  is important. To avoid static reservations they are
	 * stored in reversed order starting from the end of the block */

	//Entry 6 no other rule applies Outer tag default Inner tag  not present DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index =
		priv->port_info[port]
			.vlan.ingress_vlan_block_info.final_filters_idx--;
	vlan_cfg.filter.outer_vlan.type =
		MXL862XX_EXTENDEDVLAN_FILTER_TYPE_DEFAULT;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add default DISCARD entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	//Entry 5 no other rule applies Outer tag default Inner tag  present DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index =
		priv->port_info[port]
			.vlan.ingress_vlan_block_info.final_filters_idx--;
	vlan_cfg.filter.outer_vlan.type =
		MXL862XX_EXTENDEDVLAN_FILTER_TYPE_DEFAULT;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(priv->dev,
			"%s: Port:%d failed to add default DISCARD entry:%d for ingress extended VLAN block ID:%d\n",
			__func__, port, vlan_cfg.entry_index,
			vlan_cfg.extended_vlan_block_id);
		goto EXIT;
	}

	// Entry 4  untagged pkts. If there's PVID accept and add PVID tag, otherwise reject
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index =
		priv->port_info[port]
			.vlan.ingress_vlan_block_info.final_filters_idx--;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	if (!priv->port_info[port].vlan.pvid) {
		vlan_cfg.treatment.remove_tag =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;
	} else {
		vlan_cfg.treatment.add_outer_vlan = true;
		vlan_cfg.treatment.outer_vlan.vid_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL;
		vlan_cfg.treatment.outer_vlan.tpid =
			MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q;
		vlan_cfg.treatment.outer_vlan.priority_mode =
			MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
		vlan_cfg.treatment.outer_vlan.priority_val = 0;
		vlan_cfg.treatment.outer_vlan.dei =
			MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0;
		vlan_cfg.treatment.outer_vlan.vid_val =
			priv->port_info[port].vlan.pvid;
	}

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	// Entry 3 : Only Outer tag present : not matching  DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index =
		priv->port_info[port]
			.vlan.ingress_vlan_block_info.final_filters_idx--;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	// Entry 2 : Outer and Inner VLAN tag present : not matching  DISCARD
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	vlan_cfg.entry_index =
		priv->port_info[port]
			.vlan.ingress_vlan_block_info.final_filters_idx--;

	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* VID specific filtering rules which should be executed first before final ones.
	 * Storing starts at the beginning of the block. */

	ret = __get_vlan_vid_filters_idx(priv, port, true, vid, &filter_0, &filter_1, &idx);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	// Entry 0 : Outer and Inner VLAN tag present :  matching  ACCEPT
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_0 >= 0 ?
			filter_0 :
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.ingress_vlan_block_info.vlans[idx]
		.filters_idx[0] = vlan_cfg.entry_index;

	// Entry 1 : Only Outer tag is present : matching  ACCEPT
	memset(&vlan_cfg, 0, sizeof(vlan_cfg));
	vlan_cfg.extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;
	/* if found recycled entry reuse it, otherwise create new one */
	vlan_cfg.entry_index =
		filter_1 >= 0 ?
			filter_1 :
			priv->port_info[port]
				.vlan.ingress_vlan_block_info.vid_filters_idx++;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = vid;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG;
	vlan_cfg.treatment.remove_tag =
		MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK)
		goto EXIT;

	/* store VLAN filtering rules ID's (for VLAN delete, if needed) */
	priv->port_info[port]
		.vlan.ingress_vlan_block_info.vlans[idx]
		.filters_idx[1] = vlan_cfg.entry_index;

	priv->port_info[port].vlan.ingress_vlan_block_info.vlans[idx].vid = vid;
	priv->port_info[port].vlan.ingress_vlan_block_info.vlans[idx].used = true;

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
static void mxl862xx_port_vlan_add(struct dsa_switch *ds, int port,
				  const struct switchdev_obj_port_vlan *vlan)
#else
static int mxl862xx_port_vlan_add(struct dsa_switch *ds, int port,
				  const struct switchdev_obj_port_vlan *vlan,
				  struct netlink_ext_ack *extack)
#endif
{
	int ret = -EINVAL;
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_bridge_port_config_t br_port_cfg = { 0 };
	bool untagged = vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED;
	bool pvid = vlan->flags & BRIDGE_VLAN_INFO_PVID;
	uint8_t cpu_port = priv->hw_info->cpu_port;
	bool vlan_sp_tag = (priv->port_info[cpu_port].tag_protocol == DSA_TAG_PROTO_MXL862_8021Q);
	bool standalone_port = false;
#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
	uint16_t vid = vlan->vid_begin;
#else
	uint16_t vid = vlan->vid;
#endif

	if (port < 0 || port >= MAX_PORTS) {
		dev_err(priv->dev, "invalid port: %d\n", port);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
		NL_SET_ERR_MSG_MOD(extack, "Port out of range");
#endif
		goto EXIT;
	}

	if (!((struct dsa_port *)dsa_to_port(ds, port))) {
		dev_err(ds->dev, "%s:  port:%d is out of DSA domain\n", __func__, port);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
		NL_SET_ERR_MSG_MOD(extack, "Port out of DSA domain");
#endif
		goto EXIT;
	}

	/* standalone port */
	if ((priv->port_info[port].bridge == NULL) && (!dsa_is_cpu_port(ds, port)))
		standalone_port = true;

	if (vid == 0)
		goto EXIT;

	/* If this is request to set pvid, just overwrite it as there may be
	 * only one pid per port */
	if (pvid)
		priv->port_info[port].vlan.pvid = vid;
	/* If this is pvid disable request, check if there's already matching vid
	 * and only then disable it. If vid doesn't match active pvid, don't touch it */
	else {
		if (priv->port_info[port].vlan.pvid == vid)
			priv->port_info[port].vlan.pvid = 0;
	}

	/* Check if there's enough room for ingress and egress rules */
	if ((priv->port_info[port].vlan.ingress_vlan_block_info.final_filters_idx -
			priv->port_info[port].vlan.ingress_vlan_block_info.vid_filters_idx) <
			(priv->port_info[port].vlan.ingress_vlan_block_info.entries_per_vlan)) {

		dev_err(ds->dev,
			"%s: Port:%d vlan:%d. Number of avaliable ingress entries too low. Required:%d  ff_idx:%d vf_idx:%d .\n",
			__func__, port, vid,
			priv->port_info[port].vlan.ingress_vlan_block_info.entries_per_vlan,
			priv->port_info[port].vlan.ingress_vlan_block_info.final_filters_idx,
			priv->port_info[port].vlan.ingress_vlan_block_info.vid_filters_idx);

#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
		ret = -ENOSPC;
		NL_SET_ERR_MSG_MOD(extack, "Reached max number of VLAN ingress filter entries per port");
#endif
		goto EXIT;
	}

	if ((priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx -
			priv->port_info[port].vlan.egress_vlan_block_info.vid_filters_idx) <
			(priv->port_info[port].vlan.egress_vlan_block_info.entries_per_vlan)) {

		dev_err(ds->dev,
			"%s: Port:%d vlan:%d. Number of avaliable egress entries too low. Required:%d  ff_idx:%d vf_idx:%d .\n",
			__func__, port, vid,
			priv->port_info[port].vlan.egress_vlan_block_info.entries_per_vlan,
			priv->port_info[port].vlan.egress_vlan_block_info.final_filters_idx,
			priv->port_info[port].vlan.egress_vlan_block_info.vid_filters_idx);

#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
		ret = -ENOSPC;
		NL_SET_ERR_MSG_MOD(extack, "Reached max number of VLAN egress filter entries per port");
#endif
		goto EXIT;
	}

	/* Although 4-byte vlan special tagging handling is similar to 8 byte MxL tagging,
	 * keep VLAN rules separate for better readibility */
	if (vlan_sp_tag) {
		if (!dsa_is_cpu_port(ds, port)) {
		/* Special rules for CPU port based on user port id */
			ret = __prepare_vlan_ingress_filters_sp_tag_cpu(ds, port, cpu_port);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare ingress filters for VLAN:%d with vlan_filtering disabled\n",
					__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to prepare ingress filters with vlan_filtering disabled");
#endif
				goto EXIT;
			}
			ret = __prepare_vlan_egress_filters_sp_tag_cpu(ds, cpu_port, vid, untagged);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare egress filters for VLAN:%d with vlan_filtering disabled\n",
					__func__, cpu_port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to prepare egress filters with vlan_filtering disabled");
#endif
				goto EXIT;
			}
			/* vlan_filtering disabled */
			/* skiping this configuration for vlan_sp_tag/cpu port as it requires special rules defined above */
			if (!priv->port_info[port].vlan.filtering) {
				dev_info(ds->dev,
					"%s: port:%d setting VLAN:%d with vlan_filtering disabled\n",
					__func__, port, vid);
				ret = __prepare_vlan_ingress_filters_off_sp_tag(ds, port, vid);
				if (ret != MXL862XX_STATUS_OK) {
					dev_err(ds->dev,
						"%s: Port:%d failed to prepare ingress filters for VLAN:%d with vlan_filtering disabled\n",
						__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
					NL_SET_ERR_MSG_MOD(extack, "Failed to prepare ingress filters with vlan_filtering disabled");
#endif
					goto EXIT;
				}

				ret = __prepare_vlan_egress_filters_off_sp_tag(ds, port, vid, untagged);
				if (ret != MXL862XX_STATUS_OK) {
					dev_err(ds->dev,
						"%s: Port:%d failed to prepare egress filters for VLAN:%d with vlan_filtering disabled\n",
						__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
					NL_SET_ERR_MSG_MOD(extack, "Failed to prepare egress filters with vlan_filtering disabled");
#endif
					goto EXIT;
				}
			}
			/* vlan_filtering enabled */
			else {
				/* special rules for the CPU port are already defined,
				 * so define only the rules for user ports */
				ret = __prepare_vlan_ingress_filters_sp_tag(ds, port, vid);
				if (ret != MXL862XX_STATUS_OK) {
					dev_err(ds->dev,
						"%s: Port:%d failed to prepare ingress filters for VLAN:%d\n",
						__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
					NL_SET_ERR_MSG_MOD(extack, "Failed to prepare ingress filters for VLAN");
#endif
					goto EXIT;
				}

				ret = __prepare_vlan_egress_filters_sp_tag(ds, port, vid, untagged);
				if (ret != MXL862XX_STATUS_OK) {
					dev_err(ds->dev,
						"%s: Port:%d failed to prepare egress filters for VLAN:%d\n",
						__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
					NL_SET_ERR_MSG_MOD(extack, "Failed to prepare egress filters for VLAN");
#endif
					goto EXIT;
				}
			}
		} else {
			/* CPU port. This else block handles explicit request for adding
			 * VLAN to CPU port. Only egress rule requires reconfiguration.*/
			ret = __prepare_vlan_egress_filters_sp_tag_cpu(ds, cpu_port, vid, untagged);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare egress filters for VLAN:%d with vlan_filtering disabled\n",
					__func__, cpu_port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to prepare egress filters with vlan_filtering disabled");
#endif
				goto EXIT;
			}
		}

		/* CPU port is explicitely added by the DSA framework to the new vlans.
		   Apply block_id with filtering rules defined while processing user ports
			For 8021q special tag mode cpu port rules may change because of new ports added,
			so they need to be reloaded */
		{
			mxl862xx_ctp_port_config_t ctp_param = { 0 };

			ctp_param.logical_port_id = cpu_port + 1;
			ctp_param.mask = MXL862XX_CTP_PORT_CONFIG_MASK_EGRESS_VLAN |
					     MXL862XX_CTP_PORT_CONFIG_MASK_INGRESS_VLAN;
			ctp_param.egress_extended_vlan_enable = true;
			ctp_param.egress_extended_vlan_block_id =
			priv->port_info[cpu_port].vlan.egress_vlan_block_info.block_id;
			ctp_param.ingress_extended_vlan_enable = true;
			ctp_param.ingress_extended_vlan_block_id =
				priv->port_info[cpu_port].vlan.ingress_vlan_block_info.block_id;

			ret = mxl862xx_ctp_port_config_set(&mxl_dev, &ctp_param);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: CTP port %d config failed on port config set with %d\n",
					__func__, cpu_port, ret);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to configure VLAN for cpu port");
#endif
				goto EXIT;
			}
		}
	}

	/* VLAN rules for 8 byte MxL tagging*/
	else {
		/* vlan_filtering disabled */
		/* skiping this configuration for vlan_sp_tag/cpu port as it requires special rules defined above */
		if (!priv->port_info[port].vlan.filtering) {
			ret = __prepare_vlan_ingress_filters_off(priv, port, vid);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare ingress filters for VLAN:%d with vlan_filtering disabled\n",
					__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to prepare ingress filters with vlan_filtering disabled");
#endif
				goto EXIT;
			}

			ret = __prepare_vlan_egress_filters_off(priv, port, vid, untagged);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare egress filters for VLAN:%d with vlan_filtering disabled\n",
					__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to prepare egress filters with vlan_filtering disabled");
#endif
				goto EXIT;
			}
		}
		/* vlan_filtering enabled */
		else {
			ret = __prepare_vlan_ingress_filters(ds, port, vid);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare ingress filters for VLAN:%d\n",
					__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to prepare ingress filters for VLAN");
#endif
				goto EXIT;
			}
			ret = __prepare_vlan_egress_filters(ds, port, vid, untagged);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: Port:%d failed to prepare egress filters for VLAN:%d\n",
					__func__, port, vid);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to prepare egress filters for VLAN");
#endif
				goto EXIT;
			}
		}

		/* CPU port is explicitely added by the DSA framework to new vlans */
		if (dsa_is_cpu_port(ds, port)) {
			mxl862xx_ctp_port_config_t ctp_param = { 0 };

			ctp_param.logical_port_id = port + 1;
			ctp_param.mask = MXL862XX_CTP_PORT_CONFIG_MASK_EGRESS_VLAN |
					     MXL862XX_CTP_PORT_CONFIG_MASK_INGRESS_VLAN;
			ctp_param.egress_extended_vlan_enable = true;
			ctp_param.egress_extended_vlan_block_id =
				priv->port_info[port].vlan.egress_vlan_block_info.block_id;
			ctp_param.ingress_extended_vlan_enable = true;
			ctp_param.ingress_extended_vlan_block_id =
				priv->port_info[port].vlan.ingress_vlan_block_info.block_id;

			ret = mxl862xx_ctp_port_config_set(&mxl_dev, &ctp_param);
			if (ret != MXL862XX_STATUS_OK) {
				dev_err(ds->dev,
					"%s: CTP port %d config failed on port config set with %d\n",
					__func__, port, ret);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
				NL_SET_ERR_MSG_MOD(extack, "Failed to configure VLAN for cpu port");
#endif
				goto EXIT;
			}

			goto EXIT;
		}
	}

	/* Update bridge port */
	br_port_cfg.bridge_port_id = port + 1;
	br_port_cfg.mask |= MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN |
			     MXL862XX_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN |
				  MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING;
	br_port_cfg.egress_extended_vlan_enable = true;
	br_port_cfg.egress_extended_vlan_block_id =
		priv->port_info[port].vlan.egress_vlan_block_info.block_id;
	br_port_cfg.ingress_extended_vlan_enable = true;
	br_port_cfg.ingress_extended_vlan_block_id =
		priv->port_info[port].vlan.ingress_vlan_block_info.block_id;

	/* Disable MAC learning for standalone ports. */
	br_port_cfg.src_mac_learning_disable =
				(standalone_port) ? true : false;

	ret = mxl862xx_bridge_port_config_set(&mxl_dev, &br_port_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		dev_err(ds->dev,
			"%s: Bridge port configuration for port %d failed with %d\n",
			__func__, port, ret);
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
		NL_SET_ERR_MSG_MOD(extack, "Bridge port configuration for VLAN failed");
#endif
		goto EXIT;
	}

EXIT:
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
	return ret;
#else
	return;
#endif
}

static int __deactivate_vlan_filter_entry(u16 block_id, u16 entry_idx)
{
	int ret = -EINVAL;
	mxl862xx_extendedvlan_config_t vlan_cfg = { 0 };

	/* Set default reset values as it makes the rule transparent */
	vlan_cfg.extended_vlan_block_id = block_id;
	vlan_cfg.entry_index = entry_idx;
	vlan_cfg.filter.outer_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.outer_vlan.priority_enable = true;
	vlan_cfg.filter.outer_vlan.priority_val = 0;
	vlan_cfg.filter.outer_vlan.vid_enable = true;
	vlan_cfg.filter.outer_vlan.vid_val = 0;
	vlan_cfg.filter.inner_vlan.type = MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL;
	vlan_cfg.filter.inner_vlan.priority_enable = true;
	vlan_cfg.filter.inner_vlan.priority_val = 0;
	vlan_cfg.filter.inner_vlan.vid_enable = true;
	vlan_cfg.filter.inner_vlan.vid_val = 0;
	vlan_cfg.treatment.add_outer_vlan = true;
	vlan_cfg.treatment.outer_vlan.priority_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.outer_vlan.priority_val = 0;
	vlan_cfg.treatment.add_inner_vlan = true;
	vlan_cfg.treatment.inner_vlan.priority_mode = MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL;
	vlan_cfg.treatment.inner_vlan.priority_val = 0;

	ret = mxl862xx_extended_vlan_set(&mxl_dev, &vlan_cfg);
	if (ret != MXL862XX_STATUS_OK) {
		pr_err("%s: failed to deactivate entry:%d for VLAN block ID:%d\n",
			__func__, vlan_cfg.entry_index, vlan_cfg.extended_vlan_block_id);
		goto exit;
	}

exit:
	return ret;
}

static int mxl862xx_port_vlan_del(struct dsa_switch *ds, int port,
				  const struct switchdev_obj_port_vlan *vlan)
{
	int ret = -EINVAL;
	int dir;
	struct mxl862xx_priv *priv = ds->priv;
#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
	uint16_t vid = vlan->vid_begin;
#else
	uint16_t vid = vlan->vid;
#endif

	for (dir = 0 ; dir < 2 ; dir++) {
		struct mxl862xx_extended_vlan_block_info *block_info = (dir == 0)
			? &priv->port_info[port].vlan.ingress_vlan_block_info
			: &priv->port_info[port].vlan.egress_vlan_block_info;
		char *dir_txt = (dir == 0)	? "ingress" : "egress";
		int16_t entry_idx;
		int vlan_idx, x;
		u16 block_id = block_info->block_id;
		/* Indicator of the last dynamic vid related entry being processed.
		 * Required for cleanup of static rules at the end of the block. */
		bool last_vlan = false;
		bool vlan_found = false;

		/* check if vlan is present */
		for (vlan_idx = 0; vlan_idx < MAX_VLANS; vlan_idx++) {
			if ((block_info->vlans[vlan_idx].vid == vid)
					&& block_info->vlans[vlan_idx].used)
				vlan_found = true;

			if (vlan_idx == MAX_VLANS - 1)
				last_vlan = true;

			if (vlan_found)
				break;
		}

		if (!vlan_found) {
			dev_err(ds->dev, "%s: Port:%d VLAN:%d not found (%s)\n", __func__, port, vid, dir_txt);
			goto static_rules_cleanup;
		}

		/* cleanup */
		for (x = 0; x < VID_RULES ; x++) {
			entry_idx = block_info->vlans[vlan_idx].filters_idx[x];
			if (entry_idx != IDX_INVAL) {
				ret = __deactivate_vlan_filter_entry(block_id, entry_idx);
				if (ret != MXL862XX_STATUS_OK)
					goto EXIT;
			}
		}

		/* cleanup of the vlan record in the port vlan inventory */
		block_info->vlans[vlan_idx].vid = 0;
		block_info->vlans[vlan_idx].used = false;

		/* find the first free slot for storing recycled filter entries */
		for (x = 0; x < MAX_VLANS; x++) {
			if (!(block_info->filter_entries_recycled[x].valid)) {
				block_info->filter_entries_recycled[x].filters_idx[0] = block_info->vlans[vlan_idx].filters_idx[0];
				block_info->filter_entries_recycled[x].filters_idx[1] = block_info->vlans[vlan_idx].filters_idx[1];
				block_info->filter_entries_recycled[x].valid = true;
				block_info->vlans[vlan_idx].filters_idx[0] = IDX_INVAL;
				block_info->vlans[vlan_idx].filters_idx[1] = IDX_INVAL;
				break;
			}

			if (x == MAX_VLANS - 1) {
				ret = -ENOSPC;
				dev_err(ds->dev,
					"%s: Port:%d no free slots for recycled %s filter entries\n",
					__func__, port, dir_txt);
				goto EXIT;
			}
		}

static_rules_cleanup:
		/* If this is the last vlan entry or no entries left,
		 * remove static entries (placed at the end of the block) */
		if (last_vlan) {
			for (entry_idx = block_info->final_filters_idx; entry_idx < block_info->filters_max ; entry_idx++) {
				ret = __deactivate_vlan_filter_entry(block_id, entry_idx);
				if (ret != MXL862XX_STATUS_OK)
					goto EXIT;
			}
			/* Entries cleared, so point out to the end */
			block_info->final_filters_idx = entry_idx;
		}
	}

/*  The block release is not needed as the blocks/entries are distributed
 *  evenly for all ports so it's static assignment. */

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 18, 0) > LINUX_VERSION_CODE)
static int mxl862xx_port_fdb_add(struct dsa_switch *ds, int port,
				 const unsigned char *addr, u16 vid)
#else
static int mxl862xx_port_fdb_add(struct dsa_switch *ds, int port,
				 const unsigned char *addr, u16 vid, struct dsa_db db)
#endif
{
	int ret = -EINVAL;
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_mac_table_add_t mac_table_add = { 0 };
	uint8_t i = 0;

	memcpy(mac_table_add.mac, addr, ETH_ALEN);

	mac_table_add.port_id = port + 1;
	mac_table_add.tci = (vid & 0xFFF);
	mac_table_add.static_entry = true;

	/* For CPU port add entries corresponding to all FIDs */
	for (i = 0; i < priv->hw_info->phy_ports; i++) {

		if (!(dsa_is_cpu_port(ds, port)))
			i = port;
		/* Bypass entry add for the isolated port as it may turn back
		 * the traffic originated on the host to the cpu port */
		if (priv->port_info[i].isolated)
			continue;

		mac_table_add.fid = priv->port_info[i].bridgeID;
		ret = mxl862xx_mac_table_entry_add(&mxl_dev, &mac_table_add);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev, "%s: Port:%d failed to add MAC table entry for FID:%d\n",
				__func__, port, mac_table_add.fid);
			goto EXIT;
		}

	if (!(dsa_is_cpu_port(ds, port)))
		break;
	}

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 18, 0) > LINUX_VERSION_CODE)
static int mxl862xx_port_fdb_del(struct dsa_switch *ds, int port,
				 const unsigned char *addr, u16 vid)
#else
static int mxl862xx_port_fdb_del(struct dsa_switch *ds, int port,
				 const unsigned char *addr, u16 vid, struct dsa_db db)
#endif
{
	int ret = -EINVAL;
	struct mxl862xx_priv *priv = ds->priv;
	mxl862xx_mac_table_remove_t mac_table_remove = { 0 };
	uint8_t i = 0;

	memcpy(mac_table_remove.mac, addr, ETH_ALEN);
	mac_table_remove.tci = (vid & 0xFFF);

	/* For CPU port remove entries corresponding to all FIDs */
	for (i = 0; i < priv->hw_info->phy_ports; i++) {
		if (!(dsa_is_cpu_port(ds, port)))
			i = port;
		mac_table_remove.fid = priv->port_info[i].bridgeID;
		ret = mxl862xx_mac_table_entry_remove(&mxl_dev, &mac_table_remove);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev, "%s: Port:%d failed to remove MAC table entry for FID:%d\n",
				__func__, port, mac_table_remove.fid);
			goto EXIT;
		}

		if (!(dsa_is_cpu_port(ds, port)))
			break;
	}

EXIT:
	return ret;
}

static int mxl862xx_port_fdb_dump(struct dsa_switch *ds, int port,
				  dsa_fdb_dump_cb_t *cb, void *data)
{
	int ret = -EINVAL;
	mxl862xx_mac_table_read_t mac_table_read = { 0 };

	mac_table_read.initial = 1;

	for (;;) {
		ret = mxl862xx_mac_table_entry_read(&mxl_dev, &mac_table_read);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: Port:%d failed to read MAC table entry\n",
				__func__, port);
			goto EXIT;
		}

		if (mac_table_read.last == 1)
			break;

		if (mac_table_read.port_id == port + 1)
			cb(mac_table_read.mac, mac_table_read.tci & 0x0FFF,
			   mac_table_read.static_entry, data);

		memset(&mac_table_read, 0, sizeof(mac_table_read));
	}

EXIT:
	return ret;
}

#if (KERNEL_VERSION(5, 11, 0) < LINUX_VERSION_CODE)
static int mxl862xx_port_pre_bridge_flags(struct dsa_switch *ds, int port,
	struct switchdev_brport_flags flags, struct netlink_ext_ack *extack)
{
	int ret = 0;

	if (flags.mask & ~(BR_FLOOD | BR_MCAST_FLOOD | BR_BCAST_FLOOD | BR_LEARNING)) {
		dev_err(ds->dev, "%s: Port:%d unsupported bridge flags:0x%lx\n",
				__func__, port, flags.mask);
		if (flags.mask & ~(BR_FLOOD | BR_MCAST_FLOOD | BR_BCAST_FLOOD | BR_LEARNING)) {
			ret = -EINVAL;
			NL_SET_ERR_MSG_MOD(extack, "Unsupported bridge flags:0x%lx");
		}
	}

	return ret;
}


static int mxl862xx_port_bridge_flags(struct dsa_switch *ds, int port,
	struct switchdev_brport_flags flags, struct netlink_ext_ack *extack)
{
	int ret = 0;
	uint16_t bridge_id;
	struct mxl862xx_priv *priv = ds->priv;
	bool bridge_ctx = true;

	if (!dsa_is_user_port(ds, port))
		return 0;

	/* .port_pre_bridge_flags is called after this function,
	 *  so the supported flags check is needed also here */
	if (flags.mask & ~(BR_FLOOD | BR_MCAST_FLOOD | BR_BCAST_FLOOD | BR_LEARNING)) {
		dev_err(ds->dev, "%s: Port:%d unsupported bridge flags:0x%lx\n",
				__func__, port, flags.mask);
		if (flags.mask & ~(BR_FLOOD | BR_MCAST_FLOOD | BR_BCAST_FLOOD | BR_LEARNING)) {
			ret = -EINVAL;
			goto EXIT;
		}
	}

	bridge_id = priv->port_info[port].bridgeID;
	if ((bridge_id == 0) || (priv->port_info[port].bridge == NULL))
		bridge_ctx = false;

	/* Handle flooding flags (bridge context) */
	if (bridge_ctx && (flags.mask & (BR_FLOOD|BR_MCAST_FLOOD|BR_BCAST_FLOOD))) {
		mxl862xx_bridge_config_t bridge_config = { 0 };

		bridge_config.mask = MXL862XX_BRIDGE_CONFIG_MASK_FORWARDING_MODE;
		bridge_config.bridge_id = bridge_id;

		if (flags.mask & BR_FLOOD)
			bridge_config.forward_unknown_unicast = (flags.val & BR_FLOOD) ?
				MXL862XX_BRIDGE_FORWARD_FLOOD : MXL862XX_BRIDGE_FORWARD_DISCARD;
		if (flags.mask & BR_MCAST_FLOOD) {
			bridge_config.forward_unknown_multicast_ip = (flags.val & BR_MCAST_FLOOD) ?
				MXL862XX_BRIDGE_FORWARD_FLOOD : MXL862XX_BRIDGE_FORWARD_DISCARD;
			bridge_config.forward_unknown_multicast_non_ip = bridge_config.forward_unknown_multicast_ip;
		}
		if (flags.mask & BR_BCAST_FLOOD)
			bridge_config.forward_broadcast = (flags.val & BR_BCAST_FLOOD) ?
			MXL862XX_BRIDGE_FORWARD_FLOOD : MXL862XX_BRIDGE_FORWARD_DISCARD;

		ret = mxl862xx_bridge_config_set(&mxl_dev, &bridge_config);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev, "%s: Port:%d bridge:%d configuration  failed\n",
				__func__, port, bridge_config.bridge_id);
			NL_SET_ERR_MSG_MOD(extack, "Configuration of bridge flooding flags failed");
			goto EXIT;
		}
	}
	/* Handle learning flag (bridge port context) */
	if (flags.mask & BR_LEARNING) {
		mxl862xx_bridge_port_config_t br_port_cfg = { 0 };

		br_port_cfg.mask =	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING;
		br_port_cfg.bridge_port_id = port + 1;
		br_port_cfg.src_mac_learning_disable = (flags.val & BR_LEARNING) ? false : true;
		ret = mxl862xx_bridge_port_config_set(&mxl_dev, &br_port_cfg);
		if (ret != MXL862XX_STATUS_OK) {
			dev_err(ds->dev,
				"%s: MAC learning disable for port %d failed with ret=%d\n",
				__func__, port, ret);
			NL_SET_ERR_MSG_MOD(extack, "Configuration of bridge port learning flags failed");
			goto EXIT;
		}
	}

EXIT:
	return ret;
}
#endif

#if (KERNEL_VERSION(5, 11, 0) < LINUX_VERSION_CODE)
#if (KERNEL_VERSION(5, 19, 0) > LINUX_VERSION_CODE)
static int mxl862xx_change_tag_protocol(struct dsa_switch *ds, int port,
				     enum dsa_tag_protocol proto)
#else
static int mxl862xx_change_tag_protocol(struct dsa_switch *ds,
				     enum dsa_tag_protocol proto)
#endif
{
	int ret = MXL862XX_STATUS_OK;

	dev_info(ds->dev, "%s: DSA tag protocol change not supported\n",  __func__);
	return ret;
}
#endif

#if (KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE)
static enum dsa_tag_protocol mxl862xx_get_tag_protocol(struct dsa_switch *ds,
						       int port)
#else
static enum dsa_tag_protocol mxl862xx_get_tag_protocol(struct dsa_switch *ds,
						       int port, enum dsa_tag_protocol m)
#endif
{
	enum dsa_tag_protocol tag_proto;

	tag_proto = __dt_parse_tag_proto(ds, port);

	return tag_proto;
}

static const struct dsa_switch_ops mxl862xx_switch_ops = {
	.get_ethtool_stats = mxl862xx_get_ethtool_stats,
	.get_strings = mxl862xx_get_strings,
	.get_sset_count = mxl862xx_get_sset_count,
#if (KERNEL_VERSION(5, 11, 0) < LINUX_VERSION_CODE)
	.change_tag_protocol	= mxl862xx_change_tag_protocol,
#endif
	.get_tag_protocol = mxl862xx_get_tag_protocol,
	.phy_read = mxl862xx_phy_read,
	.phy_write = mxl862xx_phy_write,
#if (KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE)
	.phylink_mac_config = mxl862xx_phylink_mac_config,
	.phylink_mac_link_down = mxl862xx_phylink_mac_link_down,
	.phylink_mac_link_up = mxl862xx_phylink_mac_link_up,
#if (KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE)
	.phylink_validate = mxl862xx_phylink_validate,
#else
	.phylink_get_caps = mxl862xx_phylink_get_caps,
#endif
#endif
	.set_ageing_time = mxl862xx_set_ageing_time,
	.port_bridge_join = mxl862xx_port_bridge_join,
	.port_bridge_leave = mxl862xx_port_bridge_leave,
	.port_disable = mxl862xx_port_disable,
	.port_enable = mxl862xx_port_enable,
	.port_fast_age = mxl862xx_port_fast_age,
	.port_stp_state_set = mxl862xx_port_stp_state_set,
	.port_mirror_add = mxl862xx_port_mirror_add,
	.port_mirror_del = mxl862xx_port_mirror_del,
#if (KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE)
	.port_vlan_prepare	= mxl862xx_port_vlan_prepare,
#endif
	.port_vlan_filtering = mxl862xx_port_vlan_filtering,
	.port_vlan_add = mxl862xx_port_vlan_add,
	.port_vlan_del = mxl862xx_port_vlan_del,
	.port_fdb_add = mxl862xx_port_fdb_add,
	.port_fdb_del = mxl862xx_port_fdb_del,
	.port_fdb_dump = mxl862xx_port_fdb_dump,
#if (KERNEL_VERSION(5, 11, 0) < LINUX_VERSION_CODE)
	.port_pre_bridge_flags = mxl862xx_port_pre_bridge_flags,
	.port_bridge_flags = mxl862xx_port_bridge_flags,
#endif
	.setup = mxl862xx_setup,
};

static int mxl862xx_probe(struct mdio_device *mdiodev)
{
	struct device *dev = &mdiodev->dev;
	struct mxl862xx_priv *priv;
	struct dsa_switch *ds;
	int ret;
	struct sys_fw_image_version sys_img_ver = { 0 };

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "%s: Error allocating mxl862xx switch\n",
			__func__);
		return -ENOMEM;
	}

	priv->dev = dev;
	priv->bus = mdiodev->bus;
	priv->sw_addr = mdiodev->addr;
	priv->hw_info = of_device_get_match_data(dev);
	if (!priv->hw_info)
		return -EINVAL;

	mxl_dev.bus = mdiodev->bus;
	mxl_dev.sw_addr = mdiodev->addr;

#if (KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE)
	mutex_init(&priv->pce_table_lock);
#endif

	ret = sys_misc_fw_version(&mxl_dev, &sys_img_ver);
	if (ret < 0)
		dev_err(dev, "\t%40s:\t0x%x\n",
			"fapi_GSW_FW_Version failed with ret code", ret);
	else {
		dev_info(dev, "\t%40s:\t%x\n", "Iv Major",
			 sys_img_ver.iv_major);
		dev_info(dev, "\t%40s:\t%x\n", "Iv Minor",
			 sys_img_ver.iv_minor);
		dev_info(dev, "\t%40s:\t%u\n", "Revision",
			 sys_img_ver.iv_revision);
		dev_info(dev, "\t%40s:\t%u\n", "Build Num",
			 sys_img_ver.iv_build_num);
	}

#if (KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE)
	ds = devm_kzalloc(dev, sizeof(*ds), GFP_KERNEL);
	if (!ds) {
		dev_err(dev, "%s: Error allocating DSA switch\n", __func__);
		return -ENOMEM;
	}

	ds->dev = dev;
	ds->num_ports = priv->hw_info->max_ports;
#else
	ds = dsa_switch_alloc(&mdiodev->dev, priv->hw_info->max_ports);
#endif
	ds->priv = priv;
	ds->ops = priv->hw_info->ops;
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	ds->assisted_learning_on_cpu_port = true;
#endif

	dev_set_drvdata(dev, ds);

	ret = dsa_register_switch(ds);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "%s: Error %d register DSA switch\n",
				__func__, ret);
		return ret;
	}

	if (!dsa_is_cpu_port(ds, priv->hw_info->cpu_port)) {
		dev_err(dev,
			"wrong CPU port defined, HW only supports port: %i",
			priv->hw_info->cpu_port);
		ret = -EINVAL;
		dsa_unregister_switch(ds);
	}

	return ret;
}

static void mxl862xx_remove(struct mdio_device *mdiodev)
{
	struct dsa_switch *ds = dev_get_drvdata(&mdiodev->dev);

	dsa_unregister_switch(ds);

}

static const struct mxl862xx_hw_info mxl86282_data = {
	.max_ports = 9,
	.phy_ports = 8,
	.cpu_port = 8,
	.ops = &mxl862xx_switch_ops,
};

static const struct mxl862xx_hw_info mxl86252_data = {
	.max_ports = 9,
	.phy_ports = 5,
	.cpu_port = 8,
	.ops = &mxl862xx_switch_ops,
};

static const struct of_device_id mxl862xx_of_match[] = {
	{ .compatible = "mxl,86282", .data = &mxl86282_data },
	{ .compatible = "mxl,86252", .data = &mxl86252_data },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, mxl862xx_of_match);

static struct mdio_driver mxl862xx_driver = {
	.probe  = mxl862xx_probe,
	.remove = mxl862xx_remove,
	.mdiodrv.driver = {
		.name = "mxl862xx",
		.of_match_table = mxl862xx_of_match,
	},
};

mdio_module_driver(mxl862xx_driver);

MODULE_DESCRIPTION("Driver for MaxLinear MxL862xx switch family");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mxl862xx");
