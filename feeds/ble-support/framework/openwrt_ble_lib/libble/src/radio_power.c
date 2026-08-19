/**
 * @file radio_power.c
 * @brief Radio TX power management with per-chip level tables.
 *
 * Maps requested power levels to the nearest supported value
 * for each chip family and provides public API implementation.
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "ble_core_internal.h"
#include "log.h"

#define EXPORT __attribute__((visibility("default")))

/* ── CC2652R1 TX power levels (dBm) ── */
static const int8_t cc2652r1_levels[] = {
    -21, -18, -15, -12, -9, -6, -3, 0, 1, 2, 3, 4, 5
};
#define CC2652R1_LEVEL_COUNT  (int)(sizeof(cc2652r1_levels)/sizeof(cc2652r1_levels[0]))
#define CC2652R1_MIN  (-21)
#define CC2652R1_MAX  5

/* ── nRF52840 / nRF54L15 TX power levels (dBm) ── */
static const int8_t nrf52_levels[] = {
    -40, -20, -16, -12, -8, -4, 0, 2, 3, 4, 5, 6, 7, 8
};
#define NRF52_LEVEL_COUNT  (int)(sizeof(nrf52_levels)/sizeof(nrf52_levels[0]))
#define NRF52_MIN  (-40)
#define NRF52_MAX  8

/* ── EFR32xG21 — quasi-continuous 0.5 dBm steps (integer rounding) ── */
#define EFR32XG21_MIN  (-30)
#define EFR32XG21_MAX  20

/**
 * Find the nearest supported level from a discrete table.
 */
static int8_t find_nearest_level(const int8_t *levels, int count, int8_t target)
{
    int8_t nearest = levels[0];
    int min_diff = abs((int)target - (int)levels[0]);
    for (int i = 1; i < count; i++) {
        int diff = abs((int)target - (int)levels[i]);
        if (diff < min_diff) {
            min_diff = diff;
            nearest = levels[i];
        }
    }
    return nearest;
}

/**
 * Map requested power for nRF52840/nRF54L15.
 */
int8_t radio_power_nrf52_nearest(int8_t target)
{
    return find_nearest_level(nrf52_levels, NRF52_LEVEL_COUNT, target);
}

/**
 * Map requested power for CC2652R1.
 */
int8_t radio_power_cc2652_nearest(int8_t target)
{
    return find_nearest_level(cc2652r1_levels, CC2652R1_LEVEL_COUNT, target);
}

/**
 * Map requested power for EFR32xG21 (supports every integer dBm in range).
 */
int8_t radio_power_efr32_nearest(int8_t target)
{
    if (target < EFR32XG21_MIN) return EFR32XG21_MIN;
    if (target > EFR32XG21_MAX) return EFR32XG21_MAX;
    return target;
}

/**
 * Determine chip family from active profile and map power accordingly.
 */
static int8_t map_power_for_active_chip(int8_t requested)
{
    libble_ctx_t *ctx = libble_get_ctx();
    if (ctx->active_profile_idx < 0)
        return requested; /* BlueZ — pass through */

    chip_profile_t *cp = ctx->profiles[ctx->active_profile_idx];
    if (!cp || !cp->name)
        return requested;

    if (strcmp(cp->name, "ti_npi") == 0)
        return radio_power_cc2652_nearest(requested);
    else if (strcmp(cp->name, "hci_h4") == 0)
        return radio_power_nrf52_nearest(requested);
    else
        return requested; /* JSON or unknown — pass through */
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Radio TX Power
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_set_radio_tx_power(int8_t power_dbm)
{
    libble_ctx_t *ctx = libble_get_ctx();
    if (!ctx->initialized)
        return BLE_ERR_NODEV;

    int8_t actual = map_power_for_active_chip(power_dbm);

    /* Try chip profile's set_radio_tx_power if available */
    if (ctx->active_profile_idx >= 0) {
        chip_profile_t *cp = ctx->profiles[ctx->active_profile_idx];
        if (cp->set_radio_tx_power) {
            transport_plugin_t *tp = ctx->transports[ctx->active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->set_radio_tx_power(actual, &cmd);
            if (ret < 0) return BLE_ERR_IO;
            ssize_t w = write(tp->get_fd(), cmd.data, cmd.len);
            if (w < 0) return BLE_ERR_IO;
        }
    } else {
        /* BlueZ: use HCI LE Set Advertising Parameters TX Power extension
         * or vendor command — for now pass-through; BlueZ manages power */
        BLE_LOG_DBG("BlueZ transport: radio power managed by kernel, requested=%d", actual);
    }

    BLE_LOG_INFO("Radio TX power set: requested=%d dBm, actual=%d dBm",
             power_dbm, actual);
    return (int)actual;
}

EXPORT int ble_get_radio_power_range(int8_t *min_dbm, int8_t *max_dbm)
{
    if (!min_dbm || !max_dbm)
        return BLE_ERR_INVAL;

    libble_ctx_t *ctx = libble_get_ctx();
    if (!ctx->initialized)
        return BLE_ERR_NODEV;

    if (ctx->active_profile_idx >= 0) {
        chip_profile_t *cp = ctx->profiles[ctx->active_profile_idx];
        if (cp->get_radio_power_range)
            return cp->get_radio_power_range(min_dbm, max_dbm);

        /* Fallback based on profile name */
        if (strcmp(cp->name, "ti_npi") == 0) {
            *min_dbm = CC2652R1_MIN;
            *max_dbm = CC2652R1_MAX;
        } else if (strcmp(cp->name, "hci_h4") == 0) {
            *min_dbm = NRF52_MIN;
            *max_dbm = NRF52_MAX;
        } else {
            *min_dbm = -20;
            *max_dbm = 4;
        }
    } else {
        /* BlueZ — typical range */
        *min_dbm = -40;
        *max_dbm = 20;
    }
    return BLE_OK;
}
