/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_MAC_H
#define ATH12K_MAC_H

#include <net/mac80211.h>
#include <net/cfg80211.h>
#include "wmi.h"

#include "../../net/mac80211/qcn_extns/cmn_extn.h"

extern unsigned int ath12k_ppe_ds_enabled;
struct ath12k;
struct ath12k_base;
struct ath12k_hw;
struct ath12k_hw_group;
struct ath12k_pdev_map;
struct ath12k_vif;
struct ath12k_link_sta;
enum ath12k_mlo_recovery_mode;

struct ath12k_generic_iter {
	struct ath12k *ar;
	int ret;
};

/* number of failed packets (20 packets with 16 sw reties each) */
#define ATH12K_KICKOUT_THRESHOLD		(20 * 16)

/* Use insanely high numbers to make sure that the firmware implementation
 * won't start, we have the same functionality already in hostapd. Unit
 * is seconds.
 */
#define ATH12K_KEEPALIVE_MIN_IDLE		3747
#define ATH12K_KEEPALIVE_MAX_IDLE		3895
#define ATH12K_KEEPALIVE_MAX_UNRESPONSIVE	3900

#define ATH12K_PDEV_TX_POWER_INVALID		((u32)-1)
#define ATH12K_PDEV_TX_POWER_REFRESH_TIME_MSECS	5000 /* msecs */

/* FIXME: should these be in ieee80211.h? */
#define IEEE80211_VHT_MCS_SUPPORT_0_11_MASK	GENMASK(23, 16)
#define IEEE80211_DISABLE_VHT_MCS_SUPPORT_0_11	BIT(24)

#define ATH12K_CHAN_WIDTH_NUM			14
#define ATH12K_BW_NSS_MAP_ENABLE		BIT(31)
#define ATH12K_PEER_RX_NSS_160MHZ		GENMASK(2, 0)

#define ATH12K_OBSS_PD_MAX_THRESHOLD			-82
#define ATH12K_OBSS_PD_THRESHOLD_DISABLED		-128
#define ATH12K_OBSS_PD_THRESHOLD_IN_DBM			BIT(29)
#define ATH12K_OBSS_PD_SRG_EN				BIT(30)
#define ATH12K_OBSS_PD_NON_SRG_EN			BIT(31)

#define ATH12K_TX_POWER_MAX_VAL	70
#define ATH12K_TX_POWER_MIN_VAL	0

#define ATH12K_DEFAULT_LINK_ID	0
#define ATH12K_INVALID_LINK_ID	255

/* Default link after the IEEE802.11 defined Max link id limit
 * for driver usage purpose.
 */
#define ATH12K_DEFAULT_SCAN_LINK	IEEE80211_MLD_MAX_NUM_LINKS
#define ATH12K_LAST_SCAN_LINK		(IEEE80211_MLD_MAX_NUM_LINKS + \
					ATH12K_GROUP_MAX_RADIO - 1)
#define ATH12K_MAX_NUM_BRIDGE_LINKS	2
#define ATH12K_BRIDGE_LINK_MIN		(ATH12K_LAST_SCAN_LINK + 1)
#define ATH12K_BRIDGE_LINK_MAX		(ATH12K_LAST_SCAN_LINK + \
					ATH12K_MAX_NUM_BRIDGE_LINKS)
/* Define 1 scan link for each radio for parallel scan purposes */
#define ATH12K_NUM_MAX_LINKS		(IEEE80211_MLD_MAX_NUM_LINKS + \
					ATH12K_GROUP_MAX_RADIO + \
					ATH12K_MAX_NUM_BRIDGE_LINKS)
#define ATH12K_BRIDGE_LINKS_MASK	GENMASK(ATH12K_BRIDGE_LINK_MAX, \
					ATH12K_BRIDGE_LINK_MIN)
#define ATH12K_IEEE80211_MLD_MAX_LINKS_MASK	\
					GENMASK(IEEE80211_MLD_MAX_NUM_LINKS - \
					1, 0)
#define ATH12K_MAX_STA_LINKS	3
#define ATH12K_SCAN_LINKS_MASK	GENMASK(ATH12K_LAST_SCAN_LINK, IEEE80211_MLD_MAX_NUM_LINKS)
#define ATH12K_PDEV_SIGNAL_UPDATE_TIME_MSECS	2000

#define HECAP_PHY_SUBFMR_GET(hecap_phy) \
	u8_get_bits(hecap_phy[3], IEEE80211_HE_PHY_CAP3_SU_BEAMFORMER)

