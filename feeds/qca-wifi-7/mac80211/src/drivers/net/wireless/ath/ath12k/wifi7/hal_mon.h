/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_rx.h"
#include "hal_desc.h"

#ifndef HAL_MON_H
#define HAL_MON_H

#define HAL_MPDU_START_SW_FRAME_GRP_NULL_DATA	0x3

#define RX_MON_MPDU_END_WMASK	0xff
#define HAL_MON_RX_MPDU_START_TLV_SIZE		\
	sizeof(struct hal_rx_mpdu_start)
#define HAL_MON_RX_MSDU_END_TLV_SIZE		\
	sizeof(struct hal_rx_msdu_end)
#define HAL_MON_RX_PPDU_EU_STATS_TLV_SIZE	\
	sizeof(struct hal_rx_ppdu_end_user_stats)

#define HAL_RX_PPDU_START_INFO0_PPDU_ID			GENMASK(15, 0)
#define HAL_RX_PPDU_START_INFO1_CHAN_NUM		GENMASK(15, 0)
#define HAL_RX_PPDU_START_INFO1_CHAN_FREQ		GENMASK(31, 16)

struct hal_rx_ppdu_start {
	__le32 info0;
	__le32 info1;
	__le32 ppdu_start_ts_31_0;
	__le32 ppdu_start_ts_63_32;
	__le32 rsvd0[2];
} __packed;

#define HAL_RX_PPDU_END_USER_STATS_INFO0_MCS			GENMASK(16, 13)
#define HAL_RX_PPDU_END_USER_STATS_INFO0_NSS			GENMASK(19, 17)

#define HAL_RX_PPDU_END_USER_STATS_INFO1_PEER_ID		GENMASK(13, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO1_DEVICE_ID		GENMASK(15, 14)
#define HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR	GENMASK(26, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK	GENMASK(10, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID		BIT(11)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_QOS_CTRL_VALID		BIT(12)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_HT_CTRL_VALID		BIT(13)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_SEQ_CTRL_VALID		BIT(14)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE               GENMASK(24, 21)

#define HAL_RX_PPDU_END_USER_STATS_INFO3_AST_INDEX		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO3_FRAME_CTRL		GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO4_SEQ_CTRL		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO4_QOS_CTRL		GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO5_UDP_MSDU_CNT		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO5_TCP_MSDU_CNT		GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO6_OTHER_MSDU_CNT		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO6_TCP_ACK_MSDU_CNT	GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO7_TID_BITMAP		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO7_TID_EOSP_BITMAP	GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO8_MPDU_OK_BYTE_CNT       GENMASK(24, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO9_MPDU_ERR_BYTE_CNT      GENMASK(24, 0)

#define HAL_RX_PPDU_END_USER_STATS_INFO10_MSDU_RETRY_CNT        GENMASK(31, 16)
#define HAL_RX_PPDU_END_USER_STATS_INFO11_MPDU_RETRY_CNT        GENMASK(28, 18)

/* The below hal_rx_ppdu_end_user_stats structure is non-compact
 * structure for PPDU END USER STATS TLV.
 * Only WCN chipset is using non-compact tlv structures.
 */
struct hal_rx_ppdu_end_user_stats {
	__le32 rsvd0;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 ht_ctrl;
	__le32 rsvd1[2];
	__le32 info5;
	__le32 info6;
	__le32 usr_resp_ref;
	__le32 info7;
	__le32 rsvd2[4];
	__le32 info8;
	__le32 rsvd3;
	__le32 info9;
	__le32 info10;
	__le32 rsvd4;
	__le32 usr_resp_ref_ext;
	__le32 info11;
	__le32 rsvd5[6];
} __packed;

struct hal_rx_ppdu_end_user_stats_ext {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 rsvd0;
} __packed;

#define HAL_RX_HT_SIG_INFO_INFO0_MCS		GENMASK(6, 0)
#define HAL_RX_HT_SIG_INFO_INFO0_BW		BIT(7)

#define HAL_RX_HT_SIG_INFO_INFO1_STBC		GENMASK(5, 4)
#define HAL_RX_HT_SIG_INFO_INFO1_FEC_CODING	BIT(6)
#define HAL_RX_HT_SIG_INFO_INFO1_GI		BIT(7)

struct hal_rx_ht_sig_info {
	__le32 info0;
	__le32 info1;
} __packed;

#define HAL_RX_LSIG_B_INFO_INFO0_RATE	GENMASK(3, 0)
#define HAL_RX_LSIG_B_INFO_INFO0_LEN	GENMASK(15, 4)

struct hal_rx_lsig_b_info {
	__le32 info0;
} __packed;

#define HAL_RX_LSIG_A_INFO_INFO0_RATE		GENMASK(3, 0)
#define HAL_RX_LSIG_A_INFO_INFO0_LEN		GENMASK(16, 5)
#define HAL_RX_LSIG_A_INFO_INFO0_PKT_TYPE	GENMASK(27, 24)

struct hal_rx_lsig_a_info {
	__le32 info0;
} __packed;

#define HAL_RX_VHT_SIG_A_INFO_INFO0_BW		GENMASK(1, 0)
#define HAL_RX_VHT_SIG_A_INFO_INFO0_STBC	BIT(3)
#define HAL_RX_VHT_SIG_A_INFO_INFO0_GROUP_ID	GENMASK(9, 4)
#define HAL_RX_VHT_SIG_A_INFO_INFO0_NSTS	GENMASK(21, 10)

#define HAL_RX_VHT_SIG_A_INFO_INFO1_GI_SETTING		GENMASK(1, 0)
#define HAL_RX_VHT_SIG_A_INFO_INFO1_SU_MU_CODING	BIT(2)
#define HAL_RX_VHT_SIG_A_INFO_INFO1_MCS			GENMASK(7, 4)
#define HAL_RX_VHT_SIG_A_INFO_INFO1_BEAMFORMED		BIT(8)

