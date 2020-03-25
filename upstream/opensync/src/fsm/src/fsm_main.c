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
#include <sys/types.h>
#include <unistd.h>

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
#include "fsm.h"
#include "nf_utils.h"

/******************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN

/******************************************************************************/


/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

static log_severity_t  fsm_log_severity = LOG_SEVERITY_INFO;


/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/


int main(int argc, char ** argv)
{
    struct ev_loop *loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &fsm_log_severity)) {
        return -1;
    }

    // enable logging
    target_log_open("FSM", 0);
    LOGN("Starting FSM (Flow Service Manager)");
    log_severity_set(fsm_log_severity);

    /* Register to dynamic severity updates */
    log_register_dynamic_severity(loop);

    backtrace_init();

    json_memdbg_init(loop);

    fsm_init_mgr(loop);

    if (evsched_init(loop) == false) {
        LOGE("Initializing FSM "
             "(Failed to initialize EVSCHED)");
        return -1;
    }

    if (!target_init(TARGET_INIT_MGR_FSM, loop)) {
        return -1;
    }

    if (!ovsdb_init_loop(loop, "FSM")) {
        LOGE("Initializing FSM "
             "(Failed to initialize OVSDB)");
        return -1;
    }

    fsm_event_init();

    if (fsm_ovsdb_init()) {
        LOGE("Initializing FSM "
             "(Failed to initialize FSM tables)");
        return -1;
    }

    if (dpp_init() == false) {
        LOGE("Error initializing dpp lib\n");
        return -1;
    }
    
    if (nf_ct_init(loop) < 0)
    {
        LOGE("Eror initializing conntrack\n");
        return -1;
    }
    ev_run(loop, 0);

    target_close(TARGET_INIT_MGR_FSM, loop);

    if (!ovsdb_stop_loop(loop)) {
        LOGE("Stopping FSM "
             "(Failed to stop OVSDB");
    }

    ev_loop_destroy(loop);

    LOGN("Exiting FSM");

    return 0;
}
