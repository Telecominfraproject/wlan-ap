/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <net/if.h>

#include <sys/types.h>

#include <ev.h>

#include <syslog.h>
#include <unl.h>

#include "sm.h"
#include "ubus_collector.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

/* global list populated by ubus_collector */
extern dpp_event_report_data_t g_event_report;

/* new part */
typedef struct {
	bool initialized;

	/* Internal structure used to lower layer radio selection */
	radio_entry_t *radio_cfg;

	/* Internal structure used to lower layer radio selection */
	ev_timer report_timer;

	/* Structure containing cloud request timer params */
	sm_stats_request_t request;

	/* Structure pointing to upper layer events storage */
	dpp_event_report_data_t report;

	/* Reporting start timestamp used for reporting timestamp calculation */
	uint64_t report_ts;
} sm_events_ctx_t;

/* Common place holder for all events stat report contexts */
sm_events_ctx_t g_sm_events_ctx;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static bool dpp_events_report_timer_set(ev_timer *timer, bool enable)
{
	if (enable) {
		ev_timer_again(EV_DEFAULT, timer);
	} else {
		ev_timer_stop(EV_DEFAULT, timer);
	}

	return true;
}

static bool dpp_events_report_timer_restart(ev_timer *timer)
{
	sm_events_ctx_t *events_ctx = (sm_events_ctx_t *)timer->data;
	sm_stats_request_t *request_ctx = &events_ctx->request;

	if (request_ctx->reporting_count) {
		request_ctx->reporting_count--;

		LOG(DEBUG, "Updated events reporting count=%d",
		    request_ctx->reporting_count);

		/* If reporting_count becomes zero, then stop reporting */
		if (0 == request_ctx->reporting_count) {
			dpp_events_report_timer_set(timer, false);

			LOG(DEBUG, "Stopped events reporting (count expired)");
			return true;
		}
	}

	return true;
}

static void sm_events_report_clear_client(ds_dlist_t *report_list)
{
	ds_dlist_iter_t record_iter;

	if (ds_dlist_is_empty(report_list))
		return;

	dpp_event_record_t *record = NULL;
	for (record = ds_dlist_ifirst(&record_iter, report_list);
			record != NULL;
			record = ds_dlist_inext(&record_iter)) {
		ds_dlist_iremove(&record_iter);
		dpp_event_record_free(record);
		record = NULL;
	}
}

static void sm_events_report_clear_channel(ds_dlist_t *report_list)
{
	if (!ds_dlist_is_empty(report_list)) {
		dpp_event_channel_switch_t      *record = NULL;
		ds_dlist_iter_t                 record_iter;

		for (record = ds_dlist_ifirst(&record_iter, report_list);
			record != NULL;
			record = ds_dlist_inext(&record_iter))
				{
					ds_dlist_iremove(&record_iter);
					dpp_event_channel_record_free(record);
					record = NULL;
				}
	}
}

