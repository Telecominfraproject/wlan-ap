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

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "os.h"
#include "util.h"
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
#include "cm2.h"

#include <arpa/inet.h>


#define MODULE_ID LOG_MODULE_ID_OVSDB

#define OVSDB_FILTER_LEN                25

/* BLE definitions */
#define CM2_BLE_INTERVAL_VALUE_DEFAULT  0
#define CM2_BLE_TXPOWER_VALUE_DEFAULT   0
#define CM2_BLE_MODE_OFF                "off"
#define CM2_BLE_MODE_ON                 "on"
#define CM2_BLE_MSG_ONBOARDING          "on_boarding"
#define CM2_BLE_MSG_DIAGNOSTIC          "diagnostic"
#define CM2_BLE_MSG_LOCATE              "locate"

#define CM2_BASE64_ARGV_MAX             64

#ifndef CONFIG_OPENSYNC_CM2_MTU_ON_GRE
#define CONFIG_OPENSYNC_CM2_MTU_ON_GRE           1500
#endif

static void
cm2_util_set_not_used_link(void);

ovsdb_table_t table_Open_vSwitch;
ovsdb_table_t table_Manager;
ovsdb_table_t table_SSL;
ovsdb_table_t table_AWLAN_Node;
ovsdb_table_t table_Wifi_Master_State;
ovsdb_table_t table_Connection_Manager_Uplink;
ovsdb_table_t table_AW_Bluetooth_Config;
ovsdb_table_t table_Wifi_Inet_Config;
ovsdb_table_t table_Wifi_Inet_State;
ovsdb_table_t table_Wifi_VIF_Config;
ovsdb_table_t table_Wifi_VIF_State;
ovsdb_table_t table_Port;
ovsdb_table_t table_Bridge;

void callback_AWLAN_Node(ovsdb_update_monitor_t *mon,
        struct schema_AWLAN_Node *old_rec,
        struct schema_AWLAN_Node *awlan)
{
    bool valid;

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        g_state.have_awlan = false;
    }
    else
    {
        g_state.have_awlan = true;
    }

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(AWLAN_Node, manager_addr)))
    {
        // manager_addr changed
        valid = cm2_set_addr(CM2_DEST_MANAGER, awlan->manager_addr);
        g_state.addr_manager.updated = valid;
    }

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(AWLAN_Node, redirector_addr)))
    {
        // redirector_addr changed
        valid = cm2_set_addr(CM2_DEST_REDIR, awlan->redirector_addr);
        g_state.addr_redirector.updated = valid;
    }

    if (    ovsdb_update_changed(mon, SCHEMA_COLUMN(AWLAN_Node, device_mode))
         || ovsdb_update_changed(mon, SCHEMA_COLUMN(AWLAN_Node, factory_reset))
       )
    {
        target_device_config_set(awlan);
    }

    if (    ovsdb_update_changed(mon, SCHEMA_COLUMN(AWLAN_Node, min_backoff))
         || ovsdb_update_changed(mon, SCHEMA_COLUMN(AWLAN_Node, max_backoff))
       )
    {
        g_state.min_backoff = awlan->min_backoff;
        g_state.max_backoff = awlan->max_backoff;
    }

    cm2_update_state(CM2_REASON_AWLAN);
}

void callback_Manager(ovsdb_update_monitor_t *mon,
        struct schema_Manager *old_rec,
        struct schema_Manager *manager)
{
    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        g_state.have_manager = false;
        g_state.connected = false;
    }
    else
    {
        g_state.have_manager = true;
        g_state.connected = manager->is_connected;
        if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Manager, is_connected)))
        {
            // is_connected changed
            LOG(DEBUG, "Manager.is_connected = %s", str_bool(manager->is_connected));
        }
    }

    cm2_update_state(CM2_REASON_MANAGER);
}

static bool
cm2_util_get_link_is_used(struct schema_Connection_Manager_Uplink *uplink)
{
    if (!strcmp(uplink->if_name, g_state.link.if_name) &&
        g_state.link.is_used)
        return true;

    return false;
}

static bool
cm2_util_vif_is_sta(const char *ifname)
{
    struct schema_Wifi_VIF_Config vconf;
    struct schema_Wifi_VIF_State vstate;

    MEMZERO(vconf);
    MEMZERO(vstate);

    /* This function can be called after underlying wifi
     * interface configuration and state were removed.
     *
     * In such case we fall back to checking raw interface
     * names hoping to get it right.
     *
     * When we assign these names explicitly
     * there's no need for any extra checks.
     *
     * On some platforms it's not possible to rename
     * interfaces so we're stuck with wl%d and wl%d.%d.
     * Moreover extenders are expected (due to apparent
     * wl driver limitation) to use wl%d primary interfaces
     * for station role. However non-extender devices will
     * use wl%d as an ap interface. This isn't a problem
     * because we can check cm2_is_extender() and filter
     * that case out.
     *
     * TODO: define a target api as this is platform dependant
     */

    if (!cm2_is_extender())
        return false;

    if (ovsdb_table_select_one(&table_Wifi_VIF_Config,
                SCHEMA_COLUMN(Wifi_VIF_Config, if_name), ifname, &vconf))
        return !strcmp(vconf.mode, "sta");

    if (ovsdb_table_select_one(&table_Wifi_VIF_State,
                SCHEMA_COLUMN(Wifi_VIF_State, if_name), ifname, &vstate))
        return !strcmp(vstate.mode, "sta");

    LOGI("%s: %s: unable to find in ovsdb, guessing", __func__, ifname);

    if (strstr(ifname, "wl") == ifname && !strstr(ifname, "."))
        return true;

    if (strstr(ifname, "bhaul-sta") == ifname)
        return true;

    LOGI("%s: %s: either not a sta, or unable to infer", __func__, ifname);
    return false;
}

static int cm2_util_set_defined_priority(char *if_type) {
    int priority;

    if (!strcmp(if_type, VLAN_TYPE_NAME))
        priority = 3;
    else if (!strcmp(if_type, ETH_TYPE_NAME))
        priority = 2;
    else
        priority = 1;

    return priority;
}

/*
 * Helper functions for WM, NM to translate Wifi_Master_State to Connection_Manager_Uplink table
 */

bool
cm2_ovsdb_set_Wifi_Inet_Config_interface_enabled(bool state, char *ifname)
{
    struct schema_Wifi_Inet_Config icfg;
    int                            ret;

    memset(&icfg, 0, sizeof(icfg));

    if (strlen(ifname) == 0)
        return 0;

    LOGI("%s change interface state: %d", ifname, state);

    icfg.enabled = state;
    char *filter[] = { "+",
                       SCHEMA_COLUMN(Wifi_Inet_Config, enabled),
                       NULL };

    ret = ovsdb_table_update_where_f(&table_Wifi_Inet_Config,
                 ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Inet_Config, if_name), ifname),
                 &icfg, filter);

    return ret == 1;
}


