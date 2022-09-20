// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Felix Fietkau <nbd@nbd.name>
 */
#include <libubus.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <net/if.h>

#include "spotfilter.h"

static struct blob_buf b;

enum {
	IFACE_ATTR_NAME,
	IFACE_ATTR_CONFIG,
	IFACE_ATTR_DEVICES,
	__IFACE_ATTR_MAX,
};

static const struct blobmsg_policy iface_policy[__IFACE_ATTR_MAX] = {
	[IFACE_ATTR_NAME] = { "name", BLOBMSG_TYPE_STRING },
	[IFACE_ATTR_CONFIG] = { "config", BLOBMSG_TYPE_TABLE },
	[IFACE_ATTR_DEVICES] = { "devices", BLOBMSG_TYPE_ARRAY },
};

static int
interface_ubus_add(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *req, const char *method,
		   struct blob_attr *msg)
{
	struct blob_attr *tb[__IFACE_ATTR_MAX];
	struct blob_attr *cur;
	const char *name;

	blobmsg_parse(iface_policy, __IFACE_ATTR_MAX, tb, blobmsg_data(msg), blobmsg_len(msg));

	if ((cur = tb[IFACE_ATTR_NAME]) != NULL)
		name = blobmsg_get_string(tb[IFACE_ATTR_NAME]);
	else
		return UBUS_STATUS_INVALID_ARGUMENT;

	if ((cur = tb[IFACE_ATTR_DEVICES]) != NULL &&
	    blobmsg_check_array(cur, BLOBMSG_TYPE_STRING) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	interface_add(name, tb[IFACE_ATTR_CONFIG], tb[IFACE_ATTR_DEVICES]);

	return 0;
}

static int
interface_ubus_remove(struct ubus_context *ctx, struct ubus_object *obj,
		      struct ubus_request_data *req, const char *method,
		      struct blob_attr *msg)
{
	struct interface *iface;
	struct blob_attr *tb;

	blobmsg_parse(&iface_policy[IFACE_ATTR_NAME], 1, &tb,
		      blobmsg_data(msg), blobmsg_len(msg));

	if (tb)
		return UBUS_STATUS_INVALID_ARGUMENT;

	iface = avl_find_element(&interfaces, blobmsg_get_string(tb), iface, node);
	if (!iface)
		return UBUS_STATUS_NOT_FOUND;

	interface_free(iface);
	return 0;
}

static int
check_devices(struct ubus_context *ctx, struct ubus_object *obj,
	      struct ubus_request_data *req, const char *method,
	      struct blob_attr *msg)
{
	interface_check_devices();
	return 0;
}

enum {
	CLIENT_ATTR_IFACE,
	CLIENT_ATTR_ADDR,
	CLIENT_ATTR_ID,
	CLIENT_ATTR_STATE,
	CLIENT_ATTR_DNS_STATE,
	CLIENT_ATTR_ACCOUNTING,
	CLIENT_ATTR_DATA,
	CLIENT_ATTR_FLUSH,
	__CLIENT_ATTR_MAX
};

static const struct blobmsg_policy client_policy[__CLIENT_ATTR_MAX] = {
	[CLIENT_ATTR_IFACE] = { "interface", BLOBMSG_TYPE_STRING },
	[CLIENT_ATTR_ADDR] = { "address", BLOBMSG_TYPE_STRING },
	[CLIENT_ATTR_ID] = { "id", BLOBMSG_TYPE_STRING },
	[CLIENT_ATTR_STATE] = { "state", BLOBMSG_TYPE_INT32 },
	[CLIENT_ATTR_DNS_STATE] = { "dns_state", BLOBMSG_TYPE_INT32 },
	[CLIENT_ATTR_ACCOUNTING] = { "accounting", BLOBMSG_TYPE_ARRAY },
	[CLIENT_ATTR_DATA] = { "data", BLOBMSG_TYPE_TABLE },
	[CLIENT_ATTR_FLUSH] = { "flush", BLOBMSG_TYPE_BOOL },
};

static int
client_ubus_init(struct blob_attr *msg, struct blob_attr **tb,
		 struct interface **iface, const void **addr,
		 const char **id, struct client **cl)
{
	struct blob_attr *cur;

	blobmsg_parse(client_policy, __CLIENT_ATTR_MAX, tb,
		      blobmsg_data(msg), blobmsg_len(msg));

	if ((cur = tb[CLIENT_ATTR_IFACE]) != NULL)
		*iface = avl_find_element(&interfaces, blobmsg_get_string(cur),
					  *iface, node);
	else
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (!*iface)
		return UBUS_STATUS_NOT_FOUND;

	if ((cur = tb[CLIENT_ATTR_ADDR]) != NULL)
		*addr = ether_aton(blobmsg_get_string(cur));
	else
		*addr = NULL;

	if ((cur = tb[CLIENT_ATTR_ID]) != NULL)
		*id = blobmsg_get_string(cur);
	else
		*id = NULL;

	if (*addr)
		*cl = avl_find_element(&(*iface)->clients, *addr, *cl, node);
	else if (*id)
		*cl = avl_find_element(&(*iface)->client_ids, *id, *cl, id_node);
	else
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (*cl && !*addr)
		*addr = (*cl)->node.key;

	return 0;
}

static int
client_accounting_flags(struct blob_attr *attr)
{
	struct blob_attr *cur;
	int flags = 0;
	int rem;

	blobmsg_for_each_attr(cur, attr, rem) {
		const char *val = blobmsg_get_string(cur);

		if (!strcmp(val, "ul"))
			flags |= SPOTFILTER_CLIENT_F_ACCT_UL;
		else if (!strcmp(val, "dl"))
			flags |= SPOTFILTER_CLIENT_F_ACCT_DL;
	}

	return flags;
}


static int
client_ubus_update(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *req, const char *method,
		   struct blob_attr *msg)
{
	struct blob_attr *tb[__CLIENT_ATTR_MAX];
	struct interface *iface = NULL;
	struct blob_attr *cur;
	struct client *cl = NULL;
	const void *addr = NULL;
	const char *id = NULL;
	int state = -1, dns_state = -1;
	int accounting = -1;
	bool flush = false;
	int ret;

