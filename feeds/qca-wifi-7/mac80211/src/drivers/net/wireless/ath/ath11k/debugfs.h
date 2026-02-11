/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ATH11K_DEBUGFS_H_
#define _ATH11K_DEBUGFS_H_

#include "hal_tx.h"

#define ATH11K_TX_POWER_MAX_VAL	70
#define ATH11K_TX_POWER_MIN_VAL	0
#define ATH11K_DEBUG_ENABLE_MEMORY_STATS 1

#define ATH11K_MAX_NRPS 7
#define MAC_UNIT_LEN    3

/* htt_dbg_ext_stats_type */
enum ath11k_dbg_htt_ext_stats_type {
	ATH11K_DBG_HTT_EXT_STATS_RESET                      =  0,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_TX                    =  1,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_RX                    =  2,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_TX_HWQ                =  3,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_TX_SCHED              =  4,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_ERROR                 =  5,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_TQM                   =  6,
	ATH11K_DBG_HTT_EXT_STATS_TQM_CMDQ                   =  7,
	ATH11K_DBG_HTT_EXT_STATS_TX_DE_INFO                 =  8,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_TX_RATE               =  9,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_RX_RATE               =  10,
	ATH11K_DBG_HTT_EXT_STATS_PEER_INFO                  =  11,
	ATH11K_DBG_HTT_EXT_STATS_TX_SELFGEN_INFO            =  12,
	ATH11K_DBG_HTT_EXT_STATS_TX_MU_HWQ                  =  13,
	ATH11K_DBG_HTT_EXT_STATS_RING_IF_INFO               =  14,
	ATH11K_DBG_HTT_EXT_STATS_SRNG_INFO                  =  15,
	ATH11K_DBG_HTT_EXT_STATS_SFM_INFO                   =  16,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_TX_MU                 =  17,
	ATH11K_DBG_HTT_EXT_STATS_ACTIVE_PEERS_LIST          =  18,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_CCA_STATS             =  19,
	ATH11K_DBG_HTT_EXT_STATS_TWT_SESSIONS               =  20,
	ATH11K_DBG_HTT_EXT_STATS_REO_RESOURCE_STATS         =  21,
	ATH11K_DBG_HTT_EXT_STATS_TX_SOUNDING_INFO           =  22,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_OBSS_PD_STATS	    =  23,
	ATH11K_DBG_HTT_EXT_STATS_RING_BACKPRESSURE_STATS    =  24,
	ATH11K_DBG_HTT_EXT_STATS_LATENCY_PROF_STATS         =  25,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_UL_TRIG_STATS         =  26,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_UL_MUMIMO_TRIG_STATS  =  27,
	ATH11K_DBG_HTT_EXT_STATS_FSE_RX                     =  28,
	ATH11K_DBG_HTT_EXT_STATS_PEER_CTRL_PATH_TXRX_STATS  =  29,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_RX_RATE_EXT           =  30,
	ATH11K_DBG_HTT_EXT_STATS_PDEV_TX_RATE_TXBF_STATS    =  31,
	ATH11K_DBG_HTT_EXT_STATS_TXBF_OFDMA		    =  32,
	ATH11K_DBG_HTT_EXT_STA_11AX_UL_STATS                =  33,
	ATH11K_DBG_HTT_EXT_VDEV_RTT_RESP_STATS              =  34,
	ATH11K_DBG_HTT_EXT_PKTLOG_AND_HTT_RING_STATS        =  35,
	ATH11K_DBG_HTT_EXT_DLPAGER_STATS                    =  36,
	ATH11K_DBG_HTT_EXT_PHY_COUNTERS_AND_PHY_STATS	    =  37,

	/* keep this last */
	ATH11K_DBG_HTT_NUM_EXT_STATS,
};

#define ATH11K_DEBUG_DBR_ENTRIES_MAX 512

enum ath11k_dbg_dbr_event {
	ATH11K_DBG_DBR_EVENT_INVALID,
	ATH11K_DBG_DBR_EVENT_RX,
	ATH11K_DBG_DBR_EVENT_REPLENISH,
	ATH11K_DBG_DBR_EVENT_MAX,
};

