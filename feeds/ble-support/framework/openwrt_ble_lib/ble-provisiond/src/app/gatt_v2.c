/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file gatt_v2.c
 * @brief Shared, transport-agnostic state for GATT provisioning v2 (§19).
 *
 * See gatt_v2.h. Values and status codes mirror the shipped Python reference
 * (`wifi_provision.py`) so the phone app sees identical behaviour regardless of
 * whether the peripheral is served by BlueZ or by the TI multi_role firmware.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "gatt_v2.h"

#define V2_RESULT_DEFAULT   "{\"status\":\"idle\"}"
#define V2_AUTH_IDLE        "{\"status\":\"idle\"}"
#define V2_AUTH_SUCCESS     "{\"status\":\"success\"}"
#define V2_AUTH_FAILED \
	"{\"status\":\"failed\",\"reason\":\"invalid_credential\"}"
#define V2_UPGRADE_DEFAULT  "{\"state\":\"idle\",\"message\":\"\"}"
#define V2_NOT_IMPLEMENTED  "{\"status\":\"error\",\"reason\":\"not_implemented\"}"
#define V2_NOT_AUTHENTICATED \
	"{\"status\":\"failed\",\"reason\":\"not_authenticated\"}"

static struct {
	uint8_t           device_status;
	char              result[GATT_V2_VALUE_MAX];
	char              upgrade[GATT_V2_VALUE_MAX];
	char              auth[GATT_V2_VALUE_MAX];
	gatt_v2_notify_fn notify;
	gatt_v2_status_fn status_cb;
} v2;

void gatt_v2_init(void)
{
	/* Hooks are owned by the transport and outlive a re-init of the values. */
	gatt_v2_notify_fn keep = v2.notify;
	gatt_v2_status_fn keep_status = v2.status_cb;
	memset(&v2, 0, sizeof(v2));
	v2.notify = keep;
	v2.status_cb = keep_status;
	v2.device_status = GATT_V2_DS_IDLE;
	snprintf(v2.result,  sizeof(v2.result),  "%s", V2_RESULT_DEFAULT);
	snprintf(v2.upgrade, sizeof(v2.upgrade), "%s", V2_UPGRADE_DEFAULT);
	snprintf(v2.auth,    sizeof(v2.auth),    "%s", V2_AUTH_IDLE);
}

void gatt_v2_set_notify_fn(gatt_v2_notify_fn fn)
{
	v2.notify = fn;
}

void gatt_v2_set_status_fn(gatt_v2_status_fn fn)
{
	v2.status_cb = fn;
}

/* ── Advertisement device_status ── */

uint8_t gatt_v2_device_status(void)
{
	return v2.device_status;
}

void gatt_v2_set_device_status(uint8_t status)
{
	if (v2.device_status == status)
		return;
	syslog(LOG_DEBUG, "gatt_v2: device_status %u -> %u",
		   v2.device_status, status);
	v2.device_status = status;

	/* The byte is advertised, so let the transport republish it now rather than
     * on its next poll (§19.6). */
	if (v2.status_cb)
		v2.status_cb(status);
}

bool gatt_v2_upgrade_active(void)
{
	return v2.device_status >= GATT_V2_DS_FW_DOWNLOADING &&
		   v2.device_status <= GATT_V2_DS_FW_REBOOTING;
}

bool gatt_v2_upgrade_terminal(void)
{
	return v2.device_status == GATT_V2_DS_FW_SUCCESS ||
		   v2.device_status == GATT_V2_DS_FW_FAILED;
}

/* ── Notify helper ── */

static void v2_push(gatt_v2_chr_t chr, const char *json)
{
	if (!v2.notify || !json)
		return;
	v2.notify(chr, (const uint8_t *)json, (uint16_t)strlen(json));
}

/* ── A2 command outcome ── */

void gatt_v2_result_set(const char *json)
{
	if (!json || !*json)
		return;
	snprintf(v2.result, sizeof(v2.result), "%s", json);
	syslog(LOG_INFO, "gatt_v2: result = %s", v2.result);

	/*
     * A3 first: that is where the shipped phone app subscribes and reads, so it
     * is the channel that has to work. A2 carries the same bytes as an extension
     * for clients that prefer request/response on one attribute — publishing to
     * both costs one extra notification and keeps either style usable.
     */
	v2_push(GATT_V2_CHR_RESULT, v2.result);
	v2_push(GATT_V2_CHR_WRITE, v2.result);
}

const char *gatt_v2_result_json(void)
{
	return v2.result[0] ? v2.result : V2_RESULT_DEFAULT;
}

/* ── A5 Auth outcome ── */

void gatt_v2_auth_set(bool ok)
{
	snprintf(v2.auth, sizeof(v2.auth), "%s",
			 ok ? V2_AUTH_SUCCESS : V2_AUTH_FAILED);
	/*
     * Log the verdict only — never the credential, and never the reason string
     * at a level that could distinguish "wrong password" from "no root hash",
     * which would help an attacker profile the device.
     */
	syslog(LOG_INFO, "gatt_v2: A5 auth %s", ok ? "success" : "failed");
	v2_push(GATT_V2_CHR_AUTH, v2.auth);
}

void gatt_v2_auth_reset(void)
{
	snprintf(v2.auth, sizeof(v2.auth), "%s", V2_AUTH_IDLE);
	v2_push(GATT_V2_CHR_AUTH, v2.auth);
}

const char *gatt_v2_auth_json(void)
{
	return v2.auth[0] ? v2.auth : V2_AUTH_IDLE;
}

/* ── A7 Upgrade state ── */

void gatt_v2_upgrade_set(const char *state, const char *message)
{
	if (!state || !*state)
		return;

	/*
     * Escape nothing: messages are produced internally (fixed strings or
     * bounded error text), never echoed from the phone, so they cannot break
     * the JSON. Keep the field order the reference uses.
     */
	snprintf(v2.upgrade, sizeof(v2.upgrade),
			 "{\"state\":\"%s\",\"message\":\"%s\"}",
			 state, message ? message : "");
	syslog(LOG_INFO, "gatt_v2: A7 state=%s message='%s'",
		   state, message ? message : "");
	v2_push(GATT_V2_CHR_UPGRADE, v2.upgrade);
}

const char *gatt_v2_upgrade_json(void)
{
	return v2.upgrade[0] ? v2.upgrade : V2_UPGRADE_DEFAULT;
}

/* ── Shared replies ── */

const char *gatt_v2_not_implemented_json(void)
{
	return V2_NOT_IMPLEMENTED;
}

const char *gatt_v2_not_authenticated_json(void)
{
	return V2_NOT_AUTHENTICATED;
}

int gatt_v2_mac_hex(char *out, size_t out_sz)
{
	if (!out || out_sz < 13)
		return 0;
	out[0] = '\0';

	/* Provided by libble: "XX:XX:XX:XX:XX:XX", or "" when unknown. */
	extern const char *ble_get_bd_address(void);
	const char *bd = ble_get_bd_address();
	if (!bd || !bd[0])
		return 0;

	size_t n = 0;
	for (const char *p = bd; *p && n < out_sz - 1; p++) {
		char c = *p;
		if (c == ':' || c == '-')
			continue;
		if (c >= 'A' && c <= 'F')
			c = (char)(c - 'A' + 'a');
		out[n++] = c;
	}
	out[n] = '\0';
	return (int)n;
}
