/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "core.h"
#include "sdwf.h"
#include "dp_peer.h"
#include "debug.h"
#include "htc.h"
#include <linux/module.h>
#include "debugfs.h"
#include "telemetry_agent_if.h"

bool ath12k_sdwf_service_configured(struct ath12k_base *ab, u16 svc_id)
{
	struct ath12k_qos_ctx *qos_ctx;
	bool status;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return false;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return false;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	status = qos_ctx->svc_class[svc_id].configured;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Configured:%d",
		   svc_id,
		   qos_ctx->svc_class[svc_id].configured);
	spin_unlock_bh(&qos_ctx->profile_lock);
	return status;
}
static u32
ath12k_get_enabled_sdwf_params_mask(struct ath12k_qos_params *qos_params)
{
	u32 enabled_sdwf_params = 0;

	if (qos_params->min_data_rate != QOS_PARAM_DEFAULT_MIN_THROUGHPUT)
		enabled_sdwf_params |= (1 << SDWF_PARAM_MIN_THROUGHPUT);
	if (qos_params->mean_data_rate != QOS_PARAM_DEFAULT_MAX_THROUGHPUT)
		enabled_sdwf_params |= (1 << SDWF_PARAM_MAX_THROUGHPUT);
	if (qos_params->burst_size != QOS_PARAM_DEFAULT_BURST_SIZE)
		enabled_sdwf_params |= (1 << SDWF_PARAM_BURST_SIZE);
	if (qos_params->min_service_interval != QOS_PARAM_DEFAULT_SVC_INTERVAL)
		enabled_sdwf_params |= (1 << SDWF_PARAM_SERVICE_INTERVAL);
	if (qos_params->delay_bound != QOS_PARAM_DEFAULT_DELAY_BOUND)
		enabled_sdwf_params |= (1 << SDWF_PARAM_DELAY_BOUND);
	if (qos_params->msdu_life_time != QOS_PARAM_DEFAULT_TIME_TO_LIVE)
		enabled_sdwf_params |= (1 << SDWF_PARAM_MSDU_TTL);
	if (qos_params->msdu_delivery_info != QOS_PARAM_DEFAULT_MSDU_LOSS_RATE)
		enabled_sdwf_params |= (1 << SDWF_PARAM_MSDU_LOSS);

	return enabled_sdwf_params;

}

static u32 ath12k_get_enabled_param_mask(struct ath12k_base *ab, u8 qos_id)
{
	struct ath12k_qos_ctx *qos_ctx;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx)
		return 0;

	return ath12k_get_enabled_sdwf_params_mask(&qos_ctx->profiles[qos_id].params);
}


int ath12k_sdwf_map_service_class(struct ath12k_base *ab, u16 svc_id,
				  u16 dl_qos_id,
				  u16 ul_qos_id)
{
	int ret = -EINVAL;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return ret;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return ret;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	qos_ctx->svc_class[svc_id].dl_qos_id = dl_qos_id;
	qos_ctx->svc_class[svc_id].ul_qos_id = ul_qos_id;
	qos_ctx->svc_class[svc_id].configured = true;

	if (dl_qos_id != QOS_ID_INVALID)
		qos_ctx->profiles[dl_qos_id].svc_id = svc_id;

	if (ul_qos_id != QOS_ID_INVALID)
		qos_ctx->profiles[ul_qos_id].svc_id = svc_id;

	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Config | DL QoS ID:%d | UL QoS ID:%d",
		   svc_id,
		   qos_ctx->svc_class[svc_id].dl_qos_id,
		   qos_ctx->svc_class[svc_id].ul_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return 0;
}

int ath12k_sdwf_unmap_service_class(struct ath12k_base *ab, u16 svc_id)
{
	int ret = -EINVAL;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return ret;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return ret;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	qos_ctx->svc_class[svc_id].dl_qos_id = QOS_ID_INVALID;
	qos_ctx->svc_class[svc_id].ul_qos_id = QOS_ID_INVALID;
	qos_ctx->svc_class[svc_id].configured = false;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Disable| DL QoS ID:%d | UL QoS ID:%d",
		   svc_id,
		   qos_ctx->svc_class[svc_id].dl_qos_id,
		   qos_ctx->svc_class[svc_id].ul_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return 0;
}

u16 ath12k_sdwf_get_dl_qos_id(struct ath12k_base *ab, u16 svc_id)
{
	u16 dl_qos_id = QOS_ID_INVALID;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return dl_qos_id;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return dl_qos_id;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	dl_qos_id = qos_ctx->svc_class[svc_id].dl_qos_id;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Get DL QoS ID:%d",
		   svc_id,
		   dl_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return dl_qos_id;
}

u16 ath12k_sdwf_get_ul_qos_id(struct ath12k_base *ab, u16 svc_id)
{
	u16 ul_qos_id = QOS_ID_INVALID;
	struct ath12k_qos_ctx *qos_ctx;

	if (svc_id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Service Class ID: %d is invalid", svc_id);
		return ul_qos_id;
	}

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return ul_qos_id;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	ul_qos_id = qos_ctx->svc_class[svc_id].ul_qos_id;
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Service Class %d Get DL QoS ID:%d",
		   svc_id,
		   ul_qos_id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	return ul_qos_id;
}

static struct ath12k_dp_peer_qos*
ath12k_sdwf_get_qos_ctx(struct ath12k_base *ab,
			struct ath12k_dp_peer *peer)
{
	struct ath12k_dp_peer_qos *peer_qos = peer->qos;

	if (!peer_qos)
		peer_qos = ath12k_dp_peer_qos_alloc(ab->dp, peer);

	return peer_qos;
}

void ath12k_get_peer_sla_config(struct ath12k_base *ab,
				struct ath12k_dp_link_peer *peer, u16 *sla_mask)
{
	struct ath12k_dp_peer_qos *peer_ctx = NULL;
	struct ath12k_qos_ctx *sawf_ctx;
	int tid, q_id;
	struct ath12k_msduq *msduq_map;

