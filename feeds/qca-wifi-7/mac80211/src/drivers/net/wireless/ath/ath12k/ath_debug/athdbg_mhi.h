/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef ATHDBG_MHI_H
#define ATHDBG_MHI_H

#include <linux/timer.h>

struct ath12k_base;

void athdbg_mhi_q6_dump_bl_sram_mem(struct ath12k_base *ab);
void athdbg_mhi_q6_boot_debug_timeout_hdlr_internal(struct ath12k_base *ab);

#endif /* ATHDBG_MHI_H */
