// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <libubus.h>

#include "dhcprelay.h"

enum {
	DS_CONFIG_BRIDGES,
	DS_CONFIG_DEVICES,
	__DS_CONFIG_MAX
};

static const struct blobmsg_policy dhcprelay_config_policy[__DS_CONFIG_MAX] = {
	[DS_CONFIG_BRIDGES] = { "bridges", BLOBMSG_TYPE_TABLE },
	[DS_CONFIG_DEVICES] = { "devices", BLOBMSG_TYPE_ARRAY },
};

static struct blob_buf b;

static int
dhcprelay_ubus_config(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *req, const char *method,
		   struct blob_attr *msg)
{
	struct blob_attr *tb[__DS_CONFIG_MAX];

	blobmsg_parse(dhcprelay_config_policy, __DS_CONFIG_MAX, tb,
		      blobmsg_data(msg), blobmsg_len(msg));

	dhcprelay_dev_config_update(tb[DS_CONFIG_BRIDGES], tb[DS_CONFIG_DEVICES]);

	return 0;
}


static int
dhcprelay_ubus_check_devices(struct ubus_context *ctx, struct ubus_object *obj,
			  struct ubus_request_data *req, const char *method,
			  struct blob_attr *msg)
{
	dhcprelay_update_devices();

	return 0;
}

static const struct ubus_method dhcprelay_methods[] = {
	UBUS_METHOD("config", dhcprelay_ubus_config, dhcprelay_config_policy),
	UBUS_METHOD_NOARG("check_devices", dhcprelay_ubus_check_devices),
};

static struct ubus_object_type dhcprelay_object_type =
	UBUS_OBJECT_TYPE("dhcprelay", dhcprelay_methods);

static struct ubus_object dhcprelay_object = {
	.name = "dhcprelay",
	.type = &dhcprelay_object_type,
	.methods = dhcprelay_methods,
	.n_methods = ARRAY_SIZE(dhcprelay_methods),
};

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	ubus_add_object(ctx, &dhcprelay_object);
}

static struct ubus_auto_conn conn;

void dhcprelay_ubus_init(void)
{
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);
}

void dhcprelay_ubus_done(void)
{
	ubus_auto_shutdown(&conn);
	blob_buf_free(&b);
}

static bool
dhcprelay_vlan_match(struct blob_attr *vlans, int id)
{
	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, vlans, rem) {
		unsigned int vlan;

		switch (blobmsg_type(cur)) {
			case BLOBMSG_TYPE_STRING:
				vlan = atoi(blobmsg_get_string(cur));
				break;
			case BLOBMSG_TYPE_INT32:
				vlan = blobmsg_get_u32(cur);
				break;
			default:
				continue;
		}


		if (vlan == id)
			return true;
	}

	return false;
}

static bool
dhcprelay_ignore_match(struct blob_attr *ignore, const char *name)
{
	struct blob_attr *cur;
	int rem;

	if (!ignore)
		return false;

	blobmsg_for_each_attr(cur, ignore, rem) {
		if (!strcmp(blobmsg_get_string(cur), name))
			return true;
	}

	return false;
}

static void
dhcprelay_parse_port_list(struct bridge_entry *br, struct blob_attr *attr)
{
	struct blob_attr *cur;
	int rem;

	if (blobmsg_check_array(attr, BLOBMSG_TYPE_STRING) < 0)
		return;

	blobmsg_for_each_attr(cur, attr, rem) {
		char *sep, *name = blobmsg_get_string(cur);
		bool tagged = false;

		sep = strchr(name, '\n');
		if (sep)
			*sep = 0;

		sep = strchr(name, ':');
		if (sep) {
			*sep = 0;
			tagged = strchr(sep + 1, 't');
		}

		if (dhcprelay_ignore_match(br->ignore, name))
			continue;

		if (tagged)
			continue;

		dhcprelay_dev_add(blobmsg_get_string(cur), false);
	}
}

