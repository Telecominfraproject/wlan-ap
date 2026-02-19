/*
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef TELEMETRY_AGENT_SAWF_H
#define TELEMETRY_AGENT_SAWF_H

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include "../inc/telemetry_agent.h"

#define SAWF_MIN_SVC_CLASS 1
#define SAWF_MAX_SVC_CLASS 128
#define SAWF_MAX_LATENCY_TYPE 3

#define MSEC_TO_JIFFIES(x) jiffies + msecs_to_jiffies(x)

#define pr_sawf_err(...) printk(KERN_ERR "SAWF Telemetry: " __VA_ARGS__)
#define pr_sawf_info(...) printk(KERN_INFO "SAWF Telemetry: " __VA_ARGS__)
#define pr_sawf_dbg(...) printk(KERN_DEBUG "SAWF Telemetry: " __VA_ARGS__)

#define DELAY_BOUND_UNIT 1000
#define THROUGHPUT_UNIT (1000/8)
#define MSDU_LOSS_UNIT 1/1000000

#define MAC_ADDR_SIZE 6

/**
 * telemetry_sawf_mov_avg_params - telemetry sawf moving average params
 * @packet: num packets
 * @window: window no
 */
struct telemetry_sawf_mov_avg_params {
	uint32_t packet;
	uint32_t window;
};

/**
 * telemetry_sawf_sla_params - telemetry sawf sla params
 * @num_packets: num packets
 * @time_secs: time in secs
 */
struct telemetry_sawf_sla_params {
	uint32_t num_packets;
	uint32_t time_secs;
};

/**
 * telemetry_sawf_sla_detect_type - telemetry sawf sla detect-type
 * @SAWF_SLA_DETECT_NUM_PACKET: per-n-pkts detection-type
 * @SAWF_SLA_DETECT_PER_SECOND: per-second detection-type
 * @SAWF_SLA_DETECT_MOV_AVG: movig-average detection-type
 * @SAWF_SLA_DETECT_NUM_SECOND: per-n-second detection-type
 * @SAWF_SLA_DETECT_MAX: max-number
 */
enum telemetry_sawf_sla_detect_type {
	SAWF_SLA_DETECT_NUM_PACKET,
	SAWF_SLA_DETECT_PER_SECOND,
	SAWF_SLA_DETECT_MOV_AVG,
	SAWF_SLA_DETECT_NUM_SECOND,
	SAWF_SLA_DETECT_MAX,
};

/**
 * telemetry_sawf_sla_param - telemetry sawf sla params
 * @min_thruput_rate: minimum throughput
 * @max_thruput_rate: maximum throughput
 * @burst_size: burst-size num-pkts
 * @service_interval: service-interval
 * @delay_bound: delay-bound
 * @msdu_ttl: TTL for msdu-drop
 * @msdu_rate_loss:  msdu-loss rate
 * @packet_error_rate: packet error rate
 * @mcs_min_threshold: mcs min threshold
 * @mcs_max_threshold: mcs max threshold
 * @retries_threshold: retries threshold
 */
struct telemetry_sawf_sla_param {
	uint8_t min_thruput_rate;
	uint8_t max_thruput_rate;
	uint8_t burst_size;
	uint8_t service_interval;
	uint8_t delay_bound;
	uint8_t msdu_ttl;
	uint8_t msdu_rate_loss;
	uint8_t packet_error_rate;
	uint8_t mcs_min_threshold;
	uint8_t mcs_max_threshold;
	uint8_t retries_threshold;
};

/**
 * telemetry_sawf_param - Enum indicating SAWF pram type
 * @SAWF_PARAM_MIN_THROUGHPUT: minimum throughput
 * @SAWF_PARAM_MAX_THROUGHPUT: maximum throughput
 * @SAWF_PARAM_BURST_SIZE: burst-size num-pkts
 * @SAWF_PARAM_SERVICE_INTERVAL: service-interval
 * @SAWF_PARAM_DELAY_BOUND: delay-bound
 * @SAWF_PARAM_MSDU_TTL: TTL for msdu-drop
 * @SAWF_PARAM_MSDU_LOSS:  msdu-loss rate
 * @SAWF_PARAM_PACKET_ERROR_RATE: packet error rate
 * @SAWF_PARAM_RETRIES_PERCENTAGE: retries percentage
 * @SAWF_PARAM_MCS_BOUND: mcs bound
 */
