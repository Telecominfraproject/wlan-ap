// SPDX-License-Identifier: GPL-2.0-only
/*
 * LP5812 LED driver
 *
 * Copyright (C) 2025 Texas Instruments
 *
 * Author: Jared Zhou <jared-zhou@ti.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/led-class-multicolor.h>
#include "leds-lp5812.h"

#define LP5812_SC_LED "SC_LED"
#define LP5812_MC_LED "MC_LED"

#define LP5812_AUTO_PAUSE_ADDR(chan)  (LP5812_AEU_BASE + (chan) * 26)
#define LP5812_AUTO_PLAYBACK_ADDR(chan)  (LP5812_AEU_BASE + (chan) * 260 + 1)
#define LP5812_AEU_PWM_ADDR(chan, aeu, pwm_chan)  \
	(LP5812_AEU_BASE + (chan) * 26 + ((aeu) - 1) * 8 + 2 + (pwm_chan) - 1)
#define LP5812_AEU_SLOPE_TIME_ADDR(chan, aeu, slope_chan)  \
	(LP5812_AEU_BASE + (chan) * 26 + ((aeu) - 1) * 8 + 2 + 5 + ((slope_chan) / 2))
#define LP5812_AEU_PLAYBACK_ADDR(chan, aeu)  \
	(LP5812_AEU_BASE + (chan) * 26 + ((aeu) - 1) * 8 + 2 + 5 + 2)

#define to_lp5812_led(x) container_of(x, struct lp5812_data, kobj)
#define to_anim_engine_unit(x) container_of(x, struct anim_engine_unit, kobj)

static int lp5812_read_tsd_config_status(struct lp5812_chip *chip, u8 *reg_val);

/* Begin common functions */
static struct lp5812_led *cdev_to_lp5812_led(struct led_classdev *cdev)
{
	return container_of(cdev, struct lp5812_led, cdev);
}

static struct lp5812_led *mcled_cdev_to_lp5812_led(struct led_classdev_mc *mc_cdev)
{
	return container_of(mc_cdev, struct lp5812_led, mc_cdev);
}

static struct lp5812_led *dev_to_lp5812_led(struct device *dev)
{
	return cdev_to_lp5812_led(dev_get_drvdata(dev));
}

static struct lp5812_led *dev_to_lp5812_led_mc(struct device *dev)
{
	return mcled_cdev_to_lp5812_led(dev_get_drvdata(dev));
}

