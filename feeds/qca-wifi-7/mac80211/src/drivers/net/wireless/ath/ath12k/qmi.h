/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_QMI_H
#define ATH12K_QMI_H

#include <linux/mutex.h>
#include <linux/soc/qcom/qmi.h>

#define ATH12K_HOST_VERSION_STRING		"WIN"
#define ATH12K_QMI_WLANFW_TIMEOUT_MS		10000
#define ATH12K_QMI_MAX_BDF_FILE_NAME_SIZE	64
#define ATH12K_QMI_CALDB_ADDRESS		0x4BA00000
#define ATH12K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01	128
#define ATH12K_QMI_WLFW_SERVICE_ID_V01		0x45
#define ATH12K_QMI_WLFW_SERVICE_VERS_V01	0x01
#define ATH12K_QMI_WLFW_SERVICE_INS_ID_V01	0x02
#define ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_WCN7850 0x1

#define ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN9274	0x07
#define ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_IPQ5332	0x2
#define ATH12K_QMI_WLFW_SERVICE_INS_ID_V01_QCN6432	0x60
#define ATH12K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01	32
#define ATH12K_QMI_RESP_LEN_MAX			8192
#define ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01	52
#define ATH12K_QMI_CALDB_SIZE			0x480000
#define ATH12K_QMI_BDF_EXT_STR_LENGTH		0x20
#define ATH12K_QMI_FW_MEM_REQ_SEGMENT_CNT	3
#define ATH12K_QMI_WLFW_MAX_DEV_MEM_NUM_V01 4
#define ATH12K_QMI_DEVMEM_CMEM_INDEX	0
#define ATH12K_QMI_M3_DUMP_SIZE                 0x100000

#define QMI_WLFW_REQUEST_MEM_IND_V01		0x0035
#define QMI_WLFW_FW_MEM_READY_IND_V01		0x0037
#define QMI_WLFW_COLD_BOOT_CAL_DONE_IND_V01	0x0021
#define QMI_WLFW_FW_READY_IND_V01		0x0038
#define QMI_WLFW_M3_DUMP_UPLOAD_REQ_IND_V01    	0x004D
#define QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01   	0x004E

#define QMI_WLANFW_MAX_DATA_SIZE_V01		6144
#define ATH12K_FIRMWARE_MODE_OFF		4
#define ATH12K_COLD_BOOT_FW_RESET_DELAY		(60 * HZ)

#define ATH12K_BOARD_ID_DEFAULT	0xFF

#define QCN6432_DEVICE_BAR_SIZE                0x200000
#define ATH12K_RCV_GIC_MSI_HDLR_DELAY          (3 * HZ)

#define AFC_SLOT_SIZE				0x1000
#define AFC_MAX_SLOT				2
#define AFC_MEM_SIZE				(AFC_SLOT_SIZE * AFC_MAX_SLOT)

struct ath12k_base;
struct ath12k_hw_group;

enum ath12k_target_mem_mode {
	ATH12K_QMI_TARGET_MEM_MODE_DEFAULT = 0,
	ATH12K_QMI_TARGET_MEM_MODE_512M,
};

enum ath12k_qmi_file_type {
	ATH12K_QMI_FILE_TYPE_BDF_GOLDEN	= 0,
	ATH12K_QMI_FILE_TYPE_CALDATA	= 2,
	ATH12K_QMI_FILE_TYPE_EEPROM	= 3,
	ATH12K_QMI_MAX_FILE_TYPE	= 4,
};

enum ath12k_qmi_bdf_type {
	ATH12K_QMI_BDF_TYPE_BIN			= 0,
	ATH12K_QMI_BDF_TYPE_ELF			= 1,
	ATH12K_QMI_BDF_TYPE_REGDB		= 4,
	ATH12K_QMI_BDF_TYPE_CALIBRATION		= 5,
	ATH12K_QMI_BDF_TYPE_RXGAINLUT		= 7,
};

