/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>

#include <uci.h>
#include <uci_blob.h>
#include <curl/curl.h>

#include "log.h"
#include "const.h"
#include "target.h"
#include "evsched.h"
#include "radio.h"
#include "vif.h"
#include "vlan.h"
#include "nl80211.h"
#include "utils.h"
#include "phy.h"
#include "captive.h"
#include "ovsdb_table.h"
#include "ovsdb_sync.h"
#include "rrm_config.h"

#define MODULE_ID LOG_MODULE_ID_VIF
#define UCI_BUFFER_SIZE 80

extern ovsdb_table_t table_Wifi_VIF_Config;
extern ovsdb_table_t table_Hotspot20_Icon_Config;

extern struct blob_buf b;
extern struct blob_buf del;

struct blob_buf hs20 = { };
struct blob_buf osu = { };

struct blob_buf mesh = { };

enum {
	WIF_ATTR_DEVICE,
	WIF_ATTR_IFNAME,
	WIF_ATTR_INDEX,
	WIF_ATTR_MODE,
	WIF_ATTR_SSID,
	WIF_ATTR_BSSID,
	WIF_ATTR_CHANNEL,
	WIF_ATTR_ENCRYPTION,
	WIF_ATTR_KEY,
	WIF_ATTR_DISABLED,
	WIF_ATTR_HIDDEN,
	WIF_ATTR_ISOLATE,
	WIF_ATTR_NETWORK,
	WIF_ATTR_AUTH_SERVER,
	WIF_ATTR_AUTH_PORT,
	WIF_ATTR_AUTH_SECRET,
	WIF_ATTR_ACCT_SERVER,
	WIF_ATTR_ACCT_PORT,
	WIF_ATTR_ACCT_SECRET,
	WIF_ATTR_ACCT_INTERVAL,
	WIF_ATTR_REQ_CUI,
	WIF_ATTR_IEEE80211R,
	WIF_ATTR_IEEE80211W,
	WIF_ATTR_MOBILITY_DOMAIN,
	WIF_ATTR_FT_OVER_DS,
	WIF_ATTR_FT_PSK_LOCAL,
	WIF_ATTR_UAPSD,
	WIF_ATTR_VLAN_ID,
	WIF_ATTR_VID,
	WIF_ATTR_MACLIST,
	WIF_ATTR_MACFILTER,
	WIF_ATTR_RATELIMIT,
	WIF_ATTR_URATE,
	WIF_ATTR_DRATE,
	WIF_ATTR_CURATE,
	WIF_ATTR_CDRATE,
	WIF_ATTR_IEEE80211V,
	WIF_ATTR_BSS_TRANSITION,
	WIF_ATTR_DISABLE_EAP_RETRY,
	WIF_ATTR_IEEE80211K,
	WIF_ATTR_RTS_THRESHOLD,
	WIF_ATTR_DTIM_PERIOD,
	WIF_ATTR_INTERWORKING,
	WIF_ATTR_HS20,
	WIF_ATTR_HESSID,
	WIF_ATTR_ROAMING_CONSORTIUM,
	WIF_ATTR_VENUE_NAME,
	WIF_ATTR_VENUE_GROUP,
	WIF_ATTR_VENUE_TYPE,
	WIF_ATTR_VENUE_URL,
	WIF_ATTR_NETWORK_AUTH_TYPE,
	WIF_ATTR_IPADDR_TYPE_AVAILABILITY,
	WIF_ATTR_DOMAIN_NAME,
	WIF_ATTR_MCC_MNC,
	WIF_ATTR_NAI_REALM,
	WIF_ATTR_GAS_ADDR3,
	WIF_ATTR_QOS_MAP_SET,
	WIF_ATTR_OSEN,
	WIF_ATTR_ACCESS_NETWORK_TYPE,
	WIF_ATTR_INTERNET,
	WIF_ATTR_ESR,
	WIF_ATTR_ASRA,
	WIF_ATTR_UESA,
	WIF_ATTR_DISABLE_DGAF,
	WIF_ATTR_WAN_METRICS,
	WIF_ATTR_ANQP_DOMAIN_ID,
	WIF_ATTR_DEAUTH_REQUEST_TIMEOUT,
	WIF_ATTR_OPER_FRIENDLY_NAME,
	WIF_ATTR_OPERATING_CLASS,
	WIF_ATTR_OPER_ICON,
	WIF_ATTR_PROBE_ACCEPT_RATE,
	WIF_ATTR_CLIENT_CONNECT_THRESHOLD,
	WIF_ATTR_CLIENT_DISCONNECT_THRESHOLD,
	WIF_ATTR_BEACON_RATE,
	WIF_ATTR_MCAST_RATE,
	WIF_ATTR_RADIUS_NAS_ID_ATTR,
	WIF_ATTR_RADIUS_NAS_IP_ATTR,
	WIF_ATTR_RADIUS_AUTH_REQ_ATTR,
	WIF_ATTR_RADIUS_ACCT_REQ_ATTR,
	WIF_ATTR_MESH_ID,
	WIF_ATTR_MESH_FWDING,
	WIF_ATTR_MESH_MCAST_RATE,
	WIF_ATTR_DYNAMIC_VLAN,
	WIF_ATTR_DVLAN_FILE,
	WIF_ATTR_DVLAN_NAMING,
	WIF_ATTR_DVLAN_BRIDGE,
	WIF_ATTR_MIN_HW_MODE,
	__WIF_ATTR_MAX,
};

