/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file gatt_v2_frag.h
 * @brief Fragment + zlib + md5 framing for GATT provisioning v2 (§19.3).
 *
 * The wire format is fixed by the phone app; this module implements it byte for
 * byte as the shipped reference does (`wifi_provision.py`, `zip_and_fragment`):
 *
 *     raw       = the JSON document, UTF-8, no whitespace
 *     payload   = zlib(raw, level 9) || raw_len(4, BIG-endian) || md5(raw)(16)
 *     fragment  = total(1) || index(1) || payload[index*stride ...]
 *
 * `raw_len` is big-endian (Python `struct.pack(">I", …)`) while every other
 * multi-byte field in this project is little-endian — a mismatch here produces a
 * plausible-looking but wrong length, so it is called out rather than assumed.
 *
 * Used in both directions:
 *
 *   - **Reassembly** on A2 writes: fragments arrive, are reassembled, verified
 *     against the length and digest, and inflated back to JSON.
 *   - **Fragmented output** on A1/A4 reads: a JSON snapshot is compressed once
 *     and handed out one fragment per read.
 *
 * This is NOT the FIRST/LAST `mtu_segment` scheme (§15). That one is ours; this
 * one is the app's, and the app is already shipped.
 */
#ifndef GATT_V2_FRAG_H
#define GATT_V2_FRAG_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Chunk size at the full ATT MTU: 247 - ATT_HEADER(3) - CUSTOM_HEADER(2).
 *
 * This is the reference's `MAX_CHUNK`, and the largest chunk we ever emit — but
 * NOT a fixed stride. The stride we actually use is derived from the negotiated
 * MTU (v2_frag_stride_for_mtu), because a fragment that does not fit the MTU is
 * silently truncated by the ATT layer and the client then fails a checksum on
 * bytes it never received. The reassembler likewise does not assume any
 * particular stride; it takes it from the first non-final fragment.
 */
#define V2_FRAG_ATT_MTU        247
#define V2_FRAG_ATT_HEADER     3
#define V2_FRAG_CUSTOM_HEADER  2
#define V2_FRAG_MAX_CHUNK      (V2_FRAG_ATT_MTU - V2_FRAG_ATT_HEADER - \
								V2_FRAG_CUSTOM_HEADER)   /* 242 */

/** Smallest ATT MTU the spec allows, and so the smallest we plan for. */
#define V2_FRAG_MIN_MTU        23

/** Chunk size at the minimum MTU: 23 - 3 - 2 = 18. */
#define V2_FRAG_MIN_CHUNK      (V2_FRAG_MIN_MTU - V2_FRAG_ATT_HEADER - \
								V2_FRAG_CUSTOM_HEADER)   /* 18 */

/**
 * The chunk size to emit on a link whose negotiated ATT MTU is @mtu.
 *
 * Uses the reference's own arithmetic, `mtu - ATT_HEADER - CUSTOM_HEADER`. The
 * ATT header is 3 rather than 1 because the same framing has to survive a Write
 * Request in the other direction, where the header really is 3 bytes; sizing to
 * the read-only limit would produce fragments the client could not echo back.
 *
 * @param mtu negotiated ATT MTU, or 0 when it is not known. Unknown is treated
 *            as the 23-byte default — more fragments, but every one of them
 *            deliverable, which is the safe direction when the alternative is
 *            silent truncation.
 * @return a chunk size in [V2_FRAG_MIN_CHUNK, V2_FRAG_MAX_CHUNK].
 */
uint16_t v2_frag_stride_for_mtu(uint16_t mtu);

/** Trailer appended to the compressed bytes: raw_len(4) + md5(16). */
#define V2_FRAG_TAIL_LEN       20

/**
 * Buffer caps.
 *
 * `total` and `index` are single bytes, so the format allows up to 255 fragments
 * (≈ 61 KB of payload). We cap far below that: a uCentral configuration is a few
 * KB of JSON and compresses well, and these are static buffers in a daemon on an
 * access point. A payload beyond the cap is rejected with a clear reason rather
 * than truncated into a checksum failure.
 */
