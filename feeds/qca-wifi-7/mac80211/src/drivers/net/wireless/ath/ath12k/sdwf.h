/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ATH12K_SDWF_H
#define ATH12K_SDWF_H

#include "ath/ath_dp_accel_cfg.h"

#define SDWF_PEER_MSDUQ_INVALID 0xFFFF
#define SDWF_METADATA_INVALID   0xFFFFFFFF
#define SDWF_MAX_TID_SUPPORT 8
#define SDWF_PCP_VALID  0x4
#define FLOW_START 1
#define FLOW_STOP  2

#define SDWF_VALID		1
#define SDWF_VALID_MASK		BIT(22)
#define SDWF_VALID_TAG		0xAA

#define SDWF_TAG_ID		GENMASK(31, 24)
#define SDWF_PEER_MSDUQ_ID	GENMASK(15, 0)
#define SDWF_PEER_ID		GENMASK(15, 6)
#define SDWF_MSDUQ_ID		GENMASK(5, 0)

#define SCS_SVC_ID_MASK		GENMASK(15, 8)
#define SCS_PROTOCOL_MASK	GENMASK(7, 0)

struct ath12k_sdwf_svc {
	u16 dl_qos_id;
	bool configured;
	u16 ul_qos_id;
};

#define HTT_H2T_MSG_TYPE_ID			GENMASK(7, 0)
#define HTT_T2H_STREAMING_STATS_IND_HDR_SIZE	4
#define SAWF_TTH_TID_MASK			GENMASK(3, 0)
#define SAWF_TTH_QTYPE_MASK			GENMASK(7, 4)

/* MSG_TYPE => HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ
 *
 * @details
 * The following field definitions describe the format of the HTT host
 * to target message that requests the target to start or stop producing
 * ongoing stats of the specified type.
 *
 * |31|30	  |23	       16|15	       8|7	      0|
 * |-----------------------------------------------------------|
 * |EN| reserved  | stats type	 |    reserved	|   msg type   |
 * |-----------------------------------------------------------|
 * |		       config param [0]			       |
 * |-----------------------------------------------------------|
 * |		       config param [1]			       |
 * |-----------------------------------------------------------|
 * |		       config param [2]			       |
 * |-----------------------------------------------------------|
 * |		       config param [3]			       |
 * |-----------------------------------------------------------|
 * Where:
 *   - EN is an enable/disable flag
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this is a streaming stats upload request message
 *     Value: 0x20 (HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ)
 *   - STATS_TYPE
 *     Bits 23:16
 *     Purpose: identifies which FW statistics to upload
 *     Value: Defined values are HTT_STRM_GEN_MPDUS_STATS for basic
 *	      stats and HTT_STRM_GEN_MPDUS_DETAILS_STAT for extended
 *	      stats.
 *   - ENABLE
 *     Bit 31
 *     Purpose: enable/disable the target's ongoing stats of the specified type
 *     Value:
 *	   0 - disable ongoing production of the specified stats type
 *	   1 - enable  ongoing production of the specified stats type
 *   - CONFIG_PARAM [0]
 *     Bits 31:0
 *     Purpose: give an opaque configuration value to the specified stats type
 *     Value: stats-type specific configuration value
 *   - CONFIG_PARAM [1]
 *     Bits 31:0
 *     Purpose: give an opaque configuration value to the specified stats type
 *     Value: stats-type specific configuration value
 *   - CONFIG_PARAM [2]
 *     Bits 31:0
 *     Purpose: give an opaque configuration value to the specified stats type
 *     Value: stats-type specific configuration value
 *   - CONFIG_PARAM [3]
 *     Bits 31:0
 *     Purpose: give an opaque configuration value to the specified stats type
 *     Value: stats-type specific configuration value
 */

#define HTT_STRM_GEN_MPDUS_STATS			43
#define HTT_STRM_GEN_MPDUS_DETAILS_STATS		44
#define HTT_H2T_MSG_TYPE_STREAMING_STATS_TYPE		GENMASK(23, 16)
#define HTT_H2T_MSG_TYPE_STREAMING_STATS_CONFIGURE	BIT(31)

struct ath12k_htt_h2t_sawf_streaming_req {
	u32 info;
	u32 config_param_0;
	u32 config_param_1;
	u32 config_param_2;
	u32 config_param_3;
};

/**
 * @brief target -> host streaming statistics upload
 *
 * MSG_TYPE => HTT_T2H_MSG_TYPE_STREAMING_STATS_IND
 *
 * @details
 * The following field definitions describe the format of the HTT target
 * to host streaming stats upload indication message.
 * The host can use a HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ message to enable
 * the target to produce an ongoing series of HTT_T2H_MSG_TYPE_STREAMING_STATS_IND
 * STREAMING_STATS_IND messages, and can also use the
 * HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ message to halt the target's production of
 * HTT_T2H_MSG_TYPE_STREAMING_STATS_IND messages.
 *
 * The HTT_T2H_MSG_TYPE_STREAMING_STATS_IND message contains a payload of TLVs
 * containing the stats enabled by the host's HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ
 * message.
 *
 * |31						 8|7		 0|
 * |--------------------------------------------------------------|
 * |		       reserved			  |    msg type   |
 * |--------------------------------------------------------------|
 * |		       type-specific stats info			  |
 * |--------------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: Identifies this as a streaming statistics upload indication
 *	       message.
 *    Value: 0x2f (HTT_T2H_MSG_TYPE_STREAMING_STATS_IND)
 */

