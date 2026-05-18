/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2023 MediaTek Incorporation. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <glob.h>

#define LOG_MGMT_RELAYFS_PATH		"/sys/kernel/debug/npu/tops/log-mgmt*"
#define LOG_MGMT_NAME			"log-mgmt"

#define LOG_OFFLOAD_RELAYFS_PATH	"/sys/kernel/debug/npu/tops/log-offload*"
#define LOG_OFFLOAD_NAME		"log-offload"

#define LOGGER_DEBUGFS_PATH		"/sys/kernel/debug/npu/tops/logger"

#define BUFFER_LEN			(0x1000)

enum log_num {
	LOG_NUM_MGMT = 0,
	LOG_NUM_OFFLOAD,

	__LOG_NUM_MAX
};
#define LOG_NUM_MAX			__LOG_NUM_MAX

struct logger_runtime_info {
	const char *relayfs_path;
	glob_t relayfs_glob;
	int relayfs_fd;
	const char *log_name;
	char *log_file_path;
};

int tops_tool_run_logger(int argc, char *argv[]);

#endif /* __LOGGER_H__ */
