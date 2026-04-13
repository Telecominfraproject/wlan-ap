// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/relay.h>
#include "core.h"
#include "debug.h"

bool peer_is_in_cfr_unassoc_pool(struct ath12k *ar, u8 *peer_mac)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	int i;

	if (!ar->cfr.cfr_enabled)
		return false;

	spin_lock_bh(&cfr->lock);
	for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
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

void ath12k_cfr_lut_update_paddr(struct ath12k *ar, dma_addr_t paddr,
				 u32 buf_id)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_cfr_look_up_table *lut;

	if (cfr->lut) {
		lut = &cfr->lut[buf_id];
		lut->dbr_address = paddr;
	}
}

void ath12k_cfr_decrement_peer_count(struct ath12k *ar,
				     struct ath12k_link_sta *arsta)
{
	struct ath12k_cfr *cfr = &ar->cfr;

	spin_lock_bh(&cfr->lock);

	if (cfr->cfr_enabled_peer_cnt == 0) {
		spin_unlock_bh(&cfr->lock);
		return;
	}

	if (arsta->cfr_capture.cfr_enable)
		cfr->cfr_enabled_peer_cnt--;

	spin_unlock_bh(&cfr->lock);
}

struct ath12k_dbring *ath12k_cfr_get_dbring(struct ath12k *ar)
{
	if (ar->cfr.cfr_enabled)
		return &ar->cfr.rx_ring;

	return NULL;
}

static inline
void ath12k_cfr_release_lut_entry(struct ath12k_cfr_look_up_table *lut)
{
	memset(lut, 0, sizeof(*lut));
}

static void ath12k_cfr_rfs_write(struct ath12k *ar, const void *head,
				 u32 head_len, const void *data, u32 data_len,
				 const void * tail, int tail_data)
{
	struct ath12k_cfr *cfr = &ar->cfr;

	if (!ar->cfr.rfs_cfr_capture)
		return;

	relay_write(cfr->rfs_cfr_capture, head, head_len);
	relay_write(cfr->rfs_cfr_capture, data, data_len);
	relay_write(cfr->rfs_cfr_capture, tail, tail_data);
	relay_flush(cfr->rfs_cfr_capture);
}

static void ath12k_cfr_free_pending_dbr_events(struct ath12k *ar)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_cfr_look_up_table *lut = NULL;
	int i;

	if (!cfr->lut)
		return;

	for (i = 0; i < cfr->lut_num; i++) {
		lut = &cfr->lut[i];
		if (lut->dbr_recv && !lut->tx_recv &&
		    (lut->dbr_tstamp < cfr->last_success_tstamp)) {
			ath12k_dbring_bufs_replenish(ar, &cfr->rx_ring, lut->buff,
						     WMI_DIRECT_BUF_CFR, GFP_ATOMIC);
			ath12k_cfr_release_lut_entry(lut);
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

static int ath12k_cfr_correlate_and_relay(struct ath12k *ar,
					  struct ath12k_cfr_look_up_table *lut,
					  u8 event_type)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	u64 diff;

	if (event_type == ATH12K_CORRELATE_TX_EVENT) {
		if (lut->tx_recv)
			cfr->cfr_dma_aborts++;
		cfr->tx_evt_cnt++;
		lut->tx_recv = true;
	} else if (event_type == ATH12K_CORRELATE_DBR_EVENT) {
		cfr->dbr_evt_cnt++;
		lut->dbr_recv = true;
	}

	if (lut->dbr_recv && lut->tx_recv) {
		if (lut->dbr_ppdu_id == lut->tx_ppdu_id) {
			cfr->last_success_tstamp = lut->dbr_tstamp;
			if (lut->dbr_tstamp > lut->txrx_tstamp) {
				diff = lut->dbr_tstamp - lut->txrx_tstamp;
				ath12k_dbg(ar->ab, ATH12K_DBG_CFR,
					   "txrx event -> dbr event delay = %u ms",
					   jiffies_to_msecs(diff));
			} else if (lut->txrx_tstamp > lut->dbr_tstamp) {
				diff = lut->txrx_tstamp - lut->dbr_tstamp;
				ath12k_dbg(ar->ab, ATH12K_DBG_CFR,
					   "dbr event -> txrx event delay = %u ms",
					   jiffies_to_msecs(diff));
			}
			ath12k_cfr_free_pending_dbr_events(ar);

			cfr->release_cnt++;
			return ATH12K_CORRELATE_STATUS_RELEASE;
		} else {
			/*
			 * When there is a ppdu id mismatch, discard the TXRX
			 * event since multiple PPDUs are likely to have same
			 * dma addr, due to ucode aborts.
			 */

			ath12k_dbg(ar->ab, ATH12K_DBG_CFR,
				   "Received dbr event twice for the same lut entry");
			lut->tx_recv = false;
			lut->tx_ppdu_id = 0;
			cfr->clear_txrx_event++;
			cfr->cfr_dma_aborts++;
			return ATH12K_CORRELATE_STATUS_HOLD;
		}
	} else {
		return ATH12K_CORRELATE_STATUS_HOLD;
	}
}

static u8 freeze_reason_to_capture_type(struct ath12k_base *ab, void *freeze_tlv)
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
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "CFR Capture Method Type not found");
	}

	return CFR_CAPTURE_METHOD_AUTO;
}

