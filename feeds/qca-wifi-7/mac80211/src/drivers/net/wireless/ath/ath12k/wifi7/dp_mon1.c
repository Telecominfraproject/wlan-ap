// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the definitions related to the monitor quad ring
 * model
 */

#include "../dp_mon.h"
#include "dp_mon1.h"
#include "../debug.h"
#include "hal_mon.h"
#include "dp_rx.h"
#include "../dp_mon_filter.h"

struct ath12k_dp_arch_mon_ops ath12k_wifi7_dp_arch_mon_quad_ring_ops = {
	.rx_srng_setup = ath12k_wifi7_dp_mon_rx_srng_setup,
	.rx_srng_cleanup = ath12k_wifi7_dp_mon_rx_srng_cleanup,
	.rx_buf_setup = ath12k_wifi7_dp_mon_rx_buf_setup,
	.rx_buf_free = ath12k_wifi7_dp_mon_rx_buf_free,
	.rx_htt_srng_setup = ath12k_wifi7_dp_mon_rx_htt_srng_setup,
	.mon_pdev_alloc = ath12k_dp_mon_pdev_alloc,
	.mon_pdev_free = ath12k_dp_mon_pdev_free,
	.mon_pdev_rx_srng_setup = NULL,
	.mon_pdev_rx_srng_cleanup = NULL,
	.mon_pdev_rx_htt_srng_setup = NULL,
	.mon_pdev_rx_attach = ath12k_dp_mon_pdev_rx_attach,
	.mon_pdev_rx_mpdu_list_init = NULL,
	.mon_rx_srng_process = ath12k_wifi7_dp_mon_rx_quad_ring_process,
	.update_telemetry_stats = NULL,
	.rx_filter_alloc = ath12k_dp_mon_rx_filter_alloc,
	.rx_filter_free = ath12k_dp_mon_rx_filter_free,
	.rx_stats_enable = NULL,
	.rx_stats_disable = NULL,
	.rx_filter_update = NULL,
	.rx_filter_update = ath12k_wifi7_dp_mon_rx_update_ring_filter,
	.rx_monitor_mode_set = ath12k_wifi7_dp_mon_rx_monitor_mode_set,
	.rx_monitor_mode_reset = ath12k_wifi7_dp_mon_rx_monitor_mode_reset,
	.setup_ppdu_desc = NULL,
	.cleanup_ppdu_desc = NULL,
	.rx_nrp_set = NULL,
	.rx_nrp_reset = NULL,
	.mon_rx_wmask = NULL,
	.rx_enable_packet_filters = NULL,
	.pktlog_config = NULL,
};

int ath12k_wifi7_dp_mon_rx_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;
	int i, ret;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		idr_init(&dp_mon->rx_mon_status_refill_ring[i].bufs_idr);
		spin_lock_init(&dp_mon->rx_mon_status_refill_ring[i].idr_lock);
	}

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		srng = &dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring;
		ret = ath12k_dp_srng_setup(ab, srng,
					   HAL_RXDMA_MONITOR_STATUS, 0, i,
					   DP_RXDMA_MON_STATUS_RING_SIZE);
		if (ret) {
			ath12k_warn(dp, "failed to setup mon status ring %d\n", i);
			return ret;
		}
	}

	return 0;
}

void ath12k_wifi7_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		srng = &dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring;
		ath12k_dp_srng_cleanup(ab, srng);
	}
}

static int
ath12k_wifi7_dp_mon_rx_status_bufs_replenish(struct ath12k_dp *dp,
					     struct dp_rxdma_mon_ring *rx_ring,
					     int req_entries)
{
	struct ath12k_base *ab = dp->ab;
	enum hal_rx_buf_return_buf_manager mgr =
		ab->hal.hal_params->rx_buf_rbm;
	int num_free, num_remain, buf_id;
	void *desc;
	struct hal_srng *srng;
	struct sk_buff *skb;
	dma_addr_t paddr;
	u32 cookie;

	req_entries = min(req_entries, rx_ring->bufs_max);

	srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	if (!req_entries && (num_free > (rx_ring->bufs_max * 3) / 4))
		req_entries = num_free;

	req_entries = min(num_free, req_entries);
	num_remain = req_entries;

	while (num_remain > 0) {
		skb = dev_alloc_skb(RX_MON_STATUS_BUF_SIZE);
		if (!skb)
			break;

		if (!IS_ALIGNED((unsigned long)skb->data,
				RX_MON_STATUS_BUF_ALIGN)) {
			skb_pull(skb,
				 PTR_ALIGN(skb->data, RX_MON_STATUS_BUF_ALIGN) -
				 skb->data);
		}

		paddr = dma_map_single(ab->dev, skb->data,
				       skb->len + skb_tailroom(skb),
				       DMA_FROM_DEVICE);
		if (dma_mapping_error(ab->dev, paddr))
			goto fail_free_skb;

		spin_lock_bh(&rx_ring->idr_lock);
		buf_id = idr_alloc(&rx_ring->bufs_idr, skb, 0,
				   rx_ring->bufs_max * 3, GFP_ATOMIC);
		spin_unlock_bh(&rx_ring->idr_lock);
		if (buf_id < 0)
			goto fail_dma_unmap;
		cookie = u32_encode_bits(buf_id, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

		desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (!desc)
			goto fail_buf_unassign;

		ATH12K_SKB_RXCB(skb)->paddr = paddr;

		num_remain--;

		ath12k_hal_rx_buf_addr_info_set(desc, paddr, cookie, mgr);
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return req_entries - num_remain;

fail_buf_unassign:
	spin_lock_bh(&rx_ring->idr_lock);
	idr_remove(&rx_ring->bufs_idr, buf_id);
	spin_unlock_bh(&rx_ring->idr_lock);
fail_dma_unmap:
	dma_unmap_single(ab->dev, paddr, skb->len + skb_tailroom(skb),
			 DMA_FROM_DEVICE);
fail_free_skb:
	dev_kfree_skb_any(skb);

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return req_entries - num_remain;
}

int ath12k_wifi7_dp_mon_rx_buf_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	int i;
	int num_entries;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		rx_ring = &dp_mon->rx_mon_status_refill_ring[i];
		num_entries = rx_ring->refill_buf_ring.size /
			ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_STATUS);
		rx_ring->bufs_max = num_entries;

		ath12k_wifi7_dp_mon_rx_status_bufs_replenish(dp, rx_ring, num_entries);
	}

	return 0;
}

