// SPDX-License-Identifier: GPL-2.0
/* FILE NAME:  an8801.c
 * PURPOSE:
 *      Airoha phy driver for Linux
 * NOTES:
 *
 */

/* INCLUDE FILE DECLARATIONS
 */

#include <linux/of_device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/version.h>
#include <linux/debugfs.h>

#include "an8801.h"

MODULE_DESCRIPTION("Airoha AN8801 PHY drivers");
MODULE_AUTHOR("Airoha");
MODULE_LICENSE("GPL");

#define phydev_mdiobus(phy)        ((phy)->mdio.bus)
#define phydev_mdiobus_lock(phy)   (phydev_mdiobus(phy)->mdio_lock)
#define phydev_cfg(phy)            ((struct an8801_priv *)(phy)->priv)

#define mdiobus_lock(phy)          (mutex_lock(&phydev_mdiobus_lock(phy)))
#define mdiobus_unlock(phy)        (mutex_unlock(&phydev_mdiobus_lock(phy)))

#define MAX_SGMII_AN_RETRY              (100)

#ifdef AN8801SB_DEBUGFS
#define AN8801_DEBUGFS_POLARITY_HELP_STRING \
	"\nUsage: echo [tx_polarity] [rx_polarity] > /sys/" \
	"kernel/debug/mdio-bus\':[phy_addr]/polarity" \
	"\npolarity: tx_normal, tx_reverse, rx_normal, rx_reverse" \
	"\ntx_normal is tx polarity is normal." \
	"\ntx_reverse is tx polarity need to be swapped." \
	"\nrx_normal is rx polarity is normal." \
	"\nrx_reverse is rx polarity need to be swapped." \
	"\nFor example tx polarity need to be swapped. " \
	"But rx polarity is normal." \
	"\necho tx_reverse rx_normal > /sys/" \
	"kernel/debug/mdio-bus\':[phy_addr]/polarity" \
	"\n"
#define AN8801_DEBUGFS_RX_ERROR_STRING \
	"\nRx param is not correct." \
	"\nrx_normal: rx polarity is normal." \
	"rx_reverse: rx polarity is reverse.\n"
#define AN8801_DEBUGFS_TX_ERROR_STRING \
	"\nTx param is not correct." \
	"\ntx_normal: tx polarity is normal." \
	"tx_reverse: tx polarity is reverse.\n"
#define AN8801_DEBUGFS_PBUS_HELP_STRING \
	"\nUsage: echo w [pbus_addr] [pbus_reg] [value] > /sys/" \
	"kernel/debug/mdio-bus\':[phy_addr]/pbus_reg_op" \
	"\n       echo r [pbus_addr] [pbus_reg] > /sys/" \
	"kernel/debug/mdio-bus\':[phy_addr]/pbus_reg_op" \
	"\nRead example: PBUS addr 0x19, Register 0x19a4" \
	"\necho r 19 19a4 > /sys/" \
	"kernel/debug/mdio-bus\':[phy_addr]/pbus_reg_op" \
	"\nWrite example: PBUS addr 0x19, Register 0xcf8 0x1a01503" \
	"\necho w 19 cf8 1a01503> /sys/" \
	"kernel/debug/mdio-bus\':[phy_addr]/pbus_reg_op" \
	"\n"
#endif

#if (KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE)
#define phydev_dev(_dev) (&_dev->dev)
#else
#define phydev_dev(_dev) (&_dev->mdio.dev)
#endif

/* For reference only
 *	GPIO1    <-> LED0,
 *	GPIO2    <-> LED1,
 *	GPIO3    <-> LED2,
 */
/* User-defined.B */
static const struct AIR_LED_CFG_T led_cfg_dlt[MAX_LED_SIZE] = {
//   LED Enable,          GPIO,    LED Polarity,      LED ON,    LED Blink
	/* LED0 */
	{LED_ENABLE, AIR_LED_GPIO1, AIR_ACTIVE_LOW,  AIR_LED0_ON, AIR_LED0_BLK},
	/* LED1 */
	{LED_ENABLE, AIR_LED_GPIO2, AIR_ACTIVE_HIGH, AIR_LED1_ON, AIR_LED1_BLK},
	/* LED2 */
	{LED_ENABLE, AIR_LED_GPIO3, AIR_ACTIVE_HIGH, AIR_LED2_ON, AIR_LED2_BLK},
};

