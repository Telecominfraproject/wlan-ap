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

#include "target.h"

#if defined(CONFIG_TARGET_MANAGER)

#define TARGET_MANAGER_PATH(x) CONFIG_TARGET_PATH_BIN "/" x
/******************************************************************************
 *  MANAGERS definitions
 *****************************************************************************/
target_managers_config_t target_managers_config[] =
{
#if defined(CONFIG_TARGET_MANAGER_WM)
    {
        .name = TARGET_MANAGER_PATH("wm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_NM)
    {
        .name = TARGET_MANAGER_PATH("nm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_CM)
    {
        .name = TARGET_MANAGER_PATH("cm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_SM)
    {
        .name =TARGET_MANAGER_PATH("sm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_UM)
    {
        .name = TARGET_MANAGER_PATH("um"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_LM)
    {
        .name = TARGET_MANAGER_PATH("lm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_BM)
    {
        .name = TARGET_MANAGER_PATH("bm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_FM)
    {
        .name = TARGET_MANAGER_PATH("fm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_LEDM)
    {
        .name = TARGET_MANAGER_PATH("ledm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_OM)
    {
        .name = TARGET_MANAGER_PATH("om"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_BLEM)
    {
        .name = TARGET_MANAGER_PATH("blem"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_QM)
    {
        .name = TARGET_MANAGER_PATH("qm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_TM)
    {
        .name =  TARGET_MANAGER_PATH("tm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_FSM)
    {
        .name =  TARGET_MANAGER_PATH("fsm"),
        .always_restart = 1,
        .restart_delay = -1,
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_FCM)
    {
        .name =  TARGET_MANAGER_PATH("fcm"),
        .always_restart = 1,
        .restart_delay = -1,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_XM)
    {
        .name =  TARGET_MANAGER_PATH("xm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_HELLO_WORLD)
    {
        .name =  TARGET_MANAGER_PATH("hello_world"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_PM)
    {
        .name = TARGET_MANAGER_PATH("pm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_PPM)
    {
        .name = TARGET_MANAGER_PATH("ppm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_NFM)
    {
        .name = TARGET_MANAGER_PATH("nfm"),
        .needs_plan_b = false,
    },
#endif

};
int target_managers_num =
    (sizeof(target_managers_config) / sizeof(target_managers_config[0]));

#endif /* CONFIG_TARGET_MANAGER */


