/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
# Copyright (c) 2012-2014,2016 Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

              Diag Consumer Interface (DCI)

GENERAL DESCRIPTION

Implementation of functions specific to DCI.

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

$Header:

when       who    what, where, why
--------   ---    ----------------------------------------------------------
10/08/12   RA     Interface Implementation for DCI I/O
03/20/12   SJ     Created
===========================================================================*/

#include <stdlib.h>
#include "comdef.h"
#include "stdio.h"
#include "diag_lsmi.h"
#include "./../include/diag_lsm.h"
#include "diagsvc_malloc.h"
#include "diag_lsm_event_i.h"
#include "diag_lsm_log_i.h"
#include "diag_lsm_msg_i.h"
#include "diag.h" /* For definition of diag_cmd_rsp */
#include "diag_lsm_pkt_i.h"
#include "diag_lsm_dci_i.h"
#include "diag_lsm_dci.h"
#include "diag_shared_i.h" /* For different constants */
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "errno.h"
#include <pthread.h>
#include <stdint.h>
#include <eventi.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>

int dci_transaction_id;
int num_dci_proc;
struct diag_dci_client_tbl *dci_client_tbl;

static inline int diag_dci_get_proc(int client_id)
{
	int i;
	for (i = 0; i < num_dci_proc && dci_client_tbl; i++)
		if (dci_client_tbl[i].client_info.client_id == client_id)
			return i;

	return -1;
}

int diag_lsm_dci_init(void)
{
	int i, num_remote_proc = 0, success = 0;
	uint16 remote_proc = 0;
	struct diag_dci_client_tbl *temp = NULL;

	success = diag_has_remote_device(&remote_proc);
	if (success) {
		for (num_remote_proc = 0; remote_proc; num_remote_proc++)
			remote_proc &= remote_proc - 1;
	} else {
		DIAG_LOGE("diag: Unable to get remote processor info. Continuing with just the local processor\n");
	}

	num_dci_proc = num_remote_proc + 1; /* Add 1 for Local processor */
	dci_client_tbl = (struct diag_dci_client_tbl *)malloc(num_dci_proc * sizeof(struct diag_dci_client_tbl));
	if (!dci_client_tbl)
		return DIAG_DCI_NO_MEM;

	dci_transaction_id = 0;
	for (i = 0; i < num_dci_proc; i++) {
		temp = &dci_client_tbl[i];
		temp->dci_req_buf = NULL;
		diag_pkt_rsp_tracking_tbl *head = &temp->req_tbl_head;
		head->next = head;
		head->prev = head;
		head->info = NULL;
		pthread_mutex_init(&(dci_client_tbl[i].req_tbl_mutex), NULL);
		temp->client_info.notification_list = 0;
		temp->client_info.signal_type = 0;
		temp->client_info.token = i;
		temp->client_info.client_id = INVALID_DCI_CLIENT;
		temp->data_signal_flag = DISABLE;
		temp->data_signal_type = DIAG_INVALID_SIGNAL;
		temp->func_ptr_logs = (void *)NULL;
		temp->func_ptr_events = (void *)NULL;
		temp->version = 0;
		pthread_mutex_init(&temp->req_tbl_mutex, NULL);
	}

	return DIAG_DCI_NO_ERROR;
}

void diag_lsm_dci_deinit(void)
{
	if (dci_client_tbl) {
		free(dci_client_tbl);
		dci_client_tbl = NULL;
	}
}

int diag_dci_get_dci_version(void)
{
	return DCI_VERSION;
}

int diag_dci_set_version(int client_id, int version)
{
	int proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_PARAM_FAIL;

	if (!dci_client_tbl) {
		DIAG_LOGE(" diag: In %s, dci_client_tbl is NULL\n", __func__);
		return DIAG_DCI_NO_REG;
	}

	if (version < 0 || version > DCI_VERSION) {
		DIAG_LOGE(" diag: In %s, Unsupported version req:%d cur:%d\n",
			__func__, version, DCI_VERSION);
		return DIAG_DCI_NOT_SUPPORTED;
	}

	dci_client_tbl[proc].version = version;
	return DIAG_DCI_NO_ERROR;
}

static void dci_delete_request_entry(diag_pkt_rsp_tracking_tbl *entry)
{
	if (!entry)
		return;
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	free(entry->info);
	free(entry);
}