struct ath11k_dbg_dbr_entry {
	u32 hp;
	u32 tp;
	u64 timestamp;
	enum ath11k_dbg_dbr_event event;
};

struct ath11k_dbg_dbr_data {
	/* protects ath11k_db_ring_debug data */
	spinlock_t lock;
	struct ath11k_dbg_dbr_entry *entries;
	u32 dbr_debug_idx;
	u32 num_ring_debug_entries;
};

struct ath11k_debug_dbr {
	struct ath11k_dbg_dbr_data dbr_dbg_data;
	struct dentry *dbr_debugfs;
	bool dbr_debug_enabled;
};

#define ATH11K_CCK_RATES			4
#define ATH11K_OFDM_RATES			8
#define AT11K_HT_RATES				8
/* VHT rates includes extra MCS. sent by FW */
#define ATH11K_VHT_RATES			12
#define ATH11K_HE_RATES				12
#define ATH11K_HE_RATES_WITH_EXTRA_MCS		14
#define ATH11K_NSS_1				1
#define ATH11K_NSS_4				4
#define ATH11K_NSS_8				8
#define TPC_STATS_WAIT_TIME			(1 * HZ)
#define MAX_TPC_PREAM_STR_LEN			7
/* Max negative power value to indicate error */
#define TPC_INVAL				-128
#define TPC_MAX					127
#define TPC_STATS_TOT_ROW			700
#define TPC_STATS_TOT_COLUMN			100
#define ATH11K_TPC_STATS_BUF_SIZE   (TPC_STATS_TOT_ROW * TPC_STATS_TOT_COLUMN)

#define ATH11K_DRV_TX_STATS_SIZE      1024

enum ath11k_debug_tpc_stats_type {
	ATH11K_DBG_TPC_STATS_SU,
	ATH11K_DBG_TPC_STATS_SU_WITH_TXBF,
	ATH11K_DBG_TPC_STATS_MU,
	ATH11K_DBG_TPC_STATS_MU_WITH_TXBF,
	/*last*/
	ATH11K_DBG_TPC_MAX_STATS,
};

enum ath11k_debug_tpc_stats_ctl_mode {
	ATH11K_TPC_STATS_CTL_MODE_CCK,
	ATH11K_TPC_STATS_CTL_MODE_OFDM,
	ATH11K_TPC_STATS_CTL_MODE_BW_20,
	ATH11K_TPC_STATS_CTL_MODE_BW_40,
	ATH11K_TPC_STATS_CTL_MODE_BW_80,
	ATH11K_TPC_STATS_CTL_MODE_BW_160,
};

struct debug_htt_stats_req {
	bool done;
	u8 pdev_id;
	u8 type;
	u8 peer_addr[ETH_ALEN];
	struct completion cmpln;
	u32 buf_len;
	u8 buf[];
};

struct ath_pktlog_hdr {
	u16 flags;
	u16 missed_cnt;
	u16 log_type;
	u16 size;
	u32 timestamp;
	u32 type_specific_data;
	u8 payload[];
};

#define ATH11K_HTT_PEER_STATS_RESET BIT(16)

#define ATH11K_HTT_STATS_BUF_SIZE (1024 * 512)
#define ATH11K_FW_STATS_BUF_SIZE (1024 * 1024)

enum ath11k_pktlog_filter {
	ATH11K_PKTLOG_RX		= 0x000000001,
	ATH11K_PKTLOG_TX		= 0x000000002,
	ATH11K_PKTLOG_RCFIND		= 0x000000004,
	ATH11K_PKTLOG_RCUPDATE		= 0x000000008,
	ATH11K_PKTLOG_EVENT_SMART_ANT	= 0x000000020,
	ATH11K_PKTLOG_EVENT_SW		= 0x000000040,
	ATH11K_PKTLOG_ANY		= 0x00000006f,
};

enum ath11k_pktlog_mode {
	ATH11K_PKTLOG_MODE_LITE = 1,
	ATH11K_PKTLOG_MODE_FULL = 2,
	ATH11K_PKTLOG_MODE_CBF_LITE = 3,
	ATH11K_PKTLOG_MODE_CBF_FULL = 4,
	ATH11K_PKTLOG_MODE_MAX = 5,
};

