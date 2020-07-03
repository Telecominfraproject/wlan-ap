#include <string.h>
#include "log.h"
#include "uci_helper.h"
#include "iwinfo.h"

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

int uci_remove(char* type, char* section, int section_index, char* option)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    char   uci_cmd[80];
    int rc;

    snprintf(uci_cmd,sizeof(uci_cmd),"%s.@%s[%d].%s", type, section, section_index, option);
    LOGD("UCI command remove: %s", uci_cmd );

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if ((rc = uci_lookup_ptr(ctx, &ptr, uci_cmd, true)) != UCI_OK ||
            (ptr.o == NULL || ptr.o->v.string == NULL))
    {
        LOGN("UCI remove %s.@%s[%d].%s not found: %d", type, section, section_index, option, rc);
        uci_free_context(ctx);
        return UCI_OK;
    }

    if (ptr.flags & UCI_LOOKUP_COMPLETE)
    {
        rc = uci_delete(ctx, &ptr);
    } else {
        LOGN("UCI lookup %s.@%s[%d].%s not complete: %d", type, section, section_index, option, rc);
    }

    // TODO: Might want to put commit in its own function
    if ((rc = uci_commit(ctx, &ptr.p, false)) != UCI_OK)
    {
        LOGN("UCI remove %s.@%s[%d].%s commit error: %d", type, section, section_index, option, rc);
        uci_free_context(ctx);
        return false;
    }

    uci_free_context(ctx);
    return rc;
}

bool uci_add_write(char* type, char* section)
{
    struct uci_ptr ptr;
    struct uci_context *ctx = NULL;
    struct uci_package *pkg = NULL;
    int rc = 0;

    ctx = uci_alloc_context();
    if (!ctx) return false;

    if(UCI_OK != uci_load(ctx, type, &pkg))
        return false;

    if((pkg = uci_lookup_package(ctx, type)) != NULL)
    {
        ptr.p = pkg;
        uci_add_section(ctx, pkg, section, &ptr.s);
    }

    if ((rc = uci_commit(ctx, &ptr.p, false)) != UCI_OK)
    {
        LOGN("UCI Add  %s.@%s commit error: %d", type, section, rc);
        uci_free_context(ctx);
        return false;
    }

    uci_free_context(ctx);

    return true;

}

