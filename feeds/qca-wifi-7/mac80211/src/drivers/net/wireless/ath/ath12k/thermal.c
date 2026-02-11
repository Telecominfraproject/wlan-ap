// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "core.h"
#include "debug.h"
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
#include <soc/qcom/eawtp.h>
#endif

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
struct ath12k_ps_context ath12k_global_ps_ctx;

uint8_t ath12k_get_number_of_active_eth_ports(void)
{
	struct eawtp_port_info pact_info = {0};

	if (ath12k_global_ps_ctx.get_actv_eth_ports_cb)
		ath12k_global_ps_ctx.get_actv_eth_ports_cb(0, &pact_info);

	return pact_info.num_active_port;
}

static void
ath12k_pdev_notify_power_save_metric(u8 count, u8 idx_map,
				     enum ath12k_ps_metric_change metric)
{
	u8 idx, idx_i;
	u8 update_ps_change = 0;
	u8 power_reduction_dbm = 0;
	struct ath12k *tmp_ar;
	struct ath12k_base *tmp_ab;
	struct ath12k_hw_group *ag = ath12k_global_ps_ctx.ag;
	struct ath12k_pdev *pdev;

	if (ag->eth_power_reduction == ATH12K_DEFAULT_POWER_REDUCTION ||
	    ag->dbs_power_reduction == ATH12K_DEFAULT_POWER_REDUCTION) {
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "Power and Thermal optimization: dbs_power_reduction and eth_power_reduction values are not set\n");
		return;
	}

	switch (metric) {
	case PS_WLAN_DBS_CHANGE:
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "Power and Thermal optimization: Change in active pdevs active_pdevs:%u\n",
			   count);
		/* Update to FW about Power Reduction value only when there
		 * is a change in DBS status:
		 * 1. If DBS in, Get Ethernet Port count and check
		 *    a. If Ethernet Port count is lower than TH,
		 *       send dbs_pwr_reduction_dbm to FW
		 *    b. If Ethernet Port count is greter than the TH,
		 *       send dbs_pwr_reduction_dbm + eth_pwr_reduction_dbm
		 *       to FW
		 * 2. If DBS out, update power reduction dbm to 0
		 */
		if (ath12k_global_ps_ctx.num_active_pdev != count) {
			if (count >= ACTIVE_PDEV_TH) {
				/* Set ath12k_global_ps_ctx.num_active_port */
				power_reduction_dbm = ag->dbs_power_reduction;
				if (ath12k_global_ps_ctx.num_active_port > ETH_PORT_COUNT)
					power_reduction_dbm += ag->eth_power_reduction;
				update_ps_change |= (1 << metric);
				ath12k_global_ps_ctx.dbs_state = DBS_IN;
			} else if ((count < ACTIVE_PDEV_TH) &&
				   (ath12k_global_ps_ctx.num_active_pdev >= ACTIVE_PDEV_TH)) {
				update_ps_change |= (1 << metric);
				ath12k_global_ps_ctx.dbs_state = DBS_OUT;
			}
			ath12k_global_ps_ctx.num_active_pdev = count;
		}
		break;
	case PS_ETH_PORT_CHANGE:
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "Power and Thermal optimization: Change in active ethernet ports active_eth_ports:%u\n",
			   count);
		/* Update Power Reduction value to FW only for DBS IN state
		 * when there is a change in active ethernet port count:
		 * 1. If DBS IN state,
		 *     a. If Ethernet Port count is crossing the TH,
		 *        send dbs_pwr_reduction_dbm + eth_pwr_reduction_dbm
		 *        from ini to FW
		 *     b. If Ethernet Port count is coming below the TH,
		 *        send dbs_pwr_reduction_dbm to FW
		 * 2. If DBS OUT state, No Update
		 */
		if (ath12k_global_ps_ctx.num_active_port != count &&
		    ath12k_global_ps_ctx.dbs_state == DBS_IN) {
			if (ath12k_global_ps_ctx.num_active_port <= ETH_PORT_COUNT &&
			    count > ETH_PORT_COUNT) {
				power_reduction_dbm = ag->eth_power_reduction +
						      ag->dbs_power_reduction;
				update_ps_change |= (1 << metric);
			} else if (ath12k_global_ps_ctx.num_active_port > ETH_PORT_COUNT &&
				   count <= ETH_PORT_COUNT) {
				power_reduction_dbm = ag->dbs_power_reduction;
				update_ps_change |= (1 << metric);
			}
		}
		ath12k_global_ps_ctx.num_active_port = count;
		break;
	default:
		break;
	}

	if (update_ps_change) {
		for (idx = 0; idx < ag->num_hw; idx++) {
			tmp_ab = ag->ab[idx];
			if (!tmp_ab)
				continue;
			for (idx_i = 0; idx_i < tmp_ab->num_radios; idx_i++) {
				if (ath12k_global_ps_ctx.dbs_state == DBS_IN &&
				    !(idx_map & (1 << idx)))
					continue;
				rcu_read_lock();
				pdev = rcu_dereference(tmp_ab->pdevs_active[idx_i]);
				if (!pdev) {
					rcu_read_unlock();
					continue;
				}
				tmp_ar = pdev->ar;
				if (tmp_ar && tmp_ar->num_stations) {
					ath12k_dbg(tmp_ab, ATH12K_DBG_WMI,
						   "Power and Thermal optimization sending WMI command to reduces power power_reduction_dbm:%u\n",
						   power_reduction_dbm);
					ath12k_wmi_pdev_set_param(tmp_ar,
								  WMI_PDEV_PARAM_PWR_REDUCTION_IN_QUARTER_DB,
								  power_reduction_dbm,
								  tmp_ar->pdev->pdev_id);
				}
				rcu_read_unlock();
			}
		}
	}
}