static bool
cm2_util_set_dhcp(struct schema_Wifi_Master_State *master, bool refresh)
{
    struct schema_Wifi_Inet_Config iconf_update;
    struct schema_Wifi_Inet_Config iconf;
    bool                           dhcp_active;
    bool                           empty_addr;
    int                            ret;

    if (!strcmp(master->port_state, "inactive"))
        return false;

    MEMZERO(iconf);
    MEMZERO(iconf_update);

    ret = ovsdb_table_select_one(&table_Wifi_Inet_Config,
                SCHEMA_COLUMN(Wifi_Inet_Config, if_name), master->if_name, &iconf);
    if (!ret)
        LOGI("%s: %s: Failed to get interface config", __func__, master->if_name);

    LOGI("%s Set dhcp: inet_addr = %s assign_scheme = %s refresh = %d", master->if_name, master->inet_addr, iconf.ip_assign_scheme, refresh);

    dhcp_active = !strcmp(iconf.ip_assign_scheme, "dhcp");
    empty_addr = !((strlen(master->inet_addr) > 0) && strcmp(master->inet_addr, "0.0.0.0") != 0);

    if (refresh && dhcp_active) {
        if (!empty_addr) {
            STRSCPY(iconf_update.ip_assign_scheme, "none");
        }
        else {
            if (iconf.enabled) {
                ret = cm2_ovsdb_set_Wifi_Inet_Config_interface_enabled(false, master->if_name);
                if (!ret)
                    LOGW("%s: %s: Failed to clear interface enabled", __func__, master->if_name);

                ret = cm2_ovsdb_set_Wifi_Inet_Config_interface_enabled(true, master->if_name);
                if (!ret)
                    LOGW("%s: %s: Failed to set interface enabled", __func__, master->if_name);
            }
            return false;
        }
    }

    if (!refresh && !empty_addr)
        return true;

    if (empty_addr) {
        if (!dhcp_active)
            STRSCPY(iconf_update.ip_assign_scheme, "dhcp");
        else
            return false;
    }

    iconf_update.ip_assign_scheme_exists = true;

    char *filter[] = { "+",
                       SCHEMA_COLUMN(Wifi_Inet_Config, ip_assign_scheme),
                       NULL };

    ret = ovsdb_table_update_where_f(&table_Wifi_Inet_Config,
                 ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Inet_Config, if_name), master->if_name),
                 &iconf_update, filter);

    return false;
}

bool
cm2_ovsdb_refresh_dhcp(char *if_name)
{
    struct schema_Wifi_Master_State  mstate;
    int                              ret;

    LOGI("%s: Trigger refresh dhcp", if_name);

    MEMZERO(mstate);

    ret = ovsdb_table_select_one(&table_Wifi_Master_State,
                SCHEMA_COLUMN(Wifi_Master_State, if_name), if_name, &mstate);
    if (!ret) {
        LOGW("%s: %s: Failed to get master row", __func__, mstate.if_name);
    } else
        ret = cm2_util_set_dhcp(&mstate, true);

    return ret;
}

static char*
cm2_util_get_gateway_ip(char *ip_addr, char *netmask)
{
     struct in_addr remote_addr;
     struct in_addr netmask_addr;

     inet_aton(ip_addr, &remote_addr);
     inet_aton(netmask, &netmask_addr);

     remote_addr.s_addr &= netmask_addr.s_addr;
     remote_addr.s_addr |= htonl(0x00000001U);

     return inet_ntoa(remote_addr);
}

/* Function triggers creating GRE interfaces
 * In the second phase of implementation need to be moved to WM2 */
static int
cm2_ovsdb_insert_Wifi_Inet_Config(struct schema_Wifi_Master_State *master)
{
    struct schema_Wifi_Inet_Config icfg;
    int                            ret;

    memset(&icfg, 0, sizeof(icfg));

    icfg.enabled = false;
    STRSCPY(icfg.if_type, GRE_TYPE_NAME);

    icfg.gre_local_inet_addr_exists = true;
    STRSCPY(icfg.gre_local_inet_addr, master->inet_addr);

    icfg.enabled_exists = true;
    icfg.enabled = true;

    icfg.network_exists = true;
    icfg.network = true;

    icfg.mtu_exists = true;
    icfg.mtu = CONFIG_OPENSYNC_CM2_MTU_ON_GRE;

    icfg.gre_remote_inet_addr_exists = true;
    STRSCPY(icfg.gre_remote_inet_addr, cm2_util_get_gateway_ip(master->inet_addr, master->netmask));

    icfg.gre_ifname_exists = true;
    STRSCPY(icfg.gre_ifname, master->if_name);
    STRSCPY(icfg.if_name, "g-");
    STRLCAT(icfg.if_name, master->if_name);

    icfg.ip_assign_scheme_exists = true;
    STRSCPY(icfg.ip_assign_scheme, "none");

    LOGN("%s: Create new gre iface: %s", master->if_name, icfg.if_name);

    ret = ovsdb_table_upsert_simple(&table_Wifi_Inet_Config,
                                    SCHEMA_COLUMN(Wifi_Inet_Config, if_name),
                                    icfg.if_name,
                                    &icfg,
                                    NULL);
    if (!ret)
        LOGD("%s update creating GRE failed %s", __func__, master->if_name);

    return ret;
}

/* Function removing GRE interface */
static void
cm2_ovsdb_remove_Wifi_Inet_Config(char *if_name, bool gre) {
    char gre_ifname[IFNAME_SIZE];
    int  ret;

    if (!gre) {
        STRSCPY(gre_ifname, "g-");
        STRLCAT(gre_ifname, if_name);
    } else {
        STRSCPY(gre_ifname, if_name);
    }

    LOGN("%s: Remove gre if_name = %s", if_name, gre_ifname);
    ret = ovsdb_table_delete_simple(&table_Wifi_Inet_Config,
                                   SCHEMA_COLUMN(Wifi_Inet_Config, if_name),
                                   gre_ifname);
    if (!ret)
        LOGI("%s Remove row failed %s", __func__, gre_ifname);
}

/* Function required as a workaround for CAES-599 */
void cm2_ovsdb_remove_unused_gre_interfaces(void) {
    struct schema_Connection_Manager_Uplink *uplink;
    void   *uplink_p;
    int    count;
    int    i;

    uplink_p = ovsdb_table_select(&table_Connection_Manager_Uplink,
                                  SCHEMA_COLUMN(Connection_Manager_Uplink, if_type),
                                  "gre",
                                  &count);

    LOGD("%s Available gre links count = %d", __func__, count);

    if (uplink_p) {
        for (i = 0; i < count; i++) {
            uplink = (struct schema_Connection_Manager_Uplink *) (uplink_p + table_Connection_Manager_Uplink.schema_size * i);
            LOGD("%s link = %s type = %s is_used = %d",
                 __func__, uplink->if_name, uplink->if_name, uplink->is_used);

            if (strstr(uplink->if_name, "g-") != uplink->if_name)
                continue;

            if (uplink->is_used)
                continue;

            cm2_ovsdb_remove_Wifi_Inet_Config(uplink->if_name, true);
        }
        free(uplink_p);
    }
}

