/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH_DP_ACCEL_CFG_H
#define ATH_DP_ACCEL_CFG_H
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <net/mac80211.h>

#define ATH_WIFI_NSS_PLUGIN_ENABLE 1

#define ATH_SAWF_SVID_VALID 0x1
#define ATH_SAWF_DSCP_VALID 0x2
#define ATH_SAWF_PCP_VALID  0x4
#define QCA_WIFI_NSS_PLUGINS_MSCS 1
/*
 * wifi classifier metadata
 * ----------------------------------------------------------------------------
 * | tag    | mlo_key_valid| sawf_valid| reserved| mlo key | peer_id | msduq   |
 * |(8 bits)|   (1 bit)    |  (1 bit)  | (1 bit) | (5 bits)| (10 bit)| (6 bits)|
 * ----------------------------------------------------------------------------
 */


/**
 * struct mlo_param - mlo metadata params
 * @in_dest_Dev: input parameter netdev handle
 * @in_dest_mac: input parameter peer mac address
 * @out_ppe_ds_node_id: output parameter ds node id
 */

struct  mlo_param {
	struct net_device *in_dest_dev;
	u8      *in_dest_mac;
	u8      out_ppe_ds_node_id;
};

/**
 * struct sawf_param - sawf metadata params
 * @netdev : Netdevice
 * @peer_mac : Destination peer mac address
 * @service_id : Service class id
 * @dscp : Differentiated Services Code Point
 * @rule_id : Rule id
 * @sawf_rule_type: Rule type
 * @pcp: pcp value
 * @valid_flag: flag to indicate if pcp is valid or not
 * @mcast_flag: flag to indicate if query is for multicast
 */

struct  sawf_param {
	struct net_device *netdev;
	u8      *peer_mac,
		sawf_rule_type,
		mcast_flag:1;
	u32     service_id,
		dscp,
		rule_id,
		pcp,
		valid_flag;
};

/**
 * struct ath_dp_metadata_param - wifi classifier metadata
 * @mlo_param: mlo metadata info
 * @sawf_param: sawf param
 */

struct	ath_dp_metadata_param {
	uint8_t is_mlo_param_valid:1,
		is_sawf_param_valid:1,
		is_scs_mscs:1,
		reserved:5;
	struct	mlo_param	mlo_param;
	struct	sawf_param	sawf_param;
};

struct ath_ul_params {
	struct net_device *dst_dev;
	struct net_device *src_dev;
	u8 *dst_mac;
	u8 *src_mac;
	u8 fw_service_id;
	u8 rv_service_id;
	u8 start_or_stop;
	u32 fw_mark_metadata;
	u32 rv_mark_metadata;
};

/**
 * ath_mscs_get_priority_param
 *
 * @dst_dev - Destination netdev
 * @src_dev - Source netdev
 * @src_mac - Source mac address
 * @dest_mac - Destination mac address
 * @skb - Socket buffer
 */
struct ath_mscs_get_priority_param {
	struct net_device *dst_dev;
	struct net_device *src_dev;
	uint8_t *src_mac;
	uint8_t *dst_mac;
	struct sk_buff *skb;
};

/**
 * struct ath_dp_accel_cfg_ops - dp accelerator configuraton ops
 * @ppeds_get_node_id: fetch ds node id from ath driver for given peer mac
 * @get_mscs_priority: update skb priority based on MSCS context for a given
 * MSCS session
 *
 */
struct ath_dp_accel_cfg_ops {
	bool (*ppeds_get_node_id)(struct ieee80211_vif *vif,
				  struct wireless_dev *wdev,
				  const u8 *peer_mac, u8 *node_id);

	uint32_t (*get_metadata_info)(struct ath_dp_metadata_param *md_param);
	void (*sdwf_ul_config)(struct ath_ul_params *params);
	int (*get_mscs_priority)(struct ath_mscs_get_priority_param *params);
};

/**
 * struct ath_dp_accel_cfg - config retrieval buffer
 * @in_dest_dev: input parameter netdev handle
 * @in_dest_mac: input parameter peer mac address
 * @out_ppe_ds_node_id: output parameter ds node id
 */
struct ath_dp_accel_cfg {
	struct net_device *in_dest_dev;
	u8 *in_dest_mac;
	u8 out_ppe_ds_node_id;
};

/**
 * ath_dp_accel_cfg_ops_callback_register() - register accel cfg ops
 * @ath_cb: accel_cfg_ops to register
 *
 * Return: 0 - success, failure otherwise
 */

int ath_dp_accel_cfg_ops_callback_register(const struct ath_dp_accel_cfg_ops *ath_cb);

/**
 * ath_dp_accel_cfg_ops_callback_unregister() - unregister accel cfg ops
 *
 * Return: None
 */
void ath_dp_accel_cfg_ops_callback_unregister(void);

/**
 * ath_dp_accel_cfg_fetch_ds_node_id() - Retrieve ds node id
 * @ds_info: info required to fetch the node id
 *
 * Return: true - success, false - failure
 */
bool ath_dp_accel_cfg_fetch_ds_node_id(struct ath_dp_accel_cfg *info);

u32 ath_get_metadata_info(struct ath_dp_metadata_param *dp_metadata_param);
void ath_sawf_uplink(struct ath_ul_params *params);
int ath_mscs_peer_lookup_n_get_priority(struct ath_mscs_get_priority_param *params);
#endif
