/*
* Copyright (c) 2011-2012 Qualcomm Atheros Inc. All Rights Reserved.
* Qualcomm Atheros Proprietary and Confidential.
*/

#ifndef _NL80211_DRV_H_
#define _NL80211_DRV_H_

#include <netlink/socket.h>

#if !defined(WIN_AP_HOST) && !defined(MDM)
#ifndef sockaddr_storage
#define sockaddr_storage __kernel_sockaddr_storage
#endif
#endif

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <linux/nl80211.h>
#include <netinet/in.h>
#include "libtcmd.h"

int nl80211_init(struct tcmd_cfg *cfg);
int nl80211_tcmd_tx(struct tcmd_cfg *cfg, void *buf, int len);
int nl80211_tcmd_rx(struct tcmd_cfg *cfg);
int nl80211_tcmd_start(struct tcmd_cfg *cfg);
int nl80211_tcmd_stop(struct tcmd_cfg *cfg);
#ifndef CONFIG_AR6002_REV6
int nl80211_set_ep(uint32_t *driv_ep, enum tcmd_ep ep);
#endif
#endif /* _NL80211_DRV_H_ */