static int lp5812_write(struct lp5812_chip *chip, u16 reg, u8 val)
{
	int ret;
	struct i2c_msg msg;
	struct device *dev = &chip->i2c_cl->dev;
	u8 extracted_bits; /* save 9th and 8th bit of reg address */
	u8 buf[2] = {(u8)(reg & 0xFF), val};

	extracted_bits = (reg >> 8) & 0x03;
	msg.addr = (chip->i2c_cl->addr << 2) | extracted_bits;
	msg.flags = 0;
	msg.len = sizeof(buf);
	msg.buf = buf;

	ret = i2c_transfer(chip->i2c_cl->adapter, &msg, 1);
	if (ret != 1) {
		dev_err(dev, "i2c write error, ret=%d\n", ret);
		ret = ret < 0 ? ret : -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

static int lp5812_read(struct lp5812_chip *chip, u16 reg, u8 *val)
{
	int ret;
	u8 ret_val;  /* lp5812_chip return value */
	u8 extracted_bits; /* save 9th and 8th bit of reg address */
	u8 converted_reg;  /* extracted 8bit from reg */
	struct device *dev = &chip->i2c_cl->dev;
	struct i2c_msg msgs[2];

	extracted_bits = (reg >> 8) & 0x03;
	converted_reg = (u8)(reg & 0xFF);

	msgs[0].addr = (chip->i2c_cl->addr << 2) | extracted_bits;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &converted_reg;

	msgs[1].addr = (chip->i2c_cl->addr << 2) | extracted_bits;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &ret_val;

	ret = i2c_transfer(chip->i2c_cl->adapter, msgs, 2);
	if (ret != 2) {
		dev_err(dev, "Read reg value error, ret=%d\n", ret);
		*val = 0;
		ret = ret < 0 ? ret : -EIO;
	} else {
		*val = ret_val;
		ret = 0;
	}

	return ret;
}

static int lp5812_parse_common_child(struct device_node *child,
				     struct lp5812_led_config *cfg,
				     int child_number, int color_number)
{
	int reg, ret;

	ret = of_property_read_u32(child, "reg", &reg);
	if (ret)
		return ret;

	cfg[child_number].led_id[color_number] = reg;

	of_property_read_u8(child, "led-cur", &cfg[child_number].led_current[color_number]);
	of_property_read_u8(child, "max-cur", &cfg[child_number].max_current[color_number]);

	return 0;
}

static int lp5812_parse_multi_led_child(struct device_node *child,
					struct lp5812_led_config *cfg,
					int child_number, int color_number)
{
	int color_id, ret;

	ret = of_property_read_u32(child, "color", &color_id);
	if (ret)
		return ret;

	cfg[child_number].color_id[color_number] = color_id;
	return 0;
}

static int lp5812_parse_multi_led(struct device_node *np,
				  struct lp5812_led_config *cfg,
				  int child_number)
{
	int num_colors = 0, ret;

	for_each_available_child_of_node_scoped(np, child) {
		ret = lp5812_parse_common_child(child, cfg,
						child_number, num_colors);
		if (ret)
			return ret;

		ret = lp5812_parse_multi_led_child(child, cfg, child_number,
						   num_colors);
		if (ret)
			return ret;

		num_colors++;
	}

	cfg[child_number].num_colors = num_colors;
	cfg[child_number].is_sc_led = 0;

	return 0;
}

static int lp5812_parse_single_led(struct device_node *np,
				   struct lp5812_led_config *cfg,
				   int child_number)
{
	int ret;

	ret = lp5812_parse_common_child(np, cfg, child_number, 0);
	if (ret)
		return ret;

	cfg[child_number].num_colors = 1;
	cfg[child_number].is_sc_led = 1;

	return 0;
}

static int lp5812_parse_logical_led(struct device_node *np,
				    struct lp5812_led_config *cfg,
				    int child_number)
{
	int chan_nr, ret;

	of_property_read_string(np, "label", &cfg[child_number].name);

	ret = of_property_read_u32(np, "reg", &chan_nr);
	if (ret)
		return ret;

	cfg[child_number].chan_nr = chan_nr;

	if (of_node_name_eq(np, "multi-led"))
		return lp5812_parse_multi_led(np, cfg, child_number);
	else
		return lp5812_parse_single_led(np, cfg, child_number);
}

static struct lp5812_data *lp5812_of_populate_pdata(struct device *dev,
						    struct device_node *np,
						    struct lp5812_chip *chip)
{
	struct device_node *child;
	struct lp5812_data *pdata;
	struct lp5812_led_config *cfg;
	int num_channels, i = 0, ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	num_channels = of_get_available_child_count(np);
	if (num_channels == 0) {
		dev_err(dev, "no LED channels\n");
		return ERR_PTR(-EINVAL);
	}

	cfg = devm_kcalloc(dev, num_channels, sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return ERR_PTR(-ENOMEM);

	pdata->led_config = &cfg[0];
	pdata->num_channels = num_channels;

	for_each_available_child_of_node(np, child) {
		ret = lp5812_parse_logical_led(child, cfg, i);
		if (ret) {
			of_node_put(child);
			return ERR_PTR(-EINVAL);
		}
		i++;
	}

	of_property_read_string(np, "label", &pdata->label);

	return pdata;
}

/* End common functions */

/* Begin device functions */
static int lp5812_update_regs_config(struct lp5812_chip *chip)
{
	int ret;
	u8 reg_val; /* save register value */

	ret = lp5812_write(chip, chip->cfg->reg_cmd_update.addr, LP5812_UPDATE_CMD_VAL);
	if (ret)
		return ret;

	ret = lp5812_read_tsd_config_status(chip, &reg_val);
	if (ret == 0)
		return (int)(reg_val & 0x01);

	return ret;
}

static int lp5812_disable_all_leds(struct lp5812_chip *chip)
{
	int ret;

	ret = lp5812_write(chip, chip->cfg->reg_led_en_1.addr, 0x00);
	if (ret)
		return ret;
	ret = lp5812_write(chip, chip->cfg->reg_led_en_2.addr, 0x00);
	if (ret)
		return ret;

	return ret;
}

static int lp5812_fault_clear(struct lp5812_chip *chip, u8 value)
{
	u8 reg_val;

	if (value == 0)
		reg_val = LOD_CLEAR_VAL;
	else if (value == 1)
		reg_val = LSD_CLEAR_VAL;
	else if (value == 2)
		reg_val = TSD_CLEAR_VAL;
	else if (value == 3)
		reg_val = FAULT_CLEAR_ALL;
	else
		return -EINVAL;

	return lp5812_write(chip, chip->cfg->reg_reset.addr, reg_val);
}

static int lp5812_device_command(struct lp5812_chip *chip, enum device_command command)
{
	switch (command) {
	case LP5812_DEV_CMD_UPDATE:
		return lp5812_write(chip, chip->cfg->reg_cmd_update.addr, LP5812_UPDATE_CMD_VAL);
	case LP5812_DEV_CMD_START:
		return lp5812_write(chip, chip->cfg->reg_cmd_start.addr, LP5812_START_CMD_VAL);
	case LP5812_DEV_CMD_STOP:
		return lp5812_write(chip, chip->cfg->reg_cmd_stop.addr, LP5812_STOP_CMD_VAL);
	case LP5812_DEV_CMD_PAUSE:
		return lp5812_write(chip, chip->cfg->reg_cmd_pause.addr, LP5812_PAUSE_CMD_VAL);
	case LP5812_DEV_CMD_CONTINUE:
		return lp5812_write(chip, chip->cfg->reg_cmd_continue.addr,
			LP5812_CONTINUE_CMD_VAL);
	default:
		return -EINVAL;
	}
}

static int lp5812_read_tsd_config_status(struct lp5812_chip *chip, u8 *reg_val)
{
	return lp5812_read(chip, chip->cfg->reg_tsd_config_status.addr, reg_val);
}

static void set_mix_sel_led(struct lp5812_chip *chip, int mix_sel_led)
{
	if (mix_sel_led == 0)
		chip->u_drive_mode.s_drive_mode.mix_sel_led_0 = 1;

	if (mix_sel_led == 1)
		chip->u_drive_mode.s_drive_mode.mix_sel_led_1 = 1;

	if (mix_sel_led == 2)
		chip->u_drive_mode.s_drive_mode.mix_sel_led_2 = 1;

	if (mix_sel_led == 3)
		chip->u_drive_mode.s_drive_mode.mix_sel_led_3 = 1;
}

static ssize_t parse_drive_mode(struct lp5812_chip *chip, char *str)
{
	char *sub_str;
	int tcm_scan_num, mix_scan_num, mix_sel_led, scan_oder[4], i, ret;

	chip->u_drive_mode.s_drive_mode.mix_sel_led_0 = 0;
	chip->u_drive_mode.s_drive_mode.mix_sel_led_1 = 0;
	chip->u_drive_mode.s_drive_mode.mix_sel_led_2 = 0;
	chip->u_drive_mode.s_drive_mode.mix_sel_led_3 = 0;

	sub_str = strsep(&str, ":");
	if (sysfs_streq(sub_str, "direct_mode")) {
		chip->u_drive_mode.s_drive_mode.led_mode = 0;
	} else if (sysfs_streq(sub_str, "tcmscan")) {
		/* Get tcm scan number */
		sub_str = strsep(&str, ":");
		if (!sub_str)
			return -EINVAL;
		ret = kstrtoint(sub_str, 0, &tcm_scan_num);
		if (ret)
			return ret;
		if (tcm_scan_num < 0 || tcm_scan_num > 4)
			return -EINVAL;
		chip->u_drive_mode.s_drive_mode.led_mode = tcm_scan_num;

		for (i = 0; i < tcm_scan_num; i++) {
			sub_str = strsep(&str, ":");
			if (!sub_str)
				return -EINVAL;
			ret = kstrtoint(sub_str, 0, &scan_oder[i]);
			if (ret)
				return ret;
		}

		chip->u_scan_order.s_scan_order.scan_order_0 = scan_oder[0];
		chip->u_scan_order.s_scan_order.scan_order_1 = scan_oder[1];
		chip->u_scan_order.s_scan_order.scan_order_2 = scan_oder[2];
		chip->u_scan_order.s_scan_order.scan_order_3 = scan_oder[3];
	} else if (sysfs_streq(sub_str, "mixscan")) {
		/* Get mix scan number */
		sub_str = strsep(&str, ":");
		if (!sub_str)
			return -EINVAL;
		ret = kstrtoint(sub_str, 0, &mix_scan_num);
		if (ret)
			return ret;
		if (mix_scan_num < 0 || mix_scan_num > 3)
			return -EINVAL;

		chip->u_drive_mode.s_drive_mode.led_mode = mix_scan_num + 4;
		/* Get mix_sel_led */
		sub_str = strsep(&str, ":");
		if (!sub_str)
			return -EINVAL;
		ret = kstrtoint(sub_str, 0, &mix_sel_led);
		if (ret)
			return ret;
		if (mix_sel_led < 0 || mix_sel_led > 3)
			return -EINVAL;
		set_mix_sel_led(chip, mix_sel_led);

		for (i = 0; i < mix_scan_num; i++) {
			sub_str = strsep(&str, ":");
			if (!sub_str)
				return -EINVAL;
			ret = kstrtoint(sub_str, 0, &scan_oder[i]);
			if (ret)
				return ret;
			if (scan_oder[i] == mix_sel_led || scan_oder[i] < 0 || scan_oder[i] > 3)
				return -EINVAL;
		}
		chip->u_scan_order.s_scan_order.scan_order_0 = scan_oder[0];
		chip->u_scan_order.s_scan_order.scan_order_1 = scan_oder[1];
		chip->u_scan_order.s_scan_order.scan_order_2 = scan_oder[2];
		chip->u_scan_order.s_scan_order.scan_order_3 = scan_oder[3];
	} else {
		return -EINVAL;
	}
	return 0;
}

static int lp5812_set_drive_mode_scan_order(struct lp5812_chip *chip)
{
	u8 val;
	int ret;

	/* Set led mode */
	val = chip->u_drive_mode.drive_mode_val;
	ret = lp5812_write(chip, chip->cfg->reg_dev_config_1.addr, val);
	if (ret)
		return ret;

	/* Setup scan order */
	val = chip->u_scan_order.scan_order_val;
	ret = lp5812_write(chip, chip->cfg->reg_dev_config_2.addr, val);

	return ret;
}

static ssize_t dev_config_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	int ret;
	struct lp5812_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp5812_chip *chip = led->chip;

	guard(mutex)(&chip->lock);
	ret = parse_drive_mode(chip, (char *)buf);
	if (ret)
		return ret;

	ret = lp5812_set_drive_mode_scan_order(chip);
	if (ret)
		return ret;

	ret = lp5812_update_regs_config(chip);
	if (ret)
		return ret;

	ret = lp5812_disable_all_leds(chip);
	if (ret)
		return ret;

	return len;
}

static ssize_t device_command_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	struct lp5812_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp5812_chip *chip = led->chip;
	enum device_command cmd;

	if (sysfs_streq(buf, "update"))
		cmd = LP5812_DEV_CMD_UPDATE;
	else if (sysfs_streq(buf, "start"))
		cmd = LP5812_DEV_CMD_START;
	else if (sysfs_streq(buf, "stop"))
		cmd = LP5812_DEV_CMD_STOP;
	else if (sysfs_streq(buf, "pause"))
		cmd = LP5812_DEV_CMD_PAUSE;
	else if (sysfs_streq(buf, "continue"))
		cmd = LP5812_DEV_CMD_CONTINUE;
	else
		return -EINVAL;

	guard(mutex)(&chip->lock);
	lp5812_device_command(led->chip, cmd);
	return len;
}

static ssize_t fault_clear_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct lp5812_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp5812_chip *chip = led->chip;
	int fault_clear, ret;

	ret = kstrtoint(buf, 0, &fault_clear);
	if (ret)
		return ret;

	if (fault_clear < 0 || fault_clear > 3)
		return -EINVAL;

	guard(mutex)(&chip->lock);
	ret = lp5812_fault_clear(chip, fault_clear);
	if (ret)
		return -EIO;

	return len;
}

