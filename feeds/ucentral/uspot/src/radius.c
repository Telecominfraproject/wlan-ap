#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include <radcli/radcli.h>

#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>

enum {
        RADIUS_TYPE,
        RADIUS_SERVER,
        RADIUS_USERNAME,
        RADIUS_PASSWORD,
        RADIUS_CHAP_PASSWORD,
        RADIUS_CHAP_CHALLENGE,
        RADIUS_ACCT_SESSION,
        RADIUS_CLIENT_IP,
        RADIUS_CALLED_STATION,
        RADIUS_CALLING_STATION,
        RADIUS_NAS_IP,
        RADIUS_NAS_ID,
        __RADIUS_MAX,
};

static const struct blobmsg_policy radius_policy[__RADIUS_MAX] = {
        [RADIUS_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_SERVER] = { .name = "server", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_USERNAME] = { .name = "username", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_PASSWORD] = { .name = "password", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_CHAP_PASSWORD] = { .name = "chap_password", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_CHAP_CHALLENGE] = { .name = "chap_challenge", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_ACCT_SESSION] = { .name = "acct_session", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_CLIENT_IP] = { .name = "client_ip", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_CALLED_STATION] = { .name = "called_station", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_CALLING_STATION] = { .name = "calling_station", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_NAS_IP] = { .name = "nas_ip", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_NAS_ID] = { .name = "nas_id", .type = BLOBMSG_TYPE_STRING },
};

static struct config {
	char *type;
	char *server;
	char *username;
	char *password;
	char chap_password[17];
	char chap_challenge[16];
	char *acct_session;
	struct sockaddr_in client_ip;
	char *called_station;
	char *calling_station;
	struct sockaddr_in  nas_ip;
	char *nas_id;
} config;

static struct blob_buf b = {};
static struct blob_attr *tb[__RADIUS_MAX] = {};

static int
str_to_hex(char *in, char *out, int olen)
{
	int ilen = strlen(in);
	int len = 0;

	while (ilen >= 2 && olen > 1) {
		int c;
		sscanf(in, "%2x", &c);
		*out++ = (char) c;

		in += 2;
		ilen -= 2;
		len++;
	}
	*out = '\0';

	return len;
}

static int
result(rc_handle const *rh, int accept, VALUE_PAIR *pair)
{
	struct blob_buf b = {};

	blob_buf_init(&b, 0);

	blobmsg_add_u32(&b, "access-accept", accept);

	if (pair) {
		void *c = blobmsg_open_table(&b, "reply");
		char name[33], value[256];
		VALUE_PAIR *vp;

		for (vp = pair; vp != NULL; vp = vp->next) {
			if (rc_avpair_tostr(rh, vp, name, sizeof(name), value,
					    sizeof(value)) == -1)
				break;
			blobmsg_add_string(&b, name, value);
		}
		blobmsg_close_table(&b, c);
	}
	printf("%s", blobmsg_format_json(b.head, true));

	return accept;
}

static void
config_load(void)
{
	if (tb[RADIUS_TYPE])
		config.type = blobmsg_get_string(tb[RADIUS_TYPE]);

	if (tb[RADIUS_SERVER])
		config.server = blobmsg_get_string(tb[RADIUS_SERVER]);

	if (tb[RADIUS_USERNAME])
		config.username = blobmsg_get_string(tb[RADIUS_USERNAME]);

	if (tb[RADIUS_PASSWORD])
		config.password = blobmsg_get_string(tb[RADIUS_PASSWORD]);

	if (tb[RADIUS_CHAP_PASSWORD]) {
		*config.chap_password = '\0';
		str_to_hex(blobmsg_get_string(tb[RADIUS_CHAP_PASSWORD]), &config.chap_password[1], 16);
	}

	if (tb[RADIUS_CHAP_CHALLENGE])
		str_to_hex(blobmsg_get_string(tb[RADIUS_CHAP_CHALLENGE]), config.chap_challenge, 16);

	if (tb[RADIUS_ACCT_SESSION])
		config.acct_session = blobmsg_get_string(tb[RADIUS_ACCT_SESSION]);

	if (tb[RADIUS_CLIENT_IP]) {
		inet_pton(AF_INET, blobmsg_get_string(tb[RADIUS_CLIENT_IP]), &(config.client_ip.sin_addr));
		config.client_ip.sin_addr.s_addr = ntohl(config.client_ip.sin_addr.s_addr);
	}

	if (tb[RADIUS_CALLED_STATION])
		config.called_station = blobmsg_get_string(tb[RADIUS_CALLED_STATION]);

	if (tb[RADIUS_CALLING_STATION])
		config.calling_station = blobmsg_get_string(tb[RADIUS_CALLING_STATION]);

	if (tb[RADIUS_NAS_IP]) {
		inet_pton(AF_INET, blobmsg_get_string(tb[RADIUS_NAS_IP]), &(config.nas_ip.sin_addr));
		config.nas_ip.sin_addr.s_addr = ntohl(config.nas_ip.sin_addr.s_addr);
	}

	if (tb[RADIUS_NAS_ID])
		config.nas_id = blobmsg_get_string(tb[RADIUS_NAS_ID]);
}

