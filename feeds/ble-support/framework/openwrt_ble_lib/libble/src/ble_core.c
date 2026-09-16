/**
 * @file ble_core.c
 * @brief libble core implementation — public API.
 *
 * Implements all ble_*() functions declared in ble.h.
 * Manages transport plugin registry, chip profile dispatch,
 * event subscriber list, and library lifecycle.
 *
 * Compiled with -fvisibility=hidden; only symbols with
 * __attribute__((visibility("default"))) are exported.
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "ble_core_internal.h"
#include "ble_status_file.h"
#include "log.h"

#define EXPORT __attribute__((visibility("default")))

/* ── Singleton context ── */
static libble_ctx_t g_ctx;

/**
 * @brief Read local BD address after transport init.
 *
 * - BlueZ: reads /sys/class/bluetooth/<adapter>/address
 * - UART/HCI: sends HCI_Read_BD_ADDR (opcode 0x1009) and parses response
 */
static void read_local_bd_address(void)
{
    g_ctx.bd_address[0] = '\0';

    /* Try sysfs first (works for BlueZ and btattach'd UART) */
    const char *adapter = g_ctx.config.adapter[0] ? g_ctx.config.adapter : "hci0";
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/bluetooth/%s/address", adapter);
    FILE *fp = fopen(path, "r");
    if (fp) {
        if (fgets(g_ctx.bd_address, sizeof(g_ctx.bd_address), fp)) {
            size_t len = strlen(g_ctx.bd_address);
            while (len > 0 && (g_ctx.bd_address[len-1] == '\n' ||
                               g_ctx.bd_address[len-1] == '\r'))
                g_ctx.bd_address[--len] = '\0';
        }
        fclose(fp);
        if (g_ctx.bd_address[0]) {
            BLE_LOG_INFO("Local BD Address: %s (from sysfs)", g_ctx.bd_address);
            return;
        }
    }

    /*
     * Fallback for UART-only chips (no hci adapter in sysfs):
     * Derive virtual BLE MAC from eth0 MAC (same as vendor script).
     * Rule: take eth0 MAC, OR first byte with 0x02 (locally administered bit).
     */
    fp = fopen("/sys/class/net/eth0/address", "r");
    if (!fp)
        fp = fopen("/sys/class/net/br-lan/address", "r");
    if (fp) {
        char eth_mac[18] = {0};
        if (fgets(eth_mac, sizeof(eth_mac), fp)) {
            size_t len = strlen(eth_mac);
            while (len > 0 && (eth_mac[len-1] == '\n' || eth_mac[len-1] == '\r'))
                eth_mac[--len] = '\0';

            /* Parse eth0 MAC bytes */
            unsigned int m[6] = {0};
            if (sscanf(eth_mac, "%x:%x:%x:%x:%x:%x",
                       &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
                /* Set locally-administered bit on first byte */
                m[0] |= 0x02;
                snprintf(g_ctx.bd_address, sizeof(g_ctx.bd_address),
                         "%02X:%02X:%02X:%02X:%02X:%02X",
                         m[0] & 0xFF, m[1] & 0xFF, m[2] & 0xFF,
                         m[3] & 0xFF, m[4] & 0xFF, m[5] & 0xFF);
                BLE_LOG_INFO("Local BD Address: %s (derived from eth0)", g_ctx.bd_address);
            }
        }
        fclose(fp);
        if (g_ctx.bd_address[0])
            return;
    }

    /* Last resort: try HCI Read BD Addr for standard HCI H4 chips */
    if (g_ctx.active_transport_idx >= 0 &&
        g_ctx.transports[g_ctx.active_transport_idx]) {
        transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
        if (tp->send_raw && tp->recv_raw) {
            int is_ti = (g_ctx.active_profile_idx >= 0 &&
                         g_ctx.profiles[g_ctx.active_profile_idx] &&
                         strcmp(g_ctx.profiles[g_ctx.active_profile_idx]->name, "ti_npi") == 0);
            if (!is_ti) {
                /* Standard HCI H4: opcode=0x1009, no params */
                uint8_t cmd[] = { 0x01, 0x09, 0x10, 0x00 };
                if (tp->send_raw(cmd, 4, tp->priv) >= 0) {
                    uint8_t resp[16];
                    int n = tp->recv_raw(resp, sizeof(resp), 2000, tp->priv);
                    if (n >= 13 && resp[0] == 0x04 && resp[1] == 0x0E &&
                        resp[4] == 0x09 && resp[5] == 0x10 && resp[6] == 0x00) {
                        snprintf(g_ctx.bd_address, sizeof(g_ctx.bd_address),
                                 "%02X:%02X:%02X:%02X:%02X:%02X",
                                 resp[12], resp[11], resp[10],
                                 resp[9], resp[8], resp[7]);
                        BLE_LOG_INFO("Local BD Address: %s (from HCI)", g_ctx.bd_address);
                    }
                }
            }
            /* TI chips: MAC already derived from eth0 above, or set via SetBDADDR later */
        }
    }

    if (g_ctx.bd_address[0]) {
        BLE_LOG_INFO("Local BD Address: %s", g_ctx.bd_address);
    } else {
        BLE_LOG_WARN("Could not determine local BD address");
    }
}
libble_ctx_t *libble_get_ctx(void)
{
    return &g_ctx;
}

/* ── Error strings ── */
static const char *err_strings[] = {
    [0] = "Success",
    [1] = "Invalid argument",
    [2] = "Out of memory",
    [3] = "No device",
    [4] = "Busy",
    [5] = "Timeout",
    [6] = "I/O error",
    [7] = "Not supported",
    [8] = "Already active",
    [9] = "Not connected",
};

/* ── Event dispatch ── */
void libble_dispatch_event(const ble_event_t *event)
{
    if (!event) return;
    for (int i = 0; i < g_ctx.subscriber_count; i++) {
        if (g_ctx.subscribers[i].cb) {
            g_ctx.subscribers[i].cb(event, g_ctx.subscribers[i].user_data);
        }
    }
}

/* ── Transport event bridge ── */
void libble_transport_event_cb(const ble_event_t *event, void *ctx)
{
    (void)ctx;
    if (!event) return;

    /* Update internal state based on event type */
    switch (event->type) {
    case BLE_EVT_SCAN_COMPLETE:
        g_ctx.scan_active = false;
        g_ctx.state = BLE_STATE_IDLE;
        break;
    case BLE_EVT_CONNECT:
        g_ctx.gatt_connected = true;
        g_ctx.state = BLE_STATE_CONNECTED;
        break;
    case BLE_EVT_DISCONNECT:
        g_ctx.gatt_connected = false;
        g_ctx.state = BLE_STATE_IDLE;
        break;
    default:
        break;
    }

    libble_dispatch_event(event);
}

/* ── Transport activation with auto-detect ── */
static int activate_transport(void)
{
    const ble_config_t *cfg = &g_ctx.config;
    const char *tp_config;
    int ret;

    if (cfg->transport == BLE_TRANSPORT_AUTO) {
        /*
         * Smart auto-detect:
         * - If chip config specifies a vendor (nordic/ti/silabs) with a tty device,
         *   skip BlueZ and go directly to UART. This avoids BlueZ accidentally
         *   claiming ownership when the chip is NPI-only (e.g. TI CC2652R1).
         * - Otherwise, try BlueZ first, then UART.
         */
        bool prefer_uart = (cfg->chip.vendor[0] && cfg->chip.tty[0]);
        if (prefer_uart) {
            BLE_LOG_INFO("Transport auto-detect: UCI chip config present "
                         "(vendor=%s tty=%s) — using UART directly",
                         cfg->chip.vendor, cfg->chip.tty);
        } else {
            BLE_LOG_INFO("Transport auto-detect: trying all registered transports");
        }

        for (int i = 0; i < g_ctx.transport_count; i++) {
            transport_plugin_t *tp = g_ctx.transports[i];

            /* Skip BlueZ if UCI explicitly configures a UART chip */
            if (prefer_uart && strcmp(tp->name, "bluez") == 0)
                continue;

            if (strcmp(tp->name, "bluez") == 0)
                tp_config = cfg->adapter[0] ? cfg->adapter : "hci0";
            else
                tp_config = cfg->device_path[0] ? cfg->device_path : "/dev/ttyUSB0";

            ret = tp->init(tp_config);
            if (ret == 0) {
                g_ctx.active_transport_idx = i;
                tp->active = true;
                BLE_LOG_INFO("Transport '%s' activated", tp->name);
                return BLE_OK;
            }
            BLE_LOG_DBG("Transport '%s' failed (%d), next...", tp->name, ret);
        }
        BLE_LOG_ERR("No transport initialized successfully");
        return BLE_ERR_NODEV;
    }

    /* Specific transport requested */
    int target = -1;
    for (int i = 0; i < g_ctx.transport_count; i++) {
        if ((cfg->transport == BLE_TRANSPORT_BLUEZ &&
             strcmp(g_ctx.transports[i]->name, "bluez") == 0) ||
            (cfg->transport == BLE_TRANSPORT_UART &&
             strcmp(g_ctx.transports[i]->name, "uart_hci") == 0)) {
            target = i;
            break;
        }
    }

    if (target < 0) {
        BLE_LOG_ERR("Requested transport not registered");
        return BLE_ERR_NODEV;
    }

    transport_plugin_t *tp = g_ctx.transports[target];
    tp_config = (cfg->transport == BLE_TRANSPORT_BLUEZ)
        ? (cfg->adapter[0] ? cfg->adapter : "hci0")
        : (cfg->device_path[0] ? cfg->device_path : "/dev/ttyUSB0");

    ret = tp->init(tp_config);
    if (ret < 0) {
        BLE_LOG_ERR("Transport '%s' init failed: %d", tp->name, ret);
        return BLE_ERR_NODEV;
    }

    g_ctx.active_transport_idx = target;
    tp->active = true;
    BLE_LOG_INFO("Transport '%s' activated", tp->name);
    return BLE_OK;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Lifecycle
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_init(const ble_config_t *config)
{
    if (g_ctx.initialized)
        return BLE_ERR_ALREADY;

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.active_transport_idx = -1;
    g_ctx.active_profile_idx = -1;
    g_ctx.state = BLE_STATE_OFF;

    if (config)
        memcpy(&g_ctx.config, config, sizeof(ble_config_t));

    /* Register built-in transports */
    g_ctx.transports[g_ctx.transport_count++] = &bluez_transport_plugin;
    g_ctx.transports[g_ctx.transport_count++] = &uart_transport_plugin;

    /* Register built-in chip profiles */
    g_ctx.profiles[g_ctx.profile_count++] = &chip_profile_hci_h4;
    g_ctx.profiles[g_ctx.profile_count++] = &chip_profile_ti_npi;
    g_ctx.profiles[g_ctx.profile_count++] = &chip_profile_json;

    /* Activate transport */
    int ret = activate_transport();
    if (ret != BLE_OK) {
        ble_status_write(&g_ctx, false, "transport activation failed");
        return ret;
    }

    /* Register internal event callback with transport */
    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
    tp->register_event_callback(libble_transport_event_cb, &g_ctx);

    /* Select chip profile if UART transport */
    if (g_ctx.config.transport == BLE_TRANSPORT_UART ||
        (g_ctx.config.transport == BLE_TRANSPORT_AUTO &&
         strcmp(tp->name, "uart_hci") == 0)) {
        if (g_ctx.config.chip_profile == BLE_CHIP_AUTO) {
            g_ctx.active_profile_idx = 0; /* default to HCI H4 */
        } else {
            g_ctx.active_profile_idx = (int)g_ctx.config.chip_profile - 1;
        }

        /* Initialize chip profile (e.g. GAP_DeviceInit for TI NPI, HCI Reset for H4) */
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        BLE_LOG_INFO("Initializing chip profile: %s", cp->name);

        /* Pass GAP roles + firmware mode to TI profile before init.
         * The firmware kind is derived from the firmware file name (TI only):
         * a path containing "multi_role" / "multirole" means a self-running app
         * firmware; anything else is treated as host_test (network processor). */
        if (strcmp(cp->name, "ti_npi") == 0) {
            extern void ti_npi_set_roles(uint8_t roles);
            extern void ti_npi_set_multirole(bool enable);
            ti_npi_set_roles(g_ctx.config.chip.roles);

            const char *fw = g_ctx.config.chip.firmware_path;
            bool mr = (fw && (strstr(fw, "multi_role") || strstr(fw, "multirole")));
            ti_npi_set_multirole(mr);
            BLE_LOG_INFO("TI: firmware '%s' → %s mode",
                         (fw && *fw) ? fw : "(none)",
                         mr ? "multi_role (self-running app; host sends no chip init)"
                            : "host_test (host-driven HCI)");
        }

        /* Register chip profile's RX parser with the UART transport so that
         * vendor-specific events (e.g. TI GAP_EVT_ADV_REPORT) are parsed. */
        if (cp->parse_rx) {
            extern void uart_transport_set_chip_parser(
                int (*)(const uint8_t *, uint16_t,
                        void (*)(const ble_event_t *, void *), void *));
            uart_transport_set_chip_parser(cp->parse_rx);
            BLE_LOG_INFO("Registered %s RX parser with UART transport", cp->name);
        }

        /* send_fn wrapper for chip profile init */
        int chip_init_ret = cp->init(
            (int (*)(const uint8_t *, uint16_t, void *))tp->send_raw,
            tp->priv);
        if (chip_init_ret < 0) {
            BLE_LOG_ERR("Chip profile '%s' init send failed: %d", cp->name, chip_init_ret);
            ble_status_write(&g_ctx, false, "chip profile init failed (send)");
            tp->deinit();
            return BLE_ERR_IO;
        }

        /*
         * Wait for chip init to complete.
         * Do NOT read UART response — TI chip works fine without host
         * reading the responses (same as the working shell script).
         * Just flush the RX buffer after waiting.
         *
         * EXCEPTION: multi_role firmware mode. The chip's app boots on its own
         * and may emit NPI GATT frames immediately (e.g. connection events). We
         * must NOT wait+drain here or we would swallow those frames. The RX
         * parser (already registered above) handles everything the FW sends.
         */
        extern bool ti_npi_is_multirole(void);
        bool skip_drain = (strcmp(cp->name, "ti_npi") == 0) && ti_npi_is_multirole();
        if (skip_drain) {
            BLE_LOG_INFO("Chip init: multi_role mode — not draining RX "
                         "(letting NPI GATT frames reach the parser)");
        } else {
            usleep(1000000);  /* 1 second — enough for GAP_DeviceInitDone */
            /* Flush any accumulated RX data */
            if (tp->recv_raw) {
                uint8_t drain[256];
                int n = tp->recv_raw(drain, sizeof(drain), 100, tp->priv);
                BLE_LOG_INFO("Chip init: waited 1s, drained %d bytes from RX", n > 0 ? n : 0);
            }
        }
    }

    g_ctx.initialized = true;
    g_ctx.state = BLE_STATE_IDLE;

    /* Read local BD address (if not already read during chip profile init) */
    if (!g_ctx.bd_address[0])
        read_local_bd_address();

    /* Write runtime status file */
    ble_status_write(&g_ctx, true, NULL);

    BLE_LOG_INFO("libble initialized (transport=%s)", tp->name);
    return BLE_OK;
}

EXPORT int ble_init_from_uci(const char *uci_path)
{
    /* Parse UCI and fill ble_config_t, then call ble_init() */
    ble_config_t config;
    memset(&config, 0, sizeof(config));
    config.transport = BLE_TRANSPORT_AUTO;
    config.chip_profile = BLE_CHIP_AUTO;
    config.baud_rate = 115200;
    strncpy(config.adapter, "hci0", sizeof(config.adapter) - 1);

    (void)uci_path; /* UCI parsing done in ble-provisiond; stub for library */
    /* Applications should use ble_init() directly or rely on daemon */

    return ble_init(&config);
}

EXPORT void ble_deinit(void)
{
    if (!g_ctx.initialized)
        return;

    /* Stop active operations */
    if (g_ctx.scan_active)
        ble_scan_stop();
    if (g_ctx.beacon_active)
        ble_beacon_stop();

    /* Deinit active transport */
    if (g_ctx.active_transport_idx >= 0) {
        transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
        if (tp->active) {
            tp->deinit();
            tp->active = false;
        }
    }

    g_ctx.initialized = false;
    g_ctx.state = BLE_STATE_OFF;

    /* Remove runtime status/PID files */
    ble_status_cleanup();

    BLE_LOG_INFO("libble deinitialized");
}

EXPORT int ble_get_fd(void)
{
    if (!g_ctx.initialized || g_ctx.active_transport_idx < 0)
        return -1;
    return g_ctx.transports[g_ctx.active_transport_idx]->get_fd();
}

EXPORT int ble_process(void)
{
    if (!g_ctx.initialized || g_ctx.active_transport_idx < 0)
        return BLE_ERR_NODEV;
    return g_ctx.transports[g_ctx.active_transport_idx]->process_events();
}

EXPORT int ble_subscribe(ble_event_cb_t cb, void *user_data)
{
    if (!cb)
        return BLE_ERR_INVAL;
    if (g_ctx.subscriber_count >= SUBSCRIBER_MAX)
        return BLE_ERR_NOMEM;

    g_ctx.subscribers[g_ctx.subscriber_count].cb = cb;
    g_ctx.subscribers[g_ctx.subscriber_count].user_data = user_data;
    g_ctx.subscriber_count++;
    return BLE_OK;
}

EXPORT int ble_unsubscribe(ble_event_cb_t cb)
{
    for (int i = 0; i < g_ctx.subscriber_count; i++) {
        if (g_ctx.subscribers[i].cb == cb) {
            int remaining = g_ctx.subscriber_count - i - 1;
            if (remaining > 0)
                memmove(&g_ctx.subscribers[i], &g_ctx.subscribers[i + 1],
                        remaining * sizeof(ble_subscriber_t));
            g_ctx.subscriber_count--;
            return BLE_OK;
        }
    }
    return BLE_ERR_INVAL;
}

EXPORT const char *ble_get_transport_name(void)
{
    if (!g_ctx.initialized || g_ctx.active_transport_idx < 0)
        return "none";
    return g_ctx.transports[g_ctx.active_transport_idx]->name;
}

EXPORT const char *ble_get_bd_address(void)
{
    if (!g_ctx.initialized || !g_ctx.bd_address[0])
        return "";
    return g_ctx.bd_address;
}

EXPORT const char *ble_get_chip_profile_name(void)
{
    if (g_ctx.active_profile_idx < 0)
        return NULL;
    return g_ctx.profiles[g_ctx.active_profile_idx]->name;
}

EXPORT const char *ble_strerror(int err)
{
    int idx = (err < 0) ? -err : err;
    if (idx >= 0 && idx <= 9)
        return err_strings[idx];
    return "Unknown error";
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Scan
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_scan_start(uint32_t duration_ms, bool active, bool filter_dup)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (g_ctx.scan_active)
        return BLE_ERR_ALREADY;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    /* Build scan command via chip profile if UART, else direct HCI */
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        int ret = cp->build_scan_start(duration_ms, active, filter_dup, &cmd);
        if (ret < 0) {
            BLE_LOG_ERR("Scan: build command failed: %d", ret);
            return BLE_ERR_IO;
        }
        int fd = tp->get_fd();
        BLE_LOG_DBG("Scan: sending %u bytes via %s (fd=%d)",
                    cmd.len, cp->name, fd);
        if (cmd.len > 0) {
            char hex[97] = {0};
            int dump_len = (cmd.len > 32) ? 32 : (int)cmd.len;
            for (int i = 0; i < dump_len; i++)
                sprintf(hex + i * 3, "%02X ", cmd.data[i]);
            BLE_LOG_DBG("Scan TX: [%s]", hex);
        }

        ssize_t w;
        if (cmd.num_segments > 0) {
            for (int s = 0; s < cmd.num_segments && s < CHIP_CMD_MAX_SEGMENTS; s++) {
                w = write(fd, &cmd.data[cmd.seg_offset[s]], cmd.seg_len[s]);
                if (w != (ssize_t)cmd.seg_len[s]) {
                    BLE_LOG_ERR("Scan: seg[%d] write failed", s);
                    return BLE_ERR_IO;
                }
                if (cmd.seg_delay_ms[s] > 0)
                    usleep(cmd.seg_delay_ms[s] * 1000);
                /* Note: do NOT recv here — adv reports are read by the
                 * event loop (uloop) via uart_process_events, mirroring
                 * the ec_blescan reference design (dedicated read loop). */
            }
        } else {
            w = write(fd, cmd.data, cmd.len);
            if (w != (ssize_t)cmd.len) {
                BLE_LOG_ERR("Scan: write failed (%d/%u): %s",
                            (int)w, cmd.len, strerror(errno));
                return BLE_ERR_IO;
            }
        }
        BLE_LOG_DBG("Scan: command sent successfully (%u bytes)", cmd.len);
    } else {
        /* BlueZ transport — use HCI abstraction */
        ble_hci_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        /* Set scan params */
        cmd.opcode = 0x200B;
        cmd.params[0] = active ? 0x01 : 0x00;
        cmd.params[1] = 0x60; cmd.params[2] = 0x00;
        cmd.params[3] = 0x30; cmd.params[4] = 0x00;
        cmd.params[5] = 0x00; cmd.params[6] = 0x00;
        cmd.param_len = 7;
        tp->send_hci_cmd(&cmd);

        /* Enable scan */
        memset(&cmd, 0, sizeof(cmd));
        cmd.opcode = 0x200C;
        cmd.params[0] = 0x01;
        cmd.params[1] = filter_dup ? 0x01 : 0x00;
        cmd.param_len = 2;
        int ret = tp->send_hci_cmd(&cmd);
        if (ret < 0) return BLE_ERR_IO;
    }

    g_ctx.scan_active = true;
    g_ctx.state = BLE_STATE_SCANNING;
    BLE_LOG_INFO("Scan started (duration=%ums, active=%d)", duration_ms, active);
    return BLE_OK;
}

EXPORT int ble_scan_stop(void)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!g_ctx.scan_active)
        return BLE_OK;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        int ret = cp->build_scan_stop(&cmd);
        if (ret < 0) return BLE_ERR_IO;
        int fd = tp->get_fd();
        if (cmd.num_segments > 0) {
            for (int s = 0; s < cmd.num_segments && s < CHIP_CMD_MAX_SEGMENTS; s++) {
                write(fd, &cmd.data[cmd.seg_offset[s]], cmd.seg_len[s]);
                if (cmd.seg_delay_ms[s] > 0)
                    usleep(cmd.seg_delay_ms[s] * 1000);
            }
        } else {
            write(fd, cmd.data, cmd.len);
        }
    } else {
        ble_hci_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.opcode = 0x200C;
        cmd.params[0] = 0x00;
        cmd.params[1] = 0x00;
        cmd.param_len = 2;
        tp->send_hci_cmd(&cmd);
    }

    g_ctx.scan_active = false;
    g_ctx.state = BLE_STATE_IDLE;
    BLE_LOG_INFO("Scan stopped");
    return BLE_OK;
}

