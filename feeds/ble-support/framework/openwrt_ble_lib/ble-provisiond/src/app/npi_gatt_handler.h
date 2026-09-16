/**
 * @file npi_gatt_handler.h
 * @brief NPI GATT Server event handler for TI CC2652R1.
 *
 * When using TI NPI transport, the GATT server runs on the MCU.
 * This module receives NPI GATT events (phone writes/reads) forwarded
 * from the MCU, executes the corresponding actions on the Host, and
 * sends NPI commands back for GATT notifications to the phone.
 */
#ifndef NPI_GATT_HANDLER_H
#define NPI_GATT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/* NPI GATT Event types from MCU */
#define NPI_GATT_EVT_CHAR_WRITE     0x01  /* Client wrote to a characteristic */
#define NPI_GATT_EVT_CHAR_READ      0x02  /* Client requesting read value */
#define NPI_GATT_EVT_CONNECTED      0x03  /* BLE client connected */
#define NPI_GATT_EVT_DISCONNECTED   0x04  /* BLE client disconnected */
#define NPI_GATT_EVT_MTU_EXCHANGE   0x05  /* MTU negotiated */
#define NPI_GATT_EVT_SCAN_RESULT    0x06  /* reserved: Observer scan report */
#define NPI_GATT_EVT_READY          0x07  /* firmware booted & advertising ready */
#define NPI_GATT_EVT_BDADDR         0x08  /* firmware's actual BD address (6B LE) */

/* NPI GATT commands TO MCU */
#define NPI_GATT_CMD_NOTIFY         0x10  /* Send GATT notification */
#define NPI_GATT_CMD_READ_RSP       0x11  /* Read response value */
#define NPI_GATT_CMD_UPDATE_VALUE   0x12  /* Update characteristic value */
#define NPI_GATT_CMD_SET_NAME       0x20  /* Set GAP device name (payload: name) */
#define NPI_GATT_CMD_SET_BDADDR     0x21  /* Set BD address (payload: 6B LE) */
#define NPI_GATT_CMD_ADV_START      0x22  /* Start connectable advertising */
#define NPI_GATT_CMD_ADV_STOP       0x23  /* Stop advertising */
#define NPI_GATT_CMD_QUERY_READY    0x28  /* Ask firmware to (re)emit READY */
#define NPI_GATT_CMD_DISCONNECT     0x29  /* Terminate any active GATT link */

/*
 * Characteristic identifiers used in the NPI bridge "handle" field.
 *
 * The bridge is UUID-keyed, NOT ATT-handle-keyed: the firmware reports the
 * 16-bit characteristic UUID (via attr_to_uuid16) in CHAR_WRITE/CHAR_READ, and
 * the daemon likewise addresses NOTIFY/READ_RSP by UUID. This avoids depending
 * on the firmware's runtime ATT handles (which shift with the preceding GAP/
 * GATT/DevInfo services). Values match gatt_provision.h / gatt_server_app.h.
 */
#define NPI_HANDLE_DEV_CMD          0xFE01  /* Device Command (write) */
#define NPI_HANDLE_OTA_URL          0xFE02  /* OTA URL (write) */
#define NPI_HANDLE_DEV_STATUS       0xFE03  /* Device Status (read/notify) */
#define NPI_HANDLE_WIFI_WRITE       0xFE11  /* WiFi Config Write (write) */
#define NPI_HANDLE_WIFI_READ        0xFE12  /* WiFi Config Read (read) */
#define NPI_HANDLE_WIFI_STATUS      0xFE13  /* WiFi Status (read/notify) */
/* FE14 WiFi Scan removed — not part of the uCentral pass-through core. */

/**
 * @brief NPI GATT event structure received from MCU.
 */
typedef struct {
    uint8_t event_type;
    uint16_t conn_handle;
    uint16_t attr_handle;
    uint16_t mtu;
    uint8_t data[512];
    uint16_t data_len;
} npi_gatt_event_t;

/**
 * Initialize the NPI GATT handler (called once at startup).
 * Registers with the NPI transport to receive GATT events.
 */
int npi_gatt_handler_init(void);

/**
 * Deinitialize.
 */
void npi_gatt_handler_deinit(void);

/**
 * Process a GATT event received from the MCU via NPI.
 * Called by the NPI transport layer when a GATT-related NPI frame arrives.
 *
 * @param evt  Parsed GATT event from MCU.
 * @return 0 on success, negative on error.
 */
int npi_gatt_handle_event(const npi_gatt_event_t *evt);

/**
 * Send a GATT notification via NPI to the MCU.
 * The MCU will forward it as a BLE GATT notification to the connected phone.
 *
 * @param conn_handle  Connection handle.
 * @param attr_handle  Characteristic attribute handle.
 * @param data         Notification data.
 * @param data_len     Length of data.
 * @return 0 on success, negative on error.
 */
int npi_gatt_send_notify(uint16_t conn_handle, uint16_t attr_handle,
                         const uint8_t *data, uint16_t data_len);

/**
 * Send a read response via NPI.
 * Called when the phone reads a characteristic and the MCU asks the Host for the value.
 *
 * @param conn_handle  Connection handle.
 * @param attr_handle  Characteristic attribute handle.
 * @param data         Response data.
 * @param data_len     Length of data.
 * @return 0 on success, negative on error.
 */
int npi_gatt_send_read_rsp(uint16_t conn_handle, uint16_t attr_handle,
                           const uint8_t *data, uint16_t data_len);

/**
 * Push the provisioning setup to the multi_role firmware: set the GAP device
 * name (from UCI) and the BD address (derived from eth0), then start
 * connectable advertising. Call once after the bridge is initialized.
 *
 * @param name   NUL-terminated device name (e.g. "OpenWrt-BLE").
 * @param bdaddr 6-byte BD address, little-endian (as the chip expects).
 * @return 0 on success.
 */
int npi_gatt_send_provision_setup(const char *name, const uint8_t bdaddr[6]);

/**
 * Store the provisioning params (device name from UCI, BD address from eth0).
 * They are pushed to the firmware when it reports NPI_GATT_EVT_READY, which
 * guarantees the name update lands after the firmware's advertising is up.
 *
 * @param name    NUL-terminated device name.
 * @param bdaddr  6-byte BD address little-endian, or NULL if unknown.
 */
void npi_gatt_set_provision_params(const char *name, const uint8_t *bdaddr);

/**
 * Mirror the UCI ble.gatt_server.enabled flag into the handler. When disabled,
 * the provision setup sends ADV_STOP instead of ADV_START so the TI firmware
 * stops advertising the provisioning service — matching the BlueZ path, where
 * services are only advertised when enabled. Call before npi_gatt_query_ready()
 * / the first READY so the correct ADV state is applied.
 *
 * @param enabled true if provisioning GATT should advertise.
 */
void npi_gatt_set_gatt_enabled(bool enabled);

/**
 * Ask the firmware to (re)emit NPI_GATT_EVT_READY by sending NPI_CMD_QUERY_READY.
 * Used at daemon startup (including after a restart): the firmware only emits
 * READY on its own until first provisioned, so a restarted daemon would never
 * see it. This actively re-triggers the READY→provision handshake so the device
 * name is re-pushed. Also arms the handler to send the setup on the next READY.
 *
 * @return 0 on success.
 */
int npi_gatt_query_ready(void);

#endif /* NPI_GATT_HANDLER_H */
