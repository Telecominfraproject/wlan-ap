// SPDX-License-Identifier: GPL-2.0
#include <linux/mutex.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/delay.h>
#include <net/dsa.h>
#include <linux/bitops.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>

#include "realtek-smi-rtl8373.h"
#include "rtl8373_core.h"
#include "rtl8373_lut.h"
#include "rtl8373_interrupt.h"
#include "rtl8373_switch.h"
#include "rtl8373_vlan.h"

static void _dal_rtl8373_Vlan4kStUser2Smi(struct rtl8373_vlan_4k *pUserVlan4kEntry,
					  u32 *pSmiVlan4kEntry)
{
	u32 regValue = 0;

	regValue |= (pUserVlan4kEntry->mbr & 0x3FF);
	regValue |= (pUserVlan4kEntry->untag & 0x3FF) << 10;
	regValue |= (pUserVlan4kEntry->fid_msti & 0xF) << 20;
	regValue |= (pUserVlan4kEntry->svlan_chk_ivl_svl & 0x1) << 24;
	regValue |= (pUserVlan4kEntry->ivl_svl & 0x1) << 25;

	*pSmiVlan4kEntry = regValue;
}

static void _dal_rtl8373_Vlan4kStSmi2User(u32 smiVlan4kEntry,
					  struct rtl8373_vlan_4k *pUserVlan4kEntry)
{
	pUserVlan4kEntry->mbr = (u16)(smiVlan4kEntry & 0x3FF);
	pUserVlan4kEntry->untag = (u16)((smiVlan4kEntry >> 10) & 0x3FF);
	pUserVlan4kEntry->fid_msti = (u16)((smiVlan4kEntry >> 20) & 0xF);
	pUserVlan4kEntry->svlan_chk_ivl_svl = (u16)((smiVlan4kEntry >> 24) & 0x1);
	pUserVlan4kEntry->ivl_svl = (u16)((smiVlan4kEntry >> 25) & 0x1);
}

static int _rtl8373_setAsicVlan4kEntry(struct regmap *map,
				       struct rtl8373_vlan_4k *pVlan4kEntry)
{
	u32 vlanEntryVal = 0;
	u32 regData = 0;
	int ret;

	if (!map || !pVlan4kEntry)
		return -EINVAL;

	_dal_rtl8373_Vlan4kStUser2Smi(pVlan4kEntry, &vlanEntryVal);

	ret = regmap_read_poll_timeout(map, RTL8373_ITA_CTRL0_ADDR, regData,
				       !(regData & BIT(RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET)),
				       10, 100);
	if (ret)
		return ret;

	ret = rtl8373_setAsicReg(map, RTL8373_ITA_WRITE_DATA0_ADDR(0), vlanEntryVal);
	if (ret)
		return ret;

	regData = (pVlan4kEntry->vid << RTL8373_ITA_CTRL0_TBL_ADDR_OFFSET);
	regData |= (TB_TARGET_CVLAN << RTL8373_ITA_CTRL0_TLB_TYPE_OFFSET);
	regData |= (TB_OP_WRITE << RTL8373_ITA_CTRL0_TLB_ACT_OFFSET);
	regData |= (TB_EXECUTE << RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET);

	ret = rtl8373_setAsicReg(map, RTL8373_ITA_CTRL0_ADDR, regData);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(map, RTL8373_ITA_CTRL0_ADDR, regData,
				       !(regData & BIT(RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET)),
				       10, 100);
	if (ret)
		return ret;

	return 0;
}

static int _rtl8373_getAsicVlan4kEntry(struct regmap *map,
				       struct rtl8373_vlan_4k *pVlan4kEntry)
{
	int ret;
	u32 regData = 0;

	if (!map || !pVlan4kEntry)
		return -EINVAL;

	ret = regmap_read_poll_timeout(map, RTL8373_ITA_CTRL0_ADDR, regData,
				       !(regData & BIT(RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET)),
				       10, 100);
	if (ret)
		return ret;

	regData = pVlan4kEntry->vid;
	ret = rtl8373_setAsicRegBits(map, RTL8373_ITA_CTRL0_ADDR,
				     RTL8373_ITA_CTRL0_TBL_ADDR_MASK, regData);
	if (ret)
		return ret;

	ret = rtl8373_setAsicRegBit(map, RTL8373_ITA_CTRL0_ADDR,
				    RTL8373_ITA_CTRL0_TLB_ACT_OFFSET, TB_OP_READ);
	if (ret)
		return ret;

	ret = rtl8373_setAsicRegBits(map, RTL8373_ITA_CTRL0_ADDR,
				     RTL8373_ITA_CTRL0_TLB_TYPE_MASK, TB_TARGET_CVLAN);
	if (ret)
		return ret;

	ret = rtl8373_setAsicRegBit(map, RTL8373_ITA_CTRL0_ADDR,
				    RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET, TB_EXECUTE);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(map, RTL8373_ITA_CTRL0_ADDR, regData,
				       !(regData & BIT(RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET)),
				       10, 100);
	if (ret)
		return ret;

	ret = rtl8373_getAsicReg(map, RTL8373_ITA_READ_DATA0_ADDR(0), &regData);
	if (ret)
		return ret;

	_dal_rtl8373_Vlan4kStSmi2User(regData, pVlan4kEntry);

	return 0;
}

