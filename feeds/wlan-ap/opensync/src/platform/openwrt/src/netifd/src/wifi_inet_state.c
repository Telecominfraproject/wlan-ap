/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

enum {
	NET_ATTR_INTERFACE,
	NET_ATTR_UP,
	NET_ATTR_IPV4_ADDR,
	NET_ATTR_IPV6_ADDR,
	NET_ATTR_L3_DEVICE,
	NET_ATTR_ROUTE,
	NET_ATTR_PROTO,
	NET_ATTR_DNS,
	__NET_ATTR_MAX,
};

static const struct blobmsg_policy network_policy[__NET_ATTR_MAX] = {
	[NET_ATTR_INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_UP] = { .name = "up", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_IPV4_ADDR] = { .name = "ipv4-address", .type = BLOBMSG_TYPE_ARRAY },
	[NET_ATTR_IPV6_ADDR] = { .name = "ipv6-address", .type = BLOBMSG_TYPE_ARRAY },
	[NET_ATTR_L3_DEVICE] = { .name = "l3_device", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_ROUTE] = { .name = "route", .type = BLOBMSG_TYPE_ARRAY },
	[NET_ATTR_PROTO] = { .name = "proto", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_DNS] = { .name = "dns-server", .type = BLOBMSG_TYPE_ARRAY },
};

enum {
	IPV4_ATTR_ADDR,
	IPV4_ATTR_MASK,
	__IPV4_ATTR_MAX,
};

static const struct blobmsg_policy ipv4_addr_policy[__IPV4_ATTR_MAX] = {
	[IPV4_ATTR_ADDR] = { .name = "address", .type = BLOBMSG_TYPE_STRING },
	[IPV4_ATTR_MASK] = { .name = "mask", .type = BLOBMSG_TYPE_INT32 },
};

enum {
	IPV6_ATTR_ADDR,
	IPV6_ATTR_MASK,
	__IPV6_ATTR_MAX,
};

static const struct blobmsg_policy ipv6_addr_policy[__IPV6_ATTR_MAX] = {
	[IPV6_ATTR_ADDR] = { .name = "address", .type = BLOBMSG_TYPE_STRING },
	[IPV6_ATTR_MASK] = { .name = "mask", .type = BLOBMSG_TYPE_INT32 },
};

enum {
	ROUTE_ATTR_TARGET,
	ROUTE_ATTR_NEXTHOP,
	__ROUTE_ATTR_MAX,
};

static const struct blobmsg_policy route_policy[__ROUTE_ATTR_MAX] = {
	[ROUTE_ATTR_TARGET] = { .name = "target", .type = BLOBMSG_TYPE_STRING },
	[ROUTE_ATTR_NEXTHOP] = { .name = "nexthop", .type = BLOBMSG_TYPE_STRING },
};

static ovsdb_table_t table_Wifi_Inet_State;
static ovsdb_table_t table_Wifi_Master_State;

int l3_device_split(char *l3_device, struct iface_info *info)
{
	char *dot;

	if (strncmp(l3_device, "br-", 3))
		return -1;
	memset (info, 0, sizeof(*info));
	dot = strstr(l3_device, ".");
	if (!dot) {
		strcpy(info->name, &l3_device[3]);
	} else {
		strncpy(info->name, &l3_device[3], dot - l3_device - 3);
		info->vid = atoi(&dot[1]);
	}

	return 0;
}

