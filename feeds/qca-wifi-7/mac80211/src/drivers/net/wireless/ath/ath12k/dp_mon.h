/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_MON_H
#define ATH12K_DP_MON_H

#include "core.h"
#include "dp_peer.h"
#include "debug.h"
#include "pktlog.h"

#include "hal_mon_cmn.h"

#define ATH12K_DP_MON_RX_BUF_SIZE	2048
#define ATH12K_MON_MAGIC_VALUE		0xDECAFEED
#define ATH12K_DP_MON_MAX_RADIO_TAP_HDR 128
#define ATH12K_MON_RX_DOT11_OFFSET	5
#define ATH12K_MON_RX_PKT_OFFSET	8
#define ATH12K_DP_WLAN_MAX_AC		4

#define	ATH12K_DP_MON_MIN_FRAGS_RESTITCH	2
#define	ATH12K_DP_MON_L3_HDR_PAD		2
#define	ATH12K_DP_MON_NONRAW_L2_HDR_PAD_BYTE	2
#define	ATH12K_DP_MON_RAW_L2_HDR_PAD_BYTE	0
#define	ATH12K_DP_MON_LLC_SIZE			3
#define	ATH12K_DP_MON_SNAP_SIZE			5
#define	ATH12K_DP_MON_DECAP_HDR_SIZE		14
#define	ATH12K_DP_MON_KEYIV			0x20
#define ATH12K_DP_MON_ETH_TYPE_VLAN_LEN		4
#define ATH12K_DP_MON_ETH_TYPE_DOUBLE_VLAN_LEN	8

#define DP_RXDMA_MON_STATUS_RING_SIZE	1024
#define DP_RXDMA_MONITOR_DESC_RING_SIZE	4096
#if defined(CONFIG_ATH12K_MEM_PROFILE_512M) || defined (CPTCFG_ATH12K_MEM_PROFILE_512M)
#define DP_RXDMA_MONITOR_BUF_RING_SIZE  256
#define DP_RXDMA_MONITOR_DST_RING_SIZE  512
#else
#define DP_RXDMA_MONITOR_BUF_RING_SIZE 8192
#define DP_RXDMA_MONITOR_DST_RING_SIZE 8192
#endif
#define DP_TX_MONITOR_BUF_RING_SIZE	4096
#define DP_TX_MONITOR_DEST_RING_SIZE	2048

#define DP_TX_MONITOR_BUF_SIZE		2048
#define DP_TX_MONITOR_BUF_SIZE_MIN	48
#define DP_TX_MONITOR_BUF_SIZE_MAX	8192

#define DP_RX_MON_BUFFER_SIZE		2048
#define RX_MON_STATUS_BASE_BUF_SIZE	2048
#define RX_MON_STATUS_BUF_ALIGN		128
#define RX_MON_STATUS_BUF_RESERVATION	128
#define RX_MON_STATUS_BUF_SIZE		(RX_MON_STATUS_BASE_BUF_SIZE - \
				 (RX_MON_STATUS_BUF_RESERVATION + \
				  RX_MON_STATUS_BUF_ALIGN + \
				  SKB_DATA_ALIGN(sizeof(struct skb_shared_info))))
#define DP_NOT_PPDU_ID_WRAP_AROUND 20000

#define DP_MON_RXDMA_BUF_COOKIE_BUF_ID		GENMASK(17, 0)
#define DP_MON_RXDMA_BUF_COOKIE_PDEV_ID 	GENMASK(19, 18)
#define DP_MON_RX_HDR_LEN			128

struct ath12k_mon_data;
struct dp_mon_rx_filter;

struct ath12k_dp_mon_pad_params {
	u32 frag_size;
	u32 msdu_llc_len;
	u32 pad_byte_holder;
	u32 frag_idx;
	bool is_head_msdu;
};

struct dp_rxdma_mon_ring {
	struct dp_srng refill_buf_ring;
	struct idr bufs_idr;
	/* Protects bufs_idr */
	spinlock_t idr_lock;
	int bufs_max;
};

