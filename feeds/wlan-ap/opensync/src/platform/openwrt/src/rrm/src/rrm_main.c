/* SPDX-License-Identifier: BSD-3-Clause */

#include <ev.h>          // libev routines
#include <getopt.h>      // command line arguments

#include "log.h"         // logging routines
#include "json_util.h"   // json routines
#include "os.h"          // OS helpers
#include "ovsdb.h"       // OVSDB helpers
#include "rrm.h" // module header

/* Default log severity */
static log_severity_t  log_severity = LOG_SEVERITY_INFO;

/* Log entries from this file will contain "MAIN" */
#define MODULE_ID LOG_MODULE_ID_MAIN

int main(int argc, char ** argv)
{
	struct ev_loop *loop = EV_DEFAULT;

	// Parse command-line arguments
	if (os_get_opt(argc, argv, &log_severity))
	{
		return -1;
	}

	// Initialize logging library
	target_log_open("RRM", 0);  // 0 = syslog and TTY (if present)
	LOGN("Starting Radio Resource Manager");
	log_severity_set(log_severity);

	// Enable runtime severity updates
	log_register_dynamic_severity(loop);

	// Install crash handlers that dump the stack to the log file
	backtrace_init();

	// Allow recurrent JSON memory usage reports in the log file
	json_memdbg_init(loop);

	// Connect to OVSDB
	if (!ovsdb_init_loop(loop, "RRM"))
	{
		LOGE("Initializing RRM "
				"(Failed to initialize OVSDB)");
		return -1;
	}

	// Register to relevant OVSDB tables events
	if (rrm_setup_monitor()) {
		LOGE("Initializing RRM "
				"(Failed to initialize RRM tables)");
		return -1;
	}

	evsched_init(loop);

	rrm_ubus_init(loop);
	rrm_channel_init();

	// Start the event loop
	ev_run(loop, 0);

	if (!ovsdb_stop_loop(loop))
	{
		LOGE("Stopping RRM "
				"(Failed to stop RRM)");
	}

	ev_default_destroy();

	LOGN("Exiting RRM");

	return 0;
}
