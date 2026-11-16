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
#include "rtl8373_port.h"
#ifndef  SUCCESS  
#define SUCCESS TRUE
#endif
#ifndef FAILURE
#define FAILURE     FALSE
#endif

static rtk_port_t port3_PHYAD = 0;
static rtk_port_t port8_PHYAD = 0;
u16 rtl8373_rtct_patch[][2]=
{
    {0x81A3,0x2E},
    {0x81A4,0xE0},
    {0x81A5,0x2E},
    {0x81A6,0xE0},
    {0x81A8,0x1D},
    {0x81A9,0x00},
    {0x81AF,0x2D},
    {0x81B0,0x05},
    {0x81B2,0x09},
    {0x81B3,0x1F},
    {0x81BC,0x1D},
    {0x81BD,0x00},
    {0x81BE,0x00},
    {0x81BF,0xEA},
    {0x81C0,0x03},
    {0x81C1,0xCA},
    {0x81C2,0x1D},
    {0x81C3,0x00},
    {0x81C4,0x00},
    {0x81C5,0x3B},
    {0x81C6,0x00},
    {0x81C7,0x1A},
    {0x81C8,0x00},
    {0x81C9,0x54},
    {0x81CA,0xFF},
    {0x81CB,0xD5},
    {0x81CC,0x07},
    {0x81CD,0xFA},
    {0x81CE,0xFF},
    {0x81CF,0x14},
    {0x81D0,0x00},
    {0x81D1,0x74},
    {0x81D2,0xFF},
    {0x81D3,0xDB},
    {0x81D4,0x09},
    {0x81D5,0xF0},
    {0x81D6,0xFF},
    {0x81D7,0x97},
    {0x81D8,0xFF},
    {0x81D9,0xC2},
    {0x81DA,0x00},
    {0x81DB,0x28},
    {0x81DC,0xF1},
    {0x81DD,0xA0},
    {0x81DE,0x00},
    {0x81DF,0x94},
    {0x81E0,0x00},
    {0x81E1,0x76},
    {0x81E2,0xFF},
    {0x81E3,0xB8},
    {0x81E4,0xEA},
    {0x81E5,0xDD},
    {0x81E6,0x01},
    {0x81E7,0x6B},
    {0x81E8,0xFF},
    {0x81E9,0xAA},
    {0x81EA,0x00},
    {0x81EB,0x06},
    {0x81EC,0x05},
    {0x81ED,0x0A},
    {0x81EE,0xFF},
    {0x81EF,0x9D},
    {0x81F0,0x00},
    {0x81F1,0x10},
    {0x81F2,0x00},
    {0x81F3,0x05},
    {0x81F4,0x01},
    {0x81F5,0xE0},
    {0x81F6,0x00},
    {0x81F7,0x00},
    {0x81F8,0x00},
    {0x81F9,0x00},
    {0x81FA,0x00},
    {0x81FB,0x00},
};

u32 RTCT_START = 0;


rtk_port_t get_port3_PHYAD(void)
{
    return port3_PHYAD;
}


void set_port3_PHYAD(rtk_port_t val)
{
    port3_PHYAD = val;
}


rtk_port_t get_port8_PHYAD(void)
{
    return port8_PHYAD;
}


void set_port8_PHYAD(rtk_port_t val)
{
    port8_PHYAD = val;
}

static ret_t _rtl8373_portlink_get(struct realtek_smi *smi, rtk_port_t port, u32* link)
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


/* Function Name:
 *      dal_rtl8373_portFrcAblitiy_set
 * Description:
 *      Set port force ability
 * Input:
 *      port     - 0 ~ 9
 *      ability    - link, speed, duplex, rxpause, txpause
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */

ret_t dal_rtl8373_portFrcAbility_set(struct realtek_smi *smi, rtk_port_t port, rtk_port_ability_t* ability)
{
    u32 regData = 0;
    u32 tmp;
    rtk_api_ret_t retVal;
    if((ability->forcemode>=RTK_ENABLE_END)||(ability->link>=PORT_LINKSTATUS_END)||(ability->duplex>=PORT_DUPLEX_END)||
        (ability->speed >=PORT_SPEED_END)||(ability->txpause>=RTK_ENABLE_END)||(ability->rxpause>=RTK_ENABLE_END)||
        (ability->smi_force_fc>=RTK_ENABLE_END)||(ability->media>=PORT_MEDIA_END))
        return RT_ERR_INPUT;
  
    port = smi->ops->port_l2p_get(smi, port);
    /*if port now is link up,  force linkdown first, then set other ability*/
    _rtl8373_portlink_get(smi, port, &tmp);
    if(tmp == 1)
    {
        retVal = rtl8373_setAsicRegBit(smi->map, RTL8373_MAC_FORCE_MODE_CTRL0_ADDR(port), RTL8373_MAC_FORCE_MODE_CTRL0_FORCE_LINK_EN_OFFSET, 0);
        if(retVal != RT_ERR_OK)
            return retVal;

        retVal = rtl8373_setAsicRegBit(smi->map, RTL8373_MAC_FORCE_MODE_CTRL0_ADDR(port), RTL8373_MAC_FORCE_MODE_CTRL0_MAC_FORCE_EN_OFFSET, 1);
        if(retVal != RT_ERR_OK)
            return retVal;
    }

    regData = ability->forcemode & 1;
    regData |= (ability->link << 1);
    regData |= (ability->duplex << 2);
    regData |= ((ability->speed & 0xf) << 3);
    regData |= (ability->txpause << 7);
    regData |= (ability->rxpause << 8);
    regData |= (ability->smi_force_fc << 9);
    regData |= (ability->media << 10);

    return rtl8373_setAsicRegBits(smi->map, RTL8373_MAC_FORCE_MODE_CTRL0_ADDR(port), 0x7ff, regData);
}

