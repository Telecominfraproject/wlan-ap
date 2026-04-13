/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef ATH11K_CFR_H
#define ATH11K_CFR_H

#include "dbring.h"
#include "wmi.h"

#define ATH11K_CFR_NUM_RESP_PER_EVENT   1
#define ATH11K_CFR_EVENT_TIMEOUT_MS     1

#define ATH11K_CORRELATE_TX_EVENT 1
#define ATH11K_CORRELATE_DBR_EVENT 0

#define ATH11K_MAX_CFR_ENABLED_CLIENTS 10

#define ATH11K_CFR_START_MAGIC 0xDEADBEAF
#define ATH11K_CFR_END_MAGIC 0xBEAFDEAD

#define ATH11K_CFR_RADIO_IPQ8074 23
#define ATH11K_CFR_RADIO_QCN9074 26
#define ATH11K_CFR_RADIO_IPQ5018 28
#define ATH11K_CFR_RADIO_QCN6122 30

#define VENDOR_QCA 0x8cfdf0
#define PLATFORM_TYPE_ARM 2
#define NUM_CHAINS_FW_TO_HOST(n) ((1 << ((n) + 1)) - 1)

enum ath11k_cfr_meta_version {
	ATH11K_CFR_META_VERSION_NONE,
	ATH11K_CFR_META_VERSION_1,
	ATH11K_CFR_META_VERSION_2,
	ATH11K_CFR_META_VERSION_3,
	ATH11K_CFR_META_VERSION_4,
	ATH11K_CFR_META_VERSION_5,
	ATH11K_CFR_META_VERSION_MAX = 0xFF,
};

enum ath11k_cfr_data_version {
	ATH11K_CFR_DATA_VERSION_NONE,
	ATH11K_CFR_DATA_VERSION_1,
	ATH11K_CFR_DATA_VERSION_MAX = 0xFF,
};

enum ath11k_cfr_capture_ack_mode {
	ATH11K_CFR_CAPTURE_LEGACY_ACK,
	ATH11K_CFR_CAPTURE_DUP_LEGACY_ACK,
	ATH11K_CFR_CAPTURE_HT_ACK,
	ATH11K_CFR_CPATURE_VHT_ACK,

	/*Always keep this at last*/
	ATH11K_CFR_CPATURE_INVALID_ACK
};

enum ath11k_cfr_correlate_status {
	ATH11K_CORRELATE_STATUS_RELEASE,
	ATH11K_CORRELATE_STATUS_HOLD,
	ATH11K_CORRELATE_STATUS_ERR,
};

struct ath11k_cfr_peer_tx_param {
        u32 capture_method;
        u32 vdev_id;
        u8 peer_mac_addr[ETH_ALEN];
        u32 primary_20mhz_chan;
        u32 bandwidth;
        u32 phy_mode;
        u32 band_center_freq1;
        u32 band_center_freq2;
        u32 spatial_streams;
        u32 correlation_info_1;
        u32 correlation_info_2;
        u32 status;
        u32 timestamp_us;
        u32 counter;
        u32 chain_rssi[WMI_MAX_CHAINS];
        u16 chain_phase[WMI_MAX_CHAINS];
	u32 cfo_measurement;
	u8 agc_gain[WMI_MAX_CHAINS];
	u32 rx_start_ts;
	u32 rx_ts_reset;
};

struct cfr_metadata_version_1 {
	u8 peer_addr[ETH_ALEN];
	u8 status;
	u8 capture_bw;
	u8 channel_bw;
	u8 phy_mode;
	u16 prim20_chan;
	u16 center_freq1;
	u16 center_freq2;
	u8 capture_mode;
	u8 capture_type;
	u8 sts_count;
	u8 num_rx_chain;
	u32 timestamp;
	u32 length;
} __packed;

#define HOST_MAX_CHAINS 8
#define MAX_CFR_MU_USERS 4

struct cfr_metadata_version_2 {
	u8 peer_addr[ETH_ALEN];
	u8 status;
	u8 capture_bw;
	u8 channel_bw;
	u8 phy_mode;
	u16 prim20_chan;
	u16 center_freq1;
	u16 center_freq2;
	u8 capture_mode;
	u8 capture_type;
	u8 sts_count;
	u8 num_rx_chain;
	u32 timestamp;
	u32 length;
	u32 chain_rssi[HOST_MAX_CHAINS];
	u16 chain_phase[HOST_MAX_CHAINS];
} __packed;

