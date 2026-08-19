/**
 * @file ble_cli.c
 * @brief ble-cli — Standalone BLE command-line tool.
 *
 * Links libble.so directly. Uses its own poll() loop via ble_get_fd()
 * and ble_process(). No daemon or ubus required.
 *
 * Usage:
 *   ble-cli scan [--duration 5000] [--active] [--passive] [--json]
 *   ble-cli beacon start --uuid UUID [--major N] [--minor N]
 *   ble-cli beacon stop
 *   ble-cli gatt connect AA:BB:CC:DD:EE:FF
 *   ble-cli gatt read --handle 42
 *   ble-cli gatt write --handle 42 --value "hello"
 *   ble-cli status
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <getopt.h>

#include <ble.h>

static volatile sig_atomic_t g_running = 1;
static bool json_output = false;

static void sig_handler(int sig) { (void)sig; g_running = 0; }

/* ── Event callback ── */
static void cli_event_cb(const ble_event_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;

    switch (event->type) {
    case BLE_EVT_SCAN_RESULT:
        if (json_output) {
            printf("{\"address\":\"%s\",\"rssi\":%d,\"name\":\"%s\"}\n",
                   event->scan_result.address,
                   event->scan_result.rssi,
                   event->scan_result.name);
        } else {
            printf("  %-17s  RSSI: %4d  %s\n",
                   event->scan_result.address,
                   event->scan_result.rssi,
                   event->scan_result.name);
        }
        fflush(stdout);
        break;

    case BLE_EVT_SCAN_COMPLETE:
        if (json_output)
            printf("{\"event\":\"scan_complete\",\"count\":%u}\n",
                   event->scan_complete.count);
        else
            printf("\nScan complete (%u results)\n", event->scan_complete.count);
        g_running = 0;
        break;

    case BLE_EVT_CONNECT:
        printf("Connected: %s (handle=%u)\n",
               event->connect.address, event->connect.conn_handle);
        break;

    case BLE_EVT_DISCONNECT:
        printf("Disconnected: handle=%u reason=%u\n",
               event->disconnect.conn_handle, event->disconnect.reason);
        g_running = 0;
        break;

    case BLE_EVT_GATT_NOTIFY:
    case BLE_EVT_GATT_READ:
        printf("GATT data: handle=%u len=%u: ",
               event->gatt_data.handle, event->gatt_data.value_len);
        for (uint16_t i = 0; i < event->gatt_data.value_len; i++)
            printf("%02x ", event->gatt_data.value[i]);
        printf("\n");
        break;

    case BLE_EVT_ERROR:
        fprintf(stderr, "Error: code=%u %s\n",
                event->error.code, event->error.message);
        break;

    default:
        break;
    }
}

/* ── Event loop ── */
static void run_event_loop(int timeout_ms)
{
    int fd = ble_get_fd();
    if (fd < 0) {
        fprintf(stderr, "Error: no transport fd available\n");
        return;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int elapsed = 0;
    int poll_interval = 100; /* 100ms poll interval */

    while (g_running) {
        int ret = poll(&pfd, 1, poll_interval);
        if (ret > 0 && (pfd.revents & POLLIN))
            ble_process();
        elapsed += poll_interval;
        if (timeout_ms > 0 && elapsed >= timeout_ms)
            break;
    }
}

/* ── Commands ── */

static int cmd_scan(int argc, char *argv[])
{
    uint32_t duration = 5000;
    bool active = true;
    bool filter_dup = true;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc)
            duration = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--active") == 0)
            active = true;
        else if (strcmp(argv[i], "--passive") == 0)
            active = false;
        else if (strcmp(argv[i], "--json") == 0)
            json_output = true;
        else if (strcmp(argv[i], "--no-filter") == 0)
            filter_dup = false;
    }

    if (!json_output)
        printf("Scanning for %ums (%s, filter_dup=%d)...\n",
               duration, active ? "active" : "passive", filter_dup);

    int ret = ble_scan_start(duration, active, filter_dup);
    if (ret != BLE_OK) {
        fprintf(stderr, "scan_start failed: %s\n", ble_strerror(ret));
        return 1;
    }

    run_event_loop((int)duration + 1000);
    ble_scan_stop();
    return 0;
}