/* Function Name:
 *      dal_rtl8373_portFrcAblitiy_get
 * Description:
 *      Get port force ability
 * Input:
 *      port     - 0 ~ 9
 *      ability    - link, speed, duplex, rxpause, txpause
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portFrcAbility_get(struct realtek_smi *smi, rtk_port_t port, rtk_port_ability_t* ability)
{
    u32 regData = 0;
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_MAC_FORCE_MODE_CTRL0_ADDR(port), 0x7ff, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;
    
    ability->forcemode = regData & 1;
    ability->link = (regData >> 1) & 1;
    ability->duplex = (regData >> 2) & 1;
    ability->speed = (regData >> 3) & 0xf;
    ability->txpause = (regData >> 7) & 1;
    ability->rxpause = (regData >> 8) & 1;
	ability->smi_force_fc = (regData >> 9) & 1;
    ability->media = (regData >> 10) & 1;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_portStatus_get
 * Description:
 *      Get port status
 * Input:
 *      port     - 0 ~ 9
 *      ability    - link, speed, duplex, rxpause, txpause
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portStatus_get(struct realtek_smi *smi, rtk_port_t port, rtk_port_status_t* status)
{
    u32 regData = 0;
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    /*link status*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_LINK_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_LINK_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->link = (regData >> port) & 1;

    /*speed status*/
    retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_MAC_LINK_SPD_STS_ADDR(port), RTL8373_MAC_LINK_SPD_STS_SPD_STS_9_0_MASK(port), &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->speed= regData;

    /*duplex status*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_LINK_DUP_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->duplex= (regData >> port) & 1;

    /*tx pause status*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_TX_PAUSE_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->txpause= (regData >> port) & 1;

    /*rx pause status*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_RX_PAUSE_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->rxpause= (regData >> port) & 1;

    /*media status*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_LINK_MEDIA_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->media= (regData >> port) & 1;

    /*eee status*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_EEE_ABLTY_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->eee= (regData >> port) & 1;

    /*master status*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_MSTR_SLV_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->master= (regData >> port) & 1;

    /*master slave nway*/
    retVal = rtl8373_getAsicReg(smi->map, RTL8373_MAC_MSTR_SLV_FAULT_STS_ADDR, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    status->master_slave= (regData >> port) & 1;


    return RT_ERR_OK;  
}

