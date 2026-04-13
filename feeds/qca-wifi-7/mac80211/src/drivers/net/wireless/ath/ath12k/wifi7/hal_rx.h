/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_HAL_RX_H
#define ATH12K_HAL_RX_H

#include "hal_desc.h"
#include "hal.h"

struct ath12k_dp;

struct hal_reo_status;

struct hal_rx_wbm_rel_info {
	u32 cookie;
	enum hal_wbm_rel_src_module err_rel_src;
	enum hal_reo_dest_ring_push_reason push_reason;
	u32 err_code;
	bool first_msdu;
	bool last_msdu;
	bool continuation;
	void *rx_desc;
	bool hw_cc_done;
	__le32 peer_metadata;
};

#define HAL_INVALID_PEERID	0x3fff
#define VHT_SIG_SU_NSS_MASK 0x7

#define HAL_RX_MPDU_INFO_PN_GET_BYTE1(__val) \
	le32_get_bits((__val), GENMASK(7, 0))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE2(__val) \
	le32_get_bits((__val), GENMASK(15, 8))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE3(__val) \
	le32_get_bits((__val), GENMASK(23, 16))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE4(__val) \
	le32_get_bits((__val), GENMASK(31, 24))

#define HAL_TLV_STATUS_PPDU_NOT_DONE            0
#define HAL_TLV_STATUS_PPDU_DONE                1
#define HAL_TLV_STATUS_BUF_DONE                 2
#define HAL_TLV_STATUS_PPDU_NON_STD_DONE        3

enum hal_rx_vht_sig_a_gi_setting {
	HAL_RX_VHT_SIG_A_NORMAL_GI = 0,
	HAL_RX_VHT_SIG_A_SHORT_GI = 1,
	HAL_RX_VHT_SIG_A_SHORT_GI_AMBIGUITY = 3,
};

#define HE_GI_0_8 0
#define HE_GI_0_4 1
#define HE_GI_1_6 2
#define HE_GI_3_2 3

#define HE_LTF_1_X 0
#define HE_LTF_2_X 1
#define HE_LTF_4_X 2

enum hal_rx_ul_reception_type {
	HAL_RECEPTION_TYPE_ULOFMDA,
	HAL_RECEPTION_TYPE_ULMIMO,
	HAL_RECEPTION_TYPE_OTHER,
	HAL_RECEPTION_TYPE_FRAMELESS
};

struct hal_rx_rxpcu_classification_overview {
	u32 rsvd0;
} __packed;

struct hal_rx_msdu_desc_info {
	u32 msdu_flags;
	u16 msdu_len; /* 14 bits for length */
};

#define HAL_RX_NUM_MSDU_DESC 6
struct hal_rx_msdu_list {
	struct hal_rx_msdu_desc_info msdu_info[HAL_RX_NUM_MSDU_DESC];
	u64 paddr[HAL_RX_NUM_MSDU_DESC];
	u32 sw_cookie[HAL_RX_NUM_MSDU_DESC];
	u8 rbm[HAL_RX_NUM_MSDU_DESC];
};

#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_0 0xDDBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_1 0xADBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_2 0xBDBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_3 0xCDBEEF

/* HE Radiotap data1 Mask */
#define HE_SU_FORMAT_TYPE 0x0000
#define HE_EXT_SU_FORMAT_TYPE 0x0001
#define HE_MU_FORMAT_TYPE  0x0002
#define HE_TRIG_FORMAT_TYPE  0x0003
#define HE_BEAM_CHANGE_KNOWN 0x0008
#define HE_DL_UL_KNOWN 0x0010
#define HE_MCS_KNOWN 0x0020
#define HE_DCM_KNOWN 0x0040
#define HE_CODING_KNOWN 0x0080
#define HE_LDPC_EXTRA_SYMBOL_KNOWN 0x0100
#define HE_STBC_KNOWN 0x0200
#define HE_DATA_BW_RU_KNOWN 0x4000
#define HE_DOPPLER_KNOWN 0x8000
#define HE_BSS_COLOR_KNOWN 0x0004

