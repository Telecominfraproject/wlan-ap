/*
Copyright (c) 2019, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "target.h"
#include <stdio.h>
#include <stdbool.h>
#include "iwinfo.h"

#include "nl80211.h"
#include "phy.h"
#include "utils.h"
#include <sys/types.h>
#include <curl/curl.h>
#include <time.h>
#include "dhcpdiscovery.h"
#include "vif.h"

#define U_P_LEN 30
#define NUM_MAX_CLIENTS 10
#define MAX_IP_ADDR_LEN 16
#define MAX_PWD_LEN 65
#define MAX_ENCRYPTION_LEN 8
#define MAX_MODE_LEN 6

/*****************************************************************************
 *  INTERFACE definitions
 *****************************************************************************/

bool target_is_radio_interface_ready(char *phy_name)
{
	return true;
}

bool target_is_interface_ready(char *if_name)
{
	return iface_is_up(if_name);
}

/******************************************************************************
 *  STATS definitions
 *****************************************************************************/

bool target_radio_tx_stats_enable(radio_entry_t *radio_cfg, bool enable)
{
	return true;
}

bool target_radio_fast_scan_enable(radio_entry_t *radio_cfg, ifname_t if_name)
{
	return true;
}


/******************************************************************************
 *  CLIENT definitions
 *****************************************************************************/

target_client_record_t* target_client_record_alloc()
{
	target_client_record_t *record = NULL;

	record = malloc(sizeof(target_client_record_t));
	if (record == NULL)
		return NULL;

	memset(record, 0, sizeof(target_client_record_t));

	return record;
}

void target_client_record_free(target_client_record_t *record)
{
	if (record != NULL)
		free(record);
}

bool target_stats_clients_get(radio_entry_t *radio_cfg, radio_essid_t *essid,
			      target_stats_clients_cb_t *client_cb,
			      ds_dlist_t *client_list, void *client_ctx)
{
	struct nl_call_param nl_call_param = {
		.ifname = radio_cfg->if_name,
		.type = radio_cfg->type,
		.list = client_list,
	};
	ds_dlist_t ssid_list = DS_DLIST_INIT(ssid_list_t, node);
        ssid_list_t *ssid = NULL;
        struct nl_call_param ssid_call_param = {
                .ifname = radio_cfg->if_name,
                .type = radio_cfg->type,
                .list = &ssid_list,
        };

	bool ret = true;
        target_client_record_t *cl;

        if (nl80211_get_ssid(&ssid_call_param) < 0)
                ret = false;

        while (!ds_dlist_is_empty(&ssid_list)) {
                ssid = ds_dlist_head(&ssid_list);

                ds_dlist_t working_list = DS_DLIST_INIT(ds_dlist_t, od_cof);

                nl_call_param.ifname  = ssid->ifname;
                nl_call_param.list = &working_list;

                if (nl80211_get_assoclist(&nl_call_param) < 0) {
                       ds_dlist_remove(&ssid_list,ssid);
                       continue;
                }
                LOGD("%s: assoc returned %d, list %d", ssid->ifname, ret, ds_dlist_is_empty(&working_list));

                while (!ds_dlist_is_empty(&working_list)) {
                        cl = ds_dlist_head(&working_list);
                        strncpy(cl->info.essid, ssid->ssid, sizeof(cl->info.essid));
                        ds_dlist_remove(&working_list, cl);
                        ds_dlist_insert_tail(client_list, cl);
                }
                ds_dlist_remove(&ssid_list,ssid);
                free(ssid);
        }

	(*client_cb)(client_list, client_ctx, ret);
        return ret;
}

