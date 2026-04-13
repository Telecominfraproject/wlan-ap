// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/relay.h>
#include "core.h"
#include "debug.h"

bool peer_is_in_cfr_unassoc_pool(struct ath11k *ar, u8 *peer_mac)
{
	struct ath11k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	int i;

	if (!ar->cfr_enabled)
		return false;

	spin_lock_bh(&cfr->lock);
	for (i = 0; i < ATH11K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];
		if (!entry->is_valid)
			continue;

		if (ether_addr_equal(peer_mac, entry->peer_mac)) {
			/* Remove entry if it is single shot */
			if (entry->period == 0) {
				memset(entry->peer_mac, 0 , ETH_ALEN);
				entry->is_valid = false;
				cfr->cfr_enabled_peer_cnt--;
			}
			spin_unlock_bh(&cfr->lock);
			return true;
		}
	}

	spin_unlock_bh(&cfr->lock);

	return false;
}

void ath11k_cfr_lut_update_paddr(struct ath11k *ar, dma_addr_t paddr,
				 u32 buf_id)
{
	struct ath11k_cfr *cfr = &ar->cfr;
	struct ath11k_cfr_look_up_table *lut;

	if (cfr->lut) {
		lut = &cfr->lut[buf_id];
		lut->dbr_address = paddr;
	}
}

void ath11k_cfr_decrement_peer_count(struct ath11k *ar,
				     struct ath11k_sta *arsta)
{
	struct ath11k_cfr *cfr = &ar->cfr;

	spin_lock_bh(&cfr->lock);

	if (arsta->cfr_capture.cfr_enable)
		cfr->cfr_enabled_peer_cnt--;

	spin_unlock_bh(&cfr->lock);
}

struct ath11k_dbring *ath11k_cfr_get_dbring(struct ath11k *ar)
{
	if (ar->cfr_enabled)
		return &ar->cfr.rx_ring;
	else
		return NULL;
}

static int cfr_calculate_tones_form_dma_hdr(struct ath11k_cfir_dma_hdr *hdr)
{
	u8 bw = FIELD_GET(CFIR_DMA_HDR_INFO1_UPLOAD_PKT_BW, hdr->info1);
	u8 preamble = FIELD_GET(CFIR_DMA_HDR_INFO1_PREABLE_TYPE, hdr->info1);

	switch (preamble) {
	case ATH11K_CFR_PREAMBLE_TYPE_LEGACY:
	case ATH11K_CFR_PREAMBLE_TYPE_VHT:
		switch (bw) {
		case 0:
			return TONES_IN_20MHZ;
		case 1: /* DUP40/VHT40 */
			return TONES_IN_40MHZ;
		case 2: /* DUP80/VHT80 */
			return TONES_IN_80MHZ;
		case 3: /* DUP160/VHT160 */
			return TONES_IN_160MHZ;
		}
		fallthrough;
	case ATH11K_CFR_PREAMBLE_TYPE_HT:
		switch (bw) {
		case 0:
			return TONES_IN_20MHZ;
		case 1:
			return TONES_IN_40MHZ;
		}
	}

	return TONES_INVALID;
}

static inline
void ath11k_cfr_release_lut_entry(struct ath11k_cfr_look_up_table *lut)
{
	memset(lut, 0, sizeof(*lut));
}

static void ath11k_cfr_rfs_write(struct ath11k *ar, const void *head,
				 u32 head_len, const void *data, u32 data_len,
				 const void * tail, int tail_data)
{
	struct ath11k_cfr *cfr = &ar->cfr;

	if (!ar->cfr.rfs_cfr_capture)
		return;

	relay_write(cfr->rfs_cfr_capture, head, head_len);
	relay_write(cfr->rfs_cfr_capture, data, data_len);
	relay_write(cfr->rfs_cfr_capture, tail, tail_data);
	relay_flush(cfr->rfs_cfr_capture);
}