bool uci_write_nw(char* type, char* section, char * option, char *uci_value)
{
    struct uci_ptr ptr;
    struct uci_context *ctx;
    char   uci_cmd[80];
    int rc;

    if (!uci_value)  return UCI_ERR_MEM;

    if (!option)
        snprintf(uci_cmd,sizeof(uci_cmd),"%s.%s", type, section);
    else
        snprintf(uci_cmd,sizeof(uci_cmd),"%s.%s.%s", type, section, option);

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
        LOGN("UCI write %s.%s.%s error: %d", type, section, option, rc);
        uci_free_context(ctx);
        return false;
    }

    // TODO: Might want to put commit in its own function
    if ((rc = uci_commit(ctx, &ptr.p, false)) != UCI_OK)
    {
        LOGN("UCI write %s.%s.%s commit error: %d", type, section, option, rc);
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

#define NETWORK_TYPE "network"
#define NETWORK_IFACE_SECTION "interface"

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

int wifi_getTxChainMask(int radioIndex, int *txChainMask)
{
    char command[64];
    FILE *fp = NULL;
    int fsize = 0;
    char *buffer=NULL;
    char *point=NULL;

    memset(command, 0, 64);

    sprintf(command,"iw phy%d info | grep 'Available Antennas' > /tmp/antennainfo.txt", radioIndex);

    if(system(command) == -1)
    {
	return false;
    }

    fp = fopen("/tmp/antennainfo.txt","r");

    if(fp != NULL)
    {
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	buffer = malloc(fsize+1);
	fread(buffer, 1, fsize, fp);
	buffer[fsize] = 0;
	fclose(fp);

	point = strstr(buffer, "0x");
	point = point + 2;
        sscanf(point, "%d", txChainMask);
	free(buffer);
	return true;
    }

    return false;
}

int wifi_getRadioAllowedChannel(int radioIndex, int *allowedChannelList, int *allowedChannelListLen)
{

    char radio_if_name[6];
    char buf[IWINFO_BUFSIZE];
    int  buflen;
    int  numberOfChannels=0;
    int  rc=false;
    struct iwinfo_freqlist_entry  *freq_list    = NULL;

    memset(radio_if_name, 0, sizeof(radio_if_name));

    if(radioIndex == 0)
    {
	strncpy(radio_if_name, "phy0", sizeof(radio_if_name));
    }
    else if(radioIndex == 1)
    {
	strncpy(radio_if_name, "phy1", sizeof(radio_if_name));
    }
    else if(radioIndex == 2)
    {
	strncpy(radio_if_name, "phy2", sizeof(radio_if_name));
    }
    else
    {
	return rc;
    }

    // find iwinfo type
    const char *if_type = iwinfo_type(radio_if_name);
    const struct iwinfo_ops *winfo_ops = iwinfo_backend_by_name(if_type);

    if(0 != winfo_ops->freqlist(radio_if_name, buf, &buflen))
    {
	return rc;
    }

    freq_list = (struct iwinfo_freqlist_entry *)buf;

    for(int i = 0; i < buflen; i += sizeof(struct iwinfo_freqlist_entry))
    {
	allowedChannelList[numberOfChannels] = freq_list->channel;

	LOGN("iwinfo :radio channel: %d", freq_list->channel);
	LOGN("iwinfo : state: radio channel: %d", allowedChannelList[numberOfChannels]);
	freq_list++;
	numberOfChannels++;
    }

    *allowedChannelListLen = numberOfChannels;

    if(numberOfChannels != 0)
	rc = true;

    return rc;

}

static eFreqBand freqBand_capture[UCI_MAX_RADIOS] = {eFreqBand_5GU,eFreqBand_24G,eFreqBand_5GL};

int wifi_getRadioFreqBand(int *allowedChannels, int numberOfChannels, char *freq_band)
{
    int rc = false;

    if(numberOfChannels == 0)
	return rc;

    if(allowedChannels[0] >= 1 && allowedChannels[numberOfChannels -1] <= 11){
	strcpy(freq_band, "2.4G");
	rc = true;
    }
    if(allowedChannels[0] >= 36 && allowedChannels[numberOfChannels -1] <= 64){
	strcpy(freq_band, "5GL");
	rc = true;
    }
    if(allowedChannels[0] >= 100 && allowedChannels[numberOfChannels -1] <= 165){
	strcpy(freq_band, "5GU");
	rc = true;
    }
    if(allowedChannels[0] <= 64 && allowedChannels[numberOfChannels -1] >= 100){
	strcpy(freq_band, "5G");
	rc = true;
    }

   return rc;
}
int wifi_getRadioMacaddress(int radio_idx, char *mac)
{
    int rc = UCI_OK;
    FILE *fd=NULL;
    char buff[18];
    char file_name[30];
    if(radio_idx >= UCI_MAX_RADIOS)
    {
        LOG(ERR,"Maximum Radios Exceeded");
        return rc = UCI_ERR_UNKNOWN;
    }
    snprintf(file_name,sizeof(file_name),"/sys/class/net/wlan%d/address",radio_idx);
    fd=fopen(file_name,"r");
    if(fd == NULL)
    {
        LOG(ERR,"Failed to open mac address input files");
        return rc=UCI_ERR_UNKNOWN;
    }
    if(fscanf(fd,"%s",buff) == EOF)
    {
        LOG(ERR,"Mac Address reading failed");
        fclose(fd);
        return rc = UCI_ERR_UNKNOWN;
    }
    fclose(fd);
    strcpy(mac,buff);
    return rc;
}

int wifi_getRadioHtMode(int radio_idx, char *ht_mode)
{
    int rc = true;
    char htmode[8];

    rc = uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "htmode", htmode, 8);
    if (rc == UCI_OK )
    {
       if (!strcmp(htmode, HTMODE_noht)) {
          strcpy(ht_mode, "HT20" );  /* should be ignored based on hw_mode */
       } else if (!strcmp(htmode, HTMODE_ht20)) {
          strcpy(ht_mode, "HT20");
       } else if (!strcmp(htmode, HTMODE_ht40m)) {
          strcpy(ht_mode, "HT40-");
       } else if (!strcmp(htmode, HTMODE_ht40p)) {
          strcpy(ht_mode, "HT40+");
       } else if (!strcmp(htmode, HTMODE_ht40)) {
          strcpy(ht_mode, "HT40");
       } else if (!strcmp(htmode, HTMODE_vht20)) {
          strcpy(ht_mode, "HT20");
       } else if (!strcmp(htmode, HTMODE_vht40)) {
          strcpy(ht_mode, "HT40");
       } else if (!strcmp(htmode, HTMODE_vht80)) {
          strcpy(ht_mode, "HT80");
       } else if (!strcmp(htmode, HTMODE_vht160)) {
          strcpy(ht_mode, "HT160");
       }
    } else {
        rc = false;
    }
    return rc;
}

