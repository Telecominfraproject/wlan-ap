/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file gatt_v2.h
 * @brief Shared, transport-agnostic state for GATT provisioning v2 (§19).
 *
 * This module owns everything about the v2 service that must behave
 * identically on the BlueZ path and the TI multi_role (NPI) path:
 *
 *   - the advertisement `device_status` byte (§19.6),
 *   - the A2 command-outcome value + notification,
 *   - the A7 "Upgrade" state value + notification,
 *   - the canonical "not implemented yet" reply.
 *
 * It contains no D-Bus and no NPI code. Each transport registers ONE notify
 * function; the shared logic then pushes A2/A7 updates without knowing how they
 * reach the phone (BlueZ: PropertiesChanged; TI: NPI NOTIFY command).
 *
 * The contract mirrors the shipped Python reference (`wifi_provision.py`),
 * which the phone app already speaks — value strings and status codes are
 * intentionally identical.
 *
 * Threading: the daemon is single-threaded (uloop), so the reference's
 * `device_status_lock` is unnecessary here; ordering is guaranteed by the event
 * loop. The "busy" checks it protected are still enforced (see
 * gatt_v2_upgrade.h).
 */
#ifndef GATT_V2_H
#define GATT_V2_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The seven v2 characteristics, in the order the reference exports them.
 * Used as a transport-neutral notify/target identifier: BlueZ maps this to a
 * D-Bus object path, TI maps it to the 128-bit UUID carried over NPI.
 */
typedef enum {
	GATT_V2_CHR_READ    = 0,  /* A1 e063c3a1 — config read   (read)          */
	GATT_V2_CHR_WRITE   = 1,  /* A2 e063c3a2 — command, plus a mirror of the
                               *              outcome (write + read + notify) */
	GATT_V2_CHR_RESULT  = 2,  /* A3 e063c3a3 — command outcome, the channel the
                               *              phone app uses (read + notify)  */
	GATT_V2_CHR_STATUS  = 3,  /* A4 e063c3a4 — status        (read)          */
	GATT_V2_CHR_AUTH    = 4,  /* A5 e063c3a5 — credential + its verdict
                               *              (write + read + notify)        */
	GATT_V2_CHR_MAC     = 5,  /* A6 e063c3a6 — MAC           (read)          */
	GATT_V2_CHR_UPGRADE = 6,  /* A7 e063c3a7 — upgrade       (read + notify) */
} gatt_v2_chr_t;

/*
 * Advertisement `device_status` byte (§19.6). Values are mutually exclusive
 * across config-apply and firmware-upgrade, exactly as the reference defines
 * them — the phone app decodes these numbers.
 */
#define GATT_V2_DS_IDLE             0
#define GATT_V2_DS_CFG_APPLYING     1
#define GATT_V2_DS_CFG_SUCCESS      2
#define GATT_V2_DS_CFG_FAILED       3
#define GATT_V2_DS_FW_DOWNLOADING  10
#define GATT_V2_DS_FW_VERIFYING    11
#define GATT_V2_DS_FW_INSTALLING   12
#define GATT_V2_DS_FW_REBOOTING    13
#define GATT_V2_DS_FW_SUCCESS      14
#define GATT_V2_DS_FW_FAILED       15

/** Maximum length of an A2/A7 value we store and push. */
#define GATT_V2_VALUE_MAX         512

/**
 * Transport notify hook. Called whenever an A2/A7 value changes so the
 * connected phone is informed. May be NULL (no connection / notify
 * unsupported), in which case the value is still stored and readable.
 */
typedef void (*gatt_v2_notify_fn)(gatt_v2_chr_t chr,
								  const uint8_t *data, uint16_t len);

/** Reset all v2 shared state to defaults (idle). Call at plugin init. */
void gatt_v2_init(void);

/** Register the transport's notify hook (BlueZ or TI). Pass NULL to clear. */
void gatt_v2_set_notify_fn(gatt_v2_notify_fn fn);

/**
 * Hook called when the advertisement `device_status` byte changes.
 *
 * The byte is advertised (§19.6) and each transport republishes it differently —
 * BlueZ has to re-register the advertisement, the TI firmware has to be told over
 * NPI. Notifying on the transition means progress appears promptly instead of
 * waiting for whatever polling interval the transport uses.
 */
typedef void (*gatt_v2_status_fn)(uint8_t status);

/** Register the status-change hook. Pass NULL to clear. */
void gatt_v2_set_status_fn(gatt_v2_status_fn fn);

