/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>
#include <math.h>
#include <glob.h>
#include <libgen.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <linux/limits.h>
#include <linux/if_ether.h>

#include <uci.h>
#include <uci_blob.h>

#include <syslog.h>

#include "const.h"
#include "target.h"
#include "nl80211.h"
#include "utils.h"
#include "vif.h"

static struct mode_map mode_map[] = {
	{ 0, "11b", "11b", NULL, "NOHT", 0 },
	{ 0, "11g", "11g", NULL, "NOHT", 0 },
	{ 1, "11a", "11a", NULL, "NOHT", 0 },
	{ 0, "11n", "11g", "HT20", "HT20", 0 },
	{ 0, "11n", "11g", "HT40", "HT40", 1 },
	{ 0, "11n", "11g", "HT40-", "HT40-", 1 },
	{ 0, "11n", "11g", "HT40+", "HT40+", 1 },
	{ 0, "11n", "11g", "HT80", "HT40", 1 },
	{ 0, "11n", "11g", "HT160", "HT40", 1 },
	{ 1, "11n", "11a", "HT20", "HT20", 0 },
	{ 1, "11n", "11a", "HT40", "HT40", 1 },
	{ 1, "11n", "11a", "HT40-", "HT40-", 1 },
	{ 1, "11n", "11a", "HT40+", "HT40+", 1 },
	{ 1, "11n", "11a", "HT80", "HT40", 1 },
	{ 1, "11n", "11a", "HT160", "HT40", 1 },
	{ 1, "11ac", "11a", "HT20", "VHT20", 0 },
	{ 1, "11ac", "11a", "HT40", "VHT40", 1 },
	{ 1, "11ac", "11a", "HT40-", "VHT40", 1 },
	{ 1, "11ac", "11a", "HT40+", "VHT40", 1 },
	{ 1, "11ac", "11a", "HT80", "VHT80", 1 },
	{ 1, "11ac", "11a", "HT160", "VHT160", 1 },
	{ 0, "11ax", "11g", "HT20", "HE20", 0 },
	{ 0, "11ax", "11g", "HT40", "HE40", 1 },
	{ 0, "11ax", "11g", "HT40-", "HE40", 1 },
	{ 0, "11ax", "11g", "HT40+", "HE40", 1 },
	{ 0, "11ax", "11g", "HT80", "HE80", 1 },
	{ 0, "11ax", "11g", "HT160", "HE80", 1 },
	{ 1, "11ax", "11a", "HT20", "HE20", 0 },
	{ 1, "11ax", "11a", "HT40", "HE40", 1 },
	{ 1, "11ax", "11a", "HT40-", "HE40", 1 },
	{ 1, "11ax", "11a", "HT40+", "HE40", 1 },
	{ 1, "11ax", "11a", "HT80", "HE80", 1 },
	{ 1, "11ax", "11a", "HT160", "HE160", 1 },
};

typedef enum {
	MHz20=0,
	MHz40 = 1,
	MHz80 = 2,
	MHz160 = 4
} bm_AllowedBw;

typedef struct {
	int freq;
	bm_AllowedBw bw;
} freqBwListEntry;

freqBwListEntry freqBwList[] ={{2412, MHz20},{2417, MHz20},{2422, MHz20},{2427, MHz20},{2432, MHz20},{2437, MHz20},{2442, MHz20},{2447, MHz20},{2452, MHz20},{2457, MHz20},{2462, MHz20},{2467, MHz20},{2472, MHz20}, {2484, MHz20},
		{ 5180, MHz20|MHz40|MHz80},{5200, MHz20},{5220, MHz20|MHz40},{5240, MHz20},{5260, MHz20|MHz40|MHz80},{5280, MHz20},{5300,MHz20|MHz40},{5320, MHz20},{5500, MHz20|MHz40|MHz80},{5520, MHz20},{5540,  MHz20|MHz40}, {5560, MHz20},
		{5580, MHz20|MHz40|MHz80},{5600, MHz20},{5620, MHz20|MHz40},{5640, MHz20},{5660, MHz20|MHz40|MHz80},{5680, MHz20},{5700, MHz20|MHz40},{5720, MHz20},{5745, MHz20|MHz40|MHz80},{5765, MHz20},{5785, MHz20|MHz40},{5805, MHz20},{5825, MHz20}};