/* HE Radiotap data2 Mask */
#define HE_GI_KNOWN 0x0002
#define HE_TXBF_KNOWN 0x0010
#define HE_PE_DISAMBIGUITY_KNOWN 0x0020
#define HE_TXOP_KNOWN 0x0040
#define HE_LTF_SYMBOLS_KNOWN 0x0004
#define HE_PRE_FEC_PADDING_KNOWN 0x0008
#define HE_MIDABLE_PERIODICITY_KNOWN 0x0080

/* HE radiotap data3 shift values */
#define HE_BEAM_CHANGE_SHIFT 6
#define HE_DL_UL_SHIFT 7
#define HE_TRANSMIT_MCS_SHIFT 8
#define HE_DCM_SHIFT 12
#define HE_CODING_SHIFT 13
#define HE_LDPC_EXTRA_SYMBOL_SHIFT 14
#define HE_STBC_SHIFT 15

/* HE radiotap data4 shift values */
#define HE_STA_ID_SHIFT 4

/* HE radiotap data5 */
#define HE_GI_SHIFT 4
#define HE_LTF_SIZE_SHIFT 6
#define HE_LTF_SYM_SHIFT 8
#define HE_TXBF_SHIFT 14
#define HE_PE_DISAMBIGUITY_SHIFT 15
#define HE_PRE_FEC_PAD_SHIFT 12

/* HE radiotap data6 */
#define HE_DOPPLER_SHIFT 4
#define HE_TXOP_SHIFT 8

/* HE radiotap HE-MU flags1 */
#define HE_SIG_B_MCS_KNOWN 0x0010
#define HE_SIG_B_DCM_KNOWN 0x0040
#define HE_SIG_B_SYM_NUM_KNOWN 0x8000
#define HE_RU_0_KNOWN 0x0100
#define HE_RU_1_KNOWN 0x0200
#define HE_RU_2_KNOWN 0x0400
#define HE_RU_3_KNOWN 0x0800
#define HE_DCM_FLAG_1_SHIFT 5
#define HE_SPATIAL_REUSE_MU_KNOWN 0x0100
#define HE_SIG_B_COMPRESSION_FLAG_1_KNOWN 0x4000

/* HE radiotap HE-MU flags2 */
#define HE_SIG_B_COMPRESSION_FLAG_2_SHIFT 3
#define HE_BW_KNOWN 0x0004
#define HE_NUM_SIG_B_SYMBOLS_SHIFT 4
#define HE_SIG_B_COMPRESSION_FLAG_2_KNOWN 0x0100
#define HE_NUM_SIG_B_FLAG_2_SHIFT 9
#define HE_LTF_FLAG_2_SYMBOLS_SHIFT 12
#define HE_LTF_KNOWN 0x8000

/* HE radiotap per_user_1 */
#define HE_STA_SPATIAL_SHIFT 11
#define HE_TXBF_SHIFT 14
#define HE_RESERVED_SET_TO_1_SHIFT 19
#define HE_STA_CODING_SHIFT 20

/* HE radiotap per_user_2 */
#define HE_STA_MCS_SHIFT 4
#define HE_STA_DCM_SHIFT 5

/* HE radiotap per user known */
#define HE_USER_FIELD_POSITION_KNOWN 0x01
#define HE_STA_ID_PER_USER_KNOWN 0x02
#define HE_STA_NSTS_KNOWN 0x04
#define HE_STA_TX_BF_KNOWN 0x08
#define HE_STA_SPATIAL_CONFIG_KNOWN 0x10
#define HE_STA_MCS_KNOWN 0x20
#define HE_STA_DCM_KNOWN 0x40
#define HE_STA_CODING_KNOWN 0x80

#define HAL_RX_MPDU_ERR_FCS			BIT(0)
#define HAL_RX_MPDU_ERR_DECRYPT			BIT(1)
#define HAL_RX_MPDU_ERR_TKIP_MIC		BIT(2)
#define HAL_RX_MPDU_ERR_AMSDU_ERR		BIT(3)
#define HAL_RX_MPDU_ERR_OVERFLOW		BIT(4)
#define HAL_RX_MPDU_ERR_MSDU_LEN		BIT(5)
#define HAL_RX_MPDU_ERR_MPDU_LEN		BIT(6)
#define HAL_RX_MPDU_ERR_UNENCRYPTED_FRAME	BIT(7)

