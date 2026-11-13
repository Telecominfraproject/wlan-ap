// SPDX-License-Identifier: GPL-2.0
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include "realtek-smi-rtl8373.h"
#include <net/dsa.h>
#include <linux/version.h>
#include "rtl8373_core.h"
#include "rtl8373_lut.h"
#include "rtl8373_interrupt.h"
#include "rtl8373_vlan.h"
#include "rtl8373_mib.h"
#include "rtl8373_switch.h"
#include "rtl8373_eee.h"
#include "rtl8373_port.h"


 /*
  * DSA bridge API compatibility helpers
  */
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
 #define RTL8373_BR_JOIN_ARG              struct dsa_bridge bridge
 #define RTL8373_BR_LEAVE_ARG             struct dsa_bridge bridge
 #else
 #define RTL8373_BR_JOIN_ARG              struct net_device *bridge
 #define RTL8373_BR_LEAVE_ARG             struct net_device *bridge
 #endif
 
 /*
  * Some trees use struct switchdev_brport_flags, some use unsigned long.
  */
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)
 #define RTL8373_BR_FLAGS_TYPE            struct switchdev_brport_flags
 #define RTL8373_BR_FLAGS_MASK(_f)        ((_f).mask)
 #define RTL8373_BR_FLAGS_VAL(_f)         ((_f).val)
 #else
 #define RTL8373_BR_FLAGS_TYPE            unsigned long
 #define RTL8373_BR_FLAGS_MASK(_f)        (_f)
 #define RTL8373_BR_FLAGS_VAL(_f)         (_f)
 #endif

 
 static void *rtl8373_bridge_dev_get(struct dsa_port *dp)
 {
 	if (!dp)
 		return NULL;
 
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
 	return dp->bridge;
 #else
 	return dp->bridge_dev;
 #endif
 }
 
 static bool rtl8373_dp_in_bridge(struct dsa_port *dp)
 {
 	return rtl8373_bridge_dev_get(dp) != NULL;
 }
 

static bool rtl8373_same_bridge(struct dsa_switch *ds, int port1, int port2)
{
	struct dsa_port *dp1 = dsa_to_port(ds, port1);
	struct dsa_port *dp2 = dsa_to_port(ds, port2);

	if (!dp1 || !dp2)
		return false;

	return rtl8373_bridge_dev_get(dp1) == rtl8373_bridge_dev_get(dp2);
}
//modify mapping by user
int rtl8373_port_L2P_get(struct realtek_smi *priv, int port)
{
	if ((port >priv->end_lports)||(port <priv->start_lports))
		return -1;

	if(priv->svlan_disable == true)
	{
	#if defined(RTL8372)||defined(RTL8366U)
		if (port == 4)
			return 3;
		else if (port == 5)
			return 8;
		else
			return port+4;
	#else
			return port ;
	#endif
	}
	else
	{
		return port ;
	}
}
//modify mapping by user
int rtl8373_port_P2L_get(struct realtek_smi *priv, int port)
{
	if ((port >priv->end_pports)||(port <priv->start_pports))
		return -1;  

	if(priv->svlan_disable == true)
	{
	#if defined(RTL8372)||defined(RTL8366U)
		if (port == 3)
			return 4;
		if (port == 8)
			return 5;
		else
			return port-4;
	#else
		return port;    
	#endif
	}
	else
	{
		return port ;
	}
}
/* Function Name:
 *      rtk_switch_portmask_P2L_get
 * Description:
 *      Get logical portmask from physical portmask
 * Input:
 *      physicalPortmask    - physical port mask
 * Output:
 *      pLogicalPmask       - logical port mask
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_NOT_INIT     - Not Initialize
 *      RT_ERR_NULL_POINTER - Null pointer
 *      RT_ERR_PORT_MASK    - Error port mask
 * Note:
 *
 */
rtk_api_ret_t rtk_switch_portmask_P2L_get(struct realtek_smi *priv,
					  u32 physicalPortmask,
					  rtk_portmask_t *pLogicalPmask)
{
	int log_port;
	u32 phy_port;

	if (NULL == pLogicalPmask)
		return RT_ERR_NULL_POINTER;

	RTK_PORTMASK_CLEAR(*pLogicalPmask);

	for (phy_port = priv->start_pports; phy_port <= priv->end_pports; phy_port++) {
		if (physicalPortmask & BIT(phy_port)) {
			log_port = priv->ops->port_p2l_get(priv, phy_port);
			if (log_port >= 0)
				RTK_PORTMASK_PORT_SET(*pLogicalPmask, log_port);
		}
	}

	return RT_ERR_OK;
}


/* Function Name:
 *      rtk_switch_portmask_L2P_get
 * Description:
 *      Get physicl portmask from logical portmask
 * Input:
 *      pLogicalPmask       - logical port mask
 * Output:
 *      pPhysicalPortmask   - physical port mask
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_NOT_INIT     - Not Initialize
 *      RT_ERR_NULL_POINTER - Null pointer
 *      RT_ERR_PORT_MASK    - Error port mask
 * Note:
 *
 */
rtk_api_ret_t rtk_switch_portmask_L2P_get(struct realtek_smi *priv,
					  rtk_portmask_t *pLogicalPmask,
					  u32 *pPhysicalPortmask)
{
	int log_port, phy_port;

	if (!pLogicalPmask || !pPhysicalPortmask)
		return RT_ERR_NULL_POINTER;

	*pPhysicalPortmask = 0;

	RTK_PORTMASK_SCAN((*pLogicalPmask), log_port) {
		phy_port = priv->ops->port_l2p_get(priv, (rtk_port_t)log_port);
		if (phy_port < 0)
			continue;
		*pPhysicalPortmask |= BIT(phy_port);
	}

	return RT_ERR_OK;
}


bool rtl8373_is_vlan_valid(struct realtek_smi *priv, unsigned int vlan)
{
	unsigned int max = RTL8373_NUM_VLANS - 1;

	if (priv->vlan4k_enabled)
		max = RTL8373_NUM_VIDS - 1;

	if (vlan > max)
		return false;

	return true;
}

static int __rtl8373_enable_vlan(struct realtek_smi *smi, bool enable)
{
#if 0
    //move to init routine
    if((retVal = rtl8373_setAsicRegBit(smi->map, RTL8373_VLAN_CTRL_ADDR, RTL8373_VLAN_CTRL_TABLE_RST_OFFSET, RTK_ENABLED)) != RT_ERR_OK)
        return retVal;
#endif
    return RT_ERR_OK;
}

static int __rtl8373_enable_vlan4k(struct realtek_smi *smi, bool enable)
{
    return RT_ERR_OK;
}

