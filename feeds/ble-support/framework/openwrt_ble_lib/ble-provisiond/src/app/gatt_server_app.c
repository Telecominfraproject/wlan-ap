/**
 * @file gatt_server_app.c
 * @brief GATT Server application — exposes device management and config
 *        provisioning services to BLE clients (phone apps).
 *
 * Implementation uses libdbus (low-level D-Bus C API) to register
 * GATT services with BlueZ's GattManager1 interface.
 *
 * On OpenWrt, systemd/sd-bus is NOT available. This implementation
 * uses the standard libdbus API which is provided by the 'dbus' package.
 *
 * Services:
 *   1. Device Management (0xFE00): reboot, factory reset, OTA upgrade
 *   2. Configuration Provisioning (0xFE10): uCentral config pass-through
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <dbus/dbus.h>

#include "app_plugin.h"
#include "gatt_server_app.h"
#include "mtu_segment.h"
#include "config_provision.h"
#include "ble_advertise.h"
#include "../uci_app_config.h"

/* ── Constants ── */

#define BLUEZ_SERVICE           "org.bluez"
#define BLUEZ_GATT_MGR_IFACE   "org.bluez.GattManager1"
#define BLUEZ_GATT_SVC_IFACE   "org.bluez.GattService1"
#define BLUEZ_GATT_CHR_IFACE   "org.bluez.GattCharacteristic1"
#define DBUS_OM_IFACE           "org.freedesktop.DBus.ObjectManager"

#define MAX_CHARS_PER_SERVICE   4
#define MAX_SERVICES            2
#define NOTIFY_VALUE_MAX        512
#define STATUS_JSON_MAX         256
#define CONFIG_JSON_MAX         MTU_SEG_MAX_PAYLOAD
#define OTA_URL_MAX             1024
#define DBUS_TIMEOUT_MS         5000

/* ── Data Structures ── */

typedef struct gatt_char {
    const char *uuid;
    const char *path;
    const char *service_path;
    const char *flags[8];
    int flag_count;
    bool notifying;
    uint8_t value[NOTIFY_VALUE_MAX];
    uint16_t value_len;
    mtu_reassembly_t reassembly;
} gatt_char_t;

typedef struct gatt_service {
    const char *uuid;
    const char *path;
    bool primary;
    gatt_char_t chars[MAX_CHARS_PER_SERVICE];
    int char_count;
} gatt_service_t;

/* ── Application State ── */

static struct {
    DBusConnection *conn;
    char adapter_path[128];
    gatt_service_t services[MAX_SERVICES];
    int service_count;
    bool registered;
    bool running;
    /* Configuration */
    bool allow_ota;
    bool allow_factory_reset;
    bool allow_config;
    char pin[16];
    char device_name[64];
    /* OTA URL reassembly */
    char ota_url[OTA_URL_MAX];
    mtu_reassembly_t ota_reassembly;
    /* Config reassembly */
    char config_buf[CONFIG_JSON_MAX];
    mtu_reassembly_t config_reassembly;
    /* Current status */
    char status_json[STATUS_JSON_MAX];
    /* ATT MTU */
    uint16_t att_mtu;
    /* Advertising instance ID */
    int adv_instance_id;
} gs;

/* ── Forward Declarations ── */

static int gatt_server_init(void);
static void gatt_server_deinit(void);
static int gatt_server_start(void);
static int gatt_server_stop(void);

/* ── Helpers ── */

static void update_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gs.status_json, sizeof(gs.status_json), fmt, ap);
    va_end(ap);
}

static void build_adapter_path(const char *adapter)
{
    snprintf(gs.adapter_path, sizeof(gs.adapter_path),
             "/org/bluez/%s", adapter ? adapter : "hci0");
}

/* ══════════════════════════════════════════════════════════════════
 *  Command Handlers
 * ══════════════════════════════════════════════════════════════════ */

