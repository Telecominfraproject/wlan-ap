/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2015, 2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of  The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.

 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "debug.h"
#include "wmi.h"
#include "smart_ant.h"

static int smart_ant_alg_get_streams(u32 nss)
{
	u32 num_chains = 0, supp_tx_chainmask = (1 << nss) - 1;
	int i;

	for (i = 0; i < ATH11K_SMART_ANT_MAX_CHAINS; i++) {
		if (supp_tx_chainmask & (1 << i))
			num_chains++;
	}

	return min(num_chains, nss);
}

static void smart_ant_alg_init_param(struct ath11k *ar)
{
	struct ath11k_smart_ant_info *info = &ar->smart_ant_info;
	struct ath11k_smart_ant_params *sa_params =
					&ar->smart_ant_info.smart_ant_params;
	u32 nss;

	nss = get_num_chains(ar->cfg_tx_chainmask) ? : 1;
	info->mode = WMI_SMART_ANT_MODE_PARALLEL;
	info->default_ant = ATH11K_SMART_ANT_DEFAULT_ANT;
	info->num_fallback_rate = ATH11K_SMART_ANT_FALLBACK_RATE_DEFAULT;
	info->txrx_feedback = ATH11K_SMART_ANT_TX_FEEDBACK |
			      ATH11K_SMART_ANT_RX_FEEDBACK;

	sa_params->low_rate_threshold  	= ATH11K_SMART_ANT_PER_MIN_THRESHOLD;
	sa_params->hi_rate_threshold 	= ATH11K_SMART_ANT_PER_MAX_THRESHOLD;
	sa_params->per_diff_threshold	= ATH11K_SMART_ANT_PER_DIFF_THRESHOLD;
	sa_params->num_train_pkts	= 0;
	sa_params->pkt_len		= ATH11K_SMART_ANT_PKT_LEN_DEFAULT;
	sa_params->num_tx_ant_comb	= 1 << smart_ant_alg_get_streams(nss);
	sa_params->num_min_pkt		= ATH11K_SMART_ANT_NUM_PKT_MIN;
	sa_params->retrain_interval	= msecs_to_jiffies(
						ATH11K_SMART_ANT_RETRAIN_INTVL);
	sa_params->perf_train_interval	= msecs_to_jiffies(
						ATH11K_SMART_ANT_PERF_TRAIN_INTVL);
	sa_params->max_perf_delta	= ATH11K_SMART_ANT_TPUT_DELTA_DEFAULT;
	sa_params->hysteresis		= ATH11K_SMART_ANT_HYSTERISYS_DEFAULT;
	sa_params->min_goodput_threshold =
				ATH11K_SMART_ANT_MIN_GOODPUT_THRESHOLD;
	sa_params->avg_goodput_interval	= ATH11K_SMART_ANT_GOODPUT_INTVL_AVG;
	sa_params->ignore_goodput_interval =
				ATH11K_SMART_ANT_IGNORE_GOODPUT_INTVL;
	sa_params->num_pretrain_pkts = ATH11K_SMART_ANT_PRETRAIN_PKTS_MAX;
	sa_params->num_other_bw_pkts_threshold = ATH11K_SMART_ANT_BW_THRESHOLD;
	sa_params->enabled_train = ATH11K_SMART_ANT_TRAIN_INIT |
				   ATH11K_SMART_ANT_TRAIN_TRIGGER_PERIODIC |
				   ATH11K_SMART_ANT_TRAIN_TRIGGER_PERF |
				   ATH11K_SMART_ANT_TRAIN_TRIGGER_RX;
	sa_params->num_pkt_min_threshod[ATH11K_SMART_ANT_BW_20] =
				ATH11K_SMART_ANT_NUM_PKT_THRESHOLD_20;
	sa_params->num_pkt_min_threshod[ATH11K_SMART_ANT_BW_40] =
				ATH11K_SMART_ANT_NUM_PKT_THRESHOLD_40;
	sa_params->num_pkt_min_threshod[ATH11K_SMART_ANT_BW_80] =
				ATH11K_SMART_ANT_NUM_PKT_THRESHOLD_80;
	sa_params->default_tx_ant	= ATH11K_SMART_ANT_DEFAULT_ANT;
	sa_params->ant_change_ind	= 0;
	sa_params->max_train_ppdu	= ATH11K_SMART_ANT_NUM_TRAIN_PPDU_MAX;
	sa_params->num_rx_chain		= nss;
	if (nss > ATH11K_SMART_ANT_MAX_RX_CHAIN)
		sa_params->num_rx_chain = ATH11K_SMART_ANT_MAX_RX_CHAIN;
}

