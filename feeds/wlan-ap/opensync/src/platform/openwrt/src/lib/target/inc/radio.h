/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RADIO_H__
#define _RADIO_H__

#include "ovsdb_update.h"

#define CONFIG_APPLY_TIMEOUT 35

struct rrm_neighbor {
	char *mac;
	char *ssid;
	char *ie;
};

extern const struct target_radio_ops *radio_ops;
extern struct blob_buf b;
extern struct uci_context *uci;

extern int radio_ubus_init(void);

extern int hapd_rrm_enable(char *name, int neighbor, int beacon);
extern int hapd_rrm_set_neighbors(char *name, struct rrm_neighbor *neigh, int count);

extern void radio_maverick(void *arg);

int nl80211_channel_get(char *name, unsigned int *chan);
void set_config_apply_timeout(ovsdb_update_monitor_t *mon);
bool apc_read_conf(struct schema_APC_Config *apcconf);
bool apc_read_state(struct schema_APC_State *apcst);

#endif
