/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <net/if.h>

#include <sys/types.h>

#include <linux/sockios.h>
#include <linux/nl80211.h>

#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/socket.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>

#include <ev.h>

#include <syslog.h>
#include <libubox/avl-cmp.h>

#include <libubox/avl.h>
#include <libubox/vlist.h>
#include <unl.h>

#include "target.h"
#include "nl80211.h"
#include "phy.h"
#include "utils.h"
#include "radio.h"

extern struct ev_loop *wifihal_evloop;
static int nl80211_scan_started;
static struct unl unl;
static ev_io unl_io;

struct nl80211_scan {
	char name[IF_NAMESIZE];
	target_scan_cb_t *scan_cb;
	void *scan_ctx;
	struct avl_node avl;
	ev_async async;
};

static struct avl_tree nl80211_scan_tree = AVL_TREE_INIT(nl80211_scan_tree, avl_strcmp, false, NULL);

static int nl80211_chainmask_recv(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	unsigned int *mask = (unsigned int *)arg;

	memset(tb, 0, sizeof(tb));
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_WIPHY_ANTENNA_TX])
		*mask = nla_get_u32(tb[NL80211_ATTR_WIPHY_ANTENNA_TX]);

	return NL_OK;
}

static int nl80211_channel_recv(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	unsigned int *chan = (unsigned int *)arg;

	memset(tb, 0, sizeof(tb));
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_WIPHY_FREQ]) {
		*chan = ieee80211_frequency_to_channel(nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]));
	}

	return NL_OK;
}

static int nl80211_interface_recv(struct nl_msg *msg, void *arg)
{
        struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
        struct nl_call_param *nl_call_param = (struct nl_call_param *)arg;
        struct nlattr *tb[NL80211_ATTR_MAX + 1];
        ssid_list_t *ssid_entry = NULL;

        nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                  genlmsg_attrlen(gnlh, 0), NULL);

	if ((tb[NL80211_ATTR_SSID]) && (tb[NL80211_ATTR_IFNAME])) {
                ssid_entry = malloc(sizeof(ssid_list_t));
                strncpy(ssid_entry->ssid, nla_data(tb[NL80211_ATTR_SSID]), sizeof(ssid_entry->ssid));
                strncpy(ssid_entry->ifname, nla_data(tb[NL80211_ATTR_IFNAME]), sizeof(ssid_entry->ifname));
                ds_dlist_insert_tail(nl_call_param->list, ssid_entry);
        }
        return NL_OK;
}