bool
cm2_ovsdb_set_Wifi_Inet_Config_network_state(bool state, char *ifname)
{
    struct schema_Wifi_Inet_Config icfg;
    int                            ret;

    memset(&icfg, 0, sizeof(icfg));

    if (strlen(ifname) == 0)
        return 0;

    LOGI("%s: Set new network state: %d", ifname, state);

    icfg.network = state;
    char *filter[] = { "+",
                       SCHEMA_COLUMN(Wifi_Inet_Config, network),
                       NULL };

    ret = ovsdb_table_update_where_f(&table_Wifi_Inet_Config,
                 ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Inet_Config, if_name), ifname),
                 &icfg, filter);

    return ret == 1;
}

static void
cm2_ovsdb_util_translate_master_ip(struct schema_Wifi_Master_State *master)
{
   bool is_sta;
   int  ret;

   LOGI("%s: Detected ip change = %s", master->if_name, master->inet_addr);

   is_sta = !strcmp(master->if_type, VIF_TYPE_NAME) && cm2_util_vif_is_sta(master->if_name);

   if (strcmp(master->if_name, BR_WAN_NAME) && !is_sta)
       return;

   if (!(cm2_util_set_dhcp(master, false)))
       return;

   if (is_sta) {
       if (strcmp(master->port_state, "inactive") == 0) {
           LOGI("%s: Skip creating GRE for inactive station",
                master->if_name);
           return;
       }

       /* Skip creating more gre during onboarding, workaround for CAES-599 */
       if (g_state.link.is_used) {
           LOGI("Skip creating new gre for %s CAES-599",
                master->if_name);
           return;
       }

       LOGN("%s Trigger creating gre", master->if_name);

       ret = cm2_ovsdb_insert_Wifi_Inet_Config(master);
       if (!ret)
           LOGW("%s: %s Failed to insert GRE", __func__, master->if_name);
   }
}

static void
cm2_ovsdb_util_handle_master_sta_port_state(struct schema_Wifi_Master_State *master,
                                            bool port_state,
                                            char *gre_ifname)
{
    int ret;

    if (port_state) {
        cm2_util_set_dhcp(master, true);

        ret = cm2_ovsdb_set_Wifi_Inet_Config_interface_enabled(true, master->if_name);
        if (!ret)
            LOGW("%s: %s: Failed to set interface enabled", __func__, master->if_name);
    } else {
        ret = cm2_ovsdb_set_Wifi_Inet_Config_interface_enabled(false, master->if_name);
        if (!ret)
            LOGW("%s: %s: Failed to set interface enabled", __func__, master->if_name);

        if (!strcmp(gre_ifname, g_state.link.if_name) && g_state.link.is_used) {
            ret = cm2_ovs_insert_port_into_bridge(BR_WAN_NAME, g_state.link.if_name, false);
            if (!ret)
                LOGW("%s: Failed to remove port %s from %s", __func__, master->if_name, BR_WAN_NAME);
        }

        cm2_ovsdb_remove_Wifi_Inet_Config(gre_ifname, true);
    }

    ret = cm2_ovsdb_set_Wifi_Inet_Config_network_state(port_state, master->if_name);
    if (!ret)
        LOGW("%s: %s: Failed to set network state %d", __func__, master->if_name, port_state);
}

static void
cm2_ovsdb_util_translate_master_port_state(struct schema_Wifi_Master_State *master)
{
    struct schema_Connection_Manager_Uplink con;
    char                                    gre_ifname[IFNAME_SIZE];
    bool                                    ret;
    bool                                    port_state;

    memset(&con, 0, sizeof(con));

    if (strlen(master->port_state) <= 0)
        return;

    port_state = strcmp(master->port_state, "active") == 0 ? true : false;

    LOGN("%s: Detected new port_state = %s", master->if_name, master->port_state);

    if (!strcmp(master->if_type, VIF_TYPE_NAME) && cm2_util_vif_is_sta(master->if_name)) {
        tsnprintf(gre_ifname, sizeof(gre_ifname), "g-%s", master->if_name);
        cm2_ovsdb_util_handle_master_sta_port_state(master, port_state, gre_ifname);
        return;
    }

    if ((strcmp(master->if_type, GRE_TYPE_NAME) != 0 ||
         strstr(master->if_name, "g-") == NULL) &&
         !cm2_is_eth_type(master->if_type)) {
        LOGD("%s Skip setting interface as ready ifname = %s iftype = %s",
             __func__, master->if_name, master->if_type);
        return;
    }

    memset(&con, 0, sizeof(con));

    LOGI("%s: Add new uplink into Connection Manager Uplink table", master->if_name);

    STRSCPY(con.if_name, master->if_name);
    STRSCPY(con.if_type, master->if_type);
    con.has_L2_exists = true;
    con.has_L2 = port_state;
    con.priority_exists = true;
    /* NM2 does not handle priority Set default values by CM2 */
    //con.priority = master->uplink_priority;
    con.priority = cm2_util_set_defined_priority(master->if_type);

    ret = ovsdb_table_upsert_simple(&table_Connection_Manager_Uplink,
                                   SCHEMA_COLUMN(Connection_Manager_Uplink, if_name),
                                   con.if_name,
                                   &con,
                                   NULL);
    if (!ret)
        LOGE("%s Insert new row failed for %s", __func__, con.if_name);
}

static void
cm2_ovsdb_util_translate_master_priority(struct schema_Wifi_Master_State *master) {
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+", SCHEMA_COLUMN(Connection_Manager_Uplink, priority), NULL };

    LOGI("%s: Detected new priority = %d", master->if_name, master->uplink_priority);

    if (!strstr(master->if_name, "g-") &&
        !cm2_is_eth_type(master->if_type))
        return;

    memset(&con, 0, sizeof(con));
    con.priority_exists = true;
    //con.priority = master->uplink_priority;
    /* NM2 does not handle priority Set default values by CM2 */
    con.priority = cm2_util_set_defined_priority(master->if_type);

    int ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                         ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name),
                                         master->if_name),
                                         &con, filter);

    if (!ret)
        LOGE("%s: %s Update priority %d failed", __func__, master->if_name, con.priority);
}
/**** End helper functions*/

bool cm2_ovsdb_is_port_in_bridge(struct schema_Port *port, char *port_uuid)
{
    json_t *where;

    where = ovsdb_where_uuid("_uuid", port_uuid);
    if (!where) {
        LOGW("%s: where is NULL", __func__);
        return -1;
    }
    return ovsdb_table_select_one_where(&table_Port, where, port);
}

bool
cm2_ovsdb_connection_update_L3_state(const char *if_name, bool state)
{
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+", SCHEMA_COLUMN(Connection_Manager_Uplink, has_L3), NULL };
    int ret;

    memset(&con, 0, sizeof(con));
    con.has_L3_exists = true;
    con.has_L3 = state;

    ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                     ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                     &con, filter);
    return ret;
}

static bool
cm2_ovsdb_connection_update_used_state(char *if_name, bool state)
{
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+", SCHEMA_COLUMN(Connection_Manager_Uplink, is_used), NULL };

    memset(&con, 0, sizeof(con));
    con.is_used_exists = true;
    con.is_used = state;

    int ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                         ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                         &con, filter);
    return ret;
}

bool
cm2_ovsdb_connection_update_loop_state(const char *if_name, bool state)
{
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+", SCHEMA_COLUMN(Connection_Manager_Uplink, loop), NULL };
    int ret;

    memset(&con, 0, sizeof(con));
    con.loop_exists = true;
    con.loop = state;

    ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                     ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                     &con, filter);
    return ret;
}