int handle_device_command(uint8_t cmd)
{
    switch (cmd) {
    case GATT_CMD_REBOOT:
        syslog(LOG_NOTICE, "gatt_server: reboot requested via BLE");
        update_status("{\"status\":\"rebooting\"}");
        system("sleep 2 && reboot &");
        return 0;

    case GATT_CMD_FACTORY_RESET:
        if (!gs.allow_factory_reset) {
            syslog(LOG_WARNING, "gatt_server: factory reset denied");
            update_status("{\"status\":\"error\",\"msg\":\"disabled\"}");
            return -EPERM;
        }
        syslog(LOG_NOTICE, "gatt_server: factory reset via BLE");
        update_status("{\"status\":\"factory_reset\"}");
        system("sleep 2 && jffs2reset -y && reboot &");
        return 0;

    case GATT_CMD_OTA_UPGRADE:
        if (!gs.allow_ota) {
            syslog(LOG_WARNING, "gatt_server: OTA denied");
            update_status("{\"status\":\"error\",\"msg\":\"ota_disabled\"}");
            return -EPERM;
        }
        if (gs.ota_url[0] == '\0') {
            update_status("{\"status\":\"error\",\"msg\":\"no_ota_url\"}");
            return -EINVAL;
        }
        syslog(LOG_NOTICE, "gatt_server: OTA to %s", gs.ota_url);
        {
            char cmd_buf[OTA_URL_MAX + 64];
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "sysupgrade -n '%s' &", gs.ota_url);
            system(cmd_buf);
        }
        return 0;

    default:
        syslog(LOG_WARNING, "gatt_server: unknown command %u", cmd);
        return -EINVAL;
    }
}

int handle_ota_url(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || len >= OTA_URL_MAX) return -EINVAL;
    memcpy(gs.ota_url, data, len);
    gs.ota_url[len] = '\0';
    syslog(LOG_INFO, "gatt_server: OTA URL set: %s", gs.ota_url);
    return 0;
}

int handle_config_write(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || len >= CONFIG_JSON_MAX) return -EINVAL;
    syslog(LOG_INFO, "gatt_server: config received (%u bytes)", len);

    int ret = config_provision_apply(data, len, NULL, NULL);
    if (ret < 0) {
        update_status("{\"status\":\"error\",\"msg\":\"config_failed\",\"code\":%d}", ret);
        return ret;
    }
    update_status("{\"status\":\"config_applied\"}");
    return 0;
}

int handle_wifi_scan(void)
{
    syslog(LOG_INFO, "gatt_server: WiFi scan triggered");
    update_status("{\"status\":\"scanning\"}");
    /* Trigger scan via ubus — results returned via notification */
    int ret = system("ubus call iwinfo scan '{\"device\":\"wlan0\"}' >/dev/null 2>&1");
    if (ret != 0)
        update_status("{\"status\":\"scan_failed\"}");
    else
        update_status("{\"status\":\"scan_complete\"}");
    return ret == 0 ? 0 : -EIO;
}

int get_wifi_config(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return -EINVAL;
    return config_provision_get_current(buf, buf_size);
}

