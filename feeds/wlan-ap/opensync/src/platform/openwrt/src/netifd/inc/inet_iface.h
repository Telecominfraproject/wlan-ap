/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef INET_IFACE_H_INCLUDED
#define INET_IFACE_H_INCLUDED

#include "netifd.h"
#include "inet_base.h"
#include "inet.h"
#include "ds_tree.h"

struct netifd_iface
{
	char if_name[C_IFNAME_LEN]; /* Interface name */
	inet_base_t *if_base; /* Inet base structure */
	inet_t *if_inet; /* Inet structure */
	ds_tree_node_t if_tnode; /* ds_tree node -- for device lookup by name */

};

struct netifd_iface* netifd_iface_get_by_name(char *_ifname);
struct netifd_iface* netifd_iface_new(const char *ifname, const char *iftype);
bool netifd_iface_del(struct netifd_iface *piface);

#endif /* INET_IFACE_H_INCLUDED */