static ssize_t tsd_config_status_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	u8 reg_val;
	int tsd_stat, config_stat, ret;
	struct lp5812_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp5812_chip *chip = led->chip;

	guard(mutex)(&chip->lock);
	ret = lp5812_read(chip, chip->cfg->reg_tsd_config_status.addr, &reg_val);
	if (ret)
		return -EIO;
	tsd_stat = (reg_val >> 1) & 0x01;
	config_stat = reg_val & 0x01;

	return sysfs_emit(buf, "%d %d\n", tsd_stat, config_stat);
}

static ssize_t sw_reset_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t len)
{
	int reset, ret;
	struct lp5812_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp5812_chip *chip = led->chip;

	ret = kstrtoint(buf, 0, &reset);
	if (ret)
		return ret;

	if (reset != 1)
		return -EINVAL;

	guard(mutex)(&chip->lock);
	ret = lp5812_write(chip, chip->cfg->reg_reset.addr, LP5812_RESET);
	if (ret)
		return -EIO;

	return len;
}

static void lp5812_deinit_device(struct lp5812_chip *chip)
{
	(void)lp5812_disable_all_leds(chip);
	(void)lp5812_write(chip, chip->cfg->reg_chip_en.addr, (u8)0);
}

static int lp5812_init_device(struct lp5812_chip *chip)
{
	int ret;

	usleep_range(1000, 1100);

	ret = lp5812_write(chip, chip->cfg->reg_chip_en.addr, (u8)1);
	if (ret) {
		dev_err(&chip->i2c_cl->dev, "lp5812_enable_disable failed\n");
		return ret;
	}

	ret = lp5812_write(chip, chip->cfg->reg_dev_config_12.addr, 0x0B);
	if (ret) {
		dev_err(&chip->i2c_cl->dev, "write 0x0B to DEV_CONFIG12 failed\n");
		return ret;
	}

	ret = lp5812_update_regs_config(chip);
	if (ret) {
		dev_err(&chip->i2c_cl->dev, "lp5812_update_regs_config failed\n");
		return ret;
	}

	return 0;
}

/* End device functions */

