// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#include <libubus.h>

#include "atf.h"

#define HOSTAPD_PREFIX "hostapd."

static struct ubus_auto_conn conn;
static struct blob_buf b;

enum {
	ATF_CONFIG_RESET,
	ATF_CONFIG_VO_Q_WEIGHT,
	ATF_CONFIG_MIN_PKT_THRESH,
	ATF_CONFIG_BULK_PERCENT_THR,
	ATF_CONFIG_PRIO_PERCENT_THR,

	ATF_CONFIG_WEIGHT_NORMAL,
	ATF_CONFIG_WEIGHT_PRIO,
	ATF_CONFIG_WEIGHT_BULK,
	__ATF_CONFIG_MAX
};

static const struct blobmsg_policy atf_config_policy[__ATF_CONFIG_MAX] = {
	[ATF_CONFIG_VO_Q_WEIGHT] = { "vo_queue_weight", BLOBMSG_TYPE_INT32 },
	[ATF_CONFIG_MIN_PKT_THRESH] = { "update_pkt_threshold", BLOBMSG_TYPE_INT32 },
	[ATF_CONFIG_BULK_PERCENT_THR] = { "bulk_percent_thresh", BLOBMSG_TYPE_INT32 },
	[ATF_CONFIG_PRIO_PERCENT_THR] = { "prio_percent_thresh", BLOBMSG_TYPE_INT32 },
	[ATF_CONFIG_WEIGHT_NORMAL] = { "weight_normal", BLOBMSG_TYPE_INT32 },
	[ATF_CONFIG_WEIGHT_PRIO] = { "weight_prio", BLOBMSG_TYPE_INT32 },
	[ATF_CONFIG_WEIGHT_BULK] = { "weight_bulk", BLOBMSG_TYPE_INT32 },
};

static int
atf_ubus_config(struct ubus_context *ctx, struct ubus_object *obj,
		struct ubus_request_data *req, const char *method,
		struct blob_attr *msg)
{
	struct blob_attr *tb[__ATF_CONFIG_MAX];
	struct blob_attr *cur;
	static const struct {
		int id;
		int *field;
	} field_map[] = {
		{ ATF_CONFIG_VO_Q_WEIGHT, &config.voice_queue_weight },
		{ ATF_CONFIG_MIN_PKT_THRESH, &config.min_pkt_thresh },
		{ ATF_CONFIG_BULK_PERCENT_THR, &config.bulk_percent_thresh },
		{ ATF_CONFIG_PRIO_PERCENT_THR, &config.prio_percent_thresh },
		{ ATF_CONFIG_WEIGHT_NORMAL, &config.weight_normal },
		{ ATF_CONFIG_WEIGHT_PRIO, &config.weight_prio },
		{ ATF_CONFIG_WEIGHT_BULK, &config.weight_bulk },
	};
	bool reset = false;
	int i;

	blobmsg_parse(atf_config_policy, __ATF_CONFIG_MAX, tb,
		      blobmsg_data(msg), blobmsg_len(msg));

	if ((cur = tb[ATF_CONFIG_RESET]) != NULL)
		reset = blobmsg_get_bool(cur);

	if (reset)
		reset_config();

	for (i = 0; i < ARRAY_SIZE(field_map); i++) {
		if ((cur = tb[field_map[i].id]) != NULL)
			*(field_map[i].field) = blobmsg_get_u32(cur);
	}

	return 0;
}


static const struct ubus_method atf_methods[] = {
	UBUS_METHOD("config", atf_ubus_config, atf_config_policy),
};

static struct ubus_object_type atf_object_type =
	UBUS_OBJECT_TYPE("atfpolicy", atf_methods);

static struct ubus_object atf_object = {
	.name = "atfpolicy",
	.type = &atf_object_type,
	.methods = atf_methods,
	.n_methods = ARRAY_SIZE(atf_methods),
};

static void
atf_ubus_add_interface(struct ubus_context *ctx, const char *name)
{
	struct atf_interface *iface;

	iface = atf_interface_get(name + strlen(HOSTAPD_PREFIX));
	if (!iface)
		return;

	iface->ubus_obj = 0;
	ubus_lookup_id(ctx, name, &iface->ubus_obj);
	D("add interface %s", name + strlen(HOSTAPD_PREFIX));
}

static void
atf_ubus_lookup_cb(struct ubus_context *ctx, struct ubus_object_data *obj,
		   void *priv)
{
	if (!strncmp(obj->path, HOSTAPD_PREFIX, strlen(HOSTAPD_PREFIX)))
		atf_ubus_add_interface(ctx, obj->path);
}

void atf_ubus_set_sta_weight(struct atf_interface *iface, struct atf_station *sta)
{
	D("set sta "MAC_ADDR_FMT" weight=%d", MAC_ADDR_DATA(sta->macaddr), sta->weight);
	blob_buf_init(&b, 0);
	blobmsg_printf(&b, "sta", MAC_ADDR_FMT, MAC_ADDR_DATA(sta->macaddr));
	blobmsg_add_u32(&b, "weight", sta->weight);
	if (ubus_invoke(&conn.ctx, iface->ubus_obj, "update_airtime", b.head, NULL, NULL, 100))
		D("set airtime weight failed");
}

static void
atf_ubus_event_cb(struct ubus_context *ctx, struct ubus_event_handler *ev,
		  const char *type, struct blob_attr *msg)
{
	static const struct blobmsg_policy policy =
		{ "path", BLOBMSG_TYPE_STRING };
	struct ubus_object_data obj;
	struct blob_attr *attr;

	blobmsg_parse(&policy, 1, &attr, blobmsg_data(msg), blobmsg_len(msg));

	if (!attr)
		return;

	obj.path = blobmsg_get_string(attr);
	atf_ubus_lookup_cb(ctx, &obj, NULL);
}

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	static struct ubus_event_handler ev = {
		.cb = atf_ubus_event_cb
	};

	ubus_add_object(ctx, &atf_object);
	ubus_register_event_handler(ctx, &ev, "ubus.object.add");
	ubus_lookup(ctx, "hostapd.*", atf_ubus_lookup_cb, NULL);
}

int atf_ubus_init(void)
{
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);

	return 0;
}

void atf_ubus_stop(void)
{
	ubus_auto_shutdown(&conn);
}
