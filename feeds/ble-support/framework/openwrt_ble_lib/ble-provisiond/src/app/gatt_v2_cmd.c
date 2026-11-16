/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file gatt_v2_cmd.c
 * @brief A2 (write) dispatch for GATT provisioning v2 (§19).
 *
 * See gatt_v2_cmd.h for the contract. Parsing uses json-c, which the daemon
 * already links.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>

#include <json-c/json.h>

#include "gatt_v2.h"
#include "gatt_v2_cmd.h"
#include "gatt_v2_upgrade.h"
#include "gatt_v2_frag.h"
#include "config_apply.h"
#include "../uci_app_config.h"

/*
 * Grace period before a destructive action runs, matching the reference's
 * `await asyncio.sleep(3)`. The command returns immediately so the write
 * response — and an A3 notify — reach the phone before the device
 * goes down. Implemented as a detached shell so the event loop never blocks.
 */
#define V2_ACTION_DELAY_SEC   3

/*
 * Largest A2 payload we parse as a command. Command objects are tiny; a
 * configuration fragment is both larger and not JSON at its first byte, so this
 * only bounds the temporary buffer.
 */
#define V2_CMD_MAX_LEN        512

/* ── Device actions ── */

static gatt_v2_cmd_result_t action_reboot(void)
{
    syslog(LOG_NOTICE, "gatt_v2_cmd: reboot requested via BLE (in %ds)",
           V2_ACTION_DELAY_SEC);

    /*
     * Refuse while a configuration apply is in flight, like set_mode. This
     * reverses an earlier decision here and the reasoning is recorded so it is
     * not quietly reverted a third time.
     *
     * The previous comment argued that a reboot is *the* thing that clears a
     * stuck apply — `ucentral.active` still points at the previous configuration,
     * so the device comes back working, and the cost is the apply rather than the
     * device — and that refusing takes away the one action a user reaches for.
     * All of that is still true. What it missed is the app: a reboot mid-apply
     * takes the reboot path, so nothing ever runs the gates, nothing publishes a
     * terminal result, and the app sits on `applying` indefinitely. Trading one
     * lost apply for a permanently stuck UI is the wrong half to optimise.
     *
     * The refusal is bounded, which is what makes it safe to refuse at all: an
     * apply cannot stay busy for longer than CONFIG_APPLY_TIMEOUT_SEC. The live path
     * is bounded by `kill_t` and an inherited one by the age check in
     * ca_resume_evaluate(), and both funnel into ca_finish(), which clears the
     * state. So the worst case is one retry after at most that long, not a device
     * that can never be rebooted over BLE.
     *
     * A reboot requested while no apply is running is untouched, and the
     * out-of-band paths (the physical button, `ubus call system reboot`, ssh) are
     * all still there for a user who really is stuck — the durable outcome record
     * (§19.9.12) is what keeps *those* from stranding the app too.
     */
    if (config_apply_busy()) {
        syslog(LOG_WARNING, "gatt_v2_cmd: reboot rejected — a configuration "
               "apply is still running; it will publish a terminal result within "
               "%ds and the reboot can be retried then", CONFIG_APPLY_TIMEOUT_SEC);
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"busy: applying\"}");
        return GATT_V2_CMD_DENIED;
    }

    gatt_v2_result_set("{\"status\":\"rebooting\"}");

    char sh[64];
    snprintf(sh, sizeof(sh), "sleep %d && reboot &", V2_ACTION_DELAY_SEC);
    system(sh);
    return GATT_V2_CMD_OK;
}

