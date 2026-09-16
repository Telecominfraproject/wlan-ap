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
#include <unistd.h>
#include <stdio.h>

#include "chip_profile.h"
#include "../log.h"

/* GAP role bitmask set by ble_core before init (default = Broadcaster) */
static uint8_t s_gap_roles = BLE_ROLE_BROADCASTER;

void ti_npi_set_roles(uint8_t roles)
{
    s_gap_roles = roles ? roles : BLE_ROLE_BROADCASTER;
}

/*
 * multi_role firmware mode. When true, the chip runs a self-contained app
 * (multi_role) that boots and manages BLE on its own. In this mode the host:
 *   - sends NO chip-init HCI commands (no Reset/SetBDADDR/DeviceInit),
 *   - only bridges NPI GATT frames over UART.
 * Set by ble_core from UCI option firmware_mode=multirole.
 */
static bool s_multirole_mode = false;

void ti_npi_set_multirole(bool enable)
{
    s_multirole_mode = enable;
}

bool ti_npi_is_multirole(void)
{
    return s_multirole_mode;
}

/* HCI H4 packet type constants (TI chip uses HCI mode, not NPI framing) */
#define H4_CMD_PKT      0x01
#define H4_EVT_PKT      0x04
#define HCI_EVT_CMD_COMPLETE 0x0E

/* HCI event reassembly buffer size */
#define BUFFER_SIZE_HCI 512

/* Legacy NPI constants (kept for RX parser compatibility) */
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

/*
 * CC2652R1 TX power levels and their index values:
 * Index:  0=-21dBm, 1=-18dBm, 2=-15dBm, 3=-12dBm, 4=-9dBm,
 *         5=-6dBm, 6=-3dBm, 7=0dBm, 8=+1dBm, 9=+2dBm,
 *        10=+3dBm, 11=+4dBm, 12=+5dBm
 */
static const int8_t cc2652_power_levels[] = {
    -21, -18, -15, -12, -9, -6, -3, 0, 1, 2, 3, 4, 5
};
#define CC2652_LEVEL_COUNT  (int)(sizeof(cc2652_power_levels)/sizeof(cc2652_power_levels[0]))

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

/* Helper: build standard HCI H4 command */
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
    out->num_segments = 0;
}

/*
 * TI GAP_EVT_ADV_REPORT constants (from ec_blescan reference).
 * HCI event format: 04 FF <len> <event:2> <status:1> <eventId:4> ...
 */
#define TI_HCI_EVT_TYPE          0x04
#define TI_HCI_EVT_CODE          0xFF
#define TI_GAP_ADV_SCAN_EVENT    0x0613      /* GAP_AdvertiserScannerEvent */
#define TI_GAP_EVT_ADV_REPORT    0x00400000  /* EventId for advertisement report */

/* Reassembly buffer for HCI events (may span multiple UART reads) */
static uint8_t s_hci_buf[BUFFER_SIZE_HCI];
static uint16_t s_hci_len = 0;

/*
 * ── GATT server bridge (multi_role firmware) ──
 *
 * The TI multi_role firmware forwards phone GATT activity to the host using
 * NPI GATT frames:  SOF(0xFE) LEN(2 LE) CMD0(0xA2) CMD1 PAYLOAD FCS
 * (see DESIGN_OVERVIEW 6.2.4). These frames are interleaved on the same UART
 * as the HCI-format scan events. When parse_rx detects such a frame it hands
 * the decoded (cmd1, payload) to a bridge callback registered by the daemon's
 * npi_gatt_handler. FCS = XOR of LEN_LO..end-of-payload.
 */
#define NPI_GATT_SOF        0xFE
#define NPI_GATT_CMD0       0xA2

/* CMD1 for an Observer scan result forwarded by the multi_role firmware.
 * payload: addr(6, LE) addrType(1) rssi(1) advLen(2, LE) advData(N). */
#define NPI_GATT_EVT_SCAN_RESULT  0x06

typedef void (*ti_gatt_rx_cb_t)(uint8_t cmd1, const uint8_t *payload,
                                uint16_t plen, void *ctx);
static ti_gatt_rx_cb_t s_gatt_rx_cb = NULL;
static void *s_gatt_rx_ctx = NULL;

/* Scan-result event sink, set by npi_parse_rx for the duration of a parse call
 * so ti_gatt_extract_frames can emit BLE_EVT_SCAN_RESULT for NPI scan frames
 * (cmd1=0x06) instead of routing them to the GATT bridge callback. */
