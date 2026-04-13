/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ATH12K_TELEMETRY_AGENT_IF_H
#define ATH12K_TELEMETRY_AGENT_IF_H

#include "telemetry.h"
#include "telemetry_agent_wifi_driver_if.h"
#include "core.h"

int ath12k_telemetry_ab_agent_create_handler(struct ath12k_base *ab);
int ath12k_telemetry_ab_agent_delete_handler(struct ath12k_base *ab);

int register_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops);
int unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops);

int ath12k_get_pdev_stats(void *obj, struct agent_link_iface_stats_obj *stats);
int ath12k_get_peer_info(void *obj, struct agent_peer_iface_init_obj *stats);
int ath12k_get_pdev_info(void *obj, struct agent_pdev_iface_init_obj *stats);
int ath12k_get_peer_stats(void *obj, struct agent_peer_iface_stats_obj *stats);
int ath12k_get_psoc_info(void *obj, struct agent_psoc_iface_init_obj *statis);

int ath12k_sawf_get_tput_stats(void *soc, void *arg, u64 *in_bytes,
			       u64 *in_cnt, u64 *tx_bytes,
			       u64 *tx_cnt, u8 tid, u8 msduq);
int ath12k_sawf_get_mpdu_stats(void *soc, void *arg, u64 *svc_int_pass,
			       u64 *svc_int_fail, u64 *burst_pass,
			       u64 *burst_fail, u8 tid, u8 msduq);
int ath12k_sawf_get_drop_stats(void *soc, void *arg, u64 *pass,
			       u64 *drop, u64 *drop_ttl,
			       u8 tid, u8 msduq);
int ath12k_sawf_get_msduq_tx_stats(void *soc, void *arg,
				   void *msduq_tx_stats,
				   u8 msduq);
void ath12k_sawf_notify_breach(u8 *mac_addr, u8 svc_id, u8 param,
			       bool set_clear, u8 tid, u8 queue);
void *ath12k_telemetry_peer_ctx_alloc(void *peer, void *sawf_stats,
				      u8 *mac_addr,
				      u8 svc_id, u8 hostq_id);
void ath12k_telemetry_peer_ctx_free(void *telemetry_peer_ctx);
int ath12k_telemetry_update_tid_msduq(void *telemetry_peer_ctx,
				      u8 hostq_id, u8 tid, u8 msduq_idx);
int ath12k_telemetry_set_svclass_cfg(bool enable, u8 svc_id,
				     u32 min_tput_rate,
				     u32 max_tput_rate,
				     u32 burst_size,
				     u32 svc_interval,
				     u32 delay_bound,
				     u32 msdu_ttl,
				     u32 msdu_rate_loss);
int ath12k_telemetry_update_delay(void *telemetry_ctx, u8 tid,
				  u8 queue, u64 pass, u64 fail);
int ath12k_telemetry_update_delay_mvng(void *telemetry_ctx,
				       u8 tid, u8 queue,
				       u64 nwdelay_winavg,
				       u64 swdelay_winavg,
				       u64 hwdelay_winavg);
bool ath12k_telemetry_update_msdu_drop(void *telemetry_ctx, u8 tid,
				       u8 queue, u64 success,
				       u64 failure_drop,
				       u64 failure_ttl);
int ath12k_telemetry_reset_peer_stats(u8 *peer_mac);
int ath12k_telemetry_notify_vendor_app_event(u8 init, u8 id, u64 service_data);
int ath12k_telemetry_dynamic_app_init_deinit_notify(u8 init, u8 id, u64 service_data,
						    bool is_container_app);
bool ath12k_telemetry_is_agent_loaded(void);
void ath12k_telemetry_create_resources(struct ath12k_hw_group *ag);
void ath12k_telemetry_destroy_resources(struct ath12k_hw_group *ag);
int ath12k_telemetry_pdev_agent_create_handler(struct ath12k_pdev *pdev);
int ath12k_telemetry_pdev_agent_delete_handler(struct ath12k_pdev *pdev);
int ath12k_telemetry_peer_agent_create_handler(struct ath12k *ar,
					       const int vdev_id,
					       const u8 *addr);
int ath12k_telemetry_peer_agent_delete_handler(struct ath12k *ar,
					       const int vdev_id,
					       const u8 *addr);
int ath12k_telemetry_ab_peer_agent_create(struct ath12k_base *ab);
int ath12k_telemetry_ab_peer_agent_destroy(struct ath12k_base *ab);
void ath12k_telemetry_destroy_peer_agent_resources(void);

void ath12k_telemetry_vendor_callback(u8 init, u8 id, u8 category);
int ath12k_telemetry_set_mov_avg_params(u32 num_pkt, u32 num_win);
int ath12k_telemetry_set_sla_params(u32 num_pkt, u32 time_sec);
int ath12k_telemetry_set_sla_cfg(struct ath12k_sla_thershold_cfg param);
int ath12k_telemetry_set_sla_detect_cfg(struct ath12k_sla_detect_cfg param);
int ath12k_telemetry_get_rate(void *telemetry_ctx, u8 tid,
			      u8 queue, u32 *egress_rate,
			      u32 *ingress_rate);
int ath12k_telemetry_get_tx_rate(void *telemetry_ctx, u8 tid, u8 msduq,
				 u32 *min_tput, u32 *max_tput,
				 u32 *avg_tput, u32 *per,
				 u32 *retries_pct);
int ath12k_telemetry_get_mov_avg(void *telemetry_ctx, u8 tid,
				 u8 queue, u32 *nwdelay_avg,
				 u32 *swdelay_avg,
				 u32 *hwdelay_avg);
#endif /* ATH12K_TELEMETRY_AGENT_IF_H */
