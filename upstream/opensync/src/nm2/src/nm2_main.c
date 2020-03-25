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
#include "nm2.h"
#include "target.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

/*****************************************************************************/

static log_severity_t nm2_log_severity = LOG_SEVERITY_INFO;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/

int main(int argc, char ** argv)
{
    struct ev_loop *loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &nm2_log_severity)){
        return -1;
    }

    // enable logging
    target_log_open("NM", 0);
    LOGN("Starting network manager - NM");
    log_severity_set(nm2_log_severity);
    log_register_dynamic_severity(loop);

    backtrace_init();

    json_memdbg_init(loop);

    if (evsched_init(loop) == false) {
        LOGE("Initializing NM "
             "(Failed to initialize EVSCHED)");
        return -1;
    }

    // Connect to ovsdb
    if (!ovsdb_init_loop(loop, "NM")) {
        LOGEM("Initializing NM "
              "(Failed to initialize OVSDB)");
        return -1;
    }

    if (!nm2_iface_init())
    {
        LOGEM("Initializing NM "
              "(Failed to initialize network API)");
        return -1;
    }

    nm2_inet_config_init();
    nm2_inet_state_init();
    nm2_mac_learning_init();
    nm2_dhcp_rip_init();
    nm2_portfw_init();
    nm2_route_init();
    nm2_mac_tags_ovsdb_init();
    nm2_ip_interface_init();
    nm2_ipv6_address_init();
    nm2_ipv6_prefix_init();
    nm2_dhcpv6_client_init();
    nm2_dhcpv6_server_init();
    nm2_dhcp_option_init();
    nm2_ipv6_routeadv_init();

    ev_run(loop, 0);

    if (!ovsdb_stop_loop(loop)) {
        LOGE("Stopping NM "
             "(Failed to stop OVSDB");
    }

    ev_default_destroy();

    LOGN("Exiting NM");

    return 0;
}
