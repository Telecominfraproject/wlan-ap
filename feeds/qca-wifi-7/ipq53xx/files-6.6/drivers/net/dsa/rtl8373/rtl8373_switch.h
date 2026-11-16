// SPDX-License-Identifier: GPL-2.0
#ifndef __RTL8373_SWITCH_H__
#define __RTL8373_SWITCH_H__

#define RTL8373_PORT_NUM_CPU		8
#define RTL8373_NUM_PORTS		9
#define RTL8373_PHY_NO_MAX		8
#define RTL8373_PHY_ADDR_MAX		31

#define RTK_MAX_NUM_OF_PORT                         10
#define RTK_PORT_ID_MAX                             (RTK_MAX_NUM_OF_PORT-1)
#define RTK_PHY_ID_MAX                              (RTK_MAX_NUM_OF_PORT-4)
#define RTK_MAX_PORT_MASK                           0x3FF
#define RTK_MAX_NUM_OF_PROTO_TYPE                   (0xFFFF)

#define RTL8373_NUM_VLANS		4096
#define RTL8373_NUM_LEDGROUPS		4
#define RTL8373_NUM_VIDS		4096
#define RTL8373_PRIORITYMAX		7
#define RTL8373_NUM_FIDS		8


#ifndef DISABLED
#define DISABLED 0
#endif
#ifndef ENABLED
#define ENABLED 1
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
#if 0
#define RTL8373_PORT_ALL		(RTL8373_PORT_1 |	\
					 RTL8373_PORT_2 |	\
					 RTL8373_PORT_3 |	\
					 RTL8373_PORT_4 |	\
					 RTL8373_PORT_5 |	\
					 RTL8373_PORT_CPU)

#define RTL8373_PORT_ALL_BUT_CPU	(RTL8373_PORT_1 |	\
					 RTL8373_PORT_2 |	\
					 RTL8373_PORT_3 |	\
					 RTL8373_PORT_4 |	\
					 RTL8373_PORT_5)

#define RTL8373_PORT_ALL_EXTERNAL	(RTL8373_PORT_1 |	\
					 RTL8373_PORT_2 |	\
					 RTL8373_PORT_3 |	\
					 RTL8373_PORT_4)

#define RTL8373_PORT_ALL_INTERNAL	 RTL8373_PORT_CPU
#endif
/* Family-specific data and limits */
#define rtl8373_PHYADDRMAX		8
#define rtl8373_NUM_PHYREGS		32
#define rtl8373_PHYREGMAX		(rtl8373_NUM_PHYREGS - 1)
#define rtl8373_MAX_NUM_PORTS		9
#define rtl8373_MAX_NUM_EXTINTS	2
#define rtl8373_LEARN_LIMIT_MAX	2112


enum rtl8373_cpu_insert {
	rtl8373_CPU_INSERT_TO_ALL = 0,
	rtl8373_CPU_INSERT_TO_TRAPPING = 1,
	rtl8373_CPU_INSERT_TO_NONE = 2,
};

/**
 * struct rtl8373_extint - external interface info
 * @port: the port with an external interface
 * @id: the external interface ID, which is either 0, 1, or 2
 * @supported_interfaces: a bitmask of supported PHY interface modes
 *
 * Represents a mapping: port -> { id, supported_interfaces }. To be embedded
 * in &struct rtl8373_chip_info for every port with an external interface.
 */
struct rtl8373_extint {
	int port;
	int id;
	unsigned int supported_interfaces;
};

/**
 * struct rtl8373_cpu - CPU port configuration
 * @enable: enable/disable hardware insertion of CPU tag in switch->CPU frames
 * @mask: port mask of ports that parse should parse CPU tags
 * @trap_port: forward trapped frames to this port
 * @insert: CPU tag insertion mode in switch->CPU frames
 * @position: position of CPU tag in frame
 * @rx_length: minimum CPU RX length
 * @format: CPU tag format
 *
 * Represents the CPU tagging and CPU port configuration of the switch. These
 * settings are configurable at runtime.
 */
struct rtl8373_cpu {
	bool enable;
	u32 mask;
	u32 trap_port;
    enum rtl8373_cpu_insert insert;
};
#if 0
/**
 * struct rtl8373_port - private per-port data
 * @priv: pointer to parent realtek_smi data
 * @index: DSA port index, same as dsa_port::index
 * @stats: link statistics populated by rtl8373_stats_poll, ready for atomic
 *         access via rtl8373_get_stats64
 * @stats_lock: protect the stats structure during read/update
 * @mib_work: delayed work for polling MIB counters
 */
