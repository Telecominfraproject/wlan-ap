/**
 * @file main.c
 * @brief ble-gatt — Standalone BLE GATT client application.
 *
 * Communicates with ble-provisiond via ubus (through libble-client).
 * Supports connect, discover, read, write, subscribe, and disconnect.
 *
 * Usage:
 *   ble-gatt connect AA:BB:CC:DD:EE:FF
 *   ble-gatt discover --conn N
 *   ble-gatt read --conn N --handle N
 *   ble-gatt write --conn N --handle N --value "data"
 *   ble-gatt subscribe --conn N --cccd N
 *   ble-gatt disconnect --conn N
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <getopt.h>

#include <ble_client.h>

static volatile sig_atomic_t g_running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void gatt_notify_cb(const ble_client_gatt_data_t *data, void *ctx)
{
    (void)ctx;
    if (!data) return;

    printf("Notification: conn=%u handle=%u len=%u data=",
           data->conn_handle, data->char_handle, data->value_len);
    for (uint16_t i = 0; i < data->value_len; i++)
        printf("%02x", data->value[i]);
    printf("\n");
    fflush(stdout);
}

static void print_usage(void)
{
    fprintf(stderr,
        "ble-gatt — BLE GATT client (libble-client %s)\n\n"
        "Usage:\n"
        "  ble-gatt connect <address>\n"
        "  ble-gatt discover --conn N\n"
        "  ble-gatt read --conn N --handle N\n"
        "  ble-gatt write --conn N --handle N --value DATA\n"
        "  ble-gatt subscribe --conn N --cccd N\n"
        "  ble-gatt disconnect --conn N\n\n"
        "Options:\n"
        "  --conn N      Connection handle (from connect response)\n"
        "  --handle N    Characteristic handle\n"
        "  --cccd N      CCCD handle for notifications\n"
        "  --value DATA  Data to write (string or hex with 0x prefix)\n"
        "  --timeout MS  Operation timeout in ms (default: 10000)\n"
        "  -h, --help    Show this help\n\n"
        "Examples:\n"
        "  ble-gatt connect AA:BB:CC:DD:EE:FF\n"
        "  ble-gatt read --conn 1 --handle 42\n"
        "  ble-gatt write --conn 1 --handle 42 --value \"hello\"\n"
        "  ble-gatt write --conn 1 --handle 42 --value 0x48656c6c6f\n"
        "  ble-gatt subscribe --conn 1 --cccd 43   (keeps running for notifications)\n"
        "  ble-gatt disconnect --conn 1\n",
        BLE_CLIENT_VERSION);
}

static int connect_to_daemon(void)
{
    int ret = ble_client_connect();
    if (ret == BLE_CLIENT_ERR_UBUS) {
        fprintf(stderr, "Error: cannot connect to ubus\n");
        return 1;
    }
    if (ret == BLE_CLIENT_ERR_DAEMON) {
        fprintf(stderr, "Error: ble-provisiond not running\n");
        return 1;
    }
    return 0;
}

/* Parse --value: if starts with "0x", treat as hex; otherwise raw string */
static int parse_value(const char *input, uint8_t *buf, uint16_t buf_size,
                       uint16_t *out_len)
{
    if (!input || !buf || !out_len) return -1;

    if (strncmp(input, "0x", 2) == 0 || strncmp(input, "0X", 2) == 0) {
        /* Hex decode */
        const char *hex = input + 2;
        size_t hex_len = strlen(hex);
        if (hex_len % 2 != 0) {
            fprintf(stderr, "Error: hex value must have even number of digits\n");
            return -1;
        }
        uint16_t byte_len = (uint16_t)(hex_len / 2);
        if (byte_len > buf_size) byte_len = buf_size;
        for (uint16_t i = 0; i < byte_len; i++) {
            unsigned int byte;
            if (sscanf(&hex[i * 2], "%2x", &byte) != 1) {
                fprintf(stderr, "Error: invalid hex at position %u\n", i * 2);
                return -1;
            }
            buf[i] = (uint8_t)byte;
        }
        *out_len = byte_len;
    } else {
        /* Raw string */
        size_t len = strlen(input);
        if (len > buf_size) len = buf_size;
        memcpy(buf, input, len);
        *out_len = (uint16_t)len;
    }
    return 0;
}

static int cmd_connect(int argc, char *argv[])
{
    if (argc < 1) {
        fprintf(stderr, "Usage: ble-gatt connect <address>\n");
        return 1;
    }

    const char *address = argv[0];

    if (connect_to_daemon()) return 1;

    int ret = ble_client_gatt_connect(address);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: gatt_connect failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("Connecting to %s...\n", address);
    printf("Connection request sent. Use 'ble-gatt read/write' with --conn handle.\n");
    ble_client_disconnect();
    return 0;
}

static int cmd_discover(int argc, char *argv[])
{
    uint16_t conn = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
            conn = (uint16_t)atoi(argv[++i]);
    }

    if (connect_to_daemon()) return 1;

    /* Discover is a status query — get all services/chars for a connection */
    char json_buf[4096];
    int ret = ble_client_get_status(json_buf, sizeof(json_buf));
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: discover failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("Connection %u services:\n%s\n", conn, json_buf);
    ble_client_disconnect();
    return 0;
}

