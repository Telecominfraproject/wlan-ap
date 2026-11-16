/* SPDX-License-Identifier: BSD-3-Clause */
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
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <dirent.h>
#include <dbus/dbus.h>
#include <libubox/uloop.h>

#include "app_plugin.h"
#include "gatt_server_app.h"
#include "adv_status.h"
#include "mtu_segment.h"
#include "config_provision.h"
#include "ble_advertise.h"
#include "ble_auth.h"
#include "gatt_v2.h"
#include "gatt_v2_cmd.h"
#include "gatt_v2_conn.h"
#include "gatt_v2_read.h"
#include "gatt_v2_upgrade.h"
#include "status_doc.h"
#include "config_doc.h"
#include "config_apply.h"
#include "../uci_app_config.h"

/* ── Constants ── */

#define BLUEZ_SERVICE           "org.bluez"
#define BLUEZ_GATT_MGR_IFACE   "org.bluez.GattManager1"
#define BLUEZ_GATT_SVC_IFACE   "org.bluez.GattService1"
#define BLUEZ_GATT_CHR_IFACE   "org.bluez.GattCharacteristic1"
#define DBUS_OM_IFACE           "org.freedesktop.DBus.ObjectManager"

#define MAX_CHARS_PER_SERVICE   8   /* v2 provisioning service has 7 chars */
#define MAX_SERVICES            1   /* v2 only — FE00/FE10 retired */
#define NOTIFY_VALUE_MAX        512
#define STATUS_JSON_MAX         256
#define CONFIG_JSON_MAX         MTU_SEG_MAX_PAYLOAD

/*
 * How often the advertised status bytes are re-checked.
 *
 * Only the uplink state needs polling; device_status arrives through a hook. The
 * reference re-registers the advertisement unconditionally at this interval,
 * which restarts the advertising set every time — we compare first and only
 * re-register on a real change.
 */
#define ADV_REFRESH_INTERVAL_MS 5000
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
	/* Config reassembly */
	char config_buf[CONFIG_JSON_MAX];
	mtu_reassembly_t config_reassembly;
	/* Current status */
	char status_json[STATUS_JSON_MAX];
	/* ATT MTU */
	uint16_t att_mtu;
	/* Advertising instance ID */
	int adv_instance_id;
	/*
     * No local copy of "the manufacturer bytes BlueZ currently holds" here. There
     * used to be one, and adv_refresh() compared against it to skip a needless
     * re-register — but it could not see whether the last register had actually
     * succeeded, so after a failure it reported "no change" while nothing was
     * advertised. ble_advertise.c owns that state (`inst->mfr_data` plus
     * `inst->registered`) and is the only place that can answer the question
     * correctly. See adv_refresh() and §19.6.1.
     */
	struct uloop_timeout adv_timer;
	/*
     * Zero-delay timer used to push a status-triggered republish off the caller's
     * stack (see adv_status_changed_cb). Separate from adv_timer so arming it
     * cannot disturb the periodic 5 s refresh.
     */
	struct uloop_timeout adv_now;
	/*
     * Zero-delay timer used to retry a registration that never succeeded, armed
     * only on a disconnect edge (see adv_retry_cb). A third timer rather than a
     * reuse of adv_now because the two do different work: adv_now republishes an
     * instance that exists, this one creates the instance that does not.
     */
	struct uloop_timeout adv_retry;
} gs;

/* ── Forward Declarations ── */

static int gatt_server_init(void);
static void gatt_server_deinit(void);
static int gatt_server_start(void);
static int gatt_server_stop(void);

/* ── Helpers ── */

/*
 * The value this package ships as `device_name`, used as a sentinel meaning
 * "no name was chosen for this board".
 */
#define GS_NAME_SENTINEL     "OpenWrt-BLE"

/** OpenWrt's board identity, "vendor,model\n" (e.g. "edgecore,eap104"). */
#define GS_BOARD_NAME_FILE   "/var/sysinfo/board_name"

/**
 * Longest device name that still fits a legacy advertising PDU.
 *
 * Derived rather than written as 18, so it tracks the manufacturer payload
 * instead of silently going stale if that ever grows:
 *
 *     31  primary advertising PDU
 *   -  3  Flags AD
 *   -  8  Manufacturer Specific Data AD (2 header + 2 company + 4 payload)
 *   -  2  Complete Local Name AD header
 *   ────
 *     18  characters of name
 *
 * This is the BlueZ path's limit and it is the binding one. The TI path puts
 * the name in the *scan response*, a separate 31 bytes, where the firmware caps
 * it at MR_NAME_MAX = 20 and truncates quietly. Exceeding the BlueZ budget is
 * far worse than being truncated: BlueZ refuses the whole registration with a
 * bare "Invalid Parameters" and the device stops advertising altogether, which
 * is indistinguishable from the bring-up failures of §19.9.7.
 *
 * ble_advertise.c computes the same sum to warn at registration time. That
 * check stays — it covers an operator-configured name too — but it fires too
 * late to do anything about, so the derivation respects the budget up front.
 */
#define GS_NAME_MAX   (31u - 3u - (4u + ADV_MFR_DATA_LEN) - 2u)

/** Uppercase @src into @dst, stopping at dst_sz-1 characters. */
static void gs_upper(char *dst, size_t dst_sz, const char *src)
{
	size_t i = 0;
	for (; src[i] && i + 1 < dst_sz; i++)
		dst[i] = (char)toupper((unsigned char)src[i]);
	dst[i] = '\0';
}

/**
 * Build an advertised name from the board identity:
 * edgecore,eap104 -> EDGECORE_EAP104.
 *
 * Why derive instead of configuring. /etc/config/ble is a rendered artifact on
 * this platform rather than user state — the configuration of record is
 * /etc/ucentral/ucentral.cfg.<uuid>, and a package with no uCentral schema of
 * its own has this file restored from the package copy during an apply. A
 * per-board name written into it by hand is therefore lost, silently, and the
 * device starts advertising as "OpenWrt-BLE". Deriving it means the right name
 * comes back on its own with nothing to re-apply.
 *
 * Read directly rather than through `ubus call system board`: this runs before
 * ubus is necessarily up, and that call returns the dts model string
 * ("EdgeCore EAP104") rather than the short identifier the name is built from.
 *
 * No "-BLE" suffix. It cost four of the eighteen characters and carried no
 * information the app uses: the scan list is filtered on the manufacturer
 * data's model_id (adv_status.h), and the real vendor and model strings reach
 * the app through A4 once connected.
 *
 * When VENDOR_MODEL does not fit, the vendor is dropped rather than the name
 * truncated — the model is the half that identifies the board, so "EAP104"
 * beats "EDGECORE_EAP10". Truncation is the last resort, for a model name that
 * is on its own too long.
 *
 * @return true when @out was filled; false leaves it untouched so the caller
 *         can keep the sentinel, which is a usable name if a poor one.
 */