enum dp_mon_stats_mode {
	ATH12k_DP_MON_BASIC_STATS,
	ATH12k_DP_MON_EXTD_STATS
};

struct ath12k_dp_arch_mon_ops {
	int (*rx_srng_setup)(struct ath12k_dp *dp);
	void (*rx_srng_cleanup)(struct ath12k_dp *dp);
	int (*rx_buf_setup)(struct ath12k_dp *dp);
	void (*rx_buf_free)(struct ath12k_dp *dp);
	int (*rx_htt_srng_setup)(struct ath12k_dp *dp);
	int (*mon_pdev_alloc)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_pdev_free)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_pdev_rx_srng_setup)(struct ath12k_pdev_dp *dp_pdev,
				      u32 mac_id);
	void (*mon_pdev_rx_srng_cleanup)(struct ath12k_pdev_dp *dp_pdev);
	int (*mon_pdev_rx_htt_srng_setup)(struct ath12k_pdev_dp *dp_pdev,
					  u32 mac_id);
	void (*mon_pdev_rx_attach)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_pdev_rx_mpdu_list_init)(struct ath12k_mon_data *pmon);
	int (*mon_rx_srng_process)(struct ath12k_pdev_dp *dp_pdev, int mac_id,
				      struct napi_struct *napi, int *budget);
	int (*update_telemetry_stats)(struct ath12k_base *ab,
				       const int pdev_id);
	int (*rx_filter_alloc)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_filter_free)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_stats_enable)(struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_stats_mode mode);
	void (*rx_stats_disable)(struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_stats_mode mode);
	int (*rx_filter_update)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_monitor_mode_set)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_monitor_mode_reset)(struct ath12k_pdev_dp *dp_pdev);
	int (*setup_ppdu_desc)(struct ath12k_pdev_dp *pdev_dp);
	void (*cleanup_ppdu_desc)(struct ath12k_pdev_dp *pdev_dp);
	int (*mon_rx_wq_init)(struct ath12k_pdev_dp *pdev_dp);
	void (*mon_rx_wq_deinit)(struct ath12k_pdev_dp *pdev_dp);
	void (*rx_nrp_set)(struct ath12k_pdev_dp *dp_pdev);
	void (*rx_nrp_reset)(struct ath12k_pdev_dp *dp_pdev);
	void (*mon_rx_wmask)(void *ptr, struct htt_rx_ring_tlv_filter *tlv_filter);
	void (*rx_enable_packet_filters)(void *ptr,
						struct htt_rx_ring_tlv_filter *filter);
	void (*pktlog_config)(struct ath12k_pdev_dp *dp_pdev,
			      enum ath12k_pktlog_mode mode,
			      u32 filter, bool enable);
};

struct ath12k_dp_mon {
	struct ath12k_dp *dp;
	struct dp_rxdma_mon_ring rxdma_mon_buf_ring;
	struct dp_rxdma_mon_ring tx_mon_buf_ring;
	struct dp_rxdma_mon_ring rx_mon_status_refill_ring[MAX_RXDMA_PER_PDEV];
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	u32 mon_dest_ring_stuck_cnt;
	struct ath12k_dp_mon_desc *mon_desc_pool;
	struct list_head mon_desc_free_list;

	/* lock for ath12k_dp_mon_desc */
	spinlock_t mon_desc_lock;
	struct page_frag_cache rx_mon_pf_cache;

	u32 num_frag_replenish;
	u32 num_frag_free;
};

enum dp_monitor_type {
	ATH12K_DP_MON_TYPE_QUAD_RING,
	ATH12K_DP_MON_TYPE_DUAL_RING
};

struct ath12k_dp_mon_mpdu_meta {
	u8 decap_type;
	u8 truncated:1;
	u32 err_bitmap;
};

enum dp_mon_tx_ppdu_info_type {
	DP_MON_TX_PROT_PPDU_INFO,
	DP_MON_TX_DATA_PPDU_INFO
};

