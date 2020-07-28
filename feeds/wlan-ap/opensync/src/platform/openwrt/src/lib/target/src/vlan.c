/* SPDX-License-Identifier: BSD-3-Clause */

#include "target.h"

#include <libubox/avl.h>
#include <libubox/avl-cmp.h>
#include "uci.h"
#include "radio.h"
#include "utils.h"

#define BRIDGE_NAME(x)	(x ? "wan" : "lan")
struct vlan {
	int vid;

	int nat_created;
	int bridge_created;

	struct avl_node avl;
	struct avl_tree nat;
	struct avl_tree bridge;
};

struct vlan_vif {
	char name[64];

	int vid;
	int bridge;
	struct vlan *parent;

	struct avl_node vlan;
	struct avl_node avl;
};

int avl_intcmp(const void *k1, const void *k2, void *ptr)
{
        int a = *((int *)k1);
        int b = *((int *)k2);

	return (a != b);
}

static struct avl_tree vlan_tree = AVL_TREE_INIT(vlan_tree, avl_intcmp, false, NULL);
static struct avl_tree vif_tree = AVL_TREE_INIT(vlan_tree, avl_strcmp, false, NULL);

struct vlan *vlan_find(int vid)
{
	struct vlan *vlan = avl_find_element(&vlan_tree, &vid, vlan, avl);

	return vlan;
}

struct vlan_vif *vif_vid_find(char *name)
{
	struct vlan_vif *vif = avl_find_element(&vif_tree, name, vif, avl);

	return vif;
}

struct vlan_vif *vlan_find_vif(struct vlan *vlan, char *name)
{
	struct vlan_vif *vif = avl_find_element(&vlan->nat, name, vif, vlan);

	if (!vif)
		vif = avl_find_element(&vlan->bridge, name, vif, vlan);

	return vif;
}

static void vlan_add_network(int bridge, int vid)
{
	enum {
		NET_ATTR_IFNAME,
		NET_ATTR_PROTO,
		NET_ATTR_IPADDR,
		NET_ATTR_NETMASK,
		NET_ATTR_IP6ASSIGN,
		NET_ATTR_IP4TABLE,
		NET_ATTR_IP6TABLE,
		NET_ATTR_AUTOGEN,
		__NET_ATTR_MAX,
	};

	static const struct blobmsg_policy network_policy[__NET_ATTR_MAX] = {
		[NET_ATTR_IFNAME] = { .name = "ifname", .type = BLOBMSG_TYPE_STRING },
		[NET_ATTR_PROTO] = { .name = "proto", .type = BLOBMSG_TYPE_STRING },
		[NET_ATTR_AUTOGEN] = { .name = "autogen", .type = BLOBMSG_TYPE_INT8 },
		[NET_ATTR_IPADDR] = { .name = "ipaddr", .type = BLOBMSG_TYPE_STRING },
		[NET_ATTR_NETMASK] = { .name = "netmask", .type = BLOBMSG_TYPE_STRING },
		[NET_ATTR_IP6ASSIGN] = { .name = "ip6assign", .type = BLOBMSG_TYPE_INT32 },
		[NET_ATTR_IP4TABLE] = { .name = "ip4table", .type = BLOBMSG_TYPE_INT32 },
		[NET_ATTR_IP6TABLE] = { .name = "ip6table", .type = BLOBMSG_TYPE_INT32 },
	};

	const struct uci_blob_param_list network_param = {
		.n_params = __NET_ATTR_MAX,
		.params = network_policy,
	};

	char name[64];
	char ifname[64];

	snprintf(name, sizeof(name), "%s_%d", BRIDGE_NAME(bridge), vid);
	snprintf(ifname, sizeof(ifname), "br-%s.%d", BRIDGE_NAME(bridge), vid);
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "ifname", ifname);
	blobmsg_add_bool(&b, "autogen", 1);
	if (bridge) {
		blobmsg_add_string(&b, "proto", "dhcp");
		blobmsg_add_u32(&b, "ip4table", vid);
	} else {
		char ip[16];

		snprintf(ip, sizeof(ip), "192.168.%d.1", (vid % 250) + 2);
		blobmsg_add_string(&b, "proto", "static");
		blobmsg_add_string(&b, "ipaddr", ip);
		blobmsg_add_string(&b, "netmask", "255.255.255.0");
		blobmsg_add_u32(&b, "ip6assign", 60);
	}
	blob_to_uci_section(uci, "network", name, "interface", b.head, &network_param);
}

static void vlan_del_network(int bridge, int vid)
{
	char name[64];

	snprintf(name, sizeof(name), "network.%s_%d", BRIDGE_NAME(bridge), vid);

	uci_section_del(uci, "network", name);
}