/* Function Name:
 *      dal_rtl8373_portMaxLen_set
 * Description:
 *      Set port rx max length
 * Input:
 *      port     - 0 ~ 9
 *      type    - 0 FE:10M,100M,   1 giga:1G, 2.5G, 5G, 10G
 *      len      - max length
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portMaxLen_set(struct realtek_smi *smi, rtk_port_t port, u32 type, u32 len)
{
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    if(type == 0)
    {
        retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_100M_10M_SEL_MASK, len);
        if(retVal != RT_ERR_OK)
            return retVal;
    }
    else if(type == 1)
    {
        retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_1G_2P5G_5G_10G_SEL_MASK, len);
        if(retVal != RT_ERR_OK)
            return retVal;
    }
    else 
        return RT_ERR_INPUT;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_portMaxLen_get
 * Description:
 *      Get port rx max length
 * Input:
 *      port     - 0 ~ 9
 *      type    - 0 FE:10M,100M,   1 giga:1G, 2.5G, 5G, 10G
 *      pLen    - max length
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portMaxLen_get(struct realtek_smi *smi, rtk_port_t port, u32 type, u32* pLen)
{
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    if(type == 0)
    {
        retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_100M_10M_SEL_MASK, pLen);
        if(retVal != RT_ERR_OK)
            return retVal;
    }
    else if(type == 1)
    {
        retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_1G_2P5G_5G_10G_SEL_MASK, pLen);
        if(retVal != RT_ERR_OK)
            return retVal;
    }
    else 
        return RT_ERR_INPUT;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_portMaxLenIncTag_set
 * Description:
 *      Set port rx max length include tag length: cputag ctag, stag
 * Input:
 *      port     - 0 ~ 9
 *      enaleb  - 1: include tag length,   0: not include tag length
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portMaxLenIncTag_set(struct realtek_smi *smi, rtk_port_t port, u32 enable)
{
    port = smi->ops->port_l2p_get(smi, port);
    return rtl8373_setAsicRegBit(smi->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_TAG_INC_OFFSET, enable);
}

/* Function Name:
 *      dal_rtl8373_portMaxLenIncTag_get
 * Description:
 *      Get port rx max length include tag length: cputag ctag, stag
 * Input:
 *      port     - 0 ~ 9
 *      enaleb  - 1: include tag length,   0: not include tag length
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portMaxLenIncTag_get(struct realtek_smi *smi, rtk_port_t port, u32* pEnable)
{
    port = smi->ops->port_l2p_get(smi, port);
    return rtl8373_getAsicRegBit(smi->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_TAG_INC_OFFSET, pEnable);
}

/* Function Name:
 *      dal_rtl8373_portLoopbackEn_set
 * Description:
 *      Set port mac tx loopback to rx
 * Input:
 *      port     - 0 ~ 9
 *      enable    - 0 disable loopback,   1 enable loopback
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portLoopbackEn_set(struct realtek_smi *smi, rtk_port_t port, rtk_enable_t enable)
{
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    retVal = rtl8373_setAsicRegBit(smi->map, RTL8373_MAC_L2_PORT_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_CTRL_CFG_PORT_L_LPBK_OFFSET, enable);
    if(retVal != RT_ERR_OK)
        return retVal;
 
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_portLoopbackEn_get
 * Description:
 *      Get port mac tx loopback to rx
 * Input:
 *      port     - 0 ~ 9
 *      pEnable    - 0 disable loopback,   1 enable loopback
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portLoopbackEn_get(struct realtek_smi *smi, rtk_port_t port, rtk_enable_t * pEnable)
{
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    retVal = rtl8373_getAsicRegBit(smi->map, RTL8373_MAC_L2_PORT_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_CTRL_CFG_PORT_L_LPBK_OFFSET, pEnable);
    if(retVal != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_portBackpressureEn_set
 * Description:
 *      Set port half back pressure enable
 * Input:
 *      port     - 0 ~ 9
 *      enable    - 0 disable backpressure,   1 enable backpressure
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portBackpressureEn_set(struct realtek_smi *smi, rtk_port_t port, rtk_enable_t enable)
{
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    retVal = rtl8373_setAsicRegBit(smi->map, RTL8373_MAC_PORT_CTRL_ADDR(port), RTL8373_MAC_PORT_CTRL_BKPRES_EN_OFFSET, enable);
    if(retVal != RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_portBackpressureEn_get
 * Description:
 *      Get port half back pressure enable
 * Input:
 *      port     - 0 ~ 9
 *      pEnable    - 0 disable backpressure,   1 enable backpressure
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
ret_t dal_rtl8373_portBackpressureEn_get(struct realtek_smi *smi, rtk_port_t port, rtk_enable_t * pEnable)
{
    rtk_api_ret_t retVal;

    port = smi->ops->port_l2p_get(smi, port);
    retVal = rtl8373_getAsicRegBit(smi->map, RTL8373_MAC_PORT_CTRL_ADDR(port), RTL8373_MAC_PORT_CTRL_BKPRES_EN_OFFSET, pEnable);
    if(retVal != RT_ERR_OK)
        return retVal;
            
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_port_extphyid_set
 * Description:
 *      Set   smi address ,SMI Address, default value equals to port number
 * Input:
 *      sdsid     - 0,1
 *      phyid   - get from phy strap pin
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */

ret_t dal_rtl8373_port_extphyid_set(struct realtek_smi *smi, u32 sdsid,  u32 phyid)
{
    rtk_api_ret_t retVal;
    
    if(sdsid == SERDES_ID0)
    {
        retVal =rtl8373_setAsicRegBits(smi->map, RTL8373_SMI_MAC_TYPE_CTRL_ADDR, RTL8373_SMI_MAC_TYPE_CTRL_MAC_PORT3_TYPE_MASK, 1);
        if(retVal != RT_ERR_OK)
            return retVal;
        retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_SMI_PORT0_5_ADDR_CTRL_ADDR, RTL8373_SMI_PORT0_5_ADDR_CTRL_PORT3_ADDR_MASK, phyid);
        if(retVal != RT_ERR_OK)
            return retVal;
  
        set_port3_PHYAD(phyid);
        
    }
    else if(sdsid == SERDES_ID1)
    {
        retVal =rtl8373_setAsicRegBits(smi->map, RTL8373_SMI_MAC_TYPE_CTRL_ADDR, RTL8373_SMI_MAC_TYPE_CTRL_MAC_PORT8_TYPE_MASK, 1);
        if(retVal != RT_ERR_OK)
            return retVal;
        retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_SMI_PORT6_9_ADDR_CTRL_ADDR, RTL8373_SMI_PORT6_9_ADDR_CTRL_PORT8_ADDR_MASK, phyid);
        if(retVal != RT_ERR_OK)
            return retVal;
       set_port8_PHYAD(phyid);
    }
    else 
        return RT_ERR_INPUT;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_port_extphyid_get
 * Description:
 *      Get   smi address ,SMI Address, default value equals to port number
 * Input:
 *      sdsid     - 0,1
 *      phyid   - get smi address
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */

ret_t dal_rtl8373_port_extphyid_get(struct realtek_smi *smi, u32 sdsid,  u32 *phyid)
{
    rtk_api_ret_t retVal;

    if(phyid == NULL)
        return RT_ERR_INPUT;
    
    if(sdsid == SERDES_ID0)
    {

        retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_SMI_PORT0_5_ADDR_CTRL_ADDR, RTL8373_SMI_PORT0_5_ADDR_CTRL_PORT3_ADDR_MASK, phyid);
        if(retVal != RT_ERR_OK)
            return retVal;

    }
    else if(sdsid == SERDES_ID1)
    {

        retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_SMI_PORT6_9_ADDR_CTRL_ADDR, RTL8373_SMI_PORT6_9_ADDR_CTRL_PORT8_ADDR_MASK, phyid);
        if(retVal != RT_ERR_OK)
            return retVal;
    }
    else 
        return RT_ERR_INPUT;
    
    return RT_ERR_OK;
}
/* Function Name:
 *      dal_rtl8373_fiber_fc_set
 * Description:
 *      Configure serdes port Nway state
 * Input:
 *      sdsId        - serdes ID
 *      sdsMode      - serdes mode
 *      fc_en       - 1:enable fc, 0:disable fc
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK                   - OK
 *      RT_ERR_FAILED               - Failed
 *      RT_ERR_SMI                  - SMI access error
 *      RT_ERR_PORT_ID              - Invalid port ID.
 * Note:
 *      The API configure SERDES port Nway state
 */
rtk_api_ret_t dal_rtl8373_fiber_fc_set(struct realtek_smi *smi, u32 sdsId, rtk_sds_mode_t sdsMode, rtk_enable_t fc_en)
{
    if((sdsMode==SERDES_100FX)||(sdsMode==SERDES_1000BASEX)||(sdsMode==SERDES_2500BASEX)
        ||(sdsMode==SERDES_10GR))
    {
        switch(sdsMode)
        {
            case SERDES_100FX:
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<2, 0x1);
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<3, 0x1);
                if(fc_en)
                {
                        //rtl8371b_sds_reg_setbits(SDS_INDX, 2, 4, 11, 10, 0x3);
                        dal_rtl8373_sds_regbits_write(smi, sdsId, 2, 4, 0x3<<10, 0x3);
                }
                else
                {
                        //rtl8371b_sds_reg_setbits(SDS_INDX, 2, 4, 11, 10, 0x0); 
                        dal_rtl8373_sds_regbits_write(smi, sdsId, 2, 4, 0x3<<10, 0x0);
                }
                break;
            case SERDES_1000BASEX:
            case SERDES_2500BASEX:
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<2, 0x1);
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<3, 0x0);
                if(fc_en)
                {
                        //rtl8371b_sds_reg_setbits(SDS_INDX, 2, 4, 8, 7, 0x3);
                        dal_rtl8373_sds_regbits_write(smi, sdsId, 2, 4, 0x3<<7, 0x3);
                }
                else
                {
                        //rtl8371b_sds_reg_setbits(SDS_INDX, 2, 4, 8, 7, 0x0);	
                        dal_rtl8373_sds_regbits_write(smi, sdsId, 2, 4, 0x3<<7, 0x0);
                }
                break;  
            case SERDES_10GR: 
                if(fc_en)
                {
                        //rtl8371b_sds_reg_setbits(SDS_INDX, 31, 11, 3, 2, 0x3);
                        dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 11, 0x3<<2, 0x3);
                }
                else
                {
                        //rtl8371b_sds_reg_setbits(SDS_INDX, 31, 11, 3, 2, 0x0);  
                        dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 11, 0x3<<2, 0x0);
                }
                break; 	 
            default:
                break;  
        }		 
    }
    else
    {
        return RT_ERR_INPUT;
    }

	return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_fiber_fc_get
 * Description:
 *      Configure serdes port Nway state
 * Input:
 *      sdsId        - serdes ID
 *      sdsMode      - serdes mode
 *      fc_en       - 1:enable FC, 0:disable FC
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK                   - OK
 *      RT_ERR_FAILED               - Failed
 *      RT_ERR_SMI                  - SMI access error
 *      RT_ERR_PORT_ID              - Invalid port ID.
 * Note:
 *      The API configure SERDES port Nway state
 */