// Define the PHY interface modes
enum rtl8373_phy_interface_mode {
    rtl8373_PHY_INTERFACE_MODE_INVAL = 0,
    rtl8373_PHY_INTERFACE_MODE_INTERNAL = BIT(0),
    rtl8373_PHY_INTERFACE_MODE_MII = BIT(1),
    rtl8373_PHY_INTERFACE_MODE_TMII = BIT(2),
    rtl8373_PHY_INTERFACE_MODE_RMII = BIT(3),
    rtl8373_PHY_INTERFACE_MODE_RGMII = BIT(4),
    rtl8373_PHY_INTERFACE_MODE_SGMII = BIT(5),
    rtl8373_PHY_INTERFACE_MODE_HSGMII = BIT(6),
    rtl8373_PHY_INTERFACE_MODE_10GQXG = BIT(7),
    rtl8373_PHY_INTERFACE_MODE_10GUSXG = BIT(8),
    rtl8373_PHY_INTERFACE_MODE_10GR = BIT(9),
    rtl8373_PHY_INTERFACE_MODE_100FX = BIT(10),
    rtl8373_PHY_INTERFACE_MODE_1000BASEX = BIT(11),
    rtl8373_PHY_INTERFACE_MODE_2500BASEX = BIT(12),
};

#define PHY_INTF(_mode) (rtl8373_PHY_INTERFACE_MODE_##_mode)

#define RTL8373_STATS_INTERVAL_JIFFIES	(3 * HZ)

static int realtek_phy_read(struct realtek_smi *smi, int phy, int regnum);
static int realtek_phy_write(struct realtek_smi *smi, int phy, int regnum, u16 val);


static int rtl8373_port_set_isolation(struct realtek_smi *priv, int pport, u32 mask)
{
	return rtl8373_setAsicReg(priv->map,
				  RTL8373_PORT_ISO_PORT_PMSK_ADDR(pport),
				  mask);
}

static int rtl8373_port_get_isolation(struct realtek_smi *priv, int pport, u32 *mask)
{
	if (!mask)
		return RT_ERR_NULL_POINTER;

	return rtl8373_getAsicReg(priv->map,
				  RTL8373_PORT_ISO_PORT_PMSK_ADDR(pport),
				  mask);
}

static int rtl8373_set_port_isolation(struct realtek_smi *priv, int port,
				      rtk_portmask_t *pLmask)
{
	rtk_api_ret_t ret;
	int pport;
	u32 pmask;

	if (!priv || !pLmask)
		return RT_ERR_NULL_POINTER;

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	ret = rtk_switch_portmask_L2P_get(priv, pLmask, &pmask);
	if (ret != RT_ERR_OK)
		return ret;

	return rtl8373_port_set_isolation(priv, pport, pmask);
}

static int rtl8373_get_port_isolation(struct realtek_smi *priv, int port,
				      rtk_portmask_t *pLmask)
{
	int ret;
	int pport;
	u32 pmask;

	if (!priv || !pLmask)
		return RT_ERR_NULL_POINTER;

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	ret = rtl8373_port_get_isolation(priv, pport, &pmask);
	if (ret != RT_ERR_OK)
		return ret;

	ret = rtk_switch_portmask_P2L_get(priv, pmask, pLmask);
	if (ret != RT_ERR_OK)
		return ret;

	return RT_ERR_OK;
}


static void rtl8373_port_stp_state_set(struct dsa_switch *ds, int port, u8 state)
{
	struct realtek_smi *priv = ds->priv;
	enum rtl8373_stp_state val;
	int msti = 0;
	int ret;
	int pport;

	if (!dsa_is_user_port(ds, port))
		return;

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return;

	switch (state) {
	case BR_STATE_DISABLED:
		val = RTL8373_STP_STATE_DISABLED;
		break;
	case BR_STATE_BLOCKING:
	case BR_STATE_LISTENING:
		val = RTL8373_STP_STATE_BLOCKING;
		break;
	case BR_STATE_LEARNING:
		val = RTL8373_STP_STATE_LEARNING;
		break;
	case BR_STATE_FORWARDING:
		val = RTL8373_STP_STATE_FORWARDING;
		break;
	default:
		dev_err(priv->dev, "invalid STP state: %u\n", state);
		return;
	}

	mutex_lock(&priv->map_lock);
	ret = rtl8373_setAsicRegBits(priv->map,
				     RTL8373_MSPT_STATE_ADDR(msti),
				     0x3 << (pport * 2),
				     val);
	mutex_unlock(&priv->map_lock);

	if (ret)
		dev_err(priv->dev, "failed to set STP state on port %d (pport=%d), ret=%d\n",
			port, pport, ret);
}


static int rtl8373_cpu_config(struct realtek_smi *priv)
{
	struct rtl8373 *mb = priv->chip_data;
	struct rtl8373_cpu *cpu = &mb->cpu;
	int ret;
	ret = regmap_update_bits(priv->map, RTL8373_CPU_TAG_AWARE_CTRL_ADDR,
				 RTL8373_CPU_TAG_AWARE_CTRL_PMSK_MASK,
				 FIELD_PREP(RTL8373_CPU_TAG_AWARE_CTRL_PMSK_MASK,
					    cpu->enable ? cpu->mask : 0));
	if (ret)
		return ret;

	ret = regmap_update_bits(priv->map, RTL8373_EXT_CPU_CTRL_ADDR,
				 RTL8373_EXT_CPU_CTRL_PORT_MASK,
				 FIELD_PREP(RTL8373_EXT_CPU_CTRL_PORT_MASK,
					    cpu->enable ? cpu->trap_port : 0xF));
	if (ret)
		return ret;
	ret = regmap_update_bits(priv->map, RTL8373_CPU_TAG_CTRL_ADDR,
				 RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_INSERTMOD_MASK,
				 FIELD_PREP(RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_INSERTMOD_MASK,
					    cpu->insert));
	if (ret)
		return ret;

	ret = regmap_update_bits(priv->map, RTL8373_CPU_TAG_CTRL_ADDR,
				 RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_EN_MASK,
				 FIELD_PREP(RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_EN_MASK,
					    1));
	if (ret)
		return ret;


	return 0;
}
static int rtl8373_change_mtu(struct dsa_switch *ds, int port, int new_mtu)
{
	struct realtek_smi *smi = ds->priv;
	struct rtl8373 *rb;
	int ret, pport;
	u32 max_mtu = (u32)new_mtu & 0x3fff;

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return -EINVAL;

	rb = smi->chip_data;
	rb->max_mtu[port] = max_mtu;

	/* mtu config offloaded in PPE VP */
	if (smi->proto == DSA_TAG_PROTO_RTL8_4 || smi->proto == DSA_TAG_PROTO_QCA_8021Q)
		return 0;

	mutex_lock(&smi->map_lock);

	ret = rtl8373_setAsicRegBits(smi->map,
			RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_ADDR(pport),
			RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_MAX_LEN_TX_1G_2P5G_5G_10G_SEL_MASK,
			max_mtu);
	if (ret) {
		dev_err(smi->dev, "failed to rtl8373_change_mtu_2.5G: %d\n", ret);
		goto out;
	}

	ret = rtl8373_setAsicRegBits(smi->map,
			RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_ADDR(pport),
			RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_MAX_LEN_TX_100M_10M_SEL_MASK,
			max_mtu);
	if (ret)
		dev_err(smi->dev, "failed to rtl8373_change_mtu_100m: %d\n", ret);

out:
	mutex_unlock(&smi->map_lock);
	return ret;
}



