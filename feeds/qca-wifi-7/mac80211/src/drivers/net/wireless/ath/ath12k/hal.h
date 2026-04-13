/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_HAL_H
#define ATH12K_HAL_H

#include "hw.h"
#include "mac.h"
#include "core.h"

struct ath12k_base;
struct ath12k_dp;
struct hal_rx_reo_queue;
struct hal_rx_spd_data;
struct rx_mpdu_desc_info;

#define HAL_CE_REMAP_REG_BASE  (ab->ce_remap_base_addr)

#define HAL_RING_BASE_ALIGN	8
#define HAL_REO_QLUT_ADDR_ALIGN 256

#define HAL_LINK_DESC_ALIGN			128
#define HAL_MAX_AVAIL_BLK_RES			3

/* SRNG registers are split into two groups R0 and R2 */
#define HAL_SRNG_REG_GRP_R0	0
#define HAL_SRNG_REG_GRP_R2	1
#define HAL_SRNG_NUM_REG_GRP    2

#define HAL_ADDR_LSB_REG_MASK		0xffffffff
#define HAL_ADDR_MSB_REG_SHIFT		32

#define HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_SHFT 10
#define HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_PPE2TCL1_RNG_HALT_STAT_SHFT  18
#define RING_HALT_TIMEOUT 10
#define RNG_HALT_STAT_RETRY_COUNT 10

#define HAL_WBM2SW_REL_ERR_RING_NUM 5
#define HAL_WBM2SW_PPEDS_TX_CMPLN_MAP_ID 11
#define HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM 6
#define HAL_TX_ADDRX_EN		1
#define HAL_TX_ADDRY_EN		2

#define HAL_TX_ADDR_SEARCH_DEFAULT	0
#define HAL_TX_ADDR_SEARCH_INDEX	1

#define HAL_SHADOW_NUM_REGS_MAX			40

#define HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX	32704
/* TODO: Check with hw team on the supported scatter buf size */
#define HAL_WBM_IDLE_SCATTER_NEXT_PTR_SIZE	8
#define HAL_WBM_IDLE_SCATTER_BUF_SIZE (HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX - \
				       HAL_WBM_IDLE_SCATTER_NEXT_PTR_SIZE)

/* TODO: 16 entries per radio times MAX_VAPS_SUPPORTED */
#define HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX	32
#define HAL_DSCP_TID_TBL_SIZE			24
#define DSCP_TID_MAP_TBL_ENTRY_SIZE		64

#define RU_INVALID		0
#define RU_26			1
#define RU_52			2
#define RU_106			4
#define RU_242			9
#define RU_484			18
#define RU_996			37
#define RU_2X996		74
#define RU_3X996		111
#define RU_4X996		148
#define RU_52_26		(RU_52 + RU_26)
#define RU_106_26		(RU_106 + RU_26)
#define RU_484_242		(RU_484 + RU_242)
#define RU_996_484		(RU_996 + RU_484)
#define RU_996_484_242		(RU_996 + RU_484_242)
#define RU_2X996_484		(RU_2X996 + RU_484)
#define RU_3X996_484		(RU_3X996 + RU_484)

/* TODO: number of PMACs */
#define HAL_SRNG_NUM_PMACS      3
#define HAL_SRNG_NUM_DMAC_RINGS (HAL_SRNG_RING_ID_DMAC_CMN_ID_END - \
				 HAL_SRNG_RING_ID_DMAC_CMN_ID_START)
#define HAL_SRNG_RINGS_PER_PMAC (HAL_SRNG_RING_ID_PMAC1_ID_END - \
				 HAL_SRNG_RING_ID_PMAC1_ID_START)
#define HAL_SRNG_NUM_PMAC_RINGS (HAL_SRNG_NUM_PMACS * HAL_SRNG_RINGS_PER_PMAC)
#define HAL_SRNG_RING_ID_MAX    (HAL_SRNG_RING_ID_DMAC_CMN_ID_END + \
				 HAL_SRNG_NUM_PMAC_RINGS)

#define HAL_AST_IDX_INVALID	0xFFFF
#define HAL_RX_MAX_MCS		12
#define HAL_RX_MAX_MCS_HT	31
#define HAL_RX_MAX_MCS_VHT	9
#define HAL_RX_MAX_MCS_HE	11
#define HAL_RX_MAX_MCS_BE	15
#define HAL_RX_MAX_NSS		8
#define HAL_RX_MAX_NUM_LEGACY_RATES 12

#define HAL_ENCRYPT_TYPE_MAX	12

/* To set mcast pkt ctrl vlaues */
#define HAL_TCL_R0_VDEV_MCAST_PACKET_CTRL_MAP_n_ADDR(vdev_id) (0x00A4414C + (0x4 * (vdev_id)))
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_REG_ID(vdev_id) ((vdev_id) >> 0x4)
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_INDEX_IN_REG(vdev_id) ((vdev_id) & 0xF)
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK 0x3
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT 0x2

enum ath12k_hal_tx_pkt_ctrl_config {
       HAL_TX_PACKET_CONTROL_CONFIG_TO_FW_EXCEPTION,
       HAL_TX_PACKET_CONTROL_CONFIG_DROP_PKT,
       HAL_TX_PACKET_CONTROL_CONFIG_MEC_NOTIFY,
       HAL_TX_PACKET_CONTROL_CONFIG_PKT_TO_TQM,
};

enum hal_rx_su_mu_coding {
	HAL_RX_SU_MU_CODING_BCC,
	HAL_RX_SU_MU_CODING_LDPC,
	HAL_RX_SU_MU_CODING_MAX,
};

enum hal_rx_gi {
	HAL_RX_GI_0_8_US,
	HAL_RX_GI_0_4_US,
	HAL_RX_GI_1_6_US,
	HAL_RX_GI_3_2_US,
	HAL_RX_GI_MAX,
};

enum hal_rx_bw {
	HAL_RX_BW_20MHZ,
	HAL_RX_BW_40MHZ,
	HAL_RX_BW_80MHZ,
	HAL_RX_BW_160MHZ,
	HAL_RX_BW_240MHZ,
	HAL_RX_BW_320MHZ,
	HAL_RX_BW_MAX,
};

enum hal_rx_preamble {
	HAL_RX_PREAMBLE_11A,
	HAL_RX_PREAMBLE_11B,
	HAL_RX_PREAMBLE_11N,
	HAL_RX_PREAMBLE_11AC,
	HAL_RX_PREAMBLE_11AX,
	HAL_RX_PREAMBLE_11BA,
	HAL_RX_PREAMBLE_11BE,
	HAL_RX_PREAMBLE_MAX,
};

enum hal_rx_reception_type {
	HAL_RX_RECEPTION_TYPE_SU,
	HAL_RX_RECEPTION_TYPE_MU_MIMO,
	HAL_RX_RECEPTION_TYPE_MU_OFDMA,
	HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO,
	HAL_RX_RECEPTION_TYPE_MAX,
};

enum hal_rx_legacy_rate {
	HAL_RX_LEGACY_RATE_1_MBPS,
	HAL_RX_LEGACY_RATE_2_MBPS,
	HAL_RX_LEGACY_RATE_5_5_MBPS,
	HAL_RX_LEGACY_RATE_6_MBPS,
	HAL_RX_LEGACY_RATE_9_MBPS,
	HAL_RX_LEGACY_RATE_11_MBPS,
	HAL_RX_LEGACY_RATE_12_MBPS,
	HAL_RX_LEGACY_RATE_18_MBPS,
	HAL_RX_LEGACY_RATE_24_MBPS,
	HAL_RX_LEGACY_RATE_36_MBPS,
	HAL_RX_LEGACY_RATE_48_MBPS,
	HAL_RX_LEGACY_RATE_54_MBPS,
	HAL_RX_LEGACY_RATE_INVALID,
};

enum hal_srng_ring_id {
	HAL_SRNG_RING_ID_REO2SW0 = 0,
	HAL_SRNG_RING_ID_REO2SW1,
	HAL_SRNG_RING_ID_REO2SW2,
	HAL_SRNG_RING_ID_REO2SW3,
	HAL_SRNG_RING_ID_REO2SW4,
	HAL_SRNG_RING_ID_REO2SW5,
	HAL_SRNG_RING_ID_REO2SW6,
	HAL_SRNG_RING_ID_REO2SW7,
	HAL_SRNG_RING_ID_REO2SW8,
	HAL_SRNG_RING_ID_REO2TCL,
	HAL_SRNG_RING_ID_REO2PPE,

	HAL_SRNG_RING_ID_SW2REO  = 16,
	HAL_SRNG_RING_ID_SW2REO1,
	HAL_SRNG_RING_ID_SW2REO2,
	HAL_SRNG_RING_ID_SW2REO3,

