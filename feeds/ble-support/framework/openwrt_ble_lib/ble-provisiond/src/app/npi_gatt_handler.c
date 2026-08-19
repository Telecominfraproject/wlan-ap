/**
 * @file npi_gatt_handler.c
 * @brief NPI GATT Server event handler for TI CC2652R1.
 *
 * When the GATT server runs on the MCU (CC2652R1 with BLE5-Stack),
 * this module:
 *   1. Receives NPI events when a phone writes to a GATT characteristic
 *   2. Executes the corresponding action (reboot, WiFi config, etc.)
 *   3. Sends NPI commands back to the MCU for GATT notifications
 *
 * The MCU firmware must:
 *   - Expose the same GATT services (FE00, FE10)
 *   - Forward all characteristic writes to Host via NPI GATT events
 *   - Forward read requests to Host for dynamic values
 *   - Accept notification commands from Host and send BLE notifications
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>

#include "npi_gatt_handler.h"
#include "mtu_segment.h"
#include <ble.h>

/* Reassembly contexts for segmented writes via NPI */
static mtu_reassembly_t s_ota_reassembly;
static mtu_reassembly_t s_wifi_reassembly;

/* Current connection state */
static struct {
    bool connected;
    uint16_t conn_handle;
    uint16_t mtu;
} s_conn = { .mtu = 23 };

/* Forward declarations (implemented in gatt_server_app.c) */
extern int handle_device_command(uint8_t cmd);
extern int handle_ota_url(const uint8_t *data, uint16_t len);
extern int handle_config_write(const uint8_t *data, uint16_t len);
extern int handle_wifi_scan(void);
extern int get_wifi_config(char *buf, size_t buf_size);
extern int get_wifi_status(char *buf, size_t buf_size);

/* ── NPI Frame Builder for GATT Commands ── */

/**
 * Build NPI frame for GATT notification command to MCU.
 * NPI format: SOF(0xFE) | LEN | CMD0 | CMD1 | PAYLOAD | FCS
 * CMD0 = (subsystem << 5) | msg_type
 * For GATT: subsystem = 0x05 (GATT), msg_type = 0x02 (SREQ)
 */
static int build_npi_gatt_notify_frame(uint16_t conn_handle, uint16_t attr_handle,
                                        const uint8_t *data, uint16_t data_len,
                                        uint8_t *out, uint16_t out_max)
{
    /* NPI GATT Notification frame:
     *   CMD0 = (0x05 << 5) | 0x02 = 0xA2 (GATT subsystem, SREQ)
     *   CMD1 = NPI_GATT_CMD_NOTIFY (0x10)
     *   Payload: conn_handle(2) + attr_handle(2) + data_len(2) + data(N)
     */
    uint16_t payload_len = 6 + data_len;
    uint16_t frame_len = 1 + 2 + 2 + payload_len + 1; /* SOF + LEN + CMD + PAYLOAD + FCS */

    if (frame_len > out_max) return -ENOMEM;

    uint16_t idx = 0;
    out[idx++] = 0xFE;  /* SOF */
    out[idx++] = (uint8_t)(payload_len & 0xFF);       /* LEN_LO */
    out[idx++] = (uint8_t)((payload_len >> 8) & 0xFF); /* LEN_HI */

    /* CMD0: GATT subsystem (0x05) + SREQ (0x02) */
    uint8_t cmd0 = (0x05 << 5) | 0x02;
    out[idx++] = cmd0;
    /* CMD1: Notify command */
    out[idx++] = NPI_GATT_CMD_NOTIFY;

    /* Payload */
    out[idx++] = (uint8_t)(conn_handle & 0xFF);
    out[idx++] = (uint8_t)((conn_handle >> 8) & 0xFF);
    out[idx++] = (uint8_t)(attr_handle & 0xFF);
    out[idx++] = (uint8_t)((attr_handle >> 8) & 0xFF);
    out[idx++] = (uint8_t)(data_len & 0xFF);
    out[idx++] = (uint8_t)((data_len >> 8) & 0xFF);

    if (data_len > 0 && data)
        memcpy(&out[idx], data, data_len);
    idx += data_len;

    /* FCS: XOR of all bytes from LEN_LO to end of payload */
    uint8_t fcs = 0;
    for (uint16_t i = 1; i < idx; i++)
        fcs ^= out[i];
    out[idx++] = fcs;

    return (int)idx;
}

