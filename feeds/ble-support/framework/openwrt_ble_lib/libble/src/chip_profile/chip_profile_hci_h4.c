/**
 * @file chip_profile_hci_h4.c
 * @brief Chip profile for standard BLE HCI H4 transport.
 *
 * Supports standard Bluetooth HCI controllers (nRF52840/Zephyr, ESP32).
 * Wire format: [0x01][opcode_lo][opcode_hi][param_len][params...]
 */

#define _GNU_SOURCE

#include <string.h>
#include <errno.h>

#include "chip_profile.h"
#include "../log.h"

#define H4_CMD_PKT      0x01
#define H4_EVT_PKT      0x04

#define HCI_OP_RESET                0x0C03
#define HCI_OP_LE_SET_SCAN_PARAMS   0x200B
#define HCI_OP_LE_SET_SCAN_ENABLE   0x200C
#define HCI_OP_LE_SET_ADV_PARAMS    0x2006
#define HCI_OP_LE_SET_ADV_DATA      0x2008
#define HCI_OP_LE_SET_ADV_ENABLE    0x200A

#define HCI_EVT_CMD_COMPLETE        0x0E
#define HCI_EVT_LE_META             0x3E
#define HCI_LE_ADV_REPORT           0x02

static void build_h4_cmd(uint16_t opcode, const uint8_t *params,
                         uint8_t param_len, chip_cmd_buf_t *out)
{
    out->data[0] = H4_CMD_PKT;
    out->data[1] = (uint8_t)(opcode & 0xFF);
    out->data[2] = (uint8_t)(opcode >> 8);
    out->data[3] = param_len;
    if (param_len > 0 && params)
        memcpy(&out->data[4], params, param_len);
    out->len = 4 + param_len;
}

static int h4_init(int (*send_fn)(const uint8_t *, uint16_t, void *), void *ctx)
{
    chip_cmd_buf_t cmd;
    build_h4_cmd(HCI_OP_RESET, NULL, 0, &cmd);
    return send_fn(cmd.data, cmd.len, ctx);
}

static int h4_build_scan_start(uint32_t duration_ms, bool active,
                               bool filter_dup, chip_cmd_buf_t *out)
{
    (void)duration_ms;
    if (!out) return -EINVAL;

    uint8_t params[7] = {
        active ? 0x01 : 0x00,
        0x60, 0x00, 0x30, 0x00, 0x00, 0x00
    };
    build_h4_cmd(HCI_OP_LE_SET_SCAN_PARAMS, params, 7, out);
    uint16_t first_len = out->len;

    uint8_t enable[2] = { 0x01, filter_dup ? 0x01 : 0x00 };
    out->data[first_len] = H4_CMD_PKT;
    out->data[first_len + 1] = (uint8_t)(HCI_OP_LE_SET_SCAN_ENABLE & 0xFF);
    out->data[first_len + 2] = (uint8_t)(HCI_OP_LE_SET_SCAN_ENABLE >> 8);
    out->data[first_len + 3] = 2;
    memcpy(&out->data[first_len + 4], enable, 2);
    out->len = first_len + 6;
    return 0;
}

static int h4_build_scan_stop(chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    uint8_t params[2] = { 0x00, 0x00 };
    build_h4_cmd(HCI_OP_LE_SET_SCAN_ENABLE, params, 2, out);
    return 0;
}

static int h4_build_beacon_start(const ble_beacon_config_t *config,
                                 chip_cmd_buf_t *out)
{
    if (!config || !out) return -EINVAL;

    /* Adv params */
    uint8_t adv_params[15] = {0};
    adv_params[0] = 0xA0; adv_params[1] = 0x00;
    adv_params[2] = 0xA0; adv_params[3] = 0x00;
    adv_params[4] = 0x03; /* ADV_NONCONN_IND */
    adv_params[13] = 0x07;
    build_h4_cmd(HCI_OP_LE_SET_ADV_PARAMS, adv_params, 15, out);
    uint16_t offset = out->len;

    /* Adv data */
    uint8_t adv_data[32] = {0};
    uint8_t pos = 0;
    adv_data[1] = 0x02; adv_data[2] = 0x01; adv_data[3] = 0x06;
    pos = 3;
    /* iBeacon payload */
    uint8_t payload[25];
    payload[0] = 0x1A; payload[1] = 0xFF;
    payload[2] = 0x4C; payload[3] = 0x00;
    payload[4] = 0x02; payload[5] = 0x15;
    memcpy(&payload[6], config->uuid_bytes, 16);
    payload[22] = (config->major >> 8) & 0xFF;
    payload[23] = config->major & 0xFF;
    payload[24] = (config->minor >> 8) & 0xFF;
    /* We need 2 more bytes: minor_lo, tx_power */
    memcpy(&adv_data[pos + 1], payload, 25);
    adv_data[pos + 26] = config->minor & 0xFF;
    adv_data[pos + 27] = (uint8_t)config->tx_power;
    adv_data[0] = pos + 27; /* total adv data length */

    out->data[offset] = H4_CMD_PKT;
    out->data[offset + 1] = (uint8_t)(HCI_OP_LE_SET_ADV_DATA & 0xFF);
    out->data[offset + 2] = (uint8_t)(HCI_OP_LE_SET_ADV_DATA >> 8);
    out->data[offset + 3] = 32;
    memcpy(&out->data[offset + 4], adv_data, 32);
    offset += 36;

    /* Enable */
    uint8_t enable = 0x01;
    out->data[offset] = H4_CMD_PKT;
    out->data[offset + 1] = (uint8_t)(HCI_OP_LE_SET_ADV_ENABLE & 0xFF);
    out->data[offset + 2] = (uint8_t)(HCI_OP_LE_SET_ADV_ENABLE >> 8);
    out->data[offset + 3] = 1;
    out->data[offset + 4] = enable;
    out->len = offset + 5;
    return 0;
}

