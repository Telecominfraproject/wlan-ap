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

#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "const.h"
#include "log.h"
#include "ds_tree.h"
#include "os_regex.h"
#include "os_util.h"
#include "os_nif.h"
#include "target.h"
#include "schema.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovs_mac_learn.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

#define OVSMAC_PERIODIC_TIMER   5000                    /**< Periodic timer in ms */


static ovsdb_update_monitor_t   bridge_mon;
static ovsdb_update_monitor_t   port_mon;
static ovsdb_update_monitor_t   iface_mon;

static ovsdb_update_cbk_t       port_mon_fn;
static ovsdb_update_cbk_t       bridge_mon_fn;
static ovsdb_update_cbk_t       iface_mon_fn;

static ev_timer                 ovsmac_timer;                   /* Periodic refresh timer */
static regex_t                  ovs_appctl_re;
static bool                     ovsmac_scan_br(char *brif);

static ds_key_cmp_t ovsmac_cmp_fn;                              /* Key function for ovsmac_node structure s*/

static void  ovsmac_timer_fn(
        struct ev_loop *loop,
        ev_timer *w,
        int revents);

static char *ovsmac_find_ofport_name(char *brif, int ofport);

static void ovsmac_node_reset();
static void ovsmac_node_update(
        char *brname,
        char *ifname,
        int vlan,
        os_macaddr_t macaddr);
static void ovsmac_node_flush();
static bool ovsmac_check_iface_flt(char *iface);
static bool ovsmac_check_bridge_flt(char *bridge);


static ds_tree_t bridge_list = DS_TREE_INIT(
        (ds_key_cmp_t *)strcmp,
        struct bridge_node,
        br_node);

static ds_tree_t port_list = DS_TREE_INIT(
        (ds_key_cmp_t *)strcmp,
        struct port_node,
        pr_node);

static ds_tree_t iface_list = DS_TREE_INIT(
        (ds_key_cmp_t *)strcmp,
        struct iface_node,
        if_node);

static ds_tree_t ovsmac_list = DS_TREE_INIT(
        ovsmac_cmp_fn,
        struct ovsmac_node,
        mac_node);

static ds_tree_t bridge_flt_list = DS_TREE_INIT(
        (ds_key_cmp_t*)strcmp,
        struct iface_flt_node,
        iface_node);

static ds_tree_t iface_flt_list = DS_TREE_INIT(
        (ds_key_cmp_t*)strcmp,
        struct iface_flt_node,
        iface_node);

target_mac_learning_cb_t *g_mac_learning_cb_t;

/*
 * ===========================================================================
 *  OVSMAC functions
 * ===========================================================================
 */
bool ovsmac_init(void)
{
    int ii;
    const char **brlist;
    const char **iflist;
    struct bridge_flt_node *bf = NULL;
    struct iface_flt_node *iff = NULL;

    LOG(INFO, "OVSMAC: Initializing.");

    /* Compile the regular expression for matching a single line from "ovs-appctl" */
    if (regcomp(&ovs_appctl_re, "^ *([0-9]+|LOCAL) +([0-9]+) +(([A-Fa-f0-9]{2}:?){6})", REG_EXTENDED) != 0)
    {
        LOG(ERR, "Error compiling regular expression.");
        return false;
    }

    /*
     * Keep track of OVS Bridge table
     */
    if (!ovsdb_update_monitor(
            &bridge_mon,
            bridge_mon_fn,
            SCHEMA_TABLE(Bridge),
            OMT_ALL))
    {
        LOG(ERR, "OVSMAC: Error subscribing to the 'Bridge' table.");
        return false;
    }

    /*
     * Keep track of OVS Port table
     */
    if (!ovsdb_update_monitor(
            &port_mon,
            port_mon_fn,
            SCHEMA_TABLE(Port),
            OMT_ALL))
    {
        LOG(ERR, "OVSMAC: Error subscribing to the 'Port' table.");
        return false;
    }

    /*
     * Keep track of OVS Interface table
     */
    if (!ovsdb_update_monitor(
            &iface_mon,
            iface_mon_fn,
            SCHEMA_TABLE(Interface),
            OMT_ALL))
    {
        LOG(ERR, "OVSMAC: Error subscribing to the 'Interface' table.");
        return false;
    }

    /*
     * Initialize a bridge filter list and iface filter list
     *
     * TODO - this list should be defined by the cloud, for a moment
     * it is basically a static list in device target layer.
     */
    LOG(INFO, "OVSMAC: Initialize filter lists to:");

    brlist = target_ethclient_brlist_get();
    for (ii=0; brlist[ii]; ii++)
    {
        bf = calloc(1, sizeof(struct bridge_flt_node));
        strcpy(bf->bridge, brlist[ii]);
        ds_tree_insert(&bridge_flt_list, bf, bf->bridge);
        LOG(INFO, "OVSMAC: * %s (bridge)", brlist[ii]);
    }

    iflist = target_ethclient_iflist_get();
    for (ii=0; iflist[ii]; ii++)
    {
        iff = calloc(1, sizeof(struct iface_flt_node));
        strcpy(iff->if_iface, iflist[ii]);
        ds_tree_insert(&iface_flt_list, iff, iff->if_iface);
        LOG(INFO, "OVSMAC: * %s (interface)", iflist[ii]);
    }

    LOG(INFO, "OVSMAC: Starting timer.");

    /* Start the periodic timer */
    ev_timer_init(
            &ovsmac_timer,
            ovsmac_timer_fn,
            (double)OVSMAC_PERIODIC_TIMER / 1000.0,
            (double)OVSMAC_PERIODIC_TIMER / 1000.0);

    ev_timer_start(EV_DEFAULT, &ovsmac_timer);

    return true;
}

