// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "peer.h"
#include "debug.h"
#include "nss.h"

static struct ath11k_peer *ath11k_peer_find_list_by_id(struct ath11k_base *ab,
						       int peer_id)
{
	struct ath11k_peer *peer;

	lockdep_assert_held(&ab->base_lock);

	list_for_each_entry(peer, &ab->peers, list) {
		if (peer->peer_id != peer_id)
			continue;

		return peer;
	}

	return NULL;
}

struct ath11k_peer *ath11k_peer_find(struct ath11k_base *ab, int vdev_id,
				     const u8 *addr)
{
	struct ath11k_peer *peer;

	lockdep_assert_held(&ab->base_lock);

	list_for_each_entry(peer, &ab->peers, list) {
		if (peer->vdev_id != vdev_id)
			continue;
		if (!ether_addr_equal(peer->addr, addr))
			continue;

		return peer;
	}

	return NULL;
}

struct ath11k_peer *ath11k_peer_find_by_addr(struct ath11k_base *ab,
					     const u8 *addr)
{
	struct ath11k_peer *peer;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_peer_addr)
		return NULL;

	peer = rhashtable_lookup_fast(ab->rhead_peer_addr, addr,
				      ab->rhash_peer_addr_param);

	return peer;
}

struct ath11k_peer *ath11k_peer_find_by_id(struct ath11k_base *ab,
					   int peer_id)
{
	struct ath11k_peer *peer;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_peer_id)
		return NULL;

	peer = rhashtable_lookup_fast(ab->rhead_peer_id, &peer_id,
				      ab->rhash_peer_id_param);

	return peer;
}

struct ath11k_peer *ath11k_peer_find_by_vdev_id(struct ath11k_base *ab,
						int vdev_id)
{
	struct ath11k_peer *peer;

	spin_lock_bh(&ab->base_lock);

	list_for_each_entry(peer, &ab->peers, list) {
		if (vdev_id == peer->vdev_id) {
			spin_unlock_bh(&ab->base_lock);
			return peer;
		}
	}
	spin_unlock_bh(&ab->base_lock);
	return NULL;
}

struct ath11k_peer *ath11k_peer_find_by_ast(struct ath11k_base *ab,
					   int ast_hash)
{
	struct ath11k_peer *peer;

	lockdep_assert_held(&ab->base_lock);

	list_for_each_entry(peer, &ab->peers, list)
		if (ast_hash == peer->ast_hash)
			return peer;

	return NULL;
}

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
struct ath11k_ast_entry *ath11k_peer_ast_find_by_peer(struct ath11k_base *ab,
						      struct ath11k_peer *peer,
						      u8* addr)
{
	struct ath11k_ast_entry *ast_entry;

	lockdep_assert_held(&ab->base_lock);

	list_for_each_entry(ast_entry, &peer->ast_entry_list, ase_list)
		if (ether_addr_equal(ast_entry->addr, addr))
			return ast_entry;

	return NULL;
}

struct ath11k_ast_entry *ath11k_peer_ast_find_by_addr(struct ath11k_base *ab,
						      u8* addr)
{
	struct ath11k_ast_entry *ast_entry;
	struct ath11k_peer *peer;

	lockdep_assert_held(&ab->base_lock);

	list_for_each_entry(peer, &ab->peers, list)
		list_for_each_entry(ast_entry, &peer->ast_entry_list, ase_list)
			if (ether_addr_equal(ast_entry->addr, addr))
				return ast_entry;

	return NULL;
}

struct ath11k_ast_entry *ath11k_peer_ast_find_by_pdev_idx(struct ath11k *ar,
							  u8* addr)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_ast_entry *ast_entry;
	struct ath11k_peer *peer;

	lockdep_assert_held(&ab->base_lock);

	list_for_each_entry(peer, &ab->peers, list)
		list_for_each_entry(ast_entry, &peer->ast_entry_list, ase_list)
			if (ether_addr_equal(ast_entry->addr, addr) &&
			    ast_entry->pdev_idx == ar->pdev_idx)
				return ast_entry;

	return NULL;
}