bool target_stats_clients_convert(radio_entry_t *radio_cfg, target_client_record_t *data_new,
				  target_client_record_t *data_old, dpp_client_record_t *client_record)
{
	memcpy(client_record->info.mac, data_new->info.mac, sizeof(data_new->info.mac));
	memcpy(client_record->info.essid, data_new->info.essid, sizeof(radio_cfg->if_name));

	client_record->stats.rssi       = data_new->stats.rssi;
	client_record->stats.rate_tx    = data_new->stats.rate_tx;
	client_record->stats.rate_rx    = data_new->stats.rate_rx;
	client_record->stats.bytes_tx   = data_new->stats.bytes_tx   - data_old->stats.bytes_tx;
	client_record->stats.bytes_rx   = data_new->stats.bytes_rx   - data_old->stats.bytes_rx;
	client_record->stats.frames_tx  = data_new->stats.frames_tx  - data_old->stats.frames_tx;
	client_record->stats.frames_rx  = data_new->stats.frames_rx  - data_old->stats.frames_rx;
	client_record->stats.retries_tx = data_new->stats.retries_tx - data_old->stats.retries_tx;
	client_record->stats.errors_tx  = data_new->stats.errors_tx  - data_old->stats.errors_tx;
	client_record->stats.errors_rx  = data_new->stats.errors_rx  - data_old->stats.errors_rx;

	return true;
}


/******************************************************************************
 *  SURVEY definitions
 *****************************************************************************/

target_survey_record_t* target_survey_record_alloc()
{
	target_survey_record_t *record = NULL;

	record = malloc(sizeof(target_survey_record_t));
	if (record == NULL)
		return NULL;

	memset(record, 0, sizeof(target_survey_record_t));

	return record;
}

void target_survey_record_free(target_survey_record_t *result)
{
	if (result != NULL)
		free(result);
}

bool target_stats_survey_get(radio_entry_t *radio_cfg, uint32_t *chan_list,
			     uint32_t chan_num, radio_scan_type_t scan_type,
			     target_stats_survey_cb_t *survey_cb,
			     ds_dlist_t *survey_list, void *survey_ctx)
{
	ds_dlist_t raw_survey_list = DS_DLIST_INIT(target_survey_record_t, node);
	target_survey_record_t *survey;
	struct nl_call_param nl_call_param = {
		.ifname = radio_cfg->if_name,
		.type = radio_cfg->type,
		.list = &raw_survey_list,
	};
	bool ret = true;

	if (nl80211_get_survey(&nl_call_param) < 0)
		ret = false;
	LOGT("%s: survey returned %d, list %d", radio_cfg->if_name, ret, ds_dlist_is_empty(&raw_survey_list));
	while (!ds_dlist_is_empty(&raw_survey_list)) {
		survey = ds_dlist_head(&raw_survey_list);
		ds_dlist_remove(&raw_survey_list, survey);
		LOGI("Survey entry dur %d busy %d tx %d rx %d chan %d active %d type %d",
			survey->duration_ms, survey->chan_busy, survey->chan_tx, survey->chan_rx, survey->info.chan, survey->chan_active, scan_type);
		if ((scan_type == RADIO_SCAN_TYPE_ONCHAN) && (survey->info.chan == chan_list[0])) {
                	ds_dlist_insert_tail(survey_list, survey);
		} else if ((scan_type != RADIO_SCAN_TYPE_ONCHAN) && (survey->duration_ms != 0)) {
			ds_dlist_insert_tail(survey_list, survey);
		} else {
			target_survey_record_free(survey);
			survey = NULL;
		}
	}

	(*survey_cb)(survey_list, survey_ctx, ret);

	return ret;
}

#define PERCENT(v1, v2) (v2 > 0 ? (v1*100/v2) : 0)

bool target_stats_survey_convert(radio_entry_t *radio_cfg, radio_scan_type_t scan_type,
				 target_survey_record_t *data_new, target_survey_record_t *data_old,
				 dpp_survey_record_t *survey_record)
{
	LOGI("Survey convert scan_type %d chan %d", scan_type, data_new->info.chan);
	survey_record->info.chan     = data_new->info.chan;
	survey_record->chan_tx       = PERCENT(data_new->chan_tx, data_new->duration_ms);
	survey_record->chan_self     = PERCENT(data_new->chan_self, data_new->duration_ms);
	survey_record->chan_rx       = PERCENT(data_new->chan_rx, data_new->duration_ms);
	survey_record->chan_busy_ext = PERCENT(data_new->chan_busy_ext, data_new->duration_ms);
	survey_record->chan_busy     = PERCENT(data_new->chan_busy, data_new->duration_ms);
	survey_record->chan_noise    = data_new->chan_noise;
	survey_record->duration_ms   = data_new->duration_ms;

	return true;
}


