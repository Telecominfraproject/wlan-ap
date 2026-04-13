/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/types.h>
#include <linux/module.h>
#include "ath_debug/athdbg_mem.h"

#define kzalloc(__XX__, __YY__, ...) ({ \
	char *sname = #__VA_ARGS__;\
	void *allocaddr = athdbg_kzalloc(__XX__, __YY__, sname, THIS_MODULE->name); \
	allocaddr; \
})

#define kmalloc(__XX__, __YY__, ...) ({ \
	char *sname = #__VA_ARGS__;\
	void *allocaddr = athdbg_kmalloc(__XX__, __YY__, sname, THIS_MODULE->name); \
	allocaddr; \
})

#define kfree(__XX__)	athdbg_kfree(__XX__)

#define MINIDUMP_LOG(__XX__, __YY__, __ZZ__) \
	athmem_add_entry_to_minidump(__XX__, __YY__, __ZZ__, THIS_MODULE->name)

#define MINIDUMP_REMOVE(__XX__)	athmem_free_entry_in_minidump(__XX__)