void ovsmac_timer_fn(struct ev_loop *loop, ev_timer *w, int revents)
{
    (void)loop;
    (void)w;
    (void)revents;

    struct bridge_node *br;

    LOG(DEBUG, "OVSMAC: Periodic.");

    ovsmac_node_reset();

    ds_tree_foreach(&bridge_list, br)
    {
        if (true == ovsmac_check_bridge_flt(br->br_bridge.name))
            ovsmac_scan_br(br->br_bridge.name);
    }

    ovsmac_node_flush();
}

bool ovsmac_scan_br(char *brif)
{
    char buf[256];
    FILE *ovs_appctl;
    char cmd[256];
    int ret = false;

    if (snprintf(cmd, sizeof(cmd), "ovs-appctl fdb/show %s", brif) >= (int)sizeof(cmd))
    {
        LOG(ERR, "OVSMAC: Command buffer too small.");
        return false;
    }

    ovs_appctl = popen(cmd, "r");
    if (ovs_appctl == NULL)
    {
        printf("Error executing ovs-appctl command: %s", cmd);
        return false;
    }

    /* Skip the first line */
    if (fgets(buf, sizeof(buf), ovs_appctl) == NULL)
    {
        LOG(ERR, "OVSMAC: Premature end of ovs-appctl command. Did it fail?");
        goto err_close;
    }

    while (fgets(buf, sizeof(buf), ovs_appctl) != NULL)
    {
        char *ifname;
        char sofport[16];
        char svlan[16];
        char smac[18];

        regmatch_t rem[16];

        if (regexec(&ovs_appctl_re, buf, ARRAY_LEN(rem), rem, 0) != 0)
        {
            LOG(ERR, "Error parsing ovs-appctl output: %s\n", buf);
            continue;
        }

        os_reg_match_cpy(sofport, sizeof(sofport), buf, rem[1]);
        os_reg_match_cpy(svlan, sizeof(svlan), buf, rem[2]);
        os_reg_match_cpy(smac, sizeof(smac), buf, rem[3]);

        long ofport;
        long vlan;
        os_macaddr_t mac;

        if (!os_atol(svlan, &vlan))
        {
            LOG(ERR, "OVSMAC: ovs-appctl: Invalid VLAN: %s", svlan);
            continue;
        }

        if (!os_nif_macaddr_from_str(&mac, smac))
        {
            LOG(ERR, "OVSMAC: Invalid MAC addres: %s", smac);
            continue;
        }

        if (strcmp(sofport, "LOCAL") == 0)
        {
            ifname = brif;
        }
        else
        {
            if (!os_atol(sofport, &ofport))
            {
                LOG(ERR, "OVSMAC: ovs-appctl: Invalid ofport: %s", sofport);
                continue;
            }

            ifname = ovsmac_find_ofport_name(brif, ofport);
            if (ifname == NULL)
            {
                LOG(ERR, "OVSMAC: Unknown ofport %ld in bridge: %s", ofport, brif);
                continue;
            }
        }

        LOG(DEBUG, "bridge:%s ofport:%s vlan:%s mac:%s\n", brif, ifname, svlan, smac);

        /*
         * Check if given interface is in interface filter list
         * Ethernet clients are connected to eth0 interface
         */
        if (true == ovsmac_check_iface_flt(ifname))
        {
            ovsmac_node_update(brif, ifname, vlan, mac);
        }
    }

    ret = true;
err_close:
    pclose(ovs_appctl);
    return ret;
}

