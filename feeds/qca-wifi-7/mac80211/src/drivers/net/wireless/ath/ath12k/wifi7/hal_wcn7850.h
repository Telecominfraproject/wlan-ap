/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "../hal.h"
#include "hal_tx.h"
#include "hal_rx.h"

extern const struct hal_ops hal_wcn7850_ops;

void
ath12k_wifi7_hal_rx_desc_get_crypto_header_wcn7850(struct hal_rx_desc *desc,
						   u8 *crypto_hdr,
						   enum hal_encrypt_type encyp);
u32 ath12k_wifi7_hal_rx_h_mpdu_err_wcn7850(struct hal_rx_desc *desc);
void ath12k_wifi7_hal_rx_desc_get_dot11_hdr_wcn7850(struct hal_rx_desc *desc,
						    struct ieee80211_hdr *hdr);
void ath12k_wifi7_hal_extract_rx_desc_data_wcn7850(struct hal_rx_desc_data *rx_desc_data,
						   struct hal_rx_desc *rx_desc,
						   struct hal_rx_desc *ldesc);
void ath12k_wifi7_hal_extract_rx_spd_data_wcn7850(struct hal_rx_spd_data *rx_info,
						  struct hal_rx_desc *rx_desc, int set);
static inline
bool ath12k_wifi7_hal_wmask_compaction_rx_tlv_supported_wcn7850(void)
{
	return false;
}

static inline
bool ath12k_wifi7_hal_rx_h_first_msdu_wcn7850(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.wcn7850.msdu_end.info5,
			       RX_MSDU_END_INFO5_FIRST_MSDU);
}

static inline
bool ath12k_wifi7_hal_rx_h_last_msdu_wcn7850(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.wcn7850.msdu_end.info5,
			       RX_MSDU_END_INFO5_LAST_MSDU);
}

static inline
u8 ath12k_wifi7_hal_rx_h_l3pad_wcn7850(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.wcn7850.msdu_end.info5,
			    RX_MSDU_END_INFO5_L3_HDR_PADDING);
}

static inline
bool ath12k_wifi7_hal_rx_desc_encrypt_valid_wcn7850(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.wcn7850.mpdu_start.info4,
			       RX_MPDU_START_INFO4_ENCRYPT_INFO_VALID);
}

static inline
u32 ath12k_wifi7_hal_rx_h_enctype_wcn7850(struct hal_rx_desc *desc)
{
	if (!ath12k_wifi7_hal_rx_desc_encrypt_valid_wcn7850(desc))
		return HAL_ENCRYPT_TYPE_OPEN;

	return le32_get_bits(desc->u.wcn7850.mpdu_start.info2,
			     RX_MPDU_START_INFO2_ENC_TYPE);
}

static inline
u8 ath12k_wifi7_hal_rx_h_decap_type_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info11,
			     RX_MSDU_END_INFO11_DECAP_FORMAT);
}

static inline
u8 ath12k_wifi7_hal_rx_h_mesh_ctl_present_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info11,
			     RX_MSDU_END_INFO11_MESH_CTRL_PRESENT);
}

static inline
bool ath12k_wifi7_hal_rx_h_seq_ctrl_valid_wcn7850(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.wcn7850.mpdu_start.info4,
			       RX_MPDU_START_INFO4_MPDU_SEQ_CTRL_VALID);
}

static inline
bool ath12k_wifi7_hal_rx_h_fc_valid_wcn7850(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.wcn7850.mpdu_start.info4,
			       RX_MPDU_START_INFO4_MPDU_FCTRL_VALID);
}

static inline
u16 ath12k_wifi7_hal_rx_h_seq_no_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.mpdu_start.info4,
			     RX_MPDU_START_INFO4_MPDU_SEQ_NUM);
}

static inline
u16 ath12k_wifi7_hal_rx_h_msdu_len_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info10,
			     RX_MSDU_END_INFO10_MSDU_LENGTH);
}

static inline u8 ath12k_wifi7_hal_rx_h_sgi_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info12,
			     RX_MSDU_END_INFO12_SGI);
}

static inline
u8 ath12k_wifi7_hal_rx_h_rate_mcs_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info12,
			     RX_MSDU_END_INFO12_RATE_MCS);
}

static inline u8 ath12k_wifi7_hal_rx_h_rx_bw_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info12,
			     RX_MSDU_END_INFO12_RECV_BW);
}

static inline u32 ath12k_wifi7_hal_rx_h_freq_wcn7850(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.wcn7850.msdu_end.phy_meta_data);
}

static inline
u8 ath12k_wifi7_hal_rx_h_pkt_type_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info12,
			     RX_MSDU_END_INFO12_PKT_TYPE);
}

static inline u8 ath12k_wifi7_hal_rx_h_nss_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.msdu_end.info12,
			     RX_MSDU_END_INFO12_MIMO_SS_BITMAP);
}