	HAL_SRNG_RING_ID_REO_CMD,
	HAL_SRNG_RING_ID_REO_STATUS,

	HAL_SRNG_RING_ID_SW2TCL1 = 24,
	HAL_SRNG_RING_ID_SW2TCL2,
	HAL_SRNG_RING_ID_SW2TCL3,
	HAL_SRNG_RING_ID_SW2TCL4,
	HAL_SRNG_RING_ID_SW2TCL5,
	HAL_SRNG_RING_ID_SW2TCL6,
	HAL_SRNG_RING_ID_PPE2TCL1 = 30,

	HAL_SRNG_RING_ID_SW2TCL_CMD = 40,
	HAL_SRNG_RING_ID_SW2TCL1_CMD,
	HAL_SRNG_RING_ID_TCL_STATUS,

	HAL_SRNG_RING_ID_CE0_SRC = 64,
	HAL_SRNG_RING_ID_CE1_SRC,
	HAL_SRNG_RING_ID_CE2_SRC,
	HAL_SRNG_RING_ID_CE3_SRC,
	HAL_SRNG_RING_ID_CE4_SRC,
	HAL_SRNG_RING_ID_CE5_SRC,
	HAL_SRNG_RING_ID_CE6_SRC,
	HAL_SRNG_RING_ID_CE7_SRC,
	HAL_SRNG_RING_ID_CE8_SRC,
	HAL_SRNG_RING_ID_CE9_SRC,
	HAL_SRNG_RING_ID_CE10_SRC,
	HAL_SRNG_RING_ID_CE11_SRC,
	HAL_SRNG_RING_ID_CE12_SRC,
	HAL_SRNG_RING_ID_CE13_SRC,
	HAL_SRNG_RING_ID_CE14_SRC,
	HAL_SRNG_RING_ID_CE15_SRC,

	HAL_SRNG_RING_ID_CE0_DST = 81,
	HAL_SRNG_RING_ID_CE1_DST,
	HAL_SRNG_RING_ID_CE2_DST,
	HAL_SRNG_RING_ID_CE3_DST,
	HAL_SRNG_RING_ID_CE4_DST,
	HAL_SRNG_RING_ID_CE5_DST,
	HAL_SRNG_RING_ID_CE6_DST,
	HAL_SRNG_RING_ID_CE7_DST,
	HAL_SRNG_RING_ID_CE8_DST,
	HAL_SRNG_RING_ID_CE9_DST,
	HAL_SRNG_RING_ID_CE10_DST,
	HAL_SRNG_RING_ID_CE11_DST,
	HAL_SRNG_RING_ID_CE12_DST,
	HAL_SRNG_RING_ID_CE13_DST,
	HAL_SRNG_RING_ID_CE14_DST,
	HAL_SRNG_RING_ID_CE15_DST,

	HAL_SRNG_RING_ID_CE0_DST_STATUS = 100,
	HAL_SRNG_RING_ID_CE1_DST_STATUS,
	HAL_SRNG_RING_ID_CE2_DST_STATUS,
	HAL_SRNG_RING_ID_CE3_DST_STATUS,
	HAL_SRNG_RING_ID_CE4_DST_STATUS,
	HAL_SRNG_RING_ID_CE5_DST_STATUS,
	HAL_SRNG_RING_ID_CE6_DST_STATUS,
	HAL_SRNG_RING_ID_CE7_DST_STATUS,
	HAL_SRNG_RING_ID_CE8_DST_STATUS,
	HAL_SRNG_RING_ID_CE9_DST_STATUS,
	HAL_SRNG_RING_ID_CE10_DST_STATUS,
	HAL_SRNG_RING_ID_CE11_DST_STATUS,
	HAL_SRNG_RING_ID_CE12_DST_STATUS,
	HAL_SRNG_RING_ID_CE13_DST_STATUS,
	HAL_SRNG_RING_ID_CE14_DST_STATUS,
	HAL_SRNG_RING_ID_CE15_DST_STATUS,

	HAL_SRNG_RING_ID_WBM_IDLE_LINK = 120,
	HAL_SRNG_RING_ID_WBM_SW0_RELEASE,
	HAL_SRNG_RING_ID_WBM_SW1_RELEASE,
	HAL_SRNG_RING_ID_WBM_PPE_RELEASE = 123,

	HAL_SRNG_RING_ID_WBM2SW0_RELEASE = 128,
	HAL_SRNG_RING_ID_WBM2SW1_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW2_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW3_RELEASE, /* RX ERROR RING */
	HAL_SRNG_RING_ID_WBM2SW4_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW5_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW6_RELEASE,
	HAL_SRNG_RING_ID_WBM2SW7_RELEASE,

	HAL_SRNG_RING_ID_UMAC_ID_END = 159,

	/* Common DMAC rings shared by all LMACs */
	HAL_SRNG_RING_ID_DMAC_CMN_ID_START = 160,
	HAL_SRNG_SW2RXDMA_BUF0 = HAL_SRNG_RING_ID_DMAC_CMN_ID_START,
	HAL_SRNG_SW2RXDMA_BUF1 = 161,
	HAL_SRNG_SW2RXDMA_BUF2 = 162,

	HAL_SRNG_SW2RXMON_BUF0 = 168,

	HAL_SRNG_SW2TXMON_BUF0 = 176,

	HAL_SRNG_RING_ID_DMAC_CMN_ID_END = 183,
	HAL_SRNG_RING_ID_PMAC1_ID_START = 184,

	HAL_SRNG_RING_ID_WMAC1_SW2RXMON_BUF0 = HAL_SRNG_RING_ID_PMAC1_ID_START,

	HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_STATBUF,
	HAL_SRNG_RING_ID_WMAC1_RXDMA2SW0,
	HAL_SRNG_RING_ID_WMAC1_RXDMA2SW1,
	HAL_SRNG_RING_ID_WMAC1_RXMON2SW0 = HAL_SRNG_RING_ID_WMAC1_RXDMA2SW1,
	HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_DESC,
	HAL_SRNG_RING_ID_RXDMA_DIR_BUF,
	HAL_SRNG_RING_ID_WMAC1_TXMON2SW0_BUF0,
	HAL_SRNG_RING_ID_WMAC1_SW2TXMON_BUF0,

	HAL_SRNG_RING_ID_PMAC1_ID_END,
};

enum hal_ring_type {
	HAL_REO_DST,
	HAL_REO_EXCEPTION,
	HAL_REO_REINJECT,
	HAL_REO_CMD,
	HAL_REO_STATUS,
	HAL_REO2PPE,
	HAL_TCL_DATA,
	HAL_TCL_CMD,
	HAL_TCL_STATUS,
	HAL_CE_SRC,
	HAL_CE_DST,
	HAL_CE_DST_STATUS,
	HAL_WBM_IDLE_LINK,
	HAL_SW2WBM_RELEASE,
	HAL_WBM2SW_RELEASE,
	HAL_RXDMA_BUF,
	HAL_RXDMA_DST,
	HAL_RXDMA_MONITOR_BUF,
	HAL_RXDMA_MONITOR_STATUS,
	HAL_RXDMA_MONITOR_DST,
	HAL_RXDMA_MONITOR_DESC,
	HAL_RXDMA_DIR_BUF,
	HAL_PPE2TCL,
	HAL_PPE_RELEASE,
	HAL_TX_MONITOR_BUF,
	HAL_TX_MONITOR_DST,
	HAL_MAX_RING_TYPES,
};

#define PMM_REG_BASE_QCN9224    0xB500FC
#define HAL_IPQ5332_PMM_REG_BASE       0xCB500FC
#define HAL_IPQ5332_PMM_SIZE           0x100

enum hal_scratch_reg_enum {
	PMM_QTIMER_GLOBAL_OFFSET_LO_US,
	PMM_QTIMER_GLOBAL_OFFSET_HI_US,
	PMM_MAC0_TSF1_OFFSET_LO_US,
	PMM_MAC0_TSF1_OFFSET_HI_US,
	PMM_MAC0_TSF2_OFFSET_LO_US,
	PMM_MAC0_TSF2_OFFSET_HI_US,
	PMM_MAC1_TSF1_OFFSET_LO_US,
	PMM_MAC1_TSF1_OFFSET_HI_US,
	PMM_MAC1_TSF2_OFFSET_LO_US,
	PMM_MAC1_TSF2_OFFSET_HI_US,
	PMM_MLO_OFFSET_LO_US,
	PMM_MLO_OFFSET_HI_US,
	PMM_TQM_CLOCK_OFFSET_LO_US,
	PMM_TQM_CLOCK_OFFSET_HI_US,
	PMM_Q6_CRASH_REASON,
	PMM_SCRATCH_TWT_OFFSET,
	PMM_PMM_REG_MAX
};

