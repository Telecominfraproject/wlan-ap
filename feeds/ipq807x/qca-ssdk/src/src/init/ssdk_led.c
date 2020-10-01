/*
 * Copyright (c) 2012, 2014-2019, The Linux Foundation. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "sw.h"
#include "ssdk_init.h"
#include "ssdk_led.h"
#include <linux/kconfig.h>
#include <linux/version.h>
#include <linux/kernel.h>

#if defined(CONFIG_OF)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0))
#include <drivers/leds/leds-ipq.h>
#endif
#else
#include <drivers/leds/leds-ipq40xx.h>
#endif
#endif


#ifdef DESS
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0))
static int ssdk_dess_led_source_select(int led_num, enum led_source src_type)
{
#if defined(CONFIG_OF)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0))
	ipq_led_source_select(led_num, src_type);
#else
	ipq40xx_led_source_select(led_num, src_type);
#endif
#endif
	return 0;
}
#endif

int ssdk_dess_led_init(ssdk_init_cfg *cfg)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0))
	a_uint32_t i,led_num, led_source_id,source_id;
	led_ctrl_pattern_t  pattern;

	if(cfg->led_source_num != 0) {
		for (i = 0; i < cfg->led_source_num; i++) {

			led_source_id = cfg->led_source_cfg[i].led_source_id;
			pattern.mode = cfg->led_source_cfg[i].led_pattern.mode;
			pattern.map = cfg->led_source_cfg[i].led_pattern.map;
			pattern.freq = cfg->led_source_cfg[i].led_pattern.freq;
#ifdef IN_LED
			fal_led_source_pattern_set(0, led_source_id,&pattern);
#endif
			led_num = ((led_source_id -1) /3) + 3;
			source_id = led_source_id % 3;
			if (source_id == SRC_SELECTION_1) {
				if (led_source_id == LED_SRC_ID_1) {
					 ssdk_dess_led_source_select(led_num,
						LAN0_1000_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_4) {
					 ssdk_dess_led_source_select(led_num,
					 	LAN1_1000_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_7) {
					 ssdk_dess_led_source_select(led_num,
					 	LAN2_1000_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_10) {
					ssdk_dess_led_source_select(led_num,
						LAN3_1000_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_13) {
					ssdk_dess_led_source_select(led_num,
						WAN_1000_LNK_ACTIVITY);
				}
			}
			if (source_id == SRC_SELECTION_2) {
				if (led_source_id == LED_SRC_ID_2) {
					ssdk_dess_led_source_select(led_num,
						LAN0_100_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_5) {
					ssdk_dess_led_source_select(led_num,
						LAN1_100_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_8) {
					ssdk_dess_led_source_select(led_num,
						LAN2_100_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_11) {
					ssdk_dess_led_source_select(led_num,
						LAN3_100_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_14) {
					ssdk_dess_led_source_select(led_num,
						WAN_100_LNK_ACTIVITY);
				}
			}
			if (source_id == SRC_SELECTION_0) {
				if (led_source_id == LED_SRC_ID_3) {
					ssdk_dess_led_source_select(led_num,
						LAN0_10_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_6) {
					ssdk_dess_led_source_select(led_num,
						LAN1_10_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_9) {
					ssdk_dess_led_source_select(led_num,
						LAN2_10_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_12) {
					ssdk_dess_led_source_select(led_num,
						LAN3_10_LNK_ACTIVITY);
				}
				if (led_source_id == LED_SRC_ID_15) {
					ssdk_dess_led_source_select(led_num,
						WAN_10_LNK_ACTIVITY);
				}
			}
		}
	}
#endif
	return 0;
}
#endif