rtk_api_ret_t dal_rtl8373_fiber_fc_get(struct realtek_smi *smi, u32 sdsId, rtk_sds_mode_t sdsMode, rtk_enable_t *fc_en)
{
    u32 value=0;
    if((sdsMode==SERDES_100FX)||(sdsMode==SERDES_1000BASEX)||(sdsMode==SERDES_2500BASEX)
        ||(sdsMode==SERDES_10GR))
    {
        switch(sdsMode)
        {
            case SERDES_100FX:
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<2, 0x1);
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<3, 0x1);
                dal_rtl8373_sds_regbits_read(smi, sdsId, 2, 4, 0x3<<10, &value);
                if(value==0)
                     *fc_en=DISABLED;
                else
                    *fc_en=ENABLED;
                break;
            case SERDES_1000BASEX:
            case SERDES_2500BASEX:
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<2, 0x1);
                dal_rtl8373_sds_regbits_write(smi, sdsId, 31, 5, 0x1<<3, 0x0);
                dal_rtl8373_sds_regbits_read(smi, sdsId, 2, 4, 0x3<<7, &value);
                if(value==0)
                     *fc_en=DISABLED;
                else
                    *fc_en=ENABLED;
                break;  

            case SERDES_10GR:  
                dal_rtl8373_sds_regbits_read(smi, sdsId, 31, 11, 0x3<<2, &value);               
                if(value==0)
                     *fc_en=DISABLED;
                else
                    *fc_en=ENABLED;
                break; 	 
            default:
                break;  
        }		 
    }
    else
    {
        return RT_ERR_INPUT;
    }

	return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_port_sdsNway_set
 * Description:
 *      Configure serdes port Nway state
 * Input:
 *      sdsId        - serdes ID
 *      sdsMode      - serdes mode
 *      enable       - 1:enable nway, 0:disable nway
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK                   - OK
 *      RT_ERR_FAILED               - Failed
 *      RT_ERR_SMI                  - SMI access error
 *      RT_ERR_PORT_ID              - Invalid port ID.
 * Note:
 *      The API configure SERDES port Nway state
 */
rtk_api_ret_t dal_rtl8373_port_sdsNway_set(struct realtek_smi *smi, u32 sdsId, rtk_sds_mode_t sdsMode, rtk_enable_t enable)
{
    switch(sdsMode)
	{
		case SERDES_100FX:
		case SERDES_10GR:			 
			 break;
		case SERDES_1000BASEX:
		case SERDES_2500BASEX:
		case SERDES_SG:
		case SERDES_HSG:
			if(enable)
			{
				dal_rtl8373_sds_regbits_write(smi, sdsId, 0, 2, 0x3<<8, 0x3);
				dal_rtl8373_sds_regbits_write(smi, sdsId, 0, 4, 0x3<<1, 0x3); //force enable
			}
			else
			{
				dal_rtl8373_sds_regbits_write(smi, sdsId, 0, 2, 0x3<<8, 0x1);
				dal_rtl8373_sds_regbits_write(smi, sdsId, 0, 4, 0x3<<1, 0x3);
			}
			break;  
		case SERDES_10GUSXG:
		case SERDES_10GQXG:
			if(enable)
			{
				dal_rtl8373_sds_regbits_write(smi, sdsId, 7, 17, 0xf<<0, 0xf);
			}
			else
			{

				dal_rtl8373_sds_regbits_write(smi, sdsId, 7, 17, 0xf<<0, 0x0);
			}
			break;  
		default:
		 	return RT_ERR_INPUT;
			break;  
	 }	
	return RT_ERR_OK;
}
/* Function Name:
 *      dal_rtl8373_port_sdsNway_get
 * Description:
 *      Get serdes Nway
 * Input:
 *      sdsid        - serdes ID
 *      pState       - Nway state
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK                   - OK
 *      RT_ERR_FAILED               - Failed
 *      RT_ERR_SMI                  - SMI access error
 *      RT_ERR_PORT_ID              - Invalid port ID.
 * Note:
 *      The API configure serdes port Nway state
 */
