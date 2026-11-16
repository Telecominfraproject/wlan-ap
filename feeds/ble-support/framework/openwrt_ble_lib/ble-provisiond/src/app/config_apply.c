/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file config_apply.c
 * @brief Applying a configuration received on A2 (§19.5.5). See config_apply.h.
 *
 * Why every slow step is a child process: the reference is asyncio and pushes the
 * apply into a thread pool, so `subprocess.run(..., timeout=300)` costs it
 * nothing. This daemon is one uloop thread that also carries BLE, D-Bus and the
 * advertisement timer, and `ucentral.uc` can legitimately run for minutes — an
 * 8-LAN port reassignment takes about 150 s (ECCLOUD-10607). A synchronous call
 * would freeze the link the phone is watching the progress on.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/stat.h>

#include <json-c/json.h>
#include <libubox/uloop.h>
#include <uci.h>

#include "config_apply.h"
#include "gatt_v2.h"
#include "../uci_app_config.h"

/* ── Paths and limits, all from the reference ── */

#define CA_UCENTRAL_DIR    "/etc/ucentral"
#define CA_ACTIVE_LINK     CA_UCENTRAL_DIR "/ucentral.active"
#define CA_UCODE           "/usr/bin/ucode"
#define CA_UCENTRAL_UC     "/usr/share/ucentral/ucentral.uc"

#define CA_WIRELESS_CFG    "/etc/config/wireless"
#define CA_WIRELESS_BAK    "/tmp/wireless.ble.bak"

/** Where a child's stdout+stderr is captured for inspection. */
#define CA_LOG_PATH        "/tmp/ble-prov-apply.log"

/*
 * How long ucentral.uc gets: CONFIG_APPLY_TIMEOUT_SEC, in config_apply.h.
 *
 * It lives in the header because gatt_v2_cmd.c's refusals depend on it being
 * finite — see the note there — so it is part of this module's contract rather
 * than an internal tuning knob.
 */

/** How long a terminal device_status stays before falling back to idle. */
#define CA_STATUS_RESET_SEC    10

/** Warnings kept from ucentral.uc's output; the rest are dropped. */
#define CA_MAX_WARNINGS        10

/** Tail of the child's output attached to a failure report. */
#define CA_LOG_TAIL            200

/**
 * Where a uCentral apply is recorded so a restarted daemon — or a rebooted
 * device — can still answer for it.
 *
 * **On flash, not /tmp, and that is a reversal.** The previous path was
 * `/tmp/ble-prov-apply.state`, with the argument that the question this file
 * answers is "did an apply started by a previous instance of *me* finish?", that
 * the question dies with a reboot because `ucentral.active` is then the truth,
 * and that a tmpfs file therefore expires at exactly the right moment for no
 * flash write.
 *
 * Two of those three claims hold. The one that does not is the conclusion.
 * `ucentral.active` is indeed the truth after a reboot, but nothing *reads* it,
 * because the record naming which uuid was asked for over BLE is what has just
 * been thrown away. So the truth sat on disk, unreachable, and A3 came back as
 * `{"status":"idle"}` — which is not a terminal state, so the app waited on
 * `applying` for ever. That is the same class of bug as §19.9.9's and the same
 * class of mistake: scoping a decision to the question in front of you (resume)
 * and not to the one the user is actually asking (what happened to my apply).
 *
 * Deliberately NOT under /etc/config/. Those files are rendered output and get
 * restored to the package copy mid-apply (§19.9.9) — which is precisely when this
 * record must not vanish.
 *
 * Costs two small flash writes per apply: one when the child is forked, one when
 * the result is known. An apply already writes /etc/ucentral/ucentral.cfg.<uuid>
 * to the same overlay, so this is not a new order of magnitude. On an image whose
 * /etc is tmpfs the writes still succeed and simply do not survive, degrading to
 * the old behaviour rather than failing.
 *
 * Only the uCentral path records itself. The UCI path's child is
 * `/sbin/wifi reload`, which leaves nothing behind that can be judged after the
 * fact — no symlink to compare, no log to grade.
 */
#define CA_STATE_PATH          "/etc/ble-prov-apply.state"

/**
 * Record shapes. One line, discriminated by the leading token.
 *
 *   state=pending uuid=<N> pid=<P> started=<T> cfg=<path>
 *   state=done uuid=<N> result=<json>
 *
 * `pending` is what the old format carried, now prefixed. `done` is new and
 * exists for one reason: ca_finish() used to unlink the record, so an apply that
 * completed and was then followed by a reboot left nothing behind either. The
 * result JSON goes last because warnings contain spaces, so it is read to end of
 * line rather than with `%s`.
 *
 * A record written by the previous build lives at the old /tmp path and is simply
 * not found here. That loses the report for an apply in flight across exactly one
 * package upgrade, which is the cheapest possible migration.
 */
#define CA_STATE_PENDING       "state=pending "
#define CA_STATE_DONE          "state=done "

/** How often a resumed apply is re-checked for having exited. */
#define CA_RESUME_POLL_SEC     2

/* ── State ── */

typedef enum { CA_PATH_UCENTRAL, CA_PATH_UCI } ca_path_t;

static struct {
    bool                 running;
    ca_path_t            path;
    unsigned long        uuid;              /* uCentral path: the config uuid */
    char                 cfg_path[128];     /* uCentral path: file we wrote */
    bool                 wireless_backed_up;/* UCI path: rollback is possible */
    struct uloop_process proc;
    struct uloop_timeout kill_t;
    struct uloop_timeout status_t;

    /* Resume of an apply inherited from a previous instance of this daemon. */
    struct uloop_timeout resume_t;
    int                  resume_pid;
    long                 resume_started;
} s_ca;

/* ══════════════════════════════════════════════════════════════════
 *  Small helpers
 * ══════════════════════════════════════════════════════════════════ */

