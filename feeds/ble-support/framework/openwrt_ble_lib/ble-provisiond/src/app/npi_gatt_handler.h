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

/* NPI GATT commands TO MCU */
#define NPI_GATT_CMD_NOTIFY         0x10  /* Send GATT notification */
#define NPI_GATT_CMD_READ_RSP       0x11  /* Read response value */
#define NPI_GATT_CMD_UPDATE_VALUE   0x12  /* Update characteristic value */

/* Characteristic handle mapping (must match MCU firmware GATT table) */
#define NPI_HANDLE_DEV_CMD          0x0025  /* FE01: Device Command */
#define NPI_HANDLE_OTA_URL          0x0028  /* FE02: OTA URL */
#define NPI_HANDLE_DEV_STATUS       0x002B  /* FE03: Device Status */
#define NPI_HANDLE_WIFI_WRITE       0x0031  /* FE11: WiFi Config Write */
#define NPI_HANDLE_WIFI_READ        0x0034  /* FE12: WiFi Config Read */
#define NPI_HANDLE_WIFI_STATUS      0x0037  /* FE13: WiFi Status */
#define NPI_HANDLE_WIFI_SCAN        0x003A  /* FE14: WiFi Scan */

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

#endif /* NPI_GATT_HANDLER_H */
