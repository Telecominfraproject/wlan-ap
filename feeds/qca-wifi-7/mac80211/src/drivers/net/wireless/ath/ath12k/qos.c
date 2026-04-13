/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/module.h>
#include "core.h"
#include "wmi.h"
#include "qos.h"
#include "debug.h"

struct ath12k_qos_ctx *ath12k_get_qos(struct ath12k_base *ab)
{
	if (!ab) {
		ath12k_err(ab, "QoS context not initialized");
		return NULL;
	}
	return &ab->ag->qos;
}
EXPORT_SYMBOL(ath12k_get_qos);

static
bool ath12k_qos_validate_params(struct ath12k_base *ab,
				struct ath12k_qos_params *params)
{
	u32 value;
	bool status = false;

	value = params->min_data_rate;
	if (value != QOS_PARAM_DEFAULT_MIN_THROUGHPUT &&
	    (value < QOS_MIN_MIN_THROUGHPUT ||
	     value > QOS_MAX_MIN_THROUGHPUT)) {
		ath12k_err(ab, "Invalid Min throughput: %d", value);
		return status;
	}

	value = params->mean_data_rate;
	if (value != QOS_PARAM_DEFAULT_MAX_THROUGHPUT &&
	    (value < QOS_MIN_MAX_THROUGHPUT ||
	     value > QOS_MAX_MAX_THROUGHPUT)) {
		ath12k_err(ab, "Invalid Max througput: %d", value);
		return status;
	}

	value = params->burst_size;
	if (value != QOS_PARAM_DEFAULT_BURST_SIZE &&
	    (value < QOS_MIN_BURST_SIZE ||
	     value > QOS_MAX_BURST_SIZE)) {
		ath12k_err(ab, "Invalid Burst Size: %d", value);
		return status;
	}

	value = params->delay_bound;
	if (value != QOS_PARAM_DEFAULT_DELAY_BOUND &&
	    (value < QOS_MIN_DELAY_BOUND ||
	     value > QOS_MAX_DELAY_BOUND)) {
		ath12k_err(ab, "Invalid Delay Bound: %d", value);
		return status;
	}

	value = params->min_service_interval;
	if (value != QOS_PARAM_DEFAULT_SVC_INTERVAL &&
	    (value < QOS_MIN_SVC_INTERVAL ||
	     value > QOS_MAX_SVC_INTERVAL)) {
		ath12k_err(ab, "Invalid Service Interval: %d", value);
		return status;
	}

	value = params->msdu_life_time;
	if (value != QOS_PARAM_DEFAULT_TIME_TO_LIVE &&
	    (value < QOS_MIN_MSDU_TTL ||
	     value > QOS_MAX_MSDU_TTL)) {
		ath12k_err(ab, "Invalid MSDU TTL: %d", value);
		return status;
	}

	value = params->priority;
	if (value != QOS_PARAM_DEFAULT_PRIORITY &&
	    (value < QOS_MIN_PRIORITY ||
	     value > QOS_MAX_PRIORITY)) {
		ath12k_err(ab, "Invalid Priority: %d", value);
		return status;
	}

	value = params->tid;
	if (value != QOS_PARAM_DEFAULT_TID &&
	    (value < QOS_MIN_TID ||
	     value > QOS_MAX_TID)) {
		ath12k_err(ab, "Invalid TID: %d", value);
		return status;
	}

	value = params->msdu_delivery_info;
	if (value != QOS_PARAM_DEFAULT_MSDU_LOSS_RATE &&
	    (value < QOS_MIN_MSDU_LOSS_RATE ||
	     value > QOS_MAX_MSDU_LOSS_RATE)) {
		ath12k_err(ab, "Invalid MSDU Loss rate: %d", value);
		return status;
	}

	return true;
}

static
u16 ath12k_add_profile(struct ath12k_base *ab,
		       struct ath12k_qos_ctx *qos_ctx,
		       u16 min_id, u16 max_id,
		       struct ath12k_qos_params *params)
{
	u16 id = QOS_ID_INVALID;
	u16 index;

