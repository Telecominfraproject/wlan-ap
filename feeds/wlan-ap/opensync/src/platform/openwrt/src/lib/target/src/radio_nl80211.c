/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <evsched.h>

#include <net/if.h>

#include <sys/types.h>

#include <linux/sockios.h>

#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/socket.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>

#include <ev.h>

#include <syslog.h>
#include <libubox/avl-cmp.h>

#include <libubox/avl.h>
#include <libubox/vlist.h>
#include <unl.h>

#include <sys/time.h>

#include "nl80211_cpy.h"
#include "target.h"
#include "nl80211.h"
#include "phy.h"
#include "utils.h"
#include "radio.h"

#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"

#define STA_POLL_INTERVAL 10
extern struct ev_loop *wifihal_evloop;
extern ovsdb_table_t table_Wifi_VIF_State;
extern ovsdb_table_t table_Wifi_Associated_Clients;
static struct unl unl_req;
static struct unl unl_notify;
static ev_io unl_io;

static int avl_addrcmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, 6);
}

static struct avl_tree phy_tree = AVL_TREE_INIT(phy_tree, avl_strcmp, false, NULL);
static struct avl_tree wif_tree = AVL_TREE_INIT(wif_tree, avl_strcmp, false, NULL);
static struct avl_tree sta_tree = AVL_TREE_INIT(sta_tree, avl_addrcmp, false, NULL);

struct wifi_phy *phy_find(const char *name)
{
	struct wifi_phy *phy = avl_find_element(&phy_tree, name, phy, avl);

	return phy;
}

struct wifi_iface *vif_find(const char *name)
{
	struct wifi_iface *wif = avl_find_element(&wif_tree, name, wif, avl);

	return wif;
}

struct wifi_station *sta_find(const unsigned char *addr)
{
	struct wifi_station *sta = avl_find_element(&sta_tree, addr, sta, avl);

	return sta;
}

static void vif_add_station(struct wifi_station *sta, char *ifname, int assoc)
{
	struct schema_Wifi_Associated_Clients client;
	char mac[ETH_ALEN * 3];

	print_mac(mac, sta->addr);
	memset(&client, 0, sizeof(client));
	schema_Wifi_Associated_Clients_mark_all_present(&client);
	client._partial_update = true;
	SCHEMA_SET_STR(client.mac, mac);
	if (assoc)
		SCHEMA_SET_STR(client.state, "active");
	radio_ops->op_client(&client, ifname, assoc);
}