void ath11k_peer_ast_wds_wmi_wk(struct work_struct *wk)
{
	struct ath11k_ast_entry *ast_entry, *entry;
	struct ath11k_base *ab = container_of(wk, struct ath11k_base, wmi_ast_work);
	struct ath11k_peer *peer;
	struct ath11k *ar;
	int ret;
	u8 peer_addr[ETH_ALEN];
	int peer_id;

	ast_entry = kzalloc(sizeof(*ast_entry), GFP_ATOMIC);

	mutex_lock(&ab->base_ast_lock);
	spin_lock_bh(&ab->base_lock);

	while ((entry = list_first_entry_or_null(&ab->wmi_ast_list,
	       struct ath11k_ast_entry, wmi_list))) {
		list_del_init(&entry->wmi_list);

		if (!entry->ar || (entry->peer && entry->peer->delete_in_progress)) {
			continue;
		}
		memcpy(ast_entry, entry, sizeof(*ast_entry));
		ar = ast_entry->ar;
		peer = ast_entry->peer;
		memcpy(peer_addr, peer->addr, sizeof(peer_addr));
		peer_id = peer->peer_id;

		ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
			   "ath11k_peer_ast_wds_wmi_wk action %d ast_entry %pM peer %pM vdev %d\n",
			   ast_entry->action, ast_entry->addr, peer_addr,
			   ast_entry->vdev_id);

		if (ast_entry->action == ATH11K_WDS_WMI_ADD) {
			spin_unlock_bh(&ab->base_lock);
			ret = ath11k_wmi_send_add_update_wds_entry_cmd(ar, peer_addr,
								       ast_entry->addr,
								       ast_entry->vdev_id,
								       true);
			if (ret) {
				ath11k_warn(ar->ab, "add wds_entry_cmd failed %d for %pM, peer %pM\n",
					    ret, ast_entry->addr, peer_addr);
				if (peer)
					ath11k_nss_del_wds_peer(ar, peer_addr, peer_id,
								ast_entry->addr);
			}
		} else if (ast_entry->action == ATH11K_WDS_WMI_UPDATE) {
				if (!peer) {
					continue;
				}
				spin_unlock_bh(&ab->base_lock);
				ret = ath11k_wmi_send_add_update_wds_entry_cmd(ar, peer_addr,
									       ast_entry->addr,
									       ast_entry->vdev_id,
									       false);
				if (ret)
					ath11k_warn(ar->ab, "update wds_entry_cmd failed %d for %pM on peer %pM\n",
						    ret, ast_entry->addr, peer_addr);
		}
		spin_lock_bh(&ab->base_lock);
	}
	spin_unlock_bh(&ab->base_lock);
	mutex_unlock(&ab->base_ast_lock);
	kfree(ast_entry);
}

int ath11k_peer_add_ast(struct ath11k *ar, struct ath11k_peer *peer,
			u8* mac_addr, enum ath11k_ast_entry_type type)
{
	struct ath11k_ast_entry *ast_entry = NULL;
	struct ath11k_base *ab = ar->ab;

	lockdep_assert_held(&ab->base_lock);

	if (ab->num_ast_entries == ab->max_ast_index) {
		ath11k_warn(ab, "failed to add ast for %pM due to insufficient ast entry resource %d in target\n",
			    mac_addr, ab->max_ast_index);
		return -ENOBUFS;
	}

	if (type != ATH11K_AST_TYPE_STATIC) {
		ast_entry = ath11k_peer_ast_find_by_pdev_idx(ar, mac_addr);
		if (ast_entry && ast_entry->type != ATH11K_AST_TYPE_STATIC) {
			ath11k_dbg(ab, ATH11K_DBG_MAC, "ast_entry %pM already present on peer %pM\n",
				   mac_addr, ast_entry->peer->addr);
			return 0;
		}
	}

	if (peer && peer->delete_in_progress)
		return -EINVAL;

	ast_entry = kzalloc(sizeof(*ast_entry), GFP_ATOMIC);
	if (!ast_entry) {
		ath11k_warn(ab, "failed to alloc ast_entry for %pM\n",
			    mac_addr);
		return -ENOMEM;
	}

	switch (type) {
		case ATH11K_AST_TYPE_STATIC:
			peer->self_ast_entry = ast_entry;
			ast_entry->type = ATH11K_AST_TYPE_STATIC;
			break;
		case ATH11K_AST_TYPE_SELF:
			peer->self_ast_entry = ast_entry;
			ast_entry->type = ATH11K_AST_TYPE_SELF;
			break;
		case ATH11K_AST_TYPE_WDS:
			ast_entry->type = ATH11K_AST_TYPE_WDS;
			ast_entry->next_hop = 1;
			break;
		case ATH11K_AST_TYPE_MEC:
			ast_entry->type = ATH11K_AST_TYPE_MEC;
			ast_entry->next_hop = 1;
			break;
		default:
			ath11k_warn(ab, "unsupported ast_type %d", type);
			kfree(ast_entry);
			return -EINVAL;
	}

	INIT_LIST_HEAD(&ast_entry->ase_list);
	INIT_LIST_HEAD(&ast_entry->wmi_list);
	ast_entry->vdev_id = peer->vdev_id;
	ast_entry->pdev_idx = peer->pdev_idx;
	ast_entry->is_mapped = false;
	ast_entry->is_active = true;
	ast_entry->peer = peer;
	ast_entry->ar = ar;
	ether_addr_copy(ast_entry->addr, mac_addr);

	list_add_tail(&ast_entry->ase_list, &peer->ast_entry_list);

	ath11k_dbg(ab, ATH11K_DBG_MAC, "ath11k_peer_add_ast peer %pM ast_entry %pM, ast_type %d\n",
		   peer->addr, mac_addr, ast_entry->type);

	if ((ast_entry->type == ATH11K_AST_TYPE_WDS) ||
	    (ast_entry->type == ATH11K_AST_TYPE_MEC)) {
		ath11k_nss_add_wds_peer(ar, peer, mac_addr, ast_entry->type);
		ast_entry->action = ATH11K_WDS_WMI_ADD;
		list_add_tail(&ast_entry->wmi_list, &ab->wmi_ast_list);
		ieee80211_queue_work(ar->hw, &ab->wmi_ast_work);
	}

	ab->num_ast_entries++;
	return 0;
}

