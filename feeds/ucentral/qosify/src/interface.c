// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <net/if.h>

#include <unistd.h>
#include <errno.h>

#include <libubox/vlist.h>
#include <libubox/avl-cmp.h>
#include <libubox/uloop.h>

#include "qosify.h"

static void interface_update_cb(struct vlist_tree *tree,
				struct vlist_node *node_new,
				struct vlist_node *node_old);

static VLIST_TREE(devices, avl_strcmp, interface_update_cb, true, false);
static VLIST_TREE(interfaces, avl_strcmp, interface_update_cb, true, false);
static int socket_fd;

#define APPEND(_buf, _ofs, _format, ...) _ofs += snprintf(_buf + _ofs, sizeof(_buf) - _ofs, _format, ##__VA_ARGS__)

struct qosify_iface_config {
	struct blob_attr *data;

	bool ingress;
	bool egress;
	bool nat;
	bool host_isolate;
	bool autorate_ingress;

	const char *bandwidth_up;
	const char *bandwidth_down;
	const char *mode;
	const char *common_opts;
	const char *ingress_opts;
	const char *egress_opts;
};


struct qosify_iface {
	struct vlist_node node;

	char ifname[IFNAMSIZ];
	bool active;

	bool device;
	struct blob_attr *config_data;
	struct qosify_iface_config config;
};

enum {
	IFACE_ATTR_BW_UP,
	IFACE_ATTR_BW_DOWN,
	IFACE_ATTR_INGRESS,
	IFACE_ATTR_EGRESS,
	IFACE_ATTR_MODE,
	IFACE_ATTR_NAT,
	IFACE_ATTR_HOST_ISOLATE,
	IFACE_ATTR_AUTORATE_IN,
	IFACE_ATTR_INGRESS_OPTS,
	IFACE_ATTR_EGRESS_OPTS,
	IFACE_ATTR_OPTS,
	__IFACE_ATTR_MAX
};

static inline const char *qosify_iface_name(struct qosify_iface *iface)
{
	return iface->node.avl.key;
}

static void
iface_config_parse(struct blob_attr *attr, struct blob_attr **tb)
{
	static const struct blobmsg_policy policy[__IFACE_ATTR_MAX] = {
		[IFACE_ATTR_BW_UP] = { "bandwidth_up", BLOBMSG_TYPE_STRING },
		[IFACE_ATTR_BW_DOWN] = { "bandwidth_down", BLOBMSG_TYPE_STRING },
		[IFACE_ATTR_INGRESS] = { "ingress", BLOBMSG_TYPE_BOOL },
		[IFACE_ATTR_EGRESS] = { "egress", BLOBMSG_TYPE_BOOL },
		[IFACE_ATTR_MODE] = { "mode", BLOBMSG_TYPE_STRING },
		[IFACE_ATTR_NAT] = { "nat", BLOBMSG_TYPE_BOOL },
		[IFACE_ATTR_HOST_ISOLATE] = { "host_isolate", BLOBMSG_TYPE_BOOL },
		[IFACE_ATTR_AUTORATE_IN] = { "autorate_ingress", BLOBMSG_TYPE_BOOL },
		[IFACE_ATTR_INGRESS_OPTS] = { "ingress_options", BLOBMSG_TYPE_STRING },
		[IFACE_ATTR_EGRESS_OPTS] = { "egress_options", BLOBMSG_TYPE_STRING },
		[IFACE_ATTR_OPTS] = { "options", BLOBMSG_TYPE_STRING },
	};

	blobmsg_parse(policy, __IFACE_ATTR_MAX, tb, blobmsg_data(attr), blobmsg_len(attr));
}

static bool
iface_config_equal(struct qosify_iface *if1, struct qosify_iface *if2)
{
	struct blob_attr *tb1[__IFACE_ATTR_MAX], *tb2[__IFACE_ATTR_MAX];
	int i;

	iface_config_parse(if1->config_data, tb1);
	iface_config_parse(if2->config_data, tb2);

	for (i = 0; i < __IFACE_ATTR_MAX; i++) {
		if (!!tb1[i] != !!tb2[i])
			return false;

		if (!tb1[i])
			continue;

		if (blob_raw_len(tb1[i]) != blob_raw_len(tb2[i]))
			return false;

		if (memcmp(tb1[i], tb2[i], blob_raw_len(tb1[i])) != 0)
			return false;
	}

	return true;
}