static int cmd_beacon(int argc, char *argv[])
{
    if (argc < 1) {
        fprintf(stderr, "Usage: ble-cli beacon start|stop [options]\n");
        return 1;
    }

    if (strcmp(argv[0], "stop") == 0) {
        int ret = ble_beacon_stop();
        if (ret != BLE_OK) {
            fprintf(stderr, "beacon_stop failed: %s\n", ble_strerror(ret));
            return 1;
        }
        printf("Beacon stopped\n");
        return 0;
    }

    if (strcmp(argv[0], "start") != 0) {
        fprintf(stderr, "Unknown beacon command: %s\n", argv[0]);
        return 1;
    }

    ble_beacon_config_t config;
    memset(&config, 0, sizeof(config));
    snprintf(config.uuid, sizeof(config.uuid), "%.36s", "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0");
    config.major = 1; config.minor = 1;
    config.tx_power = -59; config.interval_ms = 100;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--uuid") == 0 && i + 1 < argc)
            snprintf(config.uuid, sizeof(config.uuid), "%s", argv[++i]);
        else if (strcmp(argv[i], "--major") == 0 && i + 1 < argc)
            config.major = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--minor") == 0 && i + 1 < argc)
            config.minor = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--tx-power") == 0 && i + 1 < argc)
            config.tx_power = (int8_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc)
            config.interval_ms = (uint16_t)atoi(argv[++i]);
    }

    ble_uuid_parse(config.uuid, config.uuid_bytes);
    int ret = ble_beacon_start(&config);
    if (ret != BLE_OK) {
        fprintf(stderr, "beacon_start failed: %s\n", ble_strerror(ret));
        return 1;
    }

    printf("iBeacon advertising: UUID=%s Major=%u Minor=%u TxPwr=%d\n",
           config.uuid, config.major, config.minor, config.tx_power);
    printf("Press Ctrl+C to stop...\n");

    run_event_loop(0); /* Run until signal */
    ble_beacon_stop();
    printf("\nBeacon stopped\n");
    return 0;
}

static int cmd_gatt(int argc, char *argv[])
{
    if (argc < 1) {
        fprintf(stderr, "Usage: ble-cli gatt connect|read|write|disconnect [options]\n");
        return 1;
    }

    if (strcmp(argv[0], "connect") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: ble-cli gatt connect AA:BB:CC:DD:EE:FF\n");
            return 1;
        }
        int ret = ble_gatt_connect(argv[1], 0);
        if (ret != BLE_OK) {
            fprintf(stderr, "gatt_connect failed: %s\n", ble_strerror(ret));
            return 1;
        }
        printf("Connecting to %s...\n", argv[1]);
        run_event_loop(10000);
        return 0;
    }

    if (strcmp(argv[0], "read") == 0) {
        uint16_t handle = 0;
        uint16_t conn = 0;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--handle") == 0 && i + 1 < argc)
                handle = (uint16_t)atoi(argv[++i]);
            else if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
                conn = (uint16_t)atoi(argv[++i]);
        }
        int ret = ble_gatt_read(conn, handle);
        if (ret != BLE_OK) {
            fprintf(stderr, "gatt_read failed: %s\n", ble_strerror(ret));
            return 1;
        }
        run_event_loop(5000);
        return 0;
    }

    if (strcmp(argv[0], "write") == 0) {
        uint16_t handle = 0;
        uint16_t conn = 0;
        const char *value = NULL;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--handle") == 0 && i + 1 < argc)
                handle = (uint16_t)atoi(argv[++i]);
            else if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
                conn = (uint16_t)atoi(argv[++i]);
            else if (strcmp(argv[i], "--value") == 0 && i + 1 < argc)
                value = argv[++i];
        }
        if (!value) {
            fprintf(stderr, "Usage: ble-cli gatt write --handle N --value DATA\n");
            return 1;
        }
        int ret = ble_gatt_write(conn, handle,
                                 (const uint8_t *)value, (uint16_t)strlen(value));
        if (ret != BLE_OK) {
            fprintf(stderr, "gatt_write failed: %s\n", ble_strerror(ret));
            return 1;
        }
        printf("Write sent to handle %u\n", handle);
        run_event_loop(3000);
        return 0;
    }

    if (strcmp(argv[0], "disconnect") == 0) {
        uint16_t conn = 0;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
                conn = (uint16_t)atoi(argv[++i]);
        }
        int ret = ble_gatt_disconnect(conn);
        if (ret != BLE_OK) {
            fprintf(stderr, "gatt_disconnect failed: %s\n", ble_strerror(ret));
            return 1;
        }
        printf("Disconnect requested\n");
        return 0;
    }

    fprintf(stderr, "Unknown gatt command: %s\n", argv[0]);
    return 1;
}

