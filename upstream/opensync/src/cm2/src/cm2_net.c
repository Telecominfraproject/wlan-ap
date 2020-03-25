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
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include "ev.h"

#include "log.h"
#include "schema.h"
#include "target.h"
#include "cm2.h"

#define CM2_VAR_RUN_PATH               "/var/run"
#define CM2_UDHCPC_DRYRUN_PREFIX_FILE  "udhcpc-cmdryrun"

/* ev_child must be the first element of the structure
 * based on that fact we can store additional data, in that case
 * interface name and interface type */
typedef struct {
    ev_child cw;
    char     if_name[128];
    char     if_type[128];
} dhcp_dryrun_t;

typedef struct {
    ev_timer timer;
    char     if_name[128];
} cm2_delayed_eth_update_t;

int cm2_ovs_insert_port_into_bridge(char *bridge, char *port, int flag_add)
{
    char *op_add = "add-port";
    char *op_del = "del-port";
    char *op_and = "&&";
    char *op_or  = "||";
    char command[128];
    char *op_log;
    char *op;

    if (flag_add) {
        op = op_add;
        op_log = op_or;
    } else {
        op = op_del;
        op_log = op_and;
    }

    LOGI("OVS bridge: %s port = %s bridge = %s", op, port, bridge);

    /* add/delete it to/from OVS bridge */
    sprintf(command, "ovs-vsctl list-ifaces %s | grep %s %s ovs-vsctl %s %s %s",
            bridge, port, op_log, op, bridge, port);

    LOGD("%s: Command: %s", __func__, command);

    return target_device_execute(command);
}

/**
 * Return the PID of the udhcpc client serving on interface @p ifname
 */
static int cm2_util_get_udhcp_pid(char *pidfile)
{
    int  pid;
    FILE *f;
    int  rc;

    f = fopen(pidfile, "r");
    if (f == NULL)
        return 0;

    rc = fscanf(f, "%d", &pid);
    fclose(f);

    /* We should read exactly 1 element */
    if (rc != 1)
        return 0;

    if (kill(pid, 0) != 0)
        return 0;

    return pid;
}

bool cm2_is_iface_in_bridge(const char *bridge, const char *port)
{
    char command[128];

    LOGD("OVS bridge: port = %s bridge = %s", port, bridge);
    sprintf(command, "ovs-vsctl list-ifaces %s | grep %s",
            bridge, port);

    LOGD("%s: Command: %s", __func__, command);
    return target_device_execute(command);
}

static void
cm2_delayed_eth_update_cb(struct ev_loop *loop, ev_timer *timer, int revents)
{
    cm2_delayed_eth_update_t *p;

    p = (void *)timer;
    LOGI("%s: delayed eth update cb", p->if_name);
    ev_timer_stop(EV_DEFAULT, &p->timer);
    cm2_ovsdb_connection_update_loop_state(p->if_name, false);
    free(p);
}

void cm2_delayed_eth_update(char *if_name, int timeout)
{
    struct schema_Connection_Manager_Uplink con;
    cm2_delayed_eth_update_t                *p;

    if (!cm2_ovsdb_connection_get_connection_by_ifname(if_name, &con)) {
        LOGW("%s: eth_update: interface does not exist", if_name);
        return;
    }

    if (con.loop) {
        LOGI("%s: eth_update: skip due to existed loop ", if_name);
        return;
    }

    if (!(p = malloc(sizeof(*p)))) {
        LOGW("%s: eth_update: memory allocation failed", if_name);
        return;
    }

    cm2_ovsdb_connection_update_loop_state(if_name, true);
    STRSCPY(p->if_name, if_name);
    ev_timer_init(&p->timer, cm2_delayed_eth_update_cb, timeout, 0);
    ev_timer_start(EV_DEFAULT, &p->timer);
    LOGI("%s: scheduling delayed eth update", if_name);
}