#define REQ_BW(htmode) (!strcmp(htmode, "HT20") ? MHz20 : !strcmp(htmode, "HT40") ? MHz40 : !strcmp(htmode, "HT80") ? MHz80 : !strcmp(htmode, "HT160") ? MHz160 : MHz20)
#define REQ_MODE(bw) (bw==MHz20 ? "HT20": bw==MHz40 ? "HT40" : bw==MHz80 ? "HT80" : bw==MHz160 ? "HT160" : "HT20")

char * get_max_channel_bw_channel(int channel_freq, const char* htmode)
{
	unsigned int i;
	bm_AllowedBw requestedBw;

	requestedBw = REQ_BW(htmode);

	for ( i = 0; i < ARRAY_SIZE(freqBwList); i++) {
		if(freqBwList[i].freq == channel_freq) {
			while (requestedBw) {
				if (freqBwList[i].bw & requestedBw) {
					return REQ_MODE(requestedBw);
				}
				requestedBw >>= 1;
			}
		}
	}
	return "HT20";
}
struct mode_map *mode_map_get_uci(const char *band, const char *htmode, const char *hwmode)
{
	unsigned int i;
	int fiveg = 1;

	if (!strcmp(band, "2.4G"))
		fiveg = 0;

	for (i = 0; i < ARRAY_SIZE(mode_map); i++) {
		if (mode_map[i].fiveg != fiveg)
			continue;
		if (strcmp(mode_map[i].hwmode, hwmode))
			continue;
		if (!mode_map[i].htmode || !strcmp(mode_map[i].htmode, htmode))
			return &mode_map[i];
	}

	return NULL;
}

struct mode_map *mode_map_get_cloud(const char *htmode, const char *hwmode)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mode_map); i++) {
		if (strcmp(mode_map[i].ucihwmode, hwmode))
			continue;
		if (!strcmp(mode_map[i].ucihtmode, htmode))
			return &mode_map[i];
	}

	return NULL;
}

static int phy_from_device(char *_path, char *phy, unsigned int idx)
{
	char path[PATH_MAX];
	unsigned int i;
	int ret = -1;
	glob_t gl;

	snprintf(path, PATH_MAX, "%s/*", _path);
	if (glob(path, 0, NULL, &gl)) {
		perror("glob");
		return -1;
	}
	for (i = 0; i < gl.gl_pathc; i++) {
		if (i != idx)
			continue;
		strncpy(phy, basename(gl.gl_pathv[i]), 6);
		ret = 0;
		break;
	}
	globfree(&gl);
	return ret;
}

int phy_from_path(char *_path, char *phy, unsigned int idx)
{
	char *path = strdup(_path);
	unsigned int  i;
	int ret = -1;
	char *plus;
	glob_t gl;

	if (glob("/sys/class/ieee80211/*", 0, NULL, &gl))
                return -1;

	plus = strstr(path, "+");
	if (plus) {
		*plus = '\0';
		plus++;
	}

	for (i = 0; i < gl.gl_pathc; i++) {
		char symlink[PATH_MAX] = {};

		if (!readlink(gl.gl_pathv[i], symlink, PATH_MAX))
			continue;
		if (!strstr(symlink, path))
			continue;
		if (plus) {
			char r_path[PATH_MAX];
			if (!realpath(gl.gl_pathv[i], r_path))
				continue;
			if (phy_from_device(dirname(r_path), phy, atoi(plus)))
				continue;
		} else {
			strncpy(phy, basename(symlink), 6);
		}
		ret = 0;
		break;
	}
	globfree(&gl);
	free(path);

	return ret;
}

