/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_H
#define ATH12K_DP_H

#include "core.h"
#include "hw.h"
#include "dp_htt.h"
#include "dp_cmn.h"
#include "hal.h"
#include "ppe.h"
#include <linux/rhashtable.h>
#include "dp_stats.h"

#define HTT_TCL_META_DATA_PEER_ID_MISSION       GENMASK(15, 3)

/* QoS meta data */
#define HTT_TCL_META_DATA_TYPE_SVC_ID_BASED	2
#define HTT_TCL_META_DATA_SAWF_SVC_ID		GENMASK(10, 3)
#define HTT_TCL_META_DATA_SAWF_TID_OVERRIDE	BIT(12)

#define HTT_TCL_META_DATA_TYPE_MISSION		GENMASK(1, 0)

#define MAX_RXDMA_PER_PDEV     2

#define MAX_NAPI_BUDGET             128
#define TX_STATUS_ENTRY_MAX_SIZE    64
#define TX_STATUS_BUFFER_SIZE       (TX_STATUS_ENTRY_MAX_SIZE * MAX_NAPI_BUDGET)

#define RX_STATUS_ENTRY_MAX_SIZE    64
#define RX_STATUS_BUFFER_SIZE       (RX_STATUS_ENTRY_MAX_SIZE * MAX_NAPI_BUDGET)

#define TX_NAPI_BUDGET             127

extern struct ath12k_ppeds_desc_params ath12k_ppeds_desc_params;

struct ath12k_base;
struct ath12k_dp_link_peer;
struct ath12k_dp;
struct ath12k_vif;
struct ath12k_link_vif;
struct hal_tcl_status_ring;
struct ath12k_ext_irq_grp;
struct ath12k_dp_rx_tid;
struct ath12k_hal_reo_cmd;
struct hal_reo_dest_ring;
enum hal_wbm_rel_bm_act;
struct dp_rx_fst;
struct ath12k_dp_mon;
struct ath12k_pdev_mon_dp;

#define DP_MON_PURGE_TIMEOUT_MS     100
#define DP_MON_SERVICE_BUDGET       128
#define MAX_TCL_RING		    4

struct dp_rxdma_ring {
	struct dp_srng refill_buf_ring;
	int bufs_max;
};

#define ATH12K_TX_COMPL_NEXT(x)	(((x) + 1) % DP_TX_COMP_RING_SIZE)

struct dp_tx_ring {
	u8 tcl_data_ring_id;
	struct dp_srng tcl_data_ring;
	struct dp_srng tcl_comp_ring;
	struct hal_wbm_completion_ring_tx *tx_status;
	int tx_status_head;
	int tx_status_tail;
};

struct dp_link_desc_bank {
	void *vaddr_unaligned;
	void *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	u32 size;
};

/* Size to enforce scatter idle list mode */
#define DP_LINK_DESC_ALLOC_SIZE_THRESH 0x200000
#define DP_LINK_DESC_BANKS_MAX 8

#define DP_LINK_DESC_START	0x4000
#define DP_LINK_DESC_SHIFT	3

#define DP_LINK_DESC_COOKIE_SET(id, page) \
	((((id) + DP_LINK_DESC_START) << DP_LINK_DESC_SHIFT) | (page))

#define DP_LINK_DESC_BANK_MASK	GENMASK(2, 0)

#define DP_RX_DESC_COOKIE_INDEX_MAX		0x3ffff
#define DP_RX_DESC_COOKIE_POOL_ID_MAX		0x1c0000
#define DP_RX_DESC_COOKIE_MAX	\
	(DP_RX_DESC_COOKIE_INDEX_MAX | DP_RX_DESC_COOKIE_POOL_ID_MAX)
#define DP_NOT_PPDU_ID_WRAP_AROUND 20000

enum ath12k_dp_ppdu_state {
	DP_PPDU_STATUS_START,
	DP_PPDU_STATUS_DONE,
};

struct ath12k_wmm_stats {
       int tx_type;
       int rx_type;
       u64 total_wmm_tx_pkts[WME_NUM_AC];
       u64 total_wmm_rx_pkts[WME_NUM_AC];
       u64 total_wmm_tx_drop[WME_NUM_AC];
       u64 total_wmm_rx_drop[WME_NUM_AC];
};

struct ath12k_pdev_telemetry_stats {
       u32 link_airtime[WLAN_MAX_AC];
       u32 tx_link_airtime[WLAN_MAX_AC];
       u32 rx_link_airtime[WLAN_MAX_AC];
	u64 tx_data_msdu_cnt;
	u64 total_tx_data_bytes;
	u64 rx_data_msdu_cnt;
	u64 total_rx_data_bytes;
	u64 time_last_assoc;
	u8 sta_vap_exist;
};

struct ath12k_atf_pdev_airtime {
	u32 tx_airtime_consumption[WME_NUM_AC];
	u32 rx_airtime_consumption[WME_NUM_AC];
};

struct ath12k_pdev_dp_stats {
	struct ath12k_pdev_telemetry_stats telemetry_stats;
	struct ath12k_atf_pdev_airtime atf_airtime;
	/* Add other new stats if required */
};

struct ath12k_pdev_dp {
	u32 mac_id;
	atomic_t num_tx_pending;
	wait_queue_head_t tx_empty_waitq;

	struct ath12k_dp *dp;
	struct ieee80211_hw *hw;

	/*Debug mask to set different level of stats*/
	u32 dp_stats_mask;

	u8 hw_link_id;
	struct ath12k *ar;
	struct ath12k_dp_hw *dp_hw;

	/* Protects ppdu stats */
	spinlock_t ppdu_list_lock;
	struct ath12k_per_peer_tx_stats peer_tx_stats;
	struct list_head ppdu_stats_info;
	u32 ppdu_stat_list_depth;

	bool dp_mon_pdev_configured;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_wmm_stats wmm_stats;
	/* Protected by ab: base lock
	 * determine when this stats is calculated based on peers
	 */
	struct ath12k_pdev_dp_stats stats;
};