enum ath12k_qmi_event_type {
	ATH12K_QMI_EVENT_SERVER_ARRIVE,
	ATH12K_QMI_EVENT_SERVER_EXIT,
	ATH12K_QMI_EVENT_REQUEST_MEM,
	ATH12K_QMI_EVENT_FW_MEM_READY,
	ATH12K_QMI_EVENT_FW_READY,
	ATH12K_QMI_EVENT_COLD_BOOT_CAL_START,
	ATH12K_QMI_EVENT_COLD_BOOT_CAL_DONE,
	ATH12K_QMI_EVENT_REGISTER_DRIVER,
	ATH12K_QMI_EVENT_UNREGISTER_DRIVER,
	ATH12K_QMI_EVENT_RECOVERY,
	ATH12K_QMI_EVENT_FORCE_FW_ASSERT,
	ATH12K_QMI_EVENT_POWER_UP,
	ATH12K_QMI_EVENT_POWER_DOWN,
	ATH12K_QMI_EVENT_HOST_CAP,
	ATH12K_QMI_EVENT_M3_DUMP_UPLOAD_REQ,
	ATH12K_QMI_EVENT_MAX,
};

struct ath12k_qmi_driver_event {
	struct list_head list;
	enum ath12k_qmi_event_type type;
	void *data;
};

struct ath12k_qmi_m3_dump_data {
	u32 pdev_id;
	u32 size;
	u64 timestamp;
	char *addr;
};

struct ath12k_qmi_ce_cfg {
	const struct ce_pipe_config *tgt_ce;
	int tgt_ce_len;
	const struct service_to_pipe *svc_to_ce_map;
	int svc_to_ce_map_len;
	const u8 *shadow_reg;
	int shadow_reg_len;
	u32 *shadow_reg_v3;
	int shadow_reg_v3_len;
};

struct ath12k_qmi_event_msg {
	struct list_head list;
	enum ath12k_qmi_event_type type;
};

struct target_mem_chunk {
	u32 size;
	u32 type;
	u32 prev_size;
	u32 prev_type;
	dma_addr_t paddr;
	union {
		void __iomem *ioaddr;
		void *addr;
	} v;
};

struct target_info {
	u32 chip_id;
	u32 chip_family;
	u32 board_id;
	u32 soc_id;
	u32 fw_version;
	u32 eeprom_caldata;
	char fw_build_timestamp[ATH12K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01 + 1];
	char fw_build_id[ATH12K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01 + 1];
	char bdf_ext[ATH12K_QMI_BDF_EXT_STR_LENGTH];
};

struct m3_mem_region {
	u32 size;
	dma_addr_t paddr;
	void *vaddr;
};

struct dev_mem_info {
	u64 start;
	u64 size;
};

