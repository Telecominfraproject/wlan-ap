/**
 * @file app_status.h
 * @brief Application-layer runtime status file for ble-provisiond.
 *
 * libble writes the low-level status (transport/chip/BD address/radio) to
 * /var/run/ble-provision.status. This module writes the APPLICATION-layer
 * status (GATT server, iBeacon, scan) to a separate file so external tools can
 * see the provisioning identity and feature state. It is transport-agnostic:
 * the same fields are recorded for the BlueZ and TI (ti_npi) paths.
 *
 * Config-derived fields (enabled flags, device name, iBeacon UUID/major/minor,
 * scan filter/duration) are read from UCI at write time. Runtime fields
 * (scan running + results file, iBeacon active) are set by the app plugins via
 * the setters below, then app_status_write() snapshots everything.
 * Connection-level runtime (connected/MTU) is intentionally not tracked here —
 * it is only meaningful while connected and not uniformly observable across
 * transports.
 *
 * Copyright © Accton Technology Corporation. All rights reserved.
 */
#ifndef APP_STATUS_H
#define APP_STATUS_H

#include <stdbool.h>
#include <stdint.h>

/* Path of the application-layer status file. */
#define APP_STATUS_FILE "/var/run/ble-provision-apps.status"

/* ── Runtime state setters (called by the app plugins on change) ── */

/* Scan currently running + the file results are being written to (NULL/"" if
 * none). */
void app_status_set_scan_running(bool running, const char *results_file);

/* iBeacon currently broadcasting. */
void app_status_set_ibeacon_active(bool active);

/*
 * Snapshot config (from UCI) + runtime state and (re)write APP_STATUS_FILE.
 * Safe to call at startup and on any state change. Never blocks on BLE.
 */
void app_status_write(void);

/* Remove the status file (on daemon shutdown). */
void app_status_remove(void);

#endif /* APP_STATUS_H */
