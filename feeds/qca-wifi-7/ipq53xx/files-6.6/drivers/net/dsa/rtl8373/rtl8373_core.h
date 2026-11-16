// SPDX-License-Identifier: GPL-2.0
#ifndef _REALTEK_RTL8373_CORE_H
#define _REALTEK_RTL8373_CORE_H

#include "rtl8373_reg_definition.h"
typedef enum rtl8373_sds_id_e 
{
    SERDES_ID0=0,
    SERDES_ID1=1,
    SERDES_IDEND
}rtl8373_sds_id_t;

typedef enum rtk_eee_speedInMacForceMode_e 
{
    EEE_MAC_FORCE_SPEED_100M = 0,
    EEE_MAC_FORCE_SPEED_500M,
    EEE_MAC_FORCE_SPEED_1000M,
    EEE_MAC_FORCE_SPEED_2P5G_LITE,
    EEE_MAC_FORCE_SPEED_2P5G,
    EEE_MAC_FORCE_SPEED_5G_LITE,
    EEE_MAC_FORCE_SPEED_5G,
    EEE_MAC_FORCE_SPEED_10G_LITE,
    EEE_MAC_FORCE_SPEED_10G,
    EEE_MAC_FORCE_SPEED_END
}rtk_eee_speedInMacForceMode_t;

/*
 * Data Type Declaration
 */