static const struct blobmsg_policy wifi_iface_policy[__WIF_ATTR_MAX] = {
	[WIF_ATTR_DEVICE] = { .name = "device", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_MODE] = { .name = "mode", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_IFNAME] = { .name = "ifname", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_INDEX] = { .name = "index", .type = BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_SSID] = { .name = "ssid", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_BSSID] = { .name = "bssid", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_CHANNEL] = { .name = "channel", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_ENCRYPTION] = { .name = "encryption", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_KEY] = { .name = "key", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_DISABLED] = { .name = "disabled", .type = BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_HIDDEN] = { .name = "hidden", .type = BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_ISOLATE] = { .name = "isolate", .type = BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_NETWORK] = { .name = "network", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_AUTH_SERVER] = { .name = "auth_server", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_AUTH_PORT] = { .name = "auth_port", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_AUTH_SECRET] = { .name = "auth_secret", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_ACCT_SERVER] = { .name = "acct_server", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_ACCT_PORT] = { .name = "acct_port", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_ACCT_SECRET] = { .name = "acct_secret", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_ACCT_INTERVAL] = { .name = "acct_interval", .type = BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_REQ_CUI] = { .name = "request_cui", .type = BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_IEEE80211R] = { .name = "ieee80211r", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_IEEE80211W] = { .name = "ieee80211w", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_MOBILITY_DOMAIN] = { .name = "mobility_domain", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_FT_OVER_DS] = { .name = "ft_over_ds", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_FT_PSK_LOCAL] = { .name = "ft_psk_generate_local" ,BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_UAPSD] = { .name = "uapsd", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_VLAN_ID] = { .name = "vlan_id", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_VID] = { .name = "vid", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_MACFILTER]  = { .name = "macfilter", .type = BLOBMSG_TYPE_STRING },
	[WIF_ATTR_MACLIST]  = { .name = "maclist", .type = BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_RATELIMIT] = { .name = "rlimit", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_URATE] = { .name = "urate", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_DRATE] = { .name = "drate", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_CURATE] = { .name = "curate", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_CDRATE] = { .name = "cdrate", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_IEEE80211V] = { .name = "ieee80211v", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_BSS_TRANSITION] = { .name = "bss_transition", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_DISABLE_EAP_RETRY] = { .name = "wpa_disable_eapol_key_retries", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_IEEE80211K] = { .name = "ieee80211k", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_RTS_THRESHOLD] = { .name = "rts_threshold", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_DTIM_PERIOD] = { .name = "dtim_period", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_INTERWORKING] = { .name = "interworking", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_HS20] = { .name = "hs20", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_HESSID] = { .name = "hessid", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_ROAMING_CONSORTIUM] = { .name = "roaming_consortium", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_VENUE_NAME] = { .name = "venue_name", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_VENUE_GROUP] = { .name = "venue_group", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_VENUE_TYPE] = { .name = "venue_type", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_VENUE_URL] = { .name = "venue_url", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_NETWORK_AUTH_TYPE] = { .name = "network_auth_type", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_IPADDR_TYPE_AVAILABILITY] = { .name = "ipaddr_type_availability", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_DOMAIN_NAME] = { .name = "domain_name", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_MCC_MNC] = { .name = "anqp_3gpp_cell_net", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_NAI_REALM] = { .name = "nai_realm", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_GAS_ADDR3] = { .name = "gas_address3", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_QOS_MAP_SET] = { .name = "qos_map_set", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_OSEN] = { .name = "osen", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_ACCESS_NETWORK_TYPE] = { .name = "access_network_type", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_INTERNET] = { .name = "internet", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_ESR] = { .name = "esr", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_ASRA] = { .name = "asra", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_UESA] = { .name = "uesa", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_DISABLE_DGAF] = { .name = "disable_dgaf", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_WAN_METRICS] = { .name = "hs20_wan_metrics", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_ANQP_DOMAIN_ID] = { .name = "anqp_domain_id", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_DEAUTH_REQUEST_TIMEOUT] = { .name = "hs20_deauth_req_timeout", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_OPER_FRIENDLY_NAME] = { .name = "hs20_oper_friendly_name", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_OPERATING_CLASS] = { .name = "hs20_operating_class", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_OPER_ICON] = { .name = "operator_icon", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_PROBE_ACCEPT_RATE] = { .name = "rssi_ignore_probe_request", .type = BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_CLIENT_CONNECT_THRESHOLD] = { .name = "signal_connect", .type = BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_CLIENT_DISCONNECT_THRESHOLD] = { .name = "signal_stay", .type = BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_BEACON_RATE] = { .name = "bcn_rate", .type = BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_MCAST_RATE] = { .name = "mcast_rate", .type = BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_RADIUS_NAS_ID_ATTR] = { .name = "nasid", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_RADIUS_NAS_IP_ATTR] = { .name = "ownip", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_RADIUS_AUTH_REQ_ATTR] = { .name = "radius_auth_req_attr", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_RADIUS_ACCT_REQ_ATTR] = { .name = "radius_acct_req_attr", BLOBMSG_TYPE_ARRAY },
	[WIF_ATTR_MESH_ID] = { .name = "mesh_id", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_MESH_FWDING] = { .name = "mesh_fwding", BLOBMSG_TYPE_BOOL },
	[WIF_ATTR_MESH_MCAST_RATE] = { .name = "mcast_rate", BLOBMSG_TYPE_INT32 },
	[WIF_ATTR_DYNAMIC_VLAN] = { .name = "dynamic_vlan", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_DVLAN_FILE] = { .name = "vlan_file", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_DVLAN_NAMING] = { .name = "vlan_naming", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_DVLAN_BRIDGE] = { .name = "vlan_bridge", BLOBMSG_TYPE_STRING },
	[WIF_ATTR_MIN_HW_MODE] = { .name = "min_hw_mode", BLOBMSG_TYPE_STRING },
};

const struct uci_blob_param_list wifi_iface_param = {
	.n_params = __WIF_ATTR_MAX,
	.params = wifi_iface_policy,
};

enum {
	WIF_MESH_NW_MTU,
	WIF_MESH_NW_PROTO,
	WIF_MESH_NW_MASTER,
	__WIF_MESH_NW_MAX,
};

static const struct blobmsg_policy wifi_mesh_nw_policy[__WIF_MESH_NW_MAX] = {
		[WIF_MESH_NW_MTU] = { .name = "mtu", BLOBMSG_TYPE_INT32 },
		[WIF_MESH_NW_PROTO] = { .name = "proto", BLOBMSG_TYPE_STRING },
		[WIF_MESH_NW_MASTER] = { .name = "master", BLOBMSG_TYPE_STRING },
};

const struct uci_blob_param_list wifi_mesh_param = {
		.n_params = __WIF_MESH_NW_MAX,
		.params = wifi_mesh_nw_policy,
};

enum {
	WIF_HS20_OSU_SERVER_URI,
	WIF_HS20_OSU_NAI,
	WIF_HS20_OSU_NAI2,
	WIF_HS20_OSU_METHOD_LIST,
	WIF_HS20_OSU_FRIENDLY_NAME,
	WIF_HS20_OSU_SERVICE_DESCRIPTION,
	WIF_HS20_OSU_ICON,
	__WIF_HS20_OSU_MAX,
};

static const struct blobmsg_policy wifi_hs20_osu_policy[__WIF_HS20_OSU_MAX] = {
		[WIF_HS20_OSU_SERVER_URI] = { .name = "osu_server_uri", BLOBMSG_TYPE_STRING },
		[WIF_HS20_OSU_NAI] = { .name = "osu_nai", BLOBMSG_TYPE_STRING },
		[WIF_HS20_OSU_NAI2] = { .name = "osu_nai2", BLOBMSG_TYPE_STRING },
		[WIF_HS20_OSU_METHOD_LIST] = { .name = "osu_method_list", BLOBMSG_TYPE_STRING },
		[WIF_HS20_OSU_FRIENDLY_NAME] = { .name = "osu_friendly_name", BLOBMSG_TYPE_ARRAY },
		[WIF_HS20_OSU_SERVICE_DESCRIPTION] = { .name = "service_description", BLOBMSG_TYPE_ARRAY },
		[WIF_HS20_OSU_ICON] = { .name = "osu_icon", BLOBMSG_TYPE_ARRAY },
};

const struct uci_blob_param_list wifi_hs20_osu_param = {
	.n_params = __WIF_HS20_OSU_MAX,
	.params = wifi_hs20_osu_policy,
};

enum {
	WIF_HS20_ICON_PATH,
	WIF_HS20_ICON_WIDTH,
	WIF_HS20_ICON_HEIGHT,
	WIF_HS20_ICON_LANG,
	WIF_HS20_ICON_TYPE,
	__WIF_HS20_ICON_MAX,
};

