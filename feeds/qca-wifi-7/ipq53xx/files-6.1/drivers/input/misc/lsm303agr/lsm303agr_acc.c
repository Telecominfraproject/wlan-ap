/*
 * STMicroelectronics lsm303agr_acc.c driver
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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#include "lsm303agr_core.h"

#define LSM303AGR_ACC_DEV_NAME	"lsm303agr_acc"

#define LSM303AGR_ACC_MIN_POLL_PERIOD_MS 1

/* I2C slave address */
#define LSM303AGR_ACC_I2C_SAD	0x29
/* Accelerometer Sensor Full Scale */
#define	LSM303AGR_ACC_FS_MSK	0x20
#define LSM303AGR_ACC_G_2G	0x00
#define LSM303AGR_ACC_G_8G	0x20

#define AXISDATA_REG		0x28
#define WHOAMI_LSM303AGR_ACC	0x33
#define WHO_AM_I		0x0F
#define CTRL_REG1		0x20
#define CTRL_REG2		0x23

#define LSM303AGR_ACC_PM_OFF		0x00
#define LSM303AGR_ACC_ENABLE_ALL_AXIS	0x07
#define LSM303AGR_ACC_AXIS_MSK		0x07
#define LSM303AGR_ACC_ODR_MSK		0xf0
#define LSM303AGR_ACC_LP_MSK		0X08
#define LSM303AGR_ACC_HR_MSK		0X08

/* device opmode */
enum lsm303agr_acc_opmode {
	LSM303AGR_ACC_OPMODE_NORMAL,
	LSM303AGR_ACC_OPMODE_HR,
	LSM303AGR_ACC_OPMODE_LP,
};

/* Device sensitivities [ug/digit] */
#define LSM303AGR_ACC_SENSITIVITY_NORMAL_2G	3900
#define LSM303AGR_ACC_SENSITIVITY_NORMAL_4G	7820
#define LSM303AGR_ACC_SENSITIVITY_NORMAL_8G	15630
#define LSM303AGR_ACC_SENSITIVITY_NORMAL_16G	46900
#define LSM303AGR_ACC_SENSITIVITY_HR_2G		980
#define LSM303AGR_ACC_SENSITIVITY_HR_4G		1950
#define LSM303AGR_ACC_SENSITIVITY_HR_8G		3900
#define LSM303AGR_ACC_SENSITIVITY_HR_16G	11720
#define LSM303AGR_ACC_SENSITIVITY_LP_2G		15630
#define LSM303AGR_ACC_SENSITIVITY_LP_4G		31260
#define LSM303AGR_ACC_SENSITIVITY_LP_8G		62520
#define LSM303AGR_ACC_SENSITIVITY_LP_16G	187580

/* Device shift values */
#define LSM303AGR_ACC_SHIFT_NORMAL_MODE	6
#define LSM303AGR_ACC_SHIFT_HR_MODE	4
#define LSM303AGR_ACC_SHIFT_LP_MODE	8

const struct {
	u16 shift;
	u32 sensitivity[4];
} lsm303agr_acc_opmode_table[] = {
	{
		/* normal mode */
		LSM303AGR_ACC_SHIFT_NORMAL_MODE,
		{
			LSM303AGR_ACC_SENSITIVITY_NORMAL_2G,
			LSM303AGR_ACC_SENSITIVITY_NORMAL_4G,
			LSM303AGR_ACC_SENSITIVITY_NORMAL_8G,
			LSM303AGR_ACC_SENSITIVITY_NORMAL_16G
		}
	},
	{
		/* hr mode */
		LSM303AGR_ACC_SHIFT_HR_MODE,
		{
			LSM303AGR_ACC_SENSITIVITY_HR_2G,
			LSM303AGR_ACC_SENSITIVITY_HR_4G,
			LSM303AGR_ACC_SENSITIVITY_HR_8G,
			LSM303AGR_ACC_SENSITIVITY_HR_16G
		}
	},
	{
		/* lp mode */
		LSM303AGR_ACC_SHIFT_LP_MODE,
		{
			LSM303AGR_ACC_SENSITIVITY_LP_2G,
			LSM303AGR_ACC_SENSITIVITY_LP_4G,
			LSM303AGR_ACC_SENSITIVITY_LP_8G,
			LSM303AGR_ACC_SENSITIVITY_LP_16G
		}
	}
};

