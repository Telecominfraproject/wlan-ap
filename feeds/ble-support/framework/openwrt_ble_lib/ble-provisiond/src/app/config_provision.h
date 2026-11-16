/**
 * @file config_provision.h
 * @brief uCentral configuration provisioning via BLE — pass-through handler.
 *
 * This module receives a complete uCentral JSON configuration from the phone
 * app (sent via BLE GATT Write), validates the basic JSON structure, and
 * forwards it to ucentral-agent via ubus for apply.
 *
 * The phone builds a standard uCentral schema JSON (radios, interfaces,
 * services, etc.) — the same format used by the cloud controller.
 * ble-provisiond does NOT parse or interpret the config contents; it only
 * acts as a BLE-to-ubus transport bridge.
 *
 * Apply path:
 *   Phone → BLE → ble-provisiond → ubus → ucentral-agent → UCI → services
 *
 * This is identical to how the cloud pushes config:
 *   Cloud → WSS → ucentral-agent → UCI → services
 */
#ifndef CONFIG_PROVISION_H
#define CONFIG_PROVISION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Status codes returned to the phone via BLE notification */
typedef enum {
    CONFIG_PROV_OK            = 0,   /**< Config applied successfully */
    CONFIG_PROV_ERR_INVALID   = -1,  /**< JSON parsing failed */
    CONFIG_PROV_ERR_AGENT     = -2,  /**< ucentral-agent unreachable */
    CONFIG_PROV_ERR_APPLY     = -3,  /**< Agent rejected the config */
    CONFIG_PROV_ERR_TIMEOUT   = -4,  /**< Agent apply timed out */
    CONFIG_PROV_ERR_DISABLED  = -5,  /**< Config provisioning disabled */
    CONFIG_PROV_ERR_NOMEM     = -6,  /**< Memory allocation failed */
} config_prov_status_t;

/** Callback invoked when the apply operation completes (async) */
typedef void (*config_prov_done_cb)(config_prov_status_t status,
                                    const char *message,
                                    void *user_data);

/**
 * Initialize the config provisioning module.
 * Must be called after ubus is connected.
 *
 * @param allow_config  Whether config provisioning is enabled (UCI gate).
 * @return 0 on success, negative on error.
 */
int config_provision_init(bool allow_config);

/**
 * De-initialize the config provisioning module.
 */
void config_provision_deinit(void);

/**
 * Apply a uCentral configuration received via BLE.
 *
 * The JSON payload is forwarded to ucentral-agent unchanged.
 * This function is asynchronous — the result is delivered via callback.
 *
 * @param json_data  Raw JSON bytes (does NOT need to be null-terminated).
 * @param json_len   Length of JSON data.
 * @param done_cb    Callback for completion notification (may be NULL).
 * @param user_data  Opaque pointer passed to callback.
 * @return 0 if the request was accepted and forwarded,
 *         negative config_prov_status_t on immediate failure.
 */
int config_provision_apply(const uint8_t *json_data, uint16_t json_len,
                           config_prov_done_cb done_cb, void *user_data);

/**
 * Get the current device configuration from ucentral-agent.
 *
 * Calls ucentral-agent to retrieve the active config and writes it to buf.
 *
 * @param buf       Output buffer for JSON.
 * @param buf_size  Buffer size.
 * @return 0 on success, negative on error.
 */
int config_provision_get_current(char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_PROVISION_H */
