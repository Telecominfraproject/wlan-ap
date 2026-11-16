/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file status_doc.c
 * @brief The A4 device-status document (§19.5). See status_doc.h.
 *
 * The one thing that shapes this file more than anything else: the daemon is a
 * single-threaded uloop, so nothing on the read path may block. The reference is
 * asyncio and can afford `subprocess.run(state.uc, timeout=10)` inline; here that
 * same call would stall BLE, D-Bus and the advertisement timer for as long as it
 * takes. So `state.uc` runs as a child under uloop_process and the read serves
 * the last result, and everything else that costs a process is read once at init.
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
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>

#include "status_doc.h"
#include "gatt_v2.h"
#include "gatt_v2_read.h"

/* Provided by ubus_api.c — the daemon's single shared ubus connection. */
extern struct ubus_context *ubus_api_get_context(void);

/* ── Sources, all fixed by the reference ── */

#define SD_HEALTH_PATH       "/tmp/ucentral.health"
#define SD_CAPABILITIES_PATH "/etc/ucentral/capabilities.json"
#define SD_STATE_CACHE       "/tmp/ucentral.state"
#define SD_STATE_UC          "/usr/share/ucentral/state.uc"

/* Standalone is marked by a flag file; the legacy name is still honoured. */
#define SD_STANDALONE_FLAG   "/etc/ucentral/.provisioned"
#define SD_LEGACY_FLAG       "/etc/ucentral/.ble_provisioned"

/** Where we capture state.uc's stdout. Our own file, not ucentral's. */
#define SD_STATE_TMP         "/tmp/ble-prov-state.json"

/**
 * How old the cached state may be before a read triggers a refresh.
 *
 * Matches the reference's `_STATE_REFRESH_INTERVAL`. The app polls A4 while its
 * dashboard is open, so a refresh per 10 s of polling keeps uptime and client
 * counts moving without running a child per read.
 */
#define SD_STATE_REFRESH_SEC 10

/**
 * How long state.uc gets before it is killed.
 *
 * The reference passes `timeout=10` to subprocess.run. Here the timeout matters
 * more than there: a child that never exits would leave `s_state.running` set and
 * the cache would freeze for the life of the daemon.
 */
#define SD_STATE_TIMEOUT_SEC 10

/**
 * Timeout for `system board`.
 *
 * Deliberately short. The call is trivial and answers in microseconds on a
 * healthy ubus, and this is the one lookup that can still happen on a read — if
 * ubus was not up when init ran, the next read retries. A generous timeout there
 * would stall the event loop for the whole of it.
 */
#define SD_UBUS_TIMEOUT_MS   1000

/* ── Cached state ── */

static struct {
    struct json_object *board;      /* `ubus call system board`, never changes */
    char serial[64];                /* fw_printenv SN, never changes */
} s_once;

static struct {
    struct json_object *doc;        /* last built document, keeps its string alive */
} s_out;

static struct {
    struct json_object  *value;     /* last good state object */
    time_t               ts;        /* when `value` was produced, 0 = never */
    bool                 running;
    struct uloop_process proc;
    struct uloop_timeout kill_t;
} s_state;

/* ══════════════════════════════════════════════════════════════════
 *  Small helpers
 * ══════════════════════════════════════════════════════════════════ */

/**
 * Parse a JSON file, or return an empty object.
 *
 * Absent, unreadable and malformed all collapse to `{}` on purpose: the app must
 * render a dashboard on a device that has never been online, where neither
 * ucentral.health nor capabilities.json exists yet.
 */
static struct json_object *json_file_or_empty(const char *path)
{
    struct json_object *o = json_object_from_file(path);
    if (!o)
        return json_object_new_object();

    /* A file holding a bare scalar would break the document's shape. */
    if (!json_object_is_type(o, json_type_object)) {
        syslog(LOG_WARNING, "status_doc: %s is not a JSON object — ignoring", path);
        json_object_put(o);
        return json_object_new_object();
    }
    return o;
}

/** True when @path exists. */
static bool path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* ══════════════════════════════════════════════════════════════════
 *  info
 * ══════════════════════════════════════════════════════════════════ */

struct board_ctx {
    struct json_object *obj;
};

static void board_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    (void)type;
    struct board_ctx *ctx = req->priv;
    if (!msg)
        return;

    char *json = blobmsg_format_json(msg, true);
    if (!json)
        return;
    ctx->obj = json_tokener_parse(json);
    free(json);
}

/**
 * `ubus call system board`, cached forever.
 *
 * Tried at init and, if ubus was not up yet, again on the next read — hence
 * `board_tried` being reset rather than latched on failure.
 */