	for (index = min_id; index <= max_id; index++) {
		if (!qos_ctx->profiles[index].ref_count) {
			memcpy(&qos_ctx->profiles[index].params,
			       params, sizeof(struct ath12k_qos_params));
			qos_ctx->profiles[index].ref_count++;
			id = index;
			ath12k_dbg(ab, ATH12K_DBG_QOS,
				   "QoS profile %d add, ref_count:%d", id,
				   qos_ctx->profiles[id].ref_count);
			break;
		}
	}
	return id;
}

static
int ath12k_update_profile(struct ath12k_base *ab,
			  struct ath12k_qos_ctx *qos_ctx,
			  u16 id, struct ath12k_qos_params *params)
{
	memcpy(&qos_ctx->profiles[id].params,
	       params, sizeof(struct ath12k_qos_params));
	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "QoS profile %d Update, ref_count:%d", id,
		   qos_ctx->profiles[id].ref_count);

	return 0;
}

static
int ath12k_del_profile(struct ath12k_base *ab,
		       struct ath12k_qos_ctx *qos_ctx, u16 id)
{
	if (!qos_ctx->profiles[id].ref_count) {
		ath12k_err(ab, "QoS ID not configured: %d", id);
		return -EINVAL;
	}

	qos_ctx->profiles[id].ref_count--;
	if (!qos_ctx->profiles[id].ref_count) {
		memset(&qos_ctx->profiles[id].params, 0,
		       sizeof(struct ath12k_qos_params));
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "QoS profile %d delete, ref_count:%d", id,
			   qos_ctx->profiles[id].ref_count);
		return 0;
	}

	ath12k_dbg(ab, ATH12K_DBG_QOS, "QoS profile %d delete, ref_count:%d",
		   id, qos_ctx->profiles[id].ref_count);

	return 0;
}

static bool ath12k_qos_id_valid(struct ath12k_base *ab, u16 id,
				u16 min_id, u16 max_id)
{
	return (id >= min_id && id <= max_id);
}

static
u16 ath12_qos_profile_exists(struct ath12k_base *ab,
			     struct ath12k_qos_ctx *qos_ctx,
			     struct ath12k_qos_params *params,
			     u16 min_id, u16 max_id)
{
	u16 index;
	u16 id = QOS_ID_INVALID;

	for (index = min_id; index <= max_id; index++) {
		if (!memcmp(params, &qos_ctx->profiles[index].params,
			    sizeof(struct ath12k_qos_params))) {
			id = index;
			break;
		}
	}
	return id;
}

void ath12k_qos_set_default(struct ath12k_qos_params *param)
{
	param->min_data_rate = QOS_PARAM_DEFAULT_MIN_THROUGHPUT;
	param->mean_data_rate = QOS_PARAM_DEFAULT_MAX_THROUGHPUT;
	param->burst_size = QOS_PARAM_DEFAULT_BURST_SIZE;
	param->min_service_interval = QOS_PARAM_DEFAULT_SVC_INTERVAL;
	param->max_service_interval = QOS_PARAM_DEFAULT_SVC_INTERVAL;
	param->delay_bound = QOS_PARAM_DEFAULT_DELAY_BOUND;
	param->msdu_life_time = QOS_PARAM_DEFAULT_TIME_TO_LIVE;
	param->priority = QOS_PARAM_DEFAULT_PRIORITY;
	param->tid = (u8)QOS_PARAM_DEFAULT_TID;
	param->msdu_delivery_info = QOS_PARAM_DEFAULT_MSDU_LOSS_RATE;
	param->ul_ofdma_disable = QOS_PARAM_DEFAULT_UL_OFDMA_DISABLE;
	param->ul_mu_mimo_disable = QOS_PARAM_DEFAULT_UL_MU_MIMO_DISABLE;
}