#define LSM303AGR_ACC_ODR10	0x20  /* 10Hz output data rate */
#define LSM303AGR_ACC_ODR50	0x40  /* 50Hz output data rate */
#define LSM303AGR_ACC_ODR100	0x50  /* 100Hz output data rate */
#define LSM303AGR_ACC_ODR200	0x60  /* 200Hz output data rate */

/* read and write with mask a given register */
static int lsm303agr_acc_write_data_with_mask(struct lsm303agr_common_data *cdata,
					      u8 reg_addr, u8 mask, u8 *data)
{
	int err;
	u8 new_data, old_data = 0;

	err = cdata->tf->read(cdata->dev, reg_addr, 1, &old_data);
	if (err < 0)
		return err;

	new_data = ((old_data & (~mask)) | ((*data) & mask));

#ifdef LSM303AGR_ACC_DEBUG
	dev_info(cdata->dev, "%s %02x o=%02x d=%02x n=%02x\n",
		 LSM303AGR_ACC_DEV_NAME, reg_addr, old_data, *data, new_data);
#endif

	/* Save for caller usage the data that is about to be written */
	*data = new_data;

	if (new_data == old_data)
		return 1;

	return cdata->tf->write(cdata->dev, reg_addr, 1, &new_data);
}

static int lsm303agr_acc_input_init(struct lsm303agr_sensor_data *sdata,
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
	set_bit(INPUT_EVENT_TYPE, sdata->input_dev->evbit);
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

struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lsm303agr_acc_odr_table[] = {
	{    5, LSM303AGR_ACC_ODR200  }, /* ODR = 200Hz */
	{   10, LSM303AGR_ACC_ODR100  }, /* ODR = 100Hz */
	{   20, LSM303AGR_ACC_ODR50   }, /* ODR = 50Hz */
	{  100, LSM303AGR_ACC_ODR10   }, /* ODR = 10Hz */
};

static int lsm303agr_acc_hw_init(struct lsm303agr_common_data *cdata)
{
	int err;
	u8 buf, wai = 0;

#ifdef LSM303AGR_ACC_DEBUG
	pr_info("%s: hw init start\n", LSM303AGR_ACC_DEV_NAME);
#endif

	err = cdata->tf->read(cdata->dev, WHO_AM_I, 1, &wai);
	if (err < 0) {
		dev_warn(cdata->dev, "Error reading WHO_AM_I\n");
		goto error;
	}

	if (wai != WHOAMI_LSM303AGR_ACC) {
		dev_err(cdata->dev,
			"device unknown (0x%02x-0x%02x)\n",
			WHOAMI_LSM303AGR_ACC, wai);
		err = -1; /* choose the right coded error */
		goto error;
	}

	buf = cdata->sensors[LSM303AGR_ACC_SENSOR].c_odr;
	err = lsm303agr_acc_write_data_with_mask(cdata, CTRL_REG1,
						 LSM303AGR_ACC_ODR_MSK, &buf);
	if (err < 0)
		goto error;

	cdata->hw_initialized = 1;

#ifdef LSM303AGR_ACC_DEBUG
	pr_info("%s: hw init done\n", LSM303AGR_ACC_DEV_NAME);
#endif
	return 0;

error:
	cdata->hw_initialized = 0;
	dev_err(cdata->dev, "hw init error 0x%02x: %d\n", buf, err);

	return err;
}

static void lsm303agr_acc_device_power_off(struct lsm303agr_common_data *cdata)
{
	int err;
	u8 buf = LSM303AGR_ACC_PM_OFF;

	err = lsm303agr_acc_write_data_with_mask(cdata, CTRL_REG1,
						 LSM303AGR_ACC_ODR_MSK, &buf);
	if (err < 0)
		dev_err(cdata->dev, "soft power off failed: %d\n", err);

	if (cdata->hw_initialized)
		cdata->hw_initialized = 0;
}

static int lsm303agr_acc_device_power_on(struct lsm303agr_common_data *cdata)
{
	if (!cdata->hw_initialized) {
		int err = lsm303agr_acc_hw_init(cdata);
		if (err < 0) {
			lsm303agr_acc_device_power_off(cdata);
			return err;
		}
	}

	return 0;
}

static int lsm303agr_acc_update_fs_range(struct lsm303agr_common_data *cdata,
					 u8 new_fs_range)
{
	int err;
	u8 indx;
	u16 opmode;
	unsigned int new_sensitivity;
	struct lsm303agr_sensor_data *sdata;

	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
	opmode = sdata->opmode;

	switch (new_fs_range) {
	case LSM303AGR_ACC_G_2G:
		indx = 0;
		break;
	case LSM303AGR_ACC_G_8G:
		indx = 2;
		break;
	default:
		dev_err(cdata->dev, "invalid fs range requested: %u\n",
			new_fs_range);
		return -EINVAL;
	}

	/* Updates configuration register 4 which contains fs range setting */
	err = lsm303agr_acc_write_data_with_mask(cdata, CTRL_REG2,
						 LSM303AGR_ACC_FS_MSK,
						 &new_fs_range);
	if (err < 0)
		goto error;

	new_sensitivity = lsm303agr_acc_opmode_table[opmode].sensitivity[indx];
	sdata->sensitivity = new_sensitivity;

#ifdef LSM303AGR_ACC_DEBUG
	dev_info(cdata->dev, "%s shift=%d, sens=%d, opm=%d\n",
		 LSM303AGR_ACC_DEV_NAME, sdata->shift, sdata->sensitivity,
		 sdata->opmode);
#endif

	return err;
error:
	dev_err(cdata->dev, "update fs range failed %d\n", err);

	return err;
}

static int lsm303agr_acc_update_odr(struct lsm303agr_common_data *cdata,
							int poll_interval)
{
	u8 buf;
	int i, err = -1;
	struct lsm303agr_sensor_data *sdata;

	/**
	 * Following, looks for the longest possible odr
	 * interval od -x /dev/input/event0 scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval
	 */
	for (i = ARRAY_SIZE(lsm303agr_acc_odr_table) - 1; i >= 0; i--) {
		if ((lsm303agr_acc_odr_table[i].cutoff_ms <= poll_interval) ||
		    (i == 0))
			break;
	}

	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
	/* also save requested odr */
	buf = (sdata->c_odr = lsm303agr_acc_odr_table[i].mask) |
	      LSM303AGR_ACC_ENABLE_ALL_AXIS;

	/*
	 * If device is currently enabled, we need to write new
	 * configuration out to it
	 */
	if (atomic_read(&cdata->enabled)) {
		err = lsm303agr_acc_write_data_with_mask(cdata, CTRL_REG1,
							 LSM303AGR_ACC_ODR_MSK |
							 LSM303AGR_ACC_AXIS_MSK,
							 &buf);
		if (err < 0)
			goto error;
	}

#ifdef LSM303AGR_ACC_DEBUG
	dev_info(cdata->dev, "update odr to 0x%02x,0x%02x: %d\n",
			CTRL_REG1, buf, err);
#endif

	return err;

error:
	dev_err(cdata->dev, "update odr failed 0x%02x,0x%02x: %d\n",
			CTRL_REG1, buf, err);

	return err;
}

static int lsm303agr_acc_update_opmode(struct lsm303agr_common_data *cdata,
				       unsigned short opmode)
{
	int err;
	struct lsm303agr_sensor_data *sdata;
	u8 lp = 0, hr = 0, indx = 0;

	switch (opmode) {
	case LSM303AGR_ACC_OPMODE_NORMAL:
		break;
	case LSM303AGR_ACC_OPMODE_HR:
		hr = (1 << __ffs(LSM303AGR_ACC_LP_MSK));
		break;
	case LSM303AGR_ACC_OPMODE_LP:
		lp = (1 << __ffs(LSM303AGR_ACC_HR_MSK));
		break;
	default:
		return -EINVAL;
	}

	/* Set LP bit in CTRL_REG1 */
	err = lsm303agr_acc_write_data_with_mask(cdata, CTRL_REG1,
						 LSM303AGR_ACC_LP_MSK, &lp);
	if (err < 0)
		goto error;

	/* Set HR bit in CTRL_REG4 */
	err = lsm303agr_acc_write_data_with_mask(cdata, CTRL_REG2,
						 LSM303AGR_ACC_HR_MSK,
						 &hr);
	if (err < 0)
		goto error;

	/* Change platform data */
	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
	sdata->opmode = opmode;
	sdata->shift = lsm303agr_acc_opmode_table[opmode].shift;

	switch (sdata->fs_range) {
	case LSM303AGR_ACC_G_2G:
		indx = 0;
		break;
	case LSM303AGR_ACC_G_8G:
		indx = 2;
		break;
	}
	sdata->sensitivity = lsm303agr_acc_opmode_table[opmode].sensitivity[indx];

#ifdef LSM303AGR_ACC_DEBUG
	dev_info(cdata->dev, "%s shift=%d, sens=%d, opm=%d\n",
		 LSM303AGR_ACC_DEV_NAME, sdata->shift, sdata->sensitivity,
		 sdata->opmode);
#endif

	return err;

error:
	dev_err(cdata->dev, "update opmode failed: %d\n", err);

	return err;
}

static int
lsm303agr_acc_get_acceleration_data(struct lsm303agr_common_data *cdata, int *xyz)
{
	int err;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	u32 sensitivity, shift;

	err = cdata->tf->read(cdata->dev, AXISDATA_REG, 6, acc_data);
	if (err < 0)
		return err;

	/* Get the current sensitivity and shift values */
	sensitivity = cdata->sensors[LSM303AGR_ACC_SENSOR].sensitivity;
	shift = cdata->sensors[LSM303AGR_ACC_SENSOR].shift;

	/* Transform LSBs into ug */
	xyz[0] = (s32)((s16)(acc_data[0] | (acc_data[1] << 8)) >> shift) * sensitivity;
	xyz[1] = (s32)((s16)(acc_data[2] | (acc_data[3] << 8)) >> shift) * sensitivity;
	xyz[2] = (s32)((s16)(acc_data[4] | (acc_data[5] << 8)) >> shift) * sensitivity;

#ifdef LSM303AGR_ACC_DEBUG
	dev_info(cdata->dev, "%s read x=%d, y=%d, z=%d\n",
		 LSM303AGR_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
#endif

	return err;
}

static void lsm303agr_acc_report_values(struct lsm303agr_common_data *cdata,
					int *xyz, s64 timestamp)
{
	struct lsm303agr_sensor_data *sdata;

	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_X, xyz[0]);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_Y, xyz[1]);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_Z, xyz[2]);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_MSB,
		    timestamp >> 32);
	input_event(sdata->input_dev, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_LSB,
		    timestamp & 0xffffffff);
	input_sync(sdata->input_dev);
}

