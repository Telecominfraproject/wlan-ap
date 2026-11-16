/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file gatt_v2_conn.h
 * @brief Per-connection state for GATT provisioning v2 (§19.4).
 *
 * Two things must be tracked per link rather than globally:
 *
 *  - **Authentication.** A single global flag meant that with two phones
 *    connected, one authenticating granted the other access to reboot, wipe or
 *    reconfigure the device.
 *  - **Config reassembly.** Fragments carry only `total` and `index`, no session
 *    id, so interleaved writes from two clients would be spliced into one
 *    corrupt document. The reference has exactly this flaw — a single
 *    instance-level chunk dict shared by every connection.
 *
 * Connections are keyed by a transport-supplied identity string: the BlueZ path
 * uses the D-Bus device object path from the ReadValue/WriteValue options, the TI
 * path uses the NPI connection handle. A string keeps one table serving both.
 */
#ifndef GATT_V2_CONN_H
#define GATT_V2_CONN_H

#include <stdint.h>
#include <stdbool.h>

#include "gatt_v2_frag.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Links tracked at once.
 *
 * Provisioning is a one-phone-at-a-time activity; the second slot exists so a
 * stale entry cannot lock out a new client. Each slot carries one reassembly
 * buffer and two fragmented-read cursors, so a slot costs roughly
 * V2_FRAG_PAYLOAD_MAX + V2_FRAG_RAW_MAX + 2 × V2_FRAG_PAYLOAD_MAX ≈ 42 KB, and
 * the table as a whole ≈ 83 KB of BSS. That is the price of not letting two
 * phones corrupt each other; it is paid once, at a fixed size, with no heap
 * churn in a daemon that runs for months.
 */
#define GATT_V2_CONN_MAX     2

/** Longest identity string we keep. A BlueZ device path is ~37 characters. */
#define GATT_V2_CONN_ID_MAX  64

/**
 * The characteristics whose reads are fragmented, and therefore need a cursor.
 *
 * Only A1 and A4 are fragmented — the reference fragments exactly these two
 * (`zip_and_fragment(get_wifi_config())` and `zip_and_fragment(
 * get_device_status())`). A3, A5, A6 and A7 return a single plain value and need
 * no cursor, so they are deliberately absent.
 */
typedef enum {
    GATT_V2_TX_CONFIG = 0,   /**< A1, configuration snapshot */
    GATT_V2_TX_STATUS = 1,   /**< A4, device status document */
    GATT_V2_TX_COUNT
} gatt_v2_tx_chr_t;

typedef struct {
    bool         in_use;
    char         id[GATT_V2_CONN_ID_MAX];
    bool         authenticated;
    uint32_t     seq;          /* bumped on use, for least-recently-used eviction */
    v2_frag_rx_t frag;         /* A2 config reassembly for this link */
    /*
     * Read cursors, one per fragmented characteristic.
     *
     * The reference keeps these on the characteristic object, shared by every
     * connection, and mitigates the resulting cross-talk with a 5 s staleness
     * rule. That only helps when two clients read more than 5 s apart; two
     * phones reading a configuration at the same time would splice each other's
     * snapshots. Per connection costs memory and removes the failure mode.
     */
    v2_frag_tx_t tx[GATT_V2_TX_COUNT];
} gatt_v2_conn_t;

/** Forget every connection. Call at plugin init and deinit. */
void gatt_v2_conn_reset_all(void);

/**
 * Find the context for @id, allocating one if this is a new link.
 *
 * When every slot is taken the least recently used one is recycled, so a
 * forgotten connection can never permanently deny service to a new client. The
 * recycled slot loses its authentication, which is the safe direction.
 *
 * @param id transport-supplied identity; NULL or empty yields NULL.
 * @return the context, or NULL only when @id is unusable.
 */
gatt_v2_conn_t *gatt_v2_conn_get(const char *id);

/**
 * Look up @id without allocating.
 *
 * @return the context, or NULL when this link has no state yet — which callers
 *         must treat as "not authenticated".
 */
gatt_v2_conn_t *gatt_v2_conn_find(const char *id);

/** Release the slot for @id. Call on disconnect. */
void gatt_v2_conn_drop(const char *id);

/**
 * True when @id is authenticated.
 *
 * Convenience for the common gate check; an unknown id is not authenticated.
 */
bool gatt_v2_conn_is_authenticated(const char *id);

/** Record the outcome of an A5 credential write for @id. */
void gatt_v2_conn_set_authenticated(const char *id, bool ok);

/**
 * Format an NPI connection handle as an identity string.
 *
 * @param out    destination, must hold at least GATT_V2_CONN_ID_MAX bytes.
 * @param out_sz size of @out.
 * @return @out.
 */
const char *gatt_v2_conn_id_from_handle(char *out, size_t out_sz,
                                        uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif /* GATT_V2_CONN_H */