struct ath12k_qmi {
	struct ath12k_base *ab;
	struct qmi_handle handle;
	struct sockaddr_qrtr sq;
	struct work_struct event_work;
	struct workqueue_struct *event_wq;
	struct list_head event_list;
	spinlock_t event_lock; /* spinlock for qmi event list */
	struct ath12k_qmi_ce_cfg ce_cfg;
	struct target_mem_chunk target_mem[ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
	u32 mem_seg_count;
	u32 target_mem_mode;
	bool target_mem_delayed;
	u8 cal_done;
	u8 cal_timeout;
	/* protected with struct ath12k_qmi::event_lock */
	bool block_event;

	u8 num_radios;
	struct target_info target;
	struct m3_mem_region m3_mem;
	unsigned int service_ins_id;
	wait_queue_head_t cold_boot_waitq;
	struct dev_mem_info dev_mem[ATH12K_QMI_WLFW_MAX_DEV_MEM_NUM_V01];
};

struct ath12k_qmi_m3_dump_upload_req_data {
        u32 pdev_id;
        u64 addr;
        u64 size;
};

#define QMI_WLANFW_HOST_CAP_REQ_MSG_V01_MAX_LEN		355

struct qmi_wlanfw_m3_dump_upload_done_req_msg_v01 {
	u32 pdev_id;
	u32 status;
};

struct qmi_wlanfw_m3_dump_upload_done_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLANFW_HOST_CAP_REQ_V01			0x0034
#define QMI_WLANFW_HOST_CAP_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLFW_HOST_CAP_RESP_V01			0x0034
#define QMI_WLFW_MAX_NUM_GPIO_V01			32
#define QMI_WLANFW_MAX_PLATFORM_NAME_LEN_V01		64
#define QMI_WLANFW_MAX_HOST_DDR_RANGE_SIZE_V01		3
#define AFC_SLOT_SIZE					0x1000
#define AFC_MAX_SLOT					2
#define AFC_MEM_SIZE					(AFC_SLOT_SIZE * AFC_MAX_SLOT)

struct qmi_wlanfw_host_ddr_range {
	u64 start;
	u64 size;
};

enum ath12k_qmi_target_mem {
	HOST_DDR_REGION_TYPE = 0x1,
	BDF_MEM_REGION_TYPE = 0x2,
	M3_DUMP_REGION_TYPE = 0x3,
	CALDB_MEM_REGION_TYPE = 0x4,
	MLO_GLOBAL_MEM_REGION_TYPE = 0x8,
	PAGEABLE_MEM_REGION_TYPE = 0x9,
	AFC_REGION_TYPE = 0xA,
};

enum qmi_wlanfw_host_build_type {
	WLANFW_HOST_BUILD_TYPE_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLANFW_HOST_BUILD_TYPE_UNSPECIFIED_V01 = 0,
	QMI_WLANFW_HOST_BUILD_TYPE_PRIMARY_V01 = 1,
	QMI_WLANFW_HOST_BUILD_TYPE_SECONDARY_V01 = 2,
	WLANFW_HOST_BUILD_TYPE_ENUM_MAX_VAL_V01 = INT_MAX,
};

#define QMI_WLFW_MAX_NUM_MLO_CHIPS_V01 4
#define QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01 2
#define QMI_WLFW_MAX_NUM_MLO_ADJ_CHIPS_V01 2

struct wlfw_host_mlo_chip_info_s_v01 {
	u8 chip_id;
	u8 num_local_links;
	u8 hw_link_id[QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01];
	u8 valid_mlo_link_id[QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01];
};

struct wlfw_host_mlo_chip_info_s_v02 {
	struct wlfw_host_mlo_chip_info_s_v01 mlo_chip_info;
	u8 num_adj_chips;
	struct wlfw_host_mlo_chip_info_s_v01 mlo_adj_chip_info[QMI_WLFW_MAX_NUM_MLO_ADJ_CHIPS_V01];
};

#define QMI_WLFW_MLO_RECONFIG_INFO_REQ_V01 0x005F
#define QMI_WLFW_MLO_RECONFIG_INFO_RESP_V01 0x005F
#define WLFW_MLO_RECONFIG_INFO_REQ_MSG_V01_MAX_MSG_LEN 122

struct qmi_wlanfw_mlo_reconfig_info_req_msg_v01 {
	u8 mlo_capable_valid;
	u8 mlo_capable;
	u8 mlo_chip_id_valid;
	u16 mlo_chip_id;
	u8 mlo_group_id_valid;
	u8 mlo_group_id;
	u8 max_mlo_peer_valid;
	u16 max_mlo_peer;
	u8 mlo_num_chips_valid;
	u8 mlo_num_chips;
	u8 mlo_chip_info_valid;
	struct wlfw_host_mlo_chip_info_s_v01 mlo_chip_info[QMI_WLFW_MAX_NUM_MLO_CHIPS_V01];
	u8 mlo_chip_info_v2_valid;
	struct wlfw_host_mlo_chip_info_s_v02 mlo_chip_info_v2[QMI_WLFW_MAX_NUM_MLO_CHIPS_V01];
};

struct qmi_wlfw_mlo_reconfig_info_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

enum ath12k_qmi_cnss_feature {
	CNSS_FEATURE_MIN_ENUM_VAL_V01 = INT_MIN,
	CNSS_QDSS_CFG_MISS_V01 = 3,
	CNSS_PCIE_PERST_NO_PULL_V01 = 4,
	CNSS_MAX_FEATURE_V01 = 64,
	CNSS_FEATURE_MAX_ENUM_VAL_V01 = INT_MAX,
};

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
	u16 cal_duraiton;
	u8 platform_name_valid;
	char platform_name[QMI_WLANFW_MAX_PLATFORM_NAME_LEN_V01 + 1];
	u8 ddr_range_valid;
	struct qmi_wlanfw_host_ddr_range ddr_range[QMI_WLANFW_MAX_HOST_DDR_RANGE_SIZE_V01];
	u8 host_build_type_valid;
	enum qmi_wlanfw_host_build_type host_build_type;
	u8 mlo_capable_valid;
	u8 mlo_capable;
	u8 mlo_chip_id_valid;
	u16 mlo_chip_id;
	u8 mlo_group_id_valid;
	u8 mlo_group_id;
	u8 max_mlo_peer_valid;
	u16 max_mlo_peer;
	u8 mlo_num_chips_valid;
	u8 mlo_num_chips;
	u8 mlo_chip_info_valid;
	struct wlfw_host_mlo_chip_info_s_v01 mlo_chip_info[QMI_WLFW_MAX_NUM_MLO_CHIPS_V01];
	u8 mlo_chip_info_v2_valid;
	struct wlfw_host_mlo_chip_info_s_v02 mlo_chip_info_v2[QMI_WLFW_MAX_NUM_MLO_CHIPS_V01];
	u8 feature_list_valid;
	u64 feature_list;
	u8 fw_cfg_support_valid;
	u8 fw_cfg_support;
};

