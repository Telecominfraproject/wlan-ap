/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include <uci.h>
#include <uci_blob.h>

#include <target.h>

#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"

#include "nl80211.h"
#include "radio.h"
#include "vif.h"
#include "phy.h"
#include "log.h"
#include "evsched.h"
#include "uci.h"
#include "utils.h"
#include "captive.h"
#include "rrm_config.h"
#include "vlan.h"
#include "radius_proxy.h"

ovsdb_table_t table_Hotspot20_Config;
ovsdb_table_t table_Hotspot20_OSU_Providers;
ovsdb_table_t table_Hotspot20_Icon_Config;

ovsdb_table_t table_APC_Config;
ovsdb_table_t table_APC_State;
unsigned int radproxy_apc = 0;

static struct uci_package *wireless;
struct uci_context *uci;
struct blob_buf b = { };
struct blob_buf del = { };
int reload_config = 0;
static struct timespec startup_time;

enum {
	WDEV_ATTR_PATH,
	WDEV_ATTR_DISABLED,
	WDEV_ATTR_CHANNEL,
	WDEV_ATTR_TXPOWER,
	WDEV_ATTR_BEACON_INT,
	WDEV_ATTR_HTMODE,
	WDEV_ATTR_HWMODE,
	WDEV_ATTR_COUNTRY,
	WDEV_ATTR_CHANBW,
	WDEV_ATTR_TX_ANTENNA,
	WDEV_ATTR_RX_ANTENNA,
	WDEV_ATTR_FREQ_BAND,
	WDEV_AATR_CHANNELS,
	WDEV_ATTR_DISABLE_B_RATES,
	WDEV_ATTR_MAXASSOC_CLIENTS,
	WDEV_ATTR_LOCAL_PWR_CONSTRAINT,
	__WDEV_ATTR_MAX,
};

static const struct blobmsg_policy wifi_device_policy[__WDEV_ATTR_MAX] = {
	[WDEV_ATTR_PATH] = { .name = "path", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_DISABLED] = { .name = "disabled", .type = BLOBMSG_TYPE_BOOL },
	[WDEV_ATTR_CHANNEL] = { .name = "channel", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_TXPOWER] = { .name = "txpower", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_BEACON_INT] = { .name = "beacon_int", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_HTMODE] = { .name = "htmode", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_HWMODE] = { .name = "hwmode", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_COUNTRY] = { .name = "country", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_CHANBW] = { .name = "chanbw", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_TX_ANTENNA] = { .name = "txantenna", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_RX_ANTENNA] = { .name = "rxantenna", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_FREQ_BAND] = { .name = "freq_band", .type = BLOBMSG_TYPE_STRING },
	[WDEV_AATR_CHANNELS] = {.name = "channels", .type = BLOBMSG_TYPE_ARRAY},
	[WDEV_ATTR_DISABLE_B_RATES] = { .name = "legacy_rates", .type = BLOBMSG_TYPE_BOOL },
	[WDEV_ATTR_MAXASSOC_CLIENTS] = { .name = "maxassoc", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_LOCAL_PWR_CONSTRAINT] = { .name = "local_pwr_constraint", .type = BLOBMSG_TYPE_INT32 },
};

#define SCHEMA_CUSTOM_OPT_SZ            24
#define SCHEMA_CUSTOM_OPTS_MAX          3

static const char custom_options_table[SCHEMA_CUSTOM_OPTS_MAX][SCHEMA_CUSTOM_OPT_SZ] =
{
	SCHEMA_CONSTS_DISABLE_B_RATES,
	SCHEMA_CONSTS_MAX_CLIENTS,
	SCHEMA_CONSTS_LOCAL_PWR_CONSTRAINT,
};