/**
 * enum hal_reo_cmd_type: Enum for REO command type
 * @HAL_REO_CMD_GET_QUEUE_STATS: Get REO queue status/stats
 * @HAL_REO_CMD_FLUSH_QUEUE: Flush all frames in REO queue
 * @HAL_REO_CMD_FLUSH_CACHE: Flush descriptor entries in the cache
 * @HAL_REO_CMD_UNBLOCK_CACHE: Unblock a descriptor's address that was blocked
 *      earlier with a 'REO_FLUSH_CACHE' command
 * @HAL_REO_CMD_FLUSH_TIMEOUT_LIST: Flush buffers/descriptors from timeout list
 * @HAL_REO_CMD_UPDATE_RX_QUEUE: Update REO queue settings
 */
enum hal_reo_cmd_type {
	HAL_REO_CMD_GET_QUEUE_STATS     = 0,
	HAL_REO_CMD_FLUSH_QUEUE         = 1,
	HAL_REO_CMD_FLUSH_CACHE         = 2,
	HAL_REO_CMD_UNBLOCK_CACHE       = 3,
	HAL_REO_CMD_FLUSH_TIMEOUT_LIST  = 4,
	HAL_REO_CMD_UPDATE_RX_QUEUE     = 5,
};

enum hal_reo_dest_ring_error_code {
	HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO,
	HAL_REO_DEST_RING_ERROR_CODE_DESC_INVALID,
	HAL_REO_DEST_RING_ERROR_CODE_AMPDU_IN_NON_BA,
	HAL_REO_DEST_RING_ERROR_CODE_NON_BA_DUPLICATE,
	HAL_REO_DEST_RING_ERROR_CODE_BA_DUPLICATE,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_2K_JUMP,
	HAL_REO_DEST_RING_ERROR_CODE_BAR_2K_JUMP,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_OOR,
	HAL_REO_DEST_RING_ERROR_CODE_BAR_OOR,
	HAL_REO_DEST_RING_ERROR_CODE_NO_BA_SESSION,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_SN_EQUALS_SSN,
	HAL_REO_DEST_RING_ERROR_CODE_PN_CHECK_FAILED,
	HAL_REO_DEST_RING_ERROR_CODE_2K_ERR_FLAG_SET,
	HAL_REO_DEST_RING_ERROR_CODE_PN_ERR_FLAG_SET,
	HAL_REO_DEST_RING_ERROR_CODE_DESC_BLOCKED,
	HAL_REO_DEST_RING_ERROR_CODE_MAX,
};

enum hal_reo_entr_rxdma_ecode {
	HAL_REO_ENTR_RING_RXDMA_ECODE_OVERFLOW_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MPDU_LEN_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FCS_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_DECRYPT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_UNECRYPTED_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MSDU_LEN_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MSDU_LIMIT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_WIFI_PARSE_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_AMSDU_PARSE_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_SA_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_DA_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FLOW_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FLUSH_REQUEST_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_AMSDU_FRAG_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MULTICAST_ECHO_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_AMSDU_ADDR_MISMATCH_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_UNAUTH_WDS_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_GROUPCAST_AMSDU_OR_WDS_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MAX,
};

enum hal_encrypt_type {
	HAL_ENCRYPT_TYPE_WEP_40,
	HAL_ENCRYPT_TYPE_WEP_104,
	HAL_ENCRYPT_TYPE_TKIP_NO_MIC,
	HAL_ENCRYPT_TYPE_WEP_128,
	HAL_ENCRYPT_TYPE_TKIP_MIC,
	HAL_ENCRYPT_TYPE_WAPI,
	HAL_ENCRYPT_TYPE_CCMP_128,
	HAL_ENCRYPT_TYPE_OPEN,
	HAL_ENCRYPT_TYPE_CCMP_256,
	HAL_ENCRYPT_TYPE_GCMP_128,
	HAL_ENCRYPT_TYPE_AES_GCMP_256,
	HAL_ENCRYPT_TYPE_WAPI_GCM_SM4,
};

enum hal_wbm_htt_tx_comp_status {
	HAL_WBM_REL_HTT_TX_COMP_STATUS_OK,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX,
};

/* hal_wbm_link_desc
 *
 *	Producer: WBM
 *	Consumer: WBM
 *
 * buf_addr_info
 *		Details of the physical address of a buffer or MSDU
 *		link descriptor.
 */

enum hal_wbm_rel_src_module {
	HAL_WBM_REL_SRC_MODULE_TQM,
	HAL_WBM_REL_SRC_MODULE_RXDMA,
	HAL_WBM_REL_SRC_MODULE_REO,
	HAL_WBM_REL_SRC_MODULE_FW,
	HAL_WBM_REL_SRC_MODULE_SW,
	HAL_WBM_REL_SRC_MODULE_MAX,
};

enum hal_tcl_encap_type {
	HAL_TCL_ENCAP_TYPE_RAW,
	HAL_TCL_ENCAP_TYPE_NATIVE_WIFI,
	HAL_TCL_ENCAP_TYPE_ETHERNET,
	HAL_TCL_ENCAP_TYPE_802_3 = 3,
	HAL_TCL_ENCAP_TYPE_MAX,
};

/**
 * enum hal_reo_cmd_status: Enum for execution status of REO command
 * @HAL_REO_CMD_SUCCESS: Command has successfully executed
 * @HAL_REO_CMD_BLOCKED: Command could not be executed as the queue
 *			 or cache was blocked
 * @HAL_REO_CMD_FAILED: Command execution failed, could be due to
 *			invalid queue desc
 * @HAL_REO_CMD_RESOURCE_BLOCKED:
 * @HAL_REO_CMD_DRAIN:
 */
enum hal_reo_cmd_status {
	HAL_REO_CMD_SUCCESS		= 0,
	HAL_REO_CMD_BLOCKED		= 1,
	HAL_REO_CMD_FAILED		= 2,
	HAL_REO_CMD_RESOURCE_BLOCKED	= 3,
	HAL_REO_CMD_DRAIN		= 0xff,
};

/**
 * enum hal_wbm_tqm_rel_reason - TQM release reason code
 * @HAL_WBM_TQM_REL_REASON_FRAME_ACKED: ACK or BACK received for the frame
 * @HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU: Command remove_mpdus initiated by SW
 * @HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX: Command remove transmitted_mpdus
 *	initiated by sw.
 * @HAL_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX: Command remove untransmitted_mpdus
 *	initiated by sw.
 * @HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES: Command remove aged msdus or
 *	mpdus.
 * @HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1: Remove command initiated by
 *	fw with fw_reason1.
 * @HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2: Remove command initiated by
 *	fw with fw_reason2.
 * @HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3: Remove command initiated by
 *	fw with fw_reason3.
 * @HAL_WBM_TQM_REL_REASON_CMD_DISABLE_QUEUE: Remove command initiated by
 *	fw with disable queue.
 * @HAL_WBM_TQM_REL_REASON_CMD_TILL_NONMATCHING: Remove command initiated by
 *	fw to remove all mpdu until 1st non-match.
 * @HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD: Dropped due to drop threshold
 *	criteria
 * @HAL_WBM_TQM_REL_REASON_DROP_LINK_DESC_UNAVAIL: Dropped due to link desc
 *	not available
 * @HAL_WBM_TQM_REL_REASON_DROP_OR_INVALID_MSDU: Dropped due drop bit set or
 *	null flow
 * @HAL_WBM_TQM_REL_REASON_MULTICAST_DROP: Dropped due mcast drop set for VDEV
 * @HAL_WBM_TQM_REL_REASON_VDEV_MISMATCH_DROP: Dropped due to being set with
 *	'TCL_drop_reason'
 */
enum hal_wbm_tqm_rel_reason {
	HAL_WBM_TQM_REL_REASON_FRAME_ACKED,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3,
	HAL_WBM_TQM_REL_REASON_CMD_DISABLE_QUEUE,
	HAL_WBM_TQM_REL_REASON_CMD_TILL_NONMATCHING,
	HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD,
	HAL_WBM_TQM_REL_REASON_DROP_LINK_DESC_UNAVAIL,
	HAL_WBM_TQM_REL_REASON_DROP_OR_INVALID_MSDU,
	HAL_WBM_TQM_REL_REASON_MULTICAST_DROP,
	HAL_WBM_TQM_REL_REASON_VDEV_MISMATCH_DROP,

	/* Keep Last */
	HAL_WBM_TQM_REL_REASON_MAX,
};

