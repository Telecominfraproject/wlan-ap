/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "hif.h"

#ifdef CPTCFG_NL80211_TESTMODE

void ath12k_tm_wmi_event_unsegmented(struct ath12k_base *ab, u32 cmd_id,
				     struct sk_buff *skb);
void ath12k_tm_process_event(struct ath12k_base *ab, u32 cmd_id,
			     const void **tb,
			     u16 length);
int ath12k_tm_cmd(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		  u8 link_id, void *data, int len);
void ath12k_fwlog_write(struct ath12k_base *ab, u8 *data, int len);

#else

static inline void ath12k_tm_wmi_event_unsegmented(struct ath12k_base *ab, u32 cmd_id,
						   struct sk_buff *skb)
{
}

static inline void ath12k_tm_process_event(struct ath12k_base *ab, u32 cmd_id,
					   const void **tb,
					   u16 length)
{
}

static inline int ath12k_tm_cmd(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				u8 link_id, void *data,
				int len)
{
	return 0;
}

static inline void ath12k_fwlog_write(struct ath12k_base *ab, u8 *data, int len)
{

}
#endif
