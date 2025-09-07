#include "udevmand.h"

void
ethers_init(void)
{
	char buf[512], *p;
	int ret;
	FILE *f;

	f = fopen("/etc/ethers", "r");

	if (!f)
		return;

	while (fgets(buf, sizeof(buf), f)) {
		uint8_t addr[ETH_ALEN];
		struct mac *mac;

		p = strtok(buf, " \t\n");
		ret = sscanf(p, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			     &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
		if (ret != 6)
			continue;

		p = strtok(NULL, " \t\n");

		if (!p)
			continue;
		mac = mac_find(addr);
		mac->ethers = strdup(p);
		ULOG_INFO("new ethers " MAC_FMT" %s\n", MAC_VAR(addr), p);
	}

	fclose(f);
}

