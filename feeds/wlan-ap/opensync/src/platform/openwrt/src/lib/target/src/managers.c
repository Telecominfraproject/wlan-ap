/*
Copyright (c) 2019, Plume Design Inc. All rights reserved.

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
#include "const.h"

/******************************************************************************
 *  MANAGERS definitions
 *****************************************************************************/

target_managers_config_t target_managers_config[] =
{
	{
                .name = TARGET_MANAGER_PATH("cm"),
                .needs_plan_b = false,
        }, {
                .name = TARGET_MANAGER_PATH("qm"),
                .needs_plan_b = false,
        }, {
		.name = TARGET_MANAGER_PATH("sm"),
		.needs_plan_b = false,
	}, {
		.name = TARGET_MANAGER_PATH("wm"),
		.needs_plan_b = false,
	}, {
		.name = TARGET_MANAGER_PATH("nm"),
		.needs_plan_b = false,
	}, {
		.name = TARGET_MANAGER_PATH("lm"),
		.needs_plan_b = false,
	}, {
		.name = TARGET_MANAGER_PATH("cmdm"),
		.needs_plan_b = false,
	}, {
		.name = TARGET_MANAGER_PATH("rrm"),
		.needs_plan_b = false,
	}, {
		.name = TARGET_MANAGER_PATH("um"),
		.needs_plan_b = false,
	}, {
		.name = TARGET_MANAGER_PATH("uccm"),
		.needs_plan_b = false,
	},

};

int target_managers_num = ARRAY_SIZE(target_managers_config);
