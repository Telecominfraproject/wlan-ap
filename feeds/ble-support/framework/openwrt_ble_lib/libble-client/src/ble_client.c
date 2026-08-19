/**
 * @file ble_client.c
 * @brief libble-client implementation — ubus wrapper for BLE daemon communication.
 *
 * All BLE operations are forwarded to ble-provisiond via ubus IPC.
 * Multiple client processes can connect simultaneously; the daemon
 * handles hardware multiplexing and broadcasts events to all subscribers.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>

#include "ble_client.h"

/* ── Internal state ── */
static struct ubus_context *ctx;
static uint32_t ble_obj_id;
static struct blob_buf b;

/* Event subscriber */
static struct ubus_event_handler ev_handler;

/* Callbacks */
static ble_client_scan_cb_t scan_cb;
static void *scan_cb_ctx;
static ble_client_gatt_cb_t gatt_notify_cb;
static void *gatt_notify_ctx;

/* ── Helpers ── */

static int ensure_connected(void)
{
    if (!ctx) return BLE_CLIENT_ERR_UBUS;
    if (ble_obj_id == 0) return BLE_CLIENT_ERR_DAEMON;
    return BLE_CLIENT_OK;
}

/* ── Event handler (receives ble.* events from daemon) ── */

static void event_receive_cb(struct ubus_context *uctx,
                             struct ubus_event_handler *ev,
                             const char *type, struct blob_attr *msg)
{
    (void)uctx; (void)ev;

    enum { E_ADDR, E_RSSI, E_NAME, E_TYPE, E_CONN,
           E_HANDLE, E_VALUE, E_VALUE_LEN, __E_MAX };
    static const struct blobmsg_policy ep[__E_MAX] = {
        [E_ADDR]      = { .name = "address",     .type = BLOBMSG_TYPE_STRING },
        [E_RSSI]      = { .name = "rssi",        .type = BLOBMSG_TYPE_INT32 },
        [E_NAME]      = { .name = "name",        .type = BLOBMSG_TYPE_STRING },
        [E_TYPE]      = { .name = "address_type",.type = BLOBMSG_TYPE_STRING },
        [E_CONN]      = { .name = "conn_handle", .type = BLOBMSG_TYPE_INT32 },
        [E_HANDLE]    = { .name = "char_handle", .type = BLOBMSG_TYPE_INT32 },
        [E_VALUE]     = { .name = "value",       .type = BLOBMSG_TYPE_STRING },
        [E_VALUE_LEN] = { .name = "value_len",   .type = BLOBMSG_TYPE_INT32 },
    };
    struct blob_attr *tb[__E_MAX];
    blobmsg_parse(ep, __E_MAX, tb, blob_data(msg), blob_len(msg));

    /* ble.scan_result event */
    if (strcmp(type, "ble.scan_result") == 0 && scan_cb) {
        ble_client_scan_result_t result;
        memset(&result, 0, sizeof(result));

        if (tb[E_ADDR])
            strncpy(result.address, blobmsg_get_string(tb[E_ADDR]),
                    sizeof(result.address) - 1);
        if (tb[E_RSSI])
            result.rssi = (int8_t)blobmsg_get_u32(tb[E_RSSI]);
        if (tb[E_NAME])
            strncpy(result.name, blobmsg_get_string(tb[E_NAME]),
                    sizeof(result.name) - 1);
        if (tb[E_TYPE])
            strncpy(result.address_type, blobmsg_get_string(tb[E_TYPE]),
                    sizeof(result.address_type) - 1);
        result.connectable = true;

        scan_cb(&result, scan_cb_ctx);
    }

    /* ble.gatt_notify event */
    if (strcmp(type, "ble.gatt_notify") == 0 && gatt_notify_cb) {
        ble_client_gatt_data_t data;
        memset(&data, 0, sizeof(data));

        if (tb[E_CONN])
            data.conn_handle = (uint16_t)blobmsg_get_u32(tb[E_CONN]);
        if (tb[E_HANDLE])
            data.char_handle = (uint16_t)blobmsg_get_u32(tb[E_HANDLE]);
        if (tb[E_VALUE] && tb[E_VALUE_LEN]) {
            const char *hex = blobmsg_get_string(tb[E_VALUE]);
            data.value_len = (uint16_t)blobmsg_get_u32(tb[E_VALUE_LEN]);
            if (data.value_len > sizeof(data.value))
                data.value_len = sizeof(data.value);
            /* Decode hex string to bytes */
            for (uint16_t i = 0; i < data.value_len && hex[i * 2]; i++) {
                unsigned int byte;
                if (sscanf(&hex[i * 2], "%2x", &byte) == 1)
                    data.value[i] = (uint8_t)byte;
            }
        }

        gatt_notify_cb(&data, gatt_notify_ctx);
    }
}

