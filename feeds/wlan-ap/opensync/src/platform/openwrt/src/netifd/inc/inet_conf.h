/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef INET_CONF_H_INCLUDED
#define INET_CONF_H_INCLUDED

#include "netifd.h"
#include "inet_iface.h"

struct netifd_iface *netifd_add_inet_conf(struct schema_Wifi_Inet_Config *iconf);
void netifd_del_inet_conf(struct schema_Wifi_Inet_Config *old_rec);
struct netifd_iface *netifd_modify_inet_conf(struct schema_Wifi_Inet_Config *iconf);
bool netifd_inet_config_set(struct netifd_iface *piface, struct schema_Wifi_Inet_Config *iconf);
bool netifd_inet_config_apply(struct netifd_iface *piface);


/* DHCP Sniffing */
bool netifd_dhcp_table_update(struct schema_DHCP_leased_IP *dlip);
bool netifd_dhcp_lease_notify(void *self, bool released, struct osn_dhcp_server_lease *dl);

#endif /* INET_CONF_H_INCLUDED */