static void cm2_dhcpc_dryrun_cb(struct ev_loop *loop, ev_child *w, int revents)
{
    struct schema_Connection_Manager_Uplink con;
    dhcp_dryrun_t                           *dhcp_dryrun;
    bool                                    status;
    int                                     ret;
    int                                     eth_timeout;

    dhcp_dryrun = (dhcp_dryrun_t *) w;

    ev_child_stop (loop, w);
    if (WIFEXITED(w->rstatus) && WEXITSTATUS(w->rstatus) == 0)
        status = true;
    else
        status = false;

    if (WIFEXITED(w->rstatus))
        LOGD("%s: %s: rstatus = %d", __func__,
             dhcp_dryrun->if_name, WEXITSTATUS(w->rstatus));

    LOGI("%s: dryrun state: %d", dhcp_dryrun->if_name, w->rstatus);

    ret = cm2_ovsdb_connection_get_connection_by_ifname(dhcp_dryrun->if_name, &con);
    if (!ret) {
        LOGD("%s: interface %s does not exist", __func__, dhcp_dryrun->if_name);
        goto release;
    }

    if (cm2_is_iface_in_bridge(BR_HOME_NAME, dhcp_dryrun->if_name)) {
        LOGI("%s: Skip run dryrun in background, iface in %s",
            dhcp_dryrun->if_name, BR_HOME_NAME);
        goto release;
    }

    if (!status && con.has_L2 && !strcmp(dhcp_dryrun->if_type, ETH_TYPE_NAME)) {
        cm2_dhcpc_start_dryrun(dhcp_dryrun->if_name, dhcp_dryrun->if_type, true);

        if (g_state.link.is_used &&
            !strcmp(g_state.link.if_type, GRE_TYPE_NAME)) {
            LOGI("Detected Leaf with pluged ethernet, connected = %d",
                 g_state.connected);
            eth_timeout = g_state.connected ? CM2_ETH_SYNC_TIMEOUT : CM2_ETH_BOOT_TIMEOUT;
            cm2_delayed_eth_update(dhcp_dryrun->if_name, eth_timeout);
        }
    }

    ret = cm2_ovsdb_connection_update_L3_state(dhcp_dryrun->if_name, status);
    if (!ret)
        LOGW("%s: %s: Update L3 state failed status = %d ret = %d",
             __func__, dhcp_dryrun->if_name, status, ret);
release:
    free(dhcp_dryrun);
}

void cm2_dhcpc_start_dryrun(char* ifname, char *iftype, bool background)
{
    char pidfile[256];
    char udhcpc_s_option[256];
    pid_t pid;
    char n_param[3];

    LOGN("%s: Trigger dryrun, background = %d", ifname, background);

    tsnprintf(pidfile, sizeof(pidfile), "%s/%s-%s.pid",
              CM2_VAR_RUN_PATH , CM2_UDHCPC_DRYRUN_PREFIX_FILE, ifname);

    pid = cm2_util_get_udhcp_pid(pidfile);
    if (pid > 0)
    {
        LOGI("%s: DHCP client already running", ifname);
        return;
    }

    tsnprintf(udhcpc_s_option, sizeof(udhcpc_s_option),
              "/usr/plume/bin/udhcpc-dryrun.sh");
#ifndef CONFIG_UDHCPC_OPTIONS_DISABLE_VENDOR_CLASSID
    char vendor_classid[256];
    if(target_model_get(vendor_classid, sizeof(vendor_classid)) == false)
    {
        tsnprintf(vendor_classid, sizeof(vendor_classid),
                  TARGET_NAME);
    }
#endif

    if (background)
        STRSCPY(n_param, "");
    else
        STRSCPY(n_param, "-n");

    char *argv_dry_run[] = {
        "/sbin/udhcpc",
        "-p", pidfile,
        n_param,
        "-t", "6",
        "-T", "1",
        "-A", "2",
        "-f",
        "-i", ifname,
        "-s", udhcpc_s_option,
#ifndef CONFIG_UDHCPC_OPTIONS_USE_CLIENTID
        "-C",
#endif
        "-S",
#ifndef CONFIG_PLUME_CM2_USE_NOT_CUSTOM_UDHCPC
        "-Q",
#endif
#ifndef CONFIG_UDHCPC_OPTIONS_DISABLE_VENDOR_CLASSID
        "-V", vendor_classid,
#endif
        "-q",
        NULL
    };

    pid = fork();
    if (pid == 0) {
        execv(argv_dry_run[0], argv_dry_run);
        LOGW("%s: %s: failed to exec dry dhcp: %d (%s)",
             __func__, ifname, errno, strerror(errno));
        exit(1);
    } else {
        dhcp_dryrun_t *dhcp_dryrun = (dhcp_dryrun_t *) malloc(sizeof(dhcp_dryrun_t));

        memset(dhcp_dryrun, 0, sizeof(dhcp_dryrun_t));
        STRSCPY(dhcp_dryrun->if_name, ifname);
        STRSCPY(dhcp_dryrun->if_type, iftype);

        ev_child_init (&dhcp_dryrun->cw, cm2_dhcpc_dryrun_cb, pid, 0);
        ev_child_start (EV_DEFAULT, &dhcp_dryrun->cw);
    }
}

void cm2_dhcpc_stop_dryrun(char *ifname)
{
    pid_t pid;
    char  pidfile[256];

    tsnprintf(pidfile, sizeof(pidfile), "%s/%s-%s.pid",
              CM2_VAR_RUN_PATH , CM2_UDHCPC_DRYRUN_PREFIX_FILE, ifname);

    pid = cm2_util_get_udhcp_pid(pidfile);
    if (!pid) {
        LOGI("%s: DHCP client is not running", ifname);
        return;
    }

    LOGI("%s: pid: %d pid file: %s", ifname, pid, pidfile);

    if (kill(pid, SIGKILL) < 0) {
        LOGW("%s: %s: failed to send kill signal: %d (%s)",
             __func__, ifname, errno, strerror(errno));
    }
}

bool cm2_is_eth_type(char *if_type) {
    return !strcmp(if_type, ETH_TYPE_NAME) ||
           !strcmp(if_type, VLAN_TYPE_NAME);
}
