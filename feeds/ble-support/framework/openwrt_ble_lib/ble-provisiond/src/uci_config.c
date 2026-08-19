/**
 * @file uci_config.c
 * @brief UCI configuration reader for ble-provisiond.
 *
 * Boot sequence:
 *   1. Read /var/sysinfo/board_name → "vendor,model" (e.g. "edgecore,eap115")
 *   2. Extract model part after comma → "eap115"
 *   3. Use model as UCI section name: uci get ble_chip.eap115.tty
 *   4. Load all chip parameters from that section
 *   5. Auto-detect chip_profile from vendor/model fields
 *
 * UCI package: /etc/config/ble_chip
 *
 * Example:
 *   config ble_chip 'eap115'
 *       option enabled '1'
 *       option vendor 'nordic'
 *       option model 'NRF54L15'
 *       option firmware '/etc/ble_fw/nordic/zephyr_hci_uart_291.signed.bin'
 *       option fw_loaded ''
 *       option tty '/dev/ttyS2'
 *       option baudrate '115200'
 *       option resetpin '542'
 *       option backdoorpin '541'
 *       option wdt_enabled '0'
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include <uci.h>
#include <ble.h>

/** Board name file (OpenWrt standard location) */
#define BOARD_NAME_FILE     "/var/sysinfo/board_name"

/** UCI package for chip hardware config */
#define UCI_PKG_CHIP        "ble_chip"

/** UCI package for application config */
#define UCI_PKG_APP         "ble"

/* ── Helpers ── */

/**
 * @brief Read /var/sysinfo/board_name and extract the model part.
 *
 * File content example: "edgecore,eap115\n"
 * → board_vendor = "edgecore"
 * → board_model  = "eap115"  (used as UCI section name)
 *
 * @param vendor_out  Output buffer for vendor string.
 * @param vendor_sz   Size of vendor buffer.
 * @param model_out   Output buffer for model string (UCI section name).
 * @param model_sz    Size of model buffer.
 * @return 0 on success, -errno on failure.
 */
static int read_board_name(char *vendor_out, size_t vendor_sz,
                           char *model_out, size_t model_sz)
{
    FILE *fp = fopen(BOARD_NAME_FILE, "r");
    if (!fp) {
        fprintf(stderr, "[uci_config] Cannot open %s: %s\n",
                BOARD_NAME_FILE, strerror(errno));
        return -ENOENT;
    }

    char line[128];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -EIO;
    }
    fclose(fp);

    /* Strip trailing newline/whitespace */
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' ||
                       isspace((unsigned char)line[len-1])))
        line[--len] = '\0';

    /* Split by comma: "vendor,model" */
    char *comma = strchr(line, ',');
    if (!comma) {
        /* No comma — treat entire string as model */
        if (vendor_out) vendor_out[0] = '\0';
        if (model_out) strncpy(model_out, line, model_sz - 1);
        return 0;
    }

    *comma = '\0';
    if (vendor_out) strncpy(vendor_out, line, vendor_sz - 1);
    if (model_out) strncpy(model_out, comma + 1, model_sz - 1);

    return 0;
}

/**
 * @brief Detect chip_profile from BLE chip vendor/model UCI options.
 *
 * This determines the FALLBACK profile used when BlueZ is not available
 * and we must talk to the chip directly via UART.
 *
 * Note: SiLabs can provide HCI via its own abstraction layer (BGAPI→HCI bridge
 * in firmware), so its fallback is also HCI H4, not BGAPI.
 * Only TI NPI is truly vendor-only without any HCI path.
 */
static ble_chip_t detect_chip_profile(const char *ble_vendor, const char *ble_model)
{
    if (!ble_vendor || !ble_vendor[0])
        return BLE_CHIP_AUTO;

    if (strcmp(ble_vendor, "nordic") == 0)
        return BLE_CHIP_HCI_H4;   /* nRF52840/nRF54L15 → native HCI H4 */
    else if (strcmp(ble_vendor, "silabs") == 0)
        return BLE_CHIP_HCI_H4;   /* EFR32xG21 → HCI via SiLabs abstraction layer */
    else if (strcmp(ble_vendor, "ti") == 0)
        return BLE_CHIP_TI_NPI;   /* CC2652R1 → vendor-only NPI, no HCI path */

    (void)ble_model;
    return BLE_CHIP_AUTO;
}

