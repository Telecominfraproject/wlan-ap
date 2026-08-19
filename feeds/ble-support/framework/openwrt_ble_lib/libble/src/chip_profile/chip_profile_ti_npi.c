/**
 * @file chip_profile_ti_npi.c
 * @brief Chip profile for TI CC2652R NPI (Network Processor Interface).
 *
 * NPI Frame: [SOF=0xFE][LEN_LO][LEN_HI][CMD0][CMD1][DATA...][FCS]
 * CMD0 = (subsystem << 5) | msg_type, FCS = XOR of all after SOF
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "chip_profile.h"
#include "../log.h"

#define NPI_SOF         0xFE
#define NPI_MSG_SREQ    0x02
#define NPI_MSG_AREQ    0x03
#define NPI_SS_SYS      0x01
#define NPI_SS_GAP      0x04

#define NPI_GAP_DEVICE_INIT         0x00
#define NPI_GAP_DEVICE_DISC_REQ     0x04
#define NPI_GAP_DEVICE_DISC_CANCEL  0x05
#define NPI_GAP_MAKE_DISCOVERABLE   0x06
#define NPI_GAP_END_DISC            0x08
#define NPI_GAP_DEVICE_INFO_EVT     0x0D
#define NPI_GAP_DEVICE_DISC_EVT     0x0E
#define NPI_SYS_PING               0x01

typedef enum {
    NPI_RX_SOF, NPI_RX_LEN_LO, NPI_RX_LEN_HI,
    NPI_RX_DATA, NPI_RX_FCS
} npi_rx_state_t;

static npi_rx_state_t s_rx_state = NPI_RX_SOF;
static uint8_t s_rx_buf[512];
static uint16_t s_rx_len = 0;
static uint16_t s_rx_expected = 0;

static uint8_t npi_fcs(const uint8_t *data, uint16_t len)
{
    uint8_t fcs = 0;
    for (uint16_t i = 0; i < len; i++) fcs ^= data[i];
    return fcs;
}

static int npi_build_frame(uint8_t subsystem, uint8_t msg_type,
                           uint8_t cmd_id, const uint8_t *payload,
                           uint16_t payload_len, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    uint16_t data_len = payload_len + 2;
    uint8_t cmd0 = (subsystem << 5) | (msg_type & 0x07);
    uint16_t idx = 0;

    out->data[idx++] = NPI_SOF;
    out->data[idx++] = (uint8_t)(data_len & 0xFF);
    out->data[idx++] = (uint8_t)(data_len >> 8);
    out->data[idx++] = cmd0;
    out->data[idx++] = cmd_id;
    if (payload_len > 0 && payload)
        memcpy(&out->data[idx], payload, payload_len);
    idx += payload_len;
    out->data[idx] = npi_fcs(&out->data[1], idx - 1);
    idx++;
    out->len = idx;
    return 0;
}

static int npi_init(int (*send_fn)(const uint8_t *, uint16_t, void *), void *ctx)
{
    uint8_t params[] = { 0x07, 0x05 };
    chip_cmd_buf_t cmd;
    int ret = npi_build_frame(NPI_SS_GAP, NPI_MSG_SREQ,
                              NPI_GAP_DEVICE_INIT, params, 2, &cmd);
    if (ret < 0) return ret;
    s_rx_state = NPI_RX_SOF;
    s_rx_len = 0;
    return send_fn(cmd.data, cmd.len, ctx);
}

static int npi_build_scan_start(uint32_t duration_ms, bool active,
                                bool filter_dup, chip_cmd_buf_t *out)
{
    (void)duration_ms; (void)filter_dup;
    uint8_t params[] = { 0x03, active ? 0x01 : 0x00, 0x00 };
    return npi_build_frame(NPI_SS_GAP, NPI_MSG_SREQ,
                           NPI_GAP_DEVICE_DISC_REQ, params, 3, out);
}

static int npi_build_scan_stop(chip_cmd_buf_t *out)
{
    return npi_build_frame(NPI_SS_GAP, NPI_MSG_SREQ,
                           NPI_GAP_DEVICE_DISC_CANCEL, NULL, 0, out);
}

static int npi_build_beacon_start(const ble_beacon_config_t *config,
                                  chip_cmd_buf_t *out)
{
    if (!config || !out) return -EINVAL;
    uint8_t params[10] = { 0x03, 0x00, 0,0,0,0,0,0, 0x07, 0x00 };
    return npi_build_frame(NPI_SS_GAP, NPI_MSG_SREQ,
                           NPI_GAP_MAKE_DISCOVERABLE, params, 10, out);
}

static int npi_build_beacon_stop(chip_cmd_buf_t *out)
{
    return npi_build_frame(NPI_SS_GAP, NPI_MSG_SREQ,
                           NPI_GAP_END_DISC, NULL, 0, out);
}

static int npi_build_hci_cmd(const ble_hci_cmd_t *cmd, chip_cmd_buf_t *out)
{
    (void)cmd; (void)out;
    return -ENOSYS;
}

static int npi_build_vendor_cmd(uint8_t ogf, uint16_t ocf,
                                const uint8_t *params, uint8_t param_len,
                                chip_cmd_buf_t *out)
{
    (void)ogf;
    uint8_t payload[256];
    payload[0] = (uint8_t)(ocf & 0xFF);
    payload[1] = (uint8_t)(ocf >> 8);
    if (param_len > 0 && params)
        memcpy(&payload[2], params, param_len);
    return npi_build_frame(0x06, NPI_MSG_SREQ, (uint8_t)(ocf & 0xFF),
                           payload, param_len + 2, out);
}

static int npi_parse_rx(const uint8_t *data, uint16_t len,
                        void (*event_cb)(const ble_event_t *, void *), void *ctx)
{
    int events = 0;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        switch (s_rx_state) {
        case NPI_RX_SOF:
            if (byte == NPI_SOF) { s_rx_state = NPI_RX_LEN_LO; s_rx_len = 0; }
            break;
        case NPI_RX_LEN_LO:
            s_rx_expected = byte; s_rx_state = NPI_RX_LEN_HI; break;
        case NPI_RX_LEN_HI:
            s_rx_expected |= ((uint16_t)byte << 8);
            s_rx_state = (s_rx_expected > 0) ? NPI_RX_DATA : NPI_RX_FCS;
            s_rx_len = 0;
            break;
        case NPI_RX_DATA:
            if (s_rx_len < sizeof(s_rx_buf))
                s_rx_buf[s_rx_len++] = byte;
            if (s_rx_len >= s_rx_expected) s_rx_state = NPI_RX_FCS;
            break;
        case NPI_RX_FCS:
            s_rx_state = NPI_RX_SOF;
            if (s_rx_len >= 2) {
                uint8_t subsystem = (s_rx_buf[0] >> 5) & 0x07;
                uint8_t cmd_id = s_rx_buf[1];
                if (subsystem == NPI_SS_GAP && cmd_id == NPI_GAP_DEVICE_INFO_EVT) {
                    ble_event_t event;
                    memset(&event, 0, sizeof(event));
                    event.type = BLE_EVT_SCAN_RESULT;
                    if (s_rx_len >= 12) {
                        uint8_t *p = &s_rx_buf[2];
                        snprintf(event.scan_result.address, BLE_ADDR_STR_LEN,
                            "%02X:%02X:%02X:%02X:%02X:%02X",
                            p[7], p[6], p[5], p[4], p[3], p[2]);
                        event.scan_result.rssi = (int8_t)p[8];
                    }
                    if (event_cb) event_cb(&event, ctx);
                    events++;
                } else if (subsystem == NPI_SS_GAP &&
                           cmd_id == NPI_GAP_DEVICE_DISC_EVT) {
                    ble_event_t event;
                    memset(&event, 0, sizeof(event));
                    event.type = BLE_EVT_SCAN_COMPLETE;
                    event.scan_complete.count = (s_rx_len >= 3) ? s_rx_buf[2] : 0;
                    if (event_cb) event_cb(&event, ctx);
                    events++;
                }
            }
            break;
        }
    }
    return events;
}

/* ── Radio TX Power (CC2652R1 via HCI_EXT_SetTxPowerCmd) ── */

