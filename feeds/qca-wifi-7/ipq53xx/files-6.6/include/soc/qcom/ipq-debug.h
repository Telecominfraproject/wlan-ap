/* SPDX-License-Identifier: ISC */
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __IPQ_DEBUG_H__
#define __IPQ_DEBUG_H__

#define NON_SECURE_WATCHDOG             0x1
#define AHB_TIMEOUT                     0x3
#define NOC_ERROR                       0x6
#define SYSTEM_RESET_OR_REBOOT          0x10
#define POWER_ON_RESET                  0x20
#define SECURE_WATCHDOG                 0x23
#define HLOS_PANIC                      0x47
#define VFSM_RESET                      0x68
#define TME_L_FATAL_ERROR               0x49
#define TME_L_WDT_BITE_FATAL_ERROR      0x69

/* IPQ5424 specific restart reason codes */
#define IPQ5424_POWER_ON_RESET		0x1
#define IPQ5424_SYSTEM_RESET_OR_REBOOT	0x2
#define IPQ5424_TME_L_SECURE_WATCHDOG	0x3
#define IPQ5424_SECURE_WATCHDOG		0x4
#define IPQ5424_NON_SECURE_WATCHDOG	0x5
#define IPQ5424_HLOS_PANIC		0x6
#define IPQ5424_EXTERNAL_WDT		0x7
#define IPQ5424_TME_L_FORCE_RESET	0x8
#define IPQ5424_TSENS_HW_RESET		0x9
#define IPQ5424_AHB_TIMEOUT		0xA
#define IPQ5424_INTERNAL_Q6_CRASH	0xB
#define IPQ5424_TSENS_SW_RESET		0xE
#define IPQ5424_RESET_MAX		0xFF

#define RESET_REASON_MSG_MAX_LEN        100

int debug_log_reset_reason(unsigned int val);

struct restart_reason {
	void __iomem *wr_addr;
	struct notifier_block panic_blk;
	struct notifier_block	ssr_blk;
	struct notifier_block	atomic_ssr_blk;
	void *cookie;
	void *atomic_cookie;
	unsigned int reset_reason;
};

#endif /* __IPQ_DEBUG_H__ */
