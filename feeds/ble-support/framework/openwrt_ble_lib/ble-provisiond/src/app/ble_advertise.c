/**
 * @file ble_advertise.c
 * @brief BLE LE Advertising via BlueZ LEAdvertisingManager1 D-Bus API.
 *
 * Exports LEAdvertisement1 D-Bus objects and registers them with BlueZ.
 * Supports connectable (GATT) and non-connectable (iBeacon) modes.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <dbus/dbus.h>

#include "ble_advertise.h"

#define BLUEZ_SERVICE       "org.bluez"
#define BLUEZ_LE_ADV_MGR    "org.bluez.LEAdvertisingManager1"
#define BLUEZ_LE_ADV_IFACE  "org.bluez.LEAdvertisement1"
#define ADV_BASE_PATH       "/org/openwrt/ble/adv"
#define DBUS_TIMEOUT_MS     10000

/* ── Module State ── */

static struct {
    DBusConnection *conn;
    char adapter_path[128];
    ble_adv_instance_t instances[BLE_ADV_MAX_INSTANCES];
    bool initialized;
} adv_ctx;

/* ── D-Bus Message Handler for LEAdvertisement1 ── */

static DBusHandlerResult adv_message_handler(DBusConnection *conn,
                                             DBusMessage *msg, void *user_data)
{
    (void)user_data;
    const char *path = dbus_message_get_path(msg);
    const char *iface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);

    if (!path || !iface || !member)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* Find the matching instance */
    ble_adv_instance_t *inst = NULL;
    for (int i = 0; i < BLE_ADV_MAX_INSTANCES; i++) {
        if (adv_ctx.instances[i].active &&
            strcmp(adv_ctx.instances[i].path, path) == 0) {
            inst = &adv_ctx.instances[i];
            break;
        }
    }
    if (!inst)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* Handle GetAll properties (BlueZ calls this during RegisterAdvertisement) */
    if (strcmp(iface, "org.freedesktop.DBus.Properties") == 0 &&
        strcmp(member, "GetAll") == 0) {

        syslog(LOG_INFO, "ble_adv: GetAll called for instance %d (path=%s)", inst->id, path);

        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (!reply) return DBUS_HANDLER_RESULT_NEED_MEMORY;

        DBusMessageIter iter, dict;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);

        /* Type property */
        {
            DBusMessageIter entry, variant;
            const char *key = "Type";
            const char *type_str = (inst->type == BLE_ADV_TYPE_PERIPHERAL)
                                   ? "peripheral" : "broadcast";
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
            dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &type_str);
            dbus_message_iter_close_container(&entry, &variant);
            dbus_message_iter_close_container(&dict, &entry);
        }

        /* LocalName (for peripheral type only) */
        if (inst->type == BLE_ADV_TYPE_PERIPHERAL && inst->local_name[0]) {
            DBusMessageIter entry, variant;
            const char *key = "LocalName";
            const char *name = inst->local_name;
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
            dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &name);
            dbus_message_iter_close_container(&entry, &variant);
            dbus_message_iter_close_container(&dict, &entry);
        }

        /* Includes — for iBeacon (has mfr_data), set empty to prevent BlueZ
         * from auto-adding local name (which would exceed 31 byte ADV limit) */
        if (inst->mfr_data_len > 0) {
            DBusMessageIter entry, variant, arr;
            const char *key = "Includes";
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as", &variant);
            dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &arr);
            /* Empty array — no auto-includes */
            dbus_message_iter_close_container(&variant, &arr);
            dbus_message_iter_close_container(&entry, &variant);
            dbus_message_iter_close_container(&dict, &entry);
        }

        /* ManufacturerData (for iBeacon) — variant(a{qv}), where v=ay */
        if (inst->mfr_data_len > 0) {
            DBusMessageIter entry, variant, mfr_dict, mfr_entry, mfr_var, mfr_arr;
            const char *key = "ManufacturerData";
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
            dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a{qv}", &variant);
            dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "{qv}", &mfr_dict);
            dbus_message_iter_open_container(&mfr_dict, DBUS_TYPE_DICT_ENTRY, NULL, &mfr_entry);
            dbus_message_iter_append_basic(&mfr_entry, DBUS_TYPE_UINT16, &inst->mfr_company_id);
            dbus_message_iter_open_container(&mfr_entry, DBUS_TYPE_VARIANT, "ay", &mfr_var);
            dbus_message_iter_open_container(&mfr_var, DBUS_TYPE_ARRAY, "y", &mfr_arr);
            const uint8_t *mfr_ptr = inst->mfr_data;
            dbus_message_iter_append_fixed_array(&mfr_arr, DBUS_TYPE_BYTE,
                                                 &mfr_ptr, (int)inst->mfr_data_len);
            dbus_message_iter_close_container(&mfr_var, &mfr_arr);
            dbus_message_iter_close_container(&mfr_entry, &mfr_var);
            dbus_message_iter_close_container(&mfr_dict, &mfr_entry);
            dbus_message_iter_close_container(&variant, &mfr_dict);
            dbus_message_iter_close_container(&entry, &variant);
            dbus_message_iter_close_container(&dict, &entry);
        }

        dbus_message_iter_close_container(&iter, &dict);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    /* Handle Release (BlueZ calls this when advertisement is removed) */
    if (strcmp(iface, BLUEZ_LE_ADV_IFACE) == 0 &&
        strcmp(member, "Release") == 0) {
        syslog(LOG_INFO, "ble_adv: instance %d released by BlueZ", inst->id);
        inst->active = false;
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* ── Register advertisement with BlueZ (async to avoid deadlock) ── */

static int register_advertisement(ble_adv_instance_t *inst)
{
    DBusMessage *msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, adv_ctx.adapter_path,
        BLUEZ_LE_ADV_MGR, "RegisterAdvertisement");
    if (!msg) return -ENOMEM;

    DBusMessageIter iter, dict;
    dbus_message_iter_init_append(msg, &iter);
    const char *adv_path = inst->path;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &adv_path);
    /* Empty options dict */
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&iter, &dict);

    /* Async send + dispatch (same pattern as RegisterApplication) */
    DBusPendingCall *pending = NULL;
    if (!dbus_connection_send_with_reply(adv_ctx.conn, msg, &pending, DBUS_TIMEOUT_MS)) {
        dbus_message_unref(msg);
        return -EIO;
    }
    dbus_message_unref(msg);

    if (!pending) return -EIO;

    syslog(LOG_INFO, "ble_adv: waiting for RegisterAdvertisement reply (instance=%d)...", inst->id);

    /* Dispatch loop: handle BlueZ's GetAll callback while waiting */
    while (!dbus_pending_call_get_completed(pending)) {
        dbus_connection_read_write_dispatch(adv_ctx.conn, 100);
    }

    DBusMessage *reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    if (!reply) {
        syslog(LOG_ERR, "ble_adv: RegisterAdvertisement[%d]: no reply", inst->id);
        return -EIO;
    }

    if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
        const char *err_name = dbus_message_get_error_name(reply);
        syslog(LOG_ERR, "ble_adv: RegisterAdvertisement[%d] error: %s",
               inst->id, err_name ? err_name : "unknown");
        dbus_message_unref(reply);
        return -EIO;
    }

    dbus_message_unref(reply);
    syslog(LOG_INFO, "ble_adv: instance %d registered (type=%s)",
           inst->id, inst->type == BLE_ADV_TYPE_PERIPHERAL ? "peripheral" : "broadcast");
    return 0;
}

