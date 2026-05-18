/* SPDX-License-Identifier:	GPL-2.0+ */
/*
 * Copyright (C) 2023 MediaTek Incorporation. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */

#ifndef __TOPS_TOOL_H__
#define __TOPS_TOOL_H__

#include "tops-tool-cmds.h"

#define TOPS_TOOL_CMD(cmd_name)			TOPS_TOOL_CMD_ ## cmd_name

typedef int (*tops_tool_cmd_func_t)(int argc, char *argv[]);

struct tops_tool_cmd {
	char *name;
	char *usage;
	char *desc;
	tops_tool_cmd_func_t func;
	uint8_t num_of_parms;
};

#endif /* __TOPS_TOOL_H__ */
