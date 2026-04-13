// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "debug.h"
#include "fse.h"
#include "dp_rx.h"
#include "peer.h"
#include <linux/module.h>

bool ath12k_fse_enable = true;
module_param_named(fse, ath12k_fse_enable, bool, 0444);
MODULE_PARM_DESC(fse, "Enable FSE feature (Default: true)");

static const struct ath_fse_ops ath_fse_ops_obj = {
	.fse_rule_add = ath12k_sfe_add_flow_entry,
	.fse_rule_delete = ath12k_sfe_delete_flow_entry,
	.fse_get_ab = ath12k_sfe_get_ab_from_vif,
};

void ath12k_fse_init(struct ath12k_base *ab)
{
	const struct ath_fse_ops *fse_ops_ptr;

	fse_ops_ptr = &ath_fse_ops_obj;
	if (!ath12k_fse_enable)
		return;

	if (ath_fse_ops_callback_register(fse_ops_ptr)) {
		ath12k_err(ab, "ath12k callback register fail\n");
		return;
	}
	ath12k_dbg(ab, ATH12K_DBG_DP_RX, "FSE context initialized\n");
}

void ath12k_fse_deinit(struct ath12k_base *ab)
{
	if (!ath12k_fse_enable)
		return;

	ath_fse_ops_callback_unregister();

	ath12k_dbg(ab, ATH12K_DBG_DP_RX, "FSE context deinitialized\n");
}

void *ath12k_sfe_get_ab_from_vif(struct ieee80211_vif *vif,
				 const u8 *peer_mac)
{
	struct ath12k_base *ab = NULL, *resultant_ab = NULL;
	struct ath12k *ar;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	unsigned long links;
	u8 link_id;

	if (!vif)
		return NULL;

	ahvif = ath12k_vif_to_ahvif(vif);
	links = ahvif->links_map;

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ahvif->link[link_id];
		if (!arvif)
			continue;

		ar = arvif->ar;
		if (!ar)
			continue;
		ab = ar->ab;
		spin_lock_bh(&ab->dp->dp_lock);
		peer = ath12k_dp_link_peer_find_by_addr(ath12k_ab_to_dp(ab), peer_mac);
		if (peer)
			resultant_ab = ab;
		spin_unlock_bh(&ab->dp->dp_lock);

		if (resultant_ab)
			return resultant_ab;
	}
	return ab;
}

static void
ath12k_sfe_update_flow_info(struct rx_flow_info *flow_info,
			    u32 *src_ip, u32 src_port,
			    u32 *dest_ip, u32 dest_port,
			    u8 protocol, u8 version, int operation)
{
	struct hal_flow_tuple_info *tuple_info = &flow_info->flow_tuple_info;

	ath12k_generic_dbg(ATH12K_DBG_DP_FST,
			   "%s S_IP:%x:%x:%x:%x,sPort:%u,D_IP:%x:%x:%x:%x,dPort:%u,Proto:%d,Ver:%d\n",
			   fse_state_to_string(operation),
			   src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port,
			   dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3],
			   dest_port, protocol, version);

	tuple_info->src_port = src_port;
	tuple_info->dest_port = dest_port;
	tuple_info->l4_protocol = protocol;

	flow_info->fse_metadata = ATH12K_RX_FSE_FLOW_MATCH_SFE;

	if (version == 4) {
		flow_info->is_addr_ipv4 = 1;
		tuple_info->src_ip_31_0 = src_ip[0];
		tuple_info->dest_ip_31_0 = dest_ip[0];
	} else if (version == 6) {
		tuple_info->src_ip_31_0 = src_ip[3];
		tuple_info->src_ip_63_32 = src_ip[2];
		tuple_info->src_ip_95_64 = src_ip[1];
		tuple_info->src_ip_127_96 = src_ip[0];
		tuple_info->dest_ip_31_0 = dest_ip[3];
		tuple_info->dest_ip_63_32 = dest_ip[2];
		tuple_info->dest_ip_95_64 = dest_ip[1];
		tuple_info->dest_ip_127_96 = dest_ip[0];
	}
}

int ath12k_sfe_add_flow_entry(void *ptr,
			      u32 *src_ip, u32 src_port,
			      u32 *dest_ip, u32 dest_port,
			      u8 protocol, u8 version)

{
	struct rx_flow_info flow_info = {0};
	struct ath12k_base *ab = (struct ath12k_base *)ptr;

	if (!ath12k_fse_enable)
		return -EINVAL;

	ath12k_sfe_update_flow_info(&flow_info, src_ip, src_port, dest_ip,
				    dest_port, protocol, version, FSE_RULE_ADD);

	return ath12k_dp_rx_flow_add_entry(ab, &flow_info);
}

int ath12k_sfe_delete_flow_entry(void *ptr,
				 u32 *src_ip, u32 src_port,
				 u32 *dest_ip, u32 dest_port,
				 u8 protocol, u8 version)
{
	struct rx_flow_info flow_info = {0};
	struct ath12k_hw *ah = NULL;
	struct ath12k *ar;
	struct ieee80211_hw *hw = (struct ieee80211_hw *)ptr;

	if (!ath12k_fse_enable)
		return -EINVAL;

	ah = hw->priv;
	if (!ah) {
		ath12k_err(NULL, "HW invalid-Flow delete failed:S_IP:%x:%x:%x:%x,sPort:%u,D_IP:%x:%x:%x:%x,dPort:%u,Proto:%d,Ver:%d",
			   src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port, dest_ip[0],
			   dest_ip[1], dest_ip[2], dest_ip[3], dest_port, protocol, version);
		return -EINVAL;
	}

	ar = ah->radio;
	ath12k_sfe_update_flow_info(&flow_info, src_ip, src_port, dest_ip,
				    dest_port, protocol, version, FSE_RULE_DELETE);

	return ath12k_dp_rx_flow_delete_entry(ar->ab, &flow_info);
}