void wifi_inet_state_set(struct blob_attr *msg)
{
	struct blob_attr *tb[__NET_ATTR_MAX] = { };
	struct schema_Wifi_Inet_State state;
	json_t *iter;
	int mtu;

	blobmsg_parse(network_policy, __NET_ATTR_MAX, tb, blob_data(msg), blob_len(msg));
	memset(&state, 0, sizeof(state));

	if (!tb[NET_ATTR_INTERFACE]) {
		LOG(ERR, "netifd: got bad event");
		return;
	}

	SCHEMA_SET_STR(state.if_name, blobmsg_get_string(tb[NET_ATTR_INTERFACE]));

	if (!strcmp(state.if_name, "loopback"))
		return;

	if (tb[NET_ATTR_UP] && blobmsg_get_bool(tb[NET_ATTR_UP])) {
		state.enabled = true;
		state.network = true;
	} else {
		state.enabled = false;
		state.network = false;
	}

	if (tb[NET_ATTR_L3_DEVICE]) {
		char *l3_device = blobmsg_get_string(tb[NET_ATTR_L3_DEVICE]);
		char mac[ETH_ALEN * 3];
		struct iface_info info;

		mtu = net_get_mtu(l3_device);
		if (mtu > 0)
			SCHEMA_SET_INT(state.mtu, mtu);
		if (!net_get_mac(l3_device, mac))
			SCHEMA_SET_STR(state.hwaddr, mac);
		if (!net_is_bridge(l3_device))
			SCHEMA_SET_STR(state.if_type, "bridge");
		else if (!strncmp(l3_device, "gre4", strlen("gre4")) ||
			!strncmp(l3_device, "gre6", strlen("gre6")))
			SCHEMA_SET_STR(state.if_type, "gre");
		else
			SCHEMA_SET_STR(state.if_type, "eth");
		if (!l3_device_split(l3_device, &info) && strcmp(info.name, state.if_name)) {
			SCHEMA_SET_STR(state.parent_ifname, info.name);
			if (info.vid)
				SCHEMA_SET_INT(state.vlan_id, info.vid);
		}
	} else {
		if (strstr(state.if_name, "wlan") != NULL)
                        SCHEMA_SET_STR(state.if_type, "vif");
		else
			SCHEMA_SET_STR(state.if_type, "eth");
	}

	if (tb[NET_ATTR_IPV4_ADDR] && blobmsg_data_len(tb[NET_ATTR_IPV4_ADDR])) {
		struct blob_attr *ipv4[__IPV4_ATTR_MAX] = { };
		struct blob_attr *first =  blobmsg_data(tb[NET_ATTR_IPV4_ADDR]);

		blobmsg_parse(ipv4_addr_policy, __IPV4_ATTR_MAX, ipv4, blobmsg_data(first), blobmsg_data_len(first));

		if (ipv4[IPV4_ATTR_ADDR] && ipv4[IPV4_ATTR_MASK]) {
			struct in_addr ip_addr = { .s_addr = 0xffffffff };

			SCHEMA_SET_STR(state.inet_addr, blobmsg_get_string(ipv4[IPV4_ATTR_ADDR]));

			ip_addr.s_addr >>= 32 - blobmsg_get_u32(ipv4[IPV4_ATTR_MASK]);
			SCHEMA_SET_STR(state.netmask, inet_ntoa(ip_addr));
		}
	}

	if (tb[NET_ATTR_IPV6_ADDR] && blobmsg_data_len(tb[NET_ATTR_IPV6_ADDR])) {
		struct blob_attr *ipv6[__IPV6_ATTR_MAX] = { };
		struct blob_attr *first =  blobmsg_data(tb[NET_ATTR_IPV6_ADDR]);

		blobmsg_parse(ipv6_addr_policy, __IPV6_ATTR_MAX, ipv6, blobmsg_data(first), blobmsg_data_len(first));

		if (ipv6[IPV6_ATTR_ADDR] && ipv6[IPV6_ATTR_MASK]) {
			char mask[8];

			snprintf(mask, sizeof(mask), "%d", blobmsg_get_u32(ipv6[IPV6_ATTR_MASK]));
			SCHEMA_SET_STR(state.inet_addr, blobmsg_get_string(ipv6[IPV6_ATTR_ADDR]));
			SCHEMA_SET_STR(state.netmask, mask);
		}
	}

	if (tb[NET_ATTR_ROUTE]) {
		struct blob_attr *cur = NULL;
		int rem = 0;

		blobmsg_for_each_attr(cur, tb[NET_ATTR_ROUTE], rem) {
			struct blob_attr *route[__ROUTE_ATTR_MAX] = { };

			blobmsg_parse(route_policy, __ROUTE_ATTR_MAX, route, blobmsg_data(cur), blobmsg_data_len(cur));

			if (!route[ROUTE_ATTR_TARGET] || !route[ROUTE_ATTR_NEXTHOP])
				continue;

			if (strcmp(blobmsg_get_string(route[ROUTE_ATTR_TARGET]), "0.0.0.0") &&
			    strcmp(blobmsg_get_string(route[ROUTE_ATTR_TARGET]), "::"))
				continue;

			SCHEMA_SET_STR(state.gateway, blobmsg_get_string(route[ROUTE_ATTR_NEXTHOP]));
			break;
		}
	}

	if (tb[NET_ATTR_DNS]) {
		struct blob_attr *cur = NULL;
		int count = 0;
		int rem = 0;

		blobmsg_for_each_attr(cur, tb[NET_ATTR_DNS], rem) {
			if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
				continue;
			STRSCPY(state.dns_keys[count], !count ? SCHEMA_CONSTS_INET_DNS_PRIMARY : SCHEMA_CONSTS_INET_DNS_SECONDARY);
			STRSCPY(state.dns[count], blobmsg_get_string(cur));
			count++;
			state.dns_len = count;

			if (++count > 1)
				break;
			break;
		}
	}

	if (tb[NET_ATTR_PROTO]) {
		char *proto = blobmsg_get_string(tb[NET_ATTR_PROTO]);
		if (!strcmp(proto, "dhcp") || !strcmp(proto, "static") || !strcmp(proto, "none"))
			SCHEMA_SET_STR(state.ip_assign_scheme, proto);
	}
	else
		SCHEMA_SET_STR(state.ip_assign_scheme, "none");

	if (!strcmp(state.ip_assign_scheme, "dhcpv6"))
		SCHEMA_SET_STR(state.ip_assign_scheme, "dhcp");

	dhcp_get_state(&state);
	firewall_get_state(&state);

	iter = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Inet_Config, if_name), state.if_name);
	if (iter) {
		struct schema_Wifi_Inet_Config config = {};

		if (ovsdb_table_select_one_where(&table_Wifi_Inet_Config, iter, &config)) {
			memcpy(&state.inet_config, &config._uuid, sizeof(config._uuid));
			state.inet_config_exists = true;
			state.inet_config_present = true;
			if (!strcmp(state.if_type, "gre")) {
				SCHEMA_SET_STR(state.gre_local_inet_addr,
						config.gre_local_inet_addr);
				SCHEMA_SET_STR(state.gre_remote_inet_addr,
						config.gre_remote_inet_addr);
				SCHEMA_SET_STR(state.gre_ifname,config.gre_ifname);
			}
		}
	}

	if (!ovsdb_table_upsert(&table_Wifi_Inet_State, &state, false))
		LOG(ERR, "inet_state: failed to insert");
}

