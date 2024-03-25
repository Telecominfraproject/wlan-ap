/*
 * Copyright (c) 2011-2012, 2016-2018, 2021 Qualcomm Technologies Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2011-2012, 2016 Qualcomm Atheros Inc.
 * All rights reserved.
 *
 * $ATH_LICENSE_HOSTSDK0_C$
 *
 * nl80211 code from iw and hwsim tool by Johannes Berg
 * http://git.sipsolutions.net/?p=iw.git;a=summary
 * http://git.sipsolutions.net/?p=hwsim.git;a=summary
 */

#include "libtcmd.h"
#include "os.h"
#ifdef WIN_AP_HOST
#include <netlink/handlers.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#include <linux/errno.h>
#endif

int cb_ret;

#ifdef LIBNL_2
static inline struct nl_sock *nl_handle_alloc(void)
{
	return nl_socket_alloc();
}

static inline void nl_handle_destroy(struct nl_handle *h)
{
	nl_socket_free(h);
}

#define nl_disable_sequence_check nl_socket_disable_seq_check
#endif

/* copied from ath6kl */
enum ar6k_testmode_attr {
	__AR6K_TM_ATTR_INVALID	= 0,
	AR6K_TM_ATTR_CMD	= 1,
	AR6K_TM_ATTR_DATA	= 2,
	AR6K_TM_ATTR_STREAM_ID	= 3,

	/* keep last */
	__AR6K_TM_ATTR_AFTER_LAST,
	AR6K_TM_ATTR_MAX	= __AR6K_TM_ATTR_AFTER_LAST - 1
};

#ifdef WIN_AP_HOST_OPEN
enum ar6k_testmode_cmd {
	AR6K_TM_CMD_VERSION		= 0,
	AR6K_TM_CMD_START		= 1,
	AR6K_TM_CMD_STOP		= 2,
	AR6K_TM_CMD_WMI_CMD		= 3,
	AR6K_TM_CMD_TCMD		= 4,
};
#else
enum ar6k_testmode_cmd {
	AR6K_TM_CMD_TCMD		= 0,
	AR6K_TM_CMD_START		= 1,
	AR6K_TM_CMD_STOP		= 2,
#ifndef CONFIG_AR6002_REV6
	AR6K_TM_CMD_WMI_CMD		= 0xF000,
#endif
};
#endif

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = arg;
	*ret = err->error;

	UNUSED(nla);
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;

	UNUSED(msg);
	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;

	UNUSED(msg);
	return NL_STOP;
}

#ifdef ANDROID
#ifndef in_addr_t
typedef uint32_t in_addr_t;
#endif
#include "netlink-private/genl.h"
/* android's libnl_2 does not include this, define it here */
static int android_genl_ctrl_resolve(struct nl_handle *handle,
				     const char *name)
{
	/*
	 * Android ICS has very minimal genl_ctrl_resolve() implementation, so
	 * need to work around that.
	 */
	struct nl_cache *cache = NULL;
	struct genl_family *nl80211 = NULL;
	int id = -1;

	if (genl_ctrl_alloc_cache(handle, &cache) < 0) {
		A_DBG("nl80211: Failed to allocate generic "
				"netlink cache");
		goto fail;
	}

	nl80211 = genl_ctrl_search_by_name(cache, name);
	if (nl80211 == NULL)
		goto fail;

	id = genl_family_get_id(nl80211);

fail:
	if (nl80211)
		genl_family_put(nl80211);
	if (cache)
		nl_cache_free(cache);

	return id;
}
#define genl_ctrl_resolve android_genl_ctrl_resolve

#define nl_socket_get_cb nl_sk_get_cb
struct nl_cb *nl_socket_get_cb(const struct nl_sock *sk)
{
	return nl_cb_get(sk->s_cb);
}

#define nl_socket_enable_msg_peek nl_sk_enable_msg_peek
void nl_socket_enable_msg_peek(struct nl_sock *sk)
{
	sk->s_flags |= NL_MSG_PEEK;
}

#define nl_socket_set_nonblocking nl_sk_set_nb
int nl_socket_set_nonblocking(const struct nl_sock *sk)
{
	fcntl(sk->s_fd, F_SETFL, O_NONBLOCK);
	return 0;
}

static int seq_ok(struct nl_msg *msg, void *arg)
{
	UNUSED(msg);
	UNUSED(arg);

	return NL_OK;
}

#define nl_socket_disable_seq_check disable_seq_check
static inline void disable_seq_check(struct nl_handle *handle)
{
	nl_cb_set(nl_socket_get_cb(handle), NL_CB_SEQ_CHECK,
		  NL_CB_CUSTOM, seq_ok, NULL);
}
#endif

struct handler_args {
	const char *group;
	int id;
};