enum telemetry_sawf_param {
	SAWF_PARAM_INVALID,
	SAWF_PARAM_MIN_THROUGHPUT,
	SAWF_PARAM_MAX_THROUGHPUT,
	SAWF_PARAM_BURST_SIZE,
	SAWF_PARAM_SERVICE_INTERVAL,
	SAWF_PARAM_DELAY_BOUND,
	SAWF_PARAM_MSDU_TTL,
	SAWF_PARAM_MSDU_LOSS,
	SAWF_PARAM_PACKET_ERROR_RATE,
	SAWF_PARAM_RETRIES_PERCENTAGE,
	SAWF_PARAM_MCS_BOUND,
	SAWF_PARAM_MAX,
};

/**
 * telemetry_sawf_svc_class_param - telemetry sawf sla params
 * @min_thruput_rate: minimum throughput
 * @max_thruput_rate: maximum throughput
 * @burst_size: burst-size num-pkts
 * @service_interval: service-interval
 * @delay_bound: delay-bound
 * @msdu_ttl: TTL for msdu-drop
 * @msdu_rate_loss:  msdu-loss rate
 * @packet_error_rate: packet error rate
 * @mcs_min_threshold: mcs min threshold
 * @mcs_max_threshold: mcs max threshold
 * @retries_threshold: retries threshold
 */
struct telemetry_sawf_svc_class_param {
	uint32_t min_thruput_rate;
	uint32_t max_thruput_rate;
	uint32_t burst_size;
	uint32_t service_interval;
	uint32_t delay_bound;
	uint32_t msdu_ttl;
	uint32_t msdu_rate_loss;
	uint32_t packet_error_rate;
	uint32_t mcs_min_threshold;
	uint32_t mcs_max_threshold;
	uint32_t retries_threshold;
};

/**
 * telemetry_sawf_sla - telemetry sawf sla-config
 * @sla_cfg: array of sla- params
 * @sla_service_ids: bitmap of service ID with SLA configured
 */
struct telemetry_sawf_sla {
	struct telemetry_sawf_sla_param sla_cfg[SAWF_MAX_SVC_CLASS];
	DECLARE_BITMAP(sla_service_ids, SAWF_MAX_SVC_CLASS);
};

/**
 * telemetry_sawf_svc_class- telemetry sawf service class config
 * @svc_class: array of service class params
 * @service_ids: bitmap of service clas ID active
 */
struct telemetry_sawf_svc_class {
	struct telemetry_sawf_svc_class_param svc_class[SAWF_MAX_SVC_CLASS];
	DECLARE_BITMAP(service_ids, SAWF_MAX_SVC_CLASS);
};

/**
 * telemetry_sawf_sla_detect_cfg - telemetry sawf sla-detect config
 * @sla_detect: array of sla_detect params
 */
struct telemetry_sawf_sla_detect_cfg {
	struct telemetry_sawf_sla_param sla_detect[SAWF_SLA_DETECT_MAX];
};

/**
 * telemetry_sawf_ctx - telemetry sawf context
 * @mov_avg: moving average params
 * @sla_params: sla-params
 * @vc_class: service class params
 * @sla: sla config array for all service-classes
 * @sla_detect: which type of SLA detections are enabled for a svc-class
 * @per_sec_timer: per-second timer list
 * @num_sec_timer: per-n-second timer list
 * @peer_list: list of peers
 * @lock: lock to protect the peer list
 */
