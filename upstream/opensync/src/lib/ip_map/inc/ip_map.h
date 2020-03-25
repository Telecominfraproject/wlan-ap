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

#ifndef IP_MAP_H_INCLUDED
#define IP_MAP_H_INCLUDED

#include "ds_tree.h"
#include "ovsdb_update.h"
#include "schema.h"

//#define MAX_RESOLVED_ADDRS 32
#define IP_MAP_MAC_ADDR_SIZE 6

struct ip_addr_mac_map
{
    uint32_t v4;
    uint8_t mac_addr[IP_MAP_MAC_ADDR_SIZE];
};

struct ip_map
{
    ds_tree_node_t node;
    struct ip_addr_mac_map map;
};

struct ip_map_mgr
{
    int ipv4_cnt;
    int initialized;
    ds_tree_t ip_tree;           // DS tree node
    void (*ovsdb_init)(void);
};



struct ip_map_mgr* ip_map_get_mgr();

/**
 * @brief initialize ip_map handle.
 *
 * receive none
 *
 * @return 0 for success and 1 for failure .
 */
int ip_map_init(void);

/**
 * @brief cleanup allocatef memody.
 *
 * receive none
 *
 * @return void.
 */
void ip_map_cleanup(void);

/**
 * @brief map single ip with mac.
 *
 * receive ip_addr_mac_map pointer to ip.
 *
 * @return return 1 on success and 0 for fail.
 */
int ip_map_to_mac(struct ip_addr_mac_map *req);

/**
 * @brief map all the list of ip with mac.
 *
 * receive count:  number of element in list.
 * receive ip_addr_mac_map pointer to the list.
 *
 * @return number of successful mapping.
 */
int ip_map_list_to_mac(uint8_t count, struct ip_addr_mac_map *req);

struct ip_map* ip_map_lookup(uint32_t ip);

#endif /* IP_MAP_H_INCLUDED */
