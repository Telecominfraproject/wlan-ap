/**
 * @file mtu_segment.h
 * @brief MTU-based segmentation and reassembly for BLE GATT payloads.
 *
 * BLE ATT has a limited MTU (default 23, negotiable up to 517).
 * For payloads larger than (ATT_MTU - 3) bytes, this module provides:
 *   - Segmentation: splits outgoing data into MTU-sized segments
 *   - Reassembly: reconstructs incoming segmented writes
 *
 * Segment header format (1 byte):
 *   Bit 7:    FIRST segment flag
 *   Bit 6:    LAST segment flag
 *   Bits 5-0: Sequence number (0-63)
 *
 * First segment additionally carries a 2-byte total_length (little-endian)
 * after the header byte, before the payload data.
 */
#ifndef MTU_SEGMENT_H
#define MTU_SEGMENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MTU_SEG_MAX_PAYLOAD     4096
#define MTU_SEG_HEADER_SIZE     1
#define MTU_SEG_FIRST_EXTRA     2   /* total_len field in first segment */
#define MTU_SEG_MAX_SEGMENTS    64

#define MTU_SEG_FLAG_FIRST      0x80
#define MTU_SEG_FLAG_LAST       0x40
#define MTU_SEG_SEQ_MASK        0x3F

/* Default ATT MTU if not negotiated */
#define MTU_SEG_DEFAULT_ATT_MTU 23

/* ══════════════════════════════════════════════════════════════════
 *  Reassembly — receiving segmented data from remote writes
 * ══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t buffer[MTU_SEG_MAX_PAYLOAD];
    uint16_t total_expected;    /**< Total payload length from first segment */
    uint16_t received_len;      /**< Bytes received so far */
    uint8_t next_seq;           /**< Expected sequence number */
    bool active;                /**< Reassembly in progress */
} mtu_reassembly_t;

/**
 * Initialize/reset reassembly context.
 */
void mtu_reassembly_init(mtu_reassembly_t *ctx);

/**
 * Reset an active reassembly (e.g., on timeout or error).
 */
void mtu_reassembly_reset(mtu_reassembly_t *ctx);

/**
 * Feed a received segment into the reassembly engine.
 *
 * @param ctx       Reassembly context
 * @param segment   Raw segment data (including header byte)
 * @param seg_len   Length of segment data
 * @return  0  = segment accepted, more expected
 *          1  = reassembly complete (data in ctx->buffer, length in ctx->received_len)
 *         -1  = error (sequence mismatch, overflow, malformed)
 */
int mtu_reassembly_feed(mtu_reassembly_t *ctx, const uint8_t *segment,
                        uint16_t seg_len);

/* ══════════════════════════════════════════════════════════════════
 *  Segmentation — splitting outgoing data for notifications/reads
 * ══════════════════════════════════════════════════════════════════ */

typedef struct {
    const uint8_t *data;
    uint16_t data_len;
    uint16_t mtu;               /**< ATT MTU (includes 3-byte ATT header) */
    uint16_t offset;            /**< Current offset into data */
    uint8_t seq;                /**< Current sequence number */
    bool done;                  /**< All segments emitted */
} mtu_segmenter_t;

/**
 * Initialize a segmenter for outgoing data.
 *
 * @param ctx       Segmenter context
 * @param data      Data to segment
 * @param data_len  Length of data
 * @param att_mtu   Negotiated ATT MTU (use MTU_SEG_DEFAULT_ATT_MTU if unknown)
 */
void mtu_segmenter_init(mtu_segmenter_t *ctx, const uint8_t *data,
                        uint16_t data_len, uint16_t att_mtu);

/**
 * Get next segment.
 *
 * @param ctx       Segmenter context
 * @param out_buf   Output buffer (caller provides at least (mtu - 3) bytes)
 * @param buf_size  Size of output buffer
 * @return Segment length in bytes, or 0 if all segments emitted.
 */
uint16_t mtu_segmenter_next(mtu_segmenter_t *ctx, uint8_t *out_buf,
                            uint16_t buf_size);

/**
 * Check if data fits in a single ATT payload (no segmentation needed).
 *
 * @param data_len  Payload length
 * @param att_mtu   Negotiated ATT MTU
 * @return true if data fits without segmentation
 */
static inline bool mtu_seg_fits_single(uint16_t data_len, uint16_t att_mtu)
{
    /* ATT payload = MTU - 3 (ATT header) */
    return data_len <= (uint16_t)(att_mtu - 3);
}

#ifdef __cplusplus
}
#endif

#endif /* MTU_SEGMENT_H */
