/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH11K_QMI_H
#define ATH11K_QMI_H

#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/remoteproc/qcom_rproc.h>

#define ATH11K_HOST_VERSION_STRING		"WIN"
#define ATH11K_QMI_WLANFW_TIMEOUT_MS		10000
#define ATH11K_QMI_MAX_BDF_FILE_NAME_SIZE	64
#define ATH11K_QMI_CALDB_ADDRESS		0x4BA00000
#define ATH11K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01	128
#define ATH11K_QMI_WLFW_SERVICE_ID_V01		0x45
#define ATH11K_QMI_WLFW_SERVICE_VERS_V01	0x01
#define ATH11K_QMI_WLFW_SERVICE_INS_ID_V01	0x02
#define ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCA6390	0x01
#define ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ8074	0x02
#define ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9074	0x07
#define ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_WCN6750	0x03
#define ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCN6122	0x40
#define ATH11K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01	32
#define ATH11K_QMI_RESP_LEN_MAX			8192
#define ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01	52
#define ATH11K_QMI_CALDB_SIZE			0x480000
#define ATH11K_QMI_BDF_EXT_STR_LENGTH		0x20
#define ATH11K_QMI_FW_MEM_REQ_SEGMENT_CNT	5
#define ATH11K_QMI_MAX_QDSS_CONFIG_FILE_NAME_SIZE 64
#define ATH11K_QMI_DEFAULT_QDSS_CONFIG_FILE_NAME "qdss_trace_config.bin"

#ifdef CPTCFG_ATH11K_MEM_PROFILE_512M
#define ATH11K_QMI_QCN9074_M3_OFFSET           0xC00000
#define ATH11K_QMI_QCN9074_QDSS_OFFSET         0xD00000
#define ATH11K_QMI_QCN9074_CALDB_OFFSET        0xE00000
#define ATH11K_QMI_QCN9074_PAGEABLE_OFFSET     0x1600000
#define ATH11K_QMI_IPQ8074_M3_DUMP_ADDRESS     0x4E800000
#define ATH11K_QMI_IPQ6018_M3_DUMP_ADDRESS     0x4E300000
#define ATH11K_QMI_IPQ8074_M3_DUMP_OFFSET      0x3800000
#define ATH11K_QMI_IPQ6018_M3_DUMP_OFFSET      0x3800000
#else
#define ATH11K_QMI_QCN9074_M3_OFFSET           0x2300000
#define ATH11K_QMI_QCN9074_QDSS_OFFSET         0x2400000
#define ATH11K_QMI_QCN9074_CALDB_OFFSET        0x2500000
#define ATH11K_QMI_QCN9074_PAGEABLE_OFFSET     0x2D00000
#define ATH11K_QMI_IPQ8074_M3_DUMP_ADDRESS     0x51000000
#define ATH11K_QMI_IPQ6018_M3_DUMP_ADDRESS     0x50100000
#define ATH11K_QMI_IPQ8074_M3_DUMP_OFFSET      0x6000000
#define ATH11K_QMI_IPQ6018_M3_DUMP_OFFSET      0x5600000
#endif

#define ATH11K_QMI_M3_DUMP_SIZE                       0x100000

#define ATH11K_QMI_IPQ8074_CALDB_OFFSET		0xA00000
#define ATH11K_QMI_IPQ8074_BDF_OFFSET		0xC0000
#define ATH11K_QMI_IPQ6018_CALDB_OFFSET		0xA00000
#define ATH11K_QMI_IPQ6018_BDF_OFFSET		0xC0000
#define ATH11K_QMI_IPQ5018_M3_OFFSET		0xD00000
#define ATH11K_QMI_IPQ5018_QDSS_OFFSET		0xE00000
#define ATH11K_QMI_IPQ5018_CALDB_OFFSET		0xF00000
#define ATH11K_QMI_QCN6122_M3_OFFSET		0xD00000
#define ATH11K_QMI_QCN6122_QDSS_OFFSET		0xE00000
#define ATH11K_QMI_QCN6122_CALDB_OFFSET		0xF00000
#define ATH11K_QMI_IPQ9574_CALDB_OFFSET		0xA00000
#define ATH11K_QMI_IPQ9574_BDF_OFFSET		0xC0000
#define ATH11K_QMI_IPQ9574_M3_OFFSET		0xD00000

