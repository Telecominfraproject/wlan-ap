/**
 * @file transport_plugin.h
 * @brief Transport plugin interface for libble (internal).
 *
 * Transport plugins abstract the physical BLE controller interface.
 * Implementations: BlueZ D-Bus, direct UART HCI.
 */
#ifndef LIBBLE_TRANSPORT_PLUGIN_H
#define LIBBLE_TRANSPORT_PLUGIN_H

#include "../ble_types.h"
#include "../../include/ble.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Transport event callback type.
 */
typedef void (*transport_event_cb_t)(const ble_event_t *event, void *ctx);

/**
 * @brief Transport plugin operations.
 */
typedef struct transport_plugin {
    const char *name;

    int (*init)(const char *config);
    int (*deinit)(void);
    int (*send_hci_cmd)(const ble_hci_cmd_t *cmd);
    int (*send_vendor_cmd)(uint8_t ogf, uint16_t ocf,
                           const uint8_t *params, uint8_t param_len);
    int (*register_event_callback)(transport_event_cb_t cb, void *ctx);
    int (*get_fd)(void);
    int (*process_events)(void);

    bool active;
} transport_plugin_t;

/* Built-in transport plugins */
extern transport_plugin_t bluez_transport_plugin;
extern transport_plugin_t uart_transport_plugin;

#endif /* LIBBLE_TRANSPORT_PLUGIN_H */