/* ── Public API ── */

/**
 * @brief Load BLE configuration from UCI using board_name auto-detection.
 *
 * Flow:
 *   1. Read /var/sysinfo/board_name → extract model (e.g. "eap115")
 *   2. Open UCI package "ble_chip"
 *   3. Look up section by model name: ble_chip.eap115
 *   4. Read all chip options from that section
 *   5. Set device_path, baud_rate, chip_profile accordingly
 *
 * @param config  Output configuration structure.
 * @return 0 on success, negative error code on failure.
 */
int uci_config_load(ble_config_t *config)
{
    if (!config) return -EINVAL;

    /* Defaults */
    memset(config, 0, sizeof(*config));
    config->transport = BLE_TRANSPORT_AUTO;
    config->chip_profile = BLE_CHIP_AUTO;
    config->baud_rate = 115200;
    config->chip.reset_pin = -1;
    config->chip.backdoor_pin = -1;

    /* ──────────────────────────────────────────────────────
     * Step 1: Read board_name to determine UCI section
     * ────────────────────────────────────────────────────── */
    char board_vendor[64] = {0};
    char board_model[64] = {0};

    int ret = read_board_name(board_vendor, sizeof(board_vendor),
                              board_model, sizeof(board_model));
    if (ret < 0) {
        fprintf(stderr, "[uci_config] WARNING: cannot read board_name, "
                "will try 'default' section\n");
        strncpy(board_model, "default", sizeof(board_model) - 1);
    }

    fprintf(stderr, "[uci_config] Board: vendor=%s model=%s → "
            "UCI section: ble_chip.%s\n",
            board_vendor, board_model, board_model);

    /* Store board info in chip section_name */
    strncpy(config->chip.section_name, board_model,
            sizeof(config->chip.section_name) - 1);

    /* ──────────────────────────────────────────────────────
     * Step 2: Open ble_chip UCI package and read section
     * ────────────────────────────────────────────────────── */
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) return -ENOMEM;

    struct uci_package *pkg = NULL;
    ret = uci_load(ctx, UCI_PKG_CHIP, &pkg);
    if (ret != UCI_OK || !pkg) {
        fprintf(stderr, "[uci_config] Cannot load UCI package '%s'\n", UCI_PKG_CHIP);
        uci_free_context(ctx);
        return -ENOENT;
    }

    /* Look up the section matching board model */
    struct uci_section *sec = uci_lookup_section(ctx, pkg, board_model);
    if (!sec) {
        fprintf(stderr, "[uci_config] Section '%s' not found in %s, "
                "trying 'default'\n", board_model, UCI_PKG_CHIP);
        sec = uci_lookup_section(ctx, pkg, "default");
    }

    if (!sec) {
        fprintf(stderr, "[uci_config] No matching ble_chip section found\n");
        uci_unload(ctx, pkg);
        uci_free_context(ctx);
        return -ENOENT;
    }

    /* ──────────────────────────────────────────────────────
     * Step 3: Read all options from the matched section
     * ────────────────────────────────────────────────────── */
    const char *val;

    /* enabled */
    val = uci_lookup_option_string(ctx, sec, "enabled");
    config->chip.enabled = (val && strcmp(val, "1") == 0);

    if (!config->chip.enabled) {
        fprintf(stderr, "[uci_config] Section '%s' is disabled\n", board_model);
        uci_unload(ctx, pkg);
        uci_free_context(ctx);
        return -ENODEV;
    }

    /* vendor (BLE chip vendor, not board vendor) */
    val = uci_lookup_option_string(ctx, sec, "vendor");
    if (val) strncpy(config->chip.vendor, val, sizeof(config->chip.vendor) - 1);

    /* model (BLE chip model) */
    val = uci_lookup_option_string(ctx, sec, "model");
    if (val) strncpy(config->chip.model, val, sizeof(config->chip.model) - 1);

    /* firmware */
    val = uci_lookup_option_string(ctx, sec, "firmware");
    if (val) strncpy(config->chip.firmware_path, val,
                     sizeof(config->chip.firmware_path) - 1);

    /* fw_loaded */
    val = uci_lookup_option_string(ctx, sec, "fw_loaded");
    config->chip.fw_loaded = (val && strlen(val) > 0 && strcmp(val, "0") != 0);

    /* tty → device_path */
    val = uci_lookup_option_string(ctx, sec, "tty");
    if (val) {
        strncpy(config->chip.tty, val, sizeof(config->chip.tty) - 1);
        strncpy(config->device_path, val, sizeof(config->device_path) - 1);
    }

    /* baudrate */
    val = uci_lookup_option_string(ctx, sec, "baudrate");
    if (val) {
        config->chip.baudrate = (uint32_t)atoi(val);
        config->baud_rate = config->chip.baudrate;
    }

    /* resetpin */
    val = uci_lookup_option_string(ctx, sec, "resetpin");
    config->chip.reset_pin = val ? atoi(val) : -1;

    /* backdoorpin */
    val = uci_lookup_option_string(ctx, sec, "backdoorpin");
    config->chip.backdoor_pin = val ? atoi(val) : -1;

    /* wdt_enabled */
    val = uci_lookup_option_string(ctx, sec, "wdt_enabled");
    config->chip.wdt_enabled = (val && strcmp(val, "1") == 0);

    uci_unload(ctx, pkg);
    uci_free_context(ctx);

    /* ──────────────────────────────────────────────────────
     * Step 4: Auto-detect transport
     *
     * RULE: ALL chips, regardless of vendor, use AUTO transport.
     * ble_core always tries BlueZ first:
     *   - If BlueZ has a working hci adapter → use BlueZ
     *   - If not → fallback to direct UART with chip_profile
     *
     * The chip_profile only determines HOW to talk to the chip
     * when falling back to UART. It does NOT affect BlueZ path.
     *
     * Rationale:
     *   - Nordic: native HCI → btattach'd to BlueZ
     *   - SiLabs: HCI via abstraction layer → btattach'd to BlueZ
     *   - TI: some models support HCI (CC2652P4, CC2340), some
     *     only NPI (CC2652R1). Even TI NPI-only chips might be
     *     paired with a separate HCI bridge in the future.
     *   - Always trying BlueZ first is safe: if no adapter found,
     *     BlueZ init simply fails fast (< 100ms) and we fallback.
     * ────────────────────────────────────────────────────── */

    config->transport = BLE_TRANSPORT_AUTO;  /* Always try BlueZ first */
    config->chip_profile = detect_chip_profile(config->chip.vendor,
                                               config->chip.model);

    fprintf(stderr, "[uci_config] Transport: AUTO (BlueZ first, "
            "fallback UART with '%s' profile)\n",
            config->chip_profile == BLE_CHIP_HCI_H4 ? "hci_h4" :
            config->chip_profile == BLE_CHIP_TI_NPI ? "ti_npi" :
            config->chip_profile == BLE_CHIP_JSON ? "json" : "auto");

    /* ──────────────────────────────────────────────────────
     * Step 5: Log summary
     * ────────────────────────────────────────────────────── */
    fprintf(stderr, "[uci_config] Loaded ble_chip.%s:\n", board_model);
    fprintf(stderr, "  vendor=%s model=%s tty=%s baud=%u\n",
            config->chip.vendor, config->chip.model,
            config->device_path, config->baud_rate);
    fprintf(stderr, "  reset_pin=%d backdoor_pin=%d wdt=%d\n",
            config->chip.reset_pin, config->chip.backdoor_pin,
            config->chip.wdt_enabled);
    fprintf(stderr, "  firmware=%s (loaded=%s)\n",
            config->chip.firmware_path,
            config->chip.fw_loaded ? "yes" : "no");
    fprintf(stderr, "  → transport=%d chip_profile=%d\n",
            config->transport, config->chip_profile);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Application configuration: /etc/config/ble
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "uci_app_config.h"