	ret = client_ubus_init(msg, tb, &iface, &addr, &id, &cl);
	if (ret)
		return ret;

	if ((cur = tb[CLIENT_ATTR_STATE]) != NULL)
		dns_state = state = blobmsg_get_u32(cur);

	if ((cur = tb[CLIENT_ATTR_DNS_STATE]) != NULL)
		dns_state = blobmsg_get_u32(cur);

	if ((cur = tb[CLIENT_ATTR_ACCOUNTING]) != NULL &&
	    blobmsg_check_array(cur, BLOBMSG_TYPE_STRING) >= 0)
		accounting = client_accounting_flags(cur);

	if (!strcmp(method, "client_remove")) {
		if (!cl)
			return UBUS_STATUS_NOT_FOUND;

		client_free(iface, cl);
		return 0;
	}

	if (!addr)
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (tb[CLIENT_ATTR_FLUSH])
		flush = blobmsg_get_bool(tb[CLIENT_ATTR_FLUSH]);

	client_set(iface, addr, id, state, dns_state, accounting,
		   tb[CLIENT_ATTR_DATA], NULL, flush);

	return 0;
}

static void
interface_dump_action(struct blob_buf *buf, struct interface *iface, uint8_t class)
{
	struct spotfilter_bpf_class *c = &iface->cdata[class];
	char ifname[IFNAMSIZ + 1];

	if (!(c->actions & SPOTFILTER_ACTION_VALID)) {
		blobmsg_add_u8(buf, "invalid", 1);
		return;
	}

	if (c->actions & SPOTFILTER_ACTION_FWMARK) {
		blobmsg_add_u32(buf, "fwmark", c->fwmark_val);
		blobmsg_add_u32(buf, "fwmark_mask", c->fwmark_mask);
	}

	if (c->actions & SPOTFILTER_ACTION_REDIRECT)
		blobmsg_add_string(buf, "redirect", if_indextoname(c->redirect_ifindex, ifname));

	if (c->actions & SPOTFILTER_ACTION_SET_DEST_MAC)
		blobmsg_add_string(buf, "dest_mac", ether_ntoa((const void *)c->dest_mac));
}

static void client_dump(struct interface *iface, struct client *cl)
{
	struct blob_attr *val;
	const char *name;
	char *buf;
	void *c;

	spotfilter_bpf_get_client(iface, &cl->key, &cl->data);

	if (cl->device)
		blobmsg_add_string(&b, "device", cl->device);

	blobmsg_add_u32(&b, "idle", cl->idle);

	blobmsg_add_u32(&b, "state", cl->data.cur_class);
	blobmsg_add_u32(&b, "dns_state", cl->data.dns_class);
	if (cl->id_node.key)
		blobmsg_add_string(&b, "id", (const char *)cl->id_node.key);

	if (cl->data.ip4addr) {
		buf = blobmsg_alloc_string_buffer(&b, "ip4addr", INET6_ADDRSTRLEN);
		inet_ntop(AF_INET, (const void *)&cl->data.ip4addr, buf, INET6_ADDRSTRLEN);
		blobmsg_add_string_buffer(&b);
	}

	if (cl->data.ip6addr[0]) {
		buf = blobmsg_alloc_string_buffer(&b, "ip6addr", INET6_ADDRSTRLEN);
		inet_ntop(AF_INET6, (const void *)cl->data.ip6addr, buf, INET6_ADDRSTRLEN);
		blobmsg_add_string_buffer(&b);
	}

	c = blobmsg_open_array(&b, "accounting");
	if (cl->data.flags & SPOTFILTER_CLIENT_F_ACCT_UL)
		blobmsg_add_string(&b, NULL, "ul");
	if (cl->data.flags & SPOTFILTER_CLIENT_F_ACCT_DL)
		blobmsg_add_string(&b, NULL, "dl");
	blobmsg_close_table(&b, c);

	c = blobmsg_open_table(&b, "data");
	kvlist_for_each(&cl->kvdata, name, val)
		blobmsg_add_blob(&b, val);
	blobmsg_close_table(&b, c);

	c = blobmsg_open_table(&b, "action");
	interface_dump_action(&b, iface, cl->data.cur_class);
	blobmsg_close_table(&b, c);

	c = blobmsg_open_table(&b, "dns_action");
	interface_dump_action(&b, iface, cl->data.dns_class);
	blobmsg_close_table(&b, c);

	blobmsg_add_u64(&b, "packets_ul", cl->data.packets_ul);
	blobmsg_add_u64(&b, "packets_dl", cl->data.packets_dl);
	blobmsg_add_u64(&b, "bytes_ul", cl->data.bytes_ul);
	blobmsg_add_u64(&b, "bytes_dl", cl->data.bytes_dl);
}

static int
client_ubus_get(struct ubus_context *ctx, struct ubus_object *obj,
		struct ubus_request_data *req, const char *method,
		struct blob_attr *msg)
{
	struct blob_attr *tb[__CLIENT_ATTR_MAX];
	struct interface *iface = NULL;
	const void *addr = NULL;
	const char *id = NULL;
	struct client *cl = NULL;
	int ret;