static const struct blobmsg_policy wifi_hs20_icon_policy[__WIF_HS20_ICON_MAX] = {
		[WIF_HS20_ICON_PATH] = { .name = "path", .type = BLOBMSG_TYPE_STRING },
		[WIF_HS20_ICON_WIDTH] = { .name = "width", BLOBMSG_TYPE_INT32 },
		[WIF_HS20_ICON_HEIGHT] = { .name = "height", BLOBMSG_TYPE_INT32 },
		[WIF_HS20_ICON_LANG] = { .name = "lang", .type = BLOBMSG_TYPE_STRING },
		[WIF_HS20_ICON_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
};

const struct uci_blob_param_list wifi_hs20_icon_param = {
	.n_params = __WIF_HS20_ICON_MAX,
	.params = wifi_hs20_icon_policy,
};

static struct vif_crypto {
	char *uci;
	char *encryption;
	char *mode;
	int enterprise;
} vif_crypto[] = {
	{ "psk", OVSDB_SECURITY_ENCRYPTION_WPA_PSK, OVSDB_SECURITY_MODE_WPA1, 0 },
	{ "psk2", OVSDB_SECURITY_ENCRYPTION_WPA_PSK, OVSDB_SECURITY_MODE_WPA2, 0 },
	{ "psk-mixed", OVSDB_SECURITY_ENCRYPTION_WPA_PSK, OVSDB_SECURITY_MODE_MIXED, 0 },
	{ "wpa", OVSDB_SECURITY_ENCRYPTION_WPA_EAP, OVSDB_SECURITY_MODE_WPA1, 1 },
	{ "wpa2", OVSDB_SECURITY_ENCRYPTION_WPA_EAP, OVSDB_SECURITY_MODE_WPA2, 1 },
	{ "wpa-mixed", OVSDB_SECURITY_ENCRYPTION_WPA_EAP, OVSDB_SECURITY_MODE_MIXED, 1 },
	{ "sae", OVSDB_SECURITY_ENCRYPTION_WPA_SAE, OVSDB_SECURITY_MODE_WPA3, 0 },
	{ "sae-mixed", OVSDB_SECURITY_ENCRYPTION_WPA_SAE, OVSDB_SECURITY_MODE_MIXED, 0 },
	{ "wpa3", OVSDB_SECURITY_ENCRYPTION_WPA3_EAP, OVSDB_SECURITY_MODE_WPA3, 1 },
	{ "wpa3-mixed", OVSDB_SECURITY_ENCRYPTION_WPA3_EAP, OVSDB_SECURITY_MODE_MIXED, 1 },
};

static int vif_config_security_set(struct blob_buf *b,
				    const struct schema_Wifi_VIF_Config *vconf)
{
	const char *encryption = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_ENCRYPT);
	const char *mode = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_MODE);
	unsigned int i;
	unsigned int acct_interval;
	const char *auth_server, *auth_port, *auth_secret, *security_key;

	if (!strcmp(encryption, OVSDB_SECURITY_ENCRYPTION_OPEN) || !mode)
		goto open;
	for (i = 0; i < ARRAY_SIZE(vif_crypto); i++) {
		if (strcmp(vif_crypto[i].encryption, encryption))
			continue;
		if (strcmp(vif_crypto[i].mode, mode))
			continue;
		blobmsg_add_string(b, "encryption", vif_crypto[i].uci);

		if (!strcmp(mode, OVSDB_SECURITY_MODE_WPA3)) {
			blobmsg_add_string(b, "ieee80211w", "2");
		} else {
			blobmsg_add_string(b, "ieee80211w", "1");
		}

		if (vif_crypto[i].enterprise) {
			acct_interval = 0;
			auth_server = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_RADIUS_IP);
			auth_port   = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_RADIUS_PORT);
			auth_secret = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_RADIUS_SECRET);
			LOGT("%s: Server IP %s port %s secret %s", vconf->if_name, auth_server, auth_port, auth_secret);
			if (!auth_server[0] || !auth_port[0] || !auth_secret[0]) {
				LOGI("%s: Incomplete RADIUS security config - SSID not created", vconf->if_name);
				return -1;
			}
			blobmsg_add_string(b, "auth_server", auth_server);
			blobmsg_add_string(b, "auth_port",   auth_port );
			blobmsg_add_string(b, "auth_secret", auth_secret );
			blobmsg_add_string(b, "acct_server",
					   SCHEMA_KEY_VAL(vconf->security, OVSDB_SECURITY_RADIUS_ACCT_IP));
			blobmsg_add_string(b, "acct_port",
					   SCHEMA_KEY_VAL(vconf->security, OVSDB_SECURITY_RADIUS_ACCT_PORT));
			blobmsg_add_string(b, "acct_secret",
					   SCHEMA_KEY_VAL(vconf->security, OVSDB_SECURITY_RADIUS_ACCT_SECRET));
			blobmsg_add_bool(b, "request_cui", 1);
			acct_interval = atoi(SCHEMA_KEY_VAL(vconf->security, OVSDB_SECURITY_RADIUS_ACCT_INTERVAL));

			if (acct_interval <= 600 && acct_interval >= 60 )
			{
				blobmsg_add_u32(b, "acct_interval", acct_interval);
			}
		} else {
			security_key = SCHEMA_KEY_VAL(vconf->security, SCHEMA_CONSTS_SECURITY_KEY);
			if (security_key == NULL) {
				LOGW("%s: Incomplete security config - SSID not created", vconf->if_name);
				return -1;
			}
			blobmsg_add_string(b, "key", security_key);
		}
	}
	return 0;
open:
	blobmsg_add_string(b, "encryption", "none");
	blobmsg_add_string(b, "key", "");
	blobmsg_add_string(b, "ieee80211w", "0");
	return 0;
}

static void vif_state_security_append(struct schema_Wifi_VIF_State *vstate,
				      int *index, const char *key, const char *value)
{
	STRSCPY(vstate->security_keys[*index], key);
	STRSCPY(vstate->security[*index], value);

	*index = *index + 1;
	vstate->security_len = *index;
}

static void vif_state_security_get(struct schema_Wifi_VIF_State *vstate,
				   struct blob_attr **tb)
{
	struct vif_crypto *vc = NULL;
	char *encryption = NULL;
	unsigned int i;
	int index = 0;
	char interval[5];

	if (tb[WIF_ATTR_ENCRYPTION]) {
		encryption = blobmsg_get_string(tb[WIF_ATTR_ENCRYPTION]);
		for (i = 0; i < ARRAY_SIZE(vif_crypto); i++)
			if (!strcmp(vif_crypto[i].uci, encryption))
				vc = &vif_crypto[i];
	}

	if (!encryption || !vc)
		goto out_none;

	if (vc->enterprise) {
		if (!tb[WIF_ATTR_AUTH_SERVER] || !tb[WIF_ATTR_AUTH_PORT] || !tb[WIF_ATTR_AUTH_SECRET])
			goto out_none;
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_ENCRYPTION,
					  vc->encryption);
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_MODE,
					  vc->mode);
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_RADIUS_SERVER_IP,
					  blobmsg_get_string(tb[WIF_ATTR_AUTH_SERVER]));
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_RADIUS_SERVER_PORT,
					  blobmsg_get_string(tb[WIF_ATTR_AUTH_PORT]));
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_RADIUS_SERVER_SECRET,
					  blobmsg_get_string(tb[WIF_ATTR_AUTH_SECRET]));

		if (tb[WIF_ATTR_ACCT_SERVER] && tb[WIF_ATTR_ACCT_PORT] && tb[WIF_ATTR_ACCT_SECRET])
		{
			vif_state_security_append(vstate, &index, OVSDB_SECURITY_RADIUS_ACCT_IP,
					blobmsg_get_string(tb[WIF_ATTR_ACCT_SERVER]));
			vif_state_security_append(vstate, &index, OVSDB_SECURITY_RADIUS_ACCT_PORT,
					blobmsg_get_string(tb[WIF_ATTR_ACCT_PORT]));
			vif_state_security_append(vstate, &index, OVSDB_SECURITY_RADIUS_ACCT_SECRET,
					blobmsg_get_string(tb[WIF_ATTR_ACCT_SECRET]));
		}

		if (tb[WIF_ATTR_ACCT_INTERVAL])
		{
			sprintf(interval, "%d", blobmsg_get_u32(tb[WIF_ATTR_ACCT_INTERVAL]));
			vif_state_security_append(vstate, &index, OVSDB_SECURITY_RADIUS_ACCT_INTERVAL,
					interval);
		}
	} else {
		if (!tb[WIF_ATTR_KEY])
			goto out_none;
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_ENCRYPTION,
					  vc->encryption);
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_MODE,
					  vc->mode);
		vif_state_security_append(vstate, &index, OVSDB_SECURITY_KEY,
					  blobmsg_get_string(tb[WIF_ATTR_KEY]));
	}
	return;

