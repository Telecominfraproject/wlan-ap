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

#ifndef OS_NIF_H_INCLUDED
#define OS_NIF_H_INCLUDED

#include <stdbool.h>

#include "os.h"
#include "ds_list.h"
#include "os_types.h"

/**
 * This structure is used as an entry in the return list of os_nif_list_get()
 */
struct os_nif_list_entry
{
    char            le_ifname[64];
    ds_list_node_t  le_node;
};

extern os_ipaddr_t os_ipaddr_any;

extern bool    os_nif_exists(char *ifname, bool *exists);
extern bool    os_nif_ipaddr_get(char* ifname, os_ipaddr_t* addr);
extern bool    os_nif_netmask_get(char* ifname, os_ipaddr_t* addr);
extern bool    os_nif_bcast_get(char* ifname, os_ipaddr_t* addr);
extern bool    os_nif_ipaddr_set(char* ifname, os_ipaddr_t addr);
extern bool    os_nif_netmask_set(char* ifname, os_ipaddr_t addr);
extern bool    os_nif_bcast_set(char* ifname, os_ipaddr_t addr);
extern bool    os_nif_mtu_get(char* ifname, int *mtu);
extern bool    os_nif_mtu_set(char* ifname, int mtu);
extern bool    os_nif_gateway_set(char* ifname, os_ipaddr_t gwaddr);
extern bool    os_nif_gateway_del(char* ifname, os_ipaddr_t gwaddr);
extern bool    os_nif_macaddr(char* ifname, os_macaddr_t *mac);
extern bool    os_nif_macaddr_get(char* ifname, os_macaddr_t *mac);
extern bool    os_nif_macaddr_set(char* ifname, os_macaddr_t mac);
extern bool    os_nif_up(char* ifname, bool ifup);
extern bool    os_nif_is_up(char* ifname, bool *up);
extern bool    os_nif_is_running(char* ifname, bool *running);
extern bool    os_nif_dhcpc_start(char* ifname, bool apply, int dhcp_time);
extern bool    os_nif_dhcpc_stop(char* ifname, bool dryrun);
extern bool    os_nif_dhcpc_refresh_lease(char* ifname);
extern bool    os_nif_softwds_create(
                                char* ifname,
                                char* parent,
                                os_macaddr_t* addr,
                                bool wrap);
extern bool    os_nif_softwds_destroy(char *ifname);
extern bool    os_nif_list_get(ds_list_t *list);
extern void             os_nif_list_free(ds_list_t *list);
extern bool    os_nif_br_add(char* ifname, char* br);
extern bool    os_nif_br_del(char* ifname);
extern bool    os_nif_ipaddr_from_str(os_ipaddr_t *ipaddr, const char* str);
extern bool    os_nif_macaddr_from_str(os_macaddr_t* mac, const char* str);
extern bool    os_nif_macaddr_to_str(os_macaddr_t* mac, char* str, const char* format);
extern pid_t   os_nif_pppoe_pidof(const char *ifname);
extern bool    os_nif_pppoe_start(const char *ifname, const char *ifparent, const char *username, const char *password);
extern bool    os_nif_pppoe_stop(const char *ifname);
extern bool    os_nif_is_interface_ready(char *if_name);

extern int     os_nif_ioctl(int cmd, void *buf);

#endif /* OS_NIF_H_INCLUDED */