struct qmi_wlanfw_host_cap_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLANFW_PHY_CAP_REQ_MSG_V01_MAX_LEN		0
#define QMI_WLANFW_PHY_CAP_REQ_V01			0x0057
#define QMI_WLANFW_PHY_CAP_RESP_MSG_V01_MAX_LEN		18
#define QMI_WLANFW_PHY_CAP_RESP_V01			0x0057

struct qmi_wlanfw_phy_cap_req_msg_v01 {
};

struct qmi_wlanfw_phy_cap_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 num_phy_valid;
	u8 num_phy;
	u8 board_id_valid;
	u32 board_id;
	u8 single_chip_mlo_support_valid;
	u8 single_chip_mlo_support;
	u8 mm_coldboot_cal_valid;
	u8 mm_coldboot_cal;
};

#define QMI_WLANFW_IND_REGISTER_REQ_MSG_V01_MAX_LEN		54
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
#define QMI_WLANFW_MAX_NUM_MEM_CFG_V01			2
#define QMI_WLANFW_MAX_STR_LEN_V01                      16

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
	struct qmi_wlanfw_mem_seg_s_v01 mem_seg[ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
};

struct qmi_wlanfw_mem_seg_resp_s_v01 {
	u64 addr;
	u32 size;
	enum qmi_wlanfw_mem_type_enum_v01 type;
	u8 restore;
};