struct telemetry_sawf_ctx {
	struct telemetry_sawf_mov_avg_params mov_avg;
	struct telemetry_sawf_sla_params sla_param;
	struct telemetry_sawf_svc_class svc_class;
	struct telemetry_sawf_sla sla;
	struct telemetry_sawf_sla_detect_cfg sla_detect;
	struct timer_list per_sec_timer;
	struct timer_list num_sec_timer;
	struct list_head peer_list;
	spinlock_t peer_list_lock;
};

/**
 * telemetry_sawf_throughput - telemetry sawf throughput-stats
 * @last_bytes: last byte-count
 * @last_count: last pkt-count
 * @avg_counter: cycle counter
 * @rate: last rate
 * @min_throughput: min throughput
 * @max_throughput: max throughput
 * @avg_throughput: avg_throughput
 */
struct telemetry_sawf_throughput
{
	uint64_t last_bytes;
	uint64_t last_count;
	uint64_t last_in_bytes;
	uint64_t last_in_count;
	uint64_t avg_counter;
	uint32_t in_rate;
	uint32_t eg_rate;
	uint32_t min_throughput;
	uint32_t max_throughput;
	uint32_t avg_throughput;
};

/**
 * telemetry_sawf_drops - telemetry sawf drop-stats
 * @last_xmit_cnt_seconds: last tx-success-count for n-secs
 * @last_drop_cnt_seconds: last tx-drop-count for n-secs
 * @last_xmit_cnt_packets: last tx-success-count for n-pkts
 * @last_drop_cnt_packets: last tx-drop-count for n-pkts
 * @last_ttl_drop_cnt_packets: last ttl-drop-count for n-pkts
 */
struct telemetry_sawf_drops
{
	uint64_t last_xmit_cnt_seconds;
	uint64_t last_drop_cnt_seconds;
	uint64_t last_ttl_drop_cnt_seconds;
	uint64_t last_xmit_cnt_packets;
	uint64_t last_drop_cnt_packets;
	uint64_t last_ttl_drop_cnt_packets;
};

/**
 * telemetry_service_interval - telemetry service-interval stats
 * @last_success: last-success count
 * @last_success: last-failure count
 */
struct telemetry_service_interval
{
	uint64_t last_success;
	uint64_t last_failure;
};

/**
 * telemetry_burst_size - telemetry burst-size stats
 * @last_success: last-success count
 * @last_success: last-failure count
 */
struct telemetry_burst_size
{
	uint64_t last_success;
	uint64_t last_failure;
};


/**
 * telemetry_per_retries - telemetry per and retries stats
 * @packet_error_rate: packet error rate
 * @retries_percentage: retries percentage
 */
struct telemetry_per_retries {
	uint64_t packet_error_rate;
	uint64_t retries_percentage;
};

/**
 * telemetry_sawf_tx - telemetry sawf tx-stats
 * @throughput: throughput
 * @msdu_drop: msdu drop count
 * @service_interval: service-interval num
 * @burst_size: burst-size stats
 * @retries_mcs_stats: retries and mcs stats
 * @per_retries_stats: per and retreis stats
 */
struct telemetry_sawf_tx {
	struct telemetry_sawf_throughput throughput;
	struct telemetry_sawf_drops msdu_drop;
	struct telemetry_service_interval service_interval;
	struct telemetry_burst_size burst_size;
	struct telemetry_msduq_tx_stats retries_mcs_stats;
	struct telemetry_per_retries per_retries_stats;
};

/**
 * telemetry_sawf_delay - telemetry sawf delay-stats
 * @last_success: last-success count
 * @last_success: last-failure count
 * @cur_window: current window no
 * @nwdelay_avg: moving average of nwdelay for all windows
 * @swdelay_avg: moving average of swdelay for all windows
 * @hwdelay_avg: moving average of hwdelay for all windows
 * @nwdelay_win_total: sum of nwdelay stats for all windows
 * @swdelay_win_total: sum of swdelay stats for all windows
 * @hwdelay_win_total: sum of hwdelay stats for all windows
 */
