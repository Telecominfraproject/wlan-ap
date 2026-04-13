// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include "athdbg_qmi.h"
#include "athdbg_qdss.h"
#include "athdbg_core.h"
#include "../core.h"

extern struct ath_debug_base *athdbg_base;

static const struct qmi_elem_info qmi_wlanfw_mem_seg_resp_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, type),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_resp_s_v01, restore),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_respond_mem_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_req_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_resp_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_req_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_mem_cfg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, offset),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, size),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_cfg_s_v01, secure_flag),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static const struct qmi_elem_info qmi_wlanfw_mem_seg_s_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01,
				  size),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum qmi_wlanfw_mem_type_enum_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, type),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, mem_cfg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_WLANFW_MAX_NUM_MEM_CFG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_cfg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0,
		.offset		= offsetof(struct qmi_wlanfw_mem_seg_s_v01, mem_cfg),
		.ei_array	= qmi_wlanfw_mem_cfg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

const struct qmi_elem_info qmi_wlanfw_request_mem_ind_msg_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_request_mem_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01,
		.elem_size	= sizeof(struct qmi_wlanfw_mem_seg_s_v01),
		.array_type	= VAR_LEN_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct qmi_wlanfw_request_mem_ind_msg_v01,
					   mem_seg),
		.ei_array	= qmi_wlanfw_mem_seg_s_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_msg_handler athdbg_qmi_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01,
		.ei = qmi_wlanfw_request_mem_ind_msg_v01_ei,
		.decoded_size =
				sizeof(struct qmi_wlanfw_request_mem_ind_msg_v01),
		.fn = athdbg_wlfw_qdss_trace_req_mem_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_SAVE_IND_V01,
		.ei = qmi_wlanfw_qdss_trace_save_ind_msg_v01_ei,
		.decoded_size =
				sizeof(struct qmi_wlanfw_qdss_trace_save_ind_msg_v01),
		.fn = athdbg_wlfw_qdss_trace_save_ind_cb,
	},
	/* end of list */
	{},
};

static const struct qmi_elem_info qmi_wlanfw_respond_mem_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct qmi_wlanfw_respond_mem_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

#define NUM_HANDLER 8

struct qmi_msg_handler *athdbg_append_dbg_handler(const struct qmi_msg_handler *handlers)
{
	struct qmi_msg_handler *wdbg_handlers;

	wdbg_handlers = kzalloc(sizeof(struct qmi_msg_handler) * NUM_HANDLER, GFP_KERNEL);

	if (wdbg_handlers == NULL)
		return NULL;

	memcpy(wdbg_handlers, handlers, 5 * sizeof(struct qmi_msg_handler));

	//append debug handler
	memcpy(&wdbg_handlers[5], athdbg_qmi_msg_handlers,
			sizeof(struct qmi_msg_handler) * 2);

	return wdbg_handlers;
}

int athdbg_qmi_handle_init(struct qmi_handle *qmi, size_t recv_buf_size,
				const struct qmi_ops *ops,
				const struct qmi_msg_handler *handlers)
{
	int ret;
	struct qmi_msg_handler *wdbg_handlers;

	wdbg_handlers = athdbg_append_dbg_handler(handlers);

	if (wdbg_handlers) {
		ret = qmi_handle_init(qmi, ATH12K_QMI_RESP_LEN_MAX, ops, wdbg_handlers);

		if (ret < 0) {
			kfree(wdbg_handlers);
			goto out;
		}

		athdbg_base->wdbg_handlers[athdbg_base->wdbg_handlers_cnt++] =
							wdbg_handlers;
	} else {
		pr_err("Fallback to qmi int without dump handlers");
		ret = qmi_handle_init(qmi, ATH12K_QMI_RESP_LEN_MAX, ops, handlers);
	}

out:
	return ret;
}
EXPORT_SYMBOL(athdbg_qmi_handle_init);