static int family_handler(struct nl_msg *msg, void *arg)
{
	struct handler_args *grp = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int rem_mcgrp;

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
		struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
			  nla_data(mcgrp), nla_len(mcgrp), NULL);

		if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
		    !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
			continue;
		else
			grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
		if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]),
				grp->group,
				nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])))
			continue;
		grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	}

	return NL_SKIP;
}

int nl_get_multicast_id(struct nl_handle *sock, const char *family,
			const char *group)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	int ret, ctrlid;
	struct handler_args grp = {
		.group = group,
		.id = -ENOENT,
	};

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		ret = -ENOMEM;
		goto out_fail_cb;
	}

	ctrlid = genl_ctrl_resolve(sock, "nlctrl");

#ifdef ANDROID
	genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, GENL_ID_CTRL, 0, 0,
			CTRL_CMD_GETFAMILY, 1);
#else
	genlmsg_put(msg, 0, 0, ctrlid, 0, 0, CTRL_CMD_GETFAMILY, 0);
#endif

	ret = -ENOBUFS;
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = nl_send_auto_complete(sock, msg);
	if (ret < 0)
		goto out;

	ret = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &ret);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

	while (ret > 0)
		nl_recvmsgs(sock, cb);

	if (ret == 0)
		ret = grp.id;
 nla_put_failure:
 out:
	nl_cb_put(cb);
 out_fail_cb:
	nlmsg_free(msg);
	return ret;
}

#ifndef CONFIG_AR6002_REV6
int nl80211_set_ep(uint32_t *driv_ep, enum tcmd_ep ep)
{
	switch (ep) {
	case TCMD_EP_TCMD:
		*driv_ep = AR6K_TM_CMD_TCMD;
	break;
	case TCMD_EP_WMI:
		*driv_ep = AR6K_TM_CMD_WMI_CMD;
	break;
	default:
		fprintf(stderr, "nl80211: unknown ep!");
		return -1;
	}
	return 0;
}
#endif

/* tcmd rx_cb wrapper to "unpack" the nl80211 msg and call the "real" cb */
int nl80211_rx_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[AR6K_TM_ATTR_MAX + 1];
	void *buf;
	int len;

	UNUSED(arg);
#ifndef WIN_AP_HOST
	A_DBG("nl80211: cb wrapper called\n");
#endif
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA] || !tb[NL80211_ATTR_WIPHY]) {
		printf("no data!\n");
		return NL_SKIP;
	}

	nla_parse(td, AR6K_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		  nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	if (!td[AR6K_TM_ATTR_DATA]) {
#ifndef WIN_AP_HOST_OPEN
		printf("no data in reply\n");
#endif
		return NL_SKIP;
	}

	buf = nla_data(td[AR6K_TM_ATTR_DATA]);
	len = nla_len(td[AR6K_TM_ATTR_DATA]);
#ifndef WIN_AP_HOST
	A_DBG("nl80211: resp received, calling custom cb\n");
#endif
	tcmd_cfg.rx_cb(buf, len);
	/* trip waiting thread */
	tcmd_cfg.timeout = true;

	return NL_SKIP;
}

int nl80211_init(struct tcmd_cfg *cfg)
{
	struct nl_cb *cb;
	int err;
#ifdef WIN_AP_HOST_OPEN
	int opt;
#endif

	if(cfg->nl_handle)
		nl_handle_destroy(cfg->nl_handle);

	cfg->nl_handle = nl_handle_alloc();
	if (!cfg->nl_handle) {
		A_DBG("Failed to allocate netlink socket.\n");
		return -ENOMEM;
	}

	if (genl_connect(cfg->nl_handle)) {
		A_DBG("Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	cfg->nl_id = genl_ctrl_resolve(cfg->nl_handle, "nl80211");
	if (cfg->nl_id < 0) {
		A_DBG("nl80211 not found.\n");
		err = -ENOENT;
		goto out_handle_destroy;
	}

	/* replace this with genl_ctrl_resolve_grp() once we move to libnl3 */
	err = nl_get_multicast_id(cfg->nl_handle, "nl80211", "testmode");
	if (err >= 0) {
		err = nl_socket_add_membership(cfg->nl_handle, err);
		if (err) {
			A_DBG("failed to join testmode group!\n");
			goto out_handle_destroy;
		}
	} else
		goto out_handle_destroy;
	/*
	 * Enable peek mode so drivers can send large amounts
	 * of data in blobs without problems.
	 */
	nl_socket_enable_msg_peek(cfg->nl_handle);

	/*
	 * disable sequence checking to handle events.
	 */
	nl_disable_sequence_check(cfg->nl_handle);

	cb = nl_socket_get_cb(cfg->nl_handle);
#ifdef ANDROID
	/* libnl_2 does not provide default handlers */
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &cb_ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &cb_ret);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &cb_ret);
#endif
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nl80211_rx_cb, NULL);

	/* so we can handle timeouts properly */
	nl_socket_set_nonblocking(cfg->nl_handle);