static void radio_config_custom_opt_set(struct blob_buf *b, struct blob_buf *del,
                                      const struct schema_Wifi_Radio_Config *rconf)
{
	int i;
	char value[20];
	const char *opt;
	const char *val;

	for (i = 0; i < SCHEMA_CUSTOM_OPTS_MAX; i++) {
		opt = custom_options_table[i];
		val = SCHEMA_KEY_VAL(rconf->custom_options, opt);

		if (!val)
			strncpy(value, "0", 20);
		else
			strncpy(value, val, 20);

		if (strcmp(opt, SCHEMA_CONSTS_DISABLE_B_RATES ) == 0) {
			if (strcmp(value, "1") == 0)
				blobmsg_add_bool(b, "legacy_rates", 0);
			else 
				blobmsg_add_bool(del, "legacy_rates", 0);
		} else if (strcmp(opt, SCHEMA_CONSTS_MAX_CLIENTS) == 0) {
			int maxassoc;
			maxassoc = strtol(value, NULL, 10);
			if (maxassoc <= 0) {
				blobmsg_add_u32(del, "maxassoc", maxassoc);
			} else {
				if (maxassoc > 100)
					maxassoc = 100;
				blobmsg_add_u32(b, "maxassoc", maxassoc);
			}
		} else if (strcmp(opt, SCHEMA_CONSTS_LOCAL_PWR_CONSTRAINT) == 0) {
			int pwr = atoi(value);
			if (!strcmp(rconf->freq_band, "2.4G")) {
				blobmsg_add_u32(del, "local_pwr_constraint", pwr);
			} else if (pwr >= 0 && pwr <=255) {
				blobmsg_add_u32(b, "local_pwr_constraint", pwr);
			} else {
				blobmsg_add_u32(del, "local_pwr_constraint", pwr);
			}
		}
	}
}

static void set_custom_option_state(struct schema_Wifi_Radio_State *rstate,
                                    int *index, const char *key,
                                    const char *value)
{
	STRSCPY(rstate->custom_options_keys[*index], key);
	STRSCPY(rstate->custom_options[*index], value);
	*index += 1;
	rstate->custom_options_len = *index;
}

static void radio_state_custom_options_get(struct schema_Wifi_Radio_State *rstate,
                                         struct blob_attr **tb)
{
	int i;
	int index = 0;
	const char *opt;
	char buf[5];

	for (i = 0; i < SCHEMA_CUSTOM_OPTS_MAX; i++) {
		opt = custom_options_table[i];

		if ((strcmp(opt, SCHEMA_CONSTS_DISABLE_B_RATES) == 0) && (tb[WDEV_ATTR_DISABLE_B_RATES])) {
			if (blobmsg_get_bool(tb[WDEV_ATTR_DISABLE_B_RATES])) {
				set_custom_option_state(rstate, &index, opt, "0");
			} else {
				set_custom_option_state(rstate, &index, opt, "1");
			}
		} else if (strcmp(opt, SCHEMA_CONSTS_MAX_CLIENTS) == 0) {
			if (tb[WDEV_ATTR_MAXASSOC_CLIENTS]) {
				snprintf(buf, sizeof(buf), "%d", blobmsg_get_u32(tb[WDEV_ATTR_MAXASSOC_CLIENTS]));
				set_custom_option_state(rstate, &index, opt, buf);
			} else {
                                set_custom_option_state(rstate, &index, opt, "0");
			}
		} else if (strcmp(opt, SCHEMA_CONSTS_LOCAL_PWR_CONSTRAINT) == 0) {
			if (tb[WDEV_ATTR_LOCAL_PWR_CONSTRAINT]) {
				snprintf(buf, sizeof(buf), "%d", blobmsg_get_u32(tb[WDEV_ATTR_LOCAL_PWR_CONSTRAINT]));
				set_custom_option_state(rstate, &index, opt, buf);
			}
		}
	}
}

const struct uci_blob_param_list wifi_device_param = {
	.n_params = __WDEV_ATTR_MAX,
	.params = wifi_device_policy,
};

