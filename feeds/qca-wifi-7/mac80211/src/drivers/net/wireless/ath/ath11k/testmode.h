/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "hif.h"

#define WMI_TLV_HDR_SIZE 4
#ifdef CPTCFG_NL80211_TESTMODE

void ath11k_tm_wmi_event(struct ath11k_base *ab, u32 cmd_id, struct sk_buff *skb);
int ath11k_tm_cmd(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		  u8 link_id, void *data, int len);

void ath11k_fwlog_write(struct ath11k_base *ab,  u8 *data, int len);
#else

static inline void ath11k_tm_wmi_event(struct ath11k_base *ab, u32 cmd_id,
				       struct sk_buff *skb)
{
}

static inline int ath11k_tm_cmd(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				u8 link_id, void *data, int len)
{
	return 0;
}

static inline void ath11k_fwlog_write(struct ath11k_base *ab,  u8 *data,
				     int len)
{

}
#endif