static int rtl8373_setup(struct dsa_switch *ds)
{
	struct realtek_smi *smi = ds->priv;
	struct rtl8373_cpu *cpu;
	struct dsa_port *cpu_dp = NULL;
	struct rtl8373 *mb;
	int ret = 0;
	int i;
	int cpu_pport;

	for (i = 0; i < ds->num_ports; ++i) {
		struct dsa_port *dp = dsa_to_port(ds, i);

		if (!dp)
			continue;

		if (dp->type == DSA_PORT_TYPE_CPU)
			cpu_dp = dp;
	}

	if (!cpu_dp)
		return -ENODEV;

	cpu_pport = smi->ops->port_l2p_get(smi, cpu_dp->index);
	if (cpu_pport < 0)
		return -EINVAL;

	mb = smi->chip_data;
	cpu = &mb->cpu;

	mutex_lock(&smi->map_lock);

	cpu->mask = BIT(cpu_pport);
	cpu->trap_port = cpu_pport;
	cpu->enable = true;

	if (smi->svlan_disable)
		cpu->insert = rtl8373_CPU_INSERT_TO_ALL;
	else
		cpu->insert = rtl8373_CPU_INSERT_TO_NONE;

	ret = rtl8373_cpu_config(smi);

	mutex_unlock(&smi->map_lock);
	return ret;
}


static int rtl8373_recompute_isolation(struct dsa_switch *ds)
{
	struct realtek_smi *smi = ds->priv;
	rtk_portmask_t logicMask;
	int cpu_port = -1;
	int port, other;
	int ret = 0;

	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_cpu_port(ds, port)) {
			cpu_port = port;
			break;
		}
	}


	if (cpu_port < 0) {
		dev_err(smi->dev, "no CPU port found for isolation recompute\n");
		return -ENODEV;
	}

	mutex_lock(&smi->map_lock);

	/* Configure all user ports */
	for (port = 0; port < ds->num_ports; port++) {
		struct dsa_port *dp = dsa_to_port(ds, port);

		if (!dp || !dsa_is_user_port(ds, port))
			continue;

		RTK_PORTMASK_CLEAR(logicMask);

		/* Allow self  CPU by default */
		RTK_PORTMASK_PORT_SET(logicMask, port);
		RTK_PORTMASK_PORT_SET(logicMask, cpu_port);

		/* If in a bridge, allow ports in the same bridge */
		if (rtl8373_dp_in_bridge(dp)) {
			for (other = 0; other < ds->num_ports; other++) {
				if (!dsa_is_user_port(ds, other))
					continue;
				if (rtl8373_same_bridge(ds, port, other))
					RTK_PORTMASK_PORT_SET(logicMask, other);
			}
		}

		ret = smi->ops->port_isolation_set(smi, port, &logicMask);
		if (ret != RT_ERR_OK) {
			dev_err(smi->dev,
				"failed to set isolation for user port %d, ret=%d\n",
				port, ret);
			goto out;
		}
	}

	/* Configure CPU port: allow all user ports  self */
	RTK_PORTMASK_CLEAR(logicMask);
	RTK_PORTMASK_PORT_SET(logicMask, cpu_port);

	for (port = 0; port < ds->num_ports; port++) {
		if (dsa_is_user_port(ds, port))
			RTK_PORTMASK_PORT_SET(logicMask, port);
	}

	ret = smi->ops->port_isolation_set(smi, cpu_port, &logicMask);
	if (ret != RT_ERR_OK) {
		dev_err(smi->dev,
			"failed to set isolation for CPU port %d, ret=%d\n",
			cpu_port, ret);
		goto out;
	}

out:
	mutex_unlock(&smi->map_lock);
	return (ret == RT_ERR_OK) ? 0 : -EIO;
}

static int rtl8373_setup_rest(struct dsa_switch *ds)
{
	struct realtek_smi *smi = ds->priv;
	int ret = 0;
	int port, ext_port;
	int utp_port;
	u32 xmit_vid;

	mutex_lock(&smi->map_lock);

	if (!smi->svlan_disable) {
		if (smi->ops->svlan_init) {
			ret = smi->ops->svlan_init(smi);
			if (ret != RT_ERR_OK) {
				dev_err(smi->dev, "dal_rtl8373_svlanInit FAIL ! (ret=%d)\n", ret);
				goto out_unlock;
			}
		}

		if (smi->ops->svlanTpid_set) {
			ret = smi->ops->svlanTpid_set(smi, ETH_P_8021Q);
			if (ret != RT_ERR_OK) {
				dev_err(smi->dev, "dal_rtl8373_svlanTpid_set FAIL ! (ret=%d)\n", ret);
				goto out_unlock;
			}
		}

		for (port = 0; port < REALTEK_SMI_DSA_PORTS; port++) {
			struct dsa_port *dp = dsa_to_port(smi->ds, port);
			rtk_svlan_memberCfg_t svlanCfg;

			//if (!dp || !dp->slave || !dp->cpu_dp)
			if (!dp || (!dp->slave && !dp->cpu_dp) )
				continue;

			if (dsa_is_cpu_port(smi->ds, port))
			{
				if (smi->ops->l2_limit_learning_cnt_set) {
					ret = smi->ops->l2_limit_learning_cnt_set(smi, port, 0);
					if (ret != RT_ERR_OK) {
						dev_err(smi->dev, "l2_limit_learning_cnt_set FAIL ! (ret=%d)\n", ret);
						goto out_unlock;
					}
				}
				continue;
			}

			xmit_vid = dp->index;
			/* Set VID[11:10]= 0b11 to support VLAN base DSA (ETH_P_DSA_8021Q) */
			xmit_vid |= 0xc00;
			utp_port = dp->index;
			ext_port = dp->cpu_dp->index;

			if (smi->ops->svlan_service_port_add) {
				ret = smi->ops->svlan_service_port_add(smi, ext_port);
				if (ret != RT_ERR_OK) {
					dev_err(smi->dev, "svlan_service_port_add FAIL ! (ret=%d)\n", ret);
					goto out_unlock;
				}
			}

			if (smi->ops->svlan_member_port_entry_set) {
				memset(&svlanCfg, 0x00, sizeof(rtk_svlan_memberCfg_t));
				RTK_PORTMASK_PORT_SET(svlanCfg.memberport, ext_port);
				RTK_PORTMASK_PORT_SET(svlanCfg.memberport, utp_port);
				RTK_PORTMASK_PORT_SET(svlanCfg.untagport, utp_port);

				ret = smi->ops->svlan_member_port_entry_set(smi, xmit_vid, &svlanCfg);
				if (ret != RT_ERR_OK) {
					dev_err(smi->dev, "svlan_member_port_entry_set FAIL ! (ret=%d)\n", ret);
					goto out_unlock;
				}
			}

			if (smi->ops->svlan_default_svlan_set) {
				ret = smi->ops->svlan_default_svlan_set(smi, utp_port, xmit_vid);
				if (ret != RT_ERR_OK) {
					dev_err(smi->dev, "svlan_default_svlan_set FAIL ! (ret=%d)\n", ret);
					goto out_unlock;
				}
			}

			if (smi->ops->l2_limit_learning_cnt_set) {
				ret = smi->ops->l2_limit_learning_cnt_set(smi, utp_port, 0);
				if (ret != RT_ERR_OK) {
					dev_err(smi->dev, "l2_limit_learning_cnt_set FAIL ! (ret=%d)\n", ret);
					goto out_unlock;
				}
			}
		}
	}
	else
	{
		// Disable learning 
		int port;
		
		for (port = 0; port < ds->num_ports; port++) {
			struct dsa_port *dp = dsa_to_port(ds, port);

			if ((!dp || !dsa_is_user_port(ds, port)) && !dsa_is_cpu_port(ds, port))
				continue;

			if (smi->ops->l2_limit_learning_cnt_set)			
				smi->ops->l2_limit_learning_cnt_set(smi, port, 0);
		}
	}

	mutex_unlock(&smi->map_lock);

	return rtl8373_recompute_isolation(ds);

out_unlock:
	mutex_unlock(&smi->map_lock);
	return ret;
}

 static int rtl8373_port_bridge_join(struct dsa_switch *ds, int port,
				    RTL8373_BR_JOIN_ARG,
 				    bool *tx_fwd_offload,
 				    struct netlink_ext_ack *extack)

 {
	struct realtek_smi *smi;
	smi = ds->priv;
		
	if (tx_fwd_offload)
		*tx_fwd_offload = false;

	/* switch forward is not allowed for traffic accelerated in PPE/SFE */
	if (smi->proto == DSA_TAG_PROTO_RTL8_4 || smi->proto == DSA_TAG_PROTO_QCA_8021Q)
		return 0;

	return rtl8373_recompute_isolation(ds);
}