#define EAPOL_WPA_KEY_INFO_KEY_TYPE		BIT(3)
#define EAPOL_WPA_KEY_INFO_ACK			BIT(7)
#define EAPOL_WPA_KEY_INFO_MIC			BIT(8)
#define EAPOL_WPA_KEY_INFO_ENCR_KEY_DATA	BIT(12) /* IEEE 802.11i/RSN only */

#define EAPOL_PACKET_TYPE_OFFSET                1
#define EAPOL_KEY_INFO_OFFSET                   5
#define EAPOL_KEY_DATA_LENGTH_OFFSET            97
#define EAPOL_WPA_KEY_NONCE_OFFSET              17
#define EAPOL_PACKET_TYPE_KEY                   3
#define LLC_SNAP_HDR_LEN			8

enum ath12k_dp_eapol_key_type {
	DP_EAPOL_KEY_TYPE_M1 = 1,
	DP_EAPOL_KEY_TYPE_M2,
	DP_EAPOL_KEY_TYPE_M3,
	DP_EAPOL_KEY_TYPE_M4,
	DP_EAPOL_KEY_TYPE_G1,
	DP_EAPOL_KEY_TYPE_G2,

	DP_EAPOL_KEY_TYPE_MAX,
};

#define DP_NUM_CLIENTS_MAX 64
#define DP_AVG_TIDS_PER_CLIENT 2
#define DP_NUM_TIDS_MAX (DP_NUM_CLIENTS_MAX * DP_AVG_TIDS_PER_CLIENT)
#define DP_AVG_MSDUS_PER_FLOW 128
#define DP_AVG_FLOWS_PER_TID 2
#define DP_AVG_MPDUS_PER_TID_MAX 128
#define DP_AVG_MSDUS_PER_MPDU 4

#define DP_RX_HASH_ENABLE	1 /* Enable hash based Rx steering */

#define DP_BA_WIN_SZ_MAX	1024

#define DP_IDLE_SCATTER_BUFS_MAX 16

#if defined(CONFIG_ATH12K_MEM_PROFILE_512M) || defined (CPTCFG_ATH12K_MEM_PROFILE_512M)
#define DP_TX_COMP_RING_SIZE           16384
#define ATH12K_NUM_POOL_TX_DESC        16384
#define DP_REO2PPE_RING_SIZE	2048
#define DP_PPE2TCL_RING_SIZE	2048
#define DP_PPE_WBM2SW_RING_SIZE	8192
#define DP_RXDMA_BUF_RING_SIZE		4096
/* TODO: revisit this count during testing */
#define ATH12K_RX_DESC_COUNT           (8192)
#define DP_RX_BUFFER_SIZE		1856
#else
//#ifdef CONFIG_ATH12K_MEM_PROFILE_DEFAULT TODO Fix the Default profile enablement
#define DP_TX_COMP_RING_SIZE           32768
#define ATH12K_NUM_POOL_TX_DESC                32768
#define DP_REO2PPE_RING_SIZE	16384
#define DP_PPE2TCL_RING_SIZE	8192
#define DP_PPE_WBM2SW_RING_SIZE	32768
#define DP_RXDMA_BUF_RING_SIZE		8192
/* TODO: revisit this count during testing */
#define ATH12K_RX_DESC_COUNT           (12288)
#define DP_RX_BUFFER_SIZE		2048
#endif

#define ATH12K_NUM_EAPOL_RESERVE       1024
#define ATH12K_DP_PDEV_TX_LIMIT        ATH12K_NUM_POOL_TX_DESC

#define DP_WBM_RELEASE_RING_SIZE	64
#define DP_TCL_DATA_RING_SIZE		2048
#define DP_TX_IDR_SIZE			DP_TX_COMP_RING_SIZE
#define DP_TCL_CMD_RING_SIZE		32
#define DP_TCL_STATUS_RING_SIZE		32
#define DP_REO_DST_RING_SIZE		8192
#define DP_REO_REINJECT_RING_SIZE	32
#define DP_REO_EXCEPTION_RING_SIZE	128
#define DP_REO_CMD_RING_SIZE		256
#define DP_REO_STATUS_RING_SIZE		2048
#define DP_RX_MAC_BUF_RING_SIZE		2048
#define DP_RXDMA_REFILL_RING_SIZE	2048
#define DP_RXDMA_ERR_DST_RING_SIZE	1024

#if defined(CONFIG_ATH12K_MEM_PROFILE_512M) || defined (CPTCFG_ATH12K_MEM_PROFILE_512M)
#define DP_RX_RELEASE_RING_SIZE		8192
#else
#define DP_RX_RELEASE_RING_SIZE		16384
#endif

#define DP_RX_BUFFER_SIZE_LITE	1024
#define DP_RX_BUFFER_ALIGN_SIZE	128

#define HAL_REO2PPE_DST_IND 6

#define DP_RXDMA_BUF_COOKIE_BUF_ID	GENMASK(17, 0)
#define DP_RXDMA_BUF_COOKIE_PDEV_ID	GENMASK(19, 18)

#define DP_HW2SW_MACID(mac_id) ({ typeof(mac_id) x = (mac_id); x ? x - 1 : 0; })
#define DP_SW2HW_MACID(mac_id) ((mac_id) + 1)

#define DP_TX_DESC_ID_MAC_ID  GENMASK(1, 0)
#define DP_TX_DESC_ID_MSDU_ID GENMASK(18, 2)
#define DP_TX_DESC_ID_POOL_ID GENMASK(20, 19)

#define ATH12K_SHADOW_DP_TIMER_INTERVAL 20
#define ATH12K_SHADOW_CTRL_TIMER_INTERVAL 10

#define ATH12K_PAGE_SIZE	PAGE_SIZE

/* Total 1024 entries in PPT, i.e 4K/4 considering 4K aligned
 * SPT pages which makes lower 12bits 0
 */
#define ATH12K_MAX_PPT_ENTRIES	1024

/* Total 512 entries in a SPT, i.e 4K Page/8 */
#define ATH12K_MAX_SPT_ENTRIES	512

