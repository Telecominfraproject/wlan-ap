/**
 * @file ble_advertise.h
 * @brief BLE LE Advertising via BlueZ LEAdvertisingManager1 D-Bus API.
 *
 * Supports multiple concurrent advertisement instances:
 *   - GATT Server: connectable, with local name (type "peripheral")
 *   - iBeacon: non-connectable, with manufacturer data (type "broadcast")
 *
 * Uses libdbus to export LEAdvertisement1 objects and register them
 * with BlueZ. Works correctly when bluetoothd is running.
 */
#ifndef BLE_ADVERTISE_H
#define BLE_ADVERTISE_H

#include <stdint.h>
#include <stdbool.h>
#include <dbus/dbus.h>

#define BLE_ADV_MAX_INSTANCES   4
#define BLE_ADV_MFR_DATA_MAX    28
#define BLE_ADV_NAME_MAX        32

typedef enum {
    BLE_ADV_TYPE_PERIPHERAL = 0,  /* connectable, for GATT server */
    BLE_ADV_TYPE_BROADCAST,       /* non-connectable, for iBeacon */
} ble_adv_type_t;

typedef struct {
    uint8_t id;                           /* instance id (1-based) */
    bool active;
    ble_adv_type_t type;
    char local_name[BLE_ADV_NAME_MAX];    /* only for peripheral type */
    bool include_tx_power;
    /* Manufacturer data (for iBeacon) */
    uint16_t mfr_company_id;              /* 0x004C for Apple */
    uint8_t mfr_data[BLE_ADV_MFR_DATA_MAX];
    uint8_t mfr_data_len;
    /* D-Bus path */
    char path[64];
} ble_adv_instance_t;

/**
 * Initialize the advertising module. Must be called with an active D-Bus connection.
 */
int ble_adv_init(DBusConnection *conn, const char *adapter_path);

/**
 * Cleanup advertising module.
 */
void ble_adv_deinit(void);

/**
 * Register a connectable advertisement (for GATT server).
 * @param local_name  Device name to advertise.
 * @return instance id (>0) on success, negative on error.
 */
int ble_adv_start_connectable(const char *local_name);

/**
 * Register a non-connectable iBeacon advertisement.
 * @param uuid_bytes  16-byte iBeacon UUID.
 * @param major       iBeacon major value.
 * @param minor       iBeacon minor value.
 * @param tx_power    Measured power at 1m (dBm).
 * @return instance id (>0) on success, negative on error.
 */
int ble_adv_start_ibeacon(const uint8_t uuid_bytes[16],
                          uint16_t major, uint16_t minor, int8_t tx_power);

/**
 * Stop and unregister an advertisement instance.
 * @param instance_id  The id returned by start functions.
 */
void ble_adv_stop(int instance_id);

/**
 * Stop all active advertisements.
 */
void ble_adv_stop_all(void);

#endif /* BLE_ADVERTISE_H */
