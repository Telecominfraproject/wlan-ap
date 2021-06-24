/* SPDX-License-Identifier: BSD-3-Clause */

#include <syslog.h>
#include "target.h"
#include <net/if.h>
#include <libubus.h>

#include "ubus.h"

#include "rrm.h"

static struct ubus_context *ubus;



enum {
	FREQ_BAND,
	CHANNEL,
	__RRM_REBALANCE_MAX,
};

static const struct blobmsg_policy rebalance_channel_policy[__RRM_REBALANCE_MAX] = {
	[FREQ_BAND] = { "freq_band", BLOBMSG_TYPE_STRING },
	[CHANNEL] = { "channel", BLOBMSG_TYPE_INT32 },
};

static int
rrm_rebalance_channel_cb(struct ubus_context *ctx, struct ubus_object *obj,
		      struct ubus_request_data *req, const char *method,
		      struct blob_attr *msg)
{
	struct blob_attr *tb[__RRM_REBALANCE_MAX] = { };
	int32_t channel;
	char *freq_band;

	if (!msg)
		return UBUS_STATUS_INVALID_ARGUMENT;

	blobmsg_parse(rebalance_channel_policy, __RRM_REBALANCE_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[FREQ_BAND] || !tb[CHANNEL])
		return UBUS_STATUS_INVALID_ARGUMENT;

	freq_band = blobmsg_get_string(tb[FREQ_BAND]);
	channel = blobmsg_get_u32(tb[CHANNEL]);

	rrm_rebalance_channel(freq_band, channel);
	return UBUS_STATUS_OK;
}

static const struct ubus_method rrm_ubus_methods[] = {
        UBUS_METHOD("rrm_rebalance_channel", rrm_rebalance_channel_cb, rebalance_channel_policy),
};

static struct ubus_object_type rrm_ubus_object_type =
        UBUS_OBJECT_TYPE("osync-rrm", rrm_ubus_methods);

static struct ubus_object rrm_ubus_object = {
        .name = "osync-rrm",
        .type = &rrm_ubus_object_type,
        .methods = rrm_ubus_methods,
        .n_methods = ARRAY_SIZE(rrm_ubus_methods),
};

static void rrm_ubus_connect(struct ubus_context *ctx)
{
	ubus = ctx;
	ubus_add_object(ubus, &rrm_ubus_object);
}

static void ubus_iwinfo_cb(struct ubus_request *req,
			    int type, struct blob_attr *msg)
{
	enum {
		IWINFO_ATTR_NOISE,
		__IWINFO_ATTR_MAX,
	};

	static const struct blobmsg_policy iwinfo_policy[__IWINFO_ATTR_MAX] = {
		[IWINFO_ATTR_NOISE] = { .name = "noise", .type = BLOBMSG_TYPE_INT32 },
	};

	struct blob_attr *tb[__IWINFO_ATTR_MAX] = { };
	uint32_t *nf = (uint32_t *)req->priv;

	blobmsg_parse(iwinfo_policy, __IWINFO_ATTR_MAX, tb, blob_data(msg), blob_len(msg));

	if (tb[IWINFO_ATTR_NOISE])
		*nf = blobmsg_get_u32(tb[IWINFO_ATTR_NOISE]);
}

int ubus_get_noise(const char *if_name, uint32_t *noise)
{
	uint32_t id;
	static struct blob_buf b;

	if (ubus_lookup_id(ubus, "iwinfo", &id))
		return -1;

	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "device", if_name);
	return ubus_invoke(ubus, id, "info", b.head, ubus_iwinfo_cb, noise, 1000);

}

int ubus_set_channel_switch(const char *if_name, uint32_t frequency, int channel_bandwidth, int sec_chan_offset)
{
	uint32_t id;
	static struct blob_buf b;
	char path[64];

	snprintf(path, sizeof(path), "hostapd.%s", if_name);

	if (ubus_lookup_id(ubus, path, &id))
		return -1;
	blob_buf_init(&b, 0);

	if (channel_bandwidth == 20 || channel_bandwidth == 40) {
		blobmsg_add_bool(&b, "ht", 1);
	} else if (channel_bandwidth == 80) {
		blobmsg_add_bool(&b, "vht", 1);
	}
	if (channel_bandwidth == 40 || channel_bandwidth == 80) {
		blobmsg_add_u32(&b, "center_freq1", frequency+30);
	}

	blobmsg_add_u32(&b, "freq", frequency);
	blobmsg_add_u32(&b, "bcn_count", 5);
	blobmsg_add_u32(&b, "bandwidth", channel_bandwidth);
	blobmsg_add_u32(&b, "sec_channel_offset", sec_chan_offset);
	return ubus_invoke(ubus, id, "switch_chan", b.head, NULL, NULL, 1000);
}

static struct ubus_instance ubus_instance = {
	.connect = rrm_ubus_connect,
};

int rrm_ubus_init(struct ev_loop *loop)
{
	return ubus_init(&ubus_instance, loop);
}