#define ATH12K_NUM_RX_SPT_PAGES	((ATH12K_RX_DESC_COUNT) / ATH12K_MAX_SPT_ENTRIES)

#define ATH12K_TX_SPT_PAGES_PER_POOL (ATH12K_NUM_POOL_TX_DESC / \
					  ATH12K_MAX_SPT_ENTRIES)
#define ATH12K_NUM_TX_SPT_PAGES	(ATH12K_TX_SPT_PAGES_PER_POOL * ATH12K_HW_MAX_QUEUES)

#define ATH12K_PPEDS_TX_SPT_PAGE_OFFSET 0
#define ATH12K_TX_SPT_PAGE_OFFSET ATH12K_NUM_PPEDS_TX_SPT_PAGES
#define ATH12K_RX_SPT_PAGE_OFFSET (ATH12K_NUM_PPEDS_TX_SPT_PAGES + ATH12K_NUM_TX_SPT_PAGES)

#define ATH12K_NUM_PPEDS_TX_SPT_PAGES (ath12k_ppeds_desc_params.num_ppeds_desc / \
					    ATH12K_MAX_SPT_ENTRIES)

#define ATH12K_NUM_SPT_PAGES	(ATH12K_NUM_TX_SPT_PAGES + ATH12K_NUM_RX_SPT_PAGES + \
				 ATH12K_NUM_PPEDS_TX_SPT_PAGES)

/* The SPT pages are divided for RX and TX, first block for RX
 * and remaining for TX
 */
#define ATH12K_NUM_TX_SPT_PAGE_START ATH12K_NUM_RX_SPT_PAGES

#define ATH12K_DP_RX_DESC_MAGIC	0xBABABABA

/* 4K aligned address have last 12 bits set to 0, this check is done
 * so that two spt pages address can be stored per 8bytes
 * of CMEM (PPT)
 */
#define ATH12K_SPT_4K_ALIGN_CHECK 0xFFF
#define ATH12K_SPT_4K_ALIGN_OFFSET 12
#define ATH12K_PPT_ADDR_OFFSET(ppt_index) (4 * (ppt_index))

/* To indicate HW of CMEM address, b0-31 are cmem base received via QMI */
#define ATH12K_CMEM_ADDR_MSB 0x10

/* Of 20 bits cookie, b0-b8 is to indicate SPT offset and b9-19 for PPT */
#define ATH12K_CC_SPT_MSB 8
#define ATH12K_CC_PPT_MSB 19
#define ATH12K_CC_PPT_SHIFT 9
#define ATH12K_DP_CC_COOKIE_SPT	GENMASK(8, 0)
#define ATH12K_DP_CC_COOKIE_PPT	GENMASK(19, 9)

#define DP_REO_QREF_NUM		GENMASK(31, 16)
#define DP_MAX_PEER_ID		2047
#define ATH12K_PEER_ID_INVALID	0x3FFF

#define DP_TCL_ENCAP_TYPE_MAX	4

/* Total size of the LUT is based on 2K peers, each having reference
 * for 17tids, note each entry is of type ath12k_reo_queue_ref
 * hence total size is 2048 * 17 * 8 = 278528
 */
#define DP_REOQ_LUT_SIZE	278528

/* Invalid TX Bank ID value */
#define DP_INVALID_BANK_ID -1

/* Tx Desc Flag bit definations */
#define DP_TX_DESC_FLAG_FAST	0x1
#define DP_TX_DESC_FLAG_MCAST	0x2
#define DP_TX_DESC_FLAG_BCAST	0x4

#define MAX_TQM_RELEASE_REASON 15
#define MAX_FW_TX_STATUS 7
#define MAX_TCL_RING 4
#define MAX_TX_COMP_RING 4

struct ath12k_dp_tx_bank_profile {
	u8 is_configured;
	u32 num_users;
	u32 bank_config;
};

struct ath12k_hp_update_timer {
	struct timer_list timer;
	bool started;
	bool init;
	u32 tx_num;
	u32 timer_tx_num;
	u32 ring_id;
	u32 interval;
	struct ath12k_base *ab;
};

struct ath12k_rx_desc_info {
	struct list_head list;
	dma_addr_t paddr;
	u8 *vaddr;
	u32 cookie;
	u8 in_use	: 1,
	   device_id	: 3,
	   is_frag	: 1,
	   reserved	: 3;
	struct sk_buff *skb;
	u32 magic;
	u64 rsvd0;
};

struct ath12k_tx_desc_info {
	struct list_head list;
	struct sk_buff *skb;
	struct sk_buff *skb_ext_desc;
	dma_addr_t paddr;
	dma_addr_t paddr_ext_desc;
	u32 desc_id; /* Cookie */
	u16 len;
	u16 ext_desc_len;
	u8 mac_id	: 5,
	   in_use	: 1,
	   reserved	: 2;
	u8 flags	: 3,
	   reserved1	: 5;
	u8 pool_id;
};

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
struct ath12k_ppeds_tx_desc_info {
	union {
		u8 align[64];
		struct {
			struct list_head list;
			struct sk_buff *skb;
			dma_addr_t paddr;
			u32 desc_id; /* Cookie */
			bool in_use;
			u8 mac_id;
			u8 pool_id;
			u8 flags;
		};
	};
};
#endif

struct ath12k_spt_info {
	dma_addr_t paddr;
	u64 *vaddr;
};

struct ath12k_reo_queue_ref {
	u32 info0;
	u32 info1;
} __packed;

struct ath12k_reo_q_addr_lut {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	u32 size;
};

struct ath12k_link_stats {
	u32 tx_enqueued;
	u32 tx_completed;
	u32 tx_bcast_mcast;
	u32 tx_dropped;
	u32 tx_errors;
	u32 rx_errors;
	u32 rx_dropped;
	u32 tx_encap_type[DP_TCL_ENCAP_TYPE_MAX];
	u32 tx_encrypt_type[HAL_ENCRYPT_TYPE_MAX];
	u32 tx_desc_type[DP_TCL_DESC_TYPE_MAX];
};

