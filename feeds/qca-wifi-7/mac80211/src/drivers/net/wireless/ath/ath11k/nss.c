// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */
#include <linux/of.h>

#include "mac.h"
#include "nss.h"
#include "debug_nss.h"
#include "core.h"
#include "peer.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "hif.h"
#include "wmi.h"
#include "../../../../../net/mac80211/sta_info.h"

enum nss_wifi_mesh_mpp_learning_mode mpp_mode = NSS_WIFI_MESH_MPP_LEARNING_MODE_INDEPENDENT_NSS;
LIST_HEAD(mesh_vaps);

/*-----------------------------ATH11K-NSS Helpers--------------------------*/

static enum ath11k_nss_opmode
ath11k_nss_get_vdev_opmode(struct ath11k_vif *arvif)
{
	switch (arvif->vdev_type) {
	case WMI_VDEV_TYPE_AP:
		return ATH11K_NSS_OPMODE_AP;
	case WMI_VDEV_TYPE_STA:
		return ATH11K_NSS_OPMODE_STA;
	default:
		ath11k_warn(arvif->ar->ab, "unsupported nss vdev type %d\n",
			    arvif->vdev_type);
	}

	return ATH11K_NSS_OPMODE_UNKNOWN;
}

static struct ath11k_vif *ath11k_nss_get_arvif_from_dev(struct net_device *dev)
{
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif;

	if (!dev)
		return NULL;

	wdev = dev->ieee80211_ptr;
	if (!wdev)
		return NULL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return NULL;

	arvif = (struct ath11k_vif *)vif->drv_priv;
	if (!arvif)
		return NULL;

	return arvif;
}

static void ath11k_nss_wifili_stats_sync(struct ath11k_base *ab,
					 struct nss_wifili_stats_sync_msg *wlsoc_stats)
{
	struct nss_wifili_device_stats *devstats = &wlsoc_stats->stats;
	struct ath11k_soc_dp_stats *soc_stats = &ab->soc_stats;
	int i;

	spin_lock_bh(&ab->base_lock);

	soc_stats->err_ring_pkts += devstats->rxwbm_stats.err_src_rxdma;
	soc_stats->invalid_rbm += devstats->rxwbm_stats.invalid_buf_mgr;

	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		soc_stats->rxdma_error[i] += devstats->rxwbm_stats.err_dma_codes[i];

	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		soc_stats->reo_error[i] += devstats->rxwbm_stats.err_reo_codes[i];

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		soc_stats->hal_reo_error[i] += devstats->rxreo_stats[i].ring_error;

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		soc_stats->tx_err.desc_na[i] += devstats->tcl_stats[i].tcl_ring_full;


	for (i = 0; i < NSS_WIFILI_MAX_TCL_DATA_RINGS_MSG; i++)
		atomic_add(devstats->txcomp_stats[i].invalid_bufsrc
			  + devstats->txcomp_stats[i].invalid_cookie
			  + devstats->tx_sw_pool_stats[i].desc_alloc_fail
			  + devstats->tx_ext_sw_pool_stats[i].desc_alloc_fail,
			  &soc_stats->tx_err.misc_fail);

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		atomic_add(devstats->tx_data_stats[i].tx_send_fail_cnt,
			   &soc_stats->tx_err.misc_fail);

	spin_unlock_bh(&ab->base_lock);
}

static void ath11k_nss_wifili_link_desc_set(struct ath11k_base *ab, void *desc,
					    struct ath11k_buffer_addr *buf_addr_info,
					    enum hal_wbm_rel_bm_act action)
{
	struct hal_wbm_release_ring *dst_desc = desc;

	dst_desc->buf_addr_info = *buf_addr_info;
	dst_desc->info0 |= FIELD_PREP(HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE,
				      HAL_WBM_REL_SRC_MODULE_SW) |
			   FIELD_PREP(HAL_WBM_RELEASE_INFO0_BM_ACTION, action) |
			   FIELD_PREP(HAL_WBM_RELEASE_INFO0_DESC_TYPE,
				      HAL_WBM_REL_DESC_TYPE_MSDU_LINK);
}

static void ath11k_nss_wifili_link_desc_return(struct ath11k_base *ab,
					       struct ath11k_buffer_addr *buf_addr_info)
{
	struct ath11k_dp *dp = &ab->dp;
	struct hal_srng *srng;
	u32 *desc;

	srng = &ab->hal.srng_list[dp->wbm_desc_rel_ring.ring_id];
	spin_lock_bh(&srng->lock);

	ath11k_hal_srng_access_begin(ab, srng);
	desc = ath11k_hal_srng_src_get_next_entry(ab, srng);

	if (!desc)
		goto exit;

	ath11k_nss_wifili_link_desc_set(ab, desc, buf_addr_info, HAL_WBM_REL_BM_ACT_PUT_IN_IDLE);

exit:
	ath11k_hal_srng_access_end(ab, srng);
	spin_unlock(&srng->lock);
}

static void ath11k_nss_get_peer_stats(struct ath11k_base *ab, struct nss_wifili_peer_stats *stats)
{
	struct ath11k_peer *peer;
	struct nss_wifili_peer_ctrl_stats *pstats = NULL;
	int i, j;
	u64 tx_packets, tx_bytes, tx_dropped = 0;
	u64 rx_packets, rx_bytes, rx_dropped;

	if (!ab->nss.stats_enabled)
		return;

	for (i = 0; i < stats->npeers; i++) {
		pstats = &stats->wpcs[i];

		rcu_read_lock();
		spin_lock_bh(&ab->base_lock);

		peer = ath11k_peer_find_by_id(ab, pstats->peer_id);
		if (!peer || !peer->sta) {
			ath11k_dbg(ab, ATH11K_DBG_NSS, "nss wifili: unable to find peer %d\n", pstats->peer_id);
			spin_unlock_bh(&ab->base_lock);
			rcu_read_unlock();
			continue;
		}

		if (!peer->nss.nss_stats) {
			spin_unlock_bh(&ab->base_lock);
			rcu_read_unlock();
			return;
		}

		if (pstats->tx.tx_success_cnt)
			peer->nss.nss_stats->last_ack = jiffies;

		if (pstats->rx.rx_recvd) {
			peer->nss.nss_stats->last_rx = jiffies;
		}

		tx_packets = pstats->tx.tx_mcast_cnt +
			pstats->tx.tx_ucast_cnt +
			pstats->tx.tx_bcast_cnt;
		peer->nss.nss_stats->tx_packets += tx_packets;
		tx_bytes = pstats->tx.tx_mcast_bytes +
			pstats->tx.tx_ucast_bytes +
			pstats->tx.tx_bcast_bytes;
		peer->nss.nss_stats->tx_bytes += tx_bytes;
		peer->nss.nss_stats->tx_retries += pstats->tx.retries;

		for (j = 0; j < NSS_WIFILI_TQM_RR_MAX; j++)
			tx_dropped += pstats->tx.dropped.drop_stats[j];

		peer->nss.nss_stats->tx_failed += tx_dropped;

		if (peer->nss.ext_vdev_up)
			ATH11K_NSS_TXRX_NETDEV_STATS(tx, peer->nss.ext_vif, tx_bytes, tx_packets);
		else
			ATH11K_NSS_TXRX_NETDEV_STATS(tx, peer->vif, tx_bytes, tx_packets);

		rx_packets = pstats->rx.rx_recvd;
		peer->nss.nss_stats->rx_packets += rx_packets;
		rx_bytes = pstats->rx.rx_recvd_bytes;
		peer->nss.nss_stats->rx_bytes += rx_bytes;
		rx_dropped = pstats->rx.err.mic_err +
			pstats->rx.err.decrypt_err;
		peer->nss.nss_stats->rx_dropped += rx_dropped;

		if (peer->nss.ext_vdev_up)
			ATH11K_NSS_TXRX_NETDEV_STATS(tx, peer->nss.ext_vif, tx_bytes, tx_packets);
		else
			ATH11K_NSS_TXRX_NETDEV_STATS(rx, peer->vif, rx_bytes, rx_packets);

		spin_unlock_bh(&ab->base_lock);
		rcu_read_unlock();
	}
}

void ath11k_nss_ext_rx_stats(struct ath11k_base *ab, struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (ab->nss.enabled)
		tlv_filter->rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS;
}

static u32 ath11k_nss_cipher_type(struct ath11k_base *ab, u32 cipher)
{
	switch (cipher) {
	case WLAN_CIPHER_SUITE_CCMP:
		return PEER_SEC_TYPE_AES_CCMP;
	case WLAN_CIPHER_SUITE_TKIP:
		return PEER_SEC_TYPE_TKIP;
	case WLAN_CIPHER_SUITE_CCMP_256:
		return PEER_SEC_TYPE_AES_CCMP_256;
	case WLAN_CIPHER_SUITE_GCMP:
		return PEER_SEC_TYPE_AES_GCMP;
	case WLAN_CIPHER_SUITE_GCMP_256:
		return PEER_SEC_TYPE_AES_GCMP_256;
	default:
		ath11k_warn(ab, "unknown cipher type %d\n", cipher);
		return PEER_SEC_TYPE_NONE;
	}
}

static void ath11k_nss_tx_encap_nwifi(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (void *)skb->data;
	u8 *qos_ctl;

	if (!ieee80211_is_data_qos(hdr->frame_control))
		return;

	qos_ctl = ieee80211_get_qos_ctl(hdr);
	memmove(skb->data + IEEE80211_QOS_CTL_LEN,
		skb->data, (void *)qos_ctl - (void *)skb->data);
	skb_pull(skb, IEEE80211_QOS_CTL_LEN);

	hdr = (void *)skb->data;
	hdr->frame_control &= ~__cpu_to_le16(IEEE80211_STYPE_QOS_DATA);
}

static void ath11k_nss_tx_encap_raw(struct sk_buff *skb)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (void *)skb->data;
	u32 cipher;

	if (!ieee80211_has_protected(hdr->frame_control) || !info->control.hw_key)
		return;

	/* Include length for MIC */
	skb_put(skb, IEEE80211_CCMP_MIC_LEN);

	/* Include length for ICV if TKIP is used */
	cipher = info->control.hw_key->cipher;
	if (cipher == WLAN_CIPHER_SUITE_TKIP)
		skb_put(skb, IEEE80211_TKIP_ICV_LEN);
}

static void ath11k_nss_peer_mem_free(struct ath11k_base *ab, u32 peer_id)
{
	struct ath11k_peer *peer;

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_by_id(ab, peer_id);
	if (!peer) {
		spin_unlock_bh(&ab->base_lock);
		ath11k_warn(ab, "ath11k_nss: unable to free peer mem, peer_id:%d\n",
			    peer_id);
		return;
	}

	dma_unmap_single(ab->dev, peer->nss.paddr,
			 WIFILI_NSS_PEER_BYTE_SIZE, DMA_FROM_DEVICE);

	kfree(peer->nss.vaddr);
	if (peer->nss.nss_stats) {
		kfree(peer->nss.nss_stats);
		peer->nss.nss_stats = NULL;
	}

	complete(&peer->nss.complete);
	spin_unlock_bh(&ab->base_lock);

	ath11k_dbg(ab, ATH11K_DBG_NSS, "nss peer %d mem freed\n", peer_id);
}

/*-----------------------------Events/Callbacks------------------------------*/

void ath11k_nss_wifili_event_receive(struct ath11k_base *ab, struct nss_wifili_msg *msg)
{
	u32 msg_type = msg->cm.type;
	enum nss_cmn_response response = msg->cm.response;
	u32 error =  msg->cm.error;
	u32 peer_id;
	struct nss_wifili_peer_stats *peer_stats;

	if (!ab)
		return;

	ath11k_dbg(ab, ATH11K_DBG_NSS, "nss wifili event received %d response %d error %d\n",
		   msg_type, response, error);

	switch (msg_type) {
	case NSS_WIFILI_INIT_MSG:
		ab->nss.response = response;
		complete(&ab->nss.complete);
		break;
	case NSS_WIFILI_PDEV_INIT_MSG:
	case NSS_WIFILI_START_MSG:
	case NSS_WIFILI_SOC_RESET_MSG:
	case NSS_WIFILI_STOP_MSG:
	case NSS_WIFILI_PDEV_DEINIT_MSG:
		ab->nss.response = response;
		complete(&ab->nss.complete);
		break;
	case NSS_WIFILI_PEER_CREATE_MSG:
		if (response != NSS_CMN_RESPONSE_EMSG)
			break;

		peer_id = (&msg->msg.peermsg)->peer_id;

		/* free peer memory allocated during peer create due to failure */
		ath11k_nss_peer_mem_free(ab, peer_id);
		break;
	case NSS_WIFILI_PEER_DELETE_MSG:
		peer_id = (&msg->msg.peermsg)->peer_id;

		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab, "ath11k_nss: peer delete failed%d\n",
				    peer_id);

		/* free peer memory allocated during peer create irrespective of
		 * delete status
		 */
		ath11k_nss_peer_mem_free(ab, peer_id);
		break;
	case NSS_WIFILI_PEER_SECURITY_TYPE_MSG:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab, "peer securty config failed\n");

		break;
	case NSS_WIFILI_PEER_UPDATE_AUTH_FLAG:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab, "peer authorize config failed\n");

		break;
	case NSS_WIFILI_STATS_MSG:
		if (response == NSS_CMN_RESPONSE_EMSG) {
			ath11k_warn(ab, "soc_dp_stats failed to get updated\n");
			break;
		}
		ath11k_nss_wifili_stats_sync(ab, &msg->msg.wlsoc_stats);
		break;
	case NSS_WIFILI_PEER_STATS_MSG:
		peer_stats = &msg->msg.peer_stats.stats;
		if (response == NSS_CMN_RESPONSE_EMSG) {
			ath11k_warn(ab, "peer stats msg failed with error = %u\n", error);
			break;
		}
		ath11k_nss_get_peer_stats(ab, peer_stats);
		break;
	case NSS_WIFILI_TID_REOQ_SETUP_MSG:
		/* TODO setup tidq */
		break;
	case NSS_WIFILI_WDS_PEER_ADD_MSG:
		ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "nss wifili wds peer add event received %d response %d error %d\n",
			   msg_type, response, error);
		break;
	case NSS_WIFILI_WDS_PEER_UPDATE_MSG:
		ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "nss wifili wds peer update event received %d response %d error %d\n",
			   msg_type, response, error);
		break;
	case NSS_WIFILI_WDS_PEER_MAP_MSG:
		ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "nss wifili wds peer map event received %d response %d error %d\n",
			   msg_type, response, error);
		break;
	case NSS_WIFILI_WDS_PEER_DEL_MSG:
		ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "nss wifili wds peer del event received %d response %d error %d\n",
			   msg_type, response, error);
		break;
	case NSS_WIFILI_PEER_4ADDR_EVENT_MSG:
		ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "nss wifili peer 4addr event received %d response %d error %d\n",
			   msg_type, response, error);
		break;
	case NSS_WIFILI_SEND_MESH_CAPABILITY_INFO:
		complete(&ab->nss.complete);
		if (response != NSS_CMN_RESPONSE_EMSG)
			ab->nss.mesh_nss_offload_enabled = true;
		ath11k_dbg(ab, ATH11K_DBG_NSS_MESH, "nss wifili mesh capability response %d\n",
			   ab->nss.mesh_nss_offload_enabled);
		break;
	case NSS_WIFILI_LINK_DESC_INFO_MSG:
		ath11k_nss_wifili_link_desc_return(ab,
						   (void *)&msg->msg.linkdescinfomsg);
		break;
	default:
		ath11k_dbg(ab, ATH11K_DBG_NSS, "unhandled event %d\n", msg_type);
		break;
	}
}

void ath11k_nss_process_mic_error(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct ath11k_vif *arvif;
	struct ath11k_peer *peer = NULL;
	struct hal_rx_desc *desc = (struct hal_rx_desc *)skb->data;
	struct wireless_dev *wdev;
	u16 peer_id;
	u8 peer_addr[ETH_ALEN];
	u8 ucast_keyidx, mcast_keyidx;
	bool is_mcbc;

	if (!ath11k_dp_rx_h_msdu_end_first_msdu(ab, desc))
		goto fail;

	is_mcbc = ath11k_dp_rx_h_attn_is_mcbc(ab, desc);
	peer_id = ath11k_dp_rx_h_mpdu_start_peer_id(ab, desc);

	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find_by_id(ab, peer_id);
	if (!peer) {
		ath11k_info(ab, "ath11k_nss:peer not found");
		spin_unlock_bh(&ab->base_lock);
		goto fail;
	}

	if (!peer->vif) {
		ath11k_warn(ab, "ath11k_nss:vif not found");
		spin_unlock_bh(&ab->base_lock);
		goto fail;
	}

	ether_addr_copy(peer_addr, peer->addr);
	mcast_keyidx = peer->mcast_keyidx;
	ucast_keyidx = peer->ucast_keyidx;
	arvif = ath11k_vif_to_arvif(peer->vif);
	spin_unlock_bh(&ab->base_lock);

	if (!arvif->is_started) {
		ath11k_warn(ab, "ath11k_nss:arvif not started");
		goto fail;
	}

	wdev = ieee80211_vif_to_wdev_relaxed(arvif->vif);
	if (!wdev) {
		ath11k_warn(ab, "ath11k_nss: wdev is null\n");
		goto fail;
	}

	if (!wdev->netdev) {
		ath11k_warn(ab, "ath11k_nss: netdev is null\n");
		goto fail;
	}

	cfg80211_michael_mic_failure(wdev->netdev, peer_addr,
				     is_mcbc ? NL80211_KEYTYPE_GROUP :
				     NL80211_KEYTYPE_PAIRWISE,
				     is_mcbc ? mcast_keyidx : ucast_keyidx,
				     NULL, GFP_ATOMIC);
	dev_kfree_skb_any(skb);
	return;

fail:
	dev_kfree_skb_any(skb);
	ath11k_warn(ab, "ath11k_nss: Failed to handle mic error\n");
	return;
}

static void
ath11k_nss_wifili_ext_callback_fn(struct ath11k_base *ab, struct sk_buff *skb,
				  __attribute__((unused)) struct napi_struct *napi)
{
	struct nss_wifili_soc_per_packet_metadata *wepm;

	wepm = (struct nss_wifili_soc_per_packet_metadata *)(skb->head +
					      NSS_WIFILI_SOC_PER_PACKET_METADATA_OFFSET);

	switch (wepm->pkt_type) {
	case NSS_WIFILI_SOC_EXT_DATA_PKT_MIC_ERROR:
		ath11k_nss_process_mic_error(ab, skb);
		break;
	default:
		ath11k_dbg(ab, ATH11K_DBG_NSS, "unknown packet type received in wifili ext cb %d",
			    wepm->pkt_type);
		dev_kfree_skb_any(skb);
		break;
	}
}

void ath11k_nss_vdev_cfg_cb(void *app_data, struct nss_cmn_msg *msg)
{
	struct ath11k_vif *arvif = (struct ath11k_vif *)app_data;

	if (!arvif)
		return;

	ath11k_dbg(arvif->ar->ab, ATH11K_DBG_NSS, "vdev cfg msg callback received msg:%d rsp:%d\n",
		   msg->type, msg->response);

	complete(&arvif->nss.complete);
}

static void ath11k_nss_vdev_event_receive(void *dev, struct nss_cmn_msg *vdev_msg)
{
	/*TODO*/
}

/* TODO: move to mac80211 after cleanups/refactoring required after feature completion */
static int ath11k_nss_deliver_rx(struct ieee80211_vif *vif, struct sk_buff *skb,
				 bool eth,  int data_offs, struct napi_struct *napi)
{
	struct sk_buff_head subframe_list;
	struct ieee80211_hdr *hdr;
	struct sk_buff *subframe;
	struct net_device *dev;
	int hdr_len;
	u8 *qc;

	dev = skb->dev;

	if (eth)
		goto deliver_msdu;

	hdr = (struct ieee80211_hdr *)skb->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);

	if (ieee80211_is_data_qos(hdr->frame_control)) {
		qc = ieee80211_get_qos_ctl(hdr);
		if (*qc & IEEE80211_QOS_CTL_A_MSDU_PRESENT)
			goto deliver_amsdu;
	}

	if (ieee80211_data_to_8023_exthdr(skb, NULL, vif->addr, vif->type,
					  data_offs - hdr_len, false)) {
		dev_kfree_skb_any(skb);
		return -EINVAL;
	}

deliver_msdu:
	skb->protocol = eth_type_trans(skb, dev);
	napi_gro_receive(napi, skb);
	return 0;

deliver_amsdu:
	/* Move to the start of the first subframe */
	skb_pull(skb, data_offs);

	__skb_queue_head_init(&subframe_list);

	/* create list containing all the subframes */
	ieee80211_amsdu_to_8023s(skb, &subframe_list, NULL,
				 vif->type, 0, NULL, NULL);

	/* This shouldn't happen, indicating error during defragmentation */
	if (skb_queue_empty(&subframe_list))
		return -EINVAL;

	while (!skb_queue_empty(&subframe_list)) {
		subframe = __skb_dequeue(&subframe_list);
		subframe->protocol = eth_type_trans(subframe, dev);
		napi_gro_receive(napi, subframe);
	}

	return 0;
}

