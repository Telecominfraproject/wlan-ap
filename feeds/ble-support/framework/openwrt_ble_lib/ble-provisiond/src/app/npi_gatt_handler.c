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
#include "app_status.h"
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

/* Provisioning setup params, pushed to the firmware when it signals READY. */
static struct {
    char     name[64];
    uint8_t  bdaddr[6];
    bool     have_addr;
    bool     valid;
    bool     sent;         /* true once provision setup pushed for this FW session */
    bool     gatt_enabled; /* mirror of UCI ble.gatt_server.enabled */
} s_prov;

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

/*
 * Bridge callback: called by libble (ti_npi chip profile) for each NPI GATT
 * frame forwarded by the multi_role firmware. Decodes the (cmd1, payload) into
 * an npi_gatt_event_t and dispatches it to npi_gatt_handle_event().
 *
 * Payload layouts (see DESIGN_OVERVIEW 6.2.4):
 *   CHAR_WRITE (0x01): conn(2) + handle(2) + len(2) + data(N)
 *   CHAR_READ  (0x02): conn(2) + handle(2)
 *   CONNECTED  (0x03): conn(2)
 *   DISCONNECT (0x04): conn(2)
 *   MTU        (0x05): conn(2) + mtu(2)
 * All multi-byte fields are little-endian.
 */
static void npi_gatt_bridge_cb(uint8_t cmd1, const uint8_t *payload,
                               uint16_t plen, void *ctx)
{
    (void)ctx;
    npi_gatt_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.event_type = cmd1;

    /*
     * When provisioning GATT is disabled (UCI gatt_server.enabled=0), behave
     * like the BlueZ path where the services are not registered at all: reject
     * every characteristic read/write so an already-connected phone (or one
     * that connected by known address without needing an advertisement) cannot
     * drive any action or read any value. Non-data events (CONNECTED /
     * DISCONNECTED / MTU / READY / ACKs) still flow so state stays consistent.
     */
    if (!s_prov.gatt_enabled &&
        (cmd1 == NPI_GATT_EVT_CHAR_WRITE || cmd1 == NPI_GATT_EVT_CHAR_READ)) {
        syslog(LOG_INFO, "npi_gatt: GATT disabled — dropping char %s (cmd1=0x%02X)",
               (cmd1 == NPI_GATT_EVT_CHAR_WRITE) ? "write" : "read", cmd1);
        return;
    }

    switch (cmd1) {
    case NPI_GATT_EVT_CHAR_WRITE:
        if (plen < 6) {
            syslog(LOG_WARNING, "npi_gatt: short CHAR_WRITE frame (%u)", plen);
            return;
        }
        evt.conn_handle = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        evt.attr_handle = (uint16_t)payload[2] | ((uint16_t)payload[3] << 8);
        evt.data_len    = (uint16_t)payload[4] | ((uint16_t)payload[5] << 8);
        if (evt.data_len > sizeof(evt.data))
            evt.data_len = sizeof(evt.data);
        if (evt.data_len > (uint16_t)(plen - 6))
            evt.data_len = (uint16_t)(plen - 6);
        memcpy(evt.data, &payload[6], evt.data_len);
        syslog(LOG_DEBUG, "npi_gatt: CHAR_WRITE conn=%u handle=0x%04X len=%u",
               evt.conn_handle, evt.attr_handle, evt.data_len);
        break;

    case NPI_GATT_EVT_CHAR_READ:
        if (plen < 4) return;
        evt.conn_handle = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        evt.attr_handle = (uint16_t)payload[2] | ((uint16_t)payload[3] << 8);
        syslog(LOG_DEBUG, "npi_gatt: CHAR_READ conn=%u handle=0x%04X",
               evt.conn_handle, evt.attr_handle);
        break;

    case NPI_GATT_EVT_CONNECTED:
        if (plen >= 2)
            evt.conn_handle = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        syslog(LOG_INFO, "npi_gatt: CONNECTED conn=%u", evt.conn_handle);
        break;

    case NPI_GATT_EVT_DISCONNECTED:
        if (plen >= 2)
            evt.conn_handle = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        syslog(LOG_INFO, "npi_gatt: DISCONNECTED conn=%u", evt.conn_handle);
        break;

    case NPI_GATT_EVT_MTU_EXCHANGE:
        if (plen < 4) return;
        evt.conn_handle = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        evt.mtu         = (uint16_t)payload[2] | ((uint16_t)payload[3] << 8);
        syslog(LOG_INFO, "npi_gatt: MTU conn=%u mtu=%u", evt.conn_handle, evt.mtu);
        break;

    case NPI_GATT_EVT_READY:
        /*
         * Firmware finished advertising init (advHandle valid). Now push the
         * provisioning setup so the name update lands after the default scan
         * response is loaded. This is the correct time — not at daemon start.
         */
        if (!s_prov.valid) {
            syslog(LOG_INFO, "npi_gatt: firmware READY (no provision params yet)");
            return;
        }
        if (s_prov.sent) {
            /*
             * Already pushed name/addr/adv once for this firmware session.
             * The firmware normally stops re-emitting READY after it applies
             * SET_NAME; if we still get READY it means the previous push was
             * not handled. Re-send once more but do not spam every 2s — log at
             * debug so the syslog stays readable.
             */
            syslog(LOG_DEBUG, "npi_gatt: firmware READY again — re-pushing provision setup");
        } else {
            syslog(LOG_INFO, "npi_gatt: firmware READY — pushing provision setup");
        }
        npi_gatt_send_provision_setup(s_prov.name,
                                      s_prov.have_addr ? s_prov.bdaddr : NULL);
        s_prov.sent = true;
        return;

    case NPI_GATT_EVT_BDADDR:
        /*
         * Firmware reports its actual (advertised) BD address. This is the
         * chip's own public address, which differs from the eth0-derived value
         * the daemon computes. Log it so the operator sees the MAC the phone
         * actually scans. Payload is 6 bytes little-endian.
         */
        if (plen >= 6) {
            syslog(LOG_INFO, "npi_gatt: firmware BD address (advertised) = "
                   "%02X:%02X:%02X:%02X:%02X:%02X",
                   payload[5], payload[4], payload[3],
                   payload[2], payload[1], payload[0]);
            /* Sync the real MAC into libble + the status file. */
            extern int ble_ti_set_actual_bd_address(const uint8_t addr_le[6]);
            ble_ti_set_actual_bd_address(payload);
        }
        return;

    case 0x7E:
        /*
         * Diagnostic ACK from the firmware: it received and handled SET_NAME
         * in the NPI frame parser (NOT an echo). Seeing this at INFO level is
         * the positive confirmation that daemon→firmware commands are working
         * and the firmware is in frame-parsing mode (not echo mode).
         */
        syslog(LOG_DEBUG, "npi_gatt: firmware ACK 0x7E — SET_NAME handled (frame parser active)");
        return;

    default:
        syslog(LOG_DEBUG, "npi_gatt: bridge cmd1=0x%02X plen=%u (ignored)",
               cmd1, plen);
        return;
    }

    npi_gatt_handle_event(&evt);
}

