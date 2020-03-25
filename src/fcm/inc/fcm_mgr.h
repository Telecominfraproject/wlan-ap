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

#ifndef FCM_MGR_H_INCLUDED
#define FCM_MGR_H_INCLUDED

#include <ev.h>          // libev routines

#include "ds_tree.h"
#include "fcm.h"

typedef struct fcm_mgr_
{
    struct ev_loop *loop;
    char *mqtt_headers[FCM_NUM_HEADER_IDS];
    ds_tree_t collect_tree;      // Holds fcm_collector_t
    ds_tree_t report_conf_tree;  // Holds fcm_report_conf_t
} fcm_mgr_t;

bool fcm_init_mgr(struct ev_loop *loop);
fcm_mgr_t* fcm_get_mgr(void);
bool init_collect_config(struct schema_FCM_Collector_Config *conf);
void update_collect_config(struct schema_FCM_Collector_Config *conf);
void delete_collect_config(struct schema_FCM_Collector_Config *conf);
void init_report_config(struct schema_FCM_Report_Config *conf);
void update_report_config(struct schema_FCM_Report_Config *conf);
void delete_report_config(struct schema_FCM_Report_Config *conf);

#endif /* FCM_MGR_H_INCLUDED */
