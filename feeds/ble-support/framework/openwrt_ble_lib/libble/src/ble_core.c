/**
 * @file ble_core.c
 * @brief libble core implementation — public API.
 *
 * Implements all ble_*() functions declared in ble.h.
 * Manages transport plugin registry, chip profile dispatch,
 * event subscriber list, and library lifecycle.
 *
 * Compiled with -fvisibility=hidden; only symbols with
 * __attribute__((visibility("default"))) are exported.
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include "ble_core_internal.h"
#include "ble_status_file.h"
#include "log.h"

#define EXPORT __attribute__((visibility("default")))

/* ── Singleton context ── */
static libble_ctx_t g_ctx;

libble_ctx_t *libble_get_ctx(void)
{
    return &g_ctx;
}

/* ── Error strings ── */
static const char *err_strings[] = {
    [0] = "Success",
    [1] = "Invalid argument",
    [2] = "Out of memory",
    [3] = "No device",
    [4] = "Busy",
    [5] = "Timeout",
    [6] = "I/O error",
    [7] = "Not supported",
    [8] = "Already active",
    [9] = "Not connected",
};

/* ── Event dispatch ── */
void libble_dispatch_event(const ble_event_t *event)
{
    if (!event) return;
    for (int i = 0; i < g_ctx.subscriber_count; i++) {
        if (g_ctx.subscribers[i].cb) {
            g_ctx.subscribers[i].cb(event, g_ctx.subscribers[i].user_data);
        }
    }
}

/* ── Transport event bridge ── */
void libble_transport_event_cb(const ble_event_t *event, void *ctx)
{
    (void)ctx;
    if (!event) return;

    /* Update internal state based on event type */
    switch (event->type) {
    case BLE_EVT_SCAN_COMPLETE:
        g_ctx.scan_active = false;
        g_ctx.state = BLE_STATE_IDLE;
        break;
    case BLE_EVT_CONNECT:
        g_ctx.gatt_connected = true;
        g_ctx.state = BLE_STATE_CONNECTED;
        break;
    case BLE_EVT_DISCONNECT:
        g_ctx.gatt_connected = false;
        g_ctx.state = BLE_STATE_IDLE;
        break;
    default:
        break;
    }

    libble_dispatch_event(event);
}