struct rx_flow_info {
	struct hal_flow_tuple_info flow_tuple_info;
	u16 fse_metadata;
	u8 ring_id;
	u8 is_addr_ipv4	:1,
	   use_ppe	:1,
	   drop		:1;
};

/* DP arch ops to communicate from common module
 * to arch specific module
 */
struct ath12k_dp_arch_ops {
	int (*dp_op_device_init)(struct ath12k_dp *dp);
	void (*dp_op_device_deinit)(struct ath12k_dp *dp);
	u32 (*dp_tx_get_vdev_bank_config)(struct ath12k_base *ab,
					  struct ath12k_link_vif *arvif, bool vdev_id_check_en);
	int (*dp_reo_cmd_send)(struct ath12k_base *ab,
			       struct ath12k_dp_rx_tid *rx_tid,
			       enum hal_reo_cmd_type type,
			       struct ath12k_hal_reo_cmd *cmd,
			       void (*cb)(struct ath12k_dp *dp, void *ctx,
				          enum hal_reo_cmd_status status));
	void (*setup_pn_check_reo_cmd)(struct ath12k_hal_reo_cmd *cmd,
				       struct ath12k_dp_rx_tid *rx_tid,
				       u32 cipher, enum set_key_cmd key_cmd);
	void (*rx_peer_tid_delete)(struct ath12k *ar,
				   struct ath12k_dp_link_peer *peer, u8 tid);
	int (*reo_cache_flush)(struct ath12k_base *ab,
				struct ath12k_dp_rx_tid *rx_tid);
	int (*rx_link_desc_return)(struct ath12k_dp *dp,
				   struct ath12k_buffer_addr *buf_addr_info,
				   enum hal_wbm_rel_bm_act action);
	int (*peer_rx_tid_reo_update)(struct ath12k *ar,
				      struct ath12k_dp_link_peer *peer,
				      struct ath12k_dp_rx_tid *rx_tid,
				      u32 ba_win_sz, u16 ssn,
				      bool update_ssn);
	int (*alloc_reo_qdesc)(struct ath12k_base *ab,
			       struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
			       enum hal_pn_type pn_type,
			       struct hal_rx_reo_queue **addr_aligned);
	void (*peer_rx_tid_qref_setup)(struct ath12k_base *ab, u16 peer_id, u16 tid,
				       dma_addr_t paddr);
	int (*dp_pdev_alloc)(struct ath12k_base *ab);
	void (*dp_pdev_free)(struct ath12k_base *ab);
	int (*rx_fst_attach)(struct ath12k_dp *dp, struct dp_rx_fst *fst);
	void (*rx_fst_detach)(struct ath12k_dp *dp, struct dp_rx_fst *fst);
	void (*rx_flow_dump_entry)(struct ath12k_dp *dp,
				   struct rx_flow_info *flow_info);
	int (*rx_flow_add_entry)(struct ath12k_dp *dp, struct rx_flow_info *flow_info);
	int (*rx_flow_delete_entry)(struct ath12k_dp *dp, struct rx_flow_info *flow_info);
	int (*rx_flow_delete_all_entries)(struct ath12k_dp *dp);
	ssize_t (*dump_fst_table)(struct ath12k_dp *dp, char *buf, int size);
	struct ath12k_dp_hw_group*(*dp_hw_group_alloc)(void);
	int (*peer_migrate_reo_cmd)(struct ath12k_dp *dp,
				    struct ath12k_dp_link_peer *peer,
				    u16 peer_id,
				    u8 chip_id);
	int (*sdwf_reinject_handler)(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_link_vif *arvif,
				     struct sk_buff *skb, struct ath12k_link_sta *arsta);
	int (*dp_tx_ring_setup)(struct ath12k_base *ab);
	void (*dp_tx_status_parse)(struct ath12k_base *ab,
				   struct hal_wbm_completion_ring_tx *desc,
				   struct hal_tx_status *ts);
};

struct ath12k_bp_stats {
       /* Head Pointer reported by the last HTT Backpressure event for the ring */
       u16 hp;

       /* Tail Pointer reported by the last HTT Backpressure event for the ring */
       u16 tp;

       /* Number of Backpressure events received for the ring */
       u32 count;

       /* Last recorded event timestamp */
       unsigned long jiffies;
};

struct ath12k_dp_ring_bp_stats {
       struct ath12k_bp_stats umac_ring_bp_stats[HTT_SW_UMAC_RING_IDX_MAX];
       struct ath12k_bp_stats lmac_ring_bp_stats[HTT_SW_LMAC_RING_IDX_MAX][MAX_RADIOS];
};

struct ath12k_device_dp_tx_err_stats {
	/* TCL Ring Descriptor unavailable */
	u32 desc_na[DP_TCL_NUM_RING_MAX];
	/* TCL Ring Buffers unavailable */
	u32 txbuf_na[DP_TCL_NUM_RING_MAX];

	u32 threshold_limit;

	/* Other failures during dp_tx due to mem allocation failure
	 * idr unavailable etc.
	 */
	atomic_t misc_fail;
	u32 tx_comp_err[DP_TX_COMP_ERR_MAX][DP_TCL_NUM_RING_MAX];
};

struct ath12k_device_dp_rx_err_stats {
	u32 rx_err[DP_RX_ERR_MAX][DP_REO_DST_RING_MAX];
};

struct ath12k_device_dp_rx_wbm_err_stats {
	u32 rxdma_error[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX];
	u32 reo_error[HAL_REO_DEST_RING_ERROR_CODE_MAX];
	u32 drop[WBM_ERR_DROP_MAX];
};

struct ath12k_tx_comp_stats {
	u32 tx_completed;
	u32 tx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX];
	u32 tqm_rel_reason[MAX_TQM_RELEASE_REASON];
	u32 fw_tx_status[MAX_FW_TX_STATUS];
};