#define ATH11K_QMI_QCN6122_M3_OFFSET            0xD00000
#define ATH11K_QMI_QCN6122_QDSS_OFFSET          0xE00000
#define ATH11K_QMI_QCN6122_CALDB_OFFSET         0xF00000
#define ATH11K_QMI_IPQ9574_CALDB_OFFSET         0xA00000
#define ATH11K_QMI_IPQ9574_BDF_OFFSET           0xC0000
#define ATH11K_QMI_IPQ9574_M3_OFFSET            0xD00000
#define QMI_WLFW_REQUEST_MEM_IND_V01		0x0035
#define QMI_WLFW_FW_MEM_READY_IND_V01		0x0037
#define QMI_WLFW_COLD_BOOT_CAL_DONE_IND_V01	0x003E
#define QMI_WLFW_FW_READY_IND_V01		0x0038
#define QMI_WLFW_FW_INIT_DONE_IND_V01		0x0038
#define QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01    0x004E
#define QMI_WLFW_M3_DUMP_UPLOAD_REQ_IND_V01     0x004D
#define QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01     0x003F
#define QMI_Q6_QDSS_ETR_SIZE_QCN9074           0x100000
#define QMI_WLFW_QDSS_TRACE_SAVE_IND_V01        0x0041

#define QMI_WLANFW_MAX_DATA_SIZE_V01		6144
#define ATH11K_FIRMWARE_MODE_OFF		4
#define ATH11K_COLD_BOOT_FW_RESET_DELAY		(60 * HZ)

#define ATH11K_QMI_DEVICE_BAR_SIZE		0x200000
#define ATH11K_RCV_GIC_MSI_HDLR_DELAY 		(3 * HZ)
#define ATH11K_QMI_QCN6122_M3_DUMP_ADDRESS	0x4E200000

#define ATH11K_QMI_IPQ9574_M3_DUMP_ADDRESS      0x4D600000

struct ath11k_base;
extern unsigned int ath11k_host_ddr_addr;
extern char *ath11k_caldata_bin_path;

enum ath11k_target_mem_mode {
 	ATH11K_QMI_TARGET_MEM_MODE_DEFAULT = 0,
 	ATH11K_QMI_TARGET_MEM_MODE_512M,
	ATH11K_QMI_TARGET_MEM_MODE_256M,
};

enum ath11k_qmi_file_type {
	ATH11K_QMI_FILE_TYPE_BDF_GOLDEN,
	ATH11K_QMI_FILE_TYPE_CALDATA = 2,
	ATH11K_QMI_FILE_TYPE_EEPROM,
	ATH11K_QMI_MAX_FILE_TYPE,
};

enum ath11k_qmi_bdf_type {
	ATH11K_QMI_BDF_TYPE_BIN			= 0,
	ATH11K_QMI_BDF_TYPE_ELF			= 1,
	ATH11K_QMI_BDF_TYPE_REGDB		= 4,
};

