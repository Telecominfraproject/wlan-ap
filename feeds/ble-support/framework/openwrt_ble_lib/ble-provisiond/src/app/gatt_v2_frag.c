/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file gatt_v2_frag.c
 * @brief Fragment + zlib + md5 framing for GATT provisioning v2 (§19.3).
 *
 * See gatt_v2_frag.h for the wire format. Compression uses zlib's `compress2` /
 * `uncompress`, which produce and consume the zlib container (2-byte header plus
 * an Adler-32 trailer) — the same thing Python's `zlib.compress` /
 * `zlib.decompress` do. Raw deflate would NOT interoperate.
 */

#define _GNU_SOURCE

#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include <zlib.h>
#include <libubox/md5.h>

#include "gatt_v2_frag.h"

/* ── Helpers ── */

static void md5_of(const void *data, size_t len, uint8_t out[16])
{
    md5_ctx_t ctx;
    md5_begin(&ctx);
    md5_hash(data, len, &ctx);
    md5_end(out, &ctx);
}

static uint32_t be32_read(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void be32_write(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void mono_now(struct timespec *ts)
{
    if (clock_gettime(CLOCK_MONOTONIC, ts) != 0) {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

/* ══════════════════════════════════════════════════════════════════
 *  Reassembly
 * ══════════════════════════════════════════════════════════════════ */

void v2_frag_rx_reset(v2_frag_rx_t *rx)
{
    if (!rx) return;
    rx->active = false;
    rx->total = 0;
    rx->count = 0;
    rx->stride = 0;
    rx->last_len = 0;
    memset(rx->got, 0, sizeof(rx->got));
    rx->raw_len = 0;
    rx->raw[0] = '\0';
}

/**
 * Verify and inflate a fully assembled payload.
 *
 * Order matters: inflate first, then compare the length and digest against the
 * trailer. Checking the digest of the *compressed* bytes would pass a payload
 * that inflates to something else entirely.
 */
static v2_frag_result_t rx_finish(v2_frag_rx_t *rx, uint32_t payload_len)
{
    if (payload_len <= V2_FRAG_TAIL_LEN) {
        syslog(LOG_WARNING, "v2_frag: payload %u bytes is too short to hold the "
               "%u-byte trailer", payload_len, (unsigned)V2_FRAG_TAIL_LEN);
        return V2_FRAG_VERIFY;
    }

    uint32_t comp_len = payload_len - V2_FRAG_TAIL_LEN;
    const uint8_t *tail = &rx->payload[comp_len];
    uint32_t want_len = be32_read(tail);            /* big-endian, see header */
    const uint8_t *want_md5 = tail + 4;

    if (want_len > V2_FRAG_RAW_MAX) {
        syslog(LOG_WARNING, "v2_frag: declared raw length %u exceeds the %u-byte "
               "cap", want_len, (unsigned)V2_FRAG_RAW_MAX);
        return V2_FRAG_VERIFY;
    }

    uLongf out_len = V2_FRAG_RAW_MAX;
    int zret = uncompress((Bytef *)rx->raw, &out_len,
                          (const Bytef *)rx->payload, (uLong)comp_len);
    if (zret != Z_OK) {
        syslog(LOG_WARNING, "v2_frag: inflate failed (zlib %d)", zret);
        return V2_FRAG_VERIFY;
    }

    if ((uint32_t)out_len != want_len) {
        syslog(LOG_WARNING, "v2_frag: length mismatch — inflated %lu, trailer "
               "says %u", (unsigned long)out_len, want_len);
        return V2_FRAG_VERIFY;
    }

    uint8_t digest[16];
    md5_of(rx->raw, out_len, digest);
    if (memcmp(digest, want_md5, 16) != 0) {
        syslog(LOG_WARNING, "v2_frag: md5 mismatch on %lu inflated bytes",
               (unsigned long)out_len);
        return V2_FRAG_VERIFY;
    }

    rx->raw_len = (uint32_t)out_len;
    rx->raw[out_len] = '\0';
    syslog(LOG_INFO, "v2_frag: reassembled %u fragments -> %u bytes of JSON",
           rx->total, rx->raw_len);
    return V2_FRAG_COMPLETE;
}

v2_frag_result_t v2_frag_rx_feed(v2_frag_rx_t *rx,
                                 const uint8_t *data, uint16_t len)
{
    if (!rx || !data || len < V2_FRAG_CUSTOM_HEADER)
        return V2_FRAG_INVALID;

    uint8_t total = data[0];
    uint8_t idx   = data[1];
    const uint8_t *chunk = data + V2_FRAG_CUSTOM_HEADER;
    uint16_t chunk_len = (uint16_t)(len - V2_FRAG_CUSTOM_HEADER);

    if (total == 0 || idx >= total) {
        syslog(LOG_WARNING, "v2_frag: bad header total=%u index=%u", total, idx);
        return V2_FRAG_INVALID;
    }
    /*
     * No separate fragment-count limit: `total` is a byte, so it cannot exceed
     * the V2_FRAG_MAX_FRAGS (255) bitmap, and the only limit that matters is the
     * payload buffer — checked per fragment below, where the offset is known.
     * A count check derived from the 242-byte stride would wrongly refuse a
     * client that fragmented for a smaller MTU.
     */

    /*
     * index 0 starts a fresh message. The reference does the same: a client that
     * restarts a transfer must not be merged with its own abandoned attempt.
     */
    if (idx == 0)
        v2_frag_rx_reset(rx);

    if (!rx->active) {
        if (idx != 0) {
            /* Mid-message fragment with no message in progress — the start was
             * lost or this is a leftover from a previous connection. */
            syslog(LOG_WARNING, "v2_frag: fragment %u/%u arrived with no message "
                   "in progress — dropping", idx, total);
            return V2_FRAG_INVALID;
        }
        rx->active = true;
        rx->total = total;
    } else if (rx->total != total) {
        syslog(LOG_WARNING, "v2_frag: fragment count changed mid-message "
               "(%u -> %u) — dropping", rx->total, total);
        v2_frag_rx_reset(rx);
        return V2_FRAG_INVALID;
    }

    /*
     * The stride is the size of a non-final fragment. It is taken from the
     * sender rather than assumed to be V2_FRAG_MAX_CHUNK, so a client that
     * fragments for a smaller MTU still interoperates; every non-final fragment
     * must then match it, which catches an inconsistent sender directly instead
     * of as a puzzling checksum failure later.
     */
    bool is_last = (idx == (uint8_t)(total - 1));

    if (!is_last) {
        if (rx->stride == 0)
            rx->stride = chunk_len;
        else if (chunk_len != rx->stride) {
            syslog(LOG_WARNING, "v2_frag: non-final fragment %u is %u bytes, "
                   "expected %u — dropping", idx, chunk_len, rx->stride);
            v2_frag_rx_reset(rx);
            return V2_FRAG_INVALID;
        }
    }

    /*
     * A single-fragment message has no stride of its own; use its own length so
     * the offset arithmetic below stays uniform.
     */
    uint16_t stride = rx->stride ? rx->stride : chunk_len;

    if (is_last && chunk_len > stride) {
        syslog(LOG_WARNING, "v2_frag: final fragment %u bytes exceeds the stride "
               "%u — dropping", chunk_len, stride);
        v2_frag_rx_reset(rx);
        return V2_FRAG_INVALID;
    }

    uint32_t offset = (uint32_t)idx * stride;
    if (offset + chunk_len > V2_FRAG_PAYLOAD_MAX) {
        syslog(LOG_WARNING, "v2_frag: fragment %u would write past the %u-byte "
               "payload buffer — dropping", idx, (unsigned)V2_FRAG_PAYLOAD_MAX);
        v2_frag_rx_reset(rx);
        return V2_FRAG_INVALID;
    }

    memcpy(&rx->payload[offset], chunk, chunk_len);
    if (is_last)
        rx->last_len = chunk_len;

    /* Count distinct fragments so a retransmitted one does not fake progress. */
    if (!rx->got[idx]) {
        rx->got[idx] = true;
        rx->count++;
    }

    if (rx->count < rx->total)
        return V2_FRAG_NEED_MORE;

    uint32_t payload_len = (uint32_t)(rx->total - 1) * stride + rx->last_len;
    v2_frag_result_t res = rx_finish(rx, payload_len);

    /*
     * Whatever the verdict, the message is done with. Keep `raw` on success so
     * the caller can read it; the buffers are reset by the caller's
     * v2_frag_rx_reset() once it has consumed the JSON.
     */
    rx->active = false;
    if (res != V2_FRAG_COMPLETE) {
        rx->raw_len = 0;
        rx->raw[0] = '\0';
    }
    return res;
}

const char *v2_frag_rx_json(const v2_frag_rx_t *rx, uint32_t *len_out)
{
    if (!rx || rx->raw_len == 0)
        return NULL;
    if (len_out)
        *len_out = rx->raw_len;
    return rx->raw;
}

/* ══════════════════════════════════════════════════════════════════
 *  Fragmented output
 * ══════════════════════════════════════════════════════════════════ */

uint16_t v2_frag_stride_for_mtu(uint16_t mtu)
{
    if (mtu < V2_FRAG_MIN_MTU)
        mtu = V2_FRAG_MIN_MTU;       /* also covers "not known yet" (0) */

    uint16_t stride = (uint16_t)(mtu - V2_FRAG_ATT_HEADER -
                                 V2_FRAG_CUSTOM_HEADER);
    if (stride > V2_FRAG_MAX_CHUNK)
        stride = V2_FRAG_MAX_CHUNK;
    return stride;
}

void v2_frag_tx_reset(v2_frag_tx_t *tx)
{
    if (!tx) return;
    tx->have = false;
    tx->total = 0;
    tx->idx = 0;
    tx->stride = 0;
    tx->payload_len = 0;
}

bool v2_frag_tx_have(const v2_frag_tx_t *tx)
{
    return tx && tx->have && tx->idx < tx->total;
}

bool v2_frag_tx_stale(const v2_frag_tx_t *tx)
{
    if (!tx || !tx->have)
        return false;

    struct timespec now;
    mono_now(&now);
    return (now.tv_sec - tx->last_read.tv_sec) >= V2_FRAG_STALE_SEC;
}

int v2_frag_tx_build(v2_frag_tx_t *tx, const char *json, uint16_t stride)
{
    if (!tx || !json)
        return -EINVAL;

    size_t raw_len = strlen(json);
    if (raw_len == 0 || raw_len > V2_FRAG_TX_RAW_MAX)
        return -EINVAL;

    /* Clamp rather than reject: a caller that miscomputes the stride should get
     * a working sequence, not a failed read. */
    if (stride < V2_FRAG_MIN_CHUNK)
        stride = V2_FRAG_MIN_CHUNK;
    else if (stride > V2_FRAG_MAX_CHUNK)
        stride = V2_FRAG_MAX_CHUNK;

    /*
     * Compress straight into the payload buffer, leaving room for the trailer.
     * zlib can expand incompressible input, so the bound is checked rather than
     * assumed.
     */
    uLongf comp_cap = V2_FRAG_PAYLOAD_MAX - V2_FRAG_TAIL_LEN;
    uLongf comp_len = comp_cap;
    int zret = compress2((Bytef *)tx->payload, &comp_len,
                         (const Bytef *)json, (uLong)raw_len, 9);
    if (zret == Z_BUF_ERROR) {
        syslog(LOG_WARNING, "v2_frag: %zu bytes of JSON do not fit in the %u-byte "
               "payload buffer once compressed", raw_len,
               (unsigned)V2_FRAG_PAYLOAD_MAX);
        return -EMSGSIZE;
    }
    if (zret != Z_OK) {
        syslog(LOG_WARNING, "v2_frag: deflate failed (zlib %d)", zret);
        return -EIO;
    }

    uint8_t *tail = &tx->payload[comp_len];
    be32_write(tail, (uint32_t)raw_len);            /* big-endian, see header */
    md5_of(json, raw_len, tail + 4);

    tx->payload_len = (uint32_t)comp_len + V2_FRAG_TAIL_LEN;

    uint32_t total = (tx->payload_len + stride - 1) / stride;
    if (total == 0 || total > 255) {
        /*
         * Reachable at a small MTU: 255 fragments of 18 bytes is only ~4.5 KB.
         * Saying so is more useful than a bare size error, because the fix is to
         * raise the MTU rather than to shrink the document.
         */
        syslog(LOG_WARNING, "v2_frag: %u payload bytes would need %u fragments at "
               "a %u-byte stride — beyond the 255 the format allows",
               tx->payload_len, total, stride);
        return -EMSGSIZE;
    }

    tx->stride = stride;
    tx->total = (uint8_t)total;
    tx->idx = 0;
    tx->have = true;
    mono_now(&tx->last_read);

    syslog(LOG_DEBUG, "v2_frag: %zu bytes of JSON -> %u payload bytes in %u "
           "fragments of %u", raw_len, tx->payload_len, tx->total, stride);
    return 0;
}

const uint8_t *v2_frag_tx_next(v2_frag_tx_t *tx, uint16_t *len_out)
{
    if (!v2_frag_tx_have(tx))
        return NULL;

    uint32_t offset = (uint32_t)tx->idx * tx->stride;
    uint32_t remain = tx->payload_len - offset;
    uint16_t chunk_len = (remain > tx->stride) ? tx->stride : (uint16_t)remain;

    tx->frag[0] = tx->total;
    tx->frag[1] = tx->idx;
    memcpy(&tx->frag[V2_FRAG_CUSTOM_HEADER], &tx->payload[offset], chunk_len);

    tx->idx++;
    tx->frag_len = (uint16_t)(V2_FRAG_CUSTOM_HEADER + chunk_len);
    mono_now(&tx->last_read);

    /*
     * Single-shot: after the final fragment the sequence is dropped so the next
     * read takes a fresh snapshot. A client that wants the document again reads
     * from the start, which is what the reference does too.
     *
     * v2_frag_tx_reset() leaves `frag`/`frag_len` alone on purpose, so a Read
     * Blob for the remainder of this last fragment can still be answered.
     */
    if (tx->idx >= tx->total)
        v2_frag_tx_reset(tx);

    if (len_out)
        *len_out = tx->frag_len;
    return tx->frag;
}

const uint8_t *v2_frag_tx_last(const v2_frag_tx_t *tx, uint16_t *len_out)
{
    if (!tx || tx->frag_len == 0)
        return NULL;
    if (len_out)
        *len_out = tx->frag_len;
    return tx->frag;
}
