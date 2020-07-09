#include <uci.h>
#include <uci_blob.h>

#include "log.h"

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
		snprintf(blob_to_string_buf, sizeof(blob_to_string_buf), "%lld",  blobmsg_get_u64(a));
		break;
	default:
		return NULL;
	}
	return blob_to_string_buf;
}

int blob_to_uci(struct blob_attr *a, const struct uci_blob_param_list *param, struct uci_section *s)
{
	int sz = sizeof(struct blob_attr) * param->n_params;
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