bool cm2_ovsdb_connection_get_connection_by_ifname(const char *if_name, struct schema_Connection_Manager_Uplink *uplink) {
    return ovsdb_table_select_one(&table_Connection_Manager_Uplink, SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name, uplink);
}

void cm2_ovsdb_connection_update_ble_phy_link(void) {
    struct schema_Connection_Manager_Uplink *uplink;
    void                                    *uplink_p;
    bool                                    state;
    int                                     wifi_cnt;
    int                                     eth_cnt;
    int                                     count;
    int                                     i;

    uplink_p = ovsdb_table_select_typed(&table_Connection_Manager_Uplink,
                                        SCHEMA_COLUMN(Connection_Manager_Uplink, has_L2),
                                        OCLM_BOOL,
                                        (void *) &state,
                                        &count);

    LOGI("BLE active phy links: %d",  count);

    wifi_cnt = 0;
    eth_cnt  = 0;

    if (uplink_p) {
        for (i = 0; i < count; i++) {
            uplink = (struct schema_Connection_Manager_Uplink *) (uplink_p + table_Connection_Manager_Uplink.schema_size * i);
            LOGI("Link %d: ifname = %s iftype = %s active state= %d", i, uplink->if_name, uplink->if_type, uplink->has_L2);

            if (cm2_is_eth_type(uplink->if_type))
                eth_cnt++;
            else if (!strcmp(uplink->if_type, GRE_TYPE_NAME))
                wifi_cnt++;
        }
        free(uplink_p);
    }

    state = eth_cnt > 0 ? true : false;
    cm2_ble_onboarding_set_status(state, BLE_ONBOARDING_STATUS_ETHERNET_LINK);
    state = wifi_cnt > 0 ? true : false;
    cm2_ble_onboarding_set_status(state, BLE_ONBOARDING_STATUS_WIFI_LINK);
    cm2_ble_onboarding_apply_config();
}

bool cm2_ovsdb_connection_update_ntp_state(const char *if_name, bool state) {
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+",  SCHEMA_COLUMN(Connection_Manager_Uplink, ntp_state), NULL};
    int ret;

    memset(&con, 0, sizeof(con));

    con.ntp_state_exists = true;
    con.ntp_state = state;

    ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                     ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                     &con, filter);
    return ret;
}

bool cm2_ovsdb_connection_update_unreachable_link_counter(const char *if_name, int counter) {
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+",  SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_link_counter), NULL};
    int ret;

    memset(&con, 0, sizeof(con));

    con.unreachable_link_counter_exists = true;
    con.unreachable_link_counter = counter;

    ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                     ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                     &con, filter);
    return ret;
}

bool cm2_ovsdb_connection_update_unreachable_router_counter(const char *if_name, int counter) {
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+",  SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_router_counter), NULL};
    int ret;

    memset(&con, 0, sizeof(con));

    con.unreachable_router_counter_exists = true;
    con.unreachable_router_counter = counter;

    ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                     ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                     &con, filter);
    return ret;
}

bool cm2_ovsdb_connection_update_unreachable_internet_counter(const char *if_name, int counter) {
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+",  SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_internet_counter), NULL};
    int ret;

    memset(&con, 0, sizeof(con));

    con.unreachable_internet_counter_exists = true;
    con.unreachable_internet_counter = counter;

    ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                     ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                     &con, filter);
    return ret;
}

bool cm2_ovsdb_connection_update_unreachable_cloud_counter(const char *if_name, int counter) {
    struct schema_Connection_Manager_Uplink con;
    char *filter[] = { "+",  SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_cloud_counter), NULL};
    int ret;

    memset(&con, 0, sizeof(con));

    con.unreachable_cloud_counter_exists = true;
    con.unreachable_cloud_counter = counter;

    ret = ovsdb_table_update_where_f(&table_Connection_Manager_Uplink,
                                         ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                                         &con, filter);
    return ret;
}

bool cm2_ovsdb_connection_remove_uplink(char *if_name) {
    int ret;

    ret = ovsdb_table_delete_simple(&table_Connection_Manager_Uplink,
                                   SCHEMA_COLUMN(Connection_Manager_Uplink, if_name),
                                   if_name);
    if (!ret)
        LOGE("%s Remove row failed %s", __func__, if_name);

    return ret == 1;
}

void cm2_connection_set_L3(struct schema_Connection_Manager_Uplink *uplink) {
    if (!uplink->has_L2)
        return;

    if (cm2_ovs_insert_port_into_bridge(BR_HOME_NAME, uplink->if_name, false))
        LOGW("%s: Failed to remove port %s from %s", __func__, BR_HOME_NAME, uplink->if_name);

#ifdef CONFIG_PLUME_CM2_DISABLE_DRYRUN_ON_GRE
    if (!strcmp(uplink->if_type, GRE_TYPE_NAME)) {
        int ret;

        LOGN("%s: GRE uplink is marked as active link", uplink->if_name);
        ret = cm2_ovsdb_connection_update_L3_state(uplink->if_name, true);
        if (!ret)
            LOGW("%s: %s: Update L3 state failed ret = %d",
                 __func__, uplink->if_name, ret);
    } else
#endif
    cm2_dhcpc_start_dryrun(uplink->if_name, uplink->if_type, false);
}

bool cm2_connection_get_used_link(struct schema_Connection_Manager_Uplink *uplink) {
    return ovsdb_table_select_one(&table_Connection_Manager_Uplink, SCHEMA_COLUMN(Connection_Manager_Uplink, is_used), "true", uplink);
}

static void cm2_connection_clear_used(void)
{
    int ret;

    if (g_state.link.is_used) {
        LOGN("%s: Remove old used link.", g_state.link.if_name);
        cm2_ovs_insert_port_into_bridge(BR_WAN_NAME, g_state.link.if_name, false);

        ret = cm2_ovsdb_connection_update_used_state(g_state.link.if_name, false);
        if (!ret)
            LOGI("%s: %s: Failed to clear used state", __func__, g_state.link.if_name);

        g_state.link.is_used = false;
        g_state.link.priority = -1;
    }
}

static void cm2_util_switch_role(struct schema_Connection_Manager_Uplink *uplink)
{
    if (!g_state.connected) {
        LOGI("Device is not connected to the Cloud");
        return;
    }

    if (!strcmp(g_state.link.if_type, GRE_TYPE_NAME) &&
        cm2_is_eth_type(uplink->if_type)) {
        LOGI("Device switch from Leaf to GW, trigger restart managers");
        target_device_restart_managers();
    }
}

static void cm2_connection_set_is_used(struct schema_Connection_Manager_Uplink *uplink)
{
    int ret;

    if (!uplink->has_L2 || !uplink->has_L3)
        return;

    if (!cm2_is_eth_type(uplink->if_type) &&
        strcmp(uplink->if_type, GRE_TYPE_NAME))
        return;

    if (g_state.link.is_used && uplink->priority <= g_state.link.priority)
        return;

    cm2_util_switch_role(uplink);
    cm2_connection_clear_used();

    STRSCPY(g_state.link.if_name, uplink->if_name);
    STRSCPY(g_state.link.if_type, uplink->if_type);
    g_state.link.is_used = true;
    g_state.link.priority = uplink->priority;

    LOGN("%s: Set new used link", uplink->if_name);
    ret = cm2_ovs_insert_port_into_bridge(BR_WAN_NAME, uplink->if_name, true);
    if (!ret)
        LOGW("%s: Failed to add port %s into %s", __func__, uplink->if_name, BR_WAN_NAME);

    ret = cm2_ovsdb_connection_update_used_state(uplink->if_name, true);
    if (!ret)
        LOGW("%s: %s: Failed to set used state", __func__, uplink->if_name);
}