struct hal_rx_vht_sig_a_info {
	__le32 info0;
	__le32 info1;
} __packed;

#define HAL_RX_USIG_CMN_INFO0_PHY_VERSION	GENMASK(2, 0)
#define HAL_RX_USIG_CMN_INFO0_BW		GENMASK(5, 3)
#define HAL_RX_USIG_CMN_INFO0_UL_DL		BIT(6)
#define HAL_RX_USIG_CMN_INFO0_BSS_COLOR		GENMASK(12, 7)
#define HAL_RX_USIG_CMN_INFO0_TXOP		GENMASK(19, 13)
#define HAL_RX_USIG_CMN_INFO0_DISREGARD		GENMASK(25, 20)
#define HAL_RX_USIG_CMN_INFO0_VALIDATE		BIT(26)

struct hal_mon_usig_cmn {
	__le32 info0;
} __packed;

#define HAL_RX_USIG_TB_INFO0_PPDU_TYPE_COMP_MODE	GENMASK(1, 0)
#define HAL_RX_USIG_TB_INFO0_VALIDATE			BIT(2)
#define HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_1		GENMASK(6, 3)
#define HAL_RX_USIG_TB_INFO0_SPATIAL_REUSE_2		GENMASK(10, 7)
#define HAL_RX_USIG_TB_INFO0_DISREGARD_1		GENMASK(15, 11)
#define HAL_RX_USIG_TB_INFO0_CRC			GENMASK(19, 16)
#define HAL_RX_USIG_TB_INFO0_TAIL			GENMASK(25, 20)
#define HAL_RX_USIG_TB_INFO0_RX_INTEG_CHECK_PASS	BIT(31)

struct hal_mon_usig_tb {
	__le32 info0;
} __packed;

#define HAL_RX_USIG_MU_INFO0_PPDU_TYPE_COMP_MODE	GENMASK(1, 0)
#define HAL_RX_USIG_MU_INFO0_VALIDATE_1			BIT(2)
#define HAL_RX_USIG_MU_INFO0_PUNC_CH_INFO		GENMASK(7, 3)
#define HAL_RX_USIG_MU_INFO0_VALIDATE_2			BIT(8)
#define HAL_RX_USIG_MU_INFO0_EHT_SIG_MCS		GENMASK(10, 9)
#define HAL_RX_USIG_MU_INFO0_NUM_EHT_SIG_SYM		GENMASK(15, 11)
#define HAL_RX_USIG_MU_INFO0_CRC			GENMASK(20, 16)
#define HAL_RX_USIG_MU_INFO0_TAIL			GENMASK(26, 21)
#define HAL_RX_USIG_MU_INFO0_RX_INTEG_CHECK_PASS	BIT(31)

struct hal_mon_usig_mu {
	__le32 info0;
} __packed;

union hal_mon_usig_non_cmn {
	struct hal_mon_usig_tb tb;
	struct hal_mon_usig_mu mu;
};

struct hal_mon_usig_hdr {
	struct hal_mon_usig_cmn cmn;
	union hal_mon_usig_non_cmn non_cmn;
} __packed;

#define HAL_RX_PHY_CMN_USER_INFO0_GI		GENMASK(17, 16)

struct hal_phyrx_common_user_info {
	__le32 rsvd0[2];
	__le32 info0;
	__le32 rsvd1;
} __packed;

#define HAL_RX_EHT_SIG_NDP_CMN_INFO0_SPATIAL_REUSE	GENMASK(3, 0)
#define HAL_RX_EHT_SIG_NDP_CMN_INFO0_GI_LTF		GENMASK(5, 4)
#define HAL_RX_EHT_SIG_NDP_CMN_INFO0_NUM_LTF_SYM	GENMASK(8, 6)
#define HAL_RX_EHT_SIG_NDP_CMN_INFO0_NSS		GENMASK(10, 7)
#define HAL_RX_EHT_SIG_NDP_CMN_INFO0_BEAMFORMED		BIT(11)
#define HAL_RX_EHT_SIG_NDP_CMN_INFO0_DISREGARD		GENMASK(13, 12)
#define HAL_RX_EHT_SIG_NDP_CMN_INFO0_CRC		GENMASK(17, 14)

struct hal_eht_sig_ndp_cmn_eb {
	__le32 info0;
} __packed;

#define HAL_RX_EHT_SIG_OVERFLOW_INFO0_SPATIAL_REUSE		GENMASK(3, 0)
#define HAL_RX_EHT_SIG_OVERFLOW_INFO0_GI_LTF			GENMASK(5, 4)
#define HAL_RX_EHT_SIG_OVERFLOW_INFO0_NUM_LTF_SYM		GENMASK(8, 6)
#define HAL_RX_EHT_SIG_OVERFLOW_INFO0_LDPC_EXTA_SYM		BIT(9)
#define HAL_RX_EHT_SIG_OVERFLOW_INFO0_PRE_FEC_PAD_FACTOR	GENMASK(11, 10)
#define HAL_RX_EHT_SIG_OVERFLOW_INFO0_DISAMBIGUITY		BIT(12)
#define HAL_RX_EHT_SIG_OVERFLOW_INFO0_DISREGARD			GENMASK(16, 13)

struct hal_eht_sig_usig_overflow {
	__le32 info0;
} __packed;

#define HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_STA_ID	GENMASK(10, 0)
#define HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_MCS	GENMASK(14, 11)
#define HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_VALIDATE	BIT(15)
#define HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_NSS	GENMASK(19, 16)
#define HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_BEAMFORMED	BIT(20)
#define HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_CODING	BIT(21)
#define HAL_RX_EHT_SIG_NON_MUMIMO_USER_INFO0_CRC	GENMASK(25, 22)

struct hal_eht_sig_non_mu_mimo {
	__le32 info0;
} __packed;

