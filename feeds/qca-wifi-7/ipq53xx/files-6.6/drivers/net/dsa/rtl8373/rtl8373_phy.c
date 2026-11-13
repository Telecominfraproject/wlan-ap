// SPDX-License-Identifier: GPL-2.0
#include <linux/phy.h>
#include <linux/phylink.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/if.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <net/genetlink.h>
//#include <linux/switch.h>
#include <linux/delay.h>
//#include <linux/phy.h>
#include <linux/version.h>

#include "realtek-smi-rtl8373.h"
#include "rtl8373_core.h"
#include "rtl8373_lut.h"
#include "rtl8373_interrupt.h"
#include "rtl8373_vlan.h"
#include "rtl8373_mib.h"
#include "rtl8373_switch.h"
#include "rtl8373_phy.h"
/**/

static int rtl8373_phy_read_mmd(struct realtek_smi *priv, u32 port, u32 dev_addr, u32 reg_addr, u32 *indata)
{
	struct dsa_port *dp;
	struct phy_device *phydev;
	int ret;

	dp = dsa_to_port(priv->ds, port);
	if (!dp)
		return -ENODEV;

	if (dp->type != DSA_PORT_TYPE_USER || !dp->slave)
		return -ENODEV;

	phydev = dp->slave->phydev;
	if (!phydev)
		return -ENODEV;

	ret = rtl8373_phy_read(priv, port, dev_addr, reg_addr, indata);
	if (ret < 0)
		return ret;

	return 0;
}

static int rtl8373_phy_write_mmd(struct realtek_smi *priv, u32 port, u32 dev_addr, u32 reg_addr, u32 indata)
{
	struct dsa_port *dp;
	struct phy_device *phydev;

	dp = dsa_to_port(priv->ds, port);
	if (!dp || dp->type != DSA_PORT_TYPE_USER || !dp->slave)
		return -ENODEV;

	phydev = dp->slave->phydev;
	if (!phydev)
		return -ENODEV;
	return rtl8373_phy_write(priv, 1<<port, dev_addr, reg_addr, (u16)indata);
}

static int rtl8373_phy_config_init(struct phy_device *phydev)
{
	//printk("@@@@ %s: enter\n", __func__);
	
    phylink_set_port_modes(phydev->supported);
    phylink_set(phydev->supported, Autoneg);
    
    phylink_set(phydev->supported, 10baseT_Full);
    phylink_set(phydev->supported, 100baseT_Full);
    phylink_set(phydev->supported, 1000baseT_Full);
    phylink_set(phydev->supported, 2500baseT_Full);

    phylink_set(phydev->supported, Pause);
    phylink_set(phydev->supported, Asym_Pause);

	//printk("@@@@ %s: exit, supported = %*pb\n", __func__, __ETHTOOL_LINK_MODE_MASK_NBITS, phydev->supported);

    return 0;
}

static int rtl8373_phy_probe(struct phy_device *phydev)
{
	//printk("@@@@ %s: enter\n", __func__);
    return 0;
}

static int rtl8372_phy_get_features(struct phy_device *phydev)
{
	//printk("@@@@ %s: enter\n", __func__);
    phylink_set_port_modes(phydev->supported);
    phylink_set(phydev->supported, Autoneg);

    phylink_set(phydev->supported, 10baseT_Full);
    phylink_set(phydev->supported, 100baseT_Full);
    phylink_set(phydev->supported, 1000baseT_Full);
    phylink_set(phydev->supported, 2500baseT_Full);

    phylink_set(phydev->supported, Pause);
    phylink_set(phydev->supported, Asym_Pause);

    return 0;
}

/* Function Name:
*      rtl8373_phy_common_c45_an_restart
* Description:
*      After set auto NegoAbility,restart the phy
* Input:
*      port - port id
* Output:
*      None
* Return:
*      0     - OK
*      -1 - invalid parameter
* Note:
*      1000 is vendor specific in C45.
*/
int rtl8373_phy_common_c45_an_restart(struct realtek_smi *priv, int port)
{
    int ret;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_AN, 0, &phyData)) != 0)
        return ret;

    if (phyData & BIT_12) /*AN is RTK_ENABLED*/
    {
        phyData |= BIT_9; /*AN restart*/
        if ((ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_AN, 0, phyData)) != 0)
            return ret;
    }
    
    return ret;
}

