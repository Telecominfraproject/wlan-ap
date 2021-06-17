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
        struct vif_fixup *vif;

        vif = avl_find_element(&vif_fixup_tree, ifname, vif, avl);
        if (vif) {
		avl_delete(&vif_fixup_tree, &vif->avl);
		free(vif);
	}
}

bool vif_fixup_captive_enabled(void)
{
        struct vif_fixup *vif, *vif_ptr;

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

	return vif->has_captive;
}

void vif_fixup_set_iface_captive(const char *ifname, bool en)
{
	struct vif_fixup * vif = NULL;

	vif = vif_fixup_find(ifname);

	vif->has_captive = en;
}