int get_wifi_status(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return -EINVAL;
    FILE *fp = popen("ubus call network.wireless status 2>/dev/null", "r");
    if (!fp) {
        snprintf(buf, buf_size, "{\"status\":\"unknown\"}");
        return 0;
    }
    size_t n = fread(buf, 1, buf_size - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    if (n == 0) snprintf(buf, buf_size, "{\"status\":\"disconnected\"}");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 *  D-Bus Message Handlers (libdbus)
 *
 *  BlueZ GATT API: the host registers as a GATT application by
 *  exporting D-Bus objects that implement GattService1 and
 *  GattCharacteristic1 interfaces, then calls RegisterApplication
 *  on the GattManager1 interface.
 *
 *  With libdbus, we handle incoming method calls via a message filter
 *  or object path handlers.
 * ══════════════════════════════════════════════════════════════════ */

/**
 * Find a characteristic by its D-Bus object path.
 */
static gatt_char_t *find_char_by_path(const char *path)
{
    for (int si = 0; si < gs.service_count; si++) {
        for (int ci = 0; ci < gs.services[si].char_count; ci++) {
            if (strcmp(gs.services[si].chars[ci].path, path) == 0)
                return &gs.services[si].chars[ci];
        }
    }
    return NULL;
}

/**
 * Find a service by its D-Bus object path.
 */
static gatt_service_t *find_service_by_path(const char *path) __attribute__((unused));
static gatt_service_t *find_service_by_path(const char *path)
{
    for (int si = 0; si < gs.service_count; si++) {
        if (strcmp(gs.services[si].path, path) == 0)
            return &gs.services[si];
    }
    return NULL;
}

/**
 * Handle ReadValue method call on a characteristic.
 */
static DBusMessage *handle_read_value(DBusMessage *msg, gatt_char_t *chr)
{
    /* Populate value on-demand for readable characteristics */
    if (strcmp(chr->uuid, GATT_CHR_DEV_STATUS_UUID) == 0) {
        if (gs.status_json[0] == '\0')
            update_status("{\"status\":\"idle\"}");
        chr->value_len = (uint16_t)strlen(gs.status_json);
        memcpy(chr->value, gs.status_json, chr->value_len);
    } else if (strcmp(chr->uuid, GATT_CHR_CFG_READ_UUID) == 0) {
        char buf[STATUS_JSON_MAX];
        config_provision_get_current(buf, sizeof(buf));
        chr->value_len = (uint16_t)strlen(buf);
        if (chr->value_len > NOTIFY_VALUE_MAX)
            chr->value_len = NOTIFY_VALUE_MAX;
        memcpy(chr->value, buf, chr->value_len);
    }

    /* Build reply with byte array */
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) return NULL;

    DBusMessageIter iter, array;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "y", &array);
    dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
                                         &chr->value, chr->value_len);
    dbus_message_iter_close_container(&iter, &array);

    return reply;
}

/**
 * Handle WriteValue method call on a characteristic.
 */
static DBusMessage *handle_write_value(DBusMessage *msg, gatt_char_t *chr)
{
    DBusMessageIter iter, array;
    if (!dbus_message_iter_init(msg, &iter))
        return dbus_message_new_error(msg, "org.bluez.Error.InvalidArguments",
                                      "No arguments");

    /* First arg: byte array (ay) */
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
        return dbus_message_new_error(msg, "org.bluez.Error.InvalidArguments",
                                      "Expected byte array");

    dbus_message_iter_recurse(&iter, &array);
    const uint8_t *bytes = NULL;
    int len = 0;
    dbus_message_iter_get_fixed_array(&array, &bytes, &len);

    if (!bytes || len <= 0)
        return dbus_message_new_error(msg, "org.bluez.Error.InvalidValueLength",
                                      "Empty write");

    syslog(LOG_DEBUG, "gatt_server: write to %s, len=%d", chr->uuid, len);

    /* ── Device Management: Command (FE01) ── */
    if (strcmp(chr->uuid, GATT_CHR_COMMAND_UUID) == 0) {
        int ret = handle_device_command(bytes[0]);
        if (ret == -EPERM)
            return dbus_message_new_error(msg, "org.bluez.Error.NotPermitted",
                                          "Operation not allowed");
        return dbus_message_new_method_return(msg);
    }

    /* ── Device Management: OTA URL (FE02) ── */
    if (strcmp(chr->uuid, GATT_CHR_OTA_URL_UUID) == 0) {
        uint8_t header = bytes[0];
        if (header & (MTU_SEG_FLAG_FIRST | MTU_SEG_FLAG_LAST)) {
            int sret = mtu_reassembly_feed(&gs.ota_reassembly, bytes, (uint16_t)len);
            if (sret == 1) {
                handle_ota_url(gs.ota_reassembly.buffer,
                               gs.ota_reassembly.received_len);
                mtu_reassembly_reset(&gs.ota_reassembly);
            }
        } else {
            handle_ota_url(bytes, (uint16_t)len);
        }
        return dbus_message_new_method_return(msg);
    }

    /* ── Config Provisioning: Write (FE11) ── */
    if (strcmp(chr->uuid, GATT_CHR_CFG_WRITE_UUID) == 0) {
        uint8_t header = bytes[0];
        if (header & (MTU_SEG_FLAG_FIRST | MTU_SEG_FLAG_LAST)) {
            int sret = mtu_reassembly_feed(&gs.config_reassembly, bytes, (uint16_t)len);
            if (sret == 1) {
                handle_config_write(gs.config_reassembly.buffer,
                                    gs.config_reassembly.received_len);
                mtu_reassembly_reset(&gs.config_reassembly);
            }
        } else {
            handle_config_write(bytes, (uint16_t)len);
        }
        return dbus_message_new_method_return(msg);
    }

    return dbus_message_new_error(msg, "org.bluez.Error.NotSupported",
                                  "Write not supported on this characteristic");
}

