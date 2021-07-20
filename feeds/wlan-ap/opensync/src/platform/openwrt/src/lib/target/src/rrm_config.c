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
#include <fcntl.h>
#include "fixup.h"

ovsdb_table_t table_Wifi_RRM_Config;
extern ovsdb_table_t table_Wifi_Radio_Config;
extern ovsdb_table_t table_Wifi_VIF_Config;
#define PHY_NAME_LEN 32

bool rrm_config_txpower(const char *rname, unsigned int txpower)
{
	char cmd[126] = {0};
	int rid = 0;
	txpower = txpower * 100;

	sscanf(rname, "radio%d", &rid);

	memset(cmd, 0, sizeof(cmd));
	/* iw dev <devname> set txpower <auto|fixed|limit> [<tx power in mBm>]*/
	snprintf(cmd, sizeof(cmd),
		"iw phy phy%d set txpower fixed %d", rid, txpower);
	system(cmd);

	return true;
}

/* Mcast & Beacon rate set */
#define NUM_OF_RATES 12
enum {
	ATH10K_DRIVER,
	ATH11K_DRIVER,
	ATH_DRIVER_MAX
};

typedef struct ath_rate_codes {
	unsigned int rate;
	unsigned long code[ATH_DRIVER_MAX];
} ath_rc;

char ath_driver_name[ATH_DRIVER_MAX][16] = {"ath10k", "ath11k"};
ath_rc  ath_rcodes[NUM_OF_RATES] = {{1,  {0x43, 0x10000103}},
				    {2,  {0x42, 0x10000102}},
				    {5,  {0x41, 0x10000101}},
				    {11, {0x40, 0x10000100}},
				    {6,  {0x3, 0x10000003}},
				    {9,  {0x7, 0x10000007}},
				    {12, {0x2, 0x10000002}},
				    {18, {0x6, 0x10000006}},
				    {24, {0x1, 0x10000001}},
				    {36, {0x5, 0x10000005}},
				    {48, {0x0, 0x10000000}},
				    {54, {0x4, 0x10000004}}};


/* get phy name */
bool get_80211phy(const char *ifname, char *phy)
{
	char path[126] = {0};
	int fd, l = 0;

	snprintf(path, sizeof(path), "/sys/class/net/%s/phy80211/name", ifname);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		LOGE("%s: Unable to read sysfs phy name", __func__);
		close(fd);
		return false;
	}
	read(fd, phy, PHY_NAME_LEN);
	close(fd);
	l = strlen(phy);
	phy[l-1] = '\0';

	return true;
}

bool set_rates_sysfs(const char *type, int rate_code, const char *ifname,
		     char *band, int dn)
{
	int fd = 0;
	char path[126] = {0};
	char value[126] = {0};
	char phy[PHY_NAME_LEN] = {0};
	unsigned int band_id = 0;

	/* get phy name */
	if (!get_80211phy(ifname, phy))
		return false;

	if (!strcmp(band, "2.4G"))
		band_id = 2;
	else
		band_id = 5;

	snprintf(path, sizeof(path),
		"/sys/kernel/debug/ieee80211/%s/%s/set_rates", phy,
		ath_driver_name[dn]);

	snprintf(value, sizeof(value), "%s %s %d 0x%x",
		 ifname, type, band_id, rate_code);

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		LOGE("%s: Unable to read sysfs set_rates", __func__);
		close(fd);
		return false;
	}
	write(fd, value, sizeof(value));
	close(fd);
	return true;
}

int find_ath_driver_name(const char *ifname)
{
	int dn = -1;
	char path[126] = {0};
	char phy[PHY_NAME_LEN] = {0};

	/* get phy name */
	if (!get_80211phy(ifname, phy))
		return -1;

	for (dn = 0; dn < ATH_DRIVER_MAX; dn++) {
		snprintf(path, sizeof(path),
			"/sys/kernel/debug/ieee80211/%s/%s/set_rates", phy,
			ath_driver_name[dn]);

		if (access(path, F_OK) == 0) {
			return dn;
		}
	}

	return -1;
}