static void vif_add_sta_rate_rule(uint8_t *addr, char *ifname)
{
	char *rule;
	ssize_t rule_sz;

	LOGI("Add mac rate rule:%s: %02X:%02X:%02X:%02X:%02X:%02X",
	     ifname, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	rule_sz = snprintf(NULL, 0, "/lib/nft-qos/mac-rate.sh add %s %02X:%02X:%02X:%02X:%02X:%02X",
			   ifname, addr[0], addr[1], addr[2], addr[3],
			   addr[4], addr[5]);

	rule = malloc(rule_sz + 1);

	snprintf(rule, rule_sz + 1, "/lib/nft-qos/mac-rate.sh add %s %02X:%02X:%02X:%02X:%02X:%02X",
		 ifname, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	system(rule);
	free(rule);
}

static void vif_del_sta_rate_rule(uint8_t *addr, char *ifname)
{
	char *rule;
	ssize_t rule_sz;

	LOGI("Del mac rate rule:%s: %02X:%02X:%02X:%02X:%02X:%02X",
	     ifname, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	rule_sz = snprintf(NULL, 0, "/lib/nft-qos/mac-rate.sh del %s %02X:%02X:%02X:%02X:%02X:%02X",
			   ifname, addr[0], addr[1],addr[2], addr[3], addr[4], addr[5]);

	rule = malloc(rule_sz + 1);
	snprintf(rule, rule_sz + 1, "/lib/nft-qos/mac-rate.sh del %s %02X:%02X:%02X:%02X:%02X:%02X",
		 ifname, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    system(rule);
    free(rule);
}

static void nl80211_add_station(struct nlattr **tb, char *ifname)
{
	struct wifi_station *sta;
	struct wifi_iface *wif;
	uint8_t *addr;

	if (tb[NL80211_ATTR_MAC] == NULL)
		return;

	addr = nla_data(tb[NL80211_ATTR_MAC]);
	vif_add_sta_rate_rule(addr, ifname);

	wif = avl_find_element(&wif_tree, ifname, wif, avl);
	if (!wif)
		return;

	sta = avl_find_element(&sta_tree, addr, sta, avl);
	/* In case of roaming between interfaces (wif) the sta would
	 * already be in the sta_tree, but however it should be inserted
	 * again with new parent wif. Also, sta should be detached from
	 * old wif and attached to new wif.
	 * When the client roams between wifs, the sta assocs
	 * first to new wif(NEW_STATION nlevent) and then the hostapd
	 * deletes (DEL_STATION nlevent) the stale client from old wif.
	 */
	if (sta) {
		if (!strcasecmp(wif->name, sta->parent->name)) {
			return;
		}
		else {
			list_del(&sta->iface);
			sta->parent = wif;
			list_add(&sta->iface, &wif->stas);
		}
	}

	sta = malloc(sizeof(*sta));
	if (!sta)
		return;

	memset(sta, 0, sizeof(*sta));
	memcpy(sta->addr, addr, 6);
	sta->avl.key = sta->addr;
	sta->parent = wif;
	avl_insert(&sta_tree, &sta->avl);
	list_add(&sta->iface, &wif->stas);

	vif_add_station(sta, ifname, 1);
}

static void _nl80211_del_station(char *ifname, struct wifi_station *sta)
{
	vif_del_sta_rate_rule(sta->addr, ifname);
	vif_add_station(sta, ifname, 0);

	/* Dont delete sta if it has roamed to a new interface */
	if (strcasecmp(ifname, sta->parent->name))
		return;

	list_del(&sta->iface);
	avl_delete(&sta_tree, &sta->avl);
	free(sta);
}

static void nl80211_del_station(struct nlattr **tb, char *ifname)
{
	struct wifi_station *sta;
	uint8_t *addr;

	if (tb[NL80211_ATTR_MAC] == NULL)
		return;

	addr = nla_data(tb[NL80211_ATTR_MAC]);
	sta = avl_find_element(&sta_tree, addr, sta, avl);
	if (!sta) {
		vif_del_sta_rate_rule(addr, ifname);
		return;
	}

	_nl80211_del_station(ifname, sta);
}

static void vif_sync_stations(char *ifname)
{
	struct schema_Wifi_Associated_Clients *ovs_clients = NULL;
	struct schema_Wifi_VIF_State vstate;
	json_t *where = NULL;
	struct wifi_station *sta = NULL, *tmp = NULL;
	int i = 0;
	uint32_t j = 0;
	char mac[ETH_ALEN * 3];

	if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_State, if_name), ifname)))
		return;
	if (!ovsdb_table_select_one_where(&table_Wifi_VIF_State, where, &vstate))
		return;
	if (WARN_ON(!(ovs_clients = calloc(vstate.associated_clients_len, sizeof(*ovs_clients)))))
		return;

	for (i = 0; i < vstate.associated_clients_len; i++) {
		if (WARN_ON(!(where = ovsdb_where_uuid("_uuid", vstate.associated_clients[i].uuid))))
			goto free;
		if (WARN_ON(!ovsdb_table_select_one_where(&table_Wifi_Associated_Clients, where, ovs_clients + i)))
			goto free;
	}

	for (i = 0; i < vstate.associated_clients_len; i++) {
		j = 0;
		avl_for_each_element_safe(&sta_tree, sta, avl, tmp) {
			if (strcasecmp(ifname, sta->parent->name)) {
				j++;
				continue;
			}
			print_mac(mac, sta->addr);
			if (!strcasecmp(ovs_clients[i].mac, mac))
				break;
			j++;
		}
		if (sta_tree.count > 0 && sta_tree.count == j)
		{
			LOGI("%s: remove stale client %s", ifname, ovs_clients[i].mac);
			schema_Wifi_Associated_Clients_mark_all_present(ovs_clients + i);
			ovs_clients[i]._partial_update = true;
			radio_ops->op_client(ovs_clients + i, ifname, false);
		}

	}

	avl_for_each_element_safe(&sta_tree, sta, avl, tmp) {

		if (strcasecmp(ifname, sta->parent->name))
			continue;

		print_mac(mac, sta->addr);
		for (i = 0; i < vstate.associated_clients_len; i++)
			if (!strcasecmp(mac, ovs_clients[i].mac))
				break;
		if (i == vstate.associated_clients_len)
		{
			LOGI("%s: add client %s", ifname, mac);
			vif_add_station(sta, ifname, true);
		}

	}

free:
	free(ovs_clients);
}

