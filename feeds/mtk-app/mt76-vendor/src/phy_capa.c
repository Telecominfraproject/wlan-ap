/* Copyright (C) 2021 Mediatek Inc. */
#define _GNU_SOURCE

#include "mt76-vendor.h"

static struct nla_policy
phy_capa_ctrl_policy[NUM_MTK_VENDOR_ATTRS_PHY_CAPA_CTRL] = {
	[MTK_VENDOR_ATTR_PHY_CAPA_CTRL_SET] = { .type = NLA_NESTED },
	[MTK_VENDOR_ATTR_PHY_CAPA_CTRL_DUMP] = { .type = NLA_NESTED },
};

static struct nla_policy
phy_capa_dump_policy[NUM_MTK_VENDOR_ATTRS_PHY_CAPA_DUMP] = {
	[MTK_VENDOR_ATTR_PHY_CAPA_DUMP_MAX_SUPPORTED_BSS] = { .type = NLA_U16 },
	[MTK_VENDOR_ATTR_PHY_CAPA_DUMP_MAX_SUPPORTED_STA] = { .type = NLA_U16 },
};

static int mt76_phy_capa_dump_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NUM_MTK_VENDOR_ATTRS_PHY_CAPA_CTRL];
	struct nlattr *tb_dump[NUM_MTK_VENDOR_ATTRS_PHY_CAPA_DUMP];
	struct nlattr *attr;
	u16 max_bss, max_sta;

	attr = unl_find_attr(&unl, msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr) {
		fprintf(stderr, "Testdata attribute not found\n");
		return NL_SKIP;
	}

	nla_parse_nested(tb, MTK_VENDOR_ATTR_PHY_CAPA_CTRL_MAX,
			 attr, phy_capa_ctrl_policy);

	if (!tb[MTK_VENDOR_ATTR_PHY_CAPA_CTRL_DUMP])
		return NL_SKIP;

	nla_parse_nested(tb_dump, MTK_VENDOR_ATTR_PHY_CAPA_DUMP_MAX,
			 tb[MTK_VENDOR_ATTR_PHY_CAPA_CTRL_DUMP], phy_capa_dump_policy);

	max_bss = nla_get_u16(tb_dump[MTK_VENDOR_ATTR_PHY_CAPA_DUMP_MAX_SUPPORTED_BSS]);
	max_sta = nla_get_u16(tb_dump[MTK_VENDOR_ATTR_PHY_CAPA_DUMP_MAX_SUPPORTED_STA]);

	printf("[vendor] Max Supported BSS=%u "
		" Max Supported STA=%u\n", max_bss, max_sta);

	return 0;
}

int mt76_phy_capa_dump(int idx, int argc, char **argv)
{
	struct nl_msg *msg;
	void *data;
	int ret = -EINVAL;

	if (unl_genl_init(&unl, "nl80211") < 0) {
		fprintf(stderr, "Failed to connect to nl80211\n");
		return 2;
	}

	msg = unl_genl_msg(&unl, NL80211_CMD_VENDOR, true);

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) ||
		nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, MTK_NL80211_VENDOR_ID) ||
		nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, MTK_NL80211_VENDOR_SUBCMD_PHY_CAPA_CTRL))
		return false;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
	if (!data)
		goto out;

	nla_nest_end(msg, data);

	ret = unl_genl_request(&unl, msg, mt76_phy_capa_dump_cb, NULL);
	if (ret)
		fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));

out:
	unl_free(&unl);

	return ret;
}
