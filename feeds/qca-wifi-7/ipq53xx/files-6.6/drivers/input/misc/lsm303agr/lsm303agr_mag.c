/*
 * STMicroelectronics lsm303agr_mag.c driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Giuseppe Barba <giuseppe.barba@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#include "lsm303agr_core.h"

#define LSM303AGR_MAG_DEV_NAME	"lsm303agr_mag"

/* DEVICE REGISTERS */
#define WHO_AM_I		0x4F
#define CFG_REG_A		0x60
#define AXISDATA_REG		0x68
#define WHOAMI_LSM303AGR_MAG	0x40
/* Device operating modes */
#define MD_CONTINUOS_MODE	0x00
#define MD_SINGLE_MODE		0x01
#define MD_IDLE1_MODE		0x02
#define MD_IDLE2_MODE		0x03
#define LSM303AGR_MAG_MODE_MSK	0x03

/* Device ODRs */
#define LSM303AGR_MAG_ODR10_HZ		0x00
#define LSM303AGR_MAG_ODR20_HZ		0x04
#define LSM303AGR_MAG_ODR50_HZ		0x08
#define LSM303AGR_MAG_ODR100_HZ		0x0C
#define LSM303AGR_MAG_ODR_MSK		0x0C

#define LSM303AGR_MAG_SENSITIVITY	1500	/* uGa/LSB */

/* ODR table */
struct {
	u32 time_ms;
	u32 reg_val;
} lsm303agr_mag_odr_table[] = {
	{   10, LSM303AGR_MAG_ODR100_HZ	}, /* ODR = 100Hz */
	{   20, LSM303AGR_MAG_ODR50_HZ	}, /* ODR = 50Hz */
	{   50, LSM303AGR_MAG_ODR20_HZ	}, /* ODR = 20Hz */
	{  100, LSM303AGR_MAG_ODR10_HZ	}, /* ODR = 10Hz */
};

/* read and write with mask a given register */
static int lsm303agr_mag_write_data_with_mask(struct lsm303agr_common_data *cdata,
					      u8 reg_addr, u8 mask, u8 *data)
{
	int err;
	u8 new_data, old_data = 0;

	err = cdata->tf->read(cdata->dev, reg_addr, 1, &old_data);
	if (err < 0)
		return err;

	new_data = ((old_data & (~mask)) | ((*data) & mask));

#ifdef LSM303AGR_MAG_DEBUG
	dev_info(cdata->dev, "%s %02x o=%02x d=%02x n=%02x\n",
		 LSM303AGR_MAG_DEV_NAME, reg_addr, old_data, *data, new_data);
#endif

	/* Save for caller usage the data that is about to be written */
	*data = new_data;

	if (new_data == old_data)
		return 1;

	return cdata->tf->write(cdata->dev, reg_addr, 1, &new_data);
}

int lsm303agr_mag_input_init(struct lsm303agr_sensor_data *sdata,
			     const char* description)
{
	int err;

	sdata->input_dev = input_allocate_device();
	if (!sdata->input_dev) {
		dev_err(sdata->cdata->dev, "input device allocation failed\n");
		return -ENOMEM;
	}

	sdata->input_dev->name = description;
	sdata->input_dev->id.bustype = sdata->cdata->bus_type;
	sdata->input_dev->dev.parent = sdata->cdata->dev;

	input_set_drvdata(sdata->input_dev, sdata);

	/* Set the input event characteristics of the probed sensor driver */
	set_bit(INPUT_EVENT_TYPE, sdata->input_dev->evbit );
	set_bit(INPUT_EVENT_TIME_MSB, sdata->input_dev->mscbit);
	set_bit(INPUT_EVENT_TIME_LSB, sdata->input_dev->mscbit);
	set_bit(INPUT_EVENT_X, sdata->input_dev->mscbit);
	set_bit(INPUT_EVENT_Y, sdata->input_dev->mscbit);
	set_bit(INPUT_EVENT_Z, sdata->input_dev->mscbit);

	err = input_register_device(sdata->input_dev);
	if (err) {
		dev_err(sdata->cdata->dev,
			"unable to register input device %s\n",
			sdata->input_dev->name);
		input_free_device(sdata->input_dev);
		return err;
	}

	return 0;
}