	lockdep_assert_held(&ab->dp->dp_lock);

	sawf_ctx = ath12k_get_qos(ab);
	if (!sawf_ctx)
		return;

	peer_ctx = ath12k_sdwf_get_qos_ctx(ab, peer->dp_peer);
	for (tid = 0; tid < QOS_TID_MAX; tid++) {
		for (q_id = 0; q_id < QOS_TID_MDSUQ_MAX; q_id++) {
			msduq_map  =  &peer_ctx->msduq_map[tid][q_id];
			if (msduq_map->reserved) {
				*sla_mask |= ath12k_get_enabled_param_mask(ab,
								msduq_map->qos_id);
			}

		}
	}
}

static u8 ath12k_sdwf_alloc_msduq(struct ath12k *ar, u32 svc_id,
				  u16 peer_id, bool scs)
{
	struct ath12k_qos_ctx *qos_ctx;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_base *ab = ar->ab;
	u16 qos_id;
	u8 scs_id; u8 qos_tag;
	u16 msduq = QOS_INVALID_MSDUQ;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return msduq;
	}

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(ab->dp, peer_id);
	if (!peer) {
		ath12k_err(ab, "Unable to find peer");
		goto ret;
	}

	if (!peer->dp_peer) {
		ath12k_err(ab, "dp_peer is NULL for peer_id %u", peer_id);
		goto ret;
	}

	qos = ath12k_sdwf_get_qos_ctx(ab, peer->dp_peer);
	if (!qos) {
		ath12k_err(ab, "Unable to find peer qos ctx");
		goto ret;
	}

	if  (scs) {
		qos_tag = u32_get_bits(svc_id, SCS_PROTOCOL_MASK);
		if (qos_tag == QOS_SCS_TAG) {
			scs_id = u32_get_bits(svc_id, SCS_SVC_ID_MASK);
			ath12k_dp_peer_scs_data(ab->dp, qos, scs_id,
						&msduq, &qos_id);
			goto ret;
		} else {
			msduq = u32_get_bits(svc_id, SCS_SVC_ID_MASK);
			goto ret;
		}
	}

	qos_id = ath12k_sdwf_get_dl_qos_id(ab, svc_id);
	if (qos_id == QOS_ID_INVALID)
		goto ret;

	msduq = ath12k_dp_peer_qos_msduq(ab, qos, peer, ar, qos_id, svc_id);
ret:
	spin_unlock_bh(&ab->dp->dp_lock);
	return msduq;
}

u32 ath_encode_sdwf_metadata(u16 msduq_id)
{
	u32 sawf_metadata = 0;

	sawf_metadata = u32_encode_bits(SDWF_VALID,
					SDWF_VALID_MASK) |
	u32_encode_bits(SDWF_VALID_TAG,
			SDWF_TAG_ID) |
	u32_encode_bits(msduq_id,
			SDWF_PEER_MSDUQ_ID);
	return sawf_metadata;
}

struct ath12k *ath12k_sdwf_get_ar_from_vif(struct wireless_dev *wdev,
					   struct ieee80211_vif *vif,
					   u8 *peer_mac, u16 *peer_id)
{
	struct ath12k_base *ab = NULL;
	struct ath12k *ar = NULL;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	u8 mac_addr[ETH_ALEN] = { 0 };
	u8 link_id;
	struct ath12k_dp_link_peer *peer;

	if (!wdev)
		return NULL;

	if (!vif)
		return NULL;

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return NULL;

	sta = ieee80211_find_sta_by_ifaddr(ahvif->ah->hw, peer_mac, NULL);
	if (!sta) {
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Peer: %pM not present", peer_mac);
		sta = wdev_to_ieee80211_vlan_sta(wdev);
		if (!sta)
			return NULL;
	}

	ahsta = (struct ath12k_sta *)sta->drv_priv;
	if (!ahsta)
		return NULL;

	rcu_read_lock();
	if (sta->mlo) {
		link_id = ahsta->primary_link_id;
		memcpy(mac_addr, ahsta->link[link_id]->addr, ETH_ALEN);
	} else if (sta->valid_links) {
		link_id = ahsta->deflink.link_id;
		memcpy(mac_addr, peer_mac, ETH_ALEN);
	} else {
		link_id = 0;
		memcpy(mac_addr, peer_mac, ETH_ALEN);
	}

	arvif = rcu_dereference(ahvif->link[link_id]);

	if (!arvif) {
		rcu_read_unlock();
		return NULL;
	}

	ar = arvif->ar;
	if (!ar) {
		rcu_read_unlock();
		return NULL;
	}
	rcu_read_unlock();

	ab = ar->ab;

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_addr(ab->dp, mac_addr);
	if (!peer) {
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Peer: %pM not present\n", mac_addr);
		spin_unlock_bh(&ab->dp->dp_lock);
		return NULL;
	}

	*peer_id = peer->peer_id;

	spin_unlock_bh(&ab->dp->dp_lock);
	return ar;
}

u32 dscp_tid_map[WMI_HOST_DSCP_MAP_MAX] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 0, 1, 0, 1, 0, 1,
        0, 2, 3, 2, 3, 2, 3, 2,
        4, 3, 4, 3, 4, 3, 4, 3,
        4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 6, 5, 6, 5,
        7, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7,
};

static inline u8
ath12k_sdwf_get_3_link_best_suitable_tid(struct ath12k_dp_link_peer *peer,
                                         u8 tid1, u8 tid2)
{
        u64 tid1_weight = 0;
        u64 tid2_weight = 0;

        /* if no weights are feeded fall back to equal distribution */
        if (!peer->tid_weight[tid1] || !peer->tid_weight[tid2]) {
                tid1_weight = peer->flow_cnt[tid1];
                tid2_weight = peer->flow_cnt[tid2];
                goto tid_selection;
        }

        /* for the first two flows fall back to equal distribution */
        if (!peer->flow_cnt[tid1] || !peer->flow_cnt[tid2]) {
                tid1_weight = peer->flow_cnt[tid1];
                tid2_weight = peer->flow_cnt[tid2];
                goto tid_selection;
        }

        /* weight based distribution */
        tid1_weight = peer->flow_cnt[tid1] * peer->tid_weight[tid2];
        tid2_weight = peer->flow_cnt[tid2] * peer->tid_weight[tid1];

tid_selection:
        if (tid1_weight > tid2_weight)
                return tid2;
	else
                return tid1;
}