enum dp_mon_tx_medium_protection_type {
	DP_MON_TX_MEDIUM_NO_PROTECTION,
	DP_MON_TX_MEDIUM_RTS_LEGACY,
	DP_MON_TX_MEDIUM_RTS_11AC_STATIC_BW,
	DP_MON_TX_MEDIUM_RTS_11AC_DYNAMIC_BW,
	DP_MON_TX_MEDIUM_CTS2SELF,
	DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_3ADDR,
	DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_4ADDR
};

enum dp_mon_status_buf_state {
	DP_MON_STATUS_MATCH,
	DP_MON_STATUS_NO_DMA,
	DP_MON_STATUS_LAG,
	DP_MON_STATUS_LEAD,
	DP_MON_STATUS_REPLINISH,
};

struct dp_mon_qosframe_addr4 {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	__le16 seq_ctrl;
	u8 addr4[ETH_ALEN];
	__le16 qos_ctrl;
} __packed;

struct dp_mon_frame_min_one {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
} __packed;

struct dp_mon_packet_info {
	u64 cookie;
	u32 dma_length:12,
	    rsvd1:4,
	    msdu_continuation:1,
	    truncated:1,
	    rsvd2:14;
	u32 rsvd3;
};

struct dp_mon_mpdu {
	struct list_head list;
	struct sk_buff *head;
	struct sk_buff *tail;
	u32 err_bitmap;
	u8 decap_format;
};

struct dp_mon_tx_ppdu_info {
	bool is_used;
	struct hal_tx_mon_ppdu_info tx_info;
	struct list_head dp_tx_mon_mpdu_list;
	struct dp_mon_mpdu *tx_mon_mpdu;
};

#define SNR_INVALID 255

#define SNR_MULTIPLIER BIT(8)
#define SNR_MUL(x, mul) ((x) * (mul))
#define SNR_RND(x, mul) ((((x) % (mul)) >= ((mul) / 2)) ? \
			 ((x) + ((mul) - 1)) / (mul) : (x) / (mul))

#define SNR_OUT(x) (SNR_RND((x), SNR_MULTIPLIER))
#define SNR_IN(x)  (SNR_MUL((x), SNR_MULTIPLIER))
#define SNR_AVG(x, y) ((((x) << 2) + (y) - (x)) >> 2)
#define SNR_UPDATE_AVG(x, y) ((x) = SNR_AVG((x), SNR_IN(y)))

struct ath12k_pdev_mon_stats {
	u32 status_ppdu_state;
	u32 status_ppdu_start;
	u32 status_ppdu_end;
	u32 status_ppdu_compl;
	u32 status_ppdu_start_mis;
	u32 status_ppdu_end_mis;
	u32 status_ppdu_done;
	u32 dest_ppdu_done;
	u32 dest_mpdu_done;
	u32 dest_mpdu_drop;
	u32 dup_mon_linkdesc_cnt;
	u32 dup_mon_buf_cnt;
	u32 dest_mon_stuck;
	u32 dest_mon_not_reaped;
};

#define DP_MON_MAX_STATUS_BUF 32

struct ath12k_mon_data {
	struct dp_link_desc_bank link_desc_banks[DP_LINK_DESC_BANKS_MAX];
	struct hal_rx_mon_ppdu_info mon_ppdu_info;

	u32 mon_ppdu_status;
	u32 mon_last_buf_cookie;
	u64 mon_last_linkdesc_paddr;
	u16 chan_noise_floor;
	u32 err_bitmap;
	u8 decap_format;

	struct ath12k_pdev_mon_stats rx_mon_stats;
	enum dp_mon_status_buf_state buf_state;
	/* lock for monitor data */
	spinlock_t mon_lock;
	struct sk_buff_head rx_status_q;
	struct dp_mon_mpdu *mon_mpdu;
	struct list_head dp_rx_mon_mpdu_list;
	struct dp_mon_tx_ppdu_info *tx_prot_ppdu_info;
	struct dp_mon_tx_ppdu_info *tx_data_ppdu_info;
};

struct ath12k_pdev_mon_dp_stats {
	u32 status_buf_reaped;
	u32 status_buf_processed;
	u32 status_buf_free;

	u32 ring_desc_empty;
	u32 ring_desc_flush;
	u32 ring_desc_trunc;

