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
#include "string.h"

#define NUM_MAX_CLIENTS 10

/*****************************************************************************
 *  INTERFACE definitions
 *****************************************************************************/

bool target_is_radio_interface_ready(char *phy_name)
{
    return true;
}

bool target_is_interface_ready(char *if_name)
{
    return true;
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
    if (record == NULL) return NULL;

    memset(record, 0, sizeof(target_client_record_t));

    return record;
}

void target_client_record_free(target_client_record_t *record)
{
    if (record != NULL)
    {
        free(record);
    }
}

bool target_stats_clients_get(
        radio_entry_t *radio_cfg,
        radio_essid_t *essid,
        target_stats_clients_cb_t *client_cb,
        ds_dlist_t *client_list,
        void *client_ctx)
{
	char                     buf[IWINFO_BUFSIZE];
	int                      len;
	struct iwinfo_assoclist_entry  *assoc_client    = NULL;
	target_client_record_t  *client_entry    = NULL;
	char stats_if_name[15];
	radio_type_t radio_type;

	memset(stats_if_name, 0, sizeof(stats_if_name));

	if(strcmp(radio_cfg->if_name, "home-ap-24") == 0)
	{
		strncpy(stats_if_name, "wlan1", sizeof(stats_if_name));
		radio_type = RADIO_TYPE_2G;
	}
	else if(strcmp(radio_cfg->if_name, "home-ap-l50") == 0)
	{
		strncpy(stats_if_name, "wlan2", sizeof(stats_if_name));
		radio_type = RADIO_TYPE_5GL;
	}
	else if(strcmp(radio_cfg->if_name, "home-ap-u50") == 0)
	{
		strncpy(stats_if_name, "wlan0", sizeof(stats_if_name));
		radio_type = RADIO_TYPE_5GU;
	}
	else
	{
		return true;
	}

	// find iwinfo type
	const char *if_type = iwinfo_type(stats_if_name);
	const struct iwinfo_ops *winfo_ops = iwinfo_backend_by_name(if_type);

	if(0 != winfo_ops->assoclist(stats_if_name, buf, &len))
	{
		return false;
	}

	assoc_client = (struct iwinfo_assoclist_entry *)buf;

	LOGN("%s:%d radiocfg.ifname.%s len.%d", __func__, __LINE__, radio_cfg->if_name, len);

	//add a for loop to traverse through the lists
	for(int i = 0; i < len; i += sizeof(struct iwinfo_assoclist_entry))
	{
		//do all the copy stuff
		client_entry = target_client_record_alloc();
		client_entry->info.type = radio_type;
		memcpy(client_entry->info.mac, assoc_client->mac, sizeof(assoc_client->mac));
		memcpy(client_entry->info.ifname, radio_cfg->if_name, sizeof(radio_cfg->if_name));
		client_entry->stats.bytes_tx = assoc_client->tx_bytes;
		client_entry->stats.bytes_rx = assoc_client->rx_bytes;
		client_entry->stats.rssi = assoc_client->signal;
		client_entry->stats.rate_tx = assoc_client->tx_rate.rate;
		client_entry->stats.rate_rx = assoc_client->tx_rate.rate;

		ds_dlist_insert_tail(client_list, client_entry);

		(*client_cb)(client_list, client_ctx, true);

		LOGN("%s:%d mac.%02x:%02x:%02x:%02x:%02x:%02x", __func__, __LINE__,
				assoc_client->mac[0],
				assoc_client->mac[1],
				assoc_client->mac[2],
				assoc_client->mac[3],
				assoc_client->mac[4],
				assoc_client->mac[5]);
		//move to next client
		assoc_client++;
	}

	return true;
}

bool target_stats_clients_convert(
        radio_entry_t *radio_cfg,
        target_client_record_t *data_new,
        target_client_record_t *data_old,
        dpp_client_record_t *client_record)
{
    memcpy(client_record->info.mac, data_new->info.mac, sizeof(data_new->info.mac));

    client_record->stats.bytes_tx   = data_new->stats.bytes_tx;
    client_record->stats.bytes_rx   = data_new->stats.bytes_rx;
    client_record->stats.rssi       = data_new->stats.rssi;
    client_record->stats.rate_tx    = data_new->stats.rate_tx;
    client_record->stats.rate_rx    = data_new->stats.rate_rx;

    return true;
}


/******************************************************************************
 *  SURVEY definitions
 *****************************************************************************/

target_survey_record_t* target_survey_record_alloc()
{
    target_survey_record_t *record = NULL;

    record = malloc(sizeof(target_survey_record_t));
    if (record == NULL) return NULL;

    memset(record, 0, sizeof(target_survey_record_t));

    return record;
}

