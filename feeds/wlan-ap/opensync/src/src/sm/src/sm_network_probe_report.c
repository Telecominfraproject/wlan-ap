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

#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ev.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

#include "sm.h"

#define MODULE_ID LOG_MODULE_ID_MAIN


/* new part */
typedef struct
{
    bool                            initialized;

    /* Internal structure used to lower layer radio selection */
    ev_timer                        report_timer;

    /* Structure containing cloud request timer params */
    sm_stats_request_t              request;
    /* Structure pointing to upper layer network probe storage */
    dpp_network_probe_report_data_t        report;

    /* Reporting start timestamp used for reporting timestamp calculation */
    uint64_t                        report_ts;
} sm_network_probe_ctx_t;

/* Common place holder for all neighbor stat report contexts */
static sm_network_probe_ctx_t              g_sm_network_probe_ctx;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static
bool dpp_network_probe_report_timer_set(
        ev_timer                   *timer,
        bool                        enable)
{
    if (enable) {
        ev_timer_again(EV_DEFAULT, timer);
    }
    else {
        ev_timer_stop(EV_DEFAULT, timer);
    }

    return true;
}


static
bool dpp_network_probe_report_timer_restart(
        ev_timer                   *timer)
{
    sm_network_probe_ctx_t                *network_probe_ctx =
        (sm_network_probe_ctx_t *) timer->data;
    sm_stats_request_t             *request_ctx =
        &network_probe_ctx->request;

    if (request_ctx->reporting_count) {
        request_ctx->reporting_count--;

        LOG(DEBUG,
            "Updated network probe reporting count=%d",
            request_ctx->reporting_count);

        /* If reporting_count becomes zero, then stop reporting */
        if (0 == request_ctx->reporting_count) {
            dpp_network_probe_report_timer_set(timer, false);

            LOG(DEBUG,
                "Stopped network probe reporting (count expired)");
            return true;
        }
    }

    return true;
}

static
void sm_network_probe_report (EV_P_ ev_timer *w, int revents)
{
    bool                           rc;

    sm_network_probe_ctx_t                *network_probe_ctx =
        (sm_network_probe_ctx_t *) w->data;
    dpp_network_probe_report_data_t       *report_ctx =
        &network_probe_ctx->report;
    sm_stats_request_t             *request_ctx =
        &network_probe_ctx->request;
    ev_timer                       *report_timer =
        &network_probe_ctx->report_timer;

    dpp_network_probe_report_timer_restart(report_timer);

    /* Get network probe stats */
    rc =
        target_stats_network_probe_get (
                &report_ctx->record);
    if (true != rc) {
        return;
    }

    LOG(DEBUG,
        "Sending network probe");

    /* Report_timestamp is base-timestamp + relative start time offset */
    report_ctx->timestamp_ms =
        request_ctx->reporting_timestamp - network_probe_ctx->report_ts +
        get_timestamp();

    LOG(INFO,
        "Sending network probe report at '%s' , latency : '%d'",
        sm_timestamp_ms_to_date(report_ctx->timestamp_ms), report_ctx->record.dns_probe.latency);

    dpp_put_network_probe(report_ctx);
}


/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
bool sm_network_probe_report_request(
        sm_stats_request_t         *request)
{
    sm_network_probe_ctx_t         *network_probe_ctx =
        &g_sm_network_probe_ctx;
    sm_stats_request_t             *request_ctx =
        &network_probe_ctx->request;
    dpp_network_probe_report_data_t       *report_ctx =
        &network_probe_ctx->report;
    ev_timer                       *report_timer =
        &network_probe_ctx->report_timer;


    if (NULL == request) {
        LOG(ERR,
            "Initializing network probe reporting "
            "(Invalid request config)");
        return false;
    }

    /* Initialize global stats only once */
    if (!network_probe_ctx->initialized) {
        memset(request_ctx, 0, sizeof(*request_ctx));
        memset(report_ctx, 0, sizeof(*report_ctx));

        LOG(INFO,
            "Initializing network probe reporting");

        /* Initialize event lib timers and pass the global
           internal cache
         */
        ev_init (report_timer, sm_network_probe_report);
        report_timer->data = network_probe_ctx;

        network_probe_ctx->initialized = true;
    }

    /* Store and compare every request parameter ...
       memcpy would be easier but we want some debug info
     */
    REQUEST_VAL_UPDATE("network_probe", reporting_count, "%d");
    REQUEST_VAL_UPDATE("network_probe", reporting_interval, "%d");
    REQUEST_VAL_UPDATE("network_probe", reporting_timestamp, "%"PRIu64"");

    /* Restart timers with new parameters */
    dpp_network_probe_report_timer_set(report_timer, false);

    if (request_ctx->reporting_interval) {
        network_probe_ctx->report_ts = get_timestamp();
        report_timer->repeat = request_ctx->reporting_interval;
        dpp_network_probe_report_timer_set(report_timer, true);

        LOG(INFO, "Started network probe reporting");
    }
    else {
        LOG(INFO, "Stopped network probe reporting");
        memset(request_ctx, 0, sizeof(*request_ctx));
    }

    return true;
}