void cm2_check_master_state_links(void) {
    struct schema_Wifi_Master_State *link;
    void   *link_p;
    int    count;
    int    ret;
    int    i;

    link_p = ovsdb_table_select(&table_Wifi_Master_State,
                                  SCHEMA_COLUMN(Wifi_Master_State, port_state),
                                  "active",
                                  &count);

    LOGI("%s: Available active links in Wifi_Master_State = %d", __func__, count);

    if (!link_p)
        return;

    for (i = 0; i < count; i++) {
        link = (struct schema_Wifi_Master_State *) (link_p + table_Wifi_Master_State.schema_size * i);
        if (cm2_util_vif_is_sta(link->if_name)) {
            LOGN("%s: Trigger creating gre", link->if_name);

            ret = cm2_ovsdb_insert_Wifi_Inet_Config(link);
            if (!ret)
               LOGW("%s: %s Failed to insert GRE", __func__, link->if_name);
        }
    }
    free(link_p);
}

void cm2_connection_recalculate_used_link(void) {
    struct schema_Connection_Manager_Uplink *uplink;
    void *uplink_p;
    int count, i;
    int priority = -1;
    int index = 0;
    bool state = true;
    bool check_master = true;

    uplink_p = ovsdb_table_select_typed(&table_Connection_Manager_Uplink,
                                        SCHEMA_COLUMN(Connection_Manager_Uplink, has_L3),
                                        OCLM_BOOL,
                                        (void *) &state,
                                        &count);

    LOGN("%s: Recalculating link. Available links %d", __func__, count);

    if (uplink_p) {
        for (i = 0; i < count; i++) {
            uplink = (struct schema_Connection_Manager_Uplink *) (uplink_p + table_Connection_Manager_Uplink.schema_size * i);
            LOGI("Link %d: ifname = %s priority = %d active state= %d", i, uplink->if_name, uplink->priority, uplink->has_L3);
            if (uplink->priority > priority) {
                index = i;
                priority = uplink->priority;
            }
        }
        uplink = (struct schema_Connection_Manager_Uplink *) (uplink_p + table_Connection_Manager_Uplink.schema_size * index);
        cm2_connection_set_is_used(uplink);
        free(uplink_p);
        check_master = false;
    }

    if (check_master)
        cm2_check_master_state_links();
}

int cm2_ovsdb_ble_config_update(uint8_t ble_status)
{
    struct schema_AW_Bluetooth_Config ble;
    int ret;

    memset(&ble, 0, sizeof(ble));

    ble.mode_exists = true;
    STRSCPY(ble.mode, CM2_BLE_MODE_ON);

    ble.command_exists = true;
    STRSCPY(ble.command, CM2_BLE_MSG_ONBOARDING);

    ble.payload_exists = true;
    snprintf(ble.payload, sizeof(ble.payload), "%02x:00:00:00:00:00", ble_status);

    ble.interval_millis_exists = true;
    ble.interval_millis = CM2_BLE_INTERVAL_VALUE_DEFAULT;

    ble.txpower_exists = true;
    ble.txpower = CM2_BLE_TXPOWER_VALUE_DEFAULT;

    ret = ovsdb_table_upsert_simple(&table_AW_Bluetooth_Config,
                                   SCHEMA_COLUMN(AW_Bluetooth_Config, command),
                                   ble.command,
                                   &ble,
                                   NULL);
    if (!ret)
        LOGE("%s Insert new row failed for %s", __func__, ble.command);

    return ret == 1;
}

void cm2_set_ble_onboarding_link_state(bool state, char *if_type, char *if_name)
{
    if (g_state.connected)
        return;

    if (cm2_is_eth_type(if_type)) {
        cm2_ble_onboarding_set_status(state, BLE_ONBOARDING_STATUS_ETHERNET_LINK);
    } else if (!strcmp(if_type, GRE_TYPE_NAME)) {
        cm2_ble_onboarding_set_status(state, BLE_ONBOARDING_STATUS_WIFI_LINK);
    }
    cm2_ble_onboarding_apply_config();
}

void callback_Wifi_Master_State(ovsdb_update_monitor_t *mon,
                                struct schema_Wifi_Master_State *old_rec,
                                struct schema_Wifi_Master_State *master)
{
    int ret;

    LOGD("%s calling %s", __func__, master->if_name);

    if (mon->mon_type == OVSDB_UPDATE_DEL) {
        char gre_ifname[IFNAME_SIZE];

        LOGI("%s: Remove row detected in Master State", master->if_name);

        if (cm2_util_vif_is_sta(master->if_name) && !strcmp(master->if_type, VIF_TYPE_NAME)) {
            tsnprintf(gre_ifname, sizeof(gre_ifname), "g-%s", master->if_name);
            ret = cm2_ovsdb_connection_remove_uplink(gre_ifname);
            if (!ret)
                LOGW("%s Remove uplink %s failed", __func__, gre_ifname);
        }

        if (strstr(master->if_name, "g-") == master->if_name &&
            cm2_util_vif_is_sta(master->if_name + strlen("g-"))) {
            ret = cm2_ovsdb_connection_remove_uplink(master->if_name);
            if (!ret)
                LOGW("%s Remove uplink %s failed", __func__, master->if_name);
        }

        return;
    }

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, port_state)))
            cm2_ovsdb_util_translate_master_port_state(master);

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, network_state)))
    {
        LOGI("%s: Detected network_state change = %s", master->if_name, master->network_state);

        if (!strncmp(master->if_name, BR_WAN_NAME, strlen(master->if_name))) {
            ret = cm2_ovsdb_set_Wifi_Inet_Config_network_state(true, master->if_name);
            if (!ret)
                LOGW("%s: %s: Failed to set network enabled", __func__, master->if_name);
        }
    }

    /* Creating GRE interfaces */
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, inet_addr)))
        cm2_ovsdb_util_translate_master_ip(master);

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, uplink_priority)))
        cm2_ovsdb_util_translate_master_priority(master);

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, if_name)))
        LOGD("%s if_name = %s changed not handled ", __func__, master->if_name);

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, if_type)))
        LOGD("%s if_type = %s changed not handled", __func__, master->if_type);

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, if_uuid)))
        LOGD("%s if_uuid changed", __func__);

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, netmask)))
        LOGD("%s netmask = %s changed not handled", __func__, master->netmask);

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Master_State, dhcpc)))
        LOGD("%s dhcpc changed not handled", __func__);
}

static void
cm2_util_set_not_used_link(void)
{
    cm2_connection_clear_used();
    cm2_update_state(CM2_REASON_LINK_NOT_USED);
    cm2_connection_recalculate_used_link();
}

