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
#include <syslog.h>
#include <getopt.h>

#include "ds_tree.h"
#include "evsched.h"
#include "log.h"
#include "os.h"
#include "os_socket.h"
#include "ovsdb.h"
#include "evext.h"
#include "os_backtrace.h"
#include "json_util.h"
#include "target.h"
#include "cm2.h"

/******************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN

/******************************************************************************/

cm2_state_t             g_state;

static log_severity_t   cm2_log_severity = LOG_SEVERITY_INFO;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

void cm2_init_capabilities(void)
{
    g_state.target_type = target_device_capabilities_get();
    LOGI("Device caps: 0x%x %s%s", g_state.target_type,
            g_state.target_type & TARGET_GW_TYPE ? "[GW]" : "",
            g_state.target_type & TARGET_EXTENDER_TYPE ? "[EXT]" : "");
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/

bool cm2_is_extender(void)
{
    return g_state.target_type & TARGET_EXTENDER_TYPE ? true : false;
}


int main(int argc, char ** argv)
{
    struct ev_loop *loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &cm2_log_severity)){
        return -1;
    }

    // enable logging
    target_log_open("CM", 0);
    LOGN("Starting CM (connection manager)");
    log_severity_set(cm2_log_severity);

    /* Register to dynamic severity updates */
    log_register_dynamic_severity(loop);

    backtrace_init();

    json_memdbg_init(loop);

    if (evsched_init(loop) == false) {
        LOGE("Initializing CM "
             "(Failed to initialize EVSCHED)");
        return -1;
    }

    if (!target_init(TARGET_INIT_MGR_CM, loop)) {
        return -1;
    }

    if (!ovsdb_init_loop(loop, "CM")) {
        LOGE("Initializing CM "
             "(Failed to initialize OVSDB)");
        return -1;
    }

    cm2_init_capabilities();
    cm2_event_init(loop);

    if (cm2_ovsdb_init()) {
        LOGE("Initializing CM "
             "(Failed to initialize CM tables)");
        return -1;
    }

    if (cm2_is_extender()) {
        cm2_wdt_init(loop);
        cm2_stability_init(loop);
    }
#ifdef BUILD_HAVE_LIBCARES
    if (evx_init_ares(g_state.loop, &g_state.eares) < 0) {
        LOGW("Ares init failed");
        return -1;
    }
#endif
    ev_run(loop, 0);

    if (cm2_is_extender()) {
        cm2_wdt_close(loop);
        cm2_stability_close(loop);
    }

    cm2_event_close(loop);

    target_close(TARGET_INIT_MGR_CM, loop);

    if (!ovsdb_stop_loop(loop)) {
        LOGE("Stopping CM "
             "(Failed to stop OVSDB");
    }
#ifdef BUILD_HAVE_LIBCARES
    evx_stop_ares(&g_state.eares);
#endif
    ev_default_destroy();

    LOGN("Exiting CM");

    return 0;
}
