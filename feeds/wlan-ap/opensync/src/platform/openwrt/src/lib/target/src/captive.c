#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <curl/easy.h>

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
#define IMAGE_PATH "/etc/opennds/htdocs/images/"
enum {
	NDS_ATTR_SESSIONTIMEOUT,
	NDS_ATTR_GATEWAYINTERFACE,
	NDS_ATTR_GATEWAYNAME,
	NDS_ATTR_LOGIN_OPTION_ENABLED,
	NDS_ATTR_ENABLED,
	NDS_ATTR_AUTHENTICATED_USERS,
	NDS_ATTR_CAPTIVE_MACLIST,
	NDS_ATTR_MACMECHANISM,
	NDS_ATTR_BROWSER_TITLE,
	NDS_ATTR_SPLASH_PAGE_LOGO,
	NDS_ATTR_PAGE_BACKGROUND_LOGO,
	NDS_ATTR_SPLASH_PAGE_TITLE,
	NDS_ATTR_REDIRECT_URL,
	NDS_ATTR_AUTHENTICATION,
	NDS_ATTR_ACCEPTANCE_POLICY,
	NDS_ATTR_LOGIN_SUCCESS_TEXT,
	__NDS_ATTR_MAX,
};
const struct blobmsg_policy opennds_policy[__NDS_ATTR_MAX] = {
	[NDS_ATTR_SESSIONTIMEOUT] = { .name = "sessiontimeout", .type = BLOBMSG_TYPE_INT32 },
	[NDS_ATTR_GATEWAYINTERFACE] = { .name = "gatewayinterface", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_GATEWAYNAME] = { .name = "gatewayname", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_LOGIN_OPTION_ENABLED] = { .name = "login_option_enabled", .type = BLOBMSG_TYPE_INT32 },
	[NDS_ATTR_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_INT32 },
	[NDS_ATTR_AUTHENTICATED_USERS]  = { .name = "authenticated_users", .type = BLOBMSG_TYPE_ARRAY },
	[NDS_ATTR_CAPTIVE_MACLIST]  = { .name = "allowedmac", .type = BLOBMSG_TYPE_ARRAY },
	[NDS_ATTR_MACMECHANISM]  = { .name = "macmechanism", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_BROWSER_TITLE]  = { .name = "browser_title", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_SPLASH_PAGE_LOGO]  = { .name = "splash_page_logo", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_PAGE_BACKGROUND_LOGO]  = { .name = "page_background_logo", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_SPLASH_PAGE_TITLE]  = { .name = "splash_page_title", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_REDIRECT_URL]  = { .name = "redirect_url", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_AUTHENTICATION]  = { .name = "authentication", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_ACCEPTANCE_POLICY]  = { .name = "acceptance_policy", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_LOGIN_SUCCESS_TEXT]  = { .name = "login_success_text", .type = BLOBMSG_TYPE_STRING },
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
			if (tc[NDS_ATTR_CAPTIVE_MACLIST]) {
				LOGN("Hi NDSCONDTN");
				struct blob_attr *cur = NULL;
				int rem = 0;
				vstate->captive_maclist_len = 0;
				blobmsg_for_each_attr(cur, tc[NDS_ATTR_CAPTIVE_MACLIST], rem) {
					if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
						continue;
					strcpy(vstate->captive_maclist[vstate->captive_maclist_len], blobmsg_get_string(cur));
					LOGN("%s: Hi MAC", (char*)vstate->captive_maclist[vstate->captive_maclist_len]);
					vstate->captive_maclist_len++;
				}
			}
			LOGN("Hi ForCNDTN");
			LOGN("Hi ForCNDTN1");
			for (i = 0; i < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; i++) {
				opt = captive_portal_options_table[i];
				LOGN("Hi c_option: %s",opt);
				/*if (!strcmp(opt, "session_timeout"))
				{
					LOGN("Hi Val:%d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
					if (tc[NDS_ATTR_SESSIONTIMEOUT])
					{
						LOGN("Hi IN");
						LOGN("Hi BuFF %s",buf);
						//length=snprintf(NULL,0,"%d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
						//LOGN("Hi LG %d",length);
						sprintf(buf,"%d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
						LOGN("Hi Buff:%s",buf);
						LOGN("Hi INForCNDTNL %d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
						//( blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]),buf,10);
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}*/
				if (strcmp(opt, "authentication") == 0) {
					LOGN("Hi Browse");
					if (tc[NDS_ATTR_LOGIN_OPTION_ENABLED]) {
						buf = "none";
						set_captive_portal_state(vstate, &index,
								captive_portal_options_table[i],
								buf);
					}
				}
				else if (strcmp(opt, "browser_title") == 0) {
					LOGN("Hi Browse");
					if (tc[NDS_ATTR_BROWSER_TITLE]) {
						LOGN("Hi BrowIN");
						buf = blobmsg_get_string(tc[NDS_ATTR_BROWSER_TITLE]);
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
LOGN("Hi GETEND");
uci_unload(cap_uci, opennds);
LOGN("Hi ENDG");
return;
}
static size_t image_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	return fwrite(ptr, size, nmemb, stream);
}
void vif_captive_portal_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	LOGN("Hi SET");
	int i,j;
	char value[255];
	const char *opt;
	const char *val;
	char VIF_section_name[20];
	FILE *file;
	blob_buf_init(&cap, 0);
	char dir[32];
	char libpath[64];

	vif_ifname_to_sectionname(ifname, VIF_section_name);

	sprintf(dir,"/etc/config/%s",VIF_section_name);
	LOGN("dir:%s",dir);
	struct stat sta = {0};
	if (stat(dir, &sta) == -1) {
		mkdir(dir, 0755);
	}
	blobmsg_add_string(&cap, "macmechanism", "allow");
	struct blob_attr *e;
	LOGN("%d: Hi Len",vconf->captive_maclist_len);
	e = blobmsg_open_array(&cap, "allowedmac");
	for (i = 0; i < vconf->captive_maclist_len; i++){
		blobmsg_add_string(&cap, NULL, (char*)vconf->captive_maclist[i]);
		LOGN("%s: Hi MAC", (char*)vconf->captive_maclist[i]);
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
			if(strcmp(value,"none")==0)
				blobmsg_add_u32(&cap, "login_option_enabled", 1);
		}
		else if (strcmp(opt, "session_timeout") == 0) {
			LOGN("%lu: Hi InTVaL",strtoul(value,NULL,10));
			blobmsg_add_u32(&cap, "sessiontimeout", strtoul(value,NULL,10));
		}
		else if (strcmp(opt, "browser_title") == 0) {
			blobmsg_add_string(&cap, "browser_title", value);
			sprintf(libpath,"%s/browser_title.txt", dir);
			file=fopen(libpath,"w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}

		else if (strcmp(opt, "splash_page_logo") == 0) {
			blobmsg_add_string(&cap, "splash_page_logo", value);
			CURL *curl;
			FILE *fp;
			CURLcode ret;
			LOGN("url:%s",value);
			char *url = value; //https://localhost:9096/filestore/netExpLogo.png==>real time: value
			char file_path[128];
			sprintf(file_path,IMAGE_PATH"%s","TipLogo");
			fp = fopen(file_path,"wb");
			if (fp == NULL) {
				LOG(ERR, "Captive: fopen failed");
				//curl_easy_cleanup(curl);
			}
			//curl_global_init(CURL_GLOBAL_DEFAULT);
			curl = curl_easy_init();
			if (curl == NULL) {
				LOG(ERR, "Captive: curl_easy_init failed");
			}
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, image_data);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
			ret = curl_easy_perform(curl);
			if (ret != CURLE_OK) {
				LOG(ERR, "Captive: Image Failed to download (CURLError: %s)", curl_easy_strerror(ret));
				//curl_easy_cleanup(curl);
				remove(file_path);
			}
			curl_easy_cleanup(curl);
			fclose(fp);
			//curl_global_cleanup();
		}
		/*else if (strcmp(opt, "splash_page_background_logo") == 0) {
			blobmsg_add_string(&cap, "page_background_logo", value);
			CURL *curl;
			FILE *fp;
			CURLcode ret;
			char *url = value; //https://localhost:9096/filestore/netExpLogo.png==>real time: value
			char file_path[128];
			sprintf(file_path,IMAGE_PATH"%s","TipBackLogo");
			fp = fopen(file_path,"wb");
			if (fp == NULL) {
				LOG(ERR, "Captive: fopen failed");
				//curl_easy_cleanup(curl);
			}
			//curl_global_init(CURL_GLOBAL_DEFAULT);
			curl = curl_easy_init();
			if (curl == NULL) {
				LOG(ERR, "Captive: curl_easy_init failed");
			}
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, image_data);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
			ret = curl_easy_perform(curl);
			if (ret != CURLE_OK) {
				LOG(ERR, "Captive: Image Failed to download (CURLError: %s)", curl_easy_strerror(ret));
				//curl_easy_cleanup(curl);
				remove(file_path);
			}
			curl_easy_cleanup(curl);
			fclose(fp);
			//curl_global_cleanup();
		}*/
		else if (strcmp(opt, "splash_page_title") == 0) {
			blobmsg_add_string(&cap, "splash_page_title", value);
			sprintf(libpath,"%s/splash_page_title.txt", dir);
			file=fopen(libpath,"w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
		else if (strcmp(opt, "acceptance_policy") == 0) {
			blobmsg_add_string(&cap, "acceptance_policy", value);
			sprintf(libpath,"%s/acceptance_policy.txt", dir);
			file=fopen(libpath,"w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
		else if (strcmp(opt, "redirect_url") == 0) {
			blobmsg_add_string(&cap, "redirect_url", value);
			sprintf(libpath,"%s/redirect_url.txt", dir);
			file=fopen(libpath,"w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
		else if (strcmp(opt, "login_success_text") == 0) {
			blobmsg_add_string(&cap, "login_success_text", value);
			sprintf(libpath,"%s/login_success_text.txt", dir);
			file=fopen(libpath,"w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
	}
	LOGN("Hi SETEND");
	blob_to_uci_section(cap_uci, "opennds", VIF_section_name, "opennds", cap.head, &opennds_param, NULL);
	LOGN("Hi SETEND:%s",VIF_section_name);
	uci_commit_all(cap_uci);
	return;
}
void captive_portal_init()
{
	cap_uci=uci_alloc_context();
	return;
}