/* ── Public API: Connection ── */

int ble_client_connect(void)
{
    if (ctx) return BLE_CLIENT_OK; /* Already connected */

    ctx = ubus_connect(NULL);
    if (!ctx) return BLE_CLIENT_ERR_UBUS;

    /* Look up the "ble.provision" object (exposed by ble-provisiond) */
    int ret = ubus_lookup_id(ctx, "ble.provision", &ble_obj_id);
    if (ret) {
        ubus_free(ctx);
        ctx = NULL;
        return BLE_CLIENT_ERR_DAEMON;
    }

    return BLE_CLIENT_OK;
}

void ble_client_disconnect(void)
{
    if (ctx) {
        blob_buf_free(&b);
        ubus_free(ctx);
        ctx = NULL;
        ble_obj_id = 0;
    }
    scan_cb = NULL;
    scan_cb_ctx = NULL;
    gatt_notify_cb = NULL;
    gatt_notify_ctx = NULL;
}

int ble_client_get_fd(void)
{
    if (!ctx) return -1;
    return ctx->sock.fd;
}

int ble_client_process(void)
{
    if (!ctx) return BLE_CLIENT_ERR_UBUS;
    ubus_handle_event(ctx);
    return BLE_CLIENT_OK;
}

/* ── Public API: Scan ── */

