/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * LP5812 Driver Header
 *
 * Copyright (C) 2025 Texas Instruments
 *
 * Author: Jared Zhou <jared-zhou@ti.com>
 */

#ifndef _LP5812_H_
#define _LP5812_H_

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/led-class-multicolor.h>

#define LP5812_REG_ENABLE				0x0000
#define LP5812_REG_RESET				0x0023
#define LP5812_DEV_CONFIG0				0x0001
#define LP5812_DEV_CONFIG1				0x0002
#define LP5812_DEV_CONFIG2				0x0003
#define LP5812_DEV_CONFIG3				0x0004
#define LP5812_DEV_CONFIG4				0x0005
#define LP5812_DEV_CONFIG5				0x0006
#define LP5812_DEV_CONFIG6				0x0007
#define LP5812_DEV_CONFIG7				0x0008
#define LP5812_DEV_CONFIG8				0x0009
#define LP5812_DEV_CONFIG9				0x000A
#define LP5812_DEV_CONFIG10				0x000B
#define LP5812_DEV_CONFIG11				0x000c
#define LP5812_DEV_CONFIG12				0x000D
#define LP5812_CMD_UPDATE				0x0010
#define LP5812_CMD_START				0x0011
#define LP5812_CMD_STOP					0x0012
#define LP5812_CMD_PAUSE				0x0013
#define LP5812_CMD_CONTINUE				0x0014
#define LP5812_LED_EN_1					0x0020
#define LP5812_LED_EN_2					0x0021
#define LP5812_FAULT_CLEAR				0x0022
#define LP5812_MANUAL_DC_BASE				0x0030
#define LP5812_AUTO_DC_BASE				0x0050
#define LP5812_MANUAL_PWM_BASE				0x0040
#define LP5812_AEU_BASE					0x0080

#define LP5812_TSD_CONFIG_STATUS			0x0300
#define LP5812_LOD_STATUS				0x0301
#define LP5812_LSD_STATUS				0x0303

#define LP5812_ENABLE_DEFAULT				0x01
#define FAULT_CLEAR_ALL					0x07
#define TSD_CLEAR_VAL					0x04
#define LSD_CLEAR_VAL					0x02
#define LOD_CLEAR_VAL					0x01
#define LP5812_RESET					0x66
#define LP5812_DEV_CONFIG12_DEFAULT			0x08

#define LP5812_UPDATE_CMD_VAL				0x55
#define LP5812_START_CMD_VAL				0xFF
#define LP5812_STOP_CMD_VAL				0xAA
#define LP5812_PAUSE_CMD_VAL				0x33
#define LP5812_CONTINUE_CMD_VAL				0xCC

#define LP5812_DEV_ATTR_RW(name)  \
	DEVICE_ATTR_RW(name)
#define LP5812_DEV_ATTR_RO(name)  \
	DEVICE_ATTR_RO(name)
#define LP5812_DEV_ATTR_WO(name)  \
	DEVICE_ATTR_WO(name)

enum control_mode {
	LP5812_MODE_MANUAL = 0,
	LP5812_MODE_AUTONOMOUS
};

enum dimming_type {
	LP5812_DIMMING_ANALOG,
	LP5812_DIMMING_PWM
};

enum pwm_dimming_scale {
	LP5812_PWM_DIMMING_SCALE_LINEAR = 0,
	LP5812_PWM_DIMMING_SCALE_EXPONENTIAL
};

enum device_command {
	LP5812_DEV_CMD_NONE,
	LP5812_DEV_CMD_UPDATE,
	LP5812_DEV_CMD_START,
	LP5812_DEV_CMD_STOP,
	LP5812_DEV_CMD_PAUSE,
	LP5812_DEV_CMD_CONTINUE
};

enum slope_time_num {
	LP5812_SLOPE_TIME_T1 = 0,
	LP5812_SLOPE_TIME_T2,
	LP5812_SLOPE_TIME_T3,
	LP5812_SLOPE_TIME_T4
};

