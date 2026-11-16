/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file gatt_v2_upgrade.c
 * @brief Firmware upgrade state machine for GATT provisioning v2 (A7, §19.8).
 *
 * See gatt_v2_upgrade.h. Behaviour is a faithful port of the Python reference
 * (`wifi_provision.py` / `FirmwareUpgradeHandler`) onto uloop:
 *
 *   - `asyncio.sleep()`            → `uloop_timeout`
 *   - `subprocess.run(sysupgrade)` → `fork()` + `uloop_process`
 *   - `device_status_lock`         → not needed (single-threaded event loop);
 *                                    the busy checks it guarded are kept.
 *
 * One deliberate fidelity note: in REAL mode the reference never publishes the
 * intermediate verifying/installing/rebooting states — `sysupgrade -T` does the
 * download+verify in one shot, then it reports `success` and flashes. We keep
 * that exact sequence so the phone app sees identical transitions.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/types.h>

#include <libubox/uloop.h>
#include <json-c/json.h>

#include "gatt_v2.h"
#include "gatt_v2_upgrade.h"
#include "../uci_app_config.h"

/* ── Reference timings (seconds) ── */
#define SIM_T_DOWNLOADING        3
#define SIM_T_VERIFYING          1
#define SIM_T_INSTALLING         3
#define SIM_T_REBOOTING          2

/* Auto-reset back to idle after a successful upgrade. */
#define REAL_IDLE_RESET_SEC    300
#define SIM_IDLE_RESET_SEC      30

/* Grace period between reporting success and actually flashing (real mode). */
#define REAL_FLASH_DELAY_SEC     3

#define UPG_URL_MAX            512
#define SYSUPGRADE_IMAGE       "/var/sysupgrade.img"
#define KEEPSTATE_SCRIPT       "/usr/sbin/ec-keepstate.sh"
/* Where the `sysupgrade -T` child's output is captured for error matching. */
#define UPG_LOG_PATH           "/tmp/ble-fw-upgrade.log"

/* A7 state vocabulary (reference-defined strings). */
#define ST_IDLE         "idle"
#define ST_DOWNLOADING  "downloading"
#define ST_VERIFYING    "verifying"
#define ST_INSTALLING   "installing"
#define ST_REBOOTING    "rebooting"
#define ST_SUCCESS      "success"
#define ST_FAILED       "failed"

/* Simulate-mode failure triggers, matched as substrings of the URL. */
#define SIM_MARK_FAIL_DOWNLOAD  "simulate-fail-download"
#define SIM_MARK_FAIL_VERIFY    "simulate-fail-verify"

/** Simulate-mode progression steps. */
typedef enum {
    SIM_STEP_DOWNLOADING = 0,
    SIM_STEP_VERIFYING,
    SIM_STEP_INSTALLING,
    SIM_STEP_REBOOTING,
    SIM_STEP_SUCCESS,
} sim_step_t;

static struct {
    char  mode[16];                 /* "real" | "simulate" */
    bool  running;                  /* an upgrade is in flight */

    /* Parsed command */
    char  url[UPG_URL_MAX];
    char  md5[33];
    bool  have_md5;
    bool  manual;
    bool  reset_to_default;

    sim_step_t sim_step;

    struct uloop_timeout step_t;    /* simulate progression / flash delay */
    struct uloop_timeout reset_t;   /* auto-reset to idle after success */
    struct uloop_process proc;      /* sysupgrade -T child */
    bool  proc_active;
} upg;

/* ── Helpers ── */

static void upg_fail(const char *message)
{
    upg.running = false;
    gatt_v2_set_device_status(GATT_V2_DS_FW_FAILED);
    /* The reference guarantees a non-empty message for "failed". */
    gatt_v2_upgrade_set(ST_FAILED, (message && *message) ? message : "upgrade failed");
}

static void upg_schedule_idle_reset(void)
{
    int secs = (strcmp(upg.mode, "simulate") == 0) ? SIM_IDLE_RESET_SEC
                                                   : REAL_IDLE_RESET_SEC;
    uloop_timeout_set(&upg.reset_t, secs * 1000);
}

static void upg_reset_cb(struct uloop_timeout *t)
{
    (void)t;
    /*
     * Only clear if we are still sitting on fw_success — an upgrade_ack or a
     * newly started operation may have moved us on, and we must not stomp it.
     */
    if (gatt_v2_device_status() == GATT_V2_DS_FW_SUCCESS) {
        gatt_v2_set_device_status(GATT_V2_DS_IDLE);
        gatt_v2_upgrade_set(ST_IDLE, "");
    }
}