static struct json_object *board_info(void)
{
    if (s_once.board)
        return s_once.board;

    struct ubus_context *ubus = ubus_api_get_context();
    if (!ubus)
        return NULL;

    uint32_t id = 0;
    if (ubus_lookup_id(ubus, "system", &id) != 0) {
        syslog(LOG_DEBUG, "status_doc: 'system' not on ubus yet");
        return NULL;
    }

    struct board_ctx ctx = { NULL };
    int ret = ubus_invoke(ubus, id, "board", NULL, board_cb, &ctx,
                          SD_UBUS_TIMEOUT_MS);
    if (ret != 0 || !ctx.obj) {
        syslog(LOG_WARNING, "status_doc: 'system board' failed (%d)", ret);
        if (ctx.obj)
            json_object_put(ctx.obj);
        return NULL;
    }

    s_once.board = ctx.obj;
    return s_once.board;
}

/**
 * Read the label serial from the u-boot environment.
 *
 * Deliberately not ucentral's own `serial`: that is the MAC, which is the cloud
 * device identity rather than the number printed on the box (ECCLOUD-10898).
 *
 * Runs once, from init, before the event loop — a fork+exec on the read path
 * would stall every other event for as long as reading the env partition takes.
 */
static void serial_load(void)
{
    s_once.serial[0] = '\0';

    FILE *fp = popen("fw_printenv -n SN 2>/dev/null", "r");
    if (!fp) {
        syslog(LOG_DEBUG, "status_doc: cannot run fw_printenv");
        return;
    }
    if (fgets(s_once.serial, sizeof(s_once.serial), fp)) {
        size_t n = strlen(s_once.serial);
        while (n > 0 && (s_once.serial[n - 1] == '\n' ||
                         s_once.serial[n - 1] == '\r' ||
                         isspace((unsigned char)s_once.serial[n - 1])))
            s_once.serial[--n] = '\0';
    }
    pclose(fp);

    if (s_once.serial[0])
        syslog(LOG_INFO, "status_doc: serial %s", s_once.serial);
    else
        syslog(LOG_INFO, "status_doc: no SN in the u-boot environment");
}

/** The MAC as the app wants it in `info`: colon-separated, upper case. */
static void mac_colons(char *out, size_t out_sz)
{
    char hex[16] = "";
    out[0] = '\0';

    if (gatt_v2_mac_hex(hex, sizeof(hex)) != 12)
        return;

    /*
     * ucentral.state keeps the MAC at the top level and get_state() drops it, so
     * this copy in `info` is the only one the app sees.
     */
    snprintf(out, out_sz, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
             toupper((unsigned char)hex[0]),  toupper((unsigned char)hex[1]),
             toupper((unsigned char)hex[2]),  toupper((unsigned char)hex[3]),
             toupper((unsigned char)hex[4]),  toupper((unsigned char)hex[5]),
             toupper((unsigned char)hex[6]),  toupper((unsigned char)hex[7]),
             toupper((unsigned char)hex[8]),  toupper((unsigned char)hex[9]),
             toupper((unsigned char)hex[10]), toupper((unsigned char)hex[11]));
}

static struct json_object *info_object(void)
{
    struct json_object *info = json_object_new_object();
    struct json_object *board = board_info();

    if (board) {
        struct json_object *v = NULL;

        if (json_object_object_get_ex(board, "model", &v))
            json_object_object_add(info, "model",
                                   json_object_new_string(
                                       json_object_get_string(v)));

        /*
         * Prefer the TIP-pinned tag (e.g. "v4.2.5-20260609BLE-ec") over the
         * OpenWrt distribution version. The dashboard wants the build suffix, not
         * just the semver triplet — same choice the LAN-side ec-info CGI makes.
         */
        struct json_object *rel = NULL;
        if (json_object_object_get_ex(board, "release", &rel)) {
            const char *ver = NULL;
            if (json_object_object_get_ex(rel, "tip-version", &v))
                ver = json_object_get_string(v);
            if ((!ver || !ver[0]) &&
                json_object_object_get_ex(rel, "version", &v))
                ver = json_object_get_string(v);
            if (ver && ver[0])
                json_object_object_add(info, "ver",
                                       json_object_new_string(ver));
        }
    }

    if (s_once.serial[0])
        json_object_object_add(info, "serial",
                               json_object_new_string(s_once.serial));

    char mac[24];
    mac_colons(mac, sizeof(mac));
    if (mac[0])
        json_object_object_add(info, "mac", json_object_new_string(mac));

    return info;
}

