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
#include "log.h"
#include "os.h"
#include "os_socket.h"
#include "ovsdb.h"
#include "evext.h"
#include "dppline.h"
#include "os_backtrace.h"
#include "json_util.h"
#include "evsched.h"

#include "sm.h"

/*****************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN

/*****************************************************************************/

struct sm_cxt           g_cxt;
struct sm_cxt           *pcxt = &g_cxt;

static log_severity_t   sm_log_severity = LOG_SEVERITY_INFO;

extern struct ev_io     wovsdb;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

/******************************************************************************
 * PUBLIC API definitions
 *****************************************************************************/

int main (int argc, char **argv)
{
    bool                rc;
    struct ev_loop     *loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &sm_log_severity)){
        return -1;
    }

    /* enable logging on lib level */
    target_log_open("SM", 0);
    LOG(NOTICE, "Initializing stats manager - SM");
    log_severity_set(sm_log_severity);

    /* Register to dynamic severity updates */
    log_register_dynamic_severity(loop);

    json_memdbg_init(loop);

    if (evsched_init(loop) == false) {
        LOGE("Initializing SM "
             "(Failed to initialize EVSCHED)");
        return -1;
    }

    /* Initialize target library */
    rc = target_init(TARGET_INIT_MGR_SM, loop);
    if (true != rc)
    {
        LOG(ERR,
            "Initializing SM "
            "(Failed to init target library)");
        return -1;
    }

    if (!dpp_init())
    {
        LOG(ERR,
            "Initializing SM "
            "(Failed to init DPP library)");
        return -1;
    }

    if (!sm_mqtt_init())
    {
        LOG(ERR,
            "Initializing SM "
            "(Failed to start MQTT)");
        return -1;
    }

    if (!sm_scan_schedule_init())
    {
        LOG(ERR,"Initializing SM"
            "(Failed to init scanning)");
        return -1;
    }

    // Connect to ovsdb
    if (!ovsdb_init_loop(loop, "SM")) {
        LOGE("Initializing SM "
             "(Failed to initialize OVSDB)");
        return -1;
    }

    if (sm_setup_monitor()) {
        return -1;
    }

    backtrace_init();

    ev_run(EV_DEFAULT, 0);

    target_close(TARGET_INIT_MGR_SM, loop);

    if (!ovsdb_stop_loop(loop)) {
        LOGE("Stopping SM "
             "(Failed to stop OVSDB");
    }

    sm_mqtt_stop();

    ev_default_destroy();

    LOGN("Exiting SM");

    return 0;
}