/******************************************************************************
 *  NEIGHBORS definitions
 *****************************************************************************/

bool target_stats_scan_start(radio_entry_t *radio_cfg, uint32_t *chan_list, uint32_t chan_num,
			     radio_scan_type_t scan_type, int32_t dwell_time,
			     target_scan_cb_t *scan_cb, void *scan_ctx)
{
	struct nl_call_param nl_call_param = {
		.ifname = radio_cfg->if_name,
	};
	bool ret = true;

	if (nl80211_scan_trigger(&nl_call_param, chan_list, chan_num, dwell_time, scan_type, scan_cb, scan_ctx) < 0)
		ret = false;
	LOGT("%s: scan trigger returned %d", radio_cfg->if_name, ret);

	if (ret == false) {
		LOG(ERR, "%s: failed to trigger scan, aborting", radio_cfg->if_name);
		(*scan_cb)(scan_ctx, ret);
	}
	return ret;
}

bool target_stats_scan_stop(radio_entry_t *radio_cfg, radio_scan_type_t scan_type)
{
	struct nl_call_param nl_call_param = {
		.ifname = radio_cfg->if_name,
	};
	bool ret = true;

	if (nl80211_scan_abort(&nl_call_param) < 0)
		ret = false;
	LOGT("%s: scan abort returned %d", radio_cfg->if_name, ret);

	return true;
}

bool target_stats_scan_get(radio_entry_t *radio_cfg, uint32_t *chan_list, uint32_t chan_num,
			   radio_scan_type_t scan_type, dpp_neighbor_report_data_t *scan_results)
{
	struct nl_call_param nl_call_param = {
		.ifname = radio_cfg->if_name,
		.type = radio_cfg->type,
		.list = &scan_results->list,
	};
	bool ret = true;

	if (nl80211_scan_dump(&nl_call_param) < 0)
		ret = false;
	LOGT("%s: scan dump returned %d, list %d", radio_cfg->if_name, ret, ds_dlist_is_empty(&scan_results->list));

	return true;
}


/******************************************************************************
 *  DEVICE definitions
 *****************************************************************************/

bool target_stats_device_temp_get(radio_entry_t *radio_cfg, dpp_device_temp_t *temp_entry)
{
	char hwmon_path[PATH_MAX];
	int32_t temperature;
	FILE *fp = NULL;

	if (phy_find_hwmon(target_map_ifname(radio_cfg->phy_name), hwmon_path)) {
		LOG(ERR, "%s: hwmon is missing", radio_cfg->phy_name);
		return false;
	}

	fp = fopen(hwmon_path, "r");
	if (!fp) {
		LOG(ERR, "%s: Failed to open temp input files", radio_cfg->phy_name);
		return false;
	}

	if (fscanf(fp,"%d",&temperature) == EOF) {
		LOG(ERR, "%s: Temperature reading failed", radio_cfg->phy_name);
		fclose(fp);
		return false;
	}

	LOGT("%s: temperature : %d", radio_cfg->phy_name, temperature);

	fclose(fp);
	temp_entry->type  = radio_cfg->type;
	temp_entry->value = temperature / 1000;

	return true;
}

bool target_stats_device_txchainmask_get(radio_entry_t *radio_cfg, dpp_device_txchainmask_t *txchainmask_entry)
{
	bool ret = true;
	txchainmask_entry->type  = radio_cfg->type;
	if (nl80211_get_tx_chainmask(target_map_ifname(radio_cfg->phy_name), &txchainmask_entry->value) < 0)
		ret = false;
	LOGT("%s: tx_chainmask %d", radio_cfg->phy_name, txchainmask_entry->value);

	return ret;
}