static int ath11k_nss_undecap_raw(struct ath11k_vif *arvif, struct sk_buff *skb,
				  int *data_offset)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct ath11k *ar = arvif->ar;
	enum hal_encrypt_type enctype;
	struct ath11k_peer *peer = NULL;
	struct ieee80211_hdr *hdr;
	int hdr_len;

	hdr = (struct ieee80211_hdr *)skb->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);

	*data_offset = hdr_len;

	/* FCS is included in the raw mode skb, we can trim it, fcs error
	 * packets are not expected to be received in this path
	 */
	skb_trim(skb, skb->len - FCS_LEN);

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_by_addr(ab, hdr->addr2);
	if (!peer) {
		ath11k_warn(ab, "peer not found for raw/nwifi undecap, drop this packet\n");
		spin_unlock_bh(&ab->base_lock);
		return -EINVAL;
	}
	enctype = peer->sec_type;

	spin_unlock_bh(&ab->base_lock);

	*data_offset += ath11k_dp_rx_crypto_param_len(ar, enctype);

	/* Strip ICV, MIC, MMIC */
	skb_trim(skb, skb->len -
		 ath11k_dp_rx_crypto_mic_len(ar, enctype));

	skb_trim(skb, skb->len -
		 ath11k_dp_rx_crypto_icv_len(ar, enctype));

	if (enctype == HAL_ENCRYPT_TYPE_TKIP_MIC)
		skb_trim(skb, skb->len - IEEE80211_CCMP_MIC_LEN);

	return 0;
}

static int ath11k_nss_undecap_nwifi(struct ath11k_vif *arvif, struct sk_buff *skb,
				    int *data_offset)
{
	struct ieee80211_hdr *hdr;
	int hdr_len;

	hdr = (struct ieee80211_hdr *)skb->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);

	*data_offset = hdr_len;

	/* We dont receive the IV from nss host on slow path
	 * hence we can return only the header length as offset.
	 **/
	return 0;
}

static void ath11k_nss_wds_type_rx(struct ath11k *ar, struct net_device *dev,
				   u8* src_mac, u8 is_sa_valid, u8 addr4_valid,
				   u16 peer_id)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_ast_entry *ast_entry = NULL;
	struct ath11k_peer *ta_peer = NULL;

	spin_lock_bh(&ab->base_lock);
	ta_peer = ath11k_peer_find_by_id(ab, peer_id);

	if (!ta_peer) {
		spin_unlock_bh(&ab->base_lock);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,"ath11k_nss_wds_type_rx ta_peer %pM\n",
		   ta_peer->addr);

	if (addr4_valid) {
		ast_entry = ath11k_peer_ast_find_by_addr(ab, src_mac);
		if (!is_sa_valid) {
			ath11k_peer_add_ast(ar, ta_peer, src_mac,
					    ATH11K_AST_TYPE_WDS);
			if (!ta_peer->nss.ext_vdev_up)
				ieee80211_rx_nss_notify_4addr(dev, ta_peer->addr);
		} else {
			if (!ast_entry) {
				ath11k_peer_add_ast(ar, ta_peer, src_mac,
						    ATH11K_AST_TYPE_WDS);
				if (!ta_peer->nss.ext_vdev_up)
					ieee80211_rx_nss_notify_4addr(dev, ta_peer->addr);
			} else if (ast_entry->type == ATH11K_AST_TYPE_WDS) {
				ath11k_peer_update_ast(ar, ta_peer, ast_entry);
				ath11k_nss_update_wds_peer(ar, ta_peer, src_mac);
			}
		}
	}

	spin_unlock_bh(&ab->base_lock);
}

static void ath11k_nss_mec_handler(struct ath11k_vif *arvif, u8* mec_mac_addr)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	struct ath11k_peer *peer = ar->bss_peer;
	u8 mac_addr[ETH_ALEN];
	u32 *mac_addr_l32;
	u16 *mac_addr_h16;

	if (!peer)
		return;

	/* mec_mac_addr has the swapped mac_addr after 4 bytes (sizeof(u32))
	 * mec_mac_addr[0]
	 * |
	 * 03:0a:00:00:2d:15:22:f0:fd:8c
	 *		^
	 * 		Swapped MAC address present after 4 bytes
	 * MAC address after swapping is 8c:fd:f0:22:15:2d */

	mac_addr_l32 = (u32 *) (mec_mac_addr + sizeof(u32));
	mac_addr_h16 = (u16 *) (mec_mac_addr + sizeof(u32) + sizeof(u32));

	*mac_addr_l32 = swab32(*mac_addr_l32);
	*mac_addr_h16 = swab16(*mac_addr_h16);

	memcpy(mac_addr, mac_addr_h16, ETH_ALEN - 4);
	memcpy(mac_addr + 2, mac_addr_l32, 4);

	if (!ether_addr_equal(arvif->vif->addr, mac_addr)) {
		spin_lock_bh(&ab->base_lock);
		ath11k_peer_add_ast(ar, peer, mac_addr,
				    ATH11K_AST_TYPE_MEC);
		spin_unlock_bh(&ab->base_lock);
	}
}

static void ath11k_nss_vdev_spl_receive_ext_wdsdata(struct ath11k_vif *arvif,
						    struct sk_buff *skb,
						    struct nss_wifi_vdev_wds_per_packet_metadata *wds_metadata)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	enum wifi_vdev_ext_wds_info_type wds_type;
	u8 is_sa_valid = 0, addr4_valid = 0;
	u16 peer_id;
	u8 src_mac[ETH_ALEN];

	is_sa_valid = wds_metadata->is_sa_valid;
	addr4_valid = wds_metadata->addr4_valid;
	wds_type = wds_metadata->wds_type;
	peer_id = wds_metadata->peer_id;

	memcpy(src_mac, ((struct ethhdr *)skb->data)->h_source, ETH_ALEN);

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,"receive_ext_wdsdata wds_type %d peer id %u sa_valid %d addr4_valid %d src_mac %pM\n",
		   wds_type, peer_id, is_sa_valid, addr4_valid, src_mac);

	switch (wds_type) {
		case NSS_WIFI_VDEV_WDS_TYPE_RX:
			ath11k_nss_wds_type_rx(ar, skb->dev, src_mac, is_sa_valid,
					       addr4_valid, peer_id);
			break;
		case NSS_WIFI_VDEV_WDS_TYPE_MEC:
			ath11k_nss_mec_handler(arvif, (u8 *)(skb->data));
			break;
		default:
			ath11k_warn(ab, "unsupported wds_type %d\n", wds_type);
			break;
	}
}

static bool ath11k_nss_vdev_data_receive_mec_check(struct ath11k *ar,
						   struct sk_buff *skb)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_ast_entry *ast_entry = NULL;
	u8 src_mac[ETH_ALEN];

	memcpy(src_mac, ((struct ethhdr *)skb->data)->h_source, ETH_ALEN);
	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "ath11k_nss_vdev_data_receive_mec_check src mac %pM\n",
		   src_mac);

	spin_lock_bh(&ab->base_lock);
	ast_entry = ath11k_peer_ast_find_by_addr(ab, src_mac);

	if (ast_entry && ast_entry->type == ATH11K_AST_TYPE_MEC) {
		spin_unlock_bh(&ab->base_lock);
		ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
			   "dropping mec traffic from %pM\n", ast_entry->addr);
		return true;
	}

	spin_unlock_bh(&ab->base_lock);
	return false;
}

static int ath11k_nss_undecap(struct ath11k_vif *arvif, struct sk_buff *skb,
			      int *data_offs, bool *eth_decap)
{
	enum ath11k_hw_txrx_mode decap_type;

	decap_type = arvif->nss.decap;

	switch (decap_type) {
	case ATH11K_HW_TXRX_RAW:
		return ath11k_nss_undecap_raw(arvif, skb, data_offs);
	case ATH11K_HW_TXRX_NATIVE_WIFI:
		return ath11k_nss_undecap_nwifi(arvif, skb, data_offs);
	case ATH11K_HW_TXRX_ETHERNET:
		*eth_decap = true;
		return 0;
	default:
		return -EINVAL;
	}
}

static void
ath11k_nss_vdev_special_data_receive(struct net_device *dev, struct sk_buff *skb,
				     __attribute__((unused)) struct napi_struct *napi)
{
	struct nss_wifi_vdev_per_packet_metadata *wifi_metadata = NULL;
	struct nss_wifi_vdev_wds_per_packet_metadata *wds_metadata = NULL;
	struct nss_wifi_vdev_addr4_data_metadata *addr4_metadata = NULL;
	struct ath11k_vif *arvif;
	struct ath11k_base *ab;
	struct ath11k_skb_rxcb *rxcb;
	bool eth_decap = false;
	int data_offs = 0;
	int ret = 0;
	struct ath11k_peer *ta_peer = NULL;

	arvif = ath11k_nss_get_arvif_from_dev(dev);
	if (!arvif) {
		dev_kfree_skb_any(skb);
		return;
	}

	ab = arvif->ar->ab;

	skb->dev = dev;

	dma_unmap_single(ab->dev, virt_to_phys(skb->head),
			 NSS_WIFI_VDEV_PER_PACKET_METADATA_OFFSET +
			 sizeof(struct nss_wifi_vdev_per_packet_metadata),
			 DMA_FROM_DEVICE);

	wifi_metadata = (struct nss_wifi_vdev_per_packet_metadata *)(skb->head +
			 NSS_WIFI_VDEV_PER_PACKET_METADATA_OFFSET);

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "dp special data from nss: wifi_metadata->pkt_type %d",
		   wifi_metadata->pkt_type);

	ret = ath11k_nss_undecap(arvif, skb, &data_offs, &eth_decap);
	if (ret) {
		ath11k_warn(ab, "error in nss rx undecap, type %d err %d\n",
			    arvif->nss.decap, ret);
		dev_kfree_skb_any(skb);
		return;
	}

	switch(wifi_metadata->pkt_type) {
	case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_WDS_LEARN:
		if (eth_decap) {
			wds_metadata = &wifi_metadata->metadata.wds_metadata;
			ath11k_nss_vdev_spl_receive_ext_wdsdata(arvif, skb,
								wds_metadata);
		}
		dev_kfree_skb_any(skb);
	break;
	case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_MCBC_RX:
		ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "",
			        "mcbc packet exception from nss: ",
			        skb->data, skb->len);
		rxcb = ATH11K_SKB_RXCB(skb);
		rxcb->rx_desc = (struct hal_rx_desc *)skb->head;
		rxcb->is_first_msdu = rxcb->is_last_msdu = true;
		rxcb->is_continuation = false;
		rxcb->is_mcbc = true;
		ath11k_dp_rx_from_nss(arvif->ar, skb, napi);
	break;
	case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_4ADDR:
		if (eth_decap) {
			addr4_metadata = &wifi_metadata->metadata.addr4_metadata;

			spin_lock_bh(&ab->base_lock);
			ta_peer = ath11k_peer_find_by_id(ab, addr4_metadata->peer_id);
			if (!ta_peer) {
				spin_unlock_bh(&ab->base_lock);
				dev_kfree_skb_any(skb);
				return;
			}

			ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "4addr exception ta_peer %pM\n",
				   ta_peer->addr);
			if (!ta_peer->nss.ext_vdev_up && addr4_metadata->addr4_valid)
			    ieee80211_rx_nss_notify_4addr(dev, ta_peer->addr);
			spin_unlock_bh(&ab->base_lock);
		}
		dev_kfree_skb_any(skb);
	break;
	default:
		ath11k_warn(ab, "unsupported pkt_type %d from nss\n", wifi_metadata->pkt_type);
		dev_kfree_skb_any(skb);
	}
}

static void
ath11k_nss_vdev_data_receive(struct net_device *dev, struct sk_buff *skb,
			     __attribute__((unused)) struct napi_struct *napi)
{
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif;
	struct ath11k_base *ab;
	bool eth_decap = false;
	int data_offs = 0;
	int ret;

	if (!dev) {
		dev_kfree_skb_any(skb);
		return;
	}

	wdev = dev->ieee80211_ptr;
	if (!wdev) {
		dev_kfree_skb_any(skb);
		return;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		dev_kfree_skb_any(skb);
		return;
	}

	arvif = (struct ath11k_vif *)vif->drv_priv;
	if (!arvif) {
		dev_kfree_skb_any(skb);
		return;
	}

	ab = arvif->ar->ab;

	skb->dev = dev;

	/* log the original skb received from nss */
	ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "", "dp rx msdu from nss: ",
			skb->data, skb->len);

	if ((vif->type == NL80211_IFTYPE_STATION && wdev->use_4addr) &&
	     ath11k_nss_vdev_data_receive_mec_check(arvif->ar, skb)) {
		dev_kfree_skb_any(skb);
		return;
	}

	ret = ath11k_nss_undecap(arvif, skb, &data_offs, &eth_decap);
	if (ret) {
		ath11k_warn(ab, "error in nss rx undecap, type %d err %d\n",
			    arvif->nss.decap, ret);
		dev_kfree_skb_any(skb);
		return;
	}

	ath11k_nss_deliver_rx(arvif->vif, skb, eth_decap, data_offs, napi);
}

static void
ath11k_nss_ext_vdev_special_data_receive(struct net_device *dev,
					 struct sk_buff *skb,
					 __attribute__((unused)) struct napi_struct *napi)
{
	dev_kfree_skb_any(skb);
}

static void
ath11k_nss_ext_vdev_data_receive(struct net_device *dev, struct sk_buff *skb,
				 __attribute__((unused)) struct napi_struct *napi)
{
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif;
	struct ath11k_base *ab;
	bool eth_decap = false;
	int data_offs = 0;
	int ret;

	if (!dev) {
		dev_kfree_skb_any(skb);
		return;
	}

	wdev = dev->ieee80211_ptr;
	if (!wdev) {
		dev_kfree_skb_any(skb);
		return;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		dev_kfree_skb_any(skb);
		return;
	}

	arvif = (struct ath11k_vif *)vif->drv_priv;
	if (!arvif) {
		dev_kfree_skb_any(skb);
		return;
	}

	ab = arvif->ar->ab;

	skb->dev = dev;

	/* log the original skb received from nss */
	ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "", "dp rx msdu from nss ext : ",
			skb->data, skb->len);

	ret = ath11k_nss_undecap(arvif, skb, &data_offs, &eth_decap);
	if (ret) {
		ath11k_warn(ab, "error in nss ext rx undecap, type %d err %d\n",
			    arvif->nss.decap, ret);
		dev_kfree_skb_any(skb);
		return;
	}

	ath11k_nss_deliver_rx(arvif->vif, skb, eth_decap, data_offs, napi);
}

/*------Mesh offload------*/

void ath11k_nss_mesh_wifili_event_receive(void *app_data,
					  struct nss_cmn_msg *cmn_msg)
{
	struct nss_wifi_mesh_msg *msg = (struct nss_wifi_mesh_msg *)cmn_msg;
	struct ath11k_base *ab = app_data;
	u32 msg_type = msg->cm.type;
	enum nss_cmn_response response = msg->cm.response;
	u32 error =  msg->cm.error;

	if (!ab)
		return;

	ath11k_dbg(ab, ATH11K_DBG_NSS_MESH, "nss mesh event received %d response %d error %d\n",
		   msg_type, response, error);

	switch (msg_type) {
	case NSS_WIFI_MESH_MSG_MPATH_ADD:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab,"failed to add an entry to mpath table mesh_da %pM vdev_id %d\n",
				    (&msg->msg.mpath_add)->dest_mac_addr,
				    (&msg->msg.mpath_add)->link_vap_id);
		break;
	case NSS_WIFI_MESH_MSG_MPATH_UPDATE:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab, "failed to update mpath entry mesh_da %pM vdev_id %d"
				    "next_hop %pM old_next_hop %pM metric %d flags 0x%u hop_count %d"
				    "exp_time %u mesh_gate %u\n",
				    (&msg->msg.mpath_update)->dest_mac_addr,
				    (&msg->msg.mpath_update)->link_vap_id,
				    (&msg->msg.mpath_update)->next_hop_mac_addr,
				    (&msg->msg.mpath_update)->old_next_hop_mac_addr,
				    (&msg->msg.mpath_update)->metric,
				    (&msg->msg.mpath_update)->path_flags,
				    (&msg->msg.mpath_update)->hop_count,
				    (&msg->msg.mpath_update)->expiry_time,
				    (&msg->msg.mpath_update)->is_mesh_gate);
		break;
	case NSS_WIFI_MESH_MSG_MPATH_DELETE:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab,"failed to remove mpath entry mesh_da %pM"
				    "vdev_id %d\n",
				    (&msg->msg.mpath_del)->mesh_dest_mac_addr,
				    (&msg->msg.mpath_del)->link_vap_id);
		break;
	case NSS_WIFI_MESH_MSG_PROXY_PATH_ADD:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab,"failed to add proxy entry da %pM mesh_da %pM \n",
				    (&msg->msg.proxy_add_msg)->dest_mac_addr,
				    (&msg->msg.proxy_add_msg)->mesh_dest_mac);
		break;
	case NSS_WIFI_MESH_MSG_PROXY_PATH_UPDATE:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab,"failed to update proxy path da %pM mesh_da %pM\n",
				    (&msg->msg.proxy_update_msg)->dest_mac_addr,
				    (&msg->msg.proxy_update_msg)->mesh_dest_mac);
		break;
	case NSS_WIFI_MESH_MSG_PROXY_PATH_DELETE:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab,"failed to remove proxy path entry da %pM mesh_da %pM\n",
				    (&msg->msg.proxy_del_msg)->dest_mac_addr,
				    (&msg->msg.proxy_del_msg)->mesh_dest_mac_addr);
		break;
	case NSS_WIFI_MESH_MSG_EXCEPTION_FLAG:
		if (response == NSS_CMN_RESPONSE_EMSG)
			ath11k_warn(ab,"failed to add the exception  da %pM\n",
				    (&msg->msg.exception_msg)->dest_mac_addr);
		break;
	default:
		ath11k_dbg(ab, ATH11K_DBG_NSS_MESH, "unhandled event %d\n", msg_type);
		break;
	}
}

static void nss_mesh_convert_path_flags(u16 *dest, u16 *src, bool to_nss)
{
	if (to_nss) {
		if (*src & IEEE80211_MESH_PATH_ACTIVE)
			*dest |= NSS_WIFI_MESH_PATH_FLAG_ACTIVE;
		if (*src & IEEE80211_MESH_PATH_RESOLVING)
			*dest |= NSS_WIFI_MESH_PATH_FLAG_RESOLVING;
		if (*src & IEEE80211_MESH_PATH_RESOLVED)
			*dest |= NSS_WIFI_MESH_PATH_FLAG_RESOLVED;
		if (*src & IEEE80211_MESH_PATH_FIXED)
			*dest |= NSS_WIFI_MESH_PATH_FLAG_FIXED;
	} else {
		if (*src & NSS_WIFI_MESH_PATH_FLAG_ACTIVE)
			*dest |= IEEE80211_MESH_PATH_ACTIVE;
		if (*src & NSS_WIFI_MESH_PATH_FLAG_RESOLVING)
			*dest |= IEEE80211_MESH_PATH_RESOLVING;
		if (*src & NSS_WIFI_MESH_PATH_FLAG_RESOLVED)
			*dest |= IEEE80211_MESH_PATH_RESOLVED;
		if (*src & NSS_WIFI_MESH_PATH_FLAG_FIXED)
			*dest |= IEEE80211_MESH_PATH_FIXED;
	}
}

static void ath11k_nss_mesh_mpath_refresh(struct ath11k_vif *arvif,
					  struct nss_wifi_mesh_msg *msg)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_path_refresh_msg *refresh_msg;
	struct ieee80211_mesh_path_offld path = {0};
	int ret;

	refresh_msg = &msg->msg.path_refresh_msg;
	ether_addr_copy(path.mesh_da, refresh_msg->dest_mac_addr);
	ether_addr_copy(path.next_hop, refresh_msg->next_hop_mac_addr);

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "Mesh path refresh event from nss, mDA %pM next_hop %pM link_vdev %d\n",
		   refresh_msg->dest_mac_addr, refresh_msg->next_hop_mac_addr,
		   refresh_msg->link_vap_id);


	if (ab->nss.debug_mode)
		return;

	ret = ieee80211_mesh_path_offld_change_notify(arvif->vif, &path,
			IEEE80211_MESH_PATH_OFFLD_ACTION_MPATH_REFRESH);
	if (ret)
		ath11k_warn(ab, "failed to notify mpath refresh nss event %d\n", ret);
}

static void ath11k_nss_mesh_path_not_found(struct ath11k_vif *arvif,
					   struct nss_wifi_mesh_msg *msg)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_mpath_not_found_msg *err_msg;
	struct ieee80211_mesh_path_offld path = {0};
	int ret;

	err_msg = &msg->msg.mpath_not_found_msg;
	ether_addr_copy(path.da, err_msg->dest_mac_addr);
	if (err_msg->is_mesh_forward_path)
		ether_addr_copy(path.ta, err_msg->transmitter_mac_addr);

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "Mesh path not found event from nss, (m)DA %pM ta %pM link vap %d\n",
		   err_msg->dest_mac_addr, err_msg->transmitter_mac_addr, err_msg->link_vap_id);


	if (ab->nss.debug_mode)
		return;

	ret = ieee80211_mesh_path_offld_change_notify(arvif->vif, &path,
			IEEE80211_MESH_PATH_OFFLD_ACTION_PATH_NOT_FOUND);
	if (ret)
		ath11k_warn(ab, "failed to notify mpath not found nss event %d\n", ret);
}

static void ath11k_nss_mesh_path_delete(struct ath11k_vif *arvif,
					struct nss_wifi_mesh_msg *msg)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_mpath_del_msg *del_msg = &msg->msg.mpath_del;
	struct ieee80211_mesh_path_offld path = {0};
	int ret;

	ether_addr_copy(path.mesh_da, del_msg->mesh_dest_mac_addr);

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "Mesh path delete event from nss, mDA %pM vap_id %d\n",
		   del_msg->mesh_dest_mac_addr, del_msg->link_vap_id);

	ret = ieee80211_mesh_path_offld_change_notify(arvif->vif, &path,
			IEEE80211_MESH_PATH_OFFLD_ACTION_MPATH_DEL);
	if (ret)
		ath11k_warn(ab, "failed to notify mpath delete nss event %d\n", ret);
}

