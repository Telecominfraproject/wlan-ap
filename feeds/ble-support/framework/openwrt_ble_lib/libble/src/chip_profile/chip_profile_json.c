/**
 * @file chip_profile_json.c
 * @brief Chip profile for JSON-over-UART protocol.
 *
 * Supports BLE controllers with custom JSON firmware.
 * Wire: {"cmd":"scan_start","id":1,...}\n
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "chip_profile.h"
#include "../log.h"

static uint16_t s_next_id = 1;
static char s_rx_buf[2048];
static uint16_t s_rx_len = 0;

static uint16_t next_id(void)
{
    uint16_t id = s_next_id;
    s_next_id = (s_next_id >= 65535) ? 1 : s_next_id + 1;
    return id;
}

static int json_init(int (*send_fn)(const uint8_t *, uint16_t, void *),
                     void *ctx)
{
    (void)send_fn; (void)ctx;
    s_next_id = 1;
    s_rx_len = 0;
    return 0;
}

static int json_build_scan_start(uint32_t duration_ms, bool active,
                                 bool filter_dup, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"scan_start\",\"id\":%u,\"duration_ms\":%u,"
        "\"active\":%s,\"filter_duplicates\":%s}\n",
        next_id(), (unsigned)duration_ms,
        active ? "true" : "false", filter_dup ? "true" : "false");
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_scan_stop(chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"scan_stop\",\"id\":%u}\n", next_id());
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_beacon_start(const ble_beacon_config_t *config,
                                   chip_cmd_buf_t *out)
{
    if (!config || !out) return -EINVAL;
    int n;
    if (config->radio_power_dbm != BLE_RADIO_POWER_DEFAULT) {
        n = snprintf((char *)out->data, sizeof(out->data),
            "{\"cmd\":\"beacon_start\",\"id\":%u,"
            "\"uuid\":\"%s\",\"major\":%u,\"minor\":%u,"
            "\"tx_power\":%d,\"radio_power\":%d}\n",
            next_id(), config->uuid, config->major, config->minor,
            config->tx_power, config->radio_power_dbm);
    } else {
        n = snprintf((char *)out->data, sizeof(out->data),
            "{\"cmd\":\"beacon_start\",\"id\":%u,"
            "\"uuid\":\"%s\",\"major\":%u,\"minor\":%u,\"tx_power\":%d}\n",
            next_id(), config->uuid, config->major, config->minor,
            config->tx_power);
    }
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_beacon_stop(chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"beacon_stop\",\"id\":%u}\n", next_id());
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_gatt_connect(const char *address, uint8_t addr_type,
                                   chip_cmd_buf_t *out)
{
    if (!address || !out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"gatt_connect\",\"id\":%u,\"address\":\"%s\","
        "\"addr_type\":%u}\n", next_id(), address, addr_type);
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_gatt_disconnect(uint16_t conn_handle, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"gatt_disconnect\",\"id\":%u,\"conn_handle\":%u}\n",
        next_id(), conn_handle);
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_gatt_discover(uint16_t conn_handle, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"gatt_discover\",\"id\":%u,\"conn_handle\":%u}\n",
        next_id(), conn_handle);
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_gatt_read(uint16_t conn_handle, uint16_t char_handle,
                                chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"gatt_read\",\"id\":%u,\"conn_handle\":%u,"
        "\"char_handle\":%u}\n", next_id(), conn_handle, char_handle);
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_gatt_write(uint16_t conn_handle, uint16_t char_handle,
                                 const uint8_t *data, uint16_t data_len,
                                 chip_cmd_buf_t *out)
{
    if (!out || !data) return -EINVAL;
    /* Encode data as hex string */
    char hex[BLE_GATT_VALUE_MAX * 2 + 1];
    for (uint16_t i = 0; i < data_len && i < BLE_GATT_VALUE_MAX; i++)
        snprintf(&hex[i * 2], 3, "%02x", data[i]);
    hex[data_len * 2] = '\0';

    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"gatt_write\",\"id\":%u,\"conn_handle\":%u,"
        "\"char_handle\":%u,\"value\":\"%s\"}\n",
        next_id(), conn_handle, char_handle, hex);
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_gatt_subscribe(uint16_t conn_handle, uint16_t cccd_handle,
                                     bool enable, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"gatt_subscribe\",\"id\":%u,\"conn_handle\":%u,"
        "\"cccd_handle\":%u,\"enable\":%s}\n",
        next_id(), conn_handle, cccd_handle, enable ? "true" : "false");
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_build_hci_cmd(const ble_hci_cmd_t *cmd, chip_cmd_buf_t *out)
{
    (void)cmd; (void)out;
    return -ENOSYS;
}

static int json_build_vendor_cmd(uint8_t ogf, uint16_t ocf,
                                 const uint8_t *params, uint8_t param_len,
                                 chip_cmd_buf_t *out)
{
    (void)ogf; (void)ocf; (void)params; (void)param_len; (void)out;
    return -ENOSYS;
}

/* ── Radio TX Power (JSON protocol) ── */