/**
 * Translate the bridge ofport to the interface name
 */
char *ovsmac_find_ofport_name(char *brif, int ofport)
{
    struct bridge_node *br;
    struct port_node *pr;
    struct iface_node *ifn;
    int pidx;
    int iidx;

    /* Find the bridge */
    ds_tree_foreach(&bridge_list, br)
    {
        if (strcmp(br->br_bridge.name, brif) == 0) break;
    }

    if (br == NULL)
    {
        LOG(ERR, "OVSMAC: Bridge %s not found. Cannot resolve ofport %s:%d", brif, brif, ofport);
        return NULL;
    }

    /* Match the port with interface */
    for (pidx = 0; pidx < br->br_bridge.ports_len; pidx++)
    {
        pr = ds_tree_find(&port_list, br->br_bridge.ports[pidx].uuid);
        if (pr == NULL)
        {
            LOG(WARNING, "OVSMAC: Bridge has invalid port UUID: %s", br->br_bridge.ports[pidx].uuid);
            continue;
        }

        /* Lookup interface */
        for (iidx = 0; iidx < pr->pr_port.interfaces_len; iidx++)
        {
            ifn = ds_tree_find(&iface_list, pr->pr_port.interfaces[iidx].uuid);
            if (ifn == NULL)
            {
                LOG(WARNING, "OVSMAC: Port has invalid interface UUID: %s", pr->pr_port.interfaces[iidx].uuid);
                continue;
            }

            /* Check the ofport of the interface */
            if (ifn->if_iface.ofport_exists && ifn->if_iface.ofport == ofport)
            {
                return ifn->if_iface.name;
            }
        }
    }

    return NULL;
}


/**
 * Reset the OVS MAC learning cache -- flag all nodes for deletion. If the node is not updated,
 * a call to ovsmac_node_flush() will permanently delete it from the cache and OVSDB.
 */
void ovsmac_node_reset(void)
{
    struct ovsmac_node *on;

    ds_tree_foreach(&ovsmac_list, on)
    {
        on->mac_flags = 0;
    }
}

/**
 * Update the OVS MAC learning entry
 */
void ovsmac_node_update(char *brname, char *ifname, int vlan, os_macaddr_t macaddr)
{
    struct ovsmac_node *on;

    struct schema_OVS_MAC_Learning key;
    (void)vlan;

    /* Fill in the necessary fields to do a sucessfull compare */
    MEMZERO(key);
    STRSCPY(key.brname, brname);
    STRSCPY(key.ifname, ifname);
    snprintf(key.hwaddr, sizeof(key.hwaddr), PRI(os_macaddr_t), FMT(os_macaddr_t, macaddr));
    str_tolower(key.hwaddr);

    on = ds_tree_find(&ovsmac_list, &key);

    /* This is a new entry */
    if (on == NULL)
    {
        on = calloc(1, sizeof(*on));
        if (on == NULL)
        {
            LOG(ERR, "OVSMAC: Unable to create new OVSMAC entry. Out of memory.");
            return;
        }

        /* Populate the new entry */
        key.vlan = vlan;
        memcpy(&on->mac, &key, sizeof(key));

        /* Insert into OVSDB */
        g_mac_learning_cb_t(&key, true);
        // note, callback modifies hwaddr (str_tolower), so 'key' is used
        // for callback while unmodified 'on' is stored to cache
        ds_tree_insert(&ovsmac_list, on, &on->mac);
    }

    on->mac_flags = OVSMAC_FLAG_ACTIVE;
}

/**
 * Flush all nodes that are not flagged with OVSMAC_FLAG_ACTIVE
 */
