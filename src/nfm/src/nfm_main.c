/*
* Copyright (c) 2019, Sagemcom.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "log.h"
#include "nfm_osfw.h"
#include "nfm_chain.h"
#include "nfm_rule.h"
#include "nfm_trule.h"
#include "nfm_ovsdb.h"
#include "os.h"
#include "target.h"
#include "os_backtrace.h"
#include "json_util.h"
#include "evsched.h"
#include "ovsdb.h"
#include <ev.h>

#define MODULE_ID LOG_MODULE_ID_MAIN

static log_severity_t nfm_log_severity = LOG_SEVERITY_INFO;

static void nfm_init(struct ev_loop *loop)
{
	nfm_osfw_init(loop);
	nfm_chain_init();
	nfm_rule_init();
	nfm_trule_init();
	nfm_ovsdb_init();
}

static void nfm_fini(void)
{
	nfm_osfw_fini();
}

int main(int argc, char **argv)
{
	struct ev_loop *loop = EV_DEFAULT;

	if (os_get_opt(argc, argv, &nfm_log_severity)) {
		LOGE("Initializing Netfilter manager: failed to get options");
		return -1;
	}

	target_log_open("NFM", 0);
	LOGN("Starting Netfilter manager - NFM");
	log_severity_set(nfm_log_severity);
	log_register_dynamic_severity(loop);

	backtrace_init();
	json_memdbg_init(loop);
	if (evsched_init(loop) == false) {
		LOGE("Initializing Netfilter manager: failed to initialize event loop");
		return -1;
	}

	if (!target_init(TARGET_INIT_MGR_NFM, loop)) {
		LOGE("Initializing Netfilter manager: failed to initialize target");
		return -1;
	}
	if (!ovsdb_init_loop(loop, "NFM")) {
		LOGE("Initializing Netfilter manager: failed to initialize OVS database");
		return -1;
	}
	nfm_init(loop);

	ev_run(loop, 0);

	nfm_fini();
	target_close(TARGET_INIT_MGR_NFM, loop);
	if (!ovsdb_stop_loop(loop)) {
		LOGE("Stopping Netfilter manager: failed to stop OVS database");
	}
	ev_default_destroy();
	LOGN("Exiting Netfilter manager - NFM");
	return 0;
}

