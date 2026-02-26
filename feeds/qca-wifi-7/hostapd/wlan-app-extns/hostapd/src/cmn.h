/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CMN_H
#define CMN_H

struct hostapd_config;
struct sta_info;
struct hostapd_iface;
struct nl_msg;
struct hostapd_channel_data;
enum ieee80211_op_mode;
struct hostapd_sta_add_params;
enum bw_type;
struct hostapd_freq_params;
struct hostapd_data;
struct ieee802_11_elems;

struct ieee80211_240mhz_vendor_oper_extn {
	u8 ccfs1;
	u8 ccfs0;
	u16 punct_bitmap;
	u16 is5ghz240mhz          :1,
	    bfmess320mhz          :3,
	    numsound320mhz        :3,
	    nonofdmaulmumimo320mhz:1,
	    mubfmr320mhz          :1;
	u8 mcs_map_320mhz[3];
} STRUCT_PACKED;

struct ieee80211_240mhz_params_extn {
	struct ieee80211_240mhz_vendor_oper_extn *eht_240mhz_capab;
	size_t eht_240mhz_capab_len;
};

struct  hostapd_sta_add_params_extn {
	struct ieee80211_240mhz_params_extn params_240mhz;
};

struct sta_info_extn {
	struct ieee80211_240mhz_params_extn params_240mhz;
};

struct ieee802_11_elems_extn {
	const u8 *eht_240mhz_capab;
	u8 eht_240mhz_capab_len;
};


#ifndef CONFIG_QCN_EXTN

static inline void
hostapd_get_oper_center_freq_seg_extn(struct hostapd_config *conf,
				      u8 *oper_centr_freq_seg0_idx,
				      u8 *oper_centr_freq_seg1_idx,
				      enum oper_chan_width *oper_chwidth)
{
	return;
}

static inline u8
hostapd_set_legacy_oper_centr_freq_seg0_extn(struct hostapd_config *conf,
					     u8 oper_centr_freq_seg0_idx)
{
	return oper_centr_freq_seg0_idx;
}

static inline int
hostapd_modify_n_chans_for_240mhz_extn(struct hostapd_iface *iface,
				       int n_chans)
{
	return n_chans;
}

static inline int
hostapd_modify_supported_op_class_for_240mhz_extn(int freq,
						  enum oper_chan_width ch_width,
						  u8 *op_class)
{
	return -1;
}

static inline void
hostapd_modify_buflen_for_240mhz_extn(size_t *buflen,
				      struct hostapd_data *hapd)
{
	return;
}

static inline int
hostapd_get_n_chans_and_frequency_extn(enum oper_chan_width oper_chwidth,
				       int cf1,
				       int *n_chans,
				       int *frequency)
{
	return -1;
}

static inline int hostapd_get_dfs_half_chwidth_extn(enum chan_width width)
{
	return 0;
}

static inline int
hostapd_dfs_get_allowed_channels_extn(int n_chans,
				      int *is_allowed,
				      unsigned int *allowed_no)
{
	return -1;
}

static inline int
hostapd_dfs_adjust_center_freq_extn(int oper_chwidth,
				    short chan,
				    u8 *oper_centr_freq_seg0_idx,
				    u8 *oper_centr_freq_seg1_idx)
{
	return -1;
}

static inline int
hostapd_get_bw_and_startchan_for_240mhz_extn(enum oper_chan_width
					     eht_oper_chwidth,
					     u8 eht_oper_centr_freq_seg0_idx,
					     u16 *bw, u8 *start_chan)
{
	return -1;
}

static inline u8 *
hostapd_eid_vendor_240mhz_extn(struct hostapd_data *hapd, u8 *eid,
			       int opmode)
{
	return eid;
}

static inline u16
hostapd_copy_sta_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     int opmode,
				     struct ieee802_11_elems_extn *elems_extn)
{
	return 0;
}

static inline void
hostapd_get_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				struct sta_info_extn *sta_extn,
				struct ieee80211_240mhz_vendor_oper_extn *dest)
{
	return;
}

static inline void hostapd_sta_os_free_extn(struct sta_info_extn *sta_extn)
{
	return;
}

static inline int
ieee802_11_parse_vendor_specific_eht_240mhz_cap_extn(struct ieee802_11_elems
						     *elems,
						     unsigned int oui_flag,
						     const u8 *pos,
						     size_t elen)
{
	return -1;
}

static inline int
ieee802_11_parse_vendor_specific_elems_extn(struct ieee802_11_elems *elems,
					    unsigned int oui_flag,
					    const u8 *pos, size_t elen)
{
	return -1;
}

static inline void
hostapd_copy_sta_add_params_extn(struct hostapd_sta_add_params_extn
				 *params_extn,
				 struct sta_info_extn *sta_extn)
{
	return;
}

