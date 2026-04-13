/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021, 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH11K_DP_TX_H
#define ATH11K_DP_TX_H

#include "core.h"
#include "hal_tx.h"

#define ATH11K_NUM_PKTS_THRSHLD_FOR_PER  50
#define ATH11K_GET_PERCENTAGE(value, total_value) (((value)*100)/(total_value))
#define ATH11K_NUM_BYTES_THRSHLD_FOR_BER 25000
#define ATH11K_TX_PKTS_BW_OFFSET	3

struct ath11k_dp_htt_wbm_tx_status {
	u32 msdu_id;
	bool acked;
	s8 ack_rssi;
	u16 peer_id;
};

/* htt_tx_msdu_desc_ext
 *
 * valid_pwr
 *		if set, tx pwr spec is valid
 *
 * valid_mcs_mask
 *		if set, tx MCS mask is valid
 *
 * valid_nss_mask
 *		if set, tx Nss mask is valid
 *
 * valid_preamble_type
 *		if set, tx preamble spec is valid
 *
 * valid_retries
 *		if set, tx retries spec is valid
 *
 * valid_bw_info
 *		if set, tx dyn_bw and bw_mask are valid
 *
 * valid_guard_interval
 *		if set, tx guard intv spec is valid
 *
 * valid_chainmask
 *		if set, tx chainmask is valid
 *
 * valid_encrypt_type
 *		if set, encrypt type is valid
 *
 * valid_key_flags
 *		if set, key flags is valid
 *
 * valid_expire_tsf
 *		if set, tx expire TSF spec is valid
 *
 * valid_chanfreq
 *		if set, chanfreq is valid
 *
 * is_dsrc
 *		if set, MSDU is a DSRC frame
 *
 * guard_interval
 *		0.4us, 0.8us, 1.6us, 3.2us
 *
 * encrypt_type
 *		0 = NO_ENCRYPT,
 *		1 = ENCRYPT,
 *		2 ~ 3 - Reserved
 *
 * retry_limit
 *		Specify the maximum number of transmissions, including the
 *		initial transmission, to attempt before giving up if no ack
 *		is received.
 *		If the tx rate is specified, then all retries shall use the
 *		same rate as the initial transmission.
 *		If no tx rate is specified, the target can choose whether to
 *		retain the original rate during the retransmissions, or to
 *		fall back to a more robust rate.
 *
 * use_dcm_11ax
 *		If set, Use Dual subcarrier modulation.
 *		Valid only for 11ax preamble types HE_SU
 *		and HE_EXT_SU
 *
 * ltf_subtype_11ax
 *		Takes enum values of htt_11ax_ltf_subtype_t
 *		Valid only for 11ax preamble types HE_SU
 *		and HE_EXT_SU
 *
 * dyn_bw
 *		0 = static bw, 1 = dynamic bw
 *
 * bw_mask
 *		Valid only if dyn_bw == 0 (static bw).
 *
 * host_tx_desc_pool
 *		If set, Firmware allocates tx_descriptors
 *		in WAL_BUFFERID_TX_HOST_DATA_EXP,instead
 *		of WAL_BUFFERID_TX_TCL_DATA_EXP.
 *		Use cases:
 *		Any time firmware uses TQM-BYPASS for Data
 *		TID, firmware expect host to set this bit.
 *
 * power
 *		unit of the power field is 0.5 dbm
 *		signed value ranging from -64dbm to 63.5 dbm
 *
 * mcs_mask
 *		mcs bit mask of 0 ~ 11
 *		Setting more than one MCS isn't currently
 *		supported by the target (but is supported
 *		in the interface in case in the future
 *		the target supports specifications of
 *		a limited set of MCS values.
 *
 * nss_mask
 *		Nss bit mask 0 ~ 7
 *		Setting more than one Nss isn't currently
 *		supported by the target (but is supported
 *		in the interface in case in the future
 *		the target supports specifications of
 *		a limited set of Nss values.
 *
 * pream_type
 *		Preamble types
 *
 * update_peer_cache
 *		When set these custom values will be
 *		used for all packets, until the next
 *		update via this ext header.
 *		This is to make sure not all packets
 *		need to include this header.
 *
 * chain_mask
 *		specify which chains to transmit from
 *
 * key_flags
 *		Key Index and related flags - used in mesh mode
 *
 * chanfreq
 *		Channel frequency: This identifies the desired channel
 *		frequency (in MHz) for tx frames. This is used by FW to help
 *		determine when it is safe to transmit or drop frames for
 *		off-channel operation.
 *		The default value of zero indicates to FW that the corresponding
 *		VDEV's home channel (if there is one) is the desired channel
 *		frequency.
 *
 * expire_tsf_lo
 *		tx expiry time (TSF) LSBs
 *
 * expire_tsf_hi
 *		tx expiry time (TSF) MSBs
 *
 * learning_frame
 *		When this flag is set, this frame will be dropped by FW
 *		rather than being enqueued to the Transmit Queue Manager (TQM) HW.
 *
 * send_as_standalone
 *		This will indicate if the msdu needs to be sent as a singleton PPDU,
 *		i.e. with no A-MSDU or A-MPDU aggregation.
 *		The scope is extended to other use-cases.
 *
 * is_host_opaque_valid
 *		set this bit to 1 if the host_opaque_cookie is populated
 *		with valid information.
 *
 * host_opaque_cookie
 *		Host opaque cookie for special frames
 */

