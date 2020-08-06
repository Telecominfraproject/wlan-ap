/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdbool.h>

#include "target.h"
#include "schema.h"

#include "utils.h"

typedef void (*awlan_update_cb)(struct schema_AWLAN_Node *awlan, schema_filter_t *filter);

/* Update the redirector address inside the AWLAN_Node table */
bool target_device_config_register(void *awlan_cb)
{
	char buf[REDIRECTOR_ADDR_SIZE];
	struct schema_AWLAN_Node awlan;
	schema_filter_t filter = { 1, {"+", SCHEMA_COLUMN(AWLAN_Node, redirector_addr), NULL} };

	if (get_redirector_addr(buf) == -1) {
		/* The redirector address file seems to not exist. Simply return */
		LOG(WARNING, "Redirector address file seems to not exist inside certs folder");
		return true;
	} else {
		memset(&awlan, 0, sizeof(awlan));
		STRSCPY(awlan.redirector_addr, buf);
		awlan_update_cb cbk = (awlan_update_cb) awlan_cb;
		cbk(&awlan, &filter);
	}

	return true;
}
