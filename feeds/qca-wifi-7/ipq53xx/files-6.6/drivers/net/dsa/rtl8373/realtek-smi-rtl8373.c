// SPDX-License-Identifier: GPL-2.0 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/bitops.h>
#include <linux/if_bridge.h>
#include <linux/version.h>

#include "realtek-smi-rtl8373.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "rtl8373_switch.h"

struct dsa_switch *g_rtl8373_smi_ds = NULL;

#define REALTEK_SMI_ACK_RETRY_COUNT		5
#define REALTEK_SMI_HW_STOP_DELAY		25	/* msecs */
#define REALTEK_SMI_HW_START_DELAY		100	/* msecs */


#if defined(CONFIG_MST_MDC_MDIO)
#define MDC_MDIO_CTRL_REG           21
#define MDC_MDIO_ADDR_REG           22
#define MDC_MDIO_DATA_LOW           23
#define MDC_MDIO_DATA_HIGH          24
#define MDC_MDIO_READ_CMD           0x1B
#define MDC_MDIO_WRITE_CMD          0x19

static int realtek_smi_write(void *ctx, u32 reg, u32 val)
{
    struct realtek_smi *smi = (struct realtek_smi *) ctx;
    struct mii_bus *bus = smi->bus;
    uint32_t regval;

    /* Lock */
    mutex_lock(&bus->mdio_lock);

    /*Ckeck busy bit, register21, bit2*/
    regval = __mdiobus_read(bus, smi->mdio_addr, MDC_MDIO_CTRL_REG);
    if (regval & 0x4) {
        mutex_unlock(&bus->mdio_lock);
        return -EBUSY;
    }

    /*write reg address, register22*/
    __mdiobus_write(bus, smi->mdio_addr, MDC_MDIO_ADDR_REG, reg);

    /*write low 16bits data, register23*/
    __mdiobus_write(bus, smi->mdio_addr, MDC_MDIO_DATA_LOW, val & 0xFFFF);

    /*write high 16bits data, register24*/
    __mdiobus_write(bus, smi->mdio_addr, MDC_MDIO_DATA_HIGH, (val >> 16) & 0xFFFF);

    /*write  write cmd,  register 21*/
    __mdiobus_write(bus, smi->mdio_addr, MDC_MDIO_CTRL_REG, MDC_MDIO_WRITE_CMD);

    /*Ckeck busy bit, register21, bit2*/
    regval = __mdiobus_read(bus, smi->mdio_addr, MDC_MDIO_CTRL_REG);
    if (regval & 0x4) {
        mutex_unlock(&bus->mdio_lock);
        return -EBUSY;
    }

    mutex_unlock(&bus->mdio_lock);

    return 0;
}

static int realtek_smi_read(void *ctx, u32 reg, u32 *val)
{
    struct realtek_smi *smi = (struct realtek_smi *) ctx;
    struct mii_bus *bus = smi->bus;
    uint16_t lo, hi;
    uint32_t regval;

    /* Lock */
    mutex_lock(&bus->mdio_lock);

    /*Ckeck busy bit, register21, bit2*/
    regval = __mdiobus_read(bus, smi->mdio_addr, MDC_MDIO_CTRL_REG);
    if (regval & 0x4) {
        mutex_unlock(&bus->mdio_lock);
        return -EBUSY;
    }

    /*write reg address, register22*/
    __mdiobus_write(bus, smi->mdio_addr, MDC_MDIO_ADDR_REG, reg);

    /* Write read command   register21 */
    __mdiobus_write(bus, smi->mdio_addr, MDC_MDIO_CTRL_REG, MDC_MDIO_READ_CMD);

    /*Ckeck busy bit, register21, bit2*/
    regval = __mdiobus_read(bus, smi->mdio_addr, MDC_MDIO_CTRL_REG);
    if (regval & 0x4) {
        mutex_unlock(&bus->mdio_lock);
        return -EBUSY;
    }

    /* Write read control code to register 23 */
    lo = __mdiobus_read(bus, smi->mdio_addr, MDC_MDIO_DATA_LOW);

    /* Read data from register 24 */
    hi = __mdiobus_read(bus, smi->mdio_addr, MDC_MDIO_DATA_HIGH);

    mutex_unlock(&bus->mdio_lock);

    *val = (hi << 16) | lo;

    return 0;
}
#else
static inline void realtek_smi_clk_delay(struct realtek_smi *smi)
{
	ndelay(smi->clk_delay);
}

