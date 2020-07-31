/* SPDX-License-Identifier: BSD-3-Clause */

#include <syslog.h>
#include "target.h"

#include "radio.h"
#include "phy.h"
#include "ubus.h"

extern struct ev_loop *wifihal_evloop;
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

enum {
	ADD_VIF_RADIO,
	ADD_VIF_NAME,
	ADD_VIF_VID,
	ADD_VIF_NETWORK,
	ADD_VIF_SSID,
	__ADD_VIF_MAX,
};

static const struct blobmsg_policy add_vif_policy[__ADD_VIF_MAX] = {
	[ADD_VIF_RADIO] = { .name = "radio", .type = BLOBMSG_TYPE_STRING },
	[ADD_VIF_NAME] = { .name = "name", .type = BLOBMSG_TYPE_STRING },
	[ADD_VIF_VID] = { .name = "vid", .type = BLOBMSG_TYPE_INT32 },
	[ADD_VIF_NETWORK] = { .name = "network", .type = BLOBMSG_TYPE_STRING },
	[ADD_VIF_SSID] = { .name = "ssid", .type = BLOBMSG_TYPE_STRING },
};

static int
radio_ubus_add_vif_cb(struct ubus_context *ctx, struct ubus_object *obj,
		      struct ubus_request_data *req, const char *method,
		      struct blob_attr *msg)
{
	struct blob_attr *tb[__ADD_VIF_MAX] = { };
	struct schema_Wifi_VIF_Config conf;
	char band[8];
	char *radio;

	if (!msg)
		return UBUS_STATUS_INVALID_ARGUMENT;

	blobmsg_parse(add_vif_policy, __ADD_VIF_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[ADD_VIF_RADIO] || !tb[ADD_VIF_NAME] || !tb[ADD_VIF_VID] ||
	    !tb[ADD_VIF_NETWORK] || !tb[ADD_VIF_SSID])
		return UBUS_STATUS_INVALID_ARGUMENT;

	radio = blobmsg_get_string(tb[ADD_VIF_RADIO]);

	memset(&conf, 0, sizeof(conf));
	schema_Wifi_VIF_Config_mark_all_present(&conf);
	conf._partial_update = true;
	conf._partial_update = true;

        SCHEMA_SET_INT(conf.rrm, 1);
        SCHEMA_SET_INT(conf.ft_psk, 0);
        SCHEMA_SET_INT(conf.group_rekey, 0);
	strscpy(conf.mac_list_type, "none", sizeof(conf.mac_list_type));
	conf.mac_list_len = 0;
	SCHEMA_SET_STR(conf.if_name, blobmsg_get_string(tb[ADD_VIF_NAME]));
	SCHEMA_SET_STR(conf.ssid_broadcast, "enabled");
	SCHEMA_SET_STR(conf.mode, "ap");
	SCHEMA_SET_INT(conf.enabled, 1);
	SCHEMA_SET_INT(conf.btm, 1);
	SCHEMA_SET_INT(conf.uapsd_enable, true);
	SCHEMA_SET_STR(conf.bridge, blobmsg_get_string(tb[ADD_VIF_NETWORK]));
	SCHEMA_SET_INT(conf.vlan_id, blobmsg_get_u32(tb[ADD_VIF_VID]));
	SCHEMA_SET_STR(conf.ssid, blobmsg_get_string(tb[ADD_VIF_SSID]));
	STRSCPY(conf.security_keys[0], "encryption");
        STRSCPY(conf.security[0], "OPEN");
        conf.security_len = 1;

	phy_get_band(target_map_ifname(radio), band);
	if (strstr(band, "5"))
		SCHEMA_SET_STR(conf.min_hw_mode, "11ac");
	else
		SCHEMA_SET_STR(conf.min_hw_mode, "11n");

	radio_ops->op_vconf(&conf, radio);

	return UBUS_STATUS_OK;
}

static const struct ubus_method radio_ubus_methods[] = {
        UBUS_METHOD("dbg_add_vif", radio_ubus_add_vif_cb, add_vif_policy),
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

static struct ubus_instance ubus_instance = {
	.connect = radio_ubus_connect,
};

int radio_ubus_init(void)
{
	return ubus_init(&ubus_instance, wifihal_evloop);
}