out_none:
	vif_state_security_append(vstate, &index, OVSDB_SECURITY_ENCRYPTION,
				  OVSDB_SECURITY_ENCRYPTION_OPEN);
}

/* Custom options table */
#define SCHEMA_CUSTOM_OPT_SZ            20
#define SCHEMA_CUSTOM_OPTS_MAX          12

const char custom_options_table[SCHEMA_CUSTOM_OPTS_MAX][SCHEMA_CUSTOM_OPT_SZ] =
{
	SCHEMA_CONSTS_RATE_LIMIT,
	SCHEMA_CONSTS_RATE_DL,
	SCHEMA_CONSTS_RATE_UL,
	SCHEMA_CONSTS_CLIENT_RATE_DL,
	SCHEMA_CONSTS_CLIENT_RATE_UL,
	SCHEMA_CONSTS_IEEE80211k,
	SCHEMA_CONSTS_RTS_THRESHOLD,
	SCHEMA_CONSTS_DTIM_PERIOD,
	SCHEMA_CONSTS_RADIUS_OPER_NAME,
	SCHEMA_CONSTS_RADIUS_NAS_ID,
	SCHEMA_CONSTS_RADIUS_NAS_IP,
	SCHEMA_CONSTS_DYNAMIC_VLAN,
};

static void vif_config_custom_opt_set(struct blob_buf *b, struct blob_buf *del,
                                      const struct schema_Wifi_VIF_Config *vconf)
{
	int i;
	char value[20];
	char operator_name[20];
	const char *opt;
	const char *val;
	struct blob_attr *n;

	for (i = 0; i < SCHEMA_CUSTOM_OPTS_MAX; i++) {
		opt = custom_options_table[i];
		val = SCHEMA_KEY_VAL(vconf->custom_options, opt);

		if (!val)
			strncpy(value, "0", 20);
		else
			strncpy(value, val, 20);

		if (strcmp(opt, "rate_limit_en") == 0) {
			if (strcmp(value, "1") == 0)
				blobmsg_add_bool(b, "rlimit", 1);
			else if (strcmp(value, "0") == 0)
				blobmsg_add_bool(b, "rlimit", 0);
		}
		else if (strcmp(opt, "ieee80211k") == 0) {
			if (strcmp(value, "1") == 0)
				blobmsg_add_bool(b, "ieee80211k", 1);
			else if (strcmp(value, "0") == 0)
				blobmsg_add_bool(b, "ieee80211k", 0);
		}
		else if (strcmp(opt, "ssid_ul_limit") == 0)
			blobmsg_add_string(b, "urate", value);
		else if (strcmp(opt, "ssid_dl_limit") == 0)
			blobmsg_add_string(b, "drate", value);
		else if (strcmp(opt, "client_dl_limit") == 0)
			blobmsg_add_string(b, "cdrate", value);
		else if (strcmp(opt, "client_ul_limit") == 0)
			blobmsg_add_string(b, "curate", value);
		else if (strcmp(opt, "rts_threshold") == 0)
			blobmsg_add_string(b, "rts_threshold", value);
		else if (strcmp(opt, "dtim_period") == 0)
			blobmsg_add_string(b, "dtim_period", value);
		else if (strcmp(opt, "radius_nas_id") == 0)
			blobmsg_add_string(b, "nasid", value);
		else if (strcmp(opt, "radius_nas_ip") == 0)
			blobmsg_add_string(b, "ownip", value);
		else if (strcmp(opt, "radius_oper_name") == 0 && strlen(value) > 0)
		{
			memset(operator_name, '\0', sizeof(operator_name));
			sprintf(operator_name, "126:s:%s", value);
			n = blobmsg_open_array(b,"radius_auth_req_attr");
			blobmsg_add_string(b, NULL, operator_name);
			blobmsg_close_array(b, n);

			n = blobmsg_open_array(b,"radius_acct_req_attr");
			blobmsg_add_string(b, NULL, operator_name);
			blobmsg_close_array(b, n);
		}
		else if (strcmp(opt, "dynamic_vlan") == 0) {
			blobmsg_add_string(b, "dynamic_vlan", value);

			if (strcmp(value, "1") == 0 || strcmp(value, "2") == 0) {
				strncpy(value, "/etc/hostapd.vlan", 20);
				blobmsg_add_string(b, "vlan_file", value);
				strncpy(value, "0", 20);
				blobmsg_add_string(b, "vlan_naming", value);
				strncpy(value, "br-wan.", 20);
				blobmsg_add_string(b, "vlan_bridge", value);
			}
			else {
				strncpy(value, "/etc/hostapd.vlan", 20);
				blobmsg_add_string(del, "vlan_file", value);
				strncpy(value, "0", 20);
				blobmsg_add_string(del, "vlan_naming", value);
				strncpy(value, "br-wan.", 20);
				blobmsg_add_string(del, "vlan_bridge", value);
			}
		}
	}
}

static void set_custom_option_state(struct schema_Wifi_VIF_State *vstate,
                                    int *index, const char *key,
                                    const char *value)
{
	STRSCPY(vstate->custom_options_keys[*index], key);
	STRSCPY(vstate->custom_options[*index], value);
	*index += 1;
	vstate->custom_options_len = *index;
}

static void vif_state_custom_options_get(struct schema_Wifi_VIF_State *vstate,
                                         struct blob_attr **tb)
{
	int i;
	int index = 0;
	const char *opt;
	char *buf = NULL;

