/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __LICENSE_MANAGER_H__
#define __LICENSE_MANAGER_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/of_device.h>

#define QMI_LICENSE_MANAGER_SERVICE_MAX_MSG_LEN 10259

#define QMI_LM_SERVICE_ID_V01 0x0423
#define QMI_LM_SERVICE_VERS_V01 0x01

#define QMI_LM_FEATURE_LIST_REQ_V01 0x0102
#define QMI_LM_FEATURE_LIST_RESP_V01 0x0102

#define QMI_LM_MAX_CHIPINFO_ID_LEN_V01 32
#define QMI_LM_MAX_FEATURE_LIST_V01 100
#define QMI_LM_MAX_LICENSE_FILES_V01 20
#define QMI_LM_MAX_BUFF_SIZE_V01	2048

#define CLIENT_MAX 		6
#define FILE_NAME_MAX 		128
#define LICENSE_IDENT_MAX_LEN	8
#define TMEL_BOUND_MAX_LICENSE_FILES	30

#define CBOR_BUFFER_SIZE	4096

#define SMEM_SOFTSKU_INFO	508
#define DDR_SPACE_LIMIT_FID	3008

#define MAX_SOC_HW_FID		12

struct qmi_lm_feature_list_req_msg_v01 {
	u32 reserved;
	u8 feature_list_valid;
	u32 feature_list_len;
	u32 feature_list[QMI_LM_MAX_FEATURE_LIST_V01];
	u8 file_result_valid;
	u32 file_result_len;
	u32 file_result[QMI_LM_MAX_LICENSE_FILES_V01];
	u8 lic_common_error_valid;
	u32 lic_common_error;
	u8 oem_id_valid;
	u32 oem_id;
	u8 jtag_id_valid;
	u32 jtag_id;
	u8 serial_number_valid;
	u64 serial_number;
	u8 req_buff_valid;
	u32 req_buff_len;
	u8 req_buff[QMI_LM_MAX_BUFF_SIZE_V01];
};
#define QMI_LM_FEATURE_LIST_REQ_MSG_V01_MAX_MSG_LEN 2580

struct qmi_lm_feature_list_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 resp_buff_valid;
	u32 resp_buff_len;
	u8 resp_buff[QMI_LM_MAX_BUFF_SIZE_V01];
};
#define QMI_LM_FEATURE_LIST_RESP_MSG_V01_MAX_MSG_LEN 2060

struct lm_files {
	int num_of_file;
	char file_name[QMI_LM_MAX_LICENSE_FILES_V01][FILE_NAME_MAX];
};

struct lm_svc_ctx {
	struct device *dev;
	struct qmi_handle *lm_svc_hdl;
	struct list_head clients_feature_list;
	struct list_head soc_hw_feature_list;
	bool license_feature;
	bool license_buf_valid;
	void *license_buf;
	struct lm_files *license_files;
	dma_addr_t license_dma_addr;
	size_t license_buf_len;
	bool soc_bounded;
	bool tmel_bounded;
};

enum sec_feature_status_type {
	SEC_FEATURE_STATUS_ACTIVE = 0x00,
	SEC_FEATURE_STATUS_NOTACTIVE = 0x01,
	SEC_FEATURE_STATUS_NOTPRESENT = 0x02,
	SEC_FEATURE_STATUS_DISABLED = 0x03,
};

struct sec_feature_value_type {
	u32 encoding_type;
	u32 feature_value;
};

struct softsku_info_smem {
	bool fid_updated;
	u32 feature_id; /*featureID*/
	bool HWEnforceStatus;
	enum sec_feature_status_type feature_status;
	bool is_time_bound;
	u64 grace_until;
	struct sec_feature_value_type feature_value;
};

struct lm_soc_hw_feat {
	struct list_head node;
	bool fid_updated;
	u32 feature_id; /*featureID*/
	enum sec_feature_status_type feature_status;
	bool HWEnforceStatus;
};

struct sec_enforce_hw_featid{
	u32 feature_id;
	bool hw_feat_status;
};

struct feature_info {
	int sq_node;
	int sq_port;
	uint32_t reserved;
	uint32_t len;
	uint32_t list[QMI_LM_MAX_FEATURE_LIST_V01];
	uint32_t file_result_len;
	uint32_t file_result[QMI_LM_MAX_LICENSE_FILES_V01];
	uint32_t lic_common_error;
	uint32_t oem_id;
	uint32_t jtag_id;
	uint64_t serial_number;
	struct list_head node;
};

struct target_info {
	int sq_node;
	int sq_port;
	uint32_t list_len;
	uint32_t list[QMI_LM_MAX_FEATURE_LIST_V01];
	uint32_t lic_file_info_len;
	int file_status[QMI_LM_MAX_LICENSE_FILES_V01];
};

struct client_target_info {
	int info_len;
	struct target_info info[CLIENT_MAX];
	int num_of_file;
	char file_name[QMI_LM_MAX_LICENSE_FILES_V01][FILE_NAME_MAX];
};

struct bindings_resp {
	void *nonce_buf;
	void *ecdsa_buf;
	uint32_t nonce_buf_len;
	uint32_t ecdsa_buf_len;
	uint32_t ecdsa_consumed_len;
};

struct ttime_get_req_params {
	void *params_buf;
	u32 buf_len;
	u32 used_buf_len;
};

struct ttime_set {
	void *ttime_buf;
	u32 buf_len;
};

struct lm_install_resp {
	u32 ident_len;
	u64 identifier;
	u32 flags;
};

struct lm_install_info {
	struct lm_install_resp lm_resp[TMEL_BOUND_MAX_LICENSE_FILES];
	u32 num_of_resp;
};

struct lm_get_toBeDel_lic {
	u64 identifiers[TMEL_BOUND_MAX_LICENSE_FILES];
	u32 used_len;
};

struct lm_license_check_cbor {
	void *buf;
	u32 buf_len;
	u32 used_len;
};

#define GET_FID_INFO 		_IOWR('L', 1, struct client_target_info)
#define LICENSE_RESCAN 		_IO('L', 2)
#define GET_BINDINGS		_IOWR('L', 3, struct bindings_resp)
#define TTIME_GET_REQ_PARAMS	_IOWR('L', 4, struct ttime_get_req_params)
#define TTIME_SET		_IOWR('L', 5, struct ttime_set)
#define GET_TMEL_BOUNDED	_IOWR('L', 6, u32)
#define LICENSE_INSTALL		_IOWR('L', 7, struct lm_install_info)
#define GET_TOBEDEL_LICENSES	_IOWR('L', 8, struct lm_get_toBeDel_lic)
#define LICENSE_CHECK		_IOWR('L', 9, struct lm_license_check_cbor)

enum req_type {
	INTERNAL,
	EXTERNAL,
	TYPE_MAX
};

#ifdef CONFIG_QTI_LICENSE_MANAGER
void *lm_get_license(enum req_type type, dma_addr_t *dma_addr, size_t *buf_len, dma_addr_t nonce_dma_addr, void *cbor_req_buf, u32 cbor_req_len);
void lm_free_license(void *buf, dma_addr_t dma_addr, size_t buf_len);
#else
static inline void *lm_get_license(enum req_type type, dma_addr_t *dma_addr, size_t *buf_len, dma_addr_t nonce_dma_addr, void *cbor_req_buf, u32 cbor_req_len)
{
	return NULL;
}

static inline void lm_free_license(void *buf, dma_addr_t dma_addr, size_t buf_len)
{
}
#endif /* CONFIG_QTI_LICENSE_MANAGER */
#endif /* __LICENSE_MANAGER_H___ */
