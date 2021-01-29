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

static void ubus_wan_ip_cb(struct ubus_request *req,
			    int type, struct blob_attr *msg)
{
	int rem = 0;
	enum {
		NET_ATTR_IP_ARRAY,
		__NET_ATTR_MAX,
	};

	static const struct blobmsg_policy network_policy[__NET_ATTR_MAX] = {
		[NET_ATTR_IP_ARRAY] = { .name = "ipv4-address", .type = BLOBMSG_TYPE_ARRAY },
	};
	struct blob_attr *tb[__NET_ATTR_MAX] = { };
	char *ipaddr = (char *)req->priv;
	blobmsg_parse(network_policy, __NET_ATTR_MAX, tb, blob_data(msg), blob_len(msg));

	enum {
		NET_ATTR_IP_ADDR,
		__NET_IP_ARRAY_MAX,
	};

	static struct blobmsg_policy ip_addr_policy[__NET_IP_ARRAY_MAX] = {
		[NET_ATTR_IP_ADDR]	= { .name = "address", .type = BLOBMSG_TYPE_STRING },
	};

	struct blob_attr *ttb = NULL;
	blobmsg_for_each_attr(ttb, tb[NET_ATTR_IP_ARRAY], rem)
	{
		struct blob_attr *tb_ip[__NET_IP_ARRAY_MAX] = {};

		blobmsg_parse(ip_addr_policy, __NET_IP_ARRAY_MAX, tb_ip, blobmsg_data(ttb), blobmsg_data_len(ttb));

		if (tb_ip[NET_ATTR_IP_ADDR]) {
			memset(ipaddr, 0, IPV4_ADDR_STR_LEN);
			strncpy(ipaddr, blobmsg_get_string(tb_ip[NET_ATTR_IP_ADDR]), IPV4_ADDR_STR_LEN);
		}
	}
}

int ubus_get_wan_ip(char *ipaddr)
{

	uint32_t id;
	int rc = 0;
	struct ubus_context *context = NULL;
	context = ubus_connect(NULL);

	if (!context) {
		rc = -1;
		goto out;
	}
	const char* path = "network.interface.wan";
	rc = ubus_lookup_id(context, path, &id);
	if (rc) {
		LOGE("Failed to lookup ubus object");
		goto out;
	}
	rc = ubus_invoke(context, id, "status", NULL, ubus_wan_ip_cb, ipaddr, 1000);

out:
	if (context)
		ubus_free(context);

	return rc;
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
