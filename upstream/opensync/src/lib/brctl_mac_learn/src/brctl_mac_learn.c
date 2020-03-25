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

/**
 * MAC learning of wired clients on the native linux bridge
 */

#include <ev.h>
#include <net/if.h>
#include <stdlib.h>

#include "ds.h"
#include "ds_list.h"
#include "ds_tree.h"
#include "log.h"
#include "schema.h"
#include "schema_consts.h"

#include "brctl_mac_learn.h"

/*****************************************************************************/

#define MAC_LEARNING_INTERVAL   10.0

#define MODULE_ID               LOG_MODULE_ID_TARGET

#if defined(CONFIG_TARGET_LAN_BRIDGE_NAME)
#define BRCTL_LAN_BRIDGE   CONFIG_TARGET_LAN_BRIDGE_NAME
#else
#define BRCTL_LAN_BRIDGE   SCHEMA_CONSTS_BR_NAME_HOME
#endif

/*****************************************************************************/

static int mac_learning_cmp(void *_a, void *_b);

struct mac_learning_flt_t {
    char                    brname[IFNAMSIZ];
    char                    ifname[IFNAMSIZ];
    int                     ifnum;
    struct ds_dlist_node    list;
};

struct mac_learning_t {
    struct schema_OVS_MAC_Learning  oml;
    bool                            valid;
    struct ds_tree_node             list;
};

/******************************************************************************
 *  Global definitions
 *****************************************************************************/

static struct ev_timer             g_mac_learning_timer;
static target_mac_learning_cb_t   *g_mac_learning_cb = NULL;

static ds_tree_t    g_mac_learning = DS_TREE_INIT(mac_learning_cmp,
                                                  struct mac_learning_t,
                                                  list);
static ds_dlist_t   g_mac_learning_flt = DS_DLIST_INIT(struct mac_learning_flt_t,
                                                       list);

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

static struct mac_learning_flt_t *mac_learning_flt_find(int ifnum)
{
    struct mac_learning_flt_t *flt;

    ds_dlist_foreach(&g_mac_learning_flt, flt) {
        if (flt->ifnum == ifnum) {
            return flt;
        }
    }

    return NULL;
}

static bool mac_learning_flt_get(const char *brname)
{
    FILE         *fp;
    char          cmd[512];
    char          buf[512];
    const char  **iflist;
    int           ifidx;
    char          ifname[IFNAMSIZ];

    /* Find port numbers for eth interfaces. Expected output:
     *
     * eth1 (2)
     * eth2 (3)
     * eth3 (4)
     * ...
     */
    snprintf(cmd, sizeof(cmd), "brctl showstp %s | grep eth", brname);

    fp = popen(cmd, "r");
    if (!fp)
    {
        LOGE("BRCTLMAC: Unable to read bridge stp table! :: brname=%s", brname);
        return false;
    }

    while (fgets(buf, sizeof(buf), fp))
    {
        int                         eth;
        int                         ifnum;
        struct mac_learning_flt_t  *flt;

        if (2 != sscanf(buf, "eth%d (%d)", &eth, &ifnum))
        {
            continue;
        }

        // Skip non ethernet clients ports
        snprintf(ifname, sizeof(ifname), "eth%d", eth);
        iflist = target_ethclient_iflist_get();
        for (ifidx=0; iflist[ifidx]; ifidx++)
        {
            if (!strcmp(ifname, iflist[ifidx]))
            {
                break;
            }
        }
        if (iflist[ifidx] == NULL)
        {
            LOGT("BRCTLMAC: Skip %s", ifname);
            continue;
        }

        flt = mac_learning_flt_find(ifnum);
        if (flt != NULL) {
            LOGT("BRCTLMAC: Already exists %s @ %s :: ifnum=%d", flt->ifname, brname, flt->ifnum);
            continue;
        }
        else
        {
            flt = calloc(1, sizeof(*flt));
            if (flt == NULL)
            {
                LOGE("BRCTLMAC: Unable to allocate mac learning entry! :: brname=%s", brname);
                return false;
            }
        }

        snprintf(flt->ifname, sizeof(flt->ifname), "eth%d", eth);
        snprintf(flt->brname, sizeof(flt->brname), "%s", brname);
        flt->ifnum = ifnum;

        ds_dlist_insert_tail(&g_mac_learning_flt, flt);

        LOGI("BRCTLMAC: Using %s @ %s :: ifnum=%d", flt->ifname, brname, flt->ifnum);
    }

    if (fp)
        pclose(fp);

    return true;
}

