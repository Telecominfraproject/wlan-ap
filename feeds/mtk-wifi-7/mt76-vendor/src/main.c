/* Copyright (C) 2021 Mediatek Inc. */
#define _GNU_SOURCE

#include <net/if.h>

#include "mt76-vendor.h"

static const char *progname;
struct unl unl;

void usage(void)
{
	static const char *const commands[] = {
		"set csi ctrl=<opt1>,<opt2>,<opt3>,<opt4> (macaddr=<macaddr>)",
		"set csi interval=<interval (us)>",
		"dump csi <packet num> <filename>",

		"set amnt <index>(0x0~0xf) <mac addr>(xx:xx:xx:xx:xx:xx)",
		"dump amnt <index> (0x0~0xf or 0xff)",

		"set ap_rfeatures he_gi=<val>",
		"set ap_rfeatures he_ltf=<val>",
		"set ap_rfeatures trig_type=<enable>,<val> (val: 0-7)",
		"set ap_rfeatures ack_policy=<val> (val: 0-4)",
		"set ap_wireless fixed_mcs=<val>",
		"set ap_wireless ofdma=<val> (0: disable, 1: DL, 2: UL)",
		"set ap_wireless nusers_ofdma=<val>",
		"set ap_wireless ppdu_type=<val> (0: SU, 1: MU, 4: LEGACY)",
		"set ap_wireless add_ba_req_bufsize=<val>",
		"set ap_wireless mimo=<val> (0: DL, 1: UL)",
		"set ap_wireless ampdu=<enable>",
		"set ap_wireless amsdu=<enable>",
		"set ap_wireless cert=<enable>",

		"set mu onoff=<val> (bitmap- UL MU-MIMO(bit3), DL MU-MIMO(bit2), UL OFDMA(bit1), DL OFDMA(bit0))",

		"dump phy_capa",
	};
	int i;

	fprintf(stderr, "Usage:\n");
	for (i = 0; i < ARRAY_SIZE(commands); i++)
		printf("  %s wlanX %s\n", progname, commands[i]);

	exit(1);
}

int main(int argc, char **argv)
{
	const char *cmd, *subcmd;
	int if_idx, ret = 0;

	progname = argv[0];
	if (argc < 4)
		usage();

	if_idx = if_nametoindex(argv[1]);
	if (!if_idx) {
		fprintf(stderr, "%s\n", strerror(errno));
		return 2;
	}

	cmd = argv[2];
	subcmd = argv[3];
	argv += 4;
	argc -= 4;

	if (!strncmp(cmd, "dump", 4)) {
		if (!strncmp(subcmd, "csi", 3))
			ret = mt76_csi_dump(if_idx, argc, argv);
		else if (!strncmp(subcmd, "amnt", 4))
			ret = mt76_amnt_dump(if_idx, argc, argv);
		else if (!strncmp(subcmd, "phy_capa", 4))
			ret = mt76_phy_capa_dump(if_idx, argc, argv);
	} else if (!strncmp(cmd, "set", 3)) {
		if (!strncmp(subcmd, "csi", 3))
			ret = mt76_csi_set(if_idx, argc, argv);
		else if (!strncmp(subcmd, "amnt", 4))
			ret = mt76_amnt_set(if_idx, argc, argv);
		else if (!strncmp(subcmd, "ap_rfeatures", 12))
			ret = mt76_ap_rfeatures_set(if_idx, argc, argv);
		else if (!strncmp(subcmd, "ap_wireless", 11))
			ret = mt76_ap_wireless_set(if_idx, argc, argv);
		else if (!strncmp(subcmd, "mu", 2))
			ret = mt76_mu_onoff_set(if_idx, argc, argv);
	} else {
		usage();
	}

	return ret;
}
