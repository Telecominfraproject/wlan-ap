/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_PEER_H
#define ATH12K_DP_PEER_H

#include "dp_rx.h"

#define ATH12K_DP_PEER_ID_INVALID              0xFFFF
#define ATH12K_3LINK_MLO_MAX_STA_LINKS         3
#define ATH12K_DATA_TID_MAX 8

struct ppdu_user_delayba {
	u16 sw_peer_id;
	u32 info0;
	u16 ru_end;
	u16 ru_start;
	u32 info1;
	u32 rate_flags;
	u32 resp_rate_flags;
};

#define ATH12K_PEER_ML_ID_VALID         BIT(13)

struct ath12k_rx_peer_rate_stats {
	u64 ht_mcs_count[HAL_RX_MAX_MCS_HT + 1];
	u64 vht_mcs_count[HAL_RX_MAX_MCS_VHT + 1];
	u64 he_mcs_count[HAL_RX_MAX_MCS_HE + 1];
	u64 be_mcs_count[HAL_RX_MAX_MCS_BE + 1];
	u64 nss_count[HAL_RX_MAX_NSS];
	u64 bw_count[HAL_RX_BW_MAX];
	u64 gi_count[HAL_RX_GI_MAX];
	u64 legacy_count[HAL_RX_MAX_NUM_LEGACY_RATES];
	u64 rx_rate[HAL_RX_BW_MAX][HAL_RX_GI_MAX][HAL_RX_MAX_NSS][HAL_RX_MAX_MCS_HT + 1];
};

struct ath12k_rx_peer_stats {
	u64 num_msdu;
	u64 num_mpdu_fcs_ok;
	u64 num_mpdu_fcs_err;
	u64 tcp_msdu_count;
	u64 udp_msdu_count;
	u64 other_msdu_count;
	u64 ampdu_msdu_count;
	u64 non_ampdu_msdu_count;
	u64 stbc_count;
	u64 beamformed_count;
	u64 coding_count[HAL_RX_SU_MU_CODING_MAX];
	u64 tid_count[IEEE80211_NUM_TIDS + 1];
	u64 pream_cnt[HAL_RX_PREAMBLE_MAX];
	u64 reception_type[HAL_RX_RECEPTION_TYPE_MAX];
	u64 rx_duration;
	u64 dcm_count;
	u64 ru_alloc_cnt[HAL_RX_RU_ALLOC_TYPE_MAX];
	struct ath12k_rx_peer_rate_stats pkt_stats;
	struct ath12k_rx_peer_rate_stats byte_stats;
};

struct ath12k_atf_peer_airtime {
	struct peer_airtime_consumption tx_airtime_consumption[WME_NUM_AC];
	struct peer_airtime_consumption rx_airtime_consumption[WME_NUM_AC];
	u64 last_update_time;
};

struct ath12k_htt_tx_stats {
       struct ath12k_htt_data_stats stats[ATH12K_STATS_TYPE_MAX];
       u64 tx_duration;
       u64 ba_fails;
       u64 ack_fails;
       u16 ru_start;
       u16 ru_tones;
       u32 mu_group[MAX_MU_GROUP_ID];
};

DECLARE_EWMA(avg_rssi, 10, 8)

struct ath12k_mscs_ctxt {
	u8 user_priority_bitmap;
	u8 user_priority_limit;
	u8 tclas_mask;
};

struct ath12k_dp_link_peer {
	struct list_head list;
	struct ieee80211_sta *sta;
	struct ath12k_dp_peer *dp_peer;
	struct ieee80211_vif *vif;
	int vdev_id;
	u8 addr[ETH_ALEN];
	int peer_id;
	u16 ast_hash;
	u8 pdev_idx;
	u16 hw_peer_id;

	struct ppdu_user_delayba ppdu_stats_delayba;
	bool delayba_flag;
	bool is_authorized;
	bool mlo;
	/* protected by ab->data_lock */

	u16 ml_id;

	/* any other ML info common for all partners can be added
	 * here and would be same for all partner peers.
	 */
	u8 ml_addr[ETH_ALEN];

	/* To ensure only certain work related to dp is done once */
	bool primary_link;

	/* for reference to ath12k_link_sta */
	u8 link_id;

	/* peer addr based rhashtable list pointer */
	struct rhash_head rhash_addr;
	bool rhash_done;

	bool is_bridge_peer;
	u8 hw_link_id;

	/* link stats */
	struct rate_info txrate;
	struct rate_info last_txrate;
	u64 rx_duration;
	u64 tx_duration;
	s8 rssi_comb;
	u16 tx_retry_failed;
	u16 tx_retry_count;
	struct ewma_avg_rssi avg_rssi;
	struct ath12k_dp_link_peer_stats peer_stats;

