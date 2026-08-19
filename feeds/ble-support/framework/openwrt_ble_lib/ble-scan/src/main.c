/**
 * @file main.c
 * @brief ble-scan — Standalone BLE scanner application.
 *
 * Communicates with ble-provisiond via ubus (through libble-client).
 * Multiple instances can run simultaneously — the daemon broadcasts
 * scan results to all subscribers.
 *
 * Usage:
 *   ble-scan [--duration 5000] [--active] [--passive] [--json] [--count N]
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
static bool json_output = false;
static uint32_t max_count = 0;
static uint32_t result_count = 0;

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void scan_result_cb(const ble_client_scan_result_t *result, void *ctx)
{
    (void)ctx;
    if (!result) return;

    if (json_output) {
        printf("{\"address\":\"%s\",\"rssi\":%d,\"name\":\"%s\","
               "\"address_type\":\"%s\",\"connectable\":%s}\n",
               result->address, result->rssi, result->name,
               result->address_type,
               result->connectable ? "true" : "false");
    } else {
        printf("  %-17s  RSSI: %4d  %-6s  %s\n",
               result->address, result->rssi,
               result->address_type, result->name);
    }
    fflush(stdout);

    result_count++;
    if (max_count > 0 && result_count >= max_count)
        g_running = 0;
}

static void print_usage(void)
{
    fprintf(stderr,
        "ble-scan — BLE scanner (libble-client %s)\n\n"
        "Usage:\n"
        "  ble-scan [options]\n\n"
        "Options:\n"
        "  --duration MS   Scan duration in milliseconds (default: 5000)\n"
        "  --active        Active scan (send scan requests)\n"
        "  --passive       Passive scan (listen only)\n"
        "  --no-filter     Do not filter duplicates\n"
        "  --json          Output in JSON format (one object per line)\n"
        "  --count N       Stop after N results\n"
        "  -h, --help      Show this help\n\n"
        "Examples:\n"
        "  ble-scan --duration 10000 --active\n"
        "  ble-scan --json --count 5\n"
        "  ble-scan --passive --duration 0    # scan indefinitely\n",
        BLE_CLIENT_VERSION);
}

int main(int argc, char *argv[])
{
    uint32_t duration = 5000;
    bool active = true;
    bool filter_dup = true;

    static struct option long_opts[] = {
        { "duration",  required_argument, NULL, 'd' },
        { "active",    no_argument,       NULL, 'a' },
        { "passive",   no_argument,       NULL, 'p' },
        { "no-filter", no_argument,       NULL, 'f' },
        { "json",      no_argument,       NULL, 'j' },
        { "count",     required_argument, NULL, 'c' },
        { "help",      no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "d:apfjc:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'd': duration = (uint32_t)atoi(optarg); break;
        case 'a': active = true; break;
        case 'p': active = false; break;
        case 'f': filter_dup = false; break;
        case 'j': json_output = true; break;
        case 'c': max_count = (uint32_t)atoi(optarg); break;
        case 'h': print_usage(); return 0;
        default:  print_usage(); return 1;
        }
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Connect to daemon */
    int ret = ble_client_connect();
    if (ret == BLE_CLIENT_ERR_UBUS) {
        fprintf(stderr, "Error: cannot connect to ubus\n");
        return 1;
    }
    if (ret == BLE_CLIENT_ERR_DAEMON) {
        fprintf(stderr, "Error: ble-provisiond not running\n");
        return 1;
    }

    /* Subscribe to scan events */
    ret = ble_client_scan_subscribe(scan_result_cb, NULL);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: failed to subscribe to scan events\n");
        ble_client_disconnect();
        return 1;
    }

    /* Start scan */
    ret = ble_client_scan_start(duration, active, filter_dup);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: scan_start failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    if (!json_output) {
        printf("Scanning for %ums (%s, filter_dup=%d)...\n",
               duration, active ? "active" : "passive", filter_dup);
        printf("  %-17s  %-10s  %-6s  %s\n",
               "ADDRESS", "RSSI", "TYPE", "NAME");
        printf("  %-17s  %-10s  %-6s  %s\n",
               "-----------------", "----", "------", "----");
    }

    /* Event loop — poll on ubus fd */
    int fd = ble_client_get_fd();
    if (fd < 0) {
        fprintf(stderr, "Error: no ubus fd available\n");
        ble_client_disconnect();
        return 1;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int elapsed = 0;
    int poll_interval = 100;

    while (g_running) {
        int pret = poll(&pfd, 1, poll_interval);
        if (pret > 0 && (pfd.revents & POLLIN))
            ble_client_process();

        elapsed += poll_interval;
        /* If duration > 0, exit after time elapsed (with margin) */
        if (duration > 0 && (uint32_t)elapsed >= duration + 500)
            break;
    }

    /* Cleanup */
    if (!json_output)
        printf("\nScan complete (%u results)\n", result_count);

    ble_client_scan_stop();
    ble_client_disconnect();
    return 0;
}
