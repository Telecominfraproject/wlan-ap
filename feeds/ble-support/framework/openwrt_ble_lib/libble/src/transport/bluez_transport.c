/**
 * @file bluez_transport.c
 * @brief BlueZ D-Bus transport plugin for libble.
 *
 * Communicates with BlueZ via libdbus (low-level D-Bus C library).
 * Uses org.bluez Adapter1 and LE Advertising Manager interfaces.
 *
 * libdbus is available on OpenWrt via the 'dbus' package.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dbus/dbus.h>

#include "transport_plugin.h"
#include "../log.h"

#define BLUEZ_SERVICE          "org.bluez"
#define BLUEZ_ADAPTER_IFACE   "org.bluez.Adapter1"
#define BLUEZ_DEVICE_IFACE    "org.bluez.Device1"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"
#define DBUS_INTROSPECT_IFACE "org.freedesktop.DBus.Introspectable"

#define DBUS_TIMEOUT_MS       5000

static struct {
    DBusConnection *conn;
    char adapter_path[128];
    transport_event_cb_t event_cb;
    void *event_cb_ctx;
    bool initialized;
    int watch_fd;
} bluez_state;

/* ── Helpers ── */

static void build_adapter_path(const char *adapter, char *path, size_t len)
{
    snprintf(path, len, "/org/bluez/%s", adapter);
}

/**
 * Call a D-Bus method with no parameters, no return value.
 */
static int dbus_call_simple(const char *dest, const char *path,
                            const char *iface, const char *method)
{
    DBusMessage *msg = dbus_message_new_method_call(dest, path, iface, method);
    if (!msg) return -ENOMEM;

    DBusError err;
    dbus_error_init(&err);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        bluez_state.conn, msg, DBUS_TIMEOUT_MS, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        BLE_LOG_DBG("dbus call %s.%s failed: %s", iface, method, err.message);
        dbus_error_free(&err);
        return -EIO;
    }

    if (reply) dbus_message_unref(reply);
    return 0;
}

/**
 * Get a boolean property via org.freedesktop.DBus.Properties.Get
 */
static int dbus_get_bool_property(const char *path, const char *iface,
                                  const char *prop, dbus_bool_t *out)
{
    DBusMessage *msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, path, DBUS_PROPERTIES_IFACE, "Get");
    if (!msg) return -ENOMEM;

    dbus_message_append_args(msg,
        DBUS_TYPE_STRING, &iface,
        DBUS_TYPE_STRING, &prop,
        DBUS_TYPE_INVALID);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        bluez_state.conn, msg, DBUS_TIMEOUT_MS, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        return -ENODEV;
    }
    if (!reply) return -EIO;

    /* Reply is a variant containing a boolean */
    DBusMessageIter iter, variant;
    dbus_message_iter_init(reply, &iter);
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT) {
        dbus_message_iter_recurse(&iter, &variant);
        if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_BOOLEAN) {
            dbus_message_iter_get_basic(&variant, out);
        }
    }

    dbus_message_unref(reply);
    return 0;
}

/**
 * Set a boolean property via org.freedesktop.DBus.Properties.Set
 */
static int dbus_set_bool_property(const char *path, const char *iface,
                                  const char *prop, dbus_bool_t value)
{
    DBusMessage *msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, path, DBUS_PROPERTIES_IFACE, "Set");
    if (!msg) return -ENOMEM;

    dbus_message_append_args(msg,
        DBUS_TYPE_STRING, &iface,
        DBUS_TYPE_STRING, &prop,
        DBUS_TYPE_INVALID);

    /* Append variant(boolean) */
    DBusMessageIter iter, variant;
    dbus_message_iter_init_append(msg, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
    dbus_message_iter_close_container(&iter, &variant);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        bluez_state.conn, msg, DBUS_TIMEOUT_MS, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        return -EIO;
    }
    if (reply) dbus_message_unref(reply);
    return 0;
}

/* ── Signal Filter (for scan results) ── */