static const char *check_str(struct blob_attr *attr)
{
	const char *str = blobmsg_get_string(attr);

	if (strchr(str, '\''))
		return NULL;

	return str;
}

static void
iface_config_set(struct qosify_iface *iface, struct blob_attr *attr)
{
	struct qosify_iface_config *cfg = &iface->config;
	struct blob_attr *tb[__IFACE_ATTR_MAX];
	struct blob_attr *cur;

	iface_config_parse(attr, tb);

	memset(cfg, 0, sizeof(*cfg));

	/* defaults */
	cfg->mode = "diffserv4";
	cfg->ingress = true;
	cfg->egress = true;
	cfg->host_isolate = true;
	cfg->autorate_ingress = true;
	cfg->nat = !iface->device;

	if ((cur = tb[IFACE_ATTR_BW_UP]) != NULL)
		cfg->bandwidth_up = check_str(cur);
	if ((cur = tb[IFACE_ATTR_BW_DOWN]) != NULL)
		cfg->bandwidth_down = check_str(cur);
	if ((cur = tb[IFACE_ATTR_MODE]) != NULL)
		cfg->mode = check_str(cur);
	if ((cur = tb[IFACE_ATTR_OPTS]) != NULL)
		cfg->common_opts = check_str(cur);
	if ((cur = tb[IFACE_ATTR_EGRESS_OPTS]) != NULL)
		cfg->egress_opts = check_str(cur);
	if ((cur = tb[IFACE_ATTR_INGRESS_OPTS]) != NULL)
		cfg->ingress_opts = check_str(cur);
	if ((cur = tb[IFACE_ATTR_INGRESS]) != NULL)
		cfg->ingress = blobmsg_get_bool(cur);
	if ((cur = tb[IFACE_ATTR_EGRESS]) != NULL)
		cfg->egress = blobmsg_get_bool(cur);
	if ((cur = tb[IFACE_ATTR_NAT]) != NULL)
		cfg->nat = blobmsg_get_bool(cur);
	if ((cur = tb[IFACE_ATTR_HOST_ISOLATE]) != NULL)
		cfg->host_isolate = blobmsg_get_bool(cur);
	if ((cur = tb[IFACE_ATTR_AUTORATE_IN]) != NULL)
		cfg->autorate_ingress = blobmsg_get_bool(cur);
}

static const char *
interface_ifb_name(struct qosify_iface *iface)
{
	static char ifname[IFNAMSIZ + 1] = "ifb-";
	int len = strlen(iface->ifname);

	if (len + 4 < IFNAMSIZ) {
		snprintf(ifname + 4, IFNAMSIZ - 4, "%s", iface->ifname);

		return ifname;
	}

	ifname[4] = iface->ifname[0];
	ifname[5] = iface->ifname[1];
	snprintf(ifname + 6, IFNAMSIZ - 6, "%s", iface->ifname + len - (IFNAMSIZ + 6) - 1);

	return ifname;
}

static int run_cmd(char *cmd, bool ignore)
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
		if (ignore)
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

static int
prepare_tc_cmd(char *buf, int len, const char *type, const char *cmd,
	       const char *dev, const char *extra)
{
	return snprintf(buf, len, "tc %s %s dev '%s' %s", type, cmd, dev, extra);
}

static int
cmd_del_qdisc(const char *ifname, const char *type)
{
	char buf[64];

	prepare_tc_cmd(buf, sizeof(buf), "qdisc", "del", ifname, type);

	return run_cmd(buf, true);
}

