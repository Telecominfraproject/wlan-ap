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

#ifndef WM2_H_INCLUDED
#define WM2_H_INCLUDED

#include "schema.h"
#include "ovsdb_table.h"

extern ovsdb_table_t table_Wifi_Radio_Config;
extern ovsdb_table_t table_Wifi_Radio_State;
extern ovsdb_table_t table_Wifi_VIF_Config;
extern ovsdb_table_t table_Wifi_VIF_State;
extern ovsdb_table_t table_Wifi_Credential_Config;
extern ovsdb_table_t table_Wifi_Associated_Clients;
extern ovsdb_table_t table_Openflow_Tag;

int     wm2_radio_init(void);

bool    wm2_clients_init(char *ssid);

bool    wm2_clients_update(struct schema_Wifi_Associated_Clients *client,
                           char *vif,
                           bool associated);

void wm2_radio_update_port_state(const char *cloud_vif_ifname);

// v1 api:

void callback_Wifi_Radio_Config_v1(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_Radio_Config *old_rec,
        struct schema_Wifi_Radio_Config *rconf,
        ovsdb_cache_row_t *row);

void callback_Wifi_VIF_Config_v1(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_VIF_Config   *old_rec,
        struct schema_Wifi_VIF_Config   *vconf,
        ovsdb_cache_row_t               *row);

void wm2_radio_config_init_v1();

#endif
