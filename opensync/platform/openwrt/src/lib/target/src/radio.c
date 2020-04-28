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

#include <stdio.h>
#include <stdbool.h>
#include <target.h>
#include "log.h"
#include "evsched.h"
#include "uci_helper.h"

#define DEFAULT_UCI_CONFIG_PATH /etc/config
#define FILE_UCI_WIFI DEFAULT_UCI_CONFIG_PATH/wireless

static bool needReset = true;  /* On start-up, we need to initialize DB from  the UCI */

static struct target_radio_ops g_rops;
static bool g_resync_ongoing = false;

static bool radio_state_get(
        int radioIndex,
        struct schema_Wifi_Radio_State *rstate)
{
    memset(rstate, 0, sizeof(*rstate));
    schema_Wifi_Radio_State_mark_all_present(rstate);
    rstate->_partial_update = true;
    rstate->channel_sync_present = false;
    rstate->channel_mode_present = false;
    rstate->radio_config_present = false;
    rstate->vif_states_present = false;

    if (UCI_OK == wifi_getRadioIfName(radioIndex, rstate->if_name, sizeof(rstate->if_name))) {
        rstate->if_name_exists = true;
        LOGN("UCI radio if_name: %s", rstate->if_name);
    }

    if (UCI_OK == wifi_getRadioHwMode(radioIndex, rstate->hw_mode, sizeof(rstate->hw_mode))) {
        rstate->hw_mode_exists = true;
        LOGN("UCI radio hw_mode: %s", rstate->hw_mode);
    }

    if (UCI_OK == wifi_getRadioChannel(radioIndex, &(rstate->channel))) {
        rstate->channel_exists = true;
        LOGN("UCI radio channel: %d", rstate->channel);
    }

    if (UCI_OK == wifi_getRadioEnable(radioIndex, &(rstate->enabled))) {
        rstate->enabled_exists = true;
        LOGN("UCI radio enabled %d", rstate->enabled);
    }

    /* tx_power gets boundary checked between 1 .. 32 */
    rstate->tx_power = 1;
    rstate->tx_power_exists = true;

    switch (radioIndex) {
	case 0:
           snprintf(rstate->ht_mode, sizeof(rstate->hw_mode),"HT80");
           snprintf(rstate->freq_band, sizeof(rstate->freq_band),"5GU");
           break;
        case 1:
           snprintf(rstate->ht_mode, sizeof(rstate->hw_mode),"HT20");
           snprintf(rstate->freq_band, sizeof(rstate->freq_band),"2.4G");
           break;
        case 2:
           snprintf(rstate->ht_mode, sizeof(rstate->hw_mode),"HT80");
           snprintf(rstate->freq_band, sizeof(rstate->freq_band),"5GL");
           break;
    }
    rstate->ht_mode_exists = true;
    rstate->freq_band_exists = true;

    snprintf(rstate->country, sizeof(rstate->country),"CA");
    rstate->country_exists = true;

    return true;
}

static bool radio_state_update(unsigned int radioIndex)
{
    struct schema_Wifi_Radio_State  rstate;

    if (!radio_state_get(radioIndex, &rstate))
    {
        LOGE("%s: Radio state update failed -- unable to get state for idx %d",
             __func__, radioIndex);
        return false;
    }
    LOGN("Updating state for radio index %d...", radioIndex);
    g_rops.op_rstate(&rstate);

    return true;
}

static bool radio_copy_config_from_state(
        int radioIndex,
        struct schema_Wifi_Radio_State *rstate,
        struct schema_Wifi_Radio_Config *rconf)
{
    memset(rconf, 0, sizeof(*rconf));
    schema_Wifi_Radio_Config_mark_all_present(rconf);
    rconf->_partial_update = true;
    rconf->vif_configs_present = false;

