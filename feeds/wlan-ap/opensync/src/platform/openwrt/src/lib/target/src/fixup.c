/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>

#include "log.h"
#include "const.h"
#include "target.h"

#include <libubox/avl-cmp.h>
#include <libubox/avl.h>
#include <libubox/vlist.h>
#include <net/if.h>

#include "fixup.h"


/*
 * VIF Fixup
 */

static struct avl_tree vif_fixup_tree = AVL_TREE_INIT(vif_fixup_tree, avl_strcmp, false, NULL);
static struct avl_tree radio_fixup_tree = AVL_TREE_INIT(radio_fixup_tree, avl_strcmp, false, NULL);

struct vif_fixup * vif_fixup_find(const char *ifname)
{
        struct vif_fixup *vif = avl_find_element(&vif_fixup_tree, ifname, vif, avl);
        if (vif)
                return vif;

	/* Not found, add */
        vif = malloc(sizeof(*vif));
        if (!vif)
                return NULL;

        memset(vif, 0, sizeof(*vif));
        strncpy(vif->name, ifname, IF_NAMESIZE);
        vif->avl.key = vif->name;
        avl_insert(&vif_fixup_tree, &vif->avl);
        return vif;
}

void vif_fixup_del(char *ifname)
{
        struct vif_fixup *vif = NULL;
        vif = avl_find_element(&vif_fixup_tree, ifname, vif, avl);
        if (vif) {
		avl_delete(&vif_fixup_tree, &vif->avl);
		free(vif);
	}
}

bool vif_fixup_captive_enabled(void)
{
        struct vif_fixup *vif_ptr = NULL;
        struct vif_fixup *vif = NULL;

	avl_for_each_element_safe(&vif_fixup_tree, vif, avl, vif_ptr) {
		if (vif->has_captive == true)
			return true;
	}
	return false;
}

bool vif_fixup_iface_captive_enabled(const char *ifname)
{
	struct vif_fixup * vif = NULL;

	vif = vif_fixup_find(ifname);

	if (vif)
		return vif->has_captive;
	else
		return false;
}

void vif_fixup_set_iface_captive(const char *ifname, bool en)
{
	struct vif_fixup * vif = NULL;

	vif = vif_fixup_find(ifname);

	if (vif)
		vif->has_captive = en;
}

struct radio_fixup * radio_fixup_find(const char *ifname)
{
        struct radio_fixup *radio = avl_find_element(&radio_fixup_tree, ifname, radio, avl);
        if (radio)
                return radio;

	/* Not found, add */
        radio = malloc(sizeof(*radio));
        if (!radio)
                return NULL;

        memset(radio, 0, sizeof(*radio));
        strncpy(radio->rname, ifname, IF_NAMESIZE);
        radio->avl.key = radio->rname;
        avl_insert(&radio_fixup_tree, &radio->avl);
        return radio;
}

void radio_fixup_del(char *ifname)
{
        struct radio_fixup *radio = NULL;
        radio = avl_find_element(&radio_fixup_tree, ifname, radio, avl);
        if (radio) {
		avl_delete(&radio_fixup_tree, &radio->avl);
		free(radio);
	}
}

void radio_fixup_set_hw_mode(const char *ifname, char *hw_mode)
{
	struct radio_fixup * radio = NULL;

	radio = radio_fixup_find(ifname);

	if (radio)
		strncpy(radio->hw_mode, hw_mode, strlen(hw_mode));
}

char * radio_fixup_get_hw_mode(const char *ifname)
{
	struct radio_fixup * radio = NULL;

	radio = radio_fixup_find(ifname);

	if (radio)
		return radio->hw_mode;
	else
		return NULL;
}

/* primary channel */
void radio_fixup_set_primary_chan(const char *ifname, int chan)
{
	struct radio_fixup * radio = NULL;

	radio = radio_fixup_find(ifname);

	if (radio)
		radio->chan = chan;
}

int radio_fixup_get_primary_chan(const char *ifname)
{
	struct radio_fixup * radio = NULL;

	radio = radio_fixup_find(ifname);

	if (radio)
		return radio->chan;
	else
		return -1;
}
