/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/mac80211.h>
#include "../ieee80211_i.h"
#include "../trace.h"
#include "../driver-ops.h"
#include "../debugfs_sta.h"
#include "../debugfs_netdev.h"

struct ieee80211_240mhz_vendor_oper_extn*
drv_get_240mhz_cap_extn(struct ieee80211_local *local,
			struct ieee80211_vif *vif,
			struct ieee80211_link_sta *link_sta)
{
	if (local->ops_extn && local->ops_extn->get_240mhz_cap_extn)
		return local->ops_extn->get_240mhz_cap_extn(vif, link_sta);

	return NULL;
}