int phy_find_hwmon_helper(char *dir, char *file, char *hwmon)
{
	glob_t gl;
	if (glob(dir, GLOB_NOSORT | GLOB_MARK, NULL, &gl))
		return -1;
	if (gl.gl_pathc) {
		strcpy(hwmon, gl.gl_pathv[0]);
		strncat(hwmon, file, PATH_MAX);
	}
	globfree(&gl);
	return 0;
}

int phy_find_hwmon(char *phy, char *hwmon, bool *DegreesNotMilliDegrees)
{
	char tmp[PATH_MAX];
	*hwmon = '\0';
	snprintf(tmp, sizeof(tmp), "/sys/class/ieee80211/%s/device/hwmon/*", phy);
	if (!phy_find_hwmon_helper(tmp, "temp1_input", hwmon)) {
		*DegreesNotMilliDegrees=false;
		return 0;
	}
	snprintf(tmp, sizeof(tmp), "/sys/class/ieee80211/%s/cooling_device/subsystem/thermal_zone0/", phy);
	if (!phy_find_hwmon_helper(tmp, "temp", hwmon)) {
		*DegreesNotMilliDegrees=true;
		return 0;
	}
	return -1;
}

int phy_get_mac(char *phy, char *mac)
{
	int sz = ETH_ALEN * 3;
	char sysfs[PATH_MAX];
	int fd;

	snprintf(sysfs, sizeof(sysfs), "/sys/class/ieee80211/%s/macaddress", phy);
	fd = open(sysfs, O_RDONLY);
	if (fd < 0)
		return -1;
	memset(mac, 0, sz);
	read(fd, mac, sz - 1);
	close(fd);

	return 0;
}

int phy_is_ready(const char *name)
{
	struct wifi_phy *phy = phy_find(name);

	return phy != NULL;
}

int phy_get_tx_chainmask(const char *name)
{
	struct wifi_phy *phy = phy_find(name);

	if (!phy)
		return 0;
	return phy->tx_ant;
}

int phy_get_rx_chainmask(const char *name)
{
	struct wifi_phy *phy = phy_find(name);

	if (!phy)
		return 0;
	return phy->rx_ant;
}

int phy_get_tx_available_antenna(const char *name)
{
	struct wifi_phy *phy = phy_find(name);

	if (!phy)
		return 0;
	return phy->tx_ant_avail;
}

int phy_get_rx_available_antenna(const char *name)
{
	struct wifi_phy *phy = phy_find(name);

	if (!phy)
		return 0;
	return phy->rx_ant_avail;
}

int phy_get_max_tx_power(const char *name , int channel)
{
	struct wifi_phy *phy = phy_find(name);

	if (!phy)
		return 0;
	return phy->chanpwr[channel]/100; //units to dBm
}

int phy_get_channels(const char *name, int *channel)
{
	struct wifi_phy *phy = phy_find(name);
	int i, j = 0;

	if (!phy)
		return 0;

	for (i = 0; (i < IEEE80211_CHAN_MAX) && (j < 64); i++)
		if (phy->channel[i])
			channel[j++] = i;
	return j;
}

// Gets all the dfs channels avaible for a radio
int phy_get_dfs_channels(const char *name, int *dfs_channels)
{
	struct wifi_phy *phy = phy_find(name);
	int i, j = 0;

	if (!phy)
		return 0;

	for (i = 0; (i < IEEE80211_CHAN_MAX) && (j < 64); i++)
		if (phy->chandfs[i])
			dfs_channels[j++] = i;
	return j;
}

static void update_channels_state(struct schema_Wifi_Radio_State *rstate,
			int *index, const char *key, int *value, int value_len)
{
	int y, z = 0;
	char str[256];

	STRSCPY(rstate->channels_keys[*index], key);

	for (y = 0; y < value_len; y++)
		z += sprintf(&str[z], "%d,", value[y]);

	memcpy(rstate->channels[*index], str, z);
	*index = *index + 1;
}