/*
 * CC2652R1 TX power levels and their NPI power index values:
 * Index:  0=-21dBm, 1=-18dBm, 2=-15dBm, 3=-12dBm, 4=-9dBm,
 *         5=-6dBm, 6=-3dBm, 7=0dBm, 8=+1dBm, 9=+2dBm,
 *        10=+3dBm, 11=+4dBm, 12=+5dBm
 */
static const int8_t cc2652_power_levels[] = {
    -21, -18, -15, -12, -9, -6, -3, 0, 1, 2, 3, 4, 5
};
#define CC2652_LEVEL_COUNT  (int)(sizeof(cc2652_power_levels)/sizeof(cc2652_power_levels[0]))

/* TI HCI Extension: Set TX Power */
#define NPI_HCI_EXT_SS          0x06
#define NPI_HCI_EXT_SET_TX_PWR  0x01

static uint8_t cc2652_power_to_index(int8_t power_dbm)
{
    int best = 0;
    int min_diff = abs((int)power_dbm - (int)cc2652_power_levels[0]);
    for (int i = 1; i < CC2652_LEVEL_COUNT; i++) {
        int diff = abs((int)power_dbm - (int)cc2652_power_levels[i]);
        if (diff < min_diff) {
            min_diff = diff;
            best = i;
        }
    }
    return (uint8_t)best;
}

