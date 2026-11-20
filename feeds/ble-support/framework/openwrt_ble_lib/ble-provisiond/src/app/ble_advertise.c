/* SPDX-License-Identifier: BSD-3-Clause */
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
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <dbus/dbus.h>

#include "ble_advertise.h"

#define BLUEZ_SERVICE       "org.bluez"
#define BLUEZ_LE_ADV_MGR    "org.bluez.LEAdvertisingManager1"
#define BLUEZ_LE_ADV_IFACE  "org.bluez.LEAdvertisement1"
#define ADV_BASE_PATH       "/org/openwrt/ble/adv"
#define DBUS_TIMEOUT_MS     10000

/*
 * Backstop for the nested dispatch loop in register_advertisement_once(), on top
 * of libdbus's own DBUS_TIMEOUT_MS. Slightly longer so libdbus normally times the
 * call out first and this only catches the cases where it cannot.
 */
#define ADV_REGISTER_WAIT_MS  (DBUS_TIMEOUT_MS + 2000)

static int64_t adv_monotonic_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
}

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

		/*
         * MinInterval / MaxInterval — uint32 milliseconds.
         *
         * Offered only when set, so an instance that leaves them at 0 keeps the
         * previous behaviour of letting the kernel choose. That matters because
         * the kernel's choice is not neutral: 1280 ms for connectable and
         * 62.5 ms for non-connectable, a 20x difference that starved the
         * provisioning advertisement to 4% of the air on eap102.
         *
         * Both are published with the same value. The pair is a range the
         * controller may pick within, and nothing here benefits from letting it
         * drift, while an inverted pair makes RegisterAdvertisement fail.
         */
		if (inst->min_interval_ms > 0 && inst->max_interval_ms > 0) {
			static const char *ivl_keys[2] = { "MinInterval", "MaxInterval" };
			uint32_t ivl_vals[2] = { inst->min_interval_ms, inst->max_interval_ms };
			for (int k = 0; k < 2; k++) {
				DBusMessageIter entry, variant;
				dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
				dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &ivl_keys[k]);
				dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
				dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &ivl_vals[k]);
				dbus_message_iter_close_container(&entry, &variant);
				dbus_message_iter_close_container(&dict, &entry);
			}
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

static void unregister_advertisement_path(const char *path);  /* fwd decl */