static void ath11k_nss_mesh_path_expiry(struct ath11k_vif *arvif,
					struct nss_wifi_mesh_msg *msg)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_path_expiry_msg *exp_msg = &msg->msg.path_expiry_msg;
	struct ieee80211_mesh_path_offld path = {0};
	int ret;

	ether_addr_copy(path.mesh_da, exp_msg->mesh_dest_mac_addr);
	ether_addr_copy(path.next_hop, exp_msg->next_hop_mac_addr);

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "Mesh path delete event from nss, mDA %pM next_hop %pM if_num %d\n",
		   exp_msg->mesh_dest_mac_addr, exp_msg->next_hop_mac_addr,
		   arvif->nss.if_num);

	ret = ieee80211_mesh_path_offld_change_notify(arvif->vif, &path,
			IEEE80211_MESH_PATH_OFFLD_ACTION_MPATH_EXP);
	if (ret)
		ath11k_warn(ab, "failed to notify mpath expiry nss event %d\n", ret);
}

static void ath11k_nss_mesh_mpp_learn(struct ath11k_vif *arvif,
				      struct nss_wifi_mesh_msg *msg)
 {
	 struct ath11k_base *ab = arvif->ar->ab;
	 struct nss_wifi_mesh_proxy_path_learn_msg *learn_msg;
	 struct ieee80211_mesh_path_offld path = {0};
	 int ret;

	 learn_msg = &msg->msg.proxy_learn_msg;

	 ether_addr_copy(path.mesh_da, learn_msg->mesh_dest_mac);
	 ether_addr_copy(path.da, learn_msg->dest_mac_addr);
	 nss_mesh_convert_path_flags(&path.flags, &learn_msg->path_flags, false);

	 ath11k_dbg(ab, ATH11K_DBG_NSS,
		    "Mesh proxy learn event from nss, mDA %pM da %pM flags 0x%x if_num %d\n",
		    learn_msg->mesh_dest_mac, learn_msg->dest_mac_addr,
		    learn_msg->path_flags, arvif->nss.if_num);

	 ret = ieee80211_mesh_path_offld_change_notify(arvif->vif, &path,
			 IEEE80211_MESH_PATH_OFFLD_ACTION_MPP_LEARN);
	 if (ret)
		 ath11k_warn(ab, "failed to notify proxy learn event %d\n", ret);
}

static void ath11k_nss_mesh_mpp_add(struct ath11k_vif *arvif,
				    struct nss_wifi_mesh_msg *msg)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_proxy_path_add_msg *add_msg = &msg->msg.proxy_add_msg;
	struct ieee80211_mesh_path_offld path = {0};
	int ret;

	ether_addr_copy(path.mesh_da, add_msg->mesh_dest_mac);
	ether_addr_copy(path.da, add_msg->dest_mac_addr);
	nss_mesh_convert_path_flags(&path.flags, &add_msg->path_flags, false);

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "Mesh proxy add event from nss, mDA %pM da %pM flags 0x%x if_num %d\n",
		   add_msg->mesh_dest_mac, add_msg->dest_mac_addr, add_msg->path_flags,
		   arvif->nss.if_num);

	ret = ieee80211_mesh_path_offld_change_notify(arvif->vif, &path,
				IEEE80211_MESH_PATH_OFFLD_ACTION_MPP_ADD);
	if (ret)
		ath11k_warn(ab, "failed to notify proxy add event %d\n", ret);
}

static void ath11k_nss_mesh_mpp_update(struct ath11k_vif *arvif,
				       struct nss_wifi_mesh_msg *msg)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_proxy_path_update_msg *umsg;
	struct ieee80211_mesh_path_offld path = {0};
	int ret;

	umsg = &msg->msg.proxy_update_msg;
	ether_addr_copy(path.mesh_da, umsg->mesh_dest_mac);
	ether_addr_copy(path.da, umsg->dest_mac_addr);
	nss_mesh_convert_path_flags(&path.flags, &umsg->path_flags, false);

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "Mesh proxy update event from nss, mDA %pM da %pM flags 0x%x if_num %d\n",
		   umsg->mesh_dest_mac, umsg->dest_mac_addr, umsg->path_flags, arvif->nss.if_num);

	ret = ieee80211_mesh_path_offld_change_notify(arvif->vif, &path,
				IEEE80211_MESH_PATH_OFFLD_ACTION_MPP_UPDATE);
	if (ret)
		ath11k_warn(ab, "failed to notify proxy update event %d\n", ret);
}

static int
ath11k_nss_mesh_process_path_table_dump_msg(struct ath11k_vif *arvif,
				     struct nss_wifi_mesh_msg *msg)
{
	struct nss_wifi_mesh_path_table_dump *mpath_dump = &msg->msg.mpath_table_dump;
	struct ath11k_nss_mpath_entry *entry;
	struct ath11k *ar = arvif->ar;
	ssize_t len;

	len = sizeof(struct nss_wifi_mesh_path_dump_entry) * mpath_dump->num_entries;
	entry = kzalloc(sizeof(*entry) + len, GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;

	memcpy(entry->mpath, mpath_dump->path_entry, len);
	entry->num_entries = mpath_dump->num_entries;
	spin_lock_bh(&ar->nss.dump_lock);
	list_add_tail(&entry->list, &arvif->nss.mpath_dump);
	arvif->nss.mpath_dump_num_entries += mpath_dump->num_entries;
	spin_unlock_bh(&ar->nss.dump_lock);

	if (!mpath_dump->more_events)
		complete(&arvif->nss.dump_mpath_complete);

	return 0;
}

static int
ath11k_nss_mesh_process_mpp_table_dump_msg(struct ath11k_vif *arvif,
				    struct nss_wifi_mesh_msg *msg)
{
	struct nss_wifi_mesh_proxy_path_table_dump *mpp_dump;
	struct ath11k_nss_mpp_entry *entry, *tmp;
	struct ath11k *ar = arvif->ar;
	struct arvif_nss *nss = &arvif->nss;
	ssize_t len;
	LIST_HEAD(local_entry_exp_update);

	mpp_dump = &msg->msg.proxy_path_table_dump;

	if (!mpp_dump->num_entries)
		return 0;

	len = sizeof(struct nss_wifi_mesh_proxy_path_dump_entry) * mpp_dump->num_entries;
	entry = kzalloc(sizeof(*entry) + len, GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;

	memcpy(entry->mpp, mpp_dump->path_entry, len);
	entry->num_entries = mpp_dump->num_entries;
	spin_lock_bh(&ar->nss.dump_lock);
	list_add_tail(&entry->list, &arvif->nss.mpp_dump);
	arvif->nss.mpp_dump_num_entries += mpp_dump->num_entries;
	spin_unlock_bh(&ar->nss.dump_lock);

	if (!mpp_dump->more_events) {
		if (arvif->nss.mpp_aging) {
			arvif->nss.mpp_aging = false;
			spin_lock_bh(&ar->nss.dump_lock);
			list_splice_tail_init(&nss->mpp_dump, &local_entry_exp_update);
			spin_unlock_bh(&ar->nss.dump_lock);

			list_for_each_entry_safe(entry, tmp, &local_entry_exp_update, list) {
				if (entry->mpp->time_diff > ATH11K_MPP_EXPIRY_TIMER_INTERVAL_MS)
					continue;
				mesh_nss_offld_proxy_path_exp_update(arvif->vif,
								     entry->mpp->dest_mac_addr,
								     entry->mpp->mesh_dest_mac,
								     entry->mpp->time_diff);
			}
			/* If mpp_dump_req is true dont free the entry
			 * since it will get freed in debug_nss_fill_mpp_dump
			 * both mpp_aging and mpp_dump_req will be true during
			 * simultaneous accessing of mpp dump entry. So this will
			 * gain the reuse of same dump result for both mpp_aging
			 * and mpp_dump_req */
			if (!arvif->nss.mpp_dump_req) {
				list_for_each_entry_safe(entry, tmp, &local_entry_exp_update, list)
					kfree(entry);
			} else {
				/* Adding back to global nss dump tbl to reuse the same
				 * tbl for mpp dump request
				 */
				spin_lock_bh(&ar->nss.dump_lock);
				list_splice_tail_init(&local_entry_exp_update, &nss->mpp_dump);
				spin_unlock_bh(&ar->nss.dump_lock);
			}
		}

		if (arvif->nss.mpp_dump_req) {
			complete(&arvif->nss.dump_mpp_complete);
			arvif->nss.mpp_dump_req = false;
		}
	}

	return 0;
}

int ath11k_nss_mesh_exception_flags(struct ath11k_vif *arvif,
			       struct nss_wifi_mesh_exception_flag_msg *nss_msg)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret = 0;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	status = nss_wifi_meshmgr_mesh_path_exception(arvif->nss.mesh_handle, nss_msg,
			msg_cb, arvif->ar->ab);

	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(arvif->ar->ab, "failed to set the exception flags\n");
		ret = -EINVAL;
	}

	return ret;
}

int ath11k_nss_exc_rate_config(struct ath11k_vif *arvif,
					struct nss_wifi_mesh_rate_limit_config *nss_exc_cfg)
{
	nss_tx_status_t status;
	int ret = 0;

	status = nss_wifi_meshmgr_config_mesh_exception_sync(arvif->nss.mesh_handle, nss_exc_cfg);

	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(arvif->ar->ab, "failed to set the exception rate ctrl\n");
		ret = -EINVAL;
	}

	return ret;
}

static void ath11k_nss_mesh_obj_vdev_event_receive(struct net_device *dev,
						   struct nss_wifi_mesh_msg *msg)
{
	struct ath11k_base *ab;
	struct ath11k_vif *arvif;
	int ret;

	arvif = ath11k_nss_get_arvif_from_dev(dev);
	if (!arvif)
		return;

	ab = arvif->ar->ab;

	switch (msg->cm.type) {
	case NSS_WIFI_MESH_MSG_PATH_REFRESH:
		ath11k_nss_mesh_mpath_refresh(arvif, msg);
		break;
	case NSS_WIFI_MESH_MSG_PATH_NOT_FOUND:
		ath11k_nss_mesh_path_not_found(arvif, msg);
		break;
	case NSS_WIFI_MESH_MSG_MPATH_DELETE:
		ath11k_nss_mesh_path_delete(arvif, msg);
		break;
	case NSS_WIFI_MESH_MSG_PATH_EXPIRY:
		ath11k_nss_mesh_path_expiry(arvif, msg);
		break;
	case NSS_WIFI_MESH_MSG_PROXY_PATH_LEARN:
		ath11k_nss_mesh_mpp_learn(arvif, msg);
		break;
	case NSS_WIFI_MESH_MSG_PROXY_PATH_ADD:
		ath11k_nss_mesh_mpp_add(arvif, msg);
		break;
	case NSS_WIFI_MESH_MSG_PROXY_PATH_UPDATE:
		ath11k_nss_mesh_mpp_update(arvif, msg);
		break;
	case NSS_WIFI_MESH_MSG_PATH_TABLE_DUMP:
		ret = ath11k_nss_mesh_process_path_table_dump_msg(arvif, msg);
		if (ret)
			ath11k_warn(arvif->ar->ab, "failed mpath table dump message %d\n",
				    ret);
		break;
	case NSS_WIFI_MESH_MSG_PROXY_PATH_TABLE_DUMP:
		ret = ath11k_nss_mesh_process_mpp_table_dump_msg(arvif, msg);
		if (ret)
			ath11k_warn(arvif->ar->ab, "failed mpp table dump message %d\n",
				    ret);
		break;
	default:
		ath11k_dbg(ab, ATH11K_DBG_NSS, "unknown message type on mesh obj vap %d\n",
			   msg->cm.type);
		break;
	}
}

static int ath11k_nss_mesh_mpath_add(struct ath11k_vif *arvif,
			      struct ieee80211_mesh_path_offld *path)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	struct nss_wifi_mesh_mpath_add_msg *msg;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_MESH, "add mpath for mesh_da %pM on radio %d\n",
		   path->mesh_da, ar->pdev->pdev_id);

	msg = kzalloc(sizeof(struct nss_wifi_mesh_mpath_add_msg), GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	ether_addr_copy(msg->dest_mac_addr, path->mesh_da);
	ether_addr_copy(msg->next_hop_mac_addr, path->next_hop);
	msg->hop_count = path->hop_count;
	msg->metric = path->metric;
	nss_mesh_convert_path_flags(&msg->path_flags, &path->flags, true);
	msg->link_vap_id = arvif->nss.if_num;
	msg->block_mesh_fwd = path->block_mesh_fwd;
	msg->metadata_type = path->metadata_type ? NSS_WIFI_MESH_PRE_HEADER_80211: NSS_WIFI_MESH_PRE_HEADER_NONE;

	status = nss_wifi_meshmgr_mesh_path_add(arvif->nss.mesh_handle, msg,
					        msg_cb, ar->ab);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab,
			    "failed to add mpath entry mesh_da %pM radio_id %d status %d\n",
			    path->mesh_da, arvif->nss.if_num, status);
		ret = -EINVAL;
	}

	kfree(msg);

	return ret;
}

static int ath11k_nss_mesh_mpath_update(struct ath11k_vif *arvif,
			      struct ieee80211_mesh_path_offld *path)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	struct nss_wifi_mesh_mpath_update_msg *msg;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_MESH,
		   "update mpath  mesh_da %pM radio %d next_hop %pM old_next_hop %pM "
		   "metric %d flags 0x%x hop_count %d "
		   "exp_time %lu mesh_gate %d\n",
		   path->mesh_da, ar->pdev->pdev_id, path->next_hop, path->old_next_hop,
		   path->metric, path->flags, path->hop_count, path->exp_time,
		   path->mesh_gate);

	msg = kzalloc(sizeof(struct nss_wifi_mesh_mpath_update_msg), GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	ether_addr_copy(msg->dest_mac_addr, path->mesh_da);
	ether_addr_copy(msg->next_hop_mac_addr, path->next_hop);
	ether_addr_copy(msg->old_next_hop_mac_addr, path->old_next_hop);
	msg->hop_count = path->hop_count;
	msg->metric = path->metric;
	nss_mesh_convert_path_flags(&msg->path_flags, &path->flags, true);
	msg->link_vap_id = arvif->nss.if_num;
	msg->is_mesh_gate = path->mesh_gate;
	msg->expiry_time = path->exp_time;
	msg->block_mesh_fwd = path->block_mesh_fwd;
	msg->metadata_type = path->metadata_type ? NSS_WIFI_MESH_PRE_HEADER_80211: NSS_WIFI_MESH_PRE_HEADER_NONE;

	msg->update_flags = NSS_WIFI_MESH_PATH_UPDATE_FLAG_NEXTHOP |
		     NSS_WIFI_MESH_PATH_UPDATE_FLAG_HOPCOUNT |
		     NSS_WIFI_MESH_PATH_UPDATE_FLAG_METRIC |
		     NSS_WIFI_MESH_PATH_UPDATE_FLAG_MESH_FLAGS |
		     NSS_WIFI_MESH_PATH_UPDATE_FLAG_BLOCK_MESH_FWD |
		     NSS_WIFI_MESH_PATH_UPDATE_FLAG_METADATA_ENABLE_VALID;

	status = nss_wifi_meshmgr_mesh_path_update(arvif->nss.mesh_handle, msg,
						    msg_cb, ar->ab);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab,
			    "failed to update mpath entry mesh_da %pM radio_id %d status %d\n",
			    path->mesh_da, arvif->nss.if_num, status);
		ret = -EINVAL;
	}

	kfree(msg);

	return ret;
}

static int ath11k_nss_mesh_mpath_del(struct ath11k_vif *arvif,
			      struct ieee80211_mesh_path_offld *path)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	struct nss_wifi_mesh_mpath_del_msg *msg;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_MESH, "del mpath for mesh_da %pM on radio %d\n",
		   path->mesh_da, ar->pdev->pdev_id);

	msg = kzalloc(sizeof(struct nss_wifi_mesh_mpath_del_msg), GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	ether_addr_copy(msg->mesh_dest_mac_addr, path->mesh_da);
	ether_addr_copy(msg->next_hop_mac_addr, path->next_hop);
	msg->link_vap_id = arvif->nss.if_num;

	status = nss_wifi_meshmgr_mesh_path_delete(arvif->nss.mesh_handle,
						   msg, msg_cb, ar->ab);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab,
			    "failed to del mpath entry mesh_da %pM radio_id %d status %d\n",
			    path->mesh_da, arvif->nss.if_num, status);
		ret = -EINVAL;
	}

	kfree(msg);

	return ret;
}

static int ath11k_nss_mesh_mpp_add_cmd(struct ath11k_vif *arvif,
			      struct ieee80211_mesh_path_offld *path)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	struct nss_wifi_mesh_proxy_path_add_msg *msg;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_MESH, "add mpp mesh_da %pM da %pM\n",
		   path->mesh_da, path->da);

	msg = kzalloc(sizeof(struct nss_wifi_mesh_proxy_path_add_msg), GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	ether_addr_copy(msg->dest_mac_addr, path->da);
	ether_addr_copy(msg->mesh_dest_mac, path->mesh_da);
	nss_mesh_convert_path_flags(&msg->path_flags, &path->flags, true);

	status = nss_wifi_meshmgr_mesh_proxy_path_add(arvif->nss.mesh_handle,
						      msg, msg_cb, ar->ab);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab,
			    "failed to add mpp entry da %pM mesh_da %pM status %d\n",
			    path->da, path->mesh_da, status);
		ret = -EINVAL;
	}

	kfree(msg);

	return ret;
}

static int ath11k_nss_mesh_mpp_update_cmd(struct ath11k_vif *arvif,
			      struct ieee80211_mesh_path_offld *path)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	struct nss_wifi_mesh_proxy_path_update_msg *msg;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_MESH, "update mpp da %pM mesh_da %pM on vap_id %d\n",
		   path->da, path->mesh_da, arvif->nss.if_num);

	msg = kzalloc(sizeof(struct nss_wifi_mesh_proxy_path_update_msg), GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	ether_addr_copy(msg->dest_mac_addr, path->da);
	ether_addr_copy(msg->mesh_dest_mac, path->mesh_da);
	nss_mesh_convert_path_flags(&msg->path_flags, &path->flags, true);
	msg->bitmap = NSS_WIFI_MESH_PATH_UPDATE_FLAG_NEXTHOP |
			NSS_WIFI_MESH_PATH_UPDATE_FLAG_HOPCOUNT;

	status = nss_wifi_meshmgr_mesh_proxy_path_update(arvif->nss.mesh_handle,
							 msg, msg_cb, ar->ab);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab,
			    "failed to update mpp da %pM mesh_da %pM status %d\n",
			    path->da, path->mesh_da, status);
		ret = -EINVAL;
	}

	kfree(msg);

	return ret;
}

static int ath11k_nss_mesh_mpp_del_cmd(struct ath11k_vif *arvif,
			      struct ieee80211_mesh_path_offld *path)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	struct nss_wifi_mesh_proxy_path_del_msg *msg;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_MESH, "del mpath for mesh_da %pM\n",
		   path->mesh_da);

	msg = kzalloc(sizeof(struct nss_wifi_mesh_proxy_path_del_msg), GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	ether_addr_copy(msg->dest_mac_addr, path->da);
	ether_addr_copy(msg->mesh_dest_mac_addr, path->mesh_da);

	status = nss_wifi_meshmgr_mesh_proxy_path_delete(arvif->nss.mesh_handle, msg,
							 msg_cb, ar->ab);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab,
			    "failed to add mpath entry mesh_da %pM status %d\n",
			    path->mesh_da, status);
		ret = -EINVAL;
	}

	kfree(msg);

	return ret;
}

int ath11k_nss_mesh_config_path(struct ath11k *ar, struct ath11k_vif *arvif,
				enum ieee80211_mesh_path_offld_cmd cmd,
				struct ieee80211_mesh_path_offld *path)
{
	int ret;


	if (!ar->ab->nss.enabled)
		return 0;

	switch (cmd) {
	case IEEE80211_MESH_PATH_OFFLD_CMD_ADD_MPATH:
		ret = ath11k_nss_mesh_mpath_add(arvif, path);
		break;
	case IEEE80211_MESH_PATH_OFFLD_CMD_UPDATE_MPATH:
		ret = ath11k_nss_mesh_mpath_update(arvif, path);
		break;
	case IEEE80211_MESH_PATH_OFFLD_CMD_DELETE_MPATH:
		ret = ath11k_nss_mesh_mpath_del(arvif, path);
		break;
	case IEEE80211_MESH_PATH_OFFLD_CMD_ADD_MPP:
		ret = ath11k_nss_mesh_mpp_add_cmd(arvif, path);
		break;
	case IEEE80211_MESH_PATH_OFFLD_CMD_UPDATE_MPP:
		ret = ath11k_nss_mesh_mpp_update_cmd(arvif, path);
		break;
	case IEEE80211_MESH_PATH_OFFLD_CMD_DELETE_MPP:
		ret = ath11k_nss_mesh_mpp_del_cmd(arvif, path);
		break;
	default:
		ath11k_warn(ar->ab, "unknown mesh path table command type %d\n", cmd);
		return -EINVAL;
	}

	return ret;
}

