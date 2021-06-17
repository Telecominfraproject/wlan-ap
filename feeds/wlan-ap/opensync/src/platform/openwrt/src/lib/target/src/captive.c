/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <uci.h>
#include <uci_blob.h>
#include <arpa/inet.h>
#include "log.h"
#include "const.h"
#include "target.h"
#include "evsched.h"
#include "radio.h"
#include "nl80211.h"
#include "utils.h"
#include "captive.h"
#include <libubox/avl-cmp.h>
#include <libubox/avl.h>
#include <libubox/vlist.h>
#include <net/if.h>
#include "fixup.h"

static struct blob_buf cap={ };
static struct blob_buf cap_blob={ };
static struct blob_buf url_buf={ };
static struct uci_package *opennds;
static struct uci_context *caps_uci;
static struct uci_context *capg_uci;
struct blob_attr *d;

#define SCHEMA_CAPTIVE_PORTAL_OPT_SZ            255
#define SCHEMA_CAPTIVE_PORTAL_OPTS_MAX          14
enum {
	NDS_ATTR_SESSIONTIMEOUT,
	NDS_ATTR_GATEWAYINTERFACE,
	NDS_ATTR_GATEWAYNAME,
	NDS_ATTR_LOGIN_OPTION_ENABLED,
	NDS_ATTR_ENABLED,
	NDS_ATTR_AUTHENTICATED_USERS,
	NDS_ATTR_CAPTIVE_ALLOWLIST,
	NDS_ATTR_MACMECHANISM,
	NDS_ATTR_BROWSER_TITLE,
	NDS_ATTR_SPLASH_PAGE_LOGO,
	NDS_ATTR_PAGE_BACKGROUND_LOGO,
	NDS_ATTR_SPLASH_PAGE_TITLE,
	NDS_ATTR_REDIRECT_URL,
	NDS_ATTR_AUTHENTICATION,
	NDS_ATTR_ACCEPTANCE_POLICY,
	NDS_ATTR_LOGIN_SUCCESS_TEXT,
	NDS_ATTR_SPLASH_PAGE,
	NDS_ATTR_BINAUTH_SCRIPT,
	NDS_ATTR_WEB_ROOT,
	NDS_ATTR_PREAUTH,
	NDS_ATTR_USERNAMEPASS_FILE,
	NDS_ATTR_RADIUS_PORT,
	NDS_ATTR_RADIUS_IP,
	NDS_ATTR_RADIUS_SECRET,
	NDS_ATTR_RADIUS_AUTH_TYPE,
	NDS_ATTR_FWHOOK_ENABLED,
	NDS_ATTR_USE_OUTDATED_MHD,
	NDS_ATTR_UNESCAPE_CALLBACK_ENABLED,
	NDS_ATTR_MAXCLIENTS,
	NDS_ATTR_PREAUTHIDLEYIMEOUT,
	NDS_ATTR_AUTHIDLETIMEOUT,
	NDS_ATTR_CHECKINTERVAL,
	NDS_ATTR_UPLOADRATE,
	NDS_ATTR_DOWNLOADRATE,
	NDS_ATTR_RATECHECKWINDOW,
	NDS_ATTR_UPLOADQUOTA,
	NDS_ATTR_DOWNLOADQUOTA,
	NDS_ATTR_USERS_TO_ROUTER,
	__NDS_ATTR_MAX,
};
const struct blobmsg_policy opennds_policy[__NDS_ATTR_MAX] = {
	[NDS_ATTR_SESSIONTIMEOUT] = { .name = "sessiontimeout", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_GATEWAYINTERFACE] = { .name = "gatewayinterface", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_GATEWAYNAME] = { .name = "gatewayname", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_LOGIN_OPTION_ENABLED] = { .name = "login_option_enabled", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_AUTHENTICATED_USERS]  = { .name = "authenticated_users", .type = BLOBMSG_TYPE_ARRAY },
	[NDS_ATTR_CAPTIVE_ALLOWLIST]  = { .name = "preauthenticated_users", .type = BLOBMSG_TYPE_ARRAY },
	[NDS_ATTR_MACMECHANISM]  = { .name = "macmechanism", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_PREAUTH]  = { .name = "preauth", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_BROWSER_TITLE]  = { .name = "browser_title", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_SPLASH_PAGE_LOGO]  = { .name = "splash_page_logo", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_PAGE_BACKGROUND_LOGO]  = { .name = "page_background_logo", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_SPLASH_PAGE_TITLE]  = { .name = "splash_page_title", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_REDIRECT_URL]  = { .name = "redirectURL", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_AUTHENTICATION]  = { .name = "authentication", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_ACCEPTANCE_POLICY]  = { .name = "acceptance_policy", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_LOGIN_SUCCESS_TEXT]  = { .name = "login_success_text", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_SPLASH_PAGE]  = { .name = "splashpage", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_BINAUTH_SCRIPT]  = { .name = "binauth", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_WEB_ROOT]  = { .name = "webroot", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_USERNAMEPASS_FILE]  = { .name = "username_password_file", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_RADIUS_PORT]  = { .name = "radius_server_port", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_RADIUS_SECRET]  = { .name = "radius_server_secret", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_RADIUS_IP]  = { .name = "radius_server_ip", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_RADIUS_AUTH_TYPE]  = { .name = "radius_auth_type", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_FWHOOK_ENABLED]  = { .name = "fwhook_enabled", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_USE_OUTDATED_MHD]  = { .name = "use_outdated_mhd", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_UNESCAPE_CALLBACK_ENABLED]  = { .name = "unescape_callback_enabled", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_MAXCLIENTS]  = { .name = "maxclients", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_PREAUTHIDLEYIMEOUT]  = { .name = "preauthidletimeout", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_AUTHIDLETIMEOUT]  = { .name = "authidletimeout", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_CHECKINTERVAL]  = { .name = "checkinterval", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_UPLOADRATE]  = { .name = "uploadrate", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_DOWNLOADRATE]  = { .name = "downloadrate", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_RATECHECKWINDOW]  = { .name = "ratecheckwindow", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_UPLOADQUOTA]  = { .name = "uploadquota", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_DOWNLOADQUOTA]  = { .name = "downloadquota", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_USERS_TO_ROUTER]  = { .name = "users_to_router", .type = BLOBMSG_TYPE_ARRAY },
};