void ath12k_wifi7_dp_mon_rx_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		rx_ring = &dp_mon->rx_mon_status_refill_ring[i];
		ath12k_dp_rxdma_mon_buf_ring_free(dp, rx_ring);
	}
}

int ath12k_wifi7_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u32 ring_id;
	int i, ret;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ring_id =
			dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;

		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, i,
						  HAL_RXDMA_MONITOR_STATUS);
		if (ret) {
			ath12k_warn(ab,
				    "failed to configure mon_status_refill_ring%d %d\n",
				    i, ret);
			return ret;
		}
	}

	return 0;
}

static struct sk_buff
*ath12k_wifi7_dp_mon_rx_alloc_status_buf(struct ath12k_dp *dp,
					 struct dp_rxdma_mon_ring *rx_ring,
					 int *buf_id)
{
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb;
	dma_addr_t paddr;

	skb = dev_alloc_skb(RX_MON_STATUS_BUF_SIZE);

	if (!skb)
		goto fail_alloc_skb;

	if (!IS_ALIGNED((unsigned long)skb->data,
			RX_MON_STATUS_BUF_ALIGN)) {
		skb_pull(skb, PTR_ALIGN(skb->data, RX_MON_STATUS_BUF_ALIGN) -
			 skb->data);
	}

	paddr = dma_map_single(ab->dev, skb->data,
			       skb->len + skb_tailroom(skb),
			       DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(ab->dev, paddr)))
		goto fail_free_skb;

	spin_lock_bh(&rx_ring->idr_lock);
	*buf_id = idr_alloc(&rx_ring->bufs_idr, skb, 0,
			    rx_ring->bufs_max, GFP_ATOMIC);
	spin_unlock_bh(&rx_ring->idr_lock);
	if (*buf_id < 0)
		goto fail_dma_unmap;

	ATH12K_SKB_RXCB(skb)->paddr = paddr;
	return skb;

fail_dma_unmap:
	dma_unmap_single(ab->dev, paddr, skb->len + skb_tailroom(skb),
			 DMA_FROM_DEVICE);
fail_free_skb:
	dev_kfree_skb_any(skb);
fail_alloc_skb:
	return NULL;
}

static enum dp_mon_status_buf_state
ath12k_wifi7_dp_mon_rx_buf_done(struct ath12k_base *ab, struct hal_srng *srng,
				struct dp_rxdma_mon_ring *rx_ring)
{
	struct ath12k_skb_rxcb *rxcb;
	struct hal_tlv_64_hdr *tlv;
	struct sk_buff *skb;
	struct ath12k_buffer_addr *status_desc;
	dma_addr_t paddr;
	u32 cookie;
	int buf_id;
	u8 rbm;

	status_desc = ath12k_hal_srng_src_next_peek(ab, srng);
	if (!status_desc)
		return DP_MON_STATUS_NO_DMA;

	ath12k_hal_rx_buf_addr_info_get(status_desc, &paddr, &cookie, &rbm);

	buf_id = u32_get_bits(cookie, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

	spin_lock_bh(&rx_ring->idr_lock);
	skb = idr_find(&rx_ring->bufs_idr, buf_id);
	spin_unlock_bh(&rx_ring->idr_lock);

	if (!skb)
		return DP_MON_STATUS_NO_DMA;

	rxcb = ATH12K_SKB_RXCB(skb);
	dma_sync_single_for_cpu(ab->dev, rxcb->paddr,
				skb->len + skb_tailroom(skb),
				DMA_FROM_DEVICE);

	tlv = (struct hal_tlv_64_hdr *)skb->data;
	if (le64_get_bits(tlv->tl, HAL_TLV_HDR_TAG) != HAL_RX_STATUS_BUFFER_DONE)
		return DP_MON_STATUS_NO_DMA;

	return DP_MON_STATUS_REPLINISH;
}

static int ath12k_wifi7_dp_mon_rx_reap_status_ring(struct ath12k_pdev_dp *pdev_dp,
						   int mac_id,
						   int *budget,
						   struct sk_buff_head *skb_list)
{
	const struct ath12k_hw_hal_params *hal_params;
	int buf_id, srng_id, num_buffs_reaped = 0;
	enum dp_mon_status_buf_state reap_status;
	struct dp_rxdma_mon_ring *rx_ring;
	struct ath12k_mon_data *pmon;
	struct ath12k_skb_rxcb *rxcb;
	struct hal_tlv_64_hdr *tlv;
	struct ath12k_buffer_addr *rx_mon_status_desc;
	struct hal_srng *srng;
	struct ath12k_dp *dp;
	struct ath12k_base *ab;
	struct ath12k_dp_mon *dp_mon;
	struct sk_buff *skb;
	dma_addr_t paddr;
	u32 cookie;
	u8 rbm;