struct ath12k_device_dp_stats {
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	u32 ppe_vp_mode_update_fail;
#endif
	u32 tx_fast_unicast[MAX_TCL_RING];
	struct ath12k_tx_comp_stats tx_comp_stats[MAX_TX_COMP_RING];
	u32 err_ring_pkts;
	u32 invalid_rbm;
	u32 reo_excep_msdu_buf_type;
	u32 free_excess_alloc_skb;
	u32 hal_reo_error[DP_REO_DST_RING_MAX];
	u32 rx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX] [ATH12K_MAX_SOCS];
	u32 reo_rx[DP_REO_DST_RING_MAX] [ATH12K_MAX_SOCS];
	u32 non_fast_unicast_rx[DP_REO_DST_RING_MAX][ATH12K_MAX_SOCS];
	u32 non_fast_mcast_rx[DP_REO_DST_RING_MAX][ATH12K_MAX_SOCS];
	u32 rx_eapol[ATH12K_MAX_SOCS];
	u32 rx_eapol_type[DP_EAPOL_KEY_TYPE_MAX][ATH12K_MAX_SOCS];
	u32 first_and_last_msdu_bit_miss;
	u32 fast_rx[DP_REO_DST_RING_MAX] [ATH12K_MAX_SOCS];
	struct ath12k_device_dp_tx_err_stats tx_err;
	struct ath12k_device_dp_rx_err_stats rx;
	struct ath12k_dp_ring_bp_stats bp_stats;
	struct ath12k_device_dp_rx_wbm_err_stats wbm_err;
	u32 tx_mcast[MAX_TCL_RING];
	u32 tx_unicast[MAX_TCL_RING];
	u32 tx_eapol[MAX_TCL_RING];
	u32 tx_eapol_type[DP_EAPOL_KEY_TYPE_MAX][MAX_TCL_RING];
	u32 tx_null_frame[MAX_TCL_RING];
	u32 rx_pkt_null_frame_dropped;
	u32 rx_pkt_null_frame_handled;
};

#define ATH12K_DP_MIN_FST_CORE_MASK 0x1
#define ATH12K_DP_MAX_FST_CORE_MASK 0xf

struct dp_fst_config {
	u32 fst_core_mask;
	u8 fst_core_map[4];
	u8 fst_num_cores;
	u8 core_idx;
};

/**
 * struct ath12k_dp_htt_rxdma_ppe_cfg_param - Rx DMA and RxOLE PPE config
 * @override: RxDMA override to override the reo_destinatoin_indication
 * @reo_dst_ind: REO destination indication value
 * @multi_buffer_msdu_override_en: Override the indication for SG
 * @intra_bss_override: Rx OLE IntraBSS override
 * @decap_raw_override: Rx Decap Raw override
 * @decap_nwifi_override: Rx Native override
 * @ip_frag_override: IP fragments override
 */
struct ath12k_dp_htt_rxdma_ppe_cfg_param {
	u8 override;
	u8 reo_dst_ind;
	u8 multi_buffer_msdu_override_en;
	u8 intra_bss_override;
	u8 decap_raw_override;
	u8 decap_nwifi_override;
	u8 ip_frag_override;
};

struct ath12k_dp {
	struct ath12k_base *ab;
	u8 num_bank_profiles;
	/* protects the access and update of bank_profiles */
	spinlock_t tx_bank_lock;
	struct ath12k_dp_tx_bank_profile *bank_profiles;
	enum ath12k_htc_ep_id eid;
	struct completion htt_tgt_version_received;
	u8 htt_tgt_ver_major;
	u8 htt_tgt_ver_minor;
	struct dp_link_desc_bank link_desc_banks[DP_LINK_DESC_BANKS_MAX];
	enum hal_rx_buf_return_buf_manager idle_link_rbm;
	struct dp_srng wbm_idle_ring;
	struct dp_srng wbm_desc_rel_ring;
	struct dp_srng reo_reinject_ring;
	struct dp_srng rx_rel_ring;
	struct dp_srng reo_except_ring;
	struct dp_srng reo_cmd_ring;
	struct dp_srng reo_status_ring;
	enum ath12k_peer_metadata_version peer_metadata_ver;
	struct dp_srng reo_dst_ring[DP_REO_DST_RING_MAX];
	struct dp_tx_ring tx_ring[DP_TCL_NUM_RING_MAX];
	struct ath12k_device_dp_stats device_stats;
	struct wbm_idle_scatter_list scatter_list[DP_IDLE_SCATTER_BUFS_MAX];
	struct list_head reo_cmd_list;
	struct list_head reo_cmd_cache_flush_list;
	u32 reo_cmd_cache_flush_count;

	/* protects access to below fields,
	 * - reo_cmd_list
	 * - reo_cmd_cache_flush_list
	 * - reo_cmd_cache_flush_count
	 */
	spinlock_t reo_cmd_lock;
	struct list_head reo_cmd_update_rx_queue_list;
	/* protects access to below field,
	 * - reo_cmd_update_rx_queue_list
	 */
	spinlock_t reo_cmd_update_rx_queue_lock;
	struct ath12k_hp_update_timer reo_cmd_timer;
	struct ath12k_hp_update_timer tx_ring_timer[DP_TCL_NUM_RING_MAX];
	struct ath12k_spt_info *spt_info;
	u32 num_spt_pages;
	u32 rx_ppt_base;
	struct ath12k_rx_desc_info *rxbaddr[ATH12K_NUM_RX_SPT_PAGES];
	struct ath12k_tx_desc_info *txbaddr[ATH12K_NUM_TX_SPT_PAGES];
	struct ath12k_ppeds_tx_desc_info **ppedstxbaddr;
	struct list_head rx_ppeds_reuse_list;
	struct list_head rx_desc_free_list;
	/* protects the free desc list */
	spinlock_t rx_desc_lock;

	struct list_head tx_desc_free_list[ATH12K_HW_MAX_QUEUES];
	/* protects the free and used desc lists */
	spinlock_t tx_desc_lock[ATH12K_HW_MAX_QUEUES];

	struct dp_rxdma_ring rx_refill_buf_ring;
	struct dp_srng rx_mac_buf_ring[MAX_RXDMA_PER_PDEV];
	struct dp_srng rxdma_err_dst_ring[MAX_RXDMA_PER_PDEV];
	struct ath12k_reo_q_addr_lut reoq_lut;
	struct ath12k_reo_q_addr_lut ml_reoq_lut;
	const struct ath12k_hw_params *hw_params;
	struct device *dev;
	struct ath12k_hal *hal;

