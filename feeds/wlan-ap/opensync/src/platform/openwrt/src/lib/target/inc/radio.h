/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RADIO_H__
#define _RADIO_H__

struct rrm_neighbor {
	char *mac;
	char *ssid;
	char *ie;
};

extern const struct target_radio_ops *radio_ops;
extern int reload_config;
extern struct blob_buf b;
extern struct uci_context *uci;

extern int radio_ubus_init(void);

extern int hapd_rrm_enable(char *name, int neighbor, int beacon);
extern int hapd_rrm_set_neighbors(char *name, struct rrm_neighbor *neigh, int count);

#endif
