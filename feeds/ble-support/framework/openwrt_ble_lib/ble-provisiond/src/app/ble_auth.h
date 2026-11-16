/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file ble_auth.h
 * @brief Application-layer authentication for GATT provisioning v2.
 *
 * Verifies a plaintext credential written by the phone (A5, Auth
 * characteristic) against the root entry of /etc/shadow, using crypt_r()
 * — the C equivalent of the reference app's crypt.crypt check.
 *
 * BLE itself stays open (no pairing/bonding); this module is the security
 * gate for config read/write. See DESIGN_OVERVIEW §19.4.
 *
 * The verification is fail-closed: empty input, or a locked/disabled/missing
 * root row, always fails. The credential and hash are never logged.
 */
#ifndef BLE_AUTH_H
#define BLE_AUTH_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Verify @provided (NUL-terminated plaintext) against the /etc/shadow root
 * hash. Re-reads /etc/shadow on every call so `passwd root` takes effect
 * without a daemon restart.
 *
 * @return true on match, false otherwise (fail-closed).
 */
bool ble_auth_verify_root_password(const char *provided);

/*
 * Authentication STATE is not kept here.
 *
 * It used to be a single module-local flag, which meant that with two phones
 * connected, one authenticating granted the other the right to reboot, wipe or
 * reconfigure the device. State now lives per link in gatt_v2_conn.h; this module
 * only answers "is this credential correct?".
 */

/**
 * Read the device MAC from eth0 as lowercase hex without separators
 * (e.g. "94ef97d938d0"). Returns the number of bytes written (12 on
 * success, 0 on failure). @out must hold at least 13 bytes.
 */
int ble_auth_get_mac(char *out, size_t out_size);

#endif /* BLE_AUTH_H */
