/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ATH12K_TELEMETRY_H
#define ATH12K_TELEMETRY_H

/**
 * struct ath12k_sla_samples_cfg- telemetry qos sla samples configuration
 * @moving_avg_pkt: Number of packets per window to calculate moving average
 * @moving_avg_win: Number of windows to calculate moving average
 * @sla_num_pkt: Number of packets for SLA detection
 * @sla_time_sec:  Number of seconds for SLA detection
 */
struct ath12k_sla_samples_cfg {
	u32 moving_avg_pkt;
	u32 moving_avg_win;
	u32 sla_num_pkt;
	u32 sla_time_sec;
};

/**
 * struct ath12k_sla_thershold_cfg- telemetry qos sla
 * thershold configuration
 * @svc_id: service class id
 * @min_throughput_rate: min throughput thershold percentage
 * @max_throughput_rate: max throughput thershold percentage
 * @burst_size:  burst size thershold percentage
 * @service_interval: service interval thershold percentage
 * @delay_bound: delay bound thershold percentage
 * @msdu_ttl: MSDU Time-To-Live thershold percentage
 * @msdu_rate_loss: MSDU loss rate thershold percentage
 */
struct ath12k_sla_thershold_cfg {
	u8 svc_id;
	u32 min_throughput_rate;
	u32 max_throughput_rate;
	u32 burst_size;
	u32 service_interval;
	u32 delay_bound;
	u32 msdu_ttl;
	u32 msdu_rate_loss;
        u8 per;
        u8 mcs_min_thres;
        u8 mcs_max_thres;
        u8 retries_thres;
};

/**
 * struct ath12k_sla_detect- telemetry qos sla
 * sla breach detection option
 * @SLA_DETECT_NUM_PACKET: Number of packets per window
 * @SLA_DETECT_PER_SECOND: Number of windows
 * @SLA_DETECT_MOV_AVG: Number of packets to calculate
 *			moving average for SLA detection
 * @SLA_DETECT_NUM_SECOND:  Number of seconds for SLA detection
 */
enum ath12k_sla_detect {
	SLA_DETECT_NUM_PACKET,
	SLA_DETECT_PER_SECOND,
	SLA_DETECT_MOV_AVG,
	SLA_DETECT_NUM_SECOND,
	SLA_DETECT_MAX,
};

/**
 * struct ath12k_sla_detect_cfg- telemetry qos sla
 * breach detection configuration
 * @sla_detect: sla detection option
 * @min_throughput_rate: min throughput thershold percentage
 * @max_throughput_rate: max throughput thershold percentage
 * @burst_size:  burst size thershold percentage
 * @service_interval: service interval thershold percentage
 * @delay_bound: delay bound thershold percentage
 * @msdu_ttl: MSDU Time-To-Live thershold percentage
 * @msdu_rate_loss: MSDU loss rate thershold percentage
 */
struct ath12k_sla_detect_cfg {
	enum ath12k_sla_detect sla_detect;
	u32 min_throughput_rate;
	u32 max_throughput_rate;
	u32 burst_size;
	u32 service_interval;
	u32 delay_bound;
	u32 msdu_ttl;
	u32 msdu_rate_loss;
	u8 per;
	u8 mcs_min_thres;
	u8 mcs_max_thres;
	u8 retries_thres;
};

/**
 * struct ath12k_telemetry_ctx- Telemetry context
 */
struct ath12k_telemetry_ctx {
	struct ath12k_sla_samples_cfg sla_samples_params;
	/* Used to protect the list */
	spinlock_t breach_ind_lock;
	struct workqueue_struct *workqueue;
	struct work_struct indicate_breach;
	struct list_head list;
};

void ath12k_telemetry_init(struct ath12k_base *ab);
void ath12k_telemetry_deinit(struct ath12k_base *ab);
int ath12k_telemetry_sdwf_sla_samples_config(struct ath12k_sla_samples_cfg param);
int ath12k_telemetry_sdwf_sla_thershold_config(struct ath12k_sla_thershold_cfg param);
int ath12k_telemetry_sdwf_sla_detection_config(struct ath12k_sla_detect_cfg param);
bool ath12k_telemetry_get_sla_num_pkts(u32 *pkt_num);
bool ath12k_telemetry_get_sla_mov_avg_num_pkt(u32 *mov_avg);
void ath12k_send_breach_indication(struct work_struct *work);
void ath12k_telemetry_breach_indication(u8 *mac_addr, u8 svc_id, u8 param, bool set_clear, u8 tid);
#endif /* ATH12K_TELEMETRY_H */
