/* SPDX-License-Identifier BSD-3-Clause */
#include <linux/netlink.h>

#ifndef __KERNEL__
#include <sys/socket.h>

#include <net/if.h>

#include <sys/types.h>

#include <linux/sockios.h>

#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#endif



#define WC_CAPT_BUF_SIZE        500

struct wc_capture_buf
{
    uint64_t TimeStamp;
    uint64_t tsInUs;
    uint64_t SessionId;
    unsigned int Type;
    unsigned int From;
    unsigned int Len;
    unsigned int Channel;
    unsigned int Direction;
    int Rssi;
    unsigned int DataRate;
    unsigned int Count;
    int wifiIf;         // for dhcp
    char staMac[6];     // for dhcp
    unsigned char Buffer[WC_CAPT_BUF_SIZE];
};


#define GENL_UCC_FAMILY_NAME		"genl_ucc"
#define GENL_UCC_MCGRP0_NAME		"genl_mcgrp0"
#define GENL_UCC_MCGRP1_NAME		"genl_mcgrp1"
#define GENL_UCC_MCGRP2_NAME		"genl_mcgrp2"

enum genl_ucc_multicast_groups {
	GENL_UCC_MCGRP0,
	GENL_UCC_MCGRP1,
	GENL_UCC_MCGRP2,
};

#define GENL_UCC_MCGRP_MAX		3

static char* genl_ucc_mcgrp_names[GENL_UCC_MCGRP_MAX] = {
	GENL_UCC_MCGRP0_NAME,
	GENL_UCC_MCGRP1_NAME,
	GENL_UCC_MCGRP2_NAME,
};

enum genl_ucc_attrs {
	GENL_UCC_ATTR_UNSPEC,		/* Must NOT use element 0 */

	GENL_UCC_ATTR_MSG,

	__GENL_UCC_ATTR__MAX,
};
#define GENL_UCC_ATTR_MAX (__GENL_UCC_ATTR__MAX - 1)


#define GENL_UCC_ATTR_MSG_MAX		256

enum {
	GENL_UCC_C_UNSPEC,		/* Must NOT use element 0 */
	GENL_UCC_C_MSG,
};



static struct nla_policy genl_ucc_policy[GENL_UCC_ATTR_MAX+1] = {
	[GENL_UCC_ATTR_MSG] = {
		.type = NLA_UNSPEC,
#ifdef __KERNEL__
		.len = sizeof(struct wc_capture_buf)
#else
		.maxlen = sizeof(struct wc_capture_buf)
#endif
	},
};


