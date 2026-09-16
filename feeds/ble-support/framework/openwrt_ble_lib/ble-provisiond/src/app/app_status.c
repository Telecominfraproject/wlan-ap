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

    char dev_name[64] = "OpenWrt-BLE";
    uci_app_get_string("ble", "gatt_server", "device_name",
                       dev_name, sizeof(dev_name), "OpenWrt-BLE");
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
        "  \"gatt_server\": {\n"
        "    \"enabled\": %s,\n"
        "    \"device_name\": \"%s\",\n"
        "    \"services\": [\"FE00\", \"FE10\"],\n"
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