int ath11k_nss_mesh_config_update(struct ieee80211_vif *vif, int changed)
{
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_config_msg *nss_msg;
	struct arvif_nss *nss = &arvif->nss;
	nss_tx_status_t status;
	int ret = 0;

	if (!ab->nss.enabled)
		return 0;

	if (!ab->nss.mesh_nss_offload_enabled)
		return -ENOTSUPP;

	if (!changed)
		return 0;

	nss_msg = kzalloc(sizeof(*nss_msg), GFP_KERNEL);
	if (!nss_msg)
		return -ENOMEM;

	if (changed & BSS_CHANGED_NSS_MESH_TTL) {
		nss_msg->ttl = vif->bss_conf.nss_offld_ttl;
		nss->mesh_ttl = vif->bss_conf.nss_offld_ttl;
		nss_msg->config_flags |= NSS_WIFI_MESH_CONFIG_FLAG_TTL_VALID;
	}

	if (changed & BSS_CHANGED_NSS_MESH_REFRESH_TIME) {
		nss_msg->mesh_path_refresh_time =
			vif->bss_conf.nss_offld_mpath_refresh_time;
		nss->mpath_refresh_time =
			vif->bss_conf.nss_offld_mpath_refresh_time;
		nss_msg->config_flags |= NSS_WIFI_MESH_CONFIG_FLAG_MPATH_REFRESH_VALID;
	}

	if (changed & BSS_CHANGED_NSS_MESH_FWD_ENABLED) {
		nss_msg->block_mesh_forwarding =
			vif->bss_conf.nss_offld_mesh_forward_enabled;
		nss->mesh_forward_enabled =
			vif->bss_conf.nss_offld_mesh_forward_enabled;
		nss_msg->config_flags |= NSS_WIFI_MESH_CONFIG_FLAG_BLOCK_MESH_FWD_VALID;
		nss_msg->metadata_type = arvif->nss.metadata_type;
		nss_msg->config_flags |= NSS_WIFI_MESH_CONFIG_FLAG_METADATA_ENABLE_VALID;
	}

	status = nss_wifi_meshmgr_mesh_config_update_sync(arvif->nss.mesh_handle,
							  nss_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "failed to configure nss mesh obj vdev nss_err:%d\n",
				status);
		ret = -EINVAL;
	}

	kfree(nss_msg);

	return ret;
}

int ath11k_nss_dump_mpath_request(struct ath11k_vif *arvif)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct ath11k *ar = arvif->ar;
	struct arvif_nss *nss = &arvif->nss;
	struct ath11k_nss_mpath_entry *entry, *tmp;
	LIST_HEAD(local_entry);
	nss_tx_status_t status;

	/* Clean up any stale entries from old events */
	spin_lock_bh(&ar->nss.dump_lock);
	list_splice_tail(&nss->mpath_dump, &local_entry);
	arvif->nss.mpath_dump_num_entries = 0;
	spin_unlock_bh(&ar->nss.dump_lock);

	list_for_each_entry_safe(entry, tmp, &local_entry, list)
		kfree(entry);

	status = nss_wifi_meshmgr_dump_mesh_path_sync(arvif->nss.mesh_handle);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "failed to send mpath dump command on mesh obj vdev nss_err:%d\n",
				status);
		return -EINVAL;
	}

	return 0;
}

int ath11k_nss_dump_mpp_request(struct ath11k_vif *arvif)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct ath11k *ar = arvif->ar;
	struct arvif_nss *nss = &arvif->nss;
	struct ath11k_nss_mpp_entry *entry, *tmp;
	LIST_HEAD(local_entry);
	nss_wifi_meshmgr_status_t status;

	if (!arvif->nss.mpp_aging) {
		/* Clean up any stale entries from old events */
		spin_lock_bh(&ar->nss.dump_lock);
		list_splice_tail_init(&nss->mpp_dump, &local_entry);
		arvif->nss.mpp_dump_num_entries = 0;
		spin_unlock_bh(&ar->nss.dump_lock);

		list_for_each_entry_safe(entry, tmp, &local_entry, list) {
			list_del(&entry->list);
			kfree(entry);
		}
	}

	arvif->nss.mpp_dump_req = true;

	status = nss_wifi_meshmgr_dump_mesh_proxy_path_sync(arvif->nss.mesh_handle);
	if (status != NSS_WIFI_MESHMGR_SUCCESS) {
		if (status == NSS_WIFI_MESHMGR_FAILURE_ONESHOT_ALREADY_ATTACHED)
			return 0;
		ath11k_warn(ab, "failed to send mpp dump command on mesh obj vdev nss_err:%d\n",
			    status);
		return -EINVAL;
	}

	return 0;
}

int ath11k_nss_mpp_timer_cb(struct timer_list *timer)
{
	nss_wifi_mesh_msg_callback_t msg_cb;
	struct arvif_nss *nss = from_timer(nss, timer,mpp_expiry_timer);
	struct ath11k_vif *arvif = container_of(nss, struct ath11k_vif, nss);
	struct ath11k_base *ab = arvif->ar->ab;
	LIST_HEAD(local_entry);
	nss_tx_status_t status;

	msg_cb = (nss_wifi_mesh_msg_callback_t)ath11k_nss_mesh_wifili_event_receive;

	if (!arvif->nss.mpp_dump_req)
		arvif->nss.mpp_dump_num_entries = 0;
	arvif->nss.mpp_aging = true;

	status = nss_wifi_meshmgr_dump_mesh_proxy_path(arvif->nss.mesh_handle, msg_cb, ab);
	if (status != NSS_TX_SUCCESS)
		ath11k_warn(ab, "failed to send mpp dump command from timer nss_err:%d\n",
				status);

	mod_timer(&nss->mpp_expiry_timer,
		 jiffies + msecs_to_jiffies(ATH11K_MPP_EXPIRY_TIMER_INTERVAL_MS));

	return 0;
}

static void
ath11k_nss_mesh_obj_vdev_data_receive(struct net_device *dev, struct sk_buff *skb,
				      struct napi_struct *napi)
{
	struct ath11k_vif *arvif;
	struct ath11k_base *ab;
	char dump_msg[100] = {0};
	struct nss_wifi_mesh_per_packet_metadata *wifi_metadata = NULL;

	arvif = ath11k_nss_get_arvif_from_dev(dev);
	if (!arvif) {
		dev_kfree_skb_any(skb);
		return;
	}

	ab = arvif->ar->ab;

	skb->dev = dev;

	snprintf(dump_msg, sizeof(dump_msg), "nss mesh obj vdev: link id %d ",
		 arvif->nss.if_num);

	ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "dp rx msdu from nss", dump_msg,
			skb->data, skb->len);

	if (arvif->nss.metadata_type == NSS_WIFI_MESH_PRE_HEADER_80211) {
		wifi_metadata = (struct nss_wifi_mesh_per_packet_metadata *)(skb->data -
				(sizeof(struct nss_wifi_mesh_per_packet_metadata)));

		ath11k_dbg(ab, ATH11K_DBG_NSS_MESH,
				"exception from nss on mesh obj vap: pkt_type %d\n",
				wifi_metadata->pkt_type);
		switch (wifi_metadata->pkt_type) {
		case NSS_WIFI_MESH_PRE_HEADER_80211:
			ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "",
					"wifi header from nss on mesh obj vdev: ",
					skb->data - sizeof(*wifi_metadata), sizeof(*wifi_metadata) + skb->len);
			dev_kfree_skb_any(skb);
		break;
		default:
			dev_kfree_skb_any(skb);
		}

		return;
	}

	ath11k_nss_deliver_rx(arvif->vif, skb, true, 0, napi);
}

static void
ath11k_nss_mesh_obj_ext_data_callback(struct net_device *dev, struct sk_buff *skb,
				      __attribute__((unused)) struct napi_struct *napi)
{
	struct ath11k_vif *arvif;
	struct ath11k_base *ab;
	struct nss_wifi_mesh_encap_ext_pkt_metadata *wifi_metadata = NULL;
	int metadata_len;

	arvif = ath11k_nss_get_arvif_from_dev(dev);
	if (!arvif) {
		dev_kfree_skb_any(skb);
		return;
	}

	ab = arvif->ar->ab;

	skb->dev = dev;

	metadata_len = NSS_WIFI_MESH_ENCAP_METADATA_OFFSET_TYPE +
		sizeof(struct nss_wifi_mesh_encap_ext_pkt_metadata);

	/* msdu from nss should contain metadata in headroom
	 * any msdu which has invalid or not contains metadata
	 * will be treated as invalid msdu and dropping it.
	 */
	if (!(metadata_len < skb_headroom(skb))) {
		ath11k_warn(ab, "msdu from nss is having invalid headroom %d\n", skb_headroom(skb));
		dev_kfree_skb_any(skb);
		return;
	}

	dma_unmap_single(ab->dev, virt_to_phys(skb->head),
			metadata_len,
			DMA_FROM_DEVICE);

	wifi_metadata = (struct nss_wifi_mesh_encap_ext_pkt_metadata *)(skb->head +
			NSS_WIFI_MESH_ENCAP_METADATA_OFFSET_TYPE);

	ath11k_dbg(ab, ATH11K_DBG_NSS_MESH, "msdu from nss ext_data _cb on mesh obj vdev");

	switch (wifi_metadata->pkt_type) {
		case NSS_WIFI_MESH_ENCAP_EXT_DATA_PKT_TYPE_MPATH_NOT_FOUND_EXC:
			ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "", "msdu from nss ext_data for mpath not found : ",
					skb->data, skb->len);
			skb->protocol = eth_type_trans(skb, dev);
			skb_reset_network_header(skb);
			dev_queue_xmit(skb);
		break;
		default:
			ath11k_warn(ab, "unknown packet type received in mesh obj ext data %d",
					wifi_metadata->pkt_type);
			dev_kfree_skb_any(skb);
	}
}

static void
ath11k_nss_mesh_link_vdev_data_receive(void *dev,
				       struct sk_buff *skb,
				       struct napi_struct *napi)
{
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif;
	struct ath11k_base *ab;
	struct wireless_dev *wdev = (struct wireless_dev *)dev;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		dev_kfree_skb_any(skb);
		return;
	}

	arvif = (struct ath11k_vif *)vif->drv_priv;
	if (!arvif) {
		dev_kfree_skb_any(skb);
		return;
	}

	ab = arvif->ar->ab;
	ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "", "msdu from nss data_receive_cb on mesh link vdev: ",
			skb->data, skb->len);
	/* data callback for mesh link vap is not expected */
	dev_kfree_skb_any(skb);
}

static void
ath11k_nss_mesh_link_vdev_special_data_receive(void *dev,
				struct sk_buff *skb,
				__attribute__((unused)) struct napi_struct *napi)
{
	struct ieee80211_vif *vif;
	struct ath11k_base *ab;
	struct nss_wifi_vdev_per_packet_metadata *wifi_metadata = NULL;
	struct ath11k_skb_rxcb *rxcb;
	struct ath11k_vif *arvif;
	struct wireless_dev *wdev = (struct wireless_dev *)dev;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		dev_kfree_skb_any(skb);
		return;
	}

	arvif = (struct ath11k_vif *)vif->drv_priv;
	if (!arvif) {
		dev_kfree_skb_any(skb);
		return;
	}

	ab = arvif->ar->ab;

	wifi_metadata = (struct nss_wifi_vdev_per_packet_metadata *)(skb->head +
			NSS_WIFI_VDEV_PER_PACKET_METADATA_OFFSET);

	ath11k_dbg(ab, ATH11K_DBG_NSS_MESH,
		   "dp special data from nss on mesh link vap: pkt_type %d\n",
		   wifi_metadata->pkt_type);

	switch (wifi_metadata->pkt_type) {
	case NSS_WIFI_VDEV_MESH_EXT_DATA_PKT_TYPE_RX_SPL_PACKET:
		ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "",
				"special packet meta data from nss on mesh link vdev: ",
				wifi_metadata,
				sizeof(struct nss_wifi_vdev_per_packet_metadata));
		ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "",
				"special packet payload from nss on mesh link vdev: ",
				skb->data, skb->len);
		dev_kfree_skb_any(skb);
		break;
	case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_MCBC_RX:
		ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "",
				"mcast packet exception from nss on mesh link vdev: ",
				skb->data, skb->len);
		rxcb = ATH11K_SKB_RXCB(skb);
		rxcb->rx_desc = (struct hal_rx_desc *)skb->head;
		rxcb->is_first_msdu = rxcb->is_last_msdu = true;
		rxcb->is_continuation = false;
		rxcb->is_mcbc = true;
		ath11k_dp_rx_from_nss(arvif->ar, skb, napi);
		break;
	case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_MESH:
		ath11k_dbg_dump(ab, ATH11K_DBG_DP_RX, "",
				"static exception path from nss on mesh link vdev: ",
				skb->data, skb->len);
		dev_kfree_skb_any(skb);
		break;
	default:
		ath11k_warn(ab, "unknown packet type received in mesh link vdev %d",
			    wifi_metadata->pkt_type);
		dev_kfree_skb_any(skb);
		break;
	}
}

int ath11k_nss_tx(struct ath11k_vif *arvif, struct sk_buff *skb)
{
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int encap_type = ath11k_dp_tx_get_encap_type(arvif, skb);
	struct ath11k_soc_dp_stats *soc_stats = &ar->ab->soc_stats;
	char dump_msg[100] = {0};

	if (!arvif->ar->ab->nss.debug_mode && encap_type != arvif->nss.encap) {
		ath11k_warn(ar->ab, "encap mismatch in nss tx skb encap type %d" \
			    "vif encap type %d\n", encap_type, arvif->nss.encap);
		goto drop;
	}

	if (encap_type == HAL_TCL_ENCAP_TYPE_ETHERNET)
		goto send;

	if (encap_type == HAL_TCL_ENCAP_TYPE_RAW)
		ath11k_nss_tx_encap_raw(skb);
	else
		ath11k_nss_tx_encap_nwifi(skb);

send:
	if (arvif->vif->type == NL80211_IFTYPE_AP_VLAN) {
		ath11k_dbg_dump(ar->ab, ATH11K_DBG_DP_TX, "ext vdev",
				"nss tx msdu: ", skb->data, skb->len);
		status = nss_wifi_ext_vdev_tx_buf(arvif->nss.ctx, skb,
						  arvif->nss.if_num);
	} else {
		if (arvif->ar->ab->nss.debug_mode) {
			if (encap_type == HAL_TCL_ENCAP_TYPE_ETHERNET &&
			    !is_multicast_ether_addr(skb->data)) {
				snprintf(dump_msg, sizeof(dump_msg),
					 "nss tx ucast msdu: %d ",
					 arvif->nss.mesh_handle);
				ath11k_dbg_dump(ar->ab, ATH11K_DBG_DP_TX, "mesh",
						dump_msg, skb->data, skb->len);
				status = nss_wifi_meshmgr_tx_buf(arvif->nss.mesh_handle,
								 skb);
			} else {
				snprintf(dump_msg, sizeof(dump_msg),
					 "nss tx mcast msdu: %d ",
					 arvif->nss.if_num);
				ath11k_dbg_dump(ar->ab, ATH11K_DBG_DP_TX, "mesh",
						dump_msg, skb->data, skb->len);
				status = nss_wifi_vdev_tx_buf(arvif->ar->nss.ctx, skb,
							      arvif->nss.if_num);
			}
		} else {
			snprintf(dump_msg, sizeof(dump_msg),
				 "nss tx msdu: %d ",
				 arvif->nss.if_num);
			ath11k_dbg_dump(ar->ab, ATH11K_DBG_DP_TX, "",
					dump_msg, skb->data, skb->len);
			status = nss_wifi_vdev_tx_buf(arvif->ar->nss.ctx, skb,
						      arvif->nss.if_num);
		}
	}

	if (status != NSS_TX_SUCCESS) {
		ath11k_dbg(ar->ab, (ATH11K_DBG_NSS | ATH11K_DBG_DP_TX),
			   "nss tx failure: %d\n", status);
		atomic_inc(&soc_stats->tx_err.nss_tx_fail);
	}

	return status;
drop:
	atomic_inc(&soc_stats->tx_err.misc_fail);
	WARN_ON_ONCE(1);
	return -EINVAL;
}

int ath11k_nss_vdev_set_cmd(struct ath11k_vif *arvif, enum ath11k_nss_vdev_cmd nss_cmd,
			    int val)
{
	struct nss_wifi_vdev_msg *vdev_msg = NULL;
	struct nss_wifi_vdev_cmd_msg *vdev_cmd;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int cmd;

	if (!ar->ab->nss.enabled)
		return 0;

	/* Monitor interface is not offloaded to nss */
	if (arvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
		return 0;

	vdev_msg = kzalloc(sizeof(*vdev_msg), GFP_ATOMIC);
	if (!vdev_msg)
		return -ENOMEM;

	switch(nss_cmd) {
	case ATH11K_NSS_WIFI_VDEV_CFG_AP_BRIDGE_CMD:
		cmd = NSS_WIFI_VDEV_CFG_AP_BRIDGE_CMD;
		break;
	case ATH11K_NSS_WIFI_VDEV_SECURITY_TYPE_CMD:
		cmd = NSS_WIFI_VDEV_SECURITY_TYPE_CMD;
		break;
	case ATH11K_NSS_WIFI_VDEV_ENCAP_TYPE_CMD:
		cmd = NSS_WIFI_VDEV_ENCAP_TYPE_CMD;
		break;
	case ATH11K_NSS_WIFI_VDEV_DECAP_TYPE_CMD:
		cmd = NSS_WIFI_VDEV_DECAP_TYPE_CMD;
		break;
	case ATH11K_NSS_WIFI_VDEV_CFG_WDS_BACKHAUL_CMD:
		cmd = NSS_WIFI_VDEV_CFG_WDS_BACKHAUL_CMD;
		break;
	case ATH11K_NSS_WIFI_VDEV_CFG_MCBC_EXC_TO_HOST_CMD:
		cmd = NSS_WIFI_VDEV_CFG_MCBC_EXC_TO_HOST_CMD;
		break;
	default:
		return -EINVAL;
	}

	ATH11K_MEMORY_STATS_INC(ar->ab, malloc_size, sizeof(*vdev_msg));

	/* TODO: Convert to function for conversion in case of many
	 * such commands
	 */
	if (cmd == NSS_WIFI_VDEV_SECURITY_TYPE_CMD)
		val = ath11k_nss_cipher_type(ar->ab, val);

	if (cmd == NSS_WIFI_VDEV_ENCAP_TYPE_CMD)
		arvif->nss.encap = val;
	else if (cmd == NSS_WIFI_VDEV_DECAP_TYPE_CMD)
		arvif->nss.decap = val;

	vdev_cmd = &vdev_msg->msg.vdev_cmd;
	vdev_cmd->cmd = cmd;
	vdev_cmd->value = val;

	nss_wifi_vdev_msg_init(vdev_msg, arvif->nss.if_num,
			       NSS_WIFI_VDEV_INTERFACE_CMD_MSG,
			       sizeof(struct nss_wifi_vdev_cmd_msg),
			       NULL, NULL);

	status = nss_wifi_vdev_tx_msg(ar->nss.ctx, vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss vdev set cmd failure cmd:%d val:%d",
			    cmd, val);
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS, "nss vdev set cmd success cmd:%d val:%d\n",
		   cmd, val);
free:
	ATH11K_MEMORY_STATS_DEC(ar->ab, malloc_size, sizeof(*vdev_msg));
	kfree(vdev_msg);
	return status;
}

static int ath11k_nss_vdev_configure(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct nss_wifi_vdev_msg *vdev_msg;
	struct nss_wifi_vdev_config_msg *vdev_cfg;
	nss_tx_status_t status;
	int ret;

	vdev_msg = kzalloc(sizeof(struct nss_wifi_vdev_msg), GFP_ATOMIC);
	if (!vdev_msg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ar->ab, malloc_size,
				sizeof(struct nss_wifi_vdev_msg));

	vdev_cfg = &vdev_msg->msg.vdev_config;

	if (arvif->vif->type == NL80211_IFTYPE_MESH_POINT)
		vdev_cfg->vap_ext_mode = WIFI_VDEV_EXT_MODE_MESH_LINK;

	vdev_cfg->radio_ifnum = ar->nss.if_num;
	vdev_cfg->vdev_id = arvif->vdev_id;

	vdev_cfg->opmode = ath11k_nss_get_vdev_opmode(arvif);

	ether_addr_copy(vdev_cfg->mac_addr, arvif->vif->addr);

	reinit_completion(&arvif->nss.complete);

	nss_wifi_vdev_msg_init(vdev_msg, arvif->nss.if_num,
			       NSS_WIFI_VDEV_INTERFACE_CONFIGURE_MSG,
			       sizeof(struct nss_wifi_vdev_config_msg),
			       (nss_wifi_vdev_msg_callback_t *)ath11k_nss_vdev_cfg_cb,
			       arvif);

	status = nss_wifi_vdev_tx_msg(ar->nss.ctx, vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "failed to configure nss vdev nss_err:%d\n",
			    status);
		ret = -EINVAL;
		goto free;
	}

	ret = wait_for_completion_timeout(&arvif->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ar->ab, "timeout in receiving nss vdev cfg response\n");
		ret = -ETIMEDOUT;
		goto free;
	}

	ret = 0;
free:
	ATH11K_MEMORY_STATS_DEC(ar->ab, malloc_size,
				sizeof(struct nss_wifi_vdev_msg));
	kfree(vdev_msg);

	return ret;
}

static int ath11k_nss_mesh_obj_assoc_link_vap(struct ath11k_vif *arvif)
{
	struct nss_wifi_mesh_assoc_link_vap *msg;
	struct ath11k_base *ab = arvif->ar->ab;
	nss_tx_status_t status;
	int ret;

	msg = kzalloc(sizeof(struct nss_wifi_mesh_assoc_link_vap), GFP_ATOMIC);
	if (!msg)
		return -ENOMEM;

	msg->link_vap_id = arvif->nss.if_num;

	ath11k_dbg(ab, ATH11K_DBG_NSS_MESH, "nss mesh assoc link vap %d, mesh handle %d\n",
		   arvif->nss.if_num, arvif->nss.mesh_handle);

	status = nss_wifi_meshmgr_assoc_link_vap_sync(arvif->nss.mesh_handle, msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "failed mesh obj vdev tx msg for assoc link vap nss_err:%d\n",
			    status);
		ret = -EINVAL;
		goto free;
	}

	ret = 0;
free:
	kfree(msg);

	return ret;
}

