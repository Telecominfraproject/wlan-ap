/* SPDX-License-Identifier: BSD-3-Clause */

#include "ubus_collector.h"

/* Global list of events received from hostapd */
dpp_event_report_data_t g_report_data;

/* Internal list of processed events ready to be deleted from hostapd */
static dpp_event_report_data_t *deletion_pending = NULL;
static struct ubus_context *ubus = NULL;
static char *ubus_object = NULL;

typedef struct {
	uint64_t session_id;
	char *bss;

	ds_dlist_node_t node;
} delete_entry_t;

static const struct blobmsg_policy ubus_collector_bss_list_policy[__BSS_LIST_DATA_MAX] = {
	[BSS_LIST_BSS_LIST] = {.name = "bss_list", .type = BLOBMSG_TYPE_ARRAY},
};

static const struct blobmsg_policy ubus_collector_bss_table_policy[__BSS_TABLE_MAX] = {
	[BSS_TABLE_BSS_NAME] = {.name = "name", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy ubus_collector_sessions_policy[__UBUS_SESSIONS_MAX] = {
	[UBUS_COLLECTOR_SESSIONS] = {.name = "sessions", .type = BLOBMSG_TYPE_TABLE},
};

static const struct blobmsg_policy client_session_events_policy[__CLIENT_EVENTS_MAX] = {
	[CLIENT_ASSOC_EVENT] = {.name = "ClientAssocEvent", .type = BLOBMSG_TYPE_TABLE},
	[CLIENT_AUTH_EVENT] = {.name = "ClientAuthEvent", .type = BLOBMSG_TYPE_TABLE},
	[CLIENT_DISCONNECT_EVENT] = {.name = "ClientDisconnectEvent", .type = BLOBMSG_TYPE_TABLE},
	[CLIENT_FAILURE_EVENT] = {.name = "ClientFailureEvent", .type = BLOBMSG_TYPE_TABLE},
	[CLIENT_FIRST_DATA_EVENT] = {.name = "ClientFirstDataEvent", .type = BLOBMSG_TYPE_TABLE},
	[CLIENT_TIMEOUT_EVENT] = {.name = "ClientTimeoutEvent", .type = BLOBMSG_TYPE_TABLE},
	[CLIENT_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
};

static const struct blobmsg_policy client_assoc_event_policy[__CLIENT_ASSOC_MAX] = {
	[CLIENT_ASSOC_ASSOC_TYPE] = {.name = "assoc_type", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_ASSOC_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_ASSOC_BAND] = {.name = "band", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_ASSOC_INTERNAL_SC] = {.name = "internal_sc", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_ASSOC_RSSI] = {.name = "rssi", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_ASSOC_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_ASSOC_SSID] = {.name = "ssid", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_ASSOC_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_ASSOC_USING11K] = {.name = "using11k", .type = BLOBMSG_TYPE_BOOL},
	[CLIENT_ASSOC_USING11R] = {.name = "using11r", .type = BLOBMSG_TYPE_BOOL},
	[CLIENT_ASSOC_USING11V] = {.name = "using11v", .type = BLOBMSG_TYPE_BOOL},
};

static const struct blobmsg_policy client_auth_event_policy[__CLIENT_AUTH_MAX] = {
	[CLIENT_AUTH_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_AUTH_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_AUTH_BAND] = {.name = "band", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_AUTH_AUTH_STATUS] = {.name = "auth_status", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_AUTH_AUTH_SSID] = {.name = "ssid", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_AUTH_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy client_disconnect_event_policy[__CLIENT_DISCONNECT_MAX] = {
	[CLIENT_DISCONNECT_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_DISCONNECT_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_DISCONNECT_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_DISCONNECT_BAND] = {.name = "band", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_DISCONNECT_RSSI] = {.name = "rssi", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_DISCONNECT_INTERNAL_RC] = {.name = "internal_rc", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_DISCONNECT_SSID] = {.name = "ssid", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy client_first_data_event_policy[__CLIENT_FIRST_DATA_MAX] = {
	[CLIENT_FIRST_DATA_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_FIRST_DATA_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_FIRST_DATA_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_FIRST_DATA_TX_TIMESTAMP] = {.name = "fdata_tx_up_ts_in_us", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_FIRST_DATA_RX_TIMESTAMP] = {.name = "fdata_rx_up_ts_in_us", .type = BLOBMSG_TYPE_INT64},
};



static int client_first_data_event_cb(struct blob_attr *msg,
				      dpp_event_record_session_t *dpp_session,
				      uint64_t event_session_id)
{
	int error = 0;
	struct blob_attr *tb_client_first_data_event[__CLIENT_FIRST_DATA_MAX] = {};
	char *mac_address = NULL;
	uint64_t session_id = event_session_id;
	uint32_t timestamp = 0;
	uint64_t rx_ts = 0;
	uint64_t tx_ts = 0;
	int i = 0;
	dpp_event_record_first_data_t *first_data_event_dpp = NULL;

	error = blobmsg_parse(client_first_data_event_policy,
			      __CLIENT_FIRST_DATA_MAX,
			      tb_client_first_data_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	for (i = 0; i < __CLIENT_FIRST_DATA_MAX; i++) {
		if (!tb_client_first_data_event[i]) {
			LOG(INFO,
			    "ubus_collector: could not parse first_data event %s policy",
			    client_first_data_event_policy[i].name);
			return -1;
		}
	}

	mac_address = blobmsg_get_string(
		tb_client_first_data_event[CLIENT_FIRST_DATA_STA_MAC]);
	if (!mac_address)
		return -1;

	timestamp = blobmsg_get_u32(
		tb_client_first_data_event[CLIENT_FIRST_DATA_TIMESTAMP]);
	rx_ts = blobmsg_get_u64(
		tb_client_first_data_event[CLIENT_FIRST_DATA_RX_TIMESTAMP]);
	tx_ts = blobmsg_get_u64(
		tb_client_first_data_event[CLIENT_FIRST_DATA_TX_TIMESTAMP]);

	first_data_event_dpp = calloc(1, sizeof(dpp_event_record_first_data_t));

	memcpy(first_data_event_dpp->sta_mac, mac_address,
	       MAC_ADDRESS_STRING_LEN);
	first_data_event_dpp->session_id = session_id;
	first_data_event_dpp->timestamp = timestamp;
	first_data_event_dpp->fdata_tx_up_ts_in_us = tx_ts;
	first_data_event_dpp->fdata_rx_up_ts_in_us = rx_ts;

	if (!ds_dlist_is_empty(&dpp_session->first_data_list))
		ds_dlist_init(&dpp_session->first_data_list,
			      dpp_event_record_first_data_t, node);

	ds_dlist_insert_tail(&dpp_session->first_data_list,
			     first_data_event_dpp);
	return 0;
}
static int client_disconnect_event_cb(struct blob_attr *msg,
				      dpp_event_record_session_t *dpp_session,
				      uint64_t event_session_id)
{
	int error = 0;
	int ssid_bytes = 0;
	struct blob_attr *tb_client_disconnect_event[__CLIENT_DISCONNECT_MAX] = {};
	char *mac_address = NULL;
	radio_essid_t ssid = {};
	char *ssid_temp = NULL;
	uint64_t session_id = event_session_id;
	uint32_t timestamp = 0;
	radio_type_t band = 0;
	uint32_t rssi = 0;
	uint32_t internal_rc = 0;
	int i = 0;
	dpp_event_record_disconnect_t *disconnect_event_dpp = NULL;

	error = blobmsg_parse(client_disconnect_event_policy,
			      __CLIENT_DISCONNECT_MAX,
			      tb_client_disconnect_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	for (i = 0; i < __CLIENT_DISCONNECT_MAX; i++) {
		if (!tb_client_disconnect_event[i]) {
			LOG(INFO,
			    "ubus_collector: could not parse disconnect event %s policy",
			    client_disconnect_event_policy[i].name);
			return -1;
		}
	}

	mac_address = blobmsg_get_string(
		tb_client_disconnect_event[CLIENT_DISCONNECT_STA_MAC]);
	if (!mac_address)
		return -1;

	ssid_temp = blobmsg_get_string(
		tb_client_disconnect_event[CLIENT_DISCONNECT_SSID]);
	ssid_bytes = snprintf(ssid, RADIO_ESSID_LEN + 1, "%s", ssid_temp);
	if (ssid_bytes > RADIO_ESSID_LEN)
		return -1;

	band = blobmsg_get_u8(
		tb_client_disconnect_event[CLIENT_DISCONNECT_BAND]);
	rssi = blobmsg_get_u32(
		tb_client_disconnect_event[CLIENT_DISCONNECT_RSSI]);
	timestamp = blobmsg_get_u32(
		tb_client_disconnect_event[CLIENT_DISCONNECT_TIMESTAMP]);
	internal_rc = blobmsg_get_u32(
		tb_client_disconnect_event[CLIENT_DISCONNECT_INTERNAL_RC]);

	disconnect_event_dpp = calloc(1, sizeof(dpp_event_record_disconnect_t));

	memcpy(disconnect_event_dpp->sta_mac, mac_address,
	       MAC_ADDRESS_STRING_LEN);
	memcpy(disconnect_event_dpp->ssid, ssid, ssid_bytes + 1);
	disconnect_event_dpp->session_id = session_id;
	disconnect_event_dpp->timestamp = timestamp;
	disconnect_event_dpp->band = band;
	disconnect_event_dpp->rssi = rssi;
	disconnect_event_dpp->internal_rc = internal_rc;

	if (!ds_dlist_is_empty(&dpp_session->disconnect_list))
		ds_dlist_init(&dpp_session->disconnect_list,
			      dpp_event_record_disconnect_t, node);

	ds_dlist_insert_tail(&dpp_session->disconnect_list,
			     disconnect_event_dpp);
	return 0;
}

static int client_auth_event_cb(struct blob_attr *msg,
				dpp_event_record_session_t *dpp_session,
				uint64_t event_session_id)
{
	int error = 0;
	int ssid_bytes = 0;
	struct blob_attr *tb_client_auth_event[__CLIENT_ASSOC_MAX] = {};
	char *mac_address = NULL;
	radio_essid_t ssid = {};
	char *ssid_temp = NULL;
	uint64_t session_id = event_session_id;
	uint32_t timestamp = 0;
	radio_type_t band = 0;
	uint32_t auth_status = 0;
	int i = 0;
	dpp_event_record_auth_t *auth_event_dpp = NULL;

	error = blobmsg_parse(client_auth_event_policy, __CLIENT_AUTH_MAX,
			      tb_client_auth_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	for (i = 0; i < __CLIENT_AUTH_MAX; i++) {
		if (!tb_client_auth_event[i]) {
			LOG(INFO,
			    "ubus_collector: could not parse auth event %s policy",
			    client_auth_event_policy[i].name);
			return -1;
		}
	}

	mac_address =
		blobmsg_get_string(tb_client_auth_event[CLIENT_AUTH_STA_MAC]);
	if (!mac_address)
		return -1;

	ssid_temp =
		blobmsg_get_string(tb_client_auth_event[CLIENT_AUTH_AUTH_SSID]);
	ssid_bytes = snprintf(ssid, RADIO_ESSID_LEN + 1, "%s", ssid_temp);
	if (ssid_bytes > RADIO_ESSID_LEN)
		return -1;

	timestamp =
		blobmsg_get_u32(tb_client_auth_event[CLIENT_AUTH_TIMESTAMP]);
	band = blobmsg_get_u8(tb_client_auth_event[CLIENT_AUTH_BAND]);
	auth_status =
		blobmsg_get_u32(tb_client_auth_event[CLIENT_AUTH_AUTH_STATUS]);

	auth_event_dpp = calloc(1, sizeof(dpp_event_record_auth_t));
	memset(auth_event_dpp, 0, sizeof(dpp_event_record_auth_t));

	memcpy(auth_event_dpp->sta_mac, mac_address, MAC_ADDRESS_STRING_LEN);
	memcpy(auth_event_dpp->ssid, ssid, ssid_bytes + 1);
	auth_event_dpp->session_id = session_id;
	auth_event_dpp->timestamp = timestamp;
	auth_event_dpp->band = band;
	auth_event_dpp->auth_status = auth_status;

	if (!ds_dlist_is_empty(&dpp_session->auth_list))
		ds_dlist_init(&dpp_session->auth_list, dpp_event_record_auth_t,
			      node);

	ds_dlist_insert_tail(&dpp_session->auth_list, auth_event_dpp);
	return 0;
}

static int client_assoc_event_cb(struct blob_attr *msg,
				 dpp_event_record_session_t *dpp_session,
				 uint64_t event_session_id)
{
	int error = 0;
	int ssid_bytes = 0;
	struct blob_attr *tb_client_assoc_event[__CLIENT_ASSOC_MAX] = {};
	char *mac_address = NULL;
	radio_essid_t ssid = {};
	char *ssid_temp = NULL;
	uint64_t session_id = event_session_id;
	uint32_t timestamp = 0;
	radio_type_t band = 0;
	assoc_type_t assoc_type = 0;
	uint32_t rssi = 0;
	uint32_t internal_sc = 0;
	bool using11k = false;
	bool using11r = false;
	bool using11v = false;
	int i = 0;
	dpp_event_record_assoc_t *assoc_event_dpp = NULL;

	error = blobmsg_parse(client_assoc_event_policy, __CLIENT_ASSOC_MAX,
			      tb_client_assoc_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	for (i = 0; i < __CLIENT_ASSOC_MAX; i++) {
		if (!tb_client_assoc_event[i]) {
			LOG(INFO,
			    "ubus_collector: could not parse assoc event %s policy",
			    client_assoc_event_policy[i].name);
			return -1;
		}
	}

	mac_address =
		blobmsg_get_string(tb_client_assoc_event[CLIENT_ASSOC_STA_MAC]);
	if (!mac_address)
		return -1;

	ssid_temp =
		blobmsg_get_string(tb_client_assoc_event[CLIENT_ASSOC_SSID]);
	ssid_bytes = snprintf(ssid, RADIO_ESSID_LEN + 1, "%s", ssid_temp);
	if (ssid_bytes > RADIO_ESSID_LEN)
		return -1;

	band = blobmsg_get_u8(tb_client_assoc_event[CLIENT_ASSOC_BAND]);
	assoc_type =
		blobmsg_get_u8(tb_client_assoc_event[CLIENT_ASSOC_ASSOC_TYPE]);
	rssi = blobmsg_get_u32(tb_client_assoc_event[CLIENT_ASSOC_RSSI]);
	timestamp =
		blobmsg_get_u32(tb_client_assoc_event[CLIENT_ASSOC_TIMESTAMP]);
	internal_sc = blobmsg_get_u32(
		tb_client_assoc_event[CLIENT_ASSOC_INTERNAL_SC]);
	using11k =
		blobmsg_get_bool(tb_client_assoc_event[CLIENT_ASSOC_USING11K]);
	using11r =
		blobmsg_get_bool(tb_client_assoc_event[CLIENT_ASSOC_USING11R]);
	using11v =
		blobmsg_get_bool(tb_client_assoc_event[CLIENT_ASSOC_USING11V]);

	assoc_event_dpp = calloc(1, sizeof(dpp_event_record_assoc_t));

	strcpy(assoc_event_dpp->sta_mac, mac_address);
	memcpy(assoc_event_dpp->ssid, ssid, ssid_bytes + 1);
	assoc_event_dpp->session_id = session_id;
	assoc_event_dpp->timestamp = timestamp;
	assoc_event_dpp->band = band;
	assoc_event_dpp->assoc_type = assoc_type;
	assoc_event_dpp->rssi = rssi;
	assoc_event_dpp->internal_sc = internal_sc;
	assoc_event_dpp->using11k = using11k;
	assoc_event_dpp->using11r = using11r;
	assoc_event_dpp->using11v = using11v;

	if (!ds_dlist_is_empty(&dpp_session->assoc_list))
		ds_dlist_init(&dpp_session->assoc_list,
			      dpp_event_record_assoc_t, node);

	ds_dlist_insert_tail(&dpp_session->assoc_list, assoc_event_dpp);
	return 0;
}

static int (*event_handler_list[__CLIENT_EVENTS_MAX - 1])(
	struct blob_attr *msg, dpp_event_record_session_t *dpp_session,
	uint64_t session_id) = {
	client_assoc_event_cb,	    client_auth_event_cb,
	client_disconnect_event_cb, NULL,
	client_first_data_event_cb, NULL,
};

static void ubus_collector_cb(struct ubus_request *req, int type,
			      struct blob_attr *msg)
{
	int error = 0;
	int rem = 0;
	struct blob_attr *tb_sessions[__UBUS_SESSIONS_MAX] = {};
	struct blob_attr *tb_session = NULL;
	struct blob_attr *tb_client_events[__CLIENT_EVENTS_MAX] = {};
	dpp_event_record_t *dpp_client_session = NULL;
	dpp_event_record_session_t *dpp_session = NULL;
	uint64_t session_id = 0;
	int session_no = 0;
	char event_message[128] = {};
	delete_entry_t *delete_entry = NULL;
	char *bss = req->priv;
	int i = 0;
	(void)type;

	if (!msg)
		goto error_out;

	LOG(INFO, "ubus_collector: received ubus collector message");

	error = blobmsg_parse(ubus_collector_sessions_policy,
			      __UBUS_SESSIONS_MAX, tb_sessions,
			      blobmsg_data(msg), blobmsg_data_len(msg));
	if (error || !tb_sessions[UBUS_COLLECTOR_SESSIONS]) {
		LOG(INFO,
		    "ubus_collector: failed while parsing session policy");
		goto error_out;
	}

	dpp_client_session = calloc(1, sizeof(dpp_event_record_t));
	if (!dpp_client_session) {
		LOG(INFO,
		    "ubus_collector: not enough memory for dpp_client_session");
		goto error_out;
	}

	ds_dlist_init(&dpp_client_session->client_session,
		      dpp_event_record_session_t, node);

	/* Iterating on multiple ClientSession types */
	blobmsg_for_each_attr(tb_session, tb_sessions[UBUS_COLLECTOR_SESSIONS],
			      rem)
	{
		dpp_session = calloc(1, sizeof(dpp_event_record_session_t));
		if (!dpp_session) {
			LOG(INFO,
			    "ubus_collector: not enough memory for dpp_session");
			goto error_out;
		}

		error = blobmsg_parse(client_session_events_policy,
				      __CLIENT_EVENTS_MAX, tb_client_events,
				      blobmsg_data(tb_session),
				      blobmsg_data_len(tb_session));
		if (error) {
			LOG(INFO,
			    "ubus_collector: failed while parsing client session events policy");
			goto error_out;
		}

		session_no++;
		LOG(INFO, "ubus_collector: processing session no %d",
		    session_no);

		if (!tb_client_events[CLIENT_SESSION_ID]) {
			LOG(INFO,
			    "ubus_collector: failed while getting client session_id");
			goto error_out;
		}

		session_id =
			blobmsg_get_u64(tb_client_events[CLIENT_SESSION_ID]);
		dpp_session->session_id = session_id;

		for (i = 0; i < __CLIENT_EVENTS_MAX - 1; i++) {
			if (tb_client_events[i]) {
				snprintf(event_message, sizeof(event_message),
					 "%s",
					 client_session_events_policy[i].name);
				LOG(INFO, "ubus_collector: processing %s",
				    event_message);

				if (!event_handler_list[i]) {
					snprintf(
						event_message,
						sizeof(event_message),
						"Event %s - handler not implemented",
						client_session_events_policy[i]
							.name);
					LOG(INFO, "ubus_collector: %s",
					    event_message);
					continue;
				}

				error = event_handler_list[i](
					tb_client_events[i], dpp_session,
					session_id);
				if (error) {
					LOG(INFO,
					    "ubus_collector: failed in event handler");
					goto error_out;
				}
			}
		}

		ds_dlist_insert_tail(&dpp_client_session->client_session,
				     dpp_session);
		dpp_session = NULL;

		/* Schedule session for deletion */
		delete_entry = calloc(1, sizeof(delete_entry_t));
		delete_entry->session_id = session_id;
		delete_entry->bss = bss;

		ds_dlist_insert_tail(&deletion_pending->list,
				     delete_entry);
		delete_entry = NULL;
	}

	/* Move all the sessions received from hostapd to global list accessible from sm_events */
	if (session_no)
		ds_dlist_insert_tail(&g_report_data.list, dpp_client_session);

	goto out;

error_out:
	free(dpp_session);
	free(dpp_client_session);
	return;

out:
	LOG(INFO, "ubus_collector: successfull parse");
}

static void ubus_collector_complete_cb(struct ubus_request *req, int ret)
{
	LOG(INFO, "ubus_collector: finished processing");
	free(req);
	return;
}

static void ubus_collector_hostapd_invoke(void *arg)
{
	uint32_t ubus_object_id = 0;
	const char *object_path = arg;
	const char *hostapd_method = "get_sessions";
	struct ubus_request *req = malloc(sizeof(struct ubus_request));
	

	if (ubus_lookup_id(ubus, object_path, &ubus_object_id)) {
		LOG(INFO,
		    "ubus_collector: could not find ubus object %s", object_path);
		free(req);
		evsched_task_reschedule();
		return;
	}

	LOG(INFO, "ubus_collector: requesting hostapd data");
	ubus_invoke_async(ubus, ubus_object_id, hostapd_method, NULL, req);

	req->data_cb = (ubus_data_handler_t)ubus_collector_cb;
	req->complete_cb = (ubus_complete_handler_t)ubus_collector_complete_cb;
	req->priv = arg;

	ubus_complete_request_async(ubus, req);

	evsched_task_reschedule();
}

static void ubus_collector_bss_cb(struct ubus_request *req, int type,
				  struct blob_attr *msg)
{
	int error = 0;
	int rem = 0;
	struct blob_attr *tb_bss_lst[__BSS_LIST_DATA_MAX] = {};
	struct blob_attr *tb_bss_table[__BSS_TABLE_MAX] = {};
	struct blob_attr *tb_bss_tbl = NULL;
	char *bss_name = NULL;

	if (!msg)
		goto error_out;

	LOG(INFO, "ubus_collector: received ubus collector message");

	error = blobmsg_parse(ubus_collector_bss_list_policy,
			      __BSS_LIST_DATA_MAX, tb_bss_lst,
			      blobmsg_data(msg), blobmsg_data_len(msg));
	if (error || !tb_bss_lst[BSS_LIST_BSS_LIST]) {
		LOG(INFO,
		    "ubus_collector: failed while parsing bss_list policy");
		goto error_out;
	}

	/* itereate bss list */
	blobmsg_for_each_attr(tb_bss_tbl, tb_bss_lst[BSS_LIST_BSS_LIST], rem)
	{
		error = blobmsg_parse(ubus_collector_bss_table_policy,
				      __BSS_TABLE_MAX, tb_bss_table,
				      blobmsg_data(tb_bss_tbl),
				      blobmsg_data_len(tb_bss_tbl));
		if (error) {
			LOG(INFO,
			    "ubus_collector: failed while parsing bss table policy");
			goto error_out;
		}

		bss_name = blobmsg_get_string(tb_bss_table[BSS_TABLE_BSS_NAME]);
		if (!bss_name)
			goto error_out;

		LOG(INFO, "ubus_collector: processing bss %s", bss_name);

		/* get sessions for current bss */
		ubus_collector_hostapd_invoke(strdup(bss_name));
	}

	goto out;

error_out:
	return;

out:
	LOG(INFO, "ubus_collector: successfull parse");
}

static void ubus_collector_hostapd_bss_invoke(void *arg)
{
	uint32_t ubus_object_id = 0;
	const char *object_path = "hostapd";
	const char *hostapd_method = "get_bss_list";
	struct ubus_request *req = malloc(sizeof(struct ubus_request));

	if (ubus_lookup_id(ubus, object_path, &ubus_object_id)) {
		LOG(INFO,
		    "ubus_collector: could not find ubus object %s", object_path);
		free(req);
		evsched_task_reschedule();
		return;
	}

	LOG(INFO, "ubus_collector: requesting hostapd data");

	ubus_invoke_async(ubus, ubus_object_id, hostapd_method, NULL, req);

	req->data_cb = (ubus_data_handler_t)ubus_collector_bss_cb;
	req->complete_cb = (ubus_complete_handler_t)ubus_collector_complete_cb;

	ubus_complete_request_async(ubus, req);

	evsched_task_reschedule();
}

static void ubus_collector_hostapd_clear(uint64_t session_id, char *bss)
{
	uint32_t ubus_object_id = 0;
	const char *object_path = bss;
	const char *hostapd_method = "clear_session";
	struct ubus_request *req = malloc(sizeof(struct ubus_request));

	if (ubus_lookup_id(ubus, object_path, &ubus_object_id)) {
		LOG(INFO,
		    "ubus_collector: could not find the designated ubus object [%s]",
		    object_path);
		free(req);
		return;
	}

	int l = snprintf(NULL, 0, "%lli", session_id);
	char str[l + 1];
	snprintf(str, l + 1, "%lli", session_id);

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "session_id", str);

	LOG(INFO, "ubus_collector: deleting session [%s]", str);
	ubus_invoke_async(ubus, ubus_object_id, hostapd_method, b.head, req);

	req->data_cb = NULL;
	req->complete_cb = (ubus_complete_handler_t)ubus_collector_complete_cb;

	ubus_complete_request_async(ubus, req);
}

static void ubus_garbage_collector(void *arg)
{
	delete_entry_t *delete_entry = NULL;

	if (ds_dlist_is_empty(&deletion_pending->list)) {
		evsched_task_reschedule();
		return;
	}

	/* Remove a single session from the deletion list */
	LOG(INFO, "ubus_collector: garbage collection");

	delete_entry = ds_dlist_head(&deletion_pending->list);
	if (delete_entry) {
		if (delete_entry->session_id)
			ubus_collector_hostapd_clear(delete_entry->session_id, delete_entry->bss);

		ds_dlist_remove_head(&deletion_pending->list);
		free(delete_entry->bss);
		free(delete_entry);
	}

	evsched_task_reschedule();
}

int ubus_collector_init(void)
{
	int sched_status = 0;

	LOG(INFO, "ubus_collector: initializing");

	ubus = ubus_connect(UBUS_SOCKET);
	if (!ubus) {
		LOG(INFO, "ubus_collector: cannot find ubus socket");
		return -1;
	}

	/* Initialize the global events and event deletion lists */
	ds_dlist_init(&g_report_data.list, dpp_event_record_session_t, node);

	deletion_pending = calloc(1, sizeof(dpp_event_record_session_t));
	ds_dlist_init(&deletion_pending->list, delete_entry_t, node);

	/* Schedule an event: invoke hostapd ubus get bss list method  */
	sched_status = evsched_task(&ubus_collector_hostapd_bss_invoke, NULL,
				    EVSCHED_SEC(UBUS_POLLING_DELAY));
	if (sched_status < 1) {
		LOG(INFO, "ubus_collector: failed at task creation, status %d",
		    sched_status);
		return -1;
	}

	/* Schedule an event: clear the hostapd sessions from opensync */
	sched_status = evsched_task(&ubus_garbage_collector, NULL,
				    EVSCHED_SEC(UBUS_GARBAGE_COLLECTION_DELAY));
	if (sched_status < 1) {
		LOG(INFO, "ubus_collector: failed at task creation, status %d",
		    sched_status);
		return -1;
	}

	return 0;
}

void ubus_collector_cleanup(void)
{
	LOG(INFO, "ubus_collector: cleaning up ubus collector");
	ubus_free(ubus);
	free(deletion_pending);
	free(ubus_object);
}
