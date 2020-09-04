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

#include "log.h"
#include "const.h"
#include "target.h"
#include "evsched.h"
#include "radio.h"
#include "nl80211.h"
#include "utils.h"
#include "captive.h"

struct blob_buf cap={ };
static struct uci_package *opennds;
static struct uci_context *cap_uci;

#define SCHEMA_CAPTIVE_PORTAL_OPT_SZ            255
#define SCHEMA_CAPTIVE_PORTAL_OPTS_MAX          9
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
	NDC_ATTR_WEB_ROOT,
	__NDS_ATTR_MAX,
};
const struct blobmsg_policy opennds_policy[__NDS_ATTR_MAX] = {
	[NDS_ATTR_SESSIONTIMEOUT] = { .name = "sessiontimeout", .type = BLOBMSG_TYPE_INT32 },
	[NDS_ATTR_GATEWAYINTERFACE] = { .name = "gatewayinterface", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_GATEWAYNAME] = { .name = "gatewayname", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_LOGIN_OPTION_ENABLED] = { .name = "login_option_enabled", .type = BLOBMSG_TYPE_INT32 },
	[NDS_ATTR_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_INT32 },
	[NDS_ATTR_AUTHENTICATED_USERS]  = { .name = "authenticated_users", .type = BLOBMSG_TYPE_ARRAY },
	[NDS_ATTR_CAPTIVE_ALLOWLIST]  = { .name = "preauthenticated_users", .type = BLOBMSG_TYPE_ARRAY },
	[NDS_ATTR_MACMECHANISM]  = { .name = "macmechanism", .type = BLOBMSG_TYPE_STRING },
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
	[NDC_ATTR_WEB_ROOT]  = { .name = "webroot", .type = BLOBMSG_TYPE_STRING },
};

struct blob_buf dnsmas={ };
static struct uci_package *dhcp;
static struct uci_context *dns;
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

void vif_state_dhcp_allowlist_get(struct schema_Wifi_VIF_State *vstate,struct uci_section *s)
{
	struct blob_attr *td[__DNS_ATTR_MAX] = { };
	struct uci_element *e = NULL;

	uci_load(dns, "dhcp", &dhcp);
	uci_foreach_element(&dhcp->sections, e) {
		struct uci_section *ip_section = uci_to_section(e);
		if (!strcmp(s->e.name, ip_section->e.name)){

			blob_buf_init(&dnsmas, 0);
			uci_to_blob(&dnsmas, ip_section, &dnsm_param);
			if(blob_len(dnsmas.head)==0)
			{
				LOGN(":Hi Length Zero");
			}
			blobmsg_parse(dnsm_policy, __DNS_ATTR_MAX, td, blob_data(dnsmas.head), blob_len(dnsmas.head));

			if (td[DNS_ATTR_IPSET]) {
				struct blob_attr *cur = NULL;
				int rem = 0;
				vstate->captive_allowlist_len = 0;
				blobmsg_for_each_attr(cur, td[DNS_ATTR_IPSET], rem) {
					if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
						continue;
					strcpy(vstate->captive_allowlist[vstate->captive_allowlist_len], blobmsg_get_string(cur));

					vstate->captive_allowlist_len++;
				}
			}
		}
	}
uci_unload(dns, dhcp);
return;
}
int ipset_create(char *ifname)
{
	char command[64];
	char type[16] = "hash:ip";
	char tail[32] = "ipset create set";
	sprintf(command,"%s_%s %s", tail, ifname, type);
	return (system(command));
}
void vif_dhcp_opennds_allowlist_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	struct blob_attr *e;
	blob_buf_init(&dnsmas, 0);
	int i;
	char ips[64];
	ipset_create(ifname);
	e = blobmsg_open_array(&dnsmas, "ipset");
	for (i = 0; i < vconf->captive_allowlist_len; i++)
	{
		sprintf(ips,"%s/%s_%s",(char*)vconf->captive_allowlist[i],"set",ifname);
		blobmsg_add_string(&dnsmas, NULL,ips);
	}
	blobmsg_close_array(&dnsmas, e);
	blob_to_uci_section(dns, "dhcp", ifname, "dnsmasq", dnsmas.head, &dnsm_param, NULL);
	uci_commit_all(dns);
	return;
}

/* Captive portal options table */
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

void vif_state_captive_portal_options_get(struct schema_Wifi_VIF_State *vstate,struct uci_section *s)
{
	int i;
	int index = 0;
	const char *opt;
	char *buf = NULL;
	char timeout[32];
	struct blob_attr *tc[__NDS_ATTR_MAX] = { };
	struct uci_element *e = NULL;

	uci_load(cap_uci, "opennds", &opennds);
	uci_foreach_element(&opennds->sections, e) {
		struct uci_section *cp_section = uci_to_section(e);
		if (!strcmp(s->e.name, cp_section->e.name)){

			blob_buf_init(&cap, 0);
			uci_to_blob(&cap, cp_section, &opennds_param);
			if(blob_len(cap.head)==0)
			{
				LOGN(":Hi Length Zero");
			}
			blobmsg_parse(opennds_policy, __NDS_ATTR_MAX, tc, blob_data(cap.head), blob_len(cap.head));
			for (i = 0; i < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; i++) {
				opt = captive_portal_options_table[i];
				if  (!strcmp(opt, "session_timeout"))
				{
					if (tc[NDS_ATTR_SESSIONTIMEOUT])
					{
						sprintf(timeout,"%d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								timeout);
					}
				}
				else if (strcmp(opt, "authentication") == 0) {
					if(tc[NDS_ATTR_AUTHENTICATION]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_AUTHENTICATION]);
						if (!strcmp(buf, "None")) {
							set_captive_portal_state(vstate, &index,
									captive_portal_options_table[i],
									buf);
						}

						else if (!strcmp(buf,"Captive Portal User List")) {
							set_captive_portal_state(vstate, &index,
									captive_portal_options_table[i],
									buf);
						}
					}
				}
				else if (strcmp(opt, "browser_title") == 0) {
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
				}else if (strcmp(opt, "redirect_url") == 0) {
					if (tc[NDS_ATTR_REDIRECT_URL]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_REDIRECT_URL]);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}else if (strcmp(opt, "acceptance_policy") == 0) {
					if (tc[NDS_ATTR_ACCEPTANCE_POLICY]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_ACCEPTANCE_POLICY]);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}else if (strcmp(opt, "login_success_text") == 0) {
					if (tc[NDS_ATTR_LOGIN_SUCCESS_TEXT]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_LOGIN_SUCCESS_TEXT]);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}
			}
		}
	}
