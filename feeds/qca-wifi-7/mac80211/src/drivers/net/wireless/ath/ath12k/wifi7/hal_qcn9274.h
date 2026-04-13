/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include "../hal.h"
#include "hal_tx.h"
#include "hal_rx.h"

extern const struct hal_ops hal_qcn9274_ops;

u32 ath12k_wifi7_hal_rx_h_mpdu_err_qcn9274(struct hal_rx_desc *desc);
void
ath12k_wifi7_hal_rx_desc_get_crypto_header_qcn9274(struct hal_rx_desc *desc,
						   u8 *crypto_hdr,
						   enum hal_encrypt_type encyp);
void
ath12k_wifi7_hal_rx_desc_get_dot11_hdr_qcn9274(struct hal_rx_desc *desc,
					       struct ieee80211_hdr *hdr);
void ath12k_wifi7_hal_extract_rx_desc_data_qcn9274(struct hal_rx_desc_data *rx_desc_data,
						   struct hal_rx_desc *rx_desc,
						   struct hal_rx_desc *ldesc);
void ath12k_wifi7_hal_extract_rx_spd_data_qcn9274(struct hal_rx_spd_data *rx_info,
						  struct hal_rx_desc *rx_desc, int set);
static inline
bool ath12k_wifi7_hal_rx_h_first_msdu_qcn9274(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.qcn9274_compact.msdu_end.info5,
			       RX_MSDU_END_INFO5_FIRST_MSDU);
}

static inline
bool ath12k_wifi7_hal_rx_h_last_msdu_qcn9274(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.qcn9274_compact.msdu_end.info5,
			       RX_MSDU_END_INFO5_LAST_MSDU);
}

static inline
u8 ath12k_wifi7_hal_rx_h_l3pad_qcn9274(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9274_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_L3_HDR_PADDING);
}

static inline
bool ath12k_wifi7_hal_encrypt_valid_qcn9274(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9274_compact.mpdu_start.info4,
			       RX_MPDU_START_INFO4_ENCRYPT_INFO_VALID);
}

static inline
u8 ath12k_wifi7_hal_rx_h_decap_type_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info11,
			     RX_MSDU_END_INFO11_DECAP_FORMAT);
}

static inline
u8 ath12k_wifi7_hal_rx_h_mesh_ctl_present_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info11,
			     RX_MSDU_END_INFO11_MESH_CTRL_PRESENT);
}

static inline
bool ath12k_wifi7_hal_rx_h_seq_ctrl_valid_qcn9274(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9274_compact.mpdu_start.info4,
			       RX_MPDU_START_INFO4_MPDU_SEQ_CTRL_VALID);
}

static inline
bool ath12k_wifi7_hal_rx_h_fc_valid_qcn9274(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9274_compact.mpdu_start.info4,
			       RX_MPDU_START_INFO4_MPDU_FCTRL_VALID);
}

static inline
bool ath12k_wifi7_hal_rx_h_is_ip_valid_qcn9274(struct hal_rx_desc *desc)
{
	return !!(le32_get_bits(desc->u.qcn9274_compact.msdu_end.info11,
				RX_MSDU_END_INFO11_IPV4) ||
		  le32_get_bits(desc->u.qcn9274_compact.msdu_end.info11,
				RX_MSDU_END_INFO11_IPV6));
}

static inline
u16 ath12k_wifi7_hal_rx_h_seq_no_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.mpdu_start.info4,
			     RX_MPDU_START_INFO4_MPDU_SEQ_NUM);
}

static inline
u16 ath12k_wifi7_hal_rx_h_msdu_len_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info10,
			     RX_MSDU_END_INFO10_MSDU_LENGTH);
}

static inline
u8 ath12k_wifi7_hal_rx_h_sgi_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info12,
			     RX_MSDU_END_INFO12_SGI);
}

static inline
u8 ath12k_wifi7_hal_rx_h_rate_mcs_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info12,
			     RX_MSDU_END_INFO12_RATE_MCS);
}

static inline
u8 ath12k_wifi7_hal_rx_h_rx_bw_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info12,
			     RX_MSDU_END_INFO12_RECV_BW);
}

static inline
u32 ath12k_wifi7_hal_rx_h_freq_qcn9274(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9274_compact.msdu_end.phy_meta_data);
}

static inline
u8 ath12k_wifi7_hal_rx_h_pkt_type_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info12,
			     RX_MSDU_END_INFO12_PKT_TYPE);
}

static inline
u8 ath12k_wifi7_hal_rx_h_nss_qcn9274(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9274_compact.msdu_end.info12,
			     RX_MSDU_END_INFO12_MIMO_SS_BITMAP);
}

static inline
u8 ath12k_wifi7_hal_rx_h_tid_qcn9274(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9274_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_TID);
}

static inline
u8 ath12k_wifi7_hal_rx_h_from_ds_qcn9274(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9274_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_FROM_DS);
}

static inline
u8 ath12k_wifi7_hal_rx_h_to_ds_qcn9274(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9274_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_TO_DS);
}

static inline
u16 ath12k_wifi7_hal_rx_h_peer_id_qcn9274(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.qcn9274_compact.mpdu_start.sw_peer_id);
}

