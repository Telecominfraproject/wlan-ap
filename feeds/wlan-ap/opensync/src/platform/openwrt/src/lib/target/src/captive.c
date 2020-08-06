#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>

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

struct blob_buf c={ };
//static struct uci_package *opennds;
//struct uci_context *cap_uci;
//struct uci_element *p = NULL;

#define SCHEMA_CAPTIVE_PORTAL_OPT_SZ            255
#define SCHEMA_CAPTIVE_PORTAL_OPTS_MAX          10
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
	NDS_ATTR_SPLASH_PAGE_BACKGROUND_LOGO,
	NDS_ATTR_SPLASH_PAGE_TITLE,
	NDS_ATTR_REDIRECT_URL,
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
	[NDS_ATTR_SPLASH_PAGE_BACKGROUND_LOGO]  = { .name = "splash_page_background_logo", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_SPLASH_PAGE_TITLE]  = { .name = "splash_page_title", .type = BLOBMSG_TYPE_STRING },
	[NDS_ATTR_REDIRECT_URL]  = { .name = "redirect_url", .type = BLOBMSG_TYPE_STRING },
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
};

static void set_captive_portal_state(struct schema_Wifi_VIF_State *vstate,
                                    int *index, const char *key,
                                    const char *value)
{
	STRSCPY(vstate->captive_portal_keys[*index], key);
	STRSCPY(vstate->captive_portal[*index], value);
	*index += 1;
	vstate->captive_portal_len = *index;
	return;
}

void vif_state_captive_portal_options_get(struct schema_Wifi_VIF_State *vstate, struct uci_section *s)
{
	int i;
	int index = 0;
	const char *opt;
	char *buf = NULL;
	struct blob_attr *tc[__NDS_ATTR_MAX] = { };
	blob_buf_init(&c, 0);
	LOGN("%s:Hi Captive_Portal",s->e.name);
	uci_to_blob(&c, s, &opennds_param);
	if(blob_len(c.head)==0)
	{
		LOGN(":Hi Length Zero");
		return;
	}
	LOGN("Hi About to parse");
	blobmsg_parse(opennds_policy, __NDS_ATTR_MAX, tc, blob_data(c.head), blob_len(c.head));
	LOGN("Hi GET");
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

	for (i = 0; i < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; i++) {
		opt = captive_portal_options_table[i];

		 if (strcmp(opt, "session_timeout") == 0) {
			if (tc[NDS_ATTR_SESSIONTIMEOUT]) {
				sprintf(buf,"%d",blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]));
				//( blobmsg_get_u32(tc[NDS_ATTR_SESSIONTIMEOUT]),buf,10);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
							buf);
			}
		 } else if (strcmp(opt, "browser_title") == 0) {
			if (tc[NDS_ATTR_BROWSER_TITLE]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_BROWSER_TITLE]);
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
			if (tc[NDS_ATTR_SPLASH_PAGE_BACKGROUND_LOGO]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_SPLASH_PAGE_BACKGROUND_LOGO]);
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
		} else if (strcmp(opt, "splash_page_title") == 0) {
			if (tc[NDS_ATTR_SPLASH_PAGE_TITLE]) {
				buf = blobmsg_get_string(tc[NDS_ATTR_SPLASH_PAGE_TITLE]);
				set_captive_portal_state(vstate, &index,
						captive_portal_options_table[i],
							buf);
			}
		}
	}
	return;
}
void vif_captive_portal_set(const struct schema_Wifi_VIF_Config *vconf, char *ifname)
{
	LOGN("Hi SET");
	int i,j;
	char value[255];
	const char *opt;
	const char *val;
	//char VIF_section_name[20];
	FILE *file;
	//struct blob_buf c;
	blob_buf_init(&c, 0);
	//struct blob_attr *a;
	/*a = blobmsg_open_array(&c, "authenticated_users");
	for (i = 0; i < 1; i++)
		blobmsg_add_string(&c, NULL, "allow all");
	blobmsg_close_array(&c, a);*/
	blobmsg_add_string(&c, "macmechanism", "allow");

	struct blob_attr *e;
	e = blobmsg_open_array(&c, "allowedmac");
	for (i = 0; i < vconf->captive_maclist_len; i++){
		blobmsg_add_string(&c, NULL, (char*)vconf->captive_maclist[i]);
		LOGN("%s: HiMAC", (char*)vconf->captive_maclist[i]);
	}
	blobmsg_close_array(&c, e);

	for (j = 0; j < SCHEMA_CAPTIVE_PORTAL_OPTS_MAX; j++) {
		opt = captive_portal_options_table[j];
		val = SCHEMA_KEY_VAL(vconf->captive_portal, opt);

		if (!val)
			strncpy(value, "0", 255);
		else
			strncpy(value, val, 255);

		if (strcmp(opt, "session_timeout") == 0) {
			blobmsg_add_u32(&c, "sessiontimeout", strtoul(value,NULL,10));
		}
		else if (strcmp(opt, "browser_title") == 0) {
			blobmsg_add_string(&c, "browser_title", value);
			file=fopen("/tmp/browser_title.txt","w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}

		else if (strcmp(opt, "splash_page_logo") == 0) {
			blobmsg_add_string(&c, "splash_page_logo", value);
			file=fopen("/tmp/splash_page_log.txt","w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}

		else if (strcmp(opt, "splash_page_background_logo") == 0) {
			blobmsg_add_string(&c, "splash_page_background", value);
			file=fopen("/tmp/splash_page_background.txt","w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
		else if (strcmp(opt, "splash_page_title") == 0) {
			blobmsg_add_string(&c, "splash_page_title", value);
			file=fopen("/tmp/splash_page_title.txt","w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
		else if (strcmp(opt, "body_content") == 0) {
			blobmsg_add_string(&c, "body_content", value);
			file=fopen("/tmp/body_content.txt","w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
		else if (strcmp(opt, "redirect_url") == 0) {
			blobmsg_add_string(&c, "redirect_url", value);
			file=fopen("/tmp/redirect_url.txt","w+");
			if (file == NULL) {
				LOGW("Failed to create a file");
			}
			fprintf(file,"%s",val);
			fclose(file);
		}
	}
	//vif_ifname_to_sectionname(ifname, VIF_section_name)
	blob_to_uci_section(uci, "opennds", ifname, "opennds", c.head, &opennds_param,NULL);
	return;
}