int ath11k_peer_update_ast(struct ath11k *ar, struct ath11k_peer *peer,
			   struct ath11k_ast_entry *ast_entry)
{
	struct ath11k_peer *old_peer = ast_entry->peer;
	struct ath11k_base *ab = ar->ab;

	lockdep_assert_held(&ab->base_lock);

	if (!ast_entry->is_mapped) {
		ath11k_warn(ab, "ath11k_peer_update_ast: ast_entry %pM not mapped yet\n",
			    ast_entry->addr);
		return -EINVAL;
	}

	if (ether_addr_equal(old_peer->addr, peer->addr) &&
	    (ast_entry->type == ATH11K_AST_TYPE_WDS) &&
	    (ast_entry->vdev_id == peer->vdev_id) &&
	    (ast_entry->is_active))
		return 0;

	if (peer && peer->delete_in_progress)
                return -EINVAL;

	ast_entry->vdev_id = peer->vdev_id;
	ast_entry->pdev_idx = peer->pdev_idx;
	ast_entry->type = ATH11K_AST_TYPE_WDS;
	ast_entry->is_active = true;
	ast_entry->peer = peer;

	list_move_tail(&ast_entry->ase_list, &peer->ast_entry_list);

	ath11k_dbg(ab, ATH11K_DBG_MAC, "ath11k_peer_update_ast old peer %pM new peer %pM ast_entry %pM\n",
		   old_peer->addr, peer->addr, ast_entry->addr);

	ast_entry->action = ATH11K_WDS_WMI_UPDATE;

	/* wmi_list entry might've been processed & removed.*/
	if (list_empty(&ast_entry->wmi_list))
		list_add_tail(&ast_entry->wmi_list, &ab->wmi_ast_list);
	else
		list_move_tail(&ast_entry->wmi_list, &ab->wmi_ast_list);

	ieee80211_queue_work(ar->hw, &ab->wmi_ast_work);

	return 0;
}

void ath11k_peer_map_ast(struct ath11k *ar, struct ath11k_peer *peer,
			 u8* mac_addr, u16 hw_peer_id, u16 ast_hash)
{
	struct ath11k_ast_entry *ast_entry = NULL;
	struct ath11k_base *ab = ar->ab;

	if (!peer)
		return;

	ast_entry = ath11k_peer_ast_find_by_peer(ab, peer, mac_addr);

	if (ast_entry) {
		ast_entry->ast_idx = hw_peer_id;
		ast_entry->is_active = true;
		ast_entry->is_mapped = true;
		ast_entry->ast_hash_value = ast_hash;

		if ((ast_entry->type == ATH11K_AST_TYPE_WDS) ||
		    (ast_entry->type == ATH11K_AST_TYPE_MEC))
			ath11k_nss_map_wds_peer(ar, peer, mac_addr, ast_entry);

		ath11k_dbg(ab, ATH11K_DBG_MAC, "ath11k_peer_map_ast peer %pM ast_entry %pM\n",
			   peer->addr, ast_entry->addr);
	}

}

void ath11k_peer_del_ast(struct ath11k *ar, struct ath11k_ast_entry *ast_entry)
{
	struct ath11k_peer *peer;
	struct ath11k_base *ab = ar->ab;

	if (!ast_entry || !ast_entry->peer)
		return;

	peer = ast_entry->peer;

	ath11k_dbg(ab, ATH11K_DBG_MAC, "ath11k_peer_del_ast pdev:%d peer %pM ast_entry %pM\n",
		   ar->pdev->pdev_id, peer->addr, ast_entry->addr);

	if ((ast_entry->type == ATH11K_AST_TYPE_WDS) ||
		(ast_entry->type == ATH11K_AST_TYPE_MEC)) {
		if (!list_empty(&ast_entry->wmi_list)) {
			ath11k_dbg(ab, ATH11K_DBG_MAC,
				   "ath11k_peer_del_ast deleting unprocessed ast entry %pM "
				   "of peer %pM from wmi list\n", ast_entry->addr, peer->addr);
			list_del_init(&ast_entry->wmi_list);
		}
	}
	list_del(&ast_entry->ase_list);

	/* WDS, MEC type AST entries need to be deleted on NSS */
	if (ast_entry->next_hop)
		ath11k_nss_del_wds_peer(ar, peer->addr, peer->peer_id,
					ast_entry->addr);

	kfree(ast_entry);
	ab->num_ast_entries--;
}