u8 ath12k_qos_get_tid(struct ath12k_base *ab, u16 id)
{
	struct ath12k_qos_ctx *qos_ctx;
	u8 tid = QOS_INVALID_TID;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return QOS_INVALID_TID;
	}

	if (id >= QOS_PROFILES_MAX) {
		ath12k_err(ab, "Invalid Profile ID: %d", id);
		return QOS_INVALID_TID;
	}
	spin_lock_bh(&qos_ctx->profile_lock);
	if (qos_ctx->profiles[id].ref_count)
		tid = qos_ctx->profiles[id].params.tid;
	spin_unlock_bh(&qos_ctx->profile_lock);

	return tid;
}

bool ath12k_qos_configured(struct ath12k_base *ab, u16 id)
{
	struct ath12k_qos_ctx *qos_ctx;
	bool status = false;

	if (id >= QOS_PROFILES_MAX)
		return status;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return status;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	status = qos_ctx->profiles[id].ref_count ? true : false;
	spin_unlock_bh(&qos_ctx->profile_lock);

	return status;
}

u16 ath12k_qos_get_legacy_id(struct ath12k_base *ab, u8 tid)
{
	if (tid > QOS_LEGACY_DL_MAX_TID)
		return QOS_ID_INVALID;

	return (QOS_LEGACY_DL_ID_MIN + tid);
}

u16 ath12k_qos_configure(struct ath12k_base *ab, struct ath12k *ar,
			 struct ath12k_qos_params *params,
			 enum qos_profile_dir qos_dir, u8 *mac_addr)
{
	u16 id = QOS_ID_INVALID;
	struct ath12k_qos_ctx *qos_ctx;
	u16 min_id, max_id;
	int ret = 0;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return id;
	}

	if (!ath12k_qos_validate_params(ab, params)) {
		ath12k_err(ab, "Invalid QoS Params");
		return id;
	}

	min_id = (qos_dir == QOS_PROFILE_DL) ? QOS_DL_ID_MIN : QOS_UL_ID_MIN;
	max_id = (qos_dir == QOS_PROFILE_DL) ? QOS_DL_ID_MAX : QOS_UL_ID_MAX;

	spin_lock_bh(&qos_ctx->profile_lock);

	id = ath12_qos_profile_exists(ab, qos_ctx, params,
				      min_id, max_id);
	if (ath12k_qos_id_valid(ab, id, min_id, max_id)) {
		qos_ctx->profiles[id].ref_count++;
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "QoS profile matched - ID:%d, ref_count:%d", id,
			   qos_ctx->profiles[id].ref_count);
		spin_unlock_bh(&qos_ctx->profile_lock);
		return id;
	}

	id = ath12k_add_profile(ab, qos_ctx, min_id, max_id, params);

	spin_unlock_bh(&qos_ctx->profile_lock);

	if (qos_dir == QOS_PROFILE_DL) {
		if (id != QOS_ID_INVALID)
			ret = ath12k_core_add_dl_qos(ab, params, id);
	}
	if (qos_dir == QOS_PROFILE_UL) {
		if (id != QOS_ID_INVALID && ar && mac_addr)
			ret = ath12k_core_config_ul_qos(ar, params, id,
							mac_addr, true);
	}

	if (ret != 0) {
		spin_lock_bh(&qos_ctx->profile_lock);
		ath12k_del_profile(ab, qos_ctx, id);
		spin_unlock_bh(&qos_ctx->profile_lock);
		id = QOS_ID_INVALID;
	}

	return id;
}

