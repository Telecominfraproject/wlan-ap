#include <syslog.h>
#include "target.h"

#include "radio.h"
#include "ubus.h"
static struct ubus_context *ubus;
extern struct ev_loop *wifihal_evloop;

int hapd_rrm_enable(char *name, int neighbor, int beacon)
{
	uint32_t id = ubus_lookup_remote(name);

	if (!ubus || !id)
		return -1;

	blob_buf_init(&b, 0);
	blobmsg_add_u8(&b, "neighbor_report", neighbor);
	blobmsg_add_u8(&b, "beacon_report", beacon);

	return ubus_invoke(ubus, id, "bss_mgmt_enable", b.head, NULL, NULL, 1000);
}

int hapd_rrm_set_neighbors(char *name, struct rrm_neighbor *neigh, int count)
{
	uint32_t id = ubus_lookup_remote(name);
	struct blob_attr *l;
	int i;

	if (!ubus || !id)
		return -1;

	blob_buf_init(&b, 0);
	l = blobmsg_open_array(&b, "list");
	for (i = 0; i < count; i++) {
		struct blob_attr *n;

		n = blobmsg_open_array(&b, NULL);
		blobmsg_add_string(&b, NULL, neigh[i].mac);
		blobmsg_add_string(&b, NULL, neigh[i].ssid);
		blobmsg_add_string(&b, NULL, neigh[i].ie);
		blobmsg_close_array(&b, n);
	}
	blobmsg_close_array(&b, l);

	return ubus_invoke(ubus, id, "rrm_nr_set", b.head, NULL, NULL, 1000);
}

enum {
	DUMMY_PARAMETER,
	__DUMMY_MAX,
};

static const struct blobmsg_policy dummy_policy[__DUMMY_MAX] = {
	[DUMMY_PARAMETER] = { .name = "dummy", .type = BLOBMSG_TYPE_INT32 },
};

static int
radio_ubus_dummy_cb(struct ubus_context *ctx, struct ubus_object *obj,
		    struct ubus_request_data *req, const char *method,
		    struct blob_attr *msg)
{
	struct blob_attr *tb[__DUMMY_MAX] = { };

	if (!msg)
		return UBUS_STATUS_INVALID_ARGUMENT;

	blobmsg_parse(dummy_policy, __DUMMY_MAX, tb, blob_data(msg), blob_len(msg));
	blob_buf_init(&b, 0);

	if (tb[DUMMY_PARAMETER])
		blobmsg_add_u32(&b, "dummy", blobmsg_get_u32(tb[DUMMY_PARAMETER]));
	blobmsg_add_string(&b, "foo", "bar");
	ubus_send_reply(ctx, req, b.head);

	{
		/* dummy code to test the ubus invoke helpers
		 * us this to see the tabel inside hapd ubus call hostapd.home_ap_u50 rrm_nr_list  
		 * we want to replace struct rrm_neighbor with the table from opensync
		 */
		struct rrm_neighbor neigh[] = {
			{
				.mac = "00:11:22:33:44:55",
				.ssid = "foo",
				.ie = "c6411e227109ef1900008095090603029b00",
			}, {
				.mac = "00:11:22:33:44:56",
				.ssid = "bar",
				.ie = "c6411e227109ef1900008095090603029b00",
			}
		};
		hapd_rrm_enable("home_ap_u50", 1, 1);
		hapd_rrm_set_neighbors("home_ap_u50", neigh, 2);
	}
	return UBUS_STATUS_OK;
}

static const struct ubus_method radio_ubus_methods[] = {
        UBUS_METHOD("dummy", radio_ubus_dummy_cb, dummy_policy),
};

static struct ubus_object_type radio_ubus_object_type =
        UBUS_OBJECT_TYPE("osync-wm", radio_ubus_methods);

static struct ubus_object radio_ubus_object = {
        .name = "osync-wm",
        .type = &radio_ubus_object_type,
        .methods = radio_ubus_methods,
        .n_methods = ARRAY_SIZE(radio_ubus_methods),
};

static void radio_ubus_connect(struct ubus_context *ctx)
{
	ubus = ctx;
	ubus_add_object(ubus, &radio_ubus_object);
}

static int radio_ubus_notify(struct ubus_context *ctx, struct ubus_object *obj,
			     struct ubus_request_data *req, const char *method,
			     struct blob_attr *msg)
{
	char *str;

	str = blobmsg_format_json(msg, true);
	LOGN("ubus: Received ubus notify '%s': %s\n", method, str);
	free(str);

	return 0;
}

static struct ubus_instance ubus_instance = {
	.connect = radio_ubus_connect,
	.notify = radio_ubus_notify,
	.list = {
			{
				.path = "hostapd.",
				.wildcard = 1,
			},
		},
	.len = 1,
};

int radio_ubus_init(void)
{
	return ubus_init(&ubus_instance, wifihal_evloop);
}
