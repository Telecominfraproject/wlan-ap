/**
 * @file ble.h
 * @brief libble — BLE Abstraction Layer Public API.
 *
 * A shared library providing unified BLE operations regardless of the
 * underlying transport (BlueZ D-Bus or UART to various BLE controllers).
 *
 * Link with: -lble
 * Depends on: libubox (for internal event loop)
 */
#ifndef LIBBLE_H
#define LIBBLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Symbol export macro ── */
#if defined(LIBBLE_BUILDING) && defined(__GNUC__) && __GNUC__ >= 4
  #define BLE_API __attribute__((visibility("default")))
#else
  #define BLE_API
#endif

/* Version */
#define LIBBLE_VERSION_MAJOR    1
#define LIBBLE_VERSION_MINOR    0
#define LIBBLE_VERSION_PATCH    0

/* ── Constants ── */
#define BLE_ADDR_STR_LEN    18
#define BLE_UUID_STR_LEN    37
#define BLE_ADV_DATA_MAX    62
#define BLE_GATT_VALUE_MAX  512
#define BLE_NAME_MAX        64

/* ── Error Codes ── */
#define BLE_OK              0
#define BLE_ERR_INVAL       (-1)
#define BLE_ERR_NOMEM       (-2)
#define BLE_ERR_NODEV       (-3)
#define BLE_ERR_BUSY        (-4)
#define BLE_ERR_TIMEOUT     (-5)
#define BLE_ERR_IO          (-6)
#define BLE_ERR_NOSYS       (-7)
#define BLE_ERR_ALREADY     (-8)
#define BLE_ERR_NOTCONN     (-9)

/* ── Transport Types ── */
typedef enum {
    BLE_TRANSPORT_AUTO = 0,
    BLE_TRANSPORT_BLUEZ,
    BLE_TRANSPORT_UART,
} ble_transport_t;

/* ── Chip Profile Types ── */
typedef enum {
    BLE_CHIP_AUTO = 0,
    BLE_CHIP_HCI_H4,
    BLE_CHIP_TI_NPI,
    BLE_CHIP_JSON,
} ble_chip_t;

/* ── Event Types ── */
typedef enum {
    BLE_EVT_SCAN_RESULT = 0,
    BLE_EVT_SCAN_COMPLETE,
    BLE_EVT_CONNECT,
    BLE_EVT_DISCONNECT,
    BLE_EVT_GATT_DISCOVER,
    BLE_EVT_GATT_READ,
    BLE_EVT_GATT_WRITE,
    BLE_EVT_GATT_NOTIFY,
    BLE_EVT_ERROR,
    BLE_EVT_STATE_CHANGE,
} ble_event_type_t;

/* ── Data Structures ── */

typedef struct {
    char address[BLE_ADDR_STR_LEN];
    char address_type[8];        /* "public" or "random" */
    int8_t rssi;
    char name[BLE_NAME_MAX];
    uint8_t adv_data[BLE_ADV_DATA_MAX];
    uint8_t adv_data_len;
    bool connectable;
} ble_scan_result_t;

typedef struct {
    char uuid[BLE_UUID_STR_LEN];
    uint8_t uuid_bytes[16];
    uint16_t major;
    uint16_t minor;
    int8_t tx_power;            /* Measured power at 1m (in iBeacon payload) */
    int8_t radio_power_dbm;     /* Actual radio TX power (hardware) — 0x7F = use chip default */
    uint16_t interval_ms;
} ble_beacon_config_t;

typedef struct {
    uint16_t conn_handle;
    char address[BLE_ADDR_STR_LEN];
    uint8_t status;
} ble_connect_info_t;

typedef struct {
    uint16_t conn_handle;
    uint16_t handle;
    char uuid[BLE_UUID_STR_LEN];
    uint8_t value[BLE_GATT_VALUE_MAX];
    uint16_t value_len;
} ble_gatt_data_t;

typedef struct {
    ble_event_type_t type;
    union {
        ble_scan_result_t scan_result;
        struct { uint16_t count; } scan_complete;
        ble_connect_info_t connect;
        struct { uint16_t conn_handle; uint8_t reason; } disconnect;
        ble_gatt_data_t gatt_data;
        struct { uint8_t code; char message[64]; } error;
    };
} ble_event_t;

/* ── Configuration ── */

/** BLE chip hardware descriptor (from UCI `config ble_chip`) */
typedef struct {
    bool enabled;                   /**< option enabled */
    char vendor[32];                /**< option vendor: 'nordic', 'ti', 'silabs' */
    char model[32];                 /**< option model: 'NRF54L15', 'CC2652R1', 'EFR32xG21' */
    char firmware_path[256];        /**< option firmware: path to .bin/.hex firmware image */
    bool fw_loaded;                 /**< option fw_loaded: firmware already flashed? */
    char tty[64];                   /**< option tty: UART device path, e.g. '/dev/ttyS2' */
    uint32_t baudrate;              /**< option baudrate: default 115200 */
    int reset_pin;                  /**< option resetpin: GPIO number for HW reset (-1=none) */
    int backdoor_pin;               /**< option backdoorpin: GPIO for bootloader entry (-1=none) */
    bool wdt_enabled;               /**< option wdt_enabled: hardware watchdog */
    char section_name[32];          /**< UCI section name, e.g. 'eap115' */
} ble_chip_config_t;