static int
netstandby_eawtp_wifi_notify_active_eth_ports(void *app_data,
					      struct eawtp_port_info *ntfy_info)
{
	struct ath12k *tmp_ar;
	u8 idx, idx_i, active_eth_ports = 0, idx_map = 0;
	struct ath12k_base *ab_tmp;
	struct ath12k_pdev *pdev;
	struct ath12k_hw_group *ag = ath12k_global_ps_ctx.ag;

	if (!ntfy_info) {
		ath12k_info(NULL, "WIFI-Netstandby: Invalid Port Info!");
		return -EINVAL;
	}

	active_eth_ports = ntfy_info->num_active_port;

	for (idx = 0; idx < ag->num_hw; idx++) {
		ab_tmp = ag->ab[idx];

		if (!ab_tmp)
			continue;

		for (idx_i = 0; idx_i < ab_tmp->num_radios; idx_i++) {
			rcu_read_lock();
			pdev = rcu_dereference(ab_tmp->pdevs_active[idx_i]);
			if (pdev && pdev->ar) {
				tmp_ar = ab_tmp->pdevs_active[idx_i]->ar;
				if (tmp_ar && tmp_ar->num_stations)
					idx_map |= (1 << idx);
			}
			rcu_read_unlock();
		}
	}

	ath12k_pdev_notify_power_save_metric(active_eth_ports, idx_map, PS_ETH_PORT_CHANGE);

	return 0;
}

int eawtp_wifi_get_and_register_cb(struct eawtp_reg_info *info)
{
	if (!info)
		return -1;

	ath12k_global_ps_ctx.get_actv_eth_ports_cb = info->get_active_ports_cb;
	ath12k_get_number_of_active_eth_ports();
	info->ntfy_port_status_to_wifi_cb = netstandby_eawtp_wifi_notify_active_eth_ports;

	ath12k_info(NULL, "WIFI-Netstandby_eawtp: WIFI registration complete");

	return 0;
}
EXPORT_SYMBOL(eawtp_wifi_get_and_register_cb);