static const u16 led_blink_cfg_dlt = AIR_LED_BLK_DUR_64M;
/* RGMII delay */
static const u8 rxdelay_force = FALSE;
static const u8 txdelay_force = FALSE;
static const u16 rxdelay_step = AIR_RGMII_DELAY_NOSTEP;
static const u8 rxdelay_align = FALSE;
static const u16 txdelay_step = AIR_RGMII_DELAY_NOSTEP;
/* User-defined.E */

/************************************************************************
 *                  F U N C T I O N S
 ************************************************************************/
static int __air_buckpbus_reg_write(struct phy_device *phydev, u32 addr,
				    u32 data)
{
	int err = 0;

	err = __phy_write(phydev, 0x1F, 4);
	if (err)
		return err;

	err |= __phy_write(phydev, 0x10, 0);
	err |= __phy_write(phydev, 0x11, (u16)(addr >> 16));
	err |= __phy_write(phydev, 0x12, (u16)(addr & 0xffff));
	err |= __phy_write(phydev, 0x13, (u16)(data >> 16));
	err |= __phy_write(phydev, 0x14, (u16)(data & 0xffff));
	err |= __phy_write(phydev, 0x1F, 0);

	return err;
}

static u32 __air_buckpbus_reg_read(struct phy_device *phydev, u32 addr)
{
	int err = 0;
	u32 data_h, data_l, data;

	err = __phy_write(phydev, 0x1F, 4);
	if (err)
		return err;

	err |= __phy_write(phydev, 0x10, 0);
	err |= __phy_write(phydev, 0x15, (u16)(addr >> 16));
	err |= __phy_write(phydev, 0x16, (u16)(addr & 0xffff));
	data_h = __phy_read(phydev, 0x17);
	data_l = __phy_read(phydev, 0x18);
	err |= __phy_write(phydev, 0x1F, 0);
	if (err)
		return INVALID_DATA;

	data = ((data_h & 0xffff) << 16) | (data_l & 0xffff);
	return data;
}

static int air_buckpbus_reg_write(struct phy_device *phydev, u32 addr, u32 data)
{
	int err = 0;

	mdiobus_lock(phydev);
	err = __air_buckpbus_reg_write(phydev, addr, data);
	mdiobus_unlock(phydev);

	return err;
}

static u32 air_buckpbus_reg_read(struct phy_device *phydev, u32 addr)
{
	u32 data;

	mdiobus_lock(phydev);
	data = __air_buckpbus_reg_read(phydev, addr);
	mdiobus_unlock(phydev);

	return data;
}

static int __an8801_cl45_write(struct phy_device *phydev, int devad, u16 reg,
				u16 val)
{
	u32 addr = (AN8801_EPHY_ADDR | AN8801_CL22 | (devad << 18) |
				(reg << 2));

	return __air_buckpbus_reg_write(phydev, addr, val);
}

static int __an8801_cl45_read(struct phy_device *phydev, int devad, u16 reg)
{
	u32 addr = (AN8801_EPHY_ADDR | AN8801_CL22 | (devad << 18) |
				(reg << 2));

	return __air_buckpbus_reg_read(phydev, addr);
}

static int an8801_cl45_write(struct phy_device *phydev, int devad, u16 reg,
			      u16 val)
{
	int err = 0;

	mdiobus_lock(phydev);
	err = __an8801_cl45_write(phydev, devad, reg, val);
	mdiobus_unlock(phydev);

	return err;
}

static int an8801_cl45_read(struct phy_device *phydev, int devad, u16 reg,
			     u16 *read_data)
{
	int data = 0;

	mdiobus_lock(phydev);
	data = __an8801_cl45_read(phydev, devad, reg);
	mdiobus_unlock(phydev);

	if (data == INVALID_DATA)
		return -EINVAL;

	*read_data = data;

	return 0;
}

static int air_sw_reset(struct phy_device *phydev)
{
	u32 reg_value;
	u8 retry = MAX_RETRY;

	/* Software Reset PHY */
	reg_value = phy_read(phydev, MII_BMCR);
	reg_value |= BMCR_RESET;
	phy_write(phydev, MII_BMCR, reg_value);
	do {
		mdelay(10);
		reg_value = phy_read(phydev, MII_BMCR);
		retry--;
		if (retry == 0) {
			phydev_err(phydev, "Reset fail !\n");
			return -1;
		}
	} while (reg_value & BMCR_RESET);

	return 0;
}

