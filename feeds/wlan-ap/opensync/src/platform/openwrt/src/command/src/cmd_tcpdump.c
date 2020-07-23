#include <libubox/list.h>
#include <evsched.h>
#include <net/if.h>

#include "command.h"

static void cmd_handler_curl_cb(struct ev_loop *loop, ev_child *child, int revents)
{
	struct task *task = container_of(child, struct task, child);

	ev_child_stop(loop, child);
	if (child->rstatus) {
		task_status(task, TASK_FAILED, "upload failed");
		return;
	}
	task_status(task, TASK_COMPLETE, NULL);
}

static void cmd_handler_curl(struct task *task)
{

	char *_url = NULL;
	char pcap[64];
	char url[128];
	char *argv[] = { "/usr/bin/curl", "-T", pcap, url, NULL };
	pid_t pid;

	snprintf(pcap, sizeof(pcap), "/tmp/%s-%d.pcap", serial, task->conf.timestamp);
	_url = SCHEMA_KEY_VAL(task->conf.payload, "ul_url");
	if (!_url) {
		LOG(ERR, "curl: command without a valid ul_url");
		task_status(task, TASK_FAILED, "invalid url");
		return;
	}
	snprintf(url, sizeof(url), "%s/%s", _url, pcap);

	pid = fork();
	if (pid == 0) {
		execv(*argv, argv);
		LOG(ERR, "curl: failed to start");
		exit(1);
	}

	if (pid < 0) {
		LOG(ERR, "curl, failed to fork");
		task_status(task, TASK_FAILED, "forking curl failed");
		return;
	}

	ev_child_init(&task->child, cmd_handler_curl_cb, pid, 0);
	ev_child_start(EV_DEFAULT, &task->child);

	LOGN("curl: started");
}

static void cmd_handler_tcpdump_cb(struct ev_loop *loop, ev_child *child, int revents)
{
	struct task *task = container_of(child, struct task, child);

	ev_child_stop(loop, child);
	if (child->rstatus) {
		task_status(task, TASK_FAILED, "tcpdump failed");
		return;
	}
	cmd_handler_curl(task);
}

pid_t cmd_handler_tcpdump(struct task *task)
{
	char ifname[IF_NAMESIZE];
	const char *network;
	char duration[64];
	char pcap[64];
	char *argv[] = { "/usr/sbin/tcpdump", "-c", "1000", "-G", duration, "-W", "1", "-w", pcap, "-i", ifname, NULL };
	pid_t pid;

	network = SCHEMA_KEY_VAL(task->conf.payload, "network");
	if (!network) {
		LOG(ERR, "tcpdump command without a valid network");
		return -1;
	}

	if (ubus_get_l3_device(network, ifname) || !strlen(ifname)) {
		LOG(ERR, "tcpdump: failed to lookup l3_device");
		return -1;
	}

	snprintf(duration, sizeof(duration), "%d", task->conf.duration - 5);
	snprintf(pcap, sizeof(pcap), "/tmp/%s-%d.pcap", serial, task->conf.timestamp);

	pid = fork();
	if (pid == 0) {
		execv(*argv, argv);
		LOG(ERR, "tcpdump: failed to exec");
		exit(1);
	}

	if (pid < 0) {
		LOG(ERR, "tcpdump: failed to fork");
		return -1;
	}

	ev_child_init(&task->child, cmd_handler_tcpdump_cb, pid, 0);
	ev_child_start(EV_DEFAULT, &task->child);

	LOGN("tcpdump: started");
	return pid;
}