static bool derive_device_name(char *out, size_t out_sz)
{
	if (!out || out_sz < 2)
		return false;

	FILE *fp = fopen(GS_BOARD_NAME_FILE, "r");
	if (!fp)
		return false;

	char line[96] = {0};
	char *got = fgets(line, sizeof(line), fp);
	fclose(fp);
	if (!got)
		return false;

	/* The file is "vendor,model\n". */
	line[strcspn(line, "\r\n")] = '\0';

	char *vendor = line;
	char *model  = strchr(line, ',');
	if (model) {
        *model++ = '\0';
	} else {
		/* No comma — treat the whole line as the model, with no vendor. */
		model  = line;
		vendor = NULL;
	}

	/* These become a BLE name: stop at the first separator in either field. */
	model[strcspn(model, " \t,")] = '\0';
	if (vendor)
		vendor[strcspn(vendor, " \t")] = '\0';
	if (!*model)
		return false;

	/* Headroom on purpose: vendor + '_' + model can reach 63 characters, and a
     * buffer that fits exactly invites a -Wformat-truncation complaint. */
	char cand[80];
	char m[32];
	gs_upper(m, sizeof(m), model);

	if (vendor && *vendor) {
		char v[32];
		gs_upper(v, sizeof(v), vendor);
		snprintf(cand, sizeof(cand), "%s_%s", v, m);

		if (strlen(cand) > GS_NAME_MAX) {
			syslog(LOG_INFO, "gatt_server: '%s' exceeds the %u-character "
							 "advertising budget — dropping the vendor",
				   cand, (unsigned)GS_NAME_MAX);
			snprintf(cand, sizeof(cand), "%s", m);
		}
	} else {
		snprintf(cand, sizeof(cand), "%s", m);
	}

	if (strlen(cand) > GS_NAME_MAX) {
		syslog(LOG_WARNING, "gatt_server: model name '%s' is longer than the "
							"%u-character advertising budget on its own — "
							"truncating", cand, (unsigned)GS_NAME_MAX);
		cand[GS_NAME_MAX] = '\0';
	}

	snprintf(out, out_sz, "%s", cand);
	return true;
}

const char *gatt_server_device_name(void)
{
	return gs.device_name;
}

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

/*
 * Note: the legacy FE01 byte-code command handler (reboot / factory reset / OTA)
 * and the FE02 OTA-URL handler have been removed along with the v1 services.
 * Those capabilities are now driven by JSON commands on A2 and implemented in
 * the shared, transport-agnostic modules gatt_v2_cmd.c and gatt_v2_upgrade.c —
 * one implementation for both the BlueZ and TI paths.
 */

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
 * Push a characteristic value change to the phone as a BlueZ notification.
 *
 * BlueZ turns an `org.freedesktop.DBus.Properties.PropertiesChanged` signal on
 * the characteristic object (with the new "Value") into an ATT notification for
 * every client that subscribed via StartNotify. This is the BlueZ half of the
 * shared notify hook (gatt_v2_set_notify_fn) — the TI half sends an NPI NOTIFY
 * command instead, so A2/A7 updates behave identically on both paths.
 *
 * The value is also cached on the characteristic so a subsequent plain read
 * returns the same content.
 */
static void bluez_v2_notify(gatt_v2_chr_t which, const uint8_t *data, uint16_t len)
{
	if (!gs.conn || gs.service_count < 1)
		return;
	if ((int)which < 0 || (int)which >= gs.services[0].char_count)
		return;

	gatt_char_t *chr = &gs.services[0].chars[which];

	/* Keep the readable cache in sync with what we notify. */
	uint16_t n = (len > NOTIFY_VALUE_MAX) ? NOTIFY_VALUE_MAX : len;
	if (data && n) {
		memcpy(chr->value, data, n);
		chr->value_len = n;
	}

	/* Only emit when a client actually subscribed. */
	if (!chr->notifying)
		return;

	DBusMessage *sig = dbus_message_new_signal(chr->path,
											   "org.freedesktop.DBus.Properties",
											   "PropertiesChanged");
	if (!sig)
		return;

	DBusMessageIter iter, dict, entry, var, arr, inval;
	dbus_message_iter_init_append(sig, &iter);

	const char *iface = BLUEZ_GATT_CHR_IFACE;
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);

	/* changed_properties: a{sv} with "Value" -> ay */
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
	dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	const char *key = "Value";
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &var);
	dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "y", &arr);
	{
		const uint8_t *p = chr->value;
		dbus_message_iter_append_fixed_array(&arr, DBUS_TYPE_BYTE,
											 &p, (int)chr->value_len);
	}
	dbus_message_iter_close_container(&var, &arr);
	dbus_message_iter_close_container(&entry, &var);
	dbus_message_iter_close_container(&dict, &entry);
	dbus_message_iter_close_container(&iter, &dict);

	/* invalidated_properties: as (empty) */
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &inval);
	dbus_message_iter_close_container(&iter, &inval);

	dbus_connection_send(gs.conn, sig, NULL);
	dbus_connection_flush(gs.conn);
	dbus_message_unref(sig);

	syslog(LOG_DEBUG, "gatt_server: notified %s (%u bytes)",
		   chr->uuid, chr->value_len);
}

/**
 * Parse the options dictionary BlueZ passes to ReadValue/WriteValue.
 *
 * BlueZ hands the application an a{sv} with, among others, "offset" (uint16) and
 * "mtu" (uint16). Ignoring "offset" is not harmless: on a read it makes a blob
 * read return the value from byte 0 again, so a client assembling a value longer
 * than ATT_MTU-1 gets a duplicated prefix instead of the tail; on a write it lets
 * one fragment of a prepared write be dispatched as if it were a whole command.
 *
 * "device" is the D-Bus object path of the remote client. It is this path that
 * makes per-connection state possible on the BlueZ path (§19.4): without it every
 * client would share one authentication flag and one reassembly buffer.
 *
 * @param msg        the method call.
 * @param skip_args  number of leading arguments before the dict
 *                   (0 for ReadValue, 1 for WriteValue's value array).
 * @param offset_out receives "offset", or 0 when absent. May be NULL.
 * @param mtu_out    receives "mtu", or 0 when absent. May be NULL.
 * @param dev_out    receives "device", or "" when absent. May be NULL.
 * @param dev_sz     size of @dev_out.
 */
static void parse_gatt_options(DBusMessage *msg, int skip_args,
							   uint16_t *offset_out, uint16_t *mtu_out,
							   char *dev_out, size_t dev_sz)
{
	if (offset_out) *offset_out = 0;
	if (mtu_out)    *mtu_out = 0;
	if (dev_out && dev_sz) dev_out[0] = '\0';

	DBusMessageIter iter;
	if (!dbus_message_iter_init(msg, &iter))
		return;
	for (int i = 0; i < skip_args; i++) {
		if (!dbus_message_iter_next(&iter))
			return;
	}
	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		return;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&iter, &dict);
	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry;
		dbus_message_iter_recurse(&dict, &entry);

		const char *key = NULL;
		if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(&entry, &key);
			if (key && dbus_message_iter_next(&entry) &&
				dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {
				DBusMessageIter var;
				dbus_message_iter_recurse(&entry, &var);
				int vt = dbus_message_iter_get_arg_type(&var);

				if (vt == DBUS_TYPE_UINT16) {
					dbus_uint16_t v = 0;
					dbus_message_iter_get_basic(&var, &v);
					if (offset_out && strcmp(key, "offset") == 0)
                        *offset_out = (uint16_t)v;
					else if (mtu_out && strcmp(key, "mtu") == 0)
                        *mtu_out = (uint16_t)v;
				} else if (vt == DBUS_TYPE_OBJECT_PATH &&
						   dev_out && dev_sz && strcmp(key, "device") == 0) {
					const char *p = NULL;
					dbus_message_iter_get_basic(&var, &p);
					if (p)
						snprintf(dev_out, dev_sz, "%s", p);
				}
			}
		}
		dbus_message_iter_next(&dict);
	}
}

