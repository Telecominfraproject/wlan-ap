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

#include <stdbool.h>

#include "log.h"
#include "schema.h"
#include "target.h"
#include "cm2.h"

/* WATCHDOG CONFIGURATION */
#define CM2_WDT_INTERVAL                10

/* CONNECTIVITY CHECK CONFIGURATION */
#define CM2_STABILITY_INTERVAL          10
#define CM2_STABILITY_THRESHOLD         8
#define CM2_STABILITY_RESTORE_CON       4
#define CM2_STABILITY_INTERNET_THRESH   6
#define CM2_STABILITY_VTAG_THRESH       10
#define CM2_STABILITY_CPU_THRESH        0.70


cm2_main_link_type cm2_util_get_link_type(void)
{
    cm2_main_link_type type = CM2_LINK_NOT_DEFINED;

    if (!g_state.link.is_used)
        return type;

    if (!strcmp(g_state.link.if_type, "eth")) {
        if (cm2_ovsdb_is_port_name("patch-w2h"))
           type = CM2_LINK_ETH_BRIDGE;
        else
           type = CM2_LINK_ETH_ROUTER;
    }

    if (!strcmp(g_state.link.if_type, "gre")) {
        type = CM2_LINK_GRE;
    }

    return type;
}

bool cm2_vtag_stability_check() {
    cm2_vtag_t *vtag = &g_state.link.vtag;

    if (vtag->state == CM2_VTAG_PENDING) {
        vtag->failure++;
        LOGI("vtag: %d connectivity failed: %d out of %d retries",
             vtag->tag, vtag->failure, CM2_STABILITY_VTAG_THRESH);

        if (vtag->failure > CM2_STABILITY_VTAG_THRESH) {
            LOGN("vtag: %d trigger rollback", vtag->tag);
            cm2_update_state(CM2_REASON_BLOCK_VTAG);
        }
        return false;
    }
    return true;
}

static bool cm2_cpu_is_low_loadavg(void) {
    char   line[128];
    bool   retval;
    char   *s_val;
    float  val;
    FILE   *f1;

    f1 = NULL;
    retval = false;

    f1 = popen("cat /proc/loadavg", "r");
    if (!f1) {
        LOGE("Failed to retreive loadavg command");
        return retval;
    }

    if (fgets(line, sizeof(line), f1) == NULL) {
        LOGW("No loadavg found");
        goto error;
    }

    while (line[strlen(line) - 1] == '\r' || line[strlen(line) - 1] == '\n') {
        line[strlen(line) - 1] = '\0';

        s_val=strtok(line," ");
        val = atof(s_val);

        if (val > CM2_STABILITY_CPU_THRESH) {
            LOGI("Skip stability check due to high CPU usage, load avg: %f", val);
            break;
        }
        retval = true;
    }

    error:
        pclose(f1);

    return retval;
}

static bool cm2_restore_connection(int cnt)
{
    char command[128];
    bool ret = true;

    LOGI("Trying restore connection");
    if (cm2_is_eth_type(g_state.link.if_type)) {
        sprintf(command, "sh /usr/plume/scripts/kick-ethernet.sh %d 0 1 ", cnt);
        LOGD("%s: Command: %s", __func__, command);
        ret = target_device_execute(command);
        if (!ret)
            LOGW("Trigger ethernet kick script failed");
        cm2_ovsdb_refresh_dhcp(BR_WAN_NAME);
    }
    return ret;
}