#define HECAP_PHY_SUBFME_GET(hecap_phy) \
	u8_get_bits(hecap_phy[4], IEEE80211_HE_PHY_CAP4_SU_BEAMFORMEE)

#define HECAP_PHY_MUBFMR_GET(hecap_phy) \
	u8_get_bits(hecap_phy[4], IEEE80211_HE_PHY_CAP4_MU_BEAMFORMER)

#define HECAP_PHY_ULMUMIMO_GET(hecap_phy) \
	u8_get_bits(hecap_phy[2], IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO)

#define HECAP_PHY_ULOFDMA_GET(hecap_phy) \
	u8_get_bits(hecap_phy[2], IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO)

#define ATH12K_MIN_TX_POWER		-127
#define ATH12K_PDEV_SUSPEND_TIMEOUT	(2 * HZ)
#define ATH12K_PDEV_RESUME_TIMEOUT	(2 * HZ)
#define ATH12K_ERP_BRIDGE_VDEV_REMOVAL_THRESHOLD	2

enum ath12k_supported_bw {
	ATH12K_BW_20    = 0,
	ATH12K_BW_40    = 1,
	ATH12K_BW_80    = 2,
	ATH12K_BW_160   = 3,
	ATH12K_BW_320   = 4,
};

/* Below BW_GAIN should be added to the SNR value of every ppdu based on the
 * bandwidth. This table is obtained from HALPHY.
 * BW         BW_Gain
 * 20          0
 * 40          3dBm
 * 80          6dBm
 * 160/80P80   9dBm
 * 320         12dBm
 */

#define ATH12K_BW_GAIN_20MHZ 0
#define ATH12K_BW_GAIN_40MHZ 3
#define ATH12K_BW_GAIN_80MHZ 6
#define ATH12K_BW_GAIN_160MHZ 9
#define ATH12K_BW_GAIN_320MHZ 12

#define ATH12K_CHWIDTH_20		20  /* Channel width 20 */
#define ATH12K_CHWIDTH_40		40  /* Channel width 40 */
#define ATH12K_CHWIDTH_80		80  /* Channel width 80 */
#define ATH12K_CHWIDTH_160		160 /* Channel width 160 */
#define ATH12K_CHWIDTH_320		320 /* Channel width 320 */

#define ATH12K_MAX_EIRP_VALS		5
#define ATH12K_MAX_TX_POWER		127
#define ATH12K_EIRP_PWR_SCALE		100
#define ATH12K_SP_AP_AND_CLIENT_POWER_DIFF_IN_DBM 6

#define ATH12K_NUM_20_MHZ_CHAN_IN_320_MHZ_CHAN	16

/**
 * struct ath12k_mac_num_chanctxs_arg - Structure to hold channel context
 * @ar: Pointer to ath12k device context
 * @num: Number of channel context for the applicable ar
 */
struct ath12k_mac_num_chanctxs_arg {
	struct ath12k *ar;
	u8 num;
};

struct ath12k_mac_get_any_chanctx_conf_arg {
	struct ath12k *ar;
	struct ieee80211_chanctx_conf *chanctx_conf;
};

/* ath12k only deals with 320 MHz, so 16 subchannels */
#define ATH12K_NUM_PWR_LEVELS  16

#define ATH12K_WLAN_PRIO_MAX    0x63
#define ATH12K_WLAN_PRIO_WEIGHT 0xff

#define ATH12K_SCAN_11D_INTERVAL               600000
#define ATH12K_11D_INVALID_VDEV_ID             0xFFFF
#define MAX_NUM_BRIDGE_VDEV_PER_MLD  2

enum ath12k_background_dfs_events {
	ATH12K_BGDFS_SUCCESS,
	ATH12K_BGDFS_ABORT,
	ATH12K_BGDFS_RADAR,
};

struct ath12k_mac_link_migrate_usr_params {
	u8 link_id;
	u8 addr[ETH_ALEN];
};

struct ath12k_mac_pri_link_migr_peer_node {
	struct list_head list;
	u16 ml_peer_id;
	u8 hw_link_id;
};

void ath12k_mac_11d_scan_start(struct ath12k *ar, u32 vdev_id);
void ath12k_mac_11d_scan_stop(struct ath12k *ar);
void ath12k_mac_11d_scan_stop_all(struct ath12k_base *ab);