static void
cm2_Connection_Manager_Uplink_handle_update(
        ovsdb_update_monitor_t *mon,
        struct schema_Connection_Manager_Uplink *uplink)
{
    bool clean_up_counters = false;
    bool reconfigure       = false;
    char *filter[8];
    int  idx = 0;
    int  ret;

    filter[idx++] = "+";

    /* Configuration part. Setting by NM, WM, Cloud */
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, priority))) {
        LOGN("%s: Uplink table: detected priority change = %d", uplink->if_name, uplink->priority);
        LOGI("%s: Main link priority: %d new priority = %d uplink is used = %d",
             uplink->if_name, g_state.link.priority , uplink->priority , uplink->is_used);
        if ((g_state.link.is_used && uplink->has_L3 && g_state.link.priority < uplink->priority) ||
            cm2_util_get_link_is_used(uplink)) {
           cm2_util_switch_role(uplink);
           reconfigure = true;
        }
    }

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, loop))) {
        LOGN("%s: Uplink table: detected loop change = %d", uplink->if_name, uplink->loop);
    }

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, has_L2))) {
        LOGN("%s: Uplink table: detected hasL2 change = %d", uplink->if_name, uplink->has_L2);
        if (!uplink->has_L2) {
            cm2_dhcpc_stop_dryrun(uplink->if_name);

            if (!strcmp(uplink->if_type, ETH_TYPE_NAME)) {
                ret = cm2_ovs_insert_port_into_bridge(BR_HOME_NAME, uplink->if_name, false);
                if (!ret)
                    LOGI("%s: Failed to remove port %s from %s", __func__, BR_HOME_NAME, uplink->if_name);
            }

            if (cm2_util_get_link_is_used(uplink)) {
                reconfigure = true;
            } else {
                clean_up_counters = true;
            }
            filter[idx++] = SCHEMA_COLUMN(Connection_Manager_Uplink, has_L3);
            uplink->has_L3 = false;
            g_state.link.vtag.state = CM2_VTAG_NOT_USED;
        } else {
            cm2_set_ble_onboarding_link_state(true, uplink->if_type, uplink->if_name);
            cm2_connection_set_L3(uplink);
        }
    }

    /* End of configuration part */

    /* Setting state part */
    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, has_L3))) {
        LOGN("%s: Uplink table: detected hasL3 change = %d", uplink->if_name, uplink->has_L3);
        cm2_connection_set_is_used(uplink);
    }

    if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, is_used))) {
        LOGN("%s: Uplink table: detected link_usage change = %d",
             uplink->if_name, uplink->is_used);

        if (!uplink->is_used && !cm2_util_get_link_is_used(uplink))
            clean_up_counters = true;

        if (uplink->is_used)
            cm2_update_state(CM2_REASON_LINK_USED);
    }

    if (uplink->is_used) {
      if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_router_counter))) {
          cm2_ble_onboarding_set_status(uplink->unreachable_router_counter == 0, BLE_ONBOARDING_STATUS_ROUTER_OK);
          cm2_ble_onboarding_apply_config();
      }

      if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_cloud_counter))) {
          cm2_ble_onboarding_set_status(uplink->unreachable_cloud_counter == 0, BLE_ONBOARDING_STATUS_CLOUD_OK);
          cm2_ble_onboarding_apply_config();
      }

      if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_internet_counter))) {
          cm2_ble_onboarding_set_status(uplink->unreachable_internet_counter == 0, BLE_ONBOARDING_STATUS_INTERNET_OK);
          cm2_ble_onboarding_apply_config();
      }
    }
    /* End of State part */


    if (clean_up_counters || reconfigure) {
        LOGN("%s: Clean up counters", uplink->if_name);

        filter[idx++] = SCHEMA_COLUMN(Connection_Manager_Uplink, ntp_state);
        uplink->ntp_state = false;
        uplink->ntp_state_exists = true;

        filter[idx++] = SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_link_counter);
        uplink->unreachable_link_counter = -1;
        uplink->unreachable_link_counter_exists = true;

        filter[idx++] = SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_router_counter);
        uplink->unreachable_router_counter = -1;
        uplink->unreachable_router_counter_exists = true;

        filter[idx++] = SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_cloud_counter);
        uplink->unreachable_cloud_counter = -1;
        uplink->unreachable_cloud_counter_exists = true;

        filter[idx++] = SCHEMA_COLUMN(Connection_Manager_Uplink, unreachable_internet_counter);
        uplink->unreachable_internet_counter = -1;
        uplink->unreachable_internet_counter_exists = true;

        filter[idx] = NULL;

        int ret = ovsdb_table_update_f(&table_Connection_Manager_Uplink, uplink, filter);
        if (!ret)
            LOGI("%s Update row failed for %s", __func__, uplink->if_name);
    }

    if (reconfigure) {
        LOGN("%s Reconfigure main link", uplink->if_name);
        cm2_connection_clear_used();
        cm2_update_state(CM2_REASON_LINK_NOT_USED);
        cm2_connection_recalculate_used_link();
    }
}

void callback_Connection_Manager_Uplink(ovsdb_update_monitor_t *mon,
                                        struct schema_Connection_Manager_Uplink *old_row,
                                        struct schema_Connection_Manager_Uplink *uplink)
{
    int ret;

    LOGD("%s mon_type = %d", __func__, mon->mon_type);

    switch (mon->mon_type) {
        default:
        case OVSDB_UPDATE_ERROR:
            LOGW("%s: mon upd error: %d", __func__, mon->mon_type);
            return;

        case OVSDB_UPDATE_DEL:
            if (cm2_util_get_link_is_used(uplink)) {
                cm2_util_set_not_used_link();
            }
            cm2_dhcpc_stop_dryrun(uplink->if_name);

            if (!strcmp(uplink->if_type, ETH_TYPE_NAME)) {
                ret = cm2_ovs_insert_port_into_bridge(BR_HOME_NAME, uplink->if_name, false);
                if (!ret)
                    LOGI("%s: Failed to remove port %s from %s", __func__, BR_HOME_NAME, uplink->if_name);
            }
            break;
        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:
            cm2_Connection_Manager_Uplink_handle_update(mon, uplink);
            break;
    }
}

