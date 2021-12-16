/* SPDX-License-Identifier: BSD-3-Clause */
#include <string.h>
#include <glob.h>
#include <linux/limits.h>
#include <libgen.h>
#include <stdio.h>
#include <libubox/blobmsg_json.h>

#include "uci.h"
#include "command.h"

ovsdb_table_t table_Node_Config;
ovsdb_table_t table_Node_State;
static struct blob_buf b;
static struct uci_context *uci;

static int val_to_array(char * val, char **array, int len)
{
	int count = 0;
	while ((array[count] = strtok(!count ? val : NULL, ":")) && ++count < len)
		;
	return count;
}

static void node_state_del(char *module)
{
	json_t *where, *cond;

	where = json_array();
	cond = ovsdb_tran_cond_single("module", OFUNC_EQ, (char *)module);
	json_array_append_new(where, cond);
	ovsdb_table_delete_where(&table_Node_State, where);
}

static void node_state_set(char *module, char *key, char *value)
{
	struct schema_Node_State node_state;
	json_t *where, *cond;

	where = json_array();
	cond = ovsdb_tran_cond_single("module", OFUNC_EQ, (char *)module);
	json_array_append_new(where, cond);
	MEMZERO(node_state);
	SCHEMA_SET_STR(node_state.module, module);
	SCHEMA_SET_STR(node_state.key, key);
	SCHEMA_SET_STR(node_state.value, value);
	SCHEMA_SET_INT(node_state.persist, true);
	ovsdb_table_upsert_where(&table_Node_State, where, &node_state, false);
}

static void node_config_set(char *module, char *key, char *value)
{
	struct schema_Node_Config node_config;
	json_t *where, *cond;

	where = json_array();
	cond = ovsdb_tran_cond_single("module", OFUNC_EQ, (char *)module);
	json_array_append_new(where, cond);
	MEMZERO(node_config);
	SCHEMA_SET_STR(node_config.module, module);
	SCHEMA_SET_STR(node_config.key, key);
	SCHEMA_SET_STR(node_config.value, value);
	ovsdb_table_upsert_where(&table_Node_Config, where, &node_config, false);
}

enum {
	LOG_ATTR_REMOTE,
	LOG_ATTR_IP,
	LOG_ATTR_PORT,
	LOG_ATTR_PROTO,
	LOG_ATTR_PRIORITY,
	__LOG_ATTR_MAX,
};

static const struct blobmsg_policy log_policy[__LOG_ATTR_MAX] = {
	[LOG_ATTR_REMOTE] = { .name = "log_remote", .type = BLOBMSG_TYPE_BOOL },
	[LOG_ATTR_IP] = { .name = "log_ip", .type = BLOBMSG_TYPE_STRING },
	[LOG_ATTR_PORT] = { .name = "log_port", .type = BLOBMSG_TYPE_STRING },
	[LOG_ATTR_PROTO] = { .name = "log_proto", .type = BLOBMSG_TYPE_STRING },
	[LOG_ATTR_PRIORITY] = { .name = "log_priority", .type = BLOBMSG_TYPE_STRING },
};

static const struct uci_blob_param_list log_param = {
	.n_params = __LOG_ATTR_MAX,
	.params = log_policy,
};

