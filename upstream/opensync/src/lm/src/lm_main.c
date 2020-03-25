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

#include "evsched.h"
#include "log.h"
#include "os.h"
#include "ovsdb.h"
#include "evext.h"
#include "os_backtrace.h"
#include "json_util.h"
#include "target.h"

#include "lm.h"

/*****************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN

/*****************************************************************************/

lm_state_t              g_state;

static log_severity_t   lm_log_severity = LOG_SEVERITY_INFO;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

static void
lm_init(void)
{
    MEMZERO(g_state);
    g_state.log_state_file = target_log_state_file();
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
int main(int argc, char ** argv)
{
    struct ev_loop *loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &lm_log_severity)){
        return -1;
    }

    lm_init();

    target_log_open("LM", 0);
    LOGN("Starting LM (log manager)");
    log_severity_set(lm_log_severity);

    backtrace_init();

    json_memdbg_init(loop);

    if (evsched_init(loop) == false) {
        LOGE("Initializing LM "
             "(Failed to initialize EVSCHED)");
        return -1;
    }

    if (!target_init(TARGET_INIT_MGR_LM, loop)) {
        return -1;
    }

    if (!ovsdb_init_loop(loop, "LM")) {
        LOGE("Initializing LM "
             "(Failed to initialize OVSDB)");
        return -1;
    }

    if (lm_ovsdb_init()) {
        LOGE("Initializing LM "
             "(Failed to initialize LM tables)");
        return -1;
    }

    // From this point on log severity can change in runtime.
    log_register_dynamic_severity(loop);

    lm_hook_init(loop);

    // Run

    ev_run(loop, 0);

    // Exit

    lm_hook_close(loop);

    target_close(TARGET_INIT_MGR_LM, loop);

    if (!ovsdb_stop_loop(loop)) {
        LOGE("Stopping LM "
             "(Failed to stop OVSDB");
    }

    ev_default_destroy();

    LOGN("Exiting LM");

    return 0;
}
