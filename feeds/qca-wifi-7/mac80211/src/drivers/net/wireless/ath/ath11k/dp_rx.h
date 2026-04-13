/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef ATH11K_DP_RX_H
#define ATH11K_DP_RX_H

#include "core.h"
#include "rx_desc.h"
#include "debug.h"

#define DP_MAX_NWIFI_HDR_LEN	36

#define DP_RX_MAX_IDR_BUF    256

#define DP_RX_MPDU_ERR_FCS			BIT(0)
#define DP_RX_MPDU_ERR_DECRYPT			BIT(1)
#define DP_RX_MPDU_ERR_TKIP_MIC			BIT(2)
#define DP_RX_MPDU_ERR_AMSDU_ERR		BIT(3)
#define DP_RX_MPDU_ERR_OVERFLOW			BIT(4)
#define DP_RX_MPDU_ERR_MSDU_LEN			BIT(5)
#define DP_RX_MPDU_ERR_MPDU_LEN			BIT(6)
#define DP_RX_MPDU_ERR_UNENCRYPTED_FRAME	BIT(7)

/* different supported pkt types for routing */
enum ath11k_routing_pkt_type {
	ATH11K_PKT_TYPE_ARP_IPV4,
	ATH11K_PKT_TYPE_NS_IPV6,
	ATH11K_PKT_TYPE_IGMP_IPV4,
	ATH11K_PKT_TYPE_MLD_IPV6,
	ATH11K_PKT_TYPE_DHCP_IPV4,
	ATH11K_PKT_TYPE_DHCP_IPV6,
	ATH11K_PKT_TYPE_DNS_TCP_IPV4,
	ATH11K_PKT_TYPE_DNS_TCP_IPV6,
	ATH11K_PKT_TYPE_DNS_UDP_IPV4,
	ATH11K_PKT_TYPE_DNS_UDP_IPV6,
	ATH11K_PKT_TYPE_ICMP_IPV4,
	ATH11K_PKT_TYPE_ICMP_IPV6,
	ATH11K_PKT_TYPE_TCP_IPV4,
	ATH11K_PKT_TYPE_TCP_IPV6,
	ATH11K_PKT_TYPE_UDP_IPV4,
	ATH11K_PKT_TYPE_UDP_IPV6,
	ATH11K_PKT_TYPE_IPV4,
	ATH11K_PKT_TYPE_IPV6,
	ATH11K_PKT_TYPE_EAP,
	ATH11K_PKT_TYPE_MAX
};

#define ATH11K_RX_PROTOCOL_TAG_START_OFFSET  128
#define ATH11K_ROUTE_WBM_RELEASE	5
#define ATH11K_ROUTE_EAP_METADATA	(ATH11K_RX_PROTOCOL_TAG_START_OFFSET + ATH11K_PKT_TYPE_EAP)

enum dp_rx_decap_type {
	DP_RX_DECAP_TYPE_RAW,
	DP_RX_DECAP_TYPE_NATIVE_WIFI,
	DP_RX_DECAP_TYPE_ETHERNET2_DIX,
	DP_RX_DECAP_TYPE_8023,
};

struct ath11k_dp_amsdu_subframe_hdr {
	u8 dst[ETH_ALEN];
	u8 src[ETH_ALEN];
	__be16 len;
} __packed;

struct ath11k_dp_rfc1042_hdr {
	u8 llc_dsap;
	u8 llc_ssap;
	u8 llc_ctrl;
	u8 snap_oui[3];
	__be16 snap_type;
} __packed;

static inline u32 ath11k_he_gi_to_nl80211_he_gi(u8 sgi)
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
        }

        return ret;
}

int ath11k_dp_rx_ampdu_start(struct ath11k_vif *arvif,
			     struct ieee80211_ampdu_params *params);
int ath11k_dp_rx_ampdu_stop(struct ath11k_vif *arvif,
			    struct ieee80211_ampdu_params *params);
int ath11k_dp_peer_rx_pn_replay_config(struct ath11k_vif *arvif,
				       const u8 *peer_addr,
				       enum set_key_cmd key_cmd,
				       struct ieee80211_key_conf *key);
void ath11k_peer_frags_flush(struct ath11k *ar, struct ath11k_peer *peer);
void ath11k_peer_rx_tid_cleanup(struct ath11k *ar, struct ath11k_peer *peer);
void ath11k_peer_rx_tid_delete(struct ath11k *ar,
			       struct ath11k_peer *peer, u8 tid);