static void vlan_add_firewall_zone(int vid, int wan)
{
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

	char name[64];

	snprintf(name, sizeof(name), "%s_%d", BRIDGE_NAME(wan), vid);
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "name", name);
	blobmsg_add_string(&b, "network", name);
	blobmsg_add_string(&b, "input", wan ? "REJECT" : "ACCEPT");
	blobmsg_add_string(&b, "output", "ACCEPT");
	blobmsg_add_string(&b, "forward", wan ? "REJECT" : "ACCEPT");
	if (wan) {
		blobmsg_add_bool(&b, "masq", 1);
		blobmsg_add_bool(&b, "mtu_fix", 1);
	}
	blobmsg_add_bool(&b, "autogen", 1);

	blob_to_uci_section(uci, "firewall", name, "zone", b.head, &zone_param);
}

static void vlan_del_firewall_zone(int vid, int wan)
{
	char name[64];

	snprintf(name, sizeof(name), "firewall.%s_%d=zone", BRIDGE_NAME(wan), vid);

	uci_section_del(uci, "zone", name);
}

static void vlan_add_dhcp(int vid)
{
	enum {
		DHCP_ATTR_INTERFACE,
		DHCP_ATTR_START,
		DHCP_ATTR_LIMIT,
		DHCP_ATTR_LEASETIME,
		DHCP_ATTR_DHCPV6,
		DHCP_ATTR_RA,
		DHCP_ATTR_AUTOGEN,
		__DHCP_ATTR_MAX,
	};

	static const struct blobmsg_policy dhcp_policy[__DHCP_ATTR_MAX] = {
		[DHCP_ATTR_INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
		[DHCP_ATTR_START] = { .name = "start", .type = BLOBMSG_TYPE_INT32 },
		[DHCP_ATTR_LIMIT] = { .name = "limit", .type = BLOBMSG_TYPE_INT32 },
		[DHCP_ATTR_LEASETIME] = { .name = "leastime", .type = BLOBMSG_TYPE_STRING },
		[DHCP_ATTR_DHCPV6] = { .name = "dhcpv6", .type = BLOBMSG_TYPE_STRING },
		[DHCP_ATTR_RA] = { .name = "ra", .type = BLOBMSG_TYPE_STRING },
		[DHCP_ATTR_AUTOGEN] = { .name = "autogen", .type = BLOBMSG_TYPE_INT32 },
	};

	const struct uci_blob_param_list dhcp_param = {
		.n_params = __DHCP_ATTR_MAX,
		.params = dhcp_policy,
	};

	char name[64];
	char lan[64];

	snprintf(name, sizeof(name), "lan_%d_fw", vid);
	snprintf(lan, sizeof(lan), "lan_%d", vid);
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "interface", lan);
	blobmsg_add_u32(&b, "start", 10);
	blobmsg_add_u32(&b, "limit", 240);
	blobmsg_add_string(&b, "leastime", "12h");
	blobmsg_add_string(&b, "dhcpv6", "server");
	blobmsg_add_string(&b, "ra", "server");
	blobmsg_add_bool(&b, "autogen", 1);

	blob_to_uci_section(uci, "dhcp", name, "dhcp", b.head, &dhcp_param);
}

static void vlan_del_dhcp(int vid)
{
	char name[64];

	snprintf(name, sizeof(name), "dhcp.lan_%d_fw=dhcp", vid);

	uci_section_del(uci, "zone", name);
}

static void vlan_add_firewall_forwarding(int vid)
{
	enum {
		FWD_ATTR_SRC,
		FWD_ATTR_DEST,
		FWD_ATTR_AUTOGEN,
		__FWD_ATTR_MAX,
	};

	static const struct blobmsg_policy fwd_policy[__FWD_ATTR_MAX] = {
		[FWD_ATTR_SRC] = { .name = "src", .type = BLOBMSG_TYPE_STRING },
		[FWD_ATTR_DEST] = { .name = "dest", .type = BLOBMSG_TYPE_STRING },
		[FWD_ATTR_AUTOGEN] = { .name = "autogen", .type = BLOBMSG_TYPE_INT8 },
	};

	const struct uci_blob_param_list fwd_param = {
		.n_params = __FWD_ATTR_MAX,
		.params = fwd_policy,
	};

	char name[64];
	char lan[64];
	char wan[64];

	snprintf(name, sizeof(name), "lan_%d_fw", vid);
	snprintf(lan, sizeof(lan), "lan_%d", vid);
	snprintf(wan, sizeof(wan), "wan_%d", vid);
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "src", lan);
	blobmsg_add_string(&b, "dest", wan);
	blobmsg_add_bool(&b, "autogen", 1);

	blob_to_uci_section(uci, "firewall", name, "forwarding", b.head, &fwd_param);
}

