/* SPDX-License-Identifier: BSD-3-Clause */

#include <syslog.h>
#include "target.h"
#include <net/if.h>
#include <libubus.h>

#include "ubus.h"

#include "rrm.h"

static struct ubus_context *ubus;

static void rrm_ubus_connect(struct ubus_context *ctx)
{
	ubus = ctx;
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

int ubus_set_channel_switch(const char *if_name, uint32_t frequency)
{
	uint32_t id;
	static struct blob_buf b;
	char path[64];

	snprintf(path, sizeof(path), "hostapd.%s", if_name);

	if (ubus_lookup_id(ubus, path, &id))
		return -1;

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "freq", frequency);
	blobmsg_add_u32(&b, "bcn_count", 1);
	return ubus_invoke(ubus, id, "switch_chan", b.head, NULL, NULL, 1000);
}

static struct ubus_instance ubus_instance = {
	.connect = rrm_ubus_connect,
};

int rrm_ubus_init(struct ev_loop *loop)
{
	return ubus_init(&ubus_instance, loop);
}
