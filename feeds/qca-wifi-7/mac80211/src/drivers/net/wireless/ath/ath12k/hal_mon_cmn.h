/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef HAL_MON_CMN_H
#define HAL_MON_CMN_H

#include "hw.h"

#define HAL_MON_INVALID_PEERID	0x3fff
#define HAL_RX_MON_MAX_AGGR_SIZE	128
#define HAL_RX_MAX_MPDU				256
#define HAL_RX_NUM_WORDS_PER_PPDU_BITMAP	(HAL_RX_MAX_MPDU >> 5)
#define EHT_MAX_USER_INFO	4
#define HAL_MAX_UL_MU_USERS	37
#define HAL_RX_MON_FCS_LEN	4

#define HAL_RX_MON_MPDU_ERR_FCS			BIT(0)
#define HAL_RX_MON_MPDU_ERR_DECRYPT		BIT(1)
#define HAL_RX_MON_MPDU_ERR_TKIP_MIC		BIT(2)
#define HAL_RX_MON_MPDU_ERR_AMSDU_ERR		BIT(3)
#define HAL_RX_MON_MPDU_ERR_OVERFLOW		BIT(4)
#define HAL_RX_MON_MPDU_ERR_MSDU_LEN		BIT(5)
#define HAL_RX_MON_MPDU_ERR_MPDU_LEN		BIT(6)
#define HAL_RX_MON_MPDU_ERR_UNENCRYPTED_FRAME	BIT(7)

#define HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VALID		BIT(30)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VER		BIT(31)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_NSS		GENMASK(2, 0)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_MCS		GENMASK(6, 3)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_LDPC		BIT(7)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_DCM		BIT(8)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_START	GENMASK(15, 9)
#define HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_SIZE		GENMASK(18, 16)

#define ATH12K_LE32_DEC_ENC(value, dec_bits, enc_bits)	\
		u32_encode_bits(le32_get_bits(value, dec_bits), enc_bits)

#define ATH12K_LE64_DEC_ENC(value, dec_bits, enc_bits) \
		u32_encode_bits(le64_get_bits(value, dec_bits), enc_bits)

struct hal_rx_u_sig_info {
	bool ul_dl;
	u8 bw;
	u8 ppdu_type_comp_mode;
	u8 eht_sig_mcs;
	u8 num_eht_sig_sym;
	struct ieee80211_radiotap_eht_usig usig;
};

struct hal_rx_tlv_aggr_info {
	bool in_progress;
	u16 cur_len;
	u16 tlv_tag;
	u8 buf[HAL_RX_MON_MAX_AGGR_SIZE];
};

struct hal_rx_user_status {
	u32 mcs:4,
	nss:3,
	ofdma_info_valid:1,
	ul_ofdma_ru_start_index:7,
	ul_ofdma_ru_width:7,
	ul_ofdma_ru_size:8;
	u32 ul_ofdma_user_v0_word0;
	u32 ul_ofdma_user_v0_word1;
	u16 ast_index; // End User Stat
	u16 sw_peer_id;// mpdu start
	u16 tid;
	u16 tcp_msdu_count;
	u16 tcp_ack_msdu_count;
	u16 udp_msdu_count;
	u16 other_msdu_count;
	u8 frame_control;
	u8 frame_control_info_valid:1,
	data_sequence_control_info_valid:1;
	u16 first_data_seq_ctrl;
	u8 preamble_type;
	u16 ht_flags;
	u16 vht_flags;
	u16 he_flags;
	u8 rs_flags;
	u8 ldpc;
	u16 mpdu_cnt_fcs_ok;
	u16 mpdu_cnt_fcs_err;
	u32 mpdu_ok_byte_count;
	u32 mpdu_err_byte_count;
	bool ampdu_present;
	u16 ampdu_id;
	u32 errmap;
	u32 mpdu_retry;
	u8 filter_category;
	u8 enc_type;
	u16 retried_msdu_count;
};

struct hal_rx_eht_info {
	u8 num_user_info;
	struct hal_rx_radiotap_eht eht;
	u32 user_info[EHT_MAX_USER_INFO];
};

struct hal_rx_mon_mpdu_info {
	u8  decap_type:3,
	    mpdu_start_received:1,
	    first_rx_hdr_rcvd:1,
	    rx_hdr_rcvd:1,
	    raw_mpdu:1,
	    truncated:1;
	u32 err_bitmap;
};

struct hal_rx_nrp_info {
	uint32_t fc_valid : 1,
		 frame_control : 16,
		 to_ds_flag : 1,
		 mac_addr2_valid : 1,
		 mcast_bcast : 1;
	uint8_t mac_addr2[ETH_ALEN];
};

struct hal_rx_mon_msdu_info {
	u32 first_buffer:1,
	    last_buffer:1;
};

