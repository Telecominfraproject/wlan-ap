/*
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "udevmand.h"

static struct ubus_auto_conn conn;
static struct ubus_subscriber subscriber;

enum {
	EVENT_ID,
	EVENT_PATH,
	__EVENT_MAX
};

static const struct blobmsg_policy status_policy[__EVENT_MAX] = {
	[EVENT_ID] = { .name = "id", .type = BLOBMSG_TYPE_INT32 },
	[EVENT_PATH] = { .name = "path", .type = BLOBMSG_TYPE_STRING },
};

static const struct ubus_watch_list {
	const char *path;
	int wildcard;
} ubus_watch_list[] = {
	{
		.path = "service",
	}, {
		.path = "dnsmasq",
	}, {
		.path = "network.interface",
	}, {
		.path = "network.status",
	},
};

static int
ubus_mac_cb(struct ubus_context *ctx, struct ubus_object *obj,
	    struct ubus_request_data *req, const char *method,
	    struct blob_attr *msg)
{
	if (!mac_dump_all())
		ubus_send_reply(ctx, req, b.head);
	return UBUS_STATUS_OK;
}

/*static int
ubus_interface_cb(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	if (!interface_dump())
		ubus_send_reply(ctx, req, b.head);
	return UBUS_STATUS_OK;
}*/

static int
ubus_port_cb(struct ubus_context *ctx, struct ubus_object *obj,
	     struct ubus_request_data *req, const char *method,
	     struct blob_attr *msg)
{
	enum {
		DUMP_DELTA,
		__DUMP_MAX
	};

	static const struct blobmsg_policy dump_policy[__DUMP_MAX] = {
		[DUMP_DELTA] = { .name = "delta", .type = BLOBMSG_TYPE_INT32 },
	};

	struct blob_attr *tb[__DUMP_MAX];
	int delta = 0;

	blobmsg_parse(dump_policy, __DUMP_MAX, tb, blob_data(msg), blob_len(msg));

	if (tb[DUMP_DELTA])
		delta = blobmsg_get_u32(tb[DUMP_DELTA]);

	iface_dump(delta);
	ubus_send_reply(ctx, req, b.head);

	return UBUS_STATUS_OK;
}

static const struct ubus_method topology_methods[] = {
	//UBUS_METHOD_NOARG("interface", ubus_interface_cb),
        UBUS_METHOD_NOARG("mac", ubus_mac_cb),
	UBUS_METHOD_NOARG("port", ubus_port_cb),
};

static struct ubus_object_type ubus_object_type =
	UBUS_OBJECT_TYPE("topology", topology_methods);

static struct ubus_object ubus_object = {
	.name = "topology",
	.type = &ubus_object_type,
	.methods = topology_methods,
	.n_methods = ARRAY_SIZE(topology_methods),
};

static int
ubus_watch_match(const char *path)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ubus_watch_list); i++) {
		int len = strlen(ubus_watch_list[i].path);

		if (ubus_watch_list[i].wildcard && strncmp(path, ubus_watch_list[i].path, len))
			continue;
		if (!ubus_watch_list[i].wildcard && strcmp(path, ubus_watch_list[i].path))
			continue;
		return 0;
	}
	return -1;
}

enum {
	INTERFACE,
	__INTERFACE_MAX,
};

static const struct blobmsg_policy interface_policy[__INTERFACE_MAX] = {
	[INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_ARRAY },
};

static void
ubus_netifd_dump_cb(struct ubus_request *req,
		    int type, struct blob_attr *msg)
{
	struct blob_attr *tb[__INTERFACE_MAX];
	struct blob_attr *iter;
	int rem;

	blobmsg_parse(interface_policy, __INTERFACE_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[INTERFACE])
		return;
	blobmsg_for_each_attr(iter, tb[INTERFACE], rem)
		interface_update(iter, 0);
}

static int
ubus_notify_cb(struct ubus_context *ctx, struct ubus_object *obj,
	       struct ubus_request_data *req, const char *method,
	       struct blob_attr *msg)
{
	if (0) {
		char *str;

		str = blobmsg_format_json(msg, true);
		ULOG_INFO("Received ubus notify '%s': %s\n", method, str);
		free(str);
	}

	if (!strcmp(method, "dhcp.ack"))
		dhcpv4_ack(msg);

	else if (!strcmp(method, "dhcp.release"))
		dhcpv4_release(msg);

	else if (!strcmp(method, "interface.update"))
		interface_update(msg, 1);

	else if (!strcmp(method, "interface.down"))
		interface_down(msg);

	return 0;
}

static void
ubus_event_handler_cb(struct ubus_context *ctx,  struct ubus_event_handler *ev,
	      const char *type, struct blob_attr *msg)
{
	struct blob_attr *tb[__EVENT_MAX];
	const char *path;
	uint32_t id;

	if (strcmp("ubus.object.add", type))
		return;

	if (0) {
		char *str;

		str = blobmsg_format_json(msg, true);
		ULOG_INFO("Received ubus notify '%s': %s\n", type, str);
		free(str);
	}

	blobmsg_parse(status_policy, __EVENT_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[EVENT_ID] || !tb[EVENT_PATH])
		return;

	path = blobmsg_get_string(tb[EVENT_PATH]);
	id = blobmsg_get_u32(tb[EVENT_ID]);

	if (!ubus_watch_match(path) && !ubus_subscribe(ctx, &subscriber, id))
		ULOG_INFO("Subscribe to %s (%u)\n", path, id);
}

static struct ubus_event_handler ubus_event_handler = { .cb = ubus_event_handler_cb };

static void
receive_list_result(struct ubus_context *ctx, struct ubus_object_data *obj,
		    void *priv)
{
	char *path = strdup(obj->path);

	if (!ubus_watch_match(path) && !ubus_subscribe(ctx, &subscriber, obj->id))
		ULOG_INFO("Subscribe to %s (%u)\n", path, obj->id);
	free(path);
}

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	uint32_t netifd;

	ubus_add_object(ctx, &ubus_object);

	ubus_register_event_handler(ctx, &ubus_event_handler, "ubus.object.add");
	ubus_register_event_handler(ctx, &ubus_event_handler, "ubus.object.remove");

	subscriber.cb = ubus_notify_cb;
	if (ubus_register_subscriber(ctx, &subscriber))
		ULOG_ERR("failed to register ubus subscriber\n");

	ubus_lookup(ctx, NULL, receive_list_result, NULL);

	if (!ubus_lookup_id(ctx, "network.interface", &netifd))
		ubus_invoke(ctx, netifd, "dump", NULL, ubus_netifd_dump_cb, NULL, 5000);
}

void
ubus_init(void)
{
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);
}

void
ubus_uninit(void)
{
	ubus_auto_shutdown(&conn);
}