	ret = client_ubus_init(msg, tb, &iface, &addr, &id, &cl);
	if (ret)
		return ret;

	if (!cl)
		return UBUS_STATUS_NOT_FOUND;

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "address", ether_ntoa(cl->node.key));
	client_dump(iface, cl);

	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static int
client_ubus_list(struct ubus_context *ctx, struct ubus_object *obj,
		 struct ubus_request_data *req, const char *method,
		 struct blob_attr *msg)
{
	struct blob_attr *iface_attr;
	struct interface *iface;
	struct client *cl;

	blobmsg_parse(&client_policy[CLIENT_ATTR_IFACE], 1, &iface_attr,
		      blobmsg_data(msg), blobmsg_len(msg));

	if (!iface_attr)
		return UBUS_STATUS_INVALID_ARGUMENT;

	iface = avl_find_element(&interfaces, blobmsg_get_string(iface_attr),
				 iface, node);
	if (!iface)
		return UBUS_STATUS_NOT_FOUND;

	blob_buf_init(&b, 0);
	avl_for_each_element(&iface->clients, cl, node) {
		void *c;

		c = blobmsg_open_table(&b, ether_ntoa(cl->node.key));
		client_dump(iface, cl);
		blobmsg_close_table(&b, c);
	}

	ubus_send_reply(ctx, req, b.head);

