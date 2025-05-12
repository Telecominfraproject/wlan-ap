/*
 * STMicroelectronics lsm303agr_mag_i2c.c driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Giuseppe Barba <giuseppe.barba@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#include "lsm303agr_core.h"

#define I2C_AUTO_INCREMENT	0x80

/* XXX: caller must hold cdata->lock */
static int lsm303agr_mag_i2c_read(struct device *dev, u8 reg_addr, int len,
				  u8 *data)
{
	struct i2c_msg msg[2];
	struct i2c_client *client = to_i2c_client(dev);

	if (len > 1)
		reg_addr |= I2C_AUTO_INCREMENT;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	return i2c_transfer(client->adapter, msg, 2);
}

/* XXX: caller must hold cdata->lock */
static int lsm303agr_mag_i2c_write(struct device *dev, u8 reg_addr, int len,
				   u8 *data)
{
	u8 send[len + 1];
	struct i2c_msg msg;
	struct i2c_client *client = to_i2c_client(dev);

	if (len > 1)
		reg_addr |= I2C_AUTO_INCREMENT;

	send[0] = reg_addr;
	memcpy(&send[1], data, len * sizeof(u8));
	len++;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len;
	msg.buf = send;

	return i2c_transfer(client->adapter, &msg, 1);
}

/* I2C IO routines */
static const struct lsm303agr_transfer_function lsm303agr_mag_i2c_tf = {
	.write = lsm303agr_mag_i2c_write,
	.read = lsm303agr_mag_i2c_read,
};

#ifdef CONFIG_PM_SLEEP
static int lsm303agr_mag_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm303agr_common_data *cdata = i2c_get_clientdata(client);

	if (cdata->on_before_suspend)
		return lsm303agr_mag_enable(cdata);
	return 0;
}

static int lsm303agr_mag_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm303agr_common_data *cdata = i2c_get_clientdata(client);

	cdata->on_before_suspend = atomic_read(&cdata->enabled);
	return lsm303agr_mag_disable(cdata);
}

static SIMPLE_DEV_PM_OPS(lsm303agr_mag_pm_ops,
				lsm303agr_mag_suspend,
				lsm303agr_mag_resume);

#define LSM303AGR_MAG_PM_OPS	(&lsm303agr_mag_pm_ops)
#else /* CONFIG_PM_SLEEP */
#define LSM303AGR_MAG_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static int lsm303agr_mag_i2c_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int err;
	struct lsm303agr_common_data *cdata;

	dev_info(&client->dev, "probe start.\n");

	/* Alloc Common data structure */
	cdata = kzalloc(sizeof(struct lsm303agr_common_data), GFP_KERNEL);
	if (cdata == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		mutex_unlock(&cdata->lock);

		return -ENOMEM;
	}

	cdata->sensor_num = LSM303AGR_MAX_SENSORS_NUM;
	cdata->dev = &client->dev;
	cdata->name = client->name;
	cdata->bus_type = BUS_I2C;
	cdata->tf = &lsm303agr_mag_i2c_tf;

	i2c_set_clientdata(client, cdata);

	mutex_init(&cdata->lock);

	err = lsm303agr_mag_probe(cdata);
	if (err < 0) {
		kfree(cdata);

		return err;
	}

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void lsm303agr_mag_i2c_remove(struct i2c_client *client)
{
	struct lsm303agr_common_data *cdata = i2c_get_clientdata(client);

	dev_info(cdata->dev, "driver removing\n");

	lsm303agr_mag_remove(cdata);
	kfree(cdata);
}
#else
static int lsm303agr_mag_i2c_remove(struct i2c_client *client)
{
	struct lsm303agr_common_data *cdata = i2c_get_clientdata(client);

	dev_info(cdata->dev, "driver removing\n");

	lsm303agr_mag_remove(cdata);
	kfree(cdata);

	return 0;
}
#endif

static const struct i2c_device_id lsm303agr_mag_i2c_id[] = {
	{ "lsm303agr_mag", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, lsm303agr_mag_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id lsm303agr_mag_i2c_id_table[] = {
	{.compatible = "st,lsm303agr_mag", },
	{ },
};
MODULE_DEVICE_TABLE(of, lsm303agr_mag_i2c_id_table);
#endif /* CONFIG_OF */

static struct i2c_driver lsm303agr_mag_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "lsm303agr_mag",
		.pm = LSM303AGR_MAG_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = lsm303agr_mag_i2c_id_table,
#endif /* CONFIG_OF */
	},
	.probe = lsm303agr_mag_i2c_probe,
	.remove = lsm303agr_mag_i2c_remove,
	.id_table = lsm303agr_mag_i2c_id,
};

module_i2c_driver(lsm303agr_mag_i2c_driver);

MODULE_DESCRIPTION("lsm303agr magnetometer i2c driver");
MODULE_AUTHOR("Armando Visconti");
MODULE_AUTHOR("Matteo Dameno");
MODULE_AUTHOR("Denis Ciocca");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL v2");

