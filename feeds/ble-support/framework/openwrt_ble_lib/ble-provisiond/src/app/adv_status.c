/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file adv_status.c
 * @brief Advertisement manufacturer data for GATT provisioning v2 (§19.6).
 *
 * See adv_status.h for the byte layout.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

#include <libubus.h>
#include <libubox/blobmsg.h>

#include "adv_status.h"
#include "gatt_v2.h"
#include "../uci_app_config.h"

/*
 * How long an uplink lookup is reused. Matches the reference's 5 s cache, which
 * exists so a network restart cannot stall the event loop on repeated calls.
 */
#define ADV_UPLINK_CACHE_SEC   5

/* Bounded wait for the ubus reply. The call is local and answers in single-digit
 * milliseconds; the timeout only exists so a wedged netifd cannot hold the event
 * loop. */
#define ADV_UBUS_TIMEOUT_MS    300

/*
 * ubus context owner. This project has no ubus_api.h — main.c declares the
 * ubus_api entry points as externs — so the accessor is declared the same way
 * rather than introducing a header just for it.
 */
extern struct ubus_context *ubus_api_get_context(void);

static struct {
    uint16_t model_id;
    /*
     * The board this tag was resolved against, kept so it can be reported beside
     * model_id — see adv_status_board(). "" when /var/sysinfo/board_name is
     * unreadable.
     *
     * Deliberately not called "model": the start-up banner already prints
     * `Model: NRF52840` for the *radio chip* (ble_status_file.c), and on eap102
     * that sits a few lines from this board's `edgecore,eap102`. Two different
     * things called model in one log is how §19.9.7 cost three wrong hypotheses.
     *
     * "Where model_id came from" is deliberately *not* here. It is only needed
     * while init decides what to log, so it lives in a local — a struct field
     * would imply something outside this file reads it, and nothing does.
     */
    char     board[64];
    uint8_t  link;
    time_t   link_ts;
    bool     link_valid;
} s_adv;

/* ── uplink lookup ── */

/* Result carried through the blobmsg walk. */
struct uplink_scan {
    bool any_up_with_ip;
    bool any_up_without_ip;
};

/**
 * True for an interface name that is an uplink candidate.
 *
 * netifd names uplinks `up0v0`, `up1v0`, …; `up_none` is the placeholder for
 * "no uplink configured" and must not be counted.
 */
static bool is_uplink_name(const char *name)
{
    if (!name) return false;
    if (strncmp(name, "up", 2) != 0) return false;
    return strcmp(name, "up_none") != 0;
}

/** True when this interface table has at least one IPv4 address. */
static bool iface_has_ipv4(struct blob_attr *iface_tbl)
{
    struct blob_attr *cur;
    size_t rem;

    blobmsg_for_each_attr(cur, iface_tbl, rem) {
        const char *key = blobmsg_name(cur);
        if (!key || strcmp(key, "ipv4-address") != 0)
            continue;
        if (blobmsg_type(cur) != BLOBMSG_TYPE_ARRAY)
            return false;

        struct blob_attr *addr;
        size_t arem;
        blobmsg_for_each_attr(addr, cur, arem) {
            struct blob_attr *field;
            size_t frem;
            blobmsg_for_each_attr(field, addr, frem) {
                const char *fk = blobmsg_name(field);
                if (fk && strcmp(fk, "address") == 0 &&
                    blobmsg_type(field) == BLOBMSG_TYPE_STRING) {
                    const char *ip = blobmsg_get_string(field);
                    if (ip && ip[0])
                        return true;
                }
            }
        }
        return false;
    }
    return false;
}

/**
 * Walk every uplink candidate rather than trusting the first one in dump order.
 *
 * ubus dump order is not guaranteed, and a device with both a wired and a
 * wireless uplink can list the not-yet-carrying one first. Reporting that as
 * ground truth made the device show as Down in the scan list while another
 * interface was online — the reference hit exactly this (ECCLOUD-10049).
 */
