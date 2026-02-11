/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef ATH11K_NSS_H
#define ATH11K_NSS_H

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
#include <nss_api_if.h>
#include <nss_cmn.h>
#include <nss_wifi_meshmgr.h>
#endif
#include "../../../../../net/mac80211/mesh.h"

struct ath11k;
struct ath11k_base;
struct ath11k_vif;
struct ath11k_peer;
struct ath11k_ast_entry;
struct ath11k_sta;
enum ath11k_ast_entry_type;
struct hal_rx_mon_ppdu_info;
struct hal_rx_user_status;

/* NSS DBG macro is not included as part of debug enum to avoid
 * frequent changes during upgrade*/
#define ATH11K_DBG_NSS		0x20000000
#define ATH11K_DBG_NSS_WDS	0x40000000
#define ATH11K_DBG_NSS_MESH	0x80000000

/* WIFILI Supported Target Types */
#define ATH11K_WIFILI_TARGET_TYPE_UNKNOWN   0xFF
#define ATH11K_WIFILI_TARGET_TYPE_QCA8074   20
#define ATH11K_WIFILI_TARGET_TYPE_QCA8074V2 24
#define ATH11K_WIFILI_TARGET_TYPE_QCA6018   25
#define ATH11K_WIFILI_TARGET_TYPE_QCN9074   26
#define ATH11K_WIFILI_TARGET_TYPE_QCA5018   29
#define ATH11K_WIFILI_TARGET_TYPE_QCN6122   30

/* Max limit for NSS Queue */
#define ATH11K_WIFIILI_MAX_TX_PROCESSQ 1024

/* Max TX Desc limit */
#define ATH11K_WIFILI_MAX_TX_DESC	65536

/* TX Desc related info */
/*TODO : Check this again during experiments for lowmem or
 changes for platforms based on num radios supported */
#define ATH11K_WIFILI_DBDC_NUM_TX_DESC (1024 * 8)
#define ATH11K_WIFILI_DBTC_NUM_TX_DESC (1024 * 8)

// TODO Revisit these page size calc
#define WIFILI_NSS_TX_DESC_SIZE 20*4
#define WIFILI_NSS_TX_EXT_DESC_SIZE 40*4
/* Number of desc per page(12bit) should be<4096, page limit per 1024 byte is 80*3=240 */
#define WIFILI_NSS_TX_DESC_PAGE_LIMIT 240
#define WIFILI_NSS_MAX_MEM_PAGE_SIZE (WIFILI_NSS_TX_DESC_PAGE_LIMIT * 1024)
#define WIFILI_NSS_MAX_EXT_MEM_PAGE_SIZE  (WIFILI_NSS_TX_DESC_PAGE_LIMIT * 1024)
#define WIFILI_RX_DESC_POOL_WEIGHT 3

/* Status of the NSS messages sent from driver */
#define ATH11K_NSS_MSG_ACK	0
/* Timeout for waiting for response from NSS on TX msg */
#define ATH11K_NSS_MSG_TIMEOUT_MS 5000

#define ATH11K_MESH_DEFAULT_ELEMENT_TTL	31
/* Init Flags */
#define WIFILI_NSS_CCE_DISABLED 0x1
#define WIFILI_ADDTL_MEM_SEG_SET 0x000000002
#define WIFILI_MULTISOC_THREAD_MAP_ENABLE 0x10

/* ATH11K NSS PEER Info */
/* Host memory allocated for peer info storage in nss */
#define WIFILI_NSS_PEER_BYTE_SIZE NSS_WIFILI_PEER_SIZE

/* ATH11K NSS Stats */
#define ATH11K_NSS_STATS_ENABLE 1
#define ATH11K_NSS_STATS_DISABLE 0

/* TX Buf cfg range */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE 4

/* TODO : Analysis based on platform */
/* TX Limit till 64 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE0 8192
/* TX Limit till 128 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE1 8192
/* TX Limit till 256 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE2 8192
/* TX Limit > 256 clients */
#define ATH11K_NSS_RADIO_TX_LIMIT_RANGE3 8192