static void syslog_state(int config)
{
	struct blob_attr *tb[__LOG_ATTR_MAX] = { };
	struct uci_package *system;
	struct uci_element *e = NULL;
	struct uci_section *s = NULL;
	char val[128];
	int ret = 0;

	blob_buf_init(&b, 0);
	ret = uci_load(uci, "system", &system);
	if (ret) {
		LOGE("%s: uci_load() failed with rc %d", __func__, ret);
		return;
	}
	uci_foreach_element(&system->sections, e) {
		s = uci_to_section(e);
		if (!strcmp(s->type, "system"))
			break;
		s = NULL;
	}
	if (!s)
		goto out;
	uci_to_blob(&b, s, &log_param);

	blobmsg_parse(log_policy, __LOG_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	if (!tb[LOG_ATTR_REMOTE] || !blobmsg_get_bool(tb[LOG_ATTR_REMOTE]) ||
	    !tb[LOG_ATTR_IP] || !tb[LOG_ATTR_PORT] || !tb[LOG_ATTR_PROTO] || !tb[LOG_ATTR_PRIORITY]) {
		node_state_del("remote");
		goto out;
	}

	snprintf(val, sizeof(val), "%s:%s:%s:%s", blobmsg_get_string(tb[LOG_ATTR_PROTO]),
		 blobmsg_get_string(tb[LOG_ATTR_IP]), blobmsg_get_string(tb[LOG_ATTR_PORT]),
		 blobmsg_get_string(tb[LOG_ATTR_PRIORITY]));
	if (config)
		node_config_set("syslog", "remote", val);
	node_state_set("syslog", "remote", val);
out:
	uci_unload(uci, system);
}

static void syslog_handler(int type,
			   struct schema_Node_Config *old,
			   struct schema_Node_Config *conf)
{
	char *vals[4];
	int del = 1;

	blob_buf_init(&b, 0);
	switch (type) {
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		if (strcmp(conf->key, "remote"))
			break;
		if (val_to_array(conf->value, vals, 4) != 4) {
			LOG(ERR, "syslog, invalid remote");
			break;
		}
		blobmsg_add_string(&b, "log_proto", vals[0]);
		blobmsg_add_string(&b, "log_ip", vals[1]);
		blobmsg_add_string(&b, "log_port", vals[2]);
		blobmsg_add_string(&b, "log_priority", vals[3]);
		blobmsg_add_string(&b, "log_hostname", serial);
		blobmsg_add_u8(&b, "log_remote", 1);
		del = 0;
		break;
	case OVSDB_UPDATE_DEL:
		blobmsg_add_u8(&b, "log_remote", 0);
		break;
	}
	blob_to_uci_section(uci, "system", "@system[-1]", "system",
			    b.head, &log_param, NULL);
	uci_commit_all(uci);
	if (del)
		node_state_del("syslog");
	else
		syslog_state(0);
}

enum {
	NTP_ATTR_SERVER,
	__NTP_ATTR_MAX,
};

static const struct blobmsg_policy ntp_policy[__NTP_ATTR_MAX] = {
	[NTP_ATTR_SERVER] = { .name = "server", .type = BLOBMSG_TYPE_ARRAY },
};

static const struct uci_blob_param_list ntp_param = {
	.n_params = __NTP_ATTR_MAX,
	.params = ntp_policy,
};

static void ntp_state(int config)
{
	struct blob_attr *tb[__NTP_ATTR_MAX] = { };
	struct uci_package *p;
        struct uci_section *s;
	struct blob_attr *cur = NULL;
	char val[128] = {};
	int first = 1, rem = 0, ret = 0;

	blob_buf_init(&b, 0);
	ret = uci_load(uci, "system", &p);
	if (ret) {
		LOGE("%s: uci_load() failed with rc %d", __func__, ret);
		return;
	}

	s = uci_lookup_section(uci, p, "ntp");
	if (!s) {
		uci_unload(uci, p);
		return;
	}

	uci_to_blob(&b, s, &ntp_param);
	blobmsg_parse(ntp_policy, __NTP_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	if (!tb[NTP_ATTR_SERVER])
		goto out;

	blobmsg_for_each_attr(cur, tb[NTP_ATTR_SERVER], rem) {
		if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
			continue;
		if (!first)
			strncat(val, ":", sizeof(val) - 1);
		first = 0;
		strncat(val, blobmsg_get_string(cur), sizeof(val) - 1);
	}

	if (config)
		node_config_set("ntp", "server", val);
	node_state_set("ntp", "server", val);
out:
	uci_unload(uci, p);
}

static void ntp_handler(int type,
			struct schema_Node_Config *old,
			struct schema_Node_Config *conf)
{
	char *vals[4];
	void *a;
	int i, count;

	blob_buf_init(&b, 0);
	switch (type) {
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		if (strcmp(conf->key, "server"))
			break;
		count = val_to_array(conf->value, vals, 4);
		if (!count) {
			LOG(ERR, "ntp: invalid server");
			break;
		}
		a = blobmsg_open_array(&b, "server");
		for (i = 0; i < count; i++)
			blobmsg_add_string(&b, NULL, vals[i]);
		blobmsg_close_array(&b, a);
		break;
	}
	blob_to_uci_section(uci, "system", "ntp", "timeserver",
			    b.head, &ntp_param, NULL);
	uci_commit_all(uci);
	ntp_state(0);
}

enum {
	LED_ATTR_SYSFS,
	LED_ATTR_TRIGGER,
	LED_ATTR_DELAYON,
	LED_ATTR_DELAYOFF,
	LED_ATTR_VALUE,
	LED_ATTR_KEY,
	LED_ATTR_DEFAULT,
	__LED_ATTR_MAX,
};

static const struct blobmsg_policy led_policy[__LED_ATTR_MAX] = {
	[LED_ATTR_SYSFS] = { .name = "sysfs", .type = BLOBMSG_TYPE_STRING },
	[LED_ATTR_TRIGGER] = { .name = "trigger", .type = BLOBMSG_TYPE_STRING },
	[LED_ATTR_DELAYON] = { .name = "delayon", .type = BLOBMSG_TYPE_STRING},
	[LED_ATTR_DELAYOFF] = { .name = "delayoff", .type = BLOBMSG_TYPE_STRING},
	[LED_ATTR_VALUE] = { .name = "value", .type = BLOBMSG_TYPE_STRING},
	[LED_ATTR_KEY] = { .name = "key", .type = BLOBMSG_TYPE_STRING},
	[LED_ATTR_DEFAULT] = { .name = "default", .type = BLOBMSG_TYPE_BOOL},
};

static const struct uci_blob_param_list led_param = {
	.n_params = __LED_ATTR_MAX,
	.params = led_policy,
};

static char led[][8]={"lan", "wan", "eth", "wifi2", "wifi5", "wlan2g", "wlan5g", "power","eth0",
			  "status", "eth1", "wifi2g", "eth2", "wifi5g", "plug", "world", "usb", "linksys", "wps", "bt"};

static struct blob_buf b;
#define DEFAULT_BOARD_JSON		"/etc/board.json"
static struct blob_attr *board_info;

static void led_state(int config)
{
	struct blob_attr *tb[__LED_ATTR_MAX] = { };
	struct uci_package *system;
	struct uci_section *s = NULL;
	struct uci_element *e = NULL;
	char val[8];
	char key[16];
	blob_buf_init(&b, 0);
	uci_load(uci, "system", &system);
	uci_foreach_element(&system->sections, e) {
		s = uci_to_section(e);
		if (!strcmp(s->type, "led")) {
			uci_to_blob(&b, s, &led_param);
			blobmsg_parse(led_policy, __LED_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));
			if(tb[LED_ATTR_KEY])
				strcpy(key, blobmsg_get_string(tb[LED_ATTR_KEY]));
			if(tb[LED_ATTR_VALUE])
				strcpy(val, blobmsg_get_string(tb[LED_ATTR_VALUE]));
			break;
		}
		s = NULL;
	}
	if (!s)
		goto out;
	if (config)
		node_config_set("led", key, val);
	node_state_set("led", key, val);
out:
	uci_unload(uci, system);
}

int available_led_check(char *led_name)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(led); i++) {
		if(!strcmp(led_name,led[i])) {
			return 1;
		}
	}
	return 0;
}

