/**
 * @file config_provision.c
 * @brief uCentral configuration provisioning — BLE to ubus pass-through.
 *
 * Receives a complete uCentral JSON configuration from the BLE phone app,
 * performs minimal validation (is it valid JSON with expected top-level
 * structure?), then forwards it to ucentral-agent via ubus for apply.
 *
 * ble-provisiond does NOT interpret the config contents — it is purely a
 * transport bridge. The ucentral-agent handles:
 *   - Full schema validation
 *   - JSON → UCI conversion
 *   - Service reload
 *   - Rollback on failure
 *
 * This ensures the BLE provisioning path is identical to the cloud path:
 *   Cloud → WSS → ucentral-agent → UCI
 *   Phone → BLE → ble-provisiond → ubus → ucentral-agent → UCI
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>

#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

#include "config_provision.h"

/* ── Module State ── */

static struct {
    bool initialized;
    bool allow_config;
    struct ubus_context *ubus;
    uint32_t agent_id;          /* ubus object id for ucentral */
    bool agent_resolved;
    /* Async callback state */
    config_prov_done_cb pending_cb;
    void *pending_user_data;
} cp_ctx;

/* ── Constants ── */

#define UCENTRAL_UBUS_OBJECT    "ucentral"
#define UCENTRAL_APPLY_METHOD   "config"
#define UCENTRAL_STATUS_METHOD  "status"
#define APPLY_TIMEOUT_MS        30000   /* 30 seconds */

/* ── Internal Helpers ── */

/**
 * Resolve the ucentral-agent ubus object ID.
 * Caches the result for subsequent calls.
 */
static int resolve_agent(void)
{
    if (cp_ctx.agent_resolved)
        return 0;

    if (!cp_ctx.ubus) {
        cp_ctx.ubus = ubus_connect(NULL);
        if (!cp_ctx.ubus) {
            syslog(LOG_ERR, "config_prov: cannot connect to ubus");
            return -1;
        }
    }

    int ret = ubus_lookup_id(cp_ctx.ubus, UCENTRAL_UBUS_OBJECT,
                             &cp_ctx.agent_id);
    if (ret != 0) {
        syslog(LOG_ERR, "config_prov: ucentral-agent not found on ubus "
               "(is it running?)");
        return -1;
    }

    cp_ctx.agent_resolved = true;
    syslog(LOG_DEBUG, "config_prov: ucentral-agent resolved (id=%u)",
           cp_ctx.agent_id);
    return 0;
}

/**
 * Minimal validation of uCentral JSON structure.
 * Only checks that it's valid JSON and has expected top-level keys.
 * Full schema validation is done by ucentral-agent.
 */
static bool validate_json_structure(const char *json, size_t len)
{
    struct json_tokener *tok = json_tokener_new();
    if (!tok) return false;

    struct json_object *obj = json_tokener_parse_ex(tok, json, (int)len);
    enum json_tokener_error err = json_tokener_get_error(tok);
    json_tokener_free(tok);

    if (!obj || err != json_tokener_success) {
        syslog(LOG_WARNING, "config_prov: payload is not valid JSON");
        if (obj) json_object_put(obj);
        return false;
    }

    /* Check for at least one expected uCentral top-level key */
    bool has_radios = json_object_object_get_ex(obj, "radios", NULL);
    bool has_interfaces = json_object_object_get_ex(obj, "interfaces", NULL);
    bool has_uuid = json_object_object_get_ex(obj, "uuid", NULL);

    json_object_put(obj);

    if (!has_radios && !has_interfaces) {
        syslog(LOG_WARNING, "config_prov: JSON missing 'radios' and "
               "'interfaces' keys — not a valid uCentral config");
        return false;
    }

    (void)has_uuid; /* uuid is recommended but not strictly required */
    return true;
}

/**
 * ubus invoke callback — receives the apply result from ucentral-agent.
 */
static void apply_result_cb(struct ubus_request *req, int type,
                            struct blob_attr *msg)
{
    (void)req;
    (void)type;

    config_prov_status_t status = CONFIG_PROV_OK;
    const char *message = "Config applied successfully";

    if (!msg) {
        status = CONFIG_PROV_ERR_APPLY;
        message = "No response from agent";
    } else {
        /* Parse the response — look for status/error fields */
        enum { ATTR_STATUS, ATTR_MSG, __ATTR_MAX };
        static const struct blobmsg_policy pol[__ATTR_MAX] = {
            [ATTR_STATUS] = { .name = "status", .type = BLOBMSG_TYPE_INT32 },
            [ATTR_MSG]    = { .name = "message", .type = BLOBMSG_TYPE_STRING },
        };
        struct blob_attr *tb[__ATTR_MAX];
        blobmsg_parse(pol, __ATTR_MAX, tb, blob_data(msg), blob_len(msg));

        if (tb[ATTR_STATUS]) {
            int s = blobmsg_get_u32(tb[ATTR_STATUS]);
            if (s != 0) {
                status = CONFIG_PROV_ERR_APPLY;
                if (tb[ATTR_MSG])
                    message = blobmsg_get_string(tb[ATTR_MSG]);
                else
                    message = "Agent reported apply failure";
            }
        }
    }

    syslog(LOG_INFO, "config_prov: apply result: status=%d msg='%s'",
           status, message);