static bool radio_state_update(struct uci_section *s, struct schema_Wifi_Radio_Config *rconf, bool force)
{
	struct blob_attr *tb[__WDEV_ATTR_MAX] = { };
	struct schema_Wifi_Radio_State  rstate;
	char phy[6];
	int antenna;
	uint32_t chan = 0;

	LOGT("%s: get state", s->e.name);

	memset(&rstate, 0, sizeof(rstate));
	schema_Wifi_Radio_State_mark_all_present(&rstate);
	rstate._partial_update = true;
	rstate.channel_sync_present = false;
	rstate.channel_mode_present = false;
	rstate.radio_config_present = false;
	rstate.vif_states_present = false;

	blob_buf_init(&b, 0);
	uci_to_blob(&b, s, &wifi_device_param);
	blobmsg_parse(wifi_device_policy, __WDEV_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	SCHEMA_SET_STR(rstate.if_name, s->e.name);

	if (!tb[WDEV_ATTR_PATH] ||
	    phy_from_path(blobmsg_get_string(tb[WDEV_ATTR_PATH]), phy)) {
		LOGE("%s has no phy", rstate.if_name);
		return false;
	}

	if (tb[WDEV_ATTR_CHANNEL]) {
		nl80211_channel_get(phy, &chan);
		if(chan)
			SCHEMA_SET_INT(rstate.channel, chan);
		else
			SCHEMA_SET_INT(rstate.channel, blobmsg_get_u32(tb[WDEV_ATTR_CHANNEL]));
	}

	SCHEMA_SET_INT(rstate.enabled, 1);
	if (!force && tb[WDEV_ATTR_DISABLED] && blobmsg_get_bool(tb[WDEV_ATTR_DISABLED]))
		SCHEMA_SET_INT(rstate.enabled, 0);
	else
		SCHEMA_SET_INT(rstate.enabled, 1);

	if (tb[WDEV_ATTR_TXPOWER]) {
		SCHEMA_SET_INT(rstate.tx_power, blobmsg_get_u32(tb[WDEV_ATTR_TXPOWER]));
		/* 0 means max in UCI, 32 is max in OVSDB */
		if (rstate.tx_power == 0)
			rstate.tx_power = 32;
	} else
		SCHEMA_SET_INT(rstate.tx_power, 32);

	if (tb[WDEV_ATTR_BEACON_INT])
		SCHEMA_SET_INT(rstate.bcn_int, blobmsg_get_u32(tb[WDEV_ATTR_BEACON_INT]));
	else
		SCHEMA_SET_INT(rstate.bcn_int, 100);

	if (tb[WDEV_ATTR_HTMODE])
		SCHEMA_SET_STR(rstate.ht_mode, blobmsg_get_string(tb[WDEV_ATTR_HTMODE]));

	if (tb[WDEV_ATTR_HWMODE])
		SCHEMA_SET_STR(rstate.hw_mode, blobmsg_get_string(tb[WDEV_ATTR_HWMODE]));

	if (tb[WDEV_ATTR_TX_ANTENNA]) {
		antenna = blobmsg_get_u32(tb[WDEV_ATTR_TX_ANTENNA]);
		if (!antenna)
			antenna = phy_get_tx_available_antenna(phy);
		if ((antenna & 0xf0) && !(antenna & 0x0f))
			antenna= antenna >> 4;
		SCHEMA_SET_INT(rstate.tx_chainmask, antenna);
	}
	else
		SCHEMA_SET_INT(rstate.tx_chainmask, phy_get_tx_chainmask(phy));

	if (tb[WDEV_ATTR_RX_ANTENNA]) {

		antenna = blobmsg_get_u32(tb[WDEV_ATTR_RX_ANTENNA]);
		if (!antenna)
			antenna = phy_get_rx_available_antenna(phy);
		if ((antenna & 0xf0) && !(antenna & 0x0f))
			antenna= antenna >> 4;
		SCHEMA_SET_INT(rstate.rx_chainmask, antenna);
	}
	else
		SCHEMA_SET_INT(rstate.rx_chainmask, phy_get_rx_chainmask(phy));

	if (rstate.hw_mode_exists && rstate.ht_mode_exists) {
		struct mode_map *m = mode_map_get_cloud(rstate.ht_mode, rstate.hw_mode);

		if (m) {
			SCHEMA_SET_STR(rstate.hw_mode, m->hwmode);
			if (m->htmode)
				SCHEMA_SET_STR(rstate.ht_mode, m->htmode);
			else
				rstate.ht_mode_exists = false;
		} else {
			LOGE("%s: failed to decode ht/hwmode", rstate.if_name);
			rstate.hw_mode_exists = false;
			rstate.ht_mode_exists = false;
		}
	}

	if (tb[WDEV_ATTR_COUNTRY])
		SCHEMA_SET_STR(rstate.country, blobmsg_get_string(tb[WDEV_ATTR_COUNTRY]));
	else
		SCHEMA_SET_STR(rstate.country, "CA");

	rstate.allowed_channels_len = phy_get_channels(phy, rstate.allowed_channels);
	rstate.allowed_channels_present = true;

	if (tb[WDEV_ATTR_FREQ_BAND])
		SCHEMA_SET_STR(rstate.freq_band, blobmsg_get_string(tb[WDEV_ATTR_FREQ_BAND]));
	else if (!phy_get_band(phy, rstate.freq_band))
		rstate.freq_band_exists = true;

	rstate.channels_len = phy_get_channels_state(phy, &rstate);

	if (strcmp(rstate.freq_band, "2.4G"))	{
                STRSCPY(rstate.hw_config_keys[0], "dfs_enable");
                snprintf(rstate.hw_config[0], sizeof(rstate.hw_config[0]), "1");
                STRSCPY(rstate.hw_config_keys[1], "dfs_ignorecac");
                snprintf(rstate.hw_config[1], sizeof(rstate.hw_config[0]), "0");
                STRSCPY(rstate.hw_config_keys[2], "dfs_usenol");
                snprintf(rstate.hw_config[2], sizeof(rstate.hw_config[0]), "1");
                rstate.hw_config_len = 3;
	}

	if (!phy_get_mac(phy, rstate.mac))
		rstate.mac_exists = true;

	radio_state_custom_options_get(&rstate, tb);

	if (rconf) {
		LOGN("%s: updating radio config", rstate.if_name);
		radio_state_to_conf(&rstate, rconf);
		SCHEMA_SET_STR(rconf->hw_type, "ath10k");
		radio_ops->op_rconf(rconf);
	}
	LOGT("%s: updating radio state", rstate.if_name);
	radio_ops->op_rstate(&rstate);

	return true;
}

bool target_radio_config_set2(const struct schema_Wifi_Radio_Config *rconf,
			      const struct schema_Wifi_Radio_Config_flags *changed)
{
	blob_buf_init(&b, 0);
	blob_buf_init(&del, 0);

	char phy[6];
	char ifname[8];

	strncpy(ifname, rconf->if_name, sizeof(ifname));
	strncpy(phy, target_map_ifname(ifname), sizeof(phy));

	if (changed->channel && rconf->channel)
		blobmsg_add_u32(&b, "channel", rconf->channel);

	if (changed->enabled)
		blobmsg_add_u8(&b, "disabled", rconf->enabled ? 0 : 1);

	if (changed->tx_power) {
		int max_tx_power;
		max_tx_power=phy_get_max_tx_power(phy,rconf->channel);
		if (rconf->tx_power<=max_tx_power) {
			blobmsg_add_u32(&b, "txpower", rconf->tx_power);
		}
		else {
			blobmsg_add_u32(&b, "txpower", max_tx_power);
		}
	}

	if (changed->tx_chainmask) {
		int tx_ant_avail,txchainmask;
		tx_ant_avail=phy_get_tx_available_antenna(phy);

		if ((tx_ant_avail & 0xf0) && !(tx_ant_avail & 0x0f)) {
			txchainmask = rconf->tx_chainmask << 4;
		} else {
			txchainmask = rconf->tx_chainmask;
		}
		if (rconf->tx_chainmask == 0) {
			blobmsg_add_u32(&b, "txantenna", tx_ant_avail);
		} else if ((txchainmask & tx_ant_avail) != txchainmask) {
			blobmsg_add_u32(&b, "txantenna", tx_ant_avail);
		} else {
			blobmsg_add_u32(&b, "txantenna", txchainmask);
		}
	}

	if (changed->rx_chainmask) {
		int rx_ant_avail, rxchainmask;
		rx_ant_avail=phy_get_rx_available_antenna(phy);

		if ((rx_ant_avail & 0xf0) && !(rx_ant_avail & 0x0f)) {
			rxchainmask = rconf->rx_chainmask << 4;
		} else {
			rxchainmask = rconf->rx_chainmask;
		}
		if (rconf->rx_chainmask == 0) {
			blobmsg_add_u32(&b, "rxantenna", rx_ant_avail);
		} else if ((rxchainmask & rx_ant_avail) != rxchainmask) {
			blobmsg_add_u32(&b, "rxantenna", rx_ant_avail);
		} else {
			blobmsg_add_u32(&b, "rxantenna", rxchainmask);
		}
	}

	if (changed->country)
		blobmsg_add_string(&b, "country", rconf->country);

	if (changed->bcn_int) {
		int beacon_int = rconf->bcn_int;

		if ((rconf->bcn_int < 50) || (rconf->bcn_int > 400))
			beacon_int = 100;
		blobmsg_add_u32(&b, "beacon_int", beacon_int);
	}

	if ((changed->ht_mode) || (changed->hw_mode) || (changed->freq_band)) {
		int channel_freq;
		channel_freq = ieee80211_channel_to_frequency(rconf->channel);
		struct mode_map *m = mode_map_get_uci(rconf->freq_band, get_max_channel_bw_channel(channel_freq, rconf->ht_mode), rconf->hw_mode);
		if (m) {
			blobmsg_add_string(&b, "htmode", m->ucihtmode);
			blobmsg_add_string(&b, "hwmode", m->ucihwmode);
			blobmsg_add_u32(&b, "chanbw", 20);
		} else
			 LOGE("%s: failed to set ht/hwmode", rconf->if_name);
	}

	struct blob_attr *n;
	int backup_channel = 0;
	backup_channel = rrm_get_backup_channel(rconf->freq_band);
	if(backup_channel) {
		n = blobmsg_open_array(&b, "channels");
		blobmsg_add_u32(&b, NULL, backup_channel);
		blobmsg_close_array(&b, n);
	} else {
		n = blobmsg_open_array(&del, "channels");
		blobmsg_add_u32(&del, NULL, backup_channel);
		blobmsg_close_array(&del, n);
	}	

	if (changed->custom_options)
		radio_config_custom_opt_set(&b, &del, rconf);

	blob_to_uci_section(uci, "wireless", rconf->if_name, "wifi-device",
			    b.head, &wifi_device_param, del.head);

	reload_config = 1;

	return true;
}

static void periodic_task(void *arg)
{
	static int counter = 0;
	struct uci_element *e = NULL, *tmp = NULL;
	int ret = 0;

	if ((counter % 15) && !reload_config)
		goto done;

	if (startup_time.tv_sec) {
		static struct timespec current_time;

		clock_gettime(CLOCK_MONOTONIC, &current_time);
		current_time.tv_sec -= startup_time.tv_sec;
		if (current_time.tv_sec > 60 * 10) {
			startup_time.tv_sec = 0;
			radio_maverick(NULL);
		}
	}

	if (reload_config) {
		LOGT("periodic: reload config");
		reload_config = 0;
		uci_commit_all(uci);
		sync();
		system("reload_config");
	}

	LOGD("periodic: start state update ");
	ret = uci_load(uci, "wireless", &wireless);
	if (ret) {
		LOGE("%s: uci_load() failed with rc %d", __func__, ret);
		return;
	}
	uci_foreach_element_safe(&wireless->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-device"))
			radio_state_update(s, NULL, false);
	}

	uci_foreach_element_safe(&wireless->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-iface"))
			vif_state_update(s, NULL);
	}
	uci_unload(uci, wireless);
	LOGD("periodic: stop state update ");

done:
	counter++;
	evsched_task_reschedule_ms(EVSCHED_SEC(1));
}

bool target_radio_config_init2(void)
{
	struct schema_Wifi_Radio_Config rconf;
	struct schema_Wifi_VIF_Config vconf;
	struct uci_element *e = NULL, *tmp = NULL;
	int invalidVifFound = 0;

	uci_load(uci, "wireless", &wireless);
	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-device"))
			radio_state_update(s, &rconf, false);
	}

	uci_foreach_element_safe(&wireless->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);
		int radio_idx;

		if (!strcmp(s->type, "wifi-iface")) {
			if (sscanf((char*)s->e.name, "wlan%d", &radio_idx)) {
				vif_state_update(s, &vconf);
			} else {
				uci_section_del(uci, "vif", "wireless", (char *)s->e.name, "wifi-iface");
				invalidVifFound = 1;
			}
		}
	}
	if (invalidVifFound) {
		uci_commit(uci, &wireless, false);
		reload_config = 1;
	}
	uci_unload(uci, wireless);

	return true;
}

