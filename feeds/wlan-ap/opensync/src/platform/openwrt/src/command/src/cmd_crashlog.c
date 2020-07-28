/* SPDX-License-Identifier: BSD-3-Clause */

#include <libubox/list.h>
#include <evsched.h>
#include <net/if.h>

#include "command.h"

#include <fcntl.h>
#include <unistd.h>

static void cmd_handler_crashlog_cb(struct ev_loop *loop, ev_child *child, int revents)
{
	struct task *task = container_of(child, struct task, child);

	ev_child_stop(loop, child);
	if (child->rstatus) {
		task_status(task, TASK_FAILED, "upload failed");
		return;
	}
	task_status(task, TASK_COMPLETE, NULL);
}

pid_t cmd_handler_crashlog(struct task *task)
{

	char *_url = NULL;
	char url[128];
	char *argv[] = { "/usr/bin/curl", "-T", "/tmp/crashlog", url, NULL };
	pid_t pid;

	if (!crash_timestamp) {
		task_status(task, TASK_FAILED, "no crashlog found");
		return -1;
	}

	system("cp /sys/kernel/debug/crashlog /tmp/crashlog");

	_url = SCHEMA_KEY_VAL(task->conf.payload, "ul_url");
	if (!_url) {
		LOG(ERR, "curl: command without a valid ul_url");
		task_status(task, TASK_FAILED, "invalid url");
		return -1;
	}
	snprintf(url, sizeof(url), "%s/%s-%lu.crashlog", _url, serial, crash_timestamp);

	pid = fork();
	if (pid == 0) {
		execv(*argv, argv);
		LOG(ERR, "curl: failed to start");
		exit(1);
	}

	if (pid < 0) {
		LOG(ERR, "curl, failed to fork");
		task_status(task, TASK_FAILED, "forking curl failed");
		return -1;
	}

	ev_child_init(&task->child, cmd_handler_crashlog_cb, pid, 0);
	ev_child_start(EV_DEFAULT, &task->child);

	LOGN("curl: started");
	return pid;
}
