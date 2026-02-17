/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ATH12K_DEBUGFS_H_
#define _ATH12K_DEBUGFS_H_

#include "core.h"

#define ATH12K_UDP_TCP_START_PORT		0
#define ATH12K_UDP_TCP_END_PORT			65535
#define ATH12K_RX_FSE_FLOW_MATCH_DEBUGFS	0xBBBB

#define ATH12K_BUF_SIZE_32 32

#define ATH12K_HTT_PEER_STATS_RESET BIT(16)

#define ATH12K_HTT_STATS_BUF_SIZE (1024 * 512)

#define ATH12K_QOS_MAP_LEN_MIN 16

enum ath12k_dbg_aggr_mode {
	ATH12K_DBG_AGGR_MODE_AUTO,
	ATH12K_DBG_AGGR_MODE_MANUAL,
	ATH12K_DBG_AGGR_MODE_MAX,
};

void ath12k_wmi_crl_path_stats_list_free(struct ath12k *ar, struct list_head *head);
int ath12k_wsi_bypass_precheck(struct ath12k_base *ab, unsigned int value);

u32 ath12k_dbg_dump_qos_profile(struct ath12k_base *ab,
				char *buf, u8 qos_id, u32 size);

#define ATH12K_MAX_NRPS 7
#define MAC_UNIT_LEN 3

struct ath12k_neighbor_peer {
	struct list_head list;
	int vdev_id;
	u8 addr[ETH_ALEN];
	u8 rssi;
	s64 timestamp;
	int pdev_id;
};

#ifdef CPTCFG_ATH12K_DEBUGFS
void ath12k_debugfs_soc_create(struct ath12k_base *ab);
void ath12k_debugfs_soc_destroy(struct ath12k_base *ab);
void ath12k_debugfs_register(struct ath12k *ar);
void ath12k_debugfs_unregister(struct ath12k *ar);
void ath12k_hw_debugfs_register(struct ath12k_hw *ah);
void ath12k_debugfs_pdev_destroy(struct ath12k_base *ab);
void ath12k_debugfs_fw_stats_init(struct ath12k *ar);
void ath12k_send_fw_hang_cmd(struct ath12k_base *ab,
			     unsigned int value);

static inline bool ath12k_debugfs_is_pktlog_peer_valid(struct ath12k *ar, u8 *addr)
{
        return (ar->debug.pktlog_peer_valid && ar->debug.pktlog_mode &&
                ether_addr_equal(addr, ar->debug.pktlog_peer_addr));
}

static inline int ath12k_extd_tx_stats_enabled(struct ath12k *ar)
{
	return ((ar->dp.dp_stats_mask &  DP_ENABLE_STATS) &&
		(ar->dp.dp_stats_mask & DP_ENABLE_EXT_TX_STATS));
}

static inline bool ath12k_extd_rx_stats_enabled(struct ath12k *ar)
{
	return ((ar->dp.dp_stats_mask &  DP_ENABLE_STATS) &&
		(ar->dp.dp_stats_mask & DP_ENABLE_EXT_RX_STATS));
}

static inline int ath12k_debugfs_rx_filter(struct ath12k *ar)
{
	return ar->debug.rx_filter;
}

static inline bool
ath12k_dp_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_STATS);
}

static inline bool
ath12k_dp_debug_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_DEBUG_STATS);
}

static inline u8 ath12k_debugfs_is_qos_stats_enabled(struct ath12k *ar)
{
	return (ar->debug.qos_stats & ATH12K_QOS_STATS_CATEG_MASK);
}

static inline bool ath12k_tid_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return (dp_pdev->dp_stats_mask & DP_ENABLE_TID_STATS);
}

void ath12k_tid_tx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_tid_tx_drop_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_tid_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_tid_drop_rx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason);
void ath12k_debugfs_nrp_clean(struct ath12k *ar, const u8 *addr);
void ath12k_debugfs_nrp_cleanup_all(struct ath12k *ar);

void ath12k_debugfs_op_vif_add(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif);
void ath12k_debugfs_pdev_create(struct ath12k_base *ab);

void ath12k_debugfs_add_interface(struct ath12k_link_vif *arvif);
void ath12k_debugfs_remove_interface(struct ath12k_link_vif *arvif);
struct dentry *ath12k_debugfs_erp_create(void);

#define ATH12K_CCK_RATES			4
#define ATH12K_OFDM_RATES			8
#define ATH12K_HT_RATES				8
#define ATH12K_VHT_RATES			12
#define ATH12K_HE_RATES				12
#define ATH12K_HE_RATES_WITH_EXTRA_MCS		14
#define ATH12K_EHT_RATES			16
#define HE_EXTRA_MCS_SUPPORT			GENMASK(31, 16)
#define ATH12K_NSS_1				1
#define ATH12K_NSS_4				4
#define ATH12K_NSS_8				8
#define ATH12K_HW_NSS(_rcode)			(((_rcode) >> 5) & 0x7)
#define TPC_STATS_WAIT_TIME			(1 * HZ)
#define MAX_TPC_PREAM_STR_LEN			7
#define TPC_INVAL				-128
#define TPC_MAX					127
#define TPC_STATS_WAIT_TIME			(1 * HZ)
#define TPC_STATS_TOT_ROW			700
#define TPC_STATS_TOT_COLUMN			100
#define MODULATION_LIMIT			126

#define ATH12K_TPC_STATS_BUF_SIZE	(TPC_STATS_TOT_ROW * TPC_STATS_TOT_COLUMN)

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

