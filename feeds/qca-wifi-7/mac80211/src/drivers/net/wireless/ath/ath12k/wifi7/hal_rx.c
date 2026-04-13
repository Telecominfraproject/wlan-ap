// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "../debug.h"
#include "../hif.h"
#include "hal.h"
#include "hal_rx.h"

static
void ath12k_wifi7_hal_reo_set_desc_hdr(struct hal_desc_header *hdr,
				       u8 owner, u8 buffer_type, u32 magic)
{
	hdr->info0 = le32_encode_bits(owner, HAL_DESC_HDR_INFO0_OWNER) |
		     le32_encode_bits(buffer_type, HAL_DESC_HDR_INFO0_BUF_TYPE);

	/* Magic pattern in reserved bits for debugging */
	hdr->info0 |= le32_encode_bits(magic, HAL_DESC_HDR_INFO0_DBG_RESERVED);
}

static int ath12k_wifi7_hal_reo_cmd_queue_stats(struct hal_tlv_64_hdr *tlv,
						struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_get_queue_stats *desc;

	tlv->tl = le64_encode_bits(HAL_REO_GET_QUEUE_STATS, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_reo_get_queue_stats *)tlv->value;
	memset_startat(desc, 0, queue_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);
	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->queue_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 = le32_encode_bits(cmd->addr_hi,
				       HAL_REO_GET_QUEUE_STATS_INFO0_QUEUE_ADDR_HI);
	if (cmd->flag & HAL_REO_CMD_FLG_STATS_CLEAR)
		desc->info0 |= cpu_to_le32(HAL_REO_GET_QUEUE_STATS_INFO0_CLEAR_STATS);

	return le32_get_bits(desc->cmd.info0, HAL_REO_CMD_HDR_INFO0_CMD_NUMBER);
}

static int ath12k_wifi7_hal_reo_cmd_flush_cache(struct ath12k_hal *hal,
						struct hal_tlv_64_hdr *tlv,
						struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_flush_cache *desc;
	u8 avail_slot = ffz(hal->avail_blk_resource);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_BLOCK_LATER) {
		if (avail_slot >= HAL_MAX_AVAIL_BLK_RES)
			return -ENOSPC;

		hal->current_blk_index = avail_slot;
	}

	tlv->tl = le64_encode_bits(HAL_REO_FLUSH_CACHE, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_reo_flush_cache *)tlv->value;
	memset_startat(desc, 0, cache_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);
	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->cache_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 = le32_encode_bits(cmd->addr_hi,
				       HAL_REO_FLUSH_CACHE_INFO0_CACHE_ADDR_HI);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_FWD_ALL_MPDUS)
		desc->info0 |= cpu_to_le32(HAL_REO_FLUSH_CACHE_INFO0_FWD_ALL_MPDUS);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_BLOCK_LATER) {
		desc->info0 |= cpu_to_le32(HAL_REO_FLUSH_CACHE_INFO0_BLOCK_CACHE_USAGE);
		desc->info0 |=
			le32_encode_bits(avail_slot,
					 HAL_REO_FLUSH_CACHE_INFO0_BLOCK_RESRC_IDX);
	}

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_NO_INVAL)
		desc->info0 |= cpu_to_le32(HAL_REO_FLUSH_CACHE_INFO0_FLUSH_WO_INVALIDATE);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_ALL)
		desc->info0 |= cpu_to_le32(HAL_REO_FLUSH_CACHE_INFO0_FLUSH_ALL);

	if (cmd->flag & HAL_REO_CMD_FLG_FLUSH_QUEUE_1K_DESC)
		desc->info0 |= cpu_to_le32(HAL_REO_FLUSH_CACHE_INFO0_FLUSH_QUEUE_1K_DESC);

	return le32_get_bits(desc->cmd.info0, HAL_REO_CMD_HDR_INFO0_CMD_NUMBER);
}

static int
ath12k_wifi7_hal_reo_cmd_update_rx_queue(struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_update_rx_queue *desc;

	tlv->tl = le64_encode_bits(HAL_REO_UPDATE_RX_REO_QUEUE, HAL_TLV_HDR_TAG) |
		  le64_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	desc = (struct hal_reo_update_rx_queue *)tlv->value;
	memset_startat(desc, 0, queue_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);
	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->queue_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 =
		le32_encode_bits(cmd->addr_hi,
				 HAL_REO_UPD_RX_QUEUE_INFO0_QUEUE_ADDR_HI) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_RX_QUEUE_NUM),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RX_QUEUE_NUM) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_VLD),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_VLD) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_ALDC),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_ASSOC_LNK_DESC_CNT) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_DIS_DUP_DETECTION),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_DIS_DUP_DETECTION) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SOFT_REORDER_EN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SOFT_REORDER_EN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_AC),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_AC) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_BAR),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BAR) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_RETRY),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RETRY) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_CHECK_2K_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_CHECK_2K_MODE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_OOR_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_OOR_MODE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_BA_WINDOW_SIZE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BA_WINDOW_SIZE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_CHECK),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_CHECK) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_EVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_EVEN_PN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_UNEVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_UNEVEN_PN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_HANDLE_ENABLE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_HANDLE_ENABLE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_SIZE),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_SIZE) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_IGNORE_AMPDU_FLG),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_IGNORE_AMPDU_FLG) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SVLD),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SVLD) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SSN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SSN) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_SEQ_2K_ERR),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SEQ_2K_ERR) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN_VALID),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_VALID) |
		le32_encode_bits(!!(cmd->upd0 & HAL_REO_CMD_UPD0_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN);

	desc->info1 =
		le32_encode_bits(cmd->rx_queue_num,
				 HAL_REO_UPD_RX_QUEUE_INFO1_RX_QUEUE_NUMBER) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_VLD),
				 HAL_REO_UPD_RX_QUEUE_INFO1_VLD) |
		le32_encode_bits(u32_get_bits(cmd->upd1, HAL_REO_CMD_UPD1_ALDC),
				 HAL_REO_UPD_RX_QUEUE_INFO1_ASSOC_LNK_DESC_COUNTER) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_DIS_DUP_DETECTION),
				 HAL_REO_UPD_RX_QUEUE_INFO1_DIS_DUP_DETECTION) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_SOFT_REORDER_EN),
				 HAL_REO_UPD_RX_QUEUE_INFO1_SOFT_REORDER_EN) |
		le32_encode_bits(u32_get_bits(cmd->upd1, HAL_REO_CMD_UPD1_AC),
				 HAL_REO_UPD_RX_QUEUE_INFO1_AC) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_BAR),
				 HAL_REO_UPD_RX_QUEUE_INFO1_BAR) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_CHECK_2K_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO1_CHECK_2K_MODE) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_RETRY),
				 HAL_REO_UPD_RX_QUEUE_INFO1_RETRY) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_OOR_MODE),
				 HAL_REO_UPD_RX_QUEUE_INFO1_OOR_MODE) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_PN_CHECK),
				 HAL_REO_UPD_RX_QUEUE_INFO1_PN_CHECK) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_EVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO1_EVEN_PN) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_UNEVEN_PN),
				 HAL_REO_UPD_RX_QUEUE_INFO1_UNEVEN_PN) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_PN_HANDLE_ENABLE),
				 HAL_REO_UPD_RX_QUEUE_INFO1_PN_HANDLE_ENABLE) |
		le32_encode_bits(!!(cmd->upd1 & HAL_REO_CMD_UPD1_IGNORE_AMPDU_FLG),
				 HAL_REO_UPD_RX_QUEUE_INFO1_IGNORE_AMPDU_FLG);

	if (cmd->pn_size == 24)
		cmd->pn_size = HAL_RX_REO_QUEUE_PN_SIZE_24;
	else if (cmd->pn_size == 48)
		cmd->pn_size = HAL_RX_REO_QUEUE_PN_SIZE_48;
	else if (cmd->pn_size == 128)
		cmd->pn_size = HAL_RX_REO_QUEUE_PN_SIZE_128;

	if (cmd->ba_window_size < 1)
		cmd->ba_window_size = 1;

	if (cmd->ba_window_size == 1)
		cmd->ba_window_size++;

	desc->info2 =
		le32_encode_bits(cmd->ba_window_size - 1,
				 HAL_REO_UPD_RX_QUEUE_INFO2_BA_WINDOW_SIZE) |
		le32_encode_bits(cmd->pn_size, HAL_REO_UPD_RX_QUEUE_INFO2_PN_SIZE) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_SVLD),
				 HAL_REO_UPD_RX_QUEUE_INFO2_SVLD) |
		le32_encode_bits(u32_get_bits(cmd->upd2, HAL_REO_CMD_UPD2_SSN),
				 HAL_REO_UPD_RX_QUEUE_INFO2_SSN) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_SEQ_2K_ERR),
				 HAL_REO_UPD_RX_QUEUE_INFO2_SEQ_2K_ERR) |
		le32_encode_bits(!!(cmd->upd2 & HAL_REO_CMD_UPD2_PN_ERR),
				 HAL_REO_UPD_RX_QUEUE_INFO2_PN_ERR);

	return le32_get_bits(desc->cmd.info0, HAL_REO_CMD_HDR_INFO0_CMD_NUMBER);
}

