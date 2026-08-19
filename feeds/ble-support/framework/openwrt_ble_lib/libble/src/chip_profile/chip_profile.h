/**
 * @file chip_profile.h
 * @brief Chip profile interface for UART transport (internal to libble).
 *
 * Chip profiles translate between unified BLE API calls and
 * chip-specific wire protocols (HCI H4, TI NPI, JSON).
 */
#ifndef LIBBLE_CHIP_PROFILE_H
#define LIBBLE_CHIP_PROFILE_H

#include "../../include/ble.h"
#include "../ble_types.h"
#include <stdint.h>
#include <stdbool.h>

/** Serialized command buffer */
typedef struct {
    uint8_t data[512];
    uint16_t len;
} chip_cmd_buf_t;

/**
 * @brief Chip profile operations.
 */
typedef struct chip_profile {
    const char *name;

    int (*init)(int (*send_fn)(const uint8_t *data, uint16_t len, void *ctx),
                void *ctx);

    int (*build_scan_start)(uint32_t duration_ms, bool active,
                            bool filter_dup, chip_cmd_buf_t *out);
    int (*build_scan_stop)(chip_cmd_buf_t *out);

    int (*build_beacon_start)(const ble_beacon_config_t *config,
                              chip_cmd_buf_t *out);
    int (*build_beacon_stop)(chip_cmd_buf_t *out);

    int (*build_gatt_connect)(const char *address, uint8_t addr_type,
                              chip_cmd_buf_t *out);
    int (*build_gatt_disconnect)(uint16_t conn_handle, chip_cmd_buf_t *out);
    int (*build_gatt_discover)(uint16_t conn_handle, chip_cmd_buf_t *out);
    int (*build_gatt_read)(uint16_t conn_handle, uint16_t char_handle,
                           chip_cmd_buf_t *out);
    int (*build_gatt_write)(uint16_t conn_handle, uint16_t char_handle,
                            const uint8_t *data, uint16_t data_len,
                            chip_cmd_buf_t *out);
    int (*build_gatt_subscribe)(uint16_t conn_handle, uint16_t cccd_handle,
                                bool enable, chip_cmd_buf_t *out);

    int (*build_hci_cmd)(const ble_hci_cmd_t *cmd, chip_cmd_buf_t *out);
    int (*build_vendor_cmd)(uint8_t ogf, uint16_t ocf,
                            const uint8_t *params, uint8_t param_len,
                            chip_cmd_buf_t *out);

    /**
     * Build command to set the radio TX power.
     * @param power_dbm Requested power in dBm (already mapped to nearest level).
     * @param out       Output command buffer.
     * @return 0 on success, negative errno on failure.
     */
    int (*set_radio_tx_power)(int8_t power_dbm, chip_cmd_buf_t *out);

    /**
     * Get supported TX power range for this chip profile.
     * @param min_dbm   Output: minimum supported dBm.
     * @param max_dbm   Output: maximum supported dBm.
     * @return 0 on success, -ENOSYS if not supported.
     */
    int (*get_radio_power_range)(int8_t *min_dbm, int8_t *max_dbm);

    int (*parse_rx)(const uint8_t *data, uint16_t len,
                    void (*event_cb)(const ble_event_t *event, void *ctx),
                    void *ctx);

    int (*probe)(int (*send_fn)(const uint8_t *data, uint16_t len, void *ctx),
                 int (*recv_fn)(uint8_t *buf, uint16_t max_len,
                                uint32_t timeout_ms, void *ctx),
                 void *ctx);
} chip_profile_t;

/* Built-in chip profiles */
extern chip_profile_t chip_profile_hci_h4;
extern chip_profile_t chip_profile_ti_npi;
extern chip_profile_t chip_profile_json;

#endif /* LIBBLE_CHIP_PROFILE_H */
