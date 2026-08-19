/**
 * @file ble_status_file.c
 * @brief Runtime status file writer for libble initialization info.
 *
 * Writes initialization status to /var/run/ble-provision.status as JSON.
 * This file is created on successful or failed init, allowing other
 * processes to query the daemon state without ubus.
 *
 * File format example:
 * {
 *   "version": "1.0.0",
 *   "pid": 1234,
 *   "timestamp": "2026-07-28T16:30:00+0800",
 *   "transport": "uart_hci",
 *   "chip_profile": "ti_npi",
 *   "chip_name": "CC2652R1",
 *   "device": "/dev/ttyUSB0",
 *   "baud_rate": 115200,
 *   "adapter": "",
 *   "bluez_version": "",
 *   "radio_power_min": -21,
 *   "radio_power_max": 5,
 *   "init_success": true,
 *   "init_error": ""
 * }
 *
 * For BlueZ transport:
 * {
 *   "version": "1.0.0",
 *   "pid": 1234,
 *   "timestamp": "2026-07-28T16:30:00+0800",
 *   "transport": "bluez",
 *   "chip_profile": "",
 *   "chip_name": "hci0 (Intel AX200)",
 *   "device": "",
 *   "baud_rate": 0,
 *   "adapter": "hci0",
 *   "bluez_version": "5.66",
 *   "radio_power_min": -40,
 *   "radio_power_max": 20,
 *   "init_success": true,
 *   "init_error": ""
 * }
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ble_core_internal.h"
#include "log.h"

/** Status file path */
#define BLE_STATUS_FILE     "/var/run/ble-provision.status"

/** PID file path */
#define BLE_PID_FILE        "/var/run/ble-provision.pid"

/**
 * @brief Detect the chip name based on config.
 */
static const char *detect_chip_name(libble_ctx_t *ctx)
{
    if (!ctx->initialized)
        return "unknown";

    /* If chip config has model info, use it directly */
    if (ctx->config.chip.model[0])
        return ctx->config.chip.model;

    /* BlueZ transport — use adapter name */
    if (ctx->active_transport_idx >= 0 &&
        ctx->transports[ctx->active_transport_idx] &&
        strcmp(ctx->transports[ctx->active_transport_idx]->name, "bluez") == 0) {
        return ctx->config.adapter[0] ? ctx->config.adapter : "hci0";
    }

    /* UART transport — identify by chip profile */
    if (ctx->active_profile_idx >= 0 && ctx->profiles[ctx->active_profile_idx]) {
        const char *profile = ctx->profiles[ctx->active_profile_idx]->name;
        if (strcmp(profile, "ti_npi") == 0)
            return "TI CC2652R1";
        else if (strcmp(profile, "hci_h4") == 0)
            return "Nordic nRF52840/nRF54L15";
        else if (strcmp(profile, "json") == 0)
            return "Custom JSON Firmware";
    }

    return "unknown";
}

/**
 * @brief Get BlueZ version string via bluetoothd or D-Bus.
 *
 * Simplified: reads from bluetoothctl or a known location.
 * Returns empty string if not using BlueZ.
 */
static void get_bluez_version(char *buf, size_t buflen)
{
    buf[0] = '\0';

    FILE *fp = popen("bluetoothd --version 2>/dev/null", "r");
    if (fp) {
        /* Output format: "bluetoothd: 5.66" or "5.66" */
        char line[128];
        if (fgets(line, sizeof(line), fp)) {
            /* Find version number (digits and dots) */
            char *p = line;
            while (*p && (*p < '0' || *p > '9')) p++;
            if (*p) {
                char *end = p;
                while (*end && (*end == '.' || (*end >= '0' && *end <= '9'))) end++;
                *end = '\0';
                snprintf(buf, buflen, "%s", p);
            }
        }
        pclose(fp);
    }
}

/**
 * @brief Get current ISO 8601 timestamp.
 */
static void get_timestamp(char *buf, size_t buflen)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, buflen, "%Y-%m-%dT%H:%M:%S%z", tm_info);
}

/**
 * @brief Write PID file.
 */
static void write_pid_file(void)
{
    FILE *fp = fopen(BLE_PID_FILE, "w");
    if (fp) {
        fprintf(fp, "%d\n", (int)getpid());
        fclose(fp);
    }
}

/**
 * @brief Write initialization status to /var/run/ble-provision.status.
 *
 * Called by ble_core after init completes (success or failure).
 * Also logs the init summary to syslog/stderr.
 *
 * @param ctx         Library context (filled during init).
 * @param success     true if init succeeded, false on failure.
 * @param error_msg   Error description if success==false, NULL otherwise.
 */