/**
 * @brief Application config structures (read from /etc/config/ble)
 */

/**
 * Load application configuration from /etc/config/ble.
 * Reads sections: ibeacon, scan, gatt, daemon
 */
int uci_app_config_load(ble_app_config_t *app_cfg)
{
    if (!app_cfg) return -EINVAL;
    memset(app_cfg, 0, sizeof(*app_cfg));

    /* Set defaults */
    app_cfg->ibeacon.txpower = -59;
    app_cfg->ibeacon.radio_power = 127; /* BLE_RADIO_POWER_DEFAULT */
    app_cfg->ibeacon.interval_ms = 100;
    strncpy(app_cfg->ibeacon.uuid, "e2c56db6dffb48d2b060d0f5a71096e0", 31);
    app_cfg->ibeacon.uuid[31] = '\0';
    app_cfg->ibeacon.major = 1;
    app_cfg->ibeacon.minor = 1;

    app_cfg->scan.duration_ms = 10000;
    app_cfg->scan.active = true;
    app_cfg->scan.filter_dup = true;
    app_cfg->scan.rssi_threshold = -100;

    app_cfg->gatt.conn_timeout_ms = 10000;
    app_cfg->gatt.auto_discover = true;
    strncpy(app_cfg->gatt.target_addr_type, "public", 6);
    app_cfg->gatt.target_addr_type[6] = '\0';

    strncpy(app_cfg->daemon.log_level, "info", 4);
    app_cfg->daemon.log_level[4] = '\0';
    app_cfg->daemon.ubus_enabled = true;
    app_cfg->daemon.respawn = true;

    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) return -ENOMEM;

    struct uci_package *pkg = NULL;
    int ret = uci_load(ctx, "ble", &pkg);
    if (ret != UCI_OK || !pkg) {
        fprintf(stderr, "[uci_app] Cannot load /etc/config/ble, using defaults\n");
        uci_free_context(ctx);
        return 0; /* Not fatal — use defaults */
    }

    const char *val;
    struct uci_section *sec;

    /* ── ibeacon section ── */
    sec = uci_lookup_section(ctx, pkg, "ibeacon");
    if (sec) {
        val = uci_lookup_option_string(ctx, sec, "enabled");
        app_cfg->ibeacon.enabled = (val && strcmp(val, "1") == 0);

        val = uci_lookup_option_string(ctx, sec, "uuid");
        if (val) strncpy(app_cfg->ibeacon.uuid, val, 31);
        app_cfg->ibeacon.uuid[31] = '\0';

        val = uci_lookup_option_string(ctx, sec, "major");
        if (val) app_cfg->ibeacon.major = (uint16_t)atoi(val);

        val = uci_lookup_option_string(ctx, sec, "minor");
        if (val) app_cfg->ibeacon.minor = (uint16_t)atoi(val);

        val = uci_lookup_option_string(ctx, sec, "txpower");
        if (val) app_cfg->ibeacon.txpower = (int8_t)atoi(val);

        val = uci_lookup_option_string(ctx, sec, "radio_power");
        if (val && val[0]) app_cfg->ibeacon.radio_power = (int8_t)atoi(val);

        val = uci_lookup_option_string(ctx, sec, "interval");
        if (val) app_cfg->ibeacon.interval_ms = (uint16_t)atoi(val);
    }

    /* ── scan section ── */
    sec = uci_lookup_section(ctx, pkg, "scan");
    if (sec) {
        val = uci_lookup_option_string(ctx, sec, "enabled");
        app_cfg->scan.enabled = (val && strcmp(val, "1") == 0);

        val = uci_lookup_option_string(ctx, sec, "duration");
        if (val) app_cfg->scan.duration_ms = (uint32_t)atoi(val);

        val = uci_lookup_option_string(ctx, sec, "active");
        if (val) app_cfg->scan.active = (strcmp(val, "1") == 0);

        val = uci_lookup_option_string(ctx, sec, "filter_dup");
        if (val) app_cfg->scan.filter_dup = (strcmp(val, "1") == 0);

        val = uci_lookup_option_string(ctx, sec, "rssi_threshold");
        if (val) app_cfg->scan.rssi_threshold = (int8_t)atoi(val);

        val = uci_lookup_option_string(ctx, sec, "max_results");
        if (val) app_cfg->scan.max_results = (uint32_t)atoi(val);
    }

    /* ── gatt_server section ── */
    sec = uci_lookup_section(ctx, pkg, "gatt_server");
    if (sec) {
        val = uci_lookup_option_string(ctx, sec, "enabled");
        app_cfg->gatt.enabled = (val && strcmp(val, "1") == 0);

        val = uci_lookup_option_string(ctx, sec, "target_address");
        if (val) { strncpy(app_cfg->gatt.target_address, val, 17); app_cfg->gatt.target_address[17] = '\0'; }

        val = uci_lookup_option_string(ctx, sec, "target_addr_type");
        if (val) { strncpy(app_cfg->gatt.target_addr_type, val, 6); app_cfg->gatt.target_addr_type[6] = '\0'; }

        val = uci_lookup_option_string(ctx, sec, "auto_discover");
        if (val) app_cfg->gatt.auto_discover = (strcmp(val, "1") == 0);

        val = uci_lookup_option_string(ctx, sec, "conn_timeout");
        if (val) app_cfg->gatt.conn_timeout_ms = (uint32_t)atoi(val);
    }

    /* ── daemon section ── */
    sec = uci_lookup_section(ctx, pkg, "daemon");
    if (sec) {
        val = uci_lookup_option_string(ctx, sec, "log_level");
        if (val) { strncpy(app_cfg->daemon.log_level, val, 6); app_cfg->daemon.log_level[6] = '\0'; }

        val = uci_lookup_option_string(ctx, sec, "ubus_enabled");
        if (val) app_cfg->daemon.ubus_enabled = (strcmp(val, "1") == 0);

        val = uci_lookup_option_string(ctx, sec, "respawn");
        if (val) app_cfg->daemon.respawn = (strcmp(val, "1") == 0);
    }

    uci_unload(ctx, pkg);
    uci_free_context(ctx);

    fprintf(stderr, "[uci_app] Loaded /etc/config/ble: ibeacon=%s scan=%s gatt=%s\n",
            app_cfg->ibeacon.enabled ? "on" : "off",
            app_cfg->scan.enabled ? "on" : "off",
            app_cfg->gatt.enabled ? "on" : "off");

    return 0;
}


