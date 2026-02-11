/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ATH12K_DEBUG_H_
#define _ATH12K_DEBUG_H_

#include "trace.h"
#include "debugfs.h"

enum ath12k_debug_mask {
	ATH12K_DBG_AHB		= 0x00000001,
	ATH12K_DBG_WMI		= 0x00000002,
	ATH12K_DBG_HTC		= 0x00000004,
	ATH12K_DBG_DP_HTT	= 0x00000008,
	ATH12K_DBG_MAC		= 0x00000010,
	ATH12K_DBG_BOOT		= 0x00000020,
	ATH12K_DBG_QMI		= 0x00000040,
	ATH12K_DBG_DATA		= 0x00000080,
	ATH12K_DBG_MGMT		= 0x00000100,
	ATH12K_DBG_REG		= 0x00000200,
	ATH12K_DBG_TESTMODE	= 0x00000400,
	ATH12K_DBG_HAL		= 0x00000800,
	ATH12K_DBG_PCI		= 0x00001000,
	ATH12K_DBG_DP_TX	= 0x00002000,
	ATH12K_DBG_DP_RX	= 0x00004000,
	ATH12K_DBG_WOW		= 0x00008000,
	ATH12K_DBG_DP_FST       = 0x00010000,
	ATH12K_DBG_PEER         = 0x00020000,
	ATH12K_DBG_INI          = 0x00040000,
	ATH12K_DBG_QOS		= 0x00080000,
	ATH12K_DBG_AFC          = 0x04000000,
	ATH12K_DBG_DP_UMAC_RESET= 0x00100000,
	ATH12K_DBG_MODE1_RECOVERY= 0x00200000,
	ATH12K_DBG_TELEMETRY	= 0x00400000,
	ATH12K_DBG_RM           = 0x00800000,
	ATH12K_DBG_PPE          = 0x08000000,
	/* TODO: Delete this comment
	 * Package Upgrade: Parity with 13.0 */
	ATH12K_DBG_CFR          = 0x01000000,
	ATH12K_DBG_CFR_DUMP     = 0x02000000,
	ATH12K_DBG_WSI_BYPASS   = 0x04000000,
	ATH12K_DBG_MLME		= 0x10000000,
	ATH12K_DBG_EAPOL	= 0x20000000,
	ATH12K_DBG_CFG		= 0x40000000,
	ATH12K_DBG_DP_MON_RX	= 0x80000000,

	/* keep last*/
	ATH12K_DBG_ANY		= 0xffffffff,
};

enum ath12k_debug_mask_level {
	ATH12K_DBG_L0,
	ATH12K_DBG_L1,
	ATH12K_DBG_L2,
	ATH12K_DBG_L3,
};

__printf(2, 3) void ath12k_info(struct ath12k_base *ab, const char *fmt, ...);
__printf(2, 3) void ath12k_err(struct ath12k_base *ab, const char *fmt, ...);
__printf(2, 3) void __ath12k_warn(struct device *dev, const char *fmt, ...);

#define ath12k_warn(ab, fmt, ...) __ath12k_warn((ab)->dev, fmt, ##__VA_ARGS__)
#define ath12k_hw_warn(ah, fmt, ...) __ath12k_warn((ah)->dev, fmt, ##__VA_ARGS__)
#define ath12k_dbg_level(ab, dbg_mask, dbg_level, fmt, ...)		\
do {									\
	if (dbg_level && ath12k_debug_mask_level >= dbg_level)	\
		ath12k_dbg(ab, dbg_mask, fmt, ##__VA_ARGS__);		\
} while (0)

extern unsigned int ath12k_debug_mask;
extern unsigned int ath12k_debug_mask_level;
extern bool ath12k_ftm_mode;

#ifdef CPTCFG_ATH12K_DEBUG
__printf(3, 4) void __ath12k_dbg(struct ath12k_base *ab,
				 enum ath12k_debug_mask mask,
				 const char *fmt, ...);
void ath12k_dbg_dump(struct ath12k_base *ab,
		     enum ath12k_debug_mask mask,
		     const char *msg, const char *prefix,
		     const void *buf, size_t len);
#else /* CPTCFG_ATH12K_DEBUG */
static inline void __ath12k_dbg(struct ath12k_base *ab,
				enum ath12k_debug_mask dbg_mask,
				const char *fmt, ...)
{
}

static inline void ath12k_dbg_dump(struct ath12k_base *ab,
				   enum ath12k_debug_mask mask,
				   const char *msg, const char *prefix,
				   const void *buf, size_t len)
{
}
#endif /* CPTCFG_ATH12K_DEBUG */

#define ath12k_dbg(ab, dbg_mask, fmt, ...)			\
do {								\
	typeof(dbg_mask) mask = (dbg_mask);			\
	if (ath12k_debug_mask & mask)				\
		__ath12k_dbg(ab, mask, fmt, ##__VA_ARGS__);	\
} while (0)

#define ath12k_generic_dbg(dbg_mask, fmt, ...)			\
	ath12k_dbg(NULL, dbg_mask, fmt, ##__VA_ARGS__)

#endif /* _ATH12K_DEBUG_H_ */
