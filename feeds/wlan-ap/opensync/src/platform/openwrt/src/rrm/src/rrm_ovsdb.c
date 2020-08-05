/* SPDX-License-Identifier: BSD-3-Clause */

#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "schema.h"

#include "rrm.h"

/* Log entries from this file will contain "OVSDB" */
#define MODULE_ID LOG_MODULE_ID_OVSDB

ovsdb_table_t table_RRM_Config;

static ovsdb_update_monitor_t rrm_update_wifi_radio_state;
static ds_tree_t rrm_radio_list =
		DS_TREE_INIT(
				(ds_key_cmp_t*)strcmp,
				rrm_radio_state_t,
				node);

static ovsdb_update_monitor_t rrm_update_wifi_vif_state;
static ds_tree_t rrm_vif_list =
		DS_TREE_INIT(
				(ds_key_cmp_t*)strcmp,
				rrm_vif_state_t,
				node);

static ovsdb_update_monitor_t rrm_update_rrm_config;
static ds_tree_t rrm_config_list =
		DS_TREE_INIT(
				(ds_key_cmp_t*)strcmp,
				rrm_config_t,
				node);

ds_tree_t* rrm_get_radio_list(void)
{
	return &rrm_radio_list;
}

ds_tree_t* rrm_get_vif_list(void)
{
	return &rrm_vif_list;
}

ds_tree_t* rrm_get_rrm_config_list(void)
{
	return &rrm_config_list;
}


static
bool rrm_is_radio_config_changed (
		radio_entry_t              *old_cfg,
		radio_entry_t              *new_cfg)
{
	if (old_cfg->chan != new_cfg->chan)
	{
		LOG(DEBUG,
				"Radio Config: %s chan changed %d != %d",
				radio_get_name_from_cfg(new_cfg),
				old_cfg->chan,
				new_cfg->chan);
		return true;
	}

	if (strcmp(old_cfg->phy_name, new_cfg->phy_name))
	{
		LOG(DEBUG,
				"Radio Config: %s phy_name changed %s != %s",
				radio_get_name_from_cfg(new_cfg),
				old_cfg->phy_name,
				new_cfg->phy_name);
		return true;
	}

	if (strcmp(old_cfg->if_name, new_cfg->if_name))
	{
		LOG(DEBUG,
				"Radio Config: %s if_name changed %s != %s",
				radio_get_name_from_cfg(new_cfg),
				old_cfg->if_name,
				new_cfg->if_name);
		return true;
	}

	return false;
}

static
void rrm_config_update(void)
{
	rrm_config_t               *rrm = NULL;
	rrm_entry_t                 rrm_data;

	ds_tree_foreach(&rrm_config_list, rrm)
	{
		memset(&rrm_data, 0, sizeof(rrm_data));

		if (strcmp(rrm->schema.freq_band, RADIO_TYPE_STR_2G) == 0) {
			rrm_data.freq_band = RADIO_TYPE_2G;
		}
		else if (strcmp(rrm->schema.freq_band, RADIO_TYPE_STR_5G) == 0) {
			rrm_data.freq_band = RADIO_TYPE_5G;
		}
		else if (strcmp(rrm->schema.freq_band, RADIO_TYPE_STR_5GL) == 0) {
			rrm_data.freq_band = RADIO_TYPE_5GL;
		}
		else if (strcmp(rrm->schema.freq_band, RADIO_TYPE_STR_5GU) == 0) {
			rrm_data.freq_band = RADIO_TYPE_5GU;
		}
		else {
			LOG(ERR,
					"RRM Config: Unknown radio frequency band: %s",
					rrm->schema.freq_band);
			rrm_data.freq_band = RADIO_TYPE_NONE;
			return;
		}

		rrm_data.backup_channel = rrm->schema.backup_channel;
		rrm_data.cell_size = rrm->schema.cell_size;
		rrm_data.probe_resp_threshold = rrm->schema.probe_resp_threshold;
		rrm_data.client_disconnect_threshold = rrm->schema.client_disconnect_threshold;
		rrm_data.snr_percentage_drop = rrm->schema.snr_percentage_drop;
		rrm_data.min_load = rrm->schema.min_load;
		rrm_data.basic_rate = rrm->schema.basic_rate;

		/* Update cache config */
		rrm->rrm_data = rrm_data;

		set_rrm_parameters(&rrm_data);
	}
}


