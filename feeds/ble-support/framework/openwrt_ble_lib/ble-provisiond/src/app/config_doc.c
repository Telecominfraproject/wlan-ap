/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file config_doc.c
 * @brief The A1 configuration document (§19.5.4). See config_doc.h.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

#include <json-c/json.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>

#include "config_doc.h"
#include "gatt_v2_read.h"

extern struct ubus_context *ubus_api_get_context(void);

#define CD_UCENTRAL_ACTIVE  "/etc/ucentral/ucentral.active"

/**
 * Timeout for `network.wireless status`.
 *
 * This call is on the read path, so the number is a compromise: netifd answers in
 * milliseconds when healthy, and a longer timeout would only mean stalling the
 * event loop for longer when it is not. One call per read *sequence*, not per
 * fragment — the snapshot hook is invoked once when the sequence starts.
 */
#define CD_UBUS_TIMEOUT_MS  1000

static struct {
    struct json_object *doc;    /* keeps the returned string alive */
} s_out;

/* ══════════════════════════════════════════════════════════════════
 *  Helpers
 * ══════════════════════════════════════════════════════════════════ */

/**
 * Copy @name from @src into @dst, or null when it is absent.
 *
 * Null rather than omitted because that is what the reference produces:
 * `iface.get("config", {}).get("ssid")` yields None, which `json.dumps` writes as
 * `null`. An app that distinguishes "absent" from "present but unset" would see a
 * different document if we dropped the key instead.
 */
static void copy_or_null(struct json_object *dst, const char *name,
                         struct json_object *src)
{
    struct json_object *v = NULL;

    if (src && json_object_object_get_ex(src, name, &v) && v)
        json_object_object_add(dst, name, json_object_get(v));
    else
        json_object_object_add(dst, name, NULL);
}

/** @src's @name, or @fallback (whose reference is consumed) when absent. */
static void copy_or_default(struct json_object *dst, const char *name,
                            struct json_object *src,
                            struct json_object *fallback)
{
    struct json_object *v = NULL;

    if (src && json_object_object_get_ex(src, name, &v) && v) {
        json_object_object_add(dst, name, json_object_get(v));
        json_object_put(fallback);
    } else {
        json_object_object_add(dst, name, fallback);
    }
}

/**
 * True when @doc carries a usable `interfaces`.
 *
 * Mirrors the reference's `if not data.get("interfaces")`, where a missing key,
 * null, and an empty array or object are all falsy and all mean "no uCentral
 * configuration, show the live topology instead".
 */
static bool has_interfaces(struct json_object *doc)
{
    struct json_object *v = NULL;

    if (!doc || !json_object_is_type(doc, json_type_object))
        return false;
    if (!json_object_object_get_ex(doc, "interfaces", &v) || !v)
        return false;

    if (json_object_is_type(v, json_type_array))
        return json_object_array_length(v) > 0;
    if (json_object_is_type(v, json_type_object))
        return json_object_object_length(v) > 0;

    return false;
}

/* ══════════════════════════════════════════════════════════════════
 *  Live topology, from netifd
 * ══════════════════════════════════════════════════════════════════ */

struct wl_ctx {
    struct json_object *obj;
};

static void wireless_cb(struct ubus_request *req, int type,
                        struct blob_attr *msg)
{
    (void)type;
    struct wl_ctx *ctx = req->priv;
    if (!msg)
        return;

    char *json = blobmsg_format_json(msg, true);
    if (!json)
        return;
    ctx->obj = json_tokener_parse(json);
    free(json);
}

/** One entry of a radio's `interfaces` array, reshaped for the app. */
static struct json_object *iface_object(struct json_object *iface)
{
    struct json_object *out = json_object_new_object();
    struct json_object *cfg = NULL;

    json_object_object_get_ex(iface, "config", &cfg);

    copy_or_null(out, "section", iface);
    copy_or_null(out, "ifname", iface);

    copy_or_null(out, "ssid", cfg);
    copy_or_null(out, "mode", cfg);
    copy_or_null(out, "encryption", cfg);
    /* These two have explicit defaults in the reference, unlike the rest. */
    copy_or_default(out, "key", cfg, json_object_new_string(""));
    copy_or_default(out, "disabled", cfg, json_object_new_boolean(0));

    copy_or_null(out, "up", iface);
    copy_or_null(out, "associated", iface);

    return out;
}