	u32 pkt_tlv_processed;
	u32 pkt_tlv_free;
	u32 pkt_tlv_to_mac80211;
	u32 pkt_tlv_truncated;

	u32 num_skb_alloc;
	u32 num_skb_free;
	u32 num_skb_to_mac80211;

	u32 num_ppdu_reaped;
	u32 num_ppdu_processed;

	u32 num_skb_raw;
	u32 num_frag_raw;

	u32 num_skb_eth;
	u32 num_frag_eth;

	u32 drop_tlv;

	u32 ppdu_desc_used;
	u32 ppdu_desc_proc;
	u32 ppdu_desc_free;

	u32 ppdu_desc_free_list_empty_cnt;
	u32 restitch_insuff_frags_cnt;
};

struct ath12k_pdev_mon_dp {
	struct ath12k_dp_mon *dp_mon;
	struct ath12k_pdev_dp *dp_pdev;
	struct dp_srng rxdma_mon_dst_ring[MAX_RXDMA_PER_PDEV];
	struct dp_srng tx_mon_dst_ring[MAX_RXDMA_PER_PDEV];

	struct ieee80211_rx_status rx_status;
	struct ath12k_mon_data mon_data;
	struct dp_mon_rx_filter **rx_filter;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc_pool;
	struct list_head ppdu_desc_used_list;
	struct list_head ppdu_desc_free_list;
	struct list_head ppdu_desc_proc_list;

	/* lock for ath12k_dp_mon_ppdu_desc */
	spinlock_t ppdu_desc_lock;
	struct list_head mon_desc_used_list;
	struct ath12k_pdev_mon_dp_stats mon_stats;

	struct work_struct rxmon_work;
	struct workqueue_struct *rxmon_wq;
};

enum ath12k_dp_mon_desc_in_use {
	DP_MON_DESC_REPLENISH = 1,
	DP_MON_DESC_STATUS_REAP,
	DP_MON_DESC_PACKET_REAP,
	DP_MON_DESC_H_PROC_ERR,
	DP_MON_DESC_TO_HW,
	DP_MON_DESC_H_REPLENISH_ERR,
};

struct ath12k_dp_mon_desc {
	struct list_head list;
	u8 *mon_buf;
	dma_addr_t paddr;
	u32 magic;
	u16 buf_len;
	u8 in_use;
	u8 end_of_ppdu;
};

static inline enum dp_monitor_type
ath12k_dp_get_mon_type(struct ath12k_dp *dp)
{
	return ((dp->hw_params->rxdma1_enable) ? ATH12K_DP_MON_TYPE_DUAL_RING :
				ATH12K_DP_MON_TYPE_QUAD_RING);
}

static inline
const struct ath12k_dp_arch_mon_ops *ath12k_dp_mon_ops_get(struct ath12k_dp *dp)
{
	if (dp && dp->dp_mon)
		return dp->dp_mon->mon_ops;

	return NULL;
}

int ath12k_dp_mon_buf_replenish(struct ath12k_dp *dp,
				struct dp_rxdma_mon_ring *buf_ring,
				struct list_head *used_list,
				int req_entries);
struct sk_buff *ath12k_dp_mon_tx_alloc_skb(void);
enum hal_tx_mon_status
ath12k_dp_mon_tx_parse_mon_status(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_mon_data *pmon,
				  struct sk_buff *skb,
				  struct napi_struct *napi,
				  u32 ppdu_id);
void ath12k_dp_mon_rx_process_ulofdma_stats(struct hal_rx_mon_ppdu_info *ppdu_info);
int ath12k_dp_mon_rx_dual_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
				       struct napi_struct *napi, int *budget);
int ath12k_dp_get_peer_telemetry_stats(struct ath12k_base *ab,
                                      const u8 *peer_addr,
                                      struct ath12k_peer_telemetry_stats *stats);

int ath12k_dp_mon_pdev_update_telemetry_stats(struct ath12k_base *ab,
                                             int pdev_id);

void ath12k_dp_rxdma_mon_buf_ring_free(struct ath12k_dp *dp,
				       struct dp_rxdma_mon_ring *rx_ring);
