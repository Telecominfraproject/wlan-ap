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
#include "../../core.h"
#include "../../ce.h"
#include "../../hw.h"
#include "../../qcn_extns/ath12k_cmn_extn.h"


struct ieee80211_240mhz_vendor_oper_extn*
ath12k_get_240_mhz_cap_extn(struct ieee80211_vif *vif,
			    struct ieee80211_link_sta *link_sta)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct sta_240mhz_info *sta;

	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	list_for_each_entry(sta, &ahvif->ath12k_vif_extn.peer_240mhz_list_extn, list) {
		if (sta && !memcmp(sta->mac_addr, link_sta->addr, ETH_ALEN)) {
			spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);
			return &sta->params_240MHz_extn;
		}
	}

	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);

	return NULL;
}

static const struct ieee80211_ops_extn ath12k_ops_wifi7_extn = {
	.get_240mhz_cap_extn = ath12k_get_240_mhz_cap_extn,
};

void ath12k_wifi7_hw_init_extn(struct ath12k_base *ab)
{
	ab->ath12k_ops_extn = &ath12k_ops_wifi7_extn;
}