static void realtek_smi_start(struct realtek_smi *smi)
{
	/* Set GPIO pins to output mode, with initial state:
	 * SCK = 0, SDA = 1
	 */
	gpiod_direction_output(smi->mdc, 0);
	gpiod_direction_output(smi->mdio, 1);
	realtek_smi_clk_delay(smi);

	/* CLK 1: 0 -> 1, 1 -> 0 */
	gpiod_set_value(smi->mdc, 1);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdc, 0);
	realtek_smi_clk_delay(smi);

	/* CLK 2: */
	gpiod_set_value(smi->mdc, 1);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdio, 0);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdc, 0);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdio, 1);
}

static void realtek_smi_stop(struct realtek_smi *smi)
{
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdio, 0);
	gpiod_set_value(smi->mdc, 1);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdio, 1);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdc, 1);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdc, 0);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdc, 1);

	/* Add a click */
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdc, 0);
	realtek_smi_clk_delay(smi);
	gpiod_set_value(smi->mdc, 1);

	/* Set GPIO pins to input mode */
	gpiod_direction_input(smi->mdio);
	gpiod_direction_input(smi->mdc);
}

static void realtek_smi_write_bits(struct realtek_smi *smi, u32 data, u32 len)
{
	for (; len > 0; len--) {
		realtek_smi_clk_delay(smi);

		/* Prepare data */
		gpiod_set_value(smi->mdio, !!(data & (1 << (len - 1))));
		realtek_smi_clk_delay(smi);

		/* Clocking */
		gpiod_set_value(smi->mdc, 1);
		realtek_smi_clk_delay(smi);
		gpiod_set_value(smi->mdc, 0);
	}
}

static void realtek_smi_read_bits(struct realtek_smi *smi, u32 len, u32 *data)
{
	gpiod_direction_input(smi->mdio);

	for (*data = 0; len > 0; len--) {
		u32 u;

		realtek_smi_clk_delay(smi);

		/* Clocking */
		gpiod_set_value(smi->mdc, 1);
		realtek_smi_clk_delay(smi);
		u = !!gpiod_get_value(smi->mdio);
		gpiod_set_value(smi->mdc, 0);

		*data |= (u << (len - 1));
	}

	gpiod_direction_output(smi->mdio, 0);
}

static int realtek_smi_wait_for_ack(struct realtek_smi *smi)
{
	int retry_cnt;

	retry_cnt = 0;
	do {
		u32 ack;

		realtek_smi_read_bits(smi, 1, &ack);
		if (ack == 0)
			break;

		if (++retry_cnt > REALTEK_SMI_ACK_RETRY_COUNT) {
			dev_err(smi->dev, "ACK timeout\n");
			return -ETIMEDOUT;
		}
	} while (1);

	return 0;
}

static int realtek_smi_write_byte(struct realtek_smi *smi, u8 data)
{
	realtek_smi_write_bits(smi, data, 8);
	return realtek_smi_wait_for_ack(smi);
}

static int realtek_smi_write_byte_noack(struct realtek_smi *smi, u8 data)
{
	realtek_smi_write_bits(smi, data, 8);
	return 0;
}

static int realtek_smi_read_byte0(struct realtek_smi *smi, u8 *data)
{
	u32 t;

	/* Read data */
	realtek_smi_read_bits(smi, 8, &t);
	*data = (t & 0xff);

	/* Send an ACK */
	realtek_smi_write_bits(smi, 0x00, 1);

	return 0;
}