int ath12k_dp_mon_rx_srng_setup(struct ath12k_dp *dp);
void ath12k_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp);
int ath12k_dp_mon_rx_buf_setup(struct ath12k_dp *dp);
void ath12k_dp_mon_rx_buf_free(struct ath12k_dp *dp);
int ath12k_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp);
int ath12k_dp_mon_pdev_alloc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_free(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev,
				     u32 mac_id);
void ath12k_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev,
					 u32 mac_id);
void ath12k_dp_mon_pdev_rx_attach(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_rx_mpdu_list_init(struct ath12k_mon_data *pmon);
void ath12k_dp_rx_mon_dest_process(struct ath12k *ar, int mac_id,
				   u32 quota, struct napi_struct *napi);
int ath12k_dp_mon_rx_set_pktlen(struct sk_buff *skb, u32 len);
void ath12k_dp_mon_rx_update_peer_su_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct ath12k_dp_link_peer *peer,
					   struct hal_rx_mon_ppdu_info *ppdu_info);
void ath12k_dp_mon_rx_update_peer_mu_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct hal_rx_mon_ppdu_info *ppdu_info);
void ath12k_dp_mon_ppdu_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
				       struct hal_rx_mon_ppdu_info *ppdu_info,
				       bool is_stat);
void ath12k_dp_mon_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_mon_ppdu_info *ppdu_info);
void ath12k_dp_mon_rx_stats_enable(struct ath12k_pdev_dp *dp_pdev,
				   enum dp_mon_stats_mode mode);
void ath12k_dp_mon_rx_stats_disable(struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_stats_mode mode);
void ath12k_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_nrp_set(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_nrp_reset(struct ath12k_pdev_dp *dp_pdev);
size_t ath12k_dp_mon_list_cut_nodes(struct list_head *list, struct list_head *head,
				    size_t count);
size_t ath12k_dp_mon_get_req_entries_from_buf_ring(struct ath12k_dp *dp,
						   struct dp_rxdma_mon_ring *rx_ring,
						   struct list_head *list);
void ath12k_dp_mon_rx_deliver_skb(struct ath12k_pdev_dp *dp_pdev,
				  struct napi_struct *napi, struct sk_buff *msdu,
				  struct ieee80211_rx_status *status,
				  struct hal_rx_mon_ppdu_info *ppduinfo);
void ath12k_dp_mon_fill_rx_stats_info(struct hal_rx_mon_ppdu_info *ppdu_info,
				      struct ieee80211_rx_status *rx_status);
void ath12k_dp_mon_update_radiotap(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_rx_mon_ppdu_info *ppduinfo,
				   struct sk_buff *mon_skb,
				   struct ieee80211_rx_status *rxs);
struct sk_buff *ath12k_dp_mon_get_skb_valid_frag(struct ath12k_dp *dp,
						 struct sk_buff *skb);
void ath12k_dp_mon_add_frag_list(struct sk_buff *skb_head, struct sk_buff *frag_list,
				 u32 frag_len);
void ath12k_dp_mon_update_skb_len(struct sk_buff *skb_head, u32 frag_len);
void ath12k_dp_mon_append_skb(struct sk_buff *skb, struct sk_buff *tmp_skb);
void ath12k_dp_mon_skb_remove_frag(struct ath12k_dp *dp, struct sk_buff *skb,
				   u16 idx, u16 truesize);
void ath12k_dp_mon_add_rx_frag(struct sk_buff *skb, const void *mon_buf,
			       int offset, int frag_len, bool take_frag_ref);
void ath12k_dp_mon_rx_process_low_thres(struct ath12k_dp *dp);
void
ath12k_dp_mon_cnt_skb_and_frags(struct sk_buff *skb, u32 *skb_count, u32 *frag_count);
void ath12k_dp_mon_pktlog_config_filter(struct ath12k_pdev_dp *dp_pdev,
					enum ath12k_pktlog_mode mode,
					u32 filter, bool enable);
u32 ath12k_wifi7_dp_mon_get_frag_size_by_idx(struct ath12k_dp *dp, struct sk_buff *skb,
					     u8 idx);
void *ath12k_dp_mon_skb_get_frag_addr(struct sk_buff *skb, u8 idx);
int ath12k_dp_mon_adj_frag_offset(struct sk_buff *skb, u8 idx, int offset);
u32 ath12k_dp_mon_get_num_frags_in_fraglist(struct sk_buff *skb);
u64 ath12k_get_timestamp_in_us(void);
void ath12k_dp_mon_fill_rx_rate(struct ath12k_pdev_dp *dp_pdev,
				struct hal_rx_mon_ppdu_info *ppdu_info,
				struct ieee80211_rx_status *rx_status);
static inline
int ath12k_dp_mon_rx_alloc(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (unlikely(!dp || !dp->dp_mon))
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_srng_setup) {
		ret = mon_ops->rx_srng_setup(dp);
		if (ret)
			return ret;
	}

	if (mon_ops && mon_ops->rx_buf_setup) {
		ret = mon_ops->rx_buf_setup(dp);
		if (ret)
			return ret;
	}

	return 0;
}