int rtl8373_vlan_tagMode_set(struct realtek_smi *priv, int port,
			     rtk_vlan_egressTagMode_t tagMode)
{
	if (!priv)
		return -EINVAL;

	/* port is physical port */
	if (port < 0 || port > priv->end_pports)
		return -EINVAL;

	if (tagMode >= VLAN_EGRESS_TAG_MODE_END)
		return RT_ERR_VLAN_TAGMODE;

	return rtl8373_setAsicRegBits(priv->map,
				      RTL8373_VLAN_PORT_EGR_TAG_ADDR(port),
				      RTL8373_VLAN_PORT_EGR_TAG_MODE_MASK(port),
				      tagMode);
}


static int rtl8373_set_vlan(struct realtek_smi *priv, int vid, u32 member,
			    u32 untag, u32 fid_msti, u32 svlan_chk_ivl_svl,
			    u32 ivl_svl)
{
	struct rtl8373_vlan_4k vlan4k;
	int ret;

	if (!priv->ops->is_vlan_valid(priv, vid))
		return -EINVAL;

	dev_dbg(priv->dev,
		"setting VLAN%d 4k members: 0x%02x, untagged: 0x%02x\n",
		vid, member, untag);

	memset(&vlan4k, 0, sizeof(vlan4k));
	vlan4k.vid = (u16)vid;
	vlan4k.mbr = member;
	vlan4k.untag = untag;
	vlan4k.svlan_chk_ivl_svl = svlan_chk_ivl_svl;
	vlan4k.ivl_svl = ivl_svl;
	vlan4k.fid_msti = fid_msti;

	ret = _rtl8373_setAsicVlan4kEntry(priv->map, &vlan4k);
	if (ret)
		return ret;

	dev_dbg(priv->dev,
		"resulting VLAN%d 4k members: 0x%02x, untagged: 0x%02x\n",
		vid, vlan4k.mbr, vlan4k.untag);

	return 0;
}

static int rtl8373_set_pvid(struct realtek_smi *priv, unsigned int port,
			    unsigned int vid)
{
	int ret;
	int pport;

	if (!priv->ops->is_vlan_valid(priv, vid))
		return -EINVAL;

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return -EINVAL;

	ret = rtl8373_setAsicRegBits(priv->map,
				     RTL8373_VLAN_PORT_PB_VLAN_ADDR(pport),
				     RTL8373_VLAN_PORT_PB_VLAN_PVID_MASK(pport),
				     vid);
	return ret;
}

static int rtl8373_enable_vlan4k(struct realtek_smi *priv, bool enable)
{
	int ret;

	if (enable) {
		ret = priv->ops->enable_vlan(priv, true);
		if (ret)
			return ret;

		priv->vlan_enabled = true;
	}

	ret = priv->ops->enable_vlan4k(priv, enable);
	if (ret)
		return ret;

	priv->vlan4k_enabled = enable;
	return 0;
}

static int rtl8373_enable_vlan(struct realtek_smi *priv, bool enable)
{
	int ret;

	ret = priv->ops->enable_vlan(priv, enable);
	if (ret)
		return ret;

	priv->vlan_enabled = enable;

	if (!enable) {
		priv->vlan4k_enabled = false;
		ret = priv->ops->enable_vlan4k(priv, false);
	}

	return ret;
}

static int rtl8373_reset_vlan(struct realtek_smi *priv)
{
	int ret;

	if (!priv)
		return -EINVAL;

	ret = rtl8373_enable_vlan(priv, false);
	if (ret)
		return ret;

	ret = rtl8373_enable_vlan4k(priv, false);
	if (ret)
		return ret;

	ret = rtl8373_setAsicRegBit(priv->map, RTL8373_VLAN_CTRL_ADDR,
				    RTL8373_VLAN_CTRL_TABLE_RST_OFFSET, true);
	if (ret)
		return ret;

	return 0;
}

