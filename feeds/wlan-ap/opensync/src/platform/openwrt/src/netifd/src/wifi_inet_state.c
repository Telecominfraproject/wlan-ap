/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <libubox/blobmsg_json.h>
#include "eth_vlan.h"

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

int l3_device_split_gre(char *l3_device, struct iface_info *info)
{
	char *delim;
	memset (info, 0, sizeof(*info));
	delim = strstr(l3_device, ".");
	if (!delim) {
		strcpy(info->name, &l3_device[6]);
	} else {
		strncpy(info->name, &l3_device[6], delim - l3_device - 6);
		info->vid = atoi(&delim[1]);
	}

	return 0;
}

int l3_device_split(char *l3_device, struct iface_info *info)
{
	char *delim;
	if (strncmp(l3_device, "br-", 3))
		return -1;
	memset (info, 0, sizeof(*info));
	if (!strncmp(l3_device, "br-gre", 6))
		delim = strstr(l3_device, "_");
	else
		delim = strstr(l3_device, ".");
	if (!delim) {
		strcpy(info->name, &l3_device[3]);
	} else {
		strncpy(info->name, &l3_device[3], delim - l3_device - 3);
		info->vid = atoi(&delim[1]);
	}

	return 0;
}

bool wifi_inet_state_del(const char *ifname);
bool wifi_inet_master_del(const char *ifname);
#define DOT_OR_DOTDOT(s) ((s)[0] == '.' && (!(s)[1] || ((s)[1] == '.' && !(s)[2])))
static struct blob_buf bb;
#define DEFAULT_BOARD_JSON		"/etc/board.json"
static struct blob_attr *board_info;

struct eth_port_state wanport;
struct eth_port_state lanport[MAX_ETH_PORTS];

static struct blob_attr* config_find_blobmsg_attr(struct blob_attr *attr, const char *name, int type)
{
	struct blobmsg_policy policy = { .name = name, .type = type };
	struct blob_attr *cur;

	blobmsg_parse(&policy, 1, &cur, blobmsg_data(attr), blobmsg_len(attr));

	return cur;
}


char* get_eth_map_info(char* iface)
{
	struct blob_attr *cur;

	blob_buf_init(&bb, 0);

	if (!blobmsg_add_json_from_file(&bb, DEFAULT_BOARD_JSON)) {
		return NULL;
	}
	if (board_info != NULL) {
		free(board_info);
		board_info = NULL;
	}
	cur = config_find_blobmsg_attr(bb.head, "network", BLOBMSG_TYPE_TABLE);
	if (!cur) {
		LOGD("Failed to find network in board.json file");
		return NULL;
	}
	board_info = blob_memdup(cur);
	if (!board_info)
		return NULL;

	cur = config_find_blobmsg_attr(board_info, iface, BLOBMSG_TYPE_TABLE);
	if (!cur) {
		LOGD("Failed to find %s in board.json file", iface);
		return NULL;
	}
	cur = config_find_blobmsg_attr(cur, "ifname", BLOBMSG_TYPE_STRING);
	if (!cur) {
		LOGD("Failed to find ifname in board.json file");
		return NULL;
	}
	return blobmsg_get_string(cur);
}



static void update_eth_state (char *eth, struct eth_port_state *eth_state)
{
	char sysfs_path[128];
	int fd = 0;
	struct dirent *ent;
	DIR *iface;
	int carrier = 1;
	ssize_t len = 0;

	memset(eth_state, 0, sizeof(struct eth_port_state));
	/* eth interface name */
	strncpy(eth_state->ifname, eth, sizeof(eth_state->ifname));

	/* eth interface state */
	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/class/net/%s/carrier", eth);

	fd = open(sysfs_path, O_RDONLY);
	if (fd < 0) {
		carrier = 0;
		close(fd);
	} else {
		len = read(fd, eth_state->state, 15);
		if (len < 0) {
			carrier = 0;
		}
		close(fd);
	}


	if(!strncmp(eth_state->state, "0", 1))
		carrier = 0;
	
	strncpy(eth_state->state, carrier? "up":"down", sizeof(eth_state->state));

	/* eth interface wan bridge */
	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/class/net/br-wan/brif");

	iface = opendir(sysfs_path);
	if (iface) {
		while ((ent = readdir(iface)) != NULL) {
			if (DOT_OR_DOTDOT(ent->d_name))
				continue;
			if (strncmp(ent->d_name, eth_state->ifname, sizeof(eth_state->ifname)) == 0)
				strncpy(eth_state->bridge, "br-wan", sizeof(eth_state->ifname));
		}
		closedir(iface);
	}

	/* eth interface lan bridge */
	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/class/net/br-lan/brif");

	iface = opendir(sysfs_path);
	if (iface) {
		while ((ent = readdir(iface)) != NULL) {
			if (DOT_OR_DOTDOT(ent->d_name))
				continue;
			if (strncmp(ent->d_name, eth_state->ifname, sizeof(eth_state->ifname)) == 0)
				strncpy(eth_state->bridge, "br-lan", sizeof(eth_state->bridge));
		}
		closedir(iface);
	}

	/* eth interface speed Mbits/sec */
	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/class/net/%s/speed", eth);

	fd = open(sysfs_path, O_RDONLY);
	if (fd < 0) {
		close(fd);
	} else {

		len = read(fd, eth_state->speed, sizeof(eth_state->speed) -1);

		if (len < 0)
			snprintf(eth_state->speed, sizeof(eth_state->speed), "0");
		else
			eth_state->speed[len-1] = '\0';
		close(fd);
	}

	/* eth interface duplex */
	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/class/net/%s/duplex", eth);

	fd = open(sysfs_path, O_RDONLY);
	if (fd < 0) {
		close(fd);
	} else {
		len = read(fd, eth_state->duplex, sizeof(eth_state->duplex) -1);

		if (len < 0)
			snprintf(eth_state->duplex, sizeof(eth_state->duplex), "none");
		else
			eth_state->duplex[len-1] = '\0';

		close(fd);
	}
}