/* Begin led functions*/
static int lp5812_read_lod_status(struct lp5812_chip *chip, int led_number, u8 *val)
{
	int ret;
	u16 reg;
	u8 reg_val;

	if (!val)
		return -1;

	if (led_number < 0x8)
		reg = chip->cfg->reg_lod_status_base.addr;
	else
		reg = chip->cfg->reg_lod_status_base.addr + 1;

	ret = lp5812_read(chip, reg, &reg_val);
	if (ret)
		return ret;

	*val = (reg_val & (1 << (led_number % 8))) ? 1 : 0;

	return ret;
}

static int lp5812_read_lsd_status(struct lp5812_chip *chip, int led_number, u8 *val)
{
	int ret;
	u16 reg;
	u8 reg_val;

	if (!val)
		return -1;

	if (led_number < 0x8)
		reg = chip->cfg->reg_lsd_status_base.addr;
	else
		reg = chip->cfg->reg_lsd_status_base.addr + 1;

	ret = lp5812_read(chip, reg, &reg_val);
	if (ret)
		return ret;

	*val = (reg_val & (1 << (led_number % 8))) ? 1 : 0;

	return ret;
}

static int lp5812_set_led_mode(struct lp5812_chip *chip, int led_number,
			       enum control_mode mode)
{
	int ret;
	u16 reg;
	u8 reg_val;

	if (led_number <= 7)
		reg = chip->cfg->reg_dev_config_3.addr;
	else
		reg = chip->cfg->reg_dev_config_4.addr;

	ret = lp5812_read(chip, reg, &reg_val);
	if (ret)
		return ret;

	if (mode == LP5812_MODE_MANUAL)
		reg_val &= ~(1 << (led_number % 8));
	else
		reg_val |= (1 << (led_number % 8));

	ret = lp5812_write(chip, reg, reg_val);
	if (ret)
		return ret;

	ret = lp5812_update_regs_config(chip);

	return ret;
}

static int lp5812_get_led_mode(struct lp5812_chip *chip, int led_number,
			       enum control_mode *mode)
{
	int ret;
	u16 reg;
	u8 reg_val;

	if (led_number <= 7)
		reg = chip->cfg->reg_dev_config_3.addr;
	else
		reg = chip->cfg->reg_dev_config_4.addr;

	ret = lp5812_read(chip, reg, &reg_val);
	if (ret)
		return ret;

	*mode = (reg_val & (1 << (led_number % 8))) ? LP5812_MODE_AUTONOMOUS : LP5812_MODE_MANUAL;
	return 0;
}

static int lp5812_set_pwm_dimming_scale(struct lp5812_chip *chip, int led_number,
					enum pwm_dimming_scale scale)
{
	int ret;
	u16 reg;
	u8 reg_val;

	if (led_number <= 7)
		reg = chip->cfg->reg_dev_config_5.addr;
	else
		reg = chip->cfg->reg_dev_config_6.addr;

	ret = lp5812_read(chip, reg, &reg_val);
	if (ret)
		return ret;
	if (scale == LP5812_PWM_DIMMING_SCALE_LINEAR)
		reg_val &= ~(1 << (led_number % 8));
	else
		reg_val |= (1 << (led_number % 8));

	ret = lp5812_write(chip, reg, reg_val);
	if (ret)
		return ret;

	ret = lp5812_update_regs_config(chip);

	return ret;
}

static int lp5812_set_phase_align(struct lp5812_chip *chip, int led_number,
				  int phase_align_val)
{
	int ret, bit_pos;
	u16 reg;
	u8 reg_val;

	reg = chip->cfg->reg_dev_config_7.addr + (led_number / 4);
	bit_pos = (led_number % 4) * 2;

	ret = lp5812_read(chip, reg, &reg_val);
	if (ret)
		return ret;
	reg_val |= (phase_align_val << bit_pos);
	ret = lp5812_write(chip, reg, reg_val);
	if (ret)
		return ret;
	ret = lp5812_update_regs_config(chip);

	return ret;
}

static ssize_t lp5812_auto_time_pause_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t len, bool start)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0, curr_val;
	int i, ret, val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	u16 reg;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		reg = LP5812_AUTO_PAUSE_ADDR(led_cfg->led_id[i]);

		/* get original value of slope time */
		ret = lp5812_read(chip, reg, &curr_val);
		if (ret)
			return ret;

		if (start == 1)
			curr_val = (curr_val & 0x0F) | (val[i] << 4);
		else
			curr_val = (curr_val & 0xF0) | (val[i]);

		ret = lp5812_write(chip, reg, curr_val);
		if (ret)
			return -EIO;
	}

	return len;
}

static int lp5812_manual_dc_pwm_control(struct lp5812_chip *chip, int led_number,
					u8 val, enum dimming_type dimming_type)
{
	int ret;
	u16 led_base_reg;

	if (dimming_type == LP5812_DIMMING_ANALOG)
		led_base_reg = chip->cfg->reg_manual_dc_base.addr;
	else
		led_base_reg = chip->cfg->reg_manual_pwm_base.addr;
	ret = lp5812_write(chip, led_base_reg + led_number, val);

	return ret;
}

static int lp5812_auto_dc(struct lp5812_chip *chip,
			  int led_number, u8 val)
{
	return lp5812_write(chip, chip->cfg->reg_auto_dc_base.addr + led_number, val);
}

static int lp5812_multicolor_brightness(struct lp5812_led *led)
{
	struct lp5812_chip *chip = led->chip;
	int ret, i;

	guard(mutex)(&chip->lock);
	for (i = 0; i < led->mc_cdev.num_colors; i++) {
		ret = lp5812_manual_dc_pwm_control(chip, led->mc_cdev.subled_info[i].channel,
						   led->mc_cdev.subled_info[i].brightness,
						   LP5812_DIMMING_PWM);
		if (ret)
			break;
	}

	return ret;
}

static int lp5812_led_brightness(struct lp5812_led *led)
{
	struct lp5812_chip *chip = led->chip;
	struct lp5812_led_config *led_cfg;
	int ret;

	led_cfg = &chip->pdata->led_config[led->chan_nr];

	guard(mutex)(&chip->lock);
	ret = lp5812_manual_dc_pwm_control(chip, led_cfg->led_id[0],
					   led->brightness, LP5812_DIMMING_PWM);

	return ret;
}

static ssize_t mode_parse(const char *buf, enum control_mode *val)
{
	if (sysfs_streq(buf, "manual"))
		*val = LP5812_MODE_MANUAL;
	else if (sysfs_streq(buf, "autonomous"))
		*val = LP5812_MODE_AUTONOMOUS;
	else
		return -EINVAL;

	return 0;
}