static void uplink_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    (void)type;
    struct uplink_scan *scan = (struct uplink_scan *)req->priv;
    if (!msg || !scan)
        return;

    struct blob_attr *cur;
    size_t rem;
    blobmsg_for_each_attr(cur, msg, rem) {
        const char *key = blobmsg_name(cur);
        if (!key || strcmp(key, "interface") != 0)
            continue;
        if (blobmsg_type(cur) != BLOBMSG_TYPE_ARRAY)
            continue;

        struct blob_attr *iface;
        size_t irem;
        blobmsg_for_each_attr(iface, cur, irem) {
            const char *name = NULL;
            bool up = false;

            struct blob_attr *field;
            size_t frem;
            blobmsg_for_each_attr(field, iface, frem) {
                const char *fk = blobmsg_name(field);
                if (!fk) continue;
                if (strcmp(fk, "interface") == 0 &&
                    blobmsg_type(field) == BLOBMSG_TYPE_STRING)
                    name = blobmsg_get_string(field);
                else if (strcmp(fk, "up") == 0 &&
                         blobmsg_type(field) == BLOBMSG_TYPE_BOOL)
                    up = blobmsg_get_bool(field);
            }

            if (!is_uplink_name(name) || !up)
                continue;

            if (iface_has_ipv4(iface))
                scan->any_up_with_ip = true;
            else
                scan->any_up_without_ip = true;
        }
    }
}

/** Current uplink state, refreshed at most every ADV_UPLINK_CACHE_SEC. */
static uint8_t uplink_state(void)
{
    time_t now = time(NULL);
    if (s_adv.link_valid && (now - s_adv.link_ts) < ADV_UPLINK_CACHE_SEC)
        return s_adv.link;

    struct ubus_context *ctx = ubus_api_get_context();
    if (!ctx) {
        /* ubus not up yet (main.c retries on start-up). Report down rather than
         * inventing a state, and do NOT cache — the next call should retry. */
        return ADV_LINK_DOWN;
    }

    uint32_t id = 0;
    if (ubus_lookup_id(ctx, "network.interface", &id) != 0) {
        syslog(LOG_DEBUG, "adv_status: network.interface not on ubus");
        return ADV_LINK_DOWN;
    }

    struct uplink_scan scan = { false, false };
    int ret = ubus_invoke(ctx, id, "dump", NULL, uplink_cb, &scan,
                          ADV_UBUS_TIMEOUT_MS);
    if (ret != 0) {
        syslog(LOG_DEBUG, "adv_status: network.interface dump failed (%d)", ret);
        /*
         * Hold the last known state — a query we could not complete is not
         * evidence that the link went down — and back off.
         *
         * The back-off is the part that was missing: without touching link_ts the
         * cache stays expired, so every later call re-invokes and pays up to
         * ADV_UBUS_TIMEOUT_MS again. That is exactly the wrong behaviour in the
         * situation that produces the failure, which is netifd being busy
         * re-rendering the network (observed on eap115 right after a switch to
         * standalone mode, ret=7 = UBUS_STATUS_TIMEOUT). Treat the stale value as
         * good for one cache period so a busy netifd costs one timeout rather than
         * one per call, all of them on the event loop.
         */
        if (s_adv.link_valid) {
            s_adv.link_ts = now;
            return s_adv.link;
        }
        return ADV_LINK_DOWN;
    }

    uint8_t link = ADV_LINK_DOWN;
    if (scan.any_up_with_ip)
        link = ADV_LINK_UP;
    else if (scan.any_up_without_ip)
        link = ADV_LINK_CONNECTING;

    if (!s_adv.link_valid || link != s_adv.link)
        syslog(LOG_DEBUG, "adv_status: uplink %u -> %u",
               s_adv.link_valid ? s_adv.link : 0u, link);

    s_adv.link = link;
    s_adv.link_ts = now;
    s_adv.link_valid = true;
    return link;
}

/* ── Public API ── */

/*
 * Per-SKU tags, table of record ECCLOUD-11895. One tag per board_name; variants
 * that share a board (-L, -H) share its tag. The phone app carries the same table.
 */
static const struct { const char *board; uint16_t id; } k_model_ids[] = {
    { "edgecore,eap101", 10 }, { "edgecore,eap102", 11 }, { "edgecore,eap104", 12 },
    { "edgecore,eap105", 13 }, { "edgecore,eap111", 14 }, { "edgecore,eap112", 15 },
    { "edgecore,eap115", 16 }, { "edgecore,eap115a", 17 }, { "edgecore,oap101", 18 },
    { "edgecore,oap101e", 19 }, { "edgecore,oap103", 20 },
};

