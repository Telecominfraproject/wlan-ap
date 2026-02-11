/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_PEER_H
#define ATH12K_PEER_H

#include "dp_peer.h"

void ath12k_peer_cleanup(struct ath12k *ar, u32 vdev_id);
int ath12k_peer_delete(struct ath12k *ar, u32 vdev_id, u8 *addr,
		       struct ath12k_sta *ahsta);
int ath12k_peer_create(struct ath12k *ar, struct ath12k_link_vif *arvif,
		       struct ieee80211_sta *sta,
		       struct ath12k_wmi_peer_create_arg *arg);
int ath12k_wait_for_peer_delete_done(struct ath12k *ar, u32 vdev_id,
				     const u8 *addr);
int ath12k_peer_mlo_link_peers_delete(struct ath12k_vif *ahvif, struct ath12k_sta *ahsta);
int ath12k_peer_send_assoc_vendor_response(const struct ath12k_dp_link_peer *peer,
					   const bool is_assoc);

int ath12k_link_sta_rhash_tbl_init(struct ath12k_base *ab);
void ath12k_link_sta_rhash_tbl_destroy(struct ath12k_base *ab);
int ath12k_link_sta_rhash_delete(struct ath12k_base *ab, struct ath12k_link_sta *arsta);
int ath12k_link_sta_rhash_add(struct ath12k_base *ab, struct ath12k_link_sta *arsta);
struct ath12k_link_sta *ath12k_link_sta_find_by_addr(struct ath12k_base *ab, const u8 *addr);
u16 ath12k_peer_ml_alloc(struct ath12k_hw *ah);
void ath12k_peer_ml_free(struct ath12k_hw *ah, struct ath12k_sta *ahsta);
void ath12k_mac_peer_disassoc(struct ath12k_base *ab, struct ieee80211_sta *sta,
			      struct ath12k_sta *ahsta,
			      enum ath12k_debug_mask debug_mask);
int ath12k_peer_mlo_link_peer_delete(struct ath12k_link_vif *arvif,
				     struct ath12k_link_sta *arsta);
static inline
struct ath12k_link_sta *ath12k_peer_get_link_sta(struct ath12k_base *ab,
						 struct ath12k_dp_link_peer *peer)
{
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;

	if (!peer->sta)
		return NULL;

	ahsta = ath12k_sta_to_ahsta(peer->sta);
	if (peer->ml_id & ATH12K_PEER_ML_ID_VALID) {
		if (!(ahsta->links_map & BIT(peer->link_id))) {
			ath12k_warn(ab, "peer %pM id %d link_id %d can't found in STA link_map 0x%x\n",
				    peer->addr, peer->peer_id, peer->link_id, ahsta->links_map);
			return NULL;
		}
		arsta = rcu_dereference(ahsta->link[peer->link_id]);
		if (!arsta)
			return NULL;
		} else {
			arsta =  &ahsta->deflink;
		}
	return arsta;
}
#endif /* _PEER_H_ */