static void nl80211_add_iface(struct nlattr **tb, char *ifname, char *phyname, int ifidx)
{
	struct wifi_iface *wif;
	uint8_t *addr;

	if (tb[NL80211_ATTR_MAC] == NULL)
		return;

	addr = nla_data(tb[NL80211_ATTR_MAC]);
	wif = avl_find_element(&wif_tree, ifname, wif, avl);
	if (wif)
		return;

	wif = malloc(sizeof(*wif));
	if (!wif)
		return;

	memset(wif, 0, sizeof(*wif));
	memcpy(wif->addr, addr, 6);
	strncpy(wif->name, ifname, IF_NAMESIZE);
	wif->avl.key = wif->name;
	INIT_LIST_HEAD(&wif->stas);
	avl_insert(&wif_tree, &wif->avl);
	wif->ifidx = ifidx;
	wif->parent = avl_find_element(&phy_tree, phyname, wif->parent, avl);
	if (wif->parent)
		list_add(&wif->phy, &wif->parent->wifs);
}

static void _nl80211_del_iface(struct wifi_iface *wif)
{
	struct wifi_station *sta, *tmp;

 	list_del(&wif->phy);
	list_for_each_entry_safe(sta, tmp, &wif->stas, iface)
		_nl80211_del_station(wif->name, sta);
	avl_delete(&wif_tree, &wif->avl);
	free(wif);
}

static void nl80211_del_iface(struct nlattr **tb, char *ifname)
{
	struct wifi_iface *wif;

	wif = avl_find_element(&wif_tree, ifname, wif, avl);
	if (!wif)
		return;
	_nl80211_del_iface(wif);
}

static void nl80211_add_phy(struct nlattr **tb, char *name)
{
	struct wifi_phy *phy;

	phy = avl_find_element(&phy_tree, name, phy, avl);
	if (!phy) {
		phy = malloc(sizeof(*phy));
		if (!phy)
			return;

		memset(phy, 0, sizeof(*phy));
		strncpy(phy->name, name, IF_NAMESIZE);
		phy->avl.key = phy->name;
		INIT_LIST_HEAD(&phy->wifs);
		avl_insert(&phy_tree, &phy->avl);
	}

	if (tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX] &&
	    tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX]) {
		phy->tx_ant_avail = nla_get_u32(tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX]);
		phy->rx_ant_avail = nla_get_u32(tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX]);
	}

	if (tb[NL80211_ATTR_WIPHY_ANTENNA_TX] &&
	    tb[NL80211_ATTR_WIPHY_ANTENNA_RX]) {
		phy->tx_ant = nla_get_u32(tb[NL80211_ATTR_WIPHY_ANTENNA_TX]);
		phy->rx_ant = nla_get_u32(tb[NL80211_ATTR_WIPHY_ANTENNA_RX]);
	}

	if (tb[NL80211_ATTR_WIPHY_BANDS]) {
		struct nlattr *nl_band = NULL;
		int rem_band = 0;

		nla_for_each_nested(nl_band, tb[NL80211_ATTR_WIPHY_BANDS], rem_band) {
			struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];

			nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band),
				  nla_len(nl_band), NULL);

			if (tb_band[NL80211_BAND_ATTR_HT_CAPA])
				phy->ht_capa = nla_get_u16(tb_band[NL80211_BAND_ATTR_HT_CAPA]);
			if (tb_band[NL80211_BAND_ATTR_VHT_CAPA])
				phy->vht_capa = nla_get_u16(tb_band[NL80211_BAND_ATTR_VHT_CAPA]);

			if (tb_band[NL80211_BAND_ATTR_FREQS]) {
			        struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
				struct nlattr *nl_freq = NULL;
				int rem_freq = 0;

				nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq) {
					static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
						[NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
						[NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
						[NL80211_FREQUENCY_ATTR_NO_IR] = { .type = NLA_FLAG },
						[__NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
						[NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
						[NL80211_FREQUENCY_ATTR_MAX_TX_POWER] = { .type = NLA_U32 },
					};
					uint32_t freq;
					int chan;

					nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq),
						  nla_len(nl_freq), freq_policy);
					if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
						continue;

					freq = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
					chan = ieee80211_frequency_to_channel(freq);
					if (chan >= IEEE80211_CHAN_MAX) {
						LOG(DEBUG, "%s: found invalid channel %d", phy->name, chan);
						continue;
					}

					phy->freq[chan] = 0;
					phy->channel[chan] = 0;
					phy->chandfs[chan] = 0;
					phy->chandisabled[chan] = 0;
					phy->chanpwr[chan] = 0;

					if (tb_freq[NL80211_FREQUENCY_ATTR_DISABLED]) {
						phy->chandisabled[chan] = 1;
						phy->chandfs[chan] = 0;
						continue;
					}

					if (chan <= 16)
						phy->band_2g = 1;
					else if (chan >= 32 && chan <= 68)
						phy->band_5gl = 1;
					else if (chan >= 96)
						phy->band_5gu = 1;

					if (tb_freq[NL80211_FREQUENCY_ATTR_RADAR]) {
						phy->chandfs[chan] = 1;
						phy->chanpwr[chan] = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER]);
						phy->chandisabled[chan] = 0;
						LOG(DEBUG, "%s: found dfs channel %d", phy->name, chan);
						continue;
					}

					phy->freq[chan] = freq;
					phy->channel[chan] = 1;

					if (tb_freq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER] &&
					    !tb_freq[NL80211_FREQUENCY_ATTR_DISABLED])
						phy->chanpwr[chan] = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER]);
				}
			}
		}
	}
}