/**
 * Handle GetManagedObjects — required by BlueZ GattManager1.
 * Returns all services and characteristics as a{oa{sa{sv}}}.
 */
static DBusMessage *handle_get_managed_objects(DBusMessage *msg)
{
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) return NULL;

    DBusMessageIter iter, dict;
    dbus_message_iter_init_append(reply, &iter);

    /* Open a{oa{sa{sv}}} */
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{oa{sa{sv}}}", &dict);

    for (int si = 0; si < gs.service_count; si++) {
        gatt_service_t *svc = &gs.services[si];

        /* ── Service entry ── */
        DBusMessageIter entry, ifaces, iface_entry, props, prop_entry, var;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_OBJECT_PATH, &svc->path);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sa{sv}}", &ifaces);
        dbus_message_iter_open_container(&ifaces, DBUS_TYPE_DICT_ENTRY, NULL, &iface_entry);
        const char *svc_iface = BLUEZ_GATT_SVC_IFACE;
        dbus_message_iter_append_basic(&iface_entry, DBUS_TYPE_STRING, &svc_iface);

        /* Properties: UUID (s), Primary (b) */
        dbus_message_iter_open_container(&iface_entry, DBUS_TYPE_ARRAY, "{sv}", &props);

        /* UUID */
        dbus_message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, NULL, &prop_entry);
        const char *key_uuid = "UUID";
        dbus_message_iter_append_basic(&prop_entry, DBUS_TYPE_STRING, &key_uuid);
        dbus_message_iter_open_container(&prop_entry, DBUS_TYPE_VARIANT, "s", &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &svc->uuid);
        dbus_message_iter_close_container(&prop_entry, &var);
        dbus_message_iter_close_container(&props, &prop_entry);

        /* Primary */
        dbus_message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, NULL, &prop_entry);
        const char *key_primary = "Primary";
        dbus_message_iter_append_basic(&prop_entry, DBUS_TYPE_STRING, &key_primary);
        dbus_message_iter_open_container(&prop_entry, DBUS_TYPE_VARIANT, "b", &var);
        dbus_bool_t prim = svc->primary ? TRUE : FALSE;
        dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &prim);
        dbus_message_iter_close_container(&prop_entry, &var);
        dbus_message_iter_close_container(&props, &prop_entry);

        dbus_message_iter_close_container(&iface_entry, &props);
        dbus_message_iter_close_container(&ifaces, &iface_entry);
        dbus_message_iter_close_container(&entry, &ifaces);
        dbus_message_iter_close_container(&dict, &entry);

        /* ── Characteristics for this service ── */
        for (int ci = 0; ci < svc->char_count; ci++) {
            gatt_char_t *chr = &svc->chars[ci];

            DBusMessageIter c_entry, c_ifaces, c_if_entry, c_props, c_prop, c_var;
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &c_entry);
            dbus_message_iter_append_basic(&c_entry, DBUS_TYPE_OBJECT_PATH, &chr->path);

            dbus_message_iter_open_container(&c_entry, DBUS_TYPE_ARRAY, "{sa{sv}}", &c_ifaces);
            dbus_message_iter_open_container(&c_ifaces, DBUS_TYPE_DICT_ENTRY, NULL, &c_if_entry);
            const char *chr_iface = BLUEZ_GATT_CHR_IFACE;
            dbus_message_iter_append_basic(&c_if_entry, DBUS_TYPE_STRING, &chr_iface);

            dbus_message_iter_open_container(&c_if_entry, DBUS_TYPE_ARRAY, "{sv}", &c_props);

            /* UUID */
            dbus_message_iter_open_container(&c_props, DBUS_TYPE_DICT_ENTRY, NULL, &c_prop);
            dbus_message_iter_append_basic(&c_prop, DBUS_TYPE_STRING, &key_uuid);
            dbus_message_iter_open_container(&c_prop, DBUS_TYPE_VARIANT, "s", &c_var);
            dbus_message_iter_append_basic(&c_var, DBUS_TYPE_STRING, &chr->uuid);
            dbus_message_iter_close_container(&c_prop, &c_var);
            dbus_message_iter_close_container(&c_props, &c_prop);

            /* Service */
            dbus_message_iter_open_container(&c_props, DBUS_TYPE_DICT_ENTRY, NULL, &c_prop);
            const char *key_svc = "Service";
            dbus_message_iter_append_basic(&c_prop, DBUS_TYPE_STRING, &key_svc);
            dbus_message_iter_open_container(&c_prop, DBUS_TYPE_VARIANT, "o", &c_var);
            dbus_message_iter_append_basic(&c_var, DBUS_TYPE_OBJECT_PATH, &chr->service_path);
            dbus_message_iter_close_container(&c_prop, &c_var);
            dbus_message_iter_close_container(&c_props, &c_prop);

            /* Flags (as) */
            dbus_message_iter_open_container(&c_props, DBUS_TYPE_DICT_ENTRY, NULL, &c_prop);
            const char *key_flags = "Flags";
            dbus_message_iter_append_basic(&c_prop, DBUS_TYPE_STRING, &key_flags);
            dbus_message_iter_open_container(&c_prop, DBUS_TYPE_VARIANT, "as", &c_var);
            DBusMessageIter flags_arr;
            dbus_message_iter_open_container(&c_var, DBUS_TYPE_ARRAY, "s", &flags_arr);
            for (int fi = 0; fi < chr->flag_count; fi++)
                dbus_message_iter_append_basic(&flags_arr, DBUS_TYPE_STRING, &chr->flags[fi]);
            dbus_message_iter_close_container(&c_var, &flags_arr);
            dbus_message_iter_close_container(&c_prop, &c_var);
            dbus_message_iter_close_container(&c_props, &c_prop);

            dbus_message_iter_close_container(&c_if_entry, &c_props);
            dbus_message_iter_close_container(&c_ifaces, &c_if_entry);
            dbus_message_iter_close_container(&c_entry, &c_ifaces);
            dbus_message_iter_close_container(&dict, &c_entry);
        }
    }

    dbus_message_iter_close_container(&iter, &dict);
    return reply;
}

