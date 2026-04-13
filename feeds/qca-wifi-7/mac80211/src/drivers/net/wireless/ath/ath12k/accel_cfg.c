// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "debug.h"
#include "accel_cfg.h"
#include "core.h"
#include "peer.h"
#include "sdwf.h"
#include "wifi7/hal_rx.h"
#include <linux/module.h>
#include <linux/if_vlan.h>

static inline
u32 ath_encode_mlo_metadata(u8 link_id)
{
	u32 mlo_metadata;

	mlo_metadata =  u32_encode_bits(ATH12K_MLO_METADATA_VALID,
					ATH12K_MLO_METADATA_VALID_MASK) |
			u32_encode_bits(ATH12K_MLO_METADATA_TAG,
					ATH12K_MLO_METADATA_TAG_MASK) |
			u32_encode_bits(link_id,
					ATH12K_MLO_METADATA_LINKID_MASK);
	return mlo_metadata;
}

/**
 * ath12k_get_ingress_mlo_dev_info() - Retrieve node id
 * @ndev: pointer to corresponding net_device
 * @peer_mac: peer mac address
 * @link_id: Buffer to fill link id
 * @node_id: Buffer to fill the node id
 * Return: true - success, false - failure
 */
void ath12k_get_ingress_mlo_dev_info(struct net_device *ndev,
				     struct  wireless_dev *wdev,
				     const u8 *peer_mac,
				     u8 *node_id, u8 *link_id)
{
	struct  ath12k_vif *ahvif;
	struct  ieee80211_sta *sta;
	struct  ath12k_sta *ahsta;
	struct  ath12k_link_vif *arvif;
	struct  ath12k_base *ab;
	struct  ieee80211_vif *vif;

	vif = wdev_to_ieee80211_vif_vlan(wdev, false);

	if (!vif)
		return;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return;

	if (vif->type == NL80211_IFTYPE_MESH_POINT) {
		*link_id = 0;
		return;
	}

	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_AP)
		return;

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);

		if (!sta) {
			pr_warn("ieee80211_sta is null");
			return;
		}
	} else if (ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
		sta = ieee80211_find_sta(vif, peer_mac);
		if (!sta) {
			sta = wdev_to_ieee80211_vlan_sta(wdev);
			if (!sta)
				return;
		}
	}
	ahsta = ath12k_sta_to_ahsta(sta);

	rcu_read_lock();
	arvif  = (!sta->mlo) ? rcu_dereference(ahvif->link[ahsta->deflink.link_id]) :
			       rcu_dereference(ahvif->link[ahsta->primary_link_id]);

	*link_id = (!sta->mlo) ? ahsta->deflink.link_id : ahsta->primary_link_id;

	ab = arvif->ar->ab;

	/* Update DS node_id only if the chipset support DS */
	if (ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS ||
	    !test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		goto unlock;

	*node_id = ab->dp->ppe.ds_node_id;
unlock:
	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_MAC,
			"Wifi-classifer mark peer %pM link_id %x node_id %x\n",
			peer_mac, *link_id, *node_id);
}

struct wireless_dev *ath12k_get_wdev_from_netdev(struct net_device *dev)
{
	struct  wireless_dev *wdev = NULL;

	wdev = dev->ieee80211_ptr;
	if (!wdev) {
		/* If the netdev is vlan, it may not have ieee80211_ptr.
		 * In that case fetch the ieee80211_ptr from its top most parent
		 */
		if (is_vlan_dev(dev)) {
			struct net_device *parent_ndev =
				vlan_dev_real_dev(dev);

			if (parent_ndev)
				wdev = parent_ndev->ieee80211_ptr;

			if (!wdev)
				return NULL;
		} else {
			return NULL;
		}
	}

	return wdev;
}