enum {
	SYS_ATTR_DEPLOYED,
	__SYS_ATTR_MAX,
};

static const struct blobmsg_policy system_policy[__SYS_ATTR_MAX] = {
	[SYS_ATTR_DEPLOYED] = { .name = "deployed", .type = BLOBMSG_TYPE_INT32 },
};

static const struct uci_blob_param_list system_param = {
	.n_params = __SYS_ATTR_MAX,
	.params = system_policy,
};

void radio_maverick(void *arg)
{
	struct blob_attr *tb[__SYS_ATTR_MAX] = { };
	struct schema_Wifi_Radio_Config rconf;
	struct uci_element *e = NULL;

	blob_buf_init(&b, 0);
	if (uci_section_to_blob(uci, "system", "tip", &b, &system_param))
		return;

	blobmsg_parse(system_policy, __SYS_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));
	if (tb[SYS_ATTR_DEPLOYED] && blobmsg_get_u32(tb[SYS_ATTR_DEPLOYED]))
		return;

	uci_load(uci, "wireless", &wireless);
	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);
		struct schema_Wifi_VIF_Config conf;
		char band[8];
		char ifname[] = "wlan0";

		if (strcmp(s->type, "wifi-device"))
			continue;
		radio_state_update(s, &rconf, true);

		if (strncmp(rconf.if_name, "radio", 5))
			continue;

		ifname[4] = rconf.if_name[5];
		memset(&conf, 0, sizeof(conf));
		schema_Wifi_VIF_Config_mark_all_present(&conf);
		conf._partial_update = true;
		conf._partial_update = true;

	        SCHEMA_SET_INT(conf.rrm, 1);
	        SCHEMA_SET_INT(conf.ft_psk, 0);
	        SCHEMA_SET_INT(conf.group_rekey, 0);
		strscpy(conf.mac_list_type, "none", sizeof(conf.mac_list_type));
		conf.mac_list_len = 0;
		SCHEMA_SET_STR(conf.if_name, ifname);
		SCHEMA_SET_STR(conf.ssid_broadcast, "enabled");
		SCHEMA_SET_STR(conf.mode, "ap");
		SCHEMA_SET_INT(conf.enabled, 1);
		SCHEMA_SET_INT(conf.btm, 1);
		SCHEMA_SET_INT(conf.uapsd_enable, true);
		SCHEMA_SET_STR(conf.bridge, "lan");
		SCHEMA_SET_INT(conf.vlan_id, 1);
		SCHEMA_SET_STR(conf.ssid, "Maverick");
		STRSCPY(conf.security_keys[0], "encryption");
	        STRSCPY(conf.security[0], "OPEN");
	        conf.security_len = 1;
		phy_get_band(target_map_ifname(rconf.if_name), band);
		if (strstr(band, "5"))
			SCHEMA_SET_STR(conf.min_hw_mode, "11ac");
		else
			SCHEMA_SET_STR(conf.min_hw_mode, "11n");

		radio_ops->op_vconf(&conf, rconf.if_name);
	}
	uci_unload(uci, wireless);
}

