// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <ath/ath_dp_accel_cfg.h>
#include <linux/module.h>

const struct ath_dp_accel_cfg_ops *ath_dp_accel_cfg_cb;

int ath_dp_accel_cfg_ops_callback_register(const struct ath_dp_accel_cfg_ops *ath_cb)
{
	if (!ath_cb) {
		pr_err("Failed to register accel cfg callbacks\n");
		return -EINVAL;
	}
	ath_dp_accel_cfg_cb = ath_cb;
	return 0;
}
EXPORT_SYMBOL(ath_dp_accel_cfg_ops_callback_register);

void ath_dp_accel_cfg_ops_callback_unregister(void)
{
	ath_dp_accel_cfg_cb = NULL;
}
EXPORT_SYMBOL(ath_dp_accel_cfg_ops_callback_unregister);

bool ath_dp_accel_cfg_fetch_ds_node_id(struct ath_dp_accel_cfg *info)
{
	struct net_device *dev = info->in_dest_dev;
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	struct ieee80211_hw *hw;

	if (!ath_dp_accel_cfg_cb)
		return false;

	wdev = dev->ieee80211_ptr;
	if (!wdev) {
		/*
		 * If the netdev is vlan, it may not have ieee80211_ptr.
		 * In that case fetch the ieee80211_ptr from its top most parent
		 */
		if (is_vlan_dev(dev)) {
			struct net_device *parent_ndev =
					vlan_dev_real_dev(dev);
			if (parent_ndev)
				wdev = parent_ndev->ieee80211_ptr;

			if (!wdev)
				return false;
		} else {
			return false;
		}
	}

	hw = wiphy_to_ieee80211_hw(wdev->wiphy);
	if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION))
		return false;

	vif = wdev_to_ieee80211_vif_vlan(wdev, false);
	if (!vif)
		return false;

	return ath_dp_accel_cfg_cb->ppeds_get_node_id(vif, wdev,
						info->in_dest_mac,
						&info->out_ppe_ds_node_id);
}
EXPORT_SYMBOL(ath_dp_accel_cfg_fetch_ds_node_id);

u32 ath_get_metadata_info(struct ath_dp_metadata_param *md_param)
{
	if (!ath_dp_accel_cfg_cb)
		return 0;

	return ath_dp_accel_cfg_cb->get_metadata_info(md_param);
}
EXPORT_SYMBOL(ath_get_metadata_info);

void ath_sawf_uplink(struct ath_ul_params *params)
{
	if (!ath_dp_accel_cfg_cb)
		return;

	ath_dp_accel_cfg_cb->sdwf_ul_config(params);
}
EXPORT_SYMBOL(ath_sawf_uplink);

int ath_mscs_peer_lookup_n_get_priority(struct ath_mscs_get_priority_param *params)
{
	if (!ath_dp_accel_cfg_cb)
		return -EINVAL;

	return ath_dp_accel_cfg_cb->get_mscs_priority(params);

}
EXPORT_SYMBOL(ath_mscs_peer_lookup_n_get_priority);