enum hal_tx_rate_stats_pkt_type {
	HAL_TX_RATE_STATS_PKT_TYPE_11A,
	HAL_TX_RATE_STATS_PKT_TYPE_11B,
	HAL_TX_RATE_STATS_PKT_TYPE_11N,
	HAL_TX_RATE_STATS_PKT_TYPE_11AC,
	HAL_TX_RATE_STATS_PKT_TYPE_11AX,
	HAL_TX_RATE_STATS_PKT_TYPE_11BA,
	HAL_TX_RATE_STATS_PKT_TYPE_11BE,
};

enum hal_tx_rate_stats_sgi {
	HAL_TX_RATE_STATS_SGI_08US,
	HAL_TX_RATE_STATS_SGI_04US,
	HAL_TX_RATE_STATS_SGI_16US,
	HAL_TX_RATE_STATS_SGI_32US,
};

struct hal_tx_status {
	enum hal_wbm_rel_src_module buf_rel_source;
	enum hal_wbm_tqm_rel_reason status;
	s8 ack_rssi;
	u32 flags; /* %HAL_TX_STATUS_FLAGS_ */
	u32 ppdu_id;
	u8 try_cnt;
	u8 tid;
	u16 peer_id;
	enum hal_tx_rate_stats_pkt_type pkt_type;
	enum hal_tx_rate_stats_sgi sgi;
	enum ath12k_supported_bw bw;
	u8 mcs;
	u16 tones;
	u8 ofdma;
	bool acked;
	u32 buffer_timestamp;
	u32 tsf;
	u8 transmit_cnt;
	u8 first_msdu;
	u8 last_msdu;
	u8 msdu_part_of_amsdu;
	u8 hw_link_id;
};

struct hal_srng_params {
	dma_addr_t ring_base_paddr;
	u32 *ring_base_vaddr;
	int num_entries;
	u32 intr_batch_cntr_thres_entries;
	u32 intr_timer_thres_us;
	u32 flags;
	u32 max_buffer_len;
	u32 low_threshold;
	u32 high_threshold;
	dma_addr_t msi_addr;
	dma_addr_t msi2_addr;
	u32 msi_data;
	u32 msi2_data;

	/* Add more params as needed */
};

enum hal_srng_dir {
	HAL_SRNG_DIR_SRC,
	HAL_SRNG_DIR_DST
};

struct ath12k_hal_reo_cmd {
	u32 addr_lo;
	u32 flag;
	u32 upd0;
	u32 upd1;
	u32 upd2;
	u32 pn[4];
	u16 rx_queue_num;
	u16 min_rel;
	u16 min_fwd;
	u8 addr_hi;
	u8 ac_list;
	u8 blocking_idx;
	u16 ba_window_size;
	u8 pn_size;
};

enum rx_msdu_start_pkt_type {
	RX_MSDU_START_PKT_TYPE_11A,
	RX_MSDU_START_PKT_TYPE_11B,
	RX_MSDU_START_PKT_TYPE_11N,
	RX_MSDU_START_PKT_TYPE_11AC,
	RX_MSDU_START_PKT_TYPE_11AX,
	RX_MSDU_START_PKT_TYPE_11BA,
	RX_MSDU_START_PKT_TYPE_11BE,
};

enum rx_msdu_start_sgi {
	RX_MSDU_START_SGI_0_8_US,
	RX_MSDU_START_SGI_0_4_US,
	RX_MSDU_START_SGI_1_6_US,
	RX_MSDU_START_SGI_3_2_US,
};

enum rx_msdu_start_recv_bw {
	RX_MSDU_START_RECV_BW_20MHZ,
	RX_MSDU_START_RECV_BW_40MHZ,
	RX_MSDU_START_RECV_BW_80MHZ,
	RX_MSDU_START_RECV_BW_160MHZ,
};

enum rx_msdu_start_reception_type {
	RX_MSDU_START_RECEPTION_TYPE_SU,
	RX_MSDU_START_RECEPTION_TYPE_DL_MU_MIMO,
	RX_MSDU_START_RECEPTION_TYPE_DL_MU_OFDMA,
	RX_MSDU_START_RECEPTION_TYPE_DL_MU_OFDMA_MIMO,
	RX_MSDU_START_RECEPTION_TYPE_UL_MU_MIMO,
	RX_MSDU_START_RECEPTION_TYPE_UL_MU_OFDMA,
	RX_MSDU_START_RECEPTION_TYPE_UL_MU_OFDMA_MIMO,
};

struct hal_rx_radiotap_eht {
	__le32 known;
	__le32 data[9];
};

struct hal_rx_desc_data {
	u32 freq;
	u32 err_bitmap;
	u32 enctype;
	u32 msdu_done:1,
	    is_decrypted:1,
	    ip_csum_fail:1,
	    l4_csum_fail:1,
	    is_first_msdu:1,
	    is_last_msdu:1,
	    mesh_ctrl_present:1,
	    fill_crypto_hdr:1,
	    seq_ctl_valid:1,
	    fc_valid:1,
	    is_ip_valid:1;
	u16 msdu_len;
	u16 peer_id;
	u16 seq_no;
	u8 pkt_type;
	u8 l3_pad_bytes;
	u8 decap;
	u8 bw;
	u8 rate_mcs;
	u8 nss;
	u8 sgi;
	u8 tid;
	bool is_mcbc;
	u8 is_to_ds:1,
	   is_from_ds:1;
};

#define BUFFER_ADDR_INFO0_ADDR         GENMASK(31, 0)

#define BUFFER_ADDR_INFO1_ADDR         GENMASK(7, 0)
#define BUFFER_ADDR_INFO1_RET_BUF_MGR  GENMASK(11, 8)
#define BUFFER_ADDR_INFO1_SW_COOKIE    GENMASK(31, 12)

struct ath12k_buffer_addr {
	__le32 info0;
	__le32 info1;
} __packed;

/* ath12k_buffer_addr
 *
 * buffer_addr_31_0
 *		Address (lower 32 bits) of the MSDU buffer or MSDU_EXTENSION
 *		descriptor or Link descriptor
 *
 * buffer_addr_39_32
 *		Address (upper 8 bits) of the MSDU buffer or MSDU_EXTENSION
 *		descriptor or Link descriptor
 *
 * return_buffer_manager (RBM)
 *		Consumer: WBM
 *		Producer: SW/FW
 *		Indicates to which buffer manager the buffer or MSDU_EXTENSION
 *		descriptor or link descriptor that is being pointed to shall be
 *		returned after the frame has been processed. It is used by WBM
 *		for routing purposes.
 *
 *		Values are defined in enum %HAL_RX_BUF_RBM_
 *
 * sw_buffer_cookie
 *		Cookie field exclusively used by SW. HW ignores the contents,
 *		accept that it passes the programmed value on to other
 *		descriptors together with the physical address.
 *
 *		Field can be used by SW to for example associate the buffers
 *		physical address with the virtual address.
 *
 *		NOTE1:
 *		The three most significant bits can have a special meaning
 *		 in case this struct is embedded in a TX_MPDU_DETAILS STRUCT,
 *		and field transmit_bw_restriction is set
 *
 *		In case of NON punctured transmission:
 *		Sw_buffer_cookie[19:17] = 3'b000: 20 MHz TX only
 *		Sw_buffer_cookie[19:17] = 3'b001: 40 MHz TX only
 *		Sw_buffer_cookie[19:17] = 3'b010: 80 MHz TX only
 *		Sw_buffer_cookie[19:17] = 3'b011: 160 MHz TX only
 *		Sw_buffer_cookie[19:17] = 3'b101: 240 MHz TX only
 *		Sw_buffer_cookie[19:17] = 3'b100: 320 MHz TX only
 *		Sw_buffer_cookie[19:18] = 2'b11: reserved
 *
 *		In case of punctured transmission:
 *		Sw_buffer_cookie[19:16] = 4'b0000: pattern 0 only
 *		Sw_buffer_cookie[19:16] = 4'b0001: pattern 1 only
 *		Sw_buffer_cookie[19:16] = 4'b0010: pattern 2 only
 *		Sw_buffer_cookie[19:16] = 4'b0011: pattern 3 only
 *		Sw_buffer_cookie[19:16] = 4'b0100: pattern 4 only
 *		Sw_buffer_cookie[19:16] = 4'b0101: pattern 5 only
 *		Sw_buffer_cookie[19:16] = 4'b0110: pattern 6 only
 *		Sw_buffer_cookie[19:16] = 4'b0111: pattern 7 only
 *		Sw_buffer_cookie[19:16] = 4'b1000: pattern 8 only
 *		Sw_buffer_cookie[19:16] = 4'b1001: pattern 9 only
 *		Sw_buffer_cookie[19:16] = 4'b1010: pattern 10 only
 *		Sw_buffer_cookie[19:16] = 4'b1011: pattern 11 only
 *		Sw_buffer_cookie[19:18] = 2'b11: reserved
 *
 *		Note: a punctured transmission is indicated by the presence
 *		 of TLV TX_PUNCTURE_SETUP embedded in the scheduler TLV
 *
 *		Sw_buffer_cookie[20:17]: Tid: The TID field in the QoS control
 *		 field
 *
 *		Sw_buffer_cookie[16]: Mpdu_qos_control_valid: This field
 *		 indicates MPDUs with a QoS control field.
 *
 */

