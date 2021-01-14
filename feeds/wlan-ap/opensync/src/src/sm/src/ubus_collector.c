/* SPDX-License-Identifier: BSD-3-Clause */

#include "ubus_collector.h"
#include <inttypes.h>

/* Global list of events received from hostapd */
dpp_event_report_data_t g_event_report;

/* Internal list of processed events ready to be deleted from hostapd */
static ds_dlist_t deletion_pending_list;
static ds_dlist_t bss_list;
static struct ubus_context *ubus = NULL;

typedef struct {
	uint64_t session_id;
	char bss[UBUS_OBJ_LEN];

	ds_dlist_node_t node;
} delete_entry_t;

typedef struct {
	char obj_name[UBUS_OBJ_LEN];
	ds_dlist_node_t node;
} bss_obj_t;


static const struct blobmsg_policy ubus_collector_bss_list_policy[__BSS_LIST_DATA_MAX] = {
	[BSS_LIST_BSS_LIST] = { .name = "bss_list", .type = BLOBMSG_TYPE_ARRAY },
};

static const struct blobmsg_policy ubus_collector_bss_table_policy[__BSS_TABLE_MAX] = {
	[BSS_OBJECT_NAME] = { .name = "name", .type = BLOBMSG_TYPE_STRING },
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
	[CLIENT_IP_EVENT] = {.name = "ClientIpEvent", .type = BLOBMSG_TYPE_TABLE},
	[CLIENT_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
};

static const struct blobmsg_policy client_assoc_event_policy[__CLIENT_ASSOC_MAX] = {
	[CLIENT_ASSOC_ASSOC_TYPE] = {.name = "assoc_type", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_ASSOC_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT64},
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
	[CLIENT_AUTH_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_AUTH_BAND] = {.name = "band", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_AUTH_AUTH_STATUS] = {.name = "auth_status", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_AUTH_AUTH_SSID] = {.name = "ssid", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_AUTH_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy client_disconnect_event_policy[__CLIENT_DISCONNECT_MAX] = {
	[CLIENT_DISCONNECT_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_DISCONNECT_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_DISCONNECT_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_DISCONNECT_BAND] = {.name = "band", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_DISCONNECT_RSSI] = {.name = "rssi", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_DISCONNECT_INTERNAL_RC] = {.name = "internal_rc", .type = BLOBMSG_TYPE_INT32},
	[CLIENT_DISCONNECT_SSID] = {.name = "ssid", .type = BLOBMSG_TYPE_STRING},
};

static const struct blobmsg_policy client_first_data_event_policy[__CLIENT_FIRST_DATA_MAX] = {
	[CLIENT_FIRST_DATA_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_FIRST_DATA_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_FIRST_DATA_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_FIRST_DATA_TX_TIMESTAMP] = {.name = "fdata_tx_up_ts_in_us", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_FIRST_DATA_RX_TIMESTAMP] = {.name = "fdata_rx_up_ts_in_us", .type = BLOBMSG_TYPE_INT64},
};

static const struct blobmsg_policy client_ip_event_policy[__CLIENT_IP_MAX] = {
	[CLIENT_IP_SESSION_ID] = {.name = "session_id", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_IP_TIMESTAMP] = {.name = "timestamp", .type = BLOBMSG_TYPE_INT64},
	[CLIENT_IP_STA_MAC] = {.name = "sta_mac", .type = BLOBMSG_TYPE_STRING},
	[CLIENT_IP_IP_ADDRESS] = {.name = "ip_address", .type = BLOBMSG_TYPE_STRING},
};

static int frequency_to_channel(int freq)
{
	/* see 802.11-2007 17.3.8.3.2 and Annex J */
	if (freq == 2484)
		return 14;
	else if (freq < 2484)
		return (freq - 2407) / 5;
	else if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	else if (freq <= 45000) /* DMG band lower limit */
		return (freq - 5000) / 5;
	else if (freq >= 58320 && freq <= 64800)
		return (freq - 56160) / 2160;
	else
		return 0;
}

static radio_type_t frequency_to_band(int freq)
{
	int chan = frequency_to_channel(freq);

	if (chan <= 16)
		return RADIO_TYPE_2G;
	else if (chan >= 32 && chan <= 68)
		return RADIO_TYPE_5GL;
	else if (chan >= 96)
		return RADIO_TYPE_5GU;
	else
		return RADIO_TYPE_NONE;
}

static int client_first_data_event_cb(struct blob_attr *msg,
				      dpp_event_record_session_t *dpp_session,
				      uint64_t event_session_id)
{
	int error = 0;
	struct blob_attr
		*tb_client_first_data_event[__CLIENT_FIRST_DATA_MAX] = {};
	char *mac_address = NULL;

	if (NULL == dpp_session)
		return -1;

	error = blobmsg_parse(client_first_data_event_policy,
			      __CLIENT_FIRST_DATA_MAX,
			      tb_client_first_data_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	dpp_session->first_data_event = dpp_event_client_first_data_record_alloc();

	if (!dpp_session->first_data_event)
		return -1;

	dpp_session->first_data_event->session_id = event_session_id;

	if (tb_client_first_data_event[CLIENT_FIRST_DATA_STA_MAC]) {
		mac_address = blobmsg_get_string(tb_client_first_data_event[CLIENT_FIRST_DATA_STA_MAC]);
		memcpy(dpp_session->first_data_event->sta_mac, mac_address, MAC_ADDRESS_STRING_LEN);
	}

	if (tb_client_first_data_event[CLIENT_FIRST_DATA_TIMESTAMP])
		dpp_session->first_data_event->timestamp = blobmsg_get_u64(tb_client_first_data_event[CLIENT_FIRST_DATA_TIMESTAMP]);

	if (tb_client_first_data_event[CLIENT_FIRST_DATA_TX_TIMESTAMP])
		dpp_session->first_data_event->fdata_tx_up_ts_in_us = blobmsg_get_u64(tb_client_first_data_event[CLIENT_FIRST_DATA_TX_TIMESTAMP]);

	if (tb_client_first_data_event[CLIENT_FIRST_DATA_RX_TIMESTAMP])
		dpp_session->first_data_event->fdata_rx_up_ts_in_us = blobmsg_get_u64(tb_client_first_data_event[CLIENT_FIRST_DATA_RX_TIMESTAMP]);

	return 0;
}

static int client_disconnect_event_cb(struct blob_attr *msg,
				      dpp_event_record_session_t *dpp_session,
				      uint64_t event_session_id)
{
	int error = 0;
	struct blob_attr
		*tb_client_disconnect_event[__CLIENT_DISCONNECT_MAX] = {};
	char *mac_address, *ssid = NULL;

	if (NULL == dpp_session)
		return -1;

	error = blobmsg_parse(client_disconnect_event_policy,
			      __CLIENT_DISCONNECT_MAX,
			      tb_client_disconnect_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	dpp_session->disconnect_event = dpp_event_client_disconnect_record_alloc();

	if (!dpp_session->disconnect_event)
		return -1;

	dpp_session->disconnect_event->session_id = event_session_id;

	if (tb_client_disconnect_event[CLIENT_DISCONNECT_STA_MAC]) {
		mac_address = blobmsg_get_string(tb_client_disconnect_event[CLIENT_DISCONNECT_STA_MAC]);
		memcpy(dpp_session->disconnect_event->sta_mac, mac_address, MAC_ADDRESS_STRING_LEN);
	}

	if (tb_client_disconnect_event[CLIENT_DISCONNECT_BAND])
		dpp_session->disconnect_event->band = frequency_to_band(blobmsg_get_u32(tb_client_disconnect_event[CLIENT_DISCONNECT_BAND]));

	if (tb_client_disconnect_event[CLIENT_DISCONNECT_RSSI])
		dpp_session->disconnect_event->rssi = blobmsg_get_u32(tb_client_disconnect_event[CLIENT_DISCONNECT_RSSI]);

	if (tb_client_disconnect_event[CLIENT_DISCONNECT_SSID]) {
		ssid = blobmsg_get_string(tb_client_disconnect_event[CLIENT_DISCONNECT_SSID]);
		memcpy(dpp_session->disconnect_event->ssid, ssid, RADIO_ESSID_LEN + 1);
	}

	if (tb_client_disconnect_event[CLIENT_DISCONNECT_TIMESTAMP])
		dpp_session->disconnect_event->timestamp = blobmsg_get_u64(tb_client_disconnect_event[CLIENT_DISCONNECT_TIMESTAMP]);

	if (tb_client_disconnect_event[CLIENT_DISCONNECT_INTERNAL_RC])
		dpp_session->disconnect_event->internal_rc = blobmsg_get_u32(tb_client_disconnect_event[CLIENT_DISCONNECT_INTERNAL_RC]);

	return 0;
}

static int client_auth_event_cb(struct blob_attr *msg,
				dpp_event_record_session_t *dpp_session,
				uint64_t event_session_id)
{
	int error = 0;
	struct blob_attr *tb_client_auth_event[__CLIENT_ASSOC_MAX] = {};
	char *mac_address, *ssid = NULL;

	if (NULL == dpp_session)
		return -1;

	error = blobmsg_parse(client_auth_event_policy, __CLIENT_AUTH_MAX,
			      tb_client_auth_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	dpp_session->auth_event = dpp_event_client_auth_record_alloc();

	if (!dpp_session->auth_event)
		return -1;

	dpp_session->auth_event->session_id = event_session_id;

	if (tb_client_auth_event[CLIENT_AUTH_STA_MAC]) {
		mac_address = blobmsg_get_string(tb_client_auth_event[CLIENT_AUTH_STA_MAC]);
		memcpy(dpp_session->auth_event->sta_mac, mac_address, MAC_ADDRESS_STRING_LEN);
	}

	if (tb_client_auth_event[CLIENT_AUTH_BAND])
		dpp_session->auth_event->band = frequency_to_band(blobmsg_get_u32(tb_client_auth_event[CLIENT_AUTH_BAND]));

	if (tb_client_auth_event[CLIENT_AUTH_AUTH_SSID]) {
		ssid = blobmsg_get_string(tb_client_auth_event[CLIENT_AUTH_AUTH_SSID]);
		memcpy(dpp_session->auth_event->ssid, ssid, RADIO_ESSID_LEN + 1);
	}

	if (tb_client_auth_event[CLIENT_AUTH_TIMESTAMP])
		dpp_session->auth_event->timestamp = blobmsg_get_u64(tb_client_auth_event[CLIENT_AUTH_TIMESTAMP]);

	if (tb_client_auth_event[CLIENT_AUTH_AUTH_STATUS])
			dpp_session->auth_event->auth_status = blobmsg_get_u32(tb_client_auth_event[CLIENT_AUTH_AUTH_STATUS]);

	return 0;
}

static int client_assoc_event_cb(struct blob_attr *msg,
				 dpp_event_record_session_t *dpp_session,
				 uint64_t event_session_id)
{
	int error = 0;
	struct blob_attr *tb_client_assoc_event[__CLIENT_ASSOC_MAX] = {};
	char *mac_address, *ssid = NULL;

	if (NULL == dpp_session)
		return -1;

	error = blobmsg_parse(client_assoc_event_policy, __CLIENT_ASSOC_MAX,
			      tb_client_assoc_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	dpp_session->assoc_event = dpp_event_client_assoc_record_alloc();

	if (!dpp_session->assoc_event)
		return -1;

	dpp_session->assoc_event->session_id = event_session_id;

	if (tb_client_assoc_event[CLIENT_ASSOC_STA_MAC]) {
		mac_address = blobmsg_get_string(tb_client_assoc_event[CLIENT_ASSOC_STA_MAC]);
		memcpy(dpp_session->assoc_event->sta_mac, mac_address, MAC_ADDRESS_STRING_LEN);
	}

	if (tb_client_assoc_event[CLIENT_ASSOC_SSID]) {
		ssid = blobmsg_get_string(tb_client_assoc_event[CLIENT_ASSOC_SSID]);
		memcpy(dpp_session->assoc_event->ssid, ssid, RADIO_ESSID_LEN + 1);
	}

	if (tb_client_assoc_event[CLIENT_ASSOC_TIMESTAMP])
		dpp_session->assoc_event->timestamp = blobmsg_get_u64(tb_client_assoc_event[CLIENT_ASSOC_TIMESTAMP]);

	if (tb_client_assoc_event[CLIENT_ASSOC_INTERNAL_SC])
		dpp_session->assoc_event->internal_sc = blobmsg_get_u32(tb_client_assoc_event[CLIENT_ASSOC_INTERNAL_SC]);

	if (tb_client_assoc_event[CLIENT_ASSOC_RSSI])
		dpp_session->assoc_event->rssi = blobmsg_get_u32(tb_client_assoc_event[CLIENT_ASSOC_RSSI]);

	if (tb_client_assoc_event[CLIENT_ASSOC_BAND])
		dpp_session->assoc_event->band = frequency_to_band(blobmsg_get_u32(tb_client_assoc_event[CLIENT_ASSOC_BAND]));

	if (tb_client_assoc_event[CLIENT_ASSOC_ASSOC_TYPE])
		dpp_session->assoc_event->assoc_type = blobmsg_get_u8(tb_client_assoc_event[CLIENT_ASSOC_ASSOC_TYPE]);

	if (tb_client_assoc_event[CLIENT_ASSOC_USING11K])
		dpp_session->assoc_event->using11k = blobmsg_get_bool(tb_client_assoc_event[CLIENT_ASSOC_USING11K]);

	if (tb_client_assoc_event[CLIENT_ASSOC_USING11R])
		dpp_session->assoc_event->using11r = blobmsg_get_bool(tb_client_assoc_event[CLIENT_ASSOC_USING11R]);

	if (tb_client_assoc_event[CLIENT_ASSOC_USING11V])
		dpp_session->assoc_event->using11v = blobmsg_get_bool(tb_client_assoc_event[CLIENT_ASSOC_USING11V]);

	return 0;
}

static int client_ip_event_cb(struct blob_attr *msg,
				dpp_event_record_session_t *dpp_session,
				uint64_t event_session_id)
{
	int error = 0;
	struct blob_attr *tb_client_ip_event[__CLIENT_IP_MAX] = {};
	char *mac_address, *ip_address = NULL;

	if (NULL == dpp_session)
		return -1;

	error = blobmsg_parse(client_ip_event_policy, __CLIENT_IP_MAX,
			      tb_client_ip_event, blobmsg_data(msg),
			      blobmsg_data_len(msg));
	if (error)
		return -1;

	dpp_session->ip_event = dpp_event_client_ip_record_alloc();

	if (!dpp_session->ip_event)
		return -1;

	dpp_session->ip_event->session_id = event_session_id;

	if (tb_client_ip_event[CLIENT_IP_STA_MAC]) {
		mac_address = blobmsg_get_string(tb_client_ip_event[CLIENT_IP_STA_MAC]);
		memcpy(dpp_session->ip_event->sta_mac, mac_address, MAC_ADDRESS_STRING_LEN);
	}

	if (tb_client_ip_event[CLIENT_IP_IP_ADDRESS]) {
		ip_address = blobmsg_get_string(tb_client_ip_event[CLIENT_IP_IP_ADDRESS]);
		memcpy(dpp_session->ip_event->ip_addr, ip_address, IP_ADDRESS_STRING_LEN);
	}

	if (tb_client_ip_event[CLIENT_IP_TIMESTAMP])
		dpp_session->ip_event->timestamp = blobmsg_get_u64(tb_client_ip_event[CLIENT_IP_TIMESTAMP]);

	return 0;
}

static int (*client_event_handler_list[__CLIENT_EVENTS_MAX - 1])(
	struct blob_attr *msg, dpp_event_record_session_t *dpp_session,
	uint64_t session_id) = {
	client_assoc_event_cb, client_auth_event_cb,
	client_disconnect_event_cb, NULL,
	client_first_data_event_cb, NULL,
	client_ip_event_cb,
};


static void ubus_collector_complete_session_cb(struct ubus_request *req, int ret)
{
	LOG(DEBUG, "ubus_collector_complete_session_cb");
	if (req)
		free(req);
}

static void ubus_collector_session_cb(struct ubus_request *req, int type,
			      struct blob_attr *msg)
{
	int error = 0;
	int rem = 0;
	struct blob_attr *tb_sessions[__UBUS_SESSIONS_MAX] = {};
	struct blob_attr *tb_session = NULL;
	struct blob_attr *tb_client_events[__CLIENT_EVENTS_MAX] = {};
	dpp_event_record_t *event_record = NULL;
	uint64_t session_id = 0;
	delete_entry_t *delete_entry = NULL;
	int i = 0;
	(void)type;

	char *ifname = (char *)req->priv;

	dpp_event_record_t *old_session = NULL;
	ds_dlist_iter_t session_iter;

	/* First remove all the old sessions from the global report which are already consumed by sm_events */
	for (old_session = ds_dlist_ifirst(&session_iter, &g_event_report.client_event_list); old_session != NULL; old_session = ds_dlist_inext(&session_iter)) {
		if (old_session && old_session->hasSMProcessed) {
			ds_dlist_iremove(&session_iter);
			dpp_event_record_free(old_session);
			old_session = NULL;
		}
	}

	if (!msg)
		return;

	error = blobmsg_parse(ubus_collector_sessions_policy,
			      __UBUS_SESSIONS_MAX, tb_sessions,
			      blobmsg_data(msg), blobmsg_data_len(msg));

	if (error || !tb_sessions[UBUS_COLLECTOR_SESSIONS]) {
		LOG(ERR, "ubus_collector: failed while parsing session policy");
		return;
	}

	blobmsg_for_each_attr(tb_session, tb_sessions[UBUS_COLLECTOR_SESSIONS],
			      rem)
	{
		error = blobmsg_parse(client_session_events_policy,
				      __CLIENT_EVENTS_MAX, tb_client_events,
				      blobmsg_data(tb_session),
				      blobmsg_data_len(tb_session));
		if (error || !tb_client_events[CLIENT_SESSION_ID]) {
			LOG(ERR, "ubus_collector: failed while parsing client session events policy");
			continue;
		}

		event_record = dpp_event_record_alloc();

		if (!event_record) {
			LOG(ERR, "ubus_collector: not enough memory for event_record");
			continue;
		}
		session_id = blobmsg_get_u64(tb_client_events[CLIENT_SESSION_ID]);
		event_record->client_session.session_id = session_id;

		for (i = 0; i < __CLIENT_EVENTS_MAX - 1; i++) {
			if (tb_client_events[i]) {
				if (!client_event_handler_list[i]) {
					LOG(ERR, "ubus_collector: Event handler not implemented");
					continue;
				}
				client_event_handler_list[i](
					tb_client_events[i], &event_record->client_session,
					session_id);
				}
			}
		event_record->hasSMProcessed = false;
		ds_dlist_insert_tail(&g_event_report.client_event_list, event_record);
		event_record = NULL;

		/* Schedule session for deletion */
		delete_entry = calloc(1, sizeof(delete_entry_t));
		if (delete_entry) {
			memset(delete_entry, 0, sizeof(delete_entry_t));
			delete_entry->session_id = session_id;
			strncpy(delete_entry->bss, ifname, UBUS_OBJ_LEN);
			ds_dlist_insert_tail(&deletion_pending_list, delete_entry);
			delete_entry = NULL;
		}
	}
}

static void ubus_collector_hostapd_invoke(void *object_path)
{

	uint32_t ubus_object_id = 0;
	const char *obj_path = object_path;
	const char *hostapd_method = "get_sessions";

	if (NULL == object_path) {
		LOG(ERR, "ubus_collector_hostapd_invoke: Missing hostapd ubus object");
		return;
	}

	struct ubus_request *request = malloc(sizeof(struct ubus_request));

	if (!request) {
		LOG(ERR, "ubus_collector_hostapd_invoke: Failed to allocate ubus request object");
		return;
	}

	if (ubus_lookup_id(ubus, obj_path, &ubus_object_id)) {
		LOG(ERR, "ubus_collector: could not find ubus object %s",
		    obj_path);
		if (request)
			free(request);
		return;
	}

	ubus_invoke_async(ubus, ubus_object_id, hostapd_method, NULL, request);
	request->data_cb = ubus_collector_session_cb;
	request->complete_cb = ubus_collector_complete_session_cb;
	request->priv = object_path;
	ubus_complete_request_async(ubus, request);
}

static void ubus_collector_complete_bss_cb(struct ubus_request *req, int ret)
{
	LOG(DEBUG, "ubus_collector_complete_bss_cb");
	if (req)
		free(req);
}

static void get_sessions(void * arg)
{
	bss_obj_t *bss_record = NULL;
	ds_dlist_iter_t record_iter;

	if (ds_dlist_is_empty(&bss_list)) {
		LOG(NOTICE, "No BSSs to get sessions for");
		evsched_task_reschedule();
		return;
	}

	for (bss_record = ds_dlist_ifirst(&record_iter, &bss_list);
			bss_record != NULL; bss_record = ds_dlist_inext(&record_iter)) {
		ubus_collector_hostapd_invoke(bss_record->obj_name);
	}
	evsched_task_reschedule();
}

static void ubus_collector_bss_cb(struct ubus_request *req, int type,
				  struct blob_attr *msg)
{
	int error = 0;
	int rem = 0;
	struct blob_attr *tb_bss_lst[__BSS_LIST_DATA_MAX] = {};
	struct blob_attr *tb_bss_tbl = NULL;
	bss_obj_t *bss_record = NULL;
	ds_dlist_iter_t record_iter;

	if (!msg)
		return;

	error = blobmsg_parse(ubus_collector_bss_list_policy,
			      __BSS_LIST_DATA_MAX, tb_bss_lst,
			      blobmsg_data(msg), blobmsg_data_len(msg));

	if (error || !tb_bss_lst[BSS_LIST_BSS_LIST]) {
		LOG(ERR,
		    "ubus_collector: failed while parsing bss_list policy");
		return;
	}

	/* iterate bss list */
	blobmsg_for_each_attr(tb_bss_tbl, tb_bss_lst[BSS_LIST_BSS_LIST], rem)
	{
		bool obj_exists = false;
		struct blob_attr *tb_bss_table[__BSS_TABLE_MAX] = {};

		error = blobmsg_parse(ubus_collector_bss_table_policy,
				      __BSS_TABLE_MAX, tb_bss_table,
				      blobmsg_data(tb_bss_tbl),
				      blobmsg_data_len(tb_bss_tbl));
		if (error) {
			LOG(ERR, "ubus_collector_ failed while parsing bss table policy");
			continue;
		}

		if (!tb_bss_table[BSS_OBJECT_NAME])
			continue;

		char *obj_name = blobmsg_get_string(tb_bss_table[BSS_OBJECT_NAME]);
		if (!ds_dlist_is_empty(&bss_list)) {
			for (bss_record = ds_dlist_ifirst(&record_iter, &bss_list);
					bss_record != NULL; bss_record = ds_dlist_inext(&record_iter)) {
				if (!strcmp(obj_name, bss_record->obj_name)) {
					obj_exists = true;
					break;
				}
			}
		}
		if (!obj_exists) {
			bss_record = calloc(1, sizeof(bss_obj_t));
			strncpy(bss_record->obj_name, obj_name, UBUS_OBJ_LEN);
			ds_dlist_insert_tail(&bss_list, bss_record);
		}
	}
}

static void ubus_collector_hostapd_bss_invoke(void *arg)
{
	uint32_t ubus_object_id = 0;

	const char *object_path = "hostapd";
	const char *hostapd_method = "get_bss_list";

	if (ubus_lookup_id(ubus, object_path, &ubus_object_id)) {
		LOG(ERR, "ubus_collector: could not find ubus object %s",
		    object_path);
		evsched_task_reschedule();
		return;
	}

	struct ubus_request *req = malloc(sizeof(struct ubus_request));

	if (!req) {
		LOG(ERR, "Failed to allocate req structure");
		return;
	}

	ubus_invoke_async(ubus, ubus_object_id, hostapd_method, NULL, req);

	req->data_cb = ubus_collector_bss_cb;
	req->complete_cb = ubus_collector_complete_bss_cb;

	ubus_complete_request_async(ubus, req);

	evsched_task_reschedule();
}

static void ubus_collector_hostapd_clear(uint64_t session_id, char *object_path)
{
	uint32_t ubus_object_id = 0;

	if (NULL == object_path)
		return;

	const char *hostapd_method = "clear_session";

	if (ubus_lookup_id(ubus, object_path, &ubus_object_id)) {
		LOG(ERR, "ubus_collector: could not find the designated ubus object [%s]", object_path);
		return;
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u64(&b, "session_id", session_id);

	if (UBUS_STATUS_OK != ubus_invoke(ubus, ubus_object_id, hostapd_method, b.head, NULL, NULL, 1000)) {
		LOG(ERR, "ubus call to clear session failed");
	}

}

static void ubus_garbage_collector(void *arg)
{
	delete_entry_t *delete_entry = NULL;

	if (ds_dlist_is_empty(&deletion_pending_list)) {
		evsched_task_reschedule();
		return;
	}

	/* Remove a single session from the deletion list */

	delete_entry = ds_dlist_head(&deletion_pending_list);
	if (delete_entry) {
		if (delete_entry->session_id) {
			ubus_collector_hostapd_clear(delete_entry->session_id,
						delete_entry->bss);
		}
		ds_dlist_remove_head(&deletion_pending_list);
		free(delete_entry);
		delete_entry = NULL;
	}

	evsched_task_reschedule();
}

int ubus_collector_init(void)
{
	int sched_status = 0;

	ubus = ubus_connect(UBUS_SOCKET);
	if (!ubus) {
		LOG(ERR, "ubus_collector: cannot find ubus socket");
		return -1;
	}

	/* Initialize the global events, session deletion and bss object lists */
	ds_dlist_init(&g_event_report.client_event_list, dpp_event_record_t, node);
	ds_dlist_init(&deletion_pending_list, delete_entry_t, node);
	ds_dlist_init(&bss_list, bss_obj_t, node);

	/* Schedule an event: invoke hostapd ubus get bss list method  */
	sched_status = evsched_task(&ubus_collector_hostapd_bss_invoke, NULL,
				    EVSCHED_SEC(UBUS_BSS_POLLING_DELAY));
	if (sched_status < 1) {
		LOG(ERR, "ubus_collector: failed at task creation, status %d",
		    sched_status);
		return -1;
	}

	/* Schedule an event: get sessions for all the BSSs  */
	sched_status = evsched_task(&get_sessions, NULL,
				    EVSCHED_SEC(UBUS_SESSIONS_POLLING_DELAY));
	if (sched_status < 1) {
		LOG(ERR, "ubus_collector: failed at task creation, status %d",
			sched_status);
		return -1;
	}

	/* Schedule an event: clear the hostapd sessions from opensync */
	sched_status = evsched_task(&ubus_garbage_collector, NULL,
				    EVSCHED_SEC(UBUS_GARBAGE_COLLECTION_DELAY));
	if (sched_status < 1) {
		LOG(ERR, "ubus_collector: failed at task creation, status %d",
		    sched_status);
		return -1;
	}

	return 0;
}

void ubus_collector_cleanup(void)
{
	ubus_free(ubus);
}
