/* SPDX-License-Identifier: BSD-3-Clause */

#include <syslog.h>
#include "target.h"

#include "netifd.h"
#include "ubus.h"

static struct ubus_context *ubus;

static void netifd_ubus_connect(struct ubus_context *ctx)
{
	ubus = ctx;
}

static int netifd_ubus_notify(struct ubus_context *ctx, struct ubus_object *obj,
			     struct ubus_request_data *req, const char *method,
			     struct blob_attr *msg)
{
	char *str;

	str = blobmsg_format_json(msg, true);
	LOGN("ubus: Received ubus notify '%s': %s\n", method, str);
	free(str);

	if (!strncmp(method, "dhcp.", 5)) {
		dhcp_lease(method, msg);
	} else if (!strncmp(method, "interface.", 10)) {
		wifi_inet_state_set(msg);
		wifi_inet_master_set(msg);
	}

	return 0;
}

static void netifd_ubus_iface_dump(struct ubus_request *req,
				   int type, struct blob_attr *msg)
{
	wifi_inet_state_set(msg);
	wifi_inet_master_set(msg);
}

static void netifd_ubus_subscribed(const char *path, uint32_t id, int add)
{
	if (!strncmp(path, "network.", 8))
		ubus_invoke(ubus, id, "status", NULL, netifd_ubus_iface_dump, NULL, 1000);
}

static struct ubus_instance ubus_instance = {
	.connect = netifd_ubus_connect,
	.subscribed = netifd_ubus_subscribed,
	.notify = netifd_ubus_notify,
	.list = {
			{
				.path = "network.interface.",
				.wildcard = 1,
			}, {
				.path = "dnsmasq",
			},
		},
	.len = 2,
};

int netifd_ubus_init(struct ev_loop *loop)
{
	return ubus_init(&ubus_instance, loop);
}
