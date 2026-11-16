/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file gatt_v2_upgrade.h
 * @brief Firmware upgrade state machine for GATT provisioning v2 (A7, §19.8).
 *
 * Drives the A7 characteristic through
 *   idle → downloading → [verifying] → [installing] → [rebooting] → success|failed
 * and keeps the advertisement `device_status` byte in step (10..15).
 *
 * The contract — command schema, validation rules, state vocabulary, error
 * messages and timings — mirrors the shipped Python reference
 * (`wifi_provision.py` / `FirmwareUpgradeHandler`), because the phone app
 * already speaks it.
 *
 * Transport-agnostic: all phone-visible output goes through gatt_v2.h, so the
 * BlueZ and TI paths share this one implementation.
 *
 * Non-blocking by construction: the daemon is a single-threaded uloop process,
 * so every delay is a `uloop_timeout` and the long-running `sysupgrade -T`
 * download/verify runs as a child watched with `uloop_process`. Nothing here
 * sleeps in the event loop.
 */
#ifndef GATT_V2_UPGRADE_H
#define GATT_V2_UPGRADE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the upgrade handler and select the mode.
 *
 * Mode resolution (first match wins):
 *   1. environment `EAP_FW_UPGRADE_MODE` — "real" or "simulate" (as the
 *      reference does),
 *   2. UCI `ble.gatt_server.fw_upgrade_mode`,
 *   3. "real".
 *
 * In "simulate" mode the state machine walks the full sequence on a timer and
 * never touches `sysupgrade`, so the phone app UI can be tested on a device
 * that must not be flashed.
 */
void gatt_v2_upgrade_init(void);

/** Cancel any pending timers/child. Call at plugin stop. */
void gatt_v2_upgrade_deinit(void);

/** The active mode string ("real" or "simulate"). Never NULL. */
const char *gatt_v2_upgrade_mode(void);

/**
 * Handle an `{"cmd":"upgrade", ...}` payload.
 *
 * Validates per §19.8 / the reference: `url` must be a non-empty https URL with
 * a host, `manual` must be boolean if present, a legacy `sha256` field is
 * rejected outright, and `md5` (if present) must be 32 hex digits. On any
 * violation A7 goes to `failed` with "invalid upgrade command" and the
 * device_status is left untouched.
 *
 * Refuses to start unless the device is idle (device_status == 0), reporting
 * A7 `failed` / "busy: device not idle".
 *
 * All outcomes are reported on A7; the caller does not need to publish
 * anything itself.
 *
 * @param json_text NUL-terminated JSON object text.
 * @return 0 when the upgrade was accepted and started,
 *         -1 when the command was invalid,
 *         -2 when the device was busy.
 */
int gatt_v2_upgrade_start(const char *json_text);

/**
 * Handle `{"cmd":"upgrade_ack"}`: clears a terminal upgrade status (14/15) back
 * to idle so the advertisement stops showing a stale fw_success/fw_failed.
 *
 * Deliberately allowed WITHOUT authentication (it only clears an advertisement
 * flag and carries no risk) — matching the reference.
 */
void gatt_v2_upgrade_ack(void);

#ifdef __cplusplus
}
#endif

#endif /* GATT_V2_UPGRADE_H */