static void rtl8373_port_bridge_leave(struct dsa_switch *ds, int port,
				RTL8373_BR_LEAVE_ARG)
{
	struct realtek_smi *smi;
	smi = ds->priv;
	
	if (smi->proto == DSA_TAG_PROTO_RTL8_4 || smi->proto == DSA_TAG_PROTO_QCA_8021Q)
		return;

	rtl8373_recompute_isolation(ds);
}

static int rtl8373_port_pre_bridge_flags(struct dsa_switch *ds, int port,
					 RTL8373_BR_FLAGS_TYPE flags,
					 struct netlink_ext_ack *extack)
{
 	if (RTL8373_BR_FLAGS_MASK(flags) & ~(BR_LEARNING | BR_FLOOD |
 					     BR_MCAST_FLOOD | BR_BCAST_FLOOD))
		return -EINVAL;

	return 0;
}

static int rtl8373_port_bridge_flags(struct dsa_switch *ds, int port,
				     RTL8373_BR_FLAGS_TYPE flags,
				     struct netlink_ext_ack *extack)
{
	struct realtek_smi *smi = ds->priv;
	int ret = 0;

	if (!dsa_is_user_port(ds, port))
		return 0;

	if (RTL8373_BR_FLAGS_MASK(flags) & BR_LEARNING) {
		bool learning = !!(RTL8373_BR_FLAGS_VAL(flags) & BR_LEARNING);

		if (smi->proto == DSA_TAG_PROTO_RTL8_4 || smi->proto == DSA_TAG_PROTO_QCA_8021Q)
			return 0;

		if (smi->ops->l2_limit_learning_cnt_set) {
			mutex_lock(&smi->map_lock);
			ret = smi->ops->l2_limit_learning_cnt_set(
				smi, port,
				learning ? rtl8373_LEARN_LIMIT_MAX : 0);
			mutex_unlock(&smi->map_lock);

			if (ret != RT_ERR_OK) {
				dev_err(smi->dev,
					"failed to set learning on port %d, ret=%d\n",
					port, ret);
				return -EIO;
			}
		}
	}

	return 0;
}

static void
_rtl8373_mac_link_up(struct dsa_switch *ds, int port, unsigned int mode,
		      phy_interface_t interface, struct phy_device *phydev,
		      int speed, int duplex, bool tx_pause, bool rx_pause)
{
	struct realtek_smi *smi = ds->priv;


	if (dsa_is_user_port(ds, port))
	{
		if (ds->ops->adjust_link && phydev)
			ds->ops->adjust_link(ds, port, phydev);
	}

	port = smi->ops->port_l2p_get(smi, port);
	if (port != smi->cpu_port)
		return;
#if 0 // [CBT] Already initial
	pr_debug("MAC link up on CPU port (%d)\n", port);

	ret = rtl8373_setAsicRegBits(smi->map, RTL8373_MAC_FORCE_MODE_CTRL0_ADDR(port),
				 RTL8373_MAC_FORCE_MODE_CTRL0_SPD_SEL_MASK, 4);
	if (ret) {
		dev_err(smi->dev, "failed to force 10 Gbit on CPU port\n");
		return;
	}
	/* Force the fixed CPU port into 10Gbit mode, no autonegotiation */
	ret = rtl8373_setAsicRegBits(smi->map, RTL8373_MAC_FORCE_MODE_CTRL0_ADDR(port),
				 RTL8373_MAC_FORCE_MODE_CTRL0_FORCE_LINK_EN_MASK|RTL8373_MAC_FORCE_MODE_CTRL0_MAC_FORCE_EN_MASK, 3);
	if (ret) {
		dev_err(smi->dev, "failed to force CPU port link up, ret=%d\n", ret);
		return;
	}
#endif

}
void rtl8373_mac_link_up(struct dsa_switch *ds, int port, unsigned int mode,
		      phy_interface_t interface, struct phy_device *phydev,
		      int speed, int duplex, bool tx_pause, bool rx_pause)
{
	struct realtek_smi *smi = ds->priv;

	mutex_lock(&smi->map_lock);
	_rtl8373_mac_link_up(ds, port, mode, interface, phydev, speed, duplex, tx_pause, rx_pause);
	mutex_unlock(&smi->map_lock);

}
static void
_rtl8373_mac_link_down(struct dsa_switch *ds, int port, unsigned int mode,
			phy_interface_t interface)
{
	struct realtek_smi *smi = ds->priv;


	if (dsa_is_user_port(ds, port))
	{
		struct dsa_port *dp = dsa_to_port(ds, port);
		struct phy_device *phydev = NULL;
		if (dp && dp->slave) {
			phydev = dp->slave->phydev;
			if (ds->ops->adjust_link && phydev)
				ds->ops->adjust_link(ds, port, phydev);
		}
	}

	port = smi->ops->port_l2p_get(smi, port);
	if (port != smi->cpu_port)
		return;
#if 0 // [CBT] Already initial	
	pr_debug("MAC link down on CPU port (%d)\n", port);

	/* Force the fixed CPU port into 1Gbit mode, no autonegotiation */
	ret = rtl8373_setAsicRegBits(smi->map, RTL8373_MAC_FORCE_MODE_CTRL0_ADDR(port),
				 RTL8373_MAC_FORCE_MODE_CTRL0_FORCE_LINK_EN_MASK|RTL8373_MAC_FORCE_MODE_CTRL0_MAC_FORCE_EN_MASK, 1);
	if (ret) {
		dev_err(smi->dev, "failed to force 1Gbit on CPU port\n");
		return;
	}
#endif
}

