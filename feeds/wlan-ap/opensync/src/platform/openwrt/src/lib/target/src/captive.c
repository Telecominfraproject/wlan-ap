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

struct blob_buf cap={ };
static struct uci_package *opennds;
static struct uci_context *cap_uci;

#define SCHEMA_CAPTIVE_PORTAL_OPT_SZ            255
#define SCHEMA_CAPTIVE_PORTAL_OPTS_MAX          10
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
	uci_load(dns, "dhcp", &dhcp);
	ip_section = uci_lookup_section(dns, dhcp,"dnsmasq");
	if(!ip_section) {
		LOGN("Section Not Found");
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
			if(!strcmp(vstate->if_name, read_ifname))
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
	char com[24]="ipset flush set_wlan1";
	int ret;
	if(!strcmp(ifname,"wlan1"))
	{
		ret=system(com);
	}
	return;
}
void vif_dhcp_opennds_allowlist_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	struct blob_attr *e;
	blob_buf_init(&dnsmas, 0);
	int i;
	char ips[64];
	char buff[64];
	ipset_flush(ifname);
	e = blobmsg_open_array(&dnsmas, "ipset");
	for (i = 0; i < vconf->captive_allowlist_len; i++)
	{
		strcpy(buff,(char*)vconf->captive_allowlist[i]);
		sprintf(ips,"/%s/set_%s", buff,ifname);
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

char splash_logo[84];
char back_image[84];
char user_file[84];

void vif_state_captive_portal_options_get(struct schema_Wifi_VIF_State *vstate, struct uci_section *s)
{
	int i;
	int index = 0;
	const char *opt;
	char *buf = NULL;
	struct blob_attr *tc[__NDS_ATTR_MAX] = { };
	struct uci_element *e = NULL;

	uci_load(cap_uci, "opennds", &opennds);
	uci_foreach_element(&opennds->sections, e) {
		struct uci_section *cp_section = uci_to_section(e);
		if (!strcmp(s->e.name, cp_section->e.name)){

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
						if (!strcmp(buf, "None")) {

							set_captive_portal_state(vstate, &index,
									captive_portal_options_table[i],
									buf);
						} else if (!strcmp(buf,"username")) {
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
						strcpy(splash_logo, buf);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				} else if (strcmp(opt, "splash_page_background_logo") == 0) {
					if (tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]);
						strcpy(back_image, buf);
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
						strcpy(user_file, buf);
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
		LOGN("Image failed to Download: %s",errbuf);
		clean_up(curl,imagefile,headerfile);
		remove(dest_file);
		return;
	}
	clean_up(curl,imagefile,headerfile);
	return ;
}
int ipset_create(char *ifname)
{
	char command[64];
	sprintf(command,"ipset create set_%s hash:ip", ifname);
	return (system(command));
}

void vif_captive_portal_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	char value[255];
	int j;
	const char *opt;
	const char *val;
	blob_buf_init(&cap, 0);
	char path[64];
	char webroot[64];

	sprintf(path,"/etc/opennds/htdocs/images/%s/",ifname);
	sprintf(webroot,"/etc/opennds/htdocs");
	char file_path[128];
	struct stat st = {0};
	if (stat(path, &st) == -1)
		mkdir(path, 0755);

	for (j = 0; j < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; j++) {
		opt = captive_portal_options_table[j];
		val = SCHEMA_KEY_VAL(vconf->captive_portal, opt);

		if (!val)
			strncpy(value, "0", 255);
		else
			strncpy(value, val, 255);

		if (!strcmp(opt, "authentication")) {
			blobmsg_add_string(&cap, "webroot",webroot);
			if (strcmp(value,"None")==0) {
				blobmsg_add_string(&cap, "enabled", "1");
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_string(&cap, "preauth","/usr/lib/opennds/login.sh");
				ipset_create(ifname);

				struct blob_attr *e;
				e = blobmsg_open_array(&cap, "preauthenticated_users");
				blobmsg_add_string(&cap, NULL, "allow tcp port 80 ipset set_wlan1");
				blobmsg_add_string(&cap, NULL, "allow tcp port 443 ipset set_wlan1");
				blobmsg_close_array(&cap, e);

			} else if (strcmp(value,"username")==0) {
				ipset_create(ifname);
				blobmsg_add_string(&cap, "enabled", "1");
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_string(&cap, "preauth", "/usr/lib/opennds/userpassword.sh");

				struct blob_attr *e;
				e = blobmsg_open_array(&cap, "preauthenticated_users");
				blobmsg_add_string(&cap, NULL, "allow tcp port 80 ipset set_wlan1");
				blobmsg_add_string(&cap, NULL, "allow tcp port 443 ipset set_wlan1");
				blobmsg_close_array(&cap, e);
			} else {
				blobmsg_add_string(&cap, "authentication", "");
				blobmsg_add_string(&cap, "enabled", "");
				blobmsg_add_string(&cap, "preauth", "");
			}
		}

		else if (strcmp(opt, "session_timeout") == 0)
			blobmsg_add_string(&cap, "sessiontimeout", value);

		else if (strcmp(opt, "browser_title") == 0)
			blobmsg_add_string(&cap, "gatewayname", value);

		else if (strcmp(opt, "splash_page_logo") == 0) {
			sprintf(file_path,"%s%s",path,"TipLogo.png");
			blobmsg_add_string(&cap, "splash_page_logo", value);

			if (strcmp(splash_logo,value) !=0)
				splash_page_logo(file_path,value);
			strncpy(splash_logo, "0", strlen(splash_logo));

		} else if (strcmp(opt, "splash_page_background_logo") == 0) {
			sprintf(file_path,"%s%s",path,"TipBackLogo.png");
			blobmsg_add_string(&cap, "page_background_logo", value);
			if (strcmp(back_image,value) !=0)
				splash_page_logo(file_path,value);
			strncpy(back_image, "0", strlen(back_image));
		}

		else if (strcmp(opt, "splash_page_title") == 0)
			blobmsg_add_string(&cap, "splash_page_title", value);

		else if (strcmp(opt, "acceptance_policy") == 0)
			blobmsg_add_string(&cap, "acceptance_policy", value);

		else if (strcmp(opt, "redirect_url") == 0)
			blobmsg_add_string(&cap, "redirectURL", value);

		else if (strcmp(opt, "login_success_text") == 0)
			blobmsg_add_string(&cap, "login_success_text", value);

		else if (strcmp(opt, "username_password_file") == 0) {
			sprintf(file_path,"%s%s",path,"userpass.dat");
			blobmsg_add_string(&cap, "username_password_file", value);
			if (strcmp(user_file,value) !=0)
				splash_page_logo(file_path,value);
			strncpy(user_file, "0", strlen(user_file));
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