void ble_status_write(libble_ctx_t *ctx, bool success, const char *error_msg)
{
    char timestamp[64];
    char bluez_ver[32];
    int8_t pwr_min = 0, pwr_max = 0;

    get_timestamp(timestamp, sizeof(timestamp));

    /* Get BlueZ version if using BlueZ transport */
    bluez_ver[0] = '\0';
    if (ctx->active_transport_idx >= 0 &&
        ctx->transports[ctx->active_transport_idx] &&
        strcmp(ctx->transports[ctx->active_transport_idx]->name, "bluez") == 0) {
        get_bluez_version(bluez_ver, sizeof(bluez_ver));
    }

    /* Get radio power range */
    if (ctx->active_profile_idx >= 0 &&
        ctx->profiles[ctx->active_profile_idx] &&
        ctx->profiles[ctx->active_profile_idx]->get_radio_power_range) {
        ctx->profiles[ctx->active_profile_idx]->get_radio_power_range(&pwr_min, &pwr_max);
    } else if (bluez_ver[0]) {
        /* BlueZ default range */
        pwr_min = -40;
        pwr_max = 20;
    }

    const char *transport_name = "unknown";
    const char *profile_name = "";
    const char *chip_name = detect_chip_name(ctx);

    if (ctx->active_transport_idx >= 0 && ctx->transports[ctx->active_transport_idx])
        transport_name = ctx->transports[ctx->active_transport_idx]->name;
    if (ctx->active_profile_idx >= 0 && ctx->profiles[ctx->active_profile_idx])
        profile_name = ctx->profiles[ctx->active_profile_idx]->name;

    /* ── Write status file ── */
    FILE *fp = fopen(BLE_STATUS_FILE, "w");
    if (fp) {
        fprintf(fp,
            "{\n"
            "  \"version\": \"%d.%d.%d\",\n"
            "  \"pid\": %d,\n"
            "  \"timestamp\": \"%s\",\n"
            "  \"transport\": \"%s\",\n"
            "  \"chip_profile\": \"%s\",\n"
            "  \"chip_vendor\": \"%s\",\n"
            "  \"chip_model\": \"%s\",\n"
            "  \"chip_name\": \"%s\",\n"
            "  \"firmware\": \"%s\",\n"
            "  \"fw_loaded\": %s,\n"
            "  \"device\": \"%s\",\n"
            "  \"baud_rate\": %u,\n"
            "  \"reset_pin\": %d,\n"
            "  \"backdoor_pin\": %d,\n"
            "  \"wdt_enabled\": %s,\n"
            "  \"adapter\": \"%s\",\n"
            "  \"bluez_version\": \"%s\",\n"
            "  \"radio_power_min\": %d,\n"
            "  \"radio_power_max\": %d,\n"
            "  \"init_success\": %s,\n"
            "  \"init_error\": \"%s\"\n"
            "}\n",
            LIBBLE_VERSION_MAJOR, LIBBLE_VERSION_MINOR, LIBBLE_VERSION_PATCH,
            (int)getpid(),
            timestamp,
            transport_name,
            profile_name,
            ctx->config.chip.vendor,
            ctx->config.chip.model,
            chip_name,
            ctx->config.chip.firmware_path,
            ctx->config.chip.fw_loaded ? "true" : "false",
            ctx->config.device_path,
            ctx->config.baud_rate,
            ctx->config.chip.reset_pin,
            ctx->config.chip.backdoor_pin,
            ctx->config.chip.wdt_enabled ? "true" : "false",
            ctx->config.adapter,
            bluez_ver,
            (int)pwr_min, (int)pwr_max,
            success ? "true" : "false",
            error_msg ? error_msg : "");
        fclose(fp);
        BLE_LOG_INFO("Status file written: %s", BLE_STATUS_FILE);
    } else {
        BLE_LOG_WARN("Cannot write status file %s: %s", BLE_STATUS_FILE, strerror(errno));
    }

    /* ── Write PID file ── */
    write_pid_file();

    /* ── Log init summary ── */
    if (success) {
        BLE_LOG_INFO("═══════════════════════════════════════════════════════");
        BLE_LOG_INFO("  BLE Provision Daemon initialized successfully");
        BLE_LOG_INFO("  Transport:    %s", transport_name);
        if (profile_name[0])
            BLE_LOG_INFO("  Chip Profile: %s", profile_name);
        if (ctx->config.chip.vendor[0])
            BLE_LOG_INFO("  Vendor:       %s", ctx->config.chip.vendor);
        if (ctx->config.chip.model[0])
            BLE_LOG_INFO("  Model:        %s", ctx->config.chip.model);
        else
            BLE_LOG_INFO("  Chip Name:    %s", chip_name);
        if (ctx->config.device_path[0])
            BLE_LOG_INFO("  UART Device:  %s @ %u baud", ctx->config.device_path, ctx->config.baud_rate);
        if (ctx->config.chip.reset_pin >= 0)
            BLE_LOG_INFO("  Reset GPIO:   %d", ctx->config.chip.reset_pin);
        if (ctx->config.chip.firmware_path[0])
            BLE_LOG_INFO("  Firmware:     %s (loaded=%s)",
                     ctx->config.chip.firmware_path,
                     ctx->config.chip.fw_loaded ? "yes" : "no");
        if (ctx->config.adapter[0])
            BLE_LOG_INFO("  Adapter:      %s", ctx->config.adapter);
        if (bluez_ver[0])
            BLE_LOG_INFO("  BlueZ:        v%s", bluez_ver);
        BLE_LOG_INFO("  TX Power:     %d to %+d dBm", pwr_min, pwr_max);
        BLE_LOG_INFO("═══════════════════════════════════════════════════════");
    } else {
        BLE_LOG_ERR("═══════════════════════════════════════════════════════");
        BLE_LOG_ERR("  BLE Provision Daemon initialization FAILED");
        BLE_LOG_ERR("  Transport:    %s", transport_name);
        BLE_LOG_ERR("  Error:        %s", error_msg ? error_msg : "unknown");
        BLE_LOG_ERR("═══════════════════════════════════════════════════════");
    }
}

/**
 * @brief Remove status and PID files on shutdown.
 */
void ble_status_cleanup(void)
{
    unlink(BLE_STATUS_FILE);
    unlink(BLE_PID_FILE);
    BLE_LOG_DBG("Status files removed");
}
