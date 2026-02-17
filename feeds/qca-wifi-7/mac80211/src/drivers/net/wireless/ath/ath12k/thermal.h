/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
#include <soc/qcom/eawtp.h>
#include "core.h"
#endif

#ifndef _ATH12K_THERMAL_
#define _ATH12K_THERMAL_

/* Below temperatures are in celsius */
#define ATH12K_THERMAL_IPA_LVL0_TEMP_LOW_MARK -100
#define ATH12K_THERMAL_IPA_LVL1_TEMP_LOW_MARK 110
#define ATH12K_THERMAL_IPA_LVL2_TEMP_LOW_MARK 115
#define ATH12K_THERMAL_IPA_LVL3_TEMP_LOW_MARK 120
#define ATH12K_THERMAL_IPA_LVL4_TEMP_LOW_MARK 125
#define ATH12K_THERMAL_IPA_LVL0_TEMP_HIGH_MARK 115
#define ATH12K_THERMAL_IPA_LVL1_TEMP_HIGH_MARK 120
#define ATH12K_THERMAL_IPA_LVL2_TEMP_HIGH_MARK 125
#define ATH12K_THERMAL_IPA_LVL3_TEMP_HIGH_MARK 130
#define ATH12K_THERMAL_IPA_LVL4_TEMP_HIGH_MARK 130

#define ATH12K_THERMAL_XFRM_LVL0_TEMP_LOW_MARK -100
#define ATH12K_THERMAL_XFRM_LVL1_TEMP_LOW_MARK 100
#define ATH12K_THERMAL_XFRM_LVL2_TEMP_LOW_MARK 105
#define ATH12K_THERMAL_XFRM_LVL3_TEMP_LOW_MARK 110
#define ATH12K_THERMAL_XFRM_LVL4_TEMP_LOW_MARK 115
#define ATH12K_THERMAL_XFRM_LVL0_TEMP_HIGH_MARK 105
#define ATH12K_THERMAL_XFRM_LVL1_TEMP_HIGH_MARK 110
#define ATH12K_THERMAL_XFRM_LVL2_TEMP_HIGH_MARK 115
#define ATH12K_THERMAL_XFRM_LVL3_TEMP_HIGH_MARK 120
#define ATH12K_THERMAL_XFRM_LVL4_TEMP_HIGH_MARK 120

#define ATH12K_THERMAL_XFRM_LVL0_TEMP_LOW_MARK_IPQ5424 -100
#define ATH12K_THERMAL_XFRM_LVL1_TEMP_LOW_MARK_IPQ5424 100
#define ATH12K_THERMAL_XFRM_LVL2_TEMP_LOW_MARK_IPQ5424 105
#define ATH12K_THERMAL_XFRM_LVL3_TEMP_LOW_MARK_IPQ5424 110
#define ATH12K_THERMAL_XFRM_LVL4_TEMP_LOW_MARK_IPQ5424 125
#define ATH12K_THERMAL_XFRM_LVL0_TEMP_HIGH_MARK_IPQ5424 105
#define ATH12K_THERMAL_XFRM_LVL1_TEMP_HIGH_MARK_IPQ5424 110
#define ATH12K_THERMAL_XFRM_LVL2_TEMP_HIGH_MARK_IPQ5424 120
#define ATH12K_THERMAL_XFRM_LVL3_TEMP_HIGH_MARK_IPQ5424 130
#define ATH12K_THERMAL_XFRM_LVL4_TEMP_HIGH_MARK_IPQ5424 130

#define ATH12K_THERMAL_IPA_LVL0_TEMP_LOW_MARK_IPQ5424 -100
#define ATH12K_THERMAL_IPA_LVL1_TEMP_LOW_MARK_IPQ5424 120
#define ATH12K_THERMAL_IPA_LVL2_TEMP_LOW_MARK_IPQ5424 125
#define ATH12K_THERMAL_IPA_LVL3_TEMP_LOW_MARK_IPQ5424 130
#define ATH12K_THERMAL_IPA_LVL4_TEMP_LOW_MARK_IPQ5424 135
#define ATH12K_THERMAL_IPA_LVL0_TEMP_HIGH_MARK_IPQ5424 125
#define ATH12K_THERMAL_IPA_LVL1_TEMP_HIGH_MARK_IPQ5424 130
#define ATH12K_THERMAL_IPA_LVL2_TEMP_HIGH_MARK_IPQ5424 135
#define ATH12K_THERMAL_IPA_LVL3_TEMP_HIGH_MARK_IPQ5424 140
#define ATH12K_THERMAL_IPA_LVL4_TEMP_HIGH_MARK_IPQ5424 140