struct hal_rx_mon_ppdu_info {
	u16 ppdu_id;
	u16 last_ppdu_id;
	u64 ppdu_ts;
	u16 num_mpdu_fcs_ok;
	u16 num_mpdu_fcs_err;
	u8 preamble_type;
	u32 mpdu_len;
	u16 chan_num;
	u16 freq;
	u16 tcp_msdu_count;
	u16 tcp_ack_msdu_count;
	u16 udp_msdu_count;
	u16 other_msdu_count;
	u16 peer_id;
	u8 rate;
	u8 mcs;
	u8 nss;
	u8 bw;
	u8 vht_flag_values1;
	u8 vht_flag_values2;
	u8 vht_flag_values3[4];
	u8 vht_flag_values4;
	u8 vht_flag_values5;
	u16 vht_flag_values6;
	u8 is_stbc;
	u8 gi;
	u8 sgi;
	u8 ldpc;
	u8 beamformed;
	s8 rssi_comb;
	u16 tid;
	u8 fc_valid;
	u16 ht_flags;
	u16 vht_flags;
	u16 he_flags;
	u16 he_mu_flags;
	u8 dcm;
	u8 ru_alloc;
	u8 reception_type;
	u64 tsft;
	u64 rx_duration;
	u8 frame_control;
	u16 ast_index;
	u8 rs_fcs_err;
	u8 rs_flags;
	u8 cck_flag;
	u8 ofdm_flag;
	u8 ulofdma_flag;
	u8 frame_control_info_valid;
	u16 he_per_user_1;
	u16 he_per_user_2;
	u8 he_per_user_position;
	u8 he_per_user_known;
	u16 he_flags1;
	u16 he_flags2;
	u8 he_RU[4];
	u16 he_data1;
	u16 he_data2;
	u16 he_data3;
	u16 he_data4;
	u16 he_data5;
	u16 he_data6;
	u32 ppdu_len;
	u16 prev_ppdu_id;
	u32 device_id;
	u16 first_data_seq_ctrl;
	u8 monitor_direct_used;
	u8 data_sequence_control_info_valid;
	u8 ltf_size;
	u8 rxpcu_filter_pass;
	s8 rssi_chain[8][8];
	u8 num_users;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	u8 addr4[ETH_ALEN];
	struct hal_rx_user_status userstats[HAL_MAX_UL_MU_USERS];
	u8 userid;
	bool first_msdu_in_mpdu;
	bool is_ampdu;
	u8 medium_prot_type;
	bool ppdu_continuation;
	bool eht_usig;
	u8 usr_nss_sum;
	u16 usr_ru_tones_sum;
	struct hal_rx_u_sig_info u_sig_info;
	bool is_eht;
	struct hal_rx_eht_info eht_info;
	struct hal_rx_tlv_aggr_info tlv_aggr;
	struct hal_rx_nrp_info nrp_info;
	u32 errmap;
	u32 mpdu_retry;
	u8 grp_id;
	u8 decap_format;
	u16 mpdu_retry_cnt;
	struct hal_rx_mon_mpdu_info mpdu_info[HAL_MAX_UL_MU_USERS];
	struct sk_buff_head mpdu_q[HAL_MAX_UL_MU_USERS];
	bool is_drop_tlv;
	struct hal_rx_mon_msdu_info msdu_info[HAL_MAX_UL_MU_USERS];
	u8 user_id;
};

struct hal_rx_mon_status_tlv_hdr {
	u32 hdr;
	u8 value[];
};

enum hal_rx_mon_status {
	HAL_RX_MON_STATUS_PPDU_NOT_DONE,
	HAL_RX_MON_STATUS_PPDU_DONE,
	HAL_RX_MON_STATUS_BUF_DONE,
	HAL_RX_MON_STATUS_BUF_ADDR,
	HAL_RX_MON_STATUS_MPDU_START,
	HAL_RX_MON_STATUS_MPDU_END,
	HAL_RX_MON_STATUS_MSDU_END,
	HAL_RX_MON_STATUS_RX_HDR,
	HAL_RX_MON_STATUS_DROP_TLV,
};

enum hal_tx_mon_status {
	HAL_TX_MON_STATUS_PPDU_NOT_DONE,
	HAL_TX_MON_MPDU_START,
	HAL_TX_MON_MPDU_END,
	HAL_TX_MON_FES_SETUP,
	HAL_TX_MON_FES_STATUS_END,
	HAL_RX_MON_RESPONSE_REQUIRED_INFO,
	HAL_TX_MON_FES_STATUS_PROT,
	HAL_TX_MON_FRAME_BITMAP_ACK,
	HAL_TX_MON_MSDU_START,
	HAL_TX_MON_RESPONSE_END_STATUS_INFO,
	HAL_TX_MON_BUFFER_ADDR,
	HAL_TX_MON_DATA,
};