void ath12k_mac_set_cw_intf_detect(struct ath12k *ar, u8 intf_bitmap);
void ath12k_mac_set_vendor_intf_detect(struct ath12k *ar, u8 intf_detect_bitmap);
void ath12k_mac_destroy(struct ath12k_hw_group *ag);
void ath12k_mac_unregister(struct ath12k_hw_group *ag);
int ath12k_mac_register(struct ath12k_hw_group *ag);
int ath12k_mac_allocate(struct ath12k_hw_group *ag);
int ath12k_mac_hw_ratecode_to_legacy_rate(u8 hw_rc, u8 preamble, u8 *rateidx,
					  u16 *rate);
u8 ath12k_mac_bitrate_to_idx(const struct ieee80211_supported_band *sband,
			     u32 bitrate);
u8 ath12k_mac_hw_rate_to_idx(const struct ieee80211_supported_band *sband,
			     u8 hw_rate, bool cck);

void __ath12k_mac_scan_finish(struct ath12k *ar);
void ath12k_mac_scan_finish(struct ath12k *ar);

struct ath12k_link_vif *ath12k_mac_get_arvif(struct ath12k *ar, u32 vdev_id);
struct ath12k_link_vif *ath12k_mac_get_arvif_by_vdev_id(struct ath12k_base *ab,
							u32 vdev_id);
int ath12k_mac_btcoex_config(struct ath12k *ar, struct ath12k_link_vif *arvif,
			   int coex, u32 wlan_prio_mask, u8 wlan_weight);
struct ath12k *ath12k_mac_get_ar_by_vdev_id(struct ath12k_base *ab, u32 vdev_id);
struct ath12k *ath12k_mac_get_ar_by_pdev_id(struct ath12k_base *ab, u32 pdev_id);

void ath12k_mac_fill_reg_tpc_info(struct ath12k *ar,
				  struct ath12k_link_vif *arvif,
                                  struct ieee80211_chanctx_conf *ctx);
void ath12k_mac_fill_reg_tpc_info_with_eirp_power(struct ath12k *ar,
						  struct ath12k_link_vif *arvif,
						  struct ieee80211_chanctx_conf *ctx);

/**
 * ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_sp - Populate both PSD and
 * EIRP power info for SP AP mode
 * @ar: Pointer to ath12k device context
 * @arvif: Virtual interface context
 * @ctx: Channel context configuration
 *
 * Invokes both PSD and EIRP power configuration routines to populate
 * transmit power control (TPC) information for 6 GHz Standard Power (SP)
 * Access Point (AP) mode. This ensures that both power spectral density
 * and equivalent isotropically radiated power levels are configured
 * according to regulatory and hardware constraints.
 */
void
ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_sp(struct ath12k *ar,
						      struct ath12k_link_vif *arvif,
						      struct ieee80211_chanctx_conf *ctx);

/**
 * ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_client_sp - Fill PSD and
 * EIRP-based reg TPC for client SP
 * @ar: Pointer to ath12k device structure
 * @arvif: Pointer to ath12k virtual interface structure
 * @chanctx: Pointer to channel context configuration
 *
 * Fills reg_tpc_info with both PSD and EIRP-based TPC values for STA in SP mode (6 GHz).
 * Internally calls:
 * - ath12k_mac_fill_reg_tpc_info_with_psd_for_client_sp_pwr_mode()
 * - ath12k_mac_fill_reg_tpc_info_with_eirp_for_client_sp_pwr_mode()
 *
 * Output is used to build WMI reg TPC TLV for AFC-compliant TXP control.
 */
void
ath12k_mac_fill_reg_tpc_info_with_psd_eirp_pwr_for_client_sp(struct ath12k *ar,
							     struct ath12k_link_vif *arvif,
							     struct ieee80211_chanctx_conf *ctx);

void ath12k_mac_drain_tx(struct ath12k *ar);
void ath12k_mac_peer_cleanup_all(struct ath12k *ar);
void ath12k_mac_dp_peer_cleanup(struct ath12k_hw *ah,
				enum ath12k_mlo_recovery_mode recovery_mode);
void ath12k_dcs_wlan_intf_cleanup(struct ath12k *ar);
int ath12k_mac_tx_mgmt_pending_free(int buf_id, void *skb, void *ctx);
enum rate_info_bw ath12k_mac_bw_to_mac80211_bw(enum ath12k_supported_bw bw);
enum ath12k_supported_bw ath12k_mac_mac80211_bw_to_ath12k_bw(enum rate_info_bw bw);
u8 ath12k_mac_get_bw_offset(enum ieee80211_sta_rx_bandwidth bandwidth);
enum hal_encrypt_type ath12k_dp_tx_get_encrypt_type(u32 cipher);
int ath12k_mac_rfkill_enable_radio(struct ath12k *ar, bool enable);
int ath12k_mac_rfkill_config(struct ath12k *ar);
int ath12k_mac_wait_tx_complete(struct ath12k *ar);
void ath12k_mac_handle_beacon(struct ath12k *ar, struct sk_buff *skb);
void ath12k_mac_handle_beacon_miss(struct ath12k *ar, u32 vdev_id);
int ath12k_mac_vif_set_keepalive(struct ath12k_link_vif *arvif,
				 enum wmi_sta_keepalive_method method,
				 u32 interval);
