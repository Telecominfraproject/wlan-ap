/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __TELEMETRY_AGENT_WIFI_DRIVER_IF_H__
#define __TELEMETRY_AGENT_WIFI_DRIVER_IF_H__

#include "telemetry_agent_app_if.h"
#define TELEMETRY_MAX_MCS (16 + 1)

#define SET_PEER_FLAG(_val, _attr) do {             \
    (_val) |= (1 << (PEER_FLAGS_##_attr##_BIT));    \
    } while (0)
#define CLEAR_PEER_FLAG(_val, _attr) do {           \
    (_val) &= ~(1 << (PEER_FLAGS_##_attr##_BIT));   \
    } while (0)
#define IS_PEER_FLAG_SET(_val, _attr) \
    ((_val >> PEER_FLAGS_##_attr##_BIT) & 0x1)

struct agent_psoc_obj {
    void *psoc_back_pointer;
    uint8_t psoc_id;
};

struct agent_pdev_obj {
    void *pdev_back_pointer;
    void *psoc_back_pointer;
    uint8_t psoc_id;
    uint8_t pdev_id;
};

struct agent_peer_obj {
    void *peer_back_pointer;
    void *pdev_back_pointer;
    void *psoc_back_pointer;
    uint8_t psoc_id;
    uint8_t pdev_id;
    uint8_t peer_mac_addr[6];
    uint16_t peer_id;
};

enum agent_notification_event {
    AGENT_NOTIFY_EVENT_INIT,
    AGENT_NOTIFY_EVENT_DEINIT,
};

enum agent_params {
	AGENT_INVALID_PARAM,
	AGENT_SET_DEBUG_LEVEL,
};

/* Bit definitions for peer_flags */
#define PEER_FLAGS_BTM_CAP_BIT 0        /* Peer BTM capability */

struct t2lm_of_tids {
	enum wlan_vendor_t2lm_direction direction; /* 0-DL, 1-UL, 2-BiDi */
	uint8_t default_link_mapping;
	uint16_t t2lm_provisioned_links[NUM_TIDS]; /*Bit0 for link0, bit1 for link1 and so on*/
};

struct eht_peer_caps {
	uint32_t tx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
	uint32_t rx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
};

struct agent_msduq_info_iface_obj {
	uint8_t is_used;
	uint8_t svc_id;
	uint8_t svc_type;
	uint8_t svc_tid;
	uint8_t svc_ac;
	uint8_t priority;
	uint32_t service_interval;
	uint32_t burst_size;
	uint32_t min_throughput;
	uint32_t delay_bound;
	uint32_t mark_metadata;
};

struct agent_peer_iface_init_obj {
	uint8_t peer_mld_mac[6];
	uint8_t peer_link_mac[6];
	uint8_t ap_mld_addr[6];
	uint8_t is_assoc_link;
	int ifindex;
	uint8_t vdev_id;
	uint8_t bw;
	uint16_t freq;
	struct t2lm_of_tids t2lm_info[WLAN_VENDOR_T2LM_MAX_VALID_DIRECTION]; /* T2LM mapping */
	struct eht_peer_caps caps;          /* Peer capabilities */
	uint8_t ieee_link_id;
	uint16_t disabled_link_bitmap;
	uint16_t peer_flags;
	uint8_t phymode;
	struct agent_msduq_info_iface_obj msduq_info[SAWF_MAX_QUEUES];
};

struct agent_pdev_iface_init_obj {
	/* This info is stroed in telemetry pdev object,
	 * so this can be ignored for now
	 */
	uint16_t link_id;
	uint8_t soc_id;
	uint8_t band;
};

struct agent_psoc_iface_init_obj {
	uint8_t soc_id;
	uint16_t max_peers;
	uint16_t num_peers;

};

struct agent_peer_iface_stats_obj {
    uint8_t peer_mld_mac[6];
    uint8_t peer_link_mac[6];
    uint8_t airtime_consumption[WLAN_AC_MAX];
    uint16_t tx_airtime_consumption[WLAN_AC_MAX];
    uint32_t tx_mpdu_retried;
    uint32_t tx_mpdu_total;
    uint32_t rx_mpdu_retried;
    uint32_t rx_mpdu_total;
    uint8_t rssi;
    uint16_t sla_mask; /* Uses telemetry_sawf_param for bitmask */
};

struct agent_link_iface_stats_obj {
    uint16_t link_id;
    uint8_t soc_id;
    uint8_t available_airtime[WLAN_AC_MAX];
    uint32_t congestion[WLAN_AC_MAX];
    uint32_t tx_mpdu_failed[WLAN_AC_MAX];
    uint32_t tx_mpdu_total[WLAN_AC_MAX];
    uint8_t link_airtime[WLAN_AC_MAX];
    uint8_t freetime;
    uint8_t obss_airtime;
    uint32_t traffic_condition[WLAN_AC_MAX];
    uint32_t error_margin[WLAN_AC_MAX];
    uint32_t num_dl_asymmetric_clients[WLAN_AC_MAX];
    uint32_t num_ul_asymmetric_clients[WLAN_AC_MAX];
    uint8_t dl_payload_ratio[WLAN_AC_MAX];
    uint8_t ul_payload_ratio[WLAN_AC_MAX];
    uint32_t avg_chan_latency[WLAN_AC_MAX];
    bool is_mon_enabled;
    uint16_t freq;
};

struct emesh_peer_iface_stats_obj {
    uint8_t peer_link_mac[6];
    uint16_t tx_airtime_consumption[WLAN_AC_MAX];
};

struct emesh_link_iface_stats_obj {
    uint8_t link_mac[6];
    uint8_t link_idle_airtime;
};

struct deter_peer_iface_stats_obj {
    /*
    uint8_t peer_link_mac[6];
    struct deter_peer_iface_stats deter[NUM_TIDS];
    uint8_t vdev_id; */
};

struct deter_link_iface_stats_obj {
    uint8_t link_mac[6];
    uint8_t hw_link_id;
    uint64_t dl_ofdma_usr[MU_MAX_USERS];
    uint64_t ul_ofdma_usr[MU_MAX_USERS];
    uint64_t dl_mimo_usr[MUMIMO_MAX_USERS];
    uint64_t ul_mimo_usr[MUMIMO_MAX_USERS];
    uint64_t dl_mode_cnt[TX_DL_MAX];
    uint64_t ul_mode_cnt[TX_UL_MAX];
    uint64_t rx_su_cnt;
    uint32_t ch_access_delay[WLAN_AC_MAX];
    struct deter_link_chan_util_stats ch_util;
    struct deter_link_ul_trigger_status ts[TX_UL_MAX];
};

struct erp_link_iface_stats_obj {
	uint64_t tx_data_msdu_cnt;
	uint64_t rx_data_msdu_cnt;
	uint64_t total_tx_data_bytes;
	uint64_t total_rx_data_bytes;
	uint8_t sta_vap_exist;
	uint64_t time_since_last_assoc;
};

struct admctrl_link_iface_stats_obj {
    uint16_t link_id;
    uint8_t freetime;
    uint8_t tx_link_airtime[WLAN_AC_MAX];
};

struct admctrl_msduq_iface_stats_obj {
    uint64_t tx_success_num;
};

struct admctrl_peer_iface_stats_obj {
    uint8_t peer_link_mac[6];
    uint8_t peer_mld_mac[6];
    uint8_t is_assoc_link;
    uint8_t tx_airtime_consumption[WLAN_AC_MAX];
    uint64_t tx_success_num;
    uint64_t mld_tx_success_num;
    uint64_t avg_tx_rate;
    struct admctrl_msduq_iface_stats_obj msduq_stats[SAWF_MAX_QUEUES];
};

enum telemetry_packet_type {
	TELEMETRY_DOT11_A = 0,
	TELEMETRY_DOT11_B = 1,
	TELEMETRY_DOT11_N = 2,
	TELEMETRY_DOT11_AC = 3,
	TELEMETRY_DOT11_AX = 4,
	TELEMETRY_DOT11_BE = 5,
	TELEMETRY_DOT11_MAX,
};

struct telemetry_pkt_type {
	uint32_t mcs_count[TELEMETRY_MAX_MCS];
};

struct telemetry_msduq_tx_stats {
	uint32_t tx_failed;
	uint32_t retry_count;
	uint32_t total_retries_count;
	struct telemetry_pkt_type packet_type[TELEMETRY_DOT11_MAX];
};

struct agent_peer_tx_ext_stats {
	uint32_t avg_ack_rssi;
	uint32_t tx_failed;
	uint32_t retries;
	uint32_t total_retries;
	uint64_t tx_cnt;
	uint64_t tx_bytes;
	struct telemetry_pkt_type packet_type[TELEMETRY_DOT11_MAX];
};

struct telemetry_agent_ops {
    int  (*agent_psoc_create_handler) (void *arg, struct agent_psoc_obj *psoc_obj);
    int  (*agent_psoc_destroy_handler) (void *arg, struct agent_psoc_obj *psoc_obj);
    int  (*agent_pdev_create_handler) (void *arg, struct agent_pdev_obj *pdev_obj);
    int  (*agent_pdev_destroy_handler) (void *arg, struct agent_pdev_obj *pdev_obj);
    int  (*agent_peer_create_handler) (void *arg, struct agent_peer_obj *peer_obj);
    int  (*agent_peer_destroy_handler) (void *arg, struct agent_peer_obj *peer_obj);
    int  (*agent_set_param) (int command, int value);
    int  (*agent_get_param) (int command);
    void (*agent_notify_app_event) (enum agent_notification_event, enum rm_services service_id,
                                    uint64_t service_data);
    void (*agent_notify_host_event) (enum agent_notification_event event,
		    		     enum rm_services service_id,
				     uint8_t category);
    void (*agent_notify_emesh_event) (enum agent_notification_event);
    void (*agent_dynamic_app_init_deinit_notify) (enum agent_notification_event,
                                    enum rm_services service_id,
                                    uint64_t service_data,
                                    bool is_container_app);
    int (*agent_get_psoc_info) (void *obj, struct agent_psoc_iface_init_obj *stats);
    int (*agent_get_pdev_info) (void *obj, struct agent_pdev_iface_init_obj *stats);
    int (*agent_get_peer_info) (void *obj, struct agent_peer_iface_init_obj *stats);
    int (*agent_get_pdev_stats) (void *obj, struct agent_link_iface_stats_obj *stats);
    int (*agent_get_peer_stats) (void *obj, struct agent_peer_iface_stats_obj *stats);
    int (*agent_get_emesh_pdev_stats) (void *obj, struct emesh_link_iface_stats_obj *stats);
    int (*agent_get_emesh_peer_stats) (void *obj, struct emesh_peer_iface_stats_obj *stats);
    int (*agent_get_deter_pdev_stats) (void *obj, struct deter_link_iface_stats_obj *stats);
    int (*agent_get_deter_peer_stats) (void *obj, struct deter_peer_iface_stats_obj *stats);
    int (*agent_get_erp_pdev_stats) (void *obj, struct erp_link_iface_stats_obj *stats);
    int (*agent_get_admctrl_pdev_stats) (void *obj, struct admctrl_link_iface_stats_obj *stats);
    int (*agent_get_admctrl_peer_stats) (void *obj, struct admctrl_peer_iface_stats_obj *stats);
    int (*agent_get_peer_tx_stats) (void *obj, void *stats);
    int (*agent_peer_sla_stats_threshold) (uint8_t *peer_mac,
					   uint32_t packet_error_rate,
					   uint32_t retries_threshold,
					   uint32_t mcs_min_threshold,
					   uint32_t mcs_max_threshold,
					   uint32_t min_thruput_rate,
					   uint32_t max_thruput_rate);
    int (*agent_get_pext_stats_enabled_flag) (void *obj, void *flag);
    int (*agent_pull_tx_peer_stats) (uint8_t *peer_mac,
				     uint32_t *min_tput,
				     uint32_t *max_tput,
				     uint32_t *avg_tput,
				     uint32_t *per,
				     uint32_t *retries_pct);

    /* SAWF ops */
    void * (*sawf_alloc_peer) (void *sawf_ctx, void *sawf_stats_ctx,
                               uint8_t *mac_addr,
                               uint8_t svc_id, uint8_t hostq_id);
    void (* sawf_free_peer) (void *telemetry_sawf_ctx);
    void (* sawf_peer_stats_reset) (void *telemetry_ctx);
    int (* sawf_updt_queue_info) (void *telemetry_sawf_ctx,
                                  uint8_t svc_id,
                                  uint8_t tid, uint8_t msduq_idx);
    int (* sawf_update_msduq_info) (void *telemetry_sawf_ctx,
                                    uint8_t hostq_id, uint8_t tid,
                                    uint8_t msduq_idx, uint8_t svc_id);
    int (* sawf_clear_msduq_info) (void *telemetry_sawf_ctx,
				   uint8_t hostq_id);
    int (* sawf_updt_delay_mvng) (uint32_t num_win, uint32_t num_pkt);
    int (* sawf_updt_sla_params) (uint32_t num_pkt, uint32_t time_sec);
    int (* sawf_set_sla_cfg) (uint8_t svc_id, uint8_t min_thruput_rate,
                              uint8_t max_thruput_rate,
                              uint8_t burst_size,
                              uint8_t service_interval,
                              uint8_t delay_bound, uint8_t msdu_ttl,
                              uint8_t msdu_rate_loss);
    int (* sawf_set_sla_config) (uint8_t svc_id, uint8_t min_thruput_rate,
                                 uint8_t max_thruput_rate,
                                 uint8_t burst_size,
                                 uint8_t service_interval,
                                 uint8_t delay_bound, uint8_t msdu_ttl,
                                 uint8_t msdu_rate_loss,
                                 uint8_t packet_error_rate,
                                 uint8_t mcs_min_threshold,
                                 uint8_t mcs_max_threshold,
                                 uint8_t retries_threshold);
    int (* sawf_set_svclass_cfg) (bool enable, uint8_t svclass_id,
                                  uint32_t min_thruput_rate,
                                  uint32_t max_thruput_rate,
                                  uint32_t burst_size,
                                  uint32_t service_interval,
                                  uint32_t delay_bound,
                                  uint32_t msdu_ttl,
                                  uint32_t msdu_rate_loss);
    int (* sawf_set_sla_dtct_cfg) (uint8_t detect_type,
                                   uint8_t min_thruput_rate,
                                   uint8_t max_thruput_rate,
                                   uint8_t burst_size,
                                   uint8_t service_interval,
                                   uint8_t delay_bound,
                                   uint8_t msdu_ttl,
                                   uint8_t msdu_rate_loss);
    int (* sawf_set_sla_detect_config) (uint8_t detect_type,
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
    int (* sawf_push_delay) (void *telemetry_sawf_ctx, uint8_t tid,
                             uint8_t queue, uint64_t pass,
                             uint64_t fail);
    int (* sawf_push_delay_mvng) (void *telemetry_ctx, uint8_t tid,
                                  uint8_t queue, uint64_t nwdelay_avg,
                                  uint64_t swdelay_avg,
                                  uint64_t hwdelay_avg);
    int (* sawf_push_msdu_drop) (void * telemetry_sawf_ctx, uint8_t tid,
                                 uint8_t queue, uint64_t pass,
                                 uint64_t fail_drop, uint64_t fail_ttl);
    int (* sawf_pull_rate) (void *telemetry_sawf_ctx, uint8_t tid,
                            uint8_t queue, uint32_t *egress_rate,
                            uint32_t *ingress_rate);
    int (* sawf_pull_tx_rate) (void *telemetry_sawf_ctx, uint8_t tid,
                               uint8_t queue,
                               uint32_t *min_tput, uint32_t *max_tput,
                               uint32_t *avg_tput, uint32_t *per,
			       uint32_t *retries_pct);
    int (* sawf_pull_mov_avg) (void *telemetry_sawf_ctx, uint8_t tid,
                               uint8_t queue, uint32_t *nwdelay_avg,
                               uint32_t *swdelay_avg, uint32_t *hwdelay_avg);
    int (* sawf_reset_peer_stats) (uint8_t *mac_addr);
    int (* sawf_get_tput_stats) (void *soc, void *arg, uint64_t *in_bytes,
                                 uint64_t *in_cnt, uint64_t *tx_bytes,
                                 uint64_t *tx_cnt, uint8_t tid,
                                 uint8_t msduq);
    int (* sawf_get_mpdu_stats) (void *soc, void *arg, uint64_t *svc_int_pass,
                                 uint64_t *svc_int_fail, uint64_t *burst_pass,
                                 uint64_t *burst_fail, uint8_t tid,
                                 uint8_t msduq);
    int (* sawf_get_drop_stats) (void *soc, void *arg, uint64_t *pass,
                                 uint64_t *drop, uint64_t *drop_ttl,
                                 uint8_t tid, uint8_t msduq);
    void (* sawf_notify_breach) (uint8_t *mac_addr, uint8_t svc_id,
                                 uint8_t param, bool set_clear, uint8_t tid,
                                 uint8_t queue_id);
    int (* sawf_get_msduq_tx_stats) (void *soc, void *arg,
                                     void *msduq_tx_stats,
                                     uint8_t msduq);
};

void wlan_cfg80211_t2lm_app_reply_generic_response(void * gen_data,
                                                    uint8_t category,
                                                    uint8_t service_id);
#endif /* __TELEMETRY_AGENT_WIFI_DRIVER_IF_H__ */