uci_unload(cap_uci, opennds);
return;
}

void clean_up(CURL *curl,FILE* imagefile, FILE* headerfile)
{
	curl_easy_cleanup(curl);
	fclose(imagefile);
	fclose(headerfile);
	return;
}

void splash_page_logo(char* dest_file, char* src_url)
{
	CURL *curl;
	CURLcode res;
	FILE *imagefile;
	FILE *headerfile;
	static const char *clientcert = "/usr/opensync/certs/client.pem";
	const char *clientkey = "/usr/opensync/certs/client_dec.key";
	static const char *pHeaderFile = "/etc/opennds/splashlogo_header";
	const char *keytype = "PEM";
	char errbuf[CURL_ERROR_SIZE];
	headerfile = fopen(pHeaderFile, "wb");
	imagefile = fopen(dest_file, "wb");
	if(imagefile == NULL){
		LOG(ERR, "fopen failed");
		fclose(headerfile);
		fclose(imagefile);
		return;
	}
	curl = curl_easy_init();

	if(curl == NULL){
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
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, imagefile);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0L);

	res = curl_easy_perform(curl);
	if(res != CURLE_OK){
		LOGN("Image failed to Download: %s",errbuf);
		clean_up(curl,imagefile,headerfile);
		remove(dest_file);
		return;
	}
	clean_up(curl,imagefile,headerfile);
	return ;
}
void vif_captive_portal_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	char value[255];
	int j,i;
	const char *opt;
	const char *val;
	blob_buf_init(&cap, 0);
	char path[64];
	char webroot[64];

	sprintf(path,"/etc/opennds/htdocs/images/%s/",ifname);
	sprintf(webroot,"/etc/opennds/htdocs");


	struct blob_attr *e;
	e = blobmsg_open_array(&cap, "preauthenticated_users");

	for(i=0;i<1;i++)
	{
		blobmsg_add_string(&cap, NULL, "allow tcp port 80 ipset set_wlan1");
		blobmsg_add_string(&cap, NULL, "allow tcp port 443 ipset set_wlan1");

	}
	blobmsg_close_array(&cap, e);

	for (j = 0; j < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; j++) {
		opt = captive_portal_options_table[j];
		val = SCHEMA_KEY_VAL(vconf->captive_portal, opt);

		if (!val)
			strncpy(value, "0", 255);
		else
			strncpy(value, val, 255);

		if (!strcmp(opt, "authentication")) {
			blobmsg_add_string(&cap, "webroot",webroot);
			//blobmsg_add_u32(&cap, "gatewayinterface",ifname);
			if(strcmp(value,"None")==0) {
				blobmsg_add_u32(&cap, "login_option_enabled", 1);
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_u32(&cap, "enabled", 1);
			}
			else if(strcmp(value,"Captive Portal User List")==0) {
				blobmsg_add_string(&cap, "authentication", value);
			}
		}
		else if (strcmp(opt, "session_timeout") == 0) {
			blobmsg_add_u32(&cap, "sessiontimeout", strtoul(value,NULL,10));
		}
		else if (strcmp(opt, "browser_title") == 0) {
			blobmsg_add_string(&cap, "gatewayname", value);

		}

		char file_path[128];
		struct stat st = {0};
		if (stat(path, &st) == -1) {
			mkdir(path, 0755);
		}

		else if (strcmp(opt, "splash_page_logo") == 0) {
			sprintf(file_path,"%s%s",path,"TipLogo.png");
			blobmsg_add_string(&cap, "splash_page_logo", value);
			splash_page_logo(file_path,value);
		}

		else if (strcmp(opt, "splash_page_background_logo") == 0) {
			sprintf(file_path,"%s%s",path,"TipBackLogo.png");
			blobmsg_add_string(&cap, "page_background_logo", value);
			splash_page_logo(file_path,value);
		}
		else if (strcmp(opt, "splash_page_title") == 0) {
			blobmsg_add_string(&cap, "splash_page_title", value);

		}
		else if (strcmp(opt, "acceptance_policy") == 0) {
			blobmsg_add_string(&cap, "acceptance_policy", value);

		}
		else if (strcmp(opt, "redirect_url") == 0) {
			blobmsg_add_string(&cap, "redirectURL", value);
		}
		else if (strcmp(opt, "login_success_text") == 0) {
			blobmsg_add_string(&cap, "login_success_text", value);

		}
	}
	blob_to_uci_section(cap_uci, "opennds", ifname, "opennds", cap.head, &opennds_param, NULL);
	uci_commit_all(cap_uci);
	return;
}
void captive_portal_init()
{
	cap_uci=uci_alloc_context();
	dns=uci_alloc_context();
	return;
}
