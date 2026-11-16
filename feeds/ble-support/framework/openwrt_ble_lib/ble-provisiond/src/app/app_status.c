/**
 * @file app_status.c
 * @brief Application-layer runtime status file writer. See app_status.h.
 *
 * Copyright © Accton Technology Corporation. All rights reserved.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <syslog.h>

#include "app_status.h"

/* UCI + libble helpers (implemented elsewhere in the daemon / library). */
extern bool uci_app_get_bool(const char *pkg, const char *section,
                             const char *option, bool default_val);
extern int  uci_app_get_string(const char *pkg, const char *section,
                               const char *option, char *buf, size_t buf_size,
                               const char *default_val);
extern const char *ble_get_transport_name(void);
extern const char *ble_get_chip_profile_name(void);

/* The resolved advertised name — see gatt_server_app.h for why reading
 * ble.gatt_server.device_name directly is not enough. */
extern const char *gatt_server_device_name(void);

/* Board identity and the resolved advertisement tag. Declared here rather than
 * by including adv_status.h, matching the externs above: this file reports other
 * modules' resolved values and takes no dependency on their headers. See
 * adv_status.h for why `board` is not called "model". */
extern const char *adv_status_board(void);
extern uint16_t    adv_status_model_id(void);

/* ── Runtime state (updated by app plugins) ── */
static struct {
    bool     scan_running;
    char     scan_results_file[128];
    bool     ibeacon_active;
} s_rt;

void app_status_set_scan_running(bool running, const char *results_file)
{
    s_rt.scan_running = running;
    if (running && results_file)
        snprintf(s_rt.scan_results_file, sizeof(s_rt.scan_results_file), "%s", results_file);
    else
        s_rt.scan_results_file[0] = '\0';
    app_status_write();
}

void app_status_set_ibeacon_active(bool active)
{
    s_rt.ibeacon_active = active;
    app_status_write();
}

static void iso_timestamp(char *buf, size_t n)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%Y-%m-%dT%H:%M:%S", &tm);
}

