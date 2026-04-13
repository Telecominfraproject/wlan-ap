/*
 *Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __ATHDEBUG_SLAB_H
#define __ATHDEBUG_SLAB_H

#include <linux/ath_memdebug.h>
#include_next <linux/vmalloc.h>

#define kmalloc(len, flags)		ath_kmalloc(len, flags, __LINE__, __func__)
#define kmemdup(ptr, len, flags)	ath_kmemdup(ptr, len, flags, __LINE__, __func__)
#define kzalloc(len,flags)		ath_kzalloc(len, flags, __LINE__, __func__)
#define vmalloc(len)			ath_vmalloc(len, __LINE__, __func__)
#define kcalloc(n, len, flags)		ath_kcalloc(n, len, flags, __LINE__, __func__)
#define vzalloc(len)                    ath_vzalloc(len, __LINE__, __func__)
#define kfree(ptr)                      ath_kfree(ptr)
#define vfree(ptr)                      ath_vfree(ptr)
#define kfree_sensitive(ptr)            ath_kfree_sensitive(ptr)

#endif /* __ATHDEBUG_SLAB_H */