/* ── Transport activation with auto-detect ── */
static int activate_transport(void)
{
    const ble_config_t *cfg = &g_ctx.config;
    const char *tp_config;
    int ret;

    if (cfg->transport == BLE_TRANSPORT_AUTO) {
        BLE_LOG_INFO("Transport auto-detect: trying all registered transports");
        for (int i = 0; i < g_ctx.transport_count; i++) {
            transport_plugin_t *tp = g_ctx.transports[i];
            if (strcmp(tp->name, "bluez") == 0)
                tp_config = cfg->adapter[0] ? cfg->adapter : "hci0";
            else
                tp_config = cfg->device_path[0] ? cfg->device_path : "/dev/ttyUSB0";

            ret = tp->init(tp_config);
            if (ret == 0) {
                g_ctx.active_transport_idx = i;
                tp->active = true;
                BLE_LOG_INFO("Transport '%s' activated", tp->name);
                return BLE_OK;
            }
            BLE_LOG_DBG("Transport '%s' failed (%d), next...", tp->name, ret);
        }
        BLE_LOG_ERR("No transport initialized successfully");
        return BLE_ERR_NODEV;
    }

    /* Specific transport requested */
    int target = -1;
    for (int i = 0; i < g_ctx.transport_count; i++) {
        if ((cfg->transport == BLE_TRANSPORT_BLUEZ &&
             strcmp(g_ctx.transports[i]->name, "bluez") == 0) ||
            (cfg->transport == BLE_TRANSPORT_UART &&
             strcmp(g_ctx.transports[i]->name, "uart_hci") == 0)) {
            target = i;
            break;
        }
    }

    if (target < 0) {
        BLE_LOG_ERR("Requested transport not registered");
        return BLE_ERR_NODEV;
    }

    transport_plugin_t *tp = g_ctx.transports[target];
    tp_config = (cfg->transport == BLE_TRANSPORT_BLUEZ)
        ? (cfg->adapter[0] ? cfg->adapter : "hci0")
        : (cfg->device_path[0] ? cfg->device_path : "/dev/ttyUSB0");

    ret = tp->init(tp_config);
    if (ret < 0) {
        BLE_LOG_ERR("Transport '%s' init failed: %d", tp->name, ret);
        return BLE_ERR_NODEV;
    }

    g_ctx.active_transport_idx = target;
    tp->active = true;
    BLE_LOG_INFO("Transport '%s' activated", tp->name);
    return BLE_OK;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Lifecycle
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_init(const ble_config_t *config)
{
    if (g_ctx.initialized)
        return BLE_ERR_ALREADY;

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.active_transport_idx = -1;
    g_ctx.active_profile_idx = -1;
    g_ctx.state = BLE_STATE_OFF;

    if (config)
        memcpy(&g_ctx.config, config, sizeof(ble_config_t));

    /* Register built-in transports */
    g_ctx.transports[g_ctx.transport_count++] = &bluez_transport_plugin;
    g_ctx.transports[g_ctx.transport_count++] = &uart_transport_plugin;

    /* Register built-in chip profiles */
    g_ctx.profiles[g_ctx.profile_count++] = &chip_profile_hci_h4;
    g_ctx.profiles[g_ctx.profile_count++] = &chip_profile_ti_npi;
    g_ctx.profiles[g_ctx.profile_count++] = &chip_profile_json;

    /* Activate transport */
    int ret = activate_transport();
    if (ret != BLE_OK) {
        ble_status_write(&g_ctx, false, "transport activation failed");
        return ret;
    }

    /* Register internal event callback with transport */
    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
    tp->register_event_callback(libble_transport_event_cb, &g_ctx);

    /* Select chip profile if UART transport */
    if (g_ctx.config.transport == BLE_TRANSPORT_UART ||
        (g_ctx.config.transport == BLE_TRANSPORT_AUTO &&
         strcmp(tp->name, "uart_hci") == 0)) {
        if (g_ctx.config.chip_profile == BLE_CHIP_AUTO) {
            g_ctx.active_profile_idx = 0; /* default to HCI H4 */
        } else {
            g_ctx.active_profile_idx = (int)g_ctx.config.chip_profile - 1;
        }
    }

    g_ctx.initialized = true;
    g_ctx.state = BLE_STATE_IDLE;

    /* Write runtime status file */
    ble_status_write(&g_ctx, true, NULL);

    BLE_LOG_INFO("libble initialized (transport=%s)", tp->name);
    return BLE_OK;
}

EXPORT int ble_init_from_uci(const char *uci_path)
{
    /* Parse UCI and fill ble_config_t, then call ble_init() */
    ble_config_t config;
    memset(&config, 0, sizeof(config));
    config.transport = BLE_TRANSPORT_AUTO;
    config.chip_profile = BLE_CHIP_AUTO;
    config.baud_rate = 115200;
    strncpy(config.adapter, "hci0", sizeof(config.adapter) - 1);

    (void)uci_path; /* UCI parsing done in ble-provisiond; stub for library */
    /* Applications should use ble_init() directly or rely on daemon */

    return ble_init(&config);
}

EXPORT void ble_deinit(void)
{
    if (!g_ctx.initialized)
        return;

    /* Stop active operations */
    if (g_ctx.scan_active)
        ble_scan_stop();
    if (g_ctx.beacon_active)
        ble_beacon_stop();

    /* Deinit active transport */
    if (g_ctx.active_transport_idx >= 0) {
        transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
        if (tp->active) {
            tp->deinit();
            tp->active = false;
        }
    }

    g_ctx.initialized = false;
    g_ctx.state = BLE_STATE_OFF;

    /* Remove runtime status/PID files */
    ble_status_cleanup();

    BLE_LOG_INFO("libble deinitialized");
}

EXPORT int ble_get_fd(void)
{
    if (!g_ctx.initialized || g_ctx.active_transport_idx < 0)
        return -1;
    return g_ctx.transports[g_ctx.active_transport_idx]->get_fd();
}

EXPORT int ble_process(void)
{
    if (!g_ctx.initialized || g_ctx.active_transport_idx < 0)
        return BLE_ERR_NODEV;
    return g_ctx.transports[g_ctx.active_transport_idx]->process_events();
}

EXPORT int ble_subscribe(ble_event_cb_t cb, void *user_data)
{
    if (!cb)
        return BLE_ERR_INVAL;
    if (g_ctx.subscriber_count >= SUBSCRIBER_MAX)
        return BLE_ERR_NOMEM;

    g_ctx.subscribers[g_ctx.subscriber_count].cb = cb;
    g_ctx.subscribers[g_ctx.subscriber_count].user_data = user_data;
    g_ctx.subscriber_count++;
    return BLE_OK;
}

EXPORT int ble_unsubscribe(ble_event_cb_t cb)
{
    for (int i = 0; i < g_ctx.subscriber_count; i++) {
        if (g_ctx.subscribers[i].cb == cb) {
            int remaining = g_ctx.subscriber_count - i - 1;
            if (remaining > 0)
                memmove(&g_ctx.subscribers[i], &g_ctx.subscribers[i + 1],
                        remaining * sizeof(ble_subscriber_t));
            g_ctx.subscriber_count--;
            return BLE_OK;
        }
    }
    return BLE_ERR_INVAL;
}

EXPORT const char *ble_get_transport_name(void)
{
    if (!g_ctx.initialized || g_ctx.active_transport_idx < 0)
        return "none";
    return g_ctx.transports[g_ctx.active_transport_idx]->name;
}

EXPORT const char *ble_get_chip_profile_name(void)
{
    if (g_ctx.active_profile_idx < 0)
        return NULL;
    return g_ctx.profiles[g_ctx.active_profile_idx]->name;
}

EXPORT const char *ble_strerror(int err)
{
    int idx = (err < 0) ? -err : err;
    if (idx >= 0 && idx <= 9)
        return err_strings[idx];
    return "Unknown error";
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Scan
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_scan_start(uint32_t duration_ms, bool active, bool filter_dup)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (g_ctx.scan_active)
        return BLE_ERR_ALREADY;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    /* Build scan command via chip profile if UART, else direct HCI */
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        int ret = cp->build_scan_start(duration_ms, active, filter_dup, &cmd);
        if (ret < 0) return BLE_ERR_IO;
        /* Send via transport raw write (as HCI cmd wrapper) */
        ble_hci_cmd_t hci = { .opcode = 0x200C, .param_len = 0 };
        /* For chip-profiled transports, use the built command directly */
        (void)hci;
        /* Write raw bytes — transport must support this path */
        ssize_t w = write(tp->get_fd(), cmd.data, cmd.len);
        if (w < 0) return BLE_ERR_IO;
    } else {
        /* BlueZ transport — use HCI abstraction */
        ble_hci_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        /* Set scan params */
        cmd.opcode = 0x200B;
        cmd.params[0] = active ? 0x01 : 0x00;
        cmd.params[1] = 0x60; cmd.params[2] = 0x00;
        cmd.params[3] = 0x30; cmd.params[4] = 0x00;
        cmd.params[5] = 0x00; cmd.params[6] = 0x00;
        cmd.param_len = 7;
        tp->send_hci_cmd(&cmd);

        /* Enable scan */
        memset(&cmd, 0, sizeof(cmd));
        cmd.opcode = 0x200C;
        cmd.params[0] = 0x01;
        cmd.params[1] = filter_dup ? 0x01 : 0x00;
        cmd.param_len = 2;
        int ret = tp->send_hci_cmd(&cmd);
        if (ret < 0) return BLE_ERR_IO;
    }

    g_ctx.scan_active = true;
    g_ctx.state = BLE_STATE_SCANNING;
    BLE_LOG_INFO("Scan started (duration=%ums, active=%d)", duration_ms, active);
    return BLE_OK;
}