u8 ath12k_mac_get_target_pdev_id(struct ath12k *ar);
int ath12k_mac_mlo_setup(struct ath12k_hw_group *ag);
int ath12k_mac_mlo_ready(struct ath12k_hw_group *ag);
int ath12k_mac_vdev_stop(struct ath12k_link_vif *arvif);
void ath12k_mac_mlo_teardown(struct ath12k_hw_group *ag);
void ath12k_mac_get_any_chanctx_conf_iter(struct ieee80211_hw *hw,
					  struct ieee80211_chanctx_conf *conf,
					  void *data);
int ath12k_mac_mlo_teardown_with_umac_reset(struct ath12k_base *ab,
					    enum wmi_mlo_tear_down_reason_code_type reason_code);
int ath12k_mac_partner_peer_cleanup(struct ath12k_base *ab);
u16 ath12k_mac_he_convert_tones_to_ru_tones(u16 tones);
enum nl80211_eht_ru_alloc ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(u16 ru_tones);
enum nl80211_eht_gi ath12k_mac_eht_gi_to_nl80211_eht_gi(u8 sgi);
struct ieee80211_bss_conf *ath12k_mac_get_link_bss_conf(struct ath12k_link_vif *arvif);
struct ath12k *ath12k_get_ar_by_vif(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    u8 link_id);
int ath12k_mac_get_fw_stats(struct ath12k *ar, struct ath12k_fw_stats_req_params *param);
int ath12k_mac_get_fw_stats_per_vif(struct ath12k *ar,
				    struct ath12k_fw_stats_req_params *param);
int ath12k_mac_op_start(struct ieee80211_hw *hw);
void ath12k_mac_op_stop(struct ieee80211_hw *hw, bool suspend);
void
ath12k_mac_op_reconfig_complete(struct ieee80211_hw *hw,
				enum ieee80211_reconfig_type reconfig_type);
void
ath12k_mac_reconfig_complete(struct ieee80211_hw *hw,
			     enum ieee80211_reconfig_type reconfig_type);
int ath12k_mac_op_add_interface(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif);
void ath12k_mac_op_remove_interface(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif);
void ath12k_mac_op_update_vif_offload(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif);
int ath12k_mac_op_config(struct ieee80211_hw *hw, u32 changed);
void ath12k_mac_op_sta_set_4addr(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta, bool enabled);
void ath12k_mac_op_link_info_changed(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct ieee80211_bss_conf *info,
				     u64 changed);
void ath12k_mac_op_vif_cfg_changed(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   u64 changed);
int
ath12k_mac_op_change_vif_links(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       u16 old_links, u16 new_links,
			       struct ieee80211_bss_conf *ol[IEEE80211_MLD_MAX_NUM_LINKS]);
void ath12k_mac_op_configure_filter(struct ieee80211_hw *hw,
				    unsigned int changed_flags,
				    unsigned int *total_flags,
				    u64 multicast);
int ath12k_mac_op_hw_scan(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  struct ieee80211_scan_request *hw_req);
void ath12k_mac_op_cancel_hw_scan(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif);
int ath12k_mac_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			  struct ieee80211_vif *vif, struct ieee80211_sta *sta,
			  struct ieee80211_key_conf *key);
void ath12k_mac_op_set_rekey_data(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct cfg80211_gtk_rekey_data *data);
int ath12k_mac_op_sta_state(struct ieee80211_hw *hw,
			    struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta,
			    enum ieee80211_sta_state old_state,
			    enum ieee80211_sta_state new_state);
int ath12k_mac_op_sta_set_txpwr(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta);
void ath12k_mac_op_link_sta_rc_update(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      struct ieee80211_link_sta *link_sta,
				      u32 changed);
int ath12k_mac_op_conf_tx(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  unsigned int link_id, u16 ac,
			  const struct ieee80211_tx_queue_params *params);
int ath12k_mac_op_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant,
			      u8 radio_id, bool is_dynamic);
