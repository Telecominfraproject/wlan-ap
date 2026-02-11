/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef ATH_FSE_H
#define ATH_FSE_H
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <net/mac80211.h>

#define ATH_FSE_TID_INVALID 0xff

struct ath_fse_ops {
	int (*fse_rule_add)(void *ab,
			    u32 *src_ip, u32 src_port,
			    u32 *dest_ip, u32 dest_port,
			    u8 protocol, u8 version);
	int (*fse_rule_delete)(void *ab,
			       u32 *src_ip, u32 src_port,
			       u32 *dest_ip, u32 dest_port,
			       u8 protocol, u8 version);
	void* (*fse_get_ab)(struct ieee80211_vif *vif, const u8 *peer_mac);
};

struct ath_fse_flow_info {
	u32 src_ip[4];
	u32 src_port;
	u32 dest_ip[4];
	u32 dest_port;
	u8 protocol;
	u8 version;
	struct net_device *src_dev;
	struct net_device *dest_dev;
	u8 *src_mac;
	u8 *dest_mac;
	u32 fw_svc_id;
	u32 rv_svc_id;
};

int ath_fse_ops_callback_register(const struct ath_fse_ops *ath_cb);

void ath_fse_ops_callback_unregister(void);

bool ath_fse_add_rule(struct ath_fse_flow_info *fse_info);

bool ath_fse_delete_rule(struct ath_fse_flow_info *fse_info);

#endif
