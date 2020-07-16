#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <target.h>
#include "log.h"

#include <uci.h>
#include <uci_blob.h>

enum {
	SYSTEM_ATTR_MODEL,
	SYSTEM_ATTR_SERIAL,
	SYSTEM_ATTR_PLATFORM,
	SYSTEM_ATTR_FIRMWARE,
	__SYSTEM_ATTR_MAX,
};

static const struct blobmsg_policy system_policy[__SYSTEM_ATTR_MAX] = {
	[SYSTEM_ATTR_MODEL] = { .name = "model", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_SERIAL] = { .name = "serial", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_PLATFORM] = { .name = "platform", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_FIRMWARE] = { .name = "firmware", .type = BLOBMSG_TYPE_STRING },
};

const struct uci_blob_param_list system_param = {
        .n_params = __SYSTEM_ATTR_MAX,
        .params = system_policy,
};

static struct blob_attr *tb[__SYSTEM_ATTR_MAX] = { };

static bool copy_data(int id, void *buf, size_t len)
{
	if (!tb[id]) {
		strncpy(buf, "unknown", len);
		return false;
	}
	strncpy(buf, blobmsg_get_string(tb[id]), len);
	return true;
}

bool target_model_get(void *buf, size_t len)
{
	return copy_data(SYSTEM_ATTR_MODEL, buf, len);
}

bool target_serial_get(void *buf, size_t len)
{
	return copy_data(SYSTEM_ATTR_SERIAL, buf, len);
}

bool target_sw_version_get(void *buf, size_t len)
{
	return copy_data(SYSTEM_ATTR_FIRMWARE, buf, len);
}

bool target_platform_version_get(void *buf, size_t len)
{
	return copy_data(SYSTEM_ATTR_PLATFORM, buf, len);
}

static __attribute__((constructor)) void tip_data_init(void)
{
	struct uci_package *package;
	struct uci_section *section;
	struct uci_context *uci;
	struct blob_buf b = { };

	uci = uci_alloc_context();
	if (!uci)
		return;
	uci_load(uci, "system", &package);

	section = uci_lookup_section(uci, package, "tip");
	if (!section)
		return;

	blob_buf_init(&b, 0);
	uci_to_blob(&b, section, &system_param);
	blobmsg_parse(system_policy, __SYSTEM_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));
	uci_unload(uci, package);
	uci_free_context(uci);
}