static int
cmd_add_qdisc(struct qosify_iface *iface, const char *ifname, bool egress, bool eth)
{
	struct qosify_iface_config *cfg = &iface->config;
	const char *bw = egress ? cfg->bandwidth_up : cfg->bandwidth_down;
	const char *dir_opts = egress ? cfg->egress_opts : cfg->ingress_opts;
	char buf[512];
	int ofs;

	cmd_del_qdisc(ifname, "root");

	ofs = prepare_tc_cmd(buf, sizeof(buf), "qdisc", "add", ifname, "root handle 1: cake");
	if (bw)
		APPEND(buf, ofs, " bandwidth %s", bw);

	APPEND(buf, ofs, " %s %sgress", cfg->mode, egress ? "e" : "in");

	if (cfg->host_isolate)
		APPEND(buf, ofs, " %snat dual-%shost",
			cfg->nat ? "" : "no",
			egress ? "src" : "dst");
	else
		APPEND(buf, ofs, " flows");

	APPEND(buf, ofs, " %s %s",
	       cfg->common_opts ? cfg->common_opts : "",
	       dir_opts ? dir_opts : "");

	run_cmd(buf, false);

	ofs = prepare_tc_cmd(buf, sizeof(buf), "filter", "add", ifname, "parent 1: bpf");
	APPEND(buf, ofs, " object-pinned /sys/fs/bpf/qosify_%sgress_%s verbose direct-action",
	       egress ? "e" : "in",
		   eth ? "eth" : "ip");

	return run_cmd(buf, false);
}

static int
cmd_del_ingress(struct qosify_iface *iface)
{
	char buf[256];

	cmd_del_qdisc(iface->ifname, "handle ffff: ingress");
	snprintf(buf, sizeof(buf), "ip link del '%s'", interface_ifb_name(iface));

	return run_cmd(buf, true);
}


static int
cmd_add_ingress(struct qosify_iface *iface, bool eth)
{
	const char *ifbdev = interface_ifb_name(iface);
	char buf[256];
	int ofs;

	cmd_del_ingress(iface);

	ofs = prepare_tc_cmd(buf, sizeof(buf), "qdisc", "add", iface->ifname, " handle ffff: ingress");
	run_cmd(buf, false);

	snprintf(buf, sizeof(buf), "ip link add '%s' type ifb", ifbdev);
	run_cmd(buf, false);

	cmd_add_qdisc(iface, ifbdev, false, eth);

	snprintf(buf, sizeof(buf), "ip link set dev '%s' up", ifbdev);
	run_cmd(buf, false);

	ofs = prepare_tc_cmd(buf, sizeof(buf), "filter", "add", iface->ifname, " parent ffff:");
	APPEND(buf, ofs, " protocol all prio 10 u32 match u32 0 0 "
			 "flowid 1:1 action mirred egress redirect dev '%s'", ifbdev);
	return run_cmd(buf, false);
}

static void
interface_start(struct qosify_iface *iface)
{
	struct ifreq ifr = {};
	bool eth;

	if (!iface->ifname[0] || iface->active)
		return;

	ULOG_INFO("start interface %s\n", iface->ifname);

	strncpy(ifr.ifr_name, iface->ifname, sizeof(ifr.ifr_name));
	if (ioctl(socket_fd, SIOCGIFHWADDR, &ifr) < 0) {
		ULOG_ERR("ioctl(SIOCGIFHWADDR, %s) failed: %s\n", iface->ifname, strerror(errno));
		return;
	}

	eth = ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER;

	if (iface->config.egress)
		cmd_add_qdisc(iface, iface->ifname, true, eth);
	if (iface->config.ingress)
		cmd_add_ingress(iface, eth);

	iface->active = true;
}

static void
interface_stop(struct qosify_iface *iface)
{
	if (!iface->ifname[0] || !iface->active)
		return;

	ULOG_INFO("stop interface %s\n", iface->ifname);
	iface->active = false;

	if (iface->config.egress)
		cmd_del_qdisc(iface->ifname, "root");
	if (iface->config.ingress)
		cmd_del_ingress(iface);
}

static void
interface_set_config(struct qosify_iface *iface, struct blob_attr *config)
{
	iface->config_data = blob_memdup(config);
	iface_config_set(iface, iface->config_data);
	interface_start(iface);
}

static void
interface_update_cb(struct vlist_tree *tree,
		    struct vlist_node *node_new, struct vlist_node *node_old)
{
	struct qosify_iface *if_new = NULL, *if_old = NULL;

	if (node_new)
		if_new = container_of(node_new, struct qosify_iface, node);
	if (node_old)
		if_old = container_of(node_old, struct qosify_iface, node);