rtk_api_ret_t dal_rtl8373_port_sdsNway_get(struct realtek_smi *smi, u32 sdsId, rtk_sds_mode_t sdsMode, rtk_enable_t *pState)
{
    u32 regData, regData1;
    
    switch(sdsMode)
    {
    	case SERDES_100FX:
    	case SERDES_10GR:			 
    		 break;
    	case SERDES_1000BASEX:
    	case SERDES_2500BASEX:
    	        dal_rtl8373_sds_regbits_read(smi, sdsId, 0, 2, 0x3<<8, &regData);
    	        if(regData == 0x3) //bit8=1
    	        {    	           
                    *pState = ENABLED;                  
    	        }
    	        else if(regData == 0x1) //bit8=1
    	        {    	           
                    *pState = DISABLED;                  
    	        }
    	        else if((regData == 0x0) ||(regData == 0x2)) //bit8=0
    	        {
    	            dal_rtl8373_sds_regbits_read(smi, sdsId, 2, 0, 0x1<<12, &regData1);
    	            if(regData1 == 0x1)
    			*pState = ENABLED;
                   else if(regData1 == 0x0)
                   	*pState = DISABLED;
                   else
                   	return RT_ERR_FAILED;
    	        }
    	        else
                   	return RT_ERR_FAILED;

    	        break;
    	case SERDES_SG:
    	case SERDES_HSG:
    		dal_rtl8373_sds_regbits_read(smi, sdsId, 0, 2, 0x3<<8, &regData);
    	        if(regData == 0x3) //bit8=1
    	        {    	           
                    *pState = ENABLED;                  
    	        }
    	        else if(regData == 0x1) //bit8=1
    	        {    	           
                    *pState = DISABLED;                  
    	        }
    	        else if((regData == 0x0) ||(regData == 0x2))//bit8=0
    	        {
    			*pState = ENABLED;                   
    	        }
    	        else
                   	return RT_ERR_FAILED;
    		break;  
    	case SERDES_10GUSXG:
    	case SERDES_10GQXG:
    		dal_rtl8373_sds_regbits_read(smi, sdsId, 7, 17, 0xf<<0, &regData);
    		if(regData == 0xf)
    			*pState = ENABLED;
    		else if(regData == 0)
    			*pState = DISABLED;
    		else
    			return RT_ERR_FAILED;
    		break;  
    	default:
    	 	return RT_ERR_INPUT;
    		break;  
     }	
	return RT_ERR_OK;
}
 

 void cfg_rl6637_sds_mode(struct realtek_smi *smi, u32 phyId,u32 sds_mode)
 {
 
	 rtl8373_phy_regbits_write(smi, 1<<phyId, 30, 0x75f3, 1, 0);
	 rtl8373_phy_regbits_write(smi, 1<<phyId, 30, 0x697a, 0x3f, sds_mode);

	 rtl8373_phy_regbits_write(smi, 1<<phyId, 31, 0xa442, 3<<2, 0);
	 rtl8373_phy_write(smi, 1<<phyId, 30, 0x75b5, 0xe084);
 
	 rtl8373_phy_regbits_write(smi, 1<<phyId, 30, 0x75b2, 3<<5, 3);
 
	 rtl8373_phy_regbits_write(smi, 1<<phyId, 31, 0xd040, 3<<8, 2);
 
	 rtl8373_phy_regbits_write(smi, 1<<phyId, 31, 0xa400, 1<<14, 1);
	 delay_0_1ms(4000);
	 rtl8373_phy_regbits_write(smi, 1<<phyId, 31, 0xa400, 1<<14, 0);
	 
 }
/* Function Name:
 *      dal_rtl8373_sdsMode_set
 * Description:
 *      Set sds mode
 * Input:
 *      sdsid     - 0 ~ 1
 *      mode  - 
 *      SERDES_10GQXG,
 *      SERDES_10GUSXG,
 *      SERDES_10GR,
 *      SERDES_HSG,
 *      SERDES_2500BASEX,
 *      SERDES_SG,
 *      SERDES_1000BASEX,
 *      SERDES_100FX,    
 *      SERDES_OFF, 
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_NOT_ALLOWED      - mode is not support
 * Note:
 *      None
 */

