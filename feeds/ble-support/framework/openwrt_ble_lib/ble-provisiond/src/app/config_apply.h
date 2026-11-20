/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file config_apply.h
 * @brief Applying a configuration received on A2 (§19.5.5).
 *
 * The first thing in this service that changes the device. Everything before it
 * — authentication, the status and configuration reads, the fragment layer — is
 * observable but harmless. This writes a uCentral configuration and restarts
 * networking, so the failure modes matter: a half-applied configuration can take
 * the radios or the management path down, and the phone is on the radio it is
 * reconfiguring.
 *
 * Two paths, chosen from the document's top-level keys exactly as the reference
 * chooses (`wifi_provision.py`, `_is_ucentral_format`):
 *
 *   uCentral  any of {unit, radios, services, metrics, globals, interfaces}
 *             → write /etc/ucentral/ucentral.cfg.<uuid> and run ucentral.uc
 *   UCI       otherwise, a `{"radios":[…]}` topology
 *             → uci set wireless.<section>.{ssid,encryption,key}, commit, reload
 *
 * Results and progress go out on A3 and in the advertisement's device_status
 * byte; nothing here talks to a transport directly.
 */
#ifndef CONFIG_APPLY_H
#define CONFIG_APPLY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CONFIG_APPLY_STARTED      = 0,   /**< accepted; the outcome arrives on A3 */
	CONFIG_APPLY_BAD_JSON     = -1,  /**< not a JSON object */
	CONFIG_APPLY_BUSY_UPGRADE = -2,  /**< a firmware upgrade is in progress */
	CONFIG_APPLY_BUSY_SELF    = -3,  /**< another apply is still running */
	CONFIG_APPLY_ERROR        = -4,  /**< could not start (write or fork failed) */
} config_apply_result_t;

/**
 * How long ucentral.uc gets before it is killed and judged where it stands.
 *
 * 300 s, not 60: an 8-LAN port-reassignment apply takes roughly 150 s, and a
 * mid-flight kill once left services half-restarted and wedged the management
 * path (ECCLOUD-10607). Long enough to finish beats short enough to feel
 * responsive, because the failure mode of the latter is a bricked management
 * interface.
 *
 * Exposed because it is the bound that makes config_apply_busy() safe to refuse
 * on. `reboot`, `factory_reset` and `set_mode` all turn a busy apply into a
 * `busy: applying` error, and that is only acceptable because busy cannot last
 * longer than this — the live path via `kill_t`, an inherited one via the age
 * check in ca_resume_evaluate(), both ending in ca_finish(). Shortening this
 * shortens the lockout; removing the bound would make those refusals permanent.
 */
#define CONFIG_APPLY_TIMEOUT_SEC   300

/**
 * Prepare timers, clear state, and report on any apply left over from before.
 *
 * Also the recovery point for an apply this daemon did not start: a record left
 * by a previous instance, or by the instance running before a reboot, is picked
 * up here and driven to a terminal result on A3 (§19.9.12). Call from the GATT
 * app's init.
 */
void config_apply_init(void);

/**
 * Stop watching a running child and release state. Call from the GATT app's
 * stop path.
 *
 * Does NOT kill the child and does NOT roll back a half-finished apply: the
 * child owns that window, and stopping the daemon mid-apply is not a reason to
 * undo what the operator asked for. It is logged loudly instead, and the state
 * record is deliberately left behind so the next instance can report the result.
 */
void config_apply_deinit(void);

/**
 * Apply @json.
 *
 * Returns as soon as the work is handed to a child process — the caller must not
 * block, and the phone gets `{"status":"applying"}` on A3 immediately so it can
 * show progress. Every outcome, including a rejection, is published on A3 by this
 * module, so the caller does not need to report anything itself.
 *
 * @param json NUL-terminated configuration document.
 * @return CONFIG_APPLY_STARTED, or a negative code describing the refusal.
 */
config_apply_result_t config_apply_start(const char *json);

/**
 * True while a configuration apply is in progress.
 *
 * Counts an apply inherited from a previous instance of the daemon as well as
 * one this instance forked, because callers use this to decide whether it is
 * safe to reboot, wipe the overlay or switch mode — and a reparented render is
 * still running.
 *
 * All three callers in gatt_v2_cmd.c turn a true here into
 * `{"status":"failed","reason":"busy: applying"}` rather than queueing or
 * proceeding. That is only defensible because the condition is bounded by
 * CONFIG_APPLY_TIMEOUT_SEC; see the note there.
 */
bool config_apply_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_APPLY_H */