bool target_stats_device_fanrpm_get(uint32_t *fan_rpm)
{
	*fan_rpm = 0;
	return true;
}

/******************************************************************************
 *  UCC Voice Video definitions
 *****************************************************************************/

bool target_stats_channel_get(char *radio_if, unsigned int *chan)
{
	bool ret = true;
	if ((nl80211_get_oper_channel(radio_if, chan)) < 0)
		ret = false;

	LOGT("%s: channel %d", radio_if, *chan);

	return ret;
}

/******************************************************************************
 *  NETWORK PROBE definitions
 *****************************************************************************/
bool target_stats_network_probe_get(dpp_network_probe_record_t *network_probe_report)
{
	int ret = 0;
	char server[] = "8.8.8.8";
	long long int begin, end;
	/* DNS probe */

	CURL *curl = curl_easy_init();
	begin = get_timestamp();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "http://www.google.com");
		curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "8.8.8.8");
		ret = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	}

	end = get_timestamp();

	network_probe_report->dns_probe.latency = (end - begin);
	memcpy(network_probe_report->dns_probe.serverIP , server, strlen(server));

        if(ret == CURLE_OK) {
		LOGT("dns resolved\n");
		network_probe_report->dns_probe.state = 1;
	} else {
		LOGT("dns not resolved\n");
		network_probe_report->dns_probe.state = 0;
	}

	/* DHCP probe */
	char ifname[] = "br-wan";

	begin = get_timestamp();

	if(generateQuery(ifname)) {
		LOGT("dhcp resolved\n");
		network_probe_report->vlan_probe.dhcpState = 1;
	} else {
		LOGT("dhcp not resolved\n");
		network_probe_report->vlan_probe.dhcpState = 0;
	}
	end = get_timestamp();
	network_probe_report->vlan_probe.dhcpLatency = (end - begin);
	memcpy(network_probe_report->vlan_probe.vlanIF , ifname, strlen(ifname));
	LOGT("dhcp latency : %d \n", network_probe_report->vlan_probe.dhcpLatency);

	return true;
}

bool target_stats_radius_probe_get(struct schema_Wifi_VIF_State    schema, dpp_radius_metrics_t *radius_probe_report)
{
	char mode[MAX_MODE_LEN];
	char encryption[MAX_ENCRYPTION_LEN];
	char radiusServerIP[MAX_IP_ADDR_LEN];
	char password[U_P_LEN];
	char port[5];
	int  portNum;
	long long int begin, end;

	LOG(INFO,"RADIUS PROBE\n ");

	if(!vif_get_security(&schema, mode, encryption, radiusServerIP, password, port))
	return false;

	if(strcmp(encryption, OVSDB_SECURITY_ENCRYPTION_WPA_EAP))
	return false;

	strcpy(radius_probe_report->serverIP, radiusServerIP);
	portNum = atoi(port);

	begin = get_timestamp();
	if(radius_probe(radiusServerIP, password, portNum)){
		radius_probe_report->radiusState = 1;
	} else {
	radius_probe_report->radiusState = 0;
	}
	end = get_timestamp();
	radius_probe_report->latency = end - begin;
	return true;
}

/******************************************************************************
 *  CAPACITY definitions
 *****************************************************************************/

bool target_stats_capacity_enable(radio_entry_t *radio_cfg, bool enabled)
{
	return true;
}

bool target_stats_capacity_get(radio_entry_t *radio_cfg,
			       target_capacity_data_t *capacity_new)
{
	return true;
}

bool target_stats_capacity_convert(target_capacity_data_t *capacity_new,
				   target_capacity_data_t *capacity_old,
				   dpp_capacity_record_t *capacity_entry)
{
	return true;
}
static __attribute__((constructor)) void sm_init(void)
{
	stats_nl80211_init();
}
