/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef __ATHMEMDEBUG_H
#define __ATHMEMDEBUG_H

struct sk_buff;
struct sk_buff_head;

void ath_minidump_update_alloc(void *ptr, size_t len, const char *struct_name,
			       const char *module_name);
void athmem_debug_print_rbtree(void);
void athmem_print_all_allocated_list(void);
void athmem_print_minidump_list(void);
void athmem_find_entry_in_rbtree(const char *struct_name);
void athmem_find_and_print_minidump_entry(const char *struct_name);
void athmem_enable_minidump_list(void);
void athmem_clear_memdebug_info(void);
char *ath_minidump_update_free(void *ptr);

void *athdbg_kmalloc(size_t size, gfp_t flags, const char *struct_name,
		     const char *module_name);
void *athdbg_kzalloc(size_t size, gfp_t flags, const char *struct_name,
		     const char *module_name);
void athdbg_kfree(const void *ptr);
void athmem_add_entry_to_minidump(void *start_addr, size_t size,
				  const char *struct_name,
				  const char *module_name);
void athmem_free_entry_in_minidump(const void *start_addr);
void athmem_clear_minidump_and_rb_tree(void);
#endif
