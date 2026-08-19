/**
 * @file ble_types.h
 * @brief Internal BLE type definitions used within libble.
 *
 * NOT installed to /usr/include — internal use only.
 */
#ifndef LIBBLE_TYPES_INTERNAL_H
#define LIBBLE_TYPES_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/** Maximum HCI command parameter length */
#define BLE_HCI_PARAM_MAX       255

/**
 * @brief HCI command structure for transport layer.
 */
typedef struct {
    uint16_t opcode;
    uint8_t params[BLE_HCI_PARAM_MAX];
    uint8_t param_len;
} ble_hci_cmd_t;

/**
 * @brief HCI event/response structure from transport layer.
 */
typedef struct {
    uint8_t event_code;
    uint16_t opcode;
    uint8_t status;
    uint8_t data[BLE_HCI_PARAM_MAX];
    uint8_t data_len;
} ble_hci_event_t;

/**
 * @brief BLE device state (internal).
 */
typedef enum {
    BLE_STATE_OFF = 0,
    BLE_STATE_IDLE,
    BLE_STATE_SCANNING,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_ERROR
} ble_state_t;

#endif /* LIBBLE_TYPES_INTERNAL_H */