int ble_client_scan_start(uint32_t duration_ms, bool active, bool filter_dup)
{
    int ret = ensure_connected();
    if (ret) return ret;

    blob_buf_init(&b, 0);
    blobmsg_add_u32(&b, "duration", duration_ms);
    blobmsg_add_u8(&b, "active", active);
    blobmsg_add_u8(&b, "filter_dup", filter_dup);

    ret = ubus_invoke(ctx, ble_obj_id, "scan_start", b.head, NULL, NULL, 5000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_scan_stop(void)
{
    int ret = ensure_connected();
    if (ret) return ret;

    blob_buf_init(&b, 0);
    ret = ubus_invoke(ctx, ble_obj_id, "scan_stop", b.head, NULL, NULL, 3000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_scan_subscribe(ble_client_scan_cb_t cb, void *user_ctx)
{
    int ret = ensure_connected();
    if (ret) return ret;
    if (!cb) return BLE_CLIENT_ERR_PARAM;

    scan_cb = cb;
    scan_cb_ctx = user_ctx;

    /* Register for ble.* events */
    memset(&ev_handler, 0, sizeof(ev_handler));
    ev_handler.cb = event_receive_cb;
    ret = ubus_register_event_handler(ctx, &ev_handler, "ble.*");
    return ret ? BLE_CLIENT_ERR_UBUS : BLE_CLIENT_OK;
}

/* ── Public API: Beacon ── */

int ble_client_beacon_start(const char *uuid, uint16_t major,
                            uint16_t minor, int8_t tx_power)
{
    int ret = ensure_connected();
    if (ret) return ret;
    if (!uuid) return BLE_CLIENT_ERR_PARAM;

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "uuid", uuid);
    blobmsg_add_u32(&b, "major", major);
    blobmsg_add_u32(&b, "minor", minor);
    blobmsg_add_u32(&b, "tx_power", (uint32_t)(int32_t)tx_power);

    ret = ubus_invoke(ctx, ble_obj_id, "beacon_start", b.head, NULL, NULL, 5000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_beacon_stop(void)
{
    int ret = ensure_connected();
    if (ret) return ret;

    blob_buf_init(&b, 0);
    ret = ubus_invoke(ctx, ble_obj_id, "beacon_stop", b.head, NULL, NULL, 3000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

/* ── Public API: Radio TX Power ── */

struct radio_power_resp {
    int actual_power;
    int done;
};

static void radio_power_cb(struct ubus_request *req, int type,
                           struct blob_attr *msg)
{
    (void)type;
    struct radio_power_resp *rp = (struct radio_power_resp *)req->priv;
    if (!rp || !msg) return;

    enum { P_ACTUAL, __P_MAX };
    static const struct blobmsg_policy pp[__P_MAX] = {
        [P_ACTUAL] = { .name = "actual_power", .type = BLOBMSG_TYPE_INT32 },
    };
    struct blob_attr *tb[__P_MAX];
    blobmsg_parse(pp, __P_MAX, tb, blob_data(msg), blob_len(msg));

    if (tb[P_ACTUAL])
        rp->actual_power = (int)(int32_t)blobmsg_get_u32(tb[P_ACTUAL]);
    rp->done = 1;
}

int ble_client_set_radio_tx_power(int8_t power_dbm)
{
    int ret = ensure_connected();
    if (ret) return ret;

    struct radio_power_resp rp = { .actual_power = (int)power_dbm, .done = 0 };

    blob_buf_init(&b, 0);
    blobmsg_add_u32(&b, "power_dbm", (uint32_t)(int32_t)power_dbm);

    ret = ubus_invoke(ctx, ble_obj_id, "set_radio_tx_power", b.head,
                      radio_power_cb, &rp, 5000);
    if (ret) return BLE_CLIENT_ERR_DAEMON;

    return rp.actual_power;
}

struct radio_range_resp {
    int8_t min_dbm;
    int8_t max_dbm;
    int done;
};

static void radio_range_cb(struct ubus_request *req, int type,
                           struct blob_attr *msg)
{
    (void)type;
    struct radio_range_resp *rr = (struct radio_range_resp *)req->priv;
    if (!rr || !msg) return;

    enum { R_MIN, R_MAX, __R_MAX };
    static const struct blobmsg_policy rp[__R_MAX] = {
        [R_MIN] = { .name = "min_dbm", .type = BLOBMSG_TYPE_INT32 },
        [R_MAX] = { .name = "max_dbm", .type = BLOBMSG_TYPE_INT32 },
    };
    struct blob_attr *tb[__R_MAX];
    blobmsg_parse(rp, __R_MAX, tb, blob_data(msg), blob_len(msg));

    if (tb[R_MIN]) rr->min_dbm = (int8_t)(int32_t)blobmsg_get_u32(tb[R_MIN]);
    if (tb[R_MAX]) rr->max_dbm = (int8_t)(int32_t)blobmsg_get_u32(tb[R_MAX]);
    rr->done = 1;
}

int ble_client_get_radio_power_range(int8_t *min_dbm, int8_t *max_dbm)
{
    int ret = ensure_connected();
    if (ret) return ret;
    if (!min_dbm || !max_dbm) return BLE_CLIENT_ERR_PARAM;

    struct radio_range_resp rr = { .min_dbm = 0, .max_dbm = 0, .done = 0 };

    blob_buf_init(&b, 0);
    ret = ubus_invoke(ctx, ble_obj_id, "get_radio_power_range", b.head,
                      radio_range_cb, &rr, 5000);
    if (ret) return BLE_CLIENT_ERR_DAEMON;

    *min_dbm = rr.min_dbm;
    *max_dbm = rr.max_dbm;
    return BLE_CLIENT_OK;
}

/* ── Public API: GATT ── */

int ble_client_gatt_connect(const char *address)
{
    int ret = ensure_connected();
    if (ret) return ret;
    if (!address) return BLE_CLIENT_ERR_PARAM;

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "address", address);

    ret = ubus_invoke(ctx, ble_obj_id, "gatt_connect", b.head, NULL, NULL, 10000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_gatt_disconnect(uint16_t conn_handle)
{
    int ret = ensure_connected();
    if (ret) return ret;

    blob_buf_init(&b, 0);
    blobmsg_add_u32(&b, "conn_handle", conn_handle);

    ret = ubus_invoke(ctx, ble_obj_id, "gatt_disconnect", b.head, NULL, NULL, 5000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_gatt_read(uint16_t conn_handle, uint16_t char_handle)
{
    int ret = ensure_connected();
    if (ret) return ret;

    blob_buf_init(&b, 0);
    blobmsg_add_u32(&b, "conn_handle", conn_handle);
    blobmsg_add_u32(&b, "char_handle", char_handle);

    ret = ubus_invoke(ctx, ble_obj_id, "gatt_read", b.head, NULL, NULL, 5000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_gatt_write(uint16_t conn_handle, uint16_t char_handle,
                          const uint8_t *data, uint16_t len)
{
    int ret = ensure_connected();
    if (ret) return ret;
    if (!data || len == 0) return BLE_CLIENT_ERR_PARAM;

    blob_buf_init(&b, 0);
    blobmsg_add_u32(&b, "conn_handle", conn_handle);
    blobmsg_add_u32(&b, "char_handle", char_handle);

    /* Encode data as hex string */
    char hex_buf[1025];
    uint16_t max_len = (len > 512) ? 512 : len;
    for (uint16_t i = 0; i < max_len; i++)
        snprintf(&hex_buf[i * 2], 3, "%02x", data[i]);
    hex_buf[max_len * 2] = '\0';

    blobmsg_add_string(&b, "value", hex_buf);
    blobmsg_add_u32(&b, "value_len", max_len);

    ret = ubus_invoke(ctx, ble_obj_id, "gatt_write", b.head, NULL, NULL, 5000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_gatt_subscribe(uint16_t conn_handle, uint16_t cccd_handle, bool enable)
{
    int ret = ensure_connected();
    if (ret) return ret;

    blob_buf_init(&b, 0);
    blobmsg_add_u32(&b, "conn_handle", conn_handle);
    blobmsg_add_u32(&b, "cccd_handle", cccd_handle);
    blobmsg_add_u8(&b, "enable", enable);

    ret = ubus_invoke(ctx, ble_obj_id, "gatt_subscribe", b.head, NULL, NULL, 5000);
    return ret ? BLE_CLIENT_ERR_DAEMON : BLE_CLIENT_OK;
}

int ble_client_gatt_on_notify(ble_client_gatt_cb_t cb, void *user_ctx)
{
    int ret = ensure_connected();
    if (ret) return ret;

    gatt_notify_cb = cb;
    gatt_notify_ctx = user_ctx;

    /* Ensure event handler is registered */
    memset(&ev_handler, 0, sizeof(ev_handler));
    ev_handler.cb = event_receive_cb;
    ret = ubus_register_event_handler(ctx, &ev_handler, "ble.*");
    return ret ? BLE_CLIENT_ERR_UBUS : BLE_CLIENT_OK;
}

/* ── Public API: Status ── */

struct status_req {
    char *buf;
    size_t len;
    int done;
};

static void status_data_cb(struct ubus_request *req, int type,
                           struct blob_attr *msg)
{
    (void)type;
    struct status_req *sr = (struct status_req *)req->priv;
    if (!sr || !msg) return;

    char *json = blobmsg_format_json(msg, true);
    if (json) {
        strncpy(sr->buf, json, sr->len - 1);
        sr->buf[sr->len - 1] = '\0';
        free(json);
        sr->done = 1;
    }
}

int ble_client_get_status(char *json_buf, size_t buf_len)
{
    int ret = ensure_connected();
    if (ret) return ret;
    if (!json_buf || buf_len == 0) return BLE_CLIENT_ERR_PARAM;

    struct status_req sr = { .buf = json_buf, .len = buf_len, .done = 0 };
    blob_buf_init(&b, 0);

    ret = ubus_invoke(ctx, ble_obj_id, "status", b.head, status_data_cb, &sr, 5000);
    if (ret) return BLE_CLIENT_ERR_DAEMON;
    if (!sr.done) return BLE_CLIENT_ERR_TIMEOUT;

    return BLE_CLIENT_OK;
}