/**
 * Handle ReadValue method call on a characteristic.
 */
static DBusMessage *handle_read_value(DBusMessage *msg, gatt_char_t *chr)
{
	/*
     * Options carry the blob-read offset, the negotiated MTU and — the reason we
     * parse them this early — the client's object path, which identifies this
     * link for the auth gate (§19.4).
     */
	uint16_t offset = 0, mtu = 0;
	char cid[GATT_V2_CONN_ID_MAX];
	parse_gatt_options(msg, 0, &offset, &mtu, cid, sizeof(cid));

	/*
     * What we will reply with. Most characteristics stage their value in
     * chr->value and let the generic offset slicing below cut it; the fragmented
     * ones (A1/A4) hand back a pointer into their own buffer with the offset
     * already applied.
     */
	const uint8_t *body = chr->value;
	uint16_t body_len = chr->value_len;
	bool offset_applied = false;

	/* ── v2 service (§19) — the only service we expose ── */
	if (strcmp(chr->uuid, GATT_V2_MAC_UUID) == 0) {
		/* A6: the MAC the device actually advertises with, lowercase hex with no
         * separators. Public — no auth needed (the app uses it for key
         * derivation/labelling). Shared helper, so TI emits identical bytes. */
		char mac[16];
		int n = gatt_v2_mac_hex(mac, sizeof(mac));
		chr->value_len = (uint16_t)n;
		if (n > 0) memcpy(chr->value, mac, (size_t)n);
	}
	else if (strcmp(chr->uuid, GATT_V2_READ_UUID) == 0 ||
			 strcmp(chr->uuid, GATT_V2_STATUS_UUID) == 0) {
		/*
         * A1 (config read) / A4 (status): auth-gated AND fragmented (§19.3).
         * The shared module owns the whole policy — the 0x00 sentinel when
         * unauthenticated, the snapshot, the per-connection cursor and the blob
         * offset — so the TI path produces identical bytes.
         *
         * It applies `offset` itself, which is why the generic slicing below is
         * skipped: a fragment cursor and a byte offset are different things, and
         * letting both act would hand out a slice of the wrong fragment.
         */
		gatt_v2_tx_chr_t which =
			(strcmp(chr->uuid, GATT_V2_READ_UUID) == 0) ? GATT_V2_TX_CONFIG
														: GATT_V2_TX_STATUS;
		/*
         * `mtu` comes from the same options dictionary. BlueZ supplies it; when
         * it does not, the shared module falls back to the 23-byte default, which
         * costs extra fragments but never produces one the link cannot carry.
         */
		int ret = gatt_v2_read_body(cid, which, offset, mtu, &body, &body_len);
		if (ret == -ERANGE)
			return dbus_message_new_error(msg, "org.bluez.Error.InvalidOffset",
										  "no fragment to continue");
		if (ret < 0)
			return dbus_message_new_error(msg, "org.bluez.Error.Failed",
										  "read failed");
		offset_applied = true;
	}
	else if (strcmp(chr->uuid, GATT_V2_WRITE_UUID) == 0) {
		/*
         * A2: mirrors the outcome of the last write, so a client can treat the
         * command attribute as request/response instead of subscribing to A3.
         * The result is stored before the write reply is sent, so a read that
         * follows a write always observes that write's outcome.
         */
		const char *s = gatt_v2_result_json();
		chr->value_len = (uint16_t)strlen(s);
		memcpy(chr->value, s, chr->value_len);
	}
	else if (strcmp(chr->uuid, GATT_V2_AUTH_UUID) == 0) {
		/*
         * A5: the verdict of the last credential write. Public on purpose — it
         * reveals only whether authentication succeeded, which the client that
         * wrote the credential already knows, and never the credential itself.
         */
		const char *s = gatt_v2_auth_json();
		chr->value_len = (uint16_t)strlen(s);
		memcpy(chr->value, s, chr->value_len);
	}
	else if (strcmp(chr->uuid, GATT_V2_RESULT_UUID) == 0) {
		/*
         * A3: the command-outcome channel the phone app subscribes to and reads.
         * Same bytes A2 mirrors — one stored value, two attributes exposing it.
         */
		const char *s = gatt_v2_result_json();
		chr->value_len = (uint16_t)strlen(s);
		memcpy(chr->value, s, chr->value_len);
	}
	else if (strcmp(chr->uuid, GATT_V2_UPGRADE_UUID) == 0) {
		/* A7: firmware upgrade state, owned by the shared v2 core. */
		const char *s = gatt_v2_upgrade_json();
		chr->value_len = (uint16_t)strlen(s);
		memcpy(chr->value, s, chr->value_len);
	}

	/*
     * Honour the "offset" option: a value longer than ATT_MTU-1 is fetched by the
     * client as a sequence of blob reads, and BlueZ asks us for each slice. The
     * reply must start AT the offset — returning the whole value every time would
     * hand the client a duplicated prefix and silently corrupt anything longer
     * than one MTU.
     *
     * Re-read chr->value here rather than trusting the values captured before the
     * branches: the staging branches above write chr->value_len.
     */
	if (!offset_applied) {
		if (offset > chr->value_len)
			return dbus_message_new_error(msg, "org.bluez.Error.InvalidOffset",
										  "offset beyond value length");
		body = chr->value + offset;
		body_len = (uint16_t)(chr->value_len - offset);
	}

	const uint8_t *slice = body;
	int slice_len = (int)body_len;

	if (offset || mtu)
		syslog(LOG_DEBUG, "gatt_server: read %s offset=%u mtu=%u -> %d bytes",
			   chr->uuid, offset, mtu, slice_len);

	/* Build reply with byte array.
     *
     * dbus_message_iter_append_fixed_array() expects the address of a POINTER
     * to the array data (const void **), NOT the address of the array itself.
     * Passing &chr->value (type uint8_t(*)[512]) happens to have the same
     * numeric value but is the wrong level of indirection; use an explicit
     * pointer variable and pass its address to be correct and safe. */
	DBusMessage *reply = dbus_message_new_method_return(msg);
	if (!reply) return NULL;

	DBusMessageIter iter, array;
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "y", &array);
	dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
										 &slice, slice_len);
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

	/*
     * Reject a write that does not start at 0, instead of ignoring the offset:
     * treating a slice as a complete payload would let a fragment be dispatched
     * as a whole command or credential.
     *
     * This does NOT refuse the app's long writes, and reading it that way sends
     * you looking in the wrong place. The app cuts its configuration into
     * 510-byte chunks whatever MTU was negotiated (§19.3), so a 512-byte value
     * arrives as an ATT Long Write — and BlueZ reassembles it below us:
     * `store_prep_data()` in src/shared/gatt-server.c merges Prepare Writes that
     * are contiguous on the same handle into one queue entry, so Execute Write
     * produces a single WriteValue with offset 0 and all 512 bytes. The TI path
     * behaves the same way for the same reason (its stack reassembles on-chip),
     * which is why both transports see one write, not three.
     *
     * What this guard actually catches is a prepared write that starts partway
     * into the attribute, or one BlueZ could not merge because the offsets were
     * not contiguous — neither of which a correct client produces here.
     */
	uint16_t offset = 0;
	char cid[GATT_V2_CONN_ID_MAX];
	parse_gatt_options(msg, 1, &offset, NULL, cid, sizeof(cid));
	if (offset != 0) {
		syslog(LOG_WARNING, "gatt_server: rejecting long write to %s (offset=%u)",
			   chr->uuid, offset);
		return dbus_message_new_error(msg, "org.bluez.Error.NotSupported",
									  "long writes are not supported");
	}

	syslog(LOG_DEBUG, "gatt_server: write to %s, len=%d", chr->uuid, len);

	/* ── v2 Auth (A5, e063c3a5) ── */
	if (strcmp(chr->uuid, GATT_V2_AUTH_UUID) == 0) {
		/* Plaintext credential; verify against /etc/shadow root. The buffer
         * is not NUL-terminated, so copy into a bounded temporary. */
		char cred[128];
		size_t clen = (len < (int)sizeof(cred) - 1) ? (size_t)len
													 : sizeof(cred) - 1;
		memcpy(cred, bytes, clen);
		cred[clen] = '\0';

		bool ok = ble_auth_verify_root_password(cred);
		/* Wipe the plaintext from the stack promptly. */
		memset(cred, 0, sizeof(cred));

		/* Authentication belongs to this link only (§19.4). */
		gatt_v2_conn_set_authenticated(cid, ok);
		/* Publish the verdict on A5 itself so the phone need not infer it. */
		gatt_v2_auth_set(ok);
		return dbus_message_new_method_return(msg);
	}

	/* ── v2 Write (A2, e063c3a2) — commands and configuration ── */
	if (strcmp(chr->uuid, GATT_V2_WRITE_UUID) == 0) {
		/*
         * The shared dispatcher owns the whole policy, including the auth rules
         * (upgrade_ack is allowed unauthenticated, everything else is not) and
         * publishing the outcome on A3/A2/A7 — so BlueZ and TI behave identically.
         * We only translate the verdict into a D-Bus reply.
         */
		gatt_v2_cmd_result_t cr =
			gatt_v2_cmd_handle_write(bytes, (uint16_t)len,
									 gatt_v2_conn_get(cid));

		switch (cr) {
		case GATT_V2_CMD_OK:
			return dbus_message_new_method_return(msg);
		case GATT_V2_CMD_DENIED:
			return dbus_message_new_error(msg, "org.bluez.Error.NotAuthorized",
										  gatt_v2_result_json());
		case GATT_V2_CMD_UNSUPPORTED:
			return dbus_message_new_error(msg, "org.bluez.Error.NotSupported",
										  gatt_v2_result_json());
		case GATT_V2_CMD_INVALID:
		default:
			return dbus_message_new_error(msg, "org.bluez.Error.InvalidArguments",
										  gatt_v2_result_json());
		}
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
 *  Per-connection authentication lifetime (§19.4)
 * ══════════════════════════════════════════════════════════════════ */

/**
 * React to `org.bluez.Device1.Connected` changing.
 *
 * Authentication is per-connection: a phone that authenticated must not leave
 * the device authenticated for whoever connects next. BlueZ has no GATT-level
 * "disconnected" callback for a server application, so we watch the Device1
 * property instead (the TI path gets this for free from NPI EVT_DISCONNECTED).
 *
 * The auth state is cleared on BOTH edges. Clearing on connect matters as much
 * as on disconnect: if a disconnect signal is ever missed, a freshly connected
 * phone would otherwise inherit the previous session's authentication.
 */
static void handle_device_connected_changed(DBusMessage *msg)
{
	DBusMessageIter iter;
	if (!dbus_message_iter_init(msg, &iter))
		return;

	/* arg0: interface name */
	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return;
	const char *iface = NULL;
	dbus_message_iter_get_basic(&iter, &iface);
	if (!iface || strcmp(iface, "org.bluez.Device1") != 0)
		return;

	/* arg1: changed_properties a{sv} */
	if (!dbus_message_iter_next(&iter) ||
		dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		return;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry;
		dbus_message_iter_recurse(&dict, &entry);

		const char *key = NULL;
		if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(&entry, &key);

			if (key && strcmp(key, "Connected") == 0 &&
				dbus_message_iter_next(&entry) &&
				dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {

				DBusMessageIter var;
				dbus_message_iter_recurse(&entry, &var);
				if (dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_BOOLEAN) {
					dbus_bool_t connected = FALSE;
					dbus_message_iter_get_basic(&var, &connected);

					/*
                     * The signal's object path IS the device, and it is the same
                     * path BlueZ passes as the "device" read/write option — so it
                     * is the key this link's state is filed under.
                     */
					const char *dev = dbus_message_get_path(msg);

					/*
                     * Drop on both edges. On disconnect for the obvious reason;
                     * on connect because BlueZ reuses a device path across
                     * sessions, so a missed disconnect would otherwise hand a
                     * freshly connected phone the previous session's
                     * authentication and its half-received configuration.
                     */
					gatt_v2_conn_drop(dev);
					gatt_v2_auth_reset();
					syslog(LOG_INFO, "gatt_server: %s %s — link state cleared",
						   dev ? dev : "(unknown device)",
						   connected ? "connected" : "disconnected");

					/*
                     * A disconnect is the one moment an advertisement that could
                     * never be registered gets a chance, so take it.
                     *
                     * adv_instance_id is only non-zero after a registration
                     * succeeded, so this fires exactly in the case that used to be
                     * terminal: the daemon started while a phone was already
                     * connected, RegisterAdvertisement was refused, and nothing
                     * else in the daemon ever asks again. On the nRF52840 the
                     * refusal is the kernel's — connectable advertising needs a
                     * free connection slot and that build has one — so the
                     * condition genuinely clears here and nowhere else.
                     *
                     * Armed, not called: this runs inside the D-Bus message
                     * filter, and start_advertising() sits in a nested dispatch
                     * loop waiting for BlueZ to answer RegisterAdvertisement
                     * (BlueZ calls GetAll on us first, so it has to). Calling it
                     * from here would re-enter D-Bus dispatch from a D-Bus
                     * callback — the same shape as the stall measured on eap115
                     * and documented at adv_status_changed_cb, where ~20 s of
                     * frozen uloop swallowed an entire config apply.
                     *
                     * Deliberately not hooked to adv_timer either: every failed
                     * attempt costs up to ADV_REGISTER_WAIT_MS of frozen uloop, so
                     * retrying on a 5 s tick would turn one stall into a recurring
                     * one. Once per disconnect edge is enough, because that edge
                     * is the only thing that changes the answer.
                     */
					if (!connected && gs.adv_instance_id <= 0)
						uloop_timeout_set(&gs.adv_retry, 0);
				}
			}
		}
		dbus_message_iter_next(&dict);
	}
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

	/* Diagnostic: log every incoming message so we can confirm whether BlueZ's
     * ReadValue/WriteValue method calls actually reach this connection. */
	syslog(LOG_DEBUG, "gatt_server: D-Bus msg iface=%s member=%s path=%s",
		   iface, member, path);

	/*
     * Signals: watch BlueZ device connect/disconnect so authentication stays
     * per-connection. Our own outgoing notifications are also PropertiesChanged
     * but carry arg0 = GattCharacteristic1, and the match rule filters on
     * Device1, so there is no feedback loop. Return NOT_YET_HANDLED so other
     * filters (e.g. ble_advertise) still see the signal.
     */
	if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL) {
		if (strcmp(iface, "org.freedesktop.DBus.Properties") == 0 &&
			strcmp(member, "PropertiesChanged") == 0)
			handle_device_connected_changed(msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

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
		/*
         * Do not answer a call that was sent with NO_REPLY_EXPECTED. bluetoothd
         * invokes StartNotify/StopNotify without a reply callback, so dbus-daemon
         * has no pending call to match our method_return against; it then falls
         * through to the policy rules, matches none, and logs
         *
         *   Rejected send message, 0 matched rules; type="method_return" …
         *   requested_reply="0" destination=":1.0" (bluetoothd)
         *
         * once per subscribe and unsubscribe, at auth.notice AND daemon.err. The
         * reply was never wanted, so the only damage is log noise — but it is
         * noise that buries the lines that matter during provisioning.
         */
		if (dbus_message_get_no_reply(msg)) {
			/* ReadValue/WriteValue always want a reply, so reaching this with one
             * of those would mean we just discarded a fragment. Say so. */
			if (strcmp(member, "StartNotify") != 0 &&
				strcmp(member, "StopNotify") != 0)
				syslog(LOG_WARNING, "gatt_server: dropped the reply to %s — the "
					   "caller set NO_REPLY_EXPECTED", member);
			dbus_message_unref(reply);
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* ══════════════════════════════════════════════════════════════════
 *  uloop integration for the BlueZ D-Bus connection
 *
 *  Without this, incoming ReadValue/WriteValue method calls arriving on
 *  gs.conn after RegisterApplication are never dispatched (the connection is
 *  only pumped during the synchronous register loop). Expose the fd so main()
 *  can watch it in uloop, and a dispatch entry point to drain it.
 * ══════════════════════════════════════════════════════════════════ */

int gatt_server_get_dbus_fd(void)
{
	if (!gs.conn)
		return -1;
	int fd = -1;
	if (!dbus_connection_get_unix_fd(gs.conn, &fd))
		return -1;
	return fd;
}

void gatt_server_dispatch(void)
{
	if (!gs.conn)
		return;
	/* Read available data and dispatch all pending messages without blocking.
     * exit_on_disconnect is disabled (see gatt_server_start) so a dropped bus
     * will not kill the daemon here; we simply stop dispatching. */
	dbus_connection_read_write(gs.conn, 0);
	while (dbus_connection_get_is_connected(gs.conn) &&
		   dbus_connection_dispatch(gs.conn) == DBUS_DISPATCH_DATA_REMAINS)
		;
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

/*
 * Register the single uCentral Provisioning v2 service (§19).
 *
 * The legacy FE00/FE10 16-bit services are retired — the phone app only speaks
 * v2, and keeping a second, unauthenticated surface that could reboot or
 * reconfigure the device would be a needless risk.
 *
 * Characteristic order matches the reference implementation exactly
 * (char0 = A1 … char6 = A7); gatt_v2_chr_t indexes into this array.
 */
static int setup_services(void)
{
	gs.service_count = 1;

	gatt_service_t *svc = &gs.services[0];
	svc->uuid = GATT_V2_SRV_UUID;
	svc->path = GATT_V2_SRV_PATH;
	svc->primary = true;
	svc->char_count = 7;

	static const char *v2_read_flags[]  = { "read" };
	static const char *v2_rw_notify[]   = { "read", "notify" };
	static const char *v2_wrn_flags[]   = { "write", "read", "notify" };

	/*
     * A2 and A5 are request/response on one attribute: write the command or the
     * credential, read back the outcome. "notify" is there so an outcome that
     * only becomes known later (the config apply of §19.3) can be pushed instead
     * of polled.
     *
     * This deviates from the reference, which declares both write-only
     * (wifi_provision.py:787 and :1203), and the deviation has been investigated
     * and cleared — read the rest of this comment before "fixing" it again.
     *
     * "notify" makes BlueZ add a CCCD, and a CCCD is an attribute, so the two
     * extra descriptors shift every ATT handle after char1. Confirmed on the wire
     * with btmon: our service spans 0x0039-0x004b with CCCDs at 0x003e and
     * 0x0046, where the reference spans 0x0039-0x0049. That made it the prime
     * suspect while chasing a provisioning failure on eap115, and it was wrong:
     * the cause was bluetoothd's battery plugin tearing the link down after a
     * bonding attempt it started itself (see warn_if_bluetoothd_battery_plugin).
     *
     * With that fixed, the shipped app authenticates and provisions against this
     * exact declaration on both transports. Two reasons to leave it alone:
     *
     *  - It is measured, not assumed. eap111 (TI firmware) has always declared
     *    A2/A5 as WRITE|READ|NOTIFY and the app works there; eap115 (BlueZ) now
     *    works with the same declaration. The extra handles do not bother it.
     *  - The TI firmware's attribute table is deliberately kept identical to this
     *    one (gatt_provision.c:71-77). Changing only the BlueZ side would hand the
     *    same app two different databases depending on which chip it reached —
     *    that is a new risk, not a removed one.
     *
     * So: matching the reference here would trade a configuration proven on both
     * transports for an untested one, to gain textual fidelity and nothing else.
     * If it ever does need to change, change the firmware in the same commit.
     */
	setup_char(&svc->chars[GATT_V2_CHR_READ], GATT_V2_READ_UUID,
			   GATT_V2_SRV_PATH "/char0", GATT_V2_SRV_PATH, v2_read_flags, 1);
	setup_char(&svc->chars[GATT_V2_CHR_WRITE], GATT_V2_WRITE_UUID,
			   GATT_V2_SRV_PATH "/char1", GATT_V2_SRV_PATH, v2_wrn_flags, 3);
	setup_char(&svc->chars[GATT_V2_CHR_RESULT], GATT_V2_RESULT_UUID,
			   GATT_V2_SRV_PATH "/char2", GATT_V2_SRV_PATH, v2_rw_notify, 2);
	setup_char(&svc->chars[GATT_V2_CHR_STATUS], GATT_V2_STATUS_UUID,
			   GATT_V2_SRV_PATH "/char3", GATT_V2_SRV_PATH, v2_read_flags, 1);
	setup_char(&svc->chars[GATT_V2_CHR_AUTH], GATT_V2_AUTH_UUID,
			   GATT_V2_SRV_PATH "/char4", GATT_V2_SRV_PATH, v2_wrn_flags, 3);
	setup_char(&svc->chars[GATT_V2_CHR_MAC], GATT_V2_MAC_UUID,
			   GATT_V2_SRV_PATH "/char5", GATT_V2_SRV_PATH, v2_read_flags, 1);
	setup_char(&svc->chars[GATT_V2_CHR_UPGRADE], GATT_V2_UPGRADE_UUID,
			   GATT_V2_SRV_PATH "/char6", GATT_V2_SRV_PATH, v2_rw_notify, 2);

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

	/*
     * Carry the v2 status bytes (§19.6). The phone app scans by manufacturer
     * data, not by service UUID, so without these four bytes it cannot label
     * this device in its scan list.
     */
	uint8_t mfr[ADV_MFR_DATA_LEN];
	int mfr_len = adv_status_mfr_data(mfr);

	int ret = ble_adv_start_connectable(gs.device_name, ADV_MFR_COMPANY_ID,
										mfr, (uint8_t)mfr_len);
	if (ret < 0) {
		syslog(LOG_ERR, "gatt_server: LE advertising failed: %d", ret);
		return ret;
	}
	gs.adv_instance_id = ret;

	syslog(LOG_INFO, "gatt_server: LE advertising started (instance=%d, name=%s, "
		   "mfr=%02X%02X%02X%02X)",
		   gs.adv_instance_id, gs.device_name,
		   mfr[0], mfr[1], mfr[2], mfr[3]);
	return 0;
}

/**
 * Republish the advertisement when its status bytes changed.
 *
 * BlueZ reads the advertisement properties only at registration, so a change is
 * invisible until the advertisement is registered again — and that restarts the
 * advertising set. Hence the comparison: republish on change, not on a timer.
 *
 * Driven by two things: a timer, because `link` changes behind our back, and an
 * immediate call whenever device_status moves, so upgrade and config progress
 * show up without waiting for the next tick.
 */
static void adv_refresh(void)
{
	if (gs.adv_instance_id <= 0)
		return;

	uint8_t mfr[ADV_MFR_DATA_LEN];
	adv_status_mfr_data(mfr);

	/*
     * No local "have the bytes changed?" gate here, on purpose.
     *
     * ble_adv_update_mfr_data() makes the same comparison *and* knows whether
     * BlueZ is actually holding the advertisement (`same && registered`), which is
     * the condition that matters. The gate used to live here, against a copy of
     * the bytes kept in `gs`, and it quietly disabled the retry after a failed
     * update: that copy still held the last *successfully registered* value, so
     * when the transient that
     * triggered the failed update went away — one uplink flap on eap115 — the
     * recomputed bytes matched the cache again and this returned early with the
     * advertisement still unregistered. 44 s with the device undiscoverable and
     * nothing retrying it.
     *
     * The call below is cheap when there is nothing to do: it returns 0 without
     * touching D-Bus.
     */
	int ret = ble_adv_update_mfr_data(gs.adv_instance_id, mfr, ADV_MFR_DATA_LEN);
	if (ret < 0) {
		/*
         * Re-registering can legitimately fail — BlueZ may refuse while a client
         * is connected. Keep the instance id so the next tick tries again: giving
         * up would leave the device permanently undiscoverable, which is far worse
         * than a status byte a few seconds stale. ble_adv_update_mfr_data() has
         * already put the previous advertisement back if it could.
         */
		syslog(LOG_WARNING, "gatt_server: advertisement refresh failed (%d) — "
			   "will retry on the next tick", ret);
		return;
	}
	if (ret == 0)
		return;                 /* already current; nothing was re-registered */

	syslog(LOG_INFO, "gatt_server: advertisement status now %02X%02X%02X%02X",
		   mfr[0], mfr[1], mfr[2], mfr[3]);
}

static void adv_refresh_timer_cb(struct uloop_timeout *t)
{
	adv_refresh();
	uloop_timeout_set(t, ADV_REFRESH_INTERVAL_MS);
}

/** Deferred republish — see adv_status_changed_cb for why it is deferred. */
static void adv_refresh_now_cb(struct uloop_timeout *t)
{
	(void)t;
	adv_refresh();
}

/**
 * Deferred retry of a registration that never succeeded.
 *
 * Armed by the Device1 PropertiesChanged handler on a disconnect; see the comment
 * there for why it is armed rather than called, and why only on that edge.
 *
 * The guard is re-checked here because arming and firing are separated by a trip
 * through uloop: another device may have connected in between, which puts the
 * connection slot back out of reach and makes the attempt pointless, or a stop
 * may have torn the server down.
 */
static void adv_retry_cb(struct uloop_timeout *t)
{
	(void)t;

	if (!gs.running || gs.adv_instance_id > 0)
		return;

	syslog(LOG_INFO, "gatt_server: link went away — retrying the advertisement "
		   "registration that failed earlier");
	start_advertising();
}

/**
 * device_status moved — republish, but on the next uloop iteration rather than
 * on the caller's stack.
 *
 * Re-registering an advertisement is a synchronous D-Bus round trip: BlueZ calls
 * GetAll on our advertisement object before it replies, so
 * register_advertisement_once() has to sit in a nested dispatch loop until the
 * reply lands. Running that straight from this callback puts it on whatever stack
 * happened to change the status — and the worst such caller is
 * config_apply_start(), where it lands between telling the phone we started and
 * actually starting:
 *
 *     gatt_v2_result_set("{\"status\":\"applying\"}")   -> notified on A3
 *     gatt_v2_set_device_status(CFG_APPLYING)           -> used to block here
 *     ca_start_ucentral(cfg)                            -> never reached
 *
 * Measured on eap115: an A2 apply published `applying`, stalled in
 * RegisterAdvertisement, and never wrote /etc/ucentral/ucentral.cfg.* nor logged a
 * single `config_apply:` line. Worst case was ~20 s of frozen uloop (two attempts
 * at ADV_REGISTER_WAIT_MS plus the 200 ms backoff between them).
 *
 * Deferring costs the advertisement a few milliseconds of staleness and takes
 * D-Bus off the apply path completely.
 */
static void adv_status_changed_cb(uint8_t status)
{
	(void)status;
	uloop_timeout_set(&gs.adv_now, 0);
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
	/*
     * The policy lives in the shipped ble.uci (`allow_factory_reset '1'`), not
     * in this fallback. The fallback is only reached when the option is absent
     * from /etc/config/ble, which is an abnormal state, and for a destructive
     * action the safe answer there is "no".
     *
     * It also has to agree with the other two readers — app_status.c and, more
     * importantly, gatt_v2_cmd.c, which is the one that actually refuses the
     * command. Three readers with different fallbacks would mean a device with a
     * missing option reports the capability and then denies it.
     */
	gs.allow_factory_reset = uci_app_get_bool("ble", "gatt_server", "allow_factory_reset", false);
	gs.allow_config = uci_app_get_bool("ble", "gatt_server", "allow_config", true);

	uci_app_get_string("ble", "gatt_server", "device_name",
					   gs.device_name, sizeof(gs.device_name), GS_NAME_SENTINEL);
	if (strcmp(gs.device_name, GS_NAME_SENTINEL) == 0) {
		char derived[sizeof(gs.device_name)] = {0};
		if (derive_device_name(derived, sizeof(derived))) {
			syslog(LOG_INFO, "gatt_server: device name derived from the board "
							 "(%s -> %s)", GS_NAME_SENTINEL, derived);
			snprintf(gs.device_name, sizeof(gs.device_name), "%s", derived);
		} else {
			syslog(LOG_WARNING, "gatt_server: cannot read %s — advertising as "
								"'%s'", GS_BOARD_NAME_FILE, gs.device_name);
		}
	}

	uci_app_get_string("ble", "gatt_server", "pin",
					   gs.pin, sizeof(gs.pin), "");

	/* Initialize config provisioning module */
	config_provision_init(gs.allow_config);

	/*
     * Shared v2 core: A2/A7 values, advertisement device_status, and the
     * firmware upgrade state machine. Transport-agnostic, so the TI path
     * initializes the very same modules.
     */
	gatt_v2_init();
	gatt_v2_upgrade_init();

	/* Advertisement status bytes (§19.6): model id from UCI, uplink from ubus. */
	adv_status_init();

	/*
     * A4 device status (§19.5). Registers itself as the fragmented-read snapshot
     * provider, so it serves both transports. Done here, before the event loop,
     * because it reads the serial number with a fork+exec that must not land on
     * the read path.
     */
	status_doc_init();

	/* A1 configuration read (§19.5.4): uCentral active config, or the live radio
     * topology on a device that has never been provisioned. */
	config_doc_init();

	/* A2 configuration apply (§19.5.5) — the first thing here that changes the
     * device, so its timers and child-process state are set up explicitly. */
	config_apply_init();

	/*
     * Per-connection state starts empty (§19.4): authentication and the config
     * reassembler are both filed per link, and dropped on every connect and
     * disconnect edge — see handle_device_connected_changed() here and the
     * CONNECTED/DISCONNECTED events on the TI path.
     */
	gatt_v2_conn_reset_all();

	/*
     * `ota=` below reports ble.gatt_server.allow_ota, which is deliberately
     * NOT a gate: the reference implementation has no such switch, and the phone
     * app expects firmware upgrade to work out of the box, so gating it would
     * break A7 on every device that never set the option. It is reported because
     * integrators read it, and left unenforced on purpose — do not "fix" it into
     * a gate without changing the app contract too.
     */
	syslog(LOG_INFO, "gatt_server: init (name=%s ota=%d fr=%d cfg=%d)",
		   gs.device_name, gs.allow_ota, gs.allow_factory_reset, gs.allow_config);
	return 0;
}

/**
 * Warn when bluetoothd is running with its battery plugin enabled.
 *
 * This is a diagnostic, never a gate. It exists because that plugin costs us a
 * provisioning session and gives no hint that it is responsible.
 *
 * What happens: a phone connects to us, bluetoothd turns around and acts as a
 * GATT *client* on the phone, and tries to read the phone's Battery Level. iOS
 * answers `Insufficient Authentication`, so bluetoothd escalates by sending an
 * SMP Security Request. The phone replies with a Pairing Request, bluetoothd has
 * no agent and a non-pairable adapter so it answers `Pairing not supported`, and
 * then — because *its own* bonding attempt failed — it tears down the link about
 * 1-2 s later.
 *
 * From the outside that looks like: connections that live three seconds, and an
 * app stuck re-asking for the password because it never has a link long enough to
 * write the credential on. Nothing in this daemon's log points at pairing, and
 * nothing we can do from here stops it: the probe is bluetoothd talking to the
 * peer, not to us.
 *
 * The fix is on the bluetoothd command line: `-P battery` (equivalently
 * `--noplugin=battery`). Confirmed on eap115 with an iPad.
 */
static void warn_if_bluetoothd_battery_plugin(void)
{
	DIR *proc = opendir("/proc");
	if (!proc)
		return;

	struct dirent *de;
	while ((de = readdir(proc)) != NULL) {
		if (de->d_name[0] < '0' || de->d_name[0] > '9')
			continue;

		char path[64];
		snprintf(path, sizeof(path), "/proc/%s/cmdline", de->d_name);

		int fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;

		/* argv, NUL-separated. 512 covers any sane bluetoothd invocation. */
		char buf[512];
		ssize_t n = read(fd, buf, sizeof(buf) - 1);
		close(fd);
		if (n <= 0)
			continue;
		buf[n] = '\0';

		/* argv[0] must be bluetoothd, not something that merely mentions it. */
		const char *base = strrchr(buf, '/');
		base = base ? base + 1 : buf;
		if (strcmp(base, "bluetoothd") != 0)
			continue;

		/*
         * Walk the remaining arguments looking for the plugin being excluded,
         * either as `-P battery` or `--noplugin=battery`. A comma-separated list
         * is why this is a substring test rather than an equality test.
         */
		bool disabled = false;
		bool prev_was_flag = false;

		for (ssize_t i = 0; i < n; ) {
			const char *arg = &buf[i];
			size_t len = strlen(arg);

			/* `-P battery`, `-Pbattery` and `--noplugin=battery` all count. */
			if (prev_was_flag && strstr(arg, "battery"))
				disabled = true;
			if (strncmp(arg, "--noplugin", 10) == 0 && strstr(arg, "battery"))
				disabled = true;
			if (strncmp(arg, "-P", 2) == 0 && len > 2 && strstr(arg, "battery"))
				disabled = true;

			prev_was_flag = (strcmp(arg, "-P") == 0 ||
							 strcmp(arg, "--noplugin") == 0);
			i += (ssize_t)len + 1;
		}

		if (!disabled) {
			syslog(LOG_WARNING,
				   "gatt_server: bluetoothd (pid %s) is running WITHOUT "
				   "'-P battery'. It will read the phone's Battery Level, fail "
				   "with Insufficient Authentication, start a bonding attempt "
				   "that cannot succeed, and then drop the link ~2 s later. "
				   "Provisioning will look like 3-second connections and an app "
				   "that keeps asking for the password. Add '-P battery' to the "
				   "bluetoothd command line.", de->d_name);
		} else {
			syslog(LOG_INFO, "gatt_server: bluetoothd battery plugin is "
				   "disabled — incoming links will not be torn down by a failed "
				   "bonding attempt");
		}
		break;
	}

	closedir(proc);
}

/**
 * Set one property on org.bluez.Adapter1.
 *
 * @param prop property name.
 * @param type DBUS_TYPE_STRING or DBUS_TYPE_BOOLEAN.
 * @param sig  the variant signature matching @type ("s" or "b").
 * @param val  pointer to the value, as libdbus wants it (`const char **` for a
 *             string, `dbus_bool_t *` for a boolean).
 *
 * Failures are logged and swallowed. None of these properties is load-bearing
 * enough to abort startup over, and an adapter that refuses one of them still
 * serves GATT.
 */
static void adapter_set_prop(const char *prop, int type, const char *sig,
							 const void *val)
{
	DBusMessage *msg = dbus_message_new_method_call(
		BLUEZ_SERVICE, gs.adapter_path,
		"org.freedesktop.DBus.Properties", "Set");
	if (!msg)
		return;

	const char *iface = "org.bluez.Adapter1";
	DBusMessageIter iter, variant;

	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &prop);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, sig, &variant);
	dbus_message_iter_append_basic(&variant, type, val);
	dbus_message_iter_close_container(&iter, &variant);

	DBusError err;
	dbus_error_init(&err);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(
		gs.conn, msg, DBUS_TIMEOUT_MS, &err);
	dbus_message_unref(msg);

	if (reply)
		dbus_message_unref(reply);
	if (dbus_error_is_set(&err)) {
		syslog(LOG_WARNING, "gatt_server: could not set Adapter1.%s: %s",
			   prop, err.message ? err.message : "unknown");
		dbus_error_free(&err);
	}
}

static int gatt_server_start(void)
{
	/*
     * On the TI (ti_npi) UART transport the GATT server and all advertising run
     * on the CC2652R1 firmware and are driven over NPI (see npi_gatt_handler).
     * There is no BlueZ adapter to talk to, so skip the entire D-Bus GATT /
     * advertising registration here — otherwise BlueZ RegisterApplication /
     * RegisterAdvertisement fail and spam the log. This keeps the BlueZ path
     * unchanged while making the TI path quiet and self-consistent.
     */
	{
		extern const char *ble_get_chip_profile_name(void);
		const char *cp = ble_get_chip_profile_name();
		if (cp && strcmp(cp, "ti_npi") == 0) {
			gs.running = true;
			syslog(LOG_INFO, "gatt_server: TI ti_npi transport — GATT/adv handled "
				   "on-chip via NPI, skipping BlueZ registration");
			return 0;
		}
	}

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

	/*
     * CRITICAL: a libdbus *private* connection defaults to exit_on_disconnect =
     * TRUE — if the bus/peer drops, libdbus calls exit() on the whole process.
     * Once this connection's fd is dispatched from uloop (so we can service
     * characteristic ReadValue/WriteValue), any disconnect message that gets
     * dispatched would otherwise silently kill the daemon (looks like a
     * "restart"). Disable it so we control our own lifecycle.
     */
	dbus_connection_set_exit_on_disconnect(gs.conn, FALSE);

	build_adapter_path("hci0");

	/* Always initialize ble_adv module (shared by GATT and iBeacon) */
	ble_adv_init(gs.conn, gs.adapter_path);

	/* Adapter Alias, so BlueZ's default name matches the advertised one. */
	if (gs.device_name[0]) {
		const char *name = gs.device_name;
		adapter_set_prop("Alias", DBUS_TYPE_STRING, "s", &name);
	}

	/*
     * Pairable = false. Authentication here is application-layer (A5 against the
     * /etc/shadow root hash); nothing in the service requires an encrypted link,
     * and no Agent is registered to answer a pairing request.
     *
     * Leaving the adapter pairable is not neutral. A phone that decides to bond —
     * either because the app asks for it or because the platform does it on its
     * own — gets an adapter that advertises "yes, you may pair" and then cannot
     * finish, because there is no agent. The phone's recovery is to drop the
     * link and try again, which shows up as a connection that lives ~3 s and an
     * app stuck re-asking for the password because it never has a link long
     * enough to write the credential on.
     *
     * The TI path has always done this, one layer down: the firmware is built
     * with GAPBOND_PAIRING_MODE_NO_PAIRING, so the chip refuses pairing outright
     * and the phone moves on. This is the BlueZ half of the same decision, and
     * the reference sets it for the same stated reason.
     */
	{
		dbus_bool_t off = FALSE;
		adapter_set_prop("Pairable", DBUS_TYPE_BOOLEAN, "b", &off);
		syslog(LOG_INFO, "gatt_server: Adapter1.Pairable = false "
			   "(auth is application-layer on A5)");
	}

	/*
     * Checked after Pairable, because the two interact: with the battery plugin
     * loaded, a non-pairable adapter is exactly what turns bluetoothd's probe
     * into a failed bonding attempt and then a dropped link.
     */
	warn_if_bluetoothd_battery_plugin();

	/* Only register GATT services + connectable advertising if enabled */
	bool gatt_enabled = uci_app_get_bool("ble", "gatt_server", "enabled", false);
	if (gatt_enabled) {
		setup_services();
		dbus_connection_add_filter(gs.conn, message_handler, NULL, NULL);

		/*
         * Before the match rule, not with the other two timers below: this one is
         * armed from the Device1 handler, which goes live on the next line, and
         * both register_application() and start_advertising() run nested D-Bus
         * dispatch loops. A disconnect signal delivered inside one of those would
         * arm a timeout whose cb is still NULL, and uloop calls it unconditionally
         * when it fires.
         *
         * adv_timer and adv_now do not need this: the only thing that arms adv_now
         * is the gatt_v2 status hook, installed after its cb, and adv_timer is
         * armed where its cb is set.
         */
		gs.adv_retry.cb = adv_retry_cb;

		/*
         * Subscribe to org.bluez.Device1 property changes so we learn when a
         * phone connects/disconnects and can keep authentication strictly
         * per-connection (§19.4). Without this match rule the bus would not
         * deliver the signal to us at all.
         */
		dbus_bus_add_match(gs.conn,
						   "type='signal',"
						   "interface='org.freedesktop.DBus.Properties',"
						   "member='PropertiesChanged',"
						   "arg0='org.bluez.Device1'",
						   &err);
		if (dbus_error_is_set(&err)) {
			syslog(LOG_WARNING, "gatt_server: could not watch Device1 "
				   "connect/disconnect (%s) — auth will not reset per "
				   "connection", err.message ? err.message : "unknown");
			dbus_error_free(&err);
		}

		/* Route shared A2/A7 updates out as BlueZ notifications. */
		gatt_v2_set_notify_fn(bluez_v2_notify);

		int ret = register_application();
		if (ret < 0) {
			syslog(LOG_WARNING, "gatt_server: BlueZ registration failed");
		}

		/* Start connectable LE advertising */
		start_advertising();

		/*
         * Keep the advertised status bytes current. The timer catches `link`
         * changing behind our back; the hook makes a device_status transition
         * visible on the next uloop iteration instead of up to one interval late.
         */
		gs.adv_timer.cb = adv_refresh_timer_cb;
		uloop_timeout_set(&gs.adv_timer, ADV_REFRESH_INTERVAL_MS);
		gs.adv_now.cb = adv_refresh_now_cb;
		gatt_v2_set_status_fn(adv_status_changed_cb);
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

	/* Drop the refresh hooks first: all of them would touch an advertisement we
     * are about to unregister. adv_now and adv_retry may be armed but not yet
     * fired, so they have to be cancelled too — otherwise they run after
     * stop_advertising() and re-register an advertisement the daemon has just
     * torn down. adv_retry is the likelier of the two to be armed here, because
     * a stop that follows a disconnect is exactly the ordering that arms it. */
	gatt_v2_set_status_fn(NULL);
	uloop_timeout_cancel(&gs.adv_timer);
	uloop_timeout_cancel(&gs.adv_now);
	uloop_timeout_cancel(&gs.adv_retry);

	stop_advertising();
	unregister_application();

	/* Stop pushing notifications before the connection goes away. */
	gatt_v2_set_notify_fn(NULL);
	gatt_v2_upgrade_deinit();
	adv_status_deinit();
	status_doc_deinit();
	config_doc_deinit();
	config_apply_deinit();
	/* Drop every link's authentication and any half-received configuration. */
	gatt_v2_conn_reset_all();

	if (gs.conn) {
		dbus_bus_remove_match(gs.conn,
							  "type='signal',"
							  "interface='org.freedesktop.DBus.Properties',"
							  "member='PropertiesChanged',"
							  "arg0='org.bluez.Device1'",
							  NULL);
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
