/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"
#include "inet_iface.h"
#include "inet_dhsnif.h"

static ds_key_cmp_t netifd_iface_cmp;

ds_tree_t netifd_iface_list = DS_TREE_INIT(netifd_iface_cmp, struct netifd_iface, if_tnode);

static inet_base_t *netifd_iface_new_inet(const char *ifname, const char *iftype);

/*
 * Find and return the netifd interface @p ifname.
 * If the interface is not found  NULL will
 * be returned.
 */
struct netifd_iface *netifd_iface_get_by_name(char *_ifname)
{
	char *ifname = _ifname;
	struct netifd_iface *piface;

	piface = ds_tree_find(&netifd_iface_list, ifname);
	if (piface != NULL)
		return piface;

	LOG(ERR, "netifd_iface_get_by_name: Couldn't find the interface(%s)", ifname);

	return NULL;
}

/*
 * Creates a new netifd interface.
 * Also initializes the inet interface.
 */
struct netifd_iface *netifd_iface_new(const char *ifname, const char *iftype)
{

	struct netifd_iface *piface;

	piface = calloc(1, sizeof(struct netifd_iface));

	if (strscpy(piface->if_name, ifname, sizeof(piface->if_name)) < 0) {
		LOG(ERR, "netifd_iface_new: %s: Error creating interface, name too long.", ifname);
		free(piface);
		return NULL;
	}

	piface->if_base = netifd_iface_new_inet(ifname, iftype);
	if (piface->if_base == NULL) {
		LOG(ERR, "netifd_iface_new: %s: Error creating inet interface.", ifname);
		free(piface);
		return NULL;
	}

	piface->if_inet = calloc(1, sizeof(inet_t));
	*(piface->if_inet) = piface->if_base->inet;

	ds_tree_insert(&netifd_iface_list, piface, piface->if_name);

	LOG(INFO, "netifd_iface_new: %s: Created new interface.", ifname);

	return piface;
}

/*
 * Delete interface and associated structures
 */
bool netifd_iface_del(struct netifd_iface *piface)
{

	bool retval = true;

	/* Stop DHCP sniffing service */
	if (!inet_dhsnif_stop(piface->if_base->in_dhsnif)) {
		LOG(ERR, "inet_base: %s: Error stopping the DHCP sniffing service.", piface->if_base->inet.in_ifname);
		retval = false;
	}
	/* Delete DHCP Sniffing Interface */
	if (piface->if_base->in_dhsnif != NULL && !inet_dhsnif_del(piface->if_base->in_dhsnif)) {
		LOG(WARN, "inet_base: %s: Error freeing DHCP sniffing object.", piface->if_base->inet.in_ifname);
		retval = false;
	}
	/* Free base and inet interfaces */
	if (piface->if_inet != NULL)
		free(piface->if_inet);
	if (piface->if_base != NULL)
		free(piface->if_base);
	/* Remove interface from global interface list */
	ds_tree_remove(&netifd_iface_list, piface);

	free(piface);
	return retval;
}

inet_base_t *netifd_iface_new_inet(const char *ifname, const char *iftype)
{

	inet_base_t *self;
	self = calloc(1, sizeof(*self));
	if (self == NULL) {
		goto error;
	}
	memset(self, 0, sizeof(inet_base_t));
	if((!strcmp(ifname, "wan") && !strcmp(iftype,"bridge")) || (!strcmp(ifname, "lan") && !strcmp(iftype,"bridge"))) {
		snprintf(self->inet.in_ifname, sizeof(self->inet.in_ifname), "br-%s", ifname);
	} else {
		STRSCPY(self->inet.in_ifname, ifname);
	}

	self->in_dhsnif = inet_dhsnif_new(self->inet.in_ifname);

	return (inet_base_t *)self;

error:
	if (self != NULL) free(self);
	return NULL;
}

/*
 * Interface comparator
 */
int netifd_iface_cmp(void *_a, void  *_b)
{
	struct netifd_iface *a = _a;
	struct netifd_iface *b = _b;

	return strcmp(a->if_name, b->if_name);
}
