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

#include "osn_inet6.h"
#include "osn_types.h"
#include "osn_priv_netlink.h"

#include "log.h"
#include "util.h"
#include "const.h"
#include "execsh.h"
#include "ds_tree.h"

/*
 * Specify the increment by which dynamic arrays are grown each time they
 * require more space
 */
#define OSN_IP6_REALLOC_GROW    16

struct osn_ip6
{
    char                    ip6_ifname[C_IFNAME_LEN];   /* Interface name */
    ds_tree_t               ip6_addr_list;              /* List of IPv6 addresses */
    ds_tree_t               ip6_dns_list;               /* List of DNS addresses */
    osn_nl_t                ip6_nl;                     /* Netlink event object */
    struct osn_ip6_status   ip6_status;                 /* Current interface status */
    osn_ip6_status_fn_t    *ip6_status_fn;              /* Status callback */
};

struct ip6_addr_node
{
    osn_ip6_addr_t          addr;                       /* IPv6 address */
    bool                    enabled;                    /* IP address should be added, otherwise removed */
    bool                    active;                     /* IP address is applied to the system */
    ds_tree_node_t          tnode;                      /* Tree node */
};

static bool ip6_addr_flush(osn_ip6_t *self);
static osn_nl_fn_t osn_ip6_nl_fn;

/*
 * Create new inet6 instance
 */
osn_ip6_t *osn_ip6_new(const char *ifname)
{
    osn_ip6_t *self = calloc(1, sizeof(*self));

    if (STRSCPY(self->ip6_ifname, ifname) < 0)
    {
        LOG(ERR, "inet6: Interface name too long: %s", ifname);
        goto error;
    }

    ds_tree_init(
            &self->ip6_addr_list,
            osn_ip6_addr_cmp,
            struct ip6_addr_node, tnode);

    ds_tree_init(
            &self->ip6_dns_list,
            osn_ip6_addr_cmp,
            struct ip6_addr_node, tnode);

    /* Initialize the netlink event object */
    osn_nl_init(&self->ip6_nl, osn_ip6_nl_fn);
    osn_nl_set_events(&self->ip6_nl, OSN_NL_IP6ADDR | OSN_NL_IP6NEIGH);
    osn_nl_set_ifname(&self->ip6_nl, ifname);

    /* Start listening to netlink events */
    osn_nl_start(&self->ip6_nl);

    return self;

error:
    if (self != NULL) free(self);

    return NULL;
}

/*
 * Delete inet6 instance
 */
bool osn_ip6_del(osn_ip6_t *self)
{
    struct ip6_addr_node *node;
    ds_tree_iter_t iter;

    /* Stop the netlink event listener */
    osn_nl_stop(&self->ip6_nl);

    /* Free status structure */
    if (self->ip6_status.is6_addr != NULL)
    {
        free(self->ip6_status.is6_addr);
    }

    if (self->ip6_status.is6_neigh != NULL)
    {
        free(self->ip6_status.is6_neigh);
    }

    if (self->ip6_status.is6_dns != NULL)
    {
        free(self->ip6_status.is6_dns);
    }

    /* Remove all active addresses */
    ip6_addr_flush(self);

    /* Free list of IPv6 address */
    ds_tree_foreach_iter(&self->ip6_addr_list, node, &iter)
    {
        ds_tree_iremove(&iter);
        free(node);
    }

    /* Free list of DNSv6 addresses */
    ds_tree_foreach_iter(&self->ip6_dns_list, node, &iter)
    {
        ds_tree_iremove(&iter);
        free(node);
    }

    free(self);

    return true;
}

static char ip6_del_cmd[] = _S(ip -6 address del "$2/$3" dev "$1");
static char ip6_add_cmd[] = _S(ip -6 address add "$2/$3" dev "$1");

/*
 * Flush all configured IPv6 interfaces
 *
 * Note: A simple ip -6 addr flush would in theory work. The problem with that command is
 * that it flushes, amongst others, the link-local address. In order to work around that
 * we have to keep track of IP addresses that are to be removed.
 */