static
void rrm_radio_cfg_update(void)
{
	rrm_radio_state_t               *radio = NULL;
	radio_entry_t                   radio_cfg;

	ds_tree_foreach(&rrm_radio_list, radio)
	{
		memset(&radio_cfg, 0, sizeof(radio_cfg));

		if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_2G) == 0) {
			radio_cfg.type = RADIO_TYPE_2G;
		}
		else if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_5G) == 0) {
			radio_cfg.type = RADIO_TYPE_5G;
		}
		else if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_5GL) == 0) {
			radio_cfg.type = RADIO_TYPE_5GL;
		}
		else if (strcmp(radio->schema.freq_band, RADIO_TYPE_STR_5GU) == 0) {
			radio_cfg.type = RADIO_TYPE_5GU;
		}
		else {
			LOG(ERR,
					"Radio Config: Unknown radio frequency band: %s",
					radio->schema.freq_band);
			radio_cfg.type = RADIO_TYPE_NONE;
			return;
		}

		/* Admin mode */
		radio_cfg.admin_status =
				radio->schema.enabled ? RADIO_STATUS_ENABLED : RADIO_STATUS_DISABLED;

		/* Assign operating channel */
		radio_cfg.chan = radio->schema.channel;

		/* Radio physical name */
		STRSCPY(radio_cfg.phy_name, radio->schema.if_name);

		/* Country code */
		STRSCPY(radio_cfg.cntry_code, radio->schema.country);

		/* Channel width and HT mode */
		if (strcmp(radio->schema.ht_mode, "HT20") == 0) {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_20MHZ;
		}
		else if (strcmp(radio->schema.ht_mode, "HT2040") == 0) {
			LOG(DEBUG,
					"Radio Config: No direct mapping for HT2040");
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_NONE;
		}
		else if (strcmp(radio->schema.ht_mode, "HT40") == 0) {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_40MHZ;
		}
		else if (strcmp(radio->schema.ht_mode, "HT40+") == 0) {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_40MHZ_ABOVE;
		}
		else if (strcmp(radio->schema.ht_mode, "HT40-") == 0) {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_40MHZ_BELOW;
		}
		else if (strcmp(radio->schema.ht_mode, "HT80") == 0) {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_80MHZ;
		}
		else if (strcmp(radio->schema.ht_mode, "HT160") == 0) {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_160MHZ;
		}

		else if (strcmp(radio->schema.ht_mode, "HT80+80") == 0) {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_80_PLUS_80MHZ;
		}
		else {
			radio_cfg.chanwidth = RADIO_CHAN_WIDTH_NONE;
			LOG(DEBUG,
					"Radio Config: Unknown radio HT mode: %s",
					radio->schema.ht_mode);
		}

		if (strcmp(radio->schema.hw_mode, "11a") == 0) {
			radio_cfg.protocol = RADIO_802_11_A;
		}
		else if (strcmp(radio->schema.hw_mode, "11b") == 0) {
			radio_cfg.protocol = RADIO_802_11_BG;
		}
		else if (strcmp(radio->schema.hw_mode, "11g") == 0) {
			radio_cfg.protocol = RADIO_802_11_BG;
		}
		else if (strcmp(radio->schema.hw_mode, "11n") == 0) {
			radio_cfg.protocol = RADIO_802_11_NG;
		}
		else if (strcmp(radio->schema.hw_mode, "11ab") == 0) {
			radio_cfg.protocol = RADIO_802_11_A;
		}
		else if (strcmp(radio->schema.hw_mode, "11ac") == 0) {
			radio_cfg.protocol = RADIO_802_11_AC;
		}
		else if (strcmp(radio->schema.hw_mode, "11ax") == 0) {
			radio_cfg.protocol = RADIO_802_11_AX;
		}
		else {
			LOG(DEBUG,
					"Radio Config: Unkown protocol: %s",
					radio->schema.hw_mode);
			radio_cfg.protocol = RADIO_802_11_AUTO;
		}

		if (radio->schema.vif_states_len > 0) {
			int ii;
			rrm_vif_state_t *vif = NULL;

			/* Lookup the first interface */
			for (ii = 0; ii < radio->schema.vif_states_len; ii++)
			{
				vif = ds_tree_find(
						&rrm_vif_list,
						radio->schema.vif_states[ii].uuid);

				if (vif == NULL) {
					continue;
				}

				if (strcmp(vif->schema.mode, "sta") == 0) {
					continue;
				}

				if (vif->schema.enabled == 0) {
					continue;
				}

				STRSCPY(radio_cfg.if_name, vif->schema.if_name);
				break;
			}
		}
		else {
			LOG(DEBUG,
					"Radio Config: No interfaces associated with %s radio.",
					radio_get_name_from_cfg(&radio_cfg));
		}

		bool is_changed =
				rrm_is_radio_config_changed(
						&radio->config,
						&radio_cfg);

		/* Update cache config */
		radio->config = radio_cfg;

		if(    is_changed
				&& radio->config.type
				&& (radio->config.chan != 0)
				&& (radio->config.if_name[0] != '\0')
				&& (radio->config.phy_name[0] != '\0')) {

			rrm_config_update();

		}

	}
}