	for (i = 0; i < SCHEMA_CUSTOM_OPTS_MAX; i++) {
		opt = custom_options_table[i];

		if (strcmp(opt, "rate_limit_en") == 0) {
			if (tb[WIF_ATTR_RATELIMIT]) {
				if (blobmsg_get_bool(tb[WIF_ATTR_RATELIMIT])) {
					set_custom_option_state(vstate, &index,
								custom_options_table[i],
								"1");
				} else {
					set_custom_option_state(vstate, &index,
								custom_options_table[i],
								"0");
				}
			}
		} else if (strcmp(opt, "ieee80211k") == 0) {
			if (tb[WIF_ATTR_IEEE80211K]) {
				if (blobmsg_get_bool(tb[WIF_ATTR_IEEE80211K])) {
					set_custom_option_state(vstate, &index,
								custom_options_table[i],
								"1");
				} else {
					set_custom_option_state(vstate, &index,
								custom_options_table[i],
								"0");
				}
			}
		} else if (strcmp(opt, "ssid_ul_limit") == 0) {
			if (tb[WIF_ATTR_URATE]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_URATE]);
				set_custom_option_state(vstate, &index,
							custom_options_table[i],
							buf);
			}
		} else if (strcmp(opt, "ssid_dl_limit") == 0) {
			if (tb[WIF_ATTR_DRATE]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_DRATE]);
				set_custom_option_state(vstate, &index,
							custom_options_table[i],
							buf);
			}
		} else if (strcmp(opt, "client_dl_limit") == 0) {
			if (tb[WIF_ATTR_CDRATE]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_CDRATE]);
				set_custom_option_state(vstate, &index,
							custom_options_table[i],
							buf);
			}
		} else if (strcmp(opt, "client_ul_limit") == 0) {
			if (tb[WIF_ATTR_CURATE]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_CURATE]);
				set_custom_option_state(vstate, &index,
							custom_options_table[i],
							buf);
			}
		} else if (strcmp(opt, "rts_threshold") == 0) {
			if (tb[WIF_ATTR_RTS_THRESHOLD]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_RTS_THRESHOLD]);
				set_custom_option_state(vstate, &index,
						custom_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "dtim_period") == 0) {
			if (tb[WIF_ATTR_DTIM_PERIOD]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_DTIM_PERIOD]);
				set_custom_option_state(vstate, &index,
						custom_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "radius_nas_id") == 0) {
			if (tb[WIF_ATTR_RADIUS_NAS_ID_ATTR]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_RADIUS_NAS_ID_ATTR]);
				set_custom_option_state(vstate, &index,
						custom_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "radius_nas_ip") == 0) {
			if (tb[WIF_ATTR_RADIUS_NAS_IP_ATTR]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_RADIUS_NAS_IP_ATTR]);
				set_custom_option_state(vstate, &index,
						custom_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "radius_oper_name") == 0) {
			static struct blobmsg_policy policy[1] = {
					{ .type = BLOBMSG_TYPE_STRING },
			};

			struct blob_attr *ttb[1];

			blobmsg_parse_array(policy, ARRAY_SIZE(ttb), ttb,
					blobmsg_data(tb[WIF_ATTR_RADIUS_AUTH_REQ_ATTR]),
					blobmsg_data_len(tb[WIF_ATTR_RADIUS_AUTH_REQ_ATTR]));

			if (ttb[0])
			{
				char value[20];
				buf = blobmsg_get_string(ttb[0]);
				if (sscanf(buf, "126:s:%s", value))
				{
					set_custom_option_state(vstate, &index,
							custom_options_table[i],
							value);
				}
			}
		} else if (strcmp(opt, "dynamic_vlan") == 0) {
			if (tb[WIF_ATTR_DYNAMIC_VLAN]) {
				buf = blobmsg_get_string(tb[WIF_ATTR_DYNAMIC_VLAN]);
				set_custom_option_state(vstate, &index,
							custom_options_table[i],
							buf);
			}
		}
	}
}

bool vif_state_update(struct uci_section *s, struct schema_Wifi_VIF_Config *vconf)
{
	struct blob_attr *tb[__WIF_ATTR_MAX] = { };
	struct schema_Wifi_VIF_State vstate;
	char mac[ETH_ALEN * 3];
	char *ifname, radio[IF_NAMESIZE];
	bool vifIsActive = false;
	char network_name[IF_NAMESIZE];

	LOGT("%s: get state", s->e.name);

	memset(&vstate, 0, sizeof(vstate));
	schema_Wifi_VIF_State_mark_all_present(&vstate);

	blob_buf_init(&b, 0);
	uci_to_blob(&b, s, &wifi_iface_param);
	blobmsg_parse(wifi_iface_policy, __WIF_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	if (!tb[WIF_ATTR_DEVICE] || !tb[WIF_ATTR_IFNAME] || !tb[WIF_ATTR_SSID]) {
		LOGN("%s: skipping invalid radio/ifname", s->e.name);
		return false;
	}

	ifname = blobmsg_get_string(tb[WIF_ATTR_IFNAME]);
	strncpy(radio, blobmsg_get_string(tb[WIF_ATTR_DEVICE]), IF_NAMESIZE);
	vifIsActive = vif_find(ifname);

	vstate._partial_update = true;
	vstate.associated_clients_present = false;
	vstate.vif_config_present = false;

	SCHEMA_SET_INT(vstate.rrm, 1);
	SCHEMA_SET_INT(vstate.ft_psk, 0);
	SCHEMA_SET_INT(vstate.group_rekey, 0);

	strscpy(vstate.mac_list_type, "none", sizeof(vstate.mac_list_type));
	vstate.mac_list_len = 0;

	SCHEMA_SET_STR(vstate.if_name, ifname);

	if (tb[WIF_ATTR_HIDDEN] && blobmsg_get_bool(tb[WIF_ATTR_HIDDEN]))
		SCHEMA_SET_STR(vstate.ssid_broadcast, "disabled");
	else
		SCHEMA_SET_STR(vstate.ssid_broadcast, "enabled");

	if (tb[WIF_ATTR_MODE])
		SCHEMA_SET_STR(vstate.mode, blobmsg_get_string(tb[WIF_ATTR_MODE]));
	else
		SCHEMA_SET_STR(vstate.mode, "ap");

	if (vifIsActive)
		SCHEMA_SET_STR(vstate.state, "up");
	else
		SCHEMA_SET_STR(vstate.state, "down");

	if (tb[WIF_ATTR_DISABLED] && blobmsg_get_bool(tb[WIF_ATTR_DISABLED]))
		SCHEMA_SET_INT(vstate.enabled, 0);
	else
		SCHEMA_SET_INT(vstate.enabled, 1);

	if (tb[WIF_ATTR_IEEE80211V] && blobmsg_get_bool(tb[WIF_ATTR_IEEE80211V]))
		SCHEMA_SET_INT(vstate.btm, 1);
	else
		SCHEMA_SET_INT(vstate.btm, 0);

	if (tb[WIF_ATTR_ISOLATE] && blobmsg_get_bool(tb[WIF_ATTR_ISOLATE]))
		SCHEMA_SET_INT(vstate.ap_bridge, 0);
	else
		SCHEMA_SET_INT(vstate.ap_bridge, 1);

//	if (tb[WIF_ATTR_UAPSD] && blobmsg_get_bool(tb[WIF_ATTR_UAPSD]))
		SCHEMA_SET_INT(vstate.uapsd_enable, true);
//	else
//		SCHEMA_SET_INT(vstate.uapsd_enable, false);

	if (tb[WIF_ATTR_NETWORK]) {
		strncpy(network_name, blobmsg_get_string(tb[WIF_ATTR_NETWORK]), IF_NAMESIZE);
		SCHEMA_SET_STR(vstate.bridge, network_name);
	}
	else
		LOGW("%s: unknown bridge/network", s->e.name);

	if (tb[WIF_ATTR_NETWORK] && !strncmp(blobmsg_get_string(tb[WIF_ATTR_NETWORK]), "gre", strlen("gre"))) {
		char network[IFNAMSIZ];
		char *token;
		strncpy(network, blobmsg_get_string(tb[WIF_ATTR_NETWORK]), IFNAMSIZ);
		token = strtok(network, "_");
		token = strtok(NULL, "_");
		if (NULL != token)
			SCHEMA_SET_INT(vstate.vlan_id, atoi(token));
	} else if (tb[WIF_ATTR_VLAN_ID]) {
		SCHEMA_SET_INT(vstate.vlan_id, blobmsg_get_u32(tb[WIF_ATTR_VLAN_ID]));
	} else {
		SCHEMA_SET_INT(vstate.vlan_id, 1);
	}

	if (tb[WIF_ATTR_SSID])
		SCHEMA_SET_STR(vstate.ssid, blobmsg_get_string(tb[WIF_ATTR_SSID]));
	else
		LOGW("%s: failed to get SSID", s->e.name);

	if (tb[WIF_ATTR_CHANNEL])
		SCHEMA_SET_INT(vstate.channel, blobmsg_get_u32(tb[WIF_ATTR_CHANNEL]));
	else
		LOGN("%s: Failed to get channel", vstate.if_name);

	if (tb[WIF_ATTR_MIN_HW_MODE])
		SCHEMA_SET_STR(vstate.min_hw_mode, blobmsg_get_string(tb[WIF_ATTR_MIN_HW_MODE]));

	if (tb[WIF_ATTR_BSSID])
		SCHEMA_SET_STR(vstate.mac, blobmsg_get_string(tb[WIF_ATTR_BSSID]));
	else if (tb[WIF_ATTR_IFNAME] && !vif_get_mac(blobmsg_get_string(tb[WIF_ATTR_IFNAME]), mac))
		SCHEMA_SET_STR(vstate.mac, mac);
	else
		LOGN("%s: Failed to get base BSSID (mac)", vstate.if_name);

	if (tb[WIF_ATTR_MACFILTER]) {
		if (!strcmp(blobmsg_get_string(tb[WIF_ATTR_MACFILTER]), "disable")) {
			vstate.mac_list_type_exists = true;
			SCHEMA_SET_STR(vstate.mac_list_type, "none");
		} else if(!strcmp(blobmsg_get_string(tb[WIF_ATTR_MACFILTER]), "allow")) {
			vstate.mac_list_type_exists = true;
			SCHEMA_SET_STR(vstate.mac_list_type, "whitelist");
		} else if(!strcmp(blobmsg_get_string(tb[WIF_ATTR_MACFILTER]), "deny")) {
			vstate.mac_list_type_exists = true;
			SCHEMA_SET_STR(vstate.mac_list_type, "blacklist");
		} else
			vstate.mac_list_type_exists = false;
	}

	if (tb[WIF_ATTR_MACLIST]) {
		struct blob_attr *cur = NULL;
		int rem = 0;

		vstate.mac_list_len = 0;
		blobmsg_for_each_attr(cur, tb[WIF_ATTR_MACLIST], rem) {
			if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
				continue;
			strcpy(vstate.mac_list[vstate.mac_list_len], blobmsg_get_string(cur));
			vstate.mac_list_len++;
		}
	}
	vif_state_security_get(&vstate, tb);
	vif_state_custom_options_get(&vstate, tb);
	if(!strcmp(network_name,"lan")) {
		vif_state_captive_portal_options_get(&vstate);
		vif_state_dhcp_allowlist_get(&vstate);
	}
	if (vconf) {
		LOGN("%s: updating vif config", radio);
		vif_state_to_conf(&vstate, vconf);
		radio_ops->op_vconf(vconf, radio);
	}
	LOGT("%s: updating vif state %s", radio, vstate.ssid);
	radio_ops->op_vstate(&vstate, radio);

	return true;
}

size_t write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

void vif_section_del(char *section_name)
{

	struct uci_package *wireless;
	struct uci_element *e = NULL, *tmp = NULL;
	int ret=0;

	ret= uci_load(uci, "wireless", &wireless);
	if (ret) {
		LOGD("%s: uci_load() failed with rc %d", section_name, ret);
		return;
	}
	uci_foreach_element_safe(&wireless->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);
		if ((s == NULL) || (s->type == NULL)) continue;
		if (!strcmp(s->type, section_name)) {
			uci_section_del(uci, "vif", "wireless", (char *)s->e.name, section_name);
		}
		else {
			continue;
		}
	}
	uci_commit(uci, &wireless, false);
	uci_unload(uci, wireless);
	reload_config = 1;

}

