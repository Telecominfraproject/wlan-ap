// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "peer.h"
#include "dp_peer.h"
#include "debug.h"
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
#include "ppe.h"
#endif
#include "telemetry_agent_if.h"

static int ath12k_wait_for_peer_common(struct ath12k_base *ab, int vdev_id,
				       const u8 *addr, bool expect_mapped)
{
	int ret;

	ret = wait_event_timeout(ab->peer_mapping_wq, ({
				bool mapped;

				spin_lock_bh(&ab->dp->dp_lock);
				mapped = !!ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp, vdev_id, addr);
				spin_unlock_bh(&ab->dp->dp_lock);

				(mapped == expect_mapped ||
				 test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags));
				}), 3 * HZ);

	if (ret <= 0)
		return -ETIMEDOUT;

	return 0;
}

void ath12k_peer_cleanup(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_base *ab = ar->ab;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ab->dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (peer->vdev_id != vdev_id)
			continue;

		ath12k_warn(ab, "removing stale peer %pM from vdev_id %d\n",
			    peer->addr, vdev_id);

		ath12k_link_peer_free(peer);
		ar->num_peers--;
	}

	spin_unlock_bh(&ab->dp->dp_lock);
}

static int ath12k_wait_for_peer_deleted(struct ath12k *ar, int vdev_id, const u8 *addr)
{
	return ath12k_wait_for_peer_common(ar->ab, vdev_id, addr, false);
}

int ath12k_wait_for_peer_delete_done(struct ath12k *ar, u32 vdev_id,
				     const u8 *addr)
{
	int ret;
	unsigned long time_left;

	ret = ath12k_wait_for_peer_deleted(ar, vdev_id, addr);
	if (ret) {
		ath12k_warn(ar->ab, "failed wait for peer deleted peer_addr : %pM\n", addr);
		return ret;
	}

	time_left = wait_for_completion_timeout(&ar->peer_delete_done,
						3 * HZ);
	if (time_left == 0) {
		ath12k_warn(ar->ab, "Timeout in receiving peer delete response peer_addr : %pM\n",
			    addr);
		return -ETIMEDOUT;
	}

	return 0;
}

static int ath12k_peer_delete_send(struct ath12k *ar, u32 vdev_id, const u8 *addr,
				   struct ath12k_sta *ahsta)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	reinit_completion(&ar->peer_delete_done);
	spin_lock_bh(&ar->ab->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp,
							    vdev_id, addr);
	if (peer && !peer->is_bridge_peer) {
		ret = ath12k_telemetry_peer_agent_delete_handler(ar, vdev_id,
								 addr);
		if (ret && ret != -EOPNOTSUPP) {
			ath12k_dbg(ab, ATH12K_DBG_PEER,
				   "failed to delete peer reference in TA for vdev_id %d addr %pM ret %d\n",
				   vdev_id, addr, ret);
		}
	}
	 spin_unlock_bh(&ar->ab->dp->dp_lock);

	ret = ath12k_wmi_send_peer_delete_cmd(ar, addr, vdev_id, ahsta);
	if (ret) {
		ath12k_warn(ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    vdev_id, addr, ret);
		return ret;
	}

	return 0;
}

static int __ath12k_peer_delete(struct ath12k *ar, u32 vdev_id, u8 *addr,
				struct ath12k_sta *ahsta)
{
	int ret;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_base *ab = ar->ab;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dp_link_peer_unassign(ar, vdev_id, addr);

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags)) {
		ath12k_warn(ar->ab, "skipped peer delete cmd for vdev_id %d addr %pM during recovery ret:%d\n",
				vdev_id, addr, -EHOSTDOWN);

		return -EHOSTDOWN;
	}

	ret = ath12k_peer_delete_send(ar, vdev_id, addr, ahsta);
	if (ret)
		return ret;

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif(ar, vdev_id);
	if (!arvif) {
		ath12k_warn(ab,"failed to get arvif with vdev_id %d,"
			    "skip ppeds ast override\n",
			    vdev_id);
	} else if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
		ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(ab, arvif, false);
	}

	rcu_read_unlock();
	ret = ath12k_wait_for_peer_delete_done(ar, vdev_id, addr);
	if (ret)
		return ret;

	return 0;
}