/**
 * This boot's identity, so a record can be told apart from one written before a
 * reboot.
 *
 * The pid in a `pending` record only means anything inside the boot that wrote
 * it. Across a reboot it is at best meaningless and at worst reused — and the
 * one process that could plausibly hold it *and* match
 * ca_pid_runs_ucentral() is a cloud-triggered apply running at startup. That
 * would make ca_resume_evaluate() poll somebody else's child for up to
 * CONFIG_APPLY_TIMEOUT_SEC before answering, with the app sitting on `applying`
 * for the whole of it. So the pid has to be positively disqualified, not merely
 * unlikely to collide.
 *
 * `/proc/sys/kernel/random/boot_id` is the right primitive because it involves no
 * clock. Comparing timestamps looked equivalent and is not: a device without an
 * RTC comes back with a clock *behind* the one that wrote the record, so
 * `started < now - uptime` reads false exactly when it most needs to read true.
 *
 * Returns false if it cannot be read, which leaves the pid check as the backstop
 * — the behaviour before this existed.
 */
static bool ca_boot_id(char *out, size_t n)
{
    if (!out || n == 0)
        return false;
    out[0] = '\0';

    FILE *f = fopen("/proc/sys/kernel/random/boot_id", "r");
    if (!f)
        return false;

    char *p = fgets(out, (int)n, f);
    fclose(f);
    if (!p)
        return false;

    char *nl = strchr(out, '\n');
    if (nl)
        *nl = '\0';
    return out[0] != '\0';
}

/** Publish a terminal result on A3 and move device_status with it. */
static void ca_finish(bool ok, const char *result_json)
{
    /*
     * Downgrade the record from `pending` to `done` rather than removing it.
     *
     * It used to be unlinked here, on the reasoning that an apply with an answer
     * has no further use for a crash-recovery record. True of recovery, false of
     * reporting: publishing the result only puts it in this process's memory, so
     * a reboot a second later took the answer with it and the app was left on
     * `applying` with no way to ask again. The `done` record is what survives
     * that, and ca_resume_from_state() replays it on the next start.
     *
     * Every terminal path reaches here — ca_fail() funnels through it — so this
     * is the one place that needs to do either.
     *
     * A `done` record is only ever a *downgrade* of a `pending` one, never a
     * fresh write, and that rule is what keeps it meaningful. Most callers of
     * ca_fail() are synchronous rejections of the A2 write itself — not a JSON
     * object, no changes to apply, uci commit failed — which the app receives in
     * the same breath as its request; persisting those would replay a stale
     * failure at every start for an apply that never ran. The UCI path is
     * excluded by the same rule: its child is `/sbin/wifi reload`, it records no
     * `pending`, and its result carries no uuid to correlate with (see
     * CA_STATE_PATH). So: rewrite what is there, create nothing.
     *
     * `resume_pid` is cleared regardless, because config_apply_busy() reports on
     * it: leaving it set would keep the daemon "busy" for ever after one resumed
     * apply, which would block reboot, factory_reset and set_mode permanently.
     */
    if (access(CA_STATE_PATH, F_OK) == 0) {
        FILE *st = fopen(CA_STATE_PATH, "w");
        if (st) {
            fprintf(st, CA_STATE_DONE "uuid=%lu result=%s\n",
                    s_ca.uuid,
                    result_json ? result_json : "{\"status\":\"idle\"}");
            fclose(st);
        } else {
            syslog(LOG_WARNING, "config_apply: cannot record the result in %s "
                                "(%s) — a reboot before the app reads A3 will "
                                "leave it on `applying`",
                   CA_STATE_PATH, strerror(errno));
            unlink(CA_STATE_PATH);
        }
    }

    uloop_timeout_cancel(&s_ca.resume_t);
    s_ca.resume_pid = 0;

    gatt_v2_set_device_status(ok ? GATT_V2_DS_CFG_SUCCESS
                                 : GATT_V2_DS_CFG_FAILED);
    gatt_v2_result_set(result_json);

    /*
     * Fall back to idle after a while, but only from a *config* terminal state.
     * A firmware upgrade may have started in the meantime and we must not stomp
     * on its status — the advertisement byte is shared.
     */
    uloop_timeout_set(&s_ca.status_t, CA_STATUS_RESET_SEC * 1000);
}

static void ca_status_reset_cb(struct uloop_timeout *t)
{
    (void)t;
    uint8_t st = gatt_v2_device_status();
    if (st == GATT_V2_DS_CFG_SUCCESS || st == GATT_V2_DS_CFG_FAILED)
        gatt_v2_set_device_status(GATT_V2_DS_IDLE);
}

/** Publish a failure built from a reason string, with optional extra members. */
static void ca_fail(const char *reason, struct json_object *extra)
{
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "status", json_object_new_string("failed"));
    json_object_object_add(o, "reason", json_object_new_string(reason));

    if (extra) {
        json_object_object_foreach(extra, k, v)
            json_object_object_add(o, k, json_object_get(v));
        json_object_put(extra);
    }

    syslog(LOG_WARNING, "config_apply: failed — %s", reason);
    ca_finish(false, json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
    json_object_put(o);
}

/**
 * True when /proc/<pid> is running `ucentral.uc`.
 *
 * Reads `cmdline`, not `comm`: `comm` is `ucode`, the interpreter, and names no
 * script. The arguments are NUL-separated, so they are flattened first —
 * otherwise a search only ever sees argv[0].
 *
 * Takes the pid as a string so both callers can pass what they already have: a
 * `/proc` directory entry, and a number read back from the state file.
 *
 * The cmdline match is also the pid-reuse guard for the resume path: a pid that
 * has been recycled into some other process will not be running this script.
 */
static bool ca_pid_runs_ucentral(const char *pid_str)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid_str);

    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;               /* gone, or never existed */

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n == 0)
        return false;

    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\0')
            buf[i] = ' ';
    }
    buf[n] = '\0';

    return strstr(buf, CA_UCENTRAL_UC) != NULL;
}

/** Read a file into a malloc'd NUL-terminated buffer, or NULL. */
static char *ca_slurp(const char *path, size_t *len_out)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long n = ftell(fp);
    if (n < 0) { fclose(fp); return NULL; }
    rewind(fp);

    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(fp); return NULL; }

    size_t got = fread(buf, 1, (size_t)n, fp);
    fclose(fp);
    buf[got] = '\0';
    if (len_out)
        *len_out = got;
    return buf;
}

