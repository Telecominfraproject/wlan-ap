#ifndef TARGET_UCI_HELPER_H_INCLUDED
#define TARGET_UCI_HELPER_H_INCLUDED

#include "target.h"
#include "uci.h"
#include "schema_consts.h"

#define UCI_WRITE(type, section, index, option, value) ({ \
        if(!uci_write(type, section, index, option, value)) \
        { \
            return false; \
        } \
})

#define UCI_READ(type, section, index, option, result, length) ({ \
        if(uci_read(type, section, index, option, result, length) != UCI_OK) \
        {\
            return false; \
        } \
})

#define UCI_REMOVE(type, section, index, option) ({ \
        if(uci_remove(type, section, index, option) != UCI_OK) \
        { \
            return false; \
        } \
})

#define UCI_BUFFER_SIZE 80
#define DEFAULT_ENC_MODE        "TKIPandAESEncryption"
#define UCI_MAX_RADIOS 4

typedef enum {
  eFreqBand_24G = 0,
  eFreqBand_5G,
  eFreqBand_5GL,
  eFreqBand_5GU
} eFreqBand;

#define HWMODE_11a "11a"
#define HWMODE_11b "11b"
#define HWMODE_11g "11g"

#define HTMODE_noht   "NOHT"
#define HTMODE_ht20   "HT20"
#define HTMODE_ht40m  "HT40-"
#define HTMODE_ht40p  "HT40+"
#define HTMODE_ht40   "HT40"
#define HTMODE_vht20  "VHT20"
#define HTMODE_vht40  "VHT40"
#define HTMODE_vht80  "VHT80"
#define HTMODE_vht160 "VHT160"

#define OVSDB_SECURITY_KEY                  "key"
#define OVSDB_SECURITY_OFTAG                "oftag"
#define OVSDB_SECURITY_MODE                 "mode"
#define OVSDB_SECURITY_MODE_WEP64           "64"
#define OVSDB_SECURITY_MODE_WEP128          "128"
#define OVSDB_SECURITY_MODE_WPA1            "1"
#define OVSDB_SECURITY_MODE_WPA2            "2"
#define OVSDB_SECURITY_MODE_MIXED           "mixed"
#define OVSDB_SECURITY_ENCRYPTION           "encryption"
#define OVSDB_SECURITY_ENCRYPTION_OPEN      "OPEN"
#define OVSDB_SECURITY_ENCRYPTION_WEP       "WEP"
#define OVSDB_SECURITY_ENCRYPTION_WPA_PSK   "WPA-PSK"
#define OVSDB_SECURITY_ENCRYPTION_WPA_EAP   "WPA-EAP"
#define OVSDB_SECURITY_RADIUS_SERVER_IP     "radius_server_ip"
#define OVSDB_SECURITY_RADIUS_SERVER_PORT   "radius_server_port"
#define OVSDB_SECURITY_RADIUS_SERVER_SECRET "radius_server_secret"
/*
 *  Functions to retrieve Radio parameters
 */
int wifi_getRadioNumberOfEntries( int *numberOfEntries );
int wifi_getRadioIfName(int radio_idx, char *radio_ifname, size_t radio_ifname_len);
int wifi_getRadioChannel(int radio_idx, int *channel);
int wifi_getRadioEnable(int radio_idx, bool *enabled);
int wifi_getRadioTxPower(int radio_idx, int *txpower );
int wifi_getRadioBeaconInterval(int radio_idx, int *beacon_int);
int wifi_getRadioFreqBand(int *allowedChannels, int numberOfChannels, char *freq_band);
int wifi_getRadioHtMode(int radio_idx, char *ht_mode);
int wifi_getRadioHwMode(int radio_idx, char *hw_mode);
int wifi_getTxChainMask(int radioIndex, int *txChainMask);
int wifi_getRadioAllowedChannel(int radioIndex, int *allowedChannelList, int *allowedChannelListLen);
int wifi_getRadioMacaddress(int radio_idx, char *mac);

/*
 *  Functions to set Radio parameters
 */
bool wifi_setRadioChannel(int radioIndex, int channel, const char *ht_mode);
bool wifi_setRadioEnabled(int radioIndex, bool enabled);
bool wifi_setRadioTxPower(int radioIndex, int txpower);
bool wifi_setRadioBeaconInterval(int radioIndex, int beacon_int);
bool wifi_setRadioModes(int radioIndex, const char *freq_band, const char *ht_mode, const char *hw_mode);

/*
 *  Functions to retrieve SSID parameters
 */
int wifi_getSSIDNumberOfEntries( int *numberOfEntries);
int wifi_getVIFName(int ssid_index, char *ssid_ifname, size_t ssid_ifname_len);
int wifi_getSSIDName(int ssid_index, char *ssid_name, size_t ssid_name_len);
int wifi_getSSIDRadioIndex(int ssid_index, int *radio_index);
int wifi_getSSIDRadioIfName(int ssid_index, char *radio_ifname, size_t radio_ifname_len);
int wifi_getSsidEnabled(int ssid_index, bool *enabled);
int wifi_getApBridgeInfo(int ssid_index, char *bridge_info, char *tmp1, char *tmp2, size_t bridge_info_len);
int wifi_getApIsolationEnable(int ssid_index, bool *enabled);
int wifi_getApSsidAdvertisementEnable(int ssid_index, bool *enabled);
int wifi_getBaseBSSID(int ssid_index,char *buf, size_t buf_len,int radio_idx);
int wifi_getApSecurityKeyPassphrase(int ssid_index, char *buf, size_t buf_len);
bool wifi_getApSecurityModeEnabled(int ssid_index, char *buf, size_t buf_len);
bool wifi_getApSecurityRadiusServer(int ssid_index, char *radius_ip, char *radius_port, char *radius_secret);
bool wifi_setFtMode(int ssid_index, const struct schema_Wifi_VIF_Config *vconf);
bool wifi_getApVlanId(int ssidIndex, int *vlan_id);

/*
 *  Functions to set SSID parameters
 */
bool wifi_setSSIDName(int ssis_index, char* ssidName);
bool wifi_setApSecurityModeEnabled(int ssid_index, const struct schema_Wifi_VIF_Config *vconf);
bool wifi_setApSsidAdvertisementEnable(int ssid_index, bool enabled);
bool wifi_setApIsolationEnable(int ssid_index, bool enabled);
bool wifi_setSsidEnabled(int ssid_index, bool enabled);
bool wifi_setApBridgeInfo(int ssid_index, char *bridge_info);
bool wifi_setApVlanNetwork(int ssid_index, int vlan_id);

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