static int npi_set_radio_tx_power(int8_t power_dbm, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    uint8_t idx = cc2652_power_to_index(power_dbm);
    uint8_t payload[1] = { idx };
    return npi_build_frame(NPI_HCI_EXT_SS, NPI_MSG_SREQ,
                           NPI_HCI_EXT_SET_TX_PWR, payload, 1, out);
}

static int npi_get_radio_power_range(int8_t *min_dbm, int8_t *max_dbm)
{
    if (!min_dbm || !max_dbm) return -EINVAL;
    *min_dbm = -21;
    *max_dbm = 5;
    return 0;
}

static int npi_probe(int (*send_fn)(const uint8_t *, uint16_t, void *),
                     int (*recv_fn)(uint8_t *, uint16_t, uint32_t, void *),
                     void *ctx)
{
    chip_cmd_buf_t cmd;
    npi_build_frame(NPI_SS_SYS, NPI_MSG_SREQ, NPI_SYS_PING, NULL, 0, &cmd);
    if (send_fn(cmd.data, cmd.len, ctx) < 0) return 0;
    uint8_t buf[32];
    int ret = recv_fn(buf, sizeof(buf), 500, ctx);
    if (ret >= 3 && buf[0] == NPI_SOF) return 1;
    return 0;
}

chip_profile_t chip_profile_ti_npi = {
    .name              = "ti_npi",
    .init              = npi_init,
    .build_scan_start  = npi_build_scan_start,
    .build_scan_stop   = npi_build_scan_stop,
    .build_beacon_start = npi_build_beacon_start,
    .build_beacon_stop = npi_build_beacon_stop,
    .build_gatt_connect = NULL,
    .build_gatt_disconnect = NULL,
    .build_gatt_discover = NULL,
    .build_gatt_read   = NULL,
    .build_gatt_write  = NULL,
    .build_gatt_subscribe = NULL,
    .build_hci_cmd     = npi_build_hci_cmd,
    .build_vendor_cmd  = npi_build_vendor_cmd,
    .set_radio_tx_power = npi_set_radio_tx_power,
    .get_radio_power_range = npi_get_radio_power_range,
    .parse_rx          = npi_parse_rx,
    .probe             = npi_probe,
};