static void ath11k_cfr_free_pending_dbr_events(struct ath11k *ar)
{
	struct ath11k_cfr *cfr = &ar->cfr;
	struct ath11k_cfr_look_up_table *lut = NULL;
	int i;

	if (!cfr->lut)
		return;

	for (i = 0; i < cfr->lut_num; i++) {
		lut = &cfr->lut[i];
		if (lut->dbr_recv && !lut->tx_recv &&
		    (lut->dbr_tstamp < cfr->last_success_tstamp)) {
			ath11k_dbring_bufs_replenish(ar, &cfr->rx_ring, lut->buff,
						     WMI_DIRECT_BUF_CFR);
			ath11k_cfr_release_lut_entry(lut);
			cfr->flush_dbr_cnt++;
		}
	}
}

/* Correlate and relay: This function correlate the data coming from
 * WMI_PDEV_DMA_RING_BUF_RELEASE_EVENT(DBR event) and
 * WMI_PEER_CFR_CAPTURE_EVENT(Tx capture event). if both the events
 * are received and PPDU id matches from the both events,
 * return CORRELATE_STATUS_RELEASE which means relay the correlated data
 * to user space. Otherwise return CORRELATE_STATUS_HOLD which means wait
 * for the second event to come. It will return CORRELATE_STATUS_ERR in
 * case of any error.
 *
 * It also check for the pending DBR events and clear those events
 * in case of corresponding TX capture event is not received for
 * the PPDU.
 */

static int ath11k_cfr_correlate_and_relay(struct ath11k *ar,
					  struct ath11k_cfr_look_up_table *lut,
					  u8 event_type)
{
	struct ath11k_cfr *cfr = &ar->cfr;
	u64 diff;

	if (event_type == ATH11K_CORRELATE_TX_EVENT) {
		if (lut->tx_recv)
			cfr->cfr_dma_aborts++;
		cfr->tx_evt_cnt++;
		lut->tx_recv = true;
	} else if (event_type == ATH11K_CORRELATE_DBR_EVENT) {
		cfr->dbr_evt_cnt++;
		lut->dbr_recv = true;
	}

	if (lut->dbr_recv && lut->tx_recv) {
		if (lut->dbr_ppdu_id == lut->tx_ppdu_id) {
			cfr->last_success_tstamp = lut->dbr_tstamp;
			if (lut->dbr_tstamp > lut->txrx_tstamp) {
				diff = lut->dbr_tstamp - lut->txrx_tstamp;
				ath11k_dbg(ar->ab, ATH11K_DBG_CFR,
					   "txrx event -> dbr event delay = %u ms",
					   jiffies_to_msecs(diff));
			} else if (lut->txrx_tstamp > lut->dbr_tstamp) {
				diff = lut->txrx_tstamp - lut->dbr_tstamp;
				ath11k_dbg(ar->ab, ATH11K_DBG_CFR,
					   "dbr event -> txrx event delay = %u ms",
					   jiffies_to_msecs(diff));
			}
			 /* Skip for IPQ8074, since its header length and data
			    length are calculated in host itself */
			if (ar->ab->hw_rev != ATH11K_HW_IPQ8074) {
				if (lut->header_length > ar->ab->hw_params.cfr_max_header_len_words ||
				    lut->payload_length > ar->ab->hw_params.cfr_max_data_len) {
					cfr->invalid_dma_length_cnt++;
					ath11k_dbg(ar->ab, ATH11K_DBG_CFR,
						   "Invalid hdr/payload len hdr %u payload %u\n",
						   lut->header_length,
						   lut->payload_length);
					return ATH11K_CORRELATE_STATUS_ERR;
				}
			}
			ath11k_cfr_free_pending_dbr_events(ar);

			cfr->release_cnt++;
			return ATH11K_CORRELATE_STATUS_RELEASE;
		} else {
			/*
			 * When there is a ppdu id mismatch, discard the TXRX
			 * event since multiple PPDUs are likely to have same
			 * dma addr, due to ucode aborts.
			 */

			ath11k_dbg(ar->ab, ATH11K_DBG_CFR,
				   "Received dbr event twice for the same lut entry");
			lut->tx_recv = false;
			lut->tx_ppdu_id = 0;
			cfr->clear_txrx_event++;
			cfr->cfr_dma_aborts++;
			return ATH11K_CORRELATE_STATUS_HOLD;
		}
	} else {
		return ATH11K_CORRELATE_STATUS_HOLD;
	}
}

