/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _FIXUP_H__
#define _FIXUP_H__

struct vif_fixup {
        struct avl_node avl;
        char name[IF_NAMESIZE];
        bool has_captive;
};

struct vif_fixup * vif_fixup_find(const char *name);
void vif_fixup_del(char *ifname);


bool vif_fixup_captive_enabled(void);
bool vif_fixup_iface_captive_enabled(const char *ifname);
void vif_fixup_set_iface_captive(const char *ifname, bool en);
#endif
