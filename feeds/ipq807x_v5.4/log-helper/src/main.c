/*
 * Copyright (c) 2025, CyberTAN Technology Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions, and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of CyberTAN Technology Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>

#include <uci.h>
#include <libubus.h>

#define DEFAULT_BACKUP_DIRECTOR  "backup_storage"
#define DEFAULT_BACKUP_FILE_NAME "backup.tgz"
#define DEFAULT_MAX_ROTATIONS    3
#define DEFAULT_SPACE_THRESH     2048
#define UCI_CONFIG_FILE          "log-helper"
#define OVERLAYFS_ROOT           "/overlay/upper"



static int rotate_logs(const char *base_log_path, int max_rotations);
static int move_file(const char *src_path, const char *dest_dir);
static void do_backup(void);

static char backup_dir[1024];



/**
 * @brief Performs log rotation.
 * @param base_log_path The path of the base log file, e.g., "app.log".
 * @param max_rotations The maximum number of backup files to keep.
 * @return 0 on success, -1 on failure.
 */
static int rotate_logs(const char *base_log_path, int max_rotations) {
	struct stat info;
	char log_name[NAME_MAX] = {0};
	char log_path[1024] = {0};
	char src_path[PATH_MAX] = {0};
	char dest_path[PATH_MAX] ={0};
	int len;

	if (!strlen(backup_dir) || stat(backup_dir, &info) != 0) {
		syslog(LOG_ERR, "Wrong backup directory: %s", backup_dir);
		return -1;
	}

	if (max_rotations <= 1) {
		syslog(LOG_ERR, "max_rotations must be greater than 1: %d", max_rotations);
		return -1;
	}

	strncpy(log_name, basename((char *)base_log_path), sizeof(log_name));
	len = snprintf(log_path, sizeof(log_path), "%s/%s", backup_dir, log_name);
	if (len >= sizeof(log_path)) {
		syslog(LOG_ERR, "Buffer too small, path was truncated.");
		return -1;
	}

	if (access(log_path, F_OK) != 0) {
		syslog(LOG_DEBUG, "File not found: %s", log_path);
		return 0;
	}

	// Remove the oldest backup file (e.g., app.log.5).
	snprintf(dest_path, sizeof(dest_path), "%s.%d", log_path, max_rotations);
	if (remove(dest_path) == 0) {
		syslog(LOG_DEBUG, "Deleted oldest backup: %s", dest_path);
	}

	// Rename files in reverse order, from the second to last backup.
	for (int i = max_rotations - 1; i >= 0; --i) {
		// Generate the source file path.
		if (i) {
			// the source is the .i backup file.
			snprintf(src_path, sizeof(src_path) - 2, "%s.%d", log_path, i);
		} else {
			// the source is the original log file.
			src_path[strlen(log_path)] = '\0'; // Ensure null-termination.
		}

		// Generate the destination file path (i+1).
		snprintf(dest_path, sizeof(dest_path), "%s.%d", log_path, i + 1);

		// Check if the source file exists before trying to rename it.
		if (access(src_path, F_OK) == 0) {
			if (!rename(src_path, dest_path)) {
				syslog(LOG_DEBUG, "rename '%s' to '%s'.", src_path, dest_path);
			} else {
				syslog(LOG_ERR, "Could not rename '%s' to '%s'.", src_path, dest_path);
				return -1;
			}
		}
	}

	return 0;
}

/**
 * @brief Moves a file from a source path to a destination directory.
 * * @param src_path Absolute path to the source file.
 * @param dest_dir Absolute path to the destination directory.
 * @return 0 on success, -1 on failure.
 */
static int move_file(const char *src_path, const char *dest_dir) {
	size_t len;
	FILE *fd_s, *fd_d;
	char buffer[4096] = {0};
	char dest_path[PATH_MAX] = {0};

	snprintf(dest_path, sizeof(dest_path) - 1, "%s/%s", dest_dir, basename((char *)src_path));

	fd_s = fopen(src_path, "rb");
	if (!fd_s) {
		syslog(LOG_ERR, "Failed to open source file %s: %s", src_path, strerror(errno));
		return -1;
	}

	fd_d = fopen(dest_path, "wb+");
	if (!fd_d) {
		syslog(LOG_ERR, "Failed to open destination file %s: %s", dest_path, strerror(errno));
		fclose(fd_s);
		return -1;
	}

	while ((len = fread(buffer, 1, sizeof(buffer), fd_s)) > 0) {
		if (fwrite(buffer, 1, len, fd_d) != len) {
			syslog(LOG_ERR, "Error writing to destination file: %s", dest_path);
			fclose(fd_s);
			fclose(fd_d);
			return -1;
		}
	}

	unlink(src_path);

	fclose(fd_s);
	fclose(fd_d);

	return 0;
}

static int build_file_list(char *file_list, size_t list_size, const char *path) {
	size_t current_len = strlen(file_list);
	size_t path_len = strlen(path);
	struct stat st;

	if (stat(path, &st) != 0) {
		syslog(LOG_WARNING, "Backup source file not found: %s", path);
		return 0;
	}

	if (current_len + path_len + 1 > list_size - 1) {
		syslog(LOG_ERR, "Buffer too small.");
		return -1;
	}

	if (strlen(file_list)) {
		strcat(file_list, " ");
	}
	strcat(file_list, path);

	return 0;
}