	dp = pdev_dp->dp;
	ab = dp->ab;
	dp_mon = dp->dp_mon;
	pmon = &pdev_dp->dp_mon_pdev->mon_data;
	srng_id = ath12k_hw_mac_id_to_srng_id(ab->hw_params, mac_id);
	rx_ring = &dp_mon->rx_mon_status_refill_ring[srng_id];

	srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while (*budget) {
		*budget -= 1;
		rx_mon_status_desc = ath12k_hal_srng_src_peek(ab, srng);
		if (!rx_mon_status_desc) {
			pmon->buf_state = DP_MON_STATUS_REPLINISH;
			break;
		}
		ath12k_hal_rx_buf_addr_info_get(rx_mon_status_desc, &paddr,
						&cookie, &rbm);
		if (paddr) {
			buf_id = u32_get_bits(cookie, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

			spin_lock_bh(&rx_ring->idr_lock);
			skb = idr_find(&rx_ring->bufs_idr, buf_id);
			spin_unlock_bh(&rx_ring->idr_lock);

			if (!skb) {
				ath12k_warn(ab, "rx monitor status with invalid buf_id %d\n",
					    buf_id);
				pmon->buf_state = DP_MON_STATUS_REPLINISH;
				goto move_next;
			}

			rxcb = ATH12K_SKB_RXCB(skb);

			dma_sync_single_for_cpu(ab->dev, rxcb->paddr,
						skb->len + skb_tailroom(skb),
						DMA_FROM_DEVICE);

			tlv = (struct hal_tlv_64_hdr *)skb->data;
			if (le64_get_bits(tlv->tl, HAL_TLV_HDR_TAG) !=
					HAL_RX_STATUS_BUFFER_DONE) {
				pmon->buf_state = DP_MON_STATUS_NO_DMA;
				ath12k_warn(ab,
					    "mon status DONE not set %llx, buf_id %d\n",
					    le64_get_bits(tlv->tl, HAL_TLV_HDR_TAG),
					    buf_id);
				/* RxDMA status done bit might not be set even
				 * though tp is moved by HW.
				 */

				/* If done status is missing:
				 * 1. As per MAC team's suggestion,
				 *    when HP + 1 entry is peeked and if DMA
				 *    is not done and if HP + 2 entry's DMA done
				 *    is set. skip HP + 1 entry and
				 *    start processing in next interrupt.
				 * 2. If HP + 2 entry's DMA done is not set,
				 *    poll onto HP + 1 entry DMA done to be set.
				 *    Check status for same buffer for next time
				 *    dp_rx_mon_status_srng_process
				 */
				reap_status = ath12k_wifi7_dp_mon_rx_buf_done(ab, srng,
									       rx_ring);
				if (reap_status == DP_MON_STATUS_NO_DMA)
					continue;

				spin_lock_bh(&rx_ring->idr_lock);
				idr_remove(&rx_ring->bufs_idr, buf_id);
				spin_unlock_bh(&rx_ring->idr_lock);

				dma_unmap_single(ab->dev, rxcb->paddr,
						 skb->len + skb_tailroom(skb),
						 DMA_FROM_DEVICE);

				dev_kfree_skb_any(skb);
				pmon->buf_state = DP_MON_STATUS_REPLINISH;
				goto move_next;
			}

			spin_lock_bh(&rx_ring->idr_lock);
			idr_remove(&rx_ring->bufs_idr, buf_id);
			spin_unlock_bh(&rx_ring->idr_lock);

			dma_unmap_single(ab->dev, rxcb->paddr,
					 skb->len + skb_tailroom(skb),
					 DMA_FROM_DEVICE);

			if (ath12k_dp_mon_rx_set_pktlen(skb, RX_MON_STATUS_BUF_SIZE)) {
				dev_kfree_skb_any(skb);
				goto move_next;
			}
			__skb_queue_tail(skb_list, skb);
		} else {
			pmon->buf_state = DP_MON_STATUS_REPLINISH;
		}
move_next:
		skb = ath12k_wifi7_dp_mon_rx_alloc_status_buf(dp, rx_ring,
							       &buf_id);

		if (!skb) {
			ath12k_warn(ab, "failed to alloc buffer for status ring\n");
			hal_params = ab->hal.hal_params;
			ath12k_hal_rx_buf_addr_info_set(rx_mon_status_desc, 0,
							0,
							hal_params->rx_buf_rbm);
			num_buffs_reaped++;
			break;
		}
		rxcb = ATH12K_SKB_RXCB(skb);

		cookie = u32_encode_bits(mac_id, DP_MON_RXDMA_BUF_COOKIE_PDEV_ID) |
			 u32_encode_bits(buf_id, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

		ath12k_hal_rx_buf_addr_info_set(rx_mon_status_desc, rxcb->paddr,
						cookie,
						ab->hal.hal_params->rx_buf_rbm);
		ath12k_hal_srng_src_get_next_entry(ab, srng);
		num_buffs_reaped++;
	}
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return num_buffs_reaped;
}

static enum hal_rx_mon_status
ath12k_wifi7_dp_mon_rx_parse_dest(struct ath12k_pdev_dp *dp_pdev,
				  struct sk_buff *skb)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct hal_tlv_64_hdr *tlv;
	struct ath12k_skb_rxcb *rxcb;
	struct hal_tlv_parsed_hdr tlv_parsed_hdr = {0};
	enum hal_rx_mon_status hal_status;
	u16 tlv_tag, tlv_len, tlv_userid;
	u8 *ptr = skb->data;

	do {
		tlv = (struct hal_tlv_64_hdr *)ptr;
		tlv_tag = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_TAG);
		tlv_len = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_LEN);
		tlv_userid = le64_get_bits(tlv->tl, HAL_TLV_USR_ID);
		ptr += sizeof(*tlv);