int phy_get_channels_state(const char *name, struct schema_Wifi_Radio_State *rstate)
{
	struct wifi_phy *phy = phy_find(name);
	int i, j = 0, k = 0, l = 0;
	int dis[64], allow[64], radar[64];
	int ret = 0;

	if (!phy)
		return 0;

	LOGD("phy name :%s", name);

	for (i = 0; (i < IEEE80211_CHAN_MAX); i++) {
		if (phy->chandisabled[i]) {
			dis[j] = i;
			j++;
		} else if (phy->chandfs[i]) {
			radar[k] = i;
			k++;
		} else if (phy->channel[i]) {
			allow[l] = i;
			l++;
		}
	}

	if (j)
		update_channels_state(rstate, &ret, "disabled", dis, j);

	if (k)
		update_channels_state(rstate, &ret, "radar_detection", radar, k);

	if (l)
		update_channels_state(rstate, &ret, "allowed", allow, l);

	return ret;
}

int phy_get_band(const char *name, char *band)
{
	struct wifi_phy *phy = phy_find(name);

	*band = '\0';

	if (!phy)
		return 0;

	if (phy->band_2g)
		sprintf(band, "2.4G");
	else if (phy->band_5gl && phy->band_5gu)
		sprintf(band, "5G");
	else if (phy->band_5gl)
		sprintf(band, "5GL");
	else if (phy->band_5gu)
		sprintf(band, "5GU");
	else
		return -1;
	return 0;
}

int phy_lookup(char *name)
{
	char buf[200];
	int fd, pos;

	snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", name);

	fd = open(buf, O_RDONLY);
	if (fd < 0)
		return -1;
	pos = read(fd, buf, sizeof(buf) - 1);
	if (pos < 0) {
		close(fd);
		return -1;
	}
	buf[pos] = '\0';
	close(fd);
	return atoi(buf);
}

int vif_get_mac(char *vap, char *mac)
{
	int sz = ETH_ALEN * 3;
	char sysfs[PATH_MAX];
	int fd;

	snprintf(sysfs, sizeof(sysfs), "/sys/class/net/%s/address", vap);
	fd = open(sysfs, O_RDONLY);
	if (fd < 0)
		return -1;
	memset(mac, 0, sz);
	read(fd, mac, sz - 1);
	close(fd);

	return 0;
}

int vif_is_ready(const char *name)
{
	struct wifi_iface *iface = vif_find(name);

	return iface != NULL;
}

int blobmsg_add_hex16(struct blob_buf *b, const char *name, uint16_t val)
{
	char buf[5];

	snprintf(buf, sizeof(buf), "%04x", val);
	return blobmsg_add_string(b, name, buf);
}

int blobmsg_get_hex16(struct blob_attr *a)
{
	char *val = blobmsg_get_string(a);

	if (!val)
		return 0;
	return (int) strtoul(val, NULL, 16);
}

bool vif_state_to_conf(struct schema_Wifi_VIF_State *vstate,
		       struct schema_Wifi_VIF_Config *vconf)
{
#define VIF_COPY(x, y, z) {				\
	if (vstate->z##_exists) {			\
		SCHEMA_SET_##x(vconf->z, vstate->z);	\
		LOGT("vconf->" #z " = " #y , vconf->z);	\
	}						\
}
#define VIF_COPY_STR(x) VIF_COPY(STR, %s, x)
#define VIF_COPY_INT(x) VIF_COPY(INT, %d, x)

	int i;

	memset(vconf, 0, sizeof(*vconf));
	schema_Wifi_VIF_Config_mark_all_present(vconf);
	vconf->_partial_update = true;