/* ══════════════════════════════════════════════════════════════════
 *  Child plumbing
 * ══════════════════════════════════════════════════════════════════ */

static void ca_child_done(struct uloop_process *p, int ret);

/**
 * Fork @argv with stdout and stderr captured to CA_LOG_PATH.
 *
 * A file rather than a pipe: the parent never has to drain a fd while the child
 * is still writing, which is the same reason gatt_v2_upgrade.c does it this way
 * for `sysupgrade -T`.
 */
static bool ca_spawn(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "config_apply: fork failed: %s", strerror(errno));
        return false;
    }

    if (pid == 0) {
        /*
         * Detach into a new session, so the apply is not in this daemon's
         * process group any more.
         *
         * This is load-bearing, not hygiene. `ucentral.uc` stops a batch of
         * services at the start of an apply and restarts them at the end, and
         * `ble-provisiond` is in a position to be one of them. Stopping a procd
         * instance signals its process group, so without this the apply kills
         * its own parent and then itself, half way through — leaving
         * `/etc/ucentral/ucentral.cfg.<uuid>` written, `ucentral.active` still
         * pointing at the previous configuration, and the network in whatever
         * state the batch had reached. That is exactly the "a new cfg appeared
         * but active did not move" report.
         *
         * setsid() can only fail if we are already a group leader, which a
         * freshly forked child is not, so the result is not checked. The apply
         * surviving the daemon is deliberate: a reparented child still finishes
         * and still moves the symlink, and config_apply's own 300 s timeout no
         * longer applies to it — see the persisted-state note in §19.5.5 for
         * why a restarted daemon must re-derive the outcome rather than assume
         * `idle`.
         */
        setsid();

        int fd = open(CA_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO)
                close(fd);
        }
        execv(argv[0], argv);
        _exit(127);
    }

    s_ca.proc.pid = pid;
    s_ca.proc.cb  = ca_child_done;
    uloop_process_add(&s_ca.proc);
    s_ca.running = true;

    uloop_timeout_set(&s_ca.kill_t, CONFIG_APPLY_TIMEOUT_SEC * 1000);
    syslog(LOG_INFO, "config_apply: started pid=%d (%s)", (int)pid, argv[0]);
    return true;
}

static void ca_kill_cb(struct uloop_timeout *t)
{
    (void)t;
    if (!s_ca.running)
        return;

    /*
     * The reference reports "apply timeout (300s)" and gives up. Killing is the
     * least bad option left: the alternative is a child that never exits and an
     * apply that can never be retried.
     */
    syslog(LOG_ERR, "config_apply: child still running after %ds — killing pid %d",
           CONFIG_APPLY_TIMEOUT_SEC, (int)s_ca.proc.pid);
    kill(s_ca.proc.pid, SIGKILL);
}

/* ══════════════════════════════════════════════════════════════════
 *  uCentral path
 * ══════════════════════════════════════════════════════════════════ */

/**
 * Check the result of ucentral.uc.
 *
 * Three gates, all of them load-bearing:
 *
 *  1. exit code — 0 for a clean apply *and* for "applied with rejects", so a
 *     non-zero code is always a hard failure.
 *  2. the ucentral.active symlink — ucentral.uc only moves it at the end of a
 *     successful apply, so this is what distinguishes "committed" from "ran
 *     cleanly having rejected every rule".
 *  3. `[W]`/`[E]` lines on stdout — surfaced as `warnings`, because they are how
 *     the operator learns that a channel or htmode silently fell back to auto
 *     (ECCLOUD-10056). Throwing them away used to make a degraded apply look
 *     clean.
 */
static void ca_ucentral_done(int ret)
{
    size_t log_len = 0;
    char *log = ca_slurp(CA_LOG_PATH, &log_len);

    /* Gate 1: exit code. */
    if (ret != 0) {
        char reason[64];
        snprintf(reason, sizeof(reason), "ucentral.uc exit code %d", ret);

        struct json_object *extra = json_object_new_object();
        if (log && log_len) {
            const char *tail = (log_len > CA_LOG_TAIL)
                             ? log + (log_len - CA_LOG_TAIL) : log;
            json_object_object_add(extra, "log", json_object_new_string(tail));
        }
        free(log);
        ca_fail(reason, extra);
        return;
    }

    /* Gate 2: the active symlink must point at the file we wrote. */
    char link[256];
    ssize_t n = readlink(CA_ACTIVE_LINK, link, sizeof(link) - 1);
    if (n < 0) {
        char reason[128];
        snprintf(reason, sizeof(reason), "active symlink missing: %s",
                 strerror(errno));
        free(log);
        ca_fail(reason, NULL);
        return;
    }
    link[n] = '\0';

    /*
     * readlink may hand back an absolute path or a bare name depending on how
     * ucentral.uc created the link, so compare basenames.
     */
    const char *got = strrchr(link, '/');
    got = got ? got + 1 : link;
    const char *want = strrchr(s_ca.cfg_path, '/');
    want = want ? want + 1 : s_ca.cfg_path;

    if (strcmp(got, want) != 0) {
        struct json_object *extra = json_object_new_object();
        json_object_object_add(extra, "expected", json_object_new_string(want));
        json_object_object_add(extra, "got", json_object_new_string(got));
        free(log);
        ca_fail("active symlink not updated", extra);
        return;
    }

    /* Gate 3: collect warnings, capped. */
    struct json_object *warnings = json_object_new_array();
    int kept = 0;

    for (char *line = log; line && *line && kept < CA_MAX_WARNINGS; ) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        char *s = line;
        while (*s && isspace((unsigned char)*s))
            s++;
        /* Keep the prefix: the app renders these verbatim. */
        if (strncmp(s, "[W]", 3) == 0 || strncmp(s, "[E]", 3) == 0) {
            json_object_array_add(warnings, json_object_new_string(s));
            kept++;
        }

        if (!nl)
            break;
        line = nl + 1;
    }
    free(log);

    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "status", json_object_new_string("success"));
    json_object_object_add(o, "uuid", json_object_new_int64((int64_t)s_ca.uuid));
    if (kept > 0)
        json_object_object_add(o, "warnings", warnings);
    else
        json_object_put(warnings);

    syslog(LOG_NOTICE, "config_apply: applied (uuid=%lu, %d warning(s))",
           s_ca.uuid, kept);
    ca_finish(true, json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
    json_object_put(o);
}

