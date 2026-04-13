/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef ATH12K_DP_RX_H
#define ATH12K_DP_RX_H

#include "core.h"
#include "debug.h"
#include <crypto/hash.h>

#define DP_MAX_NWIFI_HDR_LEN	30

#define ip_hdrlen(iph) ((iph)->ihl * 4)

/* different supported pkt types for routing */
enum ath12k_routing_pkt_type {
	ATH12K_PKT_TYPE_ARP_IPV4,
	ATH12K_PKT_TYPE_NS_IPV6,
	ATH12K_PKT_TYPE_IGMP_IPV4,
	ATH12K_PKT_TYPE_MLD_IPV6,
	ATH12K_PKT_TYPE_DHCP_IPV4,
	ATH12K_PKT_TYPE_DHCP_IPV6,
	ATH12K_PKT_TYPE_DNS_TCP_IPV4,
	ATH12K_PKT_TYPE_DNS_TCP_IPV6,
	ATH12K_PKT_TYPE_DNS_UDP_IPV4,
	ATH12K_PKT_TYPE_DNS_UDP_IPV6,
	ATH12K_PKT_TYPE_ICMP_IPV4,
	ATH12K_PKT_TYPE_ICMP_IPV6,
	ATH12K_PKT_TYPE_TCP_IPV4,
	ATH12K_PKT_TYPE_TCP_IPV6,
	ATH12K_PKT_TYPE_UDP_IPV4,
	ATH12K_PKT_TYPE_UDP_IPV6,
	ATH12K_PKT_TYPE_IPV4,
	ATH12K_PKT_TYPE_IPV6,
	ATH12K_PKT_TYPE_EAP,
	ATH12K_PKT_TYPE_MAX
};

enum filter_mgmt {
	FILTER_MGMT_ASSOC_REQ                   = BIT(0),
	FILTER_MGMT_ASSOC_RESP                  = BIT(1),
	FILTER_MGMT_REASSOC_REQ                 = BIT(2),
	FILTER_MGMT_REASSOC_RESP                = BIT(3),
	FILTER_MGMT_PROBE_REQ                   = BIT(4),
	FILTER_MGMT_PROBE_RESP                  = BIT(5),
	FILTER_MGMT_TIM_ADVT                    = BIT(6),
	FILTER_MGMT_RESERVED_7                  = BIT(7),
	FILTER_MGMT_BEACON                      = BIT(8),
	FILTER_MGMT_ATIM                        = BIT(9),
	FILTER_MGMT_DISASSOC                    = BIT(10),
	FILTER_MGMT_AUTH                        = BIT(11),
	FILTER_MGMT_DEAUTH                      = BIT(12),
	FILTER_MGMT_ACTION                      = BIT(13),
	FILTER_MGMT_ACT_NO_ACK                  = BIT(14),
	FILTER_MGMT_RESERVED_15                 = BIT(15),
	FILTER_MGMT_ALL                         = ~(0)
};

enum ctrl_filter {
	FILTER_CTRL_RESERVED_1                  = BIT(0),
	FILTER_CTRL_RESERVED_2                  = BIT(1),
	FILTER_CTRL_TRIGGER                     = BIT(2),
	FILTER_CTRL_RESERVED_4                  = BIT(3),
	FILTER_CTRL_BF_REP_POLL                 = BIT(4),
	FILTER_CTRL_VHT_NDP                     = BIT(5),
	FILTER_CTRL_FRAME_EXT                   = BIT(6),
	FILTER_CTRL_CTRLWRAP                    = BIT(7),
	FILTER_CTRL_BA_REQ                      = BIT(8),
	FILTER_CTRL_BA                          = BIT(9),
	FILTER_CTRL_PSPOLL                      = BIT(10),
	FILTER_CTRL_RTS                         = BIT(11),
	FILTER_CTRL_CTS                         = BIT(12),
	FILTER_CTRL_ACK                         = BIT(13),
	FILTER_CTRL_CFEND                       = BIT(14),
	FILTER_CTRL_CFEND_CFACK                 = BIT(15),
	FILTER_CTRL_ALL                         = ~(0)
};

enum data_filter {
	FILTER_DATA_DATA                        = BIT(0),
	FILTER_DATA_NULL                        = BIT(3),
	FILTER_DATA_MCAST                       = BIT(14),
	FILTER_DATA_UCAST                       = BIT(15),
	FILTER_DATA_ALL                         = ~(0)
};

#define ATH12K_RX_PROTOCOL_TAG_START_OFFSET  128
#define ATH12K_ROUTE_WBM_RELEASE(ab) \
	((ab)->hw_params->route_wbm_release)
#define ATH12K_ROUTE_EAP_METADATA       (ATH12K_RX_PROTOCOL_TAG_START_OFFSET + ATH12K_PKT_TYPE_EAP)

struct ath12k_dp_rx_tid {
	u8 tid;
	u32 *vaddr;
	dma_addr_t paddr;
	u32 size;
	u32 pending_desc_size;
	u32 ba_win_sz;
	bool active;

	/* Info related to rx fragments */
	u32 cur_sn;
	u16 last_frag_no;
	u16 rx_frag_bitmap;

