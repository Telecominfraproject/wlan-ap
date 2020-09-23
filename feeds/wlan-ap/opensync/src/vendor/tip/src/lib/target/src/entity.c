/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <target.h>
#include "log.h"

#include <uci.h>
#include <uci_blob.h>

typedef void (*awlan_update_cb)(struct schema_AWLAN_Node *awlan, schema_filter_t *filter);
#define VERSION_MATRIX_FILE "/usr/opensync/.versions"

enum {
	SYSTEM_ATTR_MODEL,
	SYSTEM_ATTR_SERIAL,
	SYSTEM_ATTR_PLATFORM,
	SYSTEM_ATTR_FIRMWARE,
	SYSTEM_ATTR_REDIRECTOR,
	SYSTEM_ATTR_INACTIVEFW,
	__SYSTEM_ATTR_MAX,
};

static const struct blobmsg_policy system_policy[__SYSTEM_ATTR_MAX] = {
	[SYSTEM_ATTR_MODEL] = { .name = "model", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_SERIAL] = { .name = "serial", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_PLATFORM] = { .name = "platform", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_FIRMWARE] = { .name = "firmware", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_REDIRECTOR] = { .name = "redirector", .type = BLOBMSG_TYPE_STRING },
	[SYSTEM_ATTR_INACTIVEFW] = { .name = "inactivefw", .type = BLOBMSG_TYPE_STRING },
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

static bool update_version_matrix(struct schema_AWLAN_Node *awlan)
{
	FILE *fp = NULL;
	char *line = NULL;
	char *key, *val, *str = NULL;
	size_t len;
	int rd = 0;
	char inactive_fw[128] = "";

	fp = fopen(VERSION_MATRIX_FILE, "r");
	if (fp == NULL) {
		LOG(ERR, "Updating version matrix failed. versions file does not exist");
		return false;
	}
	while ((rd = getline(&line, &len, fp)) != -1) {
		if (rd -1 >= 0)
			line[rd - 1] = '\0';
		else continue;

		str = strdup(line);
		key = strtok(str, ":");
		val = strtok(NULL, ":");

		if (val == NULL) {
			SCHEMA_KEY_VAL_SET(awlan->version_matrix, key, "");
		} else {
			SCHEMA_KEY_VAL_SET(awlan->version_matrix, key, val);
		}
	}

	/* Update the inactive firmware version */
	copy_data(SYSTEM_ATTR_INACTIVEFW, inactive_fw, 128);
	SCHEMA_KEY_VAL_SET(awlan->version_matrix, "FW_IMAGE_INACTIVE", inactive_fw);

	if (line != NULL)
		free(line);
	if (str != NULL)
		free(str);
	fclose(fp);
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

bool target_device_config_register(void *awlan_cb)
{
	struct schema_AWLAN_Node awlan;
	schema_filter_t filter;
	bool update_matrix = true;
	bool update_redirector = true;

	memset(&awlan, 0, sizeof(awlan));

	update_redirector = copy_data(SYSTEM_ATTR_REDIRECTOR, awlan.redirector_addr, sizeof(awlan.redirector_addr));
	update_matrix = update_version_matrix(&awlan);

	if (update_matrix && update_redirector) {
		filter = (schema_filter_t) { 2, {"+", SCHEMA_COLUMN(AWLAN_Node, redirector_addr), SCHEMA_COLUMN(AWLAN_Node, version_matrix), NULL} };
	} else if (update_matrix) {
		filter = (schema_filter_t) { 1, {"+", SCHEMA_COLUMN(AWLAN_Node, version_matrix), NULL} };
	} else if (update_redirector) {
		filter = (schema_filter_t) { 1, {"+", SCHEMA_COLUMN(AWLAN_Node, redirector_addr), NULL} };
	} else {
		/* Nothing to update */
		return true;
	}
	awlan_update_cb cbk = (awlan_update_cb) awlan_cb;
	cbk(&awlan, &filter);

	return true;
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