enum ath11k_qmi_event_type {
	ATH11K_QMI_EVENT_SERVER_ARRIVE,
	ATH11K_QMI_EVENT_SERVER_EXIT,
	ATH11K_QMI_EVENT_REQUEST_MEM,
	ATH11K_QMI_EVENT_FW_MEM_READY,
	ATH11K_QMI_EVENT_COLD_BOOT_CAL_START,
	ATH11K_QMI_EVENT_COLD_BOOT_CAL_DONE,
	ATH11K_QMI_EVENT_REGISTER_DRIVER,
	ATH11K_QMI_EVENT_UNREGISTER_DRIVER,
	ATH11K_QMI_EVENT_RECOVERY,
	ATH11K_QMI_EVENT_FORCE_FW_ASSERT,
	ATH11K_QMI_EVENT_POWER_UP,
	ATH11K_QMI_EVENT_POWER_DOWN,
	ATH11K_QMI_EVENT_M3_DUMP_UPLOAD_REQ,
	ATH11K_QMI_EVENT_QDSS_TRACE_REQ_MEM,
	ATH11K_QMI_EVENT_QDSS_TRACE_SAVE,
	ATH11K_QMI_EVENT_FW_INIT_DONE,
	ATH11K_QMI_EVENT_MAX,
};

struct ath11k_qmi_driver_event {
	struct list_head list;
	enum ath11k_qmi_event_type type;
	void *data;
};

struct ath11k_qmi_m3_dump_data {
	u32 pdev_id;
	u32 size;
	u64 timestamp;
	u32 *addr;
};

struct ath11k_qmi_ce_cfg {
	const struct ce_pipe_config *tgt_ce;
	int tgt_ce_len;
	const struct service_to_pipe *svc_to_ce_map;
	int svc_to_ce_map_len;
	const u8 *shadow_reg;
	int shadow_reg_len;
	u32 *shadow_reg_v2;
	int shadow_reg_v2_len;
};

struct ath11k_qmi_event_msg {
	struct list_head list;
	enum ath11k_qmi_event_type type;
};

struct target_mem_chunk {
	u32 size;
	u32 type;
	u32 prev_size;
	u32 prev_type;
	dma_addr_t paddr;
	union {
		u32 *vaddr;
		void __iomem *iaddr;
		void *anyaddr;
	};
};

struct target_info {
	u32 chip_id;
	u32 chip_family;
	u32 board_id;
	u32 soc_id;
	u32 fw_version;
	u32 eeprom_caldata;
	u8 regdb;
	char fw_build_timestamp[ATH11K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01 + 1];
	char fw_build_id[ATH11K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01 + 1];
	char bdf_ext[ATH11K_QMI_BDF_EXT_STR_LENGTH];
};

struct m3_mem_region {
	u32 size;
	dma_addr_t paddr;
	void *vaddr;
};

