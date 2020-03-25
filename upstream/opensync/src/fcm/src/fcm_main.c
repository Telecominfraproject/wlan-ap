/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <ev.h>          // libev routines
#include <getopt.h>      // command line arguments
#include <time.h>
#include <string.h>

#include "evsched.h"     // ev helpers
#include "log.h"         // logging routines
#include "json_util.h"   // json routines
#include "os.h"          // OS helpers
#include "ovsdb.h"       // OVSDB helpers
#include "target.h"      // target API
#include "network_metadata.h"  // network metadata API

#include "fcm.h"         // module header
#include "fcm_priv.h"
#include "fcm_mgr.h"
#include "fcm_filter.h"
#include "ip_map.h"

/* Default log severity */
static log_severity_t  log_severity = LOG_SEVERITY_INFO;

/* Log entries from this file will contain "MAIN" */
#define MODULE_ID LOG_MODULE_ID_MAIN

/**
 * Main
 *
 * Note: Command line arguments allow overriding the log severity
 */
int main(int argc, char ** argv)
{
    struct ev_loop *loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &log_severity))
    {
        return -1;
    }

    // Initialize logging library
    target_log_open("FCM", 0);  // 0 = syslog and TTY (if present)
    LOGN("Starting FCM (Flow Collection Manager)");
    log_severity_set(log_severity);

    // Enable runtime severity updates
    log_register_dynamic_severity(loop);

    // Install crash handlers that dump the stack to the log file
    backtrace_init();

    // Allow recurrent json memory usage reports in the log file
    json_memdbg_init(loop);

    // Initialize EV context
    if (evsched_init(loop) == false)
    {
        LOGE("Initializing FCM "
             "(Failed to initialize EVSCHED)");
        return -1;
    }

    // Initialize target structure
    if (!target_init(TARGET_INIT_MGR_FCM, loop))
    {
        LOGE("Initializing FCM "
             "(Failed to initialize target library)");
        return -1;
    }

    // Initialize the manager
    if (!fcm_init_mgr(loop))
    {
        LOGE("Initializing FCM "
              "(Failed to initialize manager)");
        return -1;
    }

    // Connect to OVSDB
    if (!ovsdb_init_loop(loop, "FCM"))
    {
        LOGE("Initializing FCM "
             "(Failed to initialize OVSDB)");
        return -1;
    }

    // Initialize FCM filter manager
    if (fcm_filter_init())
    {
        LOGE("Initializing FCM Filter failed " );
        return -1;
    }

    if (ip_map_init())
    {
        LOGE("Initializing Ip Map failed " );
        return -1;
    }

    // Register to relevant OVSDB tables events
    if (fcm_ovsdb_init())
    {
        LOGE("Initializing FCM "
             "(Failed to initialize FCM tables)");
        return -1;
    }

    // Initialize data pipeline
    if (dpp_init() == false)
    {
        LOGE("Initializing HELLO_WORLD "
             "(Failed to initialize DPP library)");
        return -1;
    }

    // Start the event loop
    ev_run(loop, 0);

    target_close(TARGET_INIT_MGR_FCM, loop);

    // De-init FCM filter manager
    fcm_filter_cleanup();
    ip_map_cleanup();

    if (!ovsdb_stop_loop(loop))
    {
        LOGE("Stopping FCM "
             "(Failed to stop OVSDB)");
    }

    ev_loop_destroy(loop);

    LOGN("Exiting FCM");

    return 0;
}