struct blob_buf dnsmas={ };
static struct uci_package *dhcp;
static struct uci_context *dns;
struct uci_section *ip_section;
struct blob_attr *cur = NULL;
const struct uci_blob_param_list opennds_param = {
	.n_params = __NDS_ATTR_MAX,
	.params = opennds_policy,
};

enum {
	DNS_ATTR_IPSET,
	__DNS_ATTR_MAX,
};

const struct blobmsg_policy dnsm_policy[__DNS_ATTR_MAX] = {
	[DNS_ATTR_IPSET] = { .name = "ipset", .type = BLOBMSG_TYPE_ARRAY },
};

const struct uci_blob_param_list dnsm_param = {
	.n_params = __DNS_ATTR_MAX,
	.params = dnsm_policy,
};

void vif_state_dhcp_allowlist_get(struct schema_Wifi_VIF_State *vstate)
{
	char fqdn[32];
	char read_ifname[8];
	char set[8];
	struct blob_attr *td[__DNS_ATTR_MAX] = { };

	if (vif_fixup_iface_captive_enabled(vstate->if_name) == false)
		return;

	uci_load(dns, "dhcp", &dhcp);
	ip_section = uci_lookup_section(dns, dhcp,"dnsmasq");
	if(!ip_section) {
		uci_unload(dns, dhcp);
		return;
	}
	blob_buf_init(&dnsmas, 0);
	uci_to_blob(&dnsmas, ip_section, &dnsm_param);
	blobmsg_parse(dnsm_policy, __DNS_ATTR_MAX, td, blob_data(dnsmas.head), blob_len(dnsmas.head));

	if (td[DNS_ATTR_IPSET]) {

		int rem = 0;
		vstate->captive_allowlist_len = 0;
		blobmsg_for_each_attr(cur, td[DNS_ATTR_IPSET], rem) {
			if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
				continue;
			sscanf(blobmsg_get_string(cur), "/%[^/]/%[^_]_%s", fqdn, set, read_ifname);
			if(!strcmp("opennds", read_ifname))
			{
				strcpy(vstate->captive_allowlist[vstate->captive_allowlist_len], fqdn);
				vstate->captive_allowlist_len++;
			}
		}
	}
	uci_unload(dns, dhcp);
	return;
}