bool ip6_addr_flush(osn_ip6_t *self)
{
    int rc;
    ds_tree_iter_t iter;
    struct ip6_addr_node *node;
    char saddr[OSN_IP6_ADDR_LEN];
    char spref[C_INT32_LEN];

    ds_tree_foreach_iter(&self->ip6_addr_list, node, &iter)
    {
        /*
         *  active |  enabled |  action
         *  -------------------------------
         *  true      true       remove ip,active=false
         *  true      false      remove ip,remove from list
         *  false     true       nop
         *  false     false      remove from list
         */
        if (node->active)
        {
            /* Remove IP from the system */
            //snprintf(saddr, sizeof(saddr),  PRI(in6_addr), FMT(in6_addr, node->addr.ia6_addr));
            if (inet_ntop(AF_INET6, &node->addr.ia6_addr, saddr, sizeof(saddr)) == NULL)
            {
                LOG(ERR, "inet6: %s: Unable to convert IPv6 address, skipping delete.", self->ip6_ifname);
            }

            snprintf(spref, sizeof(spref), "%d", node->addr.ia6_prefix);

            rc = execsh_log(LOG_SEVERITY_DEBUG, ip6_del_cmd, self->ip6_ifname, saddr, spref);
            if (rc != 0)
            {
                LOG(WARN, "inet6: %s: Unable to remove IPv6 address: "PRI_osn_ip6_addr,
                        self->ip6_ifname,
                        FMT_osn_ip6_addr(node->addr));
            }

            node->active = false;
        }

        /* Remove element from the list */
        if (!node->enabled)
        {
            ds_tree_iremove(&iter);
            free(node);
        }
    }

    return true;
}

/*
 * Apply configuration to system
 */
bool osn_ip6_apply(osn_ip6_t *self)
{
    struct ip6_addr_node *node;
    char saddr[C_IPV6ADDR_LEN];
    char spref[C_INT32_LEN];
    int rc;

    /* Start by issuing a flush */
    ip6_addr_flush(self);

    ds_tree_foreach(&self->ip6_addr_list, node)
    {
        if (inet_ntop(AF_INET6, &node->addr.ia6_addr, saddr, sizeof(saddr)) == NULL)
        {
            LOG(ERR, "inet6: %s: Unable to convert IPv6 address, skipping add.", self->ip6_ifname);
            continue;
        }

        snprintf(spref, sizeof(spref), "%d", node->addr.ia6_prefix);

        rc = execsh_log(LOG_SEVERITY_DEBUG, ip6_add_cmd, self->ip6_ifname, saddr, spref);
        if (rc != 0)
        {
            LOG(WARN, "inet6: %s: Unable to add IPv6 address: "PRI_osn_ip6_addr,
                    self->ip6_ifname,
                    FMT_osn_ip6_addr(node->addr));
        }

        node->active = true;
    }

    return true;
}

bool osn_ip6_addr_add(osn_ip6_t *self, const osn_ip6_addr_t *addr)
{
    struct ip6_addr_node *node;

    node = ds_tree_find(&self->ip6_addr_list, (void *)&addr->ia6_addr);
    if (node == NULL)
    {
        node = calloc(1, sizeof(*node));
        node->addr = *addr;
        ds_tree_insert(&self->ip6_addr_list, node, &node->addr.ia6_addr);
    }
    else
    {
        node->addr = *addr;
    }

    node->enabled = true;

    return true;
}

bool osn_ip6_addr_del(osn_ip6_t *ip6, const osn_ip6_addr_t *dns)
{
    struct ip6_addr_node *node;

    node = ds_tree_find(&ip6->ip6_addr_list, (void *)&dns->ia6_addr);
    if (node == NULL)
    {
        LOG(ERR, "inet6: %s: Unable to remove IPv6 address: "PRI_osn_ip6_addr". Not found.",
                ip6->ip6_ifname,
                FMT_osn_ip6_addr(*dns));
        return false;
    }

    node->enabled = false;

    return true;
}

bool osn_ip6_dns_add(osn_ip6_t *ip6, const osn_ip6_addr_t *dns)
{
    (void)ip6;
    (void)dns;

    LOG(NOTICE, "inet6: Unsupported DNSv6 server. Cannot add: "PRI_osn_ip6_addr,
            FMT_osn_ip6_addr(*dns));

    return false;
}

bool osn_ip6_dns_del(osn_ip6_t *ip6, const osn_ip6_addr_t *addr)
{
    (void)ip6;
    (void)addr;

    LOG(NOTICE, "inet6: Unsupported DNSv6 server. Cannot del:" PRI_osn_ip6_addr,
            FMT_osn_ip6_addr(*addr));

    return false;
}

/**
 * Parse a single line of a "ip -o -6 addr show dev IF" output.
 */