int eawtp_wifi_unregister_cb(void)
{
	ath12k_global_ps_ctx.get_actv_eth_ports_cb = NULL;

	ath12k_info(NULL, "WIFI-Netstandby_eawtp: WIFI unregistered");

	return 0;
}
EXPORT_SYMBOL(eawtp_wifi_unregister_cb);

void ath12k_ath_update_active_pdev_count(struct ath12k *ar)
{
	struct ath12k_hw_group *ag;
	u8 idx, idx_i, active_pdev = 0, idx_map = 0;
	struct ath12k_base *ab, *ab_tmp;
	struct ath12k_pdev *pdev;

	ab = ar->ab;

	if (!ab)
		ath12k_dbg(NULL, ATH12K_DBG_WMI, "Power and thermal optimization: ab is NULL");

	ag = ab->ag;

	if (!ag)
		return;

	for (idx = 0; idx < ag->num_hw; idx++) {
		ab_tmp = ag->ab[idx];
		if (!ab_tmp)
			continue;
		for (idx_i = 0; idx_i < ab_tmp->num_radios; idx_i++) {
			rcu_read_lock();
			pdev = rcu_dereference(ab_tmp->pdevs_active[idx_i]);
			if (pdev && pdev->ar && pdev->ar->num_stations) {
				active_pdev++;
				idx_map |= (1 << idx);
			}
			rcu_read_unlock();
		}
	}

	ath12k_pdev_notify_power_save_metric(active_pdev, idx_map, PS_WLAN_DBS_CHANGE);
}
#endif

struct tt_level_config tt_level_configs[ATH12K_THERMAL_LEVELS][ENHANCED_THERMAL_LEVELS] = {
	{
		{ /* Level 0 */
			ATH12K_THERMAL_IPA_LVL0_TEMP_LOW_MARK,
			ATH12K_THERMAL_IPA_LVL0_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL0_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 1 */
			ATH12K_THERMAL_IPA_LVL1_TEMP_LOW_MARK,
			ATH12K_THERMAL_IPA_LVL1_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL1_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT1
		},
		{ /* Level 2 */
			ATH12K_THERMAL_IPA_LVL2_TEMP_LOW_MARK,
			ATH12K_THERMAL_IPA_LVL2_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL2_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT2,
		},
		{ /* Level 3 */
			ATH12K_THERMAL_IPA_LVL3_TEMP_LOW_MARK,
			ATH12K_THERMAL_IPA_LVL3_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL3_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT3,
		},
		{ /* Level 4 */
			ATH12K_THERMAL_IPA_LVL4_TEMP_LOW_MARK,
			ATH12K_THERMAL_IPA_LVL4_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL4_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT4
		}
	},
	{
		{ /* Level 0 */
			ATH12K_THERMAL_XFRM_LVL0_TEMP_LOW_MARK,
			ATH12K_THERMAL_XFRM_LVL0_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL0_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 1 */
			ATH12K_THERMAL_XFRM_LVL1_TEMP_LOW_MARK,
			ATH12K_THERMAL_XFRM_LVL1_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL1_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 2 */
			ATH12K_THERMAL_XFRM_LVL2_TEMP_LOW_MARK,
			ATH12K_THERMAL_XFRM_LVL2_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL2_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 3 */
			ATH12K_THERMAL_XFRM_LVL3_TEMP_LOW_MARK,
			ATH12K_THERMAL_XFRM_LVL3_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL3_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 4 */
			ATH12K_THERMAL_XFRM_LVL4_TEMP_LOW_MARK,
			ATH12K_THERMAL_XFRM_LVL4_TEMP_HIGH_MARK,
			ATH12K_THERMAL_LVL4_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		}
	},
	{
		{ /* Level 0 */
			ATH12K_THERMAL_XFRM_LVL0_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_XFRM_LVL0_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL0_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 1 */
			ATH12K_THERMAL_XFRM_LVL1_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_XFRM_LVL1_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL1_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 2 */
			ATH12K_THERMAL_XFRM_LVL2_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_XFRM_LVL2_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL2_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 3 */
			ATH12K_THERMAL_XFRM_LVL3_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_XFRM_LVL3_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL3_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 4 */
			ATH12K_THERMAL_XFRM_LVL4_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_XFRM_LVL4_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL4_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		}
	},
	{
		{ /* Level 0 */
			ATH12K_THERMAL_IPA_LVL0_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_IPA_LVL0_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL0_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 1 */
			ATH12K_THERMAL_IPA_LVL1_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_IPA_LVL1_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL1_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 2 */
			ATH12K_THERMAL_IPA_LVL2_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_IPA_LVL2_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL2_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 3 */
			ATH12K_THERMAL_IPA_LVL3_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_IPA_LVL3_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL3_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		},
		{ /* Level 4 */
			ATH12K_THERMAL_IPA_LVL4_TEMP_LOW_MARK_IPQ5424,
			ATH12K_THERMAL_IPA_LVL4_TEMP_HIGH_MARK_IPQ5424,
			ATH12K_THERMAL_LVL4_DUTY_CYCLE, 0,
			THERMAL_CONFIG_POUT0
		}
	}
};

