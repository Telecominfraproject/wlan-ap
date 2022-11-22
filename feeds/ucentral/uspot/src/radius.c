#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include <radcli/radcli.h>

#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>

enum {
        RADIUS_ACCT,
        RADIUS_SERVER,
        RADIUS_ACCT_SERVER,
        RADIUS_ACCT_TYPE,
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
        RADIUS_TERMINATE_CAUSE,
        RADIUS_SESSION_TIME,
        RADIUS_INPUT_OCTETS,
        RADIUS_OUTPUT_OCTETS,
        RADIUS_INPUT_GIGAWORDS,
        RADIUS_OUTPUT_GIGAWORDS,
        RADIUS_INPUT_PACKETS,
        RADIUS_OUTPUT_PACKETS,
        RADIUS_LOGOFF_URL,
        RADIUS_CLASS,
        __RADIUS_MAX,
};

static const struct blobmsg_policy radius_policy[__RADIUS_MAX] = {
        [RADIUS_ACCT] = { .name = "acct", .type = BLOBMSG_TYPE_BOOL },
        [RADIUS_SERVER] = { .name = "server", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_ACCT_SERVER] = { .name = "acct_server", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_ACCT_TYPE] = { .name = "acct_type", .type = BLOBMSG_TYPE_INT32 },
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
        [RADIUS_TERMINATE_CAUSE] = { .name = "terminate_cause", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_SESSION_TIME] = { .name = "session_time", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_INPUT_OCTETS] = { .name = "input_octets", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_OUTPUT_OCTETS] = { .name = "output_octets", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_INPUT_GIGAWORDS] = { .name = "input_gigawords", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_OUTPUT_GIGAWORDS] = { .name = "output_gigawords", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_INPUT_PACKETS] = { .name = "input_packets", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_OUTPUT_PACKETS] = { .name = "output_packets", .type = BLOBMSG_TYPE_INT32 },
        [RADIUS_LOGOFF_URL] = { .name = "logoff_url", .type = BLOBMSG_TYPE_STRING },
        [RADIUS_CLASS] = { .name = "class", .type = BLOBMSG_TYPE_STRING },
};

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