static int realtek_smi_read_byte1(struct realtek_smi *smi, u8 *data)
{
	u32 t;

	/* Read data */
	realtek_smi_read_bits(smi, 8, &t);
	*data = (t & 0xff);

	/* Send an ACK */
	realtek_smi_write_bits(smi, 0x00, 1);

	return 0;
}

static int realtek_smi_read_byte2(struct realtek_smi *smi, u8 *data)
{
	u32 t;

	/* Read data */
	realtek_smi_read_bits(smi, 8, &t);
	*data = (t & 0xff);

	/* Send an ACK */
	realtek_smi_write_bits(smi, 0x00, 1);

	return 0;
}

static int realtek_smi_read_byte3(struct realtek_smi *smi, u8 *data)
{
	u32 t;

	/* Read data */
	realtek_smi_read_bits(smi, 8, &t);
	*data = (t & 0xff);

	/* Send an ACK */
	realtek_smi_write_bits(smi, 0x01, 1);

	return 0;
}



static int realtek_smi_read_reg(struct realtek_smi *smi, u32 addr, u32 *data)
{
	unsigned long flags;
	u8 d0 = 0;
	u8 d1 = 0;
	u8 d2 = 0;
	u8 d3= 0;
	int ret;

	spin_lock_irqsave(&smi->lock, flags);

	realtek_smi_start(smi);

	/* Send READ command */
	ret = realtek_smi_write_byte(smi, smi->cmd_read);
	if (ret)
		goto out;

	/* Set ADDR[7:0] */
	ret = realtek_smi_write_byte(smi, addr & 0xff);
	if (ret)
		goto out;

	/* Set ADDR[15:8] */
	ret = realtek_smi_write_byte(smi, addr >> 8);
	if (ret)
		goto out;

	/* Read DATA[7:0] */
	realtek_smi_read_byte0(smi, &d0);
	/* Read DATA[15:8] */
	realtek_smi_read_byte1(smi, &d1);
	/* Read DATA[23:16] */
	realtek_smi_read_byte2(smi, &d2);
	/* Read DATA[31:24] */
	realtek_smi_read_byte3(smi, &d3);

	*data = ((u32)d0) | (((u32)d1) << 8)| (((u32)d2) << 16)| (((u32)d3) << 24);

	ret = 0;

 out:
	realtek_smi_stop(smi);
	spin_unlock_irqrestore(&smi->lock, flags);

	return ret;
}

static int realtek_smi_write_reg(struct realtek_smi *smi,
				 u32 addr, u32 data, bool ack)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&smi->lock, flags);

	realtek_smi_start(smi);

	ret = realtek_smi_write_byte(smi, smi->cmd_write);
	if (ret)
		goto out;

	ret = realtek_smi_write_byte(smi, addr & 0xff);
	if (ret)
		goto out;

	ret = realtek_smi_write_byte(smi, addr >> 8);
	if (ret)
		goto out;

	ret = realtek_smi_write_byte(smi, data & 0xff);
	if (ret)
		goto out;

	if (ack)
		ret = realtek_smi_write_byte(smi, (data >> 8) & 0xff);
	else
		ret = realtek_smi_write_byte_noack(smi, (data >> 8) & 0xff);
	if (ret)
		goto out;

	if (ack)
		ret = realtek_smi_write_byte(smi, (data >> 16) & 0xff);
	else
		ret = realtek_smi_write_byte_noack(smi, (data >> 16) & 0xff);
	if (ret)
		goto out;

	if (ack)
		ret = realtek_smi_write_byte(smi, (data >> 24) & 0xff);
	else
		ret = realtek_smi_write_byte_noack(smi, (data >> 24) & 0xff);
	if (ret)
		goto out;

	ret = 0;