/* Function Name:
 *      rtl8373_phy_common_c45_autoNegoEnable_get
 * Description:
 *      Get the auto-negotiation enable or not
 * Input:
 *      port - port id
 * Output:
 *      pEnable - pointer to output the auto-negotiation state
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_autoNegoEnable_get(struct realtek_smi *priv, int port, rtk_enable_t *pEnable)
{
    int   ret;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv,  port, PHY_MMD_AN, 0, &phyData)) != 0)
        return ret;

    *pEnable = (phyData & BIT_12) ? RTK_ENABLED : RTK_DISABLED;

    return ret;
}

/* Function Name:
 *      rtl8373_phy_common_c45_autoNegoEnable_set
 * Description:
 *      Set the auto-negotiation state of the specific port enable or not.
 * Input:
 *      port - port id
 *      enable - auto-negotiation state
 * Output:
 *      None
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_autoNegoEnable_set(struct realtek_smi *priv, int port, rtk_enable_t enable)
{
    int   ret;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_AN, 0, &phyData)) != 0)
        return ret;
    phyData &= (~(BIT_12 | BIT_9));
    phyData |= (enable == RTK_ENABLED) ? (BIT_12 | BIT_9) : (0);

    ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_AN, 0, phyData);
    return ret;
}

/* Function Name:
 *      rtl8373_phy_autoNegoAbility_set
 * Description:
 *      Set ability advertisement for auto-negotiation of the specific port
 * Input:
 *      unit - unit id
 *      port - port id
 *      pAbility  - auto-negotiation ability that is going to set to PHY
 * Output:
 *      None
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_autoNegoAbility_set(struct realtek_smi *priv, int port, rtk_port_phy_ability_t *pAbility)
{
    int  ret = 0;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv,  port, PHY_MMD_AN, 16, &phyData)) != 0)
        return ret;

    phyData &= (~(BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_10 | BIT_11));

    phyData |= (pAbility->Half_10) ? (BIT_5) : (0);
    phyData |= (pAbility->Full_10) ? (BIT_6) : (0);
    phyData |= (pAbility->Half_100) ? (BIT_7) : (0);
    phyData |= (pAbility->Full_100) ? (BIT_8) : (0);
    phyData |= (pAbility->FC) ? (BIT_10) : (0);
    phyData |= (pAbility->AsyFC) ? (BIT_11) : (0);

    if ((ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_AN, 16, phyData)) != 0)
        return ret;

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_AN, 32, &phyData)) != 0)
        return ret;

    phyData &= (~(BIT_7 | BIT_8 | BIT_12));
    phyData |= (pAbility->adv_2_5G) ? (BIT_7) : (0);

    phyData |= (pAbility->adv_5G) ? (BIT_8) : (0);
    phyData |= (pAbility->adv_10GBase_T) ? (BIT_12) : (0);

    if ((ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_AN, 32, phyData)) != 0)

        return ret;

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_VEND2, 0xA412, &phyData)) != 0)
        return ret;

    phyData &= (~(BIT_9));
    phyData |= (pAbility->Full_1000) ? (BIT_9) : (0);

    if ((ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_VEND2, 0xA412, phyData)) != 0)
        return ret;
    if ((ret = rtl8373_phy_common_c45_an_restart(priv, port) != 0))
    {
    //printf("line %d\n",(uint16)__LINE__);
        return ret;
    }

    return ret;
}

/* Function Name:
 *      rtl8373_phy_autoNegoAbility_get
 * Description:
 *      get ability advertisement for auto-negotiation of the specific port
 * Input:
 *      port - port id 
 * Output:
*      pAbility  - auto-negotiation ability of the specific port
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_autoNegoAbility_get(struct realtek_smi *priv, int port, rtk_port_phy_ability_t *pAbility)
{
    int ret = 0;
    u32 phyData = 0;
    rtk_enable_t AutoNegotiationEn;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_common_c45_autoNegoEnable_get(priv, port, &AutoNegotiationEn)) != 0)
	{
		return ret;
	}

    if(AutoNegotiationEn == 0)
    {
        return -1;
    } 

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_AN, 16, &phyData)) != 0)
        return ret;

    pAbility->Half_10 = (phyData >> 5) & 1;
    pAbility->Full_10 = (phyData >> 6) & 1;  
    pAbility->Half_100 = (phyData >> 7) & 1;
    pAbility->Full_100 = (phyData >> 8) & 1;
    pAbility->FC  = (phyData >> 10) & 1;
    pAbility->AsyFC = (phyData >> 11) & 1;

    phyData = 0;
    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_AN, 32, &phyData)) != 0)
        return ret;

    pAbility->adv_2_5G = (phyData >> 7) & 1;
    pAbility->adv_5G = (phyData >> 8) & 1;
    pAbility->adv_10GBase_T = (phyData >> 12) & 1;

    phyData = 0;
    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_VEND2, 0xA412, &phyData)) != 0)
        return ret;

    pAbility->Full_1000 = (phyData >> 9) & 1;

    return ret;
}
/* Function Name:
 *      rtl8373_phy_common_c45_autoSpeed_set
 * Description:
 *      Set phy speed for the specific port
 * Input:
 *      port - port id
 *      pAbility  - phy ability
 * Output:
 *      None
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */

