/**
 * @file gatt_server_app.h
 * @brief GATT Server application plugin — exposes device management and
 *        WiFi configuration services to BLE clients (phone apps).
 *
 * Uses BlueZ GattManager1 D-Bus interface to register a GATT application
 * with services and characteristics accessible from connected peripherals.
 */
#ifndef GATT_SERVER_APP_H
#define GATT_SERVER_APP_H

#include "app_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Service UUIDs ── */
#define GATT_SRV_DEVMGMT_UUID    "0000fe00-0000-1000-8000-00805f9b34fb"
#define GATT_SRV_CFGPROV_UUID    "0000fe10-0000-1000-8000-00805f9b34fb"

/* ── Characteristic UUIDs — Device Management ── */
#define GATT_CHR_COMMAND_UUID    "0000fe01-0000-1000-8000-00805f9b34fb"
#define GATT_CHR_OTA_URL_UUID    "0000fe02-0000-1000-8000-00805f9b34fb"
#define GATT_CHR_DEV_STATUS_UUID "0000fe03-0000-1000-8000-00805f9b34fb"

/* ── Characteristic UUIDs — Configuration Provisioning ── */
#define GATT_CHR_CFG_WRITE_UUID   "0000fe11-0000-1000-8000-00805f9b34fb"
#define GATT_CHR_CFG_READ_UUID    "0000fe12-0000-1000-8000-00805f9b34fb"
#define GATT_CHR_CFG_STATUS_UUID  "0000fe13-0000-1000-8000-00805f9b34fb"
#define GATT_CHR_WIFI_SCAN_UUID   "0000fe14-0000-1000-8000-00805f9b34fb"

/* ── Command codes for GATT_CHR_COMMAND ── */
#define GATT_CMD_REBOOT          1
#define GATT_CMD_FACTORY_RESET   2
#define GATT_CMD_OTA_UPGRADE     3

/* ── D-Bus object paths ── */
#define GATT_APP_PATH            "/org/openwrt/ble/gatt"
#define GATT_SRV_DEVMGMT_PATH   GATT_APP_PATH "/service0"
#define GATT_SRV_CFGPROV_PATH   GATT_APP_PATH "/service1"

/* Plugin instance */
extern app_plugin_t gatt_server_app_plugin;

#ifdef __cplusplus
}
#endif

#endif /* GATT_SERVER_APP_H */