void target_survey_record_free(target_survey_record_t *result)
{
    if (result != NULL)
    {
        free(result);
    }
}

bool target_stats_survey_get(
        radio_entry_t *radio_cfg,
        uint32_t *chan_list,
        uint32_t chan_num,
        radio_scan_type_t scan_type,
        target_stats_survey_cb_t *survey_cb,
        ds_dlist_t *survey_list,
        void *survey_ctx)
{
    target_survey_record_t  *survey_record;

    survey_record = target_survey_record_alloc();
    survey_record->info.chan = 1;
    ds_dlist_insert_tail(survey_list, survey_record);

    (*survey_cb)(survey_list, survey_ctx, true);

    return true;
}

bool target_stats_survey_convert(
        radio_entry_t *radio_cfg,
        radio_scan_type_t scan_type,
        target_survey_record_t *data_new,
        target_survey_record_t *data_old,
        dpp_survey_record_t *survey_record)
{
    survey_record->chan_tx       = 30;
    survey_record->chan_self     = 30;
    survey_record->chan_rx       = 40;
    survey_record->chan_busy_ext = 50;
    survey_record->duration_ms   = 60;
    survey_record->chan_busy     = 70;

    return true;
}


/******************************************************************************
 *  NEIGHBORS definitions
 *****************************************************************************/
uint32_t channel_to_freq(uint32_t chan)
{
    uint32_t channel[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 144, 149, 153, 157, 161, 165};
    uint32_t freq[] = {2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320, 5500, 5520, 5540, 5560, 5580, 5660, 5680, 5700, 5720, 5745, 5765, 5785, 5805, 5825 };

    for(int i = 0; i<33; i++)
    {
      if(channel[i] == chan)
      return freq[i];
    }
    
    return 0;
}

bool target_stats_scan_start(
        radio_entry_t *radio_cfg,
        uint32_t *chan_list,
        uint32_t chan_num,
        radio_scan_type_t scan_type,
        int32_t dwell_time,
        target_scan_cb_t *scan_cb,
        void *scan_ctx)
{
    char command[64];
    uint32_t frequency;
    memset(command, 0, strlen(command));
    //sprintf(command,"iw %s scan duration %d", radio_cfg->if_name, dwell_time);
    //sprintf(command,"iw wlan0 scan duration 30");

    if(strcmp(radio_cfg->if_name, "home-ap-24") == 0)
    {
      frequency = channel_to_freq(chan_list[0]);
      sprintf(command,"iw wlan1 scan duration 30 freq %d", frequency);
    }
    else if(strcmp(radio_cfg->if_name, "home-ap-l50") == 0)
    {
      frequency = channel_to_freq(chan_list[0]);
      sprintf(command,"iw wlan2 scan duration 30 freq %d", frequency);
      LOGN("Freq: %d %d", frequency, chan_list[0]);
    }
    else if(strcmp(radio_cfg->if_name, "home-ap-u50") == 0)
    {
      frequency = channel_to_freq(chan_list[0]);
      sprintf(command,"iw wlan0 scan duration 30 freq %d", frequency);
    }

    LOGN("scanning command : %s", command);
    LOGN("channel num: %d", chan_num);
    LOGN("scan_type : %d", scan_type);
    if(system(command) == -1)
    {

    (*scan_cb)(scan_ctx, false);

    return false;

    }

    (*scan_cb)(scan_ctx, true);
   
    return true;
}

bool target_stats_scan_stop(
        radio_entry_t *radio_cfg,
        radio_scan_type_t scan_type)
{
    char command[64];

    memset(command, 0, strlen(command));
    sprintf(command,"iw %s scan abort", radio_cfg->if_name);
    
    LOGN("stop scan command : %s", command);
    if(system(command) == -1)
    return false;

    return true;
}

