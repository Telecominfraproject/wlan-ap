/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <net/mac80211.h>
#include "../ieee80211_i.h"
#include "../sta_info.h"
#include "cmn_extn.h"
#include "qcn_elem.h"


void
ieee80211_bss_240mhz_to_sta_eht_cap_extn(struct ieee80211_sub_if_data *sdata,
					 struct link_sta_info *link_sta,
					 struct ieee80211_supported_band *sband,
					 struct cfg80211_bss *cbss)
{
	struct ieee80211_bss *bss = (void *)cbss->priv;

	ieee80211_240mhz_cap_to_eht_cap_extn(sdata, link_sta, sband,
				bss->bss_extn.params_240MHz.eht_240mhz_cap);

	link_sta->cur_max_bandwidth = ieee80211_sta_cap_rx_bw(link_sta);
	link_sta->pub->bandwidth = ieee80211_sta_cur_vht_bw(link_sta);
	link_sta->pub->sta_max_bandwidth = link_sta->cur_max_bandwidth;
}

void
ieee80211_update_eht_cap_from_240mhz_nl(struct ieee80211_sub_if_data *sdata,
					struct link_sta_info *link_sta,
					struct ieee80211_supported_band *sband)
{
	struct ieee80211_240mhz_vendor_oper_extn *eht_240mhz_cap =
		drv_get_240mhz_cap_extn(sdata->local, &sdata->vif,
					link_sta->pub);

	ieee80211_240mhz_cap_to_eht_cap_extn(sdata, link_sta, sband,
					     eht_240mhz_cap);
}

void
ieee80211_240mhz_cap_to_eht_cap_extn(struct ieee80211_sub_if_data *sdata,
				     struct link_sta_info *link_sta,
				     struct ieee80211_supported_band *sband,
				     const struct
				     ieee80211_240mhz_vendor_oper_extn
				     *eht_240mhz_cap)
{
	u8 *phy_cap;
	struct ieee80211_eht_mcs_nss_supp_bw *bw_320;
	struct ieee80211_sta_eht_cap *eht_cap = &link_sta->pub->eht_cap;

	if (sband->band != NL80211_BAND_5GHZ)
		return;

	if (!eht_240mhz_cap || !eht_240mhz_cap->is5ghz240mhz)
		return;

	phy_cap = eht_cap->eht_cap_elem.phy_cap_info;
	bw_320 = &eht_cap->eht_mcs_nss_supp.bw._320;

	/* Override capabilities from QCN IE for 240MHz to EHT phy capab */
	phy_cap[0] = u8_replace_bits(
		     phy_cap[0],
		     eht_240mhz_cap->is5ghz240mhz,
		     IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ);

	if (eht_240mhz_cap->bfmess320mhz)
		phy_cap[1] = u8_replace_bits(
			     phy_cap[1],
			     eht_240mhz_cap->bfmess320mhz,
			     IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_320MHZ_MASK);

	if (eht_240mhz_cap->numsound320mhz) {
		phy_cap[2] = u8_replace_bits(
			     phy_cap[2], eht_240mhz_cap->numsound320mhz,
			     IEEE80211_EHT_240MHZ_PHY_SOUNDING_DIM_320MHZ_MASK);

		phy_cap[3] = u8_replace_bits(
			     phy_cap[3],
			     eht_240mhz_cap->numsound320mhz >> 2,
			     IEEE80211_EHT_PHY_CAP3_SOUNDING_DIM_320MHZ_MASK);
	}

	if (eht_240mhz_cap->nonofdmaulmumimo320mhz)
		phy_cap[7] = u8_replace_bits(
			     phy_cap[7],
			     eht_240mhz_cap->nonofdmaulmumimo320mhz,
			    IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_320MHZ);

	if (eht_240mhz_cap->mubfmr320mhz)
		phy_cap[7] = u8_replace_bits(
			     phy_cap[7], eht_240mhz_cap->mubfmr320mhz,
			     IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_320MHZ);

	memcpy(bw_320, &eht_240mhz_cap->mcs_map_320mhz,
	       sizeof(struct ieee80211_eht_mcs_nss_supp_bw));
}

int ieee802_11_parse_elems_vendor_qcn_extn(const u8 *pos, u8 elen,
					   struct ieee802_11_elems *elems)
{
	u8 rem_elen;
	const u8 *end;

	if (elen <= 6 || pos[3] != WLAN_OUI_QCN_TYPE_1)
		return -1;

	if (!(pos[0] == 0x8c && pos[1] == 0xfd &&
	    pos[2] == 0xf0)) /* WLAN_OUI_QCN */
		return -1;

	end = pos + elen;
	rem_elen = elen - 4;
	pos += 4;

	while ((rem_elen > 2) && (pos < end)) {
		switch (pos[0]) {
			case WLAN_OUI_QCN_ATTR_5GHZ_240MHZ_SUPP:
				if (rem_elen < 11)
					break;
				elems->elems_extn.params_240MHz.eht_240mhz_cap
					= (struct
					   ieee80211_240mhz_vendor_oper_extn *)
					  (pos + 2);
				elems->elems_extn.params_240MHz.eht_240mhz_len
					= pos[1];
				break;
			default:
				break;
		}

		rem_elen -= (2 + pos[1]);
		pos += (2 + pos[1]);
	}
	return 0;
}

static u8 *ieee80211_add_qcn_version_vendor_ie_extn(u8 *buf)
{
	*buf++ = WLAN_OUI_QCN_ATTR_VER; /* OUI QCN Attribute version */
	*buf++ = 2; /* length */
	*buf++ = 1; /* version */
	*buf++ = 0; /* sub-version */

	return buf;
}