int rrm_config_mcast_bcast_rate(const char *ifname, char *band,
				 unsigned int bcn_rate, unsigned int mcast_rate)
{
	int i = 0;
	bool rc = 0;
	unsigned int bcn_code = 0xFF;
	unsigned int mcast_code = 0xFF;
	int dn = 0;

	/*Find ath driver version ath10/11k*/
	dn = find_ath_driver_name(ifname);
	if (dn < -1) {
		LOG(ERR, "%s: No set_rate path exists", __func__);
		return -1;
	}

	/* beacon rate given by cloud in multiples of 10 */
	if (bcn_rate > 0)
		bcn_rate = bcn_rate/10;

	/* Get rate code of given rate */
	for (i = 0; i < NUM_OF_RATES; i++)
	{
		if (ath_rcodes[i].rate == bcn_rate)
			bcn_code = ath_rcodes[i].code[dn];

		if (ath_rcodes[i].rate == mcast_rate)
			mcast_code = ath_rcodes[i].code[dn];
	}

	/* Set the rates to sysfs */
	if (bcn_code != 0xFF)
	{
		if (set_rates_sysfs("beacon", bcn_code, ifname, band, dn) == false)
			rc = -1;

		/*Ath11k sets mgmt and beacon rates separately*/
		if (dn == ATH11K_DRIVER)
			if (set_rates_sysfs("mgmt", bcn_code, ifname, band, dn) == false)
				rc = -1;
	}

	if (mcast_code != 0xFF)
	{
		if (set_rates_sysfs("mcast", mcast_code, ifname, band, dn) == false)
			rc = -1;
	}

	return rc;
}

void rrm_config_vif(struct blob_buf *b, struct blob_buf *del, const char * freq_band, const char * if_name)
{
	struct schema_Wifi_RRM_Config conf;

	memset(&conf, 0, sizeof(conf));
	if (false == ovsdb_table_select_one(&table_Wifi_RRM_Config,
			SCHEMA_COLUMN(Wifi_RRM_Config, freq_band), freq_band, &conf))
	{
		LOG(DEBUG, "Wifi_VIF_Config: No RRM for band %s", freq_band );
		blobmsg_add_u32(del, "rssi_ignore_probe_request", -90);
		blobmsg_add_u32(del, "signal_connect", -90);
		blobmsg_add_u32(del, "signal_stay", -90);
		blobmsg_add_u32(del, "bcn_rate", 0);
		blobmsg_add_u32(del, "mcast_rate", 0);
	} else {
		blobmsg_add_u32(b, "rssi_ignore_probe_request", conf.probe_resp_threshold);
		blobmsg_add_u32(b, "signal_connect", conf.client_disconnect_threshold);
		blobmsg_add_u32(b, "signal_stay", conf.client_disconnect_threshold);
		blobmsg_add_u32(b, "mcast_rate", conf.mcast_rate);

		if (conf.beacon_rate == 0) {
			// Default to the lowest possible bit rate for each frequency band
			if (!strcmp(freq_band, "2.4G")) {
				blobmsg_add_u32(b, "bcn_rate", 10);
			} else {
				blobmsg_add_u32(b, "bcn_rate", 60);
			}
		} else {
			blobmsg_add_u32(b, "bcn_rate", conf.beacon_rate);
		}
		
	}
	return;
}

int rrm_get_backup_channel(const char * freq_band)
{
	struct schema_Wifi_RRM_Config conf;

	memset(&conf, 0, sizeof(conf));
	if (false == ovsdb_table_select_one(&table_Wifi_RRM_Config,
			SCHEMA_COLUMN(Wifi_RRM_Config, freq_band), freq_band, &conf))
	{
		LOG(ERR, "Wifi_Radio_Config: No RRM for band %s", freq_band );
		return 0;
	}

	return conf.backup_channel;

}