bool osn_ip6_ipaddr_parse(void *_self, int type, const char *line)
{
    char *svalid_lft;
    char *spref_lft;
    char buf[256];
    char *pbuf;
    char *sip6;
    int lft;

    osn_ip6_t *self = _self;

    LOG(DEBUG, "inet6: %s: ip_addr_show%s %s",
            self->ip6_ifname,
            type == EXECSH_PIPE_STDOUT ? ">" : "|",
            line);

    if (type != EXECSH_PIPE_STDOUT) return true;

    STRSCPY(buf, line);

    /*
     * Example output:
     *
     * # ip -o -6 addr show dev br-wan
     * 20: br-wan    inet6 2a00:ee2:1704:9901::2000/128 scope global dynamic \       valid_lft 237sec preferred_lft 124sec
     * 20: br-wan    inet6 fe80::62b4:f7ff:fef0:15c8/64 scope link \       valid_lft forever preferred_lft forever
     */

    /* Skip interface index */
    strtok_r(buf, " ", &pbuf);

    /* Skip interface name */
    strtok_r(NULL, " ", &pbuf);

    /* Skip "inet6" */
    strtok_r(NULL, " ", &pbuf);

    /* IPv6 address, includes the prefix */
    sip6 = strtok_r(NULL, " ", &pbuf);

    /*
     * It seems that there's a variable amount of keywords after the IPv6 address
     * Skip all keywords until we encounter the "valid_lft" string
     */
    while ((svalid_lft = strtok_r(NULL, " ", &pbuf)) != NULL)
    {
        if (strcmp(svalid_lft, "valid_lft") == 0) break;
    }

    /* Parse the vlaid_lft value */
    svalid_lft = strtok_r(NULL, " ", &pbuf);

    /* Skip "preferred_lft" */
    strtok_r(NULL, " ", &pbuf);

    /* Skip "preferred_lft" */
    spref_lft = strtok_r(NULL, " ", &pbuf);

    struct osn_ip6_status *is = &self->ip6_status;

    /*
     * Resize array in OSN_IP6_REALLOC_GROW increments
     */
    if ((is->is6_addr_len % OSN_IP6_REALLOC_GROW) == 0)
    {
        is->is6_addr = realloc(
                is->is6_addr,
                (is->is6_addr_len + OSN_IP6_REALLOC_GROW) * sizeof(is->is6_addr[0]));
    }

    if (!osn_ip6_addr_from_str(&is->is6_addr[is->is6_addr_len], sip6))
    {
        LOG(WARN, "inet6: %s: Error parsing IPv6 addresses -- invalid IPv6 address \"%s\" on line: %s",
                self->ip6_ifname,
                sip6,
                line);
        return true;
    }

    if (strcmp(svalid_lft, "infinite") == 0)
    {
        is->is6_addr[is->is6_addr_len].ia6_valid_lft = -1;
    }
    else
    {
        lft = atol(svalid_lft);
        if (lft == 0) lft = INT_MIN;
        is->is6_addr[is->is6_addr_len].ia6_valid_lft = lft;
    }

    if (strcmp(spref_lft, "infinite") == 0)
    {
        is->is6_addr[is->is6_addr_len].ia6_pref_lft = -1;
    }
    else
    {
        lft = atol(spref_lft);
        if (lft == 0) lft = INT_MIN;
        is->is6_addr[is->is6_addr_len].ia6_pref_lft = lft;
    }

    LOG(DEBUG, "inet6: %s: IPv6 address = "PRI_osn_ip6_addr,
            self->ip6_ifname,
            FMT_osn_ip6_addr(is->is6_addr[is->is6_addr_len]));

    is->is6_addr_len++;

    return true;
}

void osn_ip6_status_ipaddr_update(osn_ip6_t *self)
{
    int rc;

    if (self->ip6_status.is6_addr != NULL)
    {
        free(self->ip6_status.is6_addr);
    }
    self->ip6_status.is6_addr_len = 0;
    self->ip6_status.is6_addr = NULL;

    rc = execsh_fn(osn_ip6_ipaddr_parse, self, _S(ip -6 -o addr show dev "$1"), self->ip6_ifname);
    if (rc != 0)
    {
        LOG(DEBUG, "inet6: %s: \"ip -o -6 addr show\" returned error %d. IPv6 address report may be incomplete.",
                self->ip6_ifname,
                rc);
    }

    LOG(INFO, "inet6: %s: Found %zu IPv6 address(es).", self->ip6_ifname, self->ip6_status.is6_addr_len);
}
/**
 * Parse a single line of a "ip -6 neigh show" output.
 */