u32 ath12k_get_metadata_info(struct ath_dp_metadata_param *md_param)
{
	struct net_device *dest_dev = NULL;
	struct  wireless_dev *wdev = NULL;
	struct  ieee80211_hw *hw;
	u32 metadata = 0;
	u16 msduq_peer;
	u8 *dest_mac = NULL;
	u8 link_id = ATH12k_MLO_LINK_ID_INVALID;
	u8 node_id = ATH12k_DS_NODE_ID_INVALID;

	if (md_param->is_mlo_param_valid) {
		dest_dev = md_param->mlo_param.in_dest_dev;
		dest_mac = md_param->mlo_param.in_dest_mac;
	}

	if (md_param->is_sawf_param_valid) {
		if (!md_param->is_mlo_param_valid) {
			dest_dev = md_param->sawf_param.netdev;
			dest_mac = md_param->sawf_param.peer_mac;
		}
	}

	if (!dest_dev || !dest_mac) {
		return metadata;
	}

	wdev = ath12k_get_wdev_from_netdev(dest_dev);

	if (!wdev)
		return metadata;

	hw = wiphy_to_ieee80211_hw(wdev->wiphy);
	if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION)) {
		ath12k_err(NULL, "ECM cb not supported");
		return metadata;
	}

	ath12k_get_ingress_mlo_dev_info(dest_dev, wdev, dest_mac,
					&node_id, &link_id);

	/* Update node_id only in case of DS Mode and return metadata */
	if (node_id != ATH12k_DS_NODE_ID_INVALID)
		md_param->mlo_param.out_ppe_ds_node_id = node_id;

	/* Encode MLO metadata only if link_id updated */
	if (link_id != ATH12k_MLO_LINK_ID_INVALID)
		metadata |= ath_encode_mlo_metadata(link_id);

	if (md_param->is_sawf_param_valid) {
		msduq_peer = ath12k_sdwf_get_msduq_peer(wdev, dest_mac,
							&md_param->sawf_param,
							md_param->is_scs_mscs);
		 /* Encode SDWF metadata only if msduq_id updated */
		if (msduq_peer != SDWF_PEER_MSDUQ_INVALID)
			metadata |= ath_encode_sdwf_metadata(msduq_peer);
	}

	return metadata;
}

static
void ath12_sdwf_ul_config_peer(struct ieee80211_vif *vif,
			       struct wireless_dev *wdev,
			       u8 *mac, u8 start_or_stop,
			       u16 svc_id)
{

	struct ath12k *ar = NULL;
	u16 qos_id, peer_id;
	struct ath12k_qos_ctx *qos_ctx;

	ar = ath12k_sdwf_get_ar_from_vif(wdev, vif,
					 mac, &peer_id);

	if (!ar) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "sdwf_ul_config, ar is null\n");
		return;
	}

	if (!ath12k_sdwf_service_configured(ar->ab, svc_id))
		return;

	qos_id = ath12k_sdwf_get_ul_qos_id(ar->ab, svc_id);
	if (qos_id == QOS_ID_INVALID)
		return;

	qos_ctx = ath12k_get_qos(ar->ab);
	if (!qos_ctx)
		return;

	ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
		   "UL SDWF Configure: svc id:%d", svc_id);

	ath12k_core_config_ul_qos(ar, &qos_ctx->profiles[qos_id].params,
				  qos_id, mac, start_or_stop);
}