void ath11k_smart_ant_alg_sta_disconnect(struct ath11k *ar,
					 struct ieee80211_sta *sta)
{
	struct ath11k_sta *arsta = (struct ath11k_sta *)sta->drv_priv;

	if (!ath11k_smart_ant_enabled(ar) || !arsta->smart_ant_sta)
		return;

	ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT,
		   "Smart antenna disconnect for %pM\n", sta->addr);

	kfree(arsta->smart_ant_sta);
}

int ath11k_smart_ant_alg_sta_connect(struct ath11k *ar,
				     struct ath11k_vif *arvif,
				     struct ieee80211_sta *sta)
{
	struct ath11k_sta *arsta = (struct ath11k_sta *)sta->drv_priv;
	struct ath11k_smart_ant_node_config_params params;
	struct ath11k_smart_ant_sta *smart_ant_sta;
	int ret;
	u8 mac_addr[ETH_ALEN];
	u32 arg = ATH11K_SMART_ANT_TX_FEEDBACK_CONFIG_DEFAULT;

	lockdep_assert_held(&ar->conf_mutex);

	if (arvif->vdev_type != WMI_VDEV_TYPE_AP ||
	    arvif->vdev_subtype != WMI_VDEV_SUBTYPE_NONE)
		return 0;

	memset(&params, 0, sizeof(params));

	params.cmd_id = 1;
	params.arg_count = 1;
	params.vdev_id = arsta->arvif->vdev_id;
	params.arg_arr = &arg;
	ether_addr_copy(mac_addr, sta->addr);

	ret = ath11k_wmi_peer_set_smart_ant_node_config(ar, mac_addr, &params);
	if (ret) {
		ath11k_warn(ar->ab, "Failed to set feedback config\n");
		return ret;
	}

	smart_ant_sta = kzalloc(sizeof(struct ath11k_smart_ant_sta), GFP_ATOMIC);
	if (!smart_ant_sta) {
		ath11k_warn(ar->ab, "Failed to allocate smart ant sta\n");
		ret = -EINVAL;
		return ret;
	}

	arsta->smart_ant_sta = smart_ant_sta;

	return 0;
}

int ath11k_smart_ant_alg_set_default(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_smart_ant_info *info = &ar->smart_ant_info;
	int ret, i;
	u32 tx_ants[ATH11K_SMART_ANT_MAX_RATE_SERIES];

	lockdep_assert_held(&ar->conf_mutex);

	if (!ath11k_smart_ant_enabled(ar))
		return 0;

	if (!info->enabled)
		return 0;

	if (arvif->vdev_type != WMI_VDEV_TYPE_AP ||
	    arvif->vdev_subtype != WMI_VDEV_SUBTYPE_NONE) {
		ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT,
			   "Smart antenna logic not enabled for non-AP interface\n");
		return 0;
	}

	/* Set default tx/rx antennas to start with */
	ret = ath11k_wmi_pdev_set_rx_ant(ar, info->default_ant);
	if (ret) {
		ath11k_warn(ar->ab, "Failed to set rx antenna\n");
		return ret;
	}

	/* Tx antenna for every fallback rate series */
	for (i = 0; i < info->num_fallback_rate; i++)
		tx_ants[i] = info->default_ant;

	ret = ath11k_wmi_peer_set_smart_tx_ant(ar, arvif->vdev_id,
					       arvif->vif->addr, tx_ants);
	if (ret)
		ath11k_warn(ar->ab, "Failed to set tx antenna\n");

	return ret;
}

void ath11k_smart_ant_alg_disable(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_smart_ant_info *info = &ar->smart_ant_info;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	if (!ath11k_smart_ant_enabled(ar))
		return;

	if (!info->enabled)
		return;

	if (arvif->vdev_type != WMI_VDEV_TYPE_AP ||
	    arvif->vdev_subtype != WMI_VDEV_SUBTYPE_NONE)
		return;

	/* See if this is the last vif requesting to disable smart antenna */
	info->num_enabled_vif--;
	if (info->num_enabled_vif != 0)
		return;

	/* Disable smart antenna logic in fw */
	ret = ath11k_wmi_pdev_disable_smart_ant(ar, info);
	if (ret) {
		ath11k_err(ar->ab, "Wmi command to disable smart antenna is failed\n");
		return;
	}

	info->enabled = false;
}

