// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <ath/ath_fse.h>
#include <linux/module.h>

struct ath_fse_ops *fse_cb;

int ath_fse_ops_callback_register(const struct ath_fse_ops *ath_cb)
{
	if (!ath_cb) {
		pr_err("Failed to register FSE callbacks\n");
		return -EINVAL;
	}
	fse_cb = (struct ath_fse_ops *)ath_cb;
	pr_debug("FSE callbacks are registered successfully to ath\n");
	return 0;
}
EXPORT_SYMBOL(ath_fse_ops_callback_register);

void ath_fse_ops_callback_unregister(void)
{
	fse_cb = NULL;
}
EXPORT_SYMBOL(ath_fse_ops_callback_unregister);

bool ath_fse_add_rule(struct ath_fse_flow_info *fse_info)
{
	struct wireless_dev *src_wdev, *dest_wdev;
	struct ieee80211_vif *vif;
	struct ieee80211_hw *hw;
	int ret;
	void *ab;

	if (!fse_cb)
		return false;

	if (!fse_info->src_dev || !fse_info->dest_dev) {
		pr_warn("Unable to find dev for FSE rule push\n");
		return false;
	}

	src_wdev = fse_info->src_dev->ieee80211_ptr;
	dest_wdev = fse_info->dest_dev->ieee80211_ptr;
	if (!src_wdev && !dest_wdev) {
		pr_debug("Not a wlan traffic for FSE rule push:sdev:%s ddev:%s\n",
			 netdev_name(fse_info->src_dev),
			 netdev_name(fse_info->dest_dev));
		return false;
	}
	/* Based on src_dev / dest_dev is a VAP, get the 5 tuple info
	 * to configure the FSE UL flow. If source is VAP, then
	 * 5 tuple info is in UL direction, so straightaway
	 * add a rule. If dest is VAP, it is 5 tuple info has to be
	 * reversed for adding a rule.
	 */
	if (src_wdev) {
		hw = wiphy_to_ieee80211_hw(src_wdev->wiphy);
		if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION))
			return false;

		vif = wdev_to_ieee80211_vif_vlan(src_wdev, false);
		if (!vif)
			return false;

		ab = fse_cb->fse_get_ab(vif, fse_info->src_mac);
		if (!ab) {
			pr_debug("%s: Failed to add a rule in FST<ab NULL>",
				 netdev_name(fse_info->src_dev));
			return false;
		}
		ret = fse_cb->fse_rule_add(ab,
					   fse_info->src_ip, fse_info->src_port,
					   fse_info->dest_ip, fse_info->dest_port,
					   fse_info->protocol, fse_info->version);
		if (ret) {
			pr_debug("%s: Failed to add a rule in FST",
				 netdev_name(fse_info->src_dev));
			return false;
		}
	}
	if (dest_wdev) {
		hw = wiphy_to_ieee80211_hw(dest_wdev->wiphy);
		if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION))
			return false;

		vif = wdev_to_ieee80211_vif_vlan(dest_wdev, false);
		if (!vif)
			return false;

		ab = fse_cb->fse_get_ab(vif, fse_info->dest_mac);
		if (!ab) {
			pr_debug("%s: Failed to add a rule in FST<ab NULL>",
				 netdev_name(fse_info->dest_dev));
			return false;
		}
		ret = fse_cb->fse_rule_add(ab,
					   fse_info->dest_ip, fse_info->dest_port,
					   fse_info->src_ip, fse_info->src_port,
					   fse_info->protocol, fse_info->version);
		if (ret) {
			/* In case of inter VAP flow, if one direction fails
			 * to configure FSE rule, delete the rule added for
			 * the other direction as well.
			 */
			if (src_wdev) {
				fse_cb->fse_rule_delete(hw,
						fse_info->src_ip, fse_info->src_port,
						fse_info->dest_ip, fse_info->dest_port,
						fse_info->protocol, fse_info->version);
			}
			pr_debug("%s: Failed to add a rule in FST",
				 netdev_name(fse_info->dest_dev));
			return false;
		}
	}
	return true;
}
EXPORT_SYMBOL(ath_fse_add_rule);

bool ath_fse_delete_rule(struct ath_fse_flow_info *fse_info)
{
	struct wireless_dev *src_wdev, *dest_wdev;
	struct ieee80211_hw *hw;
	int fw_ret = 0;
	int rv_ret = 0;

	if (!fse_cb)
		return false;

	if (!fse_info->src_dev || !fse_info->dest_dev) {
		pr_warn("Unable to find dev for FSE rule delete");
		return false;
	}

	src_wdev = fse_info->src_dev->ieee80211_ptr;
	dest_wdev = fse_info->dest_dev->ieee80211_ptr;
	if (!src_wdev && !dest_wdev) {
		pr_debug("Not a wlan traffic for FSE rule delete:sdev:%s ddev:%s\n",
			 netdev_name(fse_info->src_dev),
			 netdev_name(fse_info->dest_dev));
		return false;
	}
	/* Based on src_dev / dest_dev is a VAP, get the 5 tuple info
	 * to delete the FSE UL flow. If source is VAP, then
	 * 5 tuple info is in UL direction, so straightaway
	 * delete a rule. If dest is VAP, it is 5 tuple info has to be
	 * reversed to delete a rule.
	 */
	if (src_wdev) {
		hw = wiphy_to_ieee80211_hw(src_wdev->wiphy);
		if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION))
			return false;

		fw_ret = fse_cb->fse_rule_delete(hw,
						 fse_info->src_ip, fse_info->src_port,
						 fse_info->dest_ip, fse_info->dest_port,
						 fse_info->protocol, fse_info->version);
	}

	if (dest_wdev) {
		hw = wiphy_to_ieee80211_hw(dest_wdev->wiphy);
		if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION))
			return false;

		rv_ret = fse_cb->fse_rule_delete(hw,
						 fse_info->dest_ip, fse_info->dest_port,
						 fse_info->src_ip, fse_info->src_port,
						 fse_info->protocol, fse_info->version);
	}
	if (!fw_ret && !rv_ret)
		return true;

	pr_debug("Failed to delete a rule in FST");
	return false;
}
EXPORT_SYMBOL(ath_fse_delete_rule);
