/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"
#include "evsched.h"

#include <uci.h>
#include <libubox/blobmsg.h>

#include "uci.h"
#include "utils.h"
#include "inet_iface.h"
#include "inet_conf.h"

enum {
	NET_ATTR_TYPE,
	NET_ATTR_PROTO,
	NET_ATTR_IPADDR,
	NET_ATTR_NETMASK,
	NET_ATTR_GATEWAY,
	NET_ATTR_DNS,
	NET_ATTR_MTU,
	NET_ATTR_DISABLED,
	NET_ATTR_IP6ASSIGN,
	NET_ATTR_IP4TABLE,
	NET_ATTR_IP6TABLE,
	NET_ATTR_IFNAME,
	NET_ATTR_VID,
	NET_ATTR_VLAN_FILTERING,
	NET_ATTR_IP6ADDR,
	NET_ATTR_GRE_REMOTE_ADDR,
	NET_ATTR_GRE6_REMOTE_ADDR,
	NET_ATTR_MESH_MCAST_FANOUT,
	NET_ATTR_MESH_HOP_PENALTY,
	NET_ATTR_MESH_MCAST_MODE,
	NET_ATTR_MESH_DIST_ARP_TABLE,
	NET_ATTR_MESH_BRIDGE_LOOP_AVOIDANCE,
	NET_ATTR_MESH_AP_ISOLATION,
	NET_ATTR_MESH_BONDING,
	NET_ATTR_MESH_ORIG_INTERVAL,
	NET_ATTR_MESH_GW_MODE,
	__NET_ATTR_MAX,
};

static const struct blobmsg_policy network_policy[__NET_ATTR_MAX] = {
	[NET_ATTR_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_PROTO] = { .name = "proto", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_IPADDR] = { .name = "ipaddr", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_NETMASK] = { .name = "netmask", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_GATEWAY] = { .name = "gateway", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_DNS] = { .name = "dns", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_MTU] = { .name = "mtu", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_DISABLED] = { .name = "disabled", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_IP6ASSIGN] = { .name = "ip6assign", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_IP4TABLE] = { .name = "ip4table", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_IP6TABLE] = { .name = "ip6table", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_IFNAME] = { .name = "ifname", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_VID] = { .name = "vid", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_VLAN_FILTERING] = { .name = "vlan_filtering", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_IP6ADDR] = { .name = "ip6addr", .type = BLOBMSG_TYPE_STRING },
	[NET_ATTR_GRE_REMOTE_ADDR] = {.name = "peeraddr", .type = BLOBMSG_TYPE_STRING},
	[NET_ATTR_GRE6_REMOTE_ADDR] = {.name = "peer6addr", .type = BLOBMSG_TYPE_STRING},
	[NET_ATTR_MESH_MCAST_FANOUT] = { .name = "multicast_fanout", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_MESH_HOP_PENALTY] = { .name = "hop_penalty", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_MESH_MCAST_MODE] = { .name = "multicast_mode", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_MESH_DIST_ARP_TABLE] = { .name = "distributed_arp_table", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_MESH_BRIDGE_LOOP_AVOIDANCE] = { .name = "bridge_loop_avoidance", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_MESH_AP_ISOLATION] = { .name = "ap_isolation", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_MESH_BONDING] = { .name = "bonding", .type = BLOBMSG_TYPE_BOOL },
	[NET_ATTR_MESH_ORIG_INTERVAL] = { .name = "orig_interval", .type = BLOBMSG_TYPE_INT32 },
	[NET_ATTR_MESH_GW_MODE] = { .name = "gw_mode", .type = BLOBMSG_TYPE_STRING },
};

const struct uci_blob_param_list network_param = {
	.n_params = __NET_ATTR_MAX,
	.params = network_policy,
};

int reload_config = 0;
ovsdb_table_t table_Wifi_Inet_Config;
struct blob_buf b = { };
struct blob_buf del = { };
struct uci_context *uci;

/* Mesh options table */
#define SCHEMA_MESH_OPT_SZ            32
#define SCHEMA_MESH_OPTS_MAX          2

const char mesh_options_table[SCHEMA_MESH_OPTS_MAX][SCHEMA_MESH_OPT_SZ] =
{
		SCHEMA_CONSTS_MESH_MCAST_FANOUT,
		SCHEMA_CONSTS_MESH_HOP_PENALTY,
};