int ath11k_peer_rx_tid_setup(struct ath11k *ar, const u8 *peer_mac, int vdev_id,
			     u8 tid, u32 ba_win_sz, u16 ssn,
			     enum hal_pn_type pn_type);
int ath11k_dp_rx_pkt_type_filter(struct ath11k *ar,
				  enum ath11k_routing_pkt_type pkt_type,
				  u32 meta_data);
void ath11k_dp_htt_htc_t2h_msg_handler(struct ath11k_base *ab,
				       struct sk_buff *skb);
int ath11k_dp_pdev_reo_setup(struct ath11k_base *ab);
void ath11k_dp_pdev_reo_cleanup(struct ath11k_base *ab);
int ath11k_dp_rx_pdev_alloc(struct ath11k_base *ab, int pdev_idx);
void ath11k_dp_rx_pdev_free(struct ath11k_base *ab, int pdev_idx);
void ath11k_dp_reo_cmd_list_cleanup(struct ath11k_base *ab);
void ath11k_dp_process_reo_status(struct ath11k_base *ab);
int ath11k_dp_process_rxdma_err(struct ath11k_base *ab, int mac_id, int budget);
int ath11k_dp_rx_process_wbm_err(struct ath11k_base *ab,
				 struct napi_struct *napi, int budget);
int ath11k_dp_process_rx_err(struct ath11k_base *ab, struct napi_struct *napi,
			     int budget);
int ath11k_dp_process_rx(struct ath11k_base *ab, int mac_id,
			 struct napi_struct *napi,
			 int budget);
int ath11k_dp_rxbufs_replenish(struct ath11k_base *ab, int mac_id,
			       struct dp_rxdma_ring *rx_ring,
			       int req_entries,
 			       enum hal_rx_buf_return_buf_manager mgr,
			       u32 *buf_id);
int ath11k_dp_htt_tlv_iter(struct ath11k_base *ab, const void *ptr, size_t len,
			   int (*iter)(struct ath11k_base *ar, u16 tag, u16 len,
				       const void *ptr, void *data),
			   void *data);
int ath11k_dp_rx_mon_status_bufs_replenish(struct ath11k_base *ab, int mac_id,
					   struct dp_rxdma_ring *rx_ring,
					   int req_entries,
					   enum hal_rx_buf_return_buf_manager mgr);
void ath11k_dp_rx_mon_dest_process(struct ath11k *ar, int mac_id, u32 quota,
				   struct napi_struct *napi);
int ath11k_dp_rx_process_mon_rings(struct ath11k_base *ab, int mac_id,
				   struct napi_struct *napi, int budget);
int ath11k_dp_rx_pdev_mon_detach(struct ath11k *ar);
int ath11k_dp_rx_pdev_mon_attach(struct ath11k *ar);
int ath11k_peer_rx_frag_setup(struct ath11k *ar, const u8 *peer_mac, int vdev_id);

int ath11k_dp_rx_pktlog_start(struct ath11k_base *ab);
int ath11k_dp_rx_pktlog_stop(struct ath11k_base *ab, bool stop_timer);

int ath11k_dp_rx_crypto_mic_len(struct ath11k *ar, enum hal_encrypt_type enctype);

int ath11k_dp_rx_crypto_mic_len(struct ath11k *ar,
				       enum hal_encrypt_type enctype);
int ath11k_dp_rx_crypto_param_len(struct ath11k *ar,
					 enum hal_encrypt_type enctype);
int ath11k_dp_rx_crypto_icv_len(struct ath11k *ar,
				       enum hal_encrypt_type enctype);
bool ath11k_dp_rx_h_msdu_end_first_msdu(struct ath11k_base *ab,
					struct hal_rx_desc *desc);
bool ath11k_dp_rx_h_attn_is_mcbc(struct ath11k_base *ab,
				 struct hal_rx_desc *desc);
u16 ath11k_dp_rx_h_mpdu_start_peer_id(struct ath11k_base *ab,
				      struct hal_rx_desc *desc);
void ath11k_dp_rx_from_nss(struct ath11k *ar, struct sk_buff *msdu,
                           struct napi_struct *napi);
#endif /* ATH11K_DP_RX_H */