static u8 freeze_reason_to_capture_type(void *freeze_tlv)
{
	struct macrx_freeze_capture_channel *freeze = freeze_tlv;
	u8 capture_reason = FIELD_GET(MACRX_FREEZE_CC_INFO0_CAPTURE_REASON,
				      freeze->info0);

	switch (capture_reason) {
	case FREEZE_REASON_TM:
		return CFR_CAPTURE_METHOD_TM;
	case FREEZE_REASON_FTM:
		return CFR_CAPTURE_METHOD_FTM;
	case FREEZE_REASON_TA_RA_TYPE_FILTER:
		return CFR_CAPTURE_METHOD_TA_RA_TYPE_FILTER;
	case FREEZE_REASON_NDPA_NDP:
		return CFR_CAPTURE_METHOD_NDPA_NDP;
	case FREEZE_REASON_ALL_PACKET:
		return CFR_CAPTURE_METHOD_ALL_PACKET;
	case FREEZE_REASON_ACK_RESP_TO_TM_FTM:
		return CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM;
	default:
		return CFR_CAPTURE_METHOD_AUTO;
	}

	return CFR_CAPTURE_METHOD_AUTO;
}

static void
extract_peer_mac_from_freeze_tlv(void *freeze_tlv, uint8_t *peermac)
{
	struct macrx_freeze_capture_channel_v3 *freeze =
		(struct macrx_freeze_capture_channel_v3 *)freeze_tlv;

	peermac[0] = freeze->packet_ta_lower_16 & 0x00FF;
	peermac[1] = (freeze->packet_ta_lower_16 & 0xFF00) >> 8;
	peermac[2] = freeze->packet_ta_mid_16 & 0x00FF;
	peermac[3] = (freeze->packet_ta_mid_16 & 0xFF00) >> 8;
	peermac[4] = freeze->packet_ta_upper_16 & 0x00FF;
	peermac[5] = (freeze->packet_ta_upper_16 & 0xFF00) >> 8;
}