/* ══════════════════════════════════════════════════════════════════
 *  D-Bus Message Filter — dispatches incoming method calls
 * ══════════════════════════════════════════════════════════════════ */

static DBusHandlerResult message_handler(DBusConnection *conn,
                                         DBusMessage *msg, void *user_data)
{
    (void)user_data;
    const char *path = dbus_message_get_path(msg);
    const char *iface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);

    if (!path || !iface || !member)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessage *reply = NULL;

    /* ObjectManager.GetManagedObjects on application root */
    if (strcmp(path, GATT_APP_PATH) == 0 &&
        strcmp(iface, DBUS_OM_IFACE) == 0 &&
        strcmp(member, "GetManagedObjects") == 0) {
        reply = handle_get_managed_objects(msg);
    }
    /* GattCharacteristic1.ReadValue */
    else if (strcmp(iface, BLUEZ_GATT_CHR_IFACE) == 0 &&
             strcmp(member, "ReadValue") == 0) {
        gatt_char_t *chr = find_char_by_path(path);
        if (chr) reply = handle_read_value(msg, chr);
    }
    /* GattCharacteristic1.WriteValue */
    else if (strcmp(iface, BLUEZ_GATT_CHR_IFACE) == 0 &&
             strcmp(member, "WriteValue") == 0) {
        gatt_char_t *chr = find_char_by_path(path);
        if (chr) reply = handle_write_value(msg, chr);
    }
    /* GattCharacteristic1.StartNotify */
    else if (strcmp(iface, BLUEZ_GATT_CHR_IFACE) == 0 &&
             strcmp(member, "StartNotify") == 0) {
        gatt_char_t *chr = find_char_by_path(path);
        if (chr) { chr->notifying = true; reply = dbus_message_new_method_return(msg); }
    }
    /* GattCharacteristic1.StopNotify */
    else if (strcmp(iface, BLUEZ_GATT_CHR_IFACE) == 0 &&
             strcmp(member, "StopNotify") == 0) {
        gatt_char_t *chr = find_char_by_path(path);
        if (chr) { chr->notifying = false; reply = dbus_message_new_method_return(msg); }
    }

    if (reply) {
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* ══════════════════════════════════════════════════════════════════
 *  Service/Characteristic Setup
 * ══════════════════════════════════════════════════════════════════ */

static void setup_char(gatt_char_t *chr, const char *uuid, const char *path,
                       const char *svc_path, const char **flags, int nflags)
{
    memset(chr, 0, sizeof(*chr));
    chr->uuid = uuid;
    chr->path = path;
    chr->service_path = svc_path;
    chr->flag_count = (nflags > 7) ? 7 : nflags;
    for (int i = 0; i < chr->flag_count; i++)
        chr->flags[i] = flags[i];
    mtu_reassembly_init(&chr->reassembly);
}

static int setup_services(void)
{
    gs.service_count = 2;

    /* ── Service 0: Device Management (0xFE00) ── */
    gatt_service_t *svc0 = &gs.services[0];
    svc0->uuid = GATT_SRV_DEVMGMT_UUID;
    svc0->path = GATT_SRV_DEVMGMT_PATH;
    svc0->primary = true;
    svc0->char_count = 3;

    static const char *cmd_flags[] = { "write" };
    setup_char(&svc0->chars[0], GATT_CHR_COMMAND_UUID,
               GATT_SRV_DEVMGMT_PATH "/char0", GATT_SRV_DEVMGMT_PATH,
               cmd_flags, 1);

    static const char *ota_flags[] = { "write" };
    setup_char(&svc0->chars[1], GATT_CHR_OTA_URL_UUID,
               GATT_SRV_DEVMGMT_PATH "/char1", GATT_SRV_DEVMGMT_PATH,
               ota_flags, 1);

    static const char *status_flags[] = { "read", "notify" };
    setup_char(&svc0->chars[2], GATT_CHR_DEV_STATUS_UUID,
               GATT_SRV_DEVMGMT_PATH "/char2", GATT_SRV_DEVMGMT_PATH,
               status_flags, 2);

    /* ── Service 1: Configuration Provisioning (0xFE10) ── */
    gatt_service_t *svc1 = &gs.services[1];
    svc1->uuid = GATT_SRV_CFGPROV_UUID;
    svc1->path = GATT_SRV_CFGPROV_PATH;
    svc1->primary = true;
    svc1->char_count = 3;

    static const char *cfg_wr_flags[] = { "write" };
    setup_char(&svc1->chars[0], GATT_CHR_CFG_WRITE_UUID,
               GATT_SRV_CFGPROV_PATH "/char0", GATT_SRV_CFGPROV_PATH,
               cfg_wr_flags, 1);

    static const char *cfg_rd_flags[] = { "read" };
    setup_char(&svc1->chars[1], GATT_CHR_CFG_READ_UUID,
               GATT_SRV_CFGPROV_PATH "/char1", GATT_SRV_CFGPROV_PATH,
               cfg_rd_flags, 1);

    static const char *cfg_st_flags[] = { "read", "notify" };
    setup_char(&svc1->chars[2], GATT_CHR_CFG_STATUS_UUID,
               GATT_SRV_CFGPROV_PATH "/char2", GATT_SRV_CFGPROV_PATH,
               cfg_st_flags, 2);

    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 *  BLE Advertising — required for phone to discover this device
 * ══════════════════════════════════════════════════════════════════ */

/**
 * Enable BLE advertising by setting adapter Discoverable + Powered,
 * and registering a LEAdvertisement with BlueZ.
 *
 * BlueZ advertising requires:
 *   1. Adapter Powered = true
 *   2. Register LEAdvertisement object (or set Discoverable = true for legacy)
 *
 * For simplicity, we use the legacy approach: set Discoverable = true.
 * This makes the device visible to scanners without needing full
 * LEAdvertisingManager1 object registration.
 */
static int start_advertising(void)
{
    /* ble_adv module already initialized in gatt_server_start() */

    /* Start connectable LE advertising for GATT server */
    int ret = ble_adv_start_connectable(gs.device_name);
    if (ret < 0) {
        syslog(LOG_ERR, "gatt_server: LE advertising failed: %d", ret);
        return ret;
    }
    gs.adv_instance_id = ret;
    syslog(LOG_INFO, "gatt_server: LE advertising started (instance=%d, name=%s)",
           gs.adv_instance_id, gs.device_name);
    return 0;
}

static void stop_advertising(void)
{
    if (gs.adv_instance_id > 0) {
        ble_adv_stop(gs.adv_instance_id);
        gs.adv_instance_id = 0;
    }
    syslog(LOG_INFO, "gatt_server: LE advertising stopped");
}

static int register_application(void)
{
    DBusMessage *msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, gs.adapter_path,
        BLUEZ_GATT_MGR_IFACE, "RegisterApplication");
    if (!msg) return -ENOMEM;

    /* Args: object_path, options dict (empty) */
    DBusMessageIter iter, dict;
    dbus_message_iter_init_append(msg, &iter);
    const char *app_path = GATT_APP_PATH;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &app_path);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&iter, &dict);

    /*
     * Use async send + manual dispatch loop.
     * RegisterApplication causes BlueZ to call back our GetManagedObjects.
     * If we use send_with_reply_and_block, we deadlock because we can't
     * process the incoming GetManagedObjects call while blocking.
     */
    DBusPendingCall *pending = NULL;
    if (!dbus_connection_send_with_reply(gs.conn, msg, &pending, DBUS_TIMEOUT_MS)) {
        dbus_message_unref(msg);
        syslog(LOG_ERR, "gatt_server: RegisterApplication send failed");
        return -EIO;
    }
    dbus_message_unref(msg);

    if (!pending) {
        syslog(LOG_ERR, "gatt_server: RegisterApplication: no pending call");
        return -EIO;
    }

    /* Dispatch loop: process incoming messages (including GetManagedObjects)
     * while waiting for the RegisterApplication reply */
    while (!dbus_pending_call_get_completed(pending)) {
        dbus_connection_read_write_dispatch(gs.conn, 100);
    }

    DBusMessage *reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    if (!reply) {
        syslog(LOG_ERR, "gatt_server: RegisterApplication: no reply");
        return -EIO;
    }

    if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
        const char *err_msg = NULL;
        dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &err_msg, DBUS_TYPE_INVALID);
        syslog(LOG_ERR, "gatt_server: RegisterApplication error: %s",
               err_msg ? err_msg : dbus_message_get_error_name(reply));
        dbus_message_unref(reply);
        return -EIO;
    }

    dbus_message_unref(reply);
    gs.registered = true;
    syslog(LOG_INFO, "gatt_server: GATT application registered with BlueZ");
    return 0;
}