static void set_led_config(char *trigger_name, char *key, char* value, char* led_string, char* led_section, int state)
{
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "sysfs", led_string);
	blobmsg_add_string(&b, "trigger", trigger_name);
	blobmsg_add_string(&b, "value", value);
	blobmsg_add_string(&b, "key", key);
	blobmsg_add_bool(&b, "default", state);
	blob_to_uci_section(uci, "system", led_section, "led", b.head, &led_param, NULL);
	return;
}

static struct blob_attr* config_find_blobmsg_attr(struct blob_attr *attr, const char *name, int type)
{
	struct blobmsg_policy policy = { .name = name, .type = type };
	struct blob_attr *cur;

	blobmsg_parse(&policy, 1, &cur, blobmsg_data(attr), blobmsg_len(attr));

	return cur;
}

static char* get_phy_map_led_info(char* wifi)
{
	struct blob_attr *cur;

	blob_buf_init(&b, 0);

	if (!blobmsg_add_json_from_file(&b, DEFAULT_BOARD_JSON)) {
		LOGD("Failed to load board.json file");
		goto out;
	}
	if (board_info != NULL) {
		free(board_info);
		board_info = NULL;
	}
	cur = config_find_blobmsg_attr(b.head, "led", BLOBMSG_TYPE_TABLE);
	if (!cur) {
		LOGD("Failed to find led objet in board.json file");
		goto out;
	}
	board_info = blob_memdup(cur);
	if (!board_info)
		goto out;
	cur = config_find_blobmsg_attr(board_info, wifi, BLOBMSG_TYPE_TABLE);
	if (!cur) {
		LOGD("Failed to find %s objet in board.json file", wifi);
		goto out;
	}
	cur = config_find_blobmsg_attr(cur, "trigger", BLOBMSG_TYPE_STRING);
	if (!cur) {
		LOGD("Failed to find trigger in board.json file");
		goto out;
	}
	return blobmsg_get_string(cur);

out:
	return "none";
}

