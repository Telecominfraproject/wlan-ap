/* SPDX-License-Identifier:	GPL-2.0+ */
/*
 * Copyright (C) 2023 MediaTek Incorporation. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */

#include <stdbool.h>
#include <signal.h>
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
#include "logger.h"

static struct logger_runtime_info runtime_infos[LOG_NUM_MAX] = {
	[LOG_NUM_MGMT] = {
		.relayfs_path = LOG_MGMT_RELAYFS_PATH,
		.log_name = LOG_MGMT_NAME,
	},
	[LOG_NUM_OFFLOAD] = {
		.relayfs_path = LOG_OFFLOAD_RELAYFS_PATH,
		.log_name = LOG_OFFLOAD_NAME,
	},
};

static bool sig_rcv;

static void __tops_logger_runtime_info_deinit(int log_num)
{
	struct logger_runtime_info *ri;

	for (; log_num >= 0; log_num--) {
		ri = &runtime_infos[log_num];

		free(ri->log_file_path);
		close(ri->relayfs_fd);
		globfree(&ri->relayfs_glob);
	}
}

static void tops_logger_runtime_info_deinit(void)
{
	__tops_logger_runtime_info_deinit(LOG_NUM_MAX - 1);
}

static int __tops_logger_runtime_info_init(struct logger_runtime_info *ri,
				   const char *log_file_dir,
				   char *log_time_str)
{
	int ret;

	ret = glob(ri->relayfs_path, 0, NULL, &ri->relayfs_glob);
	if (ret != 0) {
		fprintf(stderr,
			LOG_FMT("glob(%s) fail(%d)\n"),
			ri->relayfs_path, ret);
		goto out;
	} else if (ri->relayfs_glob.gl_pathc > 1) {
		fprintf(stderr,
			LOG_FMT("glob(%s) match %lu paths\n"),
			ri->relayfs_path, ri->relayfs_glob.gl_pathc);
		ret = -EINVAL;
		goto free_glob;
	}

	ri->relayfs_fd = open(ri->relayfs_glob.gl_pathv[0], O_RDONLY);
	if (ri->relayfs_fd < 0) {
		fprintf(stderr,
			LOG_FMT("open(%s) fail(%s)\n"),
			ri->relayfs_glob.gl_pathv[0], strerror(errno));
		ret = ri->relayfs_fd;
		goto free_glob;
	}

	ri->log_file_path = malloc(strlen(log_file_dir) + 1 +
				   strlen(ri->log_name) + 1 +
				   strlen(log_time_str) + 1);
	if (!ri->log_file_path) {
		ret = -ENOMEM;
		goto close_fd;
	}

	sprintf(ri->log_file_path, "%s/%s-%s", log_file_dir, ri->log_name, log_time_str);

out:
	return ret;

close_fd:
	close(ri->relayfs_fd);

free_glob:
	globfree(&ri->relayfs_glob);

	return ret;
}

static int tops_logger_runtime_info_init(const char *log_file_dir)
{
	char log_time_str[32];
	time_t log_time;
	int log_num;
	int ret;

	time(&log_time);
	ret = time_to_str(&log_time, log_time_str, sizeof(log_time_str));
	if (ret < 0) {
		fprintf(stderr,
			LOG_FMT("time_to_str(%lu) fail(%d)\n"),
			log_time, ret);
		goto out;
	}

	for (log_num = 0; log_num < LOG_NUM_MAX; log_num++) {
		ret = __tops_logger_runtime_info_init(
			&runtime_infos[log_num],
			log_file_dir,
			log_time_str);
		if (ret)
			goto deinit_logger_runtime_info;
	}

out:
	return ret;

deinit_logger_runtime_info:
	__tops_logger_runtime_info_deinit(log_num - 1);

	return ret;
}

static int tops_logger_log_save(char *file, char *data, uint32_t data_len)
{
	int ret = 0;
	int fd;

	fd = open(file, O_RDWR | O_APPEND | O_CREAT, 0664);
	if (fd < 0) {
		fprintf(stderr,
			LOG_FMT("open(%s) fail(%s)\n"),
			file, strerror(errno));
		ret = -1;
		goto out;
	}

	write(fd, data, data_len);
	close(fd);

out:
	return ret;
}