void rtl8373_mac_link_down(struct dsa_switch *ds, int port, unsigned int mode,
			phy_interface_t interface)
{
	struct realtek_smi *smi = ds->priv;
	
	mutex_lock(&smi->map_lock);
	_rtl8373_mac_link_down(ds, port, mode, interface);
	mutex_unlock(&smi->map_lock);
}
// [CBT]remove 'static' for fix build error.
//static int rtl8373_reset_chip(struct realtek_smi *smi)
int rtl8373_reset_chip(struct realtek_smi *smi)
{
	int timeout = 10;
	u32 val;
	int ret;

	ret = rtl8373_setAsicReg(smi->map, RTL8373_RST_GLB_CTRL_0_ADDR, RTL8373_CHIP_CTRL_RESET_HW);
    if(ret != RT_ERR_OK)
	{
		return ret;
	}
        
	do {
		usleep_range(20000, 25000);
		ret = regmap_read(smi->map, RTL8373_RST_GLB_CTRL_0_ADDR, &val);
		if (ret)
		{
			return ret;
		}

		if (!(val & RTL8373_CHIP_CTRL_RESET_HW))
			break;
	} while (--timeout);

	if (!timeout) {
		dev_err(smi->dev, "timeout waiting for the switch to reset\n");
		return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_reset_chip, REALTEK_DSA);
extern struct rtl8373_mib_counter rtl8373_mib_counters[];
extern int rtl8373_mib_size(void);
static int rtl8373_detect(struct realtek_smi *smi)
{
	struct device *dev = smi->dev;
	int ret;
	u32 val;
	/* Detect device */
	ret = regmap_read(smi->map, 0x4, &val);

	if (ret) {
		dev_err(dev, "can't get chip ID (%d)\n", ret);
		return ret;
	}
	val = val>> 8;
	switch (val) {
	case 0x837370:
		dev_info(dev, "found an RTL8373N switch\n");
	//	smi->mib_counters = rtl8373_mib_counters;
	//	smi->num_mib_counters = ARRAY_SIZE(rtl8373_mib_counters);
		smi->cpu_port = 8;// onlay port 8 can set cpu port
		smi->num_ports = 9;
		smi->start_lports = 0;
		smi->end_lports = 8;
		smi->start_pports = 0;
		smi->end_pports = 8;	
		smi->num_vlan_mc = RTL8373_NUM_VLANS;
    	smi->mib_counters = rtl8373_mib_counters;
	    smi->num_mib_counters = rtl8373_mib_size();
        break;
		
	case 0x837270:
		dev_info(dev, "found an RTL8372N switch\n");
		smi->cpu_port = 3;//port3 or 8 can set cpu port
		smi->num_ports = 9;
		smi->start_lports = 3;
		smi->end_lports = 8;
		smi->start_pports = 3;
		smi->end_pports = 8;	
		smi->num_vlan_mc = RTL8373_NUM_VLANS;
    	smi->mib_counters = rtl8373_mib_counters;
	    smi->num_mib_counters = rtl8373_mib_size();
        break;
	
	case 0x8366A8:
		dev_info(dev, "found an RTL8366U switch\n");
		smi->cpu_port = 3;  //port3 or 8 can set cpu port
		smi->num_ports = 9;
		smi->start_lports = 0;
		smi->end_lports = 5;
		smi->start_pports = 3;
		smi->end_pports = 8;	
		smi->num_vlan_mc = RTL8373_NUM_VLANS;
    	smi->mib_counters = rtl8373_mib_counters;
	    smi->num_mib_counters = rtl8373_mib_size();
        break;
	default:
		dev_err(dev, "unknown Realtek switch (id=0x%04x)\n", val);
		return -ENODEV;
	}
		
	ret = rtl8373_switch_init(smi);
	if (ret) {
		dev_err(smi->dev, "failed to initialize switch: %d\n", ret);		
	}
	dev_info(smi->dev, "rtl8373_switch_init() ret: %d\n", ret);

	return ret;
}

static int rtl8373_port_enable(struct dsa_switch *ds, int port,
			       struct phy_device *phy)
{
	struct realtek_smi *priv = ds->priv;
	int ret = 0;
	int pport;
	u32 mask;

	mutex_lock(&priv->map_lock);

	if (dsa_is_user_port(ds, port)) {
		pport = priv->ops->port_l2p_get(priv, port);
		if (pport < 0) {
			mutex_unlock(&priv->map_lock);
			return -EINVAL;
		}

		mask = BIT(pport);
		pr_info("P%d enabled (pport=%d mask=0x%x)\n", port, pport, mask);

		ret = rtl8373_phymask_write(priv, mask, 31, 0xa610, 0x2058);
		if (ret)
			dev_err(priv->dev,
				"failed to enable port %d (pport=%d), ret=%d\n",
				port, pport, ret);
	}

	mutex_unlock(&priv->map_lock);
	return ret;
}


static void rtl8373_port_disable(struct dsa_switch *ds, int port)
{
	struct realtek_smi *priv = ds->priv;
	int ret;
	int pport;
	u32 mask;

	mutex_lock(&priv->map_lock);

	if (dsa_is_user_port(ds, port)) {
		pport = priv->ops->port_l2p_get(priv, port);
		if (pport < 0) {
			mutex_unlock(&priv->map_lock);
			return;
		}

		mask = BIT(pport);
		pr_info("P%d disabled (pport=%d mask=0x%x)\n", port, pport, mask);

		ret = rtl8373_phymask_write(priv, mask, 31, 0xa610, 0x2858);
		if (ret)
			dev_err(priv->dev,
				"failed to disable port %d (pport=%d), ret=%d\n",
				port, pport, ret);
	}

	mutex_unlock(&priv->map_lock);
}

static bool rtl8373_phy_mode_supported(struct dsa_switch *ds, int port,
					 phy_interface_t interface)
{
	if (dsa_is_user_port(ds, port) &&
	    (interface == PHY_INTERFACE_MODE_NA ||
	     interface == PHY_INTERFACE_MODE_INTERNAL ||
	     interface == PHY_INTERFACE_MODE_SGMII ))
		/* Internal PHY */
		return true;
	else if (dsa_is_cpu_port(ds, port))
		/* Extension MAC */
		return true;

	return false;
}
#if 0
static int _rtl8373_sdsMode_set(struct realtek_smi *priv, u32 port,
				phy_interface_t interface)
{
	rtk_sds_mode_t mode;
	int pport;

	pport = priv->ops->port_l2p_get(priv, port);
	if (pport < 0)
		return RT_ERR_PORT_ID;

	switch (interface) {
	case PHY_INTERFACE_MODE_10GBASER:
		mode = SERDES_10GR;
		break;
	case PHY_INTERFACE_MODE_USXGMII:
		mode = SERDES_10GUSXG;
		break;
	case PHY_INTERFACE_MODE_SGMII:
		mode = SERDES_SG;
		break;
	case PHY_INTERFACE_MODE_2500BASEX:
		mode = SERDES_2500BASEX;
		break;
	case PHY_INTERFACE_MODE_1000BASEX:
		mode = SERDES_1000BASEX;
		break;
	default:
		return RT_ERR_INPUT;
	}

	if (pport == 3) {
		dal_rtl8373_port_sdsNway_set(priv, SERDES_ID0, mode,
					     (mode == SERDES_10GUSXG) ? 0 : 1);
		return dal_rtl8373_sdsMode_set(priv, SERDES_ID0, mode);
	} else if (pport == 8) {
		dal_rtl8373_port_sdsNway_set(priv, SERDES_ID1, mode,
					     (mode == SERDES_10GUSXG) ? 0 : 1);
		return dal_rtl8373_sdsMode_set(priv, SERDES_ID1, mode);
	}

	return RT_ERR_INPUT;
}
#endif
		
static void _rtl8373_phylink_mac_config(struct dsa_switch *ds, int port,
					 unsigned int mode,
					 const struct phylink_link_state *state)
{
	struct realtek_smi *smi = ds->priv;
	//int ret;

	pr_info("rtl8373_phylink_mac_config: port=%d interface=%s mode=%u link=%s\n",
            port, phy_modes(state->interface), mode,
            state->link ? "up" : "down");
			
	if (!rtl8373_phy_mode_supported(ds, port, state->interface)) {
		dev_err(smi->dev, "phy mode %s is unsupported on port %d\n",
			phy_modes(state->interface), port);
		return ;
	}

	if (mode != MLO_AN_PHY && mode != MLO_AN_FIXED) {
		dev_err(smi->dev,
			"port %d supports only conventional PHY or fixed-link\n",
			port);
		return;
	}
	if((state->interface == PHY_INTERFACE_MODE_10GBASER) ||
		(state->interface == PHY_INTERFACE_MODE_USXGMII) ||
		(state->interface == PHY_INTERFACE_MODE_SGMII) ||
		(state->interface == PHY_INTERFACE_MODE_2500BASEX) ||
		(state->interface == PHY_INTERFACE_MODE_1000BASEX) 
		)
	{
		#if 0
		if (dsa_is_cpu_port(ds, port)) {
			ret = _rtl8373_sdsMode_set(smi, port, state->interface);
			if (ret)
				dev_err(smi->dev,
					"failed to configure serdes mode on port %d: %d\n",
					port, ret);
		}
		#endif
		return;
	}
}

void rtl8373_phylink_mac_config(struct dsa_switch *ds, int port,
					 unsigned int mode,
					 const struct phylink_link_state *state)
{
	struct realtek_smi *smi = ds->priv;
	
	mutex_lock(&smi->map_lock);
	_rtl8373_phylink_mac_config(ds,port,mode,state);
	mutex_unlock(&smi->map_lock);

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
static void rtl8373_phylink_get_caps(struct dsa_switch *ds, int port,
				       struct phylink_config *config)
{
	unsigned long *interfaces = config->supported_interfaces;
	struct realtek_smi *smi = ds->priv;
	u32 extphyid=0;
	int ret;
	
	ret = dal_rtl8373_port_extphyid_get(smi, SERDES_ID1, &extphyid);

	if (dsa_is_cpu_port(ds, port)) {
		config->mac_capabilities =
			MAC_10000FD | MAC_SYM_PAUSE | MAC_ASYM_PAUSE;

		__set_bit(PHY_INTERFACE_MODE_USXGMII, interfaces);
		__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
		
		//printk("%s(%d) port %d is CPU port, supported_interfaces: %*pb\n",__func__,__LINE__,port,__ETHTOOL_LINK_MODE_MASK_NBITS, interfaces);
	} 
	else if (port == UTP_PORT8 && ret == RT_ERR_OK && extphyid != 0) {
		/* External 10G PHY port
		 */
		config->mac_capabilities =
			MAC_10000FD | MAC_2500FD | MAC_1000FD | MAC_100FD | 
			MAC_SYM_PAUSE | MAC_ASYM_PAUSE;

		__set_bit(PHY_INTERFACE_MODE_USXGMII, interfaces);
		__set_bit(PHY_INTERFACE_MODE_2500BASEX, interfaces);
		__set_bit(PHY_INTERFACE_MODE_SGMII, interfaces);
		__set_bit(PHY_INTERFACE_MODE_GMII, interfaces);
		__set_bit(PHY_INTERFACE_MODE_RGMII, interfaces);
		
		//printk("%s(%d) port %d is external PHY port, supported_interfaces: %*pb\n",__func__,__LINE__,port,__ETHTOOL_LINK_MODE_MASK_NBITS, interfaces);
	} 
	else 
	{
		/* RSGMII port, but we don't have that, and we don't
		 * specify in DT, so phylib uses the default of GMII
		 */
		config->mac_capabilities =
			MAC_2500FD | MAC_1000FD | MAC_100FD | MAC_10FD |
			MAC_SYM_PAUSE | MAC_ASYM_PAUSE;

		__set_bit(PHY_INTERFACE_MODE_2500BASEX, interfaces);
		__set_bit(PHY_INTERFACE_MODE_SGMII, interfaces);
		__set_bit(PHY_INTERFACE_MODE_GMII, interfaces);
		__set_bit(PHY_INTERFACE_MODE_RGMII, interfaces);
		
		printk("%s(%d) port %d is internal PHY port, supported_interfaces: %*pb\n",__func__,__LINE__,port,__ETHTOOL_LINK_MODE_MASK_NBITS, interfaces);
	}
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0)
static void _rtl8373_phylink_validate(struct dsa_switch *ds, int port,
                                     unsigned long *supported,
                                     struct phylink_link_state *state)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mac_supported) = { 0, };

	pr_info("rtl8373_phylink_validate: port=%d interface=%d (%s) incoming supported[0]=%#lx advertising[0]=%#lx\n",
			port, state->interface, phy_modes(state->interface),
			supported[0], state->advertising[0]);

	/* CPU port: high-speed SerDes */
	if (dsa_is_cpu_port(ds, port)) {
		phylink_set(mac_supported, 10000baseT_Full);
		phylink_set(mac_supported, 2500baseX_Full);
		phylink_set(mac_supported, 1000baseX_Full);
		phylink_set(mac_supported, Pause);
		phylink_set(mac_supported, Asym_Pause);

		linkmode_zero(state->advertising);
		phylink_set(state->advertising, 10000baseT_Full);
		phylink_set(state->advertising, 2500baseX_Full);
		phylink_set(state->advertising, 1000baseX_Full);
	} else {
		/* USER port */
		linkmode_zero(state->advertising);

		switch (state->interface) {
		case PHY_INTERFACE_MODE_2500BASEX:
			phylink_set(mac_supported, 2500baseX_Full);
			phylink_set(state->advertising, 2500baseX_Full);
			break;

		case PHY_INTERFACE_MODE_SGMII:
			phylink_set(mac_supported, 2500baseT_Full);  // HiSGMII
			phylink_set(mac_supported, 1000baseT_Full);
			phylink_set(mac_supported, 100baseT_Full);
			phylink_set(mac_supported, 10baseT_Full);

			phylink_set(state->advertising, 2500baseT_Full);
			phylink_set(state->advertising, 1000baseT_Full);
			phylink_set(state->advertising, 100baseT_Full);
			phylink_set(state->advertising, 10baseT_Full);
			break;

		case PHY_INTERFACE_MODE_INTERNAL:
			phylink_set(mac_supported, 2500baseT_Full);
            phylink_set(mac_supported, 1000baseT_Full);
            phylink_set(mac_supported, 100baseT_Full);
            phylink_set(mac_supported, 10baseT_Full);

            phylink_set(state->advertising, 2500baseT_Full);
            phylink_set(state->advertising, 1000baseT_Full);
            phylink_set(state->advertising, 100baseT_Full);
            phylink_set(state->advertising, 10baseT_Full);
            break;

		default:
			phylink_set(mac_supported, 2500baseT_Full);  // SGMII
			phylink_set(mac_supported, 1000baseT_Full);
			phylink_set(mac_supported, 100baseT_Full);
			phylink_set(mac_supported, 10baseT_Full);

			phylink_set(state->advertising, 2500baseT_Full);
			phylink_set(state->advertising, 1000baseT_Full);
			phylink_set(state->advertising, 100baseT_Full);
			phylink_set(state->advertising, 10baseT_Full);
			pr_info("%s: unsupported interface %s on port %d\n",
					__func__, phy_modes(state->interface), port);
			return;
		}

		phylink_set(mac_supported, Autoneg);
		phylink_set(mac_supported, Pause);
		phylink_set(mac_supported, Asym_Pause);
		
		phylink_set(state->advertising, Autoneg);
		phylink_set(state->advertising, Pause);
		phylink_set(state->advertising, Asym_Pause);
	}

out:
	linkmode_and(supported, supported, mac_supported);
	linkmode_and(state->advertising, state->advertising, supported);

#if 1
	pr_info("rtl8373_phylink_validate: port=%d iface=%s mac_supported[0]=%#lx supported[0]=%#lx advertising[0]=%#lx\n",
			port, phy_modes(state->interface),
			mac_supported[0], supported[0], state->advertising[0]);
#endif
}
void rtl8373_phylink_validate(struct dsa_switch *ds, int port,
                                     unsigned long *supported,
                                     struct phylink_link_state *state)
{
	struct realtek_smi *smi = ds->priv;
	mutex_lock(&smi->map_lock);
	_rtl8373_phylink_validate(ds,port,supported,state);
	mutex_unlock(&smi->map_lock);
}
#endif
#if 0
static void rtl8373_adjust_link(struct dsa_switch *ds, int port, struct phy_device *phy)
{
	struct dsa_port *dp = dsa_to_port(ds, port);
	struct realtek_smi *smi = ds->priv;
	rtk_port_ability_t ability;
	int pport, ret;

	if (!phy || !phy->drv) {
		dev_dbg(smi->dev, "%s: port=%d phy not ready\n", __func__, port);
		return;
	}

	printk(KERN_DEBUG "%s(%d):XXX %s(port=%d) link:%d s:%d d:%d phymode:%d phy_id:0x%x phy_id_mask:0x%x\n",
	       __func__, __LINE__,
	       (dp && dp->slave) ? dp->slave->name : "NULL",
	       port, phy->link, phy->speed, phy->duplex, phy->interface,
	       phy->drv->phy_id, phy->drv->phy_id_mask);

	if (!dsa_is_user_port(ds, port))
		return;

	memset(&ability, 0, sizeof(rtk_port_ability_t));

	pport = smi->ops->port_l2p_get(smi, port);
	if (pport < 0)
		return;

	ability.forcemode = RTK_ENABLED;
	ability.txpause = RTK_ENABLED;
	ability.rxpause = RTK_ENABLED;
	ability.link = phy->link ? PORT_LINKUP : PORT_LINKDOWN;
	ability.duplex = (phy->duplex == DUPLEX_FULL) ? PORT_FULL_DUPLEX : PORT_HALF_DUPLEX;
	ability.speed = (phy->speed == SPEED_2500) ? PORT_SPEED_2500M :
			(phy->speed == SPEED_1000) ? PORT_SPEED_1000M :
			(phy->speed == SPEED_100)  ? PORT_SPEED_100M :
			(phy->speed == SPEED_10)   ? PORT_SPEED_10M : 0;
	ability.media = PORT_MEDIA_UTP;

	mutex_lock(&smi->map_lock);
	ret = dal_rtl8373_portFrcAbility_set(smi, pport, &ability);
	if (ret)
		printk(KERN_ERR "dal_rtl8373_portFrcAbility_set port=%d spd=%d failed, ret=%d\n",
		       port, ability.speed, ret);
	mutex_unlock(&smi->map_lock);
}
#endif

