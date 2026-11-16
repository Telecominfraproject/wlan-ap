/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file adv_status.h
 * @brief Advertisement manufacturer data for GATT provisioning v2 (§19.6).
 *
 * The phone app scans by manufacturer data, not by service UUID — the primary
 * advertisement deliberately carries no ServiceUUIDs so there is room for the
 * Complete Local Name AD. Four bytes under company ID 0xFFFF:
 *
 *     [0] link          0 = down, 1 = carrier up but no IP, 2 = up with an IP
 *     [1] device_status the shared v2 status byte (0/1/2/3 config, 10..15 upgrade)
 *     [2] model_id low  uint16 little-endian; the app labels the scan entry with it
 *     [3] model_id high
 *
 * Transport-agnostic on purpose: the BlueZ path feeds these bytes into
 * `org.bluez.LEAdvertisement1.ManufacturerData`, and the TI path will push the
 * same four bytes to the firmware over NPI. One builder means the two paths
 * cannot drift.
 */
#ifndef ADV_STATUS_H
#define ADV_STATUS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Manufacturer company identifier the app filters on. */
#define ADV_MFR_COMPANY_ID   0xFFFF

/** Length of the manufacturer payload. */
#define ADV_MFR_DATA_LEN     4

/** Link states reported in byte 0. */
#define ADV_LINK_DOWN        0
#define ADV_LINK_CONNECTING  1   /* carrier up, no IP yet (DHCP in flight) */
#define ADV_LINK_UP          2

/**
 * Sentinel meaning "no tag was chosen for this board" — **not a tag that gets
 * advertised**.
 *
 * These two bytes are not the AP model number. The real model string reaches the
 * app through A4 after connecting; this is a per-SKU tag the app matches to
 * recognise its own devices in a scan list it cannot filter by service UUID
 * (§19.6).
 *
 * The reference hardcodes 3 for every model (`wifi_provision.py`, `MODEL_ID`),
 * and this value used to be shipped and advertised as 3 for the same reason. It
 * is now read as a sentinel instead: `adv_status_init()` sees 3 and replaces it
 * with the board's own tag from the table in `adv_status.c` (ECCLOUD-11895), so
 * `edgecore,eap104` advertises 12 rather than 3. The phone app carries the same
 * table. Any value other than 3 in `ble.gatt_server.model_id` is an integrator
 * override and is advertised verbatim.
 *
 * Same shape as the `device_name` sentinel, and for the same reason: a derived
 * value comes back by itself after `/etc/config/ble` is restored to the package
 * copy during an apply (§19.9.9), whereas a per-board value written into UCI does
 * not survive it.
 *
 * So 3 only ever reaches the air when the board is absent from the table, which
 * is logged as a warning — it is the fallback, not the default.
 */
#define ADV_MODEL_ID_DEFAULT 3

/**
 * Read `ble.gatt_server.model_id` and reset the uplink cache.
 *
 * Call once at start-up. Safe to call before ubus is reachable: the uplink
 * lookup degrades to "down" until a context exists.
 *
 * @return 0 always.
 */
int adv_status_init(void);

/** Drop cached state. */
void adv_status_deinit(void);

/**
 * Build the current four manufacturer bytes.
 *
 * The uplink state is cached for ADV_UPLINK_CACHE_SEC so that a caller polling
 * on a timer does not issue a ubus call every tick; device_status is read live
 * from the shared v2 core.
 *
 * @param out receives ADV_MFR_DATA_LEN bytes.
 * @return ADV_MFR_DATA_LEN, or 0 when @out is NULL.
 */
int adv_status_mfr_data(uint8_t out[ADV_MFR_DATA_LEN]);

/**
 * The board identity this tag was resolved against, e.g. `edgecore,eap104`.
 *
 * Never NULL; empty when `/var/sysinfo/board_name` could not be read. Exposed so
 * the status file can report the same pair the log reports — the two must agree,
 * and the only way to guarantee that is to read them from one place.
 *
 * Note the deliberate name: this is the **board**, not the "model" of the
 * start-up banner, which is the radio chip (`NRF52840`, `CC2652R1`).
 */
const char *adv_status_board(void);

/** The resolved tag actually being advertised, derived or overridden. */
uint16_t adv_status_model_id(void);

#ifdef __cplusplus
}
#endif

#endif /* ADV_STATUS_H */
