/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/**
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#define ATH11K_FW_CRASH_DUMP_VERSION 1

enum ath11k_fw_crash_dump_type {
	ATH11K_FW_CRASH_PAGING_DATA,
	ATH11K_FW_CRASH_RDDM_DATA,
	ATH11K_FW_REMOTE_MEM_DATA,
	ATH11K_FW_CRASH_DUMP_MAX,
	ATH11K_FW_QDSS_DATA,
};

struct ath11k_dump_segment {
	unsigned long addr;
	void *vaddr;
	unsigned int len;
	unsigned int type;
};

struct ath11k_dump_file_data {
	/* "ATH11K-FW-DUMP" */
	char df_magic[16];
	__le32 len;
	/* file dump version */
	__le32 version;
	/* pci device id */
	__le32 chip_id;
	/* qrtr instance id */
	__le32 qrtr_id;
	/* pci domain id */
	u8 bus_id;
	guid_t guid;
	/* time-of-day stamp */
	__le64 tv_sec;
	/* time-of-day stamp, nano-seconds */
	__le64 tv_nsec;
	/* room for growth w/out changing binary format */
	u8 unused[8];
	/* number of segments */
	__le32 num_seg;
	/* ath11k_dump_segment struct size */
	__le32 seg_size;

	struct ath11k_dump_segment *seg;
	/* struct ath11k_dump_segment + more */

	u8 data[0];
} __packed;

struct ath11k_coredump_state {
	struct ath11k_dump_file_data *header;
	struct ath11k_dump_segment *segments;
	struct completion dump_done;
	u32 num_seg;
};

struct ath11k_coredump_segment_info {
	u32 chip_id;
	u32 qrtr_id;
	u32 num_seg;
	struct ath11k_dump_segment *seg;
	u8 bus_id;
};

void ath11k_coredump_download_rddm(struct ath11k_base *ab);

#endif