static int nl80211_assoclist_recv(struct nl_msg *msg, void *arg)
{
	static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32    },
		[NL80211_STA_INFO_RX_PACKETS]    = { .type = NLA_U32    },
		[NL80211_STA_INFO_TX_PACKETS]    = { .type = NLA_U32    },
		[NL80211_STA_INFO_RX_BITRATE]    = { .type = NLA_NESTED },
		[NL80211_STA_INFO_TX_BITRATE]    = { .type = NLA_NESTED },
		[NL80211_STA_INFO_SIGNAL]        = { .type = NLA_U8     },
		[NL80211_STA_INFO_RX_BYTES]      = { .type = NLA_U32    },
		[NL80211_STA_INFO_TX_BYTES]      = { .type = NLA_U32    },
		[NL80211_STA_INFO_TX_RETRIES]    = { .type = NLA_U32    },
		[NL80211_STA_INFO_TX_FAILED]     = { .type = NLA_U32    },
		[NL80211_STA_INFO_RX_DROP_MISC]  = { .type = NLA_U64    },
		[NL80211_STA_INFO_T_OFFSET]      = { .type = NLA_U64    },
		[NL80211_STA_INFO_STA_FLAGS] =
			{ .minlen = sizeof(struct nl80211_sta_flag_update) },
	};

	static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
		[NL80211_RATE_INFO_BITRATE]      = { .type = NLA_U16    },
		[NL80211_RATE_INFO_MCS]          = { .type = NLA_U8     },
		[NL80211_RATE_INFO_40_MHZ_WIDTH] = { .type = NLA_FLAG   },
		[NL80211_RATE_INFO_SHORT_GI]     = { .type = NLA_FLAG   },
	};

	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nl_call_param *nl_call_param = (struct nl_call_param *)arg;
	struct nlattr *rinfo[NL80211_RATE_INFO_MAX + 1];
	struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	target_client_record_t *client_entry;

	memset(tb, 0, sizeof(tb));
	memset(sinfo, 0, sizeof(sinfo));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_MAC] ||
	    !tb[NL80211_ATTR_STA_INFO] ||
		nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
				 tb[NL80211_ATTR_STA_INFO], stats_policy)) {
		LOGE("%s: invalid assoc entry", nl_call_param->ifname);
		return NL_OK;
	}
	client_entry = target_client_record_alloc();
	client_entry->info.type = nl_call_param->type;
	memcpy(client_entry->info.mac, nla_data(tb[NL80211_ATTR_MAC]), ETH_ALEN);

	if (sinfo[NL80211_STA_INFO_TX_BYTES])
		client_entry->stats.bytes_tx = nla_get_u32(sinfo[NL80211_STA_INFO_TX_BYTES]);
	if (sinfo[NL80211_STA_INFO_RX_BYTES])
		client_entry->stats.bytes_rx = nla_get_u32(sinfo[NL80211_STA_INFO_RX_BYTES]);
	if (sinfo[NL80211_STA_INFO_RX_BITRATE] &&
		!nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX, sinfo[NL80211_STA_INFO_RX_BITRATE],
				  rate_policy)) {
			if (rinfo[NL80211_RATE_INFO_BITRATE32])
				client_entry->stats.rate_rx = nla_get_u32(rinfo[NL80211_RATE_INFO_BITRATE32]) * 100;
			else if (rinfo[NL80211_RATE_INFO_BITRATE])
				client_entry->stats.rate_rx = nla_get_u32(rinfo[NL80211_RATE_INFO_BITRATE]) * 100;
	}
	if (sinfo[NL80211_STA_INFO_TX_BITRATE] &&
		!nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX, sinfo[NL80211_STA_INFO_TX_BITRATE],
				  rate_policy)) {
			if (rinfo[NL80211_RATE_INFO_BITRATE32])
				client_entry->stats.rate_tx = nla_get_u32(rinfo[NL80211_RATE_INFO_BITRATE32]) * 100;
			else if (rinfo[NL80211_RATE_INFO_BITRATE])
				client_entry->stats.rate_tx = nla_get_u32(rinfo[NL80211_RATE_INFO_BITRATE]) * 100;
		}
	if (sinfo[NL80211_STA_INFO_SIGNAL])
		client_entry->stats.rssi = (signed char)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);
	if (sinfo[NL80211_STA_INFO_TX_PACKETS])
		client_entry->stats.frames_tx = nla_get_u32(sinfo[NL80211_STA_INFO_TX_PACKETS]);
	if (sinfo[NL80211_STA_INFO_RX_PACKETS])
		client_entry->stats.frames_rx = nla_get_u32(sinfo[NL80211_STA_INFO_RX_PACKETS]);
	if (sinfo[NL80211_STA_INFO_TX_RETRIES])
		client_entry->stats.retries_tx = nla_get_u32(sinfo[NL80211_STA_INFO_TX_RETRIES]);
	if (sinfo[NL80211_STA_INFO_TX_FAILED])
		client_entry->stats.errors_tx = nla_get_u32(sinfo[NL80211_STA_INFO_TX_FAILED]);
	if (sinfo[NL80211_STA_INFO_RX_DROP_MISC])
		client_entry->stats.errors_rx = nla_get_u64(sinfo[NL80211_STA_INFO_RX_DROP_MISC]);

	ds_dlist_insert_tail(nl_call_param->list, client_entry);

	return NL_OK;
}