static void ath11k_nss_vdev_unregister(struct ath11k_vif *arvif)
{
	struct ath11k_base *ab = arvif->ar->ab;

	switch (arvif->vif->type) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_STATION:
		nss_unregister_wifi_vdev_if(arvif->nss.if_num);
		ath11k_dbg(ab, ATH11K_DBG_NSS, "unregistered nss vdev %d \n",
			   arvif->nss.if_num);
		break;
	case NL80211_IFTYPE_MESH_POINT:
		nss_unregister_wifi_vdev_if(arvif->nss.if_num);
		ath11k_dbg(ab, ATH11K_DBG_NSS,
			   "unregistered nss mesh vdevs mesh link %d\n",
			   arvif->nss.if_num);
		break;
	default:
		ath11k_warn(ab, "unsupported interface type %d for nss vdev unregister\n",
			    arvif->vif->type);
		return;
	}
}

static int ath11k_nss_mesh_alloc_register(struct ath11k_vif *arvif,
					 struct net_device *netdev)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct nss_wifi_mesh_config_msg *nss_msg;
	struct arvif_nss *nss = &arvif->nss;
	int ret = 0;

	nss->mesh_ttl = ATH11K_MESH_DEFAULT_ELEMENT_TTL;
	nss->mpath_refresh_time = 1000; /* msecs */
	nss->mesh_forward_enabled = true;

	nss_msg = kzalloc(sizeof(*nss_msg), GFP_KERNEL);
	if (!nss_msg)
		return -ENOMEM;

	nss_msg->ttl = nss->mesh_ttl;
	nss_msg->mesh_path_refresh_time = nss->mpath_refresh_time;
	nss_msg->mpp_learning_mode = mpp_mode;
	nss_msg->block_mesh_forwarding = 0;
	ether_addr_copy(nss_msg->local_mac_addr, arvif->vif->addr);
	nss_msg->config_flags =
			 NSS_WIFI_MESH_CONFIG_FLAG_TTL_VALID		|
			 NSS_WIFI_MESH_CONFIG_FLAG_MPATH_REFRESH_VALID	|
			 NSS_WIFI_MESH_CONFIG_FLAG_MPP_LEARNING_MODE_VALID |
			 NSS_WIFI_MESH_CONFIG_FLAG_BLOCK_MESH_FWD_VALID |
			 NSS_WIFI_MESH_CONFIG_FLAG_LOCAL_MAC_VALID;

	arvif->nss.mesh_handle = nss_wifi_meshmgr_if_create_sync(netdev, nss_msg,
								 ath11k_nss_mesh_obj_vdev_data_receive,
								 ath11k_nss_mesh_obj_ext_data_callback,
								 ath11k_nss_mesh_obj_vdev_event_receive);
	if (arvif->nss.mesh_handle == NSS_WIFI_MESH_HANDLE_INVALID) {
		ath11k_warn(ab, "failed to create meshmgr\n");
		ret = -EINVAL;
	}

	kfree(nss_msg);

	return ret;
}

static int ath11k_nss_mesh_vdev_register(struct ath11k_vif *arvif,
					 struct net_device *netdev)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	nss_tx_status_t status;
	u32 features = 0;

	status = nss_register_wifi_vdev_if(ar->nss.ctx,
				arvif->nss.if_num,
				ath11k_nss_mesh_link_vdev_data_receive,
				ath11k_nss_mesh_link_vdev_special_data_receive,
				ath11k_nss_vdev_event_receive,
				(struct net_device *)netdev->ieee80211_ptr,
				features);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "failed to register nss mesh link vdev if_num %d nss_err:%d\n",
			    arvif->nss.if_num, status);
		nss_unregister_wifi_vdev_if(arvif->nss.if_num);
		return -EINVAL;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS, "registered nss mesh link vdev if_num %d\n",
		   arvif->nss.if_num);

	return 0;
}

static int ath11k_nss_vdev_register(struct ath11k_vif *arvif,
				    struct net_device *netdev)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	nss_tx_status_t status;
	u32 features = 0;

	switch (arvif->vif->type) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_STATION:
		status = nss_register_wifi_vdev_if(ar->nss.ctx,
					arvif->nss.if_num,
					ath11k_nss_vdev_data_receive,
					ath11k_nss_vdev_special_data_receive,
					ath11k_nss_vdev_event_receive,
					netdev, features);
		if (status != NSS_TX_SUCCESS) {
			ath11k_warn(ab, "failed to register nss vdev if_num %d nss_err:%d\n",
				    arvif->nss.if_num, status);
			return -EINVAL;
		}

		ath11k_dbg(ab, ATH11K_DBG_NSS, "registered nss vdev if_num %d\n",
			   arvif->nss.if_num);

		break;
	case NL80211_IFTYPE_MESH_POINT:
		if (!ab->nss.mesh_nss_offload_enabled)
			return -ENOTSUPP;

		if (ath11k_nss_mesh_vdev_register(arvif, netdev))
			return -EINVAL;
		break;
	default:
		ath11k_warn(ab, "unsupported interface type %d for nss vdev register\n",
			    arvif->vif->type);
		return -ENOTSUPP;
	}

	return 0;
}

static void ath11k_nss_mesh_vdev_free(struct ath11k_vif *arvif)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct ath11k *ar = arvif->ar;
	struct ath11k_nss_mpath_entry *mpath_entry, *mpath_tmp;
	struct ath11k_nss_mpp_entry *mpp_entry, *mpp_tmp;
	struct arvif_nss *nss = &arvif->nss, *nss_entry, *nss_tmp;
	LIST_HEAD(mpath_local_entry);
	LIST_HEAD(mpp_local_entry);
	nss_tx_status_t status;

	del_timer_sync(&nss->mpp_expiry_timer);

	spin_lock_bh(&ar->nss.dump_lock);
	list_splice_tail_init(&nss->mpath_dump, &mpath_local_entry);
	spin_unlock_bh(&ar->nss.dump_lock);

	list_for_each_entry_safe(mpath_entry, mpath_tmp, &mpath_local_entry, list) {
		list_del(&mpath_entry->list);
		kfree(mpath_entry);
	}

	spin_lock_bh(&ar->nss.dump_lock);
	list_splice_tail_init(&nss->mpp_dump, &mpp_local_entry);
	spin_unlock_bh(&ar->nss.dump_lock);

	list_for_each_entry_safe(mpp_entry, mpp_tmp, &mpp_local_entry, list) {
		list_del(&mpp_entry->list);
		kfree(mpp_entry);
	}

	list_for_each_entry_safe(nss_entry, nss_tmp, &mesh_vaps, list)
		list_del(&nss_entry->list);

	status = nss_dynamic_interface_dealloc_node(
						arvif->nss.if_num,
						NSS_DYNAMIC_INTERFACE_TYPE_VAP);
	if (status != NSS_TX_SUCCESS)
		ath11k_warn(ab, "failed to free nss mesh link vdev nss_err:%d\n",
			    status);
	else
		ath11k_dbg(ab, ATH11K_DBG_NSS,
			   "nss mesh link vdev interface deallocated\n");

	status = nss_wifi_meshmgr_if_destroy_sync(arvif->nss.mesh_handle);

	if (status != NSS_TX_SUCCESS)
		ath11k_warn(ab, "failed to free nss mesh object vdev nss_err:%d\n",
			    status);
	else
		ath11k_dbg(ab, ATH11K_DBG_NSS,
			   "nss mesh object vdev interface deallocated\n");
}

void ath11k_nss_vdev_free(struct ath11k_vif *arvif)
{
	struct ath11k_base *ab = arvif->ar->ab;
	nss_tx_status_t status;

	switch (arvif->vif->type) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_STATION:
		status = nss_dynamic_interface_dealloc_node(
						arvif->nss.if_num,
						NSS_DYNAMIC_INTERFACE_TYPE_VAP);
		if (status != NSS_TX_SUCCESS)
			ath11k_warn(ab, "failed to free nss vdev nss_err:%d\n",
				    status);
		else
			ath11k_dbg(ab, ATH11K_DBG_NSS,
				   "nss vdev interface deallocated\n");

		return;
	case NL80211_IFTYPE_MESH_POINT:
		ath11k_nss_mesh_vdev_free(arvif);
		return;
	default:
		ath11k_warn(ab, "unsupported interface type %d for nss vdev dealloc\n",
			    arvif->vif->type);
		return;
	}
}

struct arvif_nss *ath11k_nss_find_arvif_by_if_num(int if_num)
{
	struct arvif_nss *nss;

	list_for_each_entry(nss, &mesh_vaps, list) {
		if (if_num == nss->if_num)
			return nss;
	}
	return NULL;
}

int ath11k_nss_assoc_link_arvif_to_ifnum(struct ath11k_vif *arvif, int if_num)
{
	struct ath11k_base *ab = arvif->ar->ab;
	struct ath11k_vif *arvif_link;
	struct wireless_dev *wdev;
	struct arvif_nss *nss;
	int ret;

	wdev = ieee80211_vif_to_wdev_relaxed(arvif->vif);
	if (!wdev) {
		ath11k_warn(ab, "ath11k_nss: wdev is null\n");
		return -EINVAL;
	}

	if (!wdev->netdev) {
		ath11k_warn(ab, "ath11k_nss: netdev is null\n");
		return -EINVAL;
	}

	nss = ath11k_nss_find_arvif_by_if_num(if_num);
	if (!nss) {
		ath11k_warn(ab, "ath11k_nss: unable to find if_num %d\n",if_num);
		return -EINVAL;
	}

	arvif_link = container_of(nss, struct ath11k_vif, nss);

	ath11k_dbg(ab, ATH11K_DBG_NSS_MESH,
		   "assoc link vap ifnum %d to mesh handle of link id %d\n",
		   arvif_link->nss.if_num, arvif->nss.if_num);

	arvif_link->nss.mesh_handle = arvif->nss.mesh_handle;

	ret = ath11k_nss_mesh_obj_assoc_link_vap(arvif_link);
	if (ret)
		ath11k_warn(ab, "failed to associate link vap to mesh vap %d\n", ret);

	return 0;
}

static int ath11k_nss_mesh_vdev_alloc(struct ath11k_vif *arvif,
		struct net_device *netdev)
{
	struct ath11k_base *ab = arvif->ar->ab;
	int if_num;

	if (!ab->nss.mesh_nss_offload_enabled)
		return -ENOTSUPP;

	if_num = nss_dynamic_interface_alloc_node(NSS_DYNAMIC_INTERFACE_TYPE_VAP);
	if (if_num < 0) {
		ath11k_warn(ab, "failed to allocate nss mesh link vdev\n");
		return -EINVAL;
	}

	arvif->nss.if_num = if_num;

	INIT_LIST_HEAD(&arvif->nss.list);
	list_add_tail(&arvif->nss.list, &mesh_vaps);

	INIT_LIST_HEAD(&arvif->nss.mpath_dump);
	init_completion(&arvif->nss.dump_mpath_complete);
	INIT_LIST_HEAD(&arvif->nss.mpp_dump);
	init_completion(&arvif->nss.dump_mpp_complete);

	return 0;
}

static int ath11k_nss_vdev_alloc(struct ath11k_vif *arvif,
		struct net_device *netdev)
{
	struct ath11k_base *ab = arvif->ar->ab;
	enum nss_dynamic_interface_type if_type;
	int if_num;
	int ret;

	/* Initialize completion for verifying NSS message response */
	init_completion(&arvif->nss.complete);

	switch (arvif->vif->type) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_STATION:
		if_type = NSS_DYNAMIC_INTERFACE_TYPE_VAP;
		/* allocate interface context with NSS driver for the new vdev */
		if_num = nss_dynamic_interface_alloc_node(if_type);
		if (if_num < 0) {
			ath11k_warn(ab, "failed to allocate nss vdev\n");
			return -EINVAL;
		}

		arvif->nss.if_num = if_num;

		ath11k_dbg(ab, ATH11K_DBG_NSS, "allocated nss vdev if_num %d\n",
			   arvif->nss.if_num);

		break;
	case NL80211_IFTYPE_MESH_POINT:
		ret = ath11k_nss_mesh_vdev_alloc(arvif, netdev);
		if (ret) {
			ath11k_warn(ab, "failed to allocate nss vdev of mesh type %d\n",
				    ret);
			return ret;
		}
		break;
	default:
		ath11k_warn(ab, "unsupported interface type %d for nss vdev alloc\n",
			    arvif->vif->type);
		return -ENOTSUPP;
	}

	return 0;
}

int ath11k_nss_vdev_create(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	struct wireless_dev *wdev;
	int ret;

	if (!ab->nss.enabled)
		return 0;

	/* Monitor interface is not offloaded to nss */
	if (arvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
		return 0;

	if (arvif->nss.created)
		return 0;

	wdev = ieee80211_vif_to_wdev_relaxed(arvif->vif);
	if (!wdev) {
		ath11k_warn(ab, "ath11k_nss: wdev is null\n");
		return -EINVAL;
	}

	if (!wdev->netdev) {
		ath11k_warn(ab, "ath11k_nss: netdev is null\n");
		return -EINVAL;
	}

	ret = ath11k_nss_vdev_alloc(arvif, wdev->netdev);
	if (ret)
		return ret;

	ret = ath11k_nss_vdev_register(arvif, wdev->netdev);
	if (ret)
		goto free_vdev;

	switch (arvif->vif->type) {
	case NL80211_IFTYPE_STATION:
		ret = ath11k_nss_vdev_configure(arvif);
		if (ret)
			goto unregister_vdev;

		ret = ath11k_nss_vdev_set_cmd(arvif,
					      ATH11K_NSS_WIFI_VDEV_CFG_MCBC_EXC_TO_HOST_CMD,
					      ATH11K_NSS_ENABLE_MCBC_EXC);
		if (ret) {
			ath11k_err(ab, "failed to set MCBC in nss %d\n", ret);
			goto unregister_vdev;
		}
		break;
	case NL80211_IFTYPE_AP:
		ret = ath11k_nss_vdev_configure(arvif);
		if (ret)
			goto unregister_vdev;

		ret = ath11k_nss_vdev_set_cmd(arvif,
					      ATH11K_NSS_WIFI_VDEV_CFG_WDS_BACKHAUL_CMD,
					      true);
		if (ret) {
			ath11k_warn(ab, "failed to cfg wds backhaul in nss %d\n", ret);
			goto unregister_vdev;
		}
		break;
	case NL80211_IFTYPE_MESH_POINT:
		ret = ath11k_nss_mesh_alloc_register(arvif, wdev->netdev);
		if (ret) {
			ath11k_warn(ab, "failed to alloc and register mesh vap %d\n", ret);
			goto unregister_vdev;
		}

		ret = ath11k_nss_vdev_configure(arvif);
		if (ret) {
			ath11k_warn(ab, "failed to configure nss mesh link vdev\n");
			goto unregister_vdev;
		}

		ret = ath11k_nss_mesh_obj_assoc_link_vap(arvif);
		if (ret) {
			ath11k_warn(ab, "failed to associate link vap to mesh vap %d\n", ret);
			goto unregister_vdev;
		}

		ret = ath11k_nss_vdev_set_cmd(arvif,
					      ATH11K_NSS_WIFI_VDEV_CFG_MCBC_EXC_TO_HOST_CMD, 1);
		if (ret) {
			ath11k_warn(ab, "failed to enable mcast/bcast exception %d\n", ret);
			goto unregister_vdev;
		}

		ath11k_debugfs_nss_mesh_vap_create(arvif);

		/* This timer cb is called at specified
		 * interval to update mpp exp timeout */
		timer_setup(&arvif->nss.mpp_expiry_timer,
				ath11k_nss_mpp_timer_cb, 0);

		/* Start the initial timer in 2 secs */
		mod_timer(&arvif->nss.mpp_expiry_timer,
				jiffies + msecs_to_jiffies(2 * HZ));
		break;
	default:
		ret = -ENOTSUPP;
		goto unregister_vdev;
	}

	arvif->nss.created = true;

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "nss vdev interface created ctx %pK, ifnum %d\n",
		   ar->nss.ctx, arvif->nss.if_num);

	return ret;

unregister_vdev:
	ath11k_nss_vdev_unregister(arvif);
free_vdev:
	ath11k_nss_vdev_free(arvif);

	return ret;
}

void ath11k_nss_vdev_delete(struct ath11k_vif *arvif)
{
	if (!arvif->ar->ab->nss.enabled)
		return;

	/* Monitor interface is not offloaded to nss */
	if (arvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
		return;

	if (!arvif->nss.created)
		return;

	ath11k_nss_vdev_unregister(arvif);

	ath11k_nss_vdev_free(arvif);

	arvif->nss.created = false;
}

int ath11k_nss_vdev_up(struct ath11k_vif *arvif)
{
	struct nss_wifi_vdev_msg *vdev_msg = NULL;
	struct nss_wifi_vdev_enable_msg *vdev_en;
	struct ath11k *ar = arvif->ar;
	struct ath11k_vif *ap_vlan_arvif, *tmp;
	nss_tx_status_t status;
	int ret = 0;

	if (!ar->ab->nss.enabled)
		return 0;

	/* Monitor interface is not offloaded to nss */
	if (arvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
		return 0;

	if (arvif->vif->type == NL80211_IFTYPE_MESH_POINT) {
		status = nss_wifi_meshmgr_if_up(arvif->nss.mesh_handle);
		if (status != NSS_TX_SUCCESS) {
			ath11k_warn(ar->ab, "nss mesh vdev up error %d\n", status);
			return -EINVAL;
		}
	}

	vdev_msg = kzalloc(sizeof(struct nss_wifi_vdev_msg), GFP_ATOMIC);
	if (!vdev_msg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ar->ab, malloc_size,
				sizeof(struct nss_wifi_vdev_msg));

	vdev_en = &vdev_msg->msg.vdev_enable;

	ether_addr_copy(vdev_en->mac_addr, arvif->vif->addr);

	nss_wifi_vdev_msg_init(vdev_msg, arvif->nss.if_num,
			       NSS_WIFI_VDEV_INTERFACE_UP_MSG,
			       sizeof(struct nss_wifi_vdev_enable_msg),
			       NULL, NULL);

	status = nss_wifi_vdev_tx_msg(ar->nss.ctx, vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss vdev up tx msg error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS, "nss vdev up tx msg success\n");

	if (arvif->vif->type == NL80211_IFTYPE_AP)
		list_for_each_entry_safe(ap_vlan_arvif, tmp, &arvif->ap_vlan_arvifs,
                                         list)
			if (ap_vlan_arvif->nss.added)
				ath11k_nss_ext_vdev_up(ap_vlan_arvif);
free:
	ATH11K_MEMORY_STATS_DEC(ar->ab, malloc_size,
				sizeof(struct nss_wifi_vdev_msg));
	kfree(vdev_msg);
	return ret;
}

int ath11k_nss_vdev_down(struct ath11k_vif *arvif)
{
	struct nss_wifi_vdev_msg *vdev_msg = NULL;
	struct ath11k *ar = arvif->ar;
	struct ath11k_vif *ap_vlan_arvif, *tmp;
	nss_tx_status_t status;
	int ret = 0;

	if (!ar->ab->nss.enabled)
		return 0;

	/* Monitor interface is not offloaded to nss */
	if (arvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
		return 0;

	if (arvif->vif->type == NL80211_IFTYPE_MESH_POINT) {
		status = nss_wifi_meshmgr_if_down(arvif->nss.mesh_handle);
		if (status != NSS_TX_SUCCESS) {
			ath11k_warn(ar->ab, "nss mesh vdev up error %d\n", status);
			return -EINVAL;
		}
	}

	vdev_msg = kzalloc(sizeof(struct nss_wifi_vdev_msg), GFP_ATOMIC);
	if (!vdev_msg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ar->ab, malloc_size,
				sizeof(struct nss_wifi_vdev_msg));
	nss_wifi_vdev_msg_init(vdev_msg, arvif->nss.if_num,
			       NSS_WIFI_VDEV_INTERFACE_DOWN_MSG,
			       sizeof(struct nss_wifi_vdev_disable_msg),
			       NULL, NULL);

	status = nss_wifi_vdev_tx_msg(ar->nss.ctx, vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss vdev down tx msg error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS, "nss vdev down tx msg success\n");

	if (arvif->vif->type == NL80211_IFTYPE_AP)
		list_for_each_entry_safe(ap_vlan_arvif, tmp, &arvif->ap_vlan_arvifs,
                                         list)
			ath11k_nss_ext_vdev_down(ap_vlan_arvif);
free:
	ATH11K_MEMORY_STATS_DEC(ar->ab, malloc_size,
				sizeof(struct nss_wifi_vdev_msg));
	kfree(vdev_msg);
	return ret;
}

int ath11k_nss_ext_vdev_cfg_wds_peer(struct ath11k_vif *arvif,
				     u8 *wds_addr, u32 wds_peer_id)
{
	struct ath11k *ar = arvif->ar;
	struct nss_wifi_ext_vdev_msg *ext_vdev_msg = NULL;
	struct nss_wifi_ext_vdev_wds_msg *cfg_wds_msg = NULL;
	nss_tx_status_t status;
	int ret;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return -EINVAL;

	ext_vdev_msg = kzalloc(sizeof(struct nss_wifi_ext_vdev_msg), GFP_ATOMIC);
	if (!ext_vdev_msg)
		return -ENOMEM;

	cfg_wds_msg = &ext_vdev_msg->msg.wmsg;
	cfg_wds_msg->wds_peer_id = wds_peer_id;
	ether_addr_copy(cfg_wds_msg->mac_addr, wds_addr);

	nss_wifi_ext_vdev_msg_init(ext_vdev_msg, arvif->nss.if_num,
				   NSS_WIFI_EXT_VDEV_MSG_CONFIGURE_WDS,
				   sizeof(struct nss_wifi_ext_vdev_wds_msg),
				   NULL, arvif);

	status = nss_wifi_ext_vdev_tx_msg_sync(arvif->nss.ctx, ext_vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "failed to configure wds peer nss_err:%d\n",
			    status);
		ret = -EINVAL;
		goto free;
	}

	ret = 0;
free:
	kfree(ext_vdev_msg);

	return ret;
}

int ath11k_nss_ext_vdev_wds_4addr_allow(struct ath11k_vif *arvif,
					u32 wds_peer_id)
{
	struct ath11k *ar = arvif->ar;
	struct nss_wifili_peer_wds_4addr_allow_msg *cfg_4addr_msg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	struct nss_wifili_msg *wlmsg;
	nss_tx_status_t status;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return -EINVAL;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	cfg_4addr_msg = &wlmsg->msg.wpswm;
	cfg_4addr_msg->peer_id = wds_peer_id;
	cfg_4addr_msg->if_num = arvif->nss.if_num;
	cfg_4addr_msg->enable = true;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ar->ab->nss.if_num,
			 NSS_WIFILI_PEER_4ADDR_EVENT_MSG,
			 sizeof(struct nss_wifili_peer_wds_4addr_allow_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ar->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss wds 4addr allow if_num %d, peer_id %d cfg fail: %d\n",
			    arvif->nss.if_num, wds_peer_id, status);
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_WDS, "nss wds 4addr allow if_num %d, peer_id %d cfg complete\n",
		   arvif->nss.if_num, wds_peer_id);
free:
	kfree(wlmsg);
	return status;
}

int ath11k_nss_ext_vdev_configure(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_vif *ap_vif = arvif->nss.ap_vif;
	struct nss_wifi_ext_vdev_msg *ext_vdev_msg = NULL;
	struct nss_wifi_ext_vdev_configure_if_msg *ext_vdev_cfg = NULL;
	nss_tx_status_t status;
	int ret;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return -EINVAL;

	ext_vdev_msg = kzalloc(sizeof(struct nss_wifi_ext_vdev_msg), GFP_ATOMIC);
	if (!ext_vdev_msg)
		return -ENOMEM;

	ext_vdev_cfg = &ext_vdev_msg->msg.cmsg;

	ext_vdev_cfg->radio_ifnum = ar->nss.if_num;
	ext_vdev_cfg->pvap_ifnum = ap_vif->nss.if_num;

	ether_addr_copy(ext_vdev_cfg->mac_addr, arvif->vif->addr);

	nss_wifi_ext_vdev_msg_init(ext_vdev_msg, arvif->nss.if_num,
				   NSS_WIFI_EXT_VDEV_MSG_CONFIGURE_IF,
				   sizeof(struct nss_wifi_ext_vdev_configure_if_msg),
				   NULL, arvif);

	status = nss_wifi_ext_vdev_tx_msg_sync(arvif->ar->nss.ctx, ext_vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "failed to configure nss ext vdev nss_err:%d\n",
			    status);
		ret = -EINVAL;
		goto free;
	}

	ret = 0;
free:
	kfree(ext_vdev_msg);

	return ret;
}