struct hal_ce_srng_dest_desc;

struct hal_ce_srng_dst_status_desc;

struct hal_ce_srng_src_desc;

struct wbm_link_desc {
        struct ath12k_buffer_addr buf_addr_info;
} __packed;

struct wbm_idle_scatter_list {
        dma_addr_t paddr;
        struct wbm_link_desc *vaddr;
};

/* srng flags */
#define HAL_SRNG_FLAGS_MSI_SWAP			0x00000008
#define HAL_SRNG_FLAGS_RING_PTR_SWAP		0x00000010
#define HAL_SRNG_FLAGS_DATA_TLV_SWAP		0x00000020
#define HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN	0x00010000
#define HAL_SRNG_FLAGS_MSI_INTR			0x00020000
#define HAL_SRNG_FLAGS_HIGH_THRESH_INTR_EN	0x00080000
#define HAL_SRNG_FLAGS_CACHED                   0x20000000
#define HAL_SRNG_FLAGS_LMAC_RING		0x80000000
#define HAL_SRNG_FLAGS_CACHED                   0x20000000

/* Common SRNG ring structure for source and destination rings */
struct hal_srng {
	/* Unique SRNG ring ID */
	u8 ring_id;

	/* Ring initialization done */
	u8 initialized;

	/* Interrupt/MSI value assigned to this ring */
	int irq;

	/* Physical base address of the ring */
	dma_addr_t ring_base_paddr;

	/* Virtual base address of the ring */
	u32 *ring_base_vaddr;

	/* Number of entries in ring */
	u32 num_entries;

	/* Ring size */
	u32 ring_size;

	/* Ring size mask */
	u32 ring_size_mask;

	/* Size of ring entry */
	u32 entry_size;

	/* Interrupt timer threshold - in micro seconds */
	u32 intr_timer_thres_us;

	/* Interrupt batch counter threshold - in number of ring entries */
	u32 intr_batch_cntr_thres_entries;

	/* MSI Address */
	dma_addr_t msi_addr;

	/* MSI data */
	u32 msi_data;

	/* MSI2 Address */
	dma_addr_t msi2_addr;

	/* MSI2 data */
	u32 msi2_data;

	/* Misc flags */
	u32 flags;

	/* Lock for serializing ring index updates */
	spinlock_t lock;

	struct lock_class_key lock_key;

	/* Start offset of SRNG register groups for this ring
	 * TBD: See if this is required - register address can be derived
	 * from ring ID
	 */
	u32 hwreg_base[HAL_SRNG_NUM_REG_GRP];

	u64 timestamp;

	/* Source or Destination ring */
	enum hal_srng_dir ring_dir;

	union {
		struct {
			/* SW tail pointer */
			u32 tp;

			/* Shadow head pointer location to be updated by HW */
			volatile u32 *hp_addr;

			/* Cached head pointer */
			u32 cached_hp;

			/* Tail pointer location to be updated by SW - This
			 * will be a register address and need not be
			 * accessed through SW structure
			 */
			u32 *tp_addr;
			u32 *tp_addr_direct;

			/* Current SW loop cnt */
			u32 loop_cnt;

			/* max transfer size */
			u16 max_buffer_length;

			/* head pointer at access end */
			u32 last_hp;
		} dst_ring;

		struct {
			/* SW head pointer */
			u32 hp;

			/* SW reap head pointer */
			u32 reap_hp;

			/* Shadow tail pointer location to be updated by HW */
			u32 *tp_addr;

			/* Cached tail pointer */
			u32 cached_tp;

			/* Head pointer location to be updated by SW - This
			 * will be a register address and need not be accessed
			 * through SW structure
			 */
			u32 *hp_addr;
			u32 *hp_addr_direct;

			/* Low threshold - in number of ring entries */
			u32 low_threshold;

			/* tail pointer at access end */
			u32 last_tp;
		} src_ring;
	} u;
};

/* Interrupt mitigation - Batch threshold in terms of number of frames */
#define HAL_SRNG_INT_BATCH_THRESHOLD_PPE_WBM2SW_REL 256
#define HAL_SRNG_INT_BATCH_THRESHOLD_TX 256
#define HAL_SRNG_INT_BATCH_THRESHOLD_RX 128
#define HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL 0
#define HAL_SRNG_INT_BATCH_THRESHOLD_OTHER 1

/* Interrupt mitigation - timer threshold in us */
#define HAL_SRNG_INT_TIMER_THRESHOLD_TX 1000
#define HAL_SRNG_INT_TIMER_THRESHOLD_RX 200
#define HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL 3
#define HAL_SRNG_INT_TIMER_THRESHOLD_OTHER 256

enum hal_srng_mac_type {
	ATH12K_HAL_SRNG_UMAC,
	ATH12K_HAL_SRNG_DMAC,
	ATH12K_HAL_SRNG_PMAC
};

/* HW SRNG configuration table */
struct hal_srng_config {
	int start_ring_id;
	u16 max_rings;
	u16 entry_size;
	u32 reg_start[HAL_SRNG_NUM_REG_GRP];
	u16 reg_size[HAL_SRNG_NUM_REG_GRP];
	enum hal_srng_mac_type mac_type;
	enum hal_srng_dir ring_dir;
	u32 max_size;
	const char name[20];
};

/**
 * enum hal_rx_buf_return_buf_manager - manager for returned rx buffers
 *
 * @HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST: Buffer returned to WBM idle buffer list
 * @HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 0 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_WBM_DEV1_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 1 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_WBM_DEV2_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 2 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_FW_BM: Buffer returned to FW
 * @HAL_RX_BUF_RBM_SW0_BM: For ring 0 -- returned to host
 * @HAL_RX_BUF_RBM_SW1_BM: For ring 1 -- returned to host
 * @HAL_RX_BUF_RBM_SW2_BM: For ring 2 -- returned to host
 * @HAL_RX_BUF_RBM_SW3_BM: For ring 3 -- returned to host
 * @HAL_RX_BUF_RBM_SW4_BM: For ring 4 -- returned to host
 * @HAL_RX_BUF_RBM_SW5_BM: For ring 5 -- returned to host
 * @HAL_RX_BUF_RBM_SW6_BM: For ring 6 -- returned to host
 */

enum hal_rx_buf_return_buf_manager {
	HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST,
	HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_WBM_DEV1_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_WBM_DEV2_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_FW_BM,
	HAL_RX_BUF_RBM_SW0_BM,
	HAL_RX_BUF_RBM_SW1_BM,
	HAL_RX_BUF_RBM_SW2_BM,
	HAL_RX_BUF_RBM_SW3_BM,
	HAL_RX_BUF_RBM_SW4_BM,
	HAL_RX_BUF_RBM_SW5_BM,
	HAL_RX_BUF_RBM_SW6_BM,
	HAL_RX_BUF_RBM_WBM_DEV3_IDLE_DESC_LIST,
};

enum hal_pn_type {
	HAL_PN_TYPE_NONE,
	HAL_PN_TYPE_WPA,
	HAL_PN_TYPE_WAPI_EVEN,
	HAL_PN_TYPE_WAPI_UNEVEN,
};

enum hal_ce_desc {
	HAL_CE_DESC_SRC,
	HAL_CE_DESC_DST,
	HAL_CE_DESC_DST_STATUS,
};

struct hal_reo_status_header {
	u16 cmd_num;
	enum hal_reo_cmd_status cmd_status;
	u16 cmd_exe_time;
	u32 timestamp;
};

struct ath12k_hw_hal_params {
	enum hal_rx_buf_return_buf_manager rx_buf_rbm;
	u32 wbm2sw_cc_enable;
	u16 link_desc_size;
	u16 num_mpdus_per_link_desc;
	u16 num_tx_msdus_per_link_desc;
	u16 num_rx_msdus_per_link_desc;
	u16 num_mpdu_links_per_queue_desc;
};