static void athdbg_qmi_driver_event_work(struct work_struct *work)
{
	struct athdbg_qmi *dbg_qmi = container_of(work, struct athdbg_qmi,
					      event_work);
	struct athdbg_qmi_driver_event *event;
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);
	int ret;

	spin_lock(&dbg_qmi->event_lock);

	while (!list_empty(&dbg_qmi->event_list)) {
		event = list_first_entry(&dbg_qmi->event_list,
					 struct athdbg_qmi_driver_event, list);
		list_del(&event->list);
		spin_unlock(&dbg_qmi->event_lock);

		if (test_bit(ATH12K_FLAG_UNREGISTERING, &ab->dev_flags))
			goto skip;

		switch (event->type) {
		case ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_MEM:
			athdbg_qmi_event_qdss_trace_req_mem_hdlr(dbg_qmi);
			break;
		case ATHDBG_QMI_EVENT_QDSS_TRACE_SAVE:
			athdbg_qmi_event_qdss_trace_save_hdlr(dbg_qmi, event->data);
			break;
		case ATHDBG_QMI_EVENT_QDSS_TRACE_REQ_DATA:
			ret = athdbg_qmi_event_qdss_trace_misc_hdlr(dbg_qmi, event->data);

			if (ret < 0)
				pr_err("failed to collect phy logs : %d\n", ret);

			break;
		default:
			pr_err("invalid event type: %d", event->type);
			break;
		}

skip:
		kfree(event->data);
		kfree(event);
		spin_lock(&dbg_qmi->event_lock);
	}
	spin_unlock(&dbg_qmi->event_lock);
}

void athdbg_qmi_event_qdss_trace_save_hdlr(struct athdbg_qmi *dbg_qmi,
						  void *data)
{
	struct athdbg_qmi_event_qdss_trace_save_data *event_data = data;
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);

	if (!ab->dbg_qmi.qdss_mem_seg_len) {
		pr_err("Memory for QDSS trace is not available\n");
		return;
	}

	athdbg_coredump_qdss_dump(ab, event_data);
	athdbg_qmi_qdss_mem_free(ab);
}

int athdbg_qmi_event_qdss_trace_misc_hdlr(struct athdbg_qmi *dbg_qmi, void *data)
{
	struct qmi_wlfw_qdss_trace_data_req_msg_v01 *req;
	struct qmi_wlfw_qdss_trace_data_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct ath12k_qmi_event_qdss_trace_save_data *event_data = data;
	struct ath12k_base *ab = container_of(dbg_qmi, struct ath12k_base, dbg_qmi);
	u32 total_size = event_data->total_size;
	int ret = 0;
	u32 remaining;
	unsigned char *qdss_trace_data_temp, *qdss_trace_data = NULL;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	qdss_trace_data = kzalloc(total_size, GFP_KERNEL);

	if (!qdss_trace_data) {
		ret = -ENOMEM;
		goto out_free_req_resp;
	}

	remaining = total_size;
	qdss_trace_data_temp = qdss_trace_data;

	while (remaining) {
		if (resp->end_valid && resp->end)
			break;
		ret = qmi_txn_init(&ab->qmi.handle, &txn,
				   qmi_wlfw_qdss_trace_data_resp_msg_v01_ei, resp);
		if (ret < 0) {
			pr_err("Fail to initialize qmi txn err %d\n", ret);
			goto out_free_all;
		}

		ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
				      QMI_WLFW_QDSS_TRACE_DATA_REQ_V01,
				      QMI_WLFW_QDSS_TRACE_DATA_REQ_MSG_V01_MAX_MSG_LEN,
				      qmi_wlfw_qdss_trace_data_req_msg_v01_ei, req);
		if (ret < 0) {
			pr_err("qmi send request failed err %d\n", ret);
			qmi_txn_cancel(&txn);
			goto out_free_all;
		}

		ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));

		if (ret < 0) {
			pr_err("qmi failed to qdss data request, err = %d\n", ret);
			goto out_free_all;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			pr_err("Respond qdss data req failed, result: %d, err: %d\n",
				   resp->resp.result, resp->resp.error);
			ret = -EINVAL;
			goto out_free_all;
		}

		if (resp->total_size_valid == 1 && resp->total_size == total_size &&
		    resp->seg_id_valid == 1 && resp->seg_id == req->seg_id &&
		    resp->data_valid == 1 &&
		    resp->data_len <= QMI_WLANFW_MAX_DATA_SIZE_V01 &&
		    resp->data_len <= remaining) {
			memcpy(qdss_trace_data_temp, resp->data, resp->data_len);
		} else {
			pr_err("invalid qmi response\n");
			ret = -EINVAL;
			goto out_free_all;
		}

		remaining -= resp->data_len;
		qdss_trace_data_temp += resp->data_len;
		req->seg_id++;
	}

	if (!remaining && resp->end_valid && resp->end) {
		struct ath12k_dump_segment *segment;

		segment = vzalloc(sizeof(*segment));
		if (!segment) {
			ret = -ENOMEM;
			goto out_free_all;
		}
		segment->len = total_size;
		segment->vaddr = qdss_trace_data;
		segment->type = FW_CRASH_DUMP_QDSS_DATA;
		athdbg_base->dbg_to_ath_ops->coredump_dump_segment(ab,
							segment, segment->len);
		vfree(segment);
	} else {
		pr_err("dump collection failed: remaining-%u response end-%u\n",
			   remaining, resp->end);
	}