void lookup_pkt_rsp_transaction(unsigned char *ptr, int proc)
{
	int len, uid, found = 0;
	uint8 delete_flag = 0;
	unsigned char *temp = ptr;
	diag_pkt_rsp_tracking_tbl *head = NULL, *walk_ptr = NULL;
	diag_pkt_tracking_info info;

	if (!ptr) {
		DIAG_LOGE("  Invalid pointer in %s\n", __func__);
		return;
	}

	if (!IS_VALID_DCI_PROC(proc)) {
		DIAG_LOGE("  Invalid proc %d in %s\n", proc, __func__);
		return;
	}

	len = *(int *)temp;
	temp += sizeof(int);
	delete_flag = *(uint8 *)temp;
	temp += sizeof(uint8);
	uid = *(int *)temp;
	temp += sizeof(int);
	len = len - sizeof(int); /* actual length of response */
	memset(&info, 0, sizeof(diag_pkt_tracking_info));

	pthread_mutex_lock(&(dci_client_tbl[proc].req_tbl_mutex));
	head = &dci_client_tbl[proc].req_tbl_head;
	for (walk_ptr = head->next; walk_ptr && walk_ptr != head; walk_ptr = walk_ptr->next) {
		if (!walk_ptr->info || walk_ptr->info->uid != uid)
			continue;
		/*
		 * Found a match. Copy the response to the buffer and call
		 * the corresponding response handler
		 */
		if (len > 0 && len <= walk_ptr->info->rsp_len) {
			memcpy(&info, walk_ptr->info, sizeof(diag_pkt_tracking_info));
			memcpy(info.rsp_ptr, temp, len);
		} else {
			DIAG_LOGE(" Invalid response in %s, len:%d rsp_len: %d\n", __func__, len, walk_ptr->info->rsp_len);
		}
		/*
		 * Delete Flag will be set if it is safe to delete the entry.
		 * This means that the response is either a regular response or
		 * the last response in a sequence of delayed responses.
		 */
		if (delete_flag)
			dci_delete_request_entry(walk_ptr);
		found = 1;
		break;
	}
	pthread_mutex_unlock(&(dci_client_tbl[proc].req_tbl_mutex));

	if (found) {
		if (info.func_ptr)
			(*(info.func_ptr))(info.rsp_ptr, len, info.data_ptr);
	} else {
		DIAG_LOGE("  In %s, incorrect transaction %d, proc: %d\n", __func__, uid, proc);
	}
}

int diag_register_dci_client(int *client_id, diag_dci_peripherals *list, int proc, void *os_params)
{
	int ret = 0;
	int req_buf_len = DCI_MAX_REQ_BUF_SIZE;

	/* Make place for the header - Choose the header that has maximum size */
	if (sizeof(struct diag_dci_req_header_t) > sizeof(struct diag_dci_stream_header_t))
		req_buf_len += sizeof(struct diag_dci_req_header_t);
	else
		req_buf_len += sizeof(struct diag_dci_stream_header_t);

	if (!client_id)
		return ret;
	if (!IS_VALID_DCI_PROC(proc))
		return ret;

	if (dci_client_tbl[proc].client_info.client_id != INVALID_DCI_CLIENT) {
		DIAG_LOGE("diag: There is already a DCI client registered for this proc: %d\n", proc);
		return DIAG_DCI_DUP_CLIENT;
	}

	dci_client_tbl[proc].client_info.notification_list = *list;
	dci_client_tbl[proc].client_info.signal_type = *(int *)os_params;
	dci_client_tbl[proc].client_info.token = proc;
	dci_client_tbl[proc].data_signal_flag = DISABLE;
	dci_client_tbl[proc].data_signal_type = DIAG_INVALID_SIGNAL;
	dci_client_tbl[proc].dci_req_buf = (unsigned char *)malloc(req_buf_len);
	if (!dci_client_tbl[proc].dci_req_buf)
		return DIAG_DCI_NO_MEM;

	ret = ioctl(diag_fd, DIAG_IOCTL_DCI_REG, &dci_client_tbl[proc].client_info, 0, NULL, 0, NULL, NULL);

	if (ret == DIAG_DCI_NO_REG || ret < 0) {
		DIAG_LOGE(" could not register client, ret: %d error: %d\n", ret, errno);
		dci_client_tbl[proc].client_info.client_id = INVALID_DCI_CLIENT;
		*client_id = INVALID_DCI_CLIENT;
		ret = DIAG_DCI_NO_REG;
	} else {
		dci_client_tbl[proc].client_info.client_id = ret;
		*client_id = ret;
		ret = DIAG_DCI_NO_ERROR;
	}

	return ret;
}