void rrm_noreload_config(struct schema_Wifi_RRM_Config *conf, const char *wlanif)
{
	char pval[16];
	int rc = 0;

	if (conf->probe_resp_threshold_changed) {
		/* rssi_ignore_probe_request and signal_connect should
		 * both be probe_resp_threshold */ 
		snprintf(pval, sizeof(pval), "%d",
			 conf->probe_resp_threshold);
		rc = ubus_set_hapd_param(wlanif,
				    "rssi_ignore_probe_request", pval);
		if (rc < 0)
			LOG(ERR, "RRM: Set rssi_ignore_probe_req failed");
		rc = ubus_set_signal_thresholds(wlanif,
					   conf->probe_resp_threshold,
					   conf->client_disconnect_threshold);
		if (rc < 0)
			LOG(ERR, "RRM: Set probe_resp_threshold failed");


	}

	if (conf->client_disconnect_threshold_changed) {
		rc = ubus_set_signal_thresholds(wlanif,
					   conf->probe_resp_threshold,
					   conf->client_disconnect_threshold);
		if (rc < 0)
			LOG(ERR, "RRM: Set client_disconnect_thres failed");

	}

	if (conf->mcast_rate_changed) {
		rc = rrm_config_mcast_bcast_rate(wlanif, conf->freq_band, 0,
					    conf->mcast_rate);
		if (rc < 0)
			LOG(ERR, "RRM: Set mcast rate failed");

	}

	if (conf->beacon_rate_changed) {
		rrm_config_mcast_bcast_rate(wlanif, conf->freq_band,
					    conf->beacon_rate, 0);
		if (rc < 0)
			LOG(ERR, "RRM: Set beacon rate failed");

	}
}

void get_channel_bandwidth(const char* htmode, int *channel_bandwidth)
{
	if(!strcmp(htmode, "HT20"))
		*channel_bandwidth=20;
	else if (!strcmp(htmode, "HT40"))
		*channel_bandwidth=40;
	else if(!strcmp(htmode, "HT80"))
		*channel_bandwidth=80;
}

void rrm_radio_rebalance_channel(const struct schema_Wifi_Radio_Config *rconf)
{
	int channel_bandwidth;
	int sec_chan_offset=0;
	struct mode_map *m = NULL;
	int freq = 0;
	char *mode = NULL;
	int rid = 0;
	char wlanif[16] = {0};
	char *hw_mode;

	sscanf(rconf->if_name, "radio%d", &rid);
	snprintf(wlanif, sizeof(wlanif), "wlan%d", rid);

	freq = ieee80211_channel_to_frequency(rconf->channel);
	mode = get_max_channel_bw_channel(freq,	rconf->ht_mode);

	hw_mode = radio_fixup_get_hw_mode(rconf->if_name);
	if (hw_mode == NULL) {
		LOGE("Falied to get hw mode");
		return;
	}

	m = mode_map_get_uci(rconf->freq_band, mode, hw_mode);

	if (m)
		sec_chan_offset = m->sec_channel_offset;
	else
		LOGE("failed to get channel offset");

	get_channel_bandwidth(mode, &channel_bandwidth);


	ubus_set_channel_switch(wlanif, freq, hw_mode,
				channel_bandwidth, sec_chan_offset);
}

