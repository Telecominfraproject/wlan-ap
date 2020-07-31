/* SPDX-License-Identifier: BSD-3-Clause */

#include <syslog.h>
#include "target.h"
#include <net/if.h>
#include <libubus.h>

#include "ubus.h"

#include "command.h"

static struct ubus_context *ubus;

static void cmd_ubus_connect(struct ubus_context *ctx)
{
	ubus = ctx;
}

static void cmd_ubus_l3_dev(struct ubus_request *req,
			    int type, struct blob_attr *msg)
{
	enum {
		NET_ATTR_L3_DEVICE,
		__NET_ATTR_MAX,
	};

	static const struct blobmsg_policy network_policy[__NET_ATTR_MAX] = {
		[NET_ATTR_L3_DEVICE] = { .name = "l3_device", .type = BLOBMSG_TYPE_STRING },
	};

	struct blob_attr *tb[__NET_ATTR_MAX] = { };
	char *ifname = (char *)req->priv;

	blobmsg_parse(network_policy, __NET_ATTR_MAX, tb, blob_data(msg), blob_len(msg));
	memset(ifname, 0, IF_NAMESIZE);
	if (tb[NET_ATTR_L3_DEVICE])
		strncpy(ifname, blobmsg_get_string(tb[NET_ATTR_L3_DEVICE]), IF_NAMESIZE);
}

int ubus_get_l3_device(const char *net, char *ifname)
{
	uint32_t netifd;
	char path[64];

	snprintf(path, sizeof(path), "network.interface.%s", net);
	if (ubus_lookup_id(ubus, path, &netifd))
		return -1;
	return ubus_invoke(ubus, netifd, "status", NULL, cmd_ubus_l3_dev, ifname, 1000);
}

static struct ubus_instance ubus_instance = {
	.connect = cmd_ubus_connect,
};

int command_ubus_init(struct ev_loop *loop)
{
	return ubus_init(&ubus_instance, loop);
}