struct rtl8373_port {
	struct realtek_smi *priv;
	unsigned int index;
	struct rtnl_link_stats64 stats;
	spinlock_t stats_lock;
	struct delayed_work mib_work;
};
#endif

struct rtl8373_port {
	struct ethtool_eee eee;
};
/**
 * struct rtl8373 - driver private data
 * @priv: pointer to parent realtek_smi data
 * @irq: registered IRQ or zero
 * @chip_info: chip-specific info about the attached switch
 * @cpu: CPU tagging and CPU port configuration for this chip
 * @mib_lock: prevent concurrent reads of MIB counters
 * @ports: per-port data
 *
 * Private data for this driver.
 */
struct rtl8373 {
	struct realtek_smi *priv;
	int irq;
	unsigned int max_mtu[RTL8373_NUM_PORTS];
	bool pvid_enabled[RTL8373_NUM_PORTS];
	const struct rtl8373_chip_info *chip_info;
	struct rtl8373_cpu cpu;
	struct mutex mib_lock;
	struct rtl8373_port ports[rtl8373_MAX_NUM_PORTS];
};

/**
 * struct rtl8373_chip_info - static chip-specific info
 * @name: human-readable chip name
 * @chip_id: chip identifier
 * @chip_ver: chip silicon revision
 * @extints: available external interfaces
 * @jam_table: chip-specific initialization jam table
 * @jam_size: size of the chip's jam table
 *
 * These data are specific to a given chip in the family of switches supported
 * by this driver. When adding support for another chip in the family, a new
 * chip info should be added to the rtl8373_chip_infos array.
 */
struct rtl8373_chip_info {
	const char *name;
	u32 chip_id;
    u32 chip_ver;
	const struct rtl8373_extint extints[rtl8373_MAX_NUM_EXTINTS];
};

enum rtl8373_stp_state {
	RTL8373_STP_STATE_DISABLED = 0,
	RTL8373_STP_STATE_BLOCKING = 1,
	RTL8373_STP_STATE_LEARNING = 2,
	RTL8373_STP_STATE_FORWARDING = 3,
};
/*
 * Feature: Reset
 */
#define RTL8373_RST_GLB_CTRL_0_ADDR                                                                           (0x24)
  #define RTL8373_RST_GLB_CTRL_0_CFG_EN_8051_OFFSET                                                           (11)
  #define RTL8373_RST_GLB_CTRL_0_CFG_EN_8051_MASK                                                             (0x1 << RTL8373_RST_GLB_CTRL_0_CFG_EN_8051_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_GPHY_EN_OFFSET                                                               (7)
  #define RTL8373_RST_GLB_CTRL_0_GPHY_EN_MASK                                                                 (0xF << RTL8373_RST_GLB_CTRL_0_GPHY_EN_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_SDS_REG_RST_OFFSET                                                           (6)
  #define RTL8373_RST_GLB_CTRL_0_SDS_REG_RST_MASK                                                             (0x1 << RTL8373_RST_GLB_CTRL_0_SDS_REG_RST_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_SW_RST_OFFSET                                                                (5)
  #define RTL8373_RST_GLB_CTRL_0_SW_RST_MASK                                                                  (0x1 << RTL8373_RST_GLB_CTRL_0_SW_RST_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_DW8051_RST_OFFSET                                                            (4)
  #define RTL8373_RST_GLB_CTRL_0_DW8051_RST_MASK                                                              (0x1 << RTL8373_RST_GLB_CTRL_0_DW8051_RST_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_SW_SERDES_RST_OFFSET                                                         (3)
  #define RTL8373_RST_GLB_CTRL_0_SW_SERDES_RST_MASK                                                           (0x1 << RTL8373_RST_GLB_CTRL_0_SW_SERDES_RST_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_SW_NIC_RST_OFFSET                                                            (2)
  #define RTL8373_RST_GLB_CTRL_0_SW_NIC_RST_MASK                                                              (0x1 << RTL8373_RST_GLB_CTRL_0_SW_NIC_RST_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_SW_Q_RST_OFFSET                                                              (1)
  #define RTL8373_RST_GLB_CTRL_0_SW_Q_RST_MASK                                                                (0x1 << RTL8373_RST_GLB_CTRL_0_SW_Q_RST_OFFSET)
  #define RTL8373_RST_GLB_CTRL_0_SW_CHIP_RST_OFFSET                                                           (0)
  #define RTL8373_RST_GLB_CTRL_0_SW_CHIP_RST_MASK                                                             (0x1 << RTL8373_RST_GLB_CTRL_0_SW_CHIP_RST_OFFSET)