int ath12k_get_mscs_priority(struct ath_mscs_get_priority_param *params)
{
	struct ath12k_dp_peer *src_peer, *dst_peer;
	u8 priority, user_bitmap, user_limit;
	struct sk_buff *skb = params->skb;
	struct ieee80211_vif *src_vif;
	struct wireless_dev *src_wdev;
	struct ath12k *ar = NULL;
	struct ieee80211_hw *hw;
	struct ath12k_base *ab;
	u16 peer_id;
	int status;

	src_wdev = ath12k_get_wdev_from_netdev(params->src_dev);

	if (!src_wdev) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "src wdev is null");
		return -1;
	}

	hw = wiphy_to_ieee80211_hw(src_wdev->wiphy);
	if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION)) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "hw1 is null");
		return -1;
	}

	src_vif = wdev_to_ieee80211_vif_vlan(src_wdev, false);

	if (!src_vif) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "src vif is null");
		return -1;
	}

	ar = ath12k_sdwf_get_ar_from_vif(src_wdev, src_vif,
					 params->src_mac, &peer_id);
	if (!ar) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "get_mscs, ar is null\n");
		return -1;
	}

	ab = ar->ab;

	spin_lock_bh(&ab->dp->dp_lock);
	src_peer = ath12k_dp_peer_find(&ar->ah->dp_hw, params->src_mac);
	if (!src_peer) {
		dst_peer = ath12k_dp_peer_find(&ar->ah->dp_hw,
							     params->dst_mac);
		if (dst_peer) {
			if (dst_peer->mscs_session_exists &&
			    !skb->priority) {
				/**
				 * This is a downlink flow with priority 0 for MSCS
				 * client, this should not be accelerated as uplink
				 * flow is the one which needs to be accelerated first
				 */
				status = ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_DENY_QOS_TAG_UPDATE;
				goto skip_priority_update;
			} else {
			    status = ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_ALLOW_INVALID_QOS_TAG_UPDATE;
			    goto skip_priority_update;
			}
		}
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Peer: %pM not present\n", params->dst_mac);
		status = ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_PEER_NOT_FOUND;
		goto skip_priority_update;
	}

	if (!src_peer || !src_peer->mscs_session_exists) {
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Peer: %pM does not have an MSCS session\n",
			   params->src_mac);
		status = ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_ALLOW_INVALID_QOS_TAG_UPDATE;
		goto skip_priority_update;
	}

	user_bitmap = src_peer->mscs_ctxt.user_priority_bitmap;
	user_limit = src_peer->mscs_ctxt.user_priority_limit;
	priority = skb->priority & ATH12K_DP_MSCS_VALID_TID_MASK;

	if (!(BIT(priority) & user_bitmap)) {
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "MSCS: tid %u does match with bitmap 0x%x\n",
			   priority, user_bitmap);
		status = ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_DENY_QOS_TAG_UPDATE;
		goto skip_priority_update;
	}

	priority = min(priority, user_limit);
	ath12k_dbg(ab, ATH12K_DBG_QOS, "MSCS: tid for this MSCS session is %u\n",
		   priority);

	skb->priority = priority;
	spin_unlock_bh(&ab->dp->dp_lock);
	return ATH12K_DP_MSCS_PEER_LOOKUP_STATUS_ALLOW_MSCS_QOS_TAG_UPDATE;

skip_priority_update:
	spin_unlock_bh(&ab->dp->dp_lock);
	return status;
}

void ath12k_sdwf_ul_config(struct ath_ul_params *params)
{
	struct ieee80211_hw *hw;
	struct ieee80211_vif *dest_vif, *src_vif;
	struct wireless_dev *dest_wdev, *src_wdev;

	src_wdev = ath12k_get_wdev_from_netdev(params->src_dev);
	dest_wdev = ath12k_get_wdev_from_netdev(params->dst_dev);

	if (!src_wdev && !dest_wdev) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "wdev src_and_dest are null");
		return;
	}

	if (src_wdev) {
		hw = wiphy_to_ieee80211_hw(src_wdev->wiphy);
		if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION)) {
			ath12k_dbg(NULL, ATH12K_DBG_QOS, "hw1 is null");
			return;
		}
	}

	if (dest_wdev) {
		hw = wiphy_to_ieee80211_hw(dest_wdev->wiphy);
		if (!ieee80211_hw_check(hw, SUPPORT_ECM_REGISTRATION)) {
			ath12k_dbg(NULL, ATH12K_DBG_QOS, "hw2 is null");
			return;
		}
	}

	if (dest_wdev)
		dest_vif = wdev_to_ieee80211_vif_vlan(dest_wdev, false);
	else
		dest_vif = NULL;
	if (src_wdev)
		src_vif = wdev_to_ieee80211_vif_vlan(src_wdev, false);
	else
		src_vif = NULL;

	if (!dest_vif && !src_vif) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "src_and_dest vif is null");
		return;
	}

	if (src_vif) {
		ath12_sdwf_ul_config_peer(src_vif, src_wdev,
					  params->src_mac,
					  params->start_or_stop,
					  params->fw_service_id);
	}
	if (dest_vif) {
		ath12_sdwf_ul_config_peer(dest_vif, dest_wdev,
					  params->src_mac,
					  params->start_or_stop,
					  params->rv_service_id);
	}

	if (params->fw_mark_metadata != SDWF_METADATA_INVALID &&
	    (params->start_or_stop == FLOW_STOP)) {
		ath12k_sdwf_3_link_peer_dl_flow_count(dest_wdev, dest_vif,
						      params->dst_mac,
						      params->fw_mark_metadata,
						      params->fw_service_id);
	}

	if (params->rv_mark_metadata != SDWF_METADATA_INVALID &&
	    (params->start_or_stop == FLOW_STOP)) {
		ath12k_sdwf_3_link_peer_dl_flow_count(src_wdev, src_vif,
						      params->src_mac,
						      params->rv_mark_metadata,
						      params->rv_service_id);
	}

}