	/* protects data fields like dp_pdevs, peers and rhead_peer_addr */
	spinlock_t dp_lock;
	struct ath12k_pdev_dp __rcu *dp_pdevs[MAX_RADIOS];
	u8 num_radios;

	struct ath12k_dp_hw_group *dp_hw_grp;
	u8 device_id;

	struct ath12k_dp_arch_ops *arch_ops;

	struct dp_fst_config fst_config;

	struct ath12k_dp_mon *dp_mon;

	/* Linked list of struct ath12k_dp_link_peer */
	struct list_head peers;

	/* To synchronize rhash tbl write operation */
	struct mutex tbl_mtx_lock;

	/* The rhashtable containing struct ath12k_peer keyed by mac addr */
	struct rhashtable *rhead_peer_addr;
	struct rhashtable_params rhash_peer_addr_param;
	struct ath12k_ppe ppe;

	/*Neighbors Peer list for NAC RSSI*/
	struct list_head neighbor_peers;
	int num_nrps;
	unsigned long service_rings_running;
	bool stats_disable;

	/* HW link ID position in PPDU_ID */
	u8 link_id_offset;
	u8 link_id_bits;
	u8 rx_pktlog_mode;
};
/* @brief target -> host extended statistics upload
 *
 * @details
 * The following field definitions describe the format of the HTT target
 * to host stats upload confirmation message.
 * The message contains a cookie echoed from the HTT host->target stats
 * upload request, which identifies which request the confirmation is
 * for, and a single stats can span over multiple HTT stats indication
 * due to the HTT message size limitation so every HTT ext stats indication
 * will have tag-length-value stats information elements.
 * The tag-length header for each HTT stats IND message also includes a
 * status field, to indicate whether the request for the stat type in
 * question was fully met, partially met, unable to be met, or invalid
 * (if the stat type in question is disabled in the target).
 * A Done bit 1's indicate the end of the of stats info elements.
 *
 *
 * |31                         16|15    12|11|10 8|7   5|4       0|
 * |--------------------------------------------------------------|
 * |                   reserved                   |    msg type   |
 * |--------------------------------------------------------------|
 * |                         cookie LSBs                          |
 * |--------------------------------------------------------------|
 * |                         cookie MSBs                          |
 * |--------------------------------------------------------------|
 * |      stats entry length     | rsvd   | D|  S |   stat type   |
 * |--------------------------------------------------------------|
 * |                   type-specific stats info                   |
 * |                      (see htt_stats.h)                       |
 * |--------------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: Identifies this is a extended statistics upload confirmation
 *             message.
 *    Value: 0x1c
 *  - COOKIE_LSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: LSBs of the opaque cookie specified by the host-side requestor
 *  - COOKIE_MSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: MSBs of the opaque cookie specified by the host-side requestor
 *
 * Stats Information Element tag-length header fields:
 *  - STAT_TYPE
 *    Bits 7:0
 *    Purpose: identifies the type of statistics info held in the
 *        following information element
 *    Value: htt_dbg_ext_stats_type
 *  - STATUS
 *    Bits 10:8
 *    Purpose: indicate whether the requested stats are present
 *    Value: htt_dbg_ext_stats_status
 *  - DONE
 *    Bits 11
 *    Purpose:
 *        Indicates the completion of the stats entry, this will be the last
 *        stats conf HTT segment for the requested stats type.
 *    Value:
 *        0 -> the stats retrieval is ongoing
 *        1 -> the stats retrieval is complete
 *  - LENGTH
 *    Bits 31:16
 *    Purpose: indicate the stats information size
 *    Value: This field specifies the number of bytes of stats information
 *       that follows the element tag-length header.
 *       It is expected but not required that this length is a multiple of
 *       4 bytes.
 */

 enum dp_umac_reset_recover_action {
	 ATH12K_UMAC_RESET_RX_EVENT_NONE,
	 ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY,
	 ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC,
	 ATH12K_UMAC_RESET_DO_PRE_RESET,
	 ATH12K_UMAC_RESET_DO_POST_RESET_START,
	 ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE,
 };

enum dp_umac_reset_tx_cmd {
	ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE,
	ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE,
	ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
	ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE,
};

struct ath12k_umac_reset_ts {
	u64 trigger_start;
	u64 trigger_done;
	u64 pre_reset_start;
	u64 pre_reset_done;
	u64 post_reset_start;
	u64 post_reset_done;
	u64 post_reset_complete_start;
	u64 post_reset_complete_done;
};

struct ath12k_dp_umac_reset {
	struct ath12k_base *ab;
	dma_addr_t shmem_paddr_unaligned;
	void *shmem_vaddr_unaligned;
	dma_addr_t shmem_paddr_aligned;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr_aligned;
	size_t shmem_size;
	uint32_t magic_num;
	int intr_offset;
	struct tasklet_struct intr_tq;
	int irq_num;
	struct ath12k_umac_reset_ts ts;
	bool umac_pre_reset_in_prog;
};

#define HTT_T2H_EXT_STATS_INFO1_DONE	BIT(11)
#define HTT_T2H_EXT_STATS_INFO1_LENGTH   GENMASK(31, 16)

#define	HTT_MAC_ADDR_L32_0	GENMASK(7, 0)
#define	HTT_MAC_ADDR_L32_1	GENMASK(15, 8)
#define	HTT_MAC_ADDR_L32_2	GENMASK(23, 16)
#define	HTT_MAC_ADDR_L32_3	GENMASK(31, 24)
#define	HTT_MAC_ADDR_H16_0	GENMASK(7, 0)
#define	HTT_MAC_ADDR_H16_1	GENMASK(15, 8)

static inline int ath12k_dp_arch_op_device_init(struct ath12k_dp *dp)
{
	return dp->arch_ops->dp_op_device_init(dp);
}