static void unregister_advertisement(ble_adv_instance_t *inst)
{
    if (!inst->active || !adv_ctx.conn) return;

    DBusMessage *msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, adv_ctx.adapter_path,
        BLUEZ_LE_ADV_MGR, "UnregisterAdvertisement");
    if (msg) {
        DBusMessageIter iter;
        dbus_message_iter_init_append(msg, &iter);
        const char *path = inst->path;
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);
        dbus_connection_send(adv_ctx.conn, msg, NULL);
        dbus_connection_flush(adv_ctx.conn);
        dbus_message_unref(msg);
    }
    inst->active = false;
}

/* ── Allocate an instance slot ── */

static ble_adv_instance_t *alloc_instance(void)
{
    for (int i = 0; i < BLE_ADV_MAX_INSTANCES; i++) {
        if (!adv_ctx.instances[i].active) {
            memset(&adv_ctx.instances[i], 0, sizeof(ble_adv_instance_t));
            adv_ctx.instances[i].id = (uint8_t)(i + 1);
            snprintf(adv_ctx.instances[i].path, sizeof(adv_ctx.instances[i].path),
                     ADV_BASE_PATH "%d", i);
            return &adv_ctx.instances[i];
        }
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════════ */

int ble_adv_init(DBusConnection *conn, const char *adapter_path)
{
    memset(&adv_ctx, 0, sizeof(adv_ctx));
    adv_ctx.conn = conn;
    strncpy(adv_ctx.adapter_path, adapter_path, sizeof(adv_ctx.adapter_path) - 1);

    /* Add message filter for advertisement property requests */
    dbus_connection_add_filter(adv_ctx.conn, adv_message_handler, NULL, NULL);

    adv_ctx.initialized = true;
    syslog(LOG_INFO, "ble_adv: initialized (adapter=%s)", adapter_path);
    return 0;
}

void ble_adv_deinit(void)
{
    ble_adv_stop_all();
    if (adv_ctx.conn)
        dbus_connection_remove_filter(adv_ctx.conn, adv_message_handler, NULL);
    adv_ctx.initialized = false;
}

int ble_adv_start_connectable(const char *local_name)
{
    if (!adv_ctx.initialized) return -EINVAL;

    ble_adv_instance_t *inst = alloc_instance();
    if (!inst) {
        syslog(LOG_ERR, "ble_adv: no free instance slots");
        return -ENOMEM;
    }

    inst->type = BLE_ADV_TYPE_PERIPHERAL;
    if (local_name)
        strncpy(inst->local_name, local_name, BLE_ADV_NAME_MAX - 1);
    inst->include_tx_power = true;
    inst->active = true;

    int ret = register_advertisement(inst);
    if (ret < 0) {
        inst->active = false;
        return ret;
    }
    return inst->id;
}

int ble_adv_start_ibeacon(const uint8_t uuid_bytes[16],
                          uint16_t major, uint16_t minor, int8_t tx_power)
{
    if (!adv_ctx.initialized) return -EINVAL;

    ble_adv_instance_t *inst = alloc_instance();
    if (!inst) {
        syslog(LOG_ERR, "ble_adv: no free instance slots");
        return -ENOMEM;
    }

    inst->type = BLE_ADV_TYPE_PERIPHERAL;  /* Use peripheral — many BLE chips don't support broadcast */
    inst->mfr_company_id = 0x004C;  /* Apple */
    /* iBeacon manufacturer data: type(0x02) + len(0x15) + UUID + major + minor + txpower */
    inst->mfr_data[0] = 0x02;
    inst->mfr_data[1] = 0x15;
    memcpy(&inst->mfr_data[2], uuid_bytes, 16);
    inst->mfr_data[18] = (major >> 8) & 0xFF;
    inst->mfr_data[19] = major & 0xFF;
    inst->mfr_data[20] = (minor >> 8) & 0xFF;
    inst->mfr_data[21] = minor & 0xFF;
    inst->mfr_data[22] = (uint8_t)tx_power;
    inst->mfr_data_len = 23;
    inst->active = true;

    int ret = register_advertisement(inst);
    if (ret < 0) {
        inst->active = false;
        return ret;
    }
    return inst->id;
}

void ble_adv_stop(int instance_id)
{
    if (instance_id < 1 || instance_id > BLE_ADV_MAX_INSTANCES) return;
    ble_adv_instance_t *inst = &adv_ctx.instances[instance_id - 1];
    if (inst->active)
        unregister_advertisement(inst);
}

void ble_adv_stop_all(void)
{
    for (int i = 0; i < BLE_ADV_MAX_INSTANCES; i++) {
        if (adv_ctx.instances[i].active)
            unregister_advertisement(&adv_ctx.instances[i]);
    }
}
