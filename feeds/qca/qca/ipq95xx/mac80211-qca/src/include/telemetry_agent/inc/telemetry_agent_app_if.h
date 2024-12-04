/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __TELEMETRY_AGENT_APP_IF_H__
#define __TELEMETRY_AGENT_APP_IF_H__

#define MAX_SOCS 5
#define MAX_PDEV_LINKS 2
#define NUM_TIDS 8
#define RFS_INIT_DATA 1
#define RFS_STATS_DATA 2
#define WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX 4
#define MAX_T2LM_INFO 2
#define WLAN_AC_MAX 4
#define MAX_PEERS 512

/*
   Buffer Format
   --------------------------------------------
   |          |                  |            |
   |  HEADER  |  Payload (stats) | Tail Data  |
   |          |                  |            |
   --------------------------------------------
 */

struct emesh_peer_stats {
    uint8_t peer_link_mac[6];
    uint16_t tx_airtime_consumption[WLAN_AC_MAX];
};

struct emesh_link_stats {
    uint8_t link_mac[6];
    uint8_t link_idle_airtime;
    int num_peers;
    struct emesh_peer_stats peer_stats[MAX_PEERS];
};

struct emesh_soc_stats {
    int num_links;
    struct emesh_link_stats link_stats[MAX_PDEV_LINKS];
};


struct emesh_relyfs_stats {
    int num_soc;
    struct emesh_soc_stats soc_stats[MAX_SOCS];
};

struct telemetry_agent_header {
	u_int32_t   start_magic_num;
	u_int32_t   stats_version;
	u_int32_t   stats_type;
	u_int32_t   payload_len;
} __attribute__ ((__packed__));

struct telemetry_emesh_buffer {
        struct telemetry_agent_header header;
        struct emesh_relyfs_stats relayfs_stats;
        u_int32_t   end_magic_num;
};

enum wlan_vendor_channel_width {
	WLAN_VENDOR_CHAN_WIDTH_INVALID = 0,
	WLAN_VENDOR_CHAN_WIDTH_20MHZ = 1,
	WLAN_VENDOR_CHAN_WIDTH_40MHZ = 2,
	WLAN_VENDOR_CHAN_WIDTH_80MHZ = 3,
	WLAN_VENDOR_CHAN_WIDTH_160MZ = 4,
	WLAN_VENDOR_CHAN_WIDTH_80_80MHZ = 5,
	WLAN_VENDOR_CHAN_WIDTH_320MHZ = 6,
};

enum t2lm_band_caps {
	T2LM_BAND_INVALID,
	T2LM_BAND_2GHz,
	T2LM_BAND_5GHz,
	T2LM_BAND_5GHz_LOW,
	T2LM_BAND_5GHz_HIGH,
	T2LM_BAND_6Ghz,
	T2LM_BAND_6Ghz_LOW,
	T2LM_BAND_6GHz_HIGH,
};

enum wlan_vendor_t2lm_direction {
    WLAN_VENDOR_T2LM_INVALID_DIRECTION = 0,
    WLAN_VENDOR_T2LM_DOWNLINK_DIRECTION = 1,
    WLAN_VENDOR_T2LM_UPLINK_DIRECTION = 2,
    WLAN_VENDOR_T2LM_BIDI_DIRECTION = 3,
    WLAN_VENDOR_T2LM_MAX_VALID_DIRECTION =
        WLAN_VENDOR_T2LM_BIDI_DIRECTION,
};

struct agent_peer_stats {
	uint8_t peer_mld_mac[6];
	uint8_t peer_link_mac[6];
	uint8_t airtime_consumption[WLAN_AC_MAX];
	uint8_t m1_stats;
	uint8_t m2_stats;
	int8_t rssi;
	int16_t eff_chan_bandwidth;
	uint16_t sla_mask; /* Uses telemetry_sawf_param for bitmask */
};

struct agent_link_stats {
	uint16_t hw_link_id;
	uint8_t link_airtime[WLAN_AC_MAX];
	uint8_t freetime;
	uint8_t available_airtime[WLAN_AC_MAX];
	uint32_t m3_stats[WLAN_AC_MAX];
	uint32_t m4_stats[WLAN_AC_MAX];
	uint16_t num_peers;
	struct agent_peer_stats peer_stats[MAX_PEERS];
};

struct agent_soc_stats {
	uint8_t soc_id;
	uint16_t num_peers;
	uint8_t num_links;
	struct agent_link_stats link_stats[MAX_PDEV_LINKS];
};

/* Periodic Stats */
struct relayfs_stats {
	uint8_t num_soc;
	struct agent_soc_stats soc_stats[MAX_SOCS];
};


/*
 * Init time interface information - complete view.
 */

struct link_map_of_tids {
	enum wlan_vendor_t2lm_direction direction; /* 0-DL, 1-UL, 2-BiDi */
	uint8_t default_link_mapping;
	uint8_t tid_present[NUM_TIDS]; /* TID present on this link */
};


struct agent_peer_init_stats {
	uint8_t mld_mac_addr[6];      /* peer MLD mac */
	uint8_t link_mac_addr[6];     /* peer MLD link mac */
	uint8_t vdev_id;              /* peer vdev id */
	uint8_t ap_mld_addr[6];       /* AP MLD mac */
	struct link_map_of_tids t2lm_info[MAX_T2LM_INFO]; /* T2LM mapping */
	enum wlan_vendor_channel_width chan_bw;
	uint16_t chan_freq;                  /* channel center frequency */
	uint32_t tx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
	uint32_t rx_mcs_nss_map[WLAN_VENDOR_EHTCAP_TXRX_MCS_NSS_IDX_MAX];
	uint8_t ieee_link_id;
	uint16_t disabled_link_bitmap;
};

struct agent_link_init_stats {
	uint16_t hw_link_id;
	/* enum t2lm_band_caps band; */
	uint16_t num_peers;
	struct agent_peer_init_stats peer_stats[MAX_PEERS];
};

struct agent_soc_init_stats {
	uint8_t soc_id;
	uint16_t num_peers;
	uint8_t num_links;
	struct agent_link_init_stats link_stats[MAX_PDEV_LINKS];
};

struct relayfs_init_stats {
	uint8_t num_soc;
	struct agent_soc_init_stats soc_stats[MAX_SOCS];
};


struct telemetry_buffer {
	struct telemetry_agent_header header;
	union {
		struct relayfs_stats periodic_stats;
		struct relayfs_init_stats init_stats;
	} u;

	u_int32_t   end_magic_num;
};

#endif /* __TELEMETRY_AGENT_APP_IF_H__ */