static u8 ath12k_sdwf_get_3_link_queue_id(struct ath12k_dp *dp,
					  struct ath12k_dp_link_peer *peer,
                                          u8 tid)
{
        u8 queue_id = QOS_INVALID_MSDUQ;
        u8 ac = ath12k_tid_to_ac(tid);

        switch (ac) {
                case WME_AC_BE:
                        queue_id = ath12k_sdwf_get_3_link_best_suitable_tid(peer, 0, 3);
                        break;
                case WME_AC_BK:
                        queue_id = ath12k_sdwf_get_3_link_best_suitable_tid(peer, 1, 2);
                        break;
                case WME_AC_VI:
                        queue_id = ath12k_sdwf_get_3_link_best_suitable_tid(peer, 4, 5);
                        break;
                case WME_AC_VO:
                        queue_id = ath12k_sdwf_get_3_link_best_suitable_tid(peer, 6, 7);
                        break;
                default:
                        break;
        }

        if (queue_id != QOS_INVALID_MSDUQ) {
                if (queue_id < ATH12K_DATA_TID_MAX)
                        peer->flow_cnt[queue_id]++;
                else
                        queue_id = QOS_INVALID_MSDUQ;
        }

        return queue_id;
}

u8 ath12k_sdwf_get_peer_msduq(struct ath12k_base *ab,
                              u16 peer_id,
                              u32 dscp_pcp, bool pcp)
{
       struct ath12k_dp_link_peer *peer;
       u8 queue_id = QOS_INVALID_MSDUQ;

       if (!ath12k_mlo_3_link_tx)
               return QOS_INVALID_MSDUQ;

       spin_lock_bh(&ab->dp->dp_lock);
       peer = ath12k_dp_link_peer_find_by_id(ab->dp, peer_id);
       if (!peer) {
               ath12k_err(ab, "Unable to find peer for peer_id : %d\n",
                          peer_id);
               goto err_unlock;
       }

       if (!peer->sta)
               goto err_unlock;

       if (!peer->sta->valid_links)
	       goto err_unlock;

       if (hweight16(peer->sta->valid_links) !=
           ATH12K_3LINK_MLO_MAX_STA_LINKS)
               goto err_unlock;

       if (peer->primary_link &&
           ab->hw_params->mlo_3_link_tx_support) {
               if (pcp)
                       queue_id = dscp_pcp;
               else
                       queue_id = dscp_tid_map[dscp_pcp];

               queue_id = ath12k_sdwf_get_3_link_queue_id(ab->dp,
							  peer, queue_id);
       }

err_unlock:
       spin_unlock_bh(&ab->dp->dp_lock);
       return queue_id;
}

u16 ath12k_sdwf_get_msduq(struct wireless_dev *wdev,
			  u8 *peer_mac, u32 svc_id,
			  bool scs, u32 dscp_pcp, bool pcp)
{
	struct ath12k *ar;
	struct ieee80211_vif *vif;
	u16 peer_id;
	u16 ret_msduq = SDWF_PEER_MSDUQ_INVALID;
	u8 msduq = QOS_INVALID_MSDUQ;

	if (ath12k_mlo_3_link_tx) {
		vif = wdev_to_ieee80211_vif_vlan(wdev, false);
		if (!vif)
			return ret_msduq;
	} else {
		vif = wdev_to_ieee80211_vif(wdev);
		if (!vif)
			return ret_msduq;
	}

	ar = ath12k_sdwf_get_ar_from_vif(wdev, vif, peer_mac, &peer_id);
	if (!ar) {
		ath12k_dbg(NULL, ATH12K_DBG_QOS, "ar is NULL");
		return ret_msduq;
	}

	if (!scs && !ath12k_sdwf_service_configured(ar->ab, svc_id)) {
		/* For 3-Link MLO Clients TX link management get msduq */
		msduq = ath12k_sdwf_get_peer_msduq(ar->ab, peer_id,
						   dscp_pcp, pcp);

		if (msduq != QOS_INVALID_MSDUQ)
			ret_msduq = FIELD_PREP(SDWF_PEER_ID, peer_id) |
				FIELD_PREP(SDWF_MSDUQ_ID, msduq);

		return ret_msduq;
	}

	msduq = ath12k_sdwf_alloc_msduq(ar, svc_id, peer_id, scs);
	if (msduq != QOS_INVALID_MSDUQ)
		ret_msduq = FIELD_PREP(SDWF_PEER_ID, peer_id) |
				FIELD_PREP(SDWF_MSDUQ_ID, msduq);

	return ret_msduq;
}

u16 ath12k_sdwf_get_msduq_peer(struct wireless_dev *wdev,
			       u8 *peer_mac,
			      struct sawf_param *dl_params,
			      bool scs_mscs)
{
	u32 dscp_pcp;
	bool pcp = false;

	if (!wdev)
		return SDWF_PEER_MSDUQ_INVALID;

	if (dl_params->valid_flag & SDWF_PCP_VALID) {
		dscp_pcp = dl_params->pcp;
		pcp = true;
	} else
		dscp_pcp = dl_params->dscp;

	return ath12k_sdwf_get_msduq(wdev, peer_mac,
				     dl_params->service_id,
				     scs_mscs, dscp_pcp, pcp);
}