static bool rrm_config_update( struct schema_Wifi_RRM_Config *conf, bool addNotDelete)
{
	struct schema_Wifi_Radio_Config rconf;
	struct schema_Wifi_VIF_Config vconf;
	struct schema_Wifi_VIF_Config_flags changed;
	json_t *where;
	int i;

	if (false == ovsdb_table_select_one(&table_Wifi_Radio_Config,
			SCHEMA_COLUMN(Wifi_Radio_Config, freq_band),
			conf->freq_band, &rconf))
	{
		LOG(WARN, "Wifi_RRM_Config: No radio for band %s",
						conf->freq_band );
		return false;
	}	

	/* Set RRM configurations which do not require a wifi vifs reload
	 * and return */
	if (conf->mcast_rate_changed || conf->beacon_rate_changed ||
	    conf->probe_resp_threshold_changed ||
	    conf->client_disconnect_threshold_changed) {
		LOGI("RRM Config: beacon_rate:%s mcast_rate:%s probe_resp_threshold:%s client_disconnect_threshold_changed:%s",
		     (conf->beacon_rate_changed)? "changed":"unchanged", 
		     (conf->mcast_rate_changed)? "changed":"unchanged", 
		     (conf->probe_resp_threshold_changed)? "changed":"unchanged",
		     (conf->client_disconnect_threshold_changed)? "changed":"unchanged");

		for (i = 0; i < rconf.vif_configs_len; i++) {
			if (!(where = ovsdb_where_uuid("_uuid",
						rconf.vif_configs[i].uuid)))
				continue;

			memset(&vconf, 0, sizeof(vconf));
			if (ovsdb_table_select_one_where(&table_Wifi_VIF_Config,
							 where, &vconf))
			{
				rrm_noreload_config(conf, vconf.if_name);
			}
		}
	}

	/*Reload the vifs to configure the RRM configurations*/
	memset(&changed, 0, sizeof(changed));
	for (i = 0; i < rconf.vif_configs_len; i++) {
		if (!(where = ovsdb_where_uuid("_uuid", rconf.vif_configs[i].uuid)))
			continue;

		memset(&vconf, 0, sizeof(vconf));
		if (ovsdb_table_select_one_where(&table_Wifi_VIF_Config, where, &vconf))
		{
			LOG(DEBUG, "RRM band %s updates vif %s", conf->freq_band, vconf.if_name);
			target_vif_config_set2(&vconf, &rconf, NULL, &changed, 0);
		}
	}
	return true;
}

static bool rrm_config_changed( struct schema_Wifi_RRM_Config *old,
	struct schema_Wifi_RRM_Config *conf )
{
	if ((conf->probe_resp_threshold != old->probe_resp_threshold) ||
		(conf->client_disconnect_threshold != old->client_disconnect_threshold) ||
		(conf->beacon_rate != old->beacon_rate) ||
		(conf->mcast_rate != old->mcast_rate))
	{
		return true;
	}
	return false;
}

static bool rrm_radio_config_update(struct schema_Wifi_RRM_Config *conf )
{
	struct schema_Wifi_Radio_Config_flags changed;
	struct schema_Wifi_Radio_Config rconf;

	if (false == ovsdb_table_select_one(&table_Wifi_Radio_Config,
			SCHEMA_COLUMN(Wifi_Radio_Config, freq_band), conf->freq_band, &rconf))
	{
		LOG(WARN, "Wifi_RRM_Config: No radio for band %s", conf->freq_band );
		return false;
	}

	memset(&changed, 0, sizeof(changed));
	target_radio_config_set2(&rconf, &changed);

	return true;
}

static bool rrm_config_set( struct schema_Wifi_RRM_Config *old,
	struct schema_Wifi_RRM_Config *conf ) 
{
	if (rrm_config_changed(old, conf)) {
		rrm_config_update(conf, true);
	}

	if(conf->backup_channel != old->backup_channel) {
		rrm_radio_config_update(conf);
	}

	return true;
}

static bool rrm_config_delete( struct schema_Wifi_RRM_Config *old )
{
	return( rrm_config_update(old, false));
}

void callback_Wifi_RRM_Config(ovsdb_update_monitor_t *self,
				 struct schema_Wifi_RRM_Config *old,
				 struct schema_Wifi_RRM_Config *conf)
{
	switch (self->mon_type)
	{
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		(void) rrm_config_set(old, conf);
		break;

	case OVSDB_UPDATE_DEL:
		(void) rrm_config_delete(old);
		break;

	default:
		LOG(ERR, "Wifi_RRM_Config: unexpected mon_type %d %s", self->mon_type, self->mon_uuid);
		break;
	}
	return;
}