static int h4_build_beacon_stop(chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    uint8_t disable = 0x00;
    build_h4_cmd(HCI_OP_LE_SET_ADV_ENABLE, &disable, 1, out);
    return 0;
}

static int h4_build_hci_cmd(const ble_hci_cmd_t *cmd, chip_cmd_buf_t *out)
{
    if (!cmd || !out) return -EINVAL;
    build_h4_cmd(cmd->opcode, cmd->params, cmd->param_len, out);
    return 0;
}

static int h4_build_vendor_cmd(uint8_t ogf, uint16_t ocf,
                               const uint8_t *params, uint8_t param_len,
                               chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    uint16_t opcode = ((uint16_t)ogf << 10) | ocf;
    build_h4_cmd(opcode, params, param_len, out);
    return 0;
}

static int h4_parse_rx(const uint8_t *data, uint16_t len,
                       void (*event_cb)(const ble_event_t *, void *), void *ctx)
{
    int events = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] == H4_EVT_PKT && i + 2 < len) {
            uint8_t evt_code = data[i + 1];
            uint8_t param_len = data[i + 2];
            if (i + 3 + param_len > len) break;

            if (evt_code == HCI_EVT_LE_META && param_len >= 2 &&
                data[i + 3] == HCI_LE_ADV_REPORT) {
                ble_event_t event;
                memset(&event, 0, sizeof(event));
                event.type = BLE_EVT_SCAN_RESULT;
                if (param_len >= 10) {
                    const uint8_t *rpt = &data[i + 3 + 2];
                    snprintf(event.scan_result.address, BLE_ADDR_STR_LEN,
                        "%02X:%02X:%02X:%02X:%02X:%02X",
                        rpt[7], rpt[6], rpt[5], rpt[4], rpt[3], rpt[2]);
                    event.scan_result.rssi = (int8_t)data[i + 2 + param_len];
                }
                if (event_cb) event_cb(&event, ctx);
                events++;
            }
            i += 2 + param_len;
        }
    }
    return events;
}

/* ── Radio TX Power (nRF52840/nRF54L15 via Zephyr VS command) ── */

/* Zephyr vendor-specific: Set TX Power (opcode 0xFC0E) */
#define HCI_VS_SET_TX_POWER  0xFC0E

static int h4_set_radio_tx_power(int8_t power_dbm, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    /*
     * Zephyr VS Set TX Power command:
     *   Handle type (1 byte): 0x00 = ADV
     *   Handle (2 bytes): 0x0000
     *   TX power (1 byte, signed)
     */
    uint8_t params[4] = {
        0x00,                       /* handle_type = ADV */
        0x00, 0x00,                 /* handle = 0 */
        (uint8_t)power_dbm          /* tx_power */
    };
    build_h4_cmd(HCI_VS_SET_TX_POWER, params, 4, out);
    return 0;
}

static int h4_get_radio_power_range(int8_t *min_dbm, int8_t *max_dbm)
{
    if (!min_dbm || !max_dbm) return -EINVAL;
    *min_dbm = -40;
    *max_dbm = 8;
    return 0;
}

static int h4_probe(int (*send_fn)(const uint8_t *, uint16_t, void *),
                    int (*recv_fn)(uint8_t *, uint16_t, uint32_t, void *),
                    void *ctx)
{
    uint8_t reset[] = { H4_CMD_PKT, 0x03, 0x0C, 0x00 };
    if (send_fn(reset, 4, ctx) < 0) return 0;
    uint8_t buf[16];
    int ret = recv_fn(buf, sizeof(buf), 1000, ctx);
    if (ret >= 4 && buf[0] == H4_EVT_PKT && buf[1] == HCI_EVT_CMD_COMPLETE)
        return 1;
    return 0;
}

chip_profile_t chip_profile_hci_h4 = {
    .name              = "hci_h4",
    .init              = h4_init,
    .build_scan_start  = h4_build_scan_start,
    .build_scan_stop   = h4_build_scan_stop,
    .build_beacon_start = h4_build_beacon_start,
    .build_beacon_stop = h4_build_beacon_stop,
    .build_gatt_connect = NULL,
    .build_gatt_disconnect = NULL,
    .build_gatt_discover = NULL,
    .build_gatt_read   = NULL,
    .build_gatt_write  = NULL,
    .build_gatt_subscribe = NULL,
    .build_hci_cmd     = h4_build_hci_cmd,
    .build_vendor_cmd  = h4_build_vendor_cmd,
    .set_radio_tx_power = h4_set_radio_tx_power,
    .get_radio_power_range = h4_get_radio_power_range,
    .parse_rx          = h4_parse_rx,
    .probe             = h4_probe,
};