enum hal_eht_bw {
	HAL_EHT_BW_20,
	HAL_EHT_BW_40,
	HAL_EHT_BW_80,
	HAL_EHT_BW_160,
	HAL_EHT_BW_320_1,
	HAL_EHT_BW_320_2,
};

enum hal_mon_reception_type {
	HAL_RECEPTION_TYPE_SU,
	HAL_RECEPTION_TYPE_DL_MU_MIMO,
	HAL_RECEPTION_TYPE_DL_MU_OFMA,
	HAL_RECEPTION_TYPE_DL_MU_OFDMA_MIMO,
	HAL_RECEPTION_TYPE_UL_MU_MIMO,
	HAL_RECEPTION_TYPE_UL_MU_OFDMA,
	HAL_RECEPTION_TYPE_UL_MU_OFDMA_MIMO,
};

/* Different allowed RU in 11BE */
#define HAL_EHT_RU_26		0ULL
#define HAL_EHT_RU_52		1ULL
#define HAL_EHT_RU_78		2ULL
#define HAL_EHT_RU_106		3ULL
#define HAL_EHT_RU_132		4ULL
#define HAL_EHT_RU_242		5ULL
#define HAL_EHT_RU_484		6ULL
#define HAL_EHT_RU_726		7ULL
#define HAL_EHT_RU_996		8ULL
#define HAL_EHT_RU_996x2	9ULL
#define HAL_EHT_RU_996x3	10ULL
#define HAL_EHT_RU_996x4	11ULL
#define HAL_EHT_RU_NONE		15ULL
#define HAL_EHT_RU_INVALID	31ULL
/* MRUs spanning above 80Mhz
 * HAL_EHT_RU_996_484 = HAL_EHT_RU_484 + HAL_EHT_RU_996 + 4 (reserved)
 */
#define HAL_EHT_RU_996_484	18ULL
#define HAL_EHT_RU_996x2_484	28ULL
#define HAL_EHT_RU_996x3_484	40ULL
#define HAL_EHT_RU_996_484_242	23ULL

#define NUM_RU_BITS_PER80	16
#define NUM_RU_BITS_PER20	4

/* Different per_80Mhz band in 320Mhz bandwidth */
#define HAL_80_0	0
#define HAL_80_1	1
#define HAL_80_2	2
#define HAL_80_3	3

#define HAL_RU_80MHZ(num_band)		((num_band) * NUM_RU_BITS_PER80)
#define HAL_RU_20MHZ(idx_per_80)	((idx_per_80) * NUM_RU_BITS_PER20)

#define HAL_RU_SHIFT(num_band, idx_per_80)	\
		(HAL_RU_80MHZ(num_band) + HAL_RU_20MHZ(idx_per_80))

#define HAL_RU(ru, num_band, idx_per_80)	\
		((u64)(ru) << HAL_RU_SHIFT(num_band, idx_per_80))

/* MRU-996+484 */
#define HAL_EHT_RU_996_484_0	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 1) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_1, 0))
#define HAL_EHT_RU_996_484_1	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_1, 0))
#define HAL_EHT_RU_996_484_2	(HAL_RU(HAL_EHT_RU_996, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1))
#define HAL_EHT_RU_996_484_3	(HAL_RU(HAL_EHT_RU_996, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0))
#define HAL_EHT_RU_996_484_4	(HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_3, 0))
#define HAL_EHT_RU_996_484_5	(HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996, HAL_80_3, 0))
#define HAL_EHT_RU_996_484_6	(HAL_RU(HAL_EHT_RU_996, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 1))
#define HAL_EHT_RU_996_484_7	(HAL_RU(HAL_EHT_RU_996, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 0))

/* MRU-996x2+484 */
#define HAL_EHT_RU_996x2_484_0	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_1	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_2	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_3	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_4	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1))
#define HAL_EHT_RU_996x2_484_5	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0))
#define HAL_EHT_RU_996x2_484_6	(HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_7	(HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_8	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_9	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_3, 0))
#define HAL_EHT_RU_996x2_484_10	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 1))
#define HAL_EHT_RU_996x2_484_11	(HAL_RU(HAL_EHT_RU_996x2, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x2, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 0))