int rtl8373_port_vlan_del(struct dsa_switch *ds, int port,
			  const struct switchdev_obj_port_vlan *vlan)
{
	struct rtl8373_vlan_4k vlan4k;
	struct realtek_smi *priv = ds->priv;
	u32 pvid;
	int pport;
	int ret = 0;

	if (!priv || !vlan)
		return -EINVAL;

	if (!priv->ops->is_vlan_valid(priv, vlan->vid))
		return -EINVAL;

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return -EINVAL;

	mutex_lock(&priv->map_lock);

	ret = rtl8373_getAsicRegBits(priv->map,
				     RTL8373_VLAN_PORT_PB_VLAN_ADDR(pport),
				     RTL8373_VLAN_PORT_PB_VLAN_PVID_MASK(pport),
				     &pvid);
	if (ret) {
		dev_err(priv->dev, "failed to read PVID of port %d\n", port);
		goto out;
	}

	/*
         * switchdev semantics: the bridge may delete the PVID VLAN (e.g. port
         * leaving the bridge, netifd/ucentral reconfiguring, reboot). Refusing
         * with -EBUSY leaves the entry in the bridge VLAN group and triggers
         * WARN_ON in __vlan_group_free() (br_vlan.c), leaking the vlan objects.
         * Instead drop the PVID ("no PVID") and go on removing the membership.
         */
        if (pvid == vlan->vid) {
                ret = rtl8373_setAsicRegBits(priv->map,
                                             RTL8373_VLAN_PORT_PB_VLAN_ADDR(pport),
                                             RTL8373_VLAN_PORT_PB_VLAN_PVID_MASK(pport),
                                             0);
                if (ret) {
                        dev_err(priv->dev, "failed to clear PVID of port %d\n", port);
                        goto out;
                }
                dev_dbg(priv->dev, "port %d: VLAN %u was PVID, PVID cleared\n",
                        port, vlan->vid);
        }

	memset(&vlan4k, 0, sizeof(vlan4k));
	vlan4k.vid = vlan->vid;

	ret = _rtl8373_getAsicVlan4kEntry(priv->map, &vlan4k);
	if (ret) {
		dev_err(priv->dev, "failed to read VLAN %04x\n", vlan4k.vid);
		goto out;
	}

	vlan4k.mbr &= ~BIT(pport);
	vlan4k.untag &= ~BIT(pport);

	if (vlan4k.mbr == 0) {
		vlan4k.untag = 0;
		vlan4k.svlan_chk_ivl_svl = 0;
		vlan4k.ivl_svl = 0;
		vlan4k.fid_msti = 0;
	} else {
		vlan4k.svlan_chk_ivl_svl = 0;
		vlan4k.ivl_svl = 1;
		vlan4k.fid_msti = 0;
	}

	ret = _rtl8373_setAsicVlan4kEntry(priv->map, &vlan4k);
	if (ret) {
		dev_err(priv->dev, "failed to write VLAN %04x\n", vlan4k.vid);
		goto out;
	}

	dev_dbg(priv->dev,
		"resulting VLAN%d 4k members: 0x%02x, untagged: 0x%02x\n",
		vlan4k.vid, vlan4k.mbr, vlan4k.untag);

out:
	mutex_unlock(&priv->map_lock);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_port_vlan_del, REALTEK_DSA);


int rtl8373_port_vlan_add(struct dsa_switch *ds, int port,
			  const struct switchdev_obj_port_vlan *vlan,
			  struct netlink_ext_ack *extack)
{
	bool untagged = !!(vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED);
	bool pvid = !!(vlan->flags & BRIDGE_VLAN_INFO_PVID);
	struct realtek_smi *priv = ds->priv;
	struct rtl8373_vlan_4k vlan4k;
	u16 member;
	u16 untag;
	int pport;
	int ret = 0;

	if (!priv || !vlan)
		return -EINVAL;

	if (!priv->ops->is_vlan_valid(priv, vlan->vid)) {
		NL_SET_ERR_MSG_MOD(extack, "VLAN ID not valid");
		return -EINVAL;
	}

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return -EINVAL;

	mutex_lock(&priv->map_lock);

	/*
	 * CPU port should normally stay tagged.
	 * DSA port handling is allowed here and follows caller request.
	 */
	if (dsa_is_cpu_port(ds, port))
		untagged = false;

	memset(&vlan4k, 0, sizeof(vlan4k));
	vlan4k.vid = vlan->vid;

	dev_dbg(priv->dev, "add VLAN %d on port %d, %s, %s\n",
		vlan4k.vid, port, untagged ? "untagged" : "tagged",
		pvid ? "PVID" : "no PVID");

	ret = _rtl8373_getAsicVlan4kEntry(priv->map, &vlan4k);
	if (ret) {
		dev_err(priv->dev, "failed to read VLAN %04x\n", vlan4k.vid);
		goto out;
	}

	member = vlan4k.mbr | BIT(pport);
	untag = vlan4k.untag;

	if (untagged)
		untag |= BIT(pport);
	else
		untag &= ~BIT(pport);

	vlan4k.mbr = member;
	vlan4k.untag = untag;
	vlan4k.svlan_chk_ivl_svl = 0;
	vlan4k.ivl_svl = 1;
	vlan4k.fid_msti = 0;

	ret = _rtl8373_setAsicVlan4kEntry(priv->map, &vlan4k);
	if (ret) {
		dev_err(priv->dev, "failed to write VLAN %04x\n", vlan4k.vid);
		goto out;
	}

	if (pvid) {
		ret = rtl8373_set_pvid(priv, port, vlan->vid);
		if (ret) {
			dev_err(priv->dev,
				"failed to set PVID on port %d to VLAN %04x\n",
				port, vlan4k.vid);
			goto out;
		}
	}

out:
	mutex_unlock(&priv->map_lock);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_port_vlan_add, REALTEK_DSA);

int rtl8373_port_vlan_filtering(struct dsa_switch *ds, int port,
				bool vlan_filtering,
				struct netlink_ext_ack *extack)
{
	struct realtek_smi *priv = ds->priv;
	struct rtl8373_vlan_4k vlan4k;
	u32 vid;
	int pport;
	int ret = 0;

	if (!priv)
		return -EINVAL;

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return -EINVAL;

	mutex_lock(&priv->map_lock);

	ret = rtl8373_getAsicRegBits(priv->map,
				     RTL8373_VLAN_PORT_PB_VLAN_ADDR(pport),
				     RTL8373_VLAN_PORT_PB_VLAN_PVID_MASK(pport),
				     &vid);
	if (ret) {
		dev_err(priv->dev, "failed to read PVID of port %d\n", port);
		goto out;
	}

	if (!priv->ops->is_vlan_valid(priv, vid)) {
		ret = -EINVAL;
		goto out;
	}

	dev_dbg(priv->dev, "port %d: %s VLAN filtering, pvid %u\n",
		port, vlan_filtering ? "enable" : "disable", vid);

