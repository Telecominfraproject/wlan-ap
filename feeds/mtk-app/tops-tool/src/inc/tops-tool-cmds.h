/* SPDX-License-Identifier:	GPL-2.0+ */
/*
 * Copyright (C) 2023 MediaTek Incorporation. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */
#ifndef __TOPS_TOOL_CMDS_H__
#define __TOPS_TOOL_CMDS_H__

#include "dump.h"
#include "logger.h"

#define TOPS_TOOL_CMD_SAVE_DUMP						\
	{								\
		.name = "save_dump",					\
		.usage = "save_dump [DUMP DIRECTORY PATH]",		\
		.desc = "save dump data as file in dump directory",	\
		.func = tops_tool_save_dump_data,			\
		.num_of_parms = 1,					\
	},

#if defined(CONFIG_MTK_TOPS_TOOL_SAVE_LOG)
#define TOPS_TOOL_CMD_SAVE_LOG						\
	{								\
		.name = "save_log",					\
		.usage = "save_log [LOG DIRECTORY PATH]",		\
		.desc = "save log as file in log directory",		\
		.func = tops_tool_run_logger,				\
		.num_of_parms = 1,					\
	},
#else
#define TOPS_TOOL_CMD_SAVE_LOG
#endif
#endif /* __TOPS_TOOL_CMDS_H__ */