void ath11k_peer_ast_cleanup(struct ath11k *ar, struct ath11k_peer *peer,
                             bool is_wds, u32 free_wds_count)
{
	struct ath11k_ast_entry *ast_entry, *tmp;
	u32 ast_deleted_count = 0;

	if (peer->self_ast_entry) {
		ath11k_peer_del_ast(ar, peer->self_ast_entry);
		peer->self_ast_entry = NULL;
	}

	list_for_each_entry_safe(ast_entry, tmp, &peer->ast_entry_list,
				 ase_list) {
		if ((ast_entry->type == ATH11K_AST_TYPE_WDS) ||
		    (ast_entry->type == ATH11K_AST_TYPE_MEC))
			ast_deleted_count++;
		ath11k_peer_del_ast(ar, ast_entry);
	}

	if (!is_wds) {
		if (ast_deleted_count != free_wds_count)
			ath11k_warn(ar->ab, "ast_deleted_count (%d) mismatch on peer %pM free_wds_count (%d)!\n",
				    ast_deleted_count, peer->addr, free_wds_count);
		else
			ath11k_dbg(ar->ab, ATH11K_DBG_MAC, "ast_deleted_count (%d) on peer %pM free_wds_count (%d)\n",
				   ast_deleted_count, peer->addr, free_wds_count);
	}
}
#endif

void ath11k_peer_unmap_event(struct ath11k_base *ab, u16 peer_id)
{
	struct ath11k_peer *peer;

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_list_by_id(ab, peer_id);
	if (!peer) {
		ath11k_warn(ab, "peer-unmap-event: unknown peer id %d\n",
			    peer_id);
		goto exit;
	}

	if (peer->peer_logging_enabled)
		ath11k_dbg(ab, ATH11K_DBG_PEER, "peer unmap vdev %d peer %pM id %d\n",
			   peer->vdev_id, peer->addr, peer_id);

	list_del(&peer->list);
	kfree(peer);
	wake_up(&ab->peer_mapping_wq);

exit:
	spin_unlock_bh(&ab->base_lock);
}

void ath11k_peer_unmap_v2_event(struct ath11k_base *ab, u16 peer_id, u8 *mac_addr,
				bool is_wds, u32 free_wds_count)
{
	struct ath11k_peer *peer;
	struct ath11k *ar;

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_list_by_id(ab, peer_id);
	if (!peer) {
		ath11k_warn(ab, "peer-unmap-event: unknown peer id %d\n",
			    peer_id);
		goto exit;
	}

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, peer->vdev_id);
	if (!ar) {
		ath11k_warn(ab, "peer-unmap-event: unknown peer vdev id %d\n",
			    peer->vdev_id);
		goto free_peer;
	}

	ath11k_dbg(ab, ATH11K_DBG_DP_HTT, "htt peer unmap vdev %d peer %pM id %d is_wds %d free_wds_count %d\n",
		   peer->vdev_id, peer->addr, peer_id, is_wds, free_wds_count);

	if (ab->nss.enabled) {
		if (is_wds) {
			struct ath11k_ast_entry *ast_entry =
				ath11k_peer_ast_find_by_peer(ab, peer, mac_addr);

			if (ast_entry)
				ath11k_peer_del_ast(ar, ast_entry);
			rcu_read_unlock();
			goto exit;
		} else
			ath11k_peer_ast_cleanup(ar, peer, is_wds, free_wds_count);
	}

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	if (ar->bss_peer && ether_addr_equal(ar->bss_peer->addr, peer->addr))
		ar->bss_peer = NULL;
#endif
free_peer:
	rcu_read_unlock();
	list_del(&peer->list);
	kfree(peer);
	wake_up(&ab->peer_mapping_wq);

exit:
	spin_unlock_bh(&ab->base_lock);
}

void ath11k_peer_map_event(struct ath11k_base *ab, u8 vdev_id, u16 peer_id,
			   u8 *mac_addr, u16 ast_hash, u16 hw_peer_id)
{
	struct ath11k_peer *peer;
	struct ath11k *ar = NULL;

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, vdev_id);
	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find(ab, vdev_id, mac_addr);
	if (!peer) {
		peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
		if (!peer)
			goto exit;

		peer->vdev_id = vdev_id;
		peer->peer_id = peer_id;
		peer->ast_hash = ast_hash;
		peer->hw_peer_id = hw_peer_id;
		ether_addr_copy(peer->addr, mac_addr);
		list_add(&peer->list, &ab->peers);
		wake_up(&ab->peer_mapping_wq);
		if (ab->nss.enabled && ar)
			ath11k_nss_peer_create(ar, peer);
	}

	if (peer->peer_logging_enabled)
		ath11k_dbg(ab, ATH11K_DBG_PEER, "peer map vdev %d peer %pM id %d\n",
			   vdev_id, mac_addr, peer_id);

exit:
	spin_unlock_bh(&ab->base_lock);
	rcu_read_unlock();
}