#define ATH11K_NSS_MAX_NUMBER_OF_PAGE 96

#define NSS_TX_TID_MAX 8

#define ATH11K_NSS_TXRX_NETDEV_STATS(txrx, vif, len, pkt_count) \
do {	\
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(vif);	\
	struct pcpu_sw_netstats *tstats;	\
	\
	if (!wdev)	\
		break;	\
	tstats = this_cpu_ptr(netdev_tstats(wdev->netdev));	\
	u64_stats_update_begin(&tstats->syncp);	\
	tstats->txrx ## _packets += pkt_count;	\
	tstats->txrx ## _bytes += len;	\
	u64_stats_update_end(&tstats->syncp);	\
} while (0)

enum ath11k_nss_vdev_cmd {
	ATH11K_NSS_WIFI_VDEV_CFG_AP_BRIDGE_CMD,
	ATH11K_NSS_WIFI_VDEV_SECURITY_TYPE_CMD,
	ATH11K_NSS_WIFI_VDEV_ENCAP_TYPE_CMD,
	ATH11K_NSS_WIFI_VDEV_DECAP_TYPE_CMD,
	ATH11K_NSS_WIFI_VDEV_CFG_WDS_BACKHAUL_CMD,
	ATH11K_NSS_WIFI_VDEV_CFG_MCBC_EXC_TO_HOST_CMD,
};

#define ATH11K_MPP_EXPIRY_TIMER_INTERVAL_MS	60 * HZ

/* Enables the MCBC exception in NSS fw, 1 = enable */
#define ATH11K_NSS_ENABLE_MCBC_EXC	1

#define WIFILI_SCHEME_ID_INVALID	-1

enum ath11k_nss_opmode {
	ATH11K_NSS_OPMODE_UNKNOWN,
	ATH11K_NSS_OPMODE_AP,
	ATH11K_NSS_OPMODE_IBSS,
	ATH11K_NSS_OPMODE_STA,
	ATH11K_NSS_OPMODE_MONITOR,
};

struct peer_stats {
	u64 last_rx;
	u64 last_ack;
	u32 tx_packets;
	u32 tx_bytes;
	u32 tx_retries;
	u32 tx_failed;
	u32 rx_packets;
	u32 rx_bytes;
	u32 rx_dropped;
	u32 last_rxdrop;
	struct rate_info rxrate;
};

enum ath11k_nss_peer_sec_type {
	PEER_SEC_TYPE_NONE,
	PEER_SEC_TYPE_WEP128,
	PEER_SEC_TYPE_WEP104,
	PEER_SEC_TYPE_WEP40,
	PEER_SEC_TYPE_TKIP,
	PEER_SEC_TYPE_TKIP_NOMIC,
	PEER_SEC_TYPE_AES_CCMP,
	PEER_SEC_TYPE_WAPI,
	PEER_SEC_TYPE_AES_CCMP_256,
	PEER_SEC_TYPE_AES_GCMP,
	PEER_SEC_TYPE_AES_GCMP_256,
	PEER_SEC_TYPES_MAX
};

/* this holds the memory allocated for nss managed peer info */
struct ath11k_nss_peer {
	uint32_t *vaddr;
	dma_addr_t paddr;
	bool ext_vdev_up;
	struct ieee80211_vif *ext_vif;
	struct peer_stats *nss_stats;
	struct completion complete;
};

struct ath11k_nss_mpath_entry {
	struct list_head list;
	u32 num_entries;
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	struct nss_wifi_mesh_path_dump_entry mpath[0];
#endif
};

struct ath11k_nss_mpp_entry {
	struct list_head list;
	u32 num_entries;
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	struct nss_wifi_mesh_proxy_path_dump_entry mpp[0];
#endif
};

