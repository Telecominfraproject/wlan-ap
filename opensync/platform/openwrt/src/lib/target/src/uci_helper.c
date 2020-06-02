#include <string.h>
#include "log.h"
#include "uci_helper.h" 

static int g_nRadios = -1;
static int g_nVIFs = -1;

int uci_read(char* type, char* section, int section_index, char* option, char* result, size_t result_len)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    char   uci_cmd[80];
    int rc;

    if (!result)     return UCI_ERR_MEM;
    if (!result_len) return UCI_ERR_MEM;

    snprintf(uci_cmd,sizeof(uci_cmd),"%s.@%s[%d].%s", type, section, section_index, option);
    LOGD("UCI command read: %s", uci_cmd ); 

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_cmd, true)) != UCI_OK ||
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
        LOGN("UCI read %s.@%s[%d].%s failed: %d", type, section, section_index, option, rc); 
        uci_free_context(ctx);
        return UCI_ERR_NOTFOUND;
    }

    if (ptr.flags & UCI_LOOKUP_COMPLETE)
    {
        strncpy(result, ptr.o->v.string, result_len);
    } else {
        LOGN("UCI read %s.@%s[%d].%s not complete: %d", type, section, section_index, option, rc);
    }

    uci_free_context(ctx);
    return rc;
}

int uci_read_name(char* type, char* section, int section_index, char * option, char* result, size_t result_len)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    char   uci_cmd[80];
    int rc;

    if (!result)     return UCI_ERR_MEM;
    if (!result_len) return UCI_ERR_MEM;

    snprintf(uci_cmd,sizeof(uci_cmd),"%s.@%s[%d].%s", type, section, section_index, option);
    LOGD("UCI command read name %s", uci_cmd );

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_cmd, true)) != UCI_OK ||
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
        LOGN("UCI read name %s.@%s[%d].%s failed: %d", type, section, section_index, option, rc);
        uci_free_context(ctx);
        return UCI_ERR_NOTFOUND;
    }

    if (ptr.flags & UCI_LOOKUP_COMPLETE)
    {
        strncpy(result, ptr.s->e.name, result_len);
        LOGN("UCI section name: %s", result );
    } else {
        LOGN("UCI section name lookup not COMPLETE");
    }

    uci_free_context(ctx);
    return rc;
}

bool uci_write(char* type, char* section, int section_index, char * option, char *uci_value)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    char   uci_cmd[80];
    int rc;
 
    if (!uci_value)  return UCI_ERR_MEM;

    snprintf(uci_cmd,sizeof(uci_cmd),"%s.@%s[%d].%s", type, section, section_index, option);
    LOGN("UCI command write: %s value: %s", uci_cmd, uci_value );

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_cmd, true)) != UCI_OK ||
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
         /* Handle new option creation case */
         ptr.option = option;
    }

    ptr.value = uci_value;

    if ((rc = uci_set(ctx, &ptr)) != UCI_OK)
    {
        LOGN("UCI write %s.@%s[%d].%s error: %d", type, section, section_index, option, rc);
        uci_free_context(ctx);
        return false;
    }

    // TODO: Might want to put commit in its own function
    if ((rc = uci_commit(ctx, &ptr.p, false)) != UCI_OK)
    {
        LOGN("UCI write %s.@%s[%d].%s commit error: %d", type, section, section_index, option, rc);
        uci_free_context(ctx);
        return false;
    }

    uci_free_context(ctx);
    return true;
}

/* 
 *  WiFi UCI interface - definitions
 */

#define WIFI_TYPE "wireless"
#define WIFI_RADIO_SECTION "wifi-device"
#define WIFI_VIF_SECTION "wifi-iface"

/*
 *  WiFi Radio UCI interface
 */