static void nl80211_del_phy(struct nlattr **tb, char *name)
{
	struct wifi_iface *wif, *tmp;
	struct wifi_phy *phy;

	phy = avl_find_element(&phy_tree, name, phy, avl);
	if (!phy)
		return;
	list_for_each_entry_safe(wif, tmp, &phy->wifs, phy)
		_nl80211_del_iface(wif);
	avl_delete(&phy_tree, &phy->avl);
	free(phy);
}

static int nl80211_channel_recv(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	unsigned int *chan = (unsigned int *)arg;

	memset(tb, 0, sizeof(tb));
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_WIPHY_FREQ]) {
		*chan = ieee80211_frequency_to_channel(nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]));
	}

	return NL_OK;
}

int nl80211_channel_get(char *name, unsigned int *chan)
{
	struct nl_msg *msg;
	struct wifi_phy *phy;
	struct wifi_iface *wif=NULL;
	int idx = 0;

	phy = avl_find_element(&phy_tree, name, phy, avl);
	if (!phy)
		return -1;

	if (list_empty(&phy->wifs))
		return -1;

	wif = list_first_entry(&phy->wifs, struct wifi_iface, phy);

	if (!wif)
		return -1;

	idx = if_nametoindex(wif->name);

	if (!idx)
		return -1;

	msg = unl_genl_msg(&unl_req, NL80211_CMD_GET_INTERFACE, true);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);

	unl_genl_request(&unl_req, msg, nl80211_channel_recv, chan);

	phy->current_channel = *chan;

	return NL_OK;
}

static int nl80211_recv(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	char ifname[IFNAMSIZ] = {};
	char *pif_name=NULL;
	char phyname[IFNAMSIZ] = {};
	int ifidx = -1, phy = -1;
	memset(tb, 0, sizeof(tb));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX]) {
		ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);
		pif_name = if_indextoname(ifidx, ifname);
	}
	if ((pif_name == NULL) && tb[NL80211_ATTR_IFNAME]) {
	       	strncpy(ifname, nla_get_string(tb[NL80211_ATTR_IFNAME]), IFNAMSIZ);
	}

	if (tb[NL80211_ATTR_WIPHY]) {
		phy = nla_get_u32(tb[NL80211_ATTR_WIPHY]);
		snprintf(phyname, sizeof(phyname), "phy%d", phy);
	} else if (tb[NL80211_ATTR_WIPHY_NAME]) {
			strncpy(phyname, nla_get_string(tb[NL80211_ATTR_WIPHY_NAME]), IFNAMSIZ);
	} else if (sscanf(ifname, "wlan%d", &phy)) {
			snprintf(phyname, sizeof(phyname), "phy%d", phy);
	}

	switch (gnlh->cmd) {
	case NL80211_CMD_NEW_STATION:
		nl80211_add_station(tb, ifname);
		break;
	case NL80211_CMD_DEL_STATION:
		nl80211_del_station(tb, ifname);
		break;
	case NL80211_CMD_NEW_INTERFACE:
		nl80211_add_iface(tb, ifname, phyname, ifidx);
		break;
	case NL80211_CMD_DEL_INTERFACE:
		nl80211_del_iface(tb, ifname);
		break;
	case NL80211_CMD_DEL_WIPHY:
		nl80211_del_phy(tb, phyname);
		break;
	case NL80211_CMD_NEW_WIPHY:
	case NL80211_CMD_GET_WIPHY:
		nl80211_add_phy(tb, phyname);
		break;
	default:
		syslog(0, "%s:%s[%d]%d\n", __FILE__, __func__, __LINE__, gnlh->cmd);
		break;
	}
	return NL_OK;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	return NL_SKIP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	return NL_SKIP;
}

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static void nl80211_ev(struct ev_loop *ev, struct ev_io *io, int event)
{
	struct nl_cb *cb;

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, NULL);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, NULL);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nl80211_recv, NULL);
	nl_recvmsgs(unl_notify.sock, cb);
	nl_cb_put(cb);
}