out:
	realtek_smi_stop(smi);
	spin_unlock_irqrestore(&smi->lock, flags);

	return ret;
}


/* There is one single case when we need to use this accessor and that
 * is when issueing soft reset. Since the device reset as soon as we write
 * that bit, no ACK will come back for natural reasons.
 */
int realtek_smi_write_reg_noack(struct realtek_smi *smi, u32 addr,
				u32 data)
{
	return realtek_smi_write_reg(smi, addr, data, false);
}
EXPORT_SYMBOL_GPL(realtek_smi_write_reg_noack);

/* Regmap accessors */

static int realtek_smi_write(void *ctx, u32 reg, u32 val)
{
	struct realtek_smi *smi = ctx;

	return realtek_smi_write_reg(smi, reg, val, true);
}

static int realtek_smi_read(void *ctx, u32 reg, u32 *val)
{
	struct realtek_smi *smi = ctx;

	return realtek_smi_read_reg(smi, reg, val);
}
#endif

static const struct regmap_config realtek_smi_mdio_regmap_config = {
	.reg_bits = 10, /* A4..A0 R4..R0 */
	.val_bits = 32,
	.reg_stride = 1,
	/* PHY regs are at 0x8000 */
	.max_register = 0xffff,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.reg_read = realtek_smi_read,
	.reg_write = realtek_smi_write,
	.cache_type = REGCACHE_NONE,
};

#ifndef MII_ADDR_C45
#define MII_ADDR_C45		(1<<30)
#endif
#define DEVADDR(addr) (((addr) >> 16) & 0x1f)
#define REGNUM(addr) ((addr) & 0xffff)
static int realtek_smi_mdio_read(struct mii_bus *bus, int addr, int regnum)
{
	struct realtek_smi *smi = bus->priv;
	unsigned int data = 0;
	bool is_c45 = !!(regnum & MII_ADDR_C45);
	int ret;

	if (is_c45) {
		ret = smi->ops->phyc45_read(smi, addr,
					    DEVADDR(regnum),
					    REGNUM(regnum),
					    &data);
		if (ret)
			return ret;
		return (int)data;
	}

	return smi->ops->phy_read(smi, addr, regnum);
}


static int realtek_smi_mdio_write(struct mii_bus *bus, int addr, int regnum,
				  u16 val)
{
	struct realtek_smi *smi = bus->priv;
	bool is_c45 = !!(regnum & MII_ADDR_C45);
	int ret;

	//printk("%s(%d)XXX PHY 0x%x 0x%x 0x%x\n",__func__,__LINE__, addr, regnum, val);
	if (is_c45) 
	{
		ret = smi->ops->phyc45_write(smi, addr, DEVADDR(regnum), REGNUM(regnum), val);
		return ret;
	}
	else
	{
		ret = smi->ops->phy_write(smi, addr, regnum, val);
		return ret;
	}
}

static int realtek_smi_mdio_read_c45(struct mii_bus *bus, int addr, int devnum, int regnum)
{
	struct realtek_smi *smi = bus->priv;
	unsigned int data = 0;
	int ret;
	
	ret = smi->ops->phyc45_read(smi, addr,
				    devnum,
				    regnum,
				    &data);
	//printk("%s(%d) addr: 0x%x, dev: 0x%x, reg: 0x%x, data: 0x%x, ret: %d\n",__func__,__LINE__, addr, devnum, regnum, data, ret);
	if (ret)
		return ret;
	return (int)data;
}

static int realtek_smi_mdio_write_c45(struct mii_bus *bus, int addr, int devnum, int regnum, u16 val)
{
	struct realtek_smi *smi = bus->priv;
	int ret;

	ret = smi->ops->phyc45_write(smi, 1<<addr, devnum, regnum, val);
	//printk("%s(%d), addr: 0x%x, dev: 0x%x, reg: 0x%x, val: 0x%x, ret: %d\n",__func__,__LINE__, addr, devnum, regnum, val, ret);
	return ret;
}

