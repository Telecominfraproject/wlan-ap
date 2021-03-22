/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"
#include "inet_iface.h"
#include "inet_dhsnif.h"

#include "inet_conf.h"

static int vlan_count = 0;

void netifd_add_inet_conf(struct schema_Wifi_Inet_Config *iconf)
{
	struct netifd_iface *piface = NULL;

	if (strcmp(iconf->if_type, "bridge") && strcmp(iconf->if_type, "vlan")) {
		return;
	}

	piface = netifd_iface_get_by_name(iconf->if_name);

	if (piface)
		return;

	if (!strcmp(iconf->if_name, "wan") || !strcmp(iconf->if_name, "lan")) {
		piface = netifd_iface_new(iconf->if_name, iconf->if_type);
		if (!piface) {
			LOG(ERR, "netifd_add_inet_conf: %s: Unable to create interface.", iconf->if_name);
			return;
		}
		netifd_inet_config_set(piface);
		netifd_inet_config_apply(piface);
	} else if (iconf->vlan_id_exists && iconf->vlan_id > 2) {
		if (vlan_count < DHCP_SNIFF_MAX_VLAN && !strstr(iconf->if_name,"lan_")) {
			piface = netifd_iface_new(iconf->if_name, iconf->if_type);
			if (!piface) {
				LOG(ERR, "netifd_add_inet_conf: %s: Unable to create interface.", iconf->if_name);
				return;
			}
			netifd_inet_config_set(piface);
			netifd_inet_config_apply(piface);
			vlan_count++;
		}
	}

	return;
}

void netifd_del_inet_conf(struct schema_Wifi_Inet_Config *old_rec)
{
	struct netifd_iface *piface = NULL;

	piface = netifd_iface_get_by_name(old_rec->if_name);

	if (!piface)
		return;

	if (netifd_iface_del(piface)) {
		if (old_rec->vlan_id_exists && old_rec->vlan_id > 2)
			vlan_count--;
	} else {
		LOG(ERR, "netifd_del_inet_conf: Error during destruction of interface %s.",
				old_rec->if_name);
	}

	return;
}

struct netifd_iface* netifd_modify_inet_conf(struct schema_Wifi_Inet_Config *iconf)
{

	struct netifd_iface *piface = NULL;

	piface = netifd_iface_get_by_name(iconf->if_name);
	if (piface == NULL) {
		LOG(ERR, "Unable to modify interface %s, could't find it.",
				iconf->if_name);
	}

	return piface;
}

bool netifd_inet_dhsnif_set(struct netifd_iface *piface)
{
		LOG(INFO, "Enable dhcp sniffing callback on %s.", piface->if_base->inet.in_ifname);
		return inet_dhsnif_notify(piface->if_base->in_dhsnif,
				netifd_dhcp_lease_notify, piface->if_inet);

}

bool netifd_inet_config_set(struct netifd_iface *piface)
{
	bool retval = true;

	retval = netifd_inet_dhsnif_set(piface);

	return retval;
}

bool netifd_inet_config_apply(struct netifd_iface *piface)
{

	/* Start DHCP sniffing service */
	if (!inet_dhsnif_start(piface->if_base->in_dhsnif)) {
		LOG(ERR, "Error starting the DHCP sniffing service on %s.",
				piface->if_base->inet.in_ifname);
		return false;
	}

	return true;
}