static int nl80211_survey_recv(struct nl_msg *msg, void *arg)
{
	static struct nla_policy sp[NL80211_SURVEY_INFO_MAX + 1] = {
		[NL80211_SURVEY_INFO_FREQUENCY]		= { .type = NLA_U32 },
		[NL80211_SURVEY_INFO_TIME]		= { .type = NLA_U64 },
		[NL80211_SURVEY_INFO_TIME_TX]		= { .type = NLA_U64 },
		[NL80211_SURVEY_INFO_TIME_RX]		= { .type = NLA_U64 },
		[NL80211_SURVEY_INFO_TIME_BUSY]		= { .type = NLA_U64 },
		[NL80211_SURVEY_INFO_TIME_EXT_BUSY]	= { .type = NLA_U64 },
		[NL80211_SURVEY_INFO_TIME_SCAN]		= { .type = NLA_U64 },
		[NL80211_SURVEY_INFO_NOISE]		= { .type = NLA_U8 },
	};

	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nl_call_param *nl_call_param = (struct nl_call_param *)arg;
	struct nlattr *si[NL80211_SURVEY_INFO_MAX + 1];
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	target_survey_record_t  *survey_record;

	memset(tb, 0, sizeof(tb));
	memset(si, 0, sizeof(si));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_SURVEY_INFO])
		return NL_OK;

	if (nla_parse_nested(si, NL80211_SURVEY_INFO_MAX,
			     tb[NL80211_ATTR_SURVEY_INFO], sp))
		return NL_SKIP;

	survey_record = target_survey_record_alloc();
	if (si[NL80211_SURVEY_INFO_FREQUENCY])
		survey_record->info.chan = ieee80211_frequency_to_channel(
						nla_get_u32(si[NL80211_SURVEY_INFO_FREQUENCY]));

	if (si[NL80211_SURVEY_INFO_TIME_RX])
		survey_record->chan_self = nla_get_u64(si[NL80211_SURVEY_INFO_TIME_RX]);

	if (si[NL80211_SURVEY_INFO_TIME_TX])
		survey_record->chan_tx = nla_get_u64(si[NL80211_SURVEY_INFO_TIME_TX]);

	if (si[NL80211_SURVEY_INFO_TIME_RX])
		survey_record->chan_rx = nla_get_u64(si[NL80211_SURVEY_INFO_TIME_RX]);

	if (si[NL80211_SURVEY_INFO_TIME_BUSY])
		survey_record->chan_busy = nla_get_u64(si[NL80211_SURVEY_INFO_TIME_BUSY]);

	if (si[NL80211_SURVEY_INFO_TIME_EXT_BUSY])
		survey_record->chan_busy_ext = nla_get_u64(si[NL80211_SURVEY_INFO_TIME_EXT_BUSY]);

	if (si[NL80211_SURVEY_INFO_TIME])
		survey_record->duration_ms = nla_get_u64(si[NL80211_SURVEY_INFO_TIME]);

	if (si[NL80211_SURVEY_INFO_NOISE])
		survey_record->chan_noise = nla_get_u8(si[NL80211_SURVEY_INFO_NOISE]);

	ds_dlist_insert_tail(nl_call_param->list, survey_record);

	return NL_OK;
}

static int nl80211_scan_dump_recv(struct nl_msg *msg, void *arg)
{
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_TSF]                  = { .type = NLA_U64 },
		[NL80211_BSS_FREQUENCY]            = { .type = NLA_U32 },
		[NL80211_BSS_BSSID]                = { 0 },
		[NL80211_BSS_BEACON_INTERVAL]      = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY]           = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { 0 },
		[NL80211_BSS_SIGNAL_MBM]           = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC]        = { .type = NLA_U8  },
		[NL80211_BSS_STATUS]               = { .type = NLA_U32 },
		[NL80211_BSS_SEEN_MS_AGO]          = { .type = NLA_U32 },
		[NL80211_BSS_BEACON_IES]           = { 0 },
        };
	struct nl_call_param *nl_call_param = (struct nl_call_param *)arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	dpp_neighbor_record_list_t *neighbor;

	memset(tb, 0, sizeof(tb));
	memset(bss, 0, sizeof(bss));
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_BSS] ||
	    nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy) ||
	    !bss[NL80211_BSS_BSSID])
		return NL_OK;

	neighbor = dpp_neighbor_record_alloc();
	neighbor->entry.type = nl_call_param->type;

	if (bss[NL80211_BSS_TSF])
		neighbor->entry.tsf = nla_get_u64(bss[NL80211_BSS_TSF]);
	if (bss[NL80211_BSS_FREQUENCY])
		neighbor->entry.chan =
			ieee80211_frequency_to_channel((int)nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
	if (bss[NL80211_BSS_SIGNAL_MBM])
		neighbor->entry.sig = ((int) nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM])) / 100;
	if (bss[NL80211_BSS_SEEN_MS_AGO])
		neighbor->entry.lastseen = nla_get_u32(bss[NL80211_BSS_SEEN_MS_AGO]);
	if (bss[NL80211_BSS_BSSID])
		print_mac(neighbor->entry.bssid, nla_data(bss[NL80211_BSS_BSSID]));
	if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
		int ielen = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
		unsigned char *ie = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
		int len;

		while (ielen >= 2 && ielen >= ie[1]) {
			switch (ie[0]) {
			case 0: /* SSID */
			case 114: /* Mesh ID */
				len = min(ie[1], 32 + 1);
				memcpy(neighbor->entry.ssid, ie + 2, len);
				neighbor->entry.ssid[len] = 0;
				break;
			}

			ielen -= ie[1] + 2;
			ie += ie[1] + 2;
		}
	}

	ds_dlist_insert_tail(nl_call_param->list, neighbor);
	return NL_OK;

}