void npi_gatt_set_provision_params(const char *name, const uint8_t *bdaddr)
{
    bool prev_enabled = s_prov.gatt_enabled;
    memset(&s_prov, 0, sizeof(s_prov));
    if (name)
        snprintf(s_prov.name, sizeof(s_prov.name), "%s", name);
    if (bdaddr) {
        memcpy(s_prov.bdaddr, bdaddr, 6);
        s_prov.have_addr = true;
    }
    s_prov.gatt_enabled = prev_enabled;  /* preserve across param refresh */
    s_prov.valid = true;
    syslog(LOG_INFO, "npi_gatt: provision params set (name='%s', addr=%s) "
           "— will send on firmware READY", s_prov.name,
           s_prov.have_addr ? "yes" : "no");
}

int npi_gatt_handler_init(void)
{
    memset(&s_conn, 0, sizeof(s_conn));
    s_conn.mtu = 23;
    mtu_reassembly_init(&s_ota_reassembly);
    mtu_reassembly_init(&s_wifi_reassembly);

    /* Register with libble so multi_role GATT frames reach us. */
    ble_ti_set_gatt_bridge_cb(npi_gatt_bridge_cb, NULL);

    syslog(LOG_INFO, "npi_gatt: handler initialized (bridge callback registered)");
    return 0;
}

void npi_gatt_handler_deinit(void)
{
    ble_ti_set_gatt_bridge_cb(NULL, NULL);
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

        /* FE14 WiFi Scan removed (not part of the uCentral pass-through core;
         * the firmware no longer registers it). */

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
        syslog(LOG_DEBUG, "npi_gatt: NOTIFY conn=%u handle=0x%04X len=%u (frame %d)",
               conn_handle, attr_handle, data_len, flen);
        /* Send the complete NPI frame verbatim (no HCI framing added). */
        return ble_ti_gatt_send_raw(frame, (uint16_t)flen);
    }

    /* Segmented notification — split and send multiple frames */
    mtu_segmenter_t seg;
    mtu_segmenter_init(&seg, data, data_len, s_conn.mtu);

    uint8_t seg_buf[256];
    uint16_t seg_len;

    syslog(LOG_DEBUG, "npi_gatt: NOTIFY (segmented) conn=%u handle=0x%04X total=%u",
           conn_handle, attr_handle, data_len);
    while ((seg_len = mtu_segmenter_next(&seg, seg_buf, sizeof(seg_buf))) > 0) {
        int flen = build_npi_gatt_notify_frame(conn_handle, attr_handle,
                                                seg_buf, seg_len, frame, sizeof(frame));
        if (flen < 0) return flen;
        int r = ble_ti_gatt_send_raw(frame, (uint16_t)flen);
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
    syslog(LOG_DEBUG, "npi_gatt: READ_RSP conn=%u handle=0x%04X len=%u (frame %d)",
           conn_handle, attr_handle, data_len, flen);
    return ble_ti_gatt_send_raw(frame, (uint16_t)flen);
}

