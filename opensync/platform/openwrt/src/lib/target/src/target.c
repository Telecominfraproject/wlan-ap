/*
Copyright (c) 2017, Plume Design Inc. All rights reserved.

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

#include <stdio.h>

#include "evsched.h"
#include "os.h"
#include "os_nif.h"
#include "log.h"
#include "const.h"

#include "target.h"

struct ev_loop *wifihal_evloop = NULL;

/******************************************************************************
 *  TARGET definitions
 *****************************************************************************/

bool target_ready(struct ev_loop *loop)
{
    wifihal_evloop = loop;
    return true;
}

bool target_init(target_init_opt_t opt, struct ev_loop *loop)
{
#if 0
    if (!target_map_ifname_init())
    {
        LOGE("Target init failed to initialize interface mapping");
        return false;
    }
#endif

    wifihal_evloop = loop;

    switch (opt)
    {
        case TARGET_INIT_MGR_SM:
            break;

        case TARGET_INIT_MGR_WM:
            if (evsched_init(loop) == false)
            {
                LOGE("Initializing WM "
                        "(Failed to initialize EVSCHED)");
                return -1;
            }

//            sync_init(SYNC_MGR_WM, NULL);
            break;

        case TARGET_INIT_MGR_CM:
//            sync_init(SYNC_MGR_CM, cloud_config_mode_init);
            break;

        case TARGET_INIT_MGR_BM:
            break;

        default:
            break;
    }

    return true;
}

bool target_close(target_init_opt_t opt, struct ev_loop *loop)
{
    switch (opt)
    {
        case TARGET_INIT_MGR_WM:
//            sync_cleanup();
            /* fall through */

        case TARGET_INIT_MGR_SM:
            break;

        default:
            break;
    }

    target_map_close();

    return true;
}
#if 0
const char* target_persistent_storage_dir(void)
{
    return TARGET_PERSISTENT_STORAGE;
}

const char* target_scripts_dir(void)
{
    return TARGET_SCRIPTS_PATH;
}

const char* target_tools_dir(void)
{
    return TARGET_TOOLS_PATH;
}

const char* target_bin_dir(void)
{
    return TARGET_BIN_PATH;
}

const char* target_speedtest_dir(void)
{
    return target_tools_dir();
}
#endif