/* ══════════════════════════════════════════════════════════════════
 *  mode + cloud_active
 * ══════════════════════════════════════════════════════════════════ */

/**
 * The operator's persisted intent, not whether the cloud client happens to be up.
 *
 * `pidof ucentral` used to be the answer and it lies on exactly the setup people
 * provision from: with no uplink, cloud_discover never gets a valid clock, so it
 * never starts ucentral, so a cloud-managed AP reads as standalone. Operators
 * then configure it believing it is theirs, the cable goes in, and the controller
 * pushes its own configuration over the lot (ECCLOUD-10760).
 */
static const char *mode_string(void)
{
    if (path_exists(SD_STANDALONE_FLAG) || path_exists(SD_LEGACY_FLAG))
        return "standalone";
    return "cloud";
}

/**
 * Whether the ucentral client is really running — i.e. the controller really has
 * this device. Stays false in cloud mode with no uplink, which is the point.
 *
 * Walks /proc instead of running `pidof`: this is on the read path, and a fork
 * per read to answer a question /proc already answers would be wasteful.
 */
static bool cloud_active(void)
{
    DIR *d = opendir("/proc");
    if (!d)
        return false;

    bool found = false;
    struct dirent *e;
    while (!found && (e = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0]))
            continue;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);

        FILE *fp = fopen(path, "r");
        if (!fp)
            continue;                /* the process exited between readdir and now */

        char comm[32] = "";
        if (fgets(comm, sizeof(comm), fp)) {
            size_t n = strlen(comm);
            if (n > 0 && comm[n - 1] == '\n')
                comm[n - 1] = '\0';
            /* `comm` is the 15-char-truncated name; "ucentral" fits exactly. */
            if (strcmp(comm, "ucentral") == 0)
                found = true;
        }
        fclose(fp);
    }
    closedir(d);
    return found;
}

/* ══════════════════════════════════════════════════════════════════
 *  state — asynchronous, cached
 * ══════════════════════════════════════════════════════════════════ */

static void state_finish(void)
{
    s_state.running = false;
    uloop_timeout_cancel(&s_state.kill_t);
}

static void state_proc_cb(struct uloop_process *p, int ret)
{
    (void)p;
    state_finish();

    if (ret != 0) {
        /*
         * state.uc dies on some configurations (mesh). A stale state beats none,
         * so the cache is left alone rather than cleared.
         */
        syslog(LOG_WARNING, "status_doc: state.uc exited %d — keeping the "
               "previous state", ret);
        unlink(SD_STATE_TMP);
        return;
    }

    struct json_object *o = json_object_from_file(SD_STATE_TMP);
    unlink(SD_STATE_TMP);

    if (!o || !json_object_is_type(o, json_type_object)) {
        syslog(LOG_WARNING, "status_doc: state.uc produced no usable JSON");
        if (o)
            json_object_put(o);
        return;
    }

    if (s_state.value)
        json_object_put(s_state.value);
    s_state.value = o;
    s_state.ts = time(NULL);
    syslog(LOG_DEBUG, "status_doc: state refreshed");
}

static void state_kill_cb(struct uloop_timeout *t)
{
    (void)t;
    if (!s_state.running)
        return;

    syslog(LOG_WARNING, "status_doc: state.uc still running after %ds — killing "
           "pid %d", SD_STATE_TIMEOUT_SEC, (int)s_state.proc.pid);
    kill(s_state.proc.pid, SIGKILL);
    /* state_proc_cb still fires with the signal status; it does the cleanup. */
}

/**
 * Run state.uc as a child, capturing stdout to a file.
 *
 * Writing to a file rather than a pipe keeps the parent out of the business of
 * draining a fd while the child is still writing — the same shape
 * gatt_v2_upgrade.c uses for `sysupgrade -T`.
 *
 * Invoking the script directly (not via `ucode`) matches the reference's
 * `subprocess.run(["/usr/share/ucentral/state.uc"], capture_output=True)`: it
 * prints the state object bare on stdout, which is what we want to embed. As a
 * side effect it also refreshes /tmp/ucentral.state, which is what the LAN CGI
 * and our own cold-start fallback read (ECCLOUD-9958).
 */
static void state_refresh(void)
{
    if (s_state.running)
        return;
    if (!path_exists(SD_STATE_UC))
        return;

    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "status_doc: fork failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        int fd = open(SD_STATE_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            if (fd > STDERR_FILENO)
                close(fd);
        }
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0) {
            dup2(null, STDERR_FILENO);
            if (null > STDERR_FILENO)
                close(null);
        }
        execl(SD_STATE_UC, SD_STATE_UC, (char *)NULL);
        _exit(127);
    }

    s_state.proc.pid = pid;
    s_state.proc.cb  = state_proc_cb;
    uloop_process_add(&s_state.proc);
    s_state.running = true;

    s_state.kill_t.cb = state_kill_cb;
    uloop_timeout_set(&s_state.kill_t, SD_STATE_TIMEOUT_SEC * 1000);
}