int diag_register_dci_stream(void (*func_ptr_logs)(unsigned char *ptr, int len),
			     void (*func_ptr_events)(unsigned char *ptr, int len))
{
	if (!dci_client_tbl)
		return DIAG_DCI_NO_MEM;
	return diag_register_dci_stream_proc(dci_client_tbl[DIAG_PROC_MSM].client_info.client_id, func_ptr_logs, func_ptr_events);
}

int diag_register_dci_stream_proc(int client_id, void(*func_ptr_logs)(unsigned char *ptr, int len),
				  void(*func_ptr_events)(unsigned char *ptr, int len))
{
	int proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;

	dci_client_tbl[proc].func_ptr_logs = func_ptr_logs;
	dci_client_tbl[proc].func_ptr_events = func_ptr_events;
	return DIAG_DCI_NO_ERROR;
}

int diag_release_dci_client(int *client_id)
{
	int result = 0, proc;
	diag_pkt_rsp_tracking_tbl *head = NULL, *walk_ptr = NULL;

	if (!client_id)
		return DIAG_DCI_NO_REG;

	proc = diag_dci_get_proc(*client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;

	result = ioctl(diag_fd, DIAG_IOCTL_DCI_DEINIT, client_id, 0, NULL, 0, NULL, NULL);

	if (result != DIAG_DCI_NO_ERROR) {
		DIAG_LOGE(" diag: could not remove entries, result: %d error: %d\n", result, errno);
		return DIAG_DCI_ERR_DEREG;
	} else {
		*client_id = 0;
		dci_client_tbl[proc].client_info.client_id = INVALID_DCI_CLIENT;

		/* Delete the client requests */
		pthread_mutex_lock(&(dci_client_tbl[proc].req_tbl_mutex));
		head = &dci_client_tbl[proc].req_tbl_head;
		for (walk_ptr = head->next; walk_ptr && walk_ptr != head; walk_ptr = head->next)
			dci_delete_request_entry(walk_ptr);
		pthread_mutex_unlock(&(dci_client_tbl[proc].req_tbl_mutex));

		free(dci_client_tbl[proc].dci_req_buf);
		return DIAG_DCI_NO_ERROR;
	}
}

static diag_pkt_rsp_tracking_tbl *diag_register_dci_pkt(int proc, void (*func_ptr)(unsigned char *ptr, int len, void *data_ptr),
							int uid, unsigned char *rsp_ptr, int rsp_len, void *data_ptr)
{
	diag_pkt_tracking_info *req_info = NULL;
	diag_pkt_rsp_tracking_tbl *temp = NULL;
	diag_pkt_rsp_tracking_tbl *new_req = NULL;
	diag_pkt_rsp_tracking_tbl *head = NULL;
	if (!IS_VALID_DCI_PROC(proc))
		return NULL;

	req_info = (diag_pkt_tracking_info *)malloc(sizeof(diag_pkt_tracking_info));
	if (!req_info)
		return NULL;
	new_req = (diag_pkt_rsp_tracking_tbl *)malloc(sizeof(diag_pkt_rsp_tracking_tbl));
	if (!new_req) {
		free(req_info);
		return NULL;
	}

	req_info->uid = uid;
	req_info->func_ptr = func_ptr;
	req_info->rsp_ptr = rsp_ptr;
	req_info->rsp_len = rsp_len;
	req_info->data_ptr = data_ptr;
	new_req->info = req_info;
	new_req->next = new_req->prev = NULL;

	pthread_mutex_lock(&(dci_client_tbl[proc].req_tbl_mutex));
	head = &dci_client_tbl[proc].req_tbl_head;
	temp = head->prev;
	head->prev = new_req;
	new_req->next = head;
	new_req->prev = temp;
	temp->next = new_req;
	pthread_mutex_unlock(&(dci_client_tbl[proc].req_tbl_mutex));

	return new_req;
}

int diag_send_dci_async_req(int client_id, unsigned char buf[], int bytes, unsigned char *rsp_ptr, int rsp_len,
					   void (*func_ptr)(unsigned char *ptr, int len, void *data_ptr), void *data_ptr)
{
	int err = -1, proc;
	diag_pkt_rsp_tracking_tbl *new_req = NULL;
	struct diag_dci_req_header_t header;
	unsigned char *dci_req_buf = NULL;
	unsigned int header_len = sizeof(struct diag_dci_req_header_t);

	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;

	if ((bytes > DCI_MAX_REQ_BUF_SIZE) || (bytes < 1)) {
		DIAG_LOGE("diag: In %s, huge packet: %d, max supported: %d\n",
			  __func__, bytes, DCI_MAX_REQ_BUF_SIZE);
		return DIAG_DCI_HUGE_PACKET;
	}

	if (!buf) {
		DIAG_LOGE("diag: Request Bufffer is not set\n");
		return DIAG_DCI_NO_MEM;
	}

	dci_req_buf = dci_client_tbl[proc].dci_req_buf;

	if (!dci_req_buf) {
		DIAG_LOGE("diag: Request Buffer not initialized\n");
		return DIAG_DCI_NO_MEM;
	}
	if (!rsp_ptr) {
		DIAG_LOGE("diag: Response Buffer not initialized\n");
		return DIAG_DCI_NO_MEM;
	}
	dci_transaction_id++;
	new_req = diag_register_dci_pkt(proc, func_ptr, dci_transaction_id, rsp_ptr, rsp_len, data_ptr);
	if (!new_req)
		return DIAG_DCI_NO_MEM;
	header.start = DCI_DATA_TYPE;
	header.uid = dci_transaction_id;
	header.client_id = client_id;
	memcpy(dci_req_buf, &header, header_len);
	memcpy(dci_req_buf + header_len, buf, bytes);
	err = diag_send_data(dci_req_buf, header_len + bytes);

	/* Registration failed. Delete entry from registration table */
	if (err != DIAG_DCI_NO_ERROR) {
		pthread_mutex_lock(&(dci_client_tbl[proc].req_tbl_mutex));
		dci_delete_request_entry(new_req);
		pthread_mutex_unlock(&(dci_client_tbl[proc].req_tbl_mutex));
		err = DIAG_DCI_SEND_DATA_FAIL;
	}

	return err;
}

int diag_get_dci_support_list(diag_dci_peripherals *list)
{
	return diag_get_dci_support_list_proc(DIAG_PROC_MSM, list);
}

int diag_get_dci_support_list_proc(int proc, diag_dci_peripherals *list)
{
	struct diag_dci_peripheral_list_t p_list;
	int err = DIAG_DCI_NO_ERROR;

	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_PARAM_FAIL;

	if (!list)
		return DIAG_DCI_NO_MEM;

	p_list.proc = proc;
	err = ioctl(diag_fd, DIAG_IOCTL_DCI_SUPPORT, &p_list, 0, NULL, 0, NULL, NULL);
	if (err == DIAG_DCI_NO_ERROR)
		*list = p_list.list;

	return err;
}

int diag_log_stream_config(int client_id, int set_mask, uint16 log_codes_array[], int num_codes)
{
	int err = -1, proc;
	struct diag_dci_stream_header_t header;
	unsigned char *dci_req_buf = NULL;
	unsigned int header_len = sizeof(struct diag_dci_stream_header_t);
	unsigned int data_len = sizeof(uint16) * num_codes;

	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;
	if (num_codes < 1)
		return DIAG_DCI_PARAM_FAIL;
	dci_req_buf = dci_client_tbl[proc].dci_req_buf;
	if (!dci_req_buf)
		return DIAG_DCI_NO_MEM;
	if (data_len > DCI_MAX_REQ_BUF_SIZE) {
		DIAG_LOGE("diag: In %s, huge packet: %d/%d\n", __func__,
			  data_len, DCI_MAX_REQ_BUF_SIZE);
		return DIAG_DCI_HUGE_PACKET;
	}

	header.start = DCI_DATA_TYPE;
	header.type = DCI_LOG_TYPE;
	header.client_id = client_id;
	header.set_flag = set_mask;
	header.count = num_codes;
	memcpy(dci_req_buf, &header, header_len);
	memcpy(dci_req_buf + header_len, log_codes_array, data_len);
	err = diag_send_data(dci_req_buf, header_len + data_len);
	if (err != DIAG_DCI_NO_ERROR)
		return DIAG_DCI_SEND_DATA_FAIL;
	else
		return DIAG_DCI_NO_ERROR;
}

int diag_event_stream_config(int client_id, int set_mask, int event_id_array[], int num_id)
{
	int err = -1, proc;
	struct diag_dci_stream_header_t header;
	unsigned char *dci_req_buf = NULL;
	unsigned int header_len = sizeof(struct diag_dci_stream_header_t);
	unsigned int data_len = sizeof(int) * num_id;

	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;
	if (num_id < 1)
		return DIAG_DCI_PARAM_FAIL;
	dci_req_buf = dci_client_tbl[proc].dci_req_buf;
	if (!dci_req_buf)
		return DIAG_DCI_NO_MEM;
	if (data_len > DCI_MAX_REQ_BUF_SIZE) {
		DIAG_LOGE("diag: In %s, huge packet: %d/%d\n", __func__,
			  data_len, DCI_MAX_REQ_BUF_SIZE);
		return DIAG_DCI_HUGE_PACKET;
	}

	header.start = DCI_DATA_TYPE;
	header.type = DCI_EVENT_TYPE;
	header.client_id = client_id;
	header.set_flag = set_mask;
	header.count = num_id;
	memcpy(dci_req_buf, &header, header_len);
	memcpy(dci_req_buf + header_len, event_id_array, data_len);
	err = diag_send_data(dci_req_buf, header_len + data_len);
	if (err != DIAG_DCI_NO_ERROR) {
		DIAG_LOGE(" diag: error sending log stream config\n");
		return DIAG_DCI_SEND_DATA_FAIL;
	}

	return DIAG_DCI_NO_ERROR;
}

int diag_get_health_stats(struct diag_dci_health_stats *dci_health)
{
	return diag_get_health_stats_proc(dci_client_tbl[DIAG_PROC_MSM].client_info.client_id, dci_health, DIAG_ALL_PROC);
}

int diag_get_health_stats_proc(int client_id, struct diag_dci_health_stats *dci_health, int proc)
{
	int err = DIAG_DCI_NO_ERROR, c_proc;
	struct diag_dci_health_stats_proc health_proc;

	c_proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(c_proc))
		return DIAG_DCI_NOT_SUPPORTED;
	if (proc < DIAG_ALL_PROC || proc > DIAG_APPS_PROC)
		return DIAG_DCI_PARAM_FAIL;
	if (!dci_health)
		return DIAG_DCI_NO_MEM;

	health_proc.client_id = client_id;
	health_proc.proc = proc;
	health_proc.health.reset_status = dci_health->reset_status;

	err = ioctl(diag_fd, DIAG_IOCTL_DCI_HEALTH_STATS, &health_proc, 0, NULL, 0, NULL, NULL);
	if (err == DIAG_DCI_NO_ERROR) {
		dci_health->dropped_logs = health_proc.health.dropped_logs;
		dci_health->dropped_events = health_proc.health.dropped_events;
		dci_health->received_logs = health_proc.health.received_logs;
		dci_health->received_events = health_proc.health.received_events;
	}

	return err;
}

