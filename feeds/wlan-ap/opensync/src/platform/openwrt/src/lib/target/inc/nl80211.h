/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _NL_80211_H__
#define _NL_80211_H__

#include <net/if.h>

#include <linux/limits.h>

#include <libubox/avl.h>
#include <libubox/vlist.h>

#define IEEE80211_CHAN_MAX (196 + 1)

struct wifi_phy {
	struct avl_node avl;
	char name[IF_NAMESIZE];
	struct list_head wifs;

	unsigned short ht_capa;
	unsigned int vht_capa;

	unsigned char chandisabled[IEEE80211_CHAN_MAX];
	unsigned char channel[IEEE80211_CHAN_MAX];
	unsigned char chandfs[IEEE80211_CHAN_MAX];
	unsigned char chanpwr[IEEE80211_CHAN_MAX];
	unsigned int freq[IEEE80211_CHAN_MAX];

	int tx_ant, rx_ant, tx_ant_avail, rx_ant_avail;
	int band_2g, band_5gl, band_5gu;
};

struct wifi_iface {
	struct avl_node avl;
	uint8_t addr[6];
	int ifidx;
	int noise;
	char name[IF_NAMESIZE];
	struct wifi_phy *parent;
	struct list_head stas;
	struct list_head phy;
};

struct wifi_station {
	struct avl_node avl;
	uint8_t addr[6];
	struct wifi_iface *parent;
	struct list_head iface;

	int8_t rssi;
	uint32_t rx_packets;
	uint32_t tx_packets;
	uint32_t rx_bytes;
	uint32_t tx_bytes;
};

extern int radio_nl80211_init(void);

extern struct wifi_phy *phy_find(const char *name);
extern struct wifi_iface *vif_find(const char *name);
extern struct wifi_station *sta_find(const unsigned char *addr);

struct nl_call_param {
	char *ifname;
	radio_type_t type;
	ds_dlist_t *list;
};

typedef struct ssid_list {
        char ssid[RADIO_ESSID_LEN+1];
        char ifname[RADIO_NAME_LEN+1];
        ds_dlist_node_t node;
} ssid_list_t;

extern int stats_nl80211_init(void);
extern int nl80211_get_tx_chainmask(char *name, unsigned int *mask);
extern int nl80211_get_oper_channel(char *name, unsigned int *chan);
extern int nl80211_get_assoclist(struct nl_call_param *nl_call_param);
extern int nl80211_get_survey(struct nl_call_param *nl_call_param);
extern int nl80211_scan_trigger(struct nl_call_param *nl_call_param, uint32_t *chan_list, uint32_t chan_num,
				int dwell_time, radio_scan_type_t scan_type,
				target_scan_cb_t *scan_cb, void *scan_ctx);
extern int nl80211_scan_abort(struct nl_call_param *nl_call_param);
extern int nl80211_scan_dump(struct nl_call_param *nl_call_param);
extern int nl80211_get_ssid(struct nl_call_param *nl_call_param);

#endif
