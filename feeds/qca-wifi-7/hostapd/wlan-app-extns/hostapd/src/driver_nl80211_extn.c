/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "includes.h"
#include <sys/types.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <net/if.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/errqueue.h>
#include "common.h"
#include "eloop.h"
#include "common/qca-vendor.h"
#include "common/qca-vendor-attr.h"
#include "common/brcm_vendor.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "drivers/driver.h"
#include "drivers/driver_nl80211.h"


struct hostapd_sta_add_params;

/* This function sends a NL message only if extension parameters exist;
 * otherwise it just returns.
 */

void wpa_driver_nl80211_sta_add_extn(void *priv,
				     struct hostapd_sta_add_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_240mhz_vendor_oper_extn *oper;
	struct nl_msg *msg;
	struct nlattr *attr;
	int ret;

	/*currently added only for 240mhz operation*/

	if (!params->params_extn.params_240mhz.eht_240mhz_capab)
		return;

	oper = params->params_extn.params_240mhz.eht_240mhz_capab;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO))
		goto fail;


	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto fail;

	/* Add the Vendor-specific attributes*/
	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS,
		       oper->bfmess320mhz) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS,
		       oper->numsound320mhz) ||
	    nla_put(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP, 3,
		    oper->mcs_map_320mhz) ||
	    (oper->nonofdmaulmumimo320mhz &&
	    nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO))
	    || (oper->mubfmr320mhz &&
	    nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER)))
		goto fail;

	nla_nest_end(msg, attr);

	if (nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ1, oper->ccfs0) ||
	    nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ2, oper->ccfs1) ||
	    nla_put_u32(msg, NL80211_ATTR_PUNCT_BITMAP, oper->punct_bitmap))
		goto fail;

	if (params->mld_link_addr) {
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN,
			    params->mld_link_addr))
			goto fail;
	} else {
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN,
			    params->addr))
			goto fail;
	}

	ret = send_and_recv_cmd(drv, msg);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to send 240 MHz Vendor NL command: %s, %d",
			   strerror(-ret), ret);
		goto fail;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: 240MHz Vendor Oper: ccfs0=%u, ccfs1=%u, punctured=0x%04x, "
		   "is5ghz240mhz=%u, bfmess320mhz=%u, numsound320mhz=%u, "
		   "nonofdmaulmumimo320mhz=%u, mubfmr320mhz=%u, "
		   "mcs_map_320mhz=[0x%02x, 0x%02x, 0x%02x]",
		   oper->ccfs0,
		   oper->ccfs1,
		   oper->punct_bitmap,
		   oper->is5ghz240mhz,
		   oper->bfmess320mhz,
		   oper->numsound320mhz,
		   oper->nonofdmaulmumimo320mhz,
		   oper->mubfmr320mhz,
		   oper->mcs_map_320mhz[0],
		   oper->mcs_map_320mhz[1],
		   oper->mcs_map_320mhz[2]);

	return;

fail:
	nlmsg_free(msg);
	return;
}