static void unregister_application(void)
{
    if (!gs.registered || !gs.conn) return;

    DBusMessage *msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, gs.adapter_path,
        BLUEZ_GATT_MGR_IFACE, "UnregisterApplication");
    if (msg) {
        DBusMessageIter iter;
        dbus_message_iter_init_append(msg, &iter);
        const char *app_path = GATT_APP_PATH;
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &app_path);
        dbus_connection_send(gs.conn, msg, NULL);
        dbus_message_unref(msg);
    }
    gs.registered = false;
}

/* ══════════════════════════════════════════════════════════════════
 *  Plugin Lifecycle
 * ══════════════════════════════════════════════════════════════════ */

static int gatt_server_init(void)
{
    memset(&gs, 0, sizeof(gs));

    /* Load configuration from UCI */
    gs.allow_ota = uci_app_get_bool("ble", "gatt_server", "allow_ota", false);
    gs.allow_factory_reset = uci_app_get_bool("ble", "gatt_server", "allow_factory_reset", false);
    gs.allow_config = uci_app_get_bool("ble", "gatt_server", "allow_config", true);
    uci_app_get_string("ble", "gatt_server", "device_name",
                       gs.device_name, sizeof(gs.device_name), "OpenWrt-BLE");
    uci_app_get_string("ble", "gatt_server", "pin",
                       gs.pin, sizeof(gs.pin), "");

    /* Initialize config provisioning module */
    config_provision_init(gs.allow_config);

    syslog(LOG_INFO, "gatt_server: init (name=%s ota=%d fr=%d cfg=%d)",
           gs.device_name, gs.allow_ota, gs.allow_factory_reset, gs.allow_config);
    return 0;
}