static int ath11k_cfr_enh_process_data(struct ath11k *ar,
				       struct ath11k_dbring_data *param)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_cfr *cfr = &ar->cfr;
	struct ath11k_cfr_look_up_table *lut;
	struct ath11k_csi_cfr_header *header;
	struct ath11k_cfir_enh_dma_hdr dma_hdr;
	struct cfr_metadata_version_5 *meta;
	void *mu_rx_user_info = NULL, *freeze_tlv = NULL;
	u8 *peer_macaddr;
	u8 *data;
	u32 buf_id;
	u32 length;
	u32 freeze_tlv_len = 0;
	u32 end_magic = ATH11K_CFR_END_MAGIC;
	u8 freeze_tlv_ver;
	u8 capture_type;
	int ret = 0;
	int status;

	data = param->data;
	buf_id = param->buf_id;

	memcpy(&dma_hdr, data, sizeof(struct ath11k_cfir_enh_dma_hdr));

	freeze_tlv_ver = FIELD_GET(CFIR_DMA_HDR_INFO2_FREEZ_TLV_VER, dma_hdr.info2);

	if (FIELD_GET(CFIR_DMA_HDR_INFO2_FREEZ_DATA_INC, dma_hdr.info2)) {
		freeze_tlv = data + sizeof(struct ath11k_cfir_enh_dma_hdr);
		capture_type = freeze_reason_to_capture_type(freeze_tlv);
	}

	if (FIELD_GET(CFIR_DMA_HDR_INFO2_MURX_DATA_INC, dma_hdr.info2)) {
		if (freeze_tlv_ver == MACRX_FREEZE_TLV_VERSION_3)
			freeze_tlv_len = sizeof(struct macrx_freeze_capture_channel_v3);
		else
			freeze_tlv_len = sizeof(struct macrx_freeze_capture_channel);

		mu_rx_user_info = data + sizeof(struct ath11k_cfir_enh_dma_hdr) +
				  freeze_tlv_len;
	}

	length = FIELD_GET(CFIR_DMA_HDR_INFO0_LEN, dma_hdr.hdr.info0) * 4;
	length += dma_hdr.total_bytes;

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut) {
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	lut = &cfr->lut[buf_id];
	if (!lut) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "lut failure to process cfr data id:%d\n", buf_id);
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}


	ath11k_dbg_dump(ab, ATH11K_DBG_CFR_DUMP,"data_from_buf_rel:", "",
			data, length);

	lut->buff = param->buff;
	lut->data = data;
	lut->data_len = length;
	lut->dbr_ppdu_id = dma_hdr.hdr.phy_ppdu_id;
	lut->dbr_tstamp = jiffies;
	lut->header_length = FIELD_GET(CFIR_DMA_HDR_INFO0_LEN, dma_hdr.hdr.info0);
	lut->payload_length = dma_hdr.total_bytes;
	memcpy(&lut->dma_hdr.enh_hdr, &dma_hdr, sizeof(struct ath11k_cfir_enh_dma_hdr));

	header = &lut->header;
	meta = &header->u.meta_v5;
	meta->channel_bw = FIELD_GET(CFIR_DMA_HDR_INFO1_UPLOAD_PKT_BW,
				     dma_hdr.hdr.info1);
	meta->num_rx_chain =
		NUM_CHAINS_FW_TO_HOST(FIELD_GET(CFIR_DMA_HDR_INFO1_NUM_CHAINS,
						dma_hdr.hdr.info1));
	meta->length = length;

	if (capture_type != CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM) {
		meta->capture_type = capture_type;
		meta->sts_count = FIELD_GET(CFIR_DMA_HDR_INFO1_NSS, dma_hdr.hdr.info1) + 1;
		if (FIELD_GET(CFIR_DMA_HDR_INFO2_MURX_DATA_INC, dma_hdr.info2)) {
			peer_macaddr = meta->peer_addr.su_peer_addr;
			if (freeze_tlv)
				extract_peer_mac_from_freeze_tlv(freeze_tlv, peer_macaddr);
		}
	}

	status = ath11k_cfr_correlate_and_relay(ar, lut,
						ATH11K_CORRELATE_DBR_EVENT);

	if (status == ATH11K_CORRELATE_STATUS_RELEASE) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "releasing CFR data to user space");
		ath11k_cfr_rfs_write(ar, &lut->header,
				sizeof(struct ath11k_csi_cfr_header),
				lut->data, lut->data_len,
				&end_magic, sizeof(u32));
		ath11k_cfr_release_lut_entry(lut);
		ret = ATH11K_CORRELATE_STATUS_RELEASE;
	} else if (status == ATH11K_CORRELATE_STATUS_HOLD) {
		ret = ATH11K_CORRELATE_STATUS_HOLD;
		ath11k_dbg(ab, ATH11K_DBG_CFR,
				"tx event is not yet received holding the buf");
	} else {
		ath11k_cfr_release_lut_entry(lut);
		ret = ATH11K_CORRELATE_STATUS_ERR;
		ath11k_err(ab, "error in processing buf rel event");
	}

	spin_unlock_bh(&cfr->lut_lock);

	return ret;
}