static int an8801_led_set_usr_def(struct phy_device *phydev, u8 entity,
				   u16 polar, u16 on_evt, u16 blk_evt)
{
	int err;

	if (polar == AIR_ACTIVE_HIGH)
		on_evt |= LED_ON_POL;
	else
		on_evt &= ~LED_ON_POL;

	on_evt |= LED_ON_EN;

	err = an8801_cl45_write(phydev, 0x1f, LED_ON_CTRL(entity), on_evt);
	if (err)
		return -1;

	return an8801_cl45_write(phydev, 0x1f, LED_BLK_CTRL(entity), blk_evt);
}

static int an8801_led_set_mode(struct phy_device *phydev, u8 mode)
{
	int err;
	u16 data;

	err = an8801_cl45_read(phydev, 0x1f, LED_BCR, &data);
	if (err)
		return -1;

	switch (mode) {
	case AIR_LED_MODE_DISABLE:
		data &= ~LED_BCR_EXT_CTRL;
		data &= ~LED_BCR_MODE_MASK;
		data |= LED_BCR_MODE_DISABLE;
		break;
	case AIR_LED_MODE_USER_DEFINE:
		data |= (LED_BCR_EXT_CTRL | LED_BCR_CLK_EN);
		break;
	}
	return an8801_cl45_write(phydev, 0x1f, LED_BCR, data);
}

static int an8801_led_set_state(struct phy_device *phydev, u8 entity, u8 state)
{
	u16 data;
	int err;

	err = an8801_cl45_read(phydev, 0x1f, LED_ON_CTRL(entity), &data);
	if (err)
		return err;

	if (state)
		data |= LED_ON_EN;
	else
		data &= ~LED_ON_EN;

	return an8801_cl45_write(phydev, 0x1f, LED_ON_CTRL(entity), data);
}

static int an8801_led_init(struct phy_device *phydev)
{
	struct an8801_priv *priv = phydev_cfg(phydev);
	struct AIR_LED_CFG_T *led_cfg = priv->led_cfg;
	int ret, led_id;
	u32 data;
	u16 led_blink_cfg = priv->led_blink_cfg;

	ret = an8801_cl45_write(phydev, 0x1f, LED_BLK_DUR,
				 LED_BLINK_DURATION(led_blink_cfg));
	if (ret)
		return ret;

	ret = an8801_cl45_write(phydev, 0x1f, LED_ON_DUR,
				 (LED_BLINK_DURATION(led_blink_cfg) >> 1));
	if (ret)
		return ret;

	ret = an8801_led_set_mode(phydev, AIR_LED_MODE_USER_DEFINE);
	if (ret != 0) {
		phydev_err(phydev, "LED fail to set mode, ret %d !\n", ret);
		return ret;
	}

	for (led_id = AIR_LED0; led_id < MAX_LED_SIZE; led_id++) {
		ret = an8801_led_set_state(phydev, led_id, led_cfg[led_id].en);
		if (ret != 0) {
			phydev_err(phydev,
				   "LED fail to set LED(%d) state, ret %d !\n",
				   led_id, ret);
			return ret;
		}
		if (led_cfg[led_id].en == LED_ENABLE) {
			data = air_buckpbus_reg_read(phydev, 0x10000054);
			data |= BIT(led_cfg[led_id].gpio);
			ret |= air_buckpbus_reg_write(phydev, 0x10000054, data);

			data = air_buckpbus_reg_read(phydev, 0x10000058);
			data |= LED_GPIO_SEL(led_id, led_cfg[led_id].gpio);
			ret |= air_buckpbus_reg_write(phydev, 0x10000058, data);

			data = air_buckpbus_reg_read(phydev, 0x10000070);
			data &= ~BIT(led_cfg[led_id].gpio);
			ret |= air_buckpbus_reg_write(phydev, 0x10000070, data);

			ret |= an8801_led_set_usr_def(phydev, led_id,
				led_cfg[led_id].pol,
				led_cfg[led_id].on_cfg,
				led_cfg[led_id].blk_cfg);
			if (ret != 0) {
				phydev_err(phydev,
					   "Fail to set LED(%d) usr def, ret %d !\n",
					   led_id, ret);
				return ret;
			}
		}
	}
	phydev_info(phydev, "LED initialize OK !\n");
	return 0;
}