static
void rrm_update_wifi_radio_state_cb(ovsdb_update_monitor_t *self)
{
	pjs_errmsg_t                    perr;
	rrm_radio_state_t               *radio = NULL;

	switch (self->mon_type)
	{
	case OVSDB_UPDATE_NEW:
		/*
		 * New row update notification -- create new row, parse it and insert it into the table
		 */
		radio = calloc(1, sizeof(rrm_radio_state_t));
		if (NULL == radio) {
			LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: Failed to allocate memory");
			return;
		}

		if (!schema_Wifi_Radio_State_from_json(&radio->schema, self->mon_json_new, false, perr))
		{
			LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: %s", perr);
			free(radio);
			return;
		}

		/* The Radio state table is indexed by UUID */
		ds_tree_insert(&rrm_radio_list, radio, radio->schema._uuid.uuid);
		break;

	case OVSDB_UPDATE_MODIFY:
		/* Find the row by UUID */
		radio = ds_tree_find(&rrm_radio_list, (void *)self->mon_uuid);
		if (!radio) {
			LOG(ERR, "MODIFY: Radio State: Update request for non-existent radio UUID: %s", self->mon_uuid);
			return;
		}

		/* Update the row */
		if (!schema_Wifi_Radio_State_from_json(&radio->schema, self->mon_json_new, true, perr))
		{
			LOG(ERR, "MODIFY: Radio State: Parsing Wifi_Radio_State: %s", perr);
			return;
		}
		break;

	case OVSDB_UPDATE_DEL:
		radio = ds_tree_find(&rrm_radio_list, (void *)self->mon_uuid);
		if (!radio)
		{
			LOG(ERR, "DELETE: Radio State: Delete request for non-existent radio UUID: %s", self->mon_uuid);
			return;
		}

		ds_tree_remove(&rrm_radio_list, radio);
		free(radio);
		return;

	default:
		LOG(ERR, "Radio State: Unknown update notification type %d for UUID %s.", self->mon_type, self->mon_uuid);
		return;
	}

	/* Update the global radio configuration */
	rrm_radio_cfg_update();
}