void
ath12k_sdwf_3_link_peer_dl_flow_count(struct wireless_dev *wdev,
				      struct ieee80211_vif *vif, u8 *mac_addr,
				      u32 mark_metadata, u16 svc_id)
{
	struct ath12k *ar = NULL;
	struct ath12k_dp *dp;
	struct ath12k_dp_link_peer *peer;
	u16 peer_id, queue_id;

	if (!ath12k_mlo_3_link_tx || !mac_addr)
		return;

	ar = ath12k_sdwf_get_ar_from_vif(wdev, vif, mac_addr, &peer_id);
	if (!ar)
		return;

	if (peer_id == ATH12K_PEER_ID_INVALID) {
		ath12k_err(NULL, "Invalid Peer");
		return;
	}

	if (ath12k_sdwf_service_configured(ar->ab, svc_id))
		return;

	dp = ath12k_ab_to_dp(ar->ab);
	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (!peer || !peer->sta)
		goto err_unlock;

	if (!peer->sta->valid_links ||
	    (hweight16(peer->sta->valid_links) !=
	     ATH12K_3LINK_MLO_MAX_STA_LINKS))
		goto err_unlock;

	queue_id = mark_metadata & SDWF_PEER_MSDUQ_ID;

	if (queue_id < ATH12K_DATA_TID_MAX) {
		if (peer->flow_cnt[queue_id])
			peer->flow_cnt[queue_id]--;
	}

err_unlock:
	spin_unlock_bh(&dp->dp_lock);
	return;
}

int ath12k_htt_sawf_streaming_stats_configure(struct ath12k *ar,
					      u8 stats_type,
					      u8 configure,
					      u32 config_param_0,
					      u32 config_param_1,
					      u32 config_param_2,
					      u32 config_param_3)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ab->dp;
	struct sk_buff *skb;
	struct ath12k_htt_h2t_sawf_streaming_req *cmd;
	int len = sizeof(*cmd);
	int ret;

	if (!(ath12k_debugfs_is_qos_stats_enabled(ar) &
	      ATH12K_QOS_STATS_ADVANCED))
		return -EOPNOTSUPP;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb) {
		ath12k_err(ab, "Insufficient Memory\n");
		return -ENOMEM;
	}

	skb_put(skb, len);
	cmd = (struct ath12k_htt_h2t_sawf_streaming_req *)skb->data;
	cmd->info = u32_encode_bits(HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ,
				    HTT_H2T_MSG_TYPE_ID) |
		    u32_encode_bits(stats_type,
				    HTT_H2T_MSG_TYPE_STREAMING_STATS_TYPE) |
		    u32_encode_bits(configure,
				    HTT_H2T_MSG_TYPE_STREAMING_STATS_CONFIGURE);

	cmd->config_param_0 = config_param_0;
	cmd->config_param_1 = config_param_1;
	cmd->config_param_2 = config_param_2;
	cmd->config_param_3 = config_param_3;

	ath12k_dbg(ab, ATH12K_DBG_QOS, "Configure streaming stats :0x%x\n", cmd->info);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret)
		dev_kfree_skb_any(skb);
	return ret;
}

int ath12k_telemetry_get_msduq_tx_stats(void *ptr, void *arg,
					void *msduq_tx_stats,
					u8 msduq_id)
{
	struct ath12k_dp_hw *dp_hw = (struct ath12k_dp_hw *)ptr;
	struct ath12k_dp_peer *mld_peer = (struct ath12k_dp_peer *)arg;
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	struct msduq_tx_stats *tele_tx_stats = (struct msduq_tx_stats *)msduq_tx_stats;
	struct ath12k_mld_qos_stats *mld_qos_stats;
	struct tx_stats *qos_tx;
	unsigned long peer_links_map, scan_links_map;
	int ret = 0;
	u8 tid, q_id, link_id, pkt_type, mcs;

	if (!dp_hw || !mld_peer)
		return -ENODATA;

	tid = u8_get_bits(msduq_id, MSDUQ_TID_MASK);
	q_id = u8_get_bits(msduq_id, MSDUQ_MASK);

	if (q_id > QOS_TID_MDSUQ_MAX)
		return -EINVAL;

	rcu_read_lock();

	spin_lock_bh(&dp_hw->peer_lock);
	mld_peer = ath12k_dp_peer_find(dp_hw, mld_peer->addr);
	if (!mld_peer) {
		ret = -ENOENT;
		goto end;
	}

	mld_qos_stats = &mld_peer->mld_qos_stats[tid][q_id];
	if (!mld_qos_stats) {
		ret = -ENOENT;
		goto end;
	}
	tele_tx_stats->tx_failed = mld_qos_stats->tx_failed_pkts;

	peer_links_map = mld_peer->peer_links_map;
	scan_links_map = ATH12K_SCAN_LINKS_MASK;

	for_each_andnot_bit(link_id, &peer_links_map,
			    &scan_links_map,
			    ATH12K_NUM_MAX_LINKS) {
		tmp_peer = rcu_dereference(mld_peer->link_peers[link_id]);
		if (!tmp_peer ||
		    !tmp_peer->peer_stats.qos_stats) {
			continue;
		}

		qos_tx = &tmp_peer->peer_stats.qos_stats->qos_tx[tid][q_id];
		if (!mld_peer->qos_stats_lvl) {
			if (tmp_peer->primary_link) {
				tele_tx_stats->retry_count =
					qos_tx->retry_count;
				tele_tx_stats->total_retries_count =
					qos_tx->total_retries_count;
				for (pkt_type = 0; pkt_type < DOT11_MAX; pkt_type++) {
					for (mcs = 0; mcs < MAX_MCS; mcs++) {
						tele_tx_stats->pkt_type[pkt_type].mcs_count[mcs] =
							qos_tx->pkt_type[pkt_type].mcs_count[mcs];
					}
				}
				break;
			}
		} else {
			tele_tx_stats->retry_count +=
				qos_tx->retry_count;
			tele_tx_stats->total_retries_count +=
				qos_tx->total_retries_count;
			for (pkt_type = 0; pkt_type < DOT11_MAX; pkt_type++) {
				for (mcs = 0; mcs < MAX_MCS; mcs++) {
					tele_tx_stats->pkt_type[pkt_type].mcs_count[mcs] +=
						qos_tx->pkt_type[pkt_type].mcs_count[mcs];
				}
			}
		}
		tmp_peer = NULL;
	}
end:
	spin_unlock_bh(&dp_hw->peer_lock);
	rcu_read_unlock();
	return ret;
}

