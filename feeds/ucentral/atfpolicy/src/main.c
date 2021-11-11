// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <libubox/uloop.h>

#include "atf.h"

struct atf_config config;
int debug_flag;

void reset_config(void)
{
	memset(&config, 0, sizeof(config));

	config.voice_queue_weight = 4;
	config.min_pkt_thresh = 100;

	config.bulk_percent_thresh = (50 << ATF_AVG_SCALE) / 100;
	config.prio_percent_thresh = (30 << ATF_AVG_SCALE) / 100;

	config.weight_normal = 256;
	config.weight_bulk = 128;
	config.weight_prio = 512;
}

static void atf_update_cb(struct uloop_timeout *t)
{
	atf_interface_update_all();
	uloop_timeout_set(t, 1000);
}

int main(int argc, char **argv)
{
	static struct uloop_timeout update_timer = {
		.cb = atf_update_cb,
	};
	int ch;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			debug_flag = 1;
			break;
		}
	}

	reset_config();
	uloop_init();
	atf_ubus_init();
	atf_nl80211_init();
	atf_update_cb(&update_timer);
	uloop_run();
	atf_ubus_stop();
	uloop_done();

	return 0;
}