static int tops_logger_log_proc(struct pollfd *pfds)
{
	struct logger_runtime_info *ri;
	char buffer[BUFFER_LEN];
	int log_num;
	int ret = 0;

	for (log_num = 0; log_num < LOG_NUM_MAX; log_num++) {
		ri = &runtime_infos[log_num];
		if (pfds[log_num].revents & (POLLIN | POLLHUP | POLLERR)) {
			ret = read(pfds[log_num].fd, buffer, BUFFER_LEN);
			if (ret < 0) {
				fprintf(stderr,
					LOG_FMT("read log from %s failed(%d)\n"),
					ri->relayfs_glob.gl_pathv[0], ret);

				if (errno == EINTR || errno == EAGAIN)
					continue;

				break;
			} else if (ret == 0) {
				continue;
			}

			ret = tops_logger_log_save(ri->log_file_path,
						   buffer, ret);
			if (ret) {
				fprintf(stderr,
					LOG_FMT("save log to %s failed(%d)\n"),
					ri->log_file_path, ret);
				break;
			}
		}
	}

	return ret;
}

static void signal_handler(int sig)
{
	(void)sig;

	fprintf(stderr,
		LOG_FMT("killall or Ctrl+C received, stop saving log\n"));
	sig_rcv = true;
}

static int tops_logger_is_running(bool *running)
{
	char result[4];
	int ret = 0;
	ssize_t n;
	int fd;

	fd = open(LOGGER_DEBUGFS_PATH, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr,
			LOG_FMT("open(%s) fail(%s)\n"),
			LOGGER_DEBUGFS_PATH, strerror(errno));
		ret = errno;
		goto out;
	}

	n = read(fd, result, sizeof(result) - 1);
	if (n == -1) {
		fprintf(stderr,
			LOG_FMT("read(%s) fail(%s)\n"),
			LOGGER_DEBUGFS_PATH, strerror(errno));
		ret = errno;
		goto close_file;
	}

	result[n] = '\0';

	if (strcmp(result, "ON") == 0) {
		fprintf(stderr, LOG_FMT("logger is running\n"));
		*running = true;
	} else {
		*running = false;
	}

close_file:
	close(fd);

out:
	return ret;
}

int tops_tool_run_logger(int argc, char *argv[])
{
	const char *log_file_dir = argv[2];
	bool running = false;
	struct stat st;
	int ret;

	if (!log_file_dir) {
		ret = -EINVAL;
		goto out;
	}

	/* reserve 256 bytes for log file name */
	if (strlen(log_file_dir) > (PATH_MAX - 256)) {
		fprintf(stderr,
			LOG_FMT("log_file_dir(%s) length %zu > %u\n"),
			log_file_dir, strlen(log_file_dir), PATH_MAX - 256);
		ret = -EINVAL;
		goto out;
	}

	ret = tops_logger_is_running(&running);
	if (ret || running) {
		ret = -EINVAL;
		goto out;
	}

	if (stat(log_file_dir, &st)) {
		if (errno == ENOENT) {
			ret = mkdir_p(log_file_dir, 0775);
			if (ret) {
				fprintf(stderr,
					LOG_FMT("mkdir_p(%s) failed(%d)\n"),
					log_file_dir, ret);
				goto out;
			}
		} else {
			fprintf(stderr,
				LOG_FMT("stat(%s) failed(%s)\n"),
				log_file_dir, strerror(errno));
			ret = -EINVAL;
			goto out;
		}
	}

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);

	ret = tops_logger_runtime_info_init(log_file_dir);
	if (ret)
		goto out;

	system("echo ON > " LOGGER_DEBUGFS_PATH);

	while (1) {
		struct pollfd pfds[LOG_NUM_MAX] = {
			[LOG_NUM_MGMT] = {
				.fd = runtime_infos[LOG_NUM_MGMT].relayfs_fd,
				.events = POLLIN | POLLHUP | POLLERR,
				.revents = 0,
			},
			[LOG_NUM_OFFLOAD] = {
				.fd = runtime_infos[LOG_NUM_OFFLOAD].relayfs_fd,
				.events = POLLIN | POLLHUP | POLLERR,
				.revents = 0,
			},
		};

		if (sig_rcv)
			break;

		poll(pfds, LOG_NUM_MAX, -1);

		if (sig_rcv)
			break;

		ret = tops_logger_log_proc(pfds);
		if (ret)
			break;
	}

	system("echo OFF > " LOGGER_DEBUGFS_PATH);

	tops_logger_runtime_info_deinit();

out:
	return ret;
}