int ath12k_telemetry_get_sawf_tx_stats_drop(void *ptr, void *peer, u64 *pass,
					    u64 *drop, u64 *drop_ttl,
					    u8 tid_v, u8 msduq_id)
{
	struct ath12k_dp_hw *dp_hw = (struct ath12k_dp_hw *)ptr;
	struct ath12k_dp_peer *mld_peer = (struct ath12k_dp_peer *)peer;
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	struct ath12k_mld_qos_stats *mld_qos_stats;
	struct tx_stats *qos_tx;
	unsigned long peer_links_map, scan_links_map;
	int ret = 0;
	u8 tid, q_id, link_id;

	if (!dp_hw || !mld_peer)
		return -ENODATA;

	tid = u8_get_bits(msduq_id, MSDUQ_TID_MASK);
	q_id = u8_get_bits(msduq_id, MSDUQ_MASK);

	if (q_id > QOS_TID_MDSUQ_MAX)
		return -EINVAL;

	rcu_read_lock();
	spin_lock_bh(&dp_hw->peer_lock);
	mld_peer = ath12k_dp_peer_find(dp_hw, mld_peer->addr);
	if (!mld_peer) {
		ret = -ENOENT;
		goto end;
	}

	mld_qos_stats = &mld_peer->mld_qos_stats[tid][q_id];
	if (!mld_qos_stats) {
		ret = -ENOENT;
		goto end;
	}

	*pass = mld_qos_stats->tx_success_pkts;
	*drop = mld_qos_stats->tx_failed_pkts;

	peer_links_map = mld_peer->peer_links_map;
	scan_links_map = ATH12K_SCAN_LINKS_MASK;

	for_each_andnot_bit(link_id, &peer_links_map,
			    &scan_links_map,
			    ATH12K_NUM_MAX_LINKS) {
		tmp_peer = rcu_dereference(mld_peer->link_peers[link_id]);
		if (!tmp_peer ||
		    !tmp_peer->peer_stats.qos_stats) {
			continue;
		}

		qos_tx = &tmp_peer->peer_stats.qos_stats->qos_tx[tid][q_id];
		if (!mld_peer->qos_stats_lvl) {
			if (tmp_peer->primary_link) {
				*drop_ttl = qos_tx->dropped.age_out;
				break;
			}
		} else {
			*drop_ttl += qos_tx->dropped.age_out;
		}
		tmp_peer = NULL;
	}
end:
	spin_unlock_bh(&dp_hw->peer_lock);
	rcu_read_unlock();
	return ret;
}

int ath12k_telemetry_get_sawf_tx_stats_mpdu(void *ptr, void *peer, u64 *svc_int_pass,
					    u64 *svc_int_fail, u64 *burst_pass,
					    u64 *burst_fail, u8 tid_v, u8 msduq_id)
{
	struct ath12k_dp_hw *dp_hw = (struct ath12k_dp_hw *)ptr;
	struct ath12k_dp_peer *mld_peer = (struct ath12k_dp_peer *)peer;
	struct ath12k_mld_qos_stats *mld_qos_stats;
	int ret = 0;
	u8 tid, q_id;

	if (!dp_hw || !mld_peer)
		return -ENODATA;

	tid = u8_get_bits(msduq_id, MSDUQ_TID_MASK);
	q_id = u8_get_bits(msduq_id, MSDUQ_MASK);

	if (q_id > QOS_TID_MDSUQ_MAX)
		return -EINVAL;

	rcu_read_lock();
	spin_lock_bh(&dp_hw->peer_lock);
	mld_peer = ath12k_dp_peer_find(dp_hw, mld_peer->addr);
	if (!mld_peer) {
		ret = -ENOENT;
		goto end;
	}

	mld_qos_stats = &mld_peer->mld_qos_stats[tid][q_id];
	if (!mld_qos_stats) {
		ret = -ENOENT;
		goto end;
	}

	*svc_int_pass = mld_qos_stats->svc_intval_stats.success_cnt;
	*svc_int_fail = mld_qos_stats->svc_intval_stats.failure_cnt;
	*burst_pass = mld_qos_stats->burst_size_stats.success_cnt;
	*burst_fail = mld_qos_stats->burst_size_stats.failure_cnt;
end:
	spin_unlock_bh(&dp_hw->peer_lock);
	rcu_read_unlock();
	return ret;
}