    /* Invoke the completion callback */
    if (cp_ctx.pending_cb) {
        cp_ctx.pending_cb(status, message, cp_ctx.pending_user_data);
        cp_ctx.pending_cb = NULL;
        cp_ctx.pending_user_data = NULL;
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ══════════════════════════════════════════════════════════════════ */

int config_provision_init(bool allow_config)
{
    memset(&cp_ctx, 0, sizeof(cp_ctx));
    cp_ctx.allow_config = allow_config;
    cp_ctx.initialized = true;

    syslog(LOG_INFO, "config_prov: initialized (allow_config=%s)",
           allow_config ? "yes" : "no");
    return 0;
}

void config_provision_deinit(void)
{
    if (cp_ctx.ubus) {
        ubus_free(cp_ctx.ubus);
        cp_ctx.ubus = NULL;
    }
    cp_ctx.initialized = false;
    cp_ctx.agent_resolved = false;
    syslog(LOG_DEBUG, "config_prov: deinitialized");
}

int config_provision_apply(const uint8_t *json_data, uint16_t json_len,
                           config_prov_done_cb done_cb, void *user_data)
{
    if (!cp_ctx.initialized)
        return CONFIG_PROV_ERR_AGENT;

    /* Check feature gate */
    if (!cp_ctx.allow_config) {
        syslog(LOG_WARNING, "config_prov: config provisioning disabled by UCI");
        if (done_cb)
            done_cb(CONFIG_PROV_ERR_DISABLED, "Config provisioning disabled",
                    user_data);
        return CONFIG_PROV_ERR_DISABLED;
    }

    if (!json_data || json_len == 0) {
        syslog(LOG_ERR, "config_prov: empty payload");
        if (done_cb)
            done_cb(CONFIG_PROV_ERR_INVALID, "Empty payload", user_data);
        return CONFIG_PROV_ERR_INVALID;
    }

    syslog(LOG_INFO, "config_prov: received config (%u bytes) via BLE",
           json_len);

    /* Null-terminate for JSON parsing */
    char *json_str = malloc(json_len + 1);
    if (!json_str) {
        if (done_cb)
            done_cb(CONFIG_PROV_ERR_NOMEM, "Out of memory", user_data);
        return CONFIG_PROV_ERR_NOMEM;
    }
    memcpy(json_str, json_data, json_len);
    json_str[json_len] = '\0';

    /* Minimal structure validation */
    if (!validate_json_structure(json_str, json_len)) {
        free(json_str);
        if (done_cb)
            done_cb(CONFIG_PROV_ERR_INVALID, "Invalid JSON structure",
                    user_data);
        return CONFIG_PROV_ERR_INVALID;
    }

    /* Resolve ucentral-agent on ubus */
    if (resolve_agent() != 0) {
        free(json_str);
        if (done_cb)
            done_cb(CONFIG_PROV_ERR_AGENT, "ucentral-agent not available",
                    user_data);
        return CONFIG_PROV_ERR_AGENT;
    }

    /* Build ubus message — forward the entire JSON as a string field */
    struct blob_buf buf = {};
    blob_buf_init(&buf, 0);
    blobmsg_add_string(&buf, "config", json_str);

    free(json_str);

    /* Store callback for async result */
    cp_ctx.pending_cb = done_cb;
    cp_ctx.pending_user_data = user_data;

    /* Invoke ucentral-agent config apply via ubus */
    int ret = ubus_invoke(cp_ctx.ubus, cp_ctx.agent_id,
                          UCENTRAL_APPLY_METHOD, buf.head,
                          apply_result_cb, NULL, APPLY_TIMEOUT_MS);

    blob_buf_free(&buf);

    if (ret != 0) {
        syslog(LOG_ERR, "config_prov: ubus invoke failed (ret=%d)", ret);
        cp_ctx.pending_cb = NULL;
        cp_ctx.pending_user_data = NULL;
        if (done_cb)
            done_cb(CONFIG_PROV_ERR_AGENT,
                    "ubus invoke to ucentral-agent failed", user_data);
        return CONFIG_PROV_ERR_AGENT;
    }

    syslog(LOG_INFO, "config_prov: config forwarded to ucentral-agent, "
           "awaiting result...");
    return CONFIG_PROV_OK;
}

int config_provision_get_current(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0)
        return -EINVAL;

    if (resolve_agent() != 0) {
        snprintf(buf, buf_size, "{\"error\":\"agent_unavailable\"}");
        return CONFIG_PROV_ERR_AGENT;
    }

    /* Call ucentral status to get current running config summary */
    struct blob_buf req = {};
    blob_buf_init(&req, 0);

    struct blob_attr *reply = NULL;
    int ret = ubus_invoke(cp_ctx.ubus, cp_ctx.agent_id,
                          UCENTRAL_STATUS_METHOD, req.head,
                          NULL, &reply, 5000);
    blob_buf_free(&req);

    if (ret != 0 || !reply) {
        snprintf(buf, buf_size, "{\"error\":\"cannot_get_status\"}");
        return CONFIG_PROV_ERR_AGENT;
    }

    /* Convert blob to JSON string */
    char *json = blobmsg_format_json(reply, true);
    if (json) {
        strncpy(buf, json, buf_size - 1);
        buf[buf_size - 1] = '\0';
        free(json);
    } else {
        snprintf(buf, buf_size, "{\"error\":\"format_failed\"}");
    }

    return 0;
}