	u16 tcl_metadata;
	bool assoc_success; /* information on peer assoc status from firmware */
	u32 flow_cnt[ATH12K_DATA_TID_MAX];
	u8 tid_weight[ATH12K_DATA_TID_MAX];

	struct ath12k_atf_peer_airtime atf_peer_airtime;
	u32 atf_peer_conf_airtime;
	u32 atf_actual_airtime;
	u8 atf_group_index;
	u8 atf_ul_airtime;
	u32 atf_actual_duration;
	u32 atf_actual_ul_duration;

	bool is_assigned;
};

struct ath12k_dp_peer {
	struct list_head list;
	struct ieee80211_sta *sta;
	struct net_device *dev;
	int peer_id;
	u8 addr[ETH_ALEN];
	bool is_mlo;
	bool is_vdev_peer;
	/* hw_link_id of the radio, valid only for self bss peer */
	u8 hw_link_id;

	u8 primary_link_id;
	u8 assoc_link_id;

	/* Lock for protection of link_peers*/
	spinlock_t link_peers_lock;
	struct ath12k_dp_link_peer __rcu *link_peers[ATH12K_NUM_MAX_LINKS];

	u32 peer_links_map;
	bool primary_link_frag_setup;

	bool is_authorized;
	enum hal_pn_type pn_type;

	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1];
	struct ath12k_dp_rx_tid rx_tid[IEEE80211_NUM_TIDS + 1];

	bool use_4addr;

	struct ath12k_dp_peer_qos *qos;
	/* Info used in MMIC verification of * RX fragments */
	struct crypto_shash *tfm_mmic;
	u8 mcast_keyidx;
	u8 ucast_keyidx;
	u16 sec_type;
	u16 sec_type_grp;
	u8 vdev_type_4addr;
	bool is_reset_mcbc;
	struct ath12k_mld_qos_stats mld_qos_stats[QOS_TID_MAX][QOS_TID_MDSUQ_MAX];
	bool qos_stats_lvl;

	u8 hw_links[ATH12K_GROUP_MAX_RADIO];
	u16 stats_link_id;
	struct ath12k_dp_peer_stats stats[ATH12K_DP_MAX_MLO_LINKS];
#if defined(CPTCFG_MAC80211_PPE_SUPPORT) || defined(CPTCFG_ATH12K_PPE_DS_SUPPORT)
	int ppe_vp_num;
#endif
	struct ath12k_mscs_ctxt mscs_ctxt;
	bool mscs_session_exists;
};

#define QOS_MSDUQ_MAX ((QOS_TID_MDSUQ_MAX * QOS_TID_MAX) + MSDUQ_MAX_DEF)

#define QOS_MAX_SCS_ID 128

#define QOS_TAG_MASK	GENMASK(7, 0)
#define QOS_QOS_ID_MASK	GENMASK(15, 8)

#define QOS_SCS_TAG	0xB9
#define QOS_MSCS_TAG	0x58

#define QOS_INVALID_MSDUQ  0x3F

#define SCS_MSDUQ_MASK		GENMASK(5, 0)
#define SCS_QOS_ID_MASK		GENMASK(15, 6)

#define MSDUQ_MAX_DEF		16
#define MSDUQ_TID_MASK		GENMASK(2, 0)
#define MSDUQ_MASK		GENMASK(5, 3)

#define MSDUQ_TID		GENMASK(2, 0)
#define MSDUQ_FLOW_OVERRIDE	BIT(3)
#define MSDUQ_WHO_CL_INFO	GENMASK(5, 4)

#define QOS_NW_DELAY_MAX	0x3FFFF
#define QOS_NW_DELAY		GENMASK(23, 6)
#define QOS_NW_TAG_SHIFT	GENMASK(23, 16)
#define QOS_TAG_ID		GENMASK(31, 24)
#define QOS_NW_DELAY_SHIFT	0x6
#define QOS_VALID_TAG		BIT(30)
#define DP_RETRY_COUNT		7

struct ath12k_msduq {
	bool reserved;
	u8 qos_id;
	u32 tgt_opaque_id;
	u16 msduq;
};

struct ath12k_dl_scs {
	u16 qos_id_msduq;
};

struct ath12k_dp_peer_qos {
	struct ath12k_dl_scs scs_map[QOS_MAX_SCS_ID];
	struct ath12k_msduq msduq_map[QOS_TID_MAX][QOS_TID_MDSUQ_MAX];
	void *telemetry_peer_ctx;
};

void ath12k_peer_unmap_event(struct ath12k_base *ab, u16 peer_id);
void ath12k_peer_map_event(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
			   u8 *mac_addr, u16 ast_hash, u16 hw_peer_id);