static void (*s_scan_event_cb)(const ble_event_t *, void *) = NULL;
static void *s_scan_event_ctx = NULL;

/* Reassembly buffer for NPI GATT frames (separate from HCI event buffer) */
static uint8_t s_gatt_buf[BUFFER_SIZE_HCI];
static uint16_t s_gatt_len = 0;

void ti_npi_set_gatt_rx_cb(ti_gatt_rx_cb_t cb, void *ctx)
{
    s_gatt_rx_cb = cb;
    s_gatt_rx_ctx = ctx;
    BLE_LOG_INFO("TI: GATT RX bridge callback %s",
                 cb ? "registered" : "cleared");
}

static uint8_t ti_gatt_fcs(const uint8_t *data, uint16_t len)
{
    uint8_t f = 0;
    for (uint16_t i = 0; i < len; i++) f ^= data[i];
    return f;
}

/* npi_build_frame and npi_fcs kept for potential future NPI-mode use */
#if 0
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
#endif

static int npi_init(int (*send_fn)(const uint8_t *, uint16_t, void *), void *ctx)
{
    s_hci_len = 0;  /* reset HCI reassembly buffer */
    s_gatt_len = 0; /* reset GATT frame reassembly buffer */

    /*
     * multi_role firmware mode: the chip runs its own app and boots
     * autonomously. Do NOT send any host_test HCI init commands — they would
     * be garbage to the multi_role FW and can corrupt the UART framing. Just
     * open the channel and let the NPI GATT bridge handle traffic.
     */
    if (s_multirole_mode) {
        (void)send_fn;
        (void)ctx;
        BLE_LOG_INFO("TI: multi_role firmware mode — skipping host_test chip init "
                     "(no Reset/DeviceInit sent; UART bridge only)");
        return 0;
    }

    /* Step 1: Reset */
    uint8_t reset_cmd[] = { 0x01, 0x1D, 0xFC, 0x01, 0x00 };
    BLE_LOG_INFO("TI: Sending HCI_EXT_ResetSystemCmd (0xFC1D)");
    int ret = send_fn(reset_cmd, sizeof(reset_cmd), ctx);
    if (ret < 0) return ret;

    usleep(300000);

    /* Step 2: SetBDADDR — read from eth0 and derive virtual BLE MAC */
    uint8_t setaddr_cmd[] = { 0x01, 0x0C, 0xFC, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    {
        FILE *fp = fopen("/sys/class/net/eth0/address", "r");
        if (!fp) fp = fopen("/sys/class/net/br-lan/address", "r");
        if (fp) {
            char mac_str[18] = {0};
            if (fgets(mac_str, sizeof(mac_str), fp)) {
                unsigned int m[6] = {0};
                if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
                           &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
                    m[0] |= 0x02;  /* locally administered bit */
                    /* TI expects little-endian */
                    setaddr_cmd[4] = (uint8_t)m[5];
                    setaddr_cmd[5] = (uint8_t)m[4];
                    setaddr_cmd[6] = (uint8_t)m[3];
                    setaddr_cmd[7] = (uint8_t)m[2];
                    setaddr_cmd[8] = (uint8_t)m[1];
                    setaddr_cmd[9] = (uint8_t)m[0];
                    BLE_LOG_INFO("TI: SetBDADDR = %02X:%02X:%02X:%02X:%02X:%02X",
                                 m[0] & 0xFF, m[1] & 0xFF, m[2] & 0xFF,
                                 m[3] & 0xFF, m[4] & 0xFF, m[5] & 0xFF);
                }
            }
            fclose(fp);
        }
    }
    send_fn(setaddr_cmd, sizeof(setaddr_cmd), ctx);
    usleep(100000);

    /* Step 3: SetTxPower — default index 0x0E (max) */
    uint8_t setpwr_cmd[] = { 0x01, 0x01, 0xFC, 0x01, 0x0E };
    send_fn(setpwr_cmd, sizeof(setpwr_cmd), ctx);
    usleep(100000);

    /* Step 4: GAP_DeviceInit — profileRole from UCI (Broadcaster/Observer/both) */
    uint8_t devinit_cmd[] = { 0x01, 0x00, 0xFE, 0x08, s_gap_roles,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    send_fn(devinit_cmd, sizeof(devinit_cmd), ctx);

    BLE_LOG_INFO("TI: Init sequence sent (Reset+SetBDADDR+SetTxPower+DeviceInit role=0x%02X)",
                 s_gap_roles);
    return 0;
}

/*
 * Build an NPI GATT command frame (SOF 0xFE, LEN, CMD0 0xA2, CMD1, PAYLOAD, FCS)
 * into out->data. Used in multi_role mode to bridge scan/beacon control to the
 * firmware over the same NPI channel the GATT bridge uses. FCS = XOR of
 * LEN_LO..end-of-payload.
 */
static int npi_build_gatt_cmd(uint8_t cmd1, const uint8_t *payload,
                              uint16_t plen, chip_cmd_buf_t *out)
{
    uint16_t idx = 0;
    out->data[idx++] = NPI_GATT_SOF;
    out->data[idx++] = (uint8_t)(plen & 0xFF);
    out->data[idx++] = (uint8_t)(plen >> 8);
    out->data[idx++] = NPI_GATT_CMD0;
    out->data[idx++] = cmd1;
    if (plen && payload) { memcpy(&out->data[idx], payload, plen); idx += plen; }
    out->data[idx] = ti_gatt_fcs(&out->data[1], (uint16_t)(4 + plen));
    idx++;
    out->len = idx;
    out->num_segments = 0;
    return 0;
}

/* NPI GATT command CMD1 values (host → firmware), must match uart_app.h. */
#define NPI_GATT_CMD_SCAN_START  0x26
#define NPI_GATT_CMD_SCAN_STOP   0x27

static int npi_build_scan_start(uint32_t duration_ms, bool active,
                                bool filter_dup, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;

    /*
     * multi_role firmware mode: don't send the host_test HCI GapScan sequence
     * (the self-running app would choke on raw HCI). Instead send an NPI GATT
     * SCAN_START command; the firmware runs GapScan_enable itself and forwards
     * results back as NPI scan-result frames (cmd1=0x06). Payload:
     * durationMs(2, LE) active(1).
     */
    if (s_multirole_mode) {
        uint16_t dur = (duration_ms > 0xFFFF) ? 0xFFFF : (uint16_t)duration_ms;
        uint8_t pl[3] = { (uint8_t)(dur & 0xFF), (uint8_t)(dur >> 8),
                          (uint8_t)(active ? 1 : 0) };
        (void)filter_dup;
        BLE_LOG_INFO("TI: multi_role SCAN_START (NPI bridge, dur=%ums active=%d)",
                     duration_ms, active);
        return npi_build_gatt_cmd(NPI_GATT_CMD_SCAN_START, pl, sizeof(pl), out);
    }

    (void)duration_ms;
    (void)active;

    uint16_t idx = 0;
    uint8_t seg = 0;

    /*
     * TI BLE5 GapScan sequence (from working script):
     *   GAP_DeviceInit role=Observer, GapScan_setPhyParams, GapScan_setParam,
     *   GapScan_setFilter, GapScan_enable.
     * Note: DeviceInit here uses role=0x02 (Observer) which differs from
     * iBeacon's 0x01 (Broadcaster). They cannot run simultaneously on TI.
     */

    /* Helper macro to append a command as a segment */
    #define NPI_ADD_SEG(delay_ms) do { \
        out->seg_offset[seg] = seg_start; \
        out->seg_len[seg] = idx - seg_start; \
        out->seg_delay_ms[seg] = (delay_ms); \
        seg++; \
    } while (0)

    uint16_t seg_start;

    /* Note: GAP_DeviceInit (role=0x03) already done in chip init.
     * Scan only needs to configure and enable scanning. */

    /* Cmd 1: GapScan_setPhyParams — scan interval (0xFE60, index=0x00) */
    seg_start = idx;
    out->data[idx++] = H4_CMD_PKT;
    out->data[idx++] = 0x60; out->data[idx++] = 0xFE;
    out->data[idx++] = 0x04;
    out->data[idx++] = 0x01; out->data[idx++] = 0x00;
    out->data[idx++] = 0x20; out->data[idx++] = 0x00;
    NPI_ADD_SEG(100);

    /* Cmd 3: GapScan_setPhyParams — scan window (0xFE60, index=0x01) */
    seg_start = idx;
    out->data[idx++] = H4_CMD_PKT;
    out->data[idx++] = 0x60; out->data[idx++] = 0xFE;
    out->data[idx++] = 0x04;
    out->data[idx++] = 0x01; out->data[idx++] = 0x01;
    out->data[idx++] = 0x20; out->data[idx++] = 0x00;
    NPI_ADD_SEG(100);

    /* Cmd 4-7: GapScan_setParam (0xFE61) params 0x02..0x05 */
    for (uint8_t p = 0x02; p <= 0x05; p++) {
        seg_start = idx;
        out->data[idx++] = H4_CMD_PKT;
        out->data[idx++] = 0x61; out->data[idx++] = 0xFE;
        out->data[idx++] = 0x02;
        out->data[idx++] = 0x01;
        out->data[idx++] = p;
        NPI_ADD_SEG(100);
    }

    /*
     * Note: GapScan_setFilter (0xFE55) commands removed — they return
     * status=0x02 on this firmware and are optional. Scanning works without
     * explicit filters (all advertisements are reported).
     */
    (void)filter_dup;

    /* GapScan_enable (0xFE51) — period, duration=0x40 (640ms) */
    seg_start = idx;
    out->data[idx++] = H4_CMD_PKT;
    out->data[idx++] = 0x51; out->data[idx++] = 0xFE;
    out->data[idx++] = 0x06;
    out->data[idx++] = 0x01;  /* period */
    out->data[idx++] = 0x00;
    out->data[idx++] = 0x40; out->data[idx++] = 0x00;  /* duration 640ms */
    out->data[idx++] = 0xFF; out->data[idx++] = 0xFF;  /* maxReports=unlimited */
    NPI_ADD_SEG(100);

    #undef NPI_ADD_SEG

    out->len = idx;
    out->num_segments = seg;

    BLE_LOG_INFO("TI: Built scan start sequence (%u bytes, %u cmds)", idx, seg);
    return 0;
}