int ath12k_mac_op_get_antenna(struct ieee80211_hw *hw, u32 *tx_ant, u32 *rx_ant,
			      u8 radio_id);
int ath12k_mac_op_ampdu_action(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ieee80211_ampdu_params *params);
int ath12k_mac_op_add_chanctx(struct ieee80211_hw *hw,
			      struct ieee80211_chanctx_conf *ctx);
void ath12k_mac_op_remove_chanctx(struct ieee80211_hw *hw,
				  struct ieee80211_chanctx_conf *ctx);
void ath12k_mac_op_change_chanctx(struct ieee80211_hw *hw,
				  struct ieee80211_chanctx_conf *ctx,
				  u32 changed);
int
ath12k_mac_op_assign_vif_chanctx(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_bss_conf *link_conf,
				 struct ieee80211_chanctx_conf *ctx);
void
ath12k_mac_op_unassign_vif_chanctx(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_bss_conf *link_conf,
				   struct ieee80211_chanctx_conf *ctx);
int
ath12k_mac_op_switch_vif_chanctx(struct ieee80211_hw *hw,
				 struct ieee80211_vif_chanctx_switch *vifs,
				 int n_vifs,
				 enum ieee80211_chanctx_switch_mode mode);
int ath12k_mac_op_set_rts_threshold(struct ieee80211_hw *hw, u8 radio_id,
				    u32 value, struct ieee80211_vif *vif,
				    u32 link_id);
int ath12k_mac_op_set_frag_threshold(struct ieee80211_hw *hw, u32 value);
int
ath12k_mac_op_set_bitrate_mask(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif, unsigned int link_id,
			       const struct cfg80211_bitrate_mask *mask);
int ath12k_mac_op_get_survey(struct ieee80211_hw *hw, int idx,
			     struct survey_info *survey);
void ath12k_mac_op_flush(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			 u32 queues, bool drop);
void ath12k_mac_op_link_sta_statistics(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_link_sta *link_sta,
				       struct link_station_info *link_sinfo);
void ath12k_mac_op_sta_statistics(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct ieee80211_sta *sta,
				  struct station_info *sinfo);
int ath12k_mac_op_remain_on_channel(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct cfg80211_chan_def *chandef,
				    int duration,
				    enum ieee80211_roc_type type);
int ath12k_mac_op_cancel_remain_on_channel(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif);
int ath12k_mac_op_change_sta_links(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta,
				   u16 old_links, u16 new_links);
bool ath12k_mac_op_can_activate_links(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      u16 active_links);
int ath12k_mac_op_get_txpower(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      unsigned int link_id,
			      int *dbm);
int ath12k_mac_op_link_reconfig_remove(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       const struct cfg80211_link_reconfig_removal_params *params);
bool ath12k_mac_op_removed_link_is_primary(struct ieee80211_sta *sta, u16 removed_links);
int ath12k_mac_op_erp(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		      int link_id, struct cfg80211_erp_params *params);
int ath12k_mac_mgmt_tx(struct ath12k *ar, struct sk_buff *skb,
		       bool is_prb_rsp);
void ath12k_mac_add_p2p_noa_ie(struct ath12k *ar,
			       struct ieee80211_vif *vif,
			       struct sk_buff *skb,
			       bool is_prb_rsp);
u8 ath12k_mac_get_tx_link(struct ieee80211_sta *sta, struct ieee80211_vif *vif,
			  u8 link, struct sk_buff *skb, u32 info_flags);

void ath12k_mlo_mcast_update_tx_link_address(struct ieee80211_vif *vif,
					     u8 link_id, struct sk_buff *skb,
					     u32 info_flags);
void ath12k_mac_get_any_chandef_iter(struct ieee80211_hw *hw,
				     struct ieee80211_chanctx_conf *conf,
				     void *data);
u16 ath12k_calculate_subchannel_count(enum nl80211_chan_width width);
void ath12k_mac_bcn_tx_event(struct ath12k_link_vif *arvif);
struct ieee80211_bss_conf *ath12k_mac_get_link_bss_conf(struct ath12k_link_vif *arvif);
bool ath12k_mac_is_ml_arvif(struct ath12k_link_vif *arvif);
bool ath12k_mac_is_bridge_vdev(struct ath12k_link_vif *arvif);
void ath12k_mac_bridge_vdev_up(struct ath12k_link_vif *arvif);
int
ath12k_mac_process_link_migrate_req(struct ath12k_vif *ahvif,
				    struct ath12k_mac_link_migrate_usr_params *params);