static int nl80211_scan_trigger_recv(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static int nl80211_scan_abort_recv(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static struct nl_msg *nl80211_call_phy(char *name, int cmd, bool dump)
{
	struct nl_msg *msg;
	int idx = phy_lookup(name);

	if (idx < 0)
		return NULL;

	msg = unl_genl_msg(&unl, cmd, dump);
	nla_put_u32(msg, NL80211_ATTR_WIPHY, idx);

	return msg;
}

struct nl80211_scan *nl80211_scan_find(const char *name)
{
	struct nl80211_scan *nl80211_scan = avl_find_element(&nl80211_scan_tree, name, nl80211_scan, avl);

	if (!nl80211_scan)
		LOGN("%s: scan context does not exist", name);

	return nl80211_scan;
}

static int nl80211_scan_add(char *name, target_scan_cb_t *scan_cb, void *scan_ctx)
{
	struct nl80211_scan *nl80211_scan = avl_find_element(&nl80211_scan_tree, name, nl80211_scan, avl);

	if (!nl80211_scan) {
		nl80211_scan = malloc(sizeof(*nl80211_scan));
		if (!nl80211_scan)
			return -1;
		memset(nl80211_scan, 0, sizeof(*nl80211_scan));
		strncpy(nl80211_scan->name, name, IF_NAMESIZE);
		nl80211_scan->avl.key = nl80211_scan->name;
		avl_insert(&nl80211_scan_tree, &nl80211_scan->avl);
		LOGN("%s: added scan context", name);
	}

	nl80211_scan->scan_cb = scan_cb;
	nl80211_scan->scan_ctx = scan_ctx;
	return 0;
}

static void nl80211_scan_del(struct nl80211_scan *nl80211_scan)
{
	LOGN("%s: delete scan context", nl80211_scan->name);
	ev_async_stop(EV_DEFAULT, &nl80211_scan->async);
	avl_delete(&nl80211_scan_tree, &nl80211_scan->avl);
	free(nl80211_scan);
}

static void nl80211_scan_finish(char *name, bool state)
{
	struct nl80211_scan *nl80211_scan = nl80211_scan_find(name);

	if (nl80211_scan) {
		LOGN("%s: calling context cb", nl80211_scan->name);
		(*nl80211_scan->scan_cb)(nl80211_scan->scan_ctx, state);
		nl80211_scan_del(nl80211_scan);
	}
}

static int nl80211_recv(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	char ifname[IFNAMSIZ] = {};
	int ifidx = -1;

	memset(tb, 0, sizeof(tb));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX]) {
		ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);
		if_indextoname(ifidx, ifname);
	} else if (tb[NL80211_ATTR_IFNAME]) {
		strncpy(ifname, nla_get_string(tb[NL80211_ATTR_IFNAME]), IFNAMSIZ);
	}

	switch (gnlh->cmd) {
	case NL80211_CMD_TRIGGER_SCAN:
		LOGN("%s: scan started\n", ifname);
		break;
	case NL80211_CMD_SCAN_ABORTED:
		LOGN("%s: scan aborted\n", ifname);
		nl80211_scan_finish(ifname, false);
		break;
	case NL80211_CMD_NEW_SCAN_RESULTS:
		LOGN("%s: scan completed\n", ifname);
		nl80211_scan_finish(ifname, true);
		break;
	default:
		break;
	}
	return NL_OK;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
        return NL_SKIP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	return NL_SKIP;
}

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static void nl80211_ev(struct ev_loop *ev, struct ev_io *io, int event)
{
	struct nl_cb *cb;

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, NULL);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, NULL);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nl80211_recv, NULL);
	nl_recvmsgs(unl.sock, cb);
	nl_cb_put(cb);
}

static struct nl_msg *nl80211_call_vif(struct nl_call_param *nl_call_param, int cmd, bool dump)
{
	int idx = if_nametoindex(nl_call_param->ifname);
	struct nl_msg *msg;

