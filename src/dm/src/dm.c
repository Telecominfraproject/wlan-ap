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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ev.h>

#include "log.h"
#include "os_socket.h"
#include "os_backtrace.h"
#include "ovsdb.h"
#include "schema.h"
#include "jansson.h"
#include "monitor.h"
#include "json_util.h"
#include "target.h"

#include "dm.h"

/*****************************************************************************/

#define DM_CFG_TBL_MAX                  1
#define OVSDB_DEF_TABLE                 "Open_vSwitch"
#define TARGET_READY_POLL               10


#define GW_CFG_DIR             "/mnt/data/config"
#define GW_CFG_FILE            GW_CFG_DIR"/gw.cfg"
#define GW_CFG_FILE_MD5        GW_CFG_DIR"/gw.cfg.md5"

/*****************************************************************************/

extern struct ev_io     wovsdb;

static log_severity_t   dm_log_severity = LOG_SEVERITY_INFO;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/


static bool dm_apply_cfg(void)
{
    const char *cfg_file =     GW_CFG_FILE;
    const char *cfg_file_md5 = GW_CFG_FILE_MD5;
    char command[1024];


    snprintf(command, sizeof(command),
            "set -e; ! test -e %s || ( cd %s && md5sum -c %s 2>&1 && sh -x %s 2>&1 )",
            cfg_file, GW_CFG_DIR, cfg_file_md5, cfg_file);

    cmd_log(command);

    return true;
}


/*
 * Main
 */
int main (int argc, char ** argv)
{
    struct ev_loop *loop;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &dm_log_severity)){
        return -1;
    }

    /* Log all errors, warnings, etc. */
    target_log_open("DM", 0);
    LOG(NOTICE, "Starting diagnostic manager - DM");
    /* set application global log level */
    log_severity_set(dm_log_severity);

    backtrace_init();

    loop = ev_default_loop(0);
    if (!loop)
    {
        LOGE("Initializing DM "
             "(Can't initialize loop)");
        return 1;
    }
    json_memdbg_init(loop);

    /* start monitoring DM */
    mon_start(argc, argv);

    while(!target_ready(loop)) {
        LOGW("Target not ready yet -- waiting...");
        sleep(TARGET_READY_POLL);
    }
    if (!target_init(TARGET_INIT_MGR_DM, loop)) {
        LOGE("Initializing DM "
             "(Can't initialize target)");
        return 1;
    }

    /* connect to ovsdb-server - start monitoring */
    if (!ovsdb_ready("DM"))
    {
        LOGEM("Initializing DM "
             "(Failed to initialize OVSDB)");

        /* Let's restart the whole thing */
        target_managers_restart();

        return MON_EXIT_RESTART;
    }

    /* initialize state machine */
    init_statem();

    /* Register to dynamic severity updates */
    log_register_dynamic_severity(loop);

    dm_hook_init(loop);

    dm_apply_cfg();

    /* start main loop and wait for event to come */
    ev_run(loop, 0);

    dm_hook_close(loop);

    LOGN("Exiting DM");

    target_close(TARGET_INIT_MGR_DM, loop);

    /* clean up */
    ev_io_stop(EV_DEFAULT,  &wovsdb);
    ev_default_destroy();

    return 0;
}
