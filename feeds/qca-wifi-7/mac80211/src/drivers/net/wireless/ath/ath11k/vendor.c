// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 */

#include <net/netlink.h>
#include <net/mac80211.h>
#include "core.h"
#include "debug.h"

static const struct nla_policy
ath11k_vendor_set_wifi_config_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GTX] = {.type = NLA_FLAG}
};

static int ath11k_vendor_set_wifi_config(struct wiphy *wihpy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif;
	struct ath11k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	int ret = 0;

	if (!wdev)
		return -EINVAL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	arvif = (struct ath11k_vif*)vif->drv_priv;
	if (!arvif)
		return -EINVAL;

	ar = arvif->ar;

	mutex_lock(&ar->conf_mutex);

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath11k_vendor_set_wifi_config_policy, NULL);
	if (ret) {
		ath11k_warn(ar->ab, "invalid set wifi config policy attribute\n");
		goto exit;
	}

	ar->ap_ps_enabled = nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GTX]);
	ret = ath11k_mac_ap_ps_recalc(ar);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send ap ps ret %d\n", ret);
		goto exit;
	}

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static struct wiphy_vendor_command ath11k_vendor_commands[] = {
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = ath11k_vendor_set_wifi_config,
		.policy = ath11k_vendor_set_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX
	},
};

int ath11k_vendor_register(struct ath11k *ar)
{
	ar->hw->wiphy->vendor_commands = ath11k_vendor_commands;
	ar->hw->wiphy->n_vendor_commands = ARRAY_SIZE(ath11k_vendor_commands);

	return 0;
}