	struct sk_buff_head rx_frags;
	struct hal_reo_dest_ring *dst_ring_desc;

	/* Timer info related to fragments */
	struct timer_list frag_timer;
	struct ath12k_dp *dp;

	/* Info related to UMAC migration */
	u16     peer_id;
	u8      chip_id;
};

struct ath12k_dp_rx_reo_cache_flush_elem {
	struct list_head list;
	struct ath12k_dp_rx_tid data;
	unsigned long ts;
};

struct dp_reo_update_rx_queue_elem {
	struct list_head list;
	struct ath12k_dp_rx_tid data;
	int peer_id;
	u8 tid;
	bool reo_cmd_update_rx_queue_resend_flag;
	bool is_ml_peer;
	u16 ml_peer_id;
};

struct ath12k_dp_rx_reo_cmd {
	struct list_head list;
	struct ath12k_dp_rx_tid data;
	int cmd_num;
	void (*handler)(struct ath12k_dp *dp, void *ctx,
			enum hal_reo_cmd_status status);
};

#define ATH12K_DP_RX_REO_DESC_FREE_THRES  64
#define ATH12K_DP_RX_REO_DESC_FREE_TIMEOUT_MS 1000

enum ath12k_dp_rx_decap_type {
	DP_RX_DECAP_TYPE_RAW,
	DP_RX_DECAP_TYPE_NATIVE_WIFI,
	DP_RX_DECAP_TYPE_ETHERNET2_DIX,
	DP_RX_DECAP_TYPE_8023,
	DP_RX_DECAP_TYPE_INVALID,
};

struct ath12k_dp_rx_rfc1042_hdr {
	u8 llc_dsap;
	u8 llc_ssap;
	u8 llc_ctrl;
	u8 snap_oui[3];
	__be16 snap_type;
} __packed;

struct dp_rx_fst {
	u8 *base;
	struct hal_rx_fst *hal_rx_fst;
	u16 num_entries;
	u16 ipv4_fse_rule_cnt;
	u16 ipv6_fse_rule_cnt;
	u16 flows_per_reo[4];
	u32 flow_add_fail;
	u32 flow_del_fail;
	/* spinlock to prevent concurrent table access */
	spinlock_t fst_lock;
};

static inline u32 ath12k_he_gi_to_nl80211_he_gi(u8 sgi)
{
	u32 ret = 0;

	switch (sgi) {
	case RX_MSDU_START_SGI_0_8_US:
		ret = NL80211_RATE_INFO_HE_GI_0_8;
		break;
	case RX_MSDU_START_SGI_1_6_US:
		ret = NL80211_RATE_INFO_HE_GI_1_6;
		break;
	case RX_MSDU_START_SGI_3_2_US:
		ret = NL80211_RATE_INFO_HE_GI_3_2;
		break;
	default:
		ret = NL80211_RATE_INFO_HE_GI_0_8;
		break;
	}

	return ret;
}

static inline u32 ath12k_eht_gi_to_nl80211_eht_gi(u8 sgi)
{
	u32 ret = 0;

	switch (sgi) {
	case RX_MSDU_START_SGI_0_8_US:
		ret = NL80211_RATE_INFO_EHT_GI_0_8;
		break;
	case RX_MSDU_START_SGI_1_6_US:
		ret = NL80211_RATE_INFO_EHT_GI_1_6;
		break;
	case RX_MSDU_START_SGI_3_2_US:
		ret = NL80211_RATE_INFO_EHT_GI_3_2;
		break;
	}

	return ret;
}

int ath12k_dp_rx_ampdu_start(struct ath12k *ar,
			     struct ieee80211_ampdu_params *params,
			     u8 link_id);
int ath12k_dp_rx_ampdu_stop(struct ath12k *ar,
			    struct ieee80211_ampdu_params *params,
			    u8 link_id);
int ath12k_dp_rx_peer_pn_replay_config(struct ath12k_link_vif *arvif,
				       const u8 *peer_addr,
				       enum set_key_cmd key_cmd,
				       struct ieee80211_key_conf *key);
void ath12k_dp_rx_peer_tid_cleanup(struct ath12k *ar, struct ath12k_dp_link_peer *peer);
int ath12k_dp_rx_reo_setup(struct ath12k_base *ab);
void ath12k_dp_rx_reo_cleanup(struct ath12k_base *ab);
int ath12k_dp_rx_alloc(struct ath12k_base *ab);
void ath12k_dp_rx_free(struct ath12k_base *ab);
void ath12k_dp_rx_reo_cmd_list_cleanup(struct ath12k_base *ab);
void ath12k_dp_rx_bufs_replenish(struct ath12k_dp *dp,
				struct dp_rxdma_ring *rx_ring,
				struct list_head *used_list);
int ath12k_dp_rx_peer_frag_setup(struct ath12k *ar,
				 struct ath12k_dp_link_peer *peer,
				 struct crypto_shash *tfm);

struct ath12k_dp_link_peer *
ath12k_dp_rx_h_find_peer(struct ath12k_dp *dp, struct hal_rx_desc *rx_desc, u16 peer_id);
u8 ath12k_dp_rx_h_decap_type(struct ath12k_base *ab,
			     struct hal_rx_desc *desc);
