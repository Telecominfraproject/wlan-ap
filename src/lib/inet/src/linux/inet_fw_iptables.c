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

#include <sys/wait.h>
#include <errno.h>

#include "log.h"
#include "const.h"
#include "util.h"
#include "execsh.h"
#include "ds_tree.h"

#include "inet_fw.h"

#define FW_RULE_IPV4                (1 << 0)
#define FW_RULE_IPV6                (1 << 1)
#define FW_RULE_ALL                 (FW_RULE_IPV4 | FW_RULE_IPV6)

#if !defined(CONFIG_USE_KCONFIG)

#define CONFIG_INET_FW_IPTABLES_PATH    "/usr/sbin/iptables"
#define CONFIG_INET_FW_IP6TABLES_PATH   "/usr/sbin/ip6tables"

#endif /* CONFIG_USE_KCONFIG */

struct __inet_fw
{
    char                        fw_ifname[C_IFNAME_LEN];
    bool                        fw_enabled;
    bool                        fw_nat_enabled;
    ds_tree_t                   fw_portfw_list;
};

/* Wrapper around fw_portfw_entry so we can have it in a tree */
struct fw_portfw_entry
{
    struct inet_portforward     pf_data;            /* Port forwarding structure */
    bool                        pf_pending;         /* Pending deletion */
    ds_tree_node_t              pf_node;            /* Tree node */
};

#define FW_PORTFW_ENTRY_INIT (struct fw_portfw_entry)   \
{                                                       \
    .pf_data = INET_PORTFORWARD_INIT,                   \
}

/**
 * fw_rule_*() macros -- pass a dummy argv[0] value ("false") -- it will
 * be overwritten with the real path to iptables in the __fw_rule*() function.
 */
static bool fw_rule_add_a(inet_fw_t *self, int type, char *argv[]);
#define fw_rule_add(self, type, table, chain, ...) \
        fw_rule_add_a(self, type, C_VPACK("false", table, chain, __VA_ARGS__))

static bool fw_rule_del_a(inet_fw_t *self, int type, char *argv[]);
#define fw_rule_del(self, type, table, chain, ...) \
        fw_rule_del_a(self, type, C_VPACK("false", table, chain, __VA_ARGS__))

static bool fw_nat_start(inet_fw_t *self);
static bool fw_nat_stop(inet_fw_t *self);

static bool fw_portforward_start(inet_fw_t *self);
static bool fw_portforward_stop(inet_fw_t *self);

static ds_key_cmp_t fw_portforward_cmp;

/*
 * ===========================================================================
 *  Public interface
 * ===========================================================================
 */
bool inet_fw_init(inet_fw_t *self, const char *ifname)
{

    memset(self, 0, sizeof(*self));

    ds_tree_init(&self->fw_portfw_list, fw_portforward_cmp, struct fw_portfw_entry, pf_node);

    if (strscpy(self->fw_ifname, ifname, sizeof(self->fw_ifname)) < 0)
    {
        LOG(ERR, "fw: Interface name %s is too long.", ifname);
        return false;
    }

    /* Start by flushing NAT/LAN rules */
    (void)fw_nat_stop(self);

    return true;
}

bool inet_fw_fini(inet_fw_t *self)
{
    bool retval = inet_fw_stop(self);

    return retval;
}

inet_fw_t *inet_fw_new(const char *ifname)
{
    inet_fw_t *self = malloc(sizeof(inet_fw_t));

    if (!inet_fw_init(self, ifname))
    {
        free(self);
        return NULL;
    }

    return self;
}

bool inet_fw_del(inet_fw_t *self)
{
    bool retval = inet_fw_fini(self);
    if (!retval)
    {
        LOG(WARN, "nat: Error stopping FW on interface: %s", self->fw_ifname);
    }

    free(self);

    return retval;
}

/**
 * Start the FW service on interface
 */
bool inet_fw_start(inet_fw_t *self)
{
    if (self->fw_enabled) return true;

    if (!fw_nat_start(self))
    {
        LOG(WARN, "fw: %s: Failed to apply NAT/LAN rules.", self->fw_ifname);
    }

    if (!fw_portforward_start(self))
    {
        LOG(WARN, "fw: %s: Failed to apply port-forwarding rules.", self->fw_ifname);
    }

    self->fw_enabled = true;

    return true;
}