/* Structure to hold the vif related info for nss offload support */
struct arvif_nss {
	/* dynamic ifnum allocated by nss driver for vif */
	int if_num;
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	/* mesh handle for mesh obj vap */
	nss_wifi_mesh_handle_t mesh_handle;
#endif
	/* Used for completion status for vdev config nss messages */
	struct completion complete;
	/* Keep the copy of encap type for nss */
	int encap;
	/* Keep the copy of decap type for nss */
	int decap;
	/* AP_VLAN vif context obtained on ext vdev register */
	void* ctx;
	/* Parent AP vif stored in case of AP_VLAN vif */
	struct ath11k_vif *ap_vif;
	/* Flag to notify if vlan arvif object is added to arvif list*/
	bool added;
	/* Flag to notify if ext vdev is up/down */
	bool ext_vdev_up;
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	/* Keep the copy of di_type for nss */
	enum nss_dynamic_interface_type di_type;
#endif
	/* WDS cfg should be done only once for ext vdev */
	bool wds_cfg_done;
	bool created;

	bool mpp_aging;
	bool mpp_dump_req;
	struct timer_list mpp_expiry_timer;
	u8 mesh_ttl;
	bool mesh_forward_enabled;
	u32 metadata_type;
	u32 mpath_refresh_time;

	struct list_head list;
	struct list_head mpath_dump;
	/* total number of mpath entries in all of the mpath_dump list */
	u32 mpath_dump_num_entries;
	struct completion dump_mpath_complete;

	struct list_head mpp_dump;
	/* total number of mpp entries in all of the mpp_dump list */
	u32 mpp_dump_num_entries;
	struct completion dump_mpp_complete;
};

/* Structure to hold the pdev/radio related info for nss offload support */
struct ath11k_nss {
	/* dynamic ifnum allocated by nss driver for pdev */
	int if_num;
	/* Radio/pdev Context obtained on pdev register */
	void* ctx;
	/* protects stats from nss */
	spinlock_t dump_lock;
};

/* Structure to hold the soc related info for nss offload support */
struct ath11k_soc_nss {
	/* turn on/off nss offload support in ath11k */
	bool enabled;
	/* turn on/off nss stats support in ath11k */
	bool stats_enabled;
	/* Mesh offload support as advertised by nss */
	bool mesh_nss_offload_enabled;
	/* soc nss ctx */
	void* ctx;
	/* if_num to be used for soc related nss messages */
	int if_num;
	/* debug mode to disable the regular mesh configuration from mac80211 */
	bool debug_mode;
	/* Completion to nss message response */
	struct completion complete;
	/* Response to nss messages are stored here on msg callback
	* used only in contention free messages during init */
	int response;
	/* Below is used for identifying allocated tx descriptors */
	dma_addr_t tx_desc_paddr[ATH11K_NSS_MAX_NUMBER_OF_PAGE];
	uint32_t * tx_desc_vaddr[ATH11K_NSS_MAX_NUMBER_OF_PAGE];
	uint32_t tx_desc_size[ATH11K_NSS_MAX_NUMBER_OF_PAGE];
};

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
int ath11k_nss_tx(struct ath11k_vif *arvif, struct sk_buff *skb);
int ath11k_nss_vdev_set_cmd(struct ath11k_vif *arvif, enum ath11k_nss_vdev_cmd cmd,
			    int val);
int ath11k_nss_vdev_create(struct ath11k_vif *arvif);
void ath11k_nss_vdev_delete(struct ath11k_vif *arvif);
int ath11k_nss_vdev_up(struct ath11k_vif *arvif);
int ath11k_nss_vdev_down(struct ath11k_vif *arvif);
int ath11k_nss_peer_delete(struct ath11k_base *ab, u32 vdev_id, const u8 *addr);
int ath11k_nss_set_peer_authorize(struct ath11k *ar, u16 peer_id);
int ath11k_nss_peer_create(struct ath11k *ar, struct ath11k_peer *peer);
void ath11k_nss_peer_stats_enable(struct ath11k *ar);
void ath11k_nss_peer_stats_disable(struct ath11k *ar);
int ath11k_nss_add_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
			    u8 *dest_mac, enum ath11k_ast_entry_type type);