int lsm303agr_acc_enable(struct lsm303agr_common_data *cdata)
{
	if (!atomic_cmpxchg(&cdata->enabled, 0, 1)) {
		int err;
		struct lsm303agr_sensor_data *sdata;

		mutex_lock(&cdata->lock);

		sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
		err = lsm303agr_acc_device_power_on(cdata);
		if (err < 0) {
			atomic_set(&cdata->enabled, 0);
			return err;
		}
		schedule_delayed_work(&sdata->input_work,
				      msecs_to_jiffies(sdata->poll_interval));

		mutex_unlock(&cdata->lock);
	}

	return 0;
}
EXPORT_SYMBOL(lsm303agr_acc_enable);

int lsm303agr_acc_disable(struct lsm303agr_common_data *cdata)
{
	if (atomic_cmpxchg(&cdata->enabled, 1, 0)) {
		struct lsm303agr_sensor_data *sdata;

		sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
		cancel_delayed_work_sync(&sdata->input_work);

		mutex_lock(&cdata->lock);
		lsm303agr_acc_device_power_off(cdata);
		mutex_unlock(&cdata->lock);
	}

	return 0;
}
EXPORT_SYMBOL(lsm303agr_acc_disable);

static ssize_t attr_get_sched_num_acc(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int val;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	mutex_lock(&cdata->lock);
	val = cdata->sensors[LSM303AGR_ACC_SENSOR].schedule_num;
	mutex_unlock(&cdata->lock);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_sched_num_acc(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	unsigned long sched_num;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &sched_num))
		return -EINVAL;

	mutex_lock(&cdata->lock);
	cdata->sensors[LSM303AGR_ACC_SENSOR].schedule_num = sched_num;
	mutex_unlock(&cdata->lock);

	return size;
}