static void callback_Hotspot20_Config(ovsdb_update_monitor_t *mon,
				 struct schema_Hotspot20_Config *old,
				 struct schema_Hotspot20_Config *conf)
{

	switch (mon->mon_type)
	{
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		(void) 	vif_hs20_update(conf);
		break;

	case OVSDB_UPDATE_DEL:
		break;

	default:
		LOG(ERR, "Hotspot20_Config: unexpected mon_type %d %s", mon->mon_type, mon->mon_uuid);
		break;
	}
	return;
}

static void callback_Hotspot20_OSU_Providers(ovsdb_update_monitor_t *mon,
				 struct schema_Hotspot20_OSU_Providers *old,
				 struct schema_Hotspot20_OSU_Providers *conf)
{
	switch (mon->mon_type)
	{
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		(void) 	vif_hs20_osu_update(conf);
		break;

	case OVSDB_UPDATE_DEL:
		(void) vif_section_del("osu-provider");
		break;

	default:
		LOG(ERR, "Hotspot20_OSU_Providers: unexpected mon_type %d %s",
				mon->mon_type, mon->mon_uuid);
		break;
	}
	return;
}

static void callback_Hotspot20_Icon_Config(ovsdb_update_monitor_t *mon,
				 struct schema_Hotspot20_Icon_Config *old,
				 struct schema_Hotspot20_Icon_Config *conf)
{