/**
 * Write the configuration and hand it to ucentral.uc.
 *
 * The uuid is the apply timestamp and is stamped into the document, because it is
 * both the file name and how the app correlates the result it gets back.
 */
static config_apply_result_t ca_start_ucentral(struct json_object *cfg)
{
    s_ca.uuid = (unsigned long)time(NULL);
    json_object_object_add(cfg, "uuid",
                           json_object_new_int64((int64_t)s_ca.uuid));

    snprintf(s_ca.cfg_path, sizeof(s_ca.cfg_path),
             CA_UCENTRAL_DIR "/ucentral.cfg.%010lu", s_ca.uuid);

    const char *compact = json_object_to_json_string_ext(cfg,
                                                        JSON_C_TO_STRING_PLAIN);
    if (!compact)
        return CONFIG_APPLY_ERROR;

    FILE *fp = fopen(s_ca.cfg_path, "w");
    if (!fp) {
        syslog(LOG_ERR, "config_apply: cannot write %s: %s",
               s_ca.cfg_path, strerror(errno));
        return CONFIG_APPLY_ERROR;
    }
    size_t want = strlen(compact);
    size_t wrote = fwrite(compact, 1, want, fp);
    /* fsync before handing the path to another process: ucentral.uc reads it. */
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    if (wrote != want) {
        syslog(LOG_ERR, "config_apply: short write to %s", s_ca.cfg_path);
        unlink(s_ca.cfg_path);
        return CONFIG_APPLY_ERROR;
    }

    syslog(LOG_INFO, "config_apply: uCentral format, wrote %zu bytes to %s",
           want, s_ca.cfg_path);

    char *argv[] = { (char *)CA_UCODE, (char *)CA_UCENTRAL_UC,
                     s_ca.cfg_path, NULL };
    if (!ca_spawn(argv)) {
        unlink(s_ca.cfg_path);
        return CONFIG_APPLY_ERROR;
    }

    s_ca.path = CA_PATH_UCENTRAL;

    /*
     * Record enough to judge this apply from a cold start. Written after the
     * fork so a failed spawn leaves no record, and best-effort: a write that
     * fails costs the report, not the apply.
     */
    char boot[64];
    if (!ca_boot_id(boot, sizeof(boot)))
        snprintf(boot, sizeof(boot), "unknown");

    FILE *st = fopen(CA_STATE_PATH, "w");
    if (st) {
        fprintf(st, CA_STATE_PENDING
                "uuid=%lu pid=%d started=%ld boot=%s cfg=%s\n",
                s_ca.uuid, (int)s_ca.proc.pid, (long)time(NULL),
                boot, s_ca.cfg_path);
        fclose(st);
    } else {
        syslog(LOG_WARNING, "config_apply: cannot write %s (%s) — a daemon "
                            "restart or reboot during this apply will lose its "
                            "result and leave the app on `applying`",
               CA_STATE_PATH, strerror(errno));
    }

    return CONFIG_APPLY_STARTED;
}

/* ══════════════════════════════════════════════════════════════════
 *  UCI path
 * ══════════════════════════════════════════════════════════════════ */

/** True for a UCI section name we are willing to build a path from. */
static bool ca_section_ok(const char *s)
{
    if (!s || !*s)
        return false;
    for (const char *p = s; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-')
            return false;
    }
    return true;
}

/**
 * Apply the topology form with libuci, then reload Wi-Fi in a child.
 *
 * libuci rather than shelling out to `uci set`: the values are SSIDs and
 * passphrases that came in over the air, and building a shell command line out of
 * them would make quoting the only thing standing between an attacker and command
 * execution. The library takes them as data.
 *
 * `wifi reload` is the one slow step, so it is the only thing forked.
 */