/* MRU-996x3+484 */
#define HAL_EHT_RU_996x3_484_0	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_1	(HAL_RU(HAL_EHT_RU_484, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_2	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_3	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_4	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 1) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_5	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_3, 0))
#define HAL_EHT_RU_996x3_484_6	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 1))
#define HAL_EHT_RU_996x3_484_7	(HAL_RU(HAL_EHT_RU_996x3, HAL_80_0, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_1, 0) |	\
				 HAL_RU(HAL_EHT_RU_996x3, HAL_80_2, 0) |	\
				 HAL_RU(HAL_EHT_RU_484, HAL_80_3, 0))

#define HAL_RU_PER80(ru_per80, num_80mhz, ru_idx_per80mhz) \
			(HAL_RU(ru_per80, num_80mhz, ru_idx_per80mhz))

/* info1 subfields */
#define HAL_RX_FSE_SRC_PORT			GENMASK(15, 0)
#define HAL_RX_FSE_DEST_PORT			GENMASK(31, 16)

/* info2 subfields */
#define HAL_RX_FSE_L4_PROTOCOL			GENMASK(7, 0)
#define HAL_RX_FSE_VALID			GENMASK(8, 8)
#define HAL_RX_FSE_RESERVED			GENMASK(12, 9)
#define HAL_RX_FSE_SERVICE_CODE			GENMASK(21, 13)
#define HAL_RX_FSE_PRIORITY_VLD			GENMASK(22, 22)
#define HAL_RX_FSE_USE_PPE			GENMASK(23, 23)
#define HAL_RX_FSE_REO_INDICATION		GENMASK(28, 24)
#define HAL_RX_FSE_MSDU_DROP			GENMASK(29, 29)
#define HAL_RX_FSE_REO_DESTINATION_HANDLER	GENMASK(31, 30)

/* info 3 subfields */
#define HAL_RX_FSE_AGGREGATION_COUNT		GENMASK(15, 0)
#define HAL_RX_FSE_LRO_ELIGIBLE			GENMASK(31, 16)
#define HAL_RX_FSE_MSDU_COUNT			GENMASK(31, 16)

/* info4 subfields */
#define HAL_RX_FSE_CUMULATIVE_IP_LEN1		GENMASK(15, 0)
#define HAL_RX_FSE_CUMULATIVE_IP_LEN		GENMASK(31, 16)

/* This structure should not be modified as it is shared with HW */
struct hal_rx_fse {
	u32 src_ip_127_96;
	u32 src_ip_95_64;
	u32 src_ip_63_32;
	u32 src_ip_31_0;
	u32 dest_ip_127_96;
	u32 dest_ip_95_64;
	u32 dest_ip_63_32;
	u32 dest_ip_31_0;
	u32 info1;
	u32 info2;
	u32 metadata;
	u32 info3;
	u32 msdu_byte_count;
	u32 timestamp;
	u32 info4;
	u32 tcp_sequence_number;
};

#define HAL_FST_HASH_DATA_SIZE		37
#define HAL_FST_HASH_KEY_SIZE_WORDS	10
#define HAL_RX_FST_MAX_SEARCH		16
#define HAL_RX_FLOW_SEARCH_TABLE_SIZE	2048
#define HAL_RX_FST_TOEPLITZ_KEYLEN	40
#define NUM_OF_DWORDS_RX_FLOW_SEARCH_ENTRY	16
#define HAL_RX_FST_ENTRY_SIZE		(NUM_OF_DWORDS_RX_FLOW_SEARCH_ENTRY * 4)
#define HAL_RX_FSE_REO_DEST_FT 0

struct hal_rx_flow {
	struct hal_flow_tuple_info tuple_info;
	u32 fse_metadata;
	u16 service_code;
	u8 reo_destination_handler;
	u8 reo_indication;
	u8 use_ppe      :1,
	   drop         :1;
};

