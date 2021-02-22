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

ovsdb_table_t table_Wifi_RRM_Config;
extern ovsdb_table_t table_Wifi_Radio_Config;
extern ovsdb_table_t table_Wifi_VIF_Config;

void rrm_config_vif(struct blob_buf *b, struct blob_buf *del, const char * freq_band, const char * if_name)
{
	struct schema_Wifi_RRM_Config conf;

	memset(&conf, 0, sizeof(conf));
	if (false == ovsdb_table_select_one(&table_Wifi_RRM_Config,
			SCHEMA_COLUMN(Wifi_RRM_Config, freq_band), freq_band, &conf))
	{
		LOG(INFO, "Wifi_VIF_Config: No RRM for band %s", freq_band );
		blobmsg_add_u32(del, "rssi_ignore_probe_request", -90);
		blobmsg_add_u32(del, "signal_connect", -90);
		blobmsg_add_u32(del, "signal_stay", -90);
		blobmsg_add_u32(del, "bcn_rate", 0);
		blobmsg_add_u32(del, "mcast_rate", 0);
	} else {
		blobmsg_add_u32(b, "rssi_ignore_probe_request", conf.probe_resp_threshold);
		blobmsg_add_u32(b, "signal_connect", conf.client_disconnect_threshold);
		blobmsg_add_u32(b, "signal_stay", conf.client_disconnect_threshold);
		blobmsg_add_u32(b, "bcn_rate", conf.beacon_rate);
		blobmsg_add_u32(b, "mcast_rate", conf.mcast_rate);
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

static bool rrm_config_update( struct schema_Wifi_RRM_Config *conf, bool addNotDelete)
{
	struct schema_Wifi_Radio_Config rconf;
	struct schema_Wifi_VIF_Config vconf;
	struct schema_Wifi_VIF_Config_flags changed;
	json_t *where;
	int i;

	if (false == ovsdb_table_select_one(&table_Wifi_Radio_Config,
			SCHEMA_COLUMN(Wifi_Radio_Config, freq_band), conf->freq_band, &rconf))
	{
		LOG(WARN, "Wifi_RRM_Config: No radio for band %s", conf->freq_band );
		return false;
	}	

	memset(&changed, 0, sizeof(changed));
	for (i = 0; i < rconf.vif_configs_len; i++) {
		if (!(where = ovsdb_where_uuid("_uuid", rconf.vif_configs[i].uuid)))
			continue;

		memset(&vconf, 0, sizeof(vconf));
		if (ovsdb_table_select_one_where(&table_Wifi_VIF_Config, where, &vconf))
		{
			LOG(INFO, "RRM band %s updates vif %s", conf->freq_band, vconf.if_name);
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