	if (if_new && if_old) {
		if (!iface_config_equal(if_old, if_new)) {
			interface_stop(if_old);
			free(if_old->config_data);
			interface_set_config(if_old, if_new->config_data);
		}

		free(if_new);
		return;
	}

	if (if_old) {
		interface_stop(if_old);
		free(if_old->config_data);
		free(if_old);
	}

	if (if_new)
		interface_set_config(if_new, if_new->config_data);
}

static void
interface_create(struct blob_attr *attr, bool device)
{
	struct qosify_iface *iface;
	const char *name = blobmsg_name(attr);
	int name_len = strlen(name);
	char *name_buf;

	if (strchr(name, '\''))
		return;

	if (name_len >= IFNAMSIZ)
		return;

	if (blobmsg_type(attr) != BLOBMSG_TYPE_TABLE)
		return;

	iface = calloc_a(sizeof(*iface), &name_buf, name_len + 1);
	strcpy(name_buf, blobmsg_name(attr));
	iface->config_data = attr;
	iface->device = device;
	vlist_add(device ? &devices : &interfaces, &iface->node, name_buf);
}

void qosify_iface_config_update(struct blob_attr *ifaces, struct blob_attr *devs)
{
	struct blob_attr *cur;
	int rem;

	vlist_update(&devices);
	blobmsg_for_each_attr(cur, devs, rem)
		interface_create(cur, true);
	vlist_flush(&devices);

	vlist_update(&interfaces);
	blobmsg_for_each_attr(cur, ifaces, rem)
		interface_create(cur, false);
	vlist_flush(&interfaces);
}

static void
qosify_iface_check_device(struct qosify_iface *iface)
{
	const char *name = qosify_iface_name(iface);
	int ifindex;

	ifindex = if_nametoindex(name);
	if (!ifindex) {
		interface_stop(iface);
		iface->ifname[0] = 0;
	} else {
		snprintf(iface->ifname, sizeof(iface->ifname), "%s", name);
		interface_start(iface);
	}
}

static void
qosify_iface_check_interface(struct qosify_iface *iface)
{
	const char *name = qosify_iface_name(iface);
	char ifname[IFNAMSIZ];

	if (qosify_ubus_check_interface(name, ifname, sizeof(ifname)) == 0) {
		snprintf(iface->ifname, sizeof(iface->ifname), "%s", ifname);
		interface_start(iface);
	} else {
		interface_stop(iface);
		iface->ifname[0] = 0;
	}
}

static void qos_iface_check_cb(struct uloop_timeout *t)
{
	struct qosify_iface *iface;

	vlist_for_each_element(&devices, iface, node)
		qosify_iface_check_device(iface);
	vlist_for_each_element(&interfaces, iface, node)
		qosify_iface_check_interface(iface);
}

void qosify_iface_check(void)
{
	static struct uloop_timeout timer = {
		.cb = qos_iface_check_cb,
	};

	uloop_timeout_set(&timer, 10);
}

void qosify_iface_status(struct blob_buf *b)
{
	struct qosify_iface *iface;
	void *c, *i;

	c = blobmsg_open_table(b, "devices");
	vlist_for_each_element(&devices, iface, node) {
		i = blobmsg_open_table(b, qosify_iface_name(iface));
		blobmsg_add_u8(b, "active", iface->active);
		blobmsg_close_table(b, i);
	}
	blobmsg_close_table(b, c);

	c = blobmsg_open_table(b, "interfaces");
	vlist_for_each_element(&interfaces, iface, node) {
		i = blobmsg_open_table(b, qosify_iface_name(iface));
		blobmsg_add_u8(b, "active", iface->active);
		if (iface->ifname)
			blobmsg_add_string(b, "ifname", iface->ifname);
		blobmsg_close_table(b, i);
	}
	blobmsg_close_table(b, c);
}

int qosify_iface_init(void)
{
	socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket < 0)
		return -1;

	return 0;
}

void qosify_iface_stop(void)
{
	struct qosify_iface *iface;

	vlist_for_each_element(&interfaces, iface, node)
		interface_stop(iface);
	vlist_for_each_element(&devices, iface, node)
		interface_stop(iface);
}