/* ── Command validation (mirrors FirmwareUpgradeHandler._parse) ── */

static bool is_md5_hex(const char *s)
{
    if (!s) return false;
    size_t n = 0;
    for (; s[n]; n++) {
        char c = s[n];
        bool hex = (c >= '0' && c <= '9') ||
                   (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return n == 32;
}

/**
 * Accept only `https://<host>...` — scheme https and a non-empty host, the C
 * equivalent of the reference's startswith + urlparse netloc check.
 */
static bool is_https_url_with_host(const char *url)
{
    static const char pfx[] = "https://";
    const size_t pfx_len = sizeof(pfx) - 1;

    if (!url || strncmp(url, pfx, pfx_len) != 0)
        return false;

    const char *host = url + pfx_len;
    if (*host == '\0' || *host == '/')
        return false;           /* no netloc */
    return true;
}

/**
 * Parse + validate the upgrade command into `upg`.
 * @return true when valid; on false the caller reports "invalid upgrade command".
 */
static bool upg_parse(const char *json_text)
{
    struct json_object *obj = json_tokener_parse(json_text);
    if (!obj)
        return false;

    bool ok = false;
    struct json_object *j = NULL;

    if (!json_object_is_type(obj, json_type_object))
        goto out;

    /* cmd must be exactly "upgrade" */
    if (!json_object_object_get_ex(obj, "cmd", &j) ||
        !json_object_is_type(j, json_type_string) ||
        strcmp(json_object_get_string(j), "upgrade") != 0)
        goto out;

    /* url: required, non-empty https with a host */
    if (!json_object_object_get_ex(obj, "url", &j) ||
        !json_object_is_type(j, json_type_string))
        goto out;
    {
        const char *url = json_object_get_string(j);
        if (!url || !*url || strlen(url) >= UPG_URL_MAX)
            goto out;
        if (!is_https_url_with_host(url))
            goto out;
        snprintf(upg.url, sizeof(upg.url), "%s", url);
    }

    /* manual: optional, must be boolean when present */
    upg.manual = false;
    if (json_object_object_get_ex(obj, "manual", &j)) {
        if (!json_object_is_type(j, json_type_boolean))
            goto out;
        upg.manual = json_object_get_boolean(j);
    }

    /*
     * Legacy `sha256` is rejected outright rather than ignored, so a
     * mis-configured client cannot silently skip checksum verification.
     */
    if (json_object_object_get_ex(obj, "sha256", &j))
        goto out;

    /* md5: optional, but must be 32 hex digits when present */
    upg.have_md5 = false;
    upg.md5[0] = '\0';
    if (json_object_object_get_ex(obj, "md5", &j)) {
        if (!json_object_is_type(j, json_type_string))
            goto out;
        const char *m = json_object_get_string(j);
        if (!is_md5_hex(m))
            goto out;
        snprintf(upg.md5, sizeof(upg.md5), "%s", m);
        upg.have_md5 = true;
    }

    /* reset_to_default: optional; absent means keep config + mode. */
    upg.reset_to_default = false;
    if (json_object_object_get_ex(obj, "reset_to_default", &j))
        upg.reset_to_default = json_object_get_boolean(j) ? true : false;

    ok = true;

out:
    json_object_put(obj);
    return ok;
}

/* ── Simulate mode ── */

/** Enter "installing" and arm the next tick. */
static void sim_enter_installing(void)
{
    gatt_v2_set_device_status(GATT_V2_DS_FW_INSTALLING);
    gatt_v2_upgrade_set(ST_INSTALLING, "");
    upg.sim_step = SIM_STEP_INSTALLING;
    uloop_timeout_set(&upg.step_t, SIM_T_INSTALLING * 1000);
}

static void sim_step_cb(struct uloop_timeout *t)
{
    (void)t;

    switch (upg.sim_step) {
    case SIM_STEP_DOWNLOADING:
        /* "downloading" was published when the command was accepted. */
        if (strstr(upg.url, SIM_MARK_FAIL_DOWNLOAD)) {
            upg_fail("simulated download failure");
            return;
        }
        /* The reference only verifies when an md5 was supplied. */
        if (upg.have_md5) {
            gatt_v2_set_device_status(GATT_V2_DS_FW_VERIFYING);
            gatt_v2_upgrade_set(ST_VERIFYING, "");
            upg.sim_step = SIM_STEP_VERIFYING;
            uloop_timeout_set(&upg.step_t, SIM_T_VERIFYING * 1000);
        } else {
            sim_enter_installing();
        }
        return;

    case SIM_STEP_VERIFYING:
        if (strstr(upg.url, SIM_MARK_FAIL_VERIFY)) {
            upg_fail("simulated checksum mismatch");
            return;
        }
        sim_enter_installing();
        return;

    case SIM_STEP_INSTALLING:
        gatt_v2_set_device_status(GATT_V2_DS_FW_REBOOTING);
        gatt_v2_upgrade_set(ST_REBOOTING, "");
        upg.sim_step = SIM_STEP_REBOOTING;
        uloop_timeout_set(&upg.step_t, SIM_T_REBOOTING * 1000);
        return;

    case SIM_STEP_REBOOTING:
        /* Simulate mode deliberately does NOT reboot. */
        upg.running = false;
        gatt_v2_set_device_status(GATT_V2_DS_FW_SUCCESS);
        gatt_v2_upgrade_set(ST_SUCCESS, "");
        upg.sim_step = SIM_STEP_SUCCESS;
        upg_schedule_idle_reset();
        return;

    case SIM_STEP_SUCCESS:
    default:
        return;
    }
}

/* ── Real mode ── */

/**
 * Run the actual flash. `sysupgrade` replaces the firmware and reboots, so this
 * is the last thing the daemon does; a brief block here is harmless and the
 * keepstate probe is a fast script.
 */
static void real_flash_cb(struct uloop_timeout *t)
{
    (void)t;

    char keepstate[128] = "";

    /*
     * Carry the standalone/cloud mode across the flash (ec-keepstate.sh).
     * Without it a standalone AP returns cloud-managed and the controller
     * overwrites the config. `reset_to_default` wipes config and mode together.
     */
    if (upg.reset_to_default) {
        char cmd[160];
        snprintf(cmd, sizeof(cmd), "%s --clear >/dev/null 2>&1", KEEPSTATE_SCRIPT);
        system(cmd);
    } else {
        char cmd[160];
        snprintf(cmd, sizeof(cmd), "%s --full 2>/dev/null", KEEPSTATE_SCRIPT);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char out[128] = "";
            if (fgets(out, sizeof(out), fp)) {
                char flag[8] = "", path[96] = "";
                /* Expected: "-f <file>" */
                if (sscanf(out, "%7s %95s", flag, path) == 2 &&
                    strcmp(flag, "-f") == 0)
                    snprintf(keepstate, sizeof(keepstate), "-f '%s' ", path);
            }
            pclose(fp);
        }
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "sysupgrade %s'%s' >/dev/null 2>&1 &",
             keepstate, SYSUPGRADE_IMAGE);
    syslog(LOG_NOTICE, "gatt_v2_upgrade: flashing (%s)", cmd);
    system(cmd);
}

/**
 * `sysupgrade -T <url>` finished. Classify its output exactly as the reference
 * does, then either fail or report success and schedule the real flash.
 */
static void real_proc_cb(struct uloop_process *p, int ret)
{
    (void)p;
    upg.proc_active = false;

    char log[2048] = "";
    int fd = open(UPG_LOG_PATH, O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, log, sizeof(log) - 1);
        if (n > 0) log[n] = '\0';
        close(fd);
    }

    syslog(LOG_INFO, "gatt_v2_upgrade: 'sysupgrade -T' exited ret=%d", ret);

    if (strstr(log, "HTTP error 404")) {
        upg_fail("Image does not exist");
        return;
    }
    if (strstr(log, "Operation not permitted")) {
        upg_fail("URL failure");
        return;
    }
    if (strstr(log, "Image check failed")) {
        upg_fail("Image file download and verification error");
        return;
    }
    if (ret != 0) {
        /*
         * Any other non-zero exit (fetch failure, no such binary, ...). The
         * reference falls through silently here; we report it so the phone is
         * not left waiting on a state that will never advance.
         */
        upg_fail("download or image check failed");
        return;
    }

    /* Verified. Report success, then flash after a short grace period. */
    gatt_v2_set_device_status(GATT_V2_DS_FW_SUCCESS);
    gatt_v2_upgrade_set(ST_SUCCESS, "");

    upg.step_t.cb = real_flash_cb;
    uloop_timeout_set(&upg.step_t, REAL_FLASH_DELAY_SEC * 1000);
}

