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

	blob_to_uci_section(uci, "firewall", name, "forwarding", b.head, &fwd_param, NULL);
}

static void vlan_del_firewall_forwarding(int vid)
{
	char name[64];

	snprintf(name, sizeof(name), "lan_%d_fw", vid);

	uci_section_del(uci, "zone", "firewall", name, "forwarding");
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

	blob_to_uci_section(uci, "network", name, "rule", b.head, &route_param, NULL);
}

static void vlan_del_routing_rule(int vid)
{
	char name[64];

	snprintf(name, sizeof(name), "lan_%d_route", vid);

	uci_section_del(uci, "zone", "network", name, "rule");
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
		vlan_del_routing_rule(vif->vid);
		vlan_del_firewall_forwarding(vif->vid);
	}

	if (avl_is_empty(&vif->parent->nat) && avl_is_empty(&vif->parent->bridge))
		vif->parent->bridge_created = 0;

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

	if (!vlan->bridge_created)
		vlan->bridge_created = 1;

	if (!bridge && !vlan->nat_created) {
		vlan->nat_created = 1;
		vlan_add_routing_rule(vid);
		vlan_add_firewall_forwarding(vid);
	}
}

void vlan_del(char *name)
{
	struct vlan_vif *vif = vif_vid_find(name);

	if (!vif)
		return;
	_vlan_del(vif, 1);
}