static int build_npi_gatt_read_rsp_frame(uint16_t conn_handle, uint16_t attr_handle,
                                          const uint8_t *data, uint16_t data_len,
                                          uint8_t *out, uint16_t out_max)
{
    uint16_t payload_len = 6 + data_len;
    uint16_t frame_len = 1 + 2 + 2 + payload_len + 1;
    if (frame_len > out_max) return -ENOMEM;

    uint16_t idx = 0;
    out[idx++] = 0xFE;
    out[idx++] = (uint8_t)(payload_len & 0xFF);
    out[idx++] = (uint8_t)((payload_len >> 8) & 0xFF);
    out[idx++] = (0x05 << 5) | 0x02;  /* GATT SREQ */
    out[idx++] = NPI_GATT_CMD_READ_RSP;

    out[idx++] = (uint8_t)(conn_handle & 0xFF);
    out[idx++] = (uint8_t)((conn_handle >> 8) & 0xFF);
    out[idx++] = (uint8_t)(attr_handle & 0xFF);
    out[idx++] = (uint8_t)((attr_handle >> 8) & 0xFF);
    out[idx++] = (uint8_t)(data_len & 0xFF);
    out[idx++] = (uint8_t)((data_len >> 8) & 0xFF);

    if (data_len > 0 && data)
        memcpy(&out[idx], data, data_len);
    idx += data_len;

    uint8_t fcs = 0;
    for (uint16_t i = 1; i < idx; i++)
        fcs ^= out[i];
    out[idx++] = fcs;

    return (int)idx;
}

/* ── Public API ── */

int npi_gatt_handler_init(void)
{
    memset(&s_conn, 0, sizeof(s_conn));
    s_conn.mtu = 23;
    mtu_reassembly_init(&s_ota_reassembly);
    mtu_reassembly_init(&s_wifi_reassembly);
    syslog(LOG_INFO, "npi_gatt: handler initialized");
    return 0;
}

void npi_gatt_handler_deinit(void)
{
    s_conn.connected = false;
    syslog(LOG_INFO, "npi_gatt: handler deinitialized");
}

