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

struct agent_psoc_obj {
    void *psoc_back_pointer;
    spinlock_t psoc_lock;
    uint8_t psoc_id;
};

struct agent_pdev_obj {
    void *pdev_back_pointer;
    void *psoc_back_pointer;
    spinlock_t pdev_lock;
    uint8_t psoc_id;
    uint8_t pdev_id;
};

struct agent_peer_obj {
    void *peer_back_pointer;
    void *pdev_back_pointer;
    void *psoc_back_pointer;
    spinlock_t peer_lock;
    uint8_t psoc_id;
    uint8_t pdev_id;
    uint8_t peer_mac_addr[6];
};

enum agent_notification_event {
    AGENT_NOTIFY_EVENT_INIT,
    AGENT_NOTIFY_EVENT_DEINIT,
};

enum agent_params {
	AGENT_INVALID_PARAM,
	AGENT_SET_DEBUG_LEVEL,
};

struct t2lm_of_tids {
	enum wlan_vendor_t2lm_direction direction; /* 0-DL, 1-UL, 2-BiDi */
	uint8_t default_link_mapping;
	uint16_t t2lm_provisioned_links[NUM_TIDS]; /*Bit0 for link0, bit1 for link1 and so on*/
};

struct eht_peer_caps {
	uint32_t tx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
	uint32_t rx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
};

struct agent_peer_iface_init_obj {
	uint8_t peer_mld_mac[6];
	uint8_t peer_link_mac[6];
	uint8_t ap_mld_addr[6];
	uint8_t vdev_id;
	uint8_t bw;
	uint16_t freq;
	struct t2lm_of_tids t2lm_info[WLAN_VENDOR_T2LM_MAX_VALID_DIRECTION]; /* T2LM mapping */
	struct eht_peer_caps caps;          /* Peer capabilities */
	uint8_t ieee_link_id;
	uint16_t disabled_link_bitmap;
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
};

struct emesh_peer_iface_stats_obj {
    uint8_t peer_link_mac[6];
    uint16_t tx_airtime_consumption[WLAN_AC_MAX];
};

struct emesh_link_iface_stats_obj {
    uint8_t link_mac[6];
    uint8_t link_idle_airtime;
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
    void (*agent_notify_app_event) (enum agent_notification_event);
    void (*agent_notify_emesh_event) (enum agent_notification_event);
    int (*agent_get_psoc_info) (void *obj, struct agent_psoc_iface_init_obj *stats);
    int (*agent_get_pdev_info) (void *obj, struct agent_pdev_iface_init_obj *stats);
    int (*agent_get_peer_info) (void *obj, struct agent_peer_iface_init_obj *stats);
    int (*agent_get_psoc_stats) (void *obj);
    int (*agent_get_pdev_stats) (void *obj, struct agent_link_iface_stats_obj *stats);
    int (*agent_get_peer_stats) (void *obj, struct agent_peer_iface_stats_obj *stats);
    int (*agent_get_emesh_pdev_stats) (void *obj, struct emesh_link_iface_stats_obj *stats);
    int (*agent_get_emesh_peer_stats) (void *obj, struct emesh_peer_iface_stats_obj *stats);

    /* SAWF ops */
    void * (*sawf_alloc_peer) (void *sawf_ctx, void *sawf_stats_ctx,
                               uint8_t *mac_addr,
                               uint8_t svc_id, uint8_t hostq_id);
    void (* sawf_free_peer) (void *telemetry_sawf_ctx);
    int (* sawf_updt_queue_info) (void *telemetry_sawf_ctx,
                                  uint8_t svc_id,
                                  uint8_t tid, uint8_t msduq_idx);
    int (* sawf_updt_delay_mvng) (uint32_t num_win, uint32_t num_pkt);
    int (* sawf_updt_sla_params) (uint32_t num_pkt, uint32_t time_sec);
    int (* sawf_set_sla_cfg) (uint8_t svc_id, uint8_t min_thruput_rate,
                              uint8_t max_thruput_rate,
                              uint8_t burst_size,
                              uint8_t service_interval,
                              uint8_t delay_bound, uint8_t msdu_ttl,
                              uint8_t msdu_rate_loss);
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
                                 uint8_t param, bool set_clear, uint8_t tid);
};

void wlan_cfg80211_t2lm_app_reply_generic_response(void * gen_data,
                                                    uint8_t category);
#endif /* __TELEMETRY_AGENT_WIFI_DRIVER_IF_H__ */