void ath12k_update_tt_configs(struct ath12k *ar, int level, int tmplwm, int tmphwm,
			      int dcoffpercent, int pout_reduction_db,
			      int tx_chain_mask, int duty_cycle)
{
	ar->tt_level_configs[level].tx_chain_mask = tx_chain_mask;
	ar->tt_level_configs[level].tmplwm = tmplwm;
	ar->tt_level_configs[level].tmphwm = tmphwm;
	ar->tt_level_configs[level].dcoffpercent = dcoffpercent;
	ar->tt_level_configs[level].pout_reduction_db = pout_reduction_db;
	ar->tt_level_configs[level].duty_cycle = duty_cycle;

	ath12k_thermal_set_throttling(ar, ATH12K_THERMAL_LVL0_DUTY_CYCLE);
}

static int
ath12k_thermal_get_max_throttle_state(struct thermal_cooling_device *cdev,
				      unsigned long *state)
{
	*state = ATH12K_THERMAL_THROTTLE_MAX;

	return 0;
}

static int
ath12k_thermal_get_cur_throttle_state(struct thermal_cooling_device *cdev,
				      unsigned long *state)
{
	struct ath12k *ar = cdev->devdata;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	*state = ar->thermal.throttle_state;

	return 0;
}

static int
ath12k_thermal_set_cur_throttle_state(struct thermal_cooling_device *cdev,
				      unsigned long throttle_state)
{
	struct ath12k *ar = cdev->devdata;
	int ret;

	if (throttle_state > ATH12K_THERMAL_THROTTLE_MAX) {
		ath12k_warn(ar->ab, "throttle state %ld is exceeding the limit %d\n",
			    throttle_state, ATH12K_THERMAL_THROTTLE_MAX);
		return -EINVAL;
	}
	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);
	ret = ath12k_thermal_set_throttling(ar, throttle_state);
	return ret;
}

static const struct thermal_cooling_device_ops ath12k_thermal_ops = {
	.get_max_state = ath12k_thermal_get_max_throttle_state,
	.get_cur_state = ath12k_thermal_get_cur_throttle_state,
	.set_cur_state = ath12k_thermal_set_cur_throttle_state,
};

