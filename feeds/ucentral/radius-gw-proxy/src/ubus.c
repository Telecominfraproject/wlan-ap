/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>

#include <libubox/ulog.h>
#include <libubus.h>
#include "ubus.h"

struct ubus_auto_conn conn;
uint32_t ucentral;

enum {
	RADIUS_TYPE,
	RADIUS_DATA,
	__RADIUS_MAX,
};

static const struct blobmsg_policy frame_policy[__RADIUS_MAX] = {
	[RADIUS_TYPE] = { .name = "radius", .type = BLOBMSG_TYPE_STRING },
	[RADIUS_DATA] = { .name = "data", .type = BLOBMSG_TYPE_STRING },
};

static int ubus_frame_cb(struct ubus_context *ctx,
			 struct ubus_object *obj,
			 struct ubus_request_data *req,
			 const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[__RADIUS_MAX] = {};
	enum socket_type type;
	char *radius, *data;

	blobmsg_parse(frame_policy, __RADIUS_MAX, tb, blobmsg_data(msg), blobmsg_data_len(msg));
	if (!tb[RADIUS_TYPE] || !tb[RADIUS_DATA])
		return UBUS_STATUS_INVALID_ARGUMENT;

	radius = blobmsg_get_string(tb[RADIUS_TYPE]);
	data = blobmsg_get_string(tb[RADIUS_DATA]);

	if (!strcmp(radius, "auth"))
		type = RADIUS_AUTH;
	else if (!strcmp(radius, "acct"))
		type = RADIUS_ACCT;
	else if (!strcmp(radius, "coa"))
		type = RADIUS_DAS;
	else
		return UBUS_STATUS_INVALID_ARGUMENT;

	gateway_recv(data, type);

	return UBUS_STATUS_OK;
}
static const struct ubus_method ucentral_methods[] = {
	UBUS_METHOD("frame", ubus_frame_cb, frame_policy),
};

static struct ubus_object_type ubus_object_type =
	UBUS_OBJECT_TYPE("radius.proxy", ucentral_methods);

struct ubus_object ubus_object = {
	.name = "radius.proxy",
	.type = &ubus_object_type,
	.methods = ucentral_methods,
	.n_methods = ARRAY_SIZE(ucentral_methods),
};

static void
ubus_event_handler_cb(struct ubus_context *ctx,  struct ubus_event_handler *ev,
		      const char *type, struct blob_attr *msg)
{
	enum {
		EVENT_ID,
		EVENT_PATH,
		__EVENT_MAX
	};

	static const struct blobmsg_policy status_policy[__EVENT_MAX] = {
		[EVENT_ID] = { .name = "id", .type = BLOBMSG_TYPE_INT32 },
		[EVENT_PATH] = { .name = "path", .type = BLOBMSG_TYPE_STRING },
	};

	struct blob_attr *tb[__EVENT_MAX];
	char *path;
	uint32_t id;

	blobmsg_parse(status_policy, __EVENT_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[EVENT_ID] || !tb[EVENT_PATH])
		return;

	path = blobmsg_get_string(tb[EVENT_PATH]);
	id = blobmsg_get_u32(tb[EVENT_ID]);

	if (strcmp(path, "ucentral"))
		return;
	if (!strcmp("ubus.object.remove", type))
		ucentral = 0;
	else
		ucentral = id;
}

static struct ubus_event_handler ubus_event_handler = { .cb = ubus_event_handler_cb };

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	ubus_add_object(ctx, &ubus_object);
	ubus_register_event_handler(ctx, &ubus_event_handler, "ubus.object.add");
	ubus_register_event_handler(ctx, &ubus_event_handler, "ubus.object.remove");

	ubus_lookup_id(ctx, "ucentral", &ucentral);
}

void ubus_init(void)
{
	memset(&conn, 0, sizeof(conn));
	ucentral = 0;
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);
}

void ubus_deinit(void)
{
	ubus_auto_shutdown(&conn);
}