struct htt_tx_msdu_desc_ext {
	u32
		valid_pwr            : 1,
		valid_mcs_mask       : 1,
		valid_nss_mask       : 1,
		valid_preamble_type  : 1,
		valid_retries        : 1,
		valid_bw_info        : 1,
		valid_guard_interval : 1,
		valid_chainmask      : 1,
		valid_encrypt_type   : 1,
		valid_key_flags      : 1,
		valid_expire_tsf     : 1,
		valid_chanfreq       : 1,
		is_dsrc              : 1,
		guard_interval       : 2,
		encrypt_type         : 2,
		retry_limit          : 4,
		use_dcm_11ax         : 1,
		ltf_subtype_11ax     : 2,
		dyn_bw               : 1,
		bw_mask              : 6,
		host_tx_desc_pool    : 1;
	u32
		power                : 8,
		mcs_mask             : 12,
		nss_mask             : 8,
		pream_type           : 3,
		update_peer_cache    : 1;
	u32
		chain_mask         : 8,
		key_flags          : 8,
		chanfreq           : 16;

	u32 expire_tsf_lo;
	u32 expire_tsf_hi;

	u32
		learning_frame       :  1,
		send_as_standalone   :  1,
		is_host_opaque_valid :  1,
		rsvd0                : 29;
	u32
		host_opaque_cookie  : 16,
		rsvd1               : 16;
} __packed;

void ath11k_dp_tx_update_txcompl(struct ath11k *ar, struct hal_tx_status *ts);
int ath11k_dp_tx_htt_h2t_ver_req_msg(struct ath11k_base *ab);
int ath11k_dp_tx(struct ath11k *ar, struct ath11k_vif *arvif,
		 struct ath11k_sta *arsta, struct sk_buff *skb);
int ath11k_dp_tx_simple(struct ath11k *ar, struct ath11k_vif *arvif,
			struct sk_buff *skb, struct ath11k_sta *arsta);
void ath11k_dp_tx_completion_handler(struct ath11k_base *ab, int ring_id);
int ath11k_dp_tx_send_reo_cmd(struct ath11k_base *ab, struct dp_rx_tid *rx_tid,
			      enum hal_reo_cmd_type type,
			      struct ath11k_hal_reo_cmd *cmd,
			      void (*func)(struct ath11k_dp *, void *,
					   enum hal_reo_cmd_status));

int ath11k_dp_tx_htt_h2t_ppdu_stats_req(struct ath11k *ar, u32 mask);
int
ath11k_dp_tx_htt_h2t_ext_stats_req(struct ath11k *ar, u8 type,
				   struct htt_ext_stats_cfg_params *cfg_params,
				   u64 cookie);
int ath11k_dp_tx_htt_monitor_mode_ring_config(struct ath11k *ar, bool reset);

int ath11k_dp_tx_htt_rx_filter_setup(struct ath11k_base *ab, u32 ring_id,
				     int mac_id, enum hal_ring_type ring_type,
				     int rx_buf_size,
				     struct htt_rx_ring_tlv_filter *tlv_filter);
enum hal_tcl_encap_type
ath11k_dp_tx_get_encap_type(struct ath11k_vif *arvif, struct sk_buff *skb);

int ath11k_dp_tx_htt_rx_full_mon_setup(struct ath11k_base *ab, int mac_id,
				       bool config);

static inline void ath11k_sta_stats_update_per(struct ath11k_sta *arsta) {
	int per;

	if(!arsta)
		return;

	per = ATH11K_GET_PERCENTAGE(arsta->per_fail_pkts,
				    arsta->per_fail_pkts + arsta->per_succ_pkts);
	ewma_sta_per_add(&arsta->per, per);
	arsta->per_fail_pkts = 0;
	arsta->per_succ_pkts = 0;
}

static inline void ath11k_sta_stats_update_ber(struct ath11k_sta *arsta) {
	int ber;

	if(!arsta)
		return;

	ber = ATH11K_GET_PERCENTAGE(arsta->ber_fail_bytes,
				    arsta->ber_fail_bytes + arsta->ber_succ_bytes);
	ewma_sta_ber_add(&arsta->ber, ber);
	arsta->ber_fail_bytes = 0;
	arsta->ber_succ_bytes = 0;
}
#endif