static rc_handle *
radius_init(void)
{
	rc_handle *rh = rc_new();
	if (rh == NULL)
		return NULL;

	rh = rc_config_init(rh);
	if (rh == NULL)
		return NULL;

	rc_add_config(rh, "authserver", config.server, "code", __LINE__);
	rc_add_config(rh, "servers", "/tmp/radius.servers", "code", __LINE__);
	rc_add_config(rh, "dictionary", "/etc/radcli/dictionary", "code", __LINE__);
	rc_add_config(rh, "radius_timeout", "5", "code", __LINE__);
	rc_add_config(rh, "radius_retries", "1", "code", __LINE__);
	rc_add_config(rh, "bindaddr", "*", "code", __LINE__);

	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0)
		return NULL;

	return rh;
}

static int
auth(void)
{
	VALUE_PAIR *send = NULL, *received;
	rc_handle *rh = NULL;

	if (!config.server || !config.username || !config.password)
		return result(NULL, 0, NULL);

	rh = radius_init();
	if (!rh)
		return result(NULL, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_USER_NAME, config.username, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_USER_PASSWORD, config.password, -1, 0) == NULL)
		return result(rh, 0, NULL);

	rc_apply_config(rh);
	if (rc_auth(rh, 0, send, &received, NULL) == OK_RC)
		return result(rh, 1, received);

	return result(rh, 0, NULL);
}

static int
uam_auth(void)
{
	VALUE_PAIR *send = NULL, *received;
	rc_handle *rh = NULL;

	if (!config.server || !config.username || !config.password ||
	    !config.acct_session || !config.called_station ||
	    !config.calling_station || !config.nas_id)
		return result(NULL, 0, NULL);

	rh = radius_init();
	if (!rh)
		return result(NULL, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_USER_NAME, config.username, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_USER_PASSWORD, config.password, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_ACCT_SESSION_ID, config.acct_session, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_FRAMED_IP_ADDRESS, &config.client_ip.sin_addr, 4, 0) == NULL)
		return result(rh, 0, NULL);

	//if (rc_avpair_add(rh, &send, PW_NAS_PORT_TYPE, , -1, 0) == NULL)
	//	return result(rh, 0, NULL);

	//if (rc_avpair_add(rh, &send, PW_NAS_PORT, , -1, 0) == NULL)
	//	return result(rh, 0, NULL);

//	if (rc_avpair_add(rh, &send, PW_NAS_PORT_ID_STRING, , -1, 0) == NULL)
//		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CALLED_STATION_ID, config.called_station, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CALLING_STATION_ID, config.calling_station, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_NAS_IP_ADDRESS, &config.nas_ip.sin_addr, 4, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_NAS_IDENTIFIER, config.nas_id, -1, 0) == NULL)
		return result(rh, 0, NULL);

	rc_apply_config(rh);
	if (rc_auth(rh, 0, send, &received, NULL) == OK_RC)
		return result(rh, 1, received);

	return result(rh, 0, NULL);
}