#ifdef CONFIG_OF
static int an8801r_of_init(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct device_node *of_node = dev->of_node;
	struct an8801_priv *priv = phydev_cfg(phydev);
	u32 val = 0;

	if (of_find_property(of_node, "airoha,rxclk-delay", NULL)) {
		if (of_property_read_u32(of_node, "airoha,rxclk-delay",
					 &val) != 0) {
			phydev_err(phydev, "airoha,rxclk-delay value is invalid.");
			return -1;
		}
		if (val < AIR_RGMII_DELAY_NOSTEP ||
		    val > AIR_RGMII_DELAY_STEP_7) {
			phydev_err(phydev,
				   "airoha,rxclk-delay value %u out of range.",
				   val);
			return -1;
		}
		priv->rxdelay_force = TRUE;
		priv->rxdelay_step = val;
		priv->rxdelay_align = of_property_read_bool(of_node,
							    "airoha,rxclk-delay-align");
	}

	if (of_find_property(of_node, "airoha,txclk-delay", NULL)) {
		if (of_property_read_u32(of_node, "airoha,txclk-delay",
					 &val) != 0) {
			phydev_err(phydev,
				   "airoha,txclk-delay value is invalid.");
			return -1;
		}
		if (val < AIR_RGMII_DELAY_NOSTEP ||
		    val > AIR_RGMII_DELAY_STEP_7) {
			phydev_err(phydev,
				   "airoha,txclk-delay value %u out of range.",
				   val);
			return -1;
		}
		priv->txdelay_force = TRUE;
		priv->txdelay_step = val;
	}

	return 0;
}
#else
static int an8801r_of_init(struct phy_device *phydev)
{
	return 0;
}
#endif /* CONFIG_OF */

static int an8801r_rgmii_rxdelay(struct phy_device *phydev, u16 delay, u8 align)
{
	u32 reg_val = delay & RGMII_DELAY_STEP_MASK;

	/* align */
	if (align) {
		reg_val |= RGMII_RXDELAY_ALIGN;
		phydev_info(phydev, "Rxdelay align\n");
	}
	reg_val |= RGMII_RXDELAY_FORCE_MODE;
	air_buckpbus_reg_write(phydev, 0x1021C02C, reg_val);
	reg_val = air_buckpbus_reg_read(phydev, 0x1021C02C);
	phydev_info(phydev, "Force rxdelay = %d(0x%x)\n", delay, reg_val);
	return 0;
}

static int an8801r_rgmii_txdelay(struct phy_device *phydev, u16 delay)
{
	u32 reg_val = delay & RGMII_DELAY_STEP_MASK;

	reg_val |= RGMII_TXDELAY_FORCE_MODE;
	air_buckpbus_reg_write(phydev, 0x1021C024, reg_val);
	reg_val = air_buckpbus_reg_read(phydev, 0x1021C024);
	phydev_info(phydev, "Force txdelay = %d(0x%x)\n", delay, reg_val);
	return 0;
}

static int an8801r_rgmii_delay_config(struct phy_device *phydev)
{
	struct an8801_priv *priv = phydev_cfg(phydev);

	if (priv->rxdelay_force)
		an8801r_rgmii_rxdelay(phydev, priv->rxdelay_step,
				      priv->rxdelay_align);
	if (priv->txdelay_force)
		an8801r_rgmii_txdelay(phydev, priv->txdelay_step);
	return 0;
}

static int an8801sb_config_init(struct phy_device *phydev)
{
	int ret;

	/* disable LPM */
	ret = an8801_cl45_write(phydev, MMD_DEV_VSPEC2, 0x600, 0x1e);
	ret |= an8801_cl45_write(phydev, MMD_DEV_VSPEC2, 0x601, 0x02);
	/*default disable EEE*/
	ret |= an8801_cl45_write(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV, 0x0);
	if (ret != 0) {
		phydev_err(phydev, "AN8801SB initialize fail, ret %d !\n", ret);
		return ret;
	}

	ret = an8801_led_init(phydev);
	if (ret != 0) {
		phydev_err(phydev, "LED initialize fail, ret %d !\n", ret);
		return ret;
	}
	air_buckpbus_reg_write(phydev, 0x10270100, 0x0f);
	air_buckpbus_reg_write(phydev, 0x10270104, 0x3f);
	air_buckpbus_reg_write(phydev, 0x10270108, 0x10100303);
	phydev_info(phydev, "AN8801SB Initialize OK ! (%s)\n",
		AN8801_DRIVER_VERSION);
	return 0;
}