static void cm2_Wifi_Inet_State_handle_dhcpc(struct schema_Wifi_Inet_State *inet_state)
{
    const char   *tag = NULL;
    size_t       bufsz;
    char         buf[8192];
    char         *pbuf;
    int          vsc;
    int          i;

    LOGD("%s; Update dhcpc column", inet_state->if_name);

    if (strcmp(inet_state->if_name, BR_WAN_NAME)) {
        LOGD("%s: Skip update, only %s support", inet_state->if_name, BR_WAN_NAME);
        return;
    }

    memset(&buf, 0, sizeof(buf));

    for (i = 0; i < inet_state->dhcpc_len; i++) {
        LOGD("dhcpc: [%s]: %s", inet_state->dhcpc_keys[i], inet_state->dhcpc[i]);

        if (strcmp(inet_state->dhcpc_keys[i], "vendorspec"))
            continue;

        bufsz = base64_decode(buf, sizeof(buf), inet_state->dhcpc[i]);
        if ((int) bufsz < 0) {
            LOGW("vtag: base64: Error decoding buffer");
            return;
        }

        if (bufsz > sizeof(buf)) {
            LOGW("vtag: base64: Buf overflowed: bufsz = %zu buf size = %zu", bufsz, sizeof(buf));
            return;
        }

        if (strlen(buf) > bufsz) {
            if (bufsz + 1 > sizeof(buf)) {
                LOGW("vtag: base64: Buf is not null terminated and too long: bufsz = %zu buf size = %zu",
                     bufsz, sizeof(buf));
                return;
            }
            buf[bufsz] = '\0';
        }
        vsc = 0;
        pbuf = buf;
        while (pbuf <= buf + bufsz) {
            if (pbuf + strlen(pbuf) > buf + bufsz) {
                LOGW("vtag: base64: Format error");
                return;
            }

            if (vsc >= CM2_BASE64_ARGV_MAX) {
                LOGW("vtag: base64: Too many arguments in buffer");
                return;
            }

            tag = strstr(pbuf, "tag=");
            if (tag) {
                LOGI("vtag: tag detected: %s", tag);
                tag += 4;
                break;
            }
            vsc++;
            pbuf += strlen(pbuf) + 1;
        }

        if (tag == NULL) {
            LOGD("vtag: tag not detected");
            return;
        }

        if (g_state.link.vtag.state == CM2_VTAG_PENDING) {
            LOGI("vtag: state error: already running");
            return;
        }

        g_state.link.vtag.tag = atoi(tag);
        LOGI("vtag: set new tag: %d", g_state.link.vtag.tag);

        cm2_update_state(CM2_REASON_SET_NEW_VTAG);
    }
}

void callback_Wifi_Inet_State(ovsdb_update_monitor_t *mon,
                              struct schema_Wifi_Inet_State *old_row,
                              struct schema_Wifi_Inet_State *inet_state)
{
    LOGD("%s mon_type = %d", __func__, mon->mon_type);

    switch (mon->mon_type) {
        default:
        case OVSDB_UPDATE_ERROR:
            LOGW("%s: mon upd error: %d", __func__, mon->mon_type);
            return;

        case OVSDB_UPDATE_DEL:
            break;
        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:
            if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Inet_State, inet_addr))) {
                if (!strcmp(inet_state->if_type, BRIDGE_TYPE_NAME)) {
                    if (strlen(inet_state->inet_addr) <= 0)
                        g_state.link.is_ip = false;
                    else
                        g_state.link.is_ip = strcmp(inet_state->inet_addr, "0.0.0.0") == 0 ? false : true;
                }
            }

            if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Wifi_Inet_State, dhcpc)))
                cm2_Wifi_Inet_State_handle_dhcpc(inet_state);

            break;
    }
}

static void cm2_reconfigure_ethernet_states(void)
{
    struct schema_Connection_Manager_Uplink *uplink;
    void                                    *uplink_p;
    int                                     count;
    int                                     i;

    uplink_p = ovsdb_table_select(&table_Connection_Manager_Uplink,
                                  SCHEMA_COLUMN(Connection_Manager_Uplink, if_type),
                                  ETH_TYPE_NAME,
                                  &count);
    if (!uplink_p) {
        LOGD("%s: Ethernet uplinks to avaialble", __func__);
        return;
    }

    LOGI("Reconfigure ethernet phy links: %d",  count);

    for (i = 0; i < count; i++) {
        uplink = (struct schema_Connection_Manager_Uplink *) (uplink_p + table_Connection_Manager_Uplink.schema_size * i);
        LOGI("Link %d: ifname = %s iftype = %s has_L2 = %d has_L3 = %d", i, uplink->if_name, uplink->if_type, uplink->has_L2, uplink->has_L3);
        if (!uplink->has_L2 || !uplink->has_L3_exists)
            continue;

        if (cm2_is_iface_in_bridge(BR_HOME_NAME, uplink->if_name)) {
            LOGI("%s: Skip reconfigure iface in %s", uplink->if_name, BR_HOME_NAME);
            continue;
        }

        if (!uplink->has_L3) {
            LOGI("%s: Ethernet link must be examinated once again", uplink->if_name);
            cm2_delayed_eth_update(uplink->if_name, CM2_ETH_BOOT_TIMEOUT);
        }
    }
    free(uplink_p);
}

void callback_Bridge(ovsdb_update_monitor_t *mon,
                     struct schema_Bridge *old_row,
                     struct schema_Bridge *bridge)
{
    struct schema_Port port;
    bool               skip_uuid;
    int                i,j;

    LOGD("%s mon_type = %d", __func__, mon->mon_type);

    switch (mon->mon_type) {
        default:
        case OVSDB_UPDATE_ERROR:
            LOGW("%s: mon upd error: %d", __func__, mon->mon_type);
            return;

        case OVSDB_UPDATE_DEL:
            LOGI("%s: Bridge remove detected", bridge->name);
            break;
        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:
            if (ovsdb_update_changed(mon, SCHEMA_COLUMN(Bridge, ports))) {
               if (!strcmp(bridge->name, BR_WAN_NAME) &&
                   !strcmp(g_state.link.if_type, GRE_TYPE_NAME)) {
                   cm2_reconfigure_ethernet_states();
                   break;
               }

               if (strcmp(bridge->name, BR_HOME_NAME))
                   break;

               for (i = 0; i < bridge->ports_len; i++ ) {
                   LOGD("%s: uuid: %s", bridge->name, bridge->ports[i].uuid);
                   skip_uuid = false;
                   for (j = 0; j < old_row->ports_len; j++) {
                       if (!strcmp(bridge->ports[i].uuid, old_row->ports[j].uuid)) {
                           skip_uuid = true;
                           break;
                       }
                   }
                   if (!skip_uuid &&
                       cm2_ovsdb_is_port_in_bridge(&port, bridge->ports[i].uuid)) {
                       if (strstr(port.name, "patch-h2w")){
                           LOGI("Bridge mode detected");
                           g_state.link.is_limp_state = false;
                       }

                       if (strstr(port.name, ETH_TYPE_NAME))
                           cm2_dhcpc_stop_dryrun(port.name);
                    }
                }
            }
            break;
    }
}

bool cm2_ovsdb_set_Manager_target(char *target)
{
    struct schema_Manager manager;
    memset(&manager, 0, sizeof(manager));
    STRSCPY(manager.target, target);
    manager.is_connected = false;
    char *filter[] = { "+", SCHEMA_COLUMN(Manager, target), SCHEMA_COLUMN(Manager, is_connected), NULL };
    int ret = ovsdb_table_update_where_f(&table_Manager, NULL, &manager, filter);
    return ret == 1;
}

bool cm2_ovsdb_set_AWLAN_Node_manager_addr(char *addr)
{
    struct schema_AWLAN_Node awlan;
    memset(&awlan, 0, sizeof(awlan));
    STRSCPY(awlan.manager_addr, addr);
    char *filter[] = { "+", SCHEMA_COLUMN(AWLAN_Node, manager_addr), NULL };
    int ret = ovsdb_table_update_where_f(&table_AWLAN_Node, NULL, &awlan, filter);
    return ret == 1;
}