static void wifi_inet_conf_load(struct uci_section *s)
{
	struct blob_attr *tb[__NET_ATTR_MAX] = { };
	struct schema_Wifi_Inet_Config conf;
	struct iface_info info;
	char *ifname = NULL;
	int rc = -1;

	if (!strcmp(s->e.name, "loopback"))
		return;

	blob_buf_init(&b, 0);

	uci_to_blob(&b, s, &network_param);
	blobmsg_parse(network_policy, __NET_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));
	memset(&conf, 0, sizeof(conf));

	SCHEMA_SET_STR(conf.if_name, s->e.name);
	if (tb[NET_ATTR_TYPE])
		SCHEMA_SET_STR(conf.if_type, blobmsg_get_string(tb[NET_ATTR_TYPE]));
	else if ((strstr(s->e.name,"wan_") != NULL) || (strstr(s->e.name,"lan_") != NULL))
		SCHEMA_SET_STR(conf.if_type, "vlan");
	else if (strstr(s->e.name,"wlan") != NULL)
		SCHEMA_SET_STR(conf.if_type, "vif");
	else
		SCHEMA_SET_STR(conf.if_type, "eth");

	if (!tb[NET_ATTR_DISABLED] || !blobmsg_get_u8(tb[NET_ATTR_DISABLED])) {
		conf.enabled = true;
		conf.network = true;
	}

	if (tb[NET_ATTR_PROTO]) {
		char *proto = blobmsg_get_string(tb[NET_ATTR_PROTO]);
		if (!strcmp(proto, "dhcp") || !strcmp(proto, "static") || !strcmp(proto, "none"))
			SCHEMA_SET_STR(conf.ip_assign_scheme, proto);
	}

	if (!strcmp(conf.ip_assign_scheme, "dhcpv6"))
		SCHEMA_SET_STR(conf.ip_assign_scheme, "dhcp");

	if (!strcmp(conf.ip_assign_scheme, "static")) {
		if (tb[NET_ATTR_IPADDR])
			SCHEMA_SET_STR(conf.inet_addr, blobmsg_get_string(tb[NET_ATTR_IPADDR]));
		if (tb[NET_ATTR_NETMASK])
			SCHEMA_SET_STR(conf.netmask, blobmsg_get_string(tb[NET_ATTR_NETMASK]));
		if (tb[NET_ATTR_GATEWAY])
			SCHEMA_SET_STR(conf.gateway, blobmsg_get_string(tb[NET_ATTR_GATEWAY]));
		if (tb[NET_ATTR_DNS]) {
			STRSCPY(conf.dns_keys[0], SCHEMA_CONSTS_INET_DNS_PRIMARY);
			STRSCPY(conf.dns[0], blobmsg_get_string(tb[NET_ATTR_DNS]));
			conf.dns_len = 1;
		}
	}

	if (tb[NET_ATTR_MTU])
		SCHEMA_SET_INT(conf.mtu, blobmsg_get_u32(tb[NET_ATTR_MTU]));

	if (tb[NET_ATTR_VID])
		SCHEMA_SET_INT(conf.vlan_id, blobmsg_get_u32(tb[NET_ATTR_VID]));

	if (tb[NET_ATTR_IFNAME])
		ifname = blobmsg_get_string(tb[NET_ATTR_IFNAME]);

	if (ifname) {
		if(!strncmp(ifname, "gre4", strlen("gre4")) || !strncmp(ifname, "gre6", strlen("gre6")))
			rc = l3_device_split_gre(ifname, &info);
		else if (*ifname == '@')
			SCHEMA_SET_STR(conf.parent_ifname, &ifname[1]);
		else
			rc = l3_device_split(ifname, &info);
	}

	if (!rc) {
		SCHEMA_SET_STR(conf.parent_ifname, info.name);
		if (info.vid)
			SCHEMA_SET_INT(conf.vlan_id, info.vid);
	}

	if (strcmp(conf.if_type, "gre"))
		dhcp_get_config(&conf);

	if (!strcmp(conf.if_type, "gre")) {
		if (tb[NET_ATTR_IPADDR]) {
			SCHEMA_SET_STR(conf.gre_local_inet_addr,
					blobmsg_get_string(tb[NET_ATTR_IPADDR]));
		} else if (tb[NET_ATTR_IP6ADDR]) {
			SCHEMA_SET_STR(conf.gre_local_inet_addr,
					blobmsg_get_string(tb[NET_ATTR_IP6ADDR]));
		}

		if (tb[NET_ATTR_GRE_REMOTE_ADDR]) {
			SCHEMA_SET_STR(conf.gre_remote_inet_addr,
					blobmsg_get_string(tb[NET_ATTR_GRE_REMOTE_ADDR]));
		} else if (tb[NET_ATTR_GRE6_REMOTE_ADDR]) {
			SCHEMA_SET_STR(conf.gre_remote_inet_addr,
					blobmsg_get_string(tb[NET_ATTR_GRE6_REMOTE_ADDR]));
		}
	}

	firewall_get_config(&conf);

	if (!ovsdb_table_upsert(&table_Wifi_Inet_Config, &conf, false))
		LOG(ERR, "inet_config: failed to insert");
}