int ath12k_peer_delete(struct ath12k *ar, u32 vdev_id, u8 *addr,
		       struct ath12k_sta *ahsta)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ret = __ath12k_peer_delete(ar, vdev_id, addr, ahsta);
	if (ret && ret != -EHOSTDOWN)
		return ret;

	ar->num_peers--;

	return ret;
}

static int ath12k_wait_for_peer_created(struct ath12k *ar, int vdev_id, const u8 *addr)
{
	return ath12k_wait_for_peer_common(ar->ab, vdev_id, addr, true);
}

int ath12k_peer_create(struct ath12k *ar, struct ath12k_link_vif *arvif,
		       struct ieee80211_sta *sta,
		       struct ath12k_wmi_peer_create_arg *arg)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_link_sta *arsta;
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_sta *ahsta;
	u16 ml_peer_id;
	int ret;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (sta)
		ahsta = ath12k_sta_to_ahsta(sta);

	if (ar->num_peers >= (ar->max_num_peers - 1)) {
		ath12k_warn(ar->ab,
			    "failed to create peer due to insufficient peer entry resource in firmware\n");
		return -ENOBUFS;
	}

	spin_lock_bh(&ar->ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_pdev_idx(ar->ab->dp, ar->pdev_idx,
						    arg->peer_addr);
	if (peer) {
		spin_unlock_bh(&ar->ab->dp->dp_lock);
		return -EINVAL;
	}
	spin_unlock_bh(&ar->ab->dp->dp_lock);

	ret = ath12k_wmi_send_peer_create_cmd(ar, arg);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send peer create vdev_id %d ret %d\n",
			    arg->vdev_id, ret);
		return ret;
	}

	ret = ath12k_wait_for_peer_created(ar, arg->vdev_id,
					   arg->peer_addr);
	if (ret)
		return ret;

	spin_lock_bh(&ar->ab->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ar->ab->dp, arg->vdev_id,
							    arg->peer_addr);
	if (!peer) {
		spin_unlock_bh(&ar->ab->dp->dp_lock);
		ath12k_warn(ar->ab, "failed to find peer %pM on vdev %i after creation\n",
			    arg->peer_addr, arg->vdev_id);

		ret = __ath12k_peer_delete(ar, arg->vdev_id, arg->peer_addr, ahsta);
		if (ret)
			ath12k_warn(ar->ab, "failed to delete peer vdev_id %d addr %pM\n",
				    arg->vdev_id, arg->peer_addr);

		return -ENOENT;
	}

	peer->pdev_idx = ar->pdev_idx;
	peer->sta = sta;
	peer->vif = vif;
	peer->is_bridge_peer = arg->mlo_bridge_peer;

	if (vif->type == NL80211_IFTYPE_STATION) {
		dp_link_vif->ast_hash = peer->ast_hash;
		dp_link_vif->ast_idx = peer->hw_peer_id;

	}

	if (sta) {
		arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
					  ahsta->link[link_id]);

		peer->link_id = arsta->link_id;

		if (!peer->is_bridge_peer) {
			ret = ath12k_telemetry_peer_agent_create_handler(ar,
					arg->vdev_id,
					arg->peer_addr);
			if (ret && ret != -EOPNOTSUPP) {
				ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
						"failed to create peer reference in TA for vdev_id %d addr %pM ret %d\n",
						arg->vdev_id, arg->peer_addr, ret);
			}
		}

		/* Fill ML info into created peer */
		if (sta->mlo) {
			ml_peer_id = ahsta->ml_peer_id;
			peer->ml_id = ml_peer_id | ATH12K_PEER_ML_ID_VALID;
			ether_addr_copy(peer->ml_addr, sta->addr);

			peer->mlo = true;
		} else {
			peer->ml_id = ATH12K_MLO_PEER_ID_INVALID;
			peer->mlo = false;
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER, "peer created %pM\n", arg->peer_addr);

	ar->num_peers++;

	spin_unlock_bh(&ar->ab->dp->dp_lock);

	ret = ath12k_dp_link_peer_assign(ar, arvif->vdev_id,
					 sta, arg->peer_addr,
					 link_id, ar->hw_link_id, vif,
					 arvif->ahvif->dp_vif.ppe_vp_type,
					 arvif->ahvif->dp_vif.ppe_vp_num);

	if (ret)
		ath12k_peer_delete(ar, arg->vdev_id, arg->peer_addr, NULL);

	return ret;
}