int ath12k_telemetry_get_sawf_tx_stats_tput(void *ptr, void *peer, u64 *in_bytes,
					    u64 *in_cnt, u64 *tx_bytes,
					    u64 *tx_cnt, u8 tid_v, u8 msduq_id)
{
	struct ath12k_dp_hw *dp_hw = (struct ath12k_dp_hw *)ptr;
	struct ath12k_dp_peer *mld_peer = (struct ath12k_dp_peer *)peer;
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	struct tx_stats *qos_tx;
	unsigned long peer_links_map, scan_links_map;
	int ret = 0;
	u8 tid, q_id, link_id;

	if (!dp_hw || !mld_peer)
		return -ENODATA;

	tid = u8_get_bits(msduq_id, MSDUQ_TID_MASK);
	q_id = u8_get_bits(msduq_id, MSDUQ_MASK);

	if (q_id > QOS_TID_MDSUQ_MAX)
		return -EINVAL;

	rcu_read_lock();
	spin_lock_bh(&dp_hw->peer_lock);
	mld_peer = ath12k_dp_peer_find(dp_hw, mld_peer->addr);
	if (!mld_peer) {
		ret = -ENOENT;
		goto end;
	}

	peer_links_map = mld_peer->peer_links_map;
	scan_links_map = ATH12K_SCAN_LINKS_MASK;

	for_each_andnot_bit(link_id, &peer_links_map,
			    &scan_links_map,
			    ATH12K_NUM_MAX_LINKS) {
		tmp_peer = rcu_dereference(mld_peer->link_peers[link_id]);
		if (!tmp_peer ||
		    !tmp_peer->peer_stats.qos_stats) {
			continue;
		}

		qos_tx = &tmp_peer->peer_stats.qos_stats->qos_tx[tid][q_id];
		if (!mld_peer->qos_stats_lvl) {
			if (tmp_peer->primary_link) {
				*in_bytes = qos_tx->tx_ingress.bytes;
				*in_cnt = qos_tx->tx_ingress.num;
				*tx_bytes = qos_tx->tx_success.bytes;
				*tx_cnt = qos_tx->tx_success.num;
				break;
			}
		} else {
			*in_bytes += qos_tx->tx_ingress.bytes;
			*in_cnt += qos_tx->tx_ingress.num;
			*tx_bytes += qos_tx->tx_success.bytes;
			*tx_cnt += qos_tx->tx_success.num;
		}
		tmp_peer = NULL;
	}

end:
	spin_unlock_bh(&dp_hw->peer_lock);
	rcu_read_unlock();
	return ret;
}

static int ath12k_get_msduq_id(struct ath12k_base *ab, u8 svc_id,
			       struct ath12k_dp_peer *mld_peer,
			       u8 *tid, u8 *q_id)
{
	struct ath12k_dp_peer_qos *qos = NULL;
	u16 qos_id;
	u8 tid_l, q_id_l;

	qos_id = ath12k_sdwf_get_dl_qos_id(ab, svc_id);
	if (qos_id == QOS_DL_ID_MAX) {
		ath12k_err(NULL, "svc_id: %u not present\n", svc_id);
		return -ENODATA;
	}

	qos = mld_peer->qos;
	if (!qos)
		return -ENODATA;

	/* Find matching tid and q_id with svc_id in the reserved pool*/
	for (tid_l = 0; tid_l < QOS_TID_MAX; tid_l++) {
		for (q_id_l = 0; q_id_l < QOS_TID_MDSUQ_MAX; q_id_l++) {
			if (qos->msduq_map[tid_l][q_id_l].reserved &&
			    qos->msduq_map[tid_l][q_id_l].qos_id == qos_id) {
				ath12k_dbg(ab, ATH12K_DBG_QOS,
					   "tid %u usrdefq %u\n",
					   tid_l, q_id_l);
				*tid = tid_l;
				*q_id = q_id_l;
				return 0;
			}
		}
	}
	ath12k_dbg(ab, ATH12K_DBG_QOS, "Queue and TID not found for svc_id=%u",
		   svc_id);

	return -EINVAL;
}

static void ath12k_copy_tx_stats(struct tx_stats *src,
				 struct ath12k_tele_qos_tx *dst)
{
	u8 pkt_type, mcs;

	dst->tx_success.num += src->tx_success.num;
	dst->tx_success.bytes += src->tx_success.bytes;

	dst->tx_ingress.num += src->tx_ingress.num;
	dst->tx_ingress.bytes += src->tx_ingress.bytes;

	dst->tx_failed.num += src->tx_failed.num;
	dst->tx_failed.bytes += src->tx_failed.bytes;

	dst->dropped.fw_rem.num += src->dropped.fw_rem.num;
	dst->dropped.fw_rem.bytes += src->dropped.fw_rem.bytes;
	dst->dropped.fw_rem_notx += src->dropped.fw_rem_notx;
	dst->dropped.fw_rem_tx += src->dropped.fw_rem_tx;
	dst->dropped.age_out += src->dropped.age_out;
	dst->dropped.fw_reason1 += src->dropped.fw_reason1;
	dst->dropped.fw_reason2 += src->dropped.fw_reason2;
	dst->dropped.fw_reason3 += src->dropped.fw_reason3;
	dst->dropped.fw_rem_queue_disable += src->dropped.fw_rem_queue_disable;
	dst->dropped.fw_rem_no_match += src->dropped.fw_rem_no_match;
	dst->dropped.drop_threshold += src->dropped.drop_threshold;
	dst->dropped.drop_link_desc_na += src->dropped.drop_link_desc_na;
	dst->dropped.invalid_drop += src->dropped.invalid_drop;
	dst->dropped.mcast_vdev_drop += src->dropped.mcast_vdev_drop;
	dst->dropped.invalid_rr += src->dropped.invalid_rr;

	dst->queue_depth += src->queue_depth;
	dst->total_retries_count += src->total_retries_count;
	dst->retry_count += src->retry_count;
	dst->multiple_retry_count += src->multiple_retry_count;
	dst->failed_retry_count += src->failed_retry_count;
	dst->reinject_pkt += src->reinject_pkt;

	for (pkt_type = 0; pkt_type < DOT11_MAX; pkt_type++) {
		for (mcs = 0; mcs < MAX_MCS; mcs++) {
			dst->pkt_type[pkt_type].mcs_count[mcs] +=
				src->pkt_type[pkt_type].mcs_count[mcs];
		}
	}
}

