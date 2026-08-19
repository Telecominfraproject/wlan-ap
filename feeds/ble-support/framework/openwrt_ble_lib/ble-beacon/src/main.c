/**
 * @file main.c
 * @brief ble-beacon — Standalone BLE beacon (iBeacon) application.
 *
 * Communicates with ble-provisiond via ubus (through libble-client).
 * Fire-and-forget style: the daemon maintains beacon state even after
 * this process exits.
 *
 * Usage:
 *   ble-beacon start --uuid UUID [--major N] [--minor N] [--tx-power N]
 *   ble-beacon stop
 *   ble-beacon status
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <ble_client.h>

static void print_usage(void)
{
    fprintf(stderr,
        "ble-beacon — BLE beacon controller (libble-client %s)\n\n"
        "Usage:\n"
        "  ble-beacon start [options]\n"
        "  ble-beacon stop\n"
        "  ble-beacon status\n\n"
        "Start options:\n"
        "  --uuid UUID         iBeacon UUID (default: E2C56DB5-DFFB-48D2-B060-D0F5A71096E0)\n"
        "  --major N           Major value 0-65535 (default: 1)\n"
        "  --minor N           Minor value 0-65535 (default: 1)\n"
        "  --tx-power N        Measured TX power at 1m in dBm (default: -59)\n"
        "  --radio-power N     Actual radio TX power in dBm (default: chip default)\n"
        "                      Mapped to nearest supported level for the active chip.\n"
        "                      CC2652R1: -21 to +5 | nRF52840: -40 to +8 | EFR32: -30 to +20\n"
        "  -h, --help          Show this help\n\n"
        "Examples:\n"
        "  ble-beacon start --uuid E2C56DB5-DFFB-48D2-B060-D0F5A71096E0\n"
        "  ble-beacon start --major 100 --minor 42 --tx-power -65\n"
        "  ble-beacon start --radio-power 4\n"
        "  ble-beacon stop\n"
        "  ble-beacon status\n\n"
        "Note: The daemon maintains beacon state — advertising continues\n"
        "      after this command exits.\n",
        BLE_CLIENT_VERSION);
}

static int cmd_start(int argc, char *argv[])
{
    const char *uuid = "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0";
    uint16_t major = 1;
    uint16_t minor = 1;
    int8_t tx_power = -59;
    int8_t radio_power = 0x7F;  /* BLE_RADIO_POWER_DEFAULT */

    static struct option long_opts[] = {
        { "uuid",        required_argument, NULL, 'u' },
        { "major",       required_argument, NULL, 'M' },
        { "minor",       required_argument, NULL, 'm' },
        { "tx-power",    required_argument, NULL, 't' },
        { "radio-power", required_argument, NULL, 'r' },
        { "help",        no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    optind = 1; /* Reset getopt */
    int opt;
    while ((opt = getopt_long(argc, argv, "u:M:m:t:r:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'u': uuid = optarg; break;
        case 'M': major = (uint16_t)atoi(optarg); break;
        case 'm': minor = (uint16_t)atoi(optarg); break;
        case 't': tx_power = (int8_t)atoi(optarg); break;
        case 'r': radio_power = (int8_t)atoi(optarg); break;
        case 'h': print_usage(); return 0;
        default:  print_usage(); return 1;
        }
    }

    int ret = ble_client_connect();
    if (ret == BLE_CLIENT_ERR_UBUS) {
        fprintf(stderr, "Error: cannot connect to ubus\n");
        return 1;
    }
    if (ret == BLE_CLIENT_ERR_DAEMON) {
        fprintf(stderr, "Error: ble-provisiond not running\n");
        return 1;
    }

    /* Set radio TX power if specified */
    if (radio_power != 0x7F) {
        int actual = ble_client_set_radio_tx_power(radio_power);
        if (actual < -128) {
            fprintf(stderr, "Warning: set_radio_tx_power failed (code %d)\n", actual);
        } else {
            printf("Radio TX power: requested %d dBm, actual %d dBm\n",
                   radio_power, actual);
        }
    }

    ret = ble_client_beacon_start(uuid, major, minor, tx_power);
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: beacon_start failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("iBeacon advertising started:\n");
    printf("  UUID:        %s\n", uuid);
    printf("  Major:       %u\n", major);
    printf("  Minor:       %u\n", minor);
    printf("  TX Power:    %d dBm (measured at 1m)\n", tx_power);
    if (radio_power != 0x7F)
        printf("  Radio Power: %d dBm (hardware)\n", radio_power);
    printf("\nBeacon will continue after this process exits.\n");
    printf("Use 'ble-beacon stop' to stop advertising.\n");

    ble_client_disconnect();
    return 0;
}

static int cmd_stop(void)
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

    ret = ble_client_beacon_stop();
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: beacon_stop failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("Beacon stopped\n");
    ble_client_disconnect();
    return 0;
}

static int cmd_status(void)
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

    char json_buf[1024];
    ret = ble_client_get_status(json_buf, sizeof(json_buf));
    if (ret != BLE_CLIENT_OK) {
        fprintf(stderr, "Error: status query failed (code %d)\n", ret);
        ble_client_disconnect();
        return 1;
    }

    printf("%s\n", json_buf);
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

    if (strcmp(cmd, "start") == 0)
        return cmd_start(argc - 1, &argv[1]);
    else if (strcmp(cmd, "stop") == 0)
        return cmd_stop();
    else if (strcmp(cmd, "status") == 0)
        return cmd_status();
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}
