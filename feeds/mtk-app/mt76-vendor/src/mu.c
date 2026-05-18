/* Copyright (C) 2021 Mediatek Inc. */
#define _GNU_SOURCE

#include "mt76-vendor.h"

static int mt76_mu_onoff_set_attr(struct nl_msg *msg, int argc, char **argv)
{
	char *val;

	val = strchr(argv[0], '=');
	if (!val)
		return -EINVAL;

	*(val++) = 0;

	if (!strncmp(argv[0], "onoff", 5))
		nla_put_u8(msg, MTK_VENDOR_ATTR_MU_CTRL_ONOFF,
			   strtoul(val, NULL, 0));

	return 0;
}

int mt76_mu_onoff_set(int idx, int argc, char **argv)
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
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			MTK_NL80211_VENDOR_SUBCMD_MU_CTRL))
		return false;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
	if (!data)
		return -ENOMEM;

	mt76_mu_onoff_set_attr(msg, argc, argv);

	nla_nest_end(msg, data);

	ret = unl_genl_request(&unl, msg, NULL, NULL);
	if (ret)
		fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));

	unl_free(&unl);

	return ret;
}
