/**
 * @file ble_core_internal.h
 * @brief Internal header for libble core implementation.
 *
 * Contains the internal context structure, transport/chip profile
 * registries, and subscriber management. NOT installed to /usr/include.
 */
#ifndef LIBBLE_CORE_INTERNAL_H
#define LIBBLE_CORE_INTERNAL_H

#include "../include/ble.h"
#include "ble_types.h"
#include "transport/transport_plugin.h"
#include "chip_profile/chip_profile.h"

/** Maximum registered transports */
#define TRANSPORT_MAX   4

/** Maximum registered chip profiles */
#define CHIP_PROFILE_MAX_REG   8

/** Maximum event subscribers */
#define SUBSCRIBER_MAX  16

/**
 * @brief Event subscriber entry.
 */
typedef struct {
    ble_event_cb_t cb;
    void *user_data;
} ble_subscriber_t;

/**
 * @brief Internal library context (singleton).
 */
typedef struct {
    /* State */
    bool initialized;
    ble_state_t state;

    /* Configuration */
    ble_config_t config;

    /* Transport registry */
    transport_plugin_t *transports[TRANSPORT_MAX];
    int transport_count;
    int active_transport_idx;

    /* Chip profile registry */
    chip_profile_t *profiles[CHIP_PROFILE_MAX_REG];
    int profile_count;
    int active_profile_idx;

    /* Subscriber list */
    ble_subscriber_t subscribers[SUBSCRIBER_MAX];
    int subscriber_count;

    /* Scan state */
    bool scan_active;

    /* Beacon state */
    bool beacon_active;

    /* GATT state */
    uint16_t gatt_conn_handle;
    bool gatt_connected;
} libble_ctx_t;

/**
 * @brief Get the global libble context.
 */
libble_ctx_t *libble_get_ctx(void);

/**
 * @brief Dispatch an event to all subscribers.
 */
void libble_dispatch_event(const ble_event_t *event);

/**
 * @brief Transport event callback — internal bridge.
 */
void libble_transport_event_cb(const ble_event_t *event, void *ctx);

#endif /* LIBBLE_CORE_INTERNAL_H */
