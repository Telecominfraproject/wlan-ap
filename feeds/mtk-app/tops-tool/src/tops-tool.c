// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 MediaTek Inc. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"
#include "tops-tool.h"

static struct tops_tool_cmd cmds[] = {
	TOPS_TOOL_CMD(SAVE_DUMP)
	TOPS_TOOL_CMD(SAVE_LOG)
};

static void print_usage(void)
{
	unsigned int idx;

	printf("Usage:\n");
	printf("tops-tool [CMD]\n");
	printf("[CMD] are:\n");
	for (idx = 0; idx < ARRAY_SIZE(cmds); idx++)
		printf("\t%s\n\t%s\n\n", cmds[idx].usage, cmds[idx].desc);
}

static int verify_parameters(int argc, char *argv[], unsigned int *cmd_idx)
{
	unsigned int idx;
	char *cmd;

	if (argc < 2) {
		fprintf(stderr, LOG_FMT("CMD missing\n"));
		return -EINVAL;
	}

	cmd = argv[1];
	for (idx = 0; idx < ARRAY_SIZE(cmds); idx++) {
		if (!strncmp(cmds[idx].name, cmd, strlen(cmds[idx].name))) {
			if (argc - 2 < cmds[idx].num_of_parms) {
				fprintf(stderr,
					LOG_FMT("CMD(%s) needs %d parameter(s)\n"),
					cmds[idx].name,
					cmds[idx].num_of_parms);
				return -EINVAL;
			}

			*cmd_idx = idx;

			return 0;
		}
	}

	fprintf(stderr, LOG_FMT("CMD(%s) not support\n"), cmd);

	return -EINVAL;
}

int main(int argc, char *argv[])
{
	unsigned int cmd_idx;
	int ret = 0;

	ret = verify_parameters(argc, argv, &cmd_idx);
	if (ret) {
		print_usage();
		goto error;
	}

	ret = cmds[cmd_idx].func(argc, argv);
	if (ret) {
		fprintf(stderr,
			LOG_FMT("CMD(%s) execution failed(%d)\n"),
			cmds[cmd_idx].name, ret);
		goto error;
	}

error:
	return ret;
}
