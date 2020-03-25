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

#ifndef FCM_H_INCLUDED
#define FCM_H_INCLUDED

typedef enum
{
    FCM_RPT_NO_FMT     = -1,
    FCM_RPT_FMT_CUMUL  =  0,
    FCM_RPT_FMT_DELTA  =  1,
    FCM_RPT_FMT_RAW    =  2
} fcm_rpt_fmt_t;

typedef struct fcm_plugin_filter_
{
    char *collect;
    char *hist;
    char *report;
} fcm_plugin_filter_t;

typedef struct fcm_collect_plugin_
{
    void *plugin_ctx;      // set context by plugin and used by plugin
    void *plugin_fcm_ctx;  // set context by plugin and used by FCM framework
    void *fcm_plugin_ctx;  // set context by FCM framework and used by plugin
    fcm_plugin_filter_t filters;
    fcm_rpt_fmt_t fmt;
    char *mqtt_topic;
    void *fcm;
    int sample_interval;
    int report_interval;
    void (*collect_periodic)(struct fcm_collect_plugin_ *);
    void (*send_report)(struct fcm_collect_plugin_ *);
    void (*close_plugin)(struct fcm_collect_plugin_ *);
} fcm_collect_plugin_t;

char* fcm_get_mqtt_hdr_node_id(void);
char* fcm_get_mqtt_hdr_loc_id(void);

// Feature Test APIs
char* fcm_plugin_get_other_config(fcm_collect_plugin_t *plugin, char *key);

#endif /* FCM_H_INCLUDED */