struct ath12k_hw_regs {
	u32 hal_tcl1_ring_id;
	u32 hal_tcl1_ring_misc;
	u32 hal_tcl1_ring_tp_addr_lsb;
	u32 hal_tcl1_ring_tp_addr_msb;
	u32 hal_tcl1_ring_consumer_int_setup_ix0;
	u32 hal_tcl1_ring_consumer_int_setup_ix1;
	u32 hal_tcl1_ring_msi1_base_lsb;
	u32 hal_tcl1_ring_msi1_base_msb;
	u32 hal_tcl1_ring_msi1_data;
	u32 hal_tcl_ring_base_lsb;
	u32 hal_tcl1_ring_base_lsb;
	u32 hal_tcl1_ring_base_msb;
	u32 hal_tcl2_ring_base_lsb;

	u32 hal_tcl_status_ring_base_lsb;

	u32 hal_reo1_qdesc_addr;
	u32 hal_reo1_qdesc_max_peerid;

	u32 hal_wbm_idle_ring_base_lsb;
	u32 hal_wbm_idle_ring_misc_addr;
	u32 hal_wbm_r0_idle_list_cntl_addr;
	u32 hal_wbm_r0_idle_list_size_addr;
	u32 hal_wbm_scattered_ring_base_lsb;
	u32 hal_wbm_scattered_ring_base_msb;
	u32 hal_wbm_scattered_desc_head_info_ix0;
	u32 hal_wbm_scattered_desc_head_info_ix1;
	u32 hal_wbm_scattered_desc_tail_info_ix0;
	u32 hal_wbm_scattered_desc_tail_info_ix1;
	u32 hal_wbm_scattered_desc_ptr_hp_addr;

	u32 hal_wbm_sw_release_ring_base_lsb;
	u32 hal_wbm_sw1_release_ring_base_lsb;
	u32 hal_wbm0_release_ring_base_lsb;
	u32 hal_wbm1_release_ring_base_lsb;

	u32 pcie_qserdes_sysclk_en_sel;
	u32 pcie_pcs_osc_dtct_config_base;

	u32 hal_umac_ce0_src_reg_base;
	u32 hal_umac_ce0_dest_reg_base;
	u32 hal_umac_ce1_src_reg_base;
	u32 hal_umac_ce1_dest_reg_base;

	u32 hal_ppe_rel_ring_base;
	u32 hal_reo2ppe_ring_base;
	u32 hal_tcl_ppe2tcl_ring_base_lsb;

	u32 hal_reo2_ring_base;
	u32 hal_reo1_misc_ctrl_addr;
	u32 hal_reo1_sw_cookie_cfg0;
	u32 hal_reo1_sw_cookie_cfg1;
	u32 hal_reo1_qdesc_lut_base0;
	u32 hal_reo1_qdesc_lut_base1;
	u32 hal_reo1_ring_base_lsb;
	u32 hal_reo1_ring_base_msb;
	u32 hal_reo1_ring_id;
	u32 hal_reo1_ring_misc;
	u32 hal_reo1_ring_hp_addr_lsb;
	u32 hal_reo1_ring_hp_addr_msb;
	u32 hal_reo1_ring_producer_int_setup;
	u32 hal_reo1_ring_msi1_base_lsb;
	u32 hal_reo1_ring_msi1_base_msb;
	u32 hal_reo1_ring_msi1_data;
	u32 hal_reo1_aging_thres_ix0;
	u32 hal_reo1_aging_thres_ix1;
	u32 hal_reo1_aging_thres_ix2;
	u32 hal_reo1_aging_thres_ix3;

	u32 hal_reo2_sw0_ring_base;

	u32 hal_sw2reo_ring_base;
	u32 hal_sw2reo1_ring_base;

	u32 hal_reo_cmd_ring_base;

	u32 hal_reo_status_ring_base;
};

struct ath12k_hw_version_map {
	const struct ath12k_hw_hal_params *hal_params;
	const struct ath12k_hw_regs *hw_regs;
};

/* HAL context to be used to access SRNG APIs (currently used by data path
 * and transport (CE) modules)
 */
struct ath12k_hal {
	/* HAL internal state for all SRNG rings.
	 */
	struct hal_srng srng_list[HAL_SRNG_RING_ID_MAX];

	/* SRNG configuration table */
	struct hal_srng_config *srng_config;

	/* Remote pointer memory for HW/FW updates */
	struct {
		u32 *vaddr;
		dma_addr_t paddr;
	} rdp;

	/* Shared memory for ring pointer updates from host to FW */
	struct {
		u32 *vaddr;
		dma_addr_t paddr;
	} wrp;

	struct device *dev;
	const struct hal_ops *hal_ops;
	const struct hal_mon_ops *hal_mon_ops;
	const struct ath12k_hw_regs *regs;
	const struct ath12k_hw_hal_params *hal_params;
	/* Available REO blocking resources bitmap */
	u8 avail_blk_resource;

	u8 current_blk_index;

	/* shadow register configuration */
	u32 shadow_reg_addr[HAL_SHADOW_NUM_REGS_MAX];
	int num_shadow_reg_configured;

	u32 hal_desc_sz;

	const struct ath12k_hal_tcl_to_cmp_rbm_map *tcl_to_cmp_rbm_map;
};

enum ath12k_eht_ru_size {
	ATH12K_EHT_RU_26,
	ATH12K_EHT_RU_52,
	ATH12K_EHT_RU_106,
	ATH12K_EHT_RU_242,
	ATH12K_EHT_RU_484,
	ATH12K_EHT_RU_996,
	ATH12K_EHT_RU_996x2,
	ATH12K_EHT_RU_996x4,
	ATH12K_EHT_RU_52_26,
	ATH12K_EHT_RU_106_26,
	ATH12K_EHT_RU_484_242,
	ATH12K_EHT_RU_996_484,
	ATH12K_EHT_RU_996_484_242,
	ATH12K_EHT_RU_996x2_484,
	ATH12K_EHT_RU_996x3,
	ATH12K_EHT_RU_996x3_484,

	/* Keep last */
	ATH12K_EHT_RU_INVALID,
};

#define HAL_RX_RU_ALLOC_TYPE_MAX	ATH12K_EHT_RU_INVALID

enum hal_wbm_rel_bm_act {
	HAL_WBM_REL_BM_ACT_PUT_IN_IDLE,
	HAL_WBM_REL_BM_ACT_REL_MSDU,
};

/* hal_wbm_rel_bm_act
 *
 * put_in_idle_list
 *	Put the buffer or descriptor back in the idle list. In case of MSDU or
 *	MDPU link descriptor, BM does not need to check to release any
 *	individual MSDU buffers.
 *
 * release_msdu_list
 *	This BM action can only be used in combination with desc_type being
 *	msdu_link_descriptor. Field first_msdu_index points out which MSDU
 *	pointer in the MSDU link descriptor is the first of an MPDU that is
 *	released. BM shall release all the MSDU buffers linked to this first
 *	MSDU buffer pointer. All related MSDU buffer pointer entries shall be
 *	set to value 0, which represents the 'NULL' pointer. When all MSDU
 *	buffer pointers in the MSDU link descriptor are 'NULL', the MSDU link
 *	descriptor itself shall also be released.
 */

/* Maps Completion ring number and Return Buffer Manager Id per TCL ring */
struct ath12k_hal_tcl_to_cmp_rbm_map  {
	u8 cmp_ring_num;
	u8 rbm_id;
};

#define HAL_FST_HASH_KEY_SIZE_BYTES				40
#define HAL_RX_KEY_CACHE_SIZE					512
#define HAL_FST_IP_DA_SA_PFX_TYPE_IPV4_COMPATIBLE_IPV6		2

struct hal_rx_fse;

struct hal_rx_fst {
	struct hal_rx_fse *base_vaddr;
	dma_addr_t base_paddr;
	u8 *key;
	u8 shifted_key[HAL_FST_HASH_KEY_SIZE_BYTES];
	u32 key_cache[HAL_FST_HASH_KEY_SIZE_BYTES][HAL_RX_KEY_CACHE_SIZE];
	u16 max_entries;
	u16 max_skid_length;
	u32 fst_entry_size;
};

struct hal_flow_tuple_info {
	u32 dest_ip_127_96;
	u32 dest_ip_95_64;
	u32 dest_ip_63_32;
	u32 dest_ip_31_0;
	u32 src_ip_127_96;
	u32 src_ip_95_64;
	u32 src_ip_63_32;
	u32 src_ip_31_0;
	u32 dest_port;
	u32 src_port;
	u32 l4_protocol;
};

struct hal_wbm_idle_scatter_list;
struct hal_wbm_link_desc;