typedef enum rt_error_code_e
{
    RT_ERR_FAILED = -1,                             /* General Error                                                                    */

    /* 0x00xx for common error code */
    RT_ERR_OK = 0,                                  /* 0x0000, OK                                                                   */
    RT_ERR_INPUT,                                   /* 0x0001, invalid input parameter                                              */
    RT_ERR_UNIT_ID,                                 /* 0x0002, invalid unit id                                                      */
    RT_ERR_PORT_ID,                                 /* 0x0003, invalid port id                                                      */
    RT_ERR_PORT_MASK,                               /* 0x0004, invalid port mask                                                    */
    RT_ERR_PORT_LINKDOWN,                           /* 0x0005, link down port status                                                */
    RT_ERR_ENTRY_INDEX,                             /* 0x0006, invalid entry index                                                  */
    RT_ERR_NULL_POINTER,                            /* 0x0007, input parameter is null pointer                                      */
    RT_ERR_QUEUE_ID,                                /* 0x0008, invalid queue id                                                     */
    RT_ERR_QUEUE_NUM,                               /* 0x0009, invalid queue number                                                 */
    RT_ERR_BUSYWAIT_TIMEOUT,                        /* 0x000a, busy watting time out                                                */
    RT_ERR_MAC,                                     /* 0x000b, invalid mac address                                                  */
    RT_ERR_OUT_OF_RANGE,                            /* 0x000c, input parameter out of range                                         */
    RT_ERR_CHIP_NOT_SUPPORTED,                      /* 0x000d, functions not supported by this chip model                           */
    RT_ERR_SMI,                                     /* 0x000e, SMI error                                                            */
    RT_ERR_NOT_INIT,                                /* 0x000f, The module is not initial                                            */
    RT_ERR_CHIP_NOT_FOUND,                          /* 0x0010, The chip can not found                                               */
    RT_ERR_NOT_ALLOWED,                             /* 0x0011, actions not allowed by the function                                  */
    RT_ERR_DRIVER_NOT_FOUND,                        /* 0x0012, The driver can not found                                             */
    RT_ERR_SEM_LOCK_FAILED,                         /* 0x0013, Failed to lock semaphore                                             */
    RT_ERR_SEM_UNLOCK_FAILED,                       /* 0x0014, Failed to unlock semaphore                                           */
    RT_ERR_ENABLE,                                  /* 0x0015, invalid enable parameter                                             */
    RT_ERR_TBL_FULL,                                /* 0x0016, input table full                                                     */
    RT_ERR_UNKNOWN_REG,                                /* 0x0017, unknown register address                                             */
    RT_ERR_UNKNOWN_REG_FIELD,                        /* 0x0018, unknown register field                                                 */
    
    /* 0x01xx for vlan */
    RT_ERR_VLAN_VID = 0x0100,                       /* 0x0100, invalid vid                                                          */
    RT_ERR_VLAN_PRIORITY,                           /* 0x0101, invalid 1p priority                                                  */
    RT_ERR_VLAN_EMPTY_ENTRY,                        /* 0x0102, emtpy entry of vlan table                                            */
    RT_ERR_VLAN_ACCEPT_FRAME_TYPE,                  /* 0x0103, invalid accept frame type                                            */
    RT_ERR_VLAN_EXIST,                              /* 0x0104, vlan is exist                                                        */
    RT_ERR_VLAN_ENTRY_NOT_FOUND,                    /* 0x0105, specified vlan entry not found                                       */
    RT_ERR_VLAN_PORT_MBR_EXIST,                     /* 0x0106, member port exist in the specified vlan                              */
    RT_ERR_VLAN_TAGMODE,                            /* 0x0107, invalid tagmode                                                    */                         
    RT_ERR_VLAN_PROTO_AND_PORT,                     /* 0x0108, invalid protocol and port based vlan                              */

    /* 0x02xx for svlan */
    RT_ERR_SVLAN_ENTRY_INDEX = 0x0200,              /* 0x0200, invalid svid entry no                                                */
    RT_ERR_SVLAN_ETHER_TYPE,                        /* 0x0201, invalid SVLAN ether type                                             */
    RT_ERR_SVLAN_TABLE_FULL,                        /* 0x0202, no empty entry in SVLAN table                                        */
    RT_ERR_SVLAN_ENTRY_NOT_FOUND,                   /* 0x0203, specified svlan entry not found                                      */
    RT_ERR_SVLAN_EXIST,                             /* 0x0204, SVLAN entry is exist                                                 */
    RT_ERR_SVLAN_VID,                               /* 0x0205, invalid svid                                                         */

    /* 0x03xx for MSTP */
    RT_ERR_MSTI = 0x0300,                           /* 0x0300, invalid msti                                                         */
    RT_ERR_MSTP_STATE,                              /* 0x0301, invalid spanning tree status                                         */
    RT_ERR_MSTI_EXIST,                              /* 0x0302, MSTI exist                                                           */
    RT_ERR_MSTI_NOT_EXIST,                        /* 0x0303, MSTI not exist     */
    
    
    /* 0x06xx for L2 */
    RT_ERR_L2_HASH_KEY = 0x0600,                    /* 0x0600, invalid L2 Hash key                                                  */
    RT_ERR_L2_HASH_INDEX,                           /* 0x0601, invalid L2 Hash index                                                */
    RT_ERR_L2_CAM_INDEX,                            /* 0x0602, invalid L2 CAM index                                                 */
    RT_ERR_L2_ENRTYSEL,                             /* 0x0603, invalid EntrySel                                                     */
    RT_ERR_L2_INDEXTABLE_INDEX,                     /* 0x0604, invalid L2 index table(=portMask table) index                        */
    RT_ERR_LIMITED_L2ENTRY_NUM,                     /* 0x0605, invalid limited L2 entry number                                      */
    RT_ERR_L2_AGGREG_PORT,                          /* 0x0606, this aggregated port is not the lowest physical
                                                               port of its aggregation group                                        */
    RT_ERR_L2_FID,                                  /* 0x0607, invalid fid                                                          */
    RT_ERR_L2_VID,                                  /* 0x0608, invalid cvid                                                         */
    RT_ERR_L2_NO_EMPTY_ENTRY,                       /* 0x0609, no empty entry in L2 table                                           */
    RT_ERR_L2_ENTRY_NOTFOUND,                       /* 0x060a, specified entry not found                                            */
    RT_ERR_L2_INDEXTBL_FULL,                        /* 0x060b, the L2 index table is full                                           */
    RT_ERR_L2_INVALID_FLOWTYPE,                     /* 0x060c, invalid L2 flow type                                                 */
    RT_ERR_L2_L2UNI_PARAM,                          /* 0x060d, invalid L2 unicast parameter                                         */
    RT_ERR_L2_L2MULTI_PARAM,                        /* 0x060e, invalid L2 multicast parameter                                       */
    RT_ERR_L2_IPMULTI_PARAM,                        /* 0x060f, invalid L2 ip multicast parameter                                    */
    RT_ERR_L2_PARTIAL_HASH_KEY,                     /* 0x0610, invalid L2 partial Hash key                                          */
    RT_ERR_L2_EMPTY_ENTRY,                          /* 0x0611, the entry is empty(invalid)                                          */
    RT_ERR_L2_FLUSH_TYPE,                           /* 0x0612, the flush type is invalid                                            */
    RT_ERR_L2_NO_CPU_PORT,                          /* 0x0613, CPU port not exist                                                   */
    RT_ERR_L2_FWDTBL_FULL,                          /* 0x00060014, forwarding table is full   */
    
    RT_ERR_END                                       /* The symbol is the latest symbol                                                  */

} rt_error_code_t;