static ssize_t mode_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0;
	enum control_mode val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	int i, ret;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (mode_parse(sub_str, &val[i]))
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		ret = lp5812_set_led_mode(chip, led_cfg->led_id[i], val[i]);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t activate_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	int val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	int i, ret;
	u16 reg;
	u8 reg_val, chan_nr = 0;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
		if (val[i] != 0 && val[i] != 1)
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		if (led_cfg->led_id[i] < 0x8)
			reg = chip->cfg->reg_led_en_1.addr;
		else
			reg = chip->cfg->reg_led_en_2.addr;
		ret = lp5812_read(chip, reg, &reg_val);
		if (ret == 0) {
			if (val[i] == 0) {
				ret = lp5812_write(chip, reg,
						   reg_val & (~(1 << (led_cfg->led_id[i] % 8))));
			} else {
				ret = lp5812_write(chip, reg,
						   reg_val | (1 << (led_cfg->led_id[i] % 8)));
			}
		} else {
			return -EIO;
		}
	}

	return len;
}

static ssize_t pwm_dimming_parse(const char *buf, enum pwm_dimming_scale *val)
{
	if (sysfs_streq(buf, "linear"))
		*val = LP5812_PWM_DIMMING_SCALE_LINEAR;
	else if (sysfs_streq(buf, "exponential"))
		*val = LP5812_PWM_DIMMING_SCALE_EXPONENTIAL;
	else
		return -EINVAL;

	return 0;
}

static ssize_t pwm_dimming_scale_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0;
	enum pwm_dimming_scale val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	int i, ret;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (pwm_dimming_parse(sub_str, &val[i]))
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		ret = lp5812_set_pwm_dimming_scale(chip, led_cfg->led_id[i], val[i]);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t pwm_align_parse(const char *buf, enum control_mode *val)
{
	if (sysfs_streq(buf, "forward"))
		*val = 0;
	else if (sysfs_streq(buf, "middle"))
		*val = 2;
	else if (sysfs_streq(buf, "backward"))
		*val = 3;
	else
		return -EINVAL;

	return 0;
}

static ssize_t pwm_phase_align_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0;
	enum control_mode val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	int i, ret;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (pwm_align_parse(sub_str, &val[i]))
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		ret = lp5812_set_phase_align(chip, led_cfg->led_id[i], val[i]);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t led_current_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0;
	enum control_mode mode;
	int val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	int i, ret;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		ret = lp5812_get_led_mode(chip, led_cfg->led_id[i], &mode);
		if (ret)
			return -EIO;

		if (mode == 1)
			ret = lp5812_auto_dc(chip, led_cfg->led_id[i], val[i]);
		else
			ret = lp5812_manual_dc_pwm_control(chip, led_cfg->led_id[i],
							   val[i], LP5812_DIMMING_ANALOG);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t lod_lsd_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0, i, lsd_status, lod_status;
	int size = 0, ret;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		ret = lp5812_read_lsd_status(chip, led_cfg->led_id[i], &lsd_status);
		if (!ret)
			ret = lp5812_read_lod_status(chip, led_cfg->led_id[i], &lod_status);
		if (ret)
			return -EIO;

		size += sysfs_emit_at(buf, size, "%d:%d %d\n",
			led_cfg->led_id[i], lod_status, lsd_status);
	}
	return size;
}

static ssize_t max_current_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	const char *name = dev->platform_data;
	u8 val;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chip = led->chip;

	guard(mutex)(&chip->lock);
	lp5812_read(chip, chip->cfg->reg_dev_config_0.addr, &val);
	return sysfs_emit(buf, "%d\n", (val & 0x01));
}

static ssize_t auto_time_pause_at_start_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t len)
{
	return lp5812_auto_time_pause_store(dev, attr, buf, len, 1);
}

static ssize_t auto_time_pause_at_stop_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t len)
{
	return lp5812_auto_time_pause_store(dev, attr, buf, len, 0);
}

static ssize_t auto_playback_eau_number_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0, curr_val;
	int val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	int i, ret;
	u16 reg;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		reg = LP5812_AUTO_PLAYBACK_ADDR(led_cfg->led_id[i]);

		/* get original value of slope time */
		ret = lp5812_read(chip, reg, &curr_val);
		if (ret)
			return ret;

		curr_val = (curr_val & 0x0F) | (val[i] << 4);

		ret = lp5812_write(chip, reg, curr_val);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t auto_playback_time_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	u8 chan_nr = 0, curr_val;
	int val[LED_COLOR_ID_MAX];
	char *sub_str, *str = (char *)buf;
	int i, ret;
	u16 reg;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
	}

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		reg = LP5812_AUTO_PLAYBACK_ADDR(led_cfg->led_id[i]);

		/* get original value of slope time */
		ret = lp5812_read(chip, reg, &curr_val);
		if (ret)
			return ret;

		curr_val = (curr_val & 0xF0) | (val[i]);

		ret = lp5812_write(chip, reg, curr_val);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t aeu_playback_time_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	int val[LED_COLOR_ID_MAX];
	int aeu;
	u8 chan_nr = 0, curr_val;
	char *sub_str, *str = (char *)buf;
	int i, ret;
	u16 reg;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];

	sub_str = strsep(&str, ":");
	if (!sub_str)
		return -EINVAL;
	if (kstrtoint(&sub_str[3], 0, &aeu))
		return -EINVAL;

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
		if (val[i] < 0 || val[i] > 3)
			return -EINVAL;

		reg = LP5812_AEU_PLAYBACK_ADDR(led_cfg->led_id[i], aeu);

		ret = lp5812_read(chip, reg, &curr_val);
		if (ret)
			return ret;

		ret = lp5812_write(chip, reg, (curr_val & 0xFC) | val[i]);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t lp5812_aeu_pwm_store(struct device *dev,
				    struct device_attribute *attr,
				    u8 pwm_chan,
				    const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	int val[LED_COLOR_ID_MAX];
	int aeu;
	u8 chan_nr = 0;
	char *sub_str, *str = (char *)buf;
	int i, ret;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];

	sub_str = strsep(&str, ":");
	if (!sub_str)
		return -EINVAL;
	if (kstrtoint(&sub_str[3], 0, &aeu))
		return -EINVAL;

	pr_info("AEU = %d", aeu);

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
		if (val[i] < 0 || val[i] > 255)
			return -EINVAL;

		ret = lp5812_write(chip,
				   LP5812_AEU_PWM_ADDR(led_cfg->led_id[i], aeu, pwm_chan),
				   val[i]);
		if (ret)
			return -EIO;
	}

	return len;
}