void ath12k_bss_assoc(struct ath12k *ar, struct ath12k_link_vif *arvif,
		      struct ieee80211_bss_conf *bss_conf);
void ath12k_bss_disassoc(struct ath12k *ar, struct ath12k_link_vif *arvif);
void ath12k_mac_vif_cache_flush(struct ath12k *ar, struct ath12k_link_vif *arvif);
int ath12k_mac_conf_tx(struct ath12k_link_vif *arvif, u16 ac,
		       const struct ieee80211_tx_queue_params *params);

int ath12k_mac_set_key(struct ath12k *ar, enum set_key_cmd cmd,
                              struct ath12k_link_vif *arvif,
                              struct ath12k_link_sta *arsta,
                              struct ieee80211_key_conf *key);
int ath12k_mac_vdev_create(struct ath12k *ar, struct ath12k_link_vif *arvif,
			   bool is_bridge_vdev);

int ath12k_mac_vdev_start(struct ath12k_link_vif *arvif,
			  struct ieee80211_chanctx_conf *ctx);
void ath12k_mac_parse_tx_pwr_env(struct ath12k *ar,
				 struct ath12k_link_vif *arvif);
int ath12k_mac_start(struct ath12k *ar);
int ath12k_mac_vif_link_chan(struct ieee80211_vif *vif, u8 link_id,
                                    struct cfg80211_chan_def *def);
void ath12k_mac_bss_info_changed(struct ath12k *ar,
                                struct ath12k_link_vif *arvif,
                                struct ieee80211_bss_conf *info,
                                u64 changed);
int ath12k_mac_monitor_start(struct ath12k *ar);
void ath12k_mac_op_set_dscp_tid(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct cfg80211_qos_map *qos_map,
				unsigned int link_id);
int ath12k_mac_mlo_standby_teardown(struct ath12k_hw *ah);
void ath12k_mac_stop(struct ath12k *ar);
bool ath12k_mac_validate_active_radio_count(struct ath12k_hw *ah);
int ath12k_mac_pdev_suspend(struct ath12k *ar);
int ath12k_mac_set_eht_txbf_conf(struct ath12k_link_vif *arvif);
int ath12k_mac_set_he_txbf_conf(struct ath12k_link_vif *arvif);
int ath12k_mac_pdev_resume(struct ath12k *ar);

/* In the bitmap 0 indicates no puncturing and 1 indicated that sub channel is
 * punctured
 */
#define ATH12K_PUNCTURE_INVALID		0xFFFF
#define ATH12K_PUNCTURE_NONE		0x0000
#define ATH12K_PUNCTURE_80MHZ_MASK	0x000F
#define ATH12K_PUNCTURE_160MHZ_MASK	0x00FF
#define ATH12K_PUNCTURE_320MHZ_MASK	0xFFFF
#define ATH12K_PUNCTURE_40MHZ_MASK	0x0003

/* Used to indicate an invalid edge in puncture mask calculations */
#define ATH12K_INVALID_EDGE	0x0FFF
/* Used to indicate an invalid dbr in puncture mask calculations */
#define ATH12K_INVALID_DBR	100
/* Used to indicate an invalid PSD in calculations */
#define ATH12K_INVALID_PSD	(-1270) /* -127 multiplied by 10 */

#define ATH12K_MAX_PUNC_MASK_LIMITS	3
#define ATH12K_CHAN_MAX_PSD_POWER	127
/* The eirp power values are in 0.01dBm units */
#define ATH12K_EIRP_PWR_SCALE		100

/**
 * struct ath12k_punct_mask - Structure to hold puncture mask limits
 * @offset: Array of offsets for puncture mask limits
 * @dbr: Array of dbr values corresponding to the offsets
 *
 * This structure is used to define the puncture mask limits for different
 * bandwidths. The `offset` array holds the offset values, and the `dbr` array
 * holds the corresponding dbr values. The size of both arrays is defined by
 * `MAX_PUNC_MASK_LIMITS`.
 */
struct ath12k_punct_mask {
	s16 offset[ATH12K_MAX_PUNC_MASK_LIMITS];
	s16 dbr[ATH12K_MAX_PUNC_MASK_LIMITS];
};

struct ath12k_punct_masks {
	struct ath12k_punct_mask *l_edge;
	struct ath12k_punct_mask *r_edge;
	struct ath12k_punct_mask *l;
	struct ath12k_punct_mask *r;
};

struct ath12k_punct_edges {
	s16 pu_l_edge;
	s16 pu_r_edge;
	s16 l_edge;
	s16 r_edge;
	s16 pu_edge1;
	s16 pu_edge2;
};

