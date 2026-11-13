// SPDX-License-Identifier: GPL-2.0
#ifndef __RTL8373_PHY_H__
#define __RTL8373_PHY_H__

#ifndef PHY_MMD_PMAPMD
#define PHY_MMD_PMAPMD      1
#endif

#ifndef PHY_MMD_PCS
#define PHY_MMD_PCS         3
#endif

#ifndef PHY_MMD_AN
#define PHY_MMD_AN          7
#endif

#ifndef PHY_MMD_VEND1
#define PHY_MMD_VEND1       30   /* Vendor specific 1 */
#endif

#ifndef PHY_MMD_VEND2
#define PHY_MMD_VEND2       31   /* Vendor specific 2 */
#endif

#ifndef BIT_0
#define BIT_0       0x0001
#endif

#ifndef BIT_1
#define BIT_1       0x0002
#endif

#ifndef BIT_2
#define BIT_2       0x0004
#endif

#ifndef BIT_3
#define BIT_3       0x0008
#endif

#ifndef BIT_4
#define BIT_4       0x0010
#endif

#ifndef BIT_5
#define BIT_5       0x0020
#endif

#ifndef BIT_6
#define BIT_6       0x0040
#endif

#ifndef BIT_7
#define BIT_7       0x0080
#endif

#ifndef BIT_8
#define BIT_8       0x0100
#endif

#ifndef BIT_9
#define BIT_9       0x0200
#endif

#ifndef BIT_10
#define BIT_10      0x0400
#endif

#ifndef BIT_11
#define BIT_11      0x0800
#endif

#ifndef BIT_12
#define BIT_12      0x1000
#endif

#ifndef BIT_13
#define BIT_13      0x2000
#endif

#ifndef BIT_14
#define BIT_14      0x4000
#endif

#ifndef BIT_15
#define BIT_15      0x8000
#endif

#define PORT_NUM_IN_8224  (4)

#define RTL8224TOPDEVAD  (30)
#define RTL8224BASEID  (0x1)
#define RTL8224PHYID  (0x0)

typedef struct rtk_port_phy_ability_s
{
    u16 Half_10;
    u16 Full_10;
    u16 Half_100;
    u16 Full_100;
    u16 Full_1000;
    u16 adv_2_5G;
    u16 adv_5G;
    u16 adv_10GBase_T;
    u16 FC;
    u16 AsyFC;
} rtk_port_phy_ability_t;



/* Function Name:
*      rtl8373_phy_common_c45_an_restart
* Description:
*      Get ability advertisement for auto-negotiation of the specific port
* Input:
*      port - port id
* Output:
*      pAbility - pointer to PHY auto-negotiation ability
* Return:
*      RT_ERR_OK     - OK
*      RT_ERR_FAILED - invalid parameter
* Note:
*      1000 is vendor specific in C45.
*/
extern int rtl8373_phy_common_c45_an_restart(struct realtek_smi *priv, int port);


/* Function Name:
 *      rtl8373_phy_common_c45_autoNegoEnable_get
 * Description:
 *      Get the auto-negotiation state of the specific port
 * Input:
 *      port - port id
 * Output:
 *      pEnable - pointer to output the auto-negotiation state
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
extern int rtl8373_phy_common_c45_autoNegoEnable_get(struct realtek_smi *priv, int port, rtk_enable_t *pEnable);


/* Function Name:
 *      rtl8373_phy_common_c45_autoNegoEnable_set
 * Description:
 *      Set the auto-negotiation state of the specific port and restart auto-negotiation for enabled state.
 * Input:
 *      port - port id
 *      enable - auto-negotiation state
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
extern int rtl8373_phy_common_c45_autoNegoEnable_set(struct realtek_smi *priv, int port, rtk_enable_t enable);


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
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
extern int rtl8373_phy_autoNegoAbility_set(struct realtek_smi *priv, int port, rtk_port_phy_ability_t *pAbility);



/* Function Name:
 *      rtl8373_phy_conmmon_c45_autoSpeed_set
 * Description:
 *      Set ability advertisement for auto-negotiation of the specific port
 * Input:
 *      port - port id
 *      pAbility  - auto-negotiation ability that is going to set to PHY
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */

extern int rtl8373_phy_common_c45_autoSpeed_set(struct realtek_smi *priv, int port, rtk_port_phy_ability_t *pAbility);

/* Function Name:
 *      rtl8373_phy_autoNegoAbility_get
 * Description:
 *      get ability advertisement for auto-negotiation of the specific port
 * Input:
 *      unit - unit id
 *      port - port id
 
 * Output:
*      pAbility  - auto-negotiation ability that is going to set to PHY
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
extern int rtl8373_phy_autoNegoAbility_get(struct realtek_smi *priv, int port, rtk_port_phy_ability_t *pAbility);
/* Function Name:
 *      rtl8373_phy_common_c45_speed_get
 * Description:
 *      Get speed of the specific port
 * Input:
 *      port - port id
 * Output:
 *      pSpeed - pointer to PHY link speed
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_speed_get(struct realtek_smi *priv, int port, rtk_port_speed_t *pSpeed);

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
 *      RT_ERR_OK                 - OK
 *      RT_ERR_FAILED             - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_speed_set(struct realtek_smi *priv, int port, rtk_port_speed_t speed);

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
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_enable_set(struct realtek_smi *priv, int port, rtk_enable_t enable);

/* Function Name:
 *      rtl8373_phy_common_c45_enable_get
 * Description:
 *      Get PHY interface state enable/disable of the specific port
 * Input:
 *      port          - port id
 * Output:
 *      pEnable       - pointer to admin configuration of PHY interface
 * Return:
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_enable_get(struct realtek_smi *priv, int port, rtk_enable_t *pEnable);

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
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_duplex_get(struct realtek_smi *priv, int port, rtk_port_duplex_t *pDuplex);

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
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - invalid parameter
 * Note:
 *      None
 */
int rtl8373_phy_common_c45_duplex_set(struct realtek_smi *priv, int port, rtk_port_duplex_t duplex);

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
 *      RT_ERR_OK     - OK
 *      RT_ERR_FAILED - Failed
 * Note:
 *      Only PHY that supports proprietary resolution register can use this driver!
 */
int rtl8373_phy_common_c45_speedDuplexStatusResReg_get(struct realtek_smi *priv, int port, rtk_port_speed_t *pSpeed, rtk_port_duplex_t *pDuplex);


#endif