/**
 * Stop the FW service on interface
 */
bool inet_fw_stop(inet_fw_t *self)
{
    bool retval = true;

    if (!self->fw_enabled) return true;

    retval &= fw_nat_stop(self);
    retval &= fw_portforward_stop(self);

    self->fw_enabled = false;

    return retval;
}

bool inet_fw_nat_set(inet_fw_t *self, bool enable)
{
    self->fw_nat_enabled = enable;

    return true;
}

bool inet_fw_state_get(inet_fw_t *self, bool *nat_enabled)
{
    *nat_enabled = self->fw_enabled && self->fw_nat_enabled;

    return true;
}

/* Test if the port forwarding entry @pf already exists in the port forwarding list */
bool inet_fw_portforward_get(inet_fw_t *self, const struct inet_portforward *pf)
{
    struct fw_portfw_entry *pe;

    pe = ds_tree_find(&self->fw_portfw_list, (void *)pf);
    /* Entry not found */
    if (pe == NULL) return false;
    /* Entry was found, but is scheduled for deletion ... return false */
    if (pe->pf_pending) return false;

    return true;
}

/*
 * Add the @p pf entry to the port forwarding list. A firewall restart is required
 * for the change to take effect.
 */
bool inet_fw_portforward_set(inet_fw_t *self, const struct inet_portforward *pf)
{
    struct fw_portfw_entry *pe;

    LOG(INFO, "fw: %s: PORT FORWARD SET: %d -> "PRI_osn_ip_addr":%d",
            self->fw_ifname,
            pf->pf_src_port,
            FMT_osn_ip_addr(pf->pf_dst_ipaddr),
            pf->pf_dst_port);

    pe = ds_tree_find(&self->fw_portfw_list, (void *)pf);
    if (pe != NULL)
    {
        /* Unflag deletion */
        pe->pf_pending = false;
        return true;
    }

    pe = calloc(1, sizeof(struct fw_portfw_entry));
    if (pe == NULL)
    {
        LOG(ERR, "fw: %s: Unable to allocate port forwarding entry.", self->fw_ifname);
        return false;
    }

    memcpy(&pe->pf_data, pf, sizeof(pe->pf_data));

    ds_tree_insert(&self->fw_portfw_list, pe, &pe->pf_data);

    return true;
}


/* Delete port forwarding entry -- a firewall restart is requried for the change to take effect */
bool inet_fw_portforward_del(inet_fw_t *self, const struct inet_portforward *pf)
{
    struct fw_portfw_entry *pe = NULL;

    LOG(INFO, "fw: %s: PORT FORWARD DEL: %d -> "PRI_osn_ip_addr":%d",
            self->fw_ifname,
            pf->pf_src_port,
            FMT_osn_ip_addr(pf->pf_dst_ipaddr),
            pf->pf_dst_port);

    pe = ds_tree_find(&self->fw_portfw_list, (void *)pf);
    if (pe == NULL)
    {
        LOG(ERR, "fw: %s: Error removing port forwarding entry: %d -> "PRI_osn_ip_addr":%d",
                self->fw_ifname,
                pf->pf_src_port,
                FMT_osn_ip_addr(pf->pf_dst_ipaddr),
                pf->pf_dst_port);
        return false;
    }

    /* Flag for deletion */
    pe->pf_pending = true;

    return true;
}

/*
 * ===========================================================================
 *  NAT - Private functions
 * ===========================================================================
 */

/**
 * Either enable NAT or LAN rules on the interface
 */
bool fw_nat_start(inet_fw_t *self)
{
    bool retval = true;

    if (self->fw_nat_enabled)
    {
        LOG(INFO, "fw: %s: Installing NAT rules.", self->fw_ifname);

        retval &= fw_rule_add(self,
                FW_RULE_IPV4, "nat", "NM_NAT",
                "-o", self->fw_ifname, "-j", "MASQUERADE");

        /* Plant miniupnpd rules for port forwarding via upnp */
        retval &= fw_rule_add(self,
                FW_RULE_IPV4, "nat", "NM_PORT_FORWARD",
                "-i", self->fw_ifname, "-j", "MINIUPNPD");
    }
    else
    {
        LOG(INFO, "fw: %s: Installing LAN rules.", self->fw_ifname);

        retval &= fw_rule_add(self,
                FW_RULE_ALL, "filter", "NM_INPUT",
                "-i", self->fw_ifname, "-j", "ACCEPT");

        retval &= fw_rule_add(self,
                FW_RULE_IPV6, "filter", "NM_FORWARD",
                "-i", self->fw_ifname, "-j", "ACCEPT");
    }

    return true;
}

