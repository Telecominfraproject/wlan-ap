/* SPDX-License-Identifier: BSD-3-Clause */

#include <syslog.h>
#include "target.h"

#include "evsched.h"
#include "netifd.h"
#include "ubus.h"

#include "inet_dhcp_ubus.h"

static struct ubus_context *ubus;
static struct blob_buf b_ev;

extern struct avl_tree dhcp_event_tree;
uint32_t cached_dhcp_events_nr = 0;

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

static int ubus_get_dhcp_transactions_cb(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	void *a = NULL;
	void *t = NULL;
	void *t_msg = NULL;

	struct dhcp_event_avl_rec *rec = NULL;
	struct dhcp_transaction *e_rec = NULL;

	blob_buf_init(&b_ev, 0);
	a = blobmsg_open_table(&b_ev, "transactions");
	avl_for_each_element(&dhcp_event_tree, rec, avl)
	{
		/* open dhcp transaction */
		t = blobmsg_open_table(&b_ev, "DhcpTransaction");
		blobmsg_add_u32(&b_ev, "x_id", rec->x_id);
		/* messages for current transaction */
		for(size_t i = 0; i < rec->dhcp_records_nr; i++) {
			e_rec = &rec->dhcp_records[i];
			/* check message type */
			switch (e_rec->type) {
				case INET_UBUS_DHCP_ACK: {
					struct dhcp_ack_event *e = &e_rec->u.dhcp_ack;
					t_msg = blobmsg_open_table(&b_ev, "DhcpAckEvent");
					blobmsg_add_u32(&b_ev, "x_id", e->dhcp_common.x_id);
					blobmsg_add_u32(&b_ev, "vlan_id", e->dhcp_common.vlan_id);
					blobmsg_add_string(&b_ev, "dhcp_server_ip", e->dhcp_common.dhcp_server_ip);
					blobmsg_add_string(&b_ev, "client_ip", e->dhcp_common.client_ip);
					blobmsg_add_string(&b_ev, "relay_ip", e->dhcp_common.relay_ip);
					blobmsg_add_string(&b_ev, "device_mac_address", e->dhcp_common.device_mac_address);
					blobmsg_add_u64(&b_ev, "timestamp_ms", e->dhcp_common.timestamp_ms);
					blobmsg_add_string(&b_ev, "subnet_mask", e->subnet_mask);
					blobmsg_add_string(&b_ev, "primary_dns", e->primary_dns);
					blobmsg_add_string(&b_ev, "secondary_dns", e->secondary_dns);
					blobmsg_add_u32(&b_ev, "lease_time", e->lease_time);
					blobmsg_add_u32(&b_ev, "renewal_time", e->renewal_time);
					blobmsg_add_u32(&b_ev, "rebinding_time", e->rebinding_time);
					blobmsg_add_u32(&b_ev, "time_offset", e->time_offset);
					blobmsg_add_string(&b_ev, "gateway_ip", e->gateway_ip);
					blobmsg_close_table(&b_ev, t_msg);
					break;
				}

				case INET_UBUS_DHCP_NAK: {
					struct dhcp_nak_event *e = &e_rec->u.dhcp_nak;
					t_msg = blobmsg_open_table(&b_ev, "DhcpNakEvent");
					blobmsg_add_u32(&b_ev, "x_id", e->dhcp_common.x_id);
					blobmsg_add_u32(&b_ev, "vlan_id", e->dhcp_common.vlan_id);
					blobmsg_add_string(&b_ev, "dhcp_server_ip", e->dhcp_common.dhcp_server_ip);
					blobmsg_add_string(&b_ev, "client_ip", e->dhcp_common.client_ip);
					blobmsg_add_string(&b_ev, "relay_ip", e->dhcp_common.relay_ip);
					blobmsg_add_string(&b_ev, "device_mac_address", e->dhcp_common.device_mac_address);
					blobmsg_add_u64(&b_ev, "timestamp_ms", e->dhcp_common.timestamp_ms);
					blobmsg_add_u8(&b_ev, "from_internal", e->from_internal);
					blobmsg_close_table(&b_ev, t_msg);
					break;
				}

				case INET_UBUS_DHCP_OFFER: {
					struct dhcp_offer_event *e = &e_rec->u.dhcp_offer;
					t_msg = blobmsg_open_table(&b_ev, "DhcpOfferEvent");
					blobmsg_add_u32(&b_ev, "x_id", e->dhcp_common.x_id);
					blobmsg_add_u32(&b_ev, "vlan_id", e->dhcp_common.vlan_id);
					blobmsg_add_string(&b_ev, "dhcp_server_ip", e->dhcp_common.dhcp_server_ip);
					blobmsg_add_string(&b_ev, "client_ip", e->dhcp_common.client_ip);
					blobmsg_add_string(&b_ev, "relay_ip", e->dhcp_common.relay_ip);
					blobmsg_add_string(&b_ev, "device_mac_address", e->dhcp_common.device_mac_address);
					blobmsg_add_u64(&b_ev, "timestamp_ms", e->dhcp_common.timestamp_ms);
					blobmsg_add_u8(&b_ev, "from_internal", e->from_internal);
					blobmsg_close_table(&b_ev, t_msg);
					break;
				}

				case INET_UBUS_DHCP_INFORM: {
					struct dhcp_inform_event *e = &e_rec->u.dhcp_inform;
					t_msg = blobmsg_open_table(&b_ev, "DhcpInformEvent");
					blobmsg_add_u32(&b_ev, "x_id", e->dhcp_common.x_id);
					blobmsg_add_u32(&b_ev, "vlan_id", e->dhcp_common.vlan_id);
					blobmsg_add_string(&b_ev, "dhcp_server_ip", e->dhcp_common.dhcp_server_ip);
					blobmsg_add_string(&b_ev, "client_ip", e->dhcp_common.client_ip);
					blobmsg_add_string(&b_ev, "relay_ip", e->dhcp_common.relay_ip);
					blobmsg_add_string(&b_ev, "device_mac_address", e->dhcp_common.device_mac_address);
					blobmsg_add_u64(&b_ev, "timestamp_ms", e->dhcp_common.timestamp_ms);
					blobmsg_close_table(&b_ev, t_msg);
					break;
				}

				case INET_UBUS_DHCP_DECLINE: {
					struct dhcp_decline_event *e = &e_rec->u.dhcp_decline;
					t_msg = blobmsg_open_table(&b_ev, "DhcpDeclineEvent");
					blobmsg_add_u32(&b_ev, "x_id", e->dhcp_common.x_id);
					blobmsg_add_u32(&b_ev, "vlan_id", e->dhcp_common.vlan_id);
					blobmsg_add_string(&b_ev, "dhcp_server_ip", e->dhcp_common.dhcp_server_ip);
					blobmsg_add_string(&b_ev, "client_ip", e->dhcp_common.client_ip);
					blobmsg_add_string(&b_ev, "relay_ip", e->dhcp_common.relay_ip);
					blobmsg_add_string(&b_ev, "device_mac_address", e->dhcp_common.device_mac_address);
					blobmsg_add_u64(&b_ev, "timestamp_ms", e->dhcp_common.timestamp_ms);
					blobmsg_close_table(&b_ev, t_msg);
					break;
				}

				case INET_UBUS_DHCP_REQUEST: {
					struct dhcp_request_event *e = &e_rec->u.dhcp_request;
					t_msg = blobmsg_open_table(&b_ev, "DhcpRequestEvent");
					blobmsg_add_u32(&b_ev, "x_id", e->dhcp_common.x_id);
					blobmsg_add_u32(&b_ev, "vlan_id", e->dhcp_common.vlan_id);
					blobmsg_add_string(&b_ev, "dhcp_server_ip", e->dhcp_common.dhcp_server_ip);
					blobmsg_add_string(&b_ev, "client_ip", e->dhcp_common.client_ip);
					blobmsg_add_string(&b_ev, "relay_ip", e->dhcp_common.relay_ip);
					blobmsg_add_string(&b_ev, "device_mac_address", e->dhcp_common.device_mac_address);
					blobmsg_add_u64(&b_ev, "timestamp_ms", e->dhcp_common.timestamp_ms);
					blobmsg_add_string(&b_ev, "hostname", e->hostname);
					blobmsg_close_table(&b_ev, t_msg);
					break;
				}

				case INET_UBUS_DHCP_DISCOVER: {
					struct dhcp_discover_event *e = &e_rec->u.dhcp_discover;
					t_msg = blobmsg_open_table(&b_ev, "DhcpDiscoverEvent");
					blobmsg_add_u32(&b_ev, "x_id", e->dhcp_common.x_id);
					blobmsg_add_u32(&b_ev, "vlan_id", e->dhcp_common.vlan_id);
					blobmsg_add_string(&b_ev, "dhcp_server_ip", e->dhcp_common.dhcp_server_ip);
					blobmsg_add_string(&b_ev, "client_ip", e->dhcp_common.client_ip);
					blobmsg_add_string(&b_ev, "relay_ip", e->dhcp_common.relay_ip);
					blobmsg_add_string(&b_ev, "device_mac_address", e->dhcp_common.device_mac_address);
					blobmsg_add_u64(&b_ev, "timestamp_ms", e->dhcp_common.timestamp_ms);
					blobmsg_add_string(&b_ev, "hostname", e->hostname);
					blobmsg_close_table(&b_ev, t_msg);
					break;
				}

				default:
					break;
			}
		}
		blobmsg_close_table(&b_ev, t);
	}

	blobmsg_close_table(&b_ev, a);
	ubus_send_reply(ctx, req, b_ev.head);
	return UBUS_STATUS_OK;
}

