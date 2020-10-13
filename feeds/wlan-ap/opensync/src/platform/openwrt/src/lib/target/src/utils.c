/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>
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
	{ 0, "11b", "11b", NULL, "NOHT" },
	{ 0, "11g", "11g", NULL, "NOHT" },
	{ 1, "11a", "11a", NULL, "NOHT" },
	{ 0, "11n", "11g", "HT20", "HT20" },
	{ 0, "11n", "11g", "HT40", "HT40" },
	{ 0, "11n", "11g", "HT40-", "HT40-" },
	{ 0, "11n", "11g", "HT40+", "HT40+" },
	{ 0, "11n", "11g", "HT80", "HT40" },
	{ 0, "11n", "11g", "HT160", "HT40" },
	{ 1, "11n", "11g", "HT20", "HT20" },
	{ 1, "11n", "11a", "HT40", "HT40" },
	{ 1, "11n", "11a", "HT40-", "HT40-" },
	{ 1, "11n", "11a", "HT40+", "HT40+" },
	{ 1, "11n", "11a", "HT80", "HT40" },
	{ 1, "11n", "11a", "HT160", "HT40" },
	{ 1, "11ac", "11a", "HT20", "VHT20" },
	{ 1, "11ac", "11a", "HT40", "VHT40" },
	{ 1, "11ac", "11a", "HT40-", "VHT40" },
	{ 1, "11ac", "11a", "HT40+", "VHT40" },
	{ 1, "11ac", "11a", "HT80", "VHT80" },
	{ 1, "11ac", "11a", "HT160", "VHT160" },
	{ 0, "11ax", "11g", "HT20", "HE20" },
	{ 0, "11ax", "11g", "HT40", "HE40" },
	{ 0, "11ax", "11g", "HT40-", "HE40" },
	{ 0, "11ax", "11g", "HT40+", "HE40" },
	{ 0, "11ax", "11g", "HT80", "HE80" },
	{ 0, "11ax", "11g", "HT160", "HE80" },
	{ 1, "11ax", "11a", "HT20", "HE20" },
	{ 1, "11ax", "11a", "HT40", "HE40" },
	{ 1, "11ax", "11a", "HT40-", "HE40" },
	{ 1, "11ax", "11a", "HT40+", "HE40" },
	{ 1, "11ax", "11a", "HT80", "HE80" },
	{ 1, "11ax", "11a", "HT160", "HE160" },
};

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

int phy_find_hwmon(char *phy, char *hwmon)
{
	char tmp[PATH_MAX];
	glob_t gl;

	*hwmon = '\0';
	snprintf(tmp, sizeof(tmp), "/sys/class/ieee80211/%s/device/hwmon/*", phy);
	if (glob(tmp, GLOB_NOSORT | GLOB_MARK, NULL, &gl))
                return -1;
	if (gl.gl_pathc) {
		strcpy(hwmon, gl.gl_pathv[0]);
		strncat(hwmon, "temp1_input", PATH_MAX);
	}
	globfree(&gl);

	return 0;
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
