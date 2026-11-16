/**
 * @file npi_gatt_handler.c
 * @brief NPI GATT Server bridge for the TI CC2652R1 (multi_role firmware).
 *
 * When the GATT server runs on the MCU, this module:
 *   1. receives NPI events when a phone writes/reads a characteristic,
 *   2. routes them into the SHARED transport-agnostic v2 logic (gatt_v2*.c) —
 *      byte-for-byte the same code the BlueZ path uses,
 *   3. sends NPI commands back for read responses and notifications.
 *
 * Only the uCentral Provisioning v2 service exists; FE00/FE10 are retired.
 *
 * The firmware must:
 *   - expose the v2 service with 128-bit UUIDs,
 *   - report the COMPLETE 16-byte characteristic UUID (BLE/little-endian order)
 *     in CHAR_WRITE / CHAR_READ (see npi_gatt_handler.h and §19.9),
 *   - accept NOTIFY / READ_RSP addressed the same way.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>

#include "npi_gatt_handler.h"
#include "app_status.h"
#include "ble_auth.h"
#include <libubox/uloop.h>

#include "gatt_v2.h"
#include "gatt_v2_cmd.h"
#include "gatt_v2_conn.h"
#include "gatt_v2_read.h"
#include "adv_status.h"
#include "../uci_app_config.h"
#include <ble.h>

/* Current connection state */
static struct {
    bool connected;
    uint16_t conn_handle;
    uint16_t mtu;
} s_conn = { .mtu = 23 };

/*
 * Feature level the firmware reported (0 = it never did).
 *
 * 0 is itself a diagnosis: the marker was added at level 2, so silence means the
 * chip is running something older. That distinction is the whole point — it was
 * previously only reachable by inferring from which side effects were missing.
 */
static uint8_t s_fw_level;

/* High byte of a connection interval whose low byte has not arrived yet. */
static uint8_t s_conn_interval_hi;

/* Provisioning setup params, pushed to the firmware when it signals READY. */
static struct {
    char     name[64];
    uint8_t  bdaddr[6];
    bool     have_addr;
    bool     valid;
    bool     sent;         /* true once provision setup pushed for this FW session */
    bool     gatt_enabled; /* mirror of UCI ble.gatt_server.enabled */
} s_prov;

/*
 * ATT MTU negotiation policy (UCI ble.gatt_server.mtu_request / mtu_size).
 *
 * An MTU exchange happens once per connection and only if one side initiates it.
 * Defaulting to "wait for the client" keeps the phone app in control and makes a
 * collision impossible; an operator whose app never asks can opt in.
 */
static struct {
    bool     request;      /* ask the peer ourselves after connecting */
    uint16_t size;         /* desired MTU, 0 = let the firmware use its maximum */
} s_mtu;

/** Delay before we ask, so a client that intends to negotiate goes first. */
#define MTU_REQUEST_DELAY_MS   1000

/*
 * How often the advertised status bytes are re-checked (§19.6).
 *
 * Only the uplink needs polling; a device_status change arrives through the
 * shared hook. Same interval the BlueZ path uses, so both transports refresh at
 * the same rate.
 */
#define ADV_REFRESH_INTERVAL_MS  5000

/* Manufacturer bytes the firmware currently advertises, so a refresh can tell a
 * real change from a no-op and not spend a UART frame on nothing. */
static uint8_t  s_adv_mfr[ADV_MFR_DATA_LEN];
static bool     s_adv_mfr_valid;
static struct uloop_timeout s_adv_timer;

/* ══════════════════════════════════════════════════════════════════
 *  v2 UUID handling
 *
 *  The service base is e063c3aX-0748-40be-935e-01ebdeafa814. On the wire the
 *  UUID is little-endian, so the printed string reverses: byte[12] carries the
 *  distinguishing nibble (0xA0 service, 0xA1..0xA7 characteristics).
 * ══════════════════════════════════════════════════════════════════ */

#define V2_UUID_SEL_INDEX   12

static const uint8_t V2_UUID_LE[NPI_GATT_UUID_LEN] = {
    0x14, 0xa8, 0xaf, 0xde, 0xeb, 0x01, 0x5e, 0x93,
    0xbe, 0x40, 0x48, 0x07, 0x00 /* selector */, 0xc3, 0x63, 0xe0
};

/** Selector byte for each characteristic, indexed by gatt_v2_chr_t. */
static const uint8_t V2_CHR_SELECTOR[] = {
    [GATT_V2_CHR_READ]    = 0xa1,
    [GATT_V2_CHR_WRITE]   = 0xa2,
    [GATT_V2_CHR_RESULT]  = 0xa3,
    [GATT_V2_CHR_STATUS]  = 0xa4,
    [GATT_V2_CHR_AUTH]    = 0xa5,
    [GATT_V2_CHR_MAC]     = 0xa6,
    [GATT_V2_CHR_UPGRADE] = 0xa7,
};

/**
 * Identify a characteristic from its 16-byte UUID.
 * @return the gatt_v2_chr_t, or -1 when the UUID is not part of the v2 service.
 */
static int v2_chr_from_uuid(const uint8_t uuid[NPI_GATT_UUID_LEN])
{
    for (int i = 0; i < NPI_GATT_UUID_LEN; i++) {
        if (i == V2_UUID_SEL_INDEX)
            continue;
        if (uuid[i] != V2_UUID_LE[i])
            return -1;
    }
    for (size_t c = 0; c < sizeof(V2_CHR_SELECTOR); c++) {
        if (V2_CHR_SELECTOR[c] == uuid[V2_UUID_SEL_INDEX])
            return (int)c;
    }
    return -1;
}

/** Build the 16-byte UUID for a characteristic. */
static void v2_uuid_for_chr(gatt_v2_chr_t chr, uint8_t out[NPI_GATT_UUID_LEN])
{
    memcpy(out, V2_UUID_LE, NPI_GATT_UUID_LEN);
    out[V2_UUID_SEL_INDEX] =
        ((size_t)chr < sizeof(V2_CHR_SELECTOR)) ? V2_CHR_SELECTOR[chr] : 0x00;
}

