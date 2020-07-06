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
		struct uci_ptr ptr;
		char buf[128];

		if (!tb[i])
			continue;

		if (uci_fill_ptr(&ptr, s, param->params[i].name) != UCI_OK)
			ptr.option = param->params[i].name;

		ptr.value = buf;
		switch (param->params[i].type) {
		case BLOBMSG_TYPE_STRING:
			ptr.value = blobmsg_get_string(tb[i]);
			break;
		case BLOBMSG_TYPE_BOOL:
			snprintf(buf, sizeof(buf), "%s",
				 blobmsg_get_bool(tb[i]) ? "1" : "0");
			break;
		case BLOBMSG_TYPE_INT32:
			snprintf(buf, sizeof(buf), "%d",  blobmsg_get_u32(tb[i]));
			break;
		case BLOBMSG_TYPE_INT64:
			snprintf(buf, sizeof(buf), "%lld",  blobmsg_get_u64(tb[i]));
			break;
		default:
			LOGE("unhandled uci type %d", param->params[i].type);
			continue;
		}

		if (uci_set(s->package->ctx, &ptr) != UCI_OK)
			LOGE("failed to apply uci option %s", param->params[i].name);
	}

	free(tb);
	return 0;
}