static int rtl8373_dsa_phy_read(struct dsa_switch *ds, int phy, int regnum)
{
	struct realtek_smi *smi = ds->priv;
	int pphy;

	if (smi->slave_mii_bus)
		return -ENODEV;

	pphy = smi->ops->port_l2p_get(smi, phy);
	if (pphy < 0)
		return -EINVAL;

	return realtek_phy_read(smi, pphy, regnum);
}

static int rtl8373_dsa_phy_write(struct dsa_switch *ds, int phy, int regnum,
				 u16 val)
{
	struct realtek_smi *smi = ds->priv;
	int pphy;

	if (smi->slave_mii_bus)
		return -ENODEV;

	pphy = smi->ops->port_l2p_get(smi, phy);
	if (pphy < 0)
		return -EINVAL;

	return realtek_phy_write(smi, pphy, regnum, val);
}

static const struct dsa_switch_ops rtl8373_switch_ops = {
	.port_enable		= rtl8373_port_enable,
	.port_disable		= rtl8373_port_disable,

	.port_bridge_join	= rtl8373_port_bridge_join,
	.port_bridge_leave	= rtl8373_port_bridge_leave,
	.port_pre_bridge_flags	= rtl8373_port_pre_bridge_flags,
	.port_bridge_flags	= rtl8373_port_bridge_flags,

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0)
	.phylink_validate	= rtl8373_phylink_validate,