	switch (mon->mon_type)
	{
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		(void) 	vif_hs20_icon_update(conf);
		break;

	case OVSDB_UPDATE_DEL:
		(void) vif_section_del("hs20-icon");
		break;

	default:
		LOG(ERR, "Hotspot20_Icon_Config: unexpected mon_type %d %s",
				mon->mon_type, mon->mon_uuid);
		break;
	}
	return;

}

enum {
	WIF_APC_ENABLE,
	__WIF_APC_MAX,
};

static const struct blobmsg_policy apc_enpolicy[__WIF_APC_MAX] = {
		[WIF_APC_ENABLE] = { .name = "enabled", BLOBMSG_TYPE_BOOL },
};

const struct uci_blob_param_list apc_param = {
	.n_params = __WIF_APC_MAX,
	.params = apc_enpolicy,
};

void APC_config_update(struct schema_APC_Config *conf)
{
	struct blob_buf apcb = { };
	struct uci_context *apc_uci;

	apc_uci = uci_alloc_context();

	blob_buf_init(&apcb, 0);
	if (conf && conf->enabled == true) {
		blobmsg_add_bool(&apcb, "enabled", 1);
		system("/etc/init.d/apc start");
	} else {
		blobmsg_add_bool(&apcb, "enabled", 0);
		system("/etc/init.d/apc stop");
	}

	blob_to_uci_section(apc_uci, "apc", "apc", "apc",
			apcb.head, &apc_param, NULL);

	uci_commit_all(apc_uci);
	uci_free_context(apc_uci);
}