#define HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_STA_ID		GENMASK(10, 0)
#define HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_MCS		GENMASK(14, 11)
#define HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_CODING		BIT(15)
#define HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_SPATIAL_CODING	GENMASK(22, 16)
#define HAL_RX_EHT_SIG_MUMIMO_USER_INFO0_CRC		GENMASK(26, 23)

struct hal_eht_sig_mu_mimo {
	__le32 info0;
} __packed;

union hal_eht_sig_user_field {
	struct hal_eht_sig_mu_mimo mu_mimo;
	struct hal_eht_sig_non_mu_mimo n_mu_mimo;
};

#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_SPATIAL_REUSE		GENMASK(3, 0)
#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_GI_LTF			GENMASK(5, 4)
#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_NUM_LTF_SYM		GENMASK(8, 6)
#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_LDPC_EXTA_SYM		BIT(9)
#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_PRE_FEC_PAD_FACTOR	GENMASK(11, 10)
#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_DISAMBIGUITY		BIT(12)
#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_DISREGARD		GENMASK(16, 13)
#define HAL_RX_EHT_SIG_NON_OFDMA_INFO0_NUM_USERS		GENMASK(19, 17)

struct hal_eht_sig_non_ofdma_cmn_eb {
	__le32 info0;
	union hal_eht_sig_user_field user_field;
} __packed;

#define HAL_RX_EHT_SIG_OFDMA_EB1_SPATIAL_REUSE		GENMASK_ULL(3, 0)
#define HAL_RX_EHT_SIG_OFDMA_EB1_GI_LTF			GENMASK_ULL(5, 4)
#define HAL_RX_EHT_SIG_OFDMA_EB1_NUM_LFT_SYM		GENMASK_ULL(8, 6)
#define HAL_RX_EHT_SIG_OFDMA_EB1_LDPC_EXTRA_SYM		BIT(9)
#define HAL_RX_EHT_SIG_OFDMA_EB1_PRE_FEC_PAD_FACTOR	GENMASK_ULL(11, 10)
#define HAL_RX_EHT_SIG_OFDMA_EB1_PRE_DISAMBIGUITY	BIT(12)
#define HAL_RX_EHT_SIG_OFDMA_EB1_DISREGARD		GENMASK_ULL(16, 13)
#define HAL_RX_EHT_SIG_OFDMA_EB1_RU_ALLOC_1_1		GENMASK_ULL(25, 17)
#define HAL_RX_EHT_SIG_OFDMA_EB1_RU_ALLOC_1_2		GENMASK_ULL(34, 26)
#define HAL_RX_EHT_SIG_OFDMA_EB1_CRC			GENMASK_ULL(30, 27)

struct hal_eht_sig_ofdma_cmn_eb1 {
	__le64 info0;
} __packed;

#define HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_1		GENMASK_ULL(8, 0)
#define HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_2		GENMASK_ULL(17, 9)
#define HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_3		GENMASK_ULL(26, 18)
#define HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_4		GENMASK_ULL(35, 27)
#define HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_5		GENMASK_ULL(44, 36)
#define HAL_RX_EHT_SIG_OFDMA_EB2_RU_ALLOC_2_6		GENMASK_ULL(53, 45)
#define HAL_RX_EHT_SIG_OFDMA_EB2_MCS			GNEMASK_ULL(57, 54)

struct hal_eht_sig_ofdma_cmn_eb2 {
	__le64 info0;
} __packed;

struct hal_eht_sig_ofdma_cmn_eb {
	struct hal_eht_sig_ofdma_cmn_eb1 eb1;
	struct hal_eht_sig_ofdma_cmn_eb2 eb2;
	union hal_eht_sig_user_field user_field;
} __packed;

#define HAL_RX_USR_INFO0_PHY_PPDU_ID		GENMASK(15, 0)
#define HAL_RX_USR_INFO0_USR_RSSI		GENMASK(23, 16)
#define HAL_RX_USR_INFO0_PKT_TYPE		GENMASK(27, 24)
#define HAL_RX_USR_INFO0_STBC			BIT(28)
#define HAL_RX_USR_INFO0_RECEPTION_TYPE		GENMASK(31, 29)

#define HAL_RX_USR_INFO1_MCS			GENMASK(3, 0)
#define HAL_RX_USR_INFO1_SGI			GENMASK(5, 4)
#define HAL_RX_USR_INFO1_HE_RANGING_NDP		BIT(6)
#define HAL_RX_USR_INFO1_MIMO_SS_BITMAP		GENMASK(15, 8)
#define HAL_RX_USR_INFO1_RX_BW			GENMASK(18, 16)
#define HAL_RX_USR_INFO1_DL_OFMDA_USR_IDX	GENMASK(31, 24)

#define HAL_RX_USR_INFO2_DL_OFDMA_CONTENT_CHAN	BIT(0)
#define HAL_RX_USR_INFO2_NSS			GENMASK(10, 8)
#define HAL_RX_USR_INFO2_STREAM_OFFSET		GENMASK(13, 11)
#define HAL_RX_USR_INFO2_STA_DCM		BIT(14)
#define HAL_RX_USR_INFO2_LDPC			BIT(15)
#define HAL_RX_USR_INFO2_RU_TYPE_80_0		GENMASK(19, 16)
#define HAL_RX_USR_INFO2_RU_TYPE_80_1		GENMASK(23, 20)
#define HAL_RX_USR_INFO2_RU_TYPE_80_2		GENMASK(27, 24)
#define HAL_RX_USR_INFO2_RU_TYPE_80_3		GENMASK(31, 28)

#define HAL_RX_USR_INFO3_RU_START_IDX_80_0	GENMASK(5, 0)
#define HAL_RX_USR_INFO3_RU_START_IDX_80_1	GENMASK(13, 8)
#define HAL_RX_USR_INFO3_RU_START_IDX_80_2	GENMASK(21, 16)
#define HAL_RX_USR_INFO3_RU_START_IDX_80_3	GENMASK(29, 24)

