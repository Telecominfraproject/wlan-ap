/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file gatt_v2_read.c
 * @brief Shared read policy for the fragmented characteristics A1/A4 (§19.3).
 *
 * See gatt_v2_read.h for why reads are fragmented at all.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "gatt_v2.h"
#include "gatt_v2_read.h"

/*
 * The unauthenticated answer: one 0x00 byte.
 *
 * This matches the reference exactly ("Return minimal bytes so client knows read
 * is blocked"). It is deliberately NOT the machine-readable
 * `{"status":"failed","reason":"not_authenticated"}` JSON used on A2 and A3:
 * those are unfragmented channels, while A1/A4 bytes go straight into the app's
 * fragment reassembler, which would read `{` as a fragment count of 123. One
 * sentinel byte cannot be mistaken for a fragment header of a real document,
 * because a real one always carries index 0 first.
 */
static const uint8_t s_blocked = 0x00;

/** Sent when a snapshot cannot be framed. Small enough to always fit. */
#define V2_READ_TOO_LARGE "{\"status\":\"failed\",\"reason\":\"snapshot_too_large\"}"

static gatt_v2_snapshot_fn s_snapshot[GATT_V2_TX_COUNT];

const char *gatt_v2_read_chr_name(gatt_v2_tx_chr_t which)
{
	switch (which) {
	case GATT_V2_TX_CONFIG: return "A1";
	case GATT_V2_TX_STATUS: return "A4";
	default:                return "?";
	}
}

void gatt_v2_read_set_snapshot_fn(gatt_v2_tx_chr_t which,
								  gatt_v2_snapshot_fn fn)
{
	/* Cast to int: an enum may be unsigned, which makes `< 0` a warning. */
	if ((int)which < 0 || (int)which >= GATT_V2_TX_COUNT)
		return;
	s_snapshot[which] = fn;
}

static const char *snapshot_json(gatt_v2_tx_chr_t which)
{
	if (s_snapshot[which]) {
		const char *s = s_snapshot[which]();
		if (s && s[0])
			return s;
		syslog(LOG_WARNING, "gatt_v2_read: %s snapshot hook returned nothing",
			   gatt_v2_read_chr_name(which));
	}
	/*
     * No content yet (§19.5). Returning the not-implemented document through the
     * normal framing keeps the channel honest: the app decodes a valid single
     * fragment and sees a reason, rather than timing out on a malformed one.
     */
	return gatt_v2_not_implemented_json();
}

int gatt_v2_read_body(const char *conn_id, gatt_v2_tx_chr_t which,
					  uint16_t offset, uint16_t mtu,
					  const uint8_t **out, uint16_t *out_len)
{
	if (!out || !out_len || (int)which < 0 || (int)which >= GATT_V2_TX_COUNT)
		return -EINVAL;

	/*
     * Gate first, and without allocating a connection slot: an unauthenticated
     * reader must not be able to consume table entries by reading.
     */
	gatt_v2_conn_t *conn = gatt_v2_conn_find(conn_id);
	if (!conn || !conn->authenticated) {
        *out = &s_blocked;
        *out_len = 1;
		return 0;
	}

	v2_frag_tx_t *tx = &conn->tx[which];

	/*
     * A non-zero offset is ATT Read Blob asking for the rest of the fragment we
     * already produced. Serving the *next* fragment here would corrupt the
     * stream in a way that only shows up as a checksum failure much later, so the
     * cursor is deliberately not advanced.
     */
	if (offset != 0) {
		uint16_t last_len = 0;
		const uint8_t *last = v2_frag_tx_last(tx, &last_len);
		if (!last || offset > last_len) {
			syslog(LOG_WARNING, "gatt_v2_read: %s blob read at offset %u with "
				   "nothing to continue", gatt_v2_read_chr_name(which), offset);
			return -ERANGE;
		}
        *out = last + offset;
        *out_len = (uint16_t)(last_len - offset);
		return 0;
	}

	/*
     * Drop a sequence that has gone properly idle — the reader abandoned it, so
     * resuming would hand out a document assembled over minutes.
     *
     * The window is deliberately generous (V2_FRAG_STALE_SEC); a short one
     * resets a slow-but-healthy reader back to fragment 0 forever, which is
     * exactly what happened on eap111 with the reference's 5 s. Once a sequence
     * is in flight the right thing is to finish it.
     */
	if (v2_frag_tx_stale(tx)) {
		syslog(LOG_INFO, "gatt_v2_read: %s sequence abandoned (idle %us) — "
			   "resnapshotting", gatt_v2_read_chr_name(which),
			   (unsigned)V2_FRAG_STALE_SEC);
		v2_frag_tx_reset(tx);
	}

	if (!v2_frag_tx_have(tx)) {
		/*
         * Cut the document to fit this link. A fragment larger than the MTU
         * allows is truncated by the ATT layer, and the client then fails md5 on
         * bytes it never received — which is exactly how this surfaced on
         * hardware: a 74-byte fragment on a 65-byte MTU link, retried forever.
         */
		uint16_t stride = v2_frag_stride_for_mtu(mtu);
		const char *json = snapshot_json(which);

		int ret = v2_frag_tx_build(tx, json, stride);
		if (ret < 0) {
			syslog(LOG_WARNING, "gatt_v2_read: cannot frame the %s snapshot (%d) "
				   "at a %u-byte stride — answering snapshot_too_large",
				   gatt_v2_read_chr_name(which), ret, stride);
			/*
             * Report the failure in-band rather than dropping the read: a client
             * waiting on a read it will never get is worse than a client told
             * the document is unreadable.
             */
			if (v2_frag_tx_build(tx, V2_READ_TOO_LARGE, stride) < 0)
				return -EINVAL;   /* cannot happen: the string is 48 bytes */
		}
	}

	uint16_t len = 0;
	const uint8_t *frag = v2_frag_tx_next(tx, &len);
	if (!frag)
		return -EINVAL;

	syslog(LOG_DEBUG, "gatt_v2_read: %s fragment %u/%u (%u bytes, mtu=%u)",
		   gatt_v2_read_chr_name(which), frag[1] + 1, frag[0], len, mtu);

    *out = frag;
    *out_len = len;
	return 0;
}
