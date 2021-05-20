/* SPDX-License-Identifier: BSD-3-Clause */
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <ev.h>
#include "ubus.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <apc.h>
#include <libubox/uloop.h>

struct ubus_context *ubus_ctx = NULL;
static struct blob_buf b;
static struct blob_buf nb;
timer *notify_timer;
extern struct apc_iface * apc_ifa;
/* Mandatorily Notify APC_State period */
#define APC_NOTIFY_INTERVAL 10
/* Check if any change in APC State and notify period */
#define APC_NOTIFY_CHECK 1
static ip_addr old_drip;
static ip_addr old_bdrip;
static u8 old_state;
static unsigned int ucount = 0;

struct apc_state {
	char mode[4];
	char dr_addr[17];
	char bdr_addr[17];
	bool enabled;
} state;

static int
apc_info_handle(struct ubus_context *ctx, struct ubus_object *obj,
			  struct ubus_request_data *req, const char *method,
			  struct blob_attr *msg);

static void ubus_reconnect_timer(struct uloop_timeout *timeout);
static struct uloop_timeout reconnect = {
	.cb = ubus_reconnect_timer,
};

static void ubus_reconnect_timer(struct uloop_timeout *timeout)
{
        if (ubus_reconnect(ubus_ctx, NULL) != 0) {
                printf("APC ubus failed to reconnect\n");
                uloop_timeout_set(&reconnect, 2000);
                return;
        }

        printf("APC ubus reconnected\n");
#ifdef FD_CLOEXEC
	fcntl(ubus_ctx->sock.fd, F_SETFD,
	      fcntl(ubus_ctx->sock.fd, F_GETFD) | FD_CLOEXEC);
#endif
}

static void ubus_connection_lost(struct ubus_context *ctx)
{
	printf("APC ubus connection lost\n");
        ubus_reconnect_timer(NULL);
}

static const struct blobmsg_policy apc_policy = {
	.name = "info",
	.type = BLOBMSG_TYPE_STRING,
};

static struct ubus_method apc_object_methods[] = {
	UBUS_METHOD_NOARG("info", apc_info_handle),
};

static struct ubus_object_type apc_object_type =
	UBUS_OBJECT_TYPE("apc", apc_object_methods);

static struct ubus_object apc_object = {
	.name = "apc",
	.type = &apc_object_type,
	.methods = apc_object_methods,
	.n_methods = ARRAY_SIZE(apc_object_methods),
};

static int
apc_info_handle(struct ubus_context *ctx, struct ubus_object *obj,
			  struct ubus_request_data *req, const char *method,
			  struct blob_attr *msg)
{
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "mode", state.mode);
	blobmsg_add_string(&b, "dr_addr", state.dr_addr);
	blobmsg_add_string(&b, "bdr_addr", state.bdr_addr);
	blobmsg_add_u8(&b, "enabled", state.enabled);

	ubus_notify(ctx, &apc_object, "apc", b.head, -1);
	ubus_send_event(ctx, "apc", b.head);
	ubus_send_reply(ctx, req, b.head);

	return 0;
}

static char apc_mode[APC_MAX_MODE][8] = {"DOWN", "LOOP", "WT", "PTP", "OR", "BDR", "DR"};
int apc_update_state(void)
{
	struct in_addr dr_addr;
	struct in_addr bdr_addr;
	ip_addr cur_drip;
	ip_addr cur_bdrip;
	u8 cur_state;

	cur_drip = apc_ifa->drip;
	cur_bdrip = apc_ifa->bdrip;
	cur_state = apc_ifa->state;
	ucount++;

	if (cur_drip == old_drip &&
	    cur_bdrip == old_bdrip &&
	    cur_state == old_state && ucount < APC_NOTIFY_INTERVAL) {
		return -1;
	}

	printf("APC State update %u", ucount);
	ucount = 0;

	dr_addr.s_addr = htonl(cur_drip);
	bdr_addr.s_addr = htonl(cur_bdrip);

	state.enabled = true;
	if ((cur_state == APC_IS_DR) ||
	    (cur_state == APC_IS_BACKUP) ||
	    (cur_state == APC_IS_DROTHER)) {
		snprintf(state.mode, sizeof(state.mode), "%s",
			 &apc_mode[cur_state][0]);
		snprintf(state.dr_addr, sizeof(state.dr_addr),
			 "%s", inet_ntoa(dr_addr));
		snprintf(state.bdr_addr, sizeof(state.bdr_addr),
			 "%s", inet_ntoa(bdr_addr));
	} else if (apc_ifa->state == APC_IS_WAITING) {
		snprintf(state.mode, sizeof(state.mode), "%s",
			 &apc_mode[cur_state][0]);
		snprintf(state.dr_addr, sizeof(state.dr_addr), "0.0.0.0");
		snprintf(state.bdr_addr, sizeof(state.bdr_addr), "0.0.0.0");
	} else {
		snprintf(state.mode, sizeof(state.mode), "NC");
		snprintf(state.dr_addr, sizeof(state.dr_addr), "0.0.0.0");
		snprintf(state.bdr_addr, sizeof(state.bdr_addr), "0.0.0.0");
	}

	old_drip = cur_drip;
	old_bdrip = cur_bdrip;
	old_state = cur_state;

	return 0;
}

void apc_send_notification(struct _timer * tmr)
{
	int ustate = 0;

	ustate = apc_update_state();
	if(ustate != 0)
		return;

	printf("APC send ubus notification\n");
	blob_buf_init(&nb, 0);
	blobmsg_add_string(&nb, "mode", state.mode);
	blobmsg_add_string(&nb, "dr_addr", state.dr_addr);
	blobmsg_add_string(&nb, "bdr_addr", state.bdr_addr);
	blobmsg_add_u8(&nb, "enabled", state.enabled);
	ubus_notify(ubus_ctx, &apc_object, "apc", nb.head, -1);
}

static void add_object(struct ubus_object *obj)
{
	int ret = ubus_add_object(ubus_ctx, obj);

	if (ret != 0)
		fprintf(stderr, "Add object fail '%s': %s\n",
			obj->name, ubus_strerror(ret));
}

int
ubus_init(void) {
	ubus_ctx = ubus_connect(NULL);
	if (!ubus_ctx)
		return -EIO;

	ubus_add_uloop(ubus_ctx);
#ifdef FD_CLOEXEC
	fcntl(ubus_ctx->sock.fd, F_SETFD,
	      fcntl(ubus_ctx->sock.fd, F_GETFD) | FD_CLOEXEC);
#endif
	add_object(&apc_object);
	notify_timer = tm_new_set(apc_send_notification, NULL,
				  0, APC_NOTIFY_CHECK);
	if (notify_timer) {
		printf("APC Start state check and notify timer\n");
		tm_start(notify_timer, APC_NOTIFY_CHECK);
	}

	ubus_ctx->connection_lost = ubus_connection_lost;

	return 0;
}

void
ubus_done(void)
{
	ubus_free(ubus_ctx);
}