static inline
void ath12k_dp_mon_rx_free(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_srng_cleanup)
		mon_ops->rx_srng_cleanup(dp);

	if (mon_ops && mon_ops->rx_buf_free)
		mon_ops->rx_buf_free(dp);
}

static inline
int ath12k_dp_mon_rx_htt_setup(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret = 0;

	if (unlikely(!dp || !dp->dp_mon))
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_htt_srng_setup)
		ret = mon_ops->rx_htt_srng_setup(dp);

	return ret;

}

static inline
int ath12k_dp_mon_pdev_init(struct ath12k_pdev_dp *dp_pdev)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp *dp;
	int ret = 0;

	if (unlikely(!dp_pdev))
		return -EINVAL;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_pdev_alloc)
		ret = mon_ops->mon_pdev_alloc(dp_pdev);

	return ret;
}

static inline
void ath12k_dp_mon_pdev_deinit(struct ath12k_pdev_dp *dp_pdev)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp *dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_pdev_free)
		mon_ops->mon_pdev_free(dp_pdev);
}

static inline
int ath12k_dp_mon_pdev_rx_alloc(struct ath12k_pdev_dp *dp_pdev,
				u32 mac_id)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return -EINVAL;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_warn(dp, "mon ops is NULL\n");
		return -EINVAL;
	}

	if (mon_ops->mon_pdev_rx_srng_setup) {
		ret = mon_ops->mon_pdev_rx_srng_setup(dp_pdev,
						      mac_id);
		if (ret) {
			ath12k_warn(dp, "failed to setup HAL_RXDMA_MONITOR_DST\n");
			return -ENOMEM;
		}
	}

	if (mon_ops->setup_ppdu_desc) {
		ret = mon_ops->setup_ppdu_desc(dp_pdev);
		if (ret) {
			ath12k_warn(dp, "failed to setup ppdu desc ret = %d\n",
				    ret);
			return ret;
		}
	}

	if (mon_ops->mon_rx_wq_init) {
		ret = mon_ops->mon_rx_wq_init(dp_pdev);
		if (ret) {
			ath12k_warn(dp,
				    "failed to init mon workqueue for pdev_id %d\n",
				    mac_id);
			goto cleanup;
		}
	}

	return 0;

cleanup:
	if (mon_ops && mon_ops->cleanup_ppdu_desc)
		mon_ops->cleanup_ppdu_desc(dp_pdev);

	if (mon_ops && mon_ops->mon_pdev_rx_srng_cleanup)
		mon_ops->mon_pdev_rx_srng_cleanup(dp_pdev);

	return ret;
}

