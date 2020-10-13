/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef DPP_EVENTS_H_INCLUDED
#define DPP_EVENTS_H_INCLUDED

#include "ds.h"
#include "ds_dlist.h"

#include "dpp_types.h"

/* string limits */
#define DPP_REASON_STR_LEN 129
#define DPP_CLT_ID_LEN 129
#define MAC_ADDRESS_STRING_LEN 17

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
typedef enum { AT_ASSOC = 0, AT_REASSOC } assoc_type_t;

/* proto: DeviceType */
typedef enum { DT_AP = 0, DT_STA } device_type_t;

/* proto: FrameType */
typedef enum { FT_DEAUTH = 0, FT_DISASSOC } frame_type_t;

/* proto: CTReasonType */
typedef enum { CTR_IDLE_TOO_LONG = 0, CTR_PROBE_FAIL } ct_reason_t;

/* proto: SecurityType */
typedef enum { SEC_OPEN = 0, SEC_RADIUS, SEC_PSK } sec_type_t;

/* proto: ClientAssocEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	radio_essid_t ssid;
	uint32_t timestamp;
	radio_type_t band;
	assoc_type_t assoc_type;
	uint32_t status;
	int32_t rssi;
	uint32_t internal_sc;
	bool using11k;
	bool using11r;
	bool using11v;

	ds_dlist_node_t node;
} dpp_event_record_assoc_t;

/* proto: ClientAuthEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	uint32_t timestamp;
	radio_essid_t ssid;
	radio_type_t band;
	uint32_t auth_status;

	ds_dlist_node_t node;
} dpp_event_record_auth_t;

/* proto: ClientDisconnectEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	uint32_t timestamp;
	uint32_t reason;
	device_type_t dev_type;
	frame_type_t fr_type;
	uint64_t last_sent_up_ts_in_us;
	uint64_t last_recv_up_ts_in_us;
	uint32_t internal_rc;
	int32_t rssi;
	radio_essid_t ssid;
	radio_type_t band;

	ds_dlist_node_t node;
} dpp_event_record_disconnect_t;

/* proto: ClientConnectEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	uint32_t timestamp;
	radio_type_t band;
	assoc_type_t assoc_type;
	radio_essid_t ssid;
	sec_type_t sec_type;
	bool fbt_used;
	uint8_t ip_addr[16];
	char clt_id[DPP_CLT_ID_LEN];
	int64_t ev_time_bootup_in_us_auth;
	int64_t ev_time_bootup_in_us_assoc;
	int64_t ev_time_bootup_in_us_eapol;
	int64_t ev_time_bootup_in_us_port_enable;
	int64_t ev_time_bootup_in_us_first_rx;
	int64_t ev_time_bootup_in_us_first_tx;
	bool using11k;
	bool using11r;
	bool using11v;
	int64_t ev_time_bootup_in_us_ip;
	int32_t assoc_rssi;

	ds_dlist_node_t node;
} dpp_event_record_connect_t;

/* proto: ClientFailureEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	uint32_t timestamp;
	radio_essid_t ssid;
	int32_t reason;
	char reason_str[DPP_REASON_STR_LEN];

	ds_dlist_node_t node;
} dpp_event_record_failure_t;

/* proto: ClientFirstDataEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	uint32_t timestamp;
	uint64_t fdata_tx_up_ts_in_us;
	uint64_t fdata_rx_up_ts_in_us;

	ds_dlist_node_t node;
} dpp_event_record_first_data_t;

/* proto: ClientIdEvent */
typedef struct {
	char __barrier[46];
	mac_address_t clt_mac;
	uint64_t session_id;
	char clt_id[DPP_CLT_ID_LEN];

	ds_dlist_node_t node;
} dpp_event_record_id_t;

/* proto: ClientIpEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	uint32_t timestamp;
	uint8_t ip_addr[16];

	ds_dlist_node_t node;
} dpp_event_record_ip_t;

/* proto: ClientTimeoutEvent */
typedef struct {
	char __barrier[46];
	char sta_mac[MAC_ADDRESS_STRING_LEN + 1];
	uint64_t session_id;
	uint32_t timestamp;
	ct_reason_t r_code;
	uint64_t last_sent_up_ts_in_us;
	uint64_t last_recv_up_ts_in_us;

	ds_dlist_node_t node;
} dpp_event_record_timeout_t;

/* proto: ClientSession */
typedef struct {
	uint64_t session_id;
	ds_dlist_t assoc_list; /* dpp_event_record_assoc_t */
	ds_dlist_t auth_list; /* dpp_event_record_auth_t */
	ds_dlist_t disconnect_list; /* dpp_event_record_disconnect_t */
	ds_dlist_t failure_list; /* dpp_event_record_failure_t */
	ds_dlist_t first_data_list; /* dpp_event_record_first_data_t */
	ds_dlist_t id_list; /* dpp_event_record_id_t */
	ds_dlist_t ip_list; /* dpp_event_record_ip_t */
	ds_dlist_t timeout_list; /* dpp_event_record_timeout_t */
	ds_dlist_t connect_list; /* dpp_event_record_connect_t */

	ds_dlist_node_t node;
} dpp_event_record_session_t;

/* event record */
typedef struct {
	ds_dlist_t client_session; /* dpp_event_record_session_t */

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

/************************************/
/* ClientConnectEvent alloc/free */
/************************************/
/* alloc */
static inline dpp_event_record_connect_t *
dpp_event_client_connect_record_alloc()
{
	dpp_event_record_connect_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_connect_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_connect_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_connect_record_free(dpp_event_record_connect_t *record)
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

/*********************************/
/* ClientSession alloc/free */
/*********************************/
/* alloc */
static inline dpp_event_record_session_t *
dpp_event_client_session_record_alloc()
{
	dpp_event_record_session_t *record = NULL;

	record = malloc(sizeof(dpp_event_record_session_t));
	if (record) {
		memset(record, 0, sizeof(dpp_event_record_session_t));
	}
	return record;
}

/* free */
static inline void
dpp_event_client_session_record_free(dpp_event_record_session_t *record)
{
	if (NULL != record) {
		free(record);
	}
}

/* Events report type */
typedef struct {
	ds_dlist_t list; /* dpp_event_record_t */
} dpp_event_report_data_t;

#endif /* DPP_EVENTS_H_INCLUDED */