/** Read /var/sysinfo/board_name into s_adv.board, or leave it empty. */
static void read_board_name(void)
{
    s_adv.board[0] = '\0';

    FILE *fp = fopen("/var/sysinfo/board_name", "r");
    if (!fp)
        return;
    if (!fgets(s_adv.board, sizeof(s_adv.board), fp))
        s_adv.board[0] = '\0';
    fclose(fp);

    /* Only \r\n, because the table below is matched with strcmp() on the whole
     * line. Trimming more here would silently accept a board_name this function
     * is meant to reject. */
    s_adv.board[strcspn(s_adv.board, "\r\n")] = '\0';
}

/**
 * Tag for s_adv.board, or 0 when it is not in the table.
 *
 * A pure lookup: it does not log and does not substitute a fallback, because the
 * caller needs the "not found" answer for two different purposes and only one of
 * them is a problem. Reporting from in here instead produced a line that was
 * simply untrue when an override was in effect — it announced "advertising 3"
 * while the device advertised the override.
 *
 * 0 is safe as the not-found signal: no board carries it, and the one place 0 is
 * meaningful (an integrator setting it to suppress the device) never reaches this
 * function, because that is an override and overrides are not derived.
 */
static uint16_t board_model_id(void)
{
    for (size_t i = 0; i < sizeof(k_model_ids) / sizeof(k_model_ids[0]); i++)
        if (strcmp(s_adv.board, k_model_ids[i].board) == 0)
            return k_model_ids[i].id;
    return 0;
}

const char *adv_status_board(void)
{
    return s_adv.board;
}

uint16_t adv_status_model_id(void)
{
    return s_adv.model_id;
}

int adv_status_init(void)
{
    memset(&s_adv, 0, sizeof(s_adv));

    read_board_name();

    const char *board = s_adv.board[0] ? s_adv.board : "unknown";
    uint16_t    derived = board_model_id();      /* 0 = board not in the table */

    /* The shipped uci value is the legacy 3, which means "derive from the board";
     * any other explicit value is an integrator override and is used as is. */
    s_adv.model_id = (uint16_t)uci_app_get_int("ble", "gatt_server", "model_id",
                                               ADV_MODEL_ID_DEFAULT, 0, 65535);
    const bool is_derived = (s_adv.model_id == ADV_MODEL_ID_DEFAULT);
    if (is_derived && derived != 0)
        s_adv.model_id = derived;

    /*
     * Name the board and say where the number came from, in one line.
     *
     * `model_id=12` on its own cannot be read back to a board, and the mapping
     * is only 1:1 while nobody overrides it — an integrator who sets 12 on an
     * eap102 would produce a line that reads exactly like a derived eap104. The
     * log exists to diagnose "the app cannot see this device", so the one thing
     * it must not do is support a confident wrong inference.
     *
     * Four cases, kept apart because three of them are fine and the reader has to
     * be able to tell which one this is. The override forms print what the board
     * would have derived, because an override masking the right answer is the
     * interesting case.
     */
    if (s_adv.model_id == 0) {
        syslog(LOG_WARNING, "adv_status: model_id is 0 (board %s) — the phone app "
               "filters its scan list on this tag and will not recognise this "
               "device", board);
    } else if (is_derived && derived == 0) {
        syslog(LOG_WARNING, "adv_status: board '%s' has no model_id tag, "
               "advertising the %u fallback — the phone app will not recognise "
               "this device as one of its own", board, s_adv.model_id);
    } else if (is_derived) {
        syslog(LOG_INFO, "adv_status: advertising model_id=%u (derived from board %s)",
               s_adv.model_id, board);
    } else if (derived != 0) {
        syslog(LOG_INFO, "adv_status: advertising model_id=%u (set in "
               "ble.gatt_server.model_id; board %s derives %u)",
               s_adv.model_id, board, derived);
    } else {
        syslog(LOG_INFO, "adv_status: advertising model_id=%u (set in "
               "ble.gatt_server.model_id; board %s has no tag of its own)",
               s_adv.model_id, board);
    }

    return 0;
}

void adv_status_deinit(void)
{
    memset(&s_adv, 0, sizeof(s_adv));
}

int adv_status_mfr_data(uint8_t out[ADV_MFR_DATA_LEN])
{
    if (!out)
        return 0;

    out[0] = uplink_state();
    out[1] = gatt_v2_device_status();
    out[2] = (uint8_t)(s_adv.model_id & 0xFF);
    out[3] = (uint8_t)((s_adv.model_id >> 8) & 0xFF);

    return ADV_MFR_DATA_LEN;
}