void ath11k_nss_ext_vdev_unregister(struct ath11k_vif *arvif)
{
	struct ath11k_base *ab = arvif->ar->ab;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return;

	nss_wifi_ext_vdev_unregister_if(arvif->nss.if_num);
	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "unregistered nss vdev %d \n",
		   arvif->nss.if_num);
}

static int ath11k_nss_ext_vdev_register(struct ath11k_vif *arvif,
					struct net_device *netdev)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	nss_tx_status_t status;
	u32 features = 0;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN || arvif->nss.ctx)
		return -EINVAL;

	arvif->nss.ctx = nss_wifi_ext_vdev_register_if(arvif->nss.if_num,
						       ath11k_nss_ext_vdev_data_receive,
						       ath11k_nss_ext_vdev_special_data_receive,
						       NULL, netdev, features,
						       arvif);

	if (!arvif->nss.ctx) {
		ath11k_warn(ab, "failed to register nss vdev if_num %d nss_err:%d\n",
			    arvif->nss.if_num, status);
		return -EINVAL;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS, "registered nss ext vdev if_num %d\n",
		   arvif->nss.if_num);
	return 0;
}

static void ath11k_nss_ext_vdev_free(struct ath11k_vif *arvif)
{
	struct ath11k_base *ab = arvif->ar->ab;
	nss_tx_status_t status;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return;

	status = nss_dynamic_interface_dealloc_node(
					arvif->nss.if_num,
					arvif->nss.di_type);

	if (status != NSS_TX_SUCCESS)
		ath11k_warn(ab, "failed to free nss ext vdev err:%d\n",
			    status);
	else
		ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
			   "nss ext vdev interface deallocated\n");
}

static int ath11k_nss_ext_vdev_alloc(struct ath11k_vif *arvif,
				     struct wireless_dev *wdev)
{
	struct ath11k_base *ab = arvif->ar->ab;
	enum nss_dynamic_interface_type di_type;
	int if_num;

	if (wdev->use_4addr)
		di_type = NSS_DYNAMIC_INTERFACE_TYPE_WIFI_EXT_VDEV_WDS;
	else
		di_type = NSS_DYNAMIC_INTERFACE_TYPE_WIFI_EXT_VDEV_VLAN;

	arvif->nss.di_type = di_type;
	if_num = nss_dynamic_interface_alloc_node(di_type);
	if (if_num < 0) {
		ath11k_warn(ab, "failed to allocate nss ext vdev\n");
		return -EINVAL;
	}

	arvif->nss.if_num = if_num;
	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "nss ext vdev interface %pM di_type %d allocated if_num %d\n",
		   arvif->vif->addr, di_type, if_num);

	return 0;
}

int ath11k_nss_ext_vdev_create(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;
	struct wireless_dev *wdev;
	int ret;

	if (!ab->nss.enabled)
		return 0;

	if (arvif->nss.created)
		return 0;

	wdev = ieee80211_vif_to_wdev_relaxed(arvif->vif);
	if (!wdev) {
		ath11k_warn(ab, "ath11k_nss: ext wdev is null\n");
		return -EINVAL;
	}

	if (!wdev->netdev) {
		ath11k_warn(ab, "ath11k_nss: ext netdev is null\n");
		return -EINVAL;
	}

	ret = ath11k_nss_ext_vdev_alloc(arvif, wdev);
	if (ret)
		return ret;

	ret = ath11k_nss_ext_vdev_register(arvif, wdev->netdev);
	if (ret)
		goto free_ext_vdev;

	arvif->nss.created = true;

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "nss ext vdev interface created ctx %pK, ifnum %d\n",
		   arvif->nss.ctx, arvif->nss.if_num);

	return ret;

free_ext_vdev:
	ath11k_nss_ext_vdev_free(arvif);

	return ret;
}

void ath11k_nss_ext_vdev_delete(struct ath11k_vif *arvif)
{
	if (!arvif->ar->ab->nss.enabled)
		return;

	if (!arvif->nss.created)
		return;

	ath11k_dbg(arvif->ar->ab, ATH11K_DBG_NSS_WDS,
		   "nss ext vdev interface delete ctx %pK, ifnum %d\n",
		   arvif->nss.ctx, arvif->nss.if_num);

	ath11k_nss_ext_vdev_unregister(arvif);

	ath11k_nss_ext_vdev_free(arvif);

	arvif->nss.created = false;
}

int ath11k_nss_ext_vdev_up(struct ath11k_vif *arvif)
{
	struct nss_wifi_ext_vdev_msg *ext_vdev_msg = NULL;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	if (!ar->ab->nss.enabled)
		return 0;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return -EINVAL;

	if (arvif->nss.ext_vdev_up)
		return 0;

	ext_vdev_msg = kzalloc(sizeof(struct nss_wifi_vdev_msg), GFP_ATOMIC);
	if (!ext_vdev_msg)
		return -ENOMEM;

	nss_wifi_ext_vdev_msg_init(ext_vdev_msg, arvif->nss.if_num, NSS_IF_OPEN,
			           sizeof(struct nss_if_open), NULL, arvif);

	status = nss_wifi_ext_vdev_tx_msg_sync(arvif->nss.ctx, ext_vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss ext vdev up tx msg error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_WDS, "nss ext vdev up tx msg success\n");
	arvif->nss.ext_vdev_up = true;
free:
	kfree(ext_vdev_msg);
	return ret;
}

int ath11k_nss_ext_vdev_down(struct ath11k_vif *arvif)
{
	struct nss_wifi_ext_vdev_msg *ext_vdev_msg = NULL;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	if (!ar->ab->nss.enabled)
		return 0;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return -EINVAL;

	ext_vdev_msg = kzalloc(sizeof(struct nss_wifi_ext_vdev_msg), GFP_ATOMIC);
	if (!ext_vdev_msg)
		return -ENOMEM;

	nss_wifi_ext_vdev_msg_init(ext_vdev_msg, arvif->nss.if_num, NSS_IF_CLOSE,
			           sizeof(struct nss_if_close), NULL, arvif);

	status = nss_wifi_ext_vdev_tx_msg_sync(arvif->nss.ctx, ext_vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss ext vdev down tx msg error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_WDS, "nss ext vdev down tx msg success\n");

	arvif->nss.ext_vdev_up = false;
free:
	kfree(ext_vdev_msg);
	return ret;
}

int ath11k_nss_ext_vdev_cfg_dyn_vlan(struct ath11k_vif *arvif, u16 vlan_id)
{
	struct ath11k *ar = arvif->ar;
	struct nss_wifi_ext_vdev_msg *ext_vdev_msg = NULL;
	struct nss_wifi_ext_vdev_vlan_msg *cfg_dyn_vlan_msg = NULL;
	nss_tx_status_t status;
	int ret;

	if (!ar->ab->nss.enabled)
		return 0;

	if (arvif->vif->type != NL80211_IFTYPE_AP_VLAN)
		return -EINVAL;

	ext_vdev_msg = kzalloc(sizeof(struct nss_wifi_ext_vdev_msg), GFP_ATOMIC);
	if (!ext_vdev_msg)
		return -ENOMEM;

	cfg_dyn_vlan_msg = &ext_vdev_msg->msg.vmsg;
	cfg_dyn_vlan_msg->vlan_id = vlan_id;

	nss_wifi_ext_vdev_msg_init(ext_vdev_msg, arvif->nss.if_num,
				   NSS_WIFI_EXT_VDEV_MSG_CONFIGURE_VLAN,
				   sizeof(struct nss_wifi_ext_vdev_vlan_msg),
				   NULL, arvif);

	status = nss_wifi_ext_vdev_tx_msg_sync(arvif->nss.ctx, ext_vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "failed to configure dyn vlan nss_err:%d\n",
			    status);
		ret = -EINVAL;
		goto free;
	}

	ret = 0;
free:
	kfree(ext_vdev_msg);

	return ret;
}

int ath11k_nss_dyn_vlan_set_group_key(struct ath11k_vif *arvif, u16 vlan_id,
				      u16 group_key)
{
	struct nss_wifi_vdev_msg *vdev_msg = NULL;
	struct nss_wifi_vdev_set_vlan_group_key *vlan_group_key;
	struct ath11k *ar = arvif->ar;
	nss_tx_status_t status;
	int ret = 0;

	if (!ar->ab->nss.enabled)
		return 0;

	vdev_msg = kzalloc(sizeof(struct nss_wifi_vdev_msg), GFP_ATOMIC);
	if (!vdev_msg)
		return -ENOMEM;

	vlan_group_key = &vdev_msg->msg.vlan_group_key;
	vlan_group_key->vlan_id = vlan_id;
	vlan_group_key->group_key = group_key;

	nss_wifi_vdev_msg_init(vdev_msg, arvif->nss.if_num,
			       NSS_WIFI_VDEV_SET_GROUP_KEY,
			       sizeof(struct nss_wifi_vdev_set_vlan_group_key),
			       NULL, NULL);

	status = nss_wifi_vdev_tx_msg(ar->nss.ctx, vdev_msg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss vdev set vlan group key error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS_WDS, "nss vdev set vlan group key success\n");
free:
	kfree(vdev_msg);
	return ret;

}

/*----------------------------Peer Setup/Config -----------------------------*/

int ath11k_nss_set_peer_sec_type(struct ath11k *ar,
				 struct ath11k_peer *peer,
				 struct ieee80211_key_conf *key_conf)
{
	struct nss_wifili_peer_security_type_msg *sec_msg;
	nss_wifili_msg_callback_t msg_cb;
	struct nss_wifili_msg *wlmsg;
	nss_tx_status_t status;
	u8 *mic_key;

	if (!ar->ab->nss.enabled)
		return 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ar->ab, malloc_size,
				sizeof(struct nss_wifili_msg));

	sec_msg = &wlmsg->msg.securitymsg;
	sec_msg->peer_id = peer->peer_id;

	/* 0 -unicast , 1 - mcast/unicast */
	sec_msg->pkt_type = !(key_conf->flags & IEEE80211_KEY_FLAG_PAIRWISE);

	sec_msg->security_type = ath11k_nss_cipher_type(ar->ab,
							key_conf->cipher);

	if (sec_msg->security_type == PEER_SEC_TYPE_TKIP) {
		mic_key = &key_conf->key[NL80211_TKIP_DATA_OFFSET_RX_MIC_KEY];
		memcpy(&sec_msg->mic_key[0], mic_key, NSS_WIFILI_MIC_KEY_LEN);
	}

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ar->ab->nss.if_num,
			 NSS_WIFILI_PEER_SECURITY_TYPE_MSG,
			 sizeof(struct nss_wifili_peer_security_type_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ar->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss peer %d security cfg fail %d\n",
			    peer->peer_id, status);
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS, "nss peer id %d security cfg complete\n",
		   peer->peer_id);
free:
	ATH11K_MEMORY_STATS_DEC(ar->ab, malloc_size,
				sizeof(struct nss_wifili_msg));
	kfree(wlmsg);
	return status;
}

int ath11k_nss_set_peer_authorize(struct ath11k *ar, u16 peer_id)
{
	struct nss_wifili_peer_update_auth_flag *auth_msg;
	nss_wifili_msg_callback_t msg_cb;
	struct nss_wifili_msg *wlmsg;
	nss_tx_status_t status;

	if (!ar->ab->nss.enabled)
		return 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	auth_msg = &wlmsg->msg.peer_auth;
	auth_msg->peer_id = peer_id;
	auth_msg->auth_flag = 1;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ar->ab->nss.if_num,
			 NSS_WIFILI_PEER_UPDATE_AUTH_FLAG,
			 sizeof(struct nss_wifili_peer_update_auth_flag),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ar->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss peer %d auth cfg fail %d\n",
			    peer_id, status);
		goto free;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_NSS, "nss peer id %d auth cfg complete\n",
		   peer_id);
free:
	kfree(wlmsg);
	return status;
}

void ath11k_nss_update_sta_stats(struct ath11k_vif *arvif,
				 struct station_info *sinfo,
				 struct ieee80211_sta *sta)
{
	struct sta_info *stainfo;
	struct ath11k_peer *peer;
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;

	if (!ab->nss.stats_enabled)
		return;

	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find_by_addr(ab, sta->addr);
	if (!peer) {
		ath11k_dbg(ab, ATH11K_DBG_NSS, "sta stats: unable to find peer %pM\n",
					sta->addr);
		goto exit;
	}

	if (!peer->nss.nss_stats)
		goto exit;

	stainfo = container_of(sta, struct sta_info, sta);
	if (peer->nss.nss_stats->last_rx &&
			time_after((unsigned long)peer->nss.nss_stats->last_rx, stainfo->deflink.rx_stats.last_rx))
		stainfo->deflink.rx_stats.last_rx = peer->nss.nss_stats->last_rx;

	if (peer->nss.nss_stats->last_ack &&
			time_after((unsigned long)peer->nss.nss_stats->last_ack, stainfo->deflink.status_stats.last_ack))
		stainfo->deflink.status_stats.last_ack = peer->nss.nss_stats->last_ack;

	stainfo->deflink.rx_stats.dropped += peer->nss.nss_stats->rx_dropped -
		peer->nss.nss_stats->last_rxdrop;
	peer->nss.nss_stats->last_rxdrop = peer->nss.nss_stats->rx_dropped;

	sinfo->tx_packets = 0;
	/* Add only ac-0 count as mgmt packets uses WME_AC_BE */
	sinfo->tx_packets += stainfo->deflink.tx_stats.packets[WME_AC_BE];
	sinfo->tx_packets += peer->nss.nss_stats->tx_packets;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_PACKETS);
	sinfo->tx_bytes = 0;

	/* Add only ac-0 count as mgmt packets uses WME_AC_BE */
	sinfo->tx_bytes += stainfo->deflink.tx_stats.bytes[WME_AC_BE];
	sinfo->tx_bytes += peer->nss.nss_stats->tx_bytes;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_BYTES);

	sinfo->tx_failed = stainfo->deflink.status_stats.retry_failed +
		peer->nss.nss_stats->tx_failed;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_FAILED);

	sinfo->tx_retries = stainfo->deflink.status_stats.retry_count +
		peer->nss.nss_stats->tx_retries;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_RETRIES);

	sinfo->rx_packets = stainfo->deflink.rx_stats.packets +
		peer->nss.nss_stats->rx_packets;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_PACKETS);

	sinfo->rx_bytes = stainfo->deflink.rx_stats.bytes +
		peer->nss.nss_stats->rx_bytes;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_BYTES);

	if (peer->nss.nss_stats->rxrate.legacy || peer->nss.nss_stats->rxrate.nss) {
		if (peer->nss.nss_stats->rxrate.legacy) {
			sinfo->rxrate.legacy = peer->nss.nss_stats->rxrate.legacy;
		} else {
			sinfo->rxrate.mcs = peer->nss.nss_stats->rxrate.mcs;
			sinfo->rxrate.nss = peer->nss.nss_stats->rxrate.nss;
			sinfo->rxrate.bw = peer->nss.nss_stats->rxrate.bw;
			sinfo->rxrate.he_gi = peer->nss.nss_stats->rxrate.he_gi;
			sinfo->rxrate.he_dcm = peer->nss.nss_stats->rxrate.he_dcm;
			sinfo->rxrate.he_ru_alloc = peer->nss.nss_stats->rxrate.he_ru_alloc;
		}
		sinfo->rxrate.flags = peer->nss.nss_stats->rxrate.flags;
		sinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_BITRATE);
	}

exit:
	spin_unlock_bh(&ab->base_lock);
}

void ath11k_nss_update_sta_rxrate(struct hal_rx_mon_ppdu_info *ppdu_info,
				  struct ath11k_peer *peer,
				  struct hal_rx_user_status *user_stats)
{
	u16 ath11k_hal_rx_legacy_rates[] =
	{ 10, 20, 55, 60, 90, 110, 120, 180, 240, 360, 480, 540 };
	u16 rate = 0;
	u32 preamble_type;
	u8 mcs, nss;
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(peer->vif);
	struct ath11k *ar = arvif->ar;
	struct ath11k_base *ab = ar->ab;

	if (!ab->nss.enabled)
		return;

	if (!ieee80211_is_data(__cpu_to_le16(ppdu_info->frame_control)))
		return;

	if (!peer->nss.nss_stats)
		return;

	if (user_stats) {
		mcs = user_stats->mcs;
		nss = user_stats->nss;
		preamble_type = user_stats->preamble_type;
	} else {
		mcs = ppdu_info->mcs;
		nss = ppdu_info->nss;
		preamble_type = ppdu_info->preamble_type;
	}

        if ((preamble_type == WMI_RATE_PREAMBLE_CCK ||
                preamble_type == WMI_RATE_PREAMBLE_OFDM) &&
                (ppdu_info->rate < ATH11K_LEGACY_NUM)) {
		rate = ath11k_hal_rx_legacy_rates[ppdu_info->rate];
	}

	memset(&peer->nss.nss_stats->rxrate, 0, sizeof(peer->nss.nss_stats->rxrate));

	switch (preamble_type) {
	case WMI_RATE_PREAMBLE_OFDM:
		peer->nss.nss_stats->rxrate.legacy = rate;
		break;
	case WMI_RATE_PREAMBLE_CCK:
		peer->nss.nss_stats->rxrate.legacy = rate;
		break;
	case WMI_RATE_PREAMBLE_HT:
		if (mcs >= ATH11K_HT_MCS_NUM) {
			ath11k_dbg(ar->ab, ATH11K_DBG_NSS,
				    "Received invalid mcs in HT mode %d\n", mcs);
			return;
		}
		peer->nss.nss_stats->rxrate.mcs = mcs;
		peer->nss.nss_stats->rxrate.flags = RATE_INFO_FLAGS_MCS;
		if (ppdu_info->gi)
			peer->nss.nss_stats->rxrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case WMI_RATE_PREAMBLE_VHT:
		if (mcs > ATH11K_VHT_MCS_MAX) {
			ath11k_dbg(ar->ab, ATH11K_DBG_NSS,
				    "Received invalid mcs in VHT mode %d\n", mcs);
			return;
		}
		peer->nss.nss_stats->rxrate.mcs = mcs;
		peer->nss.nss_stats->rxrate.flags = RATE_INFO_FLAGS_VHT_MCS;
		if (ppdu_info->gi)
			peer->nss.nss_stats->rxrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case WMI_RATE_PREAMBLE_HE:
		if (mcs > ATH11K_HE_MCS_MAX) {
			ath11k_dbg(ar->ab, ATH11K_DBG_NSS,
				    "Received invalid mcs in HE mode %d\n", mcs);
			return;
		}
		peer->nss.nss_stats->rxrate.mcs = mcs;
		peer->nss.nss_stats->rxrate.flags = RATE_INFO_FLAGS_HE_MCS;
		peer->nss.nss_stats->rxrate.he_dcm = ppdu_info->dcm;
		peer->nss.nss_stats->rxrate.he_gi = ath11k_he_gi_to_nl80211_he_gi(ppdu_info->gi);
		peer->nss.nss_stats->rxrate.he_ru_alloc = ppdu_info->ru_alloc;
		break;
	}

	peer->nss.nss_stats->rxrate.nss = nss;
	peer->nss.nss_stats->rxrate.bw = ath11k_mac_bw_to_mac80211_bw(ppdu_info->bw);
}