static gatt_v2_cmd_result_t action_factory_reset(void)
{
    /*
     * Destructive: keep the long-standing UCI feature gate (default off) so a
     * device cannot be wiped unless the integrator opted in.
     */
    if (!uci_app_get_bool("ble", "gatt_server", "allow_factory_reset", false)) {
        syslog(LOG_WARNING, "gatt_v2_cmd: factory_reset denied "
               "(ble.gatt_server.allow_factory_reset is off)");
        gatt_v2_result_set("{\"status\":\"failed\","
                           "\"reason\":\"factory_reset_disabled\"}");
        return GATT_V2_CMD_DENIED;
    }

    /*
     * Refuse while a configuration apply is in flight. There was no such check
     * here at all, which was an omission rather than a decision — and the one
     * action with the least excuse for it.
     *
     * `jffs2reset -y` wipes /overlay, and an apply in flight is writing into it:
     * /etc/ucentral/ucentral.cfg.<uuid> plus whatever `ucentral.uc` has committed
     * so far. Interleaving the two means wiping a partially written configuration
     * set, and unlike the reboot case there is no previously active configuration
     * to fall back to — that is being erased as well. The device comes back at
     * defaults either way, so the user loses nothing by retrying once the apply
     * has published its result.
     *
     * Bounded by CONFIG_APPLY_TIMEOUT_SEC, like the other two refusals.
     */
    if (config_apply_busy()) {
        syslog(LOG_WARNING, "gatt_v2_cmd: factory_reset rejected — a "
               "configuration apply is still running and jffs2reset would wipe "
               "the overlay underneath it");
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"busy: applying\"}");
        return GATT_V2_CMD_DENIED;
    }

    syslog(LOG_NOTICE, "gatt_v2_cmd: factory reset requested via BLE (in %ds)",
           V2_ACTION_DELAY_SEC);
    gatt_v2_result_set("{\"status\":\"factory_resetting\"}");

    char sh[96];
    snprintf(sh, sizeof(sh), "sleep %d && jffs2reset -y && reboot &",
             V2_ACTION_DELAY_SEC);
    system(sh);
    return GATT_V2_CMD_OK;
}

/* ── Mode switching (§19.7.1) ── */

/*
 * The scripts that own the mode. We never touch the flag file or the services
 * ourselves: these know the whole sequence, and half of it is not guessable from
 * the outside.
 *
 * They are the `ec-standalone` package, and its own description says why they are
 * a package rather than part of either caller: "the BLE provisioning daemon
 * (wifi_provision.py) and the LAN CGI (ec-action) both shell out to them, so they
 * live here rather than inside either one".
 *
 * What ec-standalone 1.2 does:
 *
 *   enable_standalone.sh   touch /etc/ucentral/.provisioned (bailing out with
 *                          ucentral left running if that write fails, which is
 *                          the recoverable side), drop the pre-rename flag, sync,
 *                          stop+disable ucentral, *move* /etc/init.d/ucentral
 *                          aside so nothing can restart it, stop+disable
 *                          cloud_discover. No reboot.
 *
 *   disable_standalone.sh  move the init script back, re-enable both units,
 *                          remove both flags, sync, then `cloud_discover start`.
 *                          No reboot either — cloud_discover resolves the broker
 *                          URL and launches ucentral itself.
 *
 * Both are idempotent, and both order their steps so a power loss stops somewhere
 * recoverable. enable's early-exit test is worth reading: the flag alone does not
 * mean the work is done, because a sysupgrade keeps the flag (keep.d) but restores
 * the init script, so it also checks whether /etc/init.d/ucentral is back.
 *
 * Version matters, and older images are in the field. The copy on one eap111 under
 * test writes the *legacy* flag name and ends the cloud direction with `reboot`.
 * Nothing here depends on which one is installed — we fork and let it get on with
 * it — but anyone reading a test log needs to know the BLE link may or may not
 * survive a switch to cloud depending on the image.
 */
#define V2_ENABLE_STANDALONE   "/usr/sbin/enable_standalone.sh"
#define V2_DISABLE_STANDALONE  "/usr/sbin/disable_standalone.sh"

/**
 * Grace period before a mode script runs.
 *
 * 1 s, not the 3 s the reboot and factory-reset actions use, because that is what
 * the reference allows here and it is enough: the only thing that has to happen
 * first is the A3 notification reaching the phone, which at a 30 ms connection
 * interval is tens of milliseconds.
 *
 * It is thinner than it looks on images whose cloud script still ends in `reboot`
 * — see the note in action_set_mode() — but the service teardown there happens
 * before the reboot, so there is more than a second of real margin either way.
 */