static void mesh_opt_set(struct blob_buf *b,
                                      const struct schema_Wifi_Inet_Config *iconf)
{
	int i;
	const char *opt;
	const char *val;

	for (i = 0; i < SCHEMA_MESH_OPTS_MAX; i++) {
		opt = mesh_options_table[i];
		val = SCHEMA_KEY_VAL_NULL(iconf->mesh_options, opt);

		if (strcmp(opt, "multicast_fanout") == 0) {
			if (!val)
				blobmsg_add_u32(b, "multicast_fanout", 16);
			else
				blobmsg_add_u32(b, "multicast_fanout", atoi(val));
		}
		else if (strcmp(opt, "hop_penalty") == 0) {
			if (!val)
				blobmsg_add_u32(b, "hop_penalty", 30);
			else
				blobmsg_add_u32(b, "hop_penalty", atoi(val));
		}
	}
}

static int wifi_inet_conf_add(struct schema_Wifi_Inet_Config *iconf)
{
	const char *lease_time = SCHEMA_FIND_KEY(iconf->dhcpd, "lease_time");
	const char *dhcp_start = SCHEMA_FIND_KEY(iconf->dhcpd, "start");
	const char *dhcp_stop = SCHEMA_FIND_KEY(iconf->dhcpd, "stop");
	int len = strlen(iconf->if_name);
	int is_ipv6 = 0;

	if (len && iconf->if_name[len - 1] == '6')
		is_ipv6 = 1;

	if (!strcmp(iconf->if_type, "vif")) {
		/* No useful data in vif - all determined from Wifi_VIF_Config */
		return 0;
	}

	blob_buf_init(&b, 0);
	blob_buf_init(&del, 0);

	if (iconf->enabled_exists && !iconf->enabled)
		blobmsg_add_bool(&b, "disabled", 1);
	else
		blobmsg_add_bool(&del, "disabled", 1);

	if (!iconf->parent_ifname_exists && strcmp(iconf->if_type, "eth")
		&& strcmp(iconf->if_type, "gre") && strcmp(iconf->if_type, "mesh")) {
		blobmsg_add_string(&b, "type", iconf->if_type);
		blobmsg_add_bool(&b, "vlan_filtering", 1);
	} else if (!strcmp(iconf->if_type, "gre")) {
		blobmsg_add_string(&b, "type", iconf->if_type);
	} else
		blobmsg_add_bool(&del, "type", 1);

	if (iconf->parent_ifname_exists && iconf->vlan_id > 2) {
		char uci_ifname[256];
		if(!strncmp(iconf->parent_ifname, "gre", strlen("gre"))) {
			snprintf(uci_ifname, sizeof(uci_ifname), "gre4t-%s.%d", iconf->parent_ifname, iconf->vlan_id);
			blobmsg_add_string(&b, "ifname", uci_ifname);
			blobmsg_add_string(&b, "type", "bridge");
		}
		else {
			snprintf(uci_ifname, sizeof(uci_ifname), "br-%s.%d", iconf->parent_ifname, iconf->vlan_id);
			blobmsg_add_string(&b, "ifname", uci_ifname);
		}
	}

	if (iconf->ip_assign_scheme_exists) {
		if (is_ipv6 && !strcmp(iconf->ip_assign_scheme, "dhcp"))
			blobmsg_add_string(&b, "proto", "dhcpv6");
		else
			blobmsg_add_string(&b, "proto", iconf->ip_assign_scheme);
	}

	if (iconf->inet_addr_exists)
		blobmsg_add_string(&b, "ipaddr", iconf->inet_addr);

	if (iconf->netmask_exists)
		blobmsg_add_string(&b, "netmask", iconf->netmask);

	if (iconf->vlan_id_exists && strncmp(iconf->parent_ifname, "gre", strlen("gre"))) {
		blobmsg_add_u32(&b, "ip4table", iconf->vlan_id);
		blobmsg_add_u32(&b, "vid", iconf->vlan_id);
	} else {
		blobmsg_add_bool(&del, "ip4table", 1);
		blobmsg_add_bool(&del, "vid", 1);
	}

	if (!strcmp(iconf->if_type, "gre")) {
		if (!is_ipv6) {
			char ip_addr[IPV4_ADDR_STR_LEN];
			if (ubus_get_wan_ip(ip_addr) || !strlen(ip_addr)) {
				LOG(ERR, "netifd: Failed to get wan ip. GRE tunnel interface won't be created");
				return -1;
			}
			blobmsg_add_string(&b, "proto", "gretap");
			blobmsg_add_string(&b, "ipaddr", ip_addr);
			blobmsg_add_string(&b, "peeraddr", iconf->gre_remote_inet_addr);
			blobmsg_add_u32(&b, "mtu", 1500);
			blobmsg_add_bool(&b, "force_link", 1);
		} else {
			blobmsg_add_string(&b, "proto", "grev6tap");
			blobmsg_add_string(&b, "ip6addr", iconf->gre_local_inet_addr);
			blobmsg_add_string(&b, "peer6addr", iconf->gre_remote_inet_addr);
			blobmsg_add_u32(&b, "mtu", 1500);
			blobmsg_add_bool(&b, "force_link", 1);
		}
	}

	if (!strcmp(iconf->if_type, "mesh")) {
		mesh_opt_set(&b, iconf);
		blobmsg_add_string(&b, "proto", "batadv");
		/* Defaults */
		blobmsg_add_bool(&b, "multicast_mode", 1);
		blobmsg_add_bool(&b, "distributed_arp_table", 1);
		blobmsg_add_bool(&b, "bridge_loop_avoidance", 1);
		blobmsg_add_bool(&b, "ap_isolation", 1);
		blobmsg_add_bool(&b, "bonding", 1);
		blobmsg_add_u32(&b, "orig_interval", 5000);
		blobmsg_add_string(&b, "gw_mode", "off");
	}

	if (iconf->mtu_exists)
		blobmsg_add_u32(&b, "mtu", iconf->mtu);
	else
		blobmsg_add_bool(&del, "mtu", 1);
	blob_to_uci_section(uci, "network", iconf->if_name, "interface", b.head, &network_param, del.head);

	if (!iconf->parent_ifname_exists || iconf->vlan_id > 2) {
		dhcp_add(iconf->if_name, lease_time, dhcp_start, dhcp_stop);
		firewall_add_zone(iconf->if_name, iconf->NAT_exists && iconf->NAT);
	}

	if (!strcmp(iconf->if_type, "mesh")) {
		/* Add batman virtual switch interface to br-lan */
		uci_append_list(uci, "network", "lan", "ifname", iconf->if_name);
		/* TODO Transparent Layer 2. Remove DHCP server from br-lan */
	}

	uci_commit_all(uci);
	reload_config = 1;

	return 0;
}