struct ath11k_qmi {
	struct ath11k_base *ab;
	struct qmi_handle handle;
	struct sockaddr_qrtr sq;
	struct work_struct event_work;
	struct workqueue_struct *event_wq;
	struct list_head event_list;
	spinlock_t event_lock; /* spinlock for qmi event list */
	struct notifier_block ssr_nb;
	struct notifier_block atomic_ssr_nb;
	void *ssr_handle;
	void *atomic_ssr_handle;
	struct ath11k_qmi_ce_cfg ce_cfg;
	struct target_mem_chunk target_mem[ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
	u32 mem_seg_count;
	struct target_mem_chunk qdss_mem[ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
	u32 qdss_mem_seg_len;
	u32 target_mem_mode;
	bool target_mem_delayed;
	u8 cal_done;
	struct target_info target;
	struct m3_mem_region m3_mem;
	unsigned int service_ins_id;
	wait_queue_head_t cold_boot_waitq;
};

struct ath11k_qmi_m3_dump_upload_req_data {
	u32 pdev_id;
	u64 addr;
	u64 size;
};

#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_MSG_V01_MAX_LEN 6167
#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_RESP_MSG_V01_MAX_LEN 7
#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01 0x0044
#define QMI_WLANFW_QDSS_TRACE_CONFIG_DOWNLOAD_RESP_V01 0x0044

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

struct qmi_wlanfw_m3_dump_upload_done_req_msg_v01 {
	u32 pdev_id;
	u32 status;
};

struct qmi_wlanfw_m3_dump_upload_done_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

struct wlfw_ini_req_msg_v01 {
	u8 enablefwlog_valid;
	u8 enablefwlog;
};

struct wlfw_ini_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLFW_INI_REQ_V01 0x002F
#define WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN 4

#define QMI_WLANFW_QDSS_TRACE_MODE_REQ_V01 0x0045
#define QMI_WLANFW_QDSS_TRACE_MODE_REQ_MSG_V01_MAX_LEN 18
#define QMI_WLANFW_QDSS_TRACE_MODE_RESP_MSG_V01_MAX_LEN 7
#define QMI_WLANFW_QDSS_TRACE_MODE_RESP_V01 0x0045
#define QMI_WLANFW_QDSS_STOP_ALL_TRACE 0x3f

enum wlfw_qdss_trace_mode_enum_v01 {
	WLFW_QDSS_TRACE_MODE_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLANFW_QDSS_TRACE_OFF_V01 = 0,
	QMI_WLANFW_QDSS_TRACE_ON_V01 = 1,
	WLFW_QDSS_TRACE_MODE_ENUM_MAX_VAL_V01 = INT_MAX,
};

struct qmi_wlanfw_qdss_trace_mode_req_msg_v01 {
	u8 mode_valid;
	enum wlfw_qdss_trace_mode_enum_v01 mode;
	u8 option_valid;
	u64 option;
};

struct qmi_wlanfw_qdss_trace_mode_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLANFW_HOST_CAP_REQ_MSG_V01_MAX_LEN		194
#define QMI_WLANFW_HOST_CAP_REQ_V01			0x0034
#define QMI_WLANFW_HOST_CAP_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLFW_HOST_CAP_RESP_V01			0x0034
#define QMI_WLFW_MAX_NUM_GPIO_V01			32
#define QMI_IPQ8074_FW_MEM_MODE				0xFF
#define HOST_DDR_REGION_TYPE				0x1
#define BDF_MEM_REGION_TYPE				0x2
#define M3_DUMP_REGION_TYPE				0x3
#define CALDB_MEM_REGION_TYPE				0x4
#define QDSS_ETR_MEM_REGION_TYPE			0x6
#define PAGEABLE_MEM_REGION_TYPE			0x9
#define QMI_MEM_REGION_TYPE				0

struct qmi_wlanfw_host_cap_req_msg_v01 {
	u8 num_clients_valid;
	u32 num_clients;
	u8 wake_msi_valid;
	u32 wake_msi;
	u8 gpios_valid;
	u32 gpios_len;
	u32 gpios[QMI_WLFW_MAX_NUM_GPIO_V01];
	u8 nm_modem_valid;
	u8 nm_modem;
	u8 bdf_support_valid;
	u8 bdf_support;
	u8 bdf_cache_support_valid;
	u8 bdf_cache_support;
	u8 m3_support_valid;
	u8 m3_support;
	u8 m3_cache_support_valid;
	u8 m3_cache_support;
	u8 cal_filesys_support_valid;
	u8 cal_filesys_support;
	u8 cal_cache_support_valid;
	u8 cal_cache_support;
	u8 cal_done_valid;
	u8 cal_done;
	u8 mem_bucket_valid;
	u32 mem_bucket;
	u8 mem_cfg_mode_valid;
	u8 mem_cfg_mode;
	u8 cal_duration_valid;
	u16 cal_duration;
};

struct qmi_wlanfw_host_cap_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLANFW_IND_REGISTER_REQ_MSG_V01_MAX_LEN		66
#define QMI_WLANFW_IND_REGISTER_REQ_V01				0x0020
#define QMI_WLANFW_IND_REGISTER_RESP_MSG_V01_MAX_LEN		18
#define QMI_WLANFW_IND_REGISTER_RESP_V01			0x0020
#define QMI_WLANFW_CLIENT_ID					0x4b4e454c

struct qmi_wlanfw_ind_register_req_msg_v01 {
	u8 fw_ready_enable_valid;
	u8 fw_ready_enable;
	u8 initiate_cal_download_enable_valid;
	u8 initiate_cal_download_enable;
	u8 initiate_cal_update_enable_valid;
	u8 initiate_cal_update_enable;
	u8 msa_ready_enable_valid;
	u8 msa_ready_enable;
	u8 pin_connect_result_enable_valid;
	u8 pin_connect_result_enable;
	u8 client_id_valid;
	u32 client_id;
	u8 request_mem_enable_valid;
	u8 request_mem_enable;
	u8 fw_mem_ready_enable_valid;
	u8 fw_mem_ready_enable;
	u8 fw_init_done_enable_valid;
	u8 fw_init_done_enable;
	u8 rejuvenate_enable_valid;
	u32 rejuvenate_enable;
	u8 xo_cal_enable_valid;
	u8 xo_cal_enable;
	u8 cal_done_enable_valid;
	u8 cal_done_enable;
	u8 qdss_trace_req_mem_enable_valid;
	u8 qdss_trace_req_mem_enable;
	u8 qdss_trace_save_enable_valid;
	u8 qdss_trace_save_enable;
	u8 qdss_trace_free_enable_valid;
	u8 qdss_trace_free_enable;
	u8 respond_get_info_enable_valid;
	u8 respond_get_info_enable;
	u8 m3_dump_upload_req_enable_valid;
	u8 m3_dump_upload_req_enable;
};

struct qmi_wlanfw_ind_register_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 fw_status_valid;
	u64 fw_status;
};

#define QMI_WLANFW_REQUEST_MEM_IND_MSG_V01_MAX_LEN	1824
#define QMI_WLANFW_RESPOND_MEM_REQ_MSG_V01_MAX_LEN	888
#define QMI_WLANFW_RESPOND_MEM_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLANFW_REQUEST_MEM_IND_V01			0x0035
#define QMI_WLANFW_RESPOND_MEM_REQ_V01			0x0036
#define QMI_WLANFW_RESPOND_MEM_RESP_V01			0x0036
#define QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01            0x0040
#define QMI_WLANFW_MAX_NUM_MEM_CFG_V01			2
#define QMI_WLANFW_MAX_STR_LEN_V01                      16
#define QMI_WLANFW_MEM_WRITE_REQ_V01			0x0031
#define QMI_WLANFW_MEM_WRITE_REQ_MSG_V01_MAX_MSG_LEN	6163
#define QMI_WLANFW_MEM_READ_REQ_V01			0x0030
#define QMI_WLANFW_MEM_READ_REQ_MSG_V01_MAX_MSG_LEN	21

struct qmi_wlanfw_mem_cfg_s_v01 {
	u64 offset;
	u32 size;
	u8 secure_flag;
};

enum qmi_wlanfw_mem_type_enum_v01 {
	WLANFW_MEM_TYPE_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLANFW_MEM_TYPE_MSA_V01 = 0,
	QMI_WLANFW_MEM_TYPE_DDR_V01 = 1,
	QMI_WLANFW_MEM_BDF_V01 = 2,
	QMI_WLANFW_MEM_M3_V01 = 3,
	QMI_WLANFW_MEM_CAL_V01 = 4,
	QMI_WLANFW_MEM_DPD_V01 = 5,
	WLANFW_MEM_TYPE_ENUM_MAX_VAL_V01 = INT_MAX,
};

struct qmi_wlanfw_mem_seg_s_v01 {
	u32 size;
	enum qmi_wlanfw_mem_type_enum_v01 type;
	u32 mem_cfg_len;
	struct qmi_wlanfw_mem_cfg_s_v01 mem_cfg[QMI_WLANFW_MAX_NUM_MEM_CFG_V01];
};

struct qmi_wlanfw_request_mem_ind_msg_v01 {
	u32 mem_seg_len;
	struct qmi_wlanfw_mem_seg_s_v01 mem_seg[ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
};

struct qmi_wlanfw_mem_seg_resp_s_v01 {
	u64 addr;
	u32 size;
	enum qmi_wlanfw_mem_type_enum_v01 type;
	u8 restore;
};

struct qmi_wlanfw_respond_mem_req_msg_v01 {
	u32 mem_seg_len;
	struct qmi_wlanfw_mem_seg_resp_s_v01 mem_seg[ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
};

struct qmi_wlanfw_respond_mem_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

struct qmi_wlanfw_fw_mem_ready_ind_msg_v01 {
	char placeholder;
};

struct qmi_wlanfw_fw_ready_ind_msg_v01 {
	char placeholder;
};

struct qmi_wlanfw_fw_cold_cal_done_ind_msg_v01 {
	char placeholder;
};

struct qmi_wlfw_fw_init_done_ind_msg_v01 {
	char placeholder;
};

struct qmi_wlanfw_m3_dump_upload_req_ind_msg_v01 {
	u32 pdev_id;
	u64 addr;
	u64 size;
};

struct qmi_wlanfw_qdss_trace_save_ind_msg_v01 {
	u32 source;
	u32 total_size;
	u8 mem_seg_valid;
	u32 mem_seg_len;
	struct qmi_wlanfw_mem_seg_resp_s_v01 mem_seg[ATH11K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
	u8 file_name_valid;
	char file_name[QMI_WLANFW_MAX_STR_LEN_V01 + 1];
};

#define QDSS_TRACE_SEG_LEN_MAX 32

struct qdss_trace_mem_seg {
	u64 addr;
	u32 size;
};

struct ath11k_qmi_event_qdss_trace_save_data {
	u32 total_size;
	u32 mem_seg_len;
	struct qdss_trace_mem_seg mem_seg[QDSS_TRACE_SEG_LEN_MAX];
};

#define QMI_WLANFW_CAP_REQ_MSG_V01_MAX_LEN		0
#define QMI_WLANFW_CAP_RESP_MSG_V01_MAX_LEN		235
#define QMI_WLANFW_CAP_REQ_V01				0x0024
#define QMI_WLANFW_CAP_RESP_V01				0x0024
#define QMI_WLANFW_DEVICE_INFO_REQ_V01			0x004C
#define QMI_WLANFW_DEVICE_INFO_REQ_MSG_V01_MAX_LEN	0

enum qmi_wlanfw_pipedir_enum_v01 {
	QMI_WLFW_PIPEDIR_NONE_V01 = 0,
	QMI_WLFW_PIPEDIR_IN_V01 = 1,
	QMI_WLFW_PIPEDIR_OUT_V01 = 2,
	QMI_WLFW_PIPEDIR_INOUT_V01 = 3,
};

struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01 {
	__le32 pipe_num;
	__le32 pipe_dir;
	__le32 nentries;
	__le32 nbytes_max;
	__le32 flags;
};

struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01 {
	__le32 service_id;
	__le32 pipe_dir;
	__le32 pipe_num;
};

struct qmi_wlanfw_shadow_reg_cfg_s_v01 {
	u16 id;
	u16 offset;
};

struct qmi_wlanfw_shadow_reg_v2_cfg_s_v01 {
	u32 addr;
};

struct qmi_wlanfw_memory_region_info_s_v01 {
	u64 region_addr;
	u32 size;
	u8 secure_flag;
};

struct qmi_wlanfw_rf_chip_info_s_v01 {
	u32 chip_id;
	u32 chip_family;
};

struct qmi_wlanfw_rf_board_info_s_v01 {
	u32 board_id;
};

struct qmi_wlanfw_soc_info_s_v01 {
	u32 soc_id;
};

struct qmi_wlanfw_fw_version_info_s_v01 {
	u32 fw_version;
	char fw_build_timestamp[ATH11K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01 + 1];
};

enum qmi_wlanfw_cal_temp_id_enum_v01 {
	QMI_WLANFW_CAL_TEMP_IDX_0_V01 = 0,
	QMI_WLANFW_CAL_TEMP_IDX_1_V01 = 1,
	QMI_WLANFW_CAL_TEMP_IDX_2_V01 = 2,
	QMI_WLANFW_CAL_TEMP_IDX_3_V01 = 3,
	QMI_WLANFW_CAL_TEMP_IDX_4_V01 = 4,
	QMI_WLANFW_CAL_TEMP_ID_MAX_V01 = 0xFF,
};

struct qmi_wlanfw_cap_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 chip_info_valid;
	struct qmi_wlanfw_rf_chip_info_s_v01 chip_info;
	u8 board_info_valid;
	struct qmi_wlanfw_rf_board_info_s_v01 board_info;
	u8 soc_info_valid;
	struct qmi_wlanfw_soc_info_s_v01 soc_info;
	u8 fw_version_info_valid;
	struct qmi_wlanfw_fw_version_info_s_v01 fw_version_info;
	u8 fw_build_id_valid;
	char fw_build_id[ATH11K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01 + 1];
	u8 num_macs_valid;
	u8 num_macs;
	u8 voltage_mv_valid;
	u32 voltage_mv;
	u8 time_freq_hz_valid;
	u32 time_freq_hz;
	u8 otp_version_valid;
	u32 otp_version;
	u8 eeprom_read_timeout_valid;
	u32 eeprom_read_timeout;
	u8 regdb_support_valid;
	u8 regdb_support;
};

struct qmi_wlanfw_cap_req_msg_v01 {
	char placeholder;
};

struct qmi_wlanfw_device_info_req_msg_v01 {
	char placeholder;
};

struct qmi_wlanfw_device_info_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u64 bar_addr;
	u32 bar_size;
	u8 bar_addr_valid;
	u8 bar_size_valid;
};

#define QMI_WLANFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_LEN	6182
#define QMI_WLANFW_BDF_DOWNLOAD_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLANFW_BDF_DOWNLOAD_RESP_V01		0x0025
#define QMI_WLANFW_BDF_DOWNLOAD_REQ_V01			0x0025
/* TODO: Need to check with MCL and FW team that data can be pointer and
 * can be last element in structure
 */
struct qmi_wlanfw_bdf_download_req_msg_v01 {
	u8 valid;
	u8 file_id_valid;
	enum qmi_wlanfw_cal_temp_id_enum_v01 file_id;
	u8 total_size_valid;
	u32 total_size;
	u8 seg_id_valid;
	u32 seg_id;
	u8 data_valid;
	u32 data_len;
	u8 data[QMI_WLANFW_MAX_DATA_SIZE_V01];
	u8 end_valid;
	u8 end;
	u8 bdf_type_valid;
	u8 bdf_type;

};

struct qmi_wlanfw_bdf_download_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLANFW_M3_INFO_REQ_MSG_V01_MAX_MSG_LEN	18
#define QMI_WLANFW_M3_INFO_RESP_MSG_V01_MAX_MSG_LEN	7
#define QMI_WLANFW_M3_INFO_RESP_V01		0x003C
#define QMI_WLANFW_M3_INFO_REQ_V01		0x003C

#define QMI_WLANFW_M3_DUMP_UPLOAD_DONE_REQ_MSG_V01_MAX_MSG_LEN	14

struct qmi_wlanfw_m3_info_req_msg_v01 {
	u64 addr;
	u32 size;
};

struct qmi_wlanfw_m3_info_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLANFW_WLAN_MODE_REQ_MSG_V01_MAX_LEN	11
#define QMI_WLANFW_WLAN_MODE_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLANFW_WLAN_CFG_REQ_MSG_V01_MAX_LEN		803
#define QMI_WLANFW_WLAN_CFG_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLANFW_WLAN_INI_REQ_MSG_V01_MAX_LEN		4
#define QMI_WLANFW_WLAN_MODE_REQ_V01			0x0022
#define QMI_WLANFW_WLAN_MODE_RESP_V01			0x0022
#define QMI_WLANFW_WLAN_CFG_REQ_V01			0x0023
#define QMI_WLANFW_WLAN_CFG_RESP_V01			0x0023
#define QMI_WLANFW_WLAN_INI_REQ_V01			0x002F
#define QMI_WLANFW_MAX_NUM_CE_V01			12
#define QMI_WLANFW_MAX_NUM_SVC_V01			24
#define QMI_WLANFW_MAX_NUM_SHADOW_REG_V01		24
#define QMI_WLANFW_MAX_NUM_SHADOW_REG_V2_V01		36

struct qmi_wlanfw_wlan_mode_req_msg_v01 {
	u32 mode;
	u8 hw_debug_valid;
	u8 hw_debug;
};

struct qmi_wlanfw_wlan_mode_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

struct qmi_wlanfw_wlan_cfg_req_msg_v01 {
	u8 host_version_valid;
	char host_version[QMI_WLANFW_MAX_STR_LEN_V01 + 1];
	u8  tgt_cfg_valid;
	u32  tgt_cfg_len;
	struct qmi_wlanfw_ce_tgt_pipe_cfg_s_v01
			tgt_cfg[QMI_WLANFW_MAX_NUM_CE_V01];
	u8  svc_cfg_valid;
	u32 svc_cfg_len;
	struct qmi_wlanfw_ce_svc_pipe_cfg_s_v01
			svc_cfg[QMI_WLANFW_MAX_NUM_SVC_V01];
	u8 shadow_reg_valid;
	u32 shadow_reg_len;
	struct qmi_wlanfw_shadow_reg_cfg_s_v01
		shadow_reg[QMI_WLANFW_MAX_NUM_SHADOW_REG_V01];
	u8 shadow_reg_v2_valid;
	u32 shadow_reg_v2_len;
	struct qmi_wlanfw_shadow_reg_v2_cfg_s_v01
		shadow_reg_v2[QMI_WLANFW_MAX_NUM_SHADOW_REG_V2_V01];
};

struct qmi_wlanfw_wlan_cfg_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

struct qmi_wlanfw_wlan_ini_req_msg_v01 {
	/* Must be set to true if enablefwlog is being passed */
	u8 enablefwlog_valid;
	u8 enablefwlog;
};

struct qmi_wlanfw_wlan_ini_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

struct qmi_wlanfw_mem_read_req_msg_v01 {
	u32 offset;
	u32 mem_type;
	u32 data_len;
};

struct qmi_wlanfw_mem_read_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 data_valid;
	u32 data_len;
	u8 data[QMI_WLANFW_MAX_DATA_SIZE_V01];
};

struct qmi_wlanfw_mem_write_req_msg_v01 {
	u32 offset;
	u32 mem_type;
	u32 data_len;
	u8 data[QMI_WLANFW_MAX_DATA_SIZE_V01];
};

struct qmi_wlanfw_mem_write_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

int ath11k_qmi_firmware_start(struct ath11k_base *ab,
			      u32 mode);
void ath11k_qmi_firmware_stop(struct ath11k_base *ab);
void ath11k_qmi_deinit_service(struct ath11k_base *ab);
int ath11k_qmi_init_service(struct ath11k_base *ab);
void ath11k_qmi_free_resource(struct ath11k_base *ab);
int ath11k_qmi_fwreset_from_cold_boot(struct ath11k_base *ab);
int wlfw_send_qdss_trace_config_download_req(struct ath11k_base *ab,
		const u8 *buffer, unsigned int len);
int ath11k_send_qdss_trace_mode_req(struct ath11k_base *ab,
		enum wlfw_qdss_trace_mode_enum_v01 mode);
int ath11k_enable_fwlog(struct ath11k_base *ab);
int ath11k_qmi_mem_read(struct ath11k_base *ab, u32 mem_addr, void *mem_value, size_t count);
int ath11k_qmi_mem_write(struct ath11k_base *ab, u32 mem_addr, void* mem_value, size_t count);
void ath11k_qmi_free_target_mem_chunk(struct ath11k_base *ab);
#endif