struct hal_receive_user_info {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 user_fd_rssi_seg0;
	__le32 user_fd_rssi_seg1;
	__le32 user_fd_rssi_seg2;
	__le32 user_fd_rssi_seg3;
} __packed;

#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_MCS	GENMASK(6, 3)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_DCM		BIT(7)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_TRANSMIT_BW	GENMASK(20, 19)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_CP_LTF_SIZE	GENMASK(22, 21)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_NSTS		GENMASK(25, 23)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_BSS_COLOR		GENMASK(13, 8)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_SPATIAL_REUSE	GENMASK(18, 15)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_FORMAT_IND	BIT(0)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_BEAM_CHANGE	BIT(1)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO0_DL_UL_FLAG	BIT(2)

#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXOP_DURATION	GENMASK(6, 0)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_CODING		BIT(7)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_LDPC_EXTRA	BIT(8)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_STBC		BIT(9)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_TXBF		BIT(10)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_PKT_EXT_FACTOR	GENMASK(12, 11)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_PKT_EXT_PE_DISAM	BIT(13)
#define HAL_RX_HE_SIG_A_SU_INFO_INFO1_DOPPLER_IND	BIT(15)

struct hal_rx_he_sig_a_su_info {
	__le32 info0;
	__le32 info1;
} __packed;

#define HAL_RX_HE_SIG_A_MU_DL_INFO0_UL_FLAG		BIT(1)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_MCS_OF_SIGB		GENMASK(3, 1)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_DCM_OF_SIGB		BIT(4)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_BSS_COLOR		GENMASK(10, 5)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_SPATIAL_REUSE	GENMASK(14, 11)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_TRANSMIT_BW		GENMASK(17, 15)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_NUM_SIGB_SYMB	GENMASK(21, 18)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_COMP_MODE_SIGB	BIT(22)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_CP_LTF_SIZE		GENMASK(24, 23)
#define HAL_RX_HE_SIG_A_MU_DL_INFO0_DOPPLER_INDICATION	BIT(25)

#define HAL_RX_HE_SIG_A_MU_DL_INFO1_TXOP_DURATION	GENMASK(6, 0)
#define HAL_RX_HE_SIG_A_MU_DL_INFO1_NUM_LTF_SYMB	GENMASK(10, 8)
#define HAL_RX_HE_SIG_A_MU_DL_INFO1_LDPC_EXTRA		BIT(11)
#define HAL_RX_HE_SIG_A_MU_DL_INFO1_STBC		BIT(12)
#define HAL_RX_HE_SIG_A_MU_DL_INFO1_PKT_EXT_FACTOR	GENMASK(14, 13)
#define HAL_RX_HE_SIG_A_MU_DL_INFO1_PKT_EXT_PE_DISAM	BIT(15)

struct hal_rx_he_sig_a_mu_dl_info {
	__le32 info0;
	__le32 info1;
} __packed;

#define HAL_RX_HE_SIG_B1_MU_INFO_INFO0_RU_ALLOCATION	GENMASK(7, 0)

struct hal_rx_he_sig_b1_mu_info {
	__le32 info0;
} __packed;

#define HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_ID           GENMASK(10, 0)
#define HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_MCS		GENMASK(18, 15)
#define HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_CODING	BIT(20)
#define HAL_RX_HE_SIG_B2_MU_INFO_INFO0_STA_NSTS		GENMASK(31, 29)

struct hal_rx_he_sig_b2_mu_info {
	__le32 info0;
} __packed;

#define HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_ID	GENMASK(10, 0)
#define HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_NSTS	GENMASK(13, 11)
#define HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_TXBF	BIT(14)
#define HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_MCS	GENMASK(18, 15)
#define HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_DCM	BIT(19)
#define HAL_RX_HE_SIG_B2_OFDMA_INFO_INFO0_STA_CODING	BIT(20)

struct hal_rx_he_sig_b2_ofdma_info {
	__le32 info0;
} __packed;

#define HAL_RX_PHYRX_RSSI_LEGACY_INFO_INFO0_RECEPTION	GENMASK(3, 0)
#define HAL_RX_PHYRX_RSSI_LEGACY_INFO_INFO0_RX_BW	GENMASK(7, 5)
#define HAL_RX_PHYRX_RSSI_LEGACY_INFO_INFO1_RSSI_COMB	GENMASK(15, 8)

struct hal_rx_phyrx_rssi_legacy_info {
	__le32 info0;
	__le32 rsvd0[39];
	__le32 info1;
	__le32 rsvd1;
} __packed;

#define HAL_RX_MPDU_START_INFO0_ENCYRPT_TYP		GENMASK(5, 2)
#define HAL_RX_MPDU_START_INFO1_FILTER_CAT		GENMASK(1, 0)
#define HAL_RX_MPDU_START_INFO1_SW_GRP_ID		GENMASK(8, 2)
#define HAL_RX_MPDU_START_INFO1_PPDU_ID			GENMASK(31, 16)
#define HAL_RX_MPDU_START_INFO2_PEERID			GENMASK(29, 16)
#define HAL_RX_MPDU_START_INFO2_DEVICE_ID		GENMASK(31, 30)
#define HAL_RX_MPDU_START_INFO3_FC_VALID		BIT(0)
#define HAL_RX_MPDU_START_INFO3_ADDR2_VALID		BIT(3)
#define HAL_RX_MPDU_START_INFO3_TO_DS			BIT(17)
#define HAL_RX_MPDU_START_INFO3_MPDU_RETRY		BIT(19)
#define HAL_RX_MPDU_START_INFO4_DECAP_TYPE		GENMASK(11, 10)
#define HAL_RX_MPDU_START_INFO4_RAW_MPDU		BIT(30)
#define HAL_RX_MPDU_START_INFO5_MPDU_LEN		GENMASK(13, 0)
#define HAL_RX_MPDU_START_INFO5_MCAST_BCAST		BIT(15)
#define HAL_RX_MPDU_START_INFO6_FC_FIELD		GENMASK(15, 0)
#define HAL_RX_MPDU_START_INFO7_ADDR2_15_0		GENMASK(31, 16)
#define HAL_RX_MPDU_START_INFO8_ADDR2_47_16		GENMASK(31, 0)

