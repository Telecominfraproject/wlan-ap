/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"
#include "inet_iface.h"
#include "inet_dhsnif.h"

#include "inet_conf.h"

struct netifd_iface* netifd_add_inet_conf(struct schema_Wifi_Inet_Config *iconf)
{

	struct netifd_iface *piface = NULL;

	piface = netifd_iface_new(iconf->if_name, iconf->if_type);
	if (piface == NULL)
	{
		LOG(ERR, "netifd_add_inet_conf: %s: Unable to create interface.", iconf->if_name);
		return NULL;
	}

	return piface;
}

void netifd_del_inet_conf(struct schema_Wifi_Inet_Config *old_rec)
{
	struct netifd_iface *piface = NULL;

	piface = netifd_iface_get_by_name(old_rec->if_name);
	if (piface == NULL)
	{
		LOG(ERR, "netifd_del_inet_conf: Unable to delete non-existent interface %s.",
				old_rec->if_name);
	}

	if (piface != NULL && !netifd_iface_del(piface))
			{
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

/* Apply DHCP sniffing configuration from schema */
bool netifd_inet_dhsnif_set(struct netifd_iface *piface, const struct schema_Wifi_Inet_Config *iconf)
{
	/*
	 * Enable or disable the DHCP sniffing callback according to schema values
	 */
	if (iconf->dhcp_sniff_exists && iconf->dhcp_sniff) {
		LOG(INFO, "Enable dhcp sniffing callback on %s.",
				iconf->if_name);
		return inet_dhsnif_notify(piface->if_base->in_dhsnif,
				netifd_dhcp_lease_notify, piface->if_inet);
	}
	return inet_dhsnif_notify(piface->if_base->in_dhsnif, NULL, piface->if_inet);
}

bool netifd_inet_config_set(struct netifd_iface *piface, struct schema_Wifi_Inet_Config *iconf)
{
	bool retval = true;

	retval = netifd_inet_dhsnif_set(piface, iconf);

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