bool osn_ip6_neigh_parse(void *_self, int type, const char *line)
{
    char buf[256];
    char *pbuf;
    char *saddr;
    char *smac;
    char *sll;

    osn_ip6_t *self = _self;

    LOG(DEBUG, "inet6: %s: ip_neigh_show%s %s",
            self->ip6_ifname,
            type == EXECSH_PIPE_STDOUT ? ">" : "|",
            line);

    if (type != EXECSH_PIPE_STDOUT) return true;

    STRSCPY(buf, line);

    /*
     * Example output:
     *
     * fe80::ae1f:6bff:feb0:55ef lladdr ac:1f:6b:b0:55:ef router STALE
     */
    saddr = strtok_r(buf, " ", &pbuf);

    /*
     * The next string may contain "lladdr" or "FAILED" -- skip this whole line
     * if the next entry is not "lladdr"
     */
    sll = strtok_r(NULL, " ", &pbuf);
    if (strcmp(sll, "lladdr") != 0)
    {
        /* Skip this entry */
        return true;
    }

    /* Parse the MAC */
    smac = strtok_r(NULL, " ", &pbuf);

    if (smac == NULL || saddr == NULL)
    {
        LOG(WARN, "inet6: %s: Error parsing IPv6 neighbor output: %s",
                self->ip6_ifname,
                line);
        return true;
    }

    if (strcmp(smac, "FAILED") == 0)
    {
        /* ip -6 neigh flush may trigger this condition */
        return true;
    }

    struct osn_ip6_status *is = &self->ip6_status;

    /*
     * Resize array in OSN_IP6_REALLOC_GROW increments
     */
    if ((is->is6_neigh_len % OSN_IP6_REALLOC_GROW) == 0)
    {
        is->is6_neigh = realloc(
                is->is6_neigh,
                (is->is6_neigh_len + OSN_IP6_REALLOC_GROW) * sizeof(is->is6_neigh[0]));
    }

    if (!osn_ip6_addr_from_str(&is->is6_neigh[is->is6_neigh_len].i6n_ipaddr, saddr))
    {
        LOG(WARN, "inet6: %s: Error parsing IPv6 neighbor report -- invalid IPv6 address \"%s\" on line: %s",
                self->ip6_ifname,
                saddr,
                line);
        return true;
    }

    if (!osn_mac_addr_from_str(&is->is6_neigh[is->is6_neigh_len].i6n_hwaddr, smac))
    {
        LOG(WARN, "inet6: %s: Error parsing IPv6 neighbor report -- invalid MAC \"%s\" on line: %s",
                self->ip6_ifname,
                smac,
                line);
        return true;
    }

    is->is6_neigh_len++;

    return true;
}

void osn_ip6_status_neigh_update(osn_ip6_t *self)
{
    int rc;

    if (self->ip6_status.is6_neigh != NULL)
    {
        free(self->ip6_status.is6_neigh);
    }
    self->ip6_status.is6_neigh_len = 0;
    self->ip6_status.is6_neigh = NULL;

    rc = execsh_fn(osn_ip6_neigh_parse, self, _S(ip -6 neigh show dev "$1"), self->ip6_ifname);
    if (rc != 0)
    {
        LOG(DEBUG, "inet6: %s: \"ip -6 neigh show\" returned error %d. Neighbor report may be incomplete.",
                self->ip6_ifname,
                rc);
    }

    LOG(INFO, "inet6: %s: Found %zu neighbor(s).", self->ip6_ifname, self->ip6_status.is6_neigh_len);
}

/**
 * Netlink event callback. This function is subscribed to the following events:
 *      - OSN_NL_IP6ADDR
 *      - OSN_NL_IP6NEIGH
 *
 * This callback actually triggers the status notification callback of the
 * osn_ip6_t object.
 */
void osn_ip6_nl_fn(osn_nl_t *nl, uint64_t event, const char *ifname)
{
    (void)ifname;

    osn_ip6_t *self = CONTAINER_OF(nl, osn_ip6_t, ip6_nl);

    if (self->ip6_status_fn == NULL) return;

    if (event & OSN_NL_IP6ADDR)
    {
        /* Update IPv6 addresses */
        LOG(DEBUG, "inet6: %s: Updating IPv6 addresses.", self->ip6_ifname);
        osn_ip6_status_ipaddr_update(self);
    }

    if (event & OSN_NL_IP6NEIGH)
    {
        /* Update IPv6 neighbors */
        LOG(DEBUG, "inet6: %s: Update IPv6 neighbors.", self->ip6_ifname);
        osn_ip6_status_neigh_update(self);
    }

    self->ip6_status_fn(self, &self->ip6_status);
}

void osn_ip6_status_notify(osn_ip6_t *self, osn_ip6_status_fn_t *fn, void *data)
{
    self->ip6_status_fn = fn;
    self->ip6_status.is6_data = data;

    if (self->ip6_status_fn == NULL) return;

    /* Update interface status and call the callback right away */
    osn_ip6_status_neigh_update(self);
    self->ip6_status_fn(self, &self->ip6_status);
}