int ath12k_qos_disable(struct ath12k_base *ab, struct ath12k *ar,
		       enum qos_profile_dir qos_dir,
		       u16 id, u8 *mac_addr)
{
	struct ath12k_qos_ctx *qos_ctx;
	int ret;
	u16 min_id, max_id;
	struct ath12k_qos_params params;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return -EINVAL;
	}

	min_id = (qos_dir == QOS_PROFILE_DL) ? QOS_DL_ID_MIN : QOS_UL_ID_MIN;
	max_id = (qos_dir == QOS_PROFILE_DL) ? QOS_DL_ID_MAX : QOS_UL_ID_MAX;

	if (!ath12k_qos_id_valid(ab, id, min_id, max_id)) {
		ath12k_err(ab, "Invalid  QoS ID: %d", id);
		return -EINVAL;
	}

	spin_lock_bh(&qos_ctx->profile_lock);
	memcpy(&params, &qos_ctx->profiles[id].params,
	       sizeof(struct ath12k_qos_params));
	ret = ath12k_del_profile(ab, qos_ctx, id);
	spin_unlock_bh(&qos_ctx->profile_lock);

	/*
	 * Return if either delete failed or ref_count is decremented and is
	 * non zero - some reference still exist, profile not deleted.
	 */
	if (ret != 0 || qos_ctx->profiles[id].ref_count != 0)
		return ret;

	if (qos_dir == QOS_PROFILE_DL) {
		ret = ath12k_core_del_dl_qos(ab, id);
	} else if (qos_dir == QOS_PROFILE_UL) {
		if (ar && mac_addr) {
			ret =  ath12k_core_config_ul_qos(ar, &params, id,
							 mac_addr, false);
		}
	}
	return ret;
}

int ath12k_qos_update(struct ath12k_base *ab, struct ath12k *ar,
		      struct ath12k_qos_params *params,
		      enum qos_profile_dir qos_dir,
		      u16 id, u8 *mac_addr)
{
	u8 tid;
	struct ath12k_qos_ctx *qos_ctx;
	int ret;
	u16 min_id, max_id;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return -EINVAL;
	}

	if (!ath12k_qos_validate_params(ab, params)) {
		ath12k_err(ab, "Invalid QoS Params");
		return -EINVAL;
	}

	min_id = (qos_dir == QOS_PROFILE_DL) ? QOS_DL_ID_MIN : QOS_UL_ID_MIN;
	max_id = (qos_dir == QOS_PROFILE_DL) ? QOS_DL_ID_MAX : QOS_UL_ID_MAX;
	if (!ath12k_qos_id_valid(ab, id, min_id, max_id)) {
		ath12k_err(ab, "Invalid  QoS ID: %d", id);
		return -EINVAL;
	}
	spin_lock_bh(&qos_ctx->profile_lock);
	if (qos_ctx->profiles[id].ref_count > 1) {
		/* Allow update of QoS params only its used by one user,
		 * For morethan one user of the same QoS profile,
		 * update would result in incosistencies*/
		ath12k_err(ab, "Update failed %d since ref_count: %d",
			   id, qos_ctx->profiles[id].ref_count);
		ret = -EINVAL;
		goto ret;
	}

	tid = qos_ctx->profiles[id].params.tid;
	if (tid != params->tid) {
		ath12k_err(ab, "TID update not supported");
		ret = -EINVAL;
		goto ret;
	}

	ret = ath12k_update_profile(ab, qos_ctx, id, params);
ret:
	spin_unlock_bh(&qos_ctx->profile_lock);
	if (ret != 0)
		return ret;

	if (qos_dir == QOS_PROFILE_DL) {
		ret = ath12k_core_add_dl_qos(ab, params, id);
	} else if (qos_dir == QOS_PROFILE_UL) {
		if (ar && mac_addr) {
			ret =  ath12k_core_config_ul_qos(ar, params, id,
							 mac_addr, true);
		}
	}
	return ret;
}

int ath12k_reconfig_qos_profiles(struct ath12k_base *ab)
{
	struct ath12k_qos_ctx *qos_ctx;
	struct ath12k_qos_params *params;
	u8 index;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS Context is NULL");
		return -EINVAL;
	}

	for (index = QOS_DL_ID_MIN; index <= QOS_DL_ID_MAX; index++) {
		if (qos_ctx->profiles[index].ref_count == 0)
			continue;

		params = &qos_ctx->profiles[index].params;
		ath12k_core_add_dl_qos(ab, params, index);
	}

	return 0;
}
