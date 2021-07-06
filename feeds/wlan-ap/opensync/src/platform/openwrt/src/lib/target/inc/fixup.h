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

struct radio_fixup {
        struct avl_node avl;
        char rname[IF_NAMESIZE];
        char hw_mode[8];
};

struct radio_fixup * radio_fixup_find(const char *name);
void radio_fixup_del(char *ifname);
void radio_fixup_set_hw_mode(const char *ifname, char *hw_mode);
char *radio_fixup_get_hw_mode(const char *ifname);
#endif