u16 ath12k_peer_ml_alloc(struct ath12k_hw *ah)
{
	u16 ml_peer_id;
	int i;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (ah->num_ml_peers >= ah->max_ml_peers_supported) {
		ath12k_err(NULL, "Failed to create ML peer limit %d[%d]\n",
			   ah->max_ml_peers_supported, ah->num_ml_peers);
		return ATH12K_MLO_PEER_ID_INVALID;
	}

	ml_peer_id = ah->last_ml_peer_id;
	for (i = 0; i <= ah->max_ml_peer_ids; i++) {
		ml_peer_id = (ml_peer_id + 1) % ah->max_ml_peer_ids;

		if (!ml_peer_id)
			continue;

		if (test_bit(ml_peer_id, ah->free_ml_peer_id_map))
			continue;

		set_bit(ml_peer_id, ah->free_ml_peer_id_map);
		break;
	}

	ah->last_ml_peer_id = ml_peer_id;
	if (i == ah->max_ml_peer_ids)
		ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;

	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated ml_peer_id:%d", ml_peer_id);

	return ml_peer_id;
}

void ath12k_peer_ml_free(struct ath12k_hw *ah, struct ath12k_sta *ahsta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (sta->mlo && test_bit(ahsta->ml_peer_id, ah->free_ml_peer_id_map)) {
		clear_bit(ahsta->ml_peer_id, ah->free_ml_peer_id_map);
		ahsta->ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;
		ah->num_ml_peers--;
	}
}

int ath12k_peer_mlo_link_peer_delete(struct ath12k_link_vif *arvif,
				     struct ath12k_link_sta *arsta)
{
	struct ath12k *ar;
	struct ath12k_sta *ahsta;
	int ret;

	if (!arvif || !arsta)
		return 0;

	ar = arvif->ar;
	if (!ar)
		return 0;

	ahsta = arsta->ahsta;

	ath12k_dp_peer_cleanup(ar, arvif->vdev_id, arsta->addr);
	ath12k_dp_link_peer_unassign(ar, arvif->vdev_id, arsta->addr);

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags)) {
		ath12k_warn(ar->ab,
			    "skipped peer delete cmd for vdev_id %d addr %pM during recovery ret:%d\n",
			    arvif->vdev_id, arsta->addr, -EHOSTDOWN);
		return -EHOSTDOWN;
	}

	ret = ath12k_peer_delete_send(ar, arvif->vdev_id, arsta->addr, ahsta);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    arvif->vdev_id, arsta->addr, ret);
	}

	return ret;
}

