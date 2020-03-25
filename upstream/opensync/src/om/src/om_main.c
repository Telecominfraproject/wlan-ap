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

/*
 * Openflow Manager - Main Program Loop
 */
#include <stdlib.h>
#include <stdarg.h>
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

#include "os_backtrace.h"
#include "json_util.h"
#include "evext.h"

#include "target.h"
#include "om.h"

/*****************************************************************************/
#define MODULE_ID LOG_MODULE_ID_MAIN


/*****************************************************************************/
static log_severity_t om_log_severity = LOG_SEVERITY_DEBUG;
static struct tag_mgr tag_mgr;

/******************************************************************************
 * PROTECTED FUNCTIONS
 *****************************************************************************/

/******************************************************************************
 * PUBLIC API definitions
 *****************************************************************************/

int main( int argc, char **argv )
{
    struct ev_loop *ev_loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &om_log_severity)){
        return -1;
    }

    // Initialize logging library
    target_log_open( "OM", 0);
    LOGN( "Starting OM (Openflow manager)" );

    log_severity_set( om_log_severity );

    // Enable backtrace support
    backtrace_init();

    // Initialize JSON memory debugging
    json_memdbg_init( ev_loop );

    // Initialize EV scheduler
    if (evsched_init( ev_loop ) == false) {
        LOGEM( "Failed to initialize EVSCHED" );
        return(1);
    }

    if (!target_init( TARGET_INIT_MGR_OM, ev_loop )) {
        return(1);
    }

    // Connect to OVSDB
    if( ovsdb_init_loop( ev_loop, "OM" ) == false ) {
        LOGEM( "Failed to initialize and connect to OVSDB" );
        return(1);
    }

    memset(&tag_mgr, 0, sizeof(tag_mgr));
    tag_mgr.service_tag_update = om_template_tag_update;
    om_tag_init(&tag_mgr);

    if( !om_monitor_init() ) {
        LOGEM( "Failed to initialize Openflow OVSDB monitoring" );
        return(1);
    }

    // Register for dynamic severity updates
    log_register_dynamic_severity( ev_loop );

    // Run the main loop
    ev_run( ev_loop, 0 );

    // Cleanup and Exit
    LOGN( "Openflow Manager shutting down" );

    target_close( TARGET_INIT_MGR_OM, ev_loop );

    if (!ovsdb_stop_loop( ev_loop )) {
        LOGEM( "Failed to stop OVSDB" );
    }

    evsched_cleanup();
    ev_default_destroy();

    return(0);
}
