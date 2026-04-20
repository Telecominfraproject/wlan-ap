/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __CTX_SAVE_H
#define __CTX_SAVE_H

#include <linux/module.h>

#define CTX_SAVE_SCM_TLV_TYPE_SIZE	1
#define CTX_SAVE_SCM_TLV_LEN_SIZE	2
#define CTX_SAVE_SCM_TLV_TYPE_LEN_SIZE (CTX_SAVE_SCM_TLV_TYPE_SIZE + CTX_SAVE_SCM_TLV_LEN_SIZE)
#define INVALID 0

/* Module Meta Data File is currently set to 50K size
 * by default, where (50K / 50) = 1024 entries can be supported.
 * To support max capacity of entries,please modify
 * METADATA_FILE_SZ based on minidump tlv size.
 *
 * MMU Meta Data File is currently set to 34K size
 * by default, where (34K / 34) = 1024 entries can be supported.
 * To support max capacity of entries , please modify
 * MMU_FILE_SZ based on minidump tlv size.
*/

#define METADATA_FILE_SZ 51200
#define METADATA_FILE_ENTRY_LEN 50
#define NAME_LEN 28
#define MINIDUMP_MODULE_COUNT 50

#define MMU_FILE_SZ 34816
#define MMU_FILE_ENTRY_LEN 34

/*
 * Note : This macro will be revoked once dependency host changes get merged
 */
#define QCA_WDT_LOG_DUMP_TYPE_WLAN_MOD 4

/* TLV_Types */
enum minidump_tlv_type {
	QCA_WDT_LOG_DUMP_TYPE_INVALID,
	QCA_WDT_LOG_DUMP_TYPE_UNAME,
	QCA_WDT_LOG_DUMP_TYPE_DMESG,
	QCA_WDT_LOG_DUMP_TYPE_LEVEL1_PT,
	QCA_WDT_LOG_DUMP_TYPE_MOD,
	QCA_WDT_LOG_DUMP_TYPE_MOD_DEBUGFS,
	QCA_WDT_LOG_DUMP_TYPE_MOD_INFO,
	QCA_WDT_LOG_DUMP_TYPE_MMU_INFO,
	QCA_WDT_LOG_DUMP_TYPE_TEXT_DATA_TAIL,
	QCA_WDT_LOG_DUMP_TYPE_EMPTY,
};

/* Crash_Types
 * Enum bit positions are updated from the least significant bit (LSB) to the
 * most significant bit (MSB). Segments with these bit positions set will be dumped,
 * based on crash information found in the dmesg log
 * Bit 0 - default segments
 * Bit 1 - on host crash
 * Bit 2 - on NSS crash
 * Bit 3 - on FW crash
 * Note : MINIDUMP_CRASH_TYPE_MAX - should be max value when all supported bit postition set true
 */

enum minidump_crash_type {
	MINIDUMP_CRASH_TYPE_LIVEDUMP = 0,
	MINIDUMP_CRASH_TYPE_DEFAULT = 1,
	MINIDUMP_CRASH_TYPE_HOST = 2,
	MINIDUMP_CRASH_TYPE_NSS = 4,
	MINIDUMP_CRASH_TYPE_FW = 8,
	MINIDUMP_CRASH_TYPE_MAX = 15,
};

int minidump_fill_segments_internal(const u64 start_addr, u64 size, enum minidump_tlv_type type,
				    const char *name, int islowmem,
				    enum minidump_crash_type crashtype);
int minidump_fill_segments(const u64 start_addr, u64 size, enum minidump_tlv_type type,
			   const char *name);
int minidump_add_segments(const u64 start_addr, u64 size, enum minidump_tlv_type type,
			  const char *name, enum minidump_crash_type crashtype,
			  const char *module_name);
int minidump_store_module_info(const char *name, const unsigned long va, const unsigned long pa,
			       enum minidump_tlv_type type);
int minidump_store_mmu_info(const unsigned long va, const unsigned long pa);
int minidump_remove_segments(const uint64_t virtual_address);
int do_dump_minidump(enum minidump_crash_type crashtype);
int do_minidump(void);
int minidump_dump_modules(void);
void minidump_get_dmesg_read_info(u64 *dmesg_tail_lpos, u64 *dmesg_tail_len);

struct module_sect_attr {
	struct bin_attribute battr;
	unsigned long address;
};

struct module_sect_attrs {
	struct attribute_group grp;
	unsigned int nsections;
	struct module_sect_attr attrs[0];
};

#endif
