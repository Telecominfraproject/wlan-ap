/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef CMN_EXTN_H
#define CMN_EXTN_H

struct ieee802_11_elems;
enum ieee80211_conn_mode;
struct ieee80211_conn_settings;
struct sk_buff;
struct ieee80211_sub_if_data;
struct ieee80211_vif;
struct ieee80211_local;
struct link_sta_info;
struct ieee80211_supported_band;
struct cfg80211_bss;
struct cfg80211_chan_def;
struct ieee80211_link_sta;


struct bw240MHz_parameters_extn {
	struct ieee80211_240mhz_vendor_oper_extn *eht_240mhz_cap;
	u8 eht_240mhz_len;
};

struct link_station_parameters_extn {
	struct bw240MHz_parameters_extn params_240MHz;
};

struct ieee802_11_elems_extn {
	struct bw240MHz_parameters_extn params_240MHz;
};

struct ieee80211_bss_extn {
	struct bw240MHz_parameters_extn params_240MHz;
};

struct ieee80211_240mhz_vendor_oper_extn {
	u8 ccfs1;
	u8 ccfs0;
	u16 punctured;
	u16 is5ghz240mhz          :1,
	    bfmess320mhz          :3,
	    numsound320mhz        :3,
	    nonofdmaulmumimo320mhz:1,
	    mubfmr320mhz          :1;
	u8 mcs_map_320mhz[3];
}__packed;

#ifndef CPTCFG_QCN_EXTN

static inline u8 *
ieee80211_add_qcn_vendor_ie_extn(struct sk_buff *skb,
				 struct ieee80211_bss_extn *bss_extn,
				 int  mode)
{
	return NULL;
}

static inline void
ieee80211_240mhz_cap_to_eht_cap_extn(struct ieee80211_sub_if_data *sdata,
				     struct link_sta_info *link_sta,
				     struct ieee80211_supported_band *sband,
				     const struct
				     ieee80211_240mhz_vendor_oper_extn
				     *eht_240mhz_cap)
{
	return;
}

static inline void
ieee80211_inform_bss_extn(struct cfg80211_bss *cbss,
			  struct ieee802_11_elems *elems)
{
	return;
}

static inline int
ieee802_11_parse_elems_vendor_qcn_extn(const u8 *pos, u8 elen,
				       struct ieee802_11_elems *elems)
{
	return -1;
}

static inline int
ieee802_11_determine_ap_chan_extn(const struct ieee802_11_elems *elems,
				  struct cfg80211_chan_def *chandef,
				  struct ieee80211_sub_if_data *sdata)
{
	/* If this function returns -1, then mode is forced to be
	 * IEEE80211_CONN_MODE_HE. Hence, return 0 to not alter the
	 * AP mode.
	 */
	return 0;
}

static inline size_t
ieee802_11_qcn_ie_len_extn(struct cfg80211_bss *cbss,
			   struct ieee80211_local *local)
{
	return 0;
}

static inline struct ieee80211_240mhz_vendor_oper_extn*
drv_get_240mhz_cap_extn(struct ieee80211_local *local,
			struct ieee80211_vif *vif,
			struct ieee80211_link_sta *link_sta)
{
	return NULL;
}

static inline void
ieee80211_bss_240mhz_to_sta_eht_cap_extn(struct ieee80211_sub_if_data *sdata,
					 struct link_sta_info *link_sta,
					 struct ieee80211_supported_band *sband,
					 struct cfg80211_bss *cbss)
{
	return;
}

static inline void
ieee80211_update_eht_cap_from_240mhz_nl(struct ieee80211_sub_if_data *sdata,
					struct link_sta_info *link_sta,
					struct ieee80211_supported_band *sband)
{
	return;
}

static inline void
ieee80211_modify_bw_limit_for_240mhz(bool is_5ghz,
				     struct ieee80211_conn_settings *conn)
{
	return;
}

#else

u8 *
ieee80211_add_qcn_vendor_ie_extn(struct sk_buff *skb,
				 struct ieee80211_bss_extn *bss_extn,
				 enum ieee80211_conn_mode mode);

void
ieee80211_240mhz_cap_to_eht_cap_extn(struct ieee80211_sub_if_data *sdata,
				     struct link_sta_info *link_sta,
				     struct ieee80211_supported_band *sband,
				     const struct
				     ieee80211_240mhz_vendor_oper_extn
				     *eht_240mhz_cap);

void
ieee80211_inform_bss_extn(struct cfg80211_bss *cbss,
			  struct ieee802_11_elems *elems);

int ieee802_11_parse_elems_vendor_qcn_extn(const u8 *pos, u8 elen,
					   struct ieee802_11_elems *elems);

int
ieee802_11_determine_ap_chan_extn(const struct ieee802_11_elems *elems,
				  struct cfg80211_chan_def *chandef,
				  struct ieee80211_sub_if_data *sdata);

size_t
ieee802_11_qcn_ie_len_extn(struct cfg80211_bss *cbss,
			   struct ieee80211_local *local);

struct ieee80211_240mhz_vendor_oper_extn*
drv_get_240mhz_cap_extn(struct ieee80211_local *local,
			struct ieee80211_vif *vif,
			struct ieee80211_link_sta *link_sta);

void
ieee80211_bss_240mhz_to_sta_eht_cap_extn(struct ieee80211_sub_if_data *sdata,
					 struct link_sta_info *link_sta,
					 struct ieee80211_supported_band *sband,
					 struct cfg80211_bss *cbss);

void
ieee80211_update_eht_cap_from_240mhz_nl(struct ieee80211_sub_if_data *sdata,
					struct link_sta_info *link_sta,
					struct ieee80211_supported_band *sband);

void ieee80211_modify_bw_limit_for_240mhz(bool is_5ghz,
					  struct ieee80211_conn_settings *conn);

#endif /* CPTCFG_QCN_EXTN */

/**
 * struct ieee80211_ops_extn - extension callbacks from mac80211 to the driver
 *
 * This structure contains various callbacks that the driver may
 * handle or, in some cases, must handle, for example to configure
 * the hardware to a new channel or to transmit a frame.
 *
 * @get_240mhz_cap_extn: Get the 240MHz Capability information of the STA,
 * 	from ath12K ahvif structure to be used by the mac80211 component during
 * 	the association of STA at the AP.
 * 	This 240MHz capability information is obtained from Vendor NL Event from
 * 	hostapd during association of the STA. Use this information to update
 * 	the EHT phy capability info of the STA in the mac80211.
 */
struct ieee80211_ops_extn {
	struct ieee80211_240mhz_vendor_oper_extn*
	(*get_240mhz_cap_extn) (struct ieee80211_vif *vif,
				struct ieee80211_link_sta *link_sta);
};

#endif /* CMN_EXTN_H */