static int an8801r_config_init(struct phy_device *phydev)
{
	int ret;

	ret = an8801r_of_init(phydev);
	if (ret < 0)
		return ret;

	ret = air_sw_reset(phydev);
	if (ret < 0)
		return ret;

	air_buckpbus_reg_write(phydev, 0x11F808D0, 0x180);

	air_buckpbus_reg_write(phydev, 0x1021c004, 0x1);
	air_buckpbus_reg_write(phydev, 0x10270004, 0x3f);
	air_buckpbus_reg_write(phydev, 0x10270104, 0xff);
	air_buckpbus_reg_write(phydev, 0x10270204, 0xff);

	an8801r_rgmii_delay_config(phydev);

	ret = an8801_led_init(phydev);
	if (ret != 0) {
		phydev_err(phydev, "LED initialize fail, ret %d !\n", ret);
		return ret;
	}
	phydev_info(phydev, "AN8801R Initialize OK ! (%s)\n",
		AN8801_DRIVER_VERSION);
	return 0;
}

static int an8801_config_init(struct phy_device *phydev)
{
	if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
		an8801sb_config_init(phydev);
	} else if (phydev->interface == PHY_INTERFACE_MODE_RGMII) {
		an8801r_config_init(phydev);
	} else {
		phydev_info(phydev, "AN8801 Phy-mode not support!!!\n");
		return -1;
	}
	return 0;
}

#ifdef AN8801SB_DEBUGFS
static const char * const tx_rx_string[32] = {
		"Tx Normal, Rx Reverse",
		"Tx Reverse, Rx Reverse",
		"Tx Normal, Rx Normal",
		"Tx Reverse, Rx Normal",
};

int an8801_set_polarity(struct phy_device *phydev, int tx_rx)
{
	int ret = 0;
	unsigned long pbus_data = 0;

	pr_notice("\n[Write] Polarity %s\n", tx_rx_string[tx_rx]);
	pbus_data = (air_buckpbus_reg_read(phydev, 0x1022a0f8) &
				(~(BIT(0) | BIT(1))));
	pbus_data |= (BIT(4) | tx_rx);
	ret = air_buckpbus_reg_write(phydev, 0x1022a0f8, pbus_data);
	if (ret < 0)
		return ret;
	usleep_range(9800, 12000);
	pbus_data &= ~BIT(4);
	ret = air_buckpbus_reg_write(phydev, 0x1022a0f8, pbus_data);
	if (ret < 0)
		return ret;
	pbus_data = air_buckpbus_reg_read(phydev, 0x1022a0f8);
	tx_rx = pbus_data & (BIT(0) | BIT(1));
	pr_notice("\n[Read] Polarity %s confirm....(%8lx)\n",
		tx_rx_string[tx_rx], pbus_data);

	return ret;
}

int air_polarity_help(void)
{
	pr_notice(AN8801_DEBUGFS_POLARITY_HELP_STRING);
	return 0;
}

static ssize_t an8801_polarity_write(struct file *file, const char __user *ptr,
					size_t len, loff_t *off)
{
	struct phy_device *phydev = file->private_data;
	char buf[32], param1[32], param2[32];
	int count = len, ret = 0, tx_rx = 0;

	memset(buf, 0, 32);
	memset(param1, 0, 32);
	memset(param2, 0, 32);

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, ptr, len))
		return -EFAULT;

	ret = sscanf(buf, "%s %s", param1, param2);
	if (ret < 0)
		return ret;

	if (!strncmp("help", param1, strlen("help"))) {
		air_polarity_help();
		return count;
	}
	if (!strncmp("tx_normal", param1, strlen("tx_normal"))) {
		if (!strncmp("rx_normal", param2, strlen("rx_normal")))
			tx_rx = AIR_POL_TX_NOR_RX_NOR;
		else if (!strncmp("rx_reverse", param2, strlen("rx_reverse")))
			tx_rx = AIR_POL_TX_NOR_RX_REV;
		else {
			pr_notice(AN8801_DEBUGFS_RX_ERROR_STRING);
			return -EINVAL;
		}
	} else if (!strncmp("tx_reverse", param1, strlen("tx_reverse"))) {
		if (!strncmp("rx_normal", param2, strlen("rx_normal")))
			tx_rx = AIR_POL_TX_REV_RX_NOR;
		else if (!strncmp("rx_reverse", param2, strlen("rx_reverse")))
			tx_rx = AIR_POL_TX_REV_RX_REV;
		else {
			pr_notice(AN8801_DEBUGFS_RX_ERROR_STRING);
			return -EINVAL;
		}
	} else {
		pr_notice(AN8801_DEBUGFS_TX_ERROR_STRING);
		return -EINVAL;
	}
	ret = an8801_set_polarity(phydev, tx_rx);
	if (ret < 0)
		return ret;
	return count;
}

