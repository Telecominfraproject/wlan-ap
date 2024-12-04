/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/delay.h>
#include <linux/io.h>

/* CPU power domain register offsets */
#define CPU_HEAD_SWITCH_CTL 0x8
#define CPU_SEQ_FORCE_PWR_CTL_EN 0x1c
#define CPU_SEQ_FORCE_PWR_CTL_VAL 0x20
#define CPU_PCHANNEL_FSM_CTL 0x44

inline void a55ss_unclamp_cpu(void __iomem *reg)
{
	/* Program skew between en_few and en_rest to 40 XO clk cycles (~2us) */
	writel_relaxed(0x00000028, reg + CPU_HEAD_SWITCH_CTL);
	mb();

	/* Current sensors bypass */
	writel_relaxed(0x00000000, reg + CPU_SEQ_FORCE_PWR_CTL_EN);
	mb();

	/* Close Core logic head switch */
	writel_relaxed(0x00000642, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(4);

	/* Deassert Core Mem and Logic Clamp. (Clamp coremem is asserted by default) */
	writel_relaxed(0x00000402, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* De-Assert Core memory slp_nret_n */
	writel_relaxed(0x0000040A, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(8);

	/* De-Assert Core memory slp_ret_n */
	writel_relaxed(0x0000040E, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(8);

	/* Assert wl_en_clk */
	writel_relaxed(0x0000050E, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(2);

	/* De-assert wl_en_clk */
	writel_relaxed(0x0000040E, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* Deassert ClkOff */
	writel_relaxed(0x0000040C, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(4);

	/*Assert core pchannel power up request */
	writel_relaxed(0x00000001, reg + CPU_PCHANNEL_FSM_CTL);
	mb();

	/* Deassert Core reset */
	writel_relaxed(0x0000043C, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* Deassert core pchannel power up request */
	writel_relaxed(0x00000000, reg + CPU_PCHANNEL_FSM_CTL);
	mb();

	/* Indicate OSM that core is ACTIVE */
	writel_relaxed(0x0000443C, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* Assert CPU_PWRDUP */
	writel_relaxed(0x00000428, reg + CPU_HEAD_SWITCH_CTL);
	mb();
}