static bool hs20_download_icon(char *icon_name, char *icon_url)
{
	CURL *curl;
	FILE *fp;
	CURLcode res;
	char path[38];
	char name[32];

	strcpy(name, icon_name);
	sprintf(path, "/tmp/%s", name);

	curl = curl_easy_init();
	if (curl)
	{
		fp = fopen(path,"wb");

		if (fp == NULL)
		{
			curl_easy_cleanup(curl);
			return false;
		}

		if (icon_url == NULL)
		{
			curl_easy_cleanup(curl);
			fclose(fp);
			return false;
		}

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
		curl_easy_setopt(curl, CURLOPT_URL, icon_url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		fclose(fp);
		return res;
	}

	return true;
}


static void hs20_vif_config(struct blob_buf *b,
		const struct schema_Hotspot20_Config *hs2conf)
{
	struct blob_attr *n;
	json_t *where;
	struct schema_Hotspot20_Icon_Config iconconf;
	int i = 0;
	unsigned int len = 0;
	char domain_name[256];

	if (hs2conf->enable) {
		blobmsg_add_bool(b, "interworking", 1);
		blobmsg_add_bool(b, "hs20", 1);
	}
	else {
		blobmsg_add_bool(b, "interworking", 0);
		blobmsg_add_bool(b, "hs20", 0);
	}

	if (strlen(hs2conf->hessid))
		blobmsg_add_string(b, "hessid", hs2conf->hessid);

	n = blobmsg_open_array(b, "roaming_consortium");
	for (i = 0; i < hs2conf->roaming_oi_len; i++)
	{
		blobmsg_add_string(b, NULL, hs2conf->roaming_oi[i]);
	}
	blobmsg_close_array(b, n);

	n = blobmsg_open_array(b, "venue_name");
	for (i = 0; i < hs2conf->venue_name_len; i++)
	{
		blobmsg_add_string(b, NULL, hs2conf->venue_name[i]);
	}
	blobmsg_close_array(b, n);

	n = blobmsg_open_array(b, "venue_url");
	for (i = 0; i < hs2conf->venue_url_len; i++)
	{
		blobmsg_add_string(b, NULL, hs2conf->venue_url[i]);
	}
	blobmsg_close_array(b, n);

	len = 0;
	memset(domain_name, '\0', sizeof(domain_name));
	for (i = 0; i < hs2conf->domain_name_len; i++)
	{
		len = len + strlen(hs2conf->domain_name[i]);
		if (len < sizeof(domain_name))
		{
			strcat(domain_name, hs2conf->domain_name[i]);
			if (i != hs2conf->domain_name_len - 1)
				strcat(domain_name, ",");
		}
	}
	blobmsg_add_string(b, "domain_name", domain_name);

	n = blobmsg_open_array(b, "nai_realm");
	for (i = 0; i < hs2conf->nai_realm_len; i++)
	{
		blobmsg_add_string(b, NULL, hs2conf->nai_realm[i]);
	}
	blobmsg_close_array(b, n);

	if (strlen(hs2conf->network_auth_type))
		blobmsg_add_string(b, "network_auth_type", hs2conf->network_auth_type);

	if (strlen(hs2conf->mcc_mnc))
		blobmsg_add_string(b, "anqp_3gpp_cell_net", hs2conf->mcc_mnc);

	if (hs2conf->gas_addr3_behavior < 3)
		blobmsg_add_u32(b, "gas_address3", hs2conf->gas_addr3_behavior);

	if (strlen(hs2conf->qos_map_set))
		blobmsg_add_string(b, "qos_map_set", hs2conf->qos_map_set);

	if (hs2conf->access_network_type > 0)
		blobmsg_add_u32(b, "access_network_type", hs2conf->access_network_type);

	if (hs2conf->osen)
		blobmsg_add_bool(b, "osen", 1);
	else
		blobmsg_add_bool(b, "osen", 0);

	if (hs2conf->internet)
		blobmsg_add_bool(b, "internet", 1);
	else
		blobmsg_add_bool(b, "internet", 0);

	if (hs2conf->esr)
		blobmsg_add_bool(b, "esr", 1);
	else
		blobmsg_add_bool(b, "esr", 0);

	if (hs2conf->asra)
		blobmsg_add_bool(b, "asra", 1);
	else
		blobmsg_add_bool(b, "asra", 0);

	if (hs2conf->uesa)
		blobmsg_add_bool(b, "uesa", 1);
	else
		blobmsg_add_bool(b, "uesa", 0);

	if (hs2conf->disable_dgaf)
		blobmsg_add_bool(b, "disable_dgaf", 1);
	else
		blobmsg_add_bool(b, "disable_dgaf", 0);

	if (hs2conf->anqp_domain_id > 0)
		blobmsg_add_u32(b, "anqp_domain_id", hs2conf->anqp_domain_id);

	if (hs2conf->deauth_request_timeout > 0)
		blobmsg_add_u32(b, "hs20_deauth_req_timeout", hs2conf->deauth_request_timeout);

	if (hs2conf->operating_class > 0)
		blobmsg_add_u32(b, "hs20_operating_class", hs2conf->operating_class);

	if (strlen(hs2conf->wan_metrics))
		blobmsg_add_string(b, "hs20_wan_metrics", hs2conf->wan_metrics);

	n = blobmsg_open_array(b, "hs20_oper_friendly_name");
	for (i = 0; i < hs2conf->operator_friendly_name_len; i++)
	{
		blobmsg_add_string(b, NULL, hs2conf->operator_friendly_name[i]);
	}
	blobmsg_close_array(b, n);

	if (strlen(hs2conf->venue_group_type))
	{
		unsigned int venue_group;
		unsigned int venue_type;
		sscanf((char*)hs2conf->venue_group_type, "%d:%d", &venue_group, &venue_type);
		blobmsg_add_u32(b, "venue_group", venue_group);
		blobmsg_add_u32(b, "venue_type", venue_type);
	}

	if (hs2conf->operator_icons_len)
	{
		n = blobmsg_open_array(b, "operator_icon");
		for (i = 0; i < hs2conf->operator_icons_len; i++) {
			if (!(where = ovsdb_where_uuid("_uuid", hs2conf->operator_icons[i].uuid)))
				continue;

			if (ovsdb_table_select_one_where(&table_Hotspot20_Icon_Config, where, &iconconf))
			{
				blobmsg_add_string(b, NULL, iconconf.name);
			}
		}
		blobmsg_close_array(b, n);
	}

}

bool target_vif_config_del(const struct schema_Wifi_VIF_Config *vconf)
{
	struct uci_package *wireless;
	struct uci_element *e = NULL, *tmp = NULL;
	const char *ifname;
	int ret=0;

	vlan_del((char *)vconf->if_name);
	ret= uci_load(uci, "wireless", &wireless);
	if (ret) {
		LOGD("%s: uci_load() failed with rc %d", vconf->if_name, ret);
		return false;
	}
	uci_foreach_element_safe(&wireless->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);
		if ((s == NULL) || (s->type == NULL)) continue; 
		if (strcmp(s->type, "wifi-iface")) continue;

		ifname = uci_lookup_option_string( uci, s, "ifname" );
		if (!strcmp(ifname,vconf->if_name)) {
			uci_section_del(uci, "vif", "wireless", (char *)s->e.name, "wifi-iface");
			break;
		}
	}
	uci_commit(uci, &wireless, false);
	uci_unload(uci, wireless);
	reload_config = 1;
	return true;
}