rtk_api_ret_t dal_rtl8373_sdsMode_set(struct realtek_smi *smi, u32 sdsid, rtk_sds_mode_t mode)
{
    rtk_api_ret_t retVal;
    u32 regdata, chiptype;

    if((retVal = rtl8373_getAsicReg(smi->map, 0x4, &regdata)) != RT_ERR_OK)
        return retVal;

    regdata = regdata >> 8;

    if (regdata == 0x837300 || regdata == 0x837348) //RTL8373 and 8373I
    {
        if((sdsid == 0) && (mode != SERDES_10GQXG) && (mode != SERDES_OFF))
            return RT_ERR_NOT_ALLOWED;
        chiptype = CHIP_RTL8373_MODE;
    }
    else if (regdata == 0x837200 || regdata == 0x837248 ) //RTL8372 and 8372I 
    {
        chiptype = CHIP_RTL8372_MODE;
    }  
	else if (regdata == 0x822400)
	{
        chiptype = CHIP_RTL8224_MODE;
    }
    else if (regdata == 0x837370 || regdata == 0x837372) //RTL8373N and 8373NI
    {
        chiptype = CHIP_RTL8373N_MODE;		 
    }
    else if (regdata == 0x837270 || regdata == 0x837272) //RTL8372N and 8372NI
    {
        chiptype = CHIP_RTL8372N_MODE;
    }
    else if (regdata == 0x822470 || regdata == 0x822472) //RTL8224N and 8224NI
    {
        chiptype = CHIP_RTL8224N_MODE;		 
    }
    else if (regdata == 0x8366A8)
    {
        chiptype = CHIP_RTL8366U_MODE;
    }    
    else 
        return RT_ERR_CHIP_NOT_FOUND;
 
    if(sdsid==SERDES_ID0)
    {
        if (mode == SERDES_8221B)
        {
            rtl8373_setAsicRegBits(smi->map, RTL8373_SMI_PORT0_5_ADDR_CTRL_ADDR, RTL8373_SMI_PORT0_5_ADDR_CTRL_PORT3_ADDR_MASK, get_port3_PHYAD());
            cfg_rl6637_sds_mode(smi, UTP_PORT3, 1);  //here add 8221B phy patch
            SDS_MODE_SET_SW(smi, chiptype, sdsid, SERDES_HSG);
            SDS_MODE_SET_SW(smi, chiptype, sdsid, SERDES_SG);
            
            //rtl8373_setAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR,RTL8373_SDS_MODE_SEL_SDS0_MODE_SEL_MASK,0x1F);
            rtl8373_setAsicRegBit(smi->map, RTL8373_SMI_PORT_POLLING_SEL_ADDR, RTL8373_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL3_OFFSET, ENABLED); // RTL8372 set port3  polling internal resolution reg

        }
        else if(mode == SERDES_OFF)
        {
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x3);
            delay_0_1ms(10);
            
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x1);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x3);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x1);
                        
            delay_0_1ms(100);
        }
        else if(mode == SERDES_ON)  //sds on
        {
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x0);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x3);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x1);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x0);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x0);
            delay_0_1ms(100);
        }
        else
        {
            rtl8373_setAsicRegBit(smi->map, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_CFG_MAC3_8221B_OFFSET, DISABLED);
            //rtl8373_setAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR,RTL8373_SDS_MODE_SEL_SDS0_MODE_SEL_MASK,0x1F);
            delay_0_1ms(1000);
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x3);
            delay_0_1ms(10);
            
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x1);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x3);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x1);
                        
            delay_0_1ms(100);
             SDS_MODE_SET_SW(smi, chiptype, sdsid, mode);         
        }
    }
    if(sdsid==SERDES_ID1)
    {
        if (mode == SERDES_8221B)
        {
            rtl8373_setAsicRegBits(smi->map, RTL8373_SMI_PORT6_9_ADDR_CTRL_ADDR, RTL8373_SMI_PORT6_9_ADDR_CTRL_PORT8_ADDR_MASK, get_port8_PHYAD());
            cfg_rl6637_sds_mode(smi, UTP_PORT8, 1);  //here add 8221B phy patch 
            SDS_MODE_SET_SW(smi, chiptype, sdsid, SERDES_HSG);
            SDS_MODE_SET_SW(smi, chiptype, sdsid, SERDES_SG);
        
            //rtl8373_setAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR,RTL8373_SDS_MODE_SEL_SDS1_MODE_SEL_MASK,0x1F);
            rtl8373_setAsicRegBit(smi->map, RTL8373_SMI_PORT_POLLING_SEL_ADDR, RTL8373_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL8_OFFSET, ENABLED); // RTL8372 set port8  polling internal resolution reg
        }
        else if(mode == SERDES_OFF)
        {
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x3);
            delay_0_1ms(10);
            
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x1);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x3);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x1);
                        
            delay_0_1ms(100);
        }
        else if(mode == SERDES_ON)  //sds on
        {
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x0);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x3);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x1);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x0);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x0);
            delay_0_1ms(100);
        }
        else
        {

            rtl8373_setAsicRegBit(smi->map, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_CFG_MAC8_8221B_OFFSET, DISABLED);
            //rtl8373_setAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR,RTL8373_SDS_MODE_SEL_SDS1_MODE_SEL_MASK,0x1F);
            delay_0_1ms(1000);
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x3);
            delay_0_1ms(10);
            
            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<4, 0x1);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x1);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<6, 0x3);
            delay_0_1ms(100);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x3);
            delay_0_1ms(10);

            dal_rtl8373_sds_regbits_write(smi, sdsid, 0x20, 0x00, 0x3<<10, 0x1);
            
            delay_0_1ms(100);
             SDS_MODE_SET_SW(smi, chiptype, sdsid, mode);         
        }
    }

    delay_0_1ms(50);
    if((mode == SERDES_10GQXG)||(mode == SERDES_10GR) || (mode == SERDES_10GUSXG))
        fw_reset_flow_tgr(smi, sdsid);
    else
        fw_reset_flow_tgx(smi, sdsid);

    delay_0_1ms(50);

    return RT_ERR_OK;

}



/* Function Name:
 *      dal_rtl8373_sdsMode_get
 * Description:
 *      Get sds mode
 * Input:
 *      sdsid     - 0 ~ 1
 *      pMode  - 
 *      SERDES_10GQXG,
 *      SERDES_10GUSXG,
 *      SERDES_10GR,
 *      SERDES_HSG,
 *      SERDES_2500BASEX,
 *      SERDES_SG,
 *      SERDES_1000BASEX,
 *      SERDES_100FX,    
 *      SERDES_OFF, 
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_NOT_ALLOWED      - mode is not support
 * Note:
 *      None
 */