	return 0;
}

enum {
	WHITELIST_ATTR_IFACE,
	WHITELIST_ATTR_ADDR,
	WHITELIST_ATTR_STATE,
	__WHITELIST_ATTR_MAX
};

static const struct blobmsg_policy whitelist_policy[__WHITELIST_ATTR_MAX] = {
	[WHITELIST_ATTR_IFACE] = { "interface", BLOBMSG_TYPE_STRING },
	[WHITELIST_ATTR_ADDR] = { "address", BLOBMSG_TYPE_ARRAY },
	[WHITELIST_ATTR_STATE] = { "state", BLOBMSG_TYPE_INT32 },
};

static int
whitelist_update(struct ubus_context *ctx, struct ubus_object *obj,
		 struct ubus_request_data *req, const char *method,
		 struct blob_attr *msg)
{
	struct blob_attr *tb[__WHITELIST_ATTR_MAX];
	struct interface *iface;
	struct blob_attr *cur;
	uint8_t state = 0;
	const uint8_t *val = &state;
	int rem;

	blobmsg_parse(whitelist_policy, __WHITELIST_ATTR_MAX, tb,
		      blobmsg_data(msg), blobmsg_len(msg));

	if ((cur = tb[WHITELIST_ATTR_IFACE]) != NULL)
		iface = avl_find_element(&interfaces, blobmsg_get_string(cur),
					 iface, node);
	else
		return UBUS_STATUS_INVALID_ARGUMENT;

	if ((cur = tb[WHITELIST_ATTR_STATE]) != NULL)
		state = blobmsg_get_u32(cur);

	if ((cur = tb[WHITELIST_ATTR_ADDR]) == NULL ||
	    blobmsg_check_array(cur, BLOBMSG_TYPE_STRING) < 0)
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (!strcmp(method, "whitelist_remove"))
		val = NULL;

	blobmsg_for_each_attr(cur, tb[WHITELIST_ATTR_ADDR], rem) {
		const char *addrstr = blobmsg_get_string(cur);
		bool ipv6 = strchr(addrstr, ':');
		union {
			struct in_addr in;
			struct in6_addr in6;
		} addr = {};

		if (inet_pton(ipv6 ? AF_INET6 : AF_INET, addrstr, &addr) != 1)
			continue;

		spotfilter_bpf_set_whitelist(iface, &addr, ipv6, val);
	}

	return 0;
}

static const struct ubus_method spotfilter_methods[] = {
	UBUS_METHOD_NOARG("check_devices", check_devices),
	UBUS_METHOD("client_set", client_ubus_update, client_policy),
	UBUS_METHOD_MASK("client_remove", client_ubus_update, client_policy,
			 (1 << CLIENT_ATTR_IFACE) | (1 << CLIENT_ATTR_ADDR)),
	UBUS_METHOD_MASK("client_get", client_ubus_get, client_policy,
			 (1 << CLIENT_ATTR_IFACE) | (1 << CLIENT_ATTR_ADDR)),
	UBUS_METHOD_MASK("client_list", client_ubus_list, client_policy,
			 (1 << CLIENT_ATTR_IFACE)),
	UBUS_METHOD("interface_add", interface_ubus_add, iface_policy),
	UBUS_METHOD_MASK("interface_remove", interface_ubus_remove,
			 iface_policy, 1 << IFACE_ATTR_NAME),
	UBUS_METHOD("whitelist_add", whitelist_update, whitelist_policy),
	UBUS_METHOD_MASK("whitelist_remove", whitelist_update, whitelist_policy,
			 (1 << WHITELIST_ATTR_IFACE) | (1 << WHITELIST_ATTR_ADDR)),
};

static struct ubus_object_type spotfilter_object_type =
	UBUS_OBJECT_TYPE("spotfilter", spotfilter_methods);

static struct ubus_object spotfilter_object = {
	.name = "spotfilter",
	.type = &spotfilter_object_type,
	.methods = spotfilter_methods,
	.n_methods = ARRAY_SIZE(spotfilter_methods),
};

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	ubus_add_object(ctx, &spotfilter_object);
}

static struct ubus_auto_conn conn;

void spotfilter_ubus_notify(struct interface *iface, struct client *cl, const char *type)
{
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "interface", interface_name(iface));
	if (cl) {
		blobmsg_add_string(&b, "address", ether_ntoa(cl->node.key));
		if (cl->id_node.key)
			blobmsg_add_string(&b, "id", cl->id_node.key);
	}

	ubus_notify(&conn.ctx, &spotfilter_object, type, b.head, -1);
}

int spotfilter_ubus_init(void)
{
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);

	return 0;
}

void spotfilter_ubus_stop(void)
{
	ubus_auto_shutdown(&conn);
}