static int cmd_status(void)
{
    printf("Transport:    %s\n", ble_get_transport_name());
    const char *cp = ble_get_chip_profile_name();
    if (cp)
        printf("Chip profile: %s\n", cp);
    printf("Scanning:     %s\n", ble_scan_is_active() ? "yes" : "no");
    printf("Beacon:       %s\n", ble_beacon_is_active() ? "yes" : "no");
    return 0;
}

static void print_usage(void)
{
    fprintf(stderr,
        "ble-cli — BLE command-line tool (libble %d.%d.%d)\n\n"
        "Usage:\n"
        "  ble-cli scan [--duration MS] [--active] [--passive] [--json]\n"
        "  ble-cli beacon start --uuid UUID [--major N] [--minor N] [--tx-power N]\n"
        "  ble-cli beacon stop\n"
        "  ble-cli gatt connect AA:BB:CC:DD:EE:FF\n"
        "  ble-cli gatt read --handle N [--conn N]\n"
        "  ble-cli gatt write --handle N --value DATA [--conn N]\n"
        "  ble-cli gatt disconnect [--conn N]\n"
        "  ble-cli status\n\n"
        "Options:\n"
        "  --transport bluez|uart   Force transport (default: auto)\n"
        "  --device /dev/ttyUSB0    UART device path\n"
        "  --adapter hci0           BlueZ adapter name\n"
        "  -h, --help               Show this help\n",
        LIBBLE_VERSION_MAJOR, LIBBLE_VERSION_MINOR, LIBBLE_VERSION_PATCH);
}

int main(int argc, char *argv[])
{
    if (argc < 2) { print_usage(); return 1; }

    /* Parse global options before subcommand */
    ble_config_t config;
    memset(&config, 0, sizeof(config));
    config.transport = BLE_TRANSPORT_AUTO;
    config.baud_rate = 115200;
    strncpy(config.adapter, "hci0", sizeof(config.adapter) - 1);

    int cmd_idx = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--transport") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "bluez") == 0)
                config.transport = BLE_TRANSPORT_BLUEZ;
            else if (strcmp(argv[i], "uart") == 0)
                config.transport = BLE_TRANSPORT_UART;
            cmd_idx = i + 1;
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            strncpy(config.device_path, argv[++i], sizeof(config.device_path) - 1);
            cmd_idx = i + 1;
        } else if (strcmp(argv[i], "--adapter") == 0 && i + 1 < argc) {
            strncpy(config.adapter, argv[++i], sizeof(config.adapter) - 1);
            cmd_idx = i + 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else {
            cmd_idx = i;
            break;
        }
    }

    if (cmd_idx >= argc) { print_usage(); return 1; }

    /* Signal handlers */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Initialize libble */
    int ret = ble_init(&config);
    if (ret != BLE_OK) {
        fprintf(stderr, "ble_init failed: %s\n", ble_strerror(ret));
        return 1;
    }

    ble_subscribe(cli_event_cb, NULL);

    /* Dispatch subcommand */
    const char *cmd = argv[cmd_idx];
    int rc = 0;

    if (strcmp(cmd, "scan") == 0)
        rc = cmd_scan(argc - cmd_idx - 1, &argv[cmd_idx + 1]);
    else if (strcmp(cmd, "beacon") == 0)
        rc = cmd_beacon(argc - cmd_idx - 1, &argv[cmd_idx + 1]);
    else if (strcmp(cmd, "gatt") == 0)
        rc = cmd_gatt(argc - cmd_idx - 1, &argv[cmd_idx + 1]);
    else if (strcmp(cmd, "status") == 0)
        rc = cmd_status();
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        rc = 1;
    }

    ble_deinit();
    return rc;
}