int rtl8373_phy_common_c45_autoSpeed_set(struct realtek_smi *priv, int port, rtk_port_phy_ability_t *pAbility)
{
	u32 ret = -1;
	rtk_enable_t AutoNegotiationEn = 1;
	rtk_port_phy_ability_t ability;
	//printk("@@@@ %s: enter\n", __func__);
	
	memset(&ability,0,sizeof(rtk_port_phy_ability_t));
		
	ability.Half_10 		= pAbility->Half_10;
	ability.Full_10 		= pAbility->Full_10;
	ability.Half_100		= pAbility->Half_100;
	ability.Full_100		= pAbility->Full_100;
	ability.Full_1000		= pAbility->Full_1000;
	ability.adv_2_5G   = pAbility->adv_2_5G;
	ability.adv_5G   = pAbility->adv_5G;
	ability.adv_10GBase_T	=  pAbility->adv_10GBase_T;
	ability.FC				= pAbility->FC;
	ability.AsyFC			= pAbility->AsyFC;

	if ((ret = rtl8373_phy_common_c45_autoNegoEnable_set(priv, port, AutoNegotiationEn)) != 0)
	{
		return ret;
	}
	
	if ((ret = rtl8373_phy_autoNegoAbility_set(priv, port, &ability)) != 0)
	{
		return ret;
	}

	return 0;

}

