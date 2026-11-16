/**
 * @file app_plugin.h
 * @brief Application plugin interface for ble-provisiond.
 *
 * App plugins use libble API (ble.h) for all BLE operations and
 * subscribe to events via ble_subscribe().
 */
#ifndef BLE_APP_PLUGIN_H
#define BLE_APP_PLUGIN_H

#include <ble.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Application plugin interface.
 */
typedef struct app_plugin {
    const char *name;
    int (*init)(void);
    void (*deinit)(void);
    int (*start)(void);
    int (*stop)(void);
    bool running;
} app_plugin_t;

/* Built-in app plugins */
extern app_plugin_t ibeacon_app_plugin;
extern app_plugin_t scan_app_plugin;
extern app_plugin_t gatt_app_plugin;
extern app_plugin_t gatt_server_app_plugin;

#endif /* BLE_APP_PLUGIN_H */