static int npi_build_scan_stop(chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;

    /* multi_role: send NPI GATT SCAN_STOP; firmware runs GapScan_disable. */
    if (s_multirole_mode) {
        BLE_LOG_INFO("TI: multi_role SCAN_STOP (NPI bridge)");
        return npi_build_gatt_cmd(NPI_GATT_CMD_SCAN_STOP, NULL, 0, out);
    }

    uint16_t idx = 0;

    /* GapScan_disable (0xFE52), no params */
    out->data[idx++] = H4_CMD_PKT;
    out->data[idx++] = 0x52; out->data[idx++] = 0xFE;
    out->data[idx++] = 0x00;

    out->len = idx;
    out->num_segments = 1;
    out->seg_offset[0] = 0;
    out->seg_len[0] = idx;
    out->seg_delay_ms[0] = 100;

    BLE_LOG_INFO("TI: Built scan stop command (%u bytes)", idx);
    return 0;
}

/* NPI GATT command CMD1 for iBeacon (host → firmware), matches uart_app.h. */
#define NPI_GATT_CMD_BEACON_START  0x24
#define NPI_GATT_CMD_BEACON_STOP   0x25

static int npi_build_beacon_start(const ble_beacon_config_t *config,
                                  chip_cmd_buf_t *out)
{
    if (!config || !out) return -EINVAL;

    /*
     * multi_role firmware mode: bridge iBeacon control over NPI. The firmware
     * runs iBeacon on a dedicated (second) advertising set, coexisting with the
     * connectable GATT set. Payload (matches uart_app.c handle_frame):
     *   uuid16(2, from first 2 bytes of the 128-bit UUID) major(2) minor(2)
     *   txpwr(1). The firmware carries the full 16-byte iBeacon UUID itself;
     *   we pass the leading 2 bytes plus major/minor/measured-power.
     */
    if (s_multirole_mode) {
        uint8_t pl[7];
        pl[0] = config->uuid_bytes[0];
        pl[1] = config->uuid_bytes[1];
        pl[2] = (uint8_t)(config->major & 0xFF);
        pl[3] = (uint8_t)(config->major >> 8);
        pl[4] = (uint8_t)(config->minor & 0xFF);
        pl[5] = (uint8_t)(config->minor >> 8);
        pl[6] = (uint8_t)config->tx_power;
        BLE_LOG_INFO("TI: multi_role BEACON_START (NPI bridge, major=%u minor=%u)",
                     config->major, config->minor);
        return npi_build_gatt_cmd(NPI_GATT_CMD_BEACON_START, pl, sizeof(pl), out);
    }

    uint16_t idx = 0;

    /*
     * Note: SetBDADDR, SetTxPower, DeviceInit are done during chip init.
     * Beacon start only needs: AdvParams + AdvData + MakeDiscoverable.
     */

    /*
     * Command 1: GAP_SetAdvParams (0xFE3E)
     * From script: \x01\x3E\xFE\x15 + 21 bytes params
     */
    out->data[idx++] = H4_CMD_PKT;
    out->data[idx++] = 0x3E; out->data[idx++] = 0xFE;  /* opcode 0xFE3E */
    out->data[idx++] = 0x15;  /* param_len = 21 */
    uint8_t adv_params[] = {
        0x12, 0x00, 0xA0, 0x00, 0x00, 0xA0, 0x00, 0x00,
        0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x7F, 0x01, 0x01, 0x00
    };
    memcpy(&out->data[idx], adv_params, 21);
    idx += 21;

    /*
     * Command 2: GAP_SetAdvData (packet type 0x09, opcode 0xFE44)
     * From script: \x09\x44\xFE\x23 + 35 bytes
     * Format: advHandle(1) + operation(1) + fragmentPref(1) + dataLen(1) + advDataToken(1) + data(30)
     */
    out->data[idx++] = 0x09;  /* TI extended data packet type */
    out->data[idx++] = 0x44; out->data[idx++] = 0xFE;
    out->data[idx++] = 0x23;  /* param_len = 35 */
    out->data[idx++] = 0x00;  /* advHandle */
    out->data[idx++] = 0x00;  /* operation: complete */
    out->data[idx++] = 0x00;  /* fragmentPref */
    out->data[idx++] = 0x1E;  /* dataLen = 30 bytes */
    out->data[idx++] = 0x00;  /* advDataToken (required by TI firmware) */

    /* Advertising data: Flags + iBeacon */
    out->data[idx++] = 0x02;  /* Flags length */
    out->data[idx++] = 0x01;  /* AD Type: Flags */
    out->data[idx++] = 0x1A;  /* Flags value */

    out->data[idx++] = 0x1A;  /* iBeacon data length (26) */
    out->data[idx++] = 0xFF;  /* AD Type: Manufacturer Specific */
    out->data[idx++] = 0x4C;  /* Apple company ID low */
    out->data[idx++] = 0x00;  /* Apple company ID high */
    out->data[idx++] = 0x02;  /* iBeacon type */
    out->data[idx++] = 0x15;  /* iBeacon data length (21) */

    /* UUID (16 bytes) */
    memcpy(&out->data[idx], config->uuid_bytes, 16);
    idx += 16;

    /* Major (big-endian) */
    out->data[idx++] = (config->major >> 8) & 0xFF;
    out->data[idx++] = config->major & 0xFF;

    /* Minor (big-endian) */
    out->data[idx++] = (config->minor >> 8) & 0xFF;
    out->data[idx++] = config->minor & 0xFF;

    /* Measured power */
    out->data[idx++] = (uint8_t)config->tx_power;

    /*
     * Extra byte 0x00 between AdvData and MakeDiscoverable.
     * The working script sends this — it exceeds param_len but TI chip
     * ignores it (invalid packet type). Kept for exact byte compatibility.
     */
    out->data[idx++] = 0x00;

    /*
     * Command 3: GAP_MakeDiscoverable (0xFE3F)
     * From script: \x01\x3F\xFE\x04\x00\x00\x00\x00
     */
    out->data[idx++] = H4_CMD_PKT;
    out->data[idx++] = 0x3F; out->data[idx++] = 0xFE;  /* opcode 0xFE3F */
    out->data[idx++] = 0x04;  /* param_len = 4 */
    out->data[idx++] = 0x00;
    out->data[idx++] = 0x00;
    out->data[idx++] = 0x00;
    out->data[idx++] = 0x00;

    out->len = idx;

    /* Mark segments for sequential send with inter-command delays */
    out->num_segments = 3;
    /* Segment 0: AdvParams (25 bytes) — 100ms delay */
    out->seg_offset[0] = 0;
    out->seg_len[0] = 25;
    out->seg_delay_ms[0] = 100;
    /* Segment 1: AdvData + trailing 0x00 (40 bytes) — 500ms delay */
    out->seg_offset[1] = 25;
    out->seg_len[1] = idx - 25 - 8;
    out->seg_delay_ms[1] = 500;
    /* Segment 2: MakeDiscoverable (8 bytes) — 200ms delay */
    out->seg_offset[2] = idx - 8;
    out->seg_len[2] = 8;
    out->seg_delay_ms[2] = 200;

    BLE_LOG_INFO("TI: Built iBeacon command sequence (%u bytes, 3 cmds)", idx);
    return 0;
}