/* Check if WHO_AM_I is correct */
static int lsm303agr_mag_check_wai(struct lsm303agr_common_data *cdata)
{
	int err;
	u8 wai;

#ifdef LSM303AGR_MAG_DEBUG
	pr_info("%s: check WAI start\n", LSM303AGR_MAG_DEV_NAME);
#endif

	err = cdata->tf->read(cdata->dev, WHO_AM_I, 1, &wai);
	if (err < 0) {
		dev_warn(cdata->dev,
		"Error reading WHO_AM_I: is device available/working?\n");
		goto error;
	}

	if (wai != WHOAMI_LSM303AGR_MAG) {
		dev_err(cdata->dev,
			"device unknown. Expected: 0x%02x," " Replies: 0x%02x\n",
			WHOAMI_LSM303AGR_MAG, wai);
		err = -1; /* choose the right coded error */
		goto error;
	}

	cdata->hw_initialized = 1;
#ifdef LSM303AGR_MAG_DEBUG
	pr_info("%s: check WAI done\n", LSM303AGR_MAG_DEV_NAME);
#endif
	return 0;

error:
	cdata->hw_initialized = 0;
	dev_err(cdata->dev,
		"check WAI error 0x%02x,0x%02x: %d\n", WHO_AM_I, wai, err);

	return err;
}

static int lsm303agr_mag_set_odr(struct lsm303agr_common_data *cdata, u8 odr)
{
	u8 odr_reg;
	int err = -1, i;
	struct lsm303agr_sensor_data *sdata;

	/**
	 * Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward
	 * (longest interval), to support the poll_interval requested
	 * by the system. It must be the longest interval lower
	 * than the poll interval
	 */
	for (i = ARRAY_SIZE(lsm303agr_mag_odr_table) - 1; i >= 0; i--) {
		if ((lsm303agr_mag_odr_table[i].time_ms <= odr) || (i == 0))
			break;
	}

	odr_reg = lsm303agr_mag_odr_table[i].reg_val;
	sdata = &cdata->sensors[LSM303AGR_MAG_SENSOR];
	sdata->poll_interval = sdata->c_odr = lsm303agr_mag_odr_table[i].time_ms;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&cdata->enabled)) {
		err = lsm303agr_mag_write_data_with_mask(cdata, CFG_REG_A,
							 LSM303AGR_MAG_ODR_MSK,
							 &odr_reg);
		if (err < 0)
			goto error;
#ifdef LSM303AGR_MAG_DEBUG
		dev_info(cdata->dev, "update odr to 0x%02x,0x%02x: %d\n",
			 CFG_REG_A, odr_reg, err);
#endif
	}

	return lsm303agr_mag_odr_table[i].time_ms;

error:
	dev_err(cdata->dev, "set odr failed 0x%02x,0x%02x: %d\n",
		CFG_REG_A, odr_reg, err);

	return err;
}

static int lsm303agr_mag_set_device_mode(struct lsm303agr_common_data *cdata,
					 u8 mode)
{
	int err;

	err = lsm303agr_mag_write_data_with_mask(cdata, CFG_REG_A,
						 LSM303AGR_MAG_MODE_MSK,
						 &mode);
	if (err < 0)
		goto error;

#ifdef LSM303AGR_MAG_DEBUG
	dev_info(cdata->dev, "update mode to 0x%02x,0x%02x: %d\n",
			CFG_REG_A, mode, err);
#endif

	return 0;

error:
	dev_err(cdata->dev,
		"set continuos mode failed 0x%02x,0x%02x: %d\n",
		CFG_REG_A, mode, err);

	return err;
}

/* Set device in continuos mode */
int lsm303agr_mag_enable(struct lsm303agr_common_data *cdata)
{
	struct lsm303agr_sensor_data *sdata;
	int err;

	mutex_lock(&cdata->lock);
	/* Set the magnetometer in continuos mode */
	err = lsm303agr_mag_set_device_mode(cdata, MD_CONTINUOS_MODE);
	if (err < 0) {
		dev_err(cdata->dev, "set_continuos failed: %d\n",
			err);
		mutex_unlock(&cdata->lock);
		return err;
	}

	atomic_set(&cdata->enabled, 1);

	sdata = &cdata->sensors[LSM303AGR_MAG_SENSOR];
	if (lsm303agr_mag_set_odr(cdata, sdata->c_odr) < 0) {
		mutex_unlock(&cdata->lock);
		return -1;
	}

	/* Start scheduling input */
	schedule_delayed_work(&sdata->input_work,
			      msecs_to_jiffies(sdata->poll_interval));
	mutex_unlock(&cdata->lock);

	return 0;
}
EXPORT_SYMBOL(lsm303agr_mag_enable);