    SCHEMA_SET_STR(rconf->if_name, rstate->if_name);
    LOGT("rconf->ifname = %s", rconf->if_name);
    SCHEMA_SET_STR(rconf->freq_band, rstate->freq_band);
    LOGT("rconf->freq_band = %s", rconf->freq_band);
    SCHEMA_SET_STR(rconf->hw_type, rstate->hw_type);
    LOGT("rconf->hw_type = %s", rconf->hw_type);
    SCHEMA_SET_INT(rconf->enabled, rstate->enabled);
    LOGT("rconf->enabled = %d", rconf->enabled);
    SCHEMA_SET_INT(rconf->channel, rstate->channel);
    LOGT("rconf->channel = %d", rconf->channel);
    SCHEMA_SET_INT(rconf->tx_power, rstate->tx_power);
    LOGT("rconf->tx_power = %d", rconf->tx_power);
    SCHEMA_SET_STR(rconf->country, rstate->country);
    LOGT("rconf->country = %s", rconf->country);
    SCHEMA_SET_STR(rconf->ht_mode, rstate->ht_mode);
    LOGT("rconf->ht_mode = %s", rconf->ht_mode);
    SCHEMA_SET_STR(rconf->hw_mode, rstate->hw_mode);
    LOGT("rconf->hw_mode = %s", rconf->hw_mode);

    return true;
}

static void radio_resync_all_task(void *arg)
{
    int r, rnum;
    int ret;
    int s, snum;
    char ssid_ifname[128];
    
    LOGT("Re-sync started");

#if 1
    ret = wifi_getRadioNumberOfEntries(&rnum);
    if (ret != UCI_OK)
    {
        LOGE("%s: failed to get radio count", __func__);
        goto out;
    }
#else
   rnum = 3;
#endif

    for(r = 0; r < rnum; r++)
    {
        if (!radio_state_update(r))
        {
            LOGW("Cannot update radio state for radio index %d", r);
            continue;
        }
    }

#if 1
    ret = wifi_getSSIDNumberOfEntries(&snum);
    if (ret != UCI_OK)
    {
        LOGE("%s: failed to get SSID count", __func__);
        goto out;
    }

    if (snum == 0)
    {
        LOGE("%s: no SSIDs detected", __func__);
        goto out;
    }
#else
    snum = 5;
#endif

    for (s = 0; s < snum; s++)
    {
        memset(ssid_ifname, 0, sizeof(ssid_ifname));
        ret = wifi_getVIFName(s, ssid_ifname, sizeof(ssid_ifname));
        if (ret != UCI_OK)
        {
            continue;
        }

#if 0
        // Filter SSID's that we don't have mappings for
        if (!target_unmap_ifname_exists(ssid_ifname))
        {
            continue;
        }

        // Fetch existing clients
        if (!clients_hal_fetch_existing(s))
        {
            LOGW("Fetching existing clients for %s failed", ssid_ifname);
        }
#endif

        if (!vif_state_update(s))
        {
            LOGW("Cannot update VIF state for SSID index %d", s);
            continue;
        }
    }
out:
    LOGT("Re-sync completed");
    g_resync_ongoing = false;
}

void radio_trigger_resync()
{
    if (!g_resync_ongoing)
    {
        g_resync_ongoing = true;
        LOGI("Radio re-sync scheduled");
        evsched_task(&radio_resync_all_task, NULL,
                EVSCHED_SEC(2));
    } else
    {
        LOGT("Radio re-sync already ongoing!");
    }
}

static void healthcheck_task(void *arg)
{
    LOGI("Healthcheck re-sync");
    radio_trigger_resync();
    evsched_task_reschedule_ms(EVSCHED_SEC(15));
}

bool target_radio_init(const struct target_radio_ops *ops)
{
    g_rops = *ops;
    evsched_task(&healthcheck_task, NULL, EVSCHED_SEC(5));
    
    return true;
}

