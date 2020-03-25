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

#ifndef NFM_INCLUDE_OSFW
#define NFM_INCLUDE_OSFW

#include "schema.h"
#include <ev.h>
#include <arpa/inet.h>
#include <stdbool.h>

struct nfm_osfw_base {
	struct ev_loop *loop;
	ev_timer timer;
};

bool nfm_osfw_init(struct ev_loop *loop);
bool nfm_osfw_fini(void);
bool nfm_osfw_is_inet4(const char *protocol);
bool nfm_osfw_is_inet6(const char *protocol);
bool nfm_osfw_add_chain(int family, const char *table, const char *chain);
bool nfm_osfw_del_chain(int family, const char *table, const char *chain);
bool nfm_osfw_add_rule(const struct schema_Netfilter *conf);
bool nfm_osfw_del_rule(const struct schema_Netfilter *conf);

#endif

