#ifndef DIAG_LSM_DCI_H
#define DIAG_LSM_DCI_H

#ifdef __cplusplus
extern "C"
{
#endif
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
# Copyright (c) 2012-2013, 2015-2016 by Qualcomm Technologies, Inc.
# All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

	  Diag Consumer Interface (DCI)

GENERAL DESCRIPTION

Headers specific to DCI.

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

	    EDIT HISTORY FOR MODULE

$Header:

when       who    what, where, why
--------   ---    ----------------------------------------------------------
10/09/12   RA     Added Interface for DCI I/O
03/20/12   SJ     Created
===========================================================================*/

/*strlcpy is from OpenBSD and not supported by Meego.
GNU has an equivalent g_strlcpy implementation into glib.
Featurized with compile time USE_GLIB flag for Meego builds.*/
#ifdef USE_GLIB
#define strlcpy g_strlcpy
#define strlcat g_strlcat
#include <pthread.h>
#include <signal.h>
#endif

/* This is a bit mask used for peripheral list. */
typedef uint16 diag_dci_peripherals;

#define ENABLE			1
#define DISABLE			0
#define IN_BUF_SIZE		16384

#define DIAG_INVALID_SIGNAL	0

#define DIAG_PROC_MSM		0
#define DIAG_PROC_MDM		1

/* This is used for health stats */
struct diag_dci_health_stats {
	int dropped_logs;
	int dropped_events;
	int received_logs;
	int received_events;
	int reset_status;
};

/* List of possible error codes returned during DCI transaction */
enum {
	DIAG_DCI_NO_ERROR = 1001,	/* No error */
	DIAG_DCI_NO_REG,		/* Could not register */
	DIAG_DCI_NO_MEM,		/* Failed memory allocation */
	DIAG_DCI_NOT_SUPPORTED,		/* This particular client is not supported */
	DIAG_DCI_HUGE_PACKET,		/* Request/Response Packet too huge */
	DIAG_DCI_SEND_DATA_FAIL,	/* writing to kernel or remote peripheral fails */
	DIAG_DCI_ERR_DEREG,		/* Error while de-registering */
	DIAG_DCI_PARAM_FAIL,		/* Incorrect Parameter */
	DIAG_DCI_DUP_CLIENT		/* Client already exists for this proc */
} diag_dci_error_type;

/* ---------------------------------------------------------------------------
			External functions
 --------------------------------------------------------------------------- */

/* Initialization function required for DCI functions. Input parameters are:
   a) pointer to an int which holds client id
   b) pointer to a bit mask which holds peripheral information,
   c) an integer to specify which processor to talk to (Local or Remote in the case of Fusion Devices),
   d) void* for future needs (not implemented as of now) */
int diag_register_dci_client(int *, diag_dci_peripherals *, int, void *);

/* This API provides information about the peripherals that supports
   DCI in the local processor. Input parameters are:
   a) pointer to a diag_dci_peripherals variable to store the bit mask

   DEPRECATED - Use diag_get_dci_support_list_proc instead. */
int diag_get_dci_support_list(diag_dci_peripherals *);

/* This API provides information about the peripherals that support
   DCI in a given processor. Input Parameters are:
   a) processor id
   b) pointer to a diag_dci_peripherals variable to store the bit mask */
int diag_get_dci_support_list_proc(int proc, diag_dci_peripherals *list);

/* Version handshaking function. This will get the dci version of diag lsm */
int diag_dci_get_version(void);

/* Version handshaking function. This will register the version the client is using */
int diag_dci_set_version(int proc, int version);

/* Main command to send the DCI request. Input parameters are:
   a) client ID generate earlier
   b) request buffer
   c) request buffer length
   d) response buffer
   e) response buffer length
   f) call back function pointer
   g) data pointer */
int diag_send_dci_async_req(int client_id, unsigned char buf[], int bytes, unsigned char *rsp_ptr, int rsp_len,
			    void (*func_ptr)(unsigned char *ptr, int len, void *data_ptr), void *data_ptr);

/* Closes DCI connection for this client. The client needs to pass a pointer
   to the client id generated earlier */
int diag_release_dci_client(int *);

/* Used to set up log streaming to the client. This will send an array of log codes, which are desired
   by client. Input parameters are:
   1. Client ID
   2. Boolean value telling to set or disable logs specified later
   3. Array of log codes
   4. Number of log codes specified in argument 3
   */
int diag_log_stream_config(int client_id, int set_mask, uint16 log_codes_array[], int num_codes);

/* Initialization function required for DCI streaming. Input parameters are:
   call back function pointers.

   DEPRECATED - Use diag_register_dci_stream_proc instead */
int diag_register_dci_stream(void (*func_ptr_logs)(unsigned char *ptr, int len),
			     void (*func_ptr_events)(unsigned char *ptr, int len));

int diag_register_dci_stream_proc(int client_id,
				  void(*func_ptr_logs)(unsigned char *ptr, int len),
				  void(*func_ptr_events)(unsigned char *ptr, int len));

/* Used to set up event streaming to the client. This will send an array of event ids, which are desired
   by client. Input parameters are:
   1. Client ID
   2. Boolean value telling to set or disable event specified later
   3. Array of event id
   4. Number of event ids specified in argument 3
   */
int diag_event_stream_config(int client_id, int set_mask, int event_id_array[], int num_codes);

/* Used to query DCI statistics on all processors for logs & events.

   DEPRECATED - Use diag_get_health_stats_proc instead */
int diag_get_health_stats(struct diag_dci_health_stats *dci_health);

/* Used to query DCI statistics on a specific processor for logs & events */
int diag_get_health_stats_proc(int client_id, struct diag_dci_health_stats *dci_health, int proc);

/* Queries a given Log Code to check if it is enabled or not. Input parameters are:
   1. Client ID
   2. Log Code that needs to be checked
   3. Pointer to boolean to store the result */
int diag_get_log_status(int client_id, uint16 log_code, boolean *value);

/* Queries a given Event ID to check if it is enabled or not. Input parameters are:
   1. Client ID
   2. Event ID that needs to be checked
   3. Pointer to boolean to store the result */
int diag_get_event_status(int client_id, uint16 event_id, boolean *value);

/* Disables all the Log Codes for a given client. The Input parameters are:
   1. Client ID */
int diag_disable_all_logs(int client_id);

/* Disables all the Event ID for a given client. The Input parameters are:
   1. Client ID */
int diag_disable_all_events(int client_id);

/* Votes for real time or non real time mode. The Input paramters are:
   1. Client ID
   2. The desired mode - MODE_REALTIME or MODE_NONREALTIME */
int diag_dci_vote_real_time(int client_id, int real_time);

/* Gets the current mode (Real time or Non Real Time ) Diag is in.
   The Input parameters are:
   1. A pointer to an integer that will hold the result */
int diag_dci_get_real_time_status(int *real_time);

/* Gets the current mode (Real time or Non Real Time ) Diag is
   in, in the requested processor. The Input parameters are:
   1. processor the client is interested in
   2. A pointer to an integer that will hold the result*/
int diag_dci_get_real_time_status_proc(int proc, int *real_time);

/* Registers a signal to be fired on receiving DCI data from
   the peripherals. The input parameters are:
   1. Client ID
   2. Signal Type */
int diag_register_dci_signal_data(int client_id, int signal_type);

/* Disables the signal that fires on receiving DCI data from
   the peripherals. The input parameters are:
   1. Client ID
   2. Signal Type */
int diag_deregister_dci_signal_data(int client_id);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_LSM_DCI_H */

