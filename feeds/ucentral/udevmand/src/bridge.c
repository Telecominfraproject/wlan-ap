/*
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "udevmand.h"

#include <linux/if_bridge.h>

#define BR_MAX_ENTRY	2048

static struct uloop_timeout bridge_timer;
static struct vlist_tree bridge_mac;
static LIST_HEAD(bridge_if);

struct bridge_if {
	struct list_head list;

	char name[IF_NAMESIZE];
	unsigned int port_no;
};

static void
bridge_read_mac(const char *bridge)
{
	FILE *fd;
	int i, cnt;
	struct __fdb_entry fe[BR_MAX_ENTRY];
	char path[PATH_MAX];

	snprintf(path, PATH_MAX, "/sys/class/net/%s/brforward", bridge);
	fd = fopen(path, "r");
	if (!fd)
		return;

	cnt = fread(fe, sizeof(struct __fdb_entry), BR_MAX_ENTRY, fd);
	fclose(fd);

	for (i = 0; i < cnt; i++) {
		struct bridge_mac *b = malloc(sizeof(*b));
		struct bridge_if *brif;

		if (!b)
			continue;
		strncpy(b->bridge, bridge, IF_NAMESIZE);
		strncpy(b->ifname, bridge, IF_NAMESIZE);
		list_for_each_entry(brif, &bridge_if, list)
			if (fe[i].port_no == brif->port_no)
				strncpy(b->ifname, brif->name, IF_NAMESIZE);
		memcpy(b->addr, fe[i].mac_addr, ETH_ALEN);
		b->port_no = fe[i].port_no;
		vlist_add(&bridge_mac, &b->vlist, (void *) b);
	}
}

void
bridge_dump_if(const char *bridge)
{
	char path[PATH_MAX];
	void *c = NULL;
	glob_t gl;
	int i;

	snprintf(path, PATH_MAX, "/sys/class/net/%s/brif/*", bridge);
	if (glob(path, GLOB_MARK | GLOB_ONLYDIR | GLOB_NOSORT, NULL, &gl))
		return;

	for (i = 0; i < gl.gl_pathc; i++) {
		if (!c)
			c = blobmsg_open_array(&b, "bridge");

		blobmsg_add_string(&b, NULL, basename(gl.gl_pathv[i]));
	}
	if (c)
		blobmsg_close_array(&b, c);

	globfree(&gl);
}

static void
bridge_read_if(const char *bridge)
{
	struct bridge_if *brif;
	char path[PATH_MAX];
	glob_t gl;
	int i;

	snprintf(path, PATH_MAX, "/sys/class/net/%s/brif/*", bridge);
	if (glob(path, GLOB_MARK | GLOB_ONLYDIR | GLOB_NOSORT, NULL, &gl))
		return;

	for (i = 0; i < gl.gl_pathc; i++) {
		unsigned int port_no;
		FILE *fd;
		int ret;

		snprintf(path, PATH_MAX, "/sys/class/net/%s/brif/%s/port_no", bridge, basename(gl.gl_pathv[i]));
		fd = fopen(path, "r");
		if (!fd)
			continue;
		ret = fscanf(fd, "0x%x", &port_no);
		fclose(fd);
		if (ret != 1)
			continue;
		brif = malloc(sizeof(*brif));
		if (!brif)
			goto out;
		strcpy(brif->name, basename(gl.gl_pathv[i]));
		brif->port_no = port_no;
		list_add(&brif->list, &bridge_if);

	}
out:
	globfree(&gl);
}

static void bridge_tout(struct uloop_timeout *t)
{
	struct bridge_if *brif, *tmp;
	glob_t gl;

	int i;

	if (glob("/sys/class/net/*", GLOB_MARK | GLOB_ONLYDIR | GLOB_NOSORT, NULL, &gl))
		goto out;

	list_for_each_entry_safe(brif, tmp, &bridge_if, list) {
		list_del(&brif->list);
		free(brif);
	}

	for (i = 0; i < gl.gl_pathc; i++)
		bridge_read_if(basename(gl.gl_pathv[i]));
	vlist_update(&bridge_mac);
	for (i = 0; i < gl.gl_pathc; i++)
		bridge_read_mac(basename(gl.gl_pathv[i]));
	vlist_flush(&bridge_mac);
	globfree(&gl);

out:
	uloop_timeout_set(&bridge_timer, 1000);
}

static int bridge_cmp(const void *k1, const void *k2, void *ptr)
{
	const struct bridge_mac *b1 = (const struct bridge_mac *)k1;
	const struct bridge_mac *b2 = (const struct bridge_mac *)k2;

	return memcmp(b1->addr, b2->addr, ETH_ALEN);
}

static void bridge_update(struct vlist_tree *tree, struct vlist_node *node_new, struct vlist_node *node_old)
{
	struct bridge_mac *b1, *b2;

	b1 = container_of(node_old, struct bridge_mac, vlist);
	b2 = container_of(node_new, struct bridge_mac, vlist);

	if (!!b1 != !!b2) {
		struct bridge_mac *_b = b1 ? b1 : b2;

		ULOG_INFO("%s fdb %s:%d "MAC_FMT"\n", b1 ? "del" : "new", _b->ifname, _b->port_no, MAC_VAR(_b->addr));
	}

	if (b1) {
		list_del(&b1->mac);
		free(b1);
	}

	if (b2) {
		struct mac *mac = NULL;

		mac = mac_find(b2->addr);
		mac_update(mac, b2->bridge);
		list_add(&b2->mac, &mac->bridge_mac);
	}
}

void bridge_init(void)
{
	bridge_timer.cb = bridge_tout;
	uloop_timeout_set(&bridge_timer, 1000);
	vlist_init(&bridge_mac, bridge_cmp, bridge_update);
}

void bridge_flush(void)
{
	struct bridge_if *brif, *tmp;

	vlist_flush_all(&bridge_mac);
	list_for_each_entry_safe(brif, tmp, &bridge_if, list) {
		list_del(&brif->list);
		free(brif);
	}
}

void
bridge_mac_del(struct bridge_mac *b)
{
	list_del(&b->mac);
	vlist_delete(&bridge_mac, &b->vlist);
	free(b);
}