EXPORT bool ble_scan_is_active(void)
{
    return g_ctx.scan_active;
}

/*
 * Force-send a scan-stop to the chip regardless of our own scan_active state.
 * Used on the TI multi_role transport to clear a scan the firmware kept running
 * across a daemon restart (the daemon's state is fresh but the chip's is not).
 */
EXPORT int ble_ti_force_scan_stop(void)
{
    g_ctx.scan_active = true;   /* bypass the not-active early return */
    return ble_scan_stop();
}

EXPORT int ble_ti_force_beacon_stop(void)
{
    g_ctx.beacon_active = true; /* bypass the not-active early return */
    return ble_beacon_stop();
}

/*
 * Update the recorded local BD address with the value the firmware actually
 * advertises (TI multi_role reports its own public address, which differs from
 * the eth0-derived value computed at init). Rewrites the status file so
 * /var/run/ble-provision.status and ble_get_bd_address() reflect the real MAC.
 */
EXPORT int ble_ti_set_actual_bd_address(const uint8_t addr_le[6])
{
    if (!g_ctx.initialized || !addr_le) return BLE_ERR_INVAL;
    snprintf(g_ctx.bd_address, sizeof(g_ctx.bd_address),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             addr_le[5], addr_le[4], addr_le[3],
             addr_le[2], addr_le[1], addr_le[0]);
    BLE_LOG_INFO("Local BD Address updated from firmware: %s", g_ctx.bd_address);
    ble_status_write(&g_ctx, true, NULL);
    return BLE_OK;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Beacon
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_beacon_start(const ble_beacon_config_t *config)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!config)
        return BLE_ERR_INVAL;
    if (g_ctx.beacon_active)
        return BLE_ERR_ALREADY;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    /* Set radio TX power if explicitly configured */
    if (config->radio_power_dbm != BLE_RADIO_POWER_DEFAULT) {
        int ret = ble_set_radio_tx_power(config->radio_power_dbm);
        if (ret < -128)
            BLE_LOG_ERR("Failed to set radio TX power: %d", ret);
        else
            BLE_LOG_INFO("Radio TX power set to %d dBm", ret);
        /* Wait for chip to process before sending more commands */
        usleep(200000);
    }

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        int ret = cp->build_beacon_start(config, &cmd);
        if (ret < 0) {
            BLE_LOG_ERR("Beacon: build command failed: %d", ret);
            return BLE_ERR_IO;
        }
        int fd = tp->get_fd();
        BLE_LOG_DBG("Beacon: sending %u bytes via %s (fd=%d)",
                    cmd.len, cp->name, fd);
        /* Log first 48 bytes of command for debug */
        if (cmd.len > 0) {
            char hex[145] = {0};
            int dump_len = (cmd.len > 48) ? 48 : (int)cmd.len;
            for (int i = 0; i < dump_len; i++)
                sprintf(hex + i * 3, "%02X ", cmd.data[i]);
            BLE_LOG_DBG("Beacon TX: [%s]", hex);
        }

        ssize_t written;
        if (cmd.num_segments > 0) {
            /* Multi-segment: send each sub-command with inter-command delay */
            BLE_LOG_DBG("Beacon: sending %u segments", cmd.num_segments);

            for (int s = 0; s < cmd.num_segments && s < CHIP_CMD_MAX_SEGMENTS; s++) {
                uint16_t off = cmd.seg_offset[s];
                uint16_t slen = cmd.seg_len[s];
                uint16_t delay = cmd.seg_delay_ms[s];

                /* Flush RX before sending (clear any pending responses).
                 * NOT in multi_role mode: RX belongs to the NPI parser/event
                 * loop; draining here would swallow BDADDR/READY/scan frames. */
                {
                    extern bool ti_npi_is_multirole(void);
                    bool mr = (strcmp(cp->name, "ti_npi") == 0) && ti_npi_is_multirole();
                    if (!mr && tp->recv_raw) {
                        uint8_t drain[64];
                        tp->recv_raw(drain, sizeof(drain), 50, tp->priv);
                    }
                }

                written = write(fd, &cmd.data[off], slen);
                if (written != (ssize_t)slen) {
                    BLE_LOG_ERR("Beacon: seg[%d] write failed (%d/%u)",
                                s, (int)written, slen);
                    return BLE_ERR_IO;
                }
                BLE_LOG_DBG("Beacon: seg[%d] sent %u bytes, delay %ums",
                            s, slen, delay);
                if (delay > 0)
                    usleep(delay * 1000);
            }
            BLE_LOG_DBG("Beacon: all %u segments sent OK", cmd.num_segments);
        } else {
            /* Single command: send all at once */
            written = write(fd, cmd.data, cmd.len);
            if (written != (ssize_t)cmd.len) {
                BLE_LOG_ERR("Beacon: write failed (%d/%u): %s",
                            (int)written, cmd.len, strerror(errno));
                return BLE_ERR_IO;
            }
            BLE_LOG_DBG("Beacon: command sent (%u bytes)", cmd.len);
        }

        /*
         * Final drain — log whatever the chip sent back. SKIP in multi_role
         * mode: the firmware sends NPI events (BDADDR / READY / scan results)
         * asynchronously on this same UART, and they MUST reach the NPI parser
         * via the event loop. Draining here would consume and discard them —
         * which is exactly what dropped the BDADDR+READY frames and left the
         * status file / provisioning stale.
         */
        {
            extern bool ti_npi_is_multirole(void);
            bool mr = (strcmp(cp->name, "ti_npi") == 0) && ti_npi_is_multirole();
            if (!mr && tp->recv_raw) {
                usleep(500000);  /* Wait 500ms for all responses to arrive */
                uint8_t resp[256];
                int total_rx = 0;
                for (int attempt = 0; attempt < 3; attempt++) {
                    int n = tp->recv_raw(resp + total_rx,
                                         sizeof(resp) - total_rx, 200, tp->priv);
                    if (n > 0)
                        total_rx += n;
                    else
                        break;
                }
                if (total_rx > 0) {
                    char rhex[385] = {0};
                    int rlen = (total_rx > 64) ? 64 : total_rx;
                    for (int i = 0; i < rlen; i++)
                        sprintf(rhex + i * 3, "%02X ", resp[i]);
                    BLE_LOG_DBG("Beacon RX (%d bytes): [%s]", total_rx, rhex);
                }
            }
        }
    } else {
        /*
         * BlueZ path: advertising is handled by the daemon's ble_advertise
         * module (LEAdvertisingManager1 API) at the application layer.
         * libble just marks beacon as active; actual advertising is started
         * by ibeacon_app via ble_adv_start_ibeacon() in ble-provisiond.
         */
    }

    g_ctx.beacon_active = true;
    g_ctx.state = BLE_STATE_ADVERTISING;
    BLE_LOG_INFO("Beacon started (uuid=%s major=%u minor=%u)",
             config->uuid, config->major, config->minor);
    return BLE_OK;
}