static void update_eth_ports_states(struct schema_Wifi_Inet_State *state)
{
	char *wan = NULL;
	char *lan = NULL;
	char *eth = NULL;
	int cnt = 0;
	int i = 0;
	char port_status[128] = {'\0'};
	char brname[IFNAMSIZ] = {'\0'};

	wan = get_eth_map_info("wan");
	if (wan != NULL)
		update_eth_state(wan, &wanport);

	lan = get_eth_map_info("lan");
	if (lan != NULL) {
		eth = strtok (lan," ");
		for (i = 0; i < MAX_ETH_PORTS && eth != NULL; i++)
		{
			update_eth_state(eth, &lanport[i]);
			eth = strtok (NULL, " ");
		}
	}

	if (!strncmp(state->if_name, "wan", 3))
		strncpy(brname, "br-wan", 6);
	else if  (!strncmp(state->if_name, "lan", 3))
		strncpy(brname, "br-lan", 6);

	if (strcmp(wanport.bridge, brname) == 0) {
		STRSCPY(state->eth_ports_keys[0], wanport.ifname);
		snprintf(port_status, sizeof(port_status), "%s wan %sMbps %s", 
			 wanport.state, wanport.speed, wanport.duplex);
		STRSCPY(state->eth_ports[0], port_status);
		cnt++;
		state->eth_ports_len = cnt;
	}
	for (i = 0; i < MAX_ETH_PORTS && lanport[i].ifname != NULL; i++) {
		if (strcmp(lanport[i].bridge, brname) == 0) {
			STRSCPY(state->eth_ports_keys[state->eth_ports_len], lanport[i].ifname);
			memset(port_status, '\0', sizeof(port_status));
			snprintf(port_status, sizeof(port_status), "%s lan %sMbps %s", 
				 lanport[i].state, lanport[i].speed, lanport[i].duplex);

			STRSCPY(state->eth_ports[state->eth_ports_len], port_status);
			state->eth_ports_len++;
		}
	}

	if (!strncmp(state->if_name, "eth", 3)) {

		char *delim = NULL;
		char name[IFNAMSIZ] = {'\0'};

		delim = strstr(state->if_name, "_");
		if (delim)
			strncpy(name, state->if_name, delim - state->if_name);

		if (name[0] == '\0')
			return;


		if (strcmp(wanport.ifname, name) == 0) {
			STRSCPY(state->eth_ports_keys[0], wanport.ifname);
			memset(port_status, '\0', sizeof(port_status));
			snprintf(port_status, sizeof(port_status), "%s wan %sMbps %s", 
				 wanport.state, wanport.speed, wanport.duplex);
			STRSCPY(state->eth_ports[0], port_status);
			state->eth_ports_len = 1;
		} else {
			for (i = 0; i < MAX_ETH_PORTS && lanport[i].ifname != NULL; i++) {
				if (strcmp(lanport[i].ifname, name) == 0) {
					STRSCPY(state->eth_ports_keys[0], lanport[i].ifname);
					memset(port_status, '\0', sizeof(port_status));
					snprintf(port_status, sizeof(port_status), "%s lan %sMbps %s", 
					 lanport[i].state, lanport[i].speed, lanport[i].duplex);

					STRSCPY(state->eth_ports[0], port_status);
					state->eth_ports_len = 1;
				}
			}
		}
	}
}

static void set_vlan_trunk_state(struct schema_Wifi_Inet_State *state,
                                    int *index, const char *key,
                                    const char *value)
{
	STRSCPY(state->vlan_trunk_keys[*index], key);
	STRSCPY(state->vlan_trunk[*index], value);
	*index += 1;
	state->vlan_trunk_len = *index;
}