bool target_stats_scan_get(
        radio_entry_t *radio_cfg,
        uint32_t *chan_list,
        uint32_t chan_num,
        radio_scan_type_t scan_type,
        dpp_neighbor_report_data_t *scan_results)
{

    char command[128];
    FILE *fp=NULL;
    long int fsize;
    char *buffer=NULL;
    char *tmp=NULL;
    char sig[4];
    char lastseen[12];
    char ssid[32];
    char channwidth[4];
    char TSF[20];

    memset(command, 0, strlen(command));
 //   sprintf(command,"iw %s scan dump  > /tmp/scan%s.dump", radio_cfg->if_name, radio_cfg->if_name);
 //   LOGN("dump scan command : %s", command);
    if(strcmp(radio_cfg->if_name, "home-ap-24") == 0)
    {
      sprintf(command,"iw wlan1 scan dump  > /tmp/scanwlan1.dump");
      LOGN("dump scan command : %s", command);
      if(system(command) != -1)
      fp = fopen("/tmp/scanwlan1.dump","r");
    }
    else if(strcmp(radio_cfg->if_name, "home-ap-l50") == 0)
    {
      sprintf(command,"iw wlan2 scan dump  > /tmp/scanwlan2.dump");
      LOGN("dump scan command : %s", command);
      if(system(command) != -1)
      fp = fopen("/tmp/scanwlan2.dump","r");
    }
    else if(strcmp(radio_cfg->if_name, "home-ap-u50") == 0)
    {
      sprintf(command,"iw wlan0 scan dump  > /tmp/scanwlan0.dump");
      LOGN("dump scan command : %s", command);
      if(system(command) != -1)
      fp = fopen("/tmp/scanwlan0.dump","r");
    }

    if(fp==NULL)
    {
      LOG(ERR,"Failed to open scan dump files");
      return false;
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer = malloc(fsize+1);
    fread(buffer, 1, fsize, fp);
    buffer[fsize] = 0;
    fclose(fp);
    tmp = buffer;

    while(strstr(tmp,"BSS") != NULL)
    {
     dpp_neighbor_record_list_t *neighbor;

     neighbor = dpp_neighbor_record_alloc();
     if (neighbor == NULL) return false;

     neighbor->entry.type        = radio_cfg->type;

     tmp = strstr(tmp,"BSS");
     tmp = tmp + 4;
     strncpy(neighbor->entry.bssid, tmp, 17);

     tmp = strstr(tmp,"TSF");
     tmp = tmp + 4;
     sscanf(tmp, "%s", TSF);
     neighbor->entry.tsf = atoi(TSF);
     
     tmp = strstr(tmp,"signal");
     tmp = tmp + 8;
     strncpy(sig, tmp, 3);
     neighbor->entry.sig = atoi(sig);

     tmp = strstr(tmp,"last seen");
     tmp = tmp + 11;
     sscanf(tmp, "%s", lastseen);
     neighbor->entry.lastseen    = atoi(lastseen);

     tmp = strstr(tmp,"SSID");
     tmp = tmp + 6;
     sscanf(tmp, "%s", ssid);
     strncpy(neighbor->entry.ssid, ssid, 32);


     tmp = strstr(tmp,"STA channel width");
     tmp = tmp + 19;
     sscanf(tmp, "%s", channwidth);
     neighbor->entry.chanwidth   = atoi(channwidth);
     neighbor->entry.chan        = chan_list[0];
     
     ds_dlist_insert_tail(&scan_results->list, neighbor);
    }

    free(buffer);

    return true;
}


/******************************************************************************
 *  DEVICE definitions
 *****************************************************************************/

bool target_stats_device_temp_get(
        radio_entry_t *radio_cfg,
        dpp_device_temp_t *temp_entry)
{
    int32_t temperature;
    FILE *fp = NULL;

    if(strcmp(radio_cfg->if_name, "home-ap-24") == 0)
    {
      fp = fopen("/sys/class/hwmon/hwmon1/temp1_input","r");
    }
    else if(strcmp(radio_cfg->if_name, "home-ap-l50") == 0)
    {
     fp = fopen("/sys/class/hwmon/hwmon2/temp1_input","r");
    }
    else if(strcmp(radio_cfg->if_name, "home-ap-u50") == 0)
    {
      fp = fopen("/sys/class/hwmon/hwmon0/temp1_input","r");
    }

    if(fp==NULL)
    {
      LOG(ERR,"Failed to open temp input files");
      return false;
    }

    if(fscanf(fp,"%d",&temperature) == EOF)
    {
      LOG(ERR,"Temperature reading failed");
      fclose(fp);
      return false;
    }

    LOGN("temperature : %d", temperature);

    fclose(fp);
    temp_entry->type  = radio_cfg->type;
    temp_entry->value = (temperature/1000);

    return true;
}

bool target_stats_device_txchainmask_get(
        radio_entry_t              *radio_cfg,
        dpp_device_txchainmask_t   *txchainmask_entry)
{
    txchainmask_entry->type  = radio_cfg->type;
    txchainmask_entry->value = 2;

    return true;
}

bool target_stats_device_fanrpm_get(uint32_t *fan_rpm)
{
    return true;
}


/******************************************************************************
 *  CAPACITY definitions
 *****************************************************************************/

bool target_stats_capacity_enable(radio_entry_t *radio_cfg, bool enabled)
{
    return true;
}

bool target_stats_capacity_get(
        radio_entry_t *radio_cfg,
        target_capacity_data_t *capacity_new)
{
    return true;
}

bool target_stats_capacity_convert(
        target_capacity_data_t *capacity_new,
        target_capacity_data_t *capacity_old,
        dpp_capacity_record_t *capacity_entry)
{
    return true;
}