static void
dhcprelay_parse_vlan(struct bridge_entry *br, struct blob_attr *attr)
{
	enum {
		VL_ATTR_ID,
		VL_ATTR_PORTS,
		__VL_ATTR_MAX,
	};
	struct blobmsg_policy policy[__VL_ATTR_MAX] = {
		[VL_ATTR_ID] = { "id", BLOBMSG_TYPE_INT32 },
		[VL_ATTR_PORTS] = { "ports", BLOBMSG_TYPE_ARRAY },
	};
	struct blob_attr *tb[__VL_ATTR_MAX];
	int id;

	blobmsg_parse(policy, __VL_ATTR_MAX, tb, blobmsg_data(attr), blobmsg_len(attr));

	if (!tb[VL_ATTR_ID] || !tb[VL_ATTR_PORTS])
		return;

	id = blobmsg_get_u32(tb[VL_ATTR_ID]);
	if (!dhcprelay_vlan_match(br->vlans, id))
		return;

	dhcprelay_parse_port_list(br, tb[VL_ATTR_PORTS]);
}

static void
dhcprelay_parse_bridge_ports(struct bridge_entry *br, struct blob_attr *msg)
{
	struct blobmsg_policy policy = { "bridge-members", BLOBMSG_TYPE_ARRAY };
	struct blob_attr *attr;

	blobmsg_parse(&policy, 1, &attr, blobmsg_data(msg), blobmsg_len(msg));
	if (!attr)
		return;

	dhcprelay_parse_port_list(br, attr);
}

static void
dhcprelay_query_bridge_cb(struct ubus_request *req, int type,
			  struct blob_attr *msg)
{
	struct blobmsg_policy policy = { "bridge-vlans", BLOBMSG_TYPE_ARRAY };
	struct bridge_entry *br = req->priv;
	struct blob_attr *attr, *cur;
	int rem;

	if (!br->vlans) {
		dhcprelay_parse_bridge_ports(br, msg);
		return;
	}

	blobmsg_parse(&policy, 1, &attr, blobmsg_data(msg), blobmsg_len(msg));
	if (!attr)
		return;

	blobmsg_for_each_attr(cur, attr, rem)
		dhcprelay_parse_vlan(br, cur);
}

void dhcprelay_ubus_query_bridge(struct bridge_entry *br)
{
	struct ubus_request req;
	uint32_t id;

	if (ubus_lookup_id(&conn.ctx, "network.device", &id))
		return;

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "name", bridge_entry_name(br));
	if (ubus_invoke_async(&conn.ctx, id, "status", b.head, &req))
		return;

	req.priv = br;
	req.data_cb = dhcprelay_query_bridge_cb;
	ubus_complete_request(&conn.ctx, &req, 1000);
}

static void dhcprelay_notify_cb(struct ubus_notify_request *req,
				int type, struct blob_attr *msg)
{
	struct packet *pkt = req->req.priv;

	dhcprelay_forward_request(pkt, msg);
}

void dhcprelay_ubus_notify(const char *type, struct packet *pkt)
{
	const uint8_t *msg = pkt->data;
	struct ubus_notify_request req;
	size_t len = pkt->len;
	char *buf;
	void *c;

	if (!dhcprelay_object.has_subscribers)
		return;

	blob_buf_init(&b, 0);

	c = blobmsg_open_table(&b, "info");
	buf = blobmsg_alloc_string_buffer(&b, "device", IFNAMSIZ + 1);
	if (!if_indextoname(pkt->l2.ifindex, buf))
		return;
	blobmsg_add_string_buffer(&b);
	if (pkt->l2.vlan_proto) {
		blobmsg_add_u32(&b, "vlan_proto", pkt->l2.vlan_proto);
		blobmsg_add_u32(&b, "vlan_tci", pkt->l2.vlan_tci);
	}
	blobmsg_close_table(&b, c);

	buf = blobmsg_alloc_string_buffer(&b, "packet", 2 * len + 1);
	while (len > 0) {
		buf += sprintf(buf, "%02x", *msg);
		msg++;
		len--;
	}
	blobmsg_add_string_buffer(&b);

	ubus_notify_async(&conn.ctx, &dhcprelay_object, type, b.head, &req);
	req.data_cb = dhcprelay_notify_cb;
	req.req.priv = pkt;
	ubus_complete_request(&conn.ctx, &req.req, 5000);
}
