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
    bool active;                          /* slot is in use */
    /*
     * True while BlueZ actually holds this advertisement. Tracked separately from
     * `active` because a re-register can fail: the slot stays in use (so the id
     * the caller holds keeps working and a retry is possible) while BlueZ has
     * nothing registered.
     */
    bool registered;
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
 *
 * @param local_name      Device name to advertise.
 * @param mfr_company_id  Manufacturer company id, or 0 for none.
 * @param mfr_data        Manufacturer payload, or NULL for none.
 * @param mfr_data_len    Length of @mfr_data, 0 for none.
 * @return instance id (>0) on success, negative on error.
 *
 * The phone app scans by manufacturer data rather than by service UUID, so the
 * v2 provisioning advertisement carries both a local name and four status bytes
 * (§19.6). ServiceUUIDs are deliberately absent — including them would not leave
 * room for the Complete Local Name AD inside the 31-byte limit.
 */
int ble_adv_start_connectable(const char *local_name,
                              uint16_t mfr_company_id,
                              const uint8_t *mfr_data, uint8_t mfr_data_len);

/**
 * Replace the manufacturer payload of a running advertisement.
 *
 * BlueZ reads the LEAdvertisement1 properties only while registering, so the new
 * bytes are published by unregistering and registering again. This restarts the
 * advertising set, which is why the caller should only call this when the bytes
 * actually changed.
 *
 * @param instance_id  id returned by ble_adv_start_connectable().
 * @param data         new payload.
 * @param len          payload length.
 * @return 0 when already up to date (nothing done), 1 when re-registered,
 *         negative errno on failure.
 */
int ble_adv_update_mfr_data(int instance_id, const uint8_t *data, uint8_t len);

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
 * Stop the iBeacon advertisement, whichever slot it is in.
 *
 * Use this instead of ble_adv_stop() with a literal id. The slot the beacon
 * gets depends on whether the GATT advertisement took one first, so it is 2
 * with provisioning enabled and 1 without — and a hardcoded 2 silently leaked
 * the slot in the second case. See the comment on the definition.
 */
void ble_adv_stop_ibeacon(void);

/**
 * Stop all active advertisements.
 */
void ble_adv_stop_all(void);

#endif /* BLE_ADVERTISE_H */