static void extract_peer_mac_from_freeze_tlv(void *freeze_tlv, uint8_t *peermac)
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

static int ath12k_cfr_enh_process_data(struct ath12k *ar,
				       struct ath12k_dbring_data *param)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_cfr *cfr = &ar->cfr;
	struct ath12k_cfr_look_up_table *lut;
	struct ath12k_csi_cfr_header *header;
	struct ath12k_cfir_enh_dma_hdr dma_hdr;
	struct cfr_enh_metadata *meta;
	void *mu_rx_user_info = NULL, *freeze_tlv = NULL;
	u8 *peer_macaddr;
	u8 *data;
	u32 buf_id;
	u32 length;
	u32 freeze_tlv_len = 0;
	u32 end_magic = ATH12K_CFR_END_MAGIC;
	u8 capture_type;
	int ret = 0;
	int status;

	data = param->data;
	buf_id = param->buf_id;

	memcpy(&dma_hdr, data, sizeof(struct ath12k_cfir_enh_dma_hdr));

	if (dma_hdr.freeze_data_incl) {
		freeze_tlv = data + sizeof(struct ath12k_cfir_enh_dma_hdr);
		capture_type = freeze_reason_to_capture_type(ab, freeze_tlv);
	} else {
		capture_type = CFR_CAPTURE_METHOD_AUTO;
	}

	if (dma_hdr.mu_rx_data_incl) {
		if (dma_hdr.freeze_tlv_version == MACRX_FREEZE_TLV_VERSION_5)
			freeze_tlv_len =
				sizeof(struct macrx_freeze_capture_channel_v5);
		else if (dma_hdr.freeze_tlv_version == MACRX_FREEZE_TLV_VERSION_3)
			freeze_tlv_len =
				sizeof(struct macrx_freeze_capture_channel_v3);
		else
			freeze_tlv_len = sizeof(struct macrx_freeze_capture_channel);

		mu_rx_user_info = data + sizeof(struct ath12k_cfir_enh_dma_hdr) +
				  freeze_tlv_len;
	}

	length = dma_hdr.length * 4;
	length += dma_hdr.total_bytes;

	spin_lock_bh(&cfr->lut_lock);

	if (!cfr->lut) {
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}

	lut = &cfr->lut[buf_id];
	if (!lut) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "lut failure to process cfr data id:%d\n", buf_id);
		spin_unlock_bh(&cfr->lut_lock);
		return -EINVAL;
	}


	ath12k_dbg_dump(ab, ATH12K_DBG_CFR_DUMP,"data_from_buf_rel:", "",
			data, length);

	lut->buff = param->buff;
	lut->data = data;
	lut->data_len = length;
	lut->dbr_ppdu_id = dma_hdr.phy_ppdu_id;
	lut->dbr_tstamp = jiffies;
	lut->header_length = dma_hdr.length;
	lut->payload_length = dma_hdr.total_bytes;
	memcpy(&lut->dma_hdr.enh_hdr, &dma_hdr,
	       sizeof(struct ath12k_cfir_enh_dma_hdr));

	header = &lut->header;
	meta = &header->u.meta_enh;
	meta->channel_bw = dma_hdr.upload_pkt_bw;
	meta->num_rx_chain =
		NUM_CHAINS_FW_TO_HOST(dma_hdr.num_chains);
	meta->length = length;

	if (capture_type != CFR_CAPTURE_METHOD_ACK_RESP_TO_TM_FTM) {
		meta->capture_type = capture_type;
		meta->sts_count = dma_hdr.nss + 1;
		if (!dma_hdr.mu_rx_data_incl) {
			peer_macaddr = meta->peer_addr.su_peer_addr;
			if (dma_hdr.freeze_data_incl)
				extract_peer_mac_from_freeze_tlv(freeze_tlv,
								 peer_macaddr);
		}
	}

	status = ath12k_cfr_correlate_and_relay(ar, lut,
						ATH12K_CORRELATE_DBR_EVENT);

	if (status == ATH12K_CORRELATE_STATUS_RELEASE) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "releasing CFR data to user space");
		ath12k_cfr_rfs_write(ar, &lut->header,
				sizeof(struct ath12k_csi_cfr_header),
				lut->data, lut->data_len,
				&end_magic, sizeof(u32));
		ath12k_cfr_release_lut_entry(lut);
		ret = ATH12K_CORRELATE_STATUS_RELEASE;
	} else if (status == ATH12K_CORRELATE_STATUS_HOLD) {
		ret = ATH12K_CORRELATE_STATUS_HOLD;
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "tx event is not yet received holding the buf");
	} else {
		ath12k_cfr_release_lut_entry(lut);
		ret = ATH12K_CORRELATE_STATUS_ERR;
		ath12k_err(ab, "error in processing buf rel event");
	}

	spin_unlock_bh(&cfr->lut_lock);

	return ret;
}