int diag_get_log_status(int client_id, uint16 log_code, boolean *value)
{
	int err = DIAG_DCI_NO_ERROR, proc;
	struct diag_log_event_stats stats;

	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;
	if (!value)
		return DIAG_DCI_NO_MEM;

	stats.client_id = client_id;
	stats.code = log_code;
	stats.is_set = 0;
	err = ioctl(diag_fd, DIAG_IOCTL_DCI_LOG_STATUS, &stats, 0, NULL, 0, NULL, NULL);
	if (err != DIAG_DCI_NO_ERROR)
		return DIAG_DCI_SEND_DATA_FAIL;
	else
		*value = (stats.is_set == 1) ? TRUE : FALSE;

	return DIAG_DCI_NO_ERROR;
}

int diag_get_event_status(int client_id, uint16 event_id, boolean *value)
{
	int err = DIAG_DCI_NO_ERROR, proc;
	struct diag_log_event_stats stats;

	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;
	if (!value)
		return DIAG_DCI_NO_MEM;

	stats.client_id = client_id;
	stats.code = event_id;
	stats.is_set = 0;
	err = ioctl(diag_fd, DIAG_IOCTL_DCI_EVENT_STATUS, &stats, 0, NULL, 0, NULL, NULL);
	if (err != DIAG_DCI_NO_ERROR)
		return DIAG_DCI_SEND_DATA_FAIL;
	else
		*value = (stats.is_set == 1) ? TRUE : FALSE;

	return DIAG_DCI_NO_ERROR;
}