static config_apply_result_t ca_start_uci(struct json_object *cfg)
{
    struct json_object *radios = NULL;
    if (!json_object_object_get_ex(cfg, "radios", &radios) ||
        !json_object_is_type(radios, json_type_array)) {
        ca_fail("no changes to apply", NULL);
        return CONFIG_APPLY_ERROR;
    }

    /* Back up first: a failed apply has to be undoable. */
    s_ca.wireless_backed_up = false;
    {
        size_t len = 0;
        char *cur = ca_slurp(CA_WIRELESS_CFG, &len);
        if (cur) {
            FILE *fp = fopen(CA_WIRELESS_BAK, "w");
            if (fp) {
                if (fwrite(cur, 1, len, fp) == len)
                    s_ca.wireless_backed_up = true;
                fclose(fp);
            }
            free(cur);
        }
        if (!s_ca.wireless_backed_up)
            syslog(LOG_WARNING, "config_apply: could not back up %s — applying "
                   "anyway, but a failure will not be rolled back",
                   CA_WIRELESS_CFG);
    }

    struct uci_context *uci = uci_alloc_context();
    if (!uci) {
        ca_fail("uci context allocation failed", NULL);
        return CONFIG_APPLY_ERROR;
    }

    static const char *const keys[] = { "ssid", "encryption", "key" };
    int changes = 0;
    struct uci_ptr commit_ptr;
    bool have_pkg = false;

    size_t nr = json_object_array_length(radios);
    for (size_t i = 0; i < nr; i++) {
        struct json_object *r = json_object_array_get_idx(radios, i);
        struct json_object *ifaces = NULL;

        if (!r || !json_object_object_get_ex(r, "interfaces", &ifaces) ||
            !json_object_is_type(ifaces, json_type_array))
            continue;

        size_t ni = json_object_array_length(ifaces);
        for (size_t j = 0; j < ni; j++) {
            struct json_object *iface = json_object_array_get_idx(ifaces, j);
            struct json_object *v = NULL;

            if (!iface ||
                !json_object_object_get_ex(iface, "section", &v) || !v)
                continue;

            const char *section = json_object_get_string(v);
            if (!ca_section_ok(section)) {
                syslog(LOG_WARNING, "config_apply: skipping interface with an "
                       "unusable section name");
                continue;
            }

            for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
                if (!json_object_object_get_ex(iface, keys[k], &v) || !v)
                    continue;

                const char *val = json_object_get_string(v);
                if (!val)
                    continue;

                /* uci_lookup_ptr() writes into the buffer, so it cannot be
                 * const, and it splits on the first '=' — an '=' inside the
                 * value is therefore harmless. */
                char expr[512];
                snprintf(expr, sizeof(expr), "wireless.%s.%s=%s",
                         section, keys[k], val);

                struct uci_ptr ptr;
                if (uci_lookup_ptr(uci, &ptr, expr, true) != UCI_OK ||
                    uci_set(uci, &ptr) != UCI_OK) {
                    syslog(LOG_WARNING, "config_apply: uci set %s.%s failed",
                           section, keys[k]);
                    continue;
                }
                if (!have_pkg) {
                    commit_ptr = ptr;
                    have_pkg = true;
                }
                changes++;
                syslog(LOG_INFO, "config_apply: uci set wireless.%s.%s",
                       section, keys[k]);
            }
        }
    }

    if (changes == 0 || !have_pkg) {
        uci_free_context(uci);
        ca_fail("no changes to apply", NULL);
        return CONFIG_APPLY_ERROR;
    }

    if (uci_commit(uci, &commit_ptr.p, false) != UCI_OK) {
        uci_free_context(uci);
        ca_fail("uci commit failed", NULL);
        return CONFIG_APPLY_ERROR;
    }
    uci_free_context(uci);

    syslog(LOG_INFO, "config_apply: %d uci change(s) committed, reloading wifi",
           changes);

    char *argv[] = { (char *)"/sbin/wifi", (char *)"reload", NULL };
    if (!ca_spawn(argv)) {
        ca_fail("cannot start wifi reload", NULL);
        return CONFIG_APPLY_ERROR;
    }

    s_ca.path = CA_PATH_UCI;
    return CONFIG_APPLY_STARTED;
}

/** Put /etc/config/wireless back and reload. Best effort, by definition. */
static void ca_uci_rollback(void)
{
    if (!s_ca.wireless_backed_up) {
        syslog(LOG_ERR, "config_apply: no backup to roll back to");
        return;
    }

    size_t len = 0;
    char *bak = ca_slurp(CA_WIRELESS_BAK, &len);
    if (!bak) {
        syslog(LOG_ERR, "config_apply: backup unreadable — cannot roll back");
        return;
    }

    FILE *fp = fopen(CA_WIRELESS_CFG, "w");
    if (fp) {
        fwrite(bak, 1, len, fp);
        fclose(fp);
        syslog(LOG_NOTICE, "config_apply: rolled back %s", CA_WIRELESS_CFG);
    } else {
        syslog(LOG_ERR, "config_apply: cannot restore %s: %s",
               CA_WIRELESS_CFG, strerror(errno));
    }
    free(bak);

    /*
     * Reload with the restored file. Fire and forget: we are already on the
     * failure path and there is nothing further to report if this also fails.
     */
    pid_t pid = fork();
    if (pid == 0) {
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0) {
            dup2(null, STDOUT_FILENO);
            dup2(null, STDERR_FILENO);
        }
        execl("/sbin/wifi", "wifi", "reload", (char *)NULL);
        _exit(127);
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Completion
 * ══════════════════════════════════════════════════════════════════ */

static void ca_child_done(struct uloop_process *p, int ret)
{
    (void)p;
    s_ca.running = false;
    uloop_timeout_cancel(&s_ca.kill_t);

    if (s_ca.path == CA_PATH_UCENTRAL) {
        ca_ucentral_done(ret);
        return;
    }

    /* UCI path: the child was `wifi reload`. */
    if (ret != 0) {
        syslog(LOG_WARNING, "config_apply: wifi reload exited %d — rolling back",
               ret);
        ca_uci_rollback();
        ca_fail("wifi reload failed", NULL);
        return;
    }

    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "status", json_object_new_string("success"));
    syslog(LOG_NOTICE, "config_apply: UCI configuration applied");
    ca_finish(true, json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
    json_object_put(o);
}

/* ══════════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════════ */

/**
 * The reference's `_is_ucentral_format`: any uCentral top-level key.
 *
 * The rule used to require `interfaces[].ssids`, which missed hostname-only and
 * service-only edits and sent them down the UCI path where they did nothing
 * (ECCLOUD-10138).
 */
static bool ca_is_ucentral(struct json_object *cfg)
{
    static const char *const top[] = {
        "unit", "radios", "services", "metrics", "globals", "interfaces"
    };
    struct json_object *v = NULL;

    for (size_t i = 0; i < sizeof(top) / sizeof(top[0]); i++) {
        if (json_object_object_get_ex(cfg, top[i], &v))
            return true;
    }
    return false;
}

/**
 * True when a `ucentral.uc` is running that this daemon does not own.
 *
 * config_apply_deinit() no longer kills an apply in flight. That is the right
 * trade — see the comment there — but it opens a window this check closes: the
 * daemon is stopped mid-apply, comes back with `s_ca.running` false, and would
 * then happily fork a second `ucentral.uc` alongside the first. Two of them
 * stopping and starting the same services and writing the same uci packages is
 * worse than either outcome the old SIGKILL was trading between.
 *
 * It reads `/proc/<pid>/cmdline`, not `comm`: `comm` is `ucode`, the
 * interpreter, and says nothing about which script it is running. Walking /proc
 * rather than forking `pidof` matches `cloud_active()` in status_doc.c — this is
 * on a request path.
 *
 * Reached only when `s_ca.running` is false, so it can never match our own
 * child.
 */
static bool ca_foreign_apply_running(void)
{
    DIR *d = opendir("/proc");
    if (!d)
        return false;

    bool found = false;
    struct dirent *e;

    while (!found && (e = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0]))
            continue;

        if (ca_pid_runs_ucentral(e->d_name)) {
            syslog(LOG_WARNING, "config_apply: %s is already running as pid %s "
                                "— not starting a second one",
                   CA_UCENTRAL_UC, e->d_name);
            found = true;
        }
    }

    closedir(d);
    return found;
}