static ssize_t aeu_pwm1_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	return lp5812_aeu_pwm_store(dev, attr, LP5812_AEU_PWM1, buf, len);
}

static ssize_t aeu_pwm2_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	return lp5812_aeu_pwm_store(dev, attr, LP5812_AEU_PWM2, buf, len);
}

static ssize_t aeu_pwm3_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	return lp5812_aeu_pwm_store(dev, attr, LP5812_AEU_PWM3, buf, len);
}

static ssize_t aeu_pwm4_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	return lp5812_aeu_pwm_store(dev, attr, LP5812_AEU_PWM4, buf, len);
}

static ssize_t aeu_pwm5_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	return lp5812_aeu_pwm_store(dev, attr, LP5812_AEU_PWM5, buf, len);
}

static ssize_t lp5812_aeu_slope_time(struct device *dev,
				     struct device_attribute *attr,
				     enum slope_time_num slope_chan,
				     const char *buf, size_t len)
{
	struct lp5812_led *led;
	struct lp5812_chip *chip;
	struct lp5812_led_config *led_cfg;
	const char *name = dev->platform_data;
	int val[LED_COLOR_ID_MAX];
	u8 chan_nr = 0;
	char *sub_str, *str = (char *)buf;
	int i, ret, aeu;
	union slope_time slope_time_val;
	u16 reg;

	if (strcmp(name, LP5812_SC_LED) == 0)
		led = dev_to_lp5812_led(dev);
	else
		led = dev_to_lp5812_led_mc(dev);

	chan_nr = led->chan_nr;
	chip = led->chip;
	led_cfg = &chip->pdata->led_config[chan_nr];

	sub_str = strsep(&str, ":");
	if (!sub_str)
		return -EINVAL;
	if (kstrtoint(&sub_str[3], 0, &aeu))
		return -EINVAL;

	pr_info("AEU = %d", aeu);

	guard(mutex)(&chip->lock);
	for (i = 0; i < led_cfg->num_colors; i++) {
		sub_str = strsep(&str, " ");
		if (!sub_str)
			return -EINVAL;
		if (kstrtoint(sub_str, 0, &val[i]))
			return -EINVAL;
		if (val[i] < 0 || val[i] > 15)
			return -EINVAL;

		reg = LP5812_AEU_SLOPE_TIME_ADDR(led_cfg->led_id[i], aeu, slope_chan);

		/* get original value of slope time */
		ret = lp5812_read(chip, reg, &slope_time_val.time_val);
		if (ret)
			return ret;

		/* Update new value for slope time*/
		if (slope_chan == LP5812_SLOPE_TIME_T1 || slope_chan == LP5812_SLOPE_TIME_T3)
			slope_time_val.s_time.first = val[i];
		if (slope_chan == LP5812_SLOPE_TIME_T2 || slope_chan == LP5812_SLOPE_TIME_T4)
			slope_time_val.s_time.second = val[i];

		/* Save updated value to hardware */
		ret = lp5812_write(chip, reg, slope_time_val.time_val);
	}

	return len;
}

static ssize_t aeu_slop_time_t1_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	return lp5812_aeu_slope_time(dev, attr, LP5812_SLOPE_TIME_T1, buf, len);
}

static ssize_t aeu_slop_time_t2_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	return lp5812_aeu_slope_time(dev, attr, LP5812_SLOPE_TIME_T2, buf, len);
}

static ssize_t aeu_slop_time_t3_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	return lp5812_aeu_slope_time(dev, attr, LP5812_SLOPE_TIME_T3, buf, len);
}

static ssize_t aeu_slop_time_t4_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	return lp5812_aeu_slope_time(dev, attr, LP5812_SLOPE_TIME_T4, buf, len);
}

/* End led function */

static DEVICE_ATTR_WO(led_current);
static DEVICE_ATTR_RO(max_current);
static DEVICE_ATTR_WO(mode);
static DEVICE_ATTR_WO(activate);
static DEVICE_ATTR_WO(pwm_dimming_scale);
static DEVICE_ATTR_WO(pwm_phase_align);
static DEVICE_ATTR_WO(auto_time_pause_at_start);
static DEVICE_ATTR_WO(auto_time_pause_at_stop);
static DEVICE_ATTR_WO(auto_playback_eau_number);
static DEVICE_ATTR_WO(auto_playback_time);
static DEVICE_ATTR_WO(aeu_playback_time);
static DEVICE_ATTR_WO(aeu_pwm1);
static DEVICE_ATTR_WO(aeu_pwm2);
static DEVICE_ATTR_WO(aeu_pwm3);
static DEVICE_ATTR_WO(aeu_pwm4);
static DEVICE_ATTR_WO(aeu_pwm5);
static DEVICE_ATTR_WO(aeu_slop_time_t1);
static DEVICE_ATTR_WO(aeu_slop_time_t2);
static DEVICE_ATTR_WO(aeu_slop_time_t3);
static DEVICE_ATTR_WO(aeu_slop_time_t4);
static DEVICE_ATTR_RO(lod_lsd);

static struct attribute *lp5812_led_attrs[] = {
	&dev_attr_led_current.attr,
	&dev_attr_max_current.attr,
	&dev_attr_mode.attr,
	&dev_attr_activate.attr,
	&dev_attr_pwm_dimming_scale.attr,
	&dev_attr_pwm_phase_align.attr,
	&dev_attr_auto_time_pause_at_start.attr,
	&dev_attr_auto_time_pause_at_stop.attr,
	&dev_attr_auto_playback_eau_number.attr,
	&dev_attr_auto_playback_time.attr,
	&dev_attr_aeu_playback_time.attr,
	&dev_attr_aeu_pwm1.attr,
	&dev_attr_aeu_pwm2.attr,
	&dev_attr_aeu_pwm3.attr,
	&dev_attr_aeu_pwm4.attr,
	&dev_attr_aeu_pwm5.attr,
	&dev_attr_aeu_slop_time_t1.attr,
	&dev_attr_aeu_slop_time_t2.attr,
	&dev_attr_aeu_slop_time_t3.attr,
	&dev_attr_aeu_slop_time_t4.attr,
	&dev_attr_lod_lsd.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lp5812_led);