void cm2_connection_stability_check(void)
{
    struct schema_Connection_Manager_Uplink con;
    target_connectivity_check_option_t      opts;
    target_connectivity_check_t             cstate;
    cm2_main_link_type                      link_type;
    bool                                    ret;
    int                                     counter;

    if (!cm2_is_extender()) {
        return;
    }

    //TODO for all active links
    const char *if_name = g_state.link.if_name;

    if (!g_state.link.is_used) {
        LOGN("Waiting for new active link");
        g_state.ble_status = 0;
        cm2_ovsdb_connection_update_ble_phy_link();
        return;
    }

    ret = cm2_ovsdb_connection_get_connection_by_ifname(if_name, &con);
    if (!ret) {
        LOGW("%s interface does not exist", __func__);
        return;
    }

    if (g_state.state == CM2_STATE_NTP_CHECK) {
        opts = NTP_CHECK;
    } else {
        ret = cm2_cpu_is_low_loadavg();
        if (!ret)
            return;

        opts = LINK_CHECK | ROUTER_CHECK | NTP_CHECK;
        if (!g_state.connected)
            opts |= INTERNET_CHECK;
    }

    ret = target_device_connectivity_check(if_name, &cstate, opts);
    LOGN("Connection status %d, main link: %s opts: = %x", ret, if_name, opts);

    if (opts & LINK_CHECK) {
        counter = 0;
        if (!cstate.link_state) {
            counter = con.unreachable_link_counter + 1;
            LOGW("Detected broken link. Counter = %d", counter);
        }
        ret = cm2_ovsdb_connection_update_unreachable_link_counter(if_name, counter);
        if (!ret)
            LOGW("%s Failed update link counter in ovsdb table", __func__);
    }
    if (opts & ROUTER_CHECK) {
        counter = 0;
        if (!cstate.router_state) {
            counter =  con.unreachable_router_counter + 1;
            LOGW("Detected broken Router. Counter = %d", counter);
        }
        ret = cm2_ovsdb_connection_update_unreachable_router_counter(if_name, counter);
        if (!ret)
            LOGW("%s Failed update router counter in ovsdb table", __func__);

        if (counter > 0 && counter % CM2_STABILITY_RESTORE_CON == 0) {
            if (cm2_restore_connection(counter))
                LOGW("Restore connection trigger failed");
        }

        link_type = cm2_util_get_link_type();
        if (link_type == CM2_LINK_ETH_ROUTER) {
            if (!g_state.link.is_limp_state)
                LOGI("Device operates in Router mode");
            g_state.link.is_limp_state = true;
        } else if (link_type == CM2_LINK_ETH_BRIDGE) {
            if (g_state.link.is_limp_state)
                LOGI("Device operates in Bridge mode");
            g_state.link.is_limp_state = false;
        }

        if (con.unreachable_router_counter > 0 &&
            cm2_vtag_stability_check() &&
            !g_state.link.is_limp_state &&
            con.unreachable_router_counter + 1 > CM2_STABILITY_THRESHOLD) {
            LOGW("Restart managers due to exceeding the threshold router failures");
            target_device_restart_managers();
        }
    }
    if (opts & INTERNET_CHECK) {
        counter = 0;
        if (!cstate.internet_state) {
            counter = con.unreachable_internet_counter + 1;
            LOGW("Detected broken Internet. Counter = %d", counter);
            if (counter % CM2_STABILITY_INTERNET_THRESH == 0) {
                LOGN("Refresh br-wan interface due to Internet issue");
                cm2_ovsdb_refresh_dhcp(BR_WAN_NAME);
            }
        }
        ret = cm2_ovsdb_connection_update_unreachable_internet_counter(if_name, counter);
        if (!ret)
            LOGW("%s Failed update internet counter in ovsdb table", __func__);
    }
    if (opts & NTP_CHECK) {
        ret = cm2_ovsdb_connection_update_ntp_state(if_name,
                                                    cstate.ntp_state);
        if (!ret)
            LOGW("%s Failed update ntp state in ovsdb table", __func__);
        else
            g_state.ntp_check = cstate.ntp_state;
    }
    return;
}

void cm2_stability_cb(struct ev_loop *loop, ev_timer *watcher, int revents)
{
    (void)loop;
    (void)watcher;
    (void)revents;

    if (g_state.run_stability)
        cm2_connection_stability_check();
}

void cm2_stability_init(struct ev_loop *loop)
{
    LOGD("Initializing stability connection check");
    ev_timer_init(&g_state.stability_timer, cm2_stability_cb, CM2_STABILITY_INTERVAL, CM2_STABILITY_INTERVAL);
    g_state.stability_timer.data = NULL;
    ev_timer_start(g_state.loop, &g_state.stability_timer);
}

void cm2_stability_close(struct ev_loop *loop)
{
    LOGD("Stopping stability check");
    ev_timer_stop (loop, &g_state.stability_timer);
}

void cm2_wdt_cb(struct ev_loop *loop, ev_timer *watcher, int revents)
{
    (void)loop;
    (void)watcher;
    (void)revents;

    target_device_wdt_ping();
}

void cm2_wdt_init(struct ev_loop *loop)
{
    LOGD("Initializing WDT connection");
    ev_timer_init(&g_state.wdt_timer, cm2_wdt_cb, CM2_WDT_INTERVAL, CM2_WDT_INTERVAL);
    g_state.wdt_timer.data = NULL;
    ev_timer_start(g_state.loop, &g_state.wdt_timer);
}

void cm2_wdt_close(struct ev_loop *loop)
{
    LOGD("Stopping WDT");
    (void)loop;
}