	if (!nl80211_scan_started) {
		unl_genl_subscribe(&unl, "scan");

		ev_io_init(&unl_io, nl80211_ev, unl.sock->s_fd, EV_READ);
		ev_io_start(wifihal_evloop, &unl_io);
		nl80211_scan_started = 1;
	}

	if (!idx)
		return NULL;

	msg = unl_genl_msg(&unl, cmd, dump);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);

	return msg;
}

int nl80211_get_tx_chainmask(char *name, unsigned int *mask)
{
	struct nl_msg *msg = nl80211_call_phy(name, NL80211_CMD_GET_WIPHY, false);

	if (!msg)
		return -1;

	return unl_genl_request(&unl, msg, nl80211_chainmask_recv, mask);
}

int nl80211_get_oper_channel(char *name, unsigned int *chan)
{
	struct nl_msg *msg;
	int idx = if_nametoindex(name);

	if (!idx)
		return -1;

	msg = unl_genl_msg(&unl, NL80211_CMD_GET_INTERFACE, true);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);

	return unl_genl_request(&unl, msg, nl80211_channel_recv, chan);
}

int nl80211_get_ssid(struct nl_call_param *nl_call_param)
{
       struct nl_msg *msg = nl80211_call_vif(nl_call_param, NL80211_CMD_GET_INTERFACE, true);

        if (!msg)
                return -1;

        return unl_genl_request(&unl, msg, nl80211_interface_recv, nl_call_param);
}

int nl80211_get_assoclist(struct nl_call_param *nl_call_param)
{
	struct nl_msg *msg = nl80211_call_vif(nl_call_param, NL80211_CMD_GET_STATION, true);

	if (!msg)
		return -1;

	return unl_genl_request(&unl, msg, nl80211_assoclist_recv, nl_call_param);
}

int nl80211_get_survey(struct nl_call_param *nl_call_param)
{
	struct nl_msg *msg = nl80211_call_vif(nl_call_param, NL80211_CMD_GET_SURVEY, true);

	if (!msg)
		return -1;

	return unl_genl_request(&unl, msg, nl80211_survey_recv, nl_call_param);
}

int nl80211_scan_trigger(struct nl_call_param *nl_call_param, uint32_t *chan_list, uint32_t chan_num,
			 int dwell_time, radio_scan_type_t scan_type,
			 target_scan_cb_t *scan_cb, void *scan_ctx)
{
	struct nl_msg *msg = nl80211_call_vif(nl_call_param, NL80211_CMD_TRIGGER_SCAN, false);
	struct nlattr *freq;
	unsigned int i;

	if (!msg)
		return -1;

	LOGN("%s: not setting dwell time\n", nl_call_param->ifname);
	//nla_put_u16(msg, NL80211_ATTR_MEASUREMENT_DURATION, dwell_time);
	freq = nla_nest_start(msg, NL80211_ATTR_SCAN_FREQUENCIES);
	for (i = 0; i < chan_num; i ++)
		nla_put_u32(msg, i, ieee80211_channel_to_frequency(chan_list[i]));
	nla_nest_end(msg, freq);

	if (nl80211_scan_add(nl_call_param->ifname, scan_cb, scan_ctx))
		return -1;

	return unl_genl_request(&unl, msg, nl80211_scan_trigger_recv, NULL);
}

int nl80211_scan_abort(struct nl_call_param *nl_call_param)
{
	struct nl_msg *msg = nl80211_call_vif(nl_call_param, NL80211_CMD_ABORT_SCAN, false);
	struct nl80211_scan *nl80211_scan = nl80211_scan_find(nl_call_param->ifname);

	if (!msg)
		return -1;

	if (nl80211_scan)
		nl80211_scan_del(nl80211_scan);

	return unl_genl_request(&unl, msg, nl80211_scan_abort_recv, NULL);
}

int nl80211_scan_dump(struct nl_call_param *nl_call_param)
{
	struct nl_msg *msg = nl80211_call_vif(nl_call_param, NL80211_CMD_GET_SCAN, true);

	if (!msg)
		return -1;

	return unl_genl_request(&unl, msg, nl80211_scan_dump_recv, nl_call_param);
}

int stats_nl80211_init(void)
{
	if (unl_genl_init(&unl, "nl80211") < 0) {
		LOGE("failed to spawn nl80211");
		return -1;
	}
	return 0;
}
