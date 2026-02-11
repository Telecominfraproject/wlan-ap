// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>
#include <linux/bitfield.h>
#include <linux/inetdevice.h>
#include <linux/of.h>
#include <linux/module.h>
#include <net/if_inet6.h>

#include "mac.h"
#include "core.h"
#include "debug.h"
#include "wmi.h"
#include "hw.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "testmode.h"
#include "peer.h"
#include "debugfs.h"
#include "hif.h"
#include "wow.h"
#include "debugfs_sta.h"
#include "dp.h"
#include "dp_cmn.h"
#include "dp_tx.h"
#include "vendor.h"
#include "telemetry_agent_if.h"
#include "ppe.h"
#include "cfr.h"
#include "dp_mon.h"
#include "erp.h"
#include "vendor_services.h"
#include "ini.h"

#define CHAN2G(_channel, _freq, _flags) { \
	.band                   = NL80211_BAND_2GHZ, \
	.hw_value               = (_channel), \
	.center_freq            = (_freq), \
	.flags                  = (_flags), \
	.max_antenna_gain       = 0, \
	.max_power              = 30, \
}

#define CHAN5G(_channel, _freq, _flags) { \
	.band                   = NL80211_BAND_5GHZ, \
	.hw_value               = (_channel), \
	.center_freq            = (_freq), \
	.flags                  = (_flags), \
	.max_antenna_gain       = 0, \
	.max_power              = 30, \
}

#define CHAN6G(_channel, _freq, _flags) { \
	.band                   = NL80211_BAND_6GHZ, \
	.hw_value               = (_channel), \
	.center_freq            = (_freq), \
	.flags                  = (_flags), \
	.max_antenna_gain       = 0, \
	.max_power              = 30, \
}

static const struct ieee80211_channel ath12k_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

static const struct ieee80211_channel ath12k_5ghz_channels[] = {
	CHAN5G(36, 5180, 0),
	CHAN5G(40, 5200, 0),
	CHAN5G(44, 5220, 0),
	CHAN5G(48, 5240, 0),
	CHAN5G(52, 5260, 0),
	CHAN5G(56, 5280, 0),
	CHAN5G(60, 5300, 0),
	CHAN5G(64, 5320, 0),
	CHAN5G(100, 5500, 0),
	CHAN5G(104, 5520, 0),
	CHAN5G(108, 5540, 0),
	CHAN5G(112, 5560, 0),
	CHAN5G(116, 5580, 0),
	CHAN5G(120, 5600, 0),
	CHAN5G(124, 5620, 0),
	CHAN5G(128, 5640, 0),
	CHAN5G(132, 5660, 0),
	CHAN5G(136, 5680, 0),
	CHAN5G(140, 5700, 0),
	CHAN5G(144, 5720, 0),
	CHAN5G(149, 5745, 0),
	CHAN5G(153, 5765, 0),
	CHAN5G(157, 5785, 0),
	CHAN5G(161, 5805, 0),
	CHAN5G(165, 5825, 0),
	CHAN5G(169, 5845, 0),
	CHAN5G(173, 5865, 0),
	CHAN5G(177, 5885, 0),
};

static const struct ieee80211_channel ath12k_6ghz_channels[] = {
	/* Operating Class 136 */
	CHAN6G(2, 5935, 0),

	/* Operating Classes 131-135 */
	CHAN6G(1, 5955, 0),
	CHAN6G(5, 5975, 0),
	CHAN6G(9, 5995, 0),
	CHAN6G(13, 6015, 0),
	CHAN6G(17, 6035, 0),
	CHAN6G(21, 6055, 0),
	CHAN6G(25, 6075, 0),
	CHAN6G(29, 6095, 0),
	CHAN6G(33, 6115, 0),
	CHAN6G(37, 6135, 0),
	CHAN6G(41, 6155, 0),
	CHAN6G(45, 6175, 0),
	CHAN6G(49, 6195, 0),
	CHAN6G(53, 6215, 0),
	CHAN6G(57, 6235, 0),
	CHAN6G(61, 6255, 0),
	CHAN6G(65, 6275, 0),
	CHAN6G(69, 6295, 0),
	CHAN6G(73, 6315, 0),
	CHAN6G(77, 6335, 0),
	CHAN6G(81, 6355, 0),
	CHAN6G(85, 6375, 0),
	CHAN6G(89, 6395, 0),
	CHAN6G(93, 6415, 0),
	CHAN6G(97, 6435, 0),
	CHAN6G(101, 6455, 0),
	CHAN6G(105, 6475, 0),
	CHAN6G(109, 6495, 0),
	CHAN6G(113, 6515, 0),
	CHAN6G(117, 6535, 0),
	CHAN6G(121, 6555, 0),
	CHAN6G(125, 6575, 0),
	CHAN6G(129, 6595, 0),
	CHAN6G(133, 6615, 0),
	CHAN6G(137, 6635, 0),
	CHAN6G(141, 6655, 0),
	CHAN6G(145, 6675, 0),
	CHAN6G(149, 6695, 0),
	CHAN6G(153, 6715, 0),
	CHAN6G(157, 6735, 0),
	CHAN6G(161, 6755, 0),
	CHAN6G(165, 6775, 0),
	CHAN6G(169, 6795, 0),
	CHAN6G(173, 6815, 0),
	CHAN6G(177, 6835, 0),
	CHAN6G(181, 6855, 0),
	CHAN6G(185, 6875, 0),
	CHAN6G(189, 6895, 0),
	CHAN6G(193, 6915, 0),
	CHAN6G(197, 6935, 0),
	CHAN6G(201, 6955, 0),
	CHAN6G(205, 6975, 0),
	CHAN6G(209, 6995, 0),
	CHAN6G(213, 7015, 0),
	CHAN6G(217, 7035, 0),
	CHAN6G(221, 7055, 0),
	CHAN6G(225, 7075, 0),
	CHAN6G(229, 7095, 0),
	CHAN6G(233, 7115, 0),
};

static struct ieee80211_rate ath12k_legacy_rates[] = {
	{ .bitrate = 10,
	  .hw_value = ATH12K_HW_RATE_CCK_LP_1M },
	{ .bitrate = 20,
	  .hw_value = ATH12K_HW_RATE_CCK_LP_2M,
	  .hw_value_short = ATH12K_HW_RATE_CCK_SP_2M,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 55,
	  .hw_value = ATH12K_HW_RATE_CCK_LP_5_5M,
	  .hw_value_short = ATH12K_HW_RATE_CCK_SP_5_5M,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 110,
	  .hw_value = ATH12K_HW_RATE_CCK_LP_11M,
	  .hw_value_short = ATH12K_HW_RATE_CCK_SP_11M,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },

	{ .bitrate = 60, .hw_value = ATH12K_HW_RATE_OFDM_6M },
	{ .bitrate = 90, .hw_value = ATH12K_HW_RATE_OFDM_9M },
	{ .bitrate = 120, .hw_value = ATH12K_HW_RATE_OFDM_12M },
	{ .bitrate = 180, .hw_value = ATH12K_HW_RATE_OFDM_18M },
	{ .bitrate = 240, .hw_value = ATH12K_HW_RATE_OFDM_24M },
	{ .bitrate = 360, .hw_value = ATH12K_HW_RATE_OFDM_36M },
	{ .bitrate = 480, .hw_value = ATH12K_HW_RATE_OFDM_48M },
	{ .bitrate = 540, .hw_value = ATH12K_HW_RATE_OFDM_54M },
};

static const int
ath12k_phymodes[NUM_NL80211_BANDS][ATH12K_CHAN_WIDTH_NUM] = {
	[NL80211_BAND_2GHZ] = {
			[NL80211_CHAN_WIDTH_5] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_10] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_20_NOHT] = MODE_11BE_EHT20_2G,
			[NL80211_CHAN_WIDTH_20] = MODE_11BE_EHT20_2G,
			[NL80211_CHAN_WIDTH_40] = MODE_11BE_EHT40_2G,
			[NL80211_CHAN_WIDTH_80] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_80P80] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_160] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_320] = MODE_UNKNOWN,
	},
	[NL80211_BAND_5GHZ] = {
			[NL80211_CHAN_WIDTH_5] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_10] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_20_NOHT] = MODE_11BE_EHT20,
			[NL80211_CHAN_WIDTH_20] = MODE_11BE_EHT20,
			[NL80211_CHAN_WIDTH_40] = MODE_11BE_EHT40,
			[NL80211_CHAN_WIDTH_80] = MODE_11BE_EHT80,
			[NL80211_CHAN_WIDTH_160] = MODE_11BE_EHT160,
			[NL80211_CHAN_WIDTH_80P80] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_320] = MODE_11BE_EHT320,
	},
	[NL80211_BAND_6GHZ] = {
			[NL80211_CHAN_WIDTH_5] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_10] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_20_NOHT] = MODE_11BE_EHT20,
			[NL80211_CHAN_WIDTH_20] = MODE_11BE_EHT20,
			[NL80211_CHAN_WIDTH_40] = MODE_11BE_EHT40,
			[NL80211_CHAN_WIDTH_80] = MODE_11BE_EHT80,
			[NL80211_CHAN_WIDTH_160] = MODE_11BE_EHT160,
			[NL80211_CHAN_WIDTH_80P80] = MODE_UNKNOWN,
			[NL80211_CHAN_WIDTH_320] = MODE_11BE_EHT320,
	},

};

#define ATH12K_MAC_FIRST_OFDM_RATE_IDX 4
#define ath12k_g_rates ath12k_legacy_rates
#define ath12k_g_rates_size (ARRAY_SIZE(ath12k_legacy_rates))
#define ath12k_a_rates (ath12k_legacy_rates + 4)
#define ath12k_a_rates_size (ARRAY_SIZE(ath12k_legacy_rates) - 4)

#define ATH12K_MAC_SCAN_TIMEOUT_MSECS 200 /* in msecs */
/* Overhead due to the processing of channel switch events from FW */
#define ATH12K_SCAN_CHANNEL_SWITCH_WMI_EVT_OVERHEAD	10 /* in msecs */
#define ATH12K_MAX_NUM_BRIDGE_PER_MLD 2
#define BRIDGE_IN_RANGE(ar) (ar->num_created_bridge_vdevs < TARGET_NUM_BRIDGE_VDEVS)
#define ATH12K_MAX_AR_LINK_IDX	5
#define ATH12K_SCAN_ROC_CLEANUP_TIMEOUT_MS 3000  /* Timeout for ROC cleanup after scan */
						 /*  vdev clean */

static const u32 ath12k_smps_map[] = {
	[WLAN_HT_CAP_SM_PS_STATIC] = WMI_PEER_SMPS_STATIC,
	[WLAN_HT_CAP_SM_PS_DYNAMIC] = WMI_PEER_SMPS_DYNAMIC,
	[WLAN_HT_CAP_SM_PS_INVALID] = WMI_PEER_SMPS_PS_NONE,
	[WLAN_HT_CAP_SM_PS_DISABLED] = WMI_PEER_SMPS_PS_NONE,
};

static int ath12k_start_vdev_delay(struct ath12k *ar,
				   struct ath12k_link_vif *arvif);
static int ath12k_mac_vdev_delete(struct ath12k *ar, struct ath12k_link_vif *arvif);
static struct ath12k_link_sta *ath12k_mac_alloc_assign_link_sta(struct ath12k_hw *ah,
								struct ath12k_sta *ahsta,
								struct ath12k_vif *ahvif,
								u8 link_id);
static u8 ath12k_mac_ahsta_get_pri_link_id(struct ath12k_vif *ahvif,
					   struct ath12k_sta *ahsta,
					   unsigned long int valid_links);
static void ath12k_wmi_migration_cmd_work(struct work_struct *work);
static void ath12k_mac_vdev_ml_max_rec_links(struct ath12k_link_vif *arvif,
					     u8 ml_max_rec_links);
static void ath12k_set_dscp_tid_work(struct wiphy *wiphy, struct wiphy_work *work);
static bool ath12k_mac_is_bridge_required(u8 device_bitmap, u8 num_devices,
					  u16 *bridge_bitmap);
static const char *ath12k_mac_phymode_str(enum wmi_phy_mode mode)
{
	switch (mode) {
	case MODE_11A:
		return "11a";
	case MODE_11G:
		return "11g";
	case MODE_11B:
		return "11b";
	case MODE_11GONLY:
		return "11gonly";
	case MODE_11NA_HT20:
		return "11na-ht20";
	case MODE_11NG_HT20:
		return "11ng-ht20";
	case MODE_11NA_HT40:
		return "11na-ht40";
	case MODE_11NG_HT40:
		return "11ng-ht40";
	case MODE_11AC_VHT20:
		return "11ac-vht20";
	case MODE_11AC_VHT40:
		return "11ac-vht40";
	case MODE_11AC_VHT80:
		return "11ac-vht80";
	case MODE_11AC_VHT160:
		return "11ac-vht160";
	case MODE_11AC_VHT80_80:
		return "11ac-vht80+80";
	case MODE_11AC_VHT20_2G:
		return "11ac-vht20-2g";
	case MODE_11AC_VHT40_2G:
		return "11ac-vht40-2g";
	case MODE_11AC_VHT80_2G:
		return "11ac-vht80-2g";
	case MODE_11AX_HE20:
		return "11ax-he20";
	case MODE_11AX_HE40:
		return "11ax-he40";
	case MODE_11AX_HE80:
		return "11ax-he80";
	case MODE_11AX_HE80_80:
		return "11ax-he80+80";
	case MODE_11AX_HE160:
		return "11ax-he160";
	case MODE_11AX_HE20_2G:
		return "11ax-he20-2g";
	case MODE_11AX_HE40_2G:
		return "11ax-he40-2g";
	case MODE_11AX_HE80_2G:
		return "11ax-he80-2g";
	case MODE_11BE_EHT20:
		return "11be-eht20";
	case MODE_11BE_EHT40:
		return "11be-eht40";
	case MODE_11BE_EHT80:
		return "11be-eht80";
	case MODE_11BE_EHT80_80:
		return "11be-eht80+80";
	case MODE_11BE_EHT160:
		return "11be-eht160";
	case MODE_11BE_EHT160_160:
		return "11be-eht160+160";
	case MODE_11BE_EHT320:
		return "11be-eht320";
	case MODE_11BE_EHT20_2G:
		return "11be-eht20-2g";
	case MODE_11BE_EHT40_2G:
		return "11be-eht40-2g";
	case MODE_UNKNOWN:
		/* skip */
		break;

		/* no default handler to allow compiler to check that the
		 * enum is fully handled
		 */
	}

	return "<unknown>";
}

u16 ath12k_mac_he_convert_tones_to_ru_tones(u16 tones)
{
	switch (tones) {
	case 26:
		return RU_26;
	case 52:
		return RU_52;
	case 106:
		return RU_106;
	case 242:
		return RU_242;
	case 484:
		return RU_484;
	case 996:
		return RU_996;
	case (996 * 2):
		return RU_2X996;
	default:
		return RU_26;
	}
}
EXPORT_SYMBOL(ath12k_mac_he_convert_tones_to_ru_tones);

static void ath12k_mac_bridge_vdevs_down(struct ieee80211_hw *hw,
					 struct ath12k_vif *ahvif, u8 cur_link_id);
static void ath12k_mac_bridge_vdevs_up(struct ath12k_link_vif *arvif);

enum nl80211_eht_gi ath12k_mac_eht_gi_to_nl80211_eht_gi(u8 sgi)
{
	switch (sgi) {
	case RX_MSDU_START_SGI_0_8_US:
		return NL80211_RATE_INFO_EHT_GI_0_8;
	case RX_MSDU_START_SGI_1_6_US:
		return NL80211_RATE_INFO_EHT_GI_1_6;
	case RX_MSDU_START_SGI_3_2_US:
		return NL80211_RATE_INFO_EHT_GI_3_2;
	default:
		return NL80211_RATE_INFO_EHT_GI_0_8;
	}
}
EXPORT_SYMBOL(ath12k_mac_eht_gi_to_nl80211_eht_gi);

enum nl80211_eht_ru_alloc ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(u16 ru_tones)
{
	switch (ru_tones) {
	case 26:
		return NL80211_RATE_INFO_EHT_RU_ALLOC_26;
	case 52:
		return NL80211_RATE_INFO_EHT_RU_ALLOC_52;
	case (52 + 26):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_52P26;
	case 106:
		return NL80211_RATE_INFO_EHT_RU_ALLOC_106;
	case (106 + 26):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_106P26;
	case 242:
		return NL80211_RATE_INFO_EHT_RU_ALLOC_242;
	case 484:
		return NL80211_RATE_INFO_EHT_RU_ALLOC_484;
	case (484 + 242):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_484P242;
	case 996:
		return NL80211_RATE_INFO_EHT_RU_ALLOC_996;
	case (996 + 484):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_996P484;
	case (996 + 484 + 242):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_996P484P242;
	case (2 * 996):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_2x996;
	case (2 * 996 + 484):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_2x996P484;
	case (3 * 996):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_3x996;
	case (3 * 996 + 484):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_3x996P484;
	case (4 * 996):
		return NL80211_RATE_INFO_EHT_RU_ALLOC_4x996;
	default:
		return NL80211_RATE_INFO_EHT_RU_ALLOC_26;
	}
}
EXPORT_SYMBOL(ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc);

enum rate_info_bw
ath12k_mac_bw_to_mac80211_bw(enum ath12k_supported_bw bw)
{
	u8 ret = RATE_INFO_BW_20;

	switch (bw) {
	case ATH12K_BW_20:
		ret = RATE_INFO_BW_20;
		break;
	case ATH12K_BW_40:
		ret = RATE_INFO_BW_40;
		break;
	case ATH12K_BW_80:
		ret = RATE_INFO_BW_80;
		break;
	case ATH12K_BW_160:
		ret = RATE_INFO_BW_160;
		break;
	case ATH12K_BW_320:
		ret = RATE_INFO_BW_320;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_mac_bw_to_mac80211_bw);

enum ath12k_supported_bw ath12k_mac_mac80211_bw_to_ath12k_bw(enum rate_info_bw bw)
{
	switch (bw) {
	case RATE_INFO_BW_20:
		return ATH12K_BW_20;
	case RATE_INFO_BW_40:
		return ATH12K_BW_40;
	case RATE_INFO_BW_80:
		return ATH12K_BW_80;
	case RATE_INFO_BW_160:
		return ATH12K_BW_160;
	case RATE_INFO_BW_320:
		return ATH12K_BW_320;
	default:
		return ATH12K_BW_20;
	}
}

u8 ath12k_mac_get_bw_offset(enum ieee80211_sta_rx_bandwidth bandwidth)
{
	u8 bw_offset;

	switch (bandwidth) {
	case IEEE80211_STA_RX_BW_20:
		bw_offset = ATH12K_BW_GAIN_20MHZ;
		break;
	case IEEE80211_STA_RX_BW_40:
		bw_offset = ATH12K_BW_GAIN_40MHZ;
		break;
	case IEEE80211_STA_RX_BW_80:
		bw_offset = ATH12K_BW_GAIN_80MHZ;
		break;
	case IEEE80211_STA_RX_BW_160:
		bw_offset = ATH12K_BW_GAIN_160MHZ;
		break;
	case IEEE80211_STA_RX_BW_320:
		bw_offset = ATH12K_BW_GAIN_320MHZ;
		break;
	default:
		bw_offset = ATH12K_BW_GAIN_20MHZ;
	}

	return bw_offset;
}

int ath12k_mac_hw_ratecode_to_legacy_rate(u8 hw_rc, u8 preamble, u8 *rateidx,
					  u16 *rate)
{
	/* As default, it is OFDM rates */
	int i = ATH12K_MAC_FIRST_OFDM_RATE_IDX;
	int max_rates_idx = ath12k_g_rates_size;

	if (preamble == WMI_RATE_PREAMBLE_CCK) {
		hw_rc &= ~ATH12K_HW_RATECODE_CCK_SHORT_PREAM_MASK;
		i = 0;
		max_rates_idx = ATH12K_MAC_FIRST_OFDM_RATE_IDX;
	}

	while (i < max_rates_idx) {
		if (hw_rc == ath12k_legacy_rates[i].hw_value) {
			*rateidx = i;
			*rate = ath12k_legacy_rates[i].bitrate;
			return 0;
		}
		i++;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(ath12k_mac_hw_ratecode_to_legacy_rate);

u8 ath12k_mac_bitrate_to_idx(const struct ieee80211_supported_band *sband,
			     u32 bitrate)
{
	int i;

	for (i = 0; i < sband->n_bitrates; i++)
		if (sband->bitrates[i].bitrate == bitrate)
			return i;

	return 0;
}

static u32
ath12k_mac_max_ht_nss(const u8 *ht_mcs_mask)
{
	int nss;

	for (nss = IEEE80211_HT_MCS_MASK_LEN - 1; nss >= 0; nss--)
		if (ht_mcs_mask[nss])
			return nss + 1;

	return 1;
}

static u32
ath12k_mac_max_vht_nss(const u16 *vht_mcs_mask)
{
	int nss;

	for (nss = NL80211_VHT_NSS_MAX - 1; nss >= 0; nss--)
		if (vht_mcs_mask[nss])
			return nss + 1;

	return 1;
}

static u32
ath12k_mac_max_he_nss(const u16 he_mcs_mask[NL80211_HE_NSS_MAX])
{
	int nss;

	for (nss = NL80211_HE_NSS_MAX - 1; nss >= 0; nss--)
		if (he_mcs_mask[nss])
			return nss + 1;

	return 1;
}

static u32
ath12k_mac_max_eht_nss(const u16 eht_mcs_mask[NL80211_EHT_NSS_MAX])
{
	int nss;

	for (nss = NL80211_EHT_NSS_MAX - 1; nss >= 0; nss--)
		if (eht_mcs_mask[nss])
			return nss + 1;

	return 1;
}

static u32
ath12k_mac_max_eht_mcs_nss(const u8 *eht_mcs, int eht_mcs_set_size)
{
	int i;
	u8 nss = 0;

	for (i = 0; i < eht_mcs_set_size; i++)
		nss = max(nss, u8_get_bits(eht_mcs[i], IEEE80211_EHT_MCS_NSS_RX));

	return nss;
}

static int
ath12k_mac_bitrate_mask_num_eht_rates(struct ath12k *ar,
				      enum nl80211_band band,
				      const struct cfg80211_bitrate_mask *mask)
{
       int num_rates = 0;
       int i;

       for (i = 0; i < ARRAY_SIZE(mask->control[band].eht_mcs); i++)
               num_rates += hweight16(mask->control[band].eht_mcs[i]);

       return num_rates;
}

static u8 ath12k_parse_mpdudensity(u8 mpdudensity)
{
/*  From IEEE Std 802.11-2020 defined values for "Minimum MPDU Start Spacing":
 *   0 for no restriction
 *   1 for 1/4 us
 *   2 for 1/2 us
 *   3 for 1 us
 *   4 for 2 us
 *   5 for 4 us
 *   6 for 8 us
 *   7 for 16 us
 */
	switch (mpdudensity) {
	case 0:
		return 0;
	case 1:
	case 2:
	case 3:
	/* Our lower layer calculations limit our precision to
	 * 1 microsecond
	 */
		return 1;
	case 4:
		return 2;
	case 5:
		return 4;
	case 6:
		return 8;
	case 7:
		return 16;
	default:
		return 0;
	}
}

bool ath12k_mac_is_bridge_vdev(struct ath12k_link_vif *arvif)
{
	if (arvif->vdev_subtype == WMI_VDEV_SUBTYPE_BRIDGE)
		return true;
	return false;
}
EXPORT_SYMBOL(ath12k_mac_is_bridge_vdev);

struct ath12k *ath12k_get_ar_by_link_idx(struct ath12k_hw *ah, u16 link_idx)
{
	struct ath12k *ar;

	ar = ah->radio;
	for (int i = 0; i < ah->num_radio; i++) {
		if (ar->hw_link_id == link_idx) //ToDO: Need to take care that using hw_link_id would serve the purpose
			return ath12k_mac_get_ar_by_pdev_id(ar->ab, ar->pdev->pdev_id);
		ar++;
	}
	return NULL;
}

enum nl80211_band ath12k_get_band_based_on_freq(u32 freq)
{
	enum nl80211_band band;

	if (freq < ATH12K_MIN_5GHZ_FREQ)
		band = NL80211_BAND_2GHZ;
	else if (freq < ATH12K_MIN_6GHZ_FREQ)
		band = NL80211_BAND_5GHZ;
	else
		band = NL80211_BAND_6GHZ;

	return band;
}

int ath12k_mac_vif_link_chan(struct ieee80211_vif *vif, u8 link_id,
				    struct cfg80211_chan_def *def)
{
	struct ieee80211_bss_conf *link_conf;
	struct ieee80211_chanctx_conf *conf;

	rcu_read_lock();
	link_conf = rcu_dereference(vif->link_conf[link_id]);

	if (!link_conf) {
		rcu_read_unlock();
		return -ENOLINK;
	}

	conf = rcu_dereference(link_conf->chanctx_conf);
	if (!conf) {
		rcu_read_unlock();
		return -ENOENT;
	}
	*def = conf->def;
	rcu_read_unlock();

	return 0;
}

static struct ath12k_link_vif *
ath12k_mac_get_tx_arvif(struct ath12k_link_vif *arvif,
			struct ieee80211_bss_conf *link_conf)
{
	struct ieee80211_bss_conf *tx_bss_conf;
	struct ath12k_vif *tx_ahvif;

	lockdep_assert_wiphy(ath12k_ar_to_hw(arvif->ar)->wiphy);

	tx_bss_conf = wiphy_dereference(ath12k_ar_to_hw(arvif->ar)->wiphy,
					link_conf->tx_bss_conf);

	if (tx_bss_conf) {
		tx_ahvif = ath12k_vif_to_ahvif(tx_bss_conf->vif);
		return wiphy_dereference(tx_ahvif->ah->hw->wiphy,
					 tx_ahvif->link[tx_bss_conf->link_id]);
	}

	return NULL;
}

struct ieee80211_bss_conf *
ath12k_mac_get_link_bss_conf(struct ath12k_link_vif *arvif)
{
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct ieee80211_bss_conf *link_conf;
	struct ath12k *ar = arvif->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (arvif->link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return NULL;

	link_conf = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				      vif->link_conf[arvif->link_id]);

	return link_conf;
}

struct ieee80211_link_sta *ath12k_mac_get_link_sta(struct ath12k_link_sta *arsta)
{
	struct ath12k_sta *ahsta = arsta->ahsta;
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	struct ieee80211_link_sta *link_sta;

	lockdep_assert_wiphy(ahsta->ahvif->ah->hw->wiphy);

	if (arsta->link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return NULL;

	link_sta = wiphy_dereference(ahsta->ahvif->ah->hw->wiphy,
				     sta->link[arsta->link_id]);

	return link_sta;
}

static bool ath12k_mac_bitrate_is_cck(int bitrate)
{
	switch (bitrate) {
	case 10:
	case 20:
	case 55:
	case 110:
		return true;
	}

	return false;
}

u8 ath12k_mac_hw_rate_to_idx(const struct ieee80211_supported_band *sband,
			     u8 hw_rate, bool cck)
{
	const struct ieee80211_rate *rate;
	int i;

	for (i = 0; i < sband->n_bitrates; i++) {
		rate = &sband->bitrates[i];

		if (ath12k_mac_bitrate_is_cck(rate->bitrate) != cck)
			continue;

		if (rate->hw_value == hw_rate)
			return i;
		else if (rate->flags & IEEE80211_RATE_SHORT_PREAMBLE &&
			 rate->hw_value_short == hw_rate)
			return i;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_hw_rate_to_idx);

static u8 ath12k_mac_bitrate_to_rate(int bitrate)
{
	return DIV_ROUND_UP(bitrate, 5) |
	       (ath12k_mac_bitrate_is_cck(bitrate) ? BIT(7) : 0);
}

static void ath12k_get_arvif_iter(void *data, u8 *mac,
				  struct ieee80211_vif *vif)
{
	struct ath12k_vif_iter *arvif_iter = data;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	unsigned long links_map = ahvif->links_map;
	struct ath12k_link_vif *arvif;
	u8 link_id;

	for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(ahvif->ah->hw->wiphy, ahvif->link[link_id]);

		if (!arvif) {
			/* Adding this debug to find out how rcu-dereference
			 * is returing NULL when link address is present.
			 * There could be two reasons. Either read lock is
			 * not proper or link_id retrieved by for_each_set_bit
			 * is not right
			 */
			ath12k_err(NULL,
				   "func %s arvif is NULL: link_id %d links_map 0x%lx\n",
				   __func__, link_id, links_map);
			WARN_ON(1);
			continue;
		}

		if (arvif->vdev_id == arvif_iter->vdev_id &&
		    arvif->ar == arvif_iter->ar) {
			arvif_iter->arvif = arvif;
			break;
		}
	}
}

struct ath12k_link_vif *ath12k_mac_get_arvif(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_vif_iter arvif_iter = {};
	u32 flags;

	/* To use the arvif returned, caller must have held rcu read lock.
	 */
	WARN_ON(!rcu_read_lock_held());
	arvif_iter.vdev_id = vdev_id;
	arvif_iter.ar = ar;

	flags = IEEE80211_IFACE_ITER_RESUME_ALL;
	ieee80211_iterate_active_interfaces_atomic(ath12k_ar_to_hw(ar),
						   flags,
						   ath12k_get_arvif_iter,
						   &arvif_iter);
	if (!arvif_iter.arvif) {
		ath12k_warn(ar->ab, "No VIF found for vdev %d\n", vdev_id);
		return NULL;
	}

	return arvif_iter.arvif;
}

static void ath12k_get_active_arvif_chanctx_iter_rcu(void *data, u8 *mac,
						     struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = (void *)vif->drv_priv;
	struct ath12k_vif_chanctx_iter *arvif_iter = data;
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_bss_conf *link_conf;
	u8 link_id;

	for_each_vif_active_link(vif, link_conf, link_id) {
		arvif = rcu_dereference(ahvif->link[link_id]);
		if (arvif && arvif->ar == arvif_iter->ar) {
			if (arvif->link_id != ATH12K_DEFAULT_SCAN_LINK &&
			    arvif->chanctx.def.chan)
				arvif_iter->chanctx = &arvif->chanctx;
			else
				arvif_iter->chanctx = NULL;

			break;
		}
	}
}

struct ieee80211_chanctx_conf *
ath12k_mac_get_first_active_arvif_chanctx(struct ath12k *ar)
{
	struct ath12k_vif_chanctx_iter arvif_iter = {};
	u32 flags;

	arvif_iter.ar = ar;
	arvif_iter.chanctx = NULL;

	flags = IEEE80211_IFACE_ITER_RESUME_ALL;
	ieee80211_iterate_active_interfaces_atomic(
		ar->ah->hw,
		flags,
		ath12k_get_active_arvif_chanctx_iter_rcu,
		&arvif_iter
	);
	return arvif_iter.chanctx;
}

struct ath12k_link_vif *ath12k_mac_get_arvif_by_vdev_id(struct ath12k_base *ab,
							u32 vdev_id)
{
	int i;
	struct ath12k_pdev *pdev;
	struct ath12k_link_vif *arvif;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (pdev && pdev->ar &&
		    (pdev->ar->allocated_vdev_map & (1LL << vdev_id))) {
			arvif = ath12k_mac_get_arvif(pdev->ar, vdev_id);
			if (arvif)
				return arvif;
		}
	}

	return NULL;
}

struct ath12k *ath12k_mac_get_ar_by_vdev_id(struct ath12k_base *ab, u32 vdev_id)
{
	int i;
	struct ath12k_pdev *pdev;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (pdev && pdev->ar) {
			if (pdev->ar->allocated_vdev_map & (1LL << vdev_id))
				return pdev->ar;
		}
	}

	return NULL;
}

struct ath12k *ath12k_mac_get_ar_by_pdev_id(struct ath12k_base *ab, u32 pdev_id)
{
	int i;
	struct ath12k_pdev *pdev;

	if (ab->hw_params->single_pdev_only) {
		pdev = rcu_dereference(ab->pdevs_active[0]);
		return pdev ? pdev->ar : NULL;
	}

	if (WARN_ON(pdev_id > ab->num_radios))
		return NULL;

	for (i = 0; i < ab->num_radios; i++) {
		if (ab->fw_mode == ATH12K_FIRMWARE_MODE_FTM || ab->ag->wsi_remap_in_progress)
			pdev = &ab->pdevs[i];
		else
			pdev = rcu_dereference(ab->pdevs_active[i]);

		if (pdev && pdev->pdev_id == pdev_id)
			return (pdev->ar ? pdev->ar : NULL);
	}

	return NULL;
}

bool ath12k_mac_is_ml_arvif(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;

	lockdep_assert_wiphy(ahvif->ah->hw->wiphy);

	if (ath12k_mac_is_bridge_vdev(arvif))
		return true;

	if (ahvif->vif->valid_links & BIT(arvif->link_id))
		return true;

	return false;
}

static struct ath12k *ath12k_mac_get_ar_by_chan(struct ieee80211_hw *hw,
						struct ieee80211_channel *channel)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	int i;

	ar = ah->radio;

	if (ah->num_radio == 1)
		return ar;

	for_each_ar(ah, ar, i) {
		if (channel->center_freq >= KHZ_TO_MHZ(ar->freq_range.start_freq) &&
		    channel->center_freq <= KHZ_TO_MHZ(ar->freq_range.end_freq))
			return ar;
	}
	return NULL;
}

static struct ath12k *ath12k_mac_get_ar_by_agile_chandef(struct ieee80211_hw *hw,
							 enum nl80211_band band)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	int i;

	if (band != NL80211_BAND_5GHZ)
		return NULL;

	ar = ah->radio;
	for (i = 0; i < ah->num_radio; i++) {
		if (!ar->agile_chandef.chan)
			continue;
		if (ar->agile_chandef.chan->center_freq > ar->chan_info.low_freq &&
		    ar->agile_chandef.chan->center_freq < ar->chan_info.high_freq)
			return ar;
		ar++;
	}
	return NULL;
}

static struct ath12k *ath12k_get_ar_by_ctx(struct ieee80211_hw *hw,
					   struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k *ar;

	if (!ctx)
		return NULL;

	ar = ath12k_mac_get_ar_by_chan(hw, ctx->def.chan);

	if (!ar || ath12k_mac_get_ar_by_pdev_id(ar->ab, ar->pdev->pdev_id))
		return ar;

	return NULL;
}

struct ath12k *ath12k_get_ar_by_vif(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    u8 link_id)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(hw->wiphy);

	/* If there is one pdev within ah, then we return
	 * ar directly.
	 */
	if (ah->num_radio == 1)
		return ah->radio;

	if (!(ahvif->links_map & BIT(link_id)))
		return NULL;

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
	if (arvif && arvif->is_created)
		return arvif->ar;

	return NULL;
}

void ath12k_mac_get_any_chanctx_conf_iter(struct ieee80211_hw *hw,
					  struct ieee80211_chanctx_conf *conf,
					  void *data)
{
	struct ath12k_mac_get_any_chanctx_conf_arg *arg = data;
	struct ath12k *ctx_ar = ath12k_get_ar_by_ctx(hw, conf);

	if (ctx_ar == arg->ar)
		arg->chanctx_conf = conf;
}

static struct ath12k_link_vif *ath12k_mac_get_vif_up(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_up)
			return arvif;
	}

	return NULL;
}

static bool ath12k_mac_band_match(enum nl80211_band band1, enum WMI_HOST_WLAN_BAND band2)
{
	switch (band1) {
	case NL80211_BAND_2GHZ:
		if (band2 & WMI_HOST_WLAN_2GHZ_CAP)
			return true;
		break;
	case NL80211_BAND_5GHZ:
	case NL80211_BAND_6GHZ:
		if (band2 & WMI_HOST_WLAN_5GHZ_CAP)
			return true;
		break;
	default:
		return false;
	}

	return false;
}

static u8 ath12k_mac_get_target_pdev_id_from_vif(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	u8 pdev_id = ab->fw_pdev[0].pdev_id;
	int i;

	if (WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return pdev_id;

	band = def.chan->band;

	for (i = 0; i < ab->fw_pdev_count; i++) {
		if (ath12k_mac_band_match(band, ab->fw_pdev[i].supported_bands))
			return ab->fw_pdev[i].pdev_id;
	}

	return pdev_id;
}

u8 ath12k_mac_get_target_pdev_id(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab = ar->ab;

	if (!ab->hw_params->single_pdev_only)
		return ar->pdev->pdev_id;

	arvif = ath12k_mac_get_vif_up(ar);

	/* fw_pdev array has pdev ids derived from phy capability
	 * service ready event (pdev_and_hw_link_ids).
	 * If no vif is active, return default first index.
	 */
	if (!arvif)
		return ar->ab->fw_pdev[0].pdev_id;

	/* If active vif is found, return the pdev id matching chandef band */
	return ath12k_mac_get_target_pdev_id_from_vif(arvif);
}

static void ath12k_pdev_caps_update(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;

	ar->max_tx_power = ab->target_caps.hw_max_tx_power;

	/* FIXME: Set min_tx_power to ab->target_caps.hw_min_tx_power.
	 * But since the received value in svcrdy is same as hw_max_tx_power,
	 * we can set ar->min_tx_power to 0 currently until
	 * this is fixed in firmware
	 */
	ar->min_tx_power = 0;

	ar->txpower_limit_2g = ar->max_tx_power;
	ar->txpower_limit_5g = ar->max_tx_power;
	ar->txpower_limit_6g = ar->max_tx_power;
	ar->txpower_scale = WMI_HOST_TP_SCALE_MAX;
}

static int ath12k_mac_txpower_recalc(struct ath12k *ar)
{
	struct ath12k_pdev *pdev = ar->pdev;
	struct ath12k_link_vif *arvif;
	int ret, txpower = -1;
	u32 param;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->txpower <= 0)
			continue;

		if (txpower == -1)
			txpower = arvif->txpower;
		else
			txpower = min(txpower, arvif->txpower);
	}

	if (txpower == -1)
		return 0;

	/* txpwr is set as 2 units per dBm in FW*/
	txpower = min_t(u32, max_t(u32, ar->min_tx_power, txpower),
			ar->max_tx_power) * 2;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"txpower to set in hw %d\n", txpower / 2);

	if ((pdev->cap.supported_bands & WMI_HOST_WLAN_2GHZ_CAP) &&
	    ar->txpower_limit_2g != txpower) {
		param = WMI_PDEV_PARAM_TXPOWER_LIMIT2G;
		ret = ath12k_wmi_pdev_set_param(ar, param,
						txpower, ar->pdev->pdev_id);
		if (ret)
			goto fail;
		ar->txpower_limit_2g = txpower;
	}

	if ((pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP) &&
	    ar->txpower_limit_5g != txpower) {
		param = WMI_PDEV_PARAM_TXPOWER_LIMIT5G;
		ret = ath12k_wmi_pdev_set_param(ar, param,
						txpower, ar->pdev->pdev_id);
		if (ret)
			goto fail;
		ar->txpower_limit_5g = txpower;
	}

	if ((ar->ah->hw->wiphy->bands[NL80211_BAND_6GHZ]) &&
		ar->txpower_limit_6g != txpower) {
		param = WMI_PDEV_PARAM_TXPOWER_LIMIT5G;
		ret = ath12k_wmi_pdev_set_param(ar, param,
						txpower, ar->pdev->pdev_id);
		if (ret)
			goto fail;
		ar->txpower_limit_6g = txpower;
	}

	return 0;

fail:
	ath12k_warn(ar->ab, "failed to recalc txpower limit %d using pdev param %d: %d\n",
		    txpower / 2, param, ret);
	return ret;
}

static int ath12k_recalc_rtscts_prot(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	u32 vdev_param, rts_cts;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	vdev_param = WMI_VDEV_PARAM_ENABLE_RTSCTS;

	/* Enable RTS/CTS protection for sw retries (when legacy stations
	 * are in BSS) or by default only for second rate series.
	 * TODO: Check if we need to enable CTS 2 Self in any case
	 */
	rts_cts = WMI_USE_RTS_CTS;

	if (arvif->num_legacy_stations > 0)
		rts_cts |= WMI_RTSCTS_ACROSS_SW_RETRIES << 4;
	else
		rts_cts |= WMI_RTSCTS_FOR_SECOND_RATESERIES << 4;

	/* Need not send duplicate param value to firmware */
	if (arvif->rtscts_prot_mode == rts_cts)
		return 0;

	arvif->rtscts_prot_mode = rts_cts;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"mac vdev %d recalc rts/cts prot %d\n",
			arvif->vdev_id, rts_cts);

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, rts_cts);
	if (ret)
		ath12k_warn(ar->ab, "failed to recalculate rts/cts prot for vdev %d: %d\n",
			    arvif->vdev_id, ret);

	return ret;
}

static int ath12k_mac_set_kickout(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	u32 param;
	int ret;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_STA_KICKOUT_TH,
					ATH12K_KICKOUT_THRESHOLD,
					ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set kickout threshold on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	param = WMI_VDEV_PARAM_AP_KEEPALIVE_MIN_IDLE_INACTIVE_TIME_SECS;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param,
					    ATH12K_KEEPALIVE_MIN_IDLE);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set keepalive minimum idle time on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	param = WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_IDLE_INACTIVE_TIME_SECS;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param,
					    ATH12K_KEEPALIVE_MAX_IDLE);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set keepalive maximum idle time on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	param = WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param,
					    ATH12K_KEEPALIVE_MAX_UNRESPONSIVE);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set keepalive maximum unresponsive time on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	return 0;
}

void ath12k_mac_link_sta_rhash_cleanup(void *data,
				       struct ieee80211_sta *sta)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k *ar = data;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	u8 link_id;
	unsigned long links_map = ahsta->links_map;

	for_each_set_bit(link_id, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
		arsta = ahsta->link[link_id];
		if (!arsta)
			continue;
		arvif = arsta->arvif;
		if (!(arvif->ar == ar))
			continue;

		spin_lock_bh(&ab->base_lock);
		ath12k_link_sta_rhash_delete(ab, arsta);
		spin_unlock_bh(&ab->base_lock);
	}
}

static void ath12k_mac_dec_num_stations(struct ath12k_link_vif *arvif,
					struct ath12k_link_sta *arsta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ath12k *ar = arvif->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ar->num_stations)
		return;

	if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_STA && !sta->tdls)
		return;

	ar->num_stations--;

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	ath12k_ath_update_active_pdev_count(ar);
#endif
}

int ath12k_mac_partner_peer_cleanup(struct ath12k_base *ab)
{
	struct ath12k_base *partner_ab;
	struct ath12k_dp *dp;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	struct ieee80211_sta *sta;
	struct ieee80211_vif *vif;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k *ar;
	struct ath12k_hw *ah = ath12k_ag_to_ah(ag,0);
	struct wiphy *wiphy = ah->hw->wiphy;
	int idx, i, k, ret = 0;
	struct ar_sta_cookie *sta_cookie;
	u8 link_id;

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2)
		return ret;

	wiphy_lock(wiphy);

	for (idx = 0; idx < ag->num_devices; idx++) {
		void *cookie_table[MAX_RADIOS] = {0};
		int cookie_idx[MAX_RADIOS] = {0};

		partner_ab = ag->ab[idx];
		dp = ath12k_ab_to_dp(partner_ab);

		if (partner_ab->is_bypassed || ab == partner_ab)
			continue;

		for (i = 0; i < partner_ab->num_radios; i++) {
			ar = partner_ab->pdevs[i].ar;

			if (!ar->num_peers)
				continue;

			cookie_table[i] =
				kcalloc(ar->num_peers, sizeof(struct ar_sta_cookie),
					GFP_KERNEL);

			if (!cookie_table[i])
				goto free_tables;
		}

		spin_lock_bh(&dp->dp_lock);

		list_for_each_entry_safe(peer, tmp, &partner_ab->dp->peers, list) {
			int ix, pdv_id;

			if (!peer->sta || !peer->mlo || !peer->vif)
				continue;

			link_id = peer->link_id;
			/* get arsta */
			sta = peer->sta;
			ahsta = ath12k_sta_to_ahsta(sta);
			arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);

			/* get arvif */
			vif = peer->vif;
			ahvif = (struct ath12k_vif *)vif->drv_priv;
			/* TODO: re-write this function or check if a data
			 * structure needs to be modified to make a critical
			 * section short.
			 */
			arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);

			ar = arvif->ar;
			if (!ar)
				continue;

			ix = cookie_idx[ar->pdev_idx]++;
			pdv_id = ar->pdev_idx;
			sta_cookie = &((struct ar_sta_cookie *)cookie_table[pdv_id])[ix];
			memcpy(sta_cookie->addr, arsta->addr, ETH_ALEN);
			sta_cookie->vdev_id = arvif->vdev_id;
		}

		spin_unlock_bh(&dp->dp_lock);

		for (i = 0; i < partner_ab->num_radios; i++) {
			int pdv_id;

			ar = partner_ab->pdevs[i].ar;
			if (!ar || !ar->num_peers)
				continue;

			pdv_id = ar->pdev_idx;

			for (k = 0; k < cookie_idx[pdv_id]; k++) {
				int vid;
				u8 *addr;

				sta_cookie =
				&((struct ar_sta_cookie *)cookie_table[pdv_id])[k];

				vid = sta_cookie->vdev_id;
				addr = sta_cookie->addr;

				spin_lock_bh(&dp->dp_lock);

				peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp,
										    vid,
										    addr);
				if (!peer || !peer->sta || !peer->mlo || !peer->vif) {
					spin_unlock_bh(&dp->dp_lock);
					continue;
				}

				link_id = peer->link_id;
				vif = peer->vif;
				ahvif = (struct ath12k_vif *)vif->drv_priv;

				/* get arsta */
				sta = peer->sta;
				ahsta = ath12k_sta_to_ahsta(sta);
				arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);

				/* TODO: re-write this function or check if a data
				 * structure needs to be modified to make a critical
				 * section short.
				 */
				arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);
				ar = arvif->ar;

				spin_unlock_bh(&dp->dp_lock);

				if (!ar)
					continue;

				ret = ath12k_peer_delete(ar, vid, addr, ahsta);
				if (ret) {
					ath12k_err(partner_ab,
						   "failed to delete peer vdev_id %d addr %pM ret %d\n",
						   vid, addr, ret);
					continue;
				}

				spin_lock_bh(&partner_ab->base_lock);
				ath12k_link_sta_rhash_delete(partner_ab, arsta);
				spin_unlock_bh(&partner_ab->base_lock);

				arvif->num_stations--;
				ath12k_mac_dec_num_stations(arvif, arsta);
				wiphy_work_cancel(wiphy, &arsta->update_wk);
			}
		}
free_tables:
		for (i = 0; i < partner_ab->num_radios; i++)
			kfree(cookie_table[i]);
	}

	wiphy_unlock(wiphy);
	return ret;
}

void ath12k_mac_peer_cleanup_all(struct ath12k *ar)
{
	struct list_head peers;
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_link_vif *arvif, *tmp_vif;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	u16 peerid_index;

	INIT_LIST_HEAD(&peers);

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &dp->peers, list) {
		/* Skip Rx TID cleanup for self peer */
		if (peer->sta && peer->dp_peer)
			ath12k_dp_rx_peer_tid_cleanup(ar, peer);

		peer->sta = NULL;

		/* cleanup dp peer */
		spin_lock_bh(&dp_hw->peer_lock);
		if (peer->dp_peer) {
			dp_peer = peer->dp_peer;
			peerid_index = ath12k_dp_peer_get_peerid_index(dp, peer->peer_id);
			if (!dp_peer->is_vdev_peer)
				dp_peer->peer_links_map &= ~BIT(peer->link_id);
			rcu_assign_pointer(dp_peer->link_peers[peer->link_id], NULL);
			rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
		}
		spin_unlock_bh(&dp_hw->peer_lock);

		ath12k_dp_link_peer_rhash_delete(dp, peer);
		peer->dp_peer = NULL;

		list_del(&peer->list);
		list_add(&peer->list, &peers);
	}
	spin_unlock_bh(&dp->dp_lock);

	synchronize_rcu();

	list_for_each_entry_safe(peer, tmp, &peers, list)
		ath12k_link_peer_free(peer);

	ath12k_debugfs_nrp_cleanup_all(ar);

	ar->num_peers = 0;
	ar->num_stations = 0;

	/* Cleanup rhash table maintained for arsta by iterating over sta
	 */
	ieee80211_iterate_stations_atomic(ar->ah->hw,
					  ath12k_mac_link_sta_rhash_cleanup,
					  ar);

	/* The rhash table should be empty after cleanup
	 */
	if (atomic_read(&ab->rhead_sta_addr->nelems)) {
		ath12k_warn(ab,
			    "Destroying rhash table and has stale entries %d\n",
			    atomic_read(&ab->rhead_sta_addr->nelems));
		ath12k_link_sta_rhash_tbl_destroy(ab);
		ath12k_link_sta_rhash_tbl_init(ab);
	}
	/* Delete all the self dp_peers on asserted radio
	 */
	list_for_each_entry_safe_reverse(arvif, tmp_vif, &ar->arvifs, list) {
		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
			ath12k_dp_peer_delete(dp_hw, arvif->bssid, NULL, ar->hw_link_id);
			arvif->num_stations = 0;
		}
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"ath12k mac peer cleanup done\n");
}

void ath12k_mac_dp_peer_cleanup(struct ath12k_hw *ah,
				enum ath12k_mlo_recovery_mode recovery_mode)
{
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_peer *dp_peer, *tmp;
	struct ath12k_sta *ahsta = NULL;
	u16 peerid_index;
	struct list_head peers;

	INIT_LIST_HEAD(&peers);

	spin_lock_bh(&dp_hw->peer_lock);
	list_for_each_entry_safe(dp_peer, tmp, &dp_hw->peers, list) {

		if (!dp_peer->sta || dp_peer->is_vdev_peer) {
			ath12k_generic_dbg(ATH12K_DBG_MAC,
				   	   "Skipping vdev self dp_peer delete on addr %pM\n",
				   	   dp_peer->addr);
			continue;
		}

		/* In case of Mode-1 recovery No need free NoN-ML sta dp_peer
		 */

		if(recovery_mode == ATH12K_MLO_RECOVERY_MODE1 && !dp_peer->is_mlo)
			continue;

		if (dp_peer->is_mlo) {
			ahsta = ath12k_sta_to_ahsta(dp_peer->sta);
			peerid_index = dp_peer->peer_id;
			rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
			clear_bit(ahsta->ml_peer_id, ah->free_ml_peer_id_map);
			ahsta->ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;
			ah->num_ml_peers--;
		}
		list_del(&dp_peer->list);
		list_add(&dp_peer->list, &peers);
	}

	spin_unlock_bh(&dp_hw->peer_lock);

	synchronize_rcu();

	list_for_each_entry_safe(dp_peer, tmp, &peers, list) {
		list_del(&dp_peer->list);

		if (dp_peer->qos && dp_peer->qos->telemetry_peer_ctx)
			ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);

		if (!dp_peer->peer_links_map) {
			kfree(dp_peer->qos);
			kfree(dp_peer);
		} else
			ath12k_err(NULL, "Skipping dp_peer (%pM) due to links_map (%u)",
				   dp_peer->addr, dp_peer->peer_links_map);
	}
}

static int ath12k_mac_vdev_setup_sync(struct ath12k *ar)
{
	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags))
		return -ESHUTDOWN;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"vdev setup timeout %d\n", ATH12K_VDEV_SETUP_TIMEOUT_HZ);

	if (!wait_for_completion_timeout(&ar->vdev_setup_done,
					 ATH12K_VDEV_SETUP_TIMEOUT_HZ)){
		WARN_ON(1);
		return -ETIMEDOUT;
	}

	return ar->last_wmi_vdev_start_status ? -EINVAL : 0;
}

static int ath12k_monitor_vdev_up(struct ath12k *ar, int vdev_id)
{
	struct ath12k_wmi_vdev_up_params params = {};
	int ret;

	params.vdev_id = vdev_id;
	params.bssid = ar->mac_addr;
	ret = ath12k_wmi_vdev_up(ar, &params);
	if (ret) {
		ath12k_warn(ar->ab, "failed to put up monitor vdev %i: %d\n",
			    vdev_id, ret);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac monitor vdev %i started\n",
		   vdev_id);
	return 0;
}

static int ath12k_mac_monitor_vdev_start(struct ath12k *ar, int vdev_id,
					 struct cfg80211_chan_def *chandef)
{
	struct ieee80211_channel *channel;
	struct wmi_vdev_start_req_arg arg = {};
	struct ath12k_wmi_vdev_up_params params = {};
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	channel = chandef->chan;
	arg.vdev_id = vdev_id;
	arg.freq = channel->center_freq;
	arg.band_center_freq1 = chandef->center_freq1;
	arg.band_center_freq2 = chandef->center_freq2;

	if (channel->band >= NUM_NL80211_BANDS ||
	    chandef->width >= ATH12K_CHAN_WIDTH_NUM) {
		ath12k_warn(ar->ab, "Invalid band (%d) or width (%d)\n",
			    channel->band, chandef->width);
		return -EINVAL;
	}

	arg.mode = ath12k_phymodes[chandef->chan->band][chandef->width];
	arg.chan_radar = !!(channel->flags & IEEE80211_CHAN_RADAR);

	arg.min_power = 0;
	arg.max_power = channel->max_power;
	arg.max_reg_power = channel->max_reg_power;
	arg.max_antenna_gain = channel->max_antenna_gain;

	arg.pref_tx_streams = ar->num_tx_chains;
	arg.pref_rx_streams = ar->num_rx_chains;
	arg.punct_bitmap = 0xFFFFFFFF;

	arg.passive |= !!(chandef->chan->flags & IEEE80211_CHAN_NO_IR);

	reinit_completion(&ar->vdev_setup_done);
	reinit_completion(&ar->vdev_delete_done);

	ret = ath12k_wmi_vdev_start(ar, &arg, false);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request monitor vdev %i start: %d\n",
			    vdev_id, ret);
		return ret;
	}

	ret = ath12k_mac_vdev_setup_sync(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to synchronize setup for monitor vdev %i start: %d\n",
			    vdev_id, ret);
		return ret;
	}

	params.vdev_id = vdev_id;
	params.bssid = ar->mac_addr;
	ret = ath12k_wmi_vdev_up(ar, &params);
	if (ret) {
		ath12k_warn(ar->ab, "failed to put up monitor vdev %i: %d\n",
			    vdev_id, ret);
		goto vdev_stop;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
			"mac monitor vdev %i started\n",
			vdev_id);
	return 0;

vdev_stop:
	ret = ath12k_wmi_vdev_stop(ar, vdev_id);
	if (ret)
		ath12k_warn(ar->ab, "failed to stop monitor vdev %i after start failure: %d\n",
			    vdev_id, ret);
	return ret;
}

static int ath12k_mac_monitor_vdev_stop(struct ath12k *ar)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	reinit_completion(&ar->vdev_setup_done);

	ret = ath12k_wmi_vdev_stop(ar, ar->monitor_vdev_id);
	if (ret)
		ath12k_warn(ar->ab, "failed to request monitor vdev %i stop: %d\n",
			    ar->monitor_vdev_id, ret);

	ret = ath12k_mac_vdev_setup_sync(ar);
	if (ret)
		ath12k_warn(ar->ab, "failed to synchronize monitor vdev %i stop: %d\n",
			    ar->monitor_vdev_id, ret);

	ret = ath12k_wmi_vdev_down(ar, ar->monitor_vdev_id);
	if (ret)
		ath12k_warn(ar->ab, "failed to put down monitor vdev %i: %d\n",
			    ar->monitor_vdev_id, ret);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
			"mac monitor vdev %i stopped\n", ar->monitor_vdev_id);
	return ret;
}

static int ath12k_mac_monitor_vdev_delete(struct ath12k *ar)
{
	int ret;
	unsigned long time_left;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ar->monitor_vdev_created)
		return 0;

	reinit_completion(&ar->vdev_delete_done);

	ret = ath12k_wmi_vdev_delete(ar, ar->monitor_vdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "failed to request wmi monitor vdev %i removal: %d\n",
			    ar->monitor_vdev_id, ret);
		return ret;
	}

	time_left = wait_for_completion_timeout(&ar->vdev_delete_done,
						ATH12K_VDEV_DELETE_TIMEOUT_HZ);
	if (time_left == 0) {
		ath12k_warn(ar->ab, "Timeout in receiving vdev delete response\n");
	} else {
		ar->allocated_vdev_map &= ~(1LL << ar->monitor_vdev_id);
		spin_lock_bh(&ar->ab->base_lock);
		ar->ab->free_vdev_map |= 1LL << (ar->monitor_vdev_id);
		spin_unlock_bh(&ar->ab->base_lock);
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac monitor vdev %d deleted\n",
			   ar->monitor_vdev_id);
		WARN_ON(!ar->num_created_vdevs);
		ar->num_created_vdevs--;
		ar->monitor_vdev_id = -1;
		ar->monitor_vdev_created = false;
	}

	return ret;
}

int ath12k_mac_monitor_start(struct ath12k *ar)
{
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->monitor_started)
		return 0;

	arg.ar = ar;
	arg.chanctx_conf = NULL;
	ieee80211_iter_chan_contexts_atomic(ath12k_ar_to_hw(ar),
					    ath12k_mac_get_any_chanctx_conf_iter,
					    &arg);
	if (!arg.chanctx_conf)
		return 0;

	ret = ath12k_mac_monitor_vdev_start(ar, ar->monitor_vdev_id,
					    &arg.chanctx_conf->def);
	if (ret) {
		ath12k_warn(ar->ab, "failed to start monitor vdev: %d\n", ret);
		return ret;
	}

	ath12k_dp_mon_rx_config_monitor_mode(ar, false);
	ret = ath12k_dp_mon_rx_update_filter(ar);
	if (ret) {
		ath12k_warn(ar->ab, "fail to set monitor filter: %d\n", ret);
		return ret;
	}

	ar->monitor_started = true;
	ar->num_started_vdevs++;
	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0, "mac monitor started\n");

	return 0;
}

static int ath12k_mac_monitor_stop(struct ath12k *ar)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ar->monitor_started)
		return 0;

	ret = ath12k_mac_monitor_vdev_stop(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to stop monitor vdev: %d\n", ret);
		return ret;
	}

	ar->monitor_started = false;
	ar->num_started_vdevs--;
	ath12k_dp_mon_rx_config_monitor_mode(ar, true);
	ret = ath12k_dp_mon_rx_update_filter(ar);
	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac monitor stopped ret %d\n", ret);
	return ret;
}

static void ath12k_mac_nrp_delete(struct ath12k *ar)
{
	struct ath12k_set_neighbor_rx_params *param = NULL;
	struct ath12k_neighbor_peer *nrp = NULL, *tmp = NULL;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);
	int ret, nrp_pdev_count = 0, i, overall_status = 0;
	struct list_head nrp_local_list;

	INIT_LIST_HEAD(&nrp_local_list);

	/* First pass: identify and move matching entries to a local list */
	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry_safe(nrp, tmp, &dp->neighbor_peers, list) {
		if (nrp->pdev_id == ar->pdev->pdev_id) {
			dp->num_nrps--;
			list_del(&nrp->list);
			list_add_tail(&nrp->list, &nrp_local_list);
			nrp_pdev_count++;
		}
	}
	spin_unlock_bh(&dp->dp_lock);

	if (nrp_pdev_count == 0) {
		ath12k_err(ar->ab, "NRP pdev_count is 0\n");
		return;
	}

	param = kzalloc(sizeof(*param) * nrp_pdev_count, GFP_KERNEL);
	if (!param) {
		/* Return entries to the original list */
		spin_lock_bh(&dp->dp_lock);
		list_for_each_entry_safe(nrp, tmp, &nrp_local_list, list) {
			list_del(&nrp->list);
			list_add_tail(&nrp->list, &dp->neighbor_peers);
			dp->num_nrps++;
		}
		spin_unlock_bh(&dp->dp_lock);
		ath12k_err(ar->ab,
			   "failed to allocate memory for nrp delete during vdev stop sequence\n");
		return;
	}

	/* Process the local list without holding the lock */
	i = 0;
	list_for_each_entry_safe(nrp, tmp, &nrp_local_list, list) {
		param[i].vdev_id = nrp->vdev_id;
		ether_addr_copy(param[i].nrp_addr, nrp->addr);
		i++;
		list_del(&nrp->list);
		kfree(nrp);
	}

	for (i = 0; i < nrp_pdev_count; i++) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "mac nrp neighbor vdev delete params[%d] vdev id %d  pdev_id: %d nrp %pM\n",
			   i, param[i].vdev_id, ar->pdev->pdev_id, param[i].nrp_addr);
		ath12k_debugfs_nrp_clean(ar, param[i].nrp_addr);
		param[i].action = WMI_FILTER_NRP_ACTION_REMOVE;
		ret = ath12k_wmi_vdev_set_neighbor_rx_cmd(ar, &param[i]);
		if (ret) {
			ath12k_err(ar->ab,
				   "nrp neighbor vdev delete failed params vdev id %d action %d, nrp %pM\n",
				   param[i].vdev_id, param[i].action, param[i].nrp_addr);
			overall_status = ret;
		}
	}

	if (overall_status)
		ath12k_err(ar->ab, "Some neighbor peer deletions failed during vdev stop\n");

	kfree(param);
}

int ath12k_mac_vdev_stop(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k *ar = arvif->ar;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);
	struct ath12k_pdev_dp *dp_pdev = NULL;
	int ret = -1, num_nrps;

	if (!dp) {
		ath12k_err(ar->ab, "ath12k_dp not present%s",__func__);
		goto err;
	}

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	reinit_completion(&ar->vdev_setup_done);

	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, ar->pdev_idx);
	if (!dp_pdev) {
		rcu_read_unlock();
		ath12k_err(ar->ab, "dp_pdev not present%s",__func__);
		goto err;
	}

	memset(&dp_pdev->wmm_stats, 0, sizeof(struct ath12k_wmm_stats));
	memset(&ahvif->wmm_stats, 0, sizeof(struct ath12k_wmm_stats));

	rcu_read_unlock();

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags))
		return 0;

	if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
		spin_lock_bh(&dp->dp_lock);
		num_nrps = dp->num_nrps;
		spin_unlock_bh(&dp->dp_lock);
		if (num_nrps > 0)
			ath12k_mac_nrp_delete(ar);
	}

	ret = ath12k_wmi_vdev_stop(ar, arvif->vdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "failed to stop WMI vdev %i: %d\n",
			    arvif->vdev_id, ret);
		goto err;
	}

	ret = ath12k_mac_vdev_setup_sync(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to synchronize setup for vdev %i: %d\n",
			    arvif->vdev_id, ret);
		goto err;
	}

	WARN_ON(ar->num_started_vdevs == 0);

	ar->num_started_vdevs--;
	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "vdev %pM stopped, vdev_id %d\n",
		   ahvif->vif->addr, arvif->vdev_id);

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) {
		clear_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags);
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
				"CAC Stopped for vdev %d\n",
				arvif->vdev_id);
	}

	return 0;
err:
	return ret;
}

int ath12k_mac_op_config(struct ieee80211_hw *hw, u32 changed)
{
	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_config);

void ath12k_mac_op_sta_set_4addr(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif,
					struct ieee80211_sta *sta, bool enabled)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);

	if (enabled && !ahsta->use_4addr_set) {
		ahsta->ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
		ahsta->vlan_iface = ahvif->vlan_iface;
		wiphy_work_queue(hw->wiphy, &ahsta->set_4addr_wk);
		ahsta->use_4addr_set = true;
	}
}
EXPORT_SYMBOL(ath12k_mac_op_sta_set_4addr);

static int ath12k_mac_setup_bcn_p2p_ie(struct ath12k_link_vif *arvif,
				       struct sk_buff *bcn)
{
	struct ath12k *ar = arvif->ar;
	struct ieee80211_mgmt *mgmt;
	const u8 *p2p_ie;
	int ret;

	mgmt = (void *)bcn->data;
	p2p_ie = cfg80211_find_vendor_ie(WLAN_OUI_WFA, WLAN_OUI_TYPE_WFA_P2P,
					 mgmt->u.beacon.variable,
					 bcn->len - (mgmt->u.beacon.variable -
						     bcn->data));
	if (!p2p_ie) {
		ath12k_warn(ar->ab, "no P2P ie found in beacon\n");
		return -ENOENT;
	}

	ret = ath12k_wmi_p2p_go_bcn_ie(ar, arvif->vdev_id, p2p_ie);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit P2P GO bcn ie for vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	return 0;
}

static int ath12k_mac_remove_vendor_ie(struct sk_buff *skb, unsigned int oui,
				       u8 oui_type, size_t ie_offset)
{
	const u8 *next, *end;
	size_t len;
	u8 *ie;

	if (WARN_ON(skb->len < ie_offset))
		return -EINVAL;

	ie = (u8 *)cfg80211_find_vendor_ie(oui, oui_type,
					   skb->data + ie_offset,
					   skb->len - ie_offset);
	if (!ie)
		return -ENOENT;

	len = ie[1] + 2;
	end = skb->data + skb->len;
	next = ie + len;

	if (WARN_ON(next > end))
		return -EINVAL;

	memmove(ie, next, end - next);
	skb_trim(skb, skb->len - len);

	return 0;
}

static void ath12k_mac_set_arvif_ies(struct ath12k_link_vif *arvif, struct sk_buff *bcn,
				     u8 bssid_index, bool *nontx_profile_found)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)bcn->data;
	const struct element *elem, *nontx, *index, *nie;
	struct ieee80211_vht_cap *vht_cap;
	const u8 *start, *tail;
	const u8 *vht_cap_ie;
	u16 rem_len;
	u8 i;

	start = bcn->data + ieee80211_get_hdrlen_from_skb(bcn) + sizeof(mgmt->u.beacon);
	tail = skb_tail_pointer(bcn);
	rem_len = tail - start;

	arvif->rsnie_present = false;
	arvif->wpaie_present = false;
	arvif->beacon_prot = false;

	/* Make the TSF offset negative so beacons in the same
	 * staggered batch have the same TSF.
	 */
	if (arvif->tbtt_offset) {
		u64 adjusted_tsf = cpu_to_le64(0ULL - arvif->tbtt_offset);

		memcpy(&mgmt->u.beacon.timestamp, &adjusted_tsf, sizeof(adjusted_tsf));
	}

	elem = cfg80211_find_elem(WLAN_EID_EXT_CAPABILITY, start, (skb_tail_pointer(bcn) - start));
	if (elem && elem->datalen >= 11 &&
			(elem->data[10] & WLAN_EXT_CAPA11_BCN_PROTECT))
		arvif->beacon_prot = true;

	if (cfg80211_find_ie(WLAN_EID_RSN, start, rem_len))
		arvif->rsnie_present = true;
	if (cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPA,
				    start, rem_len))
		arvif->wpaie_present = true;
	vht_cap_ie = cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY, start, rem_len);
	if (vht_cap_ie && vht_cap_ie[1] >= sizeof(*vht_cap)) {
		vht_cap = (void *)(vht_cap_ie + 2);
		arvif->vht_cap = vht_cap->vht_cap_info;
	}

	/* Return from here for the transmitted profile */
	if (!bssid_index)
		return;

	/* Initial rsnie_present for the nontransmitted profile is set to be same as that
	 * of the transmitted profile. It will be changed if security configurations are
	 * different.
	 */
	*nontx_profile_found = false;
	for_each_element_id(elem, WLAN_EID_MULTIPLE_BSSID, start, rem_len) {
		/* Fixed minimum MBSSID element length with at least one
		 * nontransmitted BSSID profile is 12 bytes as given below;
		 * 1 (max BSSID indicator) +
		 * 2 (Nontransmitted BSSID profile: Subelement ID + length) +
		 * 4 (Nontransmitted BSSID Capabilities: tag + length + info)
		 * 2 (Nontransmitted BSSID SSID: tag + length)
		 * 3 (Nontransmitted BSSID Index: tag + length + BSSID index
		 */
		if (elem->datalen < 12 || elem->data[0] < 1)
			continue; /* Max BSSID indicator must be >=1 */

		for_each_element(nontx, elem->data + 1, elem->datalen - 1) {
			start = nontx->data;

			if (nontx->id != 0 || nontx->datalen < 4)
				continue; /* Invalid nontransmitted profile */

			if (nontx->data[0] != WLAN_EID_NON_TX_BSSID_CAP ||
			    nontx->data[1] != 2) {
				continue; /* Missing nontransmitted BSS capabilities */
			}

			if (nontx->data[4] != WLAN_EID_SSID)
				continue; /* Missing SSID for nontransmitted BSS */

			index = cfg80211_find_elem(WLAN_EID_MULTI_BSSID_IDX,
						   start, nontx->datalen);
			if (!index || index->datalen < 1 || index->data[0] == 0)
				continue; /* Invalid MBSSID Index element */

			if (index->data[0] == bssid_index) {
				*nontx_profile_found = true;
				if (cfg80211_find_ie(WLAN_EID_RSN,
						     nontx->data,
						     nontx->datalen)) {
					arvif->rsnie_present = true;
					return;
				} else if (!arvif->rsnie_present) {
					return; /* Both tx and nontx BSS are open */
				}

				nie = cfg80211_find_ext_elem(WLAN_EID_EXT_NON_INHERITANCE,
							     nontx->data,
							     nontx->datalen);
				if (!nie || nie->datalen < 2)
					return; /* Invalid non-inheritance element */

				for (i = 1; i < nie->datalen - 1; i++) {
					if (nie->data[i] == WLAN_EID_RSN) {
						arvif->rsnie_present = false;
						break;
					}
				}

				return;
			}
		}
	}
}

static void ath12k_wmi_migration_cmd_work(struct work_struct *work)
{
	struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif,
						     wmi_migration_cmd_work);
	struct ath12k *ar = arvif->ar;
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_mac_pri_link_migr_peer_node *peer_node, *tmp_peer;
	struct ath12k_dp_peer *ml_peer;
	struct ath12k_sta *ahsta;

	if (wait_for_completion_timeout(&arvif->wmi_migration_event_resp,
					ATH12K_MIGRATION_TIMEOUT_HZ))
		return;

	spin_lock_bh(&ah->dp_hw.peer_lock);

	arvif->is_umac_migration_in_progress = false;

	list_for_each_entry_safe(peer_node, tmp_peer, &arvif->peer_migrate_list, list) {
		rcu_read_lock();
		/* TODO: Need to check if we ml_peer_id validation
		 */
		ml_peer = rcu_dereference(ah->dp_hw.dp_peer_list[peer_node->ml_peer_id]);

		if (ml_peer) {
			ahsta = ath12k_sta_to_ahsta(ml_peer->sta);
			ahsta->is_migration_in_progress = false;
		}

		rcu_read_unlock();
		list_del(&peer_node->list);
		kfree(peer_node);
	}
	spin_unlock_bh(&ah->dp_hw.peer_lock);
}

static int ath12k_mac_setup_bcn_tmpl_ema(struct ath12k_link_vif *arvif,
					 struct ath12k_link_vif *tx_arvif,
					 u8 bssid_index)
{
	struct ath12k_wmi_bcn_tmpl_ema_arg ema_args;
	struct ieee80211_ema_beacons *beacons;
	bool nontx_profile_found = false;
	int ret = 0;
	u8 i;

	beacons = ieee80211_beacon_get_template_ema_list(ath12k_ar_to_hw(tx_arvif->ar),
							 tx_arvif->ahvif->vif,
							 tx_arvif->link_id);
	if (!beacons || !beacons->cnt) {
		ath12k_warn(arvif->ar->ab,
			    "failed to get ema beacon templates from mac80211\n");
		return -EPERM;
	}

	if (tx_arvif == arvif)
		ath12k_mac_set_arvif_ies(arvif, beacons->bcn[0].skb, 0, NULL);

	for (i = 0; i < beacons->cnt; i++) {
		if (tx_arvif != arvif && !nontx_profile_found) {
			ath12k_mac_set_arvif_ies(arvif, beacons->bcn[i].skb,
						 bssid_index,
						 &nontx_profile_found);
			if (arvif->beacon_prot)
				tx_arvif->beacon_prot = arvif->beacon_prot;
		}

		ema_args.bcn_cnt = beacons->cnt;
		ema_args.bcn_index = i;
		ret = ath12k_wmi_bcn_tmpl(tx_arvif, &beacons->bcn[i].offs,
					  beacons->bcn[i].skb, &ema_args);
		if (ret) {
			ath12k_warn(tx_arvif->ar->ab,
				    "failed to set ema beacon template id %i error %d\n",
				    i, ret);
			break;
		}
	}

	if (tx_arvif != arvif && !nontx_profile_found)
		ath12k_warn(arvif->ar->ab,
			    "nontransmitted bssid index %u not found in beacon template\n",
			    bssid_index);

	ieee80211_beacon_free_ema_list(beacons);
	return ret;
}

static int ath12k_mac_setup_bcn_tmpl(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ieee80211_bss_conf *link_conf;
	struct ath12k_link_vif *tx_arvif;
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_mutable_offsets offs = {};
	bool nontx_profile_found = false;
	struct sk_buff *bcn;
	int ret;

	if (ahvif->vdev_type != WMI_VDEV_TYPE_AP ||
	    ath12k_mac_is_bridge_vdev(arvif))
		return 0;

	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!link_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf to set bcn tmpl for vif %pM link %u\n",
			    vif->addr, arvif->link_id);
		return -ENOLINK;
	}

	tx_arvif = ath12k_mac_get_tx_arvif(arvif, link_conf);
	if (tx_arvif) {
		if (tx_arvif != arvif) {
			if (!tx_arvif->is_started) {
				ath12k_warn(ab,
					"Transmit vif is not started before this non Tx beacon setup for vdev %d\n",
					arvif->vdev_id);
				return -EINVAL;
			}
			if (arvif->is_up)
				return 0;
		}

		if (link_conf->ema_ap)
			return ath12k_mac_setup_bcn_tmpl_ema(arvif, tx_arvif,
							     link_conf->bssid_index);
	} else {
		/* Avoid setting beacon for non-tx vdevs if corresponding
		 * tx vdev is unmapped.
		 */
		if (link_conf->nontransmitted)
			return 0;
		tx_arvif = arvif;
	}

	bcn = ieee80211_beacon_get_template(ath12k_ar_to_hw(tx_arvif->ar),
					    tx_arvif->ahvif->vif,
					    &offs, tx_arvif->link_id);
	if (!bcn) {
		ath12k_warn(ab, "failed to get beacon template from mac80211\n");
		return -EPERM;
	}

	if (tx_arvif == arvif) {
		ath12k_mac_set_arvif_ies(arvif, bcn, 0, NULL);
	} else {
		ath12k_mac_set_arvif_ies(arvif, bcn,
					 link_conf->bssid_index,
					 &nontx_profile_found);
		if (!nontx_profile_found)
			ath12k_warn(ab,
				    "nontransmitted profile not found in beacon template\n");
	}

	if (ahvif->vif->type == NL80211_IFTYPE_AP && ahvif->vif->p2p) {
		ret = ath12k_mac_setup_bcn_p2p_ie(arvif, bcn);
		if (ret) {
			ath12k_warn(ab, "failed to setup P2P GO bcn ie: %d\n",
				    ret);
			goto free_bcn_skb;
		}

		/* P2P IE is inserted by firmware automatically (as
		 * configured above) so remove it from the base beacon
		 * template to avoid duplicate P2P IEs in beacon frames.
		 */
		ret = ath12k_mac_remove_vendor_ie(bcn, WLAN_OUI_WFA,
						  WLAN_OUI_TYPE_WFA_P2P,
						  offsetof(struct ieee80211_mgmt,
							   u.beacon.variable));
		if (ret) {
			ath12k_warn(ab, "failed to remove P2P vendor ie: %d\n",
				    ret);
			goto free_bcn_skb;
		}
	}

	ret = ath12k_wmi_bcn_tmpl(tx_arvif, &offs, bcn, NULL);

	if (ret)
		ath12k_warn(ab, "failed to submit beacon template command: %d\n",
			    ret);

free_bcn_skb:
	kfree_skb(bcn);
	return ret;
}

static void ath12k_update_bcn_tx_status_work(struct wiphy *wiphy,
					     struct wiphy_work *work)
{
	struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif,
						     update_bcn_tx_status_work);

	lockdep_assert_wiphy(wiphy);
	ath12k_mac_bcn_tx_event(arvif);
}

static void ath12k_update_bcn_template_work(struct wiphy *wiphy,
					    struct wiphy_work *work)
{
        struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif,
                                        update_bcn_template_work);
        struct ath12k *ar = arvif->ar;
        int ret = -EINVAL;

	lockdep_assert_wiphy(wiphy);

        if (!ar)
                return;

	if (arvif->is_created && arvif->is_started)
		ret = ath12k_mac_setup_bcn_tmpl(arvif);
	if (ret)
		ath12k_warn(ar->ab,
			    "failed to update bcn tmpl for vdev_id: %d ret: %d\n",
			    arvif->vdev_id, ret);
}

void ath12k_mac_bcn_tx_event(struct ath12k_link_vif *arvif)
{
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct ath12k *ar = arvif->ar;
	struct ieee80211_bss_conf* link_conf;

	link_conf = ath12k_mac_get_link_bss_conf(arvif);

	if (!link_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf in bcn tx event\n");
		return;
	}

	if (!link_conf->color_change_active && !arvif->bcca_zero_sent)
		return;

	if (link_conf->color_change_active &&
	    ieee80211_beacon_cntdwn_is_complete(vif, arvif->link_id)) {
		arvif->bcca_zero_sent = true;
		ieee80211_color_change_finish(vif, arvif->link_id);
		return;
	}

	arvif->bcca_zero_sent = false;

	if (link_conf->color_change_active && !link_conf->ema_ap)
		ieee80211_beacon_update_cntdwn(vif, arvif->link_id);
	wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy,
			 &arvif->update_bcn_template_work);
}

static void ath12k_control_beaconing(struct ath12k_link_vif *arvif,
				     struct ieee80211_bss_conf *info)
{
	struct ath12k_wmi_vdev_up_params params = {};
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_link_vif *tx_arvif;
	struct ath12k *ar = arvif->ar;
	struct ieee80211_bss_conf *link_conf;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(arvif->ar)->wiphy);

	if (!info->enable_beacon) {
		ret = ath12k_wmi_vdev_down(ar, arvif->vdev_id);
		if (ret)
			ath12k_warn(ar->ab, "failed to down vdev_id %i: %d\n",
				    arvif->vdev_id, ret);

		arvif->is_up = false;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "vdev %d down with link_id=%u\n",
			   arvif->vdev_id, arvif->link_id);
		ath12k_mac_bridge_vdevs_down(ath12k_ar_to_hw(arvif->ar),
					     ahvif, arvif->link_id);
		return;
	}

	/* Install the beacon template to the FW */
	ret = ath12k_mac_setup_bcn_tmpl(arvif);
	if (ret) {
		ath12k_warn(ar->ab, "failed to update bcn tmpl during vdev up: %d\n",
			    ret);
		return;
	}

	ahvif->aid = 0;

	ether_addr_copy(arvif->bssid, info->addr);

	params.vdev_id = arvif->vdev_id;
	params.aid = ahvif->aid;
	params.bssid = arvif->bssid;

	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!link_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf to set vdev up params for vif %pM link %u\n",
			    ahvif->vif->addr, arvif->link_id);
		return;
	}

	tx_arvif = ath12k_mac_get_tx_arvif(arvif, link_conf);
	if (tx_arvif) {
		params.tx_bssid = tx_arvif->bssid;
		params.nontx_profile_idx = info->bssid_index;
		params.nontx_profile_cnt = 1 << info->bssid_indicator;
	}
	ret = ath12k_wmi_vdev_up(arvif->ar, &params);
	if (ret) {
		ath12k_warn(ar->ab, "failed to bring up vdev %d: %i\n",
			    arvif->vdev_id, ret);
		return;
	}

	arvif->is_up = true;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac vdev %d up\n", arvif->vdev_id);
	ath12k_mac_bridge_vdevs_up(arvif);
}

static void ath12k_mac_handle_beacon_iter(void *data, u8 *mac,
					  struct ieee80211_vif *vif)
{
	struct sk_buff *skb = data;
	struct ieee80211_mgmt *mgmt = (void *)skb->data;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);

	if (vif->type != NL80211_IFTYPE_STATION)
		return;

	if (!ether_addr_equal(mgmt->bssid, vif->bss_conf.bssid))
		return;

	cancel_delayed_work(&ahvif->deflink.connection_loss_work);
}

void ath12k_mac_handle_beacon(struct ath12k *ar, struct sk_buff *skb)
{
	ieee80211_iterate_active_interfaces_atomic(ath12k_ar_to_hw(ar),
						   IEEE80211_IFACE_ITER_NORMAL,
						   ath12k_mac_handle_beacon_iter,
						   skb);
}

static void ath12k_mac_handle_beacon_miss_iter(void *data, u8 *mac,
					       struct ieee80211_vif *vif)
{
	u32 *vdev_id = data;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k *ar = ahvif->deflink.ar;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);

	if (ahvif->deflink.vdev_id != *vdev_id)
		return;

	if (!ahvif->deflink.is_up)
		return;

	ieee80211_beacon_loss(vif);

	/* Firmware doesn't report beacon loss events repeatedly. If AP probe
	 * (done by mac80211) succeeds but beacons do not resume then it
	 * doesn't make sense to continue operation. Queue connection loss work
	 * which can be cancelled when beacon is received.
	 */
	ieee80211_queue_delayed_work(hw, &ahvif->deflink.connection_loss_work,
				     ATH12K_CONNECTION_LOSS_HZ);
}

void ath12k_mac_handle_beacon_miss(struct ath12k *ar, u32 vdev_id)
{
	ieee80211_iterate_active_interfaces_atomic(ath12k_ar_to_hw(ar),
						   IEEE80211_IFACE_ITER_NORMAL,
						   ath12k_mac_handle_beacon_miss_iter,
						   &vdev_id);
}

static void ath12k_mac_vif_sta_connection_loss_work(struct work_struct *work)
{
	struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif,
						     connection_loss_work.work);
	struct ieee80211_vif *vif = arvif->ahvif->vif;

	if (!arvif->is_up)
		return;

	ieee80211_connection_loss(vif);
}

static void ath12k_mac_get_hw_link_map(struct ieee80211_vif *vif,
				       u16 ieee_link_map,
				       u16 *hw_link_map)
{
	u8 i, j;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar = NULL;

	*hw_link_map = 0;

	for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++) {
		if (!(vif->valid_links & BIT(i)))
			continue;
		if (!(ieee_link_map & BIT(i)))
			continue;
		for (j = 0; j < ATH12K_NUM_MAX_LINKS; j++) {
			arvif = rcu_dereference(ahvif->link[j]);
			if (!arvif)
				continue;
			if (arvif->link_id != i)
				continue;
			ar = arvif->ar;
			break;
		}
		if (ar)
			*hw_link_map |= BIT(ar->hw_link_id);
	}
}

static void ath12k_populate_default_mapping_flags(struct ieee80211_vif *vif,
						  struct ieee80211_neg_ttlm *neg_ttlm,
						  u8 *is_default_mapping)
{
	u8 i;
	u16 map = 0;

	/* populate def mapping for uplink map values */
	for (i = 0; i < IEEE80211_TTLM_NUM_TIDS; i++)
		map |= neg_ttlm->uplink[i];

	if (!map)
		is_default_mapping[IEEE80211_TTLM_DIRECTION_UP] = 1;
	else if ((map & vif->valid_links) == vif->valid_links)
		is_default_mapping[IEEE80211_TTLM_DIRECTION_UP] = 1;

	map = 0;
	for (i = 0; i < IEEE80211_TTLM_NUM_TIDS; i++)
		map |= neg_ttlm->downlink[i];

	if (!map)
		is_default_mapping[IEEE80211_TTLM_DIRECTION_DOWN] = 1;
	else if ((map & vif->valid_links) == vif->valid_links)
		is_default_mapping[IEEE80211_TTLM_DIRECTION_DOWN] = 1;
}

static void ath12k_populate_wmi_ttlm_peer_params(struct ath12k_link_sta *arsta,
						 struct ath12k_wmi_ttlm_peer_params *params,
						 u8 *is_default_mapping,
						 struct ieee80211_neg_ttlm *neg_ttlm)
{
	struct ath12k *ar = arsta->arvif->ar;
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	enum ath12k_wmi_ttlm_direction dir = ATH12K_WMI_TTLM_INVALID_DIRECTION;
	u8 j, k;
	u16 ieee_link_map;
	u16 hw_link_map_tid[IEEE80211_MAX_NUM_TIDS];

	if (!memcmp(neg_ttlm->downlink, neg_ttlm->uplink, sizeof(neg_ttlm->downlink)))
		dir = ATH12K_WMI_TTLM_BIDI_DIRECTION;

	if (is_default_mapping[IEEE80211_TTLM_DIRECTION_DOWN] &&
	    is_default_mapping[IEEE80211_TTLM_DIRECTION_UP])
		dir = ATH12K_WMI_TTLM_BIDI_DIRECTION;

	memset(params, 0, sizeof(struct ath12k_wmi_ttlm_peer_params));
	params->pdev_id = ath12k_mac_get_target_pdev_id(ar);
	ether_addr_copy(params->peer_macaddr, arsta->addr);
	if (dir != ATH12K_WMI_TTLM_BIDI_DIRECTION) {
		for (j = 0; j < ATH12K_WMI_TTLM_BIDI_DIRECTION; j++) {
			params->ttlm_info[params->num_dir].direction = j;
			params->ttlm_info[params->num_dir].default_link_mapping =
				is_default_mapping[j];
			if (is_default_mapping[j]) {
				params->num_dir++;
				continue;
			}
			for (k = 0; k < TTLM_MAX_NUM_TIDS; k++) {
				if (j == ATH12K_WMI_TTLM_DL_DIRECTION)
					ieee_link_map = neg_ttlm->downlink[k];
				else
					ieee_link_map = neg_ttlm->uplink[k];
				ath12k_mac_get_hw_link_map(ahsta->ahvif->vif,
							   ieee_link_map,
							   &hw_link_map_tid[k]);
				params->ttlm_info[params->num_dir].ttlm_provisioned_links[k] =
					hw_link_map_tid[k];
			}
			params->num_dir++;
		}
	} else {
		params->ttlm_info[params->num_dir].direction =
			ATH12K_WMI_TTLM_BIDI_DIRECTION;
		params->ttlm_info[params->num_dir].default_link_mapping =
			is_default_mapping[ATH12K_WMI_TTLM_DL_DIRECTION];
		if (!is_default_mapping[ATH12K_WMI_TTLM_DL_DIRECTION]) {
			for (k = 0; k < TTLM_MAX_NUM_TIDS; k++) {
				ieee_link_map = neg_ttlm->downlink[k];
				ath12k_mac_get_hw_link_map(ahsta->ahvif->vif,
							   ieee_link_map,
							   &hw_link_map_tid[k]);
			}
			memcpy(&params->ttlm_info[params->num_dir].ttlm_provisioned_links,
			       hw_link_map_tid, sizeof(u16) * TTLM_MAX_NUM_TIDS);
		}
		params->num_dir++;
	}
}

static void ath12k_peer_assoc_h_basic(struct ath12k *ar,
				      struct ath12k_link_vif *arvif,
				      struct ath12k_link_sta *arsta,
				      struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	struct ieee80211_bss_conf *bss_conf;
	u32 aid;

	lockdep_assert_wiphy(hw->wiphy);

	if (vif->type == NL80211_IFTYPE_STATION)
		aid = vif->cfg.aid;
	else
		aid = sta->aid;

	ether_addr_copy(arg->peer_mac, arsta->addr);
	arg->vdev_id = arvif->vdev_id;
	arg->peer_associd = aid;
	arg->auth_flag = true;
	/* TODO: STA WAR in ath10k for listen interval required? */
	arg->peer_listen_intval = hw->conf.listen_interval;
	arg->peer_nss = 1;

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf in peer assoc for vif %pM link %u\n",
			    vif->addr, arvif->link_id);
		return;
	}

	arg->peer_caps = bss_conf->assoc_capability;
}

static void ath12k_peer_assoc_h_crypto(struct ath12k *ar,
				       struct ath12k_link_vif *arvif,
				       struct ath12k_link_sta *arsta,
				       struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ieee80211_bss_conf *info;
	struct cfg80211_chan_def def;
	struct cfg80211_bss *bss;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	const u8 *rsnie = NULL;
	const u8 *wpaie = NULL;

	lockdep_assert_wiphy(hw->wiphy);

	info = ath12k_mac_get_link_bss_conf(arvif);
	if (!ath12k_mac_is_bridge_vdev(arvif) && !info) {
		ath12k_warn(ar->ab, "unable to access bss link conf for peer assoc crypto for vif %pM link %u\n",
			    vif->addr, arvif->link_id);
		return;
	}

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		bss = NULL;
	else
		bss = cfg80211_get_bss(hw->wiphy, def.chan, info->bssid, NULL, 0,
				       IEEE80211_BSS_TYPE_ANY, IEEE80211_PRIVACY_ANY);

	if (arvif->rsnie_present || arvif->wpaie_present) {
		if (sta->ft_auth)
			arg->need_ptk_4_way = false;
		else
			arg->need_ptk_4_way = true;

		if (arvif->wpaie_present)
			arg->need_gtk_2_way = true;
	} else if (bss) {
		const struct cfg80211_bss_ies *ies;

		rcu_read_lock();
		rsnie = ieee80211_bss_get_ie(bss, WLAN_EID_RSN);

		ies = rcu_dereference(bss->ies);

		wpaie = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT,
						WLAN_OUI_TYPE_MICROSOFT_WPA,
						ies->data,
						ies->len);
		rcu_read_unlock();
		cfg80211_put_bss(hw->wiphy, bss);
	}

	/* FIXME: base on RSN IE/WPA IE is a correct idea? */
	/* Bridge peer will be created only on ML association and only secured
	 * association is allowed. For secured association, Firmware expects
	 * WMI_PEER_NEED_PTK_4_WAY flag to set on peer_flags, hence Allow
	 * setting ptk_4_way for bridge peer.
	 */
	if (ar->supports_6ghz || rsnie || wpaie || arsta->is_bridge_peer) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "%s: rsn ie found\n", __func__);
		if (sta->ft_auth)
			arg->need_ptk_4_way = false;
		else
			arg->need_ptk_4_way = true;
	}

	if (wpaie) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "%s: wpa ie found\n", __func__);
		arg->need_gtk_2_way = true;
	}

	if (sta->mfp) {
		/* TODO: Need to check if FW supports PMF? */
		arg->is_pmf_enabled = true;
	}

	/* TODO: safe_mode_enabled (bypass 4-way handshake) flag req? */
}

static enum ieee80211_sta_rx_bandwidth
ath12k_get_radio_max_bw_caps(struct ath12k *ar,
			     enum nl80211_band band,
			     enum ieee80211_sta_rx_bandwidth sta_bw,
			     enum nl80211_iftype iftype)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_sband_iftype_data *iftype_data;
	const struct ieee80211_sta_eht_cap *eht_cap;
	const struct ieee80211_sta_he_cap *he_cap;
	int i, idx = 0;

	sband = &ar->mac.sbands[band];
	iftype_data = ar->mac.iftype[band];

	if (!sband || !iftype_data) {
		WARN_ONCE(1, "Invalid band specified :%d\n", band);
		return sta_bw;
	}

	for (i = 0; i < NUM_NL80211_IFTYPES && i != iftype; i++) {
		switch(i) {
		case NL80211_IFTYPE_STATION:
		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_MESH_POINT:
			idx++;
			break;
		default:
			break;
		}
	}

	eht_cap = &iftype_data[idx].eht_cap;
	he_cap = &iftype_data[idx].he_cap;

	if (!eht_cap || !he_cap)
		return sta_bw;

	/* EHT Caps */
	if (band != NL80211_BAND_2GHZ && eht_cap->has_eht &&
	    (eht_cap->eht_cap_elem.phy_cap_info[0] &
	     IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ))
		return IEEE80211_STA_RX_BW_320;

	/* HE Caps */
	switch (band) {
	case NL80211_BAND_5GHZ:
	case NL80211_BAND_6GHZ:
		if (he_cap->has_he) {
			if (he_cap->he_cap_elem.phy_cap_info[0] &
			    (IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
			    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G)) {
				return IEEE80211_STA_RX_BW_160;
			} else if (he_cap->he_cap_elem.phy_cap_info[0] &
				   IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G) {
				return IEEE80211_STA_RX_BW_80;
			}
		}
		break;
	case NL80211_BAND_2GHZ:
		if (he_cap->has_he &&
		    (he_cap->he_cap_elem.phy_cap_info[0] &
		     IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G))
			return IEEE80211_STA_RX_BW_40;
		break;
	default:
		break;
	}

	if (sband->vht_cap.vht_supported) {
		switch (sband->vht_cap.cap &
			IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK) {
		case IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ:
		case IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ:
			return IEEE80211_STA_RX_BW_160;
		default:
			return sta_bw;
		}
	}

	/* Keep Last */
	if (sband->ht_cap.ht_supported &&
	    (sband->ht_cap.cap & IEEE80211_HT_CAP_SUP_WIDTH_20_40))
		return IEEE80211_STA_RX_BW_40;

	return sta_bw;
}

static void ath12k_peer_assoc_h_rates(struct ath12k *ar,
				      struct ath12k_link_vif *arvif,
				      struct ath12k_link_sta *arsta,
				      struct ath12k_wmi_peer_assoc_arg *arg,
				      struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct wmi_rate_set_arg *rateset = &arg->peer_legacy_rates;
	struct cfg80211_chan_def def;
	const struct ieee80211_supported_band *sband;
	const struct ieee80211_rate *rates;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	enum nl80211_band band;
	u32 ratemask;
	u8 rate;
	int i;

	lockdep_assert_wiphy(hw->wiphy);

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc rates for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	sband = hw->wiphy->bands[band];
	ratemask = link_sta->supp_rates[band];
	ratemask &= arvif->bitrate_mask.control[band].legacy;
	rates = sband->bitrates;

	rateset->num_rates = 0;

	for (i = 0; i < 32; i++, ratemask >>= 1, rates++) {
		if (!(ratemask & 1))
			continue;

		rate = ath12k_mac_bitrate_to_rate(rates->bitrate);
		rateset->rates[rateset->num_rates] = rate;
		rateset->num_rates++;
	}
}

static bool
ath12k_peer_assoc_h_ht_masked(const u8 *ht_mcs_mask)
{
	int nss;

	for (nss = 0; nss < IEEE80211_HT_MCS_MASK_LEN; nss++)
		if (ht_mcs_mask[nss])
			return false;

	return true;
}

static bool
ath12k_peer_assoc_h_vht_masked(const u16 *vht_mcs_mask)
{
	int nss;

	for (nss = 0; nss < NL80211_VHT_NSS_MAX; nss++)
		if (vht_mcs_mask[nss])
			return false;

	return true;
}

static void ath12k_peer_assoc_h_ht(struct ath12k *ar,
				   struct ath12k_link_vif *arvif,
				   struct ath12k_link_sta *arsta,
				   struct ath12k_wmi_peer_assoc_arg *arg,
				   struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	const struct ieee80211_sta_ht_cap *ht_cap;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	int i, n;
	u8 max_nss;
	u32 stbc;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc ht for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	ht_cap = &link_sta->ht_cap;
	if (!ht_cap->ht_supported)
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	ht_mcs_mask = arvif->bitrate_mask.control[band].ht_mcs;

	if (ath12k_peer_assoc_h_ht_masked(ht_mcs_mask))
		return;

	arg->ht_flag = true;

	arg->peer_max_mpdu = (1 << (IEEE80211_HT_MAX_AMPDU_FACTOR +
				    ht_cap->ampdu_factor)) - 1;

	arg->peer_mpdu_density =
		ath12k_parse_mpdudensity(ht_cap->ampdu_density);

	arg->peer_ht_caps = ht_cap->cap;
	arg->peer_rate_caps |= WMI_HOST_RC_HT_FLAG;

	if (ht_cap->cap & IEEE80211_HT_CAP_LDPC_CODING)
		arg->ldpc_flag = true;

	if (link_sta->bandwidth >= IEEE80211_STA_RX_BW_40) {
		arg->bw_40 = true;
		arg->peer_rate_caps |= WMI_HOST_RC_CW40_FLAG;
	}

	/* As firmware handles these two flags (IEEE80211_HT_CAP_SGI_20
	 * and IEEE80211_HT_CAP_SGI_40) for enabling SGI, reset both
	 * flags if guard interval is to force Long GI
	 */
	if (arvif->bitrate_mask.control[band].gi == NL80211_TXRATE_FORCE_LGI) {
		arg->peer_ht_caps &= ~(IEEE80211_HT_CAP_SGI_20 | IEEE80211_HT_CAP_SGI_40);
	} else {
		/* Enable SGI flag if either SGI_20 or SGI_40 is supported */
		if (ht_cap->cap & (IEEE80211_HT_CAP_SGI_20 | IEEE80211_HT_CAP_SGI_40))
			arg->peer_rate_caps |= WMI_HOST_RC_SGI_FLAG;
	}

	if (ht_cap->cap & IEEE80211_HT_CAP_TX_STBC) {
		arg->peer_rate_caps |= WMI_HOST_RC_TX_STBC_FLAG;
		arg->stbc_flag = true;
	}

	if (ht_cap->cap & IEEE80211_HT_CAP_RX_STBC) {
		stbc = ht_cap->cap & IEEE80211_HT_CAP_RX_STBC;
		stbc = stbc >> IEEE80211_HT_CAP_RX_STBC_SHIFT;
		stbc = stbc << WMI_HOST_RC_RX_STBC_FLAG_S;
		arg->peer_rate_caps |= stbc;
		arg->stbc_flag = true;
	}

	if (ht_cap->mcs.rx_mask[1] && ht_cap->mcs.rx_mask[2])
		arg->peer_rate_caps |= WMI_HOST_RC_TS_FLAG;
	else if (ht_cap->mcs.rx_mask[1])
		arg->peer_rate_caps |= WMI_HOST_RC_DS_FLAG;

	for (i = 0, n = 0, max_nss = 0; i < IEEE80211_HT_MCS_MASK_LEN * 8; i++)
		if ((ht_cap->mcs.rx_mask[i / 8] & BIT(i % 8)) &&
		    (ht_mcs_mask[i / 8] & BIT(i % 8))) {
			max_nss = (i / 8) + 1;
			arg->peer_ht_rates.rates[n++] = i;
		}

	/* This is a workaround for HT-enabled STAs which break the spec
	 * and have no HT capabilities RX mask (no HT RX MCS map).
	 *
	 * As per spec, in section 20.3.5 Modulation and coding scheme (MCS),
	 * MCS 0 through 7 are mandatory in 20MHz with 800 ns GI at all STAs.
	 *
	 * Firmware asserts if such situation occurs.
	 */
	if (n == 0) {
		arg->peer_ht_rates.num_rates = 8;
		for (i = 0; i < arg->peer_ht_rates.num_rates; i++)
			arg->peer_ht_rates.rates[i] = i;
	} else {
		arg->peer_ht_rates.num_rates = n;
		arg->peer_nss = min(link_sta->rx_nss, max_nss);
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"mac ht peer %pM mcs cnt %d nss %d\n",
			arg->peer_mac,
			arg->peer_ht_rates.num_rates,
			arg->peer_nss);
}

static int ath12k_mac_get_max_vht_mcs_map(u16 mcs_map, int nss)
{
	switch ((mcs_map >> (2 * nss)) & 0x3) {
	case IEEE80211_VHT_MCS_SUPPORT_0_7: return BIT(8) - 1;
	case IEEE80211_VHT_MCS_SUPPORT_0_8: return BIT(9) - 1;
	case IEEE80211_VHT_MCS_SUPPORT_0_9: return BIT(10) - 1;
	}
	return 0;
}

static u16
ath12k_peer_assoc_h_vht_limit(u16 tx_mcs_set,
			      const u16 vht_mcs_limit[NL80211_VHT_NSS_MAX])
{
	int idx_limit;
	int nss;
	u16 mcs_map;
	u16 mcs;

	for (nss = 0; nss < NL80211_VHT_NSS_MAX; nss++) {
		mcs_map = ath12k_mac_get_max_vht_mcs_map(tx_mcs_set, nss) &
			  vht_mcs_limit[nss];

		if (mcs_map)
			idx_limit = fls(mcs_map) - 1;
		else
			idx_limit = -1;

		switch (idx_limit) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			mcs = IEEE80211_VHT_MCS_SUPPORT_0_7;
			break;
		case 8:
			mcs = IEEE80211_VHT_MCS_SUPPORT_0_8;
			break;
		case 9:
			mcs = IEEE80211_VHT_MCS_SUPPORT_0_9;
			break;
		default:
			WARN_ON(1);
			fallthrough;
		case -1:
			mcs = IEEE80211_VHT_MCS_NOT_SUPPORTED;
			break;
		}

		tx_mcs_set &= ~(0x3 << (nss * 2));
		tx_mcs_set |= mcs << (nss * 2);
	}

	return tx_mcs_set;
}

static u8 ath12k_get_nss_160mhz(struct ath12k *ar,
				u8 max_nss)
{
	u8 nss_ratio_info = ar->pdev->cap.nss_ratio_info;
	u8 max_sup_nss = 0;

	switch (nss_ratio_info) {
	case WMI_NSS_RATIO_1BY2_NSS:
		max_sup_nss = max_nss >> 1;
		break;
	case WMI_NSS_RATIO_3BY4_NSS:
		ath12k_warn(ar->ab, "WMI_NSS_RATIO_3BY4_NSS not supported\n");
		break;
	case WMI_NSS_RATIO_1_NSS:
		max_sup_nss = max_nss;
		break;
	case WMI_NSS_RATIO_2_NSS:
		ath12k_warn(ar->ab, "WMI_NSS_RATIO_2_NSS not supported\n");
		break;
	default:
		ath12k_warn(ar->ab, "invalid nss ratio received from fw: %d\n",
			    nss_ratio_info);
		break;
	}

	return max_sup_nss;
}

static u8 ath12k_get_nss_320mhz(struct ath12k *ar,
				u8 max_nss)
{
	u8 nss_ratio_info = ar->pdev->cap.nss_ratio_info;
	u8 max_sup_nss = 0;

	switch (nss_ratio_info) {
	case WMI_NSS_RATIO_1BY2_NSS:
		max_sup_nss = max_nss >> 1;
		break;
	case WMI_NSS_RATIO_3BY4_NSS:
		ath12k_warn(ar->ab, "WMI_NSS_RATIO_3BY4_NSS not supported\n");
		break;
	case WMI_NSS_RATIO_1_NSS:
		max_sup_nss = max_nss;
		break;
	case WMI_NSS_RATIO_2_NSS:
		ath12k_warn(ar->ab, "WMI_NSS_RATIO_2_NSS not supported\n");
		break;
	default:
		ath12k_warn(ar->ab, "invalid nss ratio received from fw: %d\n",
			    nss_ratio_info);
		break;
	}

	return max_sup_nss;
}

static void ath12k_peer_assoc_h_vht(struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    struct ath12k_link_sta *arsta,
				    struct ath12k_wmi_peer_assoc_arg *arg,
				    struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	const struct ieee80211_sta_vht_cap *vht_cap;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	u16 *vht_mcs_mask;
	u16 tx_mcs_map;
	u8 ampdu_factor;
	u8 max_nss, vht_mcs;
	int i, vht_nss, nss_idx;
	bool user_rate_valid = true;
	u32 rx_nss, tx_nss, nss_160;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc vht for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	vht_cap = &link_sta->vht_cap;
	if (!vht_cap->vht_supported)
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	vht_mcs_mask = arvif->bitrate_mask.control[band].vht_mcs;

	if (ath12k_peer_assoc_h_vht_masked(vht_mcs_mask))
		return;

	arg->vht_flag = true;

	/* TODO: similar flags required? */
	arg->vht_capable = true;

	if (band == NL80211_BAND_2GHZ)
		arg->vht_ng_flag = true;

	arg->peer_vht_caps = vht_cap->cap;

	ampdu_factor = (vht_cap->cap &
			IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK) >>
		       IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT;

	/* Workaround: Some Netgear/Linksys 11ac APs set Rx A-MPDU factor to
	 * zero in VHT IE. Using it would result in degraded throughput.
	 * arg->peer_max_mpdu at this point contains HT max_mpdu so keep
	 * it if VHT max_mpdu is smaller.
	 */
	arg->peer_max_mpdu = max(arg->peer_max_mpdu,
				 (1U << (IEEE80211_HT_MAX_AMPDU_FACTOR +
					ampdu_factor)) - 1);

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_80)
		arg->bw_80 = true;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_160)
		arg->bw_160 = true;

	vht_nss =  ath12k_mac_max_vht_nss(vht_mcs_mask);

	if (vht_nss > link_sta->rx_nss) {
		user_rate_valid = false;
		for (nss_idx = link_sta->rx_nss - 1; nss_idx >= 0; nss_idx--) {
			if (vht_mcs_mask[nss_idx]) {
				user_rate_valid = true;
				break;
			}
		}
	}

	if (!user_rate_valid) {
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"Setting vht range MCS value to peer supported nss:%d for peer %pM\n",
				link_sta->rx_nss, arsta->addr);
		vht_mcs_mask[link_sta->rx_nss - 1] = vht_mcs_mask[vht_nss - 1];
	}

	/* Calculate peer NSS capability from VHT capabilities if STA
	 * supports VHT.
	 */
	for (i = 0, max_nss = 0, vht_mcs = 0; i < NL80211_VHT_NSS_MAX; i++) {
		vht_mcs = __le16_to_cpu(vht_cap->vht_mcs.rx_mcs_map) >>
			  (2 * i) & 3;

		if (vht_mcs != IEEE80211_VHT_MCS_NOT_SUPPORTED &&
		    vht_mcs_mask[i])
			max_nss = i + 1;
	}
	arg->peer_nss = min(link_sta->rx_nss, max_nss);
	arg->rx_max_rate = __le16_to_cpu(vht_cap->vht_mcs.rx_highest);
	arg->rx_mcs_set = __le16_to_cpu(vht_cap->vht_mcs.rx_mcs_map);
	arg->tx_max_rate = __le16_to_cpu(vht_cap->vht_mcs.tx_highest);

	tx_mcs_map = __le16_to_cpu(vht_cap->vht_mcs.tx_mcs_map);
	arg->tx_mcs_set = ath12k_peer_assoc_h_vht_limit(tx_mcs_map, vht_mcs_mask);

	/* In QCN9274 platform, VHT MCS rate 10 and 11 is enabled by default.
	 * VHT MCS rate 10 and 11 is not supported in 11ac standard.
	 * so explicitly disable the VHT MCS rate 10 and 11 in 11ac mode.
	 */
	arg->tx_mcs_set &= ~IEEE80211_VHT_MCS_SUPPORT_0_11_MASK;
	arg->tx_mcs_set |= IEEE80211_DISABLE_VHT_MCS_SUPPORT_0_11;

	if ((arg->tx_mcs_set & IEEE80211_VHT_MCS_NOT_SUPPORTED) ==
			IEEE80211_VHT_MCS_NOT_SUPPORTED)
		arg->peer_vht_caps &= ~IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE;

	/* TODO:  Check */
	arg->tx_max_mcs_nss = 0xFF;

	if (arg->peer_phymode == MODE_11AC_VHT160) {
		tx_nss = ath12k_get_nss_160mhz(ar, max_nss);
		rx_nss = min(arg->peer_nss, tx_nss);
		arg->peer_bw_rxnss_override = ATH12K_BW_NSS_MAP_ENABLE;

		if (!rx_nss) {
			ath12k_warn(ar->ab, "invalid max_nss\n");
			return;
		}

		nss_160 = u32_encode_bits(rx_nss - 1, ATH12K_PEER_RX_NSS_160MHZ);
		arg->peer_bw_rxnss_override |= nss_160;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac vht peer %pM max_mpdu %d flags 0x%x nss_override 0x%x\n",
		   arsta->addr, arg->peer_max_mpdu, arg->peer_flags,
		   arg->peer_bw_rxnss_override);
}

static int ath12k_mac_get_max_he_mcs_map(u16 mcs_map, int nss)
{
	switch ((mcs_map >> (2 * nss)) & 0x3) {
	case IEEE80211_HE_MCS_SUPPORT_0_7: return BIT(8) - 1;
	case IEEE80211_HE_MCS_SUPPORT_0_9: return BIT(10) - 1;
	case IEEE80211_HE_MCS_SUPPORT_0_11: return BIT(12) - 1;
	}
	return 0;
}

static u16 ath12k_peer_assoc_h_he_limit(u16 tx_mcs_set,
					const u16 *he_mcs_limit)
{
	int idx_limit;
	int nss;
	u16 mcs_map;
	u16 mcs;

	for (nss = 0; nss < NL80211_HE_NSS_MAX; nss++) {
		mcs_map = ath12k_mac_get_max_he_mcs_map(tx_mcs_set, nss) &
			he_mcs_limit[nss];

		if (mcs_map)
			idx_limit = fls(mcs_map) - 1;
		else
			idx_limit = -1;

		switch (idx_limit) {
		case 0 ... 7:
			mcs = IEEE80211_HE_MCS_SUPPORT_0_7;
			break;
		case 8:
		case 9:
			mcs = IEEE80211_HE_MCS_SUPPORT_0_9;
			break;
		case 10:
		case 11:
			mcs = IEEE80211_HE_MCS_SUPPORT_0_11;
			break;
		default:
			WARN_ON(1);
			fallthrough;
		case -1:
			mcs = IEEE80211_HE_MCS_NOT_SUPPORTED;
			break;
		}

		tx_mcs_set &= ~(0x3 << (nss * 2));
		tx_mcs_set |= mcs << (nss * 2);
	}

	return tx_mcs_set;
}

static bool
ath12k_peer_assoc_h_he_masked(const u16 he_mcs_mask[NL80211_HE_NSS_MAX])
{
	int nss;

	for (nss = 0; nss < NL80211_HE_NSS_MAX; nss++)
		if (he_mcs_mask[nss])
			return false;

	return true;
}

static void ath12k_peer_assoc_h_he(struct ath12k *ar,
				   struct ath12k_link_vif *arvif,
				   struct ath12k_link_sta *arsta,
				   struct ath12k_wmi_peer_assoc_arg *arg,
				   struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	enum ieee80211_sta_rx_bandwidth radio_max_bw_caps;
	const struct ieee80211_sta_he_cap *he_cap;
	struct ieee80211_bss_conf *link_conf;
	struct cfg80211_chan_def def;
	int i;
	u8 ampdu_factor, max_nss;
	u8 rx_mcs_80 = IEEE80211_HE_MCS_NOT_SUPPORTED;
	u8 rx_mcs_160 = IEEE80211_HE_MCS_NOT_SUPPORTED;
	u16 mcs_160_map, mcs_80_map;
	u8 link_id = arvif->link_id;
	bool support_160;
	enum nl80211_band band;
	u16 *he_mcs_mask;
	u8 he_mcs;
	u16 he_tx_mcs = 0, v = 0;
	int he_nss, nss_idx;
	bool user_rate_valid = true;
	u32 rx_nss, tx_nss, nss_160;
	u32 peer_he_ops;

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, link_id, &def)))
		return;

	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!ath12k_mac_is_bridge_vdev(arvif) && !link_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf in peer assoc he for vif %pM link %u",
			    vif->addr, link_id);
		return;
	}

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc he for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	he_cap = &link_sta->he_cap;
	if (!he_cap->has_he)
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	he_mcs_mask = arvif->bitrate_mask.control[band].he_mcs;
	radio_max_bw_caps = ath12k_get_radio_max_bw_caps(ar, band, link_sta->bandwidth,
						 vif->type);

	if (ath12k_peer_assoc_h_he_masked(he_mcs_mask))
		return;

	arg->he_flag = true;

	support_160 = !!(he_cap->he_cap_elem.phy_cap_info[0] &
		  IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G);

	/* Supported HE-MCS and NSS Set of peer he_cap is intersection with self he_cp */
	mcs_160_map = le16_to_cpu(he_cap->he_mcs_nss_supp.rx_mcs_160);
	mcs_80_map = le16_to_cpu(he_cap->he_mcs_nss_supp.rx_mcs_80);

	if (support_160) {
		for (i = 7; i >= 0; i--) {
			u8 mcs_160 = (mcs_160_map >> (2 * i)) & 3;

			if (mcs_160 != IEEE80211_HE_MCS_NOT_SUPPORTED) {
				rx_mcs_160 = i + 1;
				break;
			}
		}
	}

	for (i = 7; i >= 0; i--) {
		u8 mcs_80 = (mcs_80_map >> (2 * i)) & 3;

		if (mcs_80 != IEEE80211_HE_MCS_NOT_SUPPORTED) {
			rx_mcs_80 = i + 1;
			break;
		}
	}

	if (support_160)
		max_nss = min(rx_mcs_80, rx_mcs_160);
	else
		max_nss = rx_mcs_80;

	arg->peer_nss = min(link_sta->rx_nss, max_nss);

	memcpy(&arg->peer_he_cap_macinfo, he_cap->he_cap_elem.mac_cap_info,
	       sizeof(he_cap->he_cap_elem.mac_cap_info));
	memcpy(&arg->peer_he_cap_phyinfo, he_cap->he_cap_elem.phy_cap_info,
	       sizeof(he_cap->he_cap_elem.phy_cap_info));

	if (ath12k_mac_is_bridge_vdev(arvif))
		peer_he_ops = 0;
	else
		peer_he_ops = link_conf->he_oper.params;

	arg->peer_he_ops = peer_he_ops;
	/* the top most byte is used to indicate BSS color info */
	arg->peer_he_ops &= 0xffffff;

	/* As per section 26.6.1 IEEE Std 802.11ax‐2022, if the Max AMPDU
	 * Exponent Extension in HE cap is zero, use the arg->peer_max_mpdu
	 * as calculated while parsing VHT caps(if VHT caps is present)
	 * or HT caps (if VHT caps is not present).
	 *
	 * For non-zero value of Max AMPDU Exponent Extension in HE MAC caps,
	 * if a HE STA sends VHT cap and HE cap IE in assoc request then, use
	 * MAX_AMPDU_LEN_FACTOR as 20 to calculate max_ampdu length.
	 * If a HE STA that does not send VHT cap, but HE and HT cap in assoc
	 * request, then use MAX_AMPDU_LEN_FACTOR as 16 to calculate max_ampdu
	 * length.
	 */
	ampdu_factor = u8_get_bits(he_cap->he_cap_elem.mac_cap_info[3],
				   IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_MASK);

	if (ampdu_factor) {
		if (link_sta->vht_cap.vht_supported)
			arg->peer_max_mpdu = (1 << (IEEE80211_HE_VHT_MAX_AMPDU_FACTOR +
						    ampdu_factor)) - 1;
		else if (link_sta->ht_cap.ht_supported)
			arg->peer_max_mpdu = (1 << (IEEE80211_HE_HT_MAX_AMPDU_FACTOR +
						    ampdu_factor)) - 1;
	}

	if (he_cap->he_cap_elem.phy_cap_info[6] &
	    IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT) {
		int bit = 7;
		int nss, ru;

		arg->peer_ppet.numss_m1 = he_cap->ppe_thres[0] &
					  IEEE80211_PPE_THRES_NSS_MASK;
		arg->peer_ppet.ru_bit_mask =
			(he_cap->ppe_thres[0] &
			 IEEE80211_PPE_THRES_RU_INDEX_BITMASK_MASK) >>
			IEEE80211_PPE_THRES_RU_INDEX_BITMASK_POS;

		for (nss = 0; nss <= arg->peer_ppet.numss_m1; nss++) {
			for (ru = 0; ru < 4; ru++) {
				u32 val = 0;
				int i;

				if ((arg->peer_ppet.ru_bit_mask & BIT(ru)) == 0)
					continue;
				for (i = 0; i < 6; i++) {
					val >>= 1;
					val |= ((he_cap->ppe_thres[bit / 8] >>
						 (bit % 8)) & 0x1) << 5;
					bit++;
				}
				arg->peer_ppet.ppet16_ppet8_ru3_ru0[nss] |=
								val << (ru * 6);
			}
		}
	}

	if (he_cap->he_cap_elem.mac_cap_info[0] & IEEE80211_HE_MAC_CAP0_TWT_RES)
		arg->twt_responder = true;
	if (he_cap->he_cap_elem.mac_cap_info[0] & IEEE80211_HE_MAC_CAP0_TWT_REQ)
		arg->twt_requester = true;

	he_nss = ath12k_mac_max_he_nss(he_mcs_mask);

	if (he_nss > link_sta->rx_nss) {
		user_rate_valid = false;
		for (nss_idx = link_sta->rx_nss - 1; nss_idx >= 0; nss_idx--) {
			if (he_mcs_mask[nss_idx]) {
				user_rate_valid = true;
				break;
			}
		}
	}

	if (!user_rate_valid) {
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"Setting he range MCS value to peer supported nss:%d for peer %pM\n",
				link_sta->rx_nss, arsta->addr);
		he_mcs_mask[link_sta->rx_nss - 1] = he_mcs_mask[he_nss - 1];
	}

	switch (min(link_sta->sta_max_bandwidth, radio_max_bw_caps)) {
	case IEEE80211_STA_RX_BW_160:
		v = le16_to_cpu(he_cap->he_mcs_nss_supp.rx_mcs_160);
		arg->peer_he_rx_mcs_set[WMI_HECAP_TXRX_MCS_NSS_IDX_160] = v;

		v = ath12k_peer_assoc_h_he_limit(v, he_mcs_mask);
		arg->peer_he_tx_mcs_set[WMI_HECAP_TXRX_MCS_NSS_IDX_160] = v;

		arg->peer_he_mcs_count++;
		if (!he_tx_mcs)
			he_tx_mcs = v;
		fallthrough;

	default:
		v = le16_to_cpu(he_cap->he_mcs_nss_supp.rx_mcs_80);
		arg->peer_he_rx_mcs_set[WMI_HECAP_TXRX_MCS_NSS_IDX_80] = v;

		v = le16_to_cpu(he_cap->he_mcs_nss_supp.tx_mcs_80);
		v = ath12k_peer_assoc_h_he_limit(v, he_mcs_mask);
		arg->peer_he_tx_mcs_set[WMI_HECAP_TXRX_MCS_NSS_IDX_80] = v;

		arg->peer_he_mcs_count++;
		if (!he_tx_mcs)
			he_tx_mcs = v;
		break;
	}

	/* Calculate peer NSS capability from HE capabilities if STA
	 * supports HE.
	 */
	for (i = 0, max_nss = 0, he_mcs = 0; i < NL80211_HE_NSS_MAX; i++) {
		he_mcs = he_tx_mcs >> (2 * i) & 3;

		/* In case of fixed rates, MCS Range in he_tx_mcs might have
		 * unsupported range, with he_mcs_mask set, so check either of them
		 * to find nss.
		 */
		if (he_mcs != IEEE80211_HE_MCS_NOT_SUPPORTED ||
		    he_mcs_mask[i])
			max_nss = i + 1;
	}
	arg->peer_nss = min(link_sta->rx_nss, max_nss);
	max_nss = min(max_nss, ar->num_tx_chains);

	if (arg->peer_phymode == MODE_11AX_HE160) {
		tx_nss = ath12k_get_nss_160mhz(ar, ar->num_tx_chains);
		rx_nss = min(arg->peer_nss, tx_nss);

		arg->peer_nss = min(link_sta->rx_nss, ar->num_rx_chains);
		arg->peer_bw_rxnss_override = ATH12K_BW_NSS_MAP_ENABLE;

		if (!rx_nss) {
			ath12k_warn(ar->ab, "invalid max_nss\n");
			return;
		}

		nss_160 = u32_encode_bits(rx_nss - 1, ATH12K_PEER_RX_NSS_160MHZ);
		arg->peer_bw_rxnss_override |= nss_160;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac he peer %pM nss %d mcs cnt %d nss_override 0x%x\n",
		   arsta->addr, arg->peer_nss,
		   arg->peer_he_mcs_count,
		   arg->peer_bw_rxnss_override);
}

static void ath12k_peer_assoc_h_he_6ghz(struct ath12k *ar,
					struct ath12k_link_vif *arvif,
					struct ath12k_link_sta *arsta,
					struct ath12k_wmi_peer_assoc_arg *arg,
					struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	const struct ieee80211_sta_he_cap *he_cap;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	u8 ampdu_factor, mpdu_density;

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc he 6ghz for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	he_cap = &link_sta->he_cap;

	if (!arg->he_flag || band != NL80211_BAND_6GHZ || !link_sta->he_6ghz_capa.capa)
		return;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
		arg->bw_40 = true;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_80)
		arg->bw_80 = true;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_160)
		arg->bw_160 = true;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_320)
		arg->bw_320 = true;

	arg->peer_he_caps_6ghz = le16_to_cpu(link_sta->he_6ghz_capa.capa);

	mpdu_density = u32_get_bits(arg->peer_he_caps_6ghz,
				    IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START);
	arg->peer_mpdu_density = ath12k_parse_mpdudensity(mpdu_density);

	/* From IEEE Std 802.11ax-2021 - Section 10.12.2: An HE STA shall be capable of
	 * receiving A-MPDU where the A-MPDU pre-EOF padding length is up to the value
	 * indicated by the Maximum A-MPDU Length Exponent Extension field in the HE
	 * Capabilities element and the Maximum A-MPDU Length Exponent field in HE 6 GHz
	 * Band Capabilities element in the 6 GHz band.
	 *
	 * Here, we are extracting the Max A-MPDU Exponent Extension from HE caps and
	 * factor is the Maximum A-MPDU Length Exponent from HE 6 GHZ Band capability.
	 */
	ampdu_factor = u8_get_bits(he_cap->he_cap_elem.mac_cap_info[3],
				   IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_MASK) +
			u32_get_bits(arg->peer_he_caps_6ghz,
				     IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP);

	arg->peer_max_mpdu = (1u << (IEEE80211_HE_6GHZ_MAX_AMPDU_FACTOR +
				     ampdu_factor)) - 1;
}

static int ath12k_get_smps_from_capa(const struct ieee80211_sta_ht_cap *ht_cap,
				     const struct ieee80211_sta_he_cap *he_cap,
				     const struct ieee80211_he_6ghz_capa *he_6ghz_capa,
				     int *smps)
{
	if (ht_cap->ht_supported)
		*smps = u16_get_bits(ht_cap->cap, IEEE80211_HT_CAP_SM_PS);
	else
		*smps = le16_get_bits(he_6ghz_capa->capa,
				      IEEE80211_HE_6GHZ_CAP_SM_PS);

	if (he_cap->has_he) {
		if (he_cap->he_cap_elem.mac_cap_info[5] &
		    IEEE80211_HE_MAC_CAP5_HE_DYNAMIC_SM_PS) {
			*smps = WLAN_HT_CAP_SM_PS_DYNAMIC;
		}
	}

	if (*smps >= ARRAY_SIZE(ath12k_smps_map))
		return -EINVAL;

	return 0;
}

static void ath12k_peer_assoc_h_smps(struct ath12k_link_sta *arsta,
				     struct ath12k_wmi_peer_assoc_arg *arg,
				     struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	const struct ieee80211_he_6ghz_capa *he_6ghz_capa;
	struct ath12k_link_vif *arvif = arsta->arvif;
	const struct ieee80211_sta_ht_cap *ht_cap;
	const struct ieee80211_sta_he_cap *he_cap;
	struct ath12k *ar = arvif->ar;
	int smps;

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc he for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	he_6ghz_capa = &link_sta->he_6ghz_capa;
	ht_cap = &link_sta->ht_cap;
	he_cap = &link_sta->he_cap;

	if (!ht_cap->ht_supported && !he_cap->has_he && !he_6ghz_capa->capa)
		return;

	if (ath12k_get_smps_from_capa(ht_cap, he_cap, he_6ghz_capa, &smps))
		return;

	if (he_cap->has_he) {
		if (he_cap->he_cap_elem.mac_cap_info[5] & IEEE80211_HE_MAC_CAP5_HE_DYNAMIC_SM_PS) {
			smps = WLAN_HT_CAP_SM_PS_DYNAMIC;
		}
	}

	switch (smps) {
	case WLAN_HT_CAP_SM_PS_STATIC:
		arg->static_mimops_flag = true;
		break;
	case WLAN_HT_CAP_SM_PS_DYNAMIC:
		arg->dynamic_mimops_flag = true;
		break;
	case WLAN_HT_CAP_SM_PS_DISABLED:
		arg->spatial_mux_flag = true;
		break;
	default:
		break;
	}
}

static void ath12k_peer_assoc_h_qos(struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    struct ath12k_link_sta *arsta,
				    struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);

	switch (arvif->ahvif->vdev_type) {
	case WMI_VDEV_TYPE_AP:
		if (sta->wme) {
			/* TODO: Check WME vs QoS */
			arg->is_wme_set = true;
			arg->qos_flag = true;
		}

		if (sta->wme && sta->uapsd_queues) {
			/* TODO: Check WME vs QoS */
			arg->is_wme_set = true;
			arg->apsd_flag = true;
			arg->peer_rate_caps |= WMI_HOST_RC_UAPSD_FLAG;
		}
		break;
	case WMI_VDEV_TYPE_STA:
		if (sta->wme) {
			arg->is_wme_set = true;
			arg->qos_flag = true;
		}
		break;
	default:
		break;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2, "mac peer %pM qos %d\n",
			arsta->addr, arg->qos_flag);
}

static int ath12k_peer_assoc_qos_ap(struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    struct ath12k_link_sta *arsta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ath12k_wmi_ap_ps_arg arg;
	u32 max_sp;
	u32 uapsd;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	arg.vdev_id = arvif->vdev_id;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"mac uapsd_queues 0x%x max_sp %d\n",
			sta->uapsd_queues, sta->max_sp);

	uapsd = 0;
	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_VO)
		uapsd |= WMI_AP_PS_UAPSD_AC3_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC3_TRIGGER_EN;
	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_VI)
		uapsd |= WMI_AP_PS_UAPSD_AC2_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC2_TRIGGER_EN;
	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_BK)
		uapsd |= WMI_AP_PS_UAPSD_AC1_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC1_TRIGGER_EN;
	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_BE)
		uapsd |= WMI_AP_PS_UAPSD_AC0_DELIVERY_EN |
			 WMI_AP_PS_UAPSD_AC0_TRIGGER_EN;

	max_sp = 0;
	if (sta->max_sp < MAX_WMI_AP_PS_PEER_PARAM_MAX_SP)
		max_sp = sta->max_sp;

	arg.param = WMI_AP_PS_PEER_PARAM_UAPSD;
	arg.value = uapsd;
	ret = ath12k_wmi_send_set_ap_ps_param_cmd(ar, arsta->addr, &arg);
	if (ret)
		goto err;

	arg.param = WMI_AP_PS_PEER_PARAM_MAX_SP;
	arg.value = max_sp;
	ret = ath12k_wmi_send_set_ap_ps_param_cmd(ar, arsta->addr, &arg);
	if (ret)
		goto err;

	/* TODO: revisit during testing */
	arg.param = WMI_AP_PS_PEER_PARAM_SIFS_RESP_FRMTYPE;
	arg.value = DISABLE_SIFS_RESPONSE_TRIGGER;
	ret = ath12k_wmi_send_set_ap_ps_param_cmd(ar, arsta->addr, &arg);
	if (ret)
		goto err;

	arg.param = WMI_AP_PS_PEER_PARAM_SIFS_RESP_UAPSD;
	arg.value = DISABLE_SIFS_RESPONSE_TRIGGER;
	ret = ath12k_wmi_send_set_ap_ps_param_cmd(ar, arsta->addr, &arg);
	if (ret)
		goto err;

	return 0;

err:
	ath12k_warn(ar->ab, "failed to set ap ps peer param %d for vdev %i: %d\n",
		    arg.param, arvif->vdev_id, ret);
	return ret;
}

static bool ath12k_mac_sta_has_ofdm_only(struct ieee80211_link_sta *sta)
{
	return sta->supp_rates[NL80211_BAND_2GHZ] >>
	       ATH12K_MAC_FIRST_OFDM_RATE_IDX;
}

static enum wmi_phy_mode ath12k_mac_get_phymode_vht(struct ath12k *ar,
						    struct ieee80211_link_sta *link_sta)
{
	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_160) {
		if (link_sta->vht_cap.cap & (IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
		    IEEE80211_VHT_CAP_EXT_NSS_BW_MASK))
			return MODE_11AC_VHT160;

		/* not sure if this is a valid case? */
		return MODE_UNKNOWN;
	}

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_80)
		return MODE_11AC_VHT80;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
		return MODE_11AC_VHT40;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_20)
		return MODE_11AC_VHT20;

	return MODE_UNKNOWN;
}

static enum wmi_phy_mode ath12k_mac_get_phymode_he(struct ath12k *ar,
						   struct ieee80211_link_sta *link_sta)
{
	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_160) {
		if (link_sta->he_cap.he_cap_elem.phy_cap_info[0] &
		     IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G)
			return MODE_11AX_HE160;

		return MODE_UNKNOWN;
	}

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_80)
		return MODE_11AX_HE80;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
		return MODE_11AX_HE40;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_20)
		return MODE_11AX_HE20;

	return MODE_UNKNOWN;
}

static enum wmi_phy_mode ath12k_mac_get_phymode_eht(struct ath12k *ar,
						    struct ieee80211_link_sta *link_sta)
{
	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_320)
		if (link_sta->eht_cap.eht_cap_elem.phy_cap_info[0] &
		    IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ)
			return MODE_11BE_EHT320;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_160) {
		if (link_sta->he_cap.he_cap_elem.phy_cap_info[0] &
		    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G)
			return MODE_11BE_EHT160;

		ath12k_warn(ar->ab, "invalid EHT PHY capability info for 160 Mhz: %d\n",
			    link_sta->he_cap.he_cap_elem.phy_cap_info[0]);

		return MODE_UNKNOWN;
	}

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_80)
		return MODE_11BE_EHT80;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
		return MODE_11BE_EHT40;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_20)
		return MODE_11BE_EHT20;

	return MODE_UNKNOWN;
}

static void
ath12k_peer_assoc_build_vendor_event(struct ath12k_sta *ahsta,
				     struct ieee80211_link_sta *link_sta,
				     struct ath12k_vendor_generic_peer_assoc_event *ev)
{
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	unsigned long links;
	struct ath12k_vendor_generic_peer_assoc_event *assoc_ev = ev;
	struct ath12k_vendor_mld_peer_link_entry *link_entry;
	u8 i = 0, link_id;

	sta = container_of((void *)ahsta, struct ieee80211_sta, drv_priv);

	links = ahsta->links_map;

	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		if (i >= ATH12K_WMI_MLO_MAX_LINKS)
			break;
		arsta = ahsta->link[link_id];
		arvif = ath12k_get_arvif_from_link_id(ahsta->ahvif, link_id);
		if (!(arvif && arvif->ar))
			continue;

		if (!arvif->is_started)
			continue;

		link_entry = &assoc_ev->link_entry[i];
		link_entry->hw_link_id = arvif->ar->pdev->hw_link_id;
		link_entry->link_id = arvif->link_id;
		link_entry->vdev_id = arvif->vdev_id;
		link_entry->device_id = ath12k_get_ab_device_id(arvif->ar->ab);
		link_entry->is_assoc_link = arsta->is_assoc_link;
		// To-Do: implement rssi_comb
		//link_entry->link_rssi = arsta->rssi_comb;
		link_entry->chan_bw = link_sta->bandwidth;
		// To-Do: Get vif's mld mac addr
		//ether_addr_copy(link_entry->ap_mld_mac_addr, sta->ml_addr);
		ether_addr_copy(link_entry->link_mac_addr, arsta->addr);

		assoc_ev->num_links++;

		i++;
	}

	if (sta->mlo)
		ether_addr_copy(assoc_ev->mld_mac_addr, sta->addr);
}


static bool
ath12k_peer_assoc_h_eht_masked(const u16 eht_mcs_mask[NL80211_EHT_NSS_MAX])
{
	int nss;

	for (nss = 0; nss < NL80211_EHT_NSS_MAX; nss++)
	       if (eht_mcs_mask[nss])
		       return false;

	return true;
}

static void ath12k_peer_assoc_h_phymode(struct ath12k *ar,
					struct ath12k_link_vif *arvif,
					struct ath12k_link_sta *arsta,
					struct ath12k_wmi_peer_assoc_arg *arg,
					struct ieee80211_link_sta *link_sta)
{
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	const u16 *vht_mcs_mask;
	const u16 *he_mcs_mask;
	const u16 *eht_mcs_mask;
	enum wmi_phy_mode phymode = MODE_UNKNOWN;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	ht_mcs_mask = arvif->bitrate_mask.control[band].ht_mcs;
	vht_mcs_mask = arvif->bitrate_mask.control[band].vht_mcs;
	he_mcs_mask = arvif->bitrate_mask.control[band].he_mcs;
	eht_mcs_mask = arvif->bitrate_mask.control[band].eht_mcs;

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc he for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	switch (band) {
	case NL80211_BAND_2GHZ:
		if (link_sta->eht_cap.has_eht &&
		    !ath12k_peer_assoc_h_eht_masked(eht_mcs_mask)) {
			if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
				phymode = MODE_11BE_EHT40_2G;
			else
				phymode = MODE_11BE_EHT20_2G;
		} else if (link_sta->he_cap.has_he &&
			   !ath12k_peer_assoc_h_he_masked(he_mcs_mask)) {
			if (link_sta->bandwidth == IEEE80211_STA_RX_BW_80)
				phymode = MODE_11AX_HE80_2G;
			else if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
				phymode = MODE_11AX_HE40_2G;
			else
				phymode = MODE_11AX_HE20_2G;
		} else if (link_sta->vht_cap.vht_supported &&
		    !ath12k_peer_assoc_h_vht_masked(vht_mcs_mask)) {
			if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
				phymode = MODE_11AC_VHT40;
			else
				phymode = MODE_11AC_VHT20;
		} else if (link_sta->ht_cap.ht_supported &&
			   !ath12k_peer_assoc_h_ht_masked(ht_mcs_mask)) {
			if (link_sta->bandwidth == IEEE80211_STA_RX_BW_40)
				phymode = MODE_11NG_HT40;
			else
				phymode = MODE_11NG_HT20;
		} else if (ath12k_mac_sta_has_ofdm_only(link_sta)) {
			phymode = MODE_11G;
		} else {
			phymode = MODE_11B;
		}
		break;
	case NL80211_BAND_5GHZ:
	case NL80211_BAND_6GHZ:
		/* Check EHT first */
		if (link_sta->eht_cap.has_eht) {
			phymode = ath12k_mac_get_phymode_eht(ar, link_sta);
		} else if (link_sta->he_cap.has_he &&
			   !ath12k_peer_assoc_h_he_masked(he_mcs_mask)) {
			phymode = ath12k_mac_get_phymode_he(ar, link_sta);
		} else if (link_sta->vht_cap.vht_supported &&
		    !ath12k_peer_assoc_h_vht_masked(vht_mcs_mask)) {
			phymode = ath12k_mac_get_phymode_vht(ar, link_sta);
		} else if (link_sta->ht_cap.ht_supported &&
			   !ath12k_peer_assoc_h_ht_masked(ht_mcs_mask)) {
			if (link_sta->bandwidth >= IEEE80211_STA_RX_BW_40)
				phymode = MODE_11NA_HT40;
			else
				phymode = MODE_11NA_HT20;
		} else {
			phymode = MODE_11A;
		}
		break;
	default:
		break;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac peer %pM phymode %s\n",
			arsta->addr, ath12k_mac_phymode_str(phymode));

	arg->peer_phymode = phymode;
	WARN_ON(phymode == MODE_UNKNOWN);
}

static void ath12k_mac_set_eht_mcs(u8 rx_tx_mcs7, u8 rx_tx_mcs9,
				   u8 rx_tx_mcs11, u8 rx_tx_mcs13,
				   u32 *rx_mcs, u32 *tx_mcs,
				   const u16 eht_mcs_limit[NL80211_EHT_NSS_MAX])
{
	int nss;
	u8 mcs_7 = 0, mcs_9 = 0, mcs_11 = 0, mcs_13 = 0;
	u8 peer_mcs_7 = 0, peer_mcs_9 = 0, peer_mcs_11 = 0, peer_mcs_13 = 0;

	for (nss = 0; nss < NL80211_EHT_NSS_MAX; nss++) {
		if (eht_mcs_limit[nss] & 0x00FF)
			mcs_7++;
		if (eht_mcs_limit[nss] & 0x0300)
			mcs_9++;
		if (eht_mcs_limit[nss] & 0x0C00)
			mcs_11++;
		if (eht_mcs_limit[nss] & 0x3000)
			mcs_13++;
	}

	peer_mcs_7 = u8_get_bits(rx_tx_mcs7, IEEE80211_EHT_MCS_NSS_RX);
	peer_mcs_9 = u8_get_bits(rx_tx_mcs9, IEEE80211_EHT_MCS_NSS_RX);
	peer_mcs_11 = u8_get_bits(rx_tx_mcs11, IEEE80211_EHT_MCS_NSS_RX);
	peer_mcs_13 = u8_get_bits(rx_tx_mcs13, IEEE80211_EHT_MCS_NSS_RX);

	*rx_mcs = FIELD_PREP(WMI_EHT_MCS_NSS_0_7, min(peer_mcs_7, mcs_7)) |
		  FIELD_PREP(WMI_EHT_MCS_NSS_8_9, min(peer_mcs_9, mcs_9)) |
		  FIELD_PREP(WMI_EHT_MCS_NSS_10_11, min(peer_mcs_11, mcs_11)) |
		  FIELD_PREP(WMI_EHT_MCS_NSS_12_13, min (peer_mcs_13, mcs_13));

	peer_mcs_7 = u8_get_bits(rx_tx_mcs7, IEEE80211_EHT_MCS_NSS_TX);
	peer_mcs_9 = u8_get_bits(rx_tx_mcs9, IEEE80211_EHT_MCS_NSS_TX);
	peer_mcs_11 = u8_get_bits(rx_tx_mcs11, IEEE80211_EHT_MCS_NSS_TX);
	peer_mcs_13 = u8_get_bits(rx_tx_mcs13, IEEE80211_EHT_MCS_NSS_TX);

	*tx_mcs = FIELD_PREP(WMI_EHT_MCS_NSS_0_7, min(peer_mcs_7, mcs_7)) |
		  FIELD_PREP(WMI_EHT_MCS_NSS_8_9, min(peer_mcs_9, mcs_9)) |
		  FIELD_PREP(WMI_EHT_MCS_NSS_10_11, min(peer_mcs_11, mcs_11)) |
		  FIELD_PREP(WMI_EHT_MCS_NSS_12_13, min (peer_mcs_13, mcs_13));

}

static void ath12k_mac_set_eht_ppe_threshold(const u8 *ppe_thres,
					     struct ath12k_wmi_ppe_threshold_arg *ppet)
{
	u32 bit_pos = IEEE80211_EHT_PPE_THRES_INFO_HEADER_SIZE, val;
	u8 nss, ru, i;
	u8 ppet_bit_len_per_ru = IEEE80211_EHT_PPE_THRES_INFO_PPET_SIZE * 2;

	ppet->numss_m1 = u8_get_bits(ppe_thres[0], IEEE80211_EHT_PPE_THRES_NSS_MASK);
	ppet->ru_bit_mask = u16_get_bits(get_unaligned_le16(ppe_thres),
					 IEEE80211_EHT_PPE_THRES_RU_INDEX_BITMASK_MASK);

	for (nss = 0; nss <= ppet->numss_m1; nss++) {
		for (ru = 0;
		     ru < hweight16(IEEE80211_EHT_PPE_THRES_RU_INDEX_BITMASK_MASK);
		     ru++) {
			if ((ppet->ru_bit_mask & BIT(ru)) == 0)
				continue;

			val = 0;
			for (i = 0; i < ppet_bit_len_per_ru; i++) {
				val |= (((ppe_thres[bit_pos / 8] >>
					  (bit_pos % 8)) & 0x1) << i);
				bit_pos++;
			}
			ppet->ppet16_ppet8_ru3_ru0[nss] |=
					(val << (ru * ppet_bit_len_per_ru));
		}
	}
}

static void ath12k_peer_assoc_h_eht(struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    struct ath12k_link_sta *arsta,
				    struct ath12k_wmi_peer_assoc_arg *arg,
				    struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	const struct ieee80211_eht_mcs_nss_supp *own_eht_mcs_nss_supp;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	const struct ieee80211_eht_mcs_nss_supp_20mhz_only *bw_20;
	enum ieee80211_sta_rx_bandwidth radio_max_bw_caps;
	const struct ieee80211_sta_eht_cap *own_eht_cap;
	const struct ieee80211_eht_mcs_nss_supp_bw *bw;
	const struct ieee80211_sta_eht_cap *eht_cap;
	const struct ieee80211_sta_he_cap *he_cap;
	struct ieee80211_bss_conf *link_conf;
	bool user_rate_valid = true;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	u8 max_nss;
	u16 *eht_mcs_mask;
	u32 *rx_mcs, *tx_mcs;
	int eht_nss, nss_idx;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer assoc eht for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	if (!ath12k_mac_is_bridge_vdev(arvif)) {
		link_conf = ath12k_mac_get_link_bss_conf(arvif);
		if (!link_conf) {
			ath12k_warn(ar->ab, "unable to access link_conf in peer assoc eht set\n");
			return;
		}
	}

	eht_cap = &link_sta->eht_cap;
	he_cap = &link_sta->he_cap;
	if (!he_cap->has_he || !eht_cap->has_eht)
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	eht_mcs_mask = arvif->bitrate_mask.control[band].eht_mcs;
	own_eht_cap = &ar->mac.sbands[band].iftype_data->eht_cap;
	own_eht_mcs_nss_supp = &own_eht_cap->eht_mcs_nss_supp;

	if (ath12k_peer_assoc_h_eht_masked((const u16*) eht_mcs_mask))
		return;

	arg->eht_flag = true;

	if (link_sta->bandwidth >= IEEE80211_STA_RX_BW_40)
		arg->bw_40 = true;

	if (link_sta->bandwidth >= IEEE80211_STA_RX_BW_80)
		arg->bw_80 = true;

	if (link_sta->bandwidth >= IEEE80211_STA_RX_BW_160)
		arg->bw_160 = true;

	if (link_sta->bandwidth == IEEE80211_STA_RX_BW_320)
		arg->bw_320 = true;

	if ((eht_cap->eht_cap_elem.phy_cap_info[5] &
	     IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT) &&
	    eht_cap->eht_ppe_thres[0] != 0)
		ath12k_mac_set_eht_ppe_threshold(eht_cap->eht_ppe_thres,
						 &arg->peer_eht_ppet);

	memcpy(arg->peer_eht_cap_mac, eht_cap->eht_cap_elem.mac_cap_info,
	       sizeof(eht_cap->eht_cap_elem.mac_cap_info));
	memcpy(arg->peer_eht_cap_phy, eht_cap->eht_cap_elem.phy_cap_info,
	       sizeof(eht_cap->eht_cap_elem.phy_cap_info));

	rx_mcs = arg->peer_eht_rx_mcs_set;
	tx_mcs = arg->peer_eht_tx_mcs_set;

	eht_nss = ath12k_mac_max_eht_mcs_nss((void *)own_eht_mcs_nss_supp,
						  sizeof(*own_eht_mcs_nss_supp));

	if (eht_nss > link_sta->rx_nss) {
		user_rate_valid = false;
		for (nss_idx = (link_sta->rx_nss - 1); nss_idx >= 0; nss_idx--) {
			if (eht_mcs_mask[nss_idx]) {
				user_rate_valid = true;
				break;
			}
		}
	}

	if (!user_rate_valid) {
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"Setting eht range MCS value to peer supported nss:%d for peer %pM\n",
				link_sta->rx_nss, arsta->addr);
		eht_mcs_mask[link_sta->rx_nss - 1] = eht_mcs_mask[eht_nss - 1];
	}

	bw_20 = &eht_cap->eht_mcs_nss_supp.only_20mhz;
	bw = &eht_cap->eht_mcs_nss_supp.bw._80;

	radio_max_bw_caps = ath12k_get_radio_max_bw_caps(ar, band,
							 link_sta->bandwidth,
							 vif->type);

	switch (min(link_sta->sta_max_bandwidth, radio_max_bw_caps)) {
	case IEEE80211_STA_RX_BW_320:
		bw = &eht_cap->eht_mcs_nss_supp.bw._320;
		ath12k_mac_set_eht_mcs(bw->rx_tx_mcs9_max_nss,
				       bw->rx_tx_mcs9_max_nss,
				       bw->rx_tx_mcs11_max_nss,
				       bw->rx_tx_mcs13_max_nss,
				       &rx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_320],
				       &tx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_320],
				       eht_mcs_mask);
		arg->peer_eht_mcs_count++;
		fallthrough;
	case IEEE80211_STA_RX_BW_160:
		bw = &eht_cap->eht_mcs_nss_supp.bw._160;
		ath12k_mac_set_eht_mcs(bw->rx_tx_mcs9_max_nss,
				       bw->rx_tx_mcs9_max_nss,
				       bw->rx_tx_mcs11_max_nss,
				       bw->rx_tx_mcs13_max_nss,
				       &rx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_160],
				       &tx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_160],
				       eht_mcs_mask);
		arg->peer_eht_mcs_count++;
		fallthrough;
	default:
		if (!(link_sta->he_cap.he_cap_elem.phy_cap_info[0] &
			IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_MASK_ALL)) {
			bw_20 = &eht_cap->eht_mcs_nss_supp.only_20mhz;

			ath12k_mac_set_eht_mcs(bw_20->rx_tx_mcs7_max_nss,
					       bw_20->rx_tx_mcs9_max_nss,
					       bw_20->rx_tx_mcs11_max_nss,
					       bw_20->rx_tx_mcs13_max_nss,
					       &rx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_80],
					       &tx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_80],
					       eht_mcs_mask);
		} else {
			bw = &eht_cap->eht_mcs_nss_supp.bw._80;
			ath12k_mac_set_eht_mcs(bw->rx_tx_mcs9_max_nss,
					       bw->rx_tx_mcs9_max_nss,
					       bw->rx_tx_mcs11_max_nss,
					       bw->rx_tx_mcs13_max_nss,
					       &rx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_80],
					       &tx_mcs[WMI_EHTCAP_TXRX_MCS_NSS_IDX_80],
					       eht_mcs_mask);
		}

		arg->peer_eht_mcs_count++;
		break;
	}

	if (!(link_sta->he_cap.he_cap_elem.phy_cap_info[0] &
	      IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_MASK_ALL)) {
		if (bw_20->rx_tx_mcs13_max_nss)
			max_nss = bw_20->rx_tx_mcs13_max_nss;
		else if (bw_20->rx_tx_mcs11_max_nss)
			max_nss = bw_20->rx_tx_mcs11_max_nss;
		else if (bw_20->rx_tx_mcs9_max_nss)
			max_nss = bw_20->rx_tx_mcs9_max_nss;
		else
			max_nss = bw_20->rx_tx_mcs7_max_nss;
	} else {
		max_nss = 0;
		if (bw->rx_tx_mcs13_max_nss)
			max_nss = max(max_nss, u8_get_bits(bw->rx_tx_mcs13_max_nss,
					      IEEE80211_EHT_MCS_NSS_RX));
		if (bw->rx_tx_mcs11_max_nss)
			max_nss = max(max_nss, u8_get_bits(bw->rx_tx_mcs11_max_nss,
					      IEEE80211_EHT_MCS_NSS_RX));
		if (bw->rx_tx_mcs9_max_nss)
			max_nss = max(max_nss, u8_get_bits(bw->rx_tx_mcs9_max_nss,
					      IEEE80211_EHT_MCS_NSS_RX));
	}

	max_nss = min(max_nss, (uint8_t)eht_nss);

	arg->peer_nss = min(link_sta->rx_nss, max_nss);

	if (arsta->is_bridge_peer)
		arg->punct_bitmap = ~link_sta->punctured;
	else
		arg->punct_bitmap = ~arvif->punct_bitmap;

        if (ieee80211_vif_is_mesh(vif) && link_sta->punctured)
                arg->punct_bitmap = ~link_sta->punctured;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac eht peer %pM nss %d mcs cnt %d ru_punct_bitmap 0x%x\n",
		   link_sta->addr, arg->peer_nss, arg->peer_he_mcs_count, arg->punct_bitmap);

	if (ath12k_mac_is_bridge_vdev(arvif))
		arg->enable_mcs15 = false;
	else
		arg->enable_mcs15 = link_conf->enable_mcs15;
}

static void ath12k_peer_assoc_h_mlo(struct ath12k_link_sta *arsta,
				    struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct peer_assoc_mlo_params *ml = &arg->ml;
	struct ath12k_sta *ahsta = arsta->ahsta;
	struct ath12k_link_sta *arsta_p;
	struct ath12k_link_vif *arvif;
	unsigned long links;
	u8 link_id;
	int i;

	if (!sta->mlo || ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID)
		return;

	ml->enabled = true;
	ml->assoc_link = arsta->is_assoc_link;

	if (arsta->link_id == ahsta->primary_link_id)
		ml->primary_umac = true;
	else
		ml->primary_umac = false;
	ml->peer_id_valid = true;
	ml->logical_link_idx_valid = true;

	ether_addr_copy(ml->mld_addr, sta->addr);
	ml->logical_link_idx = arsta->link_idx;
	ml->ml_peer_id = ahsta->ml_peer_id;
	ml->ieee_link_id = arsta->link_id;
	ml->bridge_peer = arsta->is_bridge_peer;
	ml->num_partner_links = 0;
	ml->eml_cap = sta->eml_cap;
	links = ahsta->links_map;

	if (sta->reconf.removed_links & BIT(arsta->link_id))
		ml->ml_reconfig = ml->mlo_link_del = true;

	if (sta->reconf.added_links & BIT(arsta->link_id))
		ml->ml_reconfig = ml->mlo_link_add = true;

	rcu_read_lock();

	i = 0;

	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		if (i >= ATH12K_WMI_MLO_PEER_MAX_LINKS)
			break;

		arsta_p = rcu_dereference(ahsta->link[link_id]);
		arvif = rcu_dereference(ahsta->ahvif->link[link_id]);

		if (arsta_p == arsta)
			continue;

		if (!arvif->is_started)
			continue;

		ml->partner_info[i].vdev_id = arvif->vdev_id;
		ml->partner_info[i].hw_link_id = arvif->ar->pdev->hw_link_id;
		ml->partner_info[i].assoc_link = arsta_p->is_assoc_link;
		ml->partner_info[i].bridge_peer = arsta_p->is_bridge_peer;
		if (arsta_p->link_id == ahsta->primary_link_id)
			   ml->partner_info[i].primary_umac = true;
		   else
			   ml->partner_info[i].primary_umac = false;
		   ml->partner_info[i].logical_link_idx_valid = true;
		ml->partner_info[i].logical_link_idx = arsta_p->link_idx;
		if (sta->reconf.removed_links & BIT(arsta_p->link_id))
			ml->ml_reconfig = ml->partner_info[i].mlo_link_del = true;
		if (sta->reconf.added_links & BIT(arsta_p->link_id))
			ml->ml_reconfig = ml->partner_info[i].mlo_link_add = true;
		ml->num_partner_links++;

		i++;
	}

	rcu_read_unlock();
}

static void ath12k_peer_assoc_h_ttlm(struct ath12k_link_sta *arsta,
				     struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_wmi_ttlm_peer_params *ttlm_params = &arg->ttlm_params;
	u8 i;
	u8 is_default_mapping[IEEE80211_MAX_TTLM_DIRECTION] = {0};
	unsigned long dmap = 0, umap = 0;

	if (!sta->mlo || ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID)
		return;

	memset(ttlm_params, 0, sizeof(struct ath12k_wmi_ttlm_peer_params *));

	for (i = 0; i < IEEE80211_MAX_NUM_TIDS; i++)
		dmap |= sta->neg_ttlm.downlink[i];
	for (i = 0; i < IEEE80211_MAX_NUM_TIDS; i++)
		umap |= sta->neg_ttlm.uplink[i];

	if (!umap && !dmap)
		return;

	ath12k_populate_default_mapping_flags(arsta->arvif->ahvif->vif,
					      &sta->neg_ttlm, is_default_mapping);

	ath12k_populate_wmi_ttlm_peer_params(arsta, ttlm_params,
					     is_default_mapping,
					     &sta->neg_ttlm);
}

static void ath12k_peer_assoc_prepare(struct ath12k *ar,
				      struct ath12k_link_vif *arvif,
				      struct ath12k_link_sta *arsta,
				      struct ath12k_wmi_peer_assoc_arg *arg,
				      bool reassoc,
				      struct ieee80211_link_sta *link_sta)
{
	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	memset(arg, 0, sizeof(*arg));

	reinit_completion(&ar->peer_assoc_done);

	arg->peer_new_assoc = !reassoc;
	ath12k_peer_assoc_h_basic(ar, arvif, arsta, arg);
	ath12k_peer_assoc_h_crypto(ar, arvif, arsta, arg);
	ath12k_peer_assoc_h_rates(ar, arvif, arsta, arg, link_sta);
	ath12k_peer_assoc_h_ht(ar, arvif, arsta, arg, link_sta);
	ath12k_peer_assoc_h_vht(ar, arvif, arsta, arg, link_sta);
	ath12k_peer_assoc_h_he(ar, arvif, arsta, arg, link_sta);
	ath12k_peer_assoc_h_he_6ghz(ar, arvif, arsta, arg, link_sta);
	ath12k_peer_assoc_h_eht(ar, arvif, arsta, arg, link_sta);
	ath12k_peer_assoc_h_qos(ar, arvif, arsta, arg);
	ath12k_peer_assoc_h_phymode(ar, arvif, arsta, arg, link_sta);
	ath12k_peer_assoc_h_smps(arsta, arg, link_sta);
	ath12k_peer_assoc_h_mlo(arsta, arg);
	ath12k_peer_assoc_h_ttlm(arsta, arg);

	arsta->peer_nss = arg->peer_nss;

	WARN_ON_ONCE(arsta->peer_nss < 1 ||
             (arsta->peer_nss > hweight32(ar->pdev->cap.tx_chain_mask)));

	/* TODO: amsdu_disable req? */
}

static int ath12k_setup_peer_smps(struct ath12k *ar, struct ath12k_link_vif *arvif,
				  const u8 *addr,
				  const struct ieee80211_sta_ht_cap *ht_cap,
				  const struct ieee80211_sta_he_cap *he_cap,
				  const struct ieee80211_he_6ghz_capa *he_6ghz_capa)
{
	int smps, ret = 0;

	if (!ht_cap->ht_supported && !he_6ghz_capa && !he_6ghz_capa)
		return 0;

	ret = ath12k_get_smps_from_capa(ht_cap, he_cap, he_6ghz_capa, &smps);
	if (ret < 0)
		return ret;

	return ath12k_wmi_set_peer_param(ar, addr, arvif->vdev_id,
					 WMI_PEER_MIMO_PS_STATE,
					 ath12k_smps_map[smps]);
}

int ath12k_mac_set_he_txbf_conf(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k *ar = arvif->ar;
	u32 param = WMI_VDEV_PARAM_SET_HEMU_MODE;
	u32 value = 0;
	int ret;
	struct ieee80211_bss_conf *link_conf;

	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!link_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf in txbf conf\n");
		return -EINVAL;
	}

	if (!link_conf->he_support)
		return 0;

	if (link_conf->he_su_beamformer) {
		value |= u32_encode_bits(HE_SU_BFER_ENABLE, HE_MODE_SU_TX_BFER);
		if (link_conf->he_mu_beamformer &&
		    ahvif->vdev_type == WMI_VDEV_TYPE_AP)
			value |= u32_encode_bits(HE_MU_BFER_ENABLE, HE_MODE_MU_TX_BFER);
	}

	if (ahvif->vif->type != NL80211_IFTYPE_MESH_POINT) {
		if (link_conf->he_full_ul_mumimo)
			value |= u32_encode_bits(HE_UL_MUMIMO_ENABLE, HE_MODE_UL_MUMIMO);
		if (link_conf->he_su_beamformee)
			value |= u32_encode_bits(HE_SU_BFEE_ENABLE, HE_MODE_SU_TX_BFEE);
	}
	if (ar->he_dl_enabled)
		value |= u32_encode_bits(HE_DL_MUOFDMA_ENABLE, HE_MODE_DL_OFDMA);
	if (ar->he_ul_enabled)
		value |= u32_encode_bits(HE_UL_MUOFDMA_ENABLE, HE_MODE_UL_OFDMA);
	if (ar->he_dlbf_enabled)
		value |= u32_encode_bits(HE_DL_OFDMA_TXBF_ENABLE, HE_MODE_DL_OFDMA_TXBF);

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		  "Set HE TXBF config: DL=%d UL=%d DLBF=%d, value=0x%x\n",
		  ar->he_dl_enabled, ar->he_ul_enabled, ar->he_dlbf_enabled, value);

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param, value);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set vdev %d HE MU mode: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	param = WMI_VDEV_PARAM_SET_HE_SOUNDING_MODE;
	value =	u32_encode_bits(HE_VHT_SOUNDING_MODE_ENABLE, HE_VHT_SOUNDING_MODE) |
		u32_encode_bits(HE_TRIG_NONTRIG_SOUNDING_MODE_ENABLE,
				HE_TRIG_NONTRIG_SOUNDING_MODE);
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param, value);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set vdev %d sounding mode: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	return 0;
}

static int ath12k_mac_vif_recalc_sta_he_txbf(struct ath12k *ar,
					     struct ath12k_link_vif *arvif,
					     struct ieee80211_sta_he_cap *he_cap,
					     int *hemode)
{
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct ieee80211_he_cap_elem he_cap_elem = {};
	struct ieee80211_sta_he_cap *cap_band;
	struct cfg80211_chan_def def;
	u8 link_id = arvif->link_id;
	struct ieee80211_bss_conf *link_conf;
	enum nl80211_band band;

	if (!ath12k_mac_is_bridge_vdev(arvif)) {
		link_conf = ath12k_mac_get_link_bss_conf(arvif);
		if (!link_conf) {
			ath12k_warn(ar->ab, "unable to access bss link conf in recalc txbf conf\n");
			return -EINVAL;
		}

		if (!link_conf->he_support)
			return 0;
	}

	if (vif->type != NL80211_IFTYPE_STATION)
		return -EINVAL;

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, link_id, &def)))
		return -EINVAL;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	if (band == NL80211_BAND_2GHZ)
		cap_band = &ar->mac.iftype[NL80211_BAND_2GHZ][vif->type].he_cap;
	else
		cap_band = &ar->mac.iftype[NL80211_BAND_5GHZ][vif->type].he_cap;

	memcpy(&he_cap_elem, &cap_band->he_cap_elem, sizeof(he_cap_elem));

	*hemode = 0;
	if (HECAP_PHY_SUBFME_GET(he_cap_elem.phy_cap_info)) {
		if (HECAP_PHY_SUBFMR_GET(he_cap->he_cap_elem.phy_cap_info))
			*hemode |= u32_encode_bits(HE_SU_BFEE_ENABLE, HE_MODE_SU_TX_BFEE);
		if (HECAP_PHY_MUBFMR_GET(he_cap->he_cap_elem.phy_cap_info))
			*hemode |= u32_encode_bits(HE_MU_BFEE_ENABLE, HE_MODE_MU_TX_BFEE);
	}

	if (vif->type != NL80211_IFTYPE_MESH_POINT) {
		*hemode |= u32_encode_bits(HE_DL_MUOFDMA_ENABLE, HE_MODE_DL_OFDMA) |
			  u32_encode_bits(HE_UL_MUOFDMA_ENABLE, HE_MODE_UL_OFDMA);

		if (HECAP_PHY_ULMUMIMO_GET(he_cap_elem.phy_cap_info))
			if (HECAP_PHY_ULMUMIMO_GET(he_cap->he_cap_elem.phy_cap_info))
				*hemode |= u32_encode_bits(HE_UL_MUMIMO_ENABLE,
							  HE_MODE_UL_MUMIMO);

		if (u32_get_bits(*hemode, HE_MODE_MU_TX_BFEE))
			*hemode |= u32_encode_bits(HE_SU_BFEE_ENABLE, HE_MODE_SU_TX_BFEE);

		if (u32_get_bits(*hemode, HE_MODE_MU_TX_BFER))
			*hemode |= u32_encode_bits(HE_SU_BFER_ENABLE, HE_MODE_SU_TX_BFER);
	}
	return 0;
}

int ath12k_mac_set_eht_txbf_conf(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k *ar = arvif->ar;
	u32 param = WMI_VDEV_PARAM_SET_EHT_MU_MODE;
	u32 value = 0;
	int ret;
	struct ieee80211_bss_conf *link_conf;

	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!link_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf in eht txbf conf\n");
		return -ENOENT;
	}

	if (!link_conf->eht_support)
		return 0;

	if (link_conf->eht_su_beamformer) {
		value |= u32_encode_bits(EHT_SU_BFER_ENABLE, EHT_MODE_SU_TX_BFER);
		if (link_conf->eht_mu_beamformer &&
		    ahvif->vdev_type == WMI_VDEV_TYPE_AP)
			value |= u32_encode_bits(EHT_MU_BFER_ENABLE,
						 EHT_MODE_MU_TX_BFER) |
				 u32_encode_bits(EHT_DL_MUOFDMA_ENABLE,
						 EHT_MODE_DL_OFDMA_MUMIMO) |
				 u32_encode_bits(EHT_UL_MUOFDMA_ENABLE,
						 EHT_MODE_UL_OFDMA_MUMIMO);
	}
	if (ahvif->vif->type != NL80211_IFTYPE_MESH_POINT) {
		if (link_conf->eht_80mhz_full_bw_ul_mumimo)
			value |= u32_encode_bits(EHT_UL_MUMIMO_ENABLE, EHT_MODE_MUMIMO);
		if (link_conf->eht_su_beamformee)
			value |= u32_encode_bits(EHT_SU_BFEE_ENABLE, EHT_MODE_SU_TX_BFEE);
	}
	if (ar->eht_dl_enabled)
		value |= u32_encode_bits(EHT_DL_MUOFDMA_ENABLE, EHT_MODE_DL_OFDMA);
	if (ar->eht_ul_enabled)
		value |= u32_encode_bits(EHT_UL_MUOFDMA_ENABLE, EHT_MODE_UL_OFDMA);
	if (ar->eht_dlbf_enabled)
		value |= u32_encode_bits(EHT_DL_OFDMA_TXBF_ENABLE,
					EHT_MODE_DL_OFDMA_TXBF);

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		  "Set EHT TXBF config: DL=%d UL=%d DLBF=%d, value=0x%x\n",
		  ar->eht_dl_enabled, ar->eht_ul_enabled, ar->eht_dlbf_enabled, value);

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param, value);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set vdev %d EHT MU mode: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	return 0;
}

static u32 ath12k_mac_ieee80211_sta_bw_to_wmi(struct ath12k *ar,
					      struct ieee80211_link_sta *link_sta)
{
	u32 bw;

	switch (link_sta->bandwidth) {
	case IEEE80211_STA_RX_BW_20:
		bw = WMI_PEER_CHWIDTH_20MHZ;
		break;
	case IEEE80211_STA_RX_BW_40:
		bw = WMI_PEER_CHWIDTH_40MHZ;
		break;
	case IEEE80211_STA_RX_BW_80:
		bw = WMI_PEER_CHWIDTH_80MHZ;
		break;
	case IEEE80211_STA_RX_BW_160:
		bw = WMI_PEER_CHWIDTH_160MHZ;
		break;
	case IEEE80211_STA_RX_BW_320:
		bw = WMI_PEER_CHWIDTH_320MHZ;
		break;
	default:
		ath12k_warn(ar->ab, "Invalid bandwidth %d for link station %pM\n",
			    link_sta->bandwidth, link_sta->addr);
		bw = WMI_PEER_CHWIDTH_20MHZ;
		break;
	}

	return bw;
}

static struct
ieee80211_link_sta *ath12k_mac_inherit_radio_cap(struct ath12k *ar,
						 struct ath12k_link_sta *arsta)
{
	u8 link_id = arsta->link_id;
	struct ieee80211_link_sta *link_sta = NULL;
	struct ieee80211_sta *sta;
	u32 freq = ar->chan_info.low_freq;
	struct ieee80211_supported_band *sband;
	enum nl80211_band band;

	sta = container_of((void *)arsta->ahsta, struct ieee80211_sta, drv_priv);

	link_sta = (struct ieee80211_link_sta *)
		   kzalloc(sizeof(struct ieee80211_link_sta), GFP_ATOMIC);

	if (!link_sta)
		return NULL;

	memset(link_sta, 0, sizeof(*link_sta));

	band = ath12k_get_band_based_on_freq(freq);
	sband = &ar->mac.sbands[band];

	link_sta->sta = sta;
	ether_addr_copy(link_sta->addr, arsta->addr);
	link_sta->link_id = link_id;
	link_sta->smps_mode = IEEE80211_SMPS_AUTOMATIC;
	link_sta->supp_rates[band] = ieee80211_mandatory_rates(sband);
	link_sta->ht_cap = sband->ht_cap;
	link_sta->vht_cap = sband->vht_cap;
	link_sta->he_cap = sband->iftype_data->he_cap;
	link_sta->he_6ghz_capa = sband->iftype_data->he_6ghz_capa;
	link_sta->eht_cap = sband->iftype_data->eht_cap;

	link_sta->agg = sta->deflink.agg;
	link_sta->rx_nss = sta->deflink.rx_nss;

	link_sta->bandwidth = IEEE80211_STA_RX_BW_20;
	link_sta->sta_max_bandwidth = link_sta->bandwidth;
	link_sta->txpwr.type = NL80211_TX_POWER_AUTOMATIC;
	link_sta->punctured = 0;

	return link_sta;
}

void ath12k_bss_assoc(struct ath12k *ar,
			     struct ath12k_link_vif *arvif,
			     struct ieee80211_bss_conf *bss_conf)
{
	struct ath12k_vif *ahvif;
	struct ieee80211_vif *vif;
	struct ath12k_wmi_vdev_up_params params = {};
	struct ieee80211_link_sta *link_sta;
	u8 link_id;
	struct ath12k_link_sta *arsta;
	struct ieee80211_sta *ap_sta;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_sta_he_cap he_cap;
	struct ieee80211_sta_ht_cap ht_cap;
	struct ieee80211_he_6ghz_capa he_6ghz_cap;
	struct ath12k_hw_group *ag;
	bool is_auth = false;
	u32 hemode = 0, bandwidth;
	int ret;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);
	u16 bridge_bitmap;
	u8 bssid[ETH_ALEN], num_devices;
	bool is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	struct ath12k_wmi_peer_assoc_arg *peer_arg __free(kfree) =
					kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return;

	/* bss_conf shouldnt be NULL expect for bridge vdev */
	if (!arvif || (!bss_conf && !is_bridge_vdev))
		return;

	ahvif = arvif->ahvif;
	vif = ath12k_ahvif_to_vif(ahvif);

	if (is_bridge_vdev) {
		link_id = arvif->link_id;
		ether_addr_copy(bssid, arvif->bssid);
	} else {
		link_id = bss_conf->link_id;
		ether_addr_copy(bssid, bss_conf->bssid);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac vdev %i link id %u assoc bssid %pM aid %d\n",
		   arvif->vdev_id, link_id, bssid, ahvif->aid);

	rcu_read_lock();

	/* During ML connection, cfg.ap_addr has the MLD address. For
	 * non-ML connection, it has the BSSID.
	 */
	ap_sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);
	if (!ap_sta) {
		ath12k_warn(ar->ab, "failed to find station entry for bss %pM vdev %i\n",
			    vif->cfg.ap_addr, arvif->vdev_id);
		rcu_read_unlock();
		return;
	}

	ahsta = ath12k_sta_to_ahsta(ap_sta);

	arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				  ahsta->link[link_id]);
	if (!arsta) {
		if (is_bridge_vdev) {
			if (!ar || !ar->ab || !ar->ab->ag) {
				rcu_read_unlock();
				return;
			}

			ag = ar->ab->ag;
			num_devices = ag->num_devices - ag->num_bypassed;

			if (!ath12k_mac_is_bridge_required(ahsta->device_bitmap,
							   num_devices,
							   &bridge_bitmap)) {
				rcu_read_unlock();
				return;
			}
		}

		ath12k_warn(ar->ab, "arsta NULL link_id %d for sta %pM in bss assoc\n",
			    link_id, ap_sta->addr);
		WARN_ON(1);
		rcu_read_unlock();
		return;
	}

	link_sta = arsta->is_bridge_peer ? ath12k_mac_inherit_radio_cap(ar, arsta) :
		   ath12k_mac_get_link_sta(arsta);
	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in bss assoc\n");
		rcu_read_unlock();
		return;
	}

	he_6ghz_cap = link_sta->he_6ghz_capa;
	he_cap = link_sta->he_cap;
	ht_cap = link_sta->ht_cap;
	bandwidth = ath12k_mac_ieee80211_sta_bw_to_wmi(ar, link_sta);

	ath12k_peer_assoc_prepare(ar, arvif, arsta, peer_arg, false, link_sta);

	if (arsta->is_bridge_peer)
		kfree(link_sta);

	/* link_sta->he_cap must be protected by rcu_read_lock */
	ret = ath12k_mac_vif_recalc_sta_he_txbf(ar, arvif, &he_cap, &hemode);
	if (ret) {
		ath12k_warn(ar->ab, "failed to recalc he txbf for vdev %i on bss %pM: %d\n",
			    arvif->vdev_id, bssid, ret);
		rcu_read_unlock();
		return;
	}

        spin_lock_bh(&ar->data_lock);
        arsta->bw = bandwidth;
        spin_unlock_bh(&ar->data_lock);

	rcu_read_unlock();

	/* keep this before ath12k_wmi_send_peer_assoc_cmd() */
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_SET_HEMU_MODE, hemode);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit vdev param txbf 0x%x: %d\n",
			    hemode, ret);
		return;
	}

	peer_arg->is_assoc = true;
	ret = ath12k_wmi_send_peer_assoc_cmd(ar, peer_arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to run peer assoc for %pM vdev %i: %d\n",
			    bssid, arvif->vdev_id, ret);
		return;
	}

	if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
		ath12k_warn(ar->ab, "failed to get peer assoc conf event for %pM vdev %i\n",
			    bssid, arvif->vdev_id);
		return;
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, arvif->vdev_id, arsta->addr);
	if (peer && !peer->assoc_success) {
		ath12k_warn(ar->ab, "peer assoc failure in firmware %pM\n", arsta->addr);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}
	spin_unlock_bh(&dp->dp_lock);

	ret = ath12k_setup_peer_smps(ar, arvif, bssid,
				     &ht_cap, &he_cap, &he_6ghz_cap);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup peer SMPS for vdev %d: %d\n",
			    arvif->vdev_id, ret);
		return;
	}

	WARN_ON(arvif->is_up);

	ahvif->aid = vif->cfg.aid;
	if (!is_bridge_vdev)
		ether_addr_copy(arvif->bssid, bss_conf->bssid);

	params.vdev_id = arvif->vdev_id;
	params.aid = ahvif->aid;
	params.bssid = arvif->bssid;

	if (!is_bridge_vdev && bss_conf->nontransmitted) {
		params.nontx_profile_idx = bss_conf->bssid_index;
		params.nontx_profile_cnt = BIT(bss_conf->bssid_indicator) - 1;
		params.tx_bssid = bss_conf->transmitter_bssid;
	}

	if (ar->ab->ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0 &&
	    !ar->ab->is_reset)
	    /* Skip sending vdev up for non-asserted links while
	     * recovering station vif type
	     */
	     goto skip_vdev_up;

	ret = ath12k_wmi_vdev_up(ar, &params);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set vdev %d up: %d\n",
			    arvif->vdev_id, ret);
		return;
	}

skip_vdev_up:
	arvif->is_up = true;
	arvif->rekey_data.enable_offload = false;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac vdev %d up (associated) bssid %pM aid %d\n",
		   arvif->vdev_id, bssid, vif->cfg.aid);

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, arvif->vdev_id,
							    arvif->bssid);
	if (peer && peer->is_authorized)
		is_auth = true;

	spin_unlock_bh(&dp->dp_lock);

	/* Authorize BSS Peer */
	if (is_auth) {
		ret = ath12k_wmi_set_peer_param(ar, arvif->bssid,
						arvif->vdev_id,
						WMI_PEER_AUTHORIZE,
						1);
		if (ret)
			ath12k_warn(ar->ab, "Unable to authorize BSS peer: %d\n", ret);
	}

	if (!is_bridge_vdev) {
		ret = ath12k_wmi_send_obss_spr_cmd(ar, arvif->vdev_id,
						   &bss_conf->he_obss_pd);
		if (ret)
			ath12k_warn(ar->ab, "failed to set vdev %i OBSS PD parameters: %d\n",
				    arvif->vdev_id, ret);
	}

	if (test_bit(WMI_TLV_SERVICE_11D_OFFLOAD, ar->ab->wmi_ab.svc_map) &&
	    ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_NONE)
		ath12k_mac_11d_scan_stop_all(ar->ab);

}

void ath12k_bss_disassoc(struct ath12k *ar,
			 struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac vdev %i disassoc bssid %pM\n",
		   arvif->vdev_id, arvif->bssid);

	ret = ath12k_wmi_vdev_down(ar, arvif->vdev_id);
	if (ret)
		ath12k_warn(ar->ab, "failed to down vdev %i: %d\n",
			    arvif->vdev_id, ret);

	arvif->is_up = false;

	if (ath12k_mac_is_bridge_vdev(arvif))
		return;

	memset(&arvif->rekey_data, 0, sizeof(arvif->rekey_data));

	cancel_delayed_work(&ahvif->deflink.connection_loss_work);
}

static u32 ath12k_mac_get_rate_hw_value(int bitrate)
{
	u32 preamble;
	u16 hw_value;
	int rate;
	size_t i;

	if (ath12k_mac_bitrate_is_cck(bitrate))
		preamble = WMI_RATE_PREAMBLE_CCK;
	else
		preamble = WMI_RATE_PREAMBLE_OFDM;

	for (i = 0; i < ARRAY_SIZE(ath12k_legacy_rates); i++) {
		if (ath12k_legacy_rates[i].bitrate != bitrate)
			continue;

		hw_value = ath12k_legacy_rates[i].hw_value;
		rate = ATH12K_HW_RATE_CODE(hw_value, 0, preamble);

		return rate;
	}

	return -EINVAL;
}

static void ath12k_recalculate_mgmt_rate(struct ath12k *ar,
					 struct ath12k_link_vif *arvif,
					 struct cfg80211_chan_def *def)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	const struct ieee80211_supported_band *sband;
	struct ieee80211_bss_conf *bss_conf;
	enum nl80211_band band;
	u8 beacon_rate_idx;
	u8 basic_rate_idx;
	int hw_rate_code;
	u32 vdev_param;
	u16 bitrate;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf in mgmt rate calc for vif %pM link %u\n",
			    vif->addr, arvif->link_id);
		return;
	}

	sband = hw->wiphy->bands[def->chan->band];
	band = def->chan->band;
	basic_rate_idx = ffs(bss_conf->basic_rates);
	if (basic_rate_idx)
		basic_rate_idx -= 1;
	bitrate = sband->bitrates[basic_rate_idx].bitrate;

	hw_rate_code = ath12k_mac_get_rate_hw_value(bitrate);
	if (hw_rate_code < 0) {
		ath12k_warn(ar->ab, "bitrate not supported %d\n", bitrate);
		return;
	}

	vdev_param = WMI_VDEV_PARAM_MGMT_RATE;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, vdev_param,
					    hw_rate_code);
	if (ret)
		ath12k_warn(ar->ab, "failed to set mgmt tx rate %d\n", ret);
	if (bss_conf->beacon_tx_rate.control[band].legacy) {
		beacon_rate_idx = ffs(bss_conf->beacon_tx_rate.control[band].legacy);
		beacon_rate_idx -=1;

		if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ)
			beacon_rate_idx += ATH12K_MAC_FIRST_OFDM_RATE_IDX;
		if (beacon_rate_idx < ARRAY_SIZE(ath12k_legacy_rates)) {
			bitrate = ath12k_legacy_rates[beacon_rate_idx].bitrate;
			hw_rate_code = ath12k_mac_get_rate_hw_value(bitrate);

		}
	}

	vdev_param = WMI_VDEV_PARAM_BEACON_RATE;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, vdev_param,
					    hw_rate_code);
	if (ret)
		ath12k_warn(ar->ab, "failed to set beacon tx rate %d\n", ret);
}

static void ath12k_update_obss_color_notify_work(struct wiphy *wiphy,
						 struct wiphy_work *work)
{
	struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif,
					update_obss_color_notify_work);
	struct ath12k *ar;

	ar = arvif->ar;

	if (!ar)
		return;

	if (arvif->is_created)
		ieee80211_obss_color_collision_notify(arvif->ahvif->vif,
						       arvif->obss_color_bitmap,
						       GFP_KERNEL,
						       arvif->link_id);
	arvif->obss_color_bitmap = 0;
}

static void ath12k_mac_init_arvif(struct ath12k_vif *ahvif,
				  struct ath12k_link_vif *arvif, int link_id,
				  bool is_bridge_vdev)
{
	struct ath12k_hw *ah = ahvif->ah;
	u8 _link_id;
	int i;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (WARN_ON(!arvif))
		return;

	if (WARN_ON(link_id >= ATH12K_NUM_MAX_LINKS))
		return;

	if (link_id < 0)
		_link_id = 0;
	else
		_link_id = link_id;

	arvif->ahvif = ahvif;
	arvif->link_id = _link_id;

	/* Protects the datapath stats update on a per link basis */
	spin_lock_init(&arvif->link_stats_lock);

	INIT_LIST_HEAD(&arvif->list);
	arvif->key_cipher = INVALID_CIPHER;
	INIT_DELAYED_WORK(&ahvif->deflink.connection_loss_work,
			  ath12k_mac_vif_sta_connection_loss_work);
	if (!is_bridge_vdev) {
		wiphy_work_init(&arvif->update_obss_color_notify_work,
				ath12k_update_obss_color_notify_work);
		wiphy_work_init(&arvif->update_bcn_template_work,
				ath12k_update_bcn_template_work);
	}
	arvif->num_stations = 0;
	init_completion(&arvif->peer_ch_width_switch_send);
	wiphy_work_init(&arvif->peer_ch_width_switch_work,
		  ath12k_wmi_peer_chan_width_switch_work);
	wiphy_work_init(&arvif->update_bcn_tx_status_work,
			ath12k_update_bcn_tx_status_work);

	init_completion(&arvif->wmi_migration_event_resp);
	INIT_WORK(&arvif->wmi_migration_cmd_work,
		  ath12k_wmi_migration_cmd_work);
	INIT_LIST_HEAD(&arvif->peer_migrate_list);

	ath12k_mac_init_arvif_extn(ahvif);

	wiphy_work_init(&arvif->set_dscp_tid_work,
			ath12k_set_dscp_tid_work);

	for (i = 0; i < ARRAY_SIZE(arvif->bitrate_mask.control); i++) {
		arvif->bitrate_mask.control[i].legacy = 0xffffffff;
		arvif->bitrate_mask.control[i].gi = NL80211_TXRATE_DEFAULT_GI;
		memset(arvif->bitrate_mask.control[i].ht_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].ht_mcs));
		memset(arvif->bitrate_mask.control[i].vht_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].vht_mcs));
		memset(arvif->bitrate_mask.control[i].he_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].he_mcs));
		memset(arvif->bitrate_mask.control[i].eht_mcs, 0xff,
		       sizeof(arvif->bitrate_mask.control[i].eht_mcs));
	}

	/* Handle MLO related assignments */
	if (link_id >= 0) {
		rcu_assign_pointer(ahvif->link[arvif->link_id], arvif);
		ahvif->links_map |= BIT(_link_id);
	}

	ath12k_generic_dbg(ATH12K_DBG_MAC,
			   "mac init link arvif (link_id %d%s) for vif %pM. links_map 0x%x",
			   _link_id, (link_id < 0) ? " deflink" : "", ahvif->vif->addr,
			   ahvif->links_map);
}

void ath12k_mac_set_vendor_intf_detect(struct ath12k *ar, u8 intf_detect_bitmap)
{
	u8 prev_intf_bitmap, dcs_enable_bitmap;

	spin_lock_bh(&ar->data_lock);
	prev_intf_bitmap = ar->dcs_enable_bitmap & ATH12K_VENDOR_VALID_INTF_BITMAP;
	if (intf_detect_bitmap != prev_intf_bitmap) {
		ar->dcs_enable_bitmap &= ~ATH12K_VENDOR_VALID_INTF_BITMAP;
		ar->dcs_enable_bitmap |= intf_detect_bitmap;
		dcs_enable_bitmap = ar->dcs_enable_bitmap;
		spin_unlock_bh(&ar->data_lock);
		ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DCS,
					  dcs_enable_bitmap, ar->pdev->pdev_id);
		return;
	}
	spin_unlock_bh(&ar->data_lock);
}

void ath12k_mac_ap_ps_recalc(struct ath12k *ar)
{
	enum ath12k_ap_ps_state state = ATH12K_AP_PS_STATE_OFF;
	struct ath12k_link_vif *arvif, *tmp;
	bool allow_ap_ps = true;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	list_for_each_entry_safe(arvif, tmp, &ar->arvifs, list) {
		if (arvif->ahvif->vdev_type != WMI_VDEV_TYPE_AP &&
		    arvif->ahvif->vdev_type != WMI_VDEV_TYPE_MONITOR) {
			allow_ap_ps = false;
			break;
		}
	}

	if (ath12k_vendor_is_service_enabled(ATH12K_VENDOR_APP_ENERGY_SERVICE))
		allow_ap_ps = false;

	if (!allow_ap_ps)
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
				 "ap ps is not allowed\n");

	if (allow_ap_ps && !ar->num_stations && ar->ap_ps_enabled)
		state = ATH12K_AP_PS_STATE_ON;

	if (ar->ap_ps_state == state)
		return;

	ret = ath12k_wmi_pdev_ap_ps_cmd_send(ar, ar->pdev->pdev_id, state);
	if (!ret)
		ar->ap_ps_state = state;
	else
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
				"failed to send ap ps command pdev_id %u state %u\n",
				ar->pdev->pdev_id, state);
}

static void ath12k_mac_remove_link_interface(struct ieee80211_hw *hw,
					     struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = arvif->ar;
	int ret;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (!ar)
		return;

	if (arvif == &ahvif->deflink)
		cancel_delayed_work_sync(&ahvif->deflink.connection_loss_work);

	if (!ath12k_mac_is_bridge_vdev(arvif)) {
		wiphy_work_cancel(ah->hw->wiphy,
				  &arvif->update_obss_color_notify_work);
		wiphy_work_cancel(ah->hw->wiphy,
				  &arvif->update_bcn_template_work);
	}
	wiphy_work_cancel(ah->hw->wiphy,
			  &arvif->peer_ch_width_switch_work);
	wiphy_work_cancel(ah->hw->wiphy, &arvif->set_dscp_tid_work);
	cancel_work_sync(&arvif->wmi_migration_cmd_work);

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac remove link interface (vdev %d link id %d)",
		   arvif->vdev_id, arvif->link_id);

	if (test_bit(WMI_TLV_SERVICE_11D_OFFLOAD, ar->ab->wmi_ab.svc_map) &&
	    ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_NONE)
		ath12k_mac_11d_scan_stop(ar);

	ret = ath12k_spectral_vif_stop(arvif);
	if (ret)
		ath12k_warn(ar->ab, "failed to stop spectral for vdev %i: %d\n",
			    arvif->vdev_id, ret);

	if (ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
		ret = ath12k_peer_delete(ar, arvif->vdev_id, arvif->bssid, NULL);
		if (ret)
			ath12k_warn(ar->ab, "failed to submit AP self-peer removal on vdev %d link id %d: %d"
				    "num_peers: %d",
				    arvif->vdev_id, arvif->link_id, ret, ar->num_peers);

		ath12k_dp_peer_delete(&ah->dp_hw, arvif->bssid, NULL, ar->hw_link_id);
	}

	ath12k_debugfs_remove_interface(arvif);
	ath12k_mac_vdev_delete(ar, arvif);
	ath12k_mac_ap_ps_recalc(ar);
}

static struct ath12k_link_vif *
ath12k_mac_assign_link_vif(struct ath12k_hw *ah, struct ieee80211_vif *vif,
			   u8 link_id, bool is_bridge_vdev)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(ah->hw->wiphy);

	arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
	if (arvif) {
		arvif->link_id = link_id;
		ath12k_dbg(NULL, ATH12K_DBG_MAC | ATH12K_DBG_BOOT,
			   "mac assign link vif: arvif found, link_id:%d\n",
			   link_id);
		return arvif;
	}

	/* If this is the first link arvif being created for an ML VIF
	 * use the preallocated deflink memory except for scan arvifs
	 */
	if (!ahvif->links_map && link_id < ATH12K_DEFAULT_SCAN_LINK) {
		arvif = &ahvif->deflink;
	} else {
		arvif = (struct ath12k_link_vif *)
		kzalloc(sizeof(struct ath12k_link_vif), GFP_KERNEL);
		if (!arvif)
			return NULL;
	}

	ath12k_mac_init_arvif(ahvif, arvif, link_id, is_bridge_vdev);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* Initialize per link specific PPE data */
	arvif->ppe_vp_profile_idx = ATH12K_INVALID_VP_PROFILE_IDX;
#endif

	return arvif;
}

static void ath12k_mac_unassign_link_vif(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_hw *ah = ahvif->ah;

	lockdep_assert_wiphy(ah->hw->wiphy);

	ahvif->links_map &= ~BIT(arvif->link_id);
	rcu_assign_pointer(ahvif->link[arvif->link_id], NULL);
	synchronize_rcu();

	if (arvif != &ahvif->deflink)
		kfree(arvif);
	else
		memset(arvif, 0, sizeof(*arvif));
}

static void
ath12k_mac_remove_and_unassign_bridge_vdevs(struct ieee80211_hw *hw,
					    struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	unsigned long links;
	u8 link_id = ATH12K_BRIDGE_LINK_MIN;

	if (!hw || !vif) {
		ath12k_err(NULL,
			   "hw or vif NA for bridge vdevs removal\n");
		return;
	}

	if (vif->type != NL80211_IFTYPE_AP &&
	    vif->type != NL80211_IFTYPE_STATION)
		return;

	ahvif = (void *)vif->drv_priv;

	if (ath12k_erp_get_sm_state() == ATH12K_ERP_ENTER_COMPLETE &&
	    vif->type == NL80211_IFTYPE_AP) {
		/* During ERP, allow bridge vdev removal when only 1 vdev is active */
		if (hweight16(ahvif->links_map & ~BIT(IEEE80211_MLD_MAX_NUM_LINKS)) > 1)
			return;
	} else {
		/* Keep bridge vdevs until all vdevs are removed */
		if (hweight16(ahvif->links_map & ~BIT(IEEE80211_MLD_MAX_NUM_LINKS)) > 0)
			return;
	}

	links = ahvif->links_map;
	for_each_set_bit_from(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!arvif) {
			ath12k_err(NULL,
				   "arvif data NA on link id %d where links_map: %lu\n",
				   link_id, links);
			continue;
		}
		ath12k_mac_remove_link_interface(hw, arvif);
		ath12k_mac_unassign_link_vif(arvif);
	}
}

int
ath12k_mac_op_change_vif_links(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       u16 old_links, u16 new_links,
			       struct ieee80211_bss_conf *ol[IEEE80211_MLD_MAX_NUM_LINKS])
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	unsigned long to_remove = old_links & ~new_links;
	unsigned long to_add = ~old_links & new_links;
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_link_vif *arvif, *scan_arvif;
	struct ath12k *arvif_ar;
	int ret;
	u8 link_id;

	lockdep_assert_wiphy(hw->wiphy);

	scan_arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[0]);
	if (scan_arvif) {
		/* During ROC sta vif created and started to do tx/rx. this block
		 * does the cleanup and brings up vif for the given channel ctx if
		 * the selected arvif already started for ROC scan
		 */

		if (scan_arvif->is_scan_vif) {
			arvif_ar = scan_arvif->ar;
			if (WARN_ON(!arvif_ar))
				return -EINVAL;

			if (scan_arvif->is_started) {
				ret = ath12k_mac_vdev_stop(scan_arvif);
				if (ret) {
					ath12k_warn(arvif_ar->ab, "failed to stop scan vdev %d: %d\n",
						    scan_arvif->vdev_id, ret);
					return -EINVAL;
				}
				scan_arvif->is_started = false;
			}

			if (scan_arvif->is_created) {
				ath12k_mac_remove_link_interface(hw, scan_arvif);
				ath12k_mac_unassign_link_vif(scan_arvif);
			}
			scan_arvif->is_scan_vif = false;
		}
	}

	ath12k_generic_dbg(ATH12K_DBG_MAC,
			   "mac vif link changed for MLD %pM old_links 0x%x new_links 0x%x\n",
			   vif->addr, old_links, new_links);

	for_each_set_bit(link_id, &to_add, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		/* mac80211 wants to add link but driver already has the
		 * link. This should not happen ideally.
		 */
		if (arvif && arvif->ar && arvif->is_scan_vif == false &&
		    !test_bit(ATH12K_FLAG_RECOVERY, &arvif->ar->ab->dev_flags)) {
			WARN_ON(1);
			return -EINVAL;
		}

		arvif = ath12k_mac_assign_link_vif(ah, vif, link_id, false);
		if (WARN_ON(!arvif))
			return -EINVAL;
	}

	for_each_set_bit(link_id, &to_remove, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (WARN_ON(!arvif))
			return -EINVAL;

		if (arvif->is_scan_vif && arvif->is_started) {
			if (ath12k_mac_vdev_stop(arvif)) {
				ath12k_generic_dbg(ATH12K_DBG_MAC, "failed to stop vdev %d\n",
						   arvif->vdev_id);
				return -EINVAL;
			}
			arvif->is_started = false;
			arvif->is_scan_vif = false;
		}

		/* In case of SSR in progress arvif->is_created is explicitly
		 * marked as false to indicate vdev creation is not done on FW side,
		 * so any genuine interface down during this shouldn't leave stale
		 * entries hence check on both arvif->ar, arvif->is_created before
		 * calling ath12k_mac_unassign_link_vif as in case of SSR arvif->ar
		 * will be valid.
		 */

		if (!arvif->ar) {
			if (!arvif->is_created) {
				ath12k_mac_unassign_link_vif(arvif);
				continue;
			}
			WARN_ON(1);
			return -EINVAL;
		}

		ath12k_mac_remove_link_interface(hw, arvif);
		ath12k_mac_unassign_link_vif(arvif);
		ath12k_mac_remove_and_unassign_bridge_vdevs(hw, vif);
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_change_vif_links);

static int ath12k_mac_fils_discovery(struct ath12k_link_vif *arvif,
				     struct ieee80211_bss_conf *info)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ath12k *ar = arvif->ar;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	struct ieee80211_bss_conf *link_conf;
	struct sk_buff *tmpl;
	int ret;
	u32 interval;
	bool unsol_bcast_probe_resp_enabled = false;

	rcu_read_lock();
	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (link_conf && link_conf->nontransmitted) {
		rcu_read_unlock();
		return 0;
	}
	rcu_read_unlock();

	if (info->fils_discovery.max_interval) {
		interval = info->fils_discovery.max_interval;

		tmpl = ieee80211_get_fils_discovery_tmpl(hw, vif, info->link_id);
		if (tmpl)
			ret = ath12k_wmi_fils_discovery_tmpl(ar, arvif->vdev_id,
							     tmpl);
	} else if (info->unsol_bcast_probe_resp_interval) {
		unsol_bcast_probe_resp_enabled = 1;
		interval = info->unsol_bcast_probe_resp_interval;

		tmpl = ieee80211_get_unsol_bcast_probe_resp_tmpl(hw, vif, info->link_id);
		if (tmpl)
			ret = ath12k_wmi_probe_resp_tmpl(ar, arvif, tmpl);
	} else { /* Disable */
		return ath12k_wmi_fils_discovery(ar, arvif->vdev_id, 0, false);
	}

	if (!tmpl) {
		ath12k_warn(ar->ab,
			    "mac vdev %i failed to retrieve %s template\n",
			    arvif->vdev_id, (unsol_bcast_probe_resp_enabled ?
			    "unsolicited broadcast probe response" :
			    "FILS discovery"));
		return -EPERM;
	}
	kfree_skb(tmpl);

	if (!ret)
		ret = ath12k_wmi_fils_discovery(ar, arvif->vdev_id, interval,
						unsol_bcast_probe_resp_enabled);

	return ret;
}


static void ath12k_mac_non_srg_th_config(struct ath12k *ar,
					 struct ieee80211_he_obss_pd *he_obss_pd,
					 u32 *param_val)
{
	s8 non_srg_th = ATH12K_OBSS_PD_THRESHOLD_DISABLED;

	if (he_obss_pd->sr_ctrl &
	    IEEE80211_HE_SPR_NON_SRG_OBSS_PD_SR_DISALLOWED) {
		non_srg_th = ATH12K_OBSS_PD_MAX_THRESHOLD;
	} else {
		if (he_obss_pd->sr_ctrl &
		    IEEE80211_HE_SPR_NON_SRG_OFFSET_PRESENT)
			non_srg_th = (ATH12K_OBSS_PD_MAX_THRESHOLD +
				      he_obss_pd->non_srg_max_offset);

		*param_val |= ATH12K_OBSS_PD_NON_SRG_EN;
	}

	if (!test_bit(WMI_TLV_SERVICE_SRG_SRP_SPATIAL_REUSE_SUPPORT,
		      ar->ab->wmi_ab.svc_map)) {
		if (non_srg_th != ATH12K_OBSS_PD_THRESHOLD_DISABLED)
			non_srg_th -= ATH12K_DEFAULT_NOISE_FLOOR;
	}

	*param_val |= (non_srg_th & GENMASK(7, 0));
}

static void ath12k_mac_srg_th_config(struct ath12k *ar,
				     struct ieee80211_he_obss_pd *he_obss_pd,
				     u32 *param_val)
{
	s8 srg_th = 0;

	if (he_obss_pd->sr_ctrl & IEEE80211_HE_SPR_SRG_INFORMATION_PRESENT) {
		srg_th = ATH12K_OBSS_PD_MAX_THRESHOLD + he_obss_pd->max_offset;
		*param_val |= ATH12K_OBSS_PD_SRG_EN;
	}

	if (test_bit(WMI_TLV_SERVICE_SRG_SRP_SPATIAL_REUSE_SUPPORT,
		     ar->ab->wmi_ab.svc_map)) {
		*param_val |= ATH12K_OBSS_PD_THRESHOLD_IN_DBM;
		*param_val |= FIELD_PREP(GENMASK(15, 8), srg_th);
	} else {
		/* SRG not supported and threshold in dB */
		*param_val &= ~(ATH12K_OBSS_PD_SRG_EN |
				ATH12K_OBSS_PD_THRESHOLD_IN_DBM);
	}
}

static int ath12k_mac_config_obss_pd(struct ath12k *ar,
				     struct ieee80211_he_obss_pd *he_obss_pd)
{
	u32 bitmap[2], param_id, param_val, pdev_id;
	int ret;

	pdev_id = ar->pdev->pdev_id;

	/* Set and enable SRG/non-SRG OBSS PD Threshold */
	param_id = WMI_PDEV_PARAM_SET_CMD_OBSS_PD_THRESHOLD;

	/* TODO: Set threshold as 0 if monitor vdev is enabled */

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"OBSS PD Params: sr_ctrl %x non_srg_thres %u srg_max %u\n",
			he_obss_pd->sr_ctrl, he_obss_pd->non_srg_max_offset,
			he_obss_pd->max_offset);

	param_val = 0;

	/* Preparing non-SRG OBSS PD Threshold Configurations */
	ath12k_mac_non_srg_th_config(ar, he_obss_pd, &param_val);

	/* Preparing SRG OBSS PD Threshold Configurations */
	ath12k_mac_srg_th_config(ar, he_obss_pd, &param_val);

	ret = ath12k_wmi_pdev_set_param(ar, param_id, param_val, pdev_id);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set obss_pd_threshold for pdev: %u\n",
			    pdev_id);
		return ret;
	}

	/* Enable OBSS PD for all access category */
	param_id  = WMI_PDEV_PARAM_SET_CMD_OBSS_PD_PER_AC;
	param_val = 0xf;
	ret = ath12k_wmi_pdev_set_param(ar, param_id, param_val, pdev_id);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set obss_pd_per_ac for pdev: %u\n",
			    pdev_id);
		return ret;
	}

	/* Set SR Prohibit */
	param_id  = WMI_PDEV_PARAM_ENABLE_SR_PROHIBIT;
	param_val = !!(he_obss_pd->sr_ctrl &
		       IEEE80211_HE_SPR_HESIGA_SR_VAL15_ALLOWED);
	ret = ath12k_wmi_pdev_set_param(ar, param_id, param_val, pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to set sr_prohibit for pdev: %u\n",
			    pdev_id);
		return ret;
	}

	if (!test_bit(WMI_TLV_SERVICE_SRG_SRP_SPATIAL_REUSE_SUPPORT,
		      ar->ab->wmi_ab.svc_map))
		return 0;

	/* Set SRG BSS Color Bitmap */
	memcpy(bitmap, he_obss_pd->bss_color_bitmap, sizeof(bitmap));
	ret = ath12k_wmi_pdev_set_srg_bss_color_bitmap(ar, bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set bss_color_bitmap for pdev: %u\n",
			    pdev_id);
		return ret;
	}

	/* Set SRG Partial BSSID Bitmap */
	memcpy(bitmap, he_obss_pd->partial_bssid_bitmap, sizeof(bitmap));
	ret = ath12k_wmi_pdev_set_srg_patial_bssid_bitmap(ar, bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set partial_bssid_bitmap for pdev: %u\n",
			    pdev_id);
		return ret;
	}

	memset(bitmap, 0xff, sizeof(bitmap));

	/* Enable all BSS Colors for SRG */
	ret = ath12k_wmi_pdev_srg_obss_color_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set srg_color_en_bitmap pdev: %u\n",
			    pdev_id);
		return ret;
	}

	/* Enable all patial BSSID mask for SRG */
	ret = ath12k_wmi_pdev_srg_obss_bssid_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set srg_bssid_en_bitmap pdev: %u\n",
			    pdev_id);
		return ret;
	}

	/* Enable all BSS Colors for non-SRG */
	ret = ath12k_wmi_pdev_non_srg_obss_color_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set non_srg_color_en_bitmap pdev: %u\n",
			    pdev_id);
		return ret;
	}

	/* Enable all patial BSSID mask for non-SRG */
	ret = ath12k_wmi_pdev_non_srg_obss_bssid_enable_bitmap(ar, bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to set non_srg_bssid_en_bitmap pdev: %u\n",
			    pdev_id);
		return ret;
	}

	return 0;
}

int ath12k_mac_get_bridge_link_id_from_ahvif(struct ath12k_vif *ahvif,
					     u16 bridge_bitmap, u8 *link_id)
{
	int ret = -EINVAL;
	struct ath12k_link_vif *arvif;
	unsigned long links;
	struct ath12k_base *ab;
	struct ath12k *ar;

	*link_id = ATH12K_BRIDGE_LINK_MIN;

	links = ahvif->links_map;
	for_each_set_bit_from(*link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = ahvif->link[*link_id];
		if (!arvif)
			continue;

		ab = arvif->ar->ab;
		ar = arvif->ar;
		if (bridge_bitmap & BIT(ab->wsi_info.index)) {
			ath12k_dbg(ab, ATH12K_DBG_PEER,
				   "arvif found link_id %d for bridge_bitmap 0x%x\n",
				   *link_id, bridge_bitmap);
			ret = 0;
			break;
		}
	}

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	ath12k_ath_update_active_pdev_count(ar);
#endif

	return ret;
}

static bool ath12k_mac_is_bridge_required(u8 device_bitmap, u8 num_devices,
					  u16 *bridge_bitmap)
{
	bool bridge_needed = false;
	u8 adj_device[ATH12K_MAX_SOCS] = {0};
	u8 i, next, prev;
	u8 device_idx = 0;

	/* There is no need to check for bridging in-case of
	 * number of devices less than 3 and only one link is added.
	 */
	if (num_devices < ATH12K_MIN_NUM_DEVICES_NLINK ||
	    hweight8(device_bitmap) < 2) {
		return bridge_needed;
	}

	/* Consider given device_bitmap as circular bitmap of size num_devices.
	 * For every given index, If set - check either of adjacent indexes
	 * are set or not. else continue.
	 */
	for (i = 0; i < num_devices; i++) {
		if (!(device_bitmap & BIT(i)))
			continue;

		next = (i + 1) % num_devices;
		prev = ((i - 1) + num_devices) % num_devices;

		/* If both adjacent bits are not set, bridging is needed and
		 * device idx where bridge peer is required will be decided
		 * based on maximum count of adj_device array
		 */
		if (!(device_bitmap & BIT(next) || device_bitmap & BIT(prev))) {
			adj_device[prev]++;
			adj_device[next]++;
			bridge_needed = true;
			*bridge_bitmap |= BIT(prev);
			*bridge_bitmap |= BIT(next);
			if (adj_device[prev] > adj_device[next])
				device_idx = prev;
			else
				device_idx = next;
		}
	}
	if (num_devices > ATH12K_MIN_NUM_DEVICES_NLINK &&
	    hweight16(device_bitmap) == 2)
		*bridge_bitmap = BIT(device_idx);

	return bridge_needed;
}

static bool ath12k_mac_get_link_idx_with_device_idx(struct ath12k_hw *ah,
						    u32 device_idx,
						    u8 *link_idx)
{
	struct ath12k *ar = ah->radio;

	for (int i = 0; i < ah->num_radio; i++) {
		if (ar->ab->wsi_info.index == device_idx) {
			*link_idx = ar->hw_link_id;
			return true;
		}
		ar++;
	}
	return false;
}

static struct ath12k_link_vif *
ath12k_get_ttlm_preferred_link_to_start(struct ieee80211_vif *vif)
{
	u8 link_id;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k *ar;
	u16 valid_links = vif->valid_links;

	for (link_id = 0; link_id < ATH12K_NUM_MAX_LINKS; link_id++) {
		if (!(valid_links & BIT(link_id)))
			continue;

		arvif = rcu_dereference(ahvif->link[link_id]);
		WARN_ON(!arvif);

		if (!arvif->is_created)
			continue;

		ar = arvif->ar;
		/* Choose 6 GHz vap if present */
		if (ar->supports_6ghz)
			return arvif;
	}

	return arvif;
}

static int
ath12k_mac_populate_ttlm_params(struct ath12k_link_vif *arvif,
				struct ath12k_wmi_tid_to_link_map_ap_params *params)
{
	struct ath12k *ar;
	struct ieee80211_advertised_ttlm_config *ttlm_conf;
	u16 ieee_link_map_value = 0, hw_link_map_value = 0;
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	u16 valid_links = vif->valid_links;
	u8 i, j;

	ar = arvif->ar;
	ttlm_conf = &vif->adv_ttlm.u.ap.ttlm_config;
	params->pdev_id = ar->pdev->pdev_id;
	params->vdev_id = arvif->vdev_id;
	params->num_ttlm_info = ttlm_conf->num_ttlm_ie;
	params->hw_link_id = ar->hw_link_id;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"ttlm_conf->num_ttlm_ie %d vdev_id %d hw_link_id %d pdev_id %d\n",
			ttlm_conf->num_ttlm_ie, params->vdev_id,
			params->hw_link_id,
			params->pdev_id);

	for (i = 0; i < ttlm_conf->num_ttlm_ie; i++) {
		params->ie[i].ttlm.direction = ATH12K_WMI_TTLM_BIDI_DIRECTION;

		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
				"ttlm_conf->adv_ttlm_conf[%d].ieee_link_bmap = 0x%x\n",
				i, ttlm_conf->adv_ttlm_conf[i].ieee_link_bmap);
		if (!ttlm_conf->adv_ttlm_conf[i].ieee_link_bmap) {
			/* default mapping */
			params->ie[i].ttlm.default_link_mapping = true;
			continue;
		}

		params->ie[i].ttlm.mapping_switch_time =
			ttlm_conf->adv_ttlm_conf[i].switch_time;
		if (ttlm_conf->adv_ttlm_conf[i].switch_time)
			params->ie[i].ttlm.mapping_switch_time_present = 1;

		if (!ttlm_conf->adv_ttlm_conf[i].duration) {
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				   "Duration value 0, ignore set ttlm");
			return -1;
		}

		params->ie[i].ttlm.expected_duration_present = 1;
		params->ie[i].ttlm.expected_duration =
			ttlm_conf->adv_ttlm_conf[i].duration;

		params->ie[i].ttlm.link_mapping_size =
			ttlm_conf->adv_ttlm_conf[i].link_mapping_size;

		ieee_link_map_value = ttlm_conf->adv_ttlm_conf[i].ieee_link_bmap;
		ath12k_mac_get_hw_link_map(vif,
					   ieee_link_map_value,
					   &hw_link_map_value);

		if ((valid_links & ieee_link_map_value) != ieee_link_map_value) {
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				   "Invalid link_map value in set_ttlm");
			return -1;
		}

		for (j = 0; j < TTLM_MAX_NUM_TIDS; j++) {
			params->ie[i].ttlm.ieee_link_map_tid[j] = ieee_link_map_value;
			params->ie[i].ttlm.hw_link_map_tid[j] = hw_link_map_value;
		}

		params->ie[i].disabled_link_bitmap = ~ieee_link_map_value;
		params->ie[i].disabled_link_bitmap &= vif->valid_links;

		for (j = 0; j < TTLM_MAX_NUM_TIDS; j++) {
			if (params->ie[i].ttlm.ieee_link_map_tid[j] == vif->valid_links) {
				params->ie[i].ttlm.default_link_mapping = true;
			} else {
				params->ie[i].ttlm.default_link_mapping = false;
				break;
			}
		}

		if (params->ie[i].disabled_link_bitmap == vif->valid_links) {
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				   "Reject all links ttlm disabled case");
			return -1;
		}

		/* convert the disabled link bitmap to hw link bitmap for target usecase
		 * this is to removed when target resolves hw link id dependency
		 */
		ath12k_mac_get_hw_link_map(vif, params->ie[i].disabled_link_bitmap,
					   &hw_link_map_value);
		params->ie[i].disabled_link_bitmap = hw_link_map_value;

		if (params->ie[i].ttlm.default_link_mapping) {
			memset(&params->ie[i].ttlm.ieee_link_map_tid,
			       0,
			       sizeof(params->ie[i].ttlm.ieee_link_map_tid));
			memset(&params->ie[i].ttlm.hw_link_map_tid,
			       0,
			       sizeof(params->ie[i].ttlm.hw_link_map_tid));
		}
	}

	return 0;
}

static void
ath12k_mac_offload_advertised_ttlm(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif)
{
	/* prepare the params needed to send the wmi command */
	struct ath12k_wmi_tid_to_link_map_ap_params map_params = {0};
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;

	/* get 6g link from vif if present from active links */
	arvif = ath12k_get_ttlm_preferred_link_to_start(vif);
	if (!arvif)
		return;

	ar = arvif->ar;
	if (ath12k_mac_populate_ttlm_params(arvif, &map_params)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "Failed to populate advertised ttlm parameters\n");
		return;
	}

	ath12k_wmi_ap_tid_to_link_map_config(ar, &map_params);
}

static void ath12k_mac_bss_offload_advertised_ttlm(struct ath12k_link_vif *arvif)
{
	/* prepare the params needed to send the wmi command */
	struct ath12k_wmi_tid_to_link_map_ap_params map_params = {};
	struct ath12k *ar = arvif->ar;

	if (!arvif->is_created)
		return;

	if (ath12k_mac_populate_ttlm_params(arvif, &map_params)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "Failed to populate advertised ttlm parameters\n");
		return;
	}

	ath12k_wmi_ap_tid_to_link_map_config(ar, &map_params);
}

static void ath12k_mac_ttlm_timer_expiry(struct ieee80211_hw *hw,
					 struct ieee80211_vif *vif,
					 u16 map)
{
	struct ieee80211_sta *ap_sta;
	struct ieee80211_link_sta *link_sta;
	struct ath12k_wmi_ttlm_peer_params params = {0};
	struct ath12k *ar;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_wmi_host_ttlm_of_tids *ttlm_info;
	unsigned long links = ahvif->links_map;
	bool default_mapping = (map == vif->valid_links) ? 1 : 0;
	u16 hw_link_map = 0;
	u8 link_id, j;

	ap_sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);
	if (!ap_sta)
		return;

	lockdep_assert_wiphy(hw->wiphy);
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		memset(&params, 0, sizeof(struct ath12k_wmi_ttlm_peer_params));
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!arvif || !arvif->ar)
			continue;

		ar = arvif->ar;
		if (ath12k_mac_is_bridge_vdev(arvif))
			continue;

		params.pdev_id = ath12k_mac_get_target_pdev_id(ar);

		if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
			continue;

		link_sta = wiphy_dereference(hw->wiphy,
					     ap_sta->link[link_id]);
		if (!link_sta)
			continue;

		memcpy(params.peer_macaddr, link_sta->addr, ETH_ALEN);
		ttlm_info = &params.ttlm_info[params.num_dir];
		ttlm_info->direction = ATH12K_WMI_TTLM_BIDI_DIRECTION;
		ttlm_info->default_link_mapping = default_mapping;
		if (!default_mapping) {
			ath12k_mac_get_hw_link_map(vif, map, &hw_link_map);
			for (j = 0; j < TTLM_MAX_NUM_TIDS; j++)
				ttlm_info->ttlm_provisioned_links[j] =
					hw_link_map;
		}
		params.num_dir++;
		if (ath12k_wmi_send_mlo_peer_tid_to_link_map_cmd(ar,
								 &params,
								 true)) {
			ath12k_warn(ar->ab, "failed to send ttlm command");
			return;
		}
	}
}

void ath12k_mac_op_vif_cfg_changed(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   u64 changed)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	unsigned long links = ahvif->links_map;
	struct ieee80211_bss_conf *info;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	u8 link_id;

	lockdep_assert_wiphy(hw->wiphy);

	if (changed & BSS_CHANGED_SSID && vif->type == NL80211_IFTYPE_AP) {
		ahvif->u.ap.ssid_len = vif->cfg.ssid_len;
		if (vif->cfg.ssid_len)
			memcpy(ahvif->u.ap.ssid, vif->cfg.ssid, vif->cfg.ssid_len);
	}

	if (changed & BSS_CHANGED_ASSOC) {
		for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
			arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
			if (!arvif || !arvif->ar)
				continue;

			ar = arvif->ar;

			if (ath12k_mac_is_bridge_vdev(arvif)) {
				if (ath12k_hw_group_recovery_in_progress(ar->ab->ag))
					continue;
				info = NULL;
			} else {
				info = ath12k_mac_get_link_bss_conf(arvif);
			}

			if (vif->cfg.assoc)
				ath12k_bss_assoc(ar, arvif, info);
			else
				ath12k_bss_disassoc(ar, arvif);
		}
	}

	if (changed & BSS_CHANGED_MLD_ADV_TTLM) {
		if (vif->type == NL80211_IFTYPE_AP) {
			/* advertised ttlm offload start request */
			ath12k_mac_offload_advertised_ttlm(hw, vif);
		} else if (vif->cfg.assoc) {
			ath12k_mac_ttlm_timer_expiry(hw, vif,
						     vif->adv_ttlm.u.mgd.ttlm_info.map);
		}
	}
}
EXPORT_SYMBOL(ath12k_mac_op_vif_cfg_changed);

static void ath12k_mac_vif_setup_ps(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct ieee80211_conf *conf = &ath12k_ar_to_hw(ar)->conf;
	enum wmi_sta_powersave_param param;
	struct ieee80211_bss_conf *info;
	enum wmi_sta_ps_mode psmode;
	int ret;
	int timeout;
	bool enable_ps;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (vif->type != NL80211_IFTYPE_STATION)
		return;

	enable_ps = arvif->ahvif->ps;
	if (enable_ps) {
		psmode = WMI_STA_PS_MODE_ENABLED;
		param = WMI_STA_PS_PARAM_INACTIVITY_TIME;

		timeout = conf->dynamic_ps_timeout;
		if (timeout == 0) {
			info = ath12k_mac_get_link_bss_conf(arvif);
			if (!info) {
				ath12k_warn(ar->ab, "unable to access bss link conf in setup ps for vif %pM link %u\n",
					    vif->addr, arvif->link_id);
				return;
			}

			/* firmware doesn't like 0 */
			timeout = ieee80211_tu_to_usec(info->beacon_int) / 1000;
		}

		ret = ath12k_wmi_set_sta_ps_param(ar, arvif->vdev_id, param,
						  timeout);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set inactivity time for vdev %d: %i\n",
				    arvif->vdev_id, ret);
			return;
		}
	} else {
		psmode = WMI_STA_PS_MODE_DISABLED;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac vdev %d psmode %s\n",
		   arvif->vdev_id, psmode ? "enable" : "disable");

	ret = ath12k_wmi_pdev_set_ps_mode(ar, arvif->vdev_id, psmode);
	if (ret)
		ath12k_warn(ar->ab, "failed to set sta power save mode %d for vdev %d: %d\n",
			    psmode, arvif->vdev_id, ret);
}

static void ath12k_mac_bridge_vdevs_down(struct ieee80211_hw *hw,
					 struct ath12k_vif *ahvif, u8 cur_link_id)
{
	struct ath12k_link_vif *arvif;
	unsigned long links, scan_links;
	int ret;
	u8 link_id;
	unsigned int num_vdev;

	/* Proceed only for MLO */
	if (!ahvif->vif->valid_links)
		return;

	links = ahvif->links_map;
	scan_links = ATH12K_SCAN_LINKS_MASK;
	num_vdev = hweight16(ahvif->links_map & ~BIT(IEEE80211_MLD_MAX_NUM_LINKS));

	for_each_andnot_bit(link_id, &links, &scan_links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!arvif) {
			ath12k_err(NULL,
				   "unable to determine the assigned link on link id %u\n",
				   link_id);
			continue;
		}
		/* Proceed bridge vdev down only after all the normal vdevs are down */
		if (link_id < IEEE80211_MLD_MAX_NUM_LINKS) {
			if (ath12k_erp_get_sm_state() == ATH12K_ERP_ENTER_COMPLETE &&
			    ahvif->vif->type == NL80211_IFTYPE_AP) {
				if (num_vdev > ATH12K_ERP_BRIDGE_VDEV_REMOVAL_THRESHOLD)
					return;
			} else {
				if (arvif->is_up)
					return;
			}

			continue;
		}

		if (arvif->is_up) {
			ret = ath12k_wmi_vdev_down(arvif->ar, arvif->vdev_id);
			if (ret) {
				ath12k_warn(arvif->ar->ab, "failed to down bridge vdev_id %i with link_id=%d: %d\n",
					    arvif->vdev_id, arvif->link_id, ret);
				continue;
			}
			arvif->is_up = false;
			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_MAC,
				   "mac bridge vdev %d down with link_id=%u\n",
				   arvif->vdev_id, arvif->link_id);
		}
	}
}

static void ath12k_mac_bridge_vdevs_up(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_wmi_vdev_up_params params = { 0 };
	int ret;
	u8 link_id = ATH12K_BRIDGE_LINK_MIN;
	unsigned long links;

	/* Proceed only for MLO */
	if (!ahvif->vif->valid_links)
		return;

	links = ahvif->links_map;
	/* Do we need to have the ahvif->links_map ~15th bit ==2 check here ?*/
	for_each_set_bit_from(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = ahvif->link[link_id];
		if (arvif->is_started) {
			if (arvif->is_up)
				continue;
			memset(&params, 0, sizeof(params));
			params.vdev_id = arvif->vdev_id;
			params.aid = ahvif->aid;
			params.bssid = arvif->bssid;
			ret = ath12k_wmi_vdev_up(arvif->ar, &params);
			if (ret) {
				ath12k_warn(arvif->ar->ab, "failed to bring bridge vdev up %d with link_id %d and failure %d\n",
					    arvif->vdev_id, link_id, ret);
			} else {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_MAC, "mac bridge vdev %d link_id %d up\n",
					   arvif->vdev_id, link_id);
				arvif->is_up = true;
			}
		}
	}
}

void ath12k_mac_bridge_vdev_up(struct ath12k_link_vif *arvif)
{
	struct ath12k_wmi_vdev_up_params params = { 0 };
	int ret;

	if (arvif->is_up)
		return;

	params.vdev_id = arvif->vdev_id;
	params.aid = arvif->ahvif->aid;
	params.bssid = arvif->bssid;
	ret = ath12k_wmi_vdev_up(arvif->ar, &params);
	if (ret)
		ath12k_warn(arvif->ar->ab,
			    "failed to bring bridge vdev up %d with link_id %d and failure %d\n",
			    arvif->vdev_id, arvif->link_id, ret);
	else
		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_MAC,
			   "mac bridge vdev %d link_id %d up\n",
			   arvif->vdev_id, arvif->link_id);
	arvif->is_up = true;
}

static void ath12k_mac_send_pwr_mode_update(struct ath12k *ar,
					    struct wireless_dev *wdev,
					    u8 link_id)
{
	ath12k_vendor_send_6ghz_power_mode_update_complete(ar, wdev, link_id);
}

/**
 * pdbm1, pdbm2 and pdbm3 - Array of dbr values for puncture mask type
 * PUNCTURE_TYPE_EDGE, PUNCTURE_TYPE_INTERIM_20_PLUS and
 * PUNCTURE_TYPE_INTERIM_20 respectively.
 */
static const s16 pdbm1[3] = {0, -200, -280};
static const s16 pdbm2[3] = {0, -200, -250};
static const s16 pdbm3[3] = {0, -200, -230};

/**
 * handle_edge_puncture - Populate puncture mask values for edge puncture type
 * @edge_punct_ctx: Pointer to the puncture context structure containing
 * edge mask pointers, edge offset values, and dB reduction values.
 *
 * This function sets the offset and dB reduction (dbr) values in the left and
 * right edge puncture mask structures for the PUNCTURE_TYPE_EDGE case. It uses
 * the provided edge offsets and a predefined dB mask array (typically pdbm1) to
 * define the regulatory mask shape on both sides of the punctured region.
 *
 * The mask is symmetric and ensures a smooth transition from the edge of the
 * punctured region to the adjacent usable spectrum.
 */
void
handle_edge_puncture(struct ath12k_puncture_ctx *edge_punct_ctx)
{
	struct ath12k_punct_mask *pu_mask_l_edge, *pu_mask_r_edge;
	s16 pu_l_edge, pu_r_edge;
	const s16 *pdbm1;

	pu_mask_l_edge = edge_punct_ctx->masks.l_edge;
	pu_mask_r_edge = edge_punct_ctx->masks.r_edge;
	pu_l_edge = edge_punct_ctx->edges.pu_l_edge;
	pu_r_edge = edge_punct_ctx->edges.pu_r_edge;
	pdbm1 = edge_punct_ctx->pdbms.pdbm1;

	ath12k_dbg(NULL, ATH12K_DBG_MAC, "EDGE puncture\n");
	pu_mask_l_edge->offset[0] = pu_l_edge - ((pu_r_edge - pu_l_edge) / 2);
	pu_mask_l_edge->dbr[0] = pdbm1[2];

	pu_mask_l_edge->offset[1] = pu_l_edge - ATH12K_PUNCTURE_OFFSET_STEP;
	pu_mask_l_edge->dbr[1] = pdbm1[1];

	pu_mask_l_edge->offset[2] = pu_l_edge;
	pu_mask_l_edge->dbr[2] = pdbm1[0];

	pu_mask_r_edge->offset[0] = pu_r_edge;
	pu_mask_r_edge->dbr[0] = pdbm1[0];

	pu_mask_r_edge->offset[1] = pu_r_edge + ATH12K_PUNCTURE_OFFSET_STEP;
	pu_mask_r_edge->dbr[1] = pdbm1[1];

	pu_mask_r_edge->offset[2] = pu_r_edge + ((pu_r_edge - pu_l_edge) / 2);
	pu_mask_r_edge->dbr[2] = pdbm1[2];
}

/**
 * handle_interim_20_plus - Populate puncture mask values for INTERIM_20_PLUS type
 * @interim_20_plus_punct_ctx: Pointer to the puncture context structure containing
 * edge and interim mask pointers, offset values, and dB reduction arrays.
 *
 * This function sets the offset and dB reduction (dbr) values in the puncture
 * mask structures for the PUNCTURE_TYPE_INTERIM_20_PLUS case. It handles both
 * edge and interim puncture shaping, ensuring smooth transitions in the
 * regulatory mask across the punctured and adjacent usable spectrum.
 *
 * The function uses predefined dB masks (pdbm1 and pdbm2) to shape the
 * attenuation profile for both edge and interim regions.
 */
void
handle_interim_20_plus(struct ath12k_puncture_ctx *interim_20_plus_punct_ctx)
{
	struct ath12k_punct_mask *pu_mask_l_edge, *pu_mask_r_edge;
	struct ath12k_punct_mask *pu_mask_l, *pu_mask_r;
	s16 pu_l_edge, pu_r_edge;
	s16 l_edge, r_edge;
	s16 pu_edge1, pu_edge2;
	const s16 *pdbm1, *pdbm2;

	pu_mask_l_edge = interim_20_plus_punct_ctx->masks.l_edge;
	pu_mask_r_edge = interim_20_plus_punct_ctx->masks.r_edge;
	pu_mask_l = interim_20_plus_punct_ctx->masks.l;
	pu_mask_r = interim_20_plus_punct_ctx->masks.r;
	pu_l_edge = interim_20_plus_punct_ctx->edges.pu_l_edge;
	pu_r_edge = interim_20_plus_punct_ctx->edges.pu_r_edge;
	l_edge = interim_20_plus_punct_ctx->edges.l_edge;
	r_edge = interim_20_plus_punct_ctx->edges.r_edge;
	pu_edge1 = interim_20_plus_punct_ctx->edges.pu_edge1;
	pu_edge2 = interim_20_plus_punct_ctx->edges.pu_edge2;
	pdbm1 = interim_20_plus_punct_ctx->pdbms.pdbm1;
	pdbm2 = interim_20_plus_punct_ctx->pdbms.pdbm2;

	/* type 2 mask */
	ath12k_dbg(NULL, ATH12K_DBG_MAC, "INTERIM 20 PLUS puncture\n");
	/* Edge concurrent puncture */
	if (l_edge != pu_l_edge || r_edge != pu_r_edge) {
		pu_mask_l_edge->offset[0] = pu_l_edge - ((pu_edge1 - pu_l_edge) / 2);
		pu_mask_l_edge->dbr[0] = pdbm1[2];

		pu_mask_l_edge->offset[1] = pu_l_edge - ATH12K_PUNCTURE_OFFSET_STEP;
		pu_mask_l_edge->dbr[1] = pdbm1[1];

		pu_mask_l_edge->offset[2] = pu_l_edge;
		pu_mask_l_edge->dbr[2] = pdbm1[0];

		pu_mask_r_edge->offset[0] = pu_r_edge;
		pu_mask_r_edge->dbr[0] = pdbm1[0];

		pu_mask_r_edge->offset[1] = pu_r_edge + ATH12K_PUNCTURE_OFFSET_STEP;
		pu_mask_r_edge->dbr[1] = pdbm1[1];

		pu_mask_r_edge->offset[2] = pu_r_edge + ((pu_r_edge - pu_edge2) / 2);
		pu_mask_r_edge->dbr[2] = pdbm1[2];
	}

	pu_mask_l->offset[0] = pu_edge1;
	pu_mask_l->dbr[0] = pdbm2[0];

	pu_mask_l->offset[1] = pu_edge1 + ATH12K_PUNCTURE_OFFSET_STEP;
	pu_mask_l->dbr[1] = pdbm2[1];

	pu_mask_l->offset[2] = pu_edge1 + ((pu_edge1 - pu_l_edge) >> 1);
	pu_mask_l->dbr[2] = pdbm2[2];

	pu_mask_r->offset[0] = pu_edge2 - ((pu_r_edge - pu_edge2) >> 1);
	pu_mask_r->dbr[0] = pdbm2[2];

	pu_mask_r->offset[1] = pu_edge2 - ATH12K_PUNCTURE_OFFSET_STEP;
	pu_mask_r->dbr[1] = pdbm2[1];

	pu_mask_r->offset[2] = pu_edge2;
	pu_mask_r->dbr[2] = pdbm2[0];
}

/**
 * handle_interim_20 - Populate puncture mask values for INTERIM_20 type
 * @interim_20_punct_ctx: Pointer to the puncture context structure containing
 * interim mask pointers, offset values, and dB reduction array.
 *
 * This function sets the offset and dB reduction (dbr) values in the left and
 * right interim puncture mask structures for the PUNCTURE_TYPE_INTERIM_20 case.
 * It defines a symmetric attenuation profile across the punctured region using
 * the provided dB mask array (typically pdbm3).
 *
 * The mask ensures a smooth regulatory transition across the 20 MHz interim
 * puncture region, helping to meet spectral emission constraints.
 */
void
handle_interim_20(struct ath12k_puncture_ctx *interim_20_punct_ctx)
{
	struct ath12k_punct_mask *pu_mask_l, *pu_mask_r;
	s16 pu_edge1, pu_edge2;
	const s16 *pdbm3;

	pu_mask_l = interim_20_punct_ctx->masks.l;
	pu_mask_r = interim_20_punct_ctx->masks.r;
	pu_edge1 = interim_20_punct_ctx->edges.pu_edge1;
	pu_edge2 = interim_20_punct_ctx->edges.pu_edge2;
	pdbm3 = interim_20_punct_ctx->pdbms.pdbm3;

	ath12k_dbg(NULL, ATH12K_DBG_MAC, "INTERIM 20 puncture\n");
	pu_mask_l->offset[0] = pu_edge1;
	pu_mask_l->dbr[0] = pdbm3[0];

	pu_mask_l->offset[1] = pu_edge1 + ATH12K_PUNCTURE_OFFSET_STEP;
	pu_mask_l->dbr[1] = pdbm3[1];

	pu_mask_l->offset[2] = pu_edge1 + ATH12K_PUNCTURE_MASK_WIDTH;
	pu_mask_l->dbr[2] = pdbm3[2];

	pu_mask_r->offset[0] = pu_edge2 - ATH12K_PUNCTURE_MASK_WIDTH;
	pu_mask_r->dbr[0] = pdbm3[2];

	pu_mask_r->offset[1] = pu_edge2 - ATH12K_PUNCTURE_OFFSET_STEP;
	pu_mask_r->dbr[1] = pdbm3[1];

	pu_mask_r->offset[2] = pu_edge2;
	pu_mask_r->dbr[2] = pdbm3[0];
}

/**
 * get_y_val - Calculate the interpolated y-value for a given x-value
 * @x1: First x-coordinate
 * @x2: Second x-coordinate
 * @y1: y-coordinate corresponding to x1
 * @y2: y-coordinate corresponding to x2
 * @x: x-coordinate for which the interpolated y-value is to be calculated
 *
 * This function calculates the interpolated y-value for a given x-value using
 * linear interpolation between two points (x1, y1) and (x2, y2). The function
 * returns the interpolated y-value based on the input x-coordinate.
 *
 * Return: The interpolated y-value for the given x-coordinate.
 */
static s16 get_y_val(s16 x1, s16 x2, s16 y1, s16 y2, s16 x)
{
	s16 den = x2 - x1;

	if (!den) {
		ath12k_err(NULL,
			   "Invalid x coordinates x1=%d y1=%d x2=%d y2=%d\n",
			   x1, y1, x2, y2);
		return ATH12K_INVALID_DBR;
	}

	return (y1 + (x - x1) * (y2 - y1) / (x2 - x1));
}

/**
 * get_regmask_puncture - Calculate the regulatory mask for punctured channels
 * @offset: Offset value for the frequency
 * @bw: Bandwidth of the channel
 * @pu_mask: Pointer to the punct_mask structure containing puncture mask limits
 *
 * This function calculates the regulatory mask for punctured channels based
 * on the given offset, bandwidth, and puncture mask limits. The mask value is
 * determined by the offset relative to the puncture mask limits defined in the
 * punct_mask structure.
 *
 * Return: The calculated regulatory mask value, or ATH12K_INVALID_DBR if the offset
 * does not fall within the defined puncture mask limits.
 */
static
s16 get_reg_mask_puncture(s16 offset, u16 bw, struct ath12k_punct_mask *pu_mask)
{
	s16 mask;

	offset *= 10;
	if (offset <= pu_mask->offset[0]) {
		mask = pu_mask->dbr[0];
	} else if ((offset > pu_mask->offset[0]) && (offset < pu_mask->offset[1])) {
		mask = get_y_val(pu_mask->offset[0], pu_mask->offset[1],
				 pu_mask->dbr[0], pu_mask->dbr[1], offset);
	} else if (offset == pu_mask->offset[1]) {
		mask = pu_mask->dbr[1];
	} else if ((offset > pu_mask->offset[1]) && (offset < pu_mask->offset[2])) {
		mask = get_y_val(pu_mask->offset[1], pu_mask->offset[2],
				 pu_mask->dbr[1], pu_mask->dbr[2], offset);
	} else if (offset >= pu_mask->offset[2]) {
		mask = pu_mask->dbr[2];
	} else {
		ath12k_err(NULL, "invalid offset %d. Offset range: [%d, %d, %d]",
			   offset, pu_mask->offset[0], pu_mask->offset[1],
			   pu_mask->offset[2]);
		mask = ATH12K_INVALID_DBR;
	}

	return mask;
}

/**
 * get_reg_mask_non_puncture - Calculate the regulatory mask for non-punctured
 * channels.
 * @offset: Offset value for the frequency
 * @bw: Bandwidth of the channel
 *
 * This function calculates the regulatory mask for non-punctured channels based
 * on the given offset and bandwidth. The mask value is determined by the offset
 * relative to the bandwidth and predefined thresholds.
 *
 * Return: The calculated regulatory mask value.
 */
static s16 get_reg_mask_non_puncture(s16 offset, u16 bw)
{
	u16 hbw = bw / 2;
	s16 mask;

	offset = abs(offset);

	if (offset >= ((bw * 3) / 2))
		mask = ATH12K_REG_MASK_DB_MIN;
	else if (offset >= bw)
		mask = ATH12K_REG_MASK_DB_MID -
			((ATH12K_REG_MASK_DB_STEP_MID * (offset - bw)) / hbw);
	else if (offset >= (hbw + ATH12K_REG_MASK_OFFSET_THRESHOLD))
		mask = ATH12K_REG_MASK_DB_BASE -
			((ATH12K_REG_MASK_DB_STEP_BASE *
			  (offset - (hbw + ATH12K_REG_MASK_OFFSET_THRESHOLD))) /
			 (hbw - ATH12K_REG_MASK_OFFSET_THRESHOLD));
	else
		mask = ATH12K_REG_MASK_DB_NONE;

	return mask;
}

/**
 * is_punc_type_invalid - Check if a puncture type is invalid
 * @punc_type: The puncture type to validate
 *
 * This function checks whether the given puncture type falls outside
 * the valid range defined by the enumeration constants
 * PUNCTURE_TYPE_FIRST and PUNCTURE_TYPE_LAST.
 *
 * Return: true if the puncture type is invalid, false otherwise.
 */
static inline bool
is_punc_type_invalid(enum ath12k_puncture_type punc_type)
{
	return (punc_type < ATH12K_PUNCTURE_TYPE_FIRST) ||
	       (punc_type > ATH12K_PUNCTURE_TYPE_LAST);
}

/**
 * get_pmask_limits - Determine the puncture mask limits for a given bandwidth
 * and puncture bitmap
 * @bw: Bandwidth for which the puncture mask limits are to be determined
 * @puncture_bitmap: Bitmap indicating the punctured sub-channels
 * @pu_mask_l_edge: Pointer to the left edge puncture mask structure
 * @pu_mask_l: Pointer to the left interim puncture mask structure
 * @pu_mask_r: Pointer to the right interim puncture mask structure
 * @pu_mask_r_edge: Pointer to the right edge puncture mask structure
 *
 * This function calculates the puncture mask limits for a given bandwidth and
 * puncture bitmap. It determines the type of puncture (edge, interim 20 MHz,
 * interim 20 MHz plus, or invalid) and sets the appropriate offset and dbr
 * values in the provided pmask structures.
 *
 * Return: The type of puncture determined (enum puncture_type).
 */
static enum ath12k_puncture_type
get_puncture_type_and_masks(u16 bw, u16 puncture_bitmap,
			    struct ath12k_punct_mask *pu_mask_l_edge,
			    struct ath12k_punct_mask *pu_mask_l,
			    struct ath12k_punct_mask *pu_mask_r,
			    struct ath12k_punct_mask *pu_mask_r_edge)
{
	u16 punc_mask;
	u16 pp;
	s16 i;
	s16 num_valid_bits;
	s16 l_edge;
	s16 r_edge;
	s16 pu_l_edge;
	s16 pu_r_edge;
	s16 pu_edge1;
	s16 pu_edge2;
	s16 punc_start;
	enum ath12k_puncture_type punc_type;
	struct ath12k_puncture_ctx punct_ctx;

	switch (bw) {
	case 80:
		punc_mask = ATH12K_PUNCTURE_80MHZ_MASK;
		num_valid_bits = 4;
		break;
	case 160:
		punc_mask = ATH12K_PUNCTURE_160MHZ_MASK;
		num_valid_bits = 8;
		break;
	case 320:
		punc_mask = ATH12K_PUNCTURE_320MHZ_MASK;
		num_valid_bits = 16;
		break;
	default:
		punc_mask = 0;
		num_valid_bits = 0;
		ath12k_dbg(NULL, ATH12K_DBG_MAC, "Bandwidth input invalid");
		return ATH12K_PUNCTURE_TYPE_INVALID;
	}

	pp = puncture_bitmap & punc_mask;
	if (!pp)
		return ATH12K_PUNCTURE_TYPE_INVALID;

	pu_l_edge = ATH12K_INVALID_EDGE;
	pu_r_edge = ATH12K_INVALID_EDGE;
	pu_edge1 = ATH12K_INVALID_EDGE;
	pu_edge2 = ATH12K_INVALID_EDGE;
	punc_start = ATH12K_INVALID_EDGE;
	l_edge        = -(bw / 2);
	r_edge        =   bw / 2;

	for (i = 0; i < num_valid_bits; i++) {
		if (!((1 << i) & pp) && pu_l_edge == ATH12K_INVALID_EDGE)
			pu_l_edge = l_edge + (i * 20);

		if (!((1 << (num_valid_bits - 1 - i)) & pp) &&
		    pu_r_edge == ATH12K_INVALID_EDGE)
			pu_r_edge = r_edge - (i * 20);

		if (punc_start != ATH12K_INVALID_EDGE && pu_edge1 == ATH12K_INVALID_EDGE &&
		    !((1 << i) & pp)) {
			/* End of interim puncture */
			pu_edge1 = punc_start;
			pu_edge2 = l_edge + (i * 20);
			punc_start = ATH12K_INVALID_EDGE;
		}

		if (((1 << i) & pp) && punc_start == ATH12K_INVALID_EDGE &&
		    ((l_edge + (i * 20)) > pu_l_edge))
			/* Start of interim puncture */
			punc_start = l_edge + (i * 20);
	}

	/* Find the puncture type */
	if (pu_edge1 == ATH12K_INVALID_EDGE && (l_edge != pu_l_edge || r_edge != pu_r_edge))
		punc_type = ATH12K_PUNCTURE_TYPE_EDGE;
	else if ((pu_edge2 - pu_edge1) >= 40)
		punc_type = ATH12K_PUNCTURE_TYPE_INTERIM_20_PLUS;
	else if ((pu_edge2 - pu_edge1) == 20)
		punc_type = ATH12K_PUNCTURE_TYPE_INTERIM_20;
	else
		punc_type = ATH12K_PUNCTURE_TYPE_INVALID;

	pu_l_edge *= 10;
	pu_r_edge *= 10;
	pu_edge1 *= 10;
	pu_edge2 *= 10;
	l_edge *= 10;
	r_edge *= 10;

	punct_ctx.masks.l_edge = pu_mask_l_edge;
	punct_ctx.masks.r_edge = pu_mask_l_edge;
	punct_ctx.masks.l = pu_mask_l;
	punct_ctx.masks.r = pu_mask_r;
	punct_ctx.edges.pu_l_edge = pu_l_edge;
	punct_ctx.edges.pu_r_edge = pu_r_edge;
	punct_ctx.edges.l_edge = l_edge;
	punct_ctx.edges.r_edge = r_edge;
	punct_ctx.edges.pu_edge1 = pu_edge1;
	punct_ctx.edges.pu_edge2 = pu_edge2;
	punct_ctx.pdbms.pdbm1 = pdbm1;
	punct_ctx.pdbms.pdbm2 = pdbm2;
	punct_ctx.pdbms.pdbm3 = pdbm3;
	switch (punc_type) {
	case ATH12K_PUNCTURE_TYPE_EDGE:
		handle_edge_puncture(&punct_ctx);
		break;
	case ATH12K_PUNCTURE_TYPE_INTERIM_20_PLUS:
		handle_interim_20_plus(&punct_ctx);
		break;
	case ATH12K_PUNCTURE_TYPE_INTERIM_20:
		handle_interim_20(&punct_ctx);
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "Investigate - Invalid puncture type!!!\n");
		break;
	}

	return punc_type;
}

/**
 * get_reg_mask - Calculate the regulatory mask for a given offset and bandwidth
 * @offset: Offset value for the frequency
 * @bw: Bandwidth of the channel
 * @punc_type: Type of puncture (enum puncture_type)
 * @pu_mask_l_edge: Pointer to the left edge puncture mask structure
 * @pu_mask_l: Pointer to the left interim puncture mask structure
 * @pu_mask_r: Pointer to the right interim puncture mask structure
 * @pu_mask_r_edge: Pointer to the right edge puncture mask structure
 *
 * This function calculates the regulatory mask for a given offset and bandwidth
 * based on the puncture type and the puncture mask limits defined in the pmask
 * structures. It determines the appropriate mask value by comparing the
 * non-puncture mask and puncture mask values.
 *
 * Return: The calculated regulatory mask value.
 */
static s16 get_reg_mask(s16 offset, u16 bw, enum ath12k_puncture_type punc_type,
			struct ath12k_punct_mask *pu_mask_l_edge,
			struct ath12k_punct_mask *pu_mask_l,
			struct ath12k_punct_mask *pu_mask_r,
			struct ath12k_punct_mask *pu_mask_r_edge)
{
	s16 mask;
	s16 mask_def;
	s16 mask_le;
	s16 mask_re;
	s16 mask_l;
	s16 mask_r;
	s16 mask_punc;

	mask_def = get_reg_mask_non_puncture(offset, bw);
	if (is_punc_type_invalid(punc_type))
		return mask_def;

	mask_le = get_reg_mask_puncture(offset, bw, pu_mask_l_edge);
	mask_re = get_reg_mask_puncture(offset, bw, pu_mask_r_edge);
	mask_l = get_reg_mask_puncture(offset, bw, pu_mask_l);
	mask_r = get_reg_mask_puncture(offset, bw, pu_mask_r);

	switch (punc_type) {
	case ATH12K_PUNCTURE_TYPE_EDGE:
		mask_punc = min(mask_le, mask_re);
		break;
	case ATH12K_PUNCTURE_TYPE_INTERIM_20_PLUS:
		if ((pu_mask_l->offset[0] <= (offset * 10)) &&
		    ((offset * 10) <= pu_mask_r->offset[2]))
			mask_punc = max(mask_l, mask_r);
		else
			mask_punc = min(mask_le, mask_re);
		break;
	case ATH12K_PUNCTURE_TYPE_INTERIM_20:
		mask_punc = max(mask_l, mask_r);
		break;
	default:
		return mask_def;
	}

	mask = min(mask_punc, mask_def);

	return mask;
}

/**
 * get_psd_limit - Get the minimum PSD limit for a given frequency
 * @freq: Frequency for which the PSD limit is to be determined
 * @num_freq_obj: Number of frequency objects in the AFC response
 * @afc_freq_info: Pointer to the array of AFC frequency objects
 *
 * This function calculates the minimum PSD (Power Spectral Density) limit for
 * a given frequency by iterating through the AFC frequency objects. It returns
 * the minimum PSD limit found within the range of the frequency objects.
 *
 * Return: Minimum PSD limit for the given frequency, or INVALID_PSD if the
 * frequency is not found within the AFC frequency objects.
 */
static s16
get_psd_limit(u16 freq, u8 num_freq_obj, struct ath12k_afc_freq_obj *afc_freq_info)
{
	u8 i;
	s16 min_psd = ATH12K_CHAN_MAX_PSD_POWER * ATH12K_EIRP_PWR_SCALE;
	bool chan_freq_found = false;

	for (i = 0; i < num_freq_obj; i++) {
		if (freq >= afc_freq_info[i].low_freq &&
		    freq <= afc_freq_info[i].high_freq) {
			chan_freq_found = true;
			if (afc_freq_info[i].max_psd < min_psd)
				min_psd = afc_freq_info[i].max_psd;
			/* Even though the frequency object is found, there may
			 * be more matching frequency-object following it.
			 * Continue search until the input frequency is out of
			 * range.
			 */
			continue;
		}

		/* Assuming AFC payload is sorted in increasing order of
		 * frequencies, stop and return here.
		 */
		if (chan_freq_found)
			return min_psd;
	}

	/* Handle for last frequency object */
	if (chan_freq_found)
		return min_psd;

	return ATH12K_INVALID_PSD;
}

void
ath12_mac_reg_get_6g_min_psd(struct ath12k *ar, u16 freq, u16 cfreq,
			     u16 puncture_bitmap, u16 bw, s16 *min_psd)
{
	u16 freq_start;
	u16 freq_end;
	s16 psd_limit;
	u16 hbw;
	u16 adj_freq_start;
	u16 adj_freq_end;
	struct ath12k_afc_freq_obj *afc_freq_info;
	u8 num_freq_obj;
	int i;

	*min_psd = ATH12K_CHAN_MAX_PSD_POWER;

	if (!(cfreq >= ATH12K_MIN_6GHZ_FREQ && cfreq <= ATH12K_MAX_6GHZ_FREQ)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Not a 6GHz freq %u", cfreq);
		return;
	}

	num_freq_obj = ar->afc.afc_reg_info->num_freq_objs;
	if (!num_freq_obj) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG, "No freq object present");
		return;
	}

	afc_freq_info = ar->afc.afc_reg_info->afc_freq_info;
	if (!afc_freq_info) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "AFC frequency info is NULL");
		return;
	}

	hbw = bw / 2;
	freq_start = cfreq - hbw;
	freq_end   = cfreq + hbw;
	adj_freq_start = max(ATH12K_MIN_6GHZ_FREQ, (cfreq - (3 * hbw)));
	adj_freq_end   = min((cfreq + (3 * hbw)), ATH12K_MAX_6GHZ_FREQ);
	*min_psd *= ATH12K_EIRP_PWR_SCALE;
	for (i = adj_freq_start; i <= adj_freq_end; i++) {
		s16 offset = i - cfreq;
		u16 modoffset = abs(offset);

		psd_limit = get_psd_limit(i, num_freq_obj, afc_freq_info);
		if (psd_limit == ATH12K_INVALID_PSD) {
			/* If PSD limit is invalid for usable freq and not
			 * punctured, return failure. Other adjacent freq can be
			 * ignored.
			 */
			if (i >= freq_start && i < freq_end) {
				if (!(puncture_bitmap &
				    (1 << ((i - freq_start) /
					   ATH12K_20MHZ_BW)))) {
					return;
				}
			}
			continue;
		}

		if (modoffset <= ((bw * 3) / 2)) {
			enum ath12k_puncture_type punc_type;
			s16 mask;
			struct ath12k_punct_mask pu_mask_l, pu_mask_r;
			struct ath12k_punct_mask pu_mask_l_edge, pu_mask_r_edge;

			punc_type =
				get_puncture_type_and_masks(bw, puncture_bitmap,
							    &pu_mask_l_edge, &pu_mask_l,
							    &pu_mask_r, &pu_mask_r_edge);
			mask = get_reg_mask(offset, bw, punc_type, &pu_mask_l_edge,
					    &pu_mask_l, &pu_mask_r, &pu_mask_r_edge);
			*min_psd = min((int16_t)(*min_psd),
				       (int16_t)(psd_limit - mask * 10));
		}
	}
	*min_psd /= ATH12K_EIRP_PWR_SCALE;
	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "freq %u cfreq %u pp %u bw %u min_psd %u\n",
		   freq, cfreq, puncture_bitmap, bw, *min_psd);
}

static struct
ieee80211_bss_conf *ath12k_get_link_bss_conf(struct ath12k_link_vif *arvif)
{
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct ieee80211_bss_conf *link_conf = NULL;

	WARN_ON(!rcu_read_lock_held());

	if (arvif->link_id > IEEE80211_MLD_MAX_NUM_LINKS)
		return NULL;

	link_conf = rcu_dereference(vif->link_conf[arvif->link_id]);

	return link_conf;
}

/**
 * ath12k_mac_fill_reg_tpc - Populate transmit power control (TPC) info
 *                           based on regulatory and firmware capabilities
 * @ar: Pointer to ath12k device context
 * @wdev: Pointer to wireless device structure
 * @arvif: Pointer to ath12k virtual interface context
 * @chanctx: Pointer to channel context configuration
 *
 * This function determines the appropriate method to populate the
 * regulatory TPC information based on the 6 GHz power mode and firmware
 * capabilities. It selects one of the following:
 *
 * - If the firmware supports both PSD and EIRP for SP mode and the
 *   device is operating in Standard Power (SP) AP mode, it invokes
 *   ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_sp().
 *
 * - If the firmware prefers EIRP-based power configuration, it calls
 *   ath12k_mac_fill_reg_tpc_info_with_eirp_power().
 *
 * - Otherwise, it falls back to ath12k_mac_fill_reg_tpc_info().
 *
 */
static void ath12k_mac_fill_reg_tpc(struct ath12k *ar, struct wireless_dev *wdev,
				    struct ath12k_link_vif *arvif,
				    struct ieee80211_chanctx_conf *chanctx)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	u8 reg_6g_power_mode;
	struct ieee80211_bss_conf *bss_conf = ath12k_get_link_bss_conf(arvif);

	if (!bss_conf) {
		ath12k_warn(ar->ab, "BSS conf is NULL for link %d\n", arvif->link_id);
		return;
	}

	reg_6g_power_mode = bss_conf->power_type;
	if (reg_6g_power_mode == IEEE80211_REG_UNSET_AP)
		reg_6g_power_mode = IEEE80211_REG_LPI_AP;
	else if (reg_6g_power_mode == IEEE80211_REG_SP_AP &&
		 !ar->afc.is_6ghz_afc_power_event_received)
		reg_6g_power_mode = REG_SP_CLIENT_TYPE;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, " reg_6g_power_mode %d\n", reg_6g_power_mode);

	if (test_bit(WMI_TLV_SERVICE_BOTH_PSD_EIRP_FOR_AP_SP_CLIENT_SP_SUPPORT,
		     ar->ab->wmi_ab.svc_map) &&
	    (reg_6g_power_mode == IEEE80211_REG_SP_AP ||
	     reg_6g_power_mode == REG_SP_CLIENT_TYPE)) {
		if (ahvif->vdev_type == WMI_VDEV_TYPE_AP)
			ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_sp(ar, arvif, chanctx);
		else if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
			ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_client_sp(ar,
										     arvif,
										     chanctx);
	} else if (test_bit(WMI_TLV_SERVICE_EIRP_PREFERRED_SUPPORT,
			    ar->ab->wmi_ab.svc_map)) {
		ath12k_mac_fill_reg_tpc_info_with_eirp_power(ar, arvif, chanctx);
	} else {
		ath12k_mac_fill_reg_tpc_info(ar, arvif, chanctx);
	}
}

void ath12k_mac_bss_info_changed(struct ath12k *ar,
				struct ath12k_link_vif *arvif,
				struct ieee80211_bss_conf *info,
				u64 changed)
{
	struct ieee80211_chanctx_conf *chanctx = &arvif->chanctx;
	struct ath12k_vif *ahvif = arvif->ahvif, *tx_ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ath12k_wmi_vdev_up_params params = { 0 };
	struct ieee80211_vif_cfg *vif_cfg = &vif->cfg;
	struct ath12k_link_vif *tx_arvif;
	struct cfg80211_chan_def def;
	u32 param_id, param_value;
	enum nl80211_band band;
	u32 vdev_param;
	int mcast_rate;
	u32 preamble;
	u16 hw_value;
	u16 bitrate;
	int ret;
	u8 rateidx;
	u32 rate;
	bool color_collision_detect;
	u8 link_id = arvif->link_id;
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(vif);

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_mac_is_bridge_vdev(arvif))
		return;

	if (unlikely(test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)))
		return;

	if (changed & BSS_CHANGED_FTM_RESPONDER &&
	    arvif->ftm_responder != info->ftm_responder &&
	    (vif->type == NL80211_IFTYPE_AP ||
	     vif->type == NL80211_IFTYPE_MESH_POINT)) {
		param_id = WMI_VDEV_PARAM_ENABLE_DISABLE_RTT_RESPONDER_ROLE;
		ret =  ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param_id,
						     info->ftm_responder);
		if (ret)
			ath12k_warn(ar->ab, "Failed to set ftm responder %i: %d\n",
				    arvif->vdev_id, ret);
		else
			arvif->ftm_responder = info->ftm_responder;
	}

	if (changed & BSS_CHANGED_6GHZ_POWER_MODE ||
	    changed & BSS_CHANGED_TPE) {
		if (WARN_ON(ath12k_mac_vif_link_chan(ahvif->vif, link_id, &def))) {
			ath12k_warn(ar->ab, "Failed to fetch chandef");
			return;
		}
		if (ar->supports_6ghz && def.chan->band == NL80211_BAND_6GHZ &&
		    (ahvif->vdev_type == WMI_VDEV_TYPE_AP ||
		     ahvif->vdev_type == WMI_VDEV_TYPE_STA) &&
		    test_bit(WMI_TLV_SERVICE_EXT_TPC_REG_SUPPORT,
			     ar->ab->wmi_ab.svc_map)) {

			if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
				ath12k_mac_parse_tx_pwr_env(ar, arvif);

			if (!chanctx) {
				ath12k_err(ar->ab, "channel context is NULL");
				return;
			}

			ath12k_mac_fill_reg_tpc(ar, wdev, arvif, chanctx);
			ret = ath12k_wmi_send_vdev_set_tpc_power(ar,
								 arvif->vdev_id,
								 &arvif->reg_tpc_info);
			if (changed & BSS_CHANGED_6GHZ_POWER_MODE &&
			    ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
				if (ret)
					ath12k_warn(ar->ab, "Failed to set 6GHZ power mode\n");
				else
					ath12k_mac_send_pwr_mode_update(ar, wdev, link_id);
			}
		} else {
			ath12k_warn(ar->ab, "Set 6GHZ power mode/TPC not applicable\n");
		}
	}

	if (changed & BSS_CHANGED_BEACON_INT) {
		arvif->beacon_interval = info->beacon_int;

		param_id = WMI_VDEV_PARAM_BEACON_INTERVAL;
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id,
						    arvif->beacon_interval);
		if (ret)
			ath12k_warn(ar->ab, "Failed to set beacon interval for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"Beacon interval: %d set for VDEV: %d\n",
					arvif->beacon_interval, arvif->vdev_id);
	}

	/* send ttlm config before vdev up, so that first beacon itself can
	 * advertise ttlm
	 */
	if (vif->type == NL80211_IFTYPE_AP &&
	    changed & BSS_CHANGED_LINK_ADV_TTLM &&
	    wiphy_ext_feature_isset(ath12k_ar_to_hw(ar)->wiphy,
				    NL80211_EXT_FEATURE_BEACON_ADVERTISED_TTLM_OFFLOAD))
		ath12k_mac_bss_offload_advertised_ttlm(arvif);

	if (changed & BSS_CHANGED_BEACON) {
		param_id = WMI_PDEV_PARAM_BEACON_TX_MODE;
		param_value = WMI_BEACON_BURST_MODE;
		ret = ath12k_wmi_pdev_set_param(ar, param_id,
						param_value, ar->pdev->pdev_id);
		if (ret)
			ath12k_warn(ar->ab, "Failed to set beacon mode for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"Set burst beacon mode for VDEV: %d\n",
					arvif->vdev_id);
		if (!arvif->do_not_send_tmpl || !arvif->bcca_zero_sent) {
			/* need to install Transmitting vif's template first */
			ret = ath12k_mac_setup_bcn_tmpl(arvif);
			if (ret)
				ath12k_warn(ar->ab, "failed to update bcn template: %d\n",
					    ret);
			if (!arvif->pending_csa_up)
				goto skip_pending_cs_up;

			memset(&params, 0, sizeof(params));
			params.vdev_id = arvif->vdev_id;
			params.aid = ahvif->aid;
			params.bssid = arvif->bssid;

			if (info->mbssid_tx_vif) {
				tx_ahvif = (void *)info->mbssid_tx_vif->drv_priv;
				tx_arvif = tx_ahvif->link[info->mbssid_tx_vif_linkid];
				params.tx_bssid = tx_arvif->bssid;
				params.nontx_profile_idx = ahvif->vif->bss_conf.bssid_index;
				params.nontx_profile_cnt = BIT(info->bssid_indicator);
			}

			if (info->mbssid_tx_vif && arvif != tx_arvif &&
			    tx_arvif->pending_csa_up) {
				/* skip non tx vif's */
				goto skip_pending_cs_up;
			}

			ret = ath12k_wmi_vdev_up(arvif->ar, &params);
			if (ret)
				ath12k_warn(ar->ab, "failed to bring vdev up %d: %d\n",
					    arvif->vdev_id, ret);

			arvif->pending_csa_up = false;

			if (info->mbssid_tx_vif && arvif == tx_arvif) {
				struct ath12k_link_vif *arvif_itr;
				list_for_each_entry(arvif_itr, &ar->arvifs, list) {
					if (!arvif_itr->pending_csa_up)
						continue;

					if (arvif_itr->tx_vdev_id != tx_arvif->vdev_id)
						continue;

					memset(&params, 0, sizeof(params));
					params.vdev_id = arvif_itr->vdev_id;
					params.aid = ahvif->aid;
					params.bssid = arvif_itr->bssid;
					params.tx_bssid = tx_arvif->bssid;
					params.nontx_profile_idx =
						ahvif->vif->bss_conf.bssid_index;
					params.nontx_profile_cnt =
						BIT(info->bssid_indicator);

					ret = ath12k_wmi_vdev_up(arvif_itr->ar, &params);
					if (ret)
						ath12k_warn(ar->ab, "failed to bring vdev up %d: %d\n",
							    arvif_itr->vdev_id, ret);
					arvif_itr->pending_csa_up = false;
				}
			}
		}
skip_pending_cs_up:
		if (arvif->bcca_zero_sent)
			arvif->do_not_send_tmpl = true;
		else
			arvif->do_not_send_tmpl = false;

		if (arvif->is_up && info->he_support) {
			param_id = WMI_VDEV_PARAM_BA_MODE;

			if (info->eht_support)
				param_value = WMI_BA_MODE_BUFFER_SIZE_1024;
			else
				param_value = WMI_BA_MODE_BUFFER_SIZE_256;

			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param_id,
							    param_value);

			if (ret)
				ath12k_warn(ar->ab,
					    "failed to set BA BUFFER SIZE %d for vdev: %d\n",
					     param_value, arvif->vdev_id);
		}
	}

	if (changed & (BSS_CHANGED_BEACON_INFO | BSS_CHANGED_BEACON)) {
		arvif->dtim_period = info->dtim_period;

		param_id = WMI_VDEV_PARAM_DTIM_PERIOD;
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id,
						    arvif->dtim_period);

		if (ret)
			ath12k_warn(ar->ab, "Failed to set dtim period for VDEV %d: %i\n",
				    arvif->vdev_id, ret);
		else
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"DTIM period: %d set for VDEV: %d\n",
					arvif->dtim_period, arvif->vdev_id);
	}

	if (changed & BSS_CHANGED_SSID &&
	    vif->type == NL80211_IFTYPE_AP) {
		ahvif->u.ap.ssid_len = vif->cfg.ssid_len;
		if (vif->cfg.ssid_len)
			memcpy(ahvif->u.ap.ssid, vif->cfg.ssid, vif->cfg.ssid_len);
		ahvif->u.ap.hidden_ssid = info->hidden_ssid;
	}

	if (changed & BSS_CHANGED_BSSID && !is_zero_ether_addr(info->bssid))
		ether_addr_copy(arvif->bssid, info->bssid);

	if (changed & BSS_CHANGED_BEACON_ENABLED) {
		if (info->enable_beacon) {
			ret = ath12k_mac_set_he_txbf_conf(arvif);
			if (ret)
				ath12k_warn(ar->ab,
					    "failed to set HE TXBF config for vdev: %d\n",
					    arvif->vdev_id);

			ret = ath12k_mac_set_eht_txbf_conf(arvif);
			if (ret)
				ath12k_warn(ar->ab,
					    "failed to set EHT TXBF config for vdev: %d\n",
					    arvif->vdev_id);
		}
		ath12k_control_beaconing(arvif, info);

		if (arvif->is_up && info->he_support && info->he_oper.params) {
			param_id = WMI_VDEV_PARAM_HEOPS_0_31;
			param_value = info->he_oper.params;
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
							    param_id,
							    param_value);
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"he oper param: %x set for VDEV: %d\n",
					 param_value, arvif->vdev_id);

			if (ret)
				ath12k_warn(ar->ab, "Failed to set he oper params %x for VDEV %d: %i\n",
					    param_value, arvif->vdev_id, ret);
		}
	}

	if (changed & BSS_CHANGED_ERP_CTS_PROT) {
		u32 cts_prot;

		cts_prot = !!(info->use_cts_prot);
		param_id = WMI_VDEV_PARAM_PROTECTION_MODE;

		if (arvif->is_started) {
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
							    param_id, cts_prot);
			if (ret)
				ath12k_warn(ar->ab, "Failed to set CTS prot for VDEV: %d\n",
					    arvif->vdev_id);
			else
				ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
						"Set CTS prot: %d for VDEV: %d\n",
						cts_prot, arvif->vdev_id);
		} else {
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"defer protection mode setup, vdev is not ready yet\n");
		}
	}

	if (changed & BSS_CHANGED_ERP_SLOT) {
		u32 slottime;

		if (info->use_short_slot)
			slottime = WMI_VDEV_SLOT_TIME_SHORT; /* 9us */

		else
			slottime = WMI_VDEV_SLOT_TIME_LONG; /* 20us */

		param_id = WMI_VDEV_PARAM_SLOT_TIME;
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id, slottime);
		if (ret)
			ath12k_warn(ar->ab, "Failed to set erp slot for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"Set slottime: %d for VDEV: %d\n",
					slottime, arvif->vdev_id);
	}

	if (changed & BSS_CHANGED_ERP_PREAMBLE) {
		u32 preamble;

		if (info->use_short_preamble)
			preamble = WMI_VDEV_PREAMBLE_SHORT;
		else
			preamble = WMI_VDEV_PREAMBLE_LONG;

		param_id = WMI_VDEV_PARAM_PREAMBLE;
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id, preamble);
		if (ret)
			ath12k_warn(ar->ab, "Failed to set preamble for VDEV: %d\n",
				    arvif->vdev_id);
		else
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L3,
					"Set preamble: %d for VDEV: %d\n",
					preamble, arvif->vdev_id);
	}

	if (changed & BSS_CHANGED_ASSOC) {
		if (vif->cfg.assoc)
			ath12k_bss_assoc(ar, arvif, info);
		else
			ath12k_bss_disassoc(ar, arvif);
	}

	if (changed & BSS_CHANGED_TXPOWER) {
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mac vdev_id %i txpower %d\n", arvif->vdev_id,
				info->txpower);
		arvif->txpower = info->txpower;
		ath12k_mac_txpower_recalc(ar);
	}

	if (changed & BSS_CHANGED_MCAST_RATE &&
	    !ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)) {
		band = def.chan->band;
		mcast_rate = info->mcast_rate[band];

		if (mcast_rate > 0) {
			rateidx = mcast_rate - 1;
		} else {
			rateidx = ffs(info->basic_rates);
			if (rateidx)
				rateidx -= 1;
		}

		if (ar->pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP)
			rateidx += ATH12K_MAC_FIRST_OFDM_RATE_IDX;

		bitrate = ath12k_legacy_rates[rateidx].bitrate;
		hw_value = ath12k_legacy_rates[rateidx].hw_value;

		if (ath12k_mac_bitrate_is_cck(bitrate))
			preamble = WMI_RATE_PREAMBLE_CCK;
		else
			preamble = WMI_RATE_PREAMBLE_OFDM;

		rate = ATH12K_HW_RATE_CODE(hw_value, 0, preamble);

		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mac vdev %d mcast_rate %x\n",
				arvif->vdev_id, rate);

		vdev_param = WMI_VDEV_PARAM_MCAST_DATA_RATE;
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    vdev_param, rate);
		if (ret)
			ath12k_warn(ar->ab,
				    "failed to set mcast rate on vdev %i: %d\n",
				    arvif->vdev_id,  ret);

		vdev_param = WMI_VDEV_PARAM_BCAST_DATA_RATE;
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    vdev_param, rate);
		if (ret)
			ath12k_warn(ar->ab,
				    "failed to set bcast rate on vdev %i: %d\n",
				    arvif->vdev_id,  ret);
	}

	if (changed & BSS_CHANGED_BASIC_RATES &&
	    !ath12k_mac_vif_link_chan(vif, arvif->link_id, &def))
		ath12k_recalculate_mgmt_rate(ar, arvif, &def);

	if (changed & BSS_CHANGED_TWT) {
		if (info->twt_requester || info->twt_responder) {
			ath12k_wmi_send_twt_enable_cmd(ar, ar->pdev->pdev_id);
			if (info->twt_responder) {
				u32 twt_support = info->twt_restricted ?
					WMI_TWT_VDEV_CFG_TWT_RESP_ITWT_BTWT_RTWT :
					info->twt_broadcast ?
					WMI_TWT_VDEV_CFG_TWT_RESP_ITWT_BTWT :
					WMI_TWT_VDEV_CFG_TWT_RESP_ITWT;
				ath12k_wmi_send_twt_vdev_cfg_cmd(ar,
								 arvif->vdev_id,
								 twt_support);
			}
		} else
			ath12k_wmi_send_twt_disable_cmd(ar, ar->pdev->pdev_id);
	}

	if (changed & BSS_CHANGED_HE_OBSS_PD)
		ath12k_mac_config_obss_pd(ar, &info->he_obss_pd);

	if (changed & BSS_CHANGED_HE_BSS_COLOR) {
		color_collision_detect = (info->he_bss_color.enabled &&
					  info->he_bss_color.collision_detection_enabled);
		if (vif->type == NL80211_IFTYPE_AP) {
			ret = ath12k_wmi_obss_color_cfg_cmd(ar,
							    arvif->vdev_id,
							    info->he_bss_color.color,
							    ATH12K_BSS_COLOR_AP_PERIODS,
							    color_collision_detect);
			if (ret)
				ath12k_warn(ar->ab, "failed to set bss color collision on vdev %i: %d\n",
					    arvif->vdev_id,  ret);

			param_id = WMI_VDEV_PARAM_BSS_COLOR;
			param_value = info->he_bss_color.color << IEEE80211_HE_OPERATION_BSS_COLOR_OFFSET;

			if (!info->he_bss_color.enabled)
				param_value |= IEEE80211_HE_OPERATION_BSS_COLOR_DISABLED;
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
							    param_id,
							    param_value);
			if (ret)
				ath12k_warn(ar->ab,
					    "failed to set bss color param on vdev %i: %d\n",
					    arvif->vdev_id,  ret);

			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"bss color param 0x%x set on vdev %i\n",
					param_value, arvif->vdev_id);

		} else if (vif->type == NL80211_IFTYPE_STATION) {
			ret = ath12k_wmi_send_bss_color_change_enable_cmd(ar,
									  arvif->vdev_id,
									  1);
			if (ret)
				ath12k_warn(ar->ab, "failed to enable bss color change on vdev %i: %d\n",
					    arvif->vdev_id,  ret);
			ret = ath12k_wmi_obss_color_cfg_cmd(ar,
							    arvif->vdev_id,
							    0,
							    ATH12K_BSS_COLOR_STA_PERIODS,
							    1);
			if (ret)
				ath12k_warn(ar->ab, "failed to set bss color collision on vdev %i: %d\n",
					    arvif->vdev_id,  ret);
		}
	}

	if (changed & BSS_CHANGED_FILS_DISCOVERY ||
	    changed & BSS_CHANGED_UNSOL_BCAST_PROBE_RESP)
		ath12k_mac_fils_discovery(arvif, info);

	if (changed & BSS_CHANGED_PS &&
	    ar->ab->hw_params->supports_sta_ps) {
		ahvif->ps = vif_cfg->ps;
		ath12k_mac_vif_setup_ps(arvif);
	}

	if (changed & BSS_CHANGED_AP_PS) {
		ar->ap_ps_enabled = info->ap_ps_enable;
		ath12k_mac_ap_ps_recalc(ar);
	}

	if (changed & BSS_CHANGED_ML_MAX_REC_LINKS)
		ath12k_mac_vdev_ml_max_rec_links(arvif, info->ml_max_rec_links);

	if (changed & BSS_CHANGED_INTF_DETECT) {
		ath12k_mac_set_cw_intf_detect(ar, info->intf_detect_bitmap);
	}
}

static struct ath12k_vif_cache *ath12k_ahvif_get_link_cache(struct ath12k_vif *ahvif,
							    u8 link_id)
{
	if (!ahvif->cache[link_id]) {
		ahvif->cache[link_id] = kzalloc(sizeof(*ahvif->cache[0]), GFP_KERNEL);
		if (ahvif->cache[link_id])
			INIT_LIST_HEAD(&ahvif->cache[link_id]->key_conf.list);
	}

	return ahvif->cache[link_id];
}

static void ath12k_ahvif_put_link_key_cache(struct ath12k_vif_cache *cache)
{
	struct ath12k_key_conf *key_conf, *tmp;

	if (!cache || list_empty(&cache->key_conf.list))
		return;
	list_for_each_entry_safe(key_conf, tmp, &cache->key_conf.list, list) {
		list_del(&key_conf->list);
		kfree(key_conf);
	}
}

static void ath12k_ahvif_put_link_cache(struct ath12k_vif *ahvif, u8 link_id)
{
	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return;

	ath12k_ahvif_put_link_key_cache(ahvif->cache[link_id]);
	kfree(ahvif->cache[link_id]);
	ahvif->cache[link_id] = NULL;
}

void ath12k_mac_op_link_info_changed(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct ieee80211_bss_conf *info,
				     u64 changed)
{
	struct ath12k *ar;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_vif_cache *cache;
	struct ath12k_link_vif *arvif;
	u8 link_id = info->link_id;

	lockdep_assert_wiphy(hw->wiphy);

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);

	/* if the vdev is not created on a certain radio,
	 * cache the info to be updated later on vdev creation
	 */

	if (!arvif || !arvif->is_created) {
		cache = ath12k_ahvif_get_link_cache(ahvif, link_id);
		if (!cache)
			return;

		cache->bss_conf_changed |= changed;

		return;
	}

	ar = arvif->ar;

	ath12k_mac_bss_info_changed(ar, arvif, info, changed);
}
EXPORT_SYMBOL(ath12k_mac_op_link_info_changed);

static struct ath12k*
ath12k_mac_select_scan_device(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      u32 center_freq)
{
	struct ath12k_hw *ah = hw->priv;
	enum nl80211_band band;
	struct ath12k *ar;
	int i;

	if (ah->num_radio == 1)
		return ah->radio;

	/* Currently mac80211 supports splitting scan requests into
	 * multiple scan requests per band.
	 * Loop through first channel and determine the scan radio
	 * TODO: There could be 5 GHz low/high channels in that case
	 * split the hw request and perform multiple scans
	 */

	if (center_freq < ATH12K_MIN_5GHZ_FREQ)
		band = NL80211_BAND_2GHZ;
	else if (center_freq < ATH12K_MIN_6GHZ_FREQ)
		band = NL80211_BAND_5GHZ;
	else
		band = NL80211_BAND_6GHZ;

	for_each_ar(ah, ar, i) {
		if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ) {
			if (center_freq >= KHZ_TO_MHZ(ar->freq_range.start_freq) &&
			    center_freq <= KHZ_TO_MHZ(ar->freq_range.end_freq))
				if (ar->mac.sbands[band].channels)
					return ath12k_mac_get_ar_by_pdev_id(ar->ab, ar->pdev->pdev_id);
		} else if (ar->mac.sbands[band].channels) {
			return ath12k_mac_get_ar_by_pdev_id(ar->ab, ar->pdev->pdev_id);
		}
	}
	return NULL;
}

void __ath12k_mac_scan_finish(struct ath12k *ar)
{
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);

	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		if (ar->scan.is_roc && ar->scan.roc_notify)
			ieee80211_remain_on_channel_expired(hw);
		fallthrough;
	case ATH12K_SCAN_STARTING:
		cancel_delayed_work(&ar->scan.timeout);
		complete_all(&ar->scan.completed);
		if (ar->scan.is_roc)
			ar->scan.scan_id = ATH12K_ROC_SCAN_ID;
		wiphy_work_queue(ar->ah->hw->wiphy, &ar->scan.vdev_clean_wk);
		break;
	}
}

void ath12k_mac_scan_finish(struct ath12k *ar)
{
	spin_lock_bh(&ar->data_lock);
	__ath12k_mac_scan_finish(ar);
	spin_unlock_bh(&ar->data_lock);
}

static int ath12k_scan_stop(struct ath12k *ar)
{
	struct ath12k_wmi_scan_cancel_arg arg = {
		.req_type = WLAN_SCAN_CANCEL_SINGLE,
		.scan_id = ar->scan.is_roc ? ATH12K_ROC_SCAN_ID : ATH12K_SCAN_ID,
		.pdev_id = ar->pdev->pdev_id,
	};
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ret = ath12k_wmi_send_scan_stop_cmd(ar, &arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to stop wmi scan: %d\n", ret);
		goto out;
	}

	ret = wait_for_completion_timeout(&ar->scan.completed, 3 * HZ);
	if (ret == 0) {
		ath12k_warn(ar->ab,
			    "failed to receive scan abort comple: timed out\n");
		ret = -ETIMEDOUT;
	} else if (ret > 0) {
		ret = 0;
	}

out:
	/* Scan state should be updated in scan completion worker but in
	 * case firmware fails to deliver the event (for whatever reason)
	 * it is desired to clean up scan state anyway. Firmware may have
	 * just dropped the scan completion event delivery due to transport
	 * pipe being overflown with data and/or it can recover on its own
	 * before next scan request is submitted.
	 */
	if (ret && ar) {
		spin_lock_bh(&ar->data_lock);
		__ath12k_mac_scan_finish(ar);
		spin_unlock_bh(&ar->data_lock);
	}

	return ret;
}

static void ath12k_scan_abort(struct ath12k *ar)
{
	int ret;
	bool need_stop = false;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
		/* This can happen if timeout worker kicked in and called
		 * abortion while scan completion was being processed.
		 */
		break;
	case ATH12K_SCAN_STARTING:
	case ATH12K_SCAN_ABORTING:
		ath12k_warn(ar->ab, "refusing scan abortion due to invalid scan state: %d\n",
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
		ar->scan.state = ATH12K_SCAN_ABORTING;
		need_stop = true;
		break;
	}

	spin_unlock_bh(&ar->data_lock);

	if (need_stop) {
		ret = ath12k_scan_stop(ar);
		if (ret)
			ath12k_warn(ar->ab, "failed to abort scan: %d\n", ret);
	}
}

static void ath12k_scan_roc_done(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k,
					 scan.roc_done.work);
	spin_lock_bh(&ar->data_lock);
	ar->scan.is_roc = false;
	spin_unlock_bh(&ar->data_lock);
}

static void ath12k_scan_timeout_work(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k,
					 scan.timeout.work);

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	ath12k_scan_abort(ar);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
}

static void ath12k_mac_scan_send_complete(struct ath12k *ar,
					  struct cfg80211_scan_info *info)
{
	struct ath12k_hw *ah = ar->ah;
	struct ath12k *partner_ar;
	int i;

	lockdep_assert_wiphy(ah->hw->wiphy);

	for_each_ar(ah, partner_ar, i)
		if (partner_ar != ar &&
		    partner_ar->scan.state == ATH12K_SCAN_RUNNING &&
		    !partner_ar->scan.is_roc)
			return;

	ieee80211_scan_completed(ah->hw, info);
}

static void ath12k_scan_vdev_clean_work(struct wiphy *wiphy, struct wiphy_work *work)
{
	struct ath12k *ar = container_of(work, struct ath12k,
					 scan.vdev_clean_wk);
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(wiphy);

	arvif = ar->scan.arvif;
	/* The scan vdev has already been deleted. This can occur when a
	 * new scan request is made on the same vif with a different
	 * frequency, causing the scan arvif to move from one radio to
	 * another. Or, scan was abrupted and via remove interface, the
	 * arvif is already deleted. Alternatively, if the scan vdev is not
	 * being used as an actual vdev, then do not delete it.
	 */
	if (!arvif || arvif->is_started || !arvif->is_scan_vif)
		goto work_complete;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac clean scan vdev (link id %u)", arvif->link_id);

	ath12k_mac_remove_link_interface(ah->hw, arvif);
	ath12k_mac_unassign_link_vif(arvif);

work_complete:
	spin_lock_bh(&ar->data_lock);
	ar->scan.arvif = NULL;
	if (!ar->scan.is_roc && ar->scan.scan_id != ATH12K_ROC_SCAN_ID) {
		struct cfg80211_scan_info info = {
			.aborted = ((ar->scan.state ==
				    ATH12K_SCAN_ABORTING) ||
				    (ar->scan.state ==
				    ATH12K_SCAN_STARTING)),
		};

		ath12k_mac_scan_send_complete(ar, &info);
	}

	ar->scan.scan_id = 0;
	ar->scan.state = ATH12K_SCAN_IDLE;
	ar->scan_channel = NULL;
	ar->scan.roc_freq = 0;
	spin_unlock_bh(&ar->data_lock);
}

static int ath12k_start_scan(struct ath12k *ar,
			     struct ath12k_wmi_scan_req_arg *arg)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_spectral_get_mode(ar) == ATH12K_SPECTRAL_BACKGROUND)
		ath12k_spectral_reset_buffer(ar);

	ret = ath12k_wmi_send_scan_start_cmd(ar, arg);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&ar->scan.started, 1 * HZ);
	if (ret == 0) {
		ret = ath12k_scan_stop(ar);
		if (ret)
			ath12k_warn(ar->ab, "failed to stop scan: %d\n", ret);

		return -ETIMEDOUT;
	}

	/* If we failed to start the scan, return error code at
	 * this point.  This is probably due to some issue in the
	 * firmware, but no need to wedge the driver due to that...
	 */
	spin_lock_bh(&ar->data_lock);
	if (ar->scan.state == ATH12K_SCAN_IDLE) {
		spin_unlock_bh(&ar->data_lock);
		return -EINVAL;
	}
	spin_unlock_bh(&ar->data_lock);

	return 0;
}

static int _ath12k_mac_get_fw_stats(struct ath12k *ar,
				    struct ath12k_fw_stats_req_params *param)
{
	struct ath12k_base *ab = ar->ab;
	unsigned long time_left;
	int ret;

	reinit_completion(&ar->fw_stats_complete);

	ret = ath12k_wmi_send_stats_request_cmd(ar, param->stats_id,
						param->vdev_id, param->pdev_id);
	if (ret) {
		ath12k_warn(ab, "failed to request fw stats: %d\n", ret);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "get fw stat pdev id %d vdev id %d stats id 0x%x\n",
		   param->pdev_id, param->vdev_id, param->stats_id);

	time_left = wait_for_completion_timeout(&ar->fw_stats_complete, 1 * HZ);
	if (!time_left) {
		ath12k_warn(ab, "time out while waiting for get fw stats\n");
		return -ETIMEDOUT;
	}

	return 0;
}

int ath12k_mac_get_fw_stats_per_vif(struct ath12k *ar,
				    struct ath12k_fw_stats_req_params *param)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret;

	guard(mutex)(&ah->hw_mutex);

	if (ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	ret = _ath12k_mac_get_fw_stats(ar, param);
	if (ret) {
		ath12k_warn(ab, "Failed to fetch stats per vif\n");
		return ret;
	}

	return 0;
}

int ath12k_mac_get_fw_stats(struct ath12k *ar,
			    struct ath12k_fw_stats_req_params *param)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	unsigned long time_left;
	int ret;

	guard(mutex)(&ah->hw_mutex);

	if (ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	ath12k_fw_stats_reset(ar);

	reinit_completion(&ar->fw_stats_done);

	ret = _ath12k_mac_get_fw_stats(ar, param);
	if (ret) {
		ath12k_warn(ab, "Failed to fetch stats per vif\n");
		return ret;
	}

	/* Firmware sends WMI_UPDATE_STATS_EVENTID back-to-back
	 * when stats data buffer limit is reached. fw_stats_complete
	 * is completed once host receives first event from firmware, but
	 * still there could be more events following. Below is to wait
	 * until firmware completes sending all the events.
	 */
	time_left = wait_for_completion_timeout(&ar->fw_stats_done, 3 * HZ);
	if (!time_left)
		ath12k_dbg(ab, ATH12K_DBG_MAC, "time out while waiting for fw stats done\n");

	return 0;
}

int ath12k_mac_op_get_txpower(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     unsigned int link_id,
				     int *dbm)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_fw_stats_req_params params = {};
	struct ath12k_fw_stats_pdev *pdev;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;
	struct ath12k *ar;
	int ret;

	/* Final Tx power is minimum of Target Power, CTL power, Regulatory
	 * Power, PSD EIRP Power. We just know the Regulatory power from the
	 * regulatory rules obtained. FW knows all these power and sets the min
	 * of these. Hence, we request the FW pdev stats in which FW reports
	 * the minimum of all vdev's channel Tx power.
	 */
	lockdep_assert_wiphy(hw->wiphy);

	arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
	if (!arvif || !arvif->ar)
		return -EINVAL;

	ar = arvif->ar;
	ab = ar->ab;
	if (ah->state != ATH12K_HW_STATE_ON)
		goto err_fallback;

	if (test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags))
		return -EAGAIN;

	/* Limit the requests to Firmware for fetching the tx power */
	if (ar->chan_tx_pwr != ATH12K_PDEV_TX_POWER_INVALID &&
	    time_before(jiffies,
			msecs_to_jiffies(ATH12K_PDEV_TX_POWER_REFRESH_TIME_MSECS) +
					 ar->last_tx_power_update))
		goto send_tx_power;

	params.pdev_id = ar->pdev->pdev_id;
	params.vdev_id = arvif->vdev_id;
	params.stats_id = WMI_REQUEST_PDEV_STAT;
	ret = ath12k_mac_get_fw_stats(ar, &params);
	if (ret) {
		ath12k_warn(ab, "failed to request fw pdev stats: %d\n", ret);
		goto err_fallback;
	}

	spin_lock_bh(&ar->data_lock);
	pdev = list_first_entry_or_null(&ar->fw_stats.pdevs,
					struct ath12k_fw_stats_pdev, list);
	if (!pdev) {
		spin_unlock_bh(&ar->data_lock);
		goto err_fallback;
	}

	/* tx power reported by firmware is in units of 0.5 dBm */
	ar->chan_tx_pwr = pdev->chan_tx_power / 2;
	spin_unlock_bh(&ar->data_lock);
	ar->last_tx_power_update = jiffies;

send_tx_power:
	*dbm = ar->chan_tx_pwr;
	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L3,
			"txpower fetched from firmware %d dBm\n", *dbm);
	return 0;

err_fallback:
	/* We didn't get txpower from FW. Hence, relying on vif->bss_conf.txpower */
	*dbm = vif->bss_conf.txpower;
	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L3,
			"txpower from firmware NaN, reported %d dBm\n", *dbm);
	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_get_txpower);

int ath12k_mac_op_link_reconfig_remove(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       const struct cfg80211_link_reconfig_removal_params *params)
{
	struct ath12k_mac_link_migrate_usr_params migrate_params = {0};
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	int ret = -EINVAL;

	lockdep_assert_wiphy(hw->wiphy);
	arvif = ahvif->link[params->link_id];
	if (!arvif)
		goto exit;

	ar = arvif->ar;

	migrate_params.link_id = params->link_id;
	/* whether WMI command was sent sucessfully or not does not really
	 * matter here since after link removal if peer is not migrated, it
	 * will be anyways disconnected
	 */
	ath12k_mac_process_link_migrate_req(ahvif, &migrate_params);

	ret = ath12k_wmi_mlo_reconfig_link_removal(ar, arvif->vdev_id,
						   params->reconfigure_elem,
						   params->elem_len);

exit:
	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_link_reconfig_remove);

/* Note: only half bandwidth agile is supported */
bool ath12k_is_supported_agile_bandwidth(enum nl80211_chan_width conf_bw,
					 enum nl80211_chan_width agile_bw)
{
	bool is_supported = false;

	switch (conf_bw) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_40:
		if (agile_bw <= conf_bw)
			is_supported = true;
		break;
	case NL80211_CHAN_WIDTH_80:
		if (agile_bw == conf_bw ||
		    agile_bw == NL80211_CHAN_WIDTH_40)
			is_supported = true;
		break;
	case NL80211_CHAN_WIDTH_160:
		if (agile_bw == conf_bw ||
		    agile_bw == NL80211_CHAN_WIDTH_80)
			is_supported = true;
		break;
	case NL80211_CHAN_WIDTH_320:
		if (agile_bw == conf_bw ||
		    agile_bw == NL80211_CHAN_WIDTH_160)
			is_supported = true;
		break;
	default:
		break;
	}

	return is_supported;
}

int ath12k_mac_op_set_radar_background(struct ieee80211_hw *hw,
				       struct cfg80211_chan_def *def)
{
	struct cfg80211_chan_def conf_def;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	bool arvif_found = false;
	struct ath12k *ar;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	if (def)
		ar = ath12k_mac_get_ar_by_chan(hw, def->chan);
	else
		ar = ath12k_mac_get_ar_by_agile_chandef(hw, NL80211_BAND_5GHZ);

	if (!ar)
		return -EINVAL;

	if (ar->ab->dfs_region == ATH12K_DFS_REG_UNSET)
		return -EINVAL;

	if (!test_bit(ar->cfg_rx_chainmask, &ar->pdev->cap.adfs_chain_mask) &&
	    !test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map))
		return -EINVAL;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		ahvif = arvif->ahvif;
		if (arvif->is_started && ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found)
		return -EINVAL;

	if (!def) {
		ret = ath12k_wmi_vdev_adfs_ocac_abort_cmd_send(ar,arvif->vdev_id);
		if (!ret) {
			memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
			ar->agile_chandef.chan = NULL;
		}
	} else {
		if (!cfg80211_chandef_valid(def))
			return -EINVAL;

		if (WARN_ON(ath12k_mac_vif_link_chan(ahvif->vif, arvif->link_id, &conf_def)))
			return -EINVAL;

		if (cfg80211_chandef_identical(&conf_def, def) &&
		    cfg80211_chandef_device_present(def)) {
			if (!test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT,
				      ar->ab->wmi_ab.svc_map))
				return -EINVAL;
			else
				return 0;
		}

		if (!(def->chan->flags & IEEE80211_CHAN_RADAR))
			return -EINVAL;

		/* Note: Only Half width and full bandwidth is supported */
		if(!(ath12k_is_supported_agile_bandwidth(conf_def.width,
							  def->width)))
			return -EINVAL;

		if (conf_def.center_freq1 == def->center_freq1)
			return -EINVAL;

		ret = ath12k_wmi_vdev_adfs_ch_cfg_cmd_send(ar, arvif->vdev_id, def);
		if (!ret) {
			memcpy(&ar->agile_chandef, def, sizeof(struct cfg80211_chan_def));
		} else {
			memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
			ar->agile_chandef.chan = NULL;
		}
	}
	return 0;
}

EXPORT_SYMBOL(ath12k_mac_op_set_radar_background);

static u8
ath12k_mac_find_link_id_by_ar(struct ath12k_vif *ahvif, struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_hw *ah = ahvif->ah;
	unsigned long links = ahvif->links_map;
	unsigned long scan_links_map;
	u8 link_id;

	lockdep_assert_wiphy(ah->hw->wiphy);

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);

		if (!arvif || !arvif->is_created)
			continue;

		if (ar == arvif->ar)
			return link_id;
	}

	/* input ar is not assigned to any of the links of ML VIF, use next
	 * available scan link for scan vdev creation. There are cases where
	 * single scan req needs to be split in driver and initiate separate
	 * scan requests to firmware based on device.
	 */

	/* Set all non-scan links (0-14) of scan_links_map so that ffs() will
	 * choose an available link among scan links (i.e link id >= 15)
	 */
	scan_links_map = ahvif->links_map | ~ATH12K_SCAN_LINKS_MASK;
	return ffs(~scan_links_map) - 1;
}

static int ath12k_mac_initiate_hw_scan(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_scan_request *hw_req,
				       struct ath12k *ar,
				       u8 from_index, u8 to_index)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct cfg80211_scan_request *req = &hw_req->req;
	struct ath12k_wmi_scan_req_arg *arg = NULL;
	u8 link_id;
	int ret;
	int i;
	bool create = true;
	u8 n_channels = to_index - from_index;
	u32 scan_timeout;

	lockdep_assert_wiphy(hw->wiphy);

	if (unlikely(test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)))
		return -ESHUTDOWN;

	arvif = &ahvif->deflink;

	/* check if any of the links of ML VIF is already started on
	 * radio(ar) correpsondig to given scan frequency and use it,
	 * if not use scan link (link 15) for scan purpose.
	 */
	link_id = ath12k_mac_find_link_id_by_ar(ahvif, ar);
	/* All scan links are occupied. ideally this should't happen as
	 * mac80211 won't schedule scan for same band until ongoing scan is
	 * completed, dont try to exceed max links just in case if it happens.
	 */
	if (link_id >= ATH12K_NUM_MAX_LINKS)
		return -EBUSY;

	arvif = ath12k_mac_assign_link_vif(ah, vif, link_id, false);

	if (!arvif) {
		ath12k_err(ar->ab, "Failed to alloc/assign link vif id %u\n",
			   link_id);
		return -ENOMEM;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
			"mac link ID %d selected for scan", arvif->link_id);

	/* If the vif is already assigned to a specific vdev of an ar,
	 * check whether its already started, vdev which is started
	 * are not allowed to switch to a new radio.
	 * If the vdev is not started, but was earlier created on a
	 * different ar, delete that vdev and create a new one. We don't
	 * delete at the scan stop as an optimization to avoid redundant
	 * delete-create vdev's for the same ar, in case the request is
	 * always on the same band for the vif
	 */
	if (arvif->is_created) {
		if (WARN_ON(!arvif->ar))
			return -EINVAL;

		if (ar != arvif->ar && arvif->is_started)
			return -EINVAL;

		if (ar != arvif->ar) {
			ath12k_mac_remove_link_interface(hw, arvif);
			ath12k_mac_unassign_link_vif(arvif);
		} else {
			create = false;
		}
	}

	if (create) {
		/* Previous arvif would've been cleared in radio switch block
		 * above, assign arvif again for create.
		 */
		arvif = ath12k_mac_assign_link_vif(ah, vif, link_id, false);

		if (!arvif) {
			ath12k_err(ar->ab, "Failed to alloc/assign link vif id %u\n",
				   link_id);
			return -ENOMEM;
		}

		if (arvif->link_id == ATH12K_DEFAULT_SCAN_LINK &&
		    (!is_broadcast_ether_addr(req->bssid) &&
		     !is_zero_ether_addr(req->bssid)))
			memcpy(arvif->bssid, req->bssid, ETH_ALEN);

		arvif->is_scan_vif = true;
		ret = ath12k_mac_vdev_create(ar, arvif, false);
		if (ret) {
			ath12k_mac_unassign_link_vif(arvif);
			ath12k_warn(ar->ab, "unable to create scan vdev %d\n", ret);
			return -EINVAL;
		}
	}

	spin_lock_bh(&ar->data_lock);
	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
		reinit_completion(&ar->scan.started);
		reinit_completion(&ar->scan.completed);
		ar->scan.state = ATH12K_SCAN_STARTING;
		ar->scan.is_roc = false;
		ar->scan.arvif = arvif;
		ret = 0;
		break;
	case ATH12K_SCAN_STARTING:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ret = -EBUSY;
		break;
	}
	spin_unlock_bh(&ar->data_lock);

	if (ret)
		goto exit;

	arg = kzalloc(sizeof(*arg), GFP_KERNEL);
	if (!arg) {
		ret = -ENOMEM;
		goto exit;
	}

	ath12k_wmi_start_scan_init(ar, arg, vif->type);
	arg->vdev_id = arvif->vdev_id;
	arg->scan_id = ATH12K_SCAN_ID;

	if (req->ie_len) {
		arg->extraie.ptr = kmemdup(req->ie, req->ie_len, GFP_KERNEL);
		if (!arg->extraie.ptr) {
			ret = -ENOMEM;
			goto exit;
		}
		arg->extraie.len = req->ie_len;
	}

	if (req->n_ssids) {
		arg->num_ssids = req->n_ssids;
		for (i = 0; i < arg->num_ssids; i++)
			arg->ssid[i] = req->ssids[i];
	} else {
		arg->scan_f_passive = 1;
	}

	if (n_channels) {
		arg->chan_list.num_chan = n_channels;
		arg->chan_list.chan = kcalloc(arg->chan_list.num_chan,
					      sizeof(struct chan_info),
					      GFP_KERNEL);

		if (!arg->chan_list.chan) {
			ret = -ENOMEM;
			goto exit;
		}

		struct chan_info *chan = &arg->chan_list.chan[0];
		for (i = 0; i < arg->chan_list.num_chan; i++)
			chan[i].freq = req->channels[i + from_index]->center_freq;
	}

	/* if duration is set, default dwell times will be overwritten */
	if (req->duration) {
		arg->dwell_time_active = req->duration;
		arg->dwell_time_active_2g = req->duration;
		arg->dwell_time_active_6g = req->duration;
		arg->dwell_time_passive = req->duration;
		arg->dwell_time_passive_6g = req->duration;
		arg->burst_duration = req->duration;
		scan_timeout = min_t(u32, arg->max_rest_time *
				    (arg->num_chan - 1) + (req->duration +
				    ATH12K_SCAN_CHANNEL_SWITCH_WMI_EVT_OVERHEAD) *
				    arg->num_chan, arg->max_scan_time);
	} else {
		scan_timeout = arg->max_scan_time;
	}
	/* Add a margin to account for event/command processing */
	scan_timeout = scan_timeout + ATH12K_MAC_SCAN_TIMEOUT_MSECS;

	ret = ath12k_start_scan(ar, arg);
	if (ret) {
		if (ret == -EBUSY)
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"scan engine is busy 11d state %d\n",
					ar->state_11d);
		else
			ath12k_warn(ar->ab, "failed to start hw scan: %d\n", ret);

		spin_lock_bh(&ar->data_lock);
		ar->scan.state = ATH12K_SCAN_IDLE;
		spin_unlock_bh(&ar->data_lock);
		goto exit;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2, "mac scan started");

	/* As per cfg80211/mac80211 scan design, it allows only one
	 * scan at a time. Hence last_scan link id is used for
	 * tracking the link id on which the scan is been done on
	 * this vif.
	 */
	ahvif->last_scan_link = arvif->link_id;

	/* Add a margin to account for event/command processing */
	ieee80211_queue_delayed_work(ath12k_ar_to_hw(ar), &ar->scan.timeout,
				     msecs_to_jiffies(scan_timeout));

exit:
	if (arg) {
		kfree(arg->chan_list.chan);
		kfree(arg->extraie.ptr);
		kfree(arg);
	}

	if (ar->state_11d == ATH12K_11D_PREPARING &&
	   ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	   arvif->vdev_subtype == WMI_VDEV_SUBTYPE_NONE)
		ath12k_mac_11d_scan_start(ar, arvif->vdev_id);

	return ret;
}

int ath12k_mac_op_hw_scan(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  struct ieee80211_scan_request *hw_req)
{
	struct ath12k *ar, *prev_ar;
	int i, from_index, to_index, ret;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ath12k_ah_to_ag(ah);

	lockdep_assert_wiphy(hw->wiphy);

	if (ath12k_check_erp_power_down(ag)) {
		ret = ath12k_core_power_up(ag);
		if (ret)
			return ret;
	}

	/* Since the targeted scan device could depend on the frequency
	 * requested in the hw_req, select the corresponding radio
	 */
	prev_ar = ath12k_mac_select_scan_device(hw, vif,
						hw_req->req.channels[0]->center_freq);
	if (!prev_ar) {
		ath12k_err(NULL, "unable to select device for scan\n");
		return -EINVAL;
	}

	/* Check ROC state before starting scan */
	spin_lock_bh(&prev_ar->data_lock);
	if (prev_ar->scan.is_roc) {
		spin_unlock_bh(&prev_ar->data_lock);
		return -EBUSY;
	}
	spin_unlock_bh(&prev_ar->data_lock);
	/* NOTE: There could be 5G low/high channels as mac80211 sees
	 * it as an single band. In that case split the hw request and
	 * perform multiple scans
	 */
	from_index = 0;
	for (i = 1; i < hw_req->req.n_channels; i++) {
		ar = ath12k_mac_select_scan_device(hw, vif,
						   hw_req->req.channels[i]->center_freq);
		if (!ar) {
			ath12k_err(NULL, "unable to select device for scan\n");
			return -EINVAL;
		}
		if (prev_ar == ar)
			continue;

		/* Check if the new radio has ROC active */
		spin_lock_bh(&ar->data_lock);
		if (ar->scan.is_roc) {
			spin_unlock_bh(&ar->data_lock);
			return -EBUSY;
		}
		spin_unlock_bh(&ar->data_lock);

		to_index = i;
		ath12k_mac_initiate_hw_scan(hw, vif, hw_req, prev_ar,
					    from_index, to_index);
		from_index = to_index;
		prev_ar = ar;
	}
	return ath12k_mac_initiate_hw_scan(hw, vif, hw_req, prev_ar, from_index, i);
}
EXPORT_SYMBOL(ath12k_mac_op_hw_scan);

void ath12k_mac_op_cancel_hw_scan(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	u16 link_id = ahvif->last_scan_link;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;

	lockdep_assert_wiphy(hw->wiphy);

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
	if (!arvif || arvif->is_started)
		return;

	ar = arvif->ar;

	ath12k_scan_abort(ar);

	cancel_delayed_work_sync(&ar->scan.timeout);
}
EXPORT_SYMBOL(ath12k_mac_op_cancel_hw_scan);

static int ath12k_install_key(struct ath12k_link_vif *arvif,
			      struct ieee80211_key_conf *key,
			      enum set_key_cmd cmd,
			      const u8 *macaddr, u32 flags)
{
	int ret;
	struct ath12k *ar = arvif->ar;
	struct wmi_vdev_install_key_arg arg = {
		.vdev_id = arvif->vdev_id,
		.key_idx = key->keyidx,
		.key_len = key->keylen,
		.key_data = key->key,
		.key_flags = flags,
		.macaddr = macaddr,
	};

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	reinit_completion(&ar->install_key_done);

	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ar->ab->ag->flags))
		return 0;

	if (cmd == DISABLE_KEY) {
		/* TODO: Check if FW expects  value other than NONE for del */
		/* arg.key_cipher = WMI_CIPHER_NONE; */
		arg.key_len = 0;
		arg.key_data = NULL;
		goto install;
	}

	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
		arg.key_cipher = WMI_CIPHER_AES_CCM;
		/* TODO: Re-check if flag is valid */
		key->flags |= IEEE80211_KEY_FLAG_GENERATE_IV_MGMT;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		arg.key_cipher = WMI_CIPHER_TKIP;
		arg.key_txmic_len = 8;
		arg.key_rxmic_len = 8;
		break;
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		arg.key_cipher = WMI_CIPHER_AES_GCM;
		key->flags |= IEEE80211_KEY_FLAG_GENERATE_IV_MGMT;
		break;
	case WLAN_CIPHER_SUITE_AES_CMAC:
		arg.key_cipher = WMI_CIPHER_AES_CMAC;
		break;
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
		arg.key_cipher = WMI_CIPHER_AES_GMAC;
		break;
	case WLAN_CIPHER_SUITE_BIP_CMAC_256:
		arg.key_cipher = WMI_CIPHER_NONE;
		break;
	default:
		ath12k_warn(ar->ab, "cipher %d is not supported\n", key->cipher);
		return -EOPNOTSUPP;
	}

	if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ar->ab->ag->flags))
		key->flags |= IEEE80211_KEY_FLAG_GENERATE_IV |
			      IEEE80211_KEY_FLAG_RESERVE_TAILROOM;

install:
	ret = ath12k_wmi_vdev_install_key(arvif->ar, &arg);

	if (ret)
		return ret;

	if (!wait_for_completion_timeout(&ar->install_key_done, 1 * HZ))
		return -ETIMEDOUT;

	if (ether_addr_equal(macaddr, arvif->bssid)) {
		arvif->key_cipher = key->cipher;
		ath12k_dp_tx_update_bank_profile(arvif);
	}

	return ar->install_key_status ? -EINVAL : 0;
}

static int ath12k_clear_peer_keys(struct ath12k_link_vif *arvif,
				  const u8 *addr)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	int first_errno = 0;
	int ret;
	int i, len;
	u32 flags = 0;
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1] = {0};

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp, arvif->vdev_id, addr);

	if (!peer || !peer->dp_peer) {
		spin_unlock_bh(&ab->dp->dp_lock);
		return -ENOENT;
	}

	len = ARRAY_SIZE(peer->dp_peer->keys);
	for (i = 0; i < len; i++) {
		if (!peer->dp_peer->keys[i])
			continue;

		keys[i] = peer->dp_peer->keys[i];

		peer->dp_peer->keys[i] = NULL;
	}
	spin_unlock_bh(&ab->dp->dp_lock);

	for (i = 0; i < len; i++) {
		if (!keys[i])
			continue;

		/* key flags are not required to delete the key */
		ret = ath12k_install_key(arvif, keys[i],
					 DISABLE_KEY, addr, flags);
		if (ret < 0 && first_errno == 0)
			first_errno = ret;

		if (ret < 0)
			ath12k_warn(ab, "failed to remove peer key %d: %d\n",
				    i, ret);
	}

	return first_errno;
}

int ath12k_mac_set_key(struct ath12k *ar, enum set_key_cmd cmd,
			      struct ath12k_link_vif *arvif,
			      struct ath12k_link_sta *arsta,
			      struct ieee80211_key_conf *key)
{
	struct ieee80211_sta *sta = NULL;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_sta *ahsta;
	const u8 *peer_addr;
	int ret;
	u32 flags = 0;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (arsta)
		sta = ath12k_ahsta_to_sta(arsta->ahsta);

	if (sta && sta->mlo &&
	    (test_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &ar->ab->dev_flags)))
	    	return 0;

	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags))
		return 1;

	if (sta)
		peer_addr = arsta->addr;
	else
		peer_addr = arvif->bssid;

	key->hw_key_idx = key->keyidx;

	/* the peer should not disappear in mid-way (unless FW goes awry) since
	 * we already hold wiphy lock. we just make sure its there now.
	 */
	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp, arvif->vdev_id,
							    peer_addr);

	if (!peer) {
		spin_unlock_bh(&ab->dp->dp_lock);

		if (cmd == SET_KEY) {
			ath12k_warn(ab, "cannot install key for non-existent peer %pM\n",
				    peer_addr);
			return -EOPNOTSUPP;
		}

		/* if the peer doesn't exist there is no key to disable
		 * anymore
		 */
		return 0;
	}
	spin_unlock_bh(&ab->dp->dp_lock);

	if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
		flags |= WMI_KEY_PAIRWISE;
	else
		flags |= WMI_KEY_GROUP;

	ret = ath12k_install_key(arvif, key, cmd, peer_addr, flags);
	if (ret) {
		ath12k_warn(ab, "ath12k_install_key failed (%d)\n", ret);
		return ret;
	}

	ret = ath12k_dp_rx_peer_pn_replay_config(arvif, peer_addr, cmd, key);
	if (ret) {
		ath12k_warn(ab, "failed to offload PN replay detection %d\n", ret);
		return ret;
	}

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp, arvif->vdev_id,
							    peer_addr);
	if (peer && peer->dp_peer && cmd == SET_KEY) {
		peer->dp_peer->keys[key->keyidx] = key;
		if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE) {
			peer->dp_peer->ucast_keyidx = key->keyidx;
			peer->dp_peer->sec_type = ath12k_dp_tx_get_encrypt_type(key->cipher);
		} else {
			peer->dp_peer->mcast_keyidx = key->keyidx;
			peer->dp_peer->sec_type_grp = ath12k_dp_tx_get_encrypt_type(key->cipher);
		}
	} else if (peer && peer->dp_peer && cmd == DISABLE_KEY) {
		peer->dp_peer->keys[key->keyidx] = NULL;
		if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
			peer->dp_peer->ucast_keyidx = 0;
		else
			peer->dp_peer->mcast_keyidx = 0;
	} else if (!peer)
		/* impossible unless FW goes crazy */
		ath12k_warn(ab, "peer %pM disappeared!\n", peer_addr);

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);

		switch (key->cipher) {
		case WLAN_CIPHER_SUITE_TKIP:
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_CCMP_256:
		case WLAN_CIPHER_SUITE_GCMP:
		case WLAN_CIPHER_SUITE_GCMP_256:
			if (cmd == SET_KEY)
				ahsta->pn_type = HAL_PN_TYPE_WPA;
			else
				ahsta->pn_type = HAL_PN_TYPE_NONE;
			break;
		default:
			ahsta->pn_type = HAL_PN_TYPE_NONE;
			break;
		}
	}

	spin_unlock_bh(&ab->dp->dp_lock);

	return 0;
}

static int ath12k_mac_update_key_cache(struct ath12k_vif_cache *cache,
				       enum set_key_cmd cmd,
				       struct ieee80211_sta *sta,
				       struct ieee80211_key_conf *key)
{
	struct ath12k_key_conf *key_conf, *tmp;

	list_for_each_entry_safe(key_conf, tmp, &cache->key_conf.list, list) {
		if (key_conf->key != key)
			continue;

		/* If SET key entry is already present in cache, nothing to do,
		 * just return
		 */
		if (cmd == SET_KEY)
			return 0;

		/* DEL key for an old SET key which driver hasn't flushed yet.
		 */
		list_del(&key_conf->list);
		kfree(key_conf);
	}

	if (cmd == SET_KEY) {
		key_conf = kzalloc(sizeof(*key_conf), GFP_KERNEL);

		if (!key_conf)
			return -ENOMEM;

		key_conf->cmd = cmd;
		key_conf->sta = sta;
		key_conf->key = key;
		list_add_tail(&key_conf->list,
			      &cache->key_conf.list);
	}

	return 0;
}

int ath12k_mac_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			  struct ieee80211_vif *vif, struct ieee80211_sta *sta,
			  struct ieee80211_key_conf *key)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta = NULL;
	struct ath12k_vif_cache *cache;
	struct ath12k_sta *ahsta;
	unsigned long links;
	u8 link_id;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	/* IGTK needs to be done in host software */
	if (key->keyidx == 4 || key->keyidx == 5)
		return 1;

	if (key->keyidx > WMI_MAX_KEY_INDEX)
		return -ENOSPC;

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);

		/* For an ML STA Pairwise key is same for all associated link Stations,
		 * hence do set key for all link STAs which are active.
		 */
		if (sta->mlo) {
			links = ahsta->links_map;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arvif = wiphy_dereference(hw->wiphy,
							  ahvif->link[link_id]);
				arsta = wiphy_dereference(hw->wiphy,
							  ahsta->link[link_id]);

				if (WARN_ON(!arvif || !arsta))
					/* arvif and arsta are expected to be valid when
					 * STA is present.
					 */
					continue;

				ret = ath12k_mac_set_key(arvif->ar, cmd, arvif,
							 arsta, key);
				if (ret)
					break;

				arsta->keys[key->keyidx] = key;
			}

			return 0;
		}

		arsta = &ahsta->deflink;
		arvif = arsta->arvif;
		if (WARN_ON(!arvif))
			return -EINVAL;

		ret = ath12k_mac_set_key(arvif->ar, cmd, arvif, arsta, key);
		if (ret)
			return ret;

		arsta->keys[key->keyidx] = key;
		return 0;
	}

	if (key->link_id >= 0 && key->link_id < IEEE80211_MLD_MAX_NUM_LINKS) {
		link_id = key->link_id;
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
	} else {
		link_id = 0;
		arvif = &ahvif->deflink;
	}

	if (!arvif || !arvif->is_created) {
		cache = ath12k_ahvif_get_link_cache(ahvif, link_id);
		if (!cache)
			return -ENOSPC;

		ret = ath12k_mac_update_key_cache(cache, cmd, sta, key);
		if (ret)
			return ret;

		return 0;
	}

	ret = ath12k_mac_set_key(arvif->ar, cmd, arvif, NULL, key);
	if (ret)
		return ret;

	/* if sta is null, consider it has self peer */
	arvif->keys[key->keyidx] = key;

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_set_key);

static int
ath12k_mac_bitrate_mask_num_he_ul_rates(struct ath12k *ar,
				    enum nl80211_band band,
				    const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].he_ul_mcs); i++)
		num_rates += hweight16(mask->control[band].he_ul_mcs[i]);

	return num_rates;
}

static int
ath12k_mac_bitrate_mask_num_ht_rates(struct ath12k *ar,
				    enum nl80211_band band,
				    const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].ht_mcs); i++)
		num_rates += hweight16(mask->control[band].ht_mcs[i]);

	return num_rates;
}

static int
ath12k_mac_bitrate_mask_num_vht_rates(struct ath12k *ar,
				      enum nl80211_band band,
				      const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].vht_mcs); i++)
		num_rates += hweight16(mask->control[band].vht_mcs[i]);

	return num_rates;
}

static int
ath12k_mac_bitrate_mask_num_he_rates(struct ath12k *ar,
				     enum nl80211_band band,
				     const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].he_mcs); i++)
		num_rates += hweight16(mask->control[band].he_mcs[i]);

	return num_rates;
}

static int
ath12k_mac_set_peer_vht_fixed_rate(struct ath12k_link_vif *arvif,
				   struct ath12k_link_sta *arsta,
				   const struct cfg80211_bitrate_mask *mask,
				   enum nl80211_band band)
{
	struct ath12k *ar = arvif->ar;
	u8 vht_rate, nss;
	u32 rate_code;
	int ret, i;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	nss = 0;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].vht_mcs); i++) {
		if (hweight16(mask->control[band].vht_mcs[i]) == 1) {
			nss = i + 1;
			vht_rate = ffs(mask->control[band].vht_mcs[i]) - 1;
		}
	}

	if (!nss) {
		ath12k_warn(ar->ab, "No single VHT Fixed rate found to set for %pM",
			    arsta->addr);
		return -EINVAL;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"Setting Fixed VHT Rate for peer %pM. Device will not switch to any other selected rates",
			arsta->addr);

	rate_code = ATH12K_HW_RATE_CODE(vht_rate, nss - 1,
					WMI_RATE_PREAMBLE_VHT);
	ret = ath12k_wmi_set_peer_param(ar, arsta->addr,
					arvif->vdev_id,
					WMI_PEER_PARAM_FIXED_RATE,
					rate_code);
	if (ret)
		ath12k_warn(ar->ab,
			    "failed to update STA %pM Fixed Rate %d: %d\n",
			     arsta->addr, rate_code, ret);

	return ret;
}

static int
ath12k_mac_set_peer_ht_fixed_rate(struct ath12k_link_vif *arvif,
				 struct ath12k_link_sta *arsta,
				 const struct cfg80211_bitrate_mask *mask,
				 enum nl80211_band band)
{
	struct ath12k_sta *ahsta = arsta->ahsta;
	struct ieee80211_link_sta *link_sta;
	struct ath12k *ar = arvif->ar;
	struct ieee80211_sta *sta;
	u8 ht_rate, nss;
	u32 rate_code;
	int ret, i;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);
	sta = container_of((void *)ahsta, struct ieee80211_sta, drv_priv);

	nss = 0;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].ht_mcs); i++) {
		if (hweight16(mask->control[band].ht_mcs[i]) == 1) {
			nss = i + 1;
			ht_rate = ffs(mask->control[band].ht_mcs[i]) - 1;
		}
	}

	if (!nss) {
		ath12k_warn(ar->ab, "No single HT Fixed rate found to set for %pM",
			    sta->addr);
		return -EINVAL;
	}

	/* Avoid updating invalid nss as fixed rate*/
	if (!arsta->is_bridge_peer) {
		rcu_read_lock();
		link_sta = rcu_dereference(sta->link[arsta->link_id]);
		if (!link_sta || nss > link_sta->rx_nss) {
			rcu_read_unlock();
			return -EINVAL;
		}
		rcu_read_unlock();
	} else {
		if (nss > sta->deflink.rx_nss)
			return -EINVAL;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"Setting Fixed HT Rate for peer %pM. Device will not switch to any other selected rates",
			sta->addr);

	rate_code = ATH12K_HW_RATE_CODE(ht_rate, nss - 1,
					WMI_RATE_PREAMBLE_HT);
	ret = ath12k_wmi_set_peer_param(ar, sta->addr,
					arvif->vdev_id,
					WMI_PEER_PARAM_FIXED_RATE,
					rate_code);

	if (ret)
		ath12k_warn(ar->ab,
			    "failed to update STA %pM HT Fixed Rate %d: %d\n",
			    sta->addr, rate_code, ret);

	return ret;
}

static int ath12k_mac_set_6g_nonht_dup_conf(struct ath12k_link_vif *arvif,
					    const struct cfg80211_chan_def *chandef)
{
	struct ath12k *ar = arvif->ar;
	int param_id, ret = 0;
	uint8_t value = 0;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_bss_conf *link_conf;
	bool is_psc = cfg80211_channel_is_psc(chandef->chan);
	enum wmi_phy_mode mode = ath12k_phymodes[chandef->chan->band][chandef->width];
	bool nontransmitted;

        rcu_read_lock();
        link_conf = ath12k_mac_get_link_bss_conf(arvif);

        if (!link_conf) {
                rcu_read_unlock();
                return -EINVAL;
        }

	nontransmitted = link_conf->nontransmitted;
	rcu_read_unlock();

	if ((ahvif->vdev_type == WMI_VDEV_TYPE_AP) &&
	    !nontransmitted &&
	    (chandef->chan->band == NL80211_BAND_6GHZ)) {
		param_id = WMI_VDEV_PARAM_6GHZ_PARAMS;
		if (mode > MODE_11AX_HE20 && !is_psc) {
			value |= WMI_VDEV_6GHZ_BITMAP_NON_HT_DUPLICATE_BEACON;
			value |= WMI_VDEV_6GHZ_BITMAP_NON_HT_DUPLICATE_BCAST_PROBE_RSP;
			value |= WMI_VDEV_6GHZ_BITMAP_NON_HT_DUPLICATE_FD_FRAME;
		}
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"Set 6GHz non-ht dup params for vdev %pM ,vdev_id %d param %d value %d\n",
				ahvif->vif->addr, arvif->vdev_id, param_id, value);
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id, param_id, value);
	}

	return ret;
}

static int
ath12k_mac_set_peer_he_fixed_rate(struct ath12k_link_vif *arvif,
				  struct ath12k_link_sta *arsta,
				  const struct cfg80211_bitrate_mask *mask,
				  enum nl80211_band band)
{
	struct ath12k *ar = arvif->ar;
	u8 he_rate, nss;
	u32 rate_code;
	int ret, i;
	struct ath12k_sta *ahsta = arsta->ahsta;
	struct ieee80211_link_sta *link_sta;
	struct ieee80211_sta *sta;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	sta = ath12k_ahsta_to_sta(ahsta);
	nss = 0;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].he_mcs); i++) {
		if (hweight16(mask->control[band].he_mcs[i]) == 1) {
			nss = i + 1;
			he_rate = ffs(mask->control[band].he_mcs[i]) - 1;
		}
	}

	if (!nss) {
		ath12k_warn(ar->ab, "No single HE Fixed rate found to set for %pM",
			    arsta->addr);
		return -EINVAL;
	}

	/* Avoid updating invalid nss as fixed rate*/
	if (!arsta->is_bridge_peer) {
		rcu_read_lock();
		link_sta = rcu_dereference(sta->link[arsta->link_id]);
		if (!link_sta || nss > link_sta->rx_nss) {
			rcu_read_unlock();
			return -EINVAL;
		}
		rcu_read_unlock();
	} else {
		if (nss > sta->deflink.rx_nss)
			return -EINVAL;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"Setting Fixed HE Rate for peer %pM. Device will not switch to any other selected rates",
			 arsta->addr);

	rate_code = ATH12K_HW_RATE_CODE(he_rate, nss - 1,
					WMI_RATE_PREAMBLE_HE);

	ret = ath12k_wmi_set_peer_param(ar, arsta->addr,
					arvif->vdev_id,
					WMI_PEER_PARAM_FIXED_RATE,
					rate_code);
	if (ret)
		ath12k_warn(ar->ab,
			    "failed to update STA %pM Fixed Rate %d: %d\n",
			    arsta->addr, rate_code, ret);

	return ret;
}

static u8 ath12k_mac_get_num_pwr_levels(struct cfg80211_chan_def *chan_def,
					bool is_psd)
{
        u8 num_pwr_levels;

        if (is_psd) {
                switch (chan_def->width) {
                case NL80211_CHAN_WIDTH_20:
                        num_pwr_levels = 1;
                        break;
                case NL80211_CHAN_WIDTH_40:
                        num_pwr_levels = 2;
                        break;
                case NL80211_CHAN_WIDTH_80:
                        num_pwr_levels = 4;
                        break;
                case NL80211_CHAN_WIDTH_80P80:
                case NL80211_CHAN_WIDTH_160:
                        num_pwr_levels = 8;
                        break;
                case NL80211_CHAN_WIDTH_320:
                	num_pwr_levels = 16;
                	break;
                default:
                        return 1;
                }
        } else {
                switch (chan_def->width) {
                case NL80211_CHAN_WIDTH_20:
                        num_pwr_levels = 1;
                        break;
                case NL80211_CHAN_WIDTH_40:
                        num_pwr_levels = 2;
                        break;
                case NL80211_CHAN_WIDTH_80:
                        num_pwr_levels = 3;
                        break;
                case NL80211_CHAN_WIDTH_80P80:
                case NL80211_CHAN_WIDTH_160:
                        num_pwr_levels = 4;
                        break;
                case NL80211_CHAN_WIDTH_320:
                	num_pwr_levels = 5;
                	break;
                default:
                        return 1;
                }
        }

        return num_pwr_levels;
}

static u16 ath12k_mac_get_6g_start_frequency(struct cfg80211_chan_def *chan_def)
{
        u16 diff_seq;

        /* It is to get the lowest channel number's center frequency of the chan.
         * For example,
         * bandwidth=40MHz, center frequency is 5965, lowest channel is 1
         * with center frequency 5955, its diff is 5965 - 5955 = 10.
         * bandwidth=80MHz, center frequency is 5985, lowest channel is 1
         * with center frequency 5955, its diff is 5985 - 5955 = 30.
         * bandwidth=160MHz, center frequency is 6025, lowest channel is 1
         * with center frequency 5955, its diff is 6025 - 5955 = 70.
         */

	if (!chan_def)
		return 0;

        switch (chan_def->width) {
        case NL80211_CHAN_WIDTH_320:
        	diff_seq = 150;
        	break;
        case NL80211_CHAN_WIDTH_160:
                diff_seq = 70;
                break;
        case NL80211_CHAN_WIDTH_80:
        case NL80211_CHAN_WIDTH_80P80:
                diff_seq = 30;
                break;
        case NL80211_CHAN_WIDTH_40:
                diff_seq = 10;
                break;
        default:
                diff_seq = 0;
        }

        return chan_def->center_freq1 - diff_seq;
}

static u16 ath12k_mac_get_seg_freq(struct cfg80211_chan_def *chan_def,
                                  u16 start_seq, u8 seq)
{
       u16 seg_seq;

       /* It is to get the center frequency of the specific bandwidth.
        * start_seq means the lowest channel number's center freqence.
        * seq 0/1/2/3 means 20MHz/40MHz/80MHz/160MHz&80P80.
        * For example,
        * lowest channel is 1, its center frequency 5955,
        * center frequency is 5955 when bandwidth=20MHz, its diff is 5955 - 5955 = 0.
        * lowest channel is 1, its center frequency 5955,
        * center frequency is 5965 when bandwidth=40MHz, its diff is 5965 - 5955 = 10.
        * lowest channel is 1, its center frequency 5955,
        * center frequency is 5985 when bandwidth=80MHz, its diff is 5985 - 5955 = 30.
        * lowest channel is 1, its center frequency 5955,
        * center frequency is 6025 when bandwidth=160MHz, its diff is 6025 - 5955 = 70.
        */
       if (chan_def->width == NL80211_CHAN_WIDTH_80P80 && seq == 3)
               return chan_def->center_freq2;

       seg_seq = 10 * (BIT(seq) - 1);
       return seg_seq + start_seq;
}

static void ath12k_mac_get_psd_channel(struct ath12k *ar,
                                      u16 step_freq,
                                      u16 *start_freq,
                                      u16 *center_freq,
                                      u8 i,
                                      struct ieee80211_channel **temp_chan,
                                      s8 *tx_power,
				      u8 reg_6g_power_mode)
{
       /* It is to get the the center frequency for each 20MHz.
        * For example, if the chan is 160MHz and center frequency is 6025,
        * then it include 8 channels, they are 1/5/9/13/17/21/25/29,
        * channel number 1's center frequency is 5955, it is parameter start_freq.
        * parameter i is the step of the 8 channels. i is 0~7 for the 8 channels.
        * the channel 1/5/9/13/17/21/25/29 maps i=0/1/2/3/4/5/6/7,
        * and maps its center frequency is 5955/5975/5995/6015/6035/6055/6075/6095,
        * the gap is 20 for each channel, parameter step_freq means the gap.
        * after get the center frequency of each channel, it is easy to find the
        * struct ieee80211_channel of it and get the max_reg_power.
        */
	*center_freq = *start_freq + i * step_freq;
	/* -1 to reg_6g_power_mode to make it 0 based indexing */
	*temp_chan = ieee80211_get_6g_channel_khz(ar->ah->hw->wiphy, MHZ_TO_KHZ(*center_freq),
						  reg_6g_power_mode - 1);
	if (*temp_chan) {
		*tx_power = (*temp_chan)->max_reg_power;
	} else {
		ath12k_err(ar->ab, "failed to get channel definition for center freq: %d\n", *center_freq);
		*tx_power = ATH12K_MIN_TX_POWER;
	}
}

static inline bool ath12k_reg_is_320_opclass(u8 opclass)
{
	return (opclass == 137);
}

static s8 ath12k_mac_find_eirp_in_afc_eirp_obj(struct ath12k_chan_eirp_obj *eirp_obj,
					       u32 freq,
					       u16 center_freq,
					       u8 nchans,
					       u8 opclass)
{
	u8 subchannels[ATH12K_NUM_20_MHZ_CHAN_IN_320_MHZ_CHAN];
	u8 k;

	if (ath12k_reg_is_320_opclass(opclass)) {
		u32 cfi_freq = ieee80211_channel_to_freq_khz(eirp_obj->cfi,
							     NL80211_BAND_6GHZ);

		/* FW sends as scaled AFC power value in AFC Power Evenid */
		if (cfi_freq == MHZ_TO_KHZ(center_freq))
			return eirp_obj->eirp_power / ATH12K_EIRP_PWR_SCALE;

		return ATH12K_MAX_TX_POWER;
	}

	ath12k_reg_fill_subchan_centers(nchans, eirp_obj->cfi, subchannels);

	for (k = 0; k < nchans; k++) {
		if (ieee80211_channel_to_freq_khz(subchannels[k], NL80211_BAND_6GHZ) ==
		    MHZ_TO_KHZ(freq)) {
			return eirp_obj->eirp_power / ATH12K_EIRP_PWR_SCALE;
		}
	}

	return ATH12K_MAX_TX_POWER;
}

static s8 ath12k_mac_find_eirp_in_afc_chan_obj(struct ath12k_afc_chan_obj *chan_obj,
					       u32 freq,
					       u16 center_freq,
					       u8 opclass)
{
	s8 afc_eirp_pwr = ATH12K_MAX_TX_POWER;
	u8 j;

	if (chan_obj->global_opclass != opclass)
		goto fail;

	for (j = 0; j < chan_obj->num_chans; j++) {
		struct ath12k_chan_eirp_obj *eirp_obj = &chan_obj->chan_eirp_info[j];
		u8 nchans = ath12k_reg_get_nsubchannels_for_opclass(opclass);

		if (!nchans)
			goto fail;

		afc_eirp_pwr = ath12k_mac_find_eirp_in_afc_eirp_obj(eirp_obj,
								    freq,
								    center_freq,
								    nchans,
								    opclass);

		if (afc_eirp_pwr != ATH12K_MAX_TX_POWER)
			break;
	}

fail:
	return afc_eirp_pwr;
}

static s8 ath12k_mac_get_afc_eirp_power(struct ath12k *ar,
					u32 freq,
					u16 center_freq,
					u16 bw)
{
	struct ath12k_afc_sp_reg_info *power_info = ar->afc.afc_reg_info;
	s8 afc_eirp_pwr = ATH12K_MAX_TX_POWER;
	u8 i, op_class = 0;

	op_class = ath12k_reg_get_opclass_from_bw(bw);
	if (!op_class)
		return afc_eirp_pwr;

	for (i = 0; i < power_info->num_chan_objs; i++) {
		struct ath12k_afc_chan_obj *chan_obj = &power_info->afc_chan_info[i];

		afc_eirp_pwr = ath12k_mac_find_eirp_in_afc_chan_obj(chan_obj,
								    freq,
								    center_freq,
								    op_class);
		if (afc_eirp_pwr != ATH12K_MAX_TX_POWER)
			break;
	}

	return afc_eirp_pwr;
}

static void ath12k_mac_get_eirp_power(struct ath12k *ar,
				      u16 *start_freq,
				      u16 *center_freq,
				      u8 i,
				      struct ieee80211_channel **temp_chan,
				      struct cfg80211_chan_def *def,
				      s8 *tx_power,
				      u8 reg_6g_power_mode)
{
       /* It is to get the the center frequency for 40MHz/80MHz/
        * 160MHz&80P80 bandwidth, and then plus 10 to the center frequency,
        * it is the center frequency of a channel number.
        * For example, when configured channel number is 1.
        * center frequency is 5965 when bandwidth=40MHz, after plus 10, it is 5975,
        * then it is channel number 5.
        * center frequency is 5985 when bandwidth=80MHz, after plus 10, it is 5995,
        * then it is channel number 9.
        * center frequency is 6025 when bandwidth=160MHz, after plus 10, it is 6035,
        * then it is channel number 17.
        * after get the center frequency of each channel, it is easy to find the
        * struct ieee80211_channel of it and get the max_reg_power.
        */
	*center_freq = ath12k_mac_get_seg_freq(def, *start_freq, i);
	/* For 20 MHz, no +10 offset is required */
	if (i != 0)
		*center_freq += 10;

	/* -1 to reg_6g_power_mode to make it 0 based indexing */
	*temp_chan = ieee80211_get_6g_channel_khz(ar->ah->hw->wiphy, MHZ_TO_KHZ(*center_freq),
						  reg_6g_power_mode - 1);
	if (*temp_chan) {
		*tx_power = (*temp_chan)->max_reg_power;
	} else {
		ath12k_err(ar->ab, "failed to get channel definition for center freq: %d\n", *center_freq);
		*tx_power = ATH12K_MIN_TX_POWER;
	}
}

void ath12k_mac_fill_reg_tpc_info(struct ath12k *ar,
                                  struct ath12k_link_vif *arvif,
                                  struct ieee80211_chanctx_conf *ctx)
{
        struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
        struct ieee80211_bss_conf *bss_conf;
        struct ath12k_reg_tpc_power_info *reg_tpc_info = &arvif->reg_tpc_info;
        struct ieee80211_channel *chan, *temp_chan;
        u8 pwr_lvl_idx, num_pwr_levels, pwr_reduction;
        bool is_psd_power = false, is_tpe_present = false;
        s8 max_tx_power[ATH12K_NUM_PWR_LEVELS],
                psd_power, tx_power = 0, eirp_power = 0;
        u16 oper_freq = 0, start_freq = 0, center_freq = 0;
	u8 reg_6g_power_mode;
	enum nl80211_chan_width bw;
	int cfi;

	rcu_read_lock();

	bss_conf = ath12k_get_link_bss_conf(arvif);
	if (!bss_conf) {
		rcu_read_unlock();
		ath12k_warn(ar->ab, "unable to access bss link conf in tpc reg fill\n");
		return;
	}

	reg_6g_power_mode = bss_conf->power_type;
	if (reg_6g_power_mode == IEEE80211_REG_SP_AP &&
	    !ar->afc.is_6ghz_afc_power_event_received)
		reg_6g_power_mode = NL80211_REG_REGULAR_CLIENT_SP + 1;

        chan = ctx->def.chan;
        oper_freq = ctx->def.chan->center_freq;
        start_freq = ath12k_mac_get_6g_start_frequency(&ctx->def);
        pwr_reduction = bss_conf->pwr_reduction;

	rcu_read_unlock();

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    (arvif->reg_tpc_info.num_tpe_psd || arvif->reg_tpc_info.num_tpe_eirp)) {
		is_tpe_present = true;
		if (reg_tpc_info->is_psd_power)
			num_pwr_levels = arvif->reg_tpc_info.num_tpe_psd;
		else
			num_pwr_levels = arvif->reg_tpc_info.num_tpe_eirp;
	} else {
		bool is_psd = ctx->def.chan->flags & IEEE80211_CHAN_PSD;

		num_pwr_levels = ath12k_mac_get_num_pwr_levels(&ctx->def,
							       is_psd);
	}
	if (!is_tpe_present) {
		memset(reg_tpc_info->tpe_eirp, ATH12K_MAX_TX_POWER,
		       IEEE80211_TPE_EIRP_ENTRIES_320MHZ * sizeof(s8));
		memset(reg_tpc_info->tpe_psd, IEEE80211_TPE_PSD_NO_LIMIT,
		       IEEE80211_TPE_PSD_ENTRIES_320MHZ * sizeof(s8));
	}

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"num tpe_psd %u, num_tpe_eirp %u num_pwr_levels = %u\n",
			arvif->reg_tpc_info.num_tpe_psd,
			arvif->reg_tpc_info.num_tpe_eirp,
			num_pwr_levels);

        for (pwr_lvl_idx = 0; pwr_lvl_idx < num_pwr_levels; pwr_lvl_idx++) {
                /* STA received TPE IE*/
                if (is_tpe_present) {
                        /* local power is PSD power*/
                        if (chan->flags & IEEE80211_CHAN_PSD) {
                                /* Connecting AP is psd power */
                                if (reg_tpc_info->is_psd_power) {
                                        is_psd_power = true;
                                        ath12k_mac_get_psd_channel(ar, 20,
                                                                   &start_freq,
                                                                   &center_freq,
                                                                   pwr_lvl_idx,
                                                                   &temp_chan,
                                                                   &tx_power,
								   reg_6g_power_mode);
					eirp_power = tx_power;
					if (temp_chan) {
						psd_power = temp_chan->psd;
						max_tx_power[pwr_lvl_idx] =
							min_t(s8,
							      psd_power,
							      reg_tpc_info->tpe_psd[pwr_lvl_idx]);
					} else {
						max_tx_power[pwr_lvl_idx] =
							reg_tpc_info->tpe_psd[pwr_lvl_idx];
					}
                                /* Connecting AP is not psd power */
                                } else {
                                        ath12k_mac_get_eirp_power(ar,
                                                                  &start_freq,
                                                                  &center_freq,
                                                                  pwr_lvl_idx,
                                                                  &temp_chan,
                                                                  &ctx->def,
                                                                  &tx_power,
								  reg_6g_power_mode);
					if (temp_chan) {
						psd_power = temp_chan->psd;
						/* convert psd power to EIRP power based
						 * on channel width
						 */
						tx_power =
							min_t(s8, tx_power,
							      psd_power + 13 + pwr_lvl_idx * 3);
					}
					max_tx_power[pwr_lvl_idx] =
					    min_t(s8,
						  tx_power,
						  reg_tpc_info->tpe_eirp[pwr_lvl_idx]);
                                }
                        /* local power is not PSD power */
                        } else {
                                /* Connecting AP is psd power */
                                if (reg_tpc_info->is_psd_power) {
                                        is_psd_power = true;
                                        ath12k_mac_get_psd_channel(ar, 20,
                                                                   &start_freq,
                                                                   &center_freq,
                                                                   pwr_lvl_idx,
                                                                   &temp_chan,
                                                                   &tx_power,
								   reg_6g_power_mode);
                                        eirp_power = tx_power;
					max_tx_power[pwr_lvl_idx] =
					    reg_tpc_info->tpe_psd[pwr_lvl_idx];
                                /* Connecting AP is not psd power */
                                } else {
                                        ath12k_mac_get_eirp_power(ar,
                                                                  &start_freq,
                                                                  &center_freq,
                                                                  pwr_lvl_idx,
                                                                  &temp_chan,
                                                                  &ctx->def,
                                                                  &tx_power,
								  reg_6g_power_mode);
                                        max_tx_power[pwr_lvl_idx] =
					    min_t(s8,
						  tx_power,
						  reg_tpc_info->tpe_eirp[pwr_lvl_idx]);
                                }
                        }
                /* STA not received TPE IE */
                } else {
                        /* local power is PSD power*/
                        if (chan->flags & IEEE80211_CHAN_PSD) {
                                is_psd_power = true;
                                ath12k_mac_get_psd_channel(ar, 20,
                                                           &start_freq,
                                                           &center_freq,
                                                           pwr_lvl_idx,
                                                           &temp_chan,
                                                           &tx_power,
							   reg_6g_power_mode);
				if (reg_6g_power_mode == IEEE80211_REG_SP_AP &&
				    ar->afc.is_6ghz_afc_power_event_received) {
					cfi = ieee80211_frequency_to_channel(center_freq);
					bw = NL80211_CHAN_WIDTH_20;
					eirp_power =
						ath12k_reg_get_afc_eirp_power(ar, bw, cfi);
					/* In some case channel obj for that
					 * particular freq  might not be received
					 */
					if (!eirp_power)
						eirp_power = tx_power;
				} else {
					eirp_power = tx_power;
				}

				if (temp_chan) {
					psd_power = temp_chan->psd;
					max_tx_power[pwr_lvl_idx] = psd_power;
				} else {
					max_tx_power[pwr_lvl_idx] = ATH12K_MIN_TX_POWER;
				}
                        } else {
                                ath12k_mac_get_eirp_power(ar,
                                                          &start_freq,
                                                          &center_freq,
                                                          pwr_lvl_idx,
                                                          &temp_chan,
                                                          &ctx->def,
                                                          &tx_power,
							  reg_6g_power_mode);
                                max_tx_power[pwr_lvl_idx] = tx_power;
				min_t(s8, tx_power, reg_tpc_info->tpe_eirp[pwr_lvl_idx]);
				if (reg_6g_power_mode == IEEE80211_REG_SP_AP &&
				    ar->afc.is_6ghz_afc_power_event_received) {
					ath12k_reg_get_afc_eirp_power_for_bw(ar, &start_freq,
									     &center_freq,
									     pwr_lvl_idx,
									     &ctx->def,
									     &tx_power);
					/* Override tx power only if afc response has a value */
					if (tx_power)
						max_tx_power[pwr_lvl_idx] = tx_power;
				}
                        }
                }

                if (is_psd_power) {
                        /* If AP local power constraint is present */
                        if (pwr_reduction)
                                eirp_power = eirp_power - pwr_reduction;

                        /* If FW updated max tx power is non zero, then take the min of
                         * firmware updated ap tx power
                         * and max power derived from above mentioned parameters.
                         */
			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
					"eirp power : %d firmware report power : %d\n",
					eirp_power, ar->max_allowed_tx_power);
                        if ((ar->max_allowed_tx_power) && (ab->hw_params->idle_ps))
                                eirp_power = min_t(s8,
                                                   eirp_power,
                                                   ar->max_allowed_tx_power);
                } else {
                        /* If AP local power constraint is present */
                        if (pwr_reduction)
                                max_tx_power[pwr_lvl_idx] =
                                        max_tx_power[pwr_lvl_idx] - pwr_reduction;
                        /* If FW updated max tx power is non zero, then take the min of
                         * firmware updated ap tx power
                         * and max power derived from above mentioned parameters.
                         */
                        if ((ar->max_allowed_tx_power) && (ab->hw_params->idle_ps))
                                max_tx_power[pwr_lvl_idx] =
                                        min_t(s8,
                                              max_tx_power[pwr_lvl_idx],
                                              ar->max_allowed_tx_power);
                }
                reg_tpc_info->chan_power_info[pwr_lvl_idx].chan_cfreq = center_freq;
                reg_tpc_info->chan_power_info[pwr_lvl_idx].tx_power =
                        max_tx_power[pwr_lvl_idx];
        }

        reg_tpc_info->num_pwr_levels = num_pwr_levels;
        reg_tpc_info->is_psd_power = is_psd_power;
        reg_tpc_info->eirp_power = eirp_power;
	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    bss_conf->power_type == IEEE80211_REG_SP_AP &&
	    !ar->afc.is_6ghz_afc_power_event_received)
		reg_tpc_info->power_type_6g = REG_SP_CLIENT_TYPE;
	else
		reg_tpc_info->power_type_6g =
			ath12k_ieee80211_ap_pwr_type_convert(reg_6g_power_mode);
}

static int
ath12k_mac_get_chan_width(enum nl80211_chan_width ch_width)
{
	switch (ch_width) {
	case NL80211_CHAN_WIDTH_320:
		return ATH12K_CHWIDTH_320;
	case NL80211_CHAN_WIDTH_160:
	case NL80211_CHAN_WIDTH_80P80:
		return ATH12K_CHWIDTH_160;
	case NL80211_CHAN_WIDTH_80:
		return ATH12K_CHWIDTH_80;
	case NL80211_CHAN_WIDTH_40:
		return ATH12K_CHWIDTH_40;
	default:
		return ATH12K_CHWIDTH_20;
	}
}

static void ath12k_mac_get_eirp_arr_for_6g(struct ath12k *ar,
					   struct cfg80211_chan_def *chan_def,
					   u8 reg_6g_power_mode,
					   s8 *max_eirp_arr,
					   u16 start_freq,
					   u16 oper_freq,
					   u32 *cfreqs)
{
	s8 max_reg_eirp = ATH12K_MAX_TX_POWER;
	s8 psd_eirp = ATH12K_MAX_TX_POWER;
	s8 afc_eirp = ATH12K_MAX_TX_POWER;
	u16 bw, max_bw;
	s8 reg_psd;
	u8 i;

	max_bw = ath12k_mac_get_chan_width(chan_def->width);

	for (i = 0, bw = ATH12K_CHWIDTH_20; bw <= max_bw; i++, bw *= 2) {
		s8 tx_power = ATH12K_MAX_TX_POWER;

		ath12k_reg_get_regulatory_pwrs(ar, MHZ_TO_KHZ(oper_freq),
					       reg_6g_power_mode - 1,
					       &max_reg_eirp, &reg_psd);

		if (chan_def->chan->flags & IEEE80211_CHAN_PSD)
			psd_eirp = ath12k_reg_psd_2_eirp(reg_psd, bw);

		tx_power = min(max_reg_eirp, psd_eirp);

		if (reg_6g_power_mode == IEEE80211_REG_SP_AP &&
		    ar->afc.is_6ghz_afc_power_event_received) {
			afc_eirp = ath12k_mac_get_afc_eirp_power(ar,
								 chan_def->chan->center_freq,
								 cfreqs[i], bw);
			tx_power = min(tx_power, afc_eirp);
		}

		max_eirp_arr[i] = tx_power;
	}
}

void
ath12k_mac_get_sp_client_power_for_connecting_ap(struct ath12k *ar,
						 struct ieee80211_chanctx_conf *ctx,
						 s8 *max_eirp_arr,
						 u8 num_pwr_levels)
{
	u16 bw = ATH12K_CHWIDTH_20;
	u16 center_freq = ctx->def.chan->center_freq;
	u8 pwr_lvl_idx;
	s8 sp_reg_eirp = ATH12K_MIN_TX_POWER, sp_reg_psd = ATH12K_MIN_TX_POWER;

	ath12k_reg_get_regulatory_pwrs(ar, MHZ_TO_KHZ(center_freq),
				       NL80211_REG_REGULAR_CLIENT_SP,
				       &sp_reg_eirp, &sp_reg_psd);

	for (pwr_lvl_idx = 0; pwr_lvl_idx < num_pwr_levels; pwr_lvl_idx++) {
		s8 tmp_eirp = ATH12K_MAX_TX_POWER;

		if (ctx->def.chan->flags & IEEE80211_CHAN_PSD)
			tmp_eirp = ath12k_reg_psd_2_eirp(sp_reg_psd, bw);

		max_eirp_arr[pwr_lvl_idx] = min(sp_reg_eirp, tmp_eirp);
		bw *= 2;
	}
}

static inline void ath12k_mac_fill_cfreqs(struct cfg80211_chan_def *chan_def,
					  u32 *cfreqs)
{
	cfreqs[0] = chan_def->chan->center_freq;
	if (chan_def->width != NL80211_CHAN_WIDTH_20)
		cfg80211_chandef_primary_freqs(chan_def, &cfreqs[1], &cfreqs[2], &cfreqs[3]);
	cfreqs[4] = chan_def->center_freq1;
}

void ath12k_mac_fill_reg_tpc_info_with_eirp_power(struct ath12k *ar,
						  struct ath12k_link_vif *arvif,
						  struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k_reg_tpc_power_info *reg_tpc_info = &arvif->reg_tpc_info;
	s8 sta_max_eirp_arr[ATH12K_MAX_EIRP_VALS];
	s8 ap_max_eirp_arr[ATH12K_MAX_EIRP_VALS];
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_bss_conf *bss_conf;
	u32 cfreqs[ATH12K_MAX_EIRP_VALS];
	u16 start_freq = 0, oper_freq = 0;
	bool is_tpe_present = false;
	u8 reg_6g_power_mode;
	u8 num_pwr_levels;
	u8 count;

	rcu_read_lock();

	bss_conf = ath12k_get_link_bss_conf(arvif);

	if (!bss_conf) {
		rcu_read_unlock();
		ath12k_warn(ar->ab, "unable to access bss link conf in tpc reg fill\n");
		return;
	}

	reg_6g_power_mode = bss_conf->power_type;
	if (reg_6g_power_mode == IEEE80211_REG_UNSET_AP)
		reg_6g_power_mode = IEEE80211_REG_LPI_AP;
	else if (reg_6g_power_mode == IEEE80211_REG_SP_AP &&
		 !ar->afc.is_6ghz_afc_power_event_received)
		reg_6g_power_mode = NL80211_REG_REGULAR_CLIENT_SP + 1;

	start_freq = ath12k_mac_get_6g_start_frequency(&ctx->def);
	oper_freq = ctx->def.chan->center_freq;

	rcu_read_unlock();

	num_pwr_levels = ath12k_mac_get_num_pwr_levels(&ctx->def, false);

	if (num_pwr_levels > ATH12K_MAX_EIRP_VALS) {
		ath12k_err(NULL, "num_pwr_levels should not be greater than ATH12K_MAX_EIRP_VALS");
		return;
	}

	ath12k_mac_fill_cfreqs(&ctx->def, cfreqs);
	if (reg_tpc_info->num_tpe_eirp)
		is_tpe_present = true;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"num tpe_eirp power levels: %u, num_pwr_levels = %u\n",
			reg_tpc_info->num_tpe_eirp, num_pwr_levels);

	if (!is_tpe_present)
		memset(reg_tpc_info->tpe_eirp, ATH12K_MAX_TX_POWER,
		       ATH12K_MAX_EIRP_VALS * sizeof(s8));

	ath12k_mac_get_eirp_arr_for_6g(ar, &ctx->def, reg_6g_power_mode,
				       ap_max_eirp_arr, start_freq,
				       oper_freq, cfreqs);

	/* In case of a Non-AFC capable SP client, calculate the EIRP values
	 * from regulatory client PSD
	 */
	if (bss_conf->power_type == IEEE80211_REG_SP_AP &&
	    !ar->afc.is_6ghz_afc_power_event_received) {
		ath12k_mac_get_sp_client_power_for_connecting_ap(ar, ctx,
								 sta_max_eirp_arr,
								 num_pwr_levels);
	} else {
		ath12k_mac_get_eirp_arr_for_6g(ar, &ctx->def, reg_6g_power_mode,
					       sta_max_eirp_arr, start_freq,
					       oper_freq, cfreqs);
	}

	for (count = 0; count < num_pwr_levels; count++) {
		s8 sta_tx_pwr;
		s8 tx_power, max_of_ap_sta_tx_pwr, ap_tx_pwr;

		ap_tx_pwr = (ahvif->vdev_type == WMI_VDEV_TYPE_STA) ? 0 : ap_max_eirp_arr[count];
		sta_tx_pwr = sta_max_eirp_arr[count];
		/* Generally, 6GHz client power is less than 6GHz AP power.
		 * In repeater, we have access tp both client and AP power.
		 * Therefore, take advantage of the maximum of AP and client power.
		 */
		max_of_ap_sta_tx_pwr = max(ap_tx_pwr, sta_tx_pwr);
		if (reg_6g_power_mode == IEEE80211_REG_SP_AP &&
		    ar->afc.is_6ghz_afc_power_event_received)
			tx_power = max_of_ap_sta_tx_pwr;
		else
			tx_power = min(reg_tpc_info->tpe_eirp[count], max_of_ap_sta_tx_pwr);

		reg_tpc_info->chan_power_info[count].chan_cfreq = cfreqs[count];
		reg_tpc_info->chan_power_info[count].tx_power = tx_power;
	}

	reg_tpc_info->num_pwr_levels = num_pwr_levels;
	reg_tpc_info->is_psd_power = false;
	reg_tpc_info->eirp_power = 0;
	/* In case of a Non-AFC capable SP client, fill the power_type as REG_SP_CLIENT_TYPE */
	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    bss_conf->power_type == IEEE80211_REG_SP_AP &&
	    !ar->afc.is_6ghz_afc_power_event_received)
		reg_tpc_info->power_type_6g = REG_SP_CLIENT_TYPE;
	else
		reg_tpc_info->power_type_6g =
			ath12k_ieee80211_ap_pwr_type_convert(reg_6g_power_mode);
}

void ath12k_mac_parse_tx_pwr_env(struct ath12k *ar,
				 struct ath12k_link_vif *arvif)
{
	struct ieee80211_bss_conf *bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	struct ath12k_reg_tpc_power_info *tpc_info = &arvif->reg_tpc_info;
	struct ieee80211_parsed_tpe_eirp *local_non_psd, *reg_non_psd, *additional_non_psd;
	struct ieee80211_parsed_tpe_psd *local_psd, *reg_psd, *additional_psd;
	struct ieee80211_parsed_tpe *tpe = &bss_conf->tpe;
	enum wmi_reg_6g_client_type client_type;
	struct ath12k_base *ab = ar->ab;
	bool psd_valid, non_psd_valid;
	int i;
	enum ieee80211_ap_reg_power root_ap_power_type = bss_conf->power_type;
	bool is_afc_power_event_received = ar->afc.is_6ghz_afc_power_event_received;

	memset(tpc_info, 0, sizeof(*tpc_info));

	if (root_ap_power_type != IEEE80211_REG_SP_AP) {
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "It is not required to parse TPE for root AP power type %d\n",
			   root_ap_power_type);
		return;
	}
	if (is_afc_power_event_received) {
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "It is not required to parse TPE for SP client as AFC power event is received\n");
		return;
	}

	client_type = ieee80211_get_6ghz_client_type(ath12k_ar_to_hw(ar)->wiphy,
						     bss_conf->power_type - 1);
	if (client_type == WMI_REG_SUBORDINATE_CLIENT &&
	    bss_conf->power_type - 1 == NL80211_REG_AP_SP &&
	    ar->ab->sp_rule)
		client_type = WMI_REG_DEFAULT_CLIENT;

	local_psd = &tpe->psd_local[client_type];
	reg_psd = &tpe->psd_reg_client[client_type];
	local_non_psd = &tpe->max_local[client_type];
	reg_non_psd = &tpe->max_reg_client[client_type];
	additional_psd = &tpe->additional_psd_reg_client[client_type];
	additional_non_psd = &tpe->additional_max_reg_client[client_type];

	psd_valid = local_psd->valid | reg_psd->valid | additional_psd->valid;
	non_psd_valid = local_non_psd->valid | reg_non_psd->valid | additional_non_psd->valid;

	if (!psd_valid && !non_psd_valid) {
		ath12k_warn(ab,
			    "no transmit power envelope match client power type %d\n",
			    client_type);
		return;
	};

	if (psd_valid) {
		tpc_info->is_psd_power = true;

		if (additional_psd->valid) {
			tpc_info->num_tpe_psd = max(local_psd->count,
						    additional_psd->count);
			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"TPE PSD power levels count %d, additional count %d\n",
					local_psd->count, additional_psd->count);
		} else {
			tpc_info->num_tpe_psd = max(local_psd->count,
						    reg_psd->count);
			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"TPE PSD power levels count %d, reg_psd count %d\n",
					local_psd->count, reg_psd->count);
		}
		if (tpc_info->num_tpe_psd > ATH12K_NUM_PWR_LEVELS)
			tpc_info->num_tpe_psd = ATH12K_NUM_PWR_LEVELS;

		for (i = 0; i < tpc_info->num_tpe_psd; i++) {
			if (additional_psd->valid) {
				tpc_info->tpe_psd[i] = min(local_psd->power[i],
							   additional_psd->power[i]) / 2;
				ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
						"TPE PSD power[%d] : %d, local psd power : %d, additional psd power : %d\n",
						i, tpc_info->tpe_psd[i],
						local_psd->power[i],
						additional_psd->power[i]);
			} else {
				tpc_info->tpe_psd[i] = min(local_psd->power[i],
							   reg_psd->power[i]) / 2;
				ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
						"TPE PSD power[%d] : %d, local psd power : %d, reg psd power : %d\n",
						i, tpc_info->tpe_psd[i],
						local_psd->power[i],
						reg_psd->power[i]);
			}
		}
	}
	if (non_psd_valid) {
		tpc_info->is_psd_power = false;
		tpc_info->eirp_power = 0;

		if (additional_non_psd->valid) {
			tpc_info->num_tpe_eirp = max(local_non_psd->count,
						     additional_non_psd->count);
			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"TPE non PSD power levels count %d, additional count %d\n",
					local_non_psd->count, additional_non_psd->count);
		} else {
			tpc_info->num_tpe_eirp = max(local_non_psd->count,
						     reg_non_psd->count);
			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"TPE non PSD power levels count %d, reg_non_psd count %d\n",
					local_non_psd->count, reg_non_psd->count);
		}
		if (tpc_info->num_tpe_eirp > ATH12K_MAX_EIRP_VALS)
			tpc_info->num_tpe_eirp = ATH12K_MAX_EIRP_VALS;

		for (i = 0; i < tpc_info->num_tpe_eirp; i++) {
			if (additional_non_psd->valid) {
				tpc_info->tpe_eirp[i] = min(local_non_psd->power[i],
							    additional_non_psd->power[i]) / 2;
				ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
						"TPE non PSD power[%d] : %d, local non psd power : %d, additional non psd power : %d\n",
						i, tpc_info->tpe_eirp[i],
						local_non_psd->power[i],
						additional_non_psd->power[i]);
			} else {
				tpc_info->tpe_eirp[i] = min(local_non_psd->power[i],
							    reg_non_psd->power[i]) / 2;
				ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
						"TPE non PSD power[%d] : %d, local non psd power : %d, reg non psd power : %d\n",
						i, tpc_info->tpe_eirp[i],
						local_non_psd->power[i],
						reg_non_psd->power[i]);
			}
		}
	}
}

static int
ath12k_mac_set_peer_eht_fixed_rate(struct ath12k_link_vif *arvif,
				   struct ath12k_link_sta *arsta,
				   const struct cfg80211_bitrate_mask *mask,
				   enum nl80211_band band)
{
	struct ath12k *ar = arvif->ar;
	u8 eht_rate, nss;
	u32 rate_code;
	int ret, i;
	struct ath12k_sta *ahsta = arsta->ahsta;
	struct ieee80211_sta *sta;
	struct ieee80211_link_sta *link_sta;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	sta = ath12k_ahsta_to_sta(ahsta);
	nss = 0;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].eht_mcs); i++) {
		if (hweight16(mask->control[band].eht_mcs[i]) == 1) {
			nss = i + 1;
			eht_rate = ffs(mask->control[band].eht_mcs[i]) - 1;
		}
	}

	if (!nss) {
		ath12k_warn(ar->ab, "No single EHT Fixed rate found to set for %pM",
			    arsta->addr);
		return -EINVAL;
	}

	/* Avoid updating invalid nss as fixed rate*/
	if (!arsta->is_bridge_peer) {
		rcu_read_lock();
		link_sta = rcu_dereference(sta->link[arsta->link_id]);
		if (!link_sta || nss > link_sta->rx_nss) {
			rcu_read_unlock();
			return -EINVAL;
		}
		rcu_read_unlock();
	} else {
		if (nss > sta->deflink.rx_nss)
			return -EINVAL;
	}
	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"Setting Fixed EHT Rate for peer %pM. Device will not switch to any other selected rates",
			arsta->addr);

	rate_code = ATH12K_HW_RATE_CODE(eht_rate, nss - 1,
					WMI_RATE_PREAMBLE_EHT);

	ret = ath12k_wmi_set_peer_param(ar, arsta->addr,
					arvif->vdev_id,
					WMI_PEER_PARAM_FIXED_RATE,
					rate_code);
	if (ret)
		ath12k_warn(ar->ab,
			    "failed to update STA %pM Fixed Rate %d: %d\n",
			    arsta->addr, rate_code, ret);

	return ret;
}

int ath12k_mac_vendor_send_disassoc_event(struct ath12k_link_sta *arsta,
					  struct ieee80211_link_sta *link_sta)
{
	struct ath12k_vendor_generic_peer_assoc_event vend_event = {0};
	struct ath12k_sta *ahsta;

	if (!link_sta || !arsta)
		return -EINVAL;

	ahsta = arsta->ahsta;

	vend_event.category = QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_DISASSOC;

	rcu_read_lock();
	ath12k_peer_assoc_build_vendor_event(ahsta, link_sta,
					     &vend_event);
	rcu_read_unlock();

	if (ath12k_vendor_send_assoc_event(&vend_event, vend_event.category))
		return -EINVAL;

	return 0;
}

int ath12k_mac_vendor_send_assoc_event(struct ath12k_link_sta *arsta,
				       struct ieee80211_link_sta *link_sta,
				       bool reassoc)
{
	struct ath12k_vendor_generic_peer_assoc_event vend_event = {0};
	struct ath12k_sta *ahsta;

	if (!link_sta || !arsta)
		return -EINVAL;

	ahsta = arsta->ahsta;

	vend_event.category = QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO;

	rcu_read_lock();
	ath12k_peer_assoc_build_vendor_event(ahsta, link_sta,
					     &vend_event);
	rcu_read_unlock();

	if (ath12k_vendor_send_assoc_event(&vend_event, vend_event.category))
		return -EINVAL;

	return 0;
}

static int ath12k_mac_station_assoc(struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    struct ath12k_link_sta *arsta,
				    bool reassoc)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ieee80211_link_sta *link_sta;
	int ret;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	struct cfg80211_bitrate_mask *mask;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);
	u8 num_vht_rates, num_he_rates, num_ht_rates, num_eht_rates;
	bool ht_supp, vht_supp, has_he, has_eht;
	u8 link_id = arvif->link_id;
	u32 bandwidth;
	struct ieee80211_sta_ht_cap ht_cap;
	struct ieee80211_sta_he_cap he_cap;
	struct ieee80211_he_6ghz_capa he_6ghz_cap;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return -EPERM;

	if (!arsta->is_bridge_peer &&
	    WARN_ON(!rcu_access_pointer(sta->link[link_id])))
		return -EINVAL;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;
	mask = &arvif->bitrate_mask;

	struct ath12k_wmi_peer_assoc_arg *peer_arg __free(kfree) =
		kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return -ENOMEM;

	link_sta = arsta->is_bridge_peer ? ath12k_mac_inherit_radio_cap(ar, arsta) :
		   ath12k_mac_get_link_sta(arsta);
	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in station assoc\n");
		return -EINVAL;
	}

	bandwidth = ath12k_mac_ieee80211_sta_bw_to_wmi(ar, link_sta);
	ht_cap = link_sta->ht_cap;
	he_cap = link_sta->he_cap;
	ht_supp = link_sta->ht_cap.ht_supported;
	vht_supp = link_sta->vht_cap.vht_supported;
	has_he = link_sta->he_cap.has_he;
	has_eht = link_sta->eht_cap.has_eht;
	he_6ghz_cap = link_sta->he_6ghz_capa;

	ath12k_peer_assoc_prepare(ar, arvif, arsta, peer_arg, reassoc, link_sta);

	if (arsta->is_bridge_peer)
		kfree(link_sta);

	if (peer_arg->peer_nss < 1) {
		ath12k_warn(ar->ab,
			    "invalid peer NSS %d\n", peer_arg->peer_nss);
		return -EINVAL;
	}
	peer_arg->is_assoc = true;
	ret = ath12k_wmi_send_peer_assoc_cmd(ar, peer_arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to run peer assoc for STA %pM vdev %i: %d\n",
			    arsta->addr, arvif->vdev_id, ret);
		return ret;
	}

	if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
		ath12k_warn(ar->ab, "failed to get peer assoc conf event for %pM vdev %i\n",
			    arsta->addr, arvif->vdev_id);
		return -ETIMEDOUT;
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, arsta->arvif->vdev_id, arsta->addr);
	if (!reassoc && peer && !peer->assoc_success) {
		ath12k_warn(ar->ab, "peer assoc failure from firmware %pM\n", arsta->addr);
		spin_unlock_bh(&dp->dp_lock);
		return -EINVAL;
	}
	spin_unlock_bh(&dp->dp_lock);

	num_vht_rates = ath12k_mac_bitrate_mask_num_vht_rates(ar, band, mask);
	num_he_rates = ath12k_mac_bitrate_mask_num_he_rates(ar, band, mask);
	num_ht_rates = ath12k_mac_bitrate_mask_num_ht_rates(ar, band, mask);
	num_eht_rates = ath12k_mac_bitrate_mask_num_eht_rates(ar, band, mask);

	/* If single VHT/HE/EHT rate is configured (by set_bitrate_mask()),
	 * peer_assoc will disable VHT/HE/EHT. This is now enabled by a peer specific
	 * fixed param.
	 * Note that all other rates and NSS will be disabled for this peer.
	 */

	spin_lock_bh(&ar->data_lock);
	arsta->bw = bandwidth;
	spin_unlock_bh(&ar->data_lock);

	ath12k_mac_vendor_send_assoc_event(arsta, link_sta, reassoc);

	if (vht_supp && num_vht_rates == 1) {
		ret = ath12k_mac_set_peer_vht_fixed_rate(arvif, arsta, mask, band);
	} else if (has_he && num_he_rates == 1) {
		ret = ath12k_mac_set_peer_he_fixed_rate(arvif, arsta, mask, band);
		if (ret)
			return ret;
	} else if (ht_supp && num_ht_rates == 1) {
		ret = ath12k_mac_set_peer_ht_fixed_rate(arvif, arsta, mask,
							band);
		if (ret)
			return ret;
	} else if (has_eht && num_eht_rates == 1) {
		ret = ath12k_mac_set_peer_eht_fixed_rate(arvif, arsta, mask, band);
		if (ret)
			return ret;
	}

	/* Re-assoc is run only to update supported rates for given station. It
	 * doesn't make much sense to reconfigure the peer completely.
	 */
	if (reassoc)
		return 0;

	ret = ath12k_setup_peer_smps(ar, arvif, arsta->addr,
				     &ht_cap,
				     &he_cap,
				     &he_6ghz_cap);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup peer SMPS for vdev %d: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	if (!sta->wme) {
		arvif->num_legacy_stations++;
		ret = ath12k_recalc_rtscts_prot(arvif);
		if (ret)
			return ret;
	}

	if (sta->wme && sta->uapsd_queues) {
		ret = ath12k_peer_assoc_qos_ap(ar, arvif, arsta);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set qos params for STA %pM for vdev %i: %d\n",
				    arsta->addr, arvif->vdev_id, ret);
			return ret;
		}
	}

	spin_lock_bh(&ar->data_lock);
	arvif->num_stations++;
	spin_unlock_bh(&ar->data_lock);

	ar->dp.stats.telemetry_stats.time_last_assoc = ktime_get_real_seconds();

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac station %pM connected to vdev %u. num_stations=%u\n",
		   arsta->addr,  arvif->vdev_id, arvif->num_stations);

	/* Trigger AP powersave recal for first peer create */
	if (ar->ap_ps_enabled) {
		ath12k_mac_ap_ps_recalc(ar);
	}

	return 0;
}

static int ath12k_mac_station_disassoc(struct ath12k *ar,
				       struct ath12k_link_vif *arvif,
				       struct ath12k_link_sta *arsta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ieee80211_link_sta *link_sta;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	rcu_read_lock();

	link_sta = arsta->is_bridge_peer ? ath12k_mac_inherit_radio_cap(ar, arsta) :
		   ath12k_mac_get_link_sta(arsta);
	rcu_read_unlock();

	spin_lock_bh(&arvif->ar->data_lock);

	if (!arvif->num_stations) {
		ath12k_warn(ar->ab,
			    "mac station disassoc for vdev %u which does not have any station connected\n",
			    arvif->vdev_id);
	} else {
		arvif->num_stations--;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "mac station %pM disconnected from vdev %u. num_stations=%u\n",
			   arsta->addr, arvif->vdev_id, arvif->num_stations);
	}

	spin_unlock_bh(&arvif->ar->data_lock);

	if (link_sta)
		ath12k_mac_vendor_send_disassoc_event(arsta, link_sta);

	if (!sta->wme) {
		arvif->num_legacy_stations--;
		return ath12k_recalc_rtscts_prot(arvif);
	}

	return 0;
}

static u16 ath12k_mac_set_punct_bitmap_device(u32 oper_freq,
					      enum nl80211_chan_width width_device,
					      u32 device_freq, u16 oper_punct_bitmap)
{
	if (oper_freq == device_freq || oper_freq < device_freq)
		return oper_punct_bitmap;

	switch (width_device) {
	case NL80211_CHAN_WIDTH_160:
		return (oper_punct_bitmap << 4);
	case NL80211_CHAN_WIDTH_320:
		return (oper_punct_bitmap << 8);
	default:
		return oper_punct_bitmap;
	}
}

static int ath12k_mac_set_peer_ch_switch_data(struct ath12k_link_vif *arvif,
					      struct ath12k_link_sta *arsta)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_peer_ch_width_switch_data *peer_data;
	struct wmi_chan_width_peer_arg *peer_arg;
	struct ieee80211_link_sta *link_sta;
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct cfg80211_chan_def def;
	u16 ru_punct_bitmap;
	bool is_bridge_vdev;
	int num_sta_count;

	if (!ar->ab->chwidth_num_peer_caps)
		return -EOPNOTSUPP;

	is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);
	if (!is_bridge_vdev &&
	     WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return -EINVAL;

	if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		num_sta_count = 1;
	else
		num_sta_count = arvif->num_stations;

	peer_data = arvif->peer_ch_width_switch_data;

	if (!peer_data) {
		peer_data = kzalloc(struct_size(peer_data, peer_arg,
						num_sta_count),
				    GFP_KERNEL);
		if (!peer_data)
			return -ENOMEM;

		peer_data->count = 0;
		arvif->peer_ch_width_switch_data = peer_data;
	}

	peer_arg = &peer_data->peer_arg[peer_data->count++];

	ru_punct_bitmap = 0;

	rcu_read_lock();
	link_sta = ath12k_mac_get_link_sta(arsta);

	if (!is_bridge_vdev && link_sta) {
		if (link_sta->he_cap.has_he && link_sta->eht_cap.has_eht)
			ru_punct_bitmap = def.punctured;

		if (ieee80211_vif_is_mesh(vif) && link_sta->punctured)
			ru_punct_bitmap = link_sta->punctured;
	}

	rcu_read_unlock();

	if (!is_bridge_vdev &&
	    (test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map) &&
	    cfg80211_chandef_device_present(&def))) {
		ru_punct_bitmap = ath12k_mac_set_punct_bitmap_device(def.chan->center_freq,
								     def.width_device,
								     def.center_freq_device,
								     def.punctured);
	}

	spin_lock_bh(&ar->data_lock);
	ether_addr_copy(peer_arg->mac_addr.addr, arsta->addr);
	peer_arg->chan_width = arsta->bw;
	peer_arg->puncture_20mhz_bitmap = ~ru_punct_bitmap;
	spin_unlock_bh(&ar->data_lock);

	if (peer_data->count == 1) {
		reinit_completion(&arvif->peer_ch_width_switch_send);
		wiphy_work_queue(ar->ah->hw->wiphy, &arvif->peer_ch_width_switch_work);
	}

	if (peer_data->count == num_sta_count)
		complete(&arvif->peer_ch_width_switch_send);

	return 0;
}

static void ath12k_sta_rc_update_wk(struct wiphy *wiphy, struct wiphy_work *wk)
{
	struct ieee80211_link_sta *link_sta;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	struct cfg80211_chan_def def;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	const u16 *vht_mcs_mask;
	const u16 *he_mcs_mask;
	const u16 *eht_mcs_mask;
	u32 changed, bw, nss, mac_nss, smps;
	int err, num_vht_rates, num_he_rates, num_ht_rates, num_eht_rates;
	const struct cfg80211_bitrate_mask *mask;
	struct ath12k_link_sta *arsta;
	struct ieee80211_vif *vif;

	lockdep_assert_wiphy(wiphy);

	arsta = container_of(wk, struct ath12k_link_sta, update_wk);
	sta = ath12k_ahsta_to_sta(arsta->ahsta);
	arvif = arsta->arvif;
	vif = ath12k_ahvif_to_vif(arvif->ahvif);
	ar = arvif->ar;

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    WARN_ON(ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)))
		return;

	if (ath12k_mac_is_bridge_vdev(arvif))
		band = ath12k_get_band_based_on_freq(ar->chan_info.low_freq);
	else
		band = def.chan->band;

	ht_mcs_mask = arvif->bitrate_mask.control[band].ht_mcs;
	vht_mcs_mask = arvif->bitrate_mask.control[band].vht_mcs;
	he_mcs_mask = arvif->bitrate_mask.control[band].he_mcs;
	eht_mcs_mask = arvif->bitrate_mask.control[band].eht_mcs;

	spin_lock_bh(&ar->data_lock);

	changed = arsta->changed;
	arsta->changed = 0;

	bw = arsta->bw;
	nss = arsta->nss;
	smps = arsta->smps;
	spin_unlock_bh(&ar->data_lock);

	nss = max_t(u32, 1, nss);
	mac_nss = max3(ath12k_mac_max_ht_nss(ht_mcs_mask),
		       ath12k_mac_max_vht_nss(vht_mcs_mask),
		       ath12k_mac_max_he_nss(he_mcs_mask));
	mac_nss = max(mac_nss,
		      ath12k_mac_max_eht_nss(eht_mcs_mask));
	nss = min(nss, mac_nss);

	struct ath12k_wmi_peer_assoc_arg *peer_arg __free(kfree) =
					kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return;

	if (changed & IEEE80211_RC_BW_CHANGED) {
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
				"mac bandwidth upgrade for sta %pM new %d\n",
				arsta->addr, bw);

		err = ath12k_mac_set_peer_ch_switch_data(arvif, arsta);
		if (!err || err == -EINVAL)
			return;

		err = ath12k_wmi_set_peer_param(ar, sta->addr,
						arvif->vdev_id, WMI_PEER_CHWIDTH,
						bw);
		if (err)
			ath12k_warn(ar->ab, "failed to update STA %pM to peer bandwidth %d: %d\n",
				    arsta->addr, bw, err);
	}

	if (changed & IEEE80211_RC_NSS_CHANGED) {
		ath12k_dbg(ar->ab, ATH12K_DBG_PEER, "mac update sta %pM nss %d\n",
			   arsta->addr, nss);

		err = ath12k_wmi_set_peer_param(ar, arsta->addr, arvif->vdev_id,
						WMI_PEER_NSS, nss);
		if (err)
			ath12k_warn(ar->ab, "failed to update STA %pM nss %d: %d\n",
				    arsta->addr, nss, err);
	}

	if (changed & IEEE80211_RC_SMPS_CHANGED) {
		ath12k_dbg(ar->ab, ATH12K_DBG_PEER, "mac update sta %pM smps %d\n",
			   arsta->addr, smps);

		err = ath12k_wmi_set_peer_param(ar, arsta->addr, arvif->vdev_id,
						WMI_PEER_MIMO_PS_STATE, smps);
		if (err)
			ath12k_warn(ar->ab, "failed to update STA %pM smps %d: %d\n",
				    arsta->addr, smps, err);
	}

	if (changed & IEEE80211_RC_SUPP_RATES_CHANGED) {
		if (arsta->disable_fixed_rate) {
			err = ath12k_wmi_set_peer_param(ar, arsta->addr,
							arvif->vdev_id,
							WMI_PEER_PARAM_FIXED_RATE,
							WMI_FIXED_RATE_NONE);
			if (err)
				ath12k_warn(ar->ab,
					    "failed to disable peer fixed rate for STA %pM ret %d\n",
					    arsta->addr, err);

			arsta->disable_fixed_rate = false;
		}
		mask = &arvif->bitrate_mask;
		num_ht_rates = ath12k_mac_bitrate_mask_num_ht_rates(ar, band,
								    mask);
		num_vht_rates = ath12k_mac_bitrate_mask_num_vht_rates(ar, band,
								      mask);
		num_he_rates = ath12k_mac_bitrate_mask_num_he_rates(ar, band,
								    mask);
		num_eht_rates = ath12k_mac_bitrate_mask_num_eht_rates(ar, band,
								      mask);

		/* Peer_assoc_prepare will reject vht rates in
		 * bitrate_mask if its not available in range format and
		 * sets vht tx_rateset as unsupported. So multiple VHT MCS
		 * setting(eg. MCS 4,5,6) per peer is not supported here.
		 * But, Single rate in VHT mask can be set as per-peer
		 * fixed rate. But even if any HT rates are configured in
		 * the bitrate mask, device will not switch to those rates
		 * when per-peer Fixed rate is set.
		 * TODO: Check RATEMASK_CMDID to support auto rates selection
		 * across HT/VHT and for multiple VHT MCS support.
		 */
		link_sta = arsta->is_bridge_peer ? ath12k_mac_inherit_radio_cap(ar, arsta) :
			   ath12k_mac_get_link_sta(arsta);
		if (!link_sta) {
			ath12k_warn(ar->ab, "unable to access link sta in peer assoc he for sta %pM link %u\n",
				    sta->addr, arsta->link_id);
			return;
		}

		if (link_sta->vht_cap.vht_supported && num_vht_rates == 1) {
			ath12k_mac_set_peer_vht_fixed_rate(arvif, arsta, mask,
							   band);
		} else if (link_sta->he_cap.has_he && num_he_rates == 1) {
			ath12k_mac_set_peer_he_fixed_rate(arvif, arsta, mask, band);
		} else if (link_sta->ht_cap.ht_supported && num_ht_rates == 1) {
			ath12k_mac_set_peer_ht_fixed_rate(arvif, arsta, mask, band);
		} else if (link_sta->eht_cap.has_eht && num_eht_rates == 1) {
			ath12k_mac_set_peer_eht_fixed_rate(arvif, arsta, mask, band);
		} else {
			/* If the peer is non-VHT/HE/EHT or no fixed VHT/HE/EHT rate
			 * is provided in the new bitrate mask we set the
			 * other rates using peer_assoc command. Also clear
			 * the peer fixed rate settings as it has higher proprity
			 * than peer assoc
			 */
			err = ath12k_wmi_set_peer_param(ar, arsta->addr,
							arvif->vdev_id,
							WMI_PEER_PARAM_FIXED_RATE,
							WMI_FIXED_RATE_NONE);
			if (err)
				ath12k_warn(ar->ab,
					    "failed to disable peer fixed rate for STA %pM ret %d\n",
					    arsta->addr, err);
			/* Do not initiate peer assoc with the reassoc flag
			 * unless the new assoc has been successfully established
			 */
			if (arsta->ahsta->state < IEEE80211_STA_ASSOC)
				goto exit;

			ath12k_peer_assoc_prepare(ar, arvif, arsta,
						  peer_arg, true, link_sta);

			peer_arg->is_assoc = false;
			err = ath12k_wmi_send_peer_assoc_cmd(ar, peer_arg);
			if (err)
				ath12k_warn(ar->ab, "failed to run peer assoc for STA %pM vdev %i: %d\n",
					    arsta->addr, arvif->vdev_id, err);

			if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ))
				ath12k_warn(ar->ab, "failed to get peer assoc conf event for %pM vdev %i\n",
					    arsta->addr, arvif->vdev_id);
		}
exit:
		if (arsta->is_bridge_peer)
			kfree(link_sta);
	}
}

static void ath12k_mac_free_unassign_link_sta(struct ath12k_hw *ah,
					      struct ath12k_sta *ahsta,
					      u8 link_id)
{
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif = ahsta->link[link_id]->arvif;
	struct ath12k_base *ab = arvif->ar->ab;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (WARN_ON(link_id >= ATH12K_NUM_MAX_LINKS))
		return;

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (WARN_ON(!arsta))
		return;

	ahsta->links_map &= ~BIT(link_id);
	ahsta->device_bitmap &= ~BIT(ab->wsi_info.index);
	ahsta->mlo_hw_link_id_bitmap &= ~BIT(arvif->ar->pdev->hw_link_id);
	ahsta->num_peer--;
	ahsta->free_logical_idx_map |= BIT(arsta->link_idx);
	rcu_assign_pointer(ahsta->link[link_id], NULL);
	synchronize_rcu();

	if (arsta == &ahsta->deflink) {
		arsta->link_id = ATH12K_INVALID_LINK_ID;
		arsta->ahsta = NULL;
		arsta->arvif = NULL;
		return;
	}

	kfree(arsta);
}

static void ath12k_sta_set_4addr_wk(struct wiphy *wiphy, struct wiphy_work *wk)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct ath12k_sta *ahsta;
	struct ath12k_vif *ahvif;
	struct ath12k_link_sta *arsta;
	struct ieee80211_sta *sta;
	unsigned long links;
	int ret = 0;
	u8 link_id;

	ahsta = container_of(wk, struct ath12k_sta, set_4addr_wk);
	sta = container_of((void *)ahsta, struct ieee80211_sta, drv_priv);
	links = ahsta->links_map;

	if (ahsta->vlan_iface)
		ath12k_ppe_ds_attach_vlan_vif_link(ahsta->vlan_iface, ahsta->ppe_vp_num);

	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arsta = rcu_dereference(ahsta->link[link_id]);
		arvif = arsta->arvif;
		ahvif = arvif->ahvif;
		ar = arvif->ar;

		ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
			   "setting USE_4ADDR for peer %pM\n", arsta->addr);

		if (!arvif->set_wds_vdev_param) {
			ath12k_wmi_set_peer_param(ar, arsta->addr,
						  arvif->vdev_id,
						  WMI_PEER_USE_4ADDR,
						  WMI_PEER_4ADDR_ALLOW_EAPOL_DATA_FRAME);
		}
		spin_lock_bh(&ar->ab->dp->dp_lock);
		peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ar->ab->dp, arvif->vdev_id,
								    arsta->addr);
		if (peer) {
			arsta->tcl_metadata = peer->tcl_metadata;
			arsta->ast_hash = peer->ast_hash;
			arsta->ast_idx = peer->hw_peer_id;
			if (peer->dp_peer) {
				peer->dp_peer->vdev_type_4addr |= BIT(peer->vif->type);
				peer->dp_peer->is_reset_mcbc = true;
				peer->dp_peer->use_4addr = true;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
				peer->dp_peer->ppe_vp_num = ahsta->ppe_vp_num;
#endif
				if (peer->vif->type == NL80211_IFTYPE_AP)
					peer->dp_peer->dev = peer->dp_peer->sta->dev;
			}
		}

		spin_unlock_bh(&ar->ab->dp->dp_lock);

		if (ahvif->dp_vif.tx_encap_type != ATH12K_HW_TXRX_ETHERNET)
			continue;

		ath12k_dp_peer_ppeds_route_setup(ar, arvif, arsta);

		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
                                                    WMI_VDEV_PARAM_AP_ENABLE_NAWDS,
                                                    1);
                arvif->nawds_support = true;
	}
}

static int ath12k_mac_inc_num_stations(struct ath12k_link_vif *arvif,
				       struct ath12k_link_sta *arsta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ath12k *ar = arvif->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_STA && !sta->tdls)
		return 0;

	if (ar->num_stations >= ar->max_num_stations)
		return -ENOBUFS;

	ar->num_stations++;

	return 0;
}

static void ath12k_mac_station_post_remove(struct ath12k *ar,
					   struct ath12k_link_vif *arvif,
					   struct ath12k_link_sta *arsta)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_mac_dec_num_stations(arvif, arsta);

	spin_lock_bh(&ar->ab->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ar->ab->dp, arvif->vdev_id,
							    arsta->addr);
	if (peer && peer->sta == sta) {
		ath12k_warn(ar->ab, "Found peer entry %pM n vdev %i after it was supposedly removed"
			    "num_peers: %d \n",
			    vif->addr, arvif->vdev_id, ar->num_peers);
		peer->sta = NULL;
		ath12k_link_peer_free(peer);
		ar->num_peers--;
	}

	spin_unlock_bh(&ar->ab->dp->dp_lock);
	ath12k_mac_ap_ps_recalc(ar);
}

static int ath12k_mac_station_unauthorize(struct ath12k *ar,
					  struct ath12k_link_vif *arvif,
					  struct ath12k_link_sta *arsta)
{
	struct ath12k_dp_link_peer *peer;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->ab->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ar->ab->dp, arvif->vdev_id,
							    arsta->addr);
	if (peer) {
		peer->is_authorized = false;
		if (peer->dp_peer)
			peer->dp_peer->is_authorized = false;
	}

	spin_unlock_bh(&ar->ab->dp->dp_lock);

	/* Driver must clear the keys during the state change from
	 * IEEE80211_STA_AUTHORIZED to IEEE80211_STA_ASSOC, since after
	 * returning from here, mac80211 is going to delete the keys
	 * in __sta_info_destroy_part2(). This will ensure that the driver does
	 * not retain stale key references after mac80211 deletes the keys.
	 */
	ret = ath12k_clear_peer_keys(arvif, arsta->addr);
	if (ret) {
		ath12k_warn(ar->ab, "failed to clear all peer keys for vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "ar->cfg_tx_chainmask: %d (%x) ar->cfg_rx_chainmask: %d (%x)\n",
		   ar->cfg_tx_chainmask, ar->num_tx_chains,
		   ar->cfg_rx_chainmask, ar->num_rx_chains);

	return 0;
}

static int ath12k_mac_station_authorize(struct ath12k *ar,
					struct ath12k_link_vif *arvif,
					struct ath12k_link_sta *arsta)
{
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ath12k_sta *ahsta = arsta->ahsta;
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->ab->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ar->ab->dp, arvif->vdev_id,
							    arsta->addr);
	if (peer) {
		peer->is_authorized = true;
		if (peer->dp_peer)
			peer->dp_peer->is_authorized = true;
	}

	spin_unlock_bh(&ar->ab->dp->dp_lock);

	if ((vif->type == NL80211_IFTYPE_STATION ||
	     sta->reconf.added_links) && arvif->is_up) {
		ret = ath12k_wmi_set_peer_param(ar, arsta->addr,
						arvif->vdev_id,
						WMI_PEER_AUTHORIZE,
						1);
		if (ret) {
			ath12k_warn(ar->ab, "Unable to authorize peer %pM vdev %d: %d\n",
				    arsta->addr, arvif->vdev_id, ret);
			return ret;
		}
	}

	return 0;
}

static int ath12k_mac_station_remove(struct ath12k *ar,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_link_sta *arsta)
{
	struct ath12k_sta *ahsta = arsta->ahsta;
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	struct ath12k_vif *ahvif = arvif->ahvif;
	int ret = 0;
	struct ath12k_link_sta *temp_arsta = NULL;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	wiphy_work_cancel(ar->ah->hw->wiphy, &arsta->update_wk);

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		WARN_ON(!arvif->is_started);
		ath12k_bss_disassoc(ar, arvif);

		ret = ath12k_mac_vdev_stop(arvif);
		if (ret)
			ath12k_warn(ar->ab, "failed to stop vdev %i: %d\n",
				    arvif->vdev_id, ret);
		arvif->is_started = false;
	}

	if (sta->mlo)
		return ret;

	ath12k_dp_peer_cleanup(ar, arvif->vdev_id, arsta->addr);

	ret = ath12k_peer_delete(ar, arvif->vdev_id, arsta->addr, ahsta);
	if (ret)
		ath12k_warn(ar->ab, "Failed to delete peer: %pM for VDEV: %d num_peers: %d\n",
			    arsta->addr, arvif->vdev_id, ar->num_peers);
	else
		ath12k_dbg(ar->ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME,
			   "Removed peer: %pM for VDEV: %d num_peers:%d\n",
			   arsta->addr, arvif->vdev_id, ar->num_peers);

	ath12k_mac_station_post_remove(ar, arvif, arsta);

	ath12k_cfr_decrement_peer_count(ar, arsta);

	spin_lock_bh(&ar->ab->base_lock);

	/* To handle roaming and split phy scenario */
	temp_arsta = ath12k_link_sta_find_by_addr(ar->ab, arsta->addr);
	if (temp_arsta && temp_arsta->arvif->ar == ar)
		ath12k_link_sta_rhash_delete(ar->ab, arsta);

	spin_unlock_bh(&ar->ab->base_lock);

	if (ahsta->links_map)
		ath12k_mac_free_unassign_link_sta(ahvif->ah,
						  arsta->ahsta, arsta->link_id);

	return ret;
}

static int ath12k_mac_station_add(struct ath12k *ar,
				  struct ath12k_link_vif *arvif,
				  struct ath12k_link_sta *arsta)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(arsta->ahsta);
	struct ath12k_wmi_peer_create_arg peer_param = {0};
	int ret;
	struct ath12k_link_sta *temp_arsta = NULL;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ret = ath12k_mac_inc_num_stations(arvif, arsta);
	if (ret) {
		ath12k_warn(ab, "refusing to associate station: too many connected already (%d)\n",
			    ar->max_num_stations);
		goto exit;
	}

	spin_lock_bh(&ab->base_lock);

	/* In case of Split PHY and roaming scenario, pdev idx
	 * might differ but both the pdev will share same rhash
	 * table. In that case update the rhash table if link_sta is
	 * already present
	 */
	temp_arsta = ath12k_link_sta_find_by_addr(ab, arsta->addr);
	if (temp_arsta && temp_arsta->arvif->ar != ar)
		ath12k_link_sta_rhash_delete(ab, temp_arsta);

	ret = ath12k_link_sta_rhash_add(ab, arsta);

	spin_unlock_bh(&ab->base_lock);
	if (ret) {
		ath12k_warn(ab, "Failed to add peer: %pM to hash table", arsta->addr);
		goto dec_num_station;
	}

	peer_param.vdev_id = arvif->vdev_id;
	peer_param.peer_addr = arsta->addr;
	if (arsta->is_bridge_peer) {
		peer_param.peer_type = WMI_PEER_TYPE_MLO_BRIDGE;

		/* For STA mode bridge peer, FW requirement is to set
		 * peer type as Default (0) during peer create.
		 */
		if (ahvif && ahvif->vdev_type == WMI_VDEV_TYPE_STA)
			peer_param.peer_type = WMI_PEER_TYPE_DEFAULT;
		peer_param.mlo_bridge_peer = true;
	} else {
		peer_param.peer_type = WMI_PEER_TYPE_DEFAULT;
		peer_param.mlo_bridge_peer = false;
	}
	peer_param.ml_enabled = sta->mlo;

	ret = ath12k_peer_create(ar, arvif, sta, &peer_param);
	if (ret) {
		ath12k_warn(ab, "Failed to add peer: %pM for VDEV: %d\n",
			    arsta->addr, arvif->vdev_id);
		goto rhash_delete;
	}

	ath12k_dbg(ab, ATH12K_DBG_PEER, "Added peer: %pM for VDEV: %d num_stations: %d\n",
		   arsta->addr, arvif->vdev_id, ar->num_stations);

	if (ieee80211_vif_is_mesh(vif)) {
		ret = ath12k_wmi_set_peer_param(ar, arsta->addr,
						arvif->vdev_id,
						WMI_PEER_USE_4ADDR, 1);
		if (ret) {
			ath12k_warn(ab, "failed to STA %pM 4addr capability: %d\n",
				    arsta->addr, ret);
			goto free_peer;
		}
	}

	if (ab->hw_params->vdev_start_delay &&
	    !arvif->is_started &&
	    arvif->ahvif->vdev_type != WMI_VDEV_TYPE_AP) {
		ret = ath12k_start_vdev_delay(ar, arvif);
		if (ret) {
			ath12k_warn(ab, "failed to delay vdev start: %d\n", ret);
			goto free_peer;
		}
	}

	return 0;

free_peer:
	ath12k_peer_delete(ar, arvif->vdev_id, arsta->addr, arsta->ahsta);
rhash_delete:
	spin_lock_bh(&ab->base_lock);
	ath12k_link_sta_rhash_delete(ab, arsta);
	spin_unlock_bh(&ab->base_lock);
dec_num_station:
	ath12k_mac_dec_num_stations(arvif, arsta);
exit:
	return ret;
}

static void ath12k_mac_map_link_sta(struct ath12k_sta *ahsta,
                                    const u8 link_id)
{
        ahsta->links_map |= BIT(link_id);
}

static int ath12k_mac_assign_link_sta(struct ath12k_hw *ah,
				      struct ath12k_sta *ahsta,
				      struct ath12k_link_sta *arsta,
				      struct ath12k_vif *ahvif,
				      u8 link_id)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	struct ieee80211_link_sta *link_sta;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;
	bool is_bridge_peer;
	int link_idx;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (!arsta || link_id >= ATH12K_NUM_MAX_LINKS)
		return -EINVAL;

	arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
	if (!arvif)
		return -EINVAL;

	memset(arsta, 0, sizeof(*arsta));

	/* For bridge peer, generate random mac_addr using kernel API
	 */
	is_bridge_peer = (ATH12K_BRIDGE_LINKS_MASK & BIT(link_id)) ? true :
								     false;
	if (is_bridge_peer) {
		eth_random_addr(arsta->addr);
		ahsta->primary_link_id = link_id;
	} else {
		link_sta = wiphy_dereference(ah->hw->wiphy, sta->link[link_id]);
		if (!link_sta)
			return -EINVAL;

		ether_addr_copy(arsta->addr, link_sta->addr);
	}

	if (!ahsta->free_logical_idx_map) {
		ath12k_warn(ab, "No free logical index available for link sta %pM\n",
			    arsta->addr);
		return -ENOSPC;
	}

	/* Allocate a logical link index by selecting the first available bit
	 * from the free logical index map
	 */
	link_idx = __ffs(ahsta->free_logical_idx_map);
	ahsta->free_logical_idx_map &= ~(BIT(link_idx));
	arsta->link_idx = link_idx;
	ahsta->num_peer++;

	arsta->link_id = link_id;
	ath12k_mac_map_link_sta(ahsta, link_id);
	arsta->arvif = arvif;
	ab = arsta->arvif->ar->ab;
	ahsta->device_bitmap |= BIT(ab->wsi_info.index);
	ahsta->mlo_hw_link_id_bitmap |= BIT(arvif->ar->pdev->hw_link_id);
	arsta->ahsta = ahsta;
	ahsta->ahvif = ahvif;
	arsta->is_bridge_peer = is_bridge_peer;

	wiphy_work_init(&arsta->update_wk, ath12k_sta_rc_update_wk);

	rcu_assign_pointer(ahsta->link[link_id], arsta);

	return 0;
}

static struct ath12k *ath12k_get_ar_by_device_idx(struct ath12k_hw_group *ag,
						  u8 device_idx)
{
	struct ath12k *ar = NULL;
	struct ath12k_hw *ah;
	u8 i, j;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		ar = ah->radio;
		for (j = 0; j < ah->num_radio; j++) {
			if (!ar)
				continue;

			if (ar->ab->wsi_info.index == device_idx)
				return ar;
			ar++;
		}
	}
	return NULL;
}

u8 ath12k_get_device_index(struct ath12k_mlo_wsi_load_info *wsi_load_info, u8 device_id)
{
	for (u8 i = 0; i < wsi_load_info->mlo_device_grp.num_devices; i++) {
		if (wsi_load_info->mlo_device_grp.wsi_order[i] == device_id)
			return i;
	}
	return WSI_INVALID_INDEX;
}

static u8 ath12k_get_wsi_next_device(struct ath12k_mlo_wsi_device_group *mlo_device_grp,
				     u8 prim_deviceid, u8 num_hop)
{
	u8 next_device_id = WSI_INVALID_ORDER;

	if (!num_hop)
		return next_device_id;

	next_device_id = (prim_deviceid + num_hop) % mlo_device_grp->num_devices;

	return next_device_id;
}

static int ath12k_send_wsi_load_info(struct ath12k_base *ab, u8 group_id)
{
	struct ath12k_wmi_wsi_stats_info_param param;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_wsi_load_info *wsi_load_info = ag->wsi_load_info;
	struct ath12k *ar = NULL;
	int ret = 0, i;

	for (i = 0; i < ATH12K_MAX_SOCS; i++) {
		if (wsi_load_info->load_stats[i].notify) {
			ar = ath12k_get_ar_by_device_idx(ag,
							 wsi_load_info->mlo_device_grp.wsi_order[i]);
			if (!ar) {
				ath12k_err(NULL, "ar is null");
				continue;
			}
			param.wsi_ingress_load_info =
				wsi_load_info->load_stats[i].ingress_cnt;
			param.wsi_egress_load_info =
				wsi_load_info->load_stats[i].egress_cnt;

			ret = ath12k_wmi_send_wsi_stats_info(ar, &param);
			if (ret)
				ath12k_warn(ar->ab,
					    "failed to initiate wmi pdev wsi stats info  %d",
					    ret);
			else
				wsi_load_info->load_stats[i].notify = false;
		}
	}
	return ret;
}

static int ath12k_wsi_load_info_stats_update(struct ath12k_vif *ahvif,
					     struct ath12k_sta *ahsta, bool append)
{
	struct ieee80211_sta *sta = container_of((void *)ahsta,
						 struct ieee80211_sta, drv_priv);
	struct ath12k_hw_group *ag;
	struct ath12k_mlo_wsi_device_group *mlo_device_grp;
	struct ath12k_link_vif *primary_arvif;
	struct ath12k_base *primary_ab;
	struct ath12k_link_vif *arvif;
	struct ath12k_mlo_wsi_load_info *wsi_load_info;
	unsigned long links;
	int ret = 0;
	u8 i, link_id;
	u8 prim_deviceid, hop_deviceid;
	u8 sec_deviceids[ATH12K_MAX_SOCS];
	u8 prim_deviceid_index, sec_deviceid_index, hop_deviceid_index;
	u8 hop_counted, num_dev_found = 0;
	u8 hops_from_primary;

	if (ahvif->vif->type != NL80211_IFTYPE_AP)
		return ret;

	if (!sta || !sta->mlo)
		return ret;

	/* Primary link device id identification */
	primary_arvif = ath12k_get_arvif_from_link_id(ahvif, ahsta->primary_link_id);
	if (!primary_arvif || !primary_arvif->ar || !primary_arvif->ar->ab)
		return ret;

	primary_ab = primary_arvif->ar->ab;
	ag = primary_ab->ag;

	if (!ag || !ag->wsi_load_info)
		return ret;

	if (ag->num_devices < ATH12K_MIN_NUM_DEVICES_NLINK)
		return ret;

	wsi_load_info = ag->wsi_load_info;

	prim_deviceid = primary_ab->wsi_info.index;

	if (!test_bit(WMI_TLV_SERVICE_PDEV_WSI_STATS_INFO_SUPPORT,
		      primary_ab->wmi_ab.svc_map))
		return 0;

	/* Secondary links device id identification */
	links = ahsta->links_map;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		if (ahsta->primary_link_id == link_id)
			continue;

		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);

		if (WARN_ON(!arvif))
			continue;

		if (!test_bit(WMI_TLV_SERVICE_PDEV_WSI_STATS_INFO_SUPPORT,
			      arvif->ar->ab->wmi_ab.svc_map))
			continue;

		sec_deviceids[num_dev_found++] = arvif->ar->ab->wsi_info.index;
	}

	/* Egress and ingress load count updation */
	prim_deviceid_index = ath12k_get_device_index(wsi_load_info, prim_deviceid);

	if (prim_deviceid_index == WSI_INVALID_INDEX) {
		ath12k_err(primary_ab, "primary device id not found in wsi_load_info\n");
		return -EOPNOTSUPP;
	}

	mlo_device_grp = &wsi_load_info->mlo_device_grp;

	if (num_dev_found) {
		wsi_load_info->load_stats[prim_deviceid_index].notify = true;
		wsi_load_info->load_stats[prim_deviceid_index].egress_cnt += append ? 1 : -1;
	} else {
		return ret;
	}

	hop_counted = 1;
	for (i = 0; i < num_dev_found; i++) {
		sec_deviceid_index = ath12k_get_device_index(wsi_load_info,
							     sec_deviceids[i]);

		if (sec_deviceid_index == WSI_INVALID_INDEX) {
			ath12k_err(NULL, "secondary device id not found in wsi_load_info\n");
			continue;
		}
		if (sec_deviceids[i] > prim_deviceid)
			hops_from_primary = sec_deviceids[i] - prim_deviceid;
		else
			hops_from_primary = mlo_device_grp->num_devices -
						(prim_deviceid - sec_deviceids[i]);

		while (hops_from_primary > hop_counted) {
			hop_deviceid = ath12k_get_wsi_next_device(mlo_device_grp,
								  prim_deviceid,
								  hop_counted);
			hop_counted++;
			if (hop_deviceid == WSI_INVALID_ORDER)
				continue;

			hop_deviceid_index = ath12k_get_device_index(wsi_load_info,
								     hop_deviceid);
			if (hop_deviceid_index == WSI_INVALID_INDEX) {
				ath12k_err(NULL,
					   "hop device id not found in wsi_load_info\n");
				continue;
			}
			wsi_load_info->load_stats[hop_deviceid_index].notify = true;
			wsi_load_info->load_stats[hop_deviceid_index].ingress_cnt +=
									append ? 1 : -1;
		}
	}

	ret = ath12k_send_wsi_load_info(primary_ab, ag->id);
	if (ret)
		ath12k_err(primary_ab, "failed to send wsi load info");

	return ret;
}

int ath12k_mac_create_bridge_peer(struct ath12k_hw *ah, struct ath12k_sta *ahsta,
				  struct ath12k_vif *ahvif, u8 link_id)
{
	int ret = -EINVAL;
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;

	if (ahsta->links_map & BIT(link_id)) {
		/* Some assumptions went wrong */
		ath12k_err(NULL, "Peer already exists on link: %d, unable to create Bridge peer\n",
			   link_id);
		return ret;
	}

	arvif = ahvif->link[link_id];
	if (!arvif) {
		ath12k_err(NULL, "Failed to get arvif to create bridge peer\n");
		return ret;
	}

	arsta = ath12k_mac_alloc_assign_link_sta(ah, ahsta, ahvif, link_id);

	if (!arsta) {
		ath12k_err(NULL, "Failed to alloc/assign link sta");
		return -ENOMEM;
	}

	ar = arvif->ar;
	if (!ar) {
		ath12k_err(NULL, "Failed to get ar to create bridge peer\n");
		ath12k_mac_free_unassign_link_sta(ah, ahsta, link_id);
		return ret;
	}

	ret = ath12k_mac_station_add(ar, arvif, arsta);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to add station: %pM for VDEV: %d\n",
			    arsta->addr, arvif->vdev_id);
		ath12k_mac_free_unassign_link_sta(ah, ahsta, link_id);
	}

	return ret;
}

int ath12k_mac_init_bridge_peer(struct ath12k_hw *ah, struct ieee80211_sta *sta,
				struct ath12k_vif *ahvif, u16 bridge_bitmap)
{
	struct ath12k_base *bridge_ab = NULL;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	u8 link_id = 0;
	int ret = -EINVAL;

	if (ath12k_mac_get_bridge_link_id_from_ahvif(ahvif, bridge_bitmap,
						     &link_id)) {
		ath12k_err(NULL, "Unable to find Bridge link vif for bitmap 0x%x\n",
			   bridge_bitmap);
		ret = -EINVAL;
		goto out_err;
	} else {
		bridge_ab = ahvif->link[link_id]->ar->ab;
		if (!test_bit(WMI_TLV_SERVICE_N_LINK_MLO_SUPPORT,
			      bridge_ab->wmi_ab.svc_map)) {
			ath12k_warn(bridge_ab,
				    "firmware doesn't support Bridge peer, so disconnect the sta %pM\n",
				    sta->addr);
			ret = -EINVAL;
			goto out_err;
		}
		ret = ath12k_mac_create_bridge_peer(ah, ahsta, ahvif, link_id);
		if (ret) {
			ath12k_err(bridge_ab, "Couldnt create Bridge peer for sta %pM\n",
				   sta->addr);
			goto out_err;
		}
		ath12k_info(bridge_ab, "Bridge peer created on link %d for sta %pM\n",
			    link_id, sta->addr);
	}
out_err:
	return ret;
}

static void ath12k_peer_mlo_link_sta_teardown(struct ath12k_hw *ah,
					      struct ath12k_vif *ahvif,
					      struct ath12k_sta *ahsta,
					      u8 link_id)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;

	arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (!arvif || !arsta)
		return;

	ar = arvif->ar;
	if (!ar)
		return;

	ath12k_mac_station_post_remove(ar, arvif, arsta);

	ath12k_cfr_decrement_peer_count(ar, arsta);

	spin_lock_bh(&ar->ab->base_lock);
	ath12k_link_sta_rhash_delete(ar->ab, arsta);
	spin_unlock_bh(&ar->ab->base_lock);

	ath12k_mac_free_unassign_link_sta(ah, ahsta, link_id);
}

static void ath12k_mac_ml_station_remove(struct ath12k_vif *ahvif,
					 struct ath12k_sta *ahsta)
{
	struct ath12k_hw *ah = ahvif->ah;
	unsigned long links;
	u8 link_id;

	lockdep_assert_wiphy(ah->hw->wiphy);

	ath12k_wsi_load_info_stats_update(ahvif, ahsta, false);

	ath12k_peer_mlo_link_peers_delete(ahvif, ahsta);

	/* validate link station removal and clear arsta links */
	links = ahsta->links_map;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		ath12k_peer_mlo_link_sta_teardown(ah, ahvif, ahsta, link_id);
	}

	ath12k_peer_ml_free(ah, ahsta);
}

static void ath12k_sta_migration_wk(struct work_struct *wk)
{
	struct ath12k_sta *ahsta = container_of(wk, struct ath12k_sta, migration_wk);
	struct ath12k_sta_migration_data *data = &ahsta->migration_data;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_base *pri_ab;
	unsigned long time_left;
	struct ath12k_dp *dp;
	bool ret = false;

	time_left = wait_for_completion_timeout(&ahsta->dp_migration_event, 2 * HZ);
	if (!time_left)
		goto send_dp_tx_event;

	pri_ab = data->ab;
	if (WARN_ON(!pri_ab))
		return;

	dp = ath12k_ab_to_dp(pri_ab);

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_id(dp, data->peer_id);
	if (WARN_ON(!peer)) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	/* if everything went good then this peer should be the primary peer now */
	if (!peer->primary_link)
		goto err_unlock;

	/* update the new primary link */
	ahsta->primary_link_id = peer->link_id;
	ret = false;

err_unlock:
	spin_unlock_bh(&dp->dp_lock);
send_dp_tx_event:
	ath12k_dp_tx_htt_pri_link_migr_msg(data->ab, data->vdev_id, data->peer_id,
					   data->ml_peer_id, data->pdev_id, data->chip_id,
					   data->ppe_vp_num, ret);

}

static int ath12k_mac_handle_link_sta_state(struct ieee80211_hw *hw,
					    struct ath12k_link_vif *arvif,
					    struct ath12k_link_sta *arsta,
					    enum ieee80211_sta_state old_state,
					    enum ieee80211_sta_state new_state)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ath12k *ar = arvif->ar;
	int ret = 0;

	lockdep_assert_wiphy(hw->wiphy);

	if (unlikely(test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)) &&
	    ar->ab->ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE2)
		return -ESHUTDOWN;

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER, "mac handle link %u sta %pM state %d -> %d\n",
		   arsta->link_id, arsta->addr, old_state, new_state);

	/* IEEE80211_STA_NONE -> IEEE80211_STA_NOTEXIST: Remove the station
	 * from driver
	 */
	if ((old_state == IEEE80211_STA_NONE &&
	     new_state == IEEE80211_STA_NOTEXIST)) {
		ret = ath12k_mac_station_remove(ar, arvif, arsta);
		if (ret) {
			ath12k_warn(ar->ab, "Failed to remove station: %pM for VDEV: %d\n",
				    arsta->addr, arvif->vdev_id);
			goto exit;
		}
	}

	/* IEEE80211_STA_NOTEXIST -> IEEE80211_STA_NONE: Add new station to driver */
	if (old_state == IEEE80211_STA_NOTEXIST &&
	    new_state == IEEE80211_STA_NONE) {
		ret = ath12k_mac_station_add(ar, arvif, arsta);
		if (ret)
			ath12k_warn(ar->ab, "Failed to add station: %pM for VDEV: %d\n",
				    arsta->addr, arvif->vdev_id);
		arsta->ahsta->low_ack_sent = false;
		arsta->ahsta->peer_delete_send_mlo_hw_bitmap = false;

	/* IEEE80211_STA_AUTH -> IEEE80211_STA_ASSOC: Send station assoc command for
	 * peer associated to AP/Mesh/ADHOC vif type.
	 */
	} else if (old_state == IEEE80211_STA_AUTH &&
		   new_state == IEEE80211_STA_ASSOC) {

		ret = ath12k_dp_peer_setup(ar, arvif, arsta->addr);
		if (ret) {
			ath12k_warn(ar->ab, "failed to setup dp for peer %pM on vdev %i (%d)\n",
					arsta->addr, arvif->vdev_id, ret);
			goto exit;
		}

		if (vif->type == NL80211_IFTYPE_AP ||
		    vif->type == NL80211_IFTYPE_MESH_POINT ||
		    vif->type == NL80211_IFTYPE_ADHOC) {
			ret = ath12k_mac_station_assoc(ar, arvif, arsta, false);
			if (ret)
				ath12k_warn(ar->ab, "Failed to associate station: %pM\n",
					    arsta->addr);
		}
	/* IEEE80211_STA_ASSOC -> IEEE80211_STA_AUTHORIZED: set peer status as
	 * authorized
	 */
	} else if (old_state == IEEE80211_STA_ASSOC &&
		   new_state == IEEE80211_STA_AUTHORIZED) {
		ret = ath12k_mac_station_authorize(ar, arvif, arsta);
		if (ret)
			ath12k_warn(ar->ab, "Failed to authorize station: %pM\n",
				    arsta->addr);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_dp_peer_ppeds_route_setup(ar, arvif, arsta);
#endif
	/* IEEE80211_STA_AUTHORIZED -> IEEE80211_STA_ASSOC: station may be in removal,
	 * deauthorize it.
	 */
	} else if (old_state == IEEE80211_STA_AUTHORIZED &&
		   new_state == IEEE80211_STA_ASSOC) {
		ath12k_mac_station_unauthorize(ar, arvif, arsta);

	/* IEEE80211_STA_ASSOC -> IEEE80211_STA_AUTH: disassoc peer connected to
	 * AP/mesh/ADHOC vif type.
	 */
	} else if (old_state == IEEE80211_STA_ASSOC &&
		   new_state == IEEE80211_STA_AUTH &&
		   (vif->type == NL80211_IFTYPE_AP ||
		    vif->type == NL80211_IFTYPE_MESH_POINT ||
		    vif->type == NL80211_IFTYPE_ADHOC)) {
		ret = ath12k_mac_station_disassoc(ar, arvif, arsta);
		if (ret)
			ath12k_warn(ar->ab, "Failed to disassociate station: %pM\n",
				    arsta->addr);
	}

exit:
	if (ret) {
		if (test_bit(ATH12K_FLAG_RECOVERY, &arvif->ar->ab->dev_flags)) {
			/* If FW recovery is ongoing, no need to move down sta states
			 * as FW will wake up with a clean slate. Hence we set the
			 * return value to 0, so that upper layers are not aware
			 * of the FW being in recovery state.
			 */
			if (old_state > new_state) {
				ath12k_warn(arvif->ar->ab, "Overwriting error with 0 during recovery after removal"
					    "of non-ml STA %pM for vdev %d with an error: %d.\n", arsta->addr,
					    arvif->vdev_id, ret);
				ret = 0;
			}
		}
	}

	return ret;
}

static int ath12k_mac_reconfig_ahsta_links_mode0(struct ath12k_hw *ah,
						 struct ath12k_sta *ahsta,
						 struct ath12k_vif *ahvif,
						 struct ieee80211_sta *sta)
{
	u32 link_to_assign, links_to_unmap;
	struct ath12k_link_sta *arsta;
	struct ieee80211_hw *hw = ah->hw;
	struct ath12k *ar;
	int ret;

	links_to_unmap = ahsta->links_map;
	/*
	 * Link only 1 link at a time as addtional links are mapped
	 * from drv_change_sta_links
	 */

	if (hweight16(ahvif->vif->active_links) > 1) {
		ath12k_err(NULL, "More than one link is not expected for STA reconfig\n");
		return -EINVAL;
	}

	link_to_assign = ffs(ahvif->vif->active_links) - 1;

	ahsta->links_map = 0;
	ahsta->mlo_hw_link_id_bitmap = 0;
	ahsta->device_bitmap = 0;
	ahsta->num_peer = 0;

	ath12k_dbg(NULL, ATH12K_DBG_MAC | ATH12K_DBG_BOOT,
		   "mac reconfig unmap links :0x%x sta link_map:0x%x vif link_map:0x%x sta valid links:%ld\n",
		   links_to_unmap, ahsta->links_map,
		   ahvif->links_map, sta->valid_links);

	arsta = wiphy_dereference(hw->wiphy, ahsta->link[link_to_assign]);
	ar = arsta->arvif->ar;

	if (WARN_ON(!ar))
		return -EINVAL;

	ret = ath12k_mac_assign_link_sta(ah, ahsta, arsta, ahvif, link_to_assign);
	if (ret) {
		ath12k_err(NULL, "failed to map link_id %d\n", link_to_assign);
		return ret;
	}

	ahsta->assoc_link_id = link_to_assign;
	ahsta->primary_link_id = link_to_assign;
	arsta->is_assoc_link = true;
	ath12k_dbg(NULL, ATH12K_DBG_MAC | ATH12K_DBG_BOOT,
		   "mac reconfig assign link sta: link_id:%d sta link_map:0x%x vif link_map:0x%x sta valid links:%ld\n",
		   link_to_assign, ahsta->links_map,
		   ahvif->links_map, sta->valid_links);

	return ret;
}

int ath12k_mac_op_sta_state(struct ieee80211_hw *hw,
			    struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta,
			    enum ieee80211_sta_state old_state,
			    enum ieee80211_sta_state new_state)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	struct wiphy *wiphy = hw->wiphy;
	struct wireless_dev *wdev;
	struct ath12k *ar = ah->radio;
	struct ath12k_hw_group *ag = ar->ab->ag;
	unsigned long links_map;
	bool is_recovery = false;
	u8 link_id = 0, active_num_devices;
	u8 t_link_id = 0;
	u16 bridge_bitmap = 0;
	int ret = -EINVAL;
	struct ath12k_dp_peer_create_params dp_params = {0};

	lockdep_assert_wiphy(wiphy);

	if ((old_state == IEEE80211_STA_NOTEXIST &&
	     new_state == IEEE80211_STA_NONE) && ag->wsi_remap_in_progress) {
		ath12k_err(NULL, "cannot allow new station association, WSI bypass is in progress\n");
		ret = -EINVAL;
		goto exit;
	}

	active_num_devices = ag->num_devices - ag->num_bypassed;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (!ahsta->ppe_vp_num)
		ahsta->ppe_vp_num = ahvif->dp_vif.ppe_vp_num;

	if (vif->type == NL80211_IFTYPE_AP_VLAN) {
		wdev = ieee80211_vif_to_wdev(vif);
		/* Update parent vif for further use */
		vif = wdev_to_ieee80211_vif_vlan(wdev, false);
		if (!vif) {
			ret = -EINVAL;
			goto exit;
		}

		ahsta->ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
		if (ahvif->vlan_iface && !ahvif->vlan_iface->attach_link_done)
			ath12k_ppe_ds_attach_vlan_vif_link(ahvif->vlan_iface,
							   ahvif->dp_vif.ppe_vp_num);
		/* Update ahvif with parent vif */
		ahvif = ath12k_vif_to_ahvif(vif);
	}
#endif

	if (ieee80211_vif_is_mld(vif) && sta->valid_links) {
		WARN_ON(!sta->mlo && hweight16(sta->valid_links) != 1);
		link_id = ffs(sta->valid_links) - 1;
	}

	/* IEEE80211_STA_NOTEXIST -> IEEE80211_STA_NONE:
	 * New station add received. If this is a ML station then
	 * ahsta->links_map will be zero and sta->valid_links will be 1.
	 * Assign default link to the first link sta.
	 */
	if (old_state == IEEE80211_STA_NOTEXIST &&
	    new_state == IEEE80211_STA_NONE) {

		if (!ahsta->links_map) {
			memset(ahsta, 0, sizeof(*ahsta));
			wiphy_work_init(&ahsta->set_4addr_wk, ath12k_sta_set_4addr_wk);
			arsta = &ahsta->deflink;
		}

		ahsta->free_logical_idx_map = U16_MAX;
		/* ML sta */
		links_map = ahsta->links_map;
		if (sta->mlo && ((!ahsta->links_map &&
		    (hweight16(sta->valid_links) == 1)) ||
		     test_bit(link_id, &links_map))) {

			links_map = ahvif->links_map;
		    	/*Add case to prevent MLO assoc from happening when UMAC recovery happens */
			for_each_set_bit(t_link_id, &links_map, IEEE80211_MLD_MAX_NUM_LINKS){
				arvif = wiphy_dereference(wiphy, ahvif->link[t_link_id]);
				if (!arvif->ar ||
				    (test_bit(ATH12K_FLAG_UMAC_RECOVERY_START,
					      &arvif->ar->ab->dev_flags))){
					ret = -EINVAL;
					goto exit;
				}
			}
			ahsta->ml_peer_id = ath12k_peer_ml_alloc(ah);
			if (ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID) {
				ath12k_hw_warn(ah, "unable to allocate ML peer id for sta %pM",
					       sta->addr);
				goto exit;
			}

			dp_params.is_mlo = true;
			dp_params.peer_id = ahsta->ml_peer_id | ATH12K_PEER_ML_ID_VALID;
			ah->num_ml_peers++;
		}

		dp_params.sta = sta;
		ret = ath12k_dp_peer_create(&ah->dp_hw, sta->addr, &dp_params, vif);
		if (ret) {
			ath12k_hw_warn(ah, "unable to create ath12k_dp_peer for sta %pM",
				       sta->addr);

			goto ml_peer_id_free;
		}
		links_map = ahsta->links_map;
		if (!test_bit(link_id, &links_map)) {
			ret = ath12k_mac_assign_link_sta(ah, ahsta, arsta, ahvif,
							 link_id);
			if (ret) {
				ath12k_hw_warn(ah, "unable assign link %d for sta %pM",
					       link_id, sta->addr);
				goto peer_delete;
			}

			/* above arsta will get memset, hence do this after assign
			 * link sta
			 */
			if (sta->mlo) {
				arsta->is_assoc_link = true;
				ahsta->assoc_link_id = link_id;
				ahsta->primary_link_id = link_id;

				init_completion(&ahsta->dp_migration_event);
				INIT_WORK(&ahsta->migration_wk, ath12k_sta_migration_wk);

				ath12k_dbg(NULL, ATH12K_DBG_MAC,
					   "mac ML STA %pM primary link (reconfig) set to %u\n",
					   sta->addr, ahsta->primary_link_id);
			}
		}
	}

	/* In the ML station scenario, activate all partner links once the
	 * client is transitioning to the associated state.
	 *
	 * FIXME: Ideally, this activation should occur when the client
	 * transitions to the authorized state. However, there are some
	 * issues with handling this in the firmware. Until the firmware
	 * can manage it properly, activate the links when the client is
	 * about to move to the associated state.
	 */
	if (ieee80211_vif_is_mld(vif) && vif->type == NL80211_IFTYPE_STATION &&
	    old_state == IEEE80211_STA_AUTH && new_state == IEEE80211_STA_ASSOC)
		ieee80211_set_active_links(vif, ieee80211_vif_usable_links(vif));

	if ((ahvif->vdev_type == WMI_VDEV_TYPE_AP || ahvif->vdev_type == WMI_VDEV_TYPE_STA) &&
	    (old_state == IEEE80211_STA_AUTH && new_state == IEEE80211_STA_ASSOC) &&
	    ath12k_mac_is_bridge_required(ahsta->device_bitmap, active_num_devices,
					  &bridge_bitmap)) {
		ret = ath12k_mac_init_bridge_peer(ah, sta, ahvif, bridge_bitmap);
		if (ret)
			goto exit;
	}
	/* Reconfig links of arsta during recovery */

	/* Mode-0 mapping of ahsta links is done below for first
	 * deflink and for additional link, it will be done in
	 * drv_change_sta_links.
	 */
	if (ahsta->state != IEEE80211_STA_NOTEXIST &&
	    old_state == IEEE80211_STA_NOTEXIST &&
	    new_state == IEEE80211_STA_NONE) {
		if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
			if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0) {
				ret = ath12k_mac_reconfig_ahsta_links_mode0(ah, ahsta,
									    ahvif, sta);

				if (ret) {
					ath12k_err(NULL,
						   "Failure in Mode-0 reconfig: %d\n",
						   ret);
					return ret;
				}

				ath12k_dbg(NULL, ATH12K_DBG_MAC,
					   "mac ML STA %pM primary link (reconfig) set to %u\n",
					   sta->addr, ahsta->primary_link_id);
			}
		} else {
			if (ahsta->use_4addr_set)
				wiphy_work_queue(wiphy, &ahsta->set_4addr_wk);
		}
	}

	/* Handle all the other state transitions in generic way */
	links_map = ahsta->links_map;
	for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);
		arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);
		/* some assumptions went wrong! */
		if (WARN_ON(!arvif || !arsta))
			continue;

		/* vdev might be in deleted */
		if (WARN_ON(!arvif->ar))
			continue;

		ret = ath12k_mac_handle_link_sta_state(hw, arvif, arsta,
						       old_state, new_state);
		if (ret) {
			if (ret != -ESHUTDOWN)
				ath12k_hw_warn(ah, "unable to move link sta %d of sta %pM from state %d to %d",
					       link_id, arsta->addr, old_state, new_state);

			/* If FW recovery is ongoing, no need to move down sta states
			 * as FW will wake up with a clean slate. Hence we set the
			 * return value to 0, so that upper layers are not aware
			 * of the FW being in recovery state.
			 */
			if (old_state > new_state) {
				if (!arvif->ar)
					continue;
				if (test_bit(ATH12K_FLAG_RECOVERY,
					     &arvif->ar->ab->dev_flags) ||
				    test_bit(ATH12K_FLAG_CRASH_FLUSH,
					     &arvif->ar->ab->dev_flags))
					is_recovery = true;
			}

			if (old_state == IEEE80211_STA_NOTEXIST &&
			    new_state == IEEE80211_STA_NONE)
				goto peer_delete;

			/* If FW recovery is going on and link sta handling for
			 * IEEE80211_STA_NONE -> IEEE80211_STA_NOT_EXIST
			 * got failed for that link. Proceed with ml_station_remove
			 * as anyway we must report success to upper layers
			 * during recovery so that it can clean up its memory.
			 */

			else if (is_recovery &&
				 old_state == IEEE80211_STA_NONE &&
				 new_state == IEEE80211_STA_NOTEXIST)
				goto ml_station_remove;
			else
				goto exit;
		}
	}

	if (old_state == IEEE80211_STA_AUTH &&  new_state == IEEE80211_STA_ASSOC)
		ath12k_wsi_load_info_stats_update(ahvif, ahsta, true);

ml_station_remove:
	/* IEEE80211_STA_NONE -> IEEE80211_STA_NOTEXIST:
	 * Remove the station from driver (handle ML sta here since that
	 * needs special handling. Normal sta will be handled in generic
	 * handler below
	 */
	if (old_state == IEEE80211_STA_NONE &&
	    new_state == IEEE80211_STA_NOTEXIST) {
		if (sta->mlo) {
			ath12k_mac_ml_station_remove(ahvif, ahsta);
			cancel_work_sync(&ahsta->migration_wk);
		} else if (is_recovery) {
			link_id = ffs(ahsta->links_map) - 1;

			arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);
			arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);

			if (!WARN_ON(!arvif || !arsta))
				ath12k_mac_station_remove(arvif->ar, arvif, arsta);
		}
		ath12k_dp_peer_delete(&ah->dp_hw, sta->addr, sta, ar->hw_link_id);
		wiphy_work_cancel(wiphy, &ahsta->set_4addr_wk);
	}

	if (ag->wsi_remap_in_progress && !ah->num_ml_peers) {
		ath12k_dbg(NULL, ATH12K_DBG_WSI_BYPASS,
			   "Bypass: Completing peer cleanup timer\n");
		complete(&ag->peer_cleanup_complete);
	}
	ret = 0;

peer_delete:
	if (ret)
		ath12k_dp_peer_delete(&ah->dp_hw, sta->addr, sta, ar->hw_link_id);
ml_peer_id_free:
	if (ret)
		ath12k_peer_ml_free(ah, ahsta);
exit:

	if (ret && is_recovery)
		ret = 0;

	/* update the state if everything went well */
	if (!ret)
		ahsta->state = new_state;

	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_sta_state);

int ath12k_mac_op_sta_set_txpwr(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k *ar;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	u8 link_id;
	int ret;
	s16 txpwr;

	lockdep_assert_wiphy(hw->wiphy);

	/* TODO: use link id from mac80211 once that's implemented */
	link_id = 0;

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
	arsta = wiphy_dereference(hw->wiphy, ahsta->link[link_id]);

	if (sta->deflink.txpwr.type == NL80211_TX_POWER_AUTOMATIC) {
		txpwr = 0;
	} else {
		txpwr = sta->deflink.txpwr.power;
		if (!txpwr) {
			ret = -EINVAL;
			goto out;
		}
	}

	if (txpwr > ATH12K_TX_POWER_MAX_VAL || txpwr < ATH12K_TX_POWER_MIN_VAL) {
		ret = -EINVAL;
		goto out;
	}

	ar = arvif->ar;

	ret = ath12k_wmi_set_peer_param(ar, arsta->addr, arvif->vdev_id,
					WMI_PEER_USE_FIXED_PWR, txpwr);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set tx power for station ret: %d\n",
			    ret);
		goto out;
	}

out:
	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_sta_set_txpwr);

void ath12k_mac_op_link_sta_rc_update(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      struct ieee80211_link_sta *link_sta,
				      u32 changed)
{
	struct ieee80211_sta *sta = link_sta->sta;
	struct ath12k *ar;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_link_peer *peer;
	u32 bw, smps;

	rcu_read_lock();
	arvif = rcu_dereference(ahvif->link[link_sta->link_id]);
	if (!arvif) {
		ath12k_hw_warn(ah, "mac sta rc update failed to fetch link vif on link id %u for peer %pM\n",
			       link_sta->link_id, sta->addr);
		rcu_read_unlock();
		return;
	}

	ar = arvif->ar;

	arsta = rcu_dereference(ahsta->link[link_sta->link_id]);
	if (!arsta) {
		rcu_read_unlock();
		ath12k_warn(ar->ab, "mac sta rc update failed to fetch link sta on link id %u for peer %pM\n",
			    link_sta->link_id, sta->addr);
		return;
	}
	spin_lock_bh(&ar->ab->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ar->ab->dp, arvif->vdev_id,
							    arsta->addr);
	if (!peer) {
		spin_unlock_bh(&ar->ab->dp->dp_lock);
		rcu_read_unlock();
		ath12k_warn(ar->ab, "mac sta rc update failed to find peer %pM on vdev %i\n",
			    arsta->addr, arvif->vdev_id);
		return;
	}

	spin_unlock_bh(&ar->ab->dp->dp_lock);

	if (arsta->link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		rcu_read_unlock();
		return;
	}

	link_sta = rcu_dereference(sta->link[arsta->link_id]);
	if (!link_sta) {
		rcu_read_unlock();
		ath12k_warn(ar->ab, "unable to access link sta in rc update for sta %pM link %u\n",
			    sta->addr, arsta->link_id);
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac sta rc update for %pM changed %08x bw %d nss %d smps %d\n",
		   arsta->addr, changed, link_sta->bandwidth, link_sta->rx_nss,
		   link_sta->smps_mode);

	spin_lock_bh(&ar->data_lock);

	if (changed & IEEE80211_RC_BW_CHANGED) {
		bw = ath12k_mac_ieee80211_sta_bw_to_wmi(ar, link_sta);
		arsta->bw = bw;
	}

	if (changed & IEEE80211_RC_NSS_CHANGED)
		arsta->nss = link_sta->rx_nss;

	if (changed & IEEE80211_RC_SMPS_CHANGED) {
		smps = WMI_PEER_SMPS_PS_NONE;

		switch (link_sta->smps_mode) {
		case IEEE80211_SMPS_AUTOMATIC:
		case IEEE80211_SMPS_OFF:
			smps = WMI_PEER_SMPS_PS_NONE;
			break;
		case IEEE80211_SMPS_STATIC:
			smps = WMI_PEER_SMPS_STATIC;
			break;
		case IEEE80211_SMPS_DYNAMIC:
			smps = WMI_PEER_SMPS_DYNAMIC;
			break;
		default:
			ath12k_warn(ar->ab, "Invalid smps %d in sta rc update for %pM link %u\n",
				    link_sta->smps_mode, arsta->addr, link_sta->link_id);
			smps = WMI_PEER_SMPS_PS_NONE;
			break;
		}

		arsta->smps = smps;
	}

	arsta->changed |= changed;

	spin_unlock_bh(&ar->data_lock);

	wiphy_work_queue(hw->wiphy, &arsta->update_wk);

	rcu_read_unlock();
}
EXPORT_SYMBOL(ath12k_mac_op_link_sta_rc_update);

static struct ath12k_link_sta *ath12k_mac_alloc_assign_link_sta(struct ath12k_hw *ah,
								struct ath12k_sta *ahsta,
								struct ath12k_vif *ahvif,
								u8 link_id)
{
	struct ath12k_link_sta *arsta;
	int ret;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (link_id >= ATH12K_NUM_MAX_LINKS)
		return NULL;

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (arsta) {
		arsta = ahsta->link[link_id];
		ath12k_mac_assign_link_sta(ah, ahsta, arsta, ahvif, link_id);
		ath12k_dbg(NULL, ATH12K_DBG_MAC | ATH12K_DBG_BOOT,
			   "mac alloc assign link sta: arsta found, link_id:%d\n",
			   link_id);
		return ahsta->link[link_id];
	}

	arsta = kmalloc(sizeof(*arsta), GFP_KERNEL);
	if (!arsta)
		return NULL;

	ret = ath12k_mac_assign_link_sta(ah, ahsta, arsta, ahvif, link_id);
	if (ret) {
		kfree(arsta);
		return NULL;
	}

	return arsta;
}

void ath12k_mac_assign_middle_link_id(struct ieee80211_sta *sta,
				      struct ath12k_sta *ahsta,
				      u8 *pri_link_id,
				      u8 num_devices)
{
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *ab;
	u8 link_id;
	u8 device_bitmap = ahsta->device_bitmap;
	u8 i, next, prev;
	bool adjacent_found = false;
	unsigned long links;

	lockdep_assert_wiphy(ah->hw->wiphy);

	/* 4 device: In case of 3 link STA association, Make sure to select
	 * the middle device link as primary_link_id of sta which is adjacent
	 * to other two devices.
	 */
	if (!(num_devices == ATH12K_MIN_NUM_DEVICES_NLINK &&
	      hweight16(sta->valid_links) == ATH12K_MAX_STA_LINKS)) {
		/* To-Do: Requirement to set primary link id for no.of devices
		 * greater than 4 has not yet confirmed. Also, Need to revisit
		 * here when STA association support extends more than 3.
		 */
		if (num_devices > ATH12K_MIN_NUM_DEVICES_NLINK)
			ath12k_err(NULL,
				   "num devices %d Combination not supported yet\n",
				   num_devices);
		return;
	}

	for (i = 0; i < num_devices; i++) {
		if (!(device_bitmap & BIT(i)))
			continue;

		next = (i + 1) % num_devices;
		prev = ((i - 1) + num_devices) % num_devices;
		if ((device_bitmap & BIT(next)) && (device_bitmap & BIT(prev))) {
			adjacent_found = true;
			break;
		}
	}

	if (!adjacent_found) {
		ath12k_err(NULL,
			   "No common adjacent devices found for sta %pM with device bitmap 0x%x\n",
			   sta->addr, device_bitmap);
		return;
	}

	links = ahsta->links_map;
	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arsta = ahsta->link[link_id];

		if (!(arsta && arsta->arvif))
			continue;

		ab = arsta->arvif->ar->ab;
		if (!ab)
			continue;

		if (ab->wsi_info.index == i) {
			ath12k_info(ab, "Overwriting primary link_id as %d for sta %pM",
				    link_id, sta->addr);
			*pri_link_id = link_id;
			return;
		}
	}
}

static bool ath12k_is_primary_link_migrate(struct ieee80211_sta *sta,
					   u16 removed_links)
{
	struct ath12k_mac_link_migrate_usr_params migrate_params = {0};
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	unsigned long removed_link = removed_links;
	struct ath12k_vif *ahvif = ahsta->ahvif;
	struct ath12k_link_vif *arvif;
	bool is_primary = false;
	struct ath12k *ar;
	u16 link_id;
	int ret;


	for_each_set_bit(link_id, &removed_link, ATH12K_NUM_MAX_LINKS) {
		if (ahsta->primary_link_id == link_id) {
			is_primary = true;
			break;
		}
	}

	/* if not primary, just return now */
	if (!is_primary)
		return false;

	/* if it happens to be primary link, trigger UMAC Migration. */
	arvif = ath12k_get_arvif_from_link_id(ahvif, ahsta->primary_link_id);
	if (WARN_ON(!arvif || !arvif->is_up || !arvif->ar))
		return true;

	ar = arvif->ar;

	migrate_params.link_id = 0xFF;
	memcpy(migrate_params.addr, sta->addr, ETH_ALEN);

	arvif->is_link_removal_in_progress = true;

	ret = ath12k_mac_process_link_migrate_req(ahvif, &migrate_params);
	if (ret) {
		arvif->is_link_removal_in_progress = false;
		return true;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac waiting for UMAC migration to finish\n");

	/* Wait for migration to finish. If it timed out, just WARN_ON()
	 * and continue since in this case, primary link will still match
	 * and whole MLD will disconnect anyway
	 */
	if (!wait_for_completion_timeout(&arvif->wmi_migration_event_resp,
					 ATH12K_MIGRATION_TIMEOUT_HZ))
		WARN_ON(1);

	arvif->is_link_removal_in_progress = false;

	/* now check again and accordingly return */
	is_primary = false;
	for_each_set_bit(link_id, &removed_link, ATH12K_NUM_MAX_LINKS) {
		if (ahsta->primary_link_id == link_id) {
			is_primary = true;
			break;
		}
	}

	return is_primary;
}

static int ath12k_sta_ml_reconfig_handler(struct ieee80211_hw *hw,
					  struct ieee80211_vif *vif,
					  struct ieee80211_sta *sta,
					  u16 old_links, u16 new_links)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_wmi_peer_assoc_arg peer_arg;
	struct ath12k_link_sta *arsta, *arsta_p;
	struct ath12k_link_vif *arvif, *arvif_p;
	struct ieee80211_link_sta *link_sta;
	struct ath12k_dp_link_peer *peer;
	unsigned long valid_links;
	struct ath12k *ar, *ar_p;
	struct ath12k_dp *dp_p;
	int i, ret = 0, len = 0;
	u32 flags = 0;
	u8 link_id;
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1] = {0};

	valid_links = sta->valid_links;

	arsta_p = ahsta->link[ahsta->primary_link_id];
	arvif_p = wiphy_dereference(hw->wiphy,
				    ahvif->link[ahsta->primary_link_id]);

	if (!arvif_p || !arsta_p) {
		ath12k_err(NULL, "unable to determine arsta\n");
		return -ENOLINK;
	}

	ar_p = arvif_p->ar;

	dp_p = ath12k_ab_to_dp(ar_p->ab);
	spin_lock_bh(&dp_p->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ar_p->ab->dp,
							    arvif_p->vdev_id,
							    arsta_p->addr);

	if (!peer || !peer->dp_peer) {
		ath12k_err(ar_p->ab, "ML reconfig: peer not found");
		spin_unlock_bh(&dp_p->dp_lock);
		return -ENOENT;
	}

	len = ARRAY_SIZE(peer->dp_peer->keys);
	for (i = 0; i < len; i++) {
		if (!peer->dp_peer->keys[i])
			continue;

		keys[i] = peer->dp_peer->keys[i];
	}
	spin_unlock_bh(&dp_p->dp_lock);

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "ML reconfig: old_links=0x%x new_links=0x%x valid_links=0x%lx\n",
		   old_links, new_links, valid_links);

	mutex_lock(&ah->hw_mutex);

	/* For each valid link:
	 * - Sends a peer association command to firmware.
	 * - Installs pairwise keys and authorizes the
	 *   station on newly added links
	 */
	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		if (!(ahvif->links_map & BIT(link_id)))
			continue;

		arsta = ahsta->link[link_id];
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);

		if (!arvif || !arsta) {
			ath12k_err(NULL, "Failed to alloc/assign link sta");
			continue;
		}

		ar = arvif->ar;
		if (!ar) {
			ath12k_err(NULL,
				   "Failed to get ar to change sta links\n");
			continue;
		}

		if (arsta->is_bridge_peer) {
			ath12k_warn(ar->ab, "Bridge Peer Not supported\n");
			continue;
		}

		rcu_read_lock();
		link_sta = ath12k_mac_get_link_sta(arsta);
		if (!link_sta) {
			rcu_read_unlock();
			ath12k_warn(ar->ab, "Link Sta not found\n");
			goto out;
		}
		ath12k_peer_assoc_prepare(ar, arvif, arsta, &peer_arg,
					  false, link_sta);

		rcu_read_unlock();

		ret = ath12k_wmi_send_peer_assoc_cmd(ar, &peer_arg);
		if (ret) {
			ath12k_warn(ar->ab, "failed to run peer assoc for vdev %i: %d\n",
				    arvif->vdev_id, ret);
			goto out;
		}

		if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
			ath12k_warn(ar->ab, "failed to get peer assoc conf event for vdev %i\n",
				    arvif->vdev_id);
			goto out;
		}

		if (sta->reconf.added_links & BIT(link_id)) {
			for (i = 0; i < len; i++) {
				if (!keys[i])
					continue;

				if (!(keys[i]->flags & IEEE80211_KEY_FLAG_PAIRWISE))
					continue;

				flags |= WMI_KEY_PAIRWISE;
				ret = ath12k_install_key(arvif,
							 keys[i],
							 SET_KEY, arsta->addr,
							 flags);
				if (ret) {
					ath12k_warn(ar->ab, "failed to add peer key %d: %d\n",
						    i, ret);
					goto out;
				}
				break;
			}

			ret = ath12k_mac_station_authorize(ar, arvif, arsta);
			if (ret) {
				ath12k_warn(ar->ab, "Unable to authorize peer %pM vdev %d: %d\n",
					    sta->addr, arvif->vdev_id, ret);
				goto out;
			}
		}
	}
	ret = 0;

out:
	mutex_unlock(&ah->hw_mutex);

	if (sta->reconf.removed_links & BIT(ahsta->primary_link_id))
		ath12k_is_primary_link_migrate(sta,
					       sta->reconf.removed_links);
	return ret;
}

int ath12k_mac_op_change_sta_links(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta,
				   u16 old_links, u16 new_links)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_link_vif *arvif, *tmp_arvif;
	struct ath12k_link_sta *arsta, *tmp_arsta, *def_arsta;
	unsigned long valid_links;
	struct ath12k *ar, *tmp_ar;
	u16 removed_link_map;
	u8 link_id, tmp_link_id, pri_link_id;
	int ret, assoc_status, result;

	lockdep_assert_wiphy(hw->wiphy);

	if (!sta->valid_links)
		return -EINVAL;

	if (sta->reconf.added_links ||
	    (sta->valid_links & sta->reconf.removed_links)) {
		return ath12k_sta_ml_reconfig_handler(hw, vif, sta,
					       old_links, new_links);
	}

	if (new_links > old_links) {
		if (ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID) {
			ath12k_hw_warn(ah, "unable to add link for ml sta %pM", sta->addr);
			return -EINVAL;
		}

		/* this op is expected only after initial sta insertion with default link */
		if (WARN_ON(ahsta->links_map == 0))
			return -EINVAL;

		if (hweight16(ahsta->links_map) >= ATH12K_WMI_MLO_PEER_MAX_LINKS) {
			ath12k_err(NULL, "More than 3 links are not supported for ML STA %pM\n",
				   sta->addr);
			return -EINVAL;
		}

		valid_links = new_links;
		for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
			if (ahsta->links_map & BIT(link_id))
				continue;

			arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
			arsta = ath12k_mac_alloc_assign_link_sta(ah, ahsta, ahvif, link_id);

			if (!arvif || !arsta) {
				ath12k_hw_warn(ah, "Failed to alloc/assign link sta");
				continue;
			}

			ar = arvif->ar;
			if (!ar)
				continue;

			ret = ath12k_mac_station_add(ar, arvif, arsta);
			if (ret) {
				ath12k_warn(ar->ab, "Failed to add station: %pM for VDEV: %d\n",
					    arsta->addr, arvif->vdev_id);
				ath12k_mac_free_unassign_link_sta(ah, ahsta, link_id);
				return ret;
			}

			ret = ath12k_dp_peer_setup(ar, arvif, arsta->addr);
			if (ret) {
				ath12k_warn(ar->ab, "peer %pM setup failed ret: %d\n",
					    arsta->addr, ret);

				result = ath12k_mac_station_remove(ar, arvif, arsta);
				if (result)
					ath12k_warn(ar->ab, "arsta %pM remove failed\n",
						    arsta->addr);

				if (sta->mlo) {
					result = ath12k_peer_mlo_link_peer_delete(arvif,
										  arsta);
					if (result)
						ath12k_warn(ar->ab, "ml arsta %pM remove failed\n",
							    arsta->addr);

					result =
					ath12k_wait_for_peer_delete_done(ar,
									 arvif->vdev_id,
									 arsta->addr);
					if (result)
						ath12k_warn(ar->ab, "peer delete timeout for arsta %pM\n",
							    arsta->addr);

					ar->num_peers--;
					ath12k_peer_mlo_link_sta_teardown(ah,
									  ahvif,
									  ahsta,
									  link_id);
				}

				return ret;
			}
		}

		if (!ret && ahsta->state < IEEE80211_STA_AUTHORIZED) {
			pri_link_id =
				ath12k_mac_ahsta_get_pri_link_id(ahvif, ahsta,
								 ahsta->links_map);
			if (pri_link_id == IEEE80211_MLD_MAX_NUM_LINKS) {
				pri_link_id = ahsta->assoc_link_id;
				ahsta->primary_link_id = pri_link_id;
			} else {
				arvif =
				wiphy_dereference(hw->wiphy, ahvif->link[pri_link_id]);

				if (vif->type == NL80211_IFTYPE_STATION && arvif &&
				    !ath12k_mac_is_bridge_vdev(arvif)) {
					assoc_status =
					ieee80211_get_link_assoc_status(vif, pri_link_id);
					/* When the assoc is failed for the selected
					 * link, then avoid updating that link as primary
					 */
					if (assoc_status != 0) {
						ath12k_err(NULL,
							   "Selected pri_link_id:%u, retain pri_link_id:%u\n",
							   pri_link_id,
							   ahsta->primary_link_id);
						goto skip_pri_link_selection;
					}
				}
				ahsta->primary_link_id = pri_link_id;
			}
skip_pri_link_selection:
			ath12k_dbg(NULL, ATH12K_DBG_MAC,
				   "mac ML STA %pM primary link set to %u\n",
				   sta->addr, ahsta->primary_link_id);
		}
	} else {
		removed_link_map = old_links ^ new_links;

		if (hweight16(removed_link_map) > 1)
			return -EINVAL;

		link_id = ffs(removed_link_map) - 1;

		arvif = ahvif->link[link_id];
		arsta = ahsta->link[link_id];
		ar = arvif->ar;

		if (!arsta || !arvif)
			return -EINVAL;

		if (vif->type == NL80211_IFTYPE_AP) {
			if (ahsta->primary_link_id == link_id) {
				/* if the peer is not undergoing migration and still the
				 * primary link is on this removal link, disconnect whole
				 * MLD sta
				 */
				if (!ahsta->is_migration_in_progress) {
					ieee80211_report_low_ack(sta, ATH12K_REPORT_LOW_ACK_NUM_PKT);
					return -EINVAL;
				} else {
					/* Migration is in progress. The event completion
					 * will handle this peer. Ignore this.
					 * But when this path is hit during ML Link Removal
					 * procedure, this should not really happen
					 */
					WARN_ON(arvif->is_link_removal_in_progress);
					return 0;
				}
			}
			ath12k_wsi_load_info_stats_update(ahvif, ahsta, false);

			ret = ath12k_mac_station_disassoc(ar, arvif, arsta);
			if (ret)
				ath12k_warn(ar->ab, "Failed to disassoc station: %pM for VDEV: %d\n",
					    arsta->addr, arvif->vdev_id);
		}

		if (vif->type == NL80211_IFTYPE_AP || vif->type == NL80211_IFTYPE_STATION) {
			ret = ath12k_mac_station_remove(ar, arvif, arsta);
			if (ret)
				ath12k_warn(ar->ab, "Failed to remove station: %pM for VDEV: %d\n",
					    sta->addr, arvif->vdev_id);

			if (ret) {
				if (test_bit(ATH12K_FLAG_RECOVERY, &arvif->ar->ab->dev_flags)) {
					ath12k_info(ar->ab, " overwriting ret %d with 0 for %pM",
						    ret, arsta->addr);
					ret = 0;
				}
			}

			if (sta->mlo) {
				ret = ath12k_peer_mlo_link_peer_delete(arvif,
								       arsta);
				if (ret)
					ath12k_warn(ar->ab, "Failed to remove ml station: %pM for VDEV: %d\n",
						    arsta->addr, arvif->vdev_id);

				ret = ath12k_wait_for_peer_delete_done(ar,
								       arvif->vdev_id,
								       arsta->addr);
				if (ret) {
					if (test_bit(ATH12K_FLAG_CRASH_FLUSH,
						     &ar->ab->dev_flags) ||
					    test_bit(ATH12K_FLAG_RECOVERY,
						     &ar->ab->dev_flags) ||
					    test_bit(ATH12K_FLAG_UMAC_RECOVERY_START,
						     &ar->ab->dev_flags)) {
						ath12k_info(ar->ab, " overwriting ret %d with 0 for %pM",
							    ret, arsta->addr);
						ret = 0;
					}
					ath12k_warn(ar->ab, "peer delete timeout for station %pM for VDEV: %d\n",
						    arsta->addr, arvif->vdev_id);
				}

				ar->num_peers--;
				ath12k_peer_mlo_link_sta_teardown(ah,
								  ahvif,
								  ahsta,
								  link_id);
			}

			if (vif->type == NL80211_IFTYPE_AP)
				ath12k_wsi_load_info_stats_update(ahvif, ahsta, true);
		}

		/* If the link that is getting removed is the assoc link id of
		 * the station, then move the contents of the next link to
		 * deflink and free the moved link memory
		 */
		if (ahsta->assoc_link_id != ahsta->primary_link_id &&
		    ahsta->assoc_link_id == link_id &&
		    hweight32(ahsta->links_map) >= 1) {
			tmp_link_id = ffs(ahsta->links_map) - 1;

			tmp_arsta = wiphy_dereference(ah->hw->wiphy,
						      ahsta->link[tmp_link_id]);
			tmp_arvif = wiphy_dereference(hw->wiphy,
						      ahvif->link[tmp_link_id]);
			tmp_ar = tmp_arvif->ar;
			if (!tmp_ar) {
				ath12k_warn(ar->ab,
					    "%s: Failed to remap deflink, ar not found\n",
					    __func__);
				return -EINVAL;
			}

			if (tmp_arsta) {
				wiphy_work_cancel(ar->ah->hw->wiphy, &tmp_arsta->update_wk);
				memcpy(&ahsta->deflink, tmp_arsta,
				       sizeof(*tmp_arsta));

				/* Free the moved link memory after removing
				 * entry from rhash table.
				 */
				spin_lock_bh(&tmp_ar->ab->base_lock);
				ath12k_link_sta_rhash_delete(tmp_ar->ab, tmp_arsta);
				spin_unlock_bh(&tmp_ar->ab->base_lock);
				kfree(tmp_arsta);

				def_arsta = &ahsta->deflink;
				def_arsta->rhash_done = false;
				wiphy_work_init(&def_arsta->update_wk, ath12k_sta_rc_update_wk);
				ahsta->assoc_link_id = tmp_link_id;
				rcu_assign_pointer(ahsta->link[tmp_link_id], def_arsta);
				synchronize_rcu();

				/* Re-add the deflink addr to hash table
				 */
				spin_lock_bh(&tmp_ar->ab->base_lock);
				ath12k_link_sta_rhash_add(tmp_ar->ab, def_arsta);
				spin_unlock_bh(&tmp_ar->ab->base_lock);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_change_sta_links);

void ath12k_mac_op_set_dscp_tid(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct cfg80211_qos_map *qos_map,
				unsigned int link_id)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_link_vif *arvif;
	struct ath12k_qos_map *new_qos_map;
	struct ath12k *ar;
	struct ath12k_vif_cache *cache;

	lockdep_assert_wiphy(hw->wiphy);

	guard(mutex)(&ah->hw_mutex);
	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return;

	new_qos_map = kzalloc(sizeof(*new_qos_map), GFP_KERNEL);
	if (!new_qos_map) {
		return;
	}
	memcpy(new_qos_map, qos_map, sizeof(*qos_map));

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
	if (!arvif || !arvif->is_created) {
		ath12k_info(NULL,
			    "qos map changes cached to apply after vdev create\n");
		cache = ath12k_ahvif_get_link_cache(ahvif, link_id);
		if (!cache) {
			kfree(new_qos_map);
			return;
		}
		cache->cache_qos_map.qos_map = new_qos_map;
		return;
	}

	ar = arvif->ar;
	if (!ar) {
		ath12k_err(NULL, "Failed to set DSCP to TID mapping\n");
		kfree(new_qos_map);
		return;
	}

	spin_lock_bh(&ar->data_lock);
	arvif->qos_map = new_qos_map;
	wiphy_work_queue(hw->wiphy, &arvif->set_dscp_tid_work);
	spin_unlock_bh(&ar->data_lock);
}
EXPORT_SYMBOL(ath12k_mac_op_set_dscp_tid);

static void ath12k_mac_update_qos_map(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	struct ath12k_qos_map *qos_map;
	struct ath12k_dp_link_vif *dp_link_vif;
	struct ath12k_vif *ahvif = arvif->ahvif;
	u8 dscp_low, dscp_high;
	u8 dscp;
	u8 tid, map_id, bank_id;
	u8 i;

	qos_map = arvif->qos_map;
	map_id = arvif->map_id;
	dp_link_vif = &ahvif->dp_vif.dp_link_vif[arvif->link_id];
	bank_id = dp_link_vif->bank_id;

	if (map_id >= HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX) {
		ath12k_err(ar->ab, "failed to find free map_id\n");
		goto free_qos_map;
	}

	if (bank_id == DP_INVALID_BANK_ID) {
		ath12k_err(ar->ab, "unable to find TX bank profile\n");
		goto free_qos_map;
	}

	for (i = 0; i < ATH12K_MAX_TID_VALUE; i++) {
		dscp_low = qos_map->up[i].low;
		dscp_high = qos_map->up[i].high;
		tid = i;

		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"dscp_low:%d, dscp_high:%d, tid:%d, map_id:%d, bank_id:%d\n",
				dscp_low, dscp_high, tid, map_id, bank_id);
		if (dscp_low == 0xFF || dscp_high == 0xFF)
			continue;
		for (dscp = dscp_low; dscp <= dscp_high; dscp++) {
			ath12k_hal_tx_update_dscp_tid_map(ar->ab, map_id, dscp, tid);
		}
	}

	if (qos_map->num_des > 0) {
		for (i = 0; i < qos_map->num_des; i++) {
			dscp = qos_map->dscp_exception[i].dscp;
			tid = qos_map->dscp_exception[i].up;

			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"dscp:%d, tid: %d, map_id:%d, bank_id:%d\n",
					 dscp, tid, map_id, bank_id);
			if (dscp == 0xFF)
				continue;
			ath12k_hal_tx_update_dscp_tid_map(ar->ab, map_id, dscp, tid);
		}
	}

free_qos_map:
	kfree(qos_map);
	qos_map = NULL;
	return;
}

static void ath12k_set_dscp_tid_work(struct wiphy *wiphy, struct wiphy_work *work)
{
	struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif, set_dscp_tid_work);
	struct ath12k_qos_map *qos_map;
	struct ath12k *ar;

	lockdep_assert_wiphy(wiphy);
	qos_map = arvif->qos_map;
	ar = arvif->ar;
	if (!ar) {
		ath12k_err(NULL, "Failed to set DSCP to TID mapping\n");
		kfree(arvif->qos_map);
		arvif->qos_map = NULL;
		return;
	}

	spin_lock_bh(&ar->data_lock);
	ath12k_mac_update_qos_map(ar, arvif);
	spin_unlock_bh(&ar->data_lock);
}

static u8 ath12k_mac_ahsta_get_pri_link_id(struct ath12k_vif *ahvif,
					   struct ath12k_sta *ahsta,
					   unsigned long int valid_links)
{
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_sta *sta;
	struct ath12k *ar;
	struct ath12k_hw_group *ag;
	u8 link_id = 0, pri_link_id;
	bool is_link_found = false;
	unsigned long links_map;
	u16 pref_valid_links = 0;
	u8 active_num_devices = 0;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (WARN_ON(!valid_links))
		return IEEE80211_MLD_MAX_NUM_LINKS;

	sta = container_of((void *)ahsta, struct ieee80211_sta, drv_priv);

	/* Handle conversion of user configured hw link id to primary link id
	 */
	if (ahvif->vif->type == NL80211_IFTYPE_STATION || ahvif->vif->type == NL80211_IFTYPE_AP) {
		links_map = ahvif->links_map;
		for_each_set_bit_from(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
			arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
			if (!arvif)
				continue;
			ar = arvif->ar;
			if (!ar)
				continue;
			if (ahvif->hw_link_id == ar->radio_idx) {
				is_link_found = true;
				break;
			}
		}
		if (is_link_found)
			ahvif->primary_link_id = arvif->link_id;
	}

	/* if not configured in debugfs then proceed to find a suitable
	 * link with available links
	 */
	if (!test_bit(ahvif->primary_link_id, &valid_links))
		goto select_pri_link;

	/* if the configured link id is present and it is preferable,
	 * take that link as the primary link
	 */
	arvif = ath12k_get_arvif_from_link_id(ahvif, ahvif->primary_link_id);
	if (arvif->ar->ab->hw_params->is_plink_preferable) {
		pri_link_id = ahvif->primary_link_id;
		goto exit_pri_link_selection;
	}

select_pri_link:
	/* among all available links, get the preferable links bitmap */
	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->ar)
			continue;

		if (!arvif->ar->ab->hw_params->is_plink_preferable)
			continue;

		/* If link removal in progress, do not consider it */
		if (arvif->is_link_removal_in_progress)
			continue;

		pref_valid_links |= BIT(link_id);
	}

	/* all the links available currently are not preferable but no
	 * other choice hence need to select among these
	 */
	if (!pref_valid_links)
		links_map = valid_links;
	else
		links_map = pref_valid_links;

	/* TODO: Currently just selecting using ffs(). Proper logic can be
	 * 	used here to select among these by using some other run
	 *	time parameters like RSSI.
	 */
	pri_link_id = ffs(links_map) - 1;

exit_pri_link_selection:
	if (!arvif || !arvif->ar || !arvif->ar->ab || !arvif->ar->ab->ag)
		return pri_link_id;

	ag = arvif->ar->ab->ag;
	active_num_devices = ag->num_devices - ag->num_bypassed;

	ath12k_mac_assign_middle_link_id(sta, ahsta, &pri_link_id,
					 active_num_devices);

	return pri_link_id;
}

static bool ath12k_mac_ahsta_is_migra_link_valid(struct ath12k_sta *ahsta,
						 u8 link_id)
{
	struct ath12k_vif *ahvif = ahsta->ahvif;
	struct ath12k_hw *ah = ahvif->ah;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (!(ahsta->links_map & BIT(link_id)))
		return false;

	if (ahsta->primary_link_id == link_id)
		return false;

	return true;
}

static bool ath12k_mac_ahsta_can_migrate(struct ath12k_sta *ahsta)
{
	unsigned long int valid_links = ahsta->links_map;
	struct ath12k_vif *ahvif = ahsta->ahvif;
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k_link_sta *arsta;
	u8 link_id;

	lockdep_assert_wiphy(ah->hw->wiphy);

	/* Currently bridge peer is not supprted. */
	valid_links &= ~ATH12K_IEEE80211_MLD_MAX_LINKS_MASK;
	for_each_set_bit(link_id, &valid_links, ATH12K_NUM_MAX_LINKS) {
		arsta = ahsta->link[link_id];
		if (!arsta)
			continue;

		if (arsta->is_bridge_peer)
			return false;
	}

	if (ahsta->state < IEEE80211_STA_ASSOC)
		return false;

	if (ahsta->is_migration_in_progress)
		return false;

	return true;
}

static int ath12k_mac_get_next_pri_link(struct ath12k_sta *ahsta, u8 *pri_link_id)
{
	struct ath12k_vif *ahvif = ahsta->ahvif;
	struct ath12k_hw *ah = ahvif->ah;
	struct ieee80211_sta *sta;
	u16 curr_links = ahsta->links_map;
	u16 links_map;

	lockdep_assert_wiphy(ah->hw->wiphy);

	sta = container_of((void *)ahsta, struct ieee80211_sta, drv_priv);

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "cur_pri_link %u, valid_links 0x%x for ML sta %pM\n",
		   ahsta->primary_link_id, curr_links, sta->addr);

	/* exclude the current primary link id from consideration */
	links_map = curr_links & ~BIT(ahsta->primary_link_id);

	/* Also exclude any links marked for removal in ML reconfig */
	links_map &= ~sta->reconf.removed_links;

	/* if only no link is available then can not really migrate*/
	if (!links_map)
		return -EINVAL;

	*pri_link_id = ath12k_mac_ahsta_get_pri_link_id(ahvif, ahsta, links_map);
	if (*pri_link_id == IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "link %u selected as primary link for ML sta %pM\n",
		   *pri_link_id, sta->addr);

	return 0;
}

static void
ath12k_mac_free_link_migr_peer_list(struct ath12k_hw *ah,
				    struct list_head *peer_migr_list)
{
	struct ath12k_mac_pri_link_migr_peer_node *peer_node, *tmp_peer;
	struct ath12k_dp_peer *ml_peer;
	struct ath12k_sta *ahsta;

	lockdep_assert_held(&ah->dp_hw.peer_lock);

	/* This list contains only the peers failed to send migration
	 * request to firmware. No need to take further action here,
	 * the requester of this migration request will handle these
	 * peers later as per the requirement
	 */
	list_for_each_entry_safe(peer_node, tmp_peer, peer_migr_list, list) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "pri link migrate: free ml_peer_id %u from migrate list\n",
			   peer_node->ml_peer_id);

		rcu_read_lock();
		/* TODO: Need to check if we ml_peer_id validation
		 */
		ml_peer = rcu_dereference(ah->dp_hw.dp_peer_list[peer_node->ml_peer_id]);

		if (ml_peer) {
			ahsta = ath12k_sta_to_ahsta(ml_peer->sta);
			ahsta->is_migration_in_progress = false;
		}

		rcu_read_unlock();
		list_del(&peer_node->list);
		kfree(peer_node);
	}
}

static struct ath12k_mac_pri_link_migr_peer_node *
ath12k_mac_get_link_migr_peer_node(struct ath12k_dp_peer *ml_peer,
				   u8 pri_link_id)
{
	struct ath12k_mac_pri_link_migr_peer_node *node;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(ml_peer->sta);
	struct ath12k_vif *ahvif = ahsta->ahvif;
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k_link_sta *arsta = ahsta->link[pri_link_id];
	struct ath12k_link_vif *arvif;
	u8 hw_link_id;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (!arsta)
		return NULL;

	arvif = arsta->arvif;
	if (!arvif || !arvif->ar)
		return NULL;

	hw_link_id = arvif->ar->pdev->hw_link_id;

	node = kzalloc(sizeof(*node), GFP_ATOMIC);
	if (!node)
		return NULL;

	INIT_LIST_HEAD(&node->list);
	node->ml_peer_id = ml_peer->peer_id & ~ATH12K_PEER_ML_ID_VALID;
	node->hw_link_id = hw_link_id;

	return node;
}

static int ath12k_mac_handle_sta_migration(struct ath12k_dp_peer *ml_peer,
					   struct ath12k_sta *ahsta, u8 link_id,
					   struct list_head *list_head, int *num_peers)
{
	struct ath12k_mac_pri_link_migr_peer_node *peer_node;
	u8 pri_link_id;
	int ret;

	if (!ath12k_mac_ahsta_can_migrate(ahsta))
		return -EPERM;

	if (link_id == 0xFF) {
		ret = ath12k_mac_get_next_pri_link(ahsta, &pri_link_id);
		if (ret)
			return ret;
	} else if (ath12k_mac_ahsta_is_migra_link_valid(ahsta, link_id)) {
		pri_link_id = link_id;
	} else {
		return -EINVAL;
	}

	peer_node = ath12k_mac_get_link_migr_peer_node(ml_peer, pri_link_id);
	if (!peer_node)
		return -ENOMEM;

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "ML sta %pM will migrate pri link to link_id %u hw_link_id %u\n",
		   ml_peer->addr, pri_link_id, peer_node->hw_link_id);

	list_add(&peer_node->list, list_head);
	ahsta->is_migration_in_progress = true;
	(*num_peers)++;

	return 0;
}

int
ath12k_mac_process_link_migrate_req(struct ath12k_vif *ahvif,
				    struct ath12k_mac_link_migrate_usr_params *params)
{
	struct ath12k_link_vif *arvif, *arvif_itr;
	struct ath12k_hw *ah = ahvif->ah;
	struct list_head peer_migr_list;
	struct ath12k_dp_peer *ml_peer;
	struct ath12k_link_sta *arsta;
	unsigned long int valid_links = ahvif->links_map;
	struct ath12k_sta *ahsta;
	int ret, num_peers = 0;
	struct ath12k *ar;
	u8 link_id;
	bool found;

	INIT_LIST_HEAD(&peer_migr_list);

	lockdep_assert_wiphy(ah->hw->wiphy);

	/* Check if firmware supports migration */
	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif)
			continue;

		if (!arvif->ar)
			return -ENOTCONN;

		if (!ath12k_wmi_is_umac_migration_supported(arvif->ar->ab))
			return -EOPNOTSUPP;
	}

	spin_lock_bh(&ah->dp_hw.peer_lock);

	/* Request for single MLD peer */
	if (!is_zero_ether_addr(params->addr)) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "pri link migrate: single peer migration\n");

		ml_peer = ath12k_dp_peer_find(&ah->dp_hw, params->addr);
		if (!ml_peer) {
			ret = -ENODEV;
			goto exit_link_migrate_req;
		}

		if (!ml_peer->is_mlo || ml_peer->is_vdev_peer) {
			ret = -EINVAL;
			goto exit_link_migrate_req;
		}

		ahsta = ath12k_sta_to_ahsta(ml_peer->sta);

		arvif = ath12k_get_arvif_from_link_id(ahvif, ahsta->primary_link_id);
		if (!arvif || !arvif->is_up || !arvif->ar) {
			ret = -ENODEV;
			goto exit_link_migrate_req;
		}

		ar = arvif->ar;

		ret = ath12k_mac_handle_sta_migration(ml_peer, ahsta, params->link_id,
						      &peer_migr_list, &num_peers);
		if (ret)
			goto exit_link_migrate_req;

		goto send_link_mig_cmd;
	}

	/* Request for all MLD peers using given link's SOC as primary SOC */
	if (params->link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		ret = -EINVAL;
		goto exit_link_migrate_req;
	}

	ath12k_dbg(NULL, ATH12K_DBG_MAC, "pri link migrate: link migration\n");

	arvif = ath12k_get_arvif_from_link_id(ahvif, params->link_id);
	if (!arvif || !arvif->is_up || !arvif->ar) {
		ret = -EINVAL;
		goto exit_link_migrate_req;
	}

	ar = arvif->ar;

	list_for_each_entry(ml_peer, &ah->dp_hw.peers, list) {
		if (!ml_peer->is_mlo || ml_peer->is_vdev_peer)
			continue;

		ahsta = ath12k_sta_to_ahsta(ml_peer->sta);

		valid_links = ahsta->links_map;
		found = false;

		for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
			arsta = ahsta->link[link_id];
			if (!arsta)
				continue;

			arvif_itr = arsta->arvif;
			if (!arvif_itr || !arvif_itr->ar)
				continue;

			/* If the link arvif is not the same as requested link vdev,
			 * do not consider for migration
			 */
			if (arvif_itr != arvif)
				continue;

			/* If current primary is not on the provided link pdev, do not
			 * consider for migration
			 */
			if (ahsta->primary_link_id != arvif_itr->link_id)
				continue;

			found = true;
			break;
		}

		if (!found)
			continue;

		ret = ath12k_mac_handle_sta_migration(ml_peer, ahsta, 0xFF,
						      &peer_migr_list, &num_peers);
		/* Errors are now ignored to prevent skipping valid peers*/
		if (ret)
			ath12k_err(ar->ab, "Primary migration skipped for %pM ret:%d\n",
				   ml_peer->addr, ret);
	}

send_link_mig_cmd:
	if (num_peers == 0)
		ret = -EINVAL;
	else
		ret = 0;

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "pri link migrate: got num peers %d for vdev_id %d\n",
		   num_peers, arvif->vdev_id);

	if (ret)
		goto exit_link_migrate_req;

	spin_unlock_bh(&ah->dp_hw.peer_lock);

	ret = ath12k_wmi_mlo_send_ptqm_migrate_cmd(arvif,
						   &peer_migr_list, num_peers);
	if (ret)
		ath12k_err(arvif->ar->ab, "Failed to migrate pri link ret %d\n",
			   ret);

	spin_lock_bh(&ah->dp_hw.peer_lock);

exit_link_migrate_req:
	ath12k_mac_free_link_migr_peer_list(ah, &peer_migr_list);
	spin_unlock_bh(&ah->dp_hw.peer_lock);
	return ret;
}

bool ath12k_mac_op_removed_link_is_primary(struct ieee80211_sta *sta,
					   u16 removed_links)
{
	return ath12k_is_primary_link_migrate(sta, removed_links);
}
EXPORT_SYMBOL(ath12k_mac_op_removed_link_is_primary);

bool ath12k_mac_op_can_activate_links(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      u16 active_links)
{
	/* TODO: Handle recovery case */

	return true;
}
EXPORT_SYMBOL(ath12k_mac_op_can_activate_links);

static int ath12k_conf_tx_uapsd(struct ath12k_link_vif *arvif,
				u16 ac, bool enable)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;
	u32 value = 0;
	int ret;

	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA)
		return 0;

	switch (ac) {
	case IEEE80211_AC_VO:
		value = WMI_STA_PS_UAPSD_AC3_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC3_TRIGGER_EN;
		break;
	case IEEE80211_AC_VI:
		value = WMI_STA_PS_UAPSD_AC2_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC2_TRIGGER_EN;
		break;
	case IEEE80211_AC_BE:
		value = WMI_STA_PS_UAPSD_AC1_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC1_TRIGGER_EN;
		break;
	case IEEE80211_AC_BK:
		value = WMI_STA_PS_UAPSD_AC0_DELIVERY_EN |
			WMI_STA_PS_UAPSD_AC0_TRIGGER_EN;
		break;
	}

	if (enable)
		ahvif->u.sta.uapsd |= value;
	else
		ahvif->u.sta.uapsd &= ~value;

	ret = ath12k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
					  WMI_STA_PS_PARAM_UAPSD,
					  ahvif->u.sta.uapsd);
	if (ret) {
		ath12k_warn(ar->ab, "could not set uapsd params %d\n", ret);
		goto exit;
	}

	if (ahvif->u.sta.uapsd)
		value = WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
	else
		value = WMI_STA_PS_RX_WAKE_POLICY_WAKE;

	ret = ath12k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
					  WMI_STA_PS_PARAM_RX_WAKE_POLICY,
					  value);
	if (ret)
		ath12k_warn(ar->ab, "could not set rx wake param %d\n", ret);

exit:
	return ret;
}

int ath12k_mac_conf_tx(struct ath12k_link_vif *arvif, u16 ac,
			      const struct ieee80211_tx_queue_params *params)
{
	struct wmi_wmm_params_arg *p = NULL;
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	switch (ac) {
	case IEEE80211_AC_VO:
		p = &arvif->wmm_params.ac_vo;
		break;
	case IEEE80211_AC_VI:
		p = &arvif->wmm_params.ac_vi;
		break;
	case IEEE80211_AC_BE:
		p = &arvif->wmm_params.ac_be;
		break;
	case IEEE80211_AC_BK:
		p = &arvif->wmm_params.ac_bk;
		break;
	}

	if (WARN_ON(!p)) {
		ret = -EINVAL;
		goto exit;
	}

	p->cwmin = params->cw_min;
	p->cwmax = params->cw_max;
	p->aifs = params->aifs;
	p->txop = params->txop;

	ret = ath12k_wmi_send_wmm_update_cmd(ar, arvif->vdev_id,
					     &arvif->wmm_params);
	if (ret) {
		ath12k_warn(ab, "pdev idx %d failed to set wmm params: %d\n",
			    ar->pdev_idx, ret);
		goto exit;
	}

	ret = ath12k_conf_tx_uapsd(arvif, ac, params->uapsd);
	if (ret)
		ath12k_warn(ab, "pdev idx %d failed to set sta uapsd: %d\n",
			    ar->pdev_idx, ret);

exit:
	return ret;
}

int ath12k_mac_op_conf_tx(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  unsigned int link_id, u16 ac,
			  const struct ieee80211_tx_queue_params *params)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k_vif_cache *cache;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
	if (!arvif || !arvif->is_created) {
		cache = ath12k_ahvif_get_link_cache(ahvif, link_id);
		if (!cache)
			return -ENOSPC;

		cache->tx_conf.changed = true;
		cache->tx_conf.ac = ac;
		cache->tx_conf.tx_queue_params = *params;

		return 0;
	}

	ret = ath12k_mac_conf_tx(arvif, ac, params);

	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_conf_tx);

static struct ieee80211_sta_ht_cap
ath12k_create_ht_cap(struct ath12k *ar, u32 ar_ht_cap, u32 rate_cap_rx_chainmask)
{
	int i;
	struct ieee80211_sta_ht_cap ht_cap = {0};
	u32 ar_vht_cap = ar->pdev->cap.vht_cap;

	if (!(ar_ht_cap & WMI_HT_CAP_ENABLED))
		return ht_cap;

	ht_cap.ht_supported = 1;
	ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
	ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_NONE;
	ht_cap.cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	ht_cap.cap |= IEEE80211_HT_CAP_DSSSCCK40;
	ht_cap.cap |= WLAN_HT_CAP_SM_PS_STATIC << IEEE80211_HT_CAP_SM_PS_SHIFT;

	if (ar_ht_cap & WMI_HT_CAP_HT20_SGI)
		ht_cap.cap |= IEEE80211_HT_CAP_SGI_20;

	if (ar_ht_cap & WMI_HT_CAP_HT40_SGI)
		ht_cap.cap |= IEEE80211_HT_CAP_SGI_40;

	if (ar_ht_cap & WMI_HT_CAP_DYNAMIC_SMPS) {
		u32 smps;

		smps   = WLAN_HT_CAP_SM_PS_DYNAMIC;
		smps <<= IEEE80211_HT_CAP_SM_PS_SHIFT;

		ht_cap.cap |= smps;
	}

	if (ar_ht_cap & WMI_HT_CAP_TX_STBC)
		ht_cap.cap |= IEEE80211_HT_CAP_TX_STBC;

	if (ar_ht_cap & WMI_HT_CAP_RX_STBC) {
		u32 stbc;

		stbc   = ar_ht_cap;
		stbc  &= WMI_HT_CAP_RX_STBC;
		stbc >>= WMI_HT_CAP_RX_STBC_MASK_SHIFT;
		stbc <<= IEEE80211_HT_CAP_RX_STBC_SHIFT;
		stbc  &= IEEE80211_HT_CAP_RX_STBC;

		ht_cap.cap |= stbc;
	}

	if (ar_ht_cap & WMI_HT_CAP_RX_LDPC)
		ht_cap.cap |= IEEE80211_HT_CAP_LDPC_CODING;

	if (ar_ht_cap & WMI_HT_CAP_L_SIG_TXOP_PROT)
		ht_cap.cap |= IEEE80211_HT_CAP_LSIG_TXOP_PROT;

	if (ar_vht_cap & WMI_VHT_CAP_MAX_MPDU_LEN_MASK)
		ht_cap.cap |= IEEE80211_HT_CAP_MAX_AMSDU;

	for (i = 0; i < ar->num_rx_chains; i++) {
			ht_cap.mcs.rx_mask[i] = 0xFF;
	}

	ht_cap.mcs.tx_params |= IEEE80211_HT_MCS_TX_DEFINED;

	return ht_cap;
}

static int ath12k_mac_set_txbf_conf(struct ath12k_link_vif *arvif)
{
	u32 value = 0;
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;
	int nsts;
	int sound_dim;
	u32 vht_cap = ar->pdev->cap.vht_cap;
	u32 vdev_param = WMI_VDEV_PARAM_TXBF;

	if (vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE)) {
		nsts = vht_cap & IEEE80211_VHT_CAP_BEAMFORMEE_STS_MASK;
		nsts >>= IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT;
		value |= SM(nsts, WMI_TXBF_STS_CAP_OFFSET);
	}

	if (vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE)) {
		sound_dim = vht_cap &
			    IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK;
		sound_dim >>= IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT;
		if (sound_dim > (ar->num_tx_chains - 1))
			sound_dim = ar->num_tx_chains - 1;
		value |= SM(sound_dim, WMI_BF_SOUND_DIM_OFFSET);
	}

	if (!value)
		return 0;

	if (vht_cap & IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE) {
		value |= WMI_VDEV_PARAM_TXBF_SU_TX_BFER;

		if ((vht_cap & IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE) &&
		    ahvif->vdev_type == WMI_VDEV_TYPE_AP)
			value |= WMI_VDEV_PARAM_TXBF_MU_TX_BFER;
	}

	if (vht_cap & IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE) {
		value |= WMI_VDEV_PARAM_TXBF_SU_TX_BFEE;

		if ((vht_cap & IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE) &&
		    ahvif->vdev_type == WMI_VDEV_TYPE_STA)
			value |= WMI_VDEV_PARAM_TXBF_MU_TX_BFEE;
	}

	return ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					     vdev_param, value);
}

static void ath12k_set_vht_txbf_cap(struct ath12k *ar, u32 *vht_cap)
{
	bool subfer, subfee;
	int sound_dim = 0;

	subfer = !!(*vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE));
	subfee = !!(*vht_cap & (IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE));

	if (ar->num_tx_chains < 2) {
		*vht_cap &= ~(IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE);
		subfer = false;
	}

	/* If SU Beaformer is not set, then disable MU Beamformer Capability */
	if (!subfer)
		*vht_cap &= ~(IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE);

	/* If SU Beaformee is not set, then disable MU Beamformee Capability */
	if (!subfee)
		*vht_cap &= ~(IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE);

	sound_dim = u32_get_bits(*vht_cap,
				 IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK);
	*vht_cap = u32_replace_bits(*vht_cap, 0,
				    IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK);

	/* TODO: Need to check invalid STS and Sound_dim values set by FW? */

	/* Enable Sounding Dimension Field only if SU BF is enabled */
	if (subfer) {
		if (sound_dim > (ar->num_tx_chains - 1))
			sound_dim = ar->num_tx_chains - 1;

		*vht_cap = u32_replace_bits(*vht_cap, sound_dim,
					    IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK);
	}

	/* Use the STS advertised by FW unless SU Beamformee is not supported*/
	if (!subfee)
		*vht_cap &= ~(IEEE80211_VHT_CAP_BEAMFORMEE_STS_MASK);
}

static struct ieee80211_sta_vht_cap
ath12k_create_vht_cap(struct ath12k *ar, u32 rate_cap_tx_chainmask,
		      u32 rate_cap_rx_chainmask)
{
	struct ieee80211_sta_vht_cap vht_cap = {0};
	u16 txmcs_map, rxmcs_map;
	int i;

	vht_cap.vht_supported = 1;
	vht_cap.cap = ar->pdev->cap.vht_cap;

	if (ar->pdev->cap.nss_ratio_enabled)
		vht_cap.vht_mcs.tx_highest |=
			cpu_to_le16(IEEE80211_VHT_EXT_NSS_BW_CAPABLE);

	ath12k_set_vht_txbf_cap(ar, &vht_cap.cap);

	/* 80P80 is not supported */
	vht_cap.cap &= ~IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;

	rxmcs_map = 0;
	txmcs_map = 0;
	for (i = 0; i < 8; i++) {
		if (i < ar->num_tx_chains)
			txmcs_map |= IEEE80211_VHT_MCS_SUPPORT_0_9 << (i * 2);
		else
			txmcs_map |= IEEE80211_VHT_MCS_NOT_SUPPORTED << (i * 2);

		if (i < ar->num_rx_chains)
			rxmcs_map |= IEEE80211_VHT_MCS_SUPPORT_0_9 << (i * 2);
		else
			rxmcs_map |= IEEE80211_VHT_MCS_NOT_SUPPORTED << (i * 2);
	}

	if (rate_cap_tx_chainmask <= 1)
		vht_cap.cap &= ~IEEE80211_VHT_CAP_TXSTBC;

	vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(rxmcs_map);
	vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(txmcs_map);

	/* Check if the HW supports 1:1 NSS ratio and reset
	 * EXT NSS BW Support field to 0 to indicate 1:1 ratio
	 */
	if (ar->pdev->cap.nss_ratio_info == WMI_NSS_RATIO_1_NSS)
		vht_cap.cap &= ~IEEE80211_VHT_CAP_EXT_NSS_BW_MASK;

	return vht_cap;
}

static void ath12k_mac_setup_ht_vht_cap(struct ath12k *ar,
					struct ath12k_pdev_cap *cap,
					u32 *ht_cap_info)
{
	struct ieee80211_supported_band *band, *band_wiphy;
	u32 rate_cap_tx_chainmask;
	u32 rate_cap_rx_chainmask;
	u32 ht_cap;

	rate_cap_tx_chainmask = ar->cfg_tx_chainmask >> cap->tx_chain_mask_shift;
	rate_cap_rx_chainmask = ar->cfg_rx_chainmask >> cap->rx_chain_mask_shift;

	if (cap->supported_bands & WMI_HOST_WLAN_2GHZ_CAP) {
		band = &ar->mac.sbands[NL80211_BAND_2GHZ];
		band_wiphy = ar->ah->hw->wiphy->bands[NL80211_BAND_2GHZ];
		ht_cap = cap->band[NL80211_BAND_2GHZ].ht_cap_info;
		if (ht_cap_info)
			*ht_cap_info = ht_cap;
		band->ht_cap = ath12k_create_ht_cap(ar, ht_cap,
						    rate_cap_rx_chainmask);
		band->vht_cap = ath12k_create_vht_cap(ar, rate_cap_tx_chainmask,
						    rate_cap_rx_chainmask);
		/* Update wiphy sband info if sband structure is set/cleared */
		if (band != band_wiphy) {
			band_wiphy->ht_cap =  band->ht_cap;
			band_wiphy->vht_cap = band->vht_cap;
			/* set/clear the value if it was duped */
			band_wiphy->vht_cap.vht_mcs.tx_highest ^=
				cpu_to_le16(IEEE80211_VHT_EXT_NSS_BW_CAPABLE);
		}
	}

	if (cap->supported_bands & WMI_HOST_WLAN_5GHZ_CAP &&
	    (ar->ab->hw_params->single_pdev_only ||
	     !ar->supports_6ghz)) {
		band = &ar->mac.sbands[NL80211_BAND_5GHZ];
		ht_cap = cap->band[NL80211_BAND_5GHZ].ht_cap_info;
		if (ht_cap_info)
			*ht_cap_info = ht_cap;
		band->ht_cap = ath12k_create_ht_cap(ar, ht_cap,
						    rate_cap_rx_chainmask);
		band->vht_cap = ath12k_create_vht_cap(ar, rate_cap_tx_chainmask,
						      rate_cap_rx_chainmask);
	}
}

static int ath12k_check_chain_mask(struct ath12k *ar, u32 ant, bool is_tx_ant)
{
	/* TODO: Check the request chainmask against the supported
	 * chainmask table which is advertised in extented_service_ready event
	 */

	return 0;
}

static void ath12k_gen_ppe_thresh(struct ath12k_wmi_ppe_threshold_arg *fw_ppet,
				  u8 *he_ppet)
{
	int nss, ru;
	u8 bit = 7;

	he_ppet[0] = fw_ppet->numss_m1 & IEEE80211_PPE_THRES_NSS_MASK;
	he_ppet[0] |= (fw_ppet->ru_bit_mask <<
		       IEEE80211_PPE_THRES_RU_INDEX_BITMASK_POS) &
		      IEEE80211_PPE_THRES_RU_INDEX_BITMASK_MASK;
	for (nss = 0; nss <= fw_ppet->numss_m1; nss++) {
		for (ru = 0; ru < 4; ru++) {
			u8 val;
			int i;

			if ((fw_ppet->ru_bit_mask & BIT(ru)) == 0)
				continue;
			val = (fw_ppet->ppet16_ppet8_ru3_ru0[nss] >> (ru * 6)) &
			       0x3f;
			val = ((val >> 3) & 0x7) | ((val & 0x7) << 3);
			for (i = 5; i >= 0; i--) {
				he_ppet[bit / 8] |=
					((val >> i) & 0x1) << ((bit % 8));
				bit++;
			}
		}
	}
}

static void
ath12k_mac_filter_he_cap_mesh(struct ieee80211_he_cap_elem *he_cap_elem)
{
	u8 m;

	m = IEEE80211_HE_MAC_CAP0_TWT_RES |
	    IEEE80211_HE_MAC_CAP0_TWT_REQ;
	he_cap_elem->mac_cap_info[0] &= ~m;

	m = IEEE80211_HE_MAC_CAP2_TRS |
	    IEEE80211_HE_MAC_CAP2_BCAST_TWT |
	    IEEE80211_HE_MAC_CAP2_MU_CASCADING;
	he_cap_elem->mac_cap_info[2] &= ~m;

	m = IEEE80211_HE_MAC_CAP3_FLEX_TWT_SCHED |
	    IEEE80211_HE_MAC_CAP2_BCAST_TWT |
	    IEEE80211_HE_MAC_CAP2_MU_CASCADING;
	he_cap_elem->mac_cap_info[3] &= ~m;

	m = IEEE80211_HE_MAC_CAP4_BSRP_BQRP_A_MPDU_AGG |
	    IEEE80211_HE_MAC_CAP4_BQR;
	he_cap_elem->mac_cap_info[4] &= ~m;

	m = IEEE80211_HE_MAC_CAP5_SUBCHAN_SELECTIVE_TRANSMISSION |
	    IEEE80211_HE_MAC_CAP5_UL_2x996_TONE_RU |
	    IEEE80211_HE_MAC_CAP5_PUNCTURED_SOUNDING |
	    IEEE80211_HE_MAC_CAP5_HT_VHT_TRIG_FRAME_RX;
	he_cap_elem->mac_cap_info[5] &= ~m;

	m = IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
	    IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO;
	he_cap_elem->phy_cap_info[2] &= ~m;

	m = IEEE80211_HE_PHY_CAP3_RX_PARTIAL_BW_SU_IN_20MHZ_MU |
	    IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_MASK |
	    IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_MASK;
	he_cap_elem->phy_cap_info[3] &= ~m;

	m = IEEE80211_HE_PHY_CAP4_MU_BEAMFORMER;
	he_cap_elem->phy_cap_info[4] &= ~m;

	m = IEEE80211_HE_PHY_CAP5_NG16_MU_FEEDBACK;
	he_cap_elem->phy_cap_info[5] &= ~m;

	m = IEEE80211_HE_PHY_CAP6_CODEBOOK_SIZE_75_MU |
	    IEEE80211_HE_PHY_CAP6_TRIG_MU_BEAMFORMING_PARTIAL_BW_FB |
	    IEEE80211_HE_PHY_CAP6_TRIG_CQI_FB |
	    IEEE80211_HE_PHY_CAP6_PARTIAL_BANDWIDTH_DL_MUMIMO;
	he_cap_elem->phy_cap_info[6] &= ~m;

	m = IEEE80211_HE_PHY_CAP7_PSR_BASED_SR |
	    IEEE80211_HE_PHY_CAP7_POWER_BOOST_FACTOR_SUPP |
	    IEEE80211_HE_PHY_CAP7_STBC_TX_ABOVE_80MHZ |
	    IEEE80211_HE_PHY_CAP7_STBC_RX_ABOVE_80MHZ;
	he_cap_elem->phy_cap_info[7] &= ~m;

	m = IEEE80211_HE_PHY_CAP8_HE_ER_SU_PPDU_4XLTF_AND_08_US_GI |
	    IEEE80211_HE_PHY_CAP8_20MHZ_IN_40MHZ_HE_PPDU_IN_2G |
	    IEEE80211_HE_PHY_CAP8_20MHZ_IN_160MHZ_HE_PPDU |
	    IEEE80211_HE_PHY_CAP8_80MHZ_IN_160MHZ_HE_PPDU;
	he_cap_elem->phy_cap_info[8] &= ~m;

	m = IEEE80211_HE_PHY_CAP9_LONGER_THAN_16_SIGB_OFDM_SYM |
	    IEEE80211_HE_PHY_CAP9_NON_TRIGGERED_CQI_FEEDBACK |
	    IEEE80211_HE_PHY_CAP9_RX_1024_QAM_LESS_THAN_242_TONE_RU |
	    IEEE80211_HE_PHY_CAP9_TX_1024_QAM_LESS_THAN_242_TONE_RU |
	    IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_COMP_SIGB |
	    IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_NON_COMP_SIGB;
	he_cap_elem->phy_cap_info[9] &= ~m;
}

static __le16 ath12k_mac_setup_he_6ghz_cap(struct ath12k_pdev_cap *pcap,
					   struct ath12k_band_cap *bcap)
{
	u8 val;

	bcap->he_6ghz_capa = IEEE80211_HT_MPDU_DENSITY_NONE;
	if (bcap->ht_cap_info & WMI_HT_CAP_DYNAMIC_SMPS)
		bcap->he_6ghz_capa |=
			u32_encode_bits(WLAN_HT_CAP_SM_PS_DYNAMIC,
					IEEE80211_HE_6GHZ_CAP_SM_PS);
	else
		bcap->he_6ghz_capa |=
			u32_encode_bits(WLAN_HT_CAP_SM_PS_DISABLED,
					IEEE80211_HE_6GHZ_CAP_SM_PS);
	val = u32_get_bits(pcap->vht_cap,
			   IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK);
	bcap->he_6ghz_capa |=
		u32_encode_bits(val, IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP);
	val = u32_get_bits(pcap->vht_cap,
			   IEEE80211_VHT_CAP_MAX_MPDU_MASK);
	bcap->he_6ghz_capa |=
		u32_encode_bits(val, IEEE80211_HE_6GHZ_CAP_MAX_MPDU_LEN);
	if (pcap->vht_cap & IEEE80211_VHT_CAP_RX_ANTENNA_PATTERN)
		bcap->he_6ghz_capa |= IEEE80211_HE_6GHZ_CAP_RX_ANTPAT_CONS;
	if (pcap->vht_cap & IEEE80211_VHT_CAP_TX_ANTENNA_PATTERN)
		bcap->he_6ghz_capa |= IEEE80211_HE_6GHZ_CAP_TX_ANTPAT_CONS;

	return cpu_to_le16(bcap->he_6ghz_capa);
}

static void ath12k_mac_set_hemcsmap(struct ath12k *ar,
				    struct ath12k_pdev_cap *cap,
				    struct ieee80211_sta_he_cap *he_cap)
{
	struct ieee80211_he_mcs_nss_supp *mcs_nss = &he_cap->he_mcs_nss_supp;
	u8 maxtxnss_160 = ath12k_get_nss_160mhz(ar, ar->num_tx_chains);
	u8 maxrxnss_160 = ath12k_get_nss_160mhz(ar, ar->num_rx_chains);
	u16 txmcs_map_160 = 0, rxmcs_map_160 = 0;
	u16 txmcs_map = 0, rxmcs_map = 0;
	u32 i;

	for (i = 0; i < 8; i++) {
		if (i < ar->num_tx_chains)
			txmcs_map |= IEEE80211_HE_MCS_SUPPORT_0_11 << (i * 2);
		else
			txmcs_map |= IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2);

		if (i < ar->num_rx_chains)
			rxmcs_map |= IEEE80211_HE_MCS_SUPPORT_0_11 << (i * 2);
		else
			rxmcs_map |= IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2);

		if (i < maxtxnss_160 &&
		    (ar->cfg_tx_chainmask >> cap->tx_chain_mask_shift) & BIT(i))
			txmcs_map_160 |= IEEE80211_HE_MCS_SUPPORT_0_11 << (i * 2);
		else
			txmcs_map_160 |= IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2);

		if (i < maxrxnss_160 &&
		    (ar->cfg_rx_chainmask >> cap->rx_chain_mask_shift) & BIT(i))
			rxmcs_map_160 |= IEEE80211_HE_MCS_SUPPORT_0_11 << (i * 2);
		else
			rxmcs_map_160 |= IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2);
	}

	mcs_nss->rx_mcs_80 = cpu_to_le16(rxmcs_map & 0xffff);
	mcs_nss->tx_mcs_80 = cpu_to_le16(txmcs_map & 0xffff);
	mcs_nss->rx_mcs_160 = cpu_to_le16(rxmcs_map_160 & 0xffff);
	mcs_nss->tx_mcs_160 = cpu_to_le16(txmcs_map_160 & 0xffff);
}

static void ath12k_mac_copy_he_cap(struct ath12k *ar,
				   struct ath12k_band_cap *band_cap,
				   int iftype, u8 num_tx_chains,
				   struct ieee80211_sta_he_cap *he_cap)
{
	struct ieee80211_he_cap_elem *he_cap_elem = &he_cap->he_cap_elem;

	he_cap->has_he = true;
	memcpy(he_cap_elem->mac_cap_info, band_cap->he_cap_info,
	       sizeof(he_cap_elem->mac_cap_info));
	memcpy(he_cap_elem->phy_cap_info, band_cap->he_cap_phy_info,
	       sizeof(he_cap_elem->phy_cap_info));

	he_cap_elem->mac_cap_info[1] &=
		IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_MASK;
	he_cap_elem->phy_cap_info[0] &=
		IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G |
		IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
		IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G;
	/* 80PLUS80 is not supported */
	he_cap_elem->phy_cap_info[0] &=
		~IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G;
	he_cap_elem->phy_cap_info[5] &=
		~IEEE80211_HE_PHY_CAP5_BEAMFORMEE_NUM_SND_DIM_UNDER_80MHZ_MASK;
	he_cap_elem->phy_cap_info[5] |= num_tx_chains - 1;

	switch (iftype) {
	case NL80211_IFTYPE_AP:
		he_cap_elem->phy_cap_info[3] &=
			~IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_MASK;
		he_cap_elem->phy_cap_info[9] |=
			IEEE80211_HE_PHY_CAP9_RX_1024_QAM_LESS_THAN_242_TONE_RU;
		break;
	case NL80211_IFTYPE_STATION:
		he_cap_elem->mac_cap_info[0] &= ~IEEE80211_HE_MAC_CAP0_TWT_RES;
		he_cap_elem->mac_cap_info[0] |= IEEE80211_HE_MAC_CAP0_TWT_REQ;
		he_cap_elem->phy_cap_info[9] |=
			IEEE80211_HE_PHY_CAP9_TX_1024_QAM_LESS_THAN_242_TONE_RU;
		break;
	case NL80211_IFTYPE_MESH_POINT:
		ath12k_mac_filter_he_cap_mesh(he_cap_elem);
		break;
	}

	ath12k_mac_set_hemcsmap(ar, &ar->pdev->cap, he_cap);
	memset(he_cap->ppe_thres, 0, sizeof(he_cap->ppe_thres));
	if (he_cap_elem->phy_cap_info[6] &
	    IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT)
		ath12k_gen_ppe_thresh(&band_cap->he_ppet, he_cap->ppe_thres);
}

static
u32 intersect_eht_mcsnss_map_nss(u32 eht_mcs_nss, u8 tx_nss, u8 rx_nss, bool is_20only_map)
{
	u8 tx_mcs_0_7, tx_mcs_8_9, tx_mcs_10_11, tx_mcs_12_13;
	u8 rx_mcs_0_7, rx_mcs_8_9, rx_mcs_10_11, rx_mcs_12_13;
	u32 rx_map, tx_map;
	u32 eht_map;

	if (is_20only_map) {
		rx_mcs_0_7 = GET_RX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_0, rx_nss);
		rx_mcs_8_9 = GET_RX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_1, rx_nss);
		rx_mcs_10_11 = GET_RX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_2, rx_nss);
		rx_mcs_12_13 = GET_RX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_3, rx_nss);

		rx_map = u32_encode_bits(rx_mcs_0_7,
					 EHT_MCS_20_MHZ_ONLY_0_7_RX) |
			 u32_encode_bits(rx_mcs_8_9,
					 EHT_MCS_20_MHZ_ONLY_8_9_RX) |
			 u32_encode_bits(rx_mcs_10_11,
					 EHT_MCS_20_MHZ_ONLY_10_11_RX) |
			 u32_encode_bits(rx_mcs_12_13,
					 EHT_MCS_20_MHZ_ONLY_12_13_RX);

		tx_mcs_0_7 = GET_TX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_0, tx_nss);
		tx_mcs_8_9 = GET_TX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_1, tx_nss);

		tx_mcs_10_11 = GET_TX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_2, tx_nss);
		tx_mcs_12_13 = GET_TX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_3, tx_nss);
		tx_map = u32_encode_bits(tx_mcs_0_7,
					 EHT_MCS_20_MHZ_ONLY_0_7_TX) |
			 u32_encode_bits(tx_mcs_8_9,
					 EHT_MCS_20_MHZ_ONLY_8_9_TX) |
			 u32_encode_bits(tx_mcs_10_11,
					 EHT_MCS_20_MHZ_ONLY_10_11_TX) |
			 u32_encode_bits(tx_mcs_12_13,
					 EHT_MCS_20_MHZ_ONLY_12_13_TX);
		eht_map = rx_map | tx_map;
	} else {
		rx_mcs_0_7 = GET_RX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_0, rx_nss);
		rx_mcs_8_9 = rx_mcs_0_7;
		rx_mcs_10_11 = GET_RX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_1, rx_nss);
		rx_mcs_12_13 = GET_RX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_2, rx_nss);
		rx_map = u32_encode_bits(rx_mcs_8_9, EHT_MCS_NSS_0_9_RX) |
			 u32_encode_bits(rx_mcs_10_11, EHT_MCS_NSS_10_11_RX) |
			 u32_encode_bits(rx_mcs_12_13, EHT_MCS_NSS_12_13_RX);

		tx_mcs_0_7 = GET_TX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_0, tx_nss);
		tx_mcs_8_9 = tx_mcs_0_7;
		tx_mcs_10_11 = GET_TX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_1, tx_nss);
		tx_mcs_12_13 = GET_TX_MCS(eht_mcs_nss, EHT_MCS_NSS_IDX_2, tx_nss);
		tx_map = u32_encode_bits(tx_mcs_8_9, EHT_MCS_NSS_0_9_TX) |
			 u32_encode_bits(tx_mcs_10_11, EHT_MCS_NSS_10_11_TX) |
			 u32_encode_bits(tx_mcs_12_13, EHT_MCS_NSS_12_13_TX);
		eht_map = rx_map | tx_map;
	}

	return eht_map;
}

static void
ath12k_mac_copy_eht_mcs_nss(struct ath12k *ar, struct ath12k_band_cap *band_cap,
			    struct ieee80211_eht_mcs_nss_supp *mcs_nss,
			    const struct ieee80211_he_cap_elem *he_cap,
			    const struct ieee80211_eht_cap_elem_fixed *eht_cap)
{
	u8 maxtxnss_160 = ath12k_get_nss_160mhz(ar, ar->num_tx_chains);
	u8 maxtxnss_320 = ath12k_get_nss_320mhz(ar, ar->num_tx_chains);
	u8 maxrxnss_160 = ath12k_get_nss_160mhz(ar, ar->num_rx_chains);
	u8 maxrxnss_320 = ath12k_get_nss_320mhz(ar, ar->num_rx_chains);
	u32 eht_map;

	if ((he_cap->phy_cap_info[0] &
	     (IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G |
	      IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
	      IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
	      IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G)) == 0) {
		eht_map = intersect_eht_mcsnss_map_nss(band_cap->eht_mcs_20_only,
						       ar->num_tx_chains, ar->num_rx_chains,
						       true);
		memcpy(&mcs_nss->only_20mhz, &eht_map,
		       sizeof(struct ieee80211_eht_mcs_nss_supp_20mhz_only));
	}

	if (he_cap->phy_cap_info[0] &
	    (IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G |
	     IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G)) {
		eht_map = intersect_eht_mcsnss_map_nss(band_cap->eht_mcs_80,
						       ar->num_tx_chains, ar->num_rx_chains,
						       false);
		memcpy(&mcs_nss->bw._80, &eht_map,
		       sizeof(struct ieee80211_eht_mcs_nss_supp_bw));
	}

	if (he_cap->phy_cap_info[0] &
	    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G) {
		eht_map = intersect_eht_mcsnss_map_nss(band_cap->eht_mcs_160,
						       maxtxnss_160, maxrxnss_160, false);
		memcpy(&mcs_nss->bw._160, &eht_map,
		       sizeof(struct ieee80211_eht_mcs_nss_supp_bw));
	}

	if (eht_cap->phy_cap_info[0] & IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ) {
		eht_map = intersect_eht_mcsnss_map_nss(band_cap->eht_mcs_320,
						       maxtxnss_320, maxrxnss_320, false);
		memcpy(&mcs_nss->bw._320, &eht_map,
		       sizeof(struct ieee80211_eht_mcs_nss_supp_bw));
	}
}

static void ath12k_mac_copy_eht_ppe_thresh(struct ath12k_wmi_ppe_threshold_arg *fw_ppet,
					   struct ieee80211_sta_eht_cap *cap)
{
	u16 bit = IEEE80211_EHT_PPE_THRES_INFO_HEADER_SIZE;
	u8 i, nss, ru, ppet_bit_len_per_ru = IEEE80211_EHT_PPE_THRES_INFO_PPET_SIZE * 2;

	u8p_replace_bits(&cap->eht_ppe_thres[0], fw_ppet->numss_m1,
			 IEEE80211_EHT_PPE_THRES_NSS_MASK);

	u16p_replace_bits((u16 *)&cap->eht_ppe_thres[0], fw_ppet->ru_bit_mask,
			  IEEE80211_EHT_PPE_THRES_RU_INDEX_BITMASK_MASK);

	for (nss = 0; nss <= fw_ppet->numss_m1; nss++) {
		for (ru = 0;
		     ru < hweight16(IEEE80211_EHT_PPE_THRES_RU_INDEX_BITMASK_MASK);
		     ru++) {
			u32 val = 0;

			if ((fw_ppet->ru_bit_mask & BIT(ru)) == 0)
				continue;

			u32p_replace_bits(&val, fw_ppet->ppet16_ppet8_ru3_ru0[nss] >>
						(ru * ppet_bit_len_per_ru),
					  GENMASK(ppet_bit_len_per_ru - 1, 0));

			for (i = 0; i < ppet_bit_len_per_ru; i++) {
				cap->eht_ppe_thres[bit / 8] |=
					(((val >> i) & 0x1) << ((bit % 8)));
				bit++;
			}
		}
	}
}

static void
ath12k_mac_filter_eht_cap_mesh(struct ieee80211_eht_cap_elem_fixed
			       *eht_cap_elem)
{
	u8 m;

	m = IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS;
	eht_cap_elem->mac_cap_info[0] &= ~m;

	m = IEEE80211_EHT_MAC_CAP1_TWO_BQRS_SUPP |
	    IEEE80211_EHT_MAC_CAP1_EHT_LINK_ADAPTATION_SUPP;
	eht_cap_elem->mac_cap_info[1] &= ~m;

	m = IEEE80211_EHT_PHY_CAP0_PARTIAL_BW_UL_MU_MIMO;
	eht_cap_elem->phy_cap_info[0] &= ~m;

	m = IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
	    IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
	    IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
	    IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK;
	eht_cap_elem->phy_cap_info[3] &= ~m;

	m = IEEE80211_EHT_PHY_CAP4_PART_BW_DL_MU_MIMO |
	    IEEE80211_EHT_PHY_CAP4_PSR_SR_SUPP |
	    IEEE80211_EHT_PHY_CAP4_POWER_BOOST_FACT_SUPP |
	    IEEE80211_EHT_PHY_CAP4_EHT_MU_PPDU_4_EHT_LTF_08_GI;
	eht_cap_elem->phy_cap_info[4] &= ~m;

	m = IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
	    IEEE80211_EHT_PHY_CAP5_TX_LESS_242_TONE_RU_SUPP |
	    IEEE80211_EHT_PHY_CAP5_RX_LESS_242_TONE_RU_SUPP |
	    IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK;
	eht_cap_elem->phy_cap_info[5] &= ~m;

	m = IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK;
	eht_cap_elem->phy_cap_info[6] &= ~m;

	m = IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ |
	    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ |
	    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_320MHZ |
	    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ |
	    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ |
	    IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_320MHZ;
	eht_cap_elem->phy_cap_info[7] &= ~m;

	m = IEEE80211_EHT_PHY_CAP8_20MHZ_ONLY_CAPS |
	    IEEE80211_EHT_PHY_CAP8_20MHZ_ONLY_TRIGGER_MUBF_FL_BW_FB_DLMUMIMO |
	    IEEE80211_EHT_PHY_CAP8_20MHZ_ONLY_MRU_SUPP;
	eht_cap_elem->phy_cap_info[8] &= ~m;
}

static void ath12k_mac_copy_eht_cap(struct ath12k *ar,
				    struct ath12k_band_cap *band_cap,
				    struct ieee80211_he_cap_elem *he_cap_elem,
				    int iftype,
				    struct ieee80211_sta_eht_cap *eht_cap)
{
	struct ieee80211_eht_cap_elem_fixed *eht_cap_elem = &eht_cap->eht_cap_elem;

	memset(eht_cap, 0, sizeof(struct ieee80211_sta_eht_cap));

	if (!(test_bit(WMI_TLV_SERVICE_11BE, ar->ab->wmi_ab.svc_map)) ||
	    ath12k_acpi_get_disable_11be(ar->ab))
		return;

	eht_cap->has_eht = true;
	memcpy(eht_cap_elem->mac_cap_info, band_cap->eht_cap_mac_info,
	       sizeof(eht_cap_elem->mac_cap_info));
	memcpy(eht_cap_elem->phy_cap_info, band_cap->eht_cap_phy_info,
	       sizeof(eht_cap_elem->phy_cap_info));

	switch (iftype) {
	case NL80211_IFTYPE_AP:
		eht_cap_elem->phy_cap_info[0] &=
			~IEEE80211_EHT_PHY_CAP0_242_TONE_RU_GT20MHZ;
		eht_cap_elem->phy_cap_info[4] &=
			~IEEE80211_EHT_PHY_CAP4_PART_BW_DL_MU_MIMO;
		eht_cap_elem->phy_cap_info[5] &=
			~IEEE80211_EHT_PHY_CAP5_TX_LESS_242_TONE_RU_SUPP;
		break;
	case NL80211_IFTYPE_STATION:
		eht_cap_elem->phy_cap_info[7] &=
			~(IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ |
			  IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ |
			  IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_320MHZ);
		eht_cap_elem->phy_cap_info[7] &=
			~(IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ |
			  IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ |
			  IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_320MHZ);
		break;
	case NL80211_IFTYPE_MESH_POINT:
		ath12k_mac_filter_eht_cap_mesh(eht_cap_elem);
		break;
	default:
		break;
	}

	ath12k_mac_copy_eht_mcs_nss(ar, band_cap, &eht_cap->eht_mcs_nss_supp,
				    he_cap_elem, eht_cap_elem);

	if (eht_cap_elem->phy_cap_info[5] &
	    IEEE80211_EHT_PHY_CAP5_PPE_THRESHOLD_PRESENT)
		ath12k_mac_copy_eht_ppe_thresh(&band_cap->eht_ppet, eht_cap);
}

static int ath12k_mac_copy_sband_iftype_data(struct ath12k *ar,
					     struct ath12k_pdev_cap *cap,
					     struct ieee80211_sband_iftype_data *data,
					     int band)
{
	struct ath12k_band_cap *band_cap = &cap->band[band];
	int i, idx = 0;

	for (i = 0; i < NUM_NL80211_IFTYPES; i++) {
		struct ieee80211_sta_he_cap *he_cap = &data[idx].he_cap;

		switch (i) {
		case NL80211_IFTYPE_STATION:
		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_MESH_POINT:
			break;

		default:
			continue;
		}

		data[idx].types_mask = BIT(i);

		ath12k_mac_copy_he_cap(ar, band_cap, i, ar->num_tx_chains, he_cap);
		if (band == NL80211_BAND_6GHZ) {
			data[idx].he_6ghz_capa.capa =
				ath12k_mac_setup_he_6ghz_cap(cap, band_cap);
		}
		ath12k_mac_copy_eht_cap(ar, band_cap, &he_cap->he_cap_elem, i,
					&data[idx].eht_cap);
		idx++;
	}

	return idx;
}

static void ath12k_mac_setup_sband_iftype_data(struct ath12k *ar,
					       struct ath12k_pdev_cap *cap)
{
	struct ieee80211_supported_band *sband;
	enum nl80211_band band;
	int count;

	if (cap->supported_bands & WMI_HOST_WLAN_2GHZ_CAP) {
		band = NL80211_BAND_2GHZ;
		count = ath12k_mac_copy_sband_iftype_data(ar, cap,
							  ar->mac.iftype[band],
							  band);
		sband = &ar->mac.sbands[band];
		_ieee80211_set_sband_iftype_data(sband, ar->mac.iftype[band],
						 count);
	}

	if (cap->supported_bands & WMI_HOST_WLAN_5GHZ_CAP) {
		band = NL80211_BAND_5GHZ;
		count = ath12k_mac_copy_sband_iftype_data(ar, cap,
							  ar->mac.iftype[band],
							  band);
		sband = &ar->mac.sbands[band];
		_ieee80211_set_sband_iftype_data(sband, ar->mac.iftype[band],
						 count);
	}

	if (cap->supported_bands & WMI_HOST_WLAN_5GHZ_CAP &&
	    ar->supports_6ghz) {
		band = NL80211_BAND_6GHZ;
		count = ath12k_mac_copy_sband_iftype_data(ar, cap,
							  ar->mac.iftype[band],
							  band);
		sband = &ar->mac.sbands[band];
		_ieee80211_set_sband_iftype_data(sband, ar->mac.iftype[band],
						 count);
	}
}

int ath12k_mac_set_tx_antenna(struct ath12k *ar, u32 tx_ant)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* Since we advertised the max cap of all radios combined during wiphy
	 * registration, ensure we dont set the antenna config higher than our
	 * limits
	 */

	tx_ant = min_t(u32, tx_ant, ar->pdev->cap.tx_chain_mask);

	ar->cfg_tx_chainmask = tx_ant;

	ar->num_tx_chains = hweight32(tx_ant);

	/* Reload HT/VHT/HE capability */
	ath12k_mac_setup_ht_vht_cap(ar, &ar->pdev->cap, NULL);
	ath12k_mac_setup_sband_iftype_data(ar, &ar->pdev->cap);

	if (ar->ah->state != ATH12K_HW_STATE_ON &&
	    ar->ah->state != ATH12K_HW_STATE_RESTARTED)
		return 0;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TX_CHAIN_MASK,
					tx_ant, ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab, "failed to set tx-chainmask: %d, req 0x%x\n",
			   ret, tx_ant);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "ar->cfg_tx_chainmask: %d (%x) ar->cfg_rx_chainmask: %d (%x)\n",
		   ar->cfg_tx_chainmask, ar->num_tx_chains,
		   ar->cfg_rx_chainmask, ar->num_rx_chains);

	return 0;
}

int ath12k_mac_set_rx_antenna(struct ath12k *ar, u32 rx_ant)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* Since we advertised the max cap of all radios combined during wiphy
	 * registration, ensure we dont set the antenna config higher than our
	 * limits
	 */
	rx_ant = min_t(u32, rx_ant, ar->pdev->cap.rx_chain_mask);

	ar->cfg_rx_chainmask = rx_ant;

	ar->num_rx_chains = hweight32(rx_ant);

	/* Reload HT/VHT/HE capability */
	ath12k_mac_setup_ht_vht_cap(ar, &ar->pdev->cap, NULL);
	ath12k_mac_setup_sband_iftype_data(ar, &ar->pdev->cap);

	if (ar->ah->state != ATH12K_HW_STATE_ON &&
	    ar->ah->state != ATH12K_HW_STATE_RESTARTED)
		return 0;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_RX_CHAIN_MASK,
					rx_ant, ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab, "failed to set rx-chainmask: %d, req 0x%x\n",
				ret, rx_ant);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "ar->cfg_tx_chainmask: %d (%x) ar->cfg_rx_chainmask: %d (%x)\n",
		   ar->cfg_tx_chainmask, ar->num_tx_chains,
		   ar->cfg_rx_chainmask, ar->num_rx_chains);

	return 0;
}

static int __ath12k_set_antenna(struct ath12k *ar, u32 tx_ant, u32 rx_ant,
				bool is_dynamic)
{
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_check_chain_mask(ar, tx_ant, true))
		return -EINVAL;

	if (ath12k_check_chain_mask(ar, rx_ant, false))
		return -EINVAL;

	/* Since we advertised the max cap of all radios combined during wiphy
	 * registration, ensure we don't set the antenna config higher than the
	 * limits
	 */
	tx_ant = min_t(u32, tx_ant, ar->pdev->cap.tx_chain_mask);
	rx_ant = min_t(u32, rx_ant, ar->pdev->cap.rx_chain_mask);

	ar->cfg_tx_chainmask = tx_ant;
	ar->cfg_rx_chainmask = rx_ant;

	ar->num_tx_chains = hweight32(tx_ant);
	ar->num_rx_chains = hweight32(rx_ant);

	/* Reload HT/VHT/HE capability */
	ath12k_mac_setup_ht_vht_cap(ar, &ar->pdev->cap, NULL);
	ath12k_mac_setup_sband_iftype_data(ar, &ar->pdev->cap);

	if (ah->state != ATH12K_HW_STATE_ON &&
	    ah->state != ATH12K_HW_STATE_RESTARTED && !is_dynamic)
		return 0;

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_TX_CHAIN_MASK,
					tx_ant, ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab, "failed to set tx-chainmask: %d, req 0x%x\n",
			    ret, tx_ant);
		return ret;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_RX_CHAIN_MASK,
					rx_ant, ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab, "failed to set rx-chainmask: %d, req 0x%x\n",
			    ret, rx_ant);
		return ret;
	}

	return 0;
}

static void ath12k_mgmt_over_wmi_tx_drop(struct ath12k *ar, struct sk_buff *skb)
{
	int num_mgmt = 0;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!(info->flags & IEEE80211_TX_CTL_TX_OFFCHAN))
		num_mgmt = atomic_dec_if_positive(&ar->num_pending_mgmt_tx);

	ieee80211_free_txskb(ar->ah->hw, skb);

	if (num_mgmt < 0)
		WARN_ON_ONCE(1);

	if (!num_mgmt)
		wake_up(&ar->txmgmt_empty_waitq);
}

static void ath12k_mac_tx_mgmt_free(struct ath12k *ar, int buf_id)
{
	struct sk_buff *msdu;
	struct ieee80211_tx_info *info;

	spin_lock_bh(&ar->txmgmt_idr_lock);
	msdu = idr_remove(&ar->txmgmt_idr, buf_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	if (!msdu)
		return;

	ath12k_core_dma_unmap_single(ar->ab->dev, ATH12K_SKB_CB(msdu)->paddr, msdu->len,
				     DMA_TO_DEVICE);

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	ath12k_mgmt_over_wmi_tx_drop(ar, msdu);
}

int ath12k_mac_tx_mgmt_pending_free(int buf_id, void *skb, void *ctx)
{
	struct ath12k *ar = ctx;

	ath12k_mac_tx_mgmt_free(ar, buf_id);

	return 0;
}

static int ath12k_mac_vif_txmgmt_idr_remove(int buf_id, void *skb, void *ctx)
{
	struct ieee80211_vif *vif = ctx;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k *ar = skb_cb->u.ar;

	if (skb_cb->vif == vif)
		ath12k_mac_tx_mgmt_free(ar, buf_id);

	return 0;
}

static int ath12k_mac_mgmt_tx_wmi(struct ath12k *ar, struct ath12k_link_vif *arvif,
				  struct sk_buff *skb)
{
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	bool is_mlme = ATH12K_MGMT_MLME_FRAME(hdr->frame_control);
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_mgmt_frame_stats *stats;
	enum hal_encrypt_type enctype;
	bool tx_params_valid = false;
	unsigned int mic_len;
	bool link_agnostic;
	dma_addr_t paddr;
	u8 frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
	int buf_id;
	int ret;
	u8 sta_addr[ETH_ALEN];

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	skb_cb->u.ar = ar;
	spin_lock_bh(&ar->txmgmt_idr_lock);
	buf_id = idr_alloc(&ar->txmgmt_idr, skb, 0,
			   ATH12K_TX_MGMT_NUM_PENDING_MAX, GFP_ATOMIC);
	spin_unlock_bh(&ar->txmgmt_idr_lock);
	if (buf_id < 0)
		return -ENOSPC;

	if (!(skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP)) {
		if ((ieee80211_is_action(hdr->frame_control) ||
		     ieee80211_is_deauth(hdr->frame_control) ||
		     ieee80211_is_disassoc(hdr->frame_control)) &&
		     ieee80211_has_protected(hdr->frame_control)) {
			if (!(skb_cb->flags & ATH12K_SKB_CIPHER_SET))
				ath12k_warn(ab, "WMI protected management tx frame without ATH12K_SKB_CIPHER_SET");

			enctype = ath12k_dp_tx_get_encrypt_type(skb_cb->cipher);
			mic_len = ath12k_dp_rx_crypto_mic_len(&ar->dp, enctype);
			skb_put(skb, mic_len);
		}
	}
#ifndef CONFIG_IO_COHERENCY
	paddr = dma_map_single(ab->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(ab->dev, paddr)) {
		ath12k_warn(ab, "failed to DMA map mgmt Tx buffer\n");
		ret = -EIO;
		goto err_free_idr;
	}
#else
	paddr = virt_to_phys(skb->data);
	if (!paddr) {
		ath12k_warn(ab, "failed to DMA map mgmt Tx buffer\n");
		ret = -EIO;
		goto err_free_idr;
	}
#endif
	skb_cb->paddr = paddr;

	link_agnostic = ATH12K_SKB_CB(skb)->flags & ATH12K_SKB_MGMT_LINK_AGNOSTIC;

#ifdef CPTCFG_ATH12K_CFR
	if (ar->cfr.cfr_enabled && ieee80211_is_probe_resp(hdr->frame_control) &&
	    peer_is_in_cfr_unassoc_pool(ar, hdr->addr1))
		tx_params_valid = true;
#endif /* CPTCFG_ATH12K_CFR */

	spin_lock_bh(&ar->data_lock);
	stats = &arvif->ahvif->mgmt_stats;
	stats->aggr_tx_mgmt_cnt++;
	spin_unlock_bh(&ar->data_lock);

	ether_addr_copy(sta_addr, hdr->addr1);

	if (info->flags & IEEE80211_TX_CTL_TX_OFFCHAN)
		ret = ath12k_wmi_offchan_mgmt_send(ar, arvif->vdev_id, buf_id, skb);
	else
		ret = ath12k_wmi_mgmt_send(ar, arvif->vdev_id, buf_id, skb,
					   link_agnostic, tx_params_valid);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send mgmt frame: %d\n", ret);
		goto err_unmap_buf;
	}

	if (is_mlme)
		ath12k_dbg(ab, ATH12K_DBG_MLME, "Transmit %s to STA %pM over WMI\n",
			   mgmt_frame_name[frm_stype], sta_addr);
	return 0;

err_unmap_buf:
	ath12k_core_dma_unmap_single(ab->dev, skb_cb->paddr,
				     skb->len, DMA_TO_DEVICE);
err_free_idr:
	spin_lock_bh(&ar->txmgmt_idr_lock);
	idr_remove(&ar->txmgmt_idr, buf_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	return ret;
}

static void ath12k_mgmt_over_wmi_tx_purge(struct ath12k *ar)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&ar->wmi_mgmt_tx_queue)) != NULL)
		ath12k_mgmt_over_wmi_tx_drop(ar, skb);
}

static int ath12k_mac_mgmt_action_frame_fill_elem(struct ath12k_link_vif *arvif,
						  struct sk_buff *skb)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ieee80211_mgmt *mgmt;
	struct ieee80211_bss_conf *link_conf;
	struct ath12k_fw_stats_req_params req_param;
	struct ath12k_fw_stats_pdev *pdev;
	int ret, cur_tx_power, max_tx_power;
	bool has_protected;
	u8 category, *buf, iv_len;
	u8 action_code, dialog_token;

	/* make sure category field is present */
	if (skb->len < IEEE80211_MIN_ACTION_SIZE) {
		skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
		return -EINVAL;
	}

	has_protected = ieee80211_has_protected(hdr->frame_control);

	/* SW_CRYPTO and hdr protected case (PMF), packet will be encrypted,
	 * we can't put in data in this case
	 */
	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ar->ab->ag->flags) &&
	    has_protected) {
	    	skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
		return -EOPNOTSUPP;
	}


	mgmt = (struct ieee80211_mgmt *)hdr;
	buf = (u8 *)&mgmt->u.action;

	/* FCTL_PROTECTED frame might have extra space added for HDR_LEN. Offset that
	 * many bytes if it is there
	 */
	if (has_protected) {
		switch (skb_cb->cipher) {
		/* Currently only for CCMP cipher suite, we asked for it via
		 * setting %IEEE80211_KEY_FLAG_GENERATE_IV_MGMT in key. Check
		 * ath12k_install_key()
		 */
		case WLAN_CIPHER_SUITE_CCMP:
			iv_len = IEEE80211_CCMP_HDR_LEN;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
		case WLAN_CIPHER_SUITE_CCMP_256:
		case WLAN_CIPHER_SUITE_GCMP:
		case WLAN_CIPHER_SUITE_GCMP_256:
		case WLAN_CIPHER_SUITE_AES_CMAC:
		case WLAN_CIPHER_SUITE_BIP_GMAC_128:
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
		case WLAN_CIPHER_SUITE_BIP_CMAC_256:
			iv_len = 0;
			break;
		default:
			skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
			return -EINVAL;
		}

		buf = buf + iv_len;
	}

	category = *buf++;

	switch (category) {
	case WLAN_CATEGORY_RADIO_MEASUREMENT:
		/* Packet Format:
		 *      Action Code | Dialog Token | Variable Len (based on Action Code)
		 */
		action_code = *buf++;
		dialog_token = *buf++;

		rcu_read_lock();

		link_conf = ath12k_mac_get_link_bss_conf(arvif);

		if (!link_conf) {
			rcu_read_unlock();
			ath12k_warn(ar->ab, "unable to access bss link conf\n");
			return -EINVAL;
		}

		cur_tx_power = link_conf->txpower;
		max_tx_power = min(link_conf->chanctx_conf->def.chan->max_reg_power,
				   (int)ar->max_tx_power / 2);

		rcu_read_unlock();

		/* fetch current tx power from FW pdev stats */
		req_param.pdev_id = ar->pdev->pdev_id;
		req_param.vdev_id = 0;
		req_param.stats_id = WMI_REQUEST_PDEV_STAT;

		ret = ath12k_mac_get_fw_stats(ar, &req_param);
		if (ret) {
			ath12k_warn(ar->ab, "failed to request fw pdev stats: %d\n", ret);
			goto check_rm_action_frame;
		}

		spin_lock_bh(&ar->data_lock);
		pdev = list_first_entry_or_null(&ar->fw_stats.pdevs,
						struct ath12k_fw_stats_pdev,
						list);
		if (!pdev) {
			spin_unlock_bh(&ar->data_lock);
			goto check_rm_action_frame;
		}

		/* Tx power is set as 2 units per dBm in FW. */
		cur_tx_power = pdev->chan_tx_power / 2;
		spin_unlock_bh(&ar->data_lock);

check_rm_action_frame:
		switch (action_code) {
		case WLAN_ACTION_RADIO_MSR_LINK_MSR_REQ:
			/* Variable Len Format:
			 *      Transmit Power | Max Tx Power
			 * We fill both of these.
			 */
			*buf++ = cur_tx_power;
			*buf = max_tx_power;

			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
					"RRM: Link Measurement Req dialog_token=%u, cur_tx_power=%d, max_tx_power=%d\n",
					dialog_token, cur_tx_power, max_tx_power);
			skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
			break;
		case WLAN_ACTION_RADIO_MSR_LINK_MSR_REP:
			/* Variable Len Format:
			 *      TPC Report | Variable Fields
			 *
			 * TPC Report Format:
			 *      Element ID | Len | Tx Power | Link Margin
			 *
			 * We fill Tx power in the TPC Report (2nd index)
			 */
			buf[2] = cur_tx_power;

			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
					"RRM: Link Measurement Resp dialog_token=%u, cur_tx_power=%d\n",
					dialog_token, cur_tx_power);
			skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
			break;
		default:
			return -EINVAL;
		}
		break;
	case WLAN_CATEGORY_PROTECTED_EHT:
		/* Set the link agnostic bit for
		 * protected eht action frames
		 */
		skb_cb->flags |= ATH12K_SKB_MGMT_LINK_AGNOSTIC;
		break;
	case WLAN_CATEGORY_WNM:
		action_code = *buf++;

		switch (action_code) {
		case WLAN_WNM_ACTION_BSS_TM_REQ:
		case WLAN_WNM_ACTION_BSS_TM_RESP:
			/* Set the link agnostic bit for
			 * BTM request and response as per
			 * IEEE Std 802.11be Draft 7.0,
			 * section 35.3.14
			 *
			 * Intentionally leaving the switch case empty
			 * to avoid resetting the link agnostic bit in
			 * default case.
			 */
			break;
		default:
			skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
		}
		break;
	case WLAN_CATEGORY_BACK:
		action_code = *buf++;
		switch (action_code) {
		case WLAN_ACTION_ADDBA_REQ:
		case WLAN_ACTION_ADDBA_RESP:
			break;
		default:
			skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
		}
		break;
	default:
		/* nothing to fill */
		skb_cb->flags &= ~ATH12K_SKB_MGMT_LINK_AGNOSTIC;
		return 0;
	}

	return 0;
}

static int ath12k_mac_mgmt_frame_fill_elem(struct ath12k_link_vif *arvif,
                                          struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

	if (!ieee80211_is_action(hdr->frame_control))
		return 0;

	return ath12k_mac_mgmt_action_frame_fill_elem(arvif, skb);
}

static void ath12k_mgmt_over_wmi_tx_work(struct wiphy *wiphy, struct wiphy_work *work)
{
	struct ath12k *ar = container_of(work, struct ath12k, wmi_mgmt_tx_work);
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_skb_cb *skb_cb;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct sk_buff *skb;
	int ret;

	lockdep_assert_wiphy(wiphy);

	while ((skb = skb_dequeue(&ar->wmi_mgmt_tx_queue)) != NULL) {
		skb_cb = ATH12K_SKB_CB(skb);
		if (!skb_cb->vif) {
			ath12k_warn(ar->ab, "no vif found for mgmt frame\n");
			ath12k_mgmt_over_wmi_tx_drop(ar, skb);
			continue;
		}

		ahvif = ath12k_vif_to_ahvif(skb_cb->vif);
		if (!(ahvif->links_map & BIT(skb_cb->link_id))) {
			ath12k_warn(ar->ab,
				    "invalid linkid %u in mgmt over wmi tx with linkmap 0x%x\n",
				    skb_cb->link_id, ahvif->links_map);
			ath12k_mgmt_over_wmi_tx_drop(ar, skb);
			continue;
		}

		arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[skb_cb->link_id]);
		if (ar->allocated_vdev_map & (1LL << arvif->vdev_id)) {
			/* Fill the data which is required to be filled in by the driver
			 * Example: Max Tx power in Link Measurement Request/Report
			 */
			ret = ath12k_mac_mgmt_frame_fill_elem(arvif, skb);
			if (ret) {
				/* If we couldn't fill the data due to any reason, let's not discard
				 * transmitting the packet.
				 * For ex: SW crypto and PMF case
				 */
				ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
					   "Cant't fill in the required data for the mgmt packet. err=%d\n",
					   ret);
			}

			ret = ath12k_mac_mgmt_tx_wmi(ar, arvif, skb);
			if (ret) {
				ath12k_warn(ar->ab, "failed to tx mgmt frame, vdev_id %d :%d\n",
					    arvif->vdev_id, ret);
				ath12k_mgmt_over_wmi_tx_drop(ar, skb);
			}
		} else {
			ath12k_warn(ar->ab,
				    "dropping mgmt frame for vdev %d link %u is_started %d\n",
				    arvif->vdev_id,
				    skb_cb->link_id,
				    arvif->is_started);
			ath12k_mgmt_over_wmi_tx_drop(ar, skb);
		}
	}
}

int ath12k_mac_mgmt_tx(struct ath12k *ar, struct sk_buff *skb,
		       bool is_prb_rsp)
{
	struct sk_buff_head *q = &ar->wmi_mgmt_tx_queue;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags))
		return -ESHUTDOWN;

	/* Drop probe response packets when the pending management tx
	 * count has reached a certain threshold, so as to prioritize
	 * other mgmt packets like auth and assoc to be sent on time
	 * for establishing successful connections.
	 */
	if (is_prb_rsp &&
	    atomic_read(&ar->num_pending_mgmt_tx) > ATH12K_PRB_RSP_DROP_THRESHOLD) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			    "dropping probe response as pending queue is almost full\n");
		return -EBUSY;
	}

	if (skb_queue_len_lockless(q) >= ATH12K_TX_MGMT_NUM_PENDING_MAX) {
		ath12k_warn(ar->ab, "mgmt tx queue is full\n");
		return -ENOSPC;
	}

	skb_queue_tail(q, skb);
	/* For some of the off chan frames in DPP, host will not receive tx status,
	 * due to that skipping incrementing pending frames for off channel frames
	 * only to avoid the leak
	 */
	if (!(info->flags & IEEE80211_TX_CTL_TX_OFFCHAN))
		atomic_inc(&ar->num_pending_mgmt_tx);

	wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &ar->wmi_mgmt_tx_work);

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_mgmt_tx);

void ath12k_mac_add_p2p_noa_ie(struct ath12k *ar,
			       struct ieee80211_vif *vif,
			       struct sk_buff *skb,
			       bool is_prb_rsp)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);

	if (likely(!is_prb_rsp))
		return;

	spin_lock_bh(&ar->data_lock);

	if (ahvif->u.ap.noa_data &&
	    !pskb_expand_head(skb, 0, ahvif->u.ap.noa_len,
			      GFP_ATOMIC))
		skb_put_data(skb, ahvif->u.ap.noa_data,
			     ahvif->u.ap.noa_len);

	spin_unlock_bh(&ar->data_lock);
}
EXPORT_SYMBOL(ath12k_mac_add_p2p_noa_ie);

/* Note: called under rcu_read_lock() */
void ath12k_mlo_mcast_update_tx_link_address(struct ieee80211_vif *vif,
					     u8 link_id, struct sk_buff *skb,
					     u32 info_flags)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ieee80211_bss_conf *bss_conf;

	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
		return;

	bss_conf = rcu_dereference(vif->link_conf[link_id]);
	if (bss_conf)
		ether_addr_copy(hdr->addr2, bss_conf->addr);
}
EXPORT_SYMBOL(ath12k_mlo_mcast_update_tx_link_address);

/* This function should be called only for a mgmt frame to a ML STA,
 * hence, such sanity checks are skipped
 */
static bool ath12k_mac_is_mgmt_link_agnostic(struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt;
	mgmt = (struct ieee80211_mgmt *)skb->data;

	if (ieee80211_is_action(mgmt->frame_control))
		return true;

	/* TODO Extend as per requirement */
	return false;
}

/* Note: called under rcu_read_lock() */
u8 ath12k_mac_get_tx_link(struct ieee80211_sta *sta, struct ieee80211_vif *vif,
			  u8 link, struct sk_buff *skb, u32 info_flags)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ieee80211_link_sta *link_sta;
	struct ieee80211_bss_conf *bss_conf;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	u8 link_id;
	unsigned long links;
	struct ath12k_base *ab = NULL;

	/* Use the link id passed or the first available link */
	if (!sta) {
		if (link != IEEE80211_LINK_UNSPECIFIED)
			return link;

		link_id = ffs(ahvif->links_map) - 1;
		return link_id;
	}

	ahsta = ath12k_sta_to_ahsta(sta);

	/* Below translation ensures we pass proper A2 & A3 for non ML clients.
	 * Also it assumes for now support only for MLO AP in this path
	 */
	if (!sta->mlo) {
		link = ahsta->deflink.link_id;

		if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
			return link;

		bss_conf = rcu_dereference(vif->link_conf[link]);
		if (bss_conf) {
			ether_addr_copy(hdr->addr2, bss_conf->addr);
			if (!ieee80211_has_tods(hdr->frame_control) &&
			    !ieee80211_has_fromds(hdr->frame_control))
				ether_addr_copy(hdr->addr3, bss_conf->addr);
		}

		return link;
	}

	/* enqueue eth enacap & data frames on primary link, FW does link
	 * selection and address translation.
	 */
	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP ||
	    ieee80211_is_data(hdr->frame_control))
		return ahsta->primary_link_id;

	/* Check if this mgmt frame can be queued at MLD level, in that
	 * case the FW can decide on which link it needs to be finally
	 * transmitted based on the power state of that link.
	 * The link param returned by this function still needs
	 * to be valid to get queued to one of the valid link FW
	 */
	if (ath12k_mac_is_mgmt_link_agnostic(skb)) {
		ATH12K_SKB_CB(skb)->flags |= ATH12K_SKB_MGMT_LINK_AGNOSTIC;
		/* For action frames this will be reset if not needed
		 * later based on action category.
		 */
	}

	/* 802.11 frame cases */
	if (link == IEEE80211_LINK_UNSPECIFIED)
		link = ahsta->deflink.link_id;

	if (!ieee80211_is_mgmt(hdr->frame_control))
		return link;

	if (ahsta->deflink.arvif->ar)
		ab = ahsta->deflink.arvif->ar->ab;

	if (ab && test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags) &&
	    ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2) {
		/* If disassoc frame comes in crash link, need to
		 * change the link which is active at that instance.
		 */
		if (ieee80211_is_disassoc(hdr->frame_control)) {
			link = ahsta->deflink.link_id;
			links = ahsta->links_map;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta = rcu_dereference(ahsta->link[link_id]);
				if (!arsta)
					continue;
				ab = arsta->arvif->ar->ab;
				if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags)) {
					link = arsta->link_id;
					ATH12K_SKB_CB(skb)->flags |= ATH12K_SKB_MGMT_LINK_AGNOSTIC;
					break;
				}
			}
		}
	}

	/* Perform address conversion for ML STA Tx */
	bss_conf = rcu_dereference(vif->link_conf[link]);
	link_sta = rcu_dereference(sta->link[link]);

	if (bss_conf && link_sta) {
		if (!(vif->type == NL80211_IFTYPE_AP &&
		    (ieee80211_is_probe_resp(hdr->frame_control) ||
		     ieee80211_is_auth(hdr->frame_control) ||
		     ieee80211_is_reassoc_resp(hdr->frame_control) ||
		     ieee80211_is_assoc_resp(hdr->frame_control))))
			ether_addr_copy(hdr->addr1, link_sta->addr);

		ether_addr_copy(hdr->addr2, bss_conf->addr);
		if (vif->type == NL80211_IFTYPE_STATION && bss_conf->bssid)
			ether_addr_copy(hdr->addr3, bss_conf->bssid);
		else if (vif->type == NL80211_IFTYPE_AP)
			ether_addr_copy(hdr->addr3, bss_conf->addr);

		return link;
	}

	if (bss_conf) {
		/* In certain cases where a ML sta associated and added subset of
		 * links on which the ML AP is active, but now sends some frame
		 * (ex. Probe request) on a different link which is active in our
		 * MLD but was not added during previous association, we can
		 * still honor the Tx to that ML STA via the requested link.
		 * The control would reach here in such case only when that link
		 * address is same as the MLD address or in worst case clients
		 * used MLD address at TA wrongly which would have helped
		 * identify the ML sta object and pass it here.
		 * If the link address of that STA is different from MLD address,
		 * then the sta object would be NULL and control won't reach
		 * here but return at the start of the function itself with !sta
		 * check. Also this would not need any translation at hdr->addr1
		 * from MLD to link address since the RA is the MLD address
		 * (same as that link address ideally) already.
		 */
		ether_addr_copy(hdr->addr2, bss_conf->addr);

		if (vif->type == NL80211_IFTYPE_STATION && bss_conf->bssid)
			ether_addr_copy(hdr->addr3, bss_conf->bssid);
		else if (vif->type == NL80211_IFTYPE_AP)
			ether_addr_copy(hdr->addr3, bss_conf->addr);
	}


	return link;
}
EXPORT_SYMBOL(ath12k_mac_get_tx_link);

void ath12k_mac_drain_tx(struct ath12k *ar)
{
	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* make sure rcu-protected mac80211 tx path itself is drained */
	synchronize_net();

	wiphy_work_cancel(ath12k_ar_to_hw(ar)->wiphy, &ar->wmi_mgmt_tx_work);
	ath12k_mgmt_over_wmi_tx_purge(ar);
}

int ath12k_mac_start(struct ath12k *ar)
{
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_pdev *pdev = ar->pdev;
	int ret;
	enum dp_mon_stats_mode mode = ATH12k_DP_MON_BASIC_STATS;
	u8 dcs_enable_bitmap;

	lockdep_assert_held(&ah->hw_mutex);
	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_check_erp_power_down(ab->ag) &&
	    !ath12k_hw_group_recovery_in_progress(ab->ag) &&
	    ar->pdev_suspend && !ab->powerup_triggered) {
		ret = ath12k_mac_pdev_resume(ar);
		if (ret) {
			ath12k_err(ab, "pdev resume command is failed: %d\n", ret);
			goto err;
		}
	}

	ath12k_info(ab, "Enabling FW Thermal throttling\n");
	ret = ath12k_thermal_set_throttling(ar, ATH12K_THERMAL_LVL0_DUTY_CYCLE);
	if (ret) {
		ath12k_err(ab, "failed to set thermal throttle: (%d)\n", ret);
		goto err;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_PMF_QOS,
					1, pdev->pdev_id);

	if (ret) {
		ath12k_err(ab, "failed to enable PMF QOS: (%d)\n", ret);
		goto err;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DYNAMIC_BW, 1,
					pdev->pdev_id);
	if (ret) {
		ath12k_err(ab, "failed to enable dynamic bw: %d\n", ret);
		goto err;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ARP_AC_OVERRIDE,
					0, pdev->pdev_id);
	if (ret) {
		ath12k_err(ab, "failed to set ac override for ARP: %d\n",
			   ret);
		goto err;
	}

	ret = ath12k_wmi_send_dfs_phyerr_offload_enable_cmd(ar, pdev->pdev_id);
	if (ret) {
		ath12k_err(ab, "failed to offload radar detection: %d\n",
			   ret);
		goto err;
	}

	ret = ath12k_dp_tx_htt_h2t_ppdu_stats_req(ar,
						  HTT_PPDU_STATS_TAG_DEFAULT);
	if (ret) {
		ath12k_err(ab, "failed to req ppdu stats: %d\n", ret);
		goto err;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_MESH_MCAST_ENABLE,
					1, pdev->pdev_id);

	if (ret) {
		ath12k_err(ab, "failed to enable MESH MCAST ENABLE: (%d\n", ret);
		goto err;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_CONG_CTRL_MAX_MSDUS,
					ATH12K_NUM_POOL_TX_DESC, pdev->pdev_id);
	if (ret) {
		ath12k_err(ab, "failed to set congestion control MAX MSDUS: %d\n", ret);
		goto err;
	}

	if (ath12k_mlo_3_link_tx) {
		ret = ath12k_wmi_pdev_set_param(ar,
						WMI_PDEV_PARAM_TID_MAPPING_3LINK_MLO,
						ath12k_mlo_3_link_tx, pdev->pdev_id);
		if (ret) {
			ath12k_err(ab,
				   "Failed to enable 3-link tid mapping"
				   "for pdev id:%d\n", pdev->pdev_id);
		}
	}

	ret = ath12k_dp_rx_pkt_type_filter(ar, ATH12K_PKT_TYPE_EAP,
					   ATH12K_ROUTE_EAP_METADATA);
	if (ret) {
		ath12k_err(ar->ab, "failed to configure EAP pkt route: %d\n", ret);
		goto err;
	}

	/* Enable(1)/Disable(0) sub channel marking */
	if (pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP) {
		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SUB_CHANNEL_MARKING,
						1, pdev->pdev_id);
		if (ret) {
			ath12k_err(ab, "failed to enable SUB CHANNEL MARKING: %d\n", ret);
			goto err;
		}
	}

	__ath12k_set_antenna(ar, ar->cfg_tx_chainmask, ar->cfg_rx_chainmask, false);

	/* TODO: Do we need to enable ANI? */

	ret = ath12k_reg_update_chan_list(ar, false);
	/* the ar state alone can be turned off for non supported country
	 * without returning the error value. As we need to update the channel
	 * for the next ar
	 */
	if (ret) {
		if (ret == -EOPNOTSUPP)
			ret = 0;
		goto err;
	}

	ar->he_dl_enabled = 1;
	ar->he_ul_enabled = 1;
	ar->he_dlbf_enabled = 1;

	ar->eht_dl_enabled = 1;
	ar->eht_ul_enabled = 1;
	ar->eht_dlbf_enabled = 1;

	ar->num_started_vdevs = 0;
	ar->num_created_vdevs = 0;
	ar->num_created_bridge_vdevs = 0;
	ar->num_peers = 0;
	ar->allocated_vdev_map = 0;
	ar->chan_tx_pwr = ATH12K_PDEV_TX_POWER_INVALID;

	spin_lock_bh(&ar->data_lock);
        ar->awgn_intf_handling_in_prog = false;
        spin_unlock_bh(&ar->data_lock);

	/* Configure monitor status ring with default rx_filter to get rx status
	 * such as rssi, rx_duration.
	 */
	ath12k_dp_mon_rx_stats_config(ar, true, mode);
	ret = ath12k_dp_mon_rx_update_filter(ar);
	if (ret && (ret != -EOPNOTSUPP)) {
		ath12k_err(ab, "failed to configure monitor status ring with default rx_filter: (%d)\n",
			   ret);
		goto err;
	}

	if (ret == -EOPNOTSUPP)
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "monitor status config is not yet supported");

	/* Configure the hash seed for hash based reo dest ring selection */
	ath12k_wmi_pdev_lro_cfg(ar, ar->pdev->pdev_id);

	/* allow device to enter IMPS */
	if (ab->hw_params->idle_ps) {
		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_IDLE_PS_CONFIG,
						1, pdev->pdev_id);
		if (ret) {
			ath12k_err(ab, "failed to enable idle ps: %d\n", ret);
			goto err;
		}
	}

	spin_lock_bh(&ar->data_lock);
	if (ar->dcs_enable_bitmap) {
		dcs_enable_bitmap = ar->dcs_enable_bitmap;
		spin_unlock_bh(&ar->data_lock);
		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DCS,
						dcs_enable_bitmap, ar->pdev->pdev_id);
		if (ret) {
			ath12k_err(ab, "failed to enable Interference Detect:%d\n", ret);
			goto err;
		}
	} else {
		spin_unlock_bh(&ar->data_lock);
	}

	rcu_assign_pointer(ab->pdevs_active[ar->pdev_idx],
			   &ab->pdevs[ar->pdev_idx]);

	return 0;
err:

	return ret;
}

static void ath12k_drain_tx(struct ath12k_hw *ah)
{
	struct ath12k *ar;
	int i;

	lockdep_assert_wiphy(ah->hw->wiphy);

	for_each_ar(ah, ar, i) {
		if (ar->ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0 || ar->ab->is_reset)
			ath12k_mac_drain_tx(ar);
	}
}

int ath12k_mac_op_start(struct ieee80211_hw *hw)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;
	struct ath12k_hw_group *ag = ath12k_ah_to_ag(ah);
	int ret, i;

	if (ath12k_ftm_mode)
		return -EPERM;

	lockdep_assert_wiphy(hw->wiphy);

	if (ath12k_check_erp_power_down(ag) &&
	    !ath12k_hw_group_recovery_in_progress(ag)) {
		ret = ath12k_core_power_up(ag);
		return ret;
	}

	ath12k_drain_tx(ah);

	guard(mutex)(&ah->hw_mutex);

	switch (ah->state) {
	case ATH12K_HW_STATE_OFF:
		ah->state = ATH12K_HW_STATE_ON;
		break;
	case ATH12K_HW_STATE_RESTARTING:
		ah->state = ATH12K_HW_STATE_RESTARTED;
		break;
	case ATH12K_HW_STATE_RESTARTED:
	case ATH12K_HW_STATE_WEDGED:
	case ATH12K_HW_STATE_ON:
	case ATH12K_HW_STATE_TM:
		ah->state = ATH12K_HW_STATE_OFF;

		WARN_ON(1);
		return -EINVAL;
	}

	for_each_ar(ah, ar, i) {
		/* If the recovery mode is already advertised as Mode-1 this means mac op start
		 * is already done and do mac_start for only asserted ab in case of Mode-1
		 * can be allowed
		 */

		if (ar->ab->is_bypassed)
			continue;

		if (ar->ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0 || ar->ab->is_reset) {
			ret = ath12k_mac_start(ar);
			if (ret) {
				ah->state = ATH12K_HW_STATE_OFF;

				ath12k_err(ar->ab, "fail to start mac operations in pdev idx %d ret %d\n",
					   ar->pdev_idx, ret);
				goto fail_start;
			}

			if (ath12k_check_erp_power_down(ag)) {
				ar->ab->powerup_triggered = false;
				ar->pdev_suspend = false;
			}
		}
		ar->pdev_suspend = false;
	}

	if (ath12k_check_erp_power_down(ag))
		clear_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags);

	return 0;

fail_start:
	for (; i > 0; i--) {
		ar = ath12k_ah_to_ar(ah, i - 1);
		ath12k_mac_stop(ar);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_start);

int ath12k_mac_rfkill_config(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	u32 param;
	int ret;

	if (ab->hw_params->rfkill_pin == 0)
		return -EOPNOTSUPP;

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "mac rfkill_pin %d rfkill_cfg %d rfkill_on_level %d",
		   ab->hw_params->rfkill_pin, ab->hw_params->rfkill_cfg,
		   ab->hw_params->rfkill_on_level);

	param = u32_encode_bits(ab->hw_params->rfkill_on_level,
				WMI_RFKILL_CFG_RADIO_LEVEL) |
		u32_encode_bits(ab->hw_params->rfkill_pin,
				WMI_RFKILL_CFG_GPIO_PIN_NUM) |
		u32_encode_bits(ab->hw_params->rfkill_cfg,
				WMI_RFKILL_CFG_PIN_AS_GPIO);

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_HW_RFKILL_CONFIG,
					param, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ab,
			    "failed to set rfkill config 0x%x: %d\n",
			    param, ret);
		return ret;
	}

	return 0;
}

int ath12k_mac_rfkill_enable_radio(struct ath12k *ar, bool enable)
{
	enum wmi_rfkill_enable_radio param;
	int ret;

	if (enable)
		param = WMI_RFKILL_ENABLE_RADIO_ON;
	else
		param = WMI_RFKILL_ENABLE_RADIO_OFF;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac %d rfkill enable %d",
		   ar->pdev_idx, param);

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_RFKILL_ENABLE,
					param, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set rfkill enable param %d: %d\n",
			    param, ret);
		return ret;
	}

	return 0;
}

void ath12k_mac_stop(struct ath12k *ar)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_hw *ah = ar->ah;
	struct htt_ppdu_stats_info *ppdu_stats, *tmp;
	int ret;
	enum dp_mon_stats_mode mode = ATH12k_DP_MON_BASIC_STATS;

	if (ar->ab->pm_suspend)
		return;

	lockdep_assert_held(&ah->hw_mutex);
	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dp_mon_rx_stats_config(ar, false, mode);
	ret = ath12k_dp_mon_rx_update_filter(ar);
	if (ret && (ret != -EOPNOTSUPP))
		ath12k_err(ar->ab, "failed to clear rx_filter for monitor status ring: (%d)\n",
			   ret);

	clear_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags);
	ath12k_dcs_wlan_intf_cleanup(ar);

	cancel_delayed_work_sync(&ar->scan.timeout);
	wiphy_work_cancel(ath12k_ar_to_hw(ar)->wiphy, &ar->scan.vdev_clean_wk);
	cancel_work_sync(&ar->regd_update_work);
	cancel_work_sync(&ar->reg_set_previous_country);
	cancel_work_sync(&ar->ab->rfkill_work);
	cancel_work_sync(&ar->ab->update_11d_work);
	cancel_work_sync(&ar->wlan_intf_work);
	ar->state_11d = ATH12K_11D_IDLE;
	complete(&ar->completed_11d_scan);

	spin_lock_bh(&dp_pdev->ppdu_list_lock);
	list_for_each_entry_safe(ppdu_stats, tmp, &dp_pdev->ppdu_stats_info, list) {
		list_del(&ppdu_stats->list);
		kfree(ppdu_stats);
	}
	spin_unlock_bh(&dp_pdev->ppdu_list_lock);

	ath12k_debugfs_nrp_cleanup_all(ar);

	if ((ath12k_erp_get_sm_state() == ATH12K_ERP_ENTER_COMPLETE) &&
	    !ar->allocated_vdev_map && !ar->pdev_suspend) {
		ret = ath12k_mac_pdev_suspend(ar);
		if (ret)
			ath12k_warn(ar->ab, "pdev suspend command is failed %d\n", ret);
	}

	rcu_assign_pointer(ar->ab->pdevs_active[ar->pdev_idx], NULL);

	synchronize_rcu();

	atomic_set(&ar->num_pending_mgmt_tx, 0);

	spin_lock_bh(&ar->data_lock);
        ar->awgn_intf_handling_in_prog = false;
        spin_unlock_bh(&ar->data_lock);
}

void ath12k_mac_op_stop(struct ieee80211_hw *hw, bool suspend)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;
	int i;

	lockdep_assert_wiphy(hw->wiphy);

	ath12k_drain_tx(ah);

	mutex_lock(&ah->hw_mutex);

	ah->state = ATH12K_HW_STATE_OFF;

	for_each_ar(ah, ar, i) {
		if (ar->ab->is_bypassed)
			continue;
		ath12k_mac_stop(ar);
	}

	mutex_unlock(&ah->hw_mutex);
}
EXPORT_SYMBOL(ath12k_mac_op_stop);

static u8
ath12k_mac_get_vdev_stats_id(struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = arvif->ar->ab;
	u8 vdev_stats_id = 0;

	do {
		if (ab->free_vdev_stats_id_map & (1LL << vdev_stats_id)) {
			vdev_stats_id++;
			if (vdev_stats_id >= ATH12K_MAX_VDEV_STATS_ID) {
				vdev_stats_id = ATH12K_INVAL_VDEV_STATS_ID;
				break;
			}
		} else {
			ab->free_vdev_stats_id_map |= (1LL << vdev_stats_id);
			break;
		}
	} while (vdev_stats_id);

	arvif->vdev_stats_id = vdev_stats_id;
	return vdev_stats_id;
}

static int ath12k_mac_setup_vdev_params_mbssid(struct ath12k_link_vif *arvif,
					       u32 *flags, u32 *tx_vdev_id)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_bss_conf *link_conf;
	struct ath12k *ar = arvif->ar;
	struct ieee80211_vif *tx_vif;
	struct ath12k_link_vif *tx_arvif;

	if (ath12k_mac_is_bridge_vdev(arvif))
		return 0;

	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!link_conf) {
		ath12k_warn(ar->ab, "unable to access bss link conf in set mbssid params for vif %pM link %u\n",
			    ahvif->vif->addr, arvif->link_id);
		return -ENOLINK;
	}

        tx_vif = link_conf->mbssid_tx_vif;
        if (!tx_vif) {
                /* Since a 6GHz AP is MBSS capable by default, FW expects
                 * Tx vdev flag to be set even in case of single bss case
                 * WMI_HOST_VDEV_FLAGS_NON_MBSSID_AP is to be used for non 6GHz
                 * cases
                 */
                if (ar->supports_6ghz && arvif->ahvif->vif->type == NL80211_IFTYPE_AP)
                        *flags = WMI_VDEV_MBSSID_FLAGS_TRANSMIT_AP;
                else
                        *flags = WMI_VDEV_MBSSID_FLAGS_NON_MBSSID_AP;
                return 0;
        }

	tx_arvif = ath12k_mac_get_tx_arvif(arvif, link_conf);
	if (!tx_arvif)
		return 0;

	if (link_conf->nontransmitted) {
		if (ath12k_ar_to_hw(ar)->wiphy !=
		    ath12k_ar_to_hw(tx_arvif->ar)->wiphy)
			return -EINVAL;

		*flags = WMI_VDEV_MBSSID_FLAGS_NON_TRANSMIT_AP;
		*tx_vdev_id = tx_arvif->vdev_id;
	} else if (tx_arvif == arvif) {
		*flags = WMI_VDEV_MBSSID_FLAGS_TRANSMIT_AP;
		*tx_vdev_id = arvif->vdev_id;
	} else {
		return -EINVAL;
	}
	arvif->tx_vdev_id = *tx_vdev_id;

	if (link_conf->ema_ap)
		*flags |= WMI_VDEV_MBSSID_FLAGS_EMA_MODE;

	return 0;
}

static int ath12k_mac_setup_vdev_create_arg(struct ath12k_link_vif *arvif,
					    struct ath12k_wmi_vdev_create_arg *arg)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_pdev *pdev = ar->pdev;
	struct ath12k_vif *ahvif = arvif->ahvif;
	bool is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	arg->if_id = arvif->vdev_id;
	arg->type = ahvif->vdev_type;
	arg->subtype = arvif->vdev_subtype;
	arg->pdev_id = pdev->pdev_id;

	arg->mbssid_tx_vdev_id = 0;
	if (is_bridge_vdev)
		arg->mbssid_flags = 0;
	else
		arg->mbssid_flags = WMI_VDEV_MBSSID_FLAGS_NON_MBSSID_AP;

	if (!test_bit(WMI_TLV_SERVICE_MBSS_PARAM_IN_VDEV_START_SUPPORT,
		      ar->ab->wmi_ab.svc_map)) {
		ret = ath12k_mac_setup_vdev_params_mbssid(arvif,
							  &arg->mbssid_flags,
							  &arg->mbssid_tx_vdev_id);
		if (ret)
			return ret;
	}

	if (pdev->cap.supported_bands & WMI_HOST_WLAN_2GHZ_CAP) {
		arg->chains[NL80211_BAND_2GHZ].tx = ar->num_tx_chains;
		arg->chains[NL80211_BAND_2GHZ].rx = ar->num_rx_chains;
	}
	if (pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP) {
		arg->chains[NL80211_BAND_5GHZ].tx = ar->num_tx_chains;
		arg->chains[NL80211_BAND_5GHZ].rx = ar->num_rx_chains;
	}
	if (pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP &&
	    ar->supports_6ghz) {
		arg->chains[NL80211_BAND_6GHZ].tx = ar->num_tx_chains;
		arg->chains[NL80211_BAND_6GHZ].rx = ar->num_rx_chains;
	}

	arg->if_stats_id = ath12k_mac_get_vdev_stats_id(arvif);

	if (ath12k_mac_is_ml_arvif(arvif)) {
		if (!is_bridge_vdev &&
		    hweight16(ahvif->vif->valid_links) > ATH12K_WMI_MLO_MAX_LINKS) {
			ath12k_warn(ar->ab, "too many MLO links during setting up vdev: %d",
				    ahvif->vif->valid_links);
			return -EINVAL;
		}

		ether_addr_copy(arg->mld_addr, ahvif->vif->addr);
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
				"MLD address:%pM for vdev:%d arvif addr :%pM",
				arg->mld_addr, arvif->vdev_id, arvif->bssid);
	}

	return 0;
}

static void ath12k_mac_update_vif_offload(struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	u32 param_id, param_value;
	int ret;

	param_id = WMI_VDEV_PARAM_TX_ENCAP_TYPE;
	if (ath12k_frame_mode != ATH12K_HW_TXRX_ETHERNET ||
	    (vif->type != NL80211_IFTYPE_STATION &&
	     vif->type != NL80211_IFTYPE_AP))
		vif->offload_flags &= ~(IEEE80211_OFFLOAD_ENCAP_ENABLED |
					IEEE80211_OFFLOAD_DECAP_ENABLED);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* TODO: DS: revisit this for DS support in WDS mode */
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) &&
	    (vif->type == NL80211_IFTYPE_AP || vif->type == NL80211_IFTYPE_STATION))
		vif->offload_flags |= (IEEE80211_OFFLOAD_ENCAP_ENABLED |
				IEEE80211_OFFLOAD_DECAP_ENABLED);
#endif

	if (vif->offload_flags & IEEE80211_OFFLOAD_ENCAP_ENABLED)
		ahvif->dp_vif.tx_encap_type = ATH12K_HW_TXRX_ETHERNET;
	else if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags))
		ahvif->dp_vif.tx_encap_type = ATH12K_HW_TXRX_RAW;
	else
		ahvif->dp_vif.tx_encap_type = ATH12K_HW_TXRX_NATIVE_WIFI;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param_id, ahvif->dp_vif.tx_encap_type);
	if (ret) {
		ath12k_warn(ab, "failed to set vdev %d tx encap mode: %d\n",
			    arvif->vdev_id, ret);
		vif->offload_flags &= ~IEEE80211_OFFLOAD_ENCAP_ENABLED;
	}

	param_id = WMI_VDEV_PARAM_RX_DECAP_TYPE;
	if (vif->offload_flags & IEEE80211_OFFLOAD_DECAP_ENABLED)
		param_value = ATH12K_HW_TXRX_ETHERNET;
	else if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags))
		param_value = ATH12K_HW_TXRX_RAW;
	else
		param_value = ATH12K_HW_TXRX_NATIVE_WIFI;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param_id, param_value);
	if (ret) {
		ath12k_warn(ab, "failed to set vdev %d rx decap mode: %d\n",
			    arvif->vdev_id, ret);
		vif->offload_flags &= ~IEEE80211_OFFLOAD_DECAP_ENABLED;
	}
}

void ath12k_mac_op_update_vif_offload(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	unsigned long links;
	int link_id;

	lockdep_assert_wiphy(hw->wiphy);

	if (vif->valid_links) {
		links = vif->valid_links;
		for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
			arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
			if (!(arvif && arvif->ar))
				continue;

			ath12k_mac_update_vif_offload(arvif);
		}

		return;
	}

	ath12k_mac_update_vif_offload(&ahvif->deflink);
}
EXPORT_SYMBOL(ath12k_mac_op_update_vif_offload);

static bool ath12k_mac_vif_ap_active_any(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	struct ath12k_link_vif *arvif;
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		list_for_each_entry(arvif, &ar->arvifs, list) {
			if (arvif->is_up &&
			    arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP)
				return true;
		}
	}
	return false;
}

void ath12k_mac_11d_scan_start(struct ath12k *ar, u32 vdev_id)
{
	struct wmi_11d_scan_start_arg arg;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ar->regdom_set_by_user)
		goto fin;

	if (ar->vdev_id_11d_scan != ATH12K_11D_INVALID_VDEV_ID)
		goto fin;

	if (!test_bit(WMI_TLV_SERVICE_11D_OFFLOAD, ar->ab->wmi_ab.svc_map))
		goto fin;

	if (ath12k_mac_vif_ap_active_any(ar->ab))
		goto fin;

	arg.vdev_id = vdev_id;
	arg.start_interval_msec = 0;
	arg.scan_period_msec = ATH12K_SCAN_11D_INTERVAL;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
			"mac start 11d scan for vdev %d\n", vdev_id);

	ret = ath12k_wmi_send_11d_scan_start_cmd(ar, &arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to start 11d scan vdev %d ret: %d\n",
			    vdev_id, ret);
	} else {
		ar->vdev_id_11d_scan = vdev_id;
		if (ar->state_11d == ATH12K_11D_PREPARING)
			ar->state_11d = ATH12K_11D_RUNNING;
	}

fin:
	if (ar->state_11d == ATH12K_11D_PREPARING) {
		ar->state_11d = ATH12K_11D_IDLE;
		complete(&ar->completed_11d_scan);
	}
}

void ath12k_mac_11d_scan_stop(struct ath12k *ar)
{
	int ret;
	u32 vdev_id;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!test_bit(WMI_TLV_SERVICE_11D_OFFLOAD, ar->ab->wmi_ab.svc_map))
		return;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
			"mac stop 11d for vdev %d\n", ar->vdev_id_11d_scan);

	if (ar->state_11d == ATH12K_11D_PREPARING) {
		ar->state_11d = ATH12K_11D_IDLE;
		complete(&ar->completed_11d_scan);
	}

	if (ar->vdev_id_11d_scan != ATH12K_11D_INVALID_VDEV_ID) {
		vdev_id = ar->vdev_id_11d_scan;

		ret = ath12k_wmi_send_11d_scan_stop_cmd(ar, vdev_id);
		if (ret) {
			ath12k_warn(ar->ab,
						"failed to stopt 11d scan vdev %d ret: %d\n",
						vdev_id, ret);
		} else {
			ar->vdev_id_11d_scan = ATH12K_11D_INVALID_VDEV_ID;
			ar->state_11d = ATH12K_11D_IDLE;
			complete(&ar->completed_11d_scan);
		}
	}
}

void ath12k_mac_11d_scan_stop_all(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	int i;

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L0, "mac stop soc 11d scan\n");

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;

		ath12k_mac_11d_scan_stop(ar);
	}
}

int ath12k_mac_pdev_resume(struct ath12k *ar)
{
	unsigned long time_left;
	int ret;

	reinit_completion(&ar->pdev_resume);
	ret = ath12k_wmi_pdev_resume(ar, ar->pdev->pdev_id);

	if (ret) {
		ath12k_err(ar->ab, "failed to send wmi resume command %d\n", ret);
		return ret;
	}

	time_left = wait_for_completion_timeout(&ar->pdev_resume,
						ATH12K_PDEV_RESUME_TIMEOUT);
	if (!time_left) {
		ath12k_err(ar->ab, "timeout in receiving pdev resume response %d\n",
			   ar->pdev->pdev_id);
		return -ETIMEDOUT;
	}

	return 0;
}

int ath12k_mac_vdev_create(struct ath12k *ar, struct ath12k_link_vif *arvif,
			   bool is_bridge_vdev)
{
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_hw *hw = ah->hw;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ath12k_wmi_vdev_create_arg vdev_arg = {0};
	struct ath12k_wmi_peer_create_arg peer_param = {0};
	struct ieee80211_bss_conf *link_conf = NULL;
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(vif);
	u32 param_id, param_value;
	u16 nss;
	int i;
	int ret, fbret, vdev_id;
	u8 link_id, link_addr[ETH_ALEN];
	struct ath12k_dp_link_vif *dp_link_vif = NULL;
	u8 mac_addr[ETH_ALEN];
	u8 mask[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00};
	int txpower = NL80211_TX_POWER_AUTOMATIC;
	u8 map_id;
	u32 rep_ul_resp;
	struct ath12k_dp_peer_create_params params = {};

	lockdep_assert_wiphy(hw->wiphy);

	/* In NO_VIRTUAL_MONITOR, its necessary to restrict only one monitor
	 * interface in each radio
	 */
	if (vif->type == NL80211_IFTYPE_MONITOR && ar->monitor_vdev_created)
		return -EINVAL;

	if (ar->pdev_suspend) {
		ret = ath12k_mac_pdev_resume(ar);
		if (ret) {
			ath12k_err(ab,
				   "vdev could not be created because the pdev failed to resume\n");
			return ret;
		}
	}

	/* If no link is active and scan vdev is requested
	 * use a default link conf for scan address purpose.
	 */
	if (arvif->link_id >= ATH12K_DEFAULT_SCAN_LINK && vif->valid_links)
		link_id = ffs(vif->valid_links) - 1;
	else if (arvif->link_id == ATH12K_DEFAULT_SCAN_LINK &&
		 vif->type == NL80211_IFTYPE_STATION)
		link_id = 0;
	else
		link_id = arvif->link_id;

	if (!is_bridge_vdev) {
		if (!arvif->is_scan_vif && link_id >= ARRAY_SIZE(vif->link_conf)) {
			ath12k_warn(ar->ab, "link_id %u exceeds max valid links for vif %pM\n",
				    link_id, vif->addr);
			return -EINVAL;
		}

		if (link_id < ATH12K_DEFAULT_SCAN_LINK) {
			link_conf = wiphy_dereference(hw->wiphy, vif->link_conf[link_id]);
			if (!link_conf && !arvif->is_scan_vif) {
				ath12k_warn(ar->ab, "unable to access bss link conf in vdev create for vif %pM link %u\n",
					    vif->addr, arvif->link_id);
				return -ENOLINK;
			}
		}

		if (arvif->link_id == ATH12K_DEFAULT_SCAN_LINK &&
		    !is_zero_ether_addr(arvif->bssid)) {
			memcpy(link_addr, arvif->bssid, ETH_ALEN);
		} else if (link_conf) {
			/* In split-phy scenario with MLO, use RANDOM MAC for scan vdev
			 * only when creating a scan link (ATH12K_DEFAULT_SCAN_LINK)
			 * to avoid MAC address conflicts across radios.
			 */
			memcpy(link_addr, link_conf->addr, ETH_ALEN);
			if (arvif->link_id == ATH12K_DEFAULT_SCAN_LINK &&
			    vif->valid_links && vif->type == NL80211_IFTYPE_STATION)
				eth_random_addr(link_addr);
			memcpy(arvif->bssid, link_addr, ETH_ALEN);
		} else {
			eth_random_addr(link_addr);
			memcpy(arvif->bssid, link_addr, ETH_ALEN);
		}
		if (link_conf)
			txpower = link_conf->txpower;
	} else if (is_bridge_vdev) {
		if (ath12k_hw_group_recovery_in_progress(ab->ag)) {
			memcpy(link_addr, arvif->bssid, ETH_ALEN);
		} else {
			/* Generate mac address for bridge vap */
			/* To Do: Need to check duplicate? */
			eth_random_addr(arvif->bssid);
			memcpy(link_addr, arvif->bssid, ETH_ALEN);
		}
	}
	/* Send vdev stats offload commands to firmware before first vdev
	 * creation. ie., when num_created_vdevs = 0
	 */
	if (ar->fw_stats.en_vdev_stats_ol && !ar->num_created_vdevs) {
		ret = ath12k_dp_tx_htt_h2t_vdev_stats_ol_req(ar, 0);
		if (ret) {
			ath12k_warn(ar->ab, "failed to request vdev stats offload: %d\n", ret);
			goto err;
		}
	}

	arvif->ar = ar;

	spin_lock_bh(&ar->ab->base_lock);
	if (!ab->free_vdev_map) {
		spin_unlock_bh(&ar->ab->base_lock);
		ath12k_warn(ar->ab, "failed to create vdev. No free vdev id left.\n");
		ret = -EINVAL;
		goto err;
	}
	vdev_id = __ffs64(ab->free_vdev_map);
	ab->free_vdev_map &= ~(1LL << vdev_id);
	spin_unlock_bh(&ar->ab->base_lock);

	arvif->vdev_id = vdev_id;
	/* Assume it as non-mbssid initially, well overwrite it later.
	 */
	arvif->tx_vdev_id = vdev_id;

	arvif->vdev_subtype = is_bridge_vdev ? WMI_VDEV_SUBTYPE_BRIDGE : WMI_VDEV_SUBTYPE_NONE;

	if (!ar->free_map_id) {
		ath12k_err(ar->ab, "No free map_id available\n");
		ret = -EINVAL;
		goto err;
	}
	map_id =  __ffs(ar->free_map_id);
	ar->free_map_id &= ~(1 << map_id);
	arvif->map_id = map_id;

	dp_link_vif = &ahvif->dp_vif.dp_link_vif[arvif->link_id];

	dp_link_vif->vdev_id = arvif->vdev_id;
	dp_link_vif->lmac_id = ar->lmac_id;
	dp_link_vif->pdev_idx = ar->pdev_idx;
	dp_link_vif->map_id = arvif->map_id;

	switch (vif->type) {
	case NL80211_IFTYPE_UNSPECIFIED:
	case NL80211_IFTYPE_STATION:
		ahvif->vdev_type = WMI_VDEV_TYPE_STA;

		if (vif->p2p)
			arvif->vdev_subtype = WMI_VDEV_SUBTYPE_P2P_CLIENT;

		break;
	case NL80211_IFTYPE_MESH_POINT:
		arvif->vdev_subtype = WMI_VDEV_SUBTYPE_MESH_11S;
		fallthrough;
	case NL80211_IFTYPE_AP:
		ahvif->vdev_type = WMI_VDEV_TYPE_AP;

		if (vif->p2p)
			arvif->vdev_subtype = WMI_VDEV_SUBTYPE_P2P_GO;

		break;
	case NL80211_IFTYPE_MONITOR:
		ahvif->vdev_type = WMI_VDEV_TYPE_MONITOR;
		ar->monitor_vdev_id = vdev_id;
		get_random_mask_addr(mac_addr, ar->mac_addr, mask);
		break;
	case NL80211_IFTYPE_P2P_DEVICE:
		ahvif->vdev_type = WMI_VDEV_TYPE_STA;
		arvif->vdev_subtype = WMI_VDEV_SUBTYPE_P2P_DEVICE;
		break;
	default:
		WARN_ON(1);
		break;
	}

	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			 "mac vdev create id %d type %d subtype %d map %llx\n",
			 arvif->vdev_id, ahvif->vdev_type, arvif->vdev_subtype,
			 ab->free_vdev_map);
	}

	vif->cab_queue = arvif->vdev_id % (ATH12K_HW_MAX_QUEUES - 1);
	for (i = 0; i < ARRAY_SIZE(vif->hw_queue); i++)
		vif->hw_queue[i] = i % (ATH12K_HW_MAX_QUEUES - 1);

	ret = ath12k_mac_setup_vdev_create_arg(arvif, &vdev_arg);
	if (ret) {
		ath12k_warn(ab, "failed to create vdev parameters %d: %d\n",
			    arvif->vdev_id, ret);
	        spin_lock_bh(&ar->ab->base_lock);
		ab->free_vdev_map |= 1LL << arvif->vdev_id;
		spin_unlock_bh(&ar->ab->base_lock);
		ar->free_map_id |= 1 << arvif->map_id;

		goto err;
	}

	ret = ath12k_wmi_vdev_create(ar, vdev_arg.type == WMI_VDEV_TYPE_MONITOR ?
				     mac_addr : arvif->bssid, &vdev_arg);
	if (ret) {
		ath12k_warn(ab, "failed to create WMI vdev %d: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	if (is_bridge_vdev)
		ar->num_created_bridge_vdevs++;
	else
		ar->num_created_vdevs++;
	arvif->is_created = true;

	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA) {
		ath12k_dbg(ab, ATH12K_DBG_MAC, "vdev %pM created, vdev_id %d\n",
		   arvif->bssid, arvif->vdev_id);
	}
	ar->allocated_vdev_map |= 1LL << arvif->vdev_id;

	spin_lock_bh(&ar->data_lock);

	/* list added is not needed during mode1 recovery
	 * as the arvif(s) updated are from the existing
	 * list
	 */
	if (!ab->recovery_start)
		list_add(&arvif->list, &ar->arvifs);

	spin_unlock_bh(&ar->data_lock);

	ath12k_mac_update_vif_offload(arvif);

	nss = hweight32(ar->cfg_tx_chainmask) ? : 1;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_NSS, nss);
	if (ret) {
		ath12k_warn(ab, "failed to set vdev %d chainmask 0x%x, nss %d :%d\n",
			    arvif->vdev_id, ar->cfg_tx_chainmask, nss, ret);
		goto err_vdev_del;
	}

	switch (ahvif->vdev_type) {
	case WMI_VDEV_TYPE_AP:
		params.is_vdev_peer = true;
		params.hw_link_id = ar->hw_link_id;

		ret = ath12k_dp_peer_create(&ah->dp_hw, arvif->bssid, &params, vif);
		if (ret) {
			ath12k_warn(ab, "failed to vdev %d create dp_peer for AP: %d\n",
				    arvif->vdev_id, ret);
			goto err_vdev_del;
		}

		peer_param.vdev_id = arvif->vdev_id;
		peer_param.peer_addr = arvif->bssid;
		peer_param.peer_type = WMI_PEER_TYPE_DEFAULT;
		ret = ath12k_peer_create(ar, arvif, NULL, &peer_param);
		if (ret) {
			ath12k_warn(ab, "failed to vdev %d create peer for AP: %d\n",
				    arvif->vdev_id, ret);
			goto err_dp_peer_del;
		}

		ret = ath12k_mac_set_kickout(arvif);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vdev %i kickout parameters: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}
		ath12k_mac_11d_scan_stop_all(ar->ab);
		break;
	case WMI_VDEV_TYPE_STA:
		param_id = WMI_STA_PS_PARAM_RX_WAKE_POLICY;
		param_value = WMI_STA_PS_RX_WAKE_POLICY_WAKE;
		ret = ath12k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
						  param_id, param_value);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vdev %d RX wake policy: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}

		param_id = WMI_STA_PS_PARAM_TX_WAKE_THRESHOLD;
		param_value = WMI_STA_PS_TX_WAKE_THRESHOLD_ALWAYS;
		ret = ath12k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
						  param_id, param_value);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vdev %d TX wake threshold: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}

		param_id = WMI_STA_PS_PARAM_PSPOLL_COUNT;
		param_value = WMI_STA_PS_PSPOLL_COUNT_NO_MAX;
		ret = ath12k_wmi_set_sta_ps_param(ar, arvif->vdev_id,
						  param_id, param_value);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vdev %d pspoll count: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}

		ret = ath12k_wmi_pdev_set_ps_mode(ar, arvif->vdev_id, false);
		if (ret) {
			ath12k_warn(ar->ab, "failed to disable vdev %d ps mode: %d\n",
				    arvif->vdev_id, ret);
			goto err_peer_del;
		}

		if (test_bit(WMI_TLV_SERVICE_11D_OFFLOAD, ab->wmi_ab.svc_map) &&
		    ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
		    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_NONE) {
			reinit_completion(&ar->completed_11d_scan);
			ar->state_11d = ATH12K_11D_PREPARING;
		}
		rep_ul_resp = ((ath12k_cfg_get(ab, ATH12K_CFG_REP_UL_RESP) >>
							ar->pdev->pdev_id) & 01);
		if (rep_ul_resp) {
			param_value = 0;
			param_id = WMI_VDEV_PARAM_SET_HEMU_MODE;
			param_value |= u32_encode_bits(HE_UL_MUMIMO_ENABLE,
							HE_MODE_UL_MUMIMO) |
					u32_encode_bits(HE_DL_MUOFDMA_ENABLE,
							HE_MODE_DL_OFDMA) |
					u32_encode_bits(HE_UL_MUOFDMA_ENABLE,
					HE_MODE_UL_OFDMA);

			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
							param_id, param_value);
			if (ret) {
				ath12k_warn(ar->ab,
				"failed to set vdev %d HE MU mode: %d\n",
				arvif->vdev_id, ret);
			}
			param_value = 0;
			param_id = WMI_VDEV_PARAM_SET_EHT_MU_MODE;
			param_value |= u32_encode_bits(EHT_UL_MUMIMO_ENABLE,
							EHT_MODE_MUMIMO) |
					u32_encode_bits(EHT_DL_MUOFDMA_ENABLE,
							EHT_MODE_DL_OFDMA) |
					u32_encode_bits(EHT_UL_MUOFDMA_ENABLE,
							EHT_MODE_UL_OFDMA);
			ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
							param_id, param_value);
			if (ret) {
				ath12k_warn(ar->ab,
				"failed to set vdev %d EHT MU mode: %d\n",
				arvif->vdev_id, ret);
			}
		}

		break;
	case WMI_VDEV_TYPE_MONITOR:
		ar->monitor_vdev_created = true;
		break;
	default:
		break;
	}

	arvif->txpower = txpower;
	ret = ath12k_mac_txpower_recalc(ar);
	if (ret)
		goto err_peer_del;

	if (ath12k_mlo_3_link_tx) {
		param_id = WMI_VDEV_PARAM_MLO_MAX_RECOM_ACTIVE_LINKS;

		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param_id, MLO_3LINK_MAX_RECOM_ACTIVE_LINKS);

		if (ret) {
			ath12k_warn(ar->ab,
				    "failed to set max recom active links"
				    "for vdev %d: %d\n",
				    arvif->vdev_id, ret);
		}
	}

	ath12k_debugfs_add_interface(arvif);

	param_id = WMI_VDEV_PARAM_RTS_THRESHOLD;
	param_value = hw->wiphy->rts_threshold;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param_id, param_value);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set rts threshold for vdev %d: %d\n",
			    arvif->vdev_id, ret);
	}

	ath12k_mac_ap_ps_recalc(ar);
	ath12k_dp_vdev_tx_attach(ar, arvif);

	if (vif->type == NL80211_IFTYPE_STATION &&
	    (wdev && wdev->use_4addr)) {
		ret = ath12k_wmi_vdev_set_param_cmd(arvif->ar, arvif->vdev_id,
						    WMI_VDEV_PARAM_WDS, 1);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set WDS vdev param: %d\n", ret);
			goto err_vdev_del;
		}
		arvif->set_wds_vdev_param = true;
	}

	return ret;

err_peer_del:
	if (ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
		fbret = ath12k_peer_delete(ar, arvif->vdev_id, link_addr, NULL);
		if (fbret) {
			ath12k_warn(ar->ab, "failed to delete peer %pM vdev_id %d ret %d\n",
				    link_addr, arvif->vdev_id, fbret);
		}
	}

err_dp_peer_del:
	if (ahvif->vdev_type == WMI_VDEV_TYPE_AP)
		ath12k_dp_peer_delete(&ah->dp_hw, arvif->bssid, NULL, ar->hw_link_id);

err_vdev_del:
	ath12k_wmi_vdev_delete(ar, arvif->vdev_id);
	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ar->monitor_vdev_created = false;
		ar->monitor_vdev_id = -1;
	}
	if (is_bridge_vdev) {
		WARN_ON(!ar->num_created_bridge_vdevs);
		ar->num_created_bridge_vdevs--;
	} else {
		WARN_ON(!ar->num_created_vdevs);
		ar->num_created_vdevs--;
	}
	arvif->is_created = false;
	arvif->ar = NULL;
	ar->allocated_vdev_map &= ~(1LL << arvif->vdev_id);
	spin_lock_bh(&ar->ab->base_lock);
	ab->free_vdev_map |= 1LL << arvif->vdev_id;
	spin_unlock_bh(&ar->ab->base_lock);
	ar->free_map_id |= 1 << arvif->map_id;
	ab->free_vdev_stats_id_map &= ~(1LL << arvif->vdev_stats_id);
	spin_lock_bh(&ar->data_lock);
	if (!list_empty(&ar->arvifs))
		list_del(&arvif->list);
	spin_unlock_bh(&ar->data_lock);
err:
	arvif->ar = NULL;
	return ret;
}

static void ath12k_mac_vif_flush_key_cache(struct ath12k_link_vif *arvif)
{
	struct ath12k_key_conf *key_conf, *tmp;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	struct ath12k_vif_cache *cache = ahvif->cache[arvif->link_id];
	int ret;

	lockdep_assert_wiphy(ah->hw->wiphy);

	list_for_each_entry_safe(key_conf, tmp, &cache->key_conf.list, list) {
		arsta = NULL;
		if (key_conf->sta) {
			ahsta = ath12k_sta_to_ahsta(key_conf->sta);
			arsta = wiphy_dereference(ah->hw->wiphy,
						  ahsta->link[arvif->link_id]);
			if (!arsta)
				goto free_cache;
		}

		ret = ath12k_mac_set_key(arvif->ar, key_conf->cmd,
					 arvif, arsta,
					 key_conf->key);
		if (ret)
			ath12k_warn(arvif->ar->ab, "unable to apply set key param to vdev %d ret %d\n",
				    arvif->vdev_id, ret);
free_cache:
		list_del(&key_conf->list);
		kfree(key_conf);
	}
}

void ath12k_mac_vif_cache_flush(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ath12k_vif_cache *cache = ahvif->cache[arvif->link_id];
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_bss_conf *link_conf;

	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!cache)
		return;

	if (cache->tx_conf.changed) {
		ret = ath12k_mac_conf_tx(arvif, cache->tx_conf.ac,
					 &cache->tx_conf.tx_queue_params);
		if (ret)
			ath12k_warn(ab,
				    "unable to apply tx config parameters to vdev %d\n",
				    ret);
	}

	if (cache->bss_conf_changed) {
		link_conf = ath12k_mac_get_link_bss_conf(arvif);
		if (!link_conf) {
			ath12k_warn(ar->ab, "unable to access bss link conf in cache flush for vif %pM link %u\n",
				    vif->addr, arvif->link_id);
			return;
		}
		ath12k_mac_bss_info_changed(ar, arvif, link_conf,
					    cache->bss_conf_changed);
	}

	if (cache->cache_qos_map.qos_map) {
		spin_lock_bh(&ar->data_lock);
		arvif->qos_map = cache->cache_qos_map.qos_map;
		ath12k_mac_update_qos_map(ar, arvif);
		spin_unlock_bh(&ar->data_lock);
		cache->cache_qos_map.qos_map = NULL;
	}

	if (!list_empty(&cache->key_conf.list))
		ath12k_mac_vif_flush_key_cache(arvif);

	ath12k_ahvif_put_link_cache(ahvif, arvif->link_id);
}

static struct ath12k *ath12k_mac_assign_vif_to_vdev(struct ieee80211_hw *hw,
						    struct ath12k_link_vif *arvif,
						    struct ieee80211_chanctx_conf *ctx,
						    bool is_bridge_vdev,
						    u16 bridge_ar_link_idx)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ath12k_link_vif *scan_arvif;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct ath12k_base *ab;
	u8 link_id = arvif->link_id, scan_link;
	unsigned long scan_link_map;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	if (ah->num_radio == 1)
		ar = ah->radio;
	else if (is_bridge_vdev)
		ar = ath12k_get_ar_by_link_idx(ah, bridge_ar_link_idx);
	else if (ctx)
		ar = ath12k_get_ar_by_ctx(hw, ctx);
	else
		return NULL;

	if (!ar)
		return NULL;

	/* cleanup the scan vdev if we are done scan on that ar
	 * and now we want to create for actual usage.
	 */
	if (ieee80211_vif_is_mld(vif)) {
		scan_link_map = ahvif->links_map & ATH12K_SCAN_LINKS_MASK;
		for_each_set_bit(scan_link, &scan_link_map, ATH12K_NUM_MAX_LINKS) {
			scan_arvif = wiphy_dereference(hw->wiphy, ahvif->link[scan_link]);
			if ((scan_arvif && scan_arvif->ar == ar) ||
			    ar->scan.arvif == arvif) {
				if (arvif->is_started) {
					ret = ath12k_mac_vdev_stop(arvif);
					if (ret) {
						ath12k_warn(ar->ab, "failed to stop vdev %d: %d\n",
							    arvif->vdev_id, ret);
						return NULL;
					}
					arvif->is_started = false;
				}
			}
			if (scan_arvif && scan_arvif->ar == ar && !is_bridge_vdev) {
				ar->scan.arvif = NULL;
				ath12k_mac_remove_link_interface(hw, scan_arvif);
				ath12k_mac_unassign_link_vif(scan_arvif);
			}
		}
	}

	if (arvif->ar) {
		if (arvif->ar->ab->is_bypassed)
			return arvif->ar;

		/* This is not expected really */
		if (!test_bit(ATH12K_FLAG_RECOVERY,&arvif->ar->ab->dev_flags) && !arvif->is_created) {
			WARN_ON(1);
			arvif->ar = NULL;
			return NULL;
		}

		if (ah->num_radio == 1)
			return arvif->ar;

		/* This can happen as scan vdev gets created during multiple scans
		 * across different radios before a vdev is brought up in
		 * a certain radio.
		 */
		if (ar != arvif->ar) {
			if (WARN_ON(arvif->is_started))
				return NULL;

			ath12k_mac_remove_link_interface(hw, arvif);
			ath12k_mac_unassign_link_vif(arvif);
		}
	}

	ab = ar->ab;

	/* Assign arvif again here since previous radio switch block
	 * would've unassigned and cleared it.
	 */
	arvif = ath12k_mac_assign_link_vif(ah, vif, link_id, is_bridge_vdev);

	if (!arvif) {
		ath12k_err(ab, "Failed to alloc/assign link vif id %u\n",
			   link_id);
		return NULL;
	}

	if (vif->type == NL80211_IFTYPE_AP &&
	    ar->num_peers > (ar->max_num_peers - 1)) {
		ath12k_warn(ab, "failed to create vdev due to insufficient peer entry resource in firmware\n");
		goto unlock;
	}

	if (arvif->is_created)
		goto flush;

	if (ath12k_core_is_vdev_limit_reached(ar, is_bridge_vdev)) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = ath12k_mac_vdev_create(ar, arvif, is_bridge_vdev);
	if (ret) {
		ath12k_warn(ab, "failed to create vdev %pM ret %d", vif->addr, ret);
		goto unlock;
	}

flush:
	/* If the vdev is created during channel assign and not during
	 * add_interface(), Apply any parameters for the vdev which were received
	 * after add_interface, corresponding to this vif.
	 */
	if (!is_bridge_vdev)
		ath12k_mac_vif_cache_flush(ar, arvif);

	arvif->ahvif->device_bitmap |= BIT(ar->ab->wsi_info.index);
unlock:
	return arvif->ar;
}

int ath12k_mac_op_add_interface(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(vif);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_vif *vlan_master_ahvif;
	struct ieee80211_vif *vlan_master_vif = NULL;
	struct ath12k_vlan_iface *vlan_iface = NULL;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar = ath12k_ah_to_ar(ah, 0);
	int ppe_vp_num = ATH12K_INVALID_PPE_VP_NUM, ppe_core_mask = 0;
	int i, ppe_vp_type = ATH12K_INVALID_PPE_VP_TYPE;
	unsigned long links_map = 0;

	lockdep_assert_wiphy(hw->wiphy);

	if (!wdev) {
		ath12k_warn(ar->ab, "Failed to get wdev from vif\n");
		return -ENODEV;
	}

	/* Get the VP number from the nss-wifi plugin,
	 * which is allocated during netdev initialization.
	 * This also handles Subsystem Recovery scenarios.
	 */
	if (ath12k_vif_get_vp_num(ahvif, wdev->netdev))
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "failed to get VP num from nss-wifi-plugin\n");

	if (ahvif->dp_vif.ppe_vp_num > 0) {
		ppe_vp_num = ahvif->dp_vif.ppe_vp_num;
		ppe_core_mask = ahvif->dp_vif.ppe_core_mask;
		ppe_vp_type = ahvif->dp_vif.ppe_vp_type;
		vlan_iface = ahvif->vlan_iface;
		links_map = ahvif->links_map;
	}

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->ag->flags))
		ahvif->mode0_recover_bridge_vdevs =
			(ahvif->links_map & ATH12K_BRIDGE_LINKS_MASK) ?
			true : false;
	else
		memset(ahvif, 0, sizeof(*ahvif));

	ahvif->ah = ah;
	ahvif->vif = vif;
	arvif = &ahvif->deflink;
	/* Restore the VP information if VP is allocated
	 * successfully at the time of iface init.
	 */
	ahvif->dp_vif.ppe_vp_num = ppe_vp_num;
	ahvif->dp_vif.ppe_vp_type = ppe_vp_type;
	ahvif->tstats = alloc_percpu_gfp(struct pcpu_netdev_tid_stats, GFP_KERNEL);
	if (!ahvif->tstats)
		return -ENOMEM;
	ath12k_mac_init_arvif(ahvif, arvif, -1, false);

	/* Check the PPE VP type and update it accordingly.
	 */

	switch (wdev->ppe_vp_type) {
	case PPE_VP_USER_TYPE_PASSIVE:
	case PPE_VP_USER_TYPE_ACTIVE:
	case PPE_VP_USER_TYPE_DS:
		ppe_vp_type = wdev->ppe_vp_type;
		break;
	default:

	/* Set default PPE VP type to ACTIVE to ensure VP allocation during interface
	 * creation in Mesh mode. Previously, the default was PASSIVE, which caused
	 * VP allocation to be freed in the ath client. This led to failures when
	 * vendor commands attempted to update VP TYPE as Active, as no VP was associated
	 * with the netdev. Changing the default to ACTIVE ensures that VP is not freed
	 * till the vendor command is received which updates the correct PPE VP type.
	 */
		ppe_vp_type = PPE_VP_USER_TYPE_ACTIVE;
	}

	if (vif->type == NL80211_IFTYPE_AP_VLAN) {
		vlan_master_vif = wdev_to_ieee80211_vif_vlan(wdev, true);
		vlan_master_ahvif = ath12k_vif_to_ahvif(vlan_master_vif);
		ahvif->vdev_type = WMI_VDEV_TYPE_AP;
		if (!vlan_master_ahvif)
			goto exit;
		ppe_vp_type = vlan_master_ahvif->dp_vif.ppe_vp_type;
		ppe_core_mask = vlan_master_ahvif->dp_vif.ppe_core_mask;
		goto ppe_vp_config;
	}

	if (vif->type == NL80211_IFTYPE_MESH_POINT &&
	    ppe_vp_type == PPE_VP_USER_TYPE_DS) {
		ppe_vp_type = PPE_VP_USER_TYPE_PASSIVE;
	}

ppe_vp_config:
	if (ppe_vp_num != ATH12K_INVALID_PPE_VP_NUM) {
		if (ppe_vp_type != ahvif->dp_vif.ppe_vp_type)
			ath12k_vif_update_vp_config(ahvif, ppe_vp_type);

		if (vif->type == NL80211_IFTYPE_AP_VLAN && vlan_iface) {
			ahvif->vlan_iface = vlan_iface;
			vlan_iface->attach_link_done = false;
			goto exit;
		}
	}

	if (vif->type == NL80211_IFTYPE_AP_VLAN &&
	    ahvif->dp_vif.ppe_vp_num != ATH12K_INVALID_PPE_VP_NUM) {
		struct ath12k_vlan_iface *vlan_iface;
		int ret;

		vlan_iface = kzalloc(sizeof(*vlan_iface), GFP_ATOMIC);
		if (!vlan_iface) {
			ret = -ENOMEM;
			if (ahvif->dp_vif.ppe_vp_type == PPE_VP_USER_TYPE_DS)
				ret = ath12k_vif_update_vp_config(ahvif, PPE_VP_USER_TYPE_PASSIVE);

			if (ret)
				return ret;
		}

		vlan_iface->parent_vif = vlan_master_vif;
		if (!links_map && vlan_master_ahvif)
			links_map = vlan_master_ahvif->links_map;

		ahvif->links_map = links_map;
		ahvif->vlan_iface = vlan_iface;
		ath12k_ppe_ds_attach_vlan_vif_link(ahvif->vlan_iface, ahvif->dp_vif.ppe_vp_num);
		goto exit;
	}

	/* Allocate Default Queue now and reassign during actual vdev create */
	vif->cab_queue = ATH12K_HW_DEFAULT_QUEUE;
	for (i = 0; i < ARRAY_SIZE(vif->hw_queue); i++)
		vif->hw_queue[i] = ATH12K_HW_DEFAULT_QUEUE;

	vif->driver_flags |= IEEE80211_VIF_SUPPORTS_UAPSD;
	if (ath12k_frame_mode == ATH12K_HW_TXRX_ETHERNET) {
		vif->offload_flags |= IEEE80211_OFFLOAD_ENCAP_4ADDR;

		if (vif->type != NL80211_IFTYPE_AP_VLAN)
			vif->offload_flags |= IEEE80211_OFFLOAD_ENCAP_MCAST;
	}

	ath12k_dbg(NULL, ATH12K_DBG_MAC, "Add interface vif address:%pM netdev:%s",
		   vif->addr, wdev->netdev->name);

	/* Defer vdev creation until assign_chanctx or hw_scan is initiated as driver
	 * will not know if this interface is an ML vif at this point.
	 */
exit:
	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_add_interface);

static void ath12k_mac_vif_unref(struct ath12k_dp *dp, struct ieee80211_vif *vif)
{
	struct ath12k_tx_desc_info *tx_desc_info;
	struct ath12k_skb_cb *skb_cb;
	struct sk_buff *skb;
	u32 tx_spt_page;
	int i, j, k;

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		spin_lock_bh(&dp->tx_desc_lock[i]);

		for (j = 0; j < ATH12K_TX_SPT_PAGES_PER_POOL; j++) {
			tx_spt_page = j + i * ATH12K_TX_SPT_PAGES_PER_POOL;
			tx_desc_info = dp->txbaddr[tx_spt_page];

			for (k = 0; k < ATH12K_MAX_SPT_ENTRIES; k++) {
				if (!tx_desc_info[k].in_use)
					continue;

				skb = tx_desc_info[k].skb;
				if (!skb)
					continue;

				skb_cb = ATH12K_SKB_CB(skb);
				if (skb_cb->vif == vif)
					skb_cb->vif = NULL;
			}
		}

		spin_unlock_bh(&dp->tx_desc_lock[i]);
	}
}

bool ath12k_mac_validate_active_radio_count(struct ath12k_hw *ah)
{
	struct ath12k *ar;
	int i, active_radio = 0;

	for_each_ar(ah, ar, i) {
		if (ar->allocated_vdev_map) {
			active_radio++;

		if (active_radio > 1)
			return false;
		}
	}

	return true;
}

int ath12k_mac_pdev_suspend(struct ath12k *ar)
{
	unsigned long time_left;
	int ret = 0;

	if (!test_bit(WMI_SERVICE_PDEV_SUSPEND_EVENT_SUPPORT, ar->ab->wmi_ab.svc_map))
		goto exit;

	reinit_completion(&ar->suspend);
	ret = ath12k_wmi_pdev_suspend(ar, WMI_PDEV_SUSPEND_AND_DISABLE_INTR,
				      ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab, "failed to send wmi suspend command %d\n", ret);
		goto exit;
	}
	time_left = wait_for_completion_timeout(&ar->suspend,
						ATH12K_PDEV_SUSPEND_TIMEOUT);
	if (!time_left) {
		ath12k_err(ar->ab, "timeout in receiving pdev suspend response %d\n",
			   ar->pdev->pdev_id);
		ret = -ETIMEDOUT;
		goto exit;
	}
exit:
	return ret;
}

static int ath12k_mac_vdev_delete(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct ath12k_dp_link_vif *dp_link_vif;
	struct ath12k_base *ab = ar->ab;
	unsigned long time_left;
	int ret = -1;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	reinit_completion(&ar->vdev_delete_done);

	ath12k_vendor_link_state_update(ar->pdev_idx, ar->ab, arvif,
					ATH12K_VENDOR_LINK_STATE_REMOVED);

	if (unlikely(test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags)))
		goto err_vdev_del;

	ret = ath12k_wmi_vdev_delete(ar, arvif->vdev_id);
	if (ret) {
		ath12k_warn(ab, "failed to delete WMI vdev %d: %d\n",
			    arvif->vdev_id, ret);
		goto err_vdev_del;
	}

	time_left = wait_for_completion_timeout(&ar->vdev_delete_done,
						ATH12K_VDEV_DELETE_TIMEOUT_HZ);
	if (time_left == 0) {
		ath12k_warn(ab, "Timeout in receiving vdev delete response %d\n",
			    arvif->vdev_id);
		goto err_vdev_del;
	}

	spin_lock_bh(&ar->ab->base_lock);
	ab->free_vdev_map |= 1LL << arvif->vdev_id;
	spin_unlock_bh(&ar->ab->base_lock);

	ar->allocated_vdev_map &= ~(1LL << arvif->vdev_id);
	ar->free_map_id |= 1 << arvif->map_id;
	if (!ath12k_mac_is_bridge_vdev(arvif)) {
		WARN_ON(!ar->num_created_vdevs);
		ar->num_created_vdevs--;
	} else {
		WARN_ON(!ar->num_created_bridge_vdevs);
		ar->num_created_bridge_vdevs--;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ar->monitor_vdev_id = -1;
		ar->monitor_vdev_created = false;
	} else if (ahvif->vdev_type != WMI_VDEV_TYPE_STA) {
		ar->dp.stats.telemetry_stats.sta_vap_exist--;
		ath12k_dbg(ab, ATH12K_DBG_MAC, "vdev %pM deleted, vdev_id %d\n",
		   vif->addr, arvif->vdev_id);
	}

err_vdev_del:
	spin_lock_bh(&ar->data_lock);
	if (!list_empty(&ar->arvifs))
		list_del(&arvif->list);
	spin_unlock_bh(&ar->data_lock);

	ath12k_peer_cleanup(ar, arvif->vdev_id);
	ath12k_ahvif_put_link_cache(ahvif, arvif->link_id);

	spin_lock_bh(&ar->data_lock);
	idr_for_each(&ar->txmgmt_idr,
		     ath12k_mac_vif_txmgmt_idr_remove, vif);
	spin_unlock_bh(&ar->data_lock);

	ath12k_mac_vif_unref(ath12k_ab_to_dp(ab), vif);
	dp_link_vif = &ahvif->dp_vif.dp_link_vif[arvif->link_id];
	ath12k_dp_tx_put_bank_profile(ath12k_ab_to_dp(ab), dp_link_vif->bank_id);

	if (arvif->splitphy_ds_bank_id != DP_INVALID_BANK_ID)
		ath12k_dp_tx_put_bank_profile(ath12k_ab_to_dp(ab),
					      arvif->splitphy_ds_bank_id);

	arvif->key_cipher = INVALID_CIPHER;

	/* Recalc txpower for remaining vdev */
	ath12k_mac_txpower_recalc(ar);

	ahvif->device_bitmap &= ~BIT(ar->ab->wsi_info.index);

	if (!ar->allocated_vdev_map && !arvif->is_scan_vif) {
		if (ath12k_erp_get_sm_state() == ATH12K_ERP_ENTER_COMPLETE) {
			if (ath12k_mac_validate_active_radio_count(ar->ah))
				ath12k_core_cleanup_power_down_q6(ab->ag);
		}
	}

	/* TODO: recal traffic pause state based on the available vdevs */
	arvif->is_created = false;
	arvif->is_scan_vif = false;
	arvif->ar = NULL;

	wiphy_work_cancel(ath12k_ar_to_hw(ar)->wiphy,
			  &arvif->update_bcn_tx_status_work);

	return ret;
}

void ath12k_mac_op_remove_interface(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif), *vlan_master_ahvif = NULL;
	struct ieee80211_vif *vlan_master_vif = NULL;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	u8 link_id;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	if (vif->type == NL80211_IFTYPE_AP_VLAN) {
		if (!ahvif->vlan_iface) {
			pr_err("vlan_iface is null\n");
			goto free_tstats;
		}
		vlan_master_vif = ahvif->vlan_iface->parent_vif;
		vlan_master_ahvif = ath12k_vif_to_ahvif(vlan_master_vif);
	} else {
		vlan_master_ahvif = ahvif;
	}
	for (link_id = 0; link_id < ATH12K_NUM_MAX_LINKS; link_id++) {
		/* if we cached some config but never received assign chanctx,
		 * free the allocated cache.
		 */
		ath12k_ahvif_put_link_cache(ahvif, link_id);
		arvif = wiphy_dereference(hw->wiphy, vlan_master_ahvif->link[link_id]);
		if (!arvif || !arvif->is_created)
			continue;

		if (vif->type == NL80211_IFTYPE_AP_VLAN) {
			ath12k_ppeds_detach_link_apvlan_vif(arvif, ahvif->vlan_iface, link_id);
			continue;
		}
		ar = arvif->ar;

		if (!ar)
			continue;

		/* Scan abortion is in progress since before this, cancel_hw_scan()
		 * is expected to be executed. Since link is anyways going to be removed
		 * now, just cancel the worker and send the scan aborted to user space
		 */
		if (ar->scan.arvif == arvif) {
			if (arvif->is_started) {
				ret = ath12k_mac_vdev_stop(arvif);
				if (ret) {
					ath12k_warn(ar->ab, "failed to stop vdev %d: %d\n",
						    arvif->vdev_id, ret);
				}
				arvif->is_started = false;
				ar->scan.arvif = NULL;
				arvif->is_scan_vif = false;
			}
			wiphy_work_cancel(hw->wiphy, &ar->scan.vdev_clean_wk);

			spin_lock_bh(&ar->data_lock);
			ar->scan.arvif = NULL;
			if (!ar->scan.is_roc) {
				struct cfg80211_scan_info info = {
					.aborted = true,
				};

				ath12k_mac_scan_send_complete(ar, &info);
			}

			ar->scan.state = ATH12K_SCAN_IDLE;
			ar->scan_channel = NULL;
			ar->scan.roc_freq = 0;
			spin_unlock_bh(&ar->data_lock);
		}

		if (arvif->is_scan_vif && arvif->is_started) {
			ret = ath12k_mac_vdev_stop(arvif);
			if (ret) {
				ath12k_warn(ar->ab, "failed to stop vdev %d: %d\n",
					    arvif->vdev_id, ret);
				goto free_vlan_iface;
			}
			arvif->is_started = false;
			arvif->is_scan_vif = false;
		}

		ath12k_mac_remove_link_interface(hw, arvif);
		ath12k_mac_unassign_link_vif(arvif);
	}

free_vlan_iface:
	kfree(ahvif->vlan_iface);
	ahvif->vlan_iface = NULL;
free_tstats:
	free_percpu(ahvif->tstats);
	ahvif->tstats = NULL;

}
EXPORT_SYMBOL(ath12k_mac_op_remove_interface);

/* FIXME: Has to be verified. */
#define SUPPORTED_FILTERS			\
	(FIF_ALLMULTI |				\
	FIF_CONTROL |				\
	FIF_PSPOLL |				\
	FIF_OTHER_BSS |				\
	FIF_BCN_PRBRESP_PROMISC |		\
	FIF_PROBE_REQ |				\
	FIF_FCSFAIL)

void ath12k_mac_op_configure_filter(struct ieee80211_hw *hw,
				    unsigned int changed_flags,
				    unsigned int *total_flags,
				    u64 multicast)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;

	lockdep_assert_wiphy(hw->wiphy);

	ar = ath12k_ah_to_ar(ah, 0);

	*total_flags &= SUPPORTED_FILTERS;
	ar->filter_flags = *total_flags;
}
EXPORT_SYMBOL(ath12k_mac_op_configure_filter);

int ath12k_mac_op_get_antenna(struct ieee80211_hw *hw, u32 *tx_ant, u32 *rx_ant,
			      u8 radio_id)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	int antennas_rx = 0, antennas_tx = 0;
	struct ath12k *ar;
	int i;

	lockdep_assert_wiphy(hw->wiphy);

	for_each_ar(ah, ar, i) {
		if ((radio_id != 255) && (radio_id != i))
			continue;
		antennas_rx = max_t(u32, antennas_rx, ar->cfg_rx_chainmask);
		antennas_tx = max_t(u32, antennas_tx, ar->cfg_tx_chainmask);

		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mac pdev %u freq limits %u->%u MHz\n",
				 ar->pdev->pdev_id, ar->chan_info.low_freq,
				 ar->chan_info.high_freq);
	}

	*tx_ant = antennas_tx;
	*rx_ant = antennas_rx;

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_get_antenna);

int ath12k_mac_op_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant,
			      u8 radio_id, bool is_dynamic)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;
	int ret = 0;
	int i;

	lockdep_assert_wiphy(hw->wiphy);

	for_each_ar(ah, ar, i) {
		if ((radio_id != 255) && (radio_id != i))
			continue;
		ret = __ath12k_set_antenna(ar, tx_ant, rx_ant, is_dynamic);
		if (ret)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_set_antenna);

static int ath12k_mac_ampdu_action(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_ampdu_params *params,
				   u8 link_id)
{
	struct ath12k *ar;
	int ret = -EINVAL;

	lockdep_assert_wiphy(hw->wiphy);

	ar = ath12k_get_ar_by_vif(hw, vif, link_id);
	if (!ar)
		return -EINVAL;

	if (unlikely(test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)))
		return -ESHUTDOWN;

	if (params->sta->mlo &&
	    (test_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &ar->ab->dev_flags)))
		return 0;

	switch (params->action) {
	case IEEE80211_AMPDU_RX_START:
		ret = ath12k_dp_rx_ampdu_start(ar, params, link_id);
		break;
	case IEEE80211_AMPDU_RX_STOP:
		ret = ath12k_dp_rx_ampdu_stop(ar, params, link_id);
		break;
	case IEEE80211_AMPDU_TX_START:
	case IEEE80211_AMPDU_TX_STOP_CONT:
	case IEEE80211_AMPDU_TX_STOP_FLUSH:
	case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
	case IEEE80211_AMPDU_TX_OPERATIONAL:
		/* Tx A-MPDU aggregation offloaded to hw/fw so deny mac80211
		 * Tx aggregation requests.
		 */
		ret = -EOPNOTSUPP;
		break;
	}

	if (ret)
		ath12k_warn(ar->ab, "unable to perform ampdu action %d for vif %pM link %u ret %d\n",
			    params->action, vif->addr, link_id, ret);

	return ret;
}

int ath12k_mac_op_ampdu_action(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ieee80211_ampdu_params *params)
{
	struct ieee80211_sta *sta = params->sta;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	unsigned long links_map = ahsta->links_map;
	int ret = -EINVAL;
	u8 link_id;

	lockdep_assert_wiphy(hw->wiphy);

	if (WARN_ON(!links_map))
		return ret;

	for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		ret = ath12k_mac_ampdu_action(hw, vif, params, link_id);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_ampdu_action);

int ath12k_mac_mlo_standby_teardown(struct ath12k_hw *ah)
{
	struct ath12k_hw_group *ag = ath12k_ah_to_ag(ah);
	struct ath12k *ar;
	int ret = 0, i;
	bool erp_standby_mode;

	lockdep_assert_wiphy(ah->hw->wiphy);
	for_each_ar(ah, ar, i) {
		if (!ar->teardown_complete_event) {
			if (ar->allocated_vdev_map)
				erp_standby_mode = true;
			else
				erp_standby_mode = false;

			ret = ath12k_wmi_mlo_teardown(ar, !ag->trigger_umac_reset,
						      WMI_MLO_TEARDOWN_REASON_STANDBY_DOWN,
						      erp_standby_mode);
			if (ret) {
				ath12k_err(ar->ab, "failed to teardown MLO for pdev_idx  %d: %d\n",
					   ar->pdev_idx, ret);
				return ret;
			}

			ag->trigger_umac_reset = true;
		}
	}

	return ret;
}

int ath12k_mac_op_add_chanctx(struct ieee80211_hw *hw,
			      struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ath12k_ah_to_ag(ah);
	struct ath12k *ar;
	struct ath12k_base *ab;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	if (ath12k_check_erp_power_down(ag)) {
		ret = ath12k_core_power_up(ag);
		if (ret)
			return ret;
	}

	ar = ath12k_get_ar_by_ctx(hw, ctx);
	if (!ar)
		return -EINVAL;

	ab = ar->ab;

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac chanctx add freq %u width %d ptr %p\n",
			ctx->def.chan->center_freq, ctx->def.width, ctx);

	spin_lock_bh(&ar->data_lock);
	/* TODO: In case of multiple channel context, populate rx_channel from
	 * Rx PPDU desc information.
	 */
	ar->rx_channel = ctx->def.chan;
	spin_unlock_bh(&ar->data_lock);
	ar->chan_tx_pwr = ATH12K_PDEV_TX_POWER_INVALID;

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_add_chanctx);

void ath12k_mac_op_remove_chanctx(struct ieee80211_hw *hw,
				  struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k *ar;
	struct ath12k_base *ab;

	lockdep_assert_wiphy(hw->wiphy);

	ar = ath12k_get_ar_by_ctx(hw, ctx);
	if (!ar)
		return;

	ab = ar->ab;

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac chanctx remove freq %u width %d ptr %p\n",
			ctx->def.chan->center_freq, ctx->def.width, ctx);

	spin_lock_bh(&ar->data_lock);
	/* TODO: In case of there is one more channel context left, populate
	 * rx_channel with the channel of that remaining channel context.
	 */
	ar->rx_channel = NULL;
	spin_unlock_bh(&ar->data_lock);
	ar->chan_tx_pwr = ATH12K_PDEV_TX_POWER_INVALID;
}
EXPORT_SYMBOL(ath12k_mac_op_remove_chanctx);

static enum wmi_phy_mode
ath12k_mac_check_down_grade_phy_mode(struct ath12k *ar,
				     enum wmi_phy_mode mode,
				     enum nl80211_band band,
				     enum nl80211_iftype type)
{
	struct ieee80211_sta_eht_cap *eht_cap = NULL;
	enum wmi_phy_mode down_mode;
	int n = ar->mac.sbands[band].n_iftype_data;
	int i;
	struct ieee80211_sband_iftype_data *data;

	if (mode < MODE_11BE_EHT20)
		return mode;

	data = ar->mac.iftype[band];
	for (i = 0; i < n; i++) {
		if (data[i].types_mask & BIT(type)) {
			eht_cap = &data[i].eht_cap;
			break;
		}
	}

	if (eht_cap && eht_cap->has_eht)
		return mode;

	switch (mode) {
	case MODE_11BE_EHT20:
		down_mode = MODE_11AX_HE20;
		break;
	case MODE_11BE_EHT40:
		down_mode = MODE_11AX_HE40;
		break;
	case MODE_11BE_EHT80:
		down_mode = MODE_11AX_HE80;
		break;
	case MODE_11BE_EHT80_80:
		down_mode = MODE_11AX_HE80_80;
		break;
	case MODE_11BE_EHT160:
	case MODE_11BE_EHT160_160:
	case MODE_11BE_EHT320:
		down_mode = MODE_11AX_HE160;
		break;
	case MODE_11BE_EHT20_2G:
		down_mode = MODE_11AX_HE20_2G;
		break;
	case MODE_11BE_EHT40_2G:
		down_mode = MODE_11AX_HE40_2G;
		break;
	default:
		down_mode = mode;
		break;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac vdev start phymode %s downgrade to %s\n",
			ath12k_mac_phymode_str(mode),
			ath12k_mac_phymode_str(down_mode));

	return down_mode;
}

static void
ath12k_mac_mlo_get_vdev_args(struct ath12k_link_vif *arvif,
			     struct wmi_ml_arg *ml_arg)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct wmi_ml_partner_info *partner_info;
	struct ieee80211_bss_conf *link_conf;
	struct ath12k_link_vif *arvif_p;
	unsigned long links;
	u8 link_id;

	lockdep_assert_wiphy(ahvif->ah->hw->wiphy);

	if (!ath12k_mac_is_ml_arvif(arvif))
		return;

	if (hweight16(ahvif->vif->valid_links) > ATH12K_WMI_MLO_MAX_LINKS)
		return;

	ml_arg->enabled = true;

	/* Driver always add a new link via VDEV START, FW takes
	 * care of internally adding this link to existing
	 * link vdevs which are advertised as partners below
	 */
	ml_arg->link_add = true;
	ml_arg->ieee_link_id = arvif->link_id;
	ml_arg->mlo_bridge_link = ath12k_mac_is_bridge_vdev(arvif);
	partner_info = ml_arg->partner_info;

	links = ahvif->links_map;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		if (ATH12K_SCAN_LINKS_MASK & BIT(link_id))
			continue;

		arvif_p = wiphy_dereference(ahvif->ah->hw->wiphy, ahvif->link[link_id]);

		if (WARN_ON(!arvif_p))
			continue;

		if (arvif == arvif_p)
			continue;

		if (!arvif_p->is_started)
			continue;

		if (ath12k_mac_is_bridge_vdev(arvif_p)) {
			ether_addr_copy(partner_info->addr, arvif_p->bssid);
		} else {
			link_conf = wiphy_dereference(ahvif->ah->hw->wiphy,
						      ahvif->vif->link_conf[arvif_p->link_id]);
			if (!link_conf)
				continue;
			ether_addr_copy(partner_info->addr, link_conf->addr);
		}

		partner_info->vdev_id = arvif_p->vdev_id;
		partner_info->hw_link_id = arvif_p->ar->pdev->hw_link_id;
		partner_info->logical_link_idx = arvif_p->link_id;
		partner_info->mlo_bridge_link = ath12k_mac_is_bridge_vdev(arvif_p);
		ml_arg->num_partner_links++;
		partner_info++;
	}
}

static bool
ath12k_mac_check_fixed_rate_settings_for_mumimo(struct ath12k_link_vif *arvif,
						const u16 *vht_mcs_mask,
						const u16 *he_mcs_mask)
{
	struct ieee80211_he_cap_elem he_cap_elem = {0};
	struct ieee80211_bss_conf *link_conf;
	struct ath12k *ar = arvif->ar;
	int nss_idx;
	int he_nss;
	int vht_nss;

	rcu_read_lock();

	link_conf = rcu_dereference(arvif->ahvif->vif->link_conf[arvif->link_id]);

	if (!link_conf) {
		rcu_read_unlock();
		return -EINVAL;
	}

	vht_nss =  ath12k_mac_max_vht_nss(vht_mcs_mask);

	if (vht_nss != 1) {
		for (nss_idx = vht_nss-1; nss_idx >= 0; nss_idx--) {
			if (vht_mcs_mask[nss_idx])
				continue;

			if (arvif->vht_cap & IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE) {
				rcu_read_unlock();
				ath12k_warn(ar->ab, "vht fixed NSS rate is allowed only when MU MIMO is disabled\n");
				return false;
			}
		}
	}

	if (!link_conf->he_support) {
		rcu_read_unlock();
		return true;
	}

	he_nss =  ath12k_mac_max_he_nss(he_mcs_mask);

	if (he_nss == 1) {
		rcu_read_unlock();
		return true;
	}

	memcpy(&he_cap_elem, &link_conf->he_cap_elem, sizeof(he_cap_elem));

	for (nss_idx = he_nss-1; nss_idx >= 0; nss_idx--) {
		if (he_mcs_mask[nss_idx])
			continue;

		if ((he_cap_elem.phy_cap_info[2] & IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO) ||
		     (he_cap_elem.phy_cap_info[4] & IEEE80211_HE_PHY_CAP4_MU_BEAMFORMER)) {
			rcu_read_unlock();
			ath12k_warn(ar->ab, "he fixed NSS rate is allowed only when MU MIMO is disabled\n");
			return false;
		}
	}
	rcu_read_unlock();
	return true;
}

void ath12k_agile_cac_abort_work(struct wiphy *wiphy,
				 struct wiphy_work *work)
{
        struct ath12k *ar = container_of(work, struct ath12k,
                                         agile_cac_abort_wq);
	struct ath12k_link_vif *arvif;
        struct ath12k_vif *ahvif;
        bool arvif_found = false;
        int ret = 0;

        list_for_each_entry(arvif, &ar->arvifs, list) {
                ahvif = arvif->ahvif;
                if (arvif->is_started &&
                    ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
                        arvif_found = true;
                        break;
                }
        }

        if (!arvif_found)
                goto err;

        ret = ath12k_wmi_vdev_adfs_ocac_abort_cmd_send(ar, arvif->vdev_id);

        if (!ret) {
                memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
                ar->agile_chandef.chan = NULL;
        } else
                goto err;

err:
        ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
                           "ADFS state can't be reset (ret=%d)\n",
                           ret);
}

void ath12k_mac_background_dfs_event(struct ath12k *ar,
				     enum ath12k_background_dfs_events ev)
{
	if (ev == ATH12K_BGDFS_RADAR) {
		cfg80211_background_radar_event(ar->ah->hw->wiphy, &ar->agile_chandef, GFP_ATOMIC);
		wiphy_work_queue(ar->ah->hw->wiphy, &ar->agile_cac_abort_wq);
	} else if (ev == ATH12K_BGDFS_ABORT) {
		cfg80211_background_cac_abort(ar->ah->hw->wiphy);
	}
}

static int
ath12k_mac_vdev_config_after_start(struct ath12k_link_vif *arvif,
				   const struct cfg80211_chan_def *chandef)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_chanctx_conf *chanctx = &arvif->chanctx;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(ahvif);
	struct wireless_dev *wdev =ieee80211_vif_to_wdev(vif);
	struct ath12k_base *ab = ar->ab;
	unsigned int dfs_cac_time;
	int ret;

	if (ath12k_mac_is_bridge_vdev(arvif))
		return 0;

	if (ar->supports_6ghz && chandef->chan->band == NL80211_BAND_6GHZ &&
            (ahvif->vdev_type == WMI_VDEV_TYPE_STA || ahvif->vdev_type == WMI_VDEV_TYPE_AP) &&
            test_bit(WMI_TLV_SERVICE_EXT_TPC_REG_SUPPORT, ar->ab->wmi_ab.svc_map)) {
		if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
			ath12k_mac_parse_tx_pwr_env(ar, arvif);

		if (!chanctx) {
			ath12k_err(ar->ab, "channel context is NULL");
			return -ENOLINK;
		}

		ath12k_mac_fill_reg_tpc(ar, wdev, arvif, chanctx);
		ath12k_wmi_send_vdev_set_tpc_power(ar, arvif->vdev_id, &arvif->reg_tpc_info);
	}

	/* Enable CAC Running Flag in the driver by checking all sub-channel's DFS
	 * state as NL80211_DFS_USABLE which indicates CAC needs to be
	 * done before channel usage. This flag is used to drop rx packets.
	 * during CAC.
	 */
	/* TODO: Set the flag for other interface types as required */
	if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP && arvif->chanctx.radar_enabled &&
	    cfg80211_chandef_dfs_usable(ar->ah->hw->wiphy, chandef)) {
		set_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags);
		dfs_cac_time = cfg80211_chandef_dfs_cac_time(ar->ah->hw->wiphy, chandef,
							     false, false);

		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "CAC started dfs_cac_time %u center_freq %d center_freq1 %d for vdev %d\n",
			   dfs_cac_time, chandef->chan->center_freq, chandef->center_freq1,
			   arvif->vdev_id);
	}

	ret = ath12k_mac_set_txbf_conf(arvif);
	if (ret)
		ath12k_warn(ab, "failed to set txbf conf for vdev %d: %d\n",
			    arvif->vdev_id, ret);

	ret = ath12k_mac_set_6g_nonht_dup_conf(arvif, chandef);
	if (ret)
		ath12k_warn(ab, "failed to set 6G non-ht dup conf for vdev %d: %d\n",
		            arvif->vdev_id, ret);
	 /* In case of ADFS, we have to abort ongoing backgrorund CAC */
	if ((ar->pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP) &&
	    test_bit(ar->cfg_rx_chainmask, &ar->pdev->cap.adfs_chain_mask) &&
	    ar->agile_chandef.chan) {
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "Aborting ongoing Agile DFS on freq %d",
			   ar->agile_chandef.chan->center_freq);
		ret = ath12k_wmi_vdev_adfs_ocac_abort_cmd_send(ar,arvif->vdev_id);
		if (!ret) {
			memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
			ar->agile_chandef.chan = NULL;
			ath12k_mac_background_dfs_event(ar, ATH12K_BGDFS_ABORT);
		} else {
			ath12k_warn(ab, "failed to abort agile CAC for vdev %d",
				    arvif->vdev_id);
		}
	}

	return ret;
}

static struct ieee80211_channel *ath12k_mac_get_a_valid_channel(struct ath12k *ar)
{
	struct ieee80211_supported_band *sband;
	enum nl80211_band band;
	u32 freq_low, freq_high;
	int chn;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!(ar->mac.sbands[band].channels))
			continue;
		sband = &ar->mac.sbands[band];
		freq_low = ar->chan_info.low_freq;
		freq_high = ar->chan_info.high_freq;

		for (chn = 0; chn < sband->n_channels; chn++) {
			if (sband->channels[chn].flags &
			    IEEE80211_CHAN_DISABLED)
				continue;

			if (sband->channels[chn].center_freq <
			    ar->chan_info.low_freq ||
			    sband->channels[chn].center_freq >
			    ar->chan_info.high_freq)
				continue;
			return &sband->channels[chn];
		}
	}
	return NULL;
}

static int
ath12k_mac_vdev_start_restart(struct ath12k_link_vif *arvif,
			      struct ieee80211_chanctx_conf *ctx,
			      bool restart)
{
	const struct cfg80211_chan_def* chandef=ctx ? &ctx->def : NULL;
	struct ath12k_vif* ahvif=arvif->ahvif;
	struct ath12k* ar=arvif->ar;
	struct ieee80211_hw* hw=ath12k_ar_to_hw(ar);
	struct wmi_vdev_start_req_arg arg={};
	struct ieee80211_bss_conf* link_conf;
	s16 punct_bitmap=arvif->punct_bitmap;
	struct ieee80211_channel* channel;
	struct ath12k_base* ab=ar->ab;
	bool is_bridge_vdev;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

	if (!is_bridge_vdev) {
		if (!chandef) {
			ath12k_warn(ar->ab,
				    "Chandef is not valid for vif %pM link %u\n",
				    ahvif->vif->addr, arvif->link_id);
			return -EINVAL;
		}

		link_conf = ath12k_mac_get_link_bss_conf(arvif);
		if (!link_conf) {
			ath12k_warn(ar->ab, "unable to access bss link conf in vdev start for vif %pM link %u\n",
				    ahvif->vif->addr, arvif->link_id);
			return -ENOLINK;
		}
	}

	reinit_completion(&ar->vdev_setup_done);

	arg.vdev_id = arvif->vdev_id;
	arg.dtim_period = arvif->dtim_period;
	arg.bcn_intval = arvif->beacon_interval;

	if (!chandef && is_bridge_vdev) {
		channel = ath12k_mac_get_a_valid_channel(ar);
		if (WARN_ON(!channel))
			return -ENODATA;
		arg.freq = channel->center_freq;
		arg.band_center_freq1 = channel->center_freq;
		arg.band_center_freq2 = channel->center_freq;
		arg.mode =
			ath12k_phymodes[channel->band][NL80211_CHAN_WIDTH_20];
		arg.min_power = 0;
		arg.max_power = channel->max_power;
		arg.max_reg_power = channel->max_reg_power;
		arg.max_antenna_gain = channel->max_antenna_gain;
	} else {
		arg.freq = chandef->chan->center_freq;
		arg.band_center_freq1 = chandef->center_freq1;
		arg.band_center_freq2 = chandef->center_freq2;
		arg.mode = ath12k_phymodes[chandef->chan->band][chandef->width];

		arg.mode = ath12k_mac_check_down_grade_phy_mode(ar, arg.mode,
								chandef->chan->band,
								ahvif->vif->type);
		arg.min_power = 0;
		arg.max_power = chandef->chan->max_power;
		arg.max_reg_power = chandef->chan->max_reg_power;
		arg.max_antenna_gain = chandef->chan->max_antenna_gain;
		if (!is_bridge_vdev &&
		    test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map) &&
		    cfg80211_chandef_device_present(chandef)) {
			arg.width_device = chandef->width_device;
			arg.center_freq_device = chandef->center_freq_device;
			punct_bitmap = ath12k_mac_set_punct_bitmap_device(chandef->chan->center_freq,
									 chandef->width_device,
									 chandef->center_freq_device,
									 punct_bitmap);
		}
	}

	arg.punct_bitmap = ~punct_bitmap;
	arg.pref_tx_streams = ar->num_tx_chains;
	arg.pref_rx_streams = ar->num_rx_chains;

	if (is_bridge_vdev)
		arg.mbssid_flags = 0;
	else
		arg.mbssid_flags = WMI_VDEV_MBSSID_FLAGS_NON_MBSSID_AP;

	arg.mbssid_tx_vdev_id = 0;
	if (test_bit(WMI_TLV_SERVICE_MBSS_PARAM_IN_VDEV_START_SUPPORT,
		     ar->ab->wmi_ab.svc_map)) {
		ret = ath12k_mac_setup_vdev_params_mbssid(arvif,
							  &arg.mbssid_flags,
							  &arg.mbssid_tx_vdev_id);
		if (ret)
			return ret;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
		arg.ssid = ahvif->u.ap.ssid;
		arg.ssid_len = ahvif->u.ap.ssid_len;
		arg.hidden_ssid = ahvif->u.ap.hidden_ssid;

		/* For now allow DFS in AP mode for vdevs except
		 * bridge vdev.
		 */
		if (chandef && !is_bridge_vdev) {
			arg.chan_radar =
				!!(chandef->chan->flags & IEEE80211_CHAN_RADAR);
			arg.freq2_radar = ctx->radar_enabled;
		}

		arg.passive = arg.chan_radar;

		spin_lock_bh(&ab->base_lock);
		arg.regdomain = ar->ab->dfs_region;
		spin_unlock_bh(&ab->base_lock);

		/* TODO: Notify if secondary 80Mhz also needs radar detection */
	}

	if (is_bridge_vdev)
		arg.passive = IEEE80211_CHAN_NO_IR;
	else
		arg.passive |= !!(chandef->chan->flags & IEEE80211_CHAN_NO_IR);

	if (!restart)
		ath12k_mac_mlo_get_vdev_args(arvif, &arg.ml);

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "mac vdev %d start center_freq %d phymode %s punct_bitmap 0x%x\n",
		   arg.vdev_id, arg.freq,
		   ath12k_mac_phymode_str(arg.mode), arg.punct_bitmap);

	ret = ath12k_wmi_vdev_start(ar, &arg, restart);
	if (ret) {
		ath12k_warn(ar->ab, "failed to %s WMI vdev %i\n",
			    restart ? "restart" : "start", arg.vdev_id);
		return ret;
	}

	ret = ath12k_mac_vdev_setup_sync(ar);
	if (ret) {
		ath12k_warn(ab, "failed to synchronize setup for vdev %i %s: %d\n",
			    arg.vdev_id, restart ? "restart" : "start", ret);
		return ret;
	}

	ar->num_started_vdevs++;
	ath12k_dbg(ab, ATH12K_DBG_MAC, "vdev %pM started, vdev_id %d\n",
		   arvif->bssid, arvif->vdev_id);

	/* For scan vif, STA related configs are not needed */
	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA && arvif->is_scan_vif)
		return 0;

	if (chandef) {
		ret = ath12k_mac_vdev_config_after_start(arvif, chandef);
		if (ret)
			ath12k_warn(ab, "failed to configure vdev %d after %s: %d\n",
				    arvif->vdev_id,
				    restart ? "restart" : "start", ret);
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		ar->dp.stats.telemetry_stats.sta_vap_exist = 1;

	return 0;
}

int ath12k_mac_vdev_start(struct ath12k_link_vif *arvif,
				 struct ieee80211_chanctx_conf *ctx)
{
	return ath12k_mac_vdev_start_restart(arvif, ctx, false);
}

static int ath12k_mac_vdev_restart(struct ath12k_link_vif *arvif,
				   struct ieee80211_chanctx_conf *ctx,
				   bool pseudo_restart)
{
	struct ath12k_base *ab = arvif->ar->ab;
	int ret;

	if(!pseudo_restart)
		return ath12k_mac_vdev_start_restart(arvif, ctx, true);

	ret = ath12k_mac_vdev_stop(arvif);
	if (ret) {
		ath12k_warn(ab, "failed to stop vdev %d: %d during restart\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	ret = ath12k_mac_vdev_start(arvif, ctx);
	if (ret) {
		ath12k_warn(ab, "failed to start vdev %d: %d during restart\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	return ret;
}

struct ath12k_mac_change_chanctx_arg {
	struct ieee80211_chanctx_conf *ctx;
	struct ieee80211_chanctx_conf *new_ctx;
	struct ieee80211_vif_chanctx_switch *vifs;
	u8 *vifs_bridge_link_id;
	int n_vifs;
	int next_vif;
	bool set_csa_active;
	struct ath12k *ar;
};

static void
ath12k_mac_change_chanctx_cnt_iter(void *data, u8 *mac,
				   struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_mac_change_chanctx_arg *arg = data;
	struct ieee80211_bss_conf *link_conf;
	struct ath12k_link_vif *arvif;
	unsigned long links_map;
	u8 link_id;

	lockdep_assert_wiphy(ahvif->ah->hw->wiphy);

	links_map = ahvif->links_map;
	for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		if (ATH12K_SCAN_LINKS_MASK & BIT(link_id))
			continue;

		arvif = wiphy_dereference(ahvif->ah->hw->wiphy, ahvif->link[link_id]);
		if (WARN_ON(!arvif))
			continue;

		if (arvif->ar != arg->ar)
			continue;

		if (!ath12k_mac_is_bridge_vdev(arvif)) {
			link_conf = wiphy_dereference(ahvif->ah->hw->wiphy,
						      vif->link_conf[link_id]);

			/* For AP + STA/Monitor mode on 5G DFS channels,
			 * start_ap will be coming post CAC and AP, STA vif
			 * won't have link_conf assigned until CAC is completed
			 * This is expected and no WARN_ON required for them
			 */
			if (cfg80211_chandef_dfs_cac_time(ahvif->ah->hw->wiphy,
							  &arg->ctx->def, false,
							  false)) {
				if (!link_conf)
					continue;
			} else if (WARN_ON(!link_conf))
				continue;

			if (rcu_access_pointer(link_conf->chanctx_conf) != arg->ctx)
				continue;

			if (arg->set_csa_active && link_conf->csa_active)
				arg->ar->csa_active_cnt++;
		} else {
			if (arvif->ar != arg->ar)
				continue;
		}

		arg->n_vifs++;
	}
}

static void
ath12k_mac_change_chanctx_fill_iter(void *data, u8 *mac,
				    struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_mac_change_chanctx_arg *arg = data;
	struct ieee80211_bss_conf *link_conf;
	struct ieee80211_chanctx_conf *ctx;
	struct ath12k_link_vif *arvif;
	unsigned long links_map;
	u8 link_id, bridge_link_id;

	lockdep_assert_wiphy(ahvif->ah->hw->wiphy);

	links_map = ahvif->links_map;
	for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
		if (ATH12K_SCAN_LINKS_MASK & BIT(link_id))
			continue;

		arvif = wiphy_dereference(ahvif->ah->hw->wiphy, ahvif->link[link_id]);
		if (WARN_ON(!arvif))
			continue;

		if (arvif->ar != arg->ar)
			continue;

		if (!ath12k_mac_is_bridge_vdev(arvif)) {
			link_conf = wiphy_dereference(ahvif->ah->hw->wiphy,
						      vif->link_conf[arvif->link_id]);

			/* For AP + STA/Monitor mode on 5G DFS channels,
			 * start_ap will be coming post CAC and AP, STA vif
			 * won't have link_conf assigned until CAC is completed
			 * This is expected and no WARN_ON required for them
			 */
			if (cfg80211_chandef_dfs_cac_time(ahvif->ah->hw->wiphy,
							  &arg->ctx->def, false,
							  false)) {
				if (!link_conf)
					continue;
			} else if (WARN_ON(!link_conf))
				continue;

			ctx = rcu_access_pointer(link_conf->chanctx_conf);
			if (ctx != arg->ctx)
				continue;

			if (WARN_ON(arg->next_vif == arg->n_vifs))
				return;

			bridge_link_id = ATH12K_NUM_MAX_LINKS;
		} else {
			if (arvif->ar != arg->ar)
				continue;

			if (WARN_ON(arg->next_vif == arg->n_vifs))
				return;

			link_conf = NULL;
			ctx = arg->ctx;
			bridge_link_id = arvif->link_id;
		}

		arg->vifs[arg->next_vif].vif = vif;
		arg->vifs[arg->next_vif].old_ctx = ctx;
		arg->vifs[arg->next_vif].new_ctx = arg->new_ctx;
		arg->vifs[arg->next_vif].link_conf = link_conf;
		arg->vifs_bridge_link_id[arg->next_vif] = bridge_link_id;
		arg->next_vif++;
	}
}

static void
ath12k_mac_update_peer_ru_punct_bitmap_iter(void *data,
					    struct ieee80211_sta *sta)
{
	struct ath12k_link_vif *arvif = data;
	struct ath12k *ar = arvif->ar;
	struct ath12k_sta *ahsta = (struct ath12k_sta *)sta->drv_priv;
	struct ath12k_link_sta *arsta;
	struct ieee80211_link_sta *link_sta;
	u8 link_id = arvif->link_id;

	if (ahsta->ahvif != arvif->ahvif)
		return;

	/* Check if there is a link sta in the vif link */
	if (!(BIT(link_id) & ahsta->links_map))
		return;

	arsta = ahsta->link[link_id];
	link_sta = ath12k_mac_get_link_sta(arsta);
	if (!link_sta) {
		ath12k_warn(ar->ab, "unable to access link sta in peer ru punct bitmap update\n");
		return;
	}

	/* Puncturing in only applicable for EHT supported peers */
	if (!link_sta->he_cap.has_he || !link_sta->eht_cap.has_eht)
		return;

	spin_lock_bh(&ar->data_lock);
	/* RC_BW_CHANGED handler has infra already to send the bitmap.
	 * Hence we can leverage from the same flag
	 */
	arsta->changed |= IEEE80211_RC_BW_CHANGED;
	spin_unlock_bh(&ar->data_lock);

	wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &arsta->update_wk);
}

void ath12k_mac_update_ru_punct_bitmap(struct ath12k_link_vif *arvif,
				       struct ieee80211_chanctx_conf *old_ctx,
				       struct ieee80211_chanctx_conf *new_ctx)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_hw *ah = ar->ah;

	if (!ath12k_mac_is_bridge_vdev(arvif) &&
	    old_ctx->def.punctured == new_ctx->def.punctured)
		return;

	ieee80211_iterate_stations_atomic(ah->hw,
					  ath12k_mac_update_peer_ru_punct_bitmap_iter,
					  arvif);
}

static int ath12k_vdev_restart_sequence(struct ath12k_link_vif *arvif,
					struct ieee80211_chanctx_conf *new_ctx,
					u64 vif_down_failed_map,
					int vdev_index)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_link_vif *tx_arvif;
	struct ath12k_vif *tx_ahvif;
	struct ieee80211_bss_conf *link;
	struct ieee80211_chanctx_conf old_chanctx;
	struct ath12k_wmi_vdev_up_params params = { 0 };
	int ret = -EINVAL;
	bool is_bridge_vdev;
	struct ath12k_vif *ahvif = arvif->ahvif;

	is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

	spin_lock_bh(&ar->data_lock);
	if (arvif->chanctx.def.chan)
		old_chanctx = arvif->chanctx;
	else
		memset(&old_chanctx, 0, sizeof(struct ieee80211_chanctx_conf));
	memcpy(&arvif->chanctx, new_ctx, sizeof(*new_ctx));
	spin_unlock_bh(&ar->data_lock);

	/* vdev is already restarted via mvr, need to setup
	 * certain config alone after restart */
	if (vdev_index == -1) {
        	ret = ath12k_mac_vdev_config_after_start(arvif, &new_ctx->def);
        	if (!ret)
                	goto beacon_tmpl_setup;
	} else if (vif_down_failed_map & BIT_ULL(vdev_index)) {
		ret = ath12k_mac_vdev_restart(arvif, new_ctx, false);
	} else {
		ret = ath12k_mac_vdev_restart(arvif, new_ctx, true);
	}

	if (ret) {
		ath12k_warn(ar->ab, "failed to restart vdev %d: %d\n",
			    arvif->vdev_id, ret);
		spin_lock_bh(&ar->data_lock);
		if (old_chanctx.def.chan)
			arvif->chanctx = old_chanctx;
		spin_unlock_bh(&ar->data_lock);
		return ret;
	}

beacon_tmpl_setup:

	ath12k_mac_update_ru_punct_bitmap(arvif, &old_chanctx, new_ctx);
	if (arvif->pending_csa_up)
		return 0;

	if (arvif->ahvif->vdev_type != WMI_VDEV_TYPE_MONITOR && !arvif->is_up)
		return -EOPNOTSUPP;

	if (!is_bridge_vdev) {
		ret = ath12k_mac_setup_bcn_tmpl(arvif);
		if (ret) {
			ath12k_warn(ar->ab, "failed to update bcn tmpl during csa: %d\n",
				    arvif->vdev_id);
			return ret;
		}
	}

	params.vdev_id = arvif->vdev_id;
	params.aid = ahvif->aid;
	params.bssid = arvif->bssid;
	if (!is_bridge_vdev) {
		rcu_read_lock();
		link = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);
		if (link->mbssid_tx_vif) {
			tx_ahvif = (void *)link->mbssid_tx_vif->drv_priv;
			tx_arvif = tx_ahvif->link[link->mbssid_tx_vif_linkid];
			params.tx_bssid = tx_arvif->bssid;
			params.nontx_profile_idx = ahvif->vif->bss_conf.bssid_index;
			params.nontx_profile_cnt = BIT(link->bssid_indicator);
		}

		if (ahvif->vif->type == NL80211_IFTYPE_STATION && link->nontransmitted) {
			params.nontx_profile_idx = link->bssid_index;
			params.nontx_profile_cnt = BIT(link->bssid_indicator) - 1;
			params.tx_bssid = link->transmitter_bssid;
		}
		rcu_read_unlock();
	}

	ret = ath12k_wmi_vdev_up(arvif->ar, &params);
	if (ret) {
		ath12k_warn(ar->ab, "failed to bring vdev up %d: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ath12k_dp_mon_rx_config_monitor_mode(ar, false);
		ret = ath12k_dp_mon_rx_update_filter(ar);
		if (ret) {
			ath12k_warn(ar->ab, "fail to set monitor filter: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static void ath12k_mac_num_chanctxs_iter(struct ieee80211_hw *hw,
                                         struct ieee80211_chanctx_conf *conf,
                                         void *data)
{
	struct ath12k_mac_num_chanctxs_arg *arg =
				(struct ath12k_mac_num_chanctxs_arg *)data;
	struct ath12k *ctx_ar, *ar = arg->ar;

	ctx_ar = ath12k_get_ar_by_ctx(ar->ah->hw, conf);

	if (ctx_ar == ar)
		arg->num++;
}

static int ath12k_mac_num_chanctxs(struct ath12k *ar)
{
	struct ath12k_mac_num_chanctxs_arg arg = { .ar = ar, .num = 0};

        ieee80211_iter_chan_contexts_atomic(ar->ah->hw,
                                            ath12k_mac_num_chanctxs_iter,
					    &arg);

	return arg.num;
}

static void ath12k_mac_update_rx_channel(struct ath12k *ar,
					 struct ieee80211_chanctx_conf *ctx,
					 struct ieee80211_vif_chanctx_switch *vifs,
					 int n_vifs)
{
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	struct cfg80211_chan_def *def = NULL;

	/* Both locks are required because ar->rx_channel is modified. This
	 * allows readers to hold either lock.
	 */
	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);
	lockdep_assert_held(&ar->data_lock);

	WARN_ON(ctx && vifs);
	WARN_ON(vifs && !n_vifs);

	/* FIXME: Sort of an optimization and a workaround. Peers and vifs are
	 * on a linked list now. Doing a lookup peer -> vif -> chanctx for each
	 * ppdu on Rx may reduce performance on low-end systems. It should be
	 * possible to make tables/hashmaps to speed the lookup up (be vary of
	 * cpu data cache lines though regarding sizes) but to keep the initial
	 * implementation simple and less intrusive fallback to the slow lookup
	 * only for multi-channel cases. Single-channel cases will remain to
	 * use the old channel derival and thus performance should not be
	 * affected much.
	 */
	rcu_read_lock();
	if (!ctx && ath12k_mac_num_chanctxs(ar) == 1) {
		arg.chanctx_conf = NULL;
		ieee80211_iter_chan_contexts_atomic(ath12k_ar_to_hw(ar),
						    ath12k_mac_get_any_chanctx_conf_iter,
						    &arg);

		if (vifs)
			def = &vifs[0].new_ctx->def;
		else if (arg.chanctx_conf)
			def = &arg.chanctx_conf->def;

		if (def)
			ar->rx_channel = def->chan;
		else
			ar->rx_channel = NULL;
	} else if ((ctx && ath12k_mac_num_chanctxs(ar) == 0) ||
		  (ctx && (ar->ah->state == ATH12K_HW_STATE_RESTARTED))) {
	       /* During driver restart due to firmware assert, since mac80211
		* already has valid channel context for given radio, channel
		* context iteration return num_chanctx > 0. So fix rx_channel
		* when restart is in progress.
		*/
		ar->rx_channel = ctx->def.chan;
	} else {
		ar->rx_channel = NULL;
	}
	rcu_read_unlock();
}

static int
ath12k_mac_multi_vdev_restart(struct ath12k *ar,
			      const struct cfg80211_chan_def *chandef,
			      u32 *vdev_id, int len,
			      bool radar_enabled)
{
	struct ath12k_base *ab = ar->ab;
	struct wmi_pdev_multiple_vdev_restart_req_arg arg = {};
	int ret, i;
	u16 punct_bitmap = chandef->punctured;

	arg.vdev_ids.id_len = len;

	for (i = 0; i < len; i++)
		arg.vdev_ids.id[i] = vdev_id[i];

	arg.vdev_start_arg.freq = chandef->chan->center_freq;
	arg.vdev_start_arg.band_center_freq1 = chandef->center_freq1;
	arg.vdev_start_arg.band_center_freq2 = chandef->center_freq2;
	arg.vdev_start_arg.mode =
		ath12k_phymodes[chandef->chan->band][chandef->width];

	arg.vdev_start_arg.min_power = 0;
	arg.vdev_start_arg.max_power = chandef->chan->max_power;
	arg.vdev_start_arg.max_reg_power = chandef->chan->max_reg_power;
	arg.vdev_start_arg.max_antenna_gain = chandef->chan->max_antenna_gain;
	arg.vdev_start_arg.chan_radar = !!(chandef->chan->flags & IEEE80211_CHAN_RADAR);
	arg.vdev_start_arg.passive = arg.vdev_start_arg.chan_radar;
	arg.vdev_start_arg.freq2_radar = radar_enabled;
	arg.vdev_start_arg.passive |= !!(chandef->chan->flags & IEEE80211_CHAN_NO_IR);

	if (test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map) &&
	    cfg80211_chandef_device_present(chandef)) {
		arg.width_device = chandef->width_device;
		arg.center_freq_device = chandef->center_freq_device;
		punct_bitmap = ath12k_mac_set_punct_bitmap_device(chandef->chan->center_freq,
								  chandef->width_device,
								  chandef->center_freq_device,
								  punct_bitmap);
	}

	arg.ru_punct_bitmap = ~punct_bitmap;

	ret = ath12k_wmi_pdev_multiple_vdev_restart(ar, &arg);
	if (ret)
		ath12k_warn(ab, "mac failed to do mvr (%d)\n", ret);

	return ret;
}

static void
ath12k_mac_update_vif_chan_extras(struct ath12k *ar,
				  struct ieee80211_vif_chanctx_switch *vifs,
				  int n_vifs)
{
	struct ath12k_base *ab = ar->ab;
	struct cfg80211_chan_def *chandef;

	chandef = &vifs[0].new_ctx->def;

	spin_lock_bh(&ar->data_lock);
        if (ar->awgn_intf_handling_in_prog && chandef) {
                if (!ar->chan_bw_interference_bitmap ||
                    (ar->chan_bw_interference_bitmap & WMI_DCS_SEG_PRI20)) {
                        if (ar->awgn_chandef.chan->center_freq !=
                            chandef->chan->center_freq) {
                                ar->awgn_intf_handling_in_prog = false;
                                ath12k_dbg(ab, ATH12K_DBG_MAC,
                                           "AWGN : channel switch completed\n");
                        } else {
                                ath12k_warn(ab, "AWGN : channel switch is not done, freq : %d\n",
                                            ar->awgn_chandef.chan->center_freq);
                        }
                } else {
                        if ((ar->awgn_chandef.chan->center_freq ==
                             chandef->chan->center_freq) &&
                            (ar->awgn_chandef.width != chandef->width)) {
                                ath12k_dbg(ab, ATH12K_DBG_MAC,
                                           "AWGN : BW reduction is complete\n");
                                ar->awgn_intf_handling_in_prog = false;
                        } else {
                                ath12k_warn(ab, "AWGN : awgn_freq : %d chan_freq %d"
                                            " awgn_width %d chan_width %d\n",
                                            ar->awgn_chandef.chan->center_freq,
                                            chandef->chan->center_freq,
                                            ar->awgn_chandef.width,
                                            chandef->width);
                        }
                }
        }
	spin_unlock_bh(&ar->data_lock);
}

static void
ath12k_mac_update_vif_chan(struct ath12k *ar,
			   struct ieee80211_vif_chanctx_switch *vifs,
			   u8 *vifs_bridge_link_id,
			   int n_vifs)
{
	struct ath12k_link_vif *arvif, *tx_arvif;
	struct ieee80211_bss_conf *link_conf;
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif, *tx_ahvif = NULL;
	u64 vif_down_failed_map = 0;
	u8 link_id;
	struct ieee80211_vif *tx_vif;
	int ret;
	int i, trans_vdev_index;
	bool monitor_vif = false;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* Each vif is mapped to each bit of vif_down_failed_map. */
	if (n_vifs > sizeof(vif_down_failed_map)*__CHAR_BIT__) {
		ath12k_warn(ar->ab, "%d n_vifs are not supported currently\n",
			    n_vifs);
		return;
	}

	tx_arvif = NULL;

	for (i = 0; i < n_vifs; i++) {
		vif = vifs[i].vif;
		ahvif = ath12k_vif_to_ahvif(vif);

		if (vifs_bridge_link_id &&
		    (ATH12K_BRIDGE_LINKS_MASK & BIT(vifs_bridge_link_id[i])))
			link_id = vifs_bridge_link_id[i];
		else
			link_id = vifs[i].link_conf->link_id;

		arvif = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
					  ahvif->link[link_id]);

		if (vif->type == NL80211_IFTYPE_MONITOR) {
			monitor_vif = true;
			continue;
		}

		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "mac chanctx switch vdev_id %i freq %u->%u width %d->%d link_id:%u\n",
			   arvif->vdev_id,
			   vifs[i].old_ctx->def.chan->center_freq,
			   vifs[i].new_ctx->def.chan->center_freq,
			   vifs[i].old_ctx->def.width,
			   vifs[i].new_ctx->def.width, link_id);

		if (!arvif->is_started) {
			memcpy(&arvif->chanctx, vifs[i].new_ctx, sizeof(*vifs[i].new_ctx));
			continue;
		}

		if (!arvif->is_up)
			continue;

		arvif->punct_bitmap = vifs[i].new_ctx->def.punctured;

		if (!ath12k_mac_is_bridge_vdev(arvif) &&
		    vifs[i].link_conf->mbssid_tx_vif &&
		    ahvif == (struct ath12k_vif *)vifs[i].link_conf->mbssid_tx_vif->drv_priv) {
			tx_vif = vifs[i].link_conf->mbssid_tx_vif;
			tx_ahvif = ath12k_vif_to_ahvif(tx_vif);
			tx_arvif = tx_ahvif->link[vifs[i].link_conf->mbssid_tx_vif_linkid];
			trans_vdev_index = i;
		}
		ret = ath12k_wmi_vdev_down(ar, arvif->vdev_id);
		if (ret) {
			vif_down_failed_map |= BIT_ULL(i);
			ath12k_warn(ab, "failed to down vdev %d: %d\n",
				    arvif->vdev_id, ret);
			continue;
		}
	}

	ath12k_mac_update_rx_channel(ar, NULL, vifs, n_vifs);

	if (tx_arvif) {
		rcu_read_lock();
		link_conf = rcu_dereference(tx_ahvif->vif->link_conf[tx_arvif->link_id]);

		if (link_conf->csa_active && tx_arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP)
			tx_arvif->pending_csa_up = true;

		rcu_read_unlock();

		ret = ath12k_vdev_restart_sequence(tx_arvif,
						   vifs[trans_vdev_index].new_ctx,
						   vif_down_failed_map,
						   trans_vdev_index);

		if (ret)
			ath12k_warn(ab, "failed to restart vdev:%d: %d\n",
				    tx_arvif->vdev_id, ret);
	}

	for (i = 0; i < n_vifs; i++) {
		ahvif = (void *)vifs[i].vif->drv_priv;

		if (vifs_bridge_link_id &&
		    (ATH12K_BRIDGE_LINKS_MASK & BIT(vifs_bridge_link_id[i])))
			link_id = vifs_bridge_link_id[i];
		else
			link_id = vifs[i].link_conf->link_id;

		arvif = ahvif->link[link_id];
		if (WARN_ON(!arvif))
			continue;

		if (!ath12k_mac_is_bridge_vdev(arvif)) {
			if (vifs[i].link_conf->mbssid_tx_vif &&
			    arvif == tx_arvif)
				continue;

			rcu_read_lock();
			link_conf = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);

			if (link_conf->csa_active && arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP)
				arvif->pending_csa_up = true;

			rcu_read_unlock();
		}

		ret = ath12k_vdev_restart_sequence(arvif,
						   vifs[i].new_ctx,
						   vif_down_failed_map, i);
		if (ret && ret != -EOPNOTSUPP) {
			ath12k_warn(ab, "failed to bring up vdev %d: %d\n",
				    arvif->vdev_id, ret);
		}
	}
}

static void
ath12k_mac_update_vif_chan_mvr(struct ath12k *ar,
			       struct ieee80211_vif_chanctx_switch *vifs,
			       u8 *vifs_bridge_link_id,
			       int n_vifs)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_vif *arvif, *tx_arvif;
	struct ath12k_vif *ahvif, *tx_ahvif;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_vif *tx_vif;
	int ret, i, time_left, trans_vdev_index, vdev_idx, n_vdevs = 0;
	u32 *vdev_ids;
	u8 size, link_id = 0;
	bool is_bridge_vdev;
	struct ieee80211_bss_conf *link;

	chandef = &vifs[0].new_ctx->def;
	tx_arvif = NULL;

	ath12k_dbg(ab, ATH12K_DBG_MAC, "mac chanctx switch via mvr");

	ath12k_mac_update_rx_channel(ar, NULL, vifs, n_vifs);

	size = ath12k_core_get_total_num_vdevs(ab);
	vdev_ids = kcalloc(size, sizeof(*vdev_ids), GFP_KERNEL);
	if (!vdev_ids) {
		ath12k_err(ar->ab, "Insufficient memory to update channel\n");
		return;
	}

	for (i = 0; i < n_vifs; i++) {
		ahvif = (void *)vifs[i].vif->drv_priv;

		if (vifs_bridge_link_id &&
		    (ATH12K_BRIDGE_LINKS_MASK & BIT(vifs_bridge_link_id[i])))
			link_id = vifs_bridge_link_id[i];
		else
			link_id = vifs[i].link_conf->link_id;

		arvif = ahvif->link[link_id];
		if (WARN_ON(!arvif))
			continue;

		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "mac chanctx switch vdev_id %i freq %u->%u width %d->%d link_id:%u\n",
			   arvif->vdev_id,
			   vifs[i].old_ctx->def.chan->center_freq,
			   vifs[i].new_ctx->def.chan->center_freq,
			   vifs[i].old_ctx->def.width,
			   vifs[i].new_ctx->def.width, link_id);

		if (!arvif->is_started) {
			memcpy(&arvif->chanctx, vifs[i].new_ctx, sizeof(*vifs[i].new_ctx));
			continue;
		}

		arvif->punct_bitmap = vifs[i].new_ctx->def.punctured;

		if (!ath12k_mac_is_bridge_vdev(arvif) &&
		    vifs[i].link_conf->mbssid_tx_vif &&
		    ahvif == (struct ath12k_vif *)vifs[i].link_conf->mbssid_tx_vif->drv_priv) {
			tx_vif = vifs[i].link_conf->mbssid_tx_vif;
			tx_ahvif = ath12k_vif_to_ahvif(tx_vif);
			tx_arvif = tx_ahvif->link[vifs[i].link_conf->mbssid_tx_vif_linkid];
			trans_vdev_index = i;
		}

		arvif->mvr_processing = true;
		vdev_ids[n_vdevs++] = arvif->vdev_id;
	}

	if (!n_vdevs) {
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "mac 0 vdevs available to switch chan ctx via mvr\n");
		goto out;
	}

	reinit_completion(&ar->mvr_complete);

	ret = ath12k_mac_multi_vdev_restart(ar, chandef, vdev_ids, n_vdevs,
					    vifs[0].new_ctx->radar_enabled);
	if (ret) {
		ath12k_warn(ab, "mac failed to send mvr command (%d)\n", ret);
		goto out;
	}

	time_left = wait_for_completion_timeout(&ar->mvr_complete,
						WMI_MVR_CMD_TIMEOUT_HZ);
	if (!time_left) {
		kfree(vdev_ids);
		ath12k_err(ar->ab, "mac mvr cmd response timed out\n");
		/* fallback to restarting one-by-one */
		return ath12k_mac_update_vif_chan(ar, vifs, vifs_bridge_link_id, n_vifs);
	}

	if (tx_arvif) {
		vdev_idx = -1;

		if (tx_arvif->mvr_processing) {
			/* failed to restart tx vif via mvr, fallback */
			arvif->mvr_processing = false;
			vdev_idx = trans_vdev_index;
			ath12k_err(ab,
				   "mac failed to restart mbssid tx vdev %d via mvr cmd\n",
				   tx_arvif->vdev_id);
		}

		rcu_read_lock();
		link = rcu_dereference(tx_ahvif->vif->link_conf[tx_arvif->link_id]);

		if (link->csa_active && tx_arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP)
			tx_arvif->pending_csa_up = true;

		rcu_read_unlock();

		WARN_ON(ath12k_mac_is_bridge_vdev(tx_arvif));

		ret = ath12k_vdev_restart_sequence(tx_arvif,
						   vifs[trans_vdev_index].new_ctx,
						   BIT_ULL(trans_vdev_index),
						   vdev_idx);
		if (ret)
			ath12k_warn(ab,
				    "mac failed to bring up mbssid tx vdev %d after mvr (%d)\n",
				    tx_arvif->vdev_id, ret);
	}

	for (i = 0; i < n_vifs; i++) {
		ahvif = (void *)vifs[i].vif->drv_priv;

		if (vifs_bridge_link_id &&
		    (ATH12K_BRIDGE_LINKS_MASK & BIT(vifs_bridge_link_id[i])))
			link_id = vifs_bridge_link_id[i];
		else
			link_id = vifs[i].link_conf->link_id;

		arvif = ahvif->link[link_id];
		if (WARN_ON(!arvif))
			continue;

		vdev_idx = -1;
		is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

		if (!is_bridge_vdev && vifs[i].link_conf->mbssid_tx_vif && arvif == tx_arvif)
			continue;

		if (arvif->mvr_processing) {
			/* failed to restart vdev via mvr, fallback */
			arvif->mvr_processing = false;
			vdev_idx = i;
			ath12k_err(ab, "mac failed to restart vdev %d via mvr cmd\n",
				   arvif->vdev_id);
		}

		if (!is_bridge_vdev) {
			rcu_read_lock();
			link = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);

			if (link->csa_active && arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP)
				arvif->pending_csa_up = true;

			rcu_read_unlock();
		}
		ret = ath12k_vdev_restart_sequence(arvif, vifs[i].new_ctx,
						   BIT_ULL(i), vdev_idx);
		if (ret && ret != -EOPNOTSUPP)
			ath12k_warn(ab, "mac failed to bring up vdev %d after mvr (%d)\n",
				    arvif->vdev_id, ret);
	}
out:
	kfree(vdev_ids);
}

static void
ath12k_mac_process_update_vif_chan(struct ath12k *ar,
				   struct ieee80211_vif_chanctx_switch *vifs,
				   u8 *vifs_bridge_link_id,
				   int n_vifs)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw_group *ag = ab->ag;

	if (ag && ag->num_devices >= ATH12K_MIN_NUM_DEVICES_NLINK) {
		if (WARN_ON(n_vifs > TARGET_NUM_VDEVS + TARGET_NUM_BRIDGE_VDEVS))
			/* should not happen */
			return;
	} else {
		if (WARN_ON(n_vifs > TARGET_NUM_VDEVS))
			/* should not happen */
			return;
	}

	if (ath12k_wmi_is_mvr_supported(ab))
		ath12k_mac_update_vif_chan_mvr(ar, vifs, vifs_bridge_link_id, n_vifs);
	else
		ath12k_mac_update_vif_chan(ar, vifs, vifs_bridge_link_id, n_vifs);

	ath12k_mac_update_vif_chan_extras(ar, vifs, n_vifs);
}

static void
ath12k_mac_update_active_vif_chan(struct ath12k *ar,
				  struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k_mac_change_chanctx_arg arg = { .ctx = ctx,
						     .new_ctx = ctx,
						     .set_csa_active = true,
						     .ar = ar };
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ieee80211_iterate_active_interfaces_atomic(hw,
						   IEEE80211_IFACE_ITER_NORMAL,
						   ath12k_mac_change_chanctx_cnt_iter,
						   &arg);
	if (arg.n_vifs == 0 || ar->csa_active_cnt > 1)
		return;

	arg.vifs = kcalloc(arg.n_vifs, sizeof(arg.vifs[0]), GFP_KERNEL);
	if (!arg.vifs)
		return;

	arg.vifs_bridge_link_id =
		kcalloc(arg.n_vifs, sizeof(arg.vifs_bridge_link_id[0]), GFP_KERNEL);
	if (!arg.vifs_bridge_link_id)
		goto out;

	if (ar->csa_active_cnt)
		ar->csa_active_cnt = 0;

	ieee80211_iterate_active_interfaces_atomic(hw,
						   IEEE80211_IFACE_ITER_NORMAL,
						   ath12k_mac_change_chanctx_fill_iter,
						   &arg);

	ath12k_mac_process_update_vif_chan(ar, arg.vifs, arg.vifs_bridge_link_id, arg.n_vifs);

out:
	kfree(arg.vifs);
}

void ath12k_mac_op_change_chanctx(struct ieee80211_hw *hw,
				  struct ieee80211_chanctx_conf *ctx,
				  u32 changed)
{
	struct ath12k *ar;
	struct ath12k_base *ab;

	lockdep_assert_wiphy(hw->wiphy);

	ar = ath12k_get_ar_by_ctx(hw, ctx);
	if (!ar)
		return;

	ab = ar->ab;

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac chanctx change freq %u width %d ptr %p changed %x\n",
			ctx->def.chan->center_freq, ctx->def.width, ctx, changed);

	/* This shouldn't really happen because channel switching should use
	 * switch_vif_chanctx().
	 */
	if (WARN_ON(changed & IEEE80211_CHANCTX_CHANGE_CHANNEL))
		return;

	if (changed & IEEE80211_CHANCTX_CHANGE_WIDTH ||
	    changed & IEEE80211_CHANCTX_CHANGE_RADAR ||
	    changed & IEEE80211_CHANCTX_CHANGE_PUNCTURING)
		ath12k_mac_update_active_vif_chan(ar, ctx);

	/* TODO: Recalc radar detection */
}
EXPORT_SYMBOL(ath12k_mac_op_change_chanctx);

static int ath12k_start_vdev_delay(struct ath12k *ar,
				   struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	int ret;

	if (WARN_ON(arvif->is_started))
		return -EBUSY;

	ret = ath12k_mac_vdev_start(arvif, &arvif->chanctx);
	if (ret) {
		ath12k_warn(ab, "failed to start vdev %i addr %pM on freq %d: %d\n",
			    arvif->vdev_id, vif->addr,
			    arvif->chanctx.def.chan->center_freq, ret);
		return ret;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ret = ath12k_monitor_vdev_up(ar, arvif->vdev_id);
		if (ret) {
			ath12k_warn(ab, "failed put monitor up: %d\n", ret);
			return ret;
		}
	}

	arvif->is_started = true;

	/* TODO: Setup ps and cts/rts protection */
	return 0;
}

static int
ath12k_mac_assign_vif_chanctx_handle(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct ieee80211_bss_conf *link_conf,
				     struct ieee80211_chanctx_conf *ctx,
				     u8 link_id, u16 bridge_ar_link_idx)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	enum ieee80211_sta_state state, prev_state;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	int ret;
	bool is_bridge_vdev;
	enum ieee80211_ap_reg_power power_type;

	lockdep_assert_wiphy(hw->wiphy);

	is_bridge_vdev = (ATH12K_BRIDGE_LINKS_MASK & BIT(link_id)) ?
			 true : false;

	if (!ctx && !is_bridge_vdev) {
		ath12k_err(NULL, "Channel ctx is NULL for vif %pM link %u\n",
			   vif->addr, link_id);
		return -EINVAL;
	}

	/* For multi radio wiphy, the vdev was not created during add_interface
	 * create now since we have a channel ctx now to assign to a specific ar/fw
	 */
	arvif = ath12k_mac_assign_link_vif(ah, vif, link_id, is_bridge_vdev);
	if (!arvif) {
		WARN_ON(1);
		return -ENOMEM;
	}

	ar = ath12k_mac_assign_vif_to_vdev(hw, arvif, ctx,
					   is_bridge_vdev,
					   bridge_ar_link_idx);
	if (!ar) {
		ath12k_hw_warn(ah, "failed to assign chanctx for vif %pM link id %u link vif is already started",
			       vif->addr, link_id);
		return -EINVAL;
	}

	ab = ar->ab;

	if (ab->is_bypassed)
		return 0;

	ath12k_vendor_link_state_update(ar->pdev_idx, ab, arvif,
					ATH12K_VENDOR_LINK_STATE_ADDED);

	ret = ath12k_ppeds_attach_link_vif(arvif, ahvif->dp_vif.ppe_vp_num,
					   &arvif->ppe_vp_profile_idx, vif);
	if (ret)
		ath12k_info(ab, "Unable to attach ppe ds node for arvif\n");

	if (ctx)
		ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mac chanctx assign ptr %p vdev_id %i, vdev_subtype=%0x\n",
				ctx, arvif->vdev_id, arvif->vdev_subtype);
	else
		ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mac chanctx for vdev_id %i vdev_subtype=%0x\n",
				arvif->vdev_id, arvif->vdev_subtype);


	if (!is_bridge_vdev)
		arvif->punct_bitmap = ctx->def.punctured;

	if (!is_bridge_vdev && ar->supports_6ghz && ctx->def.chan->band == NL80211_BAND_6GHZ &&
            (ahvif->vdev_type == WMI_VDEV_TYPE_STA ||
             ahvif->vdev_type == WMI_VDEV_TYPE_AP)) {
                power_type = vif->bss_conf.power_type;
		ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mac chanctx power type %d\n", power_type);
                if (power_type == IEEE80211_REG_UNSET_AP)
                        power_type = IEEE80211_REG_LPI_AP;

		arvif->chanctx = *ctx;

		if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
                        ath12k_mac_parse_tx_pwr_env(ar, arvif);
        }

	/* for some targets bss peer must be created before vdev_start */
	if (ab->hw_params->vdev_start_delay &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_AP &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_MONITOR &&
	    !ath12k_dp_link_peer_exist_by_vdev_id(ab->dp, arvif->vdev_id)) {
		memcpy(&arvif->chanctx, ctx, sizeof(*ctx));
		ret = 0;
		goto out;
	}

	if (!ab->hw_params->vdev_start_delay &&
	    ahvif->vdev_type == WMI_VDEV_TYPE_STA && ahvif->chanctx_peer_del_done) {
		rcu_read_lock();
		sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);
		if (!sta) {
			ath12k_warn(ar->ab, "failed to find station entry for bss vdev\n");
			rcu_read_unlock();
			goto out;
		}

		ahsta = ath12k_sta_to_ahsta(sta);
		arsta = &ahsta->deflink;
		rcu_read_unlock();

		mutex_lock(&ah->hw_mutex);
		prev_state = arsta->ahsta->state;
		for (state = IEEE80211_STA_NOTEXIST; state < prev_state;
		     state++)
			ath12k_mac_op_sta_state(ar->ah->hw, arvif->ahvif->vif, sta,
						state, (state + 1));
		mutex_unlock(&ah->hw_mutex);
		ahvif->chanctx_peer_del_done = false;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ret = ath12k_mac_monitor_start(ar);
		if (ret) {
			ath12k_mac_monitor_vdev_delete(ar);
			goto out;
		}

		if (ctx)
			memcpy(&arvif->chanctx, ctx, sizeof(*ctx));

		arvif->is_started = true;
		goto out;
	}

	if (ctx) {
		memcpy(&arvif->chanctx, ctx, sizeof(*ctx));

		if (ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
			ath12k_vendor_link_state_update(ar->pdev_idx, ab, arvif,
						ATH12K_VENDOR_LINK_STATE_ASSIGNED);
		}

		ret = ath12k_mac_vdev_start(arvif, ctx);
		if (ret) {
			ath12k_warn(ab, "failed to start vdev %i addr %pM on freq %d: %d\n",
				    arvif->vdev_id, vif->addr,
				    ctx->def.chan->center_freq, ret);
			goto out;
		}
	} else {
		memset(&arvif->chanctx, 0, sizeof(*ctx));
		ret = ath12k_mac_vdev_start(arvif, NULL);
		if (ret) {
			ath12k_warn(ab, "failed to start vdev %i addr %pM with ret %d\n",
				    arvif->vdev_id, arvif->bssid, ret);
			goto out;
		}
	}

	arvif->is_started = true;

	/* TODO: Setup ps and cts/rts protection */
	if (is_bridge_vdev && ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		ath12k_info(ab, "STA Bridge VAP created\n");

out:
	return ret;
}

void
ath12k_mac_unassign_vif_chanctx_handle(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_bss_conf *link_conf,
				       struct ieee80211_chanctx_conf *ctx,
				       u8 bridge_link_id)
{
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	u8 link_id;
	int ret;

	if (link_conf) {
        	link_id = link_conf->link_id;
	} else if (bridge_link_id) {
        	link_id = bridge_link_id;
	} else {
        	ath12k_err(NULL, "unable to get the link id\n");
	        return;
	}

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);

	/* The vif is expected to be attached to an ar's VDEV.
	 * We leave the vif/vdev in this function as is
	 * and not delete the vdev symmetric to assign_vif_chanctx()
	 * the VDEV will be deleted and unassigned either during
	 * remove_interface() or when there is a change in channel
	 * that moves the vif to a new ar
	 */
	if (!arvif || !arvif->is_created)
		return;

	ar = arvif->ar;
	ab = ar->ab;

	ath12k_vendor_link_state_update(ar->pdev_idx, ab, arvif,
					ATH12K_VENDOR_LINK_STATE_UNASSIGNED);

	if (unlikely(test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)))
		return;

	if (ctx)
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "mac chanctx unassign ptr %p vdev_id %i vdev_subtype %0x\n",
			   ctx, arvif->vdev_id, arvif->vdev_subtype);
	else
		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "mac chanctx unassign for vdev_id %i vdev_subtype %0x\n",
			   arvif->vdev_id, arvif->vdev_subtype);

	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA) {
		if (!arvif->is_started)
			ath12k_warn(ab, "interface not started vdev_id %i vdev_subtype %0x\n",
				    arvif->vdev_id, arvif->vdev_subtype);
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ret = ath12k_mac_monitor_stop(ar);
		if (ret)
			return;

		arvif->is_started = false;
	}

	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_MONITOR) {
		ath12k_bss_disassoc(ar, arvif);
		ret = ath12k_mac_vdev_stop(arvif);
		if (ret)
			ath12k_warn(ab, "failed to stop vdev %i: %d\n",
				    arvif->vdev_id, ret);
	} else if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
		   arvif->is_started && !arvif->is_up) {
		ret = ath12k_mac_vdev_stop(arvif);
		if (ret)
			ath12k_warn(ab, "failed to stop vdev %i: %d\n",
				    arvif->vdev_id, ret);
		else
			arvif->is_started = false;
	}

	ath12k_ppeds_detach_link_vif(arvif, arvif->ppe_vp_profile_idx);
	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA)
		arvif->is_started = false;

	if (ar->scan.arvif == arvif && ar->scan.state == ATH12K_SCAN_RUNNING) {
		ath12k_scan_abort(ar);
		ar->scan.arvif = NULL;
	}
	memset(&arvif->chanctx, 0, sizeof(*ctx));
	if (test_bit(WMI_TLV_SERVICE_11D_OFFLOAD, ab->wmi_ab.svc_map) &&
	    ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_NONE &&
	    ar->state_11d != ATH12K_11D_PREPARING) {
		reinit_completion(&ar->completed_11d_scan);
		ar->state_11d = ATH12K_11D_PREPARING;
	}

	/* In legacy station association with the AP,
	 * arvif is created during channel context assignment.
	 * However, since mac80211 is unaware of this link,
	 * it is not deleted automatically. To prevent stale arvif
	 * entries in ahvif, this link must be explicitly removed
	 * during channel context unassignment.
	 */
	if (!vif->valid_links) {
		ath12k_mac_remove_link_interface(hw, arvif);
		ath12k_mac_unassign_link_vif(arvif);
	}

}

static void
ath12k_mac_stop_bridge_vdevs(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	unsigned long links, scan_links;
	int ret;
	u8 link_id;
	unsigned int num_vdev;

	if (!hw || !vif) {
		ath12k_err(NULL, "Data NA for AP bridge vdevs stop\n");
		return;
	}

	/* Proceed only for MLO */
	if (!vif->valid_links)
		return;

	if (vif->type != NL80211_IFTYPE_AP)
		return;

	ahvif = (void *)vif->drv_priv;

	links = ahvif->links_map;
	scan_links = ATH12K_SCAN_LINKS_MASK;
	num_vdev = hweight16(ahvif->links_map & ~BIT(IEEE80211_MLD_MAX_NUM_LINKS));

	for_each_andnot_bit(link_id, &links, &scan_links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!arvif) {
			ath12k_err(NULL,
				   "link info NA for link: %u in AP bridge vdevs stop\n",
				   link_id);
			continue;
		}

		/* Proceed bridge vdev stop only after all the normal vdevs are stopped */
		if (link_id < IEEE80211_MLD_MAX_NUM_LINKS) {
			if (ath12k_erp_get_sm_state() == ATH12K_ERP_ENTER_COMPLETE) {
				if (num_vdev > ATH12K_ERP_BRIDGE_VDEV_REMOVAL_THRESHOLD)
					return;
			} else {
				if (arvif->is_started)
					return;
			}

			continue;
		}

		if (arvif->is_up) {
			/* When interfaces are getting removed,
			 * during CAC inprogress, the bridge vdevs
			 * will not be brought down in the normal
			 * flow since the 5G normal vdev is
			 * created and started but not brought up.
			 * However, in this case, all the bridge
			 * vdevs present will be up and they are
			 * stopped without bringing them down.
			 * So bridge vdev will be brought down
			 * here during these specific scenarios.
			 */
			ret = ath12k_wmi_vdev_down(arvif->ar, arvif->vdev_id);
			if (ret) {
				ath12k_warn(arvif->ar->ab,
					    "failed to down vdev_id %i: %d\n",
					    arvif->vdev_id, ret);
				continue;
			}
			arvif->is_up = false;
		}
		if (arvif->is_started)
			ath12k_mac_unassign_vif_chanctx_handle(hw, vif, NULL, NULL,
							       link_id);
	}
}

void
ath12k_mac_op_unassign_vif_chanctx(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_bss_conf *link_conf,
				   struct ieee80211_chanctx_conf *ctx)
{
	lockdep_assert_wiphy(hw->wiphy);

	ath12k_mac_unassign_vif_chanctx_handle(hw, vif, link_conf, ctx, 0);
	ath12k_mac_stop_bridge_vdevs(hw, vif);
}
EXPORT_SYMBOL(ath12k_mac_op_unassign_vif_chanctx);

static int ath12k_mac_target_supp_n_link_mlo(struct ath12k_base *ab)
{
	if (!ab)
		return -ENODATA;

	if (test_bit(WMI_TLV_SERVICE_N_LINK_MLO_SUPPORT, ab->wmi_ab.svc_map) &&
	    test_bit(WMI_TLV_SERVICE_BRIDGE_VDEV_SUPPORT, ab->wmi_ab.svc_map))
		return 0;

	return -EOPNOTSUPP;
}

static int ath12k_mac_get_link_idx_for_bridge(struct ieee80211_hw *hw,
					      unsigned long *link_idx_bmp)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag;
	struct ath12k *ar1, *ar2;
	int ret = -ENODATA;
	u32 adj_device1, adj_device2;
	struct ath12k_wsi_info *wsi_info, *adj_wsi_info;

	ar1 = ah->radio;
	ag = ar1->ab->ag;
	for (int i = 0; i < ah->num_radio; i++, ar1++) {
		if (!ar1)
			continue;

		ret = ath12k_mac_target_supp_n_link_mlo(ar1->ab);
		if (ret)
			goto err;

		if (ag->num_devices > ATH12K_MIN_NUM_DEVICES_NLINK) {
			ret = -EOPNOTSUPP;
			goto err;
		}

		if (BRIDGE_IN_RANGE(ar1)) {
			ar2 = ar1;
			wsi_info = ath12k_core_get_current_wsi_info(ar1->ab);
			ar2++;
			adj_device1 = wsi_info->adj_chip_idxs[0];
			adj_device2 = wsi_info->adj_chip_idxs[1];
			for (int j = 0; j < ah->num_radio - i; j++, ar2++) {
				if (!ar2)
					continue;
				adj_wsi_info = ath12k_core_get_current_wsi_info(ar2->ab);
				if (BRIDGE_IN_RANGE(ar2) &&
				    (adj_wsi_info->index == adj_device1 ||
				     adj_wsi_info->index == adj_device2)) {
					*link_idx_bmp = BIT(ar1->hw_link_id) | BIT(ar2->hw_link_id);
					ret = 0;
					goto exit;
				}
			}
			ret = -ENOMEM;
		}
	}

err:
	*link_idx_bmp = 0;
exit:
	return ret;
}

static inline struct ath12k *ath12k_mac_get_ar(struct ath12k_hw *ah,
					       u8 link_idx)
{
	struct ath12k *ar;
	int i = 0;

	if (link_idx >= ah->num_radio)
		return NULL;

	for (i = 0; i < ah->num_radio; i++) {
		ar = &ah->radio[i];
		if (ar->hw_link_id == link_idx)
			return ar;
	}

	return NULL;
}

static struct ieee80211_chanctx_conf *ath12k_mac_get_ctx_for_bridge(struct ath12k_hw *ah, u8 link_idx)
{
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;

	ar = ath12k_mac_get_ar(ah, link_idx);
	if (!ar)
		return NULL;

	arvif = list_first_entry_or_null(&ar->arvifs, struct ath12k_link_vif,
					 list);

	if (arvif && !(ATH12K_SCAN_LINKS_MASK & BIT(arvif->link_id)) &&
	    arvif->chanctx.def.chan)
		return &arvif->chanctx;
	else
		return NULL;
}

static bool ath12k_mac_need_ctx_sync(struct ieee80211_chanctx_conf *new_ctx,
				     struct ieee80211_chanctx_conf *bridge_ctx)
{
	struct cfg80211_chan_def *new_def, *bridge_def;
	struct ieee80211_channel *new_chan, *bridge_chan;

	if (!bridge_ctx->def.chan)
		return true;

	new_def = &new_ctx->def;
	bridge_def = &bridge_ctx->def;
	new_chan = new_ctx->def.chan;
	bridge_chan = bridge_ctx->def.chan;

	if ((new_chan->center_freq == bridge_chan->center_freq) &&
	    (new_def->center_freq1 == bridge_def->center_freq1) &&
	    (new_def->center_freq2 == bridge_def->center_freq2) &&
	    (ath12k_phymodes[new_chan->band][new_def->width] ==
	     ath12k_phymodes[bridge_chan->band][bridge_def->width]) &&
	    (new_chan->max_power == bridge_chan->max_power) &&
	    (new_chan->max_reg_power == bridge_chan->max_reg_power) &&
	    (new_chan->max_antenna_gain == bridge_chan->max_antenna_gain))
		return false;
	return true;
}

static int ath12k_mac_sync_ctx_on_radio(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif,
					struct ieee80211_chanctx_conf *ctx,
					int *num_devices)
{
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct ath12k_hw_group *ag = NULL;

	ar = ath12k_get_ar_by_ctx(hw, ctx);
	if (!ar)
		return -EINVAL;

	ag = ar->ab->ag;
	*num_devices = ag->num_devices - ag->num_bypassed;
	if (*num_devices < ATH12K_MIN_NUM_DEVICES_NLINK)
		goto exit;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!ath12k_mac_is_bridge_vdev(arvif))
			continue;
		if (ath12k_mac_need_ctx_sync(ctx, &arvif->chanctx)) {
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L3,
					"ctx syncing\n");
			ath12k_mac_update_active_vif_chan(ar, ctx);
		}
		break;
	}

exit:
	return 0;
}

static void ath12k_mac_handle_failures_bridge_addition(struct ieee80211_hw *hw,
						       struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = (void *)vif->drv_priv;
	struct ath12k_link_vif *arvif;
	u8 link_id = ATH12K_BRIDGE_LINK_MIN;
	unsigned long links = ahvif->links_map;

	for_each_set_bit_from(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = ahvif->link[link_id];

		if (WARN_ON(!arvif))
			continue;

		if (arvif->is_started) {
			ath12k_mac_unassign_vif_chanctx_handle(hw, vif, NULL, NULL, link_id);
		} else if (arvif->is_created) {
			ath12k_mac_remove_link_interface(hw, arvif);
			ath12k_mac_unassign_link_vif(arvif);
		} else {
			ath12k_mac_unassign_link_vif(arvif);
		}
	}
}

static void ath12k_mac_configure_bridge_vap_sta_mode(struct ieee80211_hw *hw,
						     struct ieee80211_vif *vif,
						     int num_devices)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_vif *ahvif = (void *)vif->drv_priv;
	struct ieee80211_chanctx_conf *bridge_ctx = NULL;
	int ret;
	u32 device_idx = 0;
	unsigned long bridge_bitmap = 0;
	u8 bridge_ar_link_idx;

	ret = ath12k_mac_is_bridge_required(ahvif->device_bitmap,
					    num_devices,
					    (u16 *)&bridge_bitmap);
	if (!ret)
		return;

	for_each_set_bit_from(device_idx, &bridge_bitmap, num_devices) {
		ret = ath12k_mac_get_link_idx_with_device_idx(ah, device_idx,
							      &bridge_ar_link_idx);
		if (ret) {
			bridge_ctx = ath12k_mac_get_ctx_for_bridge(ah,
								   bridge_ar_link_idx);
			ret = ath12k_mac_assign_vif_chanctx_handle(hw, vif, NULL,
								   bridge_ctx,
								   ATH12K_BRIDGE_LINK_MIN,
								   bridge_ar_link_idx);
			if (ret) {
				ath12k_dbg(NULL, ATH12K_DBG_MAC,
					   "Bridge VAP addition for STA mode failed\n");
				ath12k_mac_handle_failures_bridge_addition(hw, vif);
				continue;
			}
			break;
		}
	}
}

static int ath12k_mac_create_and_start_bridge(struct ieee80211_hw *hw,
					      struct ieee80211_vif *vif,
					      struct ieee80211_bss_conf *link_conf,
					      struct ieee80211_chanctx_conf *ctx,
					      int num_devices)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = ah->radio;
	struct ath12k_hw_group *ag = ar->ab->ag;
	struct ath12k_wsi_info *wsi_info;
	struct ath12k_vif *ahvif = (void *)vif->drv_priv;
	struct ieee80211_chanctx_conf *bridge_ctx = NULL;
	struct ath12k_link_vif *arvif;
	unsigned long links_map, link_idx_bmp;
	u32 device_idx;
	int ret;
	u8 link_id, bridge_ar_link_idx, curr_link_id;
	bool bridge_needed = false;

	/* Currently bridge vdev addition is supported in AP and STA mode */
	if (vif->type != NL80211_IFTYPE_AP &&
	    vif->type != NL80211_IFTYPE_STATION)
		goto exit;

	/* Bridge needed only during MLO */
	if (!vif->valid_links)
		goto exit;

	/* Currently bridge needed for 4 QCN9274 devices */
	if (num_devices < ATH12K_MIN_NUM_DEVICES_NLINK)
		goto exit;

	if (num_devices > ATH12K_MIN_NUM_DEVICES_NLINK) {
		ath12k_err(NULL, "Bridge vdev not yet supported for more than 4 devices\n");
		goto exit;
	}

	if (ath12k_hw_group_recovery_in_progress(ag)) {
		if (ahvif->mode0_recover_bridge_vdevs) {
			link_id = ATH12K_BRIDGE_LINK_MIN;
			links_map = ahvif->links_map;
			for_each_set_bit_from(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
				arvif = ahvif->link[link_id];

				if (WARN_ON(!arvif))
					continue;

				if (arvif->chanctx.def.chan)
					bridge_ctx = &arvif->chanctx;
				else
					bridge_ctx = NULL;

				ret = ath12k_mac_assign_vif_chanctx_handle(hw, vif, NULL, bridge_ctx, arvif->link_id, arvif->ar->hw_link_id);
				if (ret) {
					ath12k_err(NULL, "Bridge VAP addition during Mode0 recovery failed for MLD:%pM\n",
						   vif->addr);
					ath12k_mac_handle_failures_bridge_addition(hw, vif);
					break;
				} else {
					ath12k_dbg(NULL, ATH12K_DBG_MAC, "Added Bridge vdev(link_id:%u) during Mode0 recovery for MLD:%pM\n",
						   link_id, vif->addr);
				}
			}
			ahvif->mode0_recover_bridge_vdevs = false;
		}
	} else {
		/* Only MLO with more than 1 link, needs bridge vdevs */
		if (ahvif->links_map & ATH12K_BRIDGE_LINKS_MASK)
			goto exit;

		curr_link_id = link_conf->link_id;
		arvif = ahvif->link[curr_link_id];
		if (!arvif) {
			ath12k_err(NULL, "Bridge cannot be created, vdev not created with link_id=%u\n",
				   curr_link_id);
			goto exit;
		}
		device_idx = arvif->ar->ab->wsi_info.index;

		links_map = ahvif->links_map;
		for_each_set_bit(link_id, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
			if (link_id == curr_link_id)
				continue;

			arvif = ahvif->link[link_id];
			if (!arvif->ar)
				continue;
			wsi_info = ath12k_core_get_current_wsi_info(arvif->ar->ab);
			if (BIT(device_idx) & wsi_info->diag_device_idx_bmap) {
				bridge_needed = true;
				break;
			}
		}

		if (!bridge_needed)
			goto exit;

		/* STA mode Bridge vdev handling */
		if (vif->type == NL80211_IFTYPE_STATION) {
			ath12k_mac_configure_bridge_vap_sta_mode(hw, vif,
								 num_devices);
			goto exit;
		}

		/* AP mode Bridge vdev handling */
		ret = ath12k_mac_get_link_idx_for_bridge(hw, &link_idx_bmp);
		if (ret) {
			ath12k_dbg(NULL, ATH12K_DBG_MAC,
				   "Unable to determine the bridge addition radios, ret:%d\n",
				   ret);
			goto exit;
		}

		if (hweight8(link_idx_bmp) != ATH12K_MAX_NUM_BRIDGE_PER_MLD) {
			ath12k_dbg(NULL, ATH12K_DBG_MAC, "Incorrect bridge creation count:%d\n",
				   hweight8(link_idx_bmp));
			goto exit;
		}

		for_each_set_bit(bridge_ar_link_idx, &link_idx_bmp, ATH12K_MAX_AR_LINK_IDX) {
			bridge_ctx = ath12k_mac_get_ctx_for_bridge(ah, bridge_ar_link_idx);

			links_map = ahvif->links_map >> ATH12K_BRIDGE_LINK_MIN;
			link_id = (ffs(~links_map) - 1) + ATH12K_BRIDGE_LINK_MIN;

			ret = ath12k_mac_assign_vif_chanctx_handle(hw, vif, NULL, bridge_ctx, link_id, bridge_ar_link_idx);
			if (ret) {
				ath12k_err(NULL, "Bridge VAP addition failed for MLD:%pM\n", vif->addr);
				ath12k_mac_handle_failures_bridge_addition(hw, vif);
				goto exit;
			}
		}
		ath12k_dbg(NULL, ATH12K_DBG_MAC, "Bridge vdevs added for MLD:%pM\n", vif->addr);
	}

exit:
	return 0;
}

int
ath12k_mac_op_assign_vif_chanctx(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_bss_conf *link_conf,
				 struct ieee80211_chanctx_conf *ctx)
{
	int ret, num_devices = 0;

	if (!ctx)
		return -EINVAL;

	lockdep_assert_wiphy(hw->wiphy);

	ret = ath12k_mac_sync_ctx_on_radio(hw, vif, ctx, &num_devices);
	if (ret)
		goto exit;

	ret = ath12k_mac_assign_vif_chanctx_handle(hw, vif, link_conf, ctx, link_conf->link_id, 0);
	if (ret) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC, "vif chanctx not assigned\n");
		goto exit;
	}

	ret = ath12k_mac_create_and_start_bridge(hw, vif, link_conf, ctx, num_devices);

exit:
	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_assign_vif_chanctx);

static bool ath12k_mac_is_bridge_vdev_present(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (ath12k_mac_is_bridge_vdev(arvif))
			return true;
	}
	return false;
}

int
ath12k_mac_op_switch_vif_chanctx(struct ieee80211_hw *hw,
				 struct ieee80211_vif_chanctx_switch *vifs,
				 int n_vifs,
				 enum ieee80211_chanctx_switch_mode mode)
{
	//struct ath12k_hw *ah = hw->priv;
	struct ath12k *curr_ar, *new_ar, *ar;
	struct ieee80211_chanctx_conf *curr_ctx;
	int i, ret = 0, next_ctx_idx = 0, curr_ctx_n_vifs = 0;
	bool is_bridge_vdev;

	lockdep_assert_wiphy(hw->wiphy);

	/* TODO Switching a vif between two radios require deleting of vdev
	 * in its current ar and creating a vdev and applying its cached params
	 * to the new vdev in ar. So instead of returning error, handle it?
	 */
	for (i = 0; i < n_vifs; i++) {
		if (vifs[i].old_ctx->def.chan->band !=
		    vifs[i].new_ctx->def.chan->band) {
			WARN_ON(1);
			ret = -EINVAL;
			break;
		}

		curr_ar = ath12k_get_ar_by_ctx(hw, vifs[i].old_ctx);
		new_ar = ath12k_get_ar_by_ctx(hw, vifs[i].new_ctx);
		if (!curr_ar || !new_ar) {
			ath12k_err(NULL,
				   "unable to determine device for the passed channel ctx");
			ath12k_err(NULL,
				   "Old freq %d MHz (device %s) to new freq %d MHz (device %s)\n",
				   vifs[i].old_ctx->def.chan->center_freq,
				   curr_ar ? "valid" : "invalid",
				   vifs[i].new_ctx->def.chan->center_freq,
				   new_ar ? "valid" : "invalid");
			ret = -EINVAL;
			break;
		}

		/* Switching a vif between two radios is not allowed */
		if (curr_ar != new_ar) {
			ath12k_dbg(curr_ar->ab, ATH12K_DBG_MAC,
				   "mac chanctx switch to another radio not supported.");
			ret = -EOPNOTSUPP;
			break;
		}
	}

        if (ret)
                return ret;

	/* List of vifs contains data grouped by the band, example: All 2 GHz vifs
	 * ready to be switched to new context followed by all 5 GHz. The order of
	 * bands is not fixed. Send MVR when the loop processes the last vif
	 * for a particular band.
	 */
	for (i = 0; i < n_vifs; i++) {
		curr_ctx = vifs[i].old_ctx;
		ar = ath12k_get_ar_by_ctx(hw, curr_ctx);

		if ((i + 1 < n_vifs) && (vifs[i + 1].old_ctx == curr_ctx))
			continue;

		if (ar->csa_active_cnt >= curr_ctx_n_vifs)
			ar->csa_active_cnt -= curr_ctx_n_vifs;

		is_bridge_vdev = ath12k_mac_is_bridge_vdev_present(ar);
		/* Control will reach here only for the last vif for curr_ctx */
		if (ath12k_wmi_is_mvr_supported(ar->ab) || is_bridge_vdev) {
			struct ath12k_mac_change_chanctx_arg arg = {};

			arg.ar = ar;
			arg.ctx = curr_ctx;
			arg.new_ctx = vifs[i].new_ctx;
			ieee80211_iterate_active_interfaces_atomic(ar->ah->hw,
								   IEEE80211_IFACE_ITER_NORMAL,
								   ath12k_mac_change_chanctx_cnt_iter,
								   &arg);
			if (arg.n_vifs <= 1 || arg.n_vifs == curr_ctx_n_vifs)
				goto update_vif_chan;

			if (ar->csa_active_cnt)
				goto next_ctx;

			arg.vifs = kcalloc(arg.n_vifs, sizeof(arg.vifs[0]), GFP_KERNEL);
			if (!arg.vifs)
				return -ENOBUFS;

			arg.vifs_bridge_link_id =
				kcalloc(arg.n_vifs, sizeof(arg.vifs_bridge_link_id[0]), GFP_KERNEL);
			if (!arg.vifs_bridge_link_id) {
				kfree(arg.vifs);
				return -ENOBUFS;
			}

			ieee80211_iterate_active_interfaces_atomic(ar->ah->hw,
								   IEEE80211_IFACE_ITER_NORMAL,
								   ath12k_mac_change_chanctx_fill_iter,
								   &arg);

			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"mac chanctx switch n_vifs %d curr_ctx_n_vifs %d mode %d\n",
					arg.n_vifs, curr_ctx_n_vifs, mode);
			ath12k_mac_process_update_vif_chan(ar, arg.vifs, arg.vifs_bridge_link_id, arg.n_vifs);

			kfree(arg.vifs);
			kfree(arg.vifs_bridge_link_id);
			goto next_ctx;
		}

update_vif_chan:
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"mac chanctx switch n_vifs %d mode %d\n",
					i - next_ctx_idx + 1, mode);
			ath12k_mac_process_update_vif_chan(ar, vifs + next_ctx_idx, NULL,
							   i - next_ctx_idx + 1);
next_ctx:
			next_ctx_idx = i + 1;
			curr_ctx_n_vifs = 0;
	}
	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_switch_vif_chanctx);

static int
ath12k_set_vdev_param_to_all_vifs(struct ath12k *ar, int param, u32 value)
{
	struct ath12k_link_vif *arvif;
	int ret = 0;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "setting mac vdev %d param %d value %d\n",
			   param, arvif->vdev_id, value);

		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    param, value);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set param %d for vdev %d: %d\n",
				    param, arvif->vdev_id, ret);
			break;
		}
	}

	return ret;
}

/* mac80211 stores device specific RTS/Fragmentation threshold value,
 * this is set interface specific to firmware from ath12k driver
 */
int ath12k_mac_op_set_rts_threshold(struct ieee80211_hw *hw, u8 radio_id,
				    u32 value, struct ieee80211_vif *vif,
				    u32 link_id)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;
	int param_id = WMI_VDEV_PARAM_RTS_THRESHOLD, ret = 0, i;

	lockdep_assert_wiphy(hw->wiphy);

	/* Currently we set the rts threshold value to all the vifs across
	 * all radios of the single wiphy.
	 * TODO Once support for vif specific RTS threshold in mac80211 is
	 * available, ath12k can make use of it.
	 */
	for_each_ar(ah, ar, i) {
		ret = ath12k_set_vdev_param_to_all_vifs(ar, param_id, value);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set RTS config for all vdevs of pdev %d",
				    ar->pdev->pdev_id);
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_set_rts_threshold);

int ath12k_mac_op_set_frag_threshold(struct ieee80211_hw *hw, u32 value)
{
	/* Even though there's a WMI vdev param for fragmentation threshold no
	 * known firmware actually implements it. Moreover it is not possible to
	 * rely frame fragmentation to mac80211 because firmware clears the
	 * "more fragments" bit in frame control making it impossible for remote
	 * devices to reassemble frames.
	 *
	 * Hence implement a dummy callback just to say fragmentation isn't
	 * supported. This effectively prevents mac80211 from doing frame
	 * fragmentation in software.
	 */

	lockdep_assert_wiphy(hw->wiphy);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ath12k_mac_op_set_frag_threshold);

static int ath12k_mac_flush(struct ath12k *ar)
{
	int num_tx_pending = atomic_read(&ar->dp.num_tx_pending);
	long time_left;
	int ret = 0;

	time_left = wait_event_timeout(ar->dp.tx_empty_waitq,
				       (atomic_read(&ar->dp.num_tx_pending) == 0),
				       ar->ah->num_radio * ATH12K_FLUSH_TIMEOUT);
	if (time_left == 0) {
		ath12k_warn(ar->ab,
			    "failed to flush transmit queue, data pkts req %d pending %d\n",
			    num_tx_pending,
			    atomic_read(&ar->dp.num_tx_pending));
		ret = -ETIMEDOUT;
	}

	time_left = wait_event_timeout(ar->txmgmt_empty_waitq,
				       (atomic_read(&ar->num_pending_mgmt_tx) == 0),
				       ATH12K_FLUSH_TIMEOUT);
	if (time_left == 0) {
		ath12k_warn(ar->ab,
			    "failed to flush mgmt transmit queue, mgmt pkts pending %d\n",
			    atomic_read(&ar->num_pending_mgmt_tx));
		ret = -ETIMEDOUT;
	}

	return ret;
}

int ath12k_mac_wait_tx_complete(struct ath12k *ar)
{
	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_mac_drain_tx(ar);
	return ath12k_mac_flush(ar);
}

void ath12k_mac_op_flush(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			 u32 queues, bool drop)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	unsigned long links;
	struct ath12k *ar;
	u8 link_id;
	int i;

	lockdep_assert_wiphy(hw->wiphy);

	if (drop)
		return;

	/* vif can be NULL when flush() is considered for hw */
	if (!vif) {
		for_each_ar(ah, ar, i)
			ath12k_mac_flush(ar);
		return;
	}

	for_each_ar(ah, ar, i)
		wiphy_work_flush(hw->wiphy, &ar->wmi_mgmt_tx_work);

	ahvif = ath12k_vif_to_ahvif(vif);
	links = ahvif->links_map;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
		if (!(arvif && arvif->ar))
			continue;

		ath12k_mac_flush(arvif->ar);
	}
}
EXPORT_SYMBOL(ath12k_mac_op_flush);

static bool
ath12k_mac_has_single_legacy_rate(struct ath12k *ar,
				  enum nl80211_band band,
				  const struct cfg80211_bitrate_mask *mask)
{
	int num_rates = 0;

	num_rates = hweight32(mask->control[band].legacy);

	if (ath12k_mac_bitrate_mask_num_ht_rates(ar, band, mask))
		return false;

	if (ath12k_mac_bitrate_mask_num_vht_rates(ar, band, mask))
		return false;

	if (ath12k_mac_bitrate_mask_num_he_rates(ar, band, mask))
		return false;

	if (ath12k_mac_bitrate_mask_num_eht_rates(ar, band, mask))
		return false;

	return num_rates == 1;
}

static __le16
ath12k_mac_get_tx_mcs_map(const struct ieee80211_sta_he_cap *he_cap)
{
	if (he_cap->he_cap_elem.phy_cap_info[0] &
	    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G)
		return he_cap->he_mcs_nss_supp.tx_mcs_160;

	return he_cap->he_mcs_nss_supp.tx_mcs_80;
}

static bool
ath12k_mac_bitrate_mask_get_single_nss(struct ath12k *ar,
				       struct ieee80211_vif *vif,
				       enum nl80211_band band,
				       const struct cfg80211_bitrate_mask *mask,
				       int *nss)
{
	struct ieee80211_supported_band *sband = &ar->mac.sbands[band];
	u16 vht_mcs_map = le16_to_cpu(sband->vht_cap.vht_mcs.tx_mcs_map);
	const struct ieee80211_sta_he_cap *he_cap;
	u16 he_mcs_map = 0;
	u16 eht_mcs_map = 0;
	u8 ht_nss_mask = 0;
	u8 vht_nss_mask = 0;
	u8 he_nss_mask = 0;
	u8 eht_nss_mask = 0;
	u8 mcs_nss_len;
	int i;

	/* No need to consider legacy here. Basic rates are always present
	 * in bitrate mask
	 */

	for (i = 0; i < ARRAY_SIZE(mask->control[band].ht_mcs); i++) {
		if (mask->control[band].ht_mcs[i] == 0)
			continue;
		else if (mask->control[band].ht_mcs[i] ==
			 sband->ht_cap.mcs.rx_mask[i])
			ht_nss_mask |= BIT(i);
		else
			return false;
	}

	for (i = 0; i < ARRAY_SIZE(mask->control[band].vht_mcs); i++) {
		if (mask->control[band].vht_mcs[i] == 0)
			continue;
		else if (mask->control[band].vht_mcs[i] ==
			 ath12k_mac_get_max_vht_mcs_map(vht_mcs_map, i))
			vht_nss_mask |= BIT(i);
		else
			return false;
	}

	he_cap = ieee80211_get_he_iftype_cap_vif(sband, vif);
	if (!he_cap)
		return false;

	he_mcs_map = le16_to_cpu(ath12k_mac_get_tx_mcs_map(he_cap));

	for (i = 0; i < ARRAY_SIZE(mask->control[band].he_mcs); i++) {
		if (mask->control[band].he_mcs[i] == 0)
			continue;

		if (mask->control[band].he_mcs[i] ==
		    ath12k_mac_get_max_he_mcs_map(he_mcs_map, i))
			he_nss_mask |= BIT(i);
		else
			return false;
	}

	mcs_nss_len = ieee80211_eht_mcs_nss_size(&sband->iftype_data->he_cap.he_cap_elem,
						 &sband->iftype_data->eht_cap.eht_cap_elem,
						 false);
	if (mcs_nss_len == 4) {
		/* 20 MHz only STA case */
		const struct ieee80211_eht_mcs_nss_supp_20mhz_only *eht_mcs_nss =
			&sband->iftype_data->eht_cap.eht_mcs_nss_supp.only_20mhz;
		if (eht_mcs_nss->rx_tx_mcs13_max_nss)
			eht_mcs_map = 0x1fff;
		else if (eht_mcs_nss->rx_tx_mcs11_max_nss)
			eht_mcs_map = 0x07ff;
		else if (eht_mcs_nss->rx_tx_mcs9_max_nss)
			eht_mcs_map = 0x01ff;
		else
			eht_mcs_map = 0x007f;
	} else {
		const struct ieee80211_eht_mcs_nss_supp_bw *eht_mcs_nss;

		switch (mcs_nss_len) {
		case 9:
			eht_mcs_nss = &sband->iftype_data->eht_cap.eht_mcs_nss_supp.bw._320;
			break;
		case 6:
			eht_mcs_nss = &sband->iftype_data->eht_cap.eht_mcs_nss_supp.bw._160;
			break;
		case 3:
			eht_mcs_nss = &sband->iftype_data->eht_cap.eht_mcs_nss_supp.bw._80;
			break;
		default:
			return false;
		}

		if (eht_mcs_nss->rx_tx_mcs13_max_nss)
			eht_mcs_map = 0x1fff;
		else if (eht_mcs_nss->rx_tx_mcs11_max_nss)
			eht_mcs_map = 0x7ff;
		else
			eht_mcs_map = 0x1ff;
	}

	for (i = 0; i < ARRAY_SIZE(mask->control[band].eht_mcs); i++) {
		if (mask->control[band].eht_mcs[i] == 0)
			continue;

		if (mask->control[band].eht_mcs[i] < eht_mcs_map)
			eht_nss_mask |= BIT(i);
		else
			return false;
	}

	if (ht_nss_mask != vht_nss_mask || ht_nss_mask != he_nss_mask ||
	    ht_nss_mask != eht_nss_mask)
		return false;

	if (ht_nss_mask == 0)
		return false;

	if (BIT(fls(ht_nss_mask)) - 1 != ht_nss_mask)
		return false;

	*nss = fls(ht_nss_mask);

	return true;
}

static int
ath12k_mac_get_single_legacy_rate(struct ath12k *ar,
				  enum nl80211_band band,
				  const struct cfg80211_bitrate_mask *mask,
				  u32 *rate, u8 *nss)
{
	int rate_idx;
	u16 bitrate;
	u8 preamble;
	u8 hw_rate;

	if (hweight32(mask->control[band].legacy) != 1)
		return -EINVAL;

	rate_idx = ffs(mask->control[band].legacy) - 1;

	if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ)
		rate_idx += ATH12K_MAC_FIRST_OFDM_RATE_IDX;

	hw_rate = ath12k_legacy_rates[rate_idx].hw_value;
	bitrate = ath12k_legacy_rates[rate_idx].bitrate;

	if (ath12k_mac_bitrate_is_cck(bitrate))
		preamble = WMI_RATE_PREAMBLE_CCK;
	else
		preamble = WMI_RATE_PREAMBLE_OFDM;

	*nss = 1;
	*rate = ATH12K_HW_RATE_CODE(hw_rate, 0, preamble);

	return 0;
}

static int
ath12k_mac_set_fixed_rate_gi_ltf(struct ath12k_link_vif *arvif, u8 gi, u8 ltf)
{
	struct ieee80211_bss_conf *link_conf;
	struct ath12k *ar = arvif->ar;
	bool eht_support;
	int ret, param;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	rcu_read_lock();
	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!link_conf) {
		rcu_read_unlock();
		return -EINVAL;
	}

	eht_support = link_conf->eht_support;
	rcu_read_unlock();

	/* 0.8 = 0, 1.6 = 2 and 3.2 = 3. */
	if (gi && gi != 0xFF)
		gi += 1;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_SGI, gi);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set HE GI:%d, error:%d\n",
			    gi, ret);
		return ret;
	}
	/* start from 1 */
	if (ltf != 0xFF)
		ltf += 1;

        if (eht_support)
                param = WMI_VDEV_PARAM_EHT_LTF;
        else
                param = WMI_VDEV_PARAM_HE_LTF;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    param, ltf);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set LTF:%d, error:%d\n",
			    ltf, ret);
		return ret;
	}
	return 0;
}

static int
ath12k_mac_set_auto_rate_gi_ltf(struct ath12k_link_vif *arvif, u16 gi, u8 ltf)
{
	struct ath12k *ar = arvif->ar;
	int ret;
	u32 ar_gi_ltf;

	if (gi != 0xFF) {
		switch (gi) {
		case NL80211_RATE_INFO_HE_GI_0_8:
			gi = WMI_AUTORATE_800NS_GI;
			break;
		case NL80211_RATE_INFO_HE_GI_1_6:
			gi = WMI_AUTORATE_1600NS_GI;
			break;
		case NL80211_RATE_INFO_HE_GI_3_2:
			gi = WMI_AUTORATE_3200NS_GI;
			break;
		default:
			ath12k_warn(ar->ab, "Invalid GI\n");
			return -EINVAL;
		}
	}

	if (ltf != 0xFF) {
		switch (ltf) {
		case NL80211_RATE_INFO_HE_1XLTF:
			ltf = WMI_HE_AUTORATE_LTF_1X;
			break;
		case NL80211_RATE_INFO_HE_2XLTF:
			ltf = WMI_HE_AUTORATE_LTF_2X;
			break;
		case NL80211_RATE_INFO_HE_4XLTF:
			ltf = WMI_HE_AUTORATE_LTF_4X;
			break;
		default:
			ath12k_warn(ar->ab, "Invalid LTF\n");
			return -EINVAL;
		}
	}

	ar_gi_ltf = gi | ltf;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_AUTORATE_MISC_CFG,
					    ar_gi_ltf);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to set HE autorate GI:%u, LTF:%u params, error:%d\n",
			    gi, ltf, ret);
		return ret;
	}

	return 0;
}

static void ath12k_mac_vdev_ml_max_rec_links(struct ath12k_link_vif *arvif,
					     u8 ml_max_rec_links)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ath12k *ar = arvif->ar;
	u32 vdev_param;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!ieee80211_vif_is_mld(vif)) {
		ath12k_err(ar->ab, "Vdev %d is non-MLO, RMSL config is not allowed\n",
			   arvif->vdev_id);
		return;
	}

	vdev_param = WMI_VDEV_PARAM_MLO_MAX_RECOM_ACTIVE_LINKS;
	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac vdev %d max ML recommended links %u\n",
		   arvif->vdev_id, ml_max_rec_links);

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, ml_max_rec_links);
	if (ret)
		ath12k_warn(ar->ab, "failed to send max ml recom active links for vdev %d: %d\n",
			    arvif->vdev_id, ret);
}

static u32 ath12k_mac_nlgi_to_wmigi(enum nl80211_txrate_gi gi)
{
	switch (gi) {
	case NL80211_TXRATE_DEFAULT_GI:
		return WMI_GI_400_NS;
	case NL80211_TXRATE_FORCE_LGI:
		return WMI_GI_800_NS;
	default:
		return WMI_GI_400_NS;
	}
}

static int ath12k_mac_set_rate_params(struct ath12k_link_vif *arvif,
				      u32 rate, u8 nss, u8 sgi, u8 ldpc,
				      u8 he_gi, u8 he_ltf, bool he_fixed_rate,
				      u8 eht_gi, u8 eht_ltf,
				      bool eht_fixed_rate,
				      int he_ul_rate, u8 he_ul_nss)
{
	struct ieee80211_bss_conf *link_conf;
	struct ath12k *ar = arvif->ar;
	u32 vdev_param;
	u32 param_value, rate_code;
	int ret;
	bool he_support, eht_support;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!link_conf)
		return -EINVAL;

	he_support = link_conf->he_support;
	eht_support = link_conf->eht_support;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"mac set rate params vdev %i rate 0x%02x nss 0x%02x sgi 0x%02x ldpc 0x%02x\n",
			arvif->vdev_id, rate, nss, sgi, ldpc);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"he_gi 0x%02x he_ltf 0x%02x he_fixed_rate %d\n", he_gi,
			he_ltf, he_fixed_rate);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			"eht_gi:0x%02x, eht_ltf:0x%02x, eht_fixed_rate:%d\n", eht_gi,
			eht_ltf, eht_fixed_rate);

	if (!he_support || !eht_support) {
		vdev_param = WMI_VDEV_PARAM_FIXED_RATE;
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    vdev_param, rate);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set fixed rate param 0x%02x: %d\n",
				    rate, ret);
			return ret;
		}
	}

	vdev_param = WMI_VDEV_PARAM_NSS;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, nss);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set nss param %d: %d\n",
			    nss, ret);
		return ret;
	}

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    WMI_VDEV_PARAM_LDPC, ldpc);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set ldpc param %d: %d\n",
			    ldpc, ret);
		return ret;
	}

	if (eht_support) {
		if (eht_fixed_rate)
			ret = ath12k_mac_set_fixed_rate_gi_ltf(arvif, eht_gi, eht_ltf);
		else
			ret = ath12k_mac_set_auto_rate_gi_ltf(arvif, eht_gi, eht_ltf);
		if (ret)
			return ret;
	} else if (he_support) {
		if (he_fixed_rate)
			ret = ath12k_mac_set_fixed_rate_gi_ltf(arvif, he_gi, he_ltf);
		else
			ret = ath12k_mac_set_auto_rate_gi_ltf(arvif, he_gi, he_ltf);
		if (ret)
			return ret;
	} else {
		vdev_param = WMI_VDEV_PARAM_SGI;
		param_value = ath12k_mac_nlgi_to_wmigi(sgi);
		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
						    vdev_param, param_value);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set sgi param %d: %d\n",
				    sgi, ret);
			return ret;
		}
	}

	if ((he_ul_rate < 0) || !he_ul_nss)
		return 0;

	rate_code = ATH12K_HW_RATE_CODE(he_ul_rate, he_ul_nss - 1,
					WMI_RATE_PREAMBLE_HE);

	vdev_param = WMI_VDEV_PARAM_UL_FIXED_RATE;
	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
					    vdev_param, rate_code);

	if (ret) {
		ath12k_warn(ar->ab, "failed to set HE UL Fixed Rate:%d, error:%d\n",
			    he_ul_rate, ret);
	}

	return 0;
}

static bool
ath12k_mac_vht_mcs_range_present(struct ath12k *ar,
				 enum nl80211_band band,
				 const struct cfg80211_bitrate_mask *mask)
{
	int i;
	u16 vht_mcs;

	for (i = 0; i < NL80211_VHT_NSS_MAX; i++) {
		vht_mcs = mask->control[band].vht_mcs[i];

		switch (vht_mcs) {
		case 0:
		case BIT(8) - 1:
		case BIT(9) - 1:
		case BIT(10) - 1:
			break;
		default:
			return false;
		}
	}

	return true;
}

static bool
ath12k_mac_he_ul_mcs_present(struct ath12k *ar,
				enum nl80211_band band,
				const struct cfg80211_bitrate_mask *mask)
{
	int i;

	for (i = 0; i < NL80211_HE_NSS_MAX; i++) {
		if (mask->control[band].he_ul_mcs[i])
			return true;
	}

	return false;
}

static bool
ath12k_mac_he_mcs_range_present(struct ath12k *ar,
				enum nl80211_band band,
				const struct cfg80211_bitrate_mask *mask)
{
	int i;
	u16 he_mcs;

	for (i = 0; i < NL80211_HE_NSS_MAX; i++) {
		he_mcs = mask->control[band].he_mcs[i];

		switch (he_mcs) {
		case 0:
		case BIT(8) - 1:
		case BIT(10) - 1:
		case BIT(12) - 1:
			break;
		default:
			return false;
		}
	}

	return true;
}

static bool
ath12k_mac_eht_mcs_range_present(struct ath12k *ar,
				 enum nl80211_band band,
				 const struct cfg80211_bitrate_mask *mask)
{
	int i;
	u16 eht_mcs;

	for (i = 0; i < NL80211_EHT_NSS_MAX; i++) {
		eht_mcs = mask->control[band].eht_mcs[i];

		switch (eht_mcs) {
		case 0:
		case BIT(8) - 1:
		case BIT(10) - 1:
		case BIT(12) - 1:
		case BIT(14) - 1:
			break;
		case BIT(15) - 1:
		case BIT(16) - BIT(14) - 1:
			if (i != 0)
				return false;
			break;
		default:
			return false;
		}
	}

	return true;
}

static void ath12k_mac_set_bitrate_mask_iter(void *data,
					     struct ieee80211_sta *sta)
{
	struct ath12k_link_vif *arvif = data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta;
	struct ath12k *ar = arvif->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				  ahsta->link[arvif->link_id]);
	if (!arsta || arsta->arvif != arvif)
		return;

	spin_lock_bh(&ar->data_lock);
	arsta->changed |= IEEE80211_RC_SUPP_RATES_CHANGED;
	spin_unlock_bh(&ar->data_lock);

	wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &arsta->update_wk);
}

static void ath12k_mac_disable_peer_fixed_rate(void *data,
					       struct ieee80211_sta *sta)
{
	struct ath12k_link_vif *arvif = data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta;
	struct ath12k *ar = arvif->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				  ahsta->link[arvif->link_id]);

	if (!arsta || arsta->arvif != arvif)
		return;

	spin_lock_bh(&ar->data_lock);
	arsta->disable_fixed_rate = true;
	spin_unlock_bh(&ar->data_lock);

	wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &arsta->update_wk);
}

static bool
ath12k_mac_validate_fixed_rate_settings(struct ath12k *ar, enum nl80211_band band,
					const struct cfg80211_bitrate_mask *mask,
					unsigned int link_id)
{
	bool eht_fixed_rate = false, he_fixed_rate = false, vht_fixed_rate = false;
	const u16 *vht_mcs_mask, *he_mcs_mask, *eht_mcs_mask, *he_ul_mcs_mask;
	struct ieee80211_link_sta *link_sta;
	struct ath12k_dp_link_peer *peer, *tmp;
	bool he_ul_fixed_rate = false;
	u8 vht_nss, he_nss, eht_nss, he_ul_nss;
	int ret = true;

	vht_mcs_mask = mask->control[band].vht_mcs;
	he_mcs_mask = mask->control[band].he_mcs;
	eht_mcs_mask = mask->control[band].eht_mcs;
	he_ul_mcs_mask = mask->control[band].he_ul_mcs;

	if (ath12k_mac_bitrate_mask_num_vht_rates(ar, band, mask) == 1)
		vht_fixed_rate = true;

	if (ath12k_mac_bitrate_mask_num_he_rates(ar, band, mask) == 1)
		he_fixed_rate = true;

	if (ath12k_mac_bitrate_mask_num_eht_rates(ar, band, mask) == 1)
		eht_fixed_rate = true;

	if (ath12k_mac_bitrate_mask_num_he_ul_rates(ar, band, mask) == 1)
		he_ul_fixed_rate = true;

	if (!vht_fixed_rate && !he_fixed_rate && !eht_fixed_rate && !he_ul_fixed_rate)
		return true;

	vht_nss = ath12k_mac_max_vht_nss(vht_mcs_mask);
	he_nss =  ath12k_mac_max_he_nss(he_mcs_mask);
	eht_nss = ath12k_mac_max_eht_nss(eht_mcs_mask);
	he_ul_nss =  ath12k_mac_max_he_nss(he_ul_mcs_mask);

	rcu_read_lock();
	spin_lock_bh(&ar->ab->dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ar->ab->dp->peers, list) {
		if (peer->sta) {
			link_sta = rcu_dereference(peer->sta->link[link_id]);
			if (!link_sta) {
				ret = false;
				goto exit;
			}

			if (vht_fixed_rate && (!link_sta->vht_cap.vht_supported ||
					       link_sta->rx_nss < vht_nss)) {
				ret = false;
				goto exit;
			}
			if (he_fixed_rate && (!link_sta->he_cap.has_he ||
					      link_sta->rx_nss < he_nss)) {
				ret = false;
				goto exit;
			}
			if (eht_fixed_rate && (!link_sta->eht_cap.has_eht ||
					       link_sta->rx_nss < eht_nss)) {
				ret = false;
				goto exit;
			}
			/* TODO:
			*	check when UL is valid
			*/
			if (he_ul_fixed_rate && (!link_sta->he_cap.has_he ||
					link_sta->rx_nss < he_ul_nss)) {
				ret = false;
				goto exit;
			}
		}
	}
exit:
	spin_unlock_bh(&ar->ab->dp->dp_lock);
	rcu_read_unlock();
	return ret;
}

int ath12k_is_mcs_rate_changed(enum nl80211_band band,
			       const struct cfg80211_bitrate_mask *user_mask)
{
	if (user_mask->control[band].legacy_mcs_changed ||
	    user_mask->control[band].ht_mcs_changed ||
	    user_mask->control[band].vht_mcs_changed ||
	    user_mask->control[band].he_mcs_changed ||
	    user_mask->control[band].he_ul_mcs_changed ||
	    user_mask->control[band].eht_mcs_changed)
		return 1;

	return 0;
}

int
ath12k_mac_op_set_bitrate_mask(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif, unsigned int link_id,
			       const struct cfg80211_bitrate_mask *mask)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct cfg80211_chan_def def;
	struct ath12k *ar;
	enum nl80211_band band;
	const u8 *ht_mcs_mask;
	const u16 *vht_mcs_mask;
	const u16 *he_mcs_mask;
	const u16 *eht_mcs_mask;
	const u16 *he_ul_mcs_mask;
	u8 he_ltf = 0;
	u8 he_gi = 0;
	u8 eht_ltf = 0;
	u8 eht_gi = 0;
	u32 rate;
	u8 nss, mac_nss, he_ul_nss = 0;
	u8 sgi;
	u8 ldpc;
	int single_nss;
	int ret, i;
	int num_rates;
	int he_ul_rate = -1;
	bool he_fixed_rate = false;
	bool eht_fixed_rate = false;

	lockdep_assert_wiphy(hw->wiphy);

	arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);

	ar = arvif->ar;
	if (ath12k_mac_vif_link_chan(vif, arvif->link_id, &def)) {
		ret = -EPERM;
		goto out;
	}

	band = def.chan->band;
	ht_mcs_mask = mask->control[band].ht_mcs;
	vht_mcs_mask = mask->control[band].vht_mcs;
	he_mcs_mask = mask->control[band].he_mcs;
	eht_mcs_mask = mask->control[band].eht_mcs;
	ldpc = !!(ar->ht_cap_info & WMI_HT_CAP_LDPC);
	he_ul_mcs_mask = mask->control[band].he_ul_mcs;

	sgi = mask->control[band].gi;
	if (sgi == NL80211_TXRATE_FORCE_SGI) {
		ret = -EINVAL;
		goto out;
	}

	he_gi = mask->control[band].he_gi;
	he_ltf = mask->control[band].he_ltf;

	eht_gi = mask->control[band].eht_gi;
	eht_ltf = mask->control[band].eht_ltf;

	for (i = 0; i < ARRAY_SIZE(mask->control[band].he_ul_mcs); i++) {
		if (hweight16(mask->control[band].he_ul_mcs[i]) == 1) {
			he_ul_nss = i + 1;
			he_ul_rate = ffs((int)
					mask->control[band].he_ul_mcs[i]) - 1;
			break;
		}
	}
	num_rates = ath12k_mac_bitrate_mask_num_he_ul_rates(ar, band,
			mask);
	if (ath12k_mac_he_ul_mcs_present(ar, band, mask) &&
			num_rates != 1) {
		ath12k_warn(ar->ab,
				"Setting HE UL MCS Fixed Rate range is not supported\n");
		return -EINVAL;
	}

	/* mac80211 doesn't support sending a fixed HT/VHT MCS alone, rather it
	 * requires passing at least one of used basic rates along with them.
	 * Fixed rate setting across different preambles(legacy, HT, VHT) is
	 * not supported by the FW. Hence use of FIXED_RATE vdev param is not
	 * suitable for setting single HT/VHT rates.
	 * But, there could be a single basic rate passed from userspace which
	 * can be done through the FIXED_RATE param.
	 */
	if (ath12k_mac_has_single_legacy_rate(ar, band, mask)) {
		ret = ath12k_mac_get_single_legacy_rate(ar, band, mask, &rate,
							&nss);
		if (ret) {
			ath12k_warn(ar->ab, "failed to get single legacy rate for vdev %i: %d\n",
				    arvif->vdev_id, ret);
			goto out;
		}

		if(!ath12k_is_mcs_rate_changed(band, mask))
			goto skip_mcs_set;

		ieee80211_iterate_stations_mtx(hw,
					       ath12k_mac_disable_peer_fixed_rate,
					       arvif);
	} else if (ath12k_mac_bitrate_mask_get_single_nss(ar, vif, band, mask,
							  &single_nss)) {
		rate = WMI_FIXED_RATE_NONE;
		nss = single_nss;
		arvif->bitrate_mask = *mask;

		if(!ath12k_is_mcs_rate_changed(band, mask))
			goto skip_mcs_set;

		ieee80211_iterate_stations_atomic(hw,
						  ath12k_mac_set_bitrate_mask_iter,
						  arvif);
	} else {
		rate = WMI_FIXED_RATE_NONE;

		if (!ath12k_mac_check_fixed_rate_settings_for_mumimo(arvif, vht_mcs_mask, he_mcs_mask))
			return -EINVAL;

		if (!ath12k_mac_validate_fixed_rate_settings(ar, band,
							     mask, arvif->link_id))
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"failed to update fixed rate settings due to mcs/nss incompatibility\n");

		mac_nss = max3(ath12k_mac_max_ht_nss(ht_mcs_mask),
			       ath12k_mac_max_vht_nss(vht_mcs_mask),
			       ath12k_mac_max_he_nss(he_mcs_mask));
		mac_nss = max(mac_nss,
			      ath12k_mac_max_eht_nss(eht_mcs_mask));
		nss = min_t(u32, ar->num_tx_chains, mac_nss);

		/* If multiple rates across different preambles are given
		 * we can reconfigure this info with all peers using PEER_ASSOC
		 * command with the below exception cases.
		 * - Single VHT Rate : peer_assoc command accommodates only MCS
		 * range values i.e 0-7, 0-8, 0-9 for VHT. Though mac80211
		 * mandates passing basic rates along with HT/VHT rates, FW
		 * doesn't allow switching from VHT to Legacy. Hence instead of
		 * setting legacy and VHT rates using RATEMASK_CMD vdev cmd,
		 * we could set this VHT rate as peer fixed rate param, which
		 * will override FIXED rate and FW rate control algorithm.
		 * If single VHT rate is passed along with HT rates, we select
		 * the VHT rate as fixed rate for vht peers.
		 * - Multiple VHT Rates : When Multiple VHT rates are given,this
		 * can be set using RATEMASK CMD which uses FW rate-ctl alg.
		 * TODO: Setting multiple VHT MCS and replacing peer_assoc with
		 * RATEMASK_CMDID can cover all use cases of setting rates
		 * across multiple preambles and rates within same type.
		 * But requires more validation of the command at this point.
		 */

		num_rates = ath12k_mac_bitrate_mask_num_vht_rates(ar, band,
								  mask);

		if (!ath12k_mac_vht_mcs_range_present(ar, band, mask) &&
		    num_rates > 1) {
			/* TODO: Handle multiple VHT MCS values setting using
			 * RATEMASK CMD
			 */
			ath12k_warn(ar->ab,
				    "Setting more than one MCS Value in bitrate mask not supported\n");
			ret = -EINVAL;
			goto out;
		}

		num_rates = ath12k_mac_bitrate_mask_num_he_rates(ar, band, mask);
		if (num_rates == 1)
			he_fixed_rate = true;

		if (!ath12k_mac_he_mcs_range_present(ar, band, mask) &&
		    num_rates > 1) {
			ath12k_warn(ar->ab,
				    "Setting more than one HE MCS Value in bitrate mask not supported\n");
			ret = -EINVAL;
			goto out;
		}

                num_rates = ath12k_mac_bitrate_mask_num_eht_rates(ar, band, mask);
		if (num_rates == 1)
			eht_fixed_rate = true;

		if (!ath12k_mac_eht_mcs_range_present(ar, band, mask) &&
		    num_rates > 1) {
			ath12k_warn(ar->ab,
				    "Setting more than one EHT MCS Value in bitrate mask not supported\n");
			ret =-EINVAL;
			goto out;
		}

		if(!ath12k_is_mcs_rate_changed(band, mask))
			goto skip_mcs_set;

		ieee80211_iterate_stations_mtx(hw,
					       ath12k_mac_disable_peer_fixed_rate,
					       arvif);

		arvif->bitrate_mask = *mask;
		ieee80211_iterate_stations_mtx(hw,
					       ath12k_mac_set_bitrate_mask_iter,
					       arvif);
	}

skip_mcs_set:
	ret = ath12k_mac_set_rate_params(arvif, rate, nss, sgi, ldpc, he_gi,
					 he_ltf, he_fixed_rate, eht_gi, eht_ltf,
					 eht_fixed_rate, he_ul_rate, he_ul_nss);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set fixed rate params on vdev %i: %d\n",
			    arvif->vdev_id, ret);
	}

out:
	return ret;
}
EXPORT_SYMBOL(ath12k_mac_op_set_bitrate_mask);

void
ath12k_mac_reconfig_complete(struct ieee80211_hw *hw,
			     enum ieee80211_reconfig_type reconfig_type)
{
        struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
        struct ath12k *ar;
        struct ath12k_base *ab;
        struct ath12k_vif *ahvif;
        struct ath12k_link_vif *arvif;
        int recovery_count, i;

	lockdep_assert_wiphy(hw->wiphy);

	if (reconfig_type != IEEE80211_RECONFIG_TYPE_RESTART)
		return;

	guard(mutex)(&ah->hw_mutex);

	if (ah->state != ATH12K_HW_STATE_RESTARTED)
		return;

	ah->state = ATH12K_HW_STATE_ON;

	/* stop_queues() & wake_queues() will take care to stop/wake
	 * all the queues. So checking on queue 0's status before
	 * waking up should be fine.
	 */

	if (ieee80211_queue_stopped(ah->hw, 0))
		ieee80211_wake_queues(hw);

	for_each_ar(ah, ar, i) {
		ab = ar->ab;

		if (!ab->is_reset)
			continue;

		ath12k_warn(ar->ab, "pdev %d successfully recovered\n",
			    ar->pdev->pdev_id);

		if (ar->ab->hw_params->current_cc_support &&
		    ar->alpha2[0] != 0 && ar->alpha2[1] != 0) {
			struct wmi_set_current_country_arg arg = {};

			memcpy(&arg.alpha2, ar->alpha2, 2);
			ath12k_wmi_send_set_current_country_cmd(ar, &arg);
		}


		recovery_count = atomic_inc_return(&ab->recovery_count);

		ath12k_dbg(ab, ATH12K_DBG_BOOT, "recovery count %d\n",
			   recovery_count);

		/* When there are multiple radios in an SOC,
		 * the recovery has to be done for each radio
		 */
		if (recovery_count == ab->num_radios) {
			atomic_dec(&ab->reset_count);
			complete(&ab->reset_complete);
			ab->is_reset = false;
			atomic_set(&ab->fail_cont_count, 0);
			clear_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags);
			spin_lock_bh(&ar->ab->base_lock);
			ar->ab->stats.last_recovery_time =
				jiffies_to_msecs(jiffies -
						ar->ab->recovery_start_time);
			spin_unlock_bh(&ar->ab->base_lock);
			ath12k_dbg(ab, ATH12K_DBG_BOOT, "reset success\n");
		}
		if (ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0) {
			list_for_each_entry(arvif, &ar->arvifs, list) {
				ahvif = arvif->ahvif;
				ath12k_dbg(ab, ATH12K_DBG_BOOT,
					   "reconfig cipher %d up %d vdev type %d\n",
					   arvif->key_cipher,
					   arvif->is_up,
					   ahvif->vdev_type);

				/* After trigger disconnect, then upper layer will
				 * trigger connect again, then the PN number of
				 * upper layer will be reset to keep up with AP
				 * side, hence PN number mismatch will not happen.
				 */
				if (arvif->is_up &&
				    ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
				    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_NONE) {
					ieee80211_hw_restart_disconnect(ahvif->vif);

					ath12k_dbg(ab, ATH12K_DBG_BOOT, "restart disconnect\n");
				}
			}
		}

		ath12k_erp_handle_ssr(ar);
	}

	ath12k_reconfig_qos_profiles(ab);
	clear_bit(ATH12K_GROUP_FLAG_RECOVERY, &ar->ab->ag->flags);

	/* Send WMI_FW_HANG_CMD to FW after target has started. This is to
	 * update the target's SSR recovery mode after it has recovered.
	 */
	ath12k_send_fw_hang_cmd(ab, ab->fw_recovery_support);

	ath12k_info(NULL, "HW group recovery flag cleared ag dev_flags:0x%lx\n",
		    ar->ab->ag->flags);

	if (ar->commitatf) {
		if (ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_DYNAMIC_ENABLE,
					      ar->commitatf, ar->pdev->pdev_id)) {
			ath12k_warn(ar->ab, "ATF: failed to enable ATF\n");
		} else {
			ar->commitatf = false;
		}
	}

	if (ar->atf_strict_scheduling) {
		if (ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_STRICT_SCH,
					      ar->atf_strict_scheduling,
					      ar->pdev->pdev_id)) {
			ath12k_warn(ar->ab, "ATF: failed to enable strict scheduling\n");
		} else {
			ar->atf_strict_scheduling = false;
		}
	}

	if (ath12k_erp_get_sm_state() == ATH12K_ERP_ENTER_COMPLETE)
		ieee80211_queue_work(hw, &ar->ssr_erp_exit);
}

void
ath12k_mac_op_reconfig_complete(struct ieee80211_hw *hw,
				enum ieee80211_reconfig_type reconfig_type)
{
	ath12k_mac_reconfig_complete(hw, reconfig_type);
}
EXPORT_SYMBOL(ath12k_mac_op_reconfig_complete);

static int
ath12k_mac_set_mscs(struct ieee80211_hw *hw, struct ath12k_link_sta *arsta,
		    struct ath12k_sta *ahsta,
		    struct cfg80211_qm_req_data *qm_req,
		    struct cfg80211_qm_resp_data *qm_resp)
{
	struct cfg80211_qm_req_desc_data *qm_req_desc = &qm_req->qm_req_desc[0];
	struct cfg80211_qm_resp_desc_data *qm_resp_desc = &qm_resp->qm_resp_desc[0];
	struct ath12k_dp_link_peer *link_peer;
	u8 req_type = qm_req_desc->request_type;
	struct ath12k_dp_peer *peer;
	struct ath12k_dp *dp;
	struct ath12k_dp_vif *dp_vif;
	struct ath12k *ar;

	ar = arsta->arvif->ar;
	dp = ath12k_ab_to_dp(ar->ab);

	qm_resp_desc->qm_id = qm_req_desc->qm_id;

	spin_lock_bh(&dp->dp_lock);
	link_peer = ath12k_dp_link_peer_find_by_addr(dp, arsta->addr);
	if (!link_peer)
		goto send_fail_resp;

	peer = link_peer->dp_peer;

	if (!peer)
		goto send_fail_resp;

	dp_vif = &ahsta->ahvif->dp_vif;

	switch (req_type) {
	case IEEE80211_QM_ADD_REQ:
		if (peer->mscs_session_exists)
			goto send_fail_resp;
		peer->mscs_session_exists = true;
		dp_vif->mscs_hlos_tid_override = true;
		fallthrough;
	case IEEE80211_QM_CHANGE_REQ:
		peer->mscs_ctxt.user_priority_bitmap =
			qm_req_desc->user_priority_bitmap;
		peer->mscs_ctxt.user_priority_limit =
			qm_req_desc->user_priority_limit;
		peer->mscs_ctxt.tclas_mask =
			qm_req_desc->tclas_mask;

		ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
			   "MSCS: %s: peer %pM, bmap 0x%x, limit %u mask 0x%x",
			   (req_type == IEEE80211_QM_CHANGE_REQ) ? "CHANGE" :
			   "ADD",
			   peer->addr,
			   peer->mscs_ctxt.user_priority_bitmap,
			   peer->mscs_ctxt.user_priority_limit,
			   peer->mscs_ctxt.tclas_mask);
		break;
	case IEEE80211_QM_REMOVE_REQ:
		peer->mscs_session_exists = false;
		dp_vif->mscs_hlos_tid_override = false;
		ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
			   "MSCS: REMOVE peer %pM, mscs_session_exists %u",
			   peer->addr, peer->mscs_session_exists);
		break;
	default:
		goto send_fail_resp;
	}

	spin_unlock_bh(&dp->dp_lock);

	qm_resp_desc->status = IEEE80211_QM_REQ_SUCCESS;
	return 0;

send_fail_resp:
	spin_unlock_bh(&dp->dp_lock);
	qm_resp_desc->status = IEEE80211_QM_REQ_DECLINED;
	return -EINVAL;
}

static void
ath12k_mac_update_bss_chan_survey(struct ath12k *ar,
				  struct ieee80211_channel *channel)
{
	int ret;
	enum wmi_bss_chan_info_req_type type = WMI_BSS_SURVEY_REQ_TYPE_READ;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (!test_bit(WMI_TLV_SERVICE_BSS_CHANNEL_INFO_64, ar->ab->wmi_ab.svc_map) ||
	    ar->rx_channel != channel)
		return;

	if (ar->scan.state != ATH12K_SCAN_IDLE) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "ignoring bss chan info req while scanning..\n");
		return;
	}

	reinit_completion(&ar->bss_survey_done);

	ret = ath12k_wmi_pdev_bss_chan_info_request(ar, type);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send pdev bss chan info request\n");
		return;
	}

	ret = wait_for_completion_timeout(&ar->bss_survey_done, 3 * HZ);
	if (ret == 0)
		ath12k_warn(ar->ab, "bss channel survey timed out\n");
}

int ath12k_mac_op_get_survey(struct ieee80211_hw *hw, int idx,
			     struct survey_info *survey)
{
	struct ath12k *ar;
	struct ieee80211_supported_band *sband;
	struct survey_info *ar_survey;

	lockdep_assert_wiphy(hw->wiphy);

	if (idx >= ATH12K_NUM_CHANS)
		return -ENOENT;

	sband = hw->wiphy->bands[NL80211_BAND_2GHZ];
	if (sband && idx >= sband->n_channels) {
		idx -= sband->n_channels;
		sband = NULL;
	}

	if (!sband)
		sband = hw->wiphy->bands[NL80211_BAND_5GHZ];
	if (sband && idx >= sband->n_channels) {
		idx -= sband->n_channels;
		sband = NULL;
	}

	if (!sband)
		sband = hw->wiphy->bands[NL80211_BAND_6GHZ];

	if (!sband || idx >= sband->n_channels)
		return -ENOENT;

	ar = ath12k_mac_get_ar_by_chan(hw, &sband->channels[idx]);
	if (!ar) {
		if (sband->channels[idx].flags & IEEE80211_CHAN_DISABLED) {
			memset(survey, 0, sizeof(*survey));
			return 0;
		}
		return -ENOENT;
	}

	ar_survey = &ar->survey[idx];

	ath12k_mac_update_bss_chan_survey(ar, &sband->channels[idx]);

	spin_lock_bh(&ar->data_lock);
	memcpy(survey, ar_survey, sizeof(*survey));
	spin_unlock_bh(&ar->data_lock);

	survey->channel = &sband->channels[idx];

	if (ar->rx_channel == survey->channel)
		survey->filled |= SURVEY_INFO_IN_USE;

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_get_survey);

static void ath12k_mac_put_chain_rssi(struct station_info *sinfo,
				      struct ath12k_link_sta *arsta)
{
	s8 rssi;
	int i;

	for (i = 0; i < ARRAY_SIZE(sinfo->chain_signal); i++) {
		sinfo->chains &= ~BIT(i);
		rssi = arsta->chain_signal[i];

		if (rssi != ATH12K_DEFAULT_NOISE_FLOOR &&
		    rssi != ATH12K_INVALID_RSSI_FULL &&
		    rssi != ATH12K_INVALID_RSSI_EMPTY &&
		    rssi != 0) {
			sinfo->chain_signal[i] = rssi;
			sinfo->chains |= BIT(i);
			sinfo->filled |= BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL);
		}
	}
}

int ath12k_mac_btcoex_config(struct ath12k *ar, struct ath12k_link_vif *arvif,
			     int coex, u32 wlan_prio_mask, u8 wlan_weight)
{
	struct ieee80211_hw *hw = ar->ah->hw;
	struct coex_config_arg coex_config;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	if (coex == BTCOEX_CONFIGURE_DEFAULT || (test_bit(ATH12K_FLAG_BTCOEX, &ar->dev_flags) ^ coex)) {
		goto next;
	}

	coex_config.vdev_id = arvif->vdev_id;
	if (coex == BTCOEX_ENABLE) {
		coex_config.config_type = WMI_COEX_CONFIG_PTA_INTERFACE;
		coex_config.pta_num = ar->coex.pta_num;
		coex_config.coex_mode = ar->coex.coex_mode;
		coex_config.bt_txrx_time = ar->coex.bt_active_time_slot;
		coex_config.bt_priority_time = ar->coex.bt_priority_time_slot;
		coex_config.pta_algorithm = ar->coex.coex_algo_type;
		coex_config.pta_priority = ar->coex.pta_priority;
		ret = ath12k_send_coex_config_cmd(ar, &coex_config);
		if (ret) {
			ath12k_warn(ar->ab,
				    "failed to set coex config vdev_id %d ret %d\n",
				    coex_config.vdev_id, ret);
			goto out;
		}
	}

	memset(&coex_config, 0, sizeof(struct coex_config_arg));
	coex_config.vdev_id = arvif->vdev_id;
	coex_config.config_type = WMI_COEX_CONFIG_BTC_ENABLE;
	coex_config.coex_enable = coex;
	ret = ath12k_send_coex_config_cmd(ar, &coex_config);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to set coex config vdev_id %d ret %d\n",
			    coex_config.vdev_id, ret);
		goto out;
	}

next:
	if (!coex) {
		ret = 0;
		goto out;
	}

	memset(&coex_config, 0, sizeof(struct coex_config_arg));
	coex_config.vdev_id = arvif->vdev_id;
	coex_config.config_type = WMI_COEX_CONFIG_WLAN_PKT_PRIORITY;
	coex_config.wlan_pkt_type = wlan_prio_mask;
	coex_config.wlan_pkt_weight = wlan_weight;
	ret = ath12k_send_coex_config_cmd(ar, &coex_config);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to set coex config vdev_id %d ret %d\n",
			    coex_config.vdev_id, ret);
	}
out:
	return ret;
}

void ath12k_mac_op_link_sta_statistics(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_link_sta *link_sta,
				       struct link_station_info *link_sinfo)
{
	struct ath12k_sta *ahsta;
	struct ath12k_dp_link_peer_rate_info rate_info = {0};
	struct ath12k_fw_stats_req_params params = {};
	s8 signal, rssi_signal, rssi_offset;
	struct ath12k_link_sta *arsta;
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	struct ath12k *ar;
	bool db2dbm;

	if (!link_sta->sta) {
		ath12k_err(NULL, "Failed to proceed: link_sta->sta is NULL");
		return;
	}

	ahsta = ath12k_sta_to_ahsta(link_sta->sta);
	lockdep_assert_wiphy(hw->wiphy);

	arsta = wiphy_dereference(hw->wiphy, ahsta->link[link_sta->link_id]);

	if (!arsta)
		return;

	ar = ath12k_get_ar_by_vif(hw, vif, arsta->link_id);
	if (!ar)
		return;

	ab = ar->ab;
	if (!ab) {
		ath12k_err(NULL,
			   "unable to determine link sta statistics \n");
		return;
	}

	dp = ath12k_ab_to_dp(ab);
	ath12k_link_peer_get_sta_rate_info_stats(dp, arsta->addr, &rate_info);

	db2dbm = test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
			  ar->ab->wmi_ab.svc_map);

	link_sinfo->rx_duration = rate_info.rx_duration;
	link_sinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_DURATION);

	spin_lock_bh(&dp->dp_lock);
	link_sinfo->tx_duration = rate_info.tx_duration;
	link_sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_DURATION);

	if (rate_info.txrate.legacy || rate_info.txrate.nss) {
		if (rate_info.txrate.legacy) {
			link_sinfo->txrate.legacy = rate_info.txrate.legacy;
		} else {
			link_sinfo->txrate.mcs = rate_info.txrate.mcs;
			link_sinfo->txrate.nss = rate_info.txrate.nss;
			link_sinfo->txrate.bw = rate_info.txrate.bw;
			link_sinfo->txrate.he_gi = rate_info.txrate.he_gi;
			link_sinfo->txrate.he_dcm = rate_info.txrate.he_dcm;
			link_sinfo->txrate.he_ru_alloc =
				rate_info.txrate.he_ru_alloc;
			link_sinfo->txrate.eht_gi = rate_info.txrate.eht_gi;
			link_sinfo->txrate.eht_ru_alloc =
				rate_info.txrate.eht_ru_alloc;
		}
		link_sinfo->txrate.flags = rate_info.txrate.flags;
		link_sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_BITRATE);
	}

	rssi_offset = rate_info.rssi_comb + ar->rssi_offsets.avg_nf_dbm;
	rssi_signal = rate_info.rssi_comb > ar->rssi_offsets.xlna_bypass_threshold ?
		      rssi_offset + ar->rssi_offsets.xlna_bypass_offset :
		      rssi_offset;

	signal = rate_info.rssi_comb;
	spin_unlock_bh(&dp->dp_lock);

	if (ahsta->ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		/* Limit the requests to Firmware for fetching the signal strength */
		if (time_after(jiffies, msecs_to_jiffies
						(ATH12K_PDEV_SIGNAL_UPDATE_TIME_MSECS) +
						 ar->last_signal_update)) {
			params.pdev_id = ar->pdev->pdev_id;
			params.vdev_id = 0;
			params.stats_id = WMI_REQUEST_VDEV_STAT;

			ath12k_mac_get_fw_stats(ar, &params);
			ar->last_signal_update = jiffies;
		}
		if (!signal)
			signal = arsta->rssi_beacon;
	}

	spin_lock_bh(&dp->dp_lock);
	if (signal) {
		link_sinfo->signal =
			db2dbm ? rate_info.rssi_comb : rssi_signal;
		link_sinfo->filled |= BIT_ULL(NL80211_STA_INFO_SIGNAL);
	}

	link_sinfo->signal_avg =
		rate_info.signal_avg + (!db2dbm ? ar->rssi_offsets.rssi_offset : 0);

	link_sinfo->filled |= BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG);

	link_sinfo->tx_retries = rate_info.tx_retry_count;
	link_sinfo->tx_failed = rate_info.tx_retry_failed;
	link_sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_RETRIES);
	link_sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_FAILED);
	spin_unlock_bh(&dp->dp_lock);
}
EXPORT_SYMBOL(ath12k_mac_op_link_sta_statistics);

void ath12k_mac_op_sta_statistics(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct ieee80211_sta *sta,
				  struct station_info *sinfo)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_fw_stats_req_params params = {};
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;
	struct ath12k_base *ab;
	s8 signal, rssi_signal, rssi_offset;
	bool db2dbm;
	struct ath12k_dp *dp;
	struct ath12k_dp_link_peer_rate_info rate_info = {0};

	lockdep_assert_wiphy(hw->wiphy);

	arsta = &ahsta->deflink;

	ar = ath12k_get_ar_by_vif(hw, vif, arsta->link_id);
	if (!ar)
		return;

	ab = ar->ab;
	if (!ab) {
		ath12k_err(NULL,
			   "unable to determine sta statistics \n");
		return;
	}

	dp = ath12k_ab_to_dp(ab);
	ath12k_link_peer_get_sta_rate_info_stats(dp, arsta->addr, &rate_info);

	db2dbm = test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
			  ab->wmi_ab.svc_map);

	sinfo->rx_duration = rate_info.rx_duration;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_DURATION);

	spin_lock_bh(&dp->dp_lock);
	sinfo->tx_duration = rate_info.tx_duration;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_DURATION);

	if (rate_info.txrate.legacy || rate_info.txrate.nss) {
		if (rate_info.txrate.legacy) {
			sinfo->txrate.legacy = rate_info.txrate.legacy;
		} else {
			sinfo->txrate.mcs = rate_info.txrate.mcs;
			sinfo->txrate.nss = rate_info.txrate.nss;
			sinfo->txrate.bw = rate_info.txrate.bw;
			sinfo->txrate.he_gi = rate_info.txrate.he_gi;
			sinfo->txrate.he_dcm = rate_info.txrate.he_dcm;
			sinfo->txrate.he_ru_alloc = rate_info.txrate.he_ru_alloc;
			sinfo->txrate.eht_gi = rate_info.txrate.eht_gi;
			sinfo->txrate.eht_ru_alloc = rate_info.txrate.eht_ru_alloc;
		}
		sinfo->txrate.flags = rate_info.txrate.flags;
		sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_BITRATE);
	}

	rssi_offset = rate_info.rssi_comb + ar->rssi_offsets.avg_nf_dbm;
	rssi_signal = rate_info.rssi_comb > ar->rssi_offsets.xlna_bypass_threshold ?
		      rssi_offset + ar->rssi_offsets.xlna_bypass_offset :
		      rssi_offset;

	signal = rate_info.rssi_comb;
	spin_unlock_bh(&dp->dp_lock);

	if (ahsta->ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		/* Limit the requests to Firmware for fetching the signal strength */
		if (time_after(jiffies, msecs_to_jiffies
						(ATH12K_PDEV_SIGNAL_UPDATE_TIME_MSECS) +
						 ar->last_signal_update)) {
			params.pdev_id = ar->pdev->pdev_id;
			params.vdev_id = 0;
			params.stats_id = WMI_REQUEST_VDEV_STAT;
			ath12k_mac_get_fw_stats(ar, &params);
			ar->last_signal_update = jiffies;
		}

		if (!signal)
			signal = arsta->rssi_beacon;
	}

	if (!(sinfo->filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) &&
	    ahsta->ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		ath12k_mac_put_chain_rssi(sinfo, arsta);

	spin_lock_bh(&dp->dp_lock);
	if (signal) {
		sinfo->signal = db2dbm ? rate_info.rssi_comb : rssi_signal;
		sinfo->filled |= BIT_ULL(NL80211_STA_INFO_SIGNAL);
	}

	sinfo->signal_avg = rate_info.signal_avg;

	if (!db2dbm)
		sinfo->signal_avg += ar->rssi_offsets.rssi_offset;

	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_SIGNAL_AVG);

	sinfo->rx_retries = rate_info.rx_retries;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_RETRIES);

	sinfo->tx_retries = rate_info.tx_retry_count;
	sinfo->tx_failed = rate_info.tx_retry_failed;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_RETRIES);
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_FAILED);
	spin_unlock_bh(&dp->dp_lock);
}
EXPORT_SYMBOL(ath12k_mac_op_sta_statistics);

int ath12k_mac_op_cancel_remain_on_channel(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	u8 link_id =  ahvif->roc_link_id;

	arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
	if (!arvif || !arvif->is_created) {
		ath12k_err(NULL, "unable to cancel scan. arvif interface is not created\n");
		return -EINVAL;
	}

	ar = arvif->ar;
	if (!ar) {
		ath12k_err(NULL, "unable to select device to cancel scan\n");
		return -EINVAL;
	}

	lockdep_assert_wiphy(hw->wiphy);

	spin_lock_bh(&ar->data_lock);
	ar->scan.roc_notify = false;
	spin_unlock_bh(&ar->data_lock);

	ath12k_scan_abort(ar);

	cancel_delayed_work_sync(&ar->scan.timeout);

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_cancel_remain_on_channel);

int ath12k_mac_op_remain_on_channel(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct cfg80211_chan_def *chandef,
				    int duration,
				    enum ieee80211_roc_type type)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ieee80211_chanctx_conf ctx = {0};
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;
	struct ath12k *ar;
	struct ieee80211_channel *chan;
	u32 scan_time_msec;
	bool create = true;
	u8 link_id;
	int ret;

	lockdep_assert_wiphy(hw->wiphy);

	if (!chandef || !chandef->chan) {
		ath12k_err(NULL, "%s: null chandef!\n", __func__);
		return -EINVAL;
	}

	chan = chandef->chan;
	ar = ath12k_mac_select_scan_device(hw, vif, chan->center_freq);
	if (!ar)
		return -EINVAL;

	ab = ar->ab;
	if (!test_bit(WMI_TLV_SERVICE_SCAN_PHYMODE_SUPPORT,
		      ab->wmi_ab.svc_map)) {
		ath12k_err(ab, "ROC feature not supported!\n");
		return -EOPNOTSUPP;
	}

	/* check if any of the links of ML VIF is already started on
	 * radio(ar) correpsondig to given scan frequency and use it,
	 * if not use deflink(link 0) for scan purpose.
	 */

	link_id = ath12k_mac_find_link_id_by_ar(ahvif, ar);
	if (link_id == ATH12K_DEFAULT_SCAN_LINK)
		link_id = 0;
	arvif = ath12k_mac_assign_link_vif(ah, vif, link_id, false);

	if (!arvif) {
		ath12k_err(ab, "Failed to alloc/assign link vif id %u\n",
			   link_id);
		return -ENOMEM;
	}
	/* If the vif is already assigned to a specific vdev of an ar,
	 * check whether its already started, vdev which is started
	 * are not allowed to switch to a new radio.
	 * If the vdev is not started, but was earlier created on a
	 * different ar, delete that vdev and create a new one. We don't
	 * delete at the scan stop as an optimization to avoid redundant
	 * delete-create vdev's for the same ar, in case the request is
	 * always on the same band for the vif
	 */
	if (arvif->is_created) {
		if (WARN_ON(!arvif->ar))
			return -EINVAL;

		if (ar != arvif->ar && arvif->is_started && !arvif->is_scan_vif)
			return -EBUSY;

		if (ar != arvif->ar && arvif->is_started && arvif->is_scan_vif) {
			ret = ath12k_mac_vdev_stop(arvif);
			if (ret) {
				ath12k_err(ab, "Failed to stop vdev in ROC:%d\n", ret);
				return ret;
			}
			arvif->is_started = false;
		}
		if (ar != arvif->ar) {
			ath12k_mac_remove_link_interface(hw, arvif);
			ath12k_mac_unassign_link_vif(arvif);
		} else {
			create = false;
		}
	}

	if (create) {
		arvif = ath12k_mac_assign_link_vif(ah, vif, link_id, false);

		if (!arvif) {
			ath12k_err(ab, "Failed to alloc/assign link vif id %u\n",
				   link_id);
			return -ENOMEM;
		}

		arvif->is_scan_vif = true;
		ret = ath12k_mac_vdev_create(ar, arvif, false);
		if (ret) {
			ath12k_warn(ab, "unable to create scan vdev for roc: %d\n",
				    ret);
			return ret;
		}
	}

	ahvif = arvif->ahvif;
	if (!arvif->is_started && ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		ctx.def.chan = chan;
		ctx.def.center_freq1 = chan->center_freq;
		ret = ath12k_mac_vdev_start(arvif, &ctx);
		if (ret) {
			ath12k_err(ar->ab,
				   "vdev start failed for ROC STA ret: %d\n",
				   ret);
			return ret;
		}
		ar->scan.arvif = arvif;
		arvif->is_started = true;
	}

	spin_lock_bh(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
		reinit_completion(&ar->scan.started);
		reinit_completion(&ar->scan.completed);
		reinit_completion(&ar->scan.on_channel);
		ar->scan.state = ATH12K_SCAN_STARTING;
		cancel_delayed_work(&ar->scan.roc_done);
		ieee80211_queue_delayed_work(hw, &ar->scan.roc_done,
					     msecs_to_jiffies(duration +
					     ATH12K_SCAN_ROC_CLEANUP_TIMEOUT_MS));
		ar->scan.is_roc = true;
		ar->scan.arvif = arvif;
		ar->scan.roc_freq = chan->center_freq;
		ar->scan.roc_notify = true;
		ret = 0;
		break;
	case ATH12K_SCAN_STARTING:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ret = -EBUSY;
		break;
	}

	spin_unlock_bh(&ar->data_lock);

	if (ret) {
		ath12k_warn(ab, "roc scan state %d, not idle\n", ar->scan.state);
		return ret;
	}

	scan_time_msec = hw->wiphy->max_remain_on_channel_duration * 2;

	struct ath12k_wmi_scan_req_arg *arg __free(kfree) =
					kzalloc(sizeof(*arg), GFP_KERNEL);
	if (!arg)
		return -ENOMEM;

	ath12k_wmi_start_scan_init(ar, arg, vif->type);

	arg->chan_list.num_chan = 1;
	struct chan_info *chaninfo __free(kfree) = kcalloc(arg->chan_list.num_chan,
							   sizeof(struct chan_info),
							   GFP_KERNEL);
	if (!chaninfo) {
		ath12k_err(ab, "chan list memory allocation failed\n");
		return -ENOMEM;
	}

	arg->chan_list.chan = chaninfo;
	arg->chan_list.chan[0].freq = chan->center_freq;
	arg->chan_list.chan[0].phymode = ath12k_phymodes[chandef->chan->band][chandef->width];

	/* Wide Band Scan is required for bandwidth > 20_NoHT mode */
	if (chandef->width > NL80211_CHAN_WIDTH_20_NOHT) {
		arg->scan_f_wide_band = true;
		arg->chandef = chandef;
		ret = ath12k_wmi_update_scan_chan_list(ar, arg);
		if (ret) {
			ath12k_err(ab,"unable to update scan list:%d\n", ret);
			return ret;
		}
	}

	arg->vdev_id = arvif->vdev_id;
	arg->scan_id = ATH12K_ROC_SCAN_ID;
	arg->dwell_time_active = scan_time_msec;
	arg->dwell_time_passive = scan_time_msec;
	arg->max_scan_time = scan_time_msec;
	arg->scan_f_passive = 1;
	arg->scan_f_filter_prb_req = 1;

	/*these flags enables fw to tx offchan frame to unknown STA*/
	arg->scan_f_offchan_mgmt_tx = 1;
	arg->scan_f_offchan_data_tx = 1;

	arg->burst_duration = duration;

	ret = ath12k_start_scan(ar, arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to start roc scan: %d\n", ret);
		spin_lock_bh(&ar->data_lock);
		ar->scan.state = ATH12K_SCAN_IDLE;
		spin_unlock_bh(&ar->data_lock);
		return ret;
	}

	ret = wait_for_completion_timeout(&ar->scan.on_channel, 3 * HZ);
	if (ret == 0) {
		ath12k_warn(ar->ab, "failed to switch to channel for roc scan\n");
		ret = ath12k_scan_stop(ar);
		if (ret)
			ath12k_warn(ar->ab, "failed to stop scan: %d\n", ret);
		return -ETIMEDOUT;
	}
	ahvif->roc_link_id = link_id;

	ieee80211_queue_delayed_work(hw, &ar->scan.timeout,
				     msecs_to_jiffies(scan_time_msec));

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_remain_on_channel);

void ath12k_mac_op_set_rekey_data(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct cfg80211_gtk_rekey_data *data)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_rekey_data *rekey_data;
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar = ath12k_ah_to_ar(ah, 0);
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(hw->wiphy);

	arvif = &ahvif->deflink;
	rekey_data = &arvif->rekey_data;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac set rekey data vdev %d\n",
		   arvif->vdev_id);

	memcpy(rekey_data->kck, data->kck, NL80211_KCK_LEN);
	memcpy(rekey_data->kek, data->kek, NL80211_KEK_LEN);

	/* The supplicant works on big-endian, the firmware expects it on
	 * little endian.
	 */
	rekey_data->replay_ctr = get_unaligned_be64(data->replay_ctr);

	arvif->rekey_data.enable_offload = true;

	ath12k_dbg_dump(ar->ab, ATH12K_DBG_MAC, "kck", NULL,
			rekey_data->kck, NL80211_KCK_LEN);
	ath12k_dbg_dump(ar->ab, ATH12K_DBG_MAC, "kek", NULL,
			rekey_data->kck, NL80211_KEK_LEN);
	ath12k_dbg_dump(ar->ab, ATH12K_DBG_MAC, "replay ctr", NULL,
			&rekey_data->replay_ctr, sizeof(rekey_data->replay_ctr));
}
EXPORT_SYMBOL(ath12k_mac_op_set_rekey_data);

int ath12k_mac_op_erp(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		      int link_id, struct cfg80211_erp_params *params)
{
	lockdep_assert_wiphy(hw->wiphy);

	switch (params->cmd) {
		case CFG80211_ERP_CMD_ENTER:
			return ath12k_erp_enter(hw, vif, link_id, params);
		case CFG80211_ERP_CMD_EXIT:
			return ath12k_erp_exit(hw->wiphy, false);
		default:
			return -EINVAL;
	}
}
EXPORT_SYMBOL(ath12k_mac_op_erp);

/**
 * ath12k_disable_chans_outside_limit - disable channels outside the freq limits
 * @ch_lst: list of channels to check
 * @num_chans: number of channels in the list
 * @freq_low: lower frequency limit
 * @freq_high: upper frequency limit
 *
 * This function will set the IEEE80211_CHAN_DISABLED flag for channels
 * whose center frequency is outside the given frequency limits.
 */
static void
ath12k_disable_chans_outside_limit(struct ieee80211_channel *ch_lst,
				   int num_chans, u32 freq_low, u32 freq_high)
{
	int i;

	if (!ch_lst || num_chans == 0)
		return;

	for (i = 0; i < num_chans; i++) {
		if (ch_lst[i].center_freq < freq_low ||
		    ch_lst[i].center_freq > freq_high)
			ch_lst[i].flags |= IEEE80211_CHAN_DISABLED;
	}
}

void ath12k_mac_update_freq_range(struct ath12k *ar,
				  u32 freq_low, u32 freq_high)
{
	if (!(freq_low && freq_high))
		return;

	ar->chan_info.low_freq = freq_low;
	ar->chan_info.high_freq = freq_high;

	if (ar->freq_range.start_freq || ar->freq_range.end_freq) {
		ar->freq_range.start_freq = min(ar->freq_range.start_freq,
						MHZ_TO_KHZ(freq_low));
		ar->freq_range.end_freq = max(ar->freq_range.end_freq,
					      MHZ_TO_KHZ(freq_high));
	} else {
		ar->freq_range.start_freq = MHZ_TO_KHZ(freq_low);
		ar->freq_range.end_freq = MHZ_TO_KHZ(freq_high);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "mac pdev %u freq limit updated. New range %u->%u MHz\n",
		   ar->pdev->pdev_id, KHZ_TO_MHZ(ar->freq_range.start_freq),
		   KHZ_TO_MHZ(ar->freq_range.end_freq));
}

/**
 * ath12k_mac_update_ch_list - disable band chans outside the given frequency
 * @ar: pointer to ath12k structure
 * @band: pointer to the band structure
 * @freq_low: lower frequency limit
 * @freq_high: upper frequency limit
 *
 * This function will set the IEEE80211_CHAN_DISABLED flag for channels
 * whose center frequency is outside the given frequency limits.
 * For 6 GHz band, disable channels in all power modes channel lists.
 */
static void ath12k_mac_update_ch_list(struct ath12k *ar,
				      struct ieee80211_supported_band *band,
				      u32 freq_low, u32 freq_high)
{
	int i;

	if (!(freq_low && freq_high))
		return;

	ath12k_disable_chans_outside_limit(band->channels, band->n_channels,
					   freq_low, freq_high);

	if (band->band != NL80211_BAND_6GHZ)
		return;

	for (i = 0; i < NL80211_REG_NUM_POWER_MODES; i++) {
		const struct ieee80211_6ghz_channel *chan_6g = band->chan_6g[i];

		if (!chan_6g)
			continue;

		ath12k_disable_chans_outside_limit(chan_6g->channels, chan_6g->n_channels,
						   freq_low, freq_high);
	}
}

#define ATH12K_5_9_MIN_FREQ 5845
#define ATH12K_5_9_MAX_FREQ 5885

static void ath12k_mac_update_5_9_ch_list(struct ath12k *ar,
					  struct ieee80211_supported_band *band)
{
	int i;

	if (test_bit(WMI_TLV_SERVICE_5_9GHZ_SUPPORT, ar->ab->wmi_ab.svc_map))
		return;

	if (ar->ab->dfs_region != ATH12K_DFS_REG_FCC)
		return;

	for (i = 0; i < band->n_channels; i++) {
		if (band->channels[i].center_freq >= ATH12K_5_9_MIN_FREQ &&
		    band->channels[i].center_freq <= ATH12K_5_9_MAX_FREQ)
			band->channels[i].flags |= IEEE80211_CHAN_DISABLED;
	}
}

static u32 ath12k_get_phy_id(struct ath12k *ar, u32 band)
{
	struct ath12k_pdev *pdev = ar->pdev;
	struct ath12k_pdev_cap *pdev_cap = &pdev->cap;

	if (band == WMI_HOST_WLAN_2GHZ_CAP)
	return pdev_cap->band[NL80211_BAND_2GHZ].phy_id;

	if (band == WMI_HOST_WLAN_5GHZ_CAP)
		return pdev_cap->band[NL80211_BAND_5GHZ].phy_id;

	ath12k_warn(ar->ab, "unsupported phy cap:%d\n", band);

	return 0;
}

/**
 * ath12k_update_band_channels - Update split channels of same band
 * @new_ch_lst: list of channels in the new band
 * @old_ch_lst: list of channels in the orig band
 * @num_chans: number of channels in the list
 *
 * This function will enable channels in the orig_band which are
 * enabled in the new_band.
 * Note: This function assumes that the new_band and orig_band are of the
 * same band.
 *
 * Returns 0 on success, negative error code on failure.
 */
static int
ath12k_update_band_channels(const struct ieee80211_channel *new_ch_lst,
			    struct ieee80211_channel *old_ch_lst, int num_chans)
{
	int i;

	for (i = 0; i < num_chans; i++) {
		if (new_ch_lst[i].flags & IEEE80211_CHAN_DISABLED)
			continue;

		/* An enabled channel in new_band should not be already enabled
		 * in the orig_band
		 */
		if (WARN_ON(!(old_ch_lst[i].flags & IEEE80211_CHAN_DISABLED)))
			return -ENOTRECOVERABLE;

		old_ch_lst[i].flags &= ~IEEE80211_CHAN_DISABLED;
	}

	return 0;
}

static int ath12k_mac_update_band(struct ath12k *ar,
				  struct ieee80211_supported_band *orig_band,
				  struct ieee80211_supported_band *new_band)
{
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_6ghz_channel *chan_6g_old, *chan_6g_new;
	int i, ret;

	if (!orig_band || !new_band)
		return -EINVAL;

	if (orig_band->band != new_band->band)
		return -EINVAL;

	if (WARN_ON(!ab->ag->mlo_capable))
		return -EOPNOTSUPP;

	ret = ath12k_update_band_channels(new_band->channels,
					  orig_band->channels,
					  new_band->n_channels);
	if (ret)
		return ret;

	if (new_band->band != NL80211_BAND_6GHZ)
		return 0;

	for (i = 0; i < NL80211_REG_NUM_POWER_MODES; i++) {
		chan_6g_new = new_band->chan_6g[i];
		chan_6g_old = orig_band->chan_6g[i];
		if (!chan_6g_new || !chan_6g_old)
			continue;

		ret = ath12k_update_band_channels(chan_6g_new->channels,
						  chan_6g_old->channels,
						  chan_6g_new->n_channels);
		if (ret)
			return ret;
	}

	return 0;
}

static int ath12k_mac_setup_channels_rates(struct ath12k *ar,
					   u32 supported_bands,
					   struct ieee80211_supported_band *bands[])
{
	struct ath12k_base *ab = ar->ab;
	struct ieee80211_supported_band *band;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *reg_cap;
	struct ath12k_hw *ah = ar->ah;
	void *channels;
	u32 phy_id, freq_low, freq_high;
	struct ieee80211_6ghz_channel *chan_6g;
	int ret, i = 0;

	BUILD_BUG_ON((ARRAY_SIZE(ath12k_2ghz_channels) +
		      ARRAY_SIZE(ath12k_5ghz_channels) +
		      ARRAY_SIZE(ath12k_6ghz_channels)) !=
		     ATH12K_NUM_CHANS);

	reg_cap = &ab->hal_reg_cap[ar->pdev_idx];

	if (supported_bands & WMI_HOST_WLAN_2GHZ_CAP) {
		channels = kmemdup(ath12k_2ghz_channels,
				   sizeof(ath12k_2ghz_channels),
				   GFP_KERNEL);
		if (!channels)
			return -ENOMEM;

		band = &ar->mac.sbands[NL80211_BAND_2GHZ];
		band->band = NL80211_BAND_2GHZ;
		band->n_channels = ARRAY_SIZE(ath12k_2ghz_channels);
		band->channels = channels;
		band->n_bitrates = ath12k_g_rates_size;
		band->bitrates = ath12k_g_rates;

		if (ab->hw_params->single_pdev_only) {
			phy_id = ath12k_get_phy_id(ar, WMI_HOST_WLAN_2GHZ_CAP);
			reg_cap = &ab->hal_reg_cap[phy_id];
		}

		freq_low = max(reg_cap->low_2ghz_chan,
			       ab->reg_freq_2g.start_freq);
		freq_high = min(reg_cap->high_2ghz_chan,
				ab->reg_freq_2g.end_freq);

		ath12k_mac_update_ch_list(ar, band,
					  reg_cap->low_2ghz_chan,
					  reg_cap->high_2ghz_chan);

		ath12k_mac_update_freq_range(ar, freq_low, freq_high);

		ar->num_channels = ath12k_reg_get_num_chans_in_band(ar, band);
		if (!bands[NL80211_BAND_2GHZ]) {
			bands[NL80211_BAND_2GHZ] = band;
		} else {
			/* Split mac in same band under same wiphy during MLO */
			ret = ath12k_mac_update_band(ar,
						     bands[NL80211_BAND_2GHZ],
						     band);
			if (ret)
				return ret;
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
					"mac pdev %u identified as 2 GHz split mac during MLO\n",
					ar->pdev->pdev_id);
		}
	}

	if (supported_bands & WMI_HOST_WLAN_5GHZ_CAP) {
		/* If 5g end and 6g start overlaps, decide band based on
		 * the difference between target limit and ATH12K_5G_MAX_CENTER
		 */

		if (ab->wide_band && (reg_cap->low_5ghz_chan < ATH12K_MIN_6GHZ_FREQ &&
				      reg_cap->high_5ghz_chan > ATH12K_MAX_5GHZ_FREQ)) {
			/* Wide band radio can operate in either 5GHz or 6GHz,
			 * configuring the band in which the radio has to operate,
			 * default band is set to 5GHz.
			 */
			if (ab->wide_band == ATH12K_WIDE_BAND_6GHZ) {
				reg_cap->low_5ghz_chan = ATH12K_MIN_6GHZ_FREQ;
				ath12k_info(ab, "Wide band radio coming up in 6GHz band");
			} else {
				reg_cap->high_5ghz_chan = ATH12K_MAX_5GHZ_FREQ;
				ath12k_info(ab, "Wide band radio coming up in 5GHz band");
			}
		}

		if ((reg_cap->low_5ghz_chan >= ATH12K_MIN_5GHZ_FREQ) &&
		    ((reg_cap->high_5ghz_chan < ATH12K_MAX_5GHZ_FREQ) ||
		     ((reg_cap->high_5ghz_chan - ATH12K_5GHZ_MAX_CENTER) < (ATH12K_HALF_20MHZ_BW * 2)))) {
			channels = kmemdup(ath12k_5ghz_channels,
					   sizeof(ath12k_5ghz_channels), GFP_KERNEL);
			if (!channels) {
				kfree(ar->mac.sbands[NL80211_BAND_2GHZ].channels);
				ar->mac.sbands[NL80211_BAND_2GHZ].channels = NULL;
				ar->mac.sbands[NL80211_BAND_2GHZ].n_channels = 0;
				kfree(ar->mac.sbands[NL80211_BAND_6GHZ].channels);
				ar->mac.sbands[NL80211_BAND_6GHZ].channels = NULL;
				ar->mac.sbands[NL80211_BAND_6GHZ].n_channels = 0;
				for (i = 0; i < NL80211_REG_NUM_POWER_MODES; i++) {
					kfree(ar->mac.sbands[NL80211_BAND_6GHZ].chan_6g[i]);
					ar->mac.sbands[NL80211_BAND_6GHZ].chan_6g[i] = NULL;
				}

				return -ENOMEM;
			}

			band = &ar->mac.sbands[NL80211_BAND_5GHZ];
			band->band = NL80211_BAND_5GHZ;
			band->n_channels = ARRAY_SIZE(ath12k_5ghz_channels);
			band->channels = channels;
			band->n_bitrates = ath12k_a_rates_size;
			band->bitrates = ath12k_a_rates;

			if (ar->ab->hw_params->single_pdev_only)
				phy_id = ath12k_get_phy_id(ar, WMI_HOST_WLAN_5GHZ_CAP);

			freq_low = max(reg_cap->low_5ghz_chan,
				       ab->reg_freq_5g.start_freq);
			freq_high = min(reg_cap->high_5ghz_chan,
					ab->reg_freq_5g.end_freq);

			ath12k_mac_update_ch_list(ar, band,
						  reg_cap->low_5ghz_chan,
						  reg_cap->high_5ghz_chan);
			ath12k_mac_update_5_9_ch_list(ar, band);

			ath12k_mac_update_freq_range(ar, freq_low, freq_high);

			ar->num_channels = ath12k_reg_get_num_chans_in_band(ar, band);
			if (!bands[NL80211_BAND_5GHZ]) {
				bands[NL80211_BAND_5GHZ] = band;
			} else {
				/* Split mac in same band under same wiphy during MLO */
				ret = ath12k_mac_update_band(ar,
							     bands[NL80211_BAND_5GHZ],
							     band);
				if (ret)
					return ret;
				ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
					"mac pdev %u identified as 6 GHz split mac during MLO\n",
					   ar->pdev->pdev_id);
			}
		} else if (reg_cap->low_5ghz_chan >= ATH12K_MIN_6GHZ_FREQ &&
			   reg_cap->high_5ghz_chan <= ATH12K_MAX_6GHZ_FREQ) {
			band = &ar->mac.sbands[NL80211_BAND_6GHZ];
			band->band = NL80211_BAND_6GHZ;
			for (i = 0; i < NL80211_REG_NUM_POWER_MODES; i++) {
				channels = kmemdup(ath12k_6ghz_channels,
						   sizeof(ath12k_6ghz_channels),
						   GFP_KERNEL);
				chan_6g = kzalloc(sizeof(*chan_6g), GFP_ATOMIC);
				if (!channels || !chan_6g) {
					kfree(ar->mac.sbands[NL80211_BAND_2GHZ].channels);
					ar->mac.sbands[NL80211_BAND_2GHZ].channels = NULL;
					ar->mac.sbands[NL80211_BAND_2GHZ].n_channels = 0;
					kfree(ar->mac.sbands[NL80211_BAND_5GHZ].channels);
					ar->mac.sbands[NL80211_BAND_5GHZ].channels = NULL;
					ar->mac.sbands[NL80211_BAND_5GHZ].n_channels = 0;
					break;
				}
				chan_6g->channels = channels;
				chan_6g->n_channels = ARRAY_SIZE(ath12k_6ghz_channels);
				band->chan_6g[i] = chan_6g;
				channels = NULL;
				chan_6g = NULL;
			}

			if (i < NL80211_REG_NUM_POWER_MODES) {
				for (i = i - 1; i >= 0; i--) {
					chan_6g = band->chan_6g[i];
					kfree(chan_6g->channels);
					kfree(chan_6g);
					band->chan_6g[i] = NULL;
				}
				return -ENOMEM;
			}
			ar->supports_6ghz = true;
			band->n_bitrates = ath12k_a_rates_size;
			band->bitrates = ath12k_a_rates;
			band->channels = channels;
			band->n_channels = ARRAY_SIZE(ath12k_6ghz_channels);

			freq_low = max(reg_cap->low_5ghz_chan,
				       ab->reg_freq_6g.start_freq);
			freq_high = min(reg_cap->high_5ghz_chan,
					ab->reg_freq_6g.end_freq);

			ath12k_mac_update_ch_list(ar, band,
						  reg_cap->low_5ghz_chan,
						  reg_cap->high_5ghz_chan);

			ath12k_mac_update_freq_range(ar, freq_low, freq_high);
			band->n_channels = band->chan_6g[NL80211_REG_AP_LPI]->n_channels;
			band->channels = band->chan_6g[NL80211_REG_AP_LPI]->channels;
			ah->use_6ghz_regd = true;
			ar->num_channels = ath12k_reg_get_num_chans_in_band(ar, band);
			if (!bands[NL80211_BAND_6GHZ]) {
				bands[NL80211_BAND_6GHZ] = band;
			} else {
				/* Split mac in same band under same wiphy during MLO */
				ret = ath12k_mac_update_band(ar,
							     bands[NL80211_BAND_6GHZ],
							     band);
				if (ret)
					return ret;
				ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L0,
						"mac pdev %u identified as 5 GHz split mac during MLO\n",
						 ar->pdev->pdev_id);
			}
		}
	}

	return 0;
}

static u16 ath12k_mac_get_ifmodes(struct ath12k_hw *ah)
{
	struct ath12k *ar;
	int i;
	u16 interface_modes = U16_MAX;

	for_each_ar(ah, ar, i)
		interface_modes &= ar->ab->hw_params->interface_modes;

	return interface_modes == U16_MAX ? 0 : interface_modes;
}

static bool ath12k_mac_is_iface_mode_enable(struct ath12k_hw *ah,
					    enum nl80211_iftype type)
{
	struct ath12k *ar;
	int i;
	u16 interface_modes, mode = 0;
	bool is_enable = false;

	if (type == NL80211_IFTYPE_MESH_POINT) {
		if (IS_ENABLED(CPTCFG_MAC80211_MESH))
			mode = BIT(type);
	} else {
		mode = BIT(type);
	}

	for_each_ar(ah, ar, i) {
		interface_modes = ar->ab->hw_params->interface_modes;
		if (interface_modes & mode) {
			is_enable = true;
			break;
		}
	}

	return is_enable;
}

static int
ath12k_mac_setup_radio_iface_comb(struct ath12k *ar,
				  struct ieee80211_iface_combination *comb)
{
	u16 interface_modes = ar->ab->hw_params->interface_modes;
	struct ieee80211_iface_limit *limits;
	int n_limits, max_interfaces;
	bool ap, mesh, p2p;

	ap = interface_modes & BIT(NL80211_IFTYPE_AP);
	p2p = interface_modes & BIT(NL80211_IFTYPE_P2P_DEVICE);

	mesh = IS_ENABLED(CPTCFG_MAC80211_MESH) &&
	       (interface_modes & BIT(NL80211_IFTYPE_MESH_POINT));

	if ((ap || mesh) && !p2p) {
		n_limits = 2;
		max_interfaces = 16;
	} else if (p2p) {
		n_limits = 3;
		if (ap || mesh)
			max_interfaces = 16;
		else
			max_interfaces = 3;
	} else {
		n_limits = 1;
		max_interfaces = 1;
	}

	limits = kcalloc(n_limits, sizeof(*limits), GFP_KERNEL);
	if (!limits)
		return -ENOMEM;

	limits[0].max = 1;
	limits[0].types |= BIT(NL80211_IFTYPE_STATION);

	if (ap || mesh || p2p)
		limits[1].max = max_interfaces;

	if (ap)
		limits[1].types |= BIT(NL80211_IFTYPE_AP);

	if (mesh)
		limits[1].types |= BIT(NL80211_IFTYPE_MESH_POINT);

	if (p2p) {
		limits[1].types |= BIT(NL80211_IFTYPE_P2P_CLIENT) |
					BIT(NL80211_IFTYPE_P2P_GO);
		limits[2].max = 1;
		limits[2].types |= BIT(NL80211_IFTYPE_P2P_DEVICE);
	}

	comb[0].limits = limits;
	comb[0].n_limits = n_limits;
	comb[0].max_interfaces = max_interfaces;
	comb[0].num_different_channels = 1;
	comb[0].beacon_int_infra_match = true;
	comb[0].beacon_int_min_gcd = 100;
	comb[0].radar_detect_widths = BIT(NL80211_CHAN_WIDTH_20_NOHT) |
					BIT(NL80211_CHAN_WIDTH_20) |
					BIT(NL80211_CHAN_WIDTH_40) |
					BIT(NL80211_CHAN_WIDTH_80) |
					BIT(NL80211_CHAN_WIDTH_160);

	ath12k_mac_setup_radio_iface_comb_extn(comb);

	return 0;
}

static int
ath12k_mac_setup_global_iface_comb(struct ath12k_hw *ah,
				   struct wiphy_radio *radio,
				   u8 n_radio,
				   struct ieee80211_iface_combination *comb)
{
	const struct ieee80211_iface_combination *iter_comb;
	struct ieee80211_iface_limit *limits;
	int i, j, n_limits;
	bool ap, mesh, p2p;

	if (!n_radio)
		return 0;

	ap = ath12k_mac_is_iface_mode_enable(ah, NL80211_IFTYPE_AP);
	p2p = ath12k_mac_is_iface_mode_enable(ah, NL80211_IFTYPE_P2P_DEVICE);
	mesh = ath12k_mac_is_iface_mode_enable(ah, NL80211_IFTYPE_MESH_POINT);

	if ((ap || mesh) && !p2p)
		n_limits = 2;
	else if (p2p)
		n_limits = 3;
	else
		n_limits = 1;

	limits = kcalloc(n_limits, sizeof(*limits), GFP_KERNEL);
	if (!limits)
		return -ENOMEM;

	for (i = 0; i < n_radio; i++) {
		iter_comb = radio[i].iface_combinations;
		for (j = 0; j < iter_comb->n_limits && j < n_limits; j++) {
			limits[j].types |= iter_comb->limits[j].types;
			limits[j].max += iter_comb->limits[j].max;
		}

		comb->max_interfaces += iter_comb->max_interfaces;
		comb->num_different_channels += iter_comb->num_different_channels;
		comb->radar_detect_widths |= iter_comb->radar_detect_widths;
	}

	comb->limits = limits;
	comb->n_limits = n_limits;
	comb->beacon_int_infra_match = true;
	comb->beacon_int_min_gcd = 100;

	return 0;
}

static
void ath12k_mac_cleanup_iface_comb(const struct ieee80211_iface_combination *iface_comb)
{
	kfree(iface_comb[0].limits);
	kfree(iface_comb);
}

static void ath12k_mac_cleanup_iface_combinations(struct ath12k_hw *ah)
{
	struct wiphy *wiphy = ah->hw->wiphy;
	const struct wiphy_radio *radio;
	int i;

	if (wiphy->n_radio > 0) {
		radio = wiphy->radio;
		for (i = 0; i < wiphy->n_radio; i++)
			ath12k_mac_cleanup_iface_comb(radio[i].iface_combinations);

		kfree(wiphy->radio);
	}

	ath12k_mac_cleanup_iface_comb(wiphy->iface_combinations);
}

static int ath12k_mac_setup_iface_combinations(struct ath12k_hw *ah)
{
	struct ieee80211_iface_combination *combinations, *comb;
	struct wiphy *wiphy = ah->hw->wiphy;
	struct wiphy_radio *radio;
	struct ath12k_pdev_cap *cap;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int i, ret;

	combinations = kzalloc(sizeof(*combinations), GFP_KERNEL);
	if (!combinations)
		return -ENOMEM;

	if (ah->num_radio == 1) {
		ret = ath12k_mac_setup_radio_iface_comb(&ah->radio[0],
							combinations);
		if (ret) {
			ath12k_hw_warn(ah, "failed to setup radio interface combinations for one radio: %d",
				       ret);
			goto err_free_combinations;
		}

		goto out;
	}

	/* there are multiple radios */

	radio = kcalloc(ah->num_radio, sizeof(*radio), GFP_KERNEL);
	if (!radio) {
		ret = -ENOMEM;
		goto err_free_combinations;
	}

	for_each_ar(ah, ar, i) {
		comb = kzalloc(sizeof(*comb), GFP_KERNEL);
		if (!comb) {
			ret = -ENOMEM;
			goto err_free_radios;
		}

		ret = ath12k_mac_setup_radio_iface_comb(ar, comb);
		if (ret) {
			ath12k_hw_warn(ah, "failed to setup radio interface combinations for radio %d: %d",
				       i, ret);
			kfree(comb);
			goto err_free_radios;
		}

		radio[i].freq_range = &ar->freq_range;
		radio[i].n_freq_range = 1;

		radio[i].iface_combinations = comb;
		radio[i].n_iface_combinations = 1;
		/* Save per radio tx/rx chainmask */
		pdev = ar->pdev;
		cap = &pdev->cap;
		radio[i].available_antennas_tx = cap->tx_chain_mask;
		radio[i].available_antennas_rx = cap->rx_chain_mask;

	}

	ret = ath12k_mac_setup_global_iface_comb(ah, radio, ah->num_radio, combinations);
	if (ret) {
		ath12k_hw_warn(ah, "failed to setup global interface combinations: %d",
			       ret);
		goto err_free_all_radios;
	}

	wiphy->radio = radio;
	wiphy->n_radio = ah->num_radio;

out:
	wiphy->iface_combinations = combinations;
	wiphy->n_iface_combinations = 1;

	return 0;

err_free_all_radios:
	i = ah->num_radio;

err_free_radios:
	while (i--)
		ath12k_mac_cleanup_iface_comb(radio[i].iface_combinations);

	kfree(radio);

err_free_combinations:
	kfree(combinations);

	return ret;
}

static void ath12k_mac_fetch_coex_info(struct ath12k *ar)
{
        struct ath12k_pdev_cap *cap = &ar->pdev->cap;
        struct ath12k_base *ab = ar->ab;
        struct device *dev = ab->dev;

        ar->coex.coex_support = false;

        if (!(cap->supported_bands & WMI_HOST_WLAN_2GHZ_CAP))
                return;

        if (of_property_read_u32(dev->of_node, "qcom,pta-num",
                                &ar->coex.pta_num)) {
                ath12k_err(ab, "No qcom,pta_num entry in dev-tree.\n");
        }

        if (of_property_read_u32(dev->of_node, "qcom,coex-mode",
                                &ar->coex.coex_mode)) {
                ath12k_err(ab, "No qcom,coex_mode entry in dev-tree.\n");
        }

        if (of_property_read_u32(dev->of_node, "qcom,bt-active-time",
                                &ar->coex.bt_active_time_slot)) {
                ath12k_err(ab, "No qcom,bt-active-time entry in dev-tree.\n");
        }

        if (of_property_read_u32(dev->of_node, "qcom,bt-priority-time",
                                &ar->coex.bt_priority_time_slot)) {
                ath12k_err(ab, "No qcom,bt-priority-time entry in dev-tree.\n");
        }

        if (of_property_read_u32(dev->of_node, "qcom,coex-algo",
                                &ar->coex.coex_algo_type)) {
                ath12k_err(ab, "No qcom,coex-algo entry in dev-tree.\n");
        }

        if (of_property_read_u32(dev->of_node, "qcom,pta-priority",
                                &ar->coex.pta_priority)) {
                ath12k_err(ab, "No qcom,pta-priority entry in dev-tree.\n");
        }

        if (ar->coex.coex_algo_type == COEX_ALGO_OCS) {
                ar->coex.duty_cycle = 100000;
                ar->coex.wlan_duration = 80000;
        }
	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L3,
			"coex pta_num %u coex_mode %u bt_active_time_slot %u bt_priority_time_slot %u coex_algorithm %u pta_priority %u\n",
			ar->coex.pta_num,
			ar->coex.coex_mode, ar->coex.bt_active_time_slot,
			ar->coex.bt_priority_time_slot, ar->coex.coex_algo_type,
			ar->coex.pta_priority);
        ar->coex.coex_support = true;
}

static const u8 ath12k_if_types_ext_capa[] = {
	[0] = WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING,
	[2] = WLAN_EXT_CAPA3_MULTI_BSSID_SUPPORT,
	[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF,
};

static const u8 ath12k_if_types_ext_capa_sta[] = {
	[0] = WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING,
	[2] = WLAN_EXT_CAPA3_MULTI_BSSID_SUPPORT,
	[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF,
	[9] = WLAN_EXT_CAPA10_TWT_REQUESTER_SUPPORT,
};

static const u8 ath12k_if_types_ext_capa_ap[] = {
	[0] = WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING,
	[2] = WLAN_EXT_CAPA3_MULTI_BSSID_SUPPORT,
	[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF,
	[9] = WLAN_EXT_CAPA10_TWT_RESPONDER_SUPPORT,
	[10] = WLAN_EXT_CAPA11_EMA_SUPPORT,
};

static struct wiphy_iftype_ext_capab ath12k_iftypes_ext_capa[] = {
	{
		.extended_capabilities = ath12k_if_types_ext_capa,
		.extended_capabilities_mask = ath12k_if_types_ext_capa,
		.extended_capabilities_len = sizeof(ath12k_if_types_ext_capa),
	}, {
		.iftype = NL80211_IFTYPE_STATION,
		.extended_capabilities = ath12k_if_types_ext_capa_sta,
		.extended_capabilities_mask = ath12k_if_types_ext_capa_sta,
		.extended_capabilities_len =
				sizeof(ath12k_if_types_ext_capa_sta),
	}, {
		.iftype = NL80211_IFTYPE_AP,
		.extended_capabilities = ath12k_if_types_ext_capa_ap,
		.extended_capabilities_mask = ath12k_if_types_ext_capa_ap,
		.extended_capabilities_len =
				sizeof(ath12k_if_types_ext_capa_ap),
		.eml_capabilities = 0,
		.mld_capa_and_ops = 0,
	},
};

static void ath12k_mac_cleanup_unregister(struct ath12k *ar)
{
	int i;

	idr_for_each(&ar->txmgmt_idr, ath12k_mac_tx_mgmt_pending_free, ar);
	idr_destroy(&ar->txmgmt_idr);

	kfree(ar->mac.sbands[NL80211_BAND_2GHZ].channels);
	kfree(ar->mac.sbands[NL80211_BAND_5GHZ].channels);

	ar->mac.sbands[NL80211_BAND_2GHZ].channels = NULL;
	ar->mac.sbands[NL80211_BAND_5GHZ].channels = NULL;

	for (i = 0; i < NL80211_REG_NUM_POWER_MODES; i++) {
		if (!ar->mac.sbands[NL80211_BAND_6GHZ].chan_6g[i])
			continue;
		kfree(ar->mac.sbands[NL80211_BAND_6GHZ].chan_6g[i]->channels);
		kfree(ar->mac.sbands[NL80211_BAND_6GHZ].chan_6g[i]);
		ar->mac.sbands[NL80211_BAND_6GHZ].chan_6g[i] = NULL;
	}
	ar->mac.sbands[NL80211_BAND_6GHZ].channels = NULL;
}

static void ath12k_mac_hw_unregister(struct ath12k_hw *ah)
{
	struct ieee80211_hw *hw = ah->hw;
	struct ath12k *ar;
	int i;

	for_each_ar(ah, ar, i) {
		cancel_work_sync(&ar->regd_update_work);
		cancel_work_sync(&ar->reg_set_previous_country);
		ath12k_debugfs_unregister(ar);
	}

	ieee80211_unregister_hw(hw);

	for_each_ar(ah, ar, i)
		ath12k_mac_cleanup_unregister(ar);

	ath12k_mac_cleanup_iface_combinations(ah);
	kfree(ah->hw->wiphy->addresses);

	SET_IEEE80211_DEV(hw, NULL);
}

static int ath12k_mac_setup_register(struct ath12k *ar,
				     u32 *ht_cap,
				     struct ieee80211_supported_band *bands[])
{
	struct ath12k_pdev_cap *cap = &ar->pdev->cap;
	int level;
	int ret;
	u8 total_vdevs;

	init_waitqueue_head(&ar->txmgmt_empty_waitq);
	idr_init(&ar->txmgmt_idr);
	spin_lock_init(&ar->txmgmt_idr_lock);

	ath12k_pdev_caps_update(ar);

	ret = ath12k_mac_setup_channels_rates(ar,
					      cap->supported_bands,
					      bands);
	if (ret)
		return ret;

	if (test_bit(WMI_SERVICE_IS_TARGET_IPA, ar->ab->wmi_ab.svc_map)) {
		if (ar->ab->hw_params->hw_rev == ATH12K_HW_IPQ5424_HW10) {
			for (level = 0; level < ENHANCED_THERMAL_LEVELS; level++) {
				ar->tt_level_configs[level].tmplwm =
					tt_level_configs[ATH12K_IPA_IPQ5424_THERMAL_LEVEL][level].tmplwm;
				ar->tt_level_configs[level].tmphwm =
					tt_level_configs[ATH12K_IPA_IPQ5424_THERMAL_LEVEL][level].tmphwm;
				ar->tt_level_configs[level].dcoffpercent =
					tt_level_configs[ATH12K_IPA_IPQ5424_THERMAL_LEVEL][level].dcoffpercent;
				ar->tt_level_configs[level].priority = 0;
				ar->tt_level_configs[level].duty_cycle =
					ATH12K_THERMAL_DEFAULT_DUTY_CYCLE;

				if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
					     ar->ab->wmi_ab.svc_map))
					ar->tt_level_configs[level].pout_reduction_db =
						tt_level_configs[ATH12K_IPA_IPQ5424_THERMAL_LEVEL][level].pout_reduction_db;
			}
		} else {
			for (level = 0; level < ENHANCED_THERMAL_LEVELS; level++) {
				ar->tt_level_configs[level].tmplwm =
					tt_level_configs[ATH12K_IPA_THERMAL_LEVEL][level].tmplwm;
				ar->tt_level_configs[level].tmphwm =
					tt_level_configs[ATH12K_IPA_THERMAL_LEVEL][level].tmphwm;
				ar->tt_level_configs[level].dcoffpercent =
					tt_level_configs[ATH12K_IPA_THERMAL_LEVEL][level].dcoffpercent;
				ar->tt_level_configs[level].priority = 0;
				ar->tt_level_configs[level].duty_cycle =
					ATH12K_THERMAL_DEFAULT_DUTY_CYCLE;

				if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
					     ar->ab->wmi_ab.svc_map))
					ar->tt_level_configs[level].pout_reduction_db =
						tt_level_configs[ATH12K_IPA_THERMAL_LEVEL][level].pout_reduction_db;
			}
		}
	} else {
		if (ar->ab->hw_params->hw_rev == ATH12K_HW_IPQ5424_HW10) {
			for (level = 0; level < ENHANCED_THERMAL_LEVELS; level++) {
				ar->tt_level_configs[level].tmplwm =
					tt_level_configs[ATH12K_XFRM_IPQ5424_THERMAL_LEVEL][level].tmplwm;
				ar->tt_level_configs[level].tmphwm =
					tt_level_configs[ATH12K_XFRM_IPQ5424_THERMAL_LEVEL][level].tmphwm;
				ar->tt_level_configs[level].dcoffpercent =
					tt_level_configs[ATH12K_XFRM_IPQ5424_THERMAL_LEVEL][level].dcoffpercent;
				ar->tt_level_configs[level].priority = 0;
				ar->tt_level_configs[level].duty_cycle =
					ATH12K_THERMAL_DEFAULT_DUTY_CYCLE;

				if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
					     ar->ab->wmi_ab.svc_map))
					ar->tt_level_configs[level].pout_reduction_db =
						tt_level_configs[ATH12K_XFRM_IPQ5424_THERMAL_LEVEL][level].pout_reduction_db;
			}
		} else {
			for (level = 0; level < ENHANCED_THERMAL_LEVELS; level++) {
				ar->tt_level_configs[level].tmplwm =
					tt_level_configs[ATH12K_XFRM_THERMAL_LEVEL][level].tmplwm;
				ar->tt_level_configs[level].tmphwm =
					tt_level_configs[ATH12K_XFRM_THERMAL_LEVEL][level].tmphwm;
				ar->tt_level_configs[level].dcoffpercent =
					tt_level_configs[ATH12K_XFRM_THERMAL_LEVEL][level].dcoffpercent;
				ar->tt_level_configs[level].priority = 0;
				ar->tt_level_configs[level].duty_cycle =
					ATH12K_THERMAL_DEFAULT_DUTY_CYCLE;

				if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
					     ar->ab->wmi_ab.svc_map))
					ar->tt_level_configs[level].pout_reduction_db =
						tt_level_configs[ATH12K_XFRM_THERMAL_LEVEL][level].pout_reduction_db;
			}
		}
	}

	if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK, ar->ab->wmi_ab.svc_map))
		for (level = 0; level < ENHANCED_THERMAL_LEVELS; level++)
			ar->tt_level_configs[level].tx_chain_mask = ar->cfg_tx_chainmask;

	ath12k_mac_setup_ht_vht_cap(ar, cap, ht_cap);
	ath12k_mac_setup_sband_iftype_data(ar, cap);

	ar->max_num_stations = ath12k_core_get_max_station_per_radio(ar->ab);
	ar->max_num_peers = ath12k_core_get_max_peers_per_radio(ar->ab);
	ar->rssi_offsets.rssi_offset = ATH12K_DEFAULT_NOISE_FLOOR;
	ar->free_map_id = ATH12K_FREE_MAP_ID_MASK;

	total_vdevs = ath12k_core_get_total_num_vdevs(ar->ab);
	if (total_vdevs == ATH12K_MAX_NUM_VDEVS_NLINK)
		ar->max_num_stations -= TARGET_NUM_BRIDGE_SELF_PEER;

	return 0;
}

static int ath12k_alloc_per_hw_mac_addr(struct ath12k_hw *ah)
{
	struct ath12k *ar;
	struct ieee80211_hw *hw = ah->hw;
	struct mac_address *addresses;
	int i;
	ar = ah->radio;

	addresses = kzalloc(sizeof(*addresses) * ah->num_radio,
			    GFP_KERNEL);
	if(!addresses)
		return -ENOMEM;

	for (i = 0; i < ah->num_radio; i++) {
		ether_addr_copy((u8 *)(&addresses[i]), ar->mac_addr);
		ar++;
	}
	hw->wiphy->addresses = addresses;
	hw->wiphy->n_addresses = ah->num_radio;
	return 0;
}

static int ath12k_mac_hw_register(struct ath12k_hw *ah)
{
	struct ieee80211_hw *hw = ah->hw;
	struct wiphy *wiphy = hw->wiphy;
	struct ath12k *ar = ath12k_ah_to_ar(ah, 0);
	struct ath12k_base *ab = ar->ab;
	struct ath12k_pdev *pdev;
	struct ath12k_pdev_cap *cap;
	static const u32 cipher_suites[] = {
		WLAN_CIPHER_SUITE_TKIP,
		WLAN_CIPHER_SUITE_CCMP,
		WLAN_CIPHER_SUITE_AES_CMAC,
		WLAN_CIPHER_SUITE_BIP_CMAC_256,
		WLAN_CIPHER_SUITE_BIP_GMAC_128,
		WLAN_CIPHER_SUITE_BIP_GMAC_256,
		WLAN_CIPHER_SUITE_GCMP,
		WLAN_CIPHER_SUITE_GCMP_256,
		WLAN_CIPHER_SUITE_CCMP_256,
	};
	int ret, i, j;
	u32 ht_cap = U32_MAX, antennas_rx = 0, antennas_tx = 0;
	bool is_6ghz = false, is_raw_mode = false, is_monitor_disable = false;
	u8 *mac_addr = NULL;
	u8 mbssid_max_interfaces = 0;

	wiphy->max_ap_assoc_sta = 0;

	for_each_ar(ah, ar, i) {
		u32 ht_cap_info = 0;

		pdev = ar->pdev;
		if (ar->ab->pdevs_macaddr_valid) {
			ether_addr_copy(ar->mac_addr, pdev->mac_addr);
		} else {
			ether_addr_copy(ar->mac_addr, ar->ab->mac_addr);
			ar->mac_addr[4] += ar->pdev_idx;
		}

		ret = ath12k_mac_setup_register(ar, &ht_cap_info, hw->wiphy->bands);
		if (ret)
			goto err_cleanup_unregister;

		/* 6 GHz does not support HT Cap, hence do not consider it */
		if (!ar->supports_6ghz)
			ht_cap &= ht_cap_info;

		wiphy->max_ap_assoc_sta += ar->max_num_stations;

		/* Advertise the max antenna support of all radios, driver can handle
		 * per pdev specific antenna setting based on pdev cap when antenna
		 * changes are made
		 */
		cap = &pdev->cap;

		antennas_rx = max_t(u32, antennas_rx, cap->rx_chain_mask);
		antennas_tx = max_t(u32, antennas_tx, cap->tx_chain_mask);

		if (ar->supports_6ghz)
			is_6ghz = true;

		if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ar->ab->ag->flags))
			is_raw_mode = true;

		if (!ar->ab->hw_params->supports_monitor)
			is_monitor_disable = true;

		/* In non-MLO/SLO case ah->num_radio is 1, and ar->mac_addr is
		 * assigned to ieee80211_hw. In MLO with ah->num_radio > 1,
		 * in that case we take first ar mac_addr and assign to
		 * ieee80211_hw.
		 */
		if (i == 0)
			mac_addr = ar->mac_addr;

		mbssid_max_interfaces += TARGET_NUM_VDEVS;
	}

	wiphy->available_antennas_rx = antennas_rx;
	wiphy->available_antennas_tx = antennas_tx;

	if (!mac_addr) {
		ath12k_warn(ab, "mac_addr is NULL, cannot set permanent address\n");
		ret = -EINVAL;
		goto err_complete_cleanup_unregister;
	}

	SET_IEEE80211_PERM_ADDR(hw, mac_addr);
	SET_IEEE80211_DEV(hw, ab->dev);

	ret = ath12k_mac_setup_iface_combinations(ah);
	if (ret) {
		ath12k_err(ab, "failed to setup interface combinations: %d\n", ret);
		goto err_complete_cleanup_unregister;
	}

	ret = ath12k_alloc_per_hw_mac_addr(ah);
	if (ret) {
		ath12k_err(ab, "failed to register per hw mac address: %d\n", ret);
		goto err_cleanup_if_combs;
	}

	wiphy->interface_modes = ath12k_mac_get_ifmodes(ah);

	if (ah->num_radio == 1 &&
	    wiphy->bands[NL80211_BAND_2GHZ] &&
	    wiphy->bands[NL80211_BAND_5GHZ] &&
	    wiphy->bands[NL80211_BAND_6GHZ])
		ieee80211_hw_set(hw, SINGLE_SCAN_ON_ALL_BANDS);

	ieee80211_hw_set(hw, SIGNAL_DBM);
	ieee80211_hw_set(hw, SUPPORTS_PS);
	ieee80211_hw_set(hw, SUPPORTS_DYNAMIC_PS);
	ieee80211_hw_set(hw, MFP_CAPABLE);
	ieee80211_hw_set(hw, REPORTS_TX_ACK_STATUS);
	ieee80211_hw_set(hw, HAS_RATE_CONTROL);
	ieee80211_hw_set(hw, AP_LINK_PS);
	ieee80211_hw_set(hw, SPECTRUM_MGMT);
	ieee80211_hw_set(hw, CONNECTION_MONITOR);
	ieee80211_hw_set(hw, SUPPORTS_PER_STA_GTK);
	ieee80211_hw_set(hw, CHANCTX_STA_CSA);
	ieee80211_hw_set(hw, QUEUE_CONTROL);
	ieee80211_hw_set(hw, SUPPORTS_TX_FRAG);
	ieee80211_hw_set(hw, REPORTS_LOW_ACK);
	ieee80211_hw_set(hw, NO_VIRTUAL_MONITOR);
	ieee80211_hw_set(hw, SUPPORT_ECM_REGISTRATION);
	ieee80211_hw_set(hw, SUPPORTS_TID_CLASS_OFFLOAD);
	ieee80211_hw_set(hw, HAS_TX_QUEUE);
	ieee80211_hw_set(hw, SUPPORTS_DSCP_TID_MAP);
	ieee80211_hw_set(hw, SUPPORTS_MULTI_BSSID);
	ieee80211_hw_set(hw, SUPPORTS_SINGLE_CHANNEL);

	if (ath12k_frame_mode == ATH12K_HW_TXRX_ETHERNET) {
		ieee80211_hw_set(hw, SUPPORTS_TX_ENCAP_OFFLOAD);
		ieee80211_hw_set(hw, SUPPORTS_RX_DECAP_OFFLOAD);

	}

	ieee80211_hw_set(hw, SUPPORTS_VLAN_DATA_OFFLOAD);

	if (cap->nss_ratio_enabled)
		ieee80211_hw_set(hw, SUPPORTS_VHT_EXT_NSS_BW);

	if ((ht_cap & WMI_HT_CAP_ENABLED) || is_6ghz) {
		ieee80211_hw_set(hw, AMPDU_AGGREGATION);
		ieee80211_hw_set(hw, TX_AMPDU_SETUP_IN_HW);
		ieee80211_hw_set(hw, SUPPORTS_REORDERING_BUFFER);
		ieee80211_hw_set(hw, SUPPORTS_AMSDU_IN_AMPDU);
		ieee80211_hw_set(hw, USES_RSS);
	}

	if (ab->hw_params->supports_ap_ps)
        	ieee80211_hw_set(hw, SUPPORTS_AP_PS);

	wiphy->features |= NL80211_FEATURE_STATIC_SMPS;
	wiphy->flags |= WIPHY_FLAG_IBSS_RSN;

	/* TODO: Check if HT capability advertised from firmware is different
	 * for each band for a dual band capable radio. It will be tricky to
	 * handle it when the ht capability different for each band.
	 */
	if (ht_cap & WMI_HT_CAP_DYNAMIC_SMPS ||
	    (is_6ghz && ab->hw_params->supports_dynamic_smps_6ghz))
		wiphy->features |= NL80211_FEATURE_DYNAMIC_SMPS;

	wiphy->max_scan_ssids = WLAN_SCAN_PARAMS_MAX_SSID;
	wiphy->max_scan_ie_len = WLAN_SCAN_PARAMS_MAX_IE_LEN;

	hw->max_listen_interval = ATH12K_MAX_HW_LISTEN_INTERVAL;

	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	wiphy->flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH;
	wiphy->max_remain_on_channel_duration = 5000;

	wiphy->flags |= WIPHY_FLAG_AP_UAPSD;
	wiphy->features |= NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE |
				   NL80211_FEATURE_AP_SCAN;
	wiphy->features |= NL80211_FEATURE_TX_POWER_INSERTION;

	/* MLO is not yet supported so disable Wireless Extensions for now
	 * to make sure ath12k users don't use it. This flag can be removed
	 * once WIPHY_FLAG_SUPPORTS_MLO is enabled.
	 */
	wiphy->flags |= WIPHY_FLAG_DISABLE_WEXT;

	/* Copy over MLO related capabilities received from
	 * WMI_SERVICE_READY_EXT2_EVENT if single_chip_mlo_supp is set.
	 */
	if (ab->ag->mlo_capable) {
		ath12k_iftypes_ext_capa[2].eml_capabilities = cap->eml_cap;
		ath12k_iftypes_ext_capa[2].mld_capa_and_ops = cap->mld_cap;
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_MLO;

		if(!is_raw_mode)
			ieee80211_hw_set(hw, MLO_MCAST_MULTI_LINK_TX);

		if (test_bit(WMI_SERVICE_STA_MLO_RCFG_SUPPORT,
			     ar->ab->wmi_ab.svc_map))
			ath12k_iftypes_ext_capa[2].mld_capa_and_ops |=
				IEEE80211_MLD_CAP_OP_LINK_RECONF_SUPPORT;
	}

	hw->queues = ATH12K_HW_MAX_QUEUES;
	wiphy->tx_queue_len = ATH12K_QUEUE_LEN;
	hw->offchannel_tx_hw_queue = ATH12K_HW_MAX_QUEUES - 1;
	hw->max_rx_aggregation_subframes = IEEE80211_MAX_AMPDU_BUF_EHT;

	hw->vif_data_size = sizeof(struct ath12k_vif);
	hw->sta_data_size = sizeof(struct ath12k_sta);
	hw->extra_tx_headroom = ab->hw_params->iova_mask;

	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_CQM_RSSI_LIST);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_STA_TX_PWR);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_ACK_SIGNAL_SUPPORT);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_BEACON_PROTECTION);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_BEACON_ADVERTISED_TTLM_OFFLOAD);

	if (test_bit(WMI_TLV_SERVICE_BSS_COLOR_OFFLOAD, ar->ab->wmi_ab.svc_map))
		 wiphy_ext_feature_set(ar->ah->hw->wiphy,
				       NL80211_EXT_FEATURE_BSS_COLOR);

	wiphy->cipher_suites = cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites);

	wiphy->iftype_ext_capab = ath12k_iftypes_ext_capa;
	wiphy->num_iftype_ext_capab = ARRAY_SIZE(ath12k_iftypes_ext_capa);

	wiphy->mbssid_max_interfaces = mbssid_max_interfaces;
	wiphy->ema_max_profile_periodicity = TARGET_EMA_MAX_PROFILE_PERIOD;

	wiphy->mbssid_max_ngroups = TARGET_MAX_MBSSID_GROUPS;

	/* Currently ath12k isn't overriding default target beacon size
	 * explicitly, hence advertising the same to mac80211 using
	 * max_beacon_size.
	 */
	wiphy->max_beacon_size = TARGET_MAX_BEACON_SIZE;

	if (is_6ghz) {
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_FILS_DISCOVERY);
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_UNSOL_BCAST_PROBE_RESP);
		/* For 6 GHz radios, check if both FW and Host support AFC feature and if so,
		 * advertise the AFC feature support to the higher layers.
		 */
		if (test_bit(WMI_TLV_SERVICE_AFC_SUPPORT, ar->ab->wmi_ab.svc_map) &&
		    ath12k_6ghz_sp_pwrmode_supp_enabled) {
		    wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_TARGET_AND_HOST_AFC_SUPPORT);
		    /* If reg_no_action module param is set, user wishes to operate in
		     * enterprise mode of AFC.
		     */
		    if (!ath12k_afc_reg_no_action) {
			wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_RETAIL_AFC_SUPPORT);
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "Sending retail AFC feature support to higher layers\n");
		    }
		}
	}

	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_PUNCT);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_SET_SCAN_DWELL);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_BEACON_RATE_LEGACY);
	wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_MLD_LINK_REMOVAL_OFFLOAD);

	for_each_ar(ah, ar, i) {
		if (ar->ab->hw_params->ftm_responder)
			wiphy_ext_feature_set(wiphy,
					      NL80211_EXT_FEATURE_ENABLE_FTM_RESPONDER);

		if (test_bit(WMI_TLV_SERVICE_SCAN_PHYMODE_SUPPORT, ar->ab->wmi_ab.svc_map))
			ieee80211_hw_set(hw, SUPPORTS_EXT_REMAIN_ON_CHAN);

		if ((ar->pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP)) {
			if (test_bit(ar->cfg_rx_chainmask, &cap->adfs_chain_mask)) {
				wiphy_ext_feature_set(hw->wiphy,
					      NL80211_EXT_FEATURE_RADAR_BACKGROUND);
			} else if (test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT,
					  ar->ab->wmi_ab.svc_map)) {
				wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_DEVICE_BW);
				wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_RADAR_BACKGROUND);
			}
		}
	}

	ath12k_reg_init(hw);

	if (!is_raw_mode) {
		ieee80211_hw_set(hw, SW_CRYPTO_CONTROL);
		ieee80211_hw_set(hw, SUPPORT_FAST_XMIT);
	}

	if (test_bit(WMI_TLV_SERVICE_NLO, ar->wmi->wmi_ab->svc_map)) {
		wiphy->max_sched_scan_ssids = WMI_PNO_MAX_SUPP_NETWORKS;
		wiphy->max_match_sets = WMI_PNO_MAX_SUPP_NETWORKS;
		wiphy->max_sched_scan_ie_len = WMI_PNO_MAX_IE_LENGTH;
		wiphy->max_sched_scan_plans = WMI_PNO_MAX_SCHED_SCAN_PLANS;
		wiphy->max_sched_scan_plan_interval =
					WMI_PNO_MAX_SCHED_SCAN_PLAN_INT;
		wiphy->max_sched_scan_plan_iterations =
					WMI_PNO_MAX_SCHED_SCAN_PLAN_ITRNS;
		wiphy->features |= NL80211_FEATURE_ND_RANDOM_MAC_ADDR;
	}

	ret = ath12k_wow_init(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to init wow: %d\n", ret);
		goto err_cleanup_if_combs;
	}

	if (ab->ag->mlo_capable)
		wiphy_ext_feature_set(hw->wiphy, NL80211_EXT_FEATURE_ERP);

	ath12k_vendor_register(ah);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ieee80211_hw_set(hw, SUPPORT_ECM_REGISTRATION);
#endif

	ret = ieee80211_register_hw(hw);
	if (ret) {
		ath12k_err(ab, "ieee80211 registration failed: %d\n", ret);
		goto err_cleanup_if_combs;
	}

	if (is_monitor_disable)
		/* There's a race between calling ieee80211_register_hw()
		 * and here where the monitor mode is enabled for a little
		 * while. But that time is so short and in practise it make
		 * a difference in real life.
		 */
		wiphy->interface_modes &= ~BIT(NL80211_IFTYPE_MONITOR);

	ath12k_hw_debugfs_register(ah);

	for_each_ar(ah, ar, i) {
		/* Apply the regd received during initialization */
		ret = ath12k_regd_update(ar, true);
		if (ret) {
			ath12k_err(ar->ab, "ath12k regd update failed: %d\n", ret);
			goto err_unregister_hw;
		}

		if (ar->supports_6ghz && ar->ab->sp_rule &&
		    ar->ab->sp_rule->num_6ghz_sp_rule) {
			struct ath12k_afc_expiry_info *exp_info =
			    &ar->ab->afc_exp_info[ar->pdev_idx];

			if (exp_info->is_afc_exp_valid) {
				ar->afc.event_type = ATH12K_AFC_EVENT_TIMER_EXPIRY;
				ar->afc.event_subtype = exp_info->event_subtype;
				ar->afc.request_id = exp_info->req_id;
				if (ath12k_process_expiry_event(ar))
					ath12k_warn(ab,
						    "Failed to process expiry event\n");

				exp_info->is_afc_exp_valid = false;
			}
		}

		if (ar->ab->hw_params->current_cc_support && ab->new_alpha2[0]) {
			struct wmi_set_current_country_arg current_cc = {};

			memcpy(&current_cc.alpha2, ab->new_alpha2, 2);
			memcpy(&ar->alpha2, ab->new_alpha2, 2);
			ret = ath12k_wmi_send_set_current_country_cmd(ar, &current_cc);
			if (ret)
				ath12k_warn(ar->ab,
					    "failed set cc code for mac register: %d\n", ret);
		}

		ath12k_fw_stats_init(ar);
		ath12k_debugfs_register(ar);
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mac pdev %u freq limits %u->%u MHz, no. of channels %u\n",
				ar->pdev->pdev_id, ar->freq_range.start_freq,
				ar->freq_range.end_freq, ar->num_channels);
	}

	return 0;

err_unregister_hw:
	for_each_ar(ah, ar, i) {
		ath12k_fw_stats_free(&ar->fw_stats);
		ath12k_debugfs_unregister(ar);
	}

	ieee80211_unregister_hw(hw);

err_cleanup_if_combs:
	ath12k_mac_cleanup_iface_combinations(ah);

err_complete_cleanup_unregister:
	i = ah->num_radio;

err_cleanup_unregister:
	for (j = 0; j < i; j++) {
		ar = ath12k_ah_to_ar(ah, j);
		ath12k_mac_cleanup_unregister(ar);
	}

	SET_IEEE80211_DEV(hw, NULL);

	return ret;
}

static void ath12k_mac_setup(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_pdev *pdev = ar->pdev;
	u8 pdev_idx = ar->pdev_idx;

	ar->lmac_id = ath12k_hw_get_mac_from_pdev_id(ab->hw_params, pdev_idx);

	ar->wmi = &ab->wmi_ab.wmi[pdev_idx];
	/* FIXME: wmi[0] is already initialized during attach,
	 * Should we do this again?
	 */
	ath12k_wmi_pdev_attach(ab, pdev_idx);

	ath12k_mac_fetch_coex_info(ar);

	ar->cfg_tx_chainmask = pdev->cap.tx_chain_mask;
	ar->cfg_rx_chainmask = pdev->cap.rx_chain_mask;
	ar->num_tx_chains = hweight32(pdev->cap.tx_chain_mask);
	ar->num_rx_chains = hweight32(pdev->cap.rx_chain_mask);
	ar->scan.arvif = NULL;
	ar->monitor_vdev_id = -1;
	ar->monitor_started = false;
	ar->monitor_vdev_created = false;
	ar->vdev_id_11d_scan = ATH12K_11D_INVALID_VDEV_ID;

	spin_lock_init(&ar->data_lock);
	INIT_LIST_HEAD(&ar->arvifs);
	INIT_LIST_HEAD(&ar->dp.ppdu_stats_info);
	INIT_LIST_HEAD(&ar->wlan_intf_list);

	init_completion(&ar->vdev_setup_done);
	init_completion(&ar->vdev_delete_done);
	init_completion(&ar->peer_assoc_done);
	init_completion(&ar->peer_delete_done);
	init_completion(&ar->install_key_done);
	init_completion(&ar->bss_survey_done);
	init_completion(&ar->scan.started);
	init_completion(&ar->scan.completed);
	init_completion(&ar->scan.on_channel);
	init_completion(&ar->mlo_setup_done);
	init_completion(&ar->completed_11d_scan);
	init_completion(&ar->thermal.wmi_sync);
	init_completion(&ar->mvr_complete);
	init_completion(&ar->suspend);
	init_completion(&ar->pdev_resume);

	INIT_DELAYED_WORK(&ar->scan.timeout, ath12k_scan_timeout_work);
	INIT_DELAYED_WORK(&ar->scan.roc_done, ath12k_scan_roc_done);
	wiphy_work_init(&ar->scan.vdev_clean_wk, ath12k_scan_vdev_clean_work);
	INIT_WORK(&ar->regd_update_work, ath12k_regd_update_work);
	INIT_WORK(&ar->reg_set_previous_country,
		  ath12k_set_previous_country_work);
	wiphy_work_init(&ar->agile_cac_abort_wq, ath12k_agile_cac_abort_work);

	wiphy_work_init(&ar->wmi_mgmt_tx_work, ath12k_mgmt_over_wmi_tx_work);
	INIT_WORK(&ar->wlan_intf_work, ath12k_vendor_wlan_intf_stats);
	skb_queue_head_init(&ar->wmi_mgmt_tx_queue);

	ar->monitor_vdev_id = -1;
	ar->monitor_vdev_created = false;
	ar->monitor_started = false;

	INIT_WORK(&ar->erp_handle_trigger_work, ath12k_erp_handle_trigger);
	INIT_WORK(&ar->ssr_erp_exit, ath12k_erp_ssr_exit);
}

int __ath12k_mac_mlo_setup(struct ath12k *ar)
{
	u8 num_link = 0, partner_link_id[ATH12K_GROUP_MAX_RADIO] = {};
	struct ath12k_base *partner_ab, *ab = ar->ab;
	struct ath12k_hw_group *ag = ab->ag;
	u32 max_ml_peers = ab->max_ml_peer_supported;
	struct wmi_mlo_setup_arg mlo = {};
	struct ath12k_pdev *pdev;
	unsigned long time_left;
	int i, j, ret;

	lockdep_assert_held(&ag->mutex);

	reinit_completion(&ar->mlo_setup_done);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed)
			continue;

		if ((ab != partner_ab) && (max_ml_peers > partner_ab->max_ml_peer_supported))
			max_ml_peers = min(max_ml_peers, partner_ab->max_ml_peer_supported);

		for (j = 0; j < partner_ab->num_radios; j++) {
			pdev = &partner_ab->pdevs[j];

			/* Avoid the self link */
			if (ar == pdev->ar)
				continue;

			partner_link_id[num_link] = pdev->hw_link_id;
			num_link++;

			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					"device %d pdev %d hw_link_id %d num_link %d\n",
					i, j, pdev->hw_link_id, num_link);
		}
	}

	if (num_link == 0)
		return 0;

	mlo.group_id = cpu_to_le32(ag->id);
	mlo.partner_link_id = partner_link_id;
	mlo.num_partner_links = num_link;
	mlo.max_ml_peer_supported = max_ml_peers;
	ar->mlo_setup_status = 0;
	ar->ah->max_ml_peers_supported = max_ml_peers;

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"group id %d num_link %d max_ml_peers:%d\n",
			ag->id, num_link, max_ml_peers);

	ret = ath12k_wmi_mlo_setup(ar, &mlo);
	if (ret) {
		ath12k_err(ab, "failed to send  setup MLO WMI command for pdev %d: %d\n",
			   ar->pdev_idx, ret);
		return ret;
	}

	time_left = wait_for_completion_timeout(&ar->mlo_setup_done,
						WMI_MLO_CMD_TIMEOUT_HZ);

	if (!time_left || ar->mlo_setup_status)
		return ar->mlo_setup_status ? : -ETIMEDOUT;

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			"mlo setup done for pdev %d\n", ar->pdev_idx);

	return 0;
}

static int __ath12k_mac_mlo_teardown(struct ath12k *ar, bool umac_reset,
				     enum wmi_mlo_tear_down_reason_code_type reason_code)
{
	struct ath12k_base *ab = ar->ab;
	int ret;
	u8 num_link;

	if (test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags) ||
	    ath12k_check_erp_power_down(ab->ag))
		return 0;

	num_link = ath12k_get_num_partner_link(ar);

	if (num_link == 0)
		return 0;

	ret = ath12k_wmi_mlo_teardown(ar, umac_reset,
				      reason_code, false);
	if (ret) {
		ath12k_warn(ab, "failed to send MLO teardown WMI command for pdev %d: %d\n",
			    ar->pdev_idx, ret);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_MAC, "mlo teardown for pdev %d\n", ar->pdev_idx);

	return 0;
}

int ath12k_mac_mlo_teardown_with_umac_reset(struct ath12k_base *ab,
					    enum wmi_mlo_tear_down_reason_code_type reason_code)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_hw* ah;
	int i, j, ret = 0;
	struct ath12k *ar;
	bool umac_reset;

	for(i = 0; i < ag->num_hw; i++){
		ah = ag->ah[i];
		if (!ah)
			continue;

		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];

			if (ar->ab->is_bypassed ||
			    (ar->ab == ab &&
			     reason_code != WMI_MLO_TEARDOWN_REASON_DYNAMIC_WSI_REMAP)) {
				/* No need to send teardown event for asserted
				 * chip, as anyway there will be no completion
				 * event from FW.
				 */
				ar->teardown_complete_event = true;
				continue;
			}

			/* Need to umac_reset as 1 for only one chip */
			umac_reset = false;
			if (!ag->trigger_umac_reset) {
                                umac_reset = true;
                                ag->trigger_umac_reset = true;
                        }

			ret = __ath12k_mac_mlo_teardown(ar, umac_reset, reason_code);
			if (ret)
				goto out;
		}
	}

out:
        return ret;
}

int ath12k_mac_mlo_setup(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int ret;
	int i, j;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];
			if (!ar || ar->ab->is_bypassed)
				continue;

			ret = __ath12k_mac_mlo_setup(ar);
			if (ret) {
				ath12k_err(ar->ab, "failed to setup MLO: %d\n", ret);
				goto err_setup;
			}
		}
	}

	return 0;

err_setup:
	for (i = i - 1; i >= 0; i--) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		for (j = j - 1; j >= 0; j--) {
			ar = &ah->radio[j];
			if (!ar)
				continue;

			__ath12k_mac_mlo_teardown(ar, false,
						  WMI_MLO_TEARDOWN_REASON_HOST_INITIATED);
		}
	}

	return ret;
}

void ath12k_mac_mlo_teardown(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int ret, i, j;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];
			if (ar->ab->is_bypassed) {
				ath12k_info(ar->ab, "Chip is in bypassed state, skip mlo teardown");
				continue;
			}
			ret = __ath12k_mac_mlo_teardown(ar, false,
							WMI_MLO_TEARDOWN_REASON_HOST_INITIATED);
			if (ret) {
				ath12k_err(ar->ab, "failed to teardown MLO: %d\n", ret);
				break;
			}
		}
	}
}

int ath12k_mac_register(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	int i;
	int ret;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);

		ret = ath12k_mac_hw_register(ah);
		if (ret)
			goto err;
	}

	return 0;

err:
	for (i = i - 1; i >= 0; i--) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		ath12k_mac_hw_unregister(ah);
	}

	return ret;
}

void ath12k_mac_unregister(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	int i;

	for (i = ag->num_hw - 1; i >= 0; i--) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		ath12k_mac_hw_unregister(ah);
	}
}

static void ath12k_mac_hw_destroy(struct ath12k_hw *ah)
{
	ieee80211_free_hw(ah->hw);
}

static struct ath12k_hw *ath12k_mac_hw_allocate(struct ath12k_hw_group *ag,
						struct ath12k_pdev_map *pdev_map,
						u8 num_pdev_map,
						const char* phy_name)
{
	struct ieee80211_hw *hw;
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct ath12k_pdev *pdev;
	struct ath12k_hw *ah;
	int i, ret;
	u8 pdev_idx;

	hw = ieee80211_alloc_hw_nm(struct_size(ah, radio, num_pdev_map),
				   pdev_map[0].ab->ath12k_ops, phy_name);

	if (!hw)
		return NULL;

	ath12k_mac_hw_allocate_extn(hw, pdev_map[0].ab->ath12k_ops_extn);

	ah = ath12k_hw_to_ah(hw);
	ah->hw = hw;
	ah->num_radio = num_pdev_map;

	mutex_init(&ah->hw_mutex);

	spin_lock_init(&ah->afc_lock);
	spin_lock_init(&ah->dp_hw.peer_lock);
	INIT_LIST_HEAD(&ah->dp_hw.peers);

	for (i = 0; i < num_pdev_map; i++) {
		ab = pdev_map[i].ab;
		pdev_idx = pdev_map[i].pdev_idx;
		pdev = &ab->pdevs[pdev_idx];

		ar = ath12k_ah_to_ar(ah, i);
		ar->ah = ah;
		ar->ab = ab;
		ar->hw_link_id = pdev->hw_link_id;
		ar->pdev = pdev;
		ar->pdev_idx = pdev_idx;
		ar->radio_idx = i;
		pdev->ar = ar;

		ath12k_dp_cmn_update_hw_links(ab->dp, ag, ar);

		ath12k_mac_setup(ar);
		ret = ath12k_dp_pdev_pre_alloc(ar);
		if (ret) {
			ath12k_mac_hw_destroy(ah);
			ah = NULL;
			break;
		}
	}

	return ah;
}

void ath12k_mac_destroy(struct ath12k_hw_group *ag)
{
	struct ath12k_pdev *pdev;
	struct ath12k_base *ab = ag->ab[0];
	int i, j;
	struct ath12k_hw *ah;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		for (j = 0; j < ab->num_radios; j++) {
			pdev = &ab->pdevs[j];
			if (!pdev->ar)
				continue;
			pdev->ar = NULL;
		}

		ath12k_link_sta_rhash_tbl_destroy(ab);
	}

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		ath12k_mac_hw_destroy(ah);
		ath12k_ag_set_ah(ag, i, NULL);
	}
}

static void ath12k_mac_set_device_defaults(struct ath12k_base *ab)
{
	u8 total_vdevs;

	/* Initialize channel counters frequency value in hertz */
	ab->cc_freq_hz = 320000;
	spin_lock_bh(&ab->base_lock);
	total_vdevs = ath12k_core_get_total_num_vdevs(ab);
	ab->free_vdev_map = (1LL << (ab->num_radios * total_vdevs)) - 1;
	ab->num_max_vdev_supported = (ab->num_radios * total_vdevs);
	spin_unlock_bh(&ab->base_lock);
}

int ath12k_mac_allocate(struct ath12k_hw_group *ag)
{
	struct ath12k_pdev_map pdev_map[ATH12K_GROUP_MAX_RADIO];
	int mac_id, device_id, total_radio, num_hw, pdev_index;
	const char *phy_name = NULL;
	struct ath12k_pdev *pdev;
	struct ath12k_base *ab;
	struct ath12k_hw *ah;
	int ret, i, j;
	u8 radio_per_hw;

	total_radio = 0;
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		ath12k_mac_set_device_defaults(ab);
		total_radio += ab->num_radios;
		if (ag->mlo_capable) {
			for (j = 0; j < ab->num_radios; j++) {
				pdev = &ab->pdevs[j];
				if (!phy_name)
					phy_name = pdev->phy_name;
				else if(strcmp(phy_name, pdev->phy_name) > 0)
					phy_name = pdev->phy_name;

			}
		}
		ath12k_link_sta_rhash_tbl_init(ab);
	}

	if (!total_radio)
		return -EINVAL;

	if (WARN_ON(total_radio > ATH12K_GROUP_MAX_RADIO))
		return -ENOSPC;

	/* All pdev get combined and register as single wiphy based on
	 * hardware group which participate in multi-link operation else
	 * each pdev get register separately.
	 */
	if (ag->mlo_capable)
		radio_per_hw = total_radio;
	else
		radio_per_hw = 1;

	num_hw = total_radio / radio_per_hw;

	ag->num_hw = 0;
	device_id = 0;
	mac_id = 0;
	pdev_index = 0;

	for (i = 0; i < num_hw; i++) {
		for (j = 0; j < radio_per_hw; j++) {
			if (device_id >= ag->num_devices || !ag->ab[device_id]) {
				ret = -ENOSPC;
				goto err;
			}

			ab = ag->ab[device_id];
			pdev_map[j].ab = ab;
			pdev_map[j].pdev_idx = mac_id;
			mac_id++;

			/* If mac_id falls beyond the current device MACs then
			 * move to next device
			 */
			if (mac_id >= ab->num_radios) {
				mac_id = 0;
				device_id++;
			}
		}

		ab = pdev_map->ab;
		if (!ag->mlo_capable) {
			pdev = &ab->pdevs[pdev_index];
			pdev_index++;
			if (pdev_index >= ab->num_radios)
				pdev_index = 0;
			phy_name = pdev->phy_name;
		}

		ah = ath12k_mac_hw_allocate(ag, pdev_map, radio_per_hw, phy_name);
		if (!ah) {
			ath12k_warn(ab, "failed to allocate mac80211 hw device for hw_idx %d\n",
				    i);
			ret = -ENOMEM;
			goto err;
		}

		ah->dev = ab->dev;

		ag->ah[i] = ah;
		ag->num_hw++;
	}

	return 0;

err:
	for (i = i - 1; i >= 0; i--) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		ath12k_mac_hw_destroy(ah);
		ath12k_ag_set_ah(ag, i, NULL);
	}

	return ret;
}

int ath12k_mac_vif_set_keepalive(struct ath12k_link_vif *arvif,
				 enum wmi_sta_keepalive_method method,
				 u32 interval)
{
	struct wmi_sta_keepalive_arg arg = {};
	struct ath12k *ar = arvif->ar;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (arvif->ahvif->vdev_type != WMI_VDEV_TYPE_STA)
		return 0;

	if (!test_bit(WMI_TLV_SERVICE_STA_KEEP_ALIVE, ar->ab->wmi_ab.svc_map))
		return 0;

	arg.vdev_id = arvif->vdev_id;
	arg.enabled = 1;
	arg.method = method;
	arg.interval = interval;

	ret = ath12k_wmi_sta_keepalive(ar, &arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set keepalive on vdev %i: %d\n",
			    arvif->vdev_id, ret);
		return ret;
	}

	return 0;
}

u16 ath12k_calculate_subchannel_count(enum nl80211_chan_width width) {
	u16 width_num = 0;

	switch (width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		width_num = 20;
		break;
	case NL80211_CHAN_WIDTH_40:
		width_num = 40;
		break;
	case NL80211_CHAN_WIDTH_80:
	case NL80211_CHAN_WIDTH_80P80:
		width_num = 80;
		break;
	case NL80211_CHAN_WIDTH_160:
		width_num = 160;
		break;
	case NL80211_CHAN_WIDTH_320:
		width_num = 320;
		break;
	default:
		break;
	}
	return width_num/20;
}

enum ieee80211_neg_ttlm_res
ath12k_mac_op_can_neg_ttlm(struct ieee80211_hw *hw,
			   struct ieee80211_vif *vif,
			   struct ieee80211_neg_ttlm *neg_ttlm)
{
	u8 i;

	/* Verify all TIDs are mapped to the same links
	 * set in the given direction. When disjoint mapping support
	 * enabled, below condition to be removed
	 */
	for (i = 1; i < IEEE80211_TTLM_NUM_TIDS; i++) {
		if (neg_ttlm->downlink[i] != neg_ttlm->downlink[0] ||
		    neg_ttlm->uplink[i] != neg_ttlm->uplink[0])
			return NEG_TTLM_RES_REJECT;
	}

	return NEG_TTLM_RES_ACCEPT;
}
EXPORT_SYMBOL(ath12k_mac_op_can_neg_ttlm);

void ath12k_tid_to_link_mapping_evt_notify(struct ath12k_link_vif *arvif,
					   u16 mapping_switch_tsf,
					   u32 tid_to_link_mapping_status)
{
	if (arvif->is_created)
		ieee80211_advertised_ttlm_evt_notify(arvif->ahvif->vif,
						     mapping_switch_tsf,
						     tid_to_link_mapping_status,
						     arvif->link_id);
}

static void ath12k_mac_handle_ttlm_neg(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_sta *sta)
{
	struct ieee80211_neg_ttlm *neg_ttlm = &sta->neg_ttlm;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_wmi_ttlm_peer_params params = {0};
	struct ath12k_link_sta *link;
	struct ath12k *ar;
	u8 is_default_mapping[IEEE80211_MAX_TTLM_DIRECTION] = {0};
	u8 i;

	lockdep_assert_wiphy(hw->wiphy);

	ath12k_populate_default_mapping_flags(vif, neg_ttlm, is_default_mapping);

	for (i = 0; i < IEEE80211_MLD_MAX_NUM_LINKS; i++) {
		if (!(ahsta->links_map & BIT(i)))
			continue;
		memset(&params, 0, sizeof(struct ath12k_wmi_ttlm_peer_params));
		link = wiphy_dereference(hw->wiphy, ahsta->link[i]);
		if (!link)
			return;
		ar = link->arvif->ar;
		ath12k_populate_wmi_ttlm_peer_params(link, &params, is_default_mapping,
						     neg_ttlm);
		if (ath12k_wmi_send_mlo_peer_tid_to_link_map_cmd(ar, &params, true)) {
			ath12k_warn(ar->ab, "failed to send peer ttlm command");
			return;
		}
	}
}

void ath12k_mac_op_apply_neg_ttlm_per_client(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif,
					     struct ieee80211_sta *sta)
{
	ath12k_mac_handle_ttlm_neg(hw, vif, sta);
}
EXPORT_SYMBOL(ath12k_mac_op_apply_neg_ttlm_per_client);

/**
 * get_mask_details - Derive puncture mask and weight based on bandwidth.
 * @bw: Bandwidth in MHz (e.g., 40, 80, 160).
 *
 * The mapping is as follows:
 *   - 40 MHz  → mask = 0x03  (2 bits)
 *   - 80 MHz  → mask = 0x0F  (4 bits)
 *   - 160 MHz → mask = 0xFF  (8 bits)
 *
 * For unsupported bandwidths, mask is 0.
 *
 * This function is typically used in puncture pattern calculations
 * where the mask is needed to extract or align subchannel information for a
 * given bandwidth.
 *
 * Return: A bitmask with the number of bits set equal to the number of 20 MHz
 * sub-channels.
 */
static u16 get_mask_details(u16 bw)
{
	/* nchans : number of 20 Mhz bands */
	u8 nchans;
	u16 mask;

	switch (bw) {
	case ATH12K_CHWIDTH_20:
	case ATH12K_CHWIDTH_40:
	case ATH12K_CHWIDTH_80:
	case ATH12K_CHWIDTH_160:
	case ATH12K_CHWIDTH_320:
		nchans = bw / ATH12K_CHWIDTH_20;
		break;
	default:
		nchans = 0;
		break;
	}

	mask = (1 << nchans) - 1;
	return mask;
}

/**
 * get_lower_bandwidth_puncture_pattern - Extract puncture pattern for a target bandwidth
 * @prifreq: Primary channel center frequency in MHz.
 * @cur_pat: Current puncture bitmap representing inactive sub-channels.
 * @cur_cenfreq: Center frequency of the current bandwidth in MHz.
 * @cur_bw: Current bandwidth in MHz.
 * @target_bw: Target bandwidth in MHz for which the puncture pattern is needed.
 *
 * This function computes the puncture bitmap for a lower target bandwidth
 * based on the current puncture pattern and channel configuration.
 *
 * It determines the location of the target bandwidth segment within the current
 * bandwidth by calculating the offset of the primary channel from the left-most
 * 20 MHz sub-channel. It then extracts the relevant bits from the current
 * puncture bitmap corresponding to the target bandwidth.
 *
 * An example of converting a 160MHz BW puncture pattern to a 80MHz BW puncture
 * pattern is shown below:

 * |-----------------|pu|--| (current pattern = 0b0100_0000 = 0x40)
 * |-----------|pf|--------| (primary channel = 49 Current bandwidth = 160)
 * |33|37|41|45|49|53|57|61|
 * |-----0-----|-----1-----| (location of target BW in the current bw = 1)

 *             |-----|pu|--| (target pattern = 0b0100 = 0x4)
 *             |pf|--------| (primary channel = 49 target bandwidth = 80)
 *             |49|53|57|61|
 *
 * Example (6 GHz band):
 *   - Current bandwidth: 160 MHz
 *   - Current center frequency: 6185 MHz (Channel 47)
 *   - Primary channel frequency: 6245 MHz (Channel 49)
 *   - Current puncture pattern: 0x40 (binary: 0b0100_0000)
 *
 *   Computation:
 *     start_20mhz_freq = 6185 - 80 + 10 = 6115 MHz (Channel 33)
 *     target_bw_loc_in_curbw = (6245 - 6115) / 80 = 130 / 80 = 1
 *     mask = 0xF (4-bit mask for 80 MHz)
 *     n_20chans_in_target_bw = 80 / 20 = 4
 *     nbits_to_right_shift = 1 * 4 = 4
 *     target_pat = (0x40 >> 4) & 0xF = 0x4
 *
 *   Result:
 *     Extracted 80 MHz puncture pattern = 0x4
 *
 * Return: Puncture bitmap representing inactive sub-channels for the given
 * target bandwidth.
 */
static u16 get_lower_bandwidth_puncture_pattern(u16 prifreq, u16 cur_pat,
						u16 cur_cenfreq, u16 cur_bw,
						u16 target_bw)
{
	/* Location of the target bandwidth in current bandwidth */
	u8 target_bw_loc_in_curbw;
	/* Number of 20 MHz channels in the target bandwidth */
	u8 n_20chans_in_target_bw;
	/* Number of bits for the right shift */
	u8 nbits_to_right_shift;
	/* Center frequency of the left-most/first 20 MHz channel */
	u16 start_20mhz_freq;
	/* Puncture pattern in the target bandwidth */
	u16 target_pat;
	u16 mask;

	if (cur_bw < target_bw)
		return (u16)0xFFFF;

	start_20mhz_freq = cur_cenfreq - (cur_bw / 2) + (ATH12K_CHWIDTH_20 / 2);
	target_bw_loc_in_curbw = (prifreq - start_20mhz_freq) / target_bw;

	n_20chans_in_target_bw = target_bw / ATH12K_CHWIDTH_20;
	nbits_to_right_shift = target_bw_loc_in_curbw * n_20chans_in_target_bw;
	mask = get_mask_details(target_bw);

	target_pat = (cur_pat >> nbits_to_right_shift) & mask;
	return target_pat;
}

/**
 * ath12k_mac_get_punc_pattern_for_bw - Get puncture bitmap for a given
 * bandwidth.
 * @ctx: Channel context configuration.
 * @target_bw: Target bandwidth in MHz.
 *
 * Computes the punctured channel pattern for the specified target bandwidth
 * based on the current channel configuration.
 * Example 1:
 *   - Channel: 33 (6 GHz band)
 *   - Bandwidth: 320 MHz
 *   - Center frequency: 6265 MHz
 *   - Punctured sub-channels: 41 and 45
 *   - Resulting 320 MHz puncture bitmap: 0x000C (bits 2 and 3 set)
 *
 *   If target_bw = 160 MHz:
 *     - Since the primary channel is in the lower half (i.e., channel 33–61),
 *       then the function returns 0x0C (lower 8 bits of the 320 MHz bitmap).
 *     - But if the primary channel is in the upper half (e.g., channel 65–93),
 *       then the function returns 0x00 (upper 8 bits of the 320 MHz bitmap).
 *
 * Example 2:
 *   - Channel: 65 (6 GHz band)
 *   - Bandwidth: 320 MHz
 *   - Center frequency: 6265 MHz
 *   - Punctured sub-channels: 73 and 77
 *   - Resulting 320 MHz bitmap: 0x0C00 (bits 10 and 11 set)
 *
 *   If target_bw = 80 MHz:
 *     - The 320 MHz band is divided into four 80 MHz segments.
 *     - Since the primary channel is in
 *       the third 80 MHz segment (e.g., channel 65–77), the function returns
 *       (0x0C00 >> 8) & 0xF = 0x0C.
 *
 * Return: Puncture bitmap representing inactive sub-channels for given
 * bandwidth.
 */
static u16
ath12k_mac_get_punc_pattern_for_bw(struct ieee80211_chanctx_conf *ctx, u16 target_bw)
{
	u16 pri_freq;
	u16 cfreq;
	u16 cur_bw;
	u16 cur_punc_bitmap;

	cur_punc_bitmap = ctx->def.punctured;
	if (!cur_punc_bitmap)
		return 0;

	pri_freq = ctx->def.chan->center_freq;
	cfreq = ctx->def.center_freq1;
	cur_bw = ath12k_mac_get_chan_width(ctx->def.width);

	return get_lower_bandwidth_puncture_pattern(pri_freq, cur_punc_bitmap,
						    cfreq, cur_bw, target_bw);
}

/**
 * get_punc_bw - Calculate total bandwidth of punctured sub-channels
 * @punc_bitmap: Bitmap representing punctured sub-channels
 *
 * Counts the number of bits set in the puncture bitmap and multiplies
 * by 20 MHz to determine the total bandwidth that is punctured.
 *
 * Return: Total punctured bandwidth in MHz.
 */
static u16 get_punc_bw(u16 punc_bitmap)
{
	u8 count =  hweight16(punc_bitmap);

	return (count * ATH12K_CHWIDTH_20);
}

/**
 * ath12k_mac_compute_oobe_psd - Compute OOBE-based PSD values for each
 * bandwidth
 * @ar: Pointer to ath12k device context
 * @ctx: Channel context configuration
 * @pri_freq: Primary channel frequency
 * @max_bw: Maximum bandwidth in MHz
 * @cfreqs: Array of center frequencies for each bandwidth
 * @oobe_psd: Output array to store computed OOBE PSD values
 *
 * For each supported bandwidth, this function calculates the minimum
 * out-of-band emission (OOBE) PSD value based on puncture patterns and
 * effective bandwidth. The results are stored in the @oobe_psd array.
 */
static void ath12k_mac_compute_oobe_psd(struct ath12k *ar,
					struct ieee80211_chanctx_conf *ctx,
					u16 pri_freq, u16 max_bw, u32 *cfreqs,
					s16 *oobe_psd,
					u8 *num_oobe_psd)
{
	u16 bw;
	int i;

	for (i = 0, bw = ATH12K_CHWIDTH_20; bw <= max_bw; i++, bw *= 2) {
		u16 punc = ath12k_mac_get_punc_pattern_for_bw(ctx, bw);

		ath12_mac_reg_get_6g_min_psd(ar, pri_freq, cfreqs[i], punc, bw,
					     &oobe_psd[i]);
	}
	*num_oobe_psd = i;
}

/**
 * ath12k_mac_fill_subchans - Populate sub-channel center frequencies
 * @sub_chans: Output array to hold sub-channel center frequencies
 * @start_freq: Starting frequency in kHz
 * @n_subchans: Number of 20 MHz sub-channels to generate
 *
 * Fills the @sub_chans array with center frequencies for each 20 MHz
 * sub-channel, starting from @start_freq and incrementing by 20 MHz.
 */
static void
ath12k_mac_fill_subchans(u16 *sub_chans, u32 start_freq, u8 n_subchans)
{
	u8 i;

	for (i = 0; i < n_subchans; i++) {
		sub_chans[i] = start_freq;
		start_freq += ATH12K_CHWIDTH_20;
	}
}

#define ATH12K_INVALID_IDX 0xFF
/**
 * find_start_idx - Find the index of a frequency in a sub-channel list
 * @sub_chans: Array of sub-channel center frequencies
 * @freq: Frequency to locate
 * @n_subchans: Number of sub-channels in the array
 *
 * Searches the @sub_chans array for the given @freq and returns its index.
 *
 * Return: Index of @freq if found, otherwise 0xFF.
 */
static u8
find_start_idx(u16 *sub_chans, u32 freq, u8 n_subchans)
{
	u8 i;

	for (i = 0; i < n_subchans; i++) {
		if (sub_chans[i] == freq) {
			return i;
		}
	}

	ath12k_dbg(NULL, ATH12K_DBG_MAC, "freq = %u, n_subchans = %u\n", freq, n_subchans);
	ath12k_dbg(NULL, ATH12K_DBG_MAC, "subchans:\n");
	for (i = 0; i < n_subchans; i++)
		ath12k_dbg(NULL, ATH12K_DBG_MAC, "%u\n", sub_chans[i]);

	return ATH12K_INVALID_IDX;
}

/**
 * fill_chan_def_for_start_freq - Initialize channel definition for a given bandwidth
 * @ch_def: Pointer to the channel definition structure to initialize
 * @cfreq: Center frequency (in MHz) to assign to center_freq1
 * @bw: Bandwidth in MHz (e.g., ATH12K_CHWIDTH_20, _40, _80, etc.)
 *
 * This helper sets the center frequency and maps the internal bandwidth
 * enumeration to the corresponding NL80211 channel width. It is used to
 * prepare a cfg80211_chan_def structure for start frequency calculations.
 */
static void
fill_chan_def_for_start_freq(struct cfg80211_chan_def *ch_def, u32 cfreq, u16 bw)
{
	ch_def->center_freq1 = cfreq;
	switch (bw) {
	case ATH12K_CHWIDTH_320:
		ch_def->width = NL80211_CHAN_WIDTH_320;
		break;
	case ATH12K_CHWIDTH_160:
		ch_def->width = NL80211_CHAN_WIDTH_160;
		break;
	case ATH12K_CHWIDTH_80:
		ch_def->width = NL80211_CHAN_WIDTH_80;
		break;
	case ATH12K_CHWIDTH_40:
		ch_def->width = NL80211_CHAN_WIDTH_40;
		break;
	case ATH12K_CHWIDTH_20:
		ch_def->width = NL80211_CHAN_WIDTH_20;
		break;
	}
}

/**
 * ath12k_mac_map_psd_to_subchans - Map OOBE PSD values to 20 MHz sub-channels
 * @cfreqs: Array of center frequencies for each bandwidth
 * @oobe_psd: Array of OOBE PSD values per bandwidth
 * @sub_chans: Array of 20 MHz sub-channel center frequencies
 * @n_subchans: Number of sub-channels
 * @max_bw: Maximum bandwidth in MHz
 * @tpc_oobe_psd: Output array to store PSD values mapped to each sub-channel
 *
 * Example,
 * cfreqs = [6115, 6125, 6145, 6185];  - for 20, 40, 80, 160 MHz
 * sub_chans = [6115, 6135, 6155, 6175, 6195, 6215, 6235, 6255];
 * n_subchans = 4;
 * max_bw = 160;
 * oobe_psd[] = {
    22,  - for 20 MHz
    19,  - for 40 MHz
    16,  - for 80 MHz
    13,  - for 160 MHz
 * };
 * tpc_oobe_psd[] = {
    22, - 6115 MHz - 20MHz, oobe PSD value
    19, - 6135 MHz - 40MHz, oobe PSD value
    16, - 6155 MHz - 80MHz, oobe PSD value
    16, - 6175 MHz - 80MHz, oobe PSD value
    13, - 6195 MHz - 160MHz, oobe PSD value
    13, - 6215 MHz - 160MHz, oobe PSD value
    13, - 6235 MHz - 160MHz, oobe PSD value
    13, - 6255 MHz - 160MHz, oobe PSD value
 * };
 * For each bandwidth level, this function determines the starting sub-channel
 * index and fills the corresponding entries in @tpc_oobe_psd with the
 * appropriate OOBE PSD value from @oobe_psd.
 */
static void
ath12k_mac_map_psd_to_subchans(u32 *cfreqs, s16 *oobe_psd, u16 *sub_chans,
			       u8 n_subchans, u16 max_bw, s8 *tpc_oobe_psd, u8 num_oobe_psd)
{
	u16 bw = max_bw;
	int i;

	for (i = num_oobe_psd; i > 0; i--) {
		struct cfg80211_chan_def ch_def = {0};
		u8 num_20mhz_channels;
		u32 start_freq;
		u8 start_idx;
		int j;

		fill_chan_def_for_start_freq(&ch_def, cfreqs[i - 1], bw);
		start_freq = ath12k_mac_get_6g_start_frequency(&ch_def);
		num_20mhz_channels = bw / ATH12K_CHWIDTH_20;
		start_idx = find_start_idx(sub_chans, start_freq, n_subchans);
		if (start_idx == ATH12K_INVALID_IDX) {
			ath12k_err(NULL, "Filling PSD TPC with -127dBm\n");
			for (i = 0; i < n_subchans; i++)
				tpc_oobe_psd[i] = -ATH12K_MAX_TX_POWER;
			return;
		}

		for (j = start_idx;
		     num_20mhz_channels > 0 && j < ATH12K_NUM_PWR_LEVELS;
		     j++, num_20mhz_channels--)
			tpc_oobe_psd[j] = oobe_psd[i - 1];

		bw /= 2;
	}
}

/**
 * ath12k_mac_finalize_psd_table - Finalize PSD power table with regulatory constraints
 * @ar: Pointer to ath12k device context
 * @reg_tpc_info: Pointer to TPC power info structure to populate
 * @sub_chans: Array of 20 MHz sub-channel center frequencies
 * @tpc_oobe_psd: Array of OOBE-based PSD values mapped to sub-channels
 * @ctx: Channel context configuration
 *
 * For each sub-channel, this function sets the channel center frequency and
 * determines the final transmit power by applying the minimum of OOBE-based
 * and regulatory PSD limits. The results are stored in the PSD power info table.
 */
static void
ath12k_mac_finalize_psd_table(struct ath12k *ar,
			      struct ath12k_reg_tpc_power_info *reg_tpc_info,
			      u16 *sub_chans, s8 *tpc_oobe_psd,
			      struct ieee80211_chanctx_conf *ctx)
{
	u16 start_freq = sub_chans[0];
	s8 txpower;
	int i;

	for (i = 0; i < reg_tpc_info->num_psd_pwr_levels; i++) {
		u16 cfreq;
		struct ieee80211_channel *temp_chan;
		s16 reg_psd = ATH12K_MAX_TX_POWER;

		ath12k_mac_get_psd_channel(ar, ATH12K_CHWIDTH_20, &start_freq,
					   &cfreq, i, &temp_chan, &txpower,
					   IEEE80211_REG_SP_AP);

		if (temp_chan) {
			reg_psd = temp_chan->psd;
			if (reg_tpc_info->power_type_6g == REG_SP_CLIENT_TYPE)
				reg_psd -= ATH12K_SP_AP_AND_CLIENT_POWER_DIFF_IN_DBM;
		}

		reg_tpc_info->chan_psd_power_info[i].chan_cfreq = sub_chans[i];
		reg_tpc_info->chan_psd_power_info[i].tx_power =
				min(tpc_oobe_psd[i], reg_psd);

		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "freq %u tpc_oobe_psd %d reg_psd %d\n",
			   sub_chans[i], tpc_oobe_psd[i], reg_psd);
	}
}

/**
 * ath12k_mac_fill_reg_tpc_info_with_psd_for_sp_pwr_mode - Populate PSD power
 * info for SP AP mode
 * @ar: Pointer to ath12k device context
 * @arvif: Virtual interface context
 * @ctx: Channel context configuration
 *
 * Computes and fills the Power Spectral Density (PSD) transmit power levels
 * for each 20 MHz sub-channel in 6 GHz Standard Power (SP) AP mode. It uses
 * puncture patterns and regulatory constraints to determine the effective
 * transmit power per sub-channel and stores the results in the PSD power info
 * table of the virtual interface.
 */
static void
ath12k_mac_fill_reg_tpc_info_with_psd_for_sp_pwr_mode(struct ath12k *ar,
						      struct ath12k_link_vif *arvif,
						      struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k_reg_tpc_power_info *reg_tpc_info = &arvif->reg_tpc_info;
	u16 max_bw = ath12k_mac_get_chan_width(ctx->def.width);
	u16 sub_chans[ATH12K_NUM_PWR_LEVELS];
	u16 pri_freq = ctx->def.chan->center_freq;
	s16 oobe_psd[ATH12K_MAX_EIRP_VALS];
	s8 tpc_oobe_psd[ATH12K_NUM_PWR_LEVELS];
	u32 cfreqs[ATH12K_MAX_EIRP_VALS];
	u8 num_oobe_psd;
	u32 start_freq;
	u8 n_subchans;

	reg_tpc_info->num_psd_pwr_levels =
			ath12k_mac_get_num_pwr_levels(&ctx->def, true);
	ath12k_mac_fill_cfreqs(&ctx->def, cfreqs);
	ath12k_mac_compute_oobe_psd(ar, ctx, pri_freq, max_bw, cfreqs,
				    oobe_psd, &num_oobe_psd);
	start_freq = ath12k_mac_get_6g_start_frequency(&ctx->def);
	n_subchans = max_bw / ATH12K_CHWIDTH_20;
	ath12k_mac_fill_subchans(sub_chans, start_freq, n_subchans);
	ath12k_mac_map_psd_to_subchans(cfreqs, oobe_psd, sub_chans, n_subchans,
				       max_bw, tpc_oobe_psd, num_oobe_psd);
	ath12k_mac_finalize_psd_table(ar, reg_tpc_info, sub_chans, tpc_oobe_psd,
				      ctx);
}

/**
 * ath12k_mac_fill_eirp_power_table - Fill EIRP power table with regulatory
 * limits.
 * @reg_tpc_info: Pointer to TPC power info structure.
 * @cfreqs: Array of center frequencies.
 * @oobe_eirp: Array of OOBE-based EIRP values.
 * @reg_psd: Regulatory PSD limit.
 * @reg_eirp: Regulatory EIRP limit.
 * @max_bw: Maximum bandwidth.
 */
static void
ath12k_mac_fill_eirp_power_table(struct ath12k *ar,
				 struct ath12k_reg_tpc_power_info *reg_tpc_info,
				 u32 *cfreqs, s16 *oobe_eirp, s8 reg_psd,
				 s8 reg_eirp, u16 max_bw)
{
	u16 bw;
	int i;

	for (i = 0, bw = ATH12K_CHWIDTH_20; bw <= max_bw; i++, bw *= 2) {
		s16 eirp_from_psd = ath12k_reg_psd_2_eirp(reg_psd, bw);
		s16 reg_eirp_tpc = min(eirp_from_psd, reg_eirp);
		struct chan_power_info *eirp_pwr_info =
				&reg_tpc_info->chan_eirp_power_info[i];

		ath12k_dbg(ar->ab,
			   ATH12K_DBG_MAC,
			   "cfreq %u oobe_eirp %d reg_eirp %d reg_psd_to_eirp %d\n",
			   cfreqs[i], oobe_eirp[i], reg_eirp, eirp_from_psd);
		eirp_pwr_info->chan_cfreq = cfreqs[i];
		eirp_pwr_info->tx_power = min(reg_eirp_tpc, oobe_eirp[i]);
	}
}

/**
 * ath12k_mac_compute_oobe_eirp - Compute OOBE-based EIRP values for each
 * bandwidth. If the bandwidth is punctured, then oobe PSD value is used and
 * converted to EIRP, else EIRP is taken from AFC response.
 * @ar: Pointer to ath12k device context
 * @ctx: Channel context configuration
 * @pri_freq: Primary channel frequency
 * @max_bw: Maximum bandwidth
 * @cfreqs: Array of center frequencies
 * @oobe_eirp: Output array for computed EIRP values
 */
static void ath12k_mac_compute_oobe_eirp(struct ath12k *ar,
					 struct ieee80211_chanctx_conf *ctx,
					 u16 pri_freq, u16 max_bw, u32 *cfreqs,
					 s16 *oobe_eirp)
{
	u16 bw;
	int i;

	for (i = 0, bw = ATH12K_CHWIDTH_20; bw <= max_bw; i++, bw *= 2) {
		u16 punc_pattern = ath12k_mac_get_punc_pattern_for_bw(ctx, bw);

		if (punc_pattern) {
			s16 min_psd;
			u16 eff_bw;

			ath12_mac_reg_get_6g_min_psd(ar, pri_freq, cfreqs[i],
						     punc_pattern, bw, &min_psd);
			eff_bw = bw - get_punc_bw(punc_pattern);
			oobe_eirp[i] = ath12k_reg_psd_2_eirp(min_psd, eff_bw);
		} else {
			oobe_eirp[i] =
				ath12k_mac_get_afc_eirp_power(ar, pri_freq,
							      cfreqs[i], bw);
		}
	}
}

/**
 * ath12k_mac_fill_reg_tpc_info_with_eirp_for_sp_pwr_mode - Populate EIRP power
 * info for SP AP mode
 * @ar: Pointer to ath12k device context
 * @arvif: Virtual interface context
 * @ctx: Channel context configuration
 *
 * Calculates and fills EIRP (Equivalent Isotropically Radiated Power) values
 * for each supported bandwidth in 6 GHz Standard Power (SP) AP mode. It uses
 * puncture patterns to determine effective bandwidth and computes the minimum
 * EIRP based on regulatory and OOBE (out-of-band emissions) constraints.
 */
static void
ath12k_mac_fill_reg_tpc_info_with_eirp_for_sp_pwr_mode(struct ath12k *ar,
						       struct ath12k_link_vif *arvif,
						       struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k_reg_tpc_power_info *reg_tpc_info = &arvif->reg_tpc_info;
	s16 oobe_eirp[ATH12K_MAX_EIRP_VALS];
	u32 cfreqs[ATH12K_MAX_EIRP_VALS];
	s8 reg_psd, reg_eirp;
	u16 pri_freq;
	u16 max_bw;

	reg_tpc_info->power_type_6g = ath12k_ieee80211_ap_pwr_type_convert(IEEE80211_REG_SP_AP);
	reg_tpc_info->num_eirp_pwr_levels = ath12k_mac_get_num_pwr_levels(&ctx->def, false);

	pri_freq = ctx->def.chan->center_freq;
	max_bw = ath12k_mac_get_chan_width(ctx->def.width);
	ath12k_mac_fill_cfreqs(&ctx->def, cfreqs);
	ath12k_mac_compute_oobe_eirp(ar, ctx, pri_freq, max_bw, cfreqs,
				     oobe_eirp);
	ath12k_reg_get_regulatory_pwrs(ar, MHZ_TO_KHZ(pri_freq),
				       NL80211_REG_AP_SP, &reg_eirp, &reg_psd);
	ath12k_mac_fill_eirp_power_table(ar, reg_tpc_info, cfreqs, oobe_eirp,
					 reg_psd, reg_eirp, max_bw);
}

void
ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_sp(struct ath12k *ar,
						      struct ath12k_link_vif *arvif,
						      struct ieee80211_chanctx_conf *ctx)
{
	ath12k_mac_fill_reg_tpc_info_with_psd_for_sp_pwr_mode(ar, arvif, ctx);
	ath12k_mac_fill_reg_tpc_info_with_eirp_for_sp_pwr_mode(ar, arvif, ctx);
}

/**
 * ath12k_mac_init_root_tpe - Initialize local TPE buffer pointers
 * @reg_tpc_info: Pointer to the TPC power info structure
 * @tpe: Pointer to the local s8* variable to be initialized
 * @is_psd: Boolean flag indicating whether to initialize PSD (true) or EIRP (false)
 *
 * This helper function assigns the appropriate pre-allocated buffer from
 * reg_tpc_info to the local pointer `tpe`, and conditionally initializes
 * the buffer with ATH12K_MAX_TX_POWER if the corresponding num_tpe_* field is zero.
 *
 * This avoids repetitive code in functions that need to initialize either
 * tpe_psd or tpe_eirp and ensures consistent handling of default power values.
 */
static void ath12k_mac_init_root_tpe(struct ath12k_reg_tpc_power_info *reg_tpc_info,
				     s8 **tpe, bool is_psd)
{
	if (is_psd) {
		*tpe = reg_tpc_info->tpe_psd;
		if (!reg_tpc_info->num_tpe_psd)
			memset(*tpe, ATH12K_MAX_TX_POWER,
			       ATH12K_NUM_PWR_LEVELS * sizeof(s8));
	} else {
		*tpe = reg_tpc_info->tpe_eirp;
		if (!reg_tpc_info->num_tpe_eirp)
			memset(*tpe, ATH12K_MAX_TX_POWER, ATH12K_MAX_EIRP_VALS * sizeof(s8));
	}
}

/**
 * ath12k_mac_fill_reg_tpc_info_with_psd_for_client_sp_pwr_mode - Finalize
 * PSD-based reg TPC for client SP
 * @ar: Pointer to ath12k device structure
 * @arvif: Pointer to ath12k virtual interface structure
 * @chanctx: Pointer to channel context configuration
 *
 * Uses pre-filled tpe_psd[] (OOBE-bound) and regulatory PSD limits to compute
 * min(TPE, reg) per sub-CH. Populates chan_psd_power_info[] in reg_tpc_info.
 * Output is used in WMI reg TPC TLV for STA in SP mode (6 GHz).
 */
static void
ath12k_mac_fill_reg_tpc_info_with_psd_for_client_sp_pwr_mode(struct ath12k *ar,
							     struct ath12k_link_vif *arvif,
							     struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k_reg_tpc_power_info *reg_tpc_info = &arvif->reg_tpc_info;
	u16 max_bw = ath12k_mac_get_chan_width(ctx->def.width);
	u16 sub_chans[ATH12K_NUM_PWR_LEVELS];
	s8 *tpe_psd;
	u32 start_freq;
	u8 n_subchans;

	reg_tpc_info->power_type_6g = REG_SP_CLIENT_TYPE;
	start_freq = ath12k_mac_get_6g_start_frequency(&ctx->def);
	n_subchans = max_bw / ATH12K_CHWIDTH_20;
	ath12k_mac_fill_subchans(sub_chans, start_freq, n_subchans);
	reg_tpc_info->num_psd_pwr_levels = n_subchans;
	ath12k_mac_init_root_tpe(reg_tpc_info, &tpe_psd, true);
	ath12k_mac_finalize_psd_table(ar, reg_tpc_info, sub_chans, tpe_psd, ctx);
}

/**
 * ath12k_mac_get_min_psd_for_eirp - Get min PSD from tpe_psd[] for given sub-CH range
 * @ar: ath12k HW context
 * @sub_chans: sub-CH freqs (MHz)
 * @start_freq: starting freq (MHz) for current BW
 * @n_subchans: number of sub-CHs in current BW
 * @tpe_psd: PSD-based TPE array (OOBE-bound)
 * @max_n_subchans: number of sub-channels in sub-channel array
 * @punc: puncture pattern for current BW
 *
 * Finds the min(TPE) from tpe_psd[] for the sub-CHs starting at @start_freq.
 * Used to derive EIRP from PSD for client SP mode.
 * Returns the minimum PSD value found, skipping punctured sub-channels.
 */
static s8
ath12k_mac_get_min_psd_for_eirp(struct ath12k *ar, u16 *sub_chans,
				u32 start_freq, u8 n_subchans, s8 *tpe_psd,
				u8 max_n_subchans, u16 punc)
{
	u8 start_idx = find_start_idx(sub_chans, start_freq, max_n_subchans);
	s8 min_psd = ATH12K_MAX_TX_POWER;
	u8 i, j;

	for (i = 0, j = start_idx; i < n_subchans && j < max_n_subchans; i++, j++) {
		if (punc & (1 << i))
			continue;

		if (tpe_psd[j] < min_psd)
			min_psd = tpe_psd[j];
	}

	return min_psd;
}

/**
 * ath12k_mac_fill_eirp_power_level - Fill EIRP-based reg TPC entry for a given BW
 * @ar: ath12k HW context
 * @ctx: CHANCTX config
 * @reg_tpc_info: reg TPC info to be updated
 * @idx: power level index
 * @cfreq: center freq (MHz) for current BW
 * @bw: bandwidth (MHz)
 * @tpe_psd: PSD-based TPE array (OOBE-bound)
 * @tpe_eirp: EIRP-based TPE array
 * @reg_eirp: regulatory EIRP limits
 * @sub_chans: sub-CH freqs (MHz)
 * @max_n_subchans: number of sub-channel frequencies
 *
 * Computes min(TPE, reg) EIRP for the given BW using:
 * - min PSD from tpe_psd[] → converted to EIRP
 * - tpe_eirp[] and reg_eirp[] for the same BW
 *
 * Updates chan_eirp_power_info[idx] with final TXP and CFREQ.
 */
static void
ath12k_mac_fill_eirp_power_level(struct ath12k *ar,
				 struct ieee80211_chanctx_conf *ctx,
				 struct ath12k_reg_tpc_power_info *reg_tpc_info,
				 u8 idx, u32 cfreq, u16 bw,
				 s8 *tpe_psd, s8 *tpe_eirp, s8 *reg_eirp,
				 u16 *sub_chans,
				 u8 max_n_subchans)
{
	struct chan_power_info *eirp_pwr_info = &reg_tpc_info->chan_eirp_power_info[idx];
	u16 punc = ath12k_mac_get_punc_pattern_for_bw(ctx, bw);
	u16 eff_bw = bw - get_punc_bw(punc);
	struct cfg80211_chan_def ch_def = {0};
	u16 start_freq;
	s8 min_psd, eirp_psd, min_eirp;
	u8 n_subchans;

	fill_chan_def_for_start_freq(&ch_def, cfreq, bw);
	start_freq = ath12k_mac_get_6g_start_frequency(&ch_def);
	n_subchans = bw / ATH12K_CHWIDTH_20;
	min_psd = ath12k_mac_get_min_psd_for_eirp(ar, sub_chans, start_freq,
						  n_subchans, tpe_psd, max_n_subchans,
						  punc);
	eirp_psd = ath12k_reg_psd_2_eirp(min_psd, eff_bw);

	min_eirp = min(min(tpe_eirp[idx], eirp_psd), reg_eirp[idx]);

	eirp_pwr_info->chan_cfreq = cfreq;
	eirp_pwr_info->tx_power = min_eirp;
	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "BW %d EBW %d cf %u tpe_psd %d tpe_psd_eirp %d tpe_eirp %d reg_eirp %d\n",
		   bw, eff_bw, cfreq, min_psd, eirp_psd, tpe_eirp[idx], reg_eirp[idx]);
}

/**
 * ath12k_mac_fill_reg_tpc_info_with_eirp_for_client_sp_pwr_mode - Fill
 * EIRP-based reg TPC for client SP
 * @ar: Pointer to ath12k device structure
 * @arvif: Pointer to ath12k virtual interface structure
 * @chanctx: Pointer to channel context configuration
 *
 * Computes EIRP-based reg TPC for STA in SP mode (6 GHz). Uses tpe_psd[] to derive
 * EIRP from PSD, and compares it with tpe_eirp[] and reg_eirp[] to compute min(TPE, reg).
 * Populates chan_eirp_power_info[] in reg_tpc_info. Output is used in WMI reg TPC TLV.
 */
static void
ath12k_mac_fill_reg_tpc_info_with_eirp_for_client_sp_pwr_mode(struct ath12k *ar,
							      struct ath12k_link_vif *arvif,
							      struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k_reg_tpc_power_info *reg_tpc_info = &arvif->reg_tpc_info;
	s8 reg_eirp[ATH12K_MAX_EIRP_VALS];
	u32 cfreqs[ATH12K_MAX_EIRP_VALS];
	static const u16 bw[] = {ATH12K_CHWIDTH_20, ATH12K_CHWIDTH_40, ATH12K_CHWIDTH_80,
				 ATH12K_CHWIDTH_160, ATH12K_CHWIDTH_320};
	u16 sub_chans[ATH12K_NUM_PWR_LEVELS];
	u16 max_bw = ath12k_mac_get_chan_width(ctx->def.width);
	u8 max_n_subchans = max_bw / ATH12K_CHWIDTH_20;
	s8 *tpe_eirp, *tpe_psd;
	u8 num_pwr_levels, i;
	u32 start_freq;

	start_freq = ath12k_mac_get_6g_start_frequency(&ctx->def);
	ath12k_mac_fill_subchans(sub_chans, start_freq, max_n_subchans);

	num_pwr_levels = ath12k_mac_get_num_pwr_levels(&ctx->def, false);
	if (num_pwr_levels > ATH12K_MAX_EIRP_VALS) {
		ath12k_err(NULL, "num_pwr_levels should not be greater than ATH12K_MAX_EIRP_VALS");
		return;
	}
	reg_tpc_info->num_eirp_pwr_levels = num_pwr_levels;

	ath12k_mac_get_sp_client_power_for_connecting_ap(ar, ctx, reg_eirp,
							 num_pwr_levels);

	ath12k_mac_init_root_tpe(reg_tpc_info, &tpe_psd, true);
	ath12k_mac_init_root_tpe(reg_tpc_info, &tpe_eirp, false);
	ath12k_mac_fill_cfreqs(&ctx->def, cfreqs);
	for (i = 0; i < num_pwr_levels; i++)
		ath12k_mac_fill_eirp_power_level(ar, ctx, reg_tpc_info, i, cfreqs[i],
						 bw[i], tpe_psd, tpe_eirp, reg_eirp,
						 sub_chans, max_n_subchans);
}

void
ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_client_sp(struct ath12k *ar,
							     struct ath12k_link_vif *arvif,
							     struct ieee80211_chanctx_conf *ctx)
{
	ath12k_mac_fill_reg_tpc_info_with_psd_for_client_sp_pwr_mode(ar, arvif, ctx);
	ath12k_mac_fill_reg_tpc_info_with_eirp_for_client_sp_pwr_mode(ar, arvif, ctx);
}

static void
ath12k_prepare_scs_desc_resp(struct cfg80211_qm_req_desc_data *qm_req_desc,
			     struct cfg80211_qm_resp_desc_data *qm_resp_desc,
			     u8 status)
{
	qm_resp_desc->qm_id = qm_req_desc->qm_id;
	qm_resp_desc->status = status;
}

static
void ath12k_compute_qos_params(struct cfg80211_qm_qos_attributes *qos_attr,
			       u32 *s_int, u32 *b_size)
{
	u32 service_interval, burst_size, data_rate;

	service_interval = ((qos_attr->min_service_interval +
			   qos_attr->max_service_interval) >> 1);

	/* Convert microseconds to milliseconds */
	service_interval = service_interval / 1000;

	/* Data rate in Kilo Bytes Per Second */
	data_rate = qos_attr->min_data_rate / 8;

	/* Resultant burst size (in bytes) is computed as minimum of the
	 * following 3 calculations:
	 * 1. Resultant Service Interval * Minimum Data Rate
	 * 2. Resultant Service Interval * Mean Data Rate (if rcvd from station)
	 * 3. Burst Size (if rcvd from station in SCS request)
	 */
	burst_size = service_interval * data_rate;
	if (qos_attr->mean_data_rate) {
		data_rate = qos_attr->mean_data_rate / 8;
		burst_size = (burst_size < (service_interval * data_rate)) ?
			      burst_size : service_interval * data_rate;
	}

	if (qos_attr->burst_size) {
		burst_size = (burst_size < qos_attr->burst_size) ?
			      burst_size : qos_attr->burst_size;
	}

	*s_int = service_interval;
	*b_size = burst_size;
}

static
void ath12k_copy_qos_params(struct ath12k_qos_params *params,
			    struct cfg80211_qm_qos_attributes *qos_attr)
{
	u32 service_interval, burst_size;

	/* Derive the QoS params by converting the units
	 * and forms as provided in the spec to units ans forms supported
	 * by our FW scheduler. Only update the prameters that is
	 * currenlty supported by our FW.
	 */
	ath12k_compute_qos_params(qos_attr, &service_interval, &burst_size);

	params->tid = qos_attr->tid;
	if (qos_attr->min_service_interval)
		params->min_service_interval = service_interval;

	params->min_data_rate = qos_attr->min_data_rate;

	/* Convert microseconds to miliseconds */
	if (qos_attr->delay_bound)
		params->delay_bound = qos_attr->delay_bound / 1000;

	if (qos_attr->burst_size)
		params->burst_size = burst_size;

	if (qos_attr->msdu_lifetime)
		params->msdu_life_time = qos_attr->msdu_lifetime;
}

static int ath12k_process_scs_add(struct ath12k *ar, struct ath12k_sta *ahsta,
				  u16 peer_id, enum qos_profile_dir qos_dir,
				  struct cfg80211_qm_req_desc_data *qm_req,
				  u8 *addr)
{
	struct cfg80211_qm_qos_attributes *qos_attr;
	struct ath12k_qos_params params = {0};
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_link_sta *arsta;
	struct ath12k *temp_ar;
	unsigned long links;
	int ret = -EINVAL;
	u16 qos_id;
	u8 link_id;
	u8 qm_id;

	qos_attr = &qm_req->qos_attr;
	qm_id = qm_req->qm_id;

	if (!qm_req->is_qos_present) {
		qos_id = ath12k_qos_get_legacy_id(ar->ab, qm_req->priority);
	} else {
		ath12k_qos_set_default(&params);
		ath12k_copy_qos_params(&params, qos_attr);
		qos_id = ath12k_qos_configure(ar->ab, NULL, &params, qos_dir,
					      NULL);
	}

	if (qos_id == QOS_ID_INVALID)
		return ret;

	if (qos_dir == QOS_PROFILE_UL) {
		links = ahsta->links_map;

		for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
			arsta = ahsta->link[link_id];

			if (!arsta || !arsta->arvif || !arsta->arvif->ar)
				continue;

			temp_ar = arsta->arvif->ar;

			ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
				   "Configure UL qos for link:%d", link_id);
			ret = ath12k_core_config_ul_qos(temp_ar, &params,
							qos_id, arsta->addr,
							true);

			if (ret) {
				ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
					   "Configure UL qos failed, link:%d",
					   link_id);
				break;
			}
		}

		if (ret) {
			ath12k_qos_disable(ar->ab, NULL, QOS_PROFILE_UL, qos_id,
					   NULL);
			return ret;
		}
	}

	rcu_read_lock();
	peer = ath12k_dp_link_peer_find_by_peerid_index(ar->ab->dp, &ar->dp,
							peer_id);
	if (!peer) {
		ath12k_err(ar->ab, "SCS peer is NULL");
		ret = -EINVAL;
		goto ret;
	}

	if (!peer->dp_peer->qos) {
		qos = ath12k_dp_peer_qos_alloc(ar->ab->dp, peer->dp_peer);
		if (!qos) {
			ath12k_err(ar->ab, "SCS QoS is NULL");
			ret = -EINVAL;
			goto ret;
		}
	} else {
		qos = peer->dp_peer->qos;
	}

	ret =  ath12k_dp_peer_scs_add(ar->ab, qos, qm_id, qos_id);
ret:
	rcu_read_unlock();
	return ret;
}

static int ath12k_process_scs_del(struct ath12k *ar, struct ath12k_sta *ahsta,
				  u16 peer_id,
				  struct cfg80211_qm_req_desc_data *qm_req,
				  u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_qos_params params;
	struct ath12k_qos_ctx *qos_ctx;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_link_sta *arsta;
	enum qos_profile_dir qos_dir;
	u8 qm_id = qm_req->qm_id;
	struct ath12k *temp_ar;
	unsigned long links;
	int ret = -EINVAL;
	u16 qos_id;
	u8 link_id;

	rcu_read_lock();
	peer = ath12k_dp_link_peer_find_by_peerid_index(ar->ab->dp, &ar->dp,
							peer_id);
	if (!peer) {
		ath12k_err(ar->ab, "SCS peer is NULL");
		rcu_read_unlock();
		return ret;
	}

	qos = peer->dp_peer->qos;
	if (!qos) {
		ath12k_err(ar->ab, "SCS QoS is NULL");
		rcu_read_unlock();
		return ret;
	}

	qos_id = ath12k_dp_peer_scs_get_qos_id(ar->ab, qos, qm_id);
	ret = ath12k_dp_peer_scs_del(ar->ab, qos, qm_id);

	rcu_read_unlock();

	if (qos_id < QOS_LEGACY_DL_ID_MIN) {
		if (qos_id >= QOS_DL_ID_MIN && qos_id <= QOS_DL_ID_MAX)
			qos_dir = QOS_PROFILE_DL;
		else if (qos_id >= QOS_UL_ID_MIN && qos_id <= QOS_UL_ID_MAX)
			qos_dir = QOS_PROFILE_UL;
	} else {
		/* No Qos Profile Disable required for Legacy QoS ID*/
		return 0;
	}

	if (qos_dir == QOS_PROFILE_UL) {
		qos_ctx = ath12k_get_qos(ar->ab);
		if (!qos_ctx) {
			ath12k_err(ar->ab, "QoS Context is NULL");
			return -EINVAL;
		}

		if (qos_id < QOS_UL_ID_MIN || qos_id > QOS_UL_ID_MAX) {
			ath12k_err(ar->ab, "Invalid  QoS ID: %d", qos_id);
			return -EINVAL;
		}

		links = ahsta->links_map;

		spin_lock_bh(&qos_ctx->profile_lock);
		memcpy(&params, &qos_ctx->profiles[qos_id].params,
		       sizeof(struct ath12k_qos_params));
		spin_unlock_bh(&qos_ctx->profile_lock);

		for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
			arsta = ahsta->link[link_id];

			if (!arsta || !arsta->arvif || !arsta->arvif->ar)
				continue;

			temp_ar = arsta->arvif->ar;

			ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
				   "Disabling UL qos for link:%d", link_id);
			ath12k_core_config_ul_qos(temp_ar, &params, qos_id,
						  arsta->addr,	false);
		}
	}

	if (ret == 0) {
		if (qos_id < QOS_LEGACY_DL_ID_MIN)
			ret = ath12k_qos_disable(ar->ab, NULL, qos_dir,
						 qos_id, NULL);
	}

	return ret;
}

static
int ath12k_process_scs_desc(struct ath12k *ar, struct ath12k_sta *ahsta,
			    u16 peer_id,
			    struct cfg80211_qm_req_desc_data *qm_req_desc,
			    u8 *addr)
{
	u8 request_type = qm_req_desc->request_type;
	u8 dir = IEEE80211_QM_DIRECTION_DOWNLINK;
	int status = IEEE80211_QM_REQ_SUCCESS;
	enum qos_profile_dir qos_dir;
	int ret;

	if (qm_req_desc->is_qos_present)
		dir = qm_req_desc->qos_attr.direction;

	if (dir == IEEE80211_QM_DIRECTION_UPLINK) {
		qos_dir = QOS_PROFILE_UL;
	} else if (dir == IEEE80211_QM_DIRECTION_DOWNLINK) {
		qos_dir = QOS_PROFILE_DL;
	} else {
		ath12k_err(ar->ab, "SCS Add failed: SCS ID: %d", dir);
		return IEEE80211_QM_REQ_DECLINED;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_QOS, "Process scs descriptor in driver");
	ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
		   "STA:%pM, SCS ID:%u, Request type:%u, Num TCLAS:%u",
		   addr, qm_req_desc->qm_id, qm_req_desc->request_type,
		   qm_req_desc->num_tclas_elements);
	ath12k_dbg(ar->ab, ATH12K_DBG_QOS, "QoS present:%d, Priority:%u",
		   qm_req_desc->is_qos_present, qm_req_desc->priority);

	switch (request_type) {
	case IEEE80211_QM_ADD_REQ:
		ret = ath12k_process_scs_add(ar, ahsta, peer_id, qos_dir,
					     qm_req_desc, addr);

		if (ret != 0) {
			ath12k_err(ar->ab, "SCS Add failed: SCS ID: %d",
				   qm_req_desc->qm_id);
			status = IEEE80211_QM_REQ_DECLINED;
		}
		break;

	case IEEE80211_QM_REMOVE_REQ:
		ret = ath12k_process_scs_del(ar, ahsta, peer_id, qm_req_desc, addr);
		if (ret != 0) {
			ath12k_err(ar->ab, "SCS Delete Failed: SCS ID: %d",
				   qm_req_desc->qm_id);
			status = IEEE80211_QM_REQ_DECLINED;
		}
		break;

	case IEEE80211_QM_CHANGE_REQ:
		ret = ath12k_process_scs_del(ar, ahsta, peer_id, qm_req_desc, addr);

		if (ret != 0) {
			ath12k_err(ar->ab, "SCS Update DEL failed: SCS ID: %d",
				   qm_req_desc->qm_id);
			status = IEEE80211_QM_REQ_DECLINED;
			return status;
		}

		ret = ath12k_process_scs_add(ar, ahsta, peer_id, qos_dir,
					     qm_req_desc, addr);

		if (ret != 0) {
			ath12k_err(ar->ab, "SCS Update ADD failed: SCS ID: %d",
				   qm_req_desc->qm_id);
			status = IEEE80211_QM_REQ_DECLINED;
		}
		break;

	default:
		ath12k_err(ar->ab, "Invalid SCS request type\n");
		status = IEEE80211_QM_REQ_DECLINED;
	}

	return status;
}

static int
ath12k_mac_set_scs(struct ieee80211_hw *hw, struct ath12k_link_sta *arsta,
		   struct ath12k_sta *ahsta,
		   struct cfg80211_qm_req_data *qm_req,
		   struct cfg80211_qm_resp_data *qm_resp)
{
	struct cfg80211_qm_resp_desc_data *qm_resp_desc;
	struct cfg80211_qm_req_desc_data *qm_req_desc;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp *dp;
	struct ath12k *ar;
	u8 addr[ETH_ALEN];
	u8 num_scs_desc;
	u16 peer_id;
	int idx = 0;
	int status;

	ar = arsta->arvif->ar;
	dp = ath12k_ab_to_dp(ar->ab);

	spin_lock_bh(&dp->dp_lock);
	link_peer = ath12k_dp_link_peer_find_by_addr(dp, arsta->addr);
	if (!link_peer) {
		spin_unlock_bh(&dp->dp_lock);
		return -ENOENT;
	}

	peer_id = link_peer->peer_id;
	memcpy(addr, link_peer->addr, ETH_ALEN);
	spin_unlock_bh(&dp->dp_lock);

	num_scs_desc = qm_req->num_qm_desc;

	while (idx < num_scs_desc) {
		qm_req_desc = &qm_req->qm_req_desc[idx];

		status = ath12k_process_scs_desc(ar, ahsta, peer_id,
						 qm_req_desc, addr);
		qm_resp_desc = &qm_resp->qm_resp_desc[idx];
		ath12k_prepare_scs_desc_resp(qm_req_desc, qm_resp_desc,
					     status);
		idx++;
	}

	return 0;
}

int ath12k_mac_op_qos_mgmt_cfg(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta,
			       struct cfg80211_qm_req_data *qm_req,
			       struct cfg80211_qm_resp_data *qm_resp)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	enum ieee80211_qos_mgmt_type qm_type = qm_req->qm_type;
	u8 link_id = ahsta->primary_link_id;
	struct ath12k_link_sta *arsta;

	lockdep_assert_wiphy(hw->wiphy);

	arsta = wiphy_dereference(hw->wiphy, ahsta->link[link_id]);

	if (!arsta) {
		ath12k_err(NULL, "QoS mgmt arsta NULL link_id %d sta %pM\n",
			   link_id, sta->addr);
		return -EINVAL;
	}

	switch (qm_type) {
	case IEEE80211_QM_TYPE_SCS:
		return ath12k_mac_set_scs(hw, arsta, ahsta, qm_req, qm_resp);

	case IEEE80211_QM_TYPE_MSCS:
		return ath12k_mac_set_mscs(hw, arsta, ahsta, qm_req, qm_resp);

	default:
		ath12k_err(NULL, "Invalid QM Protocol\n");
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ath12k_mac_op_qos_mgmt_cfg);

/**
 * ath12k_get_nl_ap_pwr_mode - Map WMI AP/client power mode to NL80211 regulatory mode
 * @ap_mode: AP power mode (WMI_REG_INDOOR_AP, WMI_REG_STD_POWER_AP, etc.)
 * @client_type: Client type (WMI_REG_DEFAULT_CLIENT, WMI_REG_SUBORDINATE_CLIENT)
 * @is_client_needed: Indicates whether the mapping is for a client or AP
 *
 * This function translates the WMI-defined AP or client power mode into the
 * corresponding NL80211 regulatory power mode used for 6 GHz band configuration.
 * It handles both AP and client mappings based on the input flags.
 *
 * Return: A valid enum nl80211_regulatory_power_modes value on success,
 *         or NL80211_REG_NUM_POWER_MODES if the input combination is invalid.
 */
static enum nl80211_regulatory_power_modes
ath12k_get_nl_ap_pwr_mode(enum wmi_reg_6g_ap_type ap_mode,
			  enum wmi_reg_6g_client_type client_type,
			  bool is_client_needed)
{
	if (!is_client_needed) {
		switch (ap_mode) {
		case WMI_REG_INDOOR_AP: return NL80211_REG_AP_LPI;
		case WMI_REG_STD_POWER_AP: return NL80211_REG_AP_SP;
		case WMI_REG_VLP_AP: return NL80211_REG_AP_VLP;
		default: return NL80211_REG_NUM_POWER_MODES;
		}
	} else if (client_type == WMI_REG_DEFAULT_CLIENT) {
		switch (ap_mode) {
		case WMI_REG_INDOOR_AP: return NL80211_REG_REGULAR_CLIENT_LPI;
		case WMI_REG_STD_POWER_AP: return NL80211_REG_REGULAR_CLIENT_SP;
		case WMI_REG_VLP_AP: return NL80211_REG_REGULAR_CLIENT_VLP;
		default: return NL80211_REG_NUM_POWER_MODES;
		}
	} else if (client_type == WMI_REG_SUBORDINATE_CLIENT) {
		switch (ap_mode) {
		case WMI_REG_INDOOR_AP: return NL80211_REG_SUBORDINATE_CLIENT_LPI;
		case WMI_REG_STD_POWER_AP: return NL80211_REG_SUBORDINATE_CLIENT_SP;
		case WMI_REG_VLP_AP: return NL80211_REG_SUBORDINATE_CLIENT_VLP;
		default: return NL80211_REG_NUM_POWER_MODES;
		}
	}

	return NL80211_REG_NUM_POWER_MODES;
}

/**
 * ath12k_fill_chan_eirp_list - Populate EIRP list from 6 GHz channel data
 * @chan_6g: Pointer to the 6 GHz channel structure containing channel info
 * @chan_eirp_list: Output array to be filled with EIRP values per channel
 *
 * This helper function iterates over the list of 6 GHz channels and fills
 * the corresponding entries in the provided channel_power array with:
 * - tx_power: maximum regulatory transmit power
 * - center_freq: center frequency of the channel
 * - chan_num: hardware channel number
 *
 * This function is used to extract regulatory power information for each
 * channel in the specified power mode band.
 */
static void
ath12k_fill_chan_eirp_list(struct ieee80211_6ghz_channel *chan_6g,
			   struct channel_power *chan_eirp_list)
{
	u8 i;

	for (i = 0; i < chan_6g->n_channels; i++) {
		struct ieee80211_channel *channels = &chan_6g->channels[i];

		if (channels->flags & IEEE80211_CHAN_DISABLED)
			continue;

		chan_eirp_list[i].tx_power = channels->max_reg_power;
		chan_eirp_list[i].center_freq = channels->center_freq;
		chan_eirp_list[i].chan_num = channels->hw_value;
	}
}

int ath12k_mac_reg_get_max_reg_eirp_from_chan_list(struct ath12k *ar,
						   enum wmi_reg_6g_ap_type ap_6ghz_pwr_mode,
						   enum wmi_reg_6g_client_type client_type,
						   bool is_client_needed,
						   struct channel_power *chan_eirp_list)
{
	enum nl80211_regulatory_power_modes nl_ap_pwr_mode;
	struct ieee80211_6ghz_channel *chan_6g;
	struct ieee80211_supported_band *band;

	band = &ar->mac.sbands[NL80211_BAND_6GHZ];
	nl_ap_pwr_mode = ath12k_get_nl_ap_pwr_mode(ap_6ghz_pwr_mode, client_type,
						   is_client_needed);
	if (nl_ap_pwr_mode == NL80211_REG_NUM_POWER_MODES)
		return -EINVAL;

	chan_6g = band->chan_6g[nl_ap_pwr_mode];
	if (!chan_6g)
		return -EINVAL;

	ath12k_fill_chan_eirp_list(chan_6g, chan_eirp_list);

	return 0;
}

void ath12k_mac_add_bridge_vdevs_iter(void *data, u8 *mac,
				      struct ieee80211_vif *vif)
{
	struct ath12k_bridge_iter *bridge_iter = data;
	struct ath12k_hw *ah = bridge_iter->ah;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	u8 active_num_devices = bridge_iter->active_num_devices;
	u8 link_id;

	if (!vif->valid_links)
		return;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL, "Cannot re-add B.Vdev other than AP interfaces\n");
		return;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	link_id = ffs(vif->valid_links) - 1;

	rcu_read_lock();
	arvif = rcu_dereference(ahvif->link[link_id]);
	rcu_read_unlock();

	ath12k_mac_create_and_start_bridge(ah->hw, vif, vif->link_conf[link_id],
					   NULL, active_num_devices);
	ath12k_mac_bridge_vdevs_up(arvif);
	ath12k_info(NULL, "Bypass: Bridge vdevs re-added for MLD %pM\n", vif->addr);
}

void ath12k_mac_remove_bridge_vdevs_iter(void *data, u8 *mac,
					 struct ieee80211_vif *vif)
{
	struct ath12k_hw *ah = data;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	u8 link_id = ATH12K_BRIDGE_LINK_MIN;
	unsigned long links;
	int ret;

	if (!vif->valid_links)
		return;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL, "Cannot delete B.Vdev other than AP interface\n");
		return;
	}
	ahvif = ath12k_vif_to_ahvif(vif);

	links = ahvif->links_map;
	for_each_set_bit_from(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		rcu_read_lock();
		arvif = rcu_dereference(ahvif->link[link_id]);
		rcu_read_unlock();
		if (!arvif) {
			ath12k_err(NULL,
				   "unable to determine the assigned link vif on link id %d\n",
				   link_id);
			continue;
		}
		ret = ath12k_wmi_vdev_down(arvif->ar, arvif->vdev_id);
		if (ret) {
			ath12k_warn(arvif->ar->ab, "failed to down vdev_id %i: %d\n",
				    arvif->vdev_id, ret);
			continue;
		}
		arvif->is_up = false;
		ath12k_mac_unassign_vif_chanctx_handle(ah->hw, vif, NULL, NULL, link_id);
		ath12k_mac_remove_link_interface(ah->hw, arvif);
		ath12k_mac_unassign_link_vif(arvif);
	}
	ath12k_info(NULL, "Bypass: Bridge vdevs removed for MLD %pM\n", vif->addr);
}

void ath12k_mac_wsi_remap_peer_cleanup(struct ath12k_base *ab,
				       bool skip_legacy)
{
	struct ath12k_dp_link_peer *link_peer, *tmp;
	struct ath12k_sta *ahsta;
	struct ieee80211_sta *sta;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "Bypass: Starting peer cleanup\n");
	spin_lock_bh(&dp->dp_lock);
	list_for_each_entry_safe(link_peer, tmp, &dp->peers, list) {
		sta = link_peer->sta;
		if (!sta)
			continue;

		if (skip_legacy && !sta->mlo)
			continue;

		ahsta = (struct ath12k_sta *)sta->drv_priv;

		ath12k_mac_peer_disassoc(ab, sta, ahsta,
					 ATH12K_DBG_WSI_BYPASS);
	}
	spin_unlock_bh(&dp->dp_lock);
}

int ath12k_mac_dynamic_wsi_remap(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar = ab->pdevs[0].ar;
	struct ath12k_hw *ah = ar ? ar->ah : NULL;
	struct ath12k_base *partner_ab;
	struct ath12k_pdev *pdev;
	struct wiphy *wiphy;
	long time_left;
	u32 num_ml_peers;
	int idx, ret = 0;
	bool skip_legacy;
	u8 active_num_devices;

	if (!ah) {
		ath12k_err(ab, "Failed to find hw, Dynamic remap failed\n");
		return -EINVAL;
	}
	wiphy = ah->hw->wiphy;

	ag->wsi_remap_in_progress = true;
	active_num_devices = ag->num_devices - ag->num_bypassed;
	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "active num_devices before proceeding bypass %d\n",
		   active_num_devices);

	/* Cleanup ML peers before MLO teardown during bypass
	 * operation. Ensure to cleanup the legacy clients for the device
	 * which is bypassed.
	 */
	wiphy_lock(wiphy);
	for (idx = 0; idx < ag->num_devices; idx++) {
		partner_ab = ag->ab[idx];

		if (partner_ab->is_bypassed)
			continue;

		if (ab == partner_ab)
			skip_legacy = false;
		else
			skip_legacy = true;

		ath12k_mac_wsi_remap_peer_cleanup(partner_ab, skip_legacy);
	}
	num_ml_peers = ah->num_ml_peers;
	wiphy_unlock(wiphy);

	/* Start a wait timer to ensure all ML peers are cleaned up
	 * before proceeding with the UMAC reset. This is necessary to
	 * avoid disrupting inter-device communication in the firmware.
	 * Skipping this may lead to peer delete timeouts on the host,
	 * followed by a firmware assert.
	 */
	if (num_ml_peers) {
		reinit_completion(&ag->peer_cleanup_complete);
		time_left = wait_for_completion_timeout(&ag->peer_cleanup_complete,
				msecs_to_jiffies(ag->wsi_peer_clean_timeout));

		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
			   "Bypass: Waiting for ML peer cleanup\n");
		if (!time_left) {
			ath12k_err(ab, "peer cleanup didn't get completed within %lld ms, pending peers %d\n",
				   ag->wsi_peer_clean_timeout, ah->num_ml_peers);
			return -ETIMEDOUT;
		}
	}

	/* Cleanup all the ar workqueues, make it to complete/default
	 * before bypassing an device.
	 */
	if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_REMOVE_DEVICE) {
		for (idx = 0; idx < ab->num_radios; idx++) {
			pdev = &ab->pdevs[idx];
			ar = pdev->ar;

			if (!ar)
				continue;
			if (ar->scan.state == ATH12K_SCAN_RUNNING) {
				ath12k_scan_abort(ar);
				ar->scan.arvif = NULL;
			}
			ath12k_core_radio_cleanup(ar);
		}
	}

	/* Remove Bridge vdevs during wsi remove if exist
	 */
	if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_REMOVE_DEVICE &&
	    active_num_devices == ATH12K_MIN_NUM_DEVICES_NLINK) {
		ieee80211_iterate_interfaces(ah->hw, IEEE80211_IFACE_ITER_NORMAL,
					     ath12k_mac_remove_bridge_vdevs_iter,
					     ah);
	}

	ret = ath12k_core_dynamic_wsi_remap(ab);

	return ret;
}

static struct ath12k *ath12k_mac_get_ar_by_center_freq(struct ieee80211_hw *hw,
						       u16 center_freq)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	int i;

	for_each_ar(ah, ar, i) {
		if (center_freq >= ar->chan_info.low_freq &&
		    center_freq <= ar->chan_info.high_freq)
			return ar;
	}

	return NULL;
}

int ath12k_mac_op_get_afc_eirp_pwr(struct ieee80211_hw *hw,
				   u32 freq,
				   u32 *eirp_pwr)
{
	struct ath12k *ar;

	ar = ath12k_mac_get_ar_by_center_freq(hw, freq);
	if (ar && ar->afc.is_6ghz_afc_power_event_received)
		*eirp_pwr = ath12k_mac_get_afc_eirp_power(ar, freq, freq, 20);
	else
		*eirp_pwr = ATH12K_MAX_TX_POWER;

	return 0;
}
EXPORT_SYMBOL(ath12k_mac_op_get_afc_eirp_pwr);

enum nl80211_6ghz_dev_deployment_type
ath12k_mac_op_get_6ghz_dev_deployment_type(struct ieee80211_hw *hw)
{
	enum nl80211_6ghz_dev_deployment_type dep_type =
		NL80211_6GHZ_DEV_DEPLOYMENT_TYPE_UNKNOWN;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_base *ab;
	struct ath12k *ar = NULL;
	int i;

	for_each_ar(ah, ar, i) {
		if (ar->supports_6ghz)
			break;
	}

	if (!ar) {
		ath12k_err(NULL, "ar is NULL\n");
		return dep_type;
	}

	ab = ar->ab;
	switch (ab->afc_dev_deployment) {
	case ATH12K_AFC_DEPLOYMENT_INDOOR:
		dep_type = NL80211_6GHZ_DEV_DEPLOYMENT_TYPE_INDOOR;
		break;
	case ATH12K_AFC_DEPLOYMENT_OUTDOOR:
		dep_type = NL80211_6GHZ_DEV_DEPLOYMENT_TYPE_OUTDOOR;
		break;
	case ATH12K_AFC_DEPLOYMENT_UNKNOWN:
	default:
		dep_type = NL80211_6GHZ_DEV_DEPLOYMENT_TYPE_UNKNOWN;
		break;
	}

	return dep_type;
}
EXPORT_SYMBOL(ath12k_mac_op_get_6ghz_dev_deployment_type);

void ath12k_mac_set_cw_intf_detect(struct ath12k *ar, u8 intf_detect_param)
{
	u8 cw_intf, dcs_enable_bitmap;

	/* TODO: Since for now only CW Interference is supported on the set path,
	 *  later when there is support added for other interference types,
	 *  the driver would receive intf_detect_bitmap
	 */
	spin_lock_bh(&ar->data_lock);
	cw_intf = ar->dcs_enable_bitmap & WMI_DCS_CW_INTF;
	if ((~cw_intf & intf_detect_param) |
	    (cw_intf & ~intf_detect_param)) {
		ar->dcs_enable_bitmap &= ~WMI_DCS_CW_INTF;
		ar->dcs_enable_bitmap |= intf_detect_param;
		dcs_enable_bitmap = ar->dcs_enable_bitmap;
		spin_unlock_bh(&ar->data_lock);
		ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DCS,
					  dcs_enable_bitmap,
					  ar->pdev->pdev_id);
		return;
	}
	spin_unlock_bh(&ar->data_lock);
}
