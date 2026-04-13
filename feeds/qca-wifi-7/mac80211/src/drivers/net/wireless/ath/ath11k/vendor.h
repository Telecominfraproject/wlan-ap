/* SPDX-License-Identifier: ISC */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 */

#ifndef ATH11K_VENDOR_H
#define ATH11K_VENDOR_H

#define QCA_NL80211_VENDOR_ID 0x001374

enum qca_nl80211_vendor_subcmds {
	/* Wi-Fi configuration subcommand */
	QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION = 74,

	/* QCA_NL80211_VENDOR_SUBCMD_BTCOEX_CONFIG: This command is used to
	 * enable/disable BTCOEX and set priority for different type of WLAN
	 * traffic over BT low priority traffic. This uses attributes in
	 * enum qca-vendor_attr_btcoex_config.
	 */
	QCA_NL80211_VENDOR_SUBCMD_BTCOEX_CONFIG = 182,
};

/*
 * enum qca_wlan_priority_type - priority mask
 * This enum defines priority mask that user can configure
 * over BT traffic type which can be passed through
 * QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_WLAN_PRIORITY attribute.
 *
 * @QCA_WLAN_PRIORITY_BE: Bit mask for WLAN Best effort traffic
 * @QCA_WLAN_PRIORITY_BK: Bit mask for WLAN Background traffic
 * @QCA_WLAN_PRIORITY_VI: Bit mask for WLAN Video traffic
 * @QCA_WLAN_PRIORITY_VO: Bit mask for WLAN Voice traffic
 * @QCA_WLAN_PRIORITY_BEACON: Bit mask for WLAN BEACON frame
 * @QCA_WLAN_PRIORITY_MGMT: Bit mask for WLAN Management frame
*/
enum qca_wlan_priority_type {
	QCA_WLAN_PRIORITY_BE = BIT(0),
	QCA_WLAN_PRIORITY_BK = BIT(1),
	QCA_WLAN_PRIORITY_VI = BIT(2),
	QCA_WLAN_PRIORITY_VO = BIT(3),
	QCA_WLAN_PRIORITY_BEACON = BIT(4),
	QCA_WLAN_PRIORITY_MGMT = BIT(5),
};

/**
 * enum qca_wlan_vendor_attr_wlan_prio - Used to configure
 * WLAN priority mask and its respective weight value.
 * @QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_MASK - This is u8 attribute
 * used to pass traffic type mask value see %qca_wlan_priority_type
 * @QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_WEIGHT - This is u8 attribute
 * accepts value between 0 and 255 and used to configure weight for
 * traffic type mentioned in %QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_MASK.
 */
enum qca_wlan_vendor_attr_wlan_prio {
       QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_INVALID = 0,
       QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_MASK = 1,
       QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_WEIGHT = 2,

       QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_LAST,
       QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_MAX =
               QCA_WLAN_VENDOR_ATTR_WLAN_PRIO_LAST - 1,
};

/* Attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION
 */
enum qca_wlan_vendor_attr_config {
	QCA_WLAN_VENDOR_ATTR_CONFIG_GTX = 57,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_btcoex_config - Used by the vendor command
 * The use can enable/disable BTCOEX and configure WLAN priority for
 * different traffic type over BT.
 * QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_ENABLE, enable/disable BTCOEX
 * QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_WLAN_PRIORITY, This is a nested
 * attribute pass the attributes in %qca_wlan_vendor_attr_wlan_prio.
 */
enum qca_wlan_vendor_attr_btcoex_config {
	QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_ENABLE = 1,
	QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_WLAN_PRIORITY = 2,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_LAST,
	QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_BTCOEX_CONFIG_LAST - 1
};

int ath11k_vendor_register(struct ath11k *ar);
#endif /* QCA_VENDOR_H */
