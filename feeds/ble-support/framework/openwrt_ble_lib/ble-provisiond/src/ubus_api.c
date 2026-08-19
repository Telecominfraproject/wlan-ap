/**
 * @file ubus_api.c
 * @brief ubus API for ble-provisiond.
 *
 * Exposes BLE operations via ubus methods and publishes events.
 * All BLE calls go through the libble public API (ble.h).
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <ble.h>
#include "app/scan_filter.h"
#include "app/scan_file.h"

static struct ubus_context *ubus_ctx;
static struct blob_buf b;

/* ── scan_start ── */
enum { SCAN_DURATION, SCAN_ACTIVE, SCAN_FILTER_DUP, SCAN_FILTER_TYPE, __SCAN_MAX };
static const struct blobmsg_policy scan_policy[__SCAN_MAX] = {
    [SCAN_DURATION]    = { .name = "duration",   .type = BLOBMSG_TYPE_INT32 },
    [SCAN_ACTIVE]      = { .name = "active",     .type = BLOBMSG_TYPE_BOOL },
    [SCAN_FILTER_DUP]  = { .name = "filter_dup", .type = BLOBMSG_TYPE_BOOL },
    [SCAN_FILTER_TYPE] = { .name = "filter",     .type = BLOBMSG_TYPE_STRING },
};

/* Active scan state (shared with app_plugins_init for autostart) */
uint32_t active_scan_filter = SCAN_FILTER_ALL;
scan_file_ctx_t active_scan_ctx;
char active_scan_filter_name[32] = "all";

static int ubus_scan_start(struct ubus_context *ctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg)
{
    (void)obj; (void)method;
    struct blob_attr *tb[__SCAN_MAX];
    blobmsg_parse(scan_policy, __SCAN_MAX, tb, blob_data(msg), blob_len(msg));

    uint32_t duration = tb[SCAN_DURATION] ? blobmsg_get_u32(tb[SCAN_DURATION]) : 10000;
    bool active = tb[SCAN_ACTIVE] ? blobmsg_get_bool(tb[SCAN_ACTIVE]) : true;
    bool filter_dup = tb[SCAN_FILTER_DUP] ? blobmsg_get_bool(tb[SCAN_FILTER_DUP]) : true;

    /* Parse filter type */
    const char *filter_str = tb[SCAN_FILTER_TYPE] ?
                             blobmsg_get_string(tb[SCAN_FILTER_TYPE]) : "all";
    active_scan_filter = scan_filter_parse(filter_str);
    strncpy(active_scan_filter_name, filter_str, sizeof(active_scan_filter_name) - 1);

    /* Read max_records and on_limit from UCI */
    extern int uci_app_get_string(const char *, const char *, const char *,
                                  char *, size_t, const char *);
    char mr_str[16] = "1000", ol_str[16] = "rotate";
    uci_app_get_string("ble", "scan", "max_records", mr_str, sizeof(mr_str), "1000");
    uci_app_get_string("ble", "scan", "on_limit", ol_str, sizeof(ol_str), "rotate");
    uint32_t max_records = (uint32_t)atoi(mr_str);
    scan_limit_action_t on_limit = scan_file_parse_limit_action(ol_str);

    /* Open scan result file */
    scan_file_close(&active_scan_ctx);
    scan_file_open(&active_scan_ctx, active_scan_filter_name, max_records, on_limit);

    int ret = ble_scan_start(duration, active, filter_dup);
    blob_buf_init(&b, 0);
    if (ret == BLE_OK) {
        blobmsg_add_string(&b, "status", "scanning");
        blobmsg_add_u32(&b, "duration", duration);
        blobmsg_add_string(&b, "filter", filter_str);
    } else {
        blobmsg_add_string(&b, "error", ble_strerror(ret));
    }
    ubus_send_reply(ctx, req, b.head);
    return (ret == BLE_OK) ? UBUS_STATUS_OK : UBUS_STATUS_UNKNOWN_ERROR;
}

static int ubus_scan_stop(struct ubus_context *ctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg)
{
    (void)obj; (void)method; (void)msg;
    ble_scan_stop();

    /* Close scan result file */
    scan_file_close(&active_scan_ctx);

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "stopped");
    ubus_send_reply(ctx, req, b.head);
    return UBUS_STATUS_OK;
}