		/* The actual length of PPDU_END is the combined length of many PHY
		 * TLVs that follow. Skip the TLV header and
		 * rx_rxpcu_classification_overview that follows the header to get to
		 * next TLV.
		 */

		if (tlv_tag == HAL_RX_PPDU_END)
			tlv_len = sizeof(struct hal_rx_rxpcu_classification_overview);
		else
			tlv_len = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_LEN);

		tlv_parsed_hdr.tag = tlv_tag;
		tlv_parsed_hdr.len = tlv_len;
		tlv_parsed_hdr.userid = tlv_userid;
		tlv_parsed_hdr.data = ptr;

		hal_status =
			ath12k_wifi7_hal_mon_rx_parse_status_tlv(dp_pdev->dp->hal,
								 &pmon->mon_ppdu_info,
								 &tlv_parsed_hdr);
		ptr += sizeof(*tlv) + tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_64_ALIGN);

		if ((ptr - skb->data) > skb->len)
			break;

	} while ((hal_status == HAL_RX_MON_STATUS_PPDU_NOT_DONE) ||
		 (hal_status == HAL_RX_MON_STATUS_BUF_ADDR) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_START) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_END) ||
		 (hal_status == HAL_RX_MON_STATUS_MSDU_END));

	rxcb = ATH12K_SKB_RXCB(skb);
	if (rxcb->is_end_of_ppdu)
		hal_status = HAL_RX_MON_STATUS_PPDU_DONE;

	return hal_status;
}

static u32 ath12k_wifi7_dp_mon_rx_comp_ppduid(u32 msdu_ppdu_id, u32 *ppdu_id)
{
	u32 ret = 0;

	if ((*ppdu_id < msdu_ppdu_id) &&
	    ((msdu_ppdu_id - *ppdu_id) < DP_NOT_PPDU_ID_WRAP_AROUND)) {
		/* Hold on mon dest ring, and reap mon status ring. */
		*ppdu_id = msdu_ppdu_id;
		ret = msdu_ppdu_id;
	} else if ((*ppdu_id > msdu_ppdu_id) &&
		((*ppdu_id - msdu_ppdu_id) > DP_NOT_PPDU_ID_WRAP_AROUND)) {
		/* PPDU ID has exceeded the maximum value and will
		 * restart from 0.
		 */
		*ppdu_id = msdu_ppdu_id;
		ret = msdu_ppdu_id;
	}
	return ret;
}

static void
ath12k_wifi7_dp_mon_rx_next_link_desc_get(struct hal_rx_msdu_link *msdu_link,
					  dma_addr_t *paddr, u32 *sw_cookie, u8 *rbm,
					  struct ath12k_buffer_addr **pp_buf_addr_info)
{
	void *buf_addr_info;

	buf_addr_info = &msdu_link->buf_addr_info;

	ath12k_hal_rx_buf_addr_info_get(buf_addr_info,
					paddr, sw_cookie, rbm);

	*pp_buf_addr_info = buf_addr_info;
}

/* Hardware fill buffer with 128 bytes aligned. So need to reap it
 * with 128 bytes aligned.
 */
#define RXDMA_DATA_DMA_BLOCK_SIZE 128

static void
ath12k_wifi7_dp_mon_rx_get_buf_len(struct hal_rx_msdu_desc_info *info,
				   bool *is_frag, u32 *total_len,
				   u32 *frag_len, u32 *msdu_cnt)
{
	if (info->msdu_flags & RX_MSDU_DESC_INFO0_MSDU_CONTINUATION) {
		*is_frag = true;
		*frag_len = (RX_MON_STATUS_BASE_BUF_SIZE -
			     sizeof(struct hal_rx_desc)) &
			     ~(RXDMA_DATA_DMA_BLOCK_SIZE - 1);
		*total_len += *frag_len;
	} else {
		if (*is_frag)
			*frag_len = info->msdu_len - *total_len;
		else
			*frag_len = info->msdu_len;

		*msdu_cnt -= 1;
	}
}

static bool
ath12k_wifi7_dp_mon_rxdesc_mpdu_valid(struct ath12k_dp *dp,
				      struct hal_rx_desc *rx_desc)
{
	struct ath12k_base *ab = dp->ab;
	u32 tlv_tag;

	tlv_tag = hal_rx_desc_get_mpdu_start_tag(&ab->hal, rx_desc);

	return tlv_tag == HAL_RX_MPDU_START;
}