u32 ath12k_dp_rx_h_mpdu_err(struct ath12k_base *ab,
			    struct hal_rx_desc *desc);
void ath12k_dp_rx_process_reo_status(struct ath12k_base *ab);
u16 ath12k_dp_rx_h_seq_no(struct ath12k_base *ab, struct hal_rx_desc *desc);
void ath12k_dp_rx_deliver_msdu(struct ath12k_pdev_dp *dp_pdev,
			       struct napi_struct *napi,
			       struct sk_buff *msdu,
			       struct ieee80211_rx_status *status,
			       u8 hw_link_id, bool is_mcbc, u16 peer_id, u16 tid);
void ath12k_dp_reo_cmd_free(struct ath12k_dp *dp, void *ctx,
			    enum hal_reo_cmd_status status);
void ath12k_dp_rx_frags_cleanup(struct ath12k_dp_rx_tid *rx_tid,
				bool rel_link_desc);
int ath12k_dp_rx_crypto_mic_len(struct ath12k_pdev_dp *dp_pdev,
				enum hal_encrypt_type enctype);
int ath12k_dp_rx_crypto_param_len(struct ath12k_pdev_dp *dp_pdev,
				  enum hal_encrypt_type enctype);
int ath12k_dp_rx_crypto_icv_len(struct ath12k_pdev_dp *dp_pdev,
				enum hal_encrypt_type enctype);
void ath12k_dp_rx_h_undecap_frag(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				 enum hal_encrypt_type enctype, u32 flags);
int ath12k_dp_rx_h_michael_mic(struct crypto_shash *tfm, u8 *key,
			       struct ieee80211_hdr *hdr, u8 *data,
			       size_t data_len, u8 *mic);
void ath12k_dp_rx_h_undecap_raw(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				struct hal_rx_desc *rx_desc,
				enum hal_encrypt_type enctype,
				struct ieee80211_rx_status *status, bool decrypted,
				u16 peer_id, bool is_first_msdu, bool is_last_msdu);
int ath12k_hw_grp_dp_rx_invalidate_entry(struct ath12k_hw_group *ag,
					 enum dp_htt_flow_fst_operation operation,
					 struct hal_flow_tuple_info *tuple_info);
int ath12k_dp_rx_flow_add_entry(struct ath12k_base *ab,
				struct rx_flow_info *flow_info);
int ath12k_dp_rx_flow_delete_entry(struct ath12k_base *ab,
				   struct rx_flow_info *flow_info);
int ath12k_dp_rx_flow_delete_all_entries(struct ath12k_base *ab);
struct dp_rx_fst *ath12k_dp_rx_fst_attach(struct ath12k_base *ab);
void ath12k_dp_rx_fst_detach(struct ath12k_base *ab, struct dp_rx_fst *fst);
void ath12k_dp_fst_core_map_init(struct ath12k_base *ab);
void ath12k_dp_rx_fst_init(struct ath12k_base *ab);
ssize_t ath12k_dp_dump_fst_table(struct ath12k_base *ab, char *buf, int size);
size_t ath12k_dp_list_cut_nodes(struct list_head *list,
				struct list_head *head, size_t count);
void ath12k_dp_tid_cleanup(struct ath12k_base *ab);
void ath12k_dp_peer_tid_setup(struct ath12k_base *ab);
void ath12k_dp_peer_reo_tid_setup(struct ath12k *ar, int vdev_id,
				  const u8 *peer_mac);
void ath12k_dp_tid_setup(void *data, struct ieee80211_sta *sta);
void ath12k_dp_reset_rx_reo_tid_q(void *vaddr, u32 ba_window_size, u8 tid);
int
ath12k_dp_rx_htt_rxdma_rxole_ppe_cfg_set(struct ath12k_base *ab,
					 struct ath12k_dp_htt_rxdma_ppe_cfg_param *param);
void
ath12k_dp_primary_peer_migrate_setup(struct ath12k_dp *dp, void *ctx,
				     enum hal_reo_cmd_status status);
int
ath12k_dp_peer_migrate(struct ath12k_sta *ahsta, u16 peer_id,
		       u8 chip_id);
int ath12k_dp_rx_pkt_type_filter(struct ath12k *ar,
				 enum ath12k_routing_pkt_type pkt_type,
				 u32 meta_data);
int ath12k_dp_rxdma_buf_setup(struct ath12k_base *ab);
void ath12k_dp_rx_update_peer_msdu_stats(struct ath12k_dp_peer *peer,
					 struct rx_msdu_desc_info *rx_msdu_info,
					 struct rx_mpdu_desc_info *rx_mpdu_info,
					 u8 link_id, int ring_id);
void ath12k_dp_rx_skb_free(struct sk_buff *skb, struct ath12k_dp *dp, int ring,
			   enum ath12k_dp_rx_error drop_reason);
void ath12k_dp_rx_classify_mscs(struct ath12k_base *ab,
				struct ath12k_dp_peer *peer,
				struct sk_buff *msdu, u8 tid);
#endif /* ATH12K_DP_RX_H */