int wifi_getRadioNumberOfEntries( int *numberOfEntries)
{
    int rnum=0;
    char buf[20];

    if (g_nRadios == -1) {
        while ((UCI_OK == uci_read_name(WIFI_TYPE, WIFI_RADIO_SECTION, rnum, "type", buf, sizeof(buf))) && (rnum < 8))
        {
            rnum++;
        }
        g_nRadios = rnum;
    }
    *numberOfEntries = g_nRadios;
    return UCI_OK;
} 

int wifi_getRadioIfName(int radio_idx, char *radio_ifname, size_t radio_ifname_len)
{
    bool rc;
    char if_name[128];
    memset(if_name, 0, sizeof(if_name));
    rc = uci_read_name(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "type", if_name, sizeof(if_name));
    if (rc == UCI_OK)
    {
        strncpy(radio_ifname, target_unmap_ifname(if_name), radio_ifname_len);
    }
    return rc;
}

int wifi_getRadioChannel(int radio_idx, int *channel)
{
    int rc;
    char buf[20];

    rc = uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "channel", buf, 20);
    if (rc == UCI_OK )
    {
        *channel = strtol(buf,NULL,10);
    }
    return rc;
}

int wifi_getRadioHwMode(int radio_idx, char* hwMode, size_t hwMode_len)
{
    return( uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "hwmode", hwMode, hwMode_len));
}

int wifi_getRadioEnable(int radio_idx, bool *enabled )
{
    int rc;
    char result[20];

    *enabled = true;
    rc = uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "disabled", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0))
    {
        *enabled = false;
    }
    return UCI_OK;
}

int wifi_getRadioTxPower(int radio_idx, int *txpower )
{
    int rc;
    char buf[20];

    rc = uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "txpower", buf, 20);
    if (rc == UCI_OK )
    {
        *txpower = strtol(buf,NULL,10);
    }
    return rc;
}

int wifi_getRadioBeaconInterval(int radio_idx, int *beacon_int)
{
    int rc;
    char buf[20];

    rc = uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "beacon_int", buf, 20);
    if (rc == UCI_OK )
    {
        *beacon_int = strtol(buf,NULL,10);
    }
    return rc;
}

bool wifi_setRadioChannel(int radioIndex, int channel, const char *ht_mode)
{
    char    str[4];

    sprintf(str, "%d", channel);
    return uci_write(WIFI_TYPE, WIFI_RADIO_SECTION, radioIndex, "channel", str);
}

bool wifi_setRadioEnabled(int radioIndex, bool enabled)
{
    char    disabled[4];

    if (enabled) {
        sprintf(disabled, "%d", 0);
    } else {
        sprintf(disabled, "%d", 1);
    }

    return uci_write(WIFI_TYPE, WIFI_RADIO_SECTION, radioIndex, "disabled", disabled);
}

bool wifi_setRadioTxPower(int radioIndex, int txpower )
{
    char    str[4];

    sprintf(str, "%d", txpower);
    return uci_write(WIFI_TYPE, WIFI_RADIO_SECTION, radioIndex, "txpower", str);
}

bool wifi_setRadioBeaconInterval(int radioIndex, int beacon_int)
{
    char    str[4];

    sprintf(str, "%d", beacon_int);
    return uci_write(WIFI_TYPE, WIFI_RADIO_SECTION, radioIndex, "beacon_int", str);
}

/*
 * SSID UCI interfaces
 */

int wifi_getSSIDNumberOfEntries( int *numberOfEntries)
{
    int rnum=0;
    char buf[20];
    int nVifMax = 24;

    if (g_nRadios > 0)	nVifMax = g_nRadios * 8;

    if (g_nVIFs == -1) {
        if (g_nRadios > 0)  nVifMax = g_nRadios * 8;

        while ((UCI_OK == uci_read_name(WIFI_TYPE, WIFI_VIF_SECTION, rnum, "ssid", buf, sizeof(buf))) && (rnum < nVifMax))
        {
            rnum++;
        }
        g_nVIFs = rnum;
    }
    *numberOfEntries = g_nVIFs;
    return UCI_OK;
}

