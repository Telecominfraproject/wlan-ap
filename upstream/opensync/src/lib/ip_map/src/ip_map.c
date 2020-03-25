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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <arpa/inet.h>

#include "util.h"
#include "os.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "target.h"
#include "target_common.h"

#include "ip_map.h"

ovsdb_table_t table_DHCP_leased_IP;

static void ip_map_print_tree(void);

static struct ip_map_mgr ctx_mgr = { 0 };

struct ip_map_mgr* ip_map_get_mgr(void)
{
    return &ctx_mgr;
}

static int ip_map_cmp(void *a, void *b)
{
    struct ip_map *ip_a = (struct ip_map *)a;
    struct ip_map *ip_b = (struct ip_map *)b;

    return ip_a->map.v4 - ip_b->map.v4;
}

struct ip_map* ip_map_lookup(uint32_t ip)
{
    struct ip_map_mgr *mgr = ip_map_get_mgr();
    struct ip_map key;
    struct ip_map *ip_node = NULL;

    key.map.v4 = ip;
    ip_node = ds_tree_find(&mgr->ip_tree, &key);

    return ip_node;
}


static void ip_map_print_tree(void)
{
    struct ip_map_mgr *mgr = ip_map_get_mgr();
    struct ip_map *ip_node = NULL;

    ds_tree_foreach(&mgr->ip_tree, ip_node)
    {
        char tmp1[INET_ADDRSTRLEN];
        const char *res;

        res = inet_ntop(AF_INET, &ip_node->map.v4, tmp1, INET_ADDRSTRLEN);
        if (res == NULL) return;

        LOGT("ip %s, mac %02x:%02x:%02x:%02x:%02x:%02x", tmp1,
            ip_node->map.mac_addr[0], ip_node->map.mac_addr[1],
            ip_node->map.mac_addr[2], ip_node->map.mac_addr[3],
            ip_node->map.mac_addr[4], ip_node->map.mac_addr[5]);
    }
}

static int ip_map_add(struct schema_DHCP_leased_IP *rec)
{
    struct ip_map_mgr *mgr = ip_map_get_mgr();
    struct ip_map *ip_node;
    struct in_addr addr;
    int ret;

    ret = inet_pton(AF_INET, rec->inet_addr, &addr.s_addr);
    if (ret <= 0) return -1;

    ip_node = ip_map_lookup(addr.s_addr);
    if (ip_node != NULL ) return -1;

    ip_node = calloc(sizeof(*ip_node), 1);
    if (ip_node == NULL) return -1;

    ip_node->map.v4 = addr.s_addr;
    ret = hwaddr_aton(rec->hwaddr, ip_node->map.mac_addr);
    if (ret == -1) goto err_free_ip_node;

    ds_tree_insert(&mgr->ip_tree, ip_node, ip_node);
    return 0;

err_free_ip_node:
    free(ip_node);
    return -1;
}

/**
 * ip_map_delete: delete specific ip from tree
 * @old_rec: the rec needs to remove
 */
static int ip_map_delete(struct schema_DHCP_leased_IP *old_rec)
{
    struct ip_map_mgr *mgr = ip_map_get_mgr();
    struct ip_map *ip_node;
    struct in_addr addr;
    int ret;

    ret = inet_pton(AF_INET, old_rec->inet_addr, &addr.s_addr);
    if (ret <= 0) return -1;

    ip_node = ip_map_lookup(addr.s_addr);
    if (ip_node == NULL) return -1;

    ds_tree_remove(&mgr->ip_tree, ip_node);
    free(ip_node);
    return 0;
}

/**
 * ip_map_update: add a FSM Policy
 * @old_rec: the ip needs update
 */
static int ip_map_update(struct schema_DHCP_leased_IP *old_rec,
                          struct schema_DHCP_leased_IP *new_rec)
{
    struct ip_map_mgr *mgr = ip_map_get_mgr();
    struct ip_map *ip_node;
    struct in_addr addr;
    int ret;