EXPORT int ble_beacon_stop(void)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!g_ctx.beacon_active)
        return BLE_OK;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        chip_cmd_buf_t cmd;
        cp->build_beacon_stop(&cmd);
        int fd = tp->get_fd();
        if (cmd.num_segments > 0) {
            for (int s = 0; s < cmd.num_segments && s < CHIP_CMD_MAX_SEGMENTS; s++) {
                write(fd, &cmd.data[cmd.seg_offset[s]], cmd.seg_len[s]);
                if (cmd.seg_delay_ms[s] > 0)
                    usleep(cmd.seg_delay_ms[s] * 1000);
            }
        } else {
            write(fd, cmd.data, cmd.len);
        }
    } else {
        /* BlueZ: advertising stopped by daemon's ble_advertise module */
    }

    g_ctx.beacon_active = false;
    g_ctx.state = BLE_STATE_IDLE;
    BLE_LOG_INFO("Beacon stopped");
    return BLE_OK;
}

EXPORT bool ble_beacon_is_active(void)
{
    return g_ctx.beacon_active;
}

/* ── UUID helpers ── */

EXPORT int ble_uuid_parse(const char *uuid_str, uint8_t out_bytes[16])
{
    if (!uuid_str || strlen(uuid_str) < 36)
        return BLE_ERR_INVAL;

    int idx = 0;
    for (int i = 0; i < 36 && idx < 16; i++) {
        if (uuid_str[i] == '-') continue;
        char hex[3] = { uuid_str[i], uuid_str[i + 1], '\0' };
        char *endp;
        unsigned long val = strtoul(hex, &endp, 16);
        if (*endp != '\0') return BLE_ERR_INVAL;
        out_bytes[idx++] = (uint8_t)val;
        i++;
    }
    return (idx == 16) ? BLE_OK : BLE_ERR_INVAL;
}

