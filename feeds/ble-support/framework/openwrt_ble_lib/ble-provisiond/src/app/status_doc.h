/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file status_doc.h
 * @brief The A4 device-status document (§19.5).
 *
 * This is what the phone app reads to render its dashboard, and until it exists
 * the app authenticates successfully and then has nothing to show — which looks
 * like an authentication failure from the outside.
 *
 * Six keys, in the order the reference emits them
 * (`wifi_provision.py`, `get_device_status`):
 *
 *   info          model, firmware version, serial, MAC
 *   health        /tmp/ucentral.health verbatim
 *   state         live output of /usr/share/ucentral/state.uc
 *   mode          "cloud" | "standalone" | "unknown"
 *   cloud_active  whether the ucentral client is actually running
 *   capabilities  /etc/ucentral/capabilities.json verbatim
 *
 * Every source is missing-tolerant: an absent or unparsable file becomes `{}`,
 * exactly as the reference does, because a device that has never been online
 * still has to render a dashboard.
 *
 * Nothing here requires ucentral to be running. `cloud_active` reports whether it
 * is, and `mode` reports the operator's persisted intent — the two are
 * deliberately separate (see §19.7).
 */
#ifndef STATUS_DOC_H
#define STATUS_DOC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read the one-shot sources and register as the A4 snapshot provider.
 *
 * Call from the GATT app's init, before the event loop starts: the serial number
 * comes from `fw_printenv`, which is a fork+exec, and doing it here means it
 * never happens on a read.
 */
void status_doc_init(void);

/** Release cached state. Call from the GATT app's stop path. */
void status_doc_deinit(void);

/**
 * Build the document.
 *
 * Registered with gatt_v2_read_set_snapshot_fn(), so it runs once per A4 read
 * sequence rather than once per fragment.
 *
 * @return compact JSON owned by this module, valid until the next call. Never
 *         NULL — a total failure still yields `{}`.
 */
const char *status_doc_json(void);

#ifdef __cplusplus
}
#endif

#endif /* STATUS_DOC_H */