EXPORT int ble_scan_stop(void)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!g_ctx.scan_active)
        return BLE_OK;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        int ret = cp->build_scan_stop(&cmd);
        if (ret < 0) return BLE_ERR_IO;
        write(tp->get_fd(), cmd.data, cmd.len);
    } else {
        ble_hci_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.opcode = 0x200C;
        cmd.params[0] = 0x00;
        cmd.params[1] = 0x00;
        cmd.param_len = 2;
        tp->send_hci_cmd(&cmd);
    }

    g_ctx.scan_active = false;
    g_ctx.state = BLE_STATE_IDLE;
    BLE_LOG_INFO("Scan stopped");
    return BLE_OK;
}

EXPORT bool ble_scan_is_active(void)
{
    return g_ctx.scan_active;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Beacon
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_beacon_start(const ble_beacon_config_t *config)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!config)
        return BLE_ERR_INVAL;
    if (g_ctx.beacon_active)
        return BLE_ERR_ALREADY;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    /* Set radio TX power if explicitly configured */
    if (config->radio_power_dbm != BLE_RADIO_POWER_DEFAULT) {
        int ret = ble_set_radio_tx_power(config->radio_power_dbm);
        if (ret < -128)
            BLE_LOG_ERR("Failed to set radio TX power: %d", ret);
        else
            BLE_LOG_INFO("Radio TX power set to %d dBm", ret);
    }

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        int ret = cp->build_beacon_start(config, &cmd);
        if (ret < 0) return BLE_ERR_IO;
        write(tp->get_fd(), cmd.data, cmd.len);
    } else {
        /*
         * BlueZ path: advertising is handled by the daemon's ble_advertise
         * module (LEAdvertisingManager1 API) at the application layer.
         * libble just marks beacon as active; actual advertising is started
         * by ibeacon_app via ble_adv_start_ibeacon() in ble-provisiond.
         */
    }

    g_ctx.beacon_active = true;
    g_ctx.state = BLE_STATE_ADVERTISING;
    BLE_LOG_INFO("Beacon started (uuid=%s major=%u minor=%u)",
             config->uuid, config->major, config->minor);
    return BLE_OK;
}