static int register_advertisement_once(ble_adv_instance_t *inst)
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

	/*
     * Nested dispatch loop. It has to be here: BlueZ calls GetAll on our
     * LEAdvertisement1 object before it answers RegisterAdvertisement, so the
     * reply can never arrive unless this keeps servicing the connection.
     *
     * Two guards, both previously missing, and both able to hang the entire
     * daemon — everything here runs on the single uloop thread:
     *
     *   - dbus_connection_read_write_dispatch() returns FALSE once the connection
     *     is disconnected, and from then on returns *immediately* instead of
     *     waiting its 100 ms. With the result ignored, a bluetoothd that went away
     *     mid-call turned this into a tight spin at 100% CPU that nothing could
     *     interrupt, because the pending call would never complete either.
     *   - a wall-clock deadline, so a reply that never arrives for any other
     *     reason still ends the loop rather than parking the daemon in it.
     */
	int64_t deadline = adv_monotonic_ms() + ADV_REGISTER_WAIT_MS;
	int wait_err = 0;

	while (!dbus_pending_call_get_completed(pending)) {
		if (!dbus_connection_read_write_dispatch(adv_ctx.conn, 100)) {
			syslog(LOG_ERR, "ble_adv: D-Bus connection lost while waiting for "
				   "RegisterAdvertisement[%d]", inst->id);
			wait_err = -ECONNRESET;
			break;
		}
		if (adv_monotonic_ms() > deadline) {
			syslog(LOG_ERR, "ble_adv: RegisterAdvertisement[%d] gave no reply "
				   "within %d ms — abandoning this attempt",
				   inst->id, ADV_REGISTER_WAIT_MS);
			wait_err = -ETIMEDOUT;
			break;
		}
	}

	if (wait_err) {
		dbus_pending_call_cancel(pending);
		dbus_pending_call_unref(pending);
		return wait_err;
	}

	DBusMessage *reply = dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);

	if (!reply) {
		syslog(LOG_ERR, "ble_adv: RegisterAdvertisement[%d]: no reply", inst->id);
		return -EIO;
	}

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		const char *err_name = dbus_message_get_error_name(reply);

		/*
         * Name the two cases apart in the log, because they call for different
         * reactions and the log used to make them indistinguishable:
         *
         *   AlreadyExists — BlueZ matches advertising clients by D-Bus sender, so
         *                   this really does mean a registration is in the way and
         *                   clearing it can help.
         *   anything else — notably org.bluez.Error.Failed, which BlueZ returns
         *                   when it could not accept the advertisement at all
         *                   (bad data, or the controller having no advertising
         *                   instance free). Clearing a path will not fix that, and
         *                   the caller must not conclude the old registration was
         *                   the problem.
         *
         * The retry below still runs for both, because the stale-registration
         * recovery predates this and has not been re-verified; what changed is
         * that a failure can no longer leave the device dark (see
         * ble_adv_update_mfr_data).
         */
		bool already = err_name &&
					   strcmp(err_name, "org.bluez.Error.AlreadyExists") == 0;
		syslog(LOG_ERR, "ble_adv: RegisterAdvertisement[%d] error: %s (%s)",
			   inst->id, err_name ? err_name : "unknown",
			   already ? "a registration is in the way — cleanup may help"
					   : "BlueZ refused the advertisement — cleanup will not help");
		dbus_message_unref(reply);
		return already ? -EEXIST : -EIO;
	}

	dbus_message_unref(reply);
	/*
     * This line is a contract, not just a message. BCN-009, BCN-010 and BCN-011
     * all match on `registered (type=broadcast)` with the closing parenthesis
     * included, and so do the investigation probes. Adding a field inside the
     * parentheses would make every one of them silently stop matching — a
     * presence check would fail for the wrong reason and an absence check would
     * pass for the wrong reason, which is the trap the deploy guard at the top of
     * 12_beacon.sh exists for. So it stays byte-identical and the interval goes
     * on its own line below.
     */
	syslog(LOG_INFO, "ble_adv: instance %d registered (type=%s)",
		   inst->id, inst->type == BLE_ADV_TYPE_PERIPHERAL ? "peripheral" : "broadcast");

	/*
     * Logged separately because the interval is otherwise invisible from the
     * host: what we requested appears only in MGMT, and what the controller was
     * programmed with appears only in btmon. This records our half, so a wrong
     * value can be told apart from a value BlueZ discarded. It does not prove
     * the interval reached the radio — BlueZ has been reported to drop
     * MinInterval/MaxInterval in some versions, and only a second device
     * scanning can settle that.
     */
	if (inst->min_interval_ms > 0)
		syslog(LOG_INFO, "ble_adv: instance %d interval requested %u ms",
			   inst->id, inst->min_interval_ms);
	return 0;
}

/*
 * Register the advertisement, with one cleanup-and-retry pass for the failures
 * where a registration could still be sitting on our object path — typically one
 * left behind by a previous daemon instance that bluetoothd has not noticed is
 * gone. Worst case for a caller is two attempts plus the 200 ms between them;
 * callers that cannot afford that use register_advertisement_once() directly.
 */
static int register_advertisement(ble_adv_instance_t *inst)
{
	int ret = register_advertisement_once(inst);

	/*
     * Retry after clearing the path for the two errors where a registration could
     * plausibly be in the way, and for those only.
     *
     * Deliberately NOT retried:
     *   -ECONNRESET — the D-Bus connection is gone; a second attempt cannot reach
     *                 BlueZ and would just be another trip through the dispatch
     *                 loop with nothing on the other end.
     *   -ETIMEDOUT  — the first attempt already cost ADV_REGISTER_WAIT_MS. Trying
     *                 again would double a stall that is the whole reason the
     *                 deadline exists.
     */
	if (ret == -EEXIST || ret == -EIO) {
		syslog(LOG_WARNING, "ble_adv: RegisterAdvertisement failed (%d) — "
			   "cleaning up %s and retrying once", ret, inst->path);
		unregister_advertisement_path(inst->path);
		/* Give BlueZ a moment to drop the old registration. */
		usleep(200000);
		ret = register_advertisement_once(inst);
	}
	return ret;
}

