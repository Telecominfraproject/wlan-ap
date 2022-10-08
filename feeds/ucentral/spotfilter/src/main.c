// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <libubox/uloop.h>

#include "spotfilter.h"

int spotfilter_run_cmd(char *cmd, bool ignore_error)
{
	char *argv[] = { "sh", "-c", cmd, NULL };
	bool first = true;
	int status = -1;
	char buf[512];
	int fds[2];
	FILE *f;
	int pid;

	if (pipe(fds))
		return -1;

	pid = fork();
	if (!pid) {
		close(fds[0]);
		if (fds[1] != STDOUT_FILENO)
			dup2(fds[1], STDOUT_FILENO);
		if (fds[1] != STDERR_FILENO)
			dup2(fds[1], STDERR_FILENO);
		if (fds[1] > STDERR_FILENO)
			close(fds[1]);
		execv("/bin/sh", argv);
		exit(1);
	}

	if (pid < 0)
		return -1;

	close(fds[1]);
	f = fdopen(fds[0], "r");
	if (!f) {
		close(fds[0]);
		goto out;
	}

	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (!strlen(buf))
			break;
		if (ignore_error)
			continue;
		if (first) {
			ULOG_WARN("Command: %s\n", cmd);
			first = false;
		}
		ULOG_WARN("%s%s", buf, strchr(buf, '\n') ? "" : "\n");
	}

	fclose(f);

out:
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			break;

	return status;
}

static int usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [options]\n"
		"Options:\n"
		"\n", progname);

	return 1;
}

int main(int argc, char **argv)
{
	int ret = 2;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			return usage(argv[0]);
		}
	}

	ulog_open(ULOG_SYSLOG, LOG_DAEMON, "spotfilter");
	uloop_init();

	if (rtnl_init())
		return 1;

	if (spotfilter_nl80211_init())
		return 1;

	if (spotfilter_dev_init())
		return 1;

	if (spotfilter_ubus_init())
		goto out;

	ret = 0;
	uloop_run();

	spotfilter_ubus_stop();

out:
	interface_done();
	spotfilter_dev_done();
	spotfilter_nl80211_done();
	uloop_done();

	return ret;
}
