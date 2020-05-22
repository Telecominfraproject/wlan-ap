#ifndef TARGET_UCI_HELPER_H_INCLUDED
#define TARGET_UCI_HELPER_H_INCLUDED

#include "target.h"
#include "uci.h"

#define UCI_BUFFER_SIZE 80

bool uci_read(char *uci_path, char *uci_result, size_t len);
bool uci_write(char *uci_path, char *uci_value);

/*
 *  Functions to retrieve Radio parameters
 */
int wifi_getRadioNumberOfEntries( int *numberOfEntries );
int wifi_getRadioIfName(int radio_idx, char *radio_ifname, size_t radio_ifname_len);
int wifi_getRadioChannel(int radio_idx, int *channel);
int wifi_getRadioHwMode(int radio_idx, char* hwMode, size_t hwMode_len);
int wifi_getRadioEnable(int radio_idx, bool *enabled);

/*
 *  Functions to set Radio parameters
 */
bool wifi_setRadioChannel(int radioIndex, int channel, const char *ht_mode);
bool wifi_setRadioEnabled(int radioIndex, bool enabled);

/*
 *  Functions to retrieve SSID parameters
 */
int wifi_getSSIDNumberOfEntries( int *numberOfEntries);
int wifi_getVIFName(int ssid_index, char *ssid_ifname, size_t ssid_ifname_len);
int wifi_getSSIDName(int ssid_index, char *ssid_name, size_t ssid_name_len);
int wifi_getSSIDRadioIndex(int ssid_index, int *radio_index);
int wifi_getSSIDRadioIfName(int ssid_index, char *radio_ifname, size_t radio_ifname_len);
int wifi_getSSIDEnable(int ssid_index, bool *enabled);
int wifi_getApBridgeInfo(int ssid_index, char *bridge_info, char *tmp1, char *tmp2, size_t bridge_info_len);
int wifi_getApIsolationEnable(int ssid_index, bool *enabled);
int wifi_getApSsidAdvertisementEnable(int ssid_index, bool *enabled);
int wifi_getBaseBSSID(int ssid_index,char *buf, size_t buf_len);

/*
 *  Functions to set SSID parameters
 */
bool wifi_setSSIDName(const char* ssidIfName, char* ssidName);

/*
 * Functions to access OVSDB callbacks
 */
bool radio_rops_vstate(struct schema_Wifi_VIF_State *vstate);
bool radio_rops_vconfig( struct schema_Wifi_VIF_Config *vconf, const char *radio_ifname);

/*
 *  VIF functions
 */

bool vif_state_update(int ssidIndex);
bool vif_state_get(int ssidIndex, struct schema_Wifi_VIF_State *vstate);
bool vif_copy_to_config(int ssidIndex, struct schema_Wifi_VIF_State *vstate, struct schema_Wifi_VIF_Config *vconf);

#endif