struct cfr_metadata_version_3 {
	u8 status;
	u8 capture_bw;
	u8 channel_bw;
	u8 phy_mode;
	u16 prim20_chan;
	u16 center_freq1;
	u16 center_freq2;
	u8 capture_mode;
	u8 capture_type;
	u8 sts_count;
	u8 num_rx_chain;
	u64 timestamp;
	u32 length;
	u8 is_mu_ppdu;
	u8 num_mu_users;
	union {
		u8 su_peer_addr[ETH_ALEN];
		u8 mu_peer_addr[MAX_CFR_MU_USERS][ETH_ALEN];
	} peer_addr;
	u32 chain_rssi[HOST_MAX_CHAINS];
	u16 chain_phase[HOST_MAX_CHAINS];
} __packed;

struct cfr_metadata_version_4 {
	u8 peer_addr[ETH_ALEN];
	u8 status;
	u8 capture_bw;
	u8 channel_bw;
	u8 phy_mode;
	u16 prim20_chan;
	u16 center_freq1;
	u16 center_freq2;
	u8 capture_mode;
	u8 capture_type;
	u8 sts_count;
	u8 num_rx_chain;
	u32 timestamp;
	u32 length;
	u32 chain_rssi[HOST_MAX_CHAINS];
	u16 chain_phase[HOST_MAX_CHAINS];
	u32 cfo_measurement;
	u8 agc_gain[HOST_MAX_CHAINS];
	u32 rx_start_ts;
} __packed;

struct cfr_metadata_version_5 {
        u8 status;
        u8 capture_bw;
        u8 channel_bw;
        u8 phy_mode;
        u16 prim20_chan;
        u16 center_freq1;
        u16 center_freq2;
        u8 capture_mode;
        u8 capture_type;
        u8 sts_count;
        u8 num_rx_chain;
        u64 timestamp;
        u32 length;
        u8 is_mu_ppdu;
        u8 num_mu_users;
        union {
                u8 su_peer_addr[ETH_ALEN];
                u8 mu_peer_addr[MAX_CFR_MU_USERS][ETH_ALEN];
        } peer_addr;
        u32 chain_rssi[HOST_MAX_CHAINS];
        u16 chain_phase[HOST_MAX_CHAINS];
	u32 cfo_measurement;
	u8 agc_gain[HOST_MAX_CHAINS];
	u32 rx_start_ts;
} __packed;

struct ath11k_csi_cfr_header {
	u32 start_magic_num;
	u32 vendorid;
	u8 cfr_metadata_version;
	u8 cfr_data_version;
	u8 chip_type;
	u8 pltform_type;
	u32 cfr_metadata_len;
	union {
		struct cfr_metadata_version_1 meta_v1;
		struct cfr_metadata_version_2 meta_v2;
		struct cfr_metadata_version_3 meta_v3;
		struct cfr_metadata_version_4 meta_v4;
		struct cfr_metadata_version_5 meta_v5;
	} u;
} __packed;

enum ath11k_cfr_preamble_type {
	ATH11K_CFR_PREAMBLE_TYPE_LEGACY,
	ATH11K_CFR_PREAMBLE_TYPE_HT,
	ATH11K_CFR_PREAMBLE_TYPE_VHT,
};

#define TONES_IN_20MHZ  256
#define TONES_IN_40MHZ  512
#define TONES_IN_80MHZ  1024
#define TONES_IN_160MHZ 2048 /* 160 MHz isn't supported yet */
#define TONES_INVALID   0

#define CFIR_DMA_HDR_INFO0_TAG GENMASK(7, 0)
#define CFIR_DMA_HDR_INFO0_LEN GENMASK(13, 8)

#define CFIR_DMA_HDR_INFO1_UPLOAD_DONE	GENMASK(0, 0)
#define CFIR_DMA_HDR_INFO1_CAPTURE_TYPE	GENMASK(3, 1)
#define CFIR_DMA_HDR_INFO1_PREABLE_TYPE	GENMASK(5, 4)
#define CFIR_DMA_HDR_INFO1_NSS		GENMASK(8, 6)
#define CFIR_DMA_HDR_INFO1_NUM_CHAINS	GENMASK(11, 9)
#define CFIR_DMA_HDR_INFO1_UPLOAD_PKT_BW GENMASK(14, 12)
#define CFIR_DMA_HDR_INFO1_SW_PEER_ID_VALID GENMASK(15, 15)

struct ath11k_cfir_dma_hdr {
        u16 info0;
	u16 info1;
	u16 sw_peer_id;
	u16 phy_ppdu_id;
};

#define CFIR_DMA_HDR_INFO2_HDR_VER GENMASK(3, 0)
#define CFIR_DMA_HDR_INFO2_TARGET_ID GENMASK(7, 4)
#define CFIR_DMA_HDR_INFO2_CFR_FMT BIT(8)
#define CFIR_DMA_HDR_INFO2_RSVD BIT(9)
#define CFIR_DMA_HDR_INFO2_MURX_DATA_INC BIT(10)
#define CFIR_DMA_HDR_INFO2_FREEZ_DATA_INC BIT(11)
#define CFIR_DMA_HDR_INFO2_FREEZ_TLV_VER GENMASK(15, 12)

