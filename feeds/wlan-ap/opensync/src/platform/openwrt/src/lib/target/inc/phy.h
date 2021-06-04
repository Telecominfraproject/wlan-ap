/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _NL_PHY_H__
#define _NL_PHY_H__

extern int phy_from_path(char *path, char *phy);
extern int phy_get_mac(char *phy, char *mac);
extern int phy_find_hwmon(char *phy, char *hwmon, bool *DegreesNotMilliDegrees);
extern int phy_get_tx_chainmask(const char *name);
extern int phy_get_rx_chainmask(const char *name);
extern int phy_get_tx_available_antenna(const char *name);
extern int phy_get_rx_available_antenna(const char *name);
extern int phy_get_max_tx_power(const char *name , int channel);
extern int phy_get_channels(const char *name, int *channel);
extern int phy_get_dfs_channels(const char *name, int *channel);
extern int phy_get_channels_state(const char *name,
			struct schema_Wifi_Radio_State *rstate);
extern int phy_get_band(const char *name, char *band);
extern int phy_is_ready(const char *name);
extern int phy_lookup(char *name);
extern int get_current_channel(char *name);

#endif