	memset(&vlan4k, 0, sizeof(vlan4k));
	vlan4k.vid = (u16)vid;

	ret = _rtl8373_getAsicVlan4kEntry(priv->map, &vlan4k);
	if (ret) {
		dev_err(priv->dev, "failed to read VLAN %04x\n", vlan4k.vid);
		goto out;
	}

	ret = rtl8373_set_vlan(priv, vid,
			       vlan4k.mbr,
			       vlan4k.untag,
			       vlan4k.fid_msti,
			       vlan4k.svlan_chk_ivl_svl,
			       vlan_filtering ? 1 : 0);
out:
	mutex_unlock(&priv->map_lock);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_port_vlan_filtering, REALTEK_DSA);


int rtl8373_init_vlan(struct realtek_smi *priv)
{
	int lport;
	int ret;

	ret = rtl8373_reset_vlan(priv);
	if (ret)
		return ret;

	for (lport = priv->start_lports; lport <= priv->end_lports; lport++) {
		u32 mask;
		int pport;
		int cpu_pport;

		pport = priv->ops->port_l2p_get(priv, lport);
		if (pport < 0)
			continue;

		cpu_pport = priv->ops->port_l2p_get(priv, priv->cpu_port);
		if (cpu_pport < 0)
			return -EINVAL;

		if (lport == priv->cpu_port)
			mask = GENMASK(priv->end_pports, priv->start_pports);
		else
			mask = BIT(pport) | BIT(cpu_pport);

		ret = rtl8373_set_vlan(priv, lport + 1, mask, mask, 0, 0, 1);
		if (ret)
			return ret;

		ret = rtl8373_set_pvid(priv, lport, lport + 1);
		if (ret)
			return ret;

		ret = rtl8373_vlan_tagMode_set(priv, pport,
					       VLAN_EGRESS_TAG_MODE_ORIGINAL);
		if (ret)
			return ret;

		ret = rtl8373_setAsicRegBit(priv->map,
					    RTL8373_VLAN_PORT_IGR_FLTR_ADDR(pport),
					    RTL8373_VLAN_PORT_IGR_FLTR_IGR_FLTR_ACT_OFFSET(pport),
					    1);
		if (ret)
			return ret;
	}

	ret = rtl8373_setAsicRegBit(priv->map, RTL8373_VLAN_CTRL_ADDR,
				    RTL8373_VLAN_CTRL_CVLAN_FILTER_OFFSET, 1);
	if (ret)
		return ret;

	ret = rtl8373_setAsicReg(priv->map, RTL8373_VLAN_L2_LRN_DIS_ADDR(0), 0);
	if (ret)
		return ret;

	ret = rtl8373_setAsicReg(priv->map, RTL8373_VLAN_L2_LRN_DIS_ADDR(1), 0);
	if (ret)
		return ret;

	return rtl8373_enable_vlan(priv, true);
}
EXPORT_SYMBOL_NS_GPL(rtl8373_init_vlan, REALTEK_DSA);

int rtl8373_port_vlan_prepare(struct dsa_switch *ds, int port,
			      const struct switchdev_obj_port_vlan *vlan)
{
	struct realtek_smi *smi = ds->priv;
	int ret;

	if (!smi->ops->is_vlan_valid(smi, vlan->vid))
		return -EINVAL;

	mutex_lock(&smi->map_lock);
	ret = rtl8373_enable_vlan4k(smi, true);
	mutex_unlock(&smi->map_lock);

	return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_port_vlan_prepare, REALTEK_DSA);

/* Function Name:
 *      dal_rtl8373_svlanInit
 */
