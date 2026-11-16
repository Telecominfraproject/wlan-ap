/**
 * @file scan_filter.h
 * @brief BLE scan filter and beacon identification module.
 *
 * Identifies beacon types from advertising data and provides
 * filtering for scan results.
 *
 * Supported beacon types:
 *   - iBeacon (Apple, company 0x004C, prefix 02 15)
 *   - Eddystone (Google, service UUID 0xFEAA)
 *   - AltBeacon (Radius Networks, prefix BE AC)
 *   - Custom (user-defined company ID + data prefix)
 */
#ifndef SCAN_FILTER_H
#define SCAN_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <ble.h>

/* Beacon types (bitmask for combination) */
#define SCAN_FILTER_ALL           0x00  /* No filter — all devices */
#define SCAN_FILTER_ADVERTISING   0x01  /* Non-beacon devices only */
#define SCAN_FILTER_BEACON_ALL    0x02  /* All beacon types */
#define SCAN_FILTER_IBEACON       0x04  /* Apple iBeacon */
#define SCAN_FILTER_EDDYSTONE     0x08  /* Google Eddystone */
#define SCAN_FILTER_ALTBEACON     0x10  /* AltBeacon */
#define SCAN_FILTER_CUSTOM        0x20  /* Custom company ID */

/* Beacon identification result */
typedef enum {
    BEACON_TYPE_UNKNOWN = 0,
    BEACON_TYPE_IBEACON,
    BEACON_TYPE_EDDYSTONE,
    BEACON_TYPE_ALTBEACON,
    BEACON_TYPE_CUSTOM,
} beacon_type_t;

/* Parsed iBeacon data */
typedef struct {
    uint8_t uuid[16];
    char uuid_str[37];
    uint16_t major;
    uint16_t minor;
    int8_t tx_power;
} ibeacon_data_t;

/* Parsed Eddystone data */
typedef struct {
    uint8_t frame_type;  /* 0x00=UID, 0x10=URL, 0x20=TLM, 0x40=EID */
    int8_t tx_power;
    uint8_t namespace_id[10];
    uint8_t instance_id[6];
    char url[64];
} eddystone_data_t;

/* Parsed AltBeacon data */
typedef struct {
    uint8_t beacon_id[20];
    int8_t ref_rssi;
    uint8_t mfr_reserved;
} altbeacon_data_t;

/* Extended scan result with beacon info */
typedef struct {
    /* Base scan result */
    char address[18];
    char address_type[8];
    int8_t rssi;
    char name[64];
    bool connectable;
    uint8_t adv_data[62];
    uint8_t adv_data_len;
    /* Beacon identification */
    beacon_type_t beacon_type;
    union {
        ibeacon_data_t ibeacon;
        eddystone_data_t eddystone;
        altbeacon_data_t altbeacon;
    };
    /* Timestamp */
    char timestamp[32];  /* ISO 8601 */
} scan_result_ext_t;

/**
 * Identify beacon type from scan result.
 * Fills beacon-specific fields in ext_result.
 */
beacon_type_t scan_filter_identify(const ble_event_t *event,
                                   scan_result_ext_t *ext_result);

/**
 * Check if a scan result matches the given filter bitmask.
 */
bool scan_filter_match(const scan_result_ext_t *result, uint32_t filter_mask,
                       uint16_t custom_company_id);

/**
 * Parse filter string ("all", "ibeacon", "ibeacon,eddystone", etc.)
 * into a bitmask.
 */
uint32_t scan_filter_parse(const char *filter_str);

#endif /* SCAN_FILTER_H */