/* Function Name:
 *      rtl8373_phy_common_c45_speed_get
 * Description:
 *      Get speed of the specific port
 * Input:
 *      port - port id
 * Output:
 *      pSpeed - pointer to PHY link speed
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_speed_get(struct realtek_smi *priv, int port, rtk_port_speed_t *pSpeed)
{
    int   ret;
    u32 phyData = 0;
    u8   speed_l = 0;
    u8   speed_h = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_PMAPMD, 0, &phyData)) != 0)
        return ret;

    speed_l = (((phyData & BIT_6) ? 1 : 0) << 1) | ((phyData & BIT_13) ? 1 : 0);
    speed_h = (phyData & (BIT_5 | BIT_4 | BIT_3 | BIT_2)) >> 2;

    switch (speed_l)
    {
        case 0:
            *pSpeed = PORT_SPEED_10M;
            break;
        case 1:
            *pSpeed = PORT_SPEED_100M;
            break;
        case 2:
            *pSpeed = PORT_SPEED_1000M;
            break;
        case 3:
            switch (speed_h)
            {
                case 0:
                    *pSpeed = PORT_SPEED_10G;
                    break;
                case 7:
                    *pSpeed = PORT_SPEED_5G;
                    break;
                case 6:
                    *pSpeed = PORT_SPEED_2500M;
                    break;
                default:
                    *pSpeed = PORT_SPEED_10M;
                    break;
            }
            break;
        default:
            *pSpeed = PORT_SPEED_10M;
            break;
    }

    return ret;
}

/* Function Name:
 *      rtl8373_phy_common_c45_speed_set
 * Description:
 *      Set speed of the specific port
 * Input:
 *      port          - port id
 *      speed         - link speed rtk_port_speed_t
 * Output:
 *      None
 * Return:
 *      0                 - OK
 *      -1             - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_speed_set(struct realtek_smi *priv, int port, rtk_port_speed_t speed)
{
    int   ret;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv,  port, PHY_MMD_PMAPMD, 0, &phyData)) != 0)
        return ret;

    phyData &= (~(BIT_13 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2));

    switch (speed)
    {
        case PORT_SPEED_10M:
            break;
        case PORT_SPEED_100M:
            phyData |= BIT_13;
            break;
        case PORT_SPEED_1000M:
            phyData |= BIT_6;
            break;
        case PORT_SPEED_10G:
            phyData |= (BIT_13 | BIT_6);
            break;
        case PORT_SPEED_2500M:
            phyData |= (BIT_13 | BIT_6 | BIT_4 | BIT_3);
            break;
        case PORT_SPEED_5G:
            phyData |= (BIT_13 | BIT_6 | BIT_4 | BIT_3 | BIT_2);
            break;
        default:
            return RT_ERR_CHIP_NOT_SUPPORTED;
    }

    ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_PMAPMD, 0, phyData);
    return ret;
}

/* Function Name:
 *      rtl8373_phy_common_c45_enable_set
 * Description:
 *      Set PHY interface state enable/disable of the specific port
 * Input:
 *      port          - port id
 *      enable        - admin configuration of PHY interface
 * Output:
 *      None
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_enable_set(struct realtek_smi *priv, int port, rtk_enable_t enable)
{
    int   ret;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_PMAPMD, 0, &phyData)) != 0)
        return ret;

    phyData &= (~BIT_11);
    phyData |= (enable == RTK_ENABLED) ? (0) : (BIT_11);

    ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_PMAPMD, 0, phyData);
    return ret;
}

/* Function Name:
 *      rtl8373_phy_common_c45_enable_get
 * Description:
 *      Get PHY interface state enable/disable of the specific port
 * Input:
 *      port          - port id
 * Output:
 *      pEnable       - pointer to admin configuration of PHY interface
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_enable_get(struct realtek_smi *priv, int port, rtk_enable_t *pEnable)
{
    int   ret;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_PMAPMD, 0, &phyData)) != 0)
        return ret;

    *pEnable = (phyData & BIT_11) ? (RTK_DISABLED) : (RTK_ENABLED);
    return ret;
}
/* Function Name:
 *      rtl8373_phy_common_c45_duplex_get
 * Description:
 *      Get duplex of the specific port
 * Input:
 *      unit - unit id
 *      port - port id
 * Output:
 *      pDuplex - pointer to PHY duplex mode status
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_duplex_get(struct realtek_smi *priv, int port, rtk_port_duplex_t *pDuplex)
{
    int  ret = 0;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv, (u32)port, PHY_MMD_VEND2, 0xA400, &phyData)) != 0)
        return ret;

    *pDuplex = (phyData & BIT_8) ? PORT_FULL_DUPLEX : PORT_HALF_DUPLEX;

    return ret;
}

/* Function Name:
 *      rtl8373_phy_common_c45_duplex_set
 * Description:
 *      Set duplex of the specific port
 * Input:
 *      unit          - unit id
 *      port          - port id
 *      duplex        - duplex mode of the port, full or half
 * Output:
 *      None
 * Return:
 *      0     - OK
 *      -1 - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_duplex_set(struct realtek_smi *priv, int port, rtk_port_duplex_t duplex)
{
    int  ret = 0;
    u32 phyData = 0;
	//printk("@@@@ %s: enter\n", __func__);

    if ((ret = rtl8373_phy_read_mmd(priv, (u32)port, PHY_MMD_VEND2, 0xA400, &phyData)) != 0)
        return ret;

    phyData = (duplex == PORT_FULL_DUPLEX) ? (phyData | BIT_8) : (phyData & ~BIT_8);

    ret = rtl8373_phy_write_mmd(priv, port, PHY_MMD_VEND2, 0xA400, phyData);
    return ret;
}


/* Function Name:
 *      rtl8373_phy_common_c45_speedDuplexStatusResReg_get
 * Description:
 *      Get operational link speed-duplex status from proprietary register
 * Input:
 *      port    - port id
 * Output:
 *      pSpeed - pointer to PHY operational link speed
 *      pDuplex - pointer to PHY operational Duplex
 * Return:
 *      0     - OK
 *      -1 - Failed
 * Note:
 *      Only PHY that supports proprietary resolution register can use this driver!
 */
int rtl8373_phy_common_c45_speedDuplexStatusResReg_get(struct realtek_smi *priv, int port, rtk_port_speed_t *pSpeed, rtk_port_duplex_t *pDuplex)
{
    int ret = -1;
    u32 phyData, spd;
	//printk("@@@@ %s: enter\n", __func__);

    *pSpeed = PORT_SPEED_10M;
    *pDuplex = PORT_HALF_DUPLEX;
    if ((ret = rtl8373_phy_read_mmd(priv, port, PHY_MMD_VEND2, 0xA434, &phyData)) != 0) {
        return ret;
    }

    /* [10:9,5:4] */
    spd = ((phyData & (0x3UL << 9)) >> (9 - 2)) | ((phyData & (0x3UL << 4)) >> 4);
    switch (spd)
    {
        case 0x0:
          *pSpeed = PORT_SPEED_10M;
          break;
        case 0x1:
          *pSpeed = PORT_SPEED_100M;
          break;
        case 0x2:
          *pSpeed = PORT_SPEED_1000M;
          break;
        case 0x3:
          *pSpeed = PORT_SPEED_500M;
          break;
        case 0x4:
          *pSpeed = PORT_SPEED_10G;
          break;
        case 0x5:
          *pSpeed = PORT_SPEED_2500M;
          break;
        case 0x6:
          *pSpeed = PORT_SPEED_5G;
          break;        
        default:
          *pSpeed = PORT_SPEED_10M;
          break;
    }

    *pDuplex = (phyData & 0x8)? PORT_FULL_DUPLEX : PORT_HALF_DUPLEX;
    return 0;
}

