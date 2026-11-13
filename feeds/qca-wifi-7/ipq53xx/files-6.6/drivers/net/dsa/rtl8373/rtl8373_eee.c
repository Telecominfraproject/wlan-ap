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
#include "rtl8373_eee.h"
#include "rtl8373_phy.h"

static int _rtl8373_get_mac_eee(struct dsa_switch *ds, int port, struct ethtool_eee *e)
{
	struct realtek_smi *smi = ds->priv;
	int ret;
	u32 TxEeeVal,RxEeeVal,Abilityval;

	ret = rtl8373_getAsicRegBit(smi->map_nolock,RTL8373_EEE_CTRL_ADDR(smi->ops->port_l2p_get(smi, port)), RTL8373_EEE_CTRL_EEE_RX_STS_OFFSET ,  &RxEeeVal);
	if (ret)
	{
		return ret;
	}
	ret = rtl8373_getAsicRegBit(smi->map_nolock,RTL8373_EEE_CTRL_ADDR(smi->ops->port_l2p_get(smi, port)), RTL8373_EEE_CTRL_EEE_TX_STS_OFFSET ,  &TxEeeVal);
	if (ret)
	{
		return ret;
	}
	ret = rtl8373_getAsicRegBit(smi->map_nolock,RTL8373_MAC_EEE_ABLTY_ADDR, RTL8373_MAC_EEE_ABLTY_EEE_ABLTY_9_0_OFFSET ,  &Abilityval);
	if (ret)
	{
		return ret;
	}
	e->eee_enabled = (Abilityval>>smi->ops->port_l2p_get(smi, port))&1;
	e->eee_active = RxEeeVal|TxEeeVal;

	return 0;
}
int rtl8373_get_mac_eee(struct dsa_switch *ds, int port, struct ethtool_eee *e)
{
	struct realtek_smi *smi = ds->priv;
	int ret;
	
    if (!dsa_is_user_port(ds, port))
	{
		return -EINVAL;
	}
        
	port = smi->ops->port_l2p_get(smi, port);
	mutex_lock(&smi->map_lock);
	ret = _rtl8373_get_mac_eee(ds, port, e);
	mutex_unlock(&smi->map_lock);
	
	return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_get_mac_eee, REALTEK_DSA);



int _rtl8373_set_mac_eee(struct dsa_switch *ds, int port, struct ethtool_eee *e)
{
	struct realtek_smi *smi = ds->priv;
	int retVal;
	u32 phyData;

	if((retVal = rtl8373_setAsicRegBit(smi->map_nolock, RTL8373_MAC_FORCE_MODE_CTRL1_ADDR(smi->ops->port_l2p_get(smi, port)), EEE_MAC_FORCE_SPEED_100M,  e->eee_enabled)) != RT_ERR_OK)
        return retVal;  
	if((retVal = rtl8373_setAsicRegBit(smi->map_nolock, RTL8373_MAC_FORCE_MODE_CTRL1_ADDR(smi->ops->port_l2p_get(smi, port)), EEE_MAC_FORCE_SPEED_1000M,  e->eee_enabled)) != RT_ERR_OK)
        return retVal; 
	if((retVal = rtl8373_setAsicRegBit(smi->map_nolock, RTL8373_MAC_FORCE_MODE_CTRL1_ADDR(smi->ops->port_l2p_get(smi, port)), EEE_MAC_FORCE_SPEED_2P5G,  e->eee_enabled)) != RT_ERR_OK)
        return retVal; 
	if((retVal = rtl8373_phy_regbits_write(smi,1<<smi->ops->port_l2p_get(smi, port), 7, 62, 1, e->eee_enabled)) != RT_ERR_OK)
			return retVal;  
	if((retVal = rtl8373_phy_regbits_write(smi,1<<smi->ops->port_l2p_get(smi, port), 7, 60, 1<<2, e->eee_enabled)) != RT_ERR_OK)
			return retVal;
	if((retVal = rtl8373_phy_regbits_write(smi,1<<smi->ops->port_l2p_get(smi, port), 7, 60, 1<<1, e->eee_enabled)) != RT_ERR_OK)
			return retVal;  

	if ((retVal = rtl8373_phy_read(smi, smi->ops->port_l2p_get(smi, port), PHY_MMD_AN, 0, &phyData)) != 0)
	return retVal;

    if (phyData & BIT_12) /*AN is RTK_ENABLED*/
    {
        phyData |= BIT_9; /*AN restart*/
        if ((retVal = rtl8373_phy_write(smi,  1<<smi->ops->port_l2p_get(smi, port), PHY_MMD_AN, 0, phyData)) != 0)
            return retVal;
    }
	return 0;
}
int rtl8373_set_mac_eee(struct dsa_switch *ds, int port, struct ethtool_eee *e)
{
	struct realtek_smi *smi = ds->priv;
	int ret;

    if (!dsa_is_user_port(ds, port))
	{
		return -EINVAL;
	}
        
	mutex_lock(&smi->map_lock);
	ret = _rtl8373_set_mac_eee(ds, port, e);
	mutex_unlock(&smi->map_lock);
	
	return ret;
}


EXPORT_SYMBOL_NS_GPL(rtl8373_set_mac_eee, REALTEK_DSA);


