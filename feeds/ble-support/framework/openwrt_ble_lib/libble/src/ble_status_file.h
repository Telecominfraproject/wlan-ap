/**
 * @file ble_status_file.h
 * @brief Runtime status file API for init info.
 */
#ifndef BLE_STATUS_FILE_H
#define BLE_STATUS_FILE_H

#include "ble_core_internal.h"
#include <stdbool.h>

/** Status file path constant */
#define BLE_STATUS_FILE_PATH    "/var/run/ble-provision.status"
#define BLE_PID_FILE_PATH       "/var/run/ble-provision.pid"

/**
 * Write initialization status to /var/run/ble-provision.status.
 * Called after init completes (success or failure).
 */
void ble_status_write(libble_ctx_t *ctx, bool success, const char *error_msg);

/**
 * Remove status and PID files on shutdown.
 */
void ble_status_cleanup(void);

#endif /* BLE_STATUS_FILE_H */