int ath11k_nss_peer_delete(struct ath11k_base *ab, u32 vdev_id, const u8 *addr)
{
	struct nss_wifili_peer_msg *peer_msg;
	struct nss_wifili_msg *wlmsg = NULL;
	struct ath11k_peer *peer;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret;

	if (!ab->nss.enabled)
		return 0;

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find(ab, vdev_id, addr);
	if (!peer) {
		ath11k_warn(ab, "peer %pM not found on vdev_id %d for nss peer delete\n",
			    addr, vdev_id);
		spin_unlock_bh(&ab->base_lock);
		return -EINVAL;
	}

	if (!peer->nss.vaddr) {
		ath11k_warn(ab, "peer already deleted or peer create failed %pM\n",
			    addr);
		spin_unlock_bh(&ab->base_lock);
		return -EINVAL;
	}

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg) {
		ath11k_warn(ab, "nss send peer delete msg alloc failure\n");
		ret = -ENOMEM;
		goto free_peer;
	}

	peer_msg = &wlmsg->msg.peermsg;

	peer_msg->vdev_id = peer->vdev_id;
	peer_msg->peer_id = peer->peer_id;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_PEER_DELETE_MSG,
			 sizeof(struct nss_wifili_peer_msg),
			 msg_cb, NULL);

	reinit_completion(&peer->nss.complete);

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss send peer (%pM) delete msg tx error %d\n",
			    addr, status);
		ret = -EINVAL;
		kfree(wlmsg);
		goto free_peer;
	} else {
		ath11k_dbg(ab, ATH11K_DBG_NSS, "nss peer delete message success : peer_id %d\n",
			   peer->peer_id);
		ret = 0;
	}

	spin_unlock_bh(&ab->base_lock);

	kfree(wlmsg);

	/* No need to return failure or free up here, since the msg was tx succesfully
	 * the peer delete response would be received from NSS which will free up
	 * the allocated memory
	 */
	ret = wait_for_completion_timeout(&peer->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret)
		ath11k_warn(ab, "timeout while waiting for nss peer delete msg response\n");

	return 0;

free_peer:
	dma_unmap_single(ab->dev, peer->nss.paddr,
		 WIFILI_NSS_PEER_BYTE_SIZE, DMA_FROM_DEVICE);
	kfree(peer->nss.vaddr);
	if (peer->nss.nss_stats) {
		kfree(peer->nss.nss_stats);
		peer->nss.nss_stats = NULL;
	}
	spin_unlock_bh(&ab->base_lock);
	return ret;
}

int ath11k_nss_peer_create(struct ath11k *ar, struct ath11k_peer *peer)
{
	struct ath11k_base *ab = ar->ab;
	struct nss_wifili_peer_msg *peer_msg;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret;

	if (!ab->nss.enabled)
		return -ENOTSUPP;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	peer_msg = &wlmsg->msg.peermsg;

	peer_msg->vdev_id = peer->vdev_id;
	peer_msg->peer_id = peer->peer_id;
	peer_msg->hw_ast_idx = peer->hw_peer_id;
	peer_msg->tx_ast_hash = peer->ast_hash;
	ether_addr_copy(peer_msg->peer_mac_addr, peer->addr);

	peer->nss.vaddr = kzalloc(WIFILI_NSS_PEER_BYTE_SIZE, GFP_ATOMIC);

	/* Initialize completion for verifying Peer NSS message response */
	init_completion(&peer->nss.complete);

	if (!peer->nss.vaddr) {
		ath11k_warn(ab, "failed to allocate memory for nss peer info\n");
		kfree(wlmsg);
		return -ENOMEM;
	}

	peer->nss.paddr = dma_map_single(ab->dev, peer->nss.vaddr,
					 WIFILI_NSS_PEER_BYTE_SIZE, DMA_TO_DEVICE);

	ret = dma_mapping_error(ab->dev, peer->nss.paddr);
	if (ret) {
		ath11k_warn(ab, "error during nss peer info  memalloc\n");
		kfree(peer->nss.vaddr);
		ret = -ENOMEM;
		goto msg_free;
	}

	peer_msg->nss_peer_mem = peer->nss.paddr;
	peer_msg->psta_vdev_id = peer->vdev_id;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_PEER_CREATE_MSG,
			 sizeof(struct nss_wifili_peer_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ret = -EINVAL;
		ath11k_warn(ab, "nss send peer (%pM) create msg tx error: %d\n",
			    peer->addr, status);
		goto peer_mem_free;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "nss peer_create msg success mac:%pM vdev:%d peer_id:%d hw_ast_idx:%d ast_hash:%d\n",
		   peer_msg->peer_mac_addr, peer_msg->vdev_id, peer_msg->peer_id,
		   peer_msg->hw_ast_idx, peer_msg->tx_ast_hash);

	ret = ath11k_peer_add_ast(ar, peer, peer->addr,
				  ATH11K_AST_TYPE_STATIC);
	if (ret) {
		ath11k_warn(ab, "failed to add STATIC ast: %d\n", ret);
		goto peer_mem_free;
	}

	peer->nss.nss_stats = kzalloc(sizeof(*peer->nss.nss_stats), GFP_ATOMIC);
	if (!peer->nss.nss_stats) {
		ret = -ENOMEM;
		ath11k_warn(ab, "Unable to create nss stats memory\n");
		goto peer_mem_free;
	}

	goto msg_free;

peer_mem_free:
	dma_unmap_single(ab->dev, peer->nss.paddr,
			 WIFILI_NSS_PEER_BYTE_SIZE, DMA_FROM_DEVICE);
	kfree(peer->nss.vaddr);
msg_free:
	kfree(wlmsg);
	return ret;
}

int ath11k_nss_add_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
			    u8 *dest_mac, enum ath11k_ast_entry_type type)
{
	struct ath11k_base *ab = ar->ab;
	struct nss_wifili_wds_peer_msg *wds_peer_msg;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret = 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (wlmsg == NULL)
		return -ENOMEM;

	wds_peer_msg = &wlmsg->msg.wdspeermsg;

	wds_peer_msg->pdev_id = ar->pdev->pdev_id;
	wds_peer_msg->ast_type = type;
	wds_peer_msg->peer_id = peer->peer_id;

	ether_addr_copy(wds_peer_msg->peer_mac, peer->addr);
	ether_addr_copy(wds_peer_msg->dest_mac, dest_mac);

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_WDS_PEER_ADD_MSG,
			 sizeof(struct nss_wifili_wds_peer_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ret = -EINVAL;
		ath11k_warn(ab, "nss send wds add peer msg tx error: %d\n",
			    status);
		goto msg_free;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "nss add wds peer success pdev:%d peer mac:%pM dest mac:%pM peer_id:%d\n",
		   wds_peer_msg->pdev_id, wds_peer_msg->peer_mac, wds_peer_msg->dest_mac, wds_peer_msg->peer_id);

msg_free:
	kfree(wlmsg);
	return ret;
}

int ath11k_nss_update_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
			       u8 *dest_mac)
{
	struct ath11k_base *ab = ar->ab;
	struct nss_wifili_wds_peer_msg *wds_peer_msg;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret = 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (wlmsg == NULL)
		return -ENOMEM;

	wds_peer_msg = &wlmsg->msg.wdspeermsg;

	wds_peer_msg->pdev_id = ar->pdev->pdev_id;
	wds_peer_msg->ast_type = ATH11K_AST_TYPE_WDS;
	wds_peer_msg->peer_id = peer->peer_id;
	ether_addr_copy(wds_peer_msg->peer_mac, peer->addr);
	ether_addr_copy(wds_peer_msg->dest_mac, dest_mac);

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_WDS_PEER_UPDATE_MSG,
			 sizeof(struct nss_wifili_wds_peer_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ret = -EINVAL;
		ath11k_warn(ab, "nss send wds update peer msg tx error: %d\n",
			    status);
		goto msg_free;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "nss update wds peer success pdev:%d peer mac:%pM dest mac:%pM peer_id:%d\n",
		   wds_peer_msg->pdev_id, wds_peer_msg->peer_mac, wds_peer_msg->dest_mac, wds_peer_msg->peer_id);

msg_free:
	kfree(wlmsg);
	return ret;
}

int ath11k_nss_map_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
			    u8 *dest_mac, struct ath11k_ast_entry *ast_entry)
{
	struct ath11k_base *ab = ar->ab;
	struct nss_wifili_wds_peer_map_msg *wds_peer_map_msg;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	enum ath11k_ast_entry_type type = ast_entry->type;
	int ret = 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (wlmsg == NULL)
		return -ENOMEM;

	wds_peer_map_msg = &wlmsg->msg.wdspeermapmsg;

	wds_peer_map_msg->vdev_id = peer->vdev_id;
	wds_peer_map_msg->ast_idx = ast_entry->ast_idx;

	if (type == ATH11K_AST_TYPE_MEC)
		wds_peer_map_msg->peer_id = NSS_WIFILI_MEC_PEER_ID;
	else
		wds_peer_map_msg->peer_id = peer->peer_id;

	ether_addr_copy(wds_peer_map_msg->dest_mac, dest_mac);

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_WDS_PEER_MAP_MSG,
			 sizeof(struct nss_wifili_wds_peer_map_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ret = -EINVAL;
		ath11k_warn(ab, "nss send wds peer map msg tx error: %d\n",
			    status);
		goto msg_free;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "nss wds peer map success mac:%pM hw_ast_idx:%d peer_id:%d\n",
		   wds_peer_map_msg->dest_mac, wds_peer_map_msg->ast_idx, wds_peer_map_msg->peer_id);

msg_free:
	kfree(wlmsg);
	return ret;
}

int ath11k_nss_del_wds_peer(struct ath11k *ar, u8 *peer_addr, int peer_id,
			    u8 *dest_mac)
{
	struct ath11k_base *ab = ar->ab;
	struct nss_wifili_wds_peer_msg *wds_peer_msg;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret = 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (wlmsg == NULL)
		return -ENOMEM;

	wds_peer_msg = &wlmsg->msg.wdspeermsg;

	wds_peer_msg->pdev_id = ar->pdev->pdev_id;
	wds_peer_msg->ast_type = ATH11K_AST_TYPE_NONE;
	wds_peer_msg->peer_id = peer_id;
	ether_addr_copy(wds_peer_msg->peer_mac, peer_addr);
	ether_addr_copy(wds_peer_msg->dest_mac, dest_mac);

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_WDS_PEER_DEL_MSG,
			 sizeof(struct nss_wifili_wds_peer_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ret = -EINVAL;
		ath11k_warn(ab, "nss send wds del peer msg tx error: %d\n",
			    status);
		goto msg_free;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS_WDS,
		   "nss del wds peer success peer mac:%pM dest mac:%pM peer_id:%d\n",
		   wds_peer_msg->peer_mac, wds_peer_msg->dest_mac, wds_peer_msg->peer_id);

msg_free:
	kfree(wlmsg);
	return ret;
}

/*-------------------------------INIT/DEINIT---------------------------------*/

static int ath11k_nss_radio_buf_cfg(struct ath11k *ar, int range, int buf_sz)
{
	struct nss_wifili_radio_buf_cfg_msg *buf_cfg;
	struct nss_wifili_radio_cfg_msg *radio_buf_cfg_msg;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_tx_status_t status;
	int ret = 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	radio_buf_cfg_msg = &wlmsg->msg.radiocfgmsg;

	radio_buf_cfg_msg->radio_if_num = ar->nss.if_num;
	buf_cfg = &wlmsg->msg.radiocfgmsg.radiomsg.radiobufcfgmsg;
	buf_cfg->range = range;
	buf_cfg->buf_cnt = buf_sz;

	nss_cmn_msg_init(&wlmsg->cm, ar->ab->nss.if_num,
			 NSS_WIFILI_RADIO_BUF_CFG,
			 sizeof(struct nss_wifili_radio_buf_cfg_msg),
			 NULL, NULL);

	status = nss_wifili_tx_msg(ar->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ar->ab, "nss radio buf cfg send failed %d\n", status);
		ret = -EINVAL;
	} else {
		ath11k_dbg(ar->ab, ATH11K_DBG_NSS,
			   "nss radio cfg message range:%d buf_sz:%d if_num:%d ctx:%p\n",
			   range, buf_sz, ar->nss.if_num, ar->nss.ctx);
	}

	kfree(wlmsg);
	return ret;
}

static void ath11k_nss_fill_srng_info(struct ath11k_base *ab, int ring_id,
				      struct nss_wifili_hal_srng_info *hsi)
{
	struct ath11k_hal *hal = &ab->hal;
	struct hal_srng *srng;
	u32 offset;
	int i;

	if (ring_id < 0) {
		ath11k_warn(ab, "Invalid ring id used for nss init\n");
		WARN_ON(1);
		return;
	}

	srng = &hal->srng_list[ring_id];

	hsi->ring_id = srng->ring_id;
	hsi->ring_dir = srng->ring_dir;
	hsi->ring_base_paddr = srng->ring_base_paddr;
	hsi->entry_size = srng->entry_size;
	hsi->num_entries = srng->num_entries;
	hsi->flags = srng->flags;

	ath11k_dbg(ab, ATH11K_DBG_NSS,
		   "Ring info to send to nss - ring_id:%d ring_dir:%d ring_paddr:%d entry_size:%d num_entries:%d flags:%d\n",
		   hsi->ring_id, hsi->ring_dir, hsi->ring_base_paddr,
		   hsi->entry_size, hsi->num_entries, hsi->flags);

	for (i = 0; i < HAL_SRNG_NUM_REG_GRP; i++) {
		offset = srng->hwreg_base[i];

		/* For PCI based devices, get the umac ring base address offset
		 * based on window register configuration.
		 */
		if (!(srng->flags & HAL_SRNG_FLAGS_LMAC_RING))
			offset = ath11k_hif_get_window_offset(ab, srng->hwreg_base[i]);

		hsi->hwreg_base[i]  = (u32)ab->mem_pa + offset;

		ath11k_dbg(ab, ATH11K_DBG_NSS, "SRNG Register %d address %d\n",
			   i, hsi->hwreg_base[i]);
	}
}

static void ath11k_nss_tx_desc_mem_free(struct ath11k_base *ab)
{
	int i;

	for (i = 0; i < ATH11K_NSS_MAX_NUMBER_OF_PAGE; i++) {
		if (!ab->nss.tx_desc_paddr[i])
			continue;

		dma_free_coherent(ab->dev,
				  ab->nss.tx_desc_size[i],
				  ab->nss.tx_desc_vaddr[i],
				  ab->nss.tx_desc_paddr[i]);
		ab->nss.tx_desc_vaddr[i] = NULL;
		ATH11K_MEMORY_STATS_DEC(ab, dma_alloc, ab->nss.tx_desc_size[i]);
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS, "allocated tx desc mem freed\n");
}

static int ath11k_nss_tx_desc_mem_alloc(struct ath11k_base *ab, u32 required_size, u32 *page_idx)
{
	int i, alloc_size;
	int curr_page_idx;

	ath11k_dbg(ab, ATH11K_DBG_NSS, "tx desc mem alloc size: %d\n", required_size);

	curr_page_idx = *page_idx;

	for (i = 0, alloc_size = 0; i < required_size; i += alloc_size) {
		alloc_size = required_size - i;

		if (alloc_size > WIFILI_NSS_MAX_MEM_PAGE_SIZE)
			alloc_size = WIFILI_NSS_MAX_MEM_PAGE_SIZE;

		ab->nss.tx_desc_vaddr[curr_page_idx] =
					dma_alloc_coherent(ab->dev, alloc_size,
							   &ab->nss.tx_desc_paddr[curr_page_idx],
							   GFP_KERNEL);

		if (!ab->nss.tx_desc_vaddr[curr_page_idx])
			return -ENOMEM;

		ab->nss.tx_desc_size[curr_page_idx] = alloc_size;
		curr_page_idx++;

		ATH11K_MEMORY_STATS_INC(ab, dma_alloc, alloc_size);

		ath11k_dbg(ab, ATH11K_DBG_NSS,
			   "curr page %d, allocated %d, total allocated %d\n",
			   curr_page_idx, alloc_size, i + alloc_size);

		if (curr_page_idx == ATH11K_NSS_MAX_NUMBER_OF_PAGE) {
			ath11k_warn(ab, "max page number reached while tx desc mem allocation\n");
			return -EINVAL;
		}
	}
	*page_idx = curr_page_idx;
	return 0;
}

static int ath11k_nss_fill_tx_desc_info(struct ath11k_base *ab,
					struct nss_wifili_init_msg *wim)
{
	struct nss_wifili_tx_desc_addtnl_mem_msg *dam;
	u32 required_size, required_size_ext;
	struct nss_wifili_tx_desc_init_msg *dim;
	u32 tx_desc_limit_0 = 0;
	u32 tx_desc_limit_1 = 0;
	u32 tx_desc_limit_2 = 0;
	u32 dam_page_idx = 0;
	int page_idx = 0;
	int i;

	wim->tx_sw_internode_queue_size = ATH11K_WIFIILI_MAX_TX_PROCESSQ;

	dim = &wim->wtdim;
	dam = &wim->wtdam;

	dim->num_pool = ab->num_radios;
	dim->num_tx_device_limit = ATH11K_WIFILI_MAX_TX_DESC;

	//TODO Revisit below calc based on platform/mem cfg
	switch (dim->num_pool) {
	case 1:
		tx_desc_limit_0 = ATH11K_WIFILI_DBDC_NUM_TX_DESC;
		break;
	case 2:
		tx_desc_limit_0 = ATH11K_WIFILI_DBDC_NUM_TX_DESC;
		tx_desc_limit_1 = ATH11K_WIFILI_DBDC_NUM_TX_DESC;
		break;
	case 3:
		tx_desc_limit_0 = ATH11K_WIFILI_DBTC_NUM_TX_DESC;
		tx_desc_limit_1 = ATH11K_WIFILI_DBTC_NUM_TX_DESC;
		tx_desc_limit_2 = ATH11K_WIFILI_DBTC_NUM_TX_DESC;
		break;
	default:
		ath11k_warn(ab, "unexpected num radios during tx desc alloc\n");
		return -EINVAL;
	}

	dim->num_tx_desc = tx_desc_limit_0;
	dim->num_tx_desc_ext = tx_desc_limit_0;
	dim->num_tx_desc_2 = tx_desc_limit_1;
	dim->num_tx_desc_ext_2 = tx_desc_limit_1;
	dim->num_tx_desc_3 = tx_desc_limit_2;
	dim->num_tx_desc_ext_3 = tx_desc_limit_2;

	required_size = (dim->num_tx_desc + dim->num_tx_desc_2 +
			 dim->num_tx_desc_3 +
			 dim->num_pool) * WIFILI_NSS_TX_DESC_SIZE;

	required_size_ext = (dim->num_tx_desc_ext + dim->num_tx_desc_ext_2 +
			     dim->num_tx_desc_ext_3 +
			     dim->num_pool) * WIFILI_NSS_TX_EXT_DESC_SIZE;

	if (ath11k_nss_tx_desc_mem_alloc(ab, required_size, &page_idx)) {
		ath11k_warn(ab, "memory allocation for tx desc of size %d failed\n",
			    required_size);
		return -ENOMEM;
	}

	/* Fill the page number from where extension tx descriptor is available */
	dim->ext_desc_page_num  = page_idx;

	if (ath11k_nss_tx_desc_mem_alloc(ab, required_size_ext, &page_idx)) {
		ath11k_warn(ab, "memory allocation for extension tx desc of size %d failed\n",
			    required_size_ext);
		return -ENOMEM;
	}

	for (i = 0; i < page_idx; i++)	{
		if (i < NSS_WIFILI_MAX_NUMBER_OF_PAGE_MSG) {
			dim->memory_addr[i] = (u32)ab->nss.tx_desc_paddr[i];
			dim->memory_size[i] = (u32)ab->nss.tx_desc_size[i];
			dim->num_memaddr++;
		} else {
			dam_page_idx = i - NSS_WIFILI_MAX_NUMBER_OF_PAGE_MSG;
			dam->addtnl_memory_addr[dam_page_idx] = (u32)ab->nss.tx_desc_paddr[i];
			dam->addtnl_memory_size[dam_page_idx] = (u32)ab->nss.tx_desc_size[i];
			dam->num_addtnl_addr++;
		}
	}

	if (i > NSS_WIFILI_MAX_NUMBER_OF_PAGE_MSG)
		wim->flags |= WIFILI_ADDTL_MEM_SEG_SET;

	return 0;
}

static int ath11k_nss_get_target_type(struct ath11k_base *ab)
{
	switch (ab->hw_rev) {
	case ATH11K_HW_IPQ8074:
		return ATH11K_WIFILI_TARGET_TYPE_QCA8074V2;
	case ATH11K_HW_IPQ6018_HW10:
		return ATH11K_WIFILI_TARGET_TYPE_QCA6018;
	case ATH11K_HW_QCN9074_HW10:
		return ATH11K_WIFILI_TARGET_TYPE_QCN9074;
	case ATH11K_HW_IPQ5018:
		return ATH11K_WIFILI_TARGET_TYPE_QCA5018;
	case ATH11K_HW_QCN6122:
		return ATH11K_WIFILI_TARGET_TYPE_QCN6122;
	default:
		ath11k_warn(ab, "NSS Offload not supported for this HW\n");
		return ATH11K_WIFILI_TARGET_TYPE_UNKNOWN;
	}
}

static int ath11k_nss_get_interface_type(struct ath11k_base *ab)
{
	switch (ab->hw_rev) {
	case ATH11K_HW_IPQ8074:
	case ATH11K_HW_IPQ6018_HW10:
	case ATH11K_HW_IPQ5018:
		return NSS_WIFILI_INTERNAL_INTERFACE;
	case ATH11K_HW_QCN9074_HW10:
	case ATH11K_HW_QCN6122:
		return nss_get_available_wifili_external_if();
	default:
		/* This can't happen since we validated target type earlier */
		WARN_ON(1);
		return NSS_MAX_NET_INTERFACES;
	}
}