static int ath12k_hal_reo_cmd_flush_queue(struct hal_tlv_64_hdr *tlv,
					  struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_flush_queue *desc = (struct hal_reo_flush_queue *)tlv->value;

	tlv->tl = u32_encode_bits(HAL_REO_FLUSH_QUEUE, HAL_TLV_HDR_TAG) |
		  u32_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	memset_startat(desc, 0, desc_addr_lo);

	desc->cmd.info0 &= ~cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->desc_addr_lo = cpu_to_le32(cmd->addr_lo);
	desc->info0 = le32_encode_bits(cmd->addr_hi,
				       HAL_REO_FLUSH_QUEUE_INFO0_DESC_ADDR_HI);

	return le32_get_bits(desc->cmd.info0, HAL_REO_CMD_HDR_INFO0_CMD_NUMBER);
}

static int ath12k_hal_reo_cmd_unblock_cache(struct hal_tlv_64_hdr *tlv,
					    struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_reo_unblock_cache *desc =
		(struct hal_reo_unblock_cache *)tlv->value;

	tlv->tl = u32_encode_bits(HAL_REO_UNBLOCK_CACHE, HAL_TLV_HDR_TAG) |
		u32_encode_bits(sizeof(*desc), HAL_TLV_HDR_LEN);

	memset_startat(desc, 0, info0);

	desc->cmd.info0 &= ~cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	if (cmd->flag & HAL_REO_CMD_FLG_NEED_STATUS)
		desc->cmd.info0 |= cpu_to_le32(HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED);

	desc->info0 = HAL_REO_UNBLOCK_CACHE_INFO0_UNBLK_CACHE;

	return le32_get_bits(desc->cmd.info0, HAL_REO_CMD_HDR_INFO0_CMD_NUMBER);
}

int ath12k_wifi7_hal_reo_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_reo_cmd_type type,
				  struct ath12k_hal_reo_cmd *cmd)
{
	struct hal_tlv_64_hdr *reo_desc;
	int ret;

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);
	reo_desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!reo_desc) {
		ret = -ENOBUFS;
		goto out;
	}

	switch (type) {
	case HAL_REO_CMD_GET_QUEUE_STATS:
		ret = ath12k_wifi7_hal_reo_cmd_queue_stats(reo_desc, cmd);
		break;
	case HAL_REO_CMD_FLUSH_CACHE:
		ret = ath12k_wifi7_hal_reo_cmd_flush_cache(&ab->hal, reo_desc,
							   cmd);
		break;
	case HAL_REO_CMD_UPDATE_RX_QUEUE:
		ret = ath12k_wifi7_hal_reo_cmd_update_rx_queue(reo_desc, cmd);
		break;
	case HAL_REO_CMD_FLUSH_QUEUE:
		ret = ath12k_hal_reo_cmd_flush_queue(reo_desc, cmd);
		break;
	case HAL_REO_CMD_UNBLOCK_CACHE:
		ret = ath12k_hal_reo_cmd_unblock_cache(reo_desc, cmd);
		break;
	case HAL_REO_CMD_FLUSH_TIMEOUT_LIST:
		ath12k_warn(ab, "Unsupported reo command %d\n", type);
		ret = -EOPNOTSUPP;
		break;
	default:
		ath12k_warn(ab, "Unknown reo command %d\n", type);
		ret = -EINVAL;
		break;
	}

out:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

void
ath12k_wifi7_hal_rx_msdu_link_info_get(struct hal_rx_msdu_link *link,
				       u32 *num_msdus, u32 *msdu_cookies,
				       enum hal_rx_buf_return_buf_manager *rbm)
{
	struct hal_rx_msdu_details *msdu;
	u32 val;
	int i;

	*num_msdus = HAL_NUM_RX_MSDUS_PER_LINK_DESC;

	msdu = &link->msdu_link[0];
	*rbm = le32_get_bits(msdu->buf_addr_info.info1,
			     BUFFER_ADDR_INFO1_RET_BUF_MGR);

	for (i = 0; i < *num_msdus; i++) {
		msdu = &link->msdu_link[i];

		val = le32_get_bits(msdu->buf_addr_info.info0,
				    BUFFER_ADDR_INFO0_ADDR);
		if (val == 0) {
			*num_msdus = i;
			break;
		}
		*msdu_cookies = le32_get_bits(msdu->buf_addr_info.info1,
					      BUFFER_ADDR_INFO1_SW_COOKIE);
		msdu_cookies++;
	}
}

int ath12k_wifi7_hal_desc_reo_parse_err(struct ath12k_dp *dp,
					struct hal_reo_dest_ring *desc,
					dma_addr_t *paddr, u32 *desc_bank)
{
	struct ath12k_base *ab = dp->ab;
	enum hal_reo_dest_ring_push_reason push_reason;
	enum hal_reo_dest_ring_error_code err_code;
	u32 cookie, val;

	push_reason = le32_get_bits(desc->info0,
				    HAL_REO_DEST_RING_INFO0_PUSH_REASON);
	err_code = le32_get_bits(desc->info0,
				 HAL_REO_DEST_RING_INFO0_ERROR_CODE);
	dp->device_stats.wbm_err.reo_error[err_code]++;

	if (push_reason != HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED &&
	    push_reason != HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION) {
		ath12k_warn(ab, "expected error push reason code, received %d\n",
			    push_reason);
		return -EINVAL;
	}

	val = le32_get_bits(desc->info0, HAL_REO_DEST_RING_INFO0_BUFFER_TYPE);
	if (val == HAL_REO_DEST_RING_BUFFER_TYPE_MSDU) {
		return -EOPNOTSUPP;
	} else if (val != HAL_REO_DEST_RING_BUFFER_TYPE_LINK_DESC) {
		ath12k_warn(ab, "expected buffer type link_desc, val %d", val);
		return -EINVAL;
	}

	ath12k_wifi7_hal_rx_reo_ent_paddr_get(ab, &desc->buf_addr_info, paddr,
					      &cookie);
	*desc_bank = u32_get_bits(cookie, DP_LINK_DESC_BANK_MASK);

	return 0;
}

int ath12k_wifi7_hal_wbm_desc_parse_err(struct ath12k_dp *dp, void *desc,
					struct hal_rx_wbm_rel_info *rel_info)
{
	struct hal_wbm_release_ring *wbm_desc = desc;
	struct hal_wbm_release_ring_cc_rx *wbm_cc_desc = desc;
	struct ath12k_base *ab = dp->ab;
	enum hal_wbm_rel_desc_type type;
	enum hal_wbm_rel_src_module rel_src;
	bool hw_cc_done;
	u64 desc_va;
	u32 val;
	int rxdma_push_reason, rxdma_error_code, reo_push_reason, reo_error_code;
	int wbm_err;

	type = le32_get_bits(wbm_desc->info0, HAL_WBM_RELEASE_INFO0_DESC_TYPE);
	/* We expect only WBM_REL buffer type */
	if (type != HAL_WBM_REL_DESC_TYPE_REL_MSDU) {
		WARN_ON(1);
		return -EINVAL;
	}