/**
 * Flush all NAT/LAN rules
 */
bool fw_nat_stop(inet_fw_t *self)
{
    bool retval = true;

    LOG(INFO, "fw: %s: Flushing NAT/LAN related rules.", self->fw_ifname);

    /* Flush out NAT rules */
    retval &= fw_rule_del(self, FW_RULE_IPV4,
            "nat", "NM_NAT", "-o", self->fw_ifname, "-j", "MASQUERADE");

    retval &= fw_rule_del(self, FW_RULE_IPV4,
            "nat", "NM_PORT_FORWARD", "-i", self->fw_ifname, "-j", "MINIUPNPD");

    /* Flush out LAN rules */
    retval &= fw_rule_del(self, FW_RULE_ALL,
            "filter", "NM_INPUT", "-i", self->fw_ifname, "-j", "ACCEPT");

    retval &= fw_rule_del(self, FW_RULE_IPV6,
            "filter", "NM_FORWARD", "-i", self->fw_ifname, "-j", "ACCEPT");

    return retval;
}

/*
 * ===========================================================================
 *  Port forwarding - Private functions
 * ===========================================================================
 */
/*
 * Port-forwarding stuff
 */
bool fw_portforward_rule(inet_fw_t *self, const struct inet_portforward *pf, bool remove)
{
    char *proto = 0;
    char src_port[8] = { 0 };
    char to_dest[32] = { 0 };


    if (pf->pf_proto == INET_PROTO_UDP)
        proto = "udp";
    else if (pf->pf_proto == INET_PROTO_TCP)
        proto = "tcp";
    else
        return false;


    if (snprintf(src_port, sizeof(src_port), "%u", pf->pf_src_port)
                  >= (int)sizeof(src_port))
        return false;

    if (snprintf(to_dest, sizeof(to_dest), PRI_osn_ip_addr":%u",
                 FMT_osn_ip_addr(pf->pf_dst_ipaddr), pf->pf_dst_port)
                      >= (int)sizeof(to_dest))
    {
            return false;
    }

    if (remove)
    {
        return fw_rule_del(self, FW_RULE_IPV4, "nat", "NM_PORT_FORWARD",
                           "-i", self->fw_ifname,
                           "-p", proto,
                           "--dport", src_port,
                           "-j", "DNAT",
                           "--to-destination", to_dest);

    }
    else
    {
        return fw_rule_add(self, FW_RULE_IPV4, "nat", "NM_PORT_FORWARD",
                           "-i", self->fw_ifname,
                           "-p", proto,
                           "--dport", src_port,
                           "-j", "DNAT",
                           "--to-destination", to_dest);
    }

}

/* Apply the current portforwarding configuration */
bool fw_portforward_start(inet_fw_t *self)
{
    bool retval = true;

    struct fw_portfw_entry *pe;

    ds_tree_foreach(&self->fw_portfw_list, pe)
    {
        /* Skip entries flagged for deletion */
        if (pe->pf_pending) continue;

        if (!fw_portforward_rule(self, &pe->pf_data, false))
        {
            LOG(ERR, "fw: %s: Error adding port forwarding rule: %d -> "PRI_osn_ip_addr":%d",
                    self->fw_ifname,
                    pe->pf_data.pf_src_port,
                    FMT_osn_ip_addr(pe->pf_data.pf_dst_ipaddr),
                    pe->pf_data.pf_dst_port);
            retval = false;
        }
    }

    return retval;
}