	VIF_COPY_STR(if_name);
	VIF_COPY_STR(mode);
	VIF_COPY_INT(enabled);
	VIF_COPY_STR(bridge);
	VIF_COPY_INT(vlan_id);
	VIF_COPY_INT(ft_psk);
	VIF_COPY_INT(group_rekey);
	VIF_COPY_INT(ap_bridge);
	VIF_COPY_INT(wds);
	VIF_COPY_INT(uapsd_enable);
	VIF_COPY_INT(vif_radio_idx);
	VIF_COPY_STR(ssid_broadcast);
	VIF_COPY_STR(min_hw_mode);
	VIF_COPY_STR(ssid);
	VIF_COPY_INT(rrm);
	VIF_COPY_INT(btm);

	for (i = 0; i < vstate->security_len; i++){
		STRSCPY(vconf->security_keys[i], vstate->security_keys[i]);
		STRSCPY(vconf->security[i], vstate->security[i]);
	}
	vconf->security_len = vstate->security_len;

	VIF_COPY_STR(mac_list_type);
	for (i = 0; i < vstate->mac_list_len; i++)
		STRSCPY(vconf->mac_list[i], vstate->mac_list[i]);
	vconf->mac_list_len = vstate->mac_list_len;

	for (i = 0; i < vstate->custom_options_len; i++) {
		STRSCPY(vconf->custom_options_keys[i],
			vstate->custom_options_keys[i]);
		STRSCPY(vconf->custom_options[i], vstate->custom_options[i]);
	}
	vconf->custom_options_len = vstate->custom_options_len;




	for (i = 0; i < vstate->captive_allowlist_len; i++)
		STRSCPY(vconf->captive_allowlist[i], vstate->captive_allowlist[i]);
	vconf->captive_allowlist_len = vstate->captive_allowlist_len;

	for (i = 0; i < vstate->captive_portal_len; i++) {
		STRSCPY(vconf->captive_portal_keys[i],
			vstate->captive_portal_keys[i]);
		STRSCPY(vconf->captive_portal[i], vstate->captive_portal[i]);
	}
	vconf->captive_portal_len = vstate->captive_portal_len;

	return true;

#undef VIF_COPY
#undef VIF_COPY_STR
#undef VIF_COPY_INT
}

bool radio_state_to_conf(struct schema_Wifi_Radio_State *rstate,
			 struct schema_Wifi_Radio_Config *rconf)
{
#define RADIO_COPY(x, y, z) {				\
	if (rstate->z##_exists) {			\
		SCHEMA_SET_##x(rconf->z, rstate->z);	\
		LOGT("rconf->" #z " = " #y , rconf->z);	\
	}						\
}
#define RADIO_COPY_STR(x) RADIO_COPY(STR, %s, x)
#define RADIO_COPY_INT(x) RADIO_COPY(INT, %d, x)

	int i;

	memset(rconf, 0, sizeof(*rconf));
	schema_Wifi_Radio_Config_mark_all_present(rconf);
	rconf->_partial_update = true;
	rconf->vif_configs_present = false;

	RADIO_COPY_STR(if_name);
	RADIO_COPY_STR(freq_band);
	RADIO_COPY_STR(hw_type);
	RADIO_COPY_INT(enabled);
	RADIO_COPY_INT(channel);
	RADIO_COPY_INT(tx_power);
	RADIO_COPY_INT(bcn_int);
	RADIO_COPY_STR(country);
	RADIO_COPY_STR(ht_mode);
	RADIO_COPY_STR(hw_mode);
	RADIO_COPY_STR(freq_band);
	RADIO_COPY_STR(freq_band);

	for (i = 0; i < rstate->hw_config_len; i++) {
                STRSCPY(rconf->hw_config_keys[i], rstate->hw_config_keys[i]);
                STRSCPY(rconf->hw_config[i], rstate->hw_config[i]);
	}
	rconf->hw_config_len = rstate->hw_config_len;

	for (i = 0; i < rstate->custom_options_len; i++) {
		STRSCPY(rconf->custom_options_keys[i],
			rstate->custom_options_keys[i]);
		STRSCPY(rconf->custom_options[i], rstate->custom_options[i]);
	}
	rconf->custom_options_len = rstate->custom_options_len;