#define V2_FRAG_PAYLOAD_MAX    8192
#define V2_FRAG_RAW_MAX        16384

/**
 * Sanity bound on a document we are about to send.
 *
 * Much larger than V2_FRAG_RAW_MAX because it buys no buffer: the send path
 * compresses straight from the caller's string, so the binding constraint is the
 * compressed size, which compress2() enforces by failing. The A4 status document
 * embeds /etc/ucentral/capabilities.json verbatim and can be tens of KB of
 * highly repetitive JSON that compresses to a fraction of that — capping the
 * *raw* size at the receive buffer's 16 KB would have refused documents that fit
 * the wire comfortably.
 */
#define V2_FRAG_TX_RAW_MAX     65536

/**
 * Size of the received-fragment bitmap: the most the format can express.
 *
 * `total` is a single byte, so 255 is the ceiling no matter what stride the
 * sender chose. Deriving this from V2_FRAG_PAYLOAD_MAX / V2_FRAG_MAX_CHUNK
 * instead (34) would have contradicted the stride flexibility above: a client
 * fragmenting for a 23-byte MTU sends ~20-byte chunks, so a 2 KB configuration
 * becomes ~100 fragments and would have been refused as "too many" even though
 * it fits the payload buffer easily. The real limit is the payload buffer, and
 * rx_feed() enforces exactly that per fragment. 255 bytes of bitmap is a cheap
 * price for not having a second, inconsistent limit.
 */
#define V2_FRAG_MAX_FRAGS      255

/**
 * A read sequence idle for this long is treated as abandoned and re-snapshotted.
 *
 * The reference uses 5 s, and copying that number was a mistake: it exists there
 * to stop one client's leftover fragments being handed to the *next* client,
 * because the reference keeps the cursor on the characteristic object where every
 * connection shares it. Our cursors are per connection (§19.4), so that splice is
 * structurally impossible and the rule has only one job left — freshness.
 *
 * Measured on eap111, the phone app reads one fragment every 2-6 s. A 4.4 KB
 * status document is 7 fragments, so a 5 s window threw the sequence away
 * mid-delivery almost every time and the app could essentially never assemble a
 * complete document: it would collect three or four fragments, get reset back to
 * index 0, and start over.
 *
 * 30 s is chosen to be longer than any plausible gap between two reads of the
 * same sequence while still bounding how stale a resumed document can be. The
 * trade is worth making in one direction only: a slightly older document that
 * arrives beats a perfectly fresh one that never completes. Staleness within a
 * sequence is inherent to fragmenting a snapshot anyway — the first fragment is
 * already seconds older than the last by the time it is read.
 */
#define V2_FRAG_STALE_SEC      30

/* ── Reassembly (A2 writes) ── */

typedef enum {
	V2_FRAG_NEED_MORE = 0,   /**< accepted, waiting for more fragments */
	V2_FRAG_COMPLETE  = 1,   /**< message complete and verified */
	V2_FRAG_INVALID   = -1,  /**< malformed header, inconsistent stride, too big */
	V2_FRAG_VERIFY    = -2,  /**< length or md5 mismatch, or inflate failed */
} v2_frag_result_t;

typedef struct {
	bool     active;                 /* a message is in progress */
	uint8_t  total;                  /* fragment count declared by the sender */
	uint8_t  count;                  /* distinct fragments received */
	uint16_t stride;                 /* size of a non-final fragment, 0 until known */
	uint16_t last_len;               /* size of the final fragment */
	bool     got[V2_FRAG_MAX_FRAGS]; /* duplicate detection */
	uint8_t  payload[V2_FRAG_PAYLOAD_MAX];
	char     raw[V2_FRAG_RAW_MAX + 1];  /* inflated JSON, NUL-terminated */
	uint32_t raw_len;
} v2_frag_rx_t;

/** Discard any partial message. Call on connect/disconnect and after handling. */
void v2_frag_rx_reset(v2_frag_rx_t *rx);