/*
 * Unconditionally tell BlueZ to unregister an advertisement at @path, even if
 * THIS process never registered it. Used to clear a stale registration left by
 * a previous daemon instance (a crash/restart leaves BlueZ still holding the
 * advertisement object, so a fresh RegisterAdvertisement on the same path
 * returns org.bluez.Error.Failed / "Already exists").
 */
static void unregister_advertisement_path(const char *path)
{
	if (!adv_ctx.conn || !path) return;
	DBusMessage *msg = dbus_message_new_method_call(
		BLUEZ_SERVICE, adv_ctx.adapter_path,
		BLUEZ_LE_ADV_MGR, "UnregisterAdvertisement");
	if (!msg) return;
	DBusMessageIter iter;
	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);
	/* Best-effort; ignore the reply (it may legitimately fail if nothing was
     * registered, which is fine). */
	dbus_connection_send(adv_ctx.conn, msg, NULL);
	dbus_connection_flush(adv_ctx.conn);
	dbus_message_unref(msg);
	syslog(LOG_INFO, "ble_adv: sent stale-cleanup UnregisterAdvertisement for %s", path);
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
	inst->registered = false;
}

/* ── Allocate an instance slot ── */

/*
 * Clamp a requested interval into the range BlueZ accepts.
 *
 * Out-of-range values do not degrade gracefully: BlueZ fails the whole
 * RegisterAdvertisement, so the advertisement simply never happens. The beacon
 * interval comes from UCI (ble.ibeacon.interval), so a typo there would
 * otherwise silence the beacon with nothing to connect cause to effect — the
 * failure mode §19.9.7 and §19.9.13 each cost several wrong hypotheses. Clamp
 * and say so instead.
 *
 * 0 is passed through untouched and means "offer no interval", which leaves the
 * kernel default in place.
 */
static uint32_t adv_clamp_interval(uint32_t ms, int instance_hint)
{
	if (ms == 0)
		return 0;
	if (ms < BLE_ADV_INTERVAL_MIN_MS) {
		syslog(LOG_WARNING, "ble_adv: interval %u ms below the %u ms BlueZ minimum "
			   "(instance %d) — using %u ms",
			   ms, BLE_ADV_INTERVAL_MIN_MS, instance_hint, BLE_ADV_INTERVAL_MIN_MS);
		return BLE_ADV_INTERVAL_MIN_MS;
	}
	if (ms > BLE_ADV_INTERVAL_MAX_MS) {
		syslog(LOG_WARNING, "ble_adv: interval %u ms above the %u ms BlueZ maximum "
			   "(instance %d) — using %u ms",
			   ms, BLE_ADV_INTERVAL_MAX_MS, instance_hint, BLE_ADV_INTERVAL_MAX_MS);
		return BLE_ADV_INTERVAL_MAX_MS;
	}
	return ms;
}

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