static int gatt_server_start(void)
{
    syslog(LOG_INFO, "gatt_server: starting (connecting to D-Bus...)");

    DBusError err;
    dbus_error_init(&err);

    gs.conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
    if (!gs.conn || dbus_error_is_set(&err)) {
        syslog(LOG_ERR, "gatt_server: D-Bus connect failed: %s",
               err.message ? err.message : "unknown");
        dbus_error_free(&err);
        return -EIO;
    }

    build_adapter_path("hci0");

    /* Always initialize ble_adv module (shared by GATT and iBeacon) */
    ble_adv_init(gs.conn, gs.adapter_path);

    /* Set adapter Alias */
    if (gs.device_name[0]) {
        DBusMessage *msg = dbus_message_new_method_call(
            BLUEZ_SERVICE, gs.adapter_path,
            "org.freedesktop.DBus.Properties", "Set");
        if (msg) {
            DBusMessageIter aiter, variant;
            const char *iface = "org.bluez.Adapter1";
            const char *prop = "Alias";
            const char *name = gs.device_name;
            dbus_message_iter_init_append(msg, &aiter);
            dbus_message_iter_append_basic(&aiter, DBUS_TYPE_STRING, &iface);
            dbus_message_iter_append_basic(&aiter, DBUS_TYPE_STRING, &prop);
            dbus_message_iter_open_container(&aiter, DBUS_TYPE_VARIANT, "s", &variant);
            dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &name);
            dbus_message_iter_close_container(&aiter, &variant);
            DBusMessage *reply = dbus_connection_send_with_reply_and_block(
                gs.conn, msg, DBUS_TIMEOUT_MS, &err);
            dbus_message_unref(msg);
            if (reply) dbus_message_unref(reply);
            if (dbus_error_is_set(&err)) dbus_error_free(&err);
        }
    }

    /* Only register GATT services + connectable advertising if enabled */
    bool gatt_enabled = uci_app_get_bool("ble", "gatt_server", "enabled", false);
    if (gatt_enabled) {
        setup_services();
        dbus_connection_add_filter(gs.conn, message_handler, NULL, NULL);

        int ret = register_application();
        if (ret < 0) {
            syslog(LOG_WARNING, "gatt_server: BlueZ registration failed");
        }

        /* Start connectable LE advertising */
        start_advertising();
    } else {
        syslog(LOG_INFO, "gatt_server: GATT disabled, ble_adv ready for iBeacon only");
    }

    gs.running = true;
    syslog(LOG_INFO, "gatt_server: started");
    return 0;
}

static int gatt_server_stop(void)
{
    if (!gs.running) return 0;

    stop_advertising();
    unregister_application();

    if (gs.conn) {
        dbus_connection_remove_filter(gs.conn, message_handler, NULL);
        dbus_connection_unref(gs.conn);
        gs.conn = NULL;
    }

    config_provision_deinit();
    ble_adv_deinit();
    gs.running = false;
    syslog(LOG_INFO, "gatt_server: stopped");
    return 0;
}

static void gatt_server_deinit(void)
{
    gatt_server_stop();
}

/* ── Plugin export ── */

app_plugin_t gatt_server_app_plugin = {
    .name   = "gatt_server",
    .init   = gatt_server_init,
    .start  = gatt_server_start,
    .stop   = gatt_server_stop,
    .deinit = gatt_server_deinit,
};