struct ath12k_dp_peer *ath12k_dp_peer_find(struct ath12k_dp_hw *dp_hw,
					   u8 *addr);
struct ath12k_dp_peer *ath12k_dp_peer_find_by_addr_and_sta(struct ath12k_dp_hw *dp_hw,
							   u8 *addr, struct ieee80211_sta *sta);
struct ath12k_dp_peer *ath12k_dp_peer_create_find(struct ath12k_dp_hw *dp_hw, u8 *addr,
						  struct ieee80211_sta *sta,
						  bool mlo_peer);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_vdev_id_and_addr(struct ath12k_dp *dp,
					     int vdev_id, const u8 *addr);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_addr(struct ath12k_dp *dp, const u8 *addr);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_id(struct ath12k_dp *dp, int peer_id);
bool ath12k_dp_link_peer_exist_by_vdev_id(struct ath12k_dp *dp, int vdev_id);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_pdev_idx(struct ath12k_dp *dp, u8 pdev_idx,
				     const u8 *addr);
int ath12k_dp_link_peer_rhash_tbl_init(struct ath12k_dp *dp);
void ath12k_dp_link_peer_rhash_tbl_destroy(struct ath12k_dp *dp);
int ath12k_dp_link_peer_rhash_add(struct ath12k_dp *dp,
				  struct ath12k_dp_link_peer *peer);
int ath12k_dp_link_peer_rhash_delete(struct ath12k_dp *dp,
				     struct ath12k_dp_link_peer *peer);
int ath12k_dp_peer_create(struct ath12k_dp_hw *dp_hw, u8 *addr,
			  struct ath12k_dp_peer_create_params *params,
			  struct ieee80211_vif *vif);
void ath12k_dp_peer_delete(struct ath12k_dp_hw *dp_hw, u8 *addr,
			   struct ieee80211_sta *sta, u8 hw_link_id);
struct ath12k_dp_peer *ath12k_dp_peer_find_by_peerid_index(struct ath12k_dp *dp,
							   struct ath12k_pdev_dp *dp_pdev,
							   u16 peer_id);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_peerid_index(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev, u16 peer_id);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_ast(struct ath12k_dp *dp,
				int ast_hash);
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_ml_peer_vdev_id(struct ath12k_dp *dp,
					    int peer_id,
					    int vdev_id);

void ath12k_peer_mlo_map_event(struct ath12k_base *ab, struct sk_buff *skb);

static inline
enum nl80211_iftype ath12k_peer_get_peer_type(struct ath12k_dp_link_peer *peer)
{
	return peer->vif->type;
}

void ath12k_peer_mlo_unmap_event(struct ath12k_base *ab, struct sk_buff *skb);
struct ath12k_dp_peer_qos *
ath12k_dp_peer_qos_get(struct ath12k_dp *dp,
		       struct ath12k_dp_peer *peer);
struct ath12k_dp_peer_qos *
ath12k_dp_peer_qos_alloc(struct ath12k_dp *dp,
			 struct ath12k_dp_peer *peer);
void ath12k_dp_peer_qos_free(struct ath12k_dp *dp,
			     struct ath12k_dp_peer *peer);
bool ath12k_dp_qos_stats_alloc(struct ath12k *ar,
			       struct ieee80211_vif *vif,
			       struct ath12k_dp_link_peer *peer);
int ath12k_dp_peer_scs_add(struct ath12k_base *ab,
			   struct ath12k_dp_peer_qos *qos,
			   u8 scs_id, u16 qos_profile_id);
int ath12k_dp_peer_scs_del(struct ath12k_base *ab,
			   struct ath12k_dp_peer_qos *qos,
			   u8 scs_id);
int ath12k_dp_peer_scs_data(struct ath12k_dp *dp,
			    struct ath12k_dp_peer_qos *qos, u8 scs_id,
			    u16 *msduq, u16 *qos_id);
u16 ath12k_dp_peer_scs_get_qos_id(struct ath12k_base *ab,
				  struct ath12k_dp_peer_qos *qos, u8 scs_id);
u16 ath12k_dp_peer_qos_msduq(struct ath12k_base *ab,
			     struct ath12k_dp_peer_qos *qos,
			     struct ath12k_dp_link_peer *link_peer,
			     struct ath12k *ar,
			     u16 qos_id, u8 svc_id);
u16 dp_peer_msduq_qos_id(struct ath12k_base *ab,
			 struct ath12k_dp_peer_qos *qos,
			 u16 msduq);
void ath12k_peer_qos_queue_ind_handler(struct ath12k_base *ab,
				       struct sk_buff *skb);
void ath12k_link_peer_free(struct ath12k_dp_link_peer *peer);
#endif
