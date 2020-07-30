/* SPDX-License-Identifier: BSD-3-Clause */

#include <libubox/list.h>
#include <linux/limits.h>
#include <libgen.h>
#include <evsched.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>

#include <libubox/blobmsg.h>
#include <uci.h>

#include "command.h"

static int ioctl_socket = -1;
static struct blob_buf b;

static int phy_from_path(char *path, char *phy)
{
	unsigned int  i;
	int ret = -1;
	glob_t gl;

	if (glob("/sys/class/ieee80211/*", GLOB_NOSORT, NULL, &gl))
                return -1;

	for (i = 0; i < gl.gl_pathc; i++) {
		char symlink[PATH_MAX] = {};

		if (!readlink(gl.gl_pathv[i], symlink, PATH_MAX))
			continue;
		if (!strstr(symlink, path))
			continue;
		strncpy(phy, basename(symlink), 6);
		ret = 0;
		break;
	}
	globfree(&gl);

	return ret;
}

static int iface_ioctl_socket(void)
{
	if (ioctl_socket == -1) {
		ioctl_socket = socket(AF_INET, SOCK_DGRAM, 0);
		fcntl(ioctl_socket, F_SETFD, fcntl(ioctl_socket, F_GETFD) | FD_CLOEXEC);
	}

	return ioctl_socket;
}

static int iface_ioctl(int cmd, void *ifr)
{
	int sock = iface_ioctl_socket();

	return ioctl(sock, cmd, ifr);
}

static int iface_up(const char *ifname)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	if (iface_ioctl(SIOCGIFFLAGS, &ifr))
		return 0;

	ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

	return !iface_ioctl(SIOCSIFFLAGS, &ifr);
}


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

	_url = SCHEMA_KEY_VAL(task->conf.payload, "ul_url");
	if (!_url) {
		LOG(ERR, "curl: command without a valid ul_url");
		task_status(task, TASK_FAILED, "invalid url");
		return;
	}
	snprintf(pcap, sizeof(pcap), "%s-%d.pcap", serial, task->conf.timestamp);
	snprintf(url, sizeof(url), "%s/%s", _url, pcap);
	snprintf(pcap, sizeof(pcap), "/tmp/%s-%d.pcap", serial, task->conf.timestamp);

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

static void cmd_handler_tcpdump_wifi_cb(struct ev_loop *loop, ev_child *child, int revents)
{
	struct task *task = container_of(child, struct task, child);
	char iw[128];

	snprintf(iw, sizeof(iw), "iw dev %s del", task->arg);
	system(iw);
	cmd_handler_tcpdump_cb(loop, child, revents);
}

pid_t cmd_handler_tcpdump_wifi(struct task *task)
{
	enum {
		PHY_ATTR_PATH,
		__PHY_ATTR_MAX,
	};

	static const struct blobmsg_policy phy_policy[__PHY_ATTR_MAX] = {
		[PHY_ATTR_PATH] = { .name = "path", .type = BLOBMSG_TYPE_STRING },
	};

	static const struct uci_blob_param_list phy_param = {
		.n_params = __PHY_ATTR_MAX,
		.params = phy_policy,
	};

	struct blob_attr *tb[__PHY_ATTR_MAX] = { };
	char phy[IF_NAMESIZE];
	struct uci_context *uci;
	struct uci_package *p;
	struct uci_section *s;
	char duration[64];
	char pcap[64];
	char *argv[] = { "/usr/sbin/tcpdump", "-c", "1000", "-G", duration, "-W", "1", "-w", pcap, "-i", phy, NULL };
	char iw[128];
	pid_t pid;

	task->arg = SCHEMA_KEY_VAL(task->conf.payload, "wifi");
	if (!task->arg) {
		task_status(task, TASK_FAILED, "unknown wifi");
		LOG(ERR, "tcpdump command without a valid network");
		return -1;
	}

	blob_buf_init(&b, 0);
	uci = uci_alloc_context();
	uci_load(uci, "wireless", &p);
	s = uci_lookup_section(uci, p, task->arg);
        if (!s) {
		task_status(task, TASK_FAILED, "unknown wifi");
		uci_free_context(uci);
                return -1;
	}

        uci_to_blob(&b, s, &phy_param);
	uci_free_context(uci);

	blobmsg_parse(phy_policy, __PHY_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));
	if (!tb[PHY_ATTR_PATH] || phy_from_path(blobmsg_get_string(tb[PHY_ATTR_PATH]), phy)) {
		task_status(task, TASK_FAILED, "unknown wifi");
		return -1;
	}

	snprintf(duration, sizeof(duration), "%d", task->conf.duration - 5);
	snprintf(pcap, sizeof(pcap), "/tmp/%s-%d.pcap", serial, task->conf.timestamp);
	snprintf(iw, sizeof(iw), "iw phy %s interface add %s type monitor", phy, phy);
	system(iw);
	iface_up(phy);

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

	ev_child_init(&task->child, cmd_handler_tcpdump_wifi_cb, pid, 0);
	ev_child_start(EV_DEFAULT, &task->child);

	LOGN("tcpdump: started");
	return pid;
}