enum ath11k_pktlog_enum {
	ATH11K_PKTLOG_TYPE_INVALID      = 0,
	ATH11K_PKTLOG_TYPE_TX_CTRL      = 1,
	ATH11K_PKTLOG_TYPE_TX_STAT      = 2,
	ATH11K_PKTLOG_TYPE_TX_MSDU_ID   = 3,
	ATH11K_PKTLOG_TYPE_RX_STAT      = 5,
	ATH11K_PKTLOG_TYPE_RC_FIND      = 6,
	ATH11K_PKTLOG_TYPE_RC_UPDATE    = 7,
	ATH11K_PKTLOG_TYPE_TX_VIRT_ADDR = 8,
	ATH11K_PKTLOG_TYPE_RX_CBF       = 10,
	ATH11K_PKTLOG_TYPE_RX_STATBUF   = 22,
	ATH11K_PKTLOG_TYPE_PPDU_STATS   = 23,
	ATH11K_PKTLOG_TYPE_LITE_RX      = 24,
};

enum ath11k_dbg_aggr_mode {
	ATH11K_DBG_AGGR_MODE_AUTO,
	ATH11K_DBG_AGGR_MODE_MANUAL,
	ATH11K_DBG_AGGR_MODE_MAX,
};

enum ath11k_nrp_action {
	NRP_ACTION_ADD,
	NRP_ACTION_DEL,
};

enum fw_dbglog_wlan_module_id {
	WLAN_MODULE_ID_MIN = 0,
	WLAN_MODULE_INF = WLAN_MODULE_ID_MIN,
	WLAN_MODULE_WMI,
	WLAN_MODULE_STA_PWRSAVE,
	WLAN_MODULE_WHAL,
	WLAN_MODULE_COEX,
	WLAN_MODULE_ROAM,
	WLAN_MODULE_RESMGR_CHAN_MANAGER,
	WLAN_MODULE_RESMGR,
	WLAN_MODULE_VDEV_MGR,
	WLAN_MODULE_SCAN,
	WLAN_MODULE_RATECTRL,
	WLAN_MODULE_AP_PWRSAVE,
	WLAN_MODULE_BLOCKACK,
	WLAN_MODULE_MGMT_TXRX,
	WLAN_MODULE_DATA_TXRX,
	WLAN_MODULE_HTT,
	WLAN_MODULE_HOST,
	WLAN_MODULE_BEACON,
	WLAN_MODULE_OFFLOAD,
	WLAN_MODULE_WAL,
	WLAN_WAL_MODULE_DE,
	WLAN_MODULE_PCIELP,
	WLAN_MODULE_RTT,
	WLAN_MODULE_RESOURCE,
	WLAN_MODULE_DCS,
	WLAN_MODULE_CACHEMGR,
	WLAN_MODULE_ANI,
	WLAN_MODULE_P2P,
	WLAN_MODULE_CSA,
	WLAN_MODULE_NLO,
	WLAN_MODULE_CHATTER,
	WLAN_MODULE_WOW,
	WLAN_MODULE_WAL_VDEV,
	WLAN_MODULE_WAL_PDEV,
	WLAN_MODULE_TEST,
	WLAN_MODULE_STA_SMPS,
	WLAN_MODULE_SWBMISS,
	WLAN_MODULE_WMMAC,
	WLAN_MODULE_TDLS,
	WLAN_MODULE_HB,
	WLAN_MODULE_TXBF,
	WLAN_MODULE_BATCH_SCAN,
	WLAN_MODULE_THERMAL_MGR,
	WLAN_MODULE_PHYERR_DFS,
	WLAN_MODULE_RMC,
	WLAN_MODULE_STATS,
	WLAN_MODULE_NAN,
	WLAN_MODULE_IBSS_PWRSAVE,
	WLAN_MODULE_HIF_UART,
	WLAN_MODULE_LPI,
	WLAN_MODULE_EXTSCAN,
	WLAN_MODULE_UNIT_TEST,
	WLAN_MODULE_MLME,
	WLAN_MODULE_SUPPL,
	WLAN_MODULE_ERE,
	WLAN_MODULE_OCB,
	WLAN_MODULE_RSSI_MONITOR,
	WLAN_MODULE_WPM,
	WLAN_MODULE_CSS,
	WLAN_MODULE_PPS,
	WLAN_MODULE_SCAN_CH_PREDICT,
	WLAN_MODULE_MAWC,
	WLAN_MODULE_CMC_QMIC,
	WLAN_MODULE_EGAP,
	WLAN_MODULE_NAN20,
	WLAN_MODULE_QBOOST,
	WLAN_MODULE_P2P_LISTEN_OFFLOAD,
	WLAN_MODULE_HALPHY,
	WLAN_WAL_MODULE_ENQ,
	WLAN_MODULE_GNSS,
	WLAN_MODULE_WAL_MEM,
	WLAN_MODULE_SCHED_ALGO,
	WLAN_MODULE_TX,
	WLAN_MODULE_RX,
	WLAN_MODULE_WLM,
	WLAN_MODULE_RU_ALLOCATOR,
	WLAN_MODULE_11K_OFFLOAD,
	WLAN_MODULE_STA_TWT,
	WLAN_MODULE_AP_TWT,
	WLAN_MODULE_UL_OFDMA,
	WLAN_MODULE_HPCS_PULSE,
	WLAN_MODULE_DTF,
	WLAN_MODULE_QUIET_IE,
	WLAN_MODULE_SHMEM_MGR,
	WLAN_MODULE_CFIR,
	WLAN_MODULE_CODE_COVER,
	WLAN_MODULE_SHO,
	WLAN_MODULE_MLO_MGR,
	WLAN_MODULE_PEER_INIT,
	WLAN_MODULE_STA_MLO_PS,