#define V2_MODE_DELAY_SEC      1

/**
 * `{"cmd":"set_mode","mode":"standalone"|"cloud"}` (§19.7.1).
 *
 * The success result is published *before* the script runs, which is the
 * reference's ordering and not an accident: the script tears services down, so a
 * reply sent afterwards might never get out.
 *
 * On ec-standalone 1.2 neither direction reboots and the BLE link survives both,
 * so the phone can read A4 again to see the new mode. On older images the cloud
 * direction ends in `reboot` instead — there the link drops and the device is gone
 * for a minute. We do not detect which is installed and do not need to: the reply
 * is already out before either runs.
 *
 * What the phone sees next is worth knowing, because it looks inconsistent and is
 * not. A4's `mode` flips as soon as the flag file appears or disappears, which is
 * the first thing enable does and nearly the last thing disable does. But
 * `cloud_active` tracks whether the ucentral *process* is alive, and in the cloud
 * direction that waits on cloud_discover resolving a broker URL and completing EST
 * enrolment — seconds at best, never at all without an uplink. So
 * `mode=cloud, cloud_active=no` is a legitimate and possibly permanent state. It
 * is exactly the split §19.5.3 exists to report: intent and reality are different
 * facts, and conflating them is what ECCLOUD-10760 was about.
 */
static gatt_v2_cmd_result_t action_set_mode(const char *buf)
{
    struct json_object *obj = json_tokener_parse(buf);
    struct json_object *jmode = NULL;
    const char *mode = NULL;

    if (obj && json_object_object_get_ex(obj, "mode", &jmode) &&
        json_object_is_type(jmode, json_type_string))
        mode = json_object_get_string(jmode);

    bool to_standalone = (mode && strcmp(mode, "standalone") == 0);
    bool to_cloud      = (mode && strcmp(mode, "cloud") == 0);

    if (!to_standalone && !to_cloud) {
        /* Same shape and wording as the reference, including echoing the value
         * back so the app can show what it sent. */
        char r[160];
        snprintf(r, sizeof(r), "{\"status\":\"failed\",\"reason\":"
                 "\"invalid mode: %s\"}", mode ? mode : "");
        syslog(LOG_WARNING, "gatt_v2_cmd: set_mode rejected — mode '%s' is "
               "neither 'standalone' nor 'cloud'", mode ? mode : "(absent)");
        gatt_v2_result_set(r);
        if (obj)
            json_object_put(obj);
        return GATT_V2_CMD_INVALID;
    }

    /*
     * Refuse while a configuration apply is in flight. The reference has no such
     * guard; this is a deliberate addition.
     *
     * `ucentral.uc` stops a batch of services up front and restarts them at the
     * end — ec-standalone's own init script documents measuring that, and defers
     * its post-upgrade render by 15 s because of it. A mode switch in the middle
     * stops and starts an overlapping set, and the apply is allowed 300 s to run
     * (ECCLOUD-10607: an 8-LAN reassignment really does take ~150 s). On an older
     * image the cloud direction also reboots, straight through the half-applied
     * configuration. None of that is self-correcting, and serialising costs the
     * user one retry.
     */
    if (config_apply_busy()) {
        syslog(LOG_WARNING, "gatt_v2_cmd: set_mode=%s rejected — a configuration "
               "apply is still running and the switch may reboot", mode);
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"busy: applying\"}");
        json_object_put(obj);
        return GATT_V2_CMD_DENIED;
    }

    const char *script = to_standalone ? V2_ENABLE_STANDALONE
                                       : V2_DISABLE_STANDALONE;

    /*
     * Check the script is there before claiming success.
     *
     * The reference does not, and gets away with it because it only ever ran on
     * images that ship these scripts. We publish the result *before* forking, so
     * without this check a missing script means telling the phone the mode
     * changed and then doing nothing — the app would show the new mode, A4 would
     * keep reporting the old one, and the contradiction would be the only
     * symptom. An honest failure is worth one access() call.
     */
    if (access(script, X_OK) != 0) {
        syslog(LOG_ERR, "gatt_v2_cmd: set_mode=%s cannot run — %s is missing or "
               "not executable (%s). The mode scripts belong to the platform "
               "image, not to this package.", mode, script, strerror(errno));
        gatt_v2_result_set("{\"status\":\"failed\","
                           "\"reason\":\"mode scripts not available\"}");
        json_object_put(obj);
        return GATT_V2_CMD_UNSUPPORTED;
    }

    syslog(LOG_NOTICE, "gatt_v2_cmd: set_mode=%s via BLE — running %s in %ds%s",
           mode, script, V2_MODE_DELAY_SEC,
           to_cloud ? " (pre-1.2 ec-standalone reboots here; 1.2 does not)" : "");

    /* Published before the script starts; see the note above. */
    {
        char r[64];
        snprintf(r, sizeof(r), "{\"status\":\"success\",\"mode\":\"%s\"}", mode);
        gatt_v2_result_set(r);
    }

    /*
     * Same detached-shell idiom as action_reboot() and action_factory_reset()
     * directly above. The shell is safe here because `script` is one of two
     * compile-time constants chosen by the validated comparison above — the mode
     * string the phone sent never reaches the command line.
     */
    char sh[160];
    snprintf(sh, sizeof(sh), "sleep %d && %s &", V2_MODE_DELAY_SEC, script);
    system(sh);

    json_object_put(obj);
    return GATT_V2_CMD_OK;
}