int wifi_getRadioHwMode(int radio_idx, char * hw_mode)
{
    int rc1, rc2;
    char htmode[8];
    char hwmode[6];

    rc1 = uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "htmode", htmode, 8);
    rc2 = uci_read(WIFI_TYPE, WIFI_RADIO_SECTION, radio_idx, "hwmode", hwmode, 6);
    if ((rc1 == UCI_OK ) && (rc2 == UCI_OK ))
    {
       if (!strcmp(htmode, HTMODE_noht)) {
          if (!strcmp(hwmode, HWMODE_11a)) {
             strcpy(hw_mode, "11a");
          } else if (!strcmp(hwmode, HWMODE_11b)) {
             strcpy(hw_mode, "11b");
          } else if (!strcmp(hwmode, HWMODE_11g)) {
             strcpy(hw_mode, "11g");
          } else {
             rc1 = UCI_ERR_UNKNOWN;
          } 
       } else if ((!strcmp(htmode, HTMODE_ht20))  || (!strcmp(htmode, HTMODE_ht40m)) || 
                  (!strcmp(htmode, HTMODE_ht40p)) || (!strcmp(htmode, HTMODE_ht40))) {
          strcpy(hw_mode, "11n");
       } else if ((!strcmp(htmode, HTMODE_vht20)) || (!strcmp(htmode, HTMODE_vht40)) ||
                  (!strcmp(htmode, HTMODE_vht80)) || (!strcmp(htmode, HTMODE_vht160))) {
          strcpy(hw_mode, "11ac");
       } else {
          rc1 = UCI_ERR_UNKNOWN; 
       }
    }
    return rc1;
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