config_apply_result_t config_apply_start(const char *json)
{
    if (!json || !*json)
        return CONFIG_APPLY_BAD_JSON;

    /*
     * Busy, either way. Both branches answer on A3 directly instead of going
     * through ca_fail(), for the same reason the upgrade branch below does, and
     * one more:
     *
     *  - ca_fail() moves device_status to CFG_FAILED, which would tell the phone
     *    the apply *failed* while the real one is still running. The app polls
     *    A3 every two seconds, so this is the common case, not a corner.
     *  - ca_fail() funnels into ca_finish(), which rewrites CA_STATE_PATH as a
     *    `done` record and cancels the resume timer. On a resumed apply that
     *    replaces the record that lets its real outcome be reported with a
     *    failure belonging to the duplicate — and because the `done` record is
     *    replayed on the next start, that wrong answer would outlive the process
     *    that invented it. A duplicate request from the app would silently
     *    reopen the gap the state file exists to close, and now poison it too.
     *
     * The result string is still the reference's, so nothing the app sees
     * changes; what does not change is the state of an apply we are not
     * finishing. device_status is left exactly where it is: CFG_APPLYING if an
     * apply we can report on is in flight, and untouched if the running render
     * belongs to someone else — claiming APPLYING for one we cannot finish would
     * strand A3 there with nobody to move it.
     */
    if (s_ca.running) {
        syslog(LOG_WARNING, "config_apply: rejected — an apply is already running");
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"busy: applying\"}");
        return CONFIG_APPLY_BUSY_SELF;
    }

    /*
     * An apply left behind by a previous instance of this daemon, or started by
     * something else entirely — ec-standalone's init script forks ucentral.uc
     * 15 s into boot, so this window is real and not hypothetical. Same answer
     * as our own busy case: the app retries, and one render finishes cleanly
     * instead of two fighting.
     */
    if (ca_foreign_apply_running()) {
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"busy: applying\"}");
        return CONFIG_APPLY_BUSY_SELF;
    }

    /*
     * An upgrade in flight wins. Reject without touching device_status: the
     * upgrade owns it, and overwriting it would make the advertisement and A7
     * disagree about what the device is doing (Requirement 9.2).
     */
    uint8_t st = gatt_v2_device_status();
    if (st == GATT_V2_DS_FW_DOWNLOADING || st == GATT_V2_DS_FW_VERIFYING ||
        st == GATT_V2_DS_FW_INSTALLING  || st == GATT_V2_DS_FW_REBOOTING) {
        syslog(LOG_WARNING, "config_apply: rejected — firmware upgrade in progress");
        gatt_v2_result_set("{\"status\":\"failed\",\"reason\":\"busy: upgrading\"}");
        return CONFIG_APPLY_BUSY_UPGRADE;
    }

    struct json_object *cfg = json_tokener_parse(json);
    if (!cfg || !json_object_is_type(cfg, json_type_object)) {
        if (cfg)
            json_object_put(cfg);
        ca_fail("configuration is not a JSON object", NULL);
        return CONFIG_APPLY_BAD_JSON;
    }

    /*
     * Tell the phone we started before doing anything slow. The apply can take
     * minutes and the only thing worse than a slow apply is a silent one.
     */
    gatt_v2_result_set("{\"status\":\"applying\"}");
    gatt_v2_set_device_status(GATT_V2_DS_CFG_APPLYING);
    uloop_timeout_cancel(&s_ca.status_t);

    config_apply_result_t r = ca_is_ucentral(cfg) ? ca_start_ucentral(cfg)
                                                  : ca_start_uci(cfg);
    json_object_put(cfg);

    if (r != CONFIG_APPLY_STARTED && gatt_v2_device_status() ==
                                     GATT_V2_DS_CFG_APPLYING) {
        /* The path functions report their own reason except for the write/fork
         * errors, which leave the status stuck at "applying". */
        ca_fail("could not start the apply", NULL);
    }
    return r;
}

bool config_apply_busy(void)
{
    /*
     * An inherited apply counts. The callers of this (gatt_v2_cmd.c) ask "is a
     * configuration apply in progress", not "does this daemon own a child", and
     * the resume path is exactly where those two stopped meaning the same thing:
     * `running` is deliberately false for an apply reparented to init, while the
     * render is very much still going. `set_mode` relies on this to refuse a
     * mode switch that may reboot the switch mid-apply.
     *
     * config_apply_start() does not use this predicate — it checks `running` and
     * then scans /proc, because it has to tell its own child from a foreign one.
     */
    return s_ca.running || s_ca.resume_pid != 0;
}

/**
 * Decide what to report for an apply inherited from a previous instance.
 *
 * Called once at init and then on a timer for as long as the child is still
 * alive. Three outcomes:
 *
 *   still running        -> publish `applying` and keep polling
 *   gone                 -> run the gates and publish the real answer
 *   older than the
 *   apply timeout        -> stop waiting and judge it where it stands
 *
 * Judging is `ca_ucentral_done(0)` — the same function the live path uses, with
 * a zero exit code. That is not a guess: Gate 1 (the exit code) is unknowable
 * from here, and Gate 2 is the authoritative test anyway, because `ucentral.uc`
 * only moves `ucentral.active` at the end of a successful apply. A child that
 * died half way leaves the old symlink and is correctly reported as failed.
 */
