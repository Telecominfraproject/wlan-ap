/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _REALTEK_SMI_RTL8373_H
#define _REALTEK_SMI_RTL8373_H

#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/version.h>
#include <net/dsa.h>
struct realtek_smi_ops;
struct dentry;
struct inode;
struct file;
struct realtek_smi;

//#include <linux/dsa/rtk_dsa.h>
#include "rtl8373_core.h"
#include "rtl8373_lut.h"
#include "rtl8373_vlan.h"
#include "rtl8373_port.h"
//#include <linux/dsa/rtk_dsa.h>



#ifndef CONFIG_MST_MDC_MDIO
#define CONFIG_MST_MDC_MDIO
#endif
/*
 * struct rtl8366_vlan_mc - Virtual LAN member configuration
 */
struct rtl8366_vlan_mc {
	u16	vid;
	u16	untag;
	u16	member;
	u8	fid;
	u8	priority;
};

struct rtl8366_vlan_4k {
	u16	vid;
	u16	untag;
	u16	member;
	u8	fid;
};
/*
 * struct realtek_smi_ops - vtable for the per-SMI-chiptype operations
 * @detect: detects the chiptype
 */


#define REALTEK_SMI_DSA_PORTS				20


struct realtek_smi {
	struct device		*dev;
	struct reset_control    *reset_ctl;
	struct gpio_desc	*reset;
	struct gpio_desc	*mdc;
	struct gpio_desc	*mdio;
	struct regmap		*map;
	struct regmap		*map_nolock;
	struct mutex		map_lock;
	//struct mii_bus		*bus;
	struct mii_bus		*slave_mii_bus;
	int			mdio_addr;
	unsigned int		clk_delay;
	u8			cmd_read;
	u8			cmd_write;
	//const struct realtek_variant *variant;

	spinlock_t		lock; /* Locks around command writes */
	struct dsa_switch	*ds;
	struct irq_domain	*irqdomain;
	bool			leds_disabled;
	unsigned int		cpu_port;
	unsigned int		start_lports;
	unsigned int		end_lports;
	unsigned int		start_pports;
	unsigned int		end_pports;	
	unsigned int		num_ports;
	unsigned int		num_vlan_mc;
	unsigned int		num_mib_counters;
	struct rtl8373_mib_counter *mib_counters;
	unsigned int		sds_rx_swap;
	unsigned int		tx_polarity_swap;
	const struct realtek_smi_ops *ops;
	int			(*write_reg_noack)(void *ctx, u32 addr, u32 data);

	int			vlan_enabled;
	int			vlan4k_enabled;

	char			buf[4096];
	void			*chip_data; /* Per-chip extra variant data */
	struct mii_bus *bus;
	bool			svlan_disable;
	//struct rtk_dsa_port ports[REALTEK_SMI_DSA_PORTS];
	enum dsa_tag_protocol proto;

};



struct realtek_smi_ops {
	int	(*detect)(struct realtek_smi *priv);
	int	(*reset_chip)(struct realtek_smi *priv);
	int	(*setup)(struct realtek_smi *priv);
	int	(*get_mib_counter)(struct realtek_smi *priv,
				   int port,
				   struct rtl8373_mib_counter *mib,
				   u64 *mibvalue);
	int	(*get_vlan_mc)(struct realtek_smi *priv, u32 index,
			       struct rtl8366_vlan_mc *vlanmc);
	int	(*set_vlan_mc)(struct realtek_smi *priv, u32 index,
			       const struct rtl8366_vlan_mc *vlanmc);
	int	(*get_vlan_4k)(struct realtek_smi *priv, u32 vid,
			       struct rtl8366_vlan_4k *vlan4k);
	int	(*set_vlan_4k)(struct realtek_smi *priv,
			       const struct rtl8366_vlan_4k *vlan4k);
	int	(*get_mc_index)(struct realtek_smi *priv, int port, int *val);
	int	(*set_mc_index)(struct realtek_smi *priv, int port, int index);
	bool	(*is_vlan_valid)(struct realtek_smi *priv, unsigned int vlan);
	int	(*enable_vlan)(struct realtek_smi *priv, bool enable);
	int	(*enable_vlan4k)(struct realtek_smi *priv, bool enable);
	int	(*enable_port)(struct realtek_smi *priv, int port, bool enable);
	int	(*phy_read)(struct realtek_smi *priv, int phy, int regnum);
	int	(*phy_write)(struct realtek_smi *priv, int phy, int regnum,
			     u16 val);
	int	(*phyc45_read)(struct realtek_smi *priv, u32 phyid,
				     u32 dev_addr, u32 reg_addr, u32 *indata);
	int	(*phyc45_write)(struct realtek_smi *priv, u32 phyid,
				     u32 dev_addr, u32 reg_addr, u32 indata);
	int	(*port_l2p_get)(struct realtek_smi *priv, int port);	
	int	(*port_p2l_get)(struct realtek_smi *priv, int port);	
	int (*eee_enable_set)(struct realtek_smi *priv, int port, bool enable);

	int	(*svlan_init)(struct realtek_smi *priv);
	int	(*svlanTpid_set)(struct realtek_smi *priv, u32 svlanTpid);
	int	(*svlan_service_port_add)(struct realtek_smi *priv, rtk_port_t port);
	int	(*svlan_service_port_get)(struct realtek_smi *priv, rtk_portmask_t *pSvlanPmsk);
	int	(*svlan_member_port_entry_set)(struct realtek_smi *priv, u32 svid, rtk_svlan_memberCfg_t *pSvlan_cfg);
	int	(*svlan_member_port_entry_get)(struct realtek_smi *priv, u32 svid, rtk_svlan_memberCfg_t *pSvlan_cfg);
	int	(*svlan_default_svlan_set)(struct realtek_smi *priv, rtk_port_t port, rtk_vlan_t svid);
	int	(*svlan_default_svlan_get)(struct realtek_smi *priv, rtk_port_t port, rtk_vlan_t *pSvid);
	int	(*port_isolation_set)(struct realtek_smi *priv, int port, rtk_portmask_t *mask);
	int	(*port_isolation_get)(struct realtek_smi *priv, int port, rtk_portmask_t *mask);
	int	(*port_link_status_get)(struct realtek_smi *priv, rtk_port_t port, rtk_port_status_t *status);
	int	(*port_serdes_mode_get)(struct realtek_smi *priv, u32 sdsid, rtk_sds_mode_t * pMode);
	int	(*port_serdes_nway_get)(struct realtek_smi *priv, u32 sdsId, rtk_sds_mode_t sdsMode, rtk_enable_t *pState);
	int	(*l2_limit_learning_cnt_set)(struct realtek_smi *priv, rtk_port_t port, rtk_mac_cnt_t mac_cnt);
	int	(*l2_limit_learning_cnt_get)(struct realtek_smi *priv, rtk_port_t port, rtk_mac_cnt_t *pMac_cnt);
	int	(*setup_rest)(struct dsa_switch *ds);
};
struct realtek_smi_variant {
	const struct dsa_switch_ops *ds_ops;
	const struct realtek_smi_ops *ops;
	unsigned int clk_delay;
	u8 cmd_read;
	u8 cmd_write;
	size_t chip_data_sz;
};

/* SMI core calls */
int realtek_smi_write_reg_noack(struct realtek_smi *smi, u32 addr,
				u32 data);
int realtek_smi_setup_mdio(struct realtek_smi *smi);


extern const struct realtek_smi_variant rtl8373_variant;
extern const struct realtek_smi_ops rtl8373_smi_ops;
#endif /*  _REALTEK_SMI_H */