void ath11k_peer_map_v2_event(struct ath11k_base *ab, u8 vdev_id, u16 peer_id,
			      u8 *mac_addr, u16 ast_hash, u16 hw_peer_id,
			      bool is_wds)
{
	struct ath11k_peer *peer;
	struct ath11k *ar = NULL;
	int ret;

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, vdev_id);
	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find(ab, vdev_id, mac_addr);
	if (!peer && !is_wds) {
		peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
		if (!peer) {
			ath11k_warn(ab, "failed to allocated peer for %pM vdev_id %d\n",
				    mac_addr, vdev_id);
			spin_unlock_bh(&ab->base_lock);
			goto exit;
		}

		peer->vdev_id = vdev_id;
		peer->peer_id = peer_id;
		peer->ast_hash = ast_hash;
		peer->hw_peer_id = hw_peer_id;
		ether_addr_copy(peer->addr, mac_addr);
		list_add(&peer->list, &ab->peers);
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
		INIT_LIST_HEAD(&peer->ast_entry_list);
#endif
		if (ab->nss.enabled && ar) {
			ret = ath11k_nss_peer_create(ar, peer);
			if (ret) {
				ath11k_warn(ab, "failed to do nss peer create: %d\n",
					    ret);
				goto peer_free;
			}
		}
		wake_up(&ab->peer_mapping_wq);
	}

	if (is_wds)
		peer = ath11k_peer_find_by_id(ab, peer_id);

	if (ab->nss.enabled && ar)
		ath11k_peer_map_ast(ar, peer, mac_addr, hw_peer_id, ast_hash);

	if (peer->peer_logging_enabled)
		ath11k_dbg(ab, ATH11K_DBG_PEER, "peer map vdev %d peer %pM id %d is_wds %d\n",
			   vdev_id, mac_addr, peer_id, is_wds);

	spin_unlock_bh(&ab->base_lock);
	goto exit;

peer_free:
	spin_unlock_bh(&ab->base_lock);
	mutex_lock(&ar->conf_mutex);
	ath11k_peer_delete(ar, vdev_id, mac_addr);
	mutex_unlock(&ar->conf_mutex);
exit:
	rcu_read_unlock();
}

static int ath11k_wait_for_peer_common(struct ath11k_base *ab, int vdev_id,
				       const u8 *addr, bool expect_mapped)
{
	int ret;

	ret = wait_event_timeout(ab->peer_mapping_wq, ({
				bool mapped;

				spin_lock_bh(&ab->base_lock);
				mapped = !!ath11k_peer_find(ab, vdev_id, addr);
				spin_unlock_bh(&ab->base_lock);

				(mapped == expect_mapped ||
				 test_bit(ATH11K_FLAG_CRASH_FLUSH, &ab->dev_flags));
				}), 3 * HZ);

	if (ret <= 0)
		return -ETIMEDOUT;

	return 0;
}

static inline int ath11k_peer_rhash_insert(struct ath11k_base *ab,
					   struct rhashtable *rtbl,
					   struct rhash_head *rhead,
					   struct rhashtable_params *params,
					   void *key)
{
	struct ath11k_peer *tmp;

	lockdep_assert_held(&ab->tbl_mtx_lock);

	tmp = rhashtable_lookup_get_insert_fast(rtbl, rhead, *params);

	if (!tmp)
		return 0;
	else if (IS_ERR(tmp))
		return PTR_ERR(tmp);
	else
		return -EEXIST;
}

static inline int ath11k_peer_rhash_remove(struct ath11k_base *ab,
					   struct rhashtable *rtbl,
					   struct rhash_head *rhead,
					   struct rhashtable_params *params)
{
	int ret;

	lockdep_assert_held(&ab->tbl_mtx_lock);

	ret = rhashtable_remove_fast(rtbl, rhead, *params);
	if (ret && ret != -ENOENT)
		return ret;

	return 0;
}

static int ath11k_peer_rhash_add(struct ath11k_base *ab, struct ath11k_peer *peer)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);
	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (!ab->rhead_peer_id || !ab->rhead_peer_addr)
		return -EPERM;

	ret = ath11k_peer_rhash_insert(ab, ab->rhead_peer_id, &peer->rhash_id,
				       &ab->rhash_peer_id_param, &peer->peer_id);
	if (ret) {
		ath11k_warn(ab, "failed to add peer %pM with id %d in rhash_id ret %d\n",
			    peer->addr, peer->peer_id, ret);
		return ret;
	}

	ret = ath11k_peer_rhash_insert(ab, ab->rhead_peer_addr, &peer->rhash_addr,
				       &ab->rhash_peer_addr_param, &peer->addr);
	if (ret) {
		ath11k_warn(ab, "failed to add peer %pM with id %d in rhash_addr ret %d\n",
			    peer->addr, peer->peer_id, ret);
		goto err_clean;
	}

	return 0;

err_clean:
	ath11k_peer_rhash_remove(ab, ab->rhead_peer_id, &peer->rhash_id,
				 &ab->rhash_peer_id_param);
	return ret;
}