void ovsmac_node_flush(void)
{
    struct ovsmac_node *on;

    ds_tree_iter_t iter;

    for (on = ds_tree_ifirst(&iter, &ovsmac_list); on != NULL; on = ds_tree_inext(&iter))
    {
        if (!(on->mac_flags & OVSMAC_FLAG_ACTIVE))
        {
            ds_tree_iremove(&iter);
            /* Remove FROM OVSDB */
            g_mac_learning_cb_t(&(on->mac), false);
            free(on);
        }
    }
}

/**
 * ovsmac_node comparator
 */
int ovsmac_cmp_fn(void *_a, void *_b)
{
    struct schema_OVS_MAC_Learning *a = _a;
    struct schema_OVS_MAC_Learning *b = _b;

    int rc;

    rc = strcmp(a->brname, b->brname);
    if (rc != 0) return rc;

    rc = strcmp(a->ifname, b->ifname);
    if (rc != 0) return rc;

    return strcmp(a->hwaddr, b->hwaddr);
}

/*
 * ===========================================================================
 *  Bridge table functions
 * ===========================================================================
 */
void bridge_mon_fn(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t pjerr;

    struct bridge_node *bn = NULL;

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Bridge was added */
            bn = calloc(1, sizeof(struct bridge_node));
            if (bn == NULL)
            {
                LOG(ERR, "OVSMAC: Error allocating bridge_node structure.");
                return;
            }

            if (!schema_Bridge_from_json(
                    &bn->br_bridge,
                    self->mon_json_new,
                    false,
                    pjerr))
            {
                LOG(ERR, "OVSMAC: Error parsing new bridge entry: %s", pjerr);
                free(bn);
                return;
            }

            /* Insert interface into tree */
            ds_tree_insert(&bridge_list, bn, bn->br_bridge._uuid.uuid);

            LOG(DEBUG, "OVSMAC: Added bridge interface: %s", bn->br_bridge.name);

            break;

        case OVSDB_UPDATE_MODIFY:
            /* Find by UUID */
            bn = ds_tree_find(&bridge_list, (void *)self->mon_uuid);
            if (bn == NULL)
            {
                LOG(ERR, "OVSMAC: Unable to update 'Bridge' with UUID %s. Entry not found.", self->mon_uuid);
                return;
            }

            /* Update entry */
            if (!schema_Bridge_from_json(
                    &bn->br_bridge,
                    self->mon_json_new,
                    true,
                    pjerr))
            {
                LOG(ERR, "OVSMAC: Error parsing updated bridge entry: %s", pjerr);
                return;
            }

            LOG(DEBUG, "OVSMAC: Modified bridge interface: %s", bn->br_bridge.name);

            break;

        case OVSDB_UPDATE_DEL:
            /* Find by UUID and delete */
            bn = ds_tree_find(&bridge_list, (void *)self->mon_uuid);
            if (bn == NULL)
            {
                LOG(ERR, "OVSMAC: Trying to remove non-existend Interface: %s", self->mon_uuid);
                break;
            }

            LOG(DEBUG, "OVSMAC: Deleted bridge interface: %s", bn->br_bridge.name);

            ds_tree_remove(&bridge_list, bn);
            free(bn);

            break;

        case OVSDB_UPDATE_ERROR:
            LOG(ERR, "OVSMAC: Monitor notification error on Bridge table.");
            return;
    }
}

/*
 * ===========================================================================
 *  Port table functions
 * ===========================================================================
 */