bool wifi_setRadioModes(int radioIndex, const char *freq_band, const char *ht_mode, const char *hw_mode)
{
    char chanbw[4] = "20";
    char hwmode[4];
    char htmode[8];
    bool rc1, rc2, rc3;

    if (! strcmp(hw_mode, "11a")) {
       strcpy(hwmode, HWMODE_11a);
       strcpy(htmode, HTMODE_noht);
    } else if (!strcmp(hw_mode, "11b")) {
       strcpy(hwmode, HWMODE_11b);
       strcpy(htmode, HTMODE_noht);
    } else if (!strcmp(hw_mode, "11g")) {
       strcpy(hwmode, HWMODE_11g);
       strcpy(htmode, HTMODE_noht);
    } else if (!strcmp(hw_mode, "11n")) {
       if (!strcmp(freq_band, "2.4G")) {
           strcpy(hwmode, HWMODE_11g);
       } else {
           strcpy(hwmode, HWMODE_11a);
       }
       if (!strcmp(ht_mode, "HT20")) {
          strcpy(htmode, HTMODE_ht20);
       } else if (!strcmp(ht_mode, "HT40-")) {
          strcpy(htmode, HTMODE_ht40m);
       } else if (!strcmp(ht_mode, "HT40+")) {
          strcpy(htmode, HTMODE_ht40p);
       } else  if ((!strcmp(ht_mode, "HT40")) | (!strcmp(ht_mode, "HT80")) || (!strcmp(ht_mode, "HT160"))) {
          strcpy(htmode, HTMODE_ht40);
       } else {
          return UCI_ERR_UNKNOWN;
       } 
    } else if (!strcmp(hw_mode, "11ac")) {
       strcpy(hwmode, HWMODE_11a);
       if (!strcmp(ht_mode, "HT20")) {
          strcpy(htmode, HTMODE_vht20);
       } else if ((!strcmp(ht_mode, "HT40")) || (!strcmp(ht_mode, "HT40+")) || (!strcmp(ht_mode, "HT40-"))) {
          strcpy(htmode, HTMODE_vht40);
       } else if (!strcmp(ht_mode, "HT80")) {
          strcpy(htmode, HTMODE_vht80);
       } else  if (!strcmp(ht_mode, "HT160")) {
          strcpy(htmode, HTMODE_vht160);
       } else {
          return UCI_ERR_UNKNOWN;
       }
    }

    if (!strcmp(freq_band, "2.4G")) {
       freqBand_capture[radioIndex] = eFreqBand_24G;
    } else if (!strcmp(freq_band, "5G")) {
       freqBand_capture[radioIndex] = eFreqBand_5G;
    } else if (!strcmp(freq_band, "5GL")) {
       freqBand_capture[radioIndex] = eFreqBand_5GL;
    } else if (!strcmp(freq_band, "5GU")) {
       freqBand_capture[radioIndex] = eFreqBand_5GU;
    }

    rc1 = uci_write(WIFI_TYPE, WIFI_RADIO_SECTION, radioIndex,"chanbw", chanbw);
    rc2 = uci_write(WIFI_TYPE, WIFI_RADIO_SECTION, radioIndex,"hwmode", hwmode);
    rc3 = uci_write(WIFI_TYPE, WIFI_RADIO_SECTION, radioIndex,"htmode", htmode);

    return (rc1 && rc2 && rc3);
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

int wifi_getSsidEnabled(int ssid_index, bool *enabled )
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


bool wifi_setSsidEnabled(int ssid_index, bool enabled)
{
    char    val[4];

    if (enabled) {
        sprintf(val, "%d", 0);
    } else {
        sprintf(val, "%d", 1);
    }

    LOGN("wifi_setSsidEnabled =  %s", val);

    return uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "disabled", val);
}

bool wifi_setFtMode(int ssid_index,
        const struct schema_Wifi_VIF_Config *vconf)
{
    int rc;
    char mobilityDdomain[5];
    char ft_psk[2];
    const char *encryption = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_ENCRYPT);

    if(vconf->ft_mobility_domain != 0 && (strcmp(encryption,OVSDB_SECURITY_ENCRYPTION_OPEN) !=0) )
	{
	  sprintf(mobilityDdomain, "%02x", vconf->ft_mobility_domain);
	  sprintf(ft_psk, "%d", vconf->ft_psk);
	  rc = uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ieee80211r", "1") &&
          uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "mobility_domain", mobilityDdomain) &&
          uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ft_psk_generate_local", ft_psk) &&
          uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ft_over_ds", "0") &&
          uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "reassociation_deadline", "1");
	}
     else
	{
	  rc = uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ieee80211r", "0");
	}

    return rc;
}

int wifi_getApBridgeInfo(int ssid_index, char *bridge_info, char *tmp1, char *tmp2, size_t bridge_info_len)
{
    return( uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "network", bridge_info, bridge_info_len));
}

int wifi_getApIsolationEnable(int ssid_index, bool *enabled)
{
    int rc;
    char result[20];

    *enabled = false;
    rc = uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "isolate", result, 20);
    if (( rc == UCI_OK ) && (strcmp(result,"1") == 0)) 
    {
        *enabled = true;
    }
    return UCI_OK;
}