/* ── beacon_start ── */
enum { BCN_UUID, BCN_MAJOR, BCN_MINOR, BCN_TX, BCN_INTERVAL, __BCN_MAX };
static const struct blobmsg_policy beacon_policy[__BCN_MAX] = {
    [BCN_UUID]     = { .name = "uuid",     .type = BLOBMSG_TYPE_STRING },
    [BCN_MAJOR]    = { .name = "major",    .type = BLOBMSG_TYPE_INT32 },
    [BCN_MINOR]    = { .name = "minor",    .type = BLOBMSG_TYPE_INT32 },
    [BCN_TX]       = { .name = "tx_power", .type = BLOBMSG_TYPE_INT32 },
    [BCN_INTERVAL] = { .name = "interval", .type = BLOBMSG_TYPE_INT32 },
};

static int ubus_beacon_start(struct ubus_context *ctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    (void)obj; (void)method;
    struct blob_attr *tb[__BCN_MAX];
    blobmsg_parse(beacon_policy, __BCN_MAX, tb, blob_data(msg), blob_len(msg));

    ble_beacon_config_t config;
    memset(&config, 0, sizeof(config));
    snprintf(config.uuid, sizeof(config.uuid), "%.36s", "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0");
    config.major = 1; config.minor = 1;
    config.tx_power = -59; config.interval_ms = 100;

    if (tb[BCN_UUID])
        snprintf(config.uuid, sizeof(config.uuid), "%s", blobmsg_get_string(tb[BCN_UUID]));
    if (tb[BCN_MAJOR]) config.major = (uint16_t)blobmsg_get_u32(tb[BCN_MAJOR]);
    if (tb[BCN_MINOR]) config.minor = (uint16_t)blobmsg_get_u32(tb[BCN_MINOR]);
    if (tb[BCN_TX]) config.tx_power = (int8_t)blobmsg_get_u32(tb[BCN_TX]);
    if (tb[BCN_INTERVAL]) config.interval_ms = (uint16_t)blobmsg_get_u32(tb[BCN_INTERVAL]);

    ble_uuid_parse(config.uuid, config.uuid_bytes);

    int ret = ble_beacon_start(&config);

    /* On BlueZ transport, also register LE advertisement via D-Bus */
    const char *transport = ble_get_transport_name();
    if (ret == BLE_OK && transport && strcmp(transport, "bluez") == 0) {
        extern int ble_adv_start_ibeacon(const uint8_t uuid_bytes[16],
                                         uint16_t major, uint16_t minor, int8_t tx_power);
        int adv_ret = ble_adv_start_ibeacon(config.uuid_bytes,
                                            config.major, config.minor,
                                            config.tx_power);
        if (adv_ret < 0)
            syslog(LOG_WARNING, "beacon: LE adv registration failed: %d", adv_ret);
    }

    blob_buf_init(&b, 0);
    if (ret == BLE_OK) {
        blobmsg_add_string(&b, "status", "advertising");
        blobmsg_add_string(&b, "uuid", config.uuid);
        blobmsg_add_u32(&b, "major", config.major);
        blobmsg_add_u32(&b, "minor", config.minor);
    } else {
        blobmsg_add_string(&b, "error", ble_strerror(ret));
    }
    ubus_send_reply(ctx, req, b.head);
    return (ret == BLE_OK) ? UBUS_STATUS_OK : UBUS_STATUS_UNKNOWN_ERROR;
}

static int ubus_beacon_stop(struct ubus_context *ctx, struct ubus_object *obj,
                            struct ubus_request_data *req, const char *method,
                            struct blob_attr *msg)
{
    (void)obj; (void)method; (void)msg;

    /* On BlueZ, stop LE advertisement */
    const char *transport = ble_get_transport_name();
    if (transport && strcmp(transport, "bluez") == 0) {
        extern void ble_adv_stop(int instance_id);
        ble_adv_stop(2);  /* iBeacon uses instance 2 */
    }

    ble_beacon_stop();
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "stopped");
    ubus_send_reply(ctx, req, b.head);
    return UBUS_STATUS_OK;
}

/* ── gatt_connect ── */
enum { GATT_ADDR, __GATT_MAX };
static const struct blobmsg_policy gatt_policy[__GATT_MAX] = {
    [GATT_ADDR] = { .name = "address", .type = BLOBMSG_TYPE_STRING },
};