struct telemetry_sawf_delay {
	uint64_t last_success;
	uint64_t last_failure;
	uint64_t cur_window;
	uint32_t nwdelay_avg;
	uint32_t swdelay_avg;
	uint32_t hwdelay_avg;
	uint64_t nwdelay_win_total;
	uint64_t swdelay_win_total;
	uint64_t hwdelay_win_total;
};

/**
 * telemetry_sawf_stats - sawf telemetry stats
 * @tx: Tx stats
 * @delay: delay stats
 */
struct telemetry_sawf_stats {
	struct telemetry_sawf_tx tx;
	struct telemetry_sawf_delay delay;
	bool breach[SAWF_PARAM_MAX];
};

/**
 * sawf_msduq - sawf msud-queue
 * @tid: tis no
 * @htt_msduq: htt msdu-queue no
 * @svc_id: service-class id
 */
struct sawf_msduq {
	uint8_t tid;
	uint8_t htt_msduq;
	uint8_t svc_id;
};

/**
 * peer_sawf_queue - sawf-peer queue
 * @active_queue: bitmap for active service-class id
 * @msduq_map: msdu_queue-tid map
 * @msduq: msdu-queue array
 */
struct peer_sawf_queue {
	DECLARE_BITMAP(active_queue, SAWF_MAX_QUEUES);
	uint8_t msduq_map[NUM_TIDS][SAWF_MAXQ_PTID];
	struct sawf_msduq msduq[SAWF_MAX_QUEUES];
};

/**
 * telemetry_sawf_peer_ctx - Telemetry sawf-peer ctx
 * @node: list-head
 * @stats: telemetry-sawf stats
 * @mov_window:pointer to moving-window
 * @dp_peer: opaque dp-peer handle
 * @sawf-ctx: opaque peer_sawf ctx
 * @soc: opaque soc-ctx
 * @peer_msduq: per-peer msduq info
 *
 */
struct telemetry_sawf_peer_ctx {
	struct list_head node;
	struct telemetry_sawf_stats stats[SAWF_MAX_QUEUES];
	uint64_t *mov_window;
	void *sawf_ctx;
	void *soc;
	uint8_t mac_addr[MAC_ADDR_SIZE];
	struct peer_sawf_queue peer_msduq;
};

/**
 * telemetry_sawf_init_ctx - Initialize sawf-telemetry ctx
 *
 * Return: 0 on success
 */
int telemetry_sawf_init_ctx(void);

/**
 * telemetry_sawf_free_ctx - Free sawf-telemetry ctx
 *
 * Return: none
 */
void telemetry_sawf_free_ctx(void);

/**
 * telemetry_sawf_get_ctx - get sawf-telemetry ctx
 *
 * Return: sawf-telemetry pointer
 */
struct telemetry_sawf_ctx *telemetry_sawf_get_ctx(void);

/**
 * telemetry_sawf_set_sla_detect_cfg- Set sla config per detection-type
 * @svc_id: service-class id
 * @min_thruput_rate: minimum throughput
 * @max_thruput_rate: maximum throughput
 * @burst_size: burst-size num-pkts
 * @service_interval: service-interval
 * @delay_bound: delay-bound
 * @msdu_ttl: TTL for msdu-drop
 * @msdu_rate_loss:  msdu-loss rate
 *
 * Return: 0 on success
 */
int telemetry_sawf_set_sla_detect_cfg(uint8_t type,
				      uint8_t min_thruput_rate,
				      uint8_t max_thruput_rate,
				      uint8_t burst_size,
				      uint8_t service_interval,
				      uint8_t delay_bound,
				      uint8_t msdu_ttl,
				      uint8_t msdu_rate_loss);

/**
 * telemetry_sawf_set_sla_detect_config- Set sla config per detection-type
 * @svc_id: service-class id
 * @min_thruput_rate: minimum throughput
 * @max_thruput_rate: maximum throughput
 * @burst_size: burst-size num-pkts
 * @service_interval: service-interval
 * @delay_bound: delay-bound
 * @msdu_ttl: TTL for msdu-drop
 * @msdu_rate_loss:  msdu-loss rate
 * @packet_error_rate: packet error rate
 * @mcs_min_threshold: mcs min threshold
 * @mcs_max_threshold: mcs max threshold
 * @retries_threshold: retries threshold
 *
 * Return: 0 on success
 */