#define HAL_TX_PPEDS_CFG_SEARCH_IDX                GENMASK(19, 0)
#define HAL_TX_PPEDS_CFG_CACHE_SET                 GENMASK(23, 20)

struct hal_ops {
	int (*hal_init)(struct ath12k_hal *hal, u8 hw_version);
	int (*create_srng_config)(struct ath12k_hal *hal);
	void (*rx_desc_copy_end_tlv)(struct hal_rx_desc *fdesc,
				     struct hal_rx_desc *ldesc);
	void (*rx_desc_get_crypto_header)(struct hal_rx_desc *desc,
					  u8 *crypto_hdr,
					  enum hal_encrypt_type enctype);
	u16 (*rx_desc_get_mpdu_frame_ctl)(struct hal_rx_desc *desc);
	void (*rx_desc_get_dot11_hdr)(struct hal_rx_desc *desc,
				      struct ieee80211_hdr *hdr);
	void (*rx_desc_set_msdu_len)(struct hal_rx_desc *desc, u16 len);
	u8 (*rx_desc_get_msdu_src_link_id)(struct hal_rx_desc *desc);
	void (*extract_rx_desc_data)(struct hal_rx_desc_data *rx_desc_data,
				     struct hal_rx_desc *rx_desc,
				     struct hal_rx_desc *ldesc);
	void (*extract_rx_spd_data)(struct hal_rx_spd_data *rx_info,
				    struct hal_rx_desc *rx_desc, int set);
	void (*ce_dst_setup)(struct ath12k_base *ab,
			     struct hal_srng *srng, int ring_num);
	void (*set_umac_srng_ptr_addr)(struct ath12k_base *ab,
				       struct hal_srng *srng,
				       enum hal_ring_type type, int ring_num);
	void (*srng_src_hw_init)(struct ath12k_base *ab, struct hal_srng *srng,
				 u32 restore_idx);
	void (*srng_dst_hw_init)(struct ath12k_base *ab, struct hal_srng *srng,
				 u32 restore_idx);
	int (*srng_update_shadow_config)(struct ath12k_base *ab,
					 enum hal_ring_type ring_type,
					 int ring_num);
	int (*srng_get_ring_id)(struct ath12k_hal *hal, enum hal_ring_type type,
				int ring_num, int mac_id);
	u32 (*ce_get_desc_size)(enum hal_ce_desc type);
	void (*ce_src_set_desc)(struct hal_ce_srng_src_desc *desc,
				dma_addr_t paddr, u32 len, u32 id,
				u8 byte_swap_data);
	void (*ce_dst_set_desc)(struct hal_ce_srng_dest_desc *desc,
				dma_addr_t paddr);
	u32 (*ce_dst_status_get_length)(struct hal_ce_srng_dst_status_desc *desc);
	void (*set_link_desc_addr)(struct hal_wbm_link_desc *desc, u32 cookie,
				   dma_addr_t paddr,
				   enum hal_rx_buf_return_buf_manager rbm);
	void (*setup_link_idle_list)(struct ath12k_base *ab,
				     struct hal_wbm_idle_scatter_list *sbuf,
				     u32 nsbufs, u32 tot_link_desc,
				     u32 end_offset);
	void (*tx_update_dscp_tid_map)(struct ath12k_base *ab, int id, u8 dscp, u8 tid);
	void (*tx_set_dscp_tid_map)(struct ath12k_base *ab, u8 *map, int id);
	void (*tx_configure_bank_register)(struct ath12k_base *ab,
					   u32 bank_config, u8 bank_id);
	void (*write_ml_reoq_lut_addr)(struct ath12k_base *ab,
				       dma_addr_t paddr);
	void (*write_reoq_lut_addr)(struct ath12k_base *ab, dma_addr_t paddr);
	void (*reoq_lut_addr_read_enable)(struct ath12k_base *ab);
	void (*reoq_lut_set_max_peerid)(struct ath12k_base *ab);
	void (*reo_qdesc_setup)(struct hal_rx_reo_queue *qdesc,
				int tid, u32 ba_window_size,
				u32 start_seq, enum hal_pn_type type);
	void (*reo_init_cmd_ring)(struct ath12k_base *ab,
				  struct hal_srng *srng);
	void (*reo_hw_setup)(struct ath12k_base *ab);
	void (*cc_config)(struct ath12k_base *ab);
	void (*srng_hw_disable)(struct ath12k_base *ab, struct hal_srng *srng);
	void (*reset_rx_reo_tid_q)(void *vaddr, u32 ba_window_size, u8 tid);
	enum hal_rx_buf_return_buf_manager
        (*get_idle_link_rbm)(struct ath12k_hal *hal, u8 device_id);
	void (*reo_shared_qaddr_cache_clear)(struct ath12k_base *ab);
	u8 *(*rxdesc_get_mpdu_start_addr2)(struct hal_rx_desc *desc);
	bool (*rx_h_is_decrypted)(struct hal_rx_desc *desc);
	u32 (*rx_desc_get_mpdu_ppdu_id)(struct hal_rx_desc *rx_desc);
	u32 (*rx_desc_get_mpdu_start_tag)(struct hal_rx_desc *desc);
	void (*rx_reo_ent_buf_paddr_get)(void *rx_desc,
					 dma_addr_t *paddr,
					 u32 *sw_cookie,
					 struct ath12k_buffer_addr **pp_buf_addr,
					 u8 *rbm, u32 *msdu_cnt);
	void (*rx_msdu_list_get)(void *link_desc,
				 void *msdu_list,
				 u16 *num_msdus);
	u8 (*rx_h_l3pad_get)(struct hal_rx_desc *desc);
	void (*hal_mon_ops_init)(struct ath12k_hal *hal, u8 hw_version);
	void (*hal_get_tsf2_scratch_reg)(struct ath12k_base *ab, u8 mac_id,
					 u64 *value);
	void (*hal_get_tqm_scratch_reg)(struct ath12k_base *ab, u64 *value);
	void (*get_hw_hptp)(struct ath12k_base *ab, enum hal_ring_type type,
			    struct hal_srng *srng, uint32_t *hp, uint32_t *tp);
	void (*rx_desc_get_fse_info)(struct hal_rx_desc *desc,
				     struct rx_mpdu_desc_info *rx_mpdu_info);
};

static inline
enum nl80211_he_ru_alloc ath12k_he_ru_tones_to_nl80211_he_ru_alloc(u16 ru_tones)
{
	enum nl80211_he_ru_alloc ret;

	switch (ru_tones) {
	case RU_52:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_52;
		break;
	case RU_106:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_106;
		break;
	case RU_242:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_242;
		break;
	case RU_484:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_484;
		break;
	case RU_996:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_996;
		break;
	case RU_26:
		fallthrough;
	default:
		ret = NL80211_RATE_INFO_HE_RU_ALLOC_26;
		break;
	}
	return ret;
}

static inline void *ath12k_hal_dma_alloc_coherent(struct device *dev, size_t size,
						   dma_addr_t *paddr, gfp_t flag)
{
	void *vaddr = NULL;

#ifdef CONFIG_IO_COHERENCY
	vaddr = kzalloc(size, flag);
	*paddr = (dma_addr_t)virt_to_phys(vaddr);
#else
	vaddr = dma_alloc_coherent(dev, size, paddr, flag);
#endif

	return vaddr;
}

static inline void ath12k_hal_dma_free_coherent(struct device *dev, size_t size,
						 void *vaddr, dma_addr_t paddr)
{
#ifdef CONFIG_IO_COHERENCY
	kfree(vaddr);
#else
	dma_free_coherent(dev, size, vaddr, paddr);
#endif
}

u8 ath12k_hal_rx_get_msdu_src_link(struct ath12k_base *ab,
				   struct hal_rx_desc *desc);
void ath12k_hal_rx_desc_end_tlv_copy(struct ath12k_base *ab,
				     struct hal_rx_desc *fdesc,
				     struct hal_rx_desc *ldesc);
void ath12k_hal_rxdesc_set_msdu_len(struct ath12k_base *ab,
				    struct hal_rx_desc *desc,
				    u16 len);
void ath12k_hal_rx_desc_get_dot11_hdr(struct ath12k_base *ab,
				      struct hal_rx_desc *desc,
				      struct ieee80211_hdr *hdr);
void ath12k_hal_rx_desc_get_crypto_header(struct ath12k_base *ab,
					  struct hal_rx_desc *desc,
					  u8 *crypto_hdr,
					  enum hal_encrypt_type enctype);
u16 ath12k_hal_rxdesc_get_mpdu_frame_ctrl(struct ath12k_base *ab,
					  struct hal_rx_desc *desc);