static int ubus_gatt_connect(struct ubus_context *ctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    (void)obj; (void)method;
    struct blob_attr *tb[__GATT_MAX];
    blobmsg_parse(gatt_policy, __GATT_MAX, tb, blob_data(msg), blob_len(msg));

    if (!tb[GATT_ADDR]) {
        blob_buf_init(&b, 0);
        blobmsg_add_string(&b, "error", "address required");
        ubus_send_reply(ctx, req, b.head);
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    const char *addr = blobmsg_get_string(tb[GATT_ADDR]);
    int ret = ble_gatt_connect(addr, 0);
    blob_buf_init(&b, 0);
    if (ret == BLE_OK) {
        blobmsg_add_string(&b, "status", "connecting");
        blobmsg_add_string(&b, "address", addr);
    } else {
        blobmsg_add_string(&b, "error", ble_strerror(ret));
    }
    ubus_send_reply(ctx, req, b.head);
    return (ret == BLE_OK) ? UBUS_STATUS_OK : UBUS_STATUS_UNKNOWN_ERROR;
}

/* ── status ── */
static int ubus_status(struct ubus_context *ctx, struct ubus_object *obj,
                       struct ubus_request_data *req, const char *method,
                       struct blob_attr *msg)
{
    (void)obj; (void)method; (void)msg;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "transport", ble_get_transport_name());
    const char *cp = ble_get_chip_profile_name();
    if (cp) blobmsg_add_string(&b, "chip_profile", cp);
    blobmsg_add_u8(&b, "scanning", ble_scan_is_active());
    blobmsg_add_u8(&b, "beacon", ble_beacon_is_active());
    ubus_send_reply(ctx, req, b.head);
    return UBUS_STATUS_OK;
}

/* ── power ── */
enum { PWR_RADIO, __PWR_MAX };
static const struct blobmsg_policy power_policy[__PWR_MAX] = {
    [PWR_RADIO] = { .name = "radio_power", .type = BLOBMSG_TYPE_INT32 },
};

static int ubus_power(struct ubus_context *ctx, struct ubus_object *obj,
                      struct ubus_request_data *req, const char *method,
                      struct blob_attr *msg)
{
    (void)obj; (void)method;
    struct blob_attr *tb[__PWR_MAX];
    blobmsg_parse(power_policy, __PWR_MAX, tb, blob_data(msg), blob_len(msg));

    int8_t power_dbm = 0;
    if (tb[PWR_RADIO]) power_dbm = (int8_t)blobmsg_get_u32(tb[PWR_RADIO]);

    int ret = ble_set_radio_tx_power(power_dbm);
    blob_buf_init(&b, 0);
    if (ret >= -128) {
        blobmsg_add_u32(&b, "requested", power_dbm);
        blobmsg_add_u32(&b, "actual", ret);
    } else {
        blobmsg_add_string(&b, "error", ble_strerror(ret));
    }
    ubus_send_reply(ctx, req, b.head);
    return UBUS_STATUS_OK;
}

/* ── unified gatt method ── */
enum { GATT_UNI_ACTION, GATT_UNI_ADDR, GATT_UNI_HANDLE, GATT_UNI_DATA, __GATT_UNI_MAX };
static const struct blobmsg_policy gatt_uni_policy[__GATT_UNI_MAX] = {
    [GATT_UNI_ACTION] = { .name = "action",  .type = BLOBMSG_TYPE_STRING },
    [GATT_UNI_ADDR]   = { .name = "addr",    .type = BLOBMSG_TYPE_STRING },
    [GATT_UNI_HANDLE] = { .name = "handle",  .type = BLOBMSG_TYPE_INT32 },
    [GATT_UNI_DATA]   = { .name = "data",    .type = BLOBMSG_TYPE_STRING },
};

static int ubus_gatt(struct ubus_context *ctx, struct ubus_object *obj,
                     struct ubus_request_data *req, const char *method,
                     struct blob_attr *msg)
{
    (void)obj; (void)method;
    struct blob_attr *tb[__GATT_UNI_MAX];
    blobmsg_parse(gatt_uni_policy, __GATT_UNI_MAX, tb, blob_data(msg), blob_len(msg));