void app_status_write(void)
{
    char ts[64];
    iso_timestamp(ts, sizeof(ts));

    const char *transport = ble_get_transport_name();
    if (!transport) transport = "unknown";
    const char *profile = ble_get_chip_profile_name();
    if (!profile) profile = "";

    /* Config-derived fields (from UCI). */
    bool gatt_en    = uci_app_get_bool("ble", "gatt_server", "enabled", false);
    bool ibeacon_en = uci_app_get_bool("ble", "ibeacon", "enabled", false);
    bool scan_en    = uci_app_get_bool("ble", "scan", "enabled", false);

    /*
     * Report the name the radio actually uses, not the raw UCI value: the
     * shipped "OpenWrt-BLE" is a sentinel that gatt_server resolves against the
     * board. Reading UCI here would make this file disagree with the
     * advertisement on every board that takes the derived name.
     *
     * Falls back to the UCI read when gatt_server has not initialised yet, so a
     * status file written early still carries something sensible.
     */
    char dev_name[64] = "OpenWrt-BLE";
    const char *resolved = gatt_server_device_name();
    if (resolved && *resolved)
        snprintf(dev_name, sizeof(dev_name), "%s", resolved);
    else
        uci_app_get_string("ble", "gatt_server", "device_name",
                           dev_name, sizeof(dev_name), "OpenWrt-BLE");
    /*
     * Board identity and the advertised tag, from adv_status rather than re-read
     * here — same rule as device_name above: one resolver, so this file cannot
     * disagree with what is on the air.
     *
     * In the file as well as the log because `logread` is a fixed-size ring. This
     * pair is logged once at start-up, so on a busy device it is among the first
     * things to scroll out, and then the only way to answer "what tag is this
     * unit advertising" would be to restart the daemon — the same trap
     * log_since_last_start() exists for in the test suite.
     *
     * No fallback here, unlike device_name, because none is reachable: every
     * caller runs after adv_status_init(). The initial snapshot is the last thing
     * app_plugins_init() does (gatt_app.c), and the ibeacon/scan setters only
     * fire from their start/stop paths, which are later still. If that ordering
     * is ever changed, this would report model_id 0 — and 0 is not a neutral
     * placeholder, it is the value that tells the app to ignore the device, so
     * the unresolved state would be indistinguishable from a deliberate one.
     */
    const char *board = adv_status_board();
    if (!board || !*board) board = "unknown";
    unsigned model_id = adv_status_model_id();

    bool allow_ota = uci_app_get_bool("ble", "gatt_server", "allow_ota", false);
    bool allow_fr  = uci_app_get_bool("ble", "gatt_server", "allow_factory_reset", false);
    bool allow_cfg = uci_app_get_bool("ble", "gatt_server", "allow_config", true);

    char bcn_uuid[64] = "";
    char bcn_major[16] = "0", bcn_minor[16] = "0", bcn_tx[16] = "0";
    uci_app_get_string("ble", "ibeacon", "uuid", bcn_uuid, sizeof(bcn_uuid), "");
    uci_app_get_string("ble", "ibeacon", "major", bcn_major, sizeof(bcn_major), "0");
    uci_app_get_string("ble", "ibeacon", "minor", bcn_minor, sizeof(bcn_minor), "0");
    uci_app_get_string("ble", "ibeacon", "txpower", bcn_tx, sizeof(bcn_tx), "0");

    char scan_filter[32] = "all", scan_dur[16] = "10000";
    uci_app_get_string("ble", "scan", "filter", scan_filter, sizeof(scan_filter), "all");
    uci_app_get_string("ble", "scan", "duration", scan_dur, sizeof(scan_dur), "10000");
    bool scan_active_mode = uci_app_get_bool("ble", "scan", "active", true);

    FILE *fp = fopen(APP_STATUS_FILE, "w");
    if (!fp) {
        syslog(LOG_WARNING, "app_status: cannot write %s", APP_STATUS_FILE);
        return;
    }

    fprintf(fp,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"transport\": \"%s\",\n"
        "  \"chip_profile\": \"%s\",\n"
        "  \"board\": \"%s\",\n"
        "  \"model_id\": %u,\n"
        "  \"gatt_server\": {\n"
        "    \"enabled\": %s,\n"
        "    \"device_name\": \"%s\",\n"
        "    \"services\": [\"e063c3a0-0748-40be-935e-01ebdeafa814\"],\n"
        "    \"allow_ota\": %s,\n"
        "    \"allow_factory_reset\": %s,\n"
        "    \"allow_config\": %s\n"
        "  },\n"
        "  \"ibeacon\": {\n"
        "    \"enabled\": %s,\n"
        "    \"active\": %s,\n"
        "    \"uuid\": \"%s\",\n"
        "    \"major\": %s,\n"
        "    \"minor\": %s,\n"
        "    \"tx_power\": %s\n"
        "  },\n"
        "  \"scan\": {\n"
        "    \"enabled\": %s,\n"
        "    \"running\": %s,\n"
        "    \"active\": %s,\n"
        "    \"filter\": \"%s\",\n"
        "    \"duration_ms\": %s,\n"
        "    \"results_file\": \"%s\"\n"
        "  }\n"
        "}\n",
        ts, transport, profile,
        board, model_id,
        gatt_en ? "true" : "false",
        dev_name,
        allow_ota ? "true" : "false",
        allow_fr ? "true" : "false",
        allow_cfg ? "true" : "false",
        ibeacon_en ? "true" : "false",
        s_rt.ibeacon_active ? "true" : "false",
        bcn_uuid, bcn_major, bcn_minor, bcn_tx,
        scan_en ? "true" : "false",
        s_rt.scan_running ? "true" : "false",
        scan_active_mode ? "true" : "false",
        scan_filter, scan_dur,
        s_rt.scan_results_file);

    fclose(fp);
    syslog(LOG_DEBUG, "app_status: wrote %s", APP_STATUS_FILE);
}

void app_status_remove(void)
{
    remove(APP_STATUS_FILE);
}