#define CFIR_DMA_HDR_INFO3_MU_RX_NUM_USERS GENMASK(7, 0)
#define CFIR_DMA_HDR_INFO3_DECIMATION_FACT GENMASK(11, 8)
#define CFIR_DMA_HDR_INFO3_RSVD GENMASK(15, 12)

struct ath11k_cfir_enh_dma_hdr {
	struct ath11k_cfir_dma_hdr hdr;
	u16 total_bytes;
	u16 info2;
	u16 info3;
	u16 rsvd;
};

#define CFR_MAX_LUT_ENTRIES 136

struct ath11k_cfr_look_up_table {
	bool dbr_recv;
	bool tx_recv;
	u8 *data;
	u32 data_len;
	u16 dbr_ppdu_id;
	u16 tx_ppdu_id;
	dma_addr_t dbr_address;
	u32 tx_address1;
	u32 tx_address2;
	struct ath11k_csi_cfr_header header;
	union {
		struct ath11k_cfir_dma_hdr hdr;
		struct ath11k_cfir_enh_dma_hdr enh_hdr;
	} dma_hdr;
	u64 txrx_tstamp;
	u64 dbr_tstamp;
	u32 header_length;
	u32 payload_length;
	struct ath11k_dbring_element *buff;
};

enum cfr_capture_type {
	CFR_CAPTURE_METHOD_NULL_FRAME = 0,
	CFR_CAPURE_METHOD_NULL_FRAME_WITH_PHASE = 1,
	CFR_CAPTURE_METHOD_PROBE_RESP = 2,
	CFR_CAPTURE_METHOD_TM = 3,
	CFR_CAPTURE_METHOD_FTM = 4,
	CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM = 5,
	CFR_CAPTURE_METHOD_TA_RA_TYPE_FILTER = 6,
	CFR_CAPTURE_METHOD_NDPA_NDP = 7,
	CFR_CAPTURE_METHOD_ALL_PACKET = 8,
	/* Add new capture methods before this line */
	CFR_CAPTURE_METHOD_LAST_VALID,
	CFR_CAPTURE_METHOD_AUTO = 0xff,
	CFR_CAPTURE_METHOD_MAX,
};

/* enum macrx_freeze_tlv_version: Reported by uCode in enh_dma_header
 * MACRX_FREEZE_TLV_VERSION_1: Single MU UL user info reported by MAC
 * MACRX_FREEZE_TLV_VERSION_2: Upto 4 MU UL user info reported by MAC
 * MACRX_FREEZE_TLV_VERSION_3: Upto 37 MU UL user info reported by MAC
 */
enum macrx_freeze_tlv_version {
	MACRX_FREEZE_TLV_VERSION_1 = 1,
	MACRX_FREEZE_TLV_VERSION_2 = 2,
	MACRX_FREEZE_TLV_VERSION_3 = 3,
	MACRX_FREEZE_TLV_VERSION_MAX
};

enum mac_freeze_capture_reason {
	FREEZE_REASON_TM = 0,
	FREEZE_REASON_FTM,
	FREEZE_REASON_ACK_RESP_TO_TM_FTM,
	FREEZE_REASON_TA_RA_TYPE_FILTER,
	FREEZE_REASON_NDPA_NDP,
	FREEZE_REASON_ALL_PACKET,
	FREEZE_REASON_MAX,
};

#define MACRX_FREEZE_CC_INFO0_FREEZE GENMASK(0, 0)
#define MACRX_FREEZE_CC_INFO0_CAPTURE_REASON GENMASK(3, 1)
#define MACRX_FREEZE_CC_INFO0_PKT_TYPE GENMASK(5, 4)
#define MACRX_FREEZE_CC_INFO0_PKT_SUB_TYPE GENMASK(9, 6)
#define MACRX_FREEZE_CC_INFO0_RSVD GENMASK(14, 10)
#define MACRX_FREEZE_CC_INFO0_SW_PEER_ID_VALID GENMASK(15, 15)

#define MACRX_FREEZE_CC_INFO1_USER_MASK GENMASK(5, 0)
#define MACRX_FREEZE_CC_INFO1_DIRECTED GENMASK(6, 6)
#define MACRX_FREEZE_CC_INFO1_RSVD GENMASK(15, 7)

struct macrx_freeze_capture_channel {
	u16 info0;
	u16 sw_peer_id;
	u16 phy_ppdu_id;
	u16 packet_ta_lower_16;
	u16 packet_ta_mid_16;
	u16 packet_ta_upper_16;
	u16 packet_ra_lower_16;
	u16 packet_ra_mid_16;
	u16 packet_ra_upper_16;
	u16 tsf_timestamp_15_0;
	u16 tsf_timestamp_31_16;
	u16 tsf_timestamp_47_32;
	u16 tsf_timestamp_63_48;
	u16 info1;
};