void ath11k_peer_cleanup(struct ath11k *ar, u32 vdev_id)
{
	struct ath11k_peer *peer, *tmp_peer;
	struct ath11k_base *ab = ar->ab;
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	struct ath11k_ast_entry *ast_entry, *tmp_ast;
#endif

	lockdep_assert_held(&ar->conf_mutex);

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	mutex_lock(&ab->base_ast_lock);
#endif

	mutex_lock(&ab->tbl_mtx_lock);
	spin_lock_bh(&ab->base_lock);
	list_for_each_entry_safe(peer, tmp_peer, &ab->peers, list) {
		if (peer->vdev_id != vdev_id)
			continue;

		ath11k_warn(ab, "removing stale peer %pM from vdev_id %d\n",
			    peer->addr, vdev_id);

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
		if (peer->self_ast_entry) {
			ath11k_peer_del_ast(ar, peer->self_ast_entry);
			peer->self_ast_entry = NULL;
		}

		list_for_each_entry_safe(ast_entry, tmp_ast,
					 &peer->ast_entry_list, ase_list)
			ath11k_peer_del_ast(ar, ast_entry);
#endif

		ath11k_peer_rhash_delete(ab, peer);
		list_del(&peer->list);
		kfree(peer);
		ar->num_peers--;
	}

	spin_unlock_bh(&ab->base_lock);
	mutex_unlock(&ab->tbl_mtx_lock);
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	mutex_unlock(&ab->base_ast_lock);
#endif
}

static int ath11k_wait_for_peer_deleted(struct ath11k *ar, int vdev_id, const u8 *addr)
{
	return ath11k_wait_for_peer_common(ar->ab, vdev_id, addr, false);
}