int telemetry_sawf_set_sla_detect_config(uint8_t type,
					 uint8_t min_thruput_rate,
					 uint8_t max_thruput_rate,
					 uint8_t burst_size,
					 uint8_t service_interval,
					 uint8_t delay_bound,
					 uint8_t msdu_ttl,
					 uint8_t msdu_rate_loss,
					 uint8_t packet_error_rate,
					 uint8_t mcs_min_threshold,
					 uint8_t mcs_max_threshold,
					 uint8_t retries_threshold);

/**
 * telemetry_sawf_set_sla_cfg - set per-service-class sla config
 * @svc_id: service-class id
 * @min_thruput_rate: minimum throughput
 * @max_thruput_rate: maximum throughput
 * @burst_size: burst-size num-pkts
 * @service_interval: service-interval
 * @delay_bound: delay-bound
 * @msdu_ttl: TTL for msdu-drop
 * @msdu_rate_loss:  msdu-loss rate
 *
 * Return: 0 on success
 */
int telemetry_sawf_set_sla_cfg(uint8_t svc_id,
			       uint8_t min_thruput_rate,
			       uint8_t max_thruput_rate,
			       uint8_t burst_size,
			       uint8_t service_interval,
			       uint8_t delay_bound,
			       uint8_t msdu_ttl,
			       uint8_t msdu_rate_loss);

/**
 * telemetry_sawf_set_sla_config - set per-service-class sla config
 * @svc_id: service-class id
 * @min_thruput_rate: minimum throughput
 * @max_thruput_rate: maximum throughput
 * @burst_size: burst-size num-pkts
 * @service_interval: service-interval
 * @delay_bound: delay-bound
 * @msdu_ttl: TTL for msdu-drop
 * @msdu_rate_loss:  msdu-loss rate
 * @packet_error_rate: packet error rate
 * @mcs_min_threshold: mcs min threshold
 * @mcs_max_threshold: mcs max threshold
 * @retries_threshold: retries threshold
 *
 * Return: 0 on success
 */
int telemetry_sawf_set_sla_config(uint8_t svc_id,
				  uint8_t min_thruput_rate,
				  uint8_t max_thruput_rate,
				  uint8_t burst_size,
				  uint8_t service_interval,
				  uint8_t delay_bound,
				  uint8_t msdu_ttl,
				  uint8_t msdu_rate_loss,
				  uint8_t packet_error_rate,
				  uint8_t mcs_min_threshold,
				  uint8_t mcs_max_threshold,
				  uint8_t retries_threshold);

/**
 * telemetry_sawf_set_svclass_cfg - set per-service-class config
 * @enable: enable/disable
 * @svc_id: service-class id
 * @min_thruput_rate: minimum throughput
 * @max_thruput_rate: maximum throughput
 * @burst_size: burst-size num-pkts
 * @service_interval: service-interval
 * @delay_bound: delay-bound
 * @msdu_ttl: TTL for msdu-drop
 * @msdu_rate_loss:  msdu-loss rate
 *
 * Return: 0 on success
 */
int telemetry_sawf_set_svclass_cfg(bool enable,
				   uint8_t svc_id,
				   uint32_t min_thruput_rate,
				   uint32_t max_thruput_rate,
				   uint32_t burst_size,
				   uint32_t service_interval,
				   uint32_t delay_bound,
				   uint32_t msdu_ttl,
				   uint32_t msdu_rate_loss);


/**
 * telemetry_sawf_set_mov_avg_params- Set moving-average params
 * @num_packet: no of pkts
 * @window: window-no in use
 *
 * Return: 0 on success
 */
int telemetry_sawf_set_mov_avg_params(uint32_t packet, uint32_t window);

/**
 * telemetry_sawf_set_sla_params- Set sla-params
 * @num_packet: no of pkts
 * @time_secs: time in secs
 *
 * Return: 0 on success
 */
