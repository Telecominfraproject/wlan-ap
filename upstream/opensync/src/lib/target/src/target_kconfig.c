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

/*
 * ===========================================================================
 *  Generic TARGET layer implementation using Kconfig configuration
 * ===========================================================================
 */

#define MODULE_ID LOG_MODULE_ID_TARGET

#include "log.h"
#include "os_nif.h"
#include "target.h"

#if defined(CONFIG_TARGET_MANAGER)

#define TARGET_MANAGER_PATH(x) CONFIG_TARGET_PATH_BIN "/" x
/******************************************************************************
 *  MANAGERS definitions
 *****************************************************************************/
target_managers_config_t target_managers_config[] =
{
#if defined(CONFIG_TARGET_MANAGER_WM)
    {
        .name = TARGET_MANAGER_PATH("wm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_NM)
    {
        .name = TARGET_MANAGER_PATH("nm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_CM)
    {
        .name = TARGET_MANAGER_PATH("cm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_SM)
    {
        .name =TARGET_MANAGER_PATH("sm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_UM)
    {
        .name = TARGET_MANAGER_PATH("um"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_LM)
    {
        .name = TARGET_MANAGER_PATH("lm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_BM)
    {
        .name = TARGET_MANAGER_PATH("bm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_FM)
    {
        .name = TARGET_MANAGER_PATH("fm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_LEDM)
    {
        .name = TARGET_MANAGER_PATH("ledm"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_OM)
    {
        .name = TARGET_MANAGER_PATH("om"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_BLEM)
    {
        .name = TARGET_MANAGER_PATH("blem"),
        .needs_plan_b = true,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_QM)
    {
        .name = TARGET_MANAGER_PATH("qm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_TM)
    {
        .name =  TARGET_MANAGER_PATH("tm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_FSM)
    {
        .name =  TARGET_MANAGER_PATH("fsm"),
        .always_restart = 1,
        .restart_delay = -1,
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_FCM)
    {
        .name =  TARGET_MANAGER_PATH("fcm"),
        .always_restart = 1,
        .restart_delay = -1,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_XM)
    {
        .name =  TARGET_MANAGER_PATH("xm"),
        .needs_plan_b = false,
    },
#endif

#if defined(CONFIG_TARGET_MANAGER_HELLO_WORLD)
    {
        .name =  TARGET_MANAGER_PATH("hello_world"),
        .needs_plan_b = false,
    }
#endif

#if defined(CONFIG_TARGET_MANAGER_NFM)
    {
        .name = TARGET_MANAGER_PATH("nfm"),
        .needs_plan_b = false,
    },
#endif

};
int target_managers_num =
    (sizeof(target_managers_config) / sizeof(target_managers_config[0]));

#endif /* CONFIG_TARGET_MANAGER */

#if defined(CONFIG_TARGET_CAP_GATEWAY) || defined(CONFIG_TARGET_CAP_EXTENDER)
int target_device_capabilities_get()
{
    int cap = 0;

#if defined(CONFIG_TARGET_CAP_GATEWAY)
    cap |= TARGET_GW_TYPE;
#endif

#if defined(CONFIG_TARGET_CAP_EXTENDER)
    cap |= TARGET_EXTENDER_TYPE;
#endif

    return cap;
}
#endif

#if defined(CONFIG_TARGET_LAN_BRIDGE_NAME)
const char **target_ethclient_brlist_get()
{
    static const char *brlist[] = { CONFIG_TARGET_LAN_BRIDGE_NAME, NULL };
    return brlist;
}
#endif

#if defined(CONFIG_TARGET_ETH_LIST)
const char **target_ethclient_iflist_get()
{
    static const char *iflist[] =
    {
#if defined(CONFIG_TARGET_ETH0_LIST)
        CONFIG_TARGET_ETH0_NAME,
#endif
#if defined(CONFIG_TARGET_ETH1_LIST)
        CONFIG_TARGET_ETH1_NAME,
#endif
#if defined(CONFIG_TARGET_ETH2_LIST)
        CONFIG_TARGET_ETH2_NAME,
#endif
#if defined(CONFIG_TARGET_ETH3_LIST)
        CONFIG_TARGET_ETH3_NAME,
#endif
        NULL
    };

    return iflist;
}
#endif

#if defined(CONFIG_TARGET_SERIAL_FROM_MAC)
/*
 * Returns true if device serial name is correctly read
 */
bool target_serial_get(void *buff, size_t buffsz)
{
    size_t n;
    memset(buff, 0, buffsz);

    os_macaddr_t mac;

    /* get eth0 MAC address */
    if (!os_nif_macaddr(CONFIG_TARGET_SERIAL_FROM_MAC_IFNAME, &mac))
    {
        LOG(ERR, "Unable to retrieve MAC address for %s.", CONFIG_TARGET_SERIAL_FROM_MAC_IFNAME);
        return false;
    }

    /* convert this to string and set id & serial_number */
    n = snprintf(buff, buffsz, PRI(os_macaddr_plain_t), FMT(os_macaddr_t, mac));
    if (n >= buffsz)
    {
        LOG(ERR, "buffer not large enough");
        return false;
    }

    return true;
}
#endif /* CONFIG_TARGET_SERIAL_FROM_MAC */

#if defined(CONFIG_TARGET_MODEL_GET)
bool target_model_get(void *buff, size_t buffsz)
{
    snprintf(
            buff,
            buffsz,
            "%s",
            CONFIG_TARGET_MODEL);

    return true;
}
#endif /* CONFIG_TARGET_MODEL_GET */

#if defined(CONFIG_TARGET_PATH_BIN)
const char *target_bin_dir(void)
{
    return CONFIG_TARGET_PATH_BIN;
}
#endif /* CONFIG_TARGET_PATH_BIN */

#if defined(CONFIG_TARGET_PATH_TOOLS)
const char *target_tools_dir(void)
{
    return CONFIG_TARGET_PATH_TOOLS;
}
#endif /* CONFIG_TARGET_PATH_TOOLS */

#if defined(CONFIG_TARGET_PATH_SCRIPTS)
const char *target_scripts_dir(void)
{
    return CONFIG_TARGET_PATH_SCRIPTS;
}
#endif /* CONFIG_TARGET_PATH_SCRIPTS */

#if defined(CONFIG_TARGET_PATH_PERSISTENT)
const char *target_persistent_storage_dir(void)
{
    return CONFIG_TARGET_PATH_PERSISTENT;
}
#endif /* CONFIG_TARGET_PATH_PERSISTENT */

#if defined(CONFIG_TARGET_PATH_LOG_STATE)
const char *target_log_state_file(void)
{
    return CONFIG_TARGET_PATH_LOG_STATE;
}
#endif /* CONFIG_TARGET_PATH_LOG_STATE */

#if defined(CONFIG_TARGET_PATH_LOG_TRIGGER)
const char *target_log_trigger_dir(void)
{
    return CONFIG_TARGET_PATH_LOG_TRIGGER;
}
#endif /* CONFIG_TARGET_PATH_LOG_TRIGGER */

#if defined(CONFIG_TARGET_UPGRADE_SAFEUPDATE)
char *target_upg_command()
{
    return "safeupdate";
}

char *target_upg_command_full()
{
    return CONFIG_TARGET_PATH_TOOLS "/safeupdate";
}

char **target_upg_command_args(char *password)
{
    static char *upg_command_args[] = {
        CONFIG_TARGET_PATH_TOOLS "/safeupdate",
        "-l",
        "-p",
        NULL,
        NULL,
        NULL,
        NULL
    };

    // Handle password if given
    upg_command_args[3] = password ? "-P"      : "-w";
    upg_command_args[4] = password ? password  : NULL;
    upg_command_args[5] = password ? "-w"      : NULL;

    return upg_command_args;
}

#if defined(CONFIG_TARGET_UPGRADE_NO_PRECHECK)
/*
 * Implement dummy pre-check function which always returns true
 */
bool target_upg_download_required(char *url)
{
    (void)url;
    return true;
}
#endif /* CONFIG_TARGET_UPGRADE_NO_PRECHECK */

#endif /* CONFIG_TARGET_UPGRADE_SAFEUPDATE */

#if defined(CONFIG_TARGET_RESTART_SCRIPT)
bool target_device_restart_managers()
{
    if (access(CONFIG_TARGET_PATH_DISABLE_FATAL_STATE, F_OK) == 0) {
        LOGEM("FATAL condition triggered, not restarting managers by request "
        "(%s exists)", CONFIG_TARGET_PATH_DISABLE_FATAL_STATE);
    }
    else {
        pid_t pid;
        char *argv[] = {NULL} ;

        LOGEM("FATAL condition triggered, restarting managers...");
        pid = fork();
        if (pid == 0) {
            int rc = execvp(CONFIG_TARGET_RESTART_SCRIPT_CMD, argv);
            exit((rc == 0) ? 0 : 1);
        }
        while(1); // Sit in loop and wait to be restarted
    }
    return true;
}
#endif

#if defined(CONFIG_TARGET_LINUX_LOGPULL)
bool target_log_pull(const char *upload_location, const char *upload_token)
{
    // TODO: command cleanup (remove hc params, etc...)
    char shell_cmd[1024];
    snprintf(shell_cmd, sizeof(shell_cmd),
        "sh "CONFIG_TARGET_PATH_SCRIPTS"/lm_logs_collector.sh"
        " %s"
        " %s"
        " "CONFIG_TARGET_PATH_LOG_LM
        " syslog"
        " syslog_copy"
        " tmp"
        " crash"
        " /tmp/etc/openvswitch/conf.db"
        " /tmp/ovsdb.log",
        upload_location,
        upload_token);

    // On success we return true
    return !cmd_log(shell_cmd);
}
#endif

#if defined(CONFIG_PML_TARGET) && !defined(CONFIG_TARGET_WATCHDOG)
/* Implement dummy watchdog function */
bool target_device_wdt_ping(void)
{
    return true;
}
#endif

#if defined(CONFIG_TARGET_LINUX_EXECUTE)
bool target_device_execute(const char *cmd)
{
    int rc = system(cmd);

    if (!WIFEXITED(rc) || WEXITSTATUS(rc) != 0) return false;

    return true;
}
#endif

#if defined(CONFIG_TARGET_FIXED_HWREV)
bool target_hw_revision_get(void *buff, size_t buffsz)
{
    snprintf(
            buff,
            buffsz,
            "%s",
            CONFIG_TARGET_FIXED_HWREV_STRING);

    return true;
}
#endif

#if defined(CONFIG_TARGET_FIXED_SKU)
/*
 * Dummy function
 */
bool target_sku_get(void *buff, size_t buffsz)
{
    snprintf(
            buff,
            buffsz,
            "%s",
            CONFIG_TARGET_SKU_STRING);

    return true;
}
#endif

#if defined(CONFIG_TARGET_FREE_SPACE_REPORTING)
double target_upg_free_space_err()
{
        return (double)CONFIG_TARGET_FREE_SPACE_ERROR / 1024.0;
}

double target_upg_free_space_warn()
{
        return (double)CONFIG_TARGET_FREE_SPACE_WARNING / 1024.0;
}
#endif

#if defined(CONFIG_TARGET_LED_SUPPORT)
const char *target_led_device_dir(void)
{
    return CONFIG_TARGET_LED_PATH;
}

int target_led_names(const char **leds[])
{
    static const char *led_names[] =
    {
#if defined(CONFIG_TARGET_LED0)
        CONFIG_TARGET_LED0_NAME,
#endif
#if defined(CONFIG_TARGET_LED1)
        CONFIG_TARGET_LED1_NAME,
#endif
#if defined(CONFIG_TARGET_LED2)
        CONFIG_TARGET_LED2_NAME,
#endif
#if defined(CONFIG_TARGET_LED3)
        CONFIG_TARGET_LED3_NAME,
#endif
        NULL
    };

    *leds = led_names;
    return (sizeof(led_names) / (sizeof(led_names[0]))) - 1;
}
#endif /* CONFIG_TARGET_LED_SUPPORT */


#if defined(CONFIG_TARGET_CM_LINUX_SUPPORT_PACKAGE)

#include <arpa/inet.h>
#include <errno.h>

#include "os_random.h"

/* NTP CHECK CONFIGURATION */
// Max number of times to check for NTP before continuing
#define NTP_CHECK_MAX_COUNT             10  // 10 x 5 = 50 seconds
// NTP check passes once time is greater then this
#define TIME_NTP_DEFAULT                1000000
// File used to disable waiting for NTP
#define DISABLE_NTP_CHECK               "/opt/tb/cm-disable-ntp-check"

/* CONNECTIVITY CHECK CONFIGURATION */
#define PROC_NET_ROUTE                  "/proc/net/route"
#define DEFAULT_PING_PACKET_CNT         2
#define DEFAULT_PING_TIMEOUT            4

// Internet IP Addresses
static char *util_connectivity_check_inet_addrs[] = {
    "198.41.0.4",
    "192.228.79.201",
    "192.33.4.12",
    "199.7.91.13",
    "192.5.5.241",
    "198.97.190.53",
    "192.36.148.17",
    "192.58.128.30",
    "193.0.14.129",
    "199.7.83.42",
    "202.12.27.33",
    NULL
};

static int util_connectivity_get_inet_addr_cnt(void)
{
    char **p = util_connectivity_check_inet_addrs;
    int n = 0;

    while (*p) {
        p++;
        n++;
    }
    return n;
}

/******************************************************************************
 * Utility: connectivity, ntp check
 *****************************************************************************/

static int
util_timespec_cmp_lt(struct timespec *cur, struct timespec *ref)
{
     if (cur == NULL || ref == NULL)
         return 0;

     if (cur->tv_sec < ref->tv_sec)
         return 1;

     if (cur->tv_sec == ref->tv_sec)
         return cur->tv_nsec < ref->tv_nsec;

     return 0;
}

static time_t
util_year_to_epoch(int year, int month)
{
     struct tm time_formatted;

     if (year < 1900)
        return -1;

    memset(&time_formatted, 0, sizeof(time_formatted));
        time_formatted.tm_year = year - 1900;
    time_formatted.tm_mday = 1;
        time_formatted.tm_mon  = month;

    return mktime(&time_formatted);
}

static bool
util_ntp_check(void)
{
    struct timespec cur;
    struct timespec target;
    int ret = true;

    target.tv_sec = util_year_to_epoch(2014, 1);
    if (target.tv_sec < 0)
        target.tv_sec = TIME_NTP_DEFAULT;

    target.tv_nsec = 0;

    if (clock_gettime(CLOCK_REALTIME, &cur) != 0) {
        LOGE("Failed to get wall clock, errno=%d", errno);
        return false;
    }

    if (util_timespec_cmp_lt(&cur, &target))
        ret = false;

    return ret;
}

static bool
util_ping_cmd(const char *ipstr)
{
    char cmd[128];
    int rc;

    snprintf(cmd, sizeof(cmd), "ping %s -c %d -w %d >/dev/null 2>&1",
             ipstr, DEFAULT_PING_PACKET_CNT, DEFAULT_PING_TIMEOUT);

    rc = target_device_execute(cmd);
    LOGD("Ping %s result %d (cmd=%s)", ipstr, rc, cmd);

    return rc;
}

static bool
util_get_router_ip(struct in_addr *dest)
{
    FILE *f1;
    char line[128];
    char *ifn, *dst, *gw, *msk, *sptr;
    int i, rc = false;

    if ((f1 = fopen(PROC_NET_ROUTE, "rt"))) {
        while(fgets(line, sizeof(line), f1)) {
            ifn = strtok_r(line, " \t", &sptr);         // Interface name
            dst = strtok_r(NULL, " \t", &sptr);         // Destination (base 16)
            gw  = strtok_r(NULL, " \t", &sptr);         // Gateway (base 16)
            for (i = 0;i < 4;i++) {
                // Skip: Flags, RefCnt, Use, Metric
                strtok_r(NULL, " \t", &sptr);
            }
            msk = strtok_r(NULL, " \t", &sptr);         // Netmask (base 16)
            // We don't care about the rest of the values

            if (!ifn || !dst || !gw || !msk) {
                // malformatted line
                continue;
            }

            if (!strcmp(dst, "00000000") && !strcmp(msk, "00000000")) {
                // Our default route
                memset(dest, 0, sizeof(*dest));
                dest->s_addr = strtoul(gw, NULL, 16);   // Router IP
                rc = true;
                break;
            }
        }
        fclose(f1);

        if (rc) {
            LOGD("%s: Found router IP %s", PROC_NET_ROUTE, inet_ntoa(*dest));
        }
        else {
            LOGW("%s: No router IP found", PROC_NET_ROUTE);
        }
    }
    else {
        LOGE("Failed to get router IP, unable to open %s", PROC_NET_ROUTE);
    }

    return rc;
}

static bool
util_get_link_ip(const char *ifname, struct in_addr *dest)
{
    char  line[128];
    bool  retval;
    FILE  *f1;

    f1 = NULL;
    retval = false;

    f1 = popen("ip -d link | egrep gretap | "
               " egrep bhaul-sta | ( read a b c d; echo $c )", "r");

    if (!f1) {
        LOGE("Failed to retreive Wifi Link remote IP address");
        goto error;
    }

    if (fgets(line, sizeof(line), f1) == NULL) {
        LOGW("No Wifi Link remote IP address found");
        goto error;
    }

    while(line[strlen(line)-1] == '\r' || line[strlen(line)-1] == '\n') {
        line[strlen(line)-1] = '\0';
    }

    if (inet_pton(AF_INET, line, dest) != 1) {
        LOGW("Failed to parse Wifi Link remote IP address (%s)", line);
        goto error;
    }

    retval = true;

  error:
    if (f1 != NULL)
        pclose(f1);

    return retval;
}

static bool
util_connectivity_link_check(const char *ifname)
{
    struct in_addr link_ip;

    /* GRE uses IPs on backhaul to form tunnels that are put into bridges.
     * SoftWDS doesn't rely on IPs so there's nothing to ping.
     */
    if (!strstr(ifname, "bhaul-sta"))
        return true;

    if (util_get_link_ip(ifname, &link_ip)) {
        if (util_ping_cmd(inet_ntoa(link_ip)) == false)
            return false;
    }
    return true;
}

static bool
util_connectivity_router_check()
{
    struct in_addr r_addr;

    if (util_get_router_ip(&r_addr) == false) {
        // If we don't have a router, that's considered a failure
        return false;
    }

    if (util_ping_cmd(inet_ntoa(r_addr)) == false) {
        return false;
    }

    return true;
}

static bool
util_connectivity_internet_check() {
    int r;
    int cnt_addr = util_connectivity_get_inet_addr_cnt();

    r = os_rand() % cnt_addr;
    if (util_ping_cmd(util_connectivity_check_inet_addrs[r]) == false) {
        // Try again.. Some of these DNS root servers are a little flakey
        r = os_rand() % cnt_addr;
        if (util_ping_cmd(util_connectivity_check_inet_addrs[r]) == false) {
            return false;
        }
    }
    return true;
}

/******************************************************************************
 * target device connectivity check
 *****************************************************************************/

bool target_device_connectivity_check(const char *ifname,
                                      target_connectivity_check_t *cstate,
                                      target_connectivity_check_option_t opts)
{
    memset(cstate, 0 , sizeof(target_connectivity_check_t));

    if (opts & LINK_CHECK) {
        cstate->link_state = util_connectivity_link_check(ifname);
        if (!cstate->link_state)
            return false;
    }

    if (opts & ROUTER_CHECK) {
        cstate->router_state = util_connectivity_router_check();
        if (!cstate->router_state)
            return false;
    }

    if (opts & INTERNET_CHECK) {
        cstate->internet_state = util_connectivity_internet_check();
        if (!cstate->internet_state)
            return false;
    }

    if (opts & NTP_CHECK) {
        cstate->ntp_state = util_ntp_check();
        if (!cstate->ntp_state)
            return false;
    }

    return true;
}
#endif