static int ath11k_cfr_process_data(struct ath11k *ar,
				   struct ath11k_dbring_data *param)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_cfr *cfr = &ar->cfr;
	struct ath11k_cfr_look_up_table *lut;
	struct ath11k_csi_cfr_header *header;
	struct ath11k_cfir_dma_hdr dma_hdr;
	u8 *data;
	u32 end_magic = ATH11K_CFR_END_MAGIC;
	u32 buf_id;
	u32 tones;
	u32 length;
	int status;
	u8 num_chains;
	int ret = 0;

	data = param->data;
	buf_id = param->buf_id;

	memcpy(&dma_hdr, data, sizeof(struct ath11k_cfir_dma_hdr));

	tones = cfr_calculate_tones_form_dma_hdr(&dma_hdr);
	if (tones == TONES_INVALID) {
		ath11k_err(ar->ab, "Number of tones received is invalid");
		return -EINVAL;
	}

	num_chains = FIELD_GET(CFIR_DMA_HDR_INFO1_NUM_CHAINS,
			       dma_hdr.info1);

	length = sizeof(struct ath11k_cfir_dma_hdr);
	length += tones * (num_chains + 1);

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut) {
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	lut = &cfr->lut[buf_id];

	if (!lut) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "lut failure to process cfr data id:%d\n", buf_id);
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	ath11k_dbg_dump(ab, ATH11K_DBG_CFR_DUMP,"data_from_buf_rel:", "",
			data, length);

	lut->buff = param->buff;
	lut->data = data;
	lut->data_len = length;
	lut->dbr_ppdu_id = dma_hdr.phy_ppdu_id;
	lut->dbr_tstamp = jiffies;

	memcpy(&lut->dma_hdr.hdr, &dma_hdr, sizeof(struct ath11k_cfir_dma_hdr));

	header = &lut->header;
	header->u.meta_v4.channel_bw = FIELD_GET(CFIR_DMA_HDR_INFO1_UPLOAD_PKT_BW,
						 dma_hdr.info1);
	header->u.meta_v4.length = length;

	status = ath11k_cfr_correlate_and_relay(ar, lut,
						ATH11K_CORRELATE_DBR_EVENT);
	if (status == ATH11K_CORRELATE_STATUS_RELEASE) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "releasing CFR data to user space");
		ath11k_cfr_rfs_write(ar, &lut->header,
				     sizeof(struct ath11k_csi_cfr_header),
				     lut->data, lut->data_len,
				     &end_magic, sizeof(u32));
		ath11k_cfr_release_lut_entry(lut);
		ret = ATH11K_CORRELATE_STATUS_RELEASE;
	} else if (status == ATH11K_CORRELATE_STATUS_HOLD) {
		ret = ATH11K_CORRELATE_STATUS_HOLD;
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "tx event is not yet received holding the buf");
	} else {
		ret = ATH11K_CORRELATE_STATUS_ERR;
		ath11k_cfr_release_lut_entry(lut);
		ath11k_err(ab, "error in correlating events");
	}

	spin_unlock_bh(&cfr->lut_lock);

	return ret;
}

int ath11k_process_cfr_capture_event(struct ath11k_base *ab,
				     struct ath11k_cfr_peer_tx_param *params)
{
	struct ath11k *ar;
	struct ath11k_cfr *cfr;
	struct ath11k_vif *arvif;
	struct ath11k_cfr_look_up_table *lut = NULL, *temp = NULL;
	struct ath11k_dbring_element *buff;
	struct ath11k_csi_cfr_header *header;
	dma_addr_t buf_addr;
	u32 end_magic = ATH11K_CFR_END_MAGIC;
	u8 tx_status;
	int ret = 0;
	int status;
	int i;

	rcu_read_lock();
	arvif = ath11k_mac_get_arvif_by_vdev_id(ab, params->vdev_id);
	if (!arvif) {
		ath11k_warn(ab, "Failed to get arvif for vdev id %d\n",
			    params->vdev_id);
		rcu_read_unlock();
		return -ENOENT;
	}

	ar = arvif->ar;
	cfr = &ar->cfr;
	rcu_read_unlock();