EXPORT int ble_beacon_stop(void)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!g_ctx.beacon_active)
        return BLE_OK;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        cp->build_beacon_stop(&cmd);
        write(tp->get_fd(), cmd.data, cmd.len);
    } else {
        /* BlueZ: advertising stopped by daemon's ble_advertise module */
    }

    g_ctx.beacon_active = false;
    g_ctx.state = BLE_STATE_IDLE;
    BLE_LOG_INFO("Beacon stopped");
    return BLE_OK;
}

EXPORT bool ble_beacon_is_active(void)
{
    return g_ctx.beacon_active;
}

/* ── UUID helpers ── */

EXPORT int ble_uuid_parse(const char *uuid_str, uint8_t out_bytes[16])
{
    if (!uuid_str || strlen(uuid_str) < 36)
        return BLE_ERR_INVAL;

    int idx = 0;
    for (int i = 0; i < 36 && idx < 16; i++) {
        if (uuid_str[i] == '-') continue;
        char hex[3] = { uuid_str[i], uuid_str[i + 1], '\0' };
        char *endp;
        unsigned long val = strtoul(hex, &endp, 16);
        if (*endp != '\0') return BLE_ERR_INVAL;
        out_bytes[idx++] = (uint8_t)val;
        i++;
    }
    return (idx == 16) ? BLE_OK : BLE_ERR_INVAL;
}