rtk_api_ret_t dal_rtl8373_svlanInit(struct realtek_smi *smi)
{
	u32 idx = 0;
	rtk_api_ret_t retVal = 0;

	if ((retVal = dal_rtl8373_svlanPriRef_set(smi, REF_CTAG_PRI)) != RT_ERR_OK)
		return retVal;

	if ((retVal = dal_rtl8373_svlanUntagAction_set(smi, UNTAG_DROP, 0)) != RT_ERR_OK)
		return retVal;

	if ((retVal = dal_rtl8373_svlanTpid_set(smi, 0x88a8)) != RT_ERR_OK)
		return retVal;

	if ((retVal = rtl8373_setAsicReg(smi->map, RTL8373_VS_UPLINK_PORT_ADDR, 0)) != RT_ERR_OK)
		return retVal;

	for (idx = 0; idx <= RTL8373_C2SIDXMAX; idx++) {
		if ((retVal = rtl8373_setAsicReg(smi->map,
						 RTL8373_VLAN_C2S_ENTRY_ADDR(idx) + 4, 0)) != RT_ERR_OK)
			return retVal;

		if ((retVal = rtl8373_setAsicReg(smi->map,
						 RTL8373_VLAN_C2S_ENTRY_ADDR(idx), 0)) != RT_ERR_OK)
			return retVal;
	}

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanServicePort_add(struct realtek_smi *smi, rtk_port_t port)
{
	rtk_api_ret_t retVal = 0;
	u32 pmsk = 0;
	int pport;

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	if ((retVal = rtl8373_getAsicReg(smi->map, RTL8373_VS_UPLINK_PORT_ADDR, &pmsk)) != RT_ERR_OK)
		return retVal;

	pmsk |= BIT(pport);

	if ((retVal = rtl8373_setAsicReg(smi->map, RTL8373_VS_UPLINK_PORT_ADDR, pmsk)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanServicePort_get(struct realtek_smi *smi,
					       rtk_portmask_t *pSvlanPmsk)
{
	rtk_api_ret_t retVal = 0;
	u32 phyMbrPmask = 0;

	if (NULL == pSvlanPmsk)
		return RT_ERR_NULL_POINTER;

	if ((retVal = rtl8373_getAsicReg(smi->map, RTL8373_VS_UPLINK_PORT_ADDR, &phyMbrPmask)) != RT_ERR_OK)
		return retVal;

	if (rtk_switch_portmask_P2L_get(smi, phyMbrPmask, pSvlanPmsk) != RT_ERR_OK)
		return RT_ERR_FAILED;

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanServicePort_del(struct realtek_smi *smi, rtk_port_t port)
{
	rtk_api_ret_t retVal = 0;
	u32 pmsk = 0;
	int pport;

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	if ((retVal = rtl8373_getAsicReg(smi->map, RTL8373_VS_UPLINK_PORT_ADDR, &pmsk)) != RT_ERR_OK)
		return retVal;

	pmsk &= ~BIT(pport);

	if ((retVal = rtl8373_setAsicReg(smi->map, RTL8373_VS_UPLINK_PORT_ADDR, pmsk)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanTpid_set(struct realtek_smi *smi, u32 svlanTpid)
{
	rtk_api_ret_t retVal = 0;

	if (svlanTpid > 0xffff)
		return RT_ERR_INPUT;

	if ((retVal = rtl8373_setAsicReg(smi->map, RTL8373_VS_GLB_CTRL_ADDR, svlanTpid)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanTpid_get(struct realtek_smi *smi, u32 *pSvlanTpid)
{
	rtk_api_ret_t retVal = 0;
	u32 regVal = 0;

	if (NULL == pSvlanTpid)
		return RT_ERR_NULL_POINTER;

	if ((retVal = rtl8373_getAsicReg(smi->map, RTL8373_VS_GLB_CTRL_ADDR, &regVal)) != RT_ERR_OK)
		return retVal;

	*pSvlanTpid = (regVal & 0xffff);

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanPriRef_set(struct realtek_smi *smi,
					  rtk_svlan_pri_ref_t ref)
{
	rtk_api_ret_t retVal = 0;

	if (ref >= REF_PRI_END)
		return RT_ERR_INPUT;

	if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_VS_CTRL_ADDR,
					     RTL8373_VS_CTRL_SPRISEL_MASK, ref)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanPriRef_get(struct realtek_smi *smi,
					  rtk_svlan_pri_ref_t *pRef)
{
	rtk_api_ret_t retVal = 0;

	if (NULL == pRef)
		return RT_ERR_NULL_POINTER;

	if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_VS_CTRL_ADDR,
					     RTL8373_VS_CTRL_SPRISEL_MASK, pRef)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}
rtk_api_ret_t dal_rtl8373_svlanMbrPortEntry_set(struct realtek_smi *smi,
						rtk_vlan_t svid,
						rtk_svlan_memberCfg_t *pSvlan_cfg)
{
	rtk_api_ret_t retVal = 0;
	u32 phyMbrPmask, phyUntagPmask;
	struct rtl8373_vlan_4k vlan4kEntry;

	if (NULL == pSvlan_cfg)
		return RT_ERR_NULL_POINTER;

	if (svid > RTL8373_VIDMAX)
		return RT_ERR_SVLAN_VID;

	if (pSvlan_cfg->fid > RTL8373_FIDMAX)
		return RT_ERR_L2_FID;

	if (pSvlan_cfg->chk_ivl_svl > ENABLED)
		return RT_ERR_INPUT;

	if (pSvlan_cfg->ivl_svl > ENABLED)
		return RT_ERR_INPUT;

	if (pSvlan_cfg->fiden != 0)
		return RT_ERR_CHIP_NOT_SUPPORTED;

	if (pSvlan_cfg->priority != 0)
		return RT_ERR_CHIP_NOT_SUPPORTED;

	if (pSvlan_cfg->efiden != 0)
		return RT_ERR_CHIP_NOT_SUPPORTED;

	if (pSvlan_cfg->efid != 0)
		return RT_ERR_CHIP_NOT_SUPPORTED;

	if (rtk_switch_portmask_L2P_get(smi, &(pSvlan_cfg->memberport), &phyMbrPmask) != RT_ERR_OK)
		return RT_ERR_FAILED;

	if (rtk_switch_portmask_L2P_get(smi, &(pSvlan_cfg->untagport), &phyUntagPmask) != RT_ERR_OK)
		return RT_ERR_FAILED;

	memset(&vlan4kEntry, 0, sizeof(vlan4kEntry));
	vlan4kEntry.vid = svid;
	vlan4kEntry.mbr = phyMbrPmask;
	vlan4kEntry.untag = phyUntagPmask;
	vlan4kEntry.svlan_chk_ivl_svl = pSvlan_cfg->chk_ivl_svl;
	vlan4kEntry.ivl_svl = pSvlan_cfg->ivl_svl;
	vlan4kEntry.fid_msti = pSvlan_cfg->fid;

	if ((retVal = _rtl8373_setAsicVlan4kEntry(smi->map, &vlan4kEntry)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}
rtk_api_ret_t dal_rtl8373_svlanMbrPortEntry_get(struct realtek_smi *smi,
						rtk_vlan_t svid,
						rtk_svlan_memberCfg_t *pSvlan_cfg)
{
	rtk_api_ret_t retVal = 0;
	struct rtl8373_vlan_4k vlan4kEntry;

	if (NULL == pSvlan_cfg)
		return RT_ERR_NULL_POINTER;

	if (svid > RTL8373_VIDMAX)
		return RT_ERR_SVLAN_VID;

	memset(&vlan4kEntry, 0, sizeof(vlan4kEntry));
	vlan4kEntry.vid = svid;

	if ((retVal = _rtl8373_getAsicVlan4kEntry(smi->map, &vlan4kEntry)) != RT_ERR_OK)
		return retVal;

	memset(pSvlan_cfg, 0, sizeof(rtk_svlan_memberCfg_t));
	pSvlan_cfg->svid = vlan4kEntry.vid;

	if (rtk_switch_portmask_P2L_get(smi, vlan4kEntry.mbr,
					&(pSvlan_cfg->memberport)) != RT_ERR_OK)
		return RT_ERR_FAILED;

	if (rtk_switch_portmask_P2L_get(smi, vlan4kEntry.untag,
					&(pSvlan_cfg->untagport)) != RT_ERR_OK)
		return RT_ERR_FAILED;

	pSvlan_cfg->chk_ivl_svl = vlan4kEntry.svlan_chk_ivl_svl;
	pSvlan_cfg->ivl_svl = vlan4kEntry.ivl_svl;
	pSvlan_cfg->fid = vlan4kEntry.fid_msti;

	return RT_ERR_OK;
}


rtk_api_ret_t dal_rtl8373_svlanDfltSvlan_set(struct realtek_smi *smi,
					     rtk_port_t port,
					     rtk_vlan_t svid)
{
	rtk_api_ret_t retVal = 0;
	int pport;

	if (svid > RTL8373_VIDMAX)
		return RT_ERR_SVLAN_VID;

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	if ((retVal = rtl8373_setAsicRegBits(smi->map,
					     RTL8373_VS_PORT_DFLT_SVID_ADDR(pport),
					     RTL8373_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(pport),
					     svid)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanDfltSvlan_get(struct realtek_smi *smi,
					     rtk_port_t port,
					     rtk_vlan_t *pSvid)
{
	rtk_api_ret_t retVal = 0;
	int pport;

	if (NULL == pSvid)
		return RT_ERR_NULL_POINTER;

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	if ((retVal = rtl8373_getAsicRegBits(smi->map,
					     RTL8373_VS_PORT_DFLT_SVID_ADDR(pport),
					     RTL8373_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(pport),
					     pSvid)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}
rtk_api_ret_t dal_rtl8373_svlanC2S_add(struct realtek_smi *smi,
				       rtk_vlan_t vid,
				       rtk_port_t srcPort,
				       rtk_vlan_t svid)
{
	rtk_api_ret_t retVal = 0;
	int i;
	u32 empty_idx = 0xFFFF;
	int phyPort;
	u16 doneFlag = FALSE;
	u32 idx_svid = 0, idx_pmsk = 0, idx_cvid = 0;

	if (vid > RTL8373_VIDMAX)
		return RT_ERR_VLAN_VID;

	if (svid > RTL8373_VIDMAX)
		return RT_ERR_SVLAN_VID;

	phyPort = smi->ops->port_l2p_get(smi, srcPort);
	if (phyPort < 0)
		return RT_ERR_PORT_ID;

	for (i = RTL8373_C2SIDXMAX; i >= 0; i--) {
		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i),
						RTL8373_VLAN_C2S_ENTRY_SVID_ASSIGN_MASK,
						&idx_svid);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						RTK_MAX_PORT_MASK,
						&idx_pmsk);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						(0xFFF << RTK_MAX_NUM_OF_PORT),
						&idx_cvid);
		if (retVal != RT_ERR_OK)
			return retVal;

		if (idx_cvid == vid) {
			if (idx_pmsk & BIT(phyPort)) {
				if (idx_svid != svid) {
					idx_pmsk &= ~BIT(phyPort);
					if (idx_pmsk == 0)
						idx_svid = 0;

					retVal = rtl8373_setAsicRegBits(smi->map,
									RTL8373_VLAN_C2S_ENTRY_ADDR(i),
									RTL8373_VLAN_C2S_ENTRY_SVID_ASSIGN_MASK,
									idx_svid);
					if (retVal != RT_ERR_OK)
						return retVal;

					retVal = rtl8373_setAsicRegBits(smi->map,
									RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
									RTK_MAX_PORT_MASK,
									idx_pmsk);
					if (retVal != RT_ERR_OK)
						return retVal;
				} else {
					doneFlag = TRUE;
				}
			} else {
				if (idx_svid == svid && doneFlag == FALSE) {
					idx_pmsk |= BIT(phyPort);
					retVal = rtl8373_setAsicRegBits(smi->map,
									RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
									RTK_MAX_PORT_MASK,
									idx_pmsk);
					if (retVal != RT_ERR_OK)
						return retVal;

					doneFlag = TRUE;
				}
			}
		} else if (idx_svid == 0 && idx_pmsk == 0) {
			empty_idx = i;
		}
	}

	if (doneFlag == TRUE)
		return RT_ERR_OK;

	if (empty_idx != 0xFFFF) {
		retVal = rtl8373_setAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(empty_idx),
						RTL8373_VLAN_C2S_ENTRY_SVID_ASSIGN_MASK,
						svid);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_setAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(empty_idx) + 4,
						RTK_MAX_PORT_MASK,
						BIT(phyPort));
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_setAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(empty_idx) + 4,
						(0xFFF << RTK_MAX_NUM_OF_PORT),
						vid);
		if (retVal != RT_ERR_OK)
			return retVal;

		return RT_ERR_OK;
	}

	return RT_ERR_OUT_OF_RANGE;
}
rtk_api_ret_t dal_rtl8373_svlanC2S_del(struct realtek_smi *smi,
				       rtk_vlan_t vid,
				       rtk_port_t srcPort)
{
	rtk_api_ret_t retVal = 0;
	u32 i;
	int phyPort;
	u32 idx_svid, idx_pmsk, idx_cvid;

	if (vid > RTL8373_VIDMAX)
		return RT_ERR_VLAN_VID;

	phyPort = smi->ops->port_l2p_get(smi, srcPort);
	if (phyPort < 0)
		return RT_ERR_PORT_ID;

	for (i = 0; i <= RTL8373_C2SIDXMAX; i++) {
		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i),
						RTL8373_VLAN_C2S_ENTRY_SVID_ASSIGN_MASK,
						&idx_svid);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						RTK_MAX_PORT_MASK,
						&idx_pmsk);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						(0xFFF << RTK_MAX_NUM_OF_PORT),
						&idx_cvid);
		if (retVal != RT_ERR_OK)
			return retVal;

		if (idx_cvid != vid)
			continue;

		if (!(idx_pmsk & BIT(phyPort)))
			continue;

		idx_pmsk &= ~BIT(phyPort);
		if (idx_pmsk == 0) {
			idx_svid = 0;
			idx_cvid = 0;
		}

		retVal = rtl8373_setAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i),
						RTL8373_VLAN_C2S_ENTRY_SVID_ASSIGN_MASK,
						idx_svid);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_setAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						RTK_MAX_PORT_MASK,
						idx_pmsk);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_setAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						(0xFFF << RTK_MAX_NUM_OF_PORT),
						idx_cvid);
		if (retVal != RT_ERR_OK)
			return retVal;

		return RT_ERR_OK;
	}

	return RT_ERR_OUT_OF_RANGE;
}
rtk_api_ret_t dal_rtl8373_svlanC2S_get(struct realtek_smi *smi,
				       rtk_vlan_t vid,
				       rtk_port_t srcPort,
				       rtk_vlan_t *pSvid)
{
	rtk_api_ret_t retVal = 0;
	u32 i;
	u32 idx_svid, idx_pmsk, idx_cvid;
	int phyPort;

	if (NULL == pSvid)
		return RT_ERR_NULL_POINTER;

	if (vid > RTL8373_VIDMAX)
		return RT_ERR_VLAN_VID;

	phyPort = smi->ops->port_l2p_get(smi, srcPort);
	if (phyPort < 0)
		return RT_ERR_PORT_ID;

	for (i = 0; i <= RTL8373_C2SIDXMAX; i++) {
		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i),
						RTL8373_VLAN_C2S_ENTRY_SVID_ASSIGN_MASK,
						&idx_svid);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						RTK_MAX_PORT_MASK,
						&idx_pmsk);
		if (retVal != RT_ERR_OK)
			return retVal;

		retVal = rtl8373_getAsicRegBits(smi->map,
						RTL8373_VLAN_C2S_ENTRY_ADDR(i) + 4,
						(0xFFF << RTK_MAX_NUM_OF_PORT),
						&idx_cvid);
		if (retVal != RT_ERR_OK)
			return retVal;

		if (idx_cvid == vid && (idx_pmsk & BIT(phyPort))) {
			*pSvid = idx_svid;
			return RT_ERR_OK;
		}
	}

	return RT_ERR_OUT_OF_RANGE;
}


