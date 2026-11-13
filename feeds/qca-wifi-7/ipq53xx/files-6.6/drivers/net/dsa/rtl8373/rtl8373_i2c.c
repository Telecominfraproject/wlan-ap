// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/bitops.h>
#include <linux/if_bridge.h>
#include <linux/i2c.h>
#include <linux/of.h>

#include "realtek-smi-rtl8373.h"
#include "rtl8373_core.h"
#include "rtl8373_switch.h"

struct rtl8373_i2c {
	struct i2c_client *device;
	struct realtek_smi chip;
};

static int rtl8373_i2c_reg_read(void *context, unsigned int reg,
				unsigned int *val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	u8 buf[6];
	int ret;

	buf[0] = reg & 0xFF;        // Lower byte
	buf[1] = (reg >> 8) & 0xFF; // Upper byte

	ret = i2c_master_send(i2c, buf, 2);
	if (ret < 0) {
		dev_err(dev, "i2c_master_send returned %d\n", ret);
		return ret;
	}

	ret = i2c_master_recv(i2c, buf, 4);
	if (ret < 0) {
		dev_err(dev, "i2c_master_recv returned %d\n", ret);
		return ret;
	}

	*val = le32_to_cpu((buf[0] << 0) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
	return 0;
}

static int rtl8373_i2c_reg_write(void *context, unsigned int reg,
				 unsigned int val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	u8 buf[6];  // 2 bytes for the register and 4 bytes for the value
	int ret;

	// Place the register address into the buffer, assuming little-endian
	buf[0] = reg & 0xFF;        // Lower byte of the register
	buf[1] = (reg >> 8) & 0xFF; // Upper byte of the register

	// Place the value into the buffer, assuming little-endian
	buf[2] = val & 0xFF;         // Lowest byte of the value
	buf[3] = (val >> 8) & 0xFF;  // Second byte of the value
	buf[4] = (val >> 16) & 0xFF; // Third byte of the value
	buf[5] = (val >> 24) & 0xFF; // Highest byte of the value

	// Send the register address followed by the value
	ret = i2c_master_send(i2c, buf, 6); // Sending 6 bytes: 2 for address, 4 for value
	if (ret < 0) {
		dev_err(dev, "i2c_master_send returned %d\n", ret);
		return ret;
	}

	return 0;
}


int rtl8373_remove(struct realtek_smi *chip)
{
	dsa_unregister_switch(chip->ds);
	return 0;
}
EXPORT_SYMBOL(rtl8373_remove);

void rtl8373_shutdown(struct realtek_smi *chip)
{
	dsa_switch_suspend(chip->ds);
}
EXPORT_SYMBOL(rtl8373_shutdown);
static const struct regmap_config rtl8373_i2c_regmap_config = {
	.val_bits = 32,
	.reg_stride = 2,
	.reg_bits = 16,
	.pad_bits = 0,
	.write_flag_mask = 0,
	.read_flag_mask = 0,
	.reg_read = rtl8373_i2c_reg_read,
	.reg_write = rtl8373_i2c_reg_write,
	.max_register = 0,
	.cache_type = REGCACHE_NONE,
	.reg_format_endian = REGMAP_ENDIAN_LITTLE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE
};

static int rtl8373_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct rtl8373_i2c *sw_dev;
	int ret;
	const struct realtek_smi_variant *var;

	var = of_device_get_match_data(&client->dev);
	if (!var)
		return -ENODEV;
	sw_dev = devm_kzalloc(&client->dev, sizeof(struct rtl8373_i2c) + var->chip_data_sz,
			      GFP_KERNEL);
	if (!sw_dev)
		return -ENOMEM;

	sw_dev->chip.map = devm_regmap_init_i2c(client,
						   &rtl8373_i2c_regmap_config);
	if (IS_ERR(sw_dev->chip.map)) {
		ret = PTR_ERR(sw_dev->chip.map);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}
	sw_dev->chip.map_nolock = sw_dev->chip.map;
	sw_dev->chip.chip_data = (void *)sw_dev + sizeof(struct rtl8373_i2c);
	/* link forward and backward */
	sw_dev->device = client;
	i2c_set_clientdata(client, sw_dev);
	sw_dev->chip.dev = &client->dev;


	ret = rtl8373_probe(&sw_dev->chip);
	if (ret != 0)
		return ret;

	dev_info(&client->dev, "rtl8373 I2C driver loaded successfully\n");

	return 0;
}

static int rtl8373_i2c_remove(struct i2c_client *client)
{
	struct rtl8373_i2c *sw_dev = i2c_get_clientdata(client);

	if (!sw_dev)
		return 0;

	rtl8373_remove(&sw_dev->chip);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static void rtl8373_i2c_shutdown(struct i2c_client *client)
{
	struct rtl8373_i2c *sw_dev = i2c_get_clientdata(client);

	if (!sw_dev)
		return;

	rtl8373_shutdown(&sw_dev->chip);

	i2c_set_clientdata(client, NULL);
}

/*-------------------------------------------------------------------------*/

static const struct i2c_device_id rtl8373_i2c_id[] = {
	{ "rtl8373", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, rtl8373_i2c_id);

static const struct of_device_id rtl8373_i2c_of_match[] = {
	{ 
		.compatible = "realtek,rtl8373-i2c", 
		.data = &rtl8373_variant,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rtl8373_i2c_of_match);

static struct i2c_driver rtl8373_i2c_driver = {
	.driver = {
		.name = "rtl8373_I2C",
		.of_match_table = of_match_ptr(rtl8373_i2c_of_match),
	},
	.probe = rtl8373_i2c_probe,
	.remove = rtl8373_i2c_remove,
	.shutdown = rtl8373_i2c_shutdown,
	.id_table = rtl8373_i2c_id,
};
module_i2c_driver(rtl8373_i2c_driver);

MODULE_LICENSE("GPL v2");
