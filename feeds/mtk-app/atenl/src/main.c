/* Copyright (C) 2021-2022 Mediatek Inc. */

#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include "atenl.h"

static const char *progname;
bool atenl_enable;

void sig_handler(int signum)
{
	atenl_enable = false;
}

void atenl_init_signals()
{
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		goto out;
	if (signal(SIGTERM, sig_handler) == SIG_ERR)
		goto out;
	if (signal(SIGABRT, sig_handler) == SIG_ERR)
		goto out;
	if (signal(SIGUSR1, sig_handler) == SIG_ERR)
		goto out;
	if (signal(SIGUSR2, sig_handler) == SIG_ERR)
		goto out;

	return;
out:
	perror("signal");
}

static int phy_lookup_idx(struct atenl *an, const char *phyname)
{
	char buf[128];
	FILE *f;
	size_t len;
	int ret;

	atenl_nl_get_wiphy(an);
	/* TODO: Handle single wiphy model */
	if (an->is_single_wiphy)
		return 0;

	ret = snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", phyname);
	if (snprintf_error(sizeof(buf), ret))
		return -1;

	f = fopen(buf, "r");
	if (!f)
		return -1;

	len = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);

	if (!len)
		return -1;

	buf[len] = 0;
	return atoi(buf);
}

static int get_default_bridge_name(struct atenl *an)
{
	char buf[128];
	FILE *f;
	int ret;

	ret = snprintf(buf, sizeof(buf), "/sbin/procd");
	if (snprintf_error(sizeof(buf), ret))
		return -1;

	f = fopen(buf, "r");

	/* This procd is openwrt only */
	if (f) {
		an->bridge_name = BRIDGE_NAME_OPENWRT;
		fclose(f);
	} else {
		an->bridge_name = BRIDGE_NAME_RDKB;
	}

	return 0;
}

static void usage(void)
{
	printf("Usage:\n");
	printf("  %s [-u] [-i phyX]\n", progname);
	printf("options:\n"
	       "  -h = show help text\n"
	       "  -i = phy name of driver interface, please use first phy for dbdc\n"
	       "  -u = use unicast to respond to HQADLL\n"
	       "  -b = specify your bridge name\n"
	       "  -c = eeprom-related command\n"
	       "  -p = specify the flash partition name and offset (<name>:<offs>)\n");
	printf("examples:\n"
	       "  %s -u -i phy0 -b br-lan\n", progname);

	exit(EXIT_FAILURE);
}

static void atenl_handler_run(struct atenl *an)
{
	int count, sock_eth = an->sock_eth;
	fd_set readfds;

	atenl_info("Start atenl HQA command handler\n");

	while (atenl_enable) {
		FD_ZERO(&readfds);
		FD_SET(sock_eth, &readfds);
		count = select(sock_eth + 1, &readfds, NULL, NULL, NULL);

		if (count < 0) {
			atenl_err("%s: select failed, %s\n", __func__, strerror(errno));
		} else if (count == 0) {
			usleep(1000);
		} else {
			if (!FD_ISSET(sock_eth, &readfds))
				continue;
			atenl_hqa_proc_cmd(an);
		}
	}

	atenl_dbg("HQA command handler end\n");
}

int main(int argc, char **argv)
{
	int opt, phy_idx, ret = 0;
	char *phy = "phy0", *cmd = NULL;
	char *token;
	struct atenl *an;

	progname = argv[0];

	an = calloc(1, sizeof(struct atenl));
	if (!an)
		return -ENOMEM;

	while(1) {
		opt = getopt(argc, argv, "hi:uc:b:p:");
		if (opt == -1)
			break;

		switch (opt) {
			case 'h':
				usage();
				goto out;
			case 'i':
				phy = optarg;
				break;
			case 'b':
				an->bridge_name = optarg;
				break;
			case 'u':
				an->unicast = true;
				printf("Opt: use unicast to send response\n");
				break;
			case 'c':
				cmd = optarg;
				break;
			case 'p':
				token = strtok(optarg, ":");
				if (!token)
					break;
				an->flash_part = token;
				an->flash_offset = strtol(strtok(NULL, ":"), NULL, 0);
				break;
			default:
				atenl_err("Not supported option: %c\n", opt);
				goto out;
		}
	}

	phy_idx = phy_lookup_idx(an, phy);
	if (phy_idx < 0 || phy_idx > UCHAR_MAX) {
		atenl_err("Could not find phy '%s'\n", phy);
		goto out;
	}

	if (cmd) {
		atenl_eeprom_cmd_handler(an, phy_idx, cmd);
		goto out;
	}

	atenl_enable = true;
	atenl_init_signals();

	if (!an->bridge_name) {
		ret = get_default_bridge_name(an);
		if (ret) {
			atenl_err("Get default bridge name failed\n");
			goto out;
		}

		atenl_info("Bridge name is not specified, use default bridge name: %s\n", an->bridge_name);
	} else {
		atenl_info("Currently using bridge name: %s\n", an->bridge_name);
	}

	/* background ourself */
	if (!fork()) {
		ret = atenl_eeprom_init(an, phy_idx);
		if (ret)
			goto out;

		ret = atenl_eth_init(an);
		if (ret)
			goto out;

		atenl_handler_run(an);
	} else {
		usleep(800000);
	}

	ret = 0;
out:
	if (an->sock_eth)
		close(an->sock_eth);
	if (an->eeprom_fd || an->eeprom_data)
		atenl_eeprom_close(an);
	free(an);

	return ret;
}