#define RTL8373_MIB_ADDRESS(_p, _x) \
        (1 | (((_x) & 0xf) << 1) | ((((_p) / 2) & 0x3f) << 5))

 
struct rtl8366u_jam_tbl_entry {
	u16 reg;
	u32 val;
};



enum RTL8370UG_TABLE_ACCESS_EXECUTE
{
    TB_NOT_EXECUTE = 0,
    TB_EXECUTE,
};


enum RTL8370UG_TABLE_ACCESS_OP
{
    TB_OP_READ = 0,
    TB_OP_WRITE
};

enum RTL8370UG_TABLE_ACCESS_TARGET
{
    TB_TARGET_ACLRULE = 1,
    TB_TARGET_ACLACT,
    TB_TARGET_CVLAN,
    TB_TARGET_L2,
    TB_TARGET_IGMP_GROUP,
    TB_TARGET_HSA,
    TB_TARGET_HSB
};


#define RTL8373_CFG_PHY_MDI_REVERSE_ADDR                                                                      (0xA90)
#define RTL8373_N_PORTS	9
#define rtl8373_for_each_port(dev, i) \
	for (i = 0; i < RTL8373_N_PORTS; i++) \
		if (dev->enabled_ports & BIT(i))

typedef enum rtk_port_e
{
    UTP_PORT0 = 0,
    UTP_PORT1,
    UTP_PORT2,
    UTP_PORT3,
    UTP_PORT4,
    UTP_PORT5,
    UTP_PORT6,
    UTP_PORT7,
    UTP_PORT8,
    UTP_PORT9,
    
    UNDEFINE_PORT = 30,
    RTK_PORT_MAX = 31
} rtk_port_t;

typedef enum rtk_enable_e
{
    RTK_DISABLED = 0,
    RTK_ENABLED,
    RTK_ENABLE_END
} rtk_enable_t;

typedef enum chip_mode_type_e
{
    CHIP_RTL8373_MODE,
    CHIP_RTL8372_MODE,
    CHIP_RTL8224_MODE,
    CHIP_RTL8221B_MODE,
    CHIP_RTL8373N_MODE,
    CHIP_RTL8372N_MODE,
    CHIP_RTL8224N_MODE,
    CHIP_RTL8366U_MODE,
    CHIP_MODE_END
}chip_mode_type_t;

typedef enum rtk_port_speed_e
{
    PORT_SPEED_10M = 0,
    PORT_SPEED_100M,
    PORT_SPEED_1000M,
    PORT_SPEED_500M,
    PORT_SPEED_10G,
    PORT_SPEED_2500M,
    PORT_SPEED_5G,
    PORT_SPEED_2PAIR_1G,
    PORT_SPEED_2PAIR_2500M,
    PORT_SPEED_2PAIR_5G,
    PORT_SPEED_END
} rtk_port_speed_t;

typedef enum rtk_port_duplex_e
{
    PORT_HALF_DUPLEX = 0,
    PORT_FULL_DUPLEX,
    PORT_DUPLEX_END
} rtk_port_duplex_t;

typedef enum rtk_port_linkStatus_e
{
    PORT_LINKDOWN = 0,
    PORT_LINKUP,
    PORT_LINKSTATUS_END
} rtk_port_linkStatus_t;

typedef enum rtk_port_media_e
{
    PORT_MEDIA_UTP = 0,
    PORT_MEDIA_FIBER,
    PORT_MEDIA_END
}rtk_port_media_t;

typedef enum rtk_sds_mode_e 
{
    SERDES_10GQXG,
    SERDES_10GUSXG = 0xD,
    SERDES_10GR = 0x1A,
    SERDES_HSG = 0x12,
    SERDES_2500BASEX = 0x16,
    SERDES_SG = 2,
    SERDES_1000BASEX = 4,
    SERDES_100FX = 5,  
    SERDES_OFF = 0x1F,
    SERDES_8221B = 0x21,
    SERDES_ON = 0x22,
    SERDES_END
}rtk_sds_mode_t;