/* ── Configuration fragments ── */

/**
 * Feed one configuration fragment into this connection's reassembler.
 *
 * Nothing is published on A3 while fragments are still arriving — the reference
 * is silent until the message is whole, and a per-fragment result would flood the
 * notify channel and overwrite the previous command's outcome.
 */
static gatt_v2_cmd_result_t handle_config_fragment(gatt_v2_conn_t *conn,
                                                   const uint8_t *data,
                                                   uint16_t len)
{
    v2_frag_result_t fr = v2_frag_rx_feed(&conn->frag, data, len);

    switch (fr) {
    case V2_FRAG_NEED_MORE:
        /* Silent on purpose: more fragments are expected. */
        return GATT_V2_CMD_OK;

    case V2_FRAG_INVALID:
        gatt_v2_result_set("{\"status\":\"failed\","
                           "\"reason\":\"invalid_fragment\"}");
        return GATT_V2_CMD_INVALID;

    case V2_FRAG_VERIFY:
        /*
         * Same reason string the reference produces: it raises
         * ValueError("verify_failed") and publishes str(e).
         */
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"verify_failed\"}");
        return GATT_V2_CMD_INVALID;

    case V2_FRAG_COMPLETE:
        break;
    }

    uint32_t json_len = 0;
    const char *json = v2_frag_rx_json(&conn->frag, &json_len);
    if (!json) {
        /* Should not happen: COMPLETE guarantees a document. */
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"verify_failed\"}");
        v2_frag_rx_reset(&conn->frag);
        return GATT_V2_CMD_INVALID;
    }

    syslog(LOG_INFO, "gatt_v2_cmd: configuration reassembled (%u bytes of JSON)",
           json_len);

    /*
     * Hand it to the apply engine, which publishes every outcome on A3 itself —
     * including its refusals — and returns as soon as a child has the work. The
     * reassembly buffer is released first: the apply can run for minutes and
     * there is no reason to hold ~24 KB for the duration.
     */
    config_apply_result_t ar = config_apply_start(json);
    v2_frag_rx_reset(&conn->frag);

    switch (ar) {
    case CONFIG_APPLY_STARTED:
        return GATT_V2_CMD_OK;
    case CONFIG_APPLY_BUSY_UPGRADE:
    case CONFIG_APPLY_BUSY_SELF:
        return GATT_V2_CMD_DENIED;
    default:
        return GATT_V2_CMD_INVALID;
    }
}

