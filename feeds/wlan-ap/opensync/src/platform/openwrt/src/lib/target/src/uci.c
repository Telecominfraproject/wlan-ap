/* SPDX-License-Identifier: BSD-3-Clause */

#include <uci.h>
#include <uci_blob.h>
#include <libubox/avl-cmp.h>
#include <libubox/avl.h>
#include <inttypes.h>

#include "log.h"

void uci_commit_all(struct uci_context *uci)
{
	char **configs = NULL;
	char **p;

	if ((uci_list_configs(uci, &configs) != UCI_OK) || !configs) {
                LOG(ERR, "uci: failed to commit");
		return;
        }
	for (p = configs; *p; p++) {
		struct uci_ptr ptr = { };

		if (uci_lookup_ptr(uci, &ptr, *p, true) != UCI_OK)
			continue;
		uci_commit(uci, &ptr.p, false);
		uci_unload(uci, ptr.p);
	}
	free(configs);
}

static int uci_fill_ptr(struct uci_ptr *ptr, struct uci_section *s, const char *option)
{
        struct uci_package *p = s->package;

        memset(ptr, 0, sizeof(struct uci_ptr));

        ptr->package = p->e.name;
        ptr->p = p;

        ptr->section = s->e.name;
        ptr->s = s;

        ptr->option = option;
        return uci_lookup_ptr(p->ctx, ptr, NULL, false);
}

static char blob_to_string_buf[256];
static char *blob_to_string(struct blob_attr *a, uint32_t type)
{
	switch (type) {
	case BLOBMSG_TYPE_STRING:
		return blobmsg_get_string(a);
	case BLOBMSG_TYPE_BOOL:
		snprintf(blob_to_string_buf, sizeof(blob_to_string_buf), "%s",
			 blobmsg_get_bool(a) ? "1" : "0");
		break;
	case BLOBMSG_TYPE_INT32:
		snprintf(blob_to_string_buf, sizeof(blob_to_string_buf), "%d",  blobmsg_get_u32(a));
		break;
	case BLOBMSG_TYPE_INT64:
		snprintf(blob_to_string_buf, sizeof(blob_to_string_buf), "%"PRIu64,  blobmsg_get_u64(a));
		break;
	default:
		return NULL;
	}
	return blob_to_string_buf;
}

static int blob_to_uci(struct blob_attr *a, const struct uci_blob_param_list *param, struct uci_section *s)
{
	int sz = sizeof(struct blob_attr *) * param->n_params;
	struct blob_attr **tb = malloc(sz);
	int i;

	if (!tb) {
		LOGE("failed to allocate blob_to_uci table");
		return -1;
	}

	memset(tb, 0, sz);
	blobmsg_parse(param->params, param->n_params, tb, blob_data(a), blob_len(a));

	for (i = 0; i < param->n_params; i++) {
		struct blob_attr *cur;
		struct uci_ptr ptr;
		int rem = 0;

		if (!tb[i])
			continue;

		if (uci_fill_ptr(&ptr, s, param->params[i].name) != UCI_OK)
			ptr.option = param->params[i].name;

		switch (param->params[i].type) {
		case BLOBMSG_TYPE_STRING:
		case BLOBMSG_TYPE_BOOL:
		case BLOBMSG_TYPE_INT32:
		case BLOBMSG_TYPE_INT64:
			ptr.value = blob_to_string(tb[i], param->params[i].type);
			if (uci_set(s->package->ctx, &ptr) != UCI_OK)
				LOGE("failed to apply uci option %s", param->params[i].name);
			break;
		case BLOBMSG_TYPE_ARRAY:
			uci_delete(s->package->ctx, &ptr);
			blobmsg_for_each_attr(cur, tb[i], rem) {
				ptr.value = blob_to_string(cur, blobmsg_type(cur));
				if (uci_add_list(s->package->ctx, &ptr) != UCI_OK)
					LOGE("failed to apply uci option %s", param->params[i].name);
			}
			break;
		default:
			LOGE("unhandled uci type %d", param->params[i].type);
			continue;
		}
	}

	free(tb);
	return 0;
}

static int blob_to_uci_del(struct blob_attr *a, const struct uci_blob_param_list *param, struct uci_section *s)
{
	int sz = sizeof(struct blob_attr *) * param->n_params;
	struct blob_attr **tb = malloc(sz);
	int i;

	if (!tb) {
		LOGE("failed to allocate blob_to_uci table");
		return -1;
	}

	memset(tb, 0, sz);
	blobmsg_parse(param->params, param->n_params, tb, blob_data(a), blob_len(a));

	for (i = 0; i < param->n_params; i++) {
		struct uci_ptr ptr;

		if (!tb[i])
			continue;

		if (uci_fill_ptr(&ptr, s, param->params[i].name) != UCI_OK)
			ptr.option = param->params[i].name;

		uci_delete(s->package->ctx, &ptr);
	}

	free(tb);
	return 0;
}

int blob_to_uci_section(struct uci_context *uci,
			const char *package, const char *section,
			const char *type, struct blob_attr *a,
			const struct uci_blob_param_list *param,
			struct blob_attr *del)
{
	struct uci_ptr p = {};
	char tuple[128];
	int ret;

	snprintf(tuple, sizeof(tuple), "%s.%s=%s", package, section, type);
	ret = uci_lookup_ptr(uci, &p, tuple, true);
	if (ret != UCI_OK)
		goto err_out;
	ret = uci_set(uci, &p);
	if (ret != UCI_OK)
		goto err_out;
	if (del)
		blob_to_uci_del(del, param, p.s);
	blob_to_uci(a, param, p.s);

err_out:
	if (ret == UCI_OK)
		LOGT("%s: created %s", package, section);
	else
		LOG(ERR, "%s: failed to create %s", package, section);

	return UCI_OK;
}

int uci_section_del(struct uci_context *uci, char *prefix, char *package, char *section, char *type)
{
	struct uci_ptr p = {};
	char name[64];
	int ret;

	snprintf(name, sizeof(name), "%s.%s=%s", package, section, type);

	ret = uci_lookup_ptr(uci, &p, name, false);
	if (ret == UCI_OK)
		ret = uci_delete(uci, &p);

	if (ret == UCI_OK)
		LOGT("%s: deleted %s", prefix, name);
	else
		LOG(ERR, "%s: failed to delete %s", prefix, name);

	return ret;
}

int uci_section_to_blob(struct uci_context *uci, char *package, char *section,
			struct blob_buf *buf, const struct uci_blob_param_list *param)
{
        struct uci_package *p = NULL;
        struct uci_section *s = NULL;
	int ret = -1;

	if (uci_load(uci, package, &p))
		p = uci_lookup_package(uci, package);
	if (!p)
		return -1;
	s = uci_lookup_section(uci, p, section);
	if (!s)
		goto out;

	blob_buf_init(buf, 0);
	uci_to_blob(buf, s, param);
	ret = 0;

out:
	uci_unload(uci, p);
        return ret;
}