/**
 * Feed one A2 fragment.
 *
 * `index == 0` starts a fresh message and drops anything partial, matching the
 * reference — a client that retries from the beginning must not be merged with
 * its own abandoned attempt.
 *
 * @return V2_FRAG_COMPLETE when the message is assembled and verified; the JSON
 *         is then available from v2_frag_rx_json().
 */
v2_frag_result_t v2_frag_rx_feed(v2_frag_rx_t *rx,
								 const uint8_t *data, uint16_t len);

/**
 * The reassembled JSON after V2_FRAG_COMPLETE.
 *
 * @param len_out receives the byte count, may be NULL.
 * @return NUL-terminated JSON, or NULL when no complete message is held.
 */
const char *v2_frag_rx_json(const v2_frag_rx_t *rx, uint32_t *len_out);

/* ── Fragmented output (A1/A4 reads) ── */

typedef struct {
	bool     have;                   /* a snapshot is loaded */
	uint8_t  total;
	uint8_t  idx;                    /* next fragment to hand out */
	uint16_t stride;                 /* chunk size this snapshot was cut with */
	uint32_t payload_len;
	struct timespec last_read;       /* CLOCK_MONOTONIC */
	uint8_t  payload[V2_FRAG_PAYLOAD_MAX];
	uint8_t  frag[V2_FRAG_CUSTOM_HEADER + V2_FRAG_MAX_CHUNK];
	uint16_t frag_len;               /* length of the fragment last handed out */
} v2_frag_tx_t;

/** Drop the loaded snapshot and the read cursor. */
void v2_frag_tx_reset(v2_frag_tx_t *tx);

/** True when a snapshot is loaded and fragments remain to hand out. */
bool v2_frag_tx_have(const v2_frag_tx_t *tx);

/**
 * True when a loaded sequence has not been read for V2_FRAG_STALE_SEC.
 *
 * The caller resets and rebuilds in that case, so a returning client always gets
 * a consistent snapshot rather than the tail of someone else's.
 */
bool v2_frag_tx_stale(const v2_frag_tx_t *tx);

/**
 * Compress @json and lay out the fragments at @stride bytes per chunk.
 *
 * The stride is fixed for the whole sequence, which is what the client's
 * reassembler requires: every non-final fragment must be the same size. A caller
 * should therefore derive it once, from the MTU in force when the sequence
 * starts (v2_frag_stride_for_mtu).
 *
 * @param stride chunk size, clamped into
 *               [V2_FRAG_MIN_CHUNK, V2_FRAG_MAX_CHUNK].
 * @return 0 on success, -EINVAL on bad input, -EMSGSIZE when the payload does
 *         not fit V2_FRAG_PAYLOAD_MAX or needs more than 255 fragments at this
 *         stride, -EIO on a zlib failure.
 */
int v2_frag_tx_build(v2_frag_tx_t *tx, const char *json, uint16_t stride);

/**
 * Hand out the next fragment and advance the cursor.
 *
 * Resets automatically after the final fragment, so the next read starts a fresh
 * snapshot — the sequence is single-shot, exactly as the reference behaves.
 *
 * @param len_out receives the fragment length.
 * @return pointer into @tx valid until the next call, or NULL when nothing is
 *         loaded.
 */
const uint8_t *v2_frag_tx_next(v2_frag_tx_t *tx, uint16_t *len_out);

/**
 * The fragment handed out by the most recent v2_frag_tx_next(), without
 * advancing.
 *
 * This exists for ATT Read Blob: a client whose MTU cannot carry a whole
 * fragment asks for the remainder at a non-zero offset, and answering that with
 * the *next* fragment would silently corrupt the stream. The buffer survives the
 * automatic reset after the final fragment, so the tail of a sequence can still
 * be re-served.
 *
 * @param len_out receives the length, may be NULL.
 * @return the buffer, or NULL when nothing has been handed out yet.
 */
const uint8_t *v2_frag_tx_last(const v2_frag_tx_t *tx, uint16_t *len_out);

#ifdef __cplusplus
}
#endif

#endif /* GATT_V2_FRAG_H */
