// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _PKTLOG_H_
#define _PKTLOG_H_

#include "core.h"

#define PKTLOG_EXTRA_PAGES      2
#define CUR_PKTLOG_VER          10010  /* Packet log version */
#define PKTLOG_MAGIC_NUM        7735225
#define PKTLOG_NEW_MAGIC_NUM    2453506
#define PKTLOG_MAGIC_NUM_FW_VERSION_SUPPORT 0xDECDF1F0
#define ATH12K_DEBUGFS_PKTLOG_SIZE_DEFAULT (10 * 1024 * 1024)
#define ATH12K_PKTLOG_SIZE_MIN  (16 * 1024)
#define ATH12K_PKTLOG_SIZE_MAX  (50 * 1024 * 1024)
#define INVALID_OFFSET		((size_t)-1)

#define ATH12K_PKTLOG_HDR_FLAGS_MASK 0xffff
#define ATH12K_PKTLOG_HDR_FLAGS_SHIFT 0
#define ATH12K_PKTLOG_HDR_FLAGS_OFFSET 0
#define ATH12K_PKTLOG_HDR_MISSED_CNT_MASK 0xffff0000
#define ATH12K_PKTLOG_HDR_MISSED_CNT_SHIFT 16
#define ATH12K_PKTLOG_HDR_MISSED_CNT_OFFSET 0
#define ATH12K_PKTLOG_HDR_LOG_TYPE_MASK 0xffff
#define ATH12K_PKTLOG_HDR_LOG_TYPE_SHIFT 0
#define ATH12K_PKTLOG_HDR_LOG_TYPE_OFFSET 1
#define ATH12K_PKTLOG_HDR_SIZE_MASK 0xffff0000
#define ATH12K_PKTLOG_HDR_SIZE_SHIFT 16
#define ATH12K_PKTLOG_HDR_SIZE_OFFSET 1
#define ATH12K_PKTLOG_HDR_TIMESTAMP_OFFSET 2
#define ATH12K_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET 3

enum ath12k_pktlog_flag {
        PKTLOG_FLG_FRM_TYPE_LOCAL_S = 0,
        PKTLOG_FLG_FRM_TYPE_REMOTE_S,
        PKTLOG_FLG_FRM_TYPE_CLONE_S,
        PKTLOG_FLG_FRM_TYPE_UNKNOWN_S
};

enum ath12k_pktlog_mode {
        ATH12K_PKTLOG_DISABLED,
        ATH12K_PKTLOG_MODE_LITE,
        ATH12K_PKTLOG_MODE_FULL,
        ATH12K_PKTLOG_MODE_INVALID,
};

enum ath12k_pktlog_filter {
        ATH12K_PKTLOG_RX                = BIT(0),
        ATH12K_PKTLOG_TX                = BIT(1),
        ATH12K_PKTLOG_RCFIND            = BIT(2),
        ATH12K_PKTLOG_RCUPDATE          = BIT(3),
        ATH12K_PKTLOG_DBG_PRINT         = BIT(4),
        ATH12K_PKTLOG_SMART_ANT         = BIT(5),
        ATH12K_PKTLOG_SW                = BIT(6),
        ATH12K_PKTLOG_PHY_LOGGING       = BIT(7),
        ATH12K_PKTLOG_CBF               = BIT(8),
        ATH12K_PKTLOG_HYBRID            = BIT(9),
};

enum ath12k_pktlog_enum {
        ATH12K_PKTLOG_TYPE_TX_CTRL      = 1,
        ATH12K_PKTLOG_TYPE_TX_STAT      = 2,
        ATH12K_PKTLOG_TYPE_TX_MSDU_ID   = 3,
        ATH12K_PKTLOG_TYPE_TX_FRM_HDR   = 4,
        ATH12K_PKTLOG_TYPE_RX_STAT      = 5,
        ATH12K_PKTLOG_TYPE_RC_FIND      = 6,
        ATH12K_PKTLOG_TYPE_RC_UPDATE    = 7,
        ATH12K_PKTLOG_TYPE_TX_VIRT_ADDR = 8,
        ATH12K_PKTLOG_TYPE_DBG_PRINT    = 9,
        ATH12K_PKTLOG_TYPE_RX_CBF       = 10,
        ATH12K_PKTLOG_TYPE_ANI          = 11,
        ATH12K_PKTLOG_TYPE_GRPID        = 12,
        ATH12K_PKTLOG_TYPE_TX_MU        = 13,
        ATH12K_PKTLOG_TYPE_SMART_ANTENNA = 14,
        ATH12K_PKTLOG_TYPE_TX_PFSCHED_CMD = 15,
        ATH12K_PKTLOG_TYPE_TX_FW_GENERATED1 = 19,
        ATH12K_PKTLOG_TYPE_TX_FW_GENERATED2 = 20,
        ATH12K_PKTLOG_TYPE_MAX = 21,
        ATH12K_PKTLOG_TYPE_RX_STATBUF   = 22,
        ATH12K_PKTLOG_TYPE_PPDU_STATS   = 23,
        ATH12K_PKTLOG_TYPE_LITE_RX      = 24,
        ATH12K_PKTLOG_TYPE_HOST_SW_EVENT = 30,
};

struct ath12k_pktlog_hdr_arg {
	u16 log_type;
	u8 *payload;
	u16 payload_size;
	u8 *pktlog_hdr;
};

struct ath12k_pktlog_bufhdr {
        u32 magic_num;  /* Used by post processing scripts */
        u32 version;    /* Set to CUR_PKTLOG_VER */
        u8 software_image[40];
        u8 chip_info[40];
        u32 pktlog_defs_json_version;
};

struct ath12k_pktlog_buf {
        struct ath12k_pktlog_bufhdr bufhdr;
        int rd_offset;
        int wr_offset;
        char log_data[0];
};

struct ath12k_pktlog {
        struct ath12k_pktlog_buf *buf;
        u32 filter;
        u32 buf_size;           /* Size of buffer in bytes */
        spinlock_t lock;
        u8 hdr_size;
        u8 hdr_size_field_offset;
        u32 fw_version_record;
        u32 invalid_decode_info;
};

struct ath12k_pktlog_decode_info {
        u8 software_image[40];
        u8 chip_info[40];
        u32 pktlog_defs_json_version;
};

struct ath12k_pl_fw_info {
        u32 pdev_id;
        u8 software_image[40];
        u8 chip_info[40];
        u32 pktlog_defs_json_version;
} __packed;
#endif /* _PKTLOG_H_ */