struct qmi_wlanfw_respond_mem_req_msg_v01 {
	u32 mem_seg_len;
	struct qmi_wlanfw_mem_seg_resp_s_v01 mem_seg[ATH12K_QMI_WLANFW_MAX_NUM_MEM_SEG_V01];
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

#define QMI_WLANFW_CAP_REQ_MSG_V01_MAX_LEN	0
#define QMI_WLANFW_CAP_RESP_MSG_V01_MAX_LEN	207
#define QMI_WLANFW_CAP_REQ_V01			0x0024
#define QMI_WLANFW_CAP_RESP_V01			0x0024
#define QMI_WLANFW_DEVICE_INFO_REQ_V01		0x004C
#define QMI_WLANFW_DEVICE_INFO_REQ_MSG_V01	0

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

struct qmi_wlanfw_shadow_reg_v3_cfg_s_v01 {
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
	char fw_build_timestamp[ATH12K_QMI_WLANFW_MAX_TIMESTAMP_LEN_V01 + 1];
};

struct qmi_wlanfw_dev_mem_info_s_v01 {
	u64 start;
	u64 size;
};

enum qmi_wlanfw_cal_temp_id_enum_v01 {
	QMI_WLANFW_CAL_TEMP_IDX_0_V01 = 0,
	QMI_WLANFW_CAL_TEMP_IDX_1_V01 = 1,
	QMI_WLANFW_CAL_TEMP_IDX_2_V01 = 2,
	QMI_WLANFW_CAL_TEMP_IDX_3_V01 = 3,
	QMI_WLANFW_CAL_TEMP_IDX_4_V01 = 4,
	QMI_WLANFW_CAL_TEMP_ID_MAX_V01 = 0xFF,
};

enum qmi_wlanfw_rd_card_chain_cap_v01 {
	WLFW_RD_CARD_CHAIN_CAP_MIN_VAL_V01 = INT_MIN,
	WLFW_RD_CARD_CHAIN_CAP_UNSPECIFIED_V01 = 0,
	WLFW_RD_CARD_CHAIN_CAP_1x1_V01 = 1,
	WLFW_RD_CARD_CHAIN_CAP_2x2_V01 = 2,
	WLFW_RD_CARD_CHAIN_CAP_MAX_VAL_V01 = INT_MAX,
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
	char fw_build_id[ATH12K_QMI_WLANFW_MAX_BUILD_ID_LEN_V01 + 1];
	u8 num_macs_valid;
	u8 num_macs;
	u8 voltage_mv_valid;
	u32 voltage_mv;
	u8 time_freq_hz_valid;
	u32 time_freq_hz;
	u8 otp_version_valid;
	u32 otp_version;
	u8 eeprom_caldata_read_timeout_valid;
	u32 eeprom_caldata_read_timeout;
	u8 fw_caps_valid;
	u64 fw_caps;
	u8 rd_card_chain_cap_valid;
	enum qmi_wlanfw_rd_card_chain_cap_v01 rd_card_chain_cap;
	u8 dev_mem_info_valid;
	struct qmi_wlanfw_dev_mem_info_s_v01 dev_mem[ATH12K_QMI_WLFW_MAX_DEV_MEM_NUM_V01];
	u8 rxgainlut_support_valid;
	u8 rxgainlut_support;
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

#define QMI_WLANFW_WLAN_MODE_REQ_MSG_V01_MAX_LEN	15
#define QMI_WLANFW_WLAN_MODE_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLANFW_WLAN_CFG_REQ_MSG_V01_MAX_LEN		803
#define QMI_WLANFW_WLAN_CFG_RESP_MSG_V01_MAX_LEN	7
#define QMI_WLANFW_WLAN_MODE_REQ_V01			0x0022
#define QMI_WLANFW_WLAN_MODE_RESP_V01			0x0022
#define QMI_WLANFW_WLAN_CFG_REQ_V01			0x0023
#define QMI_WLANFW_WLAN_CFG_RESP_V01			0x0023
#define QMI_WLANFW_MAX_STR_LEN_V01			16
#define QMI_WLANFW_MAX_NUM_CE_V01			12
#define QMI_WLANFW_MAX_NUM_SVC_V01			24
#define QMI_WLANFW_MAX_NUM_SHADOW_REG_V01		24
#define QMI_WLANFW_MAX_NUM_SHADOW_REG_V3_V01		60

struct qmi_wlanfw_wlan_mode_req_msg_v01 {
	u32 mode;
	u8 hw_debug_valid;
	u8 hw_debug;
	u8 do_coldboot_cal_valid;
	u8 do_coldboot_cal;
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
	u8 shadow_reg_v3_valid;
	u32 shadow_reg_v3_len;
	struct qmi_wlanfw_shadow_reg_v3_cfg_s_v01
		shadow_reg_v3[QMI_WLANFW_MAX_NUM_SHADOW_REG_V3_V01];
};

struct qmi_wlanfw_wlan_cfg_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define ATH12K_QMI_WLANFW_WLAN_INI_REQ_V01	0x002F
#define ATH12K_QMI_WLANFW_WLAN_INI_RESP_V01	0x002F
#define QMI_WLANFW_WLAN_INI_REQ_MSG_V01_MAX_LEN		7
#define QMI_WLANFW_WLAN_INI_RESP_MSG_V01_MAX_LEN	7

struct qmi_wlanfw_wlan_ini_req_msg_v01 {
	/* Must be set to true if enable_fwlog is being passed */
	u8 enable_fwlog_valid;
	u8 enable_fwlog;
};

struct qmi_wlanfw_wlan_ini_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

static inline void ath12k_qmi_set_event_block(struct ath12k_qmi *qmi, bool block)
{
	lockdep_assert_held(&qmi->event_lock);

	qmi->block_event = block;
}

static inline bool ath12k_qmi_get_event_block(struct ath12k_qmi *qmi)
{
	lockdep_assert_held(&qmi->event_lock);

	return qmi->block_event;
}

struct qmi_wlanfw_m3_dump_upload_req_ind_msg_v01 {
	u32 pdev_id;
	u64 addr;
	u64 size;
};

#define QMI_MEM_REGION_TYPE                             0
#define QMI_WLANFW_MEM_WRITE_REQ_V01                    0x0031
#define QMI_WLANFW_MEM_WRITE_REQ_MSG_V01_MAX_MSG_LEN    6163
#define QMI_WLANFW_MEM_READ_REQ_V01                     0x0030
#define QMI_WLANFW_MEM_READ_REQ_MSG_V01_MAX_MSG_LEN     21

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

#define QMI_WLANFW_CFG_DOWNLOAD_REQ_V01 0x0056
#define QMI_WLANFW_CFG_DOWNLOAD_RESP_V01 0x0056
enum wlanfw_cfg_type_v01 {
	WLANFW_CFG_TYPE_MIN_VAL_V01 = INT_MIN,
	WLANFW_CFG_FILE_V01 = 0,
	WLANFW_CFG_TYPE_MAX_VAL_V01 = INT_MAX,
};

#define WLANFW_CFG_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN 6174
struct wlanfw_cfg_download_req_msg_v01 {
	u8 file_type_valid;
	enum wlanfw_cfg_type_v01 file_type;
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

#define WLANFW_CFG_DOWNLOAD_RESP_MSG_V01_MAX_MSG_LEN 7
struct wlanfw_cfg_download_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

int ath12k_qmi_mem_read(struct ath12k_base *ab, u32 mem_addr, void *mem_value,size_t count);
int ath12k_qmi_mem_write(struct ath12k_base *ab, u32 mem_addr, void* mem_value, size_t count);

#define QMI_WLFW_INI_REQ_V01 0x002F
#define WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN 4

struct wlfw_ini_req_msg_v01 {
	u8 enablefwlog_valid;
	u8 enablefwlog;
};

struct wlfw_ini_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define QMI_WLFW_PARTNER_CHIP_STATE_INFO_REQ_V01 0x0066
#define QMI_WLFW_PARTNER_CHIP_STATE_INFO_RESP_V01 0x0066
#define WLFW_PARTNER_CHIP_STATE_INFO_REQ_MSG_V01_MAX_MSG_LEN 4
#define WLFW_PARTNER_CHIP_STATE_INFO_RESP_MSG_V01_MAX_LEN 7

struct qmi_wlanfw_chip_state_info_req_msg_v01 {
	u8 partner_chip_state_valid;
	u8 partner_chip_state;
};

struct qmi_wlanfw_chip_state_info_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

enum {
        ATH12K_MLO_SHMEM_TLV_STRUCT_MGMT_RX_REO_SNAPSHOT,
        ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_RX_REO_PER_LINK_SNAPSHOT_INFO,
        ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_RX_REO_SNAPSHOT_INFO,
        ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_LINK,
        ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_LINK_INFO,
        ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_H_SHMEM,
        ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_DEVICE_CRASH_INFO,
        ATH12K_MLO_SHMEM_TLV_STRUCT_MLO_GLB_PER_DEVICE_CRASH_INFO,
};

/**
 * Macros for getting and setting the required number of bits
 * from the TLV params.
 */
#define ATH12K_MLO_SHMEM_GET_BITS(_val, _index, _num_bits) \
        (((_val) >> (_index)) & ((1 << (_num_bits)) - 1))

#define MLO_SHMEM_CHIP_CRASH_INFO_PARAM_NO_OF_DEVICES_GET(device_info) \
        ATH12K_MLO_SHMEM_GET_BITS(device_info, 0, 2) + \
        (ATH12K_MLO_SHMEM_GET_BITS(device_info, 12, 4) << 2)
#define MLO_SHMEM_CHIP_CRASH_INFO_PARAM_VALID_DEVICE_BMAP_GET(device_info) \
        ATH12K_MLO_SHMEM_GET_BITS(device_info, 2, 8)
struct mlo_glb_device_crash_info {
	/**
	 * [1:0]:  no_of_devices
	 * [4:2]:  valid_devices_bmap
	 * For number of chips beyond 3, extension fields are added.
	 * [9:5]:  valid_devices_bmap_ext
	 * [15:12]: no_of_devices_ext
	 * [31:16]: reserved
	 */
	u32 device_info;
	/*
	 * This TLV is followed by array of mlo_glb_per_device_crash_info:
	 * mlo_glb_per_device_crash_info will have multiple instances equal to
	 * num of partner devices received by no_of_chips
	 * mlo_glb_per_device_crash_info per_device_crash_info[];
	 */
};

struct mlo_glb_per_device_crash_info {
	/*
	 * crash reason, takes value in enum ath12k_mlo_chip_crash_reason
         */
        u32 crash_reason;