rtk_api_ret_t dal_rtl8373_svlanUntagAction_set(struct realtek_smi *smi,
					       rtk_svlan_untag_action_t action,
					       rtk_vlan_t svid)
{
	rtk_api_ret_t retVal = 0;

	if (action >= UNTAG_END)
		return RT_ERR_OUT_OF_RANGE;

	if (action == UNTAG_ASSIGN) {
		if (svid > RTL8373_VIDMAX)
			return RT_ERR_SVLAN_VID;
	}

	if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_VS_CTRL_ADDR,
					     RTL8373_VS_CTRL_UNTAG_MASK, action)) != RT_ERR_OK)
		return retVal;

	if (action == UNTAG_ASSIGN) {
		if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_VS_UNTAG_SVID_ADDR,
						     RTL8373_VS_UNTAG_SVID_UNTAG_SVID_MASK, svid)) != RT_ERR_OK)
			return retVal;
	}

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanUntagAction_get(struct realtek_smi *smi,
					       rtk_svlan_untag_action_t *pAction,
					       rtk_vlan_t *pSvid)
{
	rtk_api_ret_t retVal = 0;

	if (NULL == pAction || NULL == pSvid)
		return RT_ERR_NULL_POINTER;

	if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_VS_CTRL_ADDR,
					     RTL8373_VS_CTRL_UNTAG_MASK, pAction)) != RT_ERR_OK)
		return retVal;

	if (*pAction == UNTAG_ASSIGN) {
		if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_VS_UNTAG_SVID_ADDR,
						     RTL8373_VS_UNTAG_SVID_UNTAG_SVID_MASK, pSvid)) != RT_ERR_OK)
			return retVal;
	}

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_svlanUnassignAction_set(struct realtek_smi *smi,
						  rtk_svlan_unassign_action_t action)
{
	rtk_api_ret_t retVal = 0;

	if (action >= UNASSIGN_END)
		return RT_ERR_OUT_OF_RANGE;

	retVal = rtl8373_setAsicRegBit(smi->map, RTL8373_VS_CTRL_ADDR,
				       RTL8373_VS_CTRL_UIFSEG_OFFSET, action);
	return retVal;
}