void vif_hs20_osu_update(struct schema_Hotspot20_OSU_Providers *osuconf)
{
	int i;
	struct blob_attr *n;
	json_t *where;
	struct schema_Hotspot20_Icon_Config iconconf;

	blob_buf_init(&osu, 0);
	n = blobmsg_open_array(&osu, "osu_friendly_name");
	for (i = 0; i < osuconf->osu_friendly_name_len; i++)
	{
		blobmsg_add_string(&osu, NULL, osuconf->osu_friendly_name[i]);
	}
	blobmsg_close_array(&osu, n);

	n = blobmsg_open_array(&osu, "osu_service_desc");
	for (i = 0; i < osuconf->service_description_len; i++)
	{
		blobmsg_add_string(&osu, NULL, osuconf->service_description[i]);
	}
	blobmsg_close_array(&osu, n);

	if (strlen(osuconf->osu_nai))
		blobmsg_add_string(&osu, "osu_nai", osuconf->osu_nai);

	if (strlen(osuconf->osu_nai2))
		blobmsg_add_string(&osu, "osu_nai2", osuconf->osu_nai2);

	if (strlen(osuconf->server_uri))
		blobmsg_add_string(&osu, "osu_server_uri", osuconf->server_uri);

	if (osuconf->method_list_len)
		blobmsg_add_u32(&osu, "osu_method_list", osuconf->method_list[0]);

	if (osuconf->osu_icons_len)
	{
		n = blobmsg_open_array(&osu, "osu_icon");
		for (i = 0; i < osuconf->osu_icons_len; i++) {
			if (!(where = ovsdb_where_uuid("_uuid", osuconf->osu_icons[i].uuid)))
				continue;

			if (ovsdb_table_select_one_where(&table_Hotspot20_Icon_Config, where, &iconconf))
			{
				blobmsg_add_string(&osu, NULL, iconconf.name);
			}
		}
		blobmsg_close_array(&osu, n);
	}

	blob_to_uci_section(uci, "wireless", osuconf->osu_provider_name, "osu-provider",
			osu.head, &wifi_hs20_osu_param, NULL);
	reload_config = 1;
}


void vif_hs20_icon_update(struct schema_Hotspot20_Icon_Config *iconconf)
{
	char path[64];
	char name[16];

	if (hs20_download_icon(iconconf->name, iconconf->url))
	{
		blob_buf_init(&hs20, 0);

		if (iconconf->height)
			blobmsg_add_u32(&hs20, "height", iconconf->height);

		if (iconconf->width)
			blobmsg_add_u32(&hs20, "width", iconconf->width);

		if (strlen(iconconf->lang_code))
			blobmsg_add_string(&hs20, "lang", iconconf->lang_code);

		if (strlen(iconconf->img_type))
			blobmsg_add_string(&hs20, "type", iconconf->img_type);

		strcpy(name, iconconf->name);
		sprintf(path, "/tmp/%s", name);
		blobmsg_add_string(&hs20, "path", path);

		blob_to_uci_section(uci, "wireless", iconconf->icon_config_name, "hs20-icon",
				hs20.head, &wifi_hs20_icon_param, NULL);
		reload_config = 1;
	}
}

void vif_hs20_update(struct schema_Hotspot20_Config *hs2conf)
{
	int i;
	struct schema_Wifi_VIF_Config vconf;

	json_t *where;

	for (i = 0; i < hs2conf->vif_config_len; i++) {
		if (!(where = ovsdb_where_uuid("_uuid", hs2conf->vif_config[i].uuid)))
			continue;

		memset(&vconf, 0, sizeof(vconf));

		if (ovsdb_table_select_one_where(&table_Wifi_VIF_Config, where, &vconf))
		{
			blob_buf_init(&b, 0);
			hs20_vif_config(&b, hs2conf);
			blob_to_uci_section(uci, "wireless", vconf.if_name, "wifi-iface",
					b.head, &wifi_iface_param, NULL);
			reload_config = 1;
		}
	}
}

/* Mesh options table */
#define VIF_SCHEMA_MESH_OPT_SZ            32
#define VIF_SCHEMA_MESH_OPTS_MAX          2

const char vif_mesh_options_table[VIF_SCHEMA_MESH_OPTS_MAX][VIF_SCHEMA_MESH_OPT_SZ] =
{
		SCHEMA_CONSTS_MESH_MCAST_RATE,
		SCHEMA_CONSTS_MESH_MTU,
};