static int lp5812_set_brightness(struct led_classdev *cdev,
				 enum led_brightness brightness)
{
	struct lp5812_led *led = cdev_to_lp5812_led(cdev);
	const struct lp5812_device_config *cfg = led->chip->cfg;

	led->brightness = (u8)brightness;
	return cfg->brightness_fn(led);
}

static int lp5812_set_mc_brightness(struct led_classdev *cdev,
				    enum led_brightness brightness)
{
	struct led_classdev_mc *mc_dev = lcdev_to_mccdev(cdev);
	struct lp5812_led *led = mcled_cdev_to_lp5812_led(mc_dev);
	const struct lp5812_device_config *cfg = led->chip->cfg;

	led_mc_calc_color_components(&led->mc_cdev, brightness);
	return cfg->multicolor_brightness_fn(led);
}

static int lp5812_init_led(struct lp5812_led *led, struct lp5812_chip *chip, int chan)
{
	struct lp5812_data *pdata = chip->pdata;
	struct device *dev = &chip->i2c_cl->dev;
	struct mc_subled *mc_led_info;
	struct led_classdev *led_cdev;
	char name[32];
	int i, ret = 0;

	if (pdata->led_config[chan].name) {
		led->cdev.name = pdata->led_config[chan].name;
	} else {
		snprintf(name, sizeof(name), "%s:channel%d",
			 pdata->label ? : chip->i2c_cl->name, chan);
		led->cdev.name = name;
	}

	if (pdata->led_config[chan].is_sc_led == 0) {
		mc_led_info = devm_kcalloc(dev,
					   pdata->led_config[chan].num_colors,
					   sizeof(*mc_led_info), GFP_KERNEL);
		if (!mc_led_info)
			return -ENOMEM;

		led_cdev = &led->mc_cdev.led_cdev;
		led_cdev->name = led->cdev.name;
		led_cdev->brightness_set_blocking = lp5812_set_mc_brightness;
		led->mc_cdev.num_colors = pdata->led_config[chan].num_colors;
		for (i = 0; i < led->mc_cdev.num_colors; i++) {
			mc_led_info[i].color_index =
				pdata->led_config[chan].color_id[i];
			mc_led_info[i].channel =
					pdata->led_config[chan].led_id[i];
		}

		led->mc_cdev.subled_info = mc_led_info;
	} else {
		led->cdev.brightness_set_blocking = lp5812_set_brightness;
	}

	led->cdev.groups = lp5812_led_groups;
	led->chan_nr = chan;

	if (pdata->led_config[chan].is_sc_led) {
		ret = devm_led_classdev_register(dev, &led->cdev);
		if (ret == 0) {
			led->cdev.dev->platform_data = devm_kstrdup(dev, LP5812_SC_LED, GFP_KERNEL);
			if (!led->cdev.dev->platform_data)
				return -ENOMEM;
		}
	} else {
		ret = devm_led_classdev_multicolor_register(dev, &led->mc_cdev);
		if (ret == 0) {
			led->mc_cdev.led_cdev.dev->platform_data =
				devm_kstrdup(dev, LP5812_MC_LED, GFP_KERNEL);
			if (!led->mc_cdev.led_cdev.dev->platform_data)
				return -ENOMEM;

			ret = sysfs_create_groups(&led->mc_cdev.led_cdev.dev->kobj,
						  lp5812_led_groups);
			if (ret)
				dev_err(dev, "sysfs_create_groups failed\n");
		}
	}

	if (ret) {
		dev_err(dev, "led register err: %d\n", ret);
		return ret;
	}

	return 0;
}

static int lp5812_register_leds(struct lp5812_led *led, struct lp5812_chip *chip)
{
	int num_channels = chip->pdata->num_channels;
	struct lp5812_led *each;
	int ret, i, j;

	for (i = 0; i < num_channels; i++) {
		each = led + i;
		ret = lp5812_init_led(each, chip, i);
		if (ret)
			goto err_init_led;

		each->chip = chip;

		for (j = 0; j < chip->pdata->led_config[i].num_colors; j++) {
			ret = lp5812_auto_dc(chip, chip->pdata->led_config[i].led_id[j],
					     chip->pdata->led_config[i].led_id[j]);
			if (ret)
				goto err_init_led;

			ret = lp5812_manual_dc_pwm_control(chip,
							chip->pdata->led_config[i].led_id[j],
							chip->pdata->led_config[i].led_current[j],
							LP5812_DIMMING_ANALOG);
			if (ret)
				goto err_init_led;
		}
	}

	return 0;

err_init_led:
	return ret;
}

static int lp5812_register_sysfs(struct lp5812_chip *chip)
{
	struct device *dev = &chip->i2c_cl->dev;
	const struct lp5812_device_config *cfg = chip->cfg;
	int ret;

	ret = sysfs_create_group(&dev->kobj, cfg->dev_attr_group);
	if (ret)
		return ret;

	return 0;
}

static void lp5812_unregister_sysfs(struct lp5812_led *led, struct lp5812_chip *chip)
{
	struct device *dev = &chip->i2c_cl->dev;
	const struct lp5812_device_config *cfg = chip->cfg;
	struct lp5812_led *each;
	int i;

	sysfs_remove_group(&dev->kobj, cfg->dev_attr_group);

	for (i = 0; i < chip->pdata->num_channels; i++) {
		if (!chip->pdata->led_config[i].is_sc_led) {
			each = led + i;
			sysfs_remove_groups(&each->mc_cdev.led_cdev.dev->kobj, lp5812_led_groups);
		}
	}
}