static void sm_events_report(EV_P_ ev_timer *w, int revents)
{
	sm_events_ctx_t *events_ctx = (sm_events_ctx_t *)w->data;
	dpp_event_report_data_t *report_ctx = &events_ctx->report;
	ev_timer *report_timer = &events_ctx->report_timer;

	/* Event Record */
	dpp_event_record_t *dpp_record = NULL;
	dpp_event_record_t *sm_record = NULL;
	dpp_event_channel_switch_t *dpp_record_cs = NULL;
	dpp_event_channel_switch_t *sm_record_cs = NULL;
	ds_dlist_iter_t record_iter;

	dpp_events_report_timer_restart(report_timer);

	for (sm_record = ds_dlist_ifirst(&record_iter, &g_event_report.client_event_list); sm_record != NULL; sm_record = ds_dlist_inext(&record_iter)) {
		dpp_record = dpp_event_record_alloc();
		dpp_record->client_session.session_id = sm_record->client_session.session_id;
		dpp_record->client_session.auth_event =  sm_record->client_session.auth_event;
		dpp_record->client_session.assoc_event =  sm_record->client_session.assoc_event;
		dpp_record->client_session.first_data_event =  sm_record->client_session.first_data_event;
		dpp_record->client_session.disconnect_event =  sm_record->client_session.disconnect_event;
		dpp_record->client_session.auth_event =  sm_record->client_session.auth_event;
		dpp_record->client_session.ip_event =  sm_record->client_session.ip_event;

		/* Memset all event pointers in the global event record to NULL */
		memset(&sm_record->client_session, 0, sizeof(dpp_event_record_session_t));
		sm_record->hasSMProcessed = true;

		if (ds_dlist_is_empty(&report_ctx->client_event_list)) {
			ds_dlist_init(&report_ctx->client_event_list, dpp_event_record_t, node);
		}
		ds_dlist_insert_tail(&report_ctx->client_event_list, dpp_record);
	}

	for (sm_record_cs = ds_dlist_ifirst(&record_iter, &g_event_report.channel_switch_list); sm_record_cs != NULL; sm_record_cs = ds_dlist_inext(&record_iter)) {
		dpp_record_cs = dpp_event_channel_switch_alloc();

		dpp_record_cs->channel_event.band = sm_record_cs->channel_event.band;
		dpp_record_cs->channel_event.reason =  sm_record_cs->channel_event.reason;
		dpp_record_cs->channel_event.freq =  sm_record_cs->channel_event.freq;
		dpp_record_cs->channel_event.timestamp =  sm_record_cs->channel_event.timestamp;

		ds_dlist_iremove(&record_iter);
		dpp_event_channel_record_free(sm_record_cs);
		sm_record_cs = NULL;

		if (ds_dlist_is_empty(&report_ctx->channel_switch_list)) {
			ds_dlist_init(&report_ctx->channel_switch_list, dpp_event_channel_switch_t, node);
		}
		ds_dlist_insert_tail(&report_ctx->channel_switch_list, dpp_record_cs);
	}

	if (!ds_dlist_is_empty(&report_ctx->client_event_list) || !ds_dlist_is_empty(&report_ctx->channel_switch_list)) {
		LOG(DEBUG, "Sending events report...");
		dpp_put_events(report_ctx);
	}

	sm_events_report_clear_client(&report_ctx->client_event_list);
	sm_events_report_clear_channel(&report_ctx->channel_switch_list);
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
bool sm_events_report_request(radio_entry_t *radio_cfg,
			      sm_stats_request_t *request)
{
	sm_events_ctx_t *events_ctx = &g_sm_events_ctx;
	sm_stats_request_t *request_ctx = &events_ctx->request;
	dpp_event_report_data_t *report_ctx = &events_ctx->report;
	ev_timer *report_timer = &events_ctx->report_timer;

	// save radio cfg
	events_ctx->radio_cfg = radio_cfg;

	if (NULL == request) {
		LOG(ERR, "Initializing events reporting "
			 "(Invalid request config)");
		return false;
	}

	/* Initialize global stats only once */
	if (!events_ctx->initialized) {
		memset(request_ctx, 0, sizeof(*request_ctx));
		memset(report_ctx, 0, sizeof(*report_ctx));

		LOG(INFO, "Initializing events reporting");

		/* Initialize report list */
		ds_dlist_init(&report_ctx->client_event_list, dpp_event_record_t, node);
		ds_dlist_init(&report_ctx->channel_switch_list, dpp_event_channel_switch_t, node);

		/* Initialize event lib timers and pass the global
			internal cache
		 */
		ev_init(report_timer, sm_events_report);
		report_timer->data = events_ctx;

		events_ctx->initialized = true;
	}

	/* Store and compare every request parameter ...
	memcpy would be easier but we want some debug info
	*/
	REQUEST_VAL_UPDATE("event", reporting_count, "%d");
	REQUEST_VAL_UPDATE("event", reporting_interval, "%d");
	REQUEST_VAL_UPDATE("event", reporting_timestamp, "%" PRIu64 "");

	/* Restart timers with new parameters */
	dpp_events_report_timer_set(report_timer, false);

	if (request_ctx->reporting_interval) {
		events_ctx->report_ts = get_timestamp();
		report_timer->repeat = request_ctx->reporting_interval;
		dpp_events_report_timer_set(report_timer, true);

		LOG(INFO, "Started events reporting");
	} else {
		LOG(INFO, "Stopped events reporting");
		memset(request_ctx, 0, sizeof(*request_ctx));
	}

	return true;
}