static int an8801_counter_show(struct seq_file *seq, void *v)
{
	struct phy_device *phydev = seq->private;
	int ret = 0;
	u32 pkt_cnt = 0;

	seq_puts(seq, "==========AIR PHY COUNTER==========\n");
	seq_puts(seq, "|\t<<FCM COUNTER>>\n");
	seq_puts(seq, "| Rx from Line side_S    :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x10270130);
	seq_printf(seq, "%010u |\n", pkt_cnt);
	seq_puts(seq, "| Rx from Line side_E    :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x10270134);
	seq_printf(seq, "%010u |\n", pkt_cnt);
	seq_puts(seq, "| Tx to System side_S    :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x10270138);
	seq_printf(seq, "%010u |\n", pkt_cnt);
	seq_puts(seq, "| Tx to System side_E    :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x1027013C);
	seq_printf(seq, "%010u |\n", pkt_cnt);
	seq_puts(seq, "| Rx from System side_S  :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x10270120);
	seq_printf(seq, "%010u |\n", pkt_cnt);
	seq_puts(seq, "| Rx from System side_E  :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x10270124);
	seq_printf(seq, "%010u |\n", pkt_cnt);
	seq_puts(seq, "| Tx to Line side_S      :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x10270128);
	seq_printf(seq, "%010u |\n", pkt_cnt);
	seq_puts(seq, "| Tx to Line side_E      :");
	pkt_cnt = air_buckpbus_reg_read(phydev, 0x1027012C);
	seq_printf(seq, "%010u |\n", pkt_cnt);

	ret = air_buckpbus_reg_write(phydev, 0x1027011C, 0x3);
	if (ret < 0)
		seq_printf(seq, "\nClear Counter fail\n", __func__);
	else
		seq_puts(seq, "\nClear Counter!!\n");

	return ret;
}

static int an8801_counter_open(struct inode *inode, struct file *file)
{
	return single_open(file, an8801_counter_show, inode->i_private);
}

static int an8801_debugfs_pbus_help(void)
{
	pr_notice(AN8801_DEBUGFS_PBUS_HELP_STRING);
	return 0;
}

static ssize_t an8801_debugfs_pbus(struct file *file,
		const char __user *buffer, size_t count,
		loff_t *data)
{
	struct phy_device *phydev = file->private_data;
	char buf[64];
	int ret = 0;
	unsigned int reg, addr;
	unsigned long val;

	memset(buf, 0, 64);

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (buf[0] == 'w') {
		if (sscanf(buf, "w %x %x %lx", &addr, &reg, &val) == -1)
			return -EFAULT;
		if (addr > 0 && addr < 32) {
			pr_notice("\nphy=0x%x, reg=0x%x, val=0x%lx\n",
				addr, reg, val);

			ret = air_buckpbus_reg_write(phydev, reg, val);
			if (ret < 0)
				return ret;
			pr_notice("\nphy=%d, reg=0x%x, val=0x%lx confirm..\n",
				addr, reg,
				air_buckpbus_reg_read(phydev, reg));
		} else {
			pr_notice("addr is out of range(1~32)\n");
		}
	} else if (buf[0] == 'r') {
		if (sscanf(buf, "r %x %x", &addr, &reg) == -1)
			return -EFAULT;
		if (addr > 0 && addr < 32) {
			pr_notice("\nphy=0x%x, reg=0x%x, val=0x%lx\n",
				addr, reg,
				air_buckpbus_reg_read(phydev, reg));
		} else {
			pr_notice("addr is out of range(1~32)\n");
		}
	} else if (buf[0] == 'h') {
		an8801_debugfs_pbus_help();
	}

	return count;
}

int an8801_info_show(struct seq_file *seq, void *v)
{
	struct phy_device *phydev = seq->private;
	unsigned int tx_rx =
		(air_buckpbus_reg_read(phydev, 0x1022a0f8) & 0x3);
	unsigned long pbus_data = 0;

	seq_puts(seq, "<<AIR AN8801 Driver Info>>\n");
	pbus_data = air_buckpbus_reg_read(phydev, 0x1000009c);
	seq_printf(seq, "| Product Version : E%ld\n", pbus_data & 0xf);
	seq_printf(seq, "| Driver Version  : %s\n", AN8801_DRIVER_VERSION);
	pbus_data = air_buckpbus_reg_read(phydev, 0x10220b04);
	seq_printf(seq, "| Serdes Status   : Rx_Sync(%01ld), AN_Done(%01ld)\n",
		GET_BIT(pbus_data, 4), GET_BIT(pbus_data, 0));
	seq_printf(seq, "| Tx, Rx Polarity : %s(%02d)\n",
		tx_rx_string[tx_rx], tx_rx);

	seq_puts(seq, "\n");

	return 0;
}

static int an8801_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, an8801_info_show, inode->i_private);
}

static const struct file_operations an8801_info_fops = {
	.owner = THIS_MODULE,
	.open = an8801_info_open,
	.read = seq_read,
	.llseek = noop_llseek,
	.release = single_release,
};

static const struct file_operations an8801_counter_fops = {
	.owner = THIS_MODULE,
	.open = an8801_counter_open,
	.read = seq_read,
	.llseek = noop_llseek,
	.release = single_release,
};

static const struct file_operations an8801_debugfs_pbus_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = an8801_debugfs_pbus,
	.llseek = noop_llseek,
};

static const struct file_operations an8801_polarity_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = an8801_polarity_write,
	.llseek = noop_llseek,
};

int an8801_debugfs_init(struct phy_device *phydev)
{
	int ret = 0;
	struct an8801_priv *priv = phydev->priv;

	phydev_info(phydev, "Debugfs init start\n");
	priv->debugfs_root =
		debugfs_create_dir(dev_name(phydev_dev(phydev)), NULL);
	if (!priv->debugfs_root) {
		phydev_err(phydev, "Debugfs init err\n");
		ret = -ENOMEM;
	}
	debugfs_create_file(DEBUGFS_DRIVER_INFO, 0444,
					priv->debugfs_root, phydev,
					&an8801_info_fops);
	debugfs_create_file(DEBUGFS_COUNTER, 0644,
					priv->debugfs_root, phydev,
					&an8801_counter_fops);
	debugfs_create_file(DEBUGFS_PBUS_OP, S_IFREG | 0200,
					priv->debugfs_root, phydev,
					&an8801_debugfs_pbus_fops);
	debugfs_create_file(DEBUGFS_POLARITY, S_IFREG | 0200,
					priv->debugfs_root, phydev,
					&an8801_polarity_fops);
	return ret;
}

static void air_debugfs_remove(struct phy_device *phydev)
{
	struct an8801_priv *priv = phydev->priv;

	if (priv->debugfs_root != NULL) {
		debugfs_remove_recursive(priv->debugfs_root);
		priv->debugfs_root = NULL;
	}
}
#endif /*AN8801SB_DEBUGFS*/

static int an8801_phy_probe(struct phy_device *phydev)
{
	u32 reg_value, phy_id, led_id;
	struct device *dev = &phydev->mdio.dev;
	struct an8801_priv *priv = NULL;

	reg_value = phy_read(phydev, 2);
	phy_id = reg_value << 16;
	reg_value = phy_read(phydev, 3);
	phy_id |= reg_value;
	phydev_info(phydev, "PHY-ID = %x\n", phy_id);

	if (phy_id != AN8801_PHY_ID) {
		phydev_err(phydev, "AN8801 can't be detected.\n");
		return -1;
	}

	priv = devm_kzalloc(dev, sizeof(struct an8801_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	for (led_id = AIR_LED0; led_id < MAX_LED_SIZE; led_id++)
		priv->led_cfg[led_id] = led_cfg_dlt[led_id];

	priv->led_blink_cfg  = led_blink_cfg_dlt;
	priv->rxdelay_force  = rxdelay_force;
	priv->txdelay_force  = txdelay_force;
	priv->rxdelay_step   = rxdelay_step;
	priv->rxdelay_align  = rxdelay_align;
	priv->txdelay_step   = txdelay_step;

	phydev->priv = priv;
#ifdef AN8801SB_DEBUGFS
	reg_value = air_buckpbus_reg_read(phydev, 0x10000094);
	if (0 == (reg_value &
		(AN8801_RG_PKG_SEL_LSB | AN8801_RG_PKG_SEL_MSB))) {
		int ret = 0;

		ret = an8801_debugfs_init(phydev);
		if (ret < 0) {
			air_debugfs_remove(phydev);
			kfree(priv);
			return ret;
		}
	} else {
		phydev_info(phydev, "AN8801R not supprt debugfs\n");
	}
#endif
	return 0;
}

static void an8801_phy_remove(struct phy_device *phydev)
{
	struct an8801_priv *priv = (struct an8801_priv *)phydev->priv;

	if (priv) {
#ifdef AN8801SB_DEBUGFS
		air_debugfs_remove(phydev);
#endif
		kfree(priv);
		phydev_info(phydev, "AN8801 remove OK!\n");
	}
}

static int an8801sb_read_status(struct phy_device *phydev)
{
	int ret, prespeed = phydev->speed;
	u32 reg_value = 0;
	u32 an_retry = MAX_SGMII_AN_RETRY;

	ret = genphy_read_status(phydev);
	if (phydev->link == LINK_DOWN) {
		prespeed = 0;
		phydev->speed = 0;
		ret |= an8801_cl45_write(
			phydev, MMD_DEV_VSPEC2, PHY_PRE_SPEED_REG, prespeed);
	}

	if (prespeed != phydev->speed && phydev->link == LINK_UP) {
		prespeed = phydev->speed;
		ret |= an8801_cl45_write(
			phydev, MMD_DEV_VSPEC2, PHY_PRE_SPEED_REG, prespeed);
		phydev_info(phydev, "AN8801SB SPEED %d\n", prespeed);
		air_buckpbus_reg_write(phydev, 0x10270108, 0x0a0a0404);
		while (an_retry > 0) {
			mdelay(1);       /* delay 1 ms */
			reg_value = air_buckpbus_reg_read(
				phydev, 0x10220b04);
			if (reg_value & AN8801SB_SGMII_AN0_AN_DONE)
				break;
			an_retry--;
		}
		mdelay(10);        /* delay 10 ms */


		if (phydev->autoneg == AUTONEG_DISABLE) {
			phydev_info(phydev,
				"AN8801SB force speed = %d\n", prespeed);
			if (prespeed == SPEED_1000) {
				air_buckpbus_reg_write(
					phydev, 0x10220010, 0xd801);
			} else if (prespeed == SPEED_100) {
				air_buckpbus_reg_write(
					phydev, 0x10220010, 0xd401);
			} else {
				air_buckpbus_reg_write(
					phydev, 0x10220010, 0xd001);
			}

			reg_value = air_buckpbus_reg_read(
				phydev, 0x10220000);
			reg_value |= AN8801SB_SGMII_AN0_ANRESTART;
			air_buckpbus_reg_write(
				phydev, 0x10220000, reg_value);
		}
		reg_value = air_buckpbus_reg_read(phydev, 0x10220000);
		reg_value |= AN8801SB_SGMII_AN0_RESET;
		air_buckpbus_reg_write(phydev, 0x10220000, reg_value);
	}
	return ret;
}

static int an8801r_read_status(struct phy_device *phydev)
{
	int ret, prespeed = phydev->speed;
	u32 data;

	ret = genphy_read_status(phydev);
	if (phydev->link == LINK_DOWN) {
		prespeed = 0;
		phydev->speed = 0;
	}
	if (prespeed != phydev->speed && phydev->link == LINK_UP) {
		prespeed = phydev->speed;
		phydev_dbg(phydev, "AN8801R SPEED %d\n", prespeed);
		if (prespeed == SPEED_1000) {
			data = air_buckpbus_reg_read(phydev, 0x10005054);
			data |= BIT(0);
			air_buckpbus_reg_write(phydev, 0x10005054, data);
		} else {
			data = air_buckpbus_reg_read(phydev, 0x10005054);
			data &= ~BIT(0);
			air_buckpbus_reg_write(phydev, 0x10005054, data);
		}
	}
	return ret;
}

static int an8801_read_status(struct phy_device *phydev)
{
	if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
		an8801sb_read_status(phydev);
	} else if (phydev->interface == PHY_INTERFACE_MODE_RGMII) {
		an8801r_read_status(phydev);
	} else {
		phydev_info(phydev, "AN8801 Phy-mode not support!\n");
		return -1;
	}
	return 0;
}

static struct phy_driver airoha_driver[] = {
	{
		.phy_id         = AN8801_PHY_ID,
		.name           = "Airoha AN8801",
		.phy_id_mask    = 0x0ffffff0,
		.features       = PHY_GBIT_FEATURES,
		.config_init    = an8801_config_init,
		.config_aneg    = genphy_config_aneg,
		.probe          = an8801_phy_probe,
		.remove         = an8801_phy_remove,
		.read_status    = an8801_read_status,
#if (KERNEL_VERSION(4, 5, 0) < LINUX_VERSION_CODE)
		.read_mmd       = __an8801_cl45_read,
		.write_mmd      = __an8801_cl45_write,
#endif
	}
};

module_phy_driver(airoha_driver);

static struct mdio_device_id __maybe_unused airoha_tbl[] = {
	{ AN8801_PHY_ID, 0x0ffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, airoha_tbl);