    const char *action = tb[GATT_UNI_ACTION] ? blobmsg_get_string(tb[GATT_UNI_ACTION]) : "";
    const char *addr = tb[GATT_UNI_ADDR] ? blobmsg_get_string(tb[GATT_UNI_ADDR]) : "";
    uint16_t handle = tb[GATT_UNI_HANDLE] ? (uint16_t)blobmsg_get_u32(tb[GATT_UNI_HANDLE]) : 0;
    int ret = BLE_ERR_INVAL;

    blob_buf_init(&b, 0);

    if (strcmp(action, "connect") == 0) {
        ret = ble_gatt_connect(addr, 0);
    } else if (strcmp(action, "disconnect") == 0) {
        ret = ble_gatt_disconnect(0);
    } else if (strcmp(action, "read") == 0) {
        ret = ble_gatt_read(0, handle);
    } else if (strcmp(action, "write") == 0) {
        const char *hex = tb[GATT_UNI_DATA] ? blobmsg_get_string(tb[GATT_UNI_DATA]) : "";
        uint8_t data[256];
        uint16_t len = 0;
        for (int i = 0; hex[i] && hex[i+1] && len < sizeof(data); i += 2) {
            unsigned int byte;
            sscanf(&hex[i], "%2x", &byte);
            data[len++] = (uint8_t)byte;
        }
        ret = ble_gatt_write(0, handle, data, len);
    } else {
        blobmsg_add_string(&b, "error", "unknown action");
        ubus_send_reply(ctx, req, b.head);
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    if (ret == BLE_OK)
        blobmsg_add_string(&b, "status", "ok");
    else
        blobmsg_add_string(&b, "error", ble_strerror(ret));

    ubus_send_reply(ctx, req, b.head);
    return (ret == BLE_OK) ? UBUS_STATUS_OK : UBUS_STATUS_UNKNOWN_ERROR;
}

/* ── ubus object ── */

/* Unified beacon method with action dispatch */
enum { BCN_ACTION, __BCN_UNI_MAX };
static const struct blobmsg_policy beacon_uni_policy[] = {
    [BCN_ACTION] = { .name = "action", .type = BLOBMSG_TYPE_STRING },
};

static int ubus_beacon(struct ubus_context *ctx, struct ubus_object *obj,
                       struct ubus_request_data *req, const char *method,
                       struct blob_attr *msg)
{
    struct blob_attr *tb[__BCN_UNI_MAX];
    blobmsg_parse(beacon_uni_policy, __BCN_UNI_MAX, tb, blob_data(msg), blob_len(msg));

    const char *action = tb[BCN_ACTION] ? blobmsg_get_string(tb[BCN_ACTION]) : "start";

    if (strcmp(action, "stop") == 0)
        return ubus_beacon_stop(ctx, obj, req, method, msg);
    else
        return ubus_beacon_start(ctx, obj, req, method, msg);
}

/* Unified scan method with action dispatch */
enum { SCAN_ACTION, __SCAN_UNI_MAX };
static const struct blobmsg_policy scan_uni_policy[] = {
    [SCAN_ACTION] = { .name = "action", .type = BLOBMSG_TYPE_STRING },
};

static int ubus_scan(struct ubus_context *ctx, struct ubus_object *obj,
                     struct ubus_request_data *req, const char *method,
                     struct blob_attr *msg)
{
    struct blob_attr *tb[__SCAN_UNI_MAX];
    blobmsg_parse(scan_uni_policy, __SCAN_UNI_MAX, tb, blob_data(msg), blob_len(msg));

    const char *action = tb[SCAN_ACTION] ? blobmsg_get_string(tb[SCAN_ACTION]) : "start";

