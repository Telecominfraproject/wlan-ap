/**
 * @file gatt_server_app.h
 * @brief GATT Server application plugin — exposes the uCentral provisioning
 *        service (v2) to BLE clients (phone apps).
 *
 * Uses the BlueZ GattManager1 D-Bus interface to register a GATT application
 * with a single primary service and seven characteristics.
 *
 * The legacy 16-bit FE00 (Device Management) / FE10 (Config Provisioning)
 * services have been retired: every capability — authentication, device reboot,
 * factory reset, firmware upgrade, WiFi configuration and status — is now
 * exposed through the v2 service below, on both the BlueZ and TI paths.
 * See DESIGN_OVERVIEW §19.
 */
#ifndef GATT_SERVER_APP_H
#define GATT_SERVER_APP_H

#include "app_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════
 *  GATT Provisioning Service v2 (uCentral, phone-app-aligned)
 *  UUIDs are FIXED by the shipped phone app (and its Python reference
 *  implementation). Single primary service, 128-bit base
 *  e063c3aX-0748-40be-935e-01ebdeafa814.
 * ══════════════════════════════════════════════════════════════════ */
#define GATT_V2_SRV_UUID       "e063c3a0-0748-40be-935e-01ebdeafa814"
#define GATT_V2_READ_UUID      "e063c3a1-0748-40be-935e-01ebdeafa814" /* A1 read  (config) */
#define GATT_V2_WRITE_UUID     "e063c3a2-0748-40be-935e-01ebdeafa814" /* A2 write+read+notify (cmd + outcome) */
#define GATT_V2_RESULT_UUID    "e063c3a3-0748-40be-935e-01ebdeafa814" /* A3 read+notify (command outcome; the app's channel) */
#define GATT_V2_STATUS_UUID    "e063c3a4-0748-40be-935e-01ebdeafa814" /* A4 read  (status) */
#define GATT_V2_AUTH_UUID      "e063c3a5-0748-40be-935e-01ebdeafa814" /* A5 write (auth) */
#define GATT_V2_MAC_UUID       "e063c3a6-0748-40be-935e-01ebdeafa814" /* A6 read  (mac) */
#define GATT_V2_UPGRADE_UUID   "e063c3a7-0748-40be-935e-01ebdeafa814" /* A7 read+notify (upgrade) */

/*
 * D-Bus object paths. The characteristic order matches the reference
 * implementation exactly (char0 = A1 … char6 = A7) so the exported layout is
 * indistinguishable from the Python daemon the phone app was built against.
 */
#define GATT_APP_PATH            "/org/openwrt/ble/gatt"
#define GATT_V2_SRV_PATH         GATT_APP_PATH "/service0"

/* Plugin instance */
extern app_plugin_t gatt_server_app_plugin;

/*
 * Integrate the GATT server's BlueZ D-Bus connection with the daemon's uloop.
 *
 * The GATT server uses a private D-Bus connection (gs.conn). Incoming
 * characteristic ReadValue/WriteValue calls from BlueZ arrive on this
 * connection and MUST be dispatched from the main event loop; otherwise they
 * sit unhandled after RegisterApplication completes (services are visible but
 * reads/writes never reach our handlers).
 *
 * gatt_server_get_dbus_fd(): returns the connection's file descriptor to add to
 *   uloop (ULOOP_READ), or -1 if the BlueZ path is not active (e.g. TI ti_npi).
 * gatt_server_dispatch(): pump the connection once; call from the uloop fd
 *   callback whenever the fd is readable.
 */
int  gatt_server_get_dbus_fd(void);
void gatt_server_dispatch(void);

/**
 * The name this device actually advertises, after the board-derivation step.
 *
 * `ble.gatt_server.device_name` is not the answer on its own: the value this
 * package ships, "OpenWrt-BLE", is a sentinel meaning "no name was chosen for
 * this board", and the daemon then builds one from /var/sysinfo/board_name
 * (edgecore,eap104 -> EDGECORE_EAP104). Anything that reports, displays or
 * transmits the name has to go through here, or it will say "OpenWrt-BLE" while
 * the radio says something else.
 *
 * Four call sites consume this, and two of them put it on the air: BlueZ
 * advertising and the adapter Alias in gatt_server_app.c, the TI SET_NAME in
 * gatt_app.c, and the A4 document in app_status.c. Reading the UCI key instead
 * is the defect this accessor exists to prevent — it has happened twice.
 *
 * @return the resolved name, or "" before gatt_server_init() has run.
 */
const char *gatt_server_device_name(void);

#ifdef __cplusplus
}
#endif

#endif /* GATT_SERVER_APP_H */