/* Set device in idle mode */
int lsm303agr_mag_disable(struct lsm303agr_common_data *cdata)
{
	if (atomic_cmpxchg(&cdata->enabled, 1, 0)) {
		int err;
		struct lsm303agr_sensor_data *sdata;

		sdata = &cdata->sensors[LSM303AGR_MAG_SENSOR];
		cancel_delayed_work_sync(&sdata->input_work);

		mutex_lock(&cdata->lock);
		/* Set the magnetometer in idle mode */
		err = lsm303agr_mag_set_device_mode(cdata, MD_IDLE2_MODE);
		if (err < 0) {
			dev_err(cdata->dev, "set_idle failed: %d\n",
				err);
			mutex_unlock(&cdata->lock);
			return err;
		}
		mutex_unlock(&cdata->lock);
	}

	return 0;
}
EXPORT_SYMBOL(lsm303agr_mag_disable);

static void lsm303agr_mag_report_event(struct lsm303agr_common_data *cdata,
					int *xyz, s64 timestamp)
{
	struct lsm303agr_sensor_data *sdata;

	sdata = &cdata->sensors[LSM303AGR_MAG_SENSOR];

	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_X, xyz[0]);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_Y, xyz[1]);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_Z, xyz[2]);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_MSB,
		    timestamp >> 32);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_LSB,
		    timestamp & 0xffffffff);
	input_sync(sdata->input_dev);
}

static int lsm303agr_mag_get_data(struct lsm303agr_common_data *cdata,
				  int *xyz)
{
	int err;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 mag_data[6];

	err = cdata->tf->read(cdata->dev, AXISDATA_REG, 6, mag_data);
	if (err < 0)
		return err;

	/* Transform LSBs into ug */
	xyz[0] = (s32)((s16)(mag_data[0] | (mag_data[1] << 8)));
	xyz[0] *= LSM303AGR_MAG_SENSITIVITY;
	xyz[1] = (s32)((s16)(mag_data[2] | (mag_data[3] << 8)));
	xyz[1] *= LSM303AGR_MAG_SENSITIVITY;
	xyz[2] = (s32)((s16)(mag_data[4] | (mag_data[5] << 8)));
	xyz[2] *= LSM303AGR_MAG_SENSITIVITY;

#ifdef LSM303AGR_MAG_DEBUG
	dev_info(cdata->dev, "%s read x=%d, y=%d, z=%d\n",
		 LSM303AGR_MAG_DEV_NAME, xyz[0], xyz[1], xyz[2]);
#endif

	return err;
}

static void lsm303agr_mag_input_work_func(struct work_struct *work)
{
	struct lsm303agr_common_data *cdata;
	struct lsm303agr_sensor_data *sdata;
	int err, xyz[3] = {};

	sdata = container_of((struct delayed_work *)work,
			     struct lsm303agr_sensor_data, input_work);
	cdata = sdata->cdata;

	mutex_lock(&cdata->lock);
	sdata->schedule_num++;
	err = lsm303agr_mag_get_data(cdata, xyz);
	if (err < 0)
		dev_err(cdata->dev, "get_mag_data failed\n");
	else
		lsm303agr_mag_report_event(cdata, xyz,
					   lsm303agr_get_time_ns());

	schedule_delayed_work(&sdata->input_work,
			      msecs_to_jiffies(sdata->poll_interval));
	mutex_unlock(&cdata->lock);
}

static void lsm303agr_mag_input_cleanup(struct lsm303agr_common_data *cdata)
{
	struct lsm303agr_sensor_data *sdata;

	sdata = &cdata->sensors[LSM303AGR_MAG_SENSOR];
	input_unregister_device(sdata->input_dev);
	input_free_device(sdata->input_dev);
}