static void wifi_inet_conf_del(struct schema_Wifi_Inet_Config *iconf)
{
	if (!strcmp(iconf->if_name, "wan") || !strcmp(iconf->if_name, "lan")) {
		blob_buf_init(&b, 0);
		blobmsg_add_string(&b, "proto", "none");
		blob_to_uci_section(uci, "network", iconf->if_name, "interface", b.head, &network_param, NULL);
		return;
	}

	dhcp_del(iconf->if_name);
	firewall_del_zone(iconf->if_name);

	uci_section_del(uci, "network", "network", iconf->if_name, "interface");
	uci_commit_all(uci);
	reload_config = 1;
}

static void callback_Wifi_Inet_Config(ovsdb_update_monitor_t *mon,
			       struct schema_Wifi_Inet_Config *old_rec,
			       struct schema_Wifi_Inet_Config *iconf)
{
	switch (mon->mon_type) {
	case OVSDB_UPDATE_NEW:
		wifi_inet_conf_add(iconf);
		netifd_add_inet_conf(iconf);
		break;
	case OVSDB_UPDATE_MODIFY:
		wifi_inet_conf_add(iconf);
		netifd_modify_inet_conf(iconf);
		break;
	case OVSDB_UPDATE_DEL:
		wifi_inet_conf_del(old_rec);
		netifd_del_inet_conf(old_rec);
		break;
	default:
		LOG(ERR, "Invalid Wifi_Inet_Config mon_type(%d)", mon->mon_type);
	}

	return;
}

static void periodic_task(void *arg)
{
	if (reload_config) {
		uci_commit_all(uci);
		system("reload_config");
		reload_config = 0;
	}

	evsched_task_reschedule_ms(EVSCHED_SEC(5));
}

void wifi_inet_config_init(void)
{
	struct uci_element *e = NULL;
	struct uci_package *network;

	LOGI("Initializing netifd / Wifi Inet Config");
	uci = uci_alloc_context();

	OVSDB_TABLE_INIT(Wifi_Inet_Config, if_name);

	uci_load(uci, "network", &network);
	uci_foreach_element(&network->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "interface"))
			wifi_inet_conf_load(s);
	}
	uci_unload(uci, network);
	OVSDB_TABLE_MONITOR(Wifi_Inet_Config, false);
	evsched_task(&periodic_task, NULL, EVSCHED_SEC(5));

	return;
}