EXPORT int ble_uuid_format(const uint8_t bytes[16], char out_str[BLE_UUID_STR_LEN])
{
    if (!bytes || !out_str)
        return BLE_ERR_INVAL;
    snprintf(out_str, BLE_UUID_STR_LEN,
             "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);
    return BLE_OK;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — GATT
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_gatt_connect(const char *address, uint8_t addr_type)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!address)
        return BLE_ERR_INVAL;
    if (g_ctx.gatt_connected)
        return BLE_ERR_BUSY;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_connect) {
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_connect(address, addr_type, &cmd);
            if (ret == -ENOSYS) return BLE_ERR_NOSYS;
            if (ret < 0) return BLE_ERR_IO;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
        return BLE_ERR_NOSYS;
    }

    /* BlueZ: LE Create Connection HCI */
    ble_hci_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = 0x200D;
    cmd.params[0] = 0x60; cmd.params[1] = 0x00;
    cmd.params[2] = 0x30; cmd.params[3] = 0x00;
    cmd.params[4] = 0x00;
    cmd.params[5] = addr_type;
    /* Parse address bytes */
    unsigned int a[6];
    if (sscanf(address, "%02x:%02x:%02x:%02x:%02x:%02x",
               &a[5], &a[4], &a[3], &a[2], &a[1], &a[0]) != 6)
        return BLE_ERR_INVAL;
    for (int i = 0; i < 6; i++) cmd.params[6 + i] = (uint8_t)a[i];
    cmd.params[12] = 0x18; cmd.params[13] = 0x00;
    cmd.params[14] = 0x28; cmd.params[15] = 0x00;
    cmd.params[16] = 0x00; cmd.params[17] = 0x00;
    cmd.params[18] = 0xC8; cmd.params[19] = 0x00;
    cmd.params[20] = 0x00; cmd.params[21] = 0x00;
    cmd.params[22] = 0x00; cmd.params[23] = 0x00;
    cmd.param_len = 25;
    int ret = tp->send_hci_cmd(&cmd);
    return (ret < 0) ? BLE_ERR_IO : BLE_OK;
}

EXPORT int ble_gatt_disconnect(uint16_t conn_handle)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_disconnect) {
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_disconnect(conn_handle, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }

    ble_hci_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = 0x0406;
    cmd.params[0] = conn_handle & 0xFF;
    cmd.params[1] = (conn_handle >> 8) & 0xFF;
    cmd.params[2] = 0x13;
    cmd.param_len = 3;
    int ret = tp->send_hci_cmd(&cmd);
    return (ret < 0) ? BLE_ERR_IO : BLE_OK;
}

EXPORT int ble_gatt_discover(uint16_t conn_handle)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_discover) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_discover(conn_handle, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

EXPORT int ble_gatt_read(uint16_t conn_handle, uint16_t char_handle)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_read) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_read(conn_handle, char_handle, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

EXPORT int ble_gatt_write(uint16_t conn_handle, uint16_t char_handle,
                          const uint8_t *data, uint16_t data_len)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (!data || data_len == 0) return BLE_ERR_INVAL;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_write) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_write(conn_handle, char_handle,
                                           data, data_len, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

EXPORT int ble_gatt_subscribe(uint16_t conn_handle, uint16_t cccd_handle,
                              bool enable)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_subscribe) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_subscribe(conn_handle, cccd_handle,
                                               enable, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Raw HCI
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_hci_send(uint16_t opcode, const uint8_t *params,
                        uint8_t param_len)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
    ble_hci_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = opcode;
    cmd.param_len = param_len;
    if (params && param_len > 0)
        memcpy(cmd.params, params, param_len);

    int ret = tp->send_hci_cmd(&cmd);
    return (ret < 0) ? BLE_ERR_NOSYS : BLE_OK;
}

EXPORT int ble_vendor_cmd(uint16_t ocf, const uint8_t *params,
                          uint8_t param_len)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
    int ret = tp->send_vendor_cmd(0x3F, ocf, params, param_len);
    return (ret < 0) ? BLE_ERR_NOSYS : BLE_OK;
}
