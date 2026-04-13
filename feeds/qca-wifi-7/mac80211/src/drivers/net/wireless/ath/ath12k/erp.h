/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef ATH12K_ERP_H
#define ATH12K_ERP_H

enum ath12k_erp_states {
	ATH12K_ERP_OFF,
	ATH12K_ERP_ENTER_STARTED,
	ATH12K_ERP_ENTER_COMPLETE,
};

int ath12k_vendor_parse_rm_erp(struct wiphy *wiphy, struct wireless_dev *wdev,
			       struct nlattr *attrs);
void ath12k_erp_init(void);
void ath12k_erp_deinit(void);
bool ath12k_erp_in_progress(void);
void ath12k_erp_handle_trigger(struct work_struct *work);
void ath12k_erp_handle_ssr(struct ath12k *ar);
int ath12k_erp_enter(struct ieee80211_hw *hw, struct ieee80211_vif *vif, int link_id,
		     struct cfg80211_erp_params *params);
int ath12k_erp_exit(struct wiphy *wiphy, bool send_event);
enum ath12k_erp_states ath12k_erp_get_sm_state(void);
void ath12k_erp_ssr_exit(struct work_struct *work);
#endif /* ATH12K_ERP_H */