int ath12k_process_cfr_capture_event(struct ath12k_base *ab,
				     struct ath12k_cfr_peer_tx_param *params)
{
	struct ath12k *ar;
	struct ath12k_cfr *cfr;
	struct ath12k_link_vif *arvif;
	struct ath12k_cfr_look_up_table *lut = NULL, *temp = NULL;
	struct ath12k_dbring_element *buff;
	struct ath12k_csi_cfr_header *header;
	dma_addr_t buf_addr;
	u32 end_magic = ATH12K_CFR_END_MAGIC;
	u8 tx_status;
	int ret = 0;
	int status;
	int i;

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, params->vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "Failed to get arvif for vdev id %d\n",
			    params->vdev_id);
		rcu_read_unlock();
		return -ENOENT;
	}

	ar = arvif->ar;
	cfr = &ar->cfr;
	rcu_read_unlock();

	if (WMI_CFR_CAPTURE_STATUS_PEER_PS & params->status) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "CFR capture failed as peer %pM is in powersave",
			   params->peer_mac_addr);
		return -EINVAL;
	}

	if (!(WMI_CFR_PEER_CAPTURE_STATUS & params->status)) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "CFR capture failed for the peer : %pM",
			   params->peer_mac_addr);
		cfr->tx_peer_status_cfr_fail++;
		return -EINVAL;
	}

	tx_status = FIELD_GET(WMI_CFR_FRAME_TX_STATUS, params->status);

	if (tx_status != WMI_FRAME_TX_STATUS_OK) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
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
		ath12k_dbg(ab, ATH12K_DBG_CFR,
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
	header->start_magic_num = ATH12K_CFR_START_MAGIC;
	header->vendorid = VENDOR_QCA;
	header->pltform_type = PLATFORM_TYPE_ARM;

	ab->hw_params->hw_ops->fill_cfr_hdr_info(ar, header, params);

	status = ath12k_cfr_correlate_and_relay(ar, lut,
						ATH12K_CORRELATE_TX_EVENT);
	if (status == ATH12K_CORRELATE_STATUS_RELEASE) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "Releasing CFR data to user space");
		ath12k_cfr_rfs_write(ar, &lut->header,
				     sizeof(struct ath12k_csi_cfr_header),
				     lut->data, lut->data_len,
				     &end_magic, sizeof(u32));
		buff = lut->buff;
		ath12k_cfr_release_lut_entry(lut);

		ath12k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
					     WMI_DIRECT_BUF_CFR, GFP_ATOMIC);
	} else if (status == ATH12K_CORRELATE_STATUS_HOLD) {
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "dbr event is not yet received holding buf\n");
	} else {
		buff = lut->buff;
		ath12k_cfr_release_lut_entry(lut);
		ath12k_dbring_bufs_replenish(ar, &cfr->rx_ring, buff,
					     WMI_DIRECT_BUF_CFR, GFP_ATOMIC);
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

static ssize_t ath12k_read_file_enable_cfr(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	char buf[32] = {0};
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%d\n", ar->cfr.cfr_enabled);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_file_enable_cfr(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	bool enable_cfr = false;
	int ret;

	if (kstrtobool_from_user(ubuf, count, &enable_cfr))
		return -EINVAL;

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (ar->cfr.cfr_enabled == enable_cfr) {
		ret = count;
		goto out;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_PER_PEER_CFR_ENABLE,
					enable_cfr, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Failed to enable/disable per peer cfr (%d)\n",
			    ret);
		goto out;
	}

	ar->cfr.cfr_enabled = enable_cfr;
	ret = count;
out:
	return ret;
}