int telemetry_sawf_set_sla_params(uint32_t num_packet,
				  uint32_t time_secs);

/**
 * telemetry_sawf_alloc_peer- Allocate telemetry-peer context
 * @soc: soc handle
 * @sawf_stats_ctx: peer sawf-stats ctx
 * @mac_addr: MAC address
 * @svc_id: service-class id
 * @host_q_id: host msdu-queue id
 *
 * Return: opaque pointer
 */
void *telemetry_sawf_alloc_peer(void * soc, void *sawf_stats_ctx,
				uint8_t *mac_addr,
				uint8_t svc_id, uint8_t host_q_id);

/**
 * telemetry_sawf_update_queue_info- Update msdu-queue info
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @svc-id: service-class id
 * @tid: tid no
 * @msduq: msdu-queue no
 *
 * Return: 0 on success
 */
int telemetry_sawf_update_queue_info(void *telemetry_ctx,
				     uint8_t svc_id, uint8_t tid,
				     uint8_t msduq);

/**
 * telemetry_sawf_update_msdu_queue_info- Update msdu-queue info
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @hostq_id: host queue id
 * @tid: tid no
 * @msduq: msdu-queue no
 * @svc-id: service-class id
 *
 * Return: 0 on success
 */
int telemetry_sawf_update_msdu_queue_info(void *telemetry_ctx,
					  uint8_t hostq_id, uint8_t tid,
					  uint8_t msduq, uint8_t svc_id);

/**
 * telemetry_sawf_clear_msdu_queue_info- CLear Msduq active bit
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @hostq_id: host queue id
 *
 * Return: 0 on success
 */
int telemetry_sawf_clear_msdu_queue_info(void *telemetry_ctx,
					 uint8_t host_q_id);
/**
 * telemetry_sawf_free_peer - Free telemetry-peer context
 * @telemetry_peer_ctx: telemetry-peer context
 *
 * Return: 0 on success
 */
void telemetry_sawf_free_peer(void *telemetry_peer_ctx);

/**
 * telemetry_sawf_peer_stats_reset - Reset peer telemetry stats
 * @telemetry_peer_ctx: telemetry-peer context
 *
 * Return: none
 */
void telemetry_sawf_peer_stats_reset(void *telemetry_peer_ctx);

/**
 * telemetry_sawf_update_peer_delay - Update delay-stats
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @tid: tid no
 * @queue: queue no
 * @pass: no of pkts that passed delay-bound
 * @fail: no of pkts that failed delay-bound
 *
 * Return: 0 on success
 */
int telemetry_sawf_update_peer_delay(void *telemetry_ctx, uint8_t tid,
				uint8_t queue, uint64_t pass,
				uint64_t fail);

/**
 * telemetry_sawf_update_peer_delay_mov_avg - Update moving-avg delay-stats
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @tid: tid no
 * @queue: queue no
 * @nwdelay_avg: moving window average for n/w delay
 * @swdelay_avg: moving window average for s/w delay
 * @hwdelay_avg: moving window average for h/w delay
 *
 * Return: 0 on success
 */
int telemetry_sawf_update_peer_delay_mov_avg(void *telemetry_ctx,
					     uint8_t tid, uint8_t queue,
					     uint64_t nwdelay_avg,
					     uint64_t swdelay_avg,
					     uint64_t hwdelay_avg);

/**
 * telemetry_sawf_update_msdu_drop - Update msdu-drop stats
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @tid: tid no
 * @queue: queue no
 * @pass: no of pkts successfully txed
 * @fail_drop: no of dropped pkts
 * @fail_ttl: no of pkts dropped due to TTL-expiry
 * @mov_avg: pointer to rate
 *
 * Return: 0 on success
 */
int telemetry_sawf_update_msdu_drop(void *telemetry_ctx, uint8_t tid,
				    uint8_t queue, uint64_t pass,
				    uint64_t fail_drop,
				    uint64_t fail_ttl);