static int cmd_read(int argc, char *argv[])
{
    uint16_t conn = 0;
    uint16_t handle = 0;
    int timeout = 5000;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
            conn = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--handle") == 0 && i + 1 < argc)
            handle = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
            timeout = atoi(argv[++i]);
    }

    if (handle == 0) {
        fprintf(stderr, "Error: --handle is required\n");
        return 1;
    }

    if (connect_to_daemon()) return 1;

    /* Subscribe to notifications to receive the read response */
    ble_client_gatt_on_notify(gatt_notify_cb, NULL);

    int ret = ble_client_gatt_read(conn, handle);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: gatt_read failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("Read request sent (conn=%u, handle=%u). Waiting for response...\n",
           conn, handle);

    /* Brief event loop to get response */
    int fd = ble_client_get_fd();
    if (fd >= 0) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int elapsed = 0;
        while (g_running && elapsed < timeout) {
            int pret = poll(&pfd, 1, 100);
            if (pret > 0 && (pfd.revents & POLLIN))
                ble_client_process();
            elapsed += 100;
        }
    }

    ble_client_disconnect();
    return 0;
}

static int cmd_write(int argc, char *argv[])
{
    uint16_t conn = 0;
    uint16_t handle = 0;
    const char *value_str = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
            conn = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--handle") == 0 && i + 1 < argc)
            handle = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--value") == 0 && i + 1 < argc)
            value_str = argv[++i];
    }

    if (handle == 0) {
        fprintf(stderr, "Error: --handle is required\n");
        return 1;
    }
    if (!value_str) {
        fprintf(stderr, "Error: --value is required\n");
        return 1;
    }

    uint8_t data[512];
    uint16_t data_len = 0;
    if (parse_value(value_str, data, sizeof(data), &data_len) != 0)
        return 1;

    if (connect_to_daemon()) return 1;

    int ret = ble_client_gatt_write(conn, handle, data, data_len);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: gatt_write failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("Write sent: conn=%u handle=%u len=%u\n", conn, handle, data_len);
    ble_client_disconnect();
    return 0;
}

static int cmd_subscribe(int argc, char *argv[])
{
    uint16_t conn = 0;
    uint16_t cccd = 0;
    int timeout = 0; /* 0 = indefinite */

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
            conn = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--cccd") == 0 && i + 1 < argc)
            cccd = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
            timeout = atoi(argv[++i]);
    }

    if (cccd == 0) {
        fprintf(stderr, "Error: --cccd is required\n");
        return 1;
    }

    if (connect_to_daemon()) return 1;

    /* Register notification callback */
    ble_client_gatt_on_notify(gatt_notify_cb, NULL);

    /* Enable notifications via CCCD write */
    int ret = ble_client_gatt_subscribe(conn, cccd, true);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: gatt_subscribe failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("Subscribed to notifications (conn=%u, cccd=%u)\n", conn, cccd);
    printf("Waiting for notifications... (Ctrl+C to stop)\n");

    /* Event loop — run until signal or timeout */
    int fd = ble_client_get_fd();
    if (fd < 0) {
        fprintf(stderr, "Error: no ubus fd available\n");
        ble_client_disconnect();
        return 1;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int elapsed = 0;

    while (g_running) {
        int pret = poll(&pfd, 1, 100);
        if (pret > 0 && (pfd.revents & POLLIN))
            ble_client_process();
        elapsed += 100;
        if (timeout > 0 && elapsed >= timeout)
            break;
    }

    /* Unsubscribe */
    ble_client_gatt_subscribe(conn, cccd, false);
    printf("\nUnsubscribed from notifications\n");
    ble_client_disconnect();
    return 0;
}

static int cmd_disconnect(int argc, char *argv[])
{
    uint16_t conn = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--conn") == 0 && i + 1 < argc)
            conn = (uint16_t)atoi(argv[++i]);
    }

    if (connect_to_daemon()) return 1;

    int ret = ble_client_gatt_disconnect(conn);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: gatt_disconnect failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("Disconnect requested (conn=%u)\n", conn);
    ble_client_disconnect();
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage();
        return 0;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    int sub_argc = argc - 2;
    char **sub_argv = &argv[2];

    if (strcmp(cmd, "connect") == 0)
        return cmd_connect(sub_argc, sub_argv);
    else if (strcmp(cmd, "discover") == 0)
        return cmd_discover(sub_argc, sub_argv);
    else if (strcmp(cmd, "read") == 0)
        return cmd_read(sub_argc, sub_argv);
    else if (strcmp(cmd, "write") == 0)
        return cmd_write(sub_argc, sub_argv);
    else if (strcmp(cmd, "subscribe") == 0)
        return cmd_subscribe(sub_argc, sub_argv);
    else if (strcmp(cmd, "disconnect") == 0)
        return cmd_disconnect(sub_argc, sub_argv);
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}