static u32
ath12k_wifi7_dp_mon_rx_mpdu_pop(struct ath12k_pdev_dp *dp_pdev, int mac_id,
				void *ring_entry, struct sk_buff **head_msdu,
				struct sk_buff **tail_msdu,
				struct list_head *used_list,
				u32 *npackets, u32 *ppdu_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct ath12k_buffer_addr *p_buf_addr_info, *p_last_buf_addr_info;
	u32 msdu_ppdu_id = 0, msdu_cnt = 0, total_len = 0, frag_len = 0;
	u32 rx_buf_size, rx_pkt_offset, sw_cookie;
	bool is_frag, is_first_msdu, drop_mpdu = false;
	struct hal_reo_entrance_ring *ent_desc =
		(struct hal_reo_entrance_ring *)ring_entry;
	u32 rx_bufs_used = 0, i = 0, desc_bank = 0;
	struct hal_rx_desc *rx_desc, *tail_rx_desc;
	struct hal_rx_msdu_link *msdu_link_desc;
	struct sk_buff *msdu = NULL, *last = NULL;
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_buffer_addr buf_info;
	struct hal_rx_msdu_list msdu_list;
	struct ath12k_skb_rxcb *rxcb;
	u16 num_msdus = 0;
	dma_addr_t paddr;
	u8 rbm;

	ath12k_hal_rx_reo_ent_buf_paddr_get(&ab->hal, ring_entry,
					    &paddr,
					    &sw_cookie,
					    &p_last_buf_addr_info, &rbm,
					    &msdu_cnt);

	spin_lock_bh(&pmon->mon_lock);

	if (le32_get_bits(ent_desc->info1,
			  HAL_REO_ENTR_RING_INFO1_RXDMA_PUSH_REASON) ==
			  HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED) {
		u8 rxdma_err = le32_get_bits(ent_desc->info1,
					     HAL_REO_ENTR_RING_INFO1_RXDMA_ERROR_CODE);
		if (rxdma_err == HAL_REO_ENTR_RING_RXDMA_ECODE_FLUSH_REQUEST_ERR ||
		    rxdma_err == HAL_REO_ENTR_RING_RXDMA_ECODE_MPDU_LEN_ERR ||
		    rxdma_err == HAL_REO_ENTR_RING_RXDMA_ECODE_OVERFLOW_ERR) {
			drop_mpdu = true;
			pmon->rx_mon_stats.dest_mpdu_drop++;
		}
	}

	is_frag = false;
	is_first_msdu = true;
	rx_pkt_offset = sizeof(struct hal_rx_desc);

	do {
		if (pmon->mon_last_linkdesc_paddr == paddr) {
			pmon->rx_mon_stats.dup_mon_linkdesc_cnt++;
			spin_unlock_bh(&pmon->mon_lock);
			return rx_bufs_used;
		}

		desc_bank = u32_get_bits(sw_cookie, DP_LINK_DESC_BANK_MASK);
		msdu_link_desc =
			dp->link_desc_banks[desc_bank].vaddr +
			(paddr - dp->link_desc_banks[desc_bank].paddr);

		ath12k_hal_rx_msdu_list_get(&ab->hal, msdu_link_desc, &msdu_list,
					    &num_msdus);
		desc_info = ath12k_dp_get_rx_desc(dp,
						  msdu_list.sw_cookie[num_msdus - 1]);
		tail_rx_desc = (struct hal_rx_desc *)(desc_info->skb)->data;

		for (i = 0; i < num_msdus; i++) {
			u32 l2_hdr_offset;

			if (pmon->mon_last_buf_cookie == msdu_list.sw_cookie[i]) {
				ath12k_dbg(ab, ATH12K_DBG_DATA,
					   "i %d last_cookie %d is same\n",
					   i, pmon->mon_last_buf_cookie);
				drop_mpdu = true;
				pmon->rx_mon_stats.dup_mon_buf_cnt++;
				continue;
			}

			desc_info =
				ath12k_dp_get_rx_desc(dp, msdu_list.sw_cookie[i]);
			msdu = desc_info->skb;

			if (!msdu) {
				ath12k_dbg(ab, ATH12K_DBG_DATA,
					   "msdu_pop: invalid msdu (%d/%d)\n",
					   i + 1, num_msdus);
				goto next_msdu;
			}
			rxcb = ATH12K_SKB_RXCB(msdu);
			if (rxcb->paddr != msdu_list.paddr[i]) {
				ath12k_dbg(ab, ATH12K_DBG_DATA,
					   "i %d paddr %lx != %lx\n",
					   i, (unsigned long)rxcb->paddr,
					   (unsigned long)msdu_list.paddr[i]);
				drop_mpdu = true;
				continue;
			}
			if (!rxcb->unmapped) {
				dma_unmap_single(ab->dev, rxcb->paddr,
						 msdu->len +
						 skb_tailroom(msdu),
						 DMA_FROM_DEVICE);
				rxcb->unmapped = 1;
			}
			if (drop_mpdu) {
				ath12k_dbg(ab, ATH12K_DBG_DATA,
					   "i %d drop msdu %p *ppdu_id %x\n",
					   i, msdu, *ppdu_id);
				dev_kfree_skb_any(msdu);
				msdu = NULL;
				goto next_msdu;
			}

			rx_desc = (struct hal_rx_desc *)msdu->data;
			l2_hdr_offset = ath12k_hal_rx_h_l3pad_get(&ab->hal,
								  tail_rx_desc);
			if (is_first_msdu) {
				if (!ath12k_wifi7_dp_mon_rxdesc_mpdu_valid(dp,
									   rx_desc)) {
					drop_mpdu = true;
					dev_kfree_skb_any(msdu);
					msdu = NULL;
					pmon->mon_last_linkdesc_paddr = paddr;
					goto next_msdu;
				}
				msdu_ppdu_id =
					ath12k_hal_rx_desc_get_mpdu_ppdu_id(&ab->hal,
									    rx_desc);

				if (ath12k_wifi7_dp_mon_rx_comp_ppduid(msdu_ppdu_id,
								       ppdu_id)) {
					spin_unlock_bh(&pmon->mon_lock);
					return rx_bufs_used;
				}
				pmon->mon_last_linkdesc_paddr = paddr;
				is_first_msdu = false;
			}
			ath12k_wifi7_dp_mon_rx_get_buf_len(&msdu_list.msdu_info[i],
							   &is_frag, &total_len,
							   &frag_len, &msdu_cnt);
			rx_buf_size = rx_pkt_offset + l2_hdr_offset + frag_len;

			if (ath12k_dp_mon_rx_set_pktlen(msdu, rx_buf_size)) {
				dev_kfree_skb_any(msdu);
				goto next_msdu;
			}

			if (!(*head_msdu))
				*head_msdu = msdu;
			else if (last)
				last->next = msdu;

			last = msdu;
next_msdu:
			pmon->mon_last_buf_cookie = msdu_list.sw_cookie[i];
			rx_bufs_used++;
			desc_info->skb = NULL;
			list_add_tail(&desc_info->list, used_list);
		}

		ath12k_hal_rx_buf_addr_info_set(&buf_info,
						paddr, sw_cookie, rbm);

		ath12k_wifi7_dp_mon_rx_next_link_desc_get(msdu_link_desc,
							  &paddr,
							  &sw_cookie, &rbm,
							  &p_buf_addr_info);

		ath12k_dp_arch_rx_link_desc_return(dp,
						   &buf_info,
						   HAL_WBM_REL_BM_ACT_PUT_IN_IDLE);

		p_last_buf_addr_info = p_buf_addr_info;

	} while (paddr && msdu_cnt);

	spin_unlock_bh(&pmon->mon_lock);

	if (last)
		last->next = NULL;

	*tail_msdu = msdu;

	if (msdu_cnt == 0)
		*npackets = 1;

	return rx_bufs_used;
}

