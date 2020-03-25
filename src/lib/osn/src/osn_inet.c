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

#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>

#include "osn_inet.h"
#include "osn_types.h"

#include "log.h"
#include "util.h"
#include "const.h"
#include "execsh.h"
#include "ds_tree.h"


struct osn_ip
{
    char                ip_ifname[C_IFNAME_LEN];        /* Interface name */
    ds_tree_t           ip_addr_list;                   /* List of IPv4 addresses */
    ds_tree_t           ip_dns_list;                    /* List of DNS addresses */
    ds_tree_t           ip_route_gw_list;               /* Gateway routes */
};

struct ip_addr_node
{
    osn_ip_addr_t       addr;
    ds_tree_node_t      tnode;                          /* Tree node */
};

struct ip_route_gw_node
{
    osn_ip_addr_t       src;                            /* Source subnet */
    osn_ip_addr_t       gw;                             /* Destination gateway */
    ds_tree_node_t      tnode;
};

static bool ip_addr_flush(osn_ip_t *self);
static bool ip_route_flush(osn_ip_t *self);

/* execsh commands */
static char ip_addr_add_cmd[] = _S(ip address add "$2/$3" broadcast "+" dev "$1");
static char ip_addr_flush_cmd[] = _S(ip -4 address flush dev "$1");

static char ip_route_gw_add_cmd[] = _S(ip route add "$2" via "$3" dev "$1");

/* Scope global doesn't flush "local" or "link" routes */
static char ip_route_gw_flush_cmd[] = _S(ip -4 route flush dev "$1" scope global);

/*
 * Create new inet instance
 */
osn_ip_t *osn_ip_new(const char *ifname)
{
    osn_ip_t *self = calloc(1, sizeof(*self));

    if (STRSCPY(self->ip_ifname, ifname) < 0)
    {
        LOG(ERR, "inet: Interface name too long: %s", ifname);
        goto error;
    }

    ds_tree_init(
            &self->ip_addr_list,
            osn_ip_addr_cmp,
            struct ip_addr_node, tnode);

    ds_tree_init(
            &self->ip_dns_list,
            osn_ip_addr_cmp,
            struct ip_addr_node, tnode);

    ds_tree_init(
            &self->ip_route_gw_list,
            osn_ip_addr_cmp,
            struct ip_route_gw_node, tnode);

    return self;

error:
    if (self != NULL) free(self);

    return NULL;
}

/*
 * Delete inet instance
 */
bool osn_ip_del(osn_ip_t *self)
{
    struct ip_addr_node *node;
    struct ip_route_gw_node *rnode;
    ds_tree_iter_t iter;

    /* Flush routes */
    ip_route_flush(self);

    /* Remove all active addresses */
    ip_addr_flush(self);

    /* Free list of IPv4 address */
    ds_tree_foreach_iter(&self->ip_addr_list, node, &iter)
    {
        ds_tree_iremove(&iter);
        free(node);
    }

    /* Free list of DNSv4 addresses */
    ds_tree_foreach_iter(&self->ip_dns_list, node, &iter)
    {
        ds_tree_iremove(&iter);
        free(node);
    }

    /* Free list of gateway routes */
    ds_tree_foreach_iter(&self->ip_route_gw_list, rnode, &iter)
    {
        ds_tree_iremove(&iter);
        free(rnode);
    }

    free(self);

    return true;
}

/*
 * Flush all configured IPv4 routes
 */
bool ip_route_flush(osn_ip_t *self)
{
    int rc;

    rc = execsh_log(LOG_SEVERITY_INFO, ip_route_gw_flush_cmd, self->ip_ifname);
    if (rc != 0)
    {
        LOG(WARN, "inet: %s: Unable to flush IPv4 routes.", self->ip_ifname);
        return false;
    }

    return true;
}


/*
 * Flush all configured IPv4 addresses
 */
bool ip_addr_flush(osn_ip_t *self)
{
    int rc;

    rc = execsh_log(LOG_SEVERITY_INFO, ip_addr_flush_cmd, self->ip_ifname);
    if (rc != 0)
    {
        LOG(WARN, "inet: %s: Unable to flush IPv4 addresses.", self->ip_ifname);
        return false;
    }

    return true;
}

/*
 * Apply configuration to system
 */
