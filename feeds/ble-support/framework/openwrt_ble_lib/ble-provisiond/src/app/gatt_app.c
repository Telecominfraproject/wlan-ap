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
     * TI multi_role GATT bridge: when the active chip profile is ti_npi, the
     * GATT server runs on the MCU and forwards phone activity over UART as NPI
     * GATT frames. Initialize the handler so those frames are routed to the
     * shared handle_* handlers (same ones the BlueZ GATT server uses).
     */
    {
        extern const char *ble_get_chip_profile_name(void);
        extern const char *ble_get_bd_address(void);
        extern int npi_gatt_handler_init(void);
        extern int npi_gatt_send_provision_setup(const char *name,
                                                 const uint8_t bdaddr[6]);
        const char *cp = ble_get_chip_profile_name();
        if (cp && strcmp(cp, "ti_npi") == 0) {
            syslog(LOG_INFO, "TI ti_npi transport: enabling NPI GATT bridge");
            npi_gatt_handler_init();

            /*
             * Push provisioning setup to the multi_role firmware: device name
             * from UCI (same key the BlueZ GATT server uses) and BD address
             * from libble (derived from eth0), then start connectable adv.
             */
            char name[64] = "OpenWrt-BLE";
            uci_app_get_string("ble", "gatt_server", "device_name",
                               name, sizeof(name), "OpenWrt-BLE");

            uint8_t bdaddr[6] = {0};
            bool have_addr = false;
            const char *addr_str = ble_get_bd_address();
            if (addr_str && *addr_str) {
                unsigned int m[6];
                if (sscanf(addr_str, "%x:%x:%x:%x:%x:%x",
                           &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
                    /* String is big-endian (MSB first); firmware wants LE. */
                    bdaddr[0] = (uint8_t)m[5]; bdaddr[1] = (uint8_t)m[4];
                    bdaddr[2] = (uint8_t)m[3]; bdaddr[3] = (uint8_t)m[2];
                    bdaddr[4] = (uint8_t)m[1]; bdaddr[5] = (uint8_t)m[0];
                    have_addr = true;
                }
            }
            /*
             * Store the params; they are pushed to the firmware when it reports
             * NPI_GATT_EVT_READY (after its advertising set is created + the
             * default scan response is loaded), so the name update is not
             * overwritten.
             */
            extern void npi_gatt_set_provision_params(const char *name,
                                                      const uint8_t *bdaddr);
            npi_gatt_set_provision_params(name, have_addr ? bdaddr : NULL);

            /*
             * Mirror UCI gatt_server.enabled so the firmware advertises the
             * provisioning service only when enabled (parity with BlueZ, which
             * only registers + advertises services when enabled). When
             * disabled, provision setup sends ADV_STOP.
             */
            bool gatt_en = uci_app_get_bool("ble", "gatt_server", "enabled", false);
            extern void npi_gatt_set_gatt_enabled(bool enabled);
            npi_gatt_set_gatt_enabled(gatt_en);

            /*
             * Actively ask the firmware to (re)emit READY. On first boot the
             * firmware emits READY on its own; but once provisioned it stops,
             * so after a daemon restart this QUERY_READY is what makes the
             * firmware re-announce readiness → the device name is re-synced.
             */
            extern int npi_gatt_query_ready(void);
            npi_gatt_query_ready();
        }
    }

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

    /*
     * The TI multi_role firmware runs autonomously and keeps scanning/beaconing
     * across daemon restarts. So if a feature is DISABLED in UCI we must
     * actively tell the firmware to stop — otherwise a scan/beacon left running
     * from a previous session continues. (BlueZ has no such residual state; this
     * is TI-specific.) Only meaningful on the ti_npi transport.
     */
    {
        extern const char *ble_get_chip_profile_name(void);
        const char *cp0 = ble_get_chip_profile_name();
        if (cp0 && strcmp(cp0, "ti_npi") == 0) {
            extern int ble_ti_force_scan_stop(void);
            extern int ble_ti_force_beacon_stop(void);
            if (!uci_app_get_bool("ble", "scan", "enabled", false)) {
                syslog(LOG_INFO, "TI: scan disabled — sending SCAN_STOP to clear residual scan");
                ble_ti_force_scan_stop();
            }
            if (!uci_app_get_bool("ble", "ibeacon", "enabled", false)) {
                syslog(LOG_INFO, "TI: ibeacon disabled — sending BEACON_STOP to clear residual beacon");
                ble_ti_force_beacon_stop();
            }
        }
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

    /* Write the initial application-layer status snapshot. Subsequent changes
     * (scan/ibeacon/gatt start/stop, connect/disconnect, MTU) rewrite it via
     * the app_status setters. */
    {
        extern void app_status_write(void);
        app_status_write();
    }
}

void app_plugins_deinit(void)
{
    extern app_plugin_t gatt_server_app_plugin;

    {
        extern const char *ble_get_chip_profile_name(void);
        extern void npi_gatt_handler_deinit(void);
        const char *cp = ble_get_chip_profile_name();
        if (cp && strcmp(cp, "ti_npi") == 0)
            npi_gatt_handler_deinit();
    }

    gatt_server_app_plugin.stop();
    gatt_app_plugin.deinit();
    scan_app_plugin.deinit();
    ibeacon_app_plugin.deinit();

    {
        extern void app_status_remove(void);
        app_status_remove();
    }
}
