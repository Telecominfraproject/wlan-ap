/* SPDX-License-Identifier: BSD-3-Clause */

#include <libubox/blobmsg.h>

#include "target.h"

#include "netifd.h"
#include "utils.h"
#include "uci.h"

enum {
	DHCP_ATTR_INTERFACE,
	DHCP_ATTR_START,
	DHCP_ATTR_LIMIT,
	DHCP_ATTR_LEASETIME,
	DHCP_ATTR_DHCPV6,
	DHCP_ATTR_RA,
	DHCP_ATTR_IGNORE,
	DHCP_ATTR_AUTOGEN,
	__DHCP_ATTR_MAX,
};

static const struct blobmsg_policy dhcp_policy[__DHCP_ATTR_MAX] = {
	[DHCP_ATTR_INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
	[DHCP_ATTR_START] = { .name = "start", .type = BLOBMSG_TYPE_STRING },
	[DHCP_ATTR_LIMIT] = { .name = "limit", .type = BLOBMSG_TYPE_STRING },
	[DHCP_ATTR_LEASETIME] = { .name = "leasetime", .type = BLOBMSG_TYPE_STRING },
	[DHCP_ATTR_DHCPV6] = { .name = "dhcpv6", .type = BLOBMSG_TYPE_STRING },
	[DHCP_ATTR_RA] = { .name = "ra", .type = BLOBMSG_TYPE_STRING },
	[DHCP_ATTR_IGNORE] = { .name = "ignore", .type = BLOBMSG_TYPE_INT32 },
	[DHCP_ATTR_AUTOGEN] = { .name = "autogen", .type = BLOBMSG_TYPE_INT32 },
};

const struct uci_blob_param_list dhcp_param = {
	.n_params = __DHCP_ATTR_MAX,
	.params = dhcp_policy,
};

void dhcp_add(char *net, const char *lease_time, const char *start, const char *limit)
{
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "interface", net);
	if (lease_time || start || limit) {
		blobmsg_add_string(&b, "start", start ? start : "10");
		blobmsg_add_string(&b, "limit", limit ? limit : "200");
		blobmsg_add_string(&b, "leasetime", lease_time ? lease_time : "12h");
		blobmsg_add_string(&b, "dhcpv6", "server");
		blobmsg_add_string(&b, "ra", "server");
		blobmsg_add_u32(&b, "ignore", 0);
	} else {
		blobmsg_add_u32(&b, "ignore", 1);
	}
	blobmsg_add_u32(&b, "autogen", 1);

	blob_to_uci_section(uci, "dhcp", net, "dhcp", b.head, &dhcp_param, NULL);
}

void dhcp_del(char *net)
{
	uci_section_del(uci, "zone", "dhcp", net, "dhcp");
}

void dhcp_get_state(struct schema_Wifi_Inet_State *state)
{
	struct blob_attr *tb[__DHCP_ATTR_MAX] = { };

	if (uci_section_to_blob(uci, "dhcp", state->if_name, &b, &dhcp_param))
		return;

	blobmsg_parse(dhcp_policy, __DHCP_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	if (tb[DHCP_ATTR_IGNORE] && blobmsg_get_u32(tb[DHCP_ATTR_IGNORE]))
		return;

	if (tb[DHCP_ATTR_START]) {
		STRSCPY(state->dhcpd_keys[state->dhcpd_len], "start");
		STRSCPY(state->dhcpd[state->dhcpd_len++], blobmsg_get_string(tb[DHCP_ATTR_START]));
	}

	if (tb[DHCP_ATTR_LIMIT]) {
		STRSCPY(state->dhcpd_keys[state->dhcpd_len], "stop");
		STRSCPY(state->dhcpd[state->dhcpd_len++], blobmsg_get_string(tb[DHCP_ATTR_LIMIT]));
	}

	if (tb[DHCP_ATTR_LEASETIME]) {
		STRSCPY(state->dhcpd_keys[state->dhcpd_len], "lease_time");
		STRSCPY(state->dhcpd[state->dhcpd_len++], blobmsg_get_string(tb[DHCP_ATTR_LEASETIME]));
	}
}

void dhcp_get_config(struct schema_Wifi_Inet_Config *conf)
{
	struct blob_attr *tb[__DHCP_ATTR_MAX] = { };

	if (uci_section_to_blob(uci, "dhcp", conf->if_name, &b, &dhcp_param))
		return;

	blobmsg_parse(dhcp_policy, __DHCP_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	if (tb[DHCP_ATTR_IGNORE] && blobmsg_get_u32(tb[DHCP_ATTR_IGNORE]))
		return;

	if (tb[DHCP_ATTR_START]) {
		STRSCPY(conf->dhcpd_keys[conf->dhcpd_len], "start");
		STRSCPY(conf->dhcpd[conf->dhcpd_len++], blobmsg_get_string(tb[DHCP_ATTR_START]));
	}

	if (tb[DHCP_ATTR_LIMIT]) {
		STRSCPY(conf->dhcpd_keys[conf->dhcpd_len], "stop");
		STRSCPY(conf->dhcpd[conf->dhcpd_len++], blobmsg_get_string(tb[DHCP_ATTR_LIMIT]));
	}

	if (tb[DHCP_ATTR_LEASETIME]) {
		STRSCPY(conf->dhcpd_keys[conf->dhcpd_len], "lease_time");
		STRSCPY(conf->dhcpd[conf->dhcpd_len++], blobmsg_get_string(tb[DHCP_ATTR_LEASETIME]));
	}
}

void dhcp_lease(const char *method, struct blob_attr *msg)
{
	enum {
		LEASE_ATTR_MAC,
		LEASE_ATTR_IP,
		LEASE_ATTR_NAME,
		LEASE_ATTR_IFACE,
		__LEASE_ATTR_MAX,
	};

	static const struct blobmsg_policy lease_policy[__LEASE_ATTR_MAX] = {
		[LEASE_ATTR_MAC] = { .name = "mac", .type = BLOBMSG_TYPE_STRING },
		[LEASE_ATTR_IP] = { .name = "ip", .type = BLOBMSG_TYPE_STRING },
		[LEASE_ATTR_NAME] = { .name = "name", .type = BLOBMSG_TYPE_STRING },
		[LEASE_ATTR_IFACE] = { .name = "iface", .type = BLOBMSG_TYPE_STRING },
	};

	struct blob_attr *tb[__LEASE_ATTR_MAX] = { };
	blobmsg_parse(lease_policy, __LEASE_ATTR_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[LEASE_ATTR_MAC] || !tb[LEASE_ATTR_IP] || !tb[LEASE_ATTR_IFACE])
		return;

}