void ipset_flush(char *ifname)
{
	char com[32]="ipset flush set_opennds";
	system(com);
}

void vif_dhcp_opennds_allowlist_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	struct blob_attr *e;
	blob_buf_init(&dnsmas, 0);
	int i;
	char ips[128];
	char buff[64];

	if (vif_fixup_iface_captive_enabled(vconf->if_name) == false)
		return;

	ipset_flush(ifname);
	e = blobmsg_open_array(&dnsmas, "ipset");
	for (i = 0; i < vconf->captive_allowlist_len; i++)
	{
		strcpy(buff,(char*)vconf->captive_allowlist[i]);
		snprintf(ips, sizeof(ips), "/%s/set_%s", buff,"opennds");
		blobmsg_add_string(&dnsmas, NULL,ips);
	}
	blobmsg_close_array(&dnsmas, e);
	blob_to_uci_section(dns, "dhcp", "dnsmasq", "dnsmasq", dnsmas.head, &dnsm_param, NULL);
	uci_commit_all(dns);
	return;
}
/* Captive portal options table*/

const char captive_portal_options_table[SCHEMA_CAPTIVE_PORTAL_OPTS_MAX][SCHEMA_CAPTIVE_PORTAL_OPT_SZ] =
{
	SCHEMA_CONSTS_SESSION_TIMEOUT,
	SCHEMA_CONSTS_BROWSER_TITLE,
	SCHEMA_CONSTS_SPLASH_PAGE_LOGO,
	SCHEMA_CONSTS_SPLASH_PAGE_BACKGROUND_LOGO,
	SCHEMA_CONSTS_SPLASH_PAGE_TITLE,
	SCHEMA_CONSTS_REDIRECT_URL,
	SCHEMA_CONSTS_AUTHENTICATION,
	SCHEMA_CONSTS_ACCEPTANCE_POLICY,
	SCHEMA_CONSTS_LOGIN_SUCCESS_TEXT,
	SCHEMA_CONSTS_USERPASS_FILE,
	SCHEMA_CONSTS_RADIUS_PORT,
	SCHEMA_CONSTS_RADIUS_IP,
	SCHEMA_CONSTS_RADIUS_SECRET,
	SCHEMA_CONSTS_RADIUS_AUTH_TYPE
};

void set_captive_portal_state(struct schema_Wifi_VIF_State *vstate,
		int *index, const char *key,
		const char *value)
{
	STRSCPY(vstate->captive_portal_keys[*index], key);
	STRSCPY(vstate->captive_portal[*index], value);
	*index += 1;
	vstate->captive_portal_len = *index;
	return;
}