/**
 * `{"radios":[…]}` from `ubus call network.wireless status`.
 *
 * Note `band` is populated from the radio's `htmode`, which is what the reference
 * does. The name and the source disagree, but the app is the other half of this
 * contract and reads what the reference sends — so this copies the behaviour, not
 * the apparent intent.
 */
static struct json_object *topology_object(void)
{
    struct json_object *doc = json_object_new_object();
    struct json_object *radios = json_object_new_array();
    json_object_object_add(doc, "radios", radios);

    struct ubus_context *ubus = ubus_api_get_context();
    if (!ubus) {
        syslog(LOG_WARNING, "config_doc: no ubus connection — reporting no radios");
        return doc;
    }

    uint32_t id = 0;
    if (ubus_lookup_id(ubus, "network.wireless", &id) != 0) {
        syslog(LOG_WARNING, "config_doc: network.wireless not on ubus");
        return doc;
    }

    struct wl_ctx ctx = { NULL };
    int ret = ubus_invoke(ubus, id, "status", NULL, wireless_cb, &ctx,
                          CD_UBUS_TIMEOUT_MS);
    if (ret != 0 || !ctx.obj) {
        syslog(LOG_WARNING, "config_doc: network.wireless status failed (%d)", ret);
        if (ctx.obj)
            json_object_put(ctx.obj);
        return doc;
    }

    if (!json_object_is_type(ctx.obj, json_type_object)) {
        syslog(LOG_WARNING, "config_doc: network.wireless status is not an object");
        json_object_put(ctx.obj);
        return doc;
    }

    /* The reply is keyed by radio name: { "radio0": {...}, "radio1": {...} }. */
    json_object_object_foreach(ctx.obj, radio_name, radio) {
        if (!radio || !json_object_is_type(radio, json_type_object))
            continue;

        struct json_object *r = json_object_new_object();
        struct json_object *cfg = NULL;

        json_object_object_get_ex(radio, "config", &cfg);

        json_object_object_add(r, "radio", json_object_new_string(radio_name));

        /* `band` carries htmode — see the note above. */
        struct json_object *ht = NULL;
        if (cfg && json_object_object_get_ex(cfg, "htmode", &ht) && ht)
            json_object_object_add(r, "band", json_object_get(ht));
        else
            json_object_object_add(r, "band", NULL);

        copy_or_null(r, "channel", cfg);

        struct json_object *ifaces = json_object_new_array();
        json_object_object_add(r, "interfaces", ifaces);

        struct json_object *src = NULL;
        if (json_object_object_get_ex(radio, "interfaces", &src) &&
            json_object_is_type(src, json_type_array)) {
            size_t n = json_object_array_length(src);
            for (size_t i = 0; i < n; i++) {
                struct json_object *iface = json_object_array_get_idx(src, i);
                if (iface && json_object_is_type(iface, json_type_object))
                    json_object_array_add(ifaces, iface_object(iface));
            }
        }

        json_object_array_add(radios, r);
    }

    json_object_put(ctx.obj);
    return doc;
}

/* ══════════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════════ */

const char *config_doc_json(void)
{
    if (s_out.doc) {
        json_object_put(s_out.doc);
        s_out.doc = NULL;
    }

    struct json_object *doc = json_object_from_file(CD_UCENTRAL_ACTIVE);

    if (has_interfaces(doc)) {
        syslog(LOG_DEBUG, "config_doc: using %s", CD_UCENTRAL_ACTIVE);
    } else {
        if (doc) {
            /* Present but without interfaces — a telemetry-only or partial
             * configuration. The reference treats that the same as absent. */
            json_object_put(doc);
        }
        doc = topology_object();
        syslog(LOG_DEBUG, "config_doc: no uCentral configuration — reporting the "
               "live radio topology");
    }

    s_out.doc = doc;

    const char *s = json_object_to_json_string_ext(doc, JSON_C_TO_STRING_PLAIN);
    if (!s)
        return "{\"radios\":[]}";

    syslog(LOG_DEBUG, "config_doc: %zu bytes", strlen(s));
    return s;
}

void config_doc_init(void)
{
    memset(&s_out, 0, sizeof(s_out));
    gatt_v2_read_set_snapshot_fn(GATT_V2_TX_CONFIG, config_doc_json);
    syslog(LOG_INFO, "config_doc: A1 configuration document registered");
}

void config_doc_deinit(void)
{
    gatt_v2_read_set_snapshot_fn(GATT_V2_TX_CONFIG, NULL);
    if (s_out.doc) {
        json_object_put(s_out.doc);
        s_out.doc = NULL;
    }
}