static int ubus_clear_dhcp_transactions_cb(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct dhcp_event_avl_rec *rec = NULL;
	struct dhcp_event_avl_rec *rec_n = NULL;

	avl_remove_all_elements(&dhcp_event_tree, rec, avl, rec_n)
	{
		/* free events array */
		free(rec->dhcp_records);
		free(rec);
	}

	cached_dhcp_events_nr = 0;
	return 0;
}

enum { DHCP_TRANSACTION_X_ID,
	   __DHCP_TRANSACTION_MAX,
};

static const struct blobmsg_policy dhcp_transaction_del_policy[__DHCP_TRANSACTION_MAX] = {
	[DHCP_TRANSACTION_X_ID] = { .name = "x_id", .type = BLOBMSG_TYPE_INT32 },
};

static int ubus_clear_dhcp_transaction_cb(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct blob_attr *tb[__DHCP_TRANSACTION_MAX];
	struct dhcp_event_avl_rec *rec = NULL;
	struct dhcp_event_avl_rec *rec_n = NULL;
	uint32_t x_id = 0;

	blobmsg_parse(dhcp_transaction_del_policy, __DHCP_TRANSACTION_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[DHCP_TRANSACTION_X_ID]) {
		return UBUS_STATUS_INVALID_ARGUMENT;
	}

	x_id = blobmsg_get_u32(tb[DHCP_TRANSACTION_X_ID]);
	/* remove from AVL and dhcp ubus object */
	avl_for_each_element_safe(&dhcp_event_tree, rec, avl, rec_n)
	{
		if (x_id == rec->x_id) {
			cached_dhcp_events_nr -= rec->dhcp_records_nr;
			avl_delete(&dhcp_event_tree, &rec->avl);
			free(rec->dhcp_records);
			free(rec);
		}
	}

	return 0;
}