static int ath11k_nss_get_dynamic_interface_type(struct ath11k_base *ab)
{
	switch (ab->nss.if_num) {
	case NSS_WIFILI_INTERNAL_INTERFACE:
		return NSS_DYNAMIC_INTERFACE_TYPE_WIFILI_INTERNAL;
	case NSS_WIFILI_EXTERNAL_INTERFACE0:
		return NSS_DYNAMIC_INTERFACE_TYPE_WIFILI_EXTERNAL0;
	case NSS_WIFILI_EXTERNAL_INTERFACE1:
		return NSS_DYNAMIC_INTERFACE_TYPE_WIFILI_EXTERNAL1;
	default:
		ath11k_warn(ab, "NSS Offload invalid interface\n");
		return NSS_DYNAMIC_INTERFACE_TYPE_NONE;
	}
}

static int ath11k_nss_mesh_capability(struct ath11k_base *ab)
{
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret = 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	reinit_completion(&ab->nss.complete);

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_SEND_MESH_CAPABILITY_INFO,
			 sizeof(struct nss_wifili_mesh_capability_info),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss failed to get mesh capability msg %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ret = wait_for_completion_timeout(&ab->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ab, "timeout while waiting for mesh capability check\n");
		ret = -ETIMEDOUT;
		goto free;
	}

	kfree(wlmsg);
	return 0;

free:
	kfree(wlmsg);
	return ret;
}

static int ath11k_nss_init(struct ath11k_base *ab)
{
	struct nss_wifili_init_msg *wim = NULL;
	struct nss_wifili_msg *wlmsg = NULL;
	struct nss_ctx_instance *nss_contex;
	nss_wifili_msg_callback_t msg_cb;
	u32 target_type;
	u32 features = 0;
	nss_tx_status_t status;
	struct ath11k_dp *dp;
	int i, ret;
	struct device *dev = ab->dev;

	dp = &ab->dp;

	target_type = ath11k_nss_get_target_type(ab);

	if (target_type == ATH11K_WIFILI_TARGET_TYPE_UNKNOWN)
		return -ENOTSUPP;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	wim = &wlmsg->msg.init;

	wim->target_type = target_type;

	/* fill rx parameters to initialize rx context */
	wim->wrip.tlv_size = ab->hw_params.hal_desc_sz;
	wim->wrip.rx_buf_len = DP_RXDMA_NSS_REFILL_RING_SIZE;
	if (of_property_read_bool(dev->of_node, "nss-radio-priority"))
		wim->flags |= WIFILI_MULTISOC_THREAD_MAP_ENABLE;

	/* fill hal srng message */
	wim->hssm.dev_base_addr = (u32)ab->mem_pa;
	wim->hssm.shadow_rdptr_mem_addr = (u32)ab->hal.rdp.paddr;
	wim->hssm.shadow_wrptr_mem_addr = (u32)ab->hal.wrp.paddr;
	wim->hssm.lmac_rings_start_id = HAL_SRNG_RING_ID_LMAC1_ID_START;

	/* fill TCL data/completion ring info */
	wim->num_tcl_data_rings = DP_TCL_NUM_RING_MAX;
	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		ath11k_nss_fill_srng_info(ab, dp->tx_ring[i].tcl_data_ring.ring_id,
					  &wim->tcl_ring_info[i]);
		ath11k_nss_fill_srng_info(ab, dp->tx_ring[i].tcl_comp_ring.ring_id,
					  &wim->tx_comp_ring[i]);
	}

	/* allocate tx desc memory for NSS and fill corresponding info */
	ret = ath11k_nss_fill_tx_desc_info(ab, wim);
	if (ret)
		goto free;

	/* fill reo dest ring info */
	wim->num_reo_dest_rings = DP_REO_DST_RING_MAX;
	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		ath11k_nss_fill_srng_info(ab, dp->reo_dst_ring[i].ring_id,
					  &wim->reo_dest_ring[i]);
	}

	/* fill reo reinject ring info */
	ath11k_nss_fill_srng_info(ab, dp->reo_reinject_ring.ring_id,
				  &wim->reo_reinject_ring);

	/* fill reo release ring info */
	ath11k_nss_fill_srng_info(ab, dp->rx_rel_ring.ring_id,
				  &wim->rx_rel_ring);

	/* fill reo exception ring info */
	ath11k_nss_fill_srng_info(ab, dp->reo_except_ring.ring_id,
				  &wim->reo_exception_ring);

	ab->nss.if_num = ath11k_nss_get_interface_type(ab);

	ath11k_info(ab, "nss init soc nss if_num %d userpd_id %d\n", ab->nss.if_num, ab->userpd_id);

	if (ab->nss.if_num >= NSS_MAX_NET_INTERFACES) {
		ath11k_warn(ab, "NSS invalid interface\n");
		goto free;
	}

	/* register callbacks for events and exceptions with nss */
	nss_contex = nss_register_wifili_if(ab->nss.if_num, NULL,
					    (nss_wifili_callback_t)ath11k_nss_wifili_ext_callback_fn,
					    (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive,
					    (struct net_device *)ab, features);

	if (!nss_contex) {
		ath11k_warn(ab, "nss wifili register failure\n");
		goto free;
	}

	if (nss_cmn_get_state(nss_contex) != NSS_STATE_INITIALIZED) {
		ath11k_warn(ab, "nss state in default init state\n");
		goto free;
	}

	/* The registered soc context is stored in ab, and will be used for
	 * all soc related messages with nss
	 */
	ab->nss.ctx = nss_contex;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	/* Initialize the common part of the wlmsg */
	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_INIT_MSG,
			 sizeof(struct nss_wifili_init_msg),
			 msg_cb, NULL);

	reinit_completion(&ab->nss.complete);

	/* Note: response is contention free during init sequence */
	ab->nss.response = ATH11K_NSS_MSG_ACK;

	status = nss_wifili_tx_msg(nss_contex, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "failure to send nss init msg: %d \n", status);
		goto unregister;
	}

	ret = wait_for_completion_timeout(&ab->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ab, "timeout while waiting for nss init msg response\n");
		goto unregister;
	}

	/* Check if the response is success from the callback */
	if (ab->nss.response != ATH11K_NSS_MSG_ACK)
		goto unregister;

	kfree(wlmsg);

	/* Create a mesh links read debugfs entry */
	ath11k_debugfs_nss_soc_create(ab);

	/* Check for mesh capability */
	ret = ath11k_nss_mesh_capability(ab);

	if (ret)
		ath11k_err(ab, "Mesh offload is not enabled %d\n", ret);

	ath11k_dbg(ab, ATH11K_DBG_NSS, "NSS Init Message TX Success %p %d\n",
		   ab->nss.ctx, ab->nss.if_num);
	return 0;

unregister:
	nss_unregister_wifili_if(ab->nss.if_num);
free:
	ath11k_nss_tx_desc_mem_free(ab);
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(struct nss_wifili_msg));
	kfree(wlmsg);
	return -EINVAL;
}

static int ath11k_nss_stats_cfg(struct ath11k *ar, int nss_msg, int enable)
{
	struct nss_wifili_msg *wlmsg = NULL;
	struct nss_wifili_stats_cfg_msg *stats_cfg;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	struct ath11k_base *ab = ar->ab;
	int ret = 0;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	stats_cfg = &wlmsg->msg.scm;
	stats_cfg->cfg = enable;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 nss_msg,
			 sizeof(struct nss_wifili_stats_cfg_msg),
			 msg_cb, NULL);

	status = nss_wifili_tx_msg(ar->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss stats cfg %d msg tx failure: %d\n",
			    nss_msg, status);
		ret = -EINVAL;
		goto free;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS, "nss stats %d enable %d\n", nss_msg, enable);

free:
	kfree(wlmsg);
	return ret;
}

static void ath11k_nss_sojourn_stats_disable(struct ath11k *ar)
{
	ath11k_nss_stats_cfg(ar, NSS_WIFILI_STATS_V2_CFG_MSG,
			     ATH11K_NSS_STATS_DISABLE);
}

void ath11k_nss_peer_stats_disable(struct ath11k *ar)
{
	ath11k_nss_stats_cfg(ar, NSS_WIFILI_STATS_CFG_MSG,
			     ATH11K_NSS_STATS_DISABLE);
}

void ath11k_nss_peer_stats_enable(struct ath11k *ar)
{
	ath11k_nss_stats_cfg(ar, NSS_WIFILI_STATS_CFG_MSG,
			     ATH11K_NSS_STATS_ENABLE);
}

int ath11k_nss_pdev_init(struct ath11k_base *ab, int radio_id)
{
	struct ath11k *ar = ab->pdevs[radio_id].ar;
	struct nss_wifili_pdev_init_msg *pdevmsg;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	struct device *dev = ab->dev;
	int radio_if_num = -1;
	int refill_ring_id;
	int features = 0;
	int dyn_if_type;
	int ret, i, scheme_id = 0;
	u32 nss_radio_priority;

	dyn_if_type = ath11k_nss_get_dynamic_interface_type(ab);

	/* Allocate a node for dynamic interface */
	radio_if_num = nss_dynamic_interface_alloc_node(dyn_if_type);

	if (radio_if_num < 0)
		return -EINVAL;

	/* The ifnum and registered radio context is stored in ar and used
	 * for messages related to vdev/radio
	 */
	ar->nss.if_num = radio_if_num;

	/* No callbacks are registered for radio specific events/data */
	ar->nss.ctx = nss_register_wifili_radio_if((u32)radio_if_num, NULL,
						   NULL, NULL, (struct net_device *)ar,
						   features);

	if (!ar->nss.ctx) {
		ath11k_warn(ab, "failure during nss pdev register\n");
		ret = -EINVAL;
		goto dealloc;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS, "nss pdev init - id:%d init ctxt:%p ifnum:%d\n",
		   ar->pdev->pdev_id, ar->nss.ctx, ar->nss.if_num);

	if (!of_property_read_u32(dev->of_node, "nss-radio-priority", &nss_radio_priority)) {
		scheme_id = nss_wifili_thread_scheme_alloc(ab->nss.ctx, ar->nss.if_num, nss_radio_priority);
		if (scheme_id == WIFILI_SCHEME_ID_INVALID) {
			ath11k_warn(ab, "received invalid scheme_id, configuring default value\n");
			scheme_id = 0;
		}
	}
	ath11k_dbg(ab, ATH11K_DBG_NSS, "ifnum: %d scheme_id: %d nss_radio_priority: %d\n", ar->nss.if_num, scheme_id, nss_radio_priority);

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg) {
		ret = -ENOMEM;
		goto unregister;
	}

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	pdevmsg = &wlmsg->msg.pdevmsg;

	pdevmsg->radio_id = radio_id;
	pdevmsg->lmac_id = ar->lmac_id;
	pdevmsg->target_pdev_id = ar->pdev->pdev_id;
	pdevmsg->num_rx_swdesc = WIFILI_RX_DESC_POOL_WEIGHT * DP_RXDMA_BUF_RING_SIZE;
	pdevmsg->scheme_id = scheme_id;

	/* Store rxdma ring info to the message */
	refill_ring_id = ar->dp.rx_refill_buf_ring.refill_buf_ring.ring_id;
	ath11k_nss_fill_srng_info(ab, refill_ring_id, &pdevmsg->rxdma_ring);

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_PDEV_INIT_MSG,
			 sizeof(struct nss_wifili_pdev_init_msg),
			 msg_cb, NULL);

	reinit_completion(&ab->nss.complete);

	/* Note: response is contention free during init sequence */
	ab->nss.response = ATH11K_NSS_MSG_ACK;

	status = nss_wifili_tx_msg(ar->nss.ctx, wlmsg);

	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss send pdev msg tx error : %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ret = wait_for_completion_timeout(&ab->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ab, "timeout while waiting for pdev init msg response\n");
		ret = -ETIMEDOUT;
		goto free;
	}

	/* Check if the response is success from the callback */
	if (ab->nss.response != ATH11K_NSS_MSG_ACK) {
		ret = -EINVAL;
		goto free;
	}

	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	kfree(wlmsg);

	/* Disable nss sojourn stats by default */
	ath11k_nss_sojourn_stats_disable(ar);
	/* Enable nss wifili peer stats by default */
	ath11k_nss_peer_stats_enable(ar);

	/*TODO CFG Tx buffer limit as per no clients range per radio
	 * this needs to be based on target/mem cfg
	 * similar to tx desc cfg at soc init per radio
	 */

	for (i = 0; i < ATH11K_NSS_RADIO_TX_LIMIT_RANGE; i++)
		ath11k_nss_radio_buf_cfg(ar, i, ATH11K_NSS_RADIO_TX_LIMIT_RANGE3);

	return 0;

free:
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(struct nss_wifili_msg));
	kfree(wlmsg);
unregister:
	nss_unregister_wifili_radio_if(ar->nss.if_num);
dealloc:
	nss_dynamic_interface_dealloc_node(ar->nss.if_num, dyn_if_type);
	return ret;
}

/* TODO : Check if start, reset and stop messages can be done using single function as
 * body is similar, having it now for clarity */

int ath11k_nss_start(struct ath11k_base *ab)
{
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret;

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	/* Empty message for NSS Start message */
	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_START_MSG,
			 0,
			 msg_cb, NULL);

	reinit_completion(&ab->nss.complete);

	/* Note: response is contention free during init sequence */
	ab->nss.response = ATH11K_NSS_MSG_ACK;

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);

	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss send start msg tx error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ret = wait_for_completion_timeout(&ab->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ab, "timeout while waiting for response for nss start msg\n");
		ret = -ETIMEDOUT;
		goto free;
	}

	/* Check if the response is success from the callback */
	if (ab->nss.response != ATH11K_NSS_MSG_ACK) {
		ret = -EINVAL;
		goto free;
	}

	/* NSS Start success */
	ret = 0;
	ath11k_dbg(ab, ATH11K_DBG_NSS, "nss start success\n");

free:
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(struct nss_wifili_msg));
	kfree(wlmsg);
	return ret;
}

static void ath11k_nss_reset(struct ath11k_base *ab)
{
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret;

	ath11k_dbg(ab, ATH11K_DBG_NSS, "NSS reset\n");
	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg) {
		ath11k_warn(ab, "mem allocation failure during nss reset\n");
		return;
	}

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	/* Empty message for NSS Reset message */
	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_SOC_RESET_MSG,
			 0,
			 msg_cb, NULL);

	reinit_completion(&ab->nss.complete);

	/* Note: response is contention free during deinit sequence */
	ab->nss.response = ATH11K_NSS_MSG_ACK;

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);

	/* Add a retry mechanism to reset nss until success */
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss send reset msg tx error %d\n", status);
		goto free;
	}

	ret = wait_for_completion_timeout(&ab->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ab, "timeout while waiting for response for nss reset msg\n");
		goto free;
	}

	/* Check if the response is success from the callback */
	if (ab->nss.response != ATH11K_NSS_MSG_ACK) {
		ath11k_warn(ab, "failure response during nss reset %d\n", ab->nss.response);
		goto free;
	}

	/* Unregister wifili interface */
	nss_unregister_wifili_if(ab->nss.if_num);

free:
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(struct nss_wifili_msg));
	kfree(wlmsg);
}

static int ath11k_nss_stop(struct ath11k_base *ab)
{
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int ret;

	ath11k_dbg(ab, ATH11K_DBG_NSS, "NSS stop\n");
	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	/* Empty message for Stop command */
	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_STOP_MSG,
			 0,
			 msg_cb, NULL);

	reinit_completion(&ab->nss.complete);

	/* Note: response is contention free during deinit sequence */
	ab->nss.response = ATH11K_NSS_MSG_ACK;

	status = nss_wifili_tx_msg(ab->nss.ctx, wlmsg);

	/* Add a retry mechanism to stop nss until success */
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss send stop msg tx error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ret = wait_for_completion_timeout(&ab->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ab, "timeout while waiting for response for nss stop msg\n");
		ret = -ETIMEDOUT;
		goto free;
	}

	/* Check if the response is success from the callback */
	if (ab->nss.response != ATH11K_NSS_MSG_ACK) {
		ret = -EINVAL;
		goto free;
	}

	/* NSS Stop success */
	ret = 0;
free:
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	kfree(wlmsg);
	return ret;
}

int ath11k_nss_pdev_deinit(struct ath11k_base *ab, int radio_id)
{
	struct ath11k *ar = ab->pdevs[radio_id].ar;
	struct nss_wifili_pdev_deinit_msg *deinit;
	struct nss_wifili_msg *wlmsg = NULL;
	nss_wifili_msg_callback_t msg_cb;
	nss_tx_status_t status;
	int dyn_if_type;
	int ret;

	ath11k_dbg(ab, ATH11K_DBG_NSS, "NSS pdev %d deinit\n", radio_id);
	dyn_if_type = ath11k_nss_get_dynamic_interface_type(ab);

	/* Disable NSS wifili peer stats before teardown */
	if (ab->nss.stats_enabled)
		ath11k_nss_peer_stats_disable(ar);

	wlmsg = kzalloc(sizeof(struct nss_wifili_msg), GFP_ATOMIC);
	if (!wlmsg)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, sizeof(struct nss_wifili_msg));

	deinit = &wlmsg->msg.pdevdeinit;
	deinit->ifnum = radio_id;

	msg_cb = (nss_wifili_msg_callback_t)ath11k_nss_wifili_event_receive;

	nss_cmn_msg_init(&wlmsg->cm, ab->nss.if_num,
			 NSS_WIFILI_PDEV_DEINIT_MSG,
			 sizeof(struct nss_wifili_pdev_deinit_msg),
			 msg_cb, NULL);

	reinit_completion(&ab->nss.complete);

	/* Note: response is contention free during deinit sequence */
	ab->nss.response = ATH11K_NSS_MSG_ACK;

	status = nss_wifili_tx_msg(ar->nss.ctx, wlmsg);
	if (status != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "nss send pdev deinit msg tx error %d\n", status);
		ret = -EINVAL;
		goto free;
	}

	ret = wait_for_completion_timeout(&ab->nss.complete,
					  msecs_to_jiffies(ATH11K_NSS_MSG_TIMEOUT_MS));
	if (!ret) {
		ath11k_warn(ab, "timeout while waiting for pdev deinit msg response\n");
		ret = -ETIMEDOUT;
		goto free;
	}

	/* Check if the response is success from the callback */
	if (ab->nss.response != ATH11K_NSS_MSG_ACK) {
		ret = -EINVAL;
		goto free;
	}

	/* pdev deinit msg success, dealloc, deregister and return */
	ret = 0;

	/* reset thread scheme*/
	nss_wifili_thread_scheme_dealloc(ab->nss.ctx, ar->nss.if_num);

	nss_dynamic_interface_dealloc_node(ar->nss.if_num, dyn_if_type);
	nss_unregister_wifili_radio_if(ar->nss.if_num);
free:
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(struct nss_wifili_msg));
	kfree(wlmsg);
	return ret;
}

int ath11k_nss_teardown(struct ath11k_base *ab)
{
	int i, ret;

	if (!ab->nss.enabled)
		return 0;

	ath11k_nss_stop(ab);

	for (i = 0; i < ab->num_radios ; i++) {
		ret = ath11k_nss_pdev_deinit(ab, i);
		if (ret)
			ath11k_warn(ab, "failure during pdev%d deinit\n", i);
	}

	ath11k_nss_reset(ab);
	ath11k_nss_tx_desc_mem_free(ab);
	ath11k_dbg(ab, ATH11K_DBG_NSS, "NSS Teardown Complete\n");

	return 0;
}

int ath11k_nss_setup(struct ath11k_base *ab)
{
	int i;
	int ret = 0;
	u32 target_type;

	if (!ab->nss.enabled)
		return 0;

	target_type = ath11k_nss_get_target_type(ab);

	if (target_type == ATH11K_WIFILI_TARGET_TYPE_UNKNOWN)
		return -ENOTSUPP;

	/* Verify NSS support is enabled */
	if (nss_cmn_get_nss_enabled() == false) {
		ath11k_warn(ab, "NSS offload support disabled, falling back to default mode\n");
		return -ENOTSUPP;
	}

	/* Initialize completion for verifying NSS message response */
	init_completion(&ab->nss.complete);

	/* Setup common resources for NSS */
	ret = ath11k_nss_init(ab);
	if (ret) {
		ath11k_warn(ab, "NSS SOC Initialization Failed :%d\n", ret);
		goto fail;
	}

	/* Setup pdev related resources for NSS */
	for (i = 0; i < ab->num_radios; i++) {
		ret = ath11k_nss_pdev_init(ab, i);
		if (ret) {
			ath11k_warn(ab, "NSS PDEV %d Initialization Failed :%d\n", i, ret);
			goto pdev_deinit;
		}
	}

	/* Set the NSS statemachine to start */
	ret = ath11k_nss_start(ab);
	if (ret) {
		ath11k_warn(ab, "NSS Start Failed : %d\n", ret);
		goto pdev_deinit;
	}

	/* Default nexthop interface is set to ETH RX */
	ret = nss_wifi_vdev_base_set_next_hop(ab->nss.ctx, NSS_ETH_RX_INTERFACE);
	if (ret != NSS_TX_SUCCESS) {
		ath11k_warn(ab, "Failure to set default next hop : %d\n", ret);
		goto stop;
	}

	ath11k_dbg(ab, ATH11K_DBG_NSS, "NSS Setup Complete\n");
	return ret;

stop:
	ath11k_nss_stop(ab);

pdev_deinit:
	for (i -= 1; i >= 0; i--)
		ath11k_nss_pdev_deinit(ab, i);

	ath11k_nss_reset(ab);
	ath11k_nss_tx_desc_mem_free(ab);
fail:
	return ret;
}