/**
 * Start `sysupgrade -T <url>` as a child, capturing output for error matching.
 * @return true when the child was spawned.
 */
static bool real_start_download(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "gatt_v2_upgrade: fork failed");
        return false;
    }

    if (pid == 0) {
        /* Child: redirect stdout+stderr to the log, then exec. */
        int fd = open(UPG_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) close(fd);
        }
        execlp("sysupgrade", "sysupgrade", "-T", upg.url, (char *)NULL);
        _exit(127);
    }

    upg.proc.pid = pid;
    upg.proc.cb  = real_proc_cb;
    uloop_process_add(&upg.proc);
    upg.proc_active = true;
    syslog(LOG_INFO, "gatt_v2_upgrade: started 'sysupgrade -T' pid=%d", (int)pid);
    return true;
}

/* ── Public API ── */

void gatt_v2_upgrade_init(void)
{
    memset(&upg, 0, sizeof(upg));

    const char *env = getenv("EAP_FW_UPGRADE_MODE");
    char uci_mode[16] = "";
    if (!env || !*env)
        uci_app_get_string("ble", "gatt_server", "fw_upgrade_mode",
                           uci_mode, sizeof(uci_mode), "");

    const char *sel = (env && *env) ? env : uci_mode;
    if (sel && strcmp(sel, "simulate") == 0)
        snprintf(upg.mode, sizeof(upg.mode), "simulate");
    else
        snprintf(upg.mode, sizeof(upg.mode), "real");

    upg.step_t.cb  = sim_step_cb;
    upg.reset_t.cb = upg_reset_cb;

    syslog(LOG_INFO, "gatt_v2_upgrade: initialized (mode=%s)", upg.mode);
}

