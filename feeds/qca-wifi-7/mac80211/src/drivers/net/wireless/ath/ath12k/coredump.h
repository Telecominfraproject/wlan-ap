/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _ATH12K_COREDUMP_H_
#define _ATH12K_COREDUMP_H_

#define ATH12K_FW_CRASH_DUMP_V2      2

#define MAX_RAMDUMP_TABLE_SIZE  6
#define COREDUMP_DESC           "Q6-COREDUMP"
#define Q6_SFR_DESC             "Q6-SFR"

#define DESC_STRING_SIZE 20
#define FILE_NAME_STRING_SIZE 20

enum ath12k_fw_crash_dump_type {
	FW_CRASH_DUMP_PAGING_DATA,
	FW_CRASH_DUMP_RDDM_DATA,
	FW_CRASH_DUMP_REMOTE_MEM_DATA,
	FW_CRASH_DUMP_PAGEABLE_DATA,
	FW_CRASH_DUMP_M3_DUMP,
	FW_CRASH_DUMP_QDSS_DATA,
	FW_CRASH_DUMP_CALDB_DATA,
	FW_CRASH_DUMP_AFC_DATA,
	FW_CRASH_DUMP_MLO_GLOBAL_DATA,
	FW_CRASH_DUMP_NONE,

	/* keep last */
	FW_CRASH_DUMP_TYPE_MAX,
};

#define COREDUMP_TLV_HDR_SIZE 8
#define ATH12K_RAMDUMP_MAGIC    0x574C414E  /* 'WLAN' */
#define ATH12K_MAX_DUMP_ENTRIES FW_CRASH_DUMP_TYPE_MAX

struct ath12k_dump_entry {
	u32 type;
	u32 entry_start;
	u32 entry_num;
} __packed;

struct ath12k_pci_dump_file_data {
	u32 magic;
	u32 version;
	u32 chipset;
	u32 total_entries;
	struct ath12k_dump_entry entry[];
} __packed;

struct ath12k_pci_elf_coredump_state {
	struct ath12k_base *ab;
	void *elf_hdr;
	u32   elf_hdr_sz;
	struct ath12k_dump_segment *chunks;
	u32   num_chunks;
	struct completion dump_done;
};


struct ath12k_dump_segment {
       unsigned long addr;
       void *vaddr;
       unsigned int len;
       unsigned int type;
	   struct completion dump_done;
};

struct ath12k_elf_coredump_state {
	struct ath12k_base *ab;
	void *header;
	struct ath12k_ahb_dump_segment *segments;
	struct completion dump_done;
	u32 num_seg;
};

struct ath12k_ahb_dump_segment {
	unsigned long addr;
	unsigned int len;
	void *hdr_vaddr;
	void *vaddr;
};

struct ath12k_dump_file_data {
       /* "ATH12K-FW-DUMP" */
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
       /* ath12k_dump_segment struct size */
       __le32 seg_size;

       struct ath12k_dump_segment *seg;
       /* struct ath12k_dump_segment + more */

       u8 data[0];
} __packed;

struct ath12k_coredump_state {
       struct ath12k_dump_file_data *header;
       struct ath12k_dump_segment *segments;
       struct completion dump_done;
       u32 num_seg;
};

struct ath12k_coredump_segment_info {
        u32 chip_id;
        u32 qrtr_id;
        u32 num_seg;
        struct ath12k_dump_segment *seg;
        u8 bus_id;
};

struct ath12k_coredump_info {
        atomic_t num_chip;
        struct ath12k_coredump_segment_info chip_seg_info[ATH12K_MAX_SOCS];
};

#ifdef CONFIG_WANT_DEV_COREDUMP
void ath12k_coredump_download_rddm(struct ath12k_base *ab);
void ath12k_coredump_build_inline(struct ath12k_base *ab,
                                 struct ath12k_dump_segment *segments, int num_seg);
void ath12k_coredump_dump_segment(struct ath12k_base *ab,
						    struct ath12k_dump_segment *segments, size_t seg_len);
void ath12k_coredump_ahb_collect(struct ath12k_base *ab);
void ath12k_coredump_m3_dump(struct ath12k_base *ab,
                            struct ath12k_qmi_m3_dump_upload_req_data *event_data);
#else
static inline void ath12k_coredump_download_rddm(struct ath12k_base *ab)
{
}

static inline void ath12k_coredump_build_inline(struct ath12k_base *ab,
                                               struct ath12k_dump_segment *segments,
                                               int num_seg)
{
}

static inline void ath12k_coredump_ahb_collect(struct ath12k_base *ab)
{
}
static inline void
ath12k_coredump_m3_dump(struct ath12k_base *ab,
                        struct ath12k_qmi_m3_dump_upload_req_data *event_data)
{
}
#endif
struct ath12k_tlv_dump_data {
	/* see ath11k_fw_crash_dump_type above */
	__le32 type;

	/* in bytes */
	__le32 tlv_len;

	/* pad to 32-bit boundaries as needed */
	u8 tlv_data[];
} __packed;

#define MAX_RAMDUMP_TABLE_SIZE  6
#define COREDUMP_DESC           "Q6-COREDUMP"
#define Q6_SFR_DESC             "Q6-SFR"

#define DESC_STRING_SIZE 20
#define FILE_NAME_STRING_SIZE 20

struct ath12k_coredump_q6ramdump_entry {
        __le64 base_address;
        __le64 actual_phys_address;
        __le64 size;
        char description[DESC_STRING_SIZE];
        char file_name[FILE_NAME_STRING_SIZE];
};

struct ath12k_coredump_q6ramdump_header {
        __le32 version;
        __le32 header_size;
        struct ath12k_coredump_q6ramdump_entry ramdump_table[MAX_RAMDUMP_TABLE_SIZE];
};

#ifdef CPTCFG_ATH12K_COREDUMP
enum ath12k_fw_crash_dump_type ath12k_coredump_get_dump_type
						(enum ath12k_qmi_target_mem type);
void ath12k_coredump_upload(struct work_struct *work);
void ath12k_coredump_collect(struct ath12k_base *ab);
#else
#ifdef CPTCFG_ATH12K_COREDUMP
static inline enum ath12k_fw_crash_dump_type ath12k_coredump_get_dump_type
							(enum ath12k_qmi_target_mem type)
{
	return FW_CRASH_DUMP_TYPE_MAX;
}
#endif
static inline void ath12k_coredump_upload(struct work_struct *work)
{
}

static inline void ath12k_coredump_collect(struct ath12k_base *ab)
{
}
#endif

#endif
