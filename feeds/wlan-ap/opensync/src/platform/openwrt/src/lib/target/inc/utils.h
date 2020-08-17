/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _UTIL_H__
#define _UTIL_H__

#include <libubox/blobmsg.h>

#define min(a,b) (((a) < (b)) ? (a) : (b))

struct mode_map {
	int fiveg;
	char *hwmode;
	char *ucihwmode;
	char *htmode;
	char *ucihtmode;
};

extern struct mode_map *mode_map_get_uci(const char *band, const char *htmode, const char *hwmode);
extern struct mode_map *mode_map_get_cloud(const char *htmode, const char *hwmode);

extern int vif_get_mac(char *vap, char *mac);
extern int vif_is_ready(const char *name);

#define blobmsg_add_bool blobmsg_add_u8
extern int blobmsg_add_hex16(struct blob_buf *buf, const char *name, uint16_t val);
extern int blobmsg_get_hex16(struct blob_attr *a);

extern bool vif_state_to_conf(struct schema_Wifi_VIF_State *vstate, struct schema_Wifi_VIF_Config *vconf);
extern bool radio_state_to_conf(struct schema_Wifi_Radio_State *rstate, struct schema_Wifi_Radio_Config *rconf);

extern void print_mac(char *mac_addr, const unsigned char *arg);

extern int ieee80211_frequency_to_channel(int freq);
extern int ieee80211_channel_to_frequency(int chan);

extern int iface_is_up(const char *ifname);

extern int net_get_mtu(char *iface);
extern int net_get_mac(char *iface, char *mac);
extern int net_is_bridge(char *iface);
#endif
