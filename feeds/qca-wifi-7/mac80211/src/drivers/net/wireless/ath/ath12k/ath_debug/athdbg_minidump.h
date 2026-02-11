/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef _ATHDBG_MINIDUMP_H_
#define _ATHDBG_MINIDUMP_H_

#include <linux/export.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include "athdbg_core.h"
#include "athdbg_mem.h"

enum athdbg_minidump_request {
	ATHDBG_REQ_INVALID = 0x00,
	ATHDBG_REQ_COLLECT_MINI_DUMP = 0x01,
	ATHDBG_REQ_SHOW_ALL_STRUCT = 0x02,
	ATHDBG_REQ_SHOW_DUMP_LIST = 0x03,
	ATHDBG_REQ_ADD_DUMP_LIST = 0x04,
	ATHDBG_REQ_DISABLE_MINIDUMP = 0x05,
};

enum athdbg_minidump_status {
	ENABLE_MINIDUMP,
	DISABLE_MINIDUMP,
};

struct minidump_file_handler {
	const char *filename;
	umode_t permissions;
	enum athdbg_minidump_request minidump_req;
};

extern struct minidump_file_handler athdbg_debugfs_handlers[];
extern struct ath_debug_base *athdbg_base;

struct athdbg_minidump_info {
	const char *struct_name;
	void *start_addr;
	struct list_head dump_list;
	struct list_head dup_list;

};

enum athdbg_minidump_request get_minidump_req(const char *filename);
struct ath_debug_base;
struct athdbg_request;
struct ath12k_base;

void athmem_find_and_add_entry_in_minidump(const char *struct_name);
struct athdbg_minidump_info *find_dump_node(const char *struct_name);
#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
void athdbg_process_minidump_request(struct ath12k_base *ab,
				     struct athdbg_request *dbg_req);
void athdbg_create_minidump_struct_list(void);
void athdbg_clear_minidump_struct_list(void);
void athdbg_create_minidump_debugfs(struct dentry *dbg_dir, struct ath12k_base *drv_ab);
void athdbg_collect_minidump(struct athdbg_request *dbg_req,
			     struct ath12k_base *ab);
void athdbg_do_dump_minidump(struct ath12k_base *ab);
void athdbg_collect_reference_segments(struct ath12k_base *ab);
void athdbg_free_reference_segments(struct ath12k_base *ab);
void athdbg_minidump_log(void *start_addr, size_t size, const char *struct_name,
			 const char *module_name);
void athdbg_add_to_minidump_log(void *start_addr, size_t size,
				const char *struct_name, const char *module_name);
void athdbg_remove_minidump_segment(void *start_addr);
void athdbg_iterate_minidump_list(void);
void athdbg_clear_minidump_info(void);
#else
static inline void athdbg_process_minidump_request(struct ath12k_base *ab,
						   struct athdbg_request *dbg_req)
{
}

static inline void athdbg_create_minidump_struct_list(void)
{
}

static inline void athdbg_clear_minidump_struct_list(void)
{
}

static inline void athdbg_create_minidump_debugfs(struct dentry *dbg_dir,
						  struct ath12k_base *drv_ab)
{
}

static inline void athdbg_collect_minidump(struct athdbg_request *dbg_req,
					   struct ath12k_base *ab)
{
}

static inline void athdbg_do_dump_minidump(struct ath12k_base *ab)
{
}

static inline void athdbg_collect_reference_segments(struct ath12k_base *ab)
{
}

static inline void athdbg_free_reference_segments(struct ath12k_base *ab)
{
}

static inline void athdbg_remove_minidump_segment(void *start_addr)
{
}

static inline void athdbg_minidump_log(void *start_addr, size_t size,
				       const char *struct_name,
				       const char *module_name)
{
}

static inline void athdbg_add_to_minidump_log(void *start_addr, size_t size,
					      const char *struct_name,
					      const char *module_name)
{
}

static inline void athdbg_iterate_minidump_list(void)
{
}

static inline void athdbg_clear_minidump_info(void)
{
}
#endif
#endif