bool wifi_setApIsolationEnable(int ssid_index, bool enabled)
{
    char    val[4];

    if (enabled) {
        sprintf(val, "%d", 0);
    } else {
        sprintf(val, "%d", 1);
    }

    LOGN("wifi_setApIsolationEnable =  %s", val);

    return uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "isolate", val);
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

bool wifi_setApSsidAdvertisementEnable(int ssid_index, bool enabled)
{
    char    val[4];

    if (enabled) {
        sprintf(val, "%d", 0);
    } else {
        sprintf(val, "%d", 1);
    }

    LOGN("wifi_setApSsidAdvertisementEnable =  %s", val);

    return uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "hidden", val);
}

int wifi_getBaseBSSID(int ssid_index,char *buf, size_t buf_len, int radio_idx)
{
    int rc=uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "bssid", buf, buf_len);

    if(UCI_OK != rc)
    {
    int rc = UCI_OK;
    FILE *fd=NULL;
    char addr[18];
    char file_name[30];

    if(radio_idx >= UCI_MAX_RADIOS)
    {
        LOG(ERR,"Maximum Radios Exceeded");
        return rc = UCI_ERR_UNKNOWN;
    }
    snprintf(file_name,sizeof(file_name),"/sys/class/net/wlan%d/address",radio_idx);
    fd=fopen(file_name,"r");

    if(fd == NULL)
    {
        LOG(ERR,"Failed to open mac address input files");
        return rc=UCI_ERR_UNKNOWN;
    }

    if(fscanf(fd,"%s",addr) == EOF)
    {
        LOG(ERR,"Mac Address reading failed");
        fclose(fd);
        return rc = UCI_ERR_UNKNOWN;
    }
    fclose(fd);
    strcpy(buf,addr);
    return rc;
    }
    return rc;
}

bool wifi_setSSIDName(int ssid_index, char* ssidName)
{
    return uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ssid", ssidName);
}

bool wifi_setApSecurityModeEnabled(int ssid_index,
        const struct schema_Wifi_VIF_Config *vconf)
{
    bool rc = true;
    const char *encryption = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_ENCRYPT);

    if (strcmp(encryption, OVSDB_SECURITY_ENCRYPTION_OPEN) == 0)
    {
        UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "none");
        UCI_REMOVE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "key");
        UCI_REMOVE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ieee80211w");
    }
    else if (strcmp(encryption, OVSDB_SECURITY_ENCRYPTION_WPA_PSK) == 0)
    {
        char key[128];
        const char *mode = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_MODE);
        memset(key, 0, sizeof(key));
        snprintf(key, sizeof(key) - 1, "%s", (char *)SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_KEY));

        UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "key", key);
        UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ieee80211w", "1");

        if (strcmp(mode, OVSDB_SECURITY_MODE_WPA2) == 0)
        {
            UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "psk2");
        }
        else if (strcmp(mode, OVSDB_SECURITY_MODE_WPA1) == 0)
        {
            UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "psk");
        }
        else if (strcmp(mode, OVSDB_SECURITY_MODE_MIXED) == 0)
        {
            UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "psk-mixed");
        }
        else
        {
            return false;
        }
    }
    else if (strcmp(encryption, OVSDB_SECURITY_ENCRYPTION_WPA_EAP) == 0)
    {
        const char *mode = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_MODE);

        if (strcmp(mode, OVSDB_SECURITY_MODE_WPA2) == 0)
        {
            UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "wpa2");
        }
        else if (strcmp(mode, OVSDB_SECURITY_MODE_WPA1) == 0)
        {
            UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "wpa");
        }
        else if (strcmp(mode, OVSDB_SECURITY_MODE_MIXED) == 0)
        {
            UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "encryption", "wpa-mixed");
        }
        else
        {
            return false;
        }

        UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "server",
                (char *)SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_RADIUS_IP));
        UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "port",
                (char *)SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_RADIUS_PORT));
        UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "auth_secret",
                (char *)SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_RADIUS_SECRET));
        UCI_WRITE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "ieee80211w", "1");
        UCI_REMOVE(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "key");
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

