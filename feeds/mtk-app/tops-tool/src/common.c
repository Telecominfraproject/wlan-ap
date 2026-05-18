/* SPDX-License-Identifier:	GPL-2.0+ */
/*
 * Copyright (C) 2022 MediaTek Incorporation. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"

int time_to_str(time_t *time_sec, char *time_str, unsigned int time_str_size)
{
	struct tm *ptm;
	int ret;

	ptm = gmtime(time_sec);
	if (!ptm)
		return -1;

	ret = strftime(time_str, time_str_size, "%Y%m%d%H%M%S", ptm);
	if (!ret)
		return -2;

	return 0;
}

int mkdir_p(const char *path, mode_t mode)
{
	size_t path_len;
	char *cpy_path;
	char *cur_path;
	char *tmp_path;
	char *dir;
	int ret;

	path_len = strlen(path) + 1;
	if (path_len == 0)
		return -EINVAL;

	cpy_path = malloc(path_len);
	if (!cpy_path)
		return -ENOMEM;
	strncpy(cpy_path, path, path_len);

	cur_path = calloc(1, path_len);
	if (!cur_path) {
		ret = -ENOMEM;
		goto free_cpy_path;
	}

	tmp_path = malloc(path_len);
	if (!tmp_path) {
		ret = -ENOMEM;
		goto free_cur_path;
	}

	for (dir = strtok(cpy_path, "/");
	     dir != NULL;
	     dir = strtok(NULL, "/")) {
		/* keep current path */
		strncpy(tmp_path, cur_path, path_len);

		/* append directory in current path */
		ret = snprintf(cur_path, path_len, "%s/%s", tmp_path, dir);
		if (ret < 0) {
			fprintf(stderr,
				LOG_FMT("append dir(%s) in cur_path(%s) fail(%d)\n"),
				dir, cur_path, ret);
			goto free_tmp_path;
		}

		ret = mkdir(cur_path, mode);
		if (ret && errno != EEXIST) {
			fprintf(stderr,
				LOG_FMT("mkdir(%s) fail(%s)\n"),
				cur_path, strerror(errno));
			goto free_tmp_path;
		}
	}

	ret = 0;

free_tmp_path:
	free(tmp_path);

free_cur_path:
	free(cur_path);

free_cpy_path:
	free(cpy_path);

	return ret;
}