	WLAN_MODULE_ID_MAX,
	WLAN_MODULE_ID_INVALID = WLAN_MODULE_ID_MAX,
};

enum fw_dbglog_log_level {
	ATH11K_FW_DBGLOG_ML = 0,
	ATH11K_FW_DBGLOG_VERBOSE = 0,
	ATH11K_FW_DBGLOG_INFO,
	ATH11K_FW_DBGLOG_INFO_LVL_1,
	ATH11K_FW_DBGLOG_INFO_LVL_2,
	ATH11K_FW_DBGLOG_WARN,
	ATH11K_FW_DBGLOG_ERR,
	ATH11K_FW_DBGLOG_LVL_MAX
};

struct ath11k_neighbor_peer {
	struct list_head list;
	struct completion filter_done;
	bool is_filter_on;
	int vdev_id;
	u8 addr[ETH_ALEN];
	u8 rssi;
	s64 timestamp;
	bool rssi_valid;
};

struct ath11k_fw_dbglog {
	enum wmi_debug_log_param param;
	union {
		struct {
			/* log_level values are given in enum fw_dbglog_log_level */
			u16 log_level;
			/* module_id values are given in  enum fw_dbglog_wlan_module_id */
			u16 module_id;
		};
		/* value is either log_level&module_id/vdev_id/vdev_id_bitmap/log_level
		 * according to param
		 */
		u32 value;
	};
};

void ath11k_debugfs_wbm_tx_comp_stats(struct ath11k_vif *arvif);
void ath11k_debug_aggr_size_config_init(struct ath11k_vif *arvif);
void ath11k_debugfs_wmi_ctrl_stats(struct ath11k_vif *arvif);
void ath11k_wmi_crl_path_stats_list_free(struct list_head *head);

#ifdef CPTCFG_ATH11K_DEBUGFS
#define ATH11K_MEMORY_STATS_INC(_struct, _field, _size)			\
do {									\
	if (ath11k_debug_is_memory_stats_enabled(_struct)) 		\
		atomic_add(_size, &_struct->memory_stats._field);	\
} while(0)

#define ATH11K_MEMORY_STATS_DEC(_struct, _field, _size)			\
do {									\
	if (ath11k_debug_is_memory_stats_enabled(_struct))		\
		atomic_sub(_size, &_struct->memory_stats._field);	\
} while(0)

#else
#define ATH11K_MEMORY_STATS_INC(_struct, _field, _size)
#define ATH11K_MEMORY_STATS_DEC(_struct, _field, _size)
#endif

#define ATH11K_ANI_LEVEL_MAX         30
#define ATH11K_ANI_LEVEL_MIN         -5
#define ATH11K_ANI_LEVEL_AUTO        0x80
#define ATH11K_ANI_POLL_PERIOD_MAX   3000
#define ATH11K_ANI_LISTEN_PERIOD_MAX 3000