rtk_api_ret_t dal_rtl8373_svlanUnassignAction_get(struct realtek_smi *smi,
						  rtk_svlan_unassign_action_t *pAction)
{
	rtk_api_ret_t retVal = 0;

	if (NULL == pAction)
		return RT_ERR_NULL_POINTER;

	retVal = rtl8373_getAsicRegBit(smi->map, RTL8373_VS_CTRL_ADDR,
				       RTL8373_VS_CTRL_UIFSEG_OFFSET, pAction);
	return retVal;
}

rtk_api_ret_t dal_rtl8373_svlanTrapPri_set(struct realtek_smi *smi,
					   rtk_pri_t priority)
{
	rtk_api_ret_t retVal = 0;

	if (priority > RTL8373_PRIMAX)
		return RT_ERR_OUT_OF_RANGE;

	retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_SVLAN_TRAP_CTRL_ADDR,
					RTL8373_SVLAN_TRAP_CTRL_PRI_MASK, priority);
	return retVal;
}

rtk_api_ret_t dal_rtl8373_svlanTrapPri_get(struct realtek_smi *smi,
					   rtk_pri_t *pPriority)
{
	rtk_api_ret_t retVal = 0;

	if (NULL == pPriority)
		return RT_ERR_NULL_POINTER;

	retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_SVLAN_TRAP_CTRL_ADDR,
					RTL8373_SVLAN_TRAP_CTRL_PRI_MASK, pPriority);
	return retVal;
}
rtk_api_ret_t dal_rtl8373_svlanTrapCpumsk_set(struct realtek_smi *smi,
					      u32 mask)
{
	rtk_api_ret_t retVal = 0;
	u32 field_max;

	if (!RTL8373_SVLAN_TRAP_CTRL_CPU_PMSK_MASK)
		return RT_ERR_INPUT;

	field_max = RTL8373_SVLAN_TRAP_CTRL_CPU_PMSK_MASK >>
		    __ffs(RTL8373_SVLAN_TRAP_CTRL_CPU_PMSK_MASK);

	if (mask > field_max)
		return RT_ERR_OUT_OF_RANGE;

	retVal = rtl8373_setAsicRegBits(smi->map,
					RTL8373_SVLAN_TRAP_CTRL_ADDR,
					RTL8373_SVLAN_TRAP_CTRL_CPU_PMSK_MASK,
					mask);
	return retVal;
}