static void callback_APC_Config(ovsdb_update_monitor_t *mon,
                                struct schema_APC_Config *old,
                                struct schema_APC_Config *conf)
{
	if (mon->mon_type == OVSDB_UPDATE_DEL)
		APC_config_update(NULL);
	else
		APC_config_update(conf);

}

static void callback_APC_State(ovsdb_update_monitor_t *mon,
                                struct schema_APC_State *old,
                                struct schema_APC_State *conf)
{
	LOGN("APC_state: enabled:%s dr_addr:%s bdr_addr:%s mode:%s",
	     (conf->enabled_changed)? "changed":"unchanged", 
	     (conf->dr_addr_changed)? "changed":"unchanged",
	     (conf->bdr_addr_changed)? "changed":"unchanged",
	     (conf->mode_changed)? "changed":"unchanged");

	/* APC changed: if radproxy enabled then restart wireless */
	if (radproxy_apc) {
		radproxy_apc = 0;
		system("ubus call service event '{\"type\": \"config.change\", \"data\": { \"package\": \"wireless\" }}'");
	}

	/* APC changed: start / stop radius proxy service if needed */
	vif_check_radius_proxy();

}

struct schema_APC_State apc_state;
enum {
	APC_ATTR_MODE,
	APC_ATTR_DR_ADDR,
	APC_ATTR_BDR_ADDR,
	APC_ATTR_ENABLED,
	__APC_ATTR_MAX,
};