#define ATH12K_THERMAL_LVL0_DUTY_CYCLE 0
#define ATH12K_THERMAL_LVL1_DUTY_CYCLE 0
#define ATH12K_THERMAL_LVL2_DUTY_CYCLE 50
#define ATH12K_THERMAL_LVL3_DUTY_CYCLE 90
#define ATH12K_THERMAL_LVL4_DUTY_CYCLE 100

#define THERMAL_CONFIG_POUT0                        0
#define THERMAL_CONFIG_POUT1                        12
#define THERMAL_CONFIG_POUT2                        12
#define THERMAL_CONFIG_POUT3                        12
#define THERMAL_CONFIG_POUT4                        12

#define ATH12K_IPA_THERMAL_LEVEL 0
#define ATH12K_XFRM_THERMAL_LEVEL 1
#define ATH12K_XFRM_IPQ5424_THERMAL_LEVEL 2
#define ATH12K_IPA_IPQ5424_THERMAL_LEVEL 3

#define ATH12K_FW_THERMAL_THROTTLING_ENABLE  1
#define ATH12K_THERMAL_THROTTLE_MAX     100
#define ATH12K_THERMAL_DEFAULT_DUTY_CYCLE 100
#define ATH12K_HWMON_NAME_LEN           15
#define ATH12K_THERMAL_SYNC_TIMEOUT_HZ (5 * HZ)

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
#define ETH_PORT_COUNT 3
#define ATH12K_DEFAULT_POWER_REDUCTION 0xFF
#define ACTIVE_PDEV_TH 2
#endif

extern struct tt_level_config tt_level_configs[ATH12K_THERMAL_LEVELS][ENHANCED_THERMAL_LEVELS];

struct ath12k_thermal {
	struct thermal_cooling_device *cdev;
	struct completion wmi_sync;
	struct device *hwmon_dev;

	/* protected by conf_mutex */
	u32 throttle_state;
	/* temperature value in Celcius degree
	 * protected by data_lock
	 */
	int temperature;
};

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
/* Enum to indicate DBS states */
enum ath12k_dbs_in_out_state {
	DBS_OUT,
	DBS_IN,
};

struct ath12k_ps_context {
	u8 num_active_pdev;
	u8 num_active_port;
	struct ath12k_hw_group *ag;
	enum ath12k_dbs_in_out_state dbs_state;
	eawtp_get_num_active_ports_cb_t get_actv_eth_ports_cb;
};

enum ath12k_ps_metric_change {
	PS_WLAN_DBS_CHANGE = 0,
	PS_ETH_PORT_CHANGE = 1,
	PS_METRIC_CHANGE_MAX,
};
#endif

void ath12k_ath_update_active_pdev_count(struct ath12k *ar);

#if IS_REACHABLE(CONFIG_THERMAL)
int ath12k_thermal_register(struct ath12k_base *sc);
void ath12k_thermal_unregister(struct ath12k_base *sc);
int ath12k_thermal_set_throttling(struct ath12k *ar, u32 throttle_state);
void ath12k_update_tt_configs(struct ath12k *ar, int level, int tmplwm,
			      int tmphwm, int dcoffpercent, int pout_reduction_db,
			      int tx_chain_mask, int duty_cycle);
void ath12k_thermal_event_temperature(struct ath12k *ar, int temperature);
void ath12k_thermal_event_throt_level(struct ath12k *ar, int curr_level);
int ath12k_wmi_thermal_set_throttle(struct ath12k *ar);
#else
static inline int ath12k_thermal_register(struct ath12k_base *sc)
{
	return 0;
}

static inline void ath12k_thermal_unregister(struct ath12k_base *sc)
{
}

static inline int ath12k_thermal_set_throttling(struct ath12k *ar, u32 throttle_state)
{
	return 0;
}

static inline void ath12k_thermal_event_temperature(struct ath12k *ar,
						    int temperature)
{
}

#endif
#endif /* _ATH12K_THERMAL_ */