/** Main library configuration */
typedef struct {
    ble_transport_t transport;
    ble_chip_t chip_profile;
    char device_path[128];          /**< UART device (from ble_chip.tty or override) */
    char adapter[32];               /**< BlueZ adapter, e.g. "hci0" */
    uint32_t baud_rate;             /**< UART baud (from ble_chip.baudrate or override) */
    ble_chip_config_t chip;         /**< Hardware chip descriptor */
} ble_config_t;

/* ── Callback ── */
typedef void (*ble_event_cb_t)(const ble_event_t *event, void *user_data);

/* ══════════════════════════════════════════════════════════════════
 *  LIBRARY LIFECYCLE
 * ══════════════════════════════════════════════════════════════════ */

/** Initialize the library with explicit config. Returns BLE_OK or error. */
BLE_API int ble_init(const ble_config_t *config);

/** Initialize from UCI config file path (e.g. "/etc/config/ble-provision"). */
BLE_API int ble_init_from_uci(const char *uci_path);

/** Shutdown and release all resources. */
BLE_API void ble_deinit(void);

/** Get file descriptor for external event loop integration (poll/epoll/select). */
BLE_API int ble_get_fd(void);

/** Process pending events. Call when fd is readable. Non-blocking. */
BLE_API int ble_process(void);

/** Subscribe to events. Multiple subscribers allowed. */
BLE_API int ble_subscribe(ble_event_cb_t cb, void *user_data);

/** Unsubscribe. */
BLE_API int ble_unsubscribe(ble_event_cb_t cb);

/** Get active transport name (e.g. "bluez", "uart_hci"). */
BLE_API const char *ble_get_transport_name(void);

/** Get active chip profile name (e.g. "hci_h4", "ti_npi", "json"). NULL if BlueZ. */
BLE_API const char *ble_get_chip_profile_name(void);

/** Get error string for a BLE_ERR_* code. */
BLE_API const char *ble_strerror(int err);

/* ══════════════════════════════════════════════════════════════════
 *  RADIO TX POWER API
 * ══════════════════════════════════════════════════════════════════ */

/** Special value meaning "use chip default radio power" */
#define BLE_RADIO_POWER_DEFAULT   0x7F

/**
 * Set the radio TX power for advertising.
 * The value is mapped to the nearest supported level for the active chip.
 * Returns the actual power level set (may differ from requested),
 * or a negative BLE_ERR_* code on failure.
 */
BLE_API int ble_set_radio_tx_power(int8_t power_dbm);

/**
 * Get supported TX power range for the current chip.
 * Returns 0 on success, fills min_dbm and max_dbm.
 */
BLE_API int ble_get_radio_power_range(int8_t *min_dbm, int8_t *max_dbm);

/* ══════════════════════════════════════════════════════════════════
 *  SCAN API
 * ══════════════════════════════════════════════════════════════════ */

BLE_API int ble_scan_start(uint32_t duration_ms, bool active, bool filter_dup);
BLE_API int ble_scan_stop(void);
BLE_API bool ble_scan_is_active(void);

/* ══════════════════════════════════════════════════════════════════
 *  BEACON API
 * ══════════════════════════════════════════════════════════════════ */

BLE_API int ble_beacon_start(const ble_beacon_config_t *config);
BLE_API int ble_beacon_stop(void);
BLE_API bool ble_beacon_is_active(void);

/* Helper: parse UUID string into uuid_bytes field */
BLE_API int ble_uuid_parse(const char *uuid_str, uint8_t out_bytes[16]);
/* Helper: format uuid_bytes into UUID string */
BLE_API int ble_uuid_format(const uint8_t bytes[16], char out_str[BLE_UUID_STR_LEN]);

/* ══════════════════════════════════════════════════════════════════
 *  GATT API
 * ══════════════════════════════════════════════════════════════════ */

BLE_API int ble_gatt_connect(const char *address, uint8_t addr_type);
BLE_API int ble_gatt_disconnect(uint16_t conn_handle);
BLE_API int ble_gatt_discover(uint16_t conn_handle);
BLE_API int ble_gatt_read(uint16_t conn_handle, uint16_t char_handle);
BLE_API int ble_gatt_write(uint16_t conn_handle, uint16_t char_handle,
                   const uint8_t *data, uint16_t data_len);
BLE_API int ble_gatt_subscribe(uint16_t conn_handle, uint16_t cccd_handle, bool enable);

/* ══════════════════════════════════════════════════════════════════
 *  RAW HCI (advanced usage)
 * ══════════════════════════════════════════════════════════════════ */

/** Send raw HCI command. Returns BLE_ERR_NOSYS if transport doesn't support raw HCI. */
BLE_API int ble_hci_send(uint16_t opcode, const uint8_t *params, uint8_t param_len);

/** Send vendor-specific command. */
BLE_API int ble_vendor_cmd(uint16_t ocf, const uint8_t *params, uint8_t param_len);

#ifdef __cplusplus
}
#endif

#endif /* LIBBLE_H */