int diag_disable_all_logs(int client_id)
{
	int ret = DIAG_DCI_NO_ERROR, proc;
	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;
	ret = ioctl(diag_fd, DIAG_IOCTL_DCI_CLEAR_LOGS, &client_id, 0, NULL, 0, NULL, NULL);
	if (ret != DIAG_DCI_NO_ERROR) {
		DIAG_LOGE(" diag: error clearing all log masks, ret: %d, error: %d\n", ret, errno);
		return DIAG_DCI_SEND_DATA_FAIL;
	}
	return ret;
}

int diag_disable_all_events(int client_id)
{
	int ret = DIAG_DCI_NO_ERROR, proc;

	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;
	ret = ioctl(diag_fd, DIAG_IOCTL_DCI_CLEAR_EVENTS, &client_id, 0, NULL, 0, NULL, NULL);
	if (ret != DIAG_DCI_NO_ERROR) {
		DIAG_LOGE(" diag: error clearing all event masks, ret: %d, error: %d\n", ret, errno);
		return DIAG_DCI_SEND_DATA_FAIL;
	}
	return ret;
}

int diag_dci_vote_real_time(int client_id, int real_time)
{
	int err = DIAG_DCI_NO_ERROR, proc;
	struct real_time_vote_t vote;

	proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;

	if (!(real_time == MODE_REALTIME || real_time == MODE_NONREALTIME)) {
		DIAG_LOGE("diag: invalid mode change request\n");
		return DIAG_DCI_PARAM_FAIL;
	}
	vote.client_id = client_id;
	vote.proc = DIAG_PROC_DCI;
	vote.real_time_vote = real_time;
	err = ioctl(diag_fd, DIAG_IOCTL_VOTE_REAL_TIME, &vote, 0, NULL, 0, NULL, NULL);
	if (err == -1) {
		DIAG_LOGE(" diag: error voting for real time switch, ret: %d, error: %d\n", err, errno);
		err = DIAG_DCI_SEND_DATA_FAIL;
	}
	return DIAG_DCI_NO_ERROR;
}