static void
cm2_awlan_state_update_cb(
        struct schema_AWLAN_Node *awlan,
        schema_filter_t          *filter)
{
    bool ret;

    if (!filter || filter->num <= 0) {
       LOGE("Updating awlan_node (no filter fields)");
       return;
    }

    ret = ovsdb_table_update_f(
            &table_AWLAN_Node,
            awlan, filter->columns);
    if (!ret){
        LOGE("Updating awlan_node");
    }
}

bool cm2_ovsdb_is_port_name(char *port_name)
{
    struct schema_Port port;
    return ovsdb_table_select_one(&table_Port, SCHEMA_COLUMN(Port, name), port_name, &port);
}

bool cm2_ovsdb_update_Port_tag(const char *if_name, int tag, bool set)
{
    struct schema_Port port;
    int                ret;

    memset(&port, 0, sizeof(port));

    if (strlen(if_name) == 0)
        return 0;

    LOGI("%s: update vtag: %d set = %d", if_name, tag, set);
    port._partial_update = true;

    if (set) {
        SCHEMA_SET_INT(port.tag, tag);
    } else {
        port.tag_present = true;
        port.tag_exists = false;
    }

    ret = ovsdb_table_update_where(&table_Port,
                 ovsdb_where_simple(SCHEMA_COLUMN(Port, name), if_name),
                 &port);

    return ret == 1;
}

// Initialize Open_vSwitch, SSL and Manager tables
int cm2_ovsdb_init_tables(void)
{
    // SSL and Manager tables have to be referenced by Open_vSwitch
    // so we use _with_parent() to atomically update (mutate) these references
    struct schema_Open_vSwitch ovs;
    struct schema_Manager manager;
    struct schema_SSL ssl;
    struct schema_AWLAN_Node awlan;
    bool success = false;
    char *ovs_filter[] = {
        "-",
        SCHEMA_COLUMN(Open_vSwitch, bridges),
        SCHEMA_COLUMN(Open_vSwitch, manager_options),
        SCHEMA_COLUMN(Open_vSwitch, other_config),
        NULL
    };
    int retval = 0;

    /* Update redirector address from target storage! */
    LOGD("Initializing CM tables "
            "(Init AWLAN_Node device config changes)");
    target_device_config_register(cm2_awlan_state_update_cb);

    // Open_vSwitch
    LOGD("Initializing CM tables "
            "(Init Open_vSwitch table)");
    memset(&ovs, 0, sizeof(ovs));
    success = ovsdb_table_upsert_f(&table_Open_vSwitch, &ovs,
                                                        false, ovs_filter);
    if (!success) {
        LOGE("Initializing CM tables "
             "(Failed to setup OvS table)");
        retval = -1;
    }

    // Manager
    LOGD("Initializing CM tables "
            "(Init OvS.Manager table)");
    memset(&manager, 0, sizeof(manager));
    manager.inactivity_probe = 30000;
    manager.inactivity_probe_exists = true;
    strcpy(manager.target, "");
    success = ovsdb_table_upsert_with_parent_where(&table_Manager,
            NULL, &manager, false, NULL,
            SCHEMA_TABLE(Open_vSwitch), NULL,
            SCHEMA_COLUMN(Open_vSwitch, manager_options));
    if (!success) {
        LOGE("Initializing CM tables "
                     "(Failed to setup Manager table)");
        retval = -1;
    }

    // SSL
    LOGD("Initializing CM tables "
            "(Init OvS.SSL table)");
    memset(&ssl, 0, sizeof(ssl));
    strcpy(ssl.ca_cert, target_tls_cacert_filename());
    strcpy(ssl.certificate, target_tls_mycert_filename());
    strcpy(ssl.private_key, target_tls_privkey_filename());
    success = ovsdb_table_upsert_with_parent(&table_SSL,
            &ssl, false, NULL,
            SCHEMA_TABLE(Open_vSwitch), NULL,
            SCHEMA_COLUMN(Open_vSwitch, ssl));
    if (!success) {
        LOGE("Initializing CM tables "
                     "(Failed to setup SSL table)");
        retval = -1;
    }

    // AWLAN_Node
    g_state.min_backoff = 30;
    g_state.max_backoff = 60;
    LOGD("Initializing CM tables "
         "(Init AWLAN_Node table)");
    memset(&awlan, 0, sizeof(awlan));
    awlan.min_backoff = g_state.min_backoff;
    awlan.max_backoff = g_state.max_backoff;
    char *filter[] = { "+",
                       SCHEMA_COLUMN(AWLAN_Node, min_backoff),
                       SCHEMA_COLUMN(AWLAN_Node, max_backoff),
                       NULL };
    success = ovsdb_table_update_where_f(&table_AWLAN_Node, NULL, &awlan, filter);
    if (!success) {
        LOGE("Initializing CM tables "
             "(Failed to setup AWLAN_Node table)");
        retval = -1;
    }
    return retval;
}

/* Connection_Manager_Uplink table is a main table of Connection Manager for
 * setting configuration and keep information about link state.
 * Configuration/state columns set by NM/WM/Cloud:
 *   if_name  - name of ready interface for using
 *   if_type  - type of interface
 *   priority - link priority during link selection
 *   loop     - loop detected on specific interface
 * State columns set only by CM:
 *   Link states: has_L2, has_L3, is_used
 *   Link stability counters: unreachable_link_counter, unreachable_router_counter,
 *                            unreachable_cloud_counter, unreachable_internet_counter
 *   NTP state: ntp state
 */

int cm2_ovsdb_init(void)
{
    LOGI("Initializing CM tables");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT_NO_KEY(Open_vSwitch);
    OVSDB_TABLE_INIT_NO_KEY(Manager);
    OVSDB_TABLE_INIT_NO_KEY(SSL);
    OVSDB_TABLE_INIT_NO_KEY(AWLAN_Node);
    OVSDB_TABLE_INIT(Wifi_Master_State, if_name);
    OVSDB_TABLE_INIT(Wifi_Inet_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_Inet_State, if_name);
    OVSDB_TABLE_INIT(Wifi_VIF_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_VIF_State, if_name);
    OVSDB_TABLE_INIT(Connection_Manager_Uplink, if_name);
    OVSDB_TABLE_INIT_NO_KEY(AW_Bluetooth_Config);
    OVSDB_TABLE_INIT_NO_KEY(Port);
    OVSDB_TABLE_INIT_NO_KEY(Bridge);

    // Initialize OVSDB monitor callbacks
    OVSDB_TABLE_MONITOR(AWLAN_Node, false);

    // Callback for EXTENDER
    if (cm2_is_extender()) {
        OVSDB_TABLE_MONITOR(Wifi_Master_State, false);
        OVSDB_TABLE_MONITOR(Connection_Manager_Uplink, false);
        OVSDB_TABLE_MONITOR(Wifi_Inet_State, false);
        OVSDB_TABLE_MONITOR(Bridge, false);
    }

    char *filter[] = {"-", "_version", SCHEMA_COLUMN(Manager, status), NULL};
    OVSDB_TABLE_MONITOR_F(Manager, filter);

    // Initialize OvS tables
    if (cm2_ovsdb_init_tables())
    {
        LOGE("Initializing CM tables "
             "(Failed to setup tables)");
        return -1;
    }

    return 0;
}
