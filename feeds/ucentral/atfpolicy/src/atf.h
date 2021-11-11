// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#ifndef __ATF_H
#define __ATF_H

#include <net/if.h>
#include <stdint.h>

#include <libubox/avl.h>

#define ATF_AVG_SCALE	12

#define ATF_AVG_WEIGHT_FACTOR	3
#define ATF_AVG_WEIGHT_DIV	4

#define MAC_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ADDR_DATA(_a) \
	((const uint8_t *)(_a))[0], \
	((const uint8_t *)(_a))[1], \
	((const uint8_t *)(_a))[2], \
	((const uint8_t *)(_a))[3], \
	((const uint8_t *)(_a))[4], \
	((const uint8_t *)(_a))[5]

#define D(format, ...) do { \
	if (debug_flag) \
		fprintf(stderr, "DEBUG: %s(%d) " format "\n", __func__, __LINE__, ## __VA_ARGS__); \
	} while (0)

struct atf_config {
	int voice_queue_weight;
	int min_pkt_thresh;

	int bulk_percent_thresh;
	int prio_percent_thresh;

	int weight_normal;
	int weight_prio;
	int weight_bulk;
};

struct atf_interface {
	struct avl_node avl;

	char ifname[IFNAMSIZ + 1];
	uint32_t ubus_obj;

	struct avl_tree stations;
};

struct atf_stats {
	uint64_t bulk, normal, prio;
};

struct atf_station {
	struct avl_node avl;
	uint8_t macaddr[6];
	bool present;

	uint8_t stats_idx;
	struct atf_stats stats[2];

	uint16_t avg_bulk;
	uint16_t avg_prio;

	int weight;
};

extern struct atf_config config;
extern int debug_flag;

void reset_config(void);

struct atf_interface *atf_interface_get(const char *ifname);
void atf_interface_sta_update(struct atf_interface *iface);
struct atf_station *atf_interface_sta_get(struct atf_interface *iface, uint8_t *macaddr);
void atf_interface_sta_changed(struct atf_interface *iface, struct atf_station *sta);
void atf_interface_sta_flush(struct atf_interface *iface);
void atf_interface_update_all(void);

int atf_ubus_init(void);
void atf_ubus_stop(void);
void atf_ubus_set_sta_weight(struct atf_interface *iface, struct atf_station *sta);

int atf_nl80211_init(void);
int atf_nl80211_interface_update(struct atf_interface *iface);

#endif