rtk_api_ret_t dal_rtl8373_sdsMode_get(struct realtek_smi *smi, u32 sdsid, rtk_sds_mode_t * pMode)
{
    u32 regdata, regdata2, subtype;
    
    if(sdsid == 0)
    {
        rtl8373_getAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_SDS0_MODE_SEL_MASK,&regdata);
        rtl8373_getAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_SDS0_USX_SUB_MODE_MASK,&subtype);
        dal_rtl8373_sds_reg_read(smi, sdsid, 0x20, 0x00,&regdata2);
        
    }
    else if(sdsid == 1)
    {
        rtl8373_getAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_SDS1_MODE_SEL_MASK,&regdata);
        rtl8373_getAsicRegBits(smi->map, RTL8373_SDS_MODE_SEL_ADDR, RTL8373_SDS_MODE_SEL_SDS1_USX_SUB_MODE_MASK,&subtype);
        dal_rtl8373_sds_reg_read(smi, sdsid, 0x20, 0x00,&regdata2);
    }
    else 
        return RT_ERR_INPUT;
    
    if ((((regdata2 >> 4) & 0x3) == 0x1) && (((regdata2 >> 6) & 0x3) == 0x3) && (((regdata2 >> 10) & 0x3) == 0x1) )
    {
        *pMode = SERDES_OFF;
    }
    else
    {
        if(regdata == 0xD)
        {
            /*USXG*/
            if(subtype == 0)
                *pMode = SERDES_10GUSXG;
            else if (subtype == 2)
                *pMode = SERDES_10GQXG;
            else
                return RT_ERR_FAILED;
        }
        else if (regdata == 0x1A)
            *pMode = SERDES_10GR;    
        else if (regdata == 0x12)
                *pMode = SERDES_HSG;
        else if (regdata == 0x16)
                *pMode = SERDES_2500BASEX;
        else if (regdata == 0x2)
                *pMode = SERDES_SG;
        else if (regdata == 0x4)
                *pMode = SERDES_1000BASEX;
        else if (regdata == 0x5)
                *pMode = SERDES_100FX;
        else if (regdata == 0x1F)
                *pMode = SERDES_OFF;
    }
    return RT_ERR_OK;    
}

/* Function Name:
 *      dal_rtl8373_port_txPktMaxLen_set
 * Description:
 *      Set the max length of packet that tx port can transmit under difference speed
 * Input:
 *      port     - tx port number
 *      type     - 0 FE:10M,100M,   1 giga:1G, 2.5G, 5G, 10G (tx port work in different speed has different ability)
 *      pktLen   - max tx packet length
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
rtk_api_ret_t dal_rtl8373_port_txPktMaxLen_set(struct realtek_smi *smi,rtk_port_t port, u32 type, u32 pktLen)
{
    rtk_api_ret_t retVal;
    u32 bitmask = 0;   
    /* Check initialization state */
    

    /* Check pktLen */
    if (pktLen > RTL8373_PKTLENMAX)
        return RT_ERR_INPUT;  

    /* Check type */
    if (type > 1)
        return RT_ERR_INPUT;   

    if(type == 0)
    {
        bitmask = RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_MAX_LEN_TX_100M_10M_SEL_MASK;
    }
    else if(type == 1)
    {
        bitmask = RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_MAX_LEN_TX_1G_2P5G_5G_10G_SEL_MASK;
    }
    else 
        return RT_ERR_INPUT;

    port = smi->ops->port_l2p_get(smi, port);
    retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_ADDR(port), bitmask, pktLen);
    if(retVal != RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;
}

/* Function Name:
 *      dal_rtl8373_port_txPktMaxLen_get
 * Description:
 *      Get the max length of packet that tx port can transmit under specify speed
 * Input:
 *      port     - tx port number
 *      type     - 0 FE:10M,100M,   1 giga:1G, 2.5G, 5G, 10G (tx port work in different speed has different ability)
 * Output:
 *      pktLen   - max tx packet length
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 * Note:
 *      None
 */
rtk_api_ret_t dal_rtl8373_port_txPktMaxLen_get(struct realtek_smi *smi,rtk_port_t port, u32 type, u32 *pPktLen)
{
    rtk_api_ret_t retVal;
    u32 bitmask = 0;   
    /* Check initialization state */
    

    /* Check pointer */
    if(NULL == pPktLen)
        return RT_ERR_NULL_POINTER;

    /* Check type */
    if (type > 1)
        return RT_ERR_INPUT;      

    if(type == 0)
    {
        bitmask = RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_MAX_LEN_TX_100M_10M_SEL_MASK;
    }
    else if(type == 1)
    {
        bitmask = RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_MAX_LEN_TX_1G_2P5G_5G_10G_SEL_MASK;
    }
    else 
        return RT_ERR_INPUT;

    port = smi->ops->port_l2p_get(smi, port);
    retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_MAC_L2_PORT_TX_MAX_LEN_CTRL_ADDR(port), bitmask, pPktLen);
    if(retVal != RT_ERR_OK)
        return retVal;
    
    return RT_ERR_OK;   
}