static ssize_t ath12k_thermal_show_temp(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ath12k *ar = dev_get_drvdata(dev);
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	int ret, temperature;
	unsigned long time_left;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	/* Can't get temperature when the card is off */
	if (ar->ab->fw_mode != ATH12K_FIRMWARE_MODE_FTM && ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	reinit_completion(&ar->thermal.wmi_sync);
	ret = ath12k_wmi_send_pdev_temperature_cmd(ar);
	if (ret) {
		ath12k_warn(ar->ab, "failed to read temperature %d\n", ret);
		goto out;
	}

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)) {
		ret = -ESHUTDOWN;
		goto out;
	}

	time_left = wait_for_completion_timeout(&ar->thermal.wmi_sync,
						ATH12K_THERMAL_SYNC_TIMEOUT_HZ);
	if (!time_left) {
		ath12k_warn(ar->ab, "failed to synchronize thermal read\n");
		ret = -ETIMEDOUT;
		goto out;
	}

	spin_lock_bh(&ar->data_lock);
	temperature = ar->thermal.temperature;
	spin_unlock_bh(&ar->data_lock);

	/* display in millidegree celcius */
	ret = snprintf(buf, PAGE_SIZE, "%d\n", temperature * 1000);
out:
	return ret;
}

void ath12k_thermal_event_temperature(struct ath12k *ar, int temperature)
{
	spin_lock_bh(&ar->data_lock);
	ar->thermal.temperature = temperature;
	spin_unlock_bh(&ar->data_lock);
	complete(&ar->thermal.wmi_sync);
}

void ath12k_thermal_event_throt_level(struct ath12k *ar, int curr_level)
{
	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map) &&
	    curr_level >= ENHANCED_THERMAL_LEVELS)
		return;
	else if (curr_level >= THERMAL_LEVELS)
		return;

	spin_lock_bh(&ar->data_lock);
	if (test_bit(WMI_SERVICE_IS_TARGET_IPA, ar->ab->wmi_ab.svc_map)) {
		ar->thermal.throttle_state =
			tt_level_configs[ATH12K_IPA_THERMAL_LEVEL][curr_level].dcoffpercent;
	} else {
		if (ar->ab->hw_params->hw_rev == ATH12K_HW_IPQ5424_HW10)
			ar->thermal.throttle_state =
				tt_level_configs[ATH12K_XFRM_IPQ5424_THERMAL_LEVEL][curr_level].dcoffpercent;
		else
			ar->thermal.throttle_state =
				tt_level_configs[ATH12K_XFRM_THERMAL_LEVEL][curr_level].dcoffpercent;
	}
	spin_unlock_bh(&ar->data_lock);
}

static SENSOR_DEVICE_ATTR(temp1_input, 0444, ath12k_thermal_show_temp,
			  NULL, 0);

static struct attribute *ath12k_hwmon_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ath12k_hwmon);

int ath12k_thermal_set_throttling(struct ath12k *ar, u32 throttle_state)
{
	struct ath12k_base *sc = ar->ab;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ath12k_wmi_thermal_mitigation_arg param;
	int level, ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ah->state != ATH12K_HW_STATE_ON)
		return 0;

	memset(&param, 0, sizeof(param));
	param.pdev_id = ar->pdev->pdev_id;
	param.enable = ATH12K_FW_THERMAL_THROTTLING_ENABLE;
	param.dc = ATH12K_THERMAL_DEFAULT_DUTY_CYCLE;
	/* After how many duty cycles the FW sends stats to host */
	param.dc_per_event = 0x2;

	if (test_bit(WMI_SERVICE_IS_TARGET_IPA, ar->ab->wmi_ab.svc_map)) {
		tt_level_configs[ATH12K_IPA_THERMAL_LEVEL][0].dcoffpercent =
			throttle_state;
	} else {
		if (ar->ab->hw_params->hw_rev == ATH12K_HW_IPQ5424_HW10)
			tt_level_configs[ATH12K_XFRM_IPQ5424_THERMAL_LEVEL][0].dcoffpercent =
				throttle_state;
		else
			tt_level_configs[ATH12K_XFRM_THERMAL_LEVEL][0].dcoffpercent =
				throttle_state;
	}

	for (level = 0; level < ENHANCED_THERMAL_LEVELS; level++) {
		param.levelconf[level].tmplwm =
			ar->tt_level_configs[level].tmplwm;
		param.levelconf[level].tmphwm =
			ar->tt_level_configs[level].tmphwm;
		param.levelconf[level].dcoffpercent =
			ar->tt_level_configs[level].dcoffpercent;
		/* disable all data tx queues */
		param.levelconf[level].priority = 0;
		param.levelconf[level].duty_cycle = ar->tt_level_configs[level].duty_cycle;

		if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
			     ar->ab->wmi_ab.svc_map))
			param.levelconf[level].pout_reduction_db =
				ar->tt_level_configs[level].pout_reduction_db;

		if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK,
			     ar->ab->wmi_ab.svc_map)) {
			param.levelconf[level].tx_chain_mask =
				ar->tt_level_configs[level].tx_chain_mask;
		}
	}

	ret = ath12k_wmi_send_thermal_mitigation_cmd(ar, &param);
	if (ret) {
		ath12k_warn(sc, "failed to send thermal mitigation duty cycle %u ret %d\n",
			    throttle_state, ret);
	}
	return ret;
}