bool fw_portforward_stop(inet_fw_t *self)
{
    struct fw_portfw_entry *pe;
    ds_tree_iter_t iter;

    bool retval = true;

    ds_tree_foreach_iter(&self->fw_portfw_list, pe, &iter)
    {
        if (!fw_portforward_rule(self, &pe->pf_data, true))
        {
            LOG(ERR, "fw: %s: Error deleting port forwarding rule: %d -> "PRI_osn_ip_addr":%d",
                    self->fw_ifname,
                    pe->pf_data.pf_src_port,
                    FMT_osn_ip_addr(pe->pf_data.pf_dst_ipaddr),
                    pe->pf_data.pf_dst_port);
            retval = false;
        }

        /* Flush port forwarding entry */
        if (pe->pf_pending)
        {
            ds_tree_iremove(&iter);
            memset(pe, 0, sizeof(*pe));
            free(pe);
        }
    }

    return retval;
}

/*
 * ===========================================================================
 *  Static functions and utilities
 * ===========================================================================
 */

char fw_rule_add_cmd[] =
_S(
    IPTABLES="$1";
    tbl="$2";
    chain="$3";
    shift 3;
    if ! "$IPTABLES" -t "$tbl" -C "$chain" "$@" 2> /dev/null;
    then
        "$IPTABLES" -t "$tbl" -A "$chain" "$@";
    fi;
);

bool fw_rule_add_a(inet_fw_t *self,int type, char *argv[])
{
    int status;

    bool retval = true;

    if (type & FW_RULE_IPV4)
    {
        argv[0] = CONFIG_INET_FW_IPTABLES_PATH;
        status = execsh_log_a(LOG_SEVERITY_INFO, fw_rule_add_cmd, argv);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(WARN, "fw: %s: IPv4 rule insertion failed: %s",
                    self->fw_ifname,
                    fw_rule_add_cmd);
            retval = false;
        }
    }

    if (type & FW_RULE_IPV6)
    {
        argv[0] = CONFIG_INET_FW_IP6TABLES_PATH;
        status = execsh_log_a(LOG_SEVERITY_INFO, fw_rule_add_cmd,  argv);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(WARN, "fw: %s: IPv6 rule deletion failed: %s",
                    self->fw_ifname,
                    fw_rule_add_cmd);
            retval = false;
        }
    }

    return retval;
}

char fw_rule_del_cmd[] =
_S(
    IPTABLES="$1";
    tbl="$2";
    chain="$3";
    shift 3;
    if "$IPTABLES" -t "$tbl" -C "$chain" "$@" 2> /dev/null;
    then
        "$IPTABLES" -t "$tbl" -D "$chain" "$@";
    fi;
);

bool fw_rule_del_a(inet_fw_t *self, int type, char *argv[])
{
    int status;

    bool retval = true;

    if (type & FW_RULE_IPV4)
    {
        argv[0] = CONFIG_INET_FW_IPTABLES_PATH;
        status = execsh_log_a(LOG_SEVERITY_INFO, fw_rule_del_cmd, argv);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(WARN, "fw: %s: IPv4 rule deletion failed: %s",
                    self->fw_ifname,
                    fw_rule_del_cmd);
            retval = false;
        }
    }

    if (type & FW_RULE_IPV6)
    {
        argv[0] = CONFIG_INET_FW_IP6TABLES_PATH;
        status = execsh_log_a(LOG_SEVERITY_INFO, fw_rule_del_cmd,  argv);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(WARN, "fw: %s: IPv6 rule deletion failed: %s",
                    self->fw_ifname,
                    fw_rule_del_cmd);
            retval = false;
        }
    }

    return retval;
}

/* Compare two inet_portforward structures -- used for tree comparator */
int fw_portforward_cmp(void *_a, void *_b)
{
    int rc;

    struct inet_portforward *a = _a;
    struct inet_portforward *b = _b;

    rc = osn_ip_addr_cmp(&a->pf_dst_ipaddr, &b->pf_dst_ipaddr);
    if (rc != 0) return rc;

    if (a->pf_proto > b->pf_proto) return 1;
    if (a->pf_proto < b->pf_proto) return -1;

    if (a->pf_dst_port > b->pf_dst_port) return 1;
    if (a->pf_dst_port < b->pf_dst_port) return -1;

    if (a->pf_src_port > b->pf_src_port) return 1;
    if (a->pf_src_port < b->pf_src_port) return -1;

    return 0;
}
