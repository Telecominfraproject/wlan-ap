/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef STA_INFO_H
#define STA_INFO_H

struct sta_info* ap_get_sta(radiusd *radd, u8 *sta);
struct sta_info * auth_get_sta(radiusd *radd, u8 *addr);
void ap_sta_hash_add(radiusd *radd, struct sta_info *sta);
void ap_free_sta(radiusd *radd, struct sta_info *sta);
void radiusd_free_stas(radiusd *radd);
struct sta_info* ap_get_sta_radius_identifier(radiusd *radd,
					      u8 radius_identifier);
int ap_sta_for_each(radiusd *radd, int (*func)(struct sta_info *s, void *data),
		    void *data);
struct sta_info * ap_sta_add(radiusd *radd, u8 *addr);
void ap_sta_update_txrx_stats(struct radius_data *radd, struct sta_info *sta);
int ap_sta_init(struct radius_data *radd);
void ap_sta_deinit(struct radius_data *radd);
#endif /* STA_INFO_H */