bool osn_ip_apply(osn_ip_t *self)
{
    struct ip_addr_node *node;
    struct ip_route_gw_node *rnode;

    char saddr[OSN_IP_ADDR_LEN];
    char sgw[OSN_IP_ADDR_LEN];
    char spref[C_INT32_LEN];
    int rc;

    /* Start by issuing a flush */
    ip_addr_flush(self);
    ip_route_flush(self);

    /* First apply IPv4 addresses */
    ds_tree_foreach(&self->ip_addr_list, node)
    {
        if (inet_ntop(AF_INET, &node->addr.ia_addr, saddr, sizeof(saddr)) == NULL)
        {
            LOG(ERR, "inet: %s: Unable to convert IPv4 address, skipping add.", self->ip_ifname);
            continue;
        }

        snprintf(spref, sizeof(spref), "%d", node->addr.ia_prefix);

        rc = execsh_log(LOG_SEVERITY_INFO, ip_addr_add_cmd, self->ip_ifname, saddr, spref);
        if (rc != 0)
        {
            LOG(WARN, "inet: %s: Unable to add IPv4 address: "PRI_osn_ip_addr,
                    self->ip_ifname,
                    FMT_osn_ip_addr(node->addr));
            continue;
        }
    }

    /* Apply IPv4 routes */
    ds_tree_foreach(&self->ip_route_gw_list, rnode)
    {
        if (inet_ntop(AF_INET, &rnode->src.ia_addr, saddr, sizeof(saddr)) == NULL)
        {
            LOG(ERR, "inet: %s: Unable to convert IPv4 source address, skipping.", self->ip_ifname);
            continue;
        }

        if (inet_ntop(AF_INET, &rnode->gw.ia_addr, sgw, sizeof(sgw)) == NULL)
        {
            LOG(ERR, "inet: %s: Unable to convert IPv4 gateway address, skipping.", self->ip_ifname);
            continue;
        }

        rc = execsh_log(LOG_SEVERITY_INFO, ip_route_gw_add_cmd, self->ip_ifname, saddr, sgw);
        if (rc != 0)
        {
            LOG(WARN, "inet: %s: Unable to add IPv4 gateway route: "PRI_osn_ip_addr" -> "PRI_osn_ip_addr,
                    self->ip_ifname,
                    FMT_osn_ip_addr(rnode->src),
                    FMT_osn_ip_addr(rnode->gw));
            continue;
        }
    }

    return true;
}

bool osn_ip_addr_add(osn_ip_t *self, const osn_ip_addr_t *addr)
{
    struct ip_addr_node *node;

    node = ds_tree_find(&self->ip_addr_list, (void *)addr);
    if (node == NULL)
    {
        node = calloc(1, sizeof(*node));
        node->addr = *addr;
        ds_tree_insert(&self->ip_addr_list, node, &node->addr);
    }
    else
    {
        node->addr = *addr;
    }

    return true;
}

bool osn_ip_addr_del(osn_ip_t *ip, const osn_ip_addr_t *dns)
{
    struct ip_addr_node *node;

    node = ds_tree_find(&ip->ip_addr_list, (void *)dns);
    if (node == NULL)
    {
        LOG(ERR, "inet: %s: Unable to remove IPv4 address: "PRI_osn_ip_addr". Not found.",
                ip->ip_ifname,
                FMT_osn_ip_addr(*dns));
        return false;
    }

    ds_tree_remove(&ip->ip_addr_list, node);

    free(node);

    return true;
}

bool osn_ip_route_gw_add(osn_ip_t *ip, const osn_ip_addr_t *src, const osn_ip_addr_t *gw)
{
    struct ip_route_gw_node *rnode;

    rnode = ds_tree_find(&ip->ip_route_gw_list, (void *)src);
    if (rnode == NULL)
    {
        rnode = malloc(sizeof(struct ip_route_gw_node));
        rnode->src = *src;
        rnode->gw = *gw;

        ds_tree_insert(&ip->ip_route_gw_list, rnode, &rnode->src);
    }
    else
    {
        /* Update the gateway */
        rnode->gw = *gw;
    }

    return true;
}

bool osn_ip_route_gw_del(osn_ip_t *ip, const osn_ip_addr_t *src, const osn_ip_addr_t *gw)
{
    struct ip_route_gfw_node *rnode;

    rnode = ds_tree_find(&ip->ip_addr_list, (void *)src);
    if (rnode == NULL)
    {
        LOG(ERR, "inet: %s: Unable to remove IPv4 gateway route: "PRI_osn_ip_addr" -> "PRI_osn_ip_addr,
                ip->ip_ifname,
                FMT_osn_ip_addr(*src),
                FMT_osn_ip_addr(*gw));

        return false;
    }

    ds_tree_remove(&ip->ip_route_gw_list, rnode);

    free(rnode);

    return true;
}

bool osn_ip_dns_add(osn_ip_t *ip, const osn_ip_addr_t *dns)
{
    (void)ip;
    (void)dns;

    LOG(NOTICE, "inet: Unsupported DNS server configuration. Cannot add: "PRI_osn_ip_addr,
            FMT_osn_ip_addr(*dns));

    return false;
}

bool osn_ip_dns_del(osn_ip_t *ip, const osn_ip_addr_t *addr)
{
    (void)ip;
    (void)addr;

    LOG(NOTICE, "inet: Unsupported DNS server configuration. Cannot del:" PRI_osn_ip_addr,
            FMT_osn_ip_addr(*addr));

    return false;
}