void vif_state_captive_portal_options_get(struct schema_Wifi_VIF_State *vstate)
{
	int i;
	int index = 0;
	const char *opt;
	char *buf = NULL;
	struct blob_attr *tc[__NDS_ATTR_MAX] = { };
	struct uci_section *cp_section;

	if (vif_fixup_iface_captive_enabled(vstate->if_name) == false)
		return;

	uci_load(capg_uci, "opennds", &opennds);
	cp_section = uci_lookup_section(capg_uci, opennds,"opennds");
	if(!cp_section) {
		uci_unload(capg_uci, opennds);
		return;
	}
	blob_buf_init(&cap, 0);
	uci_to_blob(&cap, cp_section, &opennds_param);
	blobmsg_parse(opennds_policy, __NDS_ATTR_MAX, tc, blob_data(cap.head), blob_len(cap.head));
	for (i = 0; i < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; i++) {
		opt = captive_portal_options_table[i];
		if  (!strcmp(opt, "session_timeout"))
		{
			if (tc[NDS_ATTR_SESSIONTIMEOUT])
			{
				buf = blobmsg_get_string(tc[NDS_ATTR_SESSIONTIMEOUT]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (!strcmp(opt, "authentication")) {
			if(tc[NDS_ATTR_AUTHENTICATION]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_AUTHENTICATION]);
				if (!strcmp(buf, "None") || !strcmp(buf, "username") || !strcmp(buf, "radius")) {

					set_captive_portal_state(vstate, &index,
							captive_portal_options_table[i],
							buf);
				}
			}
		} else if (strcmp(opt, "browser_title") == 0) {
			if (tc[NDS_ATTR_GATEWAYNAME]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_GATEWAYNAME]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "splash_page_logo") == 0) {
			if (tc[NDS_ATTR_SPLASH_PAGE_LOGO]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_SPLASH_PAGE_LOGO]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "splash_page_background_logo") == 0) {
			if (tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "splash_page_title") == 0) {
			if (tc[NDS_ATTR_SPLASH_PAGE_TITLE]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_SPLASH_PAGE_TITLE]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "redirect_url") == 0) {
			if (tc[NDS_ATTR_REDIRECT_URL]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_REDIRECT_URL]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "acceptance_policy") == 0) {
			if (tc[NDS_ATTR_ACCEPTANCE_POLICY]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_ACCEPTANCE_POLICY]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "login_success_text") == 0) {
			if (tc[NDS_ATTR_LOGIN_SUCCESS_TEXT]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_LOGIN_SUCCESS_TEXT]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "username_password_file") == 0) {
			if (tc[NDS_ATTR_USERNAMEPASS_FILE]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_USERNAMEPASS_FILE]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "radius_server_ip") == 0) {
			if (tc[NDS_ATTR_RADIUS_IP]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_RADIUS_IP]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "radius_server_port") == 0) {
			if (tc[NDS_ATTR_RADIUS_PORT]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_RADIUS_PORT]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "radius_server_secret") == 0) {
			if (tc[NDS_ATTR_RADIUS_SECRET]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_RADIUS_SECRET]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		} else if (strcmp(opt, "radius_auth_type") == 0) {
			if (tc[NDS_ATTR_RADIUS_AUTH_TYPE]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_RADIUS_AUTH_TYPE]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
						buf);
			}
		}
	}
uci_unload(capg_uci, opennds);
return;
}

void clean_up(CURL *curl,FILE* imagefile, FILE* headerfile)
{
	curl_easy_cleanup(curl);
	fclose(imagefile);
	fclose(headerfile);
	return;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void splash_page_logo(char* dest_file, char* src_url)
{
	CURL *curl;
	CURLcode res;
	FILE *imagefile;
	FILE *headerfile;
	const char *clientcert = "/usr/opensync/certs/client.pem";
	const char *clientkey = "/usr/opensync/certs/client_dec.key";
	const char *pHeaderFile = "/etc/opennds/splashlogo_header";
	const char *keytype = "PEM";
	char errbuf[CURL_ERROR_SIZE];

	headerfile = fopen(pHeaderFile, "wb");
	imagefile = fopen(dest_file, "wb");
	if(imagefile == NULL){
		LOG(ERR, "fopen failed");
		if(headerfile)
			fclose(headerfile);
		return;
	}
	curl = curl_easy_init();

	if (curl == NULL){
		LOG(ERR, "curl_easy_init failed");
		clean_up(curl,imagefile,headerfile);
		return;
	}
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, headerfile);
	curl_easy_setopt(curl, CURLOPT_URL, src_url);
	curl_easy_setopt(curl, CURLOPT_SSLCERT, clientcert);
	curl_easy_setopt(curl, CURLOPT_SSLKEY, clientkey);
	curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, keytype);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, imagefile);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0L);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK){
		clean_up(curl,imagefile,headerfile);
		remove(dest_file);
		return;
	}
	clean_up(curl,imagefile,headerfile);
	return ;
}

