/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH11K_PEER_H
#define ATH11K_PEER_H

struct ppdu_user_delayba {
	u8 reserved0;
	u16 sw_peer_id;
	u32 info0;
	u16 ru_end;
	u16 ru_start;
	u32 info1;
	u32 rate_flags;
	u32 resp_rate_flags;
};

enum ath11k_ast_entry_type {
	ATH11K_AST_TYPE_NONE, /* static ast entry for connected peer */
	ATH11K_AST_TYPE_STATIC, /* static ast entry for connected peer */
	ATH11K_AST_TYPE_SELF, /* static ast entry for self peer (STA mode) */
	ATH11K_AST_TYPE_WDS,  /* WDS peer ast entry type*/
	ATH11K_AST_TYPE_MEC,  /* Multicast echo ast entry type */
	ATH11K_AST_TYPE_WDS_HM, /* HM WDS entry */
	ATH11K_AST_TYPE_STA_BSS,       /* BSS entry(STA mode) */
	ATH11K_AST_TYPE_DA,   /* AST entry based on Destination address */
	ATH11K_AST_TYPE_WDS_HM_SEC, /* HM WDS entry for secondary radio */
	ATH11K_AST_TYPE_MAX
};

enum ath11k_wds_wmi_action {
	ATH11K_WDS_WMI_ADD = 1,
	ATH11K_WDS_WMI_UPDATE,
	ATH11K_WDS_WMI_REMOVE,

	ATH11K_WDS_WMI_MAX
};

struct ath11k_ast_entry {
	u16 ast_idx;
	u8 addr[ETH_ALEN];
	enum ath11k_wds_wmi_action action;
	struct ath11k_peer *peer;
	struct ath11k *ar;
	bool next_hop;
	bool is_active;
	bool is_mapped;
	u8 pdev_idx;
	u8 vdev_id;
	u16 ast_hash_value;
	int ref_cnt;
	enum ath11k_ast_entry_type type;
	bool delete_in_progress;
	void *cookie;
	struct list_head ase_list;
	struct list_head wmi_list;
};

struct ath11k_peer {
	struct list_head list;
	struct ieee80211_sta *sta;
	struct ieee80211_vif *vif;
	int vdev_id;
	u8 addr[ETH_ALEN];
	int peer_id;
	u16 ast_hash;
	u8 pdev_idx;
	u16 hw_peer_id;
	struct ath11k_nss_peer nss;
#ifdef CPTCFG_ATH11K_NSS_SUPPORT
	struct ath11k_ast_entry *self_ast_entry;
	struct list_head ast_entry_list;
#endif

	/* protected by ab->data_lock */
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1];
	struct dp_rx_tid rx_tid[IEEE80211_NUM_TIDS + 1];

	/* peer id based rhashtable list pointer */
	struct rhash_head rhash_id;
	/* peer addr based rhashtable list pointer */
	struct rhash_head rhash_addr;

	/* Info used in MMIC verification of
	 * RX fragments
	 */
	struct crypto_shash *tfm_mmic;
	u8 mcast_keyidx;
	u8 ucast_keyidx;
	u16 sec_type;
	u16 sec_type_grp;
	bool is_authorized;
	struct ppdu_user_delayba ppdu_stats_delayba;
	bool delayba_flag;
	bool peer_logging_enabled;
	bool delete_in_progress;
	/* Peer's datapath set flag */
	bool dp_setup_done;
};

void ath11k_peer_unmap_event(struct ath11k_base *ab, u16 peer_id);
void ath11k_peer_unmap_v2_event(struct ath11k_base *ab, u16 peer_id, u8 *mac_addr,
			        bool is_wds, u32 free_wds_count);
void ath11k_peer_map_event(struct ath11k_base *ab, u8 vdev_id, u16 peer_id,
			   u8 *mac_addr, u16 ast_hash, u16 hw_peer_id);
void ath11k_peer_map_v2_event(struct ath11k_base *ab, u8 vdev_id, u16 peer_id,
			      u8 *mac_addr, u16 ast_hash, u16 hw_peer_id,
			      bool is_wds);
struct ath11k_peer *ath11k_peer_find(struct ath11k_base *ab, int vdev_id,
				     const u8 *addr);
