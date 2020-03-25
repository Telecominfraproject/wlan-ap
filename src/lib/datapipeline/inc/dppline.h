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

#ifndef __DPPLINE__H__
#define __DPPLINE__H__

#ifdef TARGET_NATIVE
#define DPP_FAST_PACK
// if fast pack is enabled, get_report won't consider
// size as a hard limit (which is an AWS requirement) but a suggestion
// so, adding new reports stops when suggested size is exceeded
// but report will be sized to fit as much as required by data
#endif

#include <stdbool.h>

#include "dpp_types.h"
#include "dpp_survey.h"
#include "dpp_neighbor.h"
#include "dpp_device.h"
#include "dpp_capacity.h"
#include "dpp_client.h"
#include "dpp_bs_client.h"
#include "dpp_rssi.h"

#ifdef USE_QM
// QM does queue-ing of reports when offline on it's own, so dpp needs
// a smaller queue size - only to merge multiple stats to single report
// 30 is requierd for mqttsim --rpm 2 to work properly
#define DPP_MAX_QUEUE_DEPTH (30)
#define DPP_MAX_QUEUE_SIZE_BYTES (512*1024) // 512 KB
#else
#define DPP_MAX_QUEUE_DEPTH (200)
#define DPP_MAX_QUEUE_SIZE_BYTES (2*1024*1024) // 2 MB
#endif

#define STATS_MQTT_BUF_SZ        (128*1024)    // 128 KB
// AWS IOT Message Size Limit = 128KB

/*
 * Initialize internal structures, call before all other APIs
 */
bool dpp_init();

/*
 * Put neighbor stats to internal queue
 */
bool dpp_put_neighbor(dpp_neighbor_report_data_t *rpt);

/*
 * Put neighbor stats to internal queue
 */
bool dpp_put_client(dpp_client_report_data_t *rpt);

/*
 * Insert channel survey report in dpp internal queue
 */
bool dpp_put_survey(dpp_survey_report_data_t *rpt);

/*
 * Insert channel capacity report in dpp internal queue
 */
bool dpp_put_capacity(dpp_capacity_report_data_t *  rpt);

/*
 * Insert device stats dpp internal queue
 */
bool dpp_put_device(dpp_device_report_data_t * rpt);

/*
 * Insert band steering stats into dpp internal queue
 */
bool dpp_put_bs_client(dpp_bs_client_report_data_t * rpt);

/*
 * Insert RSSI stats into dpp internal queue
 */
bool dpp_put_rssi(dpp_rssi_report_data_t *rpt);

/*
 * Get the protobuf packed buffer
 *
 * This buffer is ready to be send using MQTT
 */
#ifndef DPP_FAST_PACK
bool dpp_get_report(uint8_t * buff, size_t sz, uint32_t * packed_sz);
#else
bool dpp_get_report2(uint8_t **pbuff, size_t suggest_sz, uint32_t *packed_sz);
#endif

void dpp_mac_to_str(uint8_t *mac, char *str);
char* dpp_mac_str_tmp(uint8_t *mac);

/*
 * Get the number of stats internally queued
 */
int dpp_get_queue_elements();
#endif /* __DPPLINE__H__    */