	rel_src = le32_get_bits(wbm_desc->info0,
				HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE);
	if (rel_src != HAL_WBM_REL_SRC_MODULE_RXDMA &&
	    rel_src != HAL_WBM_REL_SRC_MODULE_REO) {
		rxdma_push_reason = le32_get_bits(wbm_desc->info0,
						  HAL_WBM_RELEASE_RX_INFO0_RXDMA_PUSH_REASON);
		rxdma_error_code = le32_get_bits(wbm_desc->info0,
						 HAL_WBM_RELEASE_RX_INFO0_RXDMA_ERROR_CODE);
		reo_push_reason = le32_get_bits(wbm_desc->info0,
						HAL_WBM_RELEASE_RX_INFO0_REO_PUSH_REASON);
		reo_error_code = le32_get_bits(wbm_desc->info0,
					       HAL_WBM_RELEASE_RX_INFO0_REO_ERROR_CODE);
		wbm_err =  le32_get_bits(wbm_desc->info0,
					 HAL_WBM_RELEASE_RX_INFO0_WBM_INTERNAL_ERROR);
		ath12k_warn(ab, "Invalid src rxmda(%d %d) reo(%d %d) wbm err %d",
			    rxdma_push_reason, rxdma_error_code,
			    rxdma_error_code, reo_push_reason, wbm_err);
		return -EINVAL;
	}

	/* The format of wbm rel ring desc changes based on the
	 * hw cookie conversion status
	 */
	hw_cc_done = le32_get_bits(wbm_desc->info0,
				   HAL_WBM_RELEASE_RX_INFO0_CC_STATUS);

	if (!hw_cc_done) {
		val = le32_get_bits(wbm_desc->buf_addr_info.info1,
				    BUFFER_ADDR_INFO1_RET_BUF_MGR);
		if (val != HAL_RX_BUF_RBM_SW5_BM) {
			dp->device_stats.invalid_rbm++;
			return -EINVAL;
		}

		rel_info->cookie = le32_get_bits(wbm_desc->buf_addr_info.info1,
						 BUFFER_ADDR_INFO1_SW_COOKIE);

		rel_info->rx_desc = NULL;
	} else {
		val = le32_get_bits(wbm_cc_desc->info0,
				    HAL_WBM_RELEASE_RX_CC_INFO0_RBM);
		if (val != HAL_RX_BUF_RBM_SW5_BM) {
			dp->device_stats.invalid_rbm++;
			return -EINVAL;
		}

		rel_info->cookie = le32_get_bits(wbm_cc_desc->info1,
						 HAL_WBM_RELEASE_RX_CC_INFO1_COOKIE);

		desc_va = ((u64)le32_to_cpu(wbm_cc_desc->buf_va_hi) << 32 |
			   le32_to_cpu(wbm_cc_desc->buf_va_lo));
		rel_info->rx_desc =
			(struct ath12k_rx_desc_info *)((unsigned long)desc_va);
	}

	rel_info->err_rel_src = rel_src;
	rel_info->hw_cc_done = hw_cc_done;

	rel_info->first_msdu = le32_get_bits(wbm_desc->info3,
					     HAL_WBM_RELEASE_INFO3_FIRST_MSDU);
	rel_info->last_msdu = le32_get_bits(wbm_desc->info3,
					    HAL_WBM_RELEASE_INFO3_LAST_MSDU);
	rel_info->continuation = le32_get_bits(wbm_desc->info3,
					       HAL_WBM_RELEASE_INFO3_CONTINUATION);

	if (rel_info->err_rel_src == HAL_WBM_REL_SRC_MODULE_REO) {
		rel_info->push_reason =
			le32_get_bits(wbm_desc->info0,
				      HAL_WBM_RELEASE_INFO0_REO_PUSH_REASON);
		rel_info->err_code =
			le32_get_bits(wbm_desc->info0,
				      HAL_WBM_RELEASE_INFO0_REO_ERROR_CODE);
	} else {
		rel_info->push_reason =
			le32_get_bits(wbm_desc->info0,
				      HAL_WBM_RELEASE_INFO0_RXDMA_PUSH_REASON);
		rel_info->err_code =
			le32_get_bits(wbm_desc->info0,
				      HAL_WBM_RELEASE_INFO0_RXDMA_ERROR_CODE);
	}

	rel_info->peer_metadata = wbm_desc->info2;

	return 0;
}

void ath12k_wifi7_hal_rx_reo_ent_paddr_get(struct ath12k_base *ab,
					   struct ath12k_buffer_addr *buff_addr,
					   dma_addr_t *paddr, u32 *cookie)
{
	*paddr = ((u64)(le32_get_bits(buff_addr->info1,
				      BUFFER_ADDR_INFO1_ADDR)) << 32) |
		le32_get_bits(buff_addr->info0, BUFFER_ADDR_INFO0_ADDR);

	*cookie = le32_get_bits(buff_addr->info1, BUFFER_ADDR_INFO1_SW_COOKIE);
}

void ath12k_wifi7_hal_rx_reo_ent_buf_paddr_get(void *rx_desc, dma_addr_t *paddr,
					       u32 *sw_cookie,
					       struct ath12k_buffer_addr **pp_buf_addr,
					       u8 *rbm, u32 *msdu_cnt)
{
	struct hal_reo_entrance_ring *reo_ent_ring =
		(struct hal_reo_entrance_ring *)rx_desc;
	struct ath12k_buffer_addr *buf_addr_info;
	struct rx_mpdu_desc *rx_mpdu_desc_info_details;

	rx_mpdu_desc_info_details =
			(struct rx_mpdu_desc *)&reo_ent_ring->rx_mpdu_info;

	*msdu_cnt = le32_get_bits(rx_mpdu_desc_info_details->info0,
				  RX_MPDU_DESC_INFO0_MSDU_COUNT);

	buf_addr_info = (struct ath12k_buffer_addr *)&reo_ent_ring->buf_addr_info;

	*paddr = (((u64)le32_get_bits(buf_addr_info->info1,
				      BUFFER_ADDR_INFO1_ADDR)) << 32) |
			le32_get_bits(buf_addr_info->info0,
				      BUFFER_ADDR_INFO0_ADDR);

	*sw_cookie = le32_get_bits(buf_addr_info->info1,
				   BUFFER_ADDR_INFO1_SW_COOKIE);
	*rbm = le32_get_bits(buf_addr_info->info1,
			     BUFFER_ADDR_INFO1_RET_BUF_MGR);

	*pp_buf_addr = (void *)buf_addr_info;
}

void ath12k_wifi7_hal_rx_msdu_list_get(void *desc,
				       void *list,
				       u16 *num_msdus)
{
	struct hal_rx_msdu_link *link_desc = (struct hal_rx_msdu_link *)desc;
	struct hal_rx_msdu_list *msdu_list = (struct hal_rx_msdu_list *)list;
	struct hal_rx_msdu_details *msdu_details = NULL;
	struct rx_msdu_desc *msdu_desc_info = NULL;
	u32 last = 0, first = 0;
	u8 tmp = 0;
	int i;

	last = u32_encode_bits(last, RX_MSDU_DESC_INFO0_LAST_MSDU_IN_MPDU);
	first = u32_encode_bits(first, RX_MSDU_DESC_INFO0_FIRST_MSDU_IN_MPDU);
	msdu_details = &link_desc->msdu_link[0];

	for (i = 0; i < HAL_RX_NUM_MSDU_DESC; i++) {
		if (!i && le32_get_bits(msdu_details[i].buf_addr_info.info0,
					BUFFER_ADDR_INFO0_ADDR) == 0)
			break;
		if (le32_get_bits(msdu_details[i].buf_addr_info.info0,
				  BUFFER_ADDR_INFO0_ADDR) == 0) {
			msdu_desc_info = &msdu_details[i - 1].rx_msdu_info;
			msdu_desc_info->info0 |= cpu_to_le32(last);
			break;
		}
		msdu_desc_info = &msdu_details[i].rx_msdu_info;

		if (!i)
			msdu_desc_info->info0 |= cpu_to_le32(first);
		else if (i == (HAL_RX_NUM_MSDU_DESC - 1))
			msdu_desc_info->info0 |= cpu_to_le32(last);
		msdu_list->msdu_info[i].msdu_flags = le32_to_cpu(msdu_desc_info->info0);
		msdu_list->msdu_info[i].msdu_len =
			 HAL_RX_MSDU_PKT_LENGTH_GET(msdu_desc_info->info0);
		msdu_list->sw_cookie[i] =
			le32_get_bits(msdu_details[i].buf_addr_info.info1,
				      BUFFER_ADDR_INFO1_SW_COOKIE);
		tmp = le32_get_bits(msdu_details[i].buf_addr_info.info1,
				    BUFFER_ADDR_INFO1_RET_BUF_MGR);
		msdu_list->paddr[i] =
			((u64)(le32_get_bits(msdu_details[i].buf_addr_info.info1,
					     BUFFER_ADDR_INFO1_ADDR)) << 32) |
			le32_get_bits(msdu_details[i].buf_addr_info.info0,
				      BUFFER_ADDR_INFO0_ADDR);
		msdu_list->rbm[i] = tmp;
	}
	*num_msdus = i;
}