EXPORT int ble_uuid_format(const uint8_t bytes[16], char out_str[BLE_UUID_STR_LEN])
{
    if (!bytes || !out_str)
        return BLE_ERR_INVAL;
    snprintf(out_str, BLE_UUID_STR_LEN,
             "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);
    return BLE_OK;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — GATT
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_gatt_connect(const char *address, uint8_t addr_type)
{
    if (!g_ctx.initialized)
        return BLE_ERR_NODEV;
    if (!address)
        return BLE_ERR_INVAL;
    if (g_ctx.gatt_connected)
        return BLE_ERR_BUSY;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_connect) {
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_connect(address, addr_type, &cmd);
            if (ret == -ENOSYS) return BLE_ERR_NOSYS;
            if (ret < 0) return BLE_ERR_IO;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
        return BLE_ERR_NOSYS;
    }

    /* BlueZ: LE Create Connection HCI */
    ble_hci_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = 0x200D;
    cmd.params[0] = 0x60; cmd.params[1] = 0x00;
    cmd.params[2] = 0x30; cmd.params[3] = 0x00;
    cmd.params[4] = 0x00;
    cmd.params[5] = addr_type;
    /* Parse address bytes */
    unsigned int a[6];
    if (sscanf(address, "%02x:%02x:%02x:%02x:%02x:%02x",
               &a[5], &a[4], &a[3], &a[2], &a[1], &a[0]) != 6)
        return BLE_ERR_INVAL;
    for (int i = 0; i < 6; i++) cmd.params[6 + i] = (uint8_t)a[i];
    cmd.params[12] = 0x18; cmd.params[13] = 0x00;
    cmd.params[14] = 0x28; cmd.params[15] = 0x00;
    cmd.params[16] = 0x00; cmd.params[17] = 0x00;
    cmd.params[18] = 0xC8; cmd.params[19] = 0x00;
    cmd.params[20] = 0x00; cmd.params[21] = 0x00;
    cmd.params[22] = 0x00; cmd.params[23] = 0x00;
    cmd.param_len = 25;
    int ret = tp->send_hci_cmd(&cmd);
    return (ret < 0) ? BLE_ERR_IO : BLE_OK;
}

EXPORT int ble_gatt_disconnect(uint16_t conn_handle)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];

    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_disconnect) {
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_disconnect(conn_handle, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }

    ble_hci_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = 0x0406;
    cmd.params[0] = conn_handle & 0xFF;
    cmd.params[1] = (conn_handle >> 8) & 0xFF;
    cmd.params[2] = 0x13;
    cmd.param_len = 3;
    int ret = tp->send_hci_cmd(&cmd);
    return (ret < 0) ? BLE_ERR_IO : BLE_OK;
}

