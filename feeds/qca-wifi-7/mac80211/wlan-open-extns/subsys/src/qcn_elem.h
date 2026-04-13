/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef QCN_ELEM_H
#define QCN_ELEM_H

#define WLAN_OUI_QCN                           0x8CFDF0
#define WLAN_OUI_QCN_TYPE_1                    1
#define WLAN_OUI_QCN_HEADER_LEN                4 /* WLAN_OUI_QCN + type */
#define WLAN_OUI_QCN_ATTR_VER                  1
#define WLAN_OUI_QCN_ATTR_5GHZ_240MHZ_SUPP     0x0B

#define IEEE80211_EHT_240MHZ_PHY_SOUNDING_DIM_320MHZ_MASK       BIT(0) | BIT(1)

struct ieee80211_qcn_version_vendor_elem_extn {
	u8 version;
	u8 subversion;
}__packed;

#endif /* QCN_ELEM_H */