static inline u8 ath12k_wifi7_hal_rx_h_tid_wcn7850(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.wcn7850.mpdu_start.info2,
			     RX_MPDU_START_INFO2_TID);
}

static inline
u16 ath12k_wifi7_hal_rx_h_peer_id_wcn7850(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.wcn7850.mpdu_start.sw_peer_id);
}

static inline
void ath12k_wifi7_hal_rx_desc_end_tlv_copy_wcn7850(struct hal_rx_desc *fdesc,
						   struct hal_rx_desc *ldesc)
{
	memcpy(&fdesc->u.wcn7850.msdu_end, &ldesc->u.wcn7850.msdu_end,
	       sizeof(struct rx_msdu_end_qcn9274));
}

static inline
u32 ath12k_wifi7_hal_rx_desc_get_mpdu_start_tag_wcn7850(struct hal_rx_desc *desc)
{
	return le64_get_bits(desc->u.wcn7850.mpdu_start_tag,
			    HAL_TLV_HDR_TAG);
}

static inline
u32 ath12k_wifi7_hal_rx_desc_get_mpdu_ppdu_id_wcn7850(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.wcn7850.mpdu_start.phy_ppdu_id);
}

static inline void
ath12k_wifi7_hal_rxdesc_set_msdu_len_wcn7850(struct hal_rx_desc *desc, u16 len)
{
	u32 info = __le32_to_cpu(desc->u.wcn7850.msdu_end.info10);

	info &= ~RX_MSDU_END_INFO10_MSDU_LENGTH;
	info |= u32_encode_bits(len, RX_MSDU_END_INFO10_MSDU_LENGTH);

	desc->u.wcn7850.msdu_end.info10 = __cpu_to_le32(info);
}

static inline u8 *
ath12k_wifi7_hal_rx_desc_get_msdu_payload_wcn7850(struct hal_rx_desc *desc)
{
	return &desc->u.wcn7850.msdu_payload[0];
}

static inline u32 ath12k_wifi7_hal_rx_desc_get_mpdu_start_offset_wcn7850(void)
{
	return offsetof(struct hal_rx_desc_wcn7850, mpdu_start_tag);
}

static inline u32 ath12k_wifi7_hal_rx_desc_get_msdu_end_offset_wcn7850(void)
{
	return offsetof(struct hal_rx_desc_wcn7850, msdu_end_tag);
}

static inline
bool ath12k_wifi7_hal_rxdesc_mac_addr2_valid_wcn7850(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.wcn7850.mpdu_start.info4) &
	       RX_MPDU_START_INFO4_MAC_ADDR2_VALID;
}

static inline u8 *
ath12k_wifi7_hal_rxdesc_get_mpdu_start_addr2_wcn7850(struct hal_rx_desc *desc)
{
	return ath12k_wifi7_hal_rxdesc_mac_addr2_valid_wcn7850(desc) ?
			desc->u.wcn7850.mpdu_start.addr2 : NULL;
}

static inline
bool ath12k_wifi7_hal_rx_h_is_da_mcbc_wcn7850(struct hal_rx_desc *desc)
{
	return (ath12k_wifi7_hal_rx_h_first_msdu_wcn7850(desc) &&
		(__le32_to_cpu(desc->u.wcn7850.msdu_end.info13) &
	       RX_MSDU_END_INFO13_MCAST_BCAST));
}

static inline u16
ath12k_wifi7_hal_rxdesc_get_mpdu_frame_ctrl_wcn7850(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.wcn7850.mpdu_start.frame_ctrl);
}

static inline
bool ath12k_wifi7_hal_rx_h_msdu_done_wcn7850(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.wcn7850.msdu_end.info14,
			       RX_MSDU_END_INFO14_MSDU_DONE);
}

static inline
bool ath12k_wifi7_hal_rx_h_l4_cksum_fail_wcn7850(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.wcn7850.msdu_end.info13,
			       RX_MSDU_END_INFO13_TCP_UDP_CKSUM_FAIL);
}

static inline
bool ath12k_wifi7_hal_rx_h_ip_cksum_fail_wcn7850(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.wcn7850.msdu_end.info13,
			      RX_MSDU_END_INFO13_IP_CKSUM_FAIL);
}

static inline
bool ath12k_wifi7_hal_rx_h_is_decrypted_wcn7850(struct hal_rx_desc *desc)
{
	return (le32_get_bits(desc->u.wcn7850.msdu_end.info14,
			      RX_MSDU_END_INFO14_DECRYPT_STATUS_CODE) ==
			      RX_DESC_DECRYPT_STATUS_CODE_OK);
}

static inline u32 ath12k_wifi7_hal_get_rx_desc_size_wcn7850(void)
{
	return sizeof(struct hal_rx_desc_wcn7850);
}

static inline
u8 ath12k_wifi7_hal_rx_get_msdu_src_link_wcn7850(struct hal_rx_desc *desc)
{
	return 0;
}
