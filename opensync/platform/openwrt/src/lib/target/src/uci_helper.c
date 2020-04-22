#include <string.h>
#include "log.h"
#include "uci_helper.h" 

static int g_nRadios = -1;
static int g_nVIFs = -1;

bool uci_read(char* uci_path, char* uci_result, size_t size)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    int rc;

    if (!uci_result) return false;

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_path, true)) != UCI_OK || 
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
        LOGN("UCI lookup failed:  %s %d", uci_path, rc);
        uci_free_context(ctx);
        return false;
    }

    if (ptr.flags & UCI_LOOKUP_COMPLETE)
    {
        strncpy(uci_result, ptr.o->v.string, size);
    } else {
        LOGN("UCI lookup not COMPLETE");
    }

    uci_free_context(ctx);
    return true;
}

int uci_read2(char* type, char* section, int section_index, char* var, char* result, size_t result_len)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    char   uci_cmd[80];
    int rc;

    if (!result)     return UCI_ERR_MEM;
    if (!result_len) return UCI_ERR_MEM;

    snprintf(uci_cmd,sizeof(uci_cmd),"%s.@%s[%d].%s", type, section, section_index, var);
    LOGN("UCI command is: %s", uci_cmd ); 

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_cmd, true)) != UCI_OK ||
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
        LOGN("UCI lookup failed: %d", rc ); 
        uci_free_context(ctx);
        return rc;
    }

    if (ptr.flags & UCI_LOOKUP_COMPLETE)
    {
        strncpy(result, ptr.o->v.string, result_len);
    } else {
        LOGN("UCI lookup not COMPLETE");
    }

    uci_free_context(ctx);
    return rc;
}

int uci_read_name(char* type, char* section, int section_index, char * var, char* result, size_t result_len)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    char   uci_cmd[80];
    int rc;

    if (!result)     return UCI_ERR_MEM;
    if (!result_len) return UCI_ERR_MEM;

    snprintf(uci_cmd,sizeof(uci_cmd),"%s.@%s[%d].%s", type, section, section_index, var);
    LOGN("UCI command is: %s", uci_cmd );

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_cmd, true)) != UCI_OK ||
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
        LOGN("%s:%d UCI lookup failed:  %s %d", __func__, __LINE__, uci_cmd, rc);
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

bool uci_write(char* uci_path, char* uci_value)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    int rc;

    LOGN("UCI write parameters uci_path: %s value: %s", uci_path, uci_value);

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_path, true)) != UCI_OK ||
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
        LOGN("UCI lookup failed:  %s %d", uci_path, rc);
        uci_free_context(ctx);
        return false;
    }

    ptr.value = uci_value;

    if ((rc = uci_set(ctx, &ptr)) != UCI_OK)
    {
        LOGN("UCI Set error: %d", rc);
        uci_free_context(ctx);
        return false;
    }

    // TODO: Might want to put commit in its own function
    if ((rc = uci_commit(ctx, &ptr.p, false)) != UCI_OK)
    {
        LOGN("Commit error: %d", rc);
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
    return( uci_read_name(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "type", radio_ifname, radio_ifname_len));
}

int wifi_getRadioChannel(int radio_idx, int *channel)
{
    int rc;
    char buf[20];

    rc = uci_read2(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "channel", buf, 20);
    if (rc == UCI_OK )
    {
        *channel = strtol(buf,NULL,10);
    }
    return rc;
}

int wifi_getRadioHwMode(int radio_idx, char* hwMode, size_t hwMode_len)
{
    return( uci_read2(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "hwmode", hwMode, hwMode_len));
}

int wifi_getRadioEnable(int radio_idx, bool *enabled )
{
    int rc;
    char result[20];

    *enabled = true;
    rc = uci_read2(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "disabled", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0))
    {
        *enabled = false;
    }
    return UCI_OK;
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
    return( uci_read_name(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ssid", ssid_ifname, ssid_ifname_len));
} 

int wifi_getSSIDName(int ssid_index, char *ssid_name, size_t ssid_name_len)
{
    return( uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ssid", ssid_name, ssid_name_len));
}

int wifi_getSSIDRadioIndex(int ssid_index, int *radio_index)
{
    int rc;
    char radio_ifname[20];

    rc = uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "device", radio_ifname, 20);
    if (rc == UCI_OK )
    {
        sscanf( radio_ifname, "radio%d", radio_index );
    }
    return rc;
}

int wifi_getSSIDRadioIfName(int ssid_index, char *radio_ifname, size_t radio_ifname_len)
{
    return( uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "device", radio_ifname, 20));
}

int wifi_getSSIDEnable(int ssid_index, bool *enabled )
{
    int rc;
    char result[20];

    *enabled = true;
    rc = uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "disabled", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0))
    {
        *enabled = false;
    }
    return UCI_OK;
}

int wifi_getApBridgeInfo(int ssid_index, char *bridge_info, char *tmp1, char *tmp2, size_t bridge_info_len)
{
    return( uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "network", bridge_info, bridge_info_len));
}

int wifi_getApIsolationEnable(int ssid_index, bool *enabled)
{
    int rc;
    char result[20];

    *enabled = true;    
    rc = uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "isolate", result, 20);
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
    rc = uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "hidden", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0)) 
    {
        *enabled = false;
    }
    return UCI_OK;
}

int wifi_getBaseBSSID(int ssid_index,char *buf, size_t buf_len)
{
    return( uci_read2(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "bssid", buf, buf_len));
}