#ifdef CPTCFG_ATH11K_DEBUGFS
int ath11k_debugfs_create(void);
void ath11k_debugfs_destroy(void);
int ath11k_debugfs_soc_create(struct ath11k_base *ab);
void ath11k_debugfs_soc_destroy(struct ath11k_base *ab);
int ath11k_debugfs_pdev_create(struct ath11k_base *ab);
void ath11k_debugfs_pdev_destroy(struct ath11k_base *ab);
int ath11k_debugfs_register(struct ath11k *ar);
void ath11k_debugfs_unregister(struct ath11k *ar);
void ath11k_debugfs_fw_stats_process(struct ath11k *ar, struct ath11k_fw_stats *stats);

void ath11k_debugfs_fw_stats_init(struct ath11k *ar);
void ath11k_init_tx_latency_stats(struct ath11k *ar);
void ath11k_smart_ant_debugfs_init(struct ath11k *ar);
ssize_t ath11k_debugfs_dump_soc_ring_bp_stats(struct ath11k_base *ab,
					      char *buf, int size);
int ath11k_debugfs_get_fw_stats(struct ath11k *ar, u32 pdev_id,
				u32 vdev_id, u32 stats_id);
void ath11k_debugfs_nrp_clean(struct ath11k *ar, const u8 *addr);
void ath11k_debugfs_nrp_cleanup_all(struct ath11k *ar);

static inline bool ath11k_debugfs_is_pktlog_lite_mode_enabled(struct ath11k *ar)
{
	return ((ar->debug.pktlog_mode == ATH11K_PKTLOG_MODE_LITE) || (ar->debug.pktlog_mode == ATH11K_PKTLOG_MODE_CBF_LITE));
}

static inline bool ath11k_debug_is_pktlog_cbf_mode_enabled(struct ath11k *ar)
{
	return (ar->debug.pktlog_mode >= ATH11K_PKTLOG_MODE_CBF_LITE);
}

static inline bool ath11k_debugfs_is_pktlog_rx_stats_enabled(struct ath11k *ar)
{
	return (!ar->debug.pktlog_peer_valid && ar->debug.pktlog_mode);
}

static inline bool ath11k_debugfs_is_pktlog_peer_valid(struct ath11k *ar, u8 *addr)
{
	return (ar->debug.pktlog_peer_valid && ar->debug.pktlog_mode &&
		ether_addr_equal(addr, ar->debug.pktlog_peer_addr));
}

static inline int ath11k_debugfs_is_extd_tx_stats_enabled(struct ath11k *ar)
{
	return ar->debug.extd_tx_stats;
}

static inline int ath11k_debugfs_is_extd_rx_stats_enabled(struct ath11k *ar)
{
	return ar->debug.extd_rx_stats;
}

static inline int ath11k_debugfs_rx_filter(struct ath11k *ar)
{
	return ar->debug.rx_filter;
}

void ath11k_debugfs_op_vif_add(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif);
void ath11k_debugfs_add_dbring_entry(struct ath11k *ar,
				     enum wmi_direct_buffer_module id,
				     enum ath11k_dbg_dbr_event event,
				     struct hal_srng *srng);

static inline int ath11k_debug_is_memory_stats_enabled(struct ath11k_base *ab)
{
	return ab->enable_memory_stats;
}

void ath11k_debugfs_dbg_mac_filter(struct ath11k_vif *arvif);
#else

static void ath11k_debugfs_dbg_mac_filter(struct ath11k_vif *arvif)
{
}
static inline int ath11k_debugfs_create(void)
{
	return 0;
}

static inline void ath11k_debugfs_destroy(void)
{
}

static inline ssize_t ath11k_debugfs_dump_soc_ring_bp_stats(struct ath11k_base *ab,
					      char *buf, int size)
{
	return 0;
}

static inline int ath11k_debugfs_soc_create(struct ath11k_base *ab)
{
	return 0;
}

static inline void ath11k_debugfs_soc_destroy(struct ath11k_base *ab)
{
}

static inline int ath11k_debugfs_pdev_create(struct ath11k_base *ab)
{
	return 0;
}