	if (WMI_CFR_CAPTURE_STATUS_PEER_PS & params->status) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "CFR capture failed as peer %pM is in powersave",
			   params->peer_mac_addr);
		return -EINVAL;
	}

	if (!(WMI_CFR_PEER_CAPTURE_STATUS & params->status)) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "CFR capture failed for the peer : %pM",
			   params->peer_mac_addr);
		cfr->tx_peer_status_cfr_fail++;
		return -EINVAL;
	}

	tx_status = FIELD_GET(WMI_CFR_FRAME_TX_STATUS, params->status);

	if (tx_status != WMI_FRAME_TX_STATUS_OK) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "WMI tx status %d for the peer %pM",
			   tx_status, params->peer_mac_addr);
		cfr->tx_evt_status_cfr_fail++;
		return -EINVAL;
	}

	buf_addr = (((u64)FIELD_GET(WMI_CFR_CORRELATION_INFO2_BUF_ADDR_HIGH,
				    params->correlation_info_2)) << 32) |
		   params->correlation_info_1;

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut) {
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	for (i = 0; i < cfr->lut_num; i++) {
		temp = &cfr->lut[i];
		if (temp->dbr_address == buf_addr) {
			lut = &cfr->lut[i];
			break;
		}
	}

	if (!lut) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "lut failure to process tx event\n");
		cfr->tx_dbr_lookup_fail++;
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	lut->tx_ppdu_id = FIELD_GET(WMI_CFR_CORRELATION_INFO2_PPDU_ID,
				    params->correlation_info_2);
	lut->tx_address1 = params->correlation_info_1;
	lut->tx_address2 = params->correlation_info_2;
	lut->txrx_tstamp = jiffies;

	header = &lut->header;
	header->start_magic_num = ATH11K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->pltform_type = PLATFORM_TYPE_ARM;

	ab->hw_params.hw_ops->fill_cfr_hdr_info(ar, header, params);

	status = ath11k_cfr_correlate_and_relay(ar, lut,
						ATH11K_CORRELATE_TX_EVENT);
	if (status == ATH11K_CORRELATE_STATUS_RELEASE) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "Releasing CFR data to user space");
		ath11k_cfr_rfs_write(ar, &lut->header,
				     sizeof(struct ath11k_csi_cfr_header),
				     lut->data, lut->data_len,
				     &end_magic, sizeof(u32));
		buff = lut->buff;
		ath11k_cfr_release_lut_entry(lut);

		ath11k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
					     WMI_DIRECT_BUF_CFR);
	} else if (status == ATH11K_CORRELATE_STATUS_HOLD) {
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "dbr event is not yet received holding buf\n");
	} else {
		buff = lut->buff;
		ath11k_cfr_release_lut_entry(lut);
		ath11k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
					     WMI_DIRECT_BUF_CFR);
		ret = -EINVAL;
	}

	spin_unlock_bh(&cfr->lut_lock);
	return ret;
}

static struct dentry *create_buf_file_handler(const char *filename,
					      struct dentry *parent,
					      umode_t mode,
					      struct rchan_buf *buf,
					      int *is_global)
{
	struct dentry *buf_file;

	buf_file = debugfs_create_file(filename, mode, parent, buf,
				       &relay_file_operations);
	*is_global = 1;
	return buf_file;
}

static int remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);

	return 0;
}

static struct rchan_callbacks rfs_cfr_capture_cb = {
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

static ssize_t ath11k_read_file_enable_cfr(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32] = {0};
	size_t len;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf), "%d\n", ar->cfr_enabled);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_write_file_enable_cfr(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 enable_cfr;
	int ret;

	if (kstrtou8_from_user(ubuf, count, 0, &enable_cfr))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (enable_cfr > 1) {
		ret = -EINVAL;
		goto out;
	}

	if (ar->cfr_enabled == enable_cfr) {
		ret = count;
		goto out;
	}

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_PER_PEER_CFR_ENABLE,
					enable_cfr, ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab,
			    "Failed to enable/disable per peer cfr (%d)\n",
			    ret);
		goto out;
	}

	ar->cfr_enabled = enable_cfr;
	ret = count;

out:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_enable_cfr = {
	.read = ath11k_read_file_enable_cfr,
	.write = ath11k_write_file_enable_cfr,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_file_cfr_unassoc(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	char buf[64] = {0};
	u8 peer_mac[6];
	u32 cfr_capture_enable;
	u32 cfr_capture_period;
	int available_idx = -1;
	int ret, i;

	simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);

	mutex_lock(&ar->conf_mutex);
	spin_lock_bh(&cfr->lock);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (!ar->cfr_enabled) {
		ret = -EINVAL;
		ath11k_err(ar->ab, "CFR is not enabled on this pdev %d\n",
			   ar->pdev_idx);
		goto out;
	}

	ret = sscanf(buf, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx %u %u",
		     &peer_mac[0], &peer_mac[1], &peer_mac[2], &peer_mac[3],
		     &peer_mac[4], &peer_mac[5], &cfr_capture_enable,
		     &cfr_capture_period);

	if (ret < 1) {
		ret = -EINVAL;
		goto out;
	}

	if (cfr_capture_enable && ret != 8) {
		ret = -EINVAL;
		goto out;
	}

	if (!cfr_capture_enable) {
		for (i = 0; i < ATH11K_MAX_CFR_ENABLED_CLIENTS; i++) {
			entry = &cfr->unassoc_pool[i];
			if (ether_addr_equal(peer_mac, entry->peer_mac)) {
				memset(entry->peer_mac, 0, ETH_ALEN);
				entry->is_valid = false;
				cfr->cfr_enabled_peer_cnt--;
			}
		}

		ret = count;
		goto out;
	}


	if (cfr->cfr_enabled_peer_cnt >= ATH11K_MAX_CFR_ENABLED_CLIENTS) {
		ath11k_info(ar->ab, "Max cfr peer threshold reached\n");
		ret = count;
		goto out;
	}

	for (i = 0; i < ATH11K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];

		if ((available_idx < 0) && !entry->is_valid)
			available_idx = i;

		if (ether_addr_equal(peer_mac, entry->peer_mac)) {
			ath11k_info(ar->ab,
				    "peer entry already present updating params\n");
			entry->period = cfr_capture_period;
			ret = count;
			goto out;
		}
	}

	if (available_idx >= 0) {
		entry = &cfr->unassoc_pool[available_idx];
		ether_addr_copy(entry->peer_mac, peer_mac);
		entry->period = cfr_capture_period;
		entry->is_valid = true;
		cfr->cfr_enabled_peer_cnt++;
	}

	ret = count;
