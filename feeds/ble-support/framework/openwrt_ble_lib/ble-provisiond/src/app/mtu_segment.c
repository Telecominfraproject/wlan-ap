/**
 * @file mtu_segment.c
 * @brief MTU-based segmentation and reassembly implementation.
 *
 * Provides chunked transfer for BLE GATT payloads that exceed ATT_MTU - 3.
 * Used by gatt_server_app for WiFi config JSON, OTA URLs, scan results, etc.
 */

#include <string.h>
#include <syslog.h>

#include "mtu_segment.h"

/* ══════════════════════════════════════════════════════════════════
 *  Reassembly
 * ══════════════════════════════════════════════════════════════════ */

void mtu_reassembly_init(mtu_reassembly_t *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

void mtu_reassembly_reset(mtu_reassembly_t *ctx)
{
    if (!ctx) return;
    ctx->active = false;
    ctx->total_expected = 0;
    ctx->received_len = 0;
    ctx->next_seq = 0;
}

int mtu_reassembly_feed(mtu_reassembly_t *ctx, const uint8_t *segment,
                        uint16_t seg_len)
{
    if (!ctx || !segment || seg_len < MTU_SEG_HEADER_SIZE + 1)
        return -1;

    uint8_t header = segment[0];
    uint8_t seq = header & MTU_SEG_SEQ_MASK;
    bool is_first = (header & MTU_SEG_FLAG_FIRST) != 0;
    bool is_last = (header & MTU_SEG_FLAG_LAST) != 0;

    const uint8_t *payload;
    uint16_t payload_len;

    if (is_first) {
        /* First segment: header(1) + total_len(2) + payload */
        if (seg_len < MTU_SEG_HEADER_SIZE + MTU_SEG_FIRST_EXTRA + 1)
            return -1;

        uint16_t total_len = (uint16_t)segment[1] |
                             ((uint16_t)segment[2] << 8);

        if (total_len == 0 || total_len > MTU_SEG_MAX_PAYLOAD) {
            syslog(LOG_WARNING, "mtu_seg: invalid total_len=%u", total_len);
            return -1;
        }

        /* Start fresh reassembly */
        mtu_reassembly_reset(ctx);
        ctx->active = true;
        ctx->total_expected = total_len;
        ctx->next_seq = 0;

        payload = segment + MTU_SEG_HEADER_SIZE + MTU_SEG_FIRST_EXTRA;
        payload_len = seg_len - MTU_SEG_HEADER_SIZE - MTU_SEG_FIRST_EXTRA;
    } else {
        /* Continuation segment: header(1) + payload */
        if (!ctx->active) {
            syslog(LOG_WARNING, "mtu_seg: continuation without active reassembly");
            return -1;
        }

        payload = segment + MTU_SEG_HEADER_SIZE;
        payload_len = seg_len - MTU_SEG_HEADER_SIZE;
    }

    /* Sequence check */
    if (seq != ctx->next_seq) {
        syslog(LOG_WARNING, "mtu_seg: seq mismatch, expected=%u got=%u",
               ctx->next_seq, seq);
        mtu_reassembly_reset(ctx);
        return -1;
    }

    /* Overflow check */
    if ((uint32_t)ctx->received_len + payload_len > ctx->total_expected) {
        syslog(LOG_WARNING, "mtu_seg: overflow, received=%u + %u > total=%u",
               ctx->received_len, payload_len, ctx->total_expected);
        mtu_reassembly_reset(ctx);
        return -1;
    }

    /* Copy payload into buffer */
    memcpy(ctx->buffer + ctx->received_len, payload, payload_len);
    ctx->received_len += payload_len;
    ctx->next_seq = (ctx->next_seq + 1) & MTU_SEG_SEQ_MASK;

    if (is_last) {
        /* Verify we received expected amount */
        if (ctx->received_len != ctx->total_expected) {
            syslog(LOG_WARNING, "mtu_seg: incomplete, received=%u expected=%u",
                   ctx->received_len, ctx->total_expected);
            mtu_reassembly_reset(ctx);
            return -1;
        }
        ctx->active = false;
        return 1; /* Complete */
    }

    return 0; /* More expected */
}

/* ══════════════════════════════════════════════════════════════════
 *  Segmentation
 * ══════════════════════════════════════════════════════════════════ */

void mtu_segmenter_init(mtu_segmenter_t *ctx, const uint8_t *data,
                        uint16_t data_len, uint16_t att_mtu)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->data_len = data_len;
    ctx->mtu = (att_mtu >= 23) ? att_mtu : MTU_SEG_DEFAULT_ATT_MTU;
    ctx->offset = 0;
    ctx->seq = 0;
    ctx->done = (data == NULL || data_len == 0);
}

uint16_t mtu_segmenter_next(mtu_segmenter_t *ctx, uint8_t *out_buf,
                            uint16_t buf_size)
{
    if (!ctx || !out_buf || ctx->done)
        return 0;

    /* Maximum ATT payload per write/notify */
    uint16_t max_att_payload = ctx->mtu - 3;
    if (buf_size < max_att_payload)
        max_att_payload = buf_size;

    if (max_att_payload < MTU_SEG_HEADER_SIZE + 1)
        return 0; /* Buffer too small */

    bool is_first = (ctx->offset == 0);
    uint16_t header_overhead = MTU_SEG_HEADER_SIZE +
                               (is_first ? MTU_SEG_FIRST_EXTRA : 0);

    uint16_t available = max_att_payload - header_overhead;
    uint16_t remaining = ctx->data_len - ctx->offset;
    uint16_t chunk = (remaining < available) ? remaining : available;

    bool is_last = (ctx->offset + chunk >= ctx->data_len);

    /* Build header */
    uint8_t header = ctx->seq & MTU_SEG_SEQ_MASK;
    if (is_first) header |= MTU_SEG_FLAG_FIRST;
    if (is_last)  header |= MTU_SEG_FLAG_LAST;

    uint16_t pos = 0;
    out_buf[pos++] = header;

    if (is_first) {
        /* Total length (little-endian) */
        out_buf[pos++] = (uint8_t)(ctx->data_len & 0xFF);
        out_buf[pos++] = (uint8_t)((ctx->data_len >> 8) & 0xFF);
    }

    /* Payload */
    memcpy(out_buf + pos, ctx->data + ctx->offset, chunk);
    pos += chunk;

    ctx->offset += chunk;
    ctx->seq = (ctx->seq + 1) & MTU_SEG_SEQ_MASK;

    if (is_last)
        ctx->done = true;

    return pos;
}