int ath11k_wait_for_peer_delete_done(struct ath11k *ar, u32 vdev_id,
				     const u8 *addr)
{
	int ret;
	unsigned long time_left;

	ret = ath11k_wait_for_peer_deleted(ar, vdev_id, addr);
	if (ret) {
		ath11k_warn(ar->ab, "failed wait for peer deleted");
		return ret;
	}

	time_left = wait_for_completion_timeout(&ar->peer_delete_done,
						3 * HZ);
	if (time_left == 0) {
		ath11k_warn(ar->ab, "Timeout in receiving peer delete response\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int __ath11k_peer_delete(struct ath11k *ar, u32 vdev_id, const u8 *addr)
{
	int ret;
	struct ath11k_peer *peer;
	struct ath11k_base *ab = ar->ab;
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	struct ath11k_ast_entry *ast_entry, *tmp_ast;
#endif

	lockdep_assert_held(&ar->conf_mutex);

	reinit_completion(&ar->peer_delete_done);
	ath11k_nss_peer_delete(ar->ab, vdev_id, addr);

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	mutex_lock(&ab->base_ast_lock);
#endif
	mutex_lock(&ab->tbl_mtx_lock);
	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_by_addr(ab, addr);

	/* Fallback to peer list search if the correct peer can't be found.
	 * Skip the deletion of the peer from the rhash since it has already
	 * been deleted in peer add.
	 */
	if (!peer)
		peer = ath11k_peer_find(ab, vdev_id, addr);

	if (!peer) {
		spin_unlock_bh(&ab->base_lock);
		mutex_unlock(&ab->tbl_mtx_lock);

		ath11k_warn(ab,
			    "failed to find peer vdev_id %d addr %pM in delete\n",
			    vdev_id, addr);
		return -EINVAL;
	}

	if (peer) {
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
		peer->delete_in_progress = true;
		if (peer->self_ast_entry) {
			ath11k_peer_del_ast(ar, peer->self_ast_entry);
			peer->self_ast_entry = NULL;
		}

		list_for_each_entry_safe(ast_entry, tmp_ast,
					 &peer->ast_entry_list, ase_list)
			if ((ast_entry->type == ATH11K_AST_TYPE_WDS) ||
			    (ast_entry->type == ATH11K_AST_TYPE_MEC)) {
				if (!list_empty(&ast_entry->wmi_list)) {
					ath11k_dbg(ar->ab, ATH11K_DBG_MAC,
						   "%s deleting unprocessed ast entry %pM of peer %pM from wmi list\n",
						   __func__, ast_entry->addr, addr);
					list_del_init(&ast_entry->wmi_list);
				}
			}
#endif
		ath11k_peer_rhash_delete(ab, peer);
	}

	spin_unlock_bh(&ab->base_lock);
	mutex_unlock(&ab->tbl_mtx_lock);

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	mutex_unlock(&ab->base_ast_lock);
#endif

	ret = ath11k_wmi_send_peer_delete_cmd(ar, addr, vdev_id);
	if (ret) {
		ath11k_warn(ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    vdev_id, addr, ret);
		return ret;
	}

	ret = ath11k_wait_for_peer_delete_done(ar, vdev_id, addr);
	if (ret)
		return ret;

	return 0;
}

int ath11k_peer_delete(struct ath11k *ar, u32 vdev_id, u8 *addr)
{
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	ret = __ath11k_peer_delete(ar, vdev_id, addr);
	if (ret)
		return ret;

	ATH11K_MEMORY_STATS_DEC(ar->ab, per_peer_object,
				sizeof(struct ath11k_peer));

	ar->num_peers--;

	return 0;
}

static int ath11k_wait_for_peer_created(struct ath11k *ar, int vdev_id, const u8 *addr)
{
	return ath11k_wait_for_peer_common(ar->ab, vdev_id, addr, true);
}

int ath11k_peer_create(struct ath11k *ar, struct ath11k_vif *arvif,
		       struct ieee80211_sta *sta, struct peer_create_params *param)
{
	struct ath11k_peer *peer;
	struct ieee80211_vif *vif = arvif->vif;
	struct ath11k_sta *arsta;
	int ret, fbret;

	lockdep_assert_held(&ar->conf_mutex);

	if (ar->num_peers > (ar->max_num_peers - 1)) {
		ath11k_warn(ar->ab,
			    "failed to create peer due to insufficient peer entry resource in firmware\n");
		return -ENOBUFS;
	}

	mutex_lock(&ar->ab->tbl_mtx_lock);
	spin_lock_bh(&ar->ab->base_lock);
	peer = ath11k_peer_find_by_addr(ar->ab, param->peer_addr);
	if (peer) {
		if (peer->vdev_id == param->vdev_id) {
			spin_unlock_bh(&ar->ab->base_lock);
			mutex_unlock(&ar->ab->tbl_mtx_lock);
			return -EINVAL;
		}

		/* Assume sta is transitioning to another band.
		 * Remove here the peer from rhash.
		 */
		ath11k_peer_rhash_delete(ar->ab, peer);
	}
	spin_unlock_bh(&ar->ab->base_lock);
	mutex_unlock(&ar->ab->tbl_mtx_lock);

	ret = ath11k_wmi_send_peer_create_cmd(ar, param);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send peer create vdev_id %d ret %d\n",
			    param->vdev_id, ret);
		return ret;
	}

	ret = ath11k_wait_for_peer_created(ar, param->vdev_id,
					   param->peer_addr);
	if (ret)
		return ret;

	mutex_lock(&ar->ab->tbl_mtx_lock);
	spin_lock_bh(&ar->ab->base_lock);

	peer = ath11k_peer_find(ar->ab, param->vdev_id, param->peer_addr);
	if (!peer) {
		spin_unlock_bh(&ar->ab->base_lock);
		mutex_unlock(&ar->ab->tbl_mtx_lock);
		ath11k_warn(ar->ab, "failed to find peer %pM on vdev %i after creation\n",
			    param->peer_addr, param->vdev_id);

		ret = -ENOENT;
		goto cleanup;
	}

	ret = ath11k_peer_rhash_add(ar->ab, peer);
	if (ret) {
		spin_unlock_bh(&ar->ab->base_lock);
		mutex_unlock(&ar->ab->tbl_mtx_lock);
		goto cleanup;
	}

	peer->pdev_idx = ar->pdev_idx;
	peer->sta = sta;

	if (arvif->vif->type == NL80211_IFTYPE_STATION) {
		arvif->ast_hash = peer->ast_hash;
		arvif->ast_idx = peer->hw_peer_id;
	}

	peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;
	peer->vif = vif;

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	if (vif->type == NL80211_IFTYPE_STATION && ar->ab->nss.enabled)
		ar->bss_peer = peer;
	else
		ar->bss_peer = NULL;
#endif

	if (sta) {
		arsta = ath11k_sta_to_arsta(sta);
		arsta->tcl_metadata |= FIELD_PREP(HTT_TCL_META_DATA_TYPE, 0) |
				       FIELD_PREP(HTT_TCL_META_DATA_PEER_ID,
						  peer->peer_id);

		/* set HTT extension valid bit to 0 by default */
		arsta->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT;
	}

	ATH11K_MEMORY_STATS_INC(ar->ab, per_peer_object, sizeof(*peer));

	ar->num_peers++;

	if (ath11k_mac_sta_level_info(arvif, sta)) {
		ath11k_dbg(ar->ab, ATH11K_DBG_PEER, "peer created %pM\n", param->peer_addr);
		peer->peer_logging_enabled = true;
	}

	spin_unlock_bh(&ar->ab->base_lock);
	mutex_unlock(&ar->ab->tbl_mtx_lock);

	return 0;

cleanup:
	fbret = __ath11k_peer_delete(ar, param->vdev_id, param->peer_addr);
	if (fbret)
		ath11k_warn(ar->ab, "failed peer %pM delete vdev_id %d fallback ret %d\n",
			    param->peer_addr, param->vdev_id, fbret);

	return ret;
}

int ath11k_peer_rhash_delete(struct ath11k_base *ab, struct ath11k_peer *peer)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);
	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (!ab->rhead_peer_id || !ab->rhead_peer_addr)
		return -EPERM;

	ret = ath11k_peer_rhash_remove(ab, ab->rhead_peer_addr, &peer->rhash_addr,
				       &ab->rhash_peer_addr_param);
	if (ret) {
		ath11k_warn(ab, "failed to remove peer %pM id %d in rhash_addr ret %d\n",
			    peer->addr, peer->peer_id, ret);
		return ret;
	}

	ret = ath11k_peer_rhash_remove(ab, ab->rhead_peer_id, &peer->rhash_id,
				       &ab->rhash_peer_id_param);
	if (ret) {
		ath11k_warn(ab, "failed to remove peer %pM id %d in rhash_id ret %d\n",
			    peer->addr, peer->peer_id, ret);
		return ret;
	}

	return 0;
}

