/* SPDX-License-Identifier: BSD-3-Clause */

#include <linux/limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <libubox/uloop.h>
#include <libubox/ulog.h>

static void
maverick_cb(struct uloop_timeout *delay)
{
	char link[PATH_MAX] = { };

	if (readlink("/etc/ucentral/ucentral.active", link, PATH_MAX) != -1 &&
	    strcmp(link, "/etc/ucentral/ucentral.cfg.0000000001")) {
		ULOG_INFO("found an active symlink\n");
		uloop_end();
		return;
	}

	ULOG_INFO("triggering maverick");
	if (system("/usr/libexec/ucentral/maverick.sh"))
		ULOG_ERR("failed to launch Maverick");
	uloop_end();
	return;
}

static struct uloop_timeout maverick = {
	.cb = maverick_cb,
};

int
main(int argc, char **argv)
{
	ulog_open(ULOG_STDIO | ULOG_SYSLOG, LOG_DAEMON, "maverick");

	uloop_init();
	uloop_timeout_set(&maverick, 150 * 1000);
	uloop_run();
	uloop_done();

	return 0;
}