static int
radius(void)
{
	VALUE_PAIR *send = NULL, *received;
	struct sockaddr_in client_ip = {};
	struct sockaddr_in nas_ip = {};
        char chap_challenge[16] = {};
        char chap_password[17] = {};
	rc_handle *rh = rc_new();
	uint32_t val;

	if (rh == NULL)
		return result(rh, 0, NULL);;

	rh = rc_config_init(rh);
	if (rh == NULL)
		return result(rh, 0, NULL);;

	if (tb[RADIUS_SERVER])
		rc_add_config(rh, "authserver", blobmsg_get_string(tb[RADIUS_SERVER]), "code", __LINE__);

	if (tb[RADIUS_ACCT_SERVER])
		rc_add_config(rh, "acctserver", blobmsg_get_string(tb[RADIUS_ACCT_SERVER]), "code", __LINE__);
	rc_add_config(rh, "servers", "/tmp/radius.servers", "code", __LINE__);
	rc_add_config(rh, "dictionary", "/etc/radcli/dictionary", "code", __LINE__);
	rc_add_config(rh, "radius_timeout", "2", "code", __LINE__);
	rc_add_config(rh, "radius_retries", "1", "code", __LINE__);
	rc_add_config(rh, "bindaddr", "*", "code", __LINE__);

	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0)
		return result(rh, 0, NULL);

	if (tb[RADIUS_ACCT_TYPE]) {
		val = blobmsg_get_u32(tb[RADIUS_ACCT_TYPE]);
		if (rc_avpair_add(rh, &send, PW_ACCT_STATUS_TYPE, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_USERNAME])
		if (rc_avpair_add(rh, &send, PW_USER_NAME, blobmsg_get_string(tb[RADIUS_USERNAME]), -1, 0) == NULL)
	                return result(rh, 0, NULL);

	if (tb[RADIUS_PASSWORD])
		if (rc_avpair_add(rh, &send, PW_USER_PASSWORD, blobmsg_get_string(tb[RADIUS_PASSWORD]), -1, 0) == NULL)
			return result(rh, 0, NULL);

	if (tb[RADIUS_CHAP_PASSWORD]) {
		str_to_hex(blobmsg_get_string(tb[RADIUS_CHAP_PASSWORD]), &chap_password[1], 16);
		if (rc_avpair_add(rh, &send, PW_CHAP_PASSWORD, chap_password, 17, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_CHAP_CHALLENGE]) {
		str_to_hex(blobmsg_get_string(tb[RADIUS_CHAP_CHALLENGE]), chap_challenge, 16);
		if (rc_avpair_add(rh, &send, PW_CHAP_CHALLENGE, chap_challenge, 16, 0) == NULL)
	                return result(rh, 0, NULL);
	}

	if (tb[RADIUS_ACCT_SESSION])
		if (rc_avpair_add(rh, &send, PW_ACCT_SESSION_ID, blobmsg_get_string(tb[RADIUS_ACCT_SESSION]), -1, 0) == NULL)
			return result(rh, 0, NULL);

	if (tb[RADIUS_CLIENT_IP]) {
		inet_pton(AF_INET, blobmsg_get_string(tb[RADIUS_CLIENT_IP]), &(client_ip.sin_addr));
		client_ip.sin_addr.s_addr = ntohl(client_ip.sin_addr.s_addr);
		if (rc_avpair_add(rh, &send, PW_FRAMED_IP_ADDRESS, &client_ip.sin_addr, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_CALLED_STATION])
		if (rc_avpair_add(rh, &send, PW_CALLED_STATION_ID, blobmsg_get_string(tb[RADIUS_CALLED_STATION]), -1, 0) == NULL)
			return result(rh, 0, NULL);

	if (tb[RADIUS_LOGOFF_URL])
		if (rc_avpair_add(rh, &send, 3, blobmsg_get_string(tb[RADIUS_LOGOFF_URL]), -1, 14122) == NULL)
			return result(rh, 0, NULL);

	if (tb[RADIUS_CALLING_STATION])
		if (rc_avpair_add(rh, &send, PW_CALLING_STATION_ID, blobmsg_get_string(tb[RADIUS_CALLING_STATION]), -1, 0) == NULL)
			return result(rh, 0, NULL);

	if (tb[RADIUS_NAS_IP]) {
		inet_pton(AF_INET, blobmsg_get_string(tb[RADIUS_NAS_IP]), &(nas_ip.sin_addr));
		nas_ip.sin_addr.s_addr = ntohl(nas_ip.sin_addr.s_addr);
		if (rc_avpair_add(rh, &send, PW_NAS_IP_ADDRESS, &nas_ip.sin_addr, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_NAS_ID])
		if (rc_avpair_add(rh, &send, PW_NAS_IDENTIFIER, blobmsg_get_string(tb[RADIUS_NAS_ID]), -1, 0) == NULL)
			return result(rh, 0, NULL);

	if (tb[RADIUS_TERMINATE_CAUSE]) {
		val = blobmsg_get_u32(tb[RADIUS_TERMINATE_CAUSE]);
		if (rc_avpair_add(rh, &send, PW_ACCT_TERMINATE_CAUSE, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_SESSION_TIME]) {
		val = blobmsg_get_u32(tb[RADIUS_SESSION_TIME]);
		if (rc_avpair_add(rh, &send, PW_ACCT_SESSION_TIME, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_INPUT_OCTETS]) {
		val = blobmsg_get_u32(tb[RADIUS_INPUT_OCTETS]);
		if (rc_avpair_add(rh, &send, PW_ACCT_INPUT_OCTETS, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_OUTPUT_OCTETS]) {
		val = blobmsg_get_u32(tb[RADIUS_OUTPUT_OCTETS]);
		if (rc_avpair_add(rh, &send, PW_ACCT_OUTPUT_OCTETS, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_INPUT_GIGAWORDS]) {
		val = blobmsg_get_u32(tb[RADIUS_INPUT_GIGAWORDS]);
		if (rc_avpair_add(rh, &send, PW_ACCT_INPUT_GIGAWORDS, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_OUTPUT_GIGAWORDS]) {
		val = blobmsg_get_u32(tb[RADIUS_OUTPUT_GIGAWORDS]);
		if (rc_avpair_add(rh, &send, PW_ACCT_OUTPUT_GIGAWORDS, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_INPUT_PACKETS]) {
		val = blobmsg_get_u32(tb[RADIUS_INPUT_PACKETS]);
		if (rc_avpair_add(rh, &send, PW_ACCT_INPUT_PACKETS, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_OUTPUT_PACKETS]) {
		val = blobmsg_get_u32(tb[RADIUS_OUTPUT_PACKETS]);
		if (rc_avpair_add(rh, &send, PW_ACCT_OUTPUT_PACKETS, &val, 4, 0) == NULL)
			return result(rh, 0, NULL);
	}

	if (tb[RADIUS_CLASS])
		if (rc_avpair_add(rh, &send, PW_CLASS, blobmsg_get_string(tb[RADIUS_CLASS]), -1, 0) == NULL)
			return result(rh, 0, NULL);

	val = 19;
	if (rc_avpair_add(rh, &send, PW_NAS_PORT_TYPE, &val, 4, 0) == NULL)
		return result(rh, 0, NULL);

	rc_apply_config(rh);
	if (tb[RADIUS_ACCT] && blobmsg_get_bool(tb[RADIUS_ACCT])) {
		if (rc_acct(rh, 0, send) == OK_RC)
			return result(rh, 1, NULL);
	} else {
		if (rc_auth(rh, 0, send, &received, NULL) == OK_RC)
			return result(rh, 1, received);
	}

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

	return radius();
}
