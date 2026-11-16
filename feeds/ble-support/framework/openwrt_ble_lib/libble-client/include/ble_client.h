/**
 * @file ble_client.h
 * @brief libble-client — High-level client library for BLE operations via ubus.
 *
 * This is a thin wrapper around ubus calls to ble-provisiond.
 * Multiple processes can use this simultaneously without conflict.
 * The daemon handles hardware multiplexing.
 *
 * Link with: -lble-client -lubus -lubox -lblobmsg_json
 */
#ifndef LIBBLE_CLIENT_H
#define LIBBLE_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_CLIENT_VERSION "1.0.0"

/* Symbol visibility for shared library */
#if defined(__GNUC__) && __GNUC__ >= 4
  #define BLE_CLIENT_API __attribute__((visibility("default")))
#else
  #define BLE_CLIENT_API
#endif

/* Error codes */
#define BLE_CLIENT_OK           0
#define BLE_CLIENT_ERR_UBUS    (-1)   /* ubus connection failed */
#define BLE_CLIENT_ERR_DAEMON  (-2)   /* daemon not running */
#define BLE_CLIENT_ERR_PARAM   (-3)   /* invalid parameter */
#define BLE_CLIENT_ERR_TIMEOUT (-4)   /* operation timed out */

/* Scan result callback */
typedef struct {
    char address[18];
    char address_type[8];
    int8_t rssi;
    char name[64];
    bool connectable;
} ble_client_scan_result_t;

typedef void (*ble_client_scan_cb_t)(const ble_client_scan_result_t *result, void *ctx);

/* GATT data callback */
typedef struct {
    uint16_t conn_handle;
    uint16_t char_handle;
    uint8_t value[512];
    uint16_t value_len;
} ble_client_gatt_data_t;

typedef void (*ble_client_gatt_cb_t)(const ble_client_gatt_data_t *data, void *ctx);

/* ── Connection ── */
BLE_CLIENT_API int ble_client_connect(void);
BLE_CLIENT_API void ble_client_disconnect(void);
BLE_CLIENT_API int ble_client_get_fd(void);
BLE_CLIENT_API int ble_client_process(void);

/* ── Scan ── */
BLE_CLIENT_API int ble_client_scan_start(uint32_t duration_ms, bool active, bool filter_dup);
BLE_CLIENT_API int ble_client_scan_stop(void);
BLE_CLIENT_API int ble_client_scan_subscribe(ble_client_scan_cb_t cb, void *ctx);

/* ── Beacon ── */
BLE_CLIENT_API int ble_client_beacon_start(const char *uuid, uint16_t major, uint16_t minor, int8_t tx_power);
BLE_CLIENT_API int ble_client_beacon_stop(void);

/* ── Radio TX Power ── */

/**
 * Set the radio TX power via the daemon.
 * Returns the actual power level set (may differ from requested due to chip mapping),
 * or a negative BLE_CLIENT_ERR_* code on failure.
 */
BLE_CLIENT_API int ble_client_set_radio_tx_power(int8_t power_dbm);

/**
 * Get the supported radio TX power range for the active chip.
 * Returns BLE_CLIENT_OK on success.
 */
BLE_CLIENT_API int ble_client_get_radio_power_range(int8_t *min_dbm, int8_t *max_dbm);

/* ── GATT ── */
BLE_CLIENT_API int ble_client_gatt_connect(const char *address);
BLE_CLIENT_API int ble_client_gatt_disconnect(uint16_t conn_handle);
BLE_CLIENT_API int ble_client_gatt_read(uint16_t conn_handle, uint16_t char_handle);
BLE_CLIENT_API int ble_client_gatt_write(uint16_t conn_handle, uint16_t char_handle,
                          const uint8_t *data, uint16_t len);
BLE_CLIENT_API int ble_client_gatt_subscribe(uint16_t conn_handle, uint16_t cccd_handle, bool enable);
BLE_CLIENT_API int ble_client_gatt_on_notify(ble_client_gatt_cb_t cb, void *ctx);

/* ── Status ── */
BLE_CLIENT_API int ble_client_get_status(char *json_buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* LIBBLE_CLIENT_H */
