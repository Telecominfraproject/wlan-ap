/**
 * @file gatt_app.c
 * @brief GATT client application plugin.
 *
 * Uses libble API for GATT operations: connect, discover, read, write.
 * Subscribes to libble events for connection state tracking.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>

#include "app_plugin.h"
#include "scan_filter.h"
#include "scan_file.h"

#define GATT_MAX_CONNS 4

static struct {
    char address[BLE_ADDR_STR_LEN];
    uint16_t conn_handle;
    bool active;
} connections[GATT_MAX_CONNS];

static int conn_count = 0;

static void gatt_event_cb(const ble_event_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;

    switch (event->type) {
    case BLE_EVT_CONNECT:
        syslog(LOG_INFO, "gatt: connected to %s (handle=%u)",
               event->connect.address, event->connect.conn_handle);
        if (conn_count < GATT_MAX_CONNS) {
            strncpy(connections[conn_count].address, event->connect.address,
                    BLE_ADDR_STR_LEN - 1);
            connections[conn_count].conn_handle = event->connect.conn_handle;
            connections[conn_count].active = true;
            conn_count++;
        }
        break;

    case BLE_EVT_DISCONNECT:
        syslog(LOG_INFO, "gatt: disconnected handle=%u reason=%u",
               event->disconnect.conn_handle, event->disconnect.reason);
        for (int i = 0; i < conn_count; i++) {
            if (connections[i].conn_handle == event->disconnect.conn_handle) {
                connections[i].active = false;
                /* Compact array */
                int rem = conn_count - i - 1;
                if (rem > 0)
                    memmove(&connections[i], &connections[i + 1],
                            rem * sizeof(connections[0]));
                conn_count--;
                break;
            }
        }
        break;

    case BLE_EVT_GATT_NOTIFY:
        syslog(LOG_DEBUG, "gatt: notify handle=%u len=%u",
               event->gatt_data.handle, event->gatt_data.value_len);
        break;

    default:
        break;
    }
}

static int gatt_init(void)
{
    memset(connections, 0, sizeof(connections));
    conn_count = 0;
    ble_subscribe(gatt_event_cb, NULL);
    syslog(LOG_INFO, "gatt app initialized");
    return 0;
}

static void gatt_deinit(void)
{
    /* Disconnect all */
    for (int i = 0; i < conn_count; i++) {
        if (connections[i].active)
            ble_gatt_disconnect(connections[i].conn_handle);
    }
    ble_unsubscribe(gatt_event_cb);
    conn_count = 0;
    syslog(LOG_INFO, "gatt app deinitialized");
}

static int gatt_start(void)
{
    /* GATT app is always ready — connections happen on demand */
    return 0;
}

static int gatt_stop(void)
{
    for (int i = 0; i < conn_count; i++) {
        if (connections[i].active)
            ble_gatt_disconnect(connections[i].conn_handle);
    }
    return 0;
}

app_plugin_t gatt_app_plugin = {
    .name   = "gatt",
    .init   = gatt_init,
    .deinit = gatt_deinit,
    .start  = gatt_start,
    .stop   = gatt_stop,
    .running = false,
};

/* Helper used by main.c */
void app_plugins_init(void)
{
    extern app_plugin_t gatt_server_app_plugin;
    extern bool uci_app_get_bool(const char *, const char *, const char *, bool);
    extern int uci_app_get_string(const char *, const char *, const char *,
                                  char *, size_t, const char *);

    ibeacon_app_plugin.init();
    scan_app_plugin.init();
    gatt_app_plugin.init();
    gatt_server_app_plugin.init();

    /*
     * Always start gatt_server_app — it opens D-Bus and initializes
     * ble_adv module (shared infrastructure for all advertising).
     * GATT registration + connectable advertising is conditional on
     * gatt_server.enabled in UCI (checked inside gatt_server_start).
     */
    gatt_server_app_plugin.start();

    /* Autostart ibeacon — ble_adv is now ready */
    extern bool ibeacon_autostart_enabled(void);
    if (ibeacon_autostart_enabled()) {
        syslog(LOG_INFO, "Autostart: iBeacon");
        ibeacon_app_plugin.start();
    }

    /* Autostart scan if enabled in UCI */
    if (uci_app_get_bool("ble", "scan", "enabled", false)) {
        char filter[32] = "all";
        char dur_str[16] = "10000";
        uci_app_get_string("ble", "scan", "filter", filter, sizeof(filter), "all");
        uci_app_get_string("ble", "scan", "duration", dur_str, sizeof(dur_str), "10000");

        uint32_t duration = (uint32_t)atoi(dur_str);
        bool active = uci_app_get_bool("ble", "scan", "active", true);
        bool filter_dup = uci_app_get_bool("ble", "scan", "filter_dup", true);

        /* Set filter and open file via the scan infrastructure */
        extern uint32_t active_scan_filter;
        extern scan_file_ctx_t active_scan_ctx;
        extern char active_scan_filter_name[32];
        extern uint32_t scan_filter_parse(const char *);

        char mr_str[16] = "1000", ol_str[16] = "rotate";
        uci_app_get_string("ble", "scan", "max_records", mr_str, sizeof(mr_str), "1000");
        uci_app_get_string("ble", "scan", "on_limit", ol_str, sizeof(ol_str), "rotate");
        uint32_t max_records = (uint32_t)atoi(mr_str);
        scan_limit_action_t on_limit = scan_file_parse_limit_action(ol_str);

        active_scan_filter = scan_filter_parse(filter);
        strncpy(active_scan_filter_name, filter, sizeof(active_scan_filter_name) - 1);
        scan_file_open(&active_scan_ctx, filter, max_records, on_limit);

        ble_scan_start(duration, active, filter_dup);
        syslog(LOG_INFO, "Autostart: Scan (filter=%s duration=%ums)", filter, duration);
    }
}

void app_plugins_deinit(void)
{
    extern app_plugin_t gatt_server_app_plugin;

    gatt_server_app_plugin.stop();
    gatt_app_plugin.deinit();
    scan_app_plugin.deinit();
    ibeacon_app_plugin.deinit();
}