void
ath12k_wifi7_hal_rx_msdu_link_desc_set(struct ath12k_base *ab,
				       struct hal_wbm_release_ring *desc,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action)
{
	desc->buf_addr_info = *buf_addr_info;
	desc->info0 |= le32_encode_bits(HAL_WBM_REL_SRC_MODULE_SW,
					HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE) |
		    le32_encode_bits(action, HAL_WBM_RELEASE_INFO0_BM_ACTION) |
		    le32_encode_bits(HAL_WBM_REL_DESC_TYPE_MSDU_LINK,
				     HAL_WBM_RELEASE_INFO0_DESC_TYPE);
}

void ath12k_wifi7_hal_reo_status_queue_stats(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status)
{
	struct hal_reo_get_queue_stats_status *desc =
		(struct hal_reo_get_queue_stats_status *)tlv->value;

	status->uniform_hdr.cmd_num =
				le32_get_bits(desc->hdr.info0,
					      HAL_REO_STATUS_HDR_INFO0_STATUS_NUM);
	status->uniform_hdr.cmd_status =
				le32_get_bits(desc->hdr.info0,
					      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	ath12k_dbg(ab, ATH12K_DBG_HAL, "Queue stats status:\n");
	ath12k_dbg(ab, ATH12K_DBG_HAL, "header: cmd_num %d status %d\n",
		   status->uniform_hdr.cmd_num,
		   status->uniform_hdr.cmd_status);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "ssn %u cur_idx %u\n",
		   le32_get_bits(desc->info0,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO0_SSN),
		   le32_get_bits(desc->info0,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO0_CUR_IDX));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "pn = [%08x, %08x, %08x, %08x]\n",
		   desc->pn[0], desc->pn[1], desc->pn[2], desc->pn[3]);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "last_rx: enqueue_tstamp %08x dequeue_tstamp %08x\n",
		   desc->last_rx_enqueue_timestamp,
		   desc->last_rx_dequeue_timestamp);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "rx_bitmap [%08x %08x %08x %08x %08x %08x %08x %08x]\n",
		   desc->rx_bitmap[0], desc->rx_bitmap[1], desc->rx_bitmap[2],
		   desc->rx_bitmap[3], desc->rx_bitmap[4], desc->rx_bitmap[5],
		   desc->rx_bitmap[6], desc->rx_bitmap[7]);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "count: cur_mpdu %u cur_msdu %u\n",
		   le32_get_bits(desc->info1,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO1_MPDU_COUNT),
		   le32_get_bits(desc->info1,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO1_MSDU_COUNT));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "fwd_timeout %u fwd_bar %u dup_count %u\n",
		   le32_get_bits(desc->info2,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO2_TIMEOUT_COUNT),
		   le32_get_bits(desc->info2,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO2_FDTB_COUNT),
		   le32_get_bits(desc->info2,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO2_DUPLICATE_COUNT));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "frames_in_order %u bar_rcvd %u\n",
		   le32_get_bits(desc->info3,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO3_FIO_COUNT),
		   le32_get_bits(desc->info3,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO3_BAR_RCVD_CNT));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "num_mpdus %d num_msdus %d total_bytes %d\n",
		   desc->num_mpdu_frames, desc->num_msdu_frames,
		   desc->total_bytes);
	ath12k_dbg(ab, ATH12K_DBG_HAL, "late_rcvd %u win_jump_2k %u hole_cnt %u\n",
		   le32_get_bits(desc->info4,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO4_LATE_RX_MPDU),
		   le32_get_bits(desc->info2,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO2_WINDOW_JMP2K),
		   le32_get_bits(desc->info4,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO4_HOLE_COUNT));
	ath12k_dbg(ab, ATH12K_DBG_HAL, "looping count %u\n",
		   le32_get_bits(desc->info5,
				 HAL_REO_GET_QUEUE_STATS_STATUS_INFO5_LOOPING_CNT));
}

void ath12k_wifi7_hal_reo_flush_queue_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status)
{
	struct hal_reo_flush_queue_status *desc =
		(struct hal_reo_flush_queue_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_STATUS_NUM);
	status->uniform_hdr.cmd_status =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);
	status->u.flush_queue.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_QUEUE_INFO0_ERR_DETECTED);
}

void
ath12k_wifi7_hal_reo_flush_cache_status(struct ath12k_base *ab,
					struct hal_tlv_64_hdr *tlv,
					struct hal_reo_status *status)
{
	struct ath12k_hal *hal = &ab->hal;
	struct hal_reo_flush_cache_status *desc =
		(struct hal_reo_flush_cache_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_STATUS_NUM);
	status->uniform_hdr.cmd_status =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.flush_cache.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_CACHE_STATUS_INFO0_IS_ERR);
	status->u.flush_cache.err_code =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_BLOCK_ERR_CODE);
	if (!status->u.flush_cache.err_code)
		hal->avail_blk_resource |= BIT(hal->current_blk_index);

	status->u.flush_cache.cache_controller_flush_status_hit =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_STATUS_HIT);

	status->u.flush_cache.cache_controller_flush_status_desc_type =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_DESC_TYPE);
	status->u.flush_cache.cache_controller_flush_status_client_id =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_CLIENT_ID);
	status->u.flush_cache.cache_controller_flush_status_err =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_ERR);
	status->u.flush_cache.cache_controller_flush_status_cnt =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_COUNT);
}

void ath12k_wifi7_hal_reo_unblk_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status)
{
	struct ath12k_hal *hal = &ab->hal;
	struct hal_reo_unblock_cache_status *desc =
		(struct hal_reo_unblock_cache_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_STATUS_NUM);
	status->uniform_hdr.cmd_status =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.unblock_cache.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_IS_ERR);
	status->u.unblock_cache.unblock_type =
			le32_get_bits(desc->info0,
				      HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_TYPE);

	if (!status->u.unblock_cache.err_detected &&
	    status->u.unblock_cache.unblock_type ==
	    HAL_REO_STATUS_UNBLOCK_BLOCKING_RESOURCE)
		hal->avail_blk_resource &= ~BIT(hal->current_blk_index);
}

void
ath12k_wifi7_hal_reo_flush_timeout_list_status(struct ath12k_base *ab,
					       struct hal_tlv_64_hdr *tlv,
					       struct hal_reo_status *status)
{
	struct hal_reo_flush_timeout_list_status *desc =
		(struct hal_reo_flush_timeout_list_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_STATUS_NUM);
	status->uniform_hdr.cmd_status =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.timeout_list.err_detected =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_IS_ERR);
	status->u.timeout_list.list_empty =
			le32_get_bits(desc->info0,
				      HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_LIST_EMPTY);

	status->u.timeout_list.release_desc_cnt =
		le32_get_bits(desc->info1,
			      HAL_REO_FLUSH_TIMEOUT_STATUS_INFO1_REL_DESC_COUNT);
	status->u.timeout_list.fwd_buf_cnt =
		le32_get_bits(desc->info0,
			      HAL_REO_FLUSH_TIMEOUT_STATUS_INFO1_FWD_BUF_COUNT);
}

