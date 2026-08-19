/**
 * @file ibeacon_app.c
 * @brief iBeacon broadcaster application plugin.
 *
 * Uses libble API for beacon operations. Handles autostart from UCI.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "app_plugin.h"
#include "../uci_app_config.h"

static ble_beacon_config_t beacon_config;
static bool autostart = false;

static void ibeacon_event_cb(const ble_event_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;

    /* Monitor for state changes affecting our beacon */
    if (event->type == BLE_EVT_ERROR) {
        syslog(LOG_WARNING, "ibeacon: error event code=%d", event->error.code);
    }
}

static int ibeacon_init(void)
{
    /* Default config */
    memset(&beacon_config, 0, sizeof(beacon_config));
    snprintf(beacon_config.uuid, sizeof(beacon_config.uuid),
             "%s", "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0");
    beacon_config.major = 1;
    beacon_config.minor = 1;
    beacon_config.tx_power = -59;
    beacon_config.interval_ms = 100;

    /* Read UCI config — override defaults */
    char buf[64];
    if (uci_app_get_string("ble", "ibeacon", "uuid", buf, sizeof(buf), NULL) == 0 && buf[0]) {
        /* Format UUID with dashes if not present */
        if (strlen(buf) == 32) {
            snprintf(beacon_config.uuid, sizeof(beacon_config.uuid),
                     "%.8s-%.4s-%.4s-%.4s-%.12s",
                     buf, buf+8, buf+12, buf+16, buf+20);
        } else {
            snprintf(beacon_config.uuid, sizeof(beacon_config.uuid), "%.36s", buf);
        }
    }
    if (uci_app_get_string("ble", "ibeacon", "major", buf, sizeof(buf), NULL) == 0 && buf[0])
        beacon_config.major = (uint16_t)atoi(buf);
    if (uci_app_get_string("ble", "ibeacon", "minor", buf, sizeof(buf), NULL) == 0 && buf[0])
        beacon_config.minor = (uint16_t)atoi(buf);
    if (uci_app_get_string("ble", "ibeacon", "txpower", buf, sizeof(buf), NULL) == 0 && buf[0])
        beacon_config.tx_power = (int8_t)atoi(buf);
    if (uci_app_get_string("ble", "ibeacon", "radio_power", buf, sizeof(buf), NULL) == 0 && buf[0])
        beacon_config.radio_power_dbm = (int8_t)atoi(buf);
    if (uci_app_get_string("ble", "ibeacon", "interval", buf, sizeof(buf), NULL) == 0 && buf[0])
        beacon_config.interval_ms = (uint16_t)atoi(buf);

    ble_uuid_parse(beacon_config.uuid, beacon_config.uuid_bytes);
    ble_subscribe(ibeacon_event_cb, NULL);

    /* Autostart if configured */
    autostart = uci_app_get_bool("ble", "ibeacon", "enabled", false);

    syslog(LOG_INFO, "ibeacon app initialized (autostart=%s uuid=%s major=%u minor=%u)",
           autostart ? "yes" : "no", beacon_config.uuid,
           beacon_config.major, beacon_config.minor);
    return 0;
}

static void ibeacon_deinit(void)
{
    ble_unsubscribe(ibeacon_event_cb);
    if (ble_beacon_is_active())
        ble_beacon_stop();
    syslog(LOG_INFO, "ibeacon app deinitialized");
}

static int ibeacon_start(void)
{
    ble_uuid_parse(beacon_config.uuid, beacon_config.uuid_bytes);
    int ret = ble_beacon_start(&beacon_config);

    /* On BlueZ transport, also register LE advertisement via D-Bus */
    const char *transport = ble_get_transport_name();
    if (transport && strcmp(transport, "bluez") == 0) {
        extern int ble_adv_start_ibeacon(const uint8_t uuid_bytes[16],
                                         uint16_t major, uint16_t minor, int8_t tx_power);
        int adv_ret = ble_adv_start_ibeacon(beacon_config.uuid_bytes,
                                            beacon_config.major,
                                            beacon_config.minor,
                                            beacon_config.tx_power);
        if (adv_ret < 0)
            syslog(LOG_WARNING, "ibeacon: LE advertising registration failed: %d", adv_ret);
    }

    return ret;
}

static int ibeacon_stop(void)
{
    /* On BlueZ, stop LE advertisement */
    const char *transport = ble_get_transport_name();
    if (transport && strcmp(transport, "bluez") == 0) {
        extern void ble_adv_stop(int instance_id);
        ble_adv_stop(2);  /* iBeacon uses instance 2 (after GATT's instance 1) */
    }

    return ble_beacon_stop();
}

bool ibeacon_autostart_enabled(void)
{
    return autostart;
}

app_plugin_t ibeacon_app_plugin = {
    .name   = "ibeacon",
    .init   = ibeacon_init,
    .deinit = ibeacon_deinit,
    .start  = ibeacon_start,
    .stop   = ibeacon_stop,
    .running = false,
};