int realtek_smi_setup_mdio(struct realtek_smi *smi)
{
	struct device_node *mdio_np;
	int ret;

	mdio_np = of_get_compatible_child(smi->dev->of_node, "realtek,smi-mdio");
	if (!mdio_np) {
		dev_err(smi->dev, "no MDIO bus node\n");
		return -ENODEV;
	}

	smi->slave_mii_bus = devm_mdiobus_alloc(smi->dev);
	if (!smi->slave_mii_bus) {
		ret = -ENOMEM;
		goto err_put_node;
	}
	smi->slave_mii_bus->priv = smi;
	smi->slave_mii_bus->name = "SMI slave MII";
	smi->slave_mii_bus->read = realtek_smi_mdio_read;
	smi->slave_mii_bus->write = realtek_smi_mdio_write;
	//[CBT]	for init extphy
	smi->slave_mii_bus->read_c45 = realtek_smi_mdio_read_c45;
	smi->slave_mii_bus->write_c45 = realtek_smi_mdio_write_c45;
	
	snprintf(smi->slave_mii_bus->id, MII_BUS_ID_SIZE, "SMI-%d",
		 smi->ds->index);

	smi->slave_mii_bus->dev.of_node = mdio_np;
	smi->slave_mii_bus->parent = smi->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0)
	smi->slave_mii_bus->probe_capabilities =  MDIOBUS_C22;
#endif
	smi->slave_mii_bus->phy_mask = ~0x0U; /* probe all PHY IDs */
	smi->ds->slave_mii_bus = smi->slave_mii_bus;

	ret = of_mdiobus_register(smi->slave_mii_bus, mdio_np);
	if (ret) {
		dev_err(smi->dev, "unable to register MDIO bus %s\n",
			smi->slave_mii_bus->id);
		goto err_put_node;
	}

	return 0;

err_put_node:
	of_node_put(mdio_np);

	return ret;
}

#if defined(CONFIG_MST_MDC_MDIO)
#define MAX_RTK_SMI_PROC_NUM		50
struct proc_dir_entry *realtek_smi_proc_dir = NULL;
#endif

#if defined(CONFIG_MST_MDC_MDIO)
static int realtek_smi_probe(struct mdio_device *pdev)
#else
static int realtek_smi_probe(struct platform_device *pdev)
#endif
{
	const struct realtek_smi_variant *var;
	struct device *dev = &pdev->dev;
	struct realtek_smi *smi;
	struct device_node *np;
	int ret;

	var = of_device_get_match_data(dev);
	if (!var)
		return -EINVAL;

	np = dev->of_node;

	smi = devm_kzalloc(dev, sizeof(*smi) + var->chip_data_sz, GFP_KERNEL);
	if (!smi)
		return -ENOMEM;

	smi->chip_data = (void *)smi + sizeof(*smi);
	smi->dev = dev;
	smi->clk_delay = var->clk_delay;
	smi->cmd_read = var->cmd_read;
	smi->cmd_write = var->cmd_write;
	smi->ops = var->ops;
	
	smi->map = devm_regmap_init(dev, NULL, smi,
				    &realtek_smi_mdio_regmap_config);
	if (IS_ERR(smi->map)) {
		ret = PTR_ERR(smi->map);
		dev_err(dev, "regmap init failed: %d\n", ret);
		return ret;
	}
	smi->map_nolock = smi->map;

	smi->svlan_disable = of_property_read_bool(np, "svlan-disable");
	if (smi->svlan_disable)
		dev_info(dev, "default SVLAN function disabled by DTS\n");

#if defined(CONFIG_MST_MDC_MDIO)
	smi->bus = pdev->bus;
	smi->mdio_addr = pdev->addr;
#endif

	dev_set_drvdata(dev, smi);
	spin_lock_init(&smi->lock);
	mutex_init(&smi->map_lock);

#if !defined(CONFIG_MST_MDC_MDIO)
	smi->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(smi->reset)) {
		dev_err(dev, "failed to get RESET GPIO\n");
		return PTR_ERR(smi->reset);
	}

	msleep(REALTEK_SMI_HW_STOP_DELAY);
	if (smi->reset)
		gpiod_set_value(smi->reset, 0);
	msleep(REALTEK_SMI_HW_START_DELAY);

	smi->mdc = devm_gpiod_get_optional(dev, "mdc", GPIOD_OUT_LOW);
	if (IS_ERR(smi->mdc))
		return PTR_ERR(smi->mdc);

	smi->mdio = devm_gpiod_get_optional(dev, "mdio", GPIOD_OUT_LOW);
	if (IS_ERR(smi->mdio))
		return PTR_ERR(smi->mdio);

	smi->leds_disabled = of_property_read_bool(np, "realtek,disable-leds");