int ath12k_peer_mlo_link_peers_delete(struct ath12k_vif *ahvif, struct ath12k_sta *ahsta)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	unsigned long links;
	struct ath12k *ar;
	int ret, err_ret = 0;
	u8 link_id;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (!sta->mlo)
		return -EINVAL;

	/* FW expects delete of all link peers at once before waiting for reception
	 * of peer unmap or delete responses
	 */
	links = ahsta->links_map;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
		arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);

		ret = ath12k_peer_mlo_link_peer_delete(arvif, arsta);
		if (ret)
			err_ret = ret;

	}

	/* Ensure all link peers are deleted and unmapped */
	links = ahsta->links_map;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
		arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
		if (!arvif || !arsta)
			continue;

		ar = arvif->ar;
		if (!ar)
			continue;

		if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags) ||
		    test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags) ||
		    test_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &ar->ab->dev_flags))
			continue;

		ret = ath12k_wait_for_peer_delete_done(ar, arvif->vdev_id, arsta->addr);
		if (ret) {
			err_ret = ret;
			continue;
		}
		ar->num_peers--;
	}

	return err_ret;
}

void ath12k_mac_peer_disassoc(struct ath12k_base *ab, struct ieee80211_sta *sta,
			      struct ath12k_sta *ahsta,
			      enum ath12k_debug_mask debug_mask)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (!ahsta->low_ack_sent) {
		ath12k_dbg(ab, debug_mask, "sending low ack for/disassoc:%pM\n",
			   sta->addr);
		/* set num of packets to maximum so that we distinguish in
		 * the hostapd to send disassoc irrespective of hostapd conf
		 */
		ieee80211_report_low_ack(sta, ATH12K_REPORT_LOW_ACK_NUM_PKT);
		/* Using this flag to avoid certain known warnings which
		 * will be triggerred when umac reset is happening
		 */
		ahsta->low_ack_sent = true;
		/* Track peers marked for deletion during Mode 2 recovery
		 * to send mlo_hw_link_id_bitmap only for them.
		 */
		if (ag && ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2)
			ahsta->peer_delete_send_mlo_hw_bitmap = true;
	}
}

static inline int ath12k_link_sta_rhash_insert(struct ath12k_base *ab,
					       struct rhashtable *rtbl,
					       struct rhash_head *rhead,
					       struct rhashtable_params *params)
{
	struct ath12k_link_sta *tmp;

	lockdep_assert_held(&ab->base_lock);

	tmp = rhashtable_lookup_get_insert_fast(rtbl, rhead, *params);

	if (!tmp)
		return 0;
	else if (IS_ERR(tmp))
		return PTR_ERR(tmp);
	else
		return -EEXIST;
}

static inline int ath12k_link_sta_rhash_remove(struct ath12k_base *ab,
					       struct rhashtable *rtbl,
					       struct rhash_head *rhead,
					       struct rhashtable_params *params)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);

	ret = rhashtable_remove_fast(rtbl, rhead, *params);
	if (ret && ret != -ENOENT)
		return ret;

	return 0;
}

int ath12k_link_sta_rhash_add(struct ath12k_base *ab,
			      struct ath12k_link_sta *arsta)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_sta_addr)
		return -EPERM;

	if (arsta->rhash_done)
		return 0;

	ret = ath12k_link_sta_rhash_insert(ab, ab->rhead_sta_addr,
					   &arsta->rhash_addr,
					   &ab->rhash_sta_addr_param);

	if (ret) {
		ath12k_warn(ab, "failed to add arsta %pM in rhash_addr ret %d\n",
			    arsta->addr, ret);
		arsta->rhash_done = false;
	} else {
		arsta->rhash_done = true;
	}

	return ret;
}

int ath12k_link_sta_rhash_delete(struct ath12k_base *ab,
				 struct ath12k_link_sta *arsta)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_sta_addr)
		return -EPERM;

	if (!arsta->rhash_done)
		return 0;

	ret = ath12k_link_sta_rhash_remove(ab, ab->rhead_sta_addr,
					   &arsta->rhash_addr,
					   &ab->rhash_sta_addr_param);

	if (ret) {
		ath12k_warn(ab,
			    "failed to remove arsta %pM in rhash_addr ret %d\n",
			    arsta->addr, ret);
		return ret;
	}

	arsta->rhash_done = false;

	return ret;
}