static const char *v2_chr_name(int chr)
{
    switch (chr) {
    case GATT_V2_CHR_READ:    return "A1/read";
    case GATT_V2_CHR_WRITE:   return "A2/write";
    case GATT_V2_CHR_RESULT:  return "A3/result";
    case GATT_V2_CHR_STATUS:  return "A4/status";
    case GATT_V2_CHR_AUTH:    return "A5/auth";
    case GATT_V2_CHR_MAC:     return "A6/mac";
    case GATT_V2_CHR_UPGRADE: return "A7/upgrade";
    default:                  return "unknown";
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  NPI frame building
 *
 *  SOF(0xFE) | LEN_LO | LEN_HI | CMD0(0xA2) | CMD1 | PAYLOAD | FCS
 *  FCS = XOR of LEN_LO .. last payload byte.
 *
 *  CHAR payload layout (both directions):
 *      conn_handle(2, LE) | uuid(16) | len(2, LE) | data(N)
 *  CHAR_READ from the MCU omits the len/data fields.
 * ══════════════════════════════════════════════════════════════════ */

#define NPI_CMD0_GATT_SREQ   ((0x05 << 5) | 0x02)   /* 0xA2 */

static int build_char_frame(uint8_t cmd1, uint16_t conn_handle,
                            const uint8_t uuid[NPI_GATT_UUID_LEN],
                            const uint8_t *data, uint16_t data_len,
                            uint8_t *out, uint16_t out_max)
{
    uint16_t payload_len = 2 + NPI_GATT_UUID_LEN + 2 + data_len;
    uint16_t frame_len = 1 + 2 + 2 + payload_len + 1;
    if (frame_len > out_max) return -ENOMEM;

    uint16_t idx = 0;
    out[idx++] = 0xFE;
    out[idx++] = (uint8_t)(payload_len & 0xFF);
    out[idx++] = (uint8_t)((payload_len >> 8) & 0xFF);
    out[idx++] = NPI_CMD0_GATT_SREQ;
    out[idx++] = cmd1;

    out[idx++] = (uint8_t)(conn_handle & 0xFF);
    out[idx++] = (uint8_t)((conn_handle >> 8) & 0xFF);
    memcpy(&out[idx], uuid, NPI_GATT_UUID_LEN);
    idx += NPI_GATT_UUID_LEN;
    out[idx++] = (uint8_t)(data_len & 0xFF);
    out[idx++] = (uint8_t)((data_len >> 8) & 0xFF);

    if (data_len && data) {
        memcpy(&out[idx], data, data_len);
        idx += data_len;
    }

    uint8_t fcs = 0;
    for (uint16_t i = 1; i < idx; i++)
        fcs ^= out[i];
    out[idx++] = fcs;

    return (int)idx;
}

/*
 * Build + send a generic NPI GATT command frame with a raw payload.
 */
static int npi_send_cmd(uint8_t cmd1, const uint8_t *payload, uint16_t plen)
{
    uint8_t frame[300];
    uint16_t frame_len = 1 + 2 + 2 + plen + 1;
    if (frame_len > sizeof(frame)) return -ENOMEM;

    uint16_t idx = 0;
    frame[idx++] = 0xFE;
    frame[idx++] = (uint8_t)(plen & 0xFF);
    frame[idx++] = (uint8_t)((plen >> 8) & 0xFF);
    frame[idx++] = NPI_CMD0_GATT_SREQ;
    frame[idx++] = cmd1;
    if (plen && payload) { memcpy(&frame[idx], payload, plen); idx += plen; }

    uint8_t fcs = 0;
    for (uint16_t i = 1; i < idx; i++) fcs ^= frame[i];
    frame[idx++] = fcs;

    return ble_ti_gatt_send_raw(frame, idx);
}

/* ══════════════════════════════════════════════════════════════════
 *  Outbound: notify + read response
 * ══════════════════════════════════════════════════════════════════ */

int npi_gatt_send_notify(uint16_t conn_handle,
                         const uint8_t uuid[NPI_GATT_UUID_LEN],
                         const uint8_t *data, uint16_t data_len)
{
    if (!s_conn.connected) return -ENOTCONN;

    /*
     * Values are plain JSON documents — the phone app expects them verbatim, so
     * we must NOT wrap them in the custom FIRST/LAST segment framing.
     *
     * We deliberately do NOT clamp to (ATT_MTU - 3) here. The firmware caches
     * whatever a notify carries and serves later reads from that cache, so
     * clamping on this side would truncate the readable value too — and it would
     * do so based on a guess, since the MTU we track is only as good as the
     * EVT_MTU the firmware may not have sent. The firmware knows the real
     * per-connection MTU and the TI stack already limits the notification to
     * MTU-3 when it reads the value out of the attribute table. Send the full
     * value and let the side that knows the truth do the limiting.
     */
    uint16_t send_len = data_len;
    if (send_len > NPI_GATT_VALUE_MAX) {
        send_len = NPI_GATT_VALUE_MAX;
        syslog(LOG_WARNING, "npi_gatt: value %u exceeds the firmware cache "
               "(%u) — truncating", data_len, NPI_GATT_VALUE_MAX);
    }

    uint8_t frame[600];
    int flen = build_char_frame(NPI_GATT_CMD_NOTIFY, conn_handle, uuid,
                               data, send_len, frame, sizeof(frame));
    if (flen < 0) return flen;
    return ble_ti_gatt_send_raw(frame, (uint16_t)flen);
}

int npi_gatt_send_read_rsp(uint16_t conn_handle,
                           const uint8_t uuid[NPI_GATT_UUID_LEN],
                           const uint8_t *data, uint16_t data_len)
{
    uint8_t frame[600];
    int flen = build_char_frame(NPI_GATT_CMD_READ_RSP, conn_handle, uuid,
                               data, data_len, frame, sizeof(frame));
    if (flen < 0) return flen;
    syslog(LOG_DEBUG, "npi_gatt: READ_RSP conn=%u len=%u",
           conn_handle, data_len);
    return ble_ti_gatt_send_raw(frame, (uint16_t)flen);
}

/**
 * The value a read of @chr must return, for the unfragmented characteristics.
 *
 * Single source of truth for two callers: answering a CHAR_READ the firmware
 * reported, and warming the firmware's value cache on connect. Keeping them on
 * one function is what stops a cached value from disagreeing with a live read.
 *
 * A1/A4 are NOT served here — they are fragmented and binary, and go through
 * gatt_v2_read_body() via npi_v2_answer_read() instead (§19.3).
 *
 * @param chr     characteristic index.
 * @param conn_id this link's identity; unused by the values served here, kept so
 *                the signature covers every characteristic uniformly.
 * @param macbuf  scratch buffer for the A6 value; must hold >= 13 bytes.
 * @param macsz   size of @macbuf.
 * @return a NUL-terminated string owned by the callee; never NULL.
 */
static const char *v2_read_body(int chr, const char *conn_id,
                                char *macbuf, size_t macsz)
{
    (void)conn_id;      /* the gated values moved to gatt_v2_read_body() */

    switch (chr) {
    case GATT_V2_CHR_MAC:
        /* A6: public, no auth. */
        return (gatt_v2_mac_hex(macbuf, macsz) > 0) ? macbuf : "";

    case GATT_V2_CHR_RESULT:
        /*
         * A3: the command-outcome channel the phone app subscribes to and reads.
         * Same stored value A2 mirrors.
         */
        return gatt_v2_result_json();

    case GATT_V2_CHR_WRITE:
        /* A2: mirror of the same outcome, for request/response on one attribute. */
        return gatt_v2_result_json();

    case GATT_V2_CHR_AUTH:
        /* A5: verdict of the last credential write. An extension — the reference
         * publishes no auth verdict anywhere. */
        return gatt_v2_auth_json();

    case GATT_V2_CHR_UPGRADE:
        return gatt_v2_upgrade_json();

    case GATT_V2_CHR_READ:
    case GATT_V2_CHR_STATUS:
        /*
         * Not answered here. A1/A4 are fragmented (§19.3): each read hands out
         * the next fragment of a compressed snapshot, which is binary and depends
         * on a per-connection cursor. They go through npi_v2_answer_read() →
         * gatt_v2_read_body() instead, and must never be staged in the firmware
         * cache — see npi_v2_warm_read_cache().
         */
        syslog(LOG_WARNING, "npi_gatt: v2_read_body called for a fragmented "
               "characteristic (chr=%d) — this is a bug", chr);
        return "";

    default:
        return "";
    }
}

/**
 * Answer one read the firmware reported, for any characteristic.
 *
 * A1/A4 are fragmented and binary; everything else is a single JSON (or hex)
 * string. Both end up in the same READ_RSP frame, so the firmware does not care
 * which kind it is forwarding.
 *
 * @return 0 on success, negative on a send failure.
 */
static int npi_v2_answer_read(uint16_t conn_handle,
                              const uint8_t uuid[NPI_GATT_UUID_LEN],
                              int chr, const char *cid)
{
    if (chr == GATT_V2_CHR_READ || chr == GATT_V2_CHR_STATUS) {
        gatt_v2_tx_chr_t which = (chr == GATT_V2_CHR_READ)
                               ? GATT_V2_TX_CONFIG : GATT_V2_TX_STATUS;

        const uint8_t *body = NULL;
        uint16_t body_len = 0;
        /*
         * Offset 0: the firmware only defers a read at offset 0, and answers a
         * blob read from its own copy of the fragment we last supplied.
         *
         * s_conn.mtu is what the firmware reported via EVT_MTU, so the fragment
         * is cut to fit one read response on this link. Before any exchange it is
         * 23, which is correct rather than optimistic — the link really can only
         * carry 22 bytes until the MTU goes up.
         */
        int ret = gatt_v2_read_body(cid, which, 0, s_conn.mtu, &body, &body_len);
        if (ret < 0 || !body) {
            /*
             * Answer anyway, with the same single 0x00 the shared module uses for
             * a blocked read. Sending nothing would leave the firmware holding a
             * pending ATT transaction until its timeout.
             */
            static const uint8_t blocked = 0x00;
            syslog(LOG_WARNING, "npi_gatt: %s read failed (%d) — answering 0x00",
                   gatt_v2_read_chr_name(which), ret);
            body = &blocked;
            body_len = 1;
        }
        return npi_gatt_send_read_rsp(conn_handle, uuid, body, body_len);
    }

    char mac[16];
    const char *body = v2_read_body(chr, cid, mac, sizeof(mac));
    return npi_gatt_send_read_rsp(conn_handle, uuid, (const uint8_t *)body,
                                  (uint16_t)strlen(body));
}

/**
 * Ask the firmware to start an ATT MTU exchange on this link.
 *
 * Runs on a short timer rather than straight from the CONNECTED event: the
 * exchange may only happen once per connection, so giving the client the first
 * move means enabling this option cannot break an app that negotiates on its own.
 */
static struct uloop_timeout s_mtu_timer;
static uint16_t s_mtu_conn;

static void mtu_request_cb(struct uloop_timeout *t)
{
    (void)t;
    if (!s_conn.connected) {
        syslog(LOG_DEBUG, "npi_gatt: MTU request skipped — link already gone");
        return;
    }
    if (s_conn.mtu > 23) {
        syslog(LOG_INFO, "npi_gatt: MTU request skipped — client already "
               "negotiated %u", s_conn.mtu);
        return;
    }

    uint8_t payload[4] = {
        (uint8_t)(s_mtu_conn & 0xFF), (uint8_t)((s_mtu_conn >> 8) & 0xFF),
        (uint8_t)(s_mtu.size & 0xFF), (uint8_t)((s_mtu.size >> 8) & 0xFF)
    };
    syslog(LOG_INFO, "npi_gatt: requesting ATT MTU %s on conn=%u",
           s_mtu.size ? "(configured)" : "(chip maximum)", s_mtu_conn);
    npi_send_cmd(NPI_GATT_CMD_REQ_MTU, payload, sizeof(payload));
}

/**
 * Push the advertisement manufacturer bytes to the firmware (§19.6).
 *
 * The firmware owns its advertisement payload, so the same four bytes the BlueZ
 * path writes into `LEAdvertisement1.ManufacturerData` have to travel over NPI
 * here. The builder is shared (`adv_status_mfr_data`), so both transports
 * advertise identical bytes.
 *
 * @param force send even when the bytes are unchanged. Two callers need this:
 *              the first push of a firmware session, where the firmware's state
 *              is unknown, and the disconnect handler, which wants the frame
 *              purely for its side effect — SET_ADV_MFR reloads the advertising
 *              set, which is what brings advertising back after a connection.
 */
static void npi_adv_mfr_push(bool force)
{
    uint8_t mfr[ADV_MFR_DATA_LEN];
    adv_status_mfr_data(mfr);

    if (!force && s_adv_mfr_valid &&
        memcmp(mfr, s_adv_mfr, ADV_MFR_DATA_LEN) == 0)
        return;

    uint8_t payload[2 + ADV_MFR_DATA_LEN];
    payload[0] = (uint8_t)(ADV_MFR_COMPANY_ID & 0xFF);
    payload[1] = (uint8_t)((ADV_MFR_COMPANY_ID >> 8) & 0xFF);
    memcpy(&payload[2], mfr, ADV_MFR_DATA_LEN);

    if (npi_send_cmd(NPI_GATT_CMD_SET_ADV_MFR, payload, sizeof(payload)) < 0) {
        syslog(LOG_WARNING, "npi_gatt: failed to push advertisement status");
        return;
    }

    memcpy(s_adv_mfr, mfr, ADV_MFR_DATA_LEN);
    s_adv_mfr_valid = true;
    syslog(LOG_INFO, "npi_gatt: advertisement status now %02X%02X%02X%02X",
           mfr[0], mfr[1], mfr[2], mfr[3]);
}

static void npi_adv_timer_cb(struct uloop_timeout *t)
{
    npi_adv_mfr_push(false);
    uloop_timeout_set(t, ADV_REFRESH_INTERVAL_MS);
}

/** device_status moved — republish now instead of waiting for the timer. */
static void npi_adv_status_changed_cb(uint8_t status)
{
    (void)status;
    npi_adv_mfr_push(false);
}

/**
 * Stage the current value of every cacheable readable characteristic.
 *
 * The firmware answers a read of A2/A3/A5/A6/A7 from its own cache — it cannot
 * wait for a round trip to the host without stalling the ATT transaction. An
 * unwarmed cache would make the phone's FIRST read of each of those return an
 * empty value, and on A6 (MAC) that is the very first thing the app asks for.
 *
 * **A1 and A4 are deliberately excluded.** They are fragmented (§19.3), so a
 * staged value would be fragment 0 of a snapshot nobody asked for; the cursor
 * would already have advanced and the phone's first real read would receive
 * fragment 1. The firmware therefore defers those two reads to us instead of
 * answering from cache.
 *
 * Called on connect, after the connection slot has been reset, so the staged
 * values reflect an unauthenticated session.
 */
static void npi_v2_warm_read_cache(uint16_t conn_handle)
{
    char cid[GATT_V2_CONN_ID_MAX];
    gatt_v2_conn_id_from_handle(cid, sizeof(cid), conn_handle);

    int staged = 0;
    for (int chr = 0; chr < (int)(sizeof(V2_CHR_SELECTOR)); chr++) {
        if (chr == GATT_V2_CHR_READ || chr == GATT_V2_CHR_STATUS)
            continue;

        uint8_t uuid[NPI_GATT_UUID_LEN];
        char mac[16];

        v2_uuid_for_chr((gatt_v2_chr_t)chr, uuid);
        const char *body = v2_read_body(chr, cid, mac, sizeof(mac));

        npi_gatt_send_read_rsp(conn_handle, uuid,
                               (const uint8_t *)body, (uint16_t)strlen(body));
        staged++;
    }
    syslog(LOG_DEBUG, "npi_gatt: staged %d readable values for conn=%u "
           "(A1/A4 excluded — fragmented)", staged, conn_handle);
}

/**
 * Shared notify hook for the TI transport: the v2 core calls this whenever an
 * A2/A7 value changes, and we forward it as an NPI notification. This is the TI
 * counterpart of the BlueZ PropertiesChanged emission.
 */
static void npi_v2_notify(gatt_v2_chr_t chr, const uint8_t *data, uint16_t len)
{
    if (!s_conn.connected)
        return;
    uint8_t uuid[NPI_GATT_UUID_LEN];
    v2_uuid_for_chr(chr, uuid);
    npi_gatt_send_notify(s_conn.conn_handle, uuid, data, len);
}

/* ══════════════════════════════════════════════════════════════════
 *  Inbound: bridge callback from libble
 * ══════════════════════════════════════════════════════════════════ */

static void npi_gatt_bridge_cb(uint8_t cmd1, const uint8_t *payload,
                               uint16_t plen, void *ctx)
{
    (void)ctx;
    npi_gatt_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.event_type = cmd1;

    /*
     * When provisioning GATT is disabled (UCI gatt_server.enabled=0), behave
     * like the BlueZ path where the service is not registered at all: reject
     * every characteristic read/write so an already-connected phone cannot drive
     * any action or read any value. Non-data events still flow so state stays
     * consistent.
     */
    if (!s_prov.gatt_enabled &&
        (cmd1 == NPI_GATT_EVT_CHAR_WRITE || cmd1 == NPI_GATT_EVT_CHAR_READ)) {
        syslog(LOG_INFO, "npi_gatt: GATT disabled — dropping char %s",
               (cmd1 == NPI_GATT_EVT_CHAR_WRITE) ? "write" : "read");
        return;
    }

    switch (cmd1) {
    case NPI_GATT_EVT_CHAR_WRITE: {
        const uint16_t hdr = 2 + NPI_GATT_UUID_LEN + 2;   /* conn + uuid + len */
        if (plen < hdr) {
            syslog(LOG_ERR, "npi_gatt: CHAR_WRITE too short (%u, need >=%u) — "
                   "firmware is not reporting 16-byte UUIDs; update the "
                   "multi_role firmware for v2 support", plen, hdr);
            return;
        }
        evt.conn_handle = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        memcpy(evt.uuid, &payload[2], NPI_GATT_UUID_LEN);
        evt.data_len = (uint16_t)payload[18] | ((uint16_t)payload[19] << 8);
        if (evt.data_len > sizeof(evt.data))
            evt.data_len = sizeof(evt.data);
        if (evt.data_len > (uint16_t)(plen - hdr))
            evt.data_len = (uint16_t)(plen - hdr);
        memcpy(evt.data, &payload[hdr], evt.data_len);
        break;
    }

    case NPI_GATT_EVT_CHAR_READ: {
        const uint16_t hdr = 2 + NPI_GATT_UUID_LEN;
        if (plen < hdr) {
            syslog(LOG_ERR, "npi_gatt: CHAR_READ too short (%u, need >=%u) — "
                   "firmware is not reporting 16-byte UUIDs; update the "
                   "multi_role firmware for v2 support", plen, hdr);
            return;
        }
        evt.conn_handle = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        memcpy(evt.uuid, &payload[2], NPI_GATT_UUID_LEN);
        break;
    }

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

    case NPI_GATT_EVT_NOTIFY_STATUS: {
        /*
         * Diagnostic: the outcome of a NOTIFY we sent. Whether a notification
         * reaches the phone is decided on-chip and would otherwise be invisible
         * here — GATTServApp_ProcessCharCfg() reports SUCCESS even when nobody
         * subscribed, and a CCCD write never crosses the UART.
         *
         * payload: uuid(16) status(1) cccd(1)
         */
        const uint16_t need = NPI_GATT_UUID_LEN + 2;
        if (plen < need) {
            syslog(LOG_WARNING, "npi_gatt: NOTIFY_STATUS too short (%u, need %u)",
                   plen, need);
            return;
        }
        int nchr = v2_chr_from_uuid(&payload[0]);
        uint8_t status = payload[NPI_GATT_UUID_LEN];
        uint8_t cccd   = payload[NPI_GATT_UUID_LEN + 1];

        if (cccd == 0) {
            /*
             * Not subscribed. On A5 this is the normal, correct state and must not
             * be reported as a problem: the reference publishes no auth verdict at
             * all (wifi_provision.py declares A5 write-only), so the shipped app
             * has no reason to subscribe, and the verdict we push there is our own
             * extension. The previous wording told the reader to "enable
             * notifications in the app", which is advice to break compatibility.
             *
             * On A3 and A7 a missing subscription is worth knowing about, because
             * those are the channels the app really does listen on.
             */
            if (nchr == GATT_V2_CHR_AUTH || nchr == GATT_V2_CHR_WRITE)
                syslog(LOG_DEBUG, "npi_gatt: notify on %s not delivered — nobody "
                       "subscribed, which is expected here", v2_chr_name(nchr));
            else
                syslog(LOG_WARNING, "npi_gatt: notify on %s was dropped — no "
                       "client subscribed (CCCD=0x00)", v2_chr_name(nchr));
        } else if (status != 0) {
            syslog(LOG_WARNING, "npi_gatt: notify on %s rejected by the stack "
                   "(status=%u, CCCD=0x%02X)", v2_chr_name(nchr), status, cccd);
        } else {
            syslog(LOG_INFO, "npi_gatt: notify on %s delivered (CCCD=0x%02X)",
                   v2_chr_name(nchr), cccd);
        }
        return;
    }

    case NPI_GATT_EVT_RX_ERROR: {
        /*
         * The firmware is telling us it dropped frames we sent. A non-zero
         * badfcs count means bytes went missing between here and the chip — the
         * usual cause is a UART RX ring buffer smaller than the largest frame,
         * which silently breaks long values (notifications, value staging) while
         * short ones keep working.
         */
        if (plen < 4) return;
        uint16_t badfcs   = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        uint16_t oversize = (uint16_t)payload[2] | ((uint16_t)payload[3] << 8);
        syslog(LOG_ERR, "npi_gatt: firmware dropped frames — bad FCS=%u, "
               "oversize=%u. Bytes are being lost on the UART; check the "
               "firmware's RX ring buffer size against the largest frame.",
               badfcs, oversize);
        return;
    }

    case NPI_GATT_EVT_CMD_STATUS: {
        /*
         * Outcome of a command the stack could refuse. Reported so a refusal is
         * distinguishable from a frame that never arrived.
         */
        if (plen < 2) return;
        uint8_t cmd = payload[0];
        uint8_t st  = payload[1];

        /*
         * Not a command outcome: the firmware announcing what it can do. Handled
         * before the generic path because "status" is a version here, so a
         * non-zero value is normal rather than a failure.
         */
        if (cmd == NPI_GATT_FW_LEVEL) {
            s_fw_level = st;
            if (st >= NPI_GATT_FW_LEVEL_MIN)
                syslog(LOG_INFO, "npi_gatt: firmware feature level %u "
                       "(this daemon needs >= %u)",
                       st, (unsigned)NPI_GATT_FW_LEVEL_MIN);
            else
                syslog(LOG_WARNING, "npi_gatt: firmware feature level %u is older "
                       "than this daemon needs (>= %u) — A1/A4 reads will be "
                       "answered from the chip's value cache and arrive one "
                       "fragment behind. Rebuild and reflash the firmware.",
                       st, (unsigned)NPI_GATT_FW_LEVEL_MIN);
            return;
        }

        /*
         * Also not a command outcome: the interval in force on this link, in
         * 1.25 ms units, arriving as high byte then low byte. Latch the high byte
         * and report when the low one lands.
         */
        if (cmd == NPI_GATT_CONN_INTERVAL_HI) {
            s_conn_interval_hi = st;
            return;
        }

        /*
         * Logged with an interpretation rather than bare, because the number
         * alone means nothing to most readers and the consequence is large: at
         * 24 units (30 ms) a 7-fragment A4 document spends well under a second in
         * connection events, and at 400 units (500 ms) it never finishes because
         * the phone gives up first.
         */
        if (cmd == NPI_GATT_CONN_INTERVAL) {
            unsigned units = ((unsigned)s_conn_interval_hi << 8) | st;
            s_conn_interval_hi = 0;

            /* units * 1.25 ms, kept in tenths of a ms to avoid floating point. */
            unsigned tenths = units * 125u / 10u;

            if (units > NPI_GATT_CONN_INTERVAL_SLOW)
                syslog(LOG_WARNING, "npi_gatt: connection interval = %u.%u ms "
                       "(%u units) — too slow to finish a fragmented read. Each "
                       "A1/A4 fragment costs two intervals, so the phone will "
                       "give up before the document completes. Disable the "
                       "connection-update request in the chip's SysConfig.",
                       tenths / 10, tenths % 10, units);
            else
                syslog(LOG_INFO, "npi_gatt: connection interval = %u.%u ms "
                       "(%u units)", tenths / 10, tenths % 10, units);
            return;
        }

        if (st == 0)
            syslog(LOG_INFO, "npi_gatt: firmware accepted cmd 0x%02X", cmd);
        else if (cmd == NPI_GATT_CMD_REQ_MTU)
            syslog(LOG_WARNING, "npi_gatt: MTU exchange refused (status=%u) — "
                   "the client most likely negotiated already, which is "
                   "harmless", st);
        else
            syslog(LOG_WARNING, "npi_gatt: firmware rejected cmd 0x%02X "
                   "(status=%u)", cmd, st);
        return;
    }

    case NPI_GATT_EVT_READY:
        /*
         * Firmware finished advertising init (advHandle valid). Push the
         * provisioning setup now so the name update lands after the default
         * scan response is loaded.
         */
        if (!s_prov.valid) {
            syslog(LOG_INFO, "npi_gatt: firmware READY (no provision params yet)");
            return;
        }
        if (s_prov.sent)
            syslog(LOG_DEBUG, "npi_gatt: firmware READY again — re-pushing provision setup");
        else
            syslog(LOG_INFO, "npi_gatt: firmware READY — pushing provision setup");
        npi_gatt_send_provision_setup(s_prov.name,
                                      s_prov.have_addr ? s_prov.bdaddr : NULL);
        s_prov.sent = true;

        /*
         * Push the advertisement status unconditionally: this is a fresh firmware
         * session (or a restarted daemon), so whatever we cached about the
         * firmware's payload no longer holds.
         */
        npi_adv_mfr_push(true);
        return;

    case NPI_GATT_EVT_BDADDR:
        /*
         * Firmware reports its actual (advertised) BD address — the chip's own
         * public address. Sync it into libble so the status file and the A6
         * characteristic report the MAC the phone actually scans.
         */
        if (plen >= 6) {
            syslog(LOG_INFO, "npi_gatt: firmware BD address (advertised) = "
                   "%02X:%02X:%02X:%02X:%02X:%02X",
                   payload[5], payload[4], payload[3],
                   payload[2], payload[1], payload[0]);
            extern int ble_ti_set_actual_bd_address(const uint8_t addr_le[6]);
            ble_ti_set_actual_bd_address(payload);
        }
        return;

    case 0x7E:
        /*
         * LOG_INFO, not LOG_DEBUG. The firmware picked a unique cmd1 for this
         * ack precisely so the host log can tell "the firmware really handled
         * SET_NAME" from a TX->RX loopback that would echo 0x20 instead. At
         * debug level that evidence is invisible at the default log level, and
         * PROV-005 could only ever skip — while telling the operator to go and
         * suspect the firmware. It fires once per firmware session.
         */
        syslog(LOG_INFO, "npi_gatt: firmware ACK 0x7E — SET_NAME handled");
        return;

    default:
        syslog(LOG_DEBUG, "npi_gatt: bridge cmd1=0x%02X plen=%u (ignored)",
               cmd1, plen);
        return;
    }

    npi_gatt_handle_event(&evt);
}

/* ══════════════════════════════════════════════════════════════════
 *  Event handling — routes into the shared v2 logic
 * ══════════════════════════════════════════════════════════════════ */

int npi_gatt_handle_event(const npi_gatt_event_t *evt)
{
    if (!evt) return -EINVAL;

    switch (evt->event_type) {
    /*
     * Authentication is per-connection (§19.4), so it is cleared on BOTH edges —
     * the same rule the BlueZ path applies via org.bluez.Device1.Connected.
     * Clearing on connect also protects against a missed disconnect event
     * leaving a fresh phone authenticated.
     */
    case NPI_GATT_EVT_CONNECTED: {
        char cid[GATT_V2_CONN_ID_MAX];
        gatt_v2_conn_id_from_handle(cid, sizeof(cid), evt->conn_handle);

        s_conn.connected = true;
        s_conn.conn_handle = evt->conn_handle;
        s_conn.mtu = 23;               /* until the firmware reports otherwise */

        /*
         * Say it on every connect, not once at start-up: this is the log people
         * actually capture when something misbehaves, and a chip running older
         * firmware than the daemon changes what the symptoms mean.
         */
        if (s_fw_level == 0)
            syslog(LOG_WARNING, "npi_gatt: the firmware has not reported a feature "
                   "level — it predates the marker, so it is older than level %u "
                   "and A1/A4 reads will arrive one fragment behind. Rebuild and "
                   "reflash from Application/gatt_provision.c.",
                   (unsigned)NPI_GATT_FW_LEVEL_MIN);

        /*
         * Drop then re-create this link's state. Dropping first matters: the
         * firmware reuses connection handles, so a stale slot could otherwise
         * hand a new phone the previous one's authentication and its
         * half-received configuration.
         */
        gatt_v2_conn_drop(cid);
        gatt_v2_conn_get(cid);
        gatt_v2_auth_reset();

        /* Stage every readable value so the phone's first read is not empty. */
        npi_v2_warm_read_cache(evt->conn_handle);
        if (s_mtu.request) {
            s_mtu_conn = evt->conn_handle;
            uloop_timeout_set(&s_mtu_timer, MTU_REQUEST_DELAY_MS);
        }
        return 0;
    }

    case NPI_GATT_EVT_DISCONNECTED: {
        char cid[GATT_V2_CONN_ID_MAX];
        gatt_v2_conn_id_from_handle(cid, sizeof(cid), evt->conn_handle);

        s_conn.connected = false;
        uloop_timeout_cancel(&s_mtu_timer);
        gatt_v2_conn_drop(cid);
        gatt_v2_auth_reset();

        /*
         * Re-arm advertising ourselves, instead of trusting the chip to do it.
         *
         * A connectable peripheral stops advertising the moment a central
         * connects, so something has to turn it back on when the link drops.
         * The firmware does try: `GAP_LINK_TERMINATED_EVENT` calls
         * `UartApp_evtDisconnected()` — the event being handled right here —
         * and then `multi_role_advEnableIfAllowed()`. On hardware that is not
         * enough: after the app disconnected, the device could not be scanned
         * again.
         *
         * A bare ADV_START does not fix it either, and the reason is measured
         * rather than guessed. On eap104 the device was unscannable after a
         * disconnect even with `npi_gatt: ADV_START after disconnect` in the
         * log, yet `/etc/init.d/ble-provision restart` made it scannable
         * immediately. Same chip, same image, same `s_advAllowed` — so the
         * difference is in *which* GapAdv calls each path makes:
         *
         *   restart   SET_NAME / SET_ADV_MFR  ->  GapAdv_disable()
         *                                         GapAdv_loadByHandle()
         *                                         GapAdv_enable()        works
         *
         *   ADV_START MR_PROV_ADV_START       ->  GapAdv_enable()        silent
         *             (and the firmware's own                            no-op
         *              advEnableIfAllowed)
         *
         * The reload is what restarts advertising on this stack; a bare
         * `GapAdv_enable()` on a set the controller stopped at connect does
         * not, and its return value is unchecked in the firmware
         * (multi_role.c:1732 and :1249), so the failure is invisible. That also
         * explains the original defect: the firmware's auto-resume and a host
         * ADV_START are structurally the same call, so neither could work.
         *
         * The repair is to replay the whole sequence the restart performed,
         * rather than to pick out the command that looks responsible. That
         * distinction is the point: the restart proved the *sequence* works, but
         * it does not isolate which command restored the radio, and two of them
         * do a reload — SET_NAME reloads the scan response, SET_ADV_MFR reloads
         * the advertisement data. Betting on one would be the same kind of guess
         * that produced the bare-ADV_START attempt.
         *
         *   npi_gatt_send_provision_setup()   SET_NAME, SET_BDADDR, ADV_START
         *   npi_adv_mfr_push(true)            SET_ADV_MFR
         *
         * which is what the READY handler runs, and what
         * `/etc/init.d/ble-provision restart` was measured to fix.
         *
         * Cost is four short frames plus the two 20 ms waits inside
         * send_provision_setup(). Acceptable here: a disconnect has nothing
         * time-critical in flight, and that function already blocks for the
         * same 40 ms on every firmware READY.
         *
         * Gated on `gatt_enabled` by this caller as well as inside
         * send_provision_setup(): when provisioning is off in UCI the daemon has
         * deliberately sent ADV_STOP, and a disconnect must not resurrect a
         * device that is supposed to be invisible. Keeping the outer guard means
         * the disabled case stays a quiet debug line instead of re-sending
         * ADV_STOP + DISCONNECT on every link drop. The firmware agrees —
         * SET_ADV_MFR re-enables through `multi_role_advEnableIfAllowed()`,
         * which honours the same flag.
         *
         * Unlike ADV_START, SET_ADV_MFR is answered with a command status, so
         * the outcome is visible (`firmware accepted cmd 0x2B`) rather than
         * taken on faith. That is the line worth asserting in a test.
         *
         * The firmware deserves the same fix (reload rather than a bare enable,
         * and check the status in both places), but that needs a CCS build and a
         * reflash of every unit, so this deliberately does not depend on it.
         */
        if (s_prov.gatt_enabled && s_prov.valid) {
            syslog(LOG_INFO, "npi_gatt: re-arming advertising after disconnect "
                             "(replaying provision setup + adv-data reload)");
            int r = npi_gatt_send_provision_setup(
                        s_prov.name, s_prov.have_addr ? s_prov.bdaddr : NULL);
            if (r < 0)
                syslog(LOG_WARNING, "npi_gatt: re-arm after disconnect failed "
                                    "(%d) — the device may not be discoverable "
                                    "until the daemon restarts", r);

            /*
             * The part that actually re-starts the radio. Forced, because the
             * bytes have not changed and the cache would otherwise skip it —
             * here the frame is wanted for its reload side effect, not to
             * publish new data.
             */
            npi_adv_mfr_push(true);
        } else if (s_prov.gatt_enabled) {
            syslog(LOG_WARNING, "npi_gatt: disconnect before provision params "
                                "were set — cannot re-arm advertising");
        } else {
            syslog(LOG_DEBUG, "npi_gatt: disconnect with gatt disabled — "
                              "leaving advertising off");
        }
        return 0;
    }

    case NPI_GATT_EVT_MTU_EXCHANGE:
        /*
         * Informational only: the firmware limits a notification to MTU-3 itself
         * (it is the side that knows the real value), so nothing here depends on
         * this. It is logged because it is the only visibility we have into what
         * the phone negotiated — a value stuck at 23 explains short
         * notifications, while reads still return the full value.
         */
        s_conn.mtu = evt->mtu;
        syslog(LOG_INFO, "npi_gatt: ATT MTU = %u (notifications carry up to %u bytes)",
               s_conn.mtu, (s_conn.mtu > 3) ? (unsigned)(s_conn.mtu - 3) : 0u);

        /*
         * Warn once the MTU is known, because a small one breaks the *write*
         * direction in a way we cannot compensate for. Our reads adapt (the
         * fragment size follows the MTU, §19.3), but the phone app cuts its
         * configuration into fixed 244-byte fragments — the reference's
         * MAX_CHUNK is a constant — and an ATT Write Request can only carry
         * MTU-3 bytes. Below 247 those writes simply cannot be sent.
         *
         * The fix is on the chip: SysConfig MAX_PDU_SIZE, whose default of 69
         * yields exactly the 65 seen on eap111 (69 - 4 for the L2CAP header).
         */
        if (s_conn.mtu < V2_FRAG_ATT_MTU)
            syslog(LOG_WARNING, "npi_gatt: ATT MTU %u is below %u — reads will be "
                   "split into smaller fragments, but the app's %u-byte config "
                   "writes cannot fit (raise MAX_PDU_SIZE in the firmware's "
                   "SysConfig to %u)",
                   s_conn.mtu, V2_FRAG_ATT_MTU,
                   V2_FRAG_CUSTOM_HEADER + V2_FRAG_MAX_CHUNK,
                   V2_FRAG_ATT_MTU + 4);
        return 0;

    case NPI_GATT_EVT_CHAR_WRITE:
    case NPI_GATT_EVT_CHAR_READ:
        break;                          /* handled below */

    default:
        syslog(LOG_DEBUG, "npi_gatt: unknown event type %u", evt->event_type);
        return 0;
    }

    int chr = v2_chr_from_uuid(evt->uuid);
    if (chr < 0) {
        syslog(LOG_WARNING, "npi_gatt: %s for a non-v2 UUID — ignored "
               "(the legacy FE00/FE10 services are retired)",
               (evt->event_type == NPI_GATT_EVT_CHAR_WRITE) ? "write" : "read");
        return 0;
    }

    /* This link's authentication and reassembly state (§19.4). */
    char cid[GATT_V2_CONN_ID_MAX];
    gatt_v2_conn_id_from_handle(cid, sizeof(cid), evt->conn_handle);

    /* ── Writes ── */
    if (evt->event_type == NPI_GATT_EVT_CHAR_WRITE) {
        syslog(LOG_DEBUG, "npi_gatt: write %s len=%u",
               v2_chr_name(chr), evt->data_len);

        switch (chr) {
        case GATT_V2_CHR_AUTH: {
            /* A5: plaintext credential checked against /etc/shadow root. */
            char cred[128];
            size_t clen = (evt->data_len < sizeof(cred) - 1)
                        ? evt->data_len : sizeof(cred) - 1;
            memcpy(cred, evt->data, clen);
            cred[clen] = '\0';

            bool ok = ble_auth_verify_root_password(cred);
            memset(cred, 0, sizeof(cred));      /* wipe promptly */

            /* Authentication belongs to this link only. */
            gatt_v2_conn_set_authenticated(cid, ok);
            /* Publish the verdict on A5 itself — same shape as the BlueZ path. */
            gatt_v2_auth_set(ok);
            return 0;
        }

        case GATT_V2_CHR_WRITE:
            /* A2: the shared dispatcher owns commands, auth policy, config
             * reassembly and the outcome reporting — identical to BlueZ. */
            gatt_v2_cmd_handle_write(evt->data, evt->data_len,
                                     gatt_v2_conn_get(cid));
            return 0;

        default:
            syslog(LOG_WARNING, "npi_gatt: write to non-writable %s — ignored",
                   v2_chr_name(chr));
            return 0;
        }
    }

    /* ── Reads ── */
    syslog(LOG_DEBUG, "npi_gatt: read %s", v2_chr_name(chr));

    /*
     * For A2/A3/A5/A6/A7 this refreshes the firmware's cache for the *next* read;
     * the current one was already answered locally. For A1/A4 the firmware is
     * holding the ATT transaction open waiting for exactly this frame, which is
     * what makes fragmented reads possible at all (§19.9).
     */
    return npi_v2_answer_read(evt->conn_handle, evt->uuid, chr, cid);
}

/* ══════════════════════════════════════════════════════════════════
 *  Lifecycle + provisioning handshake
 * ══════════════════════════════════════════════════════════════════ */

int npi_gatt_handler_init(void)
{
    memset(&s_conn, 0, sizeof(s_conn));
    s_conn.mtu = 23;

    /* Register with libble so multi_role GATT frames reach us. */
    ble_ti_set_gatt_bridge_cb(npi_gatt_bridge_cb, NULL);

    /* Route shared A2/A7 updates out as NPI notifications. */
    gatt_v2_set_notify_fn(npi_v2_notify);

    /*
     * MTU policy. Default ON, changed after measuring on eap111: the shipped app
     * starts an exchange on some connections and not on others, and an exchange
     * may only happen once per connection — so a link that nobody negotiates
     * stays at 23 bytes, where a configuration write cannot be sent at all.
     *
     * Asking anyway is safe: the request runs on a short timer, and
     * mtu_request_cb() skips it when the client has already negotiated.
     *
     * The compiled-in fallback matches files/ble.uci so a device whose UCI file
     * is missing behaves the same as a fresh install.
     */
    s_mtu.request = uci_app_get_bool("ble", "gatt_server", "mtu_request", true);
    /* 0 = let the firmware use the chip maximum; otherwise a legal ATT MTU. */
    s_mtu.size = (uint16_t)uci_app_get_int("ble", "gatt_server", "mtu_size",
                                           0, 0, 517);
    if (s_mtu.size != 0 && s_mtu.size < 23) {
        syslog(LOG_WARNING, "npi_gatt: mtu_size %u is below the ATT minimum (23)"
               " — using the chip maximum instead", s_mtu.size);
        s_mtu.size = 0;
    }
    s_mtu_timer.cb = mtu_request_cb;

    /*
     * Advertisement status (§19.6). The timer catches uplink changes; the hook
     * makes a device_status transition visible immediately. The first real push
     * happens when the firmware reports READY, since before that there is
     * nothing listening.
     */
    /* Per-connection auth + config reassembly starts empty (§19.4). */
    gatt_v2_conn_reset_all();

    memset(s_adv_mfr, 0, sizeof(s_adv_mfr));
    s_adv_mfr_valid = false;
    s_adv_timer.cb = npi_adv_timer_cb;
    uloop_timeout_set(&s_adv_timer, ADV_REFRESH_INTERVAL_MS);
    gatt_v2_set_status_fn(npi_adv_status_changed_cb);

    syslog(LOG_INFO, "npi_gatt: handler initialized (v2 bridge, 16-byte UUIDs; "
           "mtu_request=%s mtu_size=%u%s)",
           s_mtu.request ? "yes" : "no", s_mtu.size,
           s_mtu.size ? "" : " (chip max)");
    return 0;
}

void npi_gatt_handler_deinit(void)
{
    uloop_timeout_cancel(&s_mtu_timer);
    uloop_timeout_cancel(&s_adv_timer);
    gatt_v2_conn_reset_all();
    gatt_v2_set_status_fn(NULL);
    ble_ti_set_gatt_bridge_cb(NULL, NULL);
    gatt_v2_set_notify_fn(NULL);
    s_conn.connected = false;
    syslog(LOG_INFO, "npi_gatt: handler deinitialized");
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
     * Gate connectable advertising on UCI ble.gatt_server.enabled, mirroring the
     * BlueZ path (which only registers the service + advertises when enabled).
     */
    if (s_prov.gatt_enabled) {
        syslog(LOG_INFO, "npi_gatt: ADV_START (connectable, gatt enabled)");
        return npi_send_cmd(NPI_GATT_CMD_ADV_START, NULL, 0);
    }

    syslog(LOG_INFO, "npi_gatt: ADV_STOP + DISCONNECT (gatt disabled in UCI)");
    r = npi_send_cmd(NPI_GATT_CMD_ADV_STOP, NULL, 0);
    if (r < 0) return r;
    usleep(20000);
    /* Drop any phone already connected so it loses access, matching BlueZ where
     * the service is simply not registered. */
    return npi_send_cmd(NPI_GATT_CMD_DISCONNECT, NULL, 0);
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
     * emits READY on its own, so this is harmless; after a daemon restart it is
     * the only way to make an already-provisioned firmware re-announce.
     */
    s_prov.sent = false;
    syslog(LOG_INFO, "npi_gatt: QUERY_READY → asking firmware to re-emit READY");
    return npi_send_cmd(NPI_GATT_CMD_QUERY_READY, NULL, 0);
}