        /*
         * recovery mode, takes value in enum ath12k_mlo_recovery_mode
         */
        u32 recovery_mode;
};

/**
 * ath12k_host_mlo_glb_per_device_crash_info - per chip crash
 * information in MLO global shared memory
 * @device_id: MLO chip id
 * @crash_reason: Address of the crash_reason corresponding to device_id
 * recovery_mode: Address of recovery mode corressponding to device_id
 */
struct ath12k_host_mlo_glb_per_device_crash_info {
        u8 device_id;
        void *crash_reason;
        void *recovery_mode;
};

/**
 * ath12k_host_mlo_glb_device_crash_info - chip crash information in MLO
 * global shared memory
 * @no_of_devices: No of partner chip to which crash information is shared
 * @valid_devices_bmap: Valid chip bitmap
 * @per_device_crash_info: pointer to per chip crash information.
 */
struct ath12k_host_mlo_glb_device_crash_info {
	u8 no_of_devices;
	u8 valid_devices_bmap;
	struct ath12k_host_mlo_glb_per_device_crash_info *per_device_crash_info;
};

/**
 * ath12k_host_mlo_mem_arena - MLO Global shared memory arena context
 * @global_device_crash_info: shared memory for crash info, recovery info
 * @init_done: Initialized snapshot info
 */
struct ath12k_host_mlo_mem_arena {
	struct ath12k_host_mlo_glb_device_crash_info global_device_crash_info;
        bool init_done;
};

struct ath12k_qmi_shmem_tlv_policy {
	size_t min_len;
};

int ath12k_qmi_firmware_start(struct ath12k_base *ab,
			      u32 mode);
void ath12k_qmi_firmware_stop(struct ath12k_base *ab);
int ath12k_qmi_partner_chip_power_info_send(struct ath12k_base *ab, u8 power_state);
void ath12k_qmi_deinit_service(struct ath12k_base *ab);
int ath12k_qmi_init_service(struct ath12k_base *ab);
int ath12k_qmi_fwreset_from_cold_boot(struct ath12k_base *ab);
void ath12k_qmi_free_resource(struct ath12k_base *ab);
void ath12k_qmi_trigger_host_cap(struct ath12k_base *ab);
void ath12k_qmi_trigger_mlo_reconfig(struct ath12k_base *ab);
void ath12k_qmi_reset_mlo_mem(struct ath12k_hw_group *ag);
int ath12k_qmi_m3_dump_upload_done_ind_send(struct ath12k_base *ab,
                                            u32 pdev_id, int status);
int ath12k_enable_fwlog(struct ath12k_base *ab);
int ath12k_qmi_mlo_global_snapshot_mem_init(struct ath12k_base *ab);
void ath12k_qmi_free_target_mem_chunk(struct ath12k_base *ab);
#endif