/* ═══════════════════════════════════════════════════════════════════════════
 *  Convenience helpers for reading single UCI options
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uci_app_get_bool(const char *pkg, const char *section,
                      const char *option, bool default_val)
{
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) return default_val;

    struct uci_package *p = NULL;
    if (uci_load(ctx, pkg, &p) != UCI_OK || !p) {
        uci_free_context(ctx);
        return default_val;
    }

    struct uci_section *sec = uci_lookup_section(ctx, p, section);
    if (!sec) {
        uci_free_context(ctx);
        return default_val;
    }

    const char *val = uci_lookup_option_string(ctx, sec, option);
    bool result = default_val;
    if (val) {
        result = (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
                  strcmp(val, "yes") == 0 || strcmp(val, "on") == 0);
    }

    uci_free_context(ctx);
    return result;
}

int uci_app_get_string(const char *pkg, const char *section,
                       const char *option, char *buf, size_t buf_size,
                       const char *default_val)
{
    if (!buf || buf_size == 0) return -1;

    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        if (default_val)
            snprintf(buf, buf_size, "%s", default_val);
        else
            buf[0] = '\0';
        return 0;
    }

    struct uci_package *p = NULL;
    if (uci_load(ctx, pkg, &p) != UCI_OK || !p) {
        uci_free_context(ctx);
        if (default_val)
            snprintf(buf, buf_size, "%s", default_val);
        else
            buf[0] = '\0';
        return 0;
    }

    struct uci_section *sec = uci_lookup_section(ctx, p, section);
    const char *val = NULL;
    if (sec)
        val = uci_lookup_option_string(ctx, sec, option);

    if (val)
        snprintf(buf, buf_size, "%s", val);
    else if (default_val)
        snprintf(buf, buf_size, "%s", default_val);
    else
        buf[0] = '\0';

    uci_free_context(ctx);
    return 0;
}
