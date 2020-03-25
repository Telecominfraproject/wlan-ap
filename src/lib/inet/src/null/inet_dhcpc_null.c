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
 * ===========================================================================
 *  Dummy implementation of the DHCP client class
 * ===========================================================================
 */
#include <stdlib.h>

#include "const.h"
#include "util.h"
#include "log.h"
#include "daemon.h"

#include "inet_dhcpc.h"

struct __inet_dhcpc
{
};

inet_dhcpc_t *inet_dhcpc_new(const char *ifname)
{
    LOG(WARN, "inet_dhcpc: %s: DHCP client not supported on platform.", ifname);
    return (void *)0xdeadc0de;
}

bool inet_dhcpc_del(inet_dhcpc_t *self)
{
    (void)self;
    return true;
}

bool inet_dhcpc_start(inet_dhcpc_t *self)
{
    (void)self;
    return true;
}

bool inet_dhcpc_stop(inet_dhcpc_t *self)
{
    (void)self;
    return true;
}

bool inet_dhcpc_opt_request(inet_dhcpc_t *self, enum inet_dhcp_option opt, bool request)
{
    (void)self;
    (void)opt;
    (void)request;

    return true;
}

bool inet_dhcpc_opt_set(
        inet_dhcpc_t *self,
        enum inet_dhcp_option opt,
        const char *val)
{
    (void)self;
    (void)opt;
    (void)val;

    return true;
}

bool inet_dhcpc_opt_get(inet_dhcpc_t *self, enum inet_dhcp_option opt, bool *request, const char **value)
{
    *request = false;
    *value = NULL;
    return true;
}

bool inet_dhcpc_error_fn_set(inet_dhcpc_t *self, inet_dhcpc_error_fn_t *errfn)
{
    (void)self;
    (void)errfn;

    return true;
}

bool inet_dhcpc_state_get(inet_dhcpc_t *self, bool *enabled)
{
    (void)self;

    *enabled = false;

    return true;
}

bool inet_dhcpc_opt_notify_set(inet_dhcpc_t *self, inet_dhcpc_option_notify_fn_t *fn, void *ctx)
{
    (void)self;
    (void)fn;
    (void)ctx;

    return true;
}