int ipset_create(char *ifnds)
{
	char command[64];
	snprintf(command, sizeof(command), "ipset create set_%s hash:ip", ifnds);
	return (system(command));
}

void captive_portal_get_current_urls(char *ifname, char *splash_logo, char *back_image, char *user_file)
{
	char *buf = NULL;
	struct blob_attr *tc[__NDS_ATTR_MAX] = { };
	struct uci_section *cp_section = NULL;
	struct uci_package *opennds = NULL;
	struct uci_context *caps_uci = NULL;

	uci_load(caps_uci, "opennds", &opennds);
	cp_section = uci_lookup_section(caps_uci, opennds,"opennds");
	if(!cp_section) {
		uci_unload(caps_uci, opennds);
		return;
	}

	blob_buf_init(&url_buf, 0);
	uci_to_blob(&url_buf, cp_section, &opennds_param);

	blobmsg_parse(opennds_policy, __NDS_ATTR_MAX, tc, blob_data(url_buf.head), blob_len(url_buf.head));

	if (tc[NDS_ATTR_SPLASH_PAGE_LOGO]) {
		buf = blobmsg_get_string(tc[NDS_ATTR_SPLASH_PAGE_LOGO]);
		strcpy(splash_logo, buf);
	} else {
		splash_logo[0]=0;
	}

	if (tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]) {
		buf = blobmsg_get_string(tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]);
		strcpy(back_image, buf);
	} else {
		back_image[0]=0;
	}
	if (tc[NDS_ATTR_USERNAMEPASS_FILE]) {
		buf = blobmsg_get_string(tc[NDS_ATTR_USERNAMEPASS_FILE]);
		strcpy(user_file, buf);
	} else {
		user_file[0]=0;
	}
	uci_unload(caps_uci, opennds);

	return;
}
void opennds_parameters(char *ifname)
{
	int i;
	char users_router[7][64] = { "allow tcp port 53","allow udp port 53",
				"allow udp port 67","allow tcp port 22",
				"allow tcp port 23", "allow tcp port 80", "allow tcp port 443"};

	blob_buf_init(&cap_blob, 0);

	blobmsg_add_string(&cap_blob, "fwhook_enabled","1");
	blobmsg_add_string(&cap_blob, "use_outdated_mhd","1");
	blobmsg_add_string(&cap_blob, "unescape_callback_enabled","0");
	blobmsg_add_string(&cap_blob, "maxclients","250");
	blobmsg_add_string(&cap_blob, "preauthidletimeout","30");
	blobmsg_add_string(&cap_blob, "authidletimeout","120");
	blobmsg_add_string(&cap_blob, "checkinterval","60");
	blobmsg_add_string(&cap_blob, "uploadrate","0");
	blobmsg_add_string(&cap_blob, "downloadrate","0");
	blobmsg_add_string(&cap_blob, "ratecheckwindow","2");
	blobmsg_add_string(&cap_blob, "uploadquota","0");
	blobmsg_add_string(&cap_blob, "downloadquota","0");
	d = blobmsg_open_array(&cap_blob, "authenticated_users");
	blobmsg_add_string(&cap_blob, NULL, "allow all");
	blobmsg_close_array(&cap_blob, d);
	d = blobmsg_open_array(&cap_blob, "users_to_router");
	for(i = 0; i < 7; i++)
	{
		blobmsg_add_string(&cap_blob, NULL, users_router[i]);
	}
	blobmsg_close_array(&cap_blob, d);
	blob_to_uci_section(caps_uci, "opennds", "opennds", "opennds", cap_blob.head, &opennds_param, NULL);
	uci_commit_all(caps_uci);
	return;
}

