/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file config_doc.h
 * @brief The A1 configuration document (§19.5.4).
 *
 * What the app's Wi-Fi settings screen is built from. Until it existed that
 * screen said "No SSIDs configured" on a device with radios up, because A1
 * answered `not_implemented`.
 *
 * Two shapes, exactly as the reference chooses between them
 * (`wifi_provision.py`, `get_wifi_config`):
 *
 *   1. `/etc/ucentral/ucentral.active` verbatim, when it parses and carries a
 *      non-empty `interfaces` — the uCentral configuration the controller or a
 *      previous provisioning applied.
 *   2. otherwise a `{"radios":[…]}` topology built from
 *      `ubus call network.wireless status` — what the radios are actually doing
 *      right now, which is all a device that has never been provisioned has.
 *
 * The second shape is not a degraded version of the first; they are different
 * documents and the app handles both.
 */
#ifndef CONFIG_DOC_H
#define CONFIG_DOC_H

#ifdef __cplusplus
extern "C" {
#endif

/** Register as the A1 snapshot provider. Call from the GATT app's init. */
void config_doc_init(void);

/** Release the cached document. Call from the GATT app's stop path. */
void config_doc_deinit(void);

/**
 * Build the document.
 *
 * Registered with gatt_v2_read_set_snapshot_fn(), so it runs once per A1 read
 * sequence rather than once per fragment.
 *
 * @return compact JSON owned by this module, valid until the next call. Never
 *         NULL — with neither source available it is `{"radios":[]}`, which is
 *         the honest answer rather than an error the app cannot act on.
 */
const char *config_doc_json(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_DOC_H */
