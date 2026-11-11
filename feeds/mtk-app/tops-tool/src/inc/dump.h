/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2023 MediaTek Inc. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */

#ifndef __DUMP_H__
#define __DUMP_H__

#include <stdint.h>
#include <sys/types.h>

#define DUMP_INFO_NAME_MAX_LEN		32
#define RELAY_DUMP_SUBBUF_SIZE		2048
#define DUMP_DATA_PATH			"/sys/kernel/debug/tops/trm/dump-data"

struct dump_info {
	char name[DUMP_INFO_NAME_MAX_LEN];
	uint64_t dump_time_sec;
	uint32_t start_addr;
	uint32_t size;
	uint32_t dump_rsn;
#define DUMP_RSN_NULL				(0x0000)
#define DUMP_RSN_WDT_TIMEOUT_CORE0		(0x0001)
#define DUMP_RSN_WDT_TIMEOUT_CORE1		(0x0002)
#define DUMP_RSN_WDT_TIMEOUT_CORE2		(0x0004)
#define DUMP_RSN_WDT_TIMEOUT_CORE3		(0x0008)
#define DUMP_RSN_WDT_TIMEOUT_COREM		(0x0010)
#define DUMP_RSN_FE_RESET			(0x0020)
};

struct dump_data_header {
	struct dump_info info;
	uint32_t data_offset;
	uint32_t data_len;
	uint8_t last_frag;
};

int tops_tool_save_dump_data(int argc, char *argv[]);

#endif /* __DUMP_H__ */