static void ca_resume_evaluate(void)
{
    char pid_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d", s_ca.resume_pid);

    long age = (long)time(NULL) - s_ca.resume_started;
    bool alive = ca_pid_runs_ucentral(pid_str);

    if (alive && age < CONFIG_APPLY_TIMEOUT_SEC) {
        uloop_timeout_set(&s_ca.resume_t, CA_RESUME_POLL_SEC * 1000);
        return;
    }

    if (alive) {
        syslog(LOG_WARNING, "config_apply: inherited apply (pid %d, uuid %lu) "
                            "still running after %lds — judging it as it stands",
               s_ca.resume_pid, s_ca.uuid, age);
    } else {
        syslog(LOG_NOTICE, "config_apply: inherited apply (pid %d, uuid %lu) "
                           "has exited after %lds — checking the result",
               s_ca.resume_pid, s_ca.uuid, age);
    }

    /* Rewrites CA_STATE_PATH as a `done` record on the way out, via ca_finish(),
     * so the answer outlives this process if nobody reads it in time. */
    ca_ucentral_done(0);
}

static void ca_resume_tick_cb(struct uloop_timeout *t)
{
    (void)t;
    ca_resume_evaluate();
}

/**
 * Answer for an apply this process did not run: one started by a previous
 * instance of the daemon, or one that belonged to the boot before this one.
 *
 * Without this, A3 reads `{"status":"idle"}` after either event. The app is
 * waiting for a *terminal* state and `idle` is not one, so it waits for ever —
 * which is exactly the "reconnect and it still says applying" report, in both of
 * its forms:
 *
 *   daemon restarted mid-apply   routine here rather than exceptional, because
 *                                writing /etc/config/ble fires
 *                                `procd_add_reload_trigger "ble"` and that file
 *                                is restored to the package copy during an apply
 *                                (§19.9.9). The apply survives — deinit no
 *                                longer kills it — so all that was missing was
 *                                somebody to report for it.
 *
 *   device rebooted mid-apply    or just after it. The apply does *not* survive,
 *                                but its outcome is on disk either way, in the
 *                                `ucentral.active` symlink. What was missing was
 *                                a durable note of which uuid to ask about; see
 *                                CA_STATE_PATH for why that note used to live on
 *                                tmpfs and no longer does.
 *
 * Three cases, one function: a `done` record is replayed, a `pending` record from
 * an earlier boot is judged on the spot, and a `pending` record from this boot is
 * polled until its child exits. All three end in a terminal A3 value, which is
 * the whole point — §19.9.12.
 *
 * With one deliberate exception: replaying a `done` record is held back while
 * `gatt_server.enabled` is off, because consuming a one-shot answer on a start
 * where nothing can read A3 destroys it. See the branch for why that start is
 * the common case rather than a corner.
 */
static void ca_resume_from_state(void)
{
    char *txt = ca_slurp(CA_STATE_PATH, NULL);
    if (!txt)
        return;                     /* nothing in flight; the common case */

    /*
     * A finished apply whose result never reached anyone: replay it and drop the
     * record. One-shot by construction — it is published into A3, where it stays
     * for this daemon's lifetime, and the file is removed — so the answer
     * survives exactly one restart or reboot and a second one reports `idle`.
     * That is the honest bound: by then the question is stale.
     *
     * device_status is deliberately left at IDLE instead of being moved to
     * CFG_SUCCESS/CFG_FAILED. The advertised byte means "something is happening
     * on this device right now", and nothing is — the apply ended before this
     * process existed. Setting it would put 2 or 3 in the advertisement for
     * CA_STATUS_RESET_SEC and tell every scanning phone that an apply had just
     * completed. A3 carries the `uuid`, which is how the app correlates a result
     * with the apply it asked for (see ca_start_ucentral), so the phone that
     * cares gets its answer and the ones that do not are not misled.
     */
    if (strncmp(txt, CA_STATE_DONE, strlen(CA_STATE_DONE)) == 0) {
        /*
         * Do not spend the record on a start that has nobody to spend it on.
         *
         * Replay is one-shot, and this gate is what keeps "one-shot" from
         * meaning "burned". With `gatt_server.enabled` off there is no GATT
         * service and no advertising on either transport — gatt_server_start()
         * skips registration, and the TI path sends ADV_STOP — so no phone can
         * read A3, and publishing into it would consume the answer with nothing
         * on the other end.
         *
         * The ordering makes this necessary rather than defensive.
         * config_apply_init() runs from gatt_server_init(), which
         * app_plugins_init() calls at gatt_app.c:129; the `enabled` check does
         * not happen until gatt_server_start() at gatt_app.c:227. So without
         * this, the replay always runs first and always wins.
         *
         * It is the exact sequence that was reported from the field, and it is
         * not a corner: an apply restores /etc/config/ble to the package copy
         * (§19.9.9), the shipped value is `enabled '0'`, so the boot right after
         * a successful apply is precisely the one where GATT is off. Leaving the
         * record alone means it is still there on the next start — the one where
         * someone has turned GATT back on, which is the first start that can
         * actually deliver it.
         *
         * Only this branch is gated. The cross-boot `pending` branch below is
         * deliberately left to run with GATT off: it reads `ucentral.active`
         * while it is still fresh and stores the verdict as a `done` record, so
         * the answer is preserved for a later start rather than deferred. Gating
         * it would only move the same work.
         */
        if (!uci_app_get_bool("ble", "gatt_server", "enabled", false)) {
            syslog(LOG_NOTICE, "config_apply: holding the recorded result in %s "
                               "— gatt_server is disabled, so nothing could read "
                               "it from A3 yet", CA_STATE_PATH);
            free(txt);
            return;
        }

        unsigned long uuid = 0;
        char *res = strstr(txt, " result=");
        int got = sscanf(txt, CA_STATE_DONE "uuid=%lu", &uuid);

        if (got == 1 && res) {
            res += strlen(" result=");
            char *nl = strchr(res, '\n');
            if (nl)
                *nl = '\0';
            if (*res) {
                syslog(LOG_NOTICE, "config_apply: replaying the result of an "
                                   "apply that finished before this start "
                                   "(uuid %lu): %s", uuid, res);
                gatt_v2_result_set(res);
            }
        } else {
            syslog(LOG_WARNING, "config_apply: %s holds an unreadable result "
                                "record — discarding it", CA_STATE_PATH);
        }

        free(txt);
        unlink(CA_STATE_PATH);
        return;
    }

    unsigned long uuid = 0;
    int pid = 0;
    long started = 0;
    char boot[64] = {0};
    char cfg[128] = {0};

    int got = sscanf(txt, CA_STATE_PENDING
                     "uuid=%lu pid=%d started=%ld boot=%63s cfg=%127s",
                     &uuid, &pid, &started, boot, cfg);
    free(txt);

    if (got != 5 || pid <= 0 || !*cfg) {
        syslog(LOG_WARNING, "config_apply: %s is unreadable — discarding it",
               CA_STATE_PATH);
        unlink(CA_STATE_PATH);
        return;
    }

    char now_boot[64];
    bool cold = ca_boot_id(now_boot, sizeof(now_boot)) &&
                strcmp(boot, "unknown") != 0 &&
                strcmp(boot, now_boot) != 0;

    /*
     * `running` stays false on purpose, on both paths below. It means "this
     * daemon owns a child it can reap", and we own nothing — the process was
     * reparented to init, or died with the previous boot. What it gates is a
     * second apply being started, and ca_foreign_apply_running() covers that
     * case properly by looking at /proc.
     */
    s_ca.uuid           = uuid;
    s_ca.path           = CA_PATH_UCENTRAL;
    s_ca.resume_pid     = pid;
    s_ca.resume_started = started;
    snprintf(s_ca.cfg_path, sizeof(s_ca.cfg_path), "%s", cfg);

    /*
     * A record from an earlier boot: the apply is definitively over, whatever it
     * managed to do is already on disk, and the pid cannot be used to find out.
     * Judge it immediately instead of polling.
     *
     * Gate 2 does the work and does it correctly either way: `ucentral.uc` moves
     * `ucentral.active` only at the very end, so a reboot that landed mid-apply
     * leaves the old symlink and is reported failed, while one that landed just
     * after the commit finds the new symlink and is reported applied. Neither is
     * a guess — the symlink is the record `ucentral.uc` itself left behind.
     *
     * No `applying` is published on the way, unlike the warm path below: there is
     * nothing to be in progress, and publishing it would put a transient
     * non-terminal value on A3 immediately before the real answer.
     *
     * device_status *is* allowed to move here, which is the opposite of the
     * `done` replay above, and the asymmetry is intentional. There the outcome
     * was decided by a previous process and merely being relayed; here the gates
     * run now, in this process, so from the daemon's point of view an apply has
     * just reached a terminal state — which is exactly what the advertised byte
     * is for. It reverts after CA_STATUS_RESET_SEC either way.
     */
    if (cold) {
        syslog(LOG_NOTICE, "config_apply: an apply was in flight when the device "
                           "restarted (uuid %lu, %s) — the recorded pid %d belongs "
                           "to the previous boot, so judging it from the active "
                           "symlink now", uuid, s_ca.cfg_path, pid);
        ca_ucentral_done(0);
        return;
    }

    syslog(LOG_NOTICE, "config_apply: resuming report for an apply from a "
                       "previous instance (pid %d, uuid %lu, %s)",
           pid, uuid, s_ca.cfg_path);

    gatt_v2_set_device_status(GATT_V2_DS_CFG_APPLYING);
    gatt_v2_result_set("{\"status\":\"applying\"}");

    ca_resume_evaluate();
}