rtk_api_ret_t dal_rtl8373_svlanTrapCpumsk_get(struct realtek_smi *smi,
					      u32 *pMask)
{
	rtk_api_ret_t retVal = 0;

	if (NULL == pMask)
		return RT_ERR_NULL_POINTER;

	retVal = rtl8373_getAsicRegBits(smi->map,
					RTL8373_SVLAN_TRAP_CTRL_ADDR,
					RTL8373_SVLAN_TRAP_CTRL_CPU_PMSK_MASK,
					pMask);
	return retVal;
}


rtk_api_ret_t dal_rtl8373_vlan_portFid_set(struct realtek_smi *smi,
					   rtk_port_t port, rtk_enable_t enable,
					   rtk_fid_t fid)
{
	rtk_api_ret_t retVal = 0;
	int pport;

	if (enable >= RTK_ENABLE_END)
		return RT_ERR_ENABLE;

	if (fid > RTL8373_FIDMAX)
		return RT_ERR_L2_FID;

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	if ((retVal = rtl8373_setAsicRegBit(smi->map, RTL8373_PORT_BASED_FID_EN_ADDR,
					    pport, enable)) != RT_ERR_OK)
		return retVal;

	if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_PORT_BASED_FID_ADDR(pport),
					     RTL8373_PORT_BASED_FID_FID_MASK(pport), fid)) != RT_ERR_OK)
		return retVal;

	return RT_ERR_OK;
}

rtk_api_ret_t dal_rtl8373_vlan_portFid_get(struct realtek_smi *smi,
					   rtk_port_t port, rtk_enable_t *pEnable,
					   rtk_fid_t *pFid)
{
	rtk_api_ret_t retVal = 0;
	u32 enable = 0;
	u32 fid = 0;
	int pport;

	if (NULL == pEnable || NULL == pFid)
		return RT_ERR_NULL_POINTER;

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	if ((retVal = rtl8373_getAsicRegBit(smi->map, RTL8373_PORT_BASED_FID_EN_ADDR,
					    pport, &enable)) != RT_ERR_OK)
		return retVal;

	if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_PORT_BASED_FID_ADDR(pport),
					     RTL8373_PORT_BASED_FID_FID_MASK(pport), &fid)) != RT_ERR_OK)
		return retVal;

	*pEnable = (rtk_enable_t)enable;
	*pFid = (rtk_fid_t)fid;

	return RT_ERR_OK;
}

MODULE_IMPORT_NS(REALTEK_DSA);