static
void rrm_update_rrm_config_cb(ovsdb_update_monitor_t *self)
{

	pjs_errmsg_t                    perr;
	rrm_config_t                    *rrm_config;
	bool                            ret;

	switch (self->mon_type)
	{
	case OVSDB_UPDATE_NEW:
		rrm_config = calloc(1, sizeof(rrm_config_t));
		if (NULL == rrm_config) {
			LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: Failed to allocate memory");
			return;
		}

		ret = schema_Wifi_RRM_Config_from_json(&rrm_config->schema, self->mon_json_new, false, perr);
		if (!ret) {
			free(rrm_config);
			LOG(ERR, "Parsing RRM_Config NEW request: %s", perr);
			return;
		}
		ds_tree_insert(&rrm_config_list, rrm_config, rrm_config->schema._uuid.uuid);
		break;

	case OVSDB_UPDATE_MODIFY:
		rrm_config = ds_tree_find(&rrm_config_list, (char*)self->mon_uuid);
		if (!rrm_config)
		{
			LOG(ERR, "Unexpected MODIFY %s", self->mon_uuid);
			return;
		}
		ret = schema_Wifi_RRM_Config_from_json(&rrm_config->schema, self->mon_json_new, true, perr);
		if (!ret)
		{
			LOG(ERR, "Parsing RRM_Config MODIFY request.");
			return;
		}
		break;

	case OVSDB_UPDATE_DEL:
		rrm_config = ds_tree_find(&rrm_config_list, (char*)self->mon_uuid);
		if (!rrm_config)
		{
			LOG(ERR, "Unexpected DELETE %s", self->mon_uuid);
			return;
		}
		/* Reset configuration */
		rrm_config->schema.backup_channel = 0;
		rrm_config->schema.cell_size = 0;
		rrm_config->schema.min_load = 0;
		rrm_config->schema.basic_rate = 0;
		rrm_config->schema.snr_percentage_drop = 0;
		rrm_config->schema.client_disconnect_threshold = 0;
		rrm_config->schema.probe_resp_threshold = 0;

		ds_tree_remove(&rrm_config_list, rrm_config);
		free(rrm_config);
		return;

	default:
		LOG(ERR, "Update Monitor for RRM_Config reported an error. %s", self->mon_uuid);
		return;
	}

	rrm_config_update();

	return;
}
static
void rrm_update_wifi_vif_state_cb(ovsdb_update_monitor_t *self)
{
	pjs_errmsg_t                    perr;
	rrm_vif_state_t                 *vif = NULL;

	switch (self->mon_type)
	{
	case OVSDB_UPDATE_NEW:
		/*
		 * New row update notification -- create new row, parse it and insert it into the table
		 */
		vif = calloc(1, sizeof(rrm_vif_state_t));
		if (NULL == vif) {
			LOG(ERR, "NEW: Radio State: Parsing Wifi_Radio_State: Failed to allocate memory");
			return;
		}

		if (!schema_Wifi_VIF_State_from_json(&vif->schema, self->mon_json_new, false, perr))
		{
			LOG(ERR, "NEW: VIF Config: Parsing Wifi_Radio_Config: %s", perr);
			return;
		}

		/* The Radio config table is indexed by UUID */
		ds_tree_insert(&rrm_vif_list, vif, vif->schema._uuid.uuid);
		break;

	case OVSDB_UPDATE_MODIFY:
		/* Find the row by UUID */
		vif = ds_tree_find(&rrm_vif_list, (void *)self->mon_uuid);
		if (!vif)
		{
			LOG(ERR, "MODIFY: VIF Config: Update request for non-existent vif UUID: %s", self->mon_uuid);
			return;
		}

		/* Update the row */
		if (!schema_Wifi_VIF_State_from_json(&vif->schema, self->mon_json_new, true, perr))
		{
			LOG(ERR, "MODIFY: VIF Config: Parsing Wifi_Radio_Config: %s", perr);
			return;
		}
		break;

	case OVSDB_UPDATE_DEL:
		vif = ds_tree_find(&rrm_vif_list, (void *)self->mon_uuid);
		if (!vif)
		{
			LOG(ERR, "DELETE: VIF Config: Delete request for non-existent rrm UUID: %s", self->mon_uuid);
			return;
		}

		ds_tree_remove(&rrm_vif_list, vif);

		free(vif);

		return;

	default:
		LOG(ERR, "VIF Config: Unknown update notification type %d for UUID %s.", self->mon_type, self->mon_uuid);
		return;
	}

	/* Update the global radio configuration */
	rrm_radio_cfg_update();
}
int rrm_setup_monitor(void)
{
	/* Monitor Wifi_Radio_State */
	if (!ovsdb_update_monitor(
			&rrm_update_wifi_radio_state,
			rrm_update_wifi_radio_state_cb,
			SCHEMA_TABLE(Wifi_Radio_State),
			OMT_ALL))
	{
		LOGE("Error registering watcher for %s.",
				SCHEMA_TABLE(Wifi_Radio_State));
		return -1;
	}

	/* Monitor Wifi_VIF_State  */
	if (!ovsdb_update_monitor(
			&rrm_update_wifi_vif_state,
			rrm_update_wifi_vif_state_cb,
			SCHEMA_TABLE(Wifi_VIF_State),
			OMT_ALL))
	{
		LOGE("Error registering watcher for %s.",
				SCHEMA_TABLE(Wifi_VIF_State));
		return -1;
	}

	/* Monitor Wifi_RRM_Config */
	if (!ovsdb_update_monitor(
			&rrm_update_rrm_config,
			rrm_update_rrm_config_cb,
			SCHEMA_TABLE(Wifi_RRM_Config),
			OMT_ALL))
	{
		LOGE("Error registering watcher for %s.",
				SCHEMA_TABLE(Wifi_RRM_Config));
		return -1;
	}

	return 0;
}