void
ath12k_wifi7_hal_reo_desc_thresh_reached_status(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status)
{
	struct hal_reo_desc_thresh_reached_status *desc =
		(struct hal_reo_desc_thresh_reached_status *)tlv->value;

	status->uniform_hdr.cmd_num =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_STATUS_NUM);
	status->uniform_hdr.cmd_status =
			le32_get_bits(desc->hdr.info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);

	status->u.desc_thresh_reached.threshold_idx =
		le32_get_bits(desc->info0,
			      HAL_REO_DESC_THRESH_STATUS_INFO0_THRESH_INDEX);

	status->u.desc_thresh_reached.link_desc_counter0 =
		le32_get_bits(desc->info1,
			      HAL_REO_DESC_THRESH_STATUS_INFO1_LINK_DESC_COUNTER0);

	status->u.desc_thresh_reached.link_desc_counter1 =
		le32_get_bits(desc->info2,
			      HAL_REO_DESC_THRESH_STATUS_INFO2_LINK_DESC_COUNTER1);

	status->u.desc_thresh_reached.link_desc_counter2 =
		le32_get_bits(desc->info3,
			      HAL_REO_DESC_THRESH_STATUS_INFO3_LINK_DESC_COUNTER2);

	status->u.desc_thresh_reached.link_desc_counter_sum =
		le32_get_bits(desc->info4,
			      HAL_REO_DESC_THRESH_STATUS_INFO4_LINK_DESC_COUNTER_SUM);
}

void ath12k_wifi7_hal_reo_update_rx_reo_queue_status(struct ath12k_base *ab,
						     struct hal_tlv_64_hdr *tlv,
						     struct hal_reo_status *status)
{
	struct hal_reo_status_hdr *desc =
		(struct hal_reo_status_hdr *)tlv->value;

	status->uniform_hdr.cmd_num =
			le32_get_bits(desc->info0,
				      HAL_REO_STATUS_HDR_INFO0_STATUS_NUM);
	status->uniform_hdr.cmd_status =
			le32_get_bits(desc->info0,
				      HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS);
}

u32 ath12k_wifi7_hal_reo_qdesc_size(u32 ba_window_size, u8 tid)
{
	u32 num_ext_desc, num_1k_desc = 0;

	if (ba_window_size <= 1) {
		if (tid != HAL_DESC_REO_NON_QOS_TID)
			num_ext_desc = 1;
		else
			num_ext_desc = 0;

	} else if (ba_window_size <= 105) {
		num_ext_desc = 1;
	} else if (ba_window_size <= 210) {
		num_ext_desc = 2;
	} else if (ba_window_size <= 256) {
		num_ext_desc = 3;
	} else {
		num_ext_desc = 10;
		num_1k_desc = 1;
	}

	return sizeof(struct hal_rx_reo_queue) +
		(num_ext_desc * sizeof(struct hal_rx_reo_queue_ext)) +
		(num_1k_desc * sizeof(struct hal_rx_reo_queue_1k));
}

void ath12k_wifi7_hal_reo_qdesc_setup(struct hal_rx_reo_queue *qdesc,
				      int tid, u32 ba_window_size,
				      u32 start_seq, enum hal_pn_type type)
{
	struct hal_rx_reo_queue_ext *ext_desc;

	ath12k_wifi7_hal_reo_set_desc_hdr(&qdesc->desc_hdr, HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_0);

	qdesc->rx_queue_num = le32_encode_bits(tid, HAL_RX_REO_QUEUE_RX_QUEUE_NUMBER);

	qdesc->info0 =
		le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_VLD) |
		le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_ASSOC_LNK_DESC_COUNTER) |
		le32_encode_bits(ath12k_tid_to_ac(tid), HAL_RX_REO_QUEUE_INFO0_AC);

	if (ba_window_size < 1)
		ba_window_size = 1;

	if (ba_window_size == 1 && tid != HAL_DESC_REO_NON_QOS_TID)
		ba_window_size++;

	if (ba_window_size == 1)
		qdesc->info0 |= le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_RETRY);

	qdesc->info0 |= le32_encode_bits(ba_window_size - 1,
					 HAL_RX_REO_QUEUE_INFO0_BA_WINDOW_SIZE);
	switch (type) {
	case HAL_PN_TYPE_NONE:
	case HAL_PN_TYPE_WAPI_EVEN:
	case HAL_PN_TYPE_WAPI_UNEVEN:
		break;
	case HAL_PN_TYPE_WPA:
		qdesc->info0 |=
			le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_PN_CHECK) |
			le32_encode_bits(HAL_RX_REO_QUEUE_PN_SIZE_48,
					 HAL_RX_REO_QUEUE_INFO0_PN_SIZE);
		break;
	}

	/* TODO: Set Ignore ampdu flags based on BA window size and/or
	 * AMPDU capabilities
	 */
	qdesc->info0 |= le32_encode_bits(1, HAL_RX_REO_QUEUE_INFO0_IGNORE_AMPDU_FLG);

	qdesc->info1 |= le32_encode_bits(0, HAL_RX_REO_QUEUE_INFO1_SVLD);

	if (start_seq <= 0xfff)
		qdesc->info1 = le32_encode_bits(start_seq,
						HAL_RX_REO_QUEUE_INFO1_SSN);

	if (tid == HAL_DESC_REO_NON_QOS_TID)
		return;

	ext_desc = qdesc->ext_desc;

	/* TODO: HW queue descriptors are currently allocated for max BA
	 * window size for all QOS TIDs so that same descriptor can be used
	 * later when ADDBA request is received. This should be changed to
	 * allocate HW queue descriptors based on BA window size being
	 * negotiated (0 for non BA cases), and reallocate when BA window
	 * size changes and also send WMI message to FW to change the REO
	 * queue descriptor in Rx peer entry as part of dp_rx_tid_update.
	 */
	memset(ext_desc, 0, 3 * sizeof(*ext_desc));
	ath12k_wifi7_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_1);
	ext_desc++;
	ath12k_wifi7_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_2);
	ext_desc++;
	ath12k_wifi7_hal_reo_set_desc_hdr(&ext_desc->desc_hdr,
					  HAL_DESC_REO_OWNED,
					  HAL_DESC_REO_QUEUE_EXT_DESC,
					  REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_3);
}

void ath12k_wifi7_hal_reo_init_cmd_ring(struct ath12k_base *ab,
					struct hal_srng *srng)
{
	struct hal_srng_params params;
	struct hal_tlv_64_hdr *tlv;
	struct hal_reo_get_queue_stats *desc;
	int i, cmd_num = 1;
	int entry_size;
	u8 *entry;

	memset(&params, 0, sizeof(params));

	entry_size = ath12k_hal_srng_get_entrysize(ab, HAL_REO_CMD);
	ath12k_hal_srng_get_params(ab, srng, &params);
	entry = (u8 *)params.ring_base_vaddr;

	for (i = 0; i < params.num_entries; i++) {
		tlv = (struct hal_tlv_64_hdr *)entry;
		desc = (struct hal_reo_get_queue_stats *)tlv->value;
		desc->cmd.info0 = le32_encode_bits(cmd_num++,
						   HAL_REO_CMD_HDR_INFO0_CMD_NUMBER);
		entry += entry_size;
	}
}

void ath12k_hal_reo_ring_ctrl_hash_ix0_setup(struct ath12k_base *ab)
{
	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 curr, val;

	curr = ath12k_hif_read32(ab, reo_base + HAL_REO1_DEST_RING_CTRL_IX_0);
	val = curr & ~(REO_DEST_CTRL_IX_0_RING6_MAP_MASK <<
		       REO_DEST_CTRL_IX_0_RING6_MAP_SHFT);
	val |= (REO2PPE_DST_RING_MAP << REO_DEST_CTRL_IX_0_RING6_MAP_SHFT);

	ath12k_hif_write32(ab, reo_base + HAL_REO1_DEST_RING_CTRL_IX_0,
			   val);
}

