/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __VLAN_H__
#define __VLAN_H__

extern void vlan_add(char *vifname, int vid, int bridge);
extern void vlan_del(char *name);

#endif