static u8 *
ieee80211_add_240mhz_vendor_ie_extn(u8 *buf,
				    const struct
				    ieee80211_240mhz_vendor_oper_extn
				    *eht_240mhz_capab)
{
	if (!eht_240mhz_capab)
		return buf;

	*buf++ = WLAN_OUI_QCN_ATTR_5GHZ_240MHZ_SUPP;
	*buf++ = sizeof(struct ieee80211_240mhz_vendor_oper_extn);
	memcpy(buf, (u8 *) eht_240mhz_capab,
	       sizeof(struct ieee80211_240mhz_vendor_oper_extn));
	buf += sizeof(struct ieee80211_240mhz_vendor_oper_extn);

	return buf;
}

u8 *
ieee80211_add_qcn_vendor_ie_extn(struct sk_buff *skb,
				 struct ieee80211_bss_extn *bss_extn,
				 enum ieee80211_conn_mode mode)
{
	struct ieee80211_240mhz_vendor_oper_extn *eht_240mhz_capab;
	size_t len;
	u8 *pos;

	if (mode < IEEE80211_CONN_MODE_EHT)
		return NULL;

	eht_240mhz_capab = bss_extn->params_240MHz.eht_240mhz_len ?
			   bss_extn->params_240MHz.eht_240mhz_cap :
			   NULL;

	if (!eht_240mhz_capab)
		return NULL;

	/* len: QCN Header (4) + QCN version attribute ID (1) +
	 * 	QCN version attribute len (1) + size of QCN version attribute (2) +
	 * 	QCN 240MHz attribute ID (1) + QCN 240MHz attribute len (1) +
	 * 	size of QCN 240MHz attribute (9)
	 */
	len = WLAN_OUI_QCN_HEADER_LEN + 2 +
	      sizeof(struct ieee80211_240mhz_vendor_oper_extn) +
	      2 + sizeof(struct ieee80211_qcn_version_vendor_elem_extn);

	pos = skb_put(skb, 2 + len);
	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = len;
	*pos++ = 0x8c; /* WLAN_OUI_QCN */
	*pos++ = 0xfd;
	*pos++ = 0xf0;
	*pos++ = WLAN_OUI_QCN_TYPE_1;

	pos = ieee80211_add_qcn_version_vendor_ie_extn(pos);
	pos = ieee80211_add_240mhz_vendor_ie_extn(pos, eht_240mhz_capab);

	return pos;
}

int
ieee802_11_determine_ap_chan_extn(const struct ieee802_11_elems *elems,
				  struct cfg80211_chan_def *chandef,
				  struct ieee80211_sub_if_data *sdata)
{
	const struct ieee80211_eht_operation *eht_oper = elems->eht_operation;
	const struct ieee80211_240mhz_vendor_oper_extn *eht_240mhz_capab
		= elems->elems_extn.params_240MHz.eht_240mhz_cap;

	if (eht_oper && eht_240mhz_capab &&
	    chandef->width == NL80211_CHAN_WIDTH_160) {
		struct cfg80211_chan_def eht_chandef = *chandef;

		eht_chandef.width = NL80211_CHAN_WIDTH_320;
		eht_chandef.center_freq1 =
			ieee80211_channel_to_frequency(eht_240mhz_capab->ccfs0,
						       chandef->chan->band);
		eht_chandef.punctured = eht_240mhz_capab->punctured;

		if (!cfg80211_chandef_valid(&eht_chandef)) {
			sdata_info(sdata,
				   "AP EHT 240 MHz information is invalid, disabling EHT\n");
			return -1;
		}

		if (!cfg80211_chandef_compatible(chandef, &eht_chandef)) {
			sdata_info(sdata,
				   "AP EHT 240 MHz information is incompatible, disabling EHT\n");
			return -1;
		}

		*chandef = eht_chandef;
	}

	return 0;
}

void ieee80211_inform_bss_extn(struct cfg80211_bss *cbss,
			       struct ieee802_11_elems *elems)
{
	struct ieee80211_bss *bss = (void *)cbss->priv;

	if (cbss->channel->band == NL80211_BAND_5GHZ &&
	    elems->elems_extn.params_240MHz.eht_240mhz_len &&
	    elems->elems_extn.params_240MHz.eht_240mhz_cap) {
		bss->bss_extn.params_240MHz.eht_240mhz_len =
			elems->elems_extn.params_240MHz.eht_240mhz_len;
		bss->bss_extn.params_240MHz.eht_240mhz_cap =
			elems->elems_extn.params_240MHz.eht_240mhz_cap;

	}
}

size_t
ieee802_11_qcn_ie_len_extn(struct cfg80211_bss *cbss,
			   struct ieee80211_local *local)
{
	struct ieee80211_bss *bss = (void *)cbss->priv;
	struct ieee80211_supported_band *sband;
	size_t size = 0;

	sband = local->hw.wiphy->bands[cbss->channel->band];

	if (bss->bss_extn.params_240MHz.eht_240mhz_len &&
	    sband->band == NL80211_BAND_5GHZ)
		size = (2 + WLAN_OUI_QCN_HEADER_LEN + 2 +
			 sizeof(struct ieee80211_qcn_version_vendor_elem_extn) +
			 2 + sizeof(struct ieee80211_240mhz_vendor_oper_extn));

	return size;
}

void ieee80211_modify_bw_limit_for_240mhz(bool is_5ghz,
					  struct ieee80211_conn_settings *conn)
{
	if (is_5ghz)
		conn->bw_limit = IEEE80211_CONN_BW_LIMIT_320;
}