struct ath12k_pdbm_set {
	const s16 *pdbm1;
	const s16 *pdbm2;
	const s16 *pdbm3;
};

struct ath12k_puncture_ctx {
	struct ath12k_punct_masks masks;
	struct ath12k_punct_edges edges;
	struct ath12k_pdbm_set pdbms;
};

/**
 * enum ath12k_puncture_type - Enumeration of puncture types
 * @ATH12K_PUNCTURE_TYPE_EDGE: Represents edge puncture type
 * @ATH12K_PUNCTURE_TYPE_INTERIM_20_PLUS: Represents interim puncture type with 20 MHz
 * plus
 * @ATH12K_PUNCTURE_TYPE_INTERIM_20: Represents interim puncture type with 20 MHz
 * @ATH12K_PUNCTURE_TYPE_INVALID: Represents an invalid puncture type
 * @ATH12K_PUNCTURE_TYPE_FIRST: Place holder to hold first value for boundary checks.
 * @ATH12K_PUNCTURE_TYPE_LAST: Place holder to hold last value for boundary checks.
 *
 * This enumeration defines the different types of punctures that can occur
 * within a given bandwidth. Each type specifies a unique puncture pattern
 * and is used to determine the appropriate mask limits for the puncture.
 */
enum ath12k_puncture_type {
	ATH12K_PUNCTURE_TYPE_EDGE = 0,
	ATH12K_PUNCTURE_TYPE_INTERIM_20_PLUS,
	ATH12K_PUNCTURE_TYPE_INTERIM_20,
	ATH12K_PUNCTURE_TYPE_INVALID,

	ATH12K_PUNCTURE_TYPE_FIRST = ATH12K_PUNCTURE_TYPE_EDGE,
	ATH12K_PUNCTURE_TYPE_LAST  = ATH12K_PUNCTURE_TYPE_INTERIM_20,
};

enum ieee80211_neg_ttlm_res ath12k_mac_op_can_neg_ttlm(struct ieee80211_hw *hw,
						       struct ieee80211_vif *vif,
						       struct ieee80211_neg_ttlm *neg_ttlm);
void ath12k_mac_op_apply_neg_ttlm_per_client(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif,
					     struct ieee80211_sta *sta);
void ath12k_tid_to_link_mapping_evt_notify(struct ath12k_link_vif *arvif,
					   u16 mapping_switch_tsf,
					   u32 tid_to_link_mapping_status);

int ath12k_mac_op_set_radar_background(struct ieee80211_hw *hw,
				       struct cfg80211_chan_def *def);
void ath12k_mac_background_dfs_event(struct ath12k *ar,
				     enum ath12k_background_dfs_events ev);
int ath12k_wmi_vdev_adfs_ocac_abort_cmd_send(struct ath12k *ar, u32 vdev_id);
int ath12k_wmi_vdev_adfs_ch_cfg_cmd_send(struct ath12k *ar,
					 u32 vdev_id,
					 struct cfg80211_chan_def *def);

/* Used for offset adjustments in shaping the mask */
#define ATH12K_PUNCTURE_OFFSET_STEP	5

/* Used for wider shaping in interim puncture logic */
#define ATH12K_PUNCTURE_MASK_WIDTH	100

/* Minimum dB reduction applied when offset is beyond 1.5x the bandwidth */
#define ATH12K_REG_MASK_DB_MIN		-400

/* Base dB reduction applied when offset is beyond the bandwidth */
#define ATH12K_REG_MASK_DB_MID		-280

/* Base dB reduction applied when offset is beyond half the bandwidth */
#define ATH12K_REG_MASK_DB_BASE		-200

/* Step size for attenuation slope beyond full bandwidth */
#define ATH12K_REG_MASK_DB_STEP_MID	120

/* Step size for attenuation slope beyond half bandwidth */
#define ATH12K_REG_MASK_DB_STEP_BASE	80

/* Offset threshold adjustment used in near-range shaping */
#define ATH12K_REG_MASK_OFFSET_THRESHOLD	1

/* No attenuation applied when offset is within half bandwidth */
#define ATH12K_REG_MASK_DB_NONE	0