static inline void
wpa_driver_nl80211_sta_add_extn(void *priv,
				struct hostapd_sta_add_params
				*params)
{
	return;
}

static inline bool
hostapd_dfs_get_valid_punc_bitmap_extn(int chan_freq,
				       u16 punct_bitmap,
				       int center_freq,
				       int half_width)
{
	return false;
}

static inline int
hostapd_find_dfs_range_extn(struct hostapd_iface *iface,
			    enum chan_width bandwidth,
			    struct hostapd_freq_params *freq_params)
{
	return -1;
}

static inline int
hostapd_is_dfs_overlap_extn(struct hostapd_iface *iface,
			    enum chan_width width,
			    int center_freq, u16 punct_bitmap)
{
	return -1;
}

static inline void
hostapd_modify_supported_op_class_for_320mhz_extn(int freq,
						  u8 *op_class)
{
	return;
}

#else

void hostapd_get_oper_center_freq_seg_extn(struct hostapd_config *conf,
					   u8 *oper_centr_freq_seg0_idx,
					   u8 *oper_centr_freq_seg1_idx,
					   enum oper_chan_width *oper_chwidth);
u8 hostapd_set_legacy_oper_centr_freq_seg0_extn(struct hostapd_config *conf,
						u8 oper_centr_freq_seg0_idx);
int
hostapd_acs_update_puncturing_bitmap_extn(struct hostapd_config *conf,
					  u16 bw,
					  struct hostapd_channel_data *chan);
int hostapd_modify_n_chans_for_240mhz_extn(struct hostapd_iface *iface,
					   int n_chans);
int
hostapd_modify_supported_op_class_for_240mhz_extn(int freq,
						  enum oper_chan_width ch_width,
						  u8 *op_class);
void hostapd_modify_buflen_for_240mhz_extn(size_t *buflen,
					   struct hostapd_data *hapd);
int hostapd_dfs_get_start_chan_idx_extn(struct hostapd_iface *iface);
int hostapd_get_n_chans_and_frequency_extn(enum oper_chan_width oper_chwidth,
					   int cf1,
					   int *n_chans,
					   int *frequency);
int hostapd_get_dfs_half_chwidth_extn(enum chan_width width);
int hostapd_dfs_get_allowed_channels_extn(int n_chans,
					  int *is_allowed,
					  unsigned int *allowed_no);
int hostapd_dfs_adjust_center_freq_extn(int oper_chwidth,
					short chan,
					u8 *oper_centr_freq_seg0_idx,
					u8 *oper_centr_freq_seg1_idx);
int
hostapd_get_bw_and_startchan_for_240mhz_extn(enum oper_chan_width
					     eht_oper_chwidth,
					     u8 eht_oper_centr_freq_seg0_idx,
					     u16 *bw, u8 *start_chan);
u8 * hostapd_eid_vendor_240mhz_extn(struct hostapd_data *hapd, u8 *eid,
				    enum ieee80211_op_mode opmode);
u16
hostapd_copy_sta_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     enum ieee80211_op_mode opmode,
				     struct ieee802_11_elems_extn *elems_extn);
void hostapd_get_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				     struct sta_info_extn *sta_extn,
				     struct ieee80211_240mhz_vendor_oper_extn
				     *dest);
void hostapd_sta_os_free_extn(struct sta_info_extn *sta_extn);
int
ieee802_11_parse_vendor_specific_eht_240mhz_cap_extn(struct ieee802_11_elems
						     *elems,
						     unsigned int oui_flag,
						     const u8 *pos,
						     size_t elen);
int ieee802_11_parse_vendor_specific_elems_extn(struct ieee802_11_elems *elems,
						unsigned int oui_flag,
						const u8 *pos, size_t elen);
void hostapd_copy_sta_add_params_extn(struct hostapd_sta_add_params_extn
				      *params_extn,
				      struct sta_info_extn *sta_extn);
void wpa_driver_nl80211_sta_add_extn(void *priv,
				     struct hostapd_sta_add_params *params);
bool hostapd_dfs_get_valid_punc_bitmap_extn(int chan_freq,
					    u16 punct_bitmap,
					    int center_freq,
					    int half_width);
int hostapd_find_dfs_range_extn(struct hostapd_iface *iface,
				enum chan_width bandwidth,
				struct hostapd_freq_params *freq_params);
int hostapd_is_dfs_overlap_extn(struct hostapd_iface *iface,
				enum chan_width width,
				int center_freq, u16 punct_bitmap);
void hostapd_modify_supported_op_class_for_320mhz_extn(int freq, u8 *op_class);

#endif /* CONFIG_QCN_EXTN */
#endif /* CMN_H */