static const struct file_operations fops_enable_cfr = {
	.read = ath12k_read_file_enable_cfr,
	.write = ath12k_write_file_enable_cfr,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_write_file_cfr_unassoc(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	char buf[64] = {0};
	u8 peer_mac[6];
	u32 cfr_capture_enable;
	u32 cfr_capture_period;
	int available_idx = -1;
	int ret, i;

	simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);

	spin_lock_bh(&cfr->lock);

	if (ar->ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (!ar->cfr.cfr_enabled) {
		ret = -EINVAL;
		ath12k_err(ar->ab, "CFR is not enabled on this pdev %d\n",
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
		for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
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


	if (cfr->cfr_enabled_peer_cnt >= ATH12K_MAX_CFR_ENABLED_CLIENTS) {
		ath12k_info(ar->ab, "Max cfr peer threshold reached\n");
		ret = count;
		goto out;
	}

	for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];

		if ((available_idx < 0) && !entry->is_valid)
			available_idx = i;

		if (ether_addr_equal(peer_mac, entry->peer_mac)) {
			ath12k_info(ar->ab,
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
	return ret;
}

static ssize_t ath12k_read_file_cfr_unassoc(struct file *file,
					    char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	char buf[512] = {0};
	struct ath12k *ar = file->private_data;
	struct ath12k_cfr *cfr = &ar->cfr;
	struct cfr_unassoc_pool_entry *entry;
	int len = 0, i;

	spin_lock_bh(&cfr->lock);

	for (i = 0; i < ATH12K_MAX_CFR_ENABLED_CLIENTS; i++) {
		entry = &cfr->unassoc_pool[i];
		if (entry->is_valid)
			len += scnprintf(buf + len, sizeof(buf) - len,
					 "peer: %pM period: %u\n",
					 entry->peer_mac, entry->period);
	}

	spin_unlock_bh(&cfr->lock);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_configure_cfr_unassoc = {
	.write = ath12k_write_file_cfr_unassoc,
	.read = ath12k_read_file_cfr_unassoc,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static inline void ath12k_cfr_debug_unregister(struct ath12k *ar)
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

static inline int ath12k_cfr_debug_register(struct ath12k *ar)
{
	int ret;

	ar->cfr.rfs_cfr_capture = relay_open("cfr_capture",
					     ar->debug.debugfs_pdev,
					     ar->ab->hw_params->cfr_stream_buf_size,
					     ar->ab->hw_params->cfr_num_stream_bufs,
					     &rfs_cfr_capture_cb, NULL);
	if (!ar->cfr.rfs_cfr_capture) {
		ath12k_warn(ar->ab, "failed to open relay for cfr in pdev %d\n",
			    ar->pdev_idx);
		return -EINVAL;
	}

	ar->cfr.enable_cfr = debugfs_create_file("enable_cfr", 0600,
						 ar->debug.debugfs_pdev, ar,
						 &fops_enable_cfr);
	if (!ar->cfr.enable_cfr) {
		ath12k_warn(ar->ab, "failed to open debugfs in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	ar->cfr.cfr_unassoc = debugfs_create_file("cfr_unassoc", 0600,
						  ar->debug.debugfs_pdev, ar,
						  &fops_configure_cfr_unassoc);

	if (!ar->cfr.cfr_unassoc) {
		ath12k_warn(ar->ab,
			    "failed to open debugfs for unassoc pool in pdev %d\n",
			    ar->pdev_idx);
		ret = -EINVAL;
		goto debug_unregister;
	}

	return 0;

debug_unregister :
	ath12k_cfr_debug_unregister(ar);
	return ret;
}

static int ath12k_cfr_ring_alloc(struct ath12k *ar,
				 struct ath12k_dbring_cap *db_cap)
{
	struct ath12k_cfr *cfr = &ar->cfr;
	int ret;

	ret = ath12k_dbring_srng_setup(ar, &cfr->rx_ring,
				       1, db_cap->min_elem);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring\n");
		return ret;
	}

	ath12k_dbring_set_cfg(ar, &cfr->rx_ring,
			      ATH12K_CFR_NUM_RESP_PER_EVENT,
			      ATH12K_CFR_EVENT_TIMEOUT_MS,
			      ath12k_cfr_enh_process_data);

	ret = ath12k_dbring_buf_setup(ar, &cfr->rx_ring, db_cap);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring buffer\n");
		goto srng_cleanup;
	}

	ret = ath12k_dbring_wmi_cfg_setup(ar, &cfr->rx_ring, WMI_DIRECT_BUF_CFR);
	if (ret) {
		ath12k_warn(ar->ab, "failed to setup db ring cfg\n");
		goto buffer_cleanup;
	}

	return 0;

buffer_cleanup:
	ath12k_dbring_buf_cleanup(ar, &cfr->rx_ring);
srng_cleanup:
	ath12k_dbring_srng_cleanup(ar, &cfr->rx_ring);
	return ret;
}

void ath12k_cfr_ring_free(struct ath12k *ar)
{
	struct ath12k_cfr *cfr = &ar->cfr;

	ath12k_dbring_srng_cleanup(ar, &cfr->rx_ring);
	ath12k_dbring_buf_cleanup(ar, &cfr->rx_ring);
}

void ath12k_cfr_deinit(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_cfr *cfr;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_CFR_CAPTURE_SUPPORT, ab->wmi_ab.svc_map) ||
	    !ab->hw_params->cfr_support)
		return;

	for (i = 0; i <  ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		cfr = &ar->cfr;

		spin_lock_bh(&cfr->lut_lock);
		if (cfr->lut) {
			kfree(cfr->lut);
			cfr->lut = NULL;
		}
		spin_unlock_bh(&cfr->lut_lock);

		ar->cfr.cfr_enabled = 0;
		ath12k_cfr_debug_unregister(ar);
		ath12k_cfr_ring_free(ar);
	}
}

int ath12k_cfr_init(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_cfr *cfr;
	struct ath12k_dbring_cap db_cap;
	struct ath12k_cfr_look_up_table *lut;
	u32 num_lut_entries;
	int ret = 0;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_CFR_CAPTURE_SUPPORT, ab->wmi_ab.svc_map) ||
	    !ab->hw_params->cfr_support)
		return ret;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		cfr = &ar->cfr;

		ret = ath12k_dbring_get_cap(ar->ab, ar->pdev_idx,
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
			ath12k_warn(ab, "failed to allocate lut for pdev %d\n", i);
			return -ENOMEM;
		}

		ret = ath12k_cfr_ring_alloc(ar, &db_cap);
		if (ret) {
			ath12k_warn(ab, "failed to init cfr ring for pdev %d\n", i);
			goto deinit;
		}

		spin_lock_bh(&cfr->lock);
		cfr->lut_num = num_lut_entries;
		spin_unlock_bh(&cfr->lock);

		ret = ath12k_cfr_debug_register(ar);
		if (ret) {
			ath12k_warn(ab, "failed to register cfr for pdev %d\n", i);
			goto deinit;
		}
	}
	return 0;

deinit:
	ath12k_cfr_deinit(ab);
	return ret;
}