int diag_dci_get_real_time_status(int *real_time)
{
	return diag_dci_get_real_time_status_proc(DIAG_PROC_MSM, real_time);
}

int diag_dci_get_real_time_status_proc(int proc, int *real_time)
{
	int err = DIAG_DCI_NO_ERROR;
	struct real_time_query_t query;

	if (!real_time) {
		DIAG_LOGE("diag: invalid pointer in %s\n", __func__);
		return DIAG_DCI_PARAM_FAIL;
	}
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;
	query.proc = proc;
	err = ioctl(diag_fd, DIAG_IOCTL_GET_REAL_TIME, &query, 0, NULL, 0, NULL, NULL);
	if (err != 0) {
		DIAG_LOGE(" diag: error in getting real time status, proc: %d, err: %d, error: %d\n", proc, err, errno);
		err = DIAG_DCI_SEND_DATA_FAIL;
	}
	*real_time = query.real_time;
	return DIAG_DCI_NO_ERROR;
}

int diag_register_dci_signal_data(int client_id, int signal_type)
{
	int proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;

	if (signal_type <= DIAG_INVALID_SIGNAL)
		return DIAG_DCI_PARAM_FAIL;

	dci_client_tbl[proc].data_signal_flag = ENABLE;
	dci_client_tbl[proc].data_signal_type = signal_type;
	return DIAG_DCI_NO_ERROR;
}

int diag_deregister_dci_signal_data(int client_id)
{
	int proc = diag_dci_get_proc(client_id);
	if (!IS_VALID_DCI_PROC(proc))
		return DIAG_DCI_NOT_SUPPORTED;

	if (dci_client_tbl[proc].data_signal_type == DIAG_INVALID_SIGNAL)
		return DIAG_DCI_NO_REG;

	dci_client_tbl[proc].data_signal_flag = DISABLE;
	dci_client_tbl[proc].data_signal_type = DIAG_INVALID_SIGNAL;
	return DIAG_DCI_NO_ERROR;
}

void diag_send_to_output(FILE *op_file, const char *str, ...)
{
	char buffer[6144];
	va_list arglist;
	va_start(arglist, str);
	if (!op_file)
		return;
	vsnprintf(buffer, 6144, str, arglist);
	fprintf(op_file, "%s", buffer);
	va_end(arglist);
}

