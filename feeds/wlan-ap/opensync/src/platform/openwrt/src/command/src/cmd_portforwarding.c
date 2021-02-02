/* SPDX-License-Identifier: BSD-3-Clause */

#include <libubox/list.h>
#include <evsched.h>
#include <net/if.h>

#include "command.h"

#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/prctl.h>



static void cmd_handler_port_forwarding_cb(struct ev_loop *loop, ev_child *child, int revents)
{
	struct task *task = container_of(child, struct task, child);

	ev_child_stop(loop, child);
	task_status(task, TASK_COMPLETE, NULL);
}

int cmd_handler_stop_session(struct task *task)
{
	pid_t pid = 0;
	char buf[8];

	FILE *cmd_pipe = popen("pidof portfwd", "r");

	if (cmd_pipe) {
	fgets(buf, 512, cmd_pipe);
	pid = strtoul(buf, NULL, 10);
	}

	pclose( cmd_pipe );
	
	if(pid) {
	LOG(DEBUG,"killing pid = %d", pid);
	kill(pid, SIGKILL);
	}

	task_status(task, TASK_COMPLETE, NULL);
	LOGN("Updating the task status stop session");

	return 1;
}

pid_t cmd_handler_port_forwarding(struct task *task)
{
	char *gateway = NULL;
	char *port = NULL;
	pid_t pid;

	gateway = SCHEMA_KEY_VAL(task->conf.payload, "gateway_hostname");
	port = SCHEMA_KEY_VAL(task->conf.payload, "gateway_port");
	
	LOG(DEBUG, "gateway : %s   , port: %s ", gateway, port);
	pid = fork();
	if (pid == 0) {
		prctl(PR_SET_NAME, (unsigned long)"portfwd", NULL, NULL, NULL);
		port_forwarding(gateway, port);
		exit(1);
	}

	if (pid < 0) {
		LOG(ERR, "portforwarding, failed to fork");
		task_status(task, TASK_FAILED, "forking portforwarding failed");
		return -1;
	}

	ev_child_init(&task->child, cmd_handler_port_forwarding_cb, pid, 0);
	ev_child_start(EV_DEFAULT, &task->child);

	LOGN("portforwarding: started");
	return pid;
}
