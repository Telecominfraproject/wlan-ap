/*
 * Copyright (C) 2021 Jo-Philipp Wich <jo@mein.io>
 * Copyright (C) 2021 John Crispin <john@phrozen.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ucrun.h"

static struct blob_buf u;

ucrun_ctx_t *
ctx_to_ucrun(struct ubus_context *ctx)
{
	struct ubus_auto_conn *conn = container_of(ctx, struct ubus_auto_conn, ctx);
	ucrun_ctx_t *ucrun = container_of(conn, ucrun_ctx_t, ubus_auto_conn);

	return ucrun;
}

static uc_value_t *
uc_blob_to_json(uc_vm_t *vm, struct blob_attr *attr, bool table, const char **name);

static uc_value_t *
uc_blob_array_to_json(uc_vm_t *vm, struct blob_attr *attr, size_t len, bool table)
{
	uc_value_t *o = table ? ucv_object_new(vm) : ucv_array_new(vm);
	uc_value_t *v;
	struct blob_attr *pos;
	size_t rem = len;
	const char *name;

	if (!o)
		return NULL;

	__blob_for_each_attr(pos, attr, rem) {
		name = NULL;
		v = uc_blob_to_json(vm, pos, table, &name);

		if (table && name)
			ucv_object_add(o, name, v);
		else if (!table)
			ucv_array_push(o, v);
		else
			ucv_put(v);
	}

	return o;
}

static uc_value_t *
uc_blob_to_json(uc_vm_t *vm, struct blob_attr *attr, bool table, const char **name)
{
	void *data;
	int len;

	if (!blobmsg_check_attr(attr, false))
		return NULL;

	if (table && blobmsg_name(attr)[0])
		*name = blobmsg_name(attr);

	data = blobmsg_data(attr);
	len = blobmsg_data_len(attr);

	switch (blob_id(attr)) {
	case BLOBMSG_TYPE_BOOL:
		return ucv_boolean_new(*(uint8_t *)data);

	case BLOBMSG_TYPE_INT16:
		return ucv_int64_new((int16_t)be16_to_cpu(*(uint16_t *)data));

	case BLOBMSG_TYPE_INT32:
		return ucv_int64_new((int32_t)be32_to_cpu(*(uint32_t *)data));

	case BLOBMSG_TYPE_INT64:
		return ucv_int64_new((int64_t)be64_to_cpu(*(uint64_t *)data));

	case BLOBMSG_TYPE_DOUBLE:
		;
		union {
			double d;
			uint64_t u64;
		} v;

		v.u64 = be64_to_cpu(*(uint64_t *)data);

		return ucv_double_new(v.d);

	case BLOBMSG_TYPE_STRING:
		return ucv_string_new(data);

	case BLOBMSG_TYPE_ARRAY:
		return uc_blob_array_to_json(vm, data, len, false);

	case BLOBMSG_TYPE_TABLE:
		return uc_blob_array_to_json(vm, data, len, true);

	default:
		return NULL;
	}
}

static int
ubus_ucode_cb(struct ubus_context *ctx,
	      struct ubus_object *obj,
	      struct ubus_request_data *req,
	      const char *name,
	      struct blob_attr *msg)
{
	ucrun_ctx_t *ucrun = ctx_to_ucrun(ctx);

	/* try to find the method */
	uc_value_t *methods = ucv_object_get(ucrun->ubus, "methods", NULL);
	uc_value_t *method = NULL, *cb, *retval = NULL;

	ucv_object_foreach(methods, key, val) {
		if (strcmp(key, name))
			continue;
		method = val;
	}

	if (!method)
		return UBUS_STATUS_METHOD_NOT_FOUND;

	/* check if the callback is valid */
	cb = ucv_object_get(method, "cb", NULL);
	if (!ucv_is_callable(cb))
		return UBUS_STATUS_METHOD_NOT_FOUND;

	/* push the callback to the stack */
	ucv_get(cb);
	uc_vm_stack_push(&ucrun->vm, cb);
	if (msg)
		uc_vm_stack_push(&ucrun->vm,
				 uc_blob_array_to_json(&ucrun->vm, blob_data(msg), blob_len(msg), true));

	/* execute the callback */
	if (!uc_vm_call(&ucrun->vm, false, msg ? 1 : 0))
		retval = uc_vm_stack_pop(&ucrun->vm);

	if (ucv_type(retval) == UC_OBJECT) {
		json_object *o;

		blob_buf_init(&u, 0);
		o = ucv_to_json(retval);
		blobmsg_add_object(&u, o);
		json_object_put(o);

		/* check if we need to send a reply */
		if (blobmsg_len(u.head))
			ubus_send_reply(ctx, req, u.head);
	}

	ucv_put(retval);

	return UBUS_STATUS_OK;
}

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	ucrun_ctx_t *ucrun = ctx_to_ucrun(ctx);
	uc_value_t *connect, *retval = NULL;

	/* register the ubus object */
	ubus_add_object(ctx, &ucrun->ubus_object);

	/* check if the user code has a connect handler */
	connect = ucv_object_get(ucrun->ubus, "connect", NULL);
	if (!ucv_is_callable(connect))
		return;

	/* push the callback to the stack */
	ucv_get(connect);
	uc_vm_stack_push(&ucrun->vm, connect);

	/* execute the callback */
	if (!uc_vm_call(&ucrun->vm, false, 0))
		retval = uc_vm_stack_pop(&ucrun->vm);
	ucv_put(retval);
}

void
ubus_init(ucrun_ctx_t *ucrun)
{
	int n_methods, n = 0;

	/* validate that the ubus declaration is complete */
	uc_value_t *object = ucv_object_get(ucrun->ubus, "object", NULL);
	uc_value_t *methods = ucv_object_get(ucrun->ubus, "methods", NULL);

	ucv_get(ucrun->ubus);

	if (ucv_type(object) != UC_STRING || ucv_type(methods) != UC_OBJECT) {
		fprintf(stderr, "The ubus declaration is incomplete\n");
		return;
	}

	/* create our ubus methods */
	n_methods = ucv_object_length(methods);
	ucrun->ubus_method = calloc(n_methods, sizeof(struct ubus_method));

	ucv_object_foreach(methods, key, val) {
		if (!ucv_object_get(val, "cb", NULL))
			continue;

		ucrun->ubus_method[n].name = key;
		ucrun->ubus_method[n].handler = ubus_ucode_cb;
		n++;
	}

	/* setup the ubus object */
	ucrun->ubus_name = strdup(ucv_string_get(object));

	ucrun->ubus_object_type.name = ucrun->ubus_name;
	ucrun->ubus_object_type.methods = ucrun->ubus_method;
	ucrun->ubus_object_type.n_methods = n_methods;

	ucrun->ubus_object.name = ucrun->ubus_name;
	ucrun->ubus_object.type = &ucrun->ubus_object_type;
	ucrun->ubus_object.methods = ucrun->ubus_method;
	ucrun->ubus_object.n_methods = n_methods;

	/* try to connect to ubus */
	ucrun->ubus_auto_conn.cb = ubus_connect_handler;
	ubus_auto_connect(&ucrun->ubus_auto_conn);
}

void
ubus_deinit(ucrun_ctx_t *ucrun)
{
	if (!ucrun->ubus)
		return;

        /* disconnect from ubus and free the memory */
	ubus_auto_shutdown(&ucrun->ubus_auto_conn);

	blob_buf_free(&u);
	free(ucrun->ubus_name);
	free(ucrun->ubus_method);
}