bool wifi_getApSecurityRadiusServer(
        int ssid_index, char *radius_ip, char *radius_port, char *radius_secret)
{
    UCI_READ(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "server", radius_ip, UCI_BUFFER_SIZE);
    UCI_READ(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "port", radius_port, UCI_BUFFER_SIZE);
    UCI_READ(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "auth_secret", radius_secret, UCI_BUFFER_SIZE);

    return true;
}


bool wifi_setApBridgeInfo(int ssid_index, char *bridge_info)
{
    return( uci_write(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "network", bridge_info));
}

bool wifi_getApVlanId(int ssid_index, int *vlan_id)
{
    char result[10];
    char *p = NULL;
    *vlan_id = 1;

    memset(result, 0, sizeof(result));
    uci_read(WIFI_TYPE, WIFI_VIF_SECTION, ssid_index, "network", result, 10);

    if ((p = strstr(result, "vlan")) != NULL)
    {
        long int v = strtol(&p[4], NULL, 10);

	    *vlan_id = (int) v;
	    LOGN("wifi_getApVlanId: %d", *vlan_id);
        return UCI_OK;
    }

    return false;
}

#define MAX_VLANS 200
struct vlan_list
{
    int vlan[MAX_VLANS];
    int index;
} vlist;

bool wifi_setApVlanNetwork(int ssid_index, int vlan_id)
{
    char tmp[10];
    char eth[10];
    char vlan[10];
    char vendor[128];
    int index = 0;

    if (vlan_id > 2)
    {
        for (index = 0; index < vlist.index; index++)
        {
            if (vlist.vlan[index] == vlan_id)
               return true;
        }
        vlist.vlan[vlist.index++] = vlan_id;

        memset(vlan, 0, sizeof(vlan));
        snprintf(vlan, sizeof(vlan) - 1, "%d", vlan_id);

        LOGI("wifi_setApVlanNetwork =  %d", vlan_id);
        memset(tmp, 0, sizeof(tmp));
        snprintf(tmp, sizeof(tmp) - 1, "vlan%d", vlan_id);

        memset(eth, 0, sizeof(eth));
        if (target_platform_version_get(vendor, 128))
        {
            if (!strncmp(vendor, "OPENWRT_ECW5410", 14) ||
                !strncmp(vendor, "OPENWRT_ECW5211", 14))
                snprintf(eth, sizeof(eth) - 1, "eth0.%d", vlan_id);
            else if (!strncmp(vendor, "OPENWRT_EA8300", 14))
                snprintf(eth, sizeof(eth) - 1, "eth1.%d", vlan_id);
            else if (!strncmp(vendor, "OPENWRT_AP2220", 14))
                snprintf(eth, sizeof(eth) - 1, "eth0.%d", vlan_id);
            else
                snprintf(eth, sizeof(eth) - 1, "eth1.%d", vlan_id);
        }
        uci_write_nw(NETWORK_TYPE, tmp, NULL, NETWORK_IFACE_SECTION);
        uci_write_nw(NETWORK_TYPE, tmp, "type", "bridge");
        uci_write_nw(NETWORK_TYPE, tmp, "ifname", eth);
        uci_write_nw(NETWORK_TYPE, tmp, "proto", "dhcp");

        memset(vendor, 0, sizeof(vendor));

        if (target_platform_version_get(vendor, 128))
        {
            if (!strncmp(vendor, "OPENWRT_EA8300", 14))
            {
                uci_add_write("network", "switch_vlan");
                uci_write("network", "switch_vlan", -1, "device", "switch0");
                uci_write("network", "switch_vlan", -1, "ports", "0t 5t");
                uci_write("network", "switch_vlan", -1, "vlan", vlan);
            }
        }
        return wifi_setApBridgeInfo(ssid_index, tmp);
    }

    return false;
}