static void ath12k_dp_mon_rx_msdus_set_payload(struct ath12k_dp *dp,
					       struct sk_buff *head_msdu,
					       struct sk_buff *tail_msdu)
{
	struct ath12k_base *ab = dp->ab;
	u32 rx_pkt_offset, l2_hdr_offset, total_offset;

	if (ath12k_dp_get_mon_type(dp) == ATH12K_DP_MON_TYPE_DUAL_RING) {
		total_offset = ATH12K_MON_RX_PKT_OFFSET;
	} else {
		rx_pkt_offset = ab->hal.hal_desc_sz;
		l2_hdr_offset =
			ath12k_hal_rx_h_l3pad_get(&ab->hal,
						  (void *)tail_msdu->data);

		total_offset = rx_pkt_offset + l2_hdr_offset;
	}

	skb_pull(head_msdu, total_offset);
}

static struct sk_buff *
ath12k_dp_mon_rx_merg_msdus(struct ath12k_pdev_dp *dp_pdev,
			    struct dp_mon_mpdu *mon_mpdu,
			    struct hal_rx_mon_ppdu_info *ppdu_info,
			    struct ieee80211_rx_status *rxs)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k *ar = dp_pdev->ar;
	struct sk_buff *msdu, *mpdu_buf, *prev_buf, *head_frag_list;
	struct sk_buff *head_msdu, *tail_msdu;
	void *rx_desc;
	u8 *hdr_desc, *dest, decap_format = mon_mpdu->decap_format;
	struct ieee80211_hdr_3addr *wh;
	struct ieee80211_channel *channel;
	u32 frag_list_sum_len = 0;
	u8 channel_num = ppdu_info->chan_num;

	mpdu_buf = NULL;
	head_msdu = mon_mpdu->head;
	tail_msdu = mon_mpdu->tail;

	if (!head_msdu || !tail_msdu)
		goto err_merge_fail;

	ath12k_dp_mon_fill_rx_stats_info(ppdu_info, rxs);

	if (unlikely(rxs->band == NUM_NL80211_BANDS ||
		     !ath12k_dp_pdev_to_hw(dp_pdev)->wiphy->bands[rxs->band])) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "sband is NULL for status band %d channel_num %d center_freq %d pdev_id %d\n",
			   rxs->band, channel_num, ppdu_info->freq, ar->pdev_idx);

		spin_lock_bh(&ar->data_lock);
		channel = ar->rx_channel;
		if (channel) {
			rxs->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		}
		spin_unlock_bh(&ar->data_lock);
	}

	if (rxs->band < NUM_NL80211_BANDS)
		rxs->freq = ieee80211_channel_to_frequency(channel_num,
							   rxs->band);

	ath12k_dp_mon_fill_rx_rate(dp_pdev, ppdu_info, rxs);

	if (decap_format == DP_RX_DECAP_TYPE_RAW) {
		ath12k_dp_mon_rx_msdus_set_payload(dp, head_msdu, tail_msdu);

		prev_buf = head_msdu;
		msdu = head_msdu->next;
		head_frag_list = NULL;

		while (msdu) {
			ath12k_dp_mon_rx_msdus_set_payload(dp, msdu, tail_msdu);

			if (!head_frag_list)
				head_frag_list = msdu;

			frag_list_sum_len += msdu->len;
			prev_buf = msdu;
			msdu = msdu->next;
		}

		prev_buf->next = NULL;

		skb_trim(prev_buf, prev_buf->len);
		if (head_frag_list) {
			skb_shinfo(head_msdu)->frag_list = head_frag_list;
			head_msdu->data_len = frag_list_sum_len;
			head_msdu->len += head_msdu->data_len;
			head_msdu->next = NULL;
		}
	} else if (decap_format == DP_RX_DECAP_TYPE_NATIVE_WIFI) {
		u8 qos_pkt = 0;

		rx_desc = head_msdu->data;
		hdr_desc =
			ath12k_hal_mon_rx_desc_get_msdu_payload(dp->hal, rx_desc);

		if (!hdr_desc)
			goto err_merge_fail;

		/* Base size */
		wh = (struct ieee80211_hdr_3addr *)hdr_desc;

		if (ieee80211_is_data_qos(wh->frame_control))
			qos_pkt = 1;

		msdu = head_msdu;

		while (msdu) {
			ath12k_dp_mon_rx_msdus_set_payload(dp, msdu, tail_msdu);
			if (qos_pkt) {
				dest = skb_push(msdu, sizeof(__le16));
				if (!dest)
					goto err_merge_fail;
				memcpy(dest, hdr_desc, sizeof(struct ieee80211_qos_hdr));
			}
			prev_buf = msdu;
			msdu = msdu->next;
		}
		dest = skb_put(prev_buf, HAL_RX_MON_FCS_LEN);
		if (!dest)
			goto err_merge_fail;

		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "mpdu_buf %p mpdu_buf->len %u",
			   prev_buf, prev_buf->len);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "decap format %d is not supported!\n",
			   decap_format);
		goto err_merge_fail;
	}

	return head_msdu;

