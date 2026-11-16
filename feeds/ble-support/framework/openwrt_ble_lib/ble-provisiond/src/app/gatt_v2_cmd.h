/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file gatt_v2_cmd.h
 * @brief A2 (write) dispatch for GATT provisioning v2 (§19).
 *
 * A2 (`e063c3a2`) carries EITHER a JSON command object `{"cmd":"…", …}` OR a
 * fragmented configuration payload. This module owns that classification and
 * the whole command policy, including the authentication rules — so the BlueZ
 * path and the TI NPI path behave identically by construction.
 *
 * The policy is a faithful port of the shipped Python reference
 * (`wifi_provision.py` / `WifiProvisionCharacteristic.WriteValue`):
 *
 *   - A payload starting with '{' is a JSON command; anything else is a
 *     configuration fragment.
 *   - `upgrade_ack` is accepted WITHOUT authentication (it only clears a stale
 *     advertisement status and carries no risk).
 *   - Every other command requires authentication; otherwise A2 reports
 *     `{"status":"failed","reason":"not_authenticated"}`.
 *
 * All phone-visible output is published through gatt_v2.h (A3 outcome, mirrored
 * on A2 / A7
 * upgrade state), so callers only need the return code to decide which ATT /
 * D-Bus error to answer with.
 *
 * Outcomes are published on A3, which is what the shipped phone app subscribes
 * to, and mirrored on A2 so a client can also use request/response on the
 * attribute it wrote. Firmware upgrade progress stays on A7.
 */
#ifndef GATT_V2_CMD_H
#define GATT_V2_CMD_H

#include <stdint.h>
#include <stdbool.h>

#include "gatt_v2_conn.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Outcome of an A2 write, used by the transport to choose its reply. */
typedef enum {
    /** Handled successfully — answer the write with success. */
    GATT_V2_CMD_OK          = 0,
    /** Refused: not authenticated, or a feature gate is off. */
    GATT_V2_CMD_DENIED      = -1,
    /** Recognised, but deliberately not implemented yet. */
    GATT_V2_CMD_UNSUPPORTED = -2,
    /** Malformed payload. */
    GATT_V2_CMD_INVALID     = -3,
} gatt_v2_cmd_result_t;

/**
 * Handle one A2 write.
 *
 * Implemented commands:
 *   {"cmd":"reboot"}         reboot the device            (auth)
 *   {"cmd":"factory_reset"}  restore defaults + reboot    (auth + UCI gate
 *                                                          allow_factory_reset)
 *   {"cmd":"upgrade", …}     firmware upgrade (A7, §19.8) (auth)
 *   {"cmd":"upgrade_ack"}    clear terminal upgrade state (no auth)
 *
 * Deliberately not implemented yet — these report
 * `{"status":"error","reason":"not_implemented"}` on A3:
 *   {"cmd":"set_mode", …}    cloud/standalone switch      (§19.7)
 *   configuration payloads   WiFi config apply            (§19.3)
 *
 * @param data Raw A2 bytes (need NOT be NUL-terminated).
 * @param len  Byte count.
 * @param conn This link's state — its authentication (§19.4) and its own
 *             configuration reassembler (§19.3). NULL means an unidentified
 *             link, which is treated as unauthenticated and cannot send
 *             configuration fragments.
 * @return a gatt_v2_cmd_result_t value.
 */
gatt_v2_cmd_result_t gatt_v2_cmd_handle_write(const uint8_t *data, uint16_t len,
                                              gatt_v2_conn_t *conn);

#ifdef __cplusplus
}
#endif

#endif /* GATT_V2_CMD_H */