    ret = inet_pton(AF_INET, old_rec->inet_addr, &addr.s_addr);
    if (ret <= 0) return -1;

    ip_node = ip_map_lookup(addr.s_addr);
    if (ip_node == NULL) return -1;

    ret = inet_pton(AF_INET, new_rec->inet_addr, &addr.s_addr);
    if (ret <= 0) return -1;

    ip_node->map.v4 = addr.s_addr;
    ret = hwaddr_aton(new_rec->hwaddr, ip_node->map.mac_addr);
    if (ret == -1) return -1;

    /* remove and re-insert the same node */
    ds_tree_remove(&mgr->ip_tree, ip_node);
    ds_tree_insert(&mgr->ip_tree, ip_node, ip_node);
    return 0;
}


void callback_DHCP_leased_IP(ovsdb_update_monitor_t *mon,
                             struct schema_DHCP_leased_IP *old_rec,
                             struct schema_DHCP_leased_IP *new_rec)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW)
    {
        if (ip_map_add(new_rec))
            LOGE("ip_map_add error");
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        if (ip_map_delete(old_rec))
             LOGE("ip_map_delete error");
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        if (ip_map_update(old_rec, new_rec))
             LOGE("ip_map_update error");
    }
    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
        ip_map_print_tree();
}


void ip_map_ovsdb_init(void)
{
    OVSDB_TABLE_INIT_NO_KEY(DHCP_leased_IP);
    OVSDB_TABLE_MONITOR(DHCP_leased_IP, false);
}

/**
 * @brief initialize ip_map handle.
 *
 * receive none
 *
 * @return 0 for success and 1 for failure .
 */
int ip_map_init(void)
{
    struct ip_map_mgr *mgr = ip_map_get_mgr();

    if (mgr->initialized == 1) {
        LOGD("%s: already initialized", __func__);
        return 1;
    }

    LOGI("%s: initializing", __func__);
    ds_tree_init(&mgr->ip_tree, ip_map_cmp, struct ip_map, node);

    /* Set the default ovsdb_init callback */
    if (mgr->ovsdb_init == NULL) mgr->ovsdb_init = ip_map_ovsdb_init;

    mgr->ovsdb_init();
    mgr->initialized = 1;

    return 0;
}

/**
 * @brief cleanup allocatef memody.
 *
 * receive none
 *
 * @return void.
 */
void ip_map_cleanup(void)
{
    struct ip_map_mgr *mgr = ip_map_get_mgr();
    struct ip_map *ip_node = NULL;

    if (mgr->initialized == 0) return;

    while (!ds_tree_is_empty(&mgr->ip_tree))
    {
        ip_node = ds_tree_remove_tail(&mgr->ip_tree);
        free(ip_node);
    }
    mgr->ovsdb_init = NULL;
    mgr->initialized = 0;
}

/**
 * @brief map single ip with mac.
 *
 * receive ip_addr_mac_map pointer to ip.
 *
 * @return return 1 on success and 0 for fail.
 */
int ip_map_to_mac(struct ip_addr_mac_map *req)
{
    struct ip_map *ip_node = NULL;

    if (req == NULL) return 0;

    ip_node = ip_map_lookup(req->v4);
    if (ip_node == NULL)
    {
        memset(req->mac_addr, 0, sizeof(ip_node->map.mac_addr));
        return 0;
    }

    memcpy(req->mac_addr, ip_node->map.mac_addr, sizeof(ip_node->map.mac_addr));

    return 1;
}

/**
 * @brief map all the list of ip with mac.
 *
 * receive count:  number of element in list.
 * receive ip_addr_mac_map pointer to the list.
 *
 * @return number of successful mapping.
 */
int ip_map_list_to_mac(uint8_t count, struct ip_addr_mac_map *req)
{
    int i = 0;
    int ret = 0;

    if (req == NULL) return 0;

    for (i = 0; i < count; i++)
    {
        ret += ip_map_to_mac(req);
        req++;
    }
    return ret;
}
