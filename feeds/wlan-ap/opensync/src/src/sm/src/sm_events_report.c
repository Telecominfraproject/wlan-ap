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
extern dpp_event_record_t g_event_record;

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

	/* event list (only one for now) */
	ds_dlist_t record_list;

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

static void sm_events_report(EV_P_ ev_timer *w, int revents)
{
	sm_events_ctx_t *events_ctx = (sm_events_ctx_t *)w->data;
	dpp_event_report_data_t *report_ctx = &events_ctx->report;
	ev_timer *report_timer = &events_ctx->report_timer;

	/* Event Record */
	dpp_event_record_t *dpp_record = NULL;

	/* Client Session */
	dpp_event_record_session_t *dpp_session = NULL;
	dpp_event_record_session_t *target_session = NULL;
	ds_dlist_iter_t session_iter;

	/* Client Assoc Event */
	dpp_event_record_assoc_t *dpp_assoc = NULL;
	dpp_event_record_assoc_t *target_assoc = NULL;
	ds_dlist_iter_t assoc_iter;

	/* Client Auth Event */
	dpp_event_record_auth_t *dpp_auth = NULL;
	dpp_event_record_auth_t *target_auth = NULL;
	ds_dlist_iter_t auth_iter;

	/* Client Disconnect Event */
	dpp_event_record_disconnect_t *dpp_disconnect = NULL;
	dpp_event_record_disconnect_t *target_disconnect = NULL;
	ds_dlist_iter_t disconnect_iter;

	/* Client FirstData Event */
	dpp_event_record_first_data_t *dpp_first_data = NULL;
	dpp_event_record_first_data_t *target_first_data = NULL;
	ds_dlist_iter_t first_data_iter;

	/* Client Ip Event */
	dpp_event_record_ip_t *dpp_ip = NULL;
	dpp_event_record_ip_t *target_ip = NULL;
	ds_dlist_iter_t ip_iter;

	/* Client Connect Event */
	dpp_event_record_connect_t *dpp_connect = NULL;

	dpp_events_report_timer_restart(report_timer);

	dpp_record = calloc(1, sizeof(dpp_event_record_t));

	for (target_session = ds_dlist_ifirst(&session_iter, &g_event_record.client_session); target_session != NULL; target_session = ds_dlist_inext(&session_iter)) {
		dpp_session = calloc(1, sizeof(dpp_event_record_session_t));
		dpp_session->session_id = target_session->session_id;
		if (ds_dlist_is_empty(&target_session->disconnect_list)) {
			dpp_connect = calloc(1, sizeof(dpp_event_record_connect_t));
			dpp_connect->session_id = target_session->session_id;
			dpp_connect->timestamp = (uint32_t)time(NULL);
			for(target_assoc = ds_dlist_ifirst(&assoc_iter, &target_session->assoc_list); target_assoc != NULL; target_assoc = ds_dlist_inext(&assoc_iter)) {
				strcpy(dpp_connect->sta_mac, target_assoc->sta_mac);
				dpp_connect->ev_time_bootup_in_us_assoc = target_assoc->timestamp;
				dpp_connect->band = target_assoc->band;
				dpp_connect->assoc_type = target_assoc->assoc_type;
				strcpy(dpp_connect->ssid, target_assoc->ssid);
				dpp_connect->using11k = target_assoc->using11k;
				dpp_connect->using11r = target_assoc->using11r;
				dpp_connect->using11v = target_assoc->using11v;
				dpp_connect->assoc_rssi = target_assoc->rssi;
				ds_dlist_iremove(&assoc_iter);
				free(target_assoc);
			}

			for (target_auth = ds_dlist_ifirst(&auth_iter, &target_session->auth_list); target_auth != NULL; target_auth = ds_dlist_inext(&auth_iter)) {
				dpp_connect->ev_time_bootup_in_us_auth = target_auth->timestamp;
				ds_dlist_iremove(&auth_iter);
				free(target_auth);
			}

			for (target_disconnect = ds_dlist_ifirst(&disconnect_iter, &target_session->disconnect_list); target_disconnect != NULL; target_disconnect = ds_dlist_inext(&disconnect_iter)) {
				dpp_connect->ev_time_bootup_in_us_first_tx = target_first_data->fdata_tx_up_ts_in_us;
				dpp_connect->ev_time_bootup_in_us_first_rx = target_first_data->fdata_rx_up_ts_in_us;
				ds_dlist_iremove(&first_data_iter);
				free(target_first_data);
			}

			for (target_ip = ds_dlist_ifirst(&ip_iter, &target_session->ip_list); target_ip != NULL; target_ip = ds_dlist_inext(&ip_iter)) {
				strcpy(dpp_connect->ip_addr, target_ip->ip_addr);
				dpp_connect->ev_time_bootup_in_us_ip = target_ip->timestamp;
				ds_dlist_iremove(&ip_iter);
				free(target_ip);
			}

			if (ds_dlist_is_empty(&dpp_session->connect_list)) {
				ds_dlist_init(&dpp_session->connect_list, dpp_event_record_connect_t, node);
			}

			ds_dlist_insert_tail(&dpp_session->connect_list, dpp_connect);

		} else {

			for(target_assoc = ds_dlist_ifirst(&assoc_iter, &target_session->assoc_list); target_assoc != NULL; target_assoc = ds_dlist_inext(&assoc_iter)) {
				dpp_assoc = calloc(1, sizeof(dpp_event_record_assoc_t));
				strcpy(dpp_assoc->sta_mac, target_assoc->sta_mac);
				dpp_assoc->session_id = target_assoc->session_id;
				strcpy(dpp_assoc->ssid, target_assoc->ssid);
				dpp_assoc->timestamp = target_assoc->timestamp;
				dpp_assoc->band = target_assoc->band;
				dpp_assoc->assoc_type = target_assoc->assoc_type;
				dpp_assoc->status = target_assoc->status;
				dpp_assoc->rssi = target_assoc->rssi;
				dpp_assoc->internal_sc = target_assoc->internal_sc;
				dpp_assoc->using11k = target_assoc->using11k;
				dpp_assoc->using11r = target_assoc->using11r;
				dpp_assoc->using11v = target_assoc->using11v;
				ds_dlist_iremove(&assoc_iter);
				free(target_assoc);
				if (ds_dlist_is_empty(&dpp_session->assoc_list)) {
					ds_dlist_init(&dpp_session->assoc_list, dpp_event_record_assoc_t, node);
				}

				ds_dlist_insert_tail(&dpp_session->assoc_list, dpp_assoc);
			}

			for (target_auth = ds_dlist_ifirst(&auth_iter, &target_session->auth_list); target_auth != NULL; target_auth = ds_dlist_inext(&auth_iter)) {
				dpp_auth = calloc(1, sizeof(dpp_event_record_auth_t));
				strcpy(dpp_auth->sta_mac, target_auth->sta_mac);
				dpp_auth->session_id = target_auth->session_id;
				dpp_auth->timestamp = target_auth->timestamp;
				strcpy(dpp_auth->ssid, target_auth->ssid);
				dpp_auth->band = target_auth->band;
				dpp_auth->auth_status = target_auth->auth_status;
				ds_dlist_iremove(&auth_iter);
				free(target_auth);
				if (ds_dlist_is_empty(&dpp_session->auth_list)) {
					ds_dlist_init(&dpp_session->auth_list, dpp_event_record_auth_t, node);
				}

				ds_dlist_insert_tail(&dpp_session->auth_list, dpp_auth);
			}

			for (target_disconnect = ds_dlist_ifirst(&disconnect_iter, &target_session->disconnect_list); target_disconnect != NULL; target_disconnect = ds_dlist_inext(&disconnect_iter)) {
				dpp_disconnect = calloc(1, sizeof(dpp_event_record_disconnect_t));
				strcpy(dpp_disconnect->sta_mac, target_disconnect->sta_mac);
				dpp_disconnect->session_id = target_disconnect->session_id;
				dpp_disconnect->timestamp = target_disconnect->timestamp;
				dpp_disconnect->reason = target_disconnect->reason;
				dpp_disconnect->dev_type = target_disconnect->dev_type;
				dpp_disconnect->fr_type = target_disconnect->fr_type;
				dpp_disconnect->last_sent_up_ts_in_us = target_disconnect->last_sent_up_ts_in_us;
				dpp_disconnect->last_recv_up_ts_in_us = target_disconnect->last_recv_up_ts_in_us;
				dpp_disconnect->internal_rc = target_disconnect->internal_rc;
				dpp_disconnect->rssi = target_disconnect->rssi;
				strcpy(dpp_disconnect->ssid, target_disconnect->ssid);
				dpp_disconnect->band = target_disconnect->band;
				ds_dlist_iremove(&disconnect_iter);
				free(target_disconnect);
				if (ds_dlist_is_empty(&dpp_session->disconnect_list)) {
					ds_dlist_init(&dpp_session->disconnect_list, dpp_event_record_disconnect_t, node);
				}

				ds_dlist_insert_tail(&dpp_session->disconnect_list, dpp_disconnect);
			}

			for (target_first_data = ds_dlist_ifirst(&first_data_iter, &target_session->first_data_list); target_first_data != NULL; target_first_data = ds_dlist_inext(&first_data_iter)) {
				dpp_first_data = calloc(1, sizeof(dpp_event_record_first_data_t));
				strcpy(dpp_first_data->sta_mac, target_first_data->sta_mac);
				dpp_first_data->session_id = target_first_data->session_id;
				dpp_first_data->timestamp = target_first_data->timestamp;
				dpp_first_data->fdata_tx_up_ts_in_us = target_first_data->fdata_tx_up_ts_in_us;
				dpp_first_data->fdata_rx_up_ts_in_us = target_first_data->fdata_rx_up_ts_in_us;
				ds_dlist_iremove(&first_data_iter);
				free(target_first_data);
				if (ds_dlist_is_empty(&dpp_session->first_data_list)) {
					ds_dlist_init(&dpp_session->first_data_list, dpp_event_record_first_data_t, node);
				}

				ds_dlist_insert_tail(&dpp_session->first_data_list, dpp_first_data);
			}

			for (target_ip = ds_dlist_ifirst(&ip_iter, &target_session->ip_list); target_ip != NULL; target_ip = ds_dlist_inext(&ip_iter)) {
				dpp_ip = calloc(1, sizeof(dpp_event_record_ip_t));
				strcpy(dpp_ip->sta_mac, target_ip->sta_mac);
				strcpy(dpp_ip->ip_addr, target_ip->ip_addr);
				dpp_ip->session_id = target_ip->session_id;
				dpp_ip->timestamp = target_ip->timestamp;
				ds_dlist_iremove(&ip_iter);
				free(target_ip);
				if (ds_dlist_is_empty(&dpp_session->ip_list)) {
					ds_dlist_init(&dpp_session->ip_list, dpp_event_record_ip_t, node);
				}

				ds_dlist_insert_tail(&dpp_session->ip_list, dpp_ip);
			}
		}

		ds_dlist_iremove(&session_iter);
		free(target_session);
		if (ds_dlist_is_empty(&dpp_record->client_session)) {
			ds_dlist_init(&dpp_record->client_session, dpp_event_record_session_t, node);
		}

		ds_dlist_insert_tail(&dpp_record->client_session, dpp_session);
	}

	if (ds_dlist_is_empty(&report_ctx->list)) {
		ds_dlist_init(&report_ctx->list, dpp_event_record_t, node);
	}

	ds_dlist_insert_tail(&report_ctx->list, dpp_record);

	while(!ds_dlist_is_empty(&g_event_record.client_session)) {
		ds_dlist_remove_head(&g_event_record.client_session);
	}

	LOG(INFO, "Sending events report...");
	if (!ds_dlist_is_empty(&report_ctx->list)) {
		dpp_put_events(report_ctx);
	}
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
		ds_dlist_init(&report_ctx->list, dpp_event_record_t, node);

		/* Initialize event list */
		ds_dlist_init(&events_ctx->record_list, dpp_event_record_t,
			      node);

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