err_merge_fail:
	if (mpdu_buf && decap_format != DP_RX_DECAP_TYPE_RAW) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "err_merge_fail mpdu_buf %p", mpdu_buf);
		/* Free the head buffer */
		dev_kfree_skb_any(mpdu_buf);
	}
	return NULL;
}

static
int ath12k_dp_mon_rx_deliver(struct ath12k_pdev_dp *dp_pdev,
			     struct dp_mon_mpdu *mon_mpdu,
			     struct hal_rx_mon_ppdu_info *ppduinfo,
			     struct napi_struct *napi)
{
	struct sk_buff *mon_skb, *skb_next, *header;
	struct ieee80211_rx_status *rxs = &dp_pdev->dp_mon_pdev->rx_status;
	u8 decap = DP_RX_DECAP_TYPE_RAW;

	mon_skb = ath12k_dp_mon_rx_merg_msdus(dp_pdev, mon_mpdu, ppduinfo, rxs);
	if (!mon_skb)
		goto mon_deliver_fail;

	header = mon_skb;
	rxs->flag = 0;

	if (mon_mpdu->err_bitmap & HAL_RX_MON_MPDU_ERR_FCS)
		rxs->flag = RX_FLAG_FAILED_FCS_CRC;

	do {
		skb_next = mon_skb->next;
		if (!skb_next)
			rxs->flag &= ~RX_FLAG_AMSDU_MORE;
		else
			rxs->flag |= RX_FLAG_AMSDU_MORE;

		if (mon_skb == header) {
			header = NULL;
			rxs->flag &= ~RX_FLAG_ALLOW_SAME_PN;
		} else {
			rxs->flag |= RX_FLAG_ALLOW_SAME_PN;
		}
		rxs->flag |= RX_FLAG_ONLY_MONITOR;

		if (!(rxs->flag & RX_FLAG_ONLY_MONITOR))
			decap = mon_mpdu->decap_format;

		ath12k_dp_mon_update_radiotap(dp_pdev, ppduinfo, mon_skb, rxs);
		ath12k_dp_mon_rx_deliver_skb(dp_pdev, napi, mon_skb, rxs, ppduinfo);
		mon_skb = skb_next;
	} while (mon_skb);
	rxs->flag = 0;

	return 0;

mon_deliver_fail:
	mon_skb = mon_mpdu->head;
	while (mon_skb) {
		skb_next = mon_skb->next;
		dev_kfree_skb_any(mon_skb);
		mon_skb = skb_next;
	}
	return -EINVAL;
}

/* The destination ring processing is stuck if the destination is not
 * moving while status ring moves 16 PPDU. The destination ring processing
 * skips this destination ring PPDU as a workaround.
 */
#define MON_DEST_RING_STUCK_MAX_CNT 16