/* The below hal_rx_mpdu_start structure is non-compact
 * structure for MPDU START TLV.
 * Only WCN chipset is using non-compact tlv structures.
 */
struct hal_rx_mpdu_start {
	__le32 rsvd0[7];
	__le32 info0;
	__le32 rsvd1;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 rsvd2;
	__le32 info7;
	__le32 info8;
	__le32 info9;
	__le32 info10;
	__le32 info11;
	__le32 info12;
	__le32 rsvd3[8];
} __packed;

#define HAL_RX_MSDU_END_INFO0_SW_FRAME_GRP_ID			GENMASK(8, 2)
#define HAL_RX_MSDU_END_INFO1_DECAP_FORMAT			GENMASK(9, 8)

/* The below hal_rx_msdu_end structure is non-compact
 * structure for MSDU END TLV.
 * Only WCN chipset is using non-compact tlv structures.
 */
struct hal_rx_msdu_end {
	__le32 info0;
	__le32 rsvd0[18];
	__le32 info1;
	__le32 rsvd1[10];
	__le32 info2;
	__le32 rsvd2;
} __packed;

#define HAL_RX_PPDU_END_DURATION	GENMASK(23, 0)
struct hal_rx_ppdu_end_duration {
	__le32 rsvd0[9];
	__le32 info0;
	__le32 rsvd1[18];
} __packed;

#define HAL_TX_FES_STATUS_END_INFO0_START_TIMESTAMP_15_0	GENMASK(15, 0)
#define HAL_TX_FES_STATUS_END_INFO0_START_TIMESTAMP_31_16	GENMASK(31, 16)

struct hal_tx_fes_status_end {
	__le32 rsvd0[2];
	__le32 info0;
	__le32 reserved1[19];
} __packed;

#define HAL_TX_FES_SETUP_INFO0_NUM_OF_USERS	GENMASK(28, 23)

struct hal_tx_fes_setup {
	__le32 schedule_id;
	__le32 info0;
	__le64 rsvd0;
} __packed;

#define HAL_RX_RESP_REQ_INFO0_PPDU_ID		GENMASK(15, 0)
#define HAL_RX_RESP_REQ_INFO0_RECEPTION_TYPE	BIT(16)
#define HAL_RX_RESP_REQ_INFO1_DURATION		GENMASK(15, 0)
#define HAL_RX_RESP_REQ_INFO1_RATE_MCS		GENMASK(24, 21)
#define HAL_RX_RESP_REQ_INFO1_SGI		GENMASK(26, 25)
#define HAL_RX_RESP_REQ_INFO1_STBC		BIT(27)
#define HAL_RX_RESP_REQ_INFO1_LDPC		BIT(28)
#define HAL_RX_RESP_REQ_INFO1_IS_AMPDU		BIT(29)
#define HAL_RX_RESP_REQ_INFO2_NUM_USER		GENMASK(6, 0)
#define HAL_RX_RESP_REQ_INFO3_ADDR1_31_0	GENMASK(31, 0)
#define HAL_RX_RESP_REQ_INFO4_ADDR1_47_32	GENMASK(15, 0)
#define HAL_RX_RESP_REQ_INFO4_ADDR1_15_0	GENMASK(31, 16)
#define HAL_RX_RESP_REQ_INFO5_ADDR1_47_16	GENMASK(31, 0)

struct hal_rx_resp_req_info {
	__le32 info0;
	__le32 rsvd0[1];
	__le32 info1;
	__le32 info2;
	__le32 rsvd1[2];
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 rsvd2[5];
} __packed;

#define HAL_TX_PPDU_SETUP_INFO0_MEDIUM_PROT_TYPE	GENMASK(2, 0)
#define HAL_TX_PPDU_SETUP_INFO1_PROT_FRAME_ADDR1_31_0	GENMASK(31, 0)
#define HAL_TX_PPDU_SETUP_INFO2_PROT_FRAME_ADDR1_47_32	GENMASK(15, 0)
#define HAL_TX_PPDU_SETUP_INFO2_PROT_FRAME_ADDR2_15_0	GENMASK(31, 16)
#define HAL_TX_PPDU_SETUP_INFO3_PROT_FRAME_ADDR2_47_16	GENMASK(31, 0)
#define HAL_TX_PPDU_SETUP_INFO4_PROT_FRAME_ADDR3_31_0	GENMASK(31, 0)
#define HAL_TX_PPDU_SETUP_INFO5_PROT_FRAME_ADDR3_47_32	GENMASK(15, 0)
#define HAL_TX_PPDU_SETUP_INFO5_PROT_FRAME_ADDR4_15_0	GENMASK(31, 16)
#define HAL_TX_PPDU_SETUP_INFO6_PROT_FRAME_ADDR4_47_16	GENMASK(31, 0)

struct hal_tx_pcu_ppdu_setup_init {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 rsvd0;
	__le32 info4;
	__le32 info5;
	__le32 info6;
} __packed;

#define HAL_TX_FES_STAT_PROT_INFO0_STRT_FRM_TS_15_0	GENMASK(15, 0)
#define HAL_TX_FES_STAT_PROT_INFO0_STRT_FRM_TS_31_16	GENMASK(31, 16)
#define HAL_TX_FES_STAT_PROT_INFO1_END_FRM_TS_15_0	GENMASK(15, 0)
#define HAL_TX_FES_STAT_PROT_INFO1_END_FRM_TS_31_16	GENMASK(31, 16)

