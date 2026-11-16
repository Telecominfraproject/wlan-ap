// SPDX-License-Identifier: GPL-2.0
#ifndef __RTL8373_EEE_H__
#define __RTL8373_EEE_H__

int rtl8373_get_mac_eee(struct dsa_switch *ds, int port, struct ethtool_eee *e);
int rtl8373_set_mac_eee(struct dsa_switch *ds, int port, struct ethtool_eee *e);
#endif