// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#define _GNU_SOURCE
#include <linux/nl80211.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unl.h>

#include "atf.h"

static struct unl unl;

static void
atf_parse_tid_stats(struct atf_interface *iface, struct atf_stats *stats,
		    int tid, struct nlattr *attr)
{
	struct nlattr *tb[NL80211_TID_STATS_MAX + 1];
	uint64_t msdu;

	if (nla_parse_nested(tb, NL80211_TID_STATS_MAX, attr, NULL))
		return;

	if (!tb[NL80211_TID_STATS_TX_MSDU])
		return;

	msdu = nla_get_u64(tb[NL80211_TID_STATS_TX_MSDU]);
	switch (tid) {
	case 0:
	case 3:
		/* BE */
		stats->normal += msdu;
		break;
	case 1:
	case 2:
		/* BK */
		stats->bulk += msdu;
		break;
	case 4:
	case 5:
		/* VI */
		stats->prio += msdu;
		break;
	case 6:
	case 7:
		stats->prio += msdu * config.voice_queue_weight;
		/* VO */
		break;
	default:
		break;
	}
}

static uint64_t atf_stats_total(struct atf_stats *stats)
{
	return stats->normal + stats->prio + stats->bulk;
}

static void atf_stats_diff(struct atf_stats *dest, struct atf_stats *cur, struct atf_stats *prev)
{
	dest->normal = cur->normal - prev->normal;
	dest->prio = cur->prio - prev->prio;
	dest->bulk = cur->bulk - prev->bulk;
}

static uint16_t atf_stats_avg(uint16_t avg, uint64_t cur, uint32_t total)
{
	cur <<= ATF_AVG_SCALE;
	cur /= total;

	if (!avg)
		return (uint16_t)cur;

	avg *= ATF_AVG_WEIGHT_FACTOR;
	avg += cur * (ATF_AVG_WEIGHT_DIV - ATF_AVG_WEIGHT_FACTOR);
	avg /= ATF_AVG_WEIGHT_DIV;

	if (!avg)
		avg = 1;

	return avg;
}


static void atf_sta_update_avg(struct atf_station *sta, struct atf_stats *cur)
{
	uint64_t total = atf_stats_total(cur);

	D("sta "MAC_ADDR_FMT" total pkts: total=%d bulk=%d normal=%d prio=%d",
		MAC_ADDR_DATA(sta->macaddr), (uint32_t)total,
		(uint32_t)cur->bulk, (uint32_t)cur->normal, (uint32_t)cur->prio);
	if (total < config.min_pkt_thresh)
		return;

	sta->avg_bulk = atf_stats_avg(sta->avg_bulk, cur->bulk, total);
	sta->avg_prio = atf_stats_avg(sta->avg_prio, cur->prio, total);
	D("avg bulk=%d prio=%d",
		(sta->avg_bulk * 100) >> ATF_AVG_SCALE,
		(sta->avg_prio * 100) >> ATF_AVG_SCALE);
	sta->stats_idx = !sta->stats_idx;
}

static int
atf_sta_cb(struct nl_msg *msg, void *arg)
{
	struct atf_interface *iface = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
	struct atf_station *sta;
	struct atf_stats *stats, diff = {};
	struct nlattr *cur;
	int idx = 0;
	int rem;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_STA_INFO] || !tb[NL80211_ATTR_MAC])
		return NL_SKIP;

	if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
			     tb[NL80211_ATTR_STA_INFO], NULL))
		return NL_SKIP;

	if (!sinfo[NL80211_STA_INFO_TID_STATS])
		return NL_SKIP;

	sta = atf_interface_sta_get(iface, nla_data(tb[NL80211_ATTR_MAC]));
	if (!sta)
		return NL_SKIP;

	stats = &sta->stats[sta->stats_idx];
	memset(stats, 0, sizeof(*stats));
	nla_for_each_nested(cur, sinfo[NL80211_STA_INFO_TID_STATS], rem)
		atf_parse_tid_stats(iface, stats, idx++, cur);

	atf_stats_diff(&diff, stats, &sta->stats[!sta->stats_idx]);
	atf_sta_update_avg(sta, &diff);
	atf_interface_sta_changed(iface, sta);

	return NL_SKIP;
}

int atf_nl80211_interface_update(struct atf_interface *iface)
{
	struct nl_msg *msg;
	int ifindex;

	ifindex = if_nametoindex(iface->ifname);
	if (!ifindex)
		return -1;

	atf_interface_sta_update(iface);

	msg = unl_genl_msg(&unl, NL80211_CMD_GET_STATION, true);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);
	unl_genl_request(&unl, msg, atf_sta_cb, iface);

	atf_interface_sta_flush(iface);

	return 0;

nla_put_failure:
	nlmsg_free(msg);
	return -1;
}

int atf_nl80211_init(void)
{
	return unl_genl_init(&unl, "nl80211");
}