static inline void ath12k_dp_arch_op_device_deinit(struct ath12k_dp *dp)
{
	dp->arch_ops->dp_op_device_deinit(dp);
}

static inline u32 ath12k_dp_arch_tx_get_vdev_bank_config(struct ath12k_dp *dp,
							 struct ath12k_link_vif *arvif,
							 bool vdev_id_check_en)
{
	return dp->arch_ops->dp_tx_get_vdev_bank_config(dp->ab, arvif, vdev_id_check_en);
}

static inline int ath12k_dp_arch_reo_cmd_send(struct ath12k_dp *dp,
					      struct ath12k_dp_rx_tid *rx_tid,
					      enum hal_reo_cmd_type type,
					      struct ath12k_hal_reo_cmd *cmd,
					      void (*cb)(struct ath12k_dp *dp, void *ctx,
							 enum hal_reo_cmd_status status))
{
	return dp->arch_ops->dp_reo_cmd_send(dp->ab, rx_tid, type, cmd, cb);
}

static inline void ath12k_dp_arch_setup_pn_check_reo_cmd(struct ath12k_dp *dp,
							 struct ath12k_hal_reo_cmd *cmd,
							 struct ath12k_dp_rx_tid *rx_tid,
							 u32 cipher,
							 enum set_key_cmd key_cmd)
{
	dp->arch_ops->setup_pn_check_reo_cmd(cmd, rx_tid, cipher, key_cmd);
}

static inline void ath12k_dp_arch_rx_peer_tid_delete(struct ath12k_dp *dp,
						     struct ath12k *ar,
						     struct ath12k_dp_link_peer *peer,
						     u8 tid)
{
	dp->arch_ops->rx_peer_tid_delete(ar, peer, tid);
}

static inline int ath12k_dp_arch_reo_cache_flush(struct ath12k_dp *dp,
						 struct ath12k_dp_rx_tid *rx_tid)
{
	return dp->arch_ops->reo_cache_flush(dp->ab, rx_tid);
}

static inline
int ath12k_dp_arch_rx_link_desc_return(struct ath12k_dp *dp,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action)
{
	return dp->arch_ops->rx_link_desc_return(dp, buf_addr_info, action);
}

static inline int ath12k_dp_arch_peer_rx_tid_reo_update(struct ath12k_dp *dp,
							struct ath12k *ar,
							struct ath12k_dp_link_peer *peer,
							struct ath12k_dp_rx_tid *rx_tid,
							u32 ba_win_sz, u16 ssn,
							bool update_ssn)
{
	return dp->arch_ops->peer_rx_tid_reo_update(ar, peer, rx_tid,
						    ba_win_sz, ssn, update_ssn);
}

static inline int ath12k_dp_arch_alloc_reo_qdesc(struct ath12k_dp *dp,
						 struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
						 enum hal_pn_type pn_type,
						 struct hal_rx_reo_queue **addr_aligned)
{
	return dp->arch_ops->alloc_reo_qdesc(dp->ab, rx_tid, ssn, pn_type, addr_aligned);
}

static inline void ath12k_dp_arch_peer_rx_tid_qref_setup(struct ath12k_dp *dp,
							 u16 peer_id, u16 tid,
							 dma_addr_t paddr)
{
	dp->arch_ops->peer_rx_tid_qref_setup(dp->ab, peer_id, tid, paddr);
}

static inline int ath12k_dp_arch_rx_fst_attach(struct ath12k_dp *dp,
					       struct dp_rx_fst *fst)
{
	return dp->arch_ops->rx_fst_attach(dp, fst);
}

static inline void ath12k_dp_arch_rx_fst_detach(struct ath12k_dp *dp,
						struct dp_rx_fst *fst)
{
	dp->arch_ops->rx_fst_detach(dp, fst);
}

static inline void
ath12k_dp_arch_rx_flow_dump_entry(struct ath12k_dp *dp,
				  struct rx_flow_info *flow_info)
{
	return dp->arch_ops->rx_flow_dump_entry(dp, flow_info);
}

static inline int
ath12k_dp_arch_rx_flow_add_entry(struct ath12k_dp *dp,
				 struct rx_flow_info *flow_info)
{
	return dp->arch_ops->rx_flow_add_entry(dp, flow_info);
}

static inline int
ath12k_dp_arch_rx_flow_delete_entry(struct ath12k_dp *dp,
				    struct rx_flow_info *flow_info)
{
	return dp->arch_ops->rx_flow_delete_entry(dp, flow_info);
}

static inline int
ath12k_dp_arch_rx_flow_delete_all_entries(struct ath12k_dp *dp)
{
	return dp->arch_ops->rx_flow_delete_all_entries(dp);
}

static inline ssize_t
ath12k_dp_arch_dump_fst_table(struct ath12k_dp *dp, char *buf, int size)
{
	return dp->arch_ops->dump_fst_table(dp, buf, size);
}

static inline struct ath12k_dp_hw_group *
ath12k_core_dp_hw_group_alloc(struct ath12k_dp *dp)
{
	return dp->arch_ops->dp_hw_group_alloc();
}

static inline int ath12k_dp_arch_pdev_alloc(struct ath12k_dp *dp)
{
	return dp->arch_ops->dp_pdev_alloc(dp->ab);
}

static inline void ath12k_dp_arch_pdev_free(struct ath12k_dp *dp)
{
	dp->arch_ops->dp_pdev_free(dp->ab);
}

static inline void ath12k_dp_get_mac_addr(u32 addr_l32, u16 addr_h16, u8 *addr)
{
	memcpy(addr, &addr_l32, 4);
	memcpy(addr + 4, &addr_h16, ETH_ALEN - 4);
}

static inline struct ath12k_dp *
ath12k_dp_hw_grp_to_dp(struct ath12k_dp_hw_group *dp_hw_grp, u8 device_id)
{
	return dp_hw_grp->dp[device_id];
}

static inline struct ieee80211_hw *
ath12k_dp_pdev_to_hw(struct ath12k_pdev_dp *pdev)
{
	return pdev->hw;
}

