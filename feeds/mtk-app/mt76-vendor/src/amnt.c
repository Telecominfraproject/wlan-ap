/* Copyright (C) 2021 Mediatek Inc. */
#define _GNU_SOURCE

#include "mt76-vendor.h"

static struct nla_policy
amnt_ctrl_policy[NUM_MTK_VENDOR_ATTRS_AMNT_CTRL] = {
	[MTK_VENDOR_ATTR_AMNT_CTRL_SET] = {.type = NLA_NESTED },
	[MTK_VENDOR_ATTR_AMNT_CTRL_DUMP] = { .type = NLA_NESTED },
};

static struct nla_policy
amnt_dump_policy[NUM_MTK_VENDOR_ATTRS_AMNT_DUMP] = {
	[MTK_VENDOR_ATTR_AMNT_DUMP_INDEX] = {.type = NLA_U8 },
	[MTK_VENDOR_ATTR_AMNT_DUMP_LEN] = { .type = NLA_U8 },
	[MTK_VENDOR_ATTR_AMNT_DUMP_RESULT] = { .type = NLA_NESTED },
};

static int mt76_amnt_set_attr(struct nl_msg *msg, int argc, char **argv)
{
	void *tb1, *tb2;
	u8 a[ETH_ALEN], idx;
	int i = 0, matches;

	idx = strtoul(argv[0], NULL, 0);
	matches = sscanf(argv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			a, a+1, a+2, a+3, a+4, a+5);

	if (matches != ETH_ALEN)
		return -EINVAL;

	tb1 = nla_nest_start(msg, MTK_VENDOR_ATTR_AMNT_CTRL_SET | NLA_F_NESTED);
	if (!tb1)
		return -ENOMEM;

	nla_put_u8(msg, MTK_VENDOR_ATTR_AMNT_SET_INDEX, idx);

	tb2 = nla_nest_start(msg, MTK_VENDOR_ATTR_AMNT_SET_MACADDR | NLA_F_NESTED);
	if (!tb2) {
		nla_nest_end(msg, tb1);
		return -ENOMEM;
	}

	for (i = 0; i < ETH_ALEN; i++)
		nla_put_u8(msg, i, a[i]);

	nla_nest_end(msg, tb2);
	nla_nest_end(msg, tb1);

	return 0;
}

int mt76_amnt_set(int idx, int argc, char **argv)
{
	struct nl_msg *msg;
	void *data;
	int ret;

	if (argc < 1)
		return 1;

	if (unl_genl_init(&unl, "nl80211") < 0) {
		fprintf(stderr, "Failed to connect to nl80211\n");
		return 2;
	}

	msg = unl_genl_msg(&unl, NL80211_CMD_VENDOR, false);

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) ||
		nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, MTK_NL80211_VENDOR_ID) ||
		nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, MTK_NL80211_VENDOR_SUBCMD_AMNT_CTRL))
		return false;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
	if (!data)
		return -ENOMEM;

	mt76_amnt_set_attr(msg, argc, argv);

	nla_nest_end(msg, data);

	ret = unl_genl_request(&unl, msg, NULL, NULL);
	if (ret)
		fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));

	unl_free(&unl);

	return ret;
}

static int mt76_amnt_dump_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb1[NUM_MTK_VENDOR_ATTRS_AMNT_CTRL];
	struct nlattr *tb2[NUM_MTK_VENDOR_ATTRS_AMNT_DUMP];
	struct nlattr *attr;
	struct nlattr *data;
	struct nlattr *cur;
	struct amnt_data *res;
	int len = 0, rem;

	attr = unl_find_attr(&unl, msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr) {
		fprintf(stderr, "Testdata attribute not found\n");
		return NL_SKIP;
	}

	nla_parse_nested(tb1, MTK_VENDOR_ATTR_AMNT_CTRL_MAX,
			 attr, amnt_ctrl_policy);

	if (!tb1[MTK_VENDOR_ATTR_AMNT_CTRL_DUMP])
		return NL_SKIP;

	nla_parse_nested(tb2, NUM_MTK_VENDOR_ATTRS_AMNT_DUMP,
			 tb1[MTK_VENDOR_ATTR_AMNT_CTRL_DUMP], amnt_dump_policy);

	if (!tb2[MTK_VENDOR_ATTR_AMNT_DUMP_LEN])
		return NL_SKIP;

	len = nla_get_u8(tb2[MTK_VENDOR_ATTR_AMNT_DUMP_LEN]);
	if (!len)
		return 0;

	if (!tb2[MTK_VENDOR_ATTR_AMNT_DUMP_RESULT])
		return NL_SKIP;

	data = tb2[MTK_VENDOR_ATTR_AMNT_DUMP_RESULT];
	nla_for_each_nested(cur,data, rem) {
		res = (struct amnt_data *) nla_data(cur);
		printf("[vendor] amnt_idx: %d, addr=%x:%x:%x:%x:%x:%x, rssi=%d/%d/%d/%d, last_seen=%u\n",
			res->idx,
			res->addr[0], res->addr[1], res->addr[2],
			res->addr[3], res->addr[4], res->addr[5],
			res->rssi[0], res->rssi[1], res->rssi[2],
			res->rssi[3], res->last_seen);
	}
	return 0;
}

int mt76_amnt_dump(int idx, int argc, char **argv)
{
	struct nl_msg *msg;
	void *data, *tb1;
	int ret = -EINVAL;
	u8 amnt_idx;

	if (argc < 1)
		return 1;

	if (unl_genl_init(&unl, "nl80211") < 0) {
		fprintf(stderr, "Failed to connect to nl80211\n");
		return 2;
	}

	msg = unl_genl_msg(&unl, NL80211_CMD_VENDOR, true);

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) ||
		nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, MTK_NL80211_VENDOR_ID) ||
		nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, MTK_NL80211_VENDOR_SUBCMD_AMNT_CTRL))
		return false;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
	if (!data)
		goto out;

	tb1 = nla_nest_start(msg, MTK_VENDOR_ATTR_AMNT_CTRL_DUMP | NLA_F_NESTED);
	if (!tb1)
		goto out;

	amnt_idx = strtoul(argv[0], NULL, 0);
	nla_put_u8(msg, MTK_VENDOR_ATTR_AMNT_DUMP_INDEX, amnt_idx);

	nla_nest_end(msg, tb1);

	nla_nest_end(msg, data);

	ret = unl_genl_request(&unl, msg, mt76_amnt_dump_cb, NULL);
	if (ret)
		fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));

out:
	unl_free(&unl);

	return ret;
}