void ath12k_wifi7_hal_reo_status_queue_stats(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi7_hal_reo_flush_queue_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi7_hal_reo_flush_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi7_hal_reo_unblk_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void
ath12k_wifi7_hal_reo_flush_timeout_list_status(struct ath12k_base *ab,
					       struct hal_tlv_64_hdr *tlv,
					       struct hal_reo_status *status);
void
ath12k_wifi7_hal_reo_desc_thresh_reached_status(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status);
void
ath12k_wifi7_hal_reo_update_rx_reo_queue_status(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status);
void
ath12k_wifi7_hal_rx_msdu_link_info_get(struct hal_rx_msdu_link *link,
				       u32 *num_msdus, u32 *msdu_cookies,
				       enum hal_rx_buf_return_buf_manager *rbm);
void
ath12k_wifi7_hal_rx_msdu_link_desc_set(struct ath12k_base *ab,
				       struct hal_wbm_release_ring *desc,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action);
int ath12k_wifi7_hal_desc_reo_parse_err(struct ath12k_dp *dp,
					struct hal_reo_dest_ring *desc,
					dma_addr_t *paddr, u32 *desc_bank);
int ath12k_wifi7_hal_wbm_desc_parse_err(struct ath12k_dp *dp, void *desc,
					struct hal_rx_wbm_rel_info *rel_info);
void ath12k_wifi7_hal_rx_reo_ent_paddr_get(struct ath12k_base *ab,
					   struct ath12k_buffer_addr *buff_addr,
					   dma_addr_t *paddr, u32 *cookie);
void ath12k_wifi7_hal_reo_init_cmd_ring(struct ath12k_base *ab,
					struct hal_srng *srng);
void ath12k_wifi7_hal_reo_hw_setup(struct ath12k_base *ab);
void ath12k_wifi7_hal_reo_qdesc_setup(struct hal_rx_reo_queue *qdesc,
				      int tid, u32 ba_window_size,
				      u32 start_seq, enum hal_pn_type type);
u32 ath12k_wifi7_hal_rx_get_trunc_hash(struct hal_rx_fst *fst, u32 hash);
u32 ath12k_wifi7_hal_flow_toeplitz_hash(struct ath12k_base *ab,
					struct hal_rx_fst *fst,
					struct hal_flow_tuple_info *tuple_info);
int ath12k_wifi7_hal_rx_find_flow_from_tuple(struct ath12k_base *ab,
					     struct hal_rx_fst *fst,
					     u32 flow_hash,
					     void *flow_tuple_info,
					     u32 *flow_idx);
ssize_t ath12k_wifi7_hal_rx_dump_fst_table(struct ath12k_base *ab,
					   struct hal_rx_fst *fst,
					   char *buf, int size);
struct hal_rx_fst *ath12k_wifi7_hal_rx_fst_attach(struct ath12k_base *ab);
void ath12k_wifi7_hal_rx_fst_detach(struct ath12k_base *ab, struct hal_rx_fst *fst);
int ath12k_wifi7_hal_rx_flow_insert_entry(struct ath12k_base *ab,
					  struct hal_rx_fst *fst,
					  u32 flow_hash,
					  void *flow_tuple_info,
					  u32 *flow_idx);
void *ath12k_wifi7_hal_rx_flow_setup_fse(struct ath12k_base *ab,
					 struct hal_rx_fst *fst,
					 u32 table_offset,
					 struct hal_rx_flow *flow);
void ath12k_wifi7_hal_rx_flow_delete_entry(struct ath12k_base *ab,
					   struct hal_rx_fse *hal_fse);
void ath12k_wifi7_hal_reset_rx_reo_tid_q(void *vaddr,
					 u32 ba_window_size, u8 tid);
void ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab);
void ath12k_hal_qcn9274_get_tsf2_scratch_reg(struct ath12k_base *ab,
					     u8 mac_id, u64 *value);
void ath12k_hal_qcn9274_get_tqm_scratch_reg(struct ath12k_base *ab, u64 *value);
void ath12k_wifi7_hal_rx_msdu_list_get(void *desc,
				       void *list,
				       u16 *num_msdus);
void ath12k_wifi7_hal_rx_reo_ent_buf_paddr_get(void *rx_desc, dma_addr_t *paddr,
					       u32 *sw_cookie,
					       struct ath12k_buffer_addr **pp_buf_addr,
					       u8 *rbm, u32 *msdu_cnt);
#endif