static ssize_t attr_get_polling_rate_acc(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int val;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	mutex_lock(&cdata->lock);
	val = cdata->sensors[LSM303AGR_ACC_SENSOR].poll_interval;
	mutex_unlock(&cdata->lock);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate_acc(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long interval_ms;
	struct lsm303agr_sensor_data *sdata;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;

	if (!interval_ms)
		return -EINVAL;

	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
	interval_ms = max_t(unsigned int, (unsigned int)interval_ms,
			    sdata->min_interval);

	mutex_lock(&cdata->lock);
	sdata->poll_interval = interval_ms;
	lsm303agr_acc_update_odr(cdata, interval_ms);
	mutex_unlock(&cdata->lock);

	return size;
}

static ssize_t attr_get_range_acc(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct lsm303agr_sensor_data *sdata;
	char range = 2;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];

	mutex_lock(&cdata->lock);
	switch (sdata->fs_range) {
	case LSM303AGR_ACC_G_2G:
		range = 2;
		break;
	case LSM303AGR_ACC_G_8G:
		range = 8;
		break;
	}
	mutex_unlock(&cdata->lock);

	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range_acc(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	u8 range;
	int err;
	unsigned long val;
	struct lsm303agr_sensor_data *sdata;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	switch (val) {
	case 2:
		range = LSM303AGR_ACC_G_2G;
		break;
	case 8:
		range = LSM303AGR_ACC_G_8G;
		break;
	default:
		dev_err(cdata->dev,
			"invalid range request: %lu, discarded\n", val);
		return -EINVAL;
	}

	mutex_lock(&cdata->lock);
	err = lsm303agr_acc_update_fs_range(cdata, range);
	if (err < 0) {
		mutex_unlock(&cdata->lock);
		return err;
	}
	sdata->fs_range = range;
	mutex_unlock(&cdata->lock);

	dev_info(cdata->dev, "range set to: %lu g\n", val);

	return size;
}

static ssize_t attr_get_opmode_acc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	char opmode;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	mutex_lock(&cdata->lock);
	opmode = cdata->sensors[LSM303AGR_ACC_SENSOR].opmode;
	mutex_unlock(&cdata->lock);

	return sprintf(buf, "%d\n", opmode);
}