int ath12k_thermal_register(struct ath12k_base *sc)
{
	struct thermal_cooling_device *cdev;
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	struct ieee80211_hw *hw;
	char pdev_name[20];
	int i, ret;

	for (i = 0; i < sc->num_radios; i++) {
		pdev = &sc->pdevs[i];
		ar = pdev->ar;
		if (!ar)
			continue;
		hw = ar->ah->hw;
		memset(pdev_name, 0, sizeof(pdev_name));

		cdev = thermal_cooling_device_register("ath12k_thermal", ar,
						       &ath12k_thermal_ops);

		if (IS_ERR(cdev)) {
			ath12k_err(sc, "failed to setup thermal device result: %ld\n",
				   PTR_ERR(cdev));
			ret = -EINVAL;
			goto err_thermal_destroy;
		}

		ar->thermal.cdev = cdev;
		snprintf(pdev_name, sizeof(pdev_name), "%s%d", "cooling_device",
			 ar->hw_link_id);

		ret = sysfs_create_link(&hw->wiphy->dev.kobj, &cdev->device.kobj,
					pdev_name);
		if (ret) {
			ath12k_err(sc, "failed to create cooling device symlink\n");
			goto err_thermal_destroy;
		}

		if (!IS_REACHABLE(CONFIG_HWMON))
			return 0;

		ar->thermal.hwmon_dev = hwmon_device_register_with_groups(&hw->wiphy->dev,
									  "ath12k_hwmon", ar,
									  ath12k_hwmon_groups);
		if (IS_ERR(ar->thermal.hwmon_dev)) {
			ath12k_err(ar->ab, "failed to register hwmon device: %ld\n",
				   PTR_ERR(ar->thermal.hwmon_dev));
			ar->thermal.hwmon_dev = NULL;
			ret = -EINVAL;
			goto err_thermal_destroy;
		}
	}

	return 0;

err_thermal_destroy:
	ath12k_thermal_unregister(sc);
	return ret;
}

void ath12k_thermal_unregister(struct ath12k_base *sc)
{
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	struct ieee80211_hw *hw;
	char pdev_name[20];
	int i;

	for (i = 0; i < sc->num_radios; i++) {
		pdev = &sc->pdevs[i];
		ar = pdev->ar;
		if (!ar)
			continue;

		hw = ar->ah->hw;
		memset(pdev_name, 0, sizeof(pdev_name));

		snprintf(pdev_name, sizeof(pdev_name), "%s%d", "cooling_device",
			 ar->hw_link_id);
		if (ar->thermal.hwmon_dev)
			ar->thermal.hwmon_dev = NULL;
		sysfs_remove_link(&hw->wiphy->dev.kobj, pdev_name);
		thermal_cooling_device_unregister(ar->thermal.cdev);
	}
}
