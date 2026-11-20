/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *
 * @file gatt_v2_conn.c
 * @brief Per-connection state for GATT provisioning v2 (§19.4).
 *
 * See gatt_v2_conn.h. A fixed table rather than a linked list: the number of
 * concurrent provisioning links is tiny and bounded, and a fixed table cannot
 * leak or fragment the heap in a daemon that runs for months.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "gatt_v2_conn.h"

static gatt_v2_conn_t s_conns[GATT_V2_CONN_MAX];
static uint32_t       s_seq;

void gatt_v2_conn_reset_all(void)
{
	memset(s_conns, 0, sizeof(s_conns));
	s_seq = 0;
}

gatt_v2_conn_t *gatt_v2_conn_find(const char *id)
{
	if (!id || !id[0])
		return NULL;

	for (int i = 0; i < GATT_V2_CONN_MAX; i++) {
		if (s_conns[i].in_use && strcmp(s_conns[i].id, id) == 0)
			return &s_conns[i];
	}
	return NULL;
}

gatt_v2_conn_t *gatt_v2_conn_get(const char *id)
{
	if (!id || !id[0])
		return NULL;

	gatt_v2_conn_t *c = gatt_v2_conn_find(id);
	if (c) {
		c->seq = ++s_seq;
		return c;
	}

	/* Free slot first. */
	for (int i = 0; i < GATT_V2_CONN_MAX; i++) {
		if (!s_conns[i].in_use) {
			c = &s_conns[i];
			break;
		}
	}

	if (!c) {
		/*
         * Every slot taken. Recycle the least recently used one: a connection we
         * were never told about closing must not be able to deny service to a
         * new client forever. The recycled slot starts unauthenticated, so the
         * failure direction is safe.
         */
		c = &s_conns[0];
		for (int i = 1; i < GATT_V2_CONN_MAX; i++) {
			if (s_conns[i].seq < c->seq)
				c = &s_conns[i];
		}
		syslog(LOG_WARNING, "gatt_v2_conn: table full — recycling '%s' for '%s'",
			   c->id, id);
	}

	memset(c, 0, sizeof(*c));
	c->in_use = true;
	snprintf(c->id, sizeof(c->id), "%s", id);
	c->seq = ++s_seq;
	v2_frag_rx_reset(&c->frag);
	for (int i = 0; i < GATT_V2_TX_COUNT; i++)
		v2_frag_tx_reset(&c->tx[i]);

	syslog(LOG_DEBUG, "gatt_v2_conn: tracking '%s'", c->id);
	return c;
}

void gatt_v2_conn_drop(const char *id)
{
	gatt_v2_conn_t *c = gatt_v2_conn_find(id);
	if (!c)
		return;

	syslog(LOG_DEBUG, "gatt_v2_conn: dropping '%s'", c->id);
	/*
     * Zero the whole slot, not just the flag: it holds a partially received
     * configuration and two half-delivered read snapshots, and leaving those
     * bytes for the next client to inherit is the exact failure this table
     * exists to prevent.
     */
	memset(c, 0, sizeof(*c));
}

bool gatt_v2_conn_is_authenticated(const char *id)
{
	const gatt_v2_conn_t *c = gatt_v2_conn_find(id);
	return c ? c->authenticated : false;
}

void gatt_v2_conn_set_authenticated(const char *id, bool ok)
{
	gatt_v2_conn_t *c = gatt_v2_conn_get(id);
	if (!c) {
		syslog(LOG_WARNING, "gatt_v2_conn: cannot record auth for an unnamed "
			   "connection");
		return;
	}
	c->authenticated = ok;
}

const char *gatt_v2_conn_id_from_handle(char *out, size_t out_sz,
										uint16_t conn_handle)
{
	if (!out || out_sz == 0)
		return "";
	snprintf(out, out_sz, "npi:%u", conn_handle);
	return out;
}