static ssize_t attr_set_opmode_acc(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int err;
	u16 opmode;
	unsigned long val;
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	/* Check if argument is valid opmode */
	switch (val) {
	case LSM303AGR_ACC_OPMODE_NORMAL:
	case LSM303AGR_ACC_OPMODE_HR:
	case LSM303AGR_ACC_OPMODE_LP:
		opmode = val;
		break;
	default:
		dev_err(cdata->dev,
			"invalid range request: %lu, discarded\n", val);
		return -EINVAL;
	}

	mutex_lock(&cdata->lock);
	err = lsm303agr_acc_update_opmode(cdata, opmode);
	if (err < 0) {
		mutex_unlock(&cdata->lock);
		return err;
	}
	mutex_unlock(&cdata->lock);

	dev_info(cdata->dev, "opmode set to: %u\n", opmode);

	return size;
}

static ssize_t attr_get_enable_acc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);
	int val = atomic_read(&cdata->enabled);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable_acc(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct lsm303agr_common_data *cdata = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lsm303agr_acc_enable(cdata);
	else
		lsm303agr_acc_disable(cdata);

	return size;
}

static struct device_attribute attributes[] = {

	__ATTR(pollrate_ms, 0664, attr_get_polling_rate_acc,
	       attr_set_polling_rate_acc),
	__ATTR(range, 0664, attr_get_range_acc, attr_set_range_acc),
	__ATTR(opmode, 0664, attr_get_opmode_acc, attr_set_opmode_acc),
	__ATTR(enable_device, 0664, attr_get_enable_acc, attr_set_enable_acc),
	__ATTR(schedule_num, 0664, attr_get_sched_num_acc,
	       attr_set_sched_num_acc),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);

	dev_err(dev, "%s:Unable to create interface\n", __func__);

	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);

	return 0;
}