struct hal_tx_fes_status_prot {
	__le64 rsvd0;
	__le32 info0;
	__le32 info1;
	__le32 rsvd1[11];
} __packed;

#define HAL_TX_FES_STAT_USR_PPDU_INFO0_DURATION		GENMASK(15, 0)

struct hal_tx_fes_status_user_ppdu {
	__le64 rsvd0;
	__le32 info0;
	__le32 rsvd1[3];
} __packed;

#define HAL_TX_FES_STAT_STRT_INFO0_PROT_TS_LOWER_32	GENMASK(31, 0)
#define HAL_TX_FES_STAT_STRT_INFO1_PROT_TS_UPPER_32	GENMASK(31, 0)

struct hal_tx_fes_status_start_prot {
	__le32 info0;
	__le32 info1;
	__le64 rsvd0;
} __packed;

#define HAL_TX_FES_STATUS_START_INFO0_MEDIUM_PROT_TYPE	GENMASK(29, 27)

struct hal_tx_fes_status_start {
	__le32 rsvd0;
	__le32 info0;
	__le64 rsvd1;
} __packed;

#define HAL_TX_Q_EXT_INFO0_FRAME_CTRL		GENMASK(15, 0)
#define HAL_TX_Q_EXT_INFO0_QOS_CTRL		GENMASK(31, 16)
#define HAL_TX_Q_EXT_INFO1_AMPDU_FLAG		BIT(0)

struct hal_tx_queue_exten {
	__le32 info0;
	__le32 info1;
} __packed;

#define HAL_RX_FBM_ACK_INFO0_ADDR1_31_0		GENMASK(31, 0)
#define HAL_RX_FBM_ACK_INFO1_ADDR1_47_32	GENMASK(15, 0)
#define HAL_RX_FBM_ACK_INFO1_ADDR2_15_0		GENMASK(31, 16)
#define HAL_RX_FBM_ACK_INFO2_ADDR2_47_16	GENMASK(31, 0)

struct hal_rx_frame_bitmap_ack {
	__le32 rsvd0;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 rsvd1[10];
} __packed;

#define HAL_TX_PHY_DESC_INFO0_BF_TYPE		GENMASK(17, 16)
#define HAL_TX_PHY_DESC_INFO0_PREAMBLE_11B	BIT(20)
#define HAL_TX_PHY_DESC_INFO0_PKT_TYPE		GENMASK(24, 21)
#define HAL_TX_PHY_DESC_INFO0_BANDWIDTH		GENMASK(30, 28)
#define HAL_TX_PHY_DESC_INFO1_MCS		GENMASK(3, 0)
#define HAL_TX_PHY_DESC_INFO1_STBC		BIT(6)
#define HAL_TX_PHY_DESC_INFO2_NSS		GENMASK(23, 21)
#define HAL_TX_PHY_DESC_INFO3_AP_PKT_BW		GENMASK(6, 4)
#define HAL_TX_PHY_DESC_INFO3_LTF_SIZE		GENMASK(20, 19)
#define HAL_TX_PHY_DESC_INFO3_ACTIVE_CHANNEL	GENMASK(17, 15)

struct hal_tx_phy_desc {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
} __packed;

struct hal_tlv_parsed_hdr {
	u16 tag;
	u16 len;
	u16 userid;
	u8 *data;
};

#define MPDU_START_SELECT_INFO0_ENCYRPT_TYP                      BIT(3)
#define MPDU_START_SELECT_INFO1_FILTER_GRPID_PPDUID              BIT(4)
#define MPDU_START_SELECT_INFO2_INFO3                            BIT(5)
#define MPDU_START_SELECT_INFO4_INFO5                            BIT(6)
#define MPDU_START_SELECT_INFO6_FC                               BIT(7)
#define MPDU_START_SELECT_INFO7_INFO8                            BIT(8)
#define MPDU_START_SELECT_INFO9_INF010                           BIT(9)
#define MPDU_START_SELECT_INF011_INFO12                          BIT(10)

#define RX_MON_MPDU_START_WMASK \
		(MPDU_START_SELECT_INFO0_ENCYRPT_TYP |                \
		 MPDU_START_SELECT_INFO1_FILTER_GRPID_PPDUID |        \
		 MPDU_START_SELECT_INFO2_INFO3 |                      \
		 MPDU_START_SELECT_INFO4_INFO5 |                      \
		 MPDU_START_SELECT_INFO6_FC |                         \
		 MPDU_START_SELECT_INFO7_INFO8 |                      \
		 MPDU_START_SELECT_INFO9_INF010 |                     \
		 MPDU_START_SELECT_INF011_INFO12)

#define HAL_RX_MPDU_START_INFO0_ENCYRPT_TYP_CMPCT		GENMASK(5, 2)
#define HAL_RX_MPDU_START_INFO1_FILTER_CAT_CMPCT		GENMASK(1, 0)
#define HAL_RX_MPDU_START_INFO1_SW_GRP_ID_CMPCT			GENMASK(8, 2)
#define HAL_RX_MPDU_START_INFO1_PPDU_ID_CMPCT			GENMASK(31, 16)
#define HAL_RX_MPDU_START_INFO2_PEERID_CMPCT			GENMASK(29, 16)
#define HAL_RX_MPDU_START_INFO2_DEVICE_ID_CMPCT			GENMASK(31, 30)
#define HAL_RX_MPDU_START_INFO3_FC_VALID_CMPCT			BIT(0)
#define HAL_RX_MPDU_START_INFO3_ADDR2_VALID_CMPCT		BIT(3)
#define HAL_RX_MPDU_START_INFO3_TO_DS_CMPCT			BIT(17)
#define HAL_RX_MPDU_START_INFO3_MPDU_RETRY_CMPCT		BIT(19)
#define HAL_RX_MPDU_START_INFO4_DECAP_TYPE_CMPCT		GENMASK(11, 10)
#define HAL_RX_MPDU_START_INFO4_RAW_MPDU_CMPCT			BIT(30)
#define HAL_RX_MPDU_START_INFO5_MPDU_LEN_CMPCT			GENMASK(13, 0)
#define HAL_RX_MPDU_START_INFO5_MCAST_BCAST_CMPCT		BIT(15)
#define HAL_RX_MPDU_START_INFO6_FC_FIELD_CMPCT			GENMASK(15, 0)
#define HAL_RX_MPDU_START_INFO7_ADDR2_15_0_CMPCT		GENMASK(31, 16)
#define HAL_RX_MPDU_START_INFO8_ADDR2_47_16_CMPCT		GENMASK(31, 0)

