/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef DPP_EVENTS_H_INCLUDED
#define DPP_EVENTS_H_INCLUDED

#include "ds.h"
#include "ds_dlist.h"

#include "dpp_types.h"

/* string limits */
#define DPP_REASON_STR_LEN 129
#define DPP_CLT_ID_LEN 129

/* proto: EventType */
typedef enum {
	ET_CLIENT_ASSOC = 0,
	ET_CLIENT_AUTH,
	ET_CLIENT_DISCONNECT,
	ET_CLIENT_FAILURE,
	ET_CLIENT_FIRST_DATA,
	ET_CLIENT_ID,
	ET_CLIENT_IP,
	ET_CLIENT_TIMEOUT
} event_type_t;

/* proto: AssocType */
typedef enum { 
	AT_ASSOC = 0, 
	AT_REASSOC 
} assoc_type_t;

/* proto: DeviceType */
typedef enum { 
	DT_AP = 0, 
	DT_STA 
} device_type_t;

/* proto: FrameType */
typedef enum { 
	FT_DEAUTH = 0, 
	FT_DISASSOC 
} frame_type_t;

/* proto: CTReasonType */
typedef enum { 
	CTR_IDLE_TOO_LONG = 0, 
	CTR_PROBE_FAIL 
} ct_reason_t;

/* proto: ClientAssocEvent */
typedef struct {
	mac_address_t sta_mac;
	uint64_t session_id;
	radio_essid_t ssid;
	radio_type_t band;
	assoc_type_t assoc_type;
	uint32_t status;
	int32_t rssi;
	uint32_t internal_sc;
	bool using11k;
	bool using11r;
	bool using11v;
} dpp_event_record_assoc_t;

/* proto: ClientAuthEvent */
typedef struct {
	mac_address_t sta_mac;
	uint64_t session_id;
	radio_essid_t ssid;
	radio_type_t band;
	uint32_t auth_status;
} dpp_event_record_auth_t;

/* proto: ClientDisconnectEvent */
typedef struct {
	mac_address_t sta_mac;
	uint64_t session_id;
	uint32_t reason;
	device_type_t dev_type;
	frame_type_t fr_type;
	uint64_t last_sent_up_ts_in_us;
	uint64_t last_recv_up_ts_in_us;
	uint32_t internal_rc;
	int32_t rssi;
	radio_essid_t ssid;
	radio_type_t band;
} dpp_event_record_disconnect_t;

/* proto: ClientFailureEvent */
typedef struct {
	mac_address_t sta_mac;
	uint64_t session_id;
	radio_essid_t ssid;
	int32_t reason;
	char reason_str[DPP_REASON_STR_LEN];
} dpp_event_record_failure_t;

/* proto: ClientFirstDataEvent */
typedef struct {
	mac_address_t sta_mac;
	uint64_t session_id;
	uint64_t first_data_tx_up_ts_in_us;
	uint64_t first_data_rx_up_ts_in_us;
} dpp_event_record_first_data_t;

/* proto: ClientIdEvent */
typedef struct {
	mac_address_t clt_mac;
	uint64_t session_id;
	char clt_id[DPP_CLT_ID_LEN];
} dpp_event_record_id_t;

/* proto: ClientIpEvent */
typedef struct {
	mac_address_t sta_mac;
	uint64_t session_id;
	char ip_addr[16];
} dpp_event_record_ip_t;

/* proto: ClientTimeoutEvent */
typedef struct {
	mac_address_t sta_mac;
	uint64_t session_id;
	ct_reason_t r_code;
	uint64_t last_sent_up_ts_in_us;
	uint64_t last_recv_up_ts_in_us;

} dpp_event_record_timeout_t;

/* event record */
typedef struct {
	event_type_t event_type;
	dpp_event_record_assoc_t client_assoc_event;
	dpp_event_record_auth_t client_auth_event;
	dpp_event_record_disconnect_t client_disconnect_event;
	dpp_event_record_failure_t client_failure_event;
	dpp_event_record_first_data_t client_first_data_event;
	dpp_event_record_id_t client_id_event;
	dpp_event_record_ip_t client_ip_event;
	dpp_event_record_timeout_t client_timeout_event;
	ds_dlist_node_t node;
} dpp_event_record_t;

/*******************************/
/* ClientAssocEvent alloc/free */
/*******************************/
/* alloc */
static inline dpp_event_record_assoc_t *dpp_event_client_assoc_record_alloc()
{
	dpp_event_record_assoc_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_assoc_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_assoc_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_assoc_record_free(dpp_event_record_assoc_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/******************************/
/* ClientAuthEvent alloc/free */
/******************************/
/* alloc */
static inline dpp_event_record_auth_t *dpp_event_client_auth_record_alloc()
{
	dpp_event_record_auth_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_auth_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_auth_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_auth_record_free(dpp_event_record_auth_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/************************************/
/* ClientDisconnectEvent alloc/free */
/************************************/
/* alloc */
static inline dpp_event_record_disconnect_t *
dpp_event_client_disconnect_record_alloc()
{
	dpp_event_record_disconnect_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_disconnect_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_disconnect_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_disconnect_record_free(dpp_event_record_disconnect_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/*********************************/
/* ClientFailureEvent alloc/free */
/*********************************/
/* alloc */
static inline dpp_event_record_failure_t *
dpp_event_client_failure_record_alloc()
{
	dpp_event_record_failure_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_failure_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_failure_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_failure_record_free(dpp_event_record_failure_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/***********************************/
/* ClientFirstDataEvent alloc/free */
/***********************************/
/* alloc */
static inline dpp_event_record_first_data_t *
dpp_event_client_first_data_record_alloc()
{
	dpp_event_record_first_data_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_first_data_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_first_data_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_first_data_record_free(dpp_event_record_first_data_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/****************************/
/* ClientIdEvent alloc/free */
/****************************/
/* alloc */
static inline dpp_event_record_id_t *dpp_event_client_id_record_alloc()
{
	dpp_event_record_id_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_id_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_id_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_id_record_free(dpp_event_record_id_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/****************************/
/* ClientIpEvent alloc/free */
/****************************/
/* alloc */
static inline dpp_event_record_ip_t *dpp_event_client_ip_record_alloc()
{
	dpp_event_record_ip_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_ip_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_ip_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_ip_record_free(dpp_event_record_ip_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/*********************************/
/* ClientTimeoutEvent alloc/free */
/*********************************/
/* alloc */
static inline dpp_event_record_timeout_t *
dpp_event_client_timeout_record_alloc()
{
	dpp_event_record_timeout_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_timeout_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_timeout_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_timeout_record_free(dpp_event_record_timeout_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/* Events report type */
typedef struct {
	uint64_t timestamp_ms;
	ds_dlist_t list; /* dpp_event_record_t */
} dpp_event_report_data_t;

#endif /* DPP_EVENTS_H_INCLUDED */