/**
 * ath12_reg_get_6g_min_psd - Calculate the minimum PSD values for a given
 * frequency and bandwidth
 * @iface: Pointer to the hostapd_iface structure
 * @freq: Frequency for which the minimum PSD values are to be calculated
 * @cfreq: Center frequency of the channel
 * @punc_bitmap: Bitmap indicating the punctured sub-channels
 * @bw: Bandwidth of the channel
 * @min_psd: Pointer to the variable where the minimum PSD value will be stored
 *
 * This function calculates the minimum PSD (Power Spectral Density) values for
 * a given frequency and bandwidth. It determines the puncture type and
 * calculates the regulatory mask values based on the puncture mask limits. The
 * minimum PSD value is then calculated by iterating through the adjacent
 * frequencies and applying the regulatory mask values.
 */
void
ath12_mac_reg_get_6g_min_psd(struct ath12k *ar, u16 freq, u16 cfreq,
			     u16 puncture_bitmap, u16 bw, s16 *min_psd);

int ath12k_mac_op_qos_mgmt_cfg(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta,
			       struct cfg80211_qm_req_data *qm_req,
			       struct cfg80211_qm_resp_data *qm_resp);

/**
 * struct channel_power - Regulatory power information for a 6 GHz channel
 * @center_freq: Center frequency (in MHz) of the channel
 * @chan_num: Hardware channel number
 * @tx_power: Maximum regulatory transmit power (in dBm)
 *
 * This structure holds the regulatory EIRP information for a specific 6 GHz
 * channel. It is typically populated based on regulatory domain data.
 */
struct channel_power {
	u16 center_freq;
	u8 chan_num;
	s8 tx_power;
};

/**
 * ath12k_mac_reg_get_max_reg_eirp_from_chan_list - Populate EIRP list for 6 GHz channels
 * @ar: Pointer to the ath12k device structure
 * @ap_6ghz_pwr_mode: AP power mode (e.g., WMI_REG_INDOOR_AP, WMI_REG_STD_POWER_AP)
 * @client_type: Client type (e.g., WMI_REG_DEFAULT_CLIENT, WMI_REG_SUBORDINATE_CLIENT)
 * @is_client_needed: Flag indicating whether the EIRP list is for a client device
 * @chan_eirp_list: Output array to be filled with channel power information
 *
 * This function determines the appropriate NL80211 regulatory power mode based on
 * the AP/client configuration and fills the provided channel_power array with
 * maximum regulatory transmit power, center frequency, and channel number for
 * each 6 GHz channel in that mode.
 *
 * Return: 0 on success, -EINVAL if the power mode mapping is invalid.
 */
int ath12k_mac_reg_get_max_reg_eirp_from_chan_list(struct ath12k *ar,
						   enum wmi_reg_6g_ap_type ap_6ghz_pwr_mode,
						   enum wmi_reg_6g_client_type client_type,
						   bool is_client_needed,
						   struct channel_power *chan_eirp_list);
int __ath12k_mac_mlo_setup(struct ath12k *ar);
int ath12k_mac_dynamic_wsi_remap(struct ath12k_base *ab);
void ath12k_mac_add_bridge_vdevs_iter(void *data, u8 *mac,
				      struct ieee80211_vif *vif);

/**
 * ath12k_mac_op_get_afc_eirp_pwr - Get the 6 GHz AFC EIRP power value
 * @hw: Input hardware
 * @freq: Input freq
 * @eirp_pwr: Output EIRP power
 */
int ath12k_mac_op_get_afc_eirp_pwr(struct ieee80211_hw *hw, u32 freq,
				   u32 *eirp_pwr);
/**
 * ath12k_mac_op_get_6ghz_dev_deployment_type - Get the 6 GHz device deployment
 * type.
 * @hw: Input hardware
 */
enum nl80211_6ghz_dev_deployment_type
ath12k_mac_op_get_6ghz_dev_deployment_type(struct ieee80211_hw *hw);

struct ieee80211_chanctx_conf *
	ath12k_mac_get_first_active_arvif_chanctx(struct ath12k *ar);
struct ieee80211_link_sta *ath12k_mac_get_link_sta(struct ath12k_link_sta *arsta);

int ath12k_mac_set_tx_antenna(struct ath12k *ar, u32 tx_ant);
int ath12k_mac_set_rx_antenna(struct ath12k *ar, u32 rx_ant);

int ath12k_mac_vendor_send_assoc_event(struct ath12k_link_sta *arsta,
				       struct ieee80211_link_sta *link_sta,
				       bool reassoc);
int ath12k_mac_vendor_send_disassoc_event(struct ath12k_link_sta *arsta,
					  struct ieee80211_link_sta *link_sta);
void ath12k_mac_update_freq_range(struct ath12k *ar,
				  u32 freq_low, u32 freq_high);
#endif