/* The below hal_rx_mon_mpdu_start_compact structure is tied with the mask value
 * RX_MON_MPDU_START_WMASK. If the mask value changes the structure will also
 * change.
 * ipq5424/5332, qcn6432/9274 uses common compact staructure as they share same
 * mpdu start wmask.
 */
struct hal_rx_mon_mpdu_start_compact {
	__le32 rsvd0;
	__le32 info0;
	__le32 rsvd1;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 rsvd2;
	__le32 info7;
	__le32 info8;
	__le32 info9;
	__le32 info10;
	__le32 info11;
	__le32 info12;
} __packed;

#define MSDU_END_SELECT_INFO0_SW_GRPID                         BIT(0)
#define MSDU_END_SELECT_INFO1_DECAP_FORMAT                     BIT(9)
#define MSDU_END_SELECT_INFO2_ERR_MAP                          BIT(15)

#define RX_MON_MSDU_END_WMASK \
		(MSDU_END_SELECT_INFO0_SW_GRPID |              \
		 MSDU_END_SELECT_INFO1_DECAP_FORMAT |          \
		 MSDU_END_SELECT_INFO2_ERR_MAP)

#define HAL_RX_MSDU_END_INFO0_SW_FRAME_GRP_ID_CMPCT		GENMASK(8, 2)
#define HAL_RX_MSDU_END_INFO1_DECAP_FORMAT_CMPCT		GENMASK(9, 8)

/* The below hal_rx_mon_msdu_end_compact structure is tied with the mask value
 * RX_MON_MSDU_END_WMASK. If the mask value changes the structure will also
 * change.
 * ipq5424/5332, qcn6432/9274 uses common compact staructure as they share same
 * msdu end wmask.
 */
struct hal_rx_mon_msdu_end_compact {
	__le32 info0;
	__le32 rsvd0[2];
	__le32 info1;
	__le32 info2;
	__le32 rsvd1;
} __packed;

#define PPDU_END_USER_STATS_SELECT_INFO0_MCS_NSS                  BIT(0)
#define PPDU_END_USER_STATS_SELECT_INFO1_INFO02                   BIT(1)
#define PPDU_END_USER_STATS_SELECT_INFO3_INFO04                   BIT(2)
#define PPDU_END_USER_STATS_SELECT_HTCTRL                         BIT(3)
#define PPDU_END_USER_STATS_SELECT_INFO5_UDP_TCP_COUNT            BIT(4)
#define PPDU_END_USER_STATS_SELECT_INFO6_USR_RESP_REF             BIT(5)
#define PPDU_END_USER_STATS_SELECT_INFO7_TID_BITMAP               BIT(6)
#define PPDU_END_USER_STATS_SELECT_INFO8_MPDU_OK_COUNT            BIT(8)
#define PPDU_END_USER_STATS_SELECT_INFO9_MPDU_ERR_COUNT           BIT(9)
#define PPDU_END_USER_STATS_SELECT_INFO10_MSDU_RETRY_COUNT        BIT(10)
#define PPDU_END_USER_STATS_SELECT_USR_RESP_REF_EXT_INFO11        BIT(11)

#define RX_MON_PPDU_END_USER_STATS_WMASK \
		(PPDU_END_USER_STATS_SELECT_INFO0_MCS_NSS |          \
		 PPDU_END_USER_STATS_SELECT_INFO1_INFO02 |           \
		 PPDU_END_USER_STATS_SELECT_INFO3_INFO04 |           \
		 PPDU_END_USER_STATS_SELECT_HTCTRL |                 \
		 PPDU_END_USER_STATS_SELECT_INFO5_UDP_TCP_COUNT |    \
		 PPDU_END_USER_STATS_SELECT_INFO6_USR_RESP_REF  |    \
		 PPDU_END_USER_STATS_SELECT_INFO7_TID_BITMAP    |    \
		 PPDU_END_USER_STATS_SELECT_INFO8_MPDU_OK_COUNT |    \
		 PPDU_END_USER_STATS_SELECT_INFO9_MPDU_ERR_COUNT |   \
		 PPDU_END_USER_STATS_SELECT_INFO10_MSDU_RETRY_COUNT |\
		 PPDU_END_USER_STATS_SELECT_USR_RESP_REF_EXT_INFO11)

#define HAL_RX_PPDU_END_USER_STATS_INFO0_MCS_CMPCT			GENMASK(16, 13)
#define HAL_RX_PPDU_END_USER_STATS_INFO0_NSS_CMPCT			GENMASK(19, 17)

#define HAL_RX_PPDU_END_USER_STATS_INFO1_PEER_ID_CMPCT			GENMASK(13, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO1_DEVICE_ID_CMPCT		GENMASK(15, 14)
#define HAL_RX_PPDU_END_USER_STATS_INFO1_MPDU_CNT_FCS_ERR_CMPCT		GENMASK(26, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO2_MPDU_CNT_FCS_OK_CMPCT		GENMASK(10, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_FC_VALID_CMPCT			BIT(11)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_QOS_CTRL_VALID_CMPCT		BIT(12)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_HT_CTRL_VALID_CMPCT		BIT(13)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_SEQ_CTRL_VALID_CMPCT		BIT(14)
#define HAL_RX_PPDU_END_USER_STATS_INFO2_PKT_TYPE_CMPCT			GENMASK(24, 21)