void ath12k_wifi7_hal_reo_hw_setup(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;
	u32 ring_hash_map;

	u32 reo_base = HAL_SEQ_WCSS_UMAC_REO_REG;
	u32 val, VI_reorder_timeout;

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_GEN_ENABLE);

	val |= u32_encode_bits(1, HAL_REO1_GEN_ENABLE_AGING_LIST_ENABLE) |
	       u32_encode_bits(1, HAL_REO1_GEN_ENABLE_AGING_FLUSH_ENABLE) |
	       u32_encode_bits(1, HAL_REO1_GEN_ENABLE_INVLDT_CACHE_FOR_ZERO_VLD);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_GEN_ENABLE, val);

	val = ath12k_hif_read32(ab, reo_base + HAL_REO1_MISC_CTRL_ADDR(hal));

	val &= ~(HAL_REO1_MISC_CTL_SPARE_CTRL_DST_RING |
		 HAL_REO1_MISC_CTL_FRAG_DST_RING |
		 HAL_REO1_MISC_CTL_BAR_DST_RING);
	val |= u32_encode_bits(1,
			       HAL_REO1_MISC_CTL_SPARE_CTRL_DST_RING);
	val |= u32_encode_bits(HAL_SRNG_RING_ID_REO2SW0,
			       HAL_REO1_MISC_CTL_FRAG_DST_RING);
	val |= u32_encode_bits(HAL_SRNG_RING_ID_REO2SW0,
			       HAL_REO1_MISC_CTL_BAR_DST_RING);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_MISC_CTRL_ADDR(hal), val);

	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_0(hal),
			   HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_1(hal),
			   HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC);

	if (ath12k_reorder_VI_timeout &&
	    ath12k_reorder_VI_timeout >= MIN_VI_REORDER_TIMEOUT_MS &&
	    ath12k_reorder_VI_timeout <= MAX_VI_REORDER_TIMEOUT_MS) {
		VI_reorder_timeout = ath12k_reorder_VI_timeout * 1000;
	} else {
		VI_reorder_timeout = HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC;
	}

	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_2(hal),
			   VI_reorder_timeout);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_AGING_THRESH_IX_3(hal),
			   HAL_DEFAULT_VO_REO_TIMEOUT_USEC);

	ath12k_hal_reo_ring_ctrl_hash_ix0_setup(ab);

	/* When hash based routing of rx packet is enabled, 32 entries to map
	 * the hash values to the ring will be configured. Each hash entry uses
	 * four bits to map to a particular ring. The ring mapping will be
	 * 0:TCL, 1:SW1, 2:SW2, 3:SW3, 4:SW4, 5:Release, 6:FW and 7:SW5
	 * 8:SW6, 9:SW7, 10:SW8, 11:Not used.
	 */
	ring_hash_map = HAL_WIFI7_HASH_ROUTING_RING_SW1 |
			HAL_WIFI7_HASH_ROUTING_RING_SW2 << 4 |
			HAL_WIFI7_HASH_ROUTING_RING_SW3 << 8 |
			HAL_WIFI7_HASH_ROUTING_RING_SW4 << 12 |
			HAL_WIFI7_HASH_ROUTING_RING_SW1 << 16 |
			HAL_WIFI7_HASH_ROUTING_RING_SW2 << 20 |
			HAL_WIFI7_HASH_ROUTING_RING_SW3 << 24 |
			HAL_WIFI7_HASH_ROUTING_RING_SW4 << 28;

	ath12k_hif_write32(ab, reo_base + HAL_REO1_DEST_RING_CTRL_IX_2,
			   ring_hash_map);
	ath12k_hif_write32(ab, reo_base + HAL_REO1_DEST_RING_CTRL_IX_3,
			   ring_hash_map);
}

void ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab)
{
	struct ath12k_hal *hal = &ab->hal;
	u32 val;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->hw_params->reoq_lut_support)
		return;


	val = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
				HAL_REO1_QDESC_ADDR(hal));

	val |= u32_encode_bits(1, HAL_REO_QDESC_ADDR_READ_CLEAR_QDESC_ARRAY);
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
			   HAL_REO1_QDESC_ADDR(hal), val);

	val &= ~HAL_REO_QDESC_ADDR_READ_CLEAR_QDESC_ARRAY;
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_UMAC_REO_REG +
			   HAL_REO1_QDESC_ADDR(hal), val);
}

#define PMM_REG_OFFSET  4

static void ath12k_hal_get_tsf_reg(u8 mac_id, enum hal_scratch_reg_enum *tsf_enum_low,
				   enum hal_scratch_reg_enum *tsf_enum_hi)
{
	if (!mac_id) {
		*tsf_enum_low = PMM_MAC0_TSF2_OFFSET_LO_US;
		*tsf_enum_hi = PMM_MAC0_TSF2_OFFSET_HI_US;
	} else if (mac_id == 1) {
		*tsf_enum_low = PMM_MAC1_TSF2_OFFSET_LO_US;
		*tsf_enum_hi = PMM_MAC1_TSF2_OFFSET_HI_US;
	}
}

void ath12k_hal_qcn9274_get_tsf2_scratch_reg(struct ath12k_base *ab,
					     u8 mac_id, u64 *value)
{
	enum hal_scratch_reg_enum enum_lo, enum_hi;
	u32 offset_lo, offset_hi;

	ath12k_hal_get_tsf_reg(mac_id, &enum_lo, &enum_hi);

	if (ab->hif.ops->pmm_read32) {
		offset_lo = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(enum_lo));
		offset_hi = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(enum_hi));
	} else if (ab->hw_rev == ATH12K_HW_QCN6432_HW10) {
		offset_lo = ath12k_hif_cmem_read32(ab, PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(enum_lo));
		offset_hi = ath12k_hif_cmem_read32(ab, PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(enum_hi));
	} else {
		offset_lo = ath12k_hif_read32(ab, PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(enum_lo));
		offset_hi = ath12k_hif_read32(ab, PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(enum_hi));
	}

	*value = ((u64)(offset_hi) << 32 | offset_lo);
}

void ath12k_hal_qcn9274_get_tqm_scratch_reg(struct ath12k_base *ab, u64 *value)
{
	u32 offset_lo, offset_hi;

	if (ab->hif.ops->pmm_read32) {
		offset_lo = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_LO_US));
		offset_hi = ath12k_hif_pmm_read32(ab, ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_HI_US));
	} else if (ab->hw_rev == ATH12K_HW_QCN6432_HW10) {
		offset_lo = ath12k_hif_cmem_read32(ab,
						   PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_LO_US));
		offset_hi = ath12k_hif_cmem_read32(ab,
						   PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_HI_US));
	} else {
		offset_lo = ath12k_hif_read32(ab,
					      PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_LO_US));
		offset_hi = ath12k_hif_read32(ab,
					      PMM_REG_BASE_QCN9224 + ATH12K_PPT_ADDR_OFFSET(PMM_TQM_CLOCK_OFFSET_HI_US));
	}

	*value = ((u64)(offset_hi) << 32 | offset_lo);
}

static void
ath12k_wifi7_key_bitwise_left_shift(u8 *key, int length, int shift)
{
	int i;
	int next;

	while (shift--) {
		for (i = length - 1; i >= 0; i--) {
			if (i > 0)
				next = (key[i - 1] & 0x80 ? 1 : 0);
			else
				next = 0;

			key[i] = (key[i] << 1) | next;
		}
	}
}

static void
ath12k_wifi7_reverse_key(u8 *dest, const u8 *src, int length)
{
	int i, j;

	for (i = 0, j = length  - 1; i < length; i++, j--)
		dest[i] = src[j];
}

static void
ath12k_wifi7_hal_fst_key_configure(struct hal_rx_fst *fst)
{
	u8 key[HAL_FST_HASH_KEY_SIZE_BYTES];

	memcpy(key, fst->key, HAL_FST_HASH_KEY_SIZE_BYTES);

	ath12k_wifi7_key_bitwise_left_shift(key, HAL_FST_HASH_KEY_SIZE_BYTES, 5);
	ath12k_wifi7_reverse_key(fst->shifted_key, key, HAL_FST_HASH_KEY_SIZE_BYTES);
}

static void
ath12k_wifi7_hal_flow_toeplitz_create_cache(struct hal_rx_fst *fst)
{
	int bit;
	int value;
	int i;
	u8 *key = fst->shifted_key;
	u32 current_key = (key[0] << 24) | (key[1] << 16) | (key[2] << 8) |
		key[3];

	for (i = 0; i < HAL_FST_HASH_KEY_SIZE_BYTES; i++) {
		u8 new_key;
		u32 shifted_key[8];

		if (i + 4 < HAL_FST_HASH_KEY_SIZE_BYTES)
			new_key = key[i + 4];
		else
			new_key = 0;

		shifted_key[0] = current_key;

		for (bit = 1; bit < 8; bit++)
			shifted_key[bit] = current_key << bit | new_key >> (8 - bit);

		for (value = 0; value < (1 << 8); value++) {
			u32 hash = 0;
			int mask;

			for (bit = 0, mask = 1 << 7; bit < 8; bit++, mask >>= 1)
				if ((value & mask))
					hash ^= shifted_key[bit];

			fst->key_cache[i][value] = hash;
		}

		current_key = current_key << 8 | new_key;
	}
}

