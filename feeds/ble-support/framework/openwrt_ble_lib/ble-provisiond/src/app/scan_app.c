/**
 * @file scan_app.c
 * @brief BLE scanner application plugin.
 *
 * Uses libble API for scan operations. Tracks results and manages
 * autostart from UCI configuration.
 */

#include <string.h>
#include <syslog.h>

#include "app_plugin.h"

static uint32_t scan_duration = 10000;
static bool scan_active_mode = true;
static bool scan_filter_dup = true;
static bool autostart = false;
static uint32_t results_count = 0;

static void scan_event_cb(const ble_event_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;

    switch (event->type) {
    case BLE_EVT_SCAN_RESULT:
        results_count++;
        syslog(LOG_DEBUG, "scan: result addr=%s rssi=%d",
               event->scan_result.address, event->scan_result.rssi);
        break;
    case BLE_EVT_SCAN_COMPLETE:
        syslog(LOG_INFO, "scan: complete, %u results", event->scan_complete.count);
        break;
    default:
        break;
    }
}

static int scan_init(void)
{
    results_count = 0;
    ble_subscribe(scan_event_cb, NULL);

    if (autostart) {
        ble_scan_start(scan_duration, scan_active_mode, scan_filter_dup);
    }

    syslog(LOG_INFO, "scan app initialized");
    return 0;
}

static void scan_deinit(void)
{
    ble_unsubscribe(scan_event_cb);
    if (ble_scan_is_active())
        ble_scan_stop();
    syslog(LOG_INFO, "scan app deinitialized");
}

static int scan_start(void)
{
    results_count = 0;
    return ble_scan_start(scan_duration, scan_active_mode, scan_filter_dup);
}

static int scan_stop(void)
{
    return ble_scan_stop();
}

app_plugin_t scan_app_plugin = {
    .name   = "scan",
    .init   = scan_init,
    .deinit = scan_deinit,
    .start  = scan_start,
    .stop   = scan_stop,
    .running = false,
};
