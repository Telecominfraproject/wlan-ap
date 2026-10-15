/* Copyright (C) 2021 Mediatek Inc. */
#define _GNU_SOURCE

#include "mt76-vendor.h"

static int mt76_ap_rfeatures_set_attr(struct nl_msg *msg, int argc, char **argv)
{
	char *val, *s1, *s2, *cur;
	void *data;
	int idx = MTK_VENDOR_ATTR_RFEATURE_CTRL_TRIG_TYPE_EN;

	val = strchr(argv[0], '=');
	if (!val)
		return -EINVAL;

	*(val++) = 0;

	if (!strncmp(argv[0], "he_gi", 5)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_RFEATURE_CTRL_HE_GI, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "he_ltf", 6)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_RFEATURE_CTRL_HE_LTF, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "trig_type", 9)) {
		data = nla_nest_start(msg,
				      MTK_VENDOR_ATTR_RFEATURE_CTRL_TRIG_TYPE_CFG | NLA_F_NESTED);
		if (!data)
			return -ENOMEM;

		s1 = s2 = strdup(val);
		while ((cur = strsep(&s1, ",")) != NULL)
			nla_put_u8(msg, idx++, strtoul(cur, NULL, 0));

		nla_nest_end(msg, data);

		free(s2);
	} else if (!strncmp(argv[0], "ack_policy", 10)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_RFEATURE_CTRL_ACK_PLCY, strtoul(val, NULL, 0));
	}

	return 0;
}

int mt76_ap_rfeatures_set(int idx, int argc, char **argv)
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
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, MTK_NL80211_VENDOR_SUBCMD_RFEATURE_CTRL))
		return false;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
	if (!data)
		return -ENOMEM;

	mt76_ap_rfeatures_set_attr(msg, argc, argv);

	nla_nest_end(msg, data);

	ret = unl_genl_request(&unl, msg, NULL, NULL);
	if (ret)
		fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));

	unl_free(&unl);

	return ret;
}

static int mt76_ap_wireless_set_attr(struct nl_msg *msg, int argc, char **argv)
{
	char *val;

	val = strchr(argv[0], '=');
	if (!val)
		return -EINVAL;

	*(val++) = 0;

	if (!strncmp(argv[0], "fixed_mcs", 9)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_FIXED_MCS, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "ofdma", 5)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_FIXED_OFDMA, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "ppdu_type", 9)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_PPDU_TX_TYPE, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "nusers_ofdma", 12)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_NUSERS_OFDMA, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "add_ba_req_bufsize", 18)) {
		nla_put_u16(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_BA_BUFFER_SIZE,
			    strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "mimo", 4)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_MIMO, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "ampdu", 5)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_AMPDU, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "amsdu", 5)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_AMSDU, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "cert", 4)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_CERT, strtoul(val, NULL, 0));
	} else if (!strncmp(argv[0], "rts_sigta", 9)) {
		nla_put_u8(msg, MTK_VENDOR_ATTR_WIRELESS_CTRL_RTS_SIGTA,
			strtoul(val, NULL, 0));
	}

	return 0;
}

int mt76_ap_wireless_set(int idx, int argc, char **argv)
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
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, MTK_NL80211_VENDOR_SUBCMD_WIRELESS_CTRL))
		return false;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
	if (!data)
		return -ENOMEM;

	mt76_ap_wireless_set_attr(msg, argc, argv);

	nla_nest_end(msg, data);

	ret = unl_genl_request(&unl, msg, NULL, NULL);
	if (ret)
		fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));

	unl_free(&unl);

	return ret;
}