static int json_set_radio_tx_power(int8_t power_dbm, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    int n = snprintf((char *)out->data, sizeof(out->data),
        "{\"cmd\":\"set_tx_power\",\"id\":%u,\"power_dbm\":%d}\n",
        next_id(), (int)power_dbm);
    if (n < 0 || n >= (int)sizeof(out->data)) return -ENOMEM;
    out->len = (uint16_t)n;
    return 0;
}

static int json_get_radio_power_range(int8_t *min_dbm, int8_t *max_dbm)
{
    if (!min_dbm || !max_dbm) return -EINVAL;
    /* EFR32xG21-based JSON firmware: -30 to +20 dBm */
    *min_dbm = -30;
    *max_dbm = 20;
    return 0;
}

/* Simple JSON key finder */
static const char *json_find_str(const char *json, const char *key,
                                 char *buf, size_t buf_len)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    const char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    if (len >= buf_len) len = buf_len - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

static int json_find_int(const char *json, const char *key, int *val)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    *val = atoi(p);
    return 0;
}

static int json_parse_line(const char *line,
                           void (*event_cb)(const ble_event_t *, void *),
                           void *ctx)
{
    ble_event_t event;
    memset(&event, 0, sizeof(event));
    char str_buf[128];

    if (json_find_str(line, "event", str_buf, sizeof(str_buf))) {
        if (strcmp(str_buf, "scan_result") == 0) {
            event.type = BLE_EVT_SCAN_RESULT;
            json_find_str(line, "addr", event.scan_result.address,
                         sizeof(event.scan_result.address));
            json_find_str(line, "addr_type", event.scan_result.address_type,
                         sizeof(event.scan_result.address_type));
            int rssi = 0;
            if (json_find_int(line, "rssi", &rssi) == 0)
                event.scan_result.rssi = (int8_t)rssi;
            json_find_str(line, "name", event.scan_result.name,
                         sizeof(event.scan_result.name));
            if (event_cb) event_cb(&event, ctx);
            return 1;
        } else if (strcmp(str_buf, "scan_complete") == 0) {
            event.type = BLE_EVT_SCAN_COMPLETE;
            int count = 0;
            json_find_int(line, "count", &count);
            event.scan_complete.count = (uint16_t)count;
            if (event_cb) event_cb(&event, ctx);
            return 1;
        } else if (strcmp(str_buf, "connected") == 0) {
            event.type = BLE_EVT_CONNECT;
            json_find_str(line, "addr", event.connect.address,
                         sizeof(event.connect.address));
            if (event_cb) event_cb(&event, ctx);
            return 1;
        } else if (strcmp(str_buf, "error") == 0) {
            event.type = BLE_EVT_ERROR;
            int code = 0;
            json_find_int(line, "code", &code);
            event.error.code = (uint8_t)code;
            json_find_str(line, "message", event.error.message,
                         sizeof(event.error.message));
            if (event_cb) event_cb(&event, ctx);
            return 1;
        }
    }
    return 0;
}

static int json_parse_rx(const uint8_t *data, uint16_t len,
                         void (*event_cb)(const ble_event_t *, void *),
                         void *ctx)
{
    int events = 0;
    for (uint16_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\n' || c == '\r') {
            if (s_rx_len > 0) {
                s_rx_buf[s_rx_len] = '\0';
                events += json_parse_line(s_rx_buf, event_cb, ctx);
                s_rx_len = 0;
            }
        } else if (s_rx_len < sizeof(s_rx_buf) - 1) {
            s_rx_buf[s_rx_len++] = c;
        } else {
            s_rx_len = 0;
        }
    }
    return events;
}

static int json_probe(int (*send_fn)(const uint8_t *, uint16_t, void *),
                      int (*recv_fn)(uint8_t *, uint16_t, uint32_t, void *),
                      void *ctx)
{
    const char *cmd = "{\"cmd\":\"get_status\",\"id\":0}\n";
    if (send_fn((const uint8_t *)cmd, (uint16_t)strlen(cmd), ctx) < 0) return 0;
    uint8_t buf[256];
    int ret = recv_fn(buf, sizeof(buf) - 1, 500, ctx);
    if (ret <= 0) return 0;
    buf[ret] = '\0';
    return (strstr((char *)buf, "\"status\"") != NULL) ? 1 : 0;
}

chip_profile_t chip_profile_json = {
    .name              = "json",
    .init              = json_init,
    .build_scan_start  = json_build_scan_start,
    .build_scan_stop   = json_build_scan_stop,
    .build_beacon_start = json_build_beacon_start,
    .build_beacon_stop = json_build_beacon_stop,
    .build_gatt_connect = json_build_gatt_connect,
    .build_gatt_disconnect = json_build_gatt_disconnect,
    .build_gatt_discover = json_build_gatt_discover,
    .build_gatt_read   = json_build_gatt_read,
    .build_gatt_write  = json_build_gatt_write,
    .build_gatt_subscribe = json_build_gatt_subscribe,
    .build_hci_cmd     = json_build_hci_cmd,
    .build_vendor_cmd  = json_build_vendor_cmd,
    .set_radio_tx_power = json_set_radio_tx_power,
    .get_radio_power_range = json_get_radio_power_range,
    .parse_rx          = json_parse_rx,
    .probe             = json_probe,
};