struct hal_tx_mon_ppdu_info {
	u32 ppdu_id;
	u8  num_users;
	struct hal_rx_mon_ppdu_info rx_status;
};

static inline u64 ath12k_hal_le32hilo_to_u64(__le32 hi, __le32 lo)
{
	u64 hi64 = le32_to_cpu(hi);
	u64 lo64 = le32_to_cpu(lo);

	return (hi64 << 32) | lo64;
}

struct hal_mon_ops {
	enum hal_tx_mon_status
	(*tx_parse_status_tlv)(struct hal_tx_mon_ppdu_info *ppdu_info,
			       u16 tlv_tag,
			       const void *tlv_data,
			       u32 userid);
	enum hal_tx_mon_status
	(*tx_status_get_num_user)(u16 tlv_tag,
				  const void *tlv,
				  u8 *num_users);
	u32 (*get_mon_mpdu_start_wmask)(void);
	u32 (*get_mon_mpdu_end_wmask)(void);
	u32 (*get_mon_msdu_end_wmask)(void);
	u32 (*get_mon_ppdu_end_usr_stats_wmask)(void);
	void (*rx_mpdu_start_info_get)(const void *tlv_data, u32 userid,
				       struct hal_rx_mon_ppdu_info *info,
				       u32 tlv_len);
	void (*rx_msdu_end_info_get)(const void *tlv_data, u32 userid,
				     struct hal_rx_mon_ppdu_info *info,
				     u32 tlv_len);
	void (*rx_ppdu_eu_stats_info_get)(const void *tlv_data, u32 userid,
					  struct hal_rx_mon_ppdu_info *info,
					  u32 tlv_len);
	u8* (*rx_desc_get_msdu_payload)(void *desc);
};

static inline enum hal_tx_mon_status
ath12k_hal_mon_tx_parse_status(struct ath12k_hal *hal,
			       struct hal_tx_mon_ppdu_info *ppdu_info,
			       u16 tlv_tag, const void *tlv_data, u32 userid)
{
	return hal->hal_mon_ops->tx_parse_status_tlv(ppdu_info,
						     tlv_tag,
						     tlv_data,
						     userid);
}

static inline enum hal_tx_mon_status
ath12k_hal_mon_tx_status_get_num_user(struct ath12k_hal *hal,
				      u16 tlv_tag,
				      const void *tlv,
				      u8 *num_users)
{
	return hal->hal_mon_ops->tx_status_get_num_user(tlv_tag,
							tlv,
							num_users);
}

static inline u32 ath12k_hal_mon_rx_mpdu_start_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_mpdu_start_wmask)
		return hal->hal_mon_ops->get_mon_mpdu_start_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_mpdu_end_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_mpdu_end_wmask)
		return hal->hal_mon_ops->get_mon_mpdu_end_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_msdu_end_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_msdu_end_wmask)
		return hal->hal_mon_ops->get_mon_msdu_end_wmask();

	return 0;
}

static inline u32 ath12k_hal_mon_rx_ppdu_end_usr_stats_wmask(struct ath12k_hal *hal)
{
	if (hal->hal_mon_ops->get_mon_ppdu_end_usr_stats_wmask)
		return hal->hal_mon_ops->get_mon_ppdu_end_usr_stats_wmask();

	return 0;
}

static inline
void ath12k_hal_mon_rx_mpdu_start_info_get(struct ath12k_hal *hal,
					   const void *tlv,
					   u32 userid,
					   struct hal_rx_mon_ppdu_info *info,
					   u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_mpdu_start_info_get)
		hal->hal_mon_ops->rx_mpdu_start_info_get(tlv,
							 userid,
							 info,
							 tlv_len);
}

static inline void
ath12k_hal_mon_rx_msdu_end_info_get(struct ath12k_hal *hal,
				    const void *tlv,
				    u32 userid,
				    struct hal_rx_mon_ppdu_info *info,
				    u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_msdu_end_info_get)
		hal->hal_mon_ops->rx_msdu_end_info_get(tlv,
						       userid,
						       info,
						       tlv_len);
}

static inline void
ath12k_hal_mon_rx_ppdu_end_usr_stats_info_get(struct ath12k_hal *hal,
					      const void *tlv,
					      u32 userid,
					      struct hal_rx_mon_ppdu_info *info,
					      u32 tlv_len)
{
	if (hal->hal_mon_ops->rx_ppdu_eu_stats_info_get)
		hal->hal_mon_ops->rx_ppdu_eu_stats_info_get(tlv,
							    userid,
							    info,
							    tlv_len);
}

static inline u8*
ath12k_hal_mon_rx_desc_get_msdu_payload(struct ath12k_hal *hal,
					void *rx_desc)
{
	if (hal->hal_mon_ops->rx_desc_get_msdu_payload)
		return hal->hal_mon_ops->rx_desc_get_msdu_payload(rx_desc);

	return NULL;
}
#endif