enum aeu_pwm_num {
	LP5812_AEU_PWM1 = 1,
	LP5812_AEU_PWM2,
	LP5812_AEU_PWM3,
	LP5812_AEU_PWM4,
	LP5812_AEU_PWM5
};

union slope_time {
	struct {
		u8 first:4;
		u8 second:4;
	} __packed s_time;
	u8 time_val;
}; /* type for start/stop pause time and slope time */

union u_scan_order {
	struct {
		u8 scan_order_0:2;
		u8 scan_order_1:2;
		u8 scan_order_2:2;
		u8 scan_order_3:2;
	} s_scan_order;
	u8 scan_order_val;
};

union u_drive_mode {
	struct {
		u8 mix_sel_led_0:1;
		u8 mix_sel_led_1:1;
		u8 mix_sel_led_2:1;
		u8 mix_sel_led_3:1;
		u8 led_mode:3;
		u8 pwm_fre:1;
	} s_drive_mode;
	u8 drive_mode_val;
};

struct lp5812_reg {
	u16 addr;
	union {
		u8 val;
		u8 mask;
		u8 shift;
	};
};

struct lp5812_led;

struct lp5812_device_config {
	const struct lp5812_reg reg_reset;
	const struct lp5812_reg reg_chip_en;
	const struct lp5812_reg reg_dev_config_0;
	const struct lp5812_reg reg_dev_config_1;
	const struct lp5812_reg reg_dev_config_2;
	const struct lp5812_reg reg_dev_config_3;
	const struct lp5812_reg reg_dev_config_4;
	const struct lp5812_reg reg_dev_config_5;
	const struct lp5812_reg reg_dev_config_6;
	const struct lp5812_reg reg_dev_config_7;
	const struct lp5812_reg reg_dev_config_12;
	const struct lp5812_reg reg_cmd_update;
	const struct lp5812_reg reg_cmd_start;
	const struct lp5812_reg reg_cmd_stop;
	const struct lp5812_reg reg_cmd_pause;
	const struct lp5812_reg reg_cmd_continue;

	const struct lp5812_reg reg_led_en_1;
	const struct lp5812_reg reg_led_en_2;
	const struct lp5812_reg reg_fault_clear;
	const struct lp5812_reg reg_manual_dc_base;
	const struct lp5812_reg reg_auto_dc_base;
	const struct lp5812_reg reg_manual_pwm_base;
	const struct lp5812_reg reg_tsd_config_status;
	const struct lp5812_reg reg_aeu_base;
	const struct lp5812_reg reg_lod_status_base;
	const struct lp5812_reg reg_lsd_status_base;

	/* set LED brightness */
	int (*brightness_fn)(struct lp5812_led *led);

	/* set multicolor LED brightness */
	int (*multicolor_brightness_fn)(struct lp5812_led *led);

	/* additional device specific attributes */
	const struct attribute_group *dev_attr_group;
};

struct lp5812_led_config {
	const char *name;
	int led_id[LED_COLOR_ID_MAX];
	u8 color_id[LED_COLOR_ID_MAX];
	u8 led_current[LED_COLOR_ID_MAX];
	u8 max_current[LED_COLOR_ID_MAX];
	int num_colors;
	u8 chan_nr;
	bool is_sc_led;
};

struct lp5812_data {
	struct lp5812_led_config *led_config;
	u8 num_channels;
	const char *label;
};

struct lp5812_chip {
	struct i2c_client *i2c_cl;
	struct mutex lock; /* Protects reg access */
	struct lp5812_data *pdata;
	const struct lp5812_device_config *cfg;
	union u_scan_order u_scan_order;
	union u_drive_mode u_drive_mode;
};

struct lp5812_led {
	int chan_nr;
	struct led_classdev cdev;
	struct led_classdev_mc mc_cdev;
	u8 brightness;
	struct lp5812_chip *chip;
};

#endif /*_LP5812_H_*/
