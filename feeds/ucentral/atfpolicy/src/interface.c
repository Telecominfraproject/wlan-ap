// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Felix Fietkau <nbd@nbd.name>
 */
#include <string.h>
#include <stdlib.h>

#include <libubox/avl-cmp.h>

#include "atf.h"

static AVL_TREE(interfaces, avl_strcmp, false, NULL);

#ifndef container_of_safe
#define container_of_safe(ptr, type, member) \
	(ptr ? container_of(ptr, type, member) : NULL)
#endif

static int avl_macaddr_cmp(const void *k1, const void *k2, void *ptr)
{
	return memcmp(k1, k2, 6);
}

void atf_interface_sta_update(struct atf_interface *iface)
{
	struct atf_station *sta;

	avl_for_each_element(&iface->stations, sta, avl)
		sta->present = false;
}

struct atf_station *atf_interface_sta_get(struct atf_interface *iface, uint8_t *macaddr)
{
	struct atf_station *sta;

	sta = avl_find_element(&iface->stations, macaddr, sta, avl);
	if (sta)
		goto out;

	sta = calloc(1, sizeof(*sta));
	memcpy(sta->macaddr, macaddr, sizeof(sta->macaddr));
	sta->avl.key = sta->macaddr;
	sta->weight = -1;
	avl_insert(&iface->stations, &sta->avl);

out:
	sta->present = true;
	return sta;
}

void atf_interface_sta_flush(struct atf_interface *iface)
{
	struct atf_station *sta, *tmp;

	avl_for_each_element_safe(&iface->stations, sta, avl, tmp) {
		if (sta->present)
			continue;

		avl_delete(&iface->stations, &sta->avl);
		free(sta);
	}
}

void atf_interface_sta_changed(struct atf_interface *iface, struct atf_station *sta)
{
	int weight;

	if (sta->avg_prio > config.prio_percent_thresh)
		weight = config.weight_prio;
	else if (sta->avg_prio > config.bulk_percent_thresh)
		weight = config.weight_bulk;
	else
		weight = config.weight_normal;

	if (sta->weight == weight)
		return;

	sta->weight = weight;
	atf_ubus_set_sta_weight(iface, sta);
}

struct atf_interface *atf_interface_get(const char *ifname)
{
	struct atf_interface *iface;

	iface = avl_find_element(&interfaces, ifname, iface, avl);
	if (iface)
		return iface;

	if (strlen(ifname) + 1 > sizeof(iface->ifname))
		return NULL;

	iface = calloc(1, sizeof(*iface));
	strcpy(iface->ifname, ifname);
	iface->avl.key = iface->ifname;
	avl_init(&iface->stations, avl_macaddr_cmp, false, NULL);
	avl_insert(&interfaces, &iface->avl);

	return iface;
}

void atf_interface_update_all(void)
{
	struct atf_interface *iface, *tmp;

	avl_for_each_element_safe(&interfaces, iface, avl, tmp)
		atf_nl80211_interface_update(iface);
}