#ifdef WIN_AP_HOST_OPEN
	/* nobuf errors are useful for identifying lost packets and doing a
	 * resync. Such handling is not performed by libtcmd since we
	 * do a synchronous recv after performing a tx. Since
	 * we are listening in multicast socket and we recv/process the
	 * data only when do a tx, there are chances we receive overrun
	 * or ENOBUF errors which affects our data recv and better to
	 * avoid them
	 */
        opt = 1;
        setsockopt(nl_socket_get_fd(cfg->nl_handle), SOL_NETLINK,
                   NETLINK_NO_ENOBUFS, &opt, sizeof(opt));
#endif

	return 0;

 out_handle_destroy:
	nl_handle_destroy(cfg->nl_handle);
	return err;
}

int nl80211_tcmd_connect(struct tcmd_cfg *cfg, enum ar6k_testmode_cmd cmd )
{
        struct nl_msg *msg;
        struct nlattr *nest;
        int devidx, err = 0;

        /* CHANGE HERE: you may need to allocate larger messages! */
        msg = nlmsg_alloc();
        if (!msg) {
                A_DBG("failed to allocate netlink message\n");
                return 2;
        }

        genlmsg_put(msg, 0, 0, cfg->nl_id, 0,
                    0, NL80211_CMD_TESTMODE, 0);

        devidx = if_nametoindex(cfg->iface);
        if (devidx) {
                NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
        } else {
                A_DBG("Device not found\n");
                err = -ENOENT;
                goto out_free_msg;
        }

        nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
        if (!nest) {
                A_DBG("failed to nest\n");
                err = -1;
                goto out_free_msg;
        }

        NLA_PUT_U32(msg, AR6K_TM_ATTR_CMD, cmd);

        nla_nest_end(msg, nest);

#ifndef WIN_AP_HOST
        A_DBG("nl80211: sending message\n");
#endif
        nl_send_auto_complete(cfg->nl_handle, msg);

 out_free_msg:
        nlmsg_free(msg);
        return err;

 nla_put_failure:
        nlmsg_free(msg);
        A_DBG("building message failed\n");
        return 2;
}

int nl80211_tcmd_start(struct tcmd_cfg *cfg)
{
        return nl80211_tcmd_connect(cfg, AR6K_TM_CMD_START);
}

int nl80211_tcmd_stop(struct tcmd_cfg *cfg)
{
        return nl80211_tcmd_connect(cfg,AR6K_TM_CMD_STOP);
}

int nl80211_tcmd_tx(struct tcmd_cfg *cfg, void *buf, int len)
{
	struct nl_msg *msg;
	struct nlattr *nest;
	int devidx, err = 0;

	/* CHANGE HERE: you may need to allocate larger messages! */
	msg = nlmsg_alloc();
	if (!msg) {
		A_DBG("failed to allocate netlink message\n");
		return 2;
	}

	genlmsg_put(msg, 0, 0, cfg->nl_id, 0,
		    0, NL80211_CMD_TESTMODE, 0);

	devidx = if_nametoindex(cfg->iface);
	if (devidx) {
		NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
	} else {
		A_DBG("Device not found\n");
		err = -ENOENT;
		goto out_free_msg;
	}

	nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!nest) {
		A_DBG("failed to nest\n");
		err = -1;
		goto out_free_msg;
	}

#ifdef CONFIG_AR6002_REV6
	NLA_PUT_U32(msg, AR6K_TM_ATTR_CMD, AR6K_TM_CMD_TCMD);
#else
	NLA_PUT_U32(msg, AR6K_TM_ATTR_CMD, cfg->ep);
#endif
	NLA_PUT(msg, AR6K_TM_ATTR_DATA, len, buf);

	nla_nest_end(msg, nest);

#ifndef WIN_AP_HOST
	A_DBG("nl80211: sending message\n");
#endif
	nl_send_auto_complete(cfg->nl_handle, msg);

 out_free_msg:
	nlmsg_free(msg);
	return err;

 nla_put_failure:
	nlmsg_free(msg);
	A_DBG("building message failed\n");
	return 2;
}

int nl80211_tcmd_rx(struct tcmd_cfg *cfg)
{
	struct nl_cb *cb;
	int err = 0;

	cb = nl_socket_get_cb(cfg->nl_handle);
	if (!cb) {
		fprintf(stderr, "failed to allocate netlink callbacks\n");
		err = 2;
		goto out;
	}

	err = tcmd_set_timer(cfg);
	if (err)
		goto out;

#ifndef WIN_AP_HOST
	A_DBG("nl80211: waiting for response\n");
#endif
	while (!cfg->timeout)
		nl_recvmsgs(cfg->nl_handle, cb);

	if (!cfg->timeout)
		return tcmd_reset_timer(cfg);
	else
		return 0;

out:
	return err;
}
