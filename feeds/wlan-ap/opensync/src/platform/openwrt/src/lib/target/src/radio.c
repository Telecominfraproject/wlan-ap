/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdbool.h>

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

ovsdb_table_t table_Hotspot20_Config;

static struct uci_package *wireless;
struct uci_context *uci;
struct blob_buf b = { };
struct blob_buf del = { };
int reload_config = 0;

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
	WDEV_ATTR_FREQ_BAND,
	WDEV_ATTR_DISABLE_B_RATES,
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
	[WDEV_ATTR_TX_ANTENNA] = { .name = "tx_antenna", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_FREQ_BAND] = { .name = "freq_band", .type = BLOBMSG_TYPE_STRING },
        [WDEV_ATTR_DISABLE_B_RATES] = { .name = "legacy_rates", .type = BLOBMSG_TYPE_BOOL },
};

#define SCHEMA_CUSTOM_OPT_SZ            20
#define SCHEMA_CUSTOM_OPTS_MAX          1

static const char custom_options_table[SCHEMA_CUSTOM_OPTS_MAX][SCHEMA_CUSTOM_OPT_SZ] =
{
	SCHEMA_CONSTS_DISABLE_B_RATES,
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

	for (i = 0; i < SCHEMA_CUSTOM_OPTS_MAX; i++) {
		opt = custom_options_table[i];

		if ((strcmp(opt, SCHEMA_CONSTS_DISABLE_B_RATES) == 0) && (tb[WDEV_ATTR_DISABLE_B_RATES])) {
			if (blobmsg_get_bool(tb[WDEV_ATTR_DISABLE_B_RATES])) {
				set_custom_option_state(rstate, &index, opt, "0");
			} else {
				set_custom_option_state(rstate, &index, opt, "1");
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

	LOGN("%s: get state", s->e.name);

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

	if (tb[WDEV_ATTR_CHANNEL])
		SCHEMA_SET_INT(rstate.channel, blobmsg_get_u32(tb[WDEV_ATTR_CHANNEL]));

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

	if (tb[WDEV_ATTR_TX_ANTENNA])
		SCHEMA_SET_INT(rstate.tx_chainmask, blobmsg_get_u32(tb[WDEV_ATTR_TX_ANTENNA]));
	else
		SCHEMA_SET_INT(rstate.tx_chainmask, phy_get_tx_chainmask(phy));

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
	LOGN("%s: updating radio state", rstate.if_name);
	radio_ops->op_rstate(&rstate);

	return true;
}

bool target_radio_config_set2(const struct schema_Wifi_Radio_Config *rconf,
			      const struct schema_Wifi_Radio_Config_flags *changed)
{
	blob_buf_init(&b, 0);
	blob_buf_init(&del, 0);

	if (changed->channel && rconf->channel)
		blobmsg_add_u32(&b, "channel", rconf->channel);

	if (changed->enabled)
		blobmsg_add_u8(&b, "disabled", rconf->enabled ? 0 : 1);

	if (changed->tx_power)
		blobmsg_add_u32(&b, "txpower", rconf->tx_power);

	if (changed->tx_chainmask)
		blobmsg_add_u32(&b, "tx_antenna", rconf->tx_chainmask);

	if (changed->country)
		blobmsg_add_string(&b, "country", rconf->country);

	if (changed->bcn_int) {
		int beacon_int = rconf->bcn_int;

		if ((rconf->bcn_int < 50) || (rconf->bcn_int > 400))
			beacon_int = 100;
		blobmsg_add_u32(&b, "beacon_int", beacon_int);
	}

	if ((changed->ht_mode) || (changed->hw_mode) || (changed->freq_band)) {
		struct mode_map *m = mode_map_get_uci(rconf->freq_band, rconf->ht_mode,
						      rconf->hw_mode);
		if (m) {
			blobmsg_add_string(&b, "htmode", m->ucihtmode);
			blobmsg_add_string(&b, "hwmode", m->ucihwmode);
			blobmsg_add_u32(&b, "chanbw", 20);
		} else
			 LOGE("%s: failed to set ht/hwmode", rconf->if_name);
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

	if ((counter % 15) && !reload_config)
		goto done;

	if (reload_config) {
		LOGT("periodic: reload config");
		reload_config = 0;
		uci_commit_all(uci);
		sync();
		system("reload_config");
	}

	LOGT("periodic: start state update ");

	uci_load(uci, "wireless", &wireless);
	uci_foreach_element_safe(&wireless->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-device"))
			radio_state_update(s, NULL, false);
	}

	uci_foreach_element_safe(&wireless->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-iface"))
			if (vif_find(s->e.name))
				vif_state_update(s, NULL);
	}
	uci_unload(uci, wireless);
	LOGT("periodic: stop state update ");

done:
	counter++;
	evsched_task_reschedule_ms(EVSCHED_SEC(1));
}

bool target_radio_config_init2(void)
{
	struct schema_Wifi_Radio_Config rconf;
	struct schema_Wifi_VIF_Config vconf;
	struct uci_element *e = NULL;

	uci_load(uci, "wireless", &wireless);
	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-device"))
			radio_state_update(s, &rconf, false);
	}

	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-iface"))
			vif_state_update(s, &vconf);
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

	evsched_task(&periodic_task, NULL, EVSCHED_SEC(5));

	radio_nl80211_init();
	radio_ubus_init();

	evsched_task(&radio_maverick, NULL, EVSCHED_SEC(60 * 10));

	return true;
}

bool target_radio_config_need_reset(void)
{
	return true;
}