#endif
	.get_tag_protocol	= rtl8373_get_tag_protocol,
	.setup			= rtl8373_setup,
	.phylink_mac_config	= rtl8373_phylink_mac_config,
	.phylink_mac_link_up	= rtl8373_mac_link_up,
	.phylink_mac_link_down	= rtl8373_mac_link_down,
	.get_ethtool_stats	= rtl8373_get_ethtool_stats,
	.get_strings		= rtl8373_get_strings,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
	.phylink_get_caps	= rtl8373_phylink_get_caps,
#endif
	.get_sset_count		= rtl8373_get_sset_count,
	.port_vlan_filtering	= rtl8373_port_vlan_filtering,
	.port_vlan_add		= rtl8373_port_vlan_add,
	.port_vlan_del		= rtl8373_port_vlan_del,

	.port_fdb_add		= rtl8373_port_fdb_add,
	.port_fdb_del		= rtl8373_port_fdb_del,
	.port_fdb_dump		= rtl8373_port_fdb_dump,

	.port_mdb_add		= rtl8373_port_mdb_add,
	.port_mdb_del		= rtl8373_port_mdb_del,

	.port_max_mtu		= rtl8373_portMaxLen_get,
	.port_change_mtu	= rtl8373_change_mtu,
	.port_stp_state_set	= rtl8373_port_stp_state_set,

	.get_mac_eee		= rtl8373_get_mac_eee,
	.set_mac_eee		= rtl8373_set_mac_eee,