void opennds_section_del(char *section_name)
{
	struct uci_package *opennds;
	struct uci_element *e = NULL, *tmp = NULL;
	int ret = 0;

	ret = uci_load(caps_uci, "opennds", &opennds);
	if (ret) {
		LOGE("%s: %s uci_load() failed with rc %d", section_name, __func__, ret);
		uci_unload(caps_uci, opennds);
		return;
	}
	uci_foreach_element_safe(&opennds->sections, tmp, e) {
		struct uci_section *s = uci_to_section(e);
		if (!strcmp(s->e.name, section_name)) {
			uci_section_del(caps_uci, "vif", "opennds", (char *)s->e.name, section_name);
		}
		else {
			continue;
		}
	}

	uci_commit(caps_uci, &opennds, false);
	uci_unload(caps_uci, opennds);
}

void vif_captive_portal_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	char value[255];
	int j;
	const char *opt;
	const char *val;
	blob_buf_init(&cap, 0);
	char path[64] = {0};
	char webroot[64] = {0};

	char ipset_tcp80[64];
	char ipset_tcp443[64];

	char splash_logo[84] = {0};
	char back_image[84] = {0};
	char user_file[84] = {0};

	snprintf(path, sizeof(path), "/etc/opennds/htdocs/images/");
	snprintf(webroot, sizeof(webroot), "/etc/opennds/htdocs");

	snprintf(ipset_tcp80, sizeof(ipset_tcp80),"allow tcp port 80 ipset set_opennds");
	snprintf(ipset_tcp443, sizeof(ipset_tcp443), "allow tcp port 443 ipset set_opennds");

	char file_path[128];
	struct stat st = {0};
	if (stat(path, &st) == -1)
		mkdir(path, 0755);
	captive_portal_get_current_urls(ifname, splash_logo, back_image, user_file);

	for (j = 0; j < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; j++) {
		opt = captive_portal_options_table[j];
		val = SCHEMA_KEY_VAL(vconf->captive_portal, opt);

		if (!val)
			strncpy(value, "0", 255);
		else
			strncpy(value, val, 255);

		if (!strcmp(opt, "authentication")) {
			if (strcmp(value,"None")==0) {
				blobmsg_add_string(&cap, "webroot",webroot);
				opennds_parameters(ifname);
				blobmsg_add_string(&cap, "enabled", "1");
				blobmsg_add_string(&cap, "gatewayinterface","br-lan");
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_string(&cap, "preauth","/usr/lib/opennds/login.sh");
				ipset_create("opennds");
				d = blobmsg_open_array(&cap, "preauthenticated_users");
				blobmsg_add_string(&cap, NULL, ipset_tcp80);
				blobmsg_add_string(&cap, NULL, ipset_tcp443);
				blobmsg_close_array(&cap, d);
				vif_fixup_set_iface_captive(ifname, true);

			} else if (strcmp(value,"username")==0) {
				blobmsg_add_string(&cap, "webroot",webroot);
				opennds_parameters(ifname);
				ipset_create("opennds");
				blobmsg_add_string(&cap, "enabled", "1");
				blobmsg_add_string(&cap, "gatewayinterface","br-lan");
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_string(&cap, "preauth", "/usr/lib/opennds/userpassword.sh");
				d = blobmsg_open_array(&cap, "preauthenticated_users");
				blobmsg_add_string(&cap, NULL, ipset_tcp80);
				blobmsg_add_string(&cap, NULL, ipset_tcp443);
				blobmsg_close_array(&cap, d);

				vif_fixup_set_iface_captive(ifname, true);

			} else if (strcmp(value,"radius")==0)  {
				blobmsg_add_string(&cap, "webroot",webroot);
				opennds_parameters("opennds");
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_string(&cap, "enabled", "1");
				blobmsg_add_string(&cap, "gatewayinterface","br-lan");
				blobmsg_add_string(&cap, "preauth", "/usr/lib/opennds/radius.sh");

				ipset_create("opennds");
				d = blobmsg_open_array(&cap, "preauthenticated_users");
				blobmsg_add_string(&cap, NULL, ipset_tcp80);
				blobmsg_add_string(&cap, NULL, ipset_tcp443);
				blobmsg_close_array(&cap, d);

				vif_fixup_set_iface_captive(ifname, true);
			}
			else {
				vif_fixup_set_iface_captive(ifname, false);
				if (vif_fixup_captive_enabled() == false)
					opennds_section_del("opennds");
				return;
			}
		}
		else if (strcmp(opt, "radius_server_port") == 0)
			blobmsg_add_string(&cap, "radius_server_port", value);

		else if (strcmp(opt, "radius_server_ip") == 0)
			blobmsg_add_string(&cap, "radius_server_ip", value);

		else if (strcmp(opt, "radius_server_secret") == 0)
			blobmsg_add_string(&cap, "radius_server_secret", value);

		else if (strcmp(opt, "radius_auth_type") == 0) {
			if(!strcmp(value, "MSCHAPv2"))
				blobmsg_add_string(&cap, "radius_auth_type", "MSCHAPV2");
			else
				blobmsg_add_string(&cap, "radius_auth_type", value);
		}

		else if (strcmp(opt, "session_timeout") == 0)
			blobmsg_add_string(&cap, "sessiontimeout", value);

		else if (strcmp(opt, "browser_title") == 0)
			blobmsg_add_string(&cap, "gatewayname", value);

		else if (strcmp(opt, "splash_page_logo") == 0) {
			blobmsg_add_string(&cap, "splash_page_logo", value);
			if (strcmp(splash_logo,value) !=0) {
				snprintf(file_path, sizeof(file_path), "%s%s",path,"TipLogo.png");
				splash_page_logo(file_path,value);
			}

		} else if (strcmp(opt, "splash_page_background_logo") == 0) {
			blobmsg_add_string(&cap, "page_background_logo", value);
			if (strcmp(back_image,value) !=0) {
				snprintf(file_path, sizeof(file_path),"%s%s",path,"TipBackLogo.png");
				splash_page_logo(file_path,value);
			}
		}

		else if (strcmp(opt, "splash_page_title") == 0) {
			blobmsg_add_string(&cap, "splash_page_title", value);
		}

		else if (strcmp(opt, "acceptance_policy") == 0)
			blobmsg_add_string(&cap, "acceptance_policy", value);

		else if (strcmp(opt, "redirect_url") == 0)
			blobmsg_add_string(&cap, "redirectURL", value);

		else if (strcmp(opt, "login_success_text") == 0)
			blobmsg_add_string(&cap, "login_success_text", value);

		else if (strcmp(opt, "username_password_file") == 0) {
			blobmsg_add_string(&cap, "username_password_file", value);
			if (strcmp(user_file,value) !=0) {
				snprintf(file_path, sizeof(file_path),"%s%s",path,"userpass.dat");
				splash_page_logo(file_path,value);
			}
		}
	}
	blob_to_uci_section(caps_uci, "opennds", "opennds", "opennds", cap.head, &opennds_param, NULL);
	uci_commit_all(caps_uci);
	return;
}

void captive_portal_init()
{
	caps_uci=uci_alloc_context();
	capg_uci=uci_alloc_context();
	dns=uci_alloc_context();
	return;
}