static inline void ath11k_debugfs_pdev_destroy(struct ath11k_base *ab)
{
}

static inline int ath11k_debugfs_register(struct ath11k *ar)
{
	return 0;
}

static inline void ath11k_debugfs_unregister(struct ath11k *ar)
{
}

static inline void ath11k_debugfs_fw_stats_process(struct ath11k *ar,
						   struct ath11k_fw_stats *stats)
{
}

static inline void ath11k_debugfs_fw_stats_init(struct ath11k *ar)
{
}

static inline void ath11k_init_tx_latency_stats(struct ath11k *ar)
{
}

static inline int ath11k_debugfs_is_extd_tx_stats_enabled(struct ath11k *ar)
{
	return 0;
}

static inline int ath11k_debugfs_is_extd_rx_stats_enabled(struct ath11k *ar)
{
	return 0;
}

static inline bool ath11k_debugfs_is_pktlog_lite_mode_enabled(struct ath11k *ar)
{
	return false;
}

static inline bool ath11k_debug_is_pktlog_cbf_mode_enabled(struct ath11k *ar)
{
	return false;
}

static inline bool ath11k_debugfs_is_pktlog_rx_stats_enabled(struct ath11k *ar)
{
	return false;
}

static inline bool ath11k_debugfs_is_pktlog_peer_valid(struct ath11k *ar, u8 *addr)
{
	return false;
}

static inline int ath11k_debug_is_memory_stats_enabled(struct ath11k_base *ab)
{
	return 0;
}

static inline int ath11k_debugfs_rx_filter(struct ath11k *ar)
{
	return 0;
}

static inline int ath11k_debugfs_get_fw_stats(struct ath11k *ar,
					      u32 pdev_id, u32 vdev_id, u32 stats_id)
{
	return 0;
}

static inline void
ath11k_debugfs_add_dbring_entry(struct ath11k *ar,
				enum wmi_direct_buffer_module id,
				enum ath11k_dbg_dbr_event event,
				struct hal_srng *srng)
{
}
static inline void ath11k_smart_ant_debugfs_init(struct ath11k *ar)
{
}

static inline void ath11k_debugfs_nrp_clean(struct ath11k *ar, const u8 *addr)
{
}

static inline void ath11k_debugfs_nrp_cleanup_all(struct ath11k *ar)
{
}

#endif /* CPTCFG_ATH11K_DEBUGFS*/

#ifdef CPTCFG_ATH11K_PKTLOG
void ath11k_init_pktlog(struct ath11k *ar);
void ath11k_deinit_pktlog(struct ath11k *ar);
void ath11k_htt_pktlog_process(struct ath11k *ar, u8 *data);
void ath11k_htt_ppdu_pktlog_process(struct ath11k *ar, u8 *data, u32 len);
void ath11k_rx_stats_buf_pktlog_process(struct ath11k *ar, u8 *data, u16 log_type, u32 len);
void ath11k_cbf_pktlog_process(struct ath11k *ar, u8 *data, u32 len,
			       struct htt_t2h_ppdu_stats_ind_hdr *ind_hdr,
			       struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv *tlv_hdr);

#else /* CPTCFG_ATH11K_PKTLOG */
static inline void ath11k_init_pktlog(struct ath11k *ar)
{
}

static inline void ath11k_deinit_pktlog(struct ath11k *ar)
{
}

static inline void ath11k_htt_pktlog_process(struct ath11k *ar,
					     u8 *data)
{
}

static inline void ath11k_htt_ppdu_pktlog_process(struct ath11k *ar,
						  u8 *data, u32 len)
{
}

static inline void ath11k_pktlog_rx(struct ath11k *ar, struct sk_buff_head *amsdu)
{
}
static inline void ath11k_rx_stats_buf_pktlog_process(struct ath11k *ar,
						       u8 *data, u16 log_type, u32 len)
{
}
void ath11k_cbf_pktlog_process(struct ath11k *ar, u8 *data, u32 len,
			       struct htt_t2h_ppdu_stats_ind_hdr *ind_hdr,
			       struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv *tlv_hdr)
{
}
#endif /* CONFIG_ATH11K_PKTLOG */
#endif /* _ATH11K_DEBUGFS_H_ */
