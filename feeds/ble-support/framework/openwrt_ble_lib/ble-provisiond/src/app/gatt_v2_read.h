/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file gatt_v2_read.h
 * @brief Shared read policy for the fragmented characteristics A1/A4 (§19.3).
 *
 * The phone app does not read A1 or A4 as a single value: the reference builds a
 * compressed, md5-tagged payload and hands out **one fragment per ReadValue**
 * (`wifi_provision.py`, `WifiReadCharacteristic.ReadValue` and
 * `DeviceStatusCharacteristic.ReadValue`). Answering with one whole JSON
 * document instead is not a smaller version of the same thing — the client reads
 * byte 0 as `total` and byte 1 as `index`, so a plain `{"status":…}` looks like
 * fragment 34 of 123 and the read never completes.
 *
 * Both transports therefore go through this module, so the BlueZ and TI paths
 * cannot drift apart on:
 *
 *   - the unauthenticated answer (a single `0x00` byte, as the reference does),
 *   - when a snapshot is taken and when it is rebuilt,
 *   - how ATT Read Blob offsets are served,
 *   - which connection's cursor is advanced.
 *
 * Content is supplied by whoever owns it, through the snapshot hooks below. Until
 * the configuration and status documents exist (§19.5), the default hook answers
 * `not_implemented` — as a properly framed single fragment, so the app can decode
 * it and show an error instead of waiting for fragments that never come.
 */
#ifndef GATT_V2_READ_H
#define GATT_V2_READ_H

#include <stdint.h>

#include "gatt_v2_conn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Supplies the JSON document for a fragmented characteristic.
 *
 * Called when a read sequence starts, so the bytes a client reassembles all come
 * from one consistent snapshot.
 *
 * @return a NUL-terminated JSON document. Must not return NULL.
 */
typedef const char *(*gatt_v2_snapshot_fn)(void);

/**
 * Install the document source for @which.
 *
 * Passing NULL restores the built-in `not_implemented` answer.
 */
void gatt_v2_read_set_snapshot_fn(gatt_v2_tx_chr_t which,
                                  gatt_v2_snapshot_fn fn);

/**
 * Answer one read of a fragmented characteristic.
 *
 * @param conn_id transport-supplied connection identity (§19.4). An unknown or
 *                empty identity is treated as unauthenticated.
 * @param which   GATT_V2_TX_CONFIG (A1) or GATT_V2_TX_STATUS (A4).
 * @param offset  ATT read offset. 0 hands out the next fragment and advances the
 *                cursor; non-zero re-serves the remainder of the fragment
 *                already handed out, without advancing.
 * @param mtu     negotiated ATT MTU for this link, or 0 when unknown. Decides the
 *                fragment size, so that every fragment fits one read response
 *                instead of being truncated by the ATT layer. Only consulted
 *                when a sequence starts; an MTU change mid-document does not
 *                re-cut the fragments already accounted for.
 * @param out     receives a pointer to bytes owned by this module, valid until
 *                the next call for the same connection.
 * @param out_len receives the byte count.
 *
 * @return 0 on success, -EINVAL on bad arguments, -ERANGE when @offset points
 *         past what has been handed out (the transport should answer
 *         "invalid offset").
 */
int gatt_v2_read_body(const char *conn_id, gatt_v2_tx_chr_t which,
                      uint16_t offset, uint16_t mtu,
                      const uint8_t **out, uint16_t *out_len);

/** Human-readable name for logs: "A1"/"A4". */
const char *gatt_v2_read_chr_name(gatt_v2_tx_chr_t which);

#ifdef __cplusplus
}
#endif

#endif /* GATT_V2_READ_H */
