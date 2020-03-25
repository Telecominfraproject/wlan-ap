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
 *  NULL implementation of the DNS resolver
 * ===========================================================================
 */

#include "log.h"
#include "inet_dns.h"

inet_dns_t *inet_dns_new(const char *ifname)
{
    (void)ifname;

    LOG(WARN, "inet_dns: %s: DNS resolver not available on platform.", ifname);

    return (void *)0xdeadc0de;
}

/**
 * Destructor
 */
bool inet_dns_del(inet_dns_t *self)
{
    (void)self;
    return true;
}

bool inet_dns_start(inet_dns_t *self)
{
    (void)self;
    return true;
}

/**
 * Stop the DNS service -- simply remove the per-interface resolv file and kick off a new
 * global resolv file update
 */
bool inet_dns_stop(inet_dns_t *self)
{
    (void)self;
    return true;
}

bool inet_dns_server_set(
        inet_dns_t *self,
        osn_ip_addr_t primary,
        osn_ip_addr_t secondary)
{
    (void)self;
    (void)primary;
    (void)secondary;

    return true;
}
