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

#ifndef _SMART_ANT_ALG_
#define _SMART_ANT_ALG_

#define ATH11K_SMART_ANT_FALLBACK_RATE_DEFAULT  1
#define ATH11K_SMART_ANT_TX_FEEDBACK            0x10
#define ATH11K_SMART_ANT_RX_FEEDBACK            0x20
#define ATH11K_SMART_ANT_PER_MIN_THRESHOLD      20
#define ATH11K_SMART_ANT_PER_MAX_THRESHOLD      80
#define ATH11K_SMART_ANT_PER_DIFF_THRESHOLD     3
#define ATH11K_SMART_ANT_PKT_LEN_DEFAULT        1536
#define ATH11K_SMART_ANT_MAX_CHAINS             4
#define ATH11K_SMART_ANT_NUM_PKT_MIN            344
#define ATH11K_SMART_ANT_RETRAIN_INTVL          (2 * 60000)     /* msecs */
#define ATH11K_SMART_ANT_PERF_TRAIN_INTVL       2000            /* msecs */
#define ATH11K_SMART_ANT_TPUT_DELTA_DEFAULT     10
#define ATH11K_SMART_ANT_HYSTERISYS_DEFAULT     3
#define ATH11K_SMART_ANT_MIN_GOODPUT_THRESHOLD  6
#define ATH11K_SMART_ANT_GOODPUT_INTVL_AVG      2
#define ATH11K_SMART_ANT_IGNORE_GOODPUT_INTVL   1
#define ATH11K_SMART_ANT_PRETRAIN_PKTS_MAX      600
#define ATH11K_SMART_ANT_BW_THRESHOLD           64
#define ATH11K_SMART_ANT_NUM_PKT_THRESHOLD_20   20
#define ATH11K_SMART_ANT_NUM_PKT_THRESHOLD_40   10
#define ATH11K_SMART_ANT_NUM_PKT_THRESHOLD_80   5
#define ATH11K_SMART_ANT_DEFAULT_ANT            5
#define ATH11K_SMART_ANT_NUM_TRAIN_PPDU_MAX     50
#define ATH11K_SMART_ANT_MAX_RX_CHAIN           4
#define ATH11K_SMART_ANT_MAX_RATE_SERIES        4
#define ATH11K_SMART_ANT_BYTES_IN_DWORD         4
#define ATH11K_SMART_ANT_MASK_BYTES             0xff
#define ATH11K_SMART_ANT_LEGACY_RATE_WORDS      6
#define ATH11K_SMART_ANT_WORDS_IN_DWORD         2
#define ATH11K_SMART_ANT_WORD_BITS_LEN          16
#define ATH11K_SMART_ANT_MASK_RCODE             0x7ff
#define ATH11K_SMART_ANT_MAX_HT_RATE_WORDS      48
#define ATH11K_SMART_ANT_MAX_RATES              44
#define ATH11K_SMART_ANT_RSSI_SAMPLE            10
#define ATH11K_SMART_ANT_MAX_RETRY		8
#define ATH11K_SMART_ANT_MAX_SA_RSSI_CHAINS	8
#define ATH11K_SMART_ANT_MAX_RATE_COUNTERS	4

#define ATH11K_SMART_ANT_MAX_COMB_FB		2


#define ATH11K_SMART_ANT_TX_FEEDBACK_CONFIG_DEFAULT 0xe4

/* Max number of antenna combinations 2 ^ max_supported_ant */
#define ATH11K_SMART_ANT_COMB_MAX               16
#define ATH11K_SMART_ANT_PIN0	14
#define ATH11K_SMART_ANT_PIN1	15
#define ATH11K_SMART_ANT_FUNC0	5
#define ATH11K_SMART_ANT_FUNC1	5

#define ATH11K_SMART_ANT_NPKTS		3
#define ATH11K_SMART_ANT_NPKTS_M	0xffff
#define ATH11K_SMART_ANT_NPKTS_S	0

#define ATH11K_SMART_ANT_NSUCC		3
#define ATH11K_SMART_ANT_NSUCC_M	0xffff0000
#define ATH11K_SMART_ANT_NSUCC_S	16

#define ATH11K_SMART_ANT_RSSI		5

#define ATH11K_SMART_ANT_TX_ANT		5
#define ATH11K_SMART_ANT_TX_ANT_M	0xffffffff
#define ATH11K_SMART_ANT_TX_ANT_S	0

#define ATH11K_SMART_ANT_IS_TRAIN_PKT	6
#define ATH11K_SMART_ANT_IS_TRAIN_PKT_M	0x10000
#define ATH11K_SMART_ANT_IS_TRAIN_PKT_S	16

