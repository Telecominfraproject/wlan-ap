/* SPDX-License-Identifier: BSD-3-Clause */

#include <syslog.h>
#include "target.h"

#include "evsched.h"
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

static struct ubus_instance ubus_instance = {
	.connect = netifd_ubus_connect,
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