void config_apply_init(void)
{
    memset(&s_ca, 0, sizeof(s_ca));
    s_ca.kill_t.cb   = ca_kill_cb;
    s_ca.status_t.cb = ca_status_reset_cb;
    s_ca.resume_t.cb = ca_resume_tick_cb;
    syslog(LOG_INFO, "config_apply: initialized (timeout %ds)",
           CONFIG_APPLY_TIMEOUT_SEC);

    ca_resume_from_state();
}

void config_apply_deinit(void)
{
    uloop_timeout_cancel(&s_ca.kill_t);
    uloop_timeout_cancel(&s_ca.status_t);

    if (s_ca.running) {
        /*
         * Let it finish. We stop watching it; we do not kill it.
         *
         * This used to SIGKILL the child, with a comment explaining that a
         * half-finished apply must not be rolled back from here — and then
         * creating exactly that half-finished state by killing it mid-flight.
         * Measured on eap104: the apply died 22 s in, with `bridger` stopped and
         * the bridge being rebuilt, so the device was left with a dismantled
         * network *and* `ucentral.active` still pointing at the previous
         * configuration. `ucentral.active` had never once moved on that unit —
         * three applies, three orphaned cfg files.
         *
         * Killing is the wrong trade on this platform because the daemon being
         * stopped mid-apply is routine rather than exceptional: writing
         * /etc/config/ble fires `procd_add_reload_trigger "ble"`
         * (/etc/init.d/ble-provisiond:75), and that file is restored to the
         * package copy during an apply (§19.9.9). So the common case was a
         * self-inflicted half-apply.
         *
         * `ucentral.uc` is designed to run unattended — ec-standalone's init
         * script forks it detached at boot for exactly this reason — and
         * `ca_spawn()` now puts it in its own session, so losing the parent
         * costs it nothing. It finishes, moves the symlink, and leaves one
         * coherent configuration behind.
         *
         * The *result* is not given up with it. ca_start_ucentral() left the
         * uuid, pid and cfg path in CA_STATE_PATH, so the next instance picks
         * the apply back up in ca_resume_from_state() and runs the gates on its
         * behalf. Nothing is cleared here: the record has to outlive us, and
         * ca_finish() is the only thing that rewrites it — as a `done` record,
         * not by removing it, so the answer also outlives a reboot that happens
         * before anyone reads A3.
         *
         * The 300 s timeout still kills (ca_timeout), and should: that is a
         * child that has stopped making progress, not one we are abandoning.
         */
        syslog(LOG_WARNING, "config_apply: stopping with an apply in flight "
               "(pid %d) — leaving it to finish; %s carries its uuid so the "
               "next instance can report the result",
               (int)s_ca.proc.pid, CA_STATE_PATH);
        uloop_process_delete(&s_ca.proc);
        s_ca.running = false;
    }
}