#define ATH11K_SMART_ANT_TRAIN_PKTS	6
#define ATH11K_SMART_ANT_TRAIN_PKTS_M	0xffff
#define ATH11K_SMART_ANT_TRAIN_PKTS_S	0

#define ATH11K_SMART_ANT_MAX_RATE	7
#define ATH11K_SMART_ANT_MAX_RATE_M	0xffffffff
#define ATH11K_SMART_ANT_MAX_RATE_S	0

#define TXFD_MS(data, info) \
		((data[info] & info## _M) >> info## _S)

enum ath11k_smart_ant_bw {
	ATH11K_SMART_ANT_BW_20,
	ATH11K_SMART_ANT_BW_40,
	ATH11K_SMART_ANT_BW_80,
	ATH11K_SMART_ANT_BW_MAX
};

enum ath11k_smart_ant_train_trigger {
	ATH11K_SMART_ANT_TRAIN_INIT              = 1 << 0,
	ATH11K_SMART_ANT_TRAIN_TRIGGER_PERIODIC  = 1 << 1,
	ATH11K_SMART_ANT_TRAIN_TRIGGER_PERF      = 1 << 2,
	ATH11K_SMART_ANT_TRAIN_TRIGGER_RX        = 1 << 4,
};

enum ath11k_smart_ant_band_id {
	ATH11K_SMART_ANT_BAND_UNSPECIFIED = 0,
	ATH11K_SMART_ANT_BAND_2GHZ	  = 1,
	ATH11K_SMART_ANT_BAND_5GHZ	  = 2,
	ATH11K_SMART_ANT_BAND_6GHZ	  = 3,
	ATH11K_SMART_ANT_BAND_MAX,

};

enum ath11k_wireless_mode {
	ATH11K_WIRELESS_MODE_LEGACY,
	ATH11K_WIRELESS_MODE_HT,
	ATH11K_WIRELESS_MODE_VHT,
};

struct ath11k_smart_ant_node_config_params {
	u8 vdev_id;
	u32 cmd_id;
	u16 arg_count;
	u32 *arg_arr;
};

struct ath11k_smart_ant_peer_phy_info {
	u8 rxstreams;
	u8 streams;
	u8 cap;
	u32 mode;
	u32 ext_mode;
};

struct ath11k_smart_ant_ratetoindex {
	u8 ratecode;
	u8 rateindex;
};

struct ath11k_smart_ant_rate_info {
	struct ath11k_smart_ant_ratetoindex rates[ATH11K_SMART_ANT_MAX_RATES];
	u8 num_of_rates;
	u8 selected_antenna;
};

struct ath11k_smart_ant_train_info {
	u8 vdev_id;
	u32 rate_array[ATH11K_SMART_ANT_MAX_RATE_SERIES];
	u32 antenna_array[ATH11K_SMART_ANT_MAX_RX_CHAIN];
	u32 numpkts;
};

struct ath11k_smart_ant_train_data {
	u32 antenna;
	u32 ratecode;
	u16 nframes;
	u16 nbad;
	u8 rssi[ATH11K_SMART_ANT_MAX_CHAINS][ATH11K_SMART_ANT_RSSI_SAMPLE];
	u16 last_nframes;
	u16 numpkts;
	u8 cts_prot;
	u8 samples;
};

struct ath11k_peer_rate_code_list_cap {
	u32 rtcode_legacy[WMI_CCK_OFDM_RATES_MAX];
	u32 rtcode_20[WMI_MCS_RATES_MAX];
	u32 rtcode_40[WMI_MCS_RATES_MAX];
	u32 rtcode_80[WMI_MCS_RATES_MAX];
	u32 rt_count[WMI_RATE_COUNT_MAX];
};

struct ath11k_smart_ant_comb_fb {
	u8 nbad;
	u8 npkts;
	u8 bw;
	u8 rate;
};

struct ath11k_smart_ant_tx_feedback {
	u16 npackets;
	u16 nbad;
	u16 nshort_retries[ATH11K_SMART_ANT_MAX_RETRY];
	u16 nlong_retries[ATH11K_SMART_ANT_MAX_RETRY];
	u32 tx_antenna[ATH11K_SMART_ANT_MAX_RATE_SERIES];
	u32 rssi[ATH11K_SMART_ANT_MAX_SA_RSSI_CHAINS];
	u32 rate_mcs[ATH11K_SMART_ANT_MAX_RATE_SERIES];
	u8 rate_index;
	u8 is_trainpkt;
	u16 ratemaxphy[ATH11K_SMART_ANT_MAX_RATE_COUNTERS];
	u32 goodput;
	u8 num_comb_feedback;
	struct ath11k_smart_ant_comb_fb comb_fb[ATH11K_SMART_ANT_MAX_COMB_FB];
};

struct ath11k_smart_ant_sta {
	struct ath11k *ar;
	u8 mac_addr[ETH_ALEN];
	u8 max_bw;
	u8 txrx_chainmask;
	u8 mode;
	enum ath11k_wireless_mode wmode;
	u8 channel;
	enum ath11k_smart_ant_band_id band;
	u32 ni_cap;
	struct ath11k_peer_rate_code_list_cap rate_cap;
	struct ath11k_smart_ant_train_info train_info;
	struct ath11k_smart_ant_train_data train_data;
	struct ath11k_smart_ant_tx_feedback tx_feedback;
};

/**
 * struct smart_ant_enable_params - Smart antenna params
 * @enable: Enable/Disable
 * @mode: SA mode
 * @rx_antenna: RX antenna config
 * @gpio_pin : GPIO pin config
 * @gpio_func : GPIO function config
 */
struct smart_ant_enable_params {
	u32 gpio_pin[WMI_SMART_ANTENNA_HAL_MAX];
	u32 gpio_func[WMI_SMART_ANTENNA_HAL_MAX];
};

struct smart_ant_tx_ant_params {
	u32 *antenna_array;
	u8 vdev_id;
};

struct ath11k_smart_ant_params {
	u8 low_rate_threshold;
	u8 hi_rate_threshold;
	u8 per_diff_threshold;
	u16 num_train_pkts;
	u16 pkt_len;
	u8 num_tx_ant_comb;
	u8 num_rx_chain;
	u16 num_min_pkt;
	u32 retrain_interval;
	u32 perf_train_interval;
	u8 max_perf_delta;
	u8 hysteresis;
	u8 min_goodput_threshold;
	u8 avg_goodput_interval;
	u8 ignore_goodput_interval;
	u16 num_pretrain_pkts;
	u16 num_other_bw_pkts_threshold;
	u8 enabled_train;
	u16 num_pkt_min_threshod[ATH11K_SMART_ANT_BW_MAX];
	u32 default_tx_ant;
	u8 ant_change_ind;
	u16 max_train_ppdu;
};

struct ath11k_smart_ant_info {
	struct ath11k_smart_ant_params smart_ant_params;
	u8 num_fallback_rate;
	enum ath11k_smart_ant_band_id band_id;
	u8 txrx_chainmask;
	u32 txrx_feedback;
	u32 default_ant;
	u32 rx_antenna;
	u8 num_sta_per_ant[ATH11K_SMART_ANT_COMB_MAX];
	u16 num_sta_conneted;
	u8 mode;
	bool enabled;
	u32 num_enabled_vif;
};

#ifdef CPTCFG_ATH11K_SMART_ANT_ALG
int ath11k_smart_ant_alg_enable(struct ath11k_vif *arvif);
void ath11k_smart_ant_alg_disable(struct ath11k_vif *arvif);
int ath11k_smart_ant_alg_set_default(struct ath11k_vif *arvif);
int ath11k_smart_ant_alg_sta_connect(struct ath11k *ar,
				     struct ath11k_vif *arvif,
				     struct ieee80211_sta *sta);
void ath11k_smart_ant_alg_sta_disconnect(struct ath11k *ar,
					 struct ieee80211_sta *sta);
void ath11k_smart_ant_alg_proc_tx_feedback(struct ath11k_base *ab, u32 *data,
					   u16 peer_id);
#else
static inline
int ath11k_smart_ant_alg_enable(struct ath11k_vif *arvif)
{
	return 0;
}

static inline
void ath11k_smart_ant_alg_disable(struct ath11k_vif *arvif)
{
}

static inline
int ath11k_smart_ant_alg_set_default(struct ath11k_vif *arvif)
{
	return 0;
}

static inline
int ath11k_smart_ant_alg_sta_connect(struct ath11k *ar,
				     struct ath11k_vif *arvif,
				     struct ieee80211_sta *sta)
{
	return 0;
}

static inline
void ath11k_smart_ant_alg_sta_disconnect(struct ath11k *ar,
					 struct ieee80211_sta *sta)
{
}

static inline
void ath11k_smart_ant_alg_proc_tx_feedback(struct ath11k_base *ab,
					   u32 *data,
					   u16 peer_id)
{
}

#endif /* CPTCFG_ATH11K_SMART_ANT_ALG */
#endif /* _SMART_ANT_ALG_ */
