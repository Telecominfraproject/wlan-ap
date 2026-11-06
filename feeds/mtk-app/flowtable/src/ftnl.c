#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include "netfilter_flowtable.h"

void usage(void)
{
	printf("#########flush flow table\n");
	printf("ftnl -F\n");
	printf("#########del flow from offload table\n");
	printf("ftnl -D [sip] [dip] [proto] [sport] [dport]\n");
}

int main(int argc, char *argv[])
{
	struct ftnl_handle *h;
	struct flow_tuple tuple = {0};
	int msg = -1;
	int c;
	int ret = -1;
	const char *optstring = "FD";
	struct option opts[] = {
		{"sip", required_argument, NULL, 's'},
		{"dip", required_argument, NULL, 'd'},
		{"proto", required_argument, NULL, 'p'},
		{"sport", required_argument, NULL, 'm'},
		{"dport", required_argument, NULL, 'n'}
	};

	/* open netlink socket */
	h = ftnl_open();
	if (!h)
		return ret;

	/* parse arg */
	while ((c = getopt_long(argc, argv, optstring, opts, NULL)) != -1) {
		switch (c) {
		case 'F':
			msg = FT_MSG_FLUSH;
			break;
		case 'D':
			msg = FT_MSG_DEL;
			break;
		case 's':
			inet_aton(optarg, &tuple.sip4);
			break;
		case 'd':
			inet_aton(optarg, &tuple.dip4);
			break;
		case 'p':
			if (!strcmp(optarg, "tcp"))
				tuple.proto = IPPROTO_TCP;
			else if (!strcmp(optarg, "udp"))
				tuple.proto = IPPROTO_UDP;
			else {
				printf("proto bad value...\n");
				printf("pls set proto to udp or tcp arg : %s\n",
				       optarg);
				goto out;
			}
			break;
		case 'm':
			tuple.sport = htons(atoi(optarg));
			break;
		case 'n':
			tuple.dport = htons(atoi(optarg));
			break;
		default:
			usage();
			goto out;
		}
	}

	switch (msg) {
	case FT_MSG_FLUSH:
		ftnl_flush_table(h);
		break;
	case FT_MSG_DEL:
		ftnl_del_flow(h, &tuple);
		break;
	default:
		break;
	}

out:
	ftnl_close(h);
	return ret;
}
