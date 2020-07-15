/* SPDX-License-Identifier: BSD-3-Clause */

#include <libubox/blobmsg.h>

#include "target.h"

#include "netifd.h"
#include "utils.h"
#include "uci.h"

enum {
	ZONE_ATTR_NAME,
	ZONE_ATTR_NETWORK,
	ZONE_ATTR_INPUT,
	ZONE_ATTR_OUTPUT,
	ZONE_ATTR_FORWARD,
	ZONE_ATTR_MASQ,
	ZONE_ATTR_MTU_FIX,
	ZONE_ATTR_AUTOGEN,
	__ZONE_ATTR_MAX,
};

static const struct blobmsg_policy zone_policy[__ZONE_ATTR_MAX] = {
	[ZONE_ATTR_NAME] = { .name = "name", .type = BLOBMSG_TYPE_STRING },
	[ZONE_ATTR_NETWORK] = { .name = "network", .type = BLOBMSG_TYPE_STRING },
	[ZONE_ATTR_INPUT] = { .name = "input", .type = BLOBMSG_TYPE_STRING },
	[ZONE_ATTR_OUTPUT] = { .name = "output", .type = BLOBMSG_TYPE_STRING },
	[ZONE_ATTR_FORWARD] = { .name = "forward", .type = BLOBMSG_TYPE_STRING },
	[ZONE_ATTR_MASQ] = { .name = "masq", .type = BLOBMSG_TYPE_INT8 },
	[ZONE_ATTR_MTU_FIX] = { .name = "mtu_fix", .type = BLOBMSG_TYPE_INT8 },
	[ZONE_ATTR_AUTOGEN] = { .name = "autogen", .type = BLOBMSG_TYPE_INT8 },
};

const struct uci_blob_param_list zone_param = {
	.n_params = __ZONE_ATTR_MAX,
	.params = zone_policy,
};

void firewall_add_zone(char *net, int nat)
{
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "name", net);
	if (strcmp(net, "wan"))
		blobmsg_add_string(&b, "network", net);
	blobmsg_add_string(&b, "input", nat ? "REJECT" : "ACCEPT");
	blobmsg_add_string(&b, "output", "ACCEPT");
	blobmsg_add_string(&b, "forward", nat ? "REJECT" : "ACCEPT");
	blobmsg_add_bool(&b, "masq", nat);
	blobmsg_add_bool(&b, "mtu_fix", nat);
	blobmsg_add_bool(&b, "autogen", 1);

	blob_to_uci_section(uci, "firewall", net, "zone", b.head, &zone_param, NULL);
}

void firewall_del_zone(char *net)
{
	uci_section_del(uci, "zone", "firewall", net, "zone");
}

void firewall_get_config(struct schema_Wifi_Inet_Config *conf)
{
	struct blob_attr *tb[__ZONE_ATTR_MAX] = { };

	if (uci_section_to_blob(uci, "firewall", conf->if_name, &b, &zone_param)) {
		SCHEMA_SET_INT(conf->NAT, 0);
		return;
	}

	blobmsg_parse(zone_policy, __ZONE_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	if (tb[ZONE_ATTR_MASQ] && blobmsg_get_bool(tb[ZONE_ATTR_MASQ]))
		SCHEMA_SET_INT(conf->NAT, 1);
	else
		SCHEMA_SET_INT(conf->NAT, 0);
}

void firewall_get_state(struct schema_Wifi_Inet_State *state)
{
	struct blob_attr *tb[__ZONE_ATTR_MAX] = { };

	if (uci_section_to_blob(uci, "firewall", state->if_name, &b, &zone_param)) {
		SCHEMA_SET_INT(state->NAT, 0);
		return;
	}

	blobmsg_parse(zone_policy, __ZONE_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	if (tb[ZONE_ATTR_MASQ] && blobmsg_get_bool(tb[ZONE_ATTR_MASQ]))
		SCHEMA_SET_INT(state->NAT, 1);
	else
		SCHEMA_SET_INT(state->NAT, 0);
}