#define HAL_RX_PPDU_END_USER_STATS_INFO3_AST_INDEX_CMPCT		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO3_FRAME_CTRL_CMPCT		GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO4_SEQ_CTRL_CMPCT			GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO4_QOS_CTRL_CMPCT			GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO5_UDP_MSDU_CNT_CMPCT		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO5_TCP_MSDU_CNT_CMPCT		GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO6_OTHER_MSDU_CNT_CMPCT		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO6_TCP_ACK_MSDU_CNT_CMPCT		GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO7_TID_BITMAP_CMPCT		GENMASK(15, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO7_TID_EOSP_BITMAP_CMPCT		GENMASK(31, 16)

#define HAL_RX_PPDU_END_USER_STATS_INFO8_MPDU_OK_BYTE_CNT_CMPCT		GENMASK(24, 0)
#define HAL_RX_PPDU_END_USER_STATS_INFO9_MPDU_ERR_BYTE_CNT_CMPCT	GENMASK(24, 0)

#define HAL_RX_PPDU_END_USER_STATS_INFO10_MSDU_RETRY_CNT_CMPCT		GENMASK(31, 16)
#define HAL_RX_PPDU_END_USER_STATS_INFO11_MPDU_RETRY_CNT_CMPCT		GENMASK(28, 18)

/* The below hal_rx_mon_ppdu_end_user_stats_compact structure is tied
 * with the mask value RX_MON_PPDU_END_USER_STATS_WMASK.
 * If the mask value changes the structure will also change.
 * ipq5424/5332, qcn6432 uses common compact staructure as they share same
 * wmask and qcn9274 uses hal_rx_mon_ppdu_end_user_stats_compact_qcn9274
 */
struct hal_rx_mon_ppdu_end_user_stats_compact {
	__le32 rsvd0;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 ht_ctrl;
	__le32 rsvd1[2];
	__le32 info5;
	__le32 info6;
	__le32 usr_resp_ref;
	__le32 info7;
	__le32 rsvd2[2];
	__le32 info8;
	__le32 rsvd3;
	__le32 info9;
	__le32 info10;
	__le32 rsvd4;
	__le32 usr_resp_ref_ext;
	__le32 info11;
} __packed;

static __always_inline void
ath12k_wifi7_hal_mon_parse_rx_msdu_end_err(u32 info, u32 *errmap)
{
	if (info & RX_MSDU_END_INFO13_FCS_ERR)
		*errmap |= HAL_RX_MON_MPDU_ERR_FCS;

	if (info & RX_MSDU_END_INFO13_DECRYPT_ERR)
		*errmap |= HAL_RX_MON_MPDU_ERR_DECRYPT;

	if (info & RX_MSDU_END_INFO13_TKIP_MIC_ERR)
		*errmap |= HAL_RX_MON_MPDU_ERR_TKIP_MIC;

	if (info & RX_MSDU_END_INFO13_A_MSDU_ERROR)
		*errmap |= HAL_RX_MON_MPDU_ERR_AMSDU_ERR;

	if (info & RX_MSDU_END_INFO13_OVERFLOW_ERR)
		*errmap |= HAL_RX_MON_MPDU_ERR_OVERFLOW;

	if (info & RX_MSDU_END_INFO13_MSDU_LEN_ERR)
		*errmap |= HAL_RX_MON_MPDU_ERR_MSDU_LEN;

	if (info & RX_MSDU_END_INFO13_MPDU_LEN_ERR)
		*errmap |= HAL_RX_MON_MPDU_ERR_MPDU_LEN;
}

enum hal_rx_mon_status
ath12k_wifi7_hal_mon_rx_parse_status_tlv(struct ath12k_hal *hal,
					 struct hal_rx_mon_ppdu_info *ppdu_info,
					 struct hal_tlv_parsed_hdr *tlv_parsed_hdr);
enum hal_tx_mon_status
ath12k_wifi7_hal_mon_tx_parse_status_tlv(struct hal_tx_mon_ppdu_info *tx_ppdu_info,
					 u16 tlv_tag, const void *tlv_data, u32 userid);
enum hal_tx_mon_status
ath12k_wifi7_hal_mon_tx_status_get_num_user(u16 tlv_tag,
					    const void *tx_tlv,
					    u8 *num_users);
u32 ath12k_wifi7_hal_mon_rx_mpdu_start_wmask_get(void);
u32 ath12k_wifi7_hal_mon_rx_mpdu_end_wmask_get(void);
u32 ath12k_wifi7_hal_mon_rx_msdu_end_wmask_get(void);
u32 ath12k_wifi7_hal_mon_rx_ppdu_end_usr_stats_wmask_get(void);
void
ath12k_wifi7_hal_mon_rx_mpdu_start_info_parse(const void *tlv_data, u32 userid,
					      struct hal_rx_mon_ppdu_info *ppdu_info,
					      u32 tlv_len);
void
ath12k_wifi7_hal_mon_rx_msdu_end_info_parse(const void *tlv_data, u32 userid,
					    struct hal_rx_mon_ppdu_info *ppdu_info,
					    u32 tlv_len);
void
ath12k_wifi7_hal_mon_rx_ppdu_eu_stats_info_parse(const void *tlv_data, u32 userid,
						 struct hal_rx_mon_ppdu_info *ppdu_info,
						 u32 tlv_len);
u8 *ath12k_wifi7_hal_mon_rx_desc_get_msdu_payload(void *rx_desc);
#endif