/* SYSFS: set val to polling_ms ATTR */
static ssize_t attr_get_polling_rate_mag(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	u32 val = 0;
	struct lsm303agr_sensor_data *sdata;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	sdata = &cdata->sensors[LSM303AGR_MAG_SENSOR];

	/* read from platform data */
	mutex_lock(&cdata->lock);
	val = sdata->poll_interval;
	mutex_unlock(&cdata->lock);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate_mag(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (!val)
		return -EINVAL;

	mutex_lock(&cdata->lock);
	/* set ODR */
	val = lsm303agr_mag_set_odr(cdata, val);
	if (val < 0) {
		mutex_unlock(&cdata->lock);
		return -1;
	}

	/* write to platform data */
	cdata->sensors[LSM303AGR_MAG_SENSOR].poll_interval = val;

	mutex_unlock(&cdata->lock);

	return size;
}

/* SYSFS: set val to enable_device ATTR */
static ssize_t attr_get_enable_mag(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);
	int val = atomic_read(&cdata->enabled);

	return sprintf(buf, "%d\n", val);
}

/* SYSFS: get val from enable_device ATTR */
static ssize_t attr_set_enable_mag(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned long val;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lsm303agr_mag_enable(cdata);
	else
		lsm303agr_mag_disable(cdata);

	return size;
}

static struct device_attribute lsm303agr_mag_attributes[] = {
	__ATTR(pollrate_ms, 0664, attr_get_polling_rate_mag,
	       attr_set_polling_rate_mag),
	__ATTR(enable_device, 0664, attr_get_enable_mag, attr_set_enable_mag),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lsm303agr_mag_attributes); i++)
		if (device_create_file(dev, lsm303agr_mag_attributes + i))
			goto error;
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, lsm303agr_mag_attributes + i);

	dev_err(dev, "%s:Unable to create interface\n", __func__);

	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lsm303agr_mag_attributes); i++)
		device_remove_file(dev, lsm303agr_mag_attributes + i);

	return 0;
}

int lsm303agr_mag_probe(struct lsm303agr_common_data *cdata)
{
	int err;
	struct lsm303agr_sensor_data *sdata;

	mutex_lock(&cdata->lock);
	/* init sensor data structure */
	sdata = &cdata->sensors[LSM303AGR_MAG_SENSOR];
	sdata->cdata = cdata;

	/* Check WHO_AM_I */
	err = lsm303agr_mag_check_wai(cdata);
	if (err < 0) {
		dev_err(cdata->dev, "check WAI failed\n");
		mutex_unlock(&cdata->lock);
		return err;
	}

	/* Set device ODR to 100ms (10Hz) */
	err = lsm303agr_mag_set_odr(cdata, 100);
	if (err < 0) {
		dev_err(cdata->dev, "Set ODR On failed\n");
		mutex_unlock(&cdata->lock);
		return err;
	}

	/* Disable Magnetometer to save power */
	mutex_unlock(&cdata->lock);
	err = lsm303agr_mag_disable(cdata);
	if (err < 0) {
		dev_err(cdata->dev, "Power On failed\n");
		mutex_unlock(&cdata->lock);
		return err;
	}

	mutex_lock(&cdata->lock);
	/* Init the input framework */
	err = lsm303agr_mag_input_init(sdata, LSM303AGR_MAG_DEV_NAME);
	if (err < 0) {
		dev_err(cdata->dev, "input init failed\n");
		mutex_unlock(&cdata->lock);
		return err;
	}
	INIT_DELAYED_WORK(&sdata->input_work, lsm303agr_mag_input_work_func);

	/* Create SYSFS interface */
	err = create_sysfs_interfaces(cdata->dev);
	if (err < 0) {
		dev_err(cdata->dev,
		   "device LSM303AGR_MAG_DEV_NAME sysfs register failed\n");
		mutex_unlock(&cdata->lock);
		return err;
	}

	dev_info(cdata->dev, "%s: probed\n", LSM303AGR_MAG_DEV_NAME);
	mutex_unlock(&cdata->lock);

	return 0;
}
EXPORT_SYMBOL(lsm303agr_mag_probe);

void lsm303agr_mag_remove(struct lsm303agr_common_data *cdata)
{
	lsm303agr_mag_disable(cdata);
	lsm303agr_mag_input_cleanup(cdata);
	remove_sysfs_interfaces(cdata->dev);
}
EXPORT_SYMBOL(lsm303agr_mag_remove);

MODULE_LICENSE("GPL v2");