static int npi_build_beacon_stop(chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;

    /* multi_role: send NPI BEACON_STOP; firmware disables the iBeacon adv set. */
    if (s_multirole_mode) {
        BLE_LOG_INFO("TI: multi_role BEACON_STOP (NPI bridge)");
        return npi_build_gatt_cmd(NPI_GATT_CMD_BEACON_STOP, NULL, 0, out);
    }

    uint16_t idx = 0;

    /*
     * GAP_EndDiscoverable / GAP_AdvDisable (0xFE3F with enable=0)
     * Or simply use GAP_TerminateLinkReq... 
     * Simplest: re-send MakeDiscoverable with advEnable=0
     * Actually TI uses: HCI_LE_SetAdvertisingEnable(0) or GAP_EndDiscoverable(0xFE08)
     * From TI docs: GAP_EndDiscoverable = 0xFE46 (BLE5) or we can use standard HCI:
     * HCI_LE_Set_Advertising_Enable (opcode 0x200A) param: 0x00
     */
    out->data[idx++] = H4_CMD_PKT;
    out->data[idx++] = 0x0A; out->data[idx++] = 0x20;  /* opcode 0x200A: LE Set Adv Enable */
    out->data[idx++] = 0x01;  /* param_len = 1 */
    out->data[idx++] = 0x00;  /* enable = 0 (disable) */

    out->len = idx;
    out->num_segments = 1;
    out->seg_offset[0] = 0;
    out->seg_len[0] = idx;
    out->seg_delay_ms[0] = 100;

    BLE_LOG_INFO("TI: Built beacon stop command (%u bytes)", idx);
    return 0;
}