static bool mac_learning_parse(const char *brname)
{
    FILE *fp;
    char cmd[512];
    char buf[512];

    /* Find bridge port numbers and mac addresses. Expected output:
     *
     * 6 00:c0:02:12:35:8a yes 0.00
     * 1 40:b9:3c:1d:d9:09 no 1.80
     * 1 68:05:ca:28:c1:45 no 5.40
     * ...
     */
    snprintf(cmd,
             sizeof(cmd),
             "brctl showmacs %s | awk '{ print $1\" \"$2\" \"$3\" \"$4 }'",
             brname);

    fp = popen(cmd, "r");
    if (!fp)
    {
        LOGE("BRCTLMAC: Unable to read bridge mac table! :: brname=%s", brname);
        return false;
    }

    while (fgets(buf, sizeof(buf), fp))
    {
        int   ifnum;
        float age;
        char  mac[64];
        char  local[64];

        if ((4 != sscanf(buf, "%d %64s %64s %f", &ifnum, mac, local, &age)) ||
            (0 == strcmp(local, "yes")))
        {
            continue;
        }

        // Look only at eth interfaces
        struct mac_learning_flt_t *flt = mac_learning_flt_find(ifnum);
        if (flt == NULL)
        {
            continue;
        }

        struct schema_OVS_MAC_Learning oml;
        memset(&oml, 0, sizeof(oml));
        strscpy(oml.hwaddr, mac, sizeof(oml.hwaddr));
        strscpy(oml.brname, BRCTL_LAN_BRIDGE, sizeof(oml.brname));
        strscpy(oml.ifname, flt->ifname, sizeof(oml.ifname));

        // New entry
        bool update = false;
        struct mac_learning_t *ml;
        ml = ds_tree_find(&g_mac_learning, &oml);
        if (ml == NULL)
        {
            ml = calloc(1, sizeof(*ml));
            if (ml == NULL)
            {
                LOGE("BRCTLMAC: Error allocating struct mac_learning!");
                continue;
            }

            memcpy(&ml->oml, &oml, sizeof(ml->oml));
            ds_tree_insert(&g_mac_learning, ml, &ml->oml);

            update = true;
        }

        ml->valid = true;

        LOGT("BRCTLMAC: parsed mac table entry :: brname=%s ifname=%s mac=%s update=%s",
             oml.brname,
             oml.ifname,
             oml.hwaddr,
             update ? "true" : "false");

        // Pass new entry to NM
        if (update)
        {
            g_mac_learning_cb(&ml->oml, true);
        }
    }

    if (fp)
        pclose(fp);

    return true;
}

static void mac_learning_invalidate(void)
{
    struct mac_learning_t *ml;

    ds_tree_foreach(&g_mac_learning, ml)
    {
        ml->valid = false;
    }
}

static void mac_learning_flush(void)
{
    struct mac_learning_t  *ml;
    ds_tree_iter_t          iter;

    for (ml = ds_tree_ifirst(&iter, &g_mac_learning);
         ml != NULL;
         ml = ds_tree_inext(&iter))
    {
        if (ml->valid)
        {
            continue;
        }

        // Indicate deleted entry to NM
        g_mac_learning_cb(&ml->oml, false);

        // Remove our entry
        ds_tree_iremove(&iter);
        memset(ml, 0, sizeof(*ml));
        free(ml);
    }
}

static int mac_learning_cmp(void *_a, void *_b)
{
    struct schema_OVS_MAC_Learning *a = _a;
    struct schema_OVS_MAC_Learning *b = _b;
    return strcmp(a->hwaddr, b->hwaddr);
}

static void mac_learing_timer_cb(struct ev_loop *loop, ev_timer *watcher, int revents)
{
    // Some vendors add/remove ethernet interface in runtime
    if (!mac_learning_flt_get(BRCTL_LAN_BRIDGE))
    {
        LOGE("BRCTLMAC: Unable to create MAC learning filter!");
        return;
    }

    LOGD("BRCTLMAC: refreshing mac learning table");
    mac_learning_invalidate();
    mac_learning_parse(BRCTL_LAN_BRIDGE);
    mac_learning_flush();
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/

bool brctl_mac_learning_register(target_mac_learning_cb_t *omac_cb)
{
    if (g_mac_learning_cb)
        return false;

    // Init NM callback
    g_mac_learning_cb = omac_cb;

    // Init timer
    ev_timer_init(&g_mac_learning_timer,
                  mac_learing_timer_cb,
                  MAC_LEARNING_INTERVAL,
                  MAC_LEARNING_INTERVAL);
    ev_timer_start(EV_DEFAULT, &g_mac_learning_timer);

    LOGN("BRCTLMAC: Successfully registered MAC learning. :: brname=%s",
            BRCTL_LAN_BRIDGE);

    return true;
}