static const struct blobmsg_policy apc_policy[__APC_ATTR_MAX] = {
	[APC_ATTR_MODE] = { .name = "mode", .type = BLOBMSG_TYPE_STRING },
	[APC_ATTR_DR_ADDR] = { .name = "dr_addr", .type = BLOBMSG_TYPE_STRING },
	[APC_ATTR_BDR_ADDR] = { .name = "bdr_addr", .type = BLOBMSG_TYPE_STRING },
	[APC_ATTR_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
};

struct schema_APC_Config apc_conf;

void apc_state_set(struct blob_attr *msg)
{
	struct blob_attr *tb[__APC_ATTR_MAX] = { };

	blobmsg_parse(apc_policy, __APC_ATTR_MAX, tb,
		      blob_data(msg), blob_len(msg));

	if (tb[APC_ATTR_MODE]) {
		LOGD("APC mode: %s", blobmsg_get_string(tb[APC_ATTR_MODE]));
		SCHEMA_SET_STR(apc_state.mode,
			       blobmsg_get_string(tb[APC_ATTR_MODE]));
	}
	if (tb[APC_ATTR_DR_ADDR]) {
		LOGD("APC br-addr: %s", blobmsg_get_string(tb[APC_ATTR_DR_ADDR]));
		SCHEMA_SET_STR(apc_state.dr_addr,
			       blobmsg_get_string(tb[APC_ATTR_DR_ADDR]));
	}
	if (tb[APC_ATTR_BDR_ADDR]) {
		LOGD("APC dbr-addr: %s", blobmsg_get_string(tb[APC_ATTR_BDR_ADDR]));
		SCHEMA_SET_STR(apc_state.bdr_addr,
			       blobmsg_get_string(tb[APC_ATTR_BDR_ADDR]));
	}
	if (tb[APC_ATTR_ENABLED]) {
		LOGD("APC enabled: %d", blobmsg_get_bool(tb[APC_ATTR_ENABLED]));
		if (blobmsg_get_bool(tb[APC_ATTR_ENABLED])) {
			SCHEMA_SET_INT(apc_state.enabled, true);
		}
		else {
			SCHEMA_SET_INT(apc_state.enabled, false);
		}
	}

	LOGD("APC_state Updating");
	if (!ovsdb_table_update(&table_APC_State, &apc_state))
		LOG(ERR, "APC_state: failed to update");
}


void apc_init()
{
	/* APC Config */
	OVSDB_TABLE_INIT(APC_Config, _uuid);
	OVSDB_TABLE_MONITOR(APC_Config, false);
	SCHEMA_SET_INT(apc_conf.enabled, true);
	LOGI("APC state/config Initialize");
	if (!ovsdb_table_insert(&table_APC_Config, &apc_conf))
		LOG(ERR, "APC_Config: failed to initialize");

	/* APC State */
	OVSDB_TABLE_INIT_NO_KEY(APC_State);
	OVSDB_TABLE_MONITOR(APC_State, false);
	SCHEMA_SET_STR(apc_state.mode, "NC");
	SCHEMA_SET_STR(apc_state.dr_addr, "0.0.0.0");
	SCHEMA_SET_STR(apc_state.bdr_addr, "0.0.0.0");
	SCHEMA_SET_INT(apc_state.enabled, false);
	if (!ovsdb_table_insert(&table_APC_State, &apc_state))
		LOG(ERR, "APC_state: failed to initialize");
}

bool target_radio_init(const struct target_radio_ops *ops)
{
	uci = uci_alloc_context();

	captive_portal_init();
	target_map_init();

	target_map_insert("radio0", "phy0");
	target_map_insert("radio1", "phy1");
	target_map_insert("radio2", "phy2");

	radio_ops = ops;

	OVSDB_TABLE_INIT(Hotspot20_Config, _uuid);
	OVSDB_TABLE_MONITOR(Hotspot20_Config, false);

	OVSDB_TABLE_INIT(Hotspot20_OSU_Providers, _uuid);
	OVSDB_TABLE_MONITOR(Hotspot20_OSU_Providers, false);

	OVSDB_TABLE_INIT(Hotspot20_Icon_Config, _uuid);
	OVSDB_TABLE_MONITOR(Hotspot20_Icon_Config, false);

	OVSDB_TABLE_INIT(Wifi_RRM_Config, _uuid);
	OVSDB_TABLE_MONITOR(Wifi_RRM_Config, false);

	OVSDB_TABLE_INIT(Radius_Proxy_Config, _uuid);
	OVSDB_TABLE_MONITOR(Radius_Proxy_Config, false);

	apc_init();

	evsched_task(&periodic_task, NULL, EVSCHED_SEC(5));

	radio_nl80211_init();
	radio_ubus_init();

	clock_gettime(CLOCK_MONOTONIC, &startup_time);

	return true;
}

bool target_radio_config_need_reset(void)
{
	return true;
}

