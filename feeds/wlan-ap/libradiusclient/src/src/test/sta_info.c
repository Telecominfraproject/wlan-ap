/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#include "radiusd.h"
#include "sta_info.h"
#include "eloop_libradius.h"
#include "accounting.h"
#include "radius.h"
#include "radius_debug.h"
#include "radius_client.h"

#define AP_STA_STATS_INTERVAL 30

struct sta_info* ap_get_sta(radiusd *radd, u8 *sta)
{
    struct sta_info *s;

    s = radd->sta_hash[STA_HASH(sta)];
    while (s != NULL && memcmp(s->addr, sta, 6) != 0)
        s = s->hnext;
    return s;
}

struct sta_info * auth_get_sta(radiusd *radd, u8 *addr)
{
    struct sta_info *sta;

    sta = ap_get_sta(radd, addr);
    if (sta)
        return sta;

    radius_debug_print(RADIUSD_DEBUG_MINIMAL, "  New STA\n");

    if (radd->num_sta >= 128) {
        return NULL;
    }

    sta = ap_sta_add(radd, addr);
    if (sta == NULL) {
        radiusd_logger(radd, addr,RADIUSD_LEVEL_NOTICE,
                       "failed to add new STA entry");
    }

    return sta;
}

struct sta_info* ap_get_sta_radius_identifier(radiusd *radd,
                          u8 radius_identifier)
{
    struct sta_info *s;

    s = radd->sta_list;

    while (s) {
        if (s->radius_identifier >= 0 &&
            s->radius_identifier == radius_identifier)
            return s;
        s = s->next;
    }

    return NULL;
}

static void ap_sta_list_del(radiusd *radd, struct sta_info *sta)
{
    struct sta_info *tmp;

    if (radd->sta_list == sta) {
        radd->sta_list = sta->next;
        return;
    }

    tmp = radd->sta_list;
    while (tmp != NULL && tmp->next != sta)
        tmp = tmp->next;
    if (tmp == NULL) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not remove STA " MACSTR " from list.\n",
               MAC2STR(sta->addr));
    } else
        tmp->next = sta->next;
}


void ap_sta_hash_add(radiusd *radd, struct sta_info *sta)
{
    sta->hnext = radd->sta_hash[STA_HASH(sta->addr)];
    radd->sta_hash[STA_HASH(sta->addr)] = sta;
}

static void ap_sta_hash_del(radiusd *radd, struct sta_info *sta)
{
    struct sta_info *s;

    s = radd->sta_hash[STA_HASH(sta->addr)];
    if (s == NULL) return;
    if (memcmp(s->addr, sta->addr, 6) == 0) {
        radd->sta_hash[STA_HASH(sta->addr)] = s->hnext;
        return;
    }

    while (s->hnext != NULL && memcmp(s->hnext->addr, sta->addr, 6) != 0)
        s = s->hnext;
    if (s->hnext != NULL)
        s->hnext = s->hnext->hnext;
    else
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "AP: could not remove STA " MACSTR " from hash table\n",
               MAC2STR(sta->addr));
}

void ap_free_sta(radiusd *radd, struct sta_info *sta)
{
    accounting_sta_stop(radd, sta);

    ap_sta_hash_del(radd, sta);
    ap_sta_list_del(radd, sta);

    radd->num_sta--;

    free(sta);
}

void radiusd_free_stas(radiusd *radd)
{
    struct sta_info *sta, *prev;

    sta = radd->sta_list;

    while (sta) {
        prev = sta;
        sta = sta->next;
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Removing station " MACSTR "\n", MAC2STR(prev->addr));
        ap_free_sta(radd, prev);
    }
}

int ap_sta_for_each(radiusd *radd, int (*func)(struct sta_info *s, void *data),
            void *data)
{
    struct sta_info *s;
    int ret = 0;

    s = radd->sta_list;

    while (s) {
        ret = func(s, data);
        if (ret)
            break;
        s = s->next;
    }

    return ret;
}

struct sta_info * ap_sta_add(radiusd *radd, u8 *addr)
{
    struct sta_info *sta;

    sta = (struct sta_info *) malloc(sizeof(struct sta_info));
    if (sta == NULL)
        return NULL;
    memset(sta, 0, sizeof(struct sta_info));
    sta->radius_identifier = -1;

    /* initialize STA info data */
    memcpy(sta->addr, addr, ETH_ALEN);
    sta->next = radd->sta_list;
    radd->sta_list = sta;
    radd->num_sta++;
    ap_sta_hash_add(radd, sta);
    return sta;
}

void ap_sta_update_txrx_stats(struct radius_data *radd, struct sta_info *sta)
{
#if 0
    struct hostap_sta_driver_data data;
    time_t now;

    if (hostapd_read_sta_data(hapd, &data, sta->addr)) {
        sta->last_txrx_stats_update = 0;
        return;
    }

    time(&now);
    if (sta->last_txrx_stats_update) {
        sta->prev_int_seconds = now - sta->last_txrx_stats_update;
        sta->prev_int_tx_bytes = data.tx_bytes - sta->last_tx_bytes;
        sta->prev_int_rx_bytes = data.rx_bytes - sta->last_rx_bytes;
    }

    sta->last_txrx_stats_update = now;
    sta->last_tx_bytes = data.tx_bytes;
    sta->last_rx_bytes = data.rx_bytes;
#endif
}

static int ap_sta_update_stats(struct sta_info *sta, void *data)
{
    struct radius_data *radd = data;
    ap_sta_update_txrx_stats(radd, sta);
    return 0;
}


static void ap_sta_stats_timer(void *eloop_ctx, void *timeout_ctx)
{
    struct radius_data *radd = eloop_ctx;

    ap_sta_for_each(radd, ap_sta_update_stats, radd);

    eloop_register_timeout(AP_STA_STATS_INTERVAL, 0, ap_sta_stats_timer,
                   radd, NULL);
}


int ap_sta_init(struct radius_data *radd)
{
    eloop_register_timeout(AP_STA_STATS_INTERVAL, 0, ap_sta_stats_timer,
                   radd, NULL);
    return 0;
}


void ap_sta_deinit(struct radius_data *radd)
{
    eloop_cancel_timeout(ap_sta_stats_timer, radd, NULL);
}