u32 ath12k_wifi7_hal_flow_toeplitz_hash(struct ath12k_base *ab,
					struct hal_rx_fst *fst,
					struct hal_flow_tuple_info *tuple_info)
{
	int i, j;
	u32 hash = 0;
	u32 input[HAL_FST_HASH_KEY_SIZE_WORDS] = {0};
	u8 *tuple;

	input[0] = htonl(tuple_info->src_ip_127_96);
	input[1] = htonl(tuple_info->src_ip_95_64);
	input[2] = htonl(tuple_info->src_ip_63_32);
	input[3] = htonl(tuple_info->src_ip_31_0);
	input[4] = htonl(tuple_info->dest_ip_127_96);
	input[5] = htonl(tuple_info->dest_ip_95_64);
	input[6] = htonl(tuple_info->dest_ip_63_32);
	input[7] = htonl(tuple_info->dest_ip_31_0);
	input[8] = (tuple_info->dest_port << 16) | (tuple_info->src_port);
	input[9] = tuple_info->l4_protocol;

	tuple = (u8 *)input;

	for (i = 0, j = HAL_FST_HASH_DATA_SIZE - 1;
	     i < HAL_FST_HASH_KEY_SIZE_BYTES && j >= 0; i++, j--)
		hash ^= fst->key_cache[i][tuple[j]];

	hash >>= 12;
	hash &= (fst->max_entries - 1);

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Hash value %u\n", hash);

	return hash;
}

u32 ath12k_wifi7_hal_rx_get_trunc_hash(struct hal_rx_fst *fst, u32 hash)
{
	if (hash >= fst->max_entries)
		hash &= (fst->max_entries - 1);

	return hash;
}

void *ath12k_wifi7_hal_rx_flow_get_tuple_info(struct ath12k_base *ab,
					      struct hal_rx_fst *fst,
					      u32 hal_hash,
					      struct hal_flow_tuple_info *tuple_info)
{
	struct hal_rx_fse *hal_fse = &fst->base_vaddr[hal_hash];

	if (!hal_fse || !tuple_info)
		return NULL;

	if (u32_get_bits(hal_fse->info2, HAL_RX_FSE_VALID) == 0)
		return NULL;

	tuple_info->src_ip_127_96 = ntohl(hal_fse->src_ip_127_96);
	tuple_info->src_ip_95_64 = ntohl(hal_fse->src_ip_95_64);
	tuple_info->src_ip_63_32 = ntohl(hal_fse->src_ip_63_32);
	tuple_info->src_ip_31_0 = ntohl(hal_fse->src_ip_31_0);
	tuple_info->dest_ip_127_96 = ntohl(hal_fse->dest_ip_127_96);
	tuple_info->dest_ip_95_64 = ntohl(hal_fse->dest_ip_95_64);
	tuple_info->dest_ip_63_32 = ntohl(hal_fse->dest_ip_63_32);
	tuple_info->dest_ip_31_0 = ntohl(hal_fse->dest_ip_31_0);
	tuple_info->dest_port =
		u32_get_bits(hal_fse->info1, HAL_RX_FSE_DEST_PORT);
	tuple_info->src_port =
		u32_get_bits(hal_fse->info1, HAL_RX_FSE_SRC_PORT);
	tuple_info->l4_protocol =
		u32_get_bits(hal_fse->info2, HAL_RX_FSE_L4_PROTOCOL);

	return hal_fse;
}

int ath12k_wifi7_hal_rx_find_flow_from_tuple(struct ath12k_base *ab,
					     struct hal_rx_fst *fst,
					     u32 flow_hash,
					     void *flow_tuple_info,
					     u32 *flow_idx)
{
	int i;
	void *hal_fse = NULL;
	u32 hal_hash = 0;
	struct hal_flow_tuple_info hal_tuple_info = { 0 };

	for (i = 0; i < fst->max_skid_length; i++) {
		hal_hash = ath12k_wifi7_hal_rx_get_trunc_hash(fst, (flow_hash + i));

		hal_fse = ath12k_wifi7_hal_rx_flow_get_tuple_info(ab, fst, hal_hash,
								  &hal_tuple_info);
		if (!hal_fse)
			continue;

		/* Find the matching flow entry in HW FST */
		if (!memcmp(&hal_tuple_info, flow_tuple_info,
			    sizeof(struct hal_flow_tuple_info)))
			break;
	}

	if (i == fst->max_skid_length) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Max skid length reached for hash %u",
			   flow_hash);
		return -ERANGE;
	}

	*flow_idx = hal_hash;
	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "flow_hash = %u, skid_entry = %d, flow_addr = %pK flow_idx = %d",
		   flow_hash, i, hal_fse, *flow_idx);

	return 0;
}

ssize_t ath12k_wifi7_hal_rx_dump_fst_table(struct ath12k_base *ab,
					   struct hal_rx_fst *fst,
					   char *buf, int size)
{
	int i;
	struct hal_rx_fse *hal_fse;
	int len = 0;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	for (i = 0; i < fst->max_entries; i++) {
		hal_fse = &fst->base_vaddr[i];

		if (!u32_get_bits(hal_fse->info2, HAL_RX_FSE_VALID))
			continue;

		len += scnprintf(buf + len, size - len, "Index: %d\n", i);
		len += scnprintf(buf + len, size - len,
				 "src_ip: 0x%x 0x%x 0x%x 0x%x\n",
				 hal_fse->src_ip_127_96,
				 hal_fse->src_ip_95_64,
				 hal_fse->src_ip_63_32,
				 hal_fse->src_ip_31_0);
		len += scnprintf(buf + len, size - len,
				 "dest_ip: 0x%x 0x%x 0x%x 0x%x\n",
				 hal_fse->dest_ip_127_96,
				 hal_fse->dest_ip_95_64,
				 hal_fse->dest_ip_63_32,
				 hal_fse->dest_ip_31_0);
		len += scnprintf(buf + len, size - len,
				 "src_port: 0x%x dest port: 0x%x\n",
				 u32_get_bits(hal_fse->info1, HAL_RX_FSE_SRC_PORT),
				 u32_get_bits(hal_fse->info1, HAL_RX_FSE_DEST_PORT));
		len += scnprintf(buf + len, size - len,
				"protocol: 0x%x\n",
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_L4_PROTOCOL));
		len += scnprintf(buf + len, size - len,
				"reo_indication: 0x%x reo_handler: 0x%x\n",
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_REO_INDICATION),
				u32_get_bits(hal_fse->info2,
					     HAL_RX_FSE_REO_DESTINATION_HANDLER));
		len += scnprintf(buf + len, size - len,
				"drop 0x%x use_ppe: 0x%x service_code 0x%x\n",
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_MSDU_DROP),
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_USE_PPE),
				u32_get_bits(hal_fse->info2, HAL_RX_FSE_SERVICE_CODE));
		len += scnprintf(buf + len, size - len, "metadata: 0x%x\n\n",
				 hal_fse->metadata);
	}

	return len;
}

static const u8 ath12k_wifi7_hal_rx_fst_toeplitz_key[HAL_RX_FST_TOEPLITZ_KEYLEN] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
};