EXPORT int ble_gatt_discover(uint16_t conn_handle)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_discover) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_discover(conn_handle, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

EXPORT int ble_gatt_read(uint16_t conn_handle, uint16_t char_handle)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_read) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_read(conn_handle, char_handle, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

EXPORT int ble_gatt_write(uint16_t conn_handle, uint16_t char_handle,
                          const uint8_t *data, uint16_t data_len)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (!data || data_len == 0) return BLE_ERR_INVAL;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_write) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_write(conn_handle, char_handle,
                                           data, data_len, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

EXPORT int ble_gatt_subscribe(uint16_t conn_handle, uint16_t cccd_handle,
                              bool enable)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (g_ctx.active_profile_idx >= 0) {
        chip_profile_t *cp = g_ctx.profiles[g_ctx.active_profile_idx];
        if (cp->build_gatt_subscribe) {
            transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
            chip_cmd_buf_t cmd;
            int ret = cp->build_gatt_subscribe(conn_handle, cccd_handle,
                                               enable, &cmd);
            if (ret < 0) return BLE_ERR_NOSYS;
            write(tp->get_fd(), cmd.data, cmd.len);
            return BLE_OK;
        }
    }
    return BLE_ERR_NOSYS;
}

/* ══════════════════════════════════════════════════════════════════
 *  PUBLIC API — Raw HCI
 * ══════════════════════════════════════════════════════════════════ */