bool target_radio_config_init2()
{
    int r;
    int rnum;
    int s;
    int snum;
    int ssid_radio_idx;
    char ssid_ifname[128];
    int ret;

    struct schema_Wifi_VIF_Config   vconfig;
    struct schema_Wifi_VIF_State    vstate;
    struct schema_Wifi_Radio_Config rconfig;
    struct schema_Wifi_Radio_State  rstate;

    target_map_init();

    //Radio mappings
    target_map_insert("wifi0", "radio1");
    target_map_insert("wifi1", "radio2");
    target_map_insert("wifi2", "radio0");

    //VIF mappings
    target_map_insert("home-ap-u50", "default_radio0");
    target_map_insert("home-ap-24", "default_radio1");
    target_map_insert("home-ap-l50", "default_radio2");

#if 1
    ret = wifi_getRadioNumberOfEntries(&rnum);
    if (ret != UCI_OK)
    {
        LOGE("%s: failed to get radio count", __func__);
        return false;
    }
#else
    rnum = 3;
#endif

    for (r = 0; r < rnum; r++)
    {
        radio_state_get(r, &rstate);
        radio_copy_config_from_state(r, &rstate, &rconfig);
        g_rops.op_rconf(&rconfig);
        g_rops.op_rstate(&rstate);

#if 1
        ret = wifi_getSSIDNumberOfEntries(&snum);

        if (ret != UCI_OK)
        {
            LOGE("%s: failed to get SSID count", __func__);
            return false;
        }

        if (snum == 0)
        {
            LOGE("%s: no SSIDs detected", __func__);
            continue;
        }
#else
        snum = 5;
#endif

        for (s = 0; s < snum; s++)
        {
            memset(ssid_ifname, 0, sizeof(ssid_ifname));
            ret = wifi_getVIFName(s, ssid_ifname, sizeof(ssid_ifname));
            if (ret != UCI_OK)
            {
                LOGW("%s: failed to get AP name for index %d. Skipping.\n", __func__, s);
                continue;
            }

#if 0
            // Filter SSID's that we don't have mappings for
            if (!target_unmap_ifname_exists(ssid_ifname))
            {
                continue;
            }
#endif

            ret = wifi_getSSIDRadioIndex(s, &ssid_radio_idx);
            if (ret != UCI_OK)
            {
                LOGW("Cannot get radio index for SSID %d", s);
                continue;
            }

            if (ssid_radio_idx != r)
            {
                continue;
            }

            LOGI("Found SSID index %d: %s", s, ssid_ifname);
            if (!vif_state_get(s, &vstate))
            {
                LOGE("%s: cannot get vif state for SSID index %d", __func__, s);
                continue;
            }
            if (!vif_copy_to_config(s, &vstate, &vconfig))
            {
                LOGE("%s: cannot copy VIF state to config for SSID index %d", __func__, s);
                continue;
            }
            g_rops.op_vconf(&vconfig, rconfig.if_name);
            g_rops.op_vstate(&vstate);
        }
    }
/*
    if (!dfs_event_cb_registered)
    {
        if (wifi_chan_eventRegister(chan_event_cb) != RETURN_OK)
        {
            LOGE("Failed to register chan event callback\n");
        }

        dfs_event_cb_registered = true;
    }
*/
    return true;
}

bool target_radio_config_need_reset()
{
    return needReset;
}

static void radio_ifname_to_idx(char* if_name, int* radioIndex)
{
    // TODO: Quick hack.  This needs to be improved.
    *radioIndex = atoi(strndup(if_name + 5, 5));
}

bool target_radio_config_set2(
     const struct schema_Wifi_Radio_Config *rconf,
     const struct schema_Wifi_Radio_Config_flags *changed)
 {
     int radioIndex;

     radio_ifname_to_idx(target_map_ifname((char*)rconf->if_name), &radioIndex);

     if (changed->channel || changed->ht_mode)
     {
         if (!wifi_setRadioChannel(radioIndex, rconf->channel, rconf->ht_mode))
         {
             LOGE("%s: cannot change radio channel for %s", __func__, rconf->if_name);
             return false;
         }
     }

     return radio_state_update(radioIndex);
 }

bool radio_rops_vstate(struct schema_Wifi_VIF_State *vstate)
{
    if (!g_rops.op_vstate)
    {
        LOGE("%s: op_vstate not set", __func__);
        return false;
    }

    g_rops.op_vstate(vstate);
    return true;
}

bool radio_rops_vconfig(
        struct schema_Wifi_VIF_Config *vconf,
        const char *radio_ifname)
{
    if (!g_rops.op_vconf)
    {
        LOGE("%s: op_vconf not set", __func__);
        return false;
    }

    g_rops.op_vconf(vconf, radio_ifname);
    return true;
}