void wifi_inet_master_set(struct blob_attr *msg)
{
	struct blob_attr *tb[__NET_ATTR_MAX] = { };
	struct schema_Wifi_Master_State state;
	json_t *iter;

	blobmsg_parse(network_policy, __NET_ATTR_MAX, tb, blob_data(msg), blob_len(msg));
	memset(&state, 0, sizeof(state));

	if (!tb[NET_ATTR_INTERFACE]) {
		LOG(ERR, "netifd: got bad event");
		return;
	}

	SCHEMA_SET_STR(state.if_name, blobmsg_get_string(tb[NET_ATTR_INTERFACE]));

	if (!strcmp(state.if_name, "loopback"))
		return;

	if (tb[NET_ATTR_UP] && blobmsg_get_bool(tb[NET_ATTR_UP])) {
		SCHEMA_SET_STR(state.port_state, "active");
		SCHEMA_SET_STR(state.network_state, "up");
	} else {
		SCHEMA_SET_STR(state.port_state, "inactive");
		SCHEMA_SET_STR(state.network_state, "down");
	}

	if (tb[NET_ATTR_IPV4_ADDR] && blobmsg_data_len(tb[NET_ATTR_IPV4_ADDR])) {
		struct blob_attr *ipv4[__IPV4_ATTR_MAX] = { };
		struct blob_attr *first =  blobmsg_data(tb[NET_ATTR_IPV4_ADDR]);

		blobmsg_parse(ipv4_addr_policy, __IPV4_ATTR_MAX, ipv4, blobmsg_data(first), blobmsg_data_len(first));

		if (ipv4[IPV4_ATTR_ADDR] && ipv4[IPV4_ATTR_MASK]) {
			struct in_addr ip_addr = { .s_addr = 0xffffffff };

			SCHEMA_SET_STR(state.inet_addr, blobmsg_get_string(ipv4[IPV4_ATTR_ADDR]));

			ip_addr.s_addr >>= 32 - blobmsg_get_u32(ipv4[IPV4_ATTR_MASK]);
			SCHEMA_SET_STR(state.netmask, inet_ntoa(ip_addr));
		}
	}

	if (tb[NET_ATTR_IPV6_ADDR] && blobmsg_data_len(tb[NET_ATTR_IPV6_ADDR])) {
		struct blob_attr *ipv6[__IPV6_ATTR_MAX] = { };
		struct blob_attr *first =  blobmsg_data(tb[NET_ATTR_IPV6_ADDR]);

		blobmsg_parse(ipv6_addr_policy, __IPV6_ATTR_MAX, ipv6, blobmsg_data(first), blobmsg_data_len(first));

		if (ipv6[IPV6_ATTR_ADDR] && ipv6[IPV6_ATTR_MASK]) {
			char mask[8];

			snprintf(mask, sizeof(mask), "%d", blobmsg_get_u32(ipv6[IPV6_ATTR_MASK]));
			SCHEMA_SET_STR(state.inet_addr, blobmsg_get_string(ipv6[IPV6_ATTR_ADDR]));
			SCHEMA_SET_STR(state.netmask, mask);
		}
	}

	if (tb[NET_ATTR_L3_DEVICE]) {
		char *l3_device = blobmsg_get_string(tb[NET_ATTR_L3_DEVICE]);

		if (!net_is_bridge(l3_device))
			SCHEMA_SET_STR(state.if_type, "bridge");
		else
			SCHEMA_SET_STR(state.if_type, "eth");
	} else
		SCHEMA_SET_STR(state.if_type, "eth");

	iter = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Inet_Config, if_name), state.if_name);
	if (iter) {
		struct schema_Wifi_Inet_Config config = {};

		if (ovsdb_table_select_one_where(&table_Wifi_Inet_Config, iter, &config)) {
			memcpy(&state.if_uuid, &config._uuid, sizeof(config._uuid));
			state._uuid_present = true;
			state._uuid_exists = true;
		}
	}

	if (!ovsdb_table_upsert(&table_Wifi_Master_State, &state, false))
		LOG(ERR, "master_state: failed to insert");
}

bool wifi_inet_state_del(const char *ifname)
{
	int ret;

	ret = ovsdb_table_delete_simple(&table_Wifi_Inet_State, SCHEMA_COLUMN(Wifi_Inet_State, if_name), ifname);
	if (ret <= 0)
		LOG(ERR, "inet_state: Error deleting Wifi_Inet_State for interface %s.", ifname);

	ret = ovsdb_table_delete_simple(&table_Wifi_Master_State, SCHEMA_COLUMN(Wifi_Inet_State, if_name), ifname);
	if (ret <= 0)
		LOG(ERR, "inet_state: Error deleting Wifi_Master_State for interface %s.", ifname);

	return ret;
}

void wifi_inet_state_init(void)
{
	OVSDB_TABLE_INIT(Wifi_Inet_State, if_name);
	OVSDB_TABLE_INIT(Wifi_Master_State, if_name);
}
