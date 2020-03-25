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

#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ev.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

#include "sm.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

/******************************************************************************
 *  PROTECTED API definitions
 *****************************************************************************/

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
bool sm_radio_config_enable_tx_stats(
        radio_entry_t              *radio_cfg)
{
    bool                            status;

    /* Skip radio tx stats enable if radio is not configured */
    if (!radio_cfg)
    {
        return true;
    }

    LOG(DEBUG,
        "Initializing %s radio tx_stats on %s",
        radio_get_name_from_cfg(radio_cfg),
        radio_cfg->phy_name);

    status =
        target_radio_tx_stats_enable(
                radio_cfg,
                true);
    if (true != status)
    {
        LOG(ERR,
                "Initializing %s radio tx_stats on %s",
                radio_get_name_from_cfg(radio_cfg),
                radio_cfg->phy_name);
        return false;
    }

    return true;
}

bool sm_radio_config_enable_fast_scan(
        radio_entry_t              *radio_cfg)
{
    bool                            status;

    /* Skip radio tx stats enable if radio is not configured */
    if (!radio_cfg)
    {
        return true;
    }

    status =
        target_radio_fast_scan_enable( // Change to radio_cfg
                radio_cfg,
                radio_cfg->if_name);
    if (true != status)
    {
        LOG(ERR,
            "Updating %s radio tx_stats on %s",
            radio_get_name_from_cfg(radio_cfg),
            radio_cfg->if_name);
        return false;
    }

    return true;
}