typedef struct  rtk_port_ability_s{
    rtk_enable_t forcemode;
    rtk_enable_t txpause;
    rtk_enable_t rxpause;
    rtk_port_linkStatus_t link;
    rtk_port_duplex_t duplex;
    rtk_port_speed_t speed;
    rtk_port_media_t media;
    rtk_enable_t smi_force_fc;
}rtk_port_ability_t;
#define RTL8373_REGBITLENGTH               (32)
#define RTL8373_REGDATAMAX                  (0xFFFFFFFF)
#define RTL8373_REG_ADDR_STEP             (4)
#define RTL8373_PHYBITLENGTH               (16)
#define RTL8373_PHYDATAMAX                  (0xFFFF)

#define RTL8373_MAX_POLLCNT  1000
#ifndef rtk_api_ret_t
#define rtk_api_ret_t int
#endif
#ifndef ret_t
#define ret_t int
#endif


int rtl8373_getAsicReg(struct regmap *map, u32 reg, u32 *value);
int rtl8373_setAsicReg(struct regmap *map, u32 reg, u32 value);
int rtl8373_setAsicRegBit(struct regmap *map,u32 reg, u32 bit, u32 value);
int rtl8373_getAsicRegBit(struct regmap *map, u32 reg, u32 bit, u32 *value);
int rtl8373_setAsicRegBits(struct regmap *map, u32 reg, u32 mask, u32 value);
int rtl8373_getAsicRegBits(struct regmap *map, u32 reg, u32 mask, u32 *value);


int rtl8373_phy_read(struct realtek_smi *priv, u32 phyid,
				     u32 dev_addr, u32 reg_addr, u32 *indata);

int rtl8373_phy_write(struct realtek_smi *priv, u32 phy_mask,
				     u32 dev_addr, u32 reg_addr, u32 indata);


int rtl8373_phymask_write(struct realtek_smi *priv, u32 phy_mask,
				       u32 dev_addr, u32 reg_addr, u32 indata);

int rtl8373_phy_regbits_write(struct realtek_smi *priv, u32 phy_mask,
				       u32 dev_addr, u32 reg_addr, u32 mask, u32 indata);


int rtl8373_phy_regbits_read(struct realtek_smi *priv, u32 phyid,
                         u32 dev_addr, u32 reg_addr, u32 mask, u32 *indata);
void delay_0_1ms(u32 units);    

int dal_rtl8373_sds_reg_read(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 *pdata);
int dal_rtl8373_sds_reg_write(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 regdata);
int dal_rtl8373_sds_regbits_write(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 bit_mask, u32 bit_value);
int dal_rtl8373_sds_regbits_read(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 mask, u32 *pdata);
int dal_rtl8224_sds_regbits_read(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 bitmask, u32 * pvalue);
int dal_rtl8224_sds_regbits_write(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 bitmask, u32 value);
int dal_rtl8224_sds_reg_read(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 * pdata);
int dal_rtl8224_sds_reg_write(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 regdata);
int dal_rtl8224_top_regbit_write(struct realtek_smi *priv, u32 top_reg_addr,  u32 offset, u32 value);
int dal_rtl8224_top_regbit_read(struct realtek_smi *priv, u32 top_reg_addr,  u32 offset, u32* pvalue);
int dal_rtl8224_top_regbits_read(struct realtek_smi *priv, u32 top_reg_addr,  u32 bitmask, u32* pvalue);
int dal_rtl8224_top_regbits_write(struct realtek_smi *priv, u32 top_reg_addr, u32 bitmask, u32 value);

int dal_rtl8224_top_reg_write(struct realtek_smi *priv, u32 top_reg_addr,  u32 value);
int dal_rtl8224_top_reg_read(struct realtek_smi *priv, u32 top_reg_addr,  u32* pvalue);

enum dsa_tag_protocol
rtl8373_get_tag_protocol(struct dsa_switch *ds, int port,
			   enum dsa_tag_protocol mp);
int rtl8373_portMaxLen_get(struct dsa_switch *ds, int port);
int rtl8373_portMaxLen_set(struct dsa_switch *ds, int port, int mtu);

void RL6818C_pwr_on_patch_phy_v009(struct realtek_smi *priv, u32 phymask);
void RL6818C_pwr_on_patch_phy_v009_rls_lockmain(struct realtek_smi *priv, u32 phymask);

int rtl8373_probe(struct realtek_smi *chip);
#endif