#define MACRX_FREEZE_CC_V3_INFO0_FREEZE GENMASK(0, 0)
#define MACRX_FREEZE_CC_V3_INFO0_CAPTURE_REASON GENMASK(3, 1)
#define MACRX_FREEZE_CC_V3_INFO0_PKT_TYPE GENMASK(5, 4)
#define MACRX_FREEZE_CC_V3_INFO0_PKT_SUB_TYPE GENMASK(9, 6)
#define MACRX_FREEZE_CC_V3_INFO0_DIRECTED GENMASK(10, 10)
#define MACRX_FREEZE_CC_V3_INFO0_RSVD GENMASK(14, 11)
#define MACRX_FREEZE_CC_V3_INFO0_SW_PEER_ID_VALID GENMASK(15, 15)

/*
 * freeze_tlv v3 used by qcn9074
 */
struct macrx_freeze_capture_channel_v3 {
	u16 info0;
	u16 sw_peer_id;
	u16 phy_ppdu_id;
	u16 packet_ta_lower_16;
	u16 packet_ta_mid_16;
	u16 packet_ta_upper_16;
	u16 packet_ra_lower_16;
	u16 packet_ra_mid_16;
	u16 packet_ra_upper_16;
	u16 tsf_timestamp_15_0;
	u16 tsf_timestamp_31_16;
	u16 tsf_timestamp_47_32;
	u16 tsf_63_48_or_user_mask_36_32;
	u16 user_index_or_user_mask_15_0;
	u16 user_mask_31_16;
};

struct cfr_unassoc_pool_entry {
	u8 peer_mac[ETH_ALEN];
	u32 period;
	bool is_valid;
};

struct ath11k_cfr {
	struct ath11k_dbring rx_ring;
	/* Protects enabled for ath11k_cfr */
	spinlock_t lock;
	struct rchan *rfs_cfr_capture;
	struct dentry *enable_cfr;
	struct dentry *cfr_unassoc;
	u8 cfr_enabled_peer_cnt;
	struct ath11k_cfr_look_up_table *lut;
	u32 lut_num;
	u32 dbr_buf_size;
	u32 dbr_num_bufs;
	u32 max_mu_users;
	/* protect look up table data */
	spinlock_t lut_lock;
	u64 tx_evt_cnt;
	u64 dbr_evt_cnt;
	u64 total_tx_evt_cnt;
	u64 release_cnt;
	u64 tx_peer_status_cfr_fail;
	u64 tx_evt_status_cfr_fail;
	u64 tx_dbr_lookup_fail;
	u64 last_success_tstamp;
	u64 flush_dbr_cnt;
	u64 invalid_dma_length_cnt;
	u64 clear_txrx_event;
	u64 cfr_dma_aborts;
	u64 flush_timeout_dbr_cnt;
	struct cfr_unassoc_pool_entry unassoc_pool[ATH11K_MAX_CFR_ENABLED_CLIENTS];
};

#ifdef CPTCFG_ATH11K_CFR

int ath11k_cfr_init(struct ath11k_base *ab);
void ath11k_cfr_deinit(struct ath11k_base *ab);
struct ath11k_dbring *ath11k_cfr_get_dbring(struct ath11k *ar);
int ath11k_process_cfr_capture_event(struct ath11k_base *ab,
				     struct ath11k_cfr_peer_tx_param *params);
bool peer_is_in_cfr_unassoc_pool(struct ath11k *ar, u8 *peer_mac);
void ath11k_cfr_lut_update_paddr(struct ath11k *ar, dma_addr_t paddr,
				 u32 buf_id);
void ath11k_cfr_decrement_peer_count(struct ath11k *ar,
				     struct ath11k_sta *arsta);
#else
static inline int ath11k_cfr_init(struct ath11k_base *ab)
{
	return 0;
}
static inline void ath11k_cfr_deinit(struct ath11k_base *ab)
{
}
static inline
struct ath11k_dbring *ath11k_cfr_get_dbring(struct ath11k *ar)
{
	return NULL;
}
static inline bool peer_is_in_cfr_unassoc_pool(struct ath11k *ar, u8 *peer_mac)
{
	return false;
}
static inline
int ath11k_process_cfr_capture_event(struct ath11k_base *ab,
				     struct ath11k_cfr_peer_tx_param *params)
{
	return 0;
}
static inline void ath11k_cfr_lut_update_paddr(struct ath11k *ar,
					       dma_addr_t paddr, u32 buf_id)
{
}
static inline void ath11k_cfr_decrement_peer_count(struct ath11k *ar,
						   struct ath11k_sta *arsta)
{
}
#endif /* CPTCFG_ATH11K_CFR */
#endif /* ATH11K_CFR_H */
