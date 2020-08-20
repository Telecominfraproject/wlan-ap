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
const struct uci_blob_param_list opennds_param = {
	.n_params = __NDS_ATTR_MAX,
	.params = opennds_policy,
};
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

static void set_captive_portal_state(struct schema_Wifi_VIF_State *vstate,
		int *index, const char *key,
		const char *value)
{
	LOGN("Hi Entering to Portal Options");
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
	//int length;



	uci_load(cap_uci, "opennds", &opennds);
	LOGN("Hi SGET1 %x",(unsigned int)opennds);
	uci_foreach_element(&opennds->sections, e) {
		LOGN("Hi SGET2 :%x",(unsigned int)e);
		struct uci_section *cp_section = uci_to_section(e);
		LOGN("%s:s->e.name",s->e.name);
		LOGN("%s:cp_section->e.name",cp_section->e.name);
		//if (!strcmp(cp_section->type,"opennds")){
		if (!strcmp(s->e.name, cp_section->e.name)){
			LOGN("%s:Hi Captive_Portal",cp_section->e.name);

			blob_buf_init(&cap, 0);
			uci_to_blob(&cap, cp_section, &opennds_param);
			if(blob_len(cap.head)==0)
			{
				LOGN(":Hi Length Zero");
				//return;
			}
			LOGN("Hi About to parse");
			blobmsg_parse(opennds_policy, __NDS_ATTR_MAX, tc, blob_data(cap.head), blob_len(cap.head));
			LOGN("Hi OptionsGet");
			LOGN("Hi ForCNDTN1");

			if (tc[NDS_ATTR_CAPTIVE_ALLOWLIST]) {
				LOGN("Hi NDSCONDTN");
				struct blob_attr *cur = NULL;
				int rem = 0;
				vstate->captive_allowlist_len = 0;
				blobmsg_for_each_attr(cur, tc[NDS_ATTR_CAPTIVE_ALLOWLIST], rem) {
					if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
						continue;
					strcpy(vstate->captive_allowlist[vstate->captive_allowlist_len], blobmsg_get_string(cur));
					LOGN("%s: Hi MAC", (char*)vstate->captive_allowlist[vstate->captive_allowlist_len]);
					vstate->captive_allowlist_len++;
				}
			}
			for (i = 0; i < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; i++) {
				opt = captive_portal_options_table[i];
				LOGN("Hi c_option: %s",opt);
				if  (!strcmp(opt, "session_timeout"))
				{
					LOGN("Hi Val:%d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
					if (tc[NDS_ATTR_SESSIONTIMEOUT])
					{
						LOGN("Hi IN");
						sprintf(timeout,"%d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
						LOGN("Hi Buff:%s",timeout);
						LOGN("Hi INForCNDTNL %d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								timeout);
					}
				}
				else if (strcmp(opt, "authentication") == 0) {
					if(tc[NDS_ATTR_AUTHENTICATION]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_AUTHENTICATION]);
						if (!strcmp(buf, "None")) {
							LOGN("Hi OPENNDS1");
							set_captive_portal_state(vstate, &index,
									captive_portal_options_table[i],
									buf);
						}

						else if (!strcmp(buf,"Captive Portal User List")) {
							LOGN("Hi OPENNDS1");
							set_captive_portal_state(vstate, &index,
									captive_portal_options_table[i],
									buf);
						}
					}
				}
				else if (strcmp(opt, "browser_title") == 0) {
					LOGN("Hi Browse");
					if (tc[NDS_ATTR_GATEWAYNAME]) {
						LOGN("Hi BrowIN");
						buf = blobmsg_get_string(tc[NDS_ATTR_GATEWAYNAME]);
						LOGN("Hi BrowIN:%s",buf);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				} else if (strcmp(opt, "splash_page_logo") == 0) {
					if (tc[NDS_ATTR_SPLASH_PAGE_LOGO]) {
						LOGN("Hi SPLIN");
						buf = blobmsg_get_string(tc[NDS_ATTR_SPLASH_PAGE_LOGO]);
						LOGN("Hi BrowIN:%s",buf);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				} else if (strcmp(opt, "splash_page_background_logo") == 0) {
					LOGN("Hi SPlashPageBackgrnd");
					if (tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_PAGE_BACKGROUND_LOGO]);
						LOGN("Hi BackgrdLogo:%s",buf);
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
					LOGN("Hi Redirect");
					if (tc[NDS_ATTR_REDIRECT_URL]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_REDIRECT_URL]);
						LOGN("Hi Redirect:%s",buf);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}else if (strcmp(opt, "acceptance_policy") == 0) {
					if (tc[NDS_ATTR_ACCEPTANCE_POLICY]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_ACCEPTANCE_POLICY]);
						LOGN("Hi Acceptance:%s",buf);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}else if (strcmp(opt, "login_success_text") == 0) {
					if (tc[NDS_ATTR_LOGIN_SUCCESS_TEXT]) {
						buf = blobmsg_get_string(tc[NDS_ATTR_LOGIN_SUCCESS_TEXT]);
						LOGN("Hi login_success_text:%s",buf);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}
			}
		}
	}
uci_unload(cap_uci, opennds);
LOGN("Hi ENDG");
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
	LOGN("url:%s",src_url);
	LOGN("file_path:%s",dest_file);
	imagefile = fopen(dest_file, "wb");
	if(imagefile == NULL){
		LOG(ERR, "fopen failed");
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
	LOGN("Hi SET");
	char value[255];
	int j,i;
	const char *opt;
	const char *val;
	blob_buf_init(&cap, 0);
	char dir[32];
	char webroot[64];
	char port[64];

	sprintf(webroot,"/etc/opennds/htdocs/%s/",ifname);
	LOGN("webroot:%s",dir);

	struct blob_attr *e;
	e = blobmsg_open_array(&cap, "preauthenticated_users");
	for (i = 0; i < vconf->captive_allowlist_len; i++)
	{
		sprintf(port,"%s %s","allow tcp port 80 to",(char*)vconf->captive_allowlist[i]);
		blobmsg_add_string(&cap, NULL, port);
	}
	blobmsg_close_array(&cap, e);

	for (j = 0; j < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; j++) {
		opt = captive_portal_options_table[j];
		val = SCHEMA_KEY_VAL(vconf->captive_portal, opt);

		if (!val)
			strncpy(value, "0", 255);
		else
			strncpy(value, val, 255);
		LOGN("%s: Hi Options",value);

		if (!strcmp(opt, "authentication")) {
			blobmsg_add_u32(&cap, "enabled", 1);
			blobmsg_add_string(&cap, "webroot",webroot);
			//blobmsg_add_u32(&cap, "gatewayinterface",ifname);
			if(strcmp(value,"None")==0) {
				blobmsg_add_u32(&cap, "login_option_enabled", 0);
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_string(&cap, "splashpage","splash.html");
				blobmsg_add_string(&cap, "binauth","");
			}
			else if(strcmp(value,"Captive Portal User List")==0) {
				blobmsg_add_string(&cap, "authentication", value);
				blobmsg_add_u32(&cap, "login_option_enabled", 0);
				blobmsg_add_string(&cap, "splashpage", "splash_sitewide.html");
				blobmsg_add_string(&cap, "binauth","/usr/lib/opennds/binauth_sitewide.sh");
			}
		}
		else if (strcmp(opt, "session_timeout") == 0) {
			LOGN("%lu: Hi InTVaL",strtoul(value,NULL,10));
			blobmsg_add_u32(&cap, "sessiontimeout", strtoul(value,NULL,10));
		}
		else if (strcmp(opt, "browser_title") == 0) {
			blobmsg_add_string(&cap, "gatewayname", value);

		}

		char file_path[128];
		char path[64];
		sprintf(path,"%s%s/",webroot,"images");
		LOGN("path:%s",path);
		struct stat st = {0};
		if (stat(path, &st) == -1) {
			mkdir(path, 0755);
		}

		else if (strcmp(opt, "splash_page_logo") == 0) {
			LOGN("url:%s",value);
			sprintf(file_path,"%s%s",path,"TipLogo.png");
			blobmsg_add_string(&cap, "splash_page_logo", value);
			LOGN("file_path:%s",file_path);
			splash_page_logo(file_path,value);
			LOGN("file_path:%s",file_path);
		}

		else if (strcmp(opt, "splash_page_background_logo") == 0) {
			sprintf(file_path,"%s%s",path,"TipBackLogo.png");
			blobmsg_add_string(&cap, "page_background_logo", value);
			splash_page_logo(file_path,value);
			LOGN("file_path:%s",file_path);
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
	LOGN("Hi SETEND");
	blob_to_uci_section(cap_uci, "opennds", ifname, "opennds", cap.head, &opennds_param, NULL);
	LOGN("Hi SETEND:%s",ifname);
	uci_commit_all(cap_uci);
	return;
}
void captive_portal_init()
{
	cap_uci=uci_alloc_context();
	return;
}