    if (strcmp(action, "stop") == 0)
        return ubus_scan_stop(ctx, obj, req, method, msg);
    else
        return ubus_scan_start(ctx, obj, req, method, msg);
}

static const struct ubus_method ble_methods[] = {
    UBUS_METHOD("scan",         ubus_scan,         scan_uni_policy),
    UBUS_METHOD("scan_start",   ubus_scan_start,   scan_policy),
    UBUS_METHOD_NOARG("scan_stop", ubus_scan_stop),
    UBUS_METHOD("beacon",       ubus_beacon,       beacon_uni_policy),
    UBUS_METHOD("beacon_start", ubus_beacon_start, beacon_policy),
    UBUS_METHOD_NOARG("beacon_stop", ubus_beacon_stop),
    UBUS_METHOD("gatt",         ubus_gatt,         gatt_uni_policy),
    UBUS_METHOD("gatt_connect", ubus_gatt_connect, gatt_policy),
    UBUS_METHOD("power",        ubus_power,        power_policy),
    UBUS_METHOD_NOARG("status", ubus_status),
};

static struct ubus_object_type ble_obj_type =
    UBUS_OBJECT_TYPE("ble.provision", ble_methods);

static struct ubus_object ble_object = {
    .name = "ble.provision",
    .type = &ble_obj_type,
    .methods = ble_methods,
    .n_methods = ARRAY_SIZE(ble_methods),
};

/* ── Event publishing ── */
void ubus_publish_event(const ble_event_t *event)
{
    if (!ubus_ctx || !event) return;
    blob_buf_init(&b, 0);

    switch (event->type) {
    case BLE_EVT_SCAN_RESULT: {
        /* Identify beacon type and build extended result */
        scan_result_ext_t ext;
        memset(&ext, 0, sizeof(ext));
        scan_filter_identify(event, &ext);

        /* Check filter — only forward matching results */
        if (!scan_filter_match(&ext, active_scan_filter, 0))
            break;

        /* Write to file */
        scan_file_write_result(&active_scan_ctx, &ext);

        /* Publish via ubus event */
        blobmsg_add_string(&b, "address", ext.address);
        blobmsg_add_string(&b, "address_type", ext.address_type);
        blobmsg_add_u32(&b, "rssi", (uint32_t)(int32_t)ext.rssi);
        blobmsg_add_string(&b, "name", ext.name);
        blobmsg_add_u8(&b, "connectable", ext.connectable);
        blobmsg_add_string(&b, "timestamp", ext.timestamp);

        /* Beacon type */
        const char *btype = "unknown";
        switch (ext.beacon_type) {
        case BEACON_TYPE_IBEACON:   btype = "ibeacon"; break;
        case BEACON_TYPE_EDDYSTONE: btype = "eddystone"; break;
        case BEACON_TYPE_ALTBEACON: btype = "altbeacon"; break;
        default: break;
        }
        blobmsg_add_string(&b, "beacon_type", btype);

        /* iBeacon specific fields */
        if (ext.beacon_type == BEACON_TYPE_IBEACON) {
            blobmsg_add_string(&b, "uuid", ext.ibeacon.uuid_str);
            blobmsg_add_u32(&b, "major", ext.ibeacon.major);
            blobmsg_add_u32(&b, "minor", ext.ibeacon.minor);
            blobmsg_add_u32(&b, "tx_power", (uint32_t)(int32_t)ext.ibeacon.tx_power);
        }

        /* Adv data as hex */
        if (ext.adv_data_len > 0) {
            char hex[125] = {0};
            for (int i = 0; i < ext.adv_data_len && i < 62; i++)
                sprintf(hex + i * 2, "%02x", ext.adv_data[i]);
            blobmsg_add_string(&b, "adv_data", hex);
        }

        ubus_send_event(ubus_ctx, "ble.provision.scan_result", b.head);
        break;
    }
    case BLE_EVT_SCAN_COMPLETE:
        /* Close scan file when scan completes */
        scan_file_close(&active_scan_ctx);
        blobmsg_add_u32(&b, "count", event->scan_complete.count);
        ubus_send_event(ubus_ctx, "ble.provision.scan_complete", b.head);
        break;
    case BLE_EVT_CONNECT:
        blobmsg_add_string(&b, "address", event->connect.address);
        ubus_send_event(ubus_ctx, "ble.provision.connected", b.head);
        break;
    default:
        break;
    }
}

/* ── Init/Deinit ── */
int ubus_api_init(void)
{
    ubus_ctx = ubus_connect(NULL);
    if (!ubus_ctx) return -ECONNREFUSED;
    ubus_add_uloop(ubus_ctx);
    int ret = ubus_add_object(ubus_ctx, &ble_object);
    if (ret) { ubus_free(ubus_ctx); ubus_ctx = NULL; return -ret; }
    return 0;
}

void ubus_api_deinit(void)
{
    if (ubus_ctx) {
        ubus_remove_object(ubus_ctx, &ble_object);
        ubus_free(ubus_ctx);
        ubus_ctx = NULL;
    }
}