static inline struct ath12k_pdev_dp *
ath12k_dp_to_dp_pdev(struct ath12k_dp *dp, u8 pdev_id)
{
	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
                        "ath12k dp to dp pdev called without rcu lock");

	return rcu_dereference(dp->dp_pdevs[pdev_id]);
}

static inline int
ath12k_dp_arch_peer_migrate_reo_cmd(struct ath12k_dp *dp,
				    struct ath12k_dp_link_peer *peer,
				     u16 peer_id, u8 chip_id)
{
	return dp->arch_ops->peer_migrate_reo_cmd(dp, peer, peer_id,
						  chip_id);
}

int ath12k_dp_htt_connect(struct ath12k_dp *dp);
void ath12k_dp_vdev_tx_attach(struct ath12k *ar, struct ath12k_link_vif *arvif);
void ath12k_dp_partner_cc_init(struct ath12k_base *ab);
int ath12k_dp_get_pdev_telemetry_stats(struct ath12k_base *ab,
                                      int pdev_id,
                                      struct ath12k_pdev_telemetry_stats *stats);
int ath12k_dp_pdev_pre_alloc(struct ath12k *ar);
int ath12k_dp_tx_htt_srng_setup(struct ath12k_base *ab, u32 ring_id,
				int mac_id, enum hal_ring_type ring_type);
int ath12k_dp_peer_setup(struct ath12k *ar, struct ath12k_link_vif *arvif, const u8 *addr);
void ath12k_dp_peer_cleanup(struct ath12k *ar, int vdev_id, const u8 *addr);
void ath12k_dp_srng_cleanup(struct ath12k_base *ab, struct dp_srng *ring);
int ath12k_dp_srng_setup(struct ath12k_base *ab, struct dp_srng *ring,
			 enum hal_ring_type type, int ring_num,
			 int mac_id, int num_entries);
void ath12k_dp_link_desc_cleanup(struct ath12k_base *ab,
				 struct dp_link_desc_bank *desc_bank,
				 u32 ring_type, struct dp_srng *ring);
int ath12k_dp_link_desc_setup(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks,
			      u32 ring_type, struct hal_srng *srng,
			      u32 n_link_desc);
struct ath12k_rx_desc_info *ath12k_dp_get_rx_desc(struct ath12k_dp *dp,
						  u32 cookie);
struct ath12k_tx_desc_info *ath12k_dp_get_tx_desc(struct ath12k_dp *dp,
						  u32 desc_id);
bool ath12k_dp_wmask_compaction_rx_tlv_supported(struct ath12k_base *ab);
bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab);
void ath12k_umac_reset_notify_target_sync_and_send(struct ath12k_base *ab,
                                       enum dp_umac_reset_tx_cmd tx_event);
void ath12k_umac_reset_handle_post_reset_start(struct ath12k_base *ab);
bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab);
void ath12k_dp_tx_update_bank_profile(struct ath12k_link_vif *arvif);
void ath12k_dp_reoq_lut_addr_reset(struct ath12k_dp *dp);
void ath12k_dp_srng_msi_setup(struct ath12k_base *ab,
			      struct hal_srng_params *ring_params,
			      enum hal_ring_type type, int ring_num);
void ath12k_hal_tx_config_rbm_mapping(struct ath12k_base *ab, u8 ring_num,
				      u8 rbm_id, int ring_type);
size_t ath12k_dp_get_req_entries_from_buf_ring(struct ath12k_base *ab,
					       struct dp_rxdma_ring *rx_ring,
					       struct list_head *list);
int ath12k_dp_init_bank_profiles(struct ath12k_base *ab);
void ath12k_dp_deinit_bank_profiles(struct ath12k_base *ab);
int ath12k_dp_reoq_lut_setup(struct ath12k_base *ab);
void ath12k_dp_reoq_lut_cleanup(struct ath12k_base *ab);
int ath12k_dp_cc_init(struct ath12k_base *ab);
void ath12k_dp_cc_cleanup(struct ath12k_base *ab);
int ath12k_wbm_idle_ring_setup(struct ath12k_base *ab, u32 *n_link_desc);
int ath12k_dp_srng_common_setup(struct ath12k_base *ab);
void ath12k_dp_srng_common_cleanup(struct ath12k_base *ab);
enum ath12k_dp_eapol_key_type ath12k_dp_get_eapol_subtype(u8 *data);
ssize_t ath12k_dp_dump_device_ring_stats(struct ath12k_base *ab,
					 char *buf, int size);
void ath12k_dp_clear_link_desc_pool(struct ath12k_dp *dp);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_dp_tx_get_bank_profile(struct ath12k_base *ab, struct ath12k_link_vif *arvif,
				  struct ath12k_dp *dp, bool vdev_id_check_en);
struct ath12k_ppeds_tx_desc_info *ath12k_dp_get_ppeds_tx_desc(struct ath12k_base *ab,
							      u32 desc_id);
int ath12k_dp_cc_ppeds_desc_init(struct ath12k_base *ab);
int ath12k_dp_cc_ppeds_desc_cleanup(struct ath12k_base *ab);
void ath12k_dp_ppeds_tx_cmem_init(struct ath12k_base *ab, struct ath12k_dp *dp);
int ath12k_dp_ppe_rxole_rxdma_cfg(struct ath12k_base *ab);
void ath12k_dp_get_device_stats(struct ath12k_dp *dp,
				struct ath12k_telemetry_dp_device *telemetry_device);
int ath12k_dp_get_peer_stats(struct ath12k_vif *ahvif,
			     struct ath12k_telemetry_dp_peer *telemetry_peer,
			     u8 *addr, u8 link_id);
void ath12k_dp_get_vif_stats(struct ath12k_vif *ahvif,
			     struct ath12k_telemetry_dp_vif *telemetry_vif,
			     u8 link_id);
void ath12k_dp_get_pdev_stats(struct ath12k_pdev_dp *pdev,
			      struct ath12k_telemetry_dp_radio *telemetry_radio);
void ath12k_dp_increment_bank_num_users(struct ath12k_dp *dp,
					int bank_id);
#endif
#endif