void port_mon_fn(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t pjerr;

    struct port_node *pr = NULL;

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            pr = calloc(1, sizeof(*pr));
            if (pr == NULL)
            {
                LOG(ERR, "OVSMAC: Error allocating port_node structure.");
                return;
            }

            if (!schema_Port_from_json(
                    &pr->pr_port,
                    self->mon_json_new,
                    false,
                    pjerr))
            {
                free(pr);
                LOG(ERR, "OVSMAC: Error parsing new port entry: %s", pjerr);
                return;
            }

            ds_tree_insert(&port_list, pr, pr->pr_port._uuid.uuid);

            LOG(DEBUG, "OVSMAC: Added Port added: %s", pr->pr_port.name);

            break;

        case OVSDB_UPDATE_MODIFY:
            /* Find by UUID */
            pr = ds_tree_find(&port_list, (void *)self->mon_uuid);
            if (pr == NULL)
            {
                LOG(ERR, "OVSMAC: Unable to update 'Port' with UUID %s. Entry not found.", self->mon_uuid);
                return;
            }

            /* Update entry */
            if (!schema_Port_from_json(
                    &pr->pr_port,
                    self->mon_json_new,
                    true,
                    pjerr))
            {
                LOG(ERR, "OVSMAC: Error parsing updated port entry: %s", pjerr);
                return;
            }

            LOG(DEBUG, "OVSMAC: Modified port: %s", pr->pr_port.name);

            break;

        case OVSDB_UPDATE_DEL:
            /* Find by UUID and delete */
            pr = ds_tree_find(&port_list, (void *)self->mon_uuid);
            if (pr == NULL)
            {
                LOG(ERR, "OVSMAC: Trying to remove non-existend Port: %s", self->mon_uuid);
                break;
            }

            LOG(DEBUG, "OVSMAC: Deleted port interface: %s", pr->pr_port.name);

            ds_tree_remove(&port_list, pr);

            free(pr);

            break;

        case OVSDB_UPDATE_ERROR:
            LOG(ERR, "OVSMAC: Monitor notification error on Port table.");
            return;
    }
}

/*
 * ===========================================================================
 *  Interface table functions
 * ===========================================================================
 */
void iface_mon_fn(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t pjerr;

    struct iface_node *ifn = NULL;

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            ifn = calloc(1, sizeof(*ifn));
            if (ifn == NULL)
            {
                LOG(ERR, "OVSMAC: Error allocating iface_node structure.");
                return;
            }

            if (!schema_Interface_from_json(
                    &ifn->if_iface,
                    self->mon_json_new,
                    false,
                    pjerr))
            {
                LOG(ERR, "OVSMAC: Error parsing new interface entry: %s", pjerr);
                return;
            }

            ds_tree_insert(&iface_list, ifn, ifn->if_iface._uuid.uuid);

            LOG(DEBUG, "OVSMAC: Added interface: %s", ifn->if_iface.name);

            break;

        case OVSDB_UPDATE_MODIFY:
            /* Find by UUID */
            ifn = ds_tree_find(&iface_list, (void *)self->mon_uuid);
            if (ifn == NULL)
            {
                LOG(ERR, "OVSMAC: Unable to update 'Interface' with UUID %s. Entry not found.", self->mon_uuid);
                return;
            }

            /* Update entry */
            if (!schema_Interface_from_json(
                    &ifn->if_iface,
                    self->mon_json_new,
                    true,
                    pjerr))
            {
                LOG(ERR, "OVSMAC: Error parsing updated bridge entry: %s", pjerr);
                return;
            }

            LOG(DEBUG, "OVSMAC: Modified interface: %s", ifn->if_iface.name);
            break;

        case OVSDB_UPDATE_DEL:
            /* Find by UUID and delete */
            ifn = ds_tree_find(&iface_list, (void *)self->mon_uuid);
            if (ifn == NULL)
            {
                LOG(ERR, "OVSMAC: Trying to remove non-existend Interface: %s", self->mon_uuid);
                break;
            }

            LOG(DEBUG, "OVSMAC: Deleted interface: %s", ifn->if_iface.name);

            ds_tree_remove(&iface_list, ifn);
            free(ifn);

            break;

        case OVSDB_UPDATE_ERROR:
            LOG(ERR, "OVSMAC: Monitor notification error on Interface table.");
            return;
    }
}

/*
 * Check if given interface is in iface filter list
 */
static bool ovsmac_check_iface_flt(char *iface)
{
    struct iface_flt_node *iff;

    iff = ds_tree_find(&iface_flt_list, iface);

    return iff == NULL ? false : true;
}


/*
 * Check if given bridge in in bridge filter list
 */
static bool ovsmac_check_bridge_flt(char *bridge)
{
    struct bridge_flt_node *bf;

    bf = ds_tree_find(&bridge_flt_list, bridge);

    return bf == NULL ? false : true;
}

/*
 * ===========================================================================
 *  MAC learning
 * ===========================================================================
 */
bool ovs_mac_learning_register(target_mac_learning_cb_t *omac_cb)
{
    static bool mac_learning_init = false;

    if (!mac_learning_init)
    {
        ovsmac_init();
        mac_learning_init = false;
    }
    g_mac_learning_cb_t = omac_cb;
    return true;
}