static const struct ubus_method netifd_ubus_methods[] = {
        UBUS_METHOD_NOARG("get_dhcp_transactions", ubus_get_dhcp_transactions_cb),
        UBUS_METHOD_NOARG("clear_dhcp_transactions", ubus_clear_dhcp_transactions_cb),
        UBUS_METHOD("clear_dhcp_transaction", ubus_clear_dhcp_transaction_cb, dhcp_transaction_del_policy),
};

static struct ubus_object_type netifd_ubus_object_type =
        UBUS_OBJECT_TYPE("osync-dhcp", netifd_ubus_methods);

static struct ubus_object netifd_ubus_object = {
        .name = "osync-dhcp",
        .type = &netifd_ubus_object_type,
        .methods = netifd_ubus_methods,
        .n_methods = ARRAY_SIZE(netifd_ubus_methods),
};

static void netifd_ubus_connect(struct ubus_context *ctx)
{
	ubus = ctx;
	ubus_add_object(ubus, &netifd_ubus_object);
}

static struct ubus_instance ubus_instance = {
	.connect = netifd_ubus_connect,
	.notify = netifd_ubus_notify,
	.list = {
			{
				.path = "network.interface.",
				.wildcard = 1,
			},
			{
				.path = "dnsmasq",
			},
		},
	.len = 2,
};

int netifd_ubus_init(struct ev_loop *loop)
{
	return ubus_init(&ubus_instance, loop);
}
