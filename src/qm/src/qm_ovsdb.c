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

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "target.h"

#include "qm.h"

#define MODULE_ID LOG_MODULE_ID_OVSDB

ovsdb_table_t table_AWLAN_Node;

void callback_AWLAN_Node(ovsdb_update_monitor_t *mon,
        struct schema_AWLAN_Node *old_rec,
        struct schema_AWLAN_Node *awlan)
{
    int          ii;
    const char  *mqtt_broker = NULL;
    const char  *mqtt_topic = NULL;
    const char  *mqtt_qos = NULL;
    const char  *mqtt_port = NULL;
    int         mqtt_compress = 0;
    int         log_interval = 0;

    LOG(DEBUG, "%s %d %d", __FUNCTION__, mon->mon_type,
            awlan ? awlan->mqtt_settings_len : 0);

    // Apply MQTT settings
    if (mon->mon_type != OVSDB_UPDATE_DEL) {
        // parse map of mqtt settings
        for (ii = 0; ii < awlan->mqtt_settings_len; ii++)
        {
            const char *key = awlan->mqtt_settings_keys[ii];
            const char *val = awlan->mqtt_settings[ii];
            LOGT("mqtt_settings[%s]='%s'", key, val);

            if (strcmp(key, "broker") == 0)
            {
                mqtt_broker = val;
            }
            else if (strcmp(key, "topics") == 0)
            {
                mqtt_topic = val;
            }
            else if (strcmp(key, "qos") == 0)
            {
                mqtt_qos = val;
            }
            else if (strcmp(key, "port") == 0)
            {
                mqtt_port = val;
            }
            else if (strcmp(key, "compress") == 0)
            {
                if (strcmp(val, "zlib") == 0) mqtt_compress = 1;
            }
            else if (strcmp(key, "remote_log") == 0)
            {
                log_interval = atoi(val);
                if (log_interval < 0) log_interval = 0;
            }
            else
            {
                LOG(ERR, "Unkown MQTT option: %s", key);
            }
        }
    }

    qm_mqtt_set(mqtt_broker, mqtt_port, mqtt_topic, mqtt_qos, mqtt_compress);
    qm_mqtt_set_log_interval(log_interval);
}



int qm_ovsdb_init(void)
{
    LOGI("Initializing QM tables");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT_NO_KEY(AWLAN_Node);

    // Initialize OVSDB monitor callbacks
    OVSDB_TABLE_MONITOR(AWLAN_Node, false);

    return 0;
}
