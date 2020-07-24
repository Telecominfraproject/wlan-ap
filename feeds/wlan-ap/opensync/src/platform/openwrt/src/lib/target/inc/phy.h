/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _NL_PHY_H__
#define _NL_PHY_H__

extern int phy_from_path(char *path, char *phy);
extern int phy_get_mac(char *phy, char *mac);
extern int phy_find_hwmon(char *path, char *hwmon);
extern int phy_get_tx_chainmask(const char *name);
extern int phy_get_channels(const char *name, int *channel);
extern int phy_get_band(const char *name, char *band);
extern int phy_is_ready(const char *name);
extern int phy_lookup(char *name);

#endif
