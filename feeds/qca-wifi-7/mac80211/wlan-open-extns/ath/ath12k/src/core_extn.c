/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/completion.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include <linux/of.h>
#include "../core.h"
#include "../ce.h"
#include "../hw.h"
#include "ath12k_cmn_extn.h"
#include "../net/mac80211/ieee80211_i.h"


void ath12k_wmi_peer_migration_event_extn(struct ath12k_vif *ahvif)
{
	struct sta_240mhz_info *sta, *tmp;

	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	list_for_each_entry_safe(sta, tmp, &ahvif->ath12k_vif_extn.peer_240mhz_list_extn,
				 list) {
		list_del(&sta->list);
		kfree(sta);
	}

	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);
}

void ath12k_mac_hw_allocate_extn(struct ieee80211_hw *hw,
			  const struct ieee80211_ops_extn *ops_extn)
{
	struct ieee80211_local *local = wiphy_priv(hw->wiphy);
	local->ops_extn = ops_extn;
}

int
ath12k_add_sta_240mhz_info_extn(struct ath12k_vif *ahvif,
				u8 *mac,
				struct ieee80211_240mhz_vendor_oper_extn
				*params)
{
	struct sta_240mhz_info *sta;

	sta = kmalloc(sizeof(*sta), GFP_KERNEL);
	if (!sta)
		return -ENOMEM;

	ether_addr_copy(sta->mac_addr, mac);
	memcpy(&sta->params_240MHz_extn, params,
	       sizeof(sta->params_240MHz_extn));
	INIT_LIST_HEAD(&sta->list);
	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	list_add_tail(&sta->list, &ahvif->ath12k_vif_extn.peer_240mhz_list_extn);
	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);

	return 0;
}

void ath12k_mac_init_arvif_extn(struct ath12k_vif *ahvif)
{
	/* Protects the extensions data update for an interface*/
	spin_lock_init(&ahvif->ath12k_vif_extn.data_lock);

	INIT_LIST_HEAD(&ahvif->ath12k_vif_extn.peer_240mhz_list_extn);
}

void ath12k_mac_setup_radio_iface_comb_extn(
		struct ieee80211_iface_combination *comb)
{
	/* Add 320MHz support for 240MHz */
	comb->radar_detect_widths |= BIT(NL80211_CHAN_WIDTH_320);
}