/*
 * Build + send a generic NPI GATT command frame:
 *   SOF(0xFE) LEN(2 LE) CMD0(0xA2) CMD1 PAYLOAD FCS
 * FCS = XOR of LEN_LO..end-of-payload.
 */
static int npi_send_cmd(uint8_t cmd1, const uint8_t *payload, uint16_t plen)
{
    uint8_t frame[300];
    uint16_t frame_len = 1 + 2 + 2 + plen + 1;
    if (frame_len > sizeof(frame)) return -ENOMEM;

    uint16_t idx = 0;
    frame[idx++] = 0xFE;                              /* SOF */
    frame[idx++] = (uint8_t)(plen & 0xFF);            /* LEN_LO */
    frame[idx++] = (uint8_t)((plen >> 8) & 0xFF);     /* LEN_HI */
    frame[idx++] = (0x05 << 5) | 0x02;                /* CMD0 = 0xA2 */
    frame[idx++] = cmd1;                              /* CMD1 */
    if (plen && payload) { memcpy(&frame[idx], payload, plen); idx += plen; }

    uint8_t fcs = 0;
    for (uint16_t i = 1; i < idx; i++) fcs ^= frame[i];  /* LEN_LO..payload */
    frame[idx++] = fcs;

    return ble_ti_gatt_send_raw(frame, idx);
}

int npi_gatt_send_provision_setup(const char *name, const uint8_t bdaddr[6])
{
    int r;

    if (name && *name) {
        uint16_t nlen = (uint16_t)strlen(name);
        syslog(LOG_INFO, "npi_gatt: SET_NAME '%s' (%u)", name, nlen);
        r = npi_send_cmd(NPI_GATT_CMD_SET_NAME, (const uint8_t *)name, nlen);
        if (r < 0) return r;
        usleep(20000);
    }

    if (bdaddr) {
        syslog(LOG_INFO, "npi_gatt: SET_BDADDR %02X:%02X:%02X:%02X:%02X:%02X",
               bdaddr[5], bdaddr[4], bdaddr[3], bdaddr[2], bdaddr[1], bdaddr[0]);
        r = npi_send_cmd(NPI_GATT_CMD_SET_BDADDR, bdaddr, 6);
        if (r < 0) return r;
        usleep(20000);
    }

    /*
     * Gate connectable advertising on UCI ble.gatt_server.enabled, mirroring
     * the BlueZ path (which only registers services + advertises when enabled).
     * The TI firmware registers the GATT services at boot regardless, but if
     * provisioning is disabled we stop advertising so the phone can't discover
     * them — the observable behaviour matches BlueZ (device not advertising the
     * provisioning service). The name is still set so any active advertising
     * shows the correct name.
     */
    if (s_prov.gatt_enabled) {
        syslog(LOG_INFO, "npi_gatt: ADV_START (connectable, gatt enabled)");
        return npi_send_cmd(NPI_GATT_CMD_ADV_START, NULL, 0);
    } else {
        syslog(LOG_INFO, "npi_gatt: ADV_STOP + DISCONNECT (gatt disabled in UCI)");
        r = npi_send_cmd(NPI_GATT_CMD_ADV_STOP, NULL, 0);
        if (r < 0) return r;
        usleep(20000);
        /* Also drop any phone already connected, so it loses read/write/notify
         * access — matching the BlueZ path where the services are unregistered. */
        return npi_send_cmd(NPI_GATT_CMD_DISCONNECT, NULL, 0);
    }
}

void npi_gatt_set_gatt_enabled(bool enabled)
{
    s_prov.gatt_enabled = enabled;
    syslog(LOG_INFO, "npi_gatt: gatt_server.enabled = %s", enabled ? "yes" : "no");
}

int npi_gatt_query_ready(void)
{
    /*
     * Re-arm the handshake: allow the next READY to (re)push the provision
     * setup, then ask the firmware to emit READY. On first boot the firmware
     * emits READY on its own too, so this is harmless (we simply get the setup
     * pushed once). After a daemon restart this is the ONLY way to make the
     * already-provisioned firmware re-announce readiness.
     */
    s_prov.sent = false;
    syslog(LOG_INFO, "npi_gatt: QUERY_READY → asking firmware to re-emit READY");
    return npi_send_cmd(NPI_GATT_CMD_QUERY_READY, NULL, 0);
}