out_free_all:
	kfree(qdss_trace_data);
out_free_req_resp:
	kfree(req);
	kfree(resp);
	return ret;
}

int athdbg_qmi_qdss_trace_mem_info_send_sync(void  *qmi_ab)
{
	struct qmi_wlanfw_respond_mem_req_msg_v01 *req;
	struct qmi_wlanfw_respond_mem_resp_msg_v01 resp = {};
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;
	struct qmi_txn txn;
	int ret, i;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->mem_seg_len = ab->dbg_qmi.qdss_mem_seg_len;

	for (i = 0; i < req->mem_seg_len ; i++) {
		req->mem_seg[i].addr = ab->dbg_qmi.qdss_mem[i].paddr;
		req->mem_seg[i].size = ab->dbg_qmi.qdss_mem[i].size;
		req->mem_seg[i].type = ab->dbg_qmi.qdss_mem[i].type;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_respond_mem_resp_msg_v01_ei, &resp);

	if (ret < 0) {
		pr_err("Fail to initialize txn for QDSS trace mem request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01,
			       QMI_WLANFW_RESPOND_MEM_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_respond_mem_req_msg_v01_ei, req);

	if (ret < 0) {
		pr_err("qmi failed to respond memory request, err = %d\n",
			    ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn,
			   msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0) {
		pr_err("qmi failed memory request, err = %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("Respond mem req failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	kfree(req);
	return ret;
}

int athdbg_send_qdss_trace_mode_req(void *qmi_ab,
				    enum qmi_wlanfw_qdss_trace_mode_enum_v01 mode,
				    u64 value)
{
	int ret;
	struct qmi_txn txn;
	struct qmi_wlanfw_qdss_trace_mode_req_msg_v01 req = {};
	struct qmi_wlanfw_qdss_trace_mode_resp_msg_v01 resp = {};
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;

	req.mode_valid = 1;
	req.mode = mode;
	req.option_valid = 1;
	if (!value) {
		req.option = mode == QMI_WLANFW_QDSS_TRACE_OFF_V01 ?
			QMI_WLANFW_QDSS_STOP_ALL_TRACE : 0;
	} else {
		req.option = value;
	}

	ret = qmi_txn_init(&ab->qmi.handle, &txn,
			   qmi_wlanfw_qdss_trace_mode_resp_msg_v01_ei, &resp);
	if (ret < 0)
		return ret;

	ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
			       QMI_WLANFW_QDSS_TRACE_MODE_REQ_V01,
			       QMI_WLANFW_QDSS_TRACE_MODE_REQ_MSG_V01_MAX_LEN,
			       qmi_wlanfw_qdss_trace_mode_req_msg_v01_ei, &req);

	if (ret < 0) {
		pr_err("Failed to send QDSS trace mode request,err = %d\n", ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
	if (ret < 0)
		goto out;

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("QDSS trace mode request failed, result: %d, err: %d\n",
			    resp.resp.result, resp.resp.error);
		ret = -EINVAL;
		goto out;
	}
out:
	return ret;
}

void athdbg_qmi_qdss_mem_free(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->dbg_qmi.qdss_mem_seg_len; i++) {
		if (ab->dbg_qmi.qdss_mem[i].v.ioaddr) {
			iounmap(ab->dbg_qmi.qdss_mem[i].v.ioaddr);
			ab->dbg_qmi.qdss_mem[i].v.ioaddr = NULL;
			ab->dbg_qmi.qdss_mem[i].size = 0;
			ab->dbg_qmi.qdss_mem[i].paddr = 0;
		}
	}

	ab->dbg_qmi.qdss_mem_seg_len = 0;
	ab->is_qdss_tracing = false;
}
EXPORT_SYMBOL(athdbg_qmi_qdss_mem_free);

int athdbg_qmi_send_qdss_trace_config_download_req(void *qmi_ab,
								const u8 *buffer,
								unsigned int buffer_len)
{
	int ret = 0;
	struct qmi_wlanfw_qdss_trace_config_download_req_msg_v01 *req;
	struct qmi_wlanfw_qdss_trace_config_download_resp_msg_v01 resp;
	struct qmi_txn txn;
	const u8 *temp = buffer;
	int  max_len = QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_MSG_V01_MAX_LEN;
	unsigned int  remaining;
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	remaining = buffer_len;
	while (remaining) {
		memset(&resp, 0, sizeof(resp));
		req->total_size_valid = 1;
		req->total_size = buffer_len;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;

		if (remaining > QMI_WLANFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLANFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		memcpy(req->data, temp, req->data_len);

		ret = qmi_txn_init(&ab->qmi.handle, &txn,
				   qmi_wlanfw_qdss_trace_config_download_resp_msg_v01_ei,
				   &resp);
		if (ret < 0)
			goto out;

		ret = qmi_send_request(&ab->qmi.handle, NULL, &txn,
				QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01,
				max_len,
				qmi_wlfw_qdss_trace_config_download_req_msg_v01_ei,
				req);
		if (ret < 0) {
			pr_err("Failed to send QDSS config download request = %d\n",
					ret);
			qmi_txn_cancel(&txn);
			goto out;
		}

		ret = qmi_txn_wait(&txn, msecs_to_jiffies(ATH12K_QMI_WLANFW_TIMEOUT_MS));
		if (ret < 0)
			goto out;

		if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
			pr_err("QDSS config download request failed, res: %d,err: %d\n",
					resp.resp.result, resp.resp.error);
			ret = -EINVAL;
			goto out;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
		}

out:
		kfree(req);
		return ret;
}


int athdbg_qmi_driver_event_post(struct ath12k_qmi *qmi, enum athdbg_qmi_event_type type,
								 void *data)
{

	struct athdbg_qmi_driver_event *event;
	struct ath12k_base *ab = (struct ath12k_base *)qmi->ab;
	struct athdbg_qmi *dbg_qmi = &ab->dbg_qmi;

	event = kzalloc(sizeof(*event), GFP_ATOMIC);
	if (!event)
		return -ENOMEM;

	event->type = type;
	event->data = data;

	spin_lock(&dbg_qmi->event_lock);
	list_add_tail(&event->list, &dbg_qmi->event_list);
	spin_unlock(&dbg_qmi->event_lock);

	queue_work(dbg_qmi->event_wq, &dbg_qmi->event_work);

	return 0;
}

int athdbg_qmi_worker_init(void *qmi_ab)
{
	struct ath12k_base *ab = (struct ath12k_base *)qmi_ab;

	if (ab->dbg_qmi.event_wq != NULL)
		return 0;

	ab->dbg_qmi.event_wq = alloc_ordered_workqueue("athdbg_qmi_driver_event", 0);
	if (!ab->dbg_qmi.event_wq) {
		pr_err("failed to allocate workqueue\n");
		return -EFAULT;
	}

	INIT_LIST_HEAD(&ab->dbg_qmi.event_list);
	spin_lock_init(&ab->dbg_qmi.event_lock);
	INIT_WORK(&ab->dbg_qmi.event_work, athdbg_qmi_driver_event_work);

	return 0;
}
EXPORT_SYMBOL(athdbg_qmi_worker_init);

void athdbg_qmi_deinit(struct ath12k_base *ab)
{
	int i;

	if (ab->dbg_qmi.event_wq) {
		cancel_work_sync(&ab->dbg_qmi.event_work);
		destroy_workqueue(ab->dbg_qmi.event_wq);
	}

	for (i = 0; i < athdbg_base->wdbg_handlers_cnt; i++) {
		if (athdbg_base->wdbg_handlers[i] == NULL)
			continue;

		kfree(athdbg_base->wdbg_handlers[i]);
		athdbg_base->wdbg_handlers[i] = NULL;
	}
	athdbg_base->wdbg_handlers_cnt = 0;
}
EXPORT_SYMBOL(athdbg_qmi_deinit);