#define HTT_T2H_STREAMING_STATS_IND_HDR_SIZE	4
#define SAWF_TTH_TID_MASK			GENMASK(3, 0)
#define SAWF_TTH_QTYPE_MASK			GENMASK(7, 4)

struct htt_stats_strm_gen_mpdus_tlv {
/*
 * |31	   24|23      20|19	  16|15				 0|
 * |---------+----------+----------+------------------------------|
 * |Reserved |	 QTYPE	|   TID    |		Peer ID		  |
 * |--------------------------------------------------------------|
 * |	svc interval failure	   |   svc interval success	  |
 * |--------------------------------------------------------------|
 * |	burst size failure	   |   burst size success	  |
 * |--------------------------------------------------------------|
 */
	__le16 peer_id;
	__le16 info;
	__le16 svc_interval_success;
	__le16 svc_interval_failure;
	__le16 burst_size_success;
	__le16 burst_size_failure;
} __packed;

struct htt_stats_strm_gen_mpdus_details_tlv {
	__le16 peer_id;
	__le16 info;
	__le16 svc_interval_timestamp_prior_ms;
	__le16 svc_interval_timestamp_now_ms;
	__le16 svc_interval_interval_spec_ms;
	__le16 svc_interval_interval_margin_ms;
	/* consumed_bytes_orig:
	 * Raw count (actually estimate) of how many bytes were removed
	 * from the MSDU queue by the GEN_MPDUS operation.
	 */
	__le16 burst_size_consumed_bytes_orig;
	/* consumed_bytes_final:
	 * Adjusted count of removed bytes that incorporates normalizing
	 * by the actual service interval compared to the expected
	 * service interval.
	 * This allows the burst size computation to be independent of
	 * whether the target is doing GEN_MPDUS at only the service
	 * interval, or substantially more often than the service
	 * interval.
	 *     consumed_bytes_final = consumed_bytes_orig /
	 *	   (svc_interval / ref_svc_interval)
	 */
	__le16 burst_size_consumed_bytes_final;
	__le16 burst_size_remaining_bytes;
	__le16 burst_size_reserved;
	__le16 burst_size_burst_size_spec;
	__le16 burst_size_margin_bytes;
} __packed;

int ath12k_sdwf_map_service_class(struct ath12k_base *ab, u16 svc_id,
				  u16 dl_qos_id, u16 ul_qos_id);
int ath12k_sdwf_unmap_service_class(struct ath12k_base *ab, u16 svc_id);
bool ath12k_sdwf_service_configured(struct ath12k_base *ab, u16 svc_id);

u16 ath12k_sdwf_get_dl_qos_id(struct ath12k_base *ab, u16 svc_id);
u16 ath12k_sdwf_get_ul_qos_id(struct ath12k_base *ab, u16 svc_id);

u32 ath_encode_sdwf_metadata(u16 msduq_peer);

u16 ath12k_sdwf_get_msduq_peer(struct wireless_dev *wdev, u8 *peer_mac,
			       struct sawf_param *dl_params,
			       bool scs_mscs);

struct ath12k *ath12k_sdwf_get_ar_from_vif(struct wireless_dev *wdev,
					   struct ieee80211_vif *vif,
					   u8 *peer_mac, u16 *peer_id);
int ath12k_htt_sawf_streaming_stats_configure(struct ath12k *ar,
					      u8 stats_type,
					      u8 configure,
					      u32 config_param_0,
					      u32 config_param_1,
					      u32 config_param_2,
					      u32 config_param_3);
int ath12k_telemetry_get_sawf_tx_stats_tput(void *ptr, void *stats, u64 *in_bytes,
					    u64 *in_cnt, u64 *tx_bytes,
					    u64 *tx_cnt, u8 tid_v, u8 msduq_id);
int ath12k_telemetry_get_sawf_tx_stats_mpdu(void *ptr, void *stats, u64 *svc_int_pass,
					    u64 *svc_int_fail, u64 *burst_pass,
					    u64 *burst_fail, u8 tid_v, u8 msduq_id);
int ath12k_telemetry_get_sawf_tx_stats_drop(void *ptr, void *stats, u64 *pass,
					    u64 *drop, u64 *drop_ttl,
					    u8 tid, u8 msduq_id);
int ath12k_telemetry_get_msduq_tx_stats(void *soc, void *arg,
					void *msduq_tx_stats, u8 msduq);
int ath12k_telemetry_get_qos_stats(struct ath12k_vif *ahvif,
				   struct ath12k_telemetry_dp_peer *telemetry_peer,
				   struct ath12k_telemetry_command *cmd);
void
ath12k_sdwf_3_link_peer_dl_flow_count(struct wireless_dev *wdev,
                                      struct ieee80211_vif *vif, u8 *mac_addr,
                                      u32 mark_metadata, u16 svc_id);
#endif