#define BTCOEX_ENABLE                    1
#define BTCOEX_DISABLE                   0
#define BTCOEX_CONFIGURE_DEFAULT        -1
#define BTCOEX_THREE_WIRE_MODE           1
#define BTCOEX_PTA_MODE                  2
#define BTCOEX_MAX_PKT_WEIGHT            255
#define BTCOEX_MAX_WLAN_PRIORITY    ((QCA_WLAN_PRIORITY_MGMT << 1) - 1)

enum wmi_tpc_pream_bw {
	WMI_TPC_PREAM_CCK,
	WMI_TPC_PREAM_OFDM,
	WMI_TPC_PREAM_HT20,
	WMI_TPC_PREAM_HT40,
	WMI_TPC_PREAM_VHT20,
	WMI_TPC_PREAM_VHT40,
	WMI_TPC_PREAM_VHT80,
	WMI_TPC_PREAM_VHT160,
	WMI_TPC_PREAM_HE20,
	WMI_TPC_PREAM_HE40,
	WMI_TPC_PREAM_HE80,
	WMI_TPC_PREAM_HE160,
	WMI_TPC_PREAM_EHT20,
	WMI_TPC_PREAM_EHT40,
	WMI_TPC_PREAM_EHT60,
	WMI_TPC_PREAM_EHT80,
	WMI_TPC_PREAM_EHT120,
	WMI_TPC_PREAM_EHT140,
	WMI_TPC_PREAM_EHT160,
	WMI_TPC_PREAM_EHT200,
	WMI_TPC_PREAM_EHT240,
	WMI_TPC_PREAM_EHT280,
	WMI_TPC_PREAM_EHT320,
	WMI_TPC_PREAM_MAX
};

enum ath12k_debug_tpc_stats_ctl_mode {
	ATH12K_TPC_STATS_CTL_MODE_LEGACY_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT_VHT20_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT20_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT_VHT40_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT40_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_VHT80_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT80_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_VHT160_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT160_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HE_EHT320_5GHZ_6GHZ,
	ATH12K_TPC_STATS_CTL_MODE_CCK_2GHZ,
	ATH12K_TPC_STATS_CTL_MODE_LEGACY_2GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT20_2GHZ,
	ATH12K_TPC_STATS_CTL_MODE_HT40_2GHZ,

	ATH12K_TPC_STATS_CTL_MODE_EHT80_SU_PUNC20 = 23,
	ATH12K_TPC_STATS_CTL_MODE_EHT160_SU_PUNC20,
	ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC40,
	ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC80,
	ATH12K_TPC_STATS_CTL_MODE_EHT320_SU_PUNC120
};

enum ath12k_debug_tpc_stats_support_modes {
	ATH12K_TPC_STATS_SUPPORT_160 = 0,
	ATH12K_TPC_STATS_SUPPORT_320,
	ATH12K_TPC_STATS_SUPPORT_AX,
	ATH12K_TPC_STATS_SUPPORT_AX_EXTRA_MCS,
	ATH12K_TPC_STATS_SUPPORT_BE,
	ATH12K_TPC_STATS_SUPPORT_BE_PUNC,
};

struct ath12k_pktlog_hdr {
        u16 flags;
        u16 missed_cnt;
        u16 log_type;
        u16 size;
        u32 timestamp;
        u32 type_specific_data;
        struct mlo_timestamp m_timestamp;
        u8 payload[];
} __packed;
#else

static inline void ath12k_debugfs_pdev_destroy(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_fw_stats_init(struct ath12k *ar)
{
}

static inline bool ath12k_debugfs_is_pktlog_peer_valid(struct ath12k *ar, u8 *addr)
{
	return false;
}

static inline void ath12k_debugfs_soc_create(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_soc_destroy(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_register(struct ath12k *ar)
{
}

static inline void ath12k_debugfs_unregister(struct ath12k *ar)
{
}

static inline void ath12k_hw_debugfs_register(struct ath12k_hw *ah)
{
}

static inline int ath12k_extd_tx_stats_enabled(struct ath12k *ar)
{
	return 0;
}

static inline bool ath12k_extd_rx_stats_enabled(struct ath12k *ar)
{
	return false;
}

static inline int ath12k_debugfs_rx_filter(struct ath12k *ar)
{
	return 0;
}

static inline void ath12k_debugfs_set_rx_filter(struct ath12k *ar, u32 rx_filter)
{
}

static inline void ath12k_debugfs_op_vif_add(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif)
{
}

static inline void ath12k_debugfs_pdev_create(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_add_interface(struct ath12k_link_vif *arvif)
{
}

static inline void ath12k_debugfs_remove_interface(struct ath12k_link_vif *arvif)
{
}

static inline bool
ath12k_dp_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool
ath12k_dp_debug_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline bool
ath12k_tid_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	return false;
}

static inline u8 ath12k_debugfs_is_qos_stats_enabled(struct ath12k *ar)
{
	return 0;
}

static inline void ath12k_debugfs_nrp_clean(struct ath12k *ar, const u8 *addr)
{
}

static inline void ath12k_debugfs_nrp_cleanup_all(struct ath12k *ar)
{
}

struct dentry *ath12k_debugfs_erp_create(void)
{
       return NULL;
}

#endif /* CPTCFG_ATH12K_DEBUGFS */

void ath12k_init_pktlog(struct ath12k *ar);
void ath12k_deinit_pktlog(struct ath12k *ar);
void ath12k_htt_pktlog_process(struct ath12k *ar, u8 *data);
void ath12k_htt_ppdu_pktlog_process(struct ath12k *ar, u8 *data, u32 len);
void ath12k_dp_rx_stats_buf_pktlog_process(struct ath12k *ar, u8 *data,
					   u16 log_type, u32 len);
#endif /* _ATH12K_DEBUGFS_H_ */
