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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ev.h>
#include <mosquitto.h>

#include "log.h"
#include "os_socket.h"
#include "os_backtrace.h"
#include "ovsdb.h"
#include "schema.h"
#include "jansson.h"

#include "monitor.h"
#include "json_util.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"

#include "target.h"
#include "pasync.h"
#include "dm.h"


/* speed test json output decoding  */
#include "dm_st_pjs.h"
#include "pjs_gen_h.h"
#include "dm_st_pjs.h"
#include "pjs_gen_c.h"

#define ST_STATUS_OK    (0)     /* everything all right         */
#define ST_STATUS_JSON  (-1)    /* received non-json ST output  */
#define ST_STATUS_READ  (-2)    /* can't convert json to struct */


/* can't be on the stack */
static ovsdb_update_monitor_t st_monitor;
static bool st_in_progress = false;  /* prevent multiple speedtests simultaneous run */


/* pasync API callback, invoked upon speedtest completes its job     */
void pa_cb(int id, void * buff, int buff_sz)
{
    json_t * js = NULL;
    json_error_t    je;
    json_t * res = NULL;
    static struct streport report; /* used to convert ST output to struct */
    struct schema_Wifi_Speedtest_Status  st_status;
    pjs_errmsg_t err;
    int status = ST_STATUS_OK;
    ovs_uuid_t uuid;

    LOG(NOTICE, "Received speed test result");

    /* signal that ST has been completed */
    st_in_progress = false;

    /* clear all data from last report */
    memset(&report, 0, sizeof(struct streport));

    js = json_loads(buff, 0, &je);

    if (NULL == js)
    {
        LOG(ERR, "ST report:\n%s", (char*)buff);
        LOG(ERR, "ST report JSON validation failed=%s line=%d pos=%d",
           je.text,
           je.line,
           je.position);

        status = ST_STATUS_JSON;
    }
    else
    {
        if (!streport_from_json(&report, js, false, NULL))
        {
            LOG(ERR, "ST report conversion failed");
            status = ST_STATUS_READ;
            /* make sure that all _exists fields are set to false */
            memset(&report, 0, sizeof(struct streport));
        }
    }

    /* zero whole st_status structure */
    memset(&st_status, 0, sizeof(struct schema_Wifi_Speedtest_Status));

    /*set all relevant fields in st_status */
    if (report.downlink_exists)
    {
        st_status.DL = report.downlink;
        st_status.DL_exists = true;
    }

    if (report.uplink_exists)
    {
        st_status.UL = report.uplink;
        st_status.UL_exists = true;
    }

    if (report.latency_exists)
    {
        st_status.RTT = report.latency;
        st_status.RTT_exists = true;
    }

    if (report.isp_exists)
    {
        strcpy(st_status.ISP, report.isp);
        st_status.ISP_exists = true;
    }

    if (report.sponsor_exists)
    {
        strcpy(st_status.server_name, report.sponsor);
        st_status.server_name_exists = true;
    }

    if (report.timestamp_exists)
    {
        st_status.timestamp = report.timestamp;
        st_status.timestamp_exists = true;
    }

    if (report.DL_bytes_exists)
    {
        st_status.DL_bytes= report.DL_bytes;
        st_status.DL_bytes_exists = true;
    }

    if (report.UL_bytes_exists)
    {
        st_status.UL_bytes= report.UL_bytes;
        st_status.UL_bytes_exists = true;
    }

    st_status.testid = id;
    st_status.status = status;
    strcpy(st_status.test_type, "OOKLA");
    st_status.test_type_exists = true;

    /* fill the row with NODE data */
    if (false == ovsdb_sync_insert(SCHEMA_TABLE(Wifi_Speedtest_Status),
                                   schema_Wifi_Speedtest_Status_to_json(&st_status, err),
                                   &uuid)
       )
    {
        LOG(ERR, "Speedtest_Status insert error, ST results not written: DL: %f, UL: %f",
            st_status.DL,
            st_status.UL);
    }
    else
    {
        LOG(NOTICE, "Speedtest results written stamp: %d, status: %d, DL: %f, UL: %f, isp: %s, sponsor: %s, latency %f",
                     st_status.timestamp,
                     st_status.status,
                     st_status.DL,
                     st_status.UL,
                     st_status.ISP,
                     st_status.server_name,
                     st_status.RTT);
    }

    /* release memory */
pa_cb_end:
    if(NULL != js) json_decref(js);
    if(NULL != res) json_decref(res);

}


void dm_stupdate_cb(ovsdb_update_monitor_t *self)
{
    pjs_errmsg_t perr;
    struct schema_Wifi_Speedtest_Config st_config;

    LOG(DEBUG, "%s", __FUNCTION__);

    switch (self->mon_type)
    {
        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:

            if (st_in_progress)
            {
                LOG(ERR, "Speedtest already in progress");
                return;
            }

            if (!schema_Wifi_Speedtest_Config_from_json(&st_config, self->mon_json_new, false, perr))
            {
                LOG(ERR, "Parsing Wifi_Speedtest_Config NEW or MODIFY request: %s", perr);
                return;
            }

            /* run the speed test according to the cloud instructions */
            if (!strcmp(st_config.test_type, "OOKLA"))
            {
                const char* tools_dir = target_tools_dir();
                if (tools_dir == NULL)
                {
                    LOG(ERR, "Error, tools dir not defined");
                }
                else
                {
                    struct stat sb;
                    if ( !(0 == stat(tools_dir, &sb) && S_ISDIR(sb.st_mode)) )
                    {
                        LOG(ERR, "Error, tools dir does not exist");
                    }
                }

                char st_cmd[TARGET_BUFF_SZ];
                sprintf(st_cmd, "%s/st_ookla -qlvJ", tools_dir);
                if (false == pasync_ropen(EV_DEFAULT, st_config.testid, st_cmd, pa_cb))
                {
                    LOG(ERR, "Error running pasync_ropen 1");
                }
                else
                {
                    LOG(NOTICE, "Speedtest started!");
                    st_in_progress = true;
                }
            }
            else if (!strcmp(st_config.test_type, "MPLAB"))
            {
                LOG(ERR, "MPLAB speedtest not yet implemented");
            }
            else
            {
                LOG(ERR, "%s speedtest doesn't exist", st_config.test_type);
            }

            break;

        case OVSDB_UPDATE_DEL:
            /* Reset configuration */
            LOG(INFO, "Cloud cleared Wifi_Speedtest_Config table");
            break;

        default:
            LOG(ERR, "Update Monitor for Wifi_Speedtest_Config reported an error.");
            break;
    }
}


/*
 * Monitor Wifi_Speedtest_Config table
 */
bool dm_st_monitor()
{
    bool        ret = false;

    /* Set monitoring */
    if (false == ovsdb_update_monitor(&st_monitor,
                                      dm_stupdate_cb,
                                      SCHEMA_TABLE(Wifi_Speedtest_Config),
                                      OMT_ALL)
       )
    {
        LOG(ERR, "Error initializing Wifi_Speedtest_Config monitor");
        goto exit;
    }
    else
    {
        LOG(NOTICE, "Wifi_Speedtest_Config monitor started");
        st_in_progress = false;
    }
    ret = true;

exit:
    return ret;
}