static
void ath12k_wifi7_dp_mon_rx_dest_process(struct ath12k_pdev_dp *dp_pdev, int mac_id,
					 u32 quota, struct napi_struct *napi)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct ath12k_pdev_mon_stats *rx_mon_stats;
	u32 ppdu_id, rx_bufs_used = 0, ring_id;
	u32 mpdu_rx_bufs_used, npackets = 0;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_base *ab = dp->ab;
	void *ring_entry, *mon_dst_srng;
	struct dp_mon_mpdu *tmp_mpdu;
	LIST_HEAD(rx_desc_used_list);
	struct hal_srng *srng;

	ring_id = dp->rxdma_err_dst_ring[mac_id].ring_id;
	srng = &ab->hal.srng_list[ring_id];

	mon_dst_srng = &ab->hal.srng_list[ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, mon_dst_srng);

	ppdu_id = pmon->mon_ppdu_info.ppdu_id;
	rx_mon_stats = &pmon->rx_mon_stats;

	while ((ring_entry = ath12k_hal_srng_dst_peek(ab, mon_dst_srng))) {
		struct sk_buff *head_msdu, *tail_msdu;

		head_msdu = NULL;
		tail_msdu = NULL;

		mpdu_rx_bufs_used =
			ath12k_wifi7_dp_mon_rx_mpdu_pop(dp_pdev, mac_id, ring_entry,
							&head_msdu, &tail_msdu,
							&rx_desc_used_list,
							&npackets, &ppdu_id);

		rx_bufs_used += mpdu_rx_bufs_used;

		if (mpdu_rx_bufs_used) {
			dp_mon->mon_dest_ring_stuck_cnt = 0;
		} else {
			dp_mon->mon_dest_ring_stuck_cnt++;
			rx_mon_stats->dest_mon_not_reaped++;
		}

		if (dp_mon->mon_dest_ring_stuck_cnt > MON_DEST_RING_STUCK_MAX_CNT) {
			rx_mon_stats->dest_mon_stuck++;
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "status ring ppdu_id=%d dest ring ppdu_id=%d mon_dest_ring_stuck_cnt=%d dest_mon_not_reaped=%u dest_mon_stuck=%u\n",
				   pmon->mon_ppdu_info.ppdu_id, ppdu_id,
				   dp_mon->mon_dest_ring_stuck_cnt,
				   rx_mon_stats->dest_mon_not_reaped,
				   rx_mon_stats->dest_mon_stuck);
			spin_lock_bh(&pmon->mon_lock);
			pmon->mon_ppdu_info.ppdu_id = ppdu_id;
			spin_unlock_bh(&pmon->mon_lock);
			continue;
		}

		if (ppdu_id != pmon->mon_ppdu_info.ppdu_id) {
			spin_lock_bh(&pmon->mon_lock);
			pmon->mon_ppdu_status = DP_PPDU_STATUS_START;
			spin_unlock_bh(&pmon->mon_lock);
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "dest_rx: new ppdu_id %x != status ppdu_id %x dest_mon_not_reaped = %u dest_mon_stuck = %u\n",
				   ppdu_id, pmon->mon_ppdu_info.ppdu_id,
				   rx_mon_stats->dest_mon_not_reaped,
				   rx_mon_stats->dest_mon_stuck);
			break;
		}

		if (head_msdu && tail_msdu) {
			tmp_mpdu = kzalloc(sizeof(*tmp_mpdu), GFP_ATOMIC);
			if (!tmp_mpdu)
				break;

			tmp_mpdu->head = head_msdu;
			tmp_mpdu->tail = tail_msdu;
			tmp_mpdu->err_bitmap = pmon->err_bitmap;
			tmp_mpdu->decap_format = pmon->decap_format;
			ath12k_dp_mon_rx_deliver(dp_pdev, tmp_mpdu,
						 &pmon->mon_ppdu_info, napi);
			rx_mon_stats->dest_mpdu_done++;
			kfree(tmp_mpdu);
		}

		ring_entry = ath12k_hal_srng_dst_get_next_entry(ab,
								mon_dst_srng);
	}
	ath12k_hal_srng_access_end(ab, mon_dst_srng);

	spin_unlock_bh(&srng->lock);

	if (rx_bufs_used) {
		rx_mon_stats->dest_ppdu_done++;
		ath12k_dp_rx_bufs_replenish(dp,
					    &dp->rx_refill_buf_ring,
					    &rx_desc_used_list);
	}
}

int ath12k_wifi7_dp_mon_rx_quad_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
					     struct napi_struct *napi, int *budget)
{
	struct ath12k *ar = pdev_dp->ar;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct ath12k_pdev_mon_stats *rx_mon_stats = &pmon->rx_mon_stats;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	enum hal_rx_mon_status hal_status;
	struct sk_buff_head skb_list;
	int num_buffs_reaped;
	struct sk_buff *skb;

	__skb_queue_head_init(&skb_list);

	num_buffs_reaped = ath12k_wifi7_dp_mon_rx_reap_status_ring(pdev_dp, mac_id,
								   budget, &skb_list);
	if (!num_buffs_reaped)
		goto exit;

	while ((skb = __skb_dequeue(&skb_list))) {
		memset(ppdu_info, 0, sizeof(*ppdu_info));
		ppdu_info->peer_id = HAL_INVALID_PEERID;

		hal_status = ath12k_wifi7_dp_mon_rx_parse_dest(pdev_dp, skb);

		if (ar->monitor_started &&
		    pmon->mon_ppdu_status == DP_PPDU_STATUS_START &&
		    hal_status == HAL_TLV_STATUS_PPDU_DONE) {
			rx_mon_stats->status_ppdu_done++;
			pmon->mon_ppdu_status = DP_PPDU_STATUS_DONE;
			ath12k_wifi7_dp_mon_rx_dest_process(pdev_dp, mac_id, *budget, napi);
			pmon->mon_ppdu_status = DP_PPDU_STATUS_START;
		}

		dev_kfree_skb_any(skb);
	}
exit:
	return num_buffs_reaped;
}

void ath12k_wifi7_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_wifi7_dp_mon_rx_mon_mode_config_filter(dp_pdev);
}

void ath12k_wifi7_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_wifi7_dp_mon_rx_mon_mode_reset_filter(dp_pdev);
}