static int npi_build_hci_cmd(const ble_hci_cmd_t *cmd, chip_cmd_buf_t *out)
{
    if (!cmd || !out) return -EINVAL;
    /* Standard HCI H4 framing (TI chip in HCI mode) */
    build_h4_cmd(cmd->opcode, cmd->params, cmd->param_len, out);
    out->num_segments = 0;
    return 0;
}

static int npi_build_vendor_cmd(uint8_t ogf, uint16_t ocf,
                                const uint8_t *params, uint8_t param_len,
                                chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;
    uint16_t opcode = ((uint16_t)(ogf & 0x3F) << 10) | (ocf & 0x03FF);
    build_h4_cmd(opcode, params, param_len, out);
    out->num_segments = 0;
    return 0;
}

/**
 * Parse TI HCI-format RX stream, extract GAP_EVT_ADV_REPORT → BLE_EVT_SCAN_RESULT.
 * Mirrors the ec_blescan reference parser so scan results flow through the
 * same event path as the BlueZ transport.
 */
/*
 * Peel NPI GATT frames (SOF 0xFE, CMD0 0xA2) out of the reassembly buffer and
 * dispatch them to the GATT bridge callback. Returns the number of frames
 * dispatched. Non-GATT bytes are left in place for the HCI event parser.
 *
 * We only consume a leading run that is a valid/partial GATT frame; if the
 * buffer head is 0x04 (HCI event) we return immediately and let the HCI parser
 * run. This keeps the two stream types from corrupting each other.
 */