static void set_primary_led_color(char *ap_name, char *led_section, char *color)
{
	char green_leds[][10]= {"power", "wifi2g", "wifi5g", "lan", "wan", "eth0", "eth1", "status"};
	if (!strcmp(ap_name, "wf194c") || !strcmp(ap_name, "wf6203") || !strcmp(ap_name, "ecw5410") || !strcmp(ap_name, "ec420"))
	{
		unsigned int i;
		for (i = 0; i < ARRAY_SIZE(green_leds); i++) {
			if (!strcmp(led_section, green_leds[i])) {
				strcpy(color, "green");
				return;
			}
		}
	}
	else
		return;
}

static void get_led_info_from_sys_config(char* key, char* value)
{
	char led_string[32];
	char ap_name[16];
	char color[16];
	char led_section[16];
	char led_name_final[24];
	char sys[8];
	char class[8];
	char leds[8];
	char sysled[PATH_MAX];
	glob_t gl;
	unsigned int i;

	if (glob("/sys/class/leds/*", GLOB_NOSORT, NULL, &gl))
		return;
	for (i = 0; i < gl.gl_pathc; i++) {
		strncpy(sysled, gl.gl_pathv[i], sizeof(sysled));
		sscanf(sysled,"/%[^/]/%[^/]/%[^/]/%s", sys, class, leds, led_string);
		sscanf(led_string,"%[^:]:%[^:]:%s",ap_name, color, led_section);
		if (strlen(led_string) < 8)
			continue;
		if (available_led_check(led_section)) {
			snprintf(led_name_final, sizeof(led_name_final), "%s%s", "led_", led_section);
			if (strcmp(color, "green")) {
				set_primary_led_color(ap_name, led_section, color);
				snprintf(led_string, sizeof(led_string), "%s:%s:%s",ap_name, color, led_section);
			}
			if (!strcmp(key, "led_blink")) {
				set_led_config("heartbeat", key, value, led_string, led_name_final, 1);
			}
			else if (!strcmp(key, "led_state")) {
				if (!strcmp(value, "off")) {
					set_led_config("none", key, value, led_string, led_name_final, 0);
				}
				else {
					if (!strcmp(led_section, "wifi2g") || !strcmp(led_section, "wifi5g")) {
						set_led_config(get_phy_map_led_info(led_section), key, value, led_string, led_name_final, 1);
					}
					else
						set_led_config("none", key, value, led_string, led_name_final, 1);
				}
			}
		}
	}
	globfree(&gl);
	return;
}

static void led_handler(int type,
			struct schema_Node_Config *old,
			struct schema_Node_Config *conf)
{
	int del=1;
	switch (type) {
	case OVSDB_UPDATE_NEW:
	case OVSDB_UPDATE_MODIFY:
		if (!strcmp(conf->key, "led_blink") || !strcmp(conf->key, "led_state")) {
			get_led_info_from_sys_config(conf->key, conf->value);
			del=0;
		}
		break;
	case OVSDB_UPDATE_DEL:
		get_led_info_from_sys_config("led_state", "default");
		break;
	default:
		LOGD("Invalid Command");
	}
	uci_commit_all(uci);
	if(del)
		node_state_del("led");
	else
		led_state(0);
}

static struct node_handler {
	char *name;
	void (*handler)(int type,
			struct schema_Node_Config *old,
			struct schema_Node_Config *conf);
	void (*state)(int config);
} node_handler[] = {
	{
		.name = "syslog",
		.handler = syslog_handler,
		.state = syslog_state,
	},
	{
		.name = "ntp",
		.handler = ntp_handler,
		.state = ntp_state,
	},
	{
		.name = "led",
		.handler = led_handler,
		.state = led_state,
	},
};

static void callback_Node_Config(ovsdb_update_monitor_t *mon,
				 struct schema_Node_Config *old,
				 struct schema_Node_Config *conf)
{
	unsigned int i;
	char *module = conf->module;

	if (mon->mon_type == OVSDB_UPDATE_DEL)
		module = old->module;

	for (i = 0; i < ARRAY_SIZE(node_handler); i++) {
		if (strcmp(node_handler[i].name, module))
			continue;
		node_handler[i].handler(mon->mon_type, old, conf);
	}
}

void node_config_init(void)
{
	unsigned int i;

	uci = uci_alloc_context();

	OVSDB_TABLE_INIT_NO_KEY(Node_Config);
	OVSDB_TABLE_INIT_NO_KEY(Node_State);
	OVSDB_TABLE_MONITOR(Node_Config, false);

	for (i = 0; i < ARRAY_SIZE(node_handler); i++)
		if (node_handler[i].state)
			node_handler[i].state(1);
}
