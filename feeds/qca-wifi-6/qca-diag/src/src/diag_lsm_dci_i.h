#ifndef DIAG_LSM_DCI_I_H
#define DIAG_LSM_DCI_I_H

#ifdef __cplusplus
extern "C"
{
#endif
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
# Copyright (c) 2014,2016 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

		Diag Consumer Interface (DCI) Internal Header

GENERAL DESCRIPTION:
Headers specific to DCI. This is an internal file used by Diag.

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

			EDIT HISTORY FOR MODULE
$Header:

when       who    what, where, why
--------   ---    ----------------------------------------------------------
12/13/13   RA     Created File
===========================================================================*/

#ifdef USE_GLIB
#include <pthread.h>
#endif
#include "../include/diag_lsm_dci.h"

#define DCI_PKT_RSP_TYPE	0
#define DCI_LOG_TYPE		-1
#define DCI_EVENT_TYPE		-2
#define DCI_EXT_HDR_TYPE	-3

#define DCI_PKT_RSP_CODE	0x93
#define DCI_DELAYED_RSP_CODE	0x94

#define INVALID_DCI_CLIENT	-1
#define DCI_VERSION		1

extern int num_dci_proc;
#define IS_VALID_DCI_PROC(x)	(((x < DIAG_PROC_MSM) || (x >= num_dci_proc)) ? 0 : 1)

/*
 * Maximum size of the packet that can be written to the kernel. All DCI data
 * transactions must be restricted to this size (including DCI headers).
 */
#define DCI_MAX_REQ_BUF_SIZE	(16*1024)

/*
 * Structure to maintain information about every request sent through DCI
 *
 * @uid: unique identifier for the request
 * @func_ptr: callback function to be called on receiving a response
 * @rsp_ptr: pointer to hold the response
 * @rsp_len: length of the response
 * @data_ptr: Additional data specified by the client
 */
typedef struct {
	int uid;
	void (*func_ptr)(unsigned char *, int len, void *data_ptr);
	unsigned char *rsp_ptr;
	int rsp_len;
	void *data_ptr;
} diag_pkt_tracking_info;

/*
 * Structure to maintain a list of all requests sent by the client for bookkeeping
 * purposed. The table is keyed by the UID.
 *
 * @info: pointer to the current request entry info struct
 * @prev: pointer to the previous element in the list
 * @next: pointer to the next element in the list
 */
typedef struct diag_pkt_rsp_tracking_tbl {
	diag_pkt_tracking_info *info;
	struct diag_pkt_rsp_tracking_tbl *prev;
	struct diag_pkt_rsp_tracking_tbl *next;
} diag_pkt_rsp_tracking_tbl;


/*
 * Structure to support Local DCI packets. Defines the header
 * format for DCI packets
 *
 * @start: The start byte
 * @version: DCI version
 * @length: Length of the DCI packet
 * @code: DCI Request Packet Code
 * @tag: DCI Request Tag
 */
typedef PACK(struct) {
	uint8 start;
	uint8 version;
	uint16 length;
	uint8 code;
	uint32 tag;
} diag_dci_pkt_header_t;

/*
 * Structure to support Local DCI Delayed response packets.
 * Defines the header format of DCI Delayed Response Packets.
 *
 * @delayed_rsp_id: Delayed response ID of the delayed response sequence
 * @dci_tag: Unique Tag for the DCI request
 * @next: points to the next element in the list
 * @prev: points to the prev element in the list
 */
typedef PACK(struct) diag_dci_delayed_rsp_tbl_t{
	int delayed_rsp_id;
	int dci_tag;
	struct diag_dci_delayed_rsp_tbl_t *next;
	struct diag_dci_delayed_rsp_tbl_t *prev;
} diag_dci_delayed_rsp_tbl_t;

/*
 * Structre used to query Log or Event mask of the client.
 *
 * @client_id: Unique id for the client
 * @code: log code or event id that is to be queried
 * @is_set: pointer that will contain the result - 1 if the code is set; 0 otherwise
 */
PACK(struct) diag_log_event_stats {
	int client_id;
	uint16 code;
	int is_set;
};

/*
 * This holds registration information about the client. This struct is passed
 * betweeen the kernel and user space.
 *
 * @client_id: Unique id for the client
 * @notification_list: Bit mask of Channels to notify the client on channel change
 * @signal_type: Signal to raise when there is a channel change
 * @token: remote processor identifier
 */
PACK(struct) diag_dci_reg_tbl_t {
	int client_id;
	uint16 notification_list;
	int signal_type;
	int token;
};

/*
 * Structure to hold DCI client information
 *
 * @client_info: basic information used for registration
 * @func_ptr_logs: callback function for streaming logs
 * @func_ptr_events: callback function for streaming events
 * @data_signal_flag: flag for signaling client whenever there is incoming data
 * @data_signal_type: signal to be fired when there in incoming data
 * @req_tbl_head: list of all the active command requests
 * @req_tbl_mutex: mutex to protect request table list
 * @dci_req_buf: temporary buffer to store the requests to kernel
 */
struct diag_dci_client_tbl {
	struct diag_dci_reg_tbl_t client_info;
	void (*func_ptr_logs)(unsigned char *, int len);
	void (*func_ptr_events)(unsigned char *, int len);
	int data_signal_flag;
	int data_signal_type;
	int version;
	struct diag_pkt_rsp_tracking_tbl req_tbl_head;
	pthread_mutex_t req_tbl_mutex;
	unsigned char *dci_req_buf;
};

extern struct diag_dci_client_tbl *dci_client_tbl;

/*
 * Header for DCI request packets
 *
 * @start: DCI data type identifier
 * @uid: DCI request transaction id
 * @client_id: unique id of the client
 */
PACK(struct) diag_dci_req_header_t {
	int start;
	int uid;
	int client_id;
};

/*
 * Header for DCI Log and Event Mask updates
 *
 * @start: DCI data type identifier
 * @type: Determines if it a log mask or an event mask
 * @client_id: unique id of the client
 * @set_flag: flag to enable or disable the log/event codes
 * @count: number of log codes or event ids
 */
PACK(struct) diag_dci_stream_header_t {
	int start;
	int type;
	int client_id;
	int set_flag;
	int count;
};

/*
 * Structure to query the health of a particular peripheral
 *
 * @health: pointer to the health statistics structure
 * @proc: peripheral that the client is interested in
 */
PACK(struct) diag_dci_health_stats_proc {
	int client_id;
	struct diag_dci_health_stats health;
	int proc;
};

/*
 * Structure to query the list of peripherals supporting DCI
 *
 * @proc: processor that the client is interested in
 * @list: bit mask for supported peripherals
*/
PACK(struct) diag_dci_peripheral_list_t {
	int proc;
	diag_dci_peripherals list;
};

/* Internal function used by LSM to find transaction related information */
void lookup_pkt_rsp_transaction(unsigned char *ptr, int proc);

int diag_lsm_dci_init(void);

void diag_lsm_dci_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_LSM_DCI_I_H */