static void lsm303agr_acc_input_work_func(struct work_struct *work)
{
	struct lsm303agr_common_data *cdata;
	struct lsm303agr_sensor_data *sdata;
	int err, xyz[3] = {};

	sdata = container_of((struct delayed_work *)work,
			     struct lsm303agr_sensor_data, input_work);
	cdata = sdata->cdata;

	mutex_lock(&cdata->lock);
	sdata->schedule_num++;
	err = lsm303agr_acc_get_acceleration_data(cdata, xyz);
	if (err < 0)
		dev_err(cdata->dev, "get_acceleration_data failed\n");
	else
		lsm303agr_acc_report_values(cdata, xyz, lsm303agr_get_time_ns());

	schedule_delayed_work(&sdata->input_work, msecs_to_jiffies(
			      sdata->poll_interval));
	mutex_unlock(&cdata->lock);
}

static void lsm303agr_acc_input_cleanup(struct lsm303agr_common_data *cdata)
{
	struct lsm303agr_sensor_data *sdata;

	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];
	input_unregister_device(sdata->input_dev);
	input_free_device(sdata->input_dev);
}

int lsm303agr_acc_probe(struct lsm303agr_common_data *cdata)
{
	int err;
	struct lsm303agr_sensor_data *sdata;

	mutex_lock(&cdata->lock);
	/* init sensor data structure */
	sdata = &cdata->sensors[LSM303AGR_ACC_SENSOR];

	sdata->cdata = cdata;
	sdata->poll_interval = 100;
	sdata->min_interval = LSM303AGR_ACC_MIN_POLL_PERIOD_MS;

	err = lsm303agr_acc_device_power_on(cdata);
	if (err < 0) {
		dev_err(cdata->dev, "power on failed: %d\n", err);
		goto  err_power_off;
	}

	atomic_set(&cdata->enabled, 1);

	err = lsm303agr_acc_update_fs_range(cdata, LSM303AGR_ACC_G_2G);
	if (err < 0) {
		dev_err(cdata->dev, "update_fs_range failed\n");
		goto  err_power_off;
	}

	err = lsm303agr_acc_update_odr(cdata, sdata->poll_interval);
	if (err < 0) {
		dev_err(cdata->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = lsm303agr_acc_update_opmode(cdata, LSM303AGR_ACC_OPMODE_NORMAL);
	if (err < 0) {
		dev_err(cdata->dev, "update_opmode failed\n");
		goto  err_power_off;
	}

	err = lsm303agr_acc_input_init(sdata, LSM303AGR_ACC_DEV_NAME);
	if (err < 0) {
		dev_err(cdata->dev, "input init failed\n");
		goto err_power_off;
	}
	INIT_DELAYED_WORK(&sdata->input_work, lsm303agr_acc_input_work_func);


	err = create_sysfs_interfaces(cdata->dev);
	if (err < 0) {
		dev_err(cdata->dev,
		   "device LSM303AGR_ACC_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}

	lsm303agr_acc_device_power_off(cdata);

	/* As default, do not report information */
	atomic_set(&cdata->enabled, 0);

	dev_info(cdata->dev, "%s: probed\n", LSM303AGR_ACC_DEV_NAME);

	mutex_unlock(&cdata->lock);

	return 0;

err_input_cleanup:
	lsm303agr_acc_input_cleanup(cdata);
err_power_off:
	lsm303agr_acc_device_power_off(cdata);
	mutex_unlock(&cdata->lock);
	pr_err("%s: Driver Init failed\n", LSM303AGR_ACC_DEV_NAME);

	return err;
}
EXPORT_SYMBOL(lsm303agr_acc_probe);

void lsm303agr_acc_remove(struct lsm303agr_common_data *cdata)
{
	lsm303agr_acc_disable(cdata);
	lsm303agr_acc_input_cleanup(cdata);
	remove_sysfs_interfaces(cdata->dev);
}
EXPORT_SYMBOL(lsm303agr_acc_remove);

MODULE_LICENSE("GPL v2");