	return true;

#undef RADIO_COPY
#undef RADIO_COPY_STR
#undef RADIO_COPY_INT
}

void print_mac(char *mac_addr, const unsigned char *arg)
{
	sprintf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
		arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
}

int net_get_mac(char *iface, char *mac)
{
	int sz = ETH_ALEN * 3;
	char sysfs[PATH_MAX];
	int fd;

	snprintf(sysfs, sizeof(sysfs), "/sys/class/net/%s/address", iface);
	fd = open(sysfs, O_RDONLY);
	if (fd < 0)
		return -1;
	memset(mac, 0, sz);
	read(fd, mac, sz - 1);
	close(fd);

	return 0;
}

int net_get_mtu(char *iface)
{
	char sysfs[PATH_MAX];
	char buf[16];
	int fd;

	snprintf(sysfs, sizeof(sysfs), "/sys/class/net/%s/mtu", iface);
	fd = open(sysfs, O_RDONLY);
	if (fd < 0)
		return -1;
	memset(buf, 0, sizeof(buf));
	read(fd, buf, sizeof(buf) - 1);
	close(fd);

	return atoi(buf);
}

int net_is_bridge(char *iface)
{
	char sysfs[PATH_MAX];
	int fd;

	snprintf(sysfs, sizeof(sysfs), "/sys/class/net/%s/bridge/root_port", iface);
	fd = open(sysfs, O_RDONLY);
	if (fd < 0)
		return -1;
	close(fd);

	return 0;
}

int ieee80211_frequency_to_channel(int freq)
{
	/* see 802.11-2007 17.3.8.3.2 and Annex J */
	if (freq == 2484)
		return 14;
	else if (freq < 2484)
		return (freq - 2407) / 5;
	else if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	else if (freq <= 45000) /* DMG band lower limit */
		return (freq - 5000) / 5;
	else if (freq >= 58320 && freq <= 64800)
		return (freq - 56160) / 2160;
	else
		return 0;
}

int ieee80211_channel_to_frequency(int chan)
{
	/* see 802.11 17.3.8.3.2 and Annex J
	 * there are overlapping channel numbers in 5GHz and 2GHz bands */
	if (chan == 14)
		return 2484;
	else if (chan < 14)
		return 2407 + chan * 5;
	else if (chan >= 182 && chan <= 196)
		return 4000 + chan * 5;
	else
		return 5000 + chan * 5;
	return 0;
}
int get_current_channel(char *name)
{
	struct wifi_phy *phy = phy_find(name);

	if(phy)
		return phy->current_channel;

	return 0;
}

bool vif_get_security(struct schema_Wifi_VIF_State *vstate,  char *mode,  char *encryption, char *radiusServerIP,  char *password, char *port)
{
	int i=0;

	if(NULL == vstate->security)
	return false;

	strcpy(encryption, vstate->security[i]);
	i++;
	strcpy(password, vstate->security[i]);
	i++;
	strcpy(mode, vstate->security[i]);
	i++;
	if(!strcmp(encryption, OVSDB_SECURITY_ENCRYPTION_WPA_EAP))
	{
		strcpy(radiusServerIP, vstate->security[i]);
		i++;
		strcpy(port, vstate->security[i]);
		i++;
	}

	return true;

}

bool vif_get_key_for_key_distr(const char *secret, char *key_str)
{
	bool err = false;
	FILE *fp;
	char cmd_buf[256] = "openssl aes-128-cbc -nosalt -k ";

	strcat(cmd_buf, secret);
	strcat(cmd_buf, " -P 2>/dev/null | grep key | cut -d = -f2");
	fp = popen(cmd_buf, "r");

	
	if (fp && fscanf(fp, "%s", key_str)) {
		err = true;
	}

	fclose(fp);
	return err;
}

double dBm_to_mwatts(double dBm)
{
	return (pow(10,(dBm/10)));
}

double mWatts_to_dBm(double mW)
{
	return (10*log10(mW));
}

