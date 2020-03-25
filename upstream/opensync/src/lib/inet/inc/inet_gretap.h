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

#ifndef INET_GRETAP_H_INCLUDED
#define INET_GRETAP_H_INCLUDED

#include "inet.h"
#include "inet_base.h"
#include "inet_eth.h"

typedef struct __inet_gretap inet_gretap_t;

struct __inet_gretap
{
    /* Subclass from inet_eth_t, include base and inet for convenience */
    union
    {
        inet_t      inet;
        inet_base_t base;
        inet_eth_t  eth;
    };

    char            in_ifparent[C_IFNAME_LEN];      /* Parent interface */
    osn_ip_addr_t   in_local_addr;                  /* Local IPv4 address */
    osn_ip_addr_t   in_remote_addr;                 /* Remote IPv4 address */

};

extern inet_t *inet_gretap_new(const char *ifname);
extern bool inet_gretap_init(inet_gretap_t *self, const char *ifname);

extern bool inet_gretap_ip4tunnel_set(
        inet_t *super,
        const char *parent,
        osn_ip_addr_t laddr,
        osn_ip_addr_t raddr,
        osn_mac_addr_t rmac);

extern bool inet_gretap_service_commit(inet_base_t *super, enum inet_base_services srv, bool enable);

#endif /* INET_GRETAP_H_INCLUDED */
