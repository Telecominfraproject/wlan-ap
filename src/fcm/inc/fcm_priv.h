/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

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

#ifndef FCM_PRIV_H_INCLUDED
#define FCM_PRIV_H_INCLUDED

#include "ds_list.h"
#include "ds_tree.h"
#include "fcm.h"

#define FCM_DSO_PATH             "dso_path"
#define FCM_DSO_INIT             "dso_init"
#define FCM_DSO_DFLT_PATH        "/usr/plume/lib"
#define FCM_DSO_PREFIX           "/libfcm_"
#define FCM_DSO_TYPE             ".so"
#define FCM_DSO_PATH_LEN         (255)
#define FCM_DSO_INIT_LEN         (255)
#define FCM_SAMPLE_INTVL         (0)
#define FCM_REPORT_INTVL         (0)
#define FCM_FLTR_NAME_LEN        (33)
#define FCM_COLLECT_NAME_LEN     FCM_FLTR_NAME_LEN
#define FCM_RPT_NAME_LEN         FCM_FLTR_NAME_LEN
#define FCM_HIST_NAME_LEN        FCM_FLTR_NAME_LEN
#define FCM_MQTT_TOPIC_LEN       (257)
#define FCM_OTHER_CONFIG_KEY_LEN (65)
#define FCM_OTHER_CONFIG_VAL_LEN FCM_OTHER_CONFIG_KEY_LEN


typedef enum {
    FCM_NO_HEADER          = -1,
    FCM_HEADER_LOCATION_ID =  0,
    FCM_HEADER_NODE_ID     =  1,
    FCM_NUM_HEADER_IDS     =  2,
} fcm_header_ids;

// Holds other_config
typedef struct fcm_other_config_
{
    char key[FCM_OTHER_CONFIG_KEY_LEN];
    char val[FCM_OTHER_CONFIG_VAL_LEN];
    ds_dlist_node_t node;
} fcm_other_config_t;

// Holds FCM_Collect_Config
typedef struct fcm_collect_conf_
{
    char name[FCM_COLLECT_NAME_LEN]; // node key
    unsigned int sample_time;
    char filter_name[FCM_FLTR_NAME_LEN];
    char report_name[FCM_RPT_NAME_LEN];
    ds_tree_t *other_config;
    ds_tree_node_t node;
} fcm_collect_conf_t;

// Holds FCM_Report_Config
typedef struct fcm_report_conf_
{
    char name[FCM_RPT_NAME_LEN]; // node key
    unsigned int report_time;
    fcm_rpt_fmt_t fmt;
    unsigned int hist_time;
    char hist_filter[FCM_HIST_NAME_LEN];
    char report_filter[FCM_FLTR_NAME_LEN];
    char mqtt_topic[FCM_MQTT_TOPIC_LEN];
    ds_tree_t *other_config;
    ds_tree_node_t node;
} fcm_report_conf_t;

typedef struct fcm_hist_
{
    unsigned int ticks;   // hist_time = hist_ticks * sample_time
    unsigned int curr_ticks;
} fcm_hist_t;

typedef struct fcm_report_
{
    unsigned int report_time;
    unsigned int ticks; // report_time = ticks * sample_time
    unsigned int curr_ticks;
    fcm_hist_t hist;
    int64_t count; // Number of mqtt reports sent
} fcm_report_t;

typedef struct fcm_collector_
{
    char dso_path[FCM_DSO_PATH_LEN]; // Path of plugin shared lib
    char dso_init[FCM_DSO_INIT_LEN]; // Plugin init function
    fcm_collect_conf_t collect_conf;
    void *handle; // Stores dlopen handle
    void *plugin_init; // Stores the plugin entry function
    ev_timer sample_timer;
    fcm_report_t report;
    fcm_collect_plugin_t plugin; // Plugin collect config
    bool initialized;
    ds_tree_node_t node;
} fcm_collector_t;

int fcm_ovsdb_init(void);

#endif /* FCM_PRIV_H_INCLUDED */