int ath11k_smart_ant_alg_enable(struct ath11k_vif *arvif)
{
	struct ath11k *ar = arvif->ar;
	struct ath11k_smart_ant_info *info = &ar->smart_ant_info;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	if (!ath11k_smart_ant_enabled(ar))
		return 0;

	/* Smart antenna is tested with only AP mode, it can also be enabled
	 * for other modes, just needs more testing.
	 */
	if (arvif->vdev_type != WMI_VDEV_TYPE_AP ||
	    arvif->vdev_subtype != WMI_VDEV_SUBTYPE_NONE) {
		ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT,
			   "Smart antenna logic not enabled for non-AP interface\n");
		return 0;
	}

	info->num_enabled_vif++;
	if (info->enabled)
		return 0;

	smart_ant_alg_init_param(ar);

	ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT,
		   "Hw supports Smart antenna, enabling it in driver\n");

	info->enabled = true;

	ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT,
		   "smart mode: %d num_fallback_rate: %d num_rx_chain: %d info_enable: %d\n",
		   ar->smart_ant_info.mode, ar->smart_ant_info.num_fallback_rate,
		   ar->smart_ant_info.smart_ant_params.num_rx_chain, info->enabled);

	/* Enable smart antenna logic in fw with mode and default antenna */
	ret = ath11k_wmi_pdev_enable_smart_ant(ar, info);
	if (ret) {
		ath11k_err(ar->ab, "Wmi command to enable smart antenna is failed\n");
		return ret;
	}

	return 0;
}

static void ath11k_smart_ant_dbg_feedback(struct ath11k_base *ab,
					  struct ath11k_smart_ant_tx_feedback *fb)
{
	ath11k_dbg(ab, ATH11K_DBG_SMART_ANT,
		   "Tx feedback npkts: %d nbad: %d tx_antenna[0]: %d tx_antenna[1]: %d "
		   "train pkt: %s goodput: %d rate_maxphy: [0x%x|0x%x|0x%x|0x%x] "
		   "RSSI: [%d %d %d]\n",
		   fb->npackets, fb->nbad, fb->tx_antenna[0], fb->tx_antenna[0],
		   fb->is_trainpkt ? "True" : "False", fb->goodput, fb->ratemaxphy[0],
		   fb->ratemaxphy[1], fb->ratemaxphy[2], fb->ratemaxphy[3], fb->rssi[0],
		   fb->rssi[1], fb->rssi[2]);
}

static void ath11k_smart_ant_tx_fb_fill(struct ath11k_base *ab, u32 *data,
					struct ath11k_smart_ant_tx_feedback *fb)
{
	int i;

	fb->npackets = TXFD_MS(data, ATH11K_SMART_ANT_NPKTS);
	fb->nbad = fb->npackets - TXFD_MS(data, ATH11K_SMART_ANT_NSUCC);

	for (i = 0; i < ATH11K_SMART_ANT_MAX_SA_RSSI_CHAINS; i++)
		fb->rssi[i] = data[ATH11K_SMART_ANT_RSSI + i];

	fb->tx_antenna[0] = TXFD_MS(data, ATH11K_SMART_ANT_TX_ANT);
	fb->is_trainpkt = TXFD_MS(data, ATH11K_SMART_ANT_IS_TRAIN_PKT);
	if (fb->is_trainpkt)
		fb->goodput = TXFD_MS(data, ATH11K_SMART_ANT_TRAIN_PKTS);

	for (i = 0; i < ATH11K_SMART_ANT_MAX_RATE_COUNTERS; i++)
		fb->ratemaxphy[i] = TXFD_MS(data, ATH11K_SMART_ANT_MAX_RATE);

	ath11k_smart_ant_dbg_feedback(ab, fb);
}

void ath11k_smart_ant_alg_proc_tx_feedback(struct ath11k_base *ab, u32 *data,
					   u16 peer_id)
{
	struct ath11k_sta *arsta;
	struct ath11k_peer *peer = NULL;
	struct ath11k_smart_ant_tx_feedback tx_feedback;

	rcu_read_lock();
	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find_by_id(ab, peer_id);
	if(peer && peer->sta) {
		arsta = (struct ath11k_sta *)peer->sta->drv_priv;

		if (!arsta->smart_ant_sta)
			goto exit;

		memset(&tx_feedback, 0, sizeof(tx_feedback));
		ath11k_dbg(ab, ATH11K_DBG_SMART_ANT, "Tx feedback from sta: %pM\n",
			   peer->addr);
		ath11k_smart_ant_tx_fb_fill(ab, data, &tx_feedback);

	}
exit:
	spin_unlock_bh(&ab->base_lock);
	rcu_read_unlock();
}