/* ── Advertisement device_status ── */

uint8_t gatt_v2_device_status(void);
void    gatt_v2_set_device_status(uint8_t status);

/** True when a firmware upgrade is mid-flight (statuses 10..13). */
bool gatt_v2_upgrade_active(void);

/** True when the upgrade ended and the phone has not acknowledged (14/15). */
bool gatt_v2_upgrade_terminal(void);

/* ── Command outcome (A3 primary, A2 mirror) ── */

/**
 * Store a result JSON object (e.g. `{"status":"rebooting"}`) and notify the
 * phone.
 *
 * Published on **A3 and A2**:
 *
 *  - **A3 is the contract.** The shipped phone app subscribes to A3 and reads it
 *    for every outcome — command results, config-apply progress and the
 *    not-authenticated rejection. It must keep working.
 *  - **A2 is an extension**, carrying the same bytes so a client can treat the
 *    command attribute as request/response and read its outcome back from the
 *    attribute it wrote. Kept for future designs.
 *
 * Firmware upgrade progress is NOT published here — it has its own channel (A7),
 * exactly as in the reference, because an upgrade reports many transitions and
 * would otherwise overwrite unrelated command results.
 */
void gatt_v2_result_set(const char *json);

/** Current outcome for an A3 or A2 read. Never NULL; defaults to `{"status":"idle"}`. */
const char *gatt_v2_result_json(void);

/* ── A5 Auth outcome ── */

/**
 * Publish the verdict of an A5 credential write and notify the phone.
 *
 * Request and response share the A5 attribute, the same shape as A2: the client
 * writes the credential and reads the same attribute for the verdict, instead of
 * having to infer it from whether a later operation was refused.
 *
 * @param ok true → `{"status":"success"}`,
 *           false → `{"status":"failed","reason":"invalid_credential"}`.
 *
 * The single failure reason is deliberate: a wrong password, an empty write and
 * a locked root account are reported identically so the value cannot be used to
 * probe how the device is provisioned.
 */
void gatt_v2_auth_set(bool ok);

/**
 * Return A5 to `{"status":"idle"}`. Call wherever authentication is dropped
 * (connect/disconnect edges), so a fresh client never reads a previous client's
 * `success`.
 */
void gatt_v2_auth_reset(void);

/** Current A5 value for a read. Never NULL; defaults to `{"status":"idle"}`. */
const char *gatt_v2_auth_json(void);

/* ── A7 Upgrade state ── */

/**
 * Store `{"state":<state>,"message":<message>}` and notify the phone.
 * @param state   one of idle/downloading/verifying/installing/rebooting/
 *                success/failed (the reference's vocabulary).
 * @param message human-readable detail; "" when not applicable. Required to be
 *                non-empty for "failed".
 */
void gatt_v2_upgrade_set(const char *state, const char *message);

/** Current A7 value for a read. Never NULL; defaults to state "idle". */
const char *gatt_v2_upgrade_json(void);

/* ── Shared replies ── */

/**
 * The canonical reply for functionality that is deliberately not wired up yet
 * (currently: WiFi config apply/read and the aggregated status). Identical on
 * both transports so the phone sees one behaviour.
 *
 * @return `{"status":"error","reason":"not_implemented"}`
 */
const char *gatt_v2_not_implemented_json(void);

/**
 * The canonical reply for an access that requires authentication but has none.
 *
 * Used for BOTH a rejected A2 write and a rejected A1/A4 read, so a client gets
 * one machine-readable reason instead of a bare sentinel byte on reads and JSON
 * on writes. This deliberately diverges from the Python reference, which
 * answers unauthenticated reads with a single 0x00.
 *
 * @return `{"status":"failed","reason":"not_authenticated"}`
 */
const char *gatt_v2_not_authenticated_json(void);

/**
 * Render the A6 (MAC) value: the address the device actually advertises with,
 * as lowercase hex without separators (e.g. "94ef97d938d0") — the format the
 * phone app expects.
 *
 * Sourced from libble, so it is the BlueZ adapter address on the BlueZ path and
 * the chip-reported public address on the TI path. Shared here so both
 * transports emit byte-identical values.
 *
 * @param out     Destination; must hold at least 13 bytes.
 * @param out_sz  Size of @out.
 * @return number of characters written (12 on success, 0 when unavailable).
 */
int gatt_v2_mac_hex(char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* GATT_V2_H */