static int rtl8373_phy_suspend(struct phy_device *phydev)
{
	//printk("@@@@ %s: enter\n", __func__);
	return genphy_suspend(phydev);
}

static int rtl8373_phy_resume(struct phy_device *phydev)
{
	//printk("@@@@ %s: enter\n", __func__);
	return genphy_resume(phydev);
}

static int rtl8373_phy_config_aneg(struct phy_device *phydev)
{
	//printk("@@@@ %s: enter\n", __func__);
	//rtl8373_phy_common_c45_autoNegoEnable_set
	//rtl8373_phy_autoNegoAbility_set
	return 0;
}

static int rtl8373_portlink_get(struct realtek_smi *smi, rtk_port_t port, u32* link)
{
    u32 regData = 0;
    rtk_api_ret_t retVal;

    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_LINK_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_LINK_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *link = (regData >> port) & 1;
    
    return RT_ERR_OK;
}

static int rtl8373_phy_read_status(struct phy_device *phydev)
{
	struct realtek_smi *smi = phydev->mdio.bus->priv; 
	rtk_port_speed_t rtk_speed;
	rtk_port_duplex_t rtk_duplex;
	int pport = phydev->mdio.addr;
	int lport;
	int ret;
	u32 link;

 	lport = smi->ops->port_p2l_get(smi, pport);
	if (lport < 0)
		return -EINVAL;

	ret  = rtl8373_portlink_get(smi, lport, &link);
	 if(link == 1)
		 phydev->link= true;
	 else
	 {
        phydev->speed = SPEED_UNKNOWN;
        phydev->duplex = DUPLEX_UNKNOWN;
        phydev->link = false;
        return 0;
     }

 	ret = rtl8373_phy_common_c45_speedDuplexStatusResReg_get(smi, lport, &rtk_speed, &rtk_duplex);
	
	if (ret == 0) {
 		switch (rtk_speed) {
		case PORT_SPEED_2500M:
		case PORT_SPEED_2PAIR_2500M:
			phydev->speed = SPEED_2500;
			break;
		case PORT_SPEED_1000M:
		case PORT_SPEED_2PAIR_1G:
			phydev->speed = SPEED_1000;
			break;
		case PORT_SPEED_100M:
			phydev->speed = SPEED_100;
			break;
		case PORT_SPEED_10M:
			phydev->speed = SPEED_10;
			break;
		case PORT_SPEED_5G:
		case PORT_SPEED_2PAIR_5G:
			phydev->speed = SPEED_5000;
			break;
		case PORT_SPEED_10G:
			phydev->speed = SPEED_10000;
			break;
		default:
			phydev->speed = SPEED_UNKNOWN;
			break;
		}

 		if (rtk_duplex == PORT_FULL_DUPLEX)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;
  

 		phydev->pause = 1;
		phydev->asym_pause = 1;

	} else {  
		phydev->speed = SPEED_UNKNOWN;
		phydev->duplex = DUPLEX_UNKNOWN;
	}
  
	return 0; 
}

struct phy_driver rtl8373_phy_driver[] = {
    {
        PHY_ID_MATCH_EXACT(0x001ccad0),
        .name           = "RTL8373  Ethernet",
        .flags          = PHY_IS_INTERNAL,
        .suspend 		= rtl8373_phy_suspend,
		.resume 		= rtl8373_phy_resume,
		.config_aneg 	= rtl8373_phy_config_aneg,
		.read_status 	= rtl8373_phy_read_status,
        .config_init    = rtl8373_phy_config_init,
        .probe          = rtl8373_phy_probe,
        .get_features   = rtl8372_phy_get_features,
    }
};

static struct mdio_device_id __maybe_unused rtl8373_tbl[] = {
    { PHY_ID_MATCH_EXACT(0x001ccad0) },
    { }
};
MODULE_DEVICE_TABLE(mdio, rtl8373_tbl);

module_phy_driver(rtl8373_phy_driver);

MODULE_DESCRIPTION("Realtek RTL8373N/RTL8372N PHY Ethernet PHY driver");
MODULE_LICENSE("GPL");
