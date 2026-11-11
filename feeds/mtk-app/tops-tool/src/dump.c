// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 MediaTek Inc. All Rights Reserved.
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
#include "dump.h"

static int save_dump_data(char *dump_root_dir,
			  struct dump_data_header *dd_hdr,
			  char *dd)
{
	size_t dump_file_size = dd_hdr->info.size + sizeof(struct dump_info);
	char dump_time_str[32];
	struct stat st = { 0 };
	char *dump_file = NULL;
	char *dump_dir = NULL;
	size_t path_len;
	int ret;
	int fd;

	ret = time_to_str((time_t *)&dd_hdr->info.dump_time_sec,
			  dump_time_str, sizeof(dump_time_str));
	if (ret < 0) {
		fprintf(stderr,
			LOG_FMT("time_to_str(%lu) fail(%d)\n"),
			dd_hdr->info.dump_time_sec, ret);
		return ret;
	}

	/* create the dump directory */
	path_len = strlen(dump_root_dir) + 1 + strlen(dump_time_str) + 1;
	dump_dir = malloc(path_len);
	if (!dump_dir)
		return -ENOMEM;

	ret = snprintf(dump_dir, path_len, "%s/%s", dump_root_dir, dump_time_str);
	if (ret < 0)
		goto free_dump_dir;

	ret = mkdir(dump_dir, 0775);
	if (ret && errno != EEXIST) {
		fprintf(stderr,
			LOG_FMT("mkdir(%s) fail(%s)\n"),
			dump_dir, strerror(errno));
		goto free_dump_dir;
	}

	/* TODO: only keep latest three dump directories */

	/* create the dump file */
	path_len = strlen(dump_dir) + 1 + strlen(dd_hdr->info.name) + 1;
	dump_file = malloc(path_len);
	if (!dump_file) {
		ret = -ENOMEM;
		goto free_dump_dir;
	}

	ret = snprintf(dump_file, path_len, "%s/%s", dump_dir, dd_hdr->info.name);
	if (ret < 0)
		goto free_dump_file;

	fd = open(dump_file, O_WRONLY | O_CREAT, 0664);
	if (fd < 0) {
		fprintf(stderr,
			LOG_FMT("open(%s) fail(%s)\n"),
			dump_file, strerror(errno));
		ret = fd;
		goto free_dump_file;
	}

	/* write the dump information at the begining of dump file */
	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr,
			LOG_FMT("lseek fail(%s)\n"),
			strerror(errno));
		goto close_dump_file;
	}

	write(fd, &dd_hdr->info, sizeof(struct dump_info));

	/* write the dump data start from dump information plus data offset */
	ret = lseek(fd, dd_hdr->data_offset, SEEK_CUR);
	if (ret < 0) {
		fprintf(stderr,
			LOG_FMT("lseek fail(%s)\n"),
			strerror(errno));
		goto close_dump_file;
	}

	write(fd, dd, dd_hdr->data_len);

	if (dd_hdr->last_frag) {
		ret = stat(dump_file, &st);
		if (ret < 0) {
			fprintf(stderr,
				LOG_FMT("stat(%s) fail(%s)\n"),
				dump_file, strerror(errno));
			goto close_dump_file;
		}

		if ((size_t)st.st_size != dump_file_size) {
			fprintf(stderr,
				LOG_FMT("file(%s) size %zu != %zu\n"),
				dump_file, st.st_size, dump_file_size);
			ret = -EINVAL;
			goto close_dump_file;
		}
	}

	ret = 0;

close_dump_file:
	close(fd);

free_dump_file:
	free(dump_file);

free_dump_dir:
	free(dump_dir);

	return ret;
}

static int read_retry(int fd, void *buf, int len)
{
	int out_len = 0;
	int ret;

	while (len > 0) {
		ret = read(fd, buf, len);
		if (ret < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return ret;
		}

		if (!ret)
			break;

		out_len += ret;
		len -= ret;
		buf += ret;
	}

	return out_len;
}

int tops_tool_save_dump_data(int argc, char *argv[])
{
	char *dump_root_dir = argv[2];
	int ret = 0;
	int fd;

	if (!dump_root_dir)
		return -EINVAL;

	/* reserve 256 bytes for saving name of dump directory and dump file */
	if (strlen(dump_root_dir) > (PATH_MAX - 256)) {
		fprintf(stderr,
			LOG_FMT("dump_root_dir(%s) length %zu > %u\n"),
			dump_root_dir, strlen(dump_root_dir), PATH_MAX - 256);
		return -EINVAL;
	}

	ret = mkdir_p(dump_root_dir, 0775);
	if (ret < 0) {
		fprintf(stderr,
			LOG_FMT("mkdir_p(%s) fail(%d)\n"),
			dump_root_dir, ret);
		return ret;
	}

	fd = open(DUMP_DATA_PATH, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr,
			LOG_FMT("open(%s) fail(%s)\n"),
			DUMP_DATA_PATH, strerror(errno));
		return fd;
	}

	while (1) {
		char dd[RELAY_DUMP_SUBBUF_SIZE - sizeof(struct dump_data_header)];
		struct dump_data_header dd_hdr;
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN | POLLHUP | POLLERR,
		};

		ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			fprintf(stderr,
				LOG_FMT("poll fail(%s)\n"),
				strerror(errno));
			break;
		}

		ret = read_retry(fd, &dd_hdr, sizeof(struct dump_data_header));
		if (ret < 0) {
			fprintf(stderr,
				LOG_FMT("read dd_hdr fail(%d)\n"), ret);
			break;
		}

		if (!ret)
			continue;

		if (dd_hdr.data_len == 0) {
			fprintf(stderr,
				LOG_FMT("read empty data\n"));
			continue;
		}

		if (dd_hdr.data_len > sizeof(dd)) {
			fprintf(stderr,
				LOG_FMT("data length %u > %lu\n"),
				dd_hdr.data_len, sizeof(dd));
			ret = -ENOMEM;
			break;
		}

		ret = read_retry(fd, dd, dd_hdr.data_len);
		if (ret < 0) {
			fprintf(stderr,
				LOG_FMT("read dd fail(%d)\n"), ret);
			break;
		}

		if ((uint32_t)ret != dd_hdr.data_len) {
			fprintf(stderr,
				LOG_FMT("read dd length %u != %u\n"),
				(uint32_t)ret, dd_hdr.data_len);
			ret = -EAGAIN;
			break;
		}

		ret = save_dump_data(dump_root_dir, &dd_hdr, dd);
		if (ret) {
			fprintf(stderr,
				LOG_FMT("save_dump_data(%s) fail(%d)\n"),
				dump_root_dir, ret);
			break;
		}
	}

	close(fd);

	return ret;
}