int ath11k_nss_update_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
			       u8 *dest_mac);
int ath11k_nss_map_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
			    u8 *dest_mac, struct ath11k_ast_entry *ast_entry);
int ath11k_nss_del_wds_peer(struct ath11k *ar, u8 *peer_addr,
			    int peer_id, u8 *dest_mac);
int ath11k_nss_ext_vdev_cfg_wds_peer(struct ath11k_vif *arvif,
				     u8 *wds_addr, u32 wds_peer_id);
int ath11k_nss_ext_vdev_wds_4addr_allow(struct ath11k_vif *arvif,
					u32 wds_peer_id);
int ath11k_nss_ext_vdev_create(struct ath11k_vif *arvif);
int ath11k_nss_ext_vdev_configure(struct ath11k_vif *arvif);
void ath11k_nss_ext_vdev_unregister(struct ath11k_vif *arvif);
int ath11k_nss_ext_vdev_up(struct ath11k_vif *arvif);
int ath11k_nss_ext_vdev_down(struct ath11k_vif *arvif);
void ath11k_nss_ext_vdev_delete(struct ath11k_vif *arvif);
int ath11k_nss_ext_vdev_cfg_dyn_vlan(struct ath11k_vif *arvif, u16 vlan_id);
int ath11k_nss_dyn_vlan_set_group_key(struct ath11k_vif *arvif, u16 vlan_id,
				      u16 group_key);
int ath11k_nss_set_peer_sec_type(struct ath11k *ar, struct ath11k_peer *peer,
				 struct ieee80211_key_conf *key_conf);
void ath11k_nss_update_sta_stats(struct ath11k_vif *arvif,
				 struct station_info *sinfo,
				 struct ieee80211_sta *sta);
void ath11k_nss_update_sta_rxrate(struct hal_rx_mon_ppdu_info *ppdu_info,
				  struct ath11k_peer *peer,
				  struct hal_rx_user_status *user_stats);
int ath11k_nss_setup(struct ath11k_base *ab);
int ath11k_nss_teardown(struct ath11k_base *ab);
void ath11k_nss_ext_rx_stats(struct ath11k_base *ab, struct htt_rx_ring_tlv_filter *tlv_filter);
int ath11k_nss_dump_mpath_request(struct ath11k_vif *arvif);
int ath11k_nss_dump_mpp_request(struct ath11k_vif *arvif);
#ifdef CPTCFG_MAC80211_MESH
int ath11k_nss_mesh_config_path(struct ath11k *ar, struct ath11k_vif *arvif,
				enum ieee80211_mesh_path_offld_cmd cmd,
				struct ieee80211_mesh_path_offld *path);
#endif
int ath11k_nss_mesh_config_update(struct ieee80211_vif *vif, int changed);
int ath11k_nss_assoc_link_arvif_to_ifnum(struct ath11k_vif *arvif, int if_num);
int ath11k_nss_mesh_exception_flags(struct ath11k_vif *arvif,
			       struct nss_wifi_mesh_exception_flag_msg *nss_msg);
int ath11k_nss_exc_rate_config(struct ath11k_vif *arvif,
					struct nss_wifi_mesh_rate_limit_config *nss_exc_cfg);
#else
static inline int ath11k_nss_tx(struct ath11k_vif *arvif, struct sk_buff *skb)
{
	return 0;
}

static inline int ath11k_nss_vdev_set_cmd(struct ath11k_vif *arvif, enum ath11k_nss_vdev_cmd cmd,
					  int val)
{
	return 0;
}

