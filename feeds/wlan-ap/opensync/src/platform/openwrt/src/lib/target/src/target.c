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
#include "nl80211.h"

struct ev_loop *wifihal_evloop = NULL;
const struct target_radio_ops *radio_ops;

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
	wifihal_evloop = loop;
	target_map_init();

	target_map_insert("radio0", "phy0");
	target_map_insert("radio1", "phy1");
	target_map_insert("radio2", "phy2");

	switch (opt) {
	case TARGET_INIT_MGR_SM:
		if (evsched_init(loop) == false) {
			LOGE("Initializing SM (Failed to initialize EVSCHED)");
			return -1;
		}
		break;

	case TARGET_INIT_MGR_WM:
		if (evsched_init(loop) == false) {
			LOGE("Initializing WM (Failed to initialize EVSCHED)");
			return -1;
		}
		break;

	case TARGET_INIT_MGR_CM:
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
	switch (opt) {
	case TARGET_INIT_MGR_WM:
		/* fall through */

	case TARGET_INIT_MGR_SM:
		break;

	default:
		break;
	}

	target_map_close();

	return true;
}
