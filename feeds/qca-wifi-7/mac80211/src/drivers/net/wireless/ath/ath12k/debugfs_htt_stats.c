// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define HTT_CTRL_PATH_STATS_CAL_TYPE_STRINGS
#include <linux/vmalloc.h>
#include "core.h"
#include "debug.h"
#include "debugfs_htt_stats.h"
#include "dp_tx.h"
#include "dp_rx.h"

#define HTT_MAX_STRING_LEN 256
#define HTT_MAX_PRINT_CHAR_PER_ELEM 15

#define HTT_TLV_HDR_LEN 4

#define CHAIN_ARRAY_TO_BUF(out, buflen, arr, len)					\
	do {										\
		int index = 0; u8 i;							\
		for (i = 0; i < (len); i++) {						\
			index += scnprintf(((out) + (buflen)) + index,			\
			(ATH12K_HTT_STATS_BUF_SIZE - (buflen)) - index,			\
				" %u:%d,", i, arr[i]);					\
		}									\
		buflen += index;							\
	} while (0)

static u32
print_array_to_buf_index(u8 *buf, u32 offset, const char *header, u32 stats_index,
			 const __le32 *array, u32 array_len, const char *footer)
{
	int index = 0;
	u8 i;

	if (header) {
		index += scnprintf(buf + offset,
				   ATH12K_HTT_STATS_BUF_SIZE - offset,
				   "%s = ", header);
	}
	for (i = 0; i < array_len; i++) {
		index += scnprintf(buf + offset + index,
				   (ATH12K_HTT_STATS_BUF_SIZE - offset) - index,
				   " %u:%u,", stats_index++, le32_to_cpu(array[i]));
	}
	/* To overwrite the last trailing comma */
	index--;
	*(buf + offset + index) = '\0';

	if (footer) {
		index += scnprintf(buf + offset + index,
				   (ATH12K_HTT_STATS_BUF_SIZE - offset) - index,
				   "%s", footer);
	}
	return index;
}

static u32
print_array_to_buf(u8 *buf, u32 offset, const char *header,
		   const __le32 *array, u32 array_len, const char *footer)
{
	return print_array_to_buf_index(buf, offset, header, 0, array, array_len,
					footer);
}

static u32
print_array_to_buf_s8(u8 *buf, u32 offset, const char *header, u32 stats_index,
		      const s8 *array, u32 array_len, const char *footer)
{
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int index = 0;
	u8 i;

	if (header)
		index += scnprintf(buf + offset, buf_len - offset, "%s = ", header);

	for (i = 0; i < array_len; i++) {
		index += scnprintf(buf + offset + index, (buf_len - offset) - index,
				   " %u:%d,", stats_index++, array[i]);
	}

	index--;
	if ((offset + index) < buf_len)
		buf[offset + index] = '\0';

	if (footer) {
		index += scnprintf(buf + offset + index, (buf_len - offset) - index,
				   "%s", footer);
	}

	return index;
}

static u32
print_array_to_buf_s32(u8 *buf, u32 offset, const char *header, u32 stats_index,
		       const s32 *array, u32 array_len, const char *footer)
{
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int index = 0;
	u8 i;

	if (header)
		index += scnprintf(buf + offset, buf_len - offset, "%s = ", header);

	for (i = 0; i < array_len; i++) {
		index += scnprintf(buf + offset + index, (buf_len - offset) - index,
				   " %u:%d,", stats_index++, array[i]);
	}

	index--;
	if ((offset + index) < buf_len)
		buf[offset + index] = '\0';

	if (footer) {
		index += scnprintf(buf + offset + index, (buf_len - offset) - index,
				   "%s", footer);
	}

	return index;
}

static void htt_print_hds_prof_stats_tlv(const void *tag_buf, u16 tag_len,
					  struct debug_htt_stats_req *stats_req)
{
	const struct htt_stats_hds_prof_stats_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE, len = stats_req->buf_len;
	u8 *buf = stats_req->buf, i, j, k;

	if(tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "\nChannel Change Timings:\n");
	len += scnprintf(buf + len, buf_len - len,
			"\n|===================================================================="
			"====================================================================="
			"===============================================================|\n");

	j = htt_stats_buf->idx < HTT_STATS_HDS_PROF_STATS_CIRCULAR_BUF_LEN ?
		htt_stats_buf->idx : HTT_STATS_HDS_PROF_STATS_CIRCULAR_BUF_LEN;

	len += scnprintf(buf + len, buf_len - len,
			"|%-15s|%-15s|%-15s|%-15s|%-15s|"
			 "%-24s|%-15s|%-15s|"
			 "%-15s|%-15s|%-15s|%-15s|",
			"CHANNEL", "CENTER_FREQ", "PHYMODE", "TX_CHAINMASK", "RX_CHAINMASK",
			"CHANNEL_SWITCH_TIME(us)", "INI(us)", "TPC+CTL(us)",
			"CAL(us)", "MISC(us)", "CTL(us)", "SW_PROFILE");
	len += scnprintf(buf + len, buf_len - len,
			"\n|===================================================================="
			"====================================================================="
			"===============================================================|\n");

	for (k = 0; k < j; k++) {
		i = j-1-k;
		len += scnprintf(buf + len, buf_len - len,
				"|%-15u|%-15u|%-15u|%-15u|%-15u|"
				"%-24u|%-15u|%-15u|"
				"%-15u|%-15u|%-15u|%-15u|",
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].bandwidth_mhz),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].band_center_freq1),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].phyMode),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].txChainmask),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].rxChainmask),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].channelSwitchTime),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].iniModuleTime),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].tpcModuleTime),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].calModuleTime),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].miscModuleTime),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].ctlModuleTime),
				le32_to_cpu(htt_stats_buf->channelChange_stats[i].swProfile));

	len += scnprintf(buf + len, buf_len - len,
			"\n|````````````````````````````````````````````````````````````````"
			"`````````````````````````````````````````````````````````````````"
			"`````````````````````````````````````````````````````````````````"
			"``````|\n");
	}
	len += scnprintf(buf + len, buf_len - len,
			"|===================================================================="
			"====================================================================="
			"===============================================================|\n");

	stats_req->buf_len = len;
}

static void ath12k_htt_print_htt_stats_gtx_stats_tlv_v(const void *tag_buf, u16 tag_len,
						       struct debug_htt_stats_req *stats_req)
{
	const struct htt_stats_gtx_stats *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_GTX_TAG\n");
	len += scnprintf(buf + len, buf_len - len, "Green TX Enabled: %u\n",
			 le32_to_cpu(htt_stats_buf->gtx_enabled));
	len += scnprintf(buf + len, buf_len - len, "MIN TPC (0.25 dBm) = "
			 " 0:%u 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u 7:%u"
			 " 8:%u 9:%u 10:%u 11:%u 12:%u 13:%u 14:%u 15:%u\n",
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[0]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[1]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[2]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[3]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[4]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[5]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[6]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[7]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[8]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[9]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[10]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[11]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[12]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[13]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[14]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_min[15]));

	len += scnprintf(buf + len, buf_len - len, "MAX TPC (0.25 dBm) = "
			 " 0:%u 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u 7:%u"
			 " 8:%u 9:%u 10:%u 11:%u 12:%u 13:%u 14:%u 15:%u\n",
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[0]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[1]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[2]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[3]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[4]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[5]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[6]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[7]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[8]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[9]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[10]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[11]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[12]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[13]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[14]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_max[15]));

	len += scnprintf(buf + len, buf_len - len, "TPC DIFF MCS (0.25 dB) = "
			 " 0:%u 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u 7:%u"
			 " 8:%u 9:%u 10:%u 11:%u 12:%u 13:%u 14:%u 15:%u\n",
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[0]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[1]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[2]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[3]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[4]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[5]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[6]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[7]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[8]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[9]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[10]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[11]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[12]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[13]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[14]),
			 le32_to_cpu(htt_stats_buf->mcs_tpc_diff[15]));

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_ml_peer_details_stats_tlv(const void *tag_buf,
					   struct debug_htt_stats_req *stats_req)
{
	const struct htt_ml_peer_details_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_ML_PEER_DETAILS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len,
			 "========================\n");

	len += scnprintf(buf + len, buf_len - len,
			 "remote_mld_mac_addr = %02x:%02x:%02x:%02x:%02x:%02x\n",
			 (htt_stats_buf->remote_mld_mac_addr.mac_addr_l32 & 0xFF),
			 (htt_stats_buf->remote_mld_mac_addr.mac_addr_l32 & 0xFF00) >> 8,
			 (htt_stats_buf->remote_mld_mac_addr.mac_addr_l32 & 0xFF0000) >> 16,
			 (htt_stats_buf->remote_mld_mac_addr.mac_addr_l32 & 0xFF000000) >> 24,
			 (htt_stats_buf->remote_mld_mac_addr.mac_addr_h16 & 0xFF),
			 (htt_stats_buf->remote_mld_mac_addr.mac_addr_h16 & 0xFF00) >> 8);

	len += scnprintf(buf + len, buf_len - len,
			 "ml_peer_flags = 0x%x\n",
			 htt_stats_buf->ml_peer_flags);

	len += scnprintf(buf + len, buf_len - len,
		"num_links = %u\n",
		HTT_ML_PEER_DETAILS_NUM_LINKS_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"ml_peer_id = %u\n",
		HTT_ML_PEER_DETAILS_ML_PEER_ID_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"primary_link_idx = %u\n",
		HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"primary_chip_id = %u\n",
		HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"link_init_count = %u\n",
		HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"non_str = %u\n",
		HTT_ML_PEER_DETAILS_NON_STR_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"emlsr = %u\n",
		HTT_ML_PEER_DETAILS_EMLSR_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"is_stako = %u\n",
		HTT_ML_PEER_DETAILS_IS_STA_KO_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"num_local_links = %u\n",
		HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"allocated = %u\n",
		HTT_ML_PEER_DETAILS_ALLOCATED_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
		"participating_chips_bitmap = 0x%x\n",
		HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_GET(
			htt_stats_buf->msg_dword_2));

	len += scnprintf(buf + len, buf_len - len,
			 "=========================================== \n");
	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_ml_peer_ext_stats_tlv(const void *tag_buf,
				       struct debug_htt_stats_req *stats_req)
{
	const struct htt_ml_peer_ext_details_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_ML_PEER_EXT_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "====================\n");
	len += scnprintf(buf + len, buf_len - len,
			 "peer_assoc_ipc_recvd    = %u\n",
			 HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_GET(
				 htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "sched_peer_delete_recvd = %u\n",
			 HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_GET(
				 htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "mld_ast_index           = %u\n",
			 HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_GET(htt_stats_buf->msg_dword_1));

	len += scnprintf(buf + len, buf_len - len,
			 "=========================================== \n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_ml_link_info_stats_tlv(const void *tag_buf,
					struct debug_htt_stats_req *stats_req)
{
	const struct htt_ml_link_info_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 msg_dword_1     = le32_to_cpu(htt_stats_buf->msg_dword_1);
	u32 msg_dword_2     = le32_to_cpu(htt_stats_buf->msg_dword_2);

	len += scnprintf(buf + len, buf_len - len, "HTT_ML_LINK_INFO_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "=====================\n");
	len += scnprintf(buf + len, buf_len - len,
			 "valid             = %u\n",
			 HTT_ML_LINK_INFO_VALID_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "active            = %u\n",
			 HTT_ML_LINK_INFO_ACTIVE_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "primary           = %u\n",
			 HTT_ML_LINK_INFO_PRIMARY_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "assoc_link        = %u\n",
			 HTT_ML_LINK_INFO_ASSOC_LINK_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "chip_id           = %u\n",
			 HTT_ML_LINK_INFO_CHIP_ID_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "ieee_link_id      = %u\n",
			 HTT_ML_LINK_INFO_IEEE_LINK_ID_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "hw_link_id        = %u\n",
			 HTT_ML_LINK_INFO_HW_LINK_ID_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "logical_link_id   = %u\n",
			 HTT_ML_LINK_INFO_LOGICAL_LINK_ID_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "master_link       = %u\n",
			 HTT_ML_LINK_INFO_MASTER_LINK_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "anchor_link       = %u\n",
			 HTT_ML_LINK_INFO_ANCHOR_LINK_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "initialized       = %u\n",
			 HTT_ML_LINK_INFO_INITIALIZED_GET(htt_stats_buf->msg_dword_1));
	len += scnprintf(buf + len, buf_len - len,
			 "bridge_peer       = %u\n",
			 u32_get_bits(msg_dword_1, HTT_STATS_ML_LINK_INFO_BRIDGE_PEER));

	len += scnprintf(buf + len, buf_len - len,
			 "sw_peer_id        = %u\n",
			 HTT_ML_LINK_INFO_SW_PEER_ID_GET(htt_stats_buf->msg_dword_2));
	len += scnprintf(buf + len, buf_len - len,
			 "vdev_id           = %u\n",
			 HTT_ML_LINK_INFO_VDEV_ID_GET(htt_stats_buf->msg_dword_2));
	len += scnprintf(buf + len, buf_len - len,
			 "PS state          = %u\n",
			 u32_get_bits(msg_dword_2, HTT_STATS_ML_LINK_INFO_PS_STATE));

	len += scnprintf(buf + len, buf_len - len,
			 "primary_tid_mask  = 0x%x\n",
			 htt_stats_buf->primary_tid_mask);
	len += scnprintf(buf + len, buf_len - len,
			 "=========================================== \n");
	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_txbf_ofdma_be_ndpa_stats_tlv(const void *tag_buf,
					      struct debug_htt_stats_req *stats_req)
{
	const struct htt_txbf_ofdma_be_ndpa_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int i, null_output;
	u32 num_elements = htt_stats_buf->num_elems_be_ndpa_arr;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TXBF_OFDMA_BE_NDPA_STATS_TLV:\n");

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_queued) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_queued);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndpa_queued = %s\n", "NONE");
	}

	null_output = 1;

	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_tried) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_tried);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndpa_tried = %s\n", "NONE");
	}

	null_output = 1;

	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_flushed) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_flushed);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndpa_flushed = %s\n", "NONE");
	}

	null_output = 1;

	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_err) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndpa[i].be_ofdma_ndpa_err);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndpa_err = %s\n", "NONE");
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_txbf_ofdma_be_ndp_stats_tlv(const void *tag_buf,
					     struct debug_htt_stats_req *stats_req)
{
	const struct htt_txbf_ofdma_be_ndp_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int i, null_output;

	u32 num_elements = htt_stats_buf->num_elems_be_ndp_arr;

	len += scnprintf(buf + len, buf_len - len, "HTT_TXBF_OFDMA_BE_NDP_STATS_TLV:\n");
	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndp[i].be_ofdma_ndp_queued) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndp[i].be_ofdma_ndp_queued);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndp_queued = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndp[i].be_ofdma_ndp_flushed) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndp[i].be_ofdma_ndp_flushed);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndp_flushed = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndp[i].be_ofdma_ndp_err) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndp[i].be_ofdma_ndp_err);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndp_err = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_ndp[i].be_ofdma_ndp_tried) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_ndp[i].be_ofdma_ndp_tried);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_ndp_tried = %s\n", "NONE");
	}
	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_txbf_ofdma_be_brp_stats_tlv(const void *tag_buf,
					     struct debug_htt_stats_req *stats_req)
{
	const struct htt_txbf_ofdma_be_brp_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int i, null_output;

	u32 num_elements = htt_stats_buf->num_elems_be_brp_arr;

	len += scnprintf(buf + len, buf_len - len, "HTT_TXBF_OFDMA_BE_BRP_STATS_TLV:\n");
	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_brp[i].be_ofdma_brpoll_queued) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_brp[i].be_ofdma_brpoll_queued);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_brpoll_queued = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_brp[i].be_ofdma_brpoll_tried) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_brp[i].be_ofdma_brpoll_tried);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_brpoll_tried = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_brp[i].be_ofdma_brpoll_flushed) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_brp[i].be_ofdma_brpoll_flushed);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_brpoll_flushed = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_brp[i].be_ofdma_brp_err) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_brp[i].be_ofdma_brp_err);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_brp_err = %s\n", "NONE");
	}
	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_brp[i].be_ofdma_brp_err_num_cbf_rcvd) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_brp[i].be_ofdma_brp_err_num_cbf_rcvd);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_brp_err_num_cbf_rcvd = %s\n", "NONE");
	}
	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_txbf_ofdma_be_steer_stats_tlv(const void *tag_buf,
					       struct debug_htt_stats_req *stats_req)
{
	const struct htt_txbf_ofdma_be_steer_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int i, null_output;

	u32 num_elements = htt_stats_buf->num_elems_be_steer_arr;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TXBF_OFDMA_BE_STEER_STATS_TLV:\n");

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_steer[i].be_ofdma_num_ppdu_steer) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_steer[i].be_ofdma_num_ppdu_steer);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_num_ppdu_steer = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_steer[i].be_ofdma_num_ppdu_ol) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_steer[i].be_ofdma_num_ppdu_ol);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_num_ppdu_ol = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_steer[i].be_ofdma_num_usrs_prefetch) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_steer[i].be_ofdma_num_usrs_prefetch);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_num_usrs_prefetch = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_steer[i].be_ofdma_num_usrs_sound) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_steer[i].be_ofdma_num_usrs_sound);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_num_usrs_sound = %s\n", "NONE");
	}

	null_output = 1;
	for (i = 0; i < num_elements; i++) {
		if (htt_stats_buf->be_steer[i].be_ofdma_num_usrs_force_sound) {
			null_output = 0;
			len += scnprintf(buf + len, buf_len - len,
				" %u:%u,", i + 1,
				htt_stats_buf->be_steer[i].be_ofdma_num_usrs_force_sound);
		}
	}
	if (null_output) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_num_usrs_force_sound = %s\n", "NONE");
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_txbf_ofdma_be_steer_mpdu_stats_tlv(const void *tag_buf, u16 tag_len,
						    struct debug_htt_stats_req *stat_req)
{
	const struct ath12k_htt_txbf_ofdma_be_steer_mpdu_stats_tlv *stats_buf = tag_buf;
	u8 *buf = stat_req->buf;
	u32 len = stat_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TXBF_OFDMA_BE_STEER_MPDU_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "rbo_steer_mpdus_tried = %u\n",
			 le32_to_cpu(stats_buf->be_ofdma_rbo_steer_mpdus_tried));
	len += scnprintf(buf + len, buf_len - len, "rbo_steer_mpdus_failed = %u\n",
			 le32_to_cpu(stats_buf->be_ofdma_rbo_steer_mpdus_failed));
	len += scnprintf(buf + len, buf_len - len, "sifs_steer_mpdus_tried = %u\n",
			 le32_to_cpu(stats_buf->be_ofdma_sifs_steer_mpdus_tried));
	len += scnprintf(buf + len, buf_len - len, "sifs_steer_mpdus_failed = %u\n",
			 le32_to_cpu(stats_buf->be_ofdma_sifs_steer_mpdus_failed));

	stat_req->buf_len = len;
}

static inline void
ath12k_htt_print_be_ul_ofdma_user_stats(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_pdev_be_ul_ofdma_user_stats_tlv *htt_ul_user_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_ul_user_stats_buf))
		return;

	if (htt_ul_user_stats_buf->user_index == 0) {
		len += scnprintf(buf + len, buf_len - len,
				"HTT_RX_PDEV_BE_UL_OFDMA_USER_STAS_TLV\n");
	}

	len += scnprintf(buf + len, buf_len - len,
			 "be_rx_ulofdma_non_data_ppdu_%u = %u\n",
			 htt_ul_user_stats_buf->user_index,
			 htt_ul_user_stats_buf->be_rx_ulofdma_non_data_ppdu);
	len += scnprintf(buf + len, buf_len - len,
			 "be_rx_ulofdma_data_ppdu_%u = %u\n",
			 htt_ul_user_stats_buf->user_index,
			 htt_ul_user_stats_buf->be_rx_ulofdma_data_ppdu);
	len += scnprintf(buf + len, buf_len - len,
			 "be_rx_ulofdma_mpdu_ok_%u = %u\n",
			 htt_ul_user_stats_buf->user_index,
			 htt_ul_user_stats_buf->be_rx_ulofdma_mpdu_ok);
	len += scnprintf(buf + len, buf_len - len,
			 "be_rx_ulofdma_mpdu_fail_%u = %u\n",
			 htt_ul_user_stats_buf->user_index,
			 htt_ul_user_stats_buf->be_rx_ulofdma_mpdu_fail);
	len += scnprintf(buf + len, buf_len - len,
			 "be_rx_ulofdma_non_data_nusers_%u = %u\n",
			 htt_ul_user_stats_buf->user_index,
			 htt_ul_user_stats_buf->be_rx_ulofdma_non_data_nusers);
	len += scnprintf(buf + len, buf_len - len,
			 "be_rx_ulofdma_data_nusers_%u = %u\n",
			 htt_ul_user_stats_buf->user_index,
			 htt_ul_user_stats_buf->be_rx_ulofdma_data_nusers);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_bn_ul_ofdma_user_stats(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_pdev_bn_ul_ofdma_user_stats_tlv
						*htt_ul_user_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_ul_user_stats_buf))
		return;

	if (le32_to_cpu(htt_ul_user_stats_buf->user_index) == 0) {
		len += scnprintf(buf + len, buf_len - len,
				"HTT_RX_PDEV_BN_UL_OFDMA_USER_STAS_TLV\n");
	}

	len += scnprintf(buf + len, buf_len - len,
			 "bn_rx_ulofdma_non_data_ppdu_%u = %u\n",
			 le32_to_cpu(htt_ul_user_stats_buf->user_index),
			 le32_to_cpu(htt_ul_user_stats_buf->bn_rx_ulofdma_non_data_ppdu));
	len += scnprintf(buf + len, buf_len - len,
			 "bn_rx_ulofdma_data_ppdu_%u = %u\n",
			 le32_to_cpu(htt_ul_user_stats_buf->user_index),
			 le32_to_cpu(htt_ul_user_stats_buf->bn_rx_ulofdma_data_ppdu));
	len += scnprintf(buf + len, buf_len - len,
			 "bn_rx_ulofdma_mpdu_ok_%u = %u\n",
			 le32_to_cpu(htt_ul_user_stats_buf->user_index),
			 le32_to_cpu(htt_ul_user_stats_buf->bn_rx_ulofdma_mpdu_ok));
	len += scnprintf(buf + len, buf_len - len,
			 "bn_rx_ulofdma_mpdu_fail_%u = %u\n",
			 le32_to_cpu(htt_ul_user_stats_buf->user_index),
			 le32_to_cpu(htt_ul_user_stats_buf->bn_rx_ulofdma_mpdu_fail));
	len += scnprintf(buf + len, buf_len - len,
			 "bn_rx_ulofdma_non_data_nusers_%u = %u\n",
			 le32_to_cpu(htt_ul_user_stats_buf->user_index),
			 le32_to_cpu(htt_ul_user_stats_buf->bn_rx_ulofdma_non_data_nusers
			 ));
	len += scnprintf(buf + len, buf_len - len,
			 "bn_rx_ulofdma_data_nusers_%u = %u\n",
			 le32_to_cpu(htt_ul_user_stats_buf->user_index),
			 le32_to_cpu(htt_ul_user_stats_buf->bn_rx_ulofdma_data_nusers));

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_unavailable_error_stats_tlv(const void *tag_buf,
					     struct debug_htt_stats_req *stats_req)
{
	const struct htt_stats_error_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_ERROR_STATS_TLV:");
	len += scnprintf(buf + len, buf_len - len,
			 "No stats to print for current request: %d",
			 htt_stats_buf->htt_stats_type);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_unsupported_error_stats_tlv(const void *tag_buf,
					     struct debug_htt_stats_req *stats_req)
{
	const struct htt_stats_error_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_ERROR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "Unsupported HTT stats type: %d\n",
			 htt_stats_buf->htt_stats_type);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_vdev_txrx_stats_hw_tlv(const void *tag_buf,
					struct debug_htt_stats_req *stats_req)
{
	const struct htt_t2h_vdev_txrx_stats_hw_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_VDEV_TXRX_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "vdev_id = %u\n",
			 le32_to_cpu(htt_stats_buf->vdev_id));

	len += scnprintf(buf + len, buf_len - len, "rx_msdu_byte_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->rx_msdu_byte_cnt_hi),
			 le32_to_cpu(htt_stats_buf->rx_msdu_byte_cnt_lo));

	len += scnprintf(buf + len, buf_len - len, "rx_msdu_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->rx_msdu_cnt_hi),
			 le32_to_cpu(htt_stats_buf->rx_msdu_cnt_lo));

	len += scnprintf(buf + len, buf_len - len, "tx_msdu_byte_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tx_msdu_byte_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tx_msdu_byte_cnt_lo));

	len += scnprintf(buf + len, buf_len - len, "tx_msdu_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tx_msdu_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tx_msdu_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tx_msdu_excessive_retry_discard_cnt = 0x%08x%08x\n",
			 le32_to_cpu
			 (htt_stats_buf->tx_msdu_excessive_retry_discard_cnt_hi),
			 le32_to_cpu
			 (htt_stats_buf->tx_msdu_excessive_retry_discard_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tx_msdu_cong_ctrl_drop_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tx_msdu_cong_ctrl_drop_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tx_msdu_cong_ctrl_drop_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tx_msdu_ttl_expire_drop_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tx_msdu_ttl_expire_drop_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tx_msdu_ttl_expire_drop_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tx_msdu_excessive_retry_discard_byte_cnt = 0x%08x%08x\n",
			 le32_to_cpu
			 (htt_stats_buf->tx_msdu_excessive_retry_discard_byte_cnt_hi),
			 le32_to_cpu
			 (htt_stats_buf->tx_msdu_excessive_retry_discard_byte_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tx_msdu_cong_ctrl_drop_byte_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tx_msdu_cong_ctrl_drop_byte_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tx_msdu_cong_ctrl_drop_byte_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tx_msdu_ttl_expire_drop_byte_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tx_msdu_ttl_expire_drop_byte_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tx_msdu_ttl_expire_drop_byte_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tqm_bypass_frame_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tqm_bypass_frame_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tqm_bypass_frame_cnt_lo));

	len += scnprintf(buf + len, buf_len - len,
			 "tqm_bypass_byte_cnt = 0x%08x%08x\n",
			 le32_to_cpu(htt_stats_buf->tqm_bypass_byte_cnt_hi),
			 le32_to_cpu(htt_stats_buf->tqm_bypass_byte_cnt_lo));

	len += scnprintf(buf + len, buf_len - len, "===============================\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_stats_phy_err_tlv_v(const void *tag_buf,
					     u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_pdev_stats_phy_err_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2), ATH12K_HTT_TX_PDEV_MAX_PHY_ERR_STATS);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_STATS_PHY_ERR_TLV_V:");

	len += print_array_to_buf(buf, len, "phys_errs",htt_stats_buf->phy_errs,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_stats_tx_ppdu_stats_tlv_v(const void *tag_buf,
						   struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_pdev_stats_tx_ppdu_stats_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_PDEV_STATS_TX_PPDU_STATS_TLV_V:\n");

	len += scnprintf(buf + len, buf_len - len, "num_data_ppdus_legacy_su = %u\n",
			   htt_stats_buf->num_data_ppdus_legacy_su);

	len += scnprintf(buf + len, buf_len - len, "num_data_ppdus_ac_su = %u\n",
			   htt_stats_buf->num_data_ppdus_ac_su);

	len += scnprintf(buf + len, buf_len - len, "num_data_ppdus_ax_su = %u\n",
			   htt_stats_buf->num_data_ppdus_ax_su);

	len += scnprintf(buf + len, buf_len - len, "num_data_ppdus_ac_su_txbf = %u\n",
			   htt_stats_buf->num_data_ppdus_ac_su_txbf);

	len += scnprintf(buf + len, buf_len - len, "num_data_ppdus_ax_su_txbf = %u\n",
			   htt_stats_buf->num_data_ppdus_ax_su_txbf);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_stats_tried_mpdu_cnt_hist_tlv_v(const void *tag_buf,
							 u16 tag_len,
							 struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_pdev_stats_tried_mpdu_cnt_hist_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32  num_elements = ((tag_len - sizeof(htt_stats_buf->hist_bin_size)) >> 2);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_PDEV_STATS_TRIED_MPDU_CNT_HIST_TLV_V:\n");
	len += scnprintf(buf + len, buf_len - len, "TRIED_MPDU_CNT_HIST_BIN_SIZE : %u\n",
			   htt_stats_buf->hist_bin_size);

	len += print_array_to_buf(buf, len, "tried_mpdu_cnt_hist = %s\n",
			   htt_stats_buf->tried_mpdu_cnt_hist,
			   num_elements, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_hw_stats_wd_timeout_tlv(const void *tag_buf,
					 struct debug_htt_stats_req *stats_req)
{
	const struct htt_hw_stats_wd_timeout_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	char hw_module_name[HTT_STATS_MAX_HW_MODULE_NAME_LEN + 1] = {0};

	len += scnprintf(buf + len, buf_len - len, "HTT_HW_STATS_WD_TIMEOUT_TLV:\n");
	memcpy(hw_module_name, &(htt_stats_buf->hw_module_name[0]),
	       HTT_STATS_MAX_HW_MODULE_NAME_LEN);
	len += scnprintf(buf + len, buf_len - len, "hw_module_name = %s\n",
			   hw_module_name);
	len += scnprintf(buf + len, buf_len - len, "count = %u\n",
			   htt_stats_buf->count);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_msdu_flow_stats_tlv(const void *tag_buf,
							struct debug_htt_stats_req *stats_req)
{
	const struct htt_msdu_flow_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_MSDU_FLOW_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "last_update_timestamp = %u\n",
			   htt_stats_buf->last_update_timestamp);
	len += scnprintf(buf + len, buf_len - len, "last_add_timestamp = %u\n",
			   htt_stats_buf->last_add_timestamp);
	len += scnprintf(buf + len, buf_len - len, "last_remove_timestamp = %u\n",
			   htt_stats_buf->last_remove_timestamp);
	len += scnprintf(buf + len, buf_len - len, "total_processed_msdu_count = %u\n",
			   htt_stats_buf->total_processed_msdu_count);
	len += scnprintf(buf + len, buf_len - len, "cur_msdu_count_in_flowq = %u\n",
			   htt_stats_buf->cur_msdu_count_in_flowq);
	len += scnprintf(buf + len, buf_len - len, "sw_peer_id = %u\n",
			   htt_stats_buf->sw_peer_id);
	len += scnprintf(buf + len, buf_len - len, "tx_flow_no = %u\n",
			   htt_stats_buf->tx_flow_no__tid_num__drop_rule & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "tid_num = %u\n",
			   (htt_stats_buf->tx_flow_no__tid_num__drop_rule & 0xF0000) >>
			   16);
	len += scnprintf(buf + len, buf_len - len, "drop_rule = %u\n",
			   (htt_stats_buf->tx_flow_no__tid_num__drop_rule & 0x100000) >>
			   20);
	len += scnprintf(buf + len, buf_len - len, "last_cycle_enqueue_count = %u\n",
			   htt_stats_buf->last_cycle_enqueue_count);
	len += scnprintf(buf + len, buf_len - len, "last_cycle_dequeue_count = %u\n",
			   htt_stats_buf->last_cycle_dequeue_count);
	len += scnprintf(buf + len, buf_len - len, "last_cycle_drop_count = %u\n",
			   htt_stats_buf->last_cycle_drop_count);
	len += scnprintf(buf + len, buf_len - len, "current_drop_th = %u\n",
			   htt_stats_buf->current_drop_th);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_tx_tid_stats_tlv(const void *tag_buf,
						     struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_tid_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	char tid_name[MAX_HTT_TID_NAME + 1] = {0};

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TID_STATS_TLV:\n");
	memcpy(tid_name, &(htt_stats_buf->tid_name[0]), MAX_HTT_TID_NAME);
	len += scnprintf(buf + len, buf_len - len, "tid_name = %s\n", tid_name);
	len += scnprintf(buf + len, buf_len - len, "sw_peer_id = %u\n",
			   htt_stats_buf->sw_peer_id__tid_num & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "tid_num = %u\n",
			   (htt_stats_buf->sw_peer_id__tid_num & 0xFFFF0000) >> 16);
	len += scnprintf(buf + len, buf_len - len, "num_sched_pending = %u\n",
			   htt_stats_buf->num_sched_pending__num_ppdu_in_hwq & 0xFF);
	len += scnprintf(buf + len, buf_len - len, "num_ppdu_in_hwq = %u\n",
			   (htt_stats_buf->num_sched_pending__num_ppdu_in_hwq &
			   0xFF00) >> 8);
	len += scnprintf(buf + len, buf_len - len, "tid_flags = 0x%x\n",
			   htt_stats_buf->tid_flags);
	len += scnprintf(buf + len, buf_len - len, "hw_queued = %u\n",
			   htt_stats_buf->hw_queued);
	len += scnprintf(buf + len, buf_len - len, "hw_reaped = %u\n",
			   htt_stats_buf->hw_reaped);
	len += scnprintf(buf + len, buf_len - len, "mpdus_hw_filter = %u\n",
			   htt_stats_buf->mpdus_hw_filter);
	len += scnprintf(buf + len, buf_len - len, "qdepth_bytes = %u\n",
			   htt_stats_buf->qdepth_bytes);
	len += scnprintf(buf + len, buf_len - len, "qdepth_num_msdu = %u\n",
			   htt_stats_buf->qdepth_num_msdu);
	len += scnprintf(buf + len, buf_len - len, "qdepth_num_mpdu = %u\n",
			   htt_stats_buf->qdepth_num_mpdu);
	len += scnprintf(buf + len, buf_len - len, "last_scheduled_tsmp = %u\n",
			   htt_stats_buf->last_scheduled_tsmp);
	len += scnprintf(buf + len, buf_len - len, "pause_module_id = %u\n",
			   htt_stats_buf->pause_module_id);
	len += scnprintf(buf + len, buf_len - len, "block_module_id = %u\n",
			   htt_stats_buf->block_module_id);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_tx_tid_stats_v1_tlv(const void *tag_buf,
							struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_tid_stats_v1_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	char tid_name[MAX_HTT_TID_NAME + 1] = {0};

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TID_STATS_V1_TLV:");
	memcpy(tid_name, &(htt_stats_buf->tid_name[0]), MAX_HTT_TID_NAME);
	len += scnprintf(buf + len, buf_len - len, "tid_name = %s\n", tid_name);
	len += scnprintf(buf + len, buf_len - len, "sw_peer_id = %u\n",
			   htt_stats_buf->sw_peer_id__tid_num & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "tid_num = %u\n",
			   (htt_stats_buf->sw_peer_id__tid_num & 0xFFFF0000) >> 16);
	len += scnprintf(buf + len, buf_len - len, "num_sched_pending = %u\n",
			   htt_stats_buf->num_sched_pending__num_ppdu_in_hwq & 0xFF);
	len += scnprintf(buf + len, buf_len - len, "num_ppdu_in_hwq = %u\n",
			   (htt_stats_buf->num_sched_pending__num_ppdu_in_hwq &
			   0xFF00) >> 8);
	len += scnprintf(buf + len, buf_len - len, "tid_flags = 0x%x\n",
			   htt_stats_buf->tid_flags);
	len += scnprintf(buf + len, buf_len - len, "max_qdepth_bytes = %u\n",
			   htt_stats_buf->max_qdepth_bytes);
	len += scnprintf(buf + len, buf_len - len, "max_qdepth_n_msdus = %u\n",
			   htt_stats_buf->max_qdepth_n_msdus);
	len += scnprintf(buf + len, buf_len - len, "rsvd = %u\n",
			   htt_stats_buf->rsvd);
	len += scnprintf(buf + len, buf_len - len, "qdepth_bytes = %u\n",
			   htt_stats_buf->qdepth_bytes);
	len += scnprintf(buf + len, buf_len - len, "qdepth_num_msdu = %u\n",
			   htt_stats_buf->qdepth_num_msdu);
	len += scnprintf(buf + len, buf_len - len, "qdepth_num_mpdu = %u\n",
			   htt_stats_buf->qdepth_num_mpdu);
	len += scnprintf(buf + len, buf_len - len, "last_scheduled_tsmp = %u\n",
			   htt_stats_buf->last_scheduled_tsmp);
	len += scnprintf(buf + len, buf_len - len, "pause_module_id = %u\n",
			   htt_stats_buf->pause_module_id);
	len += scnprintf(buf + len, buf_len - len, "block_module_id = %u\n",
			   htt_stats_buf->block_module_id);
	len += scnprintf(buf + len, buf_len - len, "allow_n_flags = 0x%x\n",
			   htt_stats_buf->allow_n_flags);
	len += scnprintf(buf + len, buf_len - len, "sendn_frms_allowed = %u\n",
			   htt_stats_buf->sendn_frms_allowed);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_rx_tid_stats_tlv(const void *tag_buf,
						     struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_tid_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	char tid_name[MAX_HTT_TID_NAME + 1] = {0};

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_TID_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "sw_peer_id = %u\n",
			   htt_stats_buf->sw_peer_id__tid_num & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "tid_num = %u\n",
			   (htt_stats_buf->sw_peer_id__tid_num & 0xFFFF0000) >> 16);
	memcpy(tid_name, &(htt_stats_buf->tid_name[0]), MAX_HTT_TID_NAME);
	len += scnprintf(buf + len, buf_len - len, "tid_name = %s\n", tid_name);
	len += scnprintf(buf + len, buf_len - len, "dup_in_reorder = %u\n",
			   htt_stats_buf->dup_in_reorder);
	len += scnprintf(buf + len, buf_len - len, "dup_past_outside_window = %u\n",
			   htt_stats_buf->dup_past_outside_window);
	len += scnprintf(buf + len, buf_len - len, "dup_past_within_window = %u\n",
			   htt_stats_buf->dup_past_within_window);
	len += scnprintf(buf + len, buf_len - len, "rxdesc_err_decrypt = %u\n",
			   htt_stats_buf->rxdesc_err_decrypt);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_counter_tlv(const void *tag_buf,
						struct debug_htt_stats_req *stats_req)
{
	const struct htt_counter_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_COUNTER_TLV:\n");

	len += print_array_to_buf(buf, len, "counter_name = %s\n",
			   htt_stats_buf->counter_name,
			   HTT_MAX_COUNTER_NAME, "\n\n");

	len += scnprintf(buf + len, buf_len - len, "count = %u\n",
			   htt_stats_buf->count);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_peer_stats_cmn_tlv(const void *tag_buf,
						       struct debug_htt_stats_req *stats_req)
{
	const struct htt_peer_stats_cmn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_PEER_STATS_CMN_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ppdu_cnt = %u\n",
			   htt_stats_buf->ppdu_cnt);
	len += scnprintf(buf + len, buf_len - len, "mpdu_cnt = %u\n",
			   htt_stats_buf->mpdu_cnt);
	len += scnprintf(buf + len, buf_len - len, "msdu_cnt = %u\n",
			   htt_stats_buf->msdu_cnt);
	len += scnprintf(buf + len, buf_len - len, "pause_bitmap = %u\n",
			   htt_stats_buf->pause_bitmap);
	len += scnprintf(buf + len, buf_len - len, "block_bitmap = %u\n",
			   htt_stats_buf->block_bitmap);
	len += scnprintf(buf + len, buf_len - len, "last_rssi = %d\n",
			   htt_stats_buf->rssi);
	len += scnprintf(buf + len, buf_len - len, "enqueued_count = %llu\n",
			   htt_stats_buf->peer_enqueued_count_low |
			   ((u64)htt_stats_buf->peer_enqueued_count_high << 32));
	len += scnprintf(buf + len, buf_len - len, "dequeued_count = %llu\n",
			   htt_stats_buf->peer_dequeued_count_low |
			   ((u64)htt_stats_buf->peer_dequeued_count_high << 32));
	len += scnprintf(buf + len, buf_len - len, "dropped_count = %llu\n",
			   htt_stats_buf->peer_dropped_count_low |
			   ((u64)htt_stats_buf->peer_dropped_count_high << 32));
	len += scnprintf(buf + len, buf_len - len, "transmitted_ppdu_bytes = %llu\n",
			   htt_stats_buf->ppdu_transmitted_bytes_low |
			   ((u64)htt_stats_buf->ppdu_transmitted_bytes_high << 32));
	len += scnprintf(buf + len, buf_len - len, "ttl_removed_count = %u\n",
			   htt_stats_buf->peer_ttl_removed_count);
	len += scnprintf(buf + len, buf_len - len, "inactive_time = %u\n",
			   htt_stats_buf->inactive_time);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_peer_details_tlv(const void *tag_buf, u16 tag_len,
						     struct debug_htt_stats_req *stats_req)
{
	const struct htt_peer_details_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 peer_details = le32_to_cpu(htt_stats_buf->peer_details);
	u32 peer_ps_details = le32_to_cpu(htt_stats_buf->peer_ps_details);
	u32 peer_hist_details = le32_to_cpu(htt_stats_buf->peer_hist_details);
	u32 src_info = le32_to_cpu(htt_stats_buf->src_info);

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_PEER_DETAILS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "peer_type = %u\n",
			   le32_to_cpu(htt_stats_buf->peer_type));
	len += scnprintf(buf + len, buf_len - len, "sw_peer_id = %u\n",
			   le32_to_cpu(htt_stats_buf->sw_peer_id));
	len += scnprintf(buf + len, buf_len - len, "vdev_id = %u\n",
			   (le32_to_cpu(htt_stats_buf->vdev_pdev_ast_idx) & 0xFF));
	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n",
			 ((le32_to_cpu(htt_stats_buf->vdev_pdev_ast_idx) & 0xFF00) >> 8));
	len += scnprintf(buf + len, buf_len - len, "ast_idx = %u\n",
			   ((le32_to_cpu(
				htt_stats_buf->vdev_pdev_ast_idx) & 0xFFFF0000) >> 16));
	len += scnprintf(buf + len, buf_len - len,
			   "mac_addr = %02x:%02x:%02x:%02x:%02x:%02x\n",
			   htt_stats_buf->mac_addr.mac_addr_l32 & 0xFF,
			   (htt_stats_buf->mac_addr.mac_addr_l32 & 0xFF00) >> 8,
			   (htt_stats_buf->mac_addr.mac_addr_l32 & 0xFF0000) >> 16,
			   (htt_stats_buf->mac_addr.mac_addr_l32 & 0xFF000000) >> 24,
			   (htt_stats_buf->mac_addr.mac_addr_h16 & 0xFF),
			   (htt_stats_buf->mac_addr.mac_addr_h16 & 0xFF00) >> 8);
	len += scnprintf(buf + len, buf_len - len, "peer_flags = 0x%x\n",
			 htt_stats_buf->peer_flags);
	len += scnprintf(buf + len, buf_len - len, "qpeer_flags = 0x%x\n",
			 htt_stats_buf->qpeer_flags);

	len += scnprintf(buf + len, buf_len - len, "link_idx = %u\n",
			 u32_get_bits(peer_details, ATH12K_HTT_PEER_DETAILS_LINK_IDX));

	if (u32_get_bits(peer_details, ATH12K_HTT_PEER_DETAILS_ML_PEER_ID_VALID))
		len += scnprintf(buf + len, buf_len - len, "ml_peer_id = %u\n",
				 u32_get_bits(peer_details,
						 ATH12K_HTT_PEER_DETAILS_ML_PEER_ID));
	else
		len += scnprintf(buf + len, buf_len - len, "ml_peer_id = INVALID\n");

	len += scnprintf(buf + len, buf_len - len, "use_ppe = 0x%x\n",
			 u32_get_bits(peer_details, ATH12K_HTT_PEER_DETAILS_USE_PPE));

	len += scnprintf(buf + len, buf_len - len, "src_info = 0x%x\n",
			 u32_get_bits(src_info, ATH12K_HTT_PEER_DETAILS_SRC_INFO));

	len += scnprintf(buf + len, buf_len - len, "peer_powersave_entry_value = %u\n",
			 u32_get_bits(peer_ps_details,
				      ATH12K_HTT_PEER_PS_DETAILS_PEER_PS_ENTRY));

	len += scnprintf(buf + len, buf_len - len, "peer_powersave_pspoll_trigger = %u\n",
			 u32_get_bits(peer_ps_details,
				      ATH12K_HTT_PEER_PS_DETAILS_PEER_PS_EXIT));

	len += scnprintf(buf + len, buf_len - len, "peer_powersave_uapsd_trigger = %u\n",
			 u32_get_bits(peer_ps_details,
				      ATH12K_HTT_PEER_PS_DETAILS_PEER_PSPOLL));

	len += scnprintf(buf + len, buf_len - len, "peer_powersave_uapsd_trigger = %u\n",
			 u32_get_bits(peer_ps_details,
				      ATH12K_HTT_PEER_PS_DETAILS_PEER_UAPSD));

	len += scnprintf(buf + len, buf_len - len,
			 "peer_powersave_histogram[0](<200ms) = %u\n",
			 u32_get_bits(peer_hist_details,
				      ATH12K_HTT_PEER_HIST_DETAILS_PEER_HIST0));

	len += scnprintf(buf + len, buf_len - len,
			 "peer_powersave_histogram[1] (200-500ms)= %u\n",
			 u32_get_bits(peer_hist_details,
				      ATH12K_HTT_PEER_HIST_DETAILS_PEER_HIST1));

	len += scnprintf(buf + len, buf_len - len,
			 "peer_powersave_histogram[2] (>500ms) = %u\n",
			 u32_get_bits(peer_hist_details,
				      ATH12K_HTT_PEER_HIST_DETAILS_PEER_HIST2));

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_tx_peer_rate_stats_tlv(const void *tag_buf,
							   struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_peer_rate_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	char *tx_gi[HTT_TX_PEER_STATS_NUM_GI_COUNTERS] = {NULL};
	u8 i, j;
	u16 index = 0;

	for (j = 0; j < HTT_TX_PEER_STATS_NUM_GI_COUNTERS; j++) {
		tx_gi[j] = kmalloc(HTT_MAX_STRING_LEN, GFP_ATOMIC);
		if (!tx_gi[j])
			goto fail;
	}

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PEER_RATE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "tx_ldpc = %u\n",
			 htt_stats_buf->tx_ldpc);
	len += scnprintf(buf + len, buf_len - len, "rts_cnt = %u\n",
			 htt_stats_buf->rts_cnt);
	len += scnprintf(buf + len, buf_len - len, "ack_rssi = %u\n",
			 htt_stats_buf->ack_rssi);

	len += print_array_to_buf(buf, len, "tx_mcs", htt_stats_buf->tx_mcs,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "12:%u, 13:%u, 14:%u, 15:%u\n",
			 htt_stats_buf->tx_mcs_ext[0], htt_stats_buf->tx_mcs_ext[1],
			 htt_stats_buf->tx_mcs_ext_2[0], htt_stats_buf->tx_mcs_ext_2[1]);
	len += print_array_to_buf(buf, len, "tx_su_mcs", htt_stats_buf->tx_su_mcs,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "12:%u, 13:%u\n",
			 htt_stats_buf->tx_su_mcs_ext[0], htt_stats_buf->tx_su_mcs_ext[1]);
	len += print_array_to_buf(buf, len, "tx_mu_mcs", htt_stats_buf->tx_mu_mcs,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "12:%u, 13:%u\n",
			 htt_stats_buf->tx_mu_mcs_ext[0], htt_stats_buf->tx_mu_mcs_ext[1]);
	len += scnprintf(buf + len, buf_len - len, "tx_nss = ");
	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++)
		len += scnprintf(buf + len, buf_len - len,
				 " %u:%u ", (j + 1),
				 htt_stats_buf->tx_nss[j]);

	len += print_array_to_buf(buf, len, "\ntx_bw", htt_stats_buf->tx_bw,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "4:%u\n",
			 htt_stats_buf->tx_bw_320mhz);
	for (j = 0; j < HTT_TX_PEER_STATS_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_tx_bw = " : "quarter_tx_bw = ");
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->reduced_tx_bw[j],
				   HTT_TX_PEER_STATS_NUM_BW_COUNTERS, "\n");
	}

	len += print_array_to_buf(buf, len, "\ntx_stbc", htt_stats_buf->tx_stbc,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "12:%u, 13:%u\n",
			 htt_stats_buf->tx_stbc_ext[0], htt_stats_buf->tx_stbc_ext[1]);
	len += print_array_to_buf(buf, len, "tx_pream", htt_stats_buf->tx_pream,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_PREAMBLE_TYPES, "\n");

	for (j = 0; j < HTT_TX_PEER_STATS_NUM_GI_COUNTERS; j++) {
		index = 0;
		for (i = 0; i < HTT_TX_PEER_STATS_NUM_MCS_COUNTERS; i++)
			index += snprintf(&tx_gi[j][index], HTT_MAX_STRING_LEN - index,
					  " %u:%u,", i, htt_stats_buf->tx_gi[j][i]);

		for (i = 0; i < HTT_TX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS; i++)
			index += snprintf(&tx_gi[j][index], HTT_MAX_STRING_LEN - index,
					  " %u:%u,", i + HTT_TX_PEER_STATS_NUM_MCS_COUNTERS,
					  htt_stats_buf->tx_gi_ext[j][i]);

		len += scnprintf(buf + len, buf_len - len, "tx_gi[%u] = %s ", j, tx_gi[j]);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	len += print_array_to_buf(buf, len, "tx_dcm", htt_stats_buf->tx_dcm,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_DCM_COUNTERS, "\n\n");

	stats_req->buf_len = len;

fail:
	for (j = 0; j < HTT_TX_PEER_STATS_NUM_GI_COUNTERS; j++)
		kfree(tx_gi[j]);
}

static inline void ath12k_htt_print_rx_peer_rate_stats_tlv(const void *tag_buf,
							   struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_peer_rate_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i, j;
	u16 index = 0;
	char *rssi_chain[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS] = {NULL};
	char *rx_gi[HTT_RX_PEER_STATS_NUM_GI_COUNTERS] = {NULL};

	for (j = 0; j < HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS; j++) {
		rssi_chain[j] = kmalloc(HTT_MAX_STRING_LEN, GFP_ATOMIC);
		if (!rssi_chain[j])
			goto fail;
	}

	for (j = 0; j < HTT_RX_PEER_STATS_NUM_GI_COUNTERS; j++) {
		rx_gi[j] = kmalloc(HTT_MAX_STRING_LEN, GFP_ATOMIC);
		if (!rx_gi[j])
			goto fail;
	}

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_PEER_RATE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "nsts = %u\n",
			 htt_stats_buf->nsts);
	len += scnprintf(buf + len, buf_len - len, "rx_ldpc = %u\n",
			 htt_stats_buf->rx_ldpc);
	len += scnprintf(buf + len, buf_len - len, "rts_cnt = %u\n",
			 htt_stats_buf->rts_cnt);
	len += scnprintf(buf + len, buf_len - len, "rssi_mgmt = %u\n",
			 htt_stats_buf->rssi_mgmt);
	len += scnprintf(buf + len, buf_len - len, "rssi_data = %u\n",
			 htt_stats_buf->rssi_data);
	len += scnprintf(buf + len, buf_len - len, "rssi_comb = %u\n",
			 htt_stats_buf->rssi_comb);

	len += print_array_to_buf(buf, len, "rx_mcs", htt_stats_buf->rx_mcs,
			   ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "12:%u, 13:%u, 14:%u, 15:%u\n",
			 htt_stats_buf->rx_mcs_ext[0], htt_stats_buf->rx_mcs_ext[1],
			 htt_stats_buf->rx_mcs_ext_2[0], htt_stats_buf->rx_mcs_ext_2[1]);
	len += scnprintf(buf + len, buf_len - len, "rx_nss = ");
	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++)
		len += scnprintf(buf + len, buf_len - len,
				 " %u:%u ", (j + 1),
				 htt_stats_buf->rx_nss[j]);

	len += print_array_to_buf(buf, len, "\nrx_dcm", htt_stats_buf->rx_dcm,
			   ATH12K_HTT_RX_PDEV_STATS_NUM_DCM_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_stbc", htt_stats_buf->rx_stbc,
			   ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "12:%u, 13:%u\n",
			 htt_stats_buf->rx_stbc_ext[0], htt_stats_buf->rx_stbc_ext[1]);
	len += print_array_to_buf(buf, len, "rx_bw", htt_stats_buf->rx_bw,
			   ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS, " ");
	len += scnprintf(buf + len, buf_len - len, "4:%u\n",
			 htt_stats_buf->rx_bw_320mhz);
	for (j = 0; j < HTT_RX_PEER_STATS_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_rx_bw = " : "quarter_rx_bw = ");
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->reduced_rx_bw[j],
				   HTT_RX_PEER_STATS_NUM_BW_COUNTERS, "\n");
	}

	for (j = 0; j < HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len, "rssi_chain[%u] = ", j);
		CHAIN_ARRAY_TO_BUF(buf, len, htt_stats_buf->rssi_chain[j],
				   HTT_RX_PEER_STATS_NUM_BW_COUNTERS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len, "rssi_chain_ext[%u] = ", j);
		CHAIN_ARRAY_TO_BUF(buf, len, htt_stats_buf->rssi_chain_ext[j],
				   HTT_RX_PEER_STATS_NUM_BW_EXT_COUNTERS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < HTT_RX_PEER_STATS_NUM_GI_COUNTERS; j++) {
		memset(rx_gi[j], 0x0, HTT_MAX_STRING_LEN);
		index = 0;
		for (i = 0; i < HTT_RX_PEER_STATS_NUM_MCS_COUNTERS; i++)
			index += snprintf(&rx_gi[j][index], HTT_MAX_STRING_LEN - index,
					  " %u:%u,", i, htt_stats_buf->rx_gi[j][i]);

		for (i = 0; i < HTT_RX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS; i++)
			index += snprintf(&rx_gi[j][index], HTT_MAX_STRING_LEN - index,
					  " %u:%u,", i + HTT_RX_PEER_STATS_NUM_MCS_COUNTERS,
					  htt_stats_buf->rx_gi_ext[j][i]);

		len += scnprintf(buf + len, buf_len - len, "rx_gi[%u] = %s ", j, rx_gi[j]);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}
	len += print_array_to_buf(buf, len, "rx_pream", htt_stats_buf->rx_pream,
			   ATH12K_HTT_RX_PDEV_STATS_NUM_PREAMBLE_TYPES, "\n");

	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_non_data_ppdu = %u\n",
			 htt_stats_buf->rx_ulofdma_non_data_ppdu);
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_data_ppdu = %u\n",
			 htt_stats_buf->rx_ulofdma_data_ppdu);
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_mpdu_ok = %u\n",
			 htt_stats_buf->rx_ulofdma_mpdu_ok);
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_mpdu_fail = %u\n",
			 htt_stats_buf->rx_ulofdma_mpdu_fail);
	len += scnprintf(buf + len, buf_len - len, "rx_ulmumimo_non_data_ppdu = %u\n",
			 htt_stats_buf->rx_ulmumimo_non_data_ppdu);
	len += scnprintf(buf + len, buf_len - len, "rx_ulmumimo_data_ppdu = %u\n",
			 htt_stats_buf->rx_ulmumimo_data_ppdu);
	len += scnprintf(buf + len, buf_len - len, "rx_ulmumimo_mpdu_ok = %u\n",
			 htt_stats_buf->rx_ulmumimo_mpdu_ok);
	len += scnprintf(buf + len, buf_len - len, "rx_ulmumimo_mpdu_fail = %u\n",
			 htt_stats_buf->rx_ulmumimo_mpdu_fail);

	len += scnprintf(buf + len, buf_len - len, "rx_ul_fd_rssi = ");

	len += scnprintf(buf + len, buf_len - len, "per_chain_rssi_pkt_type = %#x\n",
			 htt_stats_buf->per_chain_rssi_pkt_type);
	for (j = 0; j < HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_per_chain_rssi_in_dbm[%u] = ", j);
		for (i = 0; i < ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS; i++)
			len += scnprintf(buf + len,
					 buf_len - len,
					 " %u:%d,",
					 i,
					 htt_stats_buf->rx_per_chain_rssi_in_dbm[j][i]);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_per_chain_rssi_in_dbm_ext[%u] = ", j);
		for (i = 0; i < HTT_RX_PEER_STATS_NUM_BW_EXT_COUNTERS; i++)
			len += scnprintf(buf + len,
					 buf_len - len,
					 " %u:%d,",
					 i,
					 htt_stats_buf->rx_per_chain_rssi_in_dbm_ext[j][i]);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	stats_req->buf_len = len;

fail:
	for (j = 0; j < HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS; j++)
		kfree(rssi_chain[j]);

	for (j = 0; j < HTT_RX_PEER_STATS_NUM_GI_COUNTERS; j++)
		kfree(rx_gi[j]);
}

static inline void
ath12k_htt_print_tx_hwq_mu_mimo_sch_stats_tlv(const void *tag_buf,
					      struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_mu_mimo_sch_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_HWQ_MU_MIMO_SCH_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_sch_posted = %u\n",
			   htt_stats_buf->mu_mimo_sch_posted);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_sch_failed = %u\n",
			   htt_stats_buf->mu_mimo_sch_failed);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_ppdu_posted = %u\n",
			   htt_stats_buf->mu_mimo_ppdu_posted);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_mu_mimo_mpdu_stats_tlv(const void *tag_buf,
					       struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_mu_mimo_mpdu_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_HWQ_MU_MIMO_MPDU_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_mpdus_queued_usr = %u\n",
			   htt_stats_buf->mu_mimo_mpdus_queued_usr);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_mpdus_tried_usr = %u\n",
			   htt_stats_buf->mu_mimo_mpdus_tried_usr);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_mpdus_failed_usr = %u\n",
			   htt_stats_buf->mu_mimo_mpdus_failed_usr);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_mpdus_requeued_usr = %u\n",
			   htt_stats_buf->mu_mimo_mpdus_requeued_usr);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_err_no_ba_usr = %u\n",
			   htt_stats_buf->mu_mimo_err_no_ba_usr);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_mpdu_underrun_usr = %u\n",
			   htt_stats_buf->mu_mimo_mpdu_underrun_usr);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_ampdu_underrun_usr = %u\n",
			   htt_stats_buf->mu_mimo_ampdu_underrun_usr);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_mu_mimo_cmn_stats_tlv(const void *tag_buf,
					      struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_mu_mimo_cmn_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_HWQ_MU_MIMO_CMN_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			   htt_stats_buf->mac_id__hwq_id__word & 0xFF);
	len += scnprintf(buf + len, buf_len - len, "hwq_id = %u\n",
			   (htt_stats_buf->mac_id__hwq_id__word & 0xFF00) >> 8);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_stats_cmn_tlv(const void *tag_buf, struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_stats_cmn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	/* TODO: HKDBG */
	len += scnprintf(buf + len, buf_len - len, "HTT_TX_HWQ_STATS_CMN_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			   htt_stats_buf->mac_id__hwq_id__word & 0xFF);
	len += scnprintf(buf + len, buf_len - len, "hwq_id = %u\n",
			   (htt_stats_buf->mac_id__hwq_id__word & 0xFF00) >> 8);
	len += scnprintf(buf + len, buf_len - len, "xretry = %u\n",
			   htt_stats_buf->xretry);
	len += scnprintf(buf + len, buf_len - len, "underrun_cnt = %u\n",
			   htt_stats_buf->underrun_cnt);
	len += scnprintf(buf + len, buf_len - len, "flush_cnt = %u\n",
			   htt_stats_buf->flush_cnt);
	len += scnprintf(buf + len, buf_len - len, "filt_cnt = %u\n",
			   htt_stats_buf->filt_cnt);
	len += scnprintf(buf + len, buf_len - len, "null_mpdu_bmap = %u\n",
			   htt_stats_buf->null_mpdu_bmap);
	len += scnprintf(buf + len, buf_len - len, "user_ack_failure = %u\n",
			   htt_stats_buf->user_ack_failure);
	len += scnprintf(buf + len, buf_len - len, "ack_tlv_proc = %u\n",
			   htt_stats_buf->ack_tlv_proc);
	len += scnprintf(buf + len, buf_len - len, "sched_id_proc = %u\n",
			   htt_stats_buf->sched_id_proc);
	len += scnprintf(buf + len, buf_len - len, "null_mpdu_tx_count = %u\n",
			   htt_stats_buf->null_mpdu_tx_count);
	len += scnprintf(buf + len, buf_len - len, "mpdu_bmap_not_recvd = %u\n",
			   htt_stats_buf->mpdu_bmap_not_recvd);
	len += scnprintf(buf + len, buf_len - len, "num_bar = %u\n",
			   htt_stats_buf->num_bar);
	len += scnprintf(buf + len, buf_len - len, "rts = %u\n",
			   htt_stats_buf->rts);
	len += scnprintf(buf + len, buf_len - len, "cts2self = %u\n",
			   htt_stats_buf->cts2self);
	len += scnprintf(buf + len, buf_len - len, "qos_null = %u\n",
			   htt_stats_buf->qos_null);
	len += scnprintf(buf + len, buf_len - len, "mpdu_tried_cnt = %u\n",
			   htt_stats_buf->mpdu_tried_cnt);
	len += scnprintf(buf + len, buf_len - len, "mpdu_queued_cnt = %u\n",
			   htt_stats_buf->mpdu_queued_cnt);
	len += scnprintf(buf + len, buf_len - len, "mpdu_ack_fail_cnt = %u\n",
			   htt_stats_buf->mpdu_ack_fail_cnt);
	len += scnprintf(buf + len, buf_len - len, "mpdu_filt_cnt = %u\n",
			   htt_stats_buf->mpdu_filt_cnt);
	len += scnprintf(buf + len, buf_len - len, "false_mpdu_ack_count = %u\n",
			   htt_stats_buf->false_mpdu_ack_count);
	len += scnprintf(buf + len, buf_len - len, "txq_timeout = %u\n",
			   htt_stats_buf->txq_timeout);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_difs_latency_stats_tlv_v(const void *tag_buf,
						 u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_difs_latency_stats_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 data_len = min_t(u16, (tag_len >> 2), HTT_TX_HWQ_MAX_DIFS_LATENCY_BINS);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_HWQ_DIFS_LATENCY_STATS_TLV_V:\n");
	len += scnprintf(buf + len, buf_len - len, "hist_intvl = %u\n",
			htt_stats_buf->hist_intvl);

	len += print_array_to_buf(buf, len, "difs_latency_hist", htt_stats_buf->difs_latency_hist,
			   data_len, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_cmd_result_stats_tlv_v(const void *tag_buf,
					       u16 tag_len,
					       struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_cmd_result_stats_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 data_len;

	data_len = min_t(u16, (tag_len >> 2), HTT_TX_HWQ_MAX_CMD_RESULT_STATS);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_HWQ_CMD_RESULT_STATS_TLV_V:\n");

	len += print_array_to_buf(buf, len, "cmd_result", htt_stats_buf->cmd_result,
			   data_len, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_cmd_stall_stats_tlv_v(const void *tag_buf,
					      u16 tag_len,
					      struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_cmd_stall_stats_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems;

	num_elems = min_t(u16, (tag_len >> 2), HTT_TX_HWQ_MAX_CMD_STALL_STATS);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_HWQ_CMD_STALL_STATS_TLV_V:\n");

	len += print_array_to_buf(buf, len, "cmd_stall_status", htt_stats_buf->cmd_stall_status,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_fes_result_stats_tlv_v(const void *tag_buf,
					       u16 tag_len,
					       struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_fes_result_stats_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems;

	num_elems = min_t(u16, (tag_len >> 2), HTT_TX_HWQ_MAX_FES_RESULT_STATS);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_HWQ_FES_RESULT_STATS_TLV_V:\n");

	len += print_array_to_buf(buf, len, "fes_result", htt_stats_buf->fes_result,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_tried_mpdu_cnt_hist_tlv_v(const void *tag_buf,
						  u16 tag_len,
						  struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_tried_mpdu_cnt_hist_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32  num_elements = ((tag_len -
			    sizeof(htt_stats_buf->hist_bin_size)) >> 2);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_HWQ_TRIED_MPDU_CNT_HIST_TLV_V:\n");
	len += scnprintf(buf + len, buf_len - len, "TRIED_MPDU_CNT_HIST_BIN_SIZE : %u\n",
			   htt_stats_buf->hist_bin_size);

	len += print_array_to_buf(buf, len, "tried_mpdu_cnt_hist",
			   htt_stats_buf->tried_mpdu_cnt_hist,
			   num_elements, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_hwq_txop_used_cnt_hist_tlv_v(const void *tag_buf,
						 u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_hwq_txop_used_cnt_hist_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 num_elements = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_HWQ_TXOP_USED_CNT_HIST_TLV_V:\n");

	len += print_array_to_buf(buf, len, "txop_used_cnt_hist",
			   htt_stats_buf->txop_used_cnt_hist,
			   num_elements, "\n\n");
	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_tx_tqm_cmdq_status_tlv(const void *tag_buf,
							   struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_tqm_cmdq_status_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TQM_CMDQ_STATUS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			   htt_stats_buf->mac_id__cmdq_id__word & 0xFF);
	len += scnprintf(buf + len, buf_len - len, "cmdq_id = %u\n",
			   (htt_stats_buf->mac_id__cmdq_id__word & 0xFF00) >> 8);
	len += scnprintf(buf + len, buf_len - len, "sync_cmd = %u\n",
			   htt_stats_buf->sync_cmd);
	len += scnprintf(buf + len, buf_len - len, "write_cmd = %u\n",
			   htt_stats_buf->write_cmd);
	len += scnprintf(buf + len, buf_len - len, "gen_mpdu_cmd = %u\n",
			   htt_stats_buf->gen_mpdu_cmd);
	len += scnprintf(buf + len, buf_len - len, "mpdu_queue_stats_cmd = %u\n",
			   htt_stats_buf->mpdu_queue_stats_cmd);
	len += scnprintf(buf + len, buf_len - len, "mpdu_head_info_cmd = %u\n",
			   htt_stats_buf->mpdu_head_info_cmd);
	len += scnprintf(buf + len, buf_len - len, "msdu_flow_stats_cmd = %u\n",
			   htt_stats_buf->msdu_flow_stats_cmd);
	len += scnprintf(buf + len, buf_len - len, "remove_mpdu_cmd = %u\n",
			   htt_stats_buf->remove_mpdu_cmd);
	len += scnprintf(buf + len, buf_len - len, "remove_msdu_cmd = %u\n",
			   htt_stats_buf->remove_msdu_cmd);
	len += scnprintf(buf + len, buf_len - len, "flush_cache_cmd = %u\n",
			   htt_stats_buf->flush_cache_cmd);
	len += scnprintf(buf + len, buf_len - len, "update_mpduq_cmd = %u\n",
			   htt_stats_buf->update_mpduq_cmd);
	len += scnprintf(buf + len, buf_len - len, "update_msduq_cmd = %u\n",
			   htt_stats_buf->update_msduq_cmd);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_de_fw2wbm_ring_full_hist_tlv(const void *tag_buf,
						 u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_de_fw2wbm_ring_full_hist_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16  num_elements = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_TX_DE_FW2WBM_RING_FULL_HIST_TLV\n");

	len += print_array_to_buf(buf, len, "fw2wbm_ring_full_hist",
			   htt_stats_buf->fw2wbm_ring_full_hist,
			   num_elements, "\n\n");

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_ring_if_stats_tlv(const void *tag_buf,
						      struct debug_htt_stats_req *stats_req)
{
	const struct htt_ring_if_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_RING_IF_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "base_addr = %u\n",
			   htt_stats_buf->base_addr);
	len += scnprintf(buf + len, buf_len - len, "elem_size = %u\n",
			   htt_stats_buf->elem_size);
	len += scnprintf(buf + len, buf_len - len, "num_elems = %u\n",
			   htt_stats_buf->num_elems__prefetch_tail_idx & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "prefetch_tail_idx = %u\n",
			   (htt_stats_buf->num_elems__prefetch_tail_idx &
			   0xFFFF0000) >> 16);
	len += scnprintf(buf + len, buf_len - len, "head_idx = %u\n",
			   htt_stats_buf->head_idx__tail_idx & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "tail_idx = %u\n",
			   (htt_stats_buf->head_idx__tail_idx & 0xFFFF0000) >> 16);
	len += scnprintf(buf + len, buf_len - len, "shadow_head_idx = %u\n",
			   htt_stats_buf->shadow_head_idx__shadow_tail_idx & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "shadow_tail_idx = %u\n",
			   (htt_stats_buf->shadow_head_idx__shadow_tail_idx &
			   0xFFFF0000) >> 16);
	len += scnprintf(buf + len, buf_len - len, "num_tail_incr = %u\n",
			   htt_stats_buf->num_tail_incr);
	len += scnprintf(buf + len, buf_len - len, "lwm_thresh = %u\n",
			   htt_stats_buf->lwm_thresh__hwm_thresh & 0xFFFF);
	len += scnprintf(buf + len, buf_len - len, "hwm_thresh = %u\n",
			   (htt_stats_buf->lwm_thresh__hwm_thresh & 0xFFFF0000) >> 16);
	len += scnprintf(buf + len, buf_len - len, "overrun_hit_count = %u\n",
			   htt_stats_buf->overrun_hit_count);
	len += scnprintf(buf + len, buf_len - len, "underrun_hit_count = %u\n",
			   htt_stats_buf->underrun_hit_count);
	len += scnprintf(buf + len, buf_len - len, "prod_blockwait_count = %u\n",
			   htt_stats_buf->prod_blockwait_count);
	len += scnprintf(buf + len, buf_len - len, "cons_blockwait_count = %u\n",
			   htt_stats_buf->cons_blockwait_count);

	len += print_array_to_buf(buf, len, "low_wm_hit_count", htt_stats_buf->low_wm_hit_count,
			   HTT_STATS_LOW_WM_BINS, "\n");
	len += print_array_to_buf(buf, len, "high_wm_hit_count", htt_stats_buf->high_wm_hit_count,
			   HTT_STATS_HIGH_WM_BINS, "\n\n");

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_ring_if_cmn_tlv(const void *tag_buf,
						    struct debug_htt_stats_req *stats_req)
{
	const struct htt_ring_if_cmn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_RING_IF_CMN_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			   htt_stats_buf->mac_id__word & 0xFF);
	len += scnprintf(buf + len, buf_len - len, "num_records = %u\n",
			   htt_stats_buf->num_records);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_sring_cmn_tlv(const void *tag_buf,
						  struct debug_htt_stats_req *stats_req)
{
	const struct htt_sring_cmn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_SRING_CMN_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "num_records = %u\n",
			   htt_stats_buf->num_records);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_rx_soc_fw_stats_tlv(const void *tag_buf,
							struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_soc_fw_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_SOC_FW_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "fw_reo_ring_data_msdu = %u\n",
			   htt_stats_buf->fw_reo_ring_data_msdu);
	len += scnprintf(buf + len, buf_len - len, "fw_to_host_data_msdu_bcmc = %u\n",
			   htt_stats_buf->fw_to_host_data_msdu_bcmc);
	len += scnprintf(buf + len, buf_len - len, "fw_to_host_data_msdu_uc = %u\n",
			   htt_stats_buf->fw_to_host_data_msdu_uc);
	len += scnprintf(buf + len, buf_len - len,
			   "ofld_remote_data_buf_recycle_cnt = %u\n",
			   htt_stats_buf->ofld_remote_data_buf_recycle_cnt);
	len += scnprintf(buf + len, buf_len - len,
			   "ofld_remote_free_buf_indication_cnt = %u\n",
			   htt_stats_buf->ofld_remote_free_buf_indication_cnt);
	len += scnprintf(buf + len, buf_len - len,
			   "ofld_buf_to_host_data_msdu_uc = %u\n",
			   htt_stats_buf->ofld_buf_to_host_data_msdu_uc);
	len += scnprintf(buf + len, buf_len - len,
			   "reo_fw_ring_to_host_data_msdu_uc = %u\n",
			   htt_stats_buf->reo_fw_ring_to_host_data_msdu_uc);
	len += scnprintf(buf + len, buf_len - len, "wbm_sw_ring_reap = %u\n",
			   htt_stats_buf->wbm_sw_ring_reap);
	len += scnprintf(buf + len, buf_len - len, "wbm_forward_to_host_cnt = %u\n",
			   htt_stats_buf->wbm_forward_to_host_cnt);
	len += scnprintf(buf + len, buf_len - len, "wbm_target_recycle_cnt = %u\n",
			   htt_stats_buf->wbm_target_recycle_cnt);
	len += scnprintf(buf + len, buf_len - len,
			   "target_refill_ring_recycle_cnt = %u\n",
			   htt_stats_buf->target_refill_ring_recycle_cnt);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_soc_fw_refill_ring_empty_tlv_v(const void *tag_buf,
						   u16 tag_len,
						   struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_soc_fw_refill_ring_empty_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2), HTT_RX_STATS_REFILL_MAX_RING);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_RX_SOC_FW_REFILL_RING_EMPTY_TLV_V:\n");

	len += print_array_to_buf(buf, len, "refill_ring_empty_cnt", htt_stats_buf->refill_ring_empty_cnt,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_dl_mu_ofdma_sch_stats_tlv(const void *tag_buf,
						   struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_pdev_dl_mu_ofdma_sch_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len,
			 "11ax DL MU_OFDMA SCH STATS:\n");
	len += print_array_to_buf(buf, len, "ax_mu_ofdma_sch_nusers",
			   htt_stats_buf->ax_mu_ofdma_sch_nusers,
		   	   ATH12K_HTT_TX_NUM_OFDMA_USER_STATS, "\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_ul_mu_ofdma_sch_stats_tlv(const void *tag_buf,
						   struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_pdev_ul_mu_ofdma_sch_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "11ax UL MU_OFDMA SCH STATS:\n");
	len += print_array_to_buf(buf, len, "ax_ul_mu_ofdma_basic_sch_nusers",
			   htt_stats_buf->ax_ul_mu_ofdma_basic_sch_nusers,
			   ATH12K_HTT_TX_NUM_OFDMA_USER_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_ul_mu_ofdma_bsr_sch_nusers",
			   htt_stats_buf->ax_ul_mu_ofdma_bsr_sch_nusers,
			   ATH12K_HTT_TX_NUM_OFDMA_USER_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_ul_mu_ofdma_bar_sch_nusers",
			   htt_stats_buf->ax_ul_mu_ofdma_bar_sch_nusers,
			   ATH12K_HTT_TX_NUM_OFDMA_USER_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_ul_mu_ofdma_brp_sch_nusers",
			   htt_stats_buf->ax_ul_mu_ofdma_brp_sch_nusers,
			   ATH12K_HTT_TX_NUM_OFDMA_USER_STATS, "\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_ul_mu_mimo_sch_stats_tlv(const void *tag_buf,
						  struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_pdev_ul_mu_mimo_sch_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "11ax UL MU_MIMO SCH STATS:\n");
	len += print_array_to_buf(buf, len, "ax_ul_mu_mimo_basic_sch_nusers",
			   htt_stats_buf->ax_ul_mu_mimo_basic_sch_nusers,
			   ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_ul_mu_mimo_brp_sch_nusers",
			   htt_stats_buf->ax_ul_mu_mimo_brp_sch_nusers,
			   ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS, "\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_dl_mu_mimo_sch_stats_tlv(const void *tag_buf,
						  struct debug_htt_stats_req *stats_req)
{
	const struct htt_tx_pdev_dl_mu_mimo_sch_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_MU_MIMO_SCH_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_sch_posted = %u\n",
			 htt_stats_buf->mu_mimo_sch_posted);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_sch_failed = %u\n",
			 htt_stats_buf->mu_mimo_sch_failed);
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_ppdu_posted = %u\n",
			 htt_stats_buf->mu_mimo_ppdu_posted);

	len += print_array_to_buf(buf, len, "ac_mu_mimo_sch_posted_per_group_index",
			   htt_stats_buf->ac_mu_mimo_sch_posted_per_grp_sz,
			   ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS, "\n");

	len += print_array_to_buf(buf, len, "ax_mu_mimo_sch_posted_per_group_index",
			   htt_stats_buf->ax_mu_mimo_sch_posted_per_grp_sz,
			   ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS, "\n");

	len += scnprintf(buf + len, buf_len - len, "11ac DL MU_MIMO SCH STATS:\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_sch_nusers",
			   htt_stats_buf->ac_mu_mimo_sch_nusers,
			   ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS, "\n");

	len += scnprintf(buf + len, buf_len - len, "\n11ax DL MU_MIMO SCH STATS:\n");
	len += print_array_to_buf(buf, len, "ax_mu_mimo_sch_nusers",
			   htt_stats_buf->ax_mu_mimo_sch_nusers,
			   ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS, "\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_soc_fw_refill_ring_num_rxdma_err_tlv_v(const void *tag_buf,
							   u16 tag_len,
							   struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_soc_fw_refill_ring_num_rxdma_err_tlv_v *htt_stats_buf =
		tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2), HTT_RX_RXDMA_MAX_ERR_CODE);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_RX_SOC_FW_REFILL_RING_NUM_RXDMA_ERR_TLV_V:\n");

	len += print_array_to_buf(buf, len, "rxdma_err", htt_stats_buf->rxdma_err,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_soc_fw_refill_ring_num_reo_err_tlv_v(const void *tag_buf,
							 u16 tag_len,
							 struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_soc_fw_refill_ring_num_reo_err_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2), HTT_RX_REO_MAX_ERR_CODE);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_RX_SOC_FW_REFILL_RING_NUM_REO_ERR_TLV_V:\n");

	len += print_array_to_buf(buf, len, "reo_err", htt_stats_buf->reo_err,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_reo_debug_stats_tlv_v(const void *tag_buf,
					  struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_reo_resource_stats_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_REO_RESOURCE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "sample_id = %u\n",
			   htt_stats_buf->sample_id);
	len += scnprintf(buf + len, buf_len - len, "total_max = %u\n",
			   htt_stats_buf->total_max);
	len += scnprintf(buf + len, buf_len - len, "total_avg = %u\n",
			   htt_stats_buf->total_avg);
	len += scnprintf(buf + len, buf_len - len, "total_sample = %u\n",
			   htt_stats_buf->total_sample);
	len += scnprintf(buf + len, buf_len - len, "non_zeros_avg = %u\n",
			   htt_stats_buf->non_zeros_avg);
	len += scnprintf(buf + len, buf_len - len, "non_zeros_sample = %u\n",
			   htt_stats_buf->non_zeros_sample);
	len += scnprintf(buf + len, buf_len - len, "last_non_zeros_max = %u\n",
			   htt_stats_buf->last_non_zeros_max);
	len += scnprintf(buf + len, buf_len - len, "last_non_zeros_min %u\n",
			   htt_stats_buf->last_non_zeros_min);
	len += scnprintf(buf + len, buf_len - len, "last_non_zeros_avg %u\n",
			   htt_stats_buf->last_non_zeros_avg);
	len += scnprintf(buf + len, buf_len - len, "last_non_zeros_sample %u\n",
			   htt_stats_buf->last_non_zeros_sample);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_soc_fw_refill_ring_num_refill_tlv_v(const void *tag_buf,
							u16 tag_len,
							struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_soc_fw_refill_ring_num_refill_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2), HTT_RX_STATS_REFILL_MAX_RING);

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_RX_SOC_FW_REFILL_RING_NUM_REFILL_TLV_V:\n");

	len += print_array_to_buf(buf, len, "refill_ring_num_refill",
			   htt_stats_buf->refill_ring_num_refill,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_rx_pdev_fw_stats_tlv(const void *tag_buf,
							 struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_pdev_fw_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_PDEV_FW_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			   le32_to_cpu(htt_stats_buf->mac_id__word) & 0xFF);
	len += scnprintf(buf + len, buf_len - len, "ppdu_recvd = %u\n",
			   le32_to_cpu(htt_stats_buf->ppdu_recvd));
	len += scnprintf(buf + len, buf_len - len, "mpdu_cnt_fcs_ok = %u\n",
			   le32_to_cpu(htt_stats_buf->mpdu_cnt_fcs_ok));
	len += scnprintf(buf + len, buf_len - len, "mpdu_cnt_fcs_err = %u\n",
			   le32_to_cpu(htt_stats_buf->mpdu_cnt_fcs_err));
	len += scnprintf(buf + len, buf_len - len, "tcp_msdu_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->tcp_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "tcp_ack_msdu_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->tcp_ack_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "udp_msdu_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->udp_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "other_msdu_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->other_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "fw_ring_mpdu_ind = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_ring_mpdu_ind));
	len += print_array_to_buf(buf, len, "fw_ring_mgmt_subtype",
				  htt_stats_buf->fw_ring_mgmt_subtype,
				  HTT_STATS_SUBTYPE_MAX, "\n");
	len += print_array_to_buf(buf, len, "fw_ring_ctrl_subtype",
				  htt_stats_buf->fw_ring_ctrl_subtype,
				  HTT_STATS_SUBTYPE_MAX, "\n");
	len += scnprintf(buf + len, buf_len - len, "fw_ring_mcast_data_msdu = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_ring_mcast_data_msdu));
	len += scnprintf(buf + len, buf_len - len, "fw_ring_bcast_data_msdu = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_ring_bcast_data_msdu));
	len += scnprintf(buf + len, buf_len - len, "fw_ring_ucast_data_msdu = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_ring_ucast_data_msdu));
	len += scnprintf(buf + len, buf_len - len, "fw_ring_null_data_msdu = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_ring_null_data_msdu));
	len += scnprintf(buf + len, buf_len - len, "fw_ring_mpdu_drop = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_ring_mpdu_drop));
	len += scnprintf(buf + len, buf_len - len, "ofld_local_data_ind_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->ofld_local_data_ind_cnt));
	len += scnprintf(buf + len, buf_len - len,
			   "ofld_local_data_buf_recycle_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->ofld_local_data_buf_recycle_cnt));
	len += scnprintf(buf + len, buf_len - len, "drx_local_data_ind_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->drx_local_data_ind_cnt));
	len += scnprintf(buf + len, buf_len - len,
			   "drx_local_data_buf_recycle_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->drx_local_data_buf_recycle_cnt));
	len += scnprintf(buf + len, buf_len - len, "local_nondata_ind_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->local_nondata_ind_cnt));
	len += scnprintf(buf + len, buf_len - len, "local_nondata_buf_recycle_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->local_nondata_buf_recycle_cnt));
	len += scnprintf(buf + len, buf_len - len, "fw_status_buf_ring_refill_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_status_buf_ring_refill_cnt));
	len += scnprintf(buf + len, buf_len - len, "fw_status_buf_ring_empty_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_status_buf_ring_empty_cnt));
	len += scnprintf(buf + len, buf_len - len, "fw_pkt_buf_ring_refill_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_pkt_buf_ring_refill_cnt));
	len += scnprintf(buf + len, buf_len - len, "fw_pkt_buf_ring_empty_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_pkt_buf_ring_empty_cnt));
	len += scnprintf(buf + len, buf_len - len, "fw_link_buf_ring_refill_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_link_buf_ring_refill_cnt));
	len += scnprintf(buf + len, buf_len - len, "fw_link_buf_ring_empty_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->fw_link_buf_ring_empty_cnt));
	len += scnprintf(buf + len, buf_len - len, "host_pkt_buf_ring_refill_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->host_pkt_buf_ring_refill_cnt));
	len += scnprintf(buf + len, buf_len - len, "host_pkt_buf_ring_empty_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->host_pkt_buf_ring_empty_cnt));
	len += scnprintf(buf + len, buf_len - len, "mon_pkt_buf_ring_refill_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_pkt_buf_ring_refill_cnt));
	len += scnprintf(buf + len, buf_len - len, "mon_pkt_buf_ring_empty_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_pkt_buf_ring_empty_cnt));
	len += scnprintf(buf + len, buf_len - len,
			   "mon_status_buf_ring_refill_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_status_buf_ring_refill_cnt));
	len += scnprintf(buf + len, buf_len - len, "mon_status_buf_ring_empty_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_status_buf_ring_empty_cnt));
	len += scnprintf(buf + len, buf_len - len, "mon_desc_buf_ring_refill_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_desc_buf_ring_refill_cnt));
	len += scnprintf(buf + len, buf_len - len, "mon_desc_buf_ring_empty_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_desc_buf_ring_empty_cnt));
	len += scnprintf(buf + len, buf_len - len, "mon_dest_ring_update_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_dest_ring_update_cnt));
	len += scnprintf(buf + len, buf_len - len, "mon_dest_ring_full_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->mon_dest_ring_full_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_suspend_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_suspend_fail_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_suspend_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_resume_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_resume_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_resume_fail_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_resume_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_ring_switch_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_ring_switch_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_ring_restore_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_ring_restore_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_flush_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_flush_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_recovery_reset_cnt = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_recovery_reset_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_lwm_prom_filter_dis = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_lwm_prom_filter_dis));
	len += scnprintf(buf + len, buf_len - len, "rx_hwm_prom_filter_en = %u\n",
			   le32_to_cpu(htt_stats_buf->rx_hwm_prom_filter_en));
	len += scnprintf(buf + len, buf_len - len, "bytes_received_low_32 = %u\n",
			   le32_to_cpu(htt_stats_buf->bytes_received_low_32));
	len += scnprintf(buf + len, buf_len - len, "bytes_received_high_32 = %u\n",
			   le32_to_cpu(htt_stats_buf->bytes_received_high_32));

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_pdev_fw_ring_mpdu_err_tlv_v(const void *tag_buf,
						struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_pdev_fw_ring_mpdu_err_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len,
			   "HTT_RX_PDEV_FW_RING_MPDU_ERR_TLV_V:\n");

	len += print_array_to_buf(buf, len, "fw_ring_mpdu_err", htt_stats_buf->fw_ring_mpdu_err,
			   HTT_RX_STATS_RXDMA_MAX_ERR, "\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_pdev_fw_mpdu_drop_tlv_v(const void *tag_buf,
					    u16 tag_len,
					    struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_pdev_fw_mpdu_drop_tlv_v *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2), HTT_RX_STATS_FW_DROP_REASON_MAX);

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_PDEV_FW_MPDU_DROP_TLV_V:\n");

	len += print_array_to_buf(buf, len, "fw_mpdu_drop", htt_stats_buf->fw_mpdu_drop,
			   num_elems, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_pdev_fw_stats_phy_err_tlv(const void *tag_buf,
					      struct debug_htt_stats_req *stats_req)
{
	const struct htt_rx_pdev_fw_stats_phy_err_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_PDEV_FW_STATS_PHY_ERR_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id__word = %u\n",
			   htt_stats_buf->mac_id__word);
	len += scnprintf(buf + len, buf_len - len, "total_phy_err_nct = %u\n",
			   htt_stats_buf->total_phy_err_cnt);

	len += print_array_to_buf(buf, len, "phy_errs", htt_stats_buf->phy_err,
			   HTT_STATS_PHY_ERR_MAX, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_pdev_stats_twt_sessions_tlv(const void *tag_buf,
					     struct debug_htt_stats_req *stats_req)
{
	const struct htt_pdev_stats_twt_sessions_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_STATS_TWT_SESSIONS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n",
			   htt_stats_buf->pdev_id);
	len += scnprintf(buf + len, buf_len - len, "num_sessions = %u\n",
			   htt_stats_buf->num_sessions);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_pdev_stats_twt_session_tlv(const void *tag_buf,
					    struct debug_htt_stats_req *stats_req)
{
	const struct htt_pdev_stats_twt_session_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_STATS_TWT_SESSION_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "vdev_id = %u\n",
			   htt_stats_buf->vdev_id);
	len += scnprintf(buf + len, buf_len - len,
			   "peer_mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
			   htt_stats_buf->peer_mac.mac_addr_l32 & 0xFF,
			   (htt_stats_buf->peer_mac.mac_addr_l32 & 0xFF00) >> 8,
			   (htt_stats_buf->peer_mac.mac_addr_l32 & 0xFF0000) >> 16,
			   (htt_stats_buf->peer_mac.mac_addr_l32 & 0xFF000000) >> 24,
			   (htt_stats_buf->peer_mac.mac_addr_h16 & 0xFF),
			   (htt_stats_buf->peer_mac.mac_addr_h16 & 0xFF00) >> 8);
	len += scnprintf(buf + len, buf_len - len, "flow_id_flags = %u\n",
			   htt_stats_buf->flow_id_flags);
	len += scnprintf(buf + len, buf_len - len, "dialog_id = %u\n",
			   htt_stats_buf->dialog_id);
	len += scnprintf(buf + len, buf_len - len, "wake_dura_us = %u\n",
			   htt_stats_buf->wake_dura_us);
	len += scnprintf(buf + len, buf_len - len, "wake_intvl_us = %u\n",
			   htt_stats_buf->wake_intvl_us);
	len += scnprintf(buf + len, buf_len - len, "sp_offset_us = %u\n",
			   htt_stats_buf->sp_offset_us);

	stats_req->buf_len = len;
}

static inline void ath12k_htt_print_backpressure_stats_tlv_v(const u32 *tag_buf,
							     u8 *data)
{
	struct debug_htt_stats_req *stats_req =
			(struct debug_htt_stats_req *)data;
	struct ath12k_htt_ring_backpressure_stats_tlv *htt_stats_buf =
			(struct ath12k_htt_ring_backpressure_stats_tlv *)tag_buf;
	int i;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n",
			   htt_stats_buf->pdev_id);
	len += scnprintf(buf + len, buf_len - len, "current_head_idx = %u\n",
			   htt_stats_buf->current_head_idx);
	len += scnprintf(buf + len, buf_len - len, "current_tail_idx = %u\n",
			   htt_stats_buf->current_tail_idx);
	len += scnprintf(buf + len, buf_len - len, "num_htt_msgs_sent = %u\n",
			   htt_stats_buf->num_htt_msgs_sent);
	len += scnprintf(buf + len, buf_len - len,
			   "backpressure_time_ms = %u\n",
			   htt_stats_buf->backpressure_time_ms);

	for (i = 0; i < 5; i++)
		len += scnprintf(buf + len, buf_len - len,
				   "backpressure_hist_%u = %u\n",
				   i + 1, htt_stats_buf->backpressure_hist[i]);

	len += scnprintf(buf + len, buf_len - len,
			   "============================");

	stats_req->buf_len = len;

}

static int ath12k_prep_htt_stats_cfg_params(struct ath12k *ar, u8 type,
					    const u8 *mac_addr,
					    struct htt_ext_stats_cfg_params *cfg_params)
{
	if (!cfg_params)
		return -EINVAL;

	switch (type) {
	case ATH12K_DBG_HTT_EXT_STATS_PDEV_TX_HWQ:
	case ATH12K_DBG_HTT_EXT_STATS_TX_MU_HWQ:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_ALL_HWQS;
		break;
	case ATH12K_DBG_HTT_EXT_STATS_PDEV_TX_SCHED:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_ALL_TXQS;
		break;
	case ATH12K_DBG_HTT_EXT_STATS_TQM_CMDQ:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_ALL_CMDQS;
		break;
	case ATH12K_DBG_HTT_EXT_STATS_PEER_INFO:
		cfg_params->cfg0 = HTT_STAT_PEER_INFO_MAC_ADDR;
		cfg_params->cfg0 |= FIELD_PREP(GENMASK(15, 1),
					HTT_PEER_STATS_REQ_MODE_FLUSH_TQM);
		cfg_params->cfg1 = HTT_STAT_DEFAULT_PEER_REQ_TYPE;
		cfg_params->cfg2 |= FIELD_PREP(GENMASK(7, 0), mac_addr[0]);
		cfg_params->cfg2 |= FIELD_PREP(GENMASK(15, 8), mac_addr[1]);
		cfg_params->cfg2 |= FIELD_PREP(GENMASK(23, 16), mac_addr[2]);
		cfg_params->cfg2 |= FIELD_PREP(GENMASK(31, 24), mac_addr[3]);
		cfg_params->cfg3 |= FIELD_PREP(GENMASK(7, 0), mac_addr[4]);
		cfg_params->cfg3 |= FIELD_PREP(GENMASK(15, 8), mac_addr[5]);
		break;
	case ATH12K_DBG_HTT_EXT_STATS_RING_IF_INFO:
	case ATH12K_DBG_HTT_EXT_STATS_SRNG_INFO:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_ALL_RINGS;
		break;
	case ATH12K_DBG_HTT_EXT_STATS_ACTIVE_PEERS_LIST:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_ACTIVE_PEERS;
		break;
	case ATH12K_DBG_HTT_EXT_STATS_PDEV_CCA_STATS:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_CCA_CUMULATIVE;
		break;
	case ATH12K_DBG_HTT_EXT_STATS_TX_SOUNDING_INFO:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_ACTIVE_VDEVS;
		break;
	case ATH12K_DBG_HTT_DBG_EXT_STATS_ML_PEERS_INFO:
		cfg_params->cfg0 = HTT_STAT_DEFAULT_CFG0_MASK;
		break;
	default:
		break;
	}

	return 0;
}

static const char *ath12k_htt_ax_tx_rx_ru_size_to_str(u8 ru_size)
{
	switch (ru_size) {
	case ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_26:
		return "26";
	case ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_52:
		return "52";
	case ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_106:
		return "106";
	case ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_242:
		return "242";
	case ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_484:
		return "484";
	case ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_996:
		return "996";
	case ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_996x2:
		return "996x2";
	default:
		return "unknown";
	}
}

static const char *ath12k_htt_be_tx_rx_ru_size_to_str(u8 ru_size)
{
	switch (ru_size) {
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_26:
		return "26";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_52:
		return "52";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_52_26:
		return "52+26";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_106:
		return "106";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_106_26:
		return "106+26";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_242:
		return "242";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_484:
		return "484";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_484_242:
		return "484+242";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996:
		return "996";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996_484:
		return "996+484";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996_484_242:
		return "996+484+242";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x2:
		return "996x2";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x2_484:
		return "996x2+484";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x3:
		return "996x3";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x3_484:
		return "996x3+484";
	case ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x4:
		return "996x4";
	default:
		return "unknown";
	}
}

static const char*
ath12k_tx_ru_size_to_str(enum ath12k_htt_stats_ru_type ru_type, u8 ru_size)
{
	if (ru_type == ATH12K_HTT_STATS_RU_TYPE_SINGLE_RU_ONLY)
		return ath12k_htt_ax_tx_rx_ru_size_to_str(ru_size);
	else if (ru_type == ATH12K_HTT_STATS_RU_TYPE_SINGLE_AND_MULTI_RU)
		return ath12k_htt_be_tx_rx_ru_size_to_str(ru_size);
	else
		return "unknown";
}

static void
ath12k_htt_print_tx_pdev_stats_cmn_tlv(const void *tag_buf, u16 tag_len,
				       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_cmn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_STATS_CMN_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "comp_delivered = %u\n",
			 le32_to_cpu(htt_stats_buf->comp_delivered));
	len += scnprintf(buf + len, buf_len - len, "self_triggers = %u\n",
			 le32_to_cpu(htt_stats_buf->self_triggers));
	len += scnprintf(buf + len, buf_len - len, "hw_queued = %u\n",
			 le32_to_cpu(htt_stats_buf->hw_queued));
	len += scnprintf(buf + len, buf_len - len, "hw_reaped = %u\n",
			 le32_to_cpu(htt_stats_buf->hw_reaped));
	len += scnprintf(buf + len, buf_len - len, "underrun = %u\n",
			 le32_to_cpu(htt_stats_buf->underrun));
	len += scnprintf(buf + len, buf_len - len, "hw_paused = %u\n",
			 le32_to_cpu(htt_stats_buf->hw_paused));
	len += scnprintf(buf + len, buf_len - len, "hw_flush = %u\n",
			 le32_to_cpu(htt_stats_buf->hw_flush));
	len += scnprintf(buf + len, buf_len - len, "hw_filt = %u\n",
			 le32_to_cpu(htt_stats_buf->hw_filt));
	len += scnprintf(buf + len, buf_len - len, "tx_abort = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_abort));
	len += scnprintf(buf + len, buf_len - len, "ppdu_ok = %u\n",
			 le32_to_cpu(htt_stats_buf->ppdu_ok));
	len += scnprintf(buf + len, buf_len - len, "mpdu_requeued = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_requed));
	len += scnprintf(buf + len, buf_len - len, "tx_xretry = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_xretry));
	len += scnprintf(buf + len, buf_len - len, "data_rc = %u\n",
			 le32_to_cpu(htt_stats_buf->data_rc));
	len += scnprintf(buf + len, buf_len - len, "mpdu_dropped_xretry = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_dropped_xretry));
	len += scnprintf(buf + len, buf_len - len, "illegal_rate_phy_err = %u\n",
			 le32_to_cpu(htt_stats_buf->illgl_rate_phy_err));
	len += scnprintf(buf + len, buf_len - len, "cont_xretry = %u\n",
			 le32_to_cpu(htt_stats_buf->cont_xretry));
	len += scnprintf(buf + len, buf_len - len, "tx_timeout = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_timeout));
	len += scnprintf(buf + len, buf_len - len, "tx_time_dur_data = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_time_dur_data));
	len += scnprintf(buf + len, buf_len - len, "pdev_resets = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_resets));
	len += scnprintf(buf + len, buf_len - len, "phy_underrun = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_underrun));
	len += scnprintf(buf + len, buf_len - len, "txop_ovf = %u\n",
			 le32_to_cpu(htt_stats_buf->txop_ovf));
	len += scnprintf(buf + len, buf_len - len, "seq_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_posted));
	len += scnprintf(buf + len, buf_len - len, "seq_failed_queueing = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_failed_queueing));
	len += scnprintf(buf + len, buf_len - len, "seq_completed = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_completed));
	len += scnprintf(buf + len, buf_len - len, "seq_restarted = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_restarted));
	len += scnprintf(buf + len, buf_len - len, "seq_txop_repost_stop = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_txop_repost_stop));
	len += scnprintf(buf + len, buf_len - len, "next_seq_cancel = %u\n",
			 le32_to_cpu(htt_stats_buf->next_seq_cancel));
	len += scnprintf(buf + len, buf_len - len, "dl_mu_mimo_seq_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_seq_posted));
	len += scnprintf(buf + len, buf_len - len, "dl_mu_ofdma_seq_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_ofdma_seq_posted));
	len += scnprintf(buf + len, buf_len - len, "ul_mu_mimo_seq_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_mumimo_seq_posted));
	len += scnprintf(buf + len, buf_len - len, "ul_mu_ofdma_seq_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_ofdma_seq_posted));
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_peer_denylist_count = %u\n",
			 le32_to_cpu(htt_stats_buf->num_mu_peer_blacklisted));
	len += scnprintf(buf + len, buf_len - len, "seq_qdepth_repost_stop = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_qdepth_repost_stop));
	len += scnprintf(buf + len, buf_len - len, "seq_min_msdu_repost_stop = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_min_msdu_repost_stop));
	len += scnprintf(buf + len, buf_len - len, "mu_seq_min_msdu_repost_stop = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_seq_min_msdu_repost_stop));
	len += scnprintf(buf + len, buf_len - len, "seq_switch_hw_paused = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_switch_hw_paused));
	len += scnprintf(buf + len, buf_len - len, "next_seq_posted_dsr = %u\n",
			 le32_to_cpu(htt_stats_buf->next_seq_posted_dsr));
	len += scnprintf(buf + len, buf_len - len, "seq_posted_isr = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_posted_isr));
	len += scnprintf(buf + len, buf_len - len, "seq_ctrl_cached = %u\n",
			 le32_to_cpu(htt_stats_buf->seq_ctrl_cached));
	len += scnprintf(buf + len, buf_len - len, "mpdu_count_tqm = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_count_tqm));
	len += scnprintf(buf + len, buf_len - len, "msdu_count_tqm = %u\n",
			 le32_to_cpu(htt_stats_buf->msdu_count_tqm));
	len += scnprintf(buf + len, buf_len - len, "mpdu_removed_tqm = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_removed_tqm));
	len += scnprintf(buf + len, buf_len - len, "msdu_removed_tqm = %u\n",
			 le32_to_cpu(htt_stats_buf->msdu_removed_tqm));
	len += scnprintf(buf + len, buf_len - len, "remove_mpdus_max_retries = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_mpdus_max_retries));
	len += scnprintf(buf + len, buf_len - len, "mpdus_sw_flush = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdus_sw_flush));
	len += scnprintf(buf + len, buf_len - len, "mpdus_hw_filter = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdus_hw_filter));
	len += scnprintf(buf + len, buf_len - len, "mpdus_truncated = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdus_truncated));
	len += scnprintf(buf + len, buf_len - len, "mpdus_ack_failed = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdus_ack_failed));
	len += scnprintf(buf + len, buf_len - len, "mpdus_expired = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdus_expired));
	len += scnprintf(buf + len, buf_len - len, "mpdus_seq_hw_retry = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdus_seq_hw_retry));
	len += scnprintf(buf + len, buf_len - len, "ack_tlv_proc = %u\n",
			 le32_to_cpu(htt_stats_buf->ack_tlv_proc));
	len += scnprintf(buf + len, buf_len - len, "coex_abort_mpdu_cnt_valid = %u\n",
			 le32_to_cpu(htt_stats_buf->coex_abort_mpdu_cnt_valid));
	len += scnprintf(buf + len, buf_len - len, "coex_abort_mpdu_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->coex_abort_mpdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "num_total_ppdus_tried_ota = %u\n",
			 le32_to_cpu(htt_stats_buf->num_total_ppdus_tried_ota));
	len += scnprintf(buf + len, buf_len - len, "num_data_ppdus_tried_ota = %u\n",
			 le32_to_cpu(htt_stats_buf->num_data_ppdus_tried_ota));
	len += scnprintf(buf + len, buf_len - len, "local_ctrl_mgmt_enqued = %u\n",
			 le32_to_cpu(htt_stats_buf->local_ctrl_mgmt_enqued));
	len += scnprintf(buf + len, buf_len - len, "local_ctrl_mgmt_freed = %u\n",
			 le32_to_cpu(htt_stats_buf->local_ctrl_mgmt_freed));
	len += scnprintf(buf + len, buf_len - len, "local_data_enqued = %u\n",
			 le32_to_cpu(htt_stats_buf->local_data_enqued));
	len += scnprintf(buf + len, buf_len - len, "local_data_freed = %u\n",
			 le32_to_cpu(htt_stats_buf->local_data_freed));
	len += scnprintf(buf + len, buf_len - len, "mpdu_tried = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_tried));
	len += scnprintf(buf + len, buf_len - len, "isr_wait_seq_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->isr_wait_seq_posted));
	len += scnprintf(buf + len, buf_len - len, "tx_active_dur_us_low = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_active_dur_us_low));
	len += scnprintf(buf + len, buf_len - len, "tx_active_dur_us_high = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_active_dur_us_high));
	len += scnprintf(buf + len, buf_len - len, "fes_offsets_err_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->fes_offsets_err_cnt));
	len += scnprintf(buf + len, buf_len - len, "thermal_suspend_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->thermal_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len, "dfs_suspend_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->dfs_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len, "tx_abort_suspend_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_abort_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "tgt_specific_opaque_txq_suspend_info = %u\n",
			 le32_to_cpu(htt_stats_buf->tgt_spec_opaque_txq_suspend_info));
	len += scnprintf(buf + len, buf_len - len, "last_suspend_reason = %u\n",
			 le32_to_cpu(htt_stats_buf->last_suspend_reason));
	len += scnprintf(buf + len, buf_len - len,
			 "num_dyn_mimo_ps_dlmumimo_sequences = %u\n",
			 le32_to_cpu(htt_stats_buf->num_dyn_mimo_ps_dlmumimo_sequences));
	len += scnprintf(buf + len, buf_len - len, "num_su_txbf_denylisted = %u\n",
			 le32_to_cpu(htt_stats_buf->num_su_txbf_denylisted));
	len += scnprintf(buf + len, buf_len - len, "pdev_up_time_us_low = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_up_time_us_low));
	len += scnprintf(buf + len, buf_len - len, "pdev_up_time_us_high = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_up_time_us_high));
	len += scnprintf(buf + len, buf_len - len, "ofdma_seq_flush = %u\n",
			 le32_to_cpu(htt_stats_buf->ofdma_seq_flush));
	len += scnprintf(buf + len, buf_len - len, "bytes_sent.low_32 = %u\n",
			 le32_to_cpu(htt_stats_buf->bytes_sent.low_32));
	len += scnprintf(buf + len, buf_len - len, "bytes_sent.high_32 = %u\n\n",
			 le32_to_cpu(htt_stats_buf->bytes_sent.high_32));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_pending_seq_cnt_on_sched_post_hist_tlv(const void *tag_buf,
								u16 tag_len,
								struct debug_htt_stats_req
								*stats_req)
{
	const struct ath12k_htt_tx_pdev_pending_seq_cnt_on_sched_post_hist_tlv
			*htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_PENDING_SEQ_CNT_ON_SCHED_POST_HIST_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += print_array_to_buf(buf, len, "pending_seq_on_sched_post_hist",
				  htt_stats_buf->pending_seq_on_sched_post_hist,
				  ATH12K_HTT_PDEV_STATS_MAX_SEQ_CTRL_HIST,
				  "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_pending_seq_cnt_in_hwq_hist_tlv(const void *tag_buf,
					u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_pending_seq_cnt_in_hwq_hist_tlv
			*htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_PENDING_SEQ_CNT_IN_HWQ_HIST_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += print_array_to_buf(buf, len, "active_seq_in_hwq_hist",
				  htt_stats_buf->active_seq_in_hwq_hist,
				  ATH12K_HTT_PDEV_STATS_MAX_ACTIVE_SEQ_IN_HWQ_HIST,
				  "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_pending_seq_cnt_in_txq_hist_tlv(const void *tag_buf,
					u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_pending_seq_cnt_in_txq_hist_tlv
			*htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_PENDING_SEQ_CNT_IN_TXQ_HIST_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += print_array_to_buf(buf, len, "active_seq_in_txq_hist",
				  htt_stats_buf->active_seq_in_txq_hist,
				  ATH12K_HTT_PDEV_STATS_MAX_SEQ_CTRL_HIST,
				  "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sched_txq_early_compl_tlv(const void *tag_buf, u16 tag_len,
					   struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sched_txq_early_compl_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_SCHED_TXQ_EARLY_COMPL_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "ist_txop_end_indicated_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->ist_txop_end_indicated_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "ist_txop_end_notify_at_cmd_status_end = %u\n",
			 le32_to_cpu
			 (htt_stats_buf->ist_txop_end_notify_at_cmd_status_end));
	len += scnprintf(buf + len, buf_len - len,
			 "ist_txop_end_notify_at_isr_end = %u\n",
			 le32_to_cpu(htt_stats_buf->ist_txop_end_notify_at_isr_end));
	len += scnprintf(buf + len, buf_len - len,
			 "sched_cmd_post_skip_on_seq_unavail = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_cmd_post_skip_on_seq_unavail));
	len += scnprintf(buf + len, buf_len - len,
			 "ist_txop_end_skip_on_seq_unavail = %u\n",
			 le32_to_cpu(htt_stats_buf->ist_txop_end_skip_on_seq_unavail));
	len += scnprintf(buf + len, buf_len - len,
			 "ist_txop_end_skip_on_mpdu_ownership = %u\n",
			 le32_to_cpu(htt_stats_buf->ist_txop_end_skip_on_mpdu_ownership));
	len += scnprintf(buf + len, buf_len - len,
			 "skip_early_schedule_due_to_per = %u\n",
			 le32_to_cpu(htt_stats_buf->skip_early_schedule_due_to_per));
	len += scnprintf(buf + len, buf_len - len,
			 "sched_cmd_posted_at_hw_txop_end = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_cmd_posted_at_hw_txop_end));
	len += scnprintf(buf + len, buf_len - len,
			 "sched_cmd_missed_at_hw_txop_end = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_cmd_missed_at_hw_txop_end));
	len += scnprintf(buf + len, buf_len - len,
			 "sched_cmd_posted_at_sched_cmd_compl = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_cmd_posted_at_sched_cmd_compl));
	len += scnprintf(buf + len, buf_len - len,
			 "sched_cmd_missed_at_sched_cmd_compl = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_cmd_missed_at_sched_cmd_compl));
	len += scnprintf(buf + len, buf_len - len, "num_QoS_sched_runs = %u\n\n",
			 le32_to_cpu(htt_stats_buf->num_qos_sched_runs));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_ap_edca_params_stats_tlv(const void *tag_buf, u16 tag_len,
						  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_ap_edca_params_stats_tlv *stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;

	if (tag_len < sizeof(*stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_AP_EDCA_PARAMS_STATS_TLV:\n");

	len += scnprintf(buf + len, buf_len - len,
			 "AP EDCA PARAMETERS FOR UL MUMIMO:\n");
	for (i = 0; i < ATH12K_HTT_NUM_AC_WMM; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ul_mumimo_less_aggressive[%u] = %u\n",
				 i, stats_buf->ul_mumimo_less_aggressive[i]);
		len += scnprintf(buf + len, buf_len - len,
				 "ul_mumimo_medium_aggressive[%u] = %u\n",
				 i, stats_buf->ul_mumimo_medium_aggressive[i]);
		len += scnprintf(buf + len, buf_len - len,
				 "ul_mumimo_highly_aggressive[%u] = %u\n",
				 i, stats_buf->ul_mumimo_highly_aggressive[i]);
		len += scnprintf(buf + len, buf_len - len,
				 "ul_mumimo_default_relaxed[%u] = %u\n\n",
				 i, stats_buf->ul_mumimo_default_relaxed[i]);
	}

	len += scnprintf(buf + len, buf_len - len,
			 "AP EDCA PARAMETERS FOR UL OFDMA:\n");
	for (i = 0; i < ATH12K_HTT_NUM_AC_WMM; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ul_muofdma_less_aggressive[%u] = %u\n",
				 i, stats_buf->ul_muofdma_less_aggressive[i]);
		len += scnprintf(buf + len, buf_len - len,
				 "ul_muofdma_medium_aggressive[%u] = %u\n",
				 i, stats_buf->ul_muofdma_medium_aggressive[i]);
		len += scnprintf(buf + len, buf_len - len,
				 "ul_muofdma_highly_aggressive[%u] = %u\n",
				 i, stats_buf->ul_muofdma_highly_aggressive[i]);
		len += scnprintf(buf + len, buf_len - len,
				 "ul_muofdma_default_relaxed[%u] = %u\n",
				 i, stats_buf->ul_muofdma_default_relaxed[i]);
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_mu_edca_params_stats_tlv(const void *tag_buf, u16 tag_len,
						  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_mu_edca_params_stats_tlv *stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_MU_EDCA_PARAMS_STATS_TLV:\n");

	len += print_array_to_buf(buf, len, "relaxed_mu_edca",
				  stats_buf->relaxed_mu_edca, ATH12K_HTT_NUM_AC_WMM,
				  "\n");
	len += print_array_to_buf(buf, len, "mumimo_aggressive_mu_edca",
				  stats_buf->mumimo_aggressive_mu_edca,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "mumimo_relaxed_mu_edca",
				  stats_buf->mumimo_relaxed_mu_edca,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "muofdma_aggressive_mu_edca",
				  stats_buf->muofdma_aggressive_mu_edca,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "muofdma_relaxed_mu_edca",
				  stats_buf->muofdma_relaxed_mu_edca,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "latency_mu_edca",
				  stats_buf->latency_mu_edca,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "psd_boost_mu_edca",
				  stats_buf->psd_boost_mu_edca,
				  ATH12K_HTT_NUM_AC_WMM, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_mlo_abort_tlv(const void *tag_buf, u16 tag_len,
				       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_mlo_abort_tlv *stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      (ATH12K_HTT_TX_PDEV_MLO_ABORT_COUNT * sizeof(int)));

	if (tag_len < sizeof(*stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_MLO_ABORT_TAG:\n");

	len += print_array_to_buf(buf, len, "mlo_abort_cnt",
				  stats_buf->mlo_abort_cnt, num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_mlo_txop_abort_tlv(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_mlo_txop_abort_tlv *stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      (ATH12K_HTT_TX_PDEV_MLO_ABORT_COUNT * sizeof(int)));

	if (tag_len < sizeof(*stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_MLO_TXOP_ABORT_TLV:\n");

	len += print_array_to_buf(buf, len, "mlo_txop_abort_cnt",
				  stats_buf->mlo_txop_abort_cnt, num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_stats_urrn_tlv(const void *tag_buf,
					u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_urrn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      HTT_TX_PDEV_MAX_URRN_STATS);

	len += scnprintf(buf + len, buf_len - len,
			"HTT_TX_PDEV_STATS_URRN_TLV:\n");

	len += print_array_to_buf(buf, len, "urrn_stats", htt_stats_buf->urrn_stats,
				  num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_stats_flush_tlv(const void *tag_buf,
					 u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_flush_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      ATH12K_HTT_TX_PDEV_MAX_FLUSH_REASON_STATS);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_FLUSH_TLV:\n");

	len += print_array_to_buf(buf, len, "flush_errs", htt_stats_buf->flush_errs,
				  num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_stats_sifs_tlv(const void *tag_buf,
					u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_sifs_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      ATH12K_HTT_TX_PDEV_MAX_SIFS_BURST_STATS);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_SIFS_TLV:\n");

	len += print_array_to_buf(buf, len, "sifs_status", htt_stats_buf->sifs_status,
				  num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_mu_ppdu_dist_stats_tlv(const void *tag_buf, u16 tag_len,
						struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_mu_ppdu_dist_stats_tlv *htt_stats_buf = tag_buf;
	char *mode;
	u8 j, hw_mode, i, str_buf_len;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 stats_value;
	u8 max_ppdu = ATH12K_HTT_STATS_MAX_NUM_MU_PPDU_PER_BURST;
	u8 max_sched = ATH12K_HTT_STATS_MAX_NUM_SCHED_STATUS;
	char str_buf[ATH12K_HTT_MAX_STRING_LEN];

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	hw_mode = le32_to_cpu(htt_stats_buf->hw_mode);

	switch (hw_mode) {
	case ATH12K_HTT_STATS_HWMODE_AC:
		len += scnprintf(buf + len, buf_len - len,
				 "HTT_TX_PDEV_AC_MU_PPDU_DISTRIBUTION_STATS:\n");
		mode = "ac";
		break;
	case ATH12K_HTT_STATS_HWMODE_AX:
		len += scnprintf(buf + len, buf_len - len,
				 "HTT_TX_PDEV_AX_MU_PPDU_DISTRIBUTION_STATS:\n");
		mode = "ax";
		break;
	case ATH12K_HTT_STATS_HWMODE_BE:
		len += scnprintf(buf + len, buf_len - len,
				 "HTT_TX_PDEV_BE_MU_PPDU_DISTRIBUTION_STATS:\n");
		mode = "be";
		break;
	default:
		return;
	}

	for (i = 0; i < ATH12K_HTT_STATS_NUM_NR_BINS ; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "%s_mu_mimo_num_seq_posted_nr%u = %u\n", mode,
				 ((i + 1) * 4), htt_stats_buf->num_seq_posted[i]);
		str_buf_len = 0;
		memset(str_buf, 0x0, sizeof(str_buf));
		for (j = 0; j < ATH12K_HTT_STATS_MAX_NUM_MU_PPDU_PER_BURST ; j++) {
			stats_value = le32_to_cpu(htt_stats_buf->num_ppdu_posted_per_burst
						  [i * max_ppdu + j]);
			str_buf_len += scnprintf(&str_buf[str_buf_len],
						ATH12K_HTT_MAX_STRING_LEN - str_buf_len,
						" %u:%u,", j, stats_value);
		}
		/* To overwrite the last trailing comma */
		str_buf[str_buf_len - 1] = '\0';
		len += scnprintf(buf + len, buf_len - len,
				 "%s_mu_mimo_num_ppdu_posted_per_burst_nr%u = %s\n",
				 mode, ((i + 1) * 4), str_buf);
		str_buf_len = 0;
		memset(str_buf, 0x0, sizeof(str_buf));
		for (j = 0; j < ATH12K_HTT_STATS_MAX_NUM_MU_PPDU_PER_BURST ; j++) {
			stats_value = le32_to_cpu(htt_stats_buf->num_ppdu_cmpl_per_burst
						  [i * max_ppdu + j]);
			str_buf_len += scnprintf(&str_buf[str_buf_len],
						ATH12K_HTT_MAX_STRING_LEN - str_buf_len,
						" %u:%u,", j, stats_value);
		}
		/* To overwrite the last trailing comma */
		str_buf[str_buf_len - 1] = '\0';
		len += scnprintf(buf + len, buf_len - len,
				 "%s_mu_mimo_num_ppdu_completed_per_burst_nr%u = %s\n",
				 mode, ((i + 1) * 4), str_buf);
		str_buf_len = 0;
		memset(str_buf, 0x0, sizeof(str_buf));
		for (j = 0; j < ATH12K_HTT_STATS_MAX_NUM_SCHED_STATUS ; j++) {
			stats_value = le32_to_cpu(htt_stats_buf->num_seq_term_status
						  [i * max_sched + j]);
			str_buf_len += scnprintf(&str_buf[str_buf_len],
						ATH12K_HTT_MAX_STRING_LEN - str_buf_len,
						" %u:%u,", j, stats_value);
		}
		/* To overwrite the last trailing comma */
		str_buf[str_buf_len - 1] = '\0';
		len += scnprintf(buf + len, buf_len - len,
				 "%s_mu_mimo_num_seq_term_status_nr%u = %s\n\n",
				 mode, ((i + 1) * 4), str_buf);
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_stats_sifs_hist_tlv(const void *tag_buf,
					     u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_sifs_hist_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      ATH12K_HTT_TX_PDEV_MAX_SIFS_BURST_HIST_STATS);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_SIFS_HIST_TLV:\n");

	len += print_array_to_buf(buf, len, "sifs_hist_status",
				  htt_stats_buf->sifs_hist_status, num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_ctrl_path_tx_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_ctrl_path_tx_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_CTRL_PATH_TX_STATS:\n");
	len += print_array_to_buf(buf, len, "fw_tx_mgmt_subtype",
				 htt_stats_buf->fw_tx_mgmt_subtype,
				 ATH12K_HTT_STATS_SUBTYPE_MAX, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_stats_tx_sched_cmn_tlv(const void *tag_buf,
					u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_tx_sched_cmn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_TX_SCHED_CMN_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "current_timestamp = %u\n\n",
			 le32_to_cpu(htt_stats_buf->current_timestamp));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_stats_sched_per_txq_tlv(const void *tag_buf,
						 u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_stats_sched_per_txq_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_STATS_SCHED_PER_TXQ_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			u32_get_bits(mac_id_word,
				     ATH12K_HTT_TX_PDEV_STATS_SCHED_PER_TXQ_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "txq_id = %u\n",
			 u32_get_bits(mac_id_word,
				      ATH12K_HTT_TX_PDEV_STATS_SCHED_PER_TXQ_ID));
	len += scnprintf(buf + len, buf_len - len, "sched_policy = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_policy));
	len += scnprintf(buf + len, buf_len - len,
			 "last_sched_cmd_posted_timestamp = %u\n",
			 le32_to_cpu(htt_stats_buf->last_sched_cmd_posted_timestamp));
	len += scnprintf(buf + len, buf_len - len,
			 "last_sched_cmd_compl_timestamp = %u\n",
			 le32_to_cpu(htt_stats_buf->last_sched_cmd_compl_timestamp));
	len += scnprintf(buf + len, buf_len - len, "sched_2_tac_lwm_count = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_2_tac_lwm_count));
	len += scnprintf(buf + len, buf_len - len, "sched_2_tac_ring_full = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_2_tac_ring_full));
	len += scnprintf(buf + len, buf_len - len, "sched_cmd_post_failure = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_cmd_post_failure));
	len += scnprintf(buf + len, buf_len - len, "num_active_tids = %u\n",
			 le32_to_cpu(htt_stats_buf->num_active_tids));
	len += scnprintf(buf + len, buf_len - len, "num_ps_schedules = %u\n",
			 le32_to_cpu(htt_stats_buf->num_ps_schedules));
	len += scnprintf(buf + len, buf_len - len, "sched_cmds_pending = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_cmds_pending));
	len += scnprintf(buf + len, buf_len - len, "num_tid_register = %u\n",
			 le32_to_cpu(htt_stats_buf->num_tid_register));
	len += scnprintf(buf + len, buf_len - len, "num_tid_unregister = %u\n",
			 le32_to_cpu(htt_stats_buf->num_tid_unregister));
	len += scnprintf(buf + len, buf_len - len, "num_qstats_queried = %u\n",
			 le32_to_cpu(htt_stats_buf->num_qstats_queried));
	len += scnprintf(buf + len, buf_len - len, "qstats_update_pending = %u\n",
			 le32_to_cpu(htt_stats_buf->qstats_update_pending));
	len += scnprintf(buf + len, buf_len - len, "last_qstats_query_timestamp = %u\n",
			 le32_to_cpu(htt_stats_buf->last_qstats_query_timestamp));
	len += scnprintf(buf + len, buf_len - len, "num_tqm_cmdq_full = %u\n",
			 le32_to_cpu(htt_stats_buf->num_tqm_cmdq_full));
	len += scnprintf(buf + len, buf_len - len, "num_de_sched_algo_trigger = %u\n",
			 le32_to_cpu(htt_stats_buf->num_de_sched_algo_trigger));
	len += scnprintf(buf + len, buf_len - len, "num_rt_sched_algo_trigger = %u\n",
			 le32_to_cpu(htt_stats_buf->num_rt_sched_algo_trigger));
	len += scnprintf(buf + len, buf_len - len, "num_tqm_sched_algo_trigger = %u\n",
			 le32_to_cpu(htt_stats_buf->num_tqm_sched_algo_trigger));
	len += scnprintf(buf + len, buf_len - len, "notify_sched = %u\n",
			 le32_to_cpu(htt_stats_buf->notify_sched));
	len += scnprintf(buf + len, buf_len - len, "dur_based_sendn_term = %u\n",
			 le32_to_cpu(htt_stats_buf->dur_based_sendn_term));
	len += scnprintf(buf + len, buf_len - len, "su_notify2_sched = %u\n",
			 le32_to_cpu(htt_stats_buf->su_notify2_sched));
	len += scnprintf(buf + len, buf_len - len, "su_optimal_queued_msdus_sched = %u\n",
			 le32_to_cpu(htt_stats_buf->su_optimal_queued_msdus_sched));
	len += scnprintf(buf + len, buf_len - len, "su_delay_timeout_sched = %u\n",
			 le32_to_cpu(htt_stats_buf->su_delay_timeout_sched));
	len += scnprintf(buf + len, buf_len - len, "su_min_txtime_sched_delay = %u\n",
			 le32_to_cpu(htt_stats_buf->su_min_txtime_sched_delay));
	len += scnprintf(buf + len, buf_len - len, "su_no_delay = %u\n",
			 le32_to_cpu(htt_stats_buf->su_no_delay));
	len += scnprintf(buf + len, buf_len - len, "num_supercycles = %u\n",
			 le32_to_cpu(htt_stats_buf->num_supercycles));
	len += scnprintf(buf + len, buf_len - len, "num_subcycles_with_sort = %u\n",
			 le32_to_cpu(htt_stats_buf->num_subcycles_with_sort));
	len += scnprintf(buf + len, buf_len - len, "num_subcycles_no_sort = %u\n\n",
			 le32_to_cpu(htt_stats_buf->num_subcycles_no_sort));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sched_txq_cmd_posted_tlv(const void *tag_buf,
					  u16 tag_len,
					  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sched_txq_cmd_posted_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elements = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len, "HTT_SCHED_TXQ_CMD_POSTED_TLV:\n");
	len += print_array_to_buf(buf, len, "sched_cmd_posted",
				  htt_stats_buf->sched_cmd_posted, num_elements, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sched_txq_cmd_reaped_tlv(const void *tag_buf,
					  u16 tag_len,
					  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sched_txq_cmd_reaped_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elements = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len, "HTT_SCHED_TXQ_CMD_REAPED_TLV:\n");
	len += print_array_to_buf(buf, len, "sched_cmd_reaped",
				  htt_stats_buf->sched_cmd_reaped, num_elements, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sched_txq_sched_order_su_tlv(const void *tag_buf,
					      u16 tag_len,
					      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sched_txq_sched_order_su_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 sched_order_su_num_entries = min_t(u32, (tag_len >> 2),
					       ATH12K_HTT_TX_PDEV_NUM_SCHED_ORDER_LOG);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_SCHED_TXQ_SCHED_ORDER_SU_TLV:\n");
	len += print_array_to_buf(buf, len, "sched_order_su",
				  htt_stats_buf->sched_order_su,
				  sched_order_su_num_entries, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sched_txq_sched_ineligibility_tlv(const void *tag_buf,
						   u16 tag_len,
						   struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sched_txq_sched_ineligibility_tlv *htt_stats_buf =
		     tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 sched_ineligibility_num_entries = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_SCHED_TXQ_SCHED_INELIGIBILITY:\n");
	len += print_array_to_buf(buf, len, "sched_ineligibility",
				  htt_stats_buf->sched_ineligibility,
				  sched_ineligibility_num_entries, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sched_txq_supercycle_trigger_tlv(const void *tag_buf,
						  u16 tag_len,
						  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sched_txq_supercycle_triggers_tlv *htt_stats_buf =
		     tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_MAX);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_SCHED_TXQ_SUPERCYCLE_TRIGGER:\n");
	len += print_array_to_buf(buf, len, "supercycle_triggers",
				  htt_stats_buf->supercycle_triggers, num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_hw_stats_pdev_errs_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_hw_stats_pdev_errs_tlv *htt_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_buf))
		return;

	mac_id_word = le32_to_cpu(htt_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_HW_STATS_PDEV_ERRS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "tx_abort = %u\n",
			 le32_to_cpu(htt_buf->tx_abort));
	len += scnprintf(buf + len, buf_len - len, "tx_abort_fail_count = %u\n",
			 le32_to_cpu(htt_buf->tx_abort_fail_count));
	len += scnprintf(buf + len, buf_len - len, "rx_abort = %u\n",
			 le32_to_cpu(htt_buf->rx_abort));
	len += scnprintf(buf + len, buf_len - len, "rx_abort_fail_count = %u\n",
			 le32_to_cpu(htt_buf->rx_abort_fail_count));
	len += scnprintf(buf + len, buf_len - len, "rx_flush_cnt = %u\n",
			 le32_to_cpu(htt_buf->rx_flush_cnt));
	len += scnprintf(buf + len, buf_len - len, "warm_reset = %u\n",
			 le32_to_cpu(htt_buf->warm_reset));
	len += scnprintf(buf + len, buf_len - len, "cold_reset = %u\n",
			 le32_to_cpu(htt_buf->cold_reset));
	len += scnprintf(buf + len, buf_len - len, "mac_cold_reset_restore_cal = %u\n",
			 le32_to_cpu(htt_buf->mac_cold_reset_restore_cal));
	len += scnprintf(buf + len, buf_len - len, "mac_cold_reset = %u\n",
			 le32_to_cpu(htt_buf->mac_cold_reset));
	len += scnprintf(buf + len, buf_len - len, "mac_warm_reset = %u\n",
			 le32_to_cpu(htt_buf->mac_warm_reset));
	len += scnprintf(buf + len, buf_len - len, "mac_only_reset = %u\n",
			 le32_to_cpu(htt_buf->mac_only_reset));
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset));
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset_ucode_trig = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_ucode_trig));
	len += scnprintf(buf + len, buf_len - len, "mac_warm_reset_restore_cal = %u\n",
			 le32_to_cpu(htt_buf->mac_warm_reset_restore_cal));
	len += scnprintf(buf + len, buf_len - len, "mac_sfm_reset = %u\n",
			 le32_to_cpu(htt_buf->mac_sfm_reset));
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset_m3_ssr = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_m3_ssr));
	len += scnprintf(buf + len, buf_len - len, "fw_rx_rings_reset = %u\n",
			 le32_to_cpu(htt_buf->fw_rx_rings_reset));
	len += scnprintf(buf + len, buf_len - len, "tx_flush = %u\n",
			 le32_to_cpu(htt_buf->tx_flush));
	len += scnprintf(buf + len, buf_len - len, "tx_glb_reset = %u\n",
			 le32_to_cpu(htt_buf->tx_glb_reset));
	len += scnprintf(buf + len, buf_len - len, "tx_txq_reset = %u\n",
			 le32_to_cpu(htt_buf->tx_txq_reset));
	len += scnprintf(buf + len, buf_len - len, "rx_timeout_reset = %u\n\n",
			 le32_to_cpu(htt_buf->rx_timeout_reset));

	len += scnprintf(buf + len, buf_len - len, "PDEV_PHY_WARM_RESET_REASONS:\n");
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset_reason_phy_m3 = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_phy_m3));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_tx_hw_stuck = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_tx_hw_stuck));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_num_cca_rx_frame_stuck = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_num_rx_frame_stuck));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_wal_rx_recovery_rst_rx_busy = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_wal_rx_rec_rx_busy));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_wal_rx_recovery_rst_mac_hang = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_wal_rx_rec_mac_hng));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_mac_reset_converted_phy_reset = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_mac_conv_phy_reset));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_tx_lifetime_expiry_cca_stuck = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_tx_exp_cca_stuck));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_tx_consecutive_flush9_war = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_tx_consec_flsh_war));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_tx_hwsch_reset_war = %u\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_tx_hwsch_reset_war));
	len += scnprintf(buf + len, buf_len - len,
			 "phy_warm_reset_reason_hwsch_wdog_or_cca_wdog_war = %u\n\n",
			 le32_to_cpu(htt_buf->phy_warm_reset_reason_hwsch_cca_wdog_war));

	len += scnprintf(buf + len, buf_len - len, "WAL_RX_RECOVERY_STATS:\n");
	len += scnprintf(buf + len, buf_len - len,
			 "wal_rx_recovery_rst_mac_hang_count = %u\n",
			 le32_to_cpu(htt_buf->wal_rx_recovery_rst_mac_hang_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "wal_rx_recovery_rst_known_sig_count = %u\n",
			 le32_to_cpu(htt_buf->wal_rx_recovery_rst_known_sig_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "wal_rx_recovery_rst_no_rx_count = %u\n",
			 le32_to_cpu(htt_buf->wal_rx_recovery_rst_no_rx_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "wal_rx_recovery_rst_no_rx_consecutive_count = %u\n",
			 le32_to_cpu(htt_buf->wal_rx_recovery_rst_no_rx_consec_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "wal_rx_recovery_rst_rx_busy_count = %u\n",
			 le32_to_cpu(htt_buf->wal_rx_recovery_rst_rx_busy_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "wal_rx_recovery_rst_phy_mac_hang_count = %u\n\n",
			 le32_to_cpu(htt_buf->wal_rx_recovery_rst_phy_mac_hang_cnt));

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_DEST_DRAIN_STATS:\n");
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_rx_descs_leak_prevention_done = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_rx_descs_leak_prevented));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_rx_descs_saved_cnt = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_rx_descs_saved_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_rxdma2reo_leak_detected = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_rxdma2reo_leak_detected));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_rxdma2fw_leak_detected = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_rxdma2fw_leak_detected));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_rxdma2wbm_leak_detected = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_rxdma2wbm_leak_detected));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_rxdma1_2sw_leak_detected = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_rxdma1_2sw_leak_detected));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_rx_drain_ok_mac_idle = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_rx_drain_ok_mac_idle));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_ok_mac_not_idle = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_ok_mac_not_idle));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_prerequisite_invld = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_prerequisite_invld));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_skip_for_non_lmac_reset = %u\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_skip_non_lmac_reset));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_dest_drain_hw_fifo_not_empty_post_drain_wait = %u\n\n",
			 le32_to_cpu(htt_buf->rx_dest_drain_hw_fifo_notempty_post_wait));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_hw_stats_intr_misc_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_hw_stats_intr_misc_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_HW_STATS_INTR_MISC_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "hw_intr_name = %s\n",
			 htt_stats_buf->hw_intr_name);
	len += scnprintf(buf + len, buf_len - len, "mask = %u\n",
			 le32_to_cpu(htt_stats_buf->mask));
	len += scnprintf(buf + len, buf_len - len, "count = %u\n\n",
			 le32_to_cpu(htt_stats_buf->count));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_hw_stats_whal_tx_tlv(const void *tag_buf, u16 tag_len,
				      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_hw_stats_whal_tx_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_HW_STATS_WHAL_TX_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "last_unpause_ppdu_id = %u\n",
			 le32_to_cpu(htt_stats_buf->last_unpause_ppdu_id));
	len += scnprintf(buf + len, buf_len - len, "hwsch_unpause_wait_tqm_write = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_unpause_wait_tqm_write));
	len += scnprintf(buf + len, buf_len - len, "hwsch_dummy_tlv_skipped = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_dummy_tlv_skipped));
	len += scnprintf(buf + len, buf_len - len,
			 "hwsch_misaligned_offset_received = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_misaligned_offset_received));
	len += scnprintf(buf + len, buf_len - len, "hwsch_reset_count = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_reset_count));
	len += scnprintf(buf + len, buf_len - len, "hwsch_dev_reset_war = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_dev_reset_war));
	len += scnprintf(buf + len, buf_len - len, "hwsch_delayed_pause = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_delayed_pause));
	len += scnprintf(buf + len, buf_len - len, "hwsch_long_delayed_pause = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_long_delayed_pause));
	len += scnprintf(buf + len, buf_len - len, "sch_rx_ppdu_no_response = %u\n",
			 le32_to_cpu(htt_stats_buf->sch_rx_ppdu_no_response));
	len += scnprintf(buf + len, buf_len - len, "sch_selfgen_response = %u\n",
			 le32_to_cpu(htt_stats_buf->sch_selfgen_response));
	len += scnprintf(buf + len, buf_len - len, "sch_rx_sifs_resp_trigger= %u\n\n",
			 le32_to_cpu(htt_stats_buf->sch_rx_sifs_resp_trigger));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_hw_stats_whal_wsib_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_hw_stats_whal_wsib_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_HW_STATS_WHAL_WSI_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "wsib_event_watchdog_timeout = %u\n",
			 le32_to_cpu(htt_stats_buf->wsib_event_watchdog_timeout));
	len += scnprintf(buf + len, buf_len - len,
			 "wsib_event_slave_tlv_length_error = %u\n",
			 le32_to_cpu(htt_stats_buf->wsib_event_slave_tlv_length_error));
	len += scnprintf(buf + len, buf_len - len, "wsib_event_slave_parity_error = %u\n",
			 le32_to_cpu(htt_stats_buf->wsib_event_slave_parity_error));
	len += scnprintf(buf + len, buf_len - len,
			 "wsib_event_slave_direct_message = %u\n",
			 le32_to_cpu(htt_stats_buf->wsib_event_slave_direct_message));
	len += scnprintf(buf + len, buf_len - len,
			 "wsib_event_slave_backpressure_error = %u\n",
			 le32_to_cpu(htt_stats_buf->wsib_event_slave_backpressure_error));
	len += scnprintf(buf + len, buf_len - len,
			 "wsib_event_master_tlv_length_error = %u\n\n",
			 le32_to_cpu(htt_stats_buf->wsib_event_master_tlv_length_error));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_hw_war_tlv(const void *tag_buf, u16 tag_len,
			    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_hw_war_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 fixed_len, array_len;
	u8 i, array_words;
	u32 mac_id;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id = __le32_to_cpu(htt_stats_buf->mac_id__word);
	fixed_len = sizeof(*htt_stats_buf);
	array_len = tag_len - fixed_len;
	array_words = array_len >> 2;

	len += scnprintf(buf + len, buf_len - len, "HTT_HW_WAR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id, ATH12K_HTT_STATS_MAC_ID));

	for (i = 0; i < array_words; i++) {
		len += scnprintf(buf + len, buf_len - len, "hw_war %u = %u\n\n",
				 i, le32_to_cpu(htt_stats_buf->hw_wars[i]));
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_tqm_cmn_stats_tlv(const void *tag_buf, u16 tag_len,
				      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_tqm_cmn_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TQM_CMN_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "max_cmdq_id = %u\n",
			 le32_to_cpu(htt_stats_buf->max_cmdq_id));
	len += scnprintf(buf + len, buf_len - len, "list_mpdu_cnt_hist_intvl = %u\n",
			 le32_to_cpu(htt_stats_buf->list_mpdu_cnt_hist_intvl));
	len += scnprintf(buf + len, buf_len - len, "add_msdu = %u\n",
			 le32_to_cpu(htt_stats_buf->add_msdu));
	len += scnprintf(buf + len, buf_len - len, "q_empty = %u\n",
			 le32_to_cpu(htt_stats_buf->q_empty));
	len += scnprintf(buf + len, buf_len - len, "q_not_empty = %u\n",
			 le32_to_cpu(htt_stats_buf->q_not_empty));
	len += scnprintf(buf + len, buf_len - len, "drop_notification = %u\n",
			 le32_to_cpu(htt_stats_buf->drop_notification));
	len += scnprintf(buf + len, buf_len - len, "desc_threshold = %u\n",
			 le32_to_cpu(htt_stats_buf->desc_threshold));
	len += scnprintf(buf + len, buf_len - len, "hwsch_tqm_invalid_status = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_tqm_invalid_status));
	len += scnprintf(buf + len, buf_len - len, "missed_tqm_gen_mpdus = %u\n",
			 le32_to_cpu(htt_stats_buf->missed_tqm_gen_mpdus));
	len += scnprintf(buf + len, buf_len - len,
			 "total_msduq_timestamp_updates = %u\n",
			 le32_to_cpu(htt_stats_buf->msduq_timestamp_updates));
	len += scnprintf(buf + len, buf_len - len,
			 "total_msduq_timestamp_updates_by_get_mpdu_head_info_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->msduq_updates_mpdu_head_info_cmd));
	len += scnprintf(buf + len, buf_len - len,
			 "total_msduq_timestamp_updates_by_emp_to_nonemp_status = %u\n",
			 le32_to_cpu(htt_stats_buf->msduq_updates_emp_to_nonemp_status));
	len += scnprintf(buf + len, buf_len - len,
			 "total_get_mpdu_head_info_cmds_by_sched_algo_la_query = %u\n",
			 le32_to_cpu(htt_stats_buf->get_mpdu_head_info_cmds_by_query));
	len += scnprintf(buf + len, buf_len - len,
			 "total_get_mpdu_head_info_cmds_by_tac = %u\n",
			 le32_to_cpu(htt_stats_buf->get_mpdu_head_info_cmds_by_tac));
	len += scnprintf(buf + len, buf_len - len,
			 "total_gen_mpdu_cmds_by_sched_algo_la_query = %u\n",
			 le32_to_cpu(htt_stats_buf->gen_mpdu_cmds_by_query));
	len += scnprintf(buf + len, buf_len - len, "active_tqm_tids = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_active_tids));
	len += scnprintf(buf + len, buf_len - len, "inactive_tqm_tids = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_inactive_tids));
	len += scnprintf(buf + len, buf_len - len, "tqm_active_msduq_flows = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_active_msduq_flows));
	len += scnprintf(buf + len, buf_len - len, "hi_prio_q_not_empty = %u\n\n",
			 le32_to_cpu(htt_stats_buf->high_prio_q_not_empty));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_tqm_error_stats_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_tqm_error_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TQM_ERROR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "q_empty_failure = %u\n",
			 le32_to_cpu(htt_stats_buf->q_empty_failure));
	len += scnprintf(buf + len, buf_len - len, "q_not_empty_failure = %u\n",
			 le32_to_cpu(htt_stats_buf->q_not_empty_failure));
	len += scnprintf(buf + len, buf_len - len, "add_msdu_failure = %u\n\n",
			 le32_to_cpu(htt_stats_buf->add_msdu_failure));

	len += scnprintf(buf + len, buf_len - len, "TQM_ERROR_RESET_STATS:\n");
	len += scnprintf(buf + len, buf_len - len, "tqm_cache_ctl_err = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_cache_ctl_err));
	len += scnprintf(buf + len, buf_len - len, "tqm_soft_reset = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_soft_reset));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_total_num_in_use_link_descs = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_num_in_use_link_descs));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_worst_case_num_lost_link_descs = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_num_lost_link_descs));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_worst_case_num_lost_host_tx_bufs_count = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_num_lost_host_tx_buf_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_num_in_use_link_descs_internal_tqm = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_num_in_use_internal_tqm));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_num_in_use_link_descs_wbm_idle_link_ring = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_num_in_use_idle_link_rng));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_time_to_tqm_hang_delta_ms = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_time_to_tqm_hang_delta_ms));
	len += scnprintf(buf + len, buf_len - len, "tqm_reset_recovery_time_ms = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_recovery_time_ms));
	len += scnprintf(buf + len, buf_len - len, "tqm_reset_num_peers_hdl = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_num_peers_hdl));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_cumm_dirty_hw_mpduq_proc_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_cumm_dirty_hw_mpduq_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_cumm_dirty_hw_msduq_proc = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_cumm_dirty_hw_msduq_proc));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_flush_cache_cmd_su_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_flush_cache_cmd_su_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_flush_cache_cmd_other_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_flush_cache_cmd_other_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_flush_cache_cmd_trig_type = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_flush_cache_cmd_trig_type));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_flush_cache_cmd_trig_cfg = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_flush_cache_cmd_trig_cfg));
	len += scnprintf(buf + len, buf_len - len,
			 "tqm_reset_flush_cache_cmd_skip_cmd_status_null = %u\n\n",
			 le32_to_cpu(htt_stats_buf->tqm_reset_flush_cmd_skp_status_null));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_tqm_gen_mpdu_stats_tlv(const void *tag_buf, u16 tag_len,
					   struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_tqm_gen_mpdu_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elements = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TQM_GEN_MPDU_STATS_TLV:\n");
	len += print_array_to_buf(buf, len, "gen_mpdu_end_reason",
				  htt_stats_buf->gen_mpdu_end_reason, num_elements,
				  "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_tqm_list_mpdu_stats_tlv(const void *tag_buf, u16 tag_len,
					    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_tqm_list_mpdu_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      ATH12K_HTT_TX_TQM_MAX_LIST_MPDU_END_REASON);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TQM_LIST_MPDU_STATS_TLV:\n");
	len += print_array_to_buf(buf, len, "list_mpdu_end_reason",
				  htt_stats_buf->list_mpdu_end_reason, num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_tqm_list_mpdu_cnt_tlv(const void *tag_buf, u16 tag_len,
					  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_tqm_list_mpdu_cnt_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = min_t(u16, (tag_len >> 2),
			      ATH12K_HTT_TX_TQM_MAX_LIST_MPDU_CNT_HISTOGRAM_BINS);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TQM_LIST_MPDU_CNT_TLV_V:\n");
	len += print_array_to_buf(buf, len, "list_mpdu_cnt_hist",
				  htt_stats_buf->list_mpdu_cnt_hist, num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_tqm_pdev_stats_tlv(const void *tag_buf, u16 tag_len,
				       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_tqm_pdev_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_TQM_PDEV_STATS_TLV_V:\n");
	len += scnprintf(buf + len, buf_len - len, "msdu_count = %u\n",
			 le32_to_cpu(htt_stats_buf->msdu_count));
	len += scnprintf(buf + len, buf_len - len, "mpdu_count = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_count));
	len += scnprintf(buf + len, buf_len - len, "remove_msdu = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_msdu));
	len += scnprintf(buf + len, buf_len - len, "remove_mpdu = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_mpdu));
	len += scnprintf(buf + len, buf_len - len, "remove_msdu_ttl = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_msdu_ttl));
	len += scnprintf(buf + len, buf_len - len, "send_bar = %u\n",
			 le32_to_cpu(htt_stats_buf->send_bar));
	len += scnprintf(buf + len, buf_len - len, "bar_sync = %u\n",
			 le32_to_cpu(htt_stats_buf->bar_sync));
	len += scnprintf(buf + len, buf_len - len, "notify_mpdu = %u\n",
			 le32_to_cpu(htt_stats_buf->notify_mpdu));
	len += scnprintf(buf + len, buf_len - len, "sync_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->sync_cmd));
	len += scnprintf(buf + len, buf_len - len, "write_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->write_cmd));
	len += scnprintf(buf + len, buf_len - len, "hwsch_trigger = %u\n",
			 le32_to_cpu(htt_stats_buf->hwsch_trigger));
	len += scnprintf(buf + len, buf_len - len, "ack_tlv_proc = %u\n",
			 le32_to_cpu(htt_stats_buf->ack_tlv_proc));
	len += scnprintf(buf + len, buf_len - len, "gen_mpdu_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->gen_mpdu_cmd));
	len += scnprintf(buf + len, buf_len - len, "gen_list_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->gen_list_cmd));
	len += scnprintf(buf + len, buf_len - len, "remove_mpdu_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_mpdu_cmd));
	len += scnprintf(buf + len, buf_len - len, "remove_mpdu_tried_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_mpdu_tried_cmd));
	len += scnprintf(buf + len, buf_len - len, "mpdu_queue_stats_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_queue_stats_cmd));
	len += scnprintf(buf + len, buf_len - len, "mpdu_head_info_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->mpdu_head_info_cmd));
	len += scnprintf(buf + len, buf_len - len, "msdu_flow_stats_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->msdu_flow_stats_cmd));
	len += scnprintf(buf + len, buf_len - len, "remove_msdu_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_msdu_cmd));
	len += scnprintf(buf + len, buf_len - len, "remove_msdu_ttl_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->remove_msdu_ttl_cmd));
	len += scnprintf(buf + len, buf_len - len, "flush_cache_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->flush_cache_cmd));
	len += scnprintf(buf + len, buf_len - len, "update_mpduq_cmd = %u\n",
			 le32_to_cpu(htt_stats_buf->update_mpduq_cmd));
	len += scnprintf(buf + len, buf_len - len, "enqueue = %u\n",
			 le32_to_cpu(htt_stats_buf->enqueue));
	len += scnprintf(buf + len, buf_len - len, "enqueue_notify = %u\n",
			 le32_to_cpu(htt_stats_buf->enqueue_notify));
	len += scnprintf(buf + len, buf_len - len, "notify_mpdu_at_head = %u\n",
			 le32_to_cpu(htt_stats_buf->notify_mpdu_at_head));
	len += scnprintf(buf + len, buf_len - len, "notify_mpdu_state_valid = %u\n",
			 le32_to_cpu(htt_stats_buf->notify_mpdu_state_valid));
	len += scnprintf(buf + len, buf_len - len, "sched_udp_notify1 = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_udp_notify1));
	len += scnprintf(buf + len, buf_len - len, "sched_udp_notify2 = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_udp_notify2));
	len += scnprintf(buf + len, buf_len - len, "sched_nonudp_notify1 = %u\n",
			 le32_to_cpu(htt_stats_buf->sched_nonudp_notify1));
	len += scnprintf(buf + len, buf_len - len, "sched_nonudp_notify2 = %u\n\n",
			 le32_to_cpu(htt_stats_buf->sched_nonudp_notify2));
	len += scnprintf(buf + len, buf_len - len, "tqm_enqueue_msdu_count = %u\n\n",
			 le32_to_cpu(htt_stats_buf->tqm_enqueue_msdu_count));
	len += scnprintf(buf + len, buf_len - len, "tqm_dropped_msdu_count = %u\n\n",
			 le32_to_cpu(htt_stats_buf->tqm_dropped_msdu_count));
	len += scnprintf(buf + len, buf_len - len, "tqm_dequeue_msdu_count = %u\n\n",
			 le32_to_cpu(htt_stats_buf->tqm_dequeue_msdu_count));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_cmn_stats_tlv(const void *tag_buf, u16 tag_len,
				     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_cmn_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_DE_CMN_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "tcl2fw_entry_count = %u\n",
			 le32_to_cpu(htt_stats_buf->tcl2fw_entry_count));
	len += scnprintf(buf + len, buf_len - len, "not_to_fw = %u\n",
			 le32_to_cpu(htt_stats_buf->not_to_fw));
	len += scnprintf(buf + len, buf_len - len, "invalid_pdev_vdev_peer = %u\n",
			 le32_to_cpu(htt_stats_buf->invalid_pdev_vdev_peer));
	len += scnprintf(buf + len, buf_len - len, "tcl_res_invalid_addrx = %u\n",
			 le32_to_cpu(htt_stats_buf->tcl_res_invalid_addrx));
	len += scnprintf(buf + len, buf_len - len, "wbm2fw_entry_count = %u\n",
			 le32_to_cpu(htt_stats_buf->wbm2fw_entry_count));
	len += scnprintf(buf + len, buf_len - len, "invalid_pdev = %u\n",
			 le32_to_cpu(htt_stats_buf->invalid_pdev));
	len += scnprintf(buf + len, buf_len - len, "tcl_res_addrx_timeout = %u\n",
			 le32_to_cpu(htt_stats_buf->tcl_res_addrx_timeout));
	len += scnprintf(buf + len, buf_len - len, "invalid_vdev = %u\n",
			 le32_to_cpu(htt_stats_buf->invalid_vdev));
	len += scnprintf(buf + len, buf_len - len, "invalid_tcl_exp_frame_desc = %u\n",
			 le32_to_cpu(htt_stats_buf->invalid_tcl_exp_frame_desc));
	len += scnprintf(buf + len, buf_len - len, "vdev_id_mismatch_count = %u\n\n",
			 le32_to_cpu(htt_stats_buf->vdev_id_mismatch_cnt));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_eapol_packets_stats_tlv(const void *tag_buf, u16 tag_len,
					       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_eapol_packets_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_DE_EAPOL_PACKETS_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "m1_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->m1_packets));
	len += scnprintf(buf + len, buf_len - len, "m1_success = %u\n",
			 le32_to_cpu(htt_stats_buf->m1_success));
	len += scnprintf(buf + len, buf_len - len, "m1_compl_fail = %u\n",
			 le32_to_cpu(htt_stats_buf->m1_compl_fail));
	len += scnprintf(buf + len, buf_len - len, "m2_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->m2_packets));
	len += scnprintf(buf + len, buf_len - len, "m2_success = %u\n",
			 le32_to_cpu(htt_stats_buf->m2_success));
	len += scnprintf(buf + len, buf_len - len, "m2_compl_fail = %u\n",
			 le32_to_cpu(htt_stats_buf->m2_compl_fail));
	len += scnprintf(buf + len, buf_len - len, "m3_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->m3_packets));
	len += scnprintf(buf + len, buf_len - len, "m3_success = %u\n",
			 le32_to_cpu(htt_stats_buf->m3_success));
	len += scnprintf(buf + len, buf_len - len, "m3_compl_fail = %u\n",
			 le32_to_cpu(htt_stats_buf->m3_compl_fail));
	len += scnprintf(buf + len, buf_len - len, "m4_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->m4_packets));
	len += scnprintf(buf + len, buf_len - len, "m4_success = %u\n",
			 le32_to_cpu(htt_stats_buf->m4_success));
	len += scnprintf(buf + len, buf_len - len, "m4_compl_fail = %u\n",
			 le32_to_cpu(htt_stats_buf->m4_compl_fail));
	len += scnprintf(buf + len, buf_len - len, "g1_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->g1_packets));
	len += scnprintf(buf + len, buf_len - len, "g1_success = %u\n",
			 le32_to_cpu(htt_stats_buf->g1_success));
	len += scnprintf(buf + len, buf_len - len, "g1_compl_fail = %u\n",
			 le32_to_cpu(htt_stats_buf->g1_compl_fail));
	len += scnprintf(buf + len, buf_len - len, "g2_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->g2_packets));
	len += scnprintf(buf + len, buf_len - len, "g2_success = %u\n",
			 le32_to_cpu(htt_stats_buf->g2_success));
	len += scnprintf(buf + len, buf_len - len, "g2_compl_fail = %u\n",
			 le32_to_cpu(htt_stats_buf->g2_compl_fail));
	len += scnprintf(buf + len, buf_len - len, "rc4_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->rc4_packets));
	len += scnprintf(buf + len, buf_len - len, "eap_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->eap_packets));
	len += scnprintf(buf + len, buf_len - len, "eapol_start_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->eapol_start_packets));
	len += scnprintf(buf + len, buf_len - len, "eapol_logoff_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->eapol_logoff_packets));
	len += scnprintf(buf + len, buf_len - len, "eapol_encap_asf_packets = %u\n\n",
			 le32_to_cpu(htt_stats_buf->eapol_encap_asf_packets));
	len += scnprintf(buf + len, buf_len - len, "m1_enq_success = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m1_enq_success));
	len += scnprintf(buf + len, buf_len - len, "m1_enq_fail = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m1_enq_fail));
	len += scnprintf(buf + len, buf_len - len, "m2_enq_success = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m2_enq_success));
	len += scnprintf(buf + len, buf_len - len, "m2_enq_fail = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m2_enq_fail));
	len += scnprintf(buf + len, buf_len - len, "m3_enq_success = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m3_enq_success));
	len += scnprintf(buf + len, buf_len - len, "m3_enq_fail = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m3_enq_fail));
	len += scnprintf(buf + len, buf_len - len, "m4_enq_success = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m4_enq_success));
	len += scnprintf(buf + len, buf_len - len, "m4_enq_fail = %u\n\n",
			 le32_to_cpu(htt_stats_buf->m4_enq_fail));
	len += scnprintf(buf + len, buf_len - len, "g1_enq_success = %u\n\n",
			 le32_to_cpu(htt_stats_buf->g1_enq_success));
	len += scnprintf(buf + len, buf_len - len, "g1_enq_fail = %u\n\n",
			 le32_to_cpu(htt_stats_buf->g1_enq_fail));
	len += scnprintf(buf + len, buf_len - len, "g2_enq_success = %u\n\n",
			 le32_to_cpu(htt_stats_buf->g2_enq_success));
	len += scnprintf(buf + len, buf_len - len, "g2_enq_fail = %u\n\n",
			 le32_to_cpu(htt_stats_buf->g2_enq_fail));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_classify_stats_tlv(const void *tag_buf, u16 tag_len,
					  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_classify_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_DE_CLASSIFY_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "arp_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->arp_packets));
	len += scnprintf(buf + len, buf_len - len, "arp_request = %u\n",
			 le32_to_cpu(htt_stats_buf->arp_request));
	len += scnprintf(buf + len, buf_len - len, "arp_response = %u\n",
			 le32_to_cpu(htt_stats_buf->arp_response));
	len += scnprintf(buf + len, buf_len - len, "igmp_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->igmp_packets));
	len += scnprintf(buf + len, buf_len - len, "dhcp_packets = %u\n",
			 le32_to_cpu(htt_stats_buf->dhcp_packets));
	len += scnprintf(buf + len, buf_len - len, "host_inspected = %u\n",
			 le32_to_cpu(htt_stats_buf->host_inspected));
	len += scnprintf(buf + len, buf_len - len, "htt_included = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_included));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_mcs = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_mcs));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_nss = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_nss));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_preamble_type = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_preamble_type));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_chainmask = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_chainmask));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_guard_interval = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_guard_interval));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_retries = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_retries));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_bw_info = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_bw_info));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_power = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_power));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_key_flags = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_key_flags));
	len += scnprintf(buf + len, buf_len - len, "htt_valid_no_encryption = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_valid_no_encryption));
	len += scnprintf(buf + len, buf_len - len, "fse_entry_count = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_entry_count));
	len += scnprintf(buf + len, buf_len - len, "fse_priority_be = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_priority_be));
	len += scnprintf(buf + len, buf_len - len, "fse_priority_high = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_priority_high));
	len += scnprintf(buf + len, buf_len - len, "fse_priority_low = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_priority_low));
	len += scnprintf(buf + len, buf_len - len, "fse_traffic_ptrn_be = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_traffic_ptrn_be));
	len += scnprintf(buf + len, buf_len - len, "fse_traffic_ptrn_over_sub = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_traffic_ptrn_over_sub));
	len += scnprintf(buf + len, buf_len - len, "fse_traffic_ptrn_bursty = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_traffic_ptrn_bursty));
	len += scnprintf(buf + len, buf_len - len, "fse_traffic_ptrn_interactive = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_traffic_ptrn_interactive));
	len += scnprintf(buf + len, buf_len - len, "fse_traffic_ptrn_periodic = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_traffic_ptrn_periodic));
	len += scnprintf(buf + len, buf_len - len, "fse_hwqueue_alloc = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_hwqueue_alloc));
	len += scnprintf(buf + len, buf_len - len, "fse_hwqueue_created = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_hwqueue_created));
	len += scnprintf(buf + len, buf_len - len, "fse_hwqueue_send_to_host = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_hwqueue_send_to_host));
	len += scnprintf(buf + len, buf_len - len, "mcast_entry = %u\n",
			 le32_to_cpu(htt_stats_buf->mcast_entry));
	len += scnprintf(buf + len, buf_len - len, "bcast_entry = %u\n",
			 le32_to_cpu(htt_stats_buf->bcast_entry));
	len += scnprintf(buf + len, buf_len - len, "htt_update_peer_cache = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_update_peer_cache));
	len += scnprintf(buf + len, buf_len - len, "htt_learning_frame = %u\n",
			 le32_to_cpu(htt_stats_buf->htt_learning_frame));
	len += scnprintf(buf + len, buf_len - len, "fse_invalid_peer = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_invalid_peer));
	len += scnprintf(buf + len, buf_len - len, "mec_notify = %u\n\n",
			 le32_to_cpu(htt_stats_buf->mec_notify));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_classify_failed_stats_tlv(const void *tag_buf, u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_classify_failed_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_DE_CLASSIFY_FAILED_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ap_bss_peer_not_found = %u\n",
			 le32_to_cpu(htt_stats_buf->ap_bss_peer_not_found));
	len += scnprintf(buf + len, buf_len - len, "ap_bcast_mcast_no_peer = %u\n",
			 le32_to_cpu(htt_stats_buf->ap_bcast_mcast_no_peer));
	len += scnprintf(buf + len, buf_len - len, "sta_delete_in_progress = %u\n",
			 le32_to_cpu(htt_stats_buf->sta_delete_in_progress));
	len += scnprintf(buf + len, buf_len - len, "ibss_no_bss_peer = %u\n",
			 le32_to_cpu(htt_stats_buf->ibss_no_bss_peer));
	len += scnprintf(buf + len, buf_len - len, "invalid_vdev_type = %u\n",
			 le32_to_cpu(htt_stats_buf->invalid_vdev_type));
	len += scnprintf(buf + len, buf_len - len, "invalid_ast_peer_entry = %u\n",
			 le32_to_cpu(htt_stats_buf->invalid_ast_peer_entry));
	len += scnprintf(buf + len, buf_len - len, "peer_entry_invalid = %u\n",
			 le32_to_cpu(htt_stats_buf->peer_entry_invalid));
	len += scnprintf(buf + len, buf_len - len, "ethertype_not_ip = %u\n",
			 le32_to_cpu(htt_stats_buf->ethertype_not_ip));
	len += scnprintf(buf + len, buf_len - len, "eapol_lookup_failed = %u\n",
			 le32_to_cpu(htt_stats_buf->eapol_lookup_failed));
	len += scnprintf(buf + len, buf_len - len, "qpeer_not_allow_data = %u\n",
			 le32_to_cpu(htt_stats_buf->qpeer_not_allow_data));
	len += scnprintf(buf + len, buf_len - len, "fse_tid_override = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_tid_override));
	len += scnprintf(buf + len, buf_len - len, "ipv6_jumbogram_zero_length = %u\n",
			 le32_to_cpu(htt_stats_buf->ipv6_jumbogram_zero_length));
	len += scnprintf(buf + len, buf_len - len, "qos_to_non_qos_in_prog = %u\n",
			 le32_to_cpu(htt_stats_buf->qos_to_non_qos_in_prog));
	len += scnprintf(buf + len, buf_len - len, "ap_bcast_mcast_eapol = %u\n",
			 le32_to_cpu(htt_stats_buf->ap_bcast_mcast_eapol));
	len += scnprintf(buf + len, buf_len - len, "unicast_on_ap_bss_peer = %u\n",
			 le32_to_cpu(htt_stats_buf->unicast_on_ap_bss_peer));
	len += scnprintf(buf + len, buf_len - len, "ap_vdev_invalid = %u\n",
			 le32_to_cpu(htt_stats_buf->ap_vdev_invalid));
	len += scnprintf(buf + len, buf_len - len, "incomplete_llc = %u\n",
			 le32_to_cpu(htt_stats_buf->incomplete_llc));
	len += scnprintf(buf + len, buf_len - len, "eapol_duplicate_m3 = %u\n",
			 le32_to_cpu(htt_stats_buf->eapol_duplicate_m3));
	len += scnprintf(buf + len, buf_len - len, "eapol_duplicate_m4 = %u\n\n",
			 le32_to_cpu(htt_stats_buf->eapol_duplicate_m4));
	len += scnprintf(buf + len, buf_len - len, "eapol_invalid_mac = %u\n",
			   le32_to_cpu(htt_stats_buf->eapol_invalid_mac));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_classify_status_stats_tlv(const void *tag_buf, u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_classify_status_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_DE_CLASSIFY_STATUS_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "eok = %u\n",
			 le32_to_cpu(htt_stats_buf->eok));
	len += scnprintf(buf + len, buf_len - len, "classify_done = %u\n",
			 le32_to_cpu(htt_stats_buf->classify_done));
	len += scnprintf(buf + len, buf_len - len, "lookup_failed = %u\n",
			 le32_to_cpu(htt_stats_buf->lookup_failed));
	len += scnprintf(buf + len, buf_len - len, "send_host_dhcp = %u\n",
			 le32_to_cpu(htt_stats_buf->send_host_dhcp));
	len += scnprintf(buf + len, buf_len - len, "send_host_mcast = %u\n",
			 le32_to_cpu(htt_stats_buf->send_host_mcast));
	len += scnprintf(buf + len, buf_len - len, "send_host_unknown_dest = %u\n",
			 le32_to_cpu(htt_stats_buf->send_host_unknown_dest));
	len += scnprintf(buf + len, buf_len - len, "send_host = %u\n",
			 le32_to_cpu(htt_stats_buf->send_host));
	len += scnprintf(buf + len, buf_len - len, "status_invalid = %u\n\n",
			 le32_to_cpu(htt_stats_buf->status_invalid));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_enqueue_packets_stats_tlv(const void *tag_buf, u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_enqueue_packets_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_DE_ENQUEUE_PACKETS_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "enqueued_pkts = %u\n",
			 le32_to_cpu(htt_stats_buf->enqueued_pkts));
	len += scnprintf(buf + len, buf_len - len, "to_tqm = %u\n",
			 le32_to_cpu(htt_stats_buf->to_tqm));
	len += scnprintf(buf + len, buf_len - len, "to_tqm_bypass = %u\n\n",
			 le32_to_cpu(htt_stats_buf->to_tqm_bypass));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_enqueue_discard_stats_tlv(const void *tag_buf, u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_enqueue_discard_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_DE_ENQUEUE_DISCARD_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "discarded_pkts = %u\n",
			 le32_to_cpu(htt_stats_buf->discarded_pkts));
	len += scnprintf(buf + len, buf_len - len, "local_frames = %u\n",
			 le32_to_cpu(htt_stats_buf->local_frames));
	len += scnprintf(buf + len, buf_len - len, "is_ext_msdu = %u\n\n",
			 le32_to_cpu(htt_stats_buf->is_ext_msdu));
	len += scnprintf(buf + len, buf_len - len, "mlo_invalid_routing_discard = %u\n",
			 le32_to_cpu(htt_stats_buf->mlo_invalid_routing_discard));
	len += scnprintf(buf + len, buf_len - len, "mlo_invalid_routing_dup_entry_discard = %u\n",
			 le32_to_cpu(htt_stats_buf->mlo_invalid_routing_dup_entry_discard));
	len += scnprintf(buf + len, buf_len - len, "discard_peer_unauthorized_pkts = %u\n",
			 le32_to_cpu(htt_stats_buf->discard_peer_unauthorized_pkts));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_de_compl_stats_tlv(const void *tag_buf, u16 tag_len,
				       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_de_compl_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_DE_COMPL_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "tcl_dummy_frame = %u\n",
			 le32_to_cpu(htt_stats_buf->tcl_dummy_frame));
	len += scnprintf(buf + len, buf_len - len, "tqm_dummy_frame = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_dummy_frame));
	len += scnprintf(buf + len, buf_len - len, "tqm_notify_frame = %u\n",
			 le32_to_cpu(htt_stats_buf->tqm_notify_frame));
	len += scnprintf(buf + len, buf_len - len, "fw2wbm_enq = %u\n",
			 le32_to_cpu(htt_stats_buf->fw2wbm_enq));
	len += scnprintf(buf + len, buf_len - len, "tqm_bypass_frame = %u\n\n",
			 le32_to_cpu(htt_stats_buf->tqm_bypass_frame));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_cmn_stats_tlv(const void *tag_buf, u16 tag_len,
					  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_cmn_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_SELFGEN_CMN_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "su_bar = %u\n",
			 le32_to_cpu(htt_stats_buf->su_bar));
	len += scnprintf(buf + len, buf_len - len, "rts = %u\n",
			 le32_to_cpu(htt_stats_buf->rts));
	len += scnprintf(buf + len, buf_len - len, "cts2self = %u\n",
			 le32_to_cpu(htt_stats_buf->cts2self));
	len += scnprintf(buf + len, buf_len - len, "qos_null = %u\n",
			 le32_to_cpu(htt_stats_buf->qos_null));
	len += scnprintf(buf + len, buf_len - len, "delayed_bar_1 = %u\n",
			 le32_to_cpu(htt_stats_buf->delayed_bar_1));
	len += scnprintf(buf + len, buf_len - len, "delayed_bar_2 = %u\n",
			 le32_to_cpu(htt_stats_buf->delayed_bar_2));
	len += scnprintf(buf + len, buf_len - len, "delayed_bar_3 = %u\n",
			 le32_to_cpu(htt_stats_buf->delayed_bar_3));
	len += scnprintf(buf + len, buf_len - len, "delayed_bar_4 = %u\n",
			 le32_to_cpu(htt_stats_buf->delayed_bar_4));
	len += scnprintf(buf + len, buf_len - len, "delayed_bar_5 = %u\n",
			 le32_to_cpu(htt_stats_buf->delayed_bar_5));
	len += scnprintf(buf + len, buf_len - len, "delayed_bar_6 = %u\n",
			 le32_to_cpu(htt_stats_buf->delayed_bar_6));
	len += scnprintf(buf + len, buf_len - len, "delayed_bar_7 = %u\n",
			 le32_to_cpu(htt_stats_buf->delayed_bar_7));
	len += scnprintf(buf + len, buf_len - len, "bar_with_tqm_head_seq_num = %u\n",
			le32_to_cpu(htt_stats_buf->bar_with_tqm_head_seq_num));
	len += scnprintf(buf + len, buf_len - len, "bar_with_tid_seq_num = %u\n",
			le32_to_cpu(htt_stats_buf->bar_with_tid_seq_num));
	len += scnprintf(buf + len, buf_len - len, "su_sw_rts_queued = %u\n",
			le32_to_cpu(htt_stats_buf->su_sw_rts_queued));
	len += scnprintf(buf + len, buf_len - len, "su_sw_rts_tried = %u\n",
			le32_to_cpu(htt_stats_buf->su_sw_rts_tried));
	len += scnprintf(buf + len, buf_len - len, "su_sw_rts_err = %u\n",
			le32_to_cpu(htt_stats_buf->su_sw_rts_err));
	len += scnprintf(buf + len, buf_len - len, "su_sw_rts_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->su_sw_rts_flushed));
	len += scnprintf(buf + len, buf_len - len, "su_sw_rts_rcvd_cts_diff_bw = %u\n",
			le32_to_cpu(htt_stats_buf->su_sw_rts_rcvd_cts_diff_bw));
	len += print_array_to_buf(buf, len, "smart_basic_trig_sch_histogram",
			 htt_stats_buf->smart_basic_trig_sch_histogram,
			ATH12K_HTT_MAX_NUM_SBT_INTR, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_ac_stats_tlv(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_ac_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;
	len += scnprintf(buf + len, buf_len - len, "HTT_TX_SELFGEN_AC_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndpa_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndpa_queued));
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndpa_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndpa));
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndp_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndp_queued));
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndp_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndp));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndpa_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndpa_queued));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndpa_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndpa));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndp_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndp_queued));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndp_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndp));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brpoll_1_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brpoll_1_queued));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brpoll_1_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brpoll_1));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brpoll_2_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brpoll_2_queued));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brpoll_2_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brpoll_2));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brpoll_3_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brpoll_3_queued));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brpoll_3_tried = %u\n\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brpoll_3));
	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_ax_stats_tlv(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_ax_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_SELFGEN_AX_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndpa_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndpa_queued));
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndpa_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndpa));
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndp_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndp_queued));
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndp_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndp));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndpa_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndpa_queued));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndpa_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndpa));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndp_queued = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndp_queued));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndp_tried = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndp));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_brpollX_queued = ");
	for (i = 0; i < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1; i++)
		len += scnprintf(buf + len, buf_len - len,
				" %u:%u ", i + 1,
				le32_to_cpu(htt_stats_buf->ax_mu_mimo_brpoll_queued[i]));

	len += scnprintf(buf + len, buf_len - len, "\nax_mu_mimo_brpollX_tried = ");
	for (i = 0; i < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1; i++)
		len += scnprintf(buf + len, buf_len - len,
				" %u:%u ", i + 1,
				le32_to_cpu(htt_stats_buf->ax_mu_mimo_brpoll[i]));

	len += scnprintf(buf + len, buf_len - len, "\nax_ul_mumimo_trigger = ");
	for (i = 0; i < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS; i++)
		len += scnprintf(buf + len, buf_len - len,
				" %u:%u ", i + 1,
				le32_to_cpu(htt_stats_buf->ax_ul_mumimo_trigger[i]));

	len += scnprintf(buf + len, buf_len - len, "\nax_basic_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->ax_basic_trigger));
	len += scnprintf(buf + len, buf_len - len, "ax_ulmumimo_total_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->ax_ulmumimo_trigger));
	len += scnprintf(buf + len, buf_len - len, "ax_bsr_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->ax_bsr_trigger));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_bar_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_bar_trigger));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_rts_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_rts_trigger));
	len += print_array_to_buf(buf, len, "combined_ax_bsr_trigger_tried",
			htt_stats_buf->combined_ax_bsr_trigger_tried,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "combined_ax_bsr_trigger_err",
			htt_stats_buf->combined_ax_bsr_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "standalone_ax_bsr_trigger_tried",
			htt_stats_buf->standalone_ax_bsr_trigger_tried,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "standalone_ax_bsr_trigger_err",
			htt_stats_buf->standalone_ax_bsr_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_ax_su_ulofdma_basic_trigger",
			htt_stats_buf->manual_ax_su_ulofdma_basic_trigger,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_ax_su_ulofdma_basic_trigger_err",
			htt_stats_buf->manual_ax_su_ulofdma_basic_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_ax_mu_ulofdma_basic_trigger",
			htt_stats_buf->manual_ax_mu_ulofdma_basic_trigger,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_ax_mu_ulofdma_basic_trigger_err",
			htt_stats_buf->manual_ax_mu_ulofdma_basic_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "ax_basic_trigger_per_ac",
			htt_stats_buf->ax_basic_trigger_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "ax_basic_trigger_errors_per_ac",
			htt_stats_buf->ax_basic_trigger_errors_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_bar_trigger_per_ac",
			htt_stats_buf->ax_mu_bar_trigger_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_bar_trigger_errors_per_ac",
			htt_stats_buf->ax_mu_bar_trigger_errors_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_be_stats_tlv(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_be_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			"HTT_TX_SELFGEN_BE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "be_su_ndpa_queued = %u\n",
			le32_to_cpu(htt_stats_buf->be_su_ndpa_queued));
	len += scnprintf(buf + len, buf_len - len, "be_su_ndpa_tried = %u\n",
			le32_to_cpu(htt_stats_buf->be_su_ndpa));
	len += scnprintf(buf + len, buf_len - len, "be_su_ndp_queued = %u\n",
			le32_to_cpu(htt_stats_buf->be_su_ndp_queued));
	len += scnprintf(buf + len, buf_len - len, "be_su_ndp_tried = %u\n",
			le32_to_cpu(htt_stats_buf->be_su_ndp));
	len += scnprintf(buf + len, buf_len - len,
			"be_mu_mimo_ndpa_queued = %u\n",
			le32_to_cpu(htt_stats_buf->be_mu_mimo_ndpa_queued));
	len += scnprintf(buf + len, buf_len - len,
			"be_mu_mimo_ndpa_tried = %u\n",
			le32_to_cpu(htt_stats_buf->be_mu_mimo_ndpa));
	len += scnprintf(buf + len, buf_len - len,
			"be_mu_mimo_ndp_queued = %u\n",
			le32_to_cpu(htt_stats_buf->be_mu_mimo_ndp_queued));
	len += scnprintf(buf + len, buf_len - len,
			"be_mu_mimo_ndp_tried = %u\n",
			le32_to_cpu(htt_stats_buf->be_mu_mimo_ndp));

	len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_brpollX_queued = ");
	for (i = 0; i < ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1; i++)
		len += scnprintf(buf + len, buf_len - len,
				" %u:%u ", i + 1,
				le32_to_cpu(htt_stats_buf->be_mu_mimo_brpoll_queued[i]));

	len += scnprintf(buf + len, buf_len - len, "\nbe_mu_mimo_brpollX_tried = ");
	for (i = 0; i < ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1; i++)
		len += scnprintf(buf + len, buf_len - len,
				" %u:%u ", i + 1,
				le32_to_cpu(htt_stats_buf->be_mu_mimo_brpoll[i]));

	len += scnprintf(buf + len, buf_len - len, "\nbe_ul_mumimo_trigger = ");
	for (i = 0; i < ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS; i++)
		len += scnprintf(buf + len, buf_len - len,
				" %u:%u ", i + 1,
				le32_to_cpu(htt_stats_buf->be_ul_mumimo_trigger[i]));

	len += scnprintf(buf + len, buf_len - len,
			"\nbe_basic_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->be_basic_trigger));
	len += scnprintf(buf + len, buf_len - len,
			"be_ulmumimo_total_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->be_ulmumimo_trigger));
	len += scnprintf(buf + len, buf_len - len, "be_bsr_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->be_bsr_trigger));
	len += scnprintf(buf + len, buf_len - len, "be_mu_bar_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->be_mu_bar_trigger));
	len += scnprintf(buf + len, buf_len - len, "be_mu_rts_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->be_mu_rts_trigger));
	len += print_array_to_buf(buf, len, "combined_be_bsr_trigger_tried",
			htt_stats_buf->combined_be_bsr_trigger_tried,
				ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "combined_be_bsr_trigger_err",
			htt_stats_buf->combined_be_bsr_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "standalone_be_bsr_trigger_tried",
			htt_stats_buf->standalone_be_bsr_trigger_tried,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "standalone_be_bsr_trigger_err",
			htt_stats_buf->standalone_be_bsr_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_be_su_ulofdma_basic_trigger",
			htt_stats_buf->manual_be_su_ulofdma_basic_trigger,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_be_su_ulofdma_basic_trigger_err",
			htt_stats_buf->manual_be_su_ulofdma_basic_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_be_mu_ulofdma_basic_trigger",
			htt_stats_buf->manual_be_mu_ulofdma_basic_trigger,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_be_mu_ulofdma_basic_trigger_err",
			htt_stats_buf->manual_be_mu_ulofdma_basic_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "be_basic_trigger_per_ac",
			htt_stats_buf->be_basic_trigger_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "be_basic_trigger_errors_per_ac",
			htt_stats_buf->be_basic_trigger_errors_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "be_mu_bar_trigger_per_ac",
			htt_stats_buf->be_mu_bar_trigger_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "be_mu_bar_trigger_errors_per_ac",
			htt_stats_buf->be_mu_bar_trigger_errors_per_ac,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_bn_stats_tlv(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_bn_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			"HTT_TX_SELFGEN_BN_STATS_TLV:\n");

	len += scnprintf(buf + len, buf_len - len,
			"\nbn_basic_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->bn_basic_trigger));
	len += scnprintf(buf + len, buf_len - len, "bn_bsr_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->bn_bsr_trigger));
	len += scnprintf(buf + len, buf_len - len, "bn_mu_bar_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->bn_mu_bar_trigger));
	len += scnprintf(buf + len, buf_len - len, "bn_mu_rts_trigger = %u\n",
			le32_to_cpu(htt_stats_buf->bn_mu_rts_trigger));
	len += print_array_to_buf(buf, len, "combined_bn_bsr_trigger_tried",
			htt_stats_buf->combined_bn_bsr_trigger_tried,
				ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "combined_bn_bsr_trigger_err",
			htt_stats_buf->combined_bn_bsr_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "standalone_bn_bsr_trigger_tried",
			htt_stats_buf->standalone_bn_bsr_trigger_tried,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "standalone_bn_bsr_trigger_err",
			htt_stats_buf->standalone_bn_bsr_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_bn_su_ulofdma_basic_trigger",
			htt_stats_buf->manual_bn_su_ulofdma_basic_trigger,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_bn_su_ulofdma_basic_trigger_err",
			htt_stats_buf->manual_bn_su_ulofdma_basic_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_bn_mu_ulofdma_basic_trigger",
			htt_stats_buf->manual_bn_mu_ulofdma_basic_trigger,
			ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "manual_bn_mu_ulofdma_basic_trigger_err",
			htt_stats_buf->manual_bn_mu_ulofdma_basic_trigger_err,
			ATH12K_HTT_NUM_AC_WMM, "\n");

	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_ac_err_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_ac_err_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_SELFGEN_AC_ERR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndp_err = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndp_err));
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndp_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndp_flushed));
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndpa_err = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndpa_err));
	len += scnprintf(buf + len, buf_len - len, "ac_su_ndpa_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ac_su_ndpa_flushed));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndpa_err = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndpa_err));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndpa_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndpa_flushed));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndp_err = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndp_err));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_ndp_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_ndp_flushed));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brp1_err = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brp1_err));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brp2_err = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brp2_err));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brp3_err = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brp3_err));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brp1_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brp1_flushed));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brp2_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brp2_flushed));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_brp3_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ac_mu_mimo_brp3_flushed));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_ax_err_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_ax_err_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_SELFGEN_AX_ERR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndp_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndp_err));
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndp_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndp_flushed));
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndpa_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndpa_err));
	len += scnprintf(buf + len, buf_len - len, "ax_su_ndpa_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ax_su_ndpa_flushed));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndpa_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndpa_err));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndpa_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndpa_flushed));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndp_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndp_err));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_ndp_flushed = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_mimo_ndp_flushed));

	len += print_array_to_buf_index(buf, len, "ax_mu_mimo_brpX_err", 1,
				htt_stats_buf->ax_mu_mimo_brp_err,
				ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1, "\n");

	len += print_array_to_buf_index(buf, len, "ax_mu_mimo_brpollX_flushed", 1,
				htt_stats_buf->ax_mu_mimo_brpoll_flushed,
				ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1, "\n");

	len += print_array_to_buf(buf, len, "ax_mu_mimo_num_cbf_rcvd_on_brp_err",
				htt_stats_buf->ax_mu_mimo_brp_err_num_cbf_received,
				ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS, "\n");

	len += print_array_to_buf_index(buf, len, "ax_ul_mumimo_trigger_err", 1,
				htt_stats_buf->ax_ul_mumimo_trigger_err,
				ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS, "\n");

	len += scnprintf(buf + len, buf_len - len, "ax_basic_trigger_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_basic_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "ax_ulmumimo_total_trigger_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_ulmumimo_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "ax_bsr_trigger_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_bsr_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_bar_trigger_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_bar_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_rts_trigger_err = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_rts_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "ax_basic_trigger_partial_resp = %u\n",
			le32_to_cpu(htt_stats_buf->ax_basic_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len, "ax_bsr_trigger_partial_resp = %u\n",
			le32_to_cpu(htt_stats_buf->ax_bsr_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len,
			"ax_mu_bar_trigger_partial_resp = %u\n",
			le32_to_cpu(htt_stats_buf->ax_mu_bar_trigger_partial_resp));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_be_err_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_be_err_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_SELFGEN_BE_ERR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "be_su_ndp_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_su_ndp_err));
	len += scnprintf(buf + len, buf_len - len, "be_su_ndp_flushed = %u\n",
			 le32_to_cpu(htt_stats_buf->be_su_ndp_flushed));
	len += scnprintf(buf + len, buf_len - len, "be_su_ndpa_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_su_ndpa_err));
	len += scnprintf(buf + len, buf_len - len, "be_su_ndpa_flushed = %u\n",
			 le32_to_cpu(htt_stats_buf->be_su_ndpa_flushed));
	len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_ndpa_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_mimo_ndpa_err));
	len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_ndpa_flushed = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_mimo_ndpa_flushed));
	len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_ndp_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_mimo_ndp_err));
	len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_ndp_flushed = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_mimo_ndp_flushed));
	len += print_array_to_buf_index(buf, len, "be_mu_mimo_brpX_err", 1,
					htt_stats_buf->be_mu_mimo_brp_err,
					ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1,
					"\n");
	len += print_array_to_buf_index(buf, len, "be_mu_mimo_brpollX_flushed", 1,
					htt_stats_buf->be_mu_mimo_brpoll_flushed,
					ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1,
					"\n");
	len += print_array_to_buf(buf, len, "be_mu_mimo_num_cbf_rcvd_on_brp_err",
				  htt_stats_buf->be_mu_mimo_brp_err_num_cbf_rxd,
				  ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS, "\n");
	len += print_array_to_buf(buf, len, "be_ul_mumimo_trigger_err",
				  htt_stats_buf->be_ul_mumimo_trigger_err,
				  ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS, "\n");
	len += scnprintf(buf + len, buf_len - len, "be_basic_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_basic_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "be_ulmumimo_total_trig_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_ulmumimo_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "be_bsr_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_bsr_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "be_mu_bar_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_bar_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "be_mu_rts_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_rts_trigger_err));
	len += scnprintf(buf + len, buf_len - len,
			 "be_basic_trigger_partial_resp = %u\n",
			 le32_to_cpu(htt_stats_buf->be_basic_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len, "be_bsr_trigger_partial_resp = %u\n",
			 le32_to_cpu(htt_stats_buf->be_bsr_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len,
			 "be_mu_bar_trigger_partial_resp = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_bar_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len, "be_mu_rts_trigger_blocked = %u\n",
			 le32_to_cpu(htt_stats_buf->be_mu_rts_trigger_blocked));
	len += scnprintf(buf + len, buf_len - len, "be_bsr_trigger_blocked = %u\n\n",
			 le32_to_cpu(htt_stats_buf->be_bsr_trigger_blocked));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_bn_err_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_selfgen_bn_err_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_SELFGEN_BN_ERR_STATS_TLV:\n");

	len += scnprintf(buf + len, buf_len - len, "bn_basic_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_basic_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "bn_bsr_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_bsr_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "bn_mu_bar_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_mu_bar_trigger_err));
	len += scnprintf(buf + len, buf_len - len, "bn_mu_rts_trigger_err = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_mu_rts_trigger_err));
	len += scnprintf(buf + len, buf_len - len,
			 "bn_basic_trigger_partial_resp = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_basic_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len, "bn_bsr_trigger_partial_resp = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_bsr_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len,
			 "bn_mu_bar_trigger_partial_resp = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_mu_bar_trigger_partial_resp));
	len += scnprintf(buf + len, buf_len - len, "bn_mu_rts_trigger_blocked = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_mu_rts_trigger_blocked));
	len += scnprintf(buf + len, buf_len - len, "bn_bsr_trigger_blocked = %u\n\n",
			 le32_to_cpu(htt_stats_buf->bn_bsr_trigger_blocked));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_ac_sched_status_stats_tlv(const void *tag_buf, u16 tag_len,
						      struct debug_htt_stats_req *stats)
{
	const struct ath12k_htt_tx_selfgen_ac_sched_status_stats_tlv *htt_stats_buf =
		     tag_buf;
	u8 *buf = stats->buf;
	u32 len = stats->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_SELFGEN_AC_SCHED_STATUS_STATS_TLV:\n");
	len += print_array_to_buf(buf, len, "ac_su_ndpa_sch_status",
				  htt_stats_buf->ac_su_ndpa_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ac_su_ndp_sch_status",
				  htt_stats_buf->ac_su_ndp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_ndpa_sch_status",
				  htt_stats_buf->ac_mu_mimo_ndpa_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_ndp_sch_status",
				  htt_stats_buf->ac_mu_mimo_ndp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_brp_sch_status",
				  htt_stats_buf->ac_mu_mimo_brp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ac_su_ndp_sch_flag_err",
				  htt_stats_buf->ac_su_ndp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_ndp_sch_flag_err",
				  htt_stats_buf->ac_mu_mimo_ndp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_brp_sch_flag_err",
				  htt_stats_buf->ac_mu_mimo_brp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n\n");

	stats->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_ax_sched_status_stats_tlv(const void *tag_buf, u16 tag_len,
						      struct debug_htt_stats_req *stats)
{
	const struct ath12k_htt_tx_selfgen_ax_sched_status_stats_tlv *htt_stats_buf =
		     tag_buf;
	u8 *buf = stats->buf;
	u32 len = stats->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_SELFGEN_AX_SCHED_STATUS_STATS_TLV:\n");
	len += print_array_to_buf(buf, len, "ax_su_ndpa_sch_status",
				  htt_stats_buf->ax_su_ndpa_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_su_ndp_sch_status",
				  htt_stats_buf->ax_su_ndp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_mimo_ndpa_sch_status",
				  htt_stats_buf->ax_mu_mimo_ndpa_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_mimo_ndp_sch_status",
				  htt_stats_buf->ax_mu_mimo_ndp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_brp_sch_status",
				  htt_stats_buf->ax_mu_brp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_bar_sch_status",
				  htt_stats_buf->ax_mu_bar_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_basic_trig_sch_status",
				  htt_stats_buf->ax_basic_trig_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_su_ndp_sch_flag_err",
				  htt_stats_buf->ax_su_ndp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_mimo_ndp_sch_flag_err",
				  htt_stats_buf->ax_mu_mimo_ndp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_brp_sch_flag_err",
				  htt_stats_buf->ax_mu_brp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_bar_sch_flag_err",
				  htt_stats_buf->ax_mu_bar_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_basic_trig_sch_flag_err",
				  htt_stats_buf->ax_basic_trig_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "ax_ulmumimo_trig_sch_status",
				  htt_stats_buf->ax_ulmumimo_trig_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "ax_ulmumimo_trig_sch_flag_err",
				  htt_stats_buf->ax_ulmumimo_trig_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n\n");

	stats->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_be_sched_status_stats_tlv(const void *tag_buf, u16 tag_len,
						      struct debug_htt_stats_req *stats)
{
	const struct ath12k_htt_tx_selfgen_be_sched_status_stats_tlv *htt_stats_buf =
		     tag_buf;
	u8 *buf = stats->buf;
	u32 len = stats->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_SELFGEN_BE_SCHED_STATUS_STATS_TLV:\n");
	len += print_array_to_buf(buf, len, "be_su_ndpa_sch_status",
				  htt_stats_buf->be_su_ndpa_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_su_ndp_sch_status",
				  htt_stats_buf->be_su_ndp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_mimo_ndpa_sch_status",
				  htt_stats_buf->be_mu_mimo_ndpa_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_mimo_ndp_sch_status",
				  htt_stats_buf->be_mu_mimo_ndp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_brp_sch_status",
				  htt_stats_buf->be_mu_brp_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_bar_sch_status",
				  htt_stats_buf->be_mu_bar_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_basic_trig_sch_status",
				  htt_stats_buf->be_basic_trig_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_su_ndp_sch_flag_err",
				  htt_stats_buf->be_su_ndp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_mimo_ndp_sch_flag_err",
				  htt_stats_buf->be_mu_mimo_ndp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_brp_sch_flag_err",
				  htt_stats_buf->be_mu_brp_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_bar_sch_flag_err",
				  htt_stats_buf->be_mu_bar_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "be_basic_trig_sch_flag_err",
				  htt_stats_buf->be_basic_trig_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "be_basic_trig_sch_flag_err",
				  htt_stats_buf->be_basic_trig_sch_flag_err,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "be_ulmumimo_trig_sch_flag_err",
				  htt_stats_buf->be_ulmumimo_trig_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n\n");

	stats->buf_len = len;
}

static void
ath12k_htt_print_tx_selfgen_bn_sched_status_stats_tlv(const void *tag_buf, u16 tag_len,
						      struct debug_htt_stats_req *stats)
{
	const struct ath12k_htt_tx_selfgen_bn_sched_status_stats_tlv *htt_stats_buf =
		     tag_buf;
	u8 *buf = stats->buf;
	u32 len = stats->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_SELFGEN_BN_SCHED_STATUS_STATS_TLV:\n");

	len += print_array_to_buf(buf, len, "bn_mu_bar_sch_status",
				  htt_stats_buf->bn_mu_bar_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "bn_basic_trig_sch_status",
				  htt_stats_buf->bn_basic_trig_sch_status,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS, "\n");
	len += print_array_to_buf(buf, len, "bn_mu_bar_sch_flag_err",
				  htt_stats_buf->bn_mu_bar_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");
	len += print_array_to_buf(buf, len, "bn_basic_trig_sch_flag_err",
				  htt_stats_buf->bn_basic_trig_sch_flag_err,
				  ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS, "\n");

	stats->buf_len = len;
}

static void
ath12k_htt_print_stats_string_tlv(const void *tag_buf, u16 tag_len,
				  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_string_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;
	u16 index = 0;
	u32 datum;
	char data[ATH12K_HTT_MAX_STRING_LEN] = {0};

	tag_len = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_STRING_TLV:\n");
	for (i = 0; i < tag_len; i++) {
		datum = __le32_to_cpu(htt_stats_buf->data[i]);
		index += scnprintf(&data[index], ATH12K_HTT_MAX_STRING_LEN - index,
				   "%.*s", 4, (char *)&datum);
		if (index >= ATH12K_HTT_MAX_STRING_LEN)
			break;
	}
	len += scnprintf(buf + len, buf_len - len, "data = %s\n\n", data);

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sring_stats_tlv(const void *tag_buf, u16 tag_len,
				 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sring_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;
	u32 avail_words;
	u32 head_tail_ptr;
	u32 sring_stat;
	u32 tail_ptr;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__ring_id__arena__ep);
	avail_words = __le32_to_cpu(htt_stats_buf->num_avail_words__num_valid_words);
	head_tail_ptr = __le32_to_cpu(htt_stats_buf->head_ptr__tail_ptr);
	sring_stat = __le32_to_cpu(htt_stats_buf->consumer_empty__producer_full);
	tail_ptr = __le32_to_cpu(htt_stats_buf->prefetch_count__internal_tail_ptr);

	len += scnprintf(buf + len, buf_len - len, "HTT_SRING_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_SRING_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "ring_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_SRING_STATS_RING_ID));
	len += scnprintf(buf + len, buf_len - len, "arena = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_SRING_STATS_ARENA));
	len += scnprintf(buf + len, buf_len - len, "ep = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_SRING_STATS_EP));
	len += scnprintf(buf + len, buf_len - len, "base_addr_lsb = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->base_addr_lsb));
	len += scnprintf(buf + len, buf_len - len, "base_addr_msb = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->base_addr_msb));
	len += scnprintf(buf + len, buf_len - len, "ring_size = %u\n",
			 le32_to_cpu(htt_stats_buf->ring_size));
	len += scnprintf(buf + len, buf_len - len, "elem_size = %u\n",
			 le32_to_cpu(htt_stats_buf->elem_size));
	len += scnprintf(buf + len, buf_len - len, "num_avail_words = %u\n",
			 u32_get_bits(avail_words,
				      ATH12K_HTT_SRING_STATS_NUM_AVAIL_WORDS));
	len += scnprintf(buf + len, buf_len - len, "num_valid_words = %u\n",
			 u32_get_bits(avail_words,
				      ATH12K_HTT_SRING_STATS_NUM_VALID_WORDS));
	len += scnprintf(buf + len, buf_len - len, "head_ptr = %u\n",
			 u32_get_bits(head_tail_ptr, ATH12K_HTT_SRING_STATS_HEAD_PTR));
	len += scnprintf(buf + len, buf_len - len, "tail_ptr = %u\n",
			 u32_get_bits(head_tail_ptr, ATH12K_HTT_SRING_STATS_TAIL_PTR));
	len += scnprintf(buf + len, buf_len - len, "consumer_empty = %u\n",
			 u32_get_bits(sring_stat,
				      ATH12K_HTT_SRING_STATS_CONSUMER_EMPTY));
	len += scnprintf(buf + len, buf_len - len, "producer_full = %u\n",
			 u32_get_bits(head_tail_ptr,
				      ATH12K_HTT_SRING_STATS_PRODUCER_FULL));
	len += scnprintf(buf + len, buf_len - len, "prefetch_count = %u\n",
			 u32_get_bits(tail_ptr, ATH12K_HTT_SRING_STATS_PREFETCH_COUNT));
	len += scnprintf(buf + len, buf_len - len, "internal_tail_ptr = %u\n\n",
			 u32_get_bits(tail_ptr,
				      ATH12K_HTT_SRING_STATS_INTERNAL_TAIL_PTR));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sfm_cmn_tlv(const void *tag_buf, u16 tag_len,
			     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sfm_cmn_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_SFM_CMN_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "buf_total = %u\n",
			 le32_to_cpu(htt_stats_buf->buf_total));
	len += scnprintf(buf + len, buf_len - len, "mem_empty = %u\n",
			 le32_to_cpu(htt_stats_buf->mem_empty));
	len += scnprintf(buf + len, buf_len - len, "deallocate_bufs = %u\n",
			 le32_to_cpu(htt_stats_buf->deallocate_bufs));
	len += scnprintf(buf + len, buf_len - len, "num_records = %u\n\n",
			 le32_to_cpu(htt_stats_buf->num_records));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sfm_client_tlv(const void *tag_buf, u16 tag_len,
				struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sfm_client_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_SFM_CLIENT_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "client_id = %u\n",
			 le32_to_cpu(htt_stats_buf->client_id));
	len += scnprintf(buf + len, buf_len - len, "buf_min = %u\n",
			 le32_to_cpu(htt_stats_buf->buf_min));
	len += scnprintf(buf + len, buf_len - len, "buf_max = %u\n",
			 le32_to_cpu(htt_stats_buf->buf_max));
	len += scnprintf(buf + len, buf_len - len, "buf_busy = %u\n",
			 le32_to_cpu(htt_stats_buf->buf_busy));
	len += scnprintf(buf + len, buf_len - len, "buf_alloc = %u\n",
			 le32_to_cpu(htt_stats_buf->buf_alloc));
	len += scnprintf(buf + len, buf_len - len, "buf_avail = %u\n",
			 le32_to_cpu(htt_stats_buf->buf_avail));
	len += scnprintf(buf + len, buf_len - len, "num_users = %u\n\n",
			 le32_to_cpu(htt_stats_buf->num_users));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_sfm_client_user_tlv(const void *tag_buf, u16 tag_len,
				     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_sfm_client_user_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u16 num_elems = tag_len >> 2;

	len += scnprintf(buf + len, buf_len - len, "HTT_SFM_CLIENT_USER_TLV:\n");
	len += print_array_to_buf(buf, len, "dwords_used_by_user_n",
				  htt_stats_buf->dwords_used_by_user_n,
				  num_elems, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_mu_mimo_sch_stats_tlv(const void *tag_buf, u16 tag_len,
					       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_mu_mimo_sch_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_MU_MIMO_SCH_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_sch_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_mimo_sch_posted));
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_sch_failed = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_mimo_sch_failed));
	len += scnprintf(buf + len, buf_len - len, "mu_mimo_ppdu_posted = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_mimo_ppdu_posted));
	len += scnprintf(buf + len, buf_len - len,
			 "\nac_mu_mimo_sch_posted_per_group_index %u (SU) = %u\n", 0,
			 le32_to_cpu(htt_stats_buf->ac_mu_mimo_per_grp_sz[0]));
	for (i = 1; i < ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ac_mu_mimo_sch_posted_per_group_index %u ", i);
		len += scnprintf(buf + len, buf_len - len,
				 "(TOTAL STREAMS = %u) = %u\n", i + 1,
				 le32_to_cpu(htt_stats_buf->ac_mu_mimo_per_grp_sz[i]));
	}

	for (i = 0; i < ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ac_mu_mimo_sch_posted_per_group_index %u ",
				 i + ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS);
		len += scnprintf(buf + len, buf_len - len,
				 "(TOTAL STREAMS = %u) = %u\n",
				 i + ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS + 1,
				 le32_to_cpu(htt_stats_buf->ac_mu_mimo_grp_sz_ext[i]));
	}

	len += scnprintf(buf + len, buf_len - len,
			 "\nax_mu_mimo_sch_posted_per_group_index %u (SU) = %u\n", 0,
			 le32_to_cpu(htt_stats_buf->ax_mu_mimo_per_grp_sz[0]));
	for (i = 1; i < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ax_mu_mimo_sch_posted_per_group_index %u ", i);
		len += scnprintf(buf + len, buf_len - len,
				 "(TOTAL STREAMS = %u) = %u\n", i + 1,
				 le32_to_cpu(htt_stats_buf->ax_mu_mimo_per_grp_sz[i]));
	}

	len += scnprintf(buf + len, buf_len - len,
			"\nbe_mu_mimo_sch_posted_per_group_index %u (SU) = %u\n", 0,
			le32_to_cpu(htt_stats_buf->be_mu_mimo_per_grp_sz[0]));
	for (i = 1; i < ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_mu_mimo_sch_posted_per_group_index %u ", i);
		len += scnprintf(buf + len, buf_len - len,
				 "(TOTAL STREAMS = %u) = %u\n", i + 1,
				 le32_to_cpu(htt_stats_buf->be_mu_mimo_per_grp_sz[i]));
	}

	len += scnprintf(buf + len, buf_len - len, "\n11ac MU_MIMO SCH STATS:\n");
	for (i = 0; i < ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_sch_nusers_");
		len += scnprintf(buf + len, buf_len - len, "%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->ac_mu_mimo_sch_nusers[i]));
	}

	len += scnprintf(buf + len, buf_len - len, "\n11ax MU_MIMO SCH STATS:\n");
	for (i = 0; i < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_sch_nusers_");
		len += scnprintf(buf + len, buf_len - len, "%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->ax_mu_mimo_sch_nusers[i]));
	}

	len += scnprintf(buf + len, buf_len - len, "\n11be MU_MIMO SCH STATS:\n");
	for (i = 0; i < ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_sch_nusers_");
		len += scnprintf(buf + len, buf_len - len, "%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->be_mu_mimo_sch_nusers[i]));
	}

	len += scnprintf(buf + len, buf_len - len, "\n11ax OFDMA SCH STATS:\n");
	for (i = 0; i < ATH12K_HTT_TX_NUM_OFDMA_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ax_ofdma_sch_nusers_%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->ax_ofdma_sch_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_ul_ofdma_basic_sch_nusers_%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->ax_ul_ofdma_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_ul_ofdma_bsr_sch_nusers_%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->ax_ul_ofdma_bsr_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_ul_ofdma_bar_sch_nusers_%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->ax_ul_ofdma_bar_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_ul_ofdma_brp_sch_nusers_%u = %u\n\n", i,
				 le32_to_cpu(htt_stats_buf->ax_ul_ofdma_brp_nusers[i]));
	}

	len += scnprintf(buf + len, buf_len - len, "11ax UL MUMIMO SCH STATS:\n");
	for (i = 0; i < ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ax_ul_mumimo_basic_sch_nusers_%u = %u\n", i,
				 le32_to_cpu(htt_stats_buf->ax_ul_mumimo_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_ul_mumimo_brp_sch_nusers_%u = %u\n\n", i,
				 le32_to_cpu(htt_stats_buf->ax_ul_mumimo_brp_nusers[i]));
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_mumimo_grp_stats_tlv(const void *tag_buf, u16 tag_len,
					      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_mumimo_grp_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int i, j, index;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_MUMIMO_GRP_STATS:\n");
	len += print_array_to_buf(buf, len, "dl_mumimo_grp_best_grp_size",
				  htt_stats_buf->dl_mumimo_grp_best_grp_size,
				  ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += print_array_to_buf_index(buf, len, "dl_mumimo_grp_best_num_usrs ", 1,
				  htt_stats_buf->dl_mumimo_grp_best_num_usrs,
				  ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS, "\n");
	len += print_array_to_buf(buf, len,
				  "dl_mumimo_grp_tputs_observed (per bin = 300 mbps)",
				  htt_stats_buf->dl_mumimo_grp_tputs,
				  ATH12K_HTT_STATS_MUMIMO_TPUT_NUM_BINS, "\n");
	len += print_array_to_buf(buf, len, "dl_mumimo_grp eligible",
				  htt_stats_buf->dl_mumimo_grp_eligible,
				  ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += print_array_to_buf(buf, len, "dl_mumimo_grp_ineligible",
				  htt_stats_buf->dl_mumimo_grp_ineligible,
				  ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += scnprintf(buf + len, buf_len - len, "dl_mumimo_grp_invalid:\n");
	for (i = 0; i < ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ; i++) {
		len += scnprintf(buf + len, buf_len - len, "grp_id = %u", i);
		index = 0;
		for (j = 0; j < ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE; j++) {
			index += scnprintf(buf + len + index,
				 (buf_len - len) - index,
				 " %u:%u,", j, le32_to_cpu
				 (htt_stats_buf->dl_mumimo_grp_invalid
				 [i * ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE + j]));
		}
		index--;
		*(buf + len + index) = '\0';
		len += index;
		len += scnprintf(buf + len, buf_len - len, "\n");
	}
	len += print_array_to_buf(buf, len, "ul_mumimo_grp_best_grp_size",
				  htt_stats_buf->ul_mumimo_grp_best_grp_size,
				  ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += print_array_to_buf_index(buf, len, "ul_mumimo_grp_best_num_usrs ", 1,
				  htt_stats_buf->ul_mumimo_grp_best_usrs,
				  ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS, "\n");
	len += print_array_to_buf(buf, len,
				  "ul_mumimo_grp_tputs_observed (per bin = 300 mbps)",
				  htt_stats_buf->ul_mumimo_grp_tputs,
				  ATH12K_HTT_STATS_MUMIMO_TPUT_NUM_BINS, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_mu_mimo_mpdu_stats_tlv(const void *tag_buf, u16 tag_len,
						struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_mpdu_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 user_index;
	u32 tx_sched_mode;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	user_index = __le32_to_cpu(htt_stats_buf->user_index);
	tx_sched_mode = __le32_to_cpu(htt_stats_buf->tx_sched_mode);

	if (tx_sched_mode == ATH12K_HTT_STATS_TX_SCHED_MODE_MU_MIMO_AC) {
		if (!user_index)
			len += scnprintf(buf + len, buf_len - len,
					 "HTT_TX_PDEV_MU_MIMO_AC_MPDU_STATS:\n");

		if (user_index < ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS) {
			len += scnprintf(buf + len, buf_len - len,
					 "ac_mu_mimo_mpdus_queued_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_queued_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ac_mu_mimo_mpdus_tried_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_tried_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ac_mu_mimo_mpdus_failed_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_failed_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ac_mu_mimo_mpdus_requeued_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_requeued_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ac_mu_mimo_err_no_ba_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->err_no_ba_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ac_mu_mimo_mpdu_underrun_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdu_underrun_usr));
			len += scnprintf(buf + len, buf_len - len,
					"ac_mu_mimo_ampdu_underrun_usr_%u = %u\n\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->ampdu_underrun_usr));
		}
	}

	if (tx_sched_mode == ATH12K_HTT_STATS_TX_SCHED_MODE_MU_MIMO_AX) {
		if (!user_index)
			len += scnprintf(buf + len, buf_len - len,
					 "HTT_TX_PDEV_MU_MIMO_AX_MPDU_STATS:\n");

		if (user_index < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS) {
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_mimo_mpdus_queued_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_queued_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_mimo_mpdus_tried_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_tried_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_mimo_mpdus_failed_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_failed_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_mimo_mpdus_requeued_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_requeued_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_mimo_err_no_ba_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->err_no_ba_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_mimo_mpdu_underrun_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdu_underrun_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_mimo_ampdu_underrun_usr_%u = %u\n\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->ampdu_underrun_usr));
		}
	}

	if (tx_sched_mode == ATH12K_HTT_STATS_TX_SCHED_MODE_MU_MIMO_BE) {
		if (!user_index)
			len += scnprintf(buf + len, buf_len - len,
					"HTT_TX_PDEV_MU_MIMO_BE_MPDU_STATS:\n\n");

		if (user_index < ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS) {
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_mimo_mpdus_queued_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_queued_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_mimo_mpdus_tried_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_tried_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_mimo_mpdus_failed_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_failed_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_mimo_mpdus_requeued_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_requeued_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_mimo_err_no_ba_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->err_no_ba_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_mimo_mpdu_underrun_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdu_underrun_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_mimo_ampdu_underrun_usr_%u = %u\n\n",
					user_index,
					le32_to_cpu(htt_stats_buf->ampdu_underrun_usr));
		}
	}

	if (tx_sched_mode == ATH12K_HTT_STATS_TX_SCHED_MODE_MU_OFDMA_AX) {
		if (!user_index)
			len += scnprintf(buf + len, buf_len - len,
					 "HTT_TX_PDEV_AX_MU_OFDMA_MPDU_STATS:\n");

		if (user_index < ATH12K_HTT_TX_NUM_OFDMA_USER_STATS) {
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_ofdma_mpdus_queued_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_queued_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_ofdma_mpdus_tried_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_tried_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_ofdma_mpdus_failed_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_failed_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_ofdma_mpdus_requeued_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdus_requeued_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_ofdma_err_no_ba_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->err_no_ba_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_ofdma_mpdu_underrun_usr_%u = %u\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->mpdu_underrun_usr));
			len += scnprintf(buf + len, buf_len - len,
					 "ax_mu_ofdma_ampdu_underrun_usr_%u = %u\n\n",
					 user_index,
					 le32_to_cpu(htt_stats_buf->ampdu_underrun_usr));
		}
	}

	if (tx_sched_mode == ATH12K_HTT_STATS_TX_SCHED_MODE_MU_OFDMA_BE) {
		if (!user_index)
			len += scnprintf(buf + len, buf_len - len,
					"HTT_TX_PDEV_BE_MU_OFDMA_MPDU_STATS:\n\n");

		if (user_index < ATH12K_HTT_TX_NUM_OFDMA_USER_STATS) {
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_ofdma_mpdus_queued_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_queued_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_ofdma_mpdus_tried_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_tried_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_ofdma_mpdus_failed_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_failed_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_ofdma_mpdus_requeued_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_requeued_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_ofdma_err_no_ba_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->err_no_ba_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_ofdma_mpdu_underrun_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdu_underrun_usr));
			len += scnprintf(buf + len, buf_len - len,
					"be_mu_ofdma_ampdu_underrun_usr_%u = %u\n\n",
					user_index,
					le32_to_cpu(htt_stats_buf->ampdu_underrun_usr));
		}
	}

	if (tx_sched_mode == ATH12K_HTT_STATS_TX_SCHED_MODE_MU_OFDMA_BN) {
		if (!user_index)
			len += scnprintf(buf + len, buf_len - len,
					"HTT_TX_PDEV_BN_MU_OFDMA_MPDU_STATS:\n\n");

		if (user_index < ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS) {
			len += scnprintf(buf + len, buf_len - len,
					"bn_mu_ofdma_mpdus_queued_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_queued_usr));
			len += scnprintf(buf + len, buf_len - len,
					"bn_mu_ofdma_mpdus_tried_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_tried_usr));
			len += scnprintf(buf + len, buf_len - len,
					"bn_mu_ofdma_mpdus_failed_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_failed_usr));
			len += scnprintf(buf + len, buf_len - len,
					"bn_mu_ofdma_mpdus_requeued_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdus_requeued_usr));
			len += scnprintf(buf + len, buf_len - len,
					"bn_mu_ofdma_err_no_ba_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->err_no_ba_usr));
			len += scnprintf(buf + len, buf_len - len,
					"bn_mu_ofdma_mpdu_underrun_usr_%u = %u\n",
					user_index,
					le32_to_cpu(htt_stats_buf->mpdu_underrun_usr));
			len += scnprintf(buf + len, buf_len - len,
					"bn_mu_ofdma_ampdu_underrun_usr_%u = %u\n\n",
					user_index,
					le32_to_cpu(htt_stats_buf->ampdu_underrun_usr));
		}
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_cca_stats_hist_tlv(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_cca_stats_hist_v1_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_CCA_STATS_HIST_TLV :\n");
	len += scnprintf(buf + len, buf_len - len, "chan_num = %u\n",
			 le32_to_cpu(htt_stats_buf->chan_num));
	len += scnprintf(buf + len, buf_len - len, "num_records = %u\n",
			 le32_to_cpu(htt_stats_buf->num_records));
	len += scnprintf(buf + len, buf_len - len, "valid_cca_counters_bitmap = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->valid_cca_counters_bitmap));
	len += scnprintf(buf + len, buf_len - len, "collection_interval = %u\n\n",
			 le32_to_cpu(htt_stats_buf->collection_interval));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_stats_cca_counters_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_stats_cca_counters_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_PDEV_STATS_CCA_COUNTERS_TLV:(in usec)\n");
	len += scnprintf(buf + len, buf_len - len, "tx_frame_usec = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_frame_usec));
	len += scnprintf(buf + len, buf_len - len, "rx_frame_usec = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_frame_usec));
	len += scnprintf(buf + len, buf_len - len, "rx_clear_usec = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_clear_usec));
	len += scnprintf(buf + len, buf_len - len, "my_rx_frame_usec = %u\n",
			 le32_to_cpu(htt_stats_buf->my_rx_frame_usec));
	len += scnprintf(buf + len, buf_len - len, "usec_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->usec_cnt));
	len += scnprintf(buf + len, buf_len - len, "med_rx_idle_usec = %u\n",
			 le32_to_cpu(htt_stats_buf->med_rx_idle_usec));
	len += scnprintf(buf + len, buf_len - len, "med_tx_idle_global_usec = %u\n",
			 le32_to_cpu(htt_stats_buf->med_tx_idle_global_usec));
	len += scnprintf(buf + len, buf_len - len, "cca_obss_usec = %u\n",
			 le32_to_cpu(htt_stats_buf->cca_obss_usec));
	len += scnprintf(buf + len, buf_len - len, "pre_rx_frame_usec = %u\n\n",
			 le32_to_cpu(htt_stats_buf->pre_rx_frame_usec));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_sounding_stats_tlv(const void *tag_buf, u16 tag_len,
				       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_sounding_stats_tlv *htt_stats_buf = tag_buf;
	const __le32 *cbf_20, *cbf_40, *cbf_80, *cbf_160, *cbf_320;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 tx_sounding_mode;
	u8 i, u;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	cbf_20 = htt_stats_buf->cbf_20;
	cbf_40 = htt_stats_buf->cbf_40;
	cbf_80 = htt_stats_buf->cbf_80;
	cbf_160 = htt_stats_buf->cbf_160;
	cbf_320 = htt_stats_buf->cbf_320;
	tx_sounding_mode = le32_to_cpu(htt_stats_buf->tx_sounding_mode);

	if (tx_sounding_mode == ATH12K_HTT_TX_AC_SOUNDING_MODE) {
		len += scnprintf(buf + len, buf_len - len,
				 "HTT_TX_AC_SOUNDING_STATS_TLV:\n");
		len += scnprintf(buf + len, buf_len - len,
				 "ac_cbf_20 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_20[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "ac_cbf_40 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_40[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "ac_cbf_80 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_80[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "ac_cbf_160 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_160[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));

		for (u = 0, i = 0; u < ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS; u++) {
			len += scnprintf(buf + len, buf_len - len,
					 "Sounding User_%u = 20MHz: %u, ", u,
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "40MHz: %u, ",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "80MHz: %u, ",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "160MHz: %u\n",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
		}
	} else if (tx_sounding_mode == ATH12K_HTT_TX_AX_SOUNDING_MODE) {
		len += scnprintf(buf + len, buf_len - len,
				 "\nHTT_TX_AX_SOUNDING_STATS_TLV:\n");
		len += scnprintf(buf + len, buf_len - len,
				 "ax_cbf_20 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_20[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_cbf_40 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_40[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_cbf_80 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_80[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "ax_cbf_160 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_160[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));

		for (u = 0, i = 0; u < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS; u++) {
			len += scnprintf(buf + len, buf_len - len,
					 "Sounding User_%u = 20MHz: %u, ", u,
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "40MHz: %u, ",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "80MHz: %u, ",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "160MHz: %u\n",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
		}
	} else if (tx_sounding_mode == ATH12K_HTT_TX_BE_SOUNDING_MODE) {
		len += scnprintf(buf + len, buf_len - len,
				 "\nHTT_TX_BE_SOUNDING_STATS_TLV:\n");
		len += scnprintf(buf + len, buf_len - len,
				 "be_cbf_20 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_20[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_20[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "be_cbf_40 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_40[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_40[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "be_cbf_80 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_80[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_80[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "be_cbf_160 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_160[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_160[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len,
				 "be_cbf_320 = IBF: %u, SU_SIFS: %u, SU_RBO: %u, ",
				 le32_to_cpu(cbf_320[ATH12K_HTT_IMPL_STEER_STATS]),
				 le32_to_cpu(cbf_320[ATH12K_HTT_EXPL_SUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_320[ATH12K_HTT_EXPL_SURBO_STEER_STATS]));
		len += scnprintf(buf + len, buf_len - len, "MU_SIFS: %u, MU_RBO: %u\n",
				 le32_to_cpu(cbf_320[ATH12K_HTT_EXPL_MUSIFS_STEER_STATS]),
				 le32_to_cpu(cbf_320[ATH12K_HTT_EXPL_MURBO_STEER_STATS]));
		for (u = 0, i = 0; u < ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS; u++) {
			len += scnprintf(buf + len, buf_len - len,
					 "Sounding User_%u = 20MHz: %u, ", u,
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "40MHz: %u, ",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len, "80MHz: %u, ",
					 le32_to_cpu(htt_stats_buf->sounding[i++]));
			len += scnprintf(buf + len, buf_len - len,
					 "160MHz: %u, 320MHz: %u\n",
					 le32_to_cpu(htt_stats_buf->sounding[i++]),
					 le32_to_cpu(htt_stats_buf->sounding_320[u]));
		}
	} else if (tx_sounding_mode == ATH12K_HTT_TX_CMN_SOUNDING_MODE) {
		len += scnprintf(buf + len, buf_len - len,
				 "\nCV UPLOAD HANDLER STATS:\n");
		len += scnprintf(buf + len, buf_len - len, "cv_nc_mismatch_err = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_nc_mismatch_err));
		len += scnprintf(buf + len, buf_len - len, "cv_fcs_err = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_fcs_err));
		len += scnprintf(buf + len, buf_len - len, "cv_frag_idx_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_frag_idx_mismatch));
		len += scnprintf(buf + len, buf_len - len, "cv_invalid_peer_id = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_invalid_peer_id));
		len += scnprintf(buf + len, buf_len - len, "cv_no_txbf_setup = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_no_txbf_setup));
		len += scnprintf(buf + len, buf_len - len, "cv_expiry_in_update = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_expiry_in_update));
		len += scnprintf(buf + len, buf_len - len, "cv_pkt_bw_exceed = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_pkt_bw_exceed));
		len += scnprintf(buf + len, buf_len - len, "cv_dma_not_done_err = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_dma_not_done_err));
		len += scnprintf(buf + len, buf_len - len, "cv_update_failed = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_update_failed));
		len += scnprintf(buf + len, buf_len - len, "cv_dma_timeout_error = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_dma_timeout_error));
		len += scnprintf(buf + len, buf_len - len, "cv_buf_ibf_uploads = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_buf_ibf_uploads));
		len += scnprintf(buf + len, buf_len - len, "cv_buf_ebf_uploads = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_buf_ebf_uploads));
		len += scnprintf(buf + len, buf_len - len, "cv_buf_received = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_buf_received));
		len += scnprintf(buf + len, buf_len - len, "cv_buf_fed_back = %u\n\n",
				 le32_to_cpu(htt_stats_buf->cv_buf_fed_back));

		len += scnprintf(buf + len, buf_len - len, "CV QUERY STATS:\n");
		len += scnprintf(buf + len, buf_len - len, "cv_total_query = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_total_query));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_total_pattern_query = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_total_pattern_query));
		len += scnprintf(buf + len, buf_len - len, "cv_total_bw_query = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_total_bw_query));
		len += scnprintf(buf + len, buf_len - len, "cv_invalid_bw_coding = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_invalid_bw_coding));
		len += scnprintf(buf + len, buf_len - len, "cv_forced_sounding = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_forced_sounding));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_standalone_sounding = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_standalone_sounding));
		len += scnprintf(buf + len, buf_len - len, "cv_nc_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_nc_mismatch));
		len += scnprintf(buf + len, buf_len - len, "cv_fb_type_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_fb_type_mismatch));
		len += scnprintf(buf + len, buf_len - len, "cv_ofdma_bw_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_ofdma_bw_mismatch));
		len += scnprintf(buf + len, buf_len - len, "cv_bw_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_bw_mismatch));
		len += scnprintf(buf + len, buf_len - len, "cv_pattern_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_pattern_mismatch));
		len += scnprintf(buf + len, buf_len - len, "cv_preamble_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_preamble_mismatch));
		len += scnprintf(buf + len, buf_len - len, "cv_nr_mismatch = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_nr_mismatch));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_in_use_cnt_exceeded = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_in_use_cnt_exceeded));
		len += scnprintf(buf + len, buf_len - len, "cv_ntbr_sounding = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_ntbr_sounding));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_found_upload_in_progress = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_found_upload_in_progress));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_expired_during_query = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_expired_during_query));
		len += scnprintf(buf + len, buf_len - len, "cv_found = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_found));
		len += scnprintf(buf + len, buf_len - len, "cv_not_found = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_not_found));
		len += scnprintf(buf + len, buf_len - len, "cv_total_query_ibf = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_total_query_ibf));
		len += scnprintf(buf + len, buf_len - len, "cv_found_ibf = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_found_ibf));
		len += scnprintf(buf + len, buf_len - len, "cv_not_found_ibf = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_not_found_ibf));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_expired_during_query_ibf = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_expired_during_query_ibf));
		len += scnprintf(buf + len, buf_len - len,
				 "adaptive_snd_total_query = %u\n",
				 le32_to_cpu(htt_stats_buf->adaptive_snd_total_query));
		len += print_array_to_buf(buf, len, "adaptive_snd_total_mcs_drop",
					  htt_stats_buf->adaptive_snd_total_mcs_drop,
					  ATH12K_HTT_TX_PDEV_STATS_TOTAL_MCS_COUNTERS,
					  "\n");
		len += scnprintf(buf + len, buf_len - len,
				 "adaptive_snd_kicked_in = %u\n",
				 le32_to_cpu(htt_stats_buf->adaptive_snd_kicked_in));
		len += scnprintf(buf + len, buf_len - len,
				 "adaptive_snd_back_to_default = %u\n",
				 le32_to_cpu(htt_stats_buf->adaptive_snd_back_to_def));
	} else if (tx_sounding_mode == ATH12K_HTT_TX_CV_CORR_MODE) {
		len += scnprintf(buf + len, buf_len - len,
				 "\nCV CORRELATION TRIGGER STATS:-\n");
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_trigger_online_mode = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_corr_trig_online_mode));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_trigger_offline_mode = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_corr_trig_offline_mode));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_trigger_hybrid_mode = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_corr_trig_hybrid_mode));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_trigger_computation_level_0 = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_corr_trig_comp_level_0));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_trigger_computation_level_1 = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_corr_trig_comp_level_1));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_trigger_computation_level_2 = %u\n",
				 le32_to_cpu(htt_stats_buf->cv_corr_trig_comp_level_2));
		len += print_array_to_buf_index(buf, len, "cv_corr_trigger_num_users", 1,
					  htt_stats_buf->cv_corr_trigger_num_users,
					  ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS, "\n");
		len += print_array_to_buf_index(buf, len, "cv_corr_trigger_num_streams",
					  1, htt_stats_buf->cv_corr_trigger_num_streams,
					  ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS, "\n");
		len += scnprintf(buf + len, buf_len - len,
				 "\n\nCV CORRELATION UPLOAD STATS:-\n");
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_total_buf_received = %u\n",
				 le32_to_cpu(htt_stats_buf->total_buf_received));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_total_buf_fed_back = %u\n",
				 le32_to_cpu(htt_stats_buf->total_buf_fed_back));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_total_processing_failed = %u\n",
				 le32_to_cpu(htt_stats_buf->total_processing_failed));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_failed_total_users_zero = %u\n",
				 le32_to_cpu(htt_stats_buf->total_users_zero));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_failed_total_users_exceeded = %u\n",
				 le32_to_cpu(htt_stats_buf->failed_tot_users_exceeded));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_failed_peer_not_found = %u\n",
				 le32_to_cpu(htt_stats_buf->failed_peer_not_found));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_user_nss_exceeded = %u\n",
				 le32_to_cpu(htt_stats_buf->user_nss_exceeded));
		len += scnprintf(buf + len, buf_len - len,
				 "cv_corr_upload_invalid_lookup_index = %u\n",
				 le32_to_cpu(htt_stats_buf->invalid_lookup_index));
		len += print_array_to_buf_index(buf, len,
					  "cv_corr_upload_total_num_users", 1,
					  htt_stats_buf->total_num_users,
					  ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS, "\n");
		len += print_array_to_buf_index(buf, len,
					  "cv_corr_upload_total_num_streams", 1,
					  htt_stats_buf->total_num_streams,
					  ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS, "\n");
		len += scnprintf(buf + len, buf_len - len,
				 "lookahead_sounding_dl_cnt = %u\n",
				 le32_to_cpu(htt_stats_buf->lookahead_sounding_dl_cnt));
		len += print_array_to_buf_index(buf, len, "lookahead_snd_dl_num_users", 1,
					  htt_stats_buf->lookahead_snd_dl_num_users,
					  ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS, "\n");
		len += scnprintf(buf + len, buf_len - len,
				 "lookahead_sounding_ul_cnt = %u\n",
				 le32_to_cpu(htt_stats_buf->lookahead_sounding_ul_cnt));
		len += print_array_to_buf_index(buf, len, "lookahead_snd_ul_num_users", 1,
					  htt_stats_buf->lookahead_snd_ul_num_users,
					  ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS, "\n\n");
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_obss_pd_stats_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_obss_pd_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;
	static const char *access_cat_names[ATH12K_HTT_NUM_AC_WMM] = {"best effort",
								      "background",
								      "video", "voice"};

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_OBSS_PD_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "num_spatial_reuse_tx = %u\n",
			 le32_to_cpu(htt_stats_buf->num_sr_tx_transmissions));
	len += scnprintf(buf + len, buf_len - len,
			 "num_spatial_reuse_opportunities = %u\n",
			 le32_to_cpu(htt_stats_buf->num_spatial_reuse_opportunities));
	len += scnprintf(buf + len, buf_len - len, "num_non_srg_opportunities = %u\n",
			 le32_to_cpu(htt_stats_buf->num_non_srg_opportunities));
	len += scnprintf(buf + len, buf_len - len, "num_non_srg_ppdu_tried = %u\n",
			 le32_to_cpu(htt_stats_buf->num_non_srg_ppdu_tried));
	len += scnprintf(buf + len, buf_len - len, "num_non_srg_ppdu_success = %u\n",
			 le32_to_cpu(htt_stats_buf->num_non_srg_ppdu_success));
	len += scnprintf(buf + len, buf_len - len, "num_srg_opportunities = %u\n",
			 le32_to_cpu(htt_stats_buf->num_srg_opportunities));
	len += scnprintf(buf + len, buf_len - len, "num_srg_ppdu_tried = %u\n",
			 le32_to_cpu(htt_stats_buf->num_srg_ppdu_tried));
	len += scnprintf(buf + len, buf_len - len, "num_srg_ppdu_success = %u\n",
			 le32_to_cpu(htt_stats_buf->num_srg_ppdu_success));
	len += scnprintf(buf + len, buf_len - len, "num_psr_opportunities = %u\n",
			 le32_to_cpu(htt_stats_buf->num_psr_opportunities));
	len += scnprintf(buf + len, buf_len - len, "num_psr_ppdu_tried = %u\n",
			 le32_to_cpu(htt_stats_buf->num_psr_ppdu_tried));
	len += scnprintf(buf + len, buf_len - len, "num_psr_ppdu_success = %u\n",
			 le32_to_cpu(htt_stats_buf->num_psr_ppdu_success));
	len += scnprintf(buf + len, buf_len - len, "min_duration_check_flush_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->num_obss_min_dur_check_flush_cnt));
	len += scnprintf(buf + len, buf_len - len, "sr_ppdu_abort_flush_cnt = %u\n\n",
			 le32_to_cpu(htt_stats_buf->num_sr_ppdu_abort_flush_cnt));

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_OBSS_PD_PER_AC_STATS:\n");
	for (i = 0; i < ATH12K_HTT_NUM_AC_WMM; i++) {
		len += scnprintf(buf + len, buf_len - len, "Access Category %u (%s)\n",
				 i, access_cat_names[i]);
		len += scnprintf(buf + len, buf_len - len,
				 "num_non_srg_ppdu_tried = %u\n",
				 le32_to_cpu(htt_stats_buf->num_non_srg_tried_per_ac[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "num_non_srg_ppdu_success = %u\n",
				 le32_to_cpu(htt_stats_buf->num_non_srg_success_ac[i]));
		len += scnprintf(buf + len, buf_len - len, "num_srg_ppdu_tried = %u\n",
				 le32_to_cpu(htt_stats_buf->num_srg_tried_per_ac[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "num_srg_ppdu_success = %u\n\n",
				 le32_to_cpu(htt_stats_buf->num_srg_success_per_ac[i]));
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_latency_prof_ctx_tlv(const void *tag_buf, u16 tag_len,
				      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_latency_prof_ctx_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_LATENCY_CTX_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "duration = %u\n",
			 le32_to_cpu(htt_stats_buf->duration));
	len += scnprintf(buf + len, buf_len - len, "tx_msdu_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "tx_mpdu_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_mpdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_msdu_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_mpdu_cnt = %u\n\n",
			 le32_to_cpu(htt_stats_buf->rx_mpdu_cnt));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_latency_prof_cnt(const void *tag_buf, u16 tag_len,
				  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_latency_prof_cnt_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_LATENCY_CNT_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "prof_enable_cnt = %u\n\n",
			 le32_to_cpu(htt_stats_buf->prof_enable_cnt));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_latency_prof_stats_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_latency_prof_stats_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 page_fault_avg = 0;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	if (le32_to_cpu(htt_stats_buf->print_header) == 1) {
		len += scnprintf(buf + len, buf_len - len,
				 "HTT_STATS_LATENCY_PROF_TLV:\n");

		len += scnprintf(buf + len, buf_len - len,
				 "|%-32s|%8s|%15s|%15s|%8s|%15s|%8s|%8s|%15s|%8s|%15s|",
				 "prof_name", "cnt", "min", "min_pcycles", "max",
				 "max_pcycles", "last", "tot", "tot_pcycles", "avg",
				 "avg_pcycles");

		len += scnprintf(buf + len, buf_len - len,
				 "%15s|%26s|%8s|%8s|%8s|%10s|%17s|%6s|\n",
				 "hist_intvl", "hist", "pf_max", "pf_avg",
				 "pf_tot", "ignoredCnt", "intHist", "intMax");
	}

	if (le32_to_cpu(htt_stats_buf->cnt)) {
		u32 cnt = le32_to_cpu(htt_stats_buf->cnt);

		page_fault_avg = le32_to_cpu(htt_stats_buf->page_fault_total)/cnt;
	}

	len += scnprintf(buf + len, buf_len - len,
			 "|%-32s|%8u|%15u|%15u|%8u|%15u|%8u|%8u|%15u|%8u|%15u|",
			 htt_stats_buf->latency_prof_name,
			 le32_to_cpu(htt_stats_buf->cnt),
			 le32_to_cpu(htt_stats_buf->min),
			 le32_to_cpu(htt_stats_buf->min_pcycles_time),
			 le32_to_cpu(htt_stats_buf->max),
			 le32_to_cpu(htt_stats_buf->max_pcycles_time),
			 le32_to_cpu(htt_stats_buf->last),
			 le32_to_cpu(htt_stats_buf->tot),
			 le32_to_cpu(htt_stats_buf->total_pcycles_time),
			 le32_to_cpu(htt_stats_buf->avg),
			 le32_to_cpu(htt_stats_buf->avg_pcycles_time));

	len += scnprintf(buf + len, buf_len - len,
			 "%15u|%8u:%8u:%8u|%8u|%8u|%8u|%10u|%5u:%5u:%5u|%6u|\n",
			 le32_to_cpu(htt_stats_buf->hist_intvl),
			 le32_to_cpu(htt_stats_buf->hist[0]),
			 le32_to_cpu(htt_stats_buf->hist[1]),
			 le32_to_cpu(htt_stats_buf->hist[2]),
			 le32_to_cpu(htt_stats_buf->page_fault_max),
			 page_fault_avg,
			 le32_to_cpu(htt_stats_buf->page_fault_total),
			 le32_to_cpu(htt_stats_buf->ignored_latency_count),
			 le32_to_cpu(htt_stats_buf->interrupts_hist[0]),
			 le32_to_cpu(htt_stats_buf->interrupts_hist[1]),
			 le32_to_cpu(htt_stats_buf->interrupts_hist[2]),
			 le32_to_cpu(htt_stats_buf->interrupts_max));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ul_ofdma_trigger_stats(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_pdev_ul_trigger_stats_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 mac_id;
	u8 j;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_RX_PDEV_UL_TRIGGER_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "rx_11ax_ul_ofdma = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_11ax_ul_ofdma));
	len += print_array_to_buf(buf, len, "ul_ofdma_rx_mcs",
				  htt_stats_buf->ul_ofdma_rx_mcs,
				  ATH12K_HTT_RX_NUM_MCS_CNTRS, "\n");
	for (j = 0; j < ATH12K_HTT_RX_NUM_GI_CNTRS; j++) {
		len += scnprintf(buf + len, buf_len - len, "ul_ofdma_rx_gi[%u]", j);
		len += print_array_to_buf(buf, len, "",
					  htt_stats_buf->ul_ofdma_rx_gi[j],
					  ATH12K_HTT_RX_NUM_MCS_CNTRS, "\n");
	}

	len += print_array_to_buf_index(buf, len, "ul_ofdma_rx_nss", 1,
					htt_stats_buf->ul_ofdma_rx_nss,
					ATH12K_HTT_RX_NUM_SPATIAL_STREAMS, "\n");
	len += print_array_to_buf(buf, len, "ul_ofdma_rx_bw",
				  htt_stats_buf->ul_ofdma_rx_bw,
				  ATH12K_HTT_RX_NUM_BW_CNTRS, "\n");

	for (j = 0; j < ATH12K_HTT_RX_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_ul_ofdma_rx_bw" :
				 "quarter_ul_ofdma_rx_bw");
		len += print_array_to_buf(buf, len, "", htt_stats_buf->red_bw[j],
					  ATH12K_HTT_RX_NUM_BW_CNTRS, "\n");
	}
	len += scnprintf(buf + len, buf_len - len, "ul_ofdma_rx_stbc = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_ofdma_rx_stbc));
	len += scnprintf(buf + len, buf_len - len, "ul_ofdma_rx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_ofdma_rx_ldpc));

	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_data_ru_size_ppdu = ");
	for (j = 0; j < ATH12K_HTT_RX_NUM_RU_SIZE_CNTRS; j++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_ax_tx_rx_ru_size_to_str(j),
				 le32_to_cpu(htt_stats_buf->data_ru_size_ppdu[j]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len,
			 "rx_ulofdma_non_data_ru_size_ppdu = ");
	for (j = 0; j < ATH12K_HTT_RX_NUM_RU_SIZE_CNTRS; j++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_ax_tx_rx_ru_size_to_str(j),
				 le32_to_cpu(htt_stats_buf->non_data_ru_size_ppdu[j]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += print_array_to_buf(buf, len, "rx_rssi_track_sta_aid",
				  htt_stats_buf->uplink_sta_aid,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf(buf, len, "rx_sta_target_rssi",
				  htt_stats_buf->uplink_sta_target_rssi,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf(buf, len, "rx_sta_fd_rssi",
				  htt_stats_buf->uplink_sta_fd_rssi,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf(buf, len, "rx_sta_power_headroom",
				  htt_stats_buf->uplink_sta_power_headroom,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += scnprintf(buf + len, buf_len - len,
			 "ul_ofdma_basic_trigger_rx_qos_null_only = %u\n\n",
			 le32_to_cpu(htt_stats_buf->ul_ofdma_bsc_trig_rx_qos_null_only));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ul_ofdma_user_stats(const void *tag_buf, u16 tag_len,
				     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_pdev_ul_ofdma_user_stats_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 user_index;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	user_index = __le32_to_cpu(htt_stats_buf->user_index);

	if (!user_index)
		len += scnprintf(buf + len, buf_len - len,
				 "HTT_RX_PDEV_UL_OFDMA_USER_STAS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_non_data_ppdu_%u = %u\n",
			 user_index,
			 le32_to_cpu(htt_stats_buf->rx_ulofdma_non_data_ppdu));
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_data_ppdu_%u = %u\n",
			 user_index,
			 le32_to_cpu(htt_stats_buf->rx_ulofdma_data_ppdu));
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_mpdu_ok_%u = %u\n",
			 user_index,
			 le32_to_cpu(htt_stats_buf->rx_ulofdma_mpdu_ok));
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_mpdu_fail_%u = %u\n",
			 user_index,
			 le32_to_cpu(htt_stats_buf->rx_ulofdma_mpdu_fail));
	len += scnprintf(buf + len, buf_len - len,
			 "rx_ulofdma_non_data_nusers_%u = %u\n", user_index,
			 le32_to_cpu(htt_stats_buf->rx_ulofdma_non_data_nusers));
	len += scnprintf(buf + len, buf_len - len, "rx_ulofdma_data_nusers_%u = %u\n\n",
			 user_index,
			 le32_to_cpu(htt_stats_buf->rx_ulofdma_data_nusers));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ul_mumimo_trig_stats(const void *tag_buf, u16 tag_len,
				      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_ul_mumimo_trig_stats_tlv *htt_stats_buf = tag_buf;
	char str_buf[ATH12K_HTT_MAX_STRING_LEN] = {0};
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 mac_id;
	u16 index;
	u8 i, j;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id = __le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_RX_PDEV_UL_MUMIMO_TRIG_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "rx_11ax_ul_mumimo = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_11ax_ul_mumimo));
	index = 0;
	memset(str_buf, 0x0, ATH12K_HTT_MAX_STRING_LEN);
	for (i = 0; i < ATH12K_HTT_RX_NUM_MCS_CNTRS; i++)
		index += scnprintf(&str_buf[index], ATH12K_HTT_MAX_STRING_LEN - index,
				  " %u:%u,", i,
				  le32_to_cpu(htt_stats_buf->ul_mumimo_rx_mcs[i]));

	for (i = 0; i < ATH12K_HTT_RX_NUM_EXTRA_MCS_CNTRS; i++)
		index += scnprintf(&str_buf[index], ATH12K_HTT_MAX_STRING_LEN - index,
				  " %u:%u,", i + ATH12K_HTT_RX_NUM_MCS_CNTRS,
				  le32_to_cpu(htt_stats_buf->ul_mumimo_rx_mcs_ext[i]));
	str_buf[--index] = '\0';
	len += scnprintf(buf + len, buf_len - len, "ul_mumimo_rx_mcs = %s\n", str_buf);

	for (j = 0; j < ATH12K_HTT_RX_NUM_GI_CNTRS; j++) {
		index = 0;
		memset(&str_buf[index], 0x0, ATH12K_HTT_MAX_STRING_LEN);
		for (i = 0; i < ATH12K_HTT_RX_NUM_MCS_CNTRS; i++)
			index += scnprintf(&str_buf[index],
					  ATH12K_HTT_MAX_STRING_LEN - index,
					  " %u:%u,", i,
					  le32_to_cpu(htt_stats_buf->ul_rx_gi[j][i]));

		for (i = 0; i < ATH12K_HTT_RX_NUM_EXTRA_MCS_CNTRS; i++)
			index += scnprintf(&str_buf[index],
					  ATH12K_HTT_MAX_STRING_LEN - index,
					  " %u:%u,", i + ATH12K_HTT_RX_NUM_MCS_CNTRS,
					  le32_to_cpu(htt_stats_buf->ul_gi_ext[j][i]));
		str_buf[--index] = '\0';
		len += scnprintf(buf + len, buf_len - len,
				 "ul_mumimo_rx_gi_%u = %s\n", j, str_buf);
	}

	index = 0;
	memset(str_buf, 0x0, ATH12K_HTT_MAX_STRING_LEN);
	len += print_array_to_buf_index(buf, len, "ul_mumimo_rx_nss", 1,
					htt_stats_buf->ul_mumimo_rx_nss,
					ATH12K_HTT_RX_NUM_SPATIAL_STREAMS, "\n");

	len += print_array_to_buf(buf, len, "ul_mumimo_rx_bw",
				  htt_stats_buf->ul_mumimo_rx_bw,
				  ATH12K_HTT_RX_NUM_BW_CNTRS, "\n");
	for (i = 0; i < ATH12K_HTT_RX_NUM_REDUCED_CHAN_TYPES; i++) {
		index = 0;
		memset(str_buf, 0x0, ATH12K_HTT_MAX_STRING_LEN);
		for (j = 0; j < ATH12K_HTT_RX_NUM_BW_CNTRS; j++)
			index += scnprintf(&str_buf[index],
					  ATH12K_HTT_MAX_STRING_LEN - index,
					  " %u:%u,", j,
					  le32_to_cpu(htt_stats_buf->red_bw[i][j]));
		str_buf[--index] = '\0';
		len += scnprintf(buf + len, buf_len - len, "%s = %s\n",
				 i == 0 ? "half_ul_mumimo_rx_bw" :
				 "quarter_ul_mumimo_rx_bw", str_buf);
	}

	len += scnprintf(buf + len, buf_len - len, "ul_mumimo_rx_stbc = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_mumimo_rx_stbc));
	len += scnprintf(buf + len, buf_len - len, "ul_mumimo_rx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_mumimo_rx_ldpc));

	for (j = 0; j < ATH12K_HTT_RX_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_ul_mumimo_rssi_in_dbm: chain%u ", j);
		len += print_array_to_buf_s8(buf, len, "", 0,
				htt_stats_buf->ul_rssi[j],
				ATH12K_HTT_RX_PDEV_STATS_TOTAL_BW_COUNTERS, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_UL_MUMIMO_USER_STATS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_ul_mumimo_target_rssi: user_%u ", j);
		len += print_array_to_buf_s8(buf, len, "", 0,
					     htt_stats_buf->tgt_rssi[j],
					     ATH12K_HTT_RX_NUM_BW_CNTRS, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_UL_MUMIMO_USER_STATS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_ul_mumimo_fd_rssi: user_%u ", j);
		len += print_array_to_buf_s8(buf, len, "", 0,
					     htt_stats_buf->fd[j],
					     ATH12K_HTT_RX_NUM_SPATIAL_STREAMS, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_UL_MUMIMO_USER_STATS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_ulmumimo_pilot_evm_db_mean: user_%u ", j);
		len += print_array_to_buf_s8(buf, len, "", 0,
					     htt_stats_buf->db[j],
					     ATH12K_HTT_RX_NUM_SPATIAL_STREAMS, "\n");
	}

	len += scnprintf(buf + len, buf_len - len,
			 "ul_mumimo_basic_trigger_rx_qos_null_only = %u\n\n",
			 le32_to_cpu(htt_stats_buf->mumimo_bsc_trig_rx_qos_null_only));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_rx_fse_stats_tlv(const void *tag_buf, u16 tag_len,
				  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_fse_stats_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_RX_FSE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "=== Software RX FSE STATS ===\n");
	len += scnprintf(buf + len, buf_len - len, "Enable count  = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_enable_cnt));
	len += scnprintf(buf + len, buf_len - len, "Disable count = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_disable_cnt));
	len += scnprintf(buf + len, buf_len - len, "Cache invalidate entry count = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_cache_invalidate_entry_cnt));
	len += scnprintf(buf + len, buf_len - len, "Full cache invalidate count = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_full_cache_invalidate_cnt));

	len += scnprintf(buf + len, buf_len - len, "\n=== Hardware RX FSE STATS ===\n");
	len += scnprintf(buf + len, buf_len - len, "Cache hits count = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_num_cache_hits_cnt));
	len += scnprintf(buf + len, buf_len - len, "Cache no. of searches = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_num_searches_cnt));
	len += scnprintf(buf + len, buf_len - len, "Cache occupancy peak count:\n");
	len += scnprintf(buf + len, buf_len - len, "[0] = %u [1-16] = %u [17-32] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[0]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[1]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[2]));
	len += scnprintf(buf + len, buf_len - len, "[33-48] = %u [49-64] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[3]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[4]));
	len += scnprintf(buf + len, buf_len - len, "[65-80] = %u [81-96] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[5]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[6]));
	len += scnprintf(buf + len, buf_len - len, "[97-112] = %u [113-127] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[7]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[8]));
	len += scnprintf(buf + len, buf_len - len, "[128] = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_peak_cnt[9]));
	len += scnprintf(buf + len, buf_len - len, "Cache occupancy current count:\n");
	len += scnprintf(buf + len, buf_len - len, "[0] = %u [1-16] = %u [17-32] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[0]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[1]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[2]));
	len += scnprintf(buf + len, buf_len - len, "[33-48] = %u [49-64] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[3]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[4]));
	len += scnprintf(buf + len, buf_len - len, "[65-80] = %u [81-96] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[5]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[6]));
	len += scnprintf(buf + len, buf_len - len, "[97-112] = %u [113-127] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[7]),
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[8]));
	len += scnprintf(buf + len, buf_len - len, "[128] = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_cache_occupancy_curr_cnt[9]));
	len += scnprintf(buf + len, buf_len - len, "Cache search square count:\n");
	len += scnprintf(buf + len, buf_len - len, "[0] = %u [1-50] = %u [51-100] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_search_stat_square_cnt[0]),
			 le32_to_cpu(htt_stats_buf->fse_search_stat_square_cnt[1]),
			 le32_to_cpu(htt_stats_buf->fse_search_stat_square_cnt[2]));
	len += scnprintf(buf + len, buf_len - len, "[101-200] = %u [201-255] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_search_stat_square_cnt[3]),
			 le32_to_cpu(htt_stats_buf->fse_search_stat_square_cnt[4]));
	len += scnprintf(buf + len, buf_len - len, "[256] = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_search_stat_square_cnt[5]));
	len += scnprintf(buf + len, buf_len - len, "Cache search peak pending count:\n");
	len += scnprintf(buf + len, buf_len - len, "[0] = %u [1-2] = %u [3-4] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_search_stat_peak_cnt[0]),
			 le32_to_cpu(htt_stats_buf->fse_search_stat_peak_cnt[1]),
			 le32_to_cpu(htt_stats_buf->fse_search_stat_peak_cnt[2]));
	len += scnprintf(buf + len, buf_len - len, "[Greater/Equal to 5] = %u\n",
			 le32_to_cpu(htt_stats_buf->fse_search_stat_peak_cnt[3]));
	len += scnprintf(buf + len, buf_len - len, "Cache search tot pending count:\n");
	len += scnprintf(buf + len, buf_len - len, "[0] = %u [1-2] = %u [3-4] = %u ",
			 le32_to_cpu(htt_stats_buf->fse_search_stat_pending_cnt[0]),
			 le32_to_cpu(htt_stats_buf->fse_search_stat_pending_cnt[1]),
			 le32_to_cpu(htt_stats_buf->fse_search_stat_pending_cnt[2]));
	len += scnprintf(buf + len, buf_len - len, "[Greater/Equal to 5] = %u\n\n",
			 le32_to_cpu(htt_stats_buf->fse_search_stat_pending_cnt[3]));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_tx_rate_txbf_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_txrate_txbf_stats_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u8 i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_PDEV_TX_RATE_TXBF_STATS:\n");
	len += scnprintf(buf + len, buf_len - len, "Legacy OFDM Rates: 6 Mbps: %u, ",
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[0]));
	len += scnprintf(buf + len, buf_len - len, "9 Mbps: %u, 12 Mbps: %u, ",
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[1]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[2]));
	len += scnprintf(buf + len, buf_len - len, "18 Mbps: %u\n",
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[3]));
	len += scnprintf(buf + len, buf_len - len, "24 Mbps: %u, 36 Mbps: %u, ",
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[4]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[5]));
	len += scnprintf(buf + len, buf_len - len, "48 Mbps: %u, 54 Mbps: %u\n",
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[6]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[7]));

	len += print_array_to_buf(buf, len, "tx_ol_mcs", htt_stats_buf->tx_su_ol_mcs,
				  ATH12K_HTT_TX_BF_RATE_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "tx_ibf_mcs", htt_stats_buf->tx_su_ibf_mcs,
				  ATH12K_HTT_TX_BF_RATE_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "tx_txbf_mcs", htt_stats_buf->tx_su_txbf_mcs,
				  ATH12K_HTT_TX_BF_RATE_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf_index(buf, len, "tx_ol_nss", 1,
					htt_stats_buf->tx_su_ol_nss,
					ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS,
					"\n");
	len += print_array_to_buf_index(buf, len, "tx_ibf_nss", 1,
					htt_stats_buf->tx_su_ibf_nss,
					ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS,
					"\n");
	len += print_array_to_buf_index(buf, len, "tx_txbf_nss", 1,
					htt_stats_buf->tx_su_txbf_nss,
					ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS,
					"\n");
	len += print_array_to_buf(buf, len, "tx_ol_bw", htt_stats_buf->tx_su_ol_bw,
				  ATH12K_HTT_TXBF_NUM_BW_CNTRS, "\n");
	for (i = 0; i < ATH12K_HTT_TXBF_NUM_REDUCED_CHAN_TYPES; i++)
		len += print_array_to_buf(buf, len, i ? "quarter_tx_ol_bw" :
					  "half_tx_ol_bw",
					  htt_stats_buf->ol[i],
					  ATH12K_HTT_TXBF_NUM_BW_CNTRS,
					  "\n");

	len += print_array_to_buf(buf, len, "tx_ibf_bw", htt_stats_buf->tx_su_ibf_bw,
				  ATH12K_HTT_TXBF_NUM_BW_CNTRS, "\n");
	for (i = 0; i < ATH12K_HTT_TXBF_NUM_REDUCED_CHAN_TYPES; i++)
		len += print_array_to_buf(buf, len, i ? "quarter_tx_ibf_bw" :
					  "half_tx_ibf_bw",
					  htt_stats_buf->ibf[i],
					  ATH12K_HTT_TXBF_NUM_BW_CNTRS,
					  "\n");

	len += print_array_to_buf(buf, len, "tx_txbf_bw", htt_stats_buf->tx_su_txbf_bw,
				  ATH12K_HTT_TXBF_NUM_BW_CNTRS, "\n");
	for (i = 0; i < ATH12K_HTT_TXBF_NUM_REDUCED_CHAN_TYPES; i++)
		len += print_array_to_buf(buf, len, i ? "quarter_tx_txbf_bw" :
					  "half_tx_txbf_bw",
					  htt_stats_buf->txbf[i],
					  ATH12K_HTT_TXBF_NUM_BW_CNTRS,
					  "\n");
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_PDEV_TXBF_FLAG_RETURN_STATS:\n");
	len += scnprintf(buf + len, buf_len - len, "TXBF_reason_code_stats: 0:%u, 1:%u,",
			 le32_to_cpu(htt_stats_buf->txbf_flag_set_mu_mode),
			 le32_to_cpu(htt_stats_buf->txbf_flag_set_final_status));
	len += scnprintf(buf + len, buf_len - len, " 2:%u, 3:%u, 4:%u, 5:%u, ",
			 le32_to_cpu(htt_stats_buf->txbf_flag_not_set_verified_txbf_mode),
			 le32_to_cpu(htt_stats_buf->txbf_flag_not_set_disable_p2p_access),
			 le32_to_cpu(htt_stats_buf->txbf_flag_not_set_max_nss_in_he160),
			 le32_to_cpu(htt_stats_buf->txbf_flag_not_set_disable_uldlofdma));
	len += scnprintf(buf + len, buf_len - len, "6:%u, 7:%u\n\n",
			 le32_to_cpu(htt_stats_buf->txbf_flag_not_set_mcs_threshold_val),
			 le32_to_cpu(htt_stats_buf->txbf_flag_not_set_final_status));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_txbf_ofdma_ax_ndpa_stats_tlv(const void *tag_buf, u16 tag_len,
					      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_txbf_ofdma_ax_ndpa_stats_tlv *stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 num_elements;
	u8 i;

	if (tag_len < sizeof(*stats_buf))
		return;

	num_elements = le32_to_cpu(stats_buf->num_elems_ax_ndpa_arr);

	len += scnprintf(buf + len, buf_len - len, "HTT_TXBF_OFDMA_AX_NDPA_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ax_ofdma_ndpa_queued =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndpa[i].ax_ofdma_ndpa_queued));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_ndpa_tried =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndpa[i].ax_ofdma_ndpa_tried));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_ndpa_flushed =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndpa[i].ax_ofdma_ndpa_flush));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_ndpa_err =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndpa[i].ax_ofdma_ndpa_err));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_txbf_ofdma_ax_ndp_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_txbf_ofdma_ax_ndp_stats_tlv *stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 num_elements;
	u8 i;

	if (tag_len < sizeof(*stats_buf))
		return;

	num_elements = le32_to_cpu(stats_buf->num_elems_ax_ndp_arr);

	len += scnprintf(buf + len, buf_len - len, "HTT_TXBF_OFDMA_AX_NDP_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ax_ofdma_ndp_queued =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndp[i].ax_ofdma_ndp_queued));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_ndp_tried =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndp[i].ax_ofdma_ndp_tried));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_ndp_flushed =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndp[i].ax_ofdma_ndp_flush));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_ndp_err =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_ndp[i].ax_ofdma_ndp_err));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_txbf_ofdma_ax_brp_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_txbf_ofdma_ax_brp_stats_tlv *stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 num_elements;
	u8 i;

	if (tag_len < sizeof(*stats_buf))
		return;

	num_elements = le32_to_cpu(stats_buf->num_elems_ax_brp_arr);

	len += scnprintf(buf + len, buf_len - len, "HTT_TXBF_OFDMA_AX_BRP_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ax_ofdma_brpoll_queued =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_brp[i].ax_ofdma_brp_queued));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_brpoll_tied =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_brp[i].ax_ofdma_brp_tried));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_brpoll_flushed =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_brp[i].ax_ofdma_brp_flushed));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_brp_err =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_brp[i].ax_ofdma_brp_err));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_brp_err_num_cbf_rcvd =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_brp[i].ax_ofdma_num_cbf_rcvd));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_txbf_ofdma_ax_steer_stats_tlv(const void *tag_buf, u16 tag_len,
					       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_txbf_ofdma_ax_steer_stats_tlv *stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 num_elements;
	u8 i;

	if (tag_len < sizeof(*stats_buf))
		return;

	num_elements = le32_to_cpu(stats_buf->num_elems_ax_steer_arr);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TXBF_OFDMA_AX_STEER_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ax_ofdma_num_ppdu_steer =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_steer[i].num_ppdu_steer));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_num_ppdu_ol =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_steer[i].num_ppdu_ol));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_num_usrs_prefetch =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_steer[i].num_usr_prefetch));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_num_usrs_sound =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_steer[i].num_usr_sound));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\nax_ofdma_num_usrs_force_sound =");
	for (i = 0; i < num_elements; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,", i + 1,
				 le32_to_cpu(stats_buf->ax_steer[i].num_usr_force_sound));
	len--;
	*(buf + len) = '\0';

	len += scnprintf(buf + len, buf_len - len, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_txbf_ofdma_ax_steer_mpdu_stats_tlv(const void *tag_buf, u16 tag_len,
						    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_txbf_ofdma_ax_steer_mpdu_stats_tlv *stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TXBF_OFDMA_AX_STEER_MPDU_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "rbo_steer_mpdus_tried = %u\n",
			 le32_to_cpu(stats_buf->ax_ofdma_rbo_steer_mpdus_tried));
	len += scnprintf(buf + len, buf_len - len, "rbo_steer_mpdus_failed = %u\n",
			 le32_to_cpu(stats_buf->ax_ofdma_rbo_steer_mpdus_failed));
	len += scnprintf(buf + len, buf_len - len, "sifs_steer_mpdus_tried = %u\n",
			 le32_to_cpu(stats_buf->ax_ofdma_sifs_steer_mpdus_tried));
	len += scnprintf(buf + len, buf_len - len, "sifs_steer_mpdus_failed = %u\n\n",
			 le32_to_cpu(stats_buf->ax_ofdma_sifs_steer_mpdus_failed));

	stats_req->buf_len = len;
}

static void ath12k_htt_print_dlpager_entry(const struct ath12k_htt_pgs_info *pg_info,
					   int idx, char *str_buf)
{
	u64 page_timestamp;
	u16 index = 0;

	page_timestamp = ath12k_le32hilo_to_u64(pg_info->ts_msb, pg_info->ts_lsb);

	index += snprintf(&str_buf[index], ATH12K_HTT_MAX_STRING_LEN - index,
			  "Index - %u ; Page Number - %u ; ",
			  idx, le32_to_cpu(pg_info->page_num));
	index += snprintf(&str_buf[index], ATH12K_HTT_MAX_STRING_LEN - index,
			  "Num of pages - %u ; Timestamp - %lluus\n",
			  le32_to_cpu(pg_info->num_pgs), page_timestamp);
}

static void
ath12k_htt_print_dlpager_stats_tlv(const void *tag_buf, u16 tag_len,
				   struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_dl_pager_stats_tlv *stat_buf = tag_buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 dword_lock, dword_unlock;
	int i;
	u8 *buf = stats_req->buf;
	u8 pg_locked;
	u8 pg_unlock;
	char str_buf[ATH12K_HTT_MAX_STRING_LEN] = {0};

	if (tag_len < sizeof(*stat_buf))
		return;

	dword_lock = le32_get_bits(stat_buf->info2,
				   ATH12K_HTT_DLPAGER_TOTAL_LOCK_PAGES_INFO2);
	dword_unlock = le32_get_bits(stat_buf->info2,
				     ATH12K_HTT_DLPAGER_TOTAL_FREE_PAGES_INFO2);

	pg_locked = ATH12K_HTT_STATS_PAGE_LOCKED;
	pg_unlock = ATH12K_HTT_STATS_PAGE_UNLOCKED;

	len += scnprintf(buf + len, buf_len - len, "HTT_DLPAGER_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ASYNC locked pages = %u\n",
			 le32_get_bits(stat_buf->info0,
				       ATH12K_HTT_DLPAGER_ASYNC_LOCK_PG_CNT_INFO0));
	len += scnprintf(buf + len, buf_len - len, "SYNC locked pages = %u\n",
			 le32_get_bits(stat_buf->info0,
				       ATH12K_HTT_DLPAGER_SYNC_LOCK_PG_CNT_INFO0));
	len += scnprintf(buf + len, buf_len - len, "Total locked pages = %u\n",
			 le32_get_bits(stat_buf->info1,
				       ATH12K_HTT_DLPAGER_TOTAL_LOCK_PAGES_INFO1));
	len += scnprintf(buf + len, buf_len - len, "Total free pages = %u\n",
			 le32_get_bits(stat_buf->info1,
				       ATH12K_HTT_DLPAGER_TOTAL_FREE_PAGES_INFO1));

	len += scnprintf(buf + len, buf_len - len, "\nLOCKED PAGES HISTORY\n");
	len += scnprintf(buf + len, buf_len - len, "last_locked_page_idx = %u\n",
			 dword_lock ? dword_lock - 1 : (ATH12K_PAGER_MAX - 1));

	for (i = 0; i < ATH12K_PAGER_MAX; i++) {
		memset(str_buf, 0x0, ATH12K_HTT_MAX_STRING_LEN);
		ath12k_htt_print_dlpager_entry(&stat_buf->pgs_info[pg_locked][i],
					       i, str_buf);
		len += scnprintf(buf + len, buf_len - len, "%s", str_buf);
	}

	len += scnprintf(buf + len, buf_len - len, "\nUNLOCKED PAGES HISTORY\n");
	len += scnprintf(buf + len, buf_len - len, "last_unlocked_page_idx = %u\n",
			 dword_unlock ? dword_unlock - 1 : ATH12K_PAGER_MAX - 1);

	for (i = 0; i < ATH12K_PAGER_MAX; i++) {
		memset(str_buf, 0x0, ATH12K_HTT_MAX_STRING_LEN);
		ath12k_htt_print_dlpager_entry(&stat_buf->pgs_info[pg_unlock][i],
					       i, str_buf);
		len += scnprintf(buf + len, buf_len - len, "%s", str_buf);
	}

	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_phy_stats_tlv(const void *tag_buf, u16 tag_len,
			       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_phy_stats_tlv *htt_stats_buf = tag_buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 *buf = stats_req->buf, i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_PHY_STATS_TLV:\n");
	for (i = 0; i < ATH12K_HTT_STATS_MAX_CHAINS; i++)
		len += scnprintf(buf + len, buf_len - len, "bdf_nf_chain[%d] = %d\n",
				 i, a_sle32_to_cpu(htt_stats_buf->nf_chain[i]));
	for (i = 0; i < ATH12K_HTT_STATS_MAX_CHAINS; i++)
		len += scnprintf(buf + len, buf_len - len, "runtime_nf_chain[%d] = %d\n",
				 i, a_sle32_to_cpu(htt_stats_buf->runtime_nf_chain[i]));
	len += scnprintf(buf + len, buf_len - len, "false_radar_cnt = %u / %u (mins)\n",
			 le32_to_cpu(htt_stats_buf->false_radar_cnt),
			 le32_to_cpu(htt_stats_buf->fw_run_time));
	len += scnprintf(buf + len, buf_len - len, "radar_cs_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->radar_cs_cnt));
	len += scnprintf(buf + len, buf_len - len, "ani_level = %d\n\n",
			 a_sle32_to_cpu(htt_stats_buf->ani_level));
	len += scnprintf(buf + len, buf_len - len, "current operating bw = %u\n",
			 le32_to_cpu(htt_stats_buf->current_operating_width));
	len += scnprintf(buf + len, buf_len - len, "current device bw = %u\n",
			 le32_to_cpu(htt_stats_buf->current_device_width));
	len += scnprintf(buf + len, buf_len - len, "last radar type = %u\n",
			 le32_to_cpu(htt_stats_buf->last_radar_type));
	len += scnprintf(buf + len, buf_len - len, "dfs regulatory domain = %u\n",
			 le32_to_cpu(htt_stats_buf->dfs_reg_domain));
	len += scnprintf(buf + len, buf_len - len, "radar mask bit = %u\n",
			 le32_to_cpu(htt_stats_buf->radar_mask_bit));
	len += scnprintf(buf + len, buf_len - len, "radar rssi = %d\n",
			 le32_to_cpu(htt_stats_buf->radar_rssi));
	len += scnprintf(buf + len, buf_len - len, "radar dfs flags = %u\n",
			 le32_to_cpu(htt_stats_buf->radar_dfs_flags));
	len += scnprintf(buf + len, buf_len - len, "operating center freq = %u\n",
			 le32_to_cpu(htt_stats_buf->band_center_frequency_operating));
	len += scnprintf(buf + len, buf_len - len, "device center freq = %u\n",
			 le32_to_cpu(htt_stats_buf->band_center_frequency_device));
	len += scnprintf(buf + len, buf_len - len, "is_static_ani = %u\n",
			u32_get_bits(le32_to_cpu(htt_stats_buf->dword__ani_mode),
			HTT_STATS_ANI_MODE_M));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_phy_counters_tlv(const void *tag_buf, u16 tag_len,
				  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_phy_counters_tlv *htt_stats_buf = tag_buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_PHY_COUNTERS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "rx_ofdma_timing_err_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_ofdma_timing_err_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_cck_fail_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_cck_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "mactx_abort_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->mactx_abort_cnt));
	len += scnprintf(buf + len, buf_len - len, "macrx_abort_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->macrx_abort_cnt));
	len += scnprintf(buf + len, buf_len - len, "phytx_abort_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->phytx_abort_cnt));
	len += scnprintf(buf + len, buf_len - len, "phyrx_abort_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->phyrx_abort_cnt));
	len += scnprintf(buf + len, buf_len - len, "phyrx_defer_abort_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->phyrx_defer_abort_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_gain_adj_lstf_event_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_gain_adj_lstf_event_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_gain_adj_non_legacy_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_gain_adj_non_legacy_cnt));
	len += print_array_to_buf(buf, len, "rx_pkt_cnt", htt_stats_buf->rx_pkt_cnt,
				  ATH12K_HTT_MAX_RX_PKT_CNT, "\n");
	len += print_array_to_buf(buf, len, "rx_pkt_crc_pass_cnt",
				  htt_stats_buf->rx_pkt_crc_pass_cnt,
				  ATH12K_HTT_MAX_RX_PKT_CRC_PASS_CNT, "\n");
	len += print_array_to_buf(buf, len, "per_blk_err_cnt",
				  htt_stats_buf->per_blk_err_cnt,
				  ATH12K_HTT_MAX_PER_BLK_ERR_CNT, "\n");
	len += print_array_to_buf(buf, len, "rx_ota_err_cnt",
				  htt_stats_buf->rx_ota_err_cnt,
				  ATH12K_HTT_MAX_RX_OTA_ERR_CNT, "\n");
	len += print_array_to_buf(buf, len, "rx_pkt_cnt_ext",
				  htt_stats_buf->rx_pkt_cnt_ext,
				  ATH12K_HTT_MAX_RX_PKT_CNT_EXT, "\n");
	len += print_array_to_buf(buf, len, "rx_pkt_crc_pass_cnt_ext",
				  htt_stats_buf->rx_pkt_crc_pass_cnt_ext,
				  ATH12K_HTT_MAX_RX_PKT_CRC_PASS_CNT_EXT, "\n");
	len += print_array_to_buf(buf, len, "rx_pkt_mu_cnt",
				  htt_stats_buf->rx_pkt_mu_cnt,
				  ATH12K_HTT_MAX_RX_PKT_MU_CNT, "\n");
	len += print_array_to_buf(buf, len, "tx_pkt_cnt",
				  htt_stats_buf->tx_pkt_cnt,
				  ATH12K_HTT_MAX_TX_PKT_CNT, "\n");
	len += print_array_to_buf(buf, len, "phy_tx_abort_cnt",
				  htt_stats_buf->phy_tx_abort_cnt,
				  ATH12K_HTT_MAX_PHY_TX_ABORT_CNT, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_phy_reset_stats_tlv(const void *tag_buf, u16 tag_len,
				     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_phy_reset_stats_tlv *htt_stats_buf = tag_buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;
	u32 calmerge_val = le32_to_cpu(htt_stats_buf->calmerge_stats);
	len += scnprintf(buf + len, buf_len - len, "HTT_PHY_RESET_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_id));
	len += scnprintf(buf + len, buf_len - len, "chan_mhz = %u\n",
			 le32_to_cpu(htt_stats_buf->chan_mhz));
	len += scnprintf(buf + len, buf_len - len, "chan_band_center_freq1 = %u\n",
			 le32_to_cpu(htt_stats_buf->chan_band_center_freq1));
	len += scnprintf(buf + len, buf_len - len, "chan_band_center_freq2 = %u\n",
			 le32_to_cpu(htt_stats_buf->chan_band_center_freq2));
	len += scnprintf(buf + len, buf_len - len, "chan_phy_mode = %u\n",
			 le32_to_cpu(htt_stats_buf->chan_phy_mode));
	len += scnprintf(buf + len, buf_len - len, "chan_flags = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->chan_flags));
	len += scnprintf(buf + len, buf_len - len, "chan_num = %u\n",
			 le32_to_cpu(htt_stats_buf->chan_num));
	len += scnprintf(buf + len, buf_len - len, "reset_cause = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->reset_cause));
	len += scnprintf(buf + len, buf_len - len, "prev_reset_cause = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->prev_reset_cause));
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset_src = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phy_warm_reset_src));
	len += scnprintf(buf + len, buf_len - len, "rx_gain_tbl_mode = %d\n",
			 le32_to_cpu(htt_stats_buf->rx_gain_tbl_mode));
	len += scnprintf(buf + len, buf_len - len, "xbar_val = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->xbar_val));
	len += scnprintf(buf + len, buf_len - len, "force_calibration = %u\n",
			 le32_to_cpu(htt_stats_buf->force_calibration));
	len += scnprintf(buf + len, buf_len - len, "phyrf_mode = %u\n",
			 le32_to_cpu(htt_stats_buf->phyrf_mode));
	len += scnprintf(buf + len, buf_len - len, "phy_homechan = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_homechan));
	len += scnprintf(buf + len, buf_len - len, "phy_tx_ch_mask = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phy_tx_ch_mask));
	len += scnprintf(buf + len, buf_len - len, "phy_rx_ch_mask = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phy_rx_ch_mask));
	len += scnprintf(buf + len, buf_len - len, "phybb_ini_mask = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phybb_ini_mask));
	len += scnprintf(buf + len, buf_len - len, "phyrf_ini_mask = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phyrf_ini_mask));
	len += scnprintf(buf + len, buf_len - len, "phy_dfs_en_mask = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phy_dfs_en_mask));
	len += scnprintf(buf + len, buf_len - len, "phy_sscan_en_mask = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phy_sscan_en_mask));
	len += scnprintf(buf + len, buf_len - len, "phy_synth_sel_mask = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->phy_synth_sel_mask));
	len += scnprintf(buf + len, buf_len - len, "phy_adfs_freq = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_adfs_freq));
	len += scnprintf(buf + len, buf_len - len, "cck_fir_settings = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->cck_fir_settings));
	len += scnprintf(buf + len, buf_len - len, "phy_dyn_pri_chan = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_dyn_pri_chan));
	len += scnprintf(buf + len, buf_len - len, "cca_thresh = 0x%0x\n",
			 le32_to_cpu(htt_stats_buf->cca_thresh));
	len += scnprintf(buf + len, buf_len - len, "dyn_cca_status = %u\n",
			 le32_to_cpu(htt_stats_buf->dyn_cca_status));
	len += scnprintf(buf + len, buf_len - len, "rxdesense_thresh_hw = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->rxdesense_thresh_hw));
	len += scnprintf(buf + len, buf_len - len, "rxdesense_thresh_sw = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->rxdesense_thresh_sw));
	len += scnprintf(buf + len, buf_len - len, "phy_bw_code = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_bw_code));
	len += scnprintf(buf + len, buf_len - len, "phy_rate_mode = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_rate_mode));
	len += scnprintf(buf + len, buf_len - len, "phy_band_code = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_band_code));
	len += scnprintf(buf + len, buf_len - len, "phy_vreg_base = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->phy_vreg_base));
	len += scnprintf(buf + len, buf_len - len, "phy_vreg_base_ext = 0x%x\n",
			 le32_to_cpu(htt_stats_buf->phy_vreg_base_ext));
	len += scnprintf(buf + len, buf_len - len, "cur_table_index = %u\n",
			 le32_to_cpu(htt_stats_buf->cur_table_index));
	len += scnprintf(buf + len, buf_len - len, "whal_config_flag = %u\n",
			 le32_to_cpu(htt_stats_buf->whal_config_flag));
	len += scnprintf(buf + len, buf_len - len, "nfcal_iteration_count_home = %u\n",
			 le32_to_cpu(htt_stats_buf->nfcal_iteration_counts[0]));
	len += scnprintf(buf + len, buf_len - len, "nfcal_iteration_count_scan = %u\n",
			 le32_to_cpu(htt_stats_buf->nfcal_iteration_counts[1]));
	len += scnprintf(buf + len, buf_len - len,
			"nfcal_iteration_count_periodic = %u\n",
			 le32_to_cpu(htt_stats_buf->nfcal_iteration_counts[2]));
	len += scnprintf(buf + len, buf_len - len, "BoardID from OTP= %u\n",
			 le32_to_cpu(htt_stats_buf->BoardIDfromOTP));
	len += scnprintf(buf + len, buf_len - len, " CalData_Compressed = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_CAL_DATA_COMPRESSED_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata Source(0 - Not present, 1 - Flash, 2 - EEPROM) = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_CAL_DATA_SOURCE_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status xtalcal = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_XTALCAL_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status tpccal2G FPC = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_TPCCAL2GFPC_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status tpccal2G OPC = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_TPCCAL2GOPC_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status tpccal5G FPC = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_TPCCAL5GFPC_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status tpccal5G OPC = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_TPCCAL5GOPC_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status tpccal6G FPC = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_TPCCAL6GFPC_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status tpccal6G OPC = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_TPCCAL6GOPC_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status rxgaincal2G = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_RXGAINCAL2G_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status rxgaincal5G = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_RXGAINCAL5G_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status rxgaincal6G = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_RXGAINCAL6G_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status aoacal2G = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_AOACAL2G_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status aoacal5G = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_AOACAL5G_M));
	len += scnprintf(buf + len, buf_len - len,
			"Caldata merge status aoacal6G = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_AOACAL6G_M));
	len += scnprintf(buf + len, buf_len - len,
			"GLUT_linearity = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_GLUT_LINEARITY_M));
	len += scnprintf(buf + len, buf_len - len,
			"PLUT_linearity = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_PLUT_LINEARITY_M));
	len += scnprintf(buf + len, buf_len - len,
			"XTAL_from_OTP = %u\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_XTAL_FROM_OTP_M));
	len += scnprintf(buf + len, buf_len - len,
			"Wlan Driver Mode = %u\n\n",
			u32_get_bits((calmerge_val),
			HTT_STATS_PHY_RESET_WLANDRIVERMODE_M));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_phy_reset_counters_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_phy_reset_counters_tlv *htt_stats_buf = tag_buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_PHY_RESET_COUNTERS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_id));
	len += scnprintf(buf + len, buf_len - len, "cf_active_low_fail_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->cf_active_low_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "cf_active_low_pass_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->cf_active_low_pass_cnt));
	len += scnprintf(buf + len, buf_len - len, "phy_off_through_vreg_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->phy_off_through_vreg_cnt));
	len += scnprintf(buf + len, buf_len - len, "force_calibration_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->force_calibration_cnt));
	len += scnprintf(buf + len, buf_len - len, "rf_mode_switch_phy_off_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rf_mode_switch_phy_off_cnt));
	len += scnprintf(buf + len, buf_len - len, "temperature_recal_cnt = %u\n\n",
			 le32_to_cpu(htt_stats_buf->temperature_recal_cnt));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_phy_tpc_stats_tlv(const void *tag_buf, u16 tag_len,
				   struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_phy_tpc_stats_tlv *htt_stats_buf = tag_buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 *buf = stats_req->buf;
	u8 i, j, k;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	u32 ctl_val = le32_to_cpu(htt_stats_buf->ctl_args);
	len += scnprintf(buf + len, buf_len - len, "HTT_PHY_TPC_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_id));
	len += scnprintf(buf + len, buf_len - len, "tx_power_scale = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_power_scale));
	len += scnprintf(buf + len, buf_len - len, "tx_power_scale_db = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_power_scale_db));
	len += scnprintf(buf + len, buf_len - len, "min_negative_tx_power = %d\n",
			 le32_to_cpu(htt_stats_buf->min_negative_tx_power));
	len += scnprintf(buf + len, buf_len - len, "reg_ctl_domain = %u\n",
			 le32_to_cpu(htt_stats_buf->reg_ctl_domain));
	len += scnprintf(buf + len, buf_len - len, "twice_max_rd_power = %u\n",
			 le32_to_cpu(htt_stats_buf->twice_max_rd_power));
	len += scnprintf(buf + len, buf_len - len, "max_tx_power = %u\n",
			 le32_to_cpu(htt_stats_buf->max_tx_power));
	len += scnprintf(buf + len, buf_len - len, "home_max_tx_power = %u\n",
			 le32_to_cpu(htt_stats_buf->home_max_tx_power));
	len += scnprintf(buf + len, buf_len - len, "psd_power = %d\n",
			 le32_to_cpu(htt_stats_buf->psd_power));
	len += scnprintf(buf + len, buf_len - len, "eirp_power = %u\n",
			 le32_to_cpu(htt_stats_buf->eirp_power));
	len += scnprintf(buf + len, buf_len - len, "power_type_6ghz = %u\n",
			 le32_to_cpu(htt_stats_buf->power_type_6ghz));
	len += print_array_to_buf(buf, len, "max_reg_allowed_power",
				  htt_stats_buf->max_reg_allowed_power,
				  ATH12K_HTT_STATS_MAX_CHAINS, "\n");
	len += print_array_to_buf(buf, len, "max_reg_allowed_power_6ghz",
				  htt_stats_buf->max_reg_allowed_power_6ghz,
				  ATH12K_HTT_STATS_MAX_CHAINS, "\n");
	len += print_array_to_buf(buf, len, "sub_band_cfreq",
				  htt_stats_buf->sub_band_cfreq,
				  ATH12K_HTT_MAX_CH_PWR_INFO_SIZE, "\n");
	len += print_array_to_buf(buf, len, "sub_band_txpower",
				  htt_stats_buf->sub_band_txpower,
				  ATH12K_HTT_MAX_CH_PWR_INFO_SIZE, "\n\n");
	len += scnprintf(buf + len, buf_len - len,
			"tpc_stats : ctl_array_gain_cap_ext2_enabled = %u\n",
			u32_get_bits((ctl_val),
					HTT_PHY_TPC_STATS_AG_CAP_EXT2_ENABLED_M));
	len += scnprintf(buf + len, buf_len - len,
			"tpc_stats : ctl_flag = %d\n",
			u32_get_bits((ctl_val),
					HTT_PHY_TPC_STATS_CTL_FLAG_M));
	len += scnprintf(buf + len, buf_len - len,
			"tpc_stats : ctl_array_gain_cap_ext2_ctl_region_grp = %u\n",
			u32_get_bits((ctl_val),
					HTT_PHY_TPC_STATS_CTL_REGION_GRP_M));
	len += scnprintf(buf + len, buf_len - len,
			"tpc_stats : ctl_sub_band_index = %u\n",
			u32_get_bits((ctl_val),
					HTT_PHY_TPC_STATS_SUB_BAND_INDEX_M));
	i = 0;
	j = 0;
	for (k = 0; k < (HTT_STATS_MAX_CHAINS + 1) * HTT_STATS_MAX_CHAINS / 2; k++) {
		len += scnprintf(buf + len, buf_len - len,
				 "tpc_stats :ctl_array_gain_cap[ntx:%d][nss:%d] = %d\n",
				 i + 1, j + 1,
				 le32_to_cpu(htt_stats_buf->array_gain_cap[k]));
		if (j == i) {
			i++;
			j = 0;
		} else {
			j++;
		}
	}
	len += print_array_to_buf(buf, len,
		"tpc_stats :max_reg_only_allowed_power =",
				  htt_stats_buf->max_reg_only_allowed_power,
				  ATH12K_HTT_STATS_MAX_CHAINS, "\n");
	len += print_array_to_buf(buf, len, "tpc_stats : tx_num_chains",
				  htt_stats_buf->tx_num_chains,
				  ATH12K_HTT_STATS_MAX_CHAINS, "\n");
	len += print_array_to_buf(buf, len, "tpc_stats : tx_power_neg",
				  htt_stats_buf->tx_power_neg,
				  ATH12K_HTT_MAX_NEGATIVE_POWER_LEVEL, "\n");
	len += print_array_to_buf(buf, len, "tpc_stats : tx_power",
				  htt_stats_buf->tx_power,
				  ATH12K_HTT_MAX_POWER_LEVEL, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_soc_txrx_stats_common_tlv(const void *tag_buf, u16 tag_len,
					   struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_t2h_soc_txrx_stats_common_tlv *htt_stats_buf = tag_buf;
	u64 drop_count;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	drop_count = ath12k_le32hilo_to_u64(htt_stats_buf->inv_peers_msdu_drop_count_hi,
					    htt_stats_buf->inv_peers_msdu_drop_count_lo);

	len += scnprintf(buf + len, buf_len - len, "HTT_SOC_COMMON_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "soc_drop_count = %llu\n\n",
			 drop_count);

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_per_rate_stats_tlv(const void *tag_buf, u16 tag_len,
				       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_per_rate_stats_tlv *stats_buf = tag_buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 ru_size_cnt = 0;
	u32 rc_mode, ru_type;
	u8 *buf = stats_req->buf, i;
	const char *mode_prefix;

	if (tag_len < sizeof(*stats_buf))
		return;

	rc_mode = le32_to_cpu(stats_buf->rc_mode);
	ru_type = le32_to_cpu(stats_buf->ru_type);

	switch (rc_mode) {
	case ATH12K_HTT_STATS_RC_MODE_DLSU:
		len += scnprintf(buf + len, buf_len - len, "HTT_TX_PER_STATS:\n");
		len += scnprintf(buf + len, buf_len - len, "\nPER_STATS_SU:\n");
		mode_prefix = "su";
		break;
	case ATH12K_HTT_STATS_RC_MODE_DLMUMIMO:
		len += scnprintf(buf + len, buf_len - len, "\nPER_STATS_DL_MUMIMO:\n");
		mode_prefix = "mu";
		break;
	case ATH12K_HTT_STATS_RC_MODE_DLOFDMA:
		len += scnprintf(buf + len, buf_len - len, "\nPER_STATS_DL_OFDMA:\n");
		mode_prefix = "ofdma";
		if (ru_type == ATH12K_HTT_STATS_RU_TYPE_SINGLE_RU_ONLY)
			ru_size_cnt = ATH12K_HTT_TX_RX_PDEV_STATS_NUM_AX_RU_SIZE_CNTRS;
		else if (ru_type == ATH12K_HTT_STATS_RU_TYPE_SINGLE_AND_MULTI_RU)
			ru_size_cnt = ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS;
		break;
	case ATH12K_HTT_STATS_RC_MODE_ULMUMIMO:
		len += scnprintf(buf + len, buf_len - len, "HTT_RX_PER_STATS:\n");
		len += scnprintf(buf + len, buf_len - len, "\nPER_STATS_UL_MUMIMO:\n");
		mode_prefix = "ulmu";
		break;
	case ATH12K_HTT_STATS_RC_MODE_ULOFDMA:
		len += scnprintf(buf + len, buf_len - len, "\nPER_STATS_UL_OFDMA:\n");
		mode_prefix = "ulofdma";
		if (ru_type == ATH12K_HTT_STATS_RU_TYPE_SINGLE_RU_ONLY)
			ru_size_cnt = ATH12K_HTT_TX_RX_PDEV_STATS_NUM_AX_RU_SIZE_CNTRS;
		else if (ru_type == ATH12K_HTT_STATS_RU_TYPE_SINGLE_AND_MULTI_RU)
			ru_size_cnt = ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS;
		break;
	default:
		return;
	}

	len += scnprintf(buf + len, buf_len - len, "\nPER per BW:\n");
	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA ||
	    rc_mode == ATH12K_HTT_STATS_RC_MODE_ULMUMIMO)
		len += scnprintf(buf + len, buf_len - len, "data_ppdus_%s = ",
				 mode_prefix);
	else
		len += scnprintf(buf + len, buf_len - len, "ppdus_tried_%s = ",
				 mode_prefix);
	for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_BW_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				 le32_to_cpu(stats_buf->per_bw[i].ppdus_tried));
	len += scnprintf(buf + len, buf_len - len, " %u:%u\n", i,
			 le32_to_cpu(stats_buf->per_bw320.ppdus_tried));

	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA ||
	    rc_mode == ATH12K_HTT_STATS_RC_MODE_ULMUMIMO)
		len += scnprintf(buf + len, buf_len - len, "non_data_ppdus_%s = ",
				 mode_prefix);
	else
		len += scnprintf(buf + len, buf_len - len, "ppdus_ack_failed_%s = ",
				 mode_prefix);
	for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_BW_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				 le32_to_cpu(stats_buf->per_bw[i].ppdus_ack_failed));
	len += scnprintf(buf + len, buf_len - len, " %u:%u\n", i,
			 le32_to_cpu(stats_buf->per_bw320.ppdus_ack_failed));

	len += scnprintf(buf + len, buf_len - len, "mpdus_tried_%s = ", mode_prefix);
	for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_BW_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				 le32_to_cpu(stats_buf->per_bw[i].mpdus_tried));
	len += scnprintf(buf + len, buf_len - len, " %u:%u\n", i,
			 le32_to_cpu(stats_buf->per_bw320.mpdus_tried));

	len += scnprintf(buf + len, buf_len - len, "mpdus_failed_%s = ", mode_prefix);
	for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_BW_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u", i,
				 le32_to_cpu(stats_buf->per_bw[i].mpdus_failed));
	len += scnprintf(buf + len, buf_len - len, " %u:%u\n", i,
			 le32_to_cpu(stats_buf->per_bw320.mpdus_failed));

	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_DLSU) {
		len += scnprintf(buf + len, buf_len - len, "\nPER per punctured_mode:\n");

		len += scnprintf(buf + len, buf_len - len, "ppdus_tried_%s =",
				 mode_prefix);
		for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS; i++)
			len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				le32_to_cpu
				(stats_buf->per_tx_su_punctured_mode[i].ppdus_tried));

		len += scnprintf(buf + len, buf_len - len, "\nppdus_ack_failed_%s =",
				 mode_prefix);
		for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS; i++)
			len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				le32_to_cpu
				(stats_buf->per_tx_su_punctured_mode[i].ppdus_ack_failed)
				);

		len += scnprintf(buf + len, buf_len - len, "\nmpdus_tried_%s =",
				 mode_prefix);
		for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS; i++)
			len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				le32_to_cpu
				(stats_buf->per_tx_su_punctured_mode[i].mpdus_tried));

		len += scnprintf(buf + len, buf_len - len, "\nmpdus_failed_%s =",
				 mode_prefix);
		for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS; i++)
			len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				le32_to_cpu
				(stats_buf->per_tx_su_punctured_mode[i].mpdus_failed));
	}

	len += scnprintf(buf + len, buf_len - len, "\n\nPER per NSS:\n");
	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA ||
	    rc_mode == ATH12K_HTT_STATS_RC_MODE_ULMUMIMO)
		len += scnprintf(buf + len, buf_len - len, "data_ppdus_%s = ",
				 mode_prefix);
	else
		len += scnprintf(buf + len, buf_len - len, "ppdus_tried_%s = ",
				 mode_prefix);
	for (i = 0; i < ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i + 1,
				 le32_to_cpu(stats_buf->per_nss[i].ppdus_tried));
	len += scnprintf(buf + len, buf_len - len, "\n");

	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA ||
	    rc_mode == ATH12K_HTT_STATS_RC_MODE_ULMUMIMO)
		len += scnprintf(buf + len, buf_len - len, "non_data_ppdus_%s = ",
				 mode_prefix);
	else
		len += scnprintf(buf + len, buf_len - len, "ppdus_ack_failed_%s = ",
				 mode_prefix);
	for (i = 0; i < ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i + 1,
				 le32_to_cpu(stats_buf->per_nss[i].ppdus_ack_failed));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "mpdus_tried_%s = ", mode_prefix);
	for (i = 0; i < ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i + 1,
				 le32_to_cpu(stats_buf->per_nss[i].mpdus_tried));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "mpdus_failed_%s = ", mode_prefix);
	for (i = 0; i < ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i + 1,
				 le32_to_cpu(stats_buf->per_nss[i].mpdus_failed));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "\nPER per MCS:\n");
	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA ||
	    rc_mode == ATH12K_HTT_STATS_RC_MODE_ULMUMIMO)
		len += scnprintf(buf + len, buf_len - len, "data_ppdus_%s = ",
				 mode_prefix);
	else
		len += scnprintf(buf + len, buf_len - len, "ppdus_tried_%s = ",
				 mode_prefix);
	for (i = 0; i < ATH12K_HTT_TXBF_RATE_STAT_NUM_MCS_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				 le32_to_cpu(stats_buf->per_mcs[i].ppdus_tried));
	len += scnprintf(buf + len, buf_len - len, "\n");

	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA ||
	    rc_mode == ATH12K_HTT_STATS_RC_MODE_ULMUMIMO)
		len += scnprintf(buf + len, buf_len - len, "non_data_ppdus_%s = ",
				 mode_prefix);
	else
		len += scnprintf(buf + len, buf_len - len, "ppdus_ack_failed_%s = ",
				 mode_prefix);
	for (i = 0; i < ATH12K_HTT_TXBF_RATE_STAT_NUM_MCS_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				 le32_to_cpu(stats_buf->per_mcs[i].ppdus_ack_failed));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "mpdus_tried_%s = ", mode_prefix);
	for (i = 0; i < ATH12K_HTT_TXBF_RATE_STAT_NUM_MCS_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				 le32_to_cpu(stats_buf->per_mcs[i].mpdus_tried));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "mpdus_failed_%s = ", mode_prefix);
	for (i = 0; i < ATH12K_HTT_TXBF_RATE_STAT_NUM_MCS_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u ", i,
				 le32_to_cpu(stats_buf->per_mcs[i].mpdus_failed));
	len += scnprintf(buf + len, buf_len - len, "\n");

	if ((rc_mode == ATH12K_HTT_STATS_RC_MODE_DLOFDMA ||
	     rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA) &&
	     ru_type != ATH12K_HTT_STATS_RU_TYPE_INVALID) {
		len += scnprintf(buf + len, buf_len - len, "\nPER per RU:\n");

		if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA)
			len += scnprintf(buf + len, buf_len - len, "data_ppdus_%s = ",
					 mode_prefix);
		else
			len += scnprintf(buf + len, buf_len - len, "ppdus_tried_%s = ",
					 mode_prefix);
		for (i = 0; i < ru_size_cnt; i++)
			len += scnprintf(buf + len, buf_len - len, " %s:%u ",
					 ath12k_tx_ru_size_to_str(ru_type, i),
					 le32_to_cpu(stats_buf->ru[i].ppdus_tried));
		len += scnprintf(buf + len, buf_len - len, "\n");

		if (rc_mode == ATH12K_HTT_STATS_RC_MODE_ULOFDMA)
			len += scnprintf(buf + len, buf_len - len,
					 "non_data_ppdus_%s = ", mode_prefix);
		else
			len += scnprintf(buf + len, buf_len - len,
					 "ppdus_ack_failed_%s = ", mode_prefix);
		for (i = 0; i < ru_size_cnt; i++)
			len += scnprintf(buf + len, buf_len - len, " %s:%u ",
					 ath12k_tx_ru_size_to_str(ru_type, i),
					 le32_to_cpu(stats_buf->ru[i].ppdus_ack_failed));
		len += scnprintf(buf + len, buf_len - len, "\n");

		len += scnprintf(buf + len, buf_len - len, "mpdus_tried_%s = ",
				 mode_prefix);
		for (i = 0; i < ru_size_cnt; i++)
			len += scnprintf(buf + len, buf_len - len, " %s:%u ",
					 ath12k_tx_ru_size_to_str(ru_type, i),
					 le32_to_cpu(stats_buf->ru[i].mpdus_tried));
		len += scnprintf(buf + len, buf_len - len, "\n");

		len += scnprintf(buf + len, buf_len - len, "mpdus_failed_%s = ",
				 mode_prefix);
		for (i = 0; i < ru_size_cnt; i++)
			len += scnprintf(buf + len, buf_len - len, " %s:%u ",
					 ath12k_tx_ru_size_to_str(ru_type, i),
					 le32_to_cpu(stats_buf->ru[i].mpdus_failed));
		len += scnprintf(buf + len, buf_len - len, "\n\n");
	}

	if (rc_mode == ATH12K_HTT_STATS_RC_MODE_DLMUMIMO) {
		len += scnprintf(buf + len, buf_len - len, "\nlast_probed_bw  = %u\n",
				 le32_to_cpu(stats_buf->last_probed_bw));
		len += scnprintf(buf + len, buf_len - len, "last_probed_nss = %u\n",
				 le32_to_cpu(stats_buf->last_probed_nss));
		len += scnprintf(buf + len, buf_len - len, "last_probed_mcs = %u\n",
				 le32_to_cpu(stats_buf->last_probed_mcs));
		len += print_array_to_buf(buf, len, "MU Probe count per RC MODE",
					  stats_buf->probe_cnt,
					  ATH12K_HTT_RC_MODE_2D_COUNT, "\n\n");
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ast_entry_tlv(const void *tag_buf, u16 tag_len,
			       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_ast_entry_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u32 mac_addr_l32;
	u32 mac_addr_h16;
	u32 ast_info;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_addr_l32 = le32_to_cpu(htt_stats_buf->mac_addr.mac_addr_l32);
	mac_addr_h16 = le32_to_cpu(htt_stats_buf->mac_addr.mac_addr_h16);
	ast_info = le32_to_cpu(htt_stats_buf->info);

	len += scnprintf(buf + len, buf_len - len, "HTT_AST_ENTRY_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ast_index = %u\n",
			 le32_to_cpu(htt_stats_buf->ast_index));
	len += scnprintf(buf + len, buf_len - len,
			 "mac_addr = %02x:%02x:%02x:%02x:%02x:%02x\n",
			 u32_get_bits(mac_addr_l32, ATH12K_HTT_MAC_ADDR_L32_0),
			 u32_get_bits(mac_addr_l32, ATH12K_HTT_MAC_ADDR_L32_1),
			 u32_get_bits(mac_addr_l32, ATH12K_HTT_MAC_ADDR_L32_2),
			 u32_get_bits(mac_addr_l32, ATH12K_HTT_MAC_ADDR_L32_3),
			 u32_get_bits(mac_addr_h16, ATH12K_HTT_MAC_ADDR_H16_0),
			 u32_get_bits(mac_addr_h16, ATH12K_HTT_MAC_ADDR_H16_1));

	len += scnprintf(buf + len, buf_len - len, "sw_peer_id = %u\n",
			 le32_to_cpu(htt_stats_buf->sw_peer_id));
	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_PDEV_ID_INFO));
	len += scnprintf(buf + len, buf_len - len, "vdev_id = %u\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_VDEV_ID_INFO));
	len += scnprintf(buf + len, buf_len - len, "next_hop = %u\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_NEXT_HOP_INFO));
	len += scnprintf(buf + len, buf_len - len, "mcast = %u\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_MCAST_INFO));
	len += scnprintf(buf + len, buf_len - len, "monitor_direct = %u\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_MONITOR_DIRECT_INFO));
	len += scnprintf(buf + len, buf_len - len, "mesh_sta = %u\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_MESH_STA_INFO));
	len += scnprintf(buf + len, buf_len - len, "mec = %u\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_MEC_INFO));
	len += scnprintf(buf + len, buf_len - len, "intra_bss = %u\n\n",
			 u32_get_bits(ast_info, ATH12K_HTT_AST_INTRA_BSS_INFO));

	stats_req->buf_len = len;
}

static const char*
ath12k_htt_get_punct_dir_type_str(enum ath12k_htt_stats_direction direction)
{
	switch (direction) {
	case ATH12K_HTT_STATS_DIRECTION_TX:
		return "tx";
	case ATH12K_HTT_STATS_DIRECTION_RX:
		return "rx";
	default:
		return "unknown";
	}
}

static const char*
ath12k_htt_get_punct_ppdu_type_str(enum ath12k_htt_stats_ppdu_type ppdu_type)
{
	switch (ppdu_type) {
	case ATH12K_HTT_STATS_PPDU_TYPE_MODE_SU:
		return "su";
	case ATH12K_HTT_STATS_PPDU_TYPE_DL_MU_MIMO:
		return "dl_mu_mimo";
	case ATH12K_HTT_STATS_PPDU_TYPE_UL_MU_MIMO:
		return "ul_mu_mimo";
	case ATH12K_HTT_STATS_PPDU_TYPE_DL_MU_OFDMA:
		return "dl_mu_ofdma";
	case ATH12K_HTT_STATS_PPDU_TYPE_UL_MU_OFDMA:
		return "ul_mu_ofdma";
	default:
		return "unknown";
	}
}

static const char*
ath12k_htt_get_punct_pream_type_str(enum ath12k_htt_stats_param_type pream_type)
{
	switch (pream_type) {
	case ATH12K_HTT_STATS_PREAM_OFDM:
		return "ofdm";
	case ATH12K_HTT_STATS_PREAM_CCK:
		return "cck";
	case ATH12K_HTT_STATS_PREAM_HT:
		return "ht";
	case ATH12K_HTT_STATS_PREAM_VHT:
		return "ac";
	case ATH12K_HTT_STATS_PREAM_HE:
		return "ax";
	case ATH12K_HTT_STATS_PREAM_EHT:
		return "be";
	default:
		return "unknown";
	}
}

static void
ath12k_htt_print_puncture_stats_tlv(const void *tag_buf, u16 tag_len,
				    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_puncture_stats_tlv *stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	const char *direction;
	const char *ppdu_type;
	const char *preamble;
	u32 mac_id__word;
	u32 subband_limit;
	u8 i;

	if (tag_len < sizeof(*stats_buf))
		return;

	mac_id__word = le32_to_cpu(stats_buf->mac_id__word);
	subband_limit = min(le32_to_cpu(stats_buf->subband_cnt),
			    ATH12K_HTT_PUNCT_STATS_MAX_SUBBAND_CNT);

	direction = ath12k_htt_get_punct_dir_type_str(le32_to_cpu(stats_buf->direction));
	ppdu_type = ath12k_htt_get_punct_ppdu_type_str(le32_to_cpu(stats_buf->ppdu_type));
	preamble = ath12k_htt_get_punct_pream_type_str(le32_to_cpu(stats_buf->preamble));

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_PUNCTURE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id__word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len,
			 "%s_%s_%s_last_used_pattern_mask = 0x%08x\n",
			 direction, preamble, ppdu_type,
			 le32_to_cpu(stats_buf->last_used_pattern_mask));

	for (i = 0; i < subband_limit; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "%s_%s_%s_num_subbands_used_cnt_%02d = %u\n",
				 direction, preamble, ppdu_type, i + 1,
				 le32_to_cpu(stats_buf->num_subbands_used_cnt[i]));
	}
	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_dmac_reset_stats_tlv(const void *tag_buf, u16 tag_len,
				      struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_dmac_reset_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u64 time;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_DMAC_RESET_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "reset_count = %u\n",
			 le32_to_cpu(htt_stats_buf->reset_count));
	time = ath12k_le32hilo_to_u64(htt_stats_buf->reset_time_hi_ms,
				      htt_stats_buf->reset_time_lo_ms);
	len += scnprintf(buf + len, buf_len - len, "reset_time_ms = %llu\n", time);
	time = ath12k_le32hilo_to_u64(htt_stats_buf->disengage_time_hi_ms,
				      htt_stats_buf->disengage_time_lo_ms);
	len += scnprintf(buf + len, buf_len - len, "disengage_time_ms = %llu\n", time);

	time = ath12k_le32hilo_to_u64(htt_stats_buf->engage_time_hi_ms,
				      htt_stats_buf->engage_time_lo_ms);
	len += scnprintf(buf + len, buf_len - len, "engage_time_ms = %llu\n", time);

	len += scnprintf(buf + len, buf_len - len, "disengage_count = %u\n",
			 le32_to_cpu(htt_stats_buf->disengage_count));
	len += scnprintf(buf + len, buf_len - len, "engage_count = %u\n",
			 le32_to_cpu(htt_stats_buf->engage_count));
	len += scnprintf(buf + len, buf_len - len, "drain_dest_ring_mask = 0x%x\n\n",
			 le32_to_cpu(htt_stats_buf->drain_dest_ring_mask));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_ppdu_dur_stats_tlv(const void *tag_buf, u16 tag_len,
					    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_ppdu_dur_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i, j;
	u32 total_ppdu_dur_hist_bins;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_PPDU_DUR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "tx_success_time_us_low = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_success_time_us_low));
	len += scnprintf(buf + len, buf_len - len, "tx_success_time_us_high = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_success_time_us_high));
	len += scnprintf(buf + len, buf_len - len, "tx_fail_time_us_low = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_fail_time_us_low));
	len += scnprintf(buf + len, buf_len - len, "tx_fail_time_us_high = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_fail_time_us_high));
	len += scnprintf(buf + len, buf_len - len, "pdev_up_time_us_low = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_up_time_us_low));
	len += scnprintf(buf + len, buf_len - len, "pdev_up_time_us_high = %u\n",
			 le32_to_cpu(htt_stats_buf->pdev_up_time_us_high));

	total_ppdu_dur_hist_bins = ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS +
				   ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_EXT_BINS;

	len += scnprintf(buf + len, buf_len - len, "tx_ppdu_dur_hist_us =");
	for (i = 0; i < total_ppdu_dur_hist_bins ; i++) {
		if (i < ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS) {
			len += scnprintf(buf + len, buf_len - len, " %u-%u : %u,",
					 i *
					 ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
					 (i + 1) *
					 ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
					 htt_stats_buf->tx_ppdu_dur_hist[i]);
		} else {
			j = i - ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS;
			len += scnprintf(buf + len, buf_len - len, " %u-%u : %u,",
					 i *
					 ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
					 (i + 1) *
					 ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
					 htt_stats_buf->tx_ppdu_dur_hist_ext[j]);
		}
	}

	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "tx_ofdma_ppdu_dur_hist_us =");
	for (i = 0; i < ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS ; i++) {
		len += scnprintf(buf + len, buf_len - len, " %u-%u : %u,",
				 i * ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
				 (i + 1) *
				 ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
				 htt_stats_buf->tx_ofdma_ppdu_dur_hist[i]);
	}
	len += scnprintf(buf + len, buf_len - len, "\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_sched_algo_ofdma_stats_tlv(const void *tag_buf, u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_sched_algo_ofdma_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_SCHED_ALGO_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += print_array_to_buf(buf, len, "rate_based_dlofdma_enabled_count",
				  htt_stats_buf->rate_based_dlofdma_enabled_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "rate_based_dlofdma_disabled_count",
				  htt_stats_buf->rate_based_dlofdma_disabled_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "rate_based_dlofdma_probing_count",
				  htt_stats_buf->rate_based_dlofdma_disabled_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "rate_based_dlofdma_monitoring_count",
				  htt_stats_buf->rate_based_dlofdma_monitor_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "chan_acc_lat_based_dlofdma_enabled_count",
				  htt_stats_buf->chan_acc_lat_based_dlofdma_enabled_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "chan_acc_lat_based_dlofdma_disabled_count",
				  htt_stats_buf->chan_acc_lat_based_dlofdma_disabled_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "chan_acc_lat_based_dlofdma_monitoring_count",
				  htt_stats_buf->chan_acc_lat_based_dlofdma_monitor_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "downgrade_to_dl_su_ru_alloc_fail",
				  htt_stats_buf->downgrade_to_dl_su_ru_alloc_fail,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "candidate_list_single_user_disable_ofdma",
				  htt_stats_buf->candidate_list_single_user_disable_ofdma,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "dl_cand_list_dropped_high_ul_qos_weight",
				  htt_stats_buf->dl_cand_list_dropped_high_ul_qos_weight,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "ax_dlofdma_disabled_due_to_pipelining",
				  htt_stats_buf->ax_dlofdma_disabled_due_to_pipelining,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "dlofdma_disabled_su_only_eligible",
				  htt_stats_buf->dlofdma_disabled_su_only_eligible,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "dlofdma_disabled_consec_no_mpdus_tried",
				  htt_stats_buf->dlofdma_disabled_consec_no_mpdus_tried,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "dlofdma_disabled_consec_no_mpdus_success",
				  htt_stats_buf->dlofdma_disabled_consec_no_mpdus_success,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "txbf_ofdma_ineligibility_stat",
				  htt_stats_buf->txbf_ofdma_ineligibility_stat,
				  ATH12K_SCHED_OFDMA_TXBF_INELIGIBILITY_MAX, "\n");
	len += print_array_to_buf(buf, len, "avg_chan_acc_lat_hist",
				  htt_stats_buf->avg_chan_acc_lat_hist,
				  ATH12K_HTT_MAX_NUM_CHAN_ACC_LAT_INTR, "\n");
	len += print_array_to_buf(buf, len, "dl_ofdma_nbinwb_selected_over_mu_mimo",
				  htt_stats_buf->dl_ofdma_nbinwb_selected_over_mu_mimo,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "dl_ofdma_nbinwb_selected_standalone",
				  htt_stats_buf->dl_ofdma_nbinwb_selected_standalone,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "running_only_dl_scheduler_cnt",
				  htt_stats_buf->running_only_dl_scheduler_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "running_only_ul_scheduler_cnt",
				  htt_stats_buf->running_only_ul_scheduler_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "running_additional_dl_scheduler_cnt",
				  htt_stats_buf->running_additional_dl_scheduler_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "running_additional_ul_scheduler_cnt",
				  htt_stats_buf->running_additional_ul_scheduler_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "running_ul_scheduler_for_bsrp_cnt",
				  htt_stats_buf->running_ul_scheduler_for_bsrp_cnt,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "running_dl_scheduler_due_to_skip_ul",
				  htt_stats_buf->running_dl_scheduler_due_to_skip_ul,
				  ATH12K_HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "running_ul_scheduler_due_to_skip_dl",
				  htt_stats_buf->running_ul_scheduler_due_to_skip_dl,
				  ATH12K_HTT_NUM_AC_WMM, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_rate_stats_be_bn_ofdma_tlv(const void *tag_buf, u16 tag_len,
						    struct debug_htt_stats_req
						    *stats_req)
{
	const struct ath12k_htt_tx_pdev_rate_stats_be_ofdma_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;
	u8 i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_RATE_STATS_BE_OFDMA_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "be_ofdma_tx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->be_ofdma_tx_ldpc));
	len += print_array_to_buf(buf, len, "be_ofdma_tx_mcs",
				  htt_stats_buf->be_ofdma_tx_mcs,
				  ATH12K_HTT_TX_PDEV_NUM_BE_MCS_CNTRS, "\n");
	len += print_array_to_buf(buf, len, "be_ofdma_eht_sig_mcs",
				  htt_stats_buf->be_ofdma_eht_sig_mcs,
				  ATH12K_HTT_TX_PDEV_NUM_EHT_SIG_MCS_CNTRS, "\n");
	len += scnprintf(buf + len, buf_len - len, "be_ofdma_tx_ru_size = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(htt_stats_buf->be_ofdma_tx_ru_size[i]));
	len += scnprintf(buf + len, buf_len - len, "\n");
	len += print_array_to_buf_index(buf, len, "be_ofdma_tx_nss = ", 1,
					htt_stats_buf->be_ofdma_tx_nss,
					ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS,
					"\n");
	len += print_array_to_buf(buf, len, "be_ofdma_tx_bw",
				  htt_stats_buf->be_ofdma_tx_bw,
				  ATH12K_HTT_TX_PDEV_NUM_BE_BW_CNTRS, "\n");
	for (i = 0; i < ATH12K_HTT_TX_PDEV_NUM_GI_CNTRS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_ofdma_tx_gi[%u]", i);
		len += print_array_to_buf(buf, len, "", htt_stats_buf->gi[i],
					  ATH12K_HTT_TX_PDEV_NUM_BE_MCS_CNTRS, "\n");
	}

	len += scnprintf(buf + len, buf_len - len, "be_ofdma_ba_ru_size = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(htt_stats_buf->be_ofdma_ba_ru_size[i]));
	len += scnprintf(buf + len, buf_len - len, "\n\n");

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_TX_PDEV_RATE_STATS_BN_OFDMA_TLV:\n");

	len += scnprintf(buf + len, buf_len - len, "bn_ofdma_tx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->bn_ofdma_tx_ldpc));
	len += print_array_to_buf(buf, len, "bn_ofdma_tx_mcs",
				  htt_stats_buf->bn_ofdma_tx_mcs,
				  ATH12K_HTT_TX_PDEV_NUM_BN_MCS_CNTRS, "\n");
	len += print_array_to_buf(buf, len, "bn_ofdma_uhr_sig_mcs",
				  htt_stats_buf->bn_ofdma_uhr_sig_mcs,
				  ATH12K_HTT_TX_PDEV_NUM_UHR_SIG_MCS_CNTRS, "\n");
	len += scnprintf(buf + len, buf_len - len, "bn_ofdma_tx_ru_size = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(htt_stats_buf->bn_ofdma_tx_ru_size[i]));
	len += scnprintf(buf + len, buf_len - len, "\n");
	len += print_array_to_buf_index(buf, len, "bn_ofdma_tx_nss = ", 1,
					htt_stats_buf->bn_ofdma_tx_nss,
					ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS,
					"\n");
	len += print_array_to_buf(buf, len, "bn_ofdma_tx_bw",
				  htt_stats_buf->bn_ofdma_tx_bw,
				  ATH12K_HTT_TX_PDEV_NUM_BN_BW_CNTRS, "\n");
	for (i = 0; i < ATH12K_HTT_TX_PDEV_NUM_GI_CNTRS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "bn_ofdma_tx_gi[%u]", i);
		len += print_array_to_buf(buf, len, "", htt_stats_buf->gi_bn[i],
					  ATH12K_HTT_TX_PDEV_NUM_BN_MCS_CNTRS, "\n");
	}

	len += scnprintf(buf + len, buf_len - len, "bn_ofdma_ba_ru_size = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(htt_stats_buf->bn_ofdma_ba_ru_size[i]));
	len += scnprintf(buf + len, buf_len - len, "\n\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_mbssid_ctrl_frame_stats_tlv(const void *tag_buf, u16 tag_len,
						  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_mbssid_ctrl_frame_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id__word);

	len += scnprintf(buf + len, buf_len - len, "HTT_MBSSID_CTRL_FRAME_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "basic_trigger_across_bss = %u\n",
			 le32_to_cpu(htt_stats_buf->basic_trigger_across_bss));
	len += scnprintf(buf + len, buf_len - len, "basic_trigger_within_bss = %u\n",
			 le32_to_cpu(htt_stats_buf->basic_trigger_within_bss));
	len += scnprintf(buf + len, buf_len - len, "bsr_trigger_across_bss = %u\n",
			 le32_to_cpu(htt_stats_buf->bsr_trigger_across_bss));
	len += scnprintf(buf + len, buf_len - len, "bsr_trigger_within_bss = %u\n",
			 le32_to_cpu(htt_stats_buf->bsr_trigger_within_bss));
	len += scnprintf(buf + len, buf_len - len, "mu_rts_across_bss = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_rts_across_bss));
	len += scnprintf(buf + len, buf_len - len, "mu_rts_within_bss = %u\n",
			 le32_to_cpu(htt_stats_buf->mu_rts_within_bss));
	len += scnprintf(buf + len, buf_len - len, "ul_mumimo_trigger_across_bss = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_mumimo_trigger_across_bss));
	len += scnprintf(buf + len, buf_len - len,
			 "ul_mumimo_trigger_within_bss = %u\n\n",
			 le32_to_cpu(htt_stats_buf->ul_mumimo_trigger_within_bss));

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_rate_stats_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_rate_stats_tlv *htt_stats_buf = tag_buf;
	u32 tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS + 1] = { 0 };
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i, j;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id_word);

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_RATE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "tx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_ldpc));
	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_tx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->ac_mu_mimo_tx_ldpc));
	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_tx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->ax_mu_mimo_tx_ldpc));
	len += scnprintf(buf + len, buf_len - len, "ofdma_tx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->ofdma_tx_ldpc));
	len += scnprintf(buf + len, buf_len - len, "rts_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rts_cnt));
	len += scnprintf(buf + len, buf_len - len, "rts_success = %u\n",
			 le32_to_cpu(htt_stats_buf->rts_success));
	len += scnprintf(buf + len, buf_len - len, "ack_rssi = %u\n",
			 le32_to_cpu(htt_stats_buf->ack_rssi));
	len += scnprintf(buf + len, buf_len - len, "tx_11ax_su_ext = %u\n",
			 le32_to_cpu(htt_stats_buf->tx_11ax_su_ext));
	len += scnprintf(buf + len, buf_len - len,
			 "Legacy CCK Rates: 1 Mbps: %u, 2 Mbps: %u, 5.5 Mbps: %u, 12 Mbps: %u\n",
			 le32_to_cpu(htt_stats_buf->tx_legacy_cck_rate[0]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_cck_rate[1]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_cck_rate[2]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_cck_rate[3]));
	len += scnprintf(buf + len, buf_len - len,
			 "Legacy OFDM Rates: 6 Mbps: %u, 9 Mbps: %u, 12 Mbps: %u, 18 Mbps: %u\n"
			 "                   24 Mbps: %u, 36 Mbps: %u, 48 Mbps: %u, 54 Mbps: %u\n",
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[0]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[1]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[2]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[3]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[4]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[5]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[6]),
			 le32_to_cpu(htt_stats_buf->tx_legacy_ofdm_rate[7]));
	len += scnprintf(buf + len, buf_len - len, "HE LTF: 1x: %u, 2x: %u, 4x: %u\n",
			 le32_to_cpu(htt_stats_buf->tx_he_ltf[1]),
			 le32_to_cpu(htt_stats_buf->tx_he_ltf[2]),
			 le32_to_cpu(htt_stats_buf->tx_he_ltf[3]));

	len += scnprintf(buf + len, buf_len - len, "tx_mcs =");

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %d:%u",
				 j - ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS,
				 le32_to_cpu(htt_stats_buf->tx_mcs_ext_2[j]));

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %u:%u",
				 j, le32_to_cpu(htt_stats_buf->tx_mcs[j]));

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %u:%u",
				 j + ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
				 le32_to_cpu(htt_stats_buf->tx_mcs_ext[j]));

	len += scnprintf(buf + len, buf_len - len, "\n");

	len += print_array_to_buf(buf, len, "ax_mu_mimo_tx_mcs",
				  htt_stats_buf->ax_mu_mimo_tx_mcs,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, NULL);
	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %u:%u",
				 j + ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
				 le32_to_cpu(htt_stats_buf->ax_mu_mimo_tx_mcs_ext[j]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += print_array_to_buf(buf, len, "ofdma_tx_mcs",
				  htt_stats_buf->ofdma_tx_mcs,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, NULL);
	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %u:%u",
				 j + ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
				 le32_to_cpu(htt_stats_buf->ofdma_tx_mcs_ext[j]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "tx_nss =");
	for (j = 1; j <= ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,",
				 j, le32_to_cpu(htt_stats_buf->tx_nss[j - 1]));
	len--;
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "ac_mu_mimo_tx_nss =");
	for (j = 1; j <= ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,",
				 j, le32_to_cpu(htt_stats_buf->ac_mu_mimo_tx_nss[j - 1]));
	len--;
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "ax_mu_mimo_tx_nss =");
	for (j = 1; j <= ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,",
				 j, le32_to_cpu(htt_stats_buf->ax_mu_mimo_tx_nss[j - 1]));
	len--;
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "ofdma_tx_nss =");
	for (j = 1; j <= ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++)
		len += scnprintf(buf + len, buf_len - len, " %u:%u,",
				 j, le32_to_cpu(htt_stats_buf->ofdma_tx_nss[j - 1]));
	len--;
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += print_array_to_buf(buf, len, "tx_bw", htt_stats_buf->tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, NULL);
	len += scnprintf(buf + len, buf_len - len, ", %u:%u\n",
			 ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS,
			 le32_to_cpu(htt_stats_buf->tx_bw_320mhz));

	len += print_array_to_buf(buf, len, "tx_stbc",
				  htt_stats_buf->tx_stbc,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, NULL);
	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %u:%u",
				 j + ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
				 le32_to_cpu(htt_stats_buf->tx_stbc_ext[j]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, (buf_len - len),
				 "tx_gi[%u] =", j);
		len += scnprintf(buf + len, buf_len - len, " -2:%u,-1:%u,",
				 htt_stats_buf->tx_gi_ext_2[j][0],
				 htt_stats_buf->tx_gi_ext_2[j][1]);
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->tx_gi[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
					  NULL);
		for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; i++)
			len += scnprintf(buf + len, buf_len - len, ", %u:%u",
					 i + ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
					 le32_to_cpu(htt_stats_buf->tx_gi_ext[j][i]));
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, (buf_len - len),
				 "ac_mu_mimo_tx_gi[%u] =", j);
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->ac_mu_mimo_tx_gi[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
					  "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, (buf_len - len),
				 "ax_mu_mimo_tx_gi[%u] =", j);
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->ax_mimo_tx_gi[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
					  NULL);
		for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; i++)
			len += scnprintf(buf + len, buf_len - len, ", %u:%u",
					 i + ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
					 le32_to_cpu(htt_stats_buf->ax_tx_gi_ext[j][i]));
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, (buf_len - len),
				 "ofdma_tx_gi[%u] = ", j);
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->ofdma_tx_gi[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
					  NULL);
		for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; i++)
			len += scnprintf(buf + len, buf_len - len, ", %u:%u",
					 i + ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS,
					 le32_to_cpu(htt_stats_buf->ofd_tx_gi_ext[j][i]));
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS; j++) {
		tx_bw[j] = htt_stats_buf->tx_bw[j];
	}
	tx_bw[j] = htt_stats_buf->tx_bw_320mhz;
	len += print_array_to_buf(buf, len, "tx_bw", tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS + 1, "\n");
	len += print_array_to_buf(buf, len, "tx_su_mcs", htt_stats_buf->tx_su_mcs,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "tx_mu_mcs", htt_stats_buf->tx_mu_mcs,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_tx_mcs",
				  htt_stats_buf->ac_mu_mimo_tx_mcs,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "ac_mu_mimo_tx_bw",
				  htt_stats_buf->ac_mu_mimo_tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "ax_mu_mimo_tx_bw",
				  htt_stats_buf->ax_mu_mimo_tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "ofdma_tx_bw",
				  htt_stats_buf->ofdma_tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "tx_pream", htt_stats_buf->tx_pream,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_PREAMBLE_TYPES, "\n");

	len += scnprintf(buf + len, buf_len - len, "ofdma_tx_ru_size = ");
	for (j = 0; j < ATH12K_HTT_TX_RX_PDEV_STATS_NUM_AX_RU_SIZE_CNTRS; j++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_ax_tx_rx_ru_size_to_str(j),
				 htt_stats_buf->ofdma_tx_ru_size[j]);

	len += print_array_to_buf(buf, len, "ofdma_he_sig_b_mcs",
				  htt_stats_buf->ofdma_he_sig_b_mcs,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_HE_SIG_B_MCS_COUNTERS,
				  "\n");

	len += print_array_to_buf(buf, len, "tx_dcm", htt_stats_buf->tx_dcm,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_DCM_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "tx_su_punctured_mode",
				  htt_stats_buf->tx_su_punctured_mode,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS, "\n");

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_tx_bw = " :"quarter_tx_bw = ");
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->reduced_tx_bw[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_ac_mu_mimo_tx_bw = " :"quarter_ac_mu_mimo_tx_bw = ");
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->reduced_ac_mu_mimo_tx_bw[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_ax_mu_mimo_tx_bw = " :"quarter_ax_mu_mimo_tx_bw = ");
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->reduced_ax_mu_mimo_tx_bw[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	}

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_ofdma_tx_bw" :"quarter_ofdma_tx_bw = ");
		len += print_array_to_buf(buf, len, NULL, htt_stats_buf->reduced_ax_mu_ofdma_tx_bw[j],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	}

	len += print_array_to_buf(buf, len, "11ax_trigger_type",
				  htt_stats_buf->trigger_type_11ax,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_11AX_TRIGGER_TYPES, "\n");

	len += print_array_to_buf(buf, len, "11be_trigger_type",
				  htt_stats_buf->trigger_type_11be,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_11BE_TRIGGER_TYPES, "\n");

	len += scnprintf(buf + len, buf_len - len,
			 "ax_su_embedded_trigger_data_ppdu_cnt = %u\n",
			 htt_stats_buf->ax_su_embedded_trigger_data_ppdu);

	len += scnprintf(buf + len, buf_len - len,
			 "ax_su_embedded_trigger_data_ppdu_err_cnt = %u\n",
			 htt_stats_buf->ax_su_embedded_trigger_data_ppdu_err);
	len += scnprintf(buf + len, buf_len - len, "Extra LTF - EHT = %u\n",
			 le32_to_cpu(htt_stats_buf->extra_eht_ltf));
	len += scnprintf(buf + len, buf_len - len, "Extra LTF - OFDMA = %u\n",
			 le32_to_cpu(htt_stats_buf->extra_eht_ltf_ofdma));
	len += scnprintf(buf + len, buf_len - len, "ofdma_ba_ru_size = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_STATS_NUM_AX_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_ax_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(htt_stats_buf->ofdma_ba_ru_size[i]));
	len += scnprintf(buf + len, buf_len - len, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_be_bn_ul_trigger_stats_tlv(const void *tag_buf, u16 tag_len,
					    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_pdev_be_ul_trig_stats_tlv *stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;

	if (tag_len < sizeof(*stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_RX_PDEV_BE_BN_UL_TRIGGER_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			u32_get_bits(le32_to_cpu(stats_buf->mac_id_word),
					ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "rx_11be_ul_ofdma = %u\n",
			 le32_to_cpu(stats_buf->rx_11be_ul_ofdma));
	len += print_array_to_buf(buf, len, "be_ul_ofdma_rx_mcs",
				  stats_buf->be_ul_ofdma_rx_mcs,
				  ATH12K_HTT_RX_NUM_BE_MCS_COUNTERS, "\n");

	for (i = 0; i < ATH12K_HTT_RX_NUM_GI_COUNTERS; i++) {
		len += scnprintf(buf + len, buf_len - len, "rx_gi[%u] = ", i);
		len += print_array_to_buf(buf, len, NULL, stats_buf->be_ul_ofdma_rx_gi[i],
					  ATH12K_HTT_RX_NUM_BE_MCS_COUNTERS, "\n");
	}

	len += print_array_to_buf(buf, len, "be_ul_ofdma_rx_nss",
				  stats_buf->be_ul_ofdma_rx_nss,
				  ATH12K_HTT_RX_NUM_SPATIAL_STREAMS, "\n");
	len += print_array_to_buf(buf, len, "be_ul_ofdma_rx_bw",
				  stats_buf->be_ul_ofdma_rx_bw,
				  ATH12K_HTT_RX_NUM_BE_BW_COUNTERS, "\n");
	len += scnprintf(buf + len, buf_len - len, "be_ul_ofdma_rx_stbc = %u\n",
			 le32_to_cpu(stats_buf->be_ul_ofdma_rx_stbc));
	len += scnprintf(buf + len, buf_len - len, "be_ul_ofdma_rx_ldpc = %u\n",
			 le32_to_cpu(stats_buf->be_ul_ofdma_rx_ldpc));

	len += scnprintf(buf + len, buf_len - len, "be_rx_data_ru_size_ppdu = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(stats_buf->be_rx_data_ru_size_ppdu[i]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "be_rx_non_data_ru_size_ppdu = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(stats_buf->be_rx_non_data_ru_size_ppdu[i]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += print_array_to_buf(buf, len, "be_uplink_sta_aid",
				  stats_buf->be_uplink_sta_aid,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf_s32(buf, len, "be_uplink_sta_target_rssi", 0,
				      stats_buf->be_uplink_sta_target_rssi,
				      ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf_s32(buf, len, "be_uplink_sta_fd_rssi", 0,
				      stats_buf->be_uplink_sta_fd_rssi,
				      ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf(buf, len, "be_uplink_sta_power_headroom",
				  stats_buf->be_uplink_sta_power_headroom,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += scnprintf(buf + len, buf_len - len,
			 "be_ul_ofdma_basic_trigger_rx_qos_null_only = %u\n",
			 le32_to_cpu(stats_buf->be_ul_ofdma_basic_trig_rx_qos_null_only));
	len += scnprintf(buf + len, buf_len - len,
			 "ul_mlo_send_qdepth_params_count = %u\n",
			 le32_to_cpu(stats_buf->ul_mlo_send_qdepth_params_count));
	len += scnprintf(buf + len, buf_len - len,
			 "ul_mlo_proc_qdepth_params_count = %u\n",
			 le32_to_cpu(stats_buf->ul_mlo_proc_qdepth_params_count));
	len += scnprintf(buf + len, buf_len - len,
			 "ul_mlo_proc_accepted_qdepth_params_count = %u\n",
			 le32_to_cpu(stats_buf->ul_mlo_proc_accepted_qdepth_params_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "ul_mlo_proc_discarded_qdepth_params_count = %u\n",
			 le32_to_cpu(stats_buf->ul_mlo_proc_discarded_qdepth_params_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_11bn_ul_ofdma = %u\n",
			 le32_to_cpu(stats_buf->rx_11bn_ul_ofdma));
	len += print_array_to_buf(buf, len, "bn_ul_ofdma_rx_mcs",
				  stats_buf->bn_ul_ofdma_rx_mcs,
				  ATH12K_HTT_RX_NUM_BN_MCS_COUNTERS, "\n");

	for (i = 0; i < ATH12K_HTT_RX_NUM_GI_COUNTERS; i++) {
		len += scnprintf(buf + len, buf_len - len, "bn_ofdma_rx_gi[%u] = ", i);
		len += print_array_to_buf(buf, len, NULL, stats_buf->bn_ul_ofdma_rx_gi[i],
					  ATH12K_HTT_RX_NUM_BN_MCS_COUNTERS, "\n");
	}

	len += print_array_to_buf(buf, len, "bn_ul_ofdma_rx_nss",
				  stats_buf->bn_ul_ofdma_rx_nss,
				  ATH12K_HTT_RX_NUM_SPATIAL_STREAMS, "\n");
	len += print_array_to_buf(buf, len, "bn_ul_ofdma_rx_bw",
				  stats_buf->bn_ul_ofdma_rx_bw,
				  ATH12K_HTT_RX_NUM_BN_BW_COUNTERS, "\n");
	len += scnprintf(buf + len, buf_len - len, "bn_ul_ofdma_rx_stbc = %u\n",
			 le32_to_cpu(stats_buf->bn_ul_ofdma_rx_stbc));
	len += scnprintf(buf + len, buf_len - len, "bn_ul_ofdma_rx_ldpc = %u\n",
			 le32_to_cpu(stats_buf->bn_ul_ofdma_rx_ldpc));

	len += scnprintf(buf + len, buf_len - len, "bn_rx_data_ru_size_ppdu = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(stats_buf->bn_rx_data_ru_size_ppdu[i]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += scnprintf(buf + len, buf_len - len, "bn_rx_non_data_ru_size_ppdu = ");
	for (i = 0; i < ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS; i++)
		len += scnprintf(buf + len, buf_len - len, " %s:%u ",
				 ath12k_htt_be_tx_rx_ru_size_to_str(i),
				 le32_to_cpu(stats_buf->bn_rx_non_data_ru_size_ppdu[i]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	len += print_array_to_buf(buf, len, "bn_uplink_sta_aid",
				  stats_buf->bn_uplink_sta_aid,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf_s32(buf, len, "bn_uplink_sta_target_rssi", 0,
				      stats_buf->bn_uplink_sta_target_rssi,
				      ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf_s32(buf, len, "bn_uplink_sta_fd_rssi", 0,
				      stats_buf->bn_uplink_sta_fd_rssi,
				      ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += print_array_to_buf(buf, len, "bn_uplink_sta_power_headroom",
				  stats_buf->bn_uplink_sta_power_headroom,
				  ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK, "\n");
	len += scnprintf(buf + len, buf_len - len,
			 "bn_ul_ofdma_basic_trigger_rx_qos_null_only = %u\n",
			 le32_to_cpu(stats_buf->bn_ul_ofdma_basic_trig_rx_qos_null_only));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_be_rate_stats_tlv(const void *tag_buf, u16 tag_len,
					   struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_rate_stats_be_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_BE_RATE_STATS_TLV:\n");
	len += print_array_to_buf(buf, len, "be_mu_mimo_tx_mcs",
				  htt_stats_buf->be_mu_mimo_tx_mcs,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BE_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_mimo_tx_nss",
				  htt_stats_buf->be_mu_mimo_tx_nss,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS, "\n");
	len += print_array_to_buf(buf, len, "be_mu_mimo_tx_bw",
				  htt_stats_buf->be_mu_mimo_tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BE_BW_COUNTERS, "\n");
	for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS; i++) {
		len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_tx_gi[%u]", i);
		len += print_array_to_buf(buf, len, "",
					  htt_stats_buf->be_mu_mimo_tx_gi[i],
					  ATH12K_HTT_TX_PDEV_STATS_NUM_BE_MCS_COUNTERS,
					  "\n");
	}
	len += scnprintf(buf + len, buf_len - len, "be_mu_mimo_tx_ldpc = %u\n\n",
			 le32_to_cpu(htt_stats_buf->be_mu_mimo_tx_ldpc));

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_tx_pdev_sawf_rate_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_tx_pdev_rate_stats_sawf_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_TX_PDEV_SAWF_RATE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "rate_retry_mcs_drop_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rate_retry_mcs_drop_cnt));
	len += scnprintf(buf + len, buf_len - len, "low_latency_rate_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->low_latency_rate_cnt));
	len += scnprintf(buf + len, buf_len - len, "su_burst_rate_drop_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->su_burst_rate_drop_cnt));
	len += scnprintf(buf + len, buf_len - len, "su_burst_rate_drop_fail_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->su_burst_rate_drop_fail_cnt));
	len += print_array_to_buf(buf, len, "mcs_drop_rate",
				  htt_stats_buf->mcs_drop_rate,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_DROP_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "per_histogram_cnt",
				  htt_stats_buf->per_histogram_cnt,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_PER_COUNTERS, "\n\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_pdev_rate_stats_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_pdev_rate_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i, j;
	u32 mac_id_word;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	mac_id_word = le32_to_cpu(htt_stats_buf->mac_id_word);

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_PDEV_RATE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "nsts = %u\n",
			 le32_to_cpu(htt_stats_buf->nsts));
	len += scnprintf(buf + len, buf_len - len, "rx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_ldpc));
	len += scnprintf(buf + len, buf_len - len, "rts_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->rts_cnt));
	len += scnprintf(buf + len, buf_len - len, "rssi_mgmt = %u\n",
			 le32_to_cpu(htt_stats_buf->rssi_mgmt));
	len += scnprintf(buf + len, buf_len - len, "rssi_data = %u\n",
			 le32_to_cpu(htt_stats_buf->rssi_data));
	len += scnprintf(buf + len, buf_len - len, "rssi_comb = %u\n",
			 le32_to_cpu(htt_stats_buf->rssi_comb));
	len += scnprintf(buf + len, buf_len - len, "rssi_in_dbm = %d\n",
			 le32_to_cpu(htt_stats_buf->rssi_in_dbm));
	len += scnprintf(buf + len, buf_len - len, "rx_evm_nss_count = %u\n",
			 le32_to_cpu(htt_stats_buf->nss_count));
	len += scnprintf(buf + len, buf_len - len, "rx_evm_pilot_count = %u\n",
			 le32_to_cpu(htt_stats_buf->pilot_count));
	len += scnprintf(buf + len, buf_len - len, "rx_11ax_su_ext = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_11ax_su_ext));
	len += scnprintf(buf + len, buf_len - len, "rx_11ac_mumimo = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_11ac_mumimo));
	len += scnprintf(buf + len, buf_len - len, "rx_11ax_mumimo = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_11ax_mumimo));
	len += scnprintf(buf + len, buf_len - len, "rx_11ax_ofdma = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_11ax_ofdma));
	len += scnprintf(buf + len, buf_len - len, "txbf = %u\n",
			 le32_to_cpu(htt_stats_buf->txbf));
	len += scnprintf(buf + len, buf_len - len, "rx_su_ndpa = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_su_ndpa));
	len += scnprintf(buf + len, buf_len - len, "rx_mu_ndpa = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_mu_ndpa));
	len += scnprintf(buf + len, buf_len - len, "rx_br_poll = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_br_poll));
	len += scnprintf(buf + len, buf_len - len, "rx_active_dur_us_low = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_active_dur_us_low));
	len += scnprintf(buf + len, buf_len - len, "rx_active_dur_us_high = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_active_dur_us_high));
	len += scnprintf(buf + len, buf_len - len, "rx_11ax_ul_ofdma = %u\n",
			 le32_to_cpu(htt_stats_buf->rx_11ax_ul_ofdma));
	len += scnprintf(buf + len, buf_len - len, "ul_ofdma_rx_stbc = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_ofdma_rx_stbc));
	len += scnprintf(buf + len, buf_len - len, "ul_ofdma_rx_ldpc = %u\n",
			 le32_to_cpu(htt_stats_buf->ul_ofdma_rx_ldpc));
	len += scnprintf(buf + len, buf_len - len, "per_chain_rssi_pkt_type = %#x\n",
			 le32_to_cpu(htt_stats_buf->per_chain_rssi_pkt_type));

	len += print_array_to_buf_index(buf, len, "rx_nss", 1, htt_stats_buf->rx_nss,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS, "\n");
	len += print_array_to_buf(buf, len, "rx_dcm", htt_stats_buf->rx_dcm,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_DCM_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_stbc", htt_stats_buf->rx_stbc,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_bw", htt_stats_buf->rx_bw,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_pream", htt_stats_buf->rx_pream,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_PREAMBLE_TYPES, "\n");
	len += print_array_to_buf(buf, len, "rx_11ax_su_txbf_mcs",
				  htt_stats_buf->rx_11ax_su_txbf_mcs,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_11ax_mu_txbf_mcs",
				  htt_stats_buf->rx_11ax_mu_txbf_mcs,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_legacy_cck_rate",
				  htt_stats_buf->rx_legacy_cck_rate,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_LEGACY_CCK_STATS, "\n");
	len += print_array_to_buf(buf, len, "rx_legacy_ofdm_rate",
				  htt_stats_buf->rx_legacy_ofdm_rate,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_LEGACY_OFDM_STATS, "\n");
	len += print_array_to_buf(buf, len, "ul_ofdma_rx_mcs",
				  htt_stats_buf->ul_ofdma_rx_mcs,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "ul_ofdma_rx_nss",
				  htt_stats_buf->ul_ofdma_rx_nss,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS, "\n");
	len += print_array_to_buf(buf, len, "ul_ofdma_rx_bw",
				  htt_stats_buf->ul_ofdma_rx_bw,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_ulofdma_non_data_ppdu",
				  htt_stats_buf->rx_ulofdma_non_data_ppdu,
				  ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulofdma_data_ppdu",
				  htt_stats_buf->rx_ulofdma_data_ppdu,
				  ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulofdma_mpdu_ok",
				  htt_stats_buf->rx_ulofdma_mpdu_ok,
				  ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulofdma_mpdu_fail",
				  htt_stats_buf->rx_ulofdma_mpdu_fail,
				  ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulofdma_non_data_nusers",
				  htt_stats_buf->rx_ulofdma_non_data_nusers,
				  ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulofdma_data_nusers",
				  htt_stats_buf->rx_ulofdma_data_nusers,
				  ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_11ax_dl_ofdma_mcs",
				  htt_stats_buf->rx_11ax_dl_ofdma_mcs,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_11ax_dl_ofdma_ru",
				  htt_stats_buf->rx_11ax_dl_ofdma_ru,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_RU_SIZE_COUNTERS, "\n");
	len += print_array_to_buf(buf, len, "rx_ulmumimo_non_data_ppdu",
				  htt_stats_buf->rx_ulmumimo_non_data_ppdu,
				  ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulmumimo_data_ppdu",
				  htt_stats_buf->rx_ulmumimo_data_ppdu,
				  ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulmumimo_mpdu_ok",
				  htt_stats_buf->rx_ulmumimo_mpdu_ok,
				  ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER, "\n");
	len += print_array_to_buf(buf, len, "rx_ulmumimo_mpdu_fail",
				  htt_stats_buf->rx_ulmumimo_mpdu_fail,
				  ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER, "\n");

	len += print_array_to_buf(buf, len, "rx_mcs",
				  htt_stats_buf->rx_mcs,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS, NULL);
	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %u:%u",
				 j + ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS,
				 le32_to_cpu(htt_stats_buf->rx_mcs_ext[j]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "pilot_evm_db[%u] =", j);
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->rx_pil_evm_db[j],
					  ATH12K_HTT_RX_PDEV_STATS_RXEVM_MAX_PILOTS_NSS,
					  "\n");
	}

	len += scnprintf(buf + len, buf_len - len, "pilot_evm_db_mean =");
	for (i = 0; i < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; i++)
		len += scnprintf(buf + len,
				 buf_len - len,
				 " %u:%d,", i,
				 le32_to_cpu(htt_stats_buf->rx_pilot_evm_db_mean[i]));
	len--;
	len += scnprintf(buf + len, buf_len - len, "\n");

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rssi_chain_in_db[%u] = ", j);
		for (i = 0; i < ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS; i++)
			len += scnprintf(buf + len,
					 buf_len - len,
					 " %u: %d,", i,
					 htt_stats_buf->rssi_chain_in_db[j][i]);
		len--;
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_gi[%u] = ", j);
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->rx_gi[j],
					  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS,
					  "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ul_ofdma_rx_gi[%u] = ", j);
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->ul_ofdma_rx_gi[j],
					  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS,
					  "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_ul_fd_rssi: nss[%u] = ", j);
		for (i = 0; i < ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER; i++)
			len += scnprintf(buf + len,
					 buf_len - len,
					 " %u:%d,",
					 i, htt_stats_buf->rx_ul_fd_rssi[j][i]);
		len--;
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_per_chain_rssi_in_dbm[%u] =", j);
		for (i = 0; i < ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS; i++)
			len += scnprintf(buf + len,
					 buf_len - len,
					 " %u:%d,",
					 i,
					 htt_stats_buf->rx_per_chain_rssi_in_dbm[j][i]);
		len--;
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_pdev_rate_ext_stats_tlv(const void *tag_buf, u16 tag_len,
					    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_pdev_rate_ext_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 j;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_PDEV_RATE_EXT_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "rssi_mgmt_in_dbm = %d\n",
			 le32_to_cpu(htt_stats_buf->rssi_mgmt_in_dbm));

	len += print_array_to_buf(buf, len, "rx_stbc_ext",
				  htt_stats_buf->rx_stbc_ext,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT, "\n");
	len += print_array_to_buf(buf, len, "ul_ofdma_rx_mcs_ext",
				  htt_stats_buf->ul_ofdma_rx_mcs_ext,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT, "\n");
	len += print_array_to_buf(buf, len, "rx_11ax_su_txbf_mcs_ext",
				  htt_stats_buf->rx_11ax_su_txbf_mcs_ext,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT, "\n");
	len += print_array_to_buf(buf, len, "rx_11ax_mu_txbf_mcs_ext",
				  htt_stats_buf->rx_11ax_mu_txbf_mcs_ext,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT, "\n");
	len += print_array_to_buf(buf, len, "rx_11ax_dl_ofdma_mcs_ext",
				  htt_stats_buf->rx_11ax_dl_ofdma_mcs_ext,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT, "\n");
	len += print_array_to_buf(buf, len, "rx_bw_ext",
				  htt_stats_buf->rx_bw_ext,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT2_COUNTERS, "\n");

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES; j++) {
		len += scnprintf(buf + len, buf_len - len, j == 0 ?
				 "half_rx_bw = " :
				 "quarter_rx_bw = ");
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->reduced_rx_bw[j],
					  ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS, "\n");
	}

	len += print_array_to_buf(buf, len, "rx_su_punctured_mode",
				  htt_stats_buf->rx_su_punctured_mode,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS,
				  "\n");

	len += print_array_to_buf(buf, len, "rx_mcs_ext",
				  htt_stats_buf->rx_mcs_ext,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT,
				  NULL);
	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS; j++)
		len += scnprintf(buf + len, buf_len - len, ", %u:%u",
				 j + ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT,
				 le32_to_cpu(htt_stats_buf->rx_mcs_ext_2[j]));
	len += scnprintf(buf + len, buf_len - len, "\n");

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_gi_ext[%u] = ", j);
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->rx_gi_ext[j],
					  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT,
					  "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "ul_ofdma_rx_gi_ext[%u] = ", j);
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->ul_ofdma_rx_gi_ext[j],
					  ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT,
					  "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rssi_chain_ext_2[%u] = ",j);
		CHAIN_ARRAY_TO_BUF(buf, len,
				   htt_stats_buf->rssi_chain_ext_2[j],
				   ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_2_COUNTERS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "rx_per_chain_rssi_ext_2_in_dbm[%u] = ", j);
		CHAIN_ARRAY_TO_BUF(buf, len,
				   htt_stats_buf->rx_per_chain_rssi_ext_2_in_dbm[j],
				   ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_2_COUNTERS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	stats_req->buf_len = len;
}

static void ath12k_htt_print_sta_ul_ofdma_stats_tlv(const void *tag_buf,
						    struct debug_htt_stats_req *stats_req)
{
	const struct htt_print_sta_ul_ofdma_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int i, j;

	len += scnprintf(buf + len, buf_len - len, "========STA UL OFDMA STATS=======\n");

	len += scnprintf(buf + len, buf_len - len, "pdev ID = %d\n",
			 htt_stats_buf->pdev_id);
	len += print_array_to_buf(buf, len, "STA HW Trigger Type",htt_stats_buf->rx_trigger_type,
			   HTT_STA_UL_OFDMA_NUM_TRIG_TYPE, "\n");

	len += scnprintf(buf + len, buf_len - len,
			 " BASIC:%u, BRPOLL:%u, MUBAR:%u, MURTS:%u BSRP:%u Others:%u",
			 htt_stats_buf->ax_trigger_type[0],
			 htt_stats_buf->ax_trigger_type[1],
			 htt_stats_buf->ax_trigger_type[2],
			 htt_stats_buf->ax_trigger_type[3],
			 htt_stats_buf->ax_trigger_type[4],
			 htt_stats_buf->ax_trigger_type[5]);
	len += scnprintf(buf + len, buf_len - len, "11ax Trigger Type\n");

	len += scnprintf(buf + len, buf_len - len,
			 " HIPRI:%u, LOWPRI:%u, BSR:%u",
			 htt_stats_buf->num_data_ppdu_responded_per_hwq[0],
			 htt_stats_buf->num_data_ppdu_responded_per_hwq[1],
			 htt_stats_buf->num_data_ppdu_responded_per_hwq[2]);
	len += scnprintf(buf + len, buf_len - len, "Data PPDU Resp per HWQ\n");

	len += scnprintf(buf + len, buf_len - len,
			 " HIPRI:%u, LOWPRI:%u, BSR:%u",
			 htt_stats_buf->num_null_delimiters_responded_per_hwq[0],
			 htt_stats_buf->num_null_delimiters_responded_per_hwq[1],
			 htt_stats_buf->num_null_delimiters_responded_per_hwq[2]);
	len += scnprintf(buf + len, buf_len - len, "Null Delim Resp per HWQ\n");

	len += scnprintf(buf + len, buf_len - len,
			 " Data:%u, NullDelim:%u",
			 htt_stats_buf->num_total_trig_responses[0],
			 htt_stats_buf->num_total_trig_responses[1]);
	len += scnprintf(buf + len, buf_len - len, "Trigger Resp Status\n");

	len += scnprintf(buf + len, buf_len - len, "Last Trigger RX Time Interval = %u\n",
			 htt_stats_buf->last_trig_rx_time_delta_ms);

	len += print_array_to_buf(buf, len, "ul_ofdma_tx_mcs" ,htt_stats_buf->ul_ofdma_tx_mcs,
			   HTT_STA_UL_OFDMA_NUM_MCS_COUNTERS, "\n");

	len += print_array_to_buf(buf, len, "ul_ofdma_tx_nss", htt_stats_buf->ul_ofdma_tx_nss,
			   ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS, "\n");

	for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		len += scnprintf(buf + len, buf_len - len, "ul_ofdma_tx_gi[%u]", j);
		len += print_array_to_buf(buf, len, NULL,htt_stats_buf->ul_ofdma_tx_gi[j],
				   HTT_STA_UL_OFDMA_NUM_MCS_COUNTERS, "\n");
	}

	len += scnprintf(buf + len, buf_len - len, "ul_ofdma_tx_ldpc = %u\n",
			 htt_stats_buf->ul_ofdma_tx_ldpc);

	len += print_array_to_buf(buf, len, "ul_ofdma_tx_bw", htt_stats_buf->ul_ofdma_tx_bw,
			   HTT_STA_UL_OFDMA_NUM_BW_COUNTERS, "\n");

	for (i = 0; i < HTT_STA_UL_OFDMA_NUM_REDUCED_CHAN_TYPES; i++) {
		len += scnprintf(buf + len, buf_len - len,
			  i == 0 ? "half_ul_ofdma_tx_bw" : "quarter_ul_ofdma_tx_bw");
		len += print_array_to_buf(buf, len, NULL,htt_stats_buf->reduced_ul_ofdma_tx_bw[i],
				   HTT_STA_UL_OFDMA_NUM_BW_COUNTERS, "\n");
	}

	len += scnprintf(buf + len, buf_len - len, "Trig Based Tx PPDU = %u\n",
			 htt_stats_buf->trig_based_ppdu_tx);
	len += scnprintf(buf + len, buf_len - len, "RBO Based Tx PPDU = %u\n",
			 htt_stats_buf->rbo_based_ppdu_tx);
	len += scnprintf(buf + len, buf_len - len, "MU to SU EDCA Switch Count = %u\n",
			 htt_stats_buf->mu_edca_to_su_edca_switch_count);
	len += scnprintf(buf + len, buf_len - len, "MU EDCA Params Apply Count = %u\n",
			 htt_stats_buf->num_mu_edca_param_apply_count);

	len += print_array_to_buf(buf, len, "current_edca_hwq_mode[AC]",
			   htt_stats_buf->current_edca_hwq_mode,
			   HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "current_cw_min", htt_stats_buf->current_cw_min,
			   HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "current_cw_max", htt_stats_buf->current_cw_max,
			   HTT_NUM_AC_WMM, "\n");
	len += print_array_to_buf(buf, len, "current_aifs", htt_stats_buf->current_aifs,
			   HTT_NUM_AC_WMM, "\n");

	len += scnprintf(buf + len, buf_len - len, "=============================\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_vdev_rtt_resp_stats_tlv(const void *tag_buf,
					 struct debug_htt_stats_req *stats_req)
{
	const struct htt_vdev_rtt_resp_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_VDEV_RTT_RESP_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "vdev_id = %u\n",
			 htt_stats_buf->vdev_id);
	len += scnprintf(buf + len, buf_len - len, "tx_ftm_suc = %u\n",
			 htt_stats_buf->tx_ftm_suc);
	len += scnprintf(buf + len, buf_len - len, "tx_ftm_suc_retry = %u\n",
			 htt_stats_buf->tx_ftm_suc_retry);
	len += scnprintf(buf + len, buf_len - len, "tx_ftm_fail = %u\n",
			 htt_stats_buf->tx_ftm_fail);
	len += scnprintf(buf + len, buf_len - len, "rx_ftmr_cnt = %u\n",
			 htt_stats_buf->rx_ftmr_cnt);
	len += scnprintf(buf + len, buf_len - len, "rx_ftmr_dup_cnt = %u\n",
			 htt_stats_buf->rx_ftmr_dup_cnt);
	len += scnprintf(buf + len, buf_len - len, "rx_iftmr_cnt = %u\n",
			 htt_stats_buf->rx_iftmr_cnt);
	len += scnprintf(buf + len, buf_len - len, "rx_iftmr_dup_cnt = %u\n",
			 htt_stats_buf->rx_iftmr_dup_cnt);
	len += scnprintf(buf + len, buf_len - len,
			 "initiator_active_responder_rejected_cnt = %u\n",
			 htt_stats_buf->initiator_active_responder_rejected_cnt);
	len += scnprintf(buf + len, buf_len - len, "responder_terminate_cnt = %u\n",
			 htt_stats_buf->responder_terminate_cnt);
	len += scnprintf(buf + len, buf_len - len,
			 "=================================================\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_vdev_rtt_init_stats_tlv(const void *tag_buf,
				 	 struct debug_htt_stats_req *stats_req)
{
	const struct htt_vdev_rtt_init_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "HTT_VDEV_RTT_INIT_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "vdev_id  = %u\n",
			 htt_stats_buf->vdev_id);
	len += scnprintf(buf + len, buf_len - len, "tx_ftmr_cnt  = %u\n",
			 htt_stats_buf->tx_ftmr_cnt);
	len += scnprintf(buf + len, buf_len - len, "tx_ftmr_fail  = %u\n",
			 htt_stats_buf->tx_ftmr_fail);
	len += scnprintf(buf + len, buf_len - len, "tx_ftmr_suc_retry  = %u\n",
			 htt_stats_buf->tx_ftmr_suc_retry);
	len += scnprintf(buf + len, buf_len - len, "rx_ftm_cnt  = %u\n",
			 htt_stats_buf->rx_ftm_cnt);
	len += scnprintf(buf + len, buf_len - len, "initiator_terminate_cnt  = %u\n",
			 htt_stats_buf->initiator_terminate_cnt);
	len += scnprintf(buf + len, buf_len - len, "tx_meas_req_count = %u\n",
			 htt_stats_buf->tx_meas_req_count);
	len += scnprintf(buf + len, buf_len - len, "===============================\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_pktlog_and_htt_ring_stats_tlv(const void *tag_buf,
					       struct debug_htt_stats_req *stats_req)
{
	const struct htt_pktlog_and_htt_ring_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_PKTLOG_AND_HTT_RING_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "pktlog_lite_drop_cnt = %u\n",
			 htt_stats_buf->pktlog_lite_drop_cnt);
	len += scnprintf(buf + len, buf_len - len, "pktlog_tqm_drop_cnt = %u\n",
			 htt_stats_buf->pktlog_tqm_drop_cnt);
	len += scnprintf(buf + len, buf_len - len, "pktlog_ppdu_stats_drop_cnt = %u\n",
			 htt_stats_buf->pktlog_ppdu_stats_drop_cnt);
	len += scnprintf(buf + len, buf_len - len, "pktlog_ppdu_ctrl_drop_cnt = %u\n",
			 htt_stats_buf->pktlog_ppdu_ctrl_drop_cnt);
	len += scnprintf(buf + len, buf_len - len, "pktlog_sw_events_drop_cnt = %u\n",
			 htt_stats_buf->pktlog_sw_events_drop_cnt);
	len += scnprintf(buf + len, buf_len - len,
			 "=================================================\n");

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_umac_ssr_stats_tlv(const void *tag_buf,
				    struct debug_htt_stats_req *stats_req)
{
        const struct htt_umac_ssr_stats_tlv *htt_stats_buf = tag_buf;
        u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
        u32 len = stats_req->buf_len;
        u8 *buf = stats_req->buf;

        len += scnprintf(buf + len, buf_len - len, "HTT_UMAC_SSR_STATS_TLV:\n");
        len += scnprintf(buf + len, buf_len - len, "total_done = %u\n",
                         htt_stats_buf->total_done);
        len += scnprintf(buf + len, buf_len - len, "trigger_requests_count = %u\n",
                         htt_stats_buf->trigger_requests_count);
        len += scnprintf(buf + len, buf_len - len, "total_trig_dropped = %u\n",
                         htt_stats_buf->total_trig_dropped);
        len += scnprintf(buf + len, buf_len - len, "umac_disengaged_count = %u\n",
                         htt_stats_buf->umac_disengaged_count);
        len += scnprintf(buf + len, buf_len - len, "umac_soft_reset_count = %u\n",
                         htt_stats_buf->umac_soft_reset_count);
        len += scnprintf(buf + len, buf_len - len, "umac_engaged_count = %u\n",
                         htt_stats_buf->umac_engaged_count);
        len += scnprintf(buf + len, buf_len - len, "last_trigger_request_ms = %u\n",
                         htt_stats_buf->last_trigger_request_ms);
        len += scnprintf(buf + len, buf_len - len, "last_start_ms = %u\n",
                         htt_stats_buf->last_start_ms);
        len += scnprintf(buf + len, buf_len - len, "last_start_disengage_umac_ms = %u\n",
                         htt_stats_buf->last_start_disengage_umac_ms);
        len += scnprintf(buf + len, buf_len - len, "last_enter_ssr_platform_thread_ms = %u\n",
                         htt_stats_buf->last_enter_ssr_platform_thread_ms);
        len += scnprintf(buf + len, buf_len - len, "last_exit_ssr_platform_thread_ms = %u\n",
                         htt_stats_buf->last_exit_ssr_platform_thread_ms);
        len += scnprintf(buf + len, buf_len - len, "last_start_engage_umac_ms = %u\n",
                         htt_stats_buf->last_start_engage_umac_ms);
        len += scnprintf(buf + len, buf_len - len, "post_reset_tqm_sync_cmd_completion_ms = %u\n",
                         htt_stats_buf->post_reset_tqm_sync_cmd_completion_ms);
        len += scnprintf(buf + len, buf_len - len, "last_done_successful_ms = %u\n",
                         htt_stats_buf->last_done_successful_ms);
        len += scnprintf(buf + len, buf_len - len, "last_e2e_delta_ms = %u\n",
                         htt_stats_buf->last_e2e_delta_ms);
        len += scnprintf(buf + len, buf_len - len, "max_e2e_delta_ms = %u\n",
                         htt_stats_buf->max_e2e_delta_ms);
        len += scnprintf(buf + len, buf_len - len, "trigger_count_for_umac_hang = %u\n",
                         htt_stats_buf->trigger_count_for_umac_hang);
        len += scnprintf(buf + len, buf_len - len, "trigger_count_for_mlo_quick_ssr = %u\n",
                         htt_stats_buf->trigger_count_for_mlo_quick_ssr);
        len += scnprintf(buf + len, buf_len - len, "trigger_count_for_unknown_signature = %u\n",
                         htt_stats_buf->trigger_count_for_unknown_signature);
        len += scnprintf(buf + len, buf_len - len, "htt_sync_mlo_initiate_umac_recovery_ms = %u\n",
                         htt_stats_buf->htt_sync_mlo_initiate_umac_recovery_ms);
        len += scnprintf(buf + len, buf_len - len, "htt_sync_do_pre_reset_ms = %u\n",
                         htt_stats_buf->htt_sync_do_pre_reset_ms);
        len += scnprintf(buf + len, buf_len - len, "htt_sync_do_post_reset_start_ms = %u\n",
                         htt_stats_buf->htt_sync_do_post_reset_start_ms);
        len += scnprintf(buf + len, buf_len - len, "htt_sync_do_post_reset_complete_ms = %u\n",
                         htt_stats_buf->htt_sync_do_post_reset_complete_ms);

        len += scnprintf(buf + len, buf_len - len,
                         "=================================================\n");

        stats_req->buf_len = len;
}

static void ath12k_htt_print_wifi_radar_stats_tlv(const void *tag_buf, u16 tag_len,
						  struct debug_htt_stats_req *stats_req)
{
	const struct htt_stats_tx_pdev_wifi_radar_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;
	u8 i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_TX_PDEV_WIFI_RADAR_TAG:\n");
	len += scnprintf(buf + len, buf_len - len,
			"=================================================\n");
	len += scnprintf(buf + len, buf_len - len, "capture_in_progress = %u\n",
			 le32_to_cpu(htt_stats_buf->capture_in_progress));
	len += scnprintf(buf + len, buf_len - len, "calibration_in_progress = %u\n",
			 le32_to_cpu(htt_stats_buf->calibration_in_progress));
	len += scnprintf(buf + len, buf_len - len, "periodicity (ms) = %u\n",
			 le32_to_cpu(htt_stats_buf->periodicity));
	len += scnprintf(buf + len, buf_len - len, "latest_req_timestamp (ms) = %u\n",
			 le32_to_cpu(htt_stats_buf->latest_req_timestamp));
	len += scnprintf(buf + len, buf_len - len, "latest_resp_timestamp (ms) = %u\n",
			 le32_to_cpu(htt_stats_buf->latest_resp_timestamp));
	len += scnprintf(buf + len, buf_len - len, "Full_calibration_timing (ms) = %u\n",
			 le32_to_cpu(htt_stats_buf->latest_calibration_timing));
	len += scnprintf(buf + len, buf_len - len, "wifi_radar_req_count = %u\n",
			 le32_to_cpu(htt_stats_buf->wifi_radar_req_count));
	len += scnprintf(buf + len, buf_len - len, "num_wifi_radar_pkt_success = %u\n",
			 le32_to_cpu(htt_stats_buf->num_wifi_radar_pkt_success));
	len += scnprintf(buf + len, buf_len - len, "num_wifi_radar_pkt_queued = %u\n",
			 le32_to_cpu(htt_stats_buf->num_wifi_radar_pkt_queued));
	len += scnprintf(buf + len, buf_len - len,
			 "num_wifi_radar_cal_pkt_success = %u\n",
			 le32_to_cpu(htt_stats_buf->num_wifi_radar_cal_pkt_success));
	len += scnprintf(buf + len, buf_len - len, "latest_wifi_radar_cal_type = %u\n",
			 le32_to_cpu(htt_stats_buf->latest_wifi_radar_cal_type));

	len += print_array_to_buf(buf, len, "calibration_type_count = ",
				  htt_stats_buf->wifi_radar_cal_type_counts,
				  ATH12K_HTT_STATS_NUM_WIFI_RADAR_CAL_TYPES, "\n");
	len += scnprintf(buf + len, buf_len - len,
			 "latest_wifi_radar_cal_fail_reason = %u\n",
			 le32_to_cpu(htt_stats_buf->latest_wifi_radar_cal_fail_reason));

	len += print_array_to_buf(buf, len, "wifi_radar_cal_fail_reason_count = ",
				  htt_stats_buf->wifi_radar_cal_type_counts,
				  ATH12K_HTT_STATS_NUM_WIFI_RADAR_CAL_FAILURE_REASONS,
				  "\n");
	len += scnprintf(buf + len, buf_len - len, "wifi_radar_licensed = %u\n",
			 le32_to_cpu(htt_stats_buf->wifi_radar_licensed));
	len += scnprintf(buf + len, buf_len - len, "wifi_radar_cal_init_tx_gain = %u\n",
			 le32_to_cpu(htt_stats_buf->wifi_radar_cal_init_tx_gain));

	len += print_array_to_buf(buf, len, "wifi_radar_cal_fail_reason_count = ",
				  htt_stats_buf->wifi_radar_cal_type_counts,
				  ATH12K_HTT_STATS_NUM_WIFI_RADAR_CAL_FAILURE_REASONS,
				  "\n");
	len += print_array_to_buf(buf, len, "cmd_result_cts2Self = ",
				  htt_stats_buf->cmd_results_cts2self,
				  ATH12K_HTT_STATS_MAX_SCH_CMD_RESULT, "\n");
	len += print_array_to_buf(buf, len, "cmd_results_wifi_radar = ",
				  htt_stats_buf->cmd_results_wifi_radar,
				  ATH12K_HTT_STATS_MAX_SCH_CMD_RESULT, "\n");

	for (i = 0; i < ATH12K_HTT_STATS_MAX_CHAINS; i++) {
		len += scnprintf(buf + len, buf_len - len, "Chain Pair %u Stats\n", i);
		len += scnprintf(buf + len, buf_len - len, "wifi_radar_tx_gain = %u\n",
				 le32_to_cpu(htt_stats_buf->wifi_radar_tx_gains[i]));
		len += scnprintf(buf + len, buf_len - len, "calibration_timing = %u\n",
				 le32_to_cpu
				 (htt_stats_buf->calibration_timing_per_chain[i]));

		len += print_array_to_buf(buf, len, "Rx gain for chain index:",
					  htt_stats_buf->wifi_radar_rx_gains[i],
					  ATH12K_HTT_STATS_MAX_CHAINS,
					  "\n");
	}
	len += scnprintf(buf + len, buf_len - len,
			 "=================================================\n");
	stats_req->buf_len = len;
}

static void
ath12k_htt_print_txbf_ofdma_be_parbw_tlv(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_txbf_ofdma_be_parbw_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_TXBF_OFDMA_BE_PARBW_TAG:\n");
	len += scnprintf(buf + len, buf_len - len, "be_ofdma_parbw_user_snd = %u\n",
			 le32_to_cpu(htt_stats_buf->be_ofdma_parbw_user_snd));
	len += scnprintf(buf + len, buf_len - len, "be_ofdma_parbw_cv = %u\n",
			 le32_to_cpu(htt_stats_buf->be_ofdma_parbw_cv));
	len += scnprintf(buf + len, buf_len - len, "be_ofdma_total_cv = %u\n",
			 le32_to_cpu(htt_stats_buf->be_ofdma_total_cv));

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_be_dl_mu_ofdma_sch_stats_tlv(const void *tag_buf, u16 tag_len,
						    struct debug_htt_stats_req *stats_req)
{
	const struct
		ath12k_htt_tx_pdev_be_dl_mu_ofdma_sch_stats_tlv * htt_stats_buf = tag_buf;
	u8 i;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			"11BE DL MU_OFDMA SCH STATS:\n");

	for (i = 0; i < ATH12K_HTT_TX_NUM_OFDMA_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				"be_mu_ofdma_sch_nusers_%u = %u\n", i,
				le32_to_cpu(htt_stats_buf->be_mu_ofdma_sch_nusers[i]));
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_be_ul_mu_ofdma_sch_stats_tlv(const void *tag_buf, u16 tag_len,
						   struct debug_htt_stats_req *stats_req)
{
	const struct
		ath12k_htt_tx_pdev_be_ul_mu_ofdma_sch_stats_tlv *htt_stats_buf = tag_buf;
	u8 i;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "\n11ax BE UL MU_OFDMA SCH STATS:\n");

	for (i = 0; i < ATH12K_HTT_TX_NUM_OFDMA_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				"be_ul_mu_ofdma_basic_sch_nusers_%u = %u\n", i,
				le32_to_cpu(
				     htt_stats_buf->be_ul_mu_ofdma_basic_sch_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				"be_ul_mu_ofdma_bsr_sch_nusers_%u = %u\n", i,
				le32_to_cpu(
				     htt_stats_buf->be_ul_mu_ofdma_bsr_sch_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				"be_ul_mu_ofdma_bar_sch_nusers_%u = %u\n", i,
				le32_to_cpu(
				     htt_stats_buf->be_ul_mu_ofdma_bar_sch_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				"be_ul_mu_ofdma_brp_sch_nusers_%u = %u\n", i,
				le32_to_cpu(
				     htt_stats_buf->be_ul_mu_ofdma_brp_sch_nusers[i]));
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_bn_dl_mu_ofdma_sch_stats_tlv(const void *tag_buf, u16 tag_len,
						      struct debug_htt_stats_req
						      *stats_req)
{
	const struct ath12k_htt_tx_pdev_bn_dl_mu_ofdma_sch_stats_tlv
				*htt_stats_buf = tag_buf;
	u8 i;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			"11BN DL MU_OFDMA SCH STATS:\n");

	for (i = 0; i < ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				"bn_mu_ofdma_sch_nusers_%u = %u\n", i,
				le32_to_cpu(htt_stats_buf->bn_mu_ofdma_sch_nusers[i]));
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_tx_pdev_bn_ul_mu_ofdma_sch_stats_tlv(const void *tag_buf, u16 tag_len,
						      struct debug_htt_stats_req
						      *stats_req)
{
	const struct ath12k_htt_tx_pdev_bn_ul_mu_ofdma_sch_stats_tlv
				*htt_stats_buf = tag_buf;
	u8 i;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "\n11BN UL MU_OFDMA SCH STATS:\n");

	for (i = 0; i < ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "bn_ul_mu_ofdma_basic_sch_nusers_%u = %u\n", i,
				 le32_to_cpu
				 (htt_stats_buf->bn_ul_mu_ofdma_basic_sch_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "bn_ul_mu_ofdma_bsr_sch_nusers_%u = %u\n", i,
				 le32_to_cpu
				 (htt_stats_buf->bn_ul_mu_ofdma_bsr_sch_nusers[i]));
		len += scnprintf(buf + len, buf_len - len,
				 "bn_ul_mu_ofdma_bar_sch_nusers_%u = %u\n", i,
				 le32_to_cpu
				 (htt_stats_buf->bn_ul_mu_ofdma_bar_sch_nusers[i]));
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_fw_ring_stats(const void *tag_buf, u16 tag_len,
				  struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_rx_ring_stats_tlv *htt_ring_stats_buf = tag_buf;
	u8 *buf =  stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_ring_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_FW_RING_STATUS_TLV_V:\n");

	len += scnprintf(buf + len, buf_len - len, "SW2RXDMA\n");
	len += scnprintf(buf + len, buf_len - len, "max_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_SIZE_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_sw2rxdma)));
	len += scnprintf(buf + len, buf_len - len, "curr_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_CURR_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_sw2rxdma)));

	len += scnprintf(buf + len, buf_len - len, "RXDMA2REO\n");
	len += scnprintf(buf + len, buf_len - len, "max_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_SIZE_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_rxdma2reo)));
	len += scnprintf(buf + len, buf_len - len, "curr_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_CURR_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_rxdma2reo)));

	len += scnprintf(buf + len, buf_len - len, "REO2SW1");
	len += scnprintf(buf + len, buf_len - len, "max_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_SIZE_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_reo2sw1)));
	len += scnprintf(buf + len, buf_len - len, "curr_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_CURR_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_reo2sw1)));

	len += scnprintf(buf + len, buf_len - len, "REO2SW4");
	len += scnprintf(buf + len, buf_len - len, "max_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_SIZE_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_reo2sw4)));
	len += scnprintf(buf + len, buf_len - len, "curr_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_CURR_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_reo2sw4)));

	len += scnprintf(buf + len, buf_len - len,
			 "BackPressure Histogram: 0ms to 250ms = %u, 250ms to 500ms = %u, Above 500ms = %u\n",
			 le32_to_cpu
			 (htt_ring_stats_buf->reo2sw4ringipa_backpress_hist[0]),
			 le32_to_cpu
			 (htt_ring_stats_buf->reo2sw4ringipa_backpress_hist[1]),
			 le32_to_cpu
			 (htt_ring_stats_buf->reo2sw4ringipa_backpress_hist[2]));

	len += scnprintf(buf + len, buf_len - len, "REFILLRINGIPA\n");
	len += scnprintf(buf + len, buf_len - len, "max_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_SIZE_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_refillringipa)));
	len += scnprintf(buf + len, buf_len - len, "curr_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_CURR_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_refillringipa)));
	len += scnprintf(buf + len, buf_len - len, "avg_entries = %u\n",
			 le32_to_cpu(htt_ring_stats_buf->datarate_refillringipa));

	len += scnprintf(buf + len, buf_len - len, "REFILLRINGHOST\n");
	len += scnprintf(buf + len, buf_len - len, "max_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_SIZE_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_refillringhost)));
	len += scnprintf(buf + len, buf_len - len, "curr_num_entries = %u\n",
			 ATH12K_HTT_STATS_RX_FW_RING_CURR_NUM_ENTRIES
			 (le32_to_cpu(htt_ring_stats_buf->entry_status_refillringhost)));
	len += scnprintf(buf + len, buf_len - len, "avg_entries = %u\n",
			 le32_to_cpu(htt_ring_stats_buf->datarate_refillringhost));

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_odd_pdev_mandatory_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_odd_mandatory_pdev_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_ODD_PDEV_MANDATORY_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "hw_queued = %u\n",
			 __le32_to_cpu(htt_stats_buf->hw_queued));
	len += scnprintf(buf + len, buf_len - len, "hw_reaped = %u\n",
			 __le32_to_cpu(htt_stats_buf->hw_reaped));
	len += scnprintf(buf + len, buf_len - len, "hw_paused = %u\n",
			 __le32_to_cpu(htt_stats_buf->hw_paused));
	len += scnprintf(buf + len, buf_len - len, "hw_filt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hw_filt));
	len += scnprintf(buf + len, buf_len - len, "seq_posted = %u\n",
			 __le32_to_cpu(htt_stats_buf->seq_posted));
	len += scnprintf(buf + len, buf_len - len, "seq_completed = %u\n",
			 __le32_to_cpu(htt_stats_buf->seq_completed));
	len += scnprintf(buf + len, buf_len - len, "underrun = %u\n",
			 __le32_to_cpu(htt_stats_buf->underrun));
	len += scnprintf(buf + len, buf_len - len, "hw_flush = %u\n",
			 __le32_to_cpu(htt_stats_buf->hw_flush));
	len += scnprintf(buf + len, buf_len - len, "next_seq_posted_dsr = %u\n",
			 __le32_to_cpu(htt_stats_buf->next_seq_posted_dsr));
	len += scnprintf(buf + len, buf_len - len, "seq_posted_isr = %u\n",
			 __le32_to_cpu(htt_stats_buf->seq_posted_isr));
	len += scnprintf(buf + len, buf_len - len, "mpdu_cnt_fcs_ok = %u\n",
			 __le32_to_cpu(htt_stats_buf->mpdu_cnt_fcs_ok));
	len += scnprintf(buf + len, buf_len - len, "mpdu_cnt_fcs_err = %u\n",
			 __le32_to_cpu(htt_stats_buf->mpdu_cnt_fcs_err));
	len += scnprintf(buf + len, buf_len - len, "msdu_count_tqm = %u\n",
			 __le32_to_cpu(htt_stats_buf->msdu_count_tqm));
	len += scnprintf(buf + len, buf_len - len, "mpdu_count_tqm = %u\n",
			 __le32_to_cpu(htt_stats_buf->mpdu_count_tqm));
	len += scnprintf(buf + len, buf_len - len, "mpdus_ack_failed = %u\n",
			 __le32_to_cpu(htt_stats_buf->mpdus_ack_failed));
	len += scnprintf(buf + len, buf_len - len, "num_data_ppdus_tried_ota = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_data_ppdus_tried_ota));
	len += scnprintf(buf + len, buf_len - len, "ppdu_ok = %u\n",
			 __le32_to_cpu(htt_stats_buf->ppdu_ok));
	len += scnprintf(buf + len, buf_len - len, "num_total_ppdus_tried_ota = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_total_ppdus_tried_ota));
	len += scnprintf(buf + len, buf_len - len, "thermal_suspend_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->thermal_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len, "dfs_suspend_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->dfs_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len, "tx_abort_suspend_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->tx_abort_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len, "suspended_txq_mask = %u\n",
			 __le32_to_cpu(htt_stats_buf->suspended_txq_mask));
	len += scnprintf(buf + len, buf_len - len, "last_suspend_reason = %u\n",
			 __le32_to_cpu(htt_stats_buf->last_suspend_reason));
	len += scnprintf(buf + len, buf_len - len, "seq_failed_queueing = %u\n",
			 __le32_to_cpu(htt_stats_buf->seq_failed_queueing));
	len += scnprintf(buf + len, buf_len - len, "seq_restarted = %u\n",
			 __le32_to_cpu(htt_stats_buf->seq_restarted));
	len += scnprintf(buf + len, buf_len - len, "seq_txop_repost_stop = %u\n",
			 __le32_to_cpu(htt_stats_buf->seq_txop_repost_stop));
	len += scnprintf(buf + len, buf_len - len, "next_seq_cancel = %u\n",
			 __le32_to_cpu(htt_stats_buf->next_seq_cancel));
	len += scnprintf(buf + len, buf_len - len, "seq_min_msdu_repost_stop = %u\n",
			 __le32_to_cpu(htt_stats_buf->seq_min_msdu_repost_stop));
	len += scnprintf(buf + len, buf_len - len, "total_phy_err_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->total_phy_err_cnt));
	len += scnprintf(buf + len, buf_len - len, "ppdu_recvd = %u\n",
			 __le32_to_cpu(htt_stats_buf->ppdu_recvd));
	len += scnprintf(buf + len, buf_len - len, "tcp_msdu_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->tcp_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "tcp_ack_msdu_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->tcp_ack_msdu_cnt));
	len += scnprintf(buf + len, buf_len - len, "udp_msdu_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->udp_msdu_cnt));

	len += print_array_to_buf(buf, len,
				  "fw_tx_mgmt_subtype",
				  htt_stats_buf->fw_tx_mgmt_subtype,
				  ATH12K_HTT_STATS_SUBTYPE_MAX, "\n");

	len += print_array_to_buf(buf, len,
				  "fw_rx_mgmt_subtype",
				  htt_stats_buf->fw_rx_mgmt_subtype,
				  ATH12K_HTT_STATS_SUBTYPE_MAX, "\n");

	len += print_array_to_buf(buf, len,
				  "fw_ring_mpdu_err",
				  htt_stats_buf->fw_ring_mpdu_err,
				  ATH12K_HTT_STATS_SUBTYPE_MAX, "\n");

	len += print_array_to_buf(buf, len,
				  "fw_rx_mgmt_subtype",
				  htt_stats_buf->fw_rx_mgmt_subtype,
				  HTT_RX_STATS_RXDMA_MAX_ERR, "\n");

	len += print_array_to_buf(buf, len,
				  "urrn_stats",
				  htt_stats_buf->urrn_stats,
				  HTT_TX_PDEV_MAX_URRN_STATS, "\n");

	len += print_array_to_buf(buf, len,
				  "sifs_status",
				  htt_stats_buf->sifs_status,
				  HTT_TX_PDEV_MAX_SIFS_BURST_STATS, "\n");

	len += print_array_to_buf(buf, len,
				  "sifs_hist_status",
				  htt_stats_buf->fw_rx_mgmt_subtype,
				  ATH12K_HTT_TX_PDEV_SIFS_BURST_HIST_STATS, "\n");

	len += scnprintf(buf + len, buf_len - len, "rx_suspend_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->rx_suspend_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_suspend_fail_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->rx_suspend_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_resume_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->rx_resume_cnt));
	len += scnprintf(buf + len, buf_len - len, "rx_resume_fail_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->rx_resume_fail_cnt));

	len += print_array_to_buf(buf, len,
				  "hwq_beacon_cmd_result",
				  htt_stats_buf->hwq_beacon_cmd_result,
				  HTT_TX_HWQ_MAX_CMD_RESULT_STATS, "\n");

	len += print_array_to_buf(buf, len,
				  "hwq_voice_cmd_result",
				  htt_stats_buf->hwq_voice_cmd_result,
				  HTT_TX_HWQ_MAX_CMD_RESULT_STATS, "\n");

	len += print_array_to_buf(buf, len,
				  "hwq_video_cmd_result",
				  htt_stats_buf->hwq_video_cmd_result,
				  HTT_TX_HWQ_MAX_CMD_RESULT_STATS, "\n");

	len += print_array_to_buf(buf, len,
				  "hwq_best_effort_cmd_result",
				  htt_stats_buf->hwq_best_effort_cmd_result,
				  HTT_TX_HWQ_MAX_CMD_RESULT_STATS, "\n");

	len += scnprintf(buf + len, buf_len - len, "hwq_beacon_mpdu_tried_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_beacon_mpdu_tried_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_voice_mpdu_tried_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_voice_mpdu_tried_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_video_mpdu_tried_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_video_mpdu_tried_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "hwq_best_effort_mpdu_tried_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_best_effort_mpdu_tried_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_beacon_mpdu_queued_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_beacon_mpdu_queued_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_voice_mpdu_queued_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_voice_mpdu_queued_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_video_mpdu_queued_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_video_mpdu_queued_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "hwq_best_effort_mpdu_queued_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_best_effort_mpdu_queued_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_beacon_mpdu_ack_fail_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_beacon_mpdu_ack_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_voice_mpdu_ack_fail_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_voice_mpdu_ack_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "hwq_video_mpdu_ack_fail_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_video_mpdu_ack_fail_cnt));
	len += scnprintf(buf + len, buf_len - len,
			 "hwq_best_effort_mpdu_ack_fail_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwq_best_effort_mpdu_ack_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "pdev_resets = %u\n",
			 __le32_to_cpu(htt_stats_buf->pdev_resets));
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset = %u\n",
			 __le32_to_cpu(htt_stats_buf->phy_warm_reset));
	len += scnprintf(buf + len, buf_len - len, "hwsch_reset_count = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwsch_reset_count));
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset_ucode_trig = %u\n",
			 __le32_to_cpu(htt_stats_buf->phy_warm_reset_ucode_trig));
	len += scnprintf(buf + len, buf_len - len, "mac_cold_reset = %u\n",
			 __le32_to_cpu(htt_stats_buf->mac_cold_reset));
	len += scnprintf(buf + len, buf_len - len, "mac_warm_reset = %u\n",
			 __le32_to_cpu(htt_stats_buf->mac_warm_reset));
	len += scnprintf(buf + len, buf_len - len, "mac_warm_reset_restore_cal = %u\n",
			 __le32_to_cpu(htt_stats_buf->mac_warm_reset_restore_cal));
	len += scnprintf(buf + len, buf_len - len, "phy_warm_reset_m3_ssr = %u\n",
			 __le32_to_cpu(htt_stats_buf->phy_warm_reset_m3_ssr));
	len += scnprintf(buf + len, buf_len - len, "fw_rx_rings_reset = %u\n",
			 __le32_to_cpu(htt_stats_buf->fw_rx_rings_reset));
	len += scnprintf(buf + len, buf_len - len, "tx_flush = %u\n",
			 __le32_to_cpu(htt_stats_buf->tx_flush));
	len += scnprintf(buf + len, buf_len - len, "hwsch_dev_reset_war = %u\n",
			 __le32_to_cpu(htt_stats_buf->hwsch_dev_reset_war));
	len += scnprintf(buf + len, buf_len - len, "mac_cold_reset_restore_cal = %u\n",
			 __le32_to_cpu(htt_stats_buf->mac_cold_reset_restore_cal));
	len += scnprintf(buf + len, buf_len - len, "mac_only_reset = %u\n",
			 __le32_to_cpu(htt_stats_buf->mac_only_reset));
	len += scnprintf(buf + len, buf_len - len, "mac_sfm_reset = %u\n",
			 __le32_to_cpu(htt_stats_buf->mac_sfm_reset));
	len += scnprintf(buf + len, buf_len - len, "rx_ldpc = %u\n",
			 __le32_to_cpu(htt_stats_buf->rx_ldpc));
	len += scnprintf(buf + len, buf_len - len, "tx_ldpc = %u\n",
			 __le32_to_cpu(htt_stats_buf->tx_ldpc));

	len += print_array_to_buf(buf, len,
				  "gen_mpdu_end_reason",
				  htt_stats_buf->gen_mpdu_end_reason,
				  HTT_TX_TQM_MAX_GEN_MPDU_END_REASON, "\n");

	len += print_array_to_buf(buf, len,
				  "list_mpdu_end_reason",
				  htt_stats_buf->list_mpdu_end_reason,
				  HTT_TX_TQM_MAX_GEN_MPDU_END_REASON, "\n");

	len += print_array_to_buf(buf, len,
				  "tx_mcs",
				  htt_stats_buf->tx_mcs,
				  (ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS +
				  ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS +
				  ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS),
				  "\n");

	len += print_array_to_buf(buf, len,
				  "tx_nss",
				  htt_stats_buf->tx_nss,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS, "\n");

	len += print_array_to_buf(buf, len,
				  "tx_bw",
				  htt_stats_buf->tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");

	len += print_array_to_buf(buf, len,
				  "half_tx_bw",
				  htt_stats_buf->half_tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");

	len += print_array_to_buf(buf, len,
				  "quarter_tx_bw",
				  htt_stats_buf->quarter_tx_bw,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS, "\n");

	len += print_array_to_buf(buf, len,
				  "tx_su_punctured_mode",
				  htt_stats_buf->tx_su_punctured_mode,
				  ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS,
				  "\n");

	len += print_array_to_buf(buf, len,
				  "rx_mcs",
				  htt_stats_buf->rx_mcs,
				  (ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS +
				  ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS +
				  ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS),
				  "\n");

	len += print_array_to_buf(buf, len,
				  "rx_nss",
				  htt_stats_buf->rx_nss,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS, "\n");

	len += print_array_to_buf(buf, len,
				  "rx_bw",
				  htt_stats_buf->rx_bw,
				  ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS, "\n");

	len += print_array_to_buf(buf, len,
				  "rx_stbc",
				  htt_stats_buf->rx_stbc,
				  (ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS +
				   ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS +
				   ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS),
				  "\n");

	len += scnprintf(buf + len, buf_len - len, "rts_cnt = %u\n",
			 __le32_to_cpu(htt_stats_buf->rts_cnt));
	len += scnprintf(buf + len, buf_len - len, "rts_success = %u\n",
			 __le32_to_cpu(htt_stats_buf->rts_success));

	len += scnprintf(buf + len, buf_len - len,
			 "=================================================\n");
	stats_req->buf_len = len;
}

static void ath12k_htt_print_pdev_tdma_stats_tlv(const void *tag_buf,
						 u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_pdev_tdma_stats_tlv *htt_stats_buf = tag_buf;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 len = stats_req->buf_len;
	u8 *buf = stats_req->buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_TDMA_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(__le32_to_cpu(htt_stats_buf->mac_id__word),
				      ATH12K_HTT_STATS_TDMA_MAC_ID_M));
	len += scnprintf(buf + len, buf_len - len, "num_tdma_active_schedules = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_tdma_active_schedules));
	len += scnprintf(buf + len, buf_len - len, "num_tdma_reserved_schedules = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_tdma_reserved_schedules));
	len += scnprintf(buf + len, buf_len - len, "num_tdma_restricted_schedules = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_tdma_restricted_schedules));
	len += scnprintf(buf + len, buf_len - len,
			 "num_tdma_unconfigured_schedules = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_tdma_unconfigured_schedules));
	len += scnprintf(buf + len, buf_len - len, "num_tdma_slot_switches = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_tdma_slot_switches));
	len += scnprintf(buf + len, buf_len - len, "num_tdma_edca_switches = %u\n",
			 __le32_to_cpu(htt_stats_buf->num_tdma_edca_switches));
	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_rx_pdev_ppdu_dur_stats_tlv(const void *tag_buf, u16 tag_len,
					    struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_pdev_ppdu_dur_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8  i, j;
	u16 index = 0;
	char data[HTT_MAX_STRING_LEN] = {0};

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_RX_PDEV_PPDU_DUR_STATS_TLV:\n");

	/* Split the buffer store mechanism into two to avoid data buffer overflow
	 */
	for (i = 0; i < ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS >> 1; i++) {
		index += snprintf(&data[index],
			 HTT_MAX_STRING_LEN - index,
			 " %u-%u : %u,",
			 i * ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
			 (i + 1) * ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
			 __le32_to_cpu(htt_stats_buf->rx_ppdu_dur_hist[i]));
	}

	len += scnprintf(buf + len, buf_len - len, "rx_ppdu_dur_hist_us_0 = %s\n", data);
	memset(data, '\0', sizeof(char) * HTT_MAX_STRING_LEN);
	index = 0;

	for (j = i; j < ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS; j++) {
		index += snprintf(&data[index],
			 HTT_MAX_STRING_LEN - index,
			 " %u-%u : %u,",
			 j * ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
			 (j + 1) * ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US,
			 htt_stats_buf->rx_ppdu_dur_hist[j]);
	}

	len += scnprintf(buf + len, buf_len - len, "rx_ppdu_dur_hist_us_1 = %s\n", data);
	len += scnprintf(buf + len, buf_len - len,
			 "===========================================\n");
	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_ul_mumimo_trig_be_stats(const void *tag_buf, u16 tag_len,
					 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_rx_pdev_ul_mumimo_trig_be_stats_tlv
		     *htt_ulmimo_tri_be_stat_buf = tag_buf;
	u8 i, j;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 mac_id_word = __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->mac_id__word);
	u16 index;
	char str_buf[HTT_MAX_STRING_LEN] = {0};

	if (tag_len < sizeof(*htt_ulmimo_tri_be_stat_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_RX_PDEV_UL_MUMIMO_TRIG_BE_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "mac_id = %u\n",
			 u32_get_bits(mac_id_word, ATH12K_HTT_STATS_MAC_ID));

	len += scnprintf(buf + len, buf_len - len, "rx_11be_ul_mumimo = %u\n",
			 __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->rx_11be_ul_mumimo));

	/* TODO: Check if enough space is present before writing BE MCS Counters */
	/* MCS -2 and -1 will be printed first */

	index = 0;
	memset(str_buf, 0x0, HTT_MAX_STRING_LEN);

	index += snprintf(&str_buf[index], HTT_MAX_STRING_LEN - index, " -2:%u,-1:%u",
			  __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_mcs
			  [ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS - 2]),
			  __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_mcs
			  [ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS - 1]));
	for (i = 0; i < ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS - 2; i++)
		index += snprintf(&str_buf[index],
			 HTT_MAX_STRING_LEN - index, " %u:%u,", i,
			 __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_mcs[i]
			 ));

	len += scnprintf(buf + len, buf_len - len,
			 "be_ul_mumimo_rx_mcs = %s\n", str_buf);

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS; j++) {
		index = 0;
		memset(&str_buf[index], 0x0, HTT_MAX_STRING_LEN);
		index += snprintf(&str_buf[index], HTT_MAX_STRING_LEN - index,
			 " -2:%u,-1:%u",
			 __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_gi[j]
			 [ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS - 2]),
			 __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_gi[j]
			 [ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS - 1]));
		for (i = 0; i < ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS - 2; i++)
			index += snprintf(&str_buf[index],
				 ATH12K_HTT_MAX_STRING_LEN - index, " %u:%u,", i,
				 __le32_to_cpu(
				 htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_gi[j][i]));

		len += scnprintf(buf + len, buf_len - len,
				 "be_ul_mumimo_rx_gi[%u] = %s\n", j, str_buf);
	}

	index = 0;
	memset(str_buf, 0x0, HTT_MAX_STRING_LEN);
	for (i = 0; i < ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS; i++)
		index += snprintf(&str_buf[index],
			 HTT_MAX_STRING_LEN - index, " %u:%u,", i + 1,
			 __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_nss[i]
			 ));

	len += scnprintf(buf + len, buf_len - len,
			 "be_ul_mumimo_rx_nss = %s\n", str_buf);

	len += print_array_to_buf(buf, len, "be_ul_mumimo_rx_bw",
			   htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_bw,
			   ATH12K_HTT_RX_PDEV_STATS_NUM_BE_BW_COUNTERS, "\n");

	len += scnprintf(buf + len, buf_len - len,
			 "be_ul_mumimo_rx_stbc = %u\n",
			 __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_stbc));
	len += scnprintf(buf + len, buf_len - len,
			 "be_ul_mumimo_rx_ldpc = %u\n",
			 __le32_to_cpu(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_rx_ldpc));

	len += print_array_to_buf(buf, len, "rx_ul_mumimo_punctured_mode",
			   htt_ulmimo_tri_be_stat_buf->rx_ul_mumimo_punctured_mode,
			   ATH12K_HTT_RX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS, "\n");

	for (j = 0; j < ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_rx_ul_mumimo_rssi_in_dbm: chain[%u] = ", j);
		CHAIN_ARRAY_TO_BUF(buf, len,
			 htt_ulmimo_tri_be_stat_buf->be_rx_ul_mumimo_chain_rssi_in_dbm[j],
			 ATH12K_HTT_RX_PDEV_STATS_NUM_BE_BW_COUNTERS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_rx_ul_mumimo_target_rssi: user[%u] = ", j);
		CHAIN_ARRAY_TO_BUF(buf, len,
			 htt_ulmimo_tri_be_stat_buf->be_rx_ul_mumimo_target_rssi[j],
			 ATH12K_HTT_RX_PDEV_STATS_NUM_BE_BW_COUNTERS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_rx_ul_mumimo_fd_rssi: user[%u] =  ", j);
		CHAIN_ARRAY_TO_BUF(buf, len,
				   htt_ulmimo_tri_be_stat_buf->be_rx_ul_mumimo_fd_rssi[j],
				   ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}

	for (j = 0; j < ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER; j++) {
		len += scnprintf(buf + len, buf_len - len,
				 "be_rx_ulmumimo_pilot_evm_dB_mean: user [%u] = ", j);
		CHAIN_ARRAY_TO_BUF(buf, len,
			 htt_ulmimo_tri_be_stat_buf->be_rx_ulmumimo_pilot_evm_db_mean[j],
			 ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS);
		len += scnprintf(buf + len, buf_len - len, "\n");
	}
	len += scnprintf(buf + len, buf_len - len,
		"be_ul_mumimo_basic_trigger_rx_qos_null_only = %u\n",
		__le32_to_cpu
		(htt_ulmimo_tri_be_stat_buf->be_ul_mumimo_basic_trigger_rx_qos_null_only)
		);

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_mlo_sched_stats_tlv(const void *tag_buf, u16 tag_len,
				     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_mlo_sched_stats_tlv *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_MLO_SCHED_STATS:\n");
	len += scnprintf(buf + len, buf_len - len, "===============================\n");
	len += scnprintf(buf + len, buf_len - len, "num_sec_link_sched: %u\n",
			 __le32_to_cpu(htt_stats_buf->pref_link_num_sec_link_sched));
	len += scnprintf(buf + len, buf_len - len, "num_pref_link_timeout: %u\n",
			 __le32_to_cpu(htt_stats_buf->pref_link_num_pref_link_timeout));
	len += scnprintf(buf + len, buf_len - len, "num_pref_link_sch_delay_ipc: %u\n",
			 __le32_to_cpu
			 (htt_stats_buf->pref_link_num_pref_link_sch_delay_ipc));
	len += scnprintf(buf + len, buf_len - len, "num_pref_link_timeout_ipc: %u\n",
			 __le32_to_cpu
			 (htt_stats_buf->pref_link_num_pref_link_timeout_ipc));
	len += scnprintf(buf + len, buf_len - len, "================================\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_pdev_mlo_ipc_stats_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{

	const struct ath12k_htt_pdev_mlo_ipc_stats_tlv	*htt_stats_buf = tag_buf;

	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i, j;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_MLO_IPC_STATS:\n");
	len += scnprintf(buf + len, buf_len - len, "===============================\n");

	for (i = 0; i < ATH12K_HTT_STATS_HWMLO_MAX_LINKS; i++) {
		len += scnprintf(buf + len, buf_len - len, "src_link: %u", i);
		for (j = 0; j < ATH12K_HTT_STATS_MLO_MAX_IPC_RINGS; j++) {
			len += scnprintf(buf + len, buf_len - len,
					 "mlo_ipc_ring_full_cnt[%u]: %u",
					 j,
					 __le32_to_cpu
					 (htt_stats_buf->mlo_ipc_ring_full_cnt[i][j]));
		}
		len += scnprintf(buf + len, buf_len - len,
				 "===============================\n");
	}

	stats_req->buf_len = len;
}

static inline void
ath12k_htt_print_pdev_bw_mgr_stats_tlv(const void *tag_buf, u16 tag_len,
				       struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_pdev_bw_mgr_stats_tlv  *htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u32 info1, centre_freq, phy_mode_info, npca_center_freq;
	u32 npca_info1, npca_info2;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	info1 = __le32_to_cpu(htt_stats_buf->mac_id__pri20_idx__freq);
	centre_freq = __le32_to_cpu(htt_stats_buf->centre_freq1__freq2);
	npca_info1 = __le32_to_cpu(htt_stats_buf->npca__phy_mode__static_pattern);
	npca_info2 = __le32_to_cpu(htt_stats_buf->npca__wifi_version__pri20_idx__freq);
	npca_center_freq = __le32_to_cpu(htt_stats_buf->npca__centre_freq1__freq2);
	phy_mode_info = __le32_to_cpu(htt_stats_buf->phy_mode__static_pattern);

	len += scnprintf(buf + len, buf_len - len, "HTT_PDEV_BW_MGR_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "Mac_id = %u\n",
			 u32_get_bits(info1, HTT_BW_MGR_STATS_MAC_ID));
	len += scnprintf(buf + len, buf_len - len, "Phy_mode = %u\n",
			 u32_get_bits(phy_mode_info, HTT_BW_MGR_STATS_CHAN_PHY_MODE));
	len += scnprintf(buf + len, buf_len - len,
			 "Pri20 = %uMhz, centre freq1 = %uMhz, freq2 = %uMhz\n",
			 u32_get_bits(info1, HTT_BW_MGR_STATS_PRI20_FREQ),
			 u32_get_bits(centre_freq, HTT_BW_MGR_STATS_CENTER_FREQ1),
			 u32_get_bits(centre_freq, HTT_BW_MGR_STATS_CENTER_FREQ2));
	len += scnprintf(buf + len, buf_len - len, "Pri20_index = %u\n",
			 u32_get_bits(info1, HTT_BW_MGR_STATS_PRI20_IDX));
	len += scnprintf(buf + len, buf_len - len,
			 "Configured Static Punctured Pattern = 0x%x\n",
			 u32_get_bits(phy_mode_info, HTT_BW_MGR_STATS_STATIC_PATTERN));

	if (u32_get_bits(npca_info2,
			 HTT_BW_MGR_STATS_WIFI_VERSION) >= HTT_WIFI_VER_11BN) {
		len += scnprintf(buf + len, buf_len - len,
				 "HTT_PDEV_NPCA_BW_MGR_STATS_TLV:\n");
		len += scnprintf(buf + len, buf_len - len, "NPCA Phy_mode = %u\n",
				 u32_get_bits(npca_info1, HTT_BW_MGR_STATS_CHAN_PHY_MODE));
		len += scnprintf(buf + len, buf_len - len,
		  "NPCA Pri20 = %uMhz, NPCA centre freq1 = %uMhz, NPCA freq2 = %uMhz\n",
		  u32_get_bits(npca_info2, HTT_BW_MGR_STATS_PRI20_FREQ),
		  u32_get_bits(npca_center_freq, HTT_BW_MGR_STATS_CENTER_FREQ1),
		  u32_get_bits(npca_center_freq, HTT_BW_MGR_STATS_CENTER_FREQ2));
		len += scnprintf(buf + len, buf_len - len, "NPCA Pri20_index = %u\n",
				 u32_get_bits(npca_info2, HTT_BW_MGR_STATS_PRI20_IDX));
		len += scnprintf(buf + len, buf_len - len,
				 "NPCA Configured Static Punctured Pattern = 0x%x\n",
				 u32_get_bits(npca_info1,
					      HTT_BW_MGR_STATS_STATIC_PATTERN));
	}

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ulmumimo_grp_stats_tlv(const void *tag_buf, u16 tag_len,
					struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_pdev_ulmumimo_grp_stats_tlv
			*htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int i, j, index;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_PDEV_ULMUMIMO_GRP_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "Default Grouping:\n");
	len += print_array_to_buf(buf, len, "ul_mumimo_grp eligible",
				  htt_stats_buf->mu_grp_eligible,
				  ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += print_array_to_buf(buf, len, "ul_mumimo_grp_ineligible",
				  htt_stats_buf->mu_grp_ineligible,
				  ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += scnprintf(buf + len, buf_len - len, "ul_mumimo_grp_invalid:\n");
	for (i = 0; i < ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ; i++) {
		len += scnprintf(buf + len, buf_len - len, "grp_id = %u", i);
		index = 0;
		for (j = 0; j < ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE; j++) {
			index += scnprintf(buf + len + index, (buf_len - len) - index,
					   " %u:%u,", j, le32_to_cpu
					   (htt_stats_buf->mu_grp_invalid
					   [i * ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE +
					   j]));
		}
		index--;
		*(buf + len + index) = '\0';
		len += index;
		len += scnprintf(buf + len, buf_len - len, "\n");
	}
	len += scnprintf(buf + len, buf_len - len,
			 "ul_mumimo_grp_candidate_skip_reason(max_grp_size):\n");
	for (i = 0; i < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len, "User_id = %u", i + 1);
		len += print_array_to_buf(buf, len, NULL,
					  htt_stats_buf->mu_grp_candidate_skip[i],
					  ATH12K_HTT_STATS_CANDIDATE_SKIP_REASON_MAX,
					  "\n");
	}

	len += scnprintf(buf + len, buf_len - len, "1SS Grouping:\n");
	len += print_array_to_buf(buf, len, "ul_mumimo_grp eligible_1ss",
			htt_stats_buf->mu_grp_eligible_1ss,
			ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += print_array_to_buf(buf, len, "ul_mumimo_grp_ineligible_1ss",
			htt_stats_buf->mu_grp_ineligible_1ss,
			ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ, "\n");
	len += scnprintf(buf + len, buf_len - len, "ul_mumimo_grp_invalid_1ss:\n");
	for (i = 0; i < ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ; i++) {
		len += scnprintf(buf + len, buf_len - len, "grp_id = %u", i);
		index = 0;
		for (j = 0; j < ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE; j++) {
			index += scnprintf(buf + len + index, (buf_len - len) - index,
					   " %u:%u,", j, le32_to_cpu
					   (htt_stats_buf->mu_grp_invalid_1ss
					   [i * ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE +
					   j]));
		}
		index--;
		*(buf + len + index) = '\0';
		len += index;
		len += scnprintf(buf + len, buf_len - len, "\n");
	}
	len += scnprintf(buf + len, buf_len - len,
			 "ul_mumimo_grp_candidate_skip_reason(max_grp_size):\n");
	for (i = 0; i < ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS; i++) {
		len += scnprintf(buf + len, buf_len - len, "User_id = %u", i + 1);
		len += print_array_to_buf(buf, len, NULL,
				htt_stats_buf->mu_grp_candidate_skip_1ss[i],
				ATH12K_HTT_STATS_CANDIDATE_SKIP_REASON_MAX, "\n");
	}

	len += scnprintf(buf + len, buf_len - len,
			"=================================================\n");
	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ulmumimo_denylist_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_pdev_ulmumimo_denylist_stats_tlv
			*htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_PDEV_ULMUMIMO_DENYLIST_STATS_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "ulmu_peer_denylist = %u\n",
			 le32_to_cpu(htt_stats_buf->num_peer_denylist_cnt));
	len += scnprintf(buf + len, buf_len - len, "trig_bitmap_fail_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->trig_bitmap_fail_cnt));
	len += scnprintf(buf + len, buf_len - len, "trig_consecutive_fail_cnt = %u\n",
			 le32_to_cpu(htt_stats_buf->trig_consecutive_fail_cnt));

	len += scnprintf(buf + len, buf_len - len,
			"=================================================\n");
	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ulmumimo_seq_term_stats_tlv(const void *tag_buf, u16 tag_len,
					     struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_pdev_ulmumimo_seq_term_stats_tlv
			*htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	int index, i;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_PDEV_ULMUMIMO_SEQ_TERM_TLV:\n");
	len += scnprintf(buf + len, buf_len - len, "num_terminate_seq = %u\n",
			 le32_to_cpu(htt_stats_buf->num_terminate_seq));
	len += scnprintf(buf + len, buf_len - len, "num_terminate_low_qdepth = %u\n",
			 le32_to_cpu(htt_stats_buf->num_terminate_low_qdepth));
	len += scnprintf(buf + len, buf_len - len, "num_terminate_seq_inefficient = %u\n",
			 le32_to_cpu(htt_stats_buf->num_terminate_seq_inefficient));

	len += scnprintf(buf + len, buf_len - len, "hist_seq_efficiency =");
	index = 0;
	for (i = 0; i < ATH12K_HTT_STATS_SEQ_EFFICIENCY_HISTOGRAM; i++) {
		index += scnprintf(buf + len + index, (buf_len - len) - index,
				   " %u-%u: %u,", (i * 10),
				   ((i + 1) * 10),
				   le32_to_cpu(htt_stats_buf->hist_seq_efficiency[i]));
	}
	index--;
	*(buf + len + index) = '\0';
	len += index;

	len += scnprintf(buf + len, buf_len - len,
			 "\n=================================================\n");

	stats_req->buf_len = len;
}

static void
ath12k_htt_print_ulmumimo_hist_ineligibility_tlv(const void *tag_buf, u16 tag_len,
						 struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_pdev_ulmumimo_hist_ineligibility_tlv
			*htt_stats_buf = tag_buf;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	u8 i, j;
	int index;

	if (tag_len < sizeof(*htt_stats_buf))
		return;

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_STATS_PDEV_ULMUMIMO_HIST_INELIGIBILITY_TLV:\n");

	len += print_array_to_buf_index(buf, len, "num_triggers", 1,
					htt_stats_buf->num_triggers,
					ATH12K_HTT_STATS_MAX_ULMUMIMO_TRIGGERS, "\n");
	len += scnprintf(buf + len, buf_len - len, "txop_history = ");
	index = 0;
	for (i = 0; i < ATH12K_HTT_STATS_TXOP_HISTOGRAM_BINS / 2; i++) {
		index += scnprintf(buf + len + index, (buf_len - len) - index,
				   " %u-%u:%u,",
				   (i * ATH12K_HTT_STATS_ULMUMIMO_DUR_INTERVAL_US),
				   ((i + 1) * ATH12K_HTT_STATS_ULMUMIMO_DUR_INTERVAL_US),
				   le32_to_cpu(htt_stats_buf->txop_history[i]));
	}
	index--;
	*(buf + len + index) = '\0';
	len += index;
	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "txop_history = ");
	index = 0;

	for (i = ATH12K_HTT_STATS_TXOP_HISTOGRAM_BINS / 2;
			i < ATH12K_HTT_STATS_TXOP_HISTOGRAM_BINS ; i++) {
		index += scnprintf(buf + len + index, (buf_len - len) - index,
				   " %u-%u:%u,",
				   (i * ATH12K_HTT_STATS_ULMUMIMO_DUR_INTERVAL_US),
				   ((i + 1) * ATH12K_HTT_STATS_ULMUMIMO_DUR_INTERVAL_US),
				   le32_to_cpu(htt_stats_buf->txop_history[i]));
	}
	index--;
	*(buf + len + index) = '\0';
	len += index;

	for (i = 0; i < ATH12K_HTT_STATS_MAX_ULMUMIMO_TRIGGERS; i++) {
		len += scnprintf(buf + len, buf_len - len, "\nNum_trigger : %u =", i + 1);
		index = 0;
		for (j = 0; j < ATH12K_HTT_STATS_MAX_PPDU_DURATION_BINS ; j++) {
			index += scnprintf(buf + len + index, buf_len - len - index,
					   " %u: %u,",
					   (j * ATH12K_HTT_STATS_ULMUMIMO_DUR_INTERVAL_US)
					   + ATH12K_HTT_STATS_ULMUMIMO_MIN_PPDU_DUR_US,
					   le32_to_cpu
					   (htt_stats_buf->ppdu_duration_hist[i][j]));
		}
		index--;
		*(buf + len + index) = '\0';
		len += index;
	}

	len += scnprintf(buf + len, buf_len - len, "\nineligible_count = %u\n",
			 le32_to_cpu(htt_stats_buf->ineligible_count));
	len += scnprintf(buf + len, buf_len - len, "history_ineligibility = %u\n",
			 le32_to_cpu(htt_stats_buf->history_ineligibility));
	len += scnprintf(buf + len, buf_len - len,
			 "=================================================\n");

	stats_req->buf_len = len;
}

static void ath12k_htt_print_latency_prof_cal_data_tlv(const void *tag_buf, u16 tag_len,
struct debug_htt_stats_req *stats_req)
{
	u32 i, j;
	void *tag = (void *)tag_buf;
	struct ath12k_htt_stats_latency_prof_cal_data_tlv *htt_stats_buf = tag;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;
	len += scnprintf(buf + len, buf_len - len,
			"HTT_STATS_LATENCY_PROF_CAL_DATA_TLV:\n");

	for (i = 1; i < ATH12K_HTT_STATS_MAX_PROF_CAL; i++) {
	/* ensure latency_prof_name is null-terminated */
		htt_stats_buf->latency_prof_name[i]
			[ATH12K_HTT_STATS_MAX_PROF_STATS_NAME_LEN - 1] = '\0';
		len += scnprintf(buf + len, buf_len - len,
				"|%-25s|%8s|%8s|%8s|%8s|%8s|%10s|%14s|%8s|%8s|%8s|",
				"cal_type", "cnt", "min", "max", "last", "tot",
				"hist_intvl", "hist", "pf_last", "pf_tot", "pf_max\n ");
		for (j = 0; j < htt_stats_buf->cal_cnt[i]; j++) {
			len += scnprintf(buf + len, buf_len - len,
			"|%-25s|%8u|%8u|%8u|%8u|%8u|%10u|%4u:%4u:%4u|%8u|%8u|%8u|\n ",
			((u8 *)
			htt_ctrl_path_cal_type_id_to_name(
			ATH12K_HTT_CTRL_PATH_CALIBRATION_STATS_CAL_TYPE_GET(
			htt_stats_buf->latency_data[i][j].enabled_cal_idx)) +
			sizeof("HTT_CTRL_PATH_STATS_CAL_TYPE")),
			htt_stats_buf->latency_data[i][j].cnt,
			htt_stats_buf->latency_data[i][j].min,
			htt_stats_buf->latency_data[i][j].max,
			htt_stats_buf->latency_data[i][j].last,
			htt_stats_buf->latency_data[i][j].tot,
			htt_stats_buf->latency_data[i][j].hist_intvl,
			htt_stats_buf->latency_data[i][j].hist[0],
			htt_stats_buf->latency_data[i][j].hist[1],
			htt_stats_buf->latency_data[i][j].hist[2],
			htt_stats_buf->latency_data[i][j].pf_last,
			htt_stats_buf->latency_data[i][j].pf_tot,
			htt_stats_buf->latency_data[i][j].pf_max);
		}
	}
	stats_req->buf_len = len;
}

static void ath12k_htt_print_ftm_tpccal_stats_tlv(const void *tag_buf, u16 tag_len, struct
debug_htt_stats_req *stats_req)
{
	u8 i;
	u8 numgain;
	u8 loop_last_idx;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	const struct ath12k_htt_stats_pdev_ftm_tpccal_tlv *htt_stats_buf = tag_buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;
	len += scnprintf(buf + len, buf_len - len, "HTT_STATS_PDEV_FTM_TPCCAL_TAG:\n");

	len += scnprintf(buf + len, buf_len - len,
			 "HTT_FTM_TPCCAL_POSTPROC_FAILURE_STATS:\n");

	len += scnprintf(buf + len, buf_len - len, "CalStatus = %d\n ",
			 htt_stats_buf->tpccal_stats_postproc.calStatus);
	len += scnprintf(buf + len, buf_len - len, "CaldbStatus = %d\n ",
			u32_get_bits(le32_to_cpu
			(htt_stats_buf->tpccal_stats_postproc.dword__numgain_caldbStatus),
			ATH12K_HTT_STATS_TPCCAL_POSTPROC_CALDBSTATUS_M));
	len += scnprintf(buf + len, buf_len - len, "Band = %d\n ",
			 u32_get_bits(le32_to_cpu
			 (htt_stats_buf->tpccal_stats_postproc.dword__channel_chain_band),
			 ATH12K_HTT_STATS_TPCCAL_POSTPROC_BAND_M));
	len += scnprintf(buf + len, buf_len - len, "Chain = %d\n ",
			 u32_get_bits(le32_to_cpu(
			 htt_stats_buf->tpccal_stats_postproc.dword__channel_chain_band),
			 ATH12K_HTT_STATS_TPCCAL_POSTPROC_CHAIN_M));
	len += scnprintf(buf + len, buf_len - len, "Channel = %d\n ",
			 u32_get_bits(le32_to_cpu(
			 htt_stats_buf->tpccal_stats_postproc.dword__channel_chain_band),
			 ATH12K_HTT_STATS_TPCCAL_POSTPROC_CHANNEL_M));
	numgain = u32_get_bits(le32_to_cpu
			(htt_stats_buf->tpccal_stats_postproc.dword__numgain_caldbStatus),
			ATH12K_HTT_STATS_TPCCAL_POSTPROC_NUMGAIN_M);
	for (i = 0; i < numgain; i++) {
		len += scnprintf(buf + len, buf_len - len,
		"TxGainidx = %2d MeasPwr = %5d Pdadc = %4d\n ",
		le32_to_cpu(htt_stats_buf->tpccal_stats_postproc.gainIndex[i]),
		le32_to_cpu(htt_stats_buf->tpccal_stats_postproc.measPwr[i]),
		le32_to_cpu(htt_stats_buf->tpccal_stats_postproc.pdadc[i]));
	}

	len += scnprintf(buf + len, buf_len - len, "\nHTT_FTM_TPCCAL_LATEST_STATS:\n");
	loop_last_idx = u32_get_bits(le32_to_cpu
				(htt_stats_buf->dword__tpccal_last_idx),
				ATH12K_HTT_STATS_TPCCAL_LAST_IDX_M) - 1 +
				ATH12K_HTT_MAX_TPCCAL_STATS;
	for (i = 0; i < ATH12K_HTT_MAX_TPCCAL_STATS; i++) {
		u8 curIdx;
		u16 measPwr;
		u8 pdadc;
		u16 channel;
		u8 chain;
		u8 gainIdx;

		curIdx = loop_last_idx % ATH12K_HTT_MAX_TPCCAL_STATS;
		measPwr = u32_get_bits(le32_to_cpu
			(htt_stats_buf->tpccal_stats[curIdx].dword__measPwr),
			ATH12K_HTT_STATS_TPCCAL_STATS_MEASPWR_M);
		pdadc = u32_get_bits(le32_to_cpu
			(htt_stats_buf->tpccal_stats[curIdx].dword__pdadc),
			ATH12K_HTT_STATS_TPCCAL_STATS_PDADC_M);
		channel = u32_get_bits(le32_to_cpu
		(htt_stats_buf->tpccal_stats[curIdx].dword__channel_chain_gainIndex),
			ATH12K_HTT_STATS_TPCCAL_STATS_CHANNEL_M);
		chain = u32_get_bits(le32_to_cpu
		(htt_stats_buf->tpccal_stats[curIdx].dword__channel_chain_gainIndex),
		ATH12K_HTT_STATS_TPCCAL_STATS_CHAIN_M);
		gainIdx = u32_get_bits(le32_to_cpu
		(htt_stats_buf->tpccal_stats[curIdx].dword__channel_chain_gainIndex),
		ATH12K_HTT_STATS_TPCCAL_STATS_GAININDEX_M);
		len += scnprintf(buf + len, buf_len - len,
				 "CurIdx = %2d Measpwr = %5d Pdadc = %4d",
				 curIdx, measPwr, pdadc);
		len += scnprintf(buf + len, buf_len - len,
				 "TxGainIndex = %2d Channel = %4d Chain = %2d\n",
				 gainIdx, channel, chain);
		loop_last_idx--;
	}
	stats_req->buf_len = len;
}

static void ath12k_htt_stats_print_phy_paprd_pb_tlv(const void *tag_buf, u16 tag_len,
struct debug_htt_stats_req *stats_req)
{
	const struct ath12k_htt_stats_phy_paprd_pb_tlv *htt_stats_buf = tag_buf;
	char str_buf[HTT_MAX_STRING_LEN] = {0};
	u8  i, j;
	u16 index = 0;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;

	if (tag_len < sizeof(*htt_stats_buf))
		return;
	len += scnprintf(buf + len, buf_len - len, "pdev_id = %u\n ",
			 le32_to_cpu(htt_stats_buf->pdev_id));
	len += scnprintf(buf + len, buf_len - len, "total_dpd_cal_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->total_dpd_cal_count));
	len += scnprintf(buf + len, buf_len - len, "chan_change_dpd_cal_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->chan_change_dpd_cal_count));
	len += scnprintf(buf + len, buf_len - len, "thermal_dpd_cal_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->thermal_dpd_cal_count));
	len += scnprintf(buf + len, buf_len - len, "recovery_dpd_cal_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->recovery_dpd_cal_count));
	len += scnprintf(buf + len, buf_len - len, "total_dpd_fail_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->total_dpd_fail_count));
	len += scnprintf(buf + len, buf_len - len, "chan_change_dpd_fail_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->chan_change_dpd_fail_count));
	len += scnprintf(buf + len, buf_len - len, "thermal_dpd_fail_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->thermal_dpd_fail_count));
	len += scnprintf(buf + len, buf_len - len, "recovery_dpd_fail_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->recovery_dpd_fail_count));
	len += scnprintf(buf + len, buf_len - len, "pb_cal_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->pb_cal_count));
	len += scnprintf(buf + len, buf_len - len, "pb_fail_count = %u\n ",
			 le32_to_cpu(htt_stats_buf->pb_fail_count));
	len += scnprintf(buf + len, buf_len - len, "last_dpd_cal_time = %u\n ",
			 le32_to_cpu(htt_stats_buf->last_dpd_cal_time));
	len += scnprintf(buf + len, buf_len - len, "last_pb_cal_time = %u\n ",
			 le32_to_cpu(htt_stats_buf->last_pb_cal_time));
	len += scnprintf(buf + len, buf_len - len, "is_dpd_valid = %u\n ",
			 le32_to_cpu(htt_stats_buf->is_dpd_valid));
	len += scnprintf(buf + len, buf_len - len, "is_pb_valid = %u\n ",
			 le32_to_cpu(htt_stats_buf->is_pb_valid));
	for (i = 0; i < ATH12K_HTT_TX_PDEV_STATS_NUM_BE_BW_COUNTERS; i++) {
		index = 0;
		memset(str_buf, 0x0, HTT_MAX_STRING_LEN);
		for (j = 0; j < ATH12K_HTT_TX_PDEV_STATS_NUM_BE_MCS_COUNTERS; j++) {
			index += snprintf(&str_buf[index], HTT_MAX_STRING_LEN - index,
				" %u:%u, ", j, htt_stats_buf->power_boost_gain[i][j]);
		}
		len += scnprintf(buf + len, buf_len - len,
			"Power Boost Gain for BW %d= %s\n", i, str_buf);
	}
	stats_req->buf_len = len;
}

static void ath12k_htt_print_pdev_rtt_delay_tlv(const void *tag_buf, u16 tag_len, struct
debug_htt_stats_req *stats_req)
{
	u8 i, j, k;
	char str_buf[HTT_STATS_PDEV_RTT_TX_RX_INSTANCES][3] = {"Tx", "Rx"};
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	const struct ath12k_htt_stats_pdev_rtt_delay_tlv *htt_stats_buf = tag_buf;

	if (tag_len < sizeof(*htt_stats_buf))
		return;
	len += scnprintf(buf + len, buf_len - len, "htt_stats_pdev_rtt_delay_tlv:\n ");
	for (i = 0; i < HTT_STATS_PDEV_RTT_DELAY_NUM_INSTANCES; i++) {
		len += scnprintf(buf + len, buf_len - len, "rtt_delay_%d\n ", i);
	for (j = 0; j < HTT_STATS_PDEV_RTT_TX_RX_INSTANCES; j++) {
		for (k = 0; k < HTT_STATS_PDEV_RTT_DELAY_PKT_BW; k++) {
			len += scnprintf(buf + len, buf_len - len,
				"%s_base_delay_%d = %d\n",
				str_buf[j], k,
				htt_stats_buf->rtt_delay[i].base_delay[j][k]);
			len += scnprintf(buf + len, buf_len - len,
				"%s_final_delay%d = %d\n",
				str_buf[j], k,
				htt_stats_buf->rtt_delay[i].final_delay[j][k]);
		}
		len += scnprintf(buf + len, buf_len - len, "per_chan_bias_%s = %d\n ",
				 str_buf[j],
				 htt_stats_buf->rtt_delay[i].per_chan_bias[j]);
		len += scnprintf(buf + len, buf_len - len, "off_chan_bias_%s = %d\n",
				 str_buf[j],
				 htt_stats_buf->rtt_delay[i].off_chan_bias[j]);
		len += scnprintf(buf + len, buf_len - len, "chan_bw_bias_%s = %d\n",
				 str_buf[j],
				 htt_stats_buf->rtt_delay[i].chan_bw_bias[j]);
		len += scnprintf(buf + len, buf_len - len, "rtt_11mc_chain_idx_%s = %u\n",
				 str_buf[j],
				 htt_stats_buf->rtt_delay[i].rtt_11mc_chain_idx[j]);
	}

	len += scnprintf(buf + len, buf_len - len, "chan_freq = %u\n",
			 htt_stats_buf->rtt_delay[i].chan_freq);
	len += scnprintf(buf + len, buf_len - len, "digital_block_status = %u\n",
			 htt_stats_buf->rtt_delay[i].digital_block_status);
	len += scnprintf(buf + len, buf_len - len, "vreg_cache  = %u\n",
			 htt_stats_buf->rtt_delay[i].vreg_cache);
	len += scnprintf(buf + len, buf_len - len, "rtt_11mc_vreg_set_cnt = %u\n",
			 htt_stats_buf->rtt_delay[i].rtt_11mc_vreg_set_cnt);
	len += scnprintf(buf + len, buf_len - len, "cfr_vreg_set_cnt = %u\n",
			 htt_stats_buf->rtt_delay[i].cfr_vreg_set_cnt);
	len += scnprintf(buf + len, buf_len - len, "cir_vreg_set_cnt = %u\n",
			 htt_stats_buf->rtt_delay[i].cir_vreg_set_cnt);
	len += scnprintf(buf + len, buf_len - len, "bandwidth = %u\n",
			 htt_stats_buf->rtt_delay[i].bandwidth);
	}

	stats_req->buf_len = len;
}

static void ath12k_htt_print_pdev_spectral_tlv(const void *tag_buf, u16 tag_len, struct
debug_htt_stats_req *stats_req)
{
	u8 i;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	const struct ath12k_htt_stats_pdev_spectral_tlv *htt_spectral_stats_buf = tag_buf;

	if (tag_len < sizeof(*htt_spectral_stats_buf))
		return;
	len += scnprintf(buf + len, buf_len - len, "htt_stats_pdev_spectral_tlv:\n ");
	len += scnprintf(buf + len, buf_len - len, "dbg_num_buf = %u\n ",
			 htt_spectral_stats_buf->dbg_num_buf);
	len += scnprintf(buf + len, buf_len - len, "dbg_num_events = %u\n ",
			 htt_spectral_stats_buf->dbg_num_events);
	len += scnprintf(buf + len, buf_len - len, "host_head_idx = %u\n ",
			 htt_spectral_stats_buf->host_head_idx);
	len += scnprintf(buf + len, buf_len - len, "host_tail_idx = %u\n ",
			 htt_spectral_stats_buf->host_tail_idx);
	len += scnprintf(buf + len, buf_len - len, "host_shadow_tail_idx = %u\n ",
			 htt_spectral_stats_buf->host_shadow_tail_idx);
	len += scnprintf(buf + len, buf_len - len, "in_ring_head_idx = %u\n ",
			 htt_spectral_stats_buf->in_ring_head_idx);
	len += scnprintf(buf + len, buf_len - len, "in_ring_tail_idx = %u\n ",
			 htt_spectral_stats_buf->in_ring_tail_idx);
	len += scnprintf(buf + len, buf_len - len, "in_ring_shadow_head_idx = %u\n ",
			 htt_spectral_stats_buf->in_ring_shadow_head_idx);
	len += scnprintf(buf + len, buf_len - len, "in_ring_shadow_tail_idx = %u\n ",
			 htt_spectral_stats_buf->in_ring_shadow_tail_idx);
	len += scnprintf(buf + len, buf_len - len, "out_ring_head_idx = %u\n ",
			 htt_spectral_stats_buf->out_ring_head_idx);
	len += scnprintf(buf + len, buf_len - len, "out_ring_tail_idx = %u\n ",
			 htt_spectral_stats_buf->out_ring_tail_idx);
	len += scnprintf(buf + len, buf_len - len, "out_ring_shadow_head_idx = %u\n ",
			 htt_spectral_stats_buf->out_ring_shadow_head_idx);
	len += scnprintf(buf + len, buf_len - len, "out_ring_shadow_tail_idx = %u\n ",
			 htt_spectral_stats_buf->out_ring_shadow_tail_idx);

	for (i = 0; i < HTT_STATS_PDEV_SPECTRAL_MAX_PCSS_RING_FOR_IPC; i++) {
		len += scnprintf(buf + len, buf_len - len,
			"ipc_rings_head_idx_%d = %u\n ",
			i, htt_spectral_stats_buf->ipc_rings[i].head_idx);
		len += scnprintf(buf + len, buf_len - len,
			"ipc_rings_tail_idx_%d = %u\n ",
			i, htt_spectral_stats_buf->ipc_rings[i].tail_idx);
		len += scnprintf(buf + len, buf_len - len,
			"ipc_rings_shadow_head_idx_%d = %u\n ",
			i, htt_spectral_stats_buf->ipc_rings[i].shadow_head_idx);
		len += scnprintf(buf + len, buf_len - len,
			"ipc_rings_shadow_tail_idx_%d = %u\n ",
			i, htt_spectral_stats_buf->ipc_rings[i].shadow_tail_idx);
	}

	for (i = 0; i < HTT_STATS_PDEV_SPECTRAL_PCFG_MAX_DET; i++) {
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_priority_%d = %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_priority);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_count_%d = %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_count);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_period_%d= %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_period);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_chn_mask_%d= %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_chn_mask);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_ena_%d= %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_ena);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_update_mask_%d= %u\n",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_update_mask);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_ready_intrpt_ %d = %u\n",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_ready_intrpt);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scans_performed_%d= %u\n",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scans_performed);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_intrpts_sent_%d= %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].intrpts_sent);
		len += scnprintf(buf + len, buf_len - len,
			"pcfg_stats_scan_pending_count_ %d =%u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].scan_pending_count);
		len += scnprintf(buf + len, buf_len - len, "num_pcss_elem_zero_%d = %u\n",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].num_pcss_elem_zero);
		len += scnprintf(buf + len, buf_len - len, "num_in_elem_zero_%d = %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].num_in_elem_zero);
		len += scnprintf(buf + len, buf_len - len, "num_out_elem_zero_%d = %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].num_out_elem_zero);
		len += scnprintf(buf + len, buf_len - len, "num_elem_moved_%d = %u\n ",
			i, htt_spectral_stats_buf->pcfg_stats_det[i].num_elem_moved);
	}

	len += scnprintf(buf + len, buf_len - len,
		"pcfg_stats_scan_no_ipc_buf_avail= %u\n",
		htt_spectral_stats_buf->pcfg_stats_vreg.scan_no_ipc_buf_avail);
	len += scnprintf(buf + len, buf_len - len,
		"pcfg_stats_agile_scan_no_ipc_buf_avail= %u\n ",
		htt_spectral_stats_buf->pcfg_stats_vreg.agile_scan_no_ipc_buf_avail);
	len += scnprintf(buf + len, buf_len - len,
		"pcfg_stats_scan_FFT_discard_count= %u\n ",
		htt_spectral_stats_buf->pcfg_stats_vreg.scan_FFT_discard_count);
	len += scnprintf(buf + len, buf_len - len,
		"pcfg_stats_scan_recapture_FFT_discard_count= %u\n ",
		htt_spectral_stats_buf->pcfg_stats_vreg.scan_recapture_FFT_discard_count);
	len += scnprintf(buf + len, buf_len - len,
		"pcfg_stats_scan_recapture_count= %u\n ",
		htt_spectral_stats_buf->pcfg_stats_vreg.scan_recapture_count);

	stats_req->buf_len = len;
}

static void ath12k_htt_print_pdev_aoa_tlv(const void *tag_buf, u16 tag_len, struct
debug_htt_stats_req *stats_req)
{
	u8 i, j;
	u8 *buf = stats_req->buf;
	u32 len = stats_req->buf_len;
	u32 buf_len = ATH12K_HTT_STATS_BUF_SIZE;
	const struct ath12k_htt_stats_pdev_aoa_tlv *htt_aoa_stats_buf = tag_buf;

	if (tag_len < sizeof(*htt_aoa_stats_buf))
		return;
	len += scnprintf(buf + len, buf_len - len, "htt_stats_pdev_aoa_tlv:\n ");
	for (i = 0; i < HTT_STATS_PDEV_AOA_MAX_HISTOGRAM; i++) {
		len += scnprintf(buf + len, buf_len - len, "gain_idx[%d] = %u\n ",
				 i, htt_aoa_stats_buf->gain_idx[i]);
		len += scnprintf(buf + len, buf_len - len, "gain_table[%d] = %u\n ",
				 i, htt_aoa_stats_buf->gain_table[i]);

		for (j = 0; j < HTT_STATS_PDEV_AOA_MAX_CHAINS; j++) {
			len += scnprintf(buf + len, buf_len - len,
					 "phase_calculated[ch%d]= %u\n ",
					 i, htt_aoa_stats_buf->phase_calculated[i][j]);
			len += scnprintf(buf + len, buf_len - len,
					 "phase_in_degree[ch%d]= %d\n ",
					 i, htt_aoa_stats_buf->phase_in_degree[i][j]);
		}
	}

	stats_req->buf_len = len;
}

static int ath12k_dbg_htt_ext_stats_parse(struct ath12k_base *ab,
					  u16 tag, u16 len, const void *tag_buf,
					  void *user_data)
{
	struct debug_htt_stats_req *stats_req = user_data;

	switch (tag) {
	case HTT_STATS_TX_PDEV_CMN_TAG:
		ath12k_htt_print_tx_pdev_stats_cmn_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_UNDERRUN_TAG:
		ath12k_htt_print_tx_pdev_stats_urrn_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_SIFS_TAG:
		ath12k_htt_print_tx_pdev_stats_sifs_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_FLUSH_TAG:
		ath12k_htt_print_tx_pdev_stats_flush_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_SIFS_HIST_TAG:
		ath12k_htt_print_tx_pdev_stats_sifs_hist_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_CTRL_PATH_TX_STATS_TAG:
		ath12k_htt_print_pdev_ctrl_path_tx_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_MU_PPDU_DIST_TAG:
		ath12k_htt_print_tx_pdev_mu_ppdu_dist_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SCHED_CMN_TAG:
		ath12k_htt_print_stats_tx_sched_cmn_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_SCHEDULER_TXQ_STATS_TAG:
		ath12k_htt_print_tx_pdev_stats_sched_per_txq_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SCHED_TXQ_CMD_POSTED_TAG:
		ath12k_htt_print_sched_txq_cmd_posted_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SCHED_TXQ_CMD_REAPED_TAG:
		ath12k_htt_print_sched_txq_cmd_reaped_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SCHED_TXQ_SCHED_ORDER_SU_TAG:
		ath12k_htt_print_sched_txq_sched_order_su_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SCHED_TXQ_SCHED_INELIGIBILITY_TAG:
		ath12k_htt_print_sched_txq_sched_ineligibility_tlv(tag_buf, len,
								   stats_req);
		break;
	case HTT_STATS_SCHED_TXQ_SUPERCYCLE_TRIGGER_TAG:
		ath12k_htt_print_sched_txq_supercycle_trigger_tlv(tag_buf, len,
								  stats_req);
		break;
	case HTT_STATS_HW_PDEV_ERRS_TAG:
		ath12k_htt_print_hw_stats_pdev_errs_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_HW_INTR_MISC_TAG:
		ath12k_htt_print_hw_stats_intr_misc_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_WHAL_TX_TAG:
		ath12k_htt_print_hw_stats_whal_tx_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_WHAL_WSI_TAG:
		ath12k_htt_print_hw_stats_whal_wsib_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_HW_WAR_TAG:
		ath12k_htt_print_hw_war_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_TQM_CMN_TAG:
		ath12k_htt_print_tx_tqm_cmn_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_TQM_ERROR_STATS_TAG:
		ath12k_htt_print_tx_tqm_error_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_TQM_GEN_MPDU_TAG:
		ath12k_htt_print_tx_tqm_gen_mpdu_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_TQM_LIST_MPDU_TAG:
		ath12k_htt_print_tx_tqm_list_mpdu_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_TQM_LIST_MPDU_CNT_TAG:
		ath12k_htt_print_tx_tqm_list_mpdu_cnt_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_TQM_PDEV_TAG:
		ath12k_htt_print_tx_tqm_pdev_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_CMN_TAG:
		ath12k_htt_print_tx_de_cmn_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_EAPOL_PACKETS_TAG:
		ath12k_htt_print_tx_de_eapol_packets_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_CLASSIFY_STATS_TAG:
		ath12k_htt_print_tx_de_classify_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_CLASSIFY_FAILED_TAG:
		ath12k_htt_print_tx_de_classify_failed_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_CLASSIFY_STATUS_TAG:
		ath12k_htt_print_tx_de_classify_status_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_ENQUEUE_PACKETS_TAG:
		ath12k_htt_print_tx_de_enqueue_packets_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_ENQUEUE_DISCARD_TAG:
		ath12k_htt_print_tx_de_enqueue_discard_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_DE_COMPL_STATS_TAG:
		ath12k_htt_print_tx_de_compl_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_CMN_STATS_TAG:
		ath12k_htt_print_tx_selfgen_cmn_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_AC_STATS_TAG:
		ath12k_htt_print_tx_selfgen_ac_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_AX_STATS_TAG:
		ath12k_htt_print_tx_selfgen_ax_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_BE_STATS_TAG:
		ath12k_htt_print_tx_selfgen_be_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_AC_ERR_STATS_TAG:
		ath12k_htt_print_tx_selfgen_ac_err_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_AX_ERR_STATS_TAG:
		ath12k_htt_print_tx_selfgen_ax_err_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_BE_ERR_STATS_TAG:
		ath12k_htt_print_tx_selfgen_be_err_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_AC_SCHED_STATUS_STATS_TAG:
		ath12k_htt_print_tx_selfgen_ac_sched_status_stats_tlv(tag_buf, len,
								      stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_AX_SCHED_STATUS_STATS_TAG:
		ath12k_htt_print_tx_selfgen_ax_sched_status_stats_tlv(tag_buf, len,
								      stats_req);
		break;
	case HTT_STATS_TX_SELFGEN_BE_SCHED_STATUS_STATS_TAG:
		ath12k_htt_print_tx_selfgen_be_sched_status_stats_tlv(tag_buf, len,
								      stats_req);
		break;
	case HTT_STATS_RX_RING_STATS_TAG:
		ath12k_htt_print_rx_fw_ring_stats(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_BE_BN_UL_TRIG_TAG:
		ath12k_htt_print_be_bn_ul_trigger_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_STRING_TAG:
		ath12k_htt_print_stats_string_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SRING_STATS_TAG:
		ath12k_htt_print_sring_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SFM_CMN_TAG:
		ath12k_htt_print_sfm_cmn_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SFM_CLIENT_TAG:
		ath12k_htt_print_sfm_client_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SFM_CLIENT_USER_TAG:
		ath12k_htt_print_sfm_client_user_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_MU_MIMO_STATS_TAG:
		ath12k_htt_print_tx_pdev_mu_mimo_sch_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_MUMIMO_GRP_STATS_TAG:
		ath12k_htt_print_tx_pdev_mumimo_grp_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_MPDU_STATS_TAG:
		ath12k_htt_print_tx_pdev_mu_mimo_mpdu_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_CCA_1SEC_HIST_TAG:
	case HTT_STATS_PDEV_CCA_100MSEC_HIST_TAG:
	case HTT_STATS_PDEV_CCA_STAT_CUMULATIVE_TAG:
		ath12k_htt_print_pdev_cca_stats_hist_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_CCA_COUNTERS_TAG:
		ath12k_htt_print_pdev_stats_cca_counters_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_SOUNDING_STATS_TAG:
		ath12k_htt_print_tx_sounding_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_OBSS_PD_TAG:
		ath12k_htt_print_pdev_obss_pd_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_LATENCY_CTX_TAG:
		ath12k_htt_print_latency_prof_ctx_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_LATENCY_CNT_TAG:
		ath12k_htt_print_latency_prof_cnt(tag_buf, len, stats_req);
		break;
	case HTT_STATS_LATENCY_PROF_STATS_TAG:
		ath12k_htt_print_latency_prof_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_UL_TRIG_STATS_TAG:
		ath12k_htt_print_ul_ofdma_trigger_stats(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_UL_OFDMA_USER_STATS_TAG:
		ath12k_htt_print_ul_ofdma_user_stats(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_UL_MUMIMO_TRIG_STATS_TAG:
		ath12k_htt_print_ul_mumimo_trig_stats(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_FSE_STATS_TAG:
		ath12k_htt_print_rx_fse_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_TX_RATE_TXBF_STATS_TAG:
		ath12k_htt_print_pdev_tx_rate_txbf_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_AX_NDPA_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_ax_ndpa_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_AX_NDP_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_ax_ndp_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_AX_BRP_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_ax_brp_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_AX_STEER_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_ax_steer_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_AX_STEER_MPDU_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_ax_steer_mpdu_stats_tlv(tag_buf, len,
								    stats_req);
		break;
	case HTT_STATS_DLPAGER_STATS_TAG:
		ath12k_htt_print_dlpager_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PHY_STATS_TAG:
		ath12k_htt_print_phy_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PHY_COUNTERS_TAG:
		ath12k_htt_print_phy_counters_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PHY_RESET_STATS_TAG:
		ath12k_htt_print_phy_reset_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PHY_RESET_COUNTERS_TAG:
		ath12k_htt_print_phy_reset_counters_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PHY_TPC_STATS_TAG:
		ath12k_htt_print_phy_tpc_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_SOC_TXRX_STATS_COMMON_TAG:
		ath12k_htt_print_soc_txrx_stats_common_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PER_RATE_STATS_TAG:
		ath12k_htt_print_tx_per_rate_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_AST_ENTRY_TAG:
		ath12k_htt_print_ast_entry_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_PUNCTURE_STATS_TAG:
		ath12k_htt_print_puncture_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_DMAC_RESET_STATS_TAG:
		ath12k_htt_print_dmac_reset_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_PPDU_DUR_TAG:
		ath12k_htt_print_tx_pdev_ppdu_dur_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_SCHED_ALGO_OFDMA_STATS_TAG:
		ath12k_htt_print_pdev_sched_algo_ofdma_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_BE_DL_MU_OFDMA_STATS_TAG:
		ath12k_htt_print_tx_pdev_be_dl_mu_ofdma_sch_stats_tlv(tag_buf,
								      len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_BE_UL_MU_OFDMA_STATS_TAG:
		ath12k_htt_print_tx_pdev_be_ul_mu_ofdma_sch_stats_tlv(tag_buf,
								      len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_RATE_STATS_BE_BN_OFDMA_TAG:
		ath12k_htt_print_tx_pdev_rate_stats_be_bn_ofdma_tlv(tag_buf, len,
								    stats_req);
		break;
	case HTT_STATS_PDEV_MBSSID_CTRL_FRAME_STATS_TAG:
		ath12k_htt_print_pdev_mbssid_ctrl_frame_stats_tlv(tag_buf, len,
								  stats_req);
		break;
	case HTT_STATS_TX_PDEV_MLO_ABORT_TAG:
		ath12k_htt_print_tx_pdev_mlo_abort_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_MLO_TXOP_ABORT_TAG:
		ath12k_htt_print_pdev_mlo_txop_abort_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_AP_EDCA_PARAMS_STATS_TAG:
		ath12k_htt_print_tx_pdev_ap_edca_params_stats_tlv(tag_buf, len,
								  stats_req);
		break;
	case HTT_STATS_TX_PDEV_MU_EDCA_PARAMS_STATS_TAG:
		ath12k_htt_print_tx_pdev_mu_edca_params_stats_tlv(tag_buf, len,
								  stats_req);
		break;
	case HTT_STATS_TX_PDEV_RATE_STATS_TAG:
		ath12k_htt_print_tx_pdev_rate_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_BE_RATE_STATS_TAG:
		ath12k_htt_print_tx_pdev_be_rate_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_SAWF_RATE_STATS_TAG:
		ath12k_htt_print_tx_pdev_sawf_rate_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_RATE_STATS_TAG:
		ath12k_htt_print_rx_pdev_rate_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_RATE_EXT_STATS_TAG:
		ath12k_htt_print_rx_pdev_rate_ext_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_PHY_ERR_TAG:
		ath12k_htt_print_tx_pdev_stats_phy_err_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_TX_PPDU_STATS_TAG:
		ath12k_htt_print_tx_pdev_stats_tx_ppdu_stats_tlv_v(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_PDEV_TRIED_MPDU_CNT_HIST_TAG:
		ath12k_htt_print_tx_pdev_stats_tried_mpdu_cnt_hist_tlv_v(tag_buf, len,
									 stats_req);
		break;
	case HTT_STATS_HW_WD_TIMEOUT_TAG:
		ath12k_htt_print_hw_stats_wd_timeout_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_PEER_MSDU_FLOWQ_TAG:
		ath12k_htt_print_msdu_flow_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_TID_DETAILS_TAG:
		ath12k_htt_print_tx_tid_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_TID_DETAILS_V1_TAG:
		ath12k_htt_print_tx_tid_stats_v1_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_RX_TID_DETAILS_TAG:
		ath12k_htt_print_rx_tid_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_COUNTER_NAME_TAG:
		ath12k_htt_print_counter_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_PEER_STATS_CMN_TAG:
		ath12k_htt_print_peer_stats_cmn_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_PEER_DETAILS_TAG:
		ath12k_htt_print_peer_details_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PEER_TX_RATE_STATS_TAG:
		ath12k_htt_print_tx_peer_rate_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_PEER_RX_RATE_STATS_TAG:
		ath12k_htt_print_rx_peer_rate_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_HWQ_MUMIMO_SCH_STATS_TAG:
		ath12k_htt_print_tx_hwq_mu_mimo_sch_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_HWQ_MUMIMO_MPDU_STATS_TAG:
		ath12k_htt_print_tx_hwq_mu_mimo_mpdu_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_HWQ_MUMIMO_CMN_STATS_TAG:
		ath12k_htt_print_tx_hwq_mu_mimo_cmn_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_HWQ_CMN_TAG:
		ath12k_htt_print_tx_hwq_stats_cmn_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_HWQ_DIFS_LATENCY_TAG:
		ath12k_htt_print_tx_hwq_difs_latency_stats_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_HWQ_CMD_RESULT_TAG:
		ath12k_htt_print_tx_hwq_cmd_result_stats_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_HWQ_CMD_STALL_TAG:
		ath12k_htt_print_tx_hwq_cmd_stall_stats_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_HWQ_FES_STATUS_TAG:
		ath12k_htt_print_tx_hwq_fes_result_stats_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_HWQ_TRIED_MPDU_CNT_HIST_TAG:
		ath12k_htt_print_tx_hwq_tried_mpdu_cnt_hist_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_HWQ_TXOP_USED_CNT_HIST_TAG:
		ath12k_htt_print_tx_hwq_txop_used_cnt_hist_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_TQM_CMDQ_STATUS_TAG:
		ath12k_htt_print_tx_tqm_cmdq_status_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_DE_FW2WBM_RING_FULL_HIST_TAG:
		ath12k_htt_print_tx_de_fw2wbm_ring_full_hist_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RING_IF_TAG:
		ath12k_htt_print_ring_if_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_RING_IF_CMN_TAG:
		ath12k_htt_print_ring_if_cmn_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_SRING_CMN_TAG:
		ath12k_htt_print_sring_cmn_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_RX_SOC_FW_STATS_TAG:
		ath12k_htt_print_rx_soc_fw_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_RX_SOC_FW_REFILL_RING_EMPTY_TAG:
		ath12k_htt_print_rx_soc_fw_refill_ring_empty_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_REFILL_RXDMA_ERR_TAG:
		ath12k_htt_print_rx_soc_fw_refill_ring_num_rxdma_err_tlv_v(
				tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_REFILL_REO_ERR_TAG:
		ath12k_htt_print_rx_soc_fw_refill_ring_num_reo_err_tlv_v(
				tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_REO_RESOURCE_STATS_TAG:
		ath12k_htt_print_rx_reo_debug_stats_tlv_v(
				tag_buf, stats_req);
		break;
	case HTT_STATS_RX_SOC_FW_REFILL_RING_NUM_REFILL_TAG:
		ath12k_htt_print_rx_soc_fw_refill_ring_num_refill_tlv_v(
				tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_FW_STATS_TAG:
		ath12k_htt_print_rx_pdev_fw_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_RX_PDEV_FW_RING_MPDU_ERR_TAG:
		ath12k_htt_print_rx_pdev_fw_ring_mpdu_err_tlv_v(tag_buf, stats_req);
		break;
	case HTT_STATS_RX_PDEV_FW_MPDU_DROP_TAG:
		ath12k_htt_print_rx_pdev_fw_mpdu_drop_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_FW_STATS_PHY_ERR_TAG:
		ath12k_htt_print_rx_pdev_fw_stats_phy_err_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_PDEV_TWT_SESSIONS_TAG:
		ath12k_htt_print_pdev_stats_twt_sessions_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_PDEV_TWT_SESSION_TAG:
		ath12k_htt_print_pdev_stats_twt_session_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_RING_BACKPRESSURE_STATS_TAG:
		ath12k_htt_print_backpressure_stats_tlv_v(tag_buf, user_data);
		break;
	case HTT_STATS_UNAVAILABLE_ERROR_STATS_TAG:
		ath12k_htt_print_unavailable_error_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_UNSUPPORTED_ERROR_STATS_TAG:
		ath12k_htt_print_unsupported_error_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_VDEV_TXRX_STATS_HW_STATS_TAG:
		ath12k_htt_print_vdev_txrx_stats_hw_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_PDEV_DL_MU_OFDMA_STATS_TAG:
		ath12k_htt_print_tx_pdev_dl_mu_ofdma_sch_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_PDEV_UL_MU_OFDMA_STATS_TAG:
		ath12k_htt_print_tx_pdev_ul_mu_ofdma_sch_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_PDEV_DL_MU_MIMO_STATS_TAG:
		ath12k_htt_print_tx_pdev_dl_mu_mimo_sch_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TX_PDEV_UL_MU_MIMO_STATS_TAG:
		ath12k_htt_print_tx_pdev_ul_mu_mimo_sch_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_VDEV_RTT_RESP_STATS_TAG:
		ath12k_htt_print_vdev_rtt_resp_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_VDEV_RTT_INIT_STATS_TAG:
		ath12k_htt_print_vdev_rtt_init_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_PKTLOG_AND_HTT_RING_STATS_TAG:
		ath12k_htt_print_pktlog_and_htt_ring_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_STA_UL_OFDMA_STATS_TAG:
		ath12k_htt_print_sta_ul_ofdma_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_BE_NDPA_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_be_ndpa_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_BE_NDP_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_be_ndp_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_BE_BRP_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_be_brp_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_BE_STEER_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_be_steer_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_BE_STEER_MPDU_STATS_TAG:
		ath12k_htt_print_txbf_ofdma_be_steer_mpdu_stats_tlv(tag_buf, len,
								    stats_req);
		break;
	case HTT_STATS_RX_PDEV_BE_UL_OFDMA_USER_STATS_TAG:
		ath12k_htt_print_be_ul_ofdma_user_stats(tag_buf, len, stats_req);
		break;
	case HTT_STATS_ML_PEER_DETAILS_TAG:
		ath12k_htt_print_ml_peer_details_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_ML_PEER_EXT_DETAILS_TAG:
		ath12k_htt_print_ml_peer_ext_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_ML_LINK_INFO_DETAILS_TAG:
		ath12k_htt_print_ml_link_info_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_UMAC_SSR_TAG:
		ath12k_htt_print_umac_ssr_stats_tlv(tag_buf, stats_req);
		break;
	case HTT_STATS_GTX_TAG:
		ath12k_htt_print_htt_stats_gtx_stats_tlv_v(tag_buf, len, stats_req);
		break;
	case HTT_STATS_HDS_PROF_STATS_TAG:
		htt_print_hds_prof_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_WIFI_RADAR_TAG:
		ath12k_htt_print_wifi_radar_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TXBF_OFDMA_BE_PARBW_TAG:
		ath12k_htt_print_txbf_ofdma_be_parbw_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_UL_MUMIMO_GRP_STATS_TAG:
		ath12k_htt_print_ulmumimo_grp_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_UL_MUMIMO_DENYLIST_STATS_TAG:
		ath12k_htt_print_ulmumimo_denylist_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_UL_MUMIMO_SEQ_TERM_STATS_TAG:
		ath12k_htt_print_ulmumimo_seq_term_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_UL_MUMIMO_HIST_INELIGIBILITY_TAG:
		ath12k_htt_print_ulmumimo_hist_ineligibility_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_ODD_PDEV_MANDATORY_TAG:
		ath12k_htt_print_odd_pdev_mandatory_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_TDMA_TAG:
		ath12k_htt_print_pdev_tdma_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_PPDU_DUR_TAG:
		ath12k_htt_print_rx_pdev_ppdu_dur_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_RX_PDEV_UL_MUMIMO_TRIG_BE_STATS_TAG:
		ath12k_htt_print_ul_mumimo_trig_be_stats(tag_buf, len, stats_req);
		break;
	case HTT_STATS_LATENCY_PROF_CAL_DATA_TAG:
		ath12k_htt_print_latency_prof_cal_data_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_FTM_TPCCAL_TAG:
		ath12k_htt_print_ftm_tpccal_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PHY_PAPRD_PB_TAG:
		ath12k_htt_stats_print_phy_paprd_pb_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_RTT_DELAY_TAG:
		ath12k_htt_print_pdev_rtt_delay_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_SPECTRAL_TAG:
		ath12k_htt_print_pdev_spectral_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_AOA_TAG:
		ath12k_htt_print_pdev_aoa_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_BW_MGR_STATS_TAG:
		ath12k_htt_print_pdev_bw_mgr_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_MLO_SCHED_STATS_TAG:
		ath12k_htt_print_mlo_sched_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_PDEV_MLO_IPC_STATS_TAG:
		ath12k_htt_print_pdev_mlo_ipc_stats_tlv(tag_buf, len, stats_req);
		break;
	case HTT_STATS_TX_PDEV_PENDING_SEQ_CNT_ON_SCHED_POST_HIST_TAG:
		ath12k_htt_print_tx_pdev_pending_seq_cnt_on_sched_post_hist_tlv(tag_buf,
									len,
									stats_req);
		break;
	case HTT_STATS_TX_PDEV_PENDING_SEQ_CNT_IN_HWQ_HIST_TAG:
		ath12k_htt_print_tx_pdev_pending_seq_cnt_in_hwq_hist_tlv(tag_buf, len,
									 stats_req);
		break;
	case HTT_STATS_TX_PDEV_PENDING_SEQ_CNT_IN_TXQ_HIST_TAG:
		ath12k_htt_print_tx_pdev_pending_seq_cnt_in_txq_hist_tlv(tag_buf, len,
									 stats_req);
		break;
	case HTT_STATS_SCHED_TXQ_EARLY_COMPL_TAG:
		ath12k_htt_print_sched_txq_early_compl_tlv(tag_buf, len, stats_req);
		break;

	case HTT_STATS_RX_PDEV_BN_UL_OFDMA_USER_TAG:
		ath12k_htt_print_bn_ul_ofdma_user_stats(tag_buf, len, stats_req);
		break;

	case HTT_STATS_TX_SELFGEN_BN_TAG:
		ath12k_htt_print_tx_selfgen_bn_stats_tlv(tag_buf, len, stats_req);
		break;

	case HTT_STATS_TX_SELFGEN_BN_ERR_TAG:
		ath12k_htt_print_tx_selfgen_bn_err_stats_tlv(tag_buf, len, stats_req);
		break;

	case HTT_STATS_TX_SELFGEN_BN_SCHED_STATUS_TAG:
		ath12k_htt_print_tx_selfgen_bn_sched_status_stats_tlv(tag_buf, len,
								      stats_req);
		break;

	case HTT_STATS_TX_PDEV_BN_DL_MU_OFDMA_STATS_TAG:
		ath12k_htt_print_tx_pdev_bn_dl_mu_ofdma_sch_stats_tlv(tag_buf, len,
								      stats_req);
		break;

	case HTT_STATS_TX_PDEV_BN_UL_MU_OFDMA_STATS_TAG:
		ath12k_htt_print_tx_pdev_bn_ul_mu_ofdma_sch_stats_tlv(tag_buf, len,
								      stats_req);
		break;

	default:
		break;
	}

	return 0;
}

void ath12k_debugfs_htt_ext_stats_handler(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct ath12k_htt_extd_stats_msg *msg;
	struct debug_htt_stats_req *stats_req;
	struct ath12k *ar;
	u32 len, pdev_id, stats_info;
	u64 cookie;
	int ret;
	bool send_completion = false;

	msg = (struct ath12k_htt_extd_stats_msg *)skb->data;
	cookie = le64_to_cpu(msg->cookie);

	if (u64_get_bits(cookie, ATH12K_HTT_STATS_COOKIE_MSB) !=
			 ATH12K_HTT_STATS_MAGIC_VALUE) {
		ath12k_warn(ab, "received invalid htt ext stats event\n");
		return;
	}

	pdev_id = u64_get_bits(cookie, ATH12K_HTT_STATS_COOKIE_LSB);
	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "failed to get ar for pdev_id %d\n", pdev_id);
		goto exit;
	}

	stats_req = ar->debug.htt_stats.stats_req;
	if (!stats_req)
		goto exit;

	spin_lock_bh(&ar->data_lock);

	stats_info = le32_to_cpu(msg->info1);
	stats_req->done = u32_get_bits(stats_info, ATH12K_HTT_T2H_EXT_STATS_INFO1_DONE);
	if (stats_req->done)
		send_completion = true;

	spin_unlock_bh(&ar->data_lock);

	len = u32_get_bits(stats_info, ATH12K_HTT_T2H_EXT_STATS_INFO1_LENGTH);
	if (len > skb->len) {
		ath12k_warn(ab, "invalid length %d for HTT stats", len);
		goto exit;
	}

	ret = ath12k_dp_htt_tlv_iter(ab, msg->data, len,
				     ath12k_dbg_htt_ext_stats_parse,
				     stats_req);
	if (ret)
		ath12k_warn(ab, "Failed to parse tlv %d\n", ret);

	if (send_completion)
		complete(&stats_req->htt_stats_rcvd);
exit:
	rcu_read_unlock();
}

static ssize_t ath12k_read_htt_stats_type(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	enum ath12k_dbg_htt_ext_stats_type type;
	char buf[32];
	size_t len;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	type = ar->debug.htt_stats.type;
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	len = scnprintf(buf, sizeof(buf), "%u\n", type);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_htt_stats_type(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	enum ath12k_dbg_htt_ext_stats_type type;
	unsigned int cfg_param[4] = {0};
	const int size = 32;
	int num_args;

	char *buf __free(kfree) = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	num_args = sscanf(buf, "%u %u %u %u %u\n", &type, &cfg_param[0],
			  &cfg_param[1], &cfg_param[2], &cfg_param[3]);
	if (!num_args || num_args > 5)
		return -EINVAL;

	if (type == ATH12K_DBG_HTT_EXT_STATS_RESET)
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	ar->debug.htt_stats.type = type;
	ar->debug.htt_stats.cfg_param[0] = cfg_param[0];
	ar->debug.htt_stats.cfg_param[1] = cfg_param[1];
	ar->debug.htt_stats.cfg_param[2] = cfg_param[2];
	ar->debug.htt_stats.cfg_param[3] = cfg_param[3];

	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return count;
}

static const struct file_operations fops_htt_stats_type = {
	.read = ath12k_read_htt_stats_type,
	.write = ath12k_write_htt_stats_type,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int ath12k_debugfs_htt_stats_req(struct ath12k *ar)
{
	struct debug_htt_stats_req *stats_req = ar->debug.htt_stats.stats_req;
	enum ath12k_dbg_htt_ext_stats_type type = stats_req->type;
	u64 cookie;
	int ret, pdev_id;
	struct htt_ext_stats_cfg_params cfg_params = { 0 };

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	init_completion(&stats_req->htt_stats_rcvd);

	pdev_id = ath12k_mac_get_target_pdev_id(ar);
	stats_req->done = false;
	stats_req->pdev_id = pdev_id;

	cookie = u64_encode_bits(ATH12K_HTT_STATS_MAGIC_VALUE,
				 ATH12K_HTT_STATS_COOKIE_MSB);
	cookie |= u64_encode_bits(pdev_id, ATH12K_HTT_STATS_COOKIE_LSB);

	if (stats_req->override_cfg_param) {
		cfg_params.cfg0 = stats_req->cfg_param[0];
		cfg_params.cfg1 = stats_req->cfg_param[1];
		cfg_params.cfg2 = stats_req->cfg_param[2];
		cfg_params.cfg3 = stats_req->cfg_param[3];
	}

	ret = ath12k_prep_htt_stats_cfg_params(ar, type, stats_req->peer_addr,
					       &cfg_params);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set htt stats cfg params: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_tx_htt_h2t_ext_stats_req(ar, type, &cfg_params, cookie);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send htt stats request: %d\n", ret);
		return ret;
	}
	if (!wait_for_completion_timeout(&stats_req->htt_stats_rcvd, 3 * HZ)) {
		spin_lock_bh(&ar->data_lock);
		if (!stats_req->done) {
			stats_req->done = true;
			spin_unlock_bh(&ar->data_lock);
			ath12k_warn(ar->ab, "stats request timed out\n");
			return -ETIMEDOUT;
		}
		spin_unlock_bh(&ar->data_lock);
	}

	return 0;
}

static int ath12k_open_htt_stats(struct inode *inode,
				 struct file *file)
{
	struct ath12k *ar = inode->i_private;
	struct debug_htt_stats_req *stats_req;
	enum ath12k_dbg_htt_ext_stats_type type = ar->debug.htt_stats.type;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret;

	if (type == ATH12K_DBG_HTT_EXT_STATS_RESET ||
	    type == ATH12K_DBG_HTT_EXT_STATS_PEER_INFO ||
	    type == ATH12K_DBG_HTT_EXT_PEER_CTRL_PATH_TXRX_STATS)
		return -EPERM;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	if (ah->state != ATH12K_HW_STATE_ON &&
	    ar->ab->fw_mode != ATH12K_FIRMWARE_MODE_FTM) {
		ret = -ENETDOWN;
		goto err_unlock;
	}

	if (ar->debug.htt_stats.stats_req) {
		ret = -EAGAIN;
		goto err_unlock;
	}

	stats_req = kzalloc(sizeof(*stats_req) + ATH12K_HTT_STATS_BUF_SIZE, GFP_KERNEL);
	if (!stats_req) {
		ret = -ENOMEM;
		goto err_unlock;
	}

	ar->debug.htt_stats.stats_req = stats_req;
	stats_req->type = type;
	stats_req->cfg_param[0] = ar->debug.htt_stats.cfg_param[0];
	stats_req->cfg_param[1] = ar->debug.htt_stats.cfg_param[1];
	stats_req->cfg_param[2] = ar->debug.htt_stats.cfg_param[2];
	stats_req->cfg_param[3] = ar->debug.htt_stats.cfg_param[3];
	stats_req->override_cfg_param = !!stats_req->cfg_param[0] ||
					!!stats_req->cfg_param[1] ||
					!!stats_req->cfg_param[2] ||
					!!stats_req->cfg_param[3];

	ret = ath12k_debugfs_htt_stats_req(ar);
	if (ret < 0)
		goto out;

	file->private_data = stats_req;

	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return 0;
out:
	kfree(stats_req);
	ar->debug.htt_stats.stats_req = NULL;
err_unlock:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return ret;
}

static int ath12k_release_htt_stats(struct inode *inode,
				    struct file *file)
{
	struct ath12k *ar = inode->i_private;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	kfree(file->private_data);
	ar->debug.htt_stats.stats_req = NULL;
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return 0;
}

static ssize_t ath12k_read_htt_stats(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct debug_htt_stats_req *stats_req = file->private_data;
	char *buf;
	u32 length;

	buf = stats_req->buf;
	length = min_t(u32, stats_req->buf_len, ATH12K_HTT_STATS_BUF_SIZE);
	return simple_read_from_buffer(user_buf, count, ppos, buf, length);
}

static const struct file_operations fops_dump_htt_stats = {
	.open = ath12k_open_htt_stats,
	.release = ath12k_release_htt_stats,
	.read = ath12k_read_htt_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_read_htt_stats_reset(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", ar->debug.htt_stats.reset);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_htt_stats_reset(struct file *file,
					    const char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	enum ath12k_dbg_htt_ext_stats_type type;
	struct htt_ext_stats_cfg_params cfg_params = { 0 };
	u8 param_pos;
	int ret;

	ret = kstrtou32_from_user(user_buf, count, 0, &type);
	if (ret)
		return ret;

	if (type >= ATH12K_DBG_HTT_NUM_EXT_STATS ||
	    type == ATH12K_DBG_HTT_EXT_STATS_RESET)
		return -E2BIG;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	cfg_params.cfg0 = HTT_STAT_DEFAULT_RESET_START_OFFSET;
	param_pos = (type >> 5) + 1;

	switch (param_pos) {
	case ATH12K_HTT_STATS_RESET_PARAM_CFG_32_BYTES:
		cfg_params.cfg1 = 1 << (cfg_params.cfg0 + type);
		break;
	case ATH12K_HTT_STATS_RESET_PARAM_CFG_64_BYTES:
		cfg_params.cfg2 = ATH12K_HTT_STATS_RESET_BITMAP32_BIT(cfg_params.cfg0 +
								      type);
		break;
	case ATH12K_HTT_STATS_RESET_PARAM_CFG_128_BYTES:
		cfg_params.cfg3 = ATH12K_HTT_STATS_RESET_BITMAP64_BIT(cfg_params.cfg0 +
								      type);
		break;
	default:
		break;
	}

	ret = ath12k_dp_tx_htt_h2t_ext_stats_req(ar,
						 ATH12K_DBG_HTT_EXT_STATS_RESET,
						 &cfg_params,
						 0ULL);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send htt stats request: %d\n", ret);
		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
		return ret;
	}

	ar->debug.htt_stats.reset = type;
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return count;
}

static const struct file_operations fops_htt_stats_reset = {
	.read = ath12k_read_htt_stats_reset,
	.write = ath12k_write_htt_stats_reset,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath12k_debugfs_htt_stats_register(struct ath12k *ar)
{
	debugfs_create_file("htt_stats_type", 0600, ar->debug.debugfs_pdev,
			    ar, &fops_htt_stats_type);
	debugfs_create_file("htt_stats", 0400, ar->debug.debugfs_pdev,
			    ar, &fops_dump_htt_stats);
	debugfs_create_file("htt_stats_reset", 0200, ar->debug.debugfs_pdev,
			    ar, &fops_htt_stats_reset);
}