#endif

	ret = smi->ops->detect(smi);
	if (ret)
		return ret;

	smi->ds = devm_kzalloc(dev, sizeof(*smi->ds), GFP_KERNEL);
	if (!smi->ds)
		return -ENOMEM;

	smi->ds->dev = dev;
	smi->ds->num_ports = smi->num_ports;
	smi->ds->priv = smi;
	smi->ds->ops = var->ds_ops;

#if defined(CONFIG_MST_MDC_MDIO)
	ret = realtek_smi_setup_mdio(smi);
	if (ret) {
		dev_err(dev, "realtek_smi_setup_mdio failed, ret=%d\n", ret);
		return ret;
	}
#endif

	ret = dsa_register_switch(smi->ds);
	if (ret) {
		dev_err(dev, "unable to register switch ret=%d\n", ret);
		return ret;
	}

	if (smi->ops->setup_rest) {
		ret = smi->ops->setup_rest(smi->ds);
		if (ret) {
			dev_err(dev, "setup_rest failed ret=%d\n", ret);
			dsa_unregister_switch(smi->ds);
			return ret;
		}
	}

	dev_info(dev, "dsa_register_switch success\n");
	return 0;
}


#if defined(CONFIG_MST_MDC_MDIO)
static void realtek_smi_remove(struct mdio_device *pdev)
#else
static int realtek_smi_remove(struct platform_device *pdev)
#endif
{
	struct realtek_smi *smi = dev_get_drvdata(&pdev->dev);

	if (!smi)
#if !defined(CONFIG_MST_MDC_MDIO)
		return 0;
#else
		return;
#endif

	if (smi->ds)
		dsa_unregister_switch(smi->ds);

	if (smi->slave_mii_bus)
		of_node_put(smi->slave_mii_bus->dev.of_node);

#if !defined(CONFIG_MST_MDC_MDIO)
	if (smi->reset)
		gpiod_set_value(smi->reset, 1);
	return 0;
#endif
}


static const struct of_device_id realtek_smi_of_match[] = {
	{
		/* FIXME: add support for RTL8373 and more */
		.compatible = "realtek,rtl8373",
		.data = &rtl8373_variant,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, realtek_smi_of_match);

#if defined(CONFIG_MST_MDC_MDIO)

static struct mdio_driver realtek_smi_mdio_driver = {
	.probe = realtek_smi_probe,
	.remove = realtek_smi_remove,
	.mdiodrv.driver = {
		.name = "realtek-smi",
		.of_match_table = of_match_ptr(realtek_smi_of_match),
	},
};
mdio_module_driver(realtek_smi_mdio_driver);

#else

static struct platform_driver realtek_smi_driver = {
	.driver = {
		.name = "realtek-smi",
		.of_match_table = of_match_ptr(realtek_smi_of_match),
	},
	.probe  = realtek_smi_probe,
	.remove = realtek_smi_remove,
};
module_platform_driver(realtek_smi_driver);

#endif

MODULE_LICENSE("GPL");