/**
 * ath12k_ds_get_node_id() - Retrieve ds node id
 * @vif: pointer to corresponding ieee80211_vif
 * @peer_mac: peer mac address
 * @node_id: Buffer to fill the node id
 *
 * Return: true - success, false - failure
 */
static bool ath12k_ds_get_node_id(struct ieee80211_vif *vif,
				  struct wireless_dev *wdev,
				  const u8 *peer_mac, u8 *node_id)
{
	struct ath12k_vif *ahvif;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;

	if (!vif || !wdev)
		return false;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return false;

	if (ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return false;

	if (ahvif->vdev_type != WMI_VDEV_TYPE_STA &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_AP)
		return false;

	rcu_read_lock();

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);
		if (!sta) {
			pr_err("ieee80211_sta is null");
			goto unlock_n_fail;
		}
	} else if (ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
		sta = ieee80211_find_sta(vif, peer_mac);
		if (!sta) {
			sta = wdev_to_ieee80211_vlan_sta(wdev);
			if (!sta)
				goto unlock_n_fail;
		}
	}

	ahsta = ath12k_sta_to_ahsta(sta);
	arvif = (!sta->mlo) ? ahvif->link[ahsta->deflink.link_id] :
			ahvif->link[ahsta->assoc_link_id];

	if (!arvif)
		goto unlock_n_fail;

	ab = arvif->ar->ab;

	/* Update and return DS node_id only if the chipset support DS*/
	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) ||
	    arvif->ppe_vp_profile_idx == ATH12K_INVALID_VP_PROFILE_IDX)
		goto unlock_n_fail;

	*node_id = ab->dp->ppe.ds_node_id;

	rcu_read_unlock();

	return true;

unlock_n_fail:
	rcu_read_unlock();
	return false;
}

static const struct ath_dp_accel_cfg_ops ath_dp_accel_cfg_ops_obj = {
	.ppeds_get_node_id = ath12k_ds_get_node_id,
	.get_metadata_info = ath12k_get_metadata_info,
	.sdwf_ul_config = ath12k_sdwf_ul_config,
	.get_mscs_priority = ath12k_get_mscs_priority,
};

/**
 * ath12k_dp_accel_cfg_init() - Initialize dp_accel_cfg context
 * @ab: ath12k_base handle
 *
 * Return: None
 */
void ath12k_dp_accel_cfg_init(struct ath12k_base *ab)
{
	const struct ath_dp_accel_cfg_ops *ath_dp_accel_cfg_ops_ptr;

	ath_dp_accel_cfg_ops_ptr = &ath_dp_accel_cfg_ops_obj;

	if (ath_dp_accel_cfg_ops_callback_register(ath_dp_accel_cfg_ops_ptr))
		return;

	ath12k_dbg(ab, ATH12K_DBG_DP_RX, "cfg accel context initialized\n");
}

/**
 * ath12k_dp_accel_cfg_deinit() - Deinitialize dp_accel_cfg context
 * @ab: ath12k_base handle
 *
 * Return: None
 */
void ath12k_dp_accel_cfg_deinit(struct ath12k_base *ab)
{
	ath_dp_accel_cfg_ops_callback_unregister();

	ath12k_dbg(ab, ATH12K_DBG_DP_RX, "DS context deinitialized\n");
}