int ble_adv_start_connectable(const char *local_name,
							  uint16_t mfr_company_id,
							  const uint8_t *mfr_data, uint8_t mfr_data_len)
{
	if (!adv_ctx.initialized) return -EINVAL;
	if (mfr_data_len > BLE_ADV_MFR_DATA_MAX) return -EINVAL;

	ble_adv_instance_t *inst = alloc_instance();
	if (!inst) {
		syslog(LOG_ERR, "ble_adv: no free instance slots");
		return -ENOMEM;
	}

	inst->type = BLE_ADV_TYPE_PERIPHERAL;
	/*
     * A fixed interval rather than the kernel default of 1280 ms. This is the
     * advertisement a phone has to find before anything else can happen, and at
     * 1280 ms it transmitted 1.6 times per 2-second rotation slot on eap102 —
     * six receptions in 62 seconds from a scanner a metre away. See
     * BLE_ADV_PROV_INTERVAL_MS.
     */
	inst->min_interval_ms = adv_clamp_interval(BLE_ADV_PROV_INTERVAL_MS, inst->id);
	inst->max_interval_ms = inst->min_interval_ms;
	if (local_name)
		strncpy(inst->local_name, local_name, BLE_ADV_NAME_MAX - 1);
	inst->include_tx_power = true;
	if (mfr_data_len && mfr_data) {
		inst->mfr_company_id = mfr_company_id;
		memcpy(inst->mfr_data, mfr_data, mfr_data_len);
		inst->mfr_data_len = mfr_data_len;
	}
	inst->active = true;

	/*
     * AD budget check. The primary advertisement holds 31 bytes:
     *   Flags                       3
     *   Manufacturer Specific Data  2 + 2 + mfr_data_len
     *   Complete Local Name         2 + strlen(name)
     * Exceeding it makes BlueZ refuse the registration with a bare
     * "Invalid Parameters", which is a miserable thing to debug — so say it
     * plainly here instead.
     */
	size_t name_len = strlen(inst->local_name);
	size_t ad_len = 3 + (inst->mfr_data_len ? (4u + inst->mfr_data_len) : 0u)
					  + (name_len ? (2u + name_len) : 0u);
	if (ad_len > 31)
		syslog(LOG_WARNING, "ble_adv: advertisement data is %zu bytes (>31); "
			   "BlueZ will reject it — shorten the device name (%zu chars)",
			   ad_len, name_len);
	else
		/*
         * LOG_INFO, not LOG_DEBUG. This is the only place the actual AD size is
         * stated, and the budget it reports is the one that makes BlueZ refuse
         * to advertise at all when a name overruns it — so it is the evidence
         * NAME-006 asserts. At debug level that test could never run at the
         * default log level, and it is BlueZ-only, which means it had no board
         * to run on until eap102. Fires once per advertising registration.
         */
		syslog(LOG_INFO, "ble_adv: advertisement data %zu/31 bytes "
			   "(name=%zu mfr=%u)", ad_len, name_len, inst->mfr_data_len);

	int ret = register_advertisement(inst);
	if (ret < 0) {
		inst->active = false;
		return ret;
	}
	inst->registered = true;
	return inst->id;
}