static int ath11k_peer_rhash_id_tbl_init(struct ath11k_base *ab)
{
	struct rhashtable_params *param;
	struct rhashtable *rhash_id_tbl;
	int ret;
	size_t size;

	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (ab->rhead_peer_id)
		return 0;

	size = sizeof(*ab->rhead_peer_id);
	rhash_id_tbl = kzalloc(size, GFP_KERNEL);
	if (!rhash_id_tbl) {
		ath11k_warn(ab, "failed to init rhash id table due to no mem (size %zu)\n",
			    size);
		return -ENOMEM;
	}

	param = &ab->rhash_peer_id_param;

	param->key_offset = offsetof(struct ath11k_peer, peer_id);
	param->head_offset = offsetof(struct ath11k_peer, rhash_id);
	param->key_len = sizeof_field(struct ath11k_peer, peer_id);
	param->automatic_shrinking = true;
	param->nelem_hint = ab->num_radios * TARGET_NUM_PEERS_PDEV(ab);

	ret = rhashtable_init(rhash_id_tbl, param);
	if (ret) {
		ath11k_warn(ab, "failed to init peer id rhash table %d\n", ret);
		goto err_free;
	}

	spin_lock_bh(&ab->base_lock);

	if (!ab->rhead_peer_id) {
		ab->rhead_peer_id = rhash_id_tbl;
	} else {
		spin_unlock_bh(&ab->base_lock);
		goto cleanup_tbl;
	}

	spin_unlock_bh(&ab->base_lock);

	return 0;

cleanup_tbl:
	rhashtable_destroy(rhash_id_tbl);
err_free:
	kfree(rhash_id_tbl);

	return ret;
}

static int ath11k_peer_rhash_addr_tbl_init(struct ath11k_base *ab)
{
	struct rhashtable_params *param;
	struct rhashtable *rhash_addr_tbl;
	int ret;
	size_t size;

	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (ab->rhead_peer_addr)
		return 0;

	size = sizeof(*ab->rhead_peer_addr);
	rhash_addr_tbl = kzalloc(size, GFP_KERNEL);
	if (!rhash_addr_tbl) {
		ath11k_warn(ab, "failed to init rhash addr table due to no mem (size %zu)\n",
			    size);
		return -ENOMEM;
	}

	param = &ab->rhash_peer_addr_param;

	param->key_offset = offsetof(struct ath11k_peer, addr);
	param->head_offset = offsetof(struct ath11k_peer, rhash_addr);
	param->key_len = sizeof_field(struct ath11k_peer, addr);
	param->automatic_shrinking = true;
	param->nelem_hint = ab->num_radios * TARGET_NUM_PEERS_PDEV(ab);

	ret = rhashtable_init(rhash_addr_tbl, param);
	if (ret) {
		ath11k_warn(ab, "failed to init peer addr rhash table %d\n", ret);
		goto err_free;
	}

	spin_lock_bh(&ab->base_lock);

	if (!ab->rhead_peer_addr) {
		ab->rhead_peer_addr = rhash_addr_tbl;
	} else {
		spin_unlock_bh(&ab->base_lock);
		goto cleanup_tbl;
	}

	spin_unlock_bh(&ab->base_lock);

	return 0;

cleanup_tbl:
	rhashtable_destroy(rhash_addr_tbl);
err_free:
	kfree(rhash_addr_tbl);

	return ret;
}

static inline void ath11k_peer_rhash_id_tbl_destroy(struct ath11k_base *ab)
{
	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (!ab->rhead_peer_id)
		return;

	rhashtable_destroy(ab->rhead_peer_id);
	kfree(ab->rhead_peer_id);
	ab->rhead_peer_id = NULL;
}

static inline void ath11k_peer_rhash_addr_tbl_destroy(struct ath11k_base *ab)
{
	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (!ab->rhead_peer_addr)
		return;

	rhashtable_destroy(ab->rhead_peer_addr);
	kfree(ab->rhead_peer_addr);
	ab->rhead_peer_addr = NULL;
}

int ath11k_peer_rhash_tbl_init(struct ath11k_base *ab)
{
	int ret;

	mutex_lock(&ab->tbl_mtx_lock);

	ret = ath11k_peer_rhash_id_tbl_init(ab);
	if (ret)
		goto out;

	ret = ath11k_peer_rhash_addr_tbl_init(ab);
	if (ret)
		goto cleanup_tbl;

	mutex_unlock(&ab->tbl_mtx_lock);

	return 0;

cleanup_tbl:
	ath11k_peer_rhash_id_tbl_destroy(ab);
out:
	mutex_unlock(&ab->tbl_mtx_lock);
	return ret;
}

void ath11k_peer_rhash_tbl_destroy(struct ath11k_base *ab)
{
	mutex_lock(&ab->tbl_mtx_lock);

	ath11k_peer_rhash_addr_tbl_destroy(ab);
	ath11k_peer_rhash_id_tbl_destroy(ab);

	mutex_unlock(&ab->tbl_mtx_lock);
}
