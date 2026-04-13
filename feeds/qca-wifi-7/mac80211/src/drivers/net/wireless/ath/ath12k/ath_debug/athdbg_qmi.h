/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef ATHDBG_QMI_H
#define ATHDBG_QMI_H

#include "../qmi.h"

#define ATH12K_QMI_MAX_QDSS_CONFIG_FILE_NAME_SIZE      64
#define ATH12K_QMI_DEFAULT_QDSS_CONFIG_FILE_NAME       "qdss_trace_config.bin"

#define QDSS_ETR_MEM_REGION_TYPE                        0x6

#define QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01     0x003F
#define QMI_Q6_QDSS_ETR_SIZE_QCN9274            0x100000
#define QMI_WLFW_QDSS_TRACE_SAVE_IND_V01        0x0041
#define QMI_WLFW_QDSS_TRACE_DATA_REQ_V01 0x0042
#define QMI_WLFW_QDSS_TRACE_DATA_RESP_V01 0x0042
#define QMI_Q6_QDSS_ETR_OFFSET_QCN9274		0x2500000

#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_MSG_V01_MAX_LEN 6167
#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_RESP_MSG_V01_MAX_LEN 7
#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01 0x0044
#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_RESP_V01 0x0044

#define QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01            0x0040

struct qmi_wlanfw_qdss_trace_config_download_req_msg_v01 {
	u8 total_size_valid;
	u32 total_size;
	u8 seg_id_valid;
	u32 seg_id;
	u8 data_valid;
	u32 data_len;
	u8 data[QMI_WLANFW_MAX_DATA_SIZE_V01];
	u8 end_valid;
	u8 end;
};

struct qmi_wlanfw_qdss_trace_config_download_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLANFW_QDSS_TRACE_MODE_REQ_V01 0x0045
#define QMI_WLANFW_QDSS_TRACE_MODE_REQ_MSG_V01_MAX_LEN 18
#define QMI_WLANFW_QDSS_TRACE_MODE_RESP_MSG_V01_MAX_LEN 7
#define QMI_WLANFW_QDSS_TRACE_MODE_RESP_V01 0x0045
#define QMI_WLANFW_QDSS_STOP_ALL_TRACE 0x01

enum qmi_wlanfw_qdss_trace_mode_enum_v01 {
	WLFW_QDSS_TRACE_MODE_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLANFW_QDSS_TRACE_OFF_V01 = 0,
	QMI_WLANFW_QDSS_TRACE_ON_V01 = 1,
	WLFW_QDSS_TRACE_MODE_ENUM_MAX_VAL_V01 = INT_MAX,
};

struct qmi_wlanfw_qdss_trace_mode_req_msg_v01 {
	u8 mode_valid;
	enum qmi_wlanfw_qdss_trace_mode_enum_v01 mode;
	u8 option_valid;
	u64 option;
};

struct qmi_wlanfw_qdss_trace_mode_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLFW_QDSS_TRACE_DATA_REQ_MSG_V01_MAX_MSG_LEN 7
#define QMI_WLFW_QDSS_TRACE_DATA_RESP_MSG_V01_MAX_MSG_LEN 6174

struct qmi_wlfw_qdss_trace_data_req_msg_v01 {
	u32 seg_id;
};

struct qmi_wlfw_qdss_trace_data_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 total_size_valid;
	u32 total_size;
	u8 seg_id_valid;
	u32 seg_id;
	u8 data_valid;
	u32 data_len;
	u8 data[QMI_WLANFW_MAX_DATA_SIZE_V01];
	u8 end_valid;
	u8 end;
};

struct qmi_wlanfw_qdss_trace_save_ind_msg_v01 {
	u32 source;
	u32 total_size;
	u8 mem_seg_valid;
	u32 mem_seg_len;
	struct qmi_wlanfw_mem_seg_resp_s_v01
			mem_seg[ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
	u8 file_name_valid;
	char file_name[QMI_WLANFW_MAX_STR_LEN_V01 + 1];
};

#define QDSS_TRACE_SEG_LEN_MAX 32

struct qdss_trace_mem_seg {
	u64 addr;
	u32 size;
};

struct ath12k_qmi_event_qdss_trace_save_data {
	u32 total_size;
	u32 mem_seg_len;
	struct qdss_trace_mem_seg mem_seg[QDSS_TRACE_SEG_LEN_MAX];
};

enum athdbg_qmi_event_type {
	ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_MEM = 15,
	ATHDBG_QMI_EVENT_QDSS_TRACE_SAVE,
	ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_DATA,
	ATHDBG_QMI_EVENT_MAX,
};

struct athdbg_qmi_driver_event {
	struct list_head list;
	enum athdbg_qmi_event_type type;
	void *data;
};


struct athdbg_qmi {
	struct work_struct event_work;
	struct workqueue_struct *event_wq;
	struct list_head event_list;
	spinlock_t event_lock; /* spinlock for qmi event list */
	struct target_mem_chunk qdss_mem[ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
	u32 qdss_mem_seg_len;
	struct m3_mem_region m3_mem;
};

void athdbg_wlfw_qdss_trace_req_mem_ind_cb(struct qmi_handle *qmi_hdl,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *data);
void athdbg_wlfw_qdss_trace_save_ind_cb(struct qmi_handle *qmi_hdl,
	struct sockaddr_qrtr *sq,
	struct qmi_txn *txn,
	const void *data);

int athdbg_configure_qmi(void *qmi_ab);
int athdbg_qmi_driver_event_post(struct ath12k_qmi *qmi,
			enum athdbg_qmi_event_type type,
			void *data);

int athdbg_qmi_qdss_trace_mem_info_send_sync(void *qmi_ab);
void athdbg_qmi_qdss_mem_free(struct ath12k_base *ab);
int athdbg_send_qdss_trace_mode_req(void *qmi_ab,
		enum qmi_wlanfw_qdss_trace_mode_enum_v01 mode, u64 value);
int athdbg_qmi_send_qdss_trace_config_download_req(void *qmi_ab,
				const u8 *buffer,
				unsigned int buffer_len);
int athdbg_qmi_event_qdss_trace_misc_hdlr(struct athdbg_qmi *dbg_qmi, void *data);
void athdbg_qmi_event_qdss_trace_save_hdlr(struct athdbg_qmi *dbg_qmi,
						  void *data);
int athdbg_qmi_handle_init(struct qmi_handle *qmi, size_t recv_buf_size,
		const struct qmi_ops *ops,
		const struct qmi_msg_handler *handlers);
#endif