void ath12k_hal_setup_link_idle_list(struct ath12k_base *ab,
				     struct wbm_idle_scatter_list *sbuf,
				     u32 nsbufs, u32 tot_link_desc,
				     u32 end_offset);
dma_addr_t ath12k_hal_srng_get_tp_addr(struct ath12k_base *ab,
				       struct hal_srng *srng);
dma_addr_t ath12k_hal_srng_get_hp_addr(struct ath12k_base *ab,
				       struct hal_srng *srng);
u32 ath12k_hal_ce_get_desc_size(struct ath12k_hal *hal, enum hal_ce_desc type);
void ath12k_hal_ce_dst_set_desc(struct ath12k_hal *hal,
				struct hal_ce_srng_dest_desc *desc,
				dma_addr_t paddr);
void ath12k_hal_ce_src_set_desc(struct ath12k_hal *hal,
				struct hal_ce_srng_src_desc *desc,
				dma_addr_t paddr, u32 len, u32 id,
				u8 byte_swap_data);
int ath12k_hal_srng_get_entrysize(struct ath12k_base *ab, u32 ring_type);
int ath12k_hal_srng_get_max_entries(struct ath12k_base *ab, u32 ring_type);
void ath12k_hal_srng_get_params(struct ath12k_base *ab, struct hal_srng *srng,
				struct hal_srng_params *params);
void *ath12k_hal_srng_dst_get_next_entry(struct ath12k_base *ab,
					 struct hal_srng *srng);
void *__ath12k_hal_srng_dst_get_next_cached_entry(struct hal_srng *srng,
						  u32 *old_tp);
void *ath12k_hal_srng_dst_get_next_cached_entry(struct ath12k_base *ab,
						struct hal_srng *srng,
						u32 *old_tp);
void *ath12k_hal_srng_src_peek(struct ath12k_base *ab, struct hal_srng *srng);
void *ath12k_hal_srng_dst_peek(struct ath12k_base *ab, struct hal_srng *srng);
void *__ath12k_hal_srng_dst_peek(struct hal_srng *srng);
int __ath12k_hal_srng_dst_num_free(struct hal_srng *srng, bool sync_hw_ptr);
int ath12k_hal_srng_dst_num_free(struct ath12k_base *ab, struct hal_srng *srng,
				 bool sync_hw_ptr);
void ath12k_hal_srng_dst_invalidate_entry(struct ath12k_dp *dp,struct hal_srng *srng,
					  int entries);
void *ath12k_hal_srng_src_get_next_reaped(struct ath12k_base *ab,
					  struct hal_srng *srng);
void *ath12k_hal_srng_src_reap_next(struct ath12k_base *ab,
				    struct hal_srng *srng);
void *ath12k_hal_srng_src_next_peek(struct ath12k_base *ab,
				    struct hal_srng *srng);
void *ath12k_hal_srng_src_get_next_entry(struct ath12k_base *ab,
					 struct hal_srng *srng);
int ath12k_hal_srng_src_num_free(struct ath12k_base *ab, struct hal_srng *srng,
				 bool sync_hw_ptr);
u32 ath12k_hal_srng_access_begin(struct ath12k_base *ab, struct hal_srng *srng);
u32 ath12k_hal_srng_access_begin_no_lock(struct hal_srng *srng);
u32 __ath12k_hal_srng_access_begin(struct hal_srng *srng);
void __ath12k_hal_srng_access_end(struct ath12k_base *ab, struct hal_srng *srng);
void ath12k_hal_srng_access_end_no_lock(struct ath12k_base *ab,
					struct hal_srng *srng);
void ath12k_hal_srng_access_end(struct ath12k_base *ab, struct hal_srng *srng);
int ath12k_hal_srng_setup_idx(struct ath12k_base *ab, enum hal_ring_type type,
			      int ring_num, int mac_id,
			      struct hal_srng_params *params,
			      u32 restore_idx);
void ath12k_hal_dump_srng_stats(struct ath12k_base *ab);
ssize_t ath12k_debugfs_hal_dump_srng_stats(struct ath12k_base *ab, char *buf, int size);
void ath12k_hal_srng_get_shadow_config(struct ath12k_base *ab,
				       u32 **cfg, u32 *len);
int ath12k_hal_srng_update_shadow_config(struct ath12k_base *ab,
					 enum hal_ring_type ring_type,
					int ring_num);
void ath12k_hal_srng_shadow_config(struct ath12k_base *ab);
void ath12k_hal_srng_shadow_update_hp_tp(struct ath12k_base *ab,
					 struct hal_srng *srng);
int ath12k_hal_srng_init(struct ath12k_base *ab);
void ath12k_hal_srng_deinit(struct ath12k_base *ab);
void ath12k_hal_set_link_desc_addr(struct ath12k_hal *hal,
				   struct wbm_link_desc *desc, u32 cookie,
				   dma_addr_t paddr, int rbm);
u32
ath12k_hal_ce_dst_status_get_length(struct ath12k_hal *hal,
				    struct hal_ce_srng_dst_status_desc *desc);
void ath12k_hal_tx_update_dscp_tid_map(struct ath12k_base *ab, int id, u8 dscp, u8 tid);
void ath12k_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, u8 *map, int id);
void ath12k_hal_tx_configure_bank_register(struct ath12k_base *ab,
					   u32 bank_config, u8 bank_id);
void ath12k_hal_write_reoq_lut_addr(struct ath12k_base *ab, dma_addr_t paddr);
void
ath12k_hal_write_ml_reoq_lut_addr(struct ath12k_base *ab, dma_addr_t paddr);
void ath12k_hal_reoq_lut_addr_read_enable(struct ath12k_base *ab);
void ath12k_hal_reoq_lut_set_max_peerid(struct ath12k_base *ab);
void ath12k_hal_reo_init_cmd_ring(struct ath12k_base *ab,
                                 struct hal_srng *srng);
void ath12k_hal_reo_hw_setup(struct ath12k_base *ab);
void ath12k_hal_rx_buf_addr_info_set(struct ath12k_buffer_addr *binfo,
				     dma_addr_t paddr, u32 cookie, u8 manager);
void ath12k_hal_rx_buf_addr_info_get(struct ath12k_buffer_addr *binfo,
				     dma_addr_t *paddr, u32 *msdu_cookies,
				     u8 *rbm);
void ath12k_hal_cc_config(struct ath12k_base *ab);
enum hal_rx_buf_return_buf_manager
ath12k_hal_get_idle_link_rbm(struct ath12k_hal *hal, u8 device_id);
void ath12k_hal_vdev_mcast_ctrl_set(struct ath12k_base *ab, u32 vdev_id,
				    u8 mcast_ctrl_val);
void ath12k_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab);
u8 *
ath12k_hal_rxdesc_get_mpdu_start_addr2(struct ath12k_hal *hal, struct hal_rx_desc *desc);
void __ath12k_hal_srng_update_tp(struct hal_srng *srng, u32 new_tp);
void ath12k_hal_srng_update_tp(struct hal_srng *srng, u32 new_tp);
bool ath12k_hal_rx_h_is_decrypted(struct ath12k_hal *hal, struct hal_rx_desc *desc);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_hal_srng_ppeds_dst_inv_entry(struct ath12k_base *ab,
					 struct hal_srng *srng, int entries);
void ath12k_hal_reo_config_reo2ppe_dest_info(struct ath12k_base *ab);
void ath12k_hal_ppeds_cfg_ast_override_map_reg(struct ath12k_base *ab, u8 idx,
					       u32 ppeds_idx_map_val);
#endif
void ath12k_hal_reset_rx_reo_tid_q(struct ath12k_hal *hal, void *qdesc,
				   u32 ba_window_size, u8 tid);
void ath12k_hal_srng_hw_disable(struct ath12k_base *ab,
                                struct hal_srng *srng);
u32 ath12k_hal_rx_desc_get_mpdu_ppdu_id(struct ath12k_hal *hal,
					struct hal_rx_desc *rx_desc);
u32 hal_rx_desc_get_mpdu_start_tag(struct ath12k_hal *hal,
				   struct hal_rx_desc *rx_desc);
void ath12k_hal_rx_reo_ent_buf_paddr_get(struct ath12k_hal *hal,
					 void *rx_desc, dma_addr_t *paddr,
					 u32 *sw_cookie,
					 struct ath12k_buffer_addr **pp_buf_addr,
					 u8 *rbm, u32 *msdu_cnt);
void ath12k_hal_rx_msdu_list_get(struct ath12k_hal *hal,
				 void *link_desc,
				 void *msdu_list,
				 u16 *num_msdus);
u8 ath12k_hal_rx_h_l3pad_get(struct ath12k_hal *hal,
			     struct hal_rx_desc *desc);
#endif