static inline
int ath12k_dp_mon_pdev_rx_htt_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return -EINVAL;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops->mon_pdev_rx_htt_srng_setup) {
		ret = mon_ops->mon_pdev_rx_htt_srng_setup(dp_pdev,
							  mac_id);
		if (ret) {
			ath12k_warn(dp, "htt setup failed for HAL_RXDMA_MONITOR_DST\n");
			return ret;
		}
	}

	if (mon_ops->mon_pdev_rx_attach)
		mon_ops->mon_pdev_rx_attach(dp_pdev);

	if (mon_ops->rx_filter_alloc) {
		ret = mon_ops->rx_filter_alloc(dp_pdev);
		if (ret) {
			ath12k_warn(dp, "failed to setup monitor rx filter ret = %d\n",
				    ret);
		}
	}

	return 0;
}

static inline
void ath12k_dp_mon_pdev_rx_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	dp = dp_pdev->dp;
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_warn(dp, "mon ops is NULL during mon pdev free\n");
		return;
	}

	if (mon_ops->mon_rx_wq_deinit)
		mon_ops->mon_rx_wq_deinit(dp_pdev);

	if (mon_ops->rx_filter_free)
		mon_ops->rx_filter_free(dp_pdev);

	if (mon_ops && mon_ops->cleanup_ppdu_desc)
		mon_ops->cleanup_ppdu_desc(dp_pdev);

	if (mon_ops && mon_ops->mon_pdev_rx_srng_cleanup)
		mon_ops->mon_pdev_rx_srng_cleanup(dp_pdev);
}

static inline
void ath12k_dp_mon_update_telemetry_stats(struct ath12k_base *ab,
					  const int pdev_id)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->update_telemetry_stats)
		mon_ops->update_telemetry_stats(ab, pdev_id);
}

static inline
void ath12k_dp_mon_rx_stats_config(struct ath12k *ar, bool enable,
				  enum dp_mon_stats_mode mode)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (enable) {
		if(mon_ops && mon_ops->rx_stats_enable)
			mon_ops->rx_stats_enable(dp_pdev, mode);
	} else {
		if(mon_ops && mon_ops->rx_stats_disable)
			mon_ops->rx_stats_disable(dp_pdev, mode);
	}
}

static inline
int ath12k_dp_mon_rx_update_filter(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	int ret;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_filter_update) {
		ret = mon_ops->rx_filter_update(dp_pdev);
		if (ret)
			return ret;
	}

	return 0;
}

static inline
void ath12k_dp_mon_rx_config_monitor_mode(struct ath12k *ar, bool reset)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!reset) {
		if(mon_ops && mon_ops->rx_monitor_mode_set)
			mon_ops->rx_monitor_mode_set(dp_pdev);
	} else {
		if(mon_ops && mon_ops->rx_monitor_mode_reset)
			mon_ops->rx_monitor_mode_reset(dp_pdev);
	}

}

static inline
void ath12k_dp_mon_rx_nrp_config(struct ath12k *ar, bool reset)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!reset) {
		if (mon_ops && mon_ops->rx_nrp_set)
			mon_ops->rx_nrp_set(dp_pdev);
	} else {
		if (mon_ops && mon_ops->rx_nrp_reset)
			mon_ops->rx_nrp_reset(dp_pdev);
	}

}

static inline
void ath12k_dp_mon_rx_config_wmask(struct ath12k_dp *dp, void *ptr,
				   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);
	if (mon_ops && mon_ops->mon_rx_wmask)
		mon_ops->mon_rx_wmask(ptr, tlv_filter);
}

static inline void
ath12k_dp_mon_rx_config_packet_type_subtype(struct ath12k_dp *dp, void *ptr,
					    struct htt_rx_ring_tlv_filter *tlv_filter)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp || !dp->dp_mon))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->rx_enable_packet_filters)
		mon_ops->rx_enable_packet_filters(ptr, tlv_filter);
}

static inline void
ath12k_dp_mon_pktlog_config(struct ath12k *ar, bool enable,
			    enum ath12k_pktlog_mode mode,
			    u32 filter)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if(mon_ops && mon_ops->pktlog_config)
		mon_ops->pktlog_config(dp_pdev, mode, filter, enable);
}

static inline void
ath12k_dp_mon_desc_reset(struct ath12k_dp_mon_desc *desc)
{
	memset((u8 *)desc + sizeof(desc->list), 0, sizeof(*desc) - sizeof(desc->list));
}
#endif