#if 0
	.adjust_link		= rtl8373_adjust_link,
#endif
	.phy_read		= rtl8373_dsa_phy_read,
	.phy_write		= rtl8373_dsa_phy_write,
};


static int realtek_phy_read(struct realtek_smi *smi, int phy, int regnum)
{
	u32 val;
	int ret;

	ret = rtl8373_phy_read(smi, phy, 0x7, regnum, &val);
	if (ret)
		return ret;

	return (int)val;
}


static int realtek_phy_write(struct realtek_smi *smi, int phy, int regnum, u16 val)
{
	u32 val32;
	val32 = 0xffff&val;
    return rtl8373_phy_write(smi, phy, 0x7, regnum, val32);
}
int rtl8373_probe(struct realtek_smi *chip)
{
	struct device *dev = chip->dev;
	int ret;

	if (!chip || !dev)
		return -EINVAL;

	chip->ops = &rtl8373_smi_ops;
	
	dev_info(dev, "[CBT] rtl8373_probe()\n");

	chip->svlan_disable = of_property_read_bool(dev->of_node,
						    "svlan-disable");
	if (chip->svlan_disable)
		dev_info(dev, "default SVLAN function disabled by DTS\n");

	ret = chip->ops->detect(chip);
	if (ret)
		return ret;

	chip->ds = devm_kzalloc(dev, sizeof(*chip->ds), GFP_KERNEL);
	if (!chip->ds)
		return -ENOMEM;

	chip->ds->dev = dev;
	chip->ds->num_ports = chip->num_ports;
	chip->ds->ops = &rtl8373_switch_ops;
	chip->ds->priv = chip;

	ret = dsa_register_switch(chip->ds);
	if (ret) {
		dev_err(dev, "unable to register switch ret=%d\n", ret);
		return ret;
	}

	if (chip->ops->setup_rest) {
		ret = chip->ops->setup_rest(chip->ds);
		if (ret) {
			dev_err(dev, "setup_rest failed ret=%d\n", ret);
			dsa_unregister_switch(chip->ds);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(rtl8373_probe);

const struct realtek_smi_ops rtl8373_smi_ops = {
	.detect		= rtl8373_detect,
	.is_vlan_valid	= rtl8373_is_vlan_valid,
	.phy_read	= realtek_phy_read,
	.phy_write	= realtek_phy_write,
	.phyc45_read	= rtl8373_phy_read,
	.phyc45_write	= rtl8373_phy_write,
	.port_l2p_get = rtl8373_port_L2P_get,
	.port_p2l_get = rtl8373_port_P2L_get,
	.enable_vlan  = __rtl8373_enable_vlan,
	.enable_vlan4k= __rtl8373_enable_vlan4k,
#if 0
	.get_vlan_mc	= rtl8373_get_vlan_mc,
	.set_vlan_mc	= rtl8373_set_vlan_mc,
	.get_vlan_4k	= rtl8373_get_vlan_4k,
	.set_vlan_4k	= rtl8373_set_vlan_4k,
	.get_mc_index	= rtl8373_get_mc_index,
	.set_mc_index	= rtl8373_set_mc_index,
	.get_mib_counter = rtl8373_get_mib_counter,
#endif
	.svlan_init = dal_rtl8373_svlanInit,
	.svlanTpid_set = dal_rtl8373_svlanTpid_set,
	.svlan_service_port_add = dal_rtl8373_svlanServicePort_add,
	.svlan_service_port_get = dal_rtl8373_svlanServicePort_get,
	.svlan_member_port_entry_set = dal_rtl8373_svlanMbrPortEntry_set,
	.svlan_member_port_entry_get = dal_rtl8373_svlanMbrPortEntry_get,
	.svlan_default_svlan_set = dal_rtl8373_svlanDfltSvlan_set,
	.svlan_default_svlan_get = dal_rtl8373_svlanDfltSvlan_get,
	.port_isolation_set = rtl8373_set_port_isolation,
	.port_isolation_get = rtl8373_get_port_isolation,
	.port_link_status_get = dal_rtl8373_portStatus_get,
	.port_serdes_mode_get = dal_rtl8373_sdsMode_get,
	.port_serdes_nway_get = dal_rtl8373_port_sdsNway_get,
	.l2_limit_learning_cnt_set = dal_rtl8373_l2_limitLearningCnt_set,
	.l2_limit_learning_cnt_get = dal_rtl8373_l2_limitLearningCnt_get,
	.setup_rest = rtl8373_setup_rest,
};

const struct realtek_smi_variant rtl8373_variant = {
	.ds_ops = &rtl8373_switch_ops,
	.ops = &rtl8373_smi_ops,
	.clk_delay = 10,
	.cmd_read = 0xa9,
	.cmd_write = 0xa8,
	.chip_data_sz = sizeof(struct rtl8373),
};
EXPORT_SYMBOL_GPL(rtl8373_variant);

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(REALTEK_DSA);
