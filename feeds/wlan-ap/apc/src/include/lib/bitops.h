/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef _APC_BITOPTS_H_
#define _APC_BITOPTS_H_

/*
 *	Bit mask operations:
 *
 *	u32_mkmask	Make bit mask consisting of <n> consecutive ones
 *			from the left and the rest filled with zeroes.
 *			E.g., u32_mkmask(5) = 0xf8000000.
 *	u32_masklen	Inverse operation to u32_mkmask, -1 if not a bitmask.
 */

u32 u32_mkmask(unsigned n);
int u32_masklen(u32 x);

#endif