static void vlan_del_firewall_forwarding(int vid)
{
	char name[64];

	snprintf(name, sizeof(name), "firewall.lan_%d_fw=forwarding", vid);

	uci_section_del(uci, "zone", name);
}

static void vlan_add_routing_rule(int vid)
{
	enum {
		ROUTE_ATTR_IN,
		ROUTE_ATTR_LOOKUP,
		__ROUTE_ATTR_MAX,
	};

	static const struct blobmsg_policy route_policy[__ROUTE_ATTR_MAX] = {
		[ROUTE_ATTR_IN] = { .name = "in", .type = BLOBMSG_TYPE_STRING },
		[ROUTE_ATTR_LOOKUP] = { .name = "lookup", .type = BLOBMSG_TYPE_INT32 },
	};

	const struct uci_blob_param_list route_param = {
		.n_params = __ROUTE_ATTR_MAX,
		.params = route_policy,
	};

	char name[64];
	char lan[64];

	snprintf(name, sizeof(name), "lan_%d_route", vid);
	snprintf(lan, sizeof(lan), "lan_%d", vid);
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "in", lan);
	blobmsg_add_u32(&b, "lookup", vid);

	blob_to_uci_section(uci, "network", name, "rule", b.head, &route_param);
}

static void vlan_del_routing_rule(int vid)
{
	char name[64];

	snprintf(name, sizeof(name), "network.lan_%d_route=rule", vid);

	uci_section_del(uci, "zone", name);
}

void _vlan_del(struct vlan_vif *vif, int del_parent)
{
	avl_delete(&vif_tree, &vif->avl);
	if (!vif->bridge)
		avl_delete(&vif->parent->bridge, &vif->vlan);
	else
		avl_delete(&vif->parent->nat, &vif->vlan);

	if (avl_is_empty(&vif->parent->nat)) {
		vif->parent->nat_created = 0;
		vlan_del_network(0, vif->vid);
		vlan_del_routing_rule(vif->vid);
		vlan_del_firewall_zone(vif->vid, 0);
		vlan_del_firewall_zone(vif->vid, 1);
		vlan_del_firewall_forwarding(vif->vid);
		vlan_del_dhcp(vif->vid);
	}
	if (avl_is_empty(&vif->parent->nat) && avl_is_empty(&vif->parent->bridge)) {
		vif->parent->bridge_created = 0;
		vlan_del_network(1, vif->vid);
	}

	if (del_parent && vif->parent->nat_created == 0 && vif->parent->bridge_created == 0) {
		avl_insert(&vlan_tree, &vif->parent->avl);
		free(vif->parent);
	}
	free(vif);
}

void vlan_add(char *vifname, int vid, int bridge)
{
	struct vlan *vlan = vlan_find(vid);
	struct vlan_vif *vif;

	if (!vlan) {
		vlan = malloc(sizeof(*vlan));
		if (!vlan) {
			LOG(ERR, "vlan: failed to add %s.%d", vifname, vid);
			return;
		}
		memset(vlan, 0, sizeof(*vlan));
		vlan->avl.key = &vlan->vid;
		vlan->vid = vid;
		avl_insert(&vlan_tree, &vlan->avl);
		avl_init(&vlan->nat, avl_strcmp, false, NULL);
		avl_init(&vlan->bridge, avl_strcmp, false, NULL);
	}

	vif = vlan_find_vif(vlan, vifname);

	if (vif && vif->vid != vid) {
		_vlan_del(vif, 0);
		vif = NULL;
	}

	if (vif)
		return;

	if (!vif) {
		vif = malloc(sizeof(*vif));
		if (!vif) {
			LOG(ERR, "vlan: failed to add vif %s", vifname);
			return;
		}
		memset(vif, 0, sizeof(*vif));
		strncpy(vif->name, vifname, sizeof(vif->name));
		vif->vlan.key = vif->name;
		vif->avl.key = vif->name;
		vif->vid = vid;
		vif->bridge = bridge;
		vif->parent = vlan;
		if (!bridge)
			avl_insert(&vlan->bridge, &vif->vlan);
		else
			avl_insert(&vlan->nat, &vif->vlan);
		avl_insert(&vif_tree, &vif->avl);
	}

	if (!vlan->bridge_created) {
		vlan->bridge_created = 1;
		vlan_add_network(1, vid);
	}

	if (!bridge && !vlan->nat_created) {
		vlan->nat_created = 1;
		vlan_add_network(0, vid);
		vlan_add_routing_rule(vid);
		vlan_add_firewall_zone(vid, 0);
		vlan_add_firewall_zone(vid, 1);
		vlan_add_firewall_forwarding(vid);
		vlan_add_dhcp(vid);
	}
}

void vlan_del(char *name)
{
	struct vlan_vif *vif = vif_vid_find(name);

	if (!vif)
		return;
	_vlan_del(vif, 1);
}