static int ti_gatt_extract_frames(uint8_t *buf, uint16_t *plen)
{
    int frames = 0;

    while (*plen >= 1) {
        /* If head is an HCI event, hand back to the HCI parser. */
        if (buf[0] == TI_HCI_EVT_TYPE)
            break;

        /* Resync: drop anything that is not our SOF. */
        if (buf[0] != NPI_GATT_SOF) {
            memmove(buf, buf + 1, *plen - 1);
            (*plen)--;
            continue;
        }

        /* Need at least SOF+LEN(2)+CMD0+CMD1 = 5 bytes to know the length. */
        if (*plen < 5)
            break;  /* wait for more */

        uint16_t payload_len = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
        uint8_t  cmd0 = buf[3];
        uint8_t  cmd1 = buf[4];
        int total = 1 + 2 + 2 + payload_len + 1;  /* + FCS */

        if (payload_len > BUFFER_SIZE_HCI) {
            /* Bogus length — drop SOF and resync. */
            memmove(buf, buf + 1, *plen - 1);
            (*plen)--;
            continue;
        }
        if (*plen < total)
            break;  /* wait for the rest of the frame */

        /* Full frame present — validate FCS over LEN_LO..end-of-payload. */
        uint8_t fcs_calc = ti_gatt_fcs(&buf[1], 4 + payload_len);
        uint8_t fcs_rx = buf[5 + payload_len];

        if (cmd0 == NPI_GATT_CMD0 && fcs_calc == fcs_rx) {
            BLE_LOG_DBG("TI GATT frame: cmd1=0x%02X payload_len=%u", cmd1, payload_len);
            if (cmd1 == NPI_GATT_EVT_SCAN_RESULT) {
                /* Observer scan result — emit as a BLE_EVT_SCAN_RESULT so it
                 * flows through the same event path as the BlueZ transport,
                 * rather than to the GATT provisioning bridge. */
                const uint8_t *pl = &buf[5];
                if (payload_len >= 10 && s_scan_event_cb) {
                    ble_event_t ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.type = BLE_EVT_SCAN_RESULT;
                    /* addr is little-endian; print MSB-first for display. */
                    snprintf(ev.scan_result.address, BLE_ADDR_STR_LEN,
                             "%02X:%02X:%02X:%02X:%02X:%02X",
                             pl[5], pl[4], pl[3], pl[2], pl[1], pl[0]);
                    snprintf(ev.scan_result.address_type,
                             sizeof(ev.scan_result.address_type), "%s",
                             pl[6] ? "random" : "public");
                    ev.scan_result.rssi = (int8_t)pl[7];
                    uint16_t adv_len = (uint16_t)pl[8] | ((uint16_t)pl[9] << 8);
                    if (adv_len > (uint16_t)(payload_len - 10))
                        adv_len = (uint16_t)(payload_len - 10);
                    if (adv_len > BLE_ADV_DATA_MAX) adv_len = BLE_ADV_DATA_MAX;
                    memcpy(ev.scan_result.adv_data, &pl[10], adv_len);
                    ev.scan_result.adv_data_len = (uint8_t)adv_len;
                    ev.scan_result.connectable = true;
                    s_scan_event_cb(&ev, s_scan_event_ctx);
                }
            } else if (s_gatt_rx_cb) {
                s_gatt_rx_cb(cmd1, &buf[5], payload_len, s_gatt_rx_ctx);
            } else {
                BLE_LOG_DBG("TI GATT frame dropped (no bridge callback)");
            }
            frames++;
        } else {
            BLE_LOG_DBG("TI GATT frame bad (cmd0=0x%02X fcs rx=0x%02X calc=0x%02X) — dropping SOF",
                        cmd0, fcs_rx, fcs_calc);
            /* Bad frame: drop just the SOF and resync. */
            memmove(buf, buf + 1, *plen - 1);
            (*plen)--;
            continue;
        }

        /* Consume the whole frame. */
        memmove(buf, buf + total, *plen - total);
        *plen -= total;
    }
    return frames;
}