static int lp5812_probe(struct i2c_client *client)
{
	struct lp5812_chip *chip;
	int ret;
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct lp5812_data *pdata = dev_get_platdata(&client->dev);
	struct device_node *np = dev_of_node(&client->dev);
	struct lp5812_led *led;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->cfg = i2c_get_match_data(client);

	if (!pdata) {
		if (np) {
			pdata = lp5812_of_populate_pdata(&client->dev, np,
							 chip);
			if (IS_ERR(pdata))
				return PTR_ERR(pdata);
		} else {
			return dev_err_probe(&client->dev, -EINVAL, "no platform data\n");
		}
	}

	led = devm_kcalloc(&client->dev,
			   pdata->num_channels, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	chip->i2c_cl = client;
	chip->pdata = pdata;

	mutex_init(&chip->lock);

	i2c_set_clientdata(client, led);

	ret = lp5812_init_device(chip);
	if (ret)
		goto err_init;

	dev_info(&client->dev, "%s Programmable led chip found\n", id->name);

	ret = lp5812_register_leds(led, chip);
	if (ret)
		goto err_out;

	ret = lp5812_register_sysfs(chip);
	if (ret) {
		dev_err_probe(&client->dev, ret, "registering sysfs failed\n");
		goto err_out;
	}

	return 0;

err_out:
	lp5812_deinit_device(chip);
err_init:
	return ret;
}

static void lp5812_remove(struct i2c_client *client)
{
	struct lp5812_led *led = i2c_get_clientdata(client);

	lp5812_unregister_sysfs(led, led->chip);
	lp5812_deinit_device(led->chip);

	dev_info(&client->dev, "Removed driver\n");
}

static LP5812_DEV_ATTR_WO(dev_config);
static LP5812_DEV_ATTR_WO(device_command);
static LP5812_DEV_ATTR_WO(sw_reset);
static LP5812_DEV_ATTR_WO(fault_clear);
static LP5812_DEV_ATTR_RO(tsd_config_status);

static struct attribute *lp5812_chip_attributes[] = {
	&dev_attr_device_command.attr,
	&dev_attr_fault_clear.attr,
	&dev_attr_sw_reset.attr,
	&dev_attr_dev_config.attr,
	&dev_attr_tsd_config_status.attr,
	NULL,
};

static const struct attribute_group lp5812_group = {
	.name = "lp5812_chip_setup",
	.attrs = lp5812_chip_attributes,
};

/* Chip specific configurations */
static struct lp5812_device_config lp5812_cfg = {
	.reg_reset = {
		.addr = LP5812_REG_RESET,
		.val  = LP5812_RESET,
	},
	.reg_chip_en = {
		.addr = LP5812_REG_ENABLE,
		.val  = LP5812_ENABLE_DEFAULT,
	},
	.reg_dev_config_0 = {
		.addr = LP5812_DEV_CONFIG0,
		.val  = 0,
	},
	.reg_dev_config_1 = {
		.addr = LP5812_DEV_CONFIG1,
		.val  = 0,
	},
	.reg_dev_config_2 = {
		.addr = LP5812_DEV_CONFIG2,
		.val  = 0,
	},
	.reg_dev_config_3 = {
		.addr = LP5812_DEV_CONFIG3,
		.val  = 0,
	},
	.reg_dev_config_4 = {
		.addr = LP5812_DEV_CONFIG4,
		.val  = 0,
	},
	.reg_dev_config_5 = {
		.addr = LP5812_DEV_CONFIG5,
		.val  = 0,
	},
	.reg_dev_config_6 = {
		.addr = LP5812_DEV_CONFIG6,
		.val  = 0,
	},
	.reg_dev_config_7 = {
		.addr = LP5812_DEV_CONFIG7,
		.val  = 0,
	},
	.reg_dev_config_12 = {
		.addr = LP5812_DEV_CONFIG12,
		.val  = LP5812_DEV_CONFIG12_DEFAULT,
	},
	.reg_cmd_update = {
		.addr = LP5812_CMD_UPDATE,
		.val  = 0,
	},
	.reg_cmd_start = {
		.addr = LP5812_CMD_START,
		.val  = 0,
	},
	.reg_cmd_stop = {
		.addr = LP5812_CMD_STOP,
		.val  = 0,
	},
	.reg_cmd_pause = {
		.addr = LP5812_CMD_PAUSE,
		.val  = 0,
	},
	.reg_cmd_continue = {
		.addr = LP5812_CMD_CONTINUE,
		.val  = 0,
	},
	.reg_tsd_config_status = {
		.addr = LP5812_TSD_CONFIG_STATUS,
		.val  = 0,
	},
	.reg_led_en_1 = {
		.addr = LP5812_LED_EN_1,
		.val  = 0,
	},
	.reg_led_en_2 = {
		.addr = LP5812_LED_EN_2,
		.val  = 0,
	},
	.reg_fault_clear = {
		.addr = LP5812_FAULT_CLEAR,
		.val  = 0,
	},
	.reg_manual_dc_base  = {
		.addr = LP5812_MANUAL_DC_BASE,
		.val  = 0,
	},
	.reg_auto_dc_base  = {
		.addr = LP5812_AUTO_DC_BASE,
		.val  = 0,
	},
	.reg_manual_pwm_base  = {
		.addr = LP5812_MANUAL_PWM_BASE,
		.val  = 0,
	},
	.reg_aeu_base  = {
		.addr = LP5812_AEU_BASE,
		.val  = 0,
	},
	.reg_lod_status_base  = {
		.addr = LP5812_LOD_STATUS,
		.val  = 0,
	},
	.reg_lsd_status_base  = {
		.addr = LP5812_LSD_STATUS,
		.val  = 0,
	},

	.brightness_fn	  = lp5812_led_brightness,
	.multicolor_brightness_fn = lp5812_multicolor_brightness,

	.dev_attr_group = &lp5812_group
};

static const struct i2c_device_id lp5812_id[] = {
	{ "lp5812", .driver_data = (kernel_ulong_t)&lp5812_cfg, },
	{ }
};

MODULE_DEVICE_TABLE(i2c, lp5812_id);

#ifdef CONFIG_OF
static const struct of_device_id of_lp5812_match[] = {
	{ .compatible = "ti,lp5812", .data = &lp5812_cfg, },
	{/* NULL */}
};

MODULE_DEVICE_TABLE(of, of_lp5812_match);
#endif

static struct i2c_driver lp5812_driver = {
	.driver = {
		.name   = "lp5812",
		.of_match_table = of_match_ptr(of_lp5812_match),
	},
	.probe		= lp5812_probe,
	.remove		= lp5812_remove,
	.id_table	= lp5812_id
};

module_i2c_driver(lp5812_driver);

MODULE_DESCRIPTION("Texas Instruments LP5812 LED Driver");
MODULE_AUTHOR("Jared Zhou");
MODULE_LICENSE("GPL");