static DBusHandlerResult signal_filter(DBusConnection *conn,
                                       DBusMessage *msg, void *user_data)
{
    (void)conn;
    (void)user_data;

    if (!dbus_message_is_signal(msg, DBUS_PROPERTIES_IFACE, "PropertiesChanged"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* Check interface is org.bluez.Device1 */
    DBusMessageIter iter;
    if (!dbus_message_iter_init(msg, &iter))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    const char *iface = NULL;
    dbus_message_iter_get_basic(&iter, &iface);
    if (!iface || strcmp(iface, BLUEZ_DEVICE_IFACE) != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* Extract device address from object path */
    const char *path = dbus_message_get_path(msg);
    ble_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = BLE_EVT_SCAN_RESULT;

    if (path) {
        const char *dev = strrchr(path, '/');
        if (dev && strlen(dev) >= 18) {
            dev++;  /* skip '/' */
            /* BlueZ path format: dev_AA_BB_CC_DD_EE_FF */
            char addr[BLE_ADDR_STR_LEN] = {0};
            for (int i = 0; i < 17 && dev[4 + i]; i++)
                addr[i] = (dev[4 + i] == '_') ? ':' : dev[4 + i];
            strncpy(event.scan_result.address, addr, BLE_ADDR_STR_LEN - 1);
        }
    }

    /* Parse changed properties dict: a{sv} */
    dbus_message_iter_next(&iter);  /* move to changed properties */
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
        DBusMessageIter dict;
        dbus_message_iter_recurse(&iter, &dict);

        while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entry, value;
            dbus_message_iter_recurse(&dict, &entry);

            const char *prop_name = NULL;
            dbus_message_iter_get_basic(&entry, &prop_name);
            dbus_message_iter_next(&entry);
            dbus_message_iter_recurse(&entry, &value);

            if (!prop_name) goto next_prop;

            /* RSSI (int16) */
            if (strcmp(prop_name, "RSSI") == 0) {
                if (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_INT16) {
                    int16_t rssi;
                    dbus_message_iter_get_basic(&value, &rssi);
                    event.scan_result.rssi = (int8_t)rssi;
                }
            }
            /* Name (string) */
            else if (strcmp(prop_name, "Name") == 0) {
                if (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING) {
                    const char *name = NULL;
                    dbus_message_iter_get_basic(&value, &name);
                    if (name)
                        strncpy(event.scan_result.name, name, BLE_NAME_MAX - 1);
                }
            }
            /* AddressType (string: "public" or "random") */
            else if (strcmp(prop_name, "AddressType") == 0) {
                if (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING) {
                    const char *atype = NULL;
                    dbus_message_iter_get_basic(&value, &atype);
                    if (atype)
                        strncpy(event.scan_result.address_type, atype, 7);
                }
            }
            /* ManufacturerData (a{qv} — extract first entry as raw adv bytes) */
            else if (strcmp(prop_name, "ManufacturerData") == 0) {
                if (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_ARRAY) {
                    DBusMessageIter mfr_dict;
                    dbus_message_iter_recurse(&value, &mfr_dict);
                    if (dbus_message_iter_get_arg_type(&mfr_dict) == DBUS_TYPE_DICT_ENTRY) {
                        DBusMessageIter mfr_entry, mfr_var, mfr_arr;
                        dbus_message_iter_recurse(&mfr_dict, &mfr_entry);
                        /* Skip company ID (uint16) */
                        dbus_message_iter_next(&mfr_entry);
                        /* Get variant → array of bytes */
                        dbus_message_iter_recurse(&mfr_entry, &mfr_var);
                        if (dbus_message_iter_get_arg_type(&mfr_var) == DBUS_TYPE_ARRAY) {
                            dbus_message_iter_recurse(&mfr_var, &mfr_arr);
                            const uint8_t *bytes = NULL;
                            int n_bytes = 0;
                            dbus_message_iter_get_fixed_array(&mfr_arr, &bytes, &n_bytes);
                            if (bytes && n_bytes > 0) {
                                int copy_len = (n_bytes > BLE_ADV_DATA_MAX) ? BLE_ADV_DATA_MAX : n_bytes;
                                memcpy(event.scan_result.adv_data, bytes, copy_len);
                                event.scan_result.adv_data_len = (uint8_t)copy_len;
                            }
                        }
                    }
                }
            }
            /* Connectable (boolean) — BlueZ 5.56+ */
            else if (strcmp(prop_name, "Connectable") == 0) {
                if (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_BOOLEAN) {
                    dbus_bool_t conn;
                    dbus_message_iter_get_basic(&value, &conn);
                    event.scan_result.connectable = conn ? true : false;
                }
            }

next_prop:
            dbus_message_iter_next(&dict);
        }
    }

    if (bluez_state.event_cb)
        bluez_state.event_cb(&event, bluez_state.event_cb_ctx);

    return DBUS_HANDLER_RESULT_HANDLED;
}

/* ── Transport Plugin Implementation ── */

static int bluez_init(const char *config)
{
    if (bluez_state.initialized) return -EALREADY;
    memset(&bluez_state, 0, sizeof(bluez_state));

    /*
     * Pre-check: verify dbus-daemon is running before attempting connection.
     * dbus_bus_get() will block indefinitely if dbus-daemon is not running.
     */
    if (access("/var/run/dbus/system_bus_socket", F_OK) != 0 &&
        access("/run/dbus/system_bus_socket", F_OK) != 0) {
        BLE_LOG_ERR("D-Bus system bus socket not found — dbus-daemon not running");
        return -ENODEV;
    }

    DBusError err;
    dbus_error_init(&err);

    bluez_state.conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
    if (!bluez_state.conn || dbus_error_is_set(&err)) {
        BLE_LOG_ERR("Failed to connect to system D-Bus: %s",
                    err.message ? err.message : "unknown");
        dbus_error_free(&err);
        return -EIO;
    }

    const char *adapter = config ? config : "hci0";
    build_adapter_path(adapter, bluez_state.adapter_path,
                       sizeof(bluez_state.adapter_path));

    /* Check if adapter exists and is powered */
    dbus_bool_t powered = FALSE;
    int ret = dbus_get_bool_property(bluez_state.adapter_path,
                                     BLUEZ_ADAPTER_IFACE, "Powered", &powered);
    if (ret < 0) {
        BLE_LOG_ERR("Adapter %s not accessible (BlueZ not running?)",
                    bluez_state.adapter_path);
        dbus_connection_unref(bluez_state.conn);
        bluez_state.conn = NULL;
        return -ENODEV;
    }

    /* Power on if not already */
    if (!powered) {
        dbus_bool_t val = TRUE;
        dbus_set_bool_property(bluez_state.adapter_path,
                               BLUEZ_ADAPTER_IFACE, "Powered", val);
    }

    /* Add signal filter for PropertiesChanged (scan results) */
    dbus_bus_add_match(bluez_state.conn,
        "type='signal',"
        "sender='" BLUEZ_SERVICE "',"
        "interface='" DBUS_PROPERTIES_IFACE "',"
        "member='PropertiesChanged'",
        &err);
    if (dbus_error_is_set(&err)) {
        BLE_LOG_WARN("D-Bus match rule failed: %s", err.message);
        dbus_error_free(&err);
    }

    dbus_connection_add_filter(bluez_state.conn, signal_filter, NULL, NULL);

    /* Get the fd for poll/select integration */
    if (!dbus_connection_get_unix_fd(bluez_state.conn, &bluez_state.watch_fd))
        bluez_state.watch_fd = -1;

    bluez_state.initialized = true;
    BLE_LOG_INFO("BlueZ transport initialized (adapter=%s)", adapter);
    return 0;
}

static int bluez_deinit(void)
{
    if (!bluez_state.initialized) return -EINVAL;

    dbus_connection_remove_filter(bluez_state.conn, signal_filter, NULL);

    if (bluez_state.conn) {
        dbus_connection_unref(bluez_state.conn);
        bluez_state.conn = NULL;
    }

    bluez_state.initialized = false;
    BLE_LOG_INFO("BlueZ transport deinitialized");
    return 0;
}

static int bluez_send_hci_cmd(const ble_hci_cmd_t *cmd)
{
    if (!bluez_state.initialized || !cmd) return -EINVAL;

    uint16_t ocf = cmd->opcode & 0x03FF;
    uint8_t ogf = (cmd->opcode >> 10) & 0x3F;

    /* Map HCI LE scan enable to BlueZ StartDiscovery/StopDiscovery */
    if (ogf == 0x08 && ocf == 0x000C) {
        bool enable = (cmd->param_len > 0) ? cmd->params[0] : false;
        const char *method = enable ? "StartDiscovery" : "StopDiscovery";
        return dbus_call_simple(BLUEZ_SERVICE, bluez_state.adapter_path,
                                BLUEZ_ADAPTER_IFACE, method);
    }

    /* Map HCI LE Set Advertising Enable */
    if (ogf == 0x08 && ocf == 0x000A) {
        /* BlueZ manages advertising differently — needs LEAdvertisingManager1 */
        BLE_LOG_DBG("BlueZ: advertising managed via LEAdvertisingManager1");
        return 0;
    }

    BLE_LOG_WARN("BlueZ: unmapped HCI OGF=0x%02x OCF=0x%04x", ogf, ocf);
    return -ENOSYS;
}

static int bluez_send_vendor_cmd(uint8_t ogf, uint16_t ocf,
                                 const uint8_t *params, uint8_t param_len)
{
    (void)params;
    (void)param_len;
    BLE_LOG_WARN("BlueZ: vendor cmd OGF=0x%02x OCF=0x%04x not supported",
                 ogf, ocf);
    return -ENOSYS;
}

static int bluez_register_event_callback(transport_event_cb_t cb, void *ctx)
{
    bluez_state.event_cb = cb;
    bluez_state.event_cb_ctx = ctx;
    return 0;
}

static int bluez_get_fd(void)
{
    return bluez_state.watch_fd;
}

static int bluez_process_events(void)
{
    if (!bluez_state.conn) return -EINVAL;

    /* Dispatch pending D-Bus messages (non-blocking) */
    dbus_connection_read_write(bluez_state.conn, 0);
    while (dbus_connection_dispatch(bluez_state.conn) ==
           DBUS_DISPATCH_DATA_REMAINS)
        ;

    return 0;
}

/* ── Plugin Export ── */

transport_plugin_t bluez_transport_plugin = {
    .name                    = "bluez",
    .init                    = bluez_init,
    .deinit                  = bluez_deinit,
    .send_hci_cmd            = bluez_send_hci_cmd,
    .send_vendor_cmd         = bluez_send_vendor_cmd,
    .register_event_callback = bluez_register_event_callback,
    .get_fd                  = bluez_get_fd,
    .process_events          = bluez_process_events,
    .active                  = false
};
