/**
 * @file uci_app_config.h
 * @brief Application configuration structures and loader for /etc/config/ble.
 *
 * UCI package: ble
 * Sections: ibeacon, scan, gatt, daemon
 */

#ifndef UCI_APP_CONFIG_H
#define UCI_APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool enabled;
    char uuid[33];          /* 32 hex chars + null */
    uint16_t major;
    uint16_t minor;
    int8_t txpower;         /* measured power at 1m (dBm) */
    int8_t radio_power;     /* actual radio TX power, 127=default */
    uint16_t interval_ms;
} ble_ibeacon_config_t;

typedef struct {
    bool enabled;
    uint32_t duration_ms;
    bool active;
    bool filter_dup;
    int8_t rssi_threshold;
    uint32_t max_results;
} ble_scan_config_t;

typedef struct {
    bool enabled;
    char target_address[18];
    char target_addr_type[8];
    bool auto_discover;
    uint32_t conn_timeout_ms;
} ble_gatt_config_t;

typedef struct {
    char log_level[8];     /* "error","warn","info","debug" */
    bool ubus_enabled;
    bool respawn;
} ble_daemon_config_t;

typedef struct {
    ble_ibeacon_config_t ibeacon;
    ble_scan_config_t scan;
    ble_gatt_config_t gatt;
    ble_daemon_config_t daemon;
} ble_app_config_t;

/**
 * Load application configuration from /etc/config/ble.
 * Reads sections: ibeacon, scan, gatt, daemon
 *
 * @param app_cfg  Output configuration structure (zeroed and filled with defaults
 *                 before reading UCI, so missing options use sane defaults).
 * @return 0 on success (including when UCI file is missing — defaults are used),
 *         -EINVAL if app_cfg is NULL.
 */
int uci_app_config_load(ble_app_config_t *app_cfg);

/**
 * Convenience: read a boolean option from /etc/config/ble.
 * Returns default_val if option not found.
 */
bool uci_app_get_bool(const char *pkg, const char *section,
                      const char *option, bool default_val);

/**
 * Convenience: read a string option from /etc/config/ble.
 * Copies into buf (with null termination). Returns 0 on success.
 */
int uci_app_get_string(const char *pkg, const char *section,
                       const char *option, char *buf, size_t buf_size,
                       const char *default_val);

#endif /* UCI_APP_CONFIG_H */