static int npi_parse_rx(const uint8_t *data, uint16_t len,
                        void (*event_cb)(const ble_event_t *, void *), void *ctx)
{
    int events = 0;

    /* Make the scan-result sink available to ti_gatt_extract_frames so NPI
     * scan frames (cmd1=0x06) become BLE_EVT_SCAN_RESULT events. */
    s_scan_event_cb = event_cb;
    s_scan_event_ctx = ctx;

    /* Debug: log incoming RX bytes */
    if (len > 0) {
        char hex[97] = {0};
        int dl = (len > 32) ? 32 : len;
        for (int i = 0; i < dl; i++)
            sprintf(hex + i * 3, "%02X ", data[i]);
        BLE_LOG_DBG("TI RX (%u bytes): [%s]", len, hex);
    }

    /*
     * GATT bridge path: if a callback is registered and the incoming stream
     * looks like NPI GATT frames, route them through the dedicated GATT buffer.
     * We accumulate into s_gatt_buf and peel complete frames. This runs only
     * when the head byte is our SOF (0xFE); HCI events (0x04) fall through to
     * the scan parser below.
     */
    if (s_gatt_rx_cb &&
        ((s_gatt_len > 0) || (len > 0 && data[0] == NPI_GATT_SOF))) {
        if (s_gatt_len + len < sizeof(s_gatt_buf)) {
            memcpy(s_gatt_buf + s_gatt_len, data, len);
            s_gatt_len += len;
            ti_gatt_extract_frames(s_gatt_buf, &s_gatt_len);

            /* After extraction the leftover is one of:
             *  - empty            → everything was GATT, done.
             *  - starts with 0xFE → a PARTIAL GATT frame; keep it buffered and
             *                       wait for the rest (do NOT feed HCI parser).
             *  - starts with 0x04 → an HCI event; move it to the HCI parser. */
            if (s_gatt_len == 0)
                return 0;
            if (s_gatt_buf[0] == NPI_GATT_SOF)
                return 0;   /* partial GATT frame — wait for more bytes */

            /* Leftover is non-GATT (HCI event); pass it on. */
            data = s_gatt_buf;
            len = s_gatt_len;
            s_gatt_len = 0;
        } else {
            s_gatt_len = 0;  /* overflow reset */
        }
    }

    /* Accumulate into reassembly buffer */
    if (s_hci_len + len < sizeof(s_hci_buf)) {
        memcpy(s_hci_buf + s_hci_len, data, len);
        s_hci_len += len;
    } else {
        /* Overflow — reset */
        s_hci_len = 0;
        return 0;
    }

    /* Process complete HCI events: [04 FF len payload...] */
    while (s_hci_len >= 3) {
        if (!(s_hci_buf[0] == TI_HCI_EVT_TYPE && s_hci_buf[1] == TI_HCI_EVT_CODE)) {
            /* Not an HCI event start — shift by 1 and retry */
            memmove(s_hci_buf, s_hci_buf + 1, s_hci_len - 1);
            s_hci_len--;
            continue;
        }

        uint8_t data_length = s_hci_buf[2];
        int total_length = data_length + 3;
        if (s_hci_len < total_length)
            break;  /* Wait for more data */

        /* Complete event — parse it */
        const uint8_t *payload = &s_hci_buf[3];
        uint16_t event_type = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        uint8_t status = payload[2];
        uint32_t event_id = (uint32_t)payload[3] | ((uint32_t)payload[4] << 8) |
                            ((uint32_t)payload[5] << 16) | ((uint32_t)payload[6] << 24);

        BLE_LOG_DBG("TI event: type=0x%04X status=0x%02X eventId=0x%08X len=%d",
                    event_type, status, event_id, total_length);

        if (event_type == TI_GAP_ADV_SCAN_EVENT &&
            event_id == TI_GAP_EVT_ADV_REPORT && status == 0x00) {
            ble_event_t event;
            memset(&event, 0, sizeof(event));
            event.type = BLE_EVT_SCAN_RESULT;

            /* MAC at payload[9..14] (little-endian) */
            const uint8_t *mac = &payload[9];
            snprintf(event.scan_result.address, BLE_ADDR_STR_LEN,
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);

            /* RSSI at payload[19] */
            event.scan_result.rssi = (int8_t)payload[19];

            /* Advertising data starts at payload[31] */
            int adv_len = total_length - 3 - 31;  /* total - header - fixed fields */
            if (adv_len > 0) {
                if (adv_len > BLE_ADV_DATA_MAX) adv_len = BLE_ADV_DATA_MAX;
                memcpy(event.scan_result.adv_data, &payload[31], adv_len);
                event.scan_result.adv_data_len = (uint8_t)adv_len;
            }
            /* TI adv reports are undirected connectable unless flagged otherwise */
            event.scan_result.connectable = true;
            snprintf(event.scan_result.address_type, sizeof(event.scan_result.address_type),
                     "%s", "public");

            BLE_LOG_INFO("TI scan result: %s rssi=%d adv_len=%d",
                         event.scan_result.address, event.scan_result.rssi, adv_len);

            if (event_cb) event_cb(&event, ctx);
            events++;
        }

        /* Remove processed event */
        memmove(s_hci_buf, s_hci_buf + total_length, s_hci_len - total_length);
        s_hci_len -= total_length;
    }

    return events;
}

static int npi_set_radio_tx_power(int8_t power_dbm, chip_cmd_buf_t *out)
{
    if (!out) return -EINVAL;

    /*
     * HCI_EXT_SetTxPowerCmd (0xFC01)
     * From script: \x01\x01\xFC\x01 + power_index
     */
    uint8_t idx_val = cc2652_power_to_index(power_dbm);
    uint16_t i = 0;

    out->data[i++] = H4_CMD_PKT;
    out->data[i++] = 0x01; out->data[i++] = 0xFC;  /* opcode 0xFC01 */
    out->data[i++] = 0x01;  /* param_len = 1 */
    out->data[i++] = idx_val;

    out->len = i;
    out->num_segments = 0;  /* single command, no segmenting needed */
    return 0;
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
    /* Send HCI Reset (0x0C03) and check for Command Complete event */
    uint8_t reset[] = { H4_CMD_PKT, 0x03, 0x0C, 0x00 };
    if (send_fn(reset, 4, ctx) < 0) return 0;
    uint8_t buf[16];
    int ret = recv_fn(buf, sizeof(buf), 1000, ctx);
    if (ret >= 4 && buf[0] == H4_EVT_PKT && buf[1] == HCI_EVT_CMD_COMPLETE)
        return 1;
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
