/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <ev.h>
#include <evsched.h>
#include <syslog.h>
#include <getopt.h>

#include "ds_tree.h"
#include "log.h"
#include "os.h"
#include "os_socket.h"
#include "ovsdb.h"
#include "evext.h"
#include "os_backtrace.h"
#include "json_util.h"
#include "target.h"

#include "netifd.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

static log_severity_t nm2_log_severity = LOG_SEVERITY_INFO;

int main(int argc, char ** argv)
{
	struct ev_loop *loop = EV_DEFAULT;

	if (os_get_opt(argc, argv, &nm2_log_severity))
		return -1;

	target_log_open("NM", 0);
	LOGN("Starting network manager - NM");
	log_severity_set(nm2_log_severity);
	log_register_dynamic_severity(loop);

	backtrace_init();

	json_memdbg_init(loop);

	if (!ovsdb_init_loop(loop, "NM")) {
		LOGEM("Initializing NM (Failed to initialize OVSDB)");
		return -1;
	}
	evsched_init(loop);

	wifi_inet_config_init();
	wifi_inet_state_init();
	netifd_ubus_init(loop);

	ev_run(loop, 0);

	if (!ovsdb_stop_loop(loop))
		LOGE("Stopping NM (Failed to stop OVSDB");

	ev_default_destroy();

	LOGN("Exiting NM");

	return 0;
}