static inline int ath11k_nss_vdev_create(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_set_peer_authorize(struct ath11k *ar, u16 peer_id)
{
	return 0;
}

static inline void ath11k_nss_vdev_delete(struct ath11k_vif *arvif)
{
}

static inline void ath11k_nss_update_sta_stats(struct ath11k_vif *arvif,
					       struct station_info *sinfo,
					       struct ieee80211_sta *sta)
{
	return;
}

static inline void ath11k_nss_update_sta_rxrate(struct hal_rx_mon_ppdu_info *ppdu_info,
						struct ath11k_peer *peer,
						struct hal_rx_user_status *user_stats)
{
	return;
}

static inline int ath11k_nss_vdev_up(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_vdev_down(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_peer_delete(struct ath11k_base *ab, u32 vdev_id,
					 const u8 *addr)
{
	return 0;
}

static inline int ath11k_nss_add_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
					  u8 *dest_mac, int type)
{
	return 0;
}

static inline int ath11k_nss_update_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
					     u8 *dest_mac)
{
	return 0;
}

static inline int ath11k_nss_map_wds_peer(struct ath11k *ar, struct ath11k_peer *peer,
					  u8 *dest_mac,
					  struct ath11k_ast_entry *ast_entry)
{
	return 0;
}

static inline int ath11k_nss_del_wds_peer(struct ath11k *ar, u8 *peer_addr,
					  int peer_id, u8 *dest_mac)
{
	return 0;
}

static inline int ath11k_nss_peer_create(struct ath11k *ar, struct ath11k_peer *peer)
{
	return 0;
}

static inline int ath11k_nss_ext_vdev_cfg_wds_peer(struct ath11k_vif *arvif,
				     u8 *wds_addr, u32 wds_peer_id)
{
	return 0;
}

static inline int ath11k_nss_ext_vdev_wds_4addr_allow(struct ath11k_vif *arvif,
					u32 wds_peer_id)
{
	return 0;
}

static inline int ath11k_nss_ext_vdev_create(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_ext_vdev_configure(struct ath11k_vif *arvif)
{
	return 0;
}

static inline void ath11k_nss_ext_vdev_unregister(struct ath11k_vif *arvif)
{
	return;
}

static inline int ath11k_nss_ext_vdev_up(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_ext_vdev_down(struct ath11k_vif *arvif)
{
	return 0;
}

static inline int ath11k_nss_ext_vdev_cfg_dyn_vlan(struct ath11k_vif *arvif,
						   u16 vlan_id)
{
	return 0;
}

static inline int ath11k_nss_dyn_vlan_set_group_key(struct ath11k_vif *arvif,
						    u16 vlan_id, u16 group_key)
{
	return 0;
}

static inline void ath11k_nss_peer_stats_enable(struct ath11k *ar)
{
	return;
}

static inline void ath11k_nss_peer_stats_disable(struct ath11k *ar)
{
	return;
}

static inline int ath11k_nss_set_peer_sec_type(struct ath11k *ar, struct ath11k_peer *peer,
					       struct ieee80211_key_conf *key_conf)
{
	return 0;
}

static inline int ath11k_nss_setup(struct ath11k_base *ab)
{
	return 0;
}

static inline void ath11k_nss_ext_vdev_delete(struct ath11k_vif *arvif)
{
	return;
}

static inline int ath11k_nss_teardown(struct ath11k_base *ab)
{
	return 0;
}

static inline void ath11k_nss_ext_rx_stats(struct ath11k_base *ab,
					   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	return;
}

#ifdef CPTCFG_MAC80211_MESH
static inline int
ath11k_nss_mesh_config_path(struct ath11k *ar, struct ath11k_vif *arvif,
			    enum ieee80211_mesh_path_offld_cmd cmd,
			    struct ieee80211_mesh_path_offld *path)
{
	return 0;
}
#endif
static inline int
ath11k_nss_mesh_config_update(struct ieee80211_vif *vif, int changed)
{
	return 0;
}

static inline int ath11k_nss_assoc_link_arvif_to_ifnum(struct ath11k_vif *arvif,
						       int if_num)
{
	return 0;
}

static inline int ath11k_nss_mesh_exception_flags(struct ath11k_vif *arvif,
						  void *nss_msg)
{
	return 0;
}

static inline int
ath11k_nss_exc_rate_config(struct ath11k_vif *arvif, void *nss_exc_cfg)
{
	return 0;
}
#endif /* CPTCFG_ATH11K_NSS_SUPPORT */
#endif