/**
 * @brief Performs the backup task based on UCI configuration.
 */
static void do_backup(void) {
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *pkg = NULL;
	struct uci_section *s = NULL;
	struct uci_element *e = NULL;
	struct uci_option *o = NULL;
	struct statvfs vfs;
	struct stat st;
	long space_threshold_kb;
	long available_kb;
	long max_rotations;
	long file_kb;
	const char *str = NULL;
	char file_list[1024] = {0};
	char cmd[1024] = {0};
	char backup_file[256] = {0};
	char tmp_file[256] = {0};
	int n, ret;

	if (uci_load(ctx, UCI_CONFIG_FILE, &pkg) != UCI_OK) {
		syslog(LOG_ERR, "Failed to load UCI config: %s", UCI_CONFIG_FILE);
		return;
	}

	s = uci_lookup_section(ctx, pkg, "global");
	if (!s) {
		syslog(LOG_ERR, "UCI section 'global' not found in %s", UCI_CONFIG_FILE);
		goto backup_exit;
	}

	// Iterate through the list of files to be backed up
	uci_foreach_element(&s->options, e) {
		struct uci_element *le;
		o = uci_to_option(e);
		if (o->type != UCI_TYPE_LIST || strcmp(o->e.name, "file_list") != 0)
			continue;

		list_for_each_entry(le, &o->v.list, list) {
			const char *f = le->name;

			if (build_file_list(file_list, sizeof(file_list), f) != 0) {
				goto backup_exit;
			}
		}
	}

	if (!strlen(file_list)) {
		goto backup_exit;
	}

	// Get the backup directory
	str = uci_lookup_option_string(ctx, s, "backup_dir");
	if (!str || strlen(str)==0)
		str = DEFAULT_BACKUP_DIRECTOR;

	snprintf(backup_dir, sizeof(backup_dir) - 1 , "/%s", str);
	mkdir(backup_dir, 0755);

	// Get the backup file name
	str = uci_lookup_option_string(ctx, s, "backup_file");
	if (!str || strlen(str)==0)
		str = DEFAULT_BACKUP_FILE_NAME;

	n = snprintf(backup_file, sizeof(backup_file) - 1, "%s/%s", backup_dir, str);
	if (n >= sizeof(backup_file)) {
		syslog(LOG_ERR, "File buffer is too small.\n");
		goto backup_exit;
	}
	n = snprintf(tmp_file, sizeof(tmp_file) - 1, "/tmp/%s", str);
	if (n >= sizeof(tmp_file)) {
		syslog(LOG_ERR, "Tmp buffer is too small.\n");
		goto backup_exit;
	}

	// Create the backup tgz file in /tmp/
	n = snprintf(cmd, sizeof(cmd) - 1, "tar -czf %s %s", tmp_file, file_list);
	if (n >= sizeof(cmd)) {
		syslog(LOG_ERR, "Command buffer is too small.\n");
		goto backup_exit;
	}

	ret = system(cmd);
	if (ret){
		syslog(LOG_ERR, "The tar command failed with return code: %d\n", ret);
		goto backup_exit;
	}

	// Get available space on the overlay filesystem
	if (statvfs(OVERLAYFS_ROOT, &vfs) != 0) {
		syslog(LOG_ERR, "Failed to get overlay filesystem stats: %s", strerror(errno));
		goto backup_exit;
	}
	available_kb = (vfs.f_bavail * vfs.f_bsize) / 1024;
	str = uci_lookup_option_string(ctx, s, "space_threshold");
	space_threshold_kb = str ? atol(str) : DEFAULT_SPACE_THRESH;
	syslog(LOG_INFO, "Overlay available space: %ldKB", available_kb);

	// Check if there is enough space
	stat(tmp_file, &st);
	file_kb = st.st_size / 1024;
	if (available_kb < (file_kb + space_threshold_kb)) {
		syslog(LOG_ERR, "Overlay space insufficient for %s. (Available: %ldKB, Required: %ldKB)",
				tmp_file, available_kb, (file_kb + space_threshold_kb));
		goto backup_exit;
	}

	// Get the maximum number of rotations
	str = uci_lookup_option_string(ctx, s, "max_rotations");
	max_rotations = str ? atol(str) : DEFAULT_MAX_ROTATIONS;

	if (max_rotations > 1 && access(backup_file, F_OK)==0) {
		rotate_logs(backup_file, max_rotations);
	}

	if (move_file(tmp_file, backup_dir) == 0) {
		syslog(LOG_INFO, "Successfully backed up to '%s'.", backup_file);
	} else {
		syslog(LOG_ERR, "Failed to move '%s' to '%s': %s", tmp_file, backup_file, strerror(errno));
	}

backup_exit:
	uci_free_context(ctx);
}



/**
 * @brief Main entry point of the application.
 */
int main(int argc, char **argv) {
	// Set up syslog with the program name
	openlog("log-helper", LOG_PID, LOG_DAEMON);

	do_backup();

	closelog();

	return 0;
}