struct ath11k_peer *ath11k_peer_find_by_addr(struct ath11k_base *ab,
					     const u8 *addr);
struct ath11k_peer *ath11k_peer_find_by_id(struct ath11k_base *ab, int peer_id);
struct ath11k_peer *ath11k_peer_find_by_ast(struct ath11k_base *ab, int ast_hash);
void ath11k_peer_cleanup(struct ath11k *ar, u32 vdev_id);
int ath11k_peer_delete(struct ath11k *ar, u32 vdev_id, u8 *addr);
int ath11k_peer_create(struct ath11k *ar, struct ath11k_vif *arvif,
		       struct ieee80211_sta *sta, struct peer_create_params *param);
int ath11k_wait_for_peer_delete_done(struct ath11k *ar, u32 vdev_id,
				     const u8 *addr);
struct ath11k_peer *ath11k_peer_find_by_vdev_id(struct ath11k_base *ab,
						int vdev_id);
int ath11k_peer_rhash_tbl_init(struct ath11k_base *ab);
void ath11k_peer_rhash_tbl_destroy(struct ath11k_base *ab);
int ath11k_peer_rhash_delete(struct ath11k_base *ab, struct ath11k_peer *peer);

#ifdef CPTCFG_ATH11K_NSS_SUPPORT
struct ath11k_ast_entry *ath11k_peer_ast_find_by_addr(struct ath11k_base *ab,
						      u8* addr);
struct ath11k_ast_entry *ath11k_peer_ast_find_by_pdev_idx(struct ath11k *ar,
							  u8* addr);
int ath11k_peer_add_ast(struct ath11k *ar, struct ath11k_peer *peer,
			u8* mac_addr, enum ath11k_ast_entry_type type);
int ath11k_peer_update_ast(struct ath11k *ar, struct ath11k_peer *peer,
			   struct ath11k_ast_entry *ast_entry);
void ath11k_peer_map_ast(struct ath11k *ar, struct ath11k_peer *peer,
			 u8* mac_addr, u16 hw_peer_id, u16 ast_hash);
void ath11k_peer_del_ast(struct ath11k *ar, struct ath11k_ast_entry *ast_entry);
void ath11k_peer_ast_cleanup(struct ath11k *ar, struct ath11k_peer *peer,
			     bool is_wds, u32 free_wds_count);
void ath11k_peer_ast_wds_wmi_wk(struct work_struct *wk);
struct ath11k_ast_entry *ath11k_peer_ast_find_by_peer(struct ath11k_base *ab,
						      struct ath11k_peer *peer,
						      u8* addr);
#else
static inline struct ath11k_ast_entry *ath11k_peer_ast_find_by_addr(struct ath11k_base *ab,
								    u8* addr)
{
	return NULL;
}

static inline struct ath11k_ast_entry *ath11k_peer_ast_find_by_pdev_idx(struct ath11k *ar,
									u8* addr)
{
	return NULL;
}

static inline int ath11k_peer_add_ast(struct ath11k *ar, struct ath11k_peer *peer,
				      u8* mac_addr, enum ath11k_ast_entry_type type)
{
	return 0;
}

static inline int ath11k_peer_update_ast(struct ath11k *ar, struct ath11k_peer *peer,
					 struct ath11k_ast_entry *ast_entry)
{
	return 0;
}

static inline void ath11k_peer_map_ast(struct ath11k *ar, struct ath11k_peer *peer,
				       u8* mac_addr, u16 hw_peer_id, u16 ast_hash)
{
	return;
}

static inline void ath11k_peer_del_ast(struct ath11k *ar,
				       struct ath11k_ast_entry *ast_entry)
{
	return;
}

static inline void ath11k_peer_ast_cleanup(struct ath11k *ar,
					   struct ath11k_peer *peer,
					   bool is_wds, u32 free_wds_count)
{
	return;
}

static inline void ath11k_peer_ast_wds_wmi_wk(struct work_struct *wk)
{
	return;
}

static inline struct ath11k_ast_entry *ath11k_peer_ast_find_by_peer(struct ath11k_base *ab,
								    struct ath11k_peer *peer,
								    u8* addr)
{
	return NULL;
}
#endif /* CPTCFG_ATH11K_NSS_SUPPORT */
#endif /* _PEER_H_ */