#define RTL8373_CHIP_CTRL_RESET_HW		BIT(0)
#define RTL8373_CHIP_CTRL_RESET_SW		BIT(1)

#define RTK_PORTMASK_IS_PORT_SET(__portmask__, __port__)    (((__portmask__).bits[0] & (0x00000001 << __port__)) ? 1 : 0)
#define RTK_PORTMASK_IS_EMPTY(__portmask__)                 (((__portmask__).bits[0] == 0) ? 1 : 0)
#define RTK_PORTMASK_CLEAR(__portmask__)                    ((__portmask__).bits[0] = 0)
#define RTK_PORTMASK_PORT_SET(__portmask__, __port__)       ((__portmask__).bits[0] |= (0x00000001 << __port__))
#define RTK_PORTMASK_PORT_CLEAR(__portmask__, __port__)     ((__portmask__).bits[0] &= ~(0x00000001 << __port__))
#define RTK_PORTMASK_SCAN(__portmask__, __port__)           for(__port__ = 0; __port__ < 32; __port__++)  if(RTK_PORTMASK_IS_PORT_SET(__portmask__, __port__))
#define RTK_PORTMASK_COMPARE(__portmask_A__, __portmask_B__)    ((__portmask_A__).bits[0] - (__portmask_B__).bits[0])

#define RTK_SCAN_ALL_PHY_PORTMASK(__port__)                 for(__port__ = 0; __port__ < 32; __port__++)  if( (rtk_switch_phyPortMask_get() & (0x00000001 << __port__)))
#define RTK_SCAN_ALL_LOG_PORT(__port__)                     for(__port__ = 0; __port__ < 32; __port__++)  if( rtk_switch_logicalPortCheck(__port__) == RT_ERR_OK)
#define RTK_SCAN_ALL_LOG_PORTMASK(__portmask__)             for((__portmask__).bits[0] = 0; (__portmask__).bits[0] < 0x7FFFF; (__portmask__).bits[0]++)  if( rtk_switch_isPortMaskValid(&__portmask__) == RT_ERR_OK)

int SDS_MODE_SET_SW(struct realtek_smi *priv, u32 CHIP_MODE, u32 SDS_INDX ,u32 SDS_MODE);
int fw_reset_flow_tgr_8224(struct realtek_smi *priv, u32 sdsid);
int fw_reset_flow_tgr(struct realtek_smi *priv, u32 sdsid);
int dal_rtl8373_sdsMode_get(struct realtek_smi *priv, u32 sdsid, rtk_sds_mode_t * pMode);
int dal_rtl8373_sdsMode_set(struct realtek_smi *priv, u32 sdsid, rtk_sds_mode_t mode);
rtk_api_ret_t fw_reset_flow_tgx(struct realtek_smi *smi, u32 sds);
rtk_api_ret_t prbs_8b10b_err_cnt_clear(struct realtek_smi *smi, u32 sds) ;
rtk_api_ret_t prbs_8b10b_err_cnt(struct realtek_smi *smi, u32 sds, u32* err_cnt) ;
rtk_api_ret_t prbs_64b66b(struct realtek_smi *smi, u32 sds, u32 prbs_sel, rtk_enable_t enable) ;
rtk_api_ret_t prbs_64b66b_err_cnt(struct realtek_smi *smi, u32 sds, u32* err_cnt) ;
rtk_api_ret_t tx_8081_64b66b(struct realtek_smi *smi, u32 sds, rtk_enable_t enable) ;

int rtl8373_switch_init(struct realtek_smi *priv);
int rtl8372_init(struct realtek_smi *priv);
int rtl8366u_init(struct realtek_smi *priv);

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
rtk_api_ret_t rtk_switch_portmask_P2L_get(struct realtek_smi *priv, u32 physicalPortmask, rtk_portmask_t *pLogicalPmask);

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
rtk_api_ret_t rtk_switch_portmask_L2P_get(struct realtek_smi *priv, rtk_portmask_t *pLogicalPmask, u32 *pPhysicalPortmask);
#endif