out:
	spin_unlock_bh(&cfr->lock);
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_file_cfr_unassoc(struct file *file,
					    char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	char buf[512] = {0};
	struct ath11k *ar = file->private_data;
	struct ath11k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	int len = 0, i;

	mutex_lock(&ar->conf_mutex);
	spin_lock_bh(&cfr->lock);

	for (i = 0; i < ATH11K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];
		if (entry->is_valid)
			len += scnprintf(buf + len, sizeof(buf) - len,
					 "peer: %pM period: %u\n",
					 entry->peer_mac, entry->period);
	}

	spin_unlock_bh(&cfr->lock);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_configure_cfr_unassoc = {
	.write = ath11k_write_file_cfr_unassoc,
	.read = ath11k_read_file_cfr_unassoc,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static inline void ath11k_cfr_debug_unregister(struct ath11k *ar)
{
	debugfs_remove(ar->cfr.enable_cfr);
	ar->cfr.enable_cfr = NULL;
	debugfs_remove(ar->cfr.cfr_unassoc);
	ar->cfr.cfr_unassoc = NULL;

	if (ar->cfr.rfs_cfr_capture) {
		relay_close(ar->cfr.rfs_cfr_capture);
		ar->cfr.rfs_cfr_capture = NULL;
	}
}

static inline int ath11k_cfr_debug_register(struct ath11k *ar)
{
	int ret;

	ar->cfr.rfs_cfr_capture = relay_open("cfr_capture",
					     ar->debug.debugfs_pdev,
					     ar->ab->hw_params.cfr_stream_buf_size,
					     ar->ab->hw_params.cfr_num_stream_bufs,
					     &rfs_cfr_capture_cb, NULL);
	if (!ar->cfr.rfs_cfr_capture) {
		ath11k_warn(ar->ab, "failed to open relay for cfr in pdev %d\n",
			    ar->pdev_idx);
		return -EINVAL;
	}

	ar->cfr.enable_cfr = debugfs_create_file("enable_cfr", 0600,
						 ar->debug.debugfs_pdev, ar,
						 &fops_enable_cfr);
	if (!ar->cfr.enable_cfr) {
		ath11k_warn(ar->ab, "failed to open debugfs in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	ar->cfr.cfr_unassoc = debugfs_create_file("cfr_unassoc", 0600,
						  ar->debug.debugfs_pdev, ar,
						  &fops_configure_cfr_unassoc);

	if (!ar->cfr.cfr_unassoc) {
		ath11k_warn(ar->ab,
			    "failed to open debugfs for unassoc pool in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	return 0;

debug_unregister :
	ath11k_cfr_debug_unregister(ar);
	return ret;
}

static int ath11k_cfr_ring_alloc(struct ath11k *ar,
				 struct ath11k_dbring_cap *db_cap)
{
	struct ath11k_cfr *cfr = &ar->cfr;
	int ret;

	ret = ath11k_dbring_srng_setup(ar, &cfr->rx_ring,
				       1, db_cap->min_elem);
	if (ret) {
		ath11k_warn(ar->ab, "failed to setup db ring\n");
		return ret;
	}

	ath11k_dbring_set_cfg(ar, &cfr->rx_ring,
			      ATH11K_CFR_NUM_RESP_PER_EVENT,
			      ATH11K_CFR_EVENT_TIMEOUT_MS,
			      ((ar->ab->hw_rev == ATH11K_HW_IPQ8074) ?
			       ath11k_cfr_process_data :
			       ath11k_cfr_enh_process_data));

	ret = ath11k_dbring_buf_setup(ar, &cfr->rx_ring, db_cap);
	if (ret) {
		ath11k_warn(ar->ab, "failed to setup db ring buffer\n");
		goto srng_cleanup;
	}

	ret = ath11k_dbring_wmi_cfg_setup(ar, &cfr->rx_ring, WMI_DIRECT_BUF_CFR);
	if (ret) {
		ath11k_warn(ar->ab, "failed to setup db ring cfg\n");
		goto buffer_cleanup;
	}

	return 0;

buffer_cleanup:
	ath11k_dbring_buf_cleanup(ar, &cfr->rx_ring);
srng_cleanup:
	ath11k_dbring_srng_cleanup(ar, &cfr->rx_ring);
	return ret;
}

void ath11k_cfr_ring_free(struct ath11k *ar)
{
	struct ath11k_cfr *cfr = &ar->cfr;

	ath11k_dbring_srng_cleanup(ar, &cfr->rx_ring);
	ath11k_dbring_buf_cleanup(ar, &cfr->rx_ring);
}

void ath11k_cfr_deinit(struct ath11k_base *ab)
{
	struct ath11k *ar;
	struct ath11k_cfr *cfr;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_CFR_CAPTURE_SUPPORT,
		      ab->wmi_ab.svc_map) || !ab->hw_params.cfr_support)
		return;

	for (i = 0; i <  ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		cfr = &ar->cfr;

		ath11k_cfr_debug_unregister(ar);
		ath11k_cfr_ring_free(ar);

		spin_lock_bh(&cfr->lut_lock);
		if (cfr->lut) {
			kfree(cfr->lut);
			cfr->lut = NULL;
		}

		spin_unlock_bh(&cfr->lut_lock);
		ar->cfr_enabled = 0;
	}
}

int ath11k_cfr_init(struct ath11k_base *ab)
{
	struct ath11k *ar;
	struct ath11k_cfr *cfr;
	struct ath11k_dbring_cap db_cap;
	struct ath11k_cfr_look_up_table *lut;
	u32 num_lut_entries;
	int ret = 0;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_CFR_CAPTURE_SUPPORT,
		      ab->wmi_ab.svc_map) || !ab->hw_params.cfr_support)
		return ret;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		cfr = &ar->cfr;

		ret = ath11k_dbring_get_cap(ar->ab, ar->pdev_idx,
					    WMI_DIRECT_BUF_CFR, &db_cap);
		if (ret)
			continue;

		idr_init(&cfr->rx_ring.bufs_idr);
		spin_lock_init(&cfr->rx_ring.idr_lock);
		spin_lock_init(&cfr->lock);
		spin_lock_init(&cfr->lut_lock);

		num_lut_entries = min((u32)CFR_MAX_LUT_ENTRIES, db_cap.min_elem);

		cfr->lut = kzalloc(num_lut_entries * sizeof(*lut), GFP_KERNEL);
		if (!cfr->lut) {
			ath11k_warn(ab, "failed to allocate lut for pdev %d\n", i);
			return -ENOMEM;
		}

		ret = ath11k_cfr_ring_alloc(ar, &db_cap);
		if (ret) {
			ath11k_warn(ab, "failed to init cfr ring for pdev %d\n", i);
			goto deinit;
		}

		spin_lock_bh(&cfr->lock);
		cfr->lut_num = num_lut_entries;
		spin_unlock_bh(&cfr->lock);

		ret = ath11k_cfr_debug_register(ar);
		if (ret) {
			ath11k_warn(ab, "failed to register cfr for pdev %d\n", i);
			goto deinit;
		}
	}

	return 0;

deinit:
	ath11k_cfr_deinit(ab);
	return ret;
}