static void vif_mesh_opt_set(struct blob_buf *b, struct blob_buf *b1,
		const struct schema_Wifi_VIF_Config *vconf)
{
	int i;
	const char *opt;
	const char *val;

	for (i = 0; i < VIF_SCHEMA_MESH_OPTS_MAX; i++) {
		opt = vif_mesh_options_table[i];
		val = SCHEMA_KEY_VAL_NULL(vconf->mesh_options, opt);

		if (strcmp(opt, "mcast_rate") == 0) {
			if (!val)
				blobmsg_add_u32(b, "mcast_rate", 24000);
			else
				blobmsg_add_u32(b, "mcast_rate", atoi(val));
		}
		else if (strcmp(opt, "mtu") == 0) {
			if (!val)
				blobmsg_add_u32(b1, "mtu", 1532);
			else
				blobmsg_add_u32(b1, "mtu", atoi(val));
		}
	}
}

static int mesh_vif_config_set(const struct schema_Wifi_Radio_Config *rconf,
			const struct schema_Wifi_VIF_Config *vconf,
			const struct schema_Wifi_VIF_Config_flags *changed)
{
	blob_buf_init(&b, 0);
	blob_buf_init(&mesh, 0);

	blobmsg_add_string(&b, "ifname", vconf->if_name);
	blobmsg_add_string(&b, "device", rconf->if_name);
	blobmsg_add_string(&b, "mode", "mesh");

	if (changed->enabled)
		blobmsg_add_bool(&b, "disabled", vconf->enabled ? 0 : 1);

	if (vif_config_security_set(&b, vconf)) {
		return -1;
	}

	blobmsg_add_string(&b, "network", vconf->if_name);
	blobmsg_add_bool(&b, "mesh_fwding", 0);
	blobmsg_add_string(&b, "mesh_id", vconf->ssid);
	vif_mesh_opt_set(&b, &mesh, vconf);
	blob_to_uci_section(uci, "wireless", vconf->if_name, "wifi-iface",
			b.head, &wifi_iface_param, NULL);

	blobmsg_add_string(&mesh, "proto", "batadv_hardif");
	blobmsg_add_string(&mesh, "master", "bat0");
	blob_to_uci_section(uci, "network", vconf->if_name, "interface",
			mesh.head, &wifi_mesh_param, NULL);

	reload_config = 1;
	return 0;
}

static int ap_vif_config_set(const struct schema_Wifi_Radio_Config *rconf,
			const struct schema_Wifi_VIF_Config *vconf,
			const struct schema_Wifi_VIF_Config_flags *changed)
{
	int vid = 0;

	blob_buf_init(&b, 0);
	blob_buf_init(&del,0);

	blobmsg_add_string(&b, "ifname", vconf->if_name);
	blobmsg_add_string(&b, "device", rconf->if_name);
	blobmsg_add_string(&b, "mode", "ap");

	if (changed->enabled)
		blobmsg_add_bool(&b, "disabled", vconf->enabled ? 0 : 1);

	if (changed->ssid)
		blobmsg_add_string(&b, "ssid", vconf->ssid);

	if (changed->ssid_broadcast) {
		if (!strcmp(vconf->ssid_broadcast, "disabled"))
			blobmsg_add_bool(&b, "hidden", 1);
		else
			blobmsg_add_bool(&b, "hidden", 0);
	}

	if (changed->ap_bridge) {
		if (vconf->ap_bridge)
			blobmsg_add_bool(&b, "isolate", 0);
		else
			blobmsg_add_bool(&b, "isolate", 1);
	}

	if (changed->uapsd_enable) {
		if (vconf->uapsd_enable)
			blobmsg_add_bool(&b, "uapsd", 1);
		else
			blobmsg_add_bool(&b, "uapsd", 0);
	}

	if (changed->min_hw_mode)
		blobmsg_add_string(&b, "min_hw_mode", vconf->min_hw_mode);

	if (changed->ft_psk || changed->ft_mobility_domain) {
		if (vconf->ft_psk && vconf->ft_mobility_domain) {
			blobmsg_add_bool(&b, "ieee80211r", 1);
			blobmsg_add_hex16(&b, "mobility_domain", vconf->ft_mobility_domain);
			blobmsg_add_bool(&b, "ft_psk_generate_local", vconf->ft_psk);
			blobmsg_add_bool(&b, "ft_over_ds", 0);
			blobmsg_add_bool(&b, "reassociation_deadline", 1);
		} else {
			blobmsg_add_bool(&b, "ieee80211r", 0);
		}
	}

	if (changed->btm) {
		if (vconf->btm) {
			blobmsg_add_bool(&b, "ieee80211v", 1);
			blobmsg_add_bool(&b, "bss_transition", 1);
		} else {
			blobmsg_add_bool(&b, "ieee80211v", 0);
			blobmsg_add_bool(&b, "bss_transition", 0);
		}
	}

	if (changed->bridge)
		blobmsg_add_string(&b, "network", vconf->bridge);

	if (changed->vlan_id && strncmp(vconf->bridge, "gre", strlen("gre"))) {
		blobmsg_add_u32(&b, "vlan_id", vconf->vlan_id);
		if (vconf->vlan_id > 2)
			vid = vconf->vlan_id;
		blobmsg_add_u32(&b, "vid", vid);
	}

	if (changed->mac_list_type) {
		struct blob_attr *a;
		int i;
		if (!strcmp(vconf->mac_list_type, "whitelist"))
			blobmsg_add_string(&b, "macfilter", "allow");
		else if (!strcmp(vconf->mac_list_type,"blacklist"))
			blobmsg_add_string(&b, "macfilter", "deny");
		else
			blobmsg_add_string(&b, "macfilter", "disable");

		a = blobmsg_open_array(&b, "maclist");
		for (i = 0; i < vconf->mac_list_len; i++)
			blobmsg_add_string(&b, NULL, (char*)vconf->mac_list[i]);
		blobmsg_close_array(&b, a);
	}

	blobmsg_add_bool(&b, "wpa_disable_eapol_key_retries", 1);
	blobmsg_add_u32(&b, "channel", rconf->channel);

	if (vif_config_security_set(&b, vconf)) {
                return -1;
        }

	if (changed->custom_options)
		vif_config_custom_opt_set(&b, &del, vconf);

	rrm_config_vif(&b, &del, rconf->freq_band, vconf->if_name);

	blob_to_uci_section(uci, "wireless", vconf->if_name, "wifi-iface",
			    b.head, &wifi_iface_param, del.head);

	if (vid)
		vlan_add((char *)vconf->if_name, vid, !strcmp(vconf->bridge, "wan"));
	else
		vlan_del((char *)vconf->if_name);

	if (changed->captive_portal)
		vif_captive_portal_set(vconf,(char*)vconf->if_name);

	if(changed->captive_allowlist)
	{
		vif_dhcp_opennds_allowlist_set(vconf,(char*)vconf->if_name);
	}

	reload_config = 1;
	return 0;
}

bool target_vif_config_set2(const struct schema_Wifi_VIF_Config *vconf,
			const struct schema_Wifi_Radio_Config *rconf,
			const struct schema_Wifi_Credential_Config *cconfs,
			const struct schema_Wifi_VIF_Config_flags *changed,
			int num_cconfs)
{
	int rc;
	if (!strcmp(vconf->mode, "mesh")) {
		rc = mesh_vif_config_set(rconf, vconf, changed);
	} else {
		rc = ap_vif_config_set(rconf, vconf, changed);
	}

	return true;
}