/* ── Public API ── */

gatt_v2_cmd_result_t gatt_v2_cmd_handle_write(const uint8_t *data, uint16_t len,
                                              gatt_v2_conn_t *conn)
{
    if (!data || len == 0)
        return GATT_V2_CMD_INVALID;

    bool authenticated = conn ? conn->authenticated : false;

    /*
     * Classify exactly as the reference does: a leading '{' means a JSON
     * command, anything else is a configuration fragment.
     */
    if (data[0] != '{') {
        if (!authenticated) {
            syslog(LOG_WARNING, "gatt_v2_cmd: config write rejected — not authenticated");
            gatt_v2_result_set(gatt_v2_not_authenticated_json());
            return GATT_V2_CMD_DENIED;
        }
        if (!conn) {
            /* No per-connection state means no reassembly buffer; refusing is
             * the only safe answer, since fragments from different links must
             * never share one. */
            syslog(LOG_WARNING, "gatt_v2_cmd: config fragment from an unnamed "
                   "connection — rejected");
            gatt_v2_result_set("{\"status\":\"failed\","
                               "\"reason\":\"no_connection_context\"}");
            return GATT_V2_CMD_INVALID;
        }
        return handle_config_fragment(conn, data, len);
    }

    if (len > V2_CMD_MAX_LEN) {
        syslog(LOG_WARNING, "gatt_v2_cmd: command too large (%u)", len);
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"invalid_command\"}");
        return GATT_V2_CMD_INVALID;
    }

    /* json-c needs a NUL-terminated string; A2 bytes are not terminated. */
    char buf[V2_CMD_MAX_LEN + 1];
    memcpy(buf, data, len);
    buf[len] = '\0';

    struct json_object *obj = json_tokener_parse(buf);
    struct json_object *jcmd = NULL;
    char cmd[32] = "";

    if (obj && json_object_is_type(obj, json_type_object) &&
        json_object_object_get_ex(obj, "cmd", &jcmd) &&
        json_object_is_type(jcmd, json_type_string)) {
        snprintf(cmd, sizeof(cmd), "%s", json_object_get_string(jcmd));
    }
    if (obj)
        json_object_put(obj);

    if (!cmd[0]) {
        syslog(LOG_WARNING, "gatt_v2_cmd: JSON write without a usable \"cmd\"");
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"invalid_command\"}");
        return GATT_V2_CMD_INVALID;
    }

    /*
     * upgrade_ack is the one command allowed before authentication: it only
     * clears a stale terminal upgrade status from the advertisement.
     */
    if (strcmp(cmd, "upgrade_ack") == 0) {
        gatt_v2_upgrade_ack();
        return GATT_V2_CMD_OK;
    }

    if (!authenticated) {
        syslog(LOG_WARNING, "gatt_v2_cmd: '%s' rejected — not authenticated", cmd);
        gatt_v2_result_set(gatt_v2_not_authenticated_json());
        return GATT_V2_CMD_DENIED;
    }

    if (strcmp(cmd, "reboot") == 0)
        return action_reboot();

    if (strcmp(cmd, "factory_reset") == 0)
        return action_factory_reset();

    if (strcmp(cmd, "upgrade") == 0) {
        /* The upgrade module validates and reports everything on A7. */
        int r = gatt_v2_upgrade_start(buf);
        if (r == -2) return GATT_V2_CMD_DENIED;     /* busy */
        if (r < 0)   return GATT_V2_CMD_INVALID;
        return GATT_V2_CMD_OK;
    }

    if (strcmp(cmd, "set_mode") == 0)
        return action_set_mode(buf);

    syslog(LOG_WARNING, "gatt_v2_cmd: unknown command '%s'", cmd);
    gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"unknown_command\"}");
    return GATT_V2_CMD_UNSUPPORTED;
}