int ble_adv_update_mfr_data(int instance_id, const uint8_t *data, uint8_t len)
{
	if (instance_id < 1 || instance_id > BLE_ADV_MAX_INSTANCES) return -EINVAL;
	if (len > BLE_ADV_MFR_DATA_MAX) return -EINVAL;

	ble_adv_instance_t *inst = &adv_ctx.instances[instance_id - 1];
	if (!inst->active) return -ENOENT;

	bool same = (inst->mfr_data_len == len &&
				 (len == 0 || memcmp(inst->mfr_data, data, len) == 0));

	/*
     * Nothing to do only when the bytes match AND BlueZ is actually holding the
     * advertisement. If a previous attempt failed we still have to register, even
     * though the stored bytes already look current.
     */
	if (same && inst->registered)
		return 0;

	/*
     * Remember what BlueZ is currently advertising, so a failed update can put it
     * back instead of leaving the device dark. See the restore block below.
     */
	uint8_t prev[BLE_ADV_MFR_DATA_MAX];
	uint8_t prev_len = inst->mfr_data_len;
	bool prev_registered = inst->registered;
	memcpy(prev, inst->mfr_data, sizeof(prev));

	if (len && data) {
		memcpy(inst->mfr_data, data, len);
		inst->mfr_data_len = (uint8_t)len;
	} else {
		inst->mfr_data_len = 0;
	}

	/*
     * Re-register so BlueZ re-reads the properties — it only reads them while
     * registering. unregister_advertisement() clears `active`, which
     * alloc_instance() would then hand to someone else, so restore it and keep
     * the same slot and object path.
     */
	if (inst->registered) {
		unregister_advertisement(inst);
		inst->active = true;
	}
	inst->registered = false;

	int ret = register_advertisement(inst);
	if (ret < 0) {
		syslog(LOG_WARNING, "ble_adv: re-register with the new manufacturer data "
			   "failed (%d)", ret);

		/*
         * Put the previous advertisement back.
         *
         * This is the part that was missing, and it is the difference between a
         * cosmetic problem and a serious one. BlueZ only reads the LEAdvertisement1
         * properties while registering, so publishing new bytes means unregistering
         * first — and when the register that follows fails, the device stops
         * advertising altogether.
         *
         * The old code left it there, on the reasoning that "the next timer tick
         * retries". That does not hold. adv_refresh() in gatt_server_app.c gated
         * itself on its own cached copy of the bytes, which still held the last
         * *successfully registered* value; when the change that triggered the
         * failed update reverted — a single uplink flap on eap115 — the freshly
         * computed bytes matched that cache again and the refresh returned early.
         * Measured result: 44 s with nothing advertised and the daemon convinced
         * there was nothing to do. (That gate is gone too; see adv_refresh.)
         *
         * Re-registering the old bytes is known to have worked at least once —
         * they are the ones BlueZ accepted — so this is the attempt most likely to
         * succeed. A stale status byte in the advertisement is a cosmetic defect;
         * an access point nobody can discover is not.
         */
		if (prev_registered) {
			memcpy(inst->mfr_data, prev, sizeof(prev));
			inst->mfr_data_len = prev_len;
			inst->active = true;

			/*
             * One attempt, not register_advertisement()'s cleanup-and-retry pair.
             * These bytes were accepted before, so clearing the path is unlikely to
             * be what makes the difference, and this runs on the event loop: every
             * extra attempt adds another ADV_REGISTER_WAIT_MS to the worst case.
             * In practice BlueZ answers in milliseconds (measured: the whole
             * fail-cleanup-retry sequence completed inside one second), so the
             * deadline is a backstop for a pathological case rather than a cost we
             * expect to pay — but a 12 s stall should not become a 24 s one just to
             * add an attempt with little chance of changing the outcome.
             */
			if (register_advertisement_once(inst) == 0) {
				inst->registered = true;
				syslog(LOG_WARNING, "ble_adv: restored the previous advertisement "
					   "(instance %d) — its status bytes are stale, but the device "
					   "is still discoverable", inst->id);
			} else {
				syslog(LOG_ERR, "ble_adv: could not restore the previous "
					   "advertisement (instance %d) — THE DEVICE IS NOT "
					   "ADVERTISING; the refresh timer will keep retrying",
					   inst->id);
			}
		}

		/*
         * Either way the stored bytes are now the old ones and `registered`
         * reflects reality, so the next refresh sees a genuine difference and tries
         * again. The slot stays alive so the caller's instance id keeps working.
         */
		return ret;
	}

	inst->registered = true;
	syslog(LOG_DEBUG, "ble_adv: manufacturer data updated (instance %d)",
		   inst->id);
	return 1;
}