/**
 * The state to embed, kicking off a refresh when the cache has aged out.
 *
 * The read is never delayed by the refresh: this returns what we have and the
 * child updates the cache for the next read. A first read before any refresh has
 * completed falls back to /tmp/ucentral.state, which cloud-mode devices have
 * because ucentral-state writes it.
 */
static struct json_object *state_object(void)
{
    time_t now = time(NULL);

    if (s_state.ts == 0 || (now - s_state.ts) >= SD_STATE_REFRESH_SEC)
        state_refresh();

    if (s_state.value)
        return json_object_get(s_state.value);   /* borrow: bump the refcount */

    /*
     * Cold start. The cache file wraps the object in {"state":{...}}, unlike
     * state.uc's bare stdout, so the member has to be unwrapped.
     */
    struct json_object *file = json_object_from_file(SD_STATE_CACHE);
    if (file) {
        struct json_object *inner = NULL;
        if (json_object_object_get_ex(file, "state", &inner) &&
            json_object_is_type(inner, json_type_object)) {
            struct json_object *ret = json_object_get(inner);
            json_object_put(file);
            return ret;
        }
        json_object_put(file);
    }

    return json_object_new_object();
}

/* ══════════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════════ */

const char *status_doc_json(void)
{
    /*
     * The previous document is freed here rather than at the end of the build:
     * json-c owns the string we returned last time, and the caller may still have
     * been holding it until now.
     */
    if (s_out.doc) {
        json_object_put(s_out.doc);
        s_out.doc = NULL;
    }

    struct json_object *doc = json_object_new_object();
    if (!doc)
        return "{}";

    /* Each computed once: cloud_active() walks /proc, so calling it again just
     * for the log line would double the cost of every read. */
    const char *mode = mode_string();
    bool cloud = cloud_active();

    /* Key order mirrors the reference's dict so the two are diffable. */
    json_object_object_add(doc, "info", info_object());
    json_object_object_add(doc, "health", json_file_or_empty(SD_HEALTH_PATH));
    json_object_object_add(doc, "state", state_object());
    json_object_object_add(doc, "mode", json_object_new_string(mode));
    json_object_object_add(doc, "cloud_active", json_object_new_boolean(cloud));
    json_object_object_add(doc, "capabilities",
                           json_file_or_empty(SD_CAPABILITIES_PATH));

    s_out.doc = doc;

    const char *s = json_object_to_json_string_ext(doc, JSON_C_TO_STRING_PLAIN);
    if (!s)
        return "{}";

    syslog(LOG_DEBUG, "status_doc: %zu bytes (mode=%s cloud_active=%s state=%s)",
           strlen(s), mode, cloud ? "yes" : "no",
           s_state.ts ? "live" : "cold");
    return s;
}

void status_doc_init(void)
{
    memset(&s_once, 0, sizeof(s_once));
    memset(&s_out, 0, sizeof(s_out));
    memset(&s_state, 0, sizeof(s_state));

    /* Both of these are free here and expensive later: init runs before the
     * event loop, so blocking costs nothing. */
    serial_load();
    board_info();

    /*
     * Warm the state cache immediately. A phone is seconds away at best, so by
     * the time it reads A4 the child has long finished and the first read carries
     * real state instead of the cold-start fallback.
     */
    state_refresh();

    gatt_v2_read_set_snapshot_fn(GATT_V2_TX_STATUS, status_doc_json);
    syslog(LOG_INFO, "status_doc: A4 status document registered");
}

void status_doc_deinit(void)
{
    gatt_v2_read_set_snapshot_fn(GATT_V2_TX_STATUS, NULL);

    if (s_state.running) {
        uloop_timeout_cancel(&s_state.kill_t);
        kill(s_state.proc.pid, SIGKILL);
        uloop_process_delete(&s_state.proc);
        s_state.running = false;
    }
    unlink(SD_STATE_TMP);

    if (s_state.value) {
        json_object_put(s_state.value);
        s_state.value = NULL;
    }
    if (s_once.board) {
        json_object_put(s_once.board);
        s_once.board = NULL;
    }
    if (s_out.doc) {
        json_object_put(s_out.doc);
        s_out.doc = NULL;
    }
}