static int
uam_chap_auth(void)
{
	VALUE_PAIR *send = NULL, *received;
	rc_handle *rh = NULL;

	if (!config.server || !config.username ||
	    !config.acct_session || !config.called_station ||
	    !config.calling_station || !config.nas_id)
		return result(NULL, 0, NULL);

	rh = radius_init();
	if (!rh)
		return result(NULL, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_USER_NAME, config.username, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CHAP_PASSWORD, config.chap_password, 17, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CHAP_CHALLENGE, config.chap_challenge, 16, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_ACCT_SESSION_ID, config.acct_session, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_FRAMED_IP_ADDRESS, &config.client_ip.sin_addr, 4, 0) == NULL)
		return result(rh, 0, NULL);

	//if (rc_avpair_add(rh, &send, PW_NAS_PORT_TYPE, , -1, 0) == NULL)
	//	return result(rh, 0, NULL);

	//if (rc_avpair_add(rh, &send, PW_NAS_PORT, , -1, 0) == NULL)
	//	return result(rh, 0, NULL);

//	if (rc_avpair_add(rh, &send, PW_NAS_PORT_ID_STRING, , -1, 0) == NULL)
//		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CALLED_STATION_ID, config.called_station, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CALLING_STATION_ID, config.calling_station, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_NAS_IP_ADDRESS, &config.nas_ip.sin_addr, 4, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_NAS_IDENTIFIER, config.nas_id, -1, 0) == NULL)
		return result(rh, 0, NULL);

	rc_apply_config(rh);
	if (rc_auth(rh, 0, send, &received, NULL) == OK_RC)
		return result(rh, 1, received);

	return result(rh, 0, NULL);
}

static int
uam_acct(void)
{
	VALUE_PAIR *send = NULL, *received;
	rc_handle *rh = NULL;

	if (!config.server || !config.username || !config.password ||
	    !config.acct_session || !config.called_station ||
	    !config.calling_station || !config.nas_id)
		return result(NULL, 0, NULL);

	rh = radius_init();
	if (!rh)
		return result(NULL, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_USER_NAME, config.username, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_USER_PASSWORD, config.password, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_ACCT_SESSION_ID, config.acct_session, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_FRAMED_IP_ADDRESS, &config.client_ip.sin_addr, 4, 0) == NULL)
		return result(rh, 0, NULL);

	//if (rc_avpair_add(rh, &send, PW_NAS_PORT_TYPE, , -1, 0) == NULL)
	//	return result(rh, 0, NULL);

	//if (rc_avpair_add(rh, &send, PW_NAS_PORT, , -1, 0) == NULL)
	//	return result(rh, 0, NULL);

//	if (rc_avpair_add(rh, &send, PW_NAS_PORT_ID_STRING, , -1, 0) == NULL)
//		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CALLED_STATION_ID, config.called_station, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_CALLING_STATION_ID, config.calling_station, -1, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_NAS_IP_ADDRESS, &config.nas_ip.sin_addr, 4, 0) == NULL)
		return result(rh, 0, NULL);

	if (rc_avpair_add(rh, &send, PW_NAS_IDENTIFIER, config.nas_id, -1, 0) == NULL)
		return result(rh, 0, NULL);

	rc_apply_config(rh);
	if (rc_auth(rh, 0, send, &received, NULL) == OK_RC)
		return result(rh, 1, received);

	return result(rh, 0, NULL);
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		return result(NULL, 0, NULL);

	blob_buf_init(&b, 0);
	if (!blobmsg_add_json_from_file(&b, argv[1]))
		return result(NULL, 0, NULL);

	blobmsg_parse(radius_policy, __RADIUS_MAX, tb, blob_data(b.head), blob_len(b.head));

	config_load();
	if (!config.type)
		return result(NULL, 0, NULL);

	if (!strcmp(config.type, "auth"))
		return auth();

	if (!strcmp(config.type, "uam-auth"))
		return uam_auth();

	if (!strcmp(config.type, "uam-chap-auth"))
		return uam_chap_auth();

	if (!strcmp(config.type, "uam-acct"))
		return uam_acct();

	return result(NULL, 0, NULL);
}