/**
 * telemetry_sawf_get_mov_avg_param - Get moving-average param
 *
 * Return: pointer to memory holding moving-average-param
 */
struct telemetry_sawf_mov_avg_params* telemetry_sawf_get_mov_avg_param
								(void);

/**
 * telemetry_sawf_get_sla_param - Get sla-param
 *
 * Return: pointer to memory holding sla-param
 */
struct telemetry_sawf_sla_params* telemetry_sawf_get_sla_param(void);

/**
 * telemetry_sawf_delay_window_alloc - Allocate delay-window
 *
 * Return: pointer to delay-window
 */
uint64_t *telemtry_sawf_delay_window_alloc(void);

/**
 * telemetry_sawf_delay_window_free - Free delay-window
 * @mov_window: delay-window pointer
 *
 * Return: 0 on success
 */
int telemtry_sawf_delay_window_free(uint64_t *mov_window);

/**
 * telemetry_sawf_get_rate - Get rate stats
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @tid: tid no
 * @queue: queue no
 * @egress_rate: pointer to egress rate
 * @inress_rate: pointer to inress rate
 *
 * Return: 0 on success
 */
int telemetry_sawf_get_rate(void *telemetry_ctx, uint8_t tid, uint8_t queue,
			    uint32_t *egress_rate, uint32_t *ingress_rate);

/**
 * telemetry_sawf_get_tx_rate - Get rate stats
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @tid: tid no
 * @queue: queue no
 * @: pointer to min throughput
 * @: pointer to max throughput
 * @: pointer to average throughput
 * @: pointer to Tx PER
 * @: pointer to TX retries percentage
 *
 * Return: 0 on success
 */

int telemetry_sawf_get_tx_rate(void *telemetry_ctx, uint8_t tid, uint8_t queue,
			    uint32_t *min_tput, uint32_t *max_tput,
			    uint32_t *avg_tput, uint32_t *per,
			    uint32_t *retries_pct);

/**
 * telemetry_sawf_pull_mov_avg - Get moving window average stats
 * @telemetry_ctx: pointer to sawf-peer-telemetry ctx
 * @tid: tid no
 * @queue: queue no
 * @nwdelay_avg: pointer to nwdelay moving-avg data
 * @swdelay_avg: pointer to swdelay moving-avg data
 * @hwdelay_avg: pointer to hwdelay moving-avg data
 *
 * Return: 0 on success
 */
int telemetry_sawf_pull_mov_avg(void *telemetry_ctx, uint8_t tid, uint8_t queue,
				uint32_t *nwdelay_avg, uint32_t *swdelay_avg,
				uint32_t *hwdelay_avg);

/**
 * telemetry_sawf_init_per_sec_sla_timer - Initialize per-sec timer to
 * 					   fetch stats
 *
 * Return: 0 on success
 */
int telemetry_sawf_init_per_sec_sla_timer(void);

/**
 * telemetry_sawf_init_sla_timer - Initialize sla timer
 *
 * Return: 0 on success
 */
int telemetry_sawf_init_sla_timer(void);

/**
 * telemetry_sawf_free_sla_timer- Initialize sla timer
 *
 * Return: 0 on success
 */
int telemetry_sawf_free_sla_timer(void);

/**
 * telemetry_detect_sla_per_sec - Detect sla breach per second
 * @timer: pointer to timer structure
 *
 * Return: void
 */
void telemetry_detect_sla_per_sec(struct timer_list *timer);

/**
 * telemetry_detect_sla_num_sec - Detect sla breach per n-seconds
 * @timer: pointer to timer structure
 *
 * Return: void
 */
void telemetry_detect_sla_num_sec(struct timer_list *timer);

/**
 * telemetry_sawf_reset_peer_stats - Reset stats for a peer
 * @mac_addr: pointer to peer mac address
 *
 * Return: 0 on success
 */
int telemetry_sawf_reset_peer_stats(uint8_t *mac_addr);
#endif /* TELEMETRY_AGENT_SAWF_H */