int wifi_getVIFName(int ssid_index, char *ssid_ifname, size_t ssid_ifname_len)
{
    bool rc;
    char if_name[128];
    memset(if_name, 0, sizeof(if_name));
    rc = uci_read_name(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ssid", if_name, sizeof(if_name));
    if (rc == UCI_OK)
    {
        strncpy(ssid_ifname, target_unmap_ifname(if_name), ssid_ifname_len);
    }
    return rc;
}

int wifi_getSSIDName(int ssid_index, char *ssid_name, size_t ssid_name_len)
{
    return( uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ssid", ssid_name, ssid_name_len));
}

int wifi_getSSIDRadioIndex(int ssid_index, int *radio_index)
{
    int rc;
    char radio_ifname[20];

    rc = uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "device", radio_ifname, 20);
    if (rc == UCI_OK )
    {
        sscanf( radio_ifname, "radio%d", radio_index );
    }
    return rc;
}

int wifi_getSSIDRadioIfName(int ssid_index, char *radio_ifname, size_t radio_ifname_len)
{
    bool rc;
    char if_name[128];
    memset(if_name, 0, sizeof(if_name));
    rc = uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "device", if_name, 20);
    if (rc == UCI_OK)
    {
        strncpy(radio_ifname, target_unmap_ifname(if_name), radio_ifname_len);
    }
    return rc;
}

int wifi_getSSIDEnable(int ssid_index, bool *enabled )
{
    int rc;
    char result[20];

    *enabled = true;
    rc = uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "disabled", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0))
    {
        *enabled = false;
    }
    return UCI_OK;
}

int wifi_getApBridgeInfo(int ssid_index, char *bridge_info, char *tmp1, char *tmp2, size_t bridge_info_len)
{
    return( uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "network", bridge_info, bridge_info_len));
}

int wifi_getApIsolationEnable(int ssid_index, bool *enabled)
{
    int rc;
    char result[20];

    *enabled = true;    
    rc = uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "isolate", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0)) 
    {
        *enabled = false;
    }
    return UCI_OK;
}

int wifi_getApSsidAdvertisementEnable(int ssid_index, bool *enabled)
{
    int rc;
    char result[20];

    *enabled = true;    
    rc = uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "hidden", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0)) 
    {
        *enabled = false;
    }
    return UCI_OK;
}

int wifi_getBaseBSSID(int ssid_index,char *buf, size_t buf_len)
{
    return( uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "bssid", buf, buf_len));
}

bool wifi_setSSIDName(int ssid_index, char* ssidName)
{
    return uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ssid", ssidName);
}

bool wifi_setApSecurityModeEnabled(int ssid_index,
        const struct schema_Wifi_VIF_Config *vconf)
{
    bool rc = true;
    if (strcmp(vconf->security[0], OVSDB_SECURITY_ENCRYPTION_OPEN) == 0)
    {
        if (!uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "none"))
        {
            rc = false;
        }
    }
    else if (strcmp(vconf->security[0], OVSDB_SECURITY_ENCRYPTION_WPA_PSK) == 0)
    {
        char key[128];
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key) - 1, "%s", vconf->security[1]);

        if(!uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "key", key))
        {
            rc = false;
        }

        if (strcmp(vconf->security[2], OVSDB_SECURITY_MODE_WPA2) == 0)
        {
            if(!uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "psk2"))
            {
                rc = false;
            }
        }
        else if (strcmp(vconf->security[2], OVSDB_SECURITY_MODE_MIXED) == 0)
        {
            if(!uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "psk-mixed"))
            {
                rc = false;
            }
        }
    }

    return rc;
}

bool wifi_getApSecurityModeEnabled(int ssid_index, char *buf, size_t buf_len)
{
    return(uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", buf, buf_len));
}

int wifi_getApSecurityKeyPassphrase(int ssid_index, char *buf, size_t buf_len)
{
    return(uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "key", buf, buf_len));
}
