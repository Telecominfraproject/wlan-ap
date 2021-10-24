#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <libubox/uloop.h>

#include "qosify.h"

static int usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [options]\n"
		"Options:\n"
		"	-f:		force reload of BPF programs\n"
		"	-l <file>	Load defaults from <file>\n"
		"	-o		only load program/maps without running as daemon\n"
		"\n", progname);

	return 1;
}

int main(int argc, char **argv)
{
	const char *load_file = NULL;
	bool force_init = false;
	bool oneshot = false;
	int ch;

	while ((ch = getopt(argc, argv, "fl:o")) != -1) {
		switch (ch) {
		case 'f':
			force_init = true;
			break;
		case 'l':
			load_file = optarg;
			break;
		case 'o':
			oneshot = true;
			break;
		default:
			return usage(argv[0]);
		}
	}

	if (qosify_loader_init(force_init))
		return 2;

	if (qosify_map_init())
		return 2;

	if (qosify_map_load_file(load_file))
		return 2;

	if (oneshot)
		return 0;

	ulog_open(ULOG_SYSLOG, LOG_DAEMON, "qosify");
	uloop_init();

	if (qosify_ubus_init() ||
	    qosify_iface_init())
		return 2;

	uloop_run();

	qosify_iface_stop();

	uloop_done();

	return 0;
}