static int ath12k_link_sta_rhash_addr_tbl_init(struct ath12k_base *ab)
{
	struct rhashtable_params *param;
	struct rhashtable *rhash_addr_tbl;
	int ret;
	size_t size;

	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (ab->rhead_sta_addr)
		return 0;

	size = sizeof(*ab->rhead_sta_addr);
	rhash_addr_tbl = kzalloc(size, GFP_KERNEL);
	if (!rhash_addr_tbl)
		return -ENOMEM;

	param = &ab->rhash_sta_addr_param;

	param->key_offset = offsetof(struct ath12k_link_sta, addr);
	param->head_offset = offsetof(struct ath12k_link_sta, rhash_addr);
	param->key_len = sizeof_field(struct ath12k_link_sta, addr);
	param->automatic_shrinking = true;
	param->nelem_hint = ab->num_radios * ath12k_core_get_max_peers_per_radio(ab);

	ret = rhashtable_init(rhash_addr_tbl, param);
	if (ret) {
		ath12k_warn(ab, "failed to init peer addr rhash table %d\n",
			    ret);
		goto err_free;
	}

	if (!ab->rhead_sta_addr)
		ab->rhead_sta_addr = rhash_addr_tbl;
	else
		goto cleanup_tbl;

	return 0;

cleanup_tbl:
	rhashtable_destroy(rhash_addr_tbl);
err_free:
	kfree(rhash_addr_tbl);

	return ret;
}

int ath12k_link_sta_rhash_tbl_init(struct ath12k_base *ab)
{
	int ret;

	mutex_lock(&ab->tbl_mtx_lock);
	ret = ath12k_link_sta_rhash_addr_tbl_init(ab);
	mutex_unlock(&ab->tbl_mtx_lock);

	return ret;
}

void ath12k_link_sta_rhash_tbl_destroy(struct ath12k_base *ab)
{
	mutex_lock(&ab->tbl_mtx_lock);

	if (!ab->rhead_sta_addr)
		goto unlock;

	rhashtable_destroy(ab->rhead_sta_addr);
	kfree(ab->rhead_sta_addr);
	ab->rhead_sta_addr = NULL;

unlock:
	mutex_unlock(&ab->tbl_mtx_lock);
}

struct ath12k_link_sta *ath12k_link_sta_find_by_addr(struct ath12k_base *ab,
						     const u8 *addr)
{
	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_sta_addr)
		return NULL;

	return rhashtable_lookup_fast(ab->rhead_sta_addr, addr,
				      ab->rhash_sta_addr_param);
}

int ath12k_peer_send_assoc_vendor_response(const struct ath12k_dp_link_peer *peer,
					   bool is_assoc)
{
	struct ieee80211_link_sta *link_sta;
	struct ath12k_link_sta *arsta;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k *ar;

	if (!peer) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid peer skipped assoc vendor response\n");
		return -EINVAL;
	}

	sta = peer->sta;
	if (!sta) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid sta skipped assoc vendor response\n");
		return -EINVAL;
	}

	if (peer->link_id < 0) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid peer link id skipped assoc vendor response\n");
		return -EINVAL;
	}
	rcu_read_lock();
	ahsta = ath12k_sta_to_ahsta(sta);
	arsta = ahsta->link[peer->link_id];
	if (!(arsta && arsta->arvif)) {
		rcu_read_unlock();
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "invalid arsta for peer: %pM skipped assoc vendor response\n",
			   peer->addr);
		return -EINVAL;
	}

	ar = arsta->arvif->ar;

	link_sta = ath12k_mac_get_link_sta(arsta);

	if (!link_sta) {
		rcu_read_unlock();
		ath12k_warn(ar->ab, "unable to access link sta skipped assoc vendor response\n");
		return -EINVAL;
	}
	rcu_read_unlock();

	if (is_assoc)
		ath12k_mac_vendor_send_assoc_event(arsta, link_sta, true);
	else
		ath12k_mac_vendor_send_disassoc_event(arsta, link_sta);

	return 0;
}