void gatt_v2_upgrade_deinit(void)
{
    uloop_timeout_cancel(&upg.step_t);
    uloop_timeout_cancel(&upg.reset_t);
    if (upg.proc_active) {
        uloop_process_delete(&upg.proc);
        upg.proc_active = false;
    }
    upg.running = false;
}

const char *gatt_v2_upgrade_mode(void)
{
    return upg.mode[0] ? upg.mode : "real";
}

int gatt_v2_upgrade_start(const char *json_text)
{
    if (!json_text || !*json_text) {
        gatt_v2_upgrade_set(ST_FAILED, "invalid upgrade command");
        return -1;
    }

    if (!upg_parse(json_text)) {
        syslog(LOG_WARNING, "gatt_v2_upgrade: command rejected (validation)");
        /* No device_status change on a validation failure (reference §5.5). */
        gatt_v2_upgrade_set(ST_FAILED, "invalid upgrade command");
        return -1;
    }

    /* Must be completely idle — serialises against config apply and itself. */
    if (upg.running || gatt_v2_device_status() != GATT_V2_DS_IDLE) {
        syslog(LOG_WARNING, "gatt_v2_upgrade: busy (device_status=%u)",
               gatt_v2_device_status());
        gatt_v2_upgrade_set(ST_FAILED, "busy: device not idle");
        return -2;
    }

    syslog(LOG_NOTICE, "gatt_v2_upgrade: accepted (mode=%s manual=%d md5=%s "
           "reset_to_default=%d url=%s)",
           upg.mode, upg.manual, upg.have_md5 ? upg.md5 : "-",
           upg.reset_to_default, upg.url);

    upg.running = true;
    uloop_timeout_cancel(&upg.reset_t);

    gatt_v2_set_device_status(GATT_V2_DS_FW_DOWNLOADING);
    gatt_v2_upgrade_set(ST_DOWNLOADING, "");

    if (strcmp(upg.mode, "simulate") == 0) {
        upg.sim_step = SIM_STEP_DOWNLOADING;
        upg.step_t.cb = sim_step_cb;
        uloop_timeout_set(&upg.step_t, SIM_T_DOWNLOADING * 1000);
        return 0;
    }

    if (!real_start_download()) {
        upg_fail("could not start sysupgrade");
        return -1;
    }
    return 0;
}

void gatt_v2_upgrade_ack(void)
{
    if (!gatt_v2_upgrade_terminal())
        return;

    syslog(LOG_INFO, "gatt_v2_upgrade: ack — clearing status %u -> idle",
           gatt_v2_device_status());
    uloop_timeout_cancel(&upg.reset_t);
    gatt_v2_set_device_status(GATT_V2_DS_IDLE);
    gatt_v2_upgrade_set(ST_IDLE, "");
}