EXPORT int ble_hci_send(uint16_t opcode, const uint8_t *params,
                        uint8_t param_len)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
    ble_hci_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = opcode;
    cmd.param_len = param_len;
    if (params && param_len > 0)
        memcpy(cmd.params, params, param_len);

    int ret = tp->send_hci_cmd(&cmd);
    return (ret < 0) ? BLE_ERR_NOSYS : BLE_OK;
}

EXPORT int ble_vendor_cmd(uint16_t ocf, const uint8_t *params,
                          uint8_t param_len)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
    int ret = tp->send_vendor_cmd(0x3F, ocf, params, param_len);
    return (ret < 0) ? BLE_ERR_NOSYS : BLE_OK;
}

/* ══════════════════════════════════════════════════════════════════
 *  TI multi_role GATT bridge (NPI GATT frames over UART)
 * ══════════════════════════════════════════════════════════════════ */

/*
 * Send a fully-formed NPI GATT frame verbatim over the active UART transport.
 * Unlike ble_hci_send(), this does NOT add HCI framing — the caller supplies
 * the complete NPI frame (SOF..FCS). Used by the daemon's npi_gatt_handler to
 * push NOTIFY / READ_RSP frames to the multi_role firmware.
 */
EXPORT int ble_ti_gatt_send_raw(const uint8_t *frame, uint16_t len)
{
    if (!g_ctx.initialized) return BLE_ERR_NODEV;
    if (!frame || len == 0) return BLE_ERR_INVAL;

    transport_plugin_t *tp = g_ctx.transports[g_ctx.active_transport_idx];
    if (!tp->send_raw) {
        BLE_LOG_ERR("ble_ti_gatt_send_raw: transport '%s' has no raw send",
                    tp->name ? tp->name : "?");
        return BLE_ERR_NOSYS;
    }
    BLE_LOG_DBG("ble_ti_gatt_send_raw: %u bytes", len);
    int ret = tp->send_raw(frame, len, tp->priv);
    return (ret < 0) ? BLE_ERR_NOSYS : BLE_OK;
}

/*
 * Register a GATT-frame receive callback with the TI chip profile. The daemon's
 * npi_gatt_handler registers here so that phone GATT activity forwarded by the
 * multi_role firmware (NPI GATT frames) is delivered to it. No-op unless the
 * active chip profile is ti_npi.
 */
EXPORT int ble_ti_set_gatt_bridge_cb(void (*cb)(uint8_t cmd1,
                                                const uint8_t *payload,
                                                uint16_t plen, void *ctx),
                                     void *ctx)
{
    extern void ti_npi_set_gatt_rx_cb(
        void (*)(uint8_t, const uint8_t *, uint16_t, void *), void *);
    ti_npi_set_gatt_rx_cb(cb, ctx);
    return BLE_OK;
}