static void vif_poll_stations(void *arg)
{
	struct wifi_iface *wif = NULL;

	avl_for_each_element(&wif_tree, wif, avl) {
		vif_sync_stations(wif->name);
	}
	evsched_task_reschedule_ms(EVSCHED_SEC(STA_POLL_INTERVAL));
}

void vif_get_assoc_stations(void)
{
	struct wifi_iface *wif = NULL;

 	avl_for_each_element(&wif_tree, wif, avl) {
		struct nl_msg *msg;
		msg = unl_genl_msg(&unl_req, NL80211_CMD_GET_STATION, true);
		nla_put_u32(msg, NL80211_ATTR_IFINDEX, wif->ifidx);
		unl_genl_request(&unl_req, msg, nl80211_recv, NULL);
 	}
}

int radio_nl80211_init(void)
{
	struct nl_msg *msg;

	if (unl_genl_init(&unl_req, "nl80211") < 0) {
		syslog(0, "nl80211: failed to connect\n");
		return -1;
	}

	if (unl_genl_init(&unl_notify, "nl80211") < 0) {
		syslog(0, "nl80211: failed to connect\n");
		return -1;
	}

	msg = unl_genl_msg(&unl_req, NL80211_CMD_GET_WIPHY, true);
	nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP);
	unl_genl_request(&unl_req, msg, nl80211_recv, NULL);
	msg = unl_genl_msg(&unl_req, NL80211_CMD_GET_INTERFACE, true);
	unl_genl_request(&unl_req, msg, nl80211_recv, NULL);

	unl_genl_subscribe(&unl_notify, "config");
	unl_genl_subscribe(&unl_notify, "mlme");
	unl_genl_subscribe(&unl_notify, "vendor");


	if (nl_socket_set_buffer_size(unl_notify.sock, 262144, 0) < 0)
		LOGE("radio_nl80211: Failed to set nl socket buffer size");

	if (nl_socket_set_nonblocking(unl_notify.sock))
		LOGE("radio_nl80211: Failed to set socket in the non blocking mode");
	/* Get already associated stations if any */
	vif_get_assoc_stations();

	ev_io_init(&unl_io, nl80211_ev, unl_notify.sock->s_fd, EV_READ);
        ev_io_start(wifihal_evloop, &unl_io);
	evsched_task(&vif_poll_stations, NULL, EVSCHED_SEC(5));

	return 0;
}

int nl80211_allowed_channels_get(char *name)
{
	struct nl_msg *msg;
	struct wifi_phy *phy;
	struct wifi_iface *wif=NULL;
	int idx = 0;

	phy = avl_find_element(&phy_tree, name, phy, avl);
	if (!phy)
		return -1;

	if (list_empty(&phy->wifs))
		return -1;

	wif = list_first_entry(&phy->wifs, struct wifi_iface, phy);

	if (!wif)
		return -1;

	idx = if_nametoindex(wif->name);

	if (!idx)
		return -1;

	msg = unl_genl_msg(&unl_req, NL80211_CMD_GET_WIPHY, true);
	unl_genl_request(&unl_req, msg, nl80211_recv, NULL);

	return NL_OK;
}
