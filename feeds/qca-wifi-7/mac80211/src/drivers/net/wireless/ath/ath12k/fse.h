/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_FSE_H
#define ATH12K_FSE_H

#include <ath/ath_fse.h>

#define ATH12K_RX_FSE_FLOW_MATCH_SFE 0xAA00
#define ATH12K_RX_FSE_FLOW_MATCH_USE_PPE 0x00FF
#define ATH12K_RX_FSE_FLOW_MSCS_RULE_PROGRAMMED 0x4000

#define ATH12K_FSE_MAGIC_NUM      0xAA
#define ATH12K_FSE_MAGIC_NUM_MASK GENMASK(7, 0)
#define ATH12K_PPE_VP_NUM         GENMASK(15, 8)

enum fse_operation {
	FSE_RULE_ADD,
	FSE_RULE_DELETE,
	FSE_RULE_MAX,
};

static inline char *fse_state_to_string(enum fse_operation operation)
{
	switch (operation) {
	case FSE_RULE_ADD:
		return "ADD";
	case FSE_RULE_DELETE:
		return "DELETE";
	default:
		return "UNKNOWN_COMMAND";
	}
}

void ath12k_fse_init(struct ath12k_base *ab);
void ath12k_fse_deinit(struct ath12k_base *ab);
void *ath12k_sfe_get_ab_from_vif(struct ieee80211_vif *vif,
				 const u8 *peer_mac);
int ath12k_sfe_add_flow_entry(void *ptr,
			      u32 *src_ip, u32 src_port,
			      u32 *dest_ip, u32 dest_port,
			      u8 protocol, u8 version);
int ath12k_sfe_delete_flow_entry(void *ptr,
				 u32 *src_ip, u32 src_port,
				 u32 *dest_ip, u32 dest_port,
				 u8 protocol, u8 version);
#endif