void ath12k_telemetry_update_tx_stats(struct ath12k_tele_qos_tx *tx, u8 link_id,
				      struct ath12k_dp_link_peer *link_peer,
				      struct ath12k_dp_peer *mld_peer,
				      u8 tid, u8 q_idx)
{
	struct tx_stats *qos_tx;
	struct ath12k_dp_peer_qos *qos = NULL;
	struct ath12k_mld_qos_stats *mld_qos;
	u32 throughput = 0, ingress_rate = 0, msduq = 0, retries_pct = 0;
	u32 min_tput = 0, max_tput = 0, avg_tput = 0, per = 0;

	qos = mld_peer->qos;
	if (!qos)
		return;

	if (link_id == INVALID_LINK_ID &&
	    mld_peer->qos_stats_lvl == ATH12K_QOS_MULTI_LINK_STATS) {
		unsigned long peer_links_map, scan_links_map;

		link_id = 0;
		peer_links_map = mld_peer->peer_links_map;
		scan_links_map = ATH12K_SCAN_LINKS_MASK;

		for_each_andnot_bit(link_id, &peer_links_map,
				    &scan_links_map,
				    ATH12K_NUM_MAX_LINKS) {
			link_peer = rcu_dereference(mld_peer->link_peers[link_id]);
			if (!link_peer ||
			    !link_peer->peer_stats.qos_stats)
				continue;
			qos_tx = &link_peer->peer_stats.qos_stats->qos_tx[tid][q_idx];
			ath12k_copy_tx_stats(qos_tx, tx);
		}
		goto tele_stats_fill;
	}

	if (link_peer->peer_stats.qos_stats) {
		qos_tx = &link_peer->peer_stats.qos_stats->qos_tx[tid][q_idx];
		ath12k_copy_tx_stats(qos_tx, tx);
	}

tele_stats_fill:
	if (qos->telemetry_peer_ctx) {
		msduq = u16_encode_bits(q_idx, MSDUQ_MASK) |
			u16_encode_bits(tid, MSDUQ_TID_MASK);
		ath12k_telemetry_get_rate(qos->telemetry_peer_ctx, tid, msduq,
					  &throughput, &ingress_rate);
		ath12k_telemetry_get_tx_rate(qos->telemetry_peer_ctx,
					     tid, msduq,
					     &min_tput, &max_tput,
					     &avg_tput, &per,
					     &retries_pct);
		tx->throughput = throughput;
		tx->ingress_rate = ingress_rate;
		tx->min_throughput = min_tput;
		tx->max_throughput = max_tput;
		tx->avg_throughput = avg_tput;
		tx->per = per;
		tx->retries_pct = retries_pct;
	}

	mld_qos = &mld_peer->mld_qos_stats[tid][q_idx];
	tx->svc_intval_stats.success_cnt =
		mld_qos->svc_intval_stats.success_cnt;
	tx->svc_intval_stats.failure_cnt =
		mld_qos->svc_intval_stats.failure_cnt;
	tx->burst_size_stats.success_cnt =
		mld_qos->burst_size_stats.success_cnt;
	tx->burst_size_stats.failure_cnt =
		mld_qos->burst_size_stats.failure_cnt;
}

static int ath12k_telemetry_get_qos_txstats(struct ath12k_base *ab,
					    struct ath12k_tele_qos_tx_ctx *tx_ctx,
					    u8 svc_id, u8 link_id,
					    struct ath12k_dp_link_peer *link_peer)
{
	struct ath12k_dp_peer *mld_peer;
	struct ath12k_tele_qos_tx *tx;
	u8 tid, q_idx;

	mld_peer = link_peer->dp_peer;
	if (svc_id == 0) {
		for (tid = 0; tid < QOS_TID_MAX; tid++) {
			for (q_idx = 0; q_idx < QOS_TID_MDSUQ_MAX; q_idx++) {
				tx = &tx_ctx->tx[tid][q_idx];
				ath12k_telemetry_update_tx_stats(tx, link_id, link_peer,
								 mld_peer, tid, q_idx);
			}
		}
	} else {
		tx = &tx_ctx->tx[0][0];
		if (ath12k_get_msduq_id(ab, svc_id, mld_peer, &tid,
					&q_idx))
			return -EINVAL;
		ath12k_telemetry_update_tx_stats(tx, link_id, link_peer, mld_peer, tid, q_idx);
		tx_ctx->tid = tid;
		tx_ctx->msduq = q_idx;
	}
	return 0;
}

static void ath12k_copy_delay_stats(struct delay_stats *src, struct ath12k_tele_qos_delay *dst)
{
	struct hist_stats *dst_hist_stats = &dst->delay_hist;
	struct hist_stats *src_hist_stats = &src->delay_hist;
	u8 index;

	for (index = 0; index < HIST_BUCKET_MAX; index++)
		dst_hist_stats->hist.freq[index] += src_hist_stats->hist.freq[index];

	dst_hist_stats->min += src_hist_stats->min;
	dst_hist_stats->max += src_hist_stats->max;
	dst_hist_stats->avg += src_hist_stats->avg;

	dst->invalid_delay_pkts += src->invalid_delay_pkts;
	dst->delay_success += src->delay_success;
	dst->delay_failure += src->delay_failure;
}

void ath12k_telemetry_update_delay_stats(struct ath12k_tele_qos_delay *delay, u8 link_id,
					 struct ath12k_dp_link_peer *link_peer,
					 struct ath12k_dp_peer *mld_peer,
					 u8 tid, u8 q_idx)
{
	struct delay_stats *qos_delay;
	struct ath12k_dp_peer_qos *qos = NULL;
	u32 nwdelay_avg = 0, swdelay_avg = 0, hwdelay_avg = 0;

	qos = mld_peer->qos;
	if (!qos)
		return;

	if (link_id == INVALID_LINK_ID &&
	    mld_peer->qos_stats_lvl == ATH12K_QOS_MULTI_LINK_STATS) {
		unsigned long peer_links_map, scan_links_map;

		link_id = 0;
		peer_links_map = mld_peer->peer_links_map;
		scan_links_map = ATH12K_SCAN_LINKS_MASK;

		for_each_andnot_bit(link_id, &peer_links_map,
				    &scan_links_map,
				    ATH12K_NUM_MAX_LINKS) {
			link_peer = rcu_dereference(mld_peer->link_peers[link_id]);
			if (!link_peer ||
			    !link_peer->peer_stats.qos_stats)
				continue;
			qos_delay = &link_peer->peer_stats.qos_stats->qos_delay[tid][q_idx];
			ath12k_copy_delay_stats(qos_delay, delay);
		}
		goto tele_stats_fill;
	}

	if (link_peer->peer_stats.qos_stats) {
		qos_delay = &link_peer->peer_stats.qos_stats->qos_delay[tid][q_idx];
		ath12k_copy_delay_stats(qos_delay, delay);
	}

tele_stats_fill:
	if (qos->telemetry_peer_ctx) {
		u16 msduq;
		msduq = u16_encode_bits(q_idx, MSDUQ_MASK) |
			u16_encode_bits(tid, MSDUQ_TID_MASK);
		ath12k_telemetry_get_mov_avg(qos->telemetry_peer_ctx,
					     tid, msduq, &nwdelay_avg,
					     &swdelay_avg, &hwdelay_avg);
		delay->nwdelay_avg = nwdelay_avg;
		delay->swdelay_avg = swdelay_avg;
		delay->hwdelay_avg = hwdelay_avg;
	}
}