int npi_gatt_handle_event(const npi_gatt_event_t *evt)
{
    if (!evt) return -EINVAL;

    switch (evt->event_type) {
    case NPI_GATT_EVT_CONNECTED:
        s_conn.connected = true;
        s_conn.conn_handle = evt->conn_handle;
        syslog(LOG_INFO, "npi_gatt: client connected (handle=%u)", evt->conn_handle);
        return 0;

    case NPI_GATT_EVT_DISCONNECTED:
        s_conn.connected = false;
        syslog(LOG_INFO, "npi_gatt: client disconnected");
        mtu_reassembly_reset(&s_ota_reassembly);
        mtu_reassembly_reset(&s_wifi_reassembly);
        return 0;

    case NPI_GATT_EVT_MTU_EXCHANGE:
        s_conn.mtu = evt->mtu;
        syslog(LOG_INFO, "npi_gatt: MTU negotiated = %u", evt->mtu);
        return 0;

    case NPI_GATT_EVT_CHAR_WRITE:
        break; /* Handle below */

    case NPI_GATT_EVT_CHAR_READ:
        break; /* Handle below */

    default:
        syslog(LOG_DEBUG, "npi_gatt: unknown event type %u", evt->event_type);
        return 0;
    }

    /* ── Handle characteristic writes ── */
    if (evt->event_type == NPI_GATT_EVT_CHAR_WRITE) {
        switch (evt->attr_handle) {
        case NPI_HANDLE_DEV_CMD:
            if (evt->data_len >= 1)
                return handle_device_command(evt->data[0]);
            break;

        case NPI_HANDLE_OTA_URL: {
            /* Check for segmented write */
            if (evt->data_len > 0 && (evt->data[0] & (MTU_SEG_FLAG_FIRST | MTU_SEG_FLAG_LAST))) {
                int r = mtu_reassembly_feed(&s_ota_reassembly, evt->data, evt->data_len);
                if (r == 1) {
                    handle_ota_url(s_ota_reassembly.buffer, s_ota_reassembly.received_len);
                    mtu_reassembly_reset(&s_ota_reassembly);
                }
            } else {
                handle_ota_url(evt->data, evt->data_len);
            }
            break;
        }

        case NPI_HANDLE_WIFI_WRITE: {
            if (evt->data_len > 0 && (evt->data[0] & (MTU_SEG_FLAG_FIRST | MTU_SEG_FLAG_LAST))) {
                int r = mtu_reassembly_feed(&s_wifi_reassembly, evt->data, evt->data_len);
                if (r == 1) {
                    handle_config_write(s_wifi_reassembly.buffer, s_wifi_reassembly.received_len);
                    mtu_reassembly_reset(&s_wifi_reassembly);
                }
            } else {
                handle_config_write(evt->data, evt->data_len);
            }
            break;
        }

        case NPI_HANDLE_WIFI_SCAN:
            handle_wifi_scan();
            break;

        default:
            syslog(LOG_DEBUG, "npi_gatt: write to unknown handle 0x%04X", evt->attr_handle);
            break;
        }
    }

    /* ── Handle characteristic reads (MCU asks Host for value) ── */
    if (evt->event_type == NPI_GATT_EVT_CHAR_READ) {
        char buf[256];
        buf[0] = '\0';

        switch (evt->attr_handle) {
        case NPI_HANDLE_DEV_STATUS:
            snprintf(buf, sizeof(buf), "{\"status\":\"idle\"}");
            break;
        case NPI_HANDLE_WIFI_READ:
            get_wifi_config(buf, sizeof(buf));
            break;
        case NPI_HANDLE_WIFI_STATUS:
            get_wifi_status(buf, sizeof(buf));
            break;
        default:
            snprintf(buf, sizeof(buf), "{}");
            break;
        }

        npi_gatt_send_read_rsp(evt->conn_handle, evt->attr_handle,
                               (const uint8_t *)buf, (uint16_t)strlen(buf));
    }

    return 0;
}

int npi_gatt_send_notify(uint16_t conn_handle, uint16_t attr_handle,
                         const uint8_t *data, uint16_t data_len)
{
    if (!s_conn.connected) return -ENOTCONN;

    uint8_t frame[600];
    uint16_t max_payload = s_conn.mtu - 3; /* ATT notification payload limit */

    if (data_len <= max_payload) {
        /* Single notification */
        int flen = build_npi_gatt_notify_frame(conn_handle, attr_handle,
                                                data, data_len, frame, sizeof(frame));
        if (flen < 0) return flen;
        return ble_hci_send(0x0000, frame, (uint8_t)flen); /* Send via transport */
    }

    /* Segmented notification — split and send multiple frames */
    mtu_segmenter_t seg;
    mtu_segmenter_init(&seg, data, data_len, s_conn.mtu);

    uint8_t seg_buf[256];
    uint16_t seg_len;

    while ((seg_len = mtu_segmenter_next(&seg, seg_buf, sizeof(seg_buf))) > 0) {
        int flen = build_npi_gatt_notify_frame(conn_handle, attr_handle,
                                                seg_buf, seg_len, frame, sizeof(frame));
        if (flen < 0) return flen;
        int r = ble_hci_send(0x0000, frame, (uint8_t)flen);
        if (r < 0) return r;
        usleep(10000); /* 10ms delay between segments */
    }

    return 0;
}

int npi_gatt_send_read_rsp(uint16_t conn_handle, uint16_t attr_handle,
                           const uint8_t *data, uint16_t data_len)
{
    uint8_t frame[600];
    int flen = build_npi_gatt_read_rsp_frame(conn_handle, attr_handle,
                                              data, data_len, frame, sizeof(frame));
    if (flen < 0) return flen;
    return ble_hci_send(0x0000, frame, (uint8_t)flen);
}