struct hal_rx_fst *
ath12k_wifi7_hal_rx_fst_attach(struct ath12k_base *ab)
{
	struct hal_rx_fst *fst;
	u32 alloc_size;

	fst = kzalloc(sizeof(*fst), GFP_KERNEL);
	if (!fst)
		return NULL;

	fst->key = (u8 *)ath12k_wifi7_hal_rx_fst_toeplitz_key;
	fst->max_skid_length = HAL_RX_FST_MAX_SEARCH;
	fst->max_entries = HAL_RX_FLOW_SEARCH_TABLE_SIZE;
	fst->fst_entry_size = HAL_RX_FST_ENTRY_SIZE;
	alloc_size = fst->max_entries * fst->fst_entry_size;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "HAL FST allocation %pK entries %u entry size %u alloc_size %u\n",
		   fst, fst->max_entries, fst->fst_entry_size, alloc_size);

	fst->base_vaddr = ath12k_hal_dma_alloc_coherent(ab->dev, alloc_size,
					     &fst->base_paddr, GFP_KERNEL);
	if (!fst->base_vaddr) {
		kfree(fst);
		return NULL;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "hal_rx_fst base address 0x%pK",
		   (void *)fst->base_paddr);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FST Key: ",
			(void *)fst->key, HAL_FST_HASH_KEY_SIZE_BYTES);

	memset((u8 *)fst->base_vaddr, 0, alloc_size);
	ath12k_wifi7_hal_fst_key_configure(fst);
	ath12k_wifi7_hal_flow_toeplitz_create_cache(fst);

	return fst;
}

void ath12k_wifi7_hal_rx_fst_detach(struct ath12k_base *ab, struct hal_rx_fst *fst)
{
	if (!fst)
		return;

	if (fst->base_vaddr)
		ath12k_hal_dma_free_coherent(ab->dev,
					      (fst->max_entries * fst->fst_entry_size),
					      fst->base_vaddr, fst->base_paddr);
	kfree(fst);
}

int ath12k_wifi7_hal_rx_flow_insert_entry(struct ath12k_base *ab,
					  struct hal_rx_fst *fst,
					  u32 flow_hash,
					  void *flow_tuple_info,
					  u32 *flow_idx)
{
	int i;
	void *hal_fse = NULL;
	u32 hal_hash = 0;
	struct hal_flow_tuple_info hal_tuple_info = { 0 };

	for (i = 0; i < fst->max_skid_length; i++) {
		hal_hash = ath12k_wifi7_hal_rx_get_trunc_hash(fst, (flow_hash + i));

		hal_fse = ath12k_wifi7_hal_rx_flow_get_tuple_info(ab, fst, hal_hash,
								  &hal_tuple_info);
		if (!hal_fse)
			break;

		/* Find the matching flow entry in HW FST */
		if (!memcmp(&hal_tuple_info, flow_tuple_info,
			    sizeof(struct hal_flow_tuple_info))) {
			ath12k_dbg(ab, ATH12K_DBG_DP_FST,
				   "Flow already exists in FST %u at skid %u",
				   hal_hash, i);
			*flow_idx = hal_hash;
			return -EEXIST;
		}
	}

	if (i == fst->max_skid_length) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Max skid length reached for hash %u",
			   flow_hash);
		return -ERANGE;
	}

	*flow_idx = hal_hash;
	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "flow_hash = %u, skid_entry = %d, flow_addr = %pK flow_idx = %d",
		    flow_hash, i, hal_fse, *flow_idx);

	return 0;
}

void *ath12k_wifi7_hal_rx_flow_setup_fse(struct ath12k_base *ab,
					 struct hal_rx_fst *fst,
					 u32 table_offset,
					 struct hal_rx_flow *flow)
{
	struct hal_rx_fse *hal_fse;

	if (table_offset >= fst->max_entries) {
		ath12k_err(ab, "HAL FSE table offset %u exceeds max entries %u",
			   table_offset, fst->max_entries);
		return NULL;
	}

	hal_fse = &fst->base_vaddr[table_offset];

	hal_fse->src_ip_127_96 = htonl(flow->tuple_info.src_ip_127_96);
	hal_fse->src_ip_95_64 = htonl(flow->tuple_info.src_ip_95_64);
	hal_fse->src_ip_63_32 = htonl(flow->tuple_info.src_ip_63_32);
	hal_fse->src_ip_31_0 = htonl(flow->tuple_info.src_ip_31_0);
	hal_fse->dest_ip_127_96 = htonl(flow->tuple_info.dest_ip_127_96);
	hal_fse->dest_ip_95_64 = htonl(flow->tuple_info.dest_ip_95_64);
	hal_fse->dest_ip_63_32 = htonl(flow->tuple_info.dest_ip_63_32);
	hal_fse->dest_ip_31_0 = htonl(flow->tuple_info.dest_ip_31_0);
	hal_fse->metadata |= flow->fse_metadata;
	hal_fse->msdu_byte_count = 0;
	hal_fse->timestamp = 0;
	hal_fse->tcp_sequence_number = 0;

	hal_fse->info1 = u32_encode_bits(flow->tuple_info.dest_port,
					 HAL_RX_FSE_DEST_PORT);
	hal_fse->info1 |= u32_encode_bits(flow->tuple_info.src_port,
					  HAL_RX_FSE_SRC_PORT);

	hal_fse->info2 = u32_encode_bits(flow->tuple_info.l4_protocol,
					 HAL_RX_FSE_L4_PROTOCOL);
	hal_fse->info2 |= u32_encode_bits(1, HAL_RX_FSE_VALID);
	hal_fse->info2 |= u32_encode_bits(flow->service_code, HAL_RX_FSE_SERVICE_CODE);
	hal_fse->info2 |= u32_encode_bits(flow->use_ppe, HAL_RX_FSE_USE_PPE);
	hal_fse->info2 |= u32_encode_bits(flow->reo_indication,
					  HAL_RX_FSE_REO_INDICATION);
	hal_fse->info2 |= u32_encode_bits(flow->drop, HAL_RX_FSE_MSDU_DROP);
	hal_fse->info2 |= u32_encode_bits(flow->reo_destination_handler,
					  HAL_RX_FSE_REO_DESTINATION_HANDLER);
	hal_fse->info3 = 0;
	hal_fse->info4 = 0;

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "Hal FSE setup:",
			hal_fse, sizeof(*hal_fse));

	return hal_fse;
}

void ath12k_wifi7_hal_rx_flow_delete_entry(struct ath12k_base *ab,
					   struct hal_rx_fse *hal_fse)
{
	if (!u32_get_bits(hal_fse->info2, HAL_RX_FSE_VALID))
		ath12k_err(ab, "HAL FSE %pK is invalid", hal_fse);
	else
		hal_fse->info2 = u32_encode_bits(0, HAL_RX_FSE_VALID);
}

void ath12k_wifi7_hal_reset_rx_reo_tid_q(void *vaddr,
                                        u32 ba_window_size, u8 tid)
{
        struct hal_rx_reo_queue *qdesc = (struct hal_rx_reo_queue *)vaddr;
        struct hal_rx_reo_queue_ext *ext_desc;
        u32 size, info0, info1, rx_queue_num;

        size = ath12k_wifi7_hal_reo_qdesc_size(ba_window_size, tid);

        rx_queue_num = qdesc->rx_queue_num;
        info0 = qdesc->info0;
        info1 = qdesc->info1;

        memset(qdesc, 0, size);

        ath12k_wifi7_hal_reo_set_desc_hdr(&qdesc->desc_hdr, HAL_DESC_REO_OWNED,
                                          HAL_DESC_REO_QUEUE_DESC,
                                          REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_0);

        qdesc->rx_queue_num = rx_queue_num;
        qdesc->info0 = info0;
        qdesc->info1 = info1;

        qdesc->info1 |= u32_encode_bits(0, HAL_RX_REO_QUEUE_INFO1_SVLD);
        qdesc->info1 |= u32_encode_bits(0,
                                        HAL_RX_REO_QUEUE_INFO1_SSN);

        if (tid == HAL_DESC_REO_NON_QOS_TID)
                return;

        ext_desc = qdesc->ext_desc;
        memset(ext_desc, 0, 3 * sizeof(*ext_desc));

        ath12k_wifi7_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
                                          HAL_DESC_REO_QUEUE_EXT_DESC,
                                          REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_1);
        ext_desc++;

        ath12k_wifi7_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
                                          HAL_DESC_REO_QUEUE_EXT_DESC,
                                          REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_2);
        ext_desc++;

        ath12k_wifi7_hal_reo_set_desc_hdr(&ext_desc->desc_hdr, HAL_DESC_REO_OWNED,
                                          HAL_DESC_REO_QUEUE_EXT_DESC,
                                          REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_3);
}