int ble_adv_start_ibeacon(const uint8_t uuid_bytes[16],
						  uint16_t major, uint16_t minor, int8_t tx_power,
						  uint32_t interval_ms)
{
	if (!adv_ctx.initialized) return -EINVAL;

	ble_adv_instance_t *inst = alloc_instance();
	if (!inst) {
		syslog(LOG_ERR, "ble_adv: no free instance slots");
		return -ENOMEM;
	}

	/*
     * From ble.ibeacon.interval, which the daemon has always read and then
     * discarded on this path: ble_beacon_start() does nothing with it when no
     * chip profile is active, and this function had no interval parameter, so
     * the option silently had no effect. The kernel was instead applying its own
     * 62.5 ms for a non-connectable advertisement — twenty times faster than the
     * provisioning advertisement next to it.
     */
	inst->min_interval_ms = adv_clamp_interval(interval_ms, inst->id);
	inst->max_interval_ms = inst->min_interval_ms;

	/*
     * Broadcast, i.e. non-connectable. This used to be PERIPHERAL, with the
     * comment "many BLE chips don't support broadcast" and no measurement behind
     * it — and that choice is what made iBeacon disappear for the whole duration
     * of a provisioning session.
     *
     * A PERIPHERAL instance makes BlueZ set MGMT_ADV_FLAG_CONNECTABLE, so the
     * kernel takes the connectable branch of is_advertising_allowed(). On the
     * nRF52840 (eap102) that branch is refused while a peripheral connection
     * exists, and the refusal is genuine: asked directly over a raw HCI socket,
     * the controller answers LE Set Advertise Enable with
     *
     *     Status: Connection Limit Exceeded (0x09)
     *
     * for ADV_IND, because enabling connectable advertising reserves a
     * connection slot and this hci_usb build has exactly one. The same two
     * commands with Advertising_Type 0x03 (ADV_NONCONN_IND) return Success with
     * the connection still up, which is what a beacon needs and all it needs.
     *
     * Nothing in the advertised payload changes: the GetAll handler above only
     * adds LocalName for PERIPHERAL instances, and this function never sets
     * local_name, so the 23 iBeacon bytes and the empty Includes array are
     * byte-for-byte what they were.
     *
     */
	inst->type = BLE_ADV_TYPE_BROADCAST;
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
	inst->registered = true;
	return inst->id;
}

void ble_adv_stop(int instance_id)
{
	if (instance_id < 1 || instance_id > BLE_ADV_MAX_INSTANCES) return;
	ble_adv_instance_t *inst = &adv_ctx.instances[instance_id - 1];
	if (inst->active)
		unregister_advertisement(inst);
}

/*
 * Stop the iBeacon advertisement, whichever slot it landed in.
 *
 * Both callers used to pass a literal 2, on the assumption that the GATT
 * advertisement always takes slot 1 first. That holds only while
 * `gatt_server.enabled` is on. With GATT off, alloc_instance() gives the beacon
 * slot 1, `ble_adv_stop(2)` then inspects an inactive slot and returns having
 * done nothing, and the beacon's slot stays `active` — so the next start
 * allocates a *second* slot for the same beacon.
 *
 * Measured on eap105, where that second start asked the controller for a second
 * advertising set and was refused. The refusal was a separate matter, but the
 * duplicate request was this bug: three start/stop cycles produced instance 1,
 * then 2, then 2 again. On a board with slots to spare the same leak is silent
 * until it reaches `no free instance slots`.
 *
 * The slot is found rather than remembered. A stored id would be one more piece
 * of mutable state to keep in step with the allocator, and this package has
 * twice deleted exactly that kind of cache after it went stale — adv_refresh()'s
 * byte cache (§19.6.1) and `model_id_derived` in adv_status.c. The search is
 * unambiguous because BLE_ADV_TYPE_BROADCAST is assigned in exactly one place,
 * ble_adv_start_ibeacon() above (no line number on purpose: the one that was
 * here drifted within a single editing session); a second kind of
 * broadcast advertisement would
 * need a discriminator of its own rather than a type match.
 */
void ble_adv_stop_ibeacon(void)
{
	for (int i = 0; i < BLE_ADV_MAX_INSTANCES; i++) {
		ble_adv_instance_t *inst = &adv_ctx.instances[i];
		if (inst->active && inst->type == BLE_ADV_TYPE_BROADCAST) {
			syslog(LOG_INFO, "ble_adv: stopping iBeacon advertisement (instance %d)",
				   inst->id);
			unregister_advertisement(inst);
			return;
		}
	}
}

void ble_adv_stop_all(void)
{
	for (int i = 0; i < BLE_ADV_MAX_INSTANCES; i++) {
		if (adv_ctx.instances[i].active)
			unregister_advertisement(&adv_ctx.instances[i]);
	}
}