void vlan_state_update(struct schema_Wifi_Inet_State *state)
{
	int i, j = 0;
	int index = 0;
	const char *opt;
	char buf[256];

	for (i = 0; i < SCHEMA_VLAN_TRUNK_MAX; i++) {
		opt = vlan_trunk_table[i];

		if ((strcmp(opt, SCHEMA_CONSTS_ALLOWED_VLANS) == 0)) {
			struct eth_port_state *eps = get_eth_port(state->parent_ifname);
			char *s = buf;
			for (j=0; j < 64; j++){
				if (eps->vlans.allowed_vlans[j] != 0)
					s += snprintf(s, sizeof(buf) - 1, " %d ",
					eps->vlans.allowed_vlans[j]);
			}
			set_vlan_trunk_state(state, &index, opt, buf);
		} else if (strcmp(opt, SCHEMA_CONSTS_PVID) == 0) {
			struct eth_port_state *eps = get_eth_port(state->parent_ifname);
			snprintf(buf, sizeof(buf), "%d", eps->vlans.pvid);
			set_vlan_trunk_state(state, &index, opt, buf);
		}
	}
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

		/* Delete VLAN interface state column if disabled */
		if (strstr(state.if_name, "_") != NULL) {
			wifi_inet_state_del(state.if_name);
			return;
		}

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
		/* Fill if_type, vlan_id and parent_ifname using
		 * if_name (eg:eth0_100) */
		else if (!strncmp(l3_device, "eth", strlen("eth"))) {
				char *delim = NULL;
				delim = strstr(state.if_name, "_");
				if (delim) {
					struct iface_info info;
					memset (&info, 0, sizeof(info));
					if (!strncmp(&delim[1], "trunk", 5))
					{
						SCHEMA_SET_STR(state.if_type,
							       "vlan_trunk");
						strncpy(info.name, &l3_device[0],
							delim - state.if_name);
						SCHEMA_SET_STR(state.parent_ifname,
							       info.name);

						vlan_state_json_parse();
						vlan_state_update(&state);
					}
					else {
						struct eth_port_state *eps;
						SCHEMA_SET_STR(state.if_type,
							       "vlan");
						strncpy(info.name, &l3_device[0],
							delim - state.if_name);
						SCHEMA_SET_STR(state.parent_ifname,
							       info.name);
						vlan_state_json_parse();
						eps = get_eth_port(info.name);
						SCHEMA_SET_INT(state.vlan_id,
			       ((eps->vlans.pvid > 0 && eps->vlans.pvid < 4095)? eps->vlans.pvid:1));
					}
				}
			}
		else
			SCHEMA_SET_STR(state.if_type, "eth");
		if (!l3_device_split(l3_device, &info) &&
		    strcmp(info.name, state.if_name)) {
			SCHEMA_SET_STR(state.parent_ifname, info.name);
			if (info.vid)
				SCHEMA_SET_INT(state.vlan_id, info.vid);
		}
	} else {
		if (strstr(state.if_name, "wlan") != NULL)
			SCHEMA_SET_STR(state.if_type, "vif");
		else if ((strstr(state.if_name, "wan_") != NULL) || (strstr(state.if_name, "lan_") != NULL))
			SCHEMA_SET_STR(state.if_type, "vlan");
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

	update_eth_ports_states(&state);

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
		/* Delete VLAN interface state column if disabled */
		if (strstr(state.if_name, "_") != NULL) {
			wifi_inet_master_del(state.if_name);
			return;
		}

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
		/* Fill if_type, vlan_id and parent_ifname using
		 * if_name (eg:eth0_100) */
		else if (!strncmp(l3_device, "eth", strlen("eth"))) {
				char *delim = NULL;
				delim = strstr(state.if_name, "_");
				if (delim)
					SCHEMA_SET_STR(state.if_type, "vlan");
		} else
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


bool wifi_inet_master_del(const char *ifname)
{
	int ret;

	ret = ovsdb_table_delete_simple(&table_Wifi_Master_State, SCHEMA_COLUMN(Wifi_Inet_State, if_name), ifname);
	if (ret <= 0)
		LOG(ERR, "inet_state: Error deleting Wifi_Master_State for interface %s.", ifname);
	return ret;
}

bool wifi_inet_state_del(const char *ifname)
{
	int ret;

	ret = ovsdb_table_delete_simple(&table_Wifi_Inet_State, SCHEMA_COLUMN(Wifi_Inet_State, if_name), ifname);
	if (ret <= 0)
		LOG(ERR, "inet_state: Error deleting Wifi_Inet_State for interface %s.", ifname);
	return ret;
}

void wifi_inet_state_init(void)
{
	OVSDB_TABLE_INIT(Wifi_Inet_State, if_name);
	OVSDB_TABLE_INIT(Wifi_Master_State, if_name);
}
