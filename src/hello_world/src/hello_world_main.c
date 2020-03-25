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

#include "log.h"         // logging routines
#include "json_util.h"   // json routines
#include "os.h"          // OS helpers
#include "ovsdb.h"       // OVSDB helpers
#include "target.h"      // target API
#include "hello_world.h" // module header

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
    target_log_open("HELLO_WORLD", 0);  // 0 = syslog and TTY (if present)
    LOGN("HELLO_WORLD");
    log_severity_set(log_severity);

    // Enable runtime severity updates
    log_register_dynamic_severity(loop);

    // Install crash handlers that dump the stack to the log file
    backtrace_init();

    // Allow recurrent json memory usage reports in the log file
    json_memdbg_init(loop);

    // Initialize target structure
    if (!target_init(TARGET_INIT_MGR_HELLO_WORLD, loop))
    {
        LOGE("Initializing HELLO_WORLD "
             "(Failed to initialize target library)");
        return -1;
    }

    // Connect to OVSDB
    if (!ovsdb_init_loop(loop, "HELLO_WORLD"))
    {
        LOGE("Initializing HELLO_WORLD "
             "(Failed to initialize OVSDB)");
        return -1;
    }

    // Register to relevant OVSDB tables events
    if (hello_world_ovsdb_init())
    {
        LOGE("Initializing HELLO_WORLD "
             "(Failed to initialize HELLO_WORLD tables)");
        return -1;
    }

    // Initialize Hello World demo
    if (!hello_world_demo_init(loop))
    {
        LOGE("Initializing HELLO_WORLD "
             "(Failed to initialize HELLO_WORLD demo)");
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

    target_close(TARGET_INIT_MGR_HELLO_WORLD, loop);

    if (!ovsdb_stop_loop(loop))
    {
        LOGE("Stopping HELLO_WORLD "
             "(Failed to stop OVSDB)");
    }

    ev_loop_destroy(loop);

    LOGN("Exiting HELLO_WORLD");

    return 0;
}