static int ath12k_telemetry_get_qos_delaystats(struct ath12k_base *ab,
					       struct ath12k_tele_qos_delay_ctx *delay_ctx,
					       u8 svc_id, u8 link_id,
					       struct ath12k_dp_link_peer *link_peer)
{
	struct ath12k_dp_peer *mld_peer;
	struct ath12k_tele_qos_delay *delay;
	u8 tid, q_idx;

	mld_peer = link_peer->dp_peer;
	if (svc_id == 0) {
		for (tid = 0; tid < QOS_TID_MAX; tid++) {
			for (q_idx = 0; q_idx < QOS_TID_MDSUQ_MAX; q_idx++) {
				delay = &delay_ctx->delay[tid][q_idx];
				ath12k_telemetry_update_delay_stats(delay,
								    link_id,
								    link_peer,
								    mld_peer,
								    tid,
								    q_idx);
			}
		}
	} else {
		delay = &delay_ctx->delay[0][0];
		if (ath12k_get_msduq_id(ab, svc_id, mld_peer, &tid,
					&q_idx))
			return -EINVAL;
		ath12k_telemetry_update_delay_stats(delay, link_id,
						    link_peer, mld_peer,
						    tid, q_idx);
		delay_ctx->tid = tid;
		delay_ctx->msduq = q_idx;
	}
	return 0;
}

int ath12k_telemetry_get_qos_stats(struct ath12k_vif *ahvif,
				   struct ath12k_telemetry_dp_peer *telemetry_peer,
				   struct ath12k_telemetry_command *cmd)
{
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct wiphy *wiphy;
	struct ath12k_dp_link_peer *peer;
	int ret = 0;
	u8 link_id, mac_addr[ETH_ALEN] = { 0 };
	bool qos_stats_lvl;

	link_id = cmd->link_id;
	memcpy(mac_addr, cmd->mac, ETH_ALEN);

	if (!ahvif)
		return -ENOENT;

	if (!ahvif->ah || !ahvif->ah->hw)
		return -ENOENT;

	wiphy = ahvif->ah->hw->wiphy;

	if (!wiphy)
		return -ENOENT;

	sta = ieee80211_find_sta_by_ifaddr(ahvif->ah->hw, mac_addr, NULL);
	if (!sta) {
		return -ENOENT;
	}

	ahsta = (struct ath12k_sta *)sta->drv_priv;
	if (!ahsta) {
		return -ENOENT;
	}

	rcu_read_lock();
	if (link_id != INVALID_LINK_ID) {
		if (!(ahsta->links_map & BIT(link_id))) {
			ath12k_err(NULL, "link_id : %u not present\n", link_id);
			ret = -ENOENT;
			goto out;
		}

		arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);
		if (!arsta || !arsta->arvif || !arsta->arvif->ar) {
			ath12k_err(NULL, " Interface or radio NA with link_id %u\n", link_id);
			ret = -ENOENT;
			goto out;
		}

		qos_stats_lvl = (arsta->arvif->ar->debug.qos_stats &
					  ATH12K_QOS_STATS_COLLECTION_MASK) >> 2;
		if (qos_stats_lvl == ATH12K_QOS_SINGLE_LINK_STATS) {
			if (link_id != ahsta->primary_link_id) {
				ath12k_err(NULL, "Single link stats collection: link_id = %u not the primary link id \n", link_id);
				ret = -ENOENT;
				goto out;
			}
		} else {
			memcpy(mac_addr, ahsta->link[link_id]->addr, ETH_ALEN);
			goto skip_link_mac_fill;
		}

	}

	if (sta->mlo) {
		link_id = ahsta->primary_link_id;
		memcpy(mac_addr, ahsta->link[link_id]->addr, ETH_ALEN);
	} else if (sta->valid_links) {
		link_id = ahsta->deflink.link_id;
	} else {
		link_id = 0;
	}

skip_link_mac_fill:
	arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);

	if (!arvif) {
		ret = -ENOENT;
		goto out;
	}

	ar = arvif->ar;
	if (!ar) {
		ret = -ENOENT;
		goto out;
	}

	ab = ar->ab;
	if (!ab) {
		ret = -ENOENT;
		goto out;
	}

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_addr(ab->dp, mac_addr);
	if (!peer) {
		ath12k_err(ab, "Peer not present\n");
		ret = -ENOENT;
		goto end;
	}

	if (cmd->feat.feat_sdwfdelay) {
		ret = ath12k_telemetry_get_qos_delaystats(ab,
							  &telemetry_peer->peer_stats.delay_ctx,
							  cmd->svc_id,
							  cmd->link_id,
							  peer);
		if (ret)
			goto end;
	}

	if (cmd->feat.feat_sdwftx) {
		ret = ath12k_telemetry_get_qos_txstats(ab,
						       &telemetry_peer->peer_stats.tx_ctx,
						       cmd->svc_id,
						       cmd->link_id,
						       peer);
	}

end:
	spin_unlock_bh(&ab->dp->dp_lock);
out:
	rcu_read_unlock();
	return ret;
}