static inline
void ath12k_wifi7_hal_rx_desc_end_tlv_copy_qcn9274(struct hal_rx_desc *fdesc,
						   struct hal_rx_desc *ldesc)
{
	fdesc->u.qcn9274_compact.msdu_end = ldesc->u.qcn9274_compact.msdu_end;
}

static inline void
ath12k_wifi7_hal_rxdesc_set_msdu_len_qcn9274(struct hal_rx_desc *desc, u16 len)
{
	u32 info = __le32_to_cpu(desc->u.qcn9274_compact.msdu_end.info10);

	info = u32_replace_bits(info, len, RX_MSDU_END_INFO10_MSDU_LENGTH);
	desc->u.qcn9274_compact.msdu_end.info10 = __cpu_to_le32(info);
}

static inline
u8 *ath12k_wifi7_hal_rx_desc_get_msdu_payload_qcn9274(struct hal_rx_desc *desc)
{
	return &desc->u.qcn9274_compact.msdu_payload[0];
}

static inline
u32 ath12k_wifi7_hal_rx_desc_get_mpdu_start_offset_qcn9274(void)
{
	return offsetof(struct hal_rx_desc_qcn9274_compact, mpdu_start);
}

static inline
u32 ath12k_wifi7_hal_rx_desc_get_msdu_end_offset_qcn9274(void)
{
	return offsetof(struct hal_rx_desc_qcn9274_compact, msdu_end);
}

static inline
bool ath12k_wifi7_hal_rxdesc_mac_addr2_valid_qcn9274(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9274_compact.mpdu_start.info4) &
			     RX_MPDU_START_INFO4_MAC_ADDR2_VALID;
}

static inline u8 *
ath12k_wifi7_hal_rxdesc_get_mpdu_start_addr2_qcn9274(struct hal_rx_desc *desc)
{
	return ath12k_wifi7_hal_rxdesc_mac_addr2_valid_qcn9274(desc) ?
			desc->u.qcn9274_compact.mpdu_start.addr2 : NULL;
}

static inline
bool ath12k_wifi7_hal_rx_h_is_da_mcbc_qcn9274(struct hal_rx_desc *desc)
{
	return (ath12k_wifi7_hal_rx_h_first_msdu_qcn9274(desc) &&
		(__le16_to_cpu(desc->u.qcn9274_compact.msdu_end.info5) &
	       RX_MSDU_END_INFO5_DA_IS_MCBC));
}

static inline u16
ath12k_wifi7_hal_rxdesc_get_mpdu_frame_ctrl_qcn9274(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.qcn9274_compact.mpdu_start.frame_ctrl);
}

static inline
bool ath12k_wifi7_hal_rx_h_msdu_done_qcn9274(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9274_compact.msdu_end.info14,
			       RX_MSDU_END_INFO14_MSDU_DONE);
}

static inline
bool ath12k_wifi7_hal_rx_h_l4_cksum_fail_qcn9274(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9274_compact.msdu_end.info13,
			       RX_MSDU_END_INFO13_TCP_UDP_CKSUM_FAIL);
}

static inline
bool ath12k_wifi7_hal_rx_h_ip_cksum_fail_qcn9274(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9274_compact.msdu_end.info13,
			       RX_MSDU_END_INFO13_IP_CKSUM_FAIL);
}

static inline
bool ath12k_wifi7_hal_rx_h_is_decrypted_qcn9274(struct hal_rx_desc *desc)
{
	return (le32_get_bits(desc->u.qcn9274_compact.msdu_end.info14,
			      RX_MSDU_END_INFO14_DECRYPT_STATUS_CODE) ==
			RX_DESC_DECRYPT_STATUS_CODE_OK);
}

static inline u32 ath12k_wifi7_hal_get_rx_desc_size_qcn9274(void)
{
	return sizeof(struct hal_rx_desc_qcn9274_compact);
}

static inline
u8 ath12k_wifi7_hal_rx_get_msdu_src_link_qcn9274(struct hal_rx_desc *desc)
{
	return le64_get_bits(desc->u.qcn9274_compact.msdu_end.msdu_end_tag,
			     RX_MSDU_END_64_TLV_SRC_LINK_ID);
}

static inline u16 ath12k_wifi7_hal_rx_mpdu_start_wmask_get_qcn9274(void)
{
	return QCN9274_MPDU_START_WMASK;
}

static inline u32 ath12k_wifi7_hal_rx_msdu_end_wmask_get_qcn9274(void)
{
	return QCN9274_MSDU_END_WMASK;
}

static inline
void ath12k_wifi7_hal_rx_desc_get_fse_info_qcn9274(struct hal_rx_desc *desc,
						   struct rx_mpdu_desc_info
						   *rx_mpdu_info)
{
	__le32 flow_idx_info = desc->u.qcn9274_compact.msdu_end.info7;

	rx_mpdu_info->flow_idx_timeout =
		le32_get_bits(flow_idx_info,
			      RX_MSDU_END_INFO7_FLOW_IDX_TIMEOUT);
	rx_mpdu_info->flow_idx_invalid =
		le32_get_bits(flow_idx_info,
			      RX_MSDU_END_INFO7_FLOW_IDX_INVALID);
	rx_mpdu_info->flow_info.flow_metadata =
		le16_get_bits(desc->u.qcn9274_compact.msdu_end.fse_metadata,
			      ATH12K_DP_RX_FSE_FLOW_METADATA_MASK);
}